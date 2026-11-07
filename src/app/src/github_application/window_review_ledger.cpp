#include "github_application/window_composition.hpp"

#include "bpr/adapters/sqlite/review_ledger_store.hpp"
#include "bpr/application/github_host_authority.hpp"
#include "bpr/application/observing_review_ledger_store.hpp"
#include "bpr/application/private_review_store.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/diff_compute.hpp"
#include "bpr/core/insights.hpp"
#include "bpr/core/private_review.hpp"
#include "bpr/core/private_thread_lineage.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/github_cli_consent.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/workbench_strip.hpp"
#include "review_mutation_execution_authority.hpp"

#include <QDesktopServices>
#include <QModelIndex>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <limits>

namespace bpr::app {
	// [[BPR-1046]]. Whether a draft sits on the accepted thread's own line,
	// which is where the window selects before it names the thread.
	auto github_application::implementation::draft_on_thread_line(
		const core::review_comment_draft& aDraft,
		const std::string_view            aIdentity) const -> bool
	{
		const auto* const thread = accepted_thread(aIdentity);
		if (thread == nullptr || thread->Outdated || !thread->Line.has_value() || *thread->Line == 0U) {
			return false;
		}
		const auto side = thread->DiffSide == application::github_diff_side::left
			? std::optional{ core::diff_side::left }
			: thread->DiffSide == application::github_diff_side::right
			? std::optional{ core::diff_side::right }
			: std::nullopt;
		return side.has_value() && aDraft.Anchor.Side == *side
			&& aDraft.Anchor.Line == *thread->Line - 1U;
	}

	// [[BPR-0657]], [[BPR-1046]]. A card's Reply names the thread the draft
	// on the thread's line answers. The window selects that line first,
	// which composes the draft, and names the thread only then. A draft on
	// any other line is left alone, and nothing is held for a later draft:
	// a thread held that way was answered by whichever draft came next,
	// wherever it was anchored. The thread identity the cards carry is the
	// projection's prefixed one; the provider's is what the proposal names.
	void github_application::implementation::begin_review_reply(const std::string& aIdentity)
	{
		constexpr auto prefix = std::string_view{ "review-thread:" };
		// [[BPR-1746]]. A thread GitHub says this viewer cannot
		// reply in takes no reply: the window has said why, and no draft
		// is made for a Publish GitHub would refuse.
		if (const auto* const thread = accepted_thread(aIdentity);
			thread != nullptr && core::decide_thread_reply(thread_reply_facts_of(*thread)).Route == core::thread_reply_route::refused) {
			return;
		}
		// [[BPR-1374]]. A reply already written in this thread and not
		// sent opens again, as the core picks it, rather than a new one:
		// after a restart Reply used to start an empty reply beside it.
		if (!Closing && SharedDrafts && aIdentity.starts_with(prefix) && SelectedSemanticSelection
			&& draft_on_thread_line(core::review_comment_draft{ .Anchor = SelectedSemanticSelection->PreferredLine }, aIdentity)) {
			const auto saved = core::reopen_draft_at(saved_draft_values(), SelectedSemanticSelection->PreferredLine, std::string{ aIdentity.substr(prefix.size()) });
			if (saved && (!ReviewDraft || ReviewDraft->Id != *saved)) {
				if (const auto read = SharedDrafts->read(DraftConsumer, *saved); read.Accepted) {
					ResumeReply.reset();
					accept_shared_draft(*read.Accepted);
					describe_held_reply();
					refresh_agent_private_context_choices();
					return;
				}
			}
		}
		// [[BPR-1527]]. An unused line placeholder has no durable draft
		// identity to retarget. Select the exact reply through the existing
		// private operation instead; first-save materialization deliberately
		// refuses a ChangeReplyTarget command.
		const auto unusedPlaceholder = ReviewDraft && ReviewDraft->Id.value() > core::maximum_private_identity
			&& core::unused_review_draft(core::revisioned_review_draft{ *ReviewDraft, ReviewDraftRevision, DraftScope })
			&& !PrivateDraftCommandPending;
		if (!Closing && (!ReviewDraft || unusedPlaceholder) && PrivateReview && aIdentity.starts_with(prefix) && SelectedSemanticSelection
			&& draft_on_thread_line(core::review_comment_draft{ .Anchor = SelectedSemanticSelection->PreferredLine }, aIdentity)) {
			const auto anchor = SelectedSemanticSelection->PreferredLine;
			if (PrivateDraftSelectionPending || !PrivateDraftsReady) {
				// Await only this exact selected line; navigation clears the continuation.
				PendingPrivateReply = std::pair{ anchor, aIdentity };
				return;
			}
			const auto owner             = SharedDrafts;
			PrivateDraftSelectionPending = true;
			PrivateReview->select_reply(anchor, std::string{ aIdentity.substr(prefix.size()) }, [this, owner, anchor](auto aCommit) {
				PrivateDraftSelectionPending = false;
				const auto resume            = qScopeGuard([this] { resume_pending_thread_reply(); });
				if (Closing || !owner || owner != SharedDrafts || !aCommit || !owner->adopt(owner->authority(), *aCommit)) {
					return;
				}
				present_saved_drafts();
				if (!SelectedSemanticSelection || SelectedSemanticSelection->PreferredLine != anchor) {
					return;
				}
				const auto accepted = owner->read(DraftConsumer, aCommit->records().back().Draft.Draft.Id);
				if (accepted.Accepted) {
					accept_shared_draft(*accepted.Accepted);
					Window->review_editor()->describe_reply_target(QStringLiteral("Replying in this thread"));
				}
			});
			return;
		}
		if (Closing || Window == nullptr || !SharedDrafts || !aIdentity.starts_with(prefix)
			|| !ReviewDraft.has_value()
			|| ReviewDraft->State != core::review_draft_state::drafting
			|| !draft_on_thread_line(*ReviewDraft, aIdentity)) {
			return;
		}
		auto thread = std::string{ aIdentity.substr(prefix.size()) };
		if (thread.empty()) {
			return;
		}
		// [[BPR-0822]]. Name the nested type: GCC 15 destroys a braced aggregate
		// member twice when a later member initializer throws.
		if (PrivateReview && PrivateDraftsReady) {
			dispatch_review_draft({ .Command = core::review_draft_command{ .Id = ReviewDraft->Id, .Kind = core::review_draft_command_kind::edit, .Body = ReviewDraft->Body }, .ExpectedRevision = ReviewDraftRevision, .ChangeReplyTarget = true, .ReplyToThread = thread, .Scope = DraftScope });
			return;
		}
		auto outcome = SharedDrafts->apply(DraftConsumer, { .Command = core::review_draft_command{ .Id = ReviewDraft->Id, .Kind = core::review_draft_command_kind::edit, .Body = ReviewDraft->Body }, .ExpectedRevision = ReviewDraftRevision, .ChangeReplyTarget = true, .ReplyToThread = thread, .Scope = DraftScope });
		if (outcome.Failure || !outcome.Accepted) {
			return;
		}
		accept_shared_draft(*outcome.Accepted);
		// The label is described once the draft carries the thread, so it
		// never names a reply the draft does not make.
		auto author = QString{ };
		if (auto* const model = Window->conversation_model(); model != nullptr) {
			const auto index = model->index_for_id(aIdentity);
			if (index.isValid()) {
				author = index.data(qt::github_conversation_model::author_login_role).toString();
			}
		}
		if (auto* const editor = Window->review_editor(); editor != nullptr) {
			editor->describe_reply_target(
				author.isEmpty() ? QStringLiteral("Replying in this thread")
								 : QStringLiteral("Replying to %1's thread").arg(author));
		}
	}

