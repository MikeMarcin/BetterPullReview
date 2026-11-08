#include "github_application/window_composition.hpp"

#include "bpr/adapters/sqlite/review_mutation_recovery_store.hpp"
#include "bpr/adapters/sqlite/review_submission_recovery_store.hpp"
#include "bpr/application/github_host_authority.hpp"
#include "bpr/application/reply_thread_observation.hpp"
#include "bpr/application/review_preflight_observation.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/cached_review_overview.hpp"
#include "bpr/core/feedback_convention.hpp"
#include "bpr/core/private_thread_lineage.hpp"
#include "bpr/core/review_preflight.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/review_submission_editor.hpp"

#include <QDesktopServices>
#include <QModelIndex>
#include <QTextStream>
#include <QVariant>

#include <algorithm>

#if !defined(BPR_BUILD_TESTING)
	#include "live_review_submission_provider.hpp"
#endif
#if defined(BPR_BUILD_TESTING)
	#include "review_submission_fixture.hpp"
#endif

namespace bpr::app {
	void github_application::implementation::compose_review_submission(const application::accepted_pull_request_handoff& aHandoff)
	{
		if (!ReviewSubmission || !Host || !WindowRead->accepted_snapshot()) {
			if (TourPublication) {
				TourPublication->invalidate();
				if (ConversationReply) {
					ConversationReply->invalidate();
				}
			}
			return;
		}
#if defined(BPR_BUILD_TESTING)
		if (!Fixture) {
			ReviewSubmission->invalidate(QStringLiteral("This window is read-only. Open a fixture to try review composition, or use a configured publishing build for your pull request."));
			if (TourPublication) {
				TourPublication->invalidate();
				if (ConversationReply) {
					ConversationReply->invalidate();
				}
			}
			return;
		}
#endif
		auto selection = select_observed_review_mutation(aHandoff, *WindowRead->accepted_snapshot());
		if (!selection) {
			const auto closed = WindowRead->accepted_snapshot()->PullRequest.State != application::github_pull_request_state::open;
			ReviewSubmission->invalidate(closed
											 ? QStringLiteral("This pull request is closed. You can read its review, but cannot submit a new review. Open an active pull request to compose a review.")
											 : QStringLiteral("The current account or pull request revision is not ready for review submission. Refresh this pull request and wait for it to finish loading."));
			if (TourPublication) {
				TourPublication->invalidate();
				if (ConversationReply) {
					ConversationReply->invalidate();
				}
			}
			return;
		}
		auto binding       = application::review_submission_live_binding{ selection->Target, selection->ActingIdentity };
		auto configuration = adapters::sqlite::review_mutation_recovery_configuration{
			.StorageRoot = ApplicationDataRoot, .Host = std::string{ binding.Target.Hostname.value() }, .Account = std::to_string(binding.ActingIdentity.Account.value()), .Repository = binding.Target.Repository.Owner + "/" + binding.Target.Repository.Name, .PullRequest = static_cast<std::int64_t>(binding.Target.Number.value())
		};
#if defined(BPR_BUILD_TESTING)
		auto factory = [configuration, binding, server = ReviewSubmissionServer]() -> std::expected<std::unique_ptr<application::review_submission_port>, application::review_mutation_failure> {
			auto store = adapters::sqlite::make_review_submission_recovery_store(configuration);
			if (!store) {
				return std::unexpected(application::review_mutation_failure::unavailable);
			}
			return detail::make_review_submission_fixture_provider(server, std::move(*store), binding);
		};
#else
		auto acceptedLease = WindowRead->mutation_transport();
		if (!acceptedLease) {
			ReviewSubmission->invalidate(QStringLiteral("This window is read-only because no publishing connection is available. Connect an account with access, then reopen the pull request."));
			if (TourPublication) {
				TourPublication->invalidate();
				if (ConversationReply) {
					ConversationReply->invalidate();
				}
			}
			return;
		}
		// [[BPR-1540]]. Why a submission stays unknown reaches the diagnostics log.
		auto factory = [configuration, binding, lease = std::move(acceptedLease), log = DiagnosticLog]() -> std::expected<std::unique_ptr<application::review_submission_port>, application::review_mutation_failure> {
			auto store = adapters::sqlite::make_review_submission_recovery_store(configuration);
			if (!store) {
				return std::unexpected(application::review_mutation_failure::unavailable);
			}
			auto provider = adapters::github::make_live_review_submission_provider(lease, binding.Target, binding.ActingIdentity, std::move(*store), log);
			if (!provider) {
				return std::unexpected(provider.error());
			}
			return std::unique_ptr<application::review_submission_port>{ std::move(*provider) };
		};
#endif
		if (TourPublication) {
#if defined(BPR_BUILD_TESTING)
			auto tourFactory = [configuration, binding, server = ReviewSubmissionServer, description = WindowRead->accepted_snapshot()->PullRequest.Body]() -> std::expected<std::unique_ptr<application::review_submission_port>, application::review_mutation_failure> {
				auto store = adapters::sqlite::make_review_submission_recovery_store(configuration);
				if (!store) {
					return std::unexpected(application::review_mutation_failure::unavailable);
				}
				return detail::make_staged_review_fixture_provider(server, std::move(*store), binding, description);
			};
			if (ConversationReply) {
				ConversationReply->adopt(binding, workbench_authority(aHandoff), DraftWindow.Profile, tourFactory, insights_capture(aHandoff));
			}
			TourPublication->adopt(binding, workbench_authority(aHandoff), tourFactory, WindowRead->accepted_snapshot()->PullRequest.Body, static_cast<bool>(Fixture) && ReviewSubmissionServer->StagedQualified, insights_capture(aHandoff));
#else
			// [[BPR-1222]]. Live publication is on (owner amendment 2026-10-09);
			// Publish still asks the write confirmation and spends a one-use grant.
			if (ConversationReply) {
				ConversationReply->adopt(binding, workbench_authority(aHandoff), DraftWindow.Profile, factory, insights_capture(aHandoff));
			}
			TourPublication->adopt(binding, workbench_authority(aHandoff), factory, WindowRead->accepted_snapshot()->PullRequest.Body, true, insights_capture(aHandoff));
#endif
		}
		// [[BPR-1853]]. Its author may only Comment; the core compares
		// this with the account the review submits as.
		const auto& author = WindowRead->accepted_snapshot()->PullRequest.Author;
		ReviewSubmission->observe_author(author.has_value() ? author->Login : std::string{ });
		ReviewSubmission->adopt(std::move(binding), workbench_authority(aHandoff), std::move(factory), insights_capture(aHandoff));
		observe_review_preflight();
	}

