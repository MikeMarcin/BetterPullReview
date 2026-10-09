#include "bpr/core/revisioned_review_draft.hpp"
#include <algorithm>
#include <limits>
namespace bpr::core {
	namespace {
		auto valid_link(const review_composition_link& aLink, std::string_view aBody) -> bool
		{
			return valid_private_scope(aLink.Scope) && valid_private_id(aLink.Composition)
				&& valid_private_id(aLink.Revision) && aLink.BodyDigest == private_digest(aBody);
		}
		auto extends_link(const review_composition_link& aPrior, const review_composition_link& aNext) -> bool
		{
			return aPrior.Scope == aNext.Scope && aPrior.Composition == aNext.Composition
				&& (aNext.Revision > aPrior.Revision || aNext == aPrior);
		}
		/// [[BPR-1345]]. Applies the command's pending mark to `aNext`, the
		/// draft after the command, or says why it may not.
		/// \details A submission key is given and taken only by the review
		/// that holds the draft: given by its begin_publish to a mark that
		/// already names the same file and head, taken by its fail_publish.
		/// A draft that review holds is released by nothing else, and a
		/// pending draft is published on its own only by a begin_publish that
		/// takes it out of the review in the same revision.
		auto pending_mark_change(const review_comment_draft& aBefore, const revisioned_review_draft_command& aCommand, review_comment_draft& aNext) -> review_draft_rejection
		{
			using rejection = review_draft_rejection;
			const auto kind = aCommand.Command.Kind;
			const auto held = aBefore.InReview.has_value() && !aBefore.InReview->Submission.empty();
			if (!aCommand.ChangePending) {
				const auto releases  = held && kind == review_draft_command_kind::fail_publish;
				const auto unmarking = aBefore.InReview.has_value() && kind == review_draft_command_kind::begin_publish;
				return releases || unmarking ? rejection::invalid_review_mark : rejection::none;
			}
			const auto& mark = aCommand.Pending;
			if (mark && (!valid(*mark) || aNext.ReplyToThread || !supported_comment_target(aNext.Anchor.Side))) {
				return rejection::invalid_review_mark;
			}
			const auto sameFile = [&aBefore](const review_pending_mark& aMark) {
				return aBefore.InReview && aBefore.InReview->Path == aMark.Path && aBefore.InReview->Head == aMark.Head;
			};
			switch (kind) {
			case review_draft_command_kind::edit:
				// Start a review sets a mark with no key; Discard review clears
				// one. Neither touches a draft a submission holds, and a review
				// takes no comment without words.
				if (held || (mark && !mark->Submission.empty())) {
					return rejection::invalid_review_mark;
				}
				if (mark && !aBefore.InReview && std::ranges::all_of(aNext.Body, [](const char aValue) { return aValue == ' ' || aValue == '\t' || aValue == '\r' || aValue == '\n' || aValue == '\v' || aValue == '\f'; })) {
					return rejection::empty_body;
				}
				break;
			case review_draft_command_kind::begin_publish:
				// Add single comment takes the draft out of the review; a
				// review's Submit gives its existing mark the submission key.
				if (held || (mark && (mark->Submission.empty() || !sameFile(*mark)))) {
					return rejection::invalid_review_mark;
				}
				break;
			case review_draft_command_kind::fail_publish:
				// That review, definitely not published: the draft stays
				// pending, without the key.
				if (!held || !mark || !mark->Submission.empty() || !sameFile(*mark)) {
					return rejection::invalid_review_mark;
				}
				break;
			case review_draft_command_kind::discard:
			case review_draft_command_kind::complete_publish:
				return rejection::invalid_review_mark;
			}
			aNext.InReview = mark;
			return rejection::none;
		}
	}