	void github_application::implementation::present_inline_publication_failure(application::review_mutation_failure                       aFailure,
																				core::review_publication_outcome                           aOutcome,
																				const std::optional<designated_review_mutation_selection>& aBinding)
	{
		if (!Window || !Window->review_editor()) {
			return;
		}
		if (aOutcome == core::review_publication_outcome::indeterminate && RetryableReviewDraft && ReviewDraft && RetryableReviewDraft->Draft.Id == ReviewDraft->Id) {
			RetryableReviewDraft->Failure = aFailure;
		}
		const auto login  = aBinding ? QString::fromStdString(aBinding->ActingIdentity.Login) : QString{ };
		const auto host   = aBinding ? QString::fromStdString(std::string{ aBinding->Target.Hostname.value() }) : QString{ };
		const auto status = aOutcome == core::review_publication_outcome::indeterminate
			? qt::github_cli_publication_recovery_text(aFailure, login, host)
			: aBinding ? qt::github_cli_publication_failure_text(aFailure, login, host)
					   : QString{ };
		if (status.isEmpty()) {
			Window->review_editor()->present(aFailure);
		} else {
			Window->review_editor()->present_publication_refusal(status);
		}
	}

	/// \brief Applies one draft command, and says once whether it was accepted.
	///
	/// \details [[BPR-1246]]. With private review the command is written
	/// durably first and accepted later, on the private review's answer, so
	/// a caller that acts on the accepted draft passes `aSettled` rather than
	/// reading `ReviewDraft` on return. It is called at most once: with
	/// true after `finish_review_draft_command` accepted the command, with
	/// false when the command was refused or never applied. A window that
	/// closes before the private answer arrives never calls it.
	void github_application::implementation::dispatch_review_draft(core::revisioned_review_draft_command aRevisionedCommand, std::function<void(bool)> aSettled)
	{
		const auto settle = [&aSettled](const bool aAccepted) {
			if (aSettled) {
				std::exchange(aSettled, { })(aAccepted);
			}
		};
		if (Closing || !ReviewDraft.has_value()
			|| ReviewMutationHost == nullptr || Window == nullptr
			|| Window->review_editor() == nullptr) {
			settle(false);
			return;
		}
		if (!SharedDrafts) {
			settle(false);
			return;
		}
		const auto& aCommand     = aRevisionedCommand.Command;
		const auto  publish      = aCommand.Kind == core::review_draft_command_kind::begin_publish;
		const auto  priorOutcome = ReviewDraft->State == core::review_draft_state::publishing
			? core::review_publication_outcome::indeterminate
			: core::review_publication_outcome::not_published;
		// [[BPR-1329]]. A Publish that stops here says why, where the draft
		// is shown; it used to stop without a word.
		const auto refusePublish = [this, publish, priorOutcome](const QString& aReason) {
			if (publish && !Closing && Window && Window->review_editor()) {
				if (priorOutcome == core::review_publication_outcome::indeterminate) {
					present_inline_publication_failure(application::review_mutation_failure::undesignated_target, priorOutcome, InlineReviewBinding);
				} else {
					Window->review_editor()->present_publication_refusal(aReason);
				}
			}
		};
		// [[BPR-1862]]. Nor does a Delete stop without a word.
		const auto discard      = aCommand.Kind == core::review_draft_command_kind::discard;
		const auto refuseDelete = [this, discard](const QString& aReason) {
			if (discard && !Closing && Window) {
				Window->present_draft_delete_refusal(aReason);
			}
		};
		if (publish) {
			const auto admission = InlineReviewBinding ? admit_cli_publication(InlineReviewBinding->Target, InlineReviewBinding->ActingIdentity)
													   : std::expected<void, application::review_mutation_failure>{ std::unexpected(application::review_mutation_failure::undesignated_target) };
			if (!admission) {
				present_inline_publication_failure(admission.error(), priorOutcome, InlineReviewBinding);
				settle(false);
				return;
			}
		}
		if (Closing || !ReviewDraft || !ReviewMutationHost || !SharedDrafts) {
			refusePublish(QStringLiteral("Not published: the pull request was refreshed before this could be sent. Your draft is kept; publish it again."));
			refuseDelete(QStringLiteral("Not deleted: the pull request was refreshed before this comment could be deleted. It is kept; delete it again."));
			settle(false);
			return;
		}
		if (publish && core::held_by_review(*ReviewDraft)) {
			// [[BPR-1345]]. A submitted review holds this draft until its
			// answer is known; nothing publishes it on its own meanwhile.
			refusePublish(QStringLiteral("Not published: this comment is being submitted with your review. Check the review in Discuss."));
			settle(false);
			return;
		}
		if (publish && ReviewDraft->ReplyToThread) {
			// [[BPR-1374]]. A reply goes only to a thread the server still
			// holds; the server's truth is read, never written over.
			const auto decision = core::decide_reply_draft(*ReviewDraft, reply_threads());
			if (decision.Standing != core::reply_draft_standing::none && !decision.Sendable) {
				refusePublish(qt::conversation_cards_view::unsent_reply_notice(decision.Standing) + QStringLiteral(" Your reply was not sent; its words are kept."));
				settle(false);
				return;
			}
		}
		if (publish && ReviewDraft->InReview && !aRevisionedCommand.ChangePending) {
			// [[BPR-1345]]. Add single comment on a pending draft posts it
			// on its own and takes it out of the review, in one revision.
			aRevisionedCommand.ChangePending = true;
			aRevisionedCommand.Pending.reset();
		}
		if (Window->review_editor()->workshop_accepting(aRevisionedCommand)) {
			Window->begin_dif_draft(aRevisionedCommand);
		}
		if (PrivateReview && PrivateDraftsReady) {
			if (aRevisionedCommand.Scope != DraftScope || aRevisionedCommand.ExpectedRevision != ReviewDraftRevision || aCommand.Id != ReviewDraft->Id) {
				Window->review_editor()->present_conflict({ .Accepted = { *ReviewDraft, ReviewDraftRevision, DraftScope }, .Refusal = core::revisioned_draft_refusal::conflict });
				refusePublish(QStringLiteral("Not published: this draft changed before it could be sent. Check it and publish again."));
				refuseDelete(QStringLiteral("Not deleted: this comment changed before it could be deleted. Check it and delete it again."));
				settle(false);
				return;
			}
			if (ReviewDraft->Id.value() > core::maximum_private_identity) {
				if (aCommand.Kind == core::review_draft_command_kind::discard && PrivateDraftCommandPending) {
					refuseDelete(QStringLiteral("Not deleted: your comment is still being saved. Delete it after saving completes."));
					settle(false);
					return;
				}
				if (aCommand.Kind == core::review_draft_command_kind::discard) {
					adopt_review_selection(SelectedSemanticSelection, qt::review_draft_editor_clear_reason::selection_cleared);
					settle(true);
					return;
				}
				if (aCommand.Kind != core::review_draft_command_kind::edit || PrivateDraftCommandPending) {
					settle(false);
					return;
				}
				PrivateDraftCommandPending = true;
				const auto owner           = SharedDrafts;
				const auto placeholder     = core::revisioned_review_draft{ *ReviewDraft, ReviewDraftRevision, DraftScope };
				PrivateReview->materialize_draft(placeholder, aRevisionedCommand, [this, owner, placeholder, aRevisionedCommand, settled = std::move(aSettled)](auto aCommit) {
					PrivateDraftCommandPending = false;
					const auto accepted        = !Closing && owner == SharedDrafts && aCommit && owner->adopt(owner->authority(), *aCommit);
					if (accepted && !aCommit->records().empty()) {
						present_saved_drafts();
						const auto saved = owner->read(DraftConsumer, aCommit->records().front().Draft.Draft.Id);
						if (saved.Accepted) {
							const auto shown = ReviewDraft && ReviewDraft->Id == placeholder.Draft.Id;
							if (shown) {
								ReviewDraft         = saved.Accepted->Draft;
								ReviewDraftRevision = saved.Accepted->Revision;
							}
#if defined(BPR_AGENT_INTEGRATIONS)
							settle_materialized_comment_workshop(placeholder, shown ? saved.Accepted : std::nullopt);
#endif
							Window->review_editor()->acknowledge_materialized(aRevisionedCommand, *saved.Accepted);
							if (shown && !Window->review_editor()->rejected_candidate()) {
								accept_shared_draft(*saved.Accepted);
							}
						}
					} else if (!Closing && owner == SharedDrafts && ReviewDraft && ReviewDraft->Id == placeholder.Draft.Id) {
						Window->review_editor()->present_conflict({ .Accepted = placeholder, .Refusal = core::revisioned_draft_refusal::conflict });
						Window->review_editor()->present_publication_refusal(!aCommit && aCommit.error().Store == application::private_store_failure::quota
																				 ? QStringLiteral("Your text has not been saved: the comment storage limit was reached. Inspect Finish review > Saved comments. Finish or discard existing active comments, or free unused empty drafts, then choose Reapply my text. Your text is kept here.")
																				 : QStringLiteral("Your text has not been saved. It is kept here; choose Reapply my text to retry before leaving this comment."));
					}
#if defined(BPR_AGENT_INTEGRATIONS)
					// Also settle a refused, retired or empty first save.
					settle_materialized_comment_workshop(placeholder, std::nullopt);
#endif
					if (settled) {
						settled(accepted);
					}
				});
				return;
			}
			if (aCommand.Kind == core::review_draft_command_kind::begin_publish && ReviewDraft->State == core::review_draft_state::publishing) {
				auto finished = false;
				if (!PendingReviewMutation && ReviewDraftRevision == aRevisionedCommand.ExpectedRevision) {
					const auto retained = SharedDrafts->read(DraftConsumer, ReviewDraft->Id);
					if (retained.Accepted) {
						finish_review_draft_command(aRevisionedCommand, retained, priorOutcome);
						finished = true;
					}
				}
				settle(finished);
				return;
			}
			if (PrivateDraftCommandPending) {
				refusePublish(QStringLiteral("Not published: your last change to this draft is still being saved. Publish again in a moment."));
				refuseDelete(QStringLiteral("Not deleted: your last change to this comment is still being saved. Delete it again in a moment."));
				settle(false);
				return;
			}
			PrivateDraftCommandPending = true;
			const auto owner           = SharedDrafts;
			PrivateReview->change_draft(aRevisionedCommand, [this, owner, aRevisionedCommand, priorOutcome, refuseDelete, settled = std::move(aSettled)](auto aCommit) {
				PrivateDraftCommandPending = false;
				const auto answer          = [&settled](const bool aAccepted) {
					if (settled) {
						settled(aAccepted);
					}
				};
				if (Closing || owner != SharedDrafts || !aCommit || !owner->adopt(owner->authority(), *aCommit)) {
					if (!Closing && owner == SharedDrafts && aRevisionedCommand.Command.Kind == core::review_draft_command_kind::begin_publish && Window && Window->review_editor()) {
						Window->review_editor()->present_publication_refusal(QStringLiteral("Not published: this draft could not be saved for publishing. Your text is kept; publish it again."));
					}
					if (!Closing && owner == SharedDrafts) {
						refuseDelete(QStringLiteral("Not deleted: the comment could not be removed from storage. It is kept; delete it again."));
					}
					answer(false);
					return;
				}
				present_saved_drafts();
				const auto accepted = owner->read(DraftConsumer, aRevisionedCommand.Command.Id);
				if (accepted.Accepted) {
					finish_review_draft_command(aRevisionedCommand, accepted, priorOutcome);
				}
				answer(accepted.Accepted.has_value() && !accepted.Failure);
			});
			return;
		}
		const auto outcome = SharedDrafts->apply(DraftConsumer, aRevisionedCommand);
		finish_review_draft_command(aRevisionedCommand, outcome, priorOutcome);
		if (outcome.Failure || !outcome.Accepted) {
			refuseDelete(QStringLiteral("Not deleted: the comment could not be deleted. It is kept; delete it again."));
		}
		settle(!outcome.Failure && outcome.Accepted.has_value());
	}

