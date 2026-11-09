#include "review_submission_workflow.hpp"

#if defined(BPR_AGENT_INTEGRATIONS)
	#include "bpr/application/agent_chat_session_host.hpp"
	#include "bpr/application/agent_comment_workshop_host.hpp"
	#include "bpr/application/agent_finding_host.hpp"
	#include "bpr/application/agent_invocation_host.hpp"
	#include "bpr/application/mcp_context_helper_supervisor.hpp"
	#include "bpr/qt/agent_launch_panel.hpp"
#endif
#if defined(BPR_BUILD_TESTING)
	#include "bpr/app/github_application.hpp"
	#include "bpr/app/worker_executor.hpp"
	#include "bpr/application/agent_launch.hpp"
	#include "bpr/application/diff_application_host.hpp"
	#include "bpr/application/epoch_application_host.hpp"
	#include "bpr/application/github_epoch_handoff.hpp"
	#include "bpr/application/port.hpp"
	#include "bpr/application/review_ledger_application_host.hpp"
	#include "bpr/application/review_ledger_port.hpp"
	#include "bpr/application/workbench_runtime_ports.hpp"
	#include "bpr/core/agent_context_supplement.hpp"
	#include "bpr/core/agent_session.hpp"
	#include "bpr/core/diff_compute.hpp"
	#include "bpr/core/diff_model.hpp"
	#include "bpr/core/diff_semantics.hpp"
	#include "bpr/core/diff_transition.hpp"
	#include "bpr/core/epoch_model.hpp"
	#include "bpr/core/github_model.hpp"
	#include "bpr/core/model.hpp"
	#include "bpr/core/profile_window.hpp"
	#include "bpr/core/review_ledger.hpp"
	#include "bpr/core/review_round_trip.hpp"
	#include "bpr/core/workbench_interaction.hpp"
	#include "bpr/qt/diff_workbench.hpp"
	#include "bpr/qt/workbench_drawer.hpp"
	#include "bpr/qt/workbench_window.hpp"
	#include "github_application/profile_hosts.hpp"
	#include "github_application/window_composition.hpp"
	#include "github_application/window_review_lanes.hpp"
	#include "github_application_fixture_access.hpp"
	#include "github_public_path_fixture.hpp"
	#include "private_review_workflow.hpp"
	#include <QAbstractItemModel>
	#include <QAbstractItemView>
	#include <QAction>
	#include <QCoreApplication>
	#include <QEventLoop>
	#include <QFlags>
	#include <QListView>
	#include <QModelIndex>
	#include <chrono>
	#include <condition_variable>
	#include <cstddef>
	#include <expected>
	#include <functional>
	#include <future>
	#include <map>
	#include <memory>
	#include <mutex>
	#include <optional>
	#include <string_view>
	#include <thread>
	#include <utility>
	#include <vector>
	#if defined(BPR_AGENT_INTEGRATIONS)
		#include "bpr/core/agent_chat_scope.hpp"
		#include "bpr/qt/finding_queue_view.hpp"
		#include "bpr/qt/review_draft_editor.hpp"
		#include "bpr/qt/side_chat_panel.hpp"
		#include "github_application/agent_composition.hpp"
		#include <QString>
		#include <filesystem>
		#include <variant>
	#endif
#endif

namespace bpr::app {
	namespace {
#if defined(BPR_BUILD_TESTING)
		auto hold_fixture_effects(application::effect_executor& aEffects) -> std::function<void()>
		{
			struct gate
			{
				std::mutex              Mutex;
				std::condition_variable Changed;
				bool                    Open = false;
			};
			auto held = std::make_shared<gate>();
			aEffects.post([held] {
				auto lock = std::unique_lock{ held->Mutex };
				static_cast<void>(held->Changed.wait_for(lock, std::chrono::seconds{ 30 }, [&held] { return held->Open; }));
			});
			return [held] {
				{
					const auto lock = std::scoped_lock{ held->Mutex };
					held->Open      = true;
				}
				held->Changed.notify_all();
			};
		}

