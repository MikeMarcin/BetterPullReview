#include "bpr/core/chat_transcript.hpp"

#include <algorithm>
#include <unordered_map>

namespace bpr::core {
	namespace {
		/// `aValue` as two digits.
		[[nodiscard]] auto two_digits(const std::int64_t aValue) -> std::string { return (aValue < 10 ? "0" : "") + std::to_string(aValue); }

		/// Where each turn of `aTranscript` stands, by identity.
		[[nodiscard]] auto places(const chat_transcript& aTranscript) -> std::unordered_map<std::string_view, std::size_t>
		{
			auto found = std::unordered_map<std::string_view, std::size_t>{ };
			found.reserve(aTranscript.Turns.size());
			for (std::size_t index = 0U; index < aTranscript.Turns.size(); ++index) {
				found.emplace(aTranscript.Turns[index].Id, index);
			}
			return found;
		}

		/// How a turn kept in both transcripts changed; none when it is the
		/// same.
		[[nodiscard]] auto change_of(const chat_turn& aBefore, const chat_turn& aAfter) -> std::optional<chat_change_kind>
		{
			if (aBefore == aAfter) {
				return std::nullopt;
			}
			if (aBefore.Running && !aAfter.Running) {
				return chat_change_kind::settled;
			}
			auto grown  = aBefore;
			grown.Reply = aAfter.Reply;
			return grown == aAfter ? chat_change_kind::reply_grew : chat_change_kind::changed;
		}

		/// The turns of `aAfter` kept from `aBefore` whose place among the
		/// kept turns is the same in both: walked in the later order, a kept
		/// turn that comes before the earlier order's next one moved.
		[[nodiscard]] auto kept_in_order(const chat_transcript& aAfter, const std::unordered_map<std::string_view, std::size_t>& aBeforePlaces) -> std::vector<bool>
		{
			auto inOrder = std::vector<bool>(aAfter.Turns.size(), false);
			auto next    = std::size_t{ 0U };
			for (std::size_t index = 0U; index < aAfter.Turns.size(); ++index) {
				const auto found = aBeforePlaces.find(aAfter.Turns[index].Id);
				if (found != aBeforePlaces.end() && found->second >= next) {
					inOrder[index] = true;
					next           = found->second + 1U;
				}
			}
			return inOrder;
		}
	}

	auto chat_agent_short_name(const std::string_view aAgent) -> std::string
	{
		if (aAgent.empty()) {
			return "The agent";
		}
		constexpr auto suffix = std::string_view{ " Code" };
		return aAgent.size() > suffix.size() && aAgent.ends_with(suffix) ? std::string{ aAgent.substr(0U, aAgent.size() - suffix.size()) } : std::string{ aAgent };
	}

	auto chat_elapsed_words(const std::int64_t aSeconds) -> std::string
	{
		const auto seconds = std::max<std::int64_t>(aSeconds, 0);
		if (seconds >= 3600) {
			return std::to_string(seconds / 3600) + ":" + two_digits(seconds / 60 % 60) + ":" + two_digits(seconds % 60);
		}
		return std::to_string(seconds / 60) + ":" + two_digits(seconds % 60);
	}

	auto chat_ending_first_words(const chat_ending& aEnding) -> std::string
	{
		if (aEnding.Outcome == agent_turn_outcome::answered) {
			const auto who = chat_agent_short_name(aEnding.Agent);
			return aEnding.Seconds ? who + " answered in " + chat_elapsed_words(*aEnding.Seconds) : who + " answered";
		}
		if (aEnding.Outcome == agent_turn_outcome::closed_with_app) {
			return aEnding.Seconds ? "Stopped when the app closed, after " + chat_elapsed_words(*aEnding.Seconds) : std::string{ "Stopped when the app closed" };
		}
		return aEnding.Seconds ? "Stopped after " + chat_elapsed_words(*aEnding.Seconds) : std::string{ "Stopped" };
	}

	auto chat_ending_words(const chat_ending& aEnding) -> std::string
	{
		if (!aEnding.Said.empty()) {
			return aEnding.Said;
		}
		auto line  = chat_ending_first_words(aEnding);
		auto parts = std::size_t{ 1U };
		if (aEnding.Cost) {
			line += " · " + agent_cost_words(*aEnding.Cost);
			++parts;
		}
		if (aEnding.Findings > 0U) {
			line += aEnding.Findings == 1U ? std::string{ " · 1 finding" } : " · " + std::to_string(aEnding.Findings) + " findings";
			++parts;
		}
		return parts == 1U ? line + "." : line;
	}

	auto chat_transcript::find(const std::string_view aId) const noexcept -> const chat_turn*
	{
		const auto found = std::ranges::find(Turns, aId, &chat_turn::Id);
		return found == Turns.end() ? nullptr : &*found;
	}

	auto stable_name(const chat_change_kind aKind) noexcept -> std::string_view
	{
		switch (aKind) {
		case chat_change_kind::appended:   return "appended";
		case chat_change_kind::removed:    return "removed";
		case chat_change_kind::reply_grew: return "reply_grew";
		case chat_change_kind::settled:    return "settled";
		case chat_change_kind::changed:    return "changed";
		}
		return "unknown";
	}