	void github_application::implementation::finish_review_draft_command(const core::revisioned_review_draft_command& aRevisionedCommand, const application::shared_draft_result& aOutcome, core::review_publication_outcome aPriorOutcome)
	{
		const auto& aCommand = aRevisionedCommand.Command;

		if (aOutcome.Failure || !aOutcome.Accepted) {
			Window->cancel_dif_draft(aRevisionedCommand);
			if (aOutcome.Accepted && aOutcome.Refusal == core::revisioned_draft_refusal::conflict) {
				Window->review_editor()->present_conflict({ .Accepted = *aOutcome.Accepted, .Refusal = aOutcome.Refusal });
			}
			return;
		}
		Window->review_editor()->acknowledge(aRevisionedCommand, *aOutcome.Accepted);
		// A durable edit can finish after another line was selected. Keep
		// its acknowledgement, without selecting that earlier draft again.
		if (aCommand.Kind == core::review_draft_command_kind::edit
			&& (!ReviewDraft || aOutcome.Accepted->Scope != DraftScope
				|| ReviewDraft->Id != aOutcome.Accepted->Draft.Id
				|| ReviewDraft->Anchor != aOutcome.Accepted->Draft.Anchor
				|| ReviewDraftRevision > aOutcome.Accepted->Revision)) {
			Window->accept_dif_draft(aRevisionedCommand, *aOutcome.Accepted);
			refresh_agent_private_context_choices();
			return;
		}
		accept_shared_draft(*aOutcome.Accepted);
		if (aRevisionedCommand.ChangeReplyTarget && aRevisionedCommand.ReplyToThread) {
			Window->review_editor()->describe_reply_target(QStringLiteral("Replying in this thread"));
		}
		Window->accept_dif_draft(aRevisionedCommand, *aOutcome.Accepted);
		if (aCommand.Kind == core::review_draft_command_kind::begin_publish) {
			PublishingDraft        = *aOutcome.Accepted;
			PublishingPriorOutcome = aPriorOutcome;
		}

		if (RetryableReviewDraft.has_value()
			&& RetryableReviewDraft->Draft.Id == ReviewDraft->Id) {
			if (ReviewDraft->State == core::review_draft_state::discarded) {
				RetryableReviewDraft.reset();
			} else {
				RetryableReviewDraft->Draft = *ReviewDraft;
			}
		}
		auto* const editor = Window->review_editor();
		editor->present(core::revisioned_review_draft{ *ReviewDraft, ReviewDraftRevision, DraftScope });
		refresh_agent_private_context_choices();
		if (aCommand.Kind == core::review_draft_command_kind::discard) {
			leave_deleted_draft();
			return;
		}
		if (aCommand.Kind != core::review_draft_command_kind::begin_publish) {
			return;
		}
		auto proposal = core::propose_publication(
			*ReviewDraft, core::review_disposition::comment);
		if (proposal && PrivateReview && PrivateDraftsReady && !ReviewDraft->ReplyToThread) {
			const auto bound = PrivateReview->binding();
			if (bound) {
				proposal->Durable = core::durable_review_publication_identity{ bound->Scope.Incarnation.bytes(), ReviewDraftRevision };
			}
		}
		if (!proposal.has_value()) {
			complete_shared_draft(false, { }, aPriorOutcome == core::review_publication_outcome::indeterminate);
			return;
		}
		// [[BPR-1745]]. A reply bound to its outdated thread is
		// published by the thread: its path and the commit it was written
		// on, with no selection. A moved head is acknowledged, as for a
		// resolution: a thread belongs to the pull request, not a commit.
		auto threadReply = std::optional<application::review_mutation_thread_reply_binding>{ };
		if (core::thread_bound_reply(*ReviewDraft)) {
			threadReply = thread_reply_binding_of(*ReviewDraft->ReplyToThread);
			if (!threadReply) {
				complete_shared_draft(false, { }, aPriorOutcome == core::review_publication_outcome::indeterminate);
				editor->present(core::revisioned_review_draft{ *ReviewDraft, ReviewDraftRevision, DraftScope });
				editor->present(application::review_mutation_failure::undesignated_target);
				return;
			}
		}
		if (!threadReply && (!SelectedDiffPublication.has_value() || !SelectedSemanticSelection.has_value())) {
			complete_shared_draft(false, { }, aPriorOutcome == core::review_publication_outcome::indeterminate);
			editor->present(core::revisioned_review_draft{ *ReviewDraft, ReviewDraftRevision, DraftScope });
			refresh_agent_private_context_choices();
			editor->present(
				application::review_mutation_failure::undesignated_target);
			return;
		}
		const auto admission = InlineReviewBinding ? admit_cli_publication(InlineReviewBinding->Target, InlineReviewBinding->ActingIdentity)
												   : std::expected<void, application::review_mutation_failure>{ std::unexpected(application::review_mutation_failure::undesignated_target) };
		if (!admission) {
			complete_shared_draft(false, { }, aPriorOutcome == core::review_publication_outcome::indeterminate);
			editor->present(core::revisioned_review_draft{ *ReviewDraft, ReviewDraftRevision, DraftScope });
			present_inline_publication_failure(admission.error(), aPriorOutcome, InlineReviewBinding);
			return;
		}
		auto request = application::review_mutation_publication_request{ .Proposal = *proposal };
		if (threadReply) {
			request.Acknowledged = true;
			request.ThreadReply  = std::move(threadReply);
		} else {
			request.Selection    = *SelectedSemanticSelection;
			request.SelectedDiff = *SelectedDiffPublication;
		}
		auto grant = review_mutation_gesture_grant(
			request,
			[&request](const core::application_session_id           aSession,
					   const core::request_generation               aGeneration,
					   application::review_mutation_target_binding  aTarget,
					   application::review_mutation_acting_identity aActor) {
				return application::detail::review_mutation_user_authority_issuer::issue(
					aSession, aGeneration, std::move(aTarget), std::move(aActor), request);
			});
		const auto context = ReviewMutationHost->publish(std::move(request), std::move(grant));
		if (context.Request.value() == 0U) {
			complete_shared_draft(false, { }, aPriorOutcome == core::review_publication_outcome::indeterminate);
			editor->present(core::revisioned_review_draft{ *ReviewDraft, ReviewDraftRevision, DraftScope });
			present_inline_publication_failure(application::review_mutation_failure::unavailable, aPriorOutcome, InlineReviewBinding);
			return;
		}
		PendingReviewMutation = context;
		PublishingBinding     = InlineReviewBinding;
		Window->review_editor()->allow_publication_retry(false);
		editor->present_publication_started();
	}