	/// [[BPR-1085]]. The accepted snapshot's published feedback, read once
	/// per snapshot for the review preflight. Context only: no request is
	/// made and the review draft is not touched.
	void github_application::implementation::observe_review_preflight()
	{
		if (!ReviewSubmission || !WindowRead || !WindowRead->accepted_snapshot() || !LastAcceptedHandoff) {
			return;
		}
		const auto& snapshot = *WindowRead->accepted_snapshot();
		const auto& handoff  = *LastAcceptedHandoff;
		if (PreflightSnapshot == std::pair{ snapshot.Snapshot, handoff.Account }) {
			return;
		}
		PreflightSnapshot = std::pair{ snapshot.Snapshot, handoff.Account };
		ReviewSubmission->observe_preflight(application::observe_review_preflight(
			core::cached_review_key{ handoff.HostAuthority.bytes(), handoff.Profile, handoff.Host, handoff.Account, handoff.Locator, handoff.Number }, snapshot));
	}

	auto github_application::implementation::current(const core::bpr_window_context& aContext) const noexcept -> bool
	{
		return !Closing && aContext == DraftWindow && Profile->current_window(aContext);
	}

	/// \brief [[BPR-1326]]. Every unpublished draft the window holds, as
	/// saved, so each shows at its line and not only the selected one.
	void github_application::implementation::present_saved_drafts()
	{
		if (Closing || Window == nullptr) {
			return;
		}
		auto drafts = std::vector<core::review_comment_draft>{ };
		auto saved  = SharedDrafts ? SharedDrafts->recovery_snapshot() : std::vector<core::revisioned_review_draft>{ };
		for (const auto& draft : saved) {
			drafts.push_back(draft.Draft);
		}
		// [[BPR-1374]]. Where each unsent reply stands against the
		// threads the server holds now, as the core decides it.
		auto replies = core::decide_reply_drafts(drafts, reply_threads());
		Window->present_saved_drafts(std::move(drafts), std::move(replies));
		publish_review_comments(saved);
		// [[BPR-1733]]. A carried or deleted suggestion leaves its note.
		present_moved_suggestions();
#if defined(BPR_AGENT_INTEGRATIONS)
		// [[BPR-1862]]. An agent turn whose draft was deleted, by any
		// route, offers Draft comment again.
		for (auto& [key, answer] : AgentAnswerDrafts) {
			if (!answer.Drafted || !SharedDrafts) {
				continue;
			}
			const auto read = SharedDrafts->read(DraftConsumer, *answer.Drafted);
			if (read.Accepted && read.Accepted->Draft.State != core::review_draft_state::discarded) {
				continue;
			}
			answer.Drafted.reset();
			Window->agent_tab()->release_answer_draft(key);
		}
#endif
	}

	/// \brief [[BPR-1374]]. The accepted snapshot's review threads, as the
	/// core reads them for an unsent reply; incomplete with none.
	auto github_application::implementation::reply_threads() const -> core::reply_thread_observation
	{
		return WindowRead && WindowRead->accepted_snapshot() ? application::observe_reply_threads(*WindowRead->accepted_snapshot()) : core::reply_thread_observation{ };
	}

	auto github_application::implementation::saved_draft_values() const -> std::vector<core::review_comment_draft>
	{
		auto drafts = std::vector<core::review_comment_draft>{ };
		if (SharedDrafts) {
			for (const auto& saved : SharedDrafts->recovery_snapshot()) {
				drafts.push_back(saved.Draft);
			}
		}
		return drafts;
	}

	/// \brief [[BPR-1374]]. The editor's draft is a reply: name its thread
	/// and its author where the label is.
	void github_application::implementation::describe_held_reply()
	{
		auto* const editor = Window != nullptr ? Window->review_editor() : nullptr;
		if (editor == nullptr || !ReviewDraft || !ReviewDraft->ReplyToThread) {
			return;
		}
		auto author = QString{ };
		if (auto* const model = Window->conversation_model(); model != nullptr) {
			const auto index = model->index_for_id("review-thread:" + *ReviewDraft->ReplyToThread);
			if (index.isValid()) {
				author = index.data(qt::github_conversation_model::author_login_role).toString();
			}
		}
		// [[BPR-1745]]. A reply bound to its outdated thread says so.
		if (core::thread_bound_reply(*ReviewDraft)) {
			editor->describe_reply_target(author.isEmpty() ? QStringLiteral("Replying in this outdated thread") : QStringLiteral("Replying to %1's outdated thread").arg(author));
			return;
		}
		editor->describe_reply_target(author.isEmpty() ? QStringLiteral("Replying in this thread") : QStringLiteral("Replying to %1's thread").arg(author));
	}