	auto promote_private_text(const std::string_view                        aText,
							  const std::optional<revisioned_review_draft>& aTarget)
		-> std::optional<revisioned_review_draft_command>
	{
		if (!aTarget || aText.size() > maximum_shared_draft_body_bytes
			|| !std::ranges::any_of(aText, [](const char aValue) { return aValue != ' ' && aValue != '\t' && aValue != '\r' && aValue != '\n' && aValue != '\v' && aValue != '\f'; })
			|| aTarget->Draft.State != review_draft_state::drafting) {
			return std::nullopt;
		}
		const auto command = revisioned_review_draft_command{
			.Command          = { .Id = aTarget->Draft.Id, .Kind = review_draft_command_kind::edit, .Body = std::string{ aText } },
			.ExpectedRevision = aTarget->Revision,
			.Scope            = aTarget->Scope
		};
		if (apply(*aTarget, command).Refusal != revisioned_draft_refusal::none) {
			return std::nullopt;
		}
		return command;
	}
	auto apply(const revisioned_review_draft& aDraft, const revisioned_review_draft_command& aCommand) -> revisioned_draft_outcome
	{
		auto result = revisioned_draft_outcome{ aDraft };
		if (aDraft.Scope.value() == 0 || aCommand.Scope != aDraft.Scope || aDraft.Draft.Id.value() == 0 || aDraft.Revision == 0 || aCommand.Command.Id != aDraft.Draft.Id) {
			result.Refusal = revisioned_draft_refusal::invalid_identity;
			return result;
		}
		if (aCommand.ExpectedRevision != aDraft.Revision) {
			result.Refusal = revisioned_draft_refusal::conflict;
			return result;
		}
		if (aCommand.Command.Body.size() > maximum_shared_draft_body_bytes || (aCommand.ReplyToThread && (aCommand.ReplyToThread->empty() || aCommand.ReplyToThread->size() > maximum_shared_draft_reply_bytes))) {
			result.Refusal = revisioned_draft_refusal::bounds;
			return result;
		}
		const auto pendingKind = aCommand.Command.Kind == review_draft_command_kind::edit || aCommand.Command.Kind == review_draft_command_kind::begin_publish
			|| aCommand.Command.Kind == review_draft_command_kind::fail_publish;
		if (aCommand.Command.Kind > review_draft_command_kind::fail_publish || (aCommand.ChangeReplyTarget && aCommand.Command.Kind != review_draft_command_kind::edit)
			|| (aCommand.ChangeRange && (aCommand.Command.Kind != review_draft_command_kind::edit || aCommand.ChangeReplyTarget)) || (!aCommand.ChangeRange && aCommand.StartLine)
			|| (aCommand.ChangePending && (!pendingKind || aCommand.ChangeReplyTarget || aCommand.ChangeRange)) || (!aCommand.ChangePending && aCommand.Pending)) {
			result.Refusal = revisioned_draft_refusal::invalid_command;
			return result;
		}
		if (aDraft.Composition && !valid_link(*aDraft.Composition, aDraft.Draft.Body)) {
			result.Refusal = revisioned_draft_refusal::invalid_identity;
			return result;
		}
		if (aCommand.Composition && (aCommand.Command.Kind != review_draft_command_kind::edit || aCommand.ChangeReplyTarget || !valid_link(*aCommand.Composition, aCommand.Command.Body) || (aDraft.Composition && !extends_link(*aDraft.Composition, *aCommand.Composition)))) {
			result.Refusal = revisioned_draft_refusal::invalid_command;
			return result;
		}

		if (aDraft.Composition && aCommand.Command.Kind == review_draft_command_kind::edit
			&& aCommand.Command.Body != aDraft.Draft.Body && !aCommand.ChangeReplyTarget && !aCommand.Composition) {
			result.Refusal = revisioned_draft_refusal::invalid_command;
			return result;
		}
		auto command = aCommand.Command;
		if (aCommand.ChangeReplyTarget) {
			command.Body = aDraft.Draft.Body;
		}
		const auto applied = apply(aDraft.Draft, command);
		if (applied.Rejection != review_draft_rejection::none) {
			result.Refusal = revisioned_draft_refusal::policy;
			result.Policy  = applied.Rejection;
			return result;
		}
		auto next = applied.Draft;
		if (aCommand.ChangeReplyTarget) {
			next.ReplyToThread = aCommand.ReplyToThread;
			// [[BPR-1212]]. A reply is anchored by its thread, which has no
			// range of its own: answering a thread keeps the text and the
			// anchor and drops the first line.
			if (next.ReplyToThread) {
				next.StartLine.reset();
			}
		}
		if (aCommand.ChangeRange) {
			next.StartLine = aCommand.StartLine;
			if (!valid_comment_range(next)) {
				result.Refusal = revisioned_draft_refusal::policy;
				result.Policy  = review_draft_rejection::invalid_range;
				return result;
			}
		}
		// [[BPR-1345]]. A reply is anchored by its thread and is never
		// pending in a review.
		if (next.ReplyToThread) {
			next.InReview.reset();
		}
		if (const auto refusal = pending_mark_change(aDraft.Draft, aCommand, next); refusal != review_draft_rejection::none) {
			result.Refusal = revisioned_draft_refusal::policy;
			result.Policy  = refusal;
			return result;
		}
		const auto composition = aCommand.Composition ? aCommand.Composition : aDraft.Composition;
		if (next == aDraft.Draft && composition == aDraft.Composition) {
			return result;
		}
		if (aDraft.Revision == std::numeric_limits<std::uint64_t>::max()) {
			result.Refusal = revisioned_draft_refusal::exhausted;
			return result;
		}
		result.Accepted = { std::move(next), aDraft.Revision + 1, aDraft.Scope, composition };
		result.Changed  = true;
		return result;
	}
}