	void github_application::implementation::adopt_review_selection(
		const std::optional<core::semantic_selection>& aSelection,
		const qt::review_draft_editor_clear_reason     aReason)
	{
		Window->set_comment_draft_unavailable({ });
		SelectedSemanticSelection = aSelection;
		PendingPrivateReply.reset();
		ReviewDraft.reset();
		ReviewDraftRevision = 0U;
		publish_preflight_selection();
		if (Window != nullptr && Window->review_editor() != nullptr) {
			auto* const editor = Window->review_editor();
			editor->present(aReason);
			// [[BPR-0810]]. The clear above drops the draft and the file
			// receipt together, but a new line in the same file leaves that
			// file's accepted ledger receipt exactly as it was, and no ledger
			// publication follows to present it again. Presenting it here
			// keeps Accept and Exclude, and every reader of the shown
			// receipt, on the file's real state rather than an empty one.
			if (aSelection.has_value()
				&& aReason == qt::review_draft_editor_clear_reason::selection_cleared
				&& SelectedDiffPublication.has_value()) {
				auto receipt = std::optional<core::file_receipt>{ };
				{
					const auto lock = std::scoped_lock{ ReviewLedgerStateMutex };
					if (ReviewLedgerReceipt.has_value()
						&& ReviewLedgerReceipt->File == receipt_file(*SelectedDiffPublication)) {
						receipt = ReviewLedgerReceipt;
					}
				}
				if (receipt.has_value()) {
					editor->present(*receipt);
				}
			}
			if (aSelection.has_value() && ReviewMutationHost != nullptr) {
				editor->present(ReviewMutationHost->capabilities());
			}
		}
		if (aSelection.has_value()) {
			present_review_draft(*aSelection);
		} else {
			refresh_agent_private_context_choices();
		}
		present_filtered_comment();
	}

	void github_application::implementation::observe_review_selection(
		const std::optional<core::semantic_selection>& aSelection)
	{
		observe_change_navigation(aSelection);
		if (PendingReviewMutation.has_value()) {
			if (!DeferredReviewSelection.has_value()
				&& aSelection == SelectedSemanticSelection) {
				return;
			}
			const auto firstDivergence
				= !DeferredReviewSelection.has_value()
				&& aSelection != SelectedSemanticSelection;
			DeferredReviewSelection.emplace(aSelection);
			if (firstDivergence && ReviewMutationHost != nullptr) {
				ReviewMutationHost->cancel(*PendingReviewMutation);
			}
			return;
		}
		if (aSelection == SelectedSemanticSelection) {
			return;
		}
		adopt_review_selection(
			aSelection,
			qt::review_draft_editor_clear_reason::selection_cleared);
	}

