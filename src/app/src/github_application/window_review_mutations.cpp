#include "github_application/window_composition.hpp"

#include "bpr/adapters/sqlite/review_mutation_recovery_store.hpp"
#include "bpr/application/github_host_authority.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/off_hunk_fact.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "github_provider.hpp"

#include <QModelIndex>

#include <algorithm>
#include <variant>

namespace bpr::app {
	auto github_application::implementation::review_mutation_target(
		const application::accepted_pull_request_handoff& aHandoff)
		-> core::review_mutation_target
	{
		return {
			.Host         = aHandoff.Host,
			.Repository   = aHandoff.Repository,
			.PullRequest  = aHandoff.PullRequest,
			.ExpectedHead = aHandoff.Head,
		};
	}

	auto github_application::implementation::exact_retryable_review_draft(
		const retryable_review_draft_record&                 aRecord,
		const core::semantic_selection&                      aSelection,
		const application::workbench_pull_request_authority& aAuthority)
		-> bool
	{
		return (aRecord.Draft.State == core::review_draft_state::drafting || aRecord.Draft.State == core::review_draft_state::publishing)
			&& aRecord.Draft.Anchor == aSelection.PreferredLine
			&& aRecord.Draft.Anchor.Comparison == aSelection.Comparison
			&& aRecord.Authority == aAuthority;
	}

	/// \brief Checks the exact current CLI publication and its retained strict wire.
	/// \details Admission supplies no write authority; the ordinary gesture must
	/// still issue its exact one-use grant and the provider reacquires credentials.
	auto github_application::implementation::admit_cli_publication(
		const application::review_mutation_target_binding&  aTarget,
		const application::review_mutation_acting_identity& aActor) const
		-> std::expected<void, application::review_mutation_failure>
	{
		using failure = application::review_mutation_failure;
		if (!Host || !WindowRead || !Host->accepted_session()
			|| !WindowRead->accepted_snapshot()) {
			return std::unexpected(failure::undesignated_target);
		}
		const auto& session = *Host->accepted_session();
		const auto* profile = find_profile(session.Profile);
		if (!profile) {
			return std::unexpected(failure::undesignated_target);
		}
		if (profile->authentication_transport() != application::github_authentication_transport::github_cli) {
			return { };
		}
		const auto& snapshot = *WindowRead->accepted_snapshot();
		const auto  head     = core::parse_git_commit_oid(snapshot.PullRequest.HeadOid.value());
		if (snapshot.Profile != session.Profile || snapshot.Host != session.Host
			|| session.Viewer.Account != snapshot.Account || session.Viewer.Account != aActor.Account
			|| session.Viewer.Login != snapshot.Viewer.Login || session.Viewer.Login != aActor.Login
			|| aTarget.Semantic.Host != session.Host || aTarget.Semantic.Repository != snapshot.Repository.Repository
			|| aTarget.Semantic.PullRequest != snapshot.PullRequest.PullRequest
			|| aTarget.Repository.Owner != snapshot.Repository.Owner || aTarget.Repository.Name != snapshot.Repository.Name
			|| aTarget.Number != snapshot.PullRequest.Number || aTarget.Hostname.value() != profile->hostname()
			|| !head || aTarget.Semantic.ExpectedHead != *head) {
			return std::unexpected(failure::undesignated_target);
		}
		const auto lease = std::dynamic_pointer_cast<adapters::github::detail::github_review_mutation_transport_lease>(WindowRead->mutation_transport());
		if (!lease || !lease->strict_cli()) {
			return std::unexpected(failure::unsupported);
		}
		return { };
	}

