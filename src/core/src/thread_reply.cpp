#include "bpr/core/thread_reply.hpp"

#include <algorithm>
#include <iterator>

namespace bpr::core {
	auto decide_thread_reply(const thread_reply_facts& aFacts) noexcept -> thread_reply_decision
	{
		const auto refuse = [](const thread_reply_refusal aRefusal) {
			return thread_reply_decision{ .Route = thread_reply_route::refused, .Refusal = aRefusal };
		};
		if (!aFacts.ReviewThread) {
			return refuse(thread_reply_refusal::not_a_review_thread);
		}
		if (aFacts.AnonymousRead) {
			return refuse(thread_reply_refusal::anonymous_read);
		}
		// [[BPR-1746]]. GitHub's `viewerCanReply` already weighs the
		// lock: on a locked pull request it is false for a reader and true for
		// a collaborator, who can still reply (BPR-Test#135, 2026-10-23). The
		// lock only names why GitHub refuses; on its own it refuses nothing.
		if (aFacts.ViewerCanReply == std::optional{ false }) {
			return refuse(aFacts.Locked == std::optional{ true } ? thread_reply_refusal::locked : thread_reply_refusal::viewer_cannot_reply);
		}
		if (aFacts.Outdated) {
			return { .Route = thread_reply_route::on_thread, .Refusal = thread_reply_refusal::none, .OutdatedNote = true };
		}
		return { .Route = thread_reply_route::on_line, .Refusal = thread_reply_refusal::none };
	}

	auto reopen_thread_reply(const std::span<const review_comment_draft> aDrafts, const std::string_view aThread) -> std::optional<review_draft_id>
	{
		// The latest, as `reopen_draft_at` picks: a store lists drafts in the
		// order they were made.
		const auto latest = std::find_if(aDrafts.rbegin(), aDrafts.rend(), [aThread](const review_comment_draft& aDraft) {
			return thread_bound_reply(aDraft) && *aDraft.ReplyToThread == aThread
				&& (aDraft.State == review_draft_state::drafting || aDraft.State == review_draft_state::publishing);
		});
		if (aThread.empty() || latest == aDrafts.rend()) {
			return std::nullopt;
		}
		return latest->Id;
	}

	auto name(const thread_reply_route aRoute) noexcept -> std::string_view
	{
		switch (aRoute) {
		case thread_reply_route::on_line:   return "on_line";
		case thread_reply_route::on_thread: return "on_thread";
		case thread_reply_route::refused:   return "refused";
		}
		return "refused";
	}

	auto name(const thread_reply_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case thread_reply_refusal::none:                return "none";
		case thread_reply_refusal::not_a_review_thread: return "not_a_review_thread";
		case thread_reply_refusal::anonymous_read:      return "anonymous_read";
		case thread_reply_refusal::viewer_cannot_reply: return "viewer_cannot_reply";
		case thread_reply_refusal::locked:              return "locked";
		}
		return "none";
	}
}