	/// \brief [[BPR-1745]]. What GitHub's thread data, as the
	/// accepted snapshot holds it, says about replying in one thread.
	/// \details [[BPR-1746]]. With GitHub's `viewerCanReply` and the
	/// pull request's lock when the conversation read carries them; a
	/// server whose schema lacks them leaves them unknown, and GitHub
	/// decides at Publish.
	auto github_application::implementation::thread_reply_facts_of(const application::github_review_thread_record& aThread) const -> core::thread_reply_facts
	{
		const auto* const snapshot = WindowRead && WindowRead->accepted_snapshot() ? &*WindowRead->accepted_snapshot() : nullptr;
		return core::thread_reply_facts{
			.ReviewThread   = !aThread.Path.empty() && aThread.Subject == application::github_review_thread_subject::line,
			.Outdated       = aThread.Outdated,
			.AnonymousRead  = snapshot == nullptr || snapshot->Mode != core::github_access_mode::authenticated,
			.ViewerCanReply = aThread.ViewerReplyPermission == application::github_permission_state::unavailable
				? std::nullopt
				: std::optional{ aThread.ViewerReplyPermission == application::github_permission_state::allowed },
			.Locked         = snapshot == nullptr || snapshot->ConversationLock == application::github_conversation_lock::unavailable
				? std::nullopt
				: std::optional{ snapshot->ConversationLock == application::github_conversation_lock::locked },
		};
	}

	/// \brief [[BPR-1745]]. Where GitHub answers a reply bound to
	/// an outdated thread: the thread's path and the commit its first
	/// comment was written on, from the accepted snapshot.
	auto github_application::implementation::thread_reply_binding_of(const std::string_view aThread) const -> std::optional<application::review_mutation_thread_reply_binding>
	{
		const auto* const thread = accepted_thread("review-thread:" + std::string{ aThread });
		if (thread == nullptr || core::decide_thread_reply(thread_reply_facts_of(*thread)).Route != core::thread_reply_route::on_thread) {
			return std::nullopt;
		}
		const auto root = std::ranges::find_if(thread->Comments, [](const application::github_review_comment_record& aComment) { return !aComment.ReplyTo.has_value(); });
		if (root == thread->Comments.end()) {
			return std::nullopt;
		}
		const auto commit = core::parse_git_commit_oid(root->Observation.OriginalCommit);
		if (!commit) {
			return std::nullopt;
		}
		return application::review_mutation_thread_reply_binding{ .Path = thread->Path, .OriginalCommit = *commit };
	}

	/// \brief [[BPR-1745]]. An outdated thread's Reply: the reply
	/// written in it and not sent opens again, or a new one bound to the
	/// thread is made. It is a draft like any reply, saved privately,
	/// published through the reply path and reconciled against the thread.
	void github_application::implementation::begin_thread_bound_reply(const std::string& aIdentity)
	{
		constexpr auto prefix = std::string_view{ "review-thread:" };
		if (Closing || Window == nullptr || !SharedDrafts || !aIdentity.starts_with(prefix)) {
			return;
		}
		auto thread = std::string{ aIdentity.substr(prefix.size()) };
		if (thread.empty() || !thread_reply_binding_of(thread)) {
			return;
		}
		if (ReviewDraft && core::thread_bound_reply(*ReviewDraft) && *ReviewDraft->ReplyToThread == thread
			&& (ReviewDraft->State == core::review_draft_state::drafting || ReviewDraft->State == core::review_draft_state::publishing)) {
			describe_held_reply();
			return;
		}
		ResumeReply.reset();
		PendingThreadReply.reset();
		if (PrivateReview) {
			if (!PrivateDraftsReady || PrivateDraftSelectionPending) {
				// The saved drafts are still being restored, or the line's
				// own draft is being opened: the reply follows, so it is the
				// draft the editor holds.
				PendingThreadReply = aIdentity;
				return;
			}
			const auto owner             = SharedDrafts;
			PrivateDraftSelectionPending = true;
			PrivateReview->select_thread_reply(thread, [this, owner](auto aCommit) {
				PrivateDraftSelectionPending = false;
				if (Closing || !owner || owner != SharedDrafts || !aCommit || aCommit->records().empty() || !owner->adopt(owner->authority(), *aCommit)) {
					return;
				}
				present_saved_drafts();
				const auto accepted = owner->read(DraftConsumer, aCommit->records().back().Draft.Draft.Id);
				if (accepted.Accepted) {
					accept_shared_draft(*accepted.Accepted);
					describe_held_reply();
				}
			});
			return;
		}
		// No private store: the reply is held for this session only.
		const auto comparison = SelectedDiffPublication ? std::optional{ core::comparison_digest(SelectedDiffPublication->Key) } : std::nullopt;
		if (!comparison) {
			return;
		}
		const auto saved = core::reopen_thread_reply(saved_draft_values(), thread);
		auto       held  = saved ? SharedDrafts->read(DraftConsumer, *saved) : SharedDrafts->select(DraftConsumer, core::thread_reply_anchor(*comparison), thread);
		if (held.Accepted && !held.Failure) {
			accept_shared_draft(*held.Accepted);
			describe_held_reply();
		}
	}

	void github_application::implementation::resume_pending_thread_reply()
	{
		if (Closing || PrivateDraftSelectionPending || !PrivateDraftsReady || !PendingThreadReply) {
			return;
		}
		begin_thread_bound_reply(*std::exchange(PendingThreadReply, std::nullopt));
	}