	auto github_application::implementation::conversation_source() const -> std::optional<application::review_permalink_source>
	{
		if (!WindowRead || !WindowRead->accepted_snapshot() || !LastAcceptedHandoff
			|| !SelectedDiffPublication || selected_diff_filtered() || !ReviewSourceMaterial || !ReviewSourceMaterial->RightLines
			|| ReviewSourceMaterial->Key != SelectedDiffPublication->Key || !EpochHost
			|| EpochHost->context(SelectedDiffPublication->Request.WorkbenchAuthority)
				!= std::optional{ SelectedDiffPublication->Request.Diff }) {
			return std::nullopt;
		}
		const auto& handoff  = *LastAcceptedHandoff;
		const auto& snapshot = *WindowRead->accepted_snapshot();
		return application::review_permalink_source{
			.Target = {
				.Semantic = review_mutation_target(handoff), .Hostname = handoff.Hostname, .ServerContract = handoff.ServerContract, .Repository = handoff.Locator, .Number = handoff.Number, .RepositoryDatabaseId = snapshot.Repository.Repository.value(), .RepositoryNodeId = std::string{ snapshot.Repository.Node.value() }, .PullRequestDatabaseId = snapshot.PullRequest.PullRequest.value(), .PullRequestNodeId = std::string{ snapshot.PullRequest.Node.value() }, .BaseRepositoryDatabaseId = snapshot.PullRequest.Repository.value(), .BaseOid = application::recorded_base(handoff), .BaseRef = snapshot.PullRequest.BaseRef, .HeadRef = snapshot.PullRequest.HeadRef, .Open = snapshot.PullRequest.State == application::github_pull_request_state::open, .ComparisonBaseOid = application::distinct_comparison_base(application::recorded_base(handoff), handoff.Base) },
			.Endpoint   = SelectedDiffPublication->RightEndpoint,
			.Comparison = SelectedDiffPublication->Key,
			.LineCount  = *ReviewSourceMaterial->RightLines
		};
	}

	void github_application::implementation::select_published_conversation()
	{
		if (!PendingPublishedConversation || !Window || !WindowRead || !LastAcceptedHandoff
			|| !WindowRead->accepted_snapshot() || !SelectedDiffPublication) {
			return;
		}
		const auto  observed = select_observed_review_mutation(*LastAcceptedHandoff, *WindowRead->accepted_snapshot());
		const auto& pending  = *PendingPublishedConversation;
		if (!observed || observed->Target != pending.Target || observed->ActingIdentity != pending.Actor
			|| core::comparison_digest(SelectedDiffPublication->Key) != pending.Anchor.Comparison) {
			PendingPublishedConversation.reset();
			return;
		}
		// A refreshed snapshot can arrive before its selected source is
		// republished. Wait for that exact material before selecting the post.
		if (!conversation_source()) {
			return;
		}
		const auto identity = "review-comment:" + pending.Comment;
		// [[BPR-1293]]. Review stays: the thread opens in place at the
		// published post.
		if (Window->conversation_model()->index_for_id(identity).isValid()
			&& Window->reveal_conversation_in_review(identity)) {
			PendingPublishedConversation.reset();
		}
	}

	void github_application::implementation::publish_review_conversations(const application::github_presentation_update& aUpdate)
	{
		auto placed = std::optional<application::github_presentation_update>{ };
		if (aUpdate.PullRequest && WindowRead && WindowRead->accepted_snapshot()) {
			// [[BPR-1338]]. The placement re-projects the conversations; the
			// window publication the read host stamped on its projection
			// stays, or body links and Open on GitHub would name none.
			PublishedMarkdownContext = aUpdate.PullRequest->MarkdownContext;
			PublishedHostname        = aUpdate.PullRequest->Hostname;
			if (auto source = conversation_source()) {
				placed                               = aUpdate;
				placed->PullRequest                  = ReviewCommentPlacement.project(*WindowRead->accepted_snapshot(), std::move(source));
				placed->PullRequest->MarkdownContext = PublishedMarkdownContext;
				placed->PullRequest->Hostname        = PublishedHostname;
				place_filtered_threads(*placed->PullRequest);
			} else if (selected_diff_filtered()) {
				ReviewCommentPlacement.clear();
				placed = aUpdate;
				place_filtered_threads(*placed->PullRequest);
			} else {
				// [[BPR-1387]]. With no selected source, placement returns the
				// host's original projection. Reuse it without copying and
				// rebuilding every conversation on the UI thread at PR open.
				ReviewCommentPlacement.clear();
			}
		}
		if (aUpdate.PullRequest) {
			observe_conversation_state();
			observe_reviewer_request_state();
			observe_draft_state_change();
		}
		Window->publish(placed ? *placed : aUpdate);
		if (aUpdate.PullRequest) {
			present_conversation_state();
			present_reviewer_offer();
			present_description_offer();
			present_comment_edit_offer();
			// [[BPR-1852]]. The title row's chip shows the viewer's change
			// over the read until a read agrees; the author's action follows.
			present_overview_draft_state();
		}
		select_published_conversation();

		if (aUpdate.PullRequest) {
			observe_review_preflight();
			// [[BPR-1374]]. A thread gone or resolved on the server changes
			// where my unsent reply stands.
			present_saved_drafts();
		}
	}

