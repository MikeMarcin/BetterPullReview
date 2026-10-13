#include "bpr/core/reply_draft_standing.hpp"

#include <algorithm>
#include <ranges>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto has_words(const std::string_view aBody) noexcept -> bool
		{
			return std::ranges::any_of(aBody, [](const char aValue) { return aValue != ' ' && aValue != '\t' && aValue != '\r' && aValue != '\n' && aValue != '\v' && aValue != '\f'; });
		}

		/// An unsent reply: one answering a thread, still being written.
		[[nodiscard]] auto unsent_reply(const review_comment_draft& aDraft) noexcept -> bool
		{
			return aDraft.ReplyToThread.has_value() && !aDraft.ReplyToThread->empty() && aDraft.State == review_draft_state::drafting;
		}

		[[nodiscard]] auto reopenable(const review_comment_draft& aDraft) noexcept -> bool
		{
			return aDraft.State == review_draft_state::drafting || aDraft.State == review_draft_state::publishing;
		}
	}

	auto decide_reply_draft(const review_comment_draft& aDraft, const reply_thread_observation& aThreads) -> reply_draft_decision
	{
		auto decision  = reply_draft_decision{ };
		decision.Draft = aDraft.Id;
		if (!unsent_reply(aDraft) || !has_words(aDraft.Body)) {
			return decision;
		}
		decision.Thread    = *aDraft.ReplyToThread;
		const auto& thread = *aDraft.ReplyToThread;
		const auto  found  = std::ranges::find(aThreads.Threads, thread, &reply_thread_fact::Thread);
		if (found != aThreads.Threads.end()) {
			decision.Standing = found->Resolved ? reply_draft_standing::thread_resolved : reply_draft_standing::answerable;
			decision.Sendable = true;
			return decision;
		}
		if (!aThreads.Complete) {
			decision.Standing = reply_draft_standing::thread_unknown;
			return decision;
		}
		decision.Standing          = reply_draft_standing::thread_deleted;
		decision.KeepableAsComment = supported_comment_target(aDraft.Anchor.Side);
		return decision;
	}

	auto decide_reply_drafts(const std::span<const review_comment_draft> aDrafts, const reply_thread_observation& aThreads) -> std::vector<reply_draft_decision>
	{
		auto decisions = std::vector<reply_draft_decision>{ };
		for (const auto& draft : aDrafts) {
			if (auto decision = decide_reply_draft(draft, aThreads); decision.Standing != reply_draft_standing::none) {
				decisions.push_back(std::move(decision));
			}
		}
		return decisions;
	}

	auto reopen_draft_at(const std::span<const review_comment_draft> aDrafts, const source_line_ref& aLine, const std::optional<std::string>& aThread) -> std::optional<review_draft_id>
	{
		// The latest: a store lists drafts in the order they were made, and an
		// earlier empty draft can sit at the same line as a later written one.
		const auto latest = std::find_if(aDrafts.rbegin(), aDrafts.rend(), [&](const review_comment_draft& aDraft) {
			return aDraft.Anchor == aLine && aDraft.ReplyToThread == aThread && reopenable(aDraft);
		});
		if (latest == aDrafts.rend()) {
			return std::nullopt;
		}
		return latest->Id;
	}

	auto resume_reply_after_refresh(const std::span<const review_comment_draft> aDrafts, const review_draft_id aHeld, const reply_thread_observation& aThreads) -> std::optional<review_draft_id>
	{
		const auto held = std::ranges::find(aDrafts, aHeld, &review_comment_draft::Id);
		if (held == aDrafts.end() || !unsent_reply(*held)) {
			return std::nullopt;
		}
		// Written or not: an empty reply the reviewer had open is held again
		// too, while its thread is there to hold it.
		const auto found = std::ranges::find(aThreads.Threads, *held->ReplyToThread, &reply_thread_fact::Thread);
		if (found == aThreads.Threads.end()) {
			return std::nullopt;
		}
		return held->Id;
	}

	auto resume_comment_after_refresh(const std::span<const review_comment_draft> aDrafts, const review_draft_id aHeld) -> std::optional<review_draft_id>
	{
		const auto held = std::ranges::find(aDrafts, aHeld, &review_comment_draft::Id);
		if (held == aDrafts.end() || held->State != review_draft_state::drafting || (held->ReplyToThread.has_value() && !held->ReplyToThread->empty())) {
			return std::nullopt;
		}
		return held->Id;
	}

	auto keep_reply_as_comment(const revisioned_review_draft& aDraft, const reply_thread_observation& aThreads) -> std::optional<revisioned_review_draft_command>
	{
		if (!decide_reply_draft(aDraft.Draft, aThreads).KeepableAsComment) {
			return std::nullopt;
		}
		auto command              = revisioned_review_draft_command{ };
		command.Command.Id        = aDraft.Draft.Id;
		command.Command.Kind      = review_draft_command_kind::edit;
		command.Command.Body      = aDraft.Draft.Body;
		command.ExpectedRevision  = aDraft.Revision;
		command.Scope             = aDraft.Scope;
		command.ChangeReplyTarget = true;
		command.ReplyToThread     = std::nullopt;
		if (apply(aDraft, command).Refusal != revisioned_draft_refusal::none) {
			return std::nullopt;
		}
		return command;
	}

	auto discard_unsent_reply(const revisioned_review_draft& aDraft) -> std::optional<revisioned_review_draft_command>
	{
		if (!unsent_reply(aDraft.Draft)) {
			return std::nullopt;
		}
		auto command             = revisioned_review_draft_command{ };
		command.Command.Id       = aDraft.Draft.Id;
		command.Command.Kind     = review_draft_command_kind::discard;
		command.ExpectedRevision = aDraft.Revision;
		command.Scope            = aDraft.Scope;
		if (apply(aDraft, command).Refusal != revisioned_draft_refusal::none) {
			return std::nullopt;
		}
		return command;
	}

	auto name(const reply_draft_standing aStanding) noexcept -> std::string_view
	{
		switch (aStanding) {
		case reply_draft_standing::none:            return "none";
		case reply_draft_standing::answerable:      return "answerable";
		case reply_draft_standing::thread_resolved: return "thread_resolved";
		case reply_draft_standing::thread_deleted:  return "thread_deleted";
		case reply_draft_standing::thread_unknown:  return "thread_unknown";
		}
		return "none";
	}
}
