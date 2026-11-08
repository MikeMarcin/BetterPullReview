#include "bpr/app/github_application.hpp"
#include "bpr/application/review_mutation_diagnostics.hpp"
#include "bpr/core/comment_edit.hpp"
#include "bpr/core/conversation_mutation_lifecycle.hpp"
#include "bpr/core/description_edit.hpp"
#include "bpr/core/draft_state.hpp"
#include "bpr/core/review_role.hpp"
#include "bpr/core/reviewer_request.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "github_application/profile_hosts.hpp"
#include "github_application/profile_runtime.hpp"
#include "github_application/review_mutation_selection.hpp"
#include "github_application/window_composition.hpp"
#include "github_application/workbench_composition.hpp"
#include "github_cli_runtime.hpp"

#include "review_mutation_execution_authority.hpp"

#include "live_review_mutation_provider.hpp"

#if defined(_WIN32)
	#include "windows_title_row_host.hpp"
#endif

#if defined(BPR_BUILD_TESTING)
	#include "bpr/application/github_window_read_host.hpp"
	#include "github_public_path_fixture.hpp"
#endif

// [[BPR-1179]]. BPR_AGENT_INTEGRATIONS and BPR_LOCAL_AUTOMATION are defined
// only in a graph that builds those component families. Every block they
// guard here composes, drives or reports one of them; without it the window
// withdraws the agent controls and no owner command endpoint is bound, and
// nothing else in this composition changes.
#include "bpr/adapters/sqlite/review_mutation_recovery_store.hpp"

#if defined(BPR_AGENT_INTEGRATIONS)
	#include "bpr/application/agent_finding_host.hpp"
#endif
#include "bpr/application/github_application_host.hpp"
#include "bpr/application/pull_request_overview.hpp"
#include "bpr/application/review_mutation_application_host.hpp"
#include "bpr/application/review_synchronization.hpp"
#include "bpr/application/workbench_application_host.hpp"
#include "bpr/core/reply_draft_standing.hpp"
#include "bpr/qt/finding_queue_view.hpp"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QTextStream>
#include <QTimer>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::app {
	// [[BPR-0802]]. From the environment rather than a flag because a flag is
	// something a course can forget, and the thing it would forget is the one
	// that keeps an unattended suite from opening a browser on the reviewer's
	// machine. `bpr_add_network_isolated_test` injects it into every
	// registered row on all three platforms, and a child the course starts
	// inherits it.
	//
	// Any non-empty value means inside. The failure mode has to be the safe
	// one: an unrecognised value that meant "outside" would be a suite allowed
	// to start a program, while one that means "inside" is at worst a person's
	// own run composing a recorder, which they would see immediately.
	//
	// [[BPR-0983]]. Read only in the tests-enabled graph. The name is in no
	// tests-disabled binary, and the tests-off Release scan in
	// `tests/end_to_end/composition_contract_tests.py` looks for it.
	auto inside_automated_test_boundary() -> bool
	{
#if defined(BPR_BUILD_TESTING)
		return !qEnvironmentVariableIsEmpty("BPR_AUTOMATED_TEST_BOUNDARY");
#else
		return false;
#endif
	}

	namespace {
		using adapters::github::github_host_profile_candidate;
		using adapters::github::github_profile_provenance;
		using adapters::github::make_live_review_mutation_provider;
		using adapters::github::validate_github_host_profile;
		using adapters::github::validated_github_host_profile;
		using adapters::github::detail::github_provider;
		using adapters::github::detail::github_wire_transport_factory_port;
		using adapters::github::detail::make_github_provider;
		using adapters::github::detail::native_github_wire_transport_factory;

	}

#if defined(BPR_AGENT_INTEGRATIONS)
	static_assert(
		qt::finding_queue_action_count == application::agent_finding_action_count,
		"the composed finding vocabulary and the queue's must stay one list");