	void github_application::implementation::refresh_review_conversations()
	{
		if (!Closing && Window && WindowRead && WindowRead->accepted_snapshot()) {
			auto projection            = ReviewCommentPlacement.project(*WindowRead->accepted_snapshot(), conversation_source());
			projection.MarkdownContext = PublishedMarkdownContext;
			projection.Hostname        = PublishedHostname;
			place_filtered_threads(projection);
			Window->publish(application::github_presentation_update{
				.PullRequest              = std::move(projection),
				.PreserveWindowNavigation = true });
			select_published_conversation();
		} else {
			ReviewCommentPlacement.clear();
		}
	}

	auto github_application::implementation::observe_review_source(const core::workbench_file_id aFile) const
		-> std::optional<application::review_mutation_source_observation>
	{
		if (Closing || !WorkbenchHost || !WindowRead || !LastAcceptedHandoff
			|| !SelectedDiffPublication || !SelectedDiffRequest || !ReviewSourceMaterial
			|| !WindowRead->accepted_snapshot() || !EpochHost
			|| EpochHost->context(SelectedDiffPublication->Request.WorkbenchAuthority)
				!= std::optional{ SelectedDiffPublication->Request.Diff }) {
			return std::nullopt;
		}
		const auto file    = WorkbenchHost->file_observation(aFile);
		const auto binding = select_observed_review_mutation(*LastAcceptedHandoff, *WindowRead->accepted_snapshot());
		if (!file || !binding) {
			return std::nullopt;
		}
		return application::review_mutation_source_observation{
			.Authority      = workbench_authority(*LastAcceptedHandoff),
			.File           = *file,
			.Target         = binding->Target,
			.ActingIdentity = binding->ActingIdentity,
			.Material       = *ReviewSourceMaterial,
			// Full-file alignment is not a provider patch-domain observation.
			.Placement = core::review_placement::unknown,
			.Filtered  = SelectedDiffPublication->Key.Filter ? filtered_comment_source() : std::nullopt,
			// [[BPR-1735]]. GitHub's own diff of the file, read from the
			// merge base and the head, which the core places a comment by.
			.WholeDiff = SelectedDiffPublication->Key.Filter ? std::nullopt : whole_diff_source([&file] {
				const auto bytes = file->CurrentPath.generic_u8string();
				return std::string{ bytes.begin(), bytes.end() };
			}()),
			// [[BPR-1736]]. What this profile learned about a single comment
			// outside GitHub's diff here; publication checks the exact key.
			.OffHunkFact = presented_off_hunk_fact(false),
		};
	}

	/// \brief [[BPR-1736]]. The scope the learned off-hunk facts of the
	/// accepted pull request are kept under, and the profile's facts; none
	/// before a pull request is accepted or when the journal can't open.
	auto github_application::implementation::off_hunk_facts() const -> std::optional<std::pair<application::off_hunk_fact_scope, application::off_hunk_fact_port*>>
	{
		if (!LastAcceptedHandoff || !WindowRead || !WindowRead->accepted_snapshot()) {
			return std::nullopt;
		}
		const auto binding = select_observed_review_mutation(*LastAcceptedHandoff, *WindowRead->accepted_snapshot());
		if (!binding) {
			return std::nullopt;
		}
		const auto hostname = std::string{ binding->Target.Hostname.value() };
		if (!OffHunkFactsOpened || OffHunkFactsRoot != ApplicationDataRoot) {
			OffHunkFactsOpened = true;
			OffHunkFactsRoot   = ApplicationDataRoot;
			auto opened        = adapters::sqlite::make_off_hunk_fact_store({ .StorageRoot = ApplicationDataRoot,
																			  .Host        = hostname,
																			  .Account     = std::to_string(binding->ActingIdentity.Account.value()),
																			  .Repository  = binding->Target.Repository.Owner + "/" + binding->Target.Repository.Name,
																			  .PullRequest = static_cast<std::int64_t>(binding->Target.Number.value()) });
			OffHunkFacts       = opened ? std::move(*opened) : nullptr;
		}
		return std::pair{ application::off_hunk_fact_scope{ .Profile              = LastAcceptedHandoff->Profile.value(),
															.Host                 = hostname,
															.Account              = binding->ActingIdentity.Account.value(),
															.RepositoryDatabaseId = binding->Target.RepositoryDatabaseId },
						  OffHunkFacts.get() };
	}