	/// \brief [[BPR-1374]]. A Refresh retires the editor's draft with the
	/// composition it was written in. The reply it held is held again
	/// once its line is chosen, as the core decides: unsent, with its
	/// thread still there. Answers whether it was.
	auto github_application::implementation::resume_reply_at(const core::source_line_ref& aLine) -> bool
	{
		if (!ResumeReply || !SharedDrafts || PendingReviewMutation) {
			return false;
		}
		const auto drafts = saved_draft_values();
		const auto again  = core::resume_reply_after_refresh(drafts, *ResumeReply, reply_threads());
		if (!again) {
			ResumeReply.reset();
			return false;
		}
		const auto reply = std::ranges::find(drafts, *again, &core::review_comment_draft::Id);
		if (reply == drafts.end() || reply->Anchor != aLine) {
			return false;
		}
		const auto read = SharedDrafts->read(DraftConsumer, *again);
		if (!read.Accepted) {
			return false;
		}
		ResumeReply.reset();
		accept_shared_draft(*read.Accepted);
		describe_held_reply();
		refresh_agent_private_context_choices();
		return true;
	}

	/// \brief [[BPR-1374]]. Once the refreshed comparison is shown again,
	/// its reply's line is chosen, which holds the reply again. The line
	/// was the selected one when the reply was written, and a same-head
	/// Refresh keeps it on screen without choosing it again.
	void github_application::implementation::resume_reply()
	{
		resume_comment();
		if (Closing || !ResumeReply || !SharedDrafts || Window == nullptr || PendingReviewMutation) {
			return;
		}
		const auto drafts = saved_draft_values();
		const auto again  = core::resume_reply_after_refresh(drafts, *ResumeReply, reply_threads());
		if (!again) {
			ResumeReply.reset();
			return;
		}
		const auto reply = std::ranges::find(drafts, *again, &core::review_comment_draft::Id);
		// [[BPR-1745]]. A reply bound to its thread has no line to
		// choose: it is held again as it is.
		if (reply != drafts.end() && core::thread_bound_reply(*reply)) {
			if (const auto read = SharedDrafts->read(DraftConsumer, *again); read.Accepted) {
				ResumeReply.reset();
				accept_shared_draft(*read.Accepted);
				describe_held_reply();
				refresh_agent_private_context_choices();
			}
			return;
		}
		if (!SelectedDiffPublication) {
			return;
		}
		if (reply == drafts.end() || core::comparison_digest(SelectedDiffPublication->Key) != reply->Anchor.Comparison) {
			return;
		}
		auto* const surface   = Window->comparison_surface();
		const auto  selection = surface->selection();
		if (selection && selection->PreferredLine == reply->Anchor) {
			if (SelectedSemanticSelection == selection) {
				present_review_draft(*selection);
			} else {
				observe_review_selection(selection);
			}
			return;
		}
		static_cast<void>(surface->select_conversation_line(reply->Anchor.Side, reply->Anchor.Line));
	}

	/// \brief [[BPR-1475]]. The new comment a reread retired is held again
	/// once the reread comparison is shown, as the core decides, while its
	/// line is still the selected one. A reviewer who has moved on keeps
	/// the place they moved to.
	void github_application::implementation::resume_comment()
	{
		if (Closing || !ResumeComment || !SharedDrafts || Window == nullptr || PendingReviewMutation || !SelectedDiffPublication) {
			return;
		}
		const auto drafts = saved_draft_values();
		const auto again  = core::resume_comment_after_refresh(drafts, *ResumeComment);
		if (!again) {
			ResumeComment.reset();
			return;
		}
		const auto comment = std::ranges::find(drafts, *again, &core::review_comment_draft::Id);
		if (comment == drafts.end() || core::comparison_digest(SelectedDiffPublication->Key) != comment->Anchor.Comparison) {
			return;
		}
		const auto selection = Window->comparison_surface()->selection();
		if (!selection || selection->PreferredLine != comment->Anchor) {
			ResumeComment.reset();
			return;
		}
		if (SelectedSemanticSelection == selection) {
			present_review_draft(*selection);
		} else {
			observe_review_selection(selection);
		}
	}

	/// \brief [[BPR-1475]]. Choosing the held comment's line holds it again.
	auto github_application::implementation::resume_comment_at(const core::source_line_ref& aLine) -> bool
	{
		if (!ResumeComment || !SharedDrafts || PendingReviewMutation) {
			return false;
		}
		const auto drafts  = saved_draft_values();
		const auto again   = core::resume_comment_after_refresh(drafts, *ResumeComment);
		const auto comment = again ? std::ranges::find(drafts, *again, &core::review_comment_draft::Id) : drafts.end();
		if (comment == drafts.end() || comment->Anchor != aLine) {
			ResumeComment.reset();
			return false;
		}
		const auto read = SharedDrafts->read(DraftConsumer, *again);
		ResumeComment.reset();
		if (!read.Accepted) {
			return false;
		}
		accept_shared_draft(*read.Accepted);
		refresh_agent_private_context_choices();
		// [[BPR-1477]]. A comment whose answer was lost is held again for
		// the same retry, under the new composition, and says why again.
		const auto retry = read.Accepted->Draft.State == core::review_draft_state::publishing;
		if (retry && SelectedDiffPublication) {
			RetryableReviewDraft = retryable_review_draft_record{
				.Draft     = read.Accepted->Draft,
				.Authority = SelectedDiffPublication->Request.WorkbenchAuthority,
			};
			present_inline_publication_failure(ResumeCommentFailure, core::review_publication_outcome::indeterminate, InlineReviewBinding);
		}
		// Open in the drawer again if it was open there. A locked draft is
		// opened at its line, not in Discuss's full composer.
		if (std::exchange(ResumeCommentOpen, false) && Window != nullptr) {
			static_cast<void>(retry ? Window->reopen_draft_in_drawer() : Window->begin_new_comment());
		}
		return true;
	}

	void github_application::implementation::change_unsent_reply(const core::revisioned_review_draft_command& aCommand)
	{
		const auto owner    = SharedDrafts;
		const auto consumer = DraftConsumer;
		PrivateReview->change_draft(aCommand, [this, owner, consumer](auto aCommit) {
			if (!Closing && owner == SharedDrafts && consumer == DraftConsumer && aCommit && owner->adopt(owner->authority(), *aCommit)) {
				present_saved_drafts();
			}
		});
	}