		/// \brief [[BPR-1211]]. Test access only: the ledger answers on the
		/// decision lane, so a synchronous fixture question waits for its
		/// answer, bounded, by delivering that lane's events.
		template <typename Answer>
		[[nodiscard]] auto await_ledger_answer(std::optional<Answer>& aAnswer) -> bool
		{
			const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{ 10 };
			while (!aAnswer.has_value() && std::chrono::steady_clock::now() < deadline) {
				QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
			}
			return aAnswer.has_value();
		}

		/// \brief Test access only: after a fixture write, the reload its own
		/// change notification queued is also answered before the course goes on,
		/// as it was when the write and its reload were synchronous.
		void await_ledger_quiescence(const application::review_ledger_application_host& aLedger)
		{
			const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{ 10 };
			for (auto quiet = 0; quiet < 3 && std::chrono::steady_clock::now() < deadline;) {
				QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
				quiet = aLedger.is_quiescent() ? quiet + 1 : 0;
			}
		}
#endif
	}

#if defined(BPR_BUILD_TESTING)
	auto detail::github_application_fixture_access::hold_review_ledger_effects(
		github_application& aApplication) -> std::function<void()>
	{
		return hold_fixture_effects(aApplication.Implementation->ReviewLedgerEffects);
	}

	auto detail::github_application_fixture_access::hold_private_review_effects(
		github_application& aApplication) -> std::function<void()>
	{
		return hold_fixture_effects(aApplication.Implementation->ReviewMutationEffects);
	}

	auto detail::github_application_fixture_access::hold_agent_conversation_effects(
		github_application& aApplication) -> std::function<void()>
	{
		return hold_fixture_effects(aApplication.Implementation->WorkbenchEffects);
	}