	/// \brief [[BPR-1736]]. The learned fact as presented now: for a single
	/// comment, or for a review (decision D3: the newer of both).
	auto github_application::implementation::presented_off_hunk_fact(const bool aReview) const -> core::off_hunk_creation_capability
	{
		const auto facts = off_hunk_facts();
		if (!facts) {
			return core::off_hunk_creation_capability::unknown;
		}
		const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		return aReview ? application::presented_review_off_hunk_fact(facts->second, facts->first, now) : application::presented_off_hunk_fact(facts->second, facts->first, now);
	}

	/// \brief [[BPR-1736]]. GitHub refused a review for a line it can't
	/// place. When a learned fact let it carry a comment outside the diff,
	/// GitHub withdrew support: the fact drops to unsupported for both
	/// operations and the page holds those comments. Nothing is resent.
	void github_application::implementation::demote_off_hunk_fact(const application::review_submission_request& aRefused)
	{
		const auto facts = off_hunk_facts();
		if (Closing || !facts || presented_off_hunk_fact(true) != core::off_hunk_creation_capability::supported) {
			return;
		}
		const auto carried = std::ranges::find_if(aRefused.Proposal.Comments, [this](const core::review_submission_comment& aComment) {
			const auto source = whole_diff_source(aComment.Path);
			return source && core::holds_review_comment(core::place_on_github_diff(source->BaseToHead, aComment.Side, aComment.Line, aComment.StartLine), core::off_hunk_creation_capability::unknown);
		});
		if (carried == aRefused.Proposal.Comments.end() || !aRefused.LiveBinding) {
			return;
		}
		const auto& target = aRefused.LiveBinding->Target;
		const auto  now    = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		static_cast<void>(application::demote_off_hunk_fact(facts->second, facts->first,
															{ .Repository = target.Repository.Owner + "/" + target.Repository.Name, .PullRequest = target.Number.value(), .Path = carried->Path, .Line = carried->Line },
															now));
		present_review_comment_holds();
		present_filtered_comment();
	}

	void github_application::implementation::retire_review_mutation_host(const core::conversation_mutation_retirement aConversation)
	{
		complete_shared_draft(false, { }, PendingReviewMutation.has_value());
		if (aConversation == core::conversation_mutation_retirement::cancel) {
			for (auto& host : RetiredReviewMutationHosts) {
				host->shutdown();
			}
		}
		if (ReviewMutationHost != nullptr) {
			if (aConversation == core::conversation_mutation_retirement::drain) {
				ReviewMutationHost->retire_conversation_changes();
			} else {
				ReviewMutationHost->shutdown();
			}
			RetiredReviewMutationHosts.push_back(
				std::move(ReviewMutationHost));
		}
		std::erase_if(
			RetiredReviewMutationHosts,
			[](const auto& aHost) { return aHost->is_quiescent(); });
		PendingReviewMutation.reset();
		DeferredReviewSelection.reset();
		RetryableReviewDraft.reset();
		// [[BPR-1774]]. A retired host answers no save of the next one.
		PendingDescriptionEdit.reset();
		PendingCommentEdit.reset();
		// [[BPR-1804]]. Nor any reviewer search or request: the picker
		// closes, and a request waiting is said to have gone unanswered.
		retire_reviewer_picker();
		// [[BPR-1852]]. Nor a draft-state change.
		retire_draft_state_change();
	}

	/// [[BPR-1824]]. Why this session's accepted read cannot have its
	/// threads resolved before any write connection is asked: a public
	/// preview, or no connection yet. [[BPR-1826]]. A closed or merged
	/// pull request is composed a connection that resolves threads.
	auto github_application::implementation::thread_resolution_connection_refusal() const -> core::thread_resolution_refusal
	{
		if (Host != nullptr
			&& std::holds_alternative<core::anonymous_intent_refused>(
				Host->public_preview_legality(core::anonymous_read_intent::resolve_review_thread))) {
			return core::thread_resolution_refusal::anonymous_read;
		}
		if (ReviewMutationHost == nullptr || !InlineReviewBinding) {
			return core::thread_resolution_refusal::no_write_connection;
		}
		return core::thread_resolution_refusal::none;
	}