	/// [[BPR-1527]]. A visible kept reply can be acted on while a reread
	/// reopens its private store. Keep its original command and CAS; a
	/// different owner, consumer or draft revision never inherits it.
	void github_application::implementation::flush_unsent_reply_commands()
	{
		if (!PendingUnsentReplyCommands) {
			return;
		}
		if (Closing || PendingUnsentReplyCommands->Owner != SharedDrafts || PendingUnsentReplyCommands->Consumer != DraftConsumer) {
			PendingUnsentReplyCommands.reset();
			return;
		}
		if (!PrivateReview || !PrivateDraftsReady || !PrivateReview->binding()) {
			return;
		}
		auto pending = std::exchange(PendingUnsentReplyCommands, std::nullopt);
		for (const auto& [id, command] : pending->Commands) {
			if (Closing || pending->Owner != SharedDrafts || pending->Consumer != DraftConsumer) {
				return;
			}
			const auto current = SharedDrafts->read(DraftConsumer, id);
			if (current.Accepted && current.Accepted->Revision == command.ExpectedRevision && current.Accepted->Scope == command.Scope) {
				change_unsent_reply(command);
			}
		}
	}

	/// \brief [[BPR-1374]]. A kept reply's note: its words become a new
	/// comment on its line, or it is discarded. Decided by the core
	/// against the threads the server holds now; nothing is published.
	void github_application::implementation::act_on_unsent_reply(const core::review_draft_id aDraft, const qt::workbench_window::unsent_reply_action aAction)
	{
		if (Closing || !SharedDrafts) {
			return;
		}
		const auto read = SharedDrafts->read(DraftConsumer, aDraft);
		if (!read.Accepted) {
			return;
		}
		const auto command = aAction == qt::workbench_window::unsent_reply_action::keep_as_comment
			? core::keep_reply_as_comment(*read.Accepted, reply_threads())
			: core::discard_unsent_reply(*read.Accepted);
		if (!command) {
			return;
		}
		if (PrivateReview) {
			if (!PrivateDraftsReady || !PrivateReview->binding()) {
				if (!PendingUnsentReplyCommands || PendingUnsentReplyCommands->Owner != SharedDrafts || PendingUnsentReplyCommands->Consumer != DraftConsumer) {
					pending_unsent_reply_commands pending;
					pending.Owner              = SharedDrafts;
					pending.Consumer           = DraftConsumer;
					PendingUnsentReplyCommands = std::move(pending);
				}
				PendingUnsentReplyCommands->Commands.insert_or_assign(aDraft, *command);
			} else {
				change_unsent_reply(*command);
			}
			return;
		}
		if (const auto outcome = SharedDrafts->apply(DraftConsumer, *command); !outcome.Failure && outcome.Accepted) {
			present_saved_drafts();
		}
	}

	/// [[BPR-1345]]. The review carries every draft pending in it on this
	/// head, in file and line order, as the draft owner holds it; a draft a
	/// submission holds is settled by that submission's answer.
	void github_application::implementation::publish_review_comments(const std::vector<core::revisioned_review_draft>& aSaved)
	{
		if (!ReviewSubmission || !SharedDrafts) {
			return;
		}
		const auto& head     = SharedDrafts->authority().Target.ExpectedHead;
		auto        comments = std::vector<core::review_submission_comment>{ };
		auto        held     = std::vector<std::string>{ };
		for (const auto& saved : aSaved) {
			const auto& draft = saved.Draft;
			if (core::pending_in_review(draft) && draft.InReview->Head == head) {
				auto comment          = core::review_submission_comment{ };
				comment.Draft         = draft.Id;
				comment.DraftRevision = saved.Revision;
				comment.Path          = draft.InReview->Path;
				comment.Side          = draft.Anchor.Side;
				comment.Line          = draft.Anchor.Line;
				comment.StartLine     = draft.StartLine;
				comment.Body          = draft.Body;
				comments.push_back(std::move(comment));
			} else if (core::held_by_review(draft) && std::ranges::find(held, draft.InReview->Submission) == held.end()) {
				held.push_back(draft.InReview->Submission);
			}
		}
		std::ranges::sort(comments, [](const core::review_submission_comment& aLeft, const core::review_submission_comment& aRight) {
			return std::tie(aLeft.Path, aLeft.Side, aLeft.Line, aLeft.Draft) < std::tie(aRight.Path, aRight.Side, aRight.Line, aRight.Draft);
		});
		PendingReviewComments = comments;
		ReviewSubmission->observe_review_comments(std::move(comments));
		if (!held.empty()) {
			ReviewSubmission->resolve_held(std::move(held));
		}
		present_review_comment_holds();
	}

