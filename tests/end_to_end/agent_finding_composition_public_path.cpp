/// \file
/// \brief RFC-BPR-0012 composition, driven through the shipped application.
///
/// \details Modelled on `agent_invocation_composition_public_path.cpp`, and
/// deliberately the same shape: one real fixture-served review, real gestures,
/// and a construction cut per composed surface.
///
/// What this course is for, in one sentence: a reviewer's gesture reaches one
/// delivered finding, converting it produces one local draft, and no GitHub
/// mutation call happens anywhere along the way.

#include "public_path_poll.hpp"

#include "../../src/app/src/github_application_fixture_access.hpp"

#include "bpr/adapters/mcp/mcp_context_helper.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/finding_queue_view.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/side_chat_panel.hpp"
#include "bpr/qt/workbench_drawer.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QListView>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QThread>
#include <QToolButton>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace {
	using namespace bpr;

	using e2e::default_agent_budget;
	using e2e::extended_agent_budget;
	using e2e::wait_terms;

	/// The reviewer's own comment, byte for byte, before any agent sees it.
	constexpr std::string_view reviewer_original
		= "BPR_REVIEWER_ORIGINAL_0504 retry\tbudget? ";
	/// Exactly what `agent_finding_canary_main.cpp` proposes instead.
	constexpr std::string_view agent_suggested
		= "BPR_FINDING_SUGGESTED_0504 name the exhausted retry in the message";
	constexpr std::string_view agent_claim
		= "BPR_FINDING_CLAIM_0504 the retry budget is not reported to the caller";

	template <typename Predicate>
	[[nodiscard]] auto wait_until_reporting(
		wait_terms&                     aTerms,
		Predicate                       aPredicate,
		const std::chrono::milliseconds aBudget = default_agent_budget) -> bool
	{
		return e2e::wait_until(aTerms, aPredicate, aBudget);
	}

	[[nodiscard]] auto shutdown(app::github_application& aApplication) -> bool
	{
		aApplication.begin_shutdown();
		auto terms = wait_terms{ };
		return wait_until_reporting(
			terms,
			[&aApplication](wait_terms& aTerms) {
				QApplication::processEvents();
				return aTerms.require(
					"shutdown-complete", aApplication.status().ShutdownComplete);
			},
			extended_agent_budget);
	}

	[[nodiscard]] auto fail(
		app::github_application& aApplication,
		const std::string_view   aReason,
		const std::string_view   aState = { }) -> int
	{
		std::cerr << "agent-finding-composition-failure:" << aReason;
		if (!aState.empty()) {
			std::cerr << ' ' << aState;
		}
		std::cerr << '\n';
		static_cast<void>(shutdown(aApplication));
		return EXIT_FAILURE;
	}

	/// \brief Whether the composed helper launch is exactly what was pinned.
	///
	/// \details Three properties, all of them absences: no argument vector, an
	/// environment of exactly the two allowlisted names in the adapter's own
	/// order, and a working directory that is the pinned snapshot root and
	/// names no host. A supervisor that could add a fourth thing to this list
	/// could add `--listen`.
	[[nodiscard]] auto exact_helper_launch(
		const app::detail::agent_finding_composition_evidence& aEvidence,
		const std::filesystem::path&                           aSnapshotRoot)
		-> bool
	{
		if (!aEvidence.McpLaunch.has_value()) {
			return false;
		}
		const auto& launch = *aEvidence.McpLaunch;
		const auto  names
			= application::mcp_context_helper_supervisor::environment_names();
		if (
			launch.Environment.size() != adapters::mcp::mcp_environment_allowlist.size()
			|| names.size() != adapters::mcp::mcp_environment_allowlist.size()) {
			return false;
		}
		for (auto index = std::size_t{ 0U }; index < names.size(); ++index) {
			if (
				names[index] != adapters::mcp::mcp_environment_allowlist[index]
				|| launch.Environment[index].first != names[index]) {
				return false;
			}
		}
		return launch.WorkingDirectory == aSnapshotRoot.string()
			&& !launch.Executable.empty()
			&& !adapters::mcp::names_uri_scheme(launch.WorkingDirectory)
			&& !adapters::mcp::names_network_authority(launch.WorkingDirectory)
			&& !application::mcp_context_helper_supervisor::names_network_location(
				   launch.WorkingDirectory)
			&& launch.Environment[1].second
			== adapters::mcp::encode_revision_binding(
				   *adapters::mcp::decode_revision_binding(
					   launch.Environment[1].second));
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication qtApplication{ argc, argv };
	QApplication::setQuitOnLastWindowClosed(false);
	if (argc != 3) {
		return EXIT_FAILURE;
	}
	const auto course = std::string_view{ argv[1] };
	if (
		course != "positive" && course != "full-agent"
		&& course != "finding-host-construction-cut"
		&& course != "workshop-construction-cut"
		&& course != "mcp-construction-cut"
		&& course != "chat-panel-construction-cut"
		&& course != "queue-construction-cut"
		&& course != "workshop-press") {
		return EXIT_FAILURE;
	}
	auto data = QTemporaryDir{ };
	if (!data.isValid()) {
		return EXIT_FAILURE;
	}
	const auto                               dataRoot = std::filesystem::path{ data.path().toStdString() };
	std::unique_ptr<app::github_application> application;
	auto                                     startupTerminal = false;
	auto                                     startup         = app::detail::github_application_fixture_access::start(
		app::detail::github_public_path_fixture::github_com,
		{
			.ApplicationDataRoot = dataRoot,
			.EpochPhase          = app::detail::github_epoch_fixture_phase::single,
			.DisableAgentFindingHostComposition
			= course == "finding-host-construction-cut",
			.DisableCommentWorkshopComposition
			= course == "workshop-construction-cut",
			.DisableMcpHelperComposition = course == "mcp-construction-cut",
			.DisableSideChatPanelComposition
			= course == "chat-panel-construction-cut",
			.DisableFindingQueueComposition
			= course == "queue-construction-cut",
			.CodexAgentExecutable = std::filesystem::path{ argv[2] },
		},
		[&](auto aResult) {
			startupTerminal = true;
			if (aResult) {
				application = std::move(*aResult);
			}
		});
	auto startupTerms = wait_terms{ };
	if (!wait_until_reporting(startupTerms, [&](wait_terms& aTerms) {
			return aTerms.require("startup-terminal", startupTerminal && startup && startup->is_quiescent());
		})
		|| !application) {
		std::cerr << "application-startup-failed:" << startupTerms.render() << '\n';
		return EXIT_FAILURE;
	}
	application->show();
	auto& window = application->window();
	window.github_connect_action()->trigger();
	auto connected = wait_terms{ };
	if (!wait_until_reporting(connected, [&window](wait_terms& aTerms) {
			aTerms.note_identifier("status", window.github_status_text());
			return aTerms.require(
					   "status-is-connected",
					   window.github_status_text() == "GitHub account connected.")
				&& aTerms.require(
					"open-enabled", window.github_open_action()->isEnabled());
		})) {
		return fail(*application, "connect", connected.render());
	}
	window.session_panel()->owner_editor()->setText(QStringLiteral("octo-org"));
	window.session_panel()->repository_editor()->setText(
		QStringLiteral("review-repository"));
	window.session_panel()->pull_request_editor()->setValue(17);
	window.github_open_action()->trigger();

	auto* const panel     = window.agent_panel();
	auto        offerings = wait_terms{ };
	if (!wait_until_reporting(offerings, [&window, panel](wait_terms& aTerms) {
			return aTerms.require(
					   "files-listed",
					   window.files_drawer()->contents_view()->model()->rowCount()
						   != 0)
				&& aTerms.require(
					"profiles-is-four", panel->profile_selector()->count() == 4)
				&& aTerms.require(
					"disclosure-present", !panel->disclosure_text().isEmpty());
		})) {
		return fail(*application, "agent-offerings", offerings.render());
	}

	auto* const files = window.files_drawer()->contents_view();
	files->selectionModel()->setCurrentIndex(
		files->model()->index(0, 0),
		QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
	auto selectedDiff = wait_terms{ };
	if (!wait_until_reporting(
			selectedDiff,
			[&window, panel](wait_terms& aTerms) {
				return aTerms.require(
						   "comparison-shown",
						   window.comparison_surface()->current_result() != nullptr)
					&& aTerms.require(
						"start-enabled", panel->start_action()->isEnabled());
			},
			extended_agent_budget)) {
		return fail(*application, "selected-diff", selectedDiff.render());
	}
	auto* const surface = window.comparison_surface();
	if (!surface->navigate_change(
			core::change_navigation_direction::next,
			core::change_navigation_policy::stop)) {
		return fail(*application, "semantic-selection-action");
	}
	auto semantic = wait_terms{ };
	if (!wait_until_reporting(semantic, [surface](wait_terms& aTerms) {
			return aTerms.require(
				"selection-present", surface->selection().has_value());
		})) {
		return fail(*application, "semantic-selection", semantic.render());
	}
	if (!window.read_action()->isEnabled()) {
		return fail(*application, "receipt-action");
	}
	window.read_action()->trigger();
	auto receipt = wait_terms{ };
	if (!wait_until_reporting(receipt, [&window](wait_terms& aTerms) {
			return aTerms.require(
				"read-checked", window.read_action()->isChecked());
		})) {
		return fail(*application, "receipt-projection", receipt.render());
	}

	auto* const editor     = window.review_editor();
	auto        draftReady = wait_terms{ };
	if (
		editor == nullptr
		|| !wait_until_reporting(draftReady, [editor](wait_terms& aTerms) {
			   return aTerms.require(
				   "draft-shown", editor->shown_draft().has_value());
		   })) {
		return fail(*application, "local-draft", draftReady.render());
	}
	// The reviewer's own bytes, tab and trailing space included, so a proposal
	// that overwrote them has nowhere to hide.
	editor->body_editor()->setPlainText(
		QString::fromUtf8(
			reviewer_original.data(),
			static_cast<qsizetype>(reviewer_original.size())));
	QApplication::processEvents();

	const auto codexIndex = panel->profile_selector()->findData(
		QVariant::fromValue<qulonglong>(core::codex_jsonl_profile_id.value()));
	if (codexIndex < 0) {
		return fail(*application, "codex-profile");
	}
	panel->profile_selector()->setCurrentIndex(codexIndex);
	// BPR-0533. The workshop course starts its turn from the comment-workshop
	// control instead of the launch panel, and from nowhere else: the press is
	// the only gesture it makes, so every process, workspace, and delivery
	// counted below belongs to it.
	//
	// The refusal is read on the surface that asked rather than on the launch
	// panel: the panel deliberately ignores a refusal while an invocation is
	// running, so a second press would leave no trace there and "never queued
	// silently" would be unfalsifiable. On the control it is the control's own
	// state, which is also where the reviewer would read it.
	auto pressedRefusal         = std::string{ };
	auto reofferedAfterProposal = false;
	if (course == "workshop-press") {
		auto* const clarify = editor->clarify_action();
		if (clarify == nullptr || !clarify->isEnabled()) {
			return fail(*application, "workshop-control");
		}
		clarify->trigger();
		QApplication::processEvents();
		if (!clarify->isEnabled()) {
			pressedRefusal = clarify->toolTip().toStdString();
		}
	} else {
		panel->start_button()->click();
	}

	auto delivered = wait_terms{ };
	if (!wait_until_reporting(
			delivered,
			[&application](wait_terms& aTerms) {
				const auto invocation = app::detail::
					github_application_fixture_access::agent_invocation_evidence(
						*application);
				aTerms.note(
					"process",
					static_cast<std::uint64_t>(
						invocation.Statistics.ProcessStarts));
				return aTerms.require(
						   "run-quiescent",
						   invocation.Quiescent
							   && invocation.Statistics.ProcessStarts == 1U)
					&& aTerms.require(
						"cleanups-is-one",
						invocation.Statistics.WorkspaceCleanups == 1U);
			},
			extended_agent_budget)) {
		return fail(*application, "agent-run", delivered.render());
	}
	QApplication::processEvents();

	// BPR-0538. The same terminal the first turn was waited on, for every turn
	// after it: quiescent, with exactly as many processes started and
	// workspaces cleaned up as turns asked for. A stage that read its evidence
	// before its turn ended would be reading the previous turn's.
	const auto awaitTurn = [&application](const std::size_t aTurns) {
		auto       terms   = wait_terms{ };
		const auto settled = wait_until_reporting(
			terms,
			[&application, aTurns](wait_terms& aTerms) {
				const auto invocation = app::detail::
					github_application_fixture_access::agent_invocation_evidence(
						*application);
				aTerms.note(
					"process",
					static_cast<std::uint64_t>(
						invocation.Statistics.ProcessStarts));
				return aTerms.require(
						   "run-quiescent",
						   invocation.Quiescent
							   && invocation.Statistics.ProcessStarts == aTurns)
					&& aTerms.require(
						"cleanups-match",
						invocation.Statistics.WorkspaceCleanups == aTurns);
			},
			extended_agent_budget);
		if (!settled) {
			std::cerr << "agent-finding-workshop-press-turn:" << aTurns << ':'
					  << terms.render() << '\n';
		}
		QApplication::processEvents();
		return settled;
	};

	const auto snapshotRoot = dataRoot / "mcp-snapshot";

	// Detailing the delivered finding is a Qt-local navigation and emits
	// nothing; it is what makes the seven disclosed facts readable.
	auto* const queue
		= app::detail::github_application_fixture_access::finding_queue(
			*application);
	if (queue != nullptr && queue->finding_count() != 0U) {
		queue->select(0U);
		QApplication::processEvents();
	}
	const auto afterDelivery
		= app::detail::github_application_fixture_access::agent_finding_evidence(
			*application);

	// The conversion gesture, on the composed queue, against the finding the
	// run delivered. Nothing is published by it and nothing can be.
	if (queue != nullptr && queue->finding_count() != 0U) {
		auto* const convert
			= queue->action(qt::finding_queue_action::convert_to_draft_comment);
		if (convert == nullptr) {
			return fail(*application, "convert-action");
		}
		convert->trigger();
		QApplication::processEvents();
	}
	const auto afterConversion
		= app::detail::github_application_fixture_access::agent_finding_evidence(
			*application);

	if (course == "workshop-press" && editor->clarify_action() != nullptr) {
		// Answered, so the offer is open again. A control that stayed shut
		// after its own answer arrived would be a workshop good for one
		// question per comment.
		reofferedAfterProposal = editor->clarify_action()->isEnabled();
	}

	// The acceptance gesture, on the shipped editor. This is the one gesture
	// that emits a local draft command, and it emits exactly one.
	if (
		afterConversion.WorkshopProposals == 1U
		&& editor->accept_revision_action() != nullptr
		&& editor->accept_revision_action()->isEnabled()) {
		editor->accept_revision_action()->trigger();
		QApplication::processEvents();
	}
	// BPR-0538. Two more stages, on the press course only, and both driven
	// through the same composed graph: the turn that answers nothing, and the
	// turn that belongs to somebody else.
	auto silentReason       = std::string{ "unread" };
	auto silentPending      = std::string{ "unread" };
	auto silentReoffered    = false;
	auto boundPending       = std::string{ };
	auto boundSession       = false;
	auto unrelatedOperation = std::string{ "unread" };
	auto unrelatedPending   = std::string{ };
	auto unrelatedSession   = false;
	auto unrelatedProposals = std::size_t{ 0U };
	auto acceptedDraftBody  = std::string{ };
	if (course == "workshop-press") {
		acceptedDraftBody = app::detail::github_application_fixture_access::
								agent_finding_evidence(*application)
									.LocalDraftBody;
		// The reviewer's own words again, typed on the shipped editor. The
		// accepted revision above left the draft byte-identical to what this
		// fixture proposes, and a workshop asked to revise text into itself
		// refuses "unchanged" - which would make the stages below prove that
		// refusal instead of what they are about.
		editor->body_editor()->setPlainText(QString::fromUtf8(
			reviewer_original.data(),
			static_cast<qsizetype>(reviewer_original.size())));
		QApplication::processEvents();
		const auto reopened = [&editor]() {
			const auto six = std::array<QAction*, 6U>{
				editor->clarify_action(),
				editor->shorten_action(),
				editor->make_actionable_action(),
				editor->phrase_as_a_question_action(),
				editor->make_more_direct_action(),
				editor->check_the_premise_action(),
			};
			return std::ranges::all_of(six, [](const QAction* aControl) {
				return aControl != nullptr && aControl->isEnabled();
			});
		};
		// Stage two: the fixture answers `shorten` with a completed turn and
		// no findings, which is what an agent with nothing to say does. The
		// press must not survive it: nothing is coming, so the six reopen and
		// the surface says so where the before/after would have been.
		auto* const shorten = editor->shorten_action();
		if (shorten == nullptr || !shorten->isEnabled()) {
			return fail(*application, "workshop-shorten-control");
		}
		shorten->trigger();
		QApplication::processEvents();
		if (!awaitTurn(2U)) {
			return fail(*application, "workshop-silent-turn");
		}
		const auto afterSilent = app::detail::
			github_application_fixture_access::agent_finding_evidence(
				*application);
		silentReason    = afterSilent.WorkshopOperationText;
		silentPending   = afterSilent.WorkshopPendingOperation;
		silentReoffered = reopened();

		// Stage three: a press that is answered, left unconverted, and so
		// still bound to its own turn when the next one runs.
		auto* const clarify = editor->clarify_action();
		if (clarify == nullptr || !clarify->isEnabled()) {
			return fail(*application, "workshop-second-clarify-control");
		}
		clarify->trigger();
		QApplication::processEvents();
		if (!awaitTurn(3U)) {
			return fail(*application, "workshop-bound-turn");
		}
		const auto afterBound = app::detail::
			github_application_fixture_access::agent_finding_evidence(
				*application);
		boundPending = afterBound.WorkshopPendingOperation;
		boundSession = afterBound.WorkshopPendingSessionBound;

		// Stage four: a second, unrelated turn — started from the launch
		// panel, so it is nobody's press — runs, delivers, and is converted.
		// Neither its ending nor its outcome may consume the press stage three
		// left bound: a revision nobody workshopped is proposed with no
		// operation named, and the question stays outstanding.
		panel->start_button()->click();
		QApplication::processEvents();
		if (!awaitTurn(4U)) {
			return fail(*application, "workshop-unrelated-turn");
		}
		if (queue != nullptr && queue->finding_count() != 0U) {
			queue->select(0U);
			QApplication::processEvents();
			auto* const convert
				= queue->action(qt::finding_queue_action::convert_to_draft_comment);
			if (convert == nullptr) {
				return fail(*application, "workshop-unrelated-convert");
			}
			convert->trigger();
			QApplication::processEvents();
		}
		const auto afterUnrelated = app::detail::
			github_application_fixture_access::agent_finding_evidence(
				*application);
		unrelatedOperation = afterUnrelated.WorkshopOperationText;
		unrelatedPending   = afterUnrelated.WorkshopPendingOperation;
		unrelatedSession   = afterUnrelated.WorkshopPendingSessionBound;
		unrelatedProposals = afterUnrelated.WorkshopStatistics.Proposals;
	}

	const auto helperRefusal
		= app::detail::github_application_fixture_access::start_mcp_context_helper(
			*application,
			"{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\"}\n");
	auto helperAnswered = wait_terms{ };
	static_cast<void>(
		helperRefusal != application::mcp_helper_refusal::none
		|| wait_until_reporting(
			helperAnswered,
			[&application](wait_terms& aTerms) {
				const auto current = app::detail::github_application_fixture_access::
					agent_finding_evidence(*application);
				return aTerms.require(
					"helper-completed", current.McpStatistics.Completions == 1U);
			},
			default_agent_budget));

	const auto evidence
		= app::detail::github_application_fixture_access::agent_finding_evidence(
			*application);
	const auto mutation
		= app::detail::github_application_fixture_access::review_mutation_evidence(
			*application);

	// Zero, everywhere, on every course. Converting a finding and accepting a
	// revision are local operations, and a backchannel from the agent host to
	// the RFC-BPR-0008 coordinator would show up here as a call nobody asked
	// for.
	const auto noGithubMutation = mutation.Submissions == 0U
		&& mutation.TargetObservations == 0U && mutation.StoredComments == 0U;

	const auto positiveMarker = evidence.FindingHostComposed
		&& evidence.CommentWorkshopComposed && evidence.McpHelperComposed
		&& evidence.SideChatPanelComposed && evidence.FindingQueueComposed
		&& evidence.QueuedFindings == 1U && evidence.WorkshopProposals == 1U
		&& evidence.LocalDraftBody == agent_suggested
		&& evidence.SideChatPresentations != 0U
		&& exact_helper_launch(evidence, snapshotRoot);

	if (course == "workshop-press") {
		// BPR-0533. Press → invocation → proposal, in the composed graph and
		// with no launch-panel gesture anywhere in it. The wait above already
		// required exactly one process start and one workspace cleanup, so the
		// run these findings came from is the one the press started.
		//
		// The three states of the asked-for operation are read in order, which
		// is what makes the pairing observable rather than assumed: recorded
		// and bound to its turn once the run is quiescent, still unspent while
		// the delivery sits in the queue, and spent — named on the proposal
		// and gone from the record — only by that turn's own outcome.
		const auto pressed = evidence.FindingHostComposed
			&& evidence.CommentWorkshopComposed && noGithubMutation
			&& afterDelivery.FindingStatistics.Deliveries == 1U
			&& afterDelivery.QueuedFindings == 1U
			&& afterDelivery.WorkshopProposals == 0U
			&& afterDelivery.WorkshopPendingOperation == "clarify"
			&& afterDelivery.WorkshopPendingSessionBound
			// Never queued silently. While the turn was unanswered the six
			// controls were shut and said why, so there was no press to queue.
			&& pressedRefusal.find("already running") != std::string::npos
			&& reofferedAfterProposal
			&& afterConversion.WorkshopProposals == 1U
			&& afterConversion.WorkshopStatistics.Proposals == 1U
			&& afterConversion.WorkshopOriginalText == reviewer_original
			&& afterConversion.WorkshopProposalText == agent_suggested
			// The before/after says which of the six produced it, and the
			// question it answered is no longer outstanding.
			&& afterConversion.WorkshopOperationText == "Proposed by: Clarify"
			&& afterConversion.WorkshopPendingOperation.empty()
			&& acceptedDraftBody == agent_suggested
			&& evidence.LocalDraftBody == reviewer_original
			// BPR-0538. The turn that ended without a revision took its press
			// with it: nothing is pending, the six are open, and the reason is
			// stated where the proposal would have been.
			&& silentPending.empty()
			&& silentReason == "No revision: the turn proposed no revision"
			&& silentReoffered
			// And the press that is waiting on its own turn survives a second,
			// unrelated turn — its ending and its converted outcome both. The
			// revision nobody workshopped is proposed with no operation named.
			&& boundPending == "clarify" && boundSession
			&& unrelatedPending == "clarify" && unrelatedSession
			&& unrelatedOperation.empty() && unrelatedProposals == 2U;
		if (!pressed) {
			std::cerr << "agent-finding-workshop-press-evidence:"
					  << afterDelivery.FindingStatistics.Deliveries << ':'
					  << afterDelivery.QueuedFindings << ':'
					  << afterConversion.WorkshopProposals << ':'
					  << mutation.Submissions
					  << "\npending-after-delivery:"
					  << afterDelivery.WorkshopPendingOperation
					  << "\npending-bound:"
					  << (afterDelivery.WorkshopPendingSessionBound ? "yes" : "no")
					  << "\npress-refusal:" << pressedRefusal
					  << "\nreoffered:"
					  << (reofferedAfterProposal ? "yes" : "no")
					  << "\noperation-text:"
					  << afterConversion.WorkshopOperationText
					  << "\npending-after-conversion:"
					  << afterConversion.WorkshopPendingOperation
					  << "\nworkshop-proposal:"
					  << afterConversion.WorkshopProposalText
					  << "\ndraft-body:" << evidence.LocalDraftBody
					  << "\naccepted-draft-body:" << acceptedDraftBody
					  << "\nsilent-reason:" << silentReason
					  << "\nsilent-pending:" << silentPending
					  << "\nsilent-reoffered:" << (silentReoffered ? "yes" : "no")
					  << "\nbound-pending:" << boundPending << ':'
					  << (boundSession ? "bound" : "unbound")
					  << "\nunrelated-pending:" << unrelatedPending << ':'
					  << (unrelatedSession ? "bound" : "unbound")
					  << "\nunrelated-operation:" << unrelatedOperation
					  << "\nunrelated-proposals:" << unrelatedProposals
					  << "\npanel-status:" << evidence.AgentPanelStatusText << '\n';
		}
		if (!shutdown(*application) || !pressed) {
			return EXIT_FAILURE;
		}
		std::cout << "agent-finding-workshop-press:pass\n";
		return EXIT_SUCCESS;
	}

	if (course == "positive" || course == "full-agent") {
		const auto exact = positiveMarker && noGithubMutation
			&& afterDelivery.FindingStatistics.Deliveries == 1U
			&& afterDelivery.FindingStatistics.DeliveredFindings == 1U
			&& afterDelivery.QueuedFindings == 1U
			&& afterDelivery.FrozenFileCount == 1U
			&& afterDelivery.FrozenSnapshotWrites == 1U
			&& afterDelivery.QueuedClaim.find(agent_claim) != std::string::npos
			// A start gesture is not a chat turn, so the frame this run was
			// handed carried no question. The fixture counts what it received
			// and says so: zero here is the other half of the chat course's
			// exactly-one.
			&& afterDelivery.QueuedClaim.find("BPR_CHAT_ECHO_0517 count=0 text=")
				!= std::string::npos
			&& afterDelivery.QueuedAnchorState == "anchored (none)"
			&& afterDelivery.WorkshopProposals == 0U
			&& afterConversion.FindingStatistics.Actions == 1U
			&& afterConversion.FindingStatistics.DraftCommands == 1U
			&& afterConversion.FindingStatistics.StoredNotes == 0U
			&& afterConversion.WorkshopStatistics.Proposals == 1U
			&& afterConversion.WorkshopOriginalText == reviewer_original
			&& afterConversion.WorkshopProposalText == agent_suggested
			// The conversion alone changes no draft. The reviewer's bytes are
			// still theirs until they accept.
			&& afterConversion.LocalDraftBody == reviewer_original
			&& evidence.LocalDraftBody == agent_suggested
			&& helperRefusal == application::mcp_helper_refusal::none
			&& evidence.McpStatistics.Starts == 1U
			&& evidence.McpStatistics.Refusals == 0U
			&& evidence.McpStatistics.Completions == 1U
			&& evidence.McpAnswer.find("snapshot_binding") != std::string::npos
			&& evidence.McpAnswer.find("read_snapshot_file") != std::string::npos
			&& std::filesystem::exists(snapshotRoot / "binding")
			&& std::filesystem::exists(snapshotRoot / "alignment");
		if (!exact) {
			std::cerr << "agent-finding-positive-evidence:"
					  << afterDelivery.FindingStatistics.Deliveries << ':'
					  << afterDelivery.QueuedFindings << ':'
					  << afterDelivery.FrozenFileCount << ':'
					  << afterDelivery.FrozenSnapshotWrites << ':'
					  << afterConversion.WorkshopProposals << ':'
					  << afterConversion.FindingStatistics.DraftCommands << ':'
					  << static_cast<int>(helperRefusal) << ':'
					  << evidence.McpStatistics.Completions << ':'
					  << mutation.Submissions << ':' << mutation.TargetObservations
					  << "\nqueued-claim:" << afterDelivery.QueuedClaim
					  << "\nanchor-state:" << afterDelivery.QueuedAnchorState
					  << "\nworkshop-original:" << afterConversion.WorkshopOriginalText
					  << "\nworkshop-proposal:" << afterConversion.WorkshopProposalText
					  << "\ndraft-body:" << evidence.LocalDraftBody
					  << "\nmcp-answer:" << evidence.McpAnswer
					  << "\npanel-status:" << evidence.AgentPanelStatusText << '\n';
		}
		if (course == "positive") {
			if (!shutdown(*application) || !exact) {
				return EXIT_FAILURE;
			}
			std::cout << "agent-finding-composition:pass\n";
			return EXIT_SUCCESS;
		}
		// The full-agent half. The launch above is the full agent session: a
		// real child process, its own workspace, a returned structured
		// payload, and a promotion the reviewer accepted. What this course
		// adds is the *absence* the criterion is about.
		//
		// Two readings, because one alone would not be enough. The composed
		// run records zero GitHub mutation calls of any kind, so nothing left
		// the application while the agent was working or afterwards. And the
		// composition source itself names the RFC-BPR-0008 coordinator zero
		// times and carries exactly one publication call site - the reviewer's
		// own `begin_publish` branch - so there is no second edge for a
		// backchannel to travel along. The positive direction, that a
		// publication does round-trip through that one path, is
		// `application_review_round_trip_composition_contract`, which pins
		// this same file by digest.
		auto composition = std::ifstream{
			std::filesystem::path{ BPR_COMPOSITION_SOURCE },
			std::ios::binary,
		};
		const auto source = std::string{
			std::istreambuf_iterator<char>{ composition },
			std::istreambuf_iterator<char>{ },
		};
		const auto occurrences
			= [&source](const std::string_view aToken) -> std::size_t {
			auto count = std::size_t{ 0U };
			auto at    = source.find(aToken);
			while (at != std::string::npos) {
				++count;
				at = source.find(aToken, at + aToken.size());
			}
			return count;
		};
		const auto noSecondEdge = !source.empty()
			&& occurrences("review_mutation_coordinator") == 0U
			&& occurrences("ReviewMutationHost->publish(") == 1U
			&& occurrences("->submit(") == 1U && occurrences(".submit(") == 0U
			&& occurrences("->observe_target(") == 0U
			&& occurrences(".observe_target(") == 0U
			// The finding host, the workshop host, and the helper supervisor
			// each reach exactly the ports they were given. None of them is
			// handed the mutation host, and no call site here hands it over.
			&& occurrences("FindingHost = std::make_unique<") == 1U
			&& occurrences("WorkshopHost = std::make_unique<") == 1U
			&& occurrences("McpHelper = std::make_unique<") == 1U
			&& source.find("FindingHost->deliver(") != std::string::npos
			&& source.find("ReviewMutationHost") != std::string::npos;
		if (!shutdown(*application) || !exact || !noSecondEdge) {
			std::cerr << "agent-finding-full-agent:" << exact << ':'
					  << noSecondEdge << ':' << source.size() << ':'
					  << mutation.Submissions << ':' << mutation.TargetObservations
					  << ':' << mutation.StoredComments << '\n';
			return EXIT_FAILURE;
		}
		std::cout << "agent-finding-full-agent:pass\n";
		return EXIT_SUCCESS;
	}

	// Every construction cut asserts the same three things about a different
	// construction: the gesture still fired, its downstream effect is zero, and
	// the shared positive marker is false.
	const auto invocation
		= app::detail::github_application_fixture_access::agent_invocation_evidence(
			*application);
	const auto gestureObserved = panel->statistics().StartIntents == 1U
		&& invocation.AgentProcessLauncherInvocations == 1U
		&& invocation.Statistics.ProcessStarts == 1U;
	auto effectsZero = false;
	if (course == "finding-host-construction-cut") {
		effectsZero = !evidence.FindingHostComposed
			&& evidence.FindingStatistics
				== application::agent_finding_host_statistics{ }
			&& evidence.QueuedFindings == 0U && evidence.WorkshopProposals == 0U
			&& evidence.LocalDraftBody == reviewer_original;
	} else if (course == "queue-construction-cut") {
		effectsZero = !evidence.FindingQueueComposed
			&& evidence.QueuedFindings == 0U
			&& evidence.FindingStatistics.Actions == 0U
			&& evidence.WorkshopProposals == 0U
			&& evidence.LocalDraftBody == reviewer_original;
	} else if (course == "workshop-construction-cut") {
		effectsZero = !evidence.CommentWorkshopComposed
			&& evidence.WorkshopStatistics
				== application::agent_comment_workshop_statistics{ }
			&& evidence.WorkshopProposals == 0U
			&& evidence.LocalDraftBody == reviewer_original
			&& evidence.RefusalsPresented == 1U;
	} else if (course == "mcp-construction-cut") {
		effectsZero = !evidence.McpHelperComposed
			&& !evidence.McpLaunch.has_value()
			&& evidence.McpStatistics
				== application::mcp_helper_supervisor_statistics{ }
			&& evidence.FrozenSnapshotWrites == 0U
			&& helperRefusal == application::mcp_helper_refusal::no_helper
			&& !std::filesystem::exists(snapshotRoot);
	} else {
		effectsZero = !evidence.SideChatPanelComposed
			&& evidence.SideChatPresentations == 0U
			&& evidence.SideChatIntents == 0U;
	}
	const auto passed = gestureObserved && effectsZero && !positiveMarker
		&& noGithubMutation;
	if (!passed) {
		std::cerr << "agent-finding-cut-evidence:" << gestureObserved << ':'
				  << effectsZero << ':' << positiveMarker << ':'
				  << noGithubMutation << ':' << evidence.QueuedFindings << ':'
				  << evidence.WorkshopProposals << ':'
				  << evidence.RefusalsPresented << ':'
				  << evidence.SideChatPresentations << ':'
				  << evidence.FrozenSnapshotWrites << "\ndraft-body:"
				  << evidence.LocalDraftBody << '\n';
	}
	if (!shutdown(*application) || !passed) {
		return EXIT_FAILURE;
	}
	std::cout << "agent-finding-" << course << ":pass\n";
	return EXIT_SUCCESS;
}