	auto detail::github_application_fixture_access::agent_conversation_loaded(
		const github_application& aApplication) -> std::optional<bool>
	{
	#if defined(BPR_AGENT_INTEGRATIONS)
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation != nullptr && implementation->AgentConversation != nullptr) {
			return implementation->AgentConversation->is_loaded();
		}
	#else
		static_cast<void>(aApplication);
	#endif
		return std::nullopt;
	}

	auto detail::github_application_fixture_access::review_ledger_available(
		github_application& aApplication) -> std::optional<bool>
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->ReviewLedgerHost == nullptr) {
			return std::nullopt;
		}
		return implementation->ReviewLedgerHost->available();
	}

	auto detail::github_application_fixture_access::review_ledger_file_exclusive(
		github_application& aApplication) -> std::optional<bool>
	{
		auto* const implementation = aApplication.Implementation.get();
		if (
			implementation == nullptr || implementation->ReviewLedgerHost == nullptr
			|| implementation->PrivateReview == nullptr || !implementation->AgentReview.has_value()) {
			return std::nullopt;
		}
		const auto laneIdle = [implementation] {
			const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{ 10 };
			while (!implementation->ReviewLedgerEffects.idle()) {
				if (std::chrono::steady_clock::now() >= deadline) {
					return false;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds{ 5 });
			}
			return true;
		};
		if (!laneIdle()) {
			return std::nullopt;
		}

		// A ledger store task running keeps the private store waiting.
		struct held_task
		{
			std::mutex              Mutex;
			std::condition_variable Changed;
			bool                    Started  = false;
			bool                    Released = false;
		};
		auto held = std::make_shared<held_task>();
		implementation->ReviewLedgerStoreEffects.post([held] {
			auto lock     = std::unique_lock{ held->Mutex };
			held->Started = true;
			held->Changed.notify_all();
			static_cast<void>(held->Changed.wait_for(lock, std::chrono::seconds{ 10 }, [&held] { return held->Released; }));
		});
		const auto release = [&held] {
			{
				const auto lock = std::scoped_lock{ held->Mutex };
				held->Released  = true;
			}
			held->Changed.notify_all();
		};
		{
			auto lock = std::unique_lock{ held->Mutex };
			if (!held->Changed.wait_for(lock, std::chrono::seconds{ 10 }, [&held] { return held->Started; })) {
				lock.unlock();
				release();
				return std::nullopt;
			}
		}
		const auto& review        = *implementation->AgentReview;
		auto        answer        = std::async(std::launch::async, [workflow = implementation->PrivateReview, target = core::review_mutation_target{ .Host = review.Host, .Repository = review.Repository, .PullRequest = review.PullRequest }, actor = review.Account] {
			static_cast<void>(workflow->pending(target, actor));
		});
		const auto  privateWaited = answer.wait_for(std::chrono::milliseconds{ 250 }) != std::future_status::ready;
		release();
		if (answer.wait_for(std::chrono::seconds{ 10 }) != std::future_status::ready || !laneIdle()) {
			return std::nullopt;
		}

		// The file exclusion held keeps a ledger store task from finishing.
		auto ledgerWaited = false;
		{
			const auto file = std::scoped_lock{ implementation->ReviewLedgerFile };
			implementation->ReviewLedgerHost->session_catalog([](const auto&) { });
			std::this_thread::sleep_for(std::chrono::milliseconds{ 250 });
			ledgerWaited = !implementation->ReviewLedgerEffects.idle();
		}
		if (!laneIdle()) {
			return std::nullopt;
		}
		return privateWaited && ledgerWaited;
	}

	auto detail::github_application_fixture_access::review_ledger_round_trip(
		github_application& aApplication) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->ReviewLedgerHost == nullptr
			|| implementation->Window == nullptr
			|| implementation->WorkbenchHost == nullptr) {
			return false;
		}
		if (implementation->ReviewLedgerRoundTripPassed) {
			return true;
		}

		auto* const window = implementation->Window.get();
		auto* const view   = window->files_drawer() != nullptr
			? window->files_drawer()->contents_view()
			: nullptr;
		if (!implementation->LastSelectedDiff.has_value()
			|| !implementation->ReviewLedgerHost->retained().Material.valid()) {
			if (view != nullptr && view->model() != nullptr
				&& view->model()->rowCount() > 0 && !view->currentIndex().isValid()) {
				view->setCurrentIndex(view->model()->index(0, 0));
			}
			return false;
		}

		const auto statistics = implementation->ReviewLedgerHost->statistics();
		// [[BPR-0830]]. Not `ReceiptLoadHits`: a queued refresh_shared_ledger re-read of a receipt committed in this process counts there too.
		implementation->ReviewLedgerWasPresentOnEntry
			= statistics.MaterialReceiptHits > 0U;
		const auto& selected = *implementation->LastSelectedDiff;
		const auto  material = core::file_material(selected.Key);
		const auto  before   = implementation->ReviewLedgerHost->retained();
		if (before.File != selected.Request.File.File
			|| before.Material != material) {
			return false;
		}
		if (before.Read == core::file_read_state::unread) {
			if (window->seen_action() == nullptr
				|| !window->seen_action()->isEnabled()) {
				return false;
			}
			window->seen_action()->trigger();
		}
		const auto after = implementation->ReviewLedgerHost->retained();
		implementation->ReviewLedgerRoundTripPassed
			= after.File == before.File && after.Material == before.Material
			&& after.Read != core::file_read_state::unread;
		return implementation->ReviewLedgerRoundTripPassed;
	}

	auto detail::github_application_fixture_access::request_fixture_diff(
		github_application& aApplication,
		const bool          aUseSecondHead) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (implementation != nullptr) {
			++implementation->FixtureDiffHelperInvocations;
		}
		if (
			implementation == nullptr || implementation->Fixture == nullptr
			|| implementation->EpochHost == nullptr
			|| implementation->DiffHost == nullptr) {
			return false;
		}
		const auto& state = implementation->EpochHost->state();
		if (!state.Authority.has_value() || !state.AcceptedComparison.has_value()) {
			return false;
		}
		const auto evidence     = implementation->Fixture->evidence();
		const auto expectedHead = aUseSecondHead ? evidence.HeadTwoOid : evidence.HeadOneOid;
		const auto objectFor    = [&evidence](const core::git_commit_oid aCommit)
			-> std::optional<core::git_blob_oid> {
			const auto hex      = core::canonical_hex(aCommit);
			const auto rendered = hex.view();
			if (rendered == evidence.BaseOid) {
				return core::parse_git_blob_oid(evidence.BaseBlobOid);
			}
			if (rendered == evidence.HeadOneOid) {
				return core::parse_git_blob_oid(evidence.HeadOneBlobOid);
			}
			if (rendered == evidence.HeadTwoOid) {
				return core::parse_git_blob_oid(evidence.HeadTwoBlobOid);
			}
			return std::nullopt;
		};
		if (
			core::canonical_hex(state.AcceptedComparison->Right.Commit).view()
			!= expectedHead) {
			return false;
		}
		const auto leftBlob  = objectFor(state.AcceptedComparison->Left.Commit);
		const auto rightBlob = objectFor(state.AcceptedComparison->Right.Commit);
		const auto options   = core::diff_options{ };
		const auto digest    = core::options_digest(options);
		if (!leftBlob.has_value() || !rightBlob.has_value() || !digest.has_value()) {
			return false;
		}
		const auto file = core::file_identity_of("review.cpp");
		if (!file.valid()) {
			return false;
		}
		implementation->request_diff(
			core::file_comparison_key{
				.Authority   = *state.Authority,
				.Repository  = state.Authority->Repository,
				.PullRequest = state.Authority->PullRequest,
				.LeftAnchor  = state.AcceptedComparison->Left.Identity,
				.RightAnchor = state.AcceptedComparison->Right.Identity,
				.LeftBlob    = leftBlob,
				.RightBlob   = rightBlob,
				.File        = file,
				.Options     = *digest,
				.Algorithm   = core::supported_diff_algorithm_revision,
			},
			*state.AcceptedComparison,
			options,
			// The fixture repository's single file. Named here because the
			// comparison key carries a digest and the syntax definition needs
			// a name, and BPR-0197 made that name a source file so the
			// classifier selects C++ rather than plain text.
			"review.cpp");
		return implementation->DiffHost->state().Pending.has_value();
	}

	auto detail::github_application_fixture_access::reaccept_fixture_revision(
		github_application& aApplication,
		const bool          aChangeHead) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (
			implementation == nullptr || implementation->Fixture == nullptr
			|| !implementation->LastAcceptedHandoff.has_value()) {
			return false;
		}
		auto handoff = clone_without_smart_git(
			*implementation->LastAcceptedHandoff);
		handoff.Snapshot = core::provider_snapshot_id{
			handoff.Snapshot.value() + 1U
		};
		handoff.PullRequestGeneration = core::request_generation{
			handoff.PullRequestGeneration.value() + 1U
		};
		if (aChangeHead) {
			const auto head = core::parse_git_commit_oid(
				implementation->Fixture->evidence().HeadTwoOid);
			if (!head.has_value()) {
				return false;
			}
			handoff.Head = *head;
		}
		if (!application::valid(handoff)) {
			return false;
		}
		implementation->accept(std::move(handoff));
		return true;
	}

	auto detail::github_application_fixture_access::mark_head_reviewed(
		github_application&    aApplication,
		const std::string_view aHeadOid) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		const auto  head           = core::parse_git_commit_oid(aHeadOid);
		if (implementation == nullptr || implementation->ReviewLedgerHost == nullptr
			|| !head.has_value()) {
			return false;
		}
		auto answer = std::make_shared<std::optional<bool>>();
		implementation->ReviewLedgerHost->persist_last_reviewed_head(*head, [answer](const std::expected<void, application::ledger_store_failure>& aStored) {
			*answer = aStored.has_value();
		});
		const auto persisted = await_ledger_answer(*answer) && **answer;
		if (persisted) {
			await_ledger_quiescence(*implementation->ReviewLedgerHost);
			implementation->publish_overview();
		}
		return persisted;
	}

	auto detail::github_application_fixture_access::seed_agent_private_context_canary(
		github_application&    aApplication,
		const std::string_view aSelectedNote,
		const std::string_view aUnselectedNote) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (
			implementation == nullptr || implementation->ReviewLedgerHost == nullptr
			|| implementation->EpochHost == nullptr || aSelectedNote.empty()
			|| aUnselectedNote.empty()) {
			return false;
		}
		const auto& state = implementation->EpochHost->state();
		if (!state.AcceptedRevision.has_value()) {
			return false;
		}
		const auto selected = application::local_note_record{
			.Id   = application::local_note_id{ 246U },
			.Note = { .Text = std::string{ aSelectedNote } },
		};
		const auto unselected = application::local_note_record{
			.Id   = application::local_note_id{ 247U },
			.Note = { .Text = std::string{ aUnselectedNote } },
		};
		auto       accepted = std::make_shared<std::size_t>(0U);
		auto       answered = std::make_shared<std::optional<bool>>();
		const auto count    = [accepted, answered](const bool aStored) {
			*accepted += aStored ? 1U : 0U;
			if (!aStored || *accepted == 3U) {
				*answered = aStored;
			}
		};
		auto* const ledger = implementation->ReviewLedgerHost.get();
		ledger->persist(selected, std::nullopt, [count](const auto& aStored) { count(aStored.has_value()); });
		ledger->persist(unselected, std::nullopt, [count](const auto& aStored) { count(aStored.has_value()); });
		ledger->persist_last_reviewed_head(state.AcceptedRevision->Head, [count](const auto& aStored) { count(aStored.has_value()); });
		const auto persisted = await_ledger_answer(*answered) && **answered;
		if (persisted && implementation->ReviewLedgerHost != nullptr) {
			await_ledger_quiescence(*implementation->ReviewLedgerHost);
		}
		if (persisted) {
			implementation->refresh_agent_private_context_choices();
		}
		return persisted;
	}

	auto detail::github_application_fixture_access::agent_invocation_context(
		github_application& aApplication)
		-> std::optional<detail::agent_invocation_context_evidence>
	{
		auto* const implementation = aApplication.Implementation.get();
		if (
			implementation == nullptr || implementation->EpochHost == nullptr
			|| implementation->ReviewLedgerHost == nullptr
			|| implementation->Window == nullptr
			|| !implementation->AgentReview.has_value()
			|| !implementation->SelectedDiffPublication.has_value()) {
			return std::nullopt;
		}
		const auto& state    = implementation->EpochHost->state();
		const auto& selected = *implementation->SelectedDiffPublication;
		if (
			!state.Authority.has_value() || !state.AcceptedRevision.has_value()
			|| !state.AcceptedComparison.has_value()) {
			return std::nullopt;
		}
		auto receipt = std::optional<core::file_receipt>{ };
		{
			const auto lock = std::scoped_lock{
				implementation->ReviewLedgerStateMutex
			};
			receipt = implementation->ReviewLedgerReceipt;
		}
		const auto privateState
			= implementation->ReviewLedgerHost->private_observation();
		auto sessionAnswer = std::make_shared<std::optional<std::expected<application::agent_session_catalog_state, application::ledger_store_failure>>>();
		implementation->ReviewLedgerHost->session_catalog([sessionAnswer](auto aCatalog) { *sessionAnswer = std::move(aCatalog); });
		if (!await_ledger_answer(*sessionAnswer)) {
			return std::nullopt;
		}
		const auto& sessionCatalog = **sessionAnswer;
		auto* const workbench      = implementation->Window->comparison_surface();
		if (
			!receipt.has_value() || !privateState.has_value()
			|| !sessionCatalog.has_value() || !sessionCatalog->Next.has_value()
			|| receipt->File != selected.Request.File.File || workbench == nullptr
			|| !workbench->selection().has_value()
			|| workbench->current_result() == nullptr
			|| workbench->current_result()->Alignment.Key != selected.Key
			|| workbench->selection()->Comparison
				!= workbench->current_result()->Alignment.Digest) {
			return std::nullopt;
		}
		const auto& authority  = *state.Authority;
		const auto& revision   = *state.AcceptedRevision;
		const auto& comparison = *state.AcceptedComparison;
		return detail::agent_invocation_context_evidence{
			.ApplicationSession = state.ApplicationSession,
			.Authority          = authority,
			.NextSession        = *sessionCatalog->Next,
			.Binding            = {
				.Host        = authority.Host,
				.Repository  = authority.Repository,
				.PullRequest = authority.PullRequest,
				.BaseOid     = revision.Base,
				.HeadOid     = revision.Head,
				.LeftEpoch   = comparison.Left.Identity.Epoch,
				.RightEpoch  = comparison.Right.Identity.Epoch,
			},
			.PullRequestUrl = "https://" + implementation->AgentReview->Hostname + "/" + implementation->AgentReview->Locator.Owner + "/" + implementation->AgentReview->Locator.Name + "/pull/" + std::to_string(implementation->AgentReview->Number.value()),
			.Files          = {
				application::agent_file_context_input{
					.Path               = selected.Request.File.PresentationPath,
					.Receipt            = *receipt,
					.ChangedSinceReview = privateState->LastReviewedHead.has_value() && *privateState->LastReviewedHead != revision.Head,
				},
			},
			.Private   = *privateState,
			.Selection = core::supplement_semantic_context{
				.File       = selected.Request.File.File,
				.Selection  = *workbench->selection(),
				.Confidence = core::supplement_mapping_confidence::exact,
				.Segments   = workbench->current_result()->Alignment.Segments,
				.Groups     = workbench->current_result()->Alignment.Groups,
			},
			.CurrentDraft = implementation->current_agent_draft_observation(),
		};
	}

	auto detail::github_application_fixture_access::window_application_session(
		const github_application&      aApplication,
		const core::bpr_window_context aContext) noexcept
		-> std::optional<core::application_session_id>
	{
		const auto found = aApplication.Sessions.find(aContext);
		if (found == aApplication.Sessions.end() || found->second->Closing) {
			return std::nullopt;
		}
		return found->second->WindowSession;
	}

	auto detail::github_application_fixture_access::agent_session_catalog(
		github_application& aApplication)
		-> std::optional<detail::agent_session_catalog_evidence>
	{
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->ReviewLedgerHost == nullptr) {
			return std::nullopt;
		}
		auto answer = std::make_shared<std::optional<std::expected<application::agent_session_catalog_state, application::ledger_store_failure>>>();
		implementation->ReviewLedgerHost->session_catalog([answer](auto aCatalog) { *answer = std::move(aCatalog); });
		if (!await_ledger_answer(*answer) || !answer->value().has_value()) {
			return std::nullopt;
		}
		auto catalog = std::move(answer->value());
		return detail::agent_session_catalog_evidence{
			.Catalog    = std::move(*catalog),
			.Statistics = implementation->ReviewLedgerHost->statistics(),
		};
	}
	#if defined(BPR_AGENT_INTEGRATIONS)
	auto detail::github_application_fixture_access::finding_queue(
		const github_application& aApplication) noexcept -> qt::finding_queue_view*
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation == nullptr ? nullptr
										 : implementation->FindingQueue;
	}

	auto detail::github_application_fixture_access::side_chat_panel(
		const github_application& aApplication) noexcept -> qt::side_chat_panel*
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation == nullptr ? nullptr
										 : implementation->SideChat;
	}

	auto detail::github_application_fixture_access::start_mcp_context_helper(
		github_application& aApplication, std::string aRequest)
		-> application::mcp_helper_refusal
	{
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->McpHelper == nullptr) {
			return application::mcp_helper_refusal::no_helper;
		}
		return implementation->McpHelper->start(std::move(aRequest));
	}

	auto detail::github_application_fixture_access::agent_chat_evidence(
		const github_application& aApplication) noexcept
		-> detail::agent_chat_composition_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return { };
		}
		auto evidence = detail::agent_chat_composition_evidence{
			.ChatHostComposed      = implementation->ChatHost != nullptr,
			.SideChatPanelComposed = implementation->SideChat != nullptr,
			.Intents               = implementation->SideChatIntents,
			.Presentations         = implementation->SideChatPresentations,
			.Turns                 = implementation->SideChatTurns,
			.RefusalsPresented     = implementation->SideChatRefusals,
			.Statistics            = implementation->ChatHost != nullptr
				? implementation->ChatHost->statistics()
				: application::agent_chat_session_statistics{ },
		};
		evidence.LastTurn         = implementation->LastChatTurn;
		evidence.LastConversation = implementation->LastAgentConversation;
		if (implementation->ChatHost != nullptr) {
			evidence.Session    = implementation->ChatHost->session();
			evidence.Observed   = implementation->ChatHost->observed();
			evidence.Projection = implementation->ChatHost->projection();
		}
		if (implementation->SideChat != nullptr) {
			evidence.SendEnabled
				= implementation->SideChat->send_action() != nullptr
					&& implementation->SideChat->send_action()->isEnabled()
				? "enabled"
				: "disabled";
			evidence.SessionStateText
				= implementation->SideChat->session_state_text().toStdString();
			evidence.HeadOidText
				= implementation->SideChat->head_oid_text().toStdString();
		}
		if (implementation->Window != nullptr) {
			if (auto* const panel = implementation->Window->agent_panel();
				panel != nullptr) {
				evidence.AgentPanelStatusText
					= panel->status_text().toStdString();
			}
		}
		return evidence;
	}

	auto detail::github_application_fixture_access::dispatch_side_chat_send(
		github_application& aApplication, std::string aText) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (
			implementation == nullptr || implementation->SideChat == nullptr
			|| implementation->ChatHost == nullptr) {
			return false;
		}
		const auto shown = implementation->ChatHost->projection();
		if (!shown.has_value()) {
			return false;
		}
		implementation->dispatch_side_chat_intent(qt::send_chat_message_intent{
			.Session = shown->Session.Id,
			.Action  = core::agent_action_intent{ shown->EntryPoint },
			.Scope   = shown->Chat.Scope,
			.Text    = std::move(aText),
		});
		return true;
	}

	auto detail::github_application_fixture_access::
		dispatch_side_chat_private_context(
			github_application&          aApplication,
			const core::agent_session_id aSession,
			const bool                   aNotes,
			const bool                   aDrafts) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->ChatHost == nullptr) {
			return false;
		}
		implementation->dispatch_side_chat_intent(qt::chat_private_context_intent{
			.OptIn = {
				.Session = aSession,
				.Notes   = aNotes,
				.Drafts  = aDrafts,
			},
		});
		return true;
	}

	auto detail::github_application_fixture_access::agent_finding_evidence(
		const github_application& aApplication) noexcept
		-> detail::agent_finding_composition_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return { };
		}
		auto evidence = detail::agent_finding_composition_evidence{
			.FindingHostComposed = implementation->FindingHost != nullptr,
			.CommentWorkshopComposed
			= implementation->WorkshopHost != nullptr,
			.McpHelperComposed     = implementation->McpHelper != nullptr,
			.SideChatPanelComposed = implementation->SideChat != nullptr,
			.FindingQueueComposed  = implementation->FindingQueue != nullptr,
			.QueuedFindings        = implementation->FindingQueue != nullptr
				? implementation->FindingQueue->finding_count()
				: 0U,
			.WorkshopProposals     = implementation->CommentWorkshopProposals,
			.RefusalsPresented     = implementation->FindingRefusalsPresented,
			.SideChatPresentations = implementation->SideChatPresentations,
			.SideChatIntents       = implementation->SideChatIntents,
			.FrozenSnapshotWrites  = implementation->FrozenSnapshotWrites,
			.FrozenFileCount       = implementation->frozen_snapshot_files().size(),
			.FindingStatistics     = implementation->FindingHost != nullptr
				? implementation->FindingHost->statistics()
				: application::agent_finding_host_statistics{ },
			.WorkshopStatistics    = implementation->WorkshopHost != nullptr
				? implementation->WorkshopHost->statistics()
				: application::agent_comment_workshop_statistics{ },
			.McpStatistics         = implementation->McpHelper != nullptr
				? implementation->McpHelper->statistics()
				: application::mcp_helper_supervisor_statistics{ },
			.McpLaunch             = implementation->McpHelper != nullptr
				? implementation->McpHelper->describe()
				: std::nullopt,
			.McpAnswer             = implementation->McpHelper != nullptr
				? implementation->McpHelper->answer()
				: std::string{ },
			.LastContinuationRealignment
			= implementation->LastContinuationRealignment,
			.LastContinuationRealignmentRetargeted
			= implementation->LastContinuationRealignmentRetargeted,
		};
		if (implementation->FindingQueue != nullptr) {
			evidence.QueuedClaim
				= implementation->FindingQueue->claim_text().toStdString();
			evidence.QueuedAnchorState
				= implementation->FindingQueue->anchor_state_text().toStdString();
		}
		if (implementation->ReviewDraft.has_value()) {
			evidence.LocalDraftBody = implementation->ReviewDraft->Body;
		}
		if (implementation->PendingWorkshopOperation.has_value()) {
			evidence.WorkshopPendingOperation = std::string{ core::stable_name(
				implementation->PendingWorkshopOperation->Operation) };
			evidence.WorkshopPendingSessionBound
				= implementation->PendingWorkshopOperation->Session.has_value();
		}
		if (implementation->Window != nullptr) {
			if (auto* const editor = implementation->Window->review_editor();
				editor != nullptr) {
				evidence.WorkshopOriginalText
					= editor->workshop_original_text().toStdString();
				evidence.WorkshopProposalText
					= editor->workshop_proposal_text().toStdString();
				evidence.WorkshopOperationText
					= editor->workshop_operation_text().toStdString();
			}
			if (auto* const panel = implementation->Window->agent_panel();
				panel != nullptr) {
				evidence.AgentPanelStatusText
					= panel->status_text().toStdString();
			}
		}
		if (implementation->SideChat != nullptr) {
			evidence.SideChatHeadOidText
				= implementation->SideChat->head_oid_text().toStdString();
		}
		return evidence;
	}
	#endif
	auto detail::github_application_fixture_access::agent_invocation_evidence(
		const github_application& aApplication) noexcept
		-> detail::agent_invocation_composition_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return { };
		}
	#if defined(BPR_AGENT_INTEGRATIONS)
		return {
			.AgentProcessLauncherComposed = implementation->AgentProcess != nullptr,
			// [[BPR-1900]]. The tour session's runs are counted with the
			// Agent tab's: a course asks whether an agent ran and finished.
			.AgentProcessLauncherInvocations
			= (implementation->AgentProcessProbe != nullptr
				   ? implementation->AgentProcessProbe->invocations()
				   : 0U)
				+ (implementation->TourProcessProbe != nullptr ? implementation->TourProcessProbe->invocations() : 0U),
			.HostComposed = implementation->AgentHost != nullptr,
			.Quiescent    = (implementation->AgentHost == nullptr
							 || implementation->AgentHost->is_quiescent())
				&& (implementation->TourSession == nullptr || implementation->TourSession->is_quiescent()),
			.Statistics    = implementation->AgentHost != nullptr
				? implementation->AgentHost->statistics()
				: application::agent_invocation_host_statistics{ },
			.WorkspaceRoot = std::filesystem::path{ implementation->LastAgentWorkspacePath }.parent_path(),
		};
	#else
		// [[BPR-1179]]. Nothing to observe: no launcher and no host are composed.
		return {
			.Quiescent     = true,
			.WorkspaceRoot = std::filesystem::path{ implementation->LastAgentWorkspacePath }.parent_path(),
		};
	#endif
	}
#endif
}