	/// [[BPR-1345]]. Start a review, or Add review comment: the shown
	/// draft stays as written and becomes pending in the review, marked
	/// with where it goes, decided exactly as Publish decides it.
	auto github_application::implementation::start_review_on_shown_draft() -> bool
	{
		auto* const editor = Window != nullptr ? Window->review_editor() : nullptr;
		if (Closing || editor == nullptr || !ReviewDraft || !SharedDrafts || ReviewMutationHost == nullptr) {
			return false;
		}
		if (ReviewDraft->State != core::review_draft_state::drafting || ReviewDraft->ReplyToThread) {
			editor->present_publication_refusal(QStringLiteral("Only a new comment being written can be added to a review."));
			return false;
		}
		if (!SelectedDiffPublication || !SelectedSemanticSelection) {
			editor->present(application::review_mutation_failure::undesignated_target);
			return false;
		}
		auto proposal        = core::review_publication_proposal{ };
		proposal.Draft       = ReviewDraft->Id;
		proposal.Anchor      = ReviewDraft->Anchor;
		proposal.Body        = editor->body_text();
		proposal.StartLine   = ReviewDraft->StartLine;
		auto request         = application::review_mutation_publication_request{ };
		request.Proposal     = std::move(proposal);
		request.Selection    = *SelectedSemanticSelection;
		request.SelectedDiff = *SelectedDiffPublication;
		// [[BPR-1706]]. A review's comments are line comments; the host
		// refuses a filtered line GitHub cannot take as one, and this says why.
		if (const auto& filtered = Window->filtered_comment_placement();
			filtered && filtered->Selection == request.Selection.PreferredLine && filtered->Target.Placement == core::filtered_comment_placement::file_comment) {
			editor->present_publication_refusal(QStringLiteral("GitHub can't attach a comment to this line, so it goes on the file and is published on its own, not in a review. Use Publish."));
			return false;
		}
		const auto destination = ReviewMutationHost->comment_destination(request);
		if (!destination) {
			editor->present(destination.error());
			return false;
		}
		// RFC-BPR-0089 M4. A review's comments go at the head at their own
		// lines; a filtered diff's line placed elsewhere is published alone.
		if (destination->RemoteLine) {
			editor->present_publication_refusal(QStringLiteral("A comment placed off this filtered diff's own line or commit is published on its own, not in a review. Use Publish."));
			return false;
		}
		auto mark = core::review_pending_mark{ };
		mark.Path = destination->Path;
		mark.Head = destination->Commit;
		if (mark.Head != SharedDrafts->authority().Target.ExpectedHead || !core::valid(mark)) {
			editor->present(application::review_mutation_failure::stale_head);
			return false;
		}
		if (QString::fromStdString(request.Proposal.Body).trimmed().isEmpty()) {
			editor->present_publication_refusal(QStringLiteral("Write the comment before adding it to a review."));
			return false;
		}
		return editor->change_pending(std::move(mark));
	}

	/// [[BPR-1345]]. Changes several drafts in one durable write, then
	/// presents them; answers whether every change was accepted.
	void github_application::implementation::change_review_drafts(std::vector<core::revisioned_review_draft_command> aCommands, std::function<void(bool)> aDone)
	{
		if (aCommands.empty()) {
			aDone(true);
			return;
		}
		if (Closing || !SharedDrafts) {
			aDone(false);
			return;
		}
		const auto owner   = SharedDrafts;
		const auto present = [this, owner](const std::vector<core::revisioned_review_draft_command>& aChanged) {
			present_saved_drafts();
			for (const auto& command : aChanged) {
				if (ReviewDraft && ReviewDraft->Id == command.Command.Id) {
					if (const auto accepted = owner->read(DraftConsumer, command.Command.Id); accepted.Accepted) {
						accept_shared_draft(*accepted.Accepted);
					}
				}
			}
			// [[BPR-1862]]. A draft deleted from Finish your review, or by
			// Discard review, while it is the line's draft.
			leave_deleted_draft();
		};
		if (PrivateReview && PrivateDraftsReady) {
			PrivateReview->change_drafts(aCommands, [this, owner, commands = aCommands, present, done = std::move(aDone)](auto aCommit) {
				const auto adopted = !Closing && owner == SharedDrafts && aCommit && owner->adopt(owner->authority(), *aCommit);
				if (adopted) {
					present(commands);
				}
				done(adopted);
			});
			return;
		}
		auto applied = std::vector<core::revisioned_review_draft_command>{ };
		for (const auto& command : aCommands) {
			const auto outcome = SharedDrafts->apply(DraftConsumer, command);
			if (outcome.Failure || !outcome.Accepted) {
				present(applied);
				aDone(false);
				return;
			}
			applied.push_back(command);
		}
		present(applied);
		aDone(true);
	}

	/// [[BPR-1345]]. Holds every draft a submission carries, at the exact
	/// revision it carries, with the submission's key, before it is sent.
	void github_application::implementation::hold_review_comments(const application::review_submission_request& aRequest, std::function<void(bool)> aDone)
	{
		if (Closing || !SharedDrafts) {
			aDone(false);
			return;
		}
		auto commands = std::vector<core::revisioned_review_draft_command>{ };
		for (const auto& comment : aRequest.Proposal.Comments) {
			const auto read = SharedDrafts->read(DraftConsumer, comment.Draft);
			if (!read.Accepted || read.Accepted->Revision != comment.DraftRevision || !core::pending_in_review(read.Accepted->Draft)
				|| read.Accepted->Draft.Body != comment.Body || read.Accepted->Draft.InReview->Path != comment.Path) {
				aDone(false);
				return;
			}
			auto mark                = *read.Accepted->Draft.InReview;
			mark.Submission          = aRequest.IdempotencyKey;
			auto command             = core::revisioned_review_draft_command{ };
			command.Command          = core::review_draft_command{ .Id = comment.Draft, .Kind = core::review_draft_command_kind::begin_publish };
			command.ExpectedRevision = comment.DraftRevision;
			command.Scope            = read.Accepted->Scope;
			command.ChangePending    = true;
			command.Pending          = std::move(mark);
			commands.push_back(std::move(command));
		}
		change_review_drafts(std::move(commands), std::move(aDone));
	}