	// The grant a gesture publishes or resolves under is minted in one
	// place: absent in attended mode, where the host's own designated
	// authority answers; the fixture's testing grant in a fixture build
	// (a one-shot for the exact publication when one is named, reusable
	// for a resolution); the user issuer's one-shot in production, through
	// the issuer the gesture names. Both gestures bind to the accepted
	// pull request the selected diff was published for.
	auto github_application::implementation::review_mutation_gesture_grant(
		const std::optional<application::review_mutation_publication_request>& aTestingRequest,
		const std::function<application::review_mutation_execution_grant(
			core::application_session_id,
			core::request_generation,
			application::review_mutation_target_binding,
			application::review_mutation_acting_identity)>&                    aIssue)
		-> application::review_mutation_execution_grant
	{
		auto grant = application::review_mutation_execution_grant{ };
		// [[BPR-1745]]. A reply bound to its thread is published
		// without a selected diff; the accepted handoff names the session
		// and pull request the grant binds to.
		// [[BPR-1824]]. So is every gesture that is no inline comment: a
		// resolution, a reaction, an edit or a reviewer request. Before,
		// they waited on a selected diff, and from Discuss with no file
		// open GitHub was never asked: "could not be resolved: unavailable".
		const auto onSelectedDiff = aTestingRequest && !aTestingRequest->ThreadReply;
		if (AttendedReviewMutationMode || !Host || !WindowRead->accepted_snapshot()
			|| !InlineReviewBinding || WorkbenchHost == nullptr
			|| (onSelectedDiff ? !SelectedDiffPublication.has_value() : !LastAcceptedHandoff)) {
			return grant;
		}
		const auto  authority = onSelectedDiff ? SelectedDiffPublication->Request.WorkbenchAuthority : workbench_authority(*LastAcceptedHandoff);
		const auto& snapshot  = *WindowRead->accepted_snapshot();
		// Reuse the accepted handoff binding retained by the host composition.
		if (snapshot.Account != authority.Account
			|| snapshot.PullRequest.PullRequest != authority.PullRequest) {
			return grant;
		}
#if defined(BPR_BUILD_TESTING)
		static_cast<void>(aIssue);
		grant = application::make_review_mutation_testing_grant(
			authority.ApplicationSession, WorkbenchHost->generation(),
			InlineReviewBinding->Target, InlineReviewBinding->ActingIdentity, aTestingRequest);
#else
		static_cast<void>(aTestingRequest);
		grant = aIssue(authority.ApplicationSession, WorkbenchHost->generation(),
					   InlineReviewBinding->Target, InlineReviewBinding->ActingIdentity);
#endif
		return grant;
	}

	void github_application::implementation::publish(
		const application::review_mutation_terminal& aTerminal)
	{
		if (!Closing && aTerminal.Lineage == application::review_lineage_state::pending && ReviewMutationHost) {
			ReviewMutationHost->repair_pending_publications();
		}
		using outcome       = core::review_publication_outcome;
		auto currentOutcome = outcome::published;
		if (!aTerminal.Result) {
			const auto failure         = aTerminal.Result.error();
			const auto definiteRefusal = failure == application::review_mutation_failure::refused_by_server
				|| application::journal_rejection(failure)
				|| failure == application::review_mutation_failure::incompatible_body
				|| failure == application::review_mutation_failure::unsupported;
			const auto unconfirmed = (aTerminal.MutationAttempted && !definiteRefusal)
				|| failure == application::review_mutation_failure::unavailable
				|| failure == application::review_mutation_failure::target_unavailable
				|| failure == application::review_mutation_failure::malformed_response;
			// [[BPR-1735]]. The journal's rejection says no comment exists.
			currentOutcome = application::journal_rejection(failure) ? outcome::rejected
				: unconfirmed                                        ? outcome::indeterminate
																	 : outcome::not_published;
		}
		const auto publicationOutcome = core::retain_review_publication_outcome(PublishingPriorOutcome, currentOutcome);
		if (!Closing && PendingReviewMutation == std::optional{ aTerminal.Context } && PrivateReview && PrivateDraftsReady && PublishingDraft) {
			if (aTerminal.Result && Window && Window->review_editor()) {
				Window->review_editor()->acknowledge_published_body(*PublishingDraft);
			}
			const auto uncertain = publicationOutcome == outcome::indeterminate;
			complete_shared_draft(aTerminal.Result.has_value(), [this, terminal = aTerminal, publicationOutcome] { finish_review_terminal(terminal, publicationOutcome); }, uncertain);
			return;
		}
		finish_review_terminal(aTerminal, publicationOutcome);
	}
}