#endif

	void github_application::implementation::compose_review_mutation(
		const application::accepted_pull_request_handoff& aHandoff)
	{
		// A fixture retry waiting for a draining host may already have been
		// superseded by another accepted read. Never compose that old context.
		if (!LastAcceptedHandoff || workbench_authority(aHandoff) != workbench_authority(*LastAcceptedHandoff)) {
			return;
		}
		const auto binding = WindowRead && WindowRead->accepted_snapshot()
			? select_observed_review_mutation(aHandoff, *WindowRead->accepted_snapshot(), true)
			: std::nullopt;
		const auto scope   = core::conversation_mutation_scope{
			.Session            = aHandoff.ApplicationSession,
			.Profile            = aHandoff.Profile,
			.ProfileRevision    = aHandoff.ProfileRevision,
			.Host               = aHandoff.Host,
			.HostAuthority      = aHandoff.HostAuthority,
			.Account            = aHandoff.Account,
			.Viewer             = aHandoff.Viewer,
			.IdentityGeneration = aHandoff.IdentityGeneration,
			.Repository         = aHandoff.Repository,
			.PullRequest        = aHandoff.PullRequest,
			.Number             = aHandoff.Number,
			.Base               = application::recorded_base(aHandoff),
			.Head               = aHandoff.Head,
			.Mode               = aHandoff.Mode,
		};
		const auto bindingCurrent = binding && ConversationMutationBinding
			&& binding->Target == ConversationMutationBinding->Target
			&& binding->ActingIdentity == ConversationMutationBinding->ActingIdentity;
		const auto retirement       = core::adopt_conversation_mutations(ConversationMutations, ConversationOverlay, scope, bindingCurrent);
		ConversationMutationBinding = binding;
		// [[BPR-1477]]. Retired with the composition below, and needed to
		// say again why a held comment's outcome is unknown.
		const auto heldRetry = RetryableReviewDraft;
		retire_review_mutation_host(retirement);
		InlineReviewBinding.reset();
		if (Window != nullptr && Window->review_editor() != nullptr) {
			Window->review_editor()->present(
				qt::review_draft_editor_clear_reason::composition_retired);
		}
		// [[BPR-1374]]. The reply being written is held again once its
		// line is chosen under the new composition; before, a Refresh
		// left it saved but shown nowhere.
		if (ReviewDraft) {
			ResumeReply = ReviewDraft->ReplyToThread && ReviewDraft->State == core::review_draft_state::drafting ? std::optional{ ReviewDraft->Id } : std::nullopt;
			// [[BPR-1475]]. So is a new comment being written: the window
			// rereads by itself after publishing, and that takes no note away.
			// [[BPR-1477]]. And one whose publication answer was lost, with
			// its retry: before, a Refresh left it saved, locked and shown
			// nowhere, and Retry was gone.
			ResumeComment        = core::resume_comment_after_refresh(std::span{ &*ReviewDraft, 1U }, ReviewDraft->Id);
			ResumeCommentOpen    = ResumeComment && DraftOpenAtRead;
			ResumeCommentFailure = heldRetry && heldRetry->Draft.Id == ReviewDraft->Id ? heldRetry->Failure : application::review_mutation_failure::unavailable;
		}
		DraftOpenAtRead = false;
		ReviewDraft.reset();
		ReviewDraftRevision = 0U;
		SelectedSemanticSelection.reset();
		refresh_agent_private_context_choices();
		publish_preflight_selection();
		ReviewProjection.clear();
		// [[BPR-1824]]. Until a write connection is composed below the
		// cards say why Resolve cannot go, not that it is "unavailable".
		if (Window != nullptr) {
			Window->set_thread_resolution_connection(thread_resolution_connection_refusal(), false);
		}
#if defined(BPR_BUILD_TESTING)
		if (Fixture == nullptr || Window == nullptr
			|| Window->review_editor() == nullptr || WorkbenchHost == nullptr
			|| Host == nullptr || !WindowRead->accepted_snapshot().has_value()) {
			return;
		}
		if (!RetiredReviewMutationHosts.empty()) {
			auto retry = std::make_shared<application::accepted_pull_request_handoff>(
				clone_without_smart_git(aHandoff));
			QTimer::singleShot(0, Window.get(), [this, retry] {
				if (!Closing) {
					compose_review_mutation(*retry);
				}
			});
			return;
		}
		auto selection = select_observed_review_mutation(
			aHandoff, *WindowRead->accepted_snapshot(), true);
		if (!selection.has_value()) {
			return;
		}
		InlineReviewBinding = *selection;
		const auto target   = selection->Target.Semantic;
		Fixture->prepare_review_mutation(target);
		Fixture->set_private_lineage(PrivateReview);
		if (ReviewMutationDecisionGate == nullptr) {
			ReviewMutationDecisionGate = std::make_unique<
				gated_review_mutation_decision_executor>(DecisionLane);
		}
		ReviewMutationHost = std::make_unique<
			application::review_mutation_application_host>(
			Fixture->review_mutation_provider(),
			selection->Target,
			selection->ActingIdentity,
			aHandoff.ApplicationSession,
			WorkbenchHost->generation(),
			application::review_mutation_execution_grant{ },
			ReviewMutationEffects,
			*ReviewMutationDecisionGate,
			*this,
			this, insights_capture(aHandoff));
		FixtureReviewMutationHostComposed = true;
#else
		if (Host == nullptr || WorkbenchHost == nullptr
			|| !WindowRead->accepted_snapshot().has_value()) {
			return;
		}
		auto selection = AttendedReviewMutationMode
			? select_designated_review_mutation(aHandoff, *WindowRead->accepted_snapshot())
			: select_observed_review_mutation(aHandoff, *WindowRead->accepted_snapshot(), true);
		if (!selection.has_value()) {
			LiveReviewMutationGrant.reset();
			return;
		}
		if (AttendedReviewMutationMode && LiveReviewMutationGrant == nullptr) {
			return;
		}
		auto lease = WindowRead->mutation_transport();
		if (lease == nullptr) {
			return;
		}
		const auto repository = selection->Target.Repository.Owner + "/"
			+ selection->Target.Repository.Name;
		auto recovery = adapters::sqlite::make_review_mutation_recovery_store({
			.StorageRoot = ApplicationDataRoot,
			.Host        = std::string{ selection->Target.Hostname.value() },
			.Account     = std::to_string(selection->ActingIdentity.Account.value()),
			.Repository  = repository,
			.PullRequest = static_cast<std::int64_t>(
				selection->Target.Number.value()),
		});
		if (!recovery.has_value()) {
			LiveReviewMutationGrant.reset();
			return;
		}
		auto provider = make_live_review_mutation_provider(
			std::move(lease),
			selection->Target,
			selection->ActingIdentity,
			std::move(*recovery), PrivateReview.get(), DiagnosticLog);
		if (provider.has_value()) {
			InlineReviewBinding = *selection;
			auto erased         = std::unique_ptr<application::review_mutation_port>{
				std::move(*provider)
			};
			// [[BPR-1828]]. Every resolve, reaction, edit, reviewer
			// request and publication leaves a github.review_mutation record.
			erased             = application::with_review_mutation_diagnostics(std::move(erased), DiagnosticLog);
			ReviewMutationHost = std::make_unique<
				application::review_mutation_application_host>(
				std::move(erased),
				std::move(selection->Target),
				std::move(selection->ActingIdentity),
				aHandoff.ApplicationSession,
				WorkbenchHost->generation(),
				std::move(LiveReviewMutationGrant),
				ReviewMutationEffects,
				DecisionLane,
				*this,
				this, insights_capture(aHandoff));
			ReviewMutationHost->repair_pending_publications();
		} else {
			LiveReviewMutationGrant.reset();
		}
#endif
		if (ReviewMutationHost == nullptr || Window == nullptr
			|| Window->review_editor() == nullptr) {
			return;
		}
		// [[BPR-1826]]. A closed or merged pull request's connection
		// resolves and unresolves threads and nothing else: its host
		// offers no other capability, and no draft is published from it.
		if (InlineReviewBinding && !InlineReviewBinding->Target.Open) {
			Window->set_thread_resolution_provider(ReviewMutationHost->capabilities().ResolvesThreads);
			present_reviewer_offer();
			present_description_offer();
			present_comment_edit_offer();
			present_draft_state_offer();
			return;
		}
		if (!ensure_shared_drafts(aHandoff)) {
			return;
		}
		auto* const editor = Window->review_editor();
		editor->describe_target(QStringLiteral("%1 · %2/%3#%4 at %5 as %6")
									.arg(QString::fromStdString(std::string{ aHandoff.Hostname.value() }))
									.arg(QString::fromStdString(aHandoff.Locator.Owner))
									.arg(QString::fromStdString(aHandoff.Locator.Name))
									.arg(aHandoff.Number.value())
									.arg(QString::fromUtf8(
										core::canonical_hex(aHandoff.Head).view()))
									.arg(QString::fromStdString(WindowRead->accepted_snapshot()->Viewer.Login)));
		editor->present(ReviewMutationHost->capabilities());
		// [[BPR-1741]]. The GitHub CLI connection resolves threads
		// too, through `gh api graphql`.
		Window->set_thread_resolution_provider(ReviewMutationHost->capabilities().ResolvesThreads);
		present_reviewer_offer();
		present_description_offer();
		present_comment_edit_offer();
		present_draft_state_offer();
	}

	/// [[BPR-1741]]. GitHub's word on what the viewer may do to a
	/// thread, from the read; unknown is not a refusal.
	auto github_application::implementation::thread_permission(const application::github_review_thread_record& aThread) noexcept -> core::thread_resolution_permission
	{
		return {
			.MayResolve   = aThread.ViewerResolvePermission != application::github_permission_state::denied,
			.MayUnresolve = aThread.ViewerUnresolvePermission != application::github_permission_state::denied,
		};
	}

	/// [[BPR-1741]]. The posts' reactions and the threads'
	/// resolutions as the viewer should see them: the accepted read, with
	/// the viewer's own changes the core keeps until GitHub and a later
	/// read agree with them.
	void github_application::implementation::present_conversation_state()
	{
		if (Closing || Window == nullptr || !WindowRead || !WindowRead->accepted_snapshot()) {
			return;
		}
		const auto& snapshot  = *WindowRead->accepted_snapshot();
		const auto  viewer    = snapshot.Viewer.Login;
		auto        reactions = std::map<std::string, core::reaction_summary>{ };
		const auto  read      = snapshot.ReactionsAvailability == application::github_reaction_availability::available
			|| snapshot.ReactionsAvailability == application::github_reaction_availability::partial;
		if (read) {
			for (const auto& record : snapshot.Reactions) {
				if (auto shown = core::project_reaction_summary(&record.Reactions, ConversationOverlay, record.Subject, viewer)) {
					reactions.insert_or_assign(record.Subject, std::move(*shown));
				}
			}
		}
		for (const auto& [subject, answered] : ConversationOverlay.Answered) {
			if (!reactions.contains(subject)) {
				if (auto shown = core::project_reaction_summary(nullptr, ConversationOverlay, subject, viewer)) {
					reactions.insert_or_assign(subject, std::move(*shown));
				}
			}
		}
		auto resolutions = std::map<std::string, qt::thread_resolution_presentation>{ };
		for (const auto& thread : snapshot.ReviewThreads) {
			const auto identity  = thread_identity(thread);
			const auto projected = core::project_thread_resolution(thread.Resolved, ConversationOverlay, identity);
			// [[BPR-1841]]. No re-read follows a resolve, so the read's
			// permission is projected with its state.
			const auto permitted = core::project_thread_resolution_permission(thread.Resolved, projected.Resolved, thread_permission(thread));
			resolutions.insert_or_assign(identity, qt::thread_resolution_presentation{
													   .Resolved     = projected.Resolved,
													   .Pending      = projected.Pending,
													   .CanResolve   = permitted.MayResolve,
													   .CanUnresolve = permitted.MayUnresolve,
												   });
		}
		Window->present_conversation_state(std::move(reactions), std::move(resolutions));
		present_overview_resolutions();
	}

	/// [[BPR-1741]]. A new accepted read reconciles the viewer's
	/// changes with what GitHub now says; another pull request drops them.
	void github_application::implementation::observe_conversation_state()
	{
		if (!WindowRead || !WindowRead->accepted_snapshot()) {
			return;
		}
		const auto& snapshot          = *WindowRead->accepted_snapshot();
		auto        observation       = core::conversation_read_observation{ };
		observation.ReactionsObserved = snapshot.ReactionsAvailability == application::github_reaction_availability::available;
		for (const auto& record : snapshot.Reactions) {
			observation.Reactions.insert_or_assign(record.Subject, record.Reactions);
		}
		for (const auto& thread : snapshot.ReviewThreads) {
			observation.Resolved.insert_or_assign(thread_identity(thread), thread.Resolved);
		}
		core::observe_conversation(ConversationOverlay, observation);
	}

	/// [[BPR-1741]]. A post's reaction: the core decides from the
	/// session's anonymity, what was read of the post and GitHub's
	/// permission, and whether the same change is still unanswered; the
	/// count changes at once, and the host writes it under a one-shot grant.
	void github_application::implementation::react_to_post(const qt::reaction_gesture& aGesture)
	{
		if (Closing || Window == nullptr || aGesture.Subject.empty()) {
			return;
		}
		auto* const panel  = Window->conversation_panel();
		const auto  notice = [panel](const QString& aText) {
			if (panel != nullptr) {
				panel->present_notice(aText);
			}
		};
		const auto anonymous = Host != nullptr
			&& std::holds_alternative<core::anonymous_intent_refused>(Host->public_preview_legality(core::anonymous_read_intent::react_to_comment));
		const auto refusal = core::decide_reaction(core::reaction_input{
			.AnonymousRead  = anonymous,
			.Observed       = aGesture.Observed,
			.ViewerCanReact = aGesture.ViewerCanReact,
			.InFlight       = core::reaction_in_flight(ConversationOverlay, aGesture.Subject, aGesture.Content),
		});
		++ReactionGestures;
		switch (refusal) {
		case core::reaction_refusal::anonymous_read:
			notice(QStringLiteral("Sign in to react; a public preview only reads."));
			return;
		case core::reaction_refusal::unobserved:
			notice(QStringLiteral("Reactions on this comment have not been read yet; refresh and try again."));
			return;
		case core::reaction_refusal::not_permitted:
			notice(QStringLiteral("GitHub does not let you react to this comment."));
			return;
		case core::reaction_refusal::in_flight:
			notice(QStringLiteral("Waiting for GitHub to answer the last change to this reaction."));
			return;
		case core::reaction_refusal::none:
			break;
		}
		if (ReviewMutationHost == nullptr || !ReviewMutationHost->capabilities().Reacts) {
			notice(QStringLiteral("Reactions are unavailable for this pull request."));
			return;
		}
		if (!core::begin_reaction_change(ConversationOverlay, aGesture.Subject, aGesture.Content, aGesture.Add)) {
			return;
		}
		present_conversation_state();
		auto request = application::review_reaction_publication_request{
			.Subject = aGesture.Subject,
			.Content = aGesture.Content,
			.Add     = aGesture.Add,
		};
		auto grant = review_mutation_gesture_grant(
			std::nullopt,
			[&request](const core::application_session_id           aSession,
					   const core::request_generation               aGeneration,
					   application::review_mutation_target_binding  aTarget,
					   application::review_mutation_acting_identity aActor) {
				return application::detail::review_mutation_user_authority_issuer::issue_reaction(
					aSession, aGeneration, std::move(aTarget), std::move(aActor), request);
			});
		const auto context = ReviewMutationHost->react(std::move(request), std::move(grant));
		if (!core::retain_conversation_mutation(ConversationMutations, { context.Request, context.Session, context.Generation },
												{ core::conversation_mutation_kind::reaction, aGesture.Subject, aGesture.Content, aGesture.Add })) {
			core::abandon_reaction_change(ConversationOverlay, aGesture.Subject, aGesture.Content);
			present_conversation_state();
		}
	}

	void github_application::implementation::publish_reaction(const application::review_reaction_terminal& aTerminal)
	{
		if (Closing || Window == nullptr) {
			return;
		}
		if (!core::settle_conversation_mutation(ConversationMutations, ConversationOverlay,
												{ aTerminal.Context.Request, aTerminal.Context.Session, aTerminal.Context.Generation },
												{ core::conversation_mutation_kind::reaction, aTerminal.Request.Subject, aTerminal.Request.Content, aTerminal.Request.Add },
												aTerminal.Result.has_value(), aTerminal.Result ? std::optional{ aTerminal.Result->Reactions } : std::nullopt)) {
			return;
		}
		if (aTerminal.Result.has_value()) {
			++ReactionCompletions;
		} else {
			++ReactionFailures;
			if (auto* const panel = Window->conversation_panel(); panel != nullptr) {
				const auto words = QString::fromUtf8(core::reaction_label(aTerminal.Request.Content).data(),
													 static_cast<qsizetype>(core::reaction_label(aTerminal.Request.Content).size()));
				panel->present_notice(aTerminal.Result.error() == application::review_mutation_failure::not_confirmed
										  ? QStringLiteral("GitHub does not show the %1 reaction; nothing was sent twice. Try again.").arg(words)
										  : QStringLiteral("The %1 reaction could not be %2: %3.")
												.arg(words, aTerminal.Request.Add ? QStringLiteral("added") : QStringLiteral("removed"),
													 QString::fromUtf8(application::name(aTerminal.Result.error()))));
			}
		}
		present_conversation_state();
	}

	/// [[BPR-1774]]. The signed-in login and the pull request's author
	/// as the accepted read gave them; empty when either is unknown or the
	/// session reads anonymously.
	auto github_application::implementation::description_edit_logins() const -> std::pair<std::string, std::string>
	{
		if (!WindowRead || !WindowRead->accepted_snapshot()) {
			return { };
		}
		const auto& snapshot  = *WindowRead->accepted_snapshot();
		const auto  anonymous = Host != nullptr
			&& std::holds_alternative<core::anonymous_intent_refused>(Host->public_preview_legality(core::anonymous_read_intent::react_to_comment));
		if (anonymous || !snapshot.PullRequest.Author.has_value()) {
			return { };
		}
		return { snapshot.PullRequest.Author->Login, snapshot.Viewer.Login };
	}

	/// [[BPR-1774]]. Edit on the description is offered to the pull
	/// request's author, the core decides, when the connection in use can
	/// write it; everyone else sees nothing new.
	void github_application::implementation::present_description_offer()
	{
		if (Closing || Window == nullptr || Window->overview_page() == nullptr) {
			return;
		}
		const auto [author, viewer] = description_edit_logins();
		Window->overview_page()->set_description_editable(core::description_edit_offered(author, viewer) && ReviewMutationHost != nullptr
														  && ReviewMutationHost->capabilities().EditsDescription);
	}

	/// [[BPR-1774]]. Save in the description editor: the core decides on
	/// the gesture (the author, a change, within GitHub's length), the host
	/// writes once under a one-shot grant after GitHub, asked first, still
	/// has the original. Nothing is sent before Save, and a refusal is said
	/// beside the editor.
	void github_application::implementation::save_description(const qt::overview_description_save& aSave)
	{
		auto* const page = Window != nullptr ? Window->overview_page() : nullptr;
		if (Closing || page == nullptr) {
			return;
		}
		++DescriptionEditGestures;
		const auto [author, viewer] = description_edit_logins();
		const auto refusal          = core::decide_description_edit({ .Author = author, .Viewer = viewer, .Original = aSave.Original, .Edited = aSave.Body });
		switch (refusal) {
		case core::description_edit_refusal::not_author:
			++DescriptionEditRefusals;
			page->present_description_status(QStringLiteral("Only the pull request's author can edit its description here."));
			return;
		case core::description_edit_refusal::unchanged:
			++DescriptionEditRefusals;
			page->present_description_status(QStringLiteral("Nothing has changed, so there is nothing to save."));
			return;
		case core::description_edit_refusal::too_long:
			++DescriptionEditRefusals;
			page->present_description_status(QStringLiteral("GitHub keeps at most 65,536 characters; this description has %1.")
												 .arg(QLocale{ }.toString(static_cast<qulonglong>(core::description_characters(aSave.Body)))));
			return;
		case core::description_edit_refusal::not_permitted:
		case core::description_edit_refusal::conflict:
		case core::description_edit_refusal::none:
			break;
		}
		if (ReviewMutationHost == nullptr || !ReviewMutationHost->capabilities().EditsDescription) {
			++DescriptionEditRefusals;
			page->present_description_status(QStringLiteral("This connection cannot edit the description."));
			return;
		}
		if (PendingDescriptionEdit.has_value()) {
			page->present_description_status(QStringLiteral("Waiting for GitHub to answer the last save."), true);
			return;
		}
		auto request = application::review_description_publication_request{ .Original = aSave.Original, .Body = aSave.Body };
		auto grant   = review_mutation_gesture_grant(
			std::nullopt,
			[&request](const core::application_session_id           aSession,
					   const core::request_generation               aGeneration,
					   application::review_mutation_target_binding  aTarget,
					   application::review_mutation_acting_identity aActor) {
				return application::detail::review_mutation_user_authority_issuer::issue_description(
					aSession, aGeneration, std::move(aTarget), std::move(aActor), request);
			});
		page->present_description_status(QStringLiteral("Saving the description to GitHub…"), true);
		PendingDescriptionEdit = ReviewMutationHost->edit_description(std::move(request), std::move(grant));
	}

	void github_application::implementation::publish_description(const application::review_description_terminal& aTerminal)
	{
		if (Closing || Window == nullptr || PendingDescriptionEdit != std::optional{ aTerminal.Context }) {
			return;
		}
		PendingDescriptionEdit.reset();
		auto* const page = Window->overview_page();
		if (page == nullptr) {
			return;
		}
		if (!aTerminal.Result.has_value()) {
			++DescriptionEditFailures;
			auto words = QString{ };
			switch (aTerminal.Result.error()) {
			case application::review_mutation_failure::refused_by_server:
				words = QStringLiteral("GitHub does not let you edit this description. Nothing was saved.");
				break;
			case application::review_mutation_failure::not_confirmed:
				words = QStringLiteral("GitHub does not show the new description; it was not sent twice. Check the pull request, then save again.");
				break;
			case application::review_mutation_failure::incompatible_body:
				words = QStringLiteral("GitHub cannot take this text. Nothing was saved.");
				break;
			case application::review_mutation_failure::target_moved:
			case application::review_mutation_failure::undesignated_target:
			case application::review_mutation_failure::target_unavailable:
				words = QStringLiteral("The pull request changed on GitHub, so nothing was saved. Refresh and try again.");
				break;
			case application::review_mutation_failure::cancelled:
				words = QStringLiteral("Saving was stopped. Nothing more is sent.");
				break;
			default:
				words = QStringLiteral("The connection could not save the description (%1). Your text is kept.")
							.arg(QString::fromUtf8(application::name(aTerminal.Result.error())));
				break;
			}
			page->present_description_status(words);
			return;
		}
		if (aTerminal.Result->Conflict) {
			// Never overwritten silently: the editor says so, keeps the typed
			// text, and offers GitHub's newer text to go on from.
			++DescriptionEditConflicts;
			page->present_description_status(QStringLiteral("Someone changed the description on GitHub since you began. Nothing was saved."));
			page->offer_newer_description(aTerminal.Result->Body);
			return;
		}
		++DescriptionEditCompletions;
		page->close_description_editor();
		// The description shows GitHub's answer the way every other fact
		// comes back: the next read.
		if (auto* const refresh = Window->github_refresh_action(); refresh != nullptr && refresh->isEnabled()) {
			Window->run_automatic_refresh([refresh] { refresh->trigger(); });
		}
	}

	/// [[BPR-1775]]. The conversation's cards, where my comments are edited.
	auto github_application::implementation::comment_edit_cards() const -> qt::conversation_cards_view*
	{
		return Window != nullptr && Window->conversation_panel() != nullptr ? Window->conversation_panel()->cards() : nullptr;
	}

	/// [[BPR-1775]]. Whether the session reads anonymously, as a reaction
	/// asks it.
	auto github_application::implementation::reads_anonymously() const -> bool
	{
		return Host != nullptr
			&& std::holds_alternative<core::anonymous_intent_refused>(Host->public_preview_legality(core::anonymous_read_intent::react_to_comment));
	}

	/// [[BPR-1775]]. Edit on my own published comments is offered in a
	/// signed-in read whose connection edits comments; the core decides
	/// each post from the read's "Mine".
	void github_application::implementation::present_comment_edit_offer()
	{
		auto* const cards = comment_edit_cards();
		if (Closing || cards == nullptr) {
			return;
		}
		const auto signedIn = WindowRead && WindowRead->accepted_snapshot() && !WindowRead->accepted_snapshot()->Viewer.Login.empty();
		cards->set_comment_edit_offered(signedIn && !reads_anonymously() && ReviewMutationHost != nullptr && ReviewMutationHost->capabilities().EditsComments);
	}

	/// [[BPR-1775]]. The post as the accepted read has it: whether the
	/// viewer wrote it, and its body; nothing when the read no longer
	/// shows it.
	auto github_application::implementation::accepted_comment(const std::string& aPost) const -> std::optional<std::pair<bool, std::string>>
	{
		if (!WindowRead || !WindowRead->accepted_snapshot()) {
			return std::nullopt;
		}
		const auto& snapshot = *WindowRead->accepted_snapshot();
		const auto  mine     = [&snapshot](const auto& aRecord) {
			return aRecord.Author.has_value() && !snapshot.Viewer.Login.empty() && core::same_login(aRecord.Author->Login, snapshot.Viewer.Login);
		};
		for (const auto& thread : snapshot.ReviewThreads) {
			for (const auto& comment : thread.Comments) {
				if (!comment.Deleted && comment_projection_id(comment) == aPost) {
					return std::pair{ mine(comment), comment.Body };
				}
			}
		}
		for (const auto& comment : snapshot.IssueComments) {
			if (!comment.Deleted && comment_projection_id(comment) == aPost) {
				return std::pair{ mine(comment), comment.Body };
			}
		}
		for (const auto& review : snapshot.Reviews) {
			if (comment_projection_id(review) == aPost) {
				return std::pair{ mine(review), review.Body };
			}
		}
		return std::nullopt;
	}

	/// [[BPR-1775]]. Save while editing one of my comments: the core
	/// decides on the gesture (mine, a change, not blank, within GitHub's
	/// length), the host writes once under a one-shot grant after GitHub,
	/// asked first, still has the original. Nothing is sent before Save,
	/// and a refusal is said beside the editor.
	void github_application::implementation::save_comment_edit(const qt::comment_edit_save& aSave)
	{
		auto* const cards = comment_edit_cards();
		if (Closing || cards == nullptr) {
			return;
		}
		++CommentEditGestures;
		const auto subject  = core::comment_edit_subject_of(aSave.Post);
		const auto accepted = accepted_comment(aSave.Post);
		if (!subject.has_value() || !accepted.has_value()) {
			++CommentEditRefusals;
			cards->present_comment_edit_status(QStringLiteral("This comment is no longer in the pull request as read. Refresh and try again."));
			return;
		}
		const auto refusal = core::decide_comment_edit({ .Mine = accepted->first, .Anonymous = reads_anonymously(), .Original = aSave.Original, .Edited = aSave.Body });
		switch (refusal) {
		case core::comment_edit_refusal::not_mine:
			++CommentEditRefusals;
			cards->present_comment_edit_status(QStringLiteral("Only your own comments can be edited here."));
			return;
		case core::comment_edit_refusal::unchanged:
			++CommentEditRefusals;
			cards->present_comment_edit_status(QStringLiteral("Nothing has changed, so there is nothing to save."));
			return;
		case core::comment_edit_refusal::empty:
			++CommentEditRefusals;
			cards->present_comment_edit_status(QStringLiteral("A comment needs some text. Deleting a comment is not offered here."));
			return;
		case core::comment_edit_refusal::too_long:
			++CommentEditRefusals;
			cards->present_comment_edit_status(QStringLiteral("GitHub keeps at most 65,536 characters; this comment has %1.")
												   .arg(QLocale{ }.toString(static_cast<qulonglong>(core::description_characters(aSave.Body)))));
			return;
		case core::comment_edit_refusal::none:
			break;
		}
		if (ReviewMutationHost == nullptr || !ReviewMutationHost->capabilities().EditsComments) {
			++CommentEditRefusals;
			cards->present_comment_edit_status(QStringLiteral("This connection cannot edit comments."));
			return;
		}
		if (PendingCommentEdit.has_value()) {
			cards->present_comment_edit_status(QStringLiteral("Waiting for GitHub to answer the last save."), true);
			return;
		}
		auto request = application::review_comment_edit_publication_request{
			.Kind     = subject->Kind,
			.Subject  = std::string{ subject->Node },
			.Original = aSave.Original,
			.Body     = aSave.Body,
		};
		auto grant = review_mutation_gesture_grant(
			std::nullopt,
			[&request](const core::application_session_id           aSession,
					   const core::request_generation               aGeneration,
					   application::review_mutation_target_binding  aTarget,
					   application::review_mutation_acting_identity aActor) {
				return application::detail::review_mutation_user_authority_issuer::issue_comment_edit(
					aSession, aGeneration, std::move(aTarget), std::move(aActor), request);
			});
		cards->present_comment_edit_status(QStringLiteral("Saving your comment to GitHub…"), true);
		PendingCommentEdit = ReviewMutationHost->edit_comment(std::move(request), std::move(grant));
	}

	void github_application::implementation::publish_comment_edit(const application::review_comment_edit_terminal& aTerminal)
	{
		if (Closing || Window == nullptr || PendingCommentEdit != std::optional{ aTerminal.Context }) {
			return;
		}
		PendingCommentEdit.reset();
		auto* const cards = comment_edit_cards();
		if (cards == nullptr) {
			return;
		}
		if (!aTerminal.Result.has_value()) {
			++CommentEditFailures;
			auto words = QString{ };
			switch (aTerminal.Result.error()) {
			case application::review_mutation_failure::refused_by_server:
				words = QStringLiteral("GitHub does not let you edit this comment. Nothing was saved.");
				break;
			case application::review_mutation_failure::not_confirmed:
				words = QStringLiteral("GitHub does not show the new text; it was not sent twice. Check the comment, then save again.");
				break;
			case application::review_mutation_failure::incompatible_body:
				words = QStringLiteral("GitHub cannot take this text. Nothing was saved.");
				break;
			case application::review_mutation_failure::target_moved:
			case application::review_mutation_failure::undesignated_target:
			case application::review_mutation_failure::target_unavailable:
				words = QStringLiteral("The comment or the pull request changed on GitHub, so nothing was saved. Refresh and try again.");
				break;
			case application::review_mutation_failure::cancelled:
				words = QStringLiteral("Saving was stopped. Nothing more is sent.");
				break;
			default:
				words = QStringLiteral("The connection could not save the comment (%1). Your text is kept.")
							.arg(QString::fromUtf8(application::name(aTerminal.Result.error())));
				break;
			}
			cards->present_comment_edit_status(words);
			return;
		}
		if (aTerminal.Result->Conflict) {
			// Never overwritten silently: the editor says so, keeps the typed
			// text, and offers GitHub's newer text to go on from.
			++CommentEditConflicts;
			cards->present_comment_edit_status(QStringLiteral("Someone changed this comment on GitHub since you began. Nothing was saved."));
			cards->offer_newer_comment(aTerminal.Result->Body);
			return;
		}
		++CommentEditCompletions;
		cards->close_comment_edit();
		// The comment shows GitHub's answer the way every other fact comes
		// back: the next read.
		if (auto* const refresh = Window->github_refresh_action(); refresh != nullptr && refresh->isEnabled()) {
			Window->run_automatic_refresh([refresh] { refresh->trigger(); });
		}
	}

	/// [[BPR-1804]]. Request review is offered in People when the
	/// connection in use requests reviewers and the session is signed in.
	void github_application::implementation::present_reviewer_offer()
	{
		if (Closing || Window == nullptr || Window->overview_page() == nullptr) {
			return;
		}
		const auto anonymous = Host != nullptr
			&& std::holds_alternative<core::anonymous_intent_refused>(Host->public_preview_legality(core::anonymous_read_intent::react_to_comment));
		const auto offered = !anonymous && WindowRead && WindowRead->accepted_snapshot() && ReviewMutationHost != nullptr
			&& ReviewMutationHost->capabilities().RequestsReviewers;
		Window->overview_page()->set_reviewer_requests_offered(offered);
		if (!offered && ReviewerPicker.Open) {
			ReviewerPicker = core::close_reviewer_picker(std::move(ReviewerPicker));
		}
	}

	void github_application::implementation::present_reviewer_picker()
	{
		if (!Closing && Window != nullptr && Window->overview_page() != nullptr) {
			Window->overview_page()->present_reviewer_picker(ReviewerPicker);
		}
	}

	/// [[BPR-1804]]. The core's transition, then its one effect: a search
	/// typed waits 250 ms for the next keystroke (only the last is asked),
	/// opening asks at once, and a choice asks GitHub once under a one-shot
	/// grant for that person.
	void github_application::implementation::reviewer_gesture(const qt::reviewer_picker_gesture& aGesture)
	{
		if (Closing || Window == nullptr) {
			return;
		}
		auto transition = core::reviewer_picker_transition{ .State = ReviewerPicker, .Effect = std::monostate{ } };
		switch (aGesture.Kind) {
		case qt::reviewer_picker_gesture::kind::open:
			if (!Window->overview_page()->reviewer_requests_offered()) {
				return;
			}
			transition = core::open_reviewer_picker(ReviewerPicker);
			break;
		case qt::reviewer_picker_gesture::kind::query:
			transition = core::type_reviewer_query(ReviewerPicker, aGesture.Text);
			break;
		case qt::reviewer_picker_gesture::kind::choose:
			++ReviewerChoices;
			transition = core::choose_reviewer(ReviewerPicker, aGesture.Text);
			break;
		case qt::reviewer_picker_gesture::kind::close:
			ReviewerPicker = core::close_reviewer_picker(std::move(ReviewerPicker));
			ReviewerSearchDebounce.reset();
			return;
		}
		ReviewerPicker = std::move(transition.State);
		if (const auto* const search = std::get_if<core::reviewer_search_effect>(&transition.Effect); search != nullptr) {
			if (aGesture.Kind == qt::reviewer_picker_gesture::kind::open) {
				search_reviewers(*search);
			} else {
				debounce_reviewer_search(*search);
			}
		} else if (const auto* const request = std::get_if<core::reviewer_request_effect>(&transition.Effect); request != nullptr) {
			request_reviewer(*request);
		}
		present_reviewer_picker();
	}

	void github_application::implementation::debounce_reviewer_search(core::reviewer_search_effect aSearch)
	{
		if (!ReviewerSearchDebounce) {
			ReviewerSearchDebounce = std::make_unique<QTimer>();
			ReviewerSearchDebounce->setSingleShot(true);
			ReviewerSearchDebounce->setInterval(reviewer_search_delay);
			QObject::connect(ReviewerSearchDebounce.get(), &QTimer::timeout, Window.get(), [this] {
				if (DebouncedReviewerSearch) {
					auto search = std::move(*DebouncedReviewerSearch);
					DebouncedReviewerSearch.reset();
					search_reviewers(search);
				}
			});
		}
		DebouncedReviewerSearch = std::move(aSearch);
		ReviewerSearchDebounce->start();
	}

	auto github_application::implementation::reviewer_failure(const application::review_mutation_failure aFailure) -> core::reviewer_request_failure
	{
		switch (aFailure) {
		case application::review_mutation_failure::refused_by_server:
		case application::review_mutation_failure::unsupported:
			return core::reviewer_request_failure::refused;
		case application::review_mutation_failure::target_moved:
		case application::review_mutation_failure::undesignated_target:
		case application::review_mutation_failure::target_unavailable:
		case application::review_mutation_failure::stale_head:
			return core::reviewer_request_failure::target_changed;
		case application::review_mutation_failure::cancelled:
			return core::reviewer_request_failure::cancelled;
		default:
			return core::reviewer_request_failure::connection;
		}
	}

	void github_application::implementation::search_reviewers(const core::reviewer_search_effect& aSearch)
	{
		if (Closing || !ReviewerPicker.Open || aSearch.Search != ReviewerPicker.Search) {
			return;
		}
		if (ReviewMutationHost == nullptr || !ReviewMutationHost->capabilities().RequestsReviewers) {
			ReviewerPicker = core::fail_reviewer_search(std::move(ReviewerPicker), aSearch.Search, core::reviewer_request_failure::refused);
			present_reviewer_picker();
			return;
		}
		++ReviewerSearches;
		const auto context                               = ReviewMutationHost->reviewers({ .Operation = application::review_reviewer_operation::search, .Query = aSearch.Query });
		PendingReviewerSearches[context.Request.value()] = aSearch.Search;
	}

	void github_application::implementation::request_reviewer(const core::reviewer_request_effect& aRequest)
	{
		if (ReviewMutationHost == nullptr || !ReviewMutationHost->capabilities().RequestsReviewers) {
			ReviewerPicker = core::fail_reviewer_request(std::move(ReviewerPicker), aRequest.Request, core::reviewer_request_failure::refused);
			return;
		}
		auto request = application::review_reviewer_publication_request{
			.Operation = application::review_reviewer_operation::request,
			.User      = aRequest.NodeId,
			.Login     = aRequest.Login,
		};
		auto grant = review_mutation_gesture_grant(
			std::nullopt,
			[&request](const core::application_session_id           aSession,
					   const core::request_generation               aGeneration,
					   application::review_mutation_target_binding  aTarget,
					   application::review_mutation_acting_identity aActor) {
				return application::detail::review_mutation_user_authority_issuer::issue_reviewer(
					aSession, aGeneration, std::move(aTarget), std::move(aActor), request);
			});
		// [[BPR-1849]]. People shows the person at once, pending, as the
		// core projects the request; GitHub's answer settles it.
		if (WindowRead && WindowRead->accepted_snapshot()) {
			const auto key = reviewer_overlay_key(*WindowRead->accepted_snapshot());
			if (ReviewerOverlayPullRequest != key) {
				ReviewerOverlay            = { };
				ReviewerOverlayPullRequest = key;
			}
			core::begin_reviewer_request_change(ReviewerOverlay, aRequest);
			present_overview_review_requests();
		}
		++ReviewerRequestsAsked;
		const auto context                               = ReviewMutationHost->reviewers(std::move(request), std::move(grant));
		PendingReviewerRequests[context.Request.value()] = aRequest.Request;
	}

	/// \brief [[BPR-1849]]. Which pull request a snapshot reads, for the
	/// review requests asked on it.
	auto github_application::implementation::reviewer_overlay_key(const application::github_pull_request_snapshot& aSnapshot) -> std::string
	{
		return aSnapshot.Repository.Owner + "/" + aSnapshot.Repository.Name + "#" + std::to_string(aSnapshot.PullRequest.Number.value());
	}

	/// \brief [[BPR-1849]]. A new accepted read reconciles the requests
	/// the viewer asked for with the ones GitHub lists; another pull
	/// request drops them.
	void github_application::implementation::observe_reviewer_request_state()
	{
		if (!WindowRead || !WindowRead->accepted_snapshot() || !ReviewerOverlayPullRequest) {
			return;
		}
		const auto& snapshot = *WindowRead->accepted_snapshot();
		if (reviewer_overlay_key(snapshot) != *ReviewerOverlayPullRequest) {
			ReviewerOverlay = { };
			ReviewerOverlayPullRequest.reset();
			return;
		}
		core::observe_reviewer_requests(ReviewerOverlay, application::requested_reviewer_logins(snapshot));
	}

	/// \brief [[BPR-1849]]. The requests People adds to the read's.
	auto github_application::implementation::projected_overview_review_requests(const application::github_pull_request_snapshot& aSnapshot) const
		-> std::vector<core::projected_reviewer_request>
	{
		if (ReviewerOverlay.Changes.empty() || ReviewerOverlayPullRequest != std::optional{ reviewer_overlay_key(aSnapshot) }) {
			return { };
		}
		return core::project_reviewer_requests(application::requested_reviewer_logins(aSnapshot), ReviewerOverlay);
	}

	/// \brief [[BPR-1849]]. A request asked, answered or withdrawn moves
	/// People and the status in place; nothing is read and no other part
	/// of the Overview is visited. A read the Overview has not been
	/// projected from yet is left to that projection.
	void github_application::implementation::present_overview_review_requests()
	{
		if (Closing || Window == nullptr || !PublishedOverview || !WindowRead || !WindowRead->accepted_snapshot()
			|| OverviewResolutionsSnapshot != std::optional{ WindowRead->accepted_snapshot()->Snapshot }) {
			return;
		}
		const auto& snapshot = *WindowRead->accepted_snapshot();
		auto        now      = projected_overview_review_requests(snapshot);
		if (now == OverviewReviewRequests) {
			return;
		}
		const auto changed     = application::apply_review_requests(*PublishedOverview, snapshot, now);
		OverviewReviewRequests = std::move(now);
		if (changed) {
			++OverviewReviewRequestUpdates;
			Window->publish_review_requests(*PublishedOverview);
		}
	}

	/// \brief [[BPR-1852]]. The draft state the window shows over a read:
	/// the viewer's own change while the overlay holds one for this pull
	/// request, the read's otherwise.
	auto github_application::implementation::projected_draft_state(const application::github_pull_request_snapshot& aSnapshot) const -> std::optional<core::projected_draft_state>
	{
		if (!DraftOverlay.held() || DraftOverlayPullRequest != std::optional{ reviewer_overlay_key(aSnapshot) }) {
			return std::nullopt;
		}
		return core::project_draft_state(aSnapshot.PullRequest.Draft, DraftOverlay);
	}

	auto github_application::implementation::draft_state_reading_now() const -> std::optional<draft_state_reading>
	{
		if (!WindowRead || !WindowRead->accepted_snapshot()) {
			return std::nullopt;
		}
		const auto& snapshot  = *WindowRead->accepted_snapshot();
		const auto  shown     = projected_draft_state(snapshot).value_or(core::projected_draft_state{ .Draft = snapshot.PullRequest.Draft });
		auto [author, viewer] = description_edit_logins();
		return draft_state_reading{ .Author = std::move(author), .Viewer = std::move(viewer), .Open = snapshot.PullRequest.State == application::github_pull_request_state::open, .Draft = shown.Draft };
	}

	/// \brief [[BPR-1852]]. Ready for review or Convert to draft is
	/// offered to the pull request's author, the core decides, when the
	/// connection in use can change the draft state; everyone else sees the
	/// chip and the banner alone.
	void github_application::implementation::present_draft_state_offer()
	{
		if (Closing || Window == nullptr) {
			return;
		}
		const auto reading = draft_state_reading_now();
		auto       offered = std::optional<core::draft_state_change>{ };
		if (reading.has_value() && ReviewMutationHost != nullptr && ReviewMutationHost->capabilities().ChangesDraftState) {
			offered = core::draft_state_offered(reading->facts());
		}
		Window->set_draft_state_offer(offered);
	}

	/// \brief [[BPR-1852]]. A new accepted read reconciles the viewer's
	/// change with GitHub's draft state; another pull request drops it.
	void github_application::implementation::observe_draft_state_change()
	{
		if (!WindowRead || !WindowRead->accepted_snapshot() || !DraftOverlayPullRequest) {
			return;
		}
		const auto& snapshot = *WindowRead->accepted_snapshot();
		if (reviewer_overlay_key(snapshot) != *DraftOverlayPullRequest) {
			DraftOverlay = { };
			DraftOverlayPullRequest.reset();
			return;
		}
		core::observe_draft_state(DraftOverlay, snapshot.PullRequest.Draft);
	}

	/// \brief [[BPR-1852]]. A change asked, answered or withdrawn moves the
	/// title row's chip, Overview's chip and banner, and Merge in place;
	/// nothing is read and no other part of the window is visited. A read
	/// the Overview has not been projected from yet is left to that
	/// projection.
	void github_application::implementation::present_overview_draft_state()
	{
		if (Closing || Window == nullptr || !WindowRead || !WindowRead->accepted_snapshot()) {
			return;
		}
		const auto& snapshot = *WindowRead->accepted_snapshot();
		const auto  now      = projected_draft_state(snapshot);
		Window->present_strip_draft(now.value_or(core::projected_draft_state{ .Draft = snapshot.PullRequest.Draft }).Draft
									&& snapshot.PullRequest.State == application::github_pull_request_state::open);
		present_draft_state_offer();
		if (!PublishedOverview || OverviewResolutionsSnapshot != std::optional{ snapshot.Snapshot }) {
			return;
		}
		if (application::apply_draft_state(*PublishedOverview, snapshot, now)) {
			++OverviewDraftStateUpdates;
			Window->publish_draft_state(*PublishedOverview);
		}
	}

	/// \brief [[BPR-1852]]. Ready for review, or Convert to draft, from
	/// Overview or the Pull Request menu: the core decides on the gesture
	/// (the author, an open pull request, not already in that state), the
	/// window shows the change at once, and the host writes it once under a
	/// one-shot grant after GitHub, asked first, says it still applies.
	void github_application::implementation::change_draft_state(const core::draft_state_change aChange)
	{
		auto* const page = Window != nullptr ? Window->overview_page() : nullptr;
		if (Closing || page == nullptr) {
			return;
		}
		++DraftStateGestures;
		const auto reading = draft_state_reading_now();
		if (!reading.has_value()) {
			++DraftStateRefusals;
			page->present_draft_state_status(aChange, QStringLiteral("The pull request is still loading. Try again when it has."));
			return;
		}
		switch (core::decide_draft_state_change(reading->facts(), aChange)) {
		case core::draft_state_refusal::not_author:
			++DraftStateRefusals;
			page->present_draft_state_status(aChange, QStringLiteral("Only the pull request's author can change whether it is a draft here."));
			return;
		case core::draft_state_refusal::not_open:
			++DraftStateRefusals;
			page->present_draft_state_status(aChange, QStringLiteral("This pull request is closed, so its draft state cannot change."));
			return;
		case core::draft_state_refusal::already:
			++DraftStateRefusals;
			page->present_draft_state_status(aChange, aChange == core::draft_state_change::ready_for_review ? QStringLiteral("This pull request is already ready for review.") : QStringLiteral("This pull request is already a draft."));
			return;
		case core::draft_state_refusal::not_permitted:
		case core::draft_state_refusal::none:
			break;
		}
		if (ReviewMutationHost == nullptr || !ReviewMutationHost->capabilities().ChangesDraftState) {
			++DraftStateRefusals;
			page->present_draft_state_status(aChange, QStringLiteral("This connection cannot change whether the pull request is a draft."));
			return;
		}
		if (PendingDraftState.has_value()) {
			page->present_draft_state_status(aChange, QStringLiteral("Waiting for GitHub to answer the last change."), true);
			return;
		}
		auto request = application::review_draft_state_publication_request{ .Change = aChange };
		auto grant   = review_mutation_gesture_grant(
			std::nullopt,
			[&request](const core::application_session_id           aSession,
					   const core::request_generation               aGeneration,
					   application::review_mutation_target_binding  aTarget,
					   application::review_mutation_acting_identity aActor) {
				return application::detail::review_mutation_user_authority_issuer::issue_draft_state(
					aSession, aGeneration, std::move(aTarget), std::move(aActor), request);
			});
		// The window shows the change at once, pending, as the core
		// projects it; GitHub's answer settles it.
		const auto& snapshot    = *WindowRead->accepted_snapshot();
		DraftOverlayPullRequest = reviewer_overlay_key(snapshot);
		core::begin_draft_state_change(DraftOverlay, ++DraftStateRequests, aChange);
		PendingDraftState = pending_draft_state{ .Request = DraftStateRequests, .Change = aChange };
		page->present_draft_state_status(aChange, QString{ }, true);
		present_overview_draft_state();
		PendingDraftState->Context = ReviewMutationHost->change_draft_state(request, std::move(grant));
	}

	void github_application::implementation::publish_draft_state(const application::review_draft_state_terminal& aTerminal)
	{
		if (Closing || Window == nullptr || !PendingDraftState.has_value() || PendingDraftState->Context != aTerminal.Context) {
			return;
		}
		const auto request = PendingDraftState->Request;
		PendingDraftState.reset();
		auto* const page   = Window->overview_page();
		const auto  change = aTerminal.Request.Change;
		if (!aTerminal.Result.has_value()) {
			++DraftStateFailures;
			// The window shows the read again; the words say why.
			core::abandon_draft_state_change(DraftOverlay, request);
			auto words = QString{ };
			switch (aTerminal.Result.error()) {
			case application::review_mutation_failure::refused_by_server:
				words = QStringLiteral("GitHub did not let you change whether this pull request is a draft. Nothing changed.");
				break;
			case application::review_mutation_failure::not_confirmed:
				words = QStringLiteral("GitHub does not show the change; it was not sent twice. Check the pull request, then try again.");
				break;
			case application::review_mutation_failure::target_moved:
			case application::review_mutation_failure::undesignated_target:
			case application::review_mutation_failure::target_unavailable:
				words = QStringLiteral("The pull request changed on GitHub, so nothing was sent. Refresh and try again.");
				break;
			case application::review_mutation_failure::cancelled:
				words = QStringLiteral("The change was stopped. Nothing more is sent.");
				break;
			default:
				words = QStringLiteral("The connection could not change the draft state (%1). Try again.").arg(QString::fromUtf8(application::name(aTerminal.Result.error())));
				break;
			}
			if (page != nullptr) {
				page->present_draft_state_status(change, words);
			}
			present_overview_draft_state();
			return;
		}
		++DraftStateCompletions;
		// GitHub's answer confirms what the window already shows, or
		// withdraws it; nothing re-reads the pull request, and the next
		// read reconciles.
		core::settle_draft_state_change(DraftOverlay, request, aTerminal.Result->Draft);
		if (page != nullptr) {
			page->present_draft_state_status(change, aTerminal.Result->Draft == core::draft_after(change) ? QString{ } : QStringLiteral("GitHub still shows the pull request as it was. Nothing changed."));
		}
		present_overview_draft_state();
	}

	/// \brief [[BPR-1852]]. A retired host answers nothing more: a change
	/// still waiting is withdrawn and said to have gone unanswered (it is
	/// never sent again).
	void github_application::implementation::retire_draft_state_change()
	{
		if (!PendingDraftState.has_value()) {
			return;
		}
		const auto change = PendingDraftState->Change;
		core::abandon_draft_state_change(DraftOverlay, PendingDraftState->Request);
		PendingDraftState.reset();
		if (Window != nullptr && Window->overview_page() != nullptr) {
			Window->overview_page()->present_draft_state_status(change, QStringLiteral("The connection closed before GitHub answered. Check the pull request, then try again."));
		}
		present_overview_draft_state();
	}

	void github_application::implementation::publish_reviewers(const application::review_reviewer_terminal& aTerminal)
	{
		if (Closing || Window == nullptr) {
			return;
		}
		const auto key = aTerminal.Context.Request.value();
		if (aTerminal.Request.Operation == application::review_reviewer_operation::search) {
			const auto found = PendingReviewerSearches.find(key);
			if (found == PendingReviewerSearches.end()) {
				return;
			}
			const auto search = found->second;
			PendingReviewerSearches.erase(found);
			if (aTerminal.Result.has_value()) {
				ReviewerPicker = core::accept_reviewer_search(std::move(ReviewerPicker), search, aTerminal.Result->Answer);
			} else {
				ReviewerPicker = core::fail_reviewer_search(std::move(ReviewerPicker), search, reviewer_failure(aTerminal.Result.error()));
			}
			present_reviewer_picker();
			return;
		}
		const auto found = PendingReviewerRequests.find(key);
		if (found == PendingReviewerRequests.end()) {
			return;
		}
		const auto request = found->second;
		PendingReviewerRequests.erase(found);
		if (!aTerminal.Result.has_value()) {
			++ReviewerRequestFailures;
			ReviewerPicker = core::fail_reviewer_request(std::move(ReviewerPicker), request, reviewer_failure(aTerminal.Result.error()));
			// [[BPR-1849]]. People shows the read again; the picker says why.
			core::abandon_reviewer_request_change(ReviewerOverlay, request);
			present_reviewer_picker();
			present_overview_review_requests();
			return;
		}
		++ReviewerRequestCompletions;
		const auto answer = core::reviewer_request_answer{ .Requested = aTerminal.Result->Answer.Requested, .Written = aTerminal.Result->Written };
		ReviewerPicker    = core::finish_reviewer_request(std::move(ReviewerPicker), request, answer);
		// [[BPR-1849]]. GitHub's answer confirms the person People already
		// shows, or withdraws them; nothing re-reads the pull request, and
		// the next read reconciles.
		core::settle_reviewer_request_change(ReviewerOverlay, request, answer);
		present_reviewer_picker();
		present_overview_review_requests();
	}

	/// [[BPR-1804]]. A retired host answers nothing more: searches are
	/// forgotten, the picker closes, and a request still waiting is said to
	/// have gone unanswered (it is never sent again).
	void github_application::implementation::retire_reviewer_picker()
	{
		PendingReviewerSearches.clear();
		for (const auto& [context, request] : PendingReviewerRequests) {
			static_cast<void>(context);
			ReviewerPicker = core::fail_reviewer_request(std::move(ReviewerPicker), request, core::reviewer_request_failure::connection);
			core::abandon_reviewer_request_change(ReviewerOverlay, request);
		}
		const auto withdrawn = !PendingReviewerRequests.empty();
		PendingReviewerRequests.clear();
		if (withdrawn) {
			present_overview_review_requests();
		}
		DebouncedReviewerSearch.reset();
		if (ReviewerSearchDebounce) {
			ReviewerSearchDebounce->stop();
		}
		if (ReviewerPicker.Open) {
			ReviewerPicker = core::close_reviewer_picker(std::move(ReviewerPicker));
			present_reviewer_picker();
		}
	}

	// [[BPR-0657]]. A card's Resolve or Unresolve: the core decides from
	// the session's anonymity, the server's capability and the thread's
	// state, before any provider is asked; a refusal is named on the
	// panel. An allowed gesture goes to the mutation host under the same
	// grant a publication gets. [[BPR-1841]]: its terminal settles the
	// core's overlay and the window projects that one thread's change;
	// nothing re-reads the pull request, and the next read reconciles.
	void github_application::implementation::resolve_review_thread(const qt::thread_resolution_gesture& aGesture)
	{
		constexpr auto prefix = std::string_view{ "review-thread:" };
		if (Closing || Window == nullptr || !aGesture.Identity.starts_with(prefix)) {
			return;
		}
		auto* const panel  = Window->conversation_panel();
		const auto  notice = [panel](const QString& aText) {
			if (panel != nullptr) {
				panel->present_notice(aText);
			}
		};
		const auto anonymous = Host != nullptr

			&& std::holds_alternative<core::anonymous_intent_refused>(Host->public_preview_legality(
				aGesture.Resolve ? core::anonymous_read_intent::resolve_review_thread
								 : core::anonymous_read_intent::unresolve_review_thread));
		// [[BPR-1824]]. Whether a connection writes to this pull request;
		// a selected diff is not asked for, so Discuss resolves with no
		// file open.
		const auto connection = thread_resolution_connection_refusal();
		// [[BPR-1741]]. GitHub's own word on whether this viewer may
		// resolve (or reopen) the thread, from the accepted read.
		// [[BPR-1841]]. As projected: a thread I resolved since the read
		// may be reopened without reading it again.
		const auto* const thread    = accepted_thread(aGesture.Identity);
		auto              permitted = true;
		if (thread != nullptr) {
			const auto projected = core::project_thread_resolution(thread->Resolved, ConversationOverlay, aGesture.Identity);
			const auto may       = core::project_thread_resolution_permission(thread->Resolved, projected.Resolved, thread_permission(*thread));
			permitted            = aGesture.Resolve ? may.MayResolve : may.MayUnresolve;
		}
		const auto refusal = core::decide_thread_resolution(core::thread_resolution_input{
			.Capability      = aGesture.Capability,
			.AnonymousRead   = anonymous,
			.Resolved        = aGesture.Resolved,
			.Resolve         = aGesture.Resolve,
			.ViewerPermitted = permitted,
			.WriteConnection = connection != core::thread_resolution_refusal::no_write_connection && ReviewMutationHost != nullptr,
		});

		++ThreadResolutionGestures;
		if (refusal != core::thread_resolution_refusal::none) {
			++ThreadResolutionRefusals;
			notice(qt::thread_resolution_refusal_text(refusal, aGesture.Resolve));
			return;
		}
		if (core::resolution_in_flight(ConversationOverlay, aGesture.Identity)) {
			++ThreadResolutionRefusals;
			notice(QStringLiteral("Waiting for GitHub to answer the last change to this conversation."));
			return;
		}
		auto request = application::review_thread_resolution_publication_request{

			.Thread  = std::string{ aGesture.Identity.substr(prefix.size()) },
			.Resolve = aGesture.Resolve,
		};
		auto grant = review_mutation_gesture_grant(
			std::nullopt,
			[&request](const core::application_session_id           aSession,
					   const core::request_generation               aGeneration,
					   application::review_mutation_target_binding  aTarget,
					   application::review_mutation_acting_identity aActor) {
				return application::detail::review_mutation_user_authority_issuer::issue_thread(
					aSession, aGeneration, std::move(aTarget), std::move(aActor), request);
			});
		notice(aGesture.Resolve ? QStringLiteral("Resolving the conversation…")
								: QStringLiteral("Reopening the conversation…"));
		// [[BPR-1741]]. The thread shows what was asked at once.
		static_cast<void>(core::begin_resolution_change(ConversationOverlay, aGesture.Identity, aGesture.Resolve));
		present_conversation_state();
		const auto context = ReviewMutationHost->resolve(std::move(request), std::move(grant));
		if (!core::retain_conversation_mutation(ConversationMutations, { context.Request, context.Session, context.Generation },
												{ .Subject = aGesture.Identity, .Add = aGesture.Resolve })) {
			core::abandon_resolution_change(ConversationOverlay, aGesture.Identity);
			present_conversation_state();
		}
	}

	/// [[BPR-1824]]. What stopped a resolution GitHub was asked for, or
	/// that the host refused before asking, and what to do about it.
	auto github_application::implementation::thread_resolution_failure_text(const application::review_mutation_failure aFailure, const bool aResolve) -> QString
	{
		using failure   = application::review_mutation_failure;
		const auto verb = aResolve ? QStringLiteral("resolved") : QStringLiteral("reopened");
		switch (aFailure) {
		case failure::refused_by_server:
		case failure::target_unavailable:
			return QStringLiteral("GitHub refused, so the thread was not %1. Your account may lack write access to the repository, or the thread is gone.").arg(verb);
		case failure::target_moved:
		case failure::undesignated_target:
		case failure::stale_head:
			return QStringLiteral("The pull request changed since it was read, so the thread was not %1. Refresh, then try again.").arg(verb);
		case failure::unsupported:
			return QStringLiteral("This connection cannot change threads, so the thread was not %1. Use the pull request's page on GitHub.").arg(verb);
		case failure::credential_unavailable:
		case failure::credential_tool_failed:
			return QStringLiteral("No GitHub sign-in was available, so the thread was not %1. Sign in again, then try again.").arg(verb);
		case failure::cancelled:
			return QStringLiteral("Stopped before GitHub answered; the thread was not %1.").arg(verb);
		case failure::not_confirmed:
			return QStringLiteral("GitHub does not show the thread %1, and the change was not sent twice. Refresh to see its state.").arg(verb);
		case failure::malformed_response:
			return QStringLiteral("GitHub's answer could not be read, so it is not known whether the thread was %1. Refresh to see its state.").arg(verb);
		case failure::unavailable:
			return QStringLiteral("GitHub could not be reached, or another change to this pull request is still being sent, so the thread was not %1. Try again.").arg(verb);
		default:
			return QStringLiteral("The thread was not %1 (%2).").arg(verb, QString::fromUtf8(application::name(aFailure)));
		}
	}

	void github_application::implementation::publish_resolution(const application::review_thread_resolution_terminal& aTerminal)
	{
		if (Closing || Window == nullptr) {
			return;
		}
		if (!core::settle_conversation_mutation(ConversationMutations, ConversationOverlay,
												{ aTerminal.Context.Request, aTerminal.Context.Session, aTerminal.Context.Generation },
												{ .Subject = "review-thread:" + aTerminal.Request.Thread, .Add = aTerminal.Request.Resolve }, aTerminal.Result.has_value())) {
			return;
		}
		present_conversation_state();
		auto* const panel = Window->conversation_panel();
		if (!aTerminal.Result.has_value()) {
			++ThreadResolutionFailures;

			if (panel != nullptr) {
				panel->present_notice(thread_resolution_failure_text(aTerminal.Result.error(), aTerminal.Request.Resolve));
			}
			return;
		}
		++ThreadResolutionCompletions;
		if (panel != nullptr) {
			panel->present_notice(aTerminal.Result->Resolved ? QStringLiteral("Conversation resolved.")
															 : QStringLiteral("Conversation reopened."));
		}
		// [[BPR-1841]]. GitHub's answer is the thread's new state: the core
		// keeps the confirmed change until a later read agrees, and the
		// window has already shown it. Re-reading the whole pull request
		// here made every resolve wait on a full refresh.
	}

	void github_application::implementation::refresh_review_projection(
		const application::review_mutation_terminal& aTerminal)
	{
#if defined(BPR_BUILD_TESTING)
		static_cast<void>(aTerminal);
		if (Fixture == nullptr) {
			return;
		}
		const auto observed  = Fixture->review_comments();
		ReviewReconciliation = application::reconcile(
			ReviewProjection, observed, { });
		ReviewProjection = ReviewReconciliation.Projection;
#else
		if (!aTerminal.Reconciliation.has_value()) {
			return;
		}
		ReviewReconciliation = application::reconcile(
			ReviewProjection, *aTerminal.Reconciliation, { });
		ReviewProjection = ReviewReconciliation.Projection;
#endif
	}
	void github_application::request_quit(int aExitCode)
	{
		if (Profile->DesktopQuit) {
			Profile->DesktopQuit(aExitCode);
		} else {
			QCoreApplication::exit(aExitCode);
		}
	}

}