	/// [[BPR-1345]]. A submission's answer settles the drafts it holds:
	/// published completes each draft it carried at its exact held
	/// revision; definitely not published releases each, still pending.
	void github_application::implementation::settle_review_comments(const std::string& aKey, const application::review_submission_publication aPublication,
																	const std::optional<application::review_submission_request>& aRequest)
	{
		if (Closing || !SharedDrafts || aPublication == application::review_submission_publication::indeterminate) {
			return;
		}
		const auto published = aPublication == application::review_submission_publication::published;
		if (PrivateReview) {
			PrivateReview->settle_review_comments(aKey, published, aRequest ? std::optional{ aRequest->Proposal } : std::nullopt);
			return;
		}
		auto commands = std::vector<core::revisioned_review_draft_command>{ };
		for (const auto& saved : SharedDrafts->recovery_snapshot()) {
			if (auto command = core::settle_held_review_draft(saved, aKey, published, aRequest ? &aRequest->Proposal : nullptr)) {
				commands.push_back(std::move(*command));
			}
		}
		change_review_drafts(std::move(commands), [](bool) { });
	}

	/// [[BPR-1345]] v002. Discard review deletes every pending draft of the
	/// review, in one durable write (owner, discard-review v001: "Discard
	/// review should delete all the review comments"). A draft a
	/// submission holds is never pending, so it is never deleted here.
	/// [[BPR-1715]]. The review's drafts are the ones it carries:
	/// each at the revision it carries, pending at the review's head. A
	/// draft written on another revision, or changed since, is kept.
	void github_application::implementation::discard_review_comments(const std::vector<core::review_submission_comment>& aCarried, const core::git_commit_oid& aHead)
	{
		if (Closing || !SharedDrafts) {
			return;
		}
		auto commands = std::vector<core::revisioned_review_draft_command>{ };
		for (const auto& saved : SharedDrafts->recovery_snapshot()) {
			const auto carried = std::ranges::any_of(aCarried, [&saved](const core::review_submission_comment& aComment) {
				return aComment.Draft == saved.Draft.Id && aComment.DraftRevision == saved.Revision;
			});
			if (!carried || !core::pending_in_review(saved.Draft) || saved.Draft.InReview->Head != aHead) {
				continue;
			}
			auto command             = core::revisioned_review_draft_command{ };
			command.Command          = core::review_draft_command{ .Id = saved.Draft.Id, .Kind = core::review_draft_command_kind::discard };
			command.ExpectedRevision = saved.Revision;
			command.Scope            = saved.Scope;
			commands.push_back(std::move(command));
		}
		change_review_drafts(std::move(commands), [](bool) { });
	}

	/// [[BPR-1345]] v002. Delete, from Finish your review: the one pending
	/// draft the page listed, at the revision it listed; a draft changed
	/// since, or no longer pending, is left as it is.
	void github_application::implementation::delete_pending_comment(const core::review_draft_id aDraft, const std::uint64_t aRevision)
	{
		if (Closing || !SharedDrafts) {
			return;
		}
		const auto read = SharedDrafts->read(DraftConsumer, aDraft);
		if (!read.Accepted || read.Accepted->Revision != aRevision || !core::pending_in_review(read.Accepted->Draft)) {
			return;
		}
		auto command             = core::revisioned_review_draft_command{ };
		command.Command          = core::review_draft_command{ .Id = aDraft, .Kind = core::review_draft_command_kind::discard };
		command.ExpectedRevision = aRevision;
		command.Scope            = read.Accepted->Scope;
		change_review_drafts({ std::move(command) }, [](bool) { });
	}

	/// \brief [[BPR-1862]]. A deleted draft is no longer its line's draft.
	/// \details The store's discard leaves the draft's record, its words
	/// kept, in the discarded state; held as the selected draft it was
	/// still drawn as the line's unpublished note, so a confirmed Delete
	/// looked like it did nothing. The line's draft is read again instead,
	/// as for a draft that was never saved: nothing at the line, and a new
	/// comment there is a new draft.
	void github_application::implementation::leave_deleted_draft()
	{
		if (Closing || Window == nullptr || !ReviewDraft || ReviewDraft->State != core::review_draft_state::discarded) {
			return;
		}
		adopt_review_selection(SelectedSemanticSelection, qt::review_draft_editor_clear_reason::selection_cleared);
	}

	void github_application::implementation::accept_shared_draft(const core::revisioned_review_draft& aAccepted)
	{
		if (Closing || aAccepted.Scope != DraftScope) {
			return;
		}
		present_saved_drafts();
		ReviewDraft         = aAccepted.Draft;
		ReviewDraftRevision = aAccepted.Revision;
		if (Window && Window->review_editor()) {
			Window->review_editor()->present(aAccepted);
			Window->review_editor()->allow_publication_retry(PrivateReview && !PendingReviewMutation && aAccepted.Draft.State == core::review_draft_state::publishing && !core::held_by_review(aAccepted.Draft));
		}
		refresh_agent_private_context_choices();
		publish_preflight_selection();
		// [[BPR-1592]]. A note waiting to become this line's draft.
		resume_note_promotion();
	}

	/// [[BPR-1085]]. My unsent comment, as the review preflight's selected
	/// draft: a new comment (a reply does not make a conversation blocking)
	/// still being written or being published. Context only.
	void github_application::implementation::publish_preflight_selection()
	{
		if (!ReviewSubmission) {
			return;
		}
		auto selection  = std::vector<core::review_preflight_draft>{ };
		auto references = std::map<std::string, qt::review_submission_preflight_reference>{ };
		if (ReviewDraft && !ReviewDraft->ReplyToThread && !ReviewDraft->Body.empty()
			&& (ReviewDraft->State == core::review_draft_state::drafting || ReviewDraft->State == core::review_draft_state::publishing)) {
			const auto  id         = "draft:" + std::to_string(ReviewDraft->Id.value());
			const auto& body       = ReviewDraft->Body;
			const auto  annotation = core::recognize_feedback_prefix(body);
			auto        subject    = annotation.Subject ? core::feedback_text(body, *annotation.Subject) : std::string_view{ body };
			subject                = subject.substr(0U, subject.find('\n'));
			selection.push_back({ .Id = id, .Body = body, .Selected = true });
			references.emplace(id, qt::review_submission_preflight_reference{ .Where = QStringLiteral("Line %1").arg(ReviewDraft->Anchor.Line + 1U), .Subject = QString::fromUtf8(subject.data(), static_cast<qsizetype>(subject.size())).trimmed() });
		}
		ReviewSubmission->observe_selection(std::move(selection), std::move(references));
	}