	void github_application::implementation::observe_change_navigation(
		const std::optional<core::semantic_selection>& aSelection)
	{
		if (WorkbenchHost == nullptr || Window == nullptr
			|| !SelectedDiffPublication.has_value()) {
			return;
		}
		const auto* const comparison = Window->comparison_surface();
		const auto&       result     = comparison->current_result();
		const auto&       selected   = *SelectedDiffPublication;
		if (!result || result->Alignment.Key != selected.Key
			|| (aSelection.has_value()
				&& aSelection->Comparison != result->Alignment.Digest)) {
			WorkbenchHost->observe_change_navigation(
				selected.Request, std::nullopt);
			return;
		}

		const auto& groups = result->Alignment.Groups;
		if (groups.size() > std::numeric_limits<std::uint32_t>::max()) {
			WorkbenchHost->observe_change_navigation(
				selected.Request, std::nullopt);
			return;
		}
		auto cursor = core::workbench_change_cursor{
			.ChangeCount = static_cast<std::uint32_t>(groups.size()),
		};
		if (aSelection && !aSelection->Group) {
			const auto index = comparison->current_semantic_index();
			const auto next  = index ? core::navigate_change(*index, aSelection->PreferredLine,
															 core::change_navigation_direction::next, core::change_navigation_policy::stop)
									 : core::change_navigation_result{ };
			if (next.Outcome == core::change_navigation_outcome::invalid_selection) {
				WorkbenchHost->observe_change_navigation(selected.Request, std::nullopt);
				return;
			}
			cursor.AtUnchangedLine   = true;
			cursor.Ordinal           = next.Index.value_or(cursor.ChangeCount);
			cursor.PreviousAvailable = cursor.Ordinal > 0U;
			cursor.NextAvailable     = cursor.Ordinal < cursor.ChangeCount;
		} else if (!aSelection) {
			cursor.PreviousAvailable = !groups.empty();
			cursor.NextAvailable     = !groups.empty();
		} else {
			cursor.Group     = aSelection->Group;
			const auto found = std::ranges::find(
				groups, *aSelection->Group, &core::change_group::Id);
			if (found != groups.end()) {
				cursor.Ordinal = static_cast<std::uint32_t>(
					std::ranges::distance(groups.begin(), found) + 1);
				cursor.PreviousAvailable = cursor.Ordinal > 1U;
				cursor.NextAvailable     = cursor.Ordinal < cursor.ChangeCount;
			}
		}
		WorkbenchHost->observe_change_navigation(selected.Request, cursor);
	}

	void github_application::implementation::present_review_draft(const core::semantic_selection& aSelection)
	{
		observe_private_review();
		if (ReviewMutationHost == nullptr || Window == nullptr
			|| Window->review_editor() == nullptr
			|| WorkbenchHost == nullptr || !SelectedDiffPublication.has_value()
			|| !core::supported_comment_target(aSelection.PreferredLine.Side)
			|| aSelection.Comparison
				!= core::comparison_digest(SelectedDiffPublication->Key)
			|| aSelection.PreferredLine.Comparison != aSelection.Comparison
			|| WorkbenchHost->generation()
				!= SelectedDiffPublication->Request.Context.Generation
			|| WorkbenchHost->selected()
				!= std::optional{ SelectedDiffPublication->Request.File.File }) {
			return;
		}
		// [[BPR-1212]]. A base line takes a draft only once its numbering
		// is known to be GitHub's; the answer presents it again.
		// [[BPR-1570]]. An answer given at once ([[BPR-1560]]'s named merge
		// base) leaves an already placed left side as it was, so nothing
		// presents it again: the draft goes on here.
		if (aSelection.PreferredLine.Side == core::diff_side::left && !base_numbered_as_github()) {
			observe_base_numbering();
			if (!base_numbered_as_github()) {
				return;
			}
		}
		if (resume_reply_at(aSelection.PreferredLine) || resume_comment_at(aSelection.PreferredLine)) {
			return;
		}
		if (ReviewDraft.has_value()
			&& ReviewDraft->Anchor == aSelection.PreferredLine
			&& (ReviewDraft->State == core::review_draft_state::drafting
				|| ReviewDraft->State == core::review_draft_state::publishing)) {
			return;
		}
		if (RetryableReviewDraft.has_value()
			&& exact_retryable_review_draft(
				*RetryableReviewDraft,
				aSelection,
				SelectedDiffPublication->Request.WorkbenchAuthority)) {
			if (!SharedDrafts) {
				return;
			}
			const auto retained = SharedDrafts->read(DraftConsumer, RetryableReviewDraft->Draft.Id);
			if (retained.Failure || !retained.Accepted) {
				return;
			}
			accept_shared_draft(*retained.Accepted);
			refresh_agent_private_context_choices();
			return;
		}
		if (!SharedDrafts) {
			return;
		}
		if (PrivateReview) {
			if (!PrivateDraftsReady || PrivateDraftSelectionPending) {
				return;
			}
			PrivateDraftSelectionPending = true;
			const auto owner             = SharedDrafts;
			const auto anchor            = aSelection.PreferredLine;
			PrivateReview->select_draft(anchor, [this, owner, anchor](auto aCommit) {
				PrivateDraftSelectionPending = false;
				// [[BPR-1745]]. An outdated thread's Reply that waited
				// for this answer is made after it, whatever it was.
				const auto resume = qScopeGuard([this] { resume_pending_thread_reply(); });
				if (Closing || owner != SharedDrafts) {
					return;
				}
				const auto adopted = aCommit && owner->adopt(owner->authority(), *aCommit);
				if (adopted) {
					present_saved_drafts();
				}
				if (!SelectedSemanticSelection || SelectedSemanticSelection->PreferredLine != anchor) {
					if (SelectedSemanticSelection) {
						present_review_draft(*SelectedSemanticSelection);
					}
					return;
				}
				if (!adopted) {
					if (!aCommit && aCommit.error().Store == application::private_store_failure::quota) {
						const auto reason = QStringLiteral("Comment storage is full. Inspect Finish review > Saved comments. Finish or discard existing active comments, or free unused empty drafts, then select the line again. Written comments and pending publications are kept.");
						Window->review_editor()->present_publication_refusal(reason);
						Window->set_comment_draft_unavailable(reason);
						return;
					}
					if (PendingPrivateReply && PendingPrivateReply->first == anchor) {
						const auto reply = std::exchange(PendingPrivateReply, std::nullopt);
						begin_review_reply(reply->second);
						return;
					}
					Window->review_editor()->present(application::review_mutation_failure::unavailable);
					return;
				}
				// Durable storage can contain an earlier empty inline draft and a
				// later accepted composition at this same exact line. Reopen
				// the latest active committed draft without allocating a new ID.
				const auto candidates = aCommit->records();
				const auto found      = std::ranges::find_if(candidates.rbegin(), candidates.rend(), [&](const auto& aRecord) { return aRecord.Draft.Draft.Anchor == anchor && !aRecord.Draft.Draft.ReplyToThread && (aRecord.Draft.Draft.State == core::review_draft_state::drafting || aRecord.Draft.Draft.State == core::review_draft_state::publishing); });
				const auto selected   = found == candidates.rend() ? owner->select(DraftConsumer, anchor) : owner->read(DraftConsumer, found->Draft.Draft.Id);
				if (selected.Accepted) {
					accept_shared_draft(*selected.Accepted);
					refresh_agent_private_context_choices();
					if (PendingPrivateReply && PendingPrivateReply->first == anchor) {
						const auto reply = std::exchange(PendingPrivateReply, std::nullopt);
						begin_review_reply(reply->second);
					}
				}
			});
			return;
		}
		const auto created = SharedDrafts->select(DraftConsumer, aSelection.PreferredLine);
		if (!created.Accepted || (created.Failure && created.Failure != application::shared_draft_failure::conflict)) {
			return;
		}
		accept_shared_draft(*created.Accepted);
		refresh_agent_private_context_choices();
	}