	auto chat_transcript_delta(const chat_transcript& aBefore, const chat_transcript& aAfter) -> chat_delta
	{
		auto       delta   = chat_delta{ };
		const auto before  = places(aBefore);
		const auto after   = places(aAfter);
		const auto inOrder = kept_in_order(aAfter, before);
		// A turn gone, or kept out of its order, leaves its earlier place.
		for (std::size_t index = 0U; index < aBefore.Turns.size(); ++index) {
			const auto found = after.find(aBefore.Turns[index].Id);
			if (found == after.end() || !inOrder[found->second]) {
				delta.push_back({ .Kind = chat_change_kind::removed, .Turn = aBefore.Turns[index].Id, .Index = index });
			}
		}
		for (std::size_t index = 0U; index < aAfter.Turns.size(); ++index) {
			const auto& turn = aAfter.Turns[index];
			if (!inOrder[index]) {
				delta.push_back({ .Kind = chat_change_kind::appended, .Turn = turn.Id, .Index = index });
			} else if (const auto kind = change_of(aBefore.Turns[before.at(turn.Id)], turn)) {
				delta.push_back({ .Kind = *kind, .Turn = turn.Id, .Index = index });
			}
		}
		return delta;
	}

	auto follow_up_apply_words(const follow_up_apply aApply) -> std::string
	{
		switch (aApply) {
		case follow_up_apply::update_suggestion: return "Update suggested comment";
		case follow_up_apply::update_draft:      return "Update draft";
		case follow_up_apply::draft_reply:       return "Draft reply";
		}
		return { };
	}

	auto project_follow_up(const follow_up_state& aState, const follow_up_thread_facts& aFacts) -> chat_transcript
	{
		auto       transcript = chat_transcript{ };
		const auto round      = [&](const std::size_t aIndex) { return aState.Subject.Key + "/round/" + std::to_string(aIndex); };
		for (std::size_t index = 0U; index < aState.Rounds.size(); ++index) {
			auto turn     = chat_turn{ };
			turn.Id       = round(index);
			turn.Question = aState.Rounds[index].Question;
			turn.Reply    = aState.Rounds[index].Answer;
			auto ending   = chat_ending{ };
			ending.Agent  = aFacts.Agent;
			if (index < aFacts.Seconds.size()) {
				ending.Seconds = aFacts.Seconds[index];
			}
			turn.Ending       = std::move(ending);
			turn.Actions.Copy = !turn.Reply.empty();
			transcript.Turns.push_back(std::move(turn));
		}
		// The question going becomes the next round, so it keeps its turn as
		// it is answered.
		if (aState.Phase == follow_up_phase::asking) {
			auto turn     = chat_turn{ };
			turn.Id       = round(aState.Rounds.size());
			turn.Question = aState.PendingQuestion;
			turn.Running  = true;
			transcript.Turns.push_back(std::move(turn));
		}
		// A proposal's run and the proposal it writes are one turn: the run
		// is the proposal's identity. Once written, the turn is only the
		// proposal, waiting for Use or Discard.
		const auto proposal = [&](const std::uint64_t aId) { return aState.Subject.Key + "/proposal/" + std::to_string(aId); };
		if (aState.Phase == follow_up_phase::proposing) {
			auto turn     = chat_turn{ };
			turn.Id       = proposal(aState.Pending);
			turn.Question = follow_up_apply_words(aState.PendingApply.value_or(follow_up_apply::update_suggestion));
			turn.Running  = true;
			transcript.Turns.push_back(std::move(turn));
		}
		if (aState.Proposal) {
			auto turn = chat_turn{ };
			turn.Id   = proposal(aState.Proposal->Id);
			turn.Attachments.push_back({ .Key = "proposal", .Payload = chat_follow_up_proposal{ .Proposal = *aState.Proposal, .Usable = aState.Phase == follow_up_phase::idle } });
			transcript.Turns.push_back(std::move(turn));
		}
		return transcript;
	}

	auto project_agent_thread(const std::vector<agent_thread_entry>& aEntries, const agent_thread_facts& aFacts) -> chat_transcript
	{
		auto transcript = chat_transcript{ };
		transcript.Turns.reserve(aEntries.size());
		for (std::size_t index = 0U; index < aEntries.size(); ++index) {
			const auto& entry = aEntries[index];
			auto        turn  = chat_turn{ };
			turn.Id           = "agent-turn-" + std::to_string(entry.Serial);
			turn.Question     = entry.Question;
			turn.Steered      = entry.Steered;
			turn.Reply        = entry.Answer;
			turn.Running      = !entry.Done;
			if (entry.Done) {
				turn.Ending       = chat_ending{ .Outcome = entry.Outcome, .Said = entry.Line };
				turn.Activity     = entry.Activity;
				const auto latest = index + 1U == aEntries.size();
				turn.Actions      = decide_agent_turn_actions({
					.Outcome          = entry.Outcome,
					.HasText          = !entry.Answer.empty(),
					.Latest           = latest,
					.Working          = aFacts.Working,
					.CausePassedSince = entry.CausePassedSince,
					.RevisionChoice   = latest && aFacts.RevisionChoice,
				});
			}
			// What the tab hangs on the reply: over it the verdict, under it
			// the report's row, then Draft all and the cards it drafts.
			if (entry.Verdict) {
				turn.Attachments.push_back({ .Key = "verdict", .Payload = *entry.Verdict, .Place = chat_attachment_place::before_reply });
			}
			if (entry.Report) {
				turn.Attachments.push_back({ .Key = "report", .Payload = *entry.Report, .Place = chat_attachment_place::after_reply });
			}
			if (!entry.Cards.empty()) {
				turn.Attachments.push_back({ .Key = "draft-all", .Payload = chat_agent_draft_all{ }, .Place = chat_attachment_place::after_reply });
				turn.Attachments.push_back({ .Key = "findings", .Payload = chat_agent_findings{ .Cards = entry.Cards }, .Place = chat_attachment_place::after_reply });
			}
			transcript.Turns.push_back(std::move(turn));
		}
		return transcript;
	}
}