	auto github_application::implementation::ensure_shared_drafts(const application::accepted_pull_request_handoff& aHandoff) -> bool
	{
		const auto* profile = find_profile(aHandoff.Profile);
		if (!profile || profile->revision().value() != aHandoff.ProfileRevision.value() || profile->host() != aHandoff.Host) {
			return false;
		}
		const auto& accepted = Host->accepted_session();
		if (!accepted || accepted->Profile != aHandoff.Profile || accepted->Host != aHandoff.Host
			|| accepted->Viewer.Account != aHandoff.Account || accepted->Mode != aHandoff.Mode
			|| Host->accepted_identity_generation() != aHandoff.IdentityGeneration
			|| aHandoff.ApplicationSession != WindowSession || !current(DraftWindow)) {
			return false;
		}
		auto authority = application::shared_draft_authority{
			.BprProfile       = DraftWindow.Profile,
			.ProfileAuthority = { profile->profile(), profile->revision(), profile->host(), application::github_canonical_hostname{ profile->hostname() }, profile->server_contract(), profile->authentication_transport(), application::github_probe_contract_revision{ 1U } },
			.HostAuthority    = aHandoff.HostAuthority,
			.Session          = Host->state().ApplicationSession,
			.Generation       = aHandoff.IdentityGeneration,
			.Account          = aHandoff.Account,
			.Mode             = aHandoff.Mode,
			.Target           = review_mutation_target(aHandoff),
		};
		if (SharedDrafts && SharedDrafts->authority() == authority) {
			return true;
		}
		PendingUnsentReplyCommands.reset();
		if (SharedDrafts) {
			SharedDrafts->detach(DraftConsumer);
		}
		PublishingDraft.reset();
		auto owner = Profile->drafts(authority);
		if (!owner) {
			SharedDrafts.reset();
			return false;
		}
		DraftScope    = owner->Scope;
		SharedDrafts  = owner->Host;
		auto attached = SharedDrafts->attach(SharedDrafts->authority(), DraftWindow,
											 [this](const core::revisioned_review_draft& aAccepted) {
												 // [[BPR-1521]]. A sibling's first save replaces only
												 // our untouched selection at this exact anchor. A local
												 // candidate keeps its own first-save acknowledgement.
												 const auto* editor    = Window ? Window->review_editor() : nullptr;
												 const auto  untouched = ReviewDraft && ReviewDraft->Id.value() > core::maximum_private_identity
													 && core::unused_review_draft({ *ReviewDraft, ReviewDraftRevision, DraftScope })
													 && aAccepted.Scope == DraftScope && aAccepted.Draft.Id.value() <= core::maximum_private_identity
													 && aAccepted.Draft.Anchor == ReviewDraft->Anchor && aAccepted.Draft.ReplyToThread == ReviewDraft->ReplyToThread
													 && (aAccepted.Draft.State == core::review_draft_state::drafting || aAccepted.Draft.State == core::review_draft_state::publishing)
													 && !PrivateDraftCommandPending && editor && !editor->rejected_candidate() && editor->shown_draft() == ReviewDraft;
												 if (!Closing && ReviewDraft && (ReviewDraft->Id == aAccepted.Draft.Id || untouched)) {
													 accept_shared_draft(aAccepted);
												 } else {
													 present_saved_drafts();
												 }
											 });
		if (!attached) {
			SharedDrafts.reset();
			return false;
		}
		DraftConsumer = *attached;
		present_saved_drafts();
		return true;
	}

	void github_application::implementation::complete_shared_draft(const bool aSuccess, std::function<void()> aAfter, bool aPreserveForRetry)
	{
		if (!SharedDrafts || !PublishingDraft) {
			return;
		}
		const auto publishing = std::exchange(PublishingDraft, std::nullopt);
		if (PrivateReview && PrivateDraftsReady) {
			const auto owner    = SharedDrafts;
			auto       complete = [this, owner, publishing, after = std::move(aAfter)](auto aCommit) {
				if (!Closing && owner == SharedDrafts && aCommit && owner->adopt(owner->authority(), *aCommit)) {
					present_saved_drafts();
					const auto accepted = owner->read(DraftConsumer, publishing->Draft.Id);
					if (accepted.Accepted && ReviewDraft && ReviewDraft->Id == publishing->Draft.Id) {
						accept_shared_draft(*accepted.Accepted);
					}
				}
				if (after) {
					after();
				}
			};
			if ((aSuccess || aPreserveForRetry) && !publishing->Draft.ReplyToThread) {
				PrivateReview->restore(std::move(complete));
			} else {
				PrivateReview->change_draft({ { publishing->Draft.Id, aSuccess ? core::review_draft_command_kind::complete_publish : core::review_draft_command_kind::fail_publish, { } }, publishing->Revision, false, { }, publishing->Scope }, std::move(complete));
			}
			return;
		}

		auto outcome = aPreserveForRetry ? SharedDrafts->read(DraftConsumer, publishing->Draft.Id)
										 : SharedDrafts->complete(SharedDrafts->authority(), publishing->Draft.Id, publishing->Revision, aSuccess);
		if (!outcome.Failure && outcome.Accepted) {
			accept_shared_draft(*outcome.Accepted);
		}
	}
}