	void github_application::implementation::finish_review_terminal(const application::review_mutation_terminal& aTerminal, core::review_publication_outcome aPublicationOutcome)
	{

		if (Closing
			|| PendingReviewMutation != std::optional{ aTerminal.Context }) {
			return;
		}
		PendingReviewMutation.reset();
		// [[BPR-1475]]. A comment or reply on GitHub: the pull request is
		// read again, whatever the editor shows now. An unknown outcome is
		// not reread. A reread now holds the draft again for its retry
		// ([[BPR-1477]]), but one the window started itself would land
		// while the reviewer presses Retry, and the composition it
		// replaces drops that retry's answer.
		if (aTerminal.Result.has_value()) {
			reread_after_publication();
		}
		const auto failedBinding = std::exchange(PublishingBinding, std::nullopt);
		refresh_review_projection(aTerminal);
		// [[BPR-1736]]. And whatever follows: the answer may have taught it.
		present_review_comment_holds();
		const auto deferred = std::move(DeferredReviewSelection);
		DeferredReviewSelection.reset();
		if (!ReviewDraft.has_value()) {
			if (deferred.has_value()) {
				adopt_review_selection(
					*deferred,
					qt::review_draft_editor_clear_reason::selection_cleared);
			}
			return;
		}
		complete_shared_draft(aTerminal.Result.has_value(), { }, aPublicationOutcome == core::review_publication_outcome::indeterminate);
		const auto sameRetry = RetryableReviewDraft.has_value()
			&& RetryableReviewDraft->Draft.Id == ReviewDraft->Id;
		if (aTerminal.Result.has_value()) {
			if (sameRetry) {
				RetryableReviewDraft.reset();
			}
		} else if (
			aPublicationOutcome == core::review_publication_outcome::indeterminate
			&& SelectedDiffPublication.has_value()
			&& ReviewDraft->Anchor.Comparison
				== core::comparison_digest(SelectedDiffPublication->Key)) {
			RetryableReviewDraft = retryable_review_draft_record{
				.Draft     = *ReviewDraft,
				.Authority = SelectedDiffPublication->Request.WorkbenchAuthority,
			};
		} else if (sameRetry) {
			RetryableReviewDraft.reset();
		}
		refresh_agent_private_context_choices();
		if (deferred.has_value()) {
			adopt_review_selection(
				*deferred,
				qt::review_draft_editor_clear_reason::selection_cleared);
			return;
		}
		if (Window == nullptr || Window->review_editor() == nullptr) {
			return;
		}
		auto* const editor = Window->review_editor();
		editor->present(core::revisioned_review_draft{ *ReviewDraft, ReviewDraftRevision, DraftScope });
		editor->allow_publication_retry(PrivateReview && !aTerminal.Result && ReviewDraft->State == core::review_draft_state::publishing && !core::held_by_review(*ReviewDraft));
		if (aTerminal.Result.has_value()) {
			editor->present(*aTerminal.Result, ReviewReconciliation);
			if (InlineReviewBinding) {
				PendingPublishedConversation = published_conversation_selection{
					.Target = InlineReviewBinding->Target, .Actor = InlineReviewBinding->ActingIdentity, .Anchor = ReviewDraft->Anchor, .Comment = aTerminal.Result->ServerCommentId
				};
				// The accepted server snapshot supplies the thread identity,
				// raw body and placement. A receipt never fabricates a post:
				// the reread above brings it ([[BPR-1475]]).
			}
		} else {
			present_inline_publication_failure(aTerminal.Result.error(), aPublicationOutcome, failedBinding);
		}
		// [[BPR-1736]]. GitHub's answer may have taught the learned fact:
		// the notice says what is known now.
		present_filtered_comment();
	}

	void github_application::implementation::retire_review_ledger()
	{
		if (PrivateReview) {
			PrivateReview->invalidate();
		}
		PrivateDraftsReady = false;
		if (PrivateNoteScope < std::numeric_limits<std::uint64_t>::max()) {
			++PrivateNoteScope;
		}
		AcceptedPrivateNotes.clear();
		if (Window != nullptr) {
			Window->private_notes()->present({ .Scope = PrivateNoteScope });
		}
		ReadStateCache.reset();
		NotePaths.reset();
		DefectThreadNames.reset();
		OverviewReceiptRevision.reset();
		OverviewReceipts.clear();
		DurableDefects.clear();
		// [[BPR-1211]]. Queued ledger work is answered cancelled and a
		// running store operation publishes nothing; the store closes on
		// its own lane once that operation returns.
		++ReviewLedgerGeneration;
		ConversationReceiptsLoading = false;
		OverviewReceiptsLoading     = false;
		OverviewReceiptsFailed      = false;
		PrivateRecordsFailed        = false;
		PendingDefectWrites.clear();
		ReviewLedgerHost.reset();
#if defined(BPR_BUILD_TESTING)
		if (ReviewLedgerDecisionGate) {
			ReviewLedgerDecisionGate->release();
		}
#endif
		{
			const auto lock = std::scoped_lock{ ReviewLedgerStateMutex };
			ReviewLedgerAuthority.reset();
			ReviewLedgerReceipt.reset();
			ReviewLedgerAvailable = false;
		}
		if (Window != nullptr) {
			Window->clear_review_ledger();
			Window->clear_defects();
			Window->agent_panel()->offer_private_context({ }, std::nullopt);
		}
	}

	void github_application::implementation::observe_private_review()
	{
		if (!PrivateReview || !EpochHost || !Window) {
			return;
		}
		const auto& state = EpochHost->state();
		if (!state.Authority || !state.AcceptedRevision || !state.AcceptedComparison) {
			return;
		}
		const auto& authority  = *state.Authority;
		const auto& revision   = *state.AcceptedRevision;
		const auto& comparison = *state.AcceptedComparison;
		auto        binding    = core::private_review_binding{ .Access = LastAcceptedHandoff ? LastAcceptedHandoff->Mode : core::github_access_mode::authenticated, .HostAuthority = authority.HostAuthority, .Revision = { authority.Host, authority.Repository, authority.PullRequest, revision.Base, revision.Head, comparison.Left.Identity.Epoch, comparison.Right.Identity.Epoch }, .LeftAnchor = comparison.Left.Identity, .RightAnchor = comparison.Right.Identity };
		const auto  index      = Window->comparison_surface()->current_semantic_index();
		if (index) {
			binding.Comparison = index->diff().Alignment.Digest;
		}
		PrivateReview->observe(binding, index, SelectedSemanticSelection ? std::optional{ SelectedSemanticSelection->PreferredLine } : std::nullopt);
		if (!PrivateDraftsReady && SharedDrafts && PrivateReview->binding()) {
			PrivateDraftsReady = true;
			const auto owner   = SharedDrafts;
			PrivateReview->restore([this, owner](auto aCommit) {
				if (Closing || owner != SharedDrafts) {
					return;
				}
				if (aCommit && owner->adopt(owner->authority(), *aCommit)) {
					present_saved_drafts();
					if (ReviewSubmission) {
						ReviewSubmission->resolve_held(std::vector<std::string>{ aCommit->held_reviews().begin(), aCommit->held_reviews().end() });
					}
					if (SelectedSemanticSelection) {
						present_review_draft(*SelectedSemanticSelection);
					}
					resume_reply();
					flush_unsent_reply_commands();
					resume_pending_thread_reply();
				} else {
					PrivateDraftsReady = false;
				}
			});
		}
	}

	void github_application::implementation::adopt_private_drafts(const application::durable_draft_commit& aCommit, bool aOpen)
	{
		if (Closing || !SharedDrafts || !SharedDrafts->adopt(SharedDrafts->authority(), aCommit)) {
			return;
		}
		present_saved_drafts();
		if (ReviewDraft && std::ranges::find(aCommit.removed_unused(), ReviewDraft->Id) != aCommit.removed_unused().end()) {
			adopt_review_selection(SelectedSemanticSelection, qt::review_draft_editor_clear_reason::selection_cleared);
		} else if (!aCommit.removed_unused().empty() && SelectedSemanticSelection) {
			Window->set_comment_draft_unavailable({ });
			present_review_draft(*SelectedSemanticSelection);
		}
		if (aOpen && !aCommit.records().empty()) {
			const auto& value = aCommit.records().front().Draft;
			const auto  read  = SharedDrafts->read(DraftConsumer, value.Draft.Id);
			if (read.Accepted && SelectedSemanticSelection && value.Draft.Anchor == SelectedSemanticSelection->PreferredLine) {
				accept_shared_draft(*read.Accepted);
				Window->open_review_composition(true);
			}
		}
	}

