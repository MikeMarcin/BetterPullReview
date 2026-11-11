#include "bpr/core/draft_roster.hpp"

#include <algorithm>
#include <tuple>

namespace bpr::core {
	auto stable_name(const draft_standing aStanding) noexcept -> std::string_view
	{
		switch (aStanding) {
		case draft_standing::in_review:
			return "in_review";
		case draft_standing::not_in_review:
			return "not_in_review";
		case draft_standing::held:
			return "held";
		case draft_standing::reply:
			return "reply";
		}
		return "unknown";
	}

	auto stable_name(const draft_add_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case draft_add_refusal::none:
			return "none";
		case draft_add_refusal::reply:
			return "reply";
		case draft_add_refusal::place_unknown:
			return "place_unknown";
		case draft_add_refusal::not_at_head:
			return "not_at_head";
		}
		return "unknown";
	}

	namespace {
		[[nodiscard]] auto blank(const std::string_view aBody) noexcept -> bool
		{
			return std::ranges::all_of(aBody, [](const char aValue) { return aValue == ' ' || aValue == '\t' || aValue == '\r' || aValue == '\n' || aValue == '\v' || aValue == '\f'; });
		}

		[[nodiscard]] auto standing_of(const review_comment_draft& aDraft) noexcept -> std::optional<draft_standing>
		{
			if (held_by_review(aDraft)) {
				return draft_standing::held;
			}
			if (aDraft.State != review_draft_state::drafting) {
				return std::nullopt;
			}
			if (aDraft.ReplyToThread) {
				return draft_standing::reply;
			}
			return pending_in_review(aDraft) ? draft_standing::in_review : draft_standing::not_in_review;
		}
	} // namespace

	auto project_draft_roster(const std::span<const revisioned_review_draft> aDrafts, const draft_roster_facts& aFacts) -> draft_roster
	{
		auto roster = draft_roster{ };
		for (const auto& saved : aDrafts) {
			const auto& draft    = saved.Draft;
			const auto  standing = standing_of(draft);
			if (!standing || blank(draft.Body)) {
				continue;
			}
			auto entry = draft_roster_entry{
				.Id            = draft.Id,
				.Revision      = saved.Revision,
				.Scope         = saved.Scope,
				.Standing      = *standing,
				.Side          = draft.Anchor.Side,
				.Line          = draft.Anchor.Line,
				.StartLine     = draft.StartLine,
				.Comparison    = draft.Anchor.Comparison,
				.ReplyToThread = draft.ReplyToThread,
				.Body          = draft.Body,
			};
			// Where it is: the review's own path, the place it recorded, its
			// thread's file, or the file its comparison was shown as.
			if (draft.InReview) {
				entry.Path = draft.InReview->Path;
			} else if (draft.Place) {
				entry.Path = draft.Place->Path;
			} else if (const auto thread = draft.ReplyToThread ? std::ranges::find(aFacts.Threads, *draft.ReplyToThread, &std::pair<std::string, std::string>::first) : aFacts.Threads.end();
					   thread != aFacts.Threads.end()) {
				entry.Path = thread->second;
			} else if (const auto shown = std::ranges::find(aFacts.Shown, draft.Anchor.Comparison, &std::pair<file_comparison_digest, std::string>::first); shown != aFacts.Shown.end()) {
				entry.Path = shown->second;
			}
			switch (*standing) {
			case draft_standing::in_review:
				++roster.InReview;
				break;
			case draft_standing::held:
				++roster.Held;
				break;
			case draft_standing::reply:
				++roster.Replies;
				entry.Refusal = draft_add_refusal::reply;
				break;
			case draft_standing::not_in_review:
				++roster.NotInReview;
				if (!draft.Place) {
					entry.Refusal = draft_add_refusal::place_unknown;
				} else if (!draft.Place->ReviewHead || *draft.Place->ReviewHead != aFacts.Head || !aFacts.Head.valid()) {
					entry.Refusal = draft_add_refusal::not_at_head;
				} else {
					entry.Mark = review_pending_mark{ .Path = draft.Place->Path, .Head = aFacts.Head, .Submission = { } };
				}
				break;
			}
			roster.Entries.push_back(std::move(entry));
		}
		std::ranges::sort(roster.Entries, [](const draft_roster_entry& aLeft, const draft_roster_entry& aRight) {
			return std::tuple{ aLeft.Path.empty(), std::string_view{ aLeft.Path }, aLeft.ReplyToThread.has_value(), aLeft.Side, aLeft.Line, aLeft.Id.value() }
			< std::tuple{ aRight.Path.empty(), std::string_view{ aRight.Path }, aRight.ReplyToThread.has_value(), aRight.Side, aRight.Line, aRight.Id.value() };
		});
		return roster;
	}

	auto draft_counts_by_file(const draft_roster& aRoster) -> std::vector<draft_file_count>
	{
		auto counts = std::vector<draft_file_count>{ };
		for (const auto& entry : aRoster.Entries) {
			if (entry.Path.empty()) {
				continue;
			}
			if (counts.empty() || counts.back().Path != entry.Path) {
				counts.push_back({ .Path = entry.Path });
			}
			auto& count = counts.back();
			if (entry.Standing == draft_standing::in_review || entry.Standing == draft_standing::held) {
				++count.InReview;
			} else {
				++count.NotInReview;
			}
		}
		return counts;
	}

	auto drafts_outside_review(const draft_roster& aRoster) -> std::vector<draft_roster_entry>
	{
		auto outside = std::vector<draft_roster_entry>{ };
		for (const auto& entry : aRoster.Entries) {
			if (entry.Standing == draft_standing::not_in_review || entry.Standing == draft_standing::reply) {
				outside.push_back(entry);
			}
		}
		return outside;
	}

	auto plan_add_to_review(const draft_roster& aRoster, const std::span<const review_draft_id> aChosen) -> std::vector<revisioned_review_draft_command>
	{
		auto commands = std::vector<revisioned_review_draft_command>{ };
		for (const auto& entry : aRoster.Entries) {
			if (entry.Standing != draft_standing::not_in_review || !entry.Mark || std::ranges::find(aChosen, entry.Id) == aChosen.end()) {
				continue;
			}
			commands.push_back(revisioned_review_draft_command{
				.Command          = { .Id = entry.Id, .Kind = review_draft_command_kind::edit, .Body = entry.Body },
				.ExpectedRevision = entry.Revision,
				.Scope            = entry.Scope,
				.ChangePending    = true,
				.Pending          = entry.Mark,
			});
		}
		return commands;
	}
} // namespace bpr::core