	auto github_application::implementation::insights_capture(const application::accepted_pull_request_handoff& aHandoff) const -> application::insights_review_capture
	{
		application::insights_review_capture result;
		result.Observer             = Profile->InsightsRecorder;
		result.Context.Host         = std::string{ aHandoff.Hostname.value() };
		result.Context.Repository   = aHandoff.Locator.Owner + "/" + aHandoff.Locator.Name;
		result.Context.PullRequest  = aHandoff.Number.value();
		result.Context.FilesChanged = aHandoff.Shape ? aHandoff.Shape->FileCount : 0;
		return result;
	}

	void github_application::implementation::compose_review_ledger(
		const application::accepted_pull_request_handoff& aHandoff)
	{
		retire_review_ledger();
		PrivateDraftsReady = false;
		auto stats         = insights_capture(aHandoff);
		stats.record(core::insights_review_kind::opened, "opened");
		auto fileStats = [stats, head = std::string{ core::canonical_hex(aHandoff.Head).view() }, files = WindowRead->accepted_snapshot() ? WindowRead->accepted_snapshot()->Files : std::vector<application::github_file_record>{ }](const core::file_receipt& aReceipt, const core::file_receipt_command_kind aKind) mutable {
			const auto file = std::ranges::find_if(files, [&](const auto& aFile) { return core::file_identity_of(aFile.Path) == aReceipt.File; });
			if (file == files.end()) {
				return;
			}
			stats.Context.Head = head;
			if (aKind == core::file_receipt_command_kind::mark_seen || aKind == core::file_receipt_command_kind::mark_read) {
				stats.record(core::insights_review_kind::seen, file->Path);
			}
			if (aKind == core::file_receipt_command_kind::accept && aReceipt.Accepted) {
				stats.record(core::insights_review_kind::accepted, file->Path, file->Additions + file->Deletions);
			}
		};

		// [[BPR-1211]]. Opening the store, like every later read and write
		// of it, happens on the ledger's effect lane. Until it answers the
		// ledger is not yet available, and nothing waits for it here.
		auto open = [scope = adapters::sqlite::review_ledger_configuration{
						 .StorageRoot = ApplicationDataRoot,
						 .Host        = std::string{ aHandoff.Hostname.value() },
						 .Account     = std::to_string(aHandoff.Account.value()),
						 .Repository  = aHandoff.Locator.Owner + "/" + aHandoff.Locator.Name,
						 .PullRequest = static_cast<std::int64_t>(aHandoff.Number.value()),
					 },
					 observer = Profile->ledger_observer(workbench_authority(aHandoff), DraftWindow)]() -> std::expected<std::unique_ptr<application::review_ledger_store_port>, application::ledger_store_failure> {
			auto opened = adapters::sqlite::make_review_ledger_store(scope);
			if (!opened.has_value()) {
				return std::unexpected(opened.error());
			}
			return std::make_unique<application::observing_review_ledger_store>(std::move(*opened), observer);
		};
		// The private review store opens the same ledger file and refuses a
		// file that changes while it validates it, so it opens after this
		// ledger's own opening has answered, as it did when that was synchronous.
		auto privateScope = adapters::sqlite::private_review_configuration{ .Ledger = { .StorageRoot = ApplicationDataRoot, .Host = std::string{ aHandoff.Hostname.value() }, .Account = std::to_string(aHandoff.Account.value()), .Repository = aHandoff.Locator.Owner + "/" + aHandoff.Locator.Name, .PullRequest = static_cast<std::int64_t>(aHandoff.Number.value()) }, .Profile = DraftWindow.Profile, .Host = aHandoff.Host, .Account = aHandoff.Account, .Repository = aHandoff.Repository, .PullRequest = aHandoff.PullRequest };
		++ReviewLedgerGeneration;
#if defined(BPR_BUILD_TESTING)
		if (ReviewLedgerDecisionGate == nullptr) {
			Profile->ReviewLedgerDecisionGates.push_back(std::make_unique<gated_review_mutation_decision_executor>(DecisionLane));
			ReviewLedgerDecisionGate = Profile->ReviewLedgerDecisionGates.back().get();
		}
		auto& ledgerDecisions = *ReviewLedgerDecisionGate;
#else
		auto& ledgerDecisions = DecisionLane;
#endif
		ReviewLedgerHost = std::make_unique<application::review_ledger_application_host>(
			std::move(open), *this, ReviewLedgerStoreEffects, ledgerDecisions,
			ledger_answer([this, privateReview = std::move(privateScope)](const std::expected<void, application::ledger_store_failure>& aOpened) mutable {
				open_private_review(std::move(privateReview));
				review_ledger_opened(aOpened.has_value());
			}),
			application::maximum_outstanding_ledger_operations, stats, std::move(fileStats));
		{
			const auto lock       = std::scoped_lock{ ReviewLedgerStateMutex };
			ReviewLedgerAuthority = workbench_authority(aHandoff);
			ReviewLedgerAvailable = false;
		}
	}

	void github_application::implementation::open_private_review(adapters::sqlite::private_review_configuration aConfiguration)
	{
		if (!PrivateReview) {
			return;
		}
		PrivateReview->open(std::move(aConfiguration), [this] {
			observe_private_review();
			if (ReviewMutationHost) {
				ReviewMutationHost->repair_pending_publications();
			}
		});
	}

	void github_application::implementation::refresh_shared_ledger(const application::workbench_pull_request_authority& aSource, ledger_changes aChanges)
	{
		if (Closing || !Profile->current_window(DraftWindow) || !ReviewLedgerHost) {
			return;
		}
		{
			const auto lock = std::scoped_lock{ ReviewLedgerStateMutex };
			if (!ReviewLedgerAvailable || !ReviewLedgerAuthority) {
				return;
			}
			const auto& target = *ReviewLedgerAuthority;
			// Sessions, provider snapshots and PR request generations belong to
			// individual views. All durable and account authority must match.
			if (target.Profile != aSource.Profile || target.ProfileRevision != aSource.ProfileRevision
				|| target.Host != aSource.Host || target.HostAuthority != aSource.HostAuthority
				|| target.Account != aSource.Account || target.Viewer != aSource.Viewer
				|| target.Repository != aSource.Repository || target.PullRequest != aSource.PullRequest
				|| target.Number != aSource.Number || target.IdentityGeneration != aSource.IdentityGeneration) {
				return;
			}
		}
		// [[BPR-0972]]. This window's own host committed its receipt and
		// conversation writes and published them as it did; rereading them
		// here found the same durable state. Private records are also
		// written by agent finding delivery through this store, so those
		// are reloaded whatever their origin.
		const auto own = aChanges.Origin == std::optional{ DraftWindow };
		if (aChanges.AllReceipts) {
			ReviewLedgerHost->invalidate_all_receipts();
		} else if (aChanges.Receipts && !own) {
			ReviewLedgerHost->invalidate_receipts(aChanges.ReceiptFiles);
		}
		// [[BPR-1211]]. Every reload below reads the store off the
		// decision lane and republishes when it answers.
		auto overviewRefreshing = false;
		if (aChanges.Receipts && !own && SelectedDiffPublication && WorkbenchHost
			&& WorkbenchHost->generation() == SelectedDiffPublication->Request.Context.Generation
			&& WorkbenchHost->selected() == std::optional{ SelectedDiffPublication->Request.File.File }
			&& ReviewLedgerHost->retained().File == SelectedDiffPublication->Request.File.File
			&& ReviewLedgerHost->retained().Material == core::file_material(SelectedDiffPublication->Key)) {
			// A reload publishes, and publishing refreshes the overview. A
			// failed reload retains the last accepted receipt and order: it
			// does not mean the selected comparison material is absent.
			overviewRefreshing = true;
			ReviewLedgerHost->refresh_receipt(ledger_answer([this](const std::expected<core::file_receipt, application::ledger_store_failure>& aRefreshed) {
				if (!aRefreshed) {
					refresh_overview_receipts();
				}
			}));
		} else if (aChanges.Receipts && !own && WorkbenchHost && ReviewLedgerAuthority) {
			// Without exact material every retained file is conservative again;
			// restoring withheld rows needs no new catalogue observation.
			WorkbenchHost->retain_actionability(*ReviewLedgerAuthority, ReviewLedgerHost->retained().File, true);
		}

		if (aChanges.Conversations && !own) {
			ReviewLedgerHost->invalidate_conversation_receipts();
			publish_read_state();
		}
		if (aChanges.PrivateRecords) {
			// A sibling may be reading a different file or epoch. Reload
			// accepted records without correlating or persisting that view;
			// the reload republishes the overview. [[BPR-1238]]: this
			// window's own write reloads them too, and they are correlated
			// again with its comparison, as opening does. Adopting the durable
			// records as they are would drop an availability this window
			// correlated but could not persist, and its next defect gesture
			// would write that stale availability back.
			reload_private_records(own);
			return;
		}
		// Conversation read state is not an overview input; changed
		// receipts move only their own rows.
		if (!overviewRefreshing && aChanges.Receipts) {
			refresh_overview_receipts();
		}
	}

	auto github_application::implementation::matches_review_ledger(
		const application::pull_request_authority_invalidation& aInvalidation) const
		-> bool
	{
		const auto lock = std::scoped_lock{ ReviewLedgerStateMutex };
		if (!ReviewLedgerAuthority.has_value()) {
			return false;
		}
		const auto& authority = *ReviewLedgerAuthority;
		return authority.ApplicationSession == aInvalidation.ApplicationSession
			&& authority.Profile == aInvalidation.Profile
			&& authority.ProfileRevision == aInvalidation.ProfileRevision
			&& authority.Host == aInvalidation.Host
			&& authority.HostAuthority == aInvalidation.HostAuthority
			&& authority.Account == aInvalidation.Account
			&& authority.Viewer == aInvalidation.Viewer
			&& authority.Repository == aInvalidation.Repository
			&& authority.PullRequest == aInvalidation.PullRequest
			&& authority.IdentityGeneration
			== aInvalidation.IdentityGeneration;
	}

	/// \brief Records Seen only while the successfully presented file is still current.
	void github_application::implementation::mark_presented_file_seen(
		const application::workbench_selected_diff_publication& aPublication,
		const core::file_receipt&                               aReceipt)
	{
		const auto* const surface = Window == nullptr ? nullptr : Window->comparison_surface();
		if (Closing || Window == nullptr || ReviewLedgerHost == nullptr || WorkbenchHost == nullptr
			|| Window->showing_home() || Window->current_modality() != qt::review_modality::review
			|| WorkbenchHost->state().Modality != core::workbench_modality::review
			|| TabLoad || TabDriver || PendingRestoration || ReadingRestoration
			|| AutoSeenPresentedRequest == std::optional{ aPublication.Request }
			|| TourSourceRequest == std::optional{ aPublication.Request }
			|| !SelectedDiffPublication || SelectedDiffPublication->Request != aPublication.Request
			|| SelectedDiffRequest != std::optional{ aPublication.Request }
			|| SelectedDiffPublication->Key != aPublication.Key
			|| EpochHost == nullptr || EpochHost->context(aPublication.Request.WorkbenchAuthority) != std::optional{ aPublication.Request.Diff }
			|| WorkbenchHost->generation() != aPublication.Request.Context.Generation
			|| WorkbenchHost->selected() != std::optional{ aPublication.Request.File.File }
			|| surface == nullptr || !surface->isVisibleTo(Window.get()) || !surface->current_result()
			|| !surface->current_context() || surface->current_context()->Key != aPublication.Key
			|| surface->current_result()->Alignment.Key != aPublication.Key
			|| aReceipt.File != receipt_file(aPublication)
			|| aReceipt.Material != core::file_material(aPublication.Key)) {
			return;
		}
		{
			const auto lock = std::scoped_lock{ ReviewLedgerStateMutex };
			if (!ReviewLedgerAvailable || ReviewLedgerAuthority != std::optional{ aPublication.Request.WorkbenchAuthority }) {
				return;
			}
		}
		AutoSeenPresentedRequest = aPublication.Request;
		const auto command       = core::file_receipt_command{
			.File     = aReceipt.File,
			.Material = aReceipt.Material,
			.Kind     = core::file_receipt_command_kind::mark_seen,
		};
		// The core absorbs Seen after Read and preserves Accept and Exclude.
		if (core::apply(aReceipt, command).Changed) {
			dispatch_receipt(command);
		}
	}

	void github_application::implementation::observe_selected_material(
		const application::diff_publication& aPublication)
	{
		if (Closing || ReviewLedgerHost == nullptr
			|| !SelectedDiffPublication.has_value()
			|| aPublication.Result == nullptr
			|| aPublication.Context.Key != SelectedDiffPublication->Key
			|| aPublication.Result->Alignment.Key
				!= SelectedDiffPublication->Key
			|| WorkbenchHost == nullptr
			|| WorkbenchHost->generation()
				!= SelectedDiffPublication->Request.Context.Generation
			|| WorkbenchHost->selected()
				!= std::optional{
					SelectedDiffPublication->Request.File.File }) {
			return;
		}
		{
			const auto lock = std::scoped_lock{ ReviewLedgerStateMutex };
			if (!ReviewLedgerAuthority.has_value()
				|| *ReviewLedgerAuthority
					!= SelectedDiffPublication->Request.WorkbenchAuthority) {
				return;
			}
		}
		// [[BPR-1331]]. The file's material, not its comparison: the
		// comparison names this epoch, so a push would reopen a file
		// whose bytes did not change.
		const auto material = core::file_material(
			SelectedDiffPublication->Key);
		if (!material.valid()) {
			return;
		}
		// [[BPR-1211]]. The receipt is read, and a carry committed, off the
		// decision lane; the ledger publishes it once durable, and the
		// draft is presented against it then.
		ReviewLedgerHost->observe_material(
			core::file_material_observation{
				.File     = receipt_file(*SelectedDiffPublication),
				.Material = material,
			},
			ledger_answer([this, presented = *SelectedDiffPublication](const std::expected<core::file_receipt, application::ledger_store_failure>& aReceipt) {
				if (aReceipt) {
					mark_presented_file_seen(presented, *aReceipt);
				}
				if (SelectedSemanticSelection.has_value()) {
					present_review_draft(*SelectedSemanticSelection);
				}
			}));
		if (SelectedSemanticSelection.has_value()) {
			present_review_draft(*SelectedSemanticSelection);
		}
	}
}
