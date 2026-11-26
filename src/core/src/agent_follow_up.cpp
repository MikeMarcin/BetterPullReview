#include "bpr/core/agent_follow_up.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto trimmed(const std::string_view aText) -> std::string_view
		{
			constexpr auto blank = std::string_view{ " \t\r\n" };
			const auto     first = aText.find_first_not_of(blank);
			if (first == std::string_view::npos) {
				return { };
			}
			return aText.substr(first, aText.find_last_not_of(blank) - first + 1U);
		}

		/// The longest prefix of at most `aLimit` bytes that ends on a whole
		/// UTF-8 sequence.
		[[nodiscard]] auto utf8_prefix(const std::string_view aText, std::size_t aLimit) -> std::string_view
		{
			if (aText.size() <= aLimit) {
				return aText;
			}
			while (aLimit > 0U && (static_cast<unsigned char>(aText[aLimit]) & 0xC0U) == 0x80U) {
				--aLimit;
			}
			return aText.substr(0U, aLimit);
		}

		[[nodiscard]] auto characters(const std::string_view aText) -> std::size_t
		{
			return static_cast<std::size_t>(std::ranges::count_if(aText, [](const char aByte) { return (static_cast<unsigned char>(aByte) & 0xC0U) != 0x80U; }));
		}

		/// `aText` bounded, with a line saying how much was left out.
		[[nodiscard]] auto bounded(const std::string_view aText, const std::size_t aLimit) -> std::string
		{
			const auto kept = utf8_prefix(aText, aLimit);
			auto       text = std::string{ kept };
			if (kept.size() < aText.size()) {
				text += "\n(" + std::to_string(aText.size() - kept.size()) + " more bytes were not sent.)";
			}
			return text;
		}

		[[nodiscard]] auto statement(const follow_up_subject_kind aKind) -> std::string_view
		{
			switch (aKind) {
			case follow_up_subject_kind::suggested_comment:
				return "The item is a comment an agent suggested for the reviewer to post on the pull request. The reviewer is asking about it before deciding whether to post it, and may ask you to rewrite it.";
			case follow_up_subject_kind::draft:  return "The item is a draft comment the reviewer is writing on the pull request. It is not posted; the reviewer may ask you to rewrite it.";
			case follow_up_subject_kind::thread: return "The item is a review conversation on the pull request. The reviewer may ask you to draft a reply to it.";
			}
			return { };
		}

		[[nodiscard]] auto place(const follow_up_subject& aSubject) -> std::string
		{
			if (aSubject.Path.empty()) {
				return "the pull request";
			}
			if (aSubject.First == 0U) {
				return aSubject.Path;
			}
			if (aSubject.Last <= aSubject.First) {
				return aSubject.Path + " line " + std::to_string(aSubject.First);
			}
			return aSubject.Path + " lines " + std::to_string(aSubject.First) + "-" + std::to_string(aSubject.Last);
		}

		/// The item, as every round's message opens.
		[[nodiscard]] auto item(const follow_up_subject& aSubject) -> std::string
		{
			auto text = std::string{ follow_up_marker } + "\n\n" + std::string{ statement(aSubject.Kind) } + "\n\nWhere: " + place(aSubject) + "\n";
			if (!trimmed(aSubject.Title).empty()) {
				text += (aSubject.Kind == follow_up_subject_kind::suggested_comment ? "The finding: " : "About: ") + bounded(trimmed(aSubject.Title), maximum_follow_up_text_bytes) + "\n";
			}
			if (aSubject.Kind != follow_up_subject_kind::thread) {
				const auto body = trimmed(aSubject.Text);
				text += aSubject.Kind == follow_up_subject_kind::suggested_comment ? "\nThe suggested comment:\n" : "\nThe reviewer's draft:\n";
				text += body.empty() ? std::string{ "(empty)\n" } : "```text\n" + bounded(body, maximum_follow_up_text_bytes) + "\n```\n";
			}
			if (!aSubject.Thread.empty()) {
				text += aSubject.Kind == follow_up_subject_kind::thread ? "\nThe conversation:\n" : "\nThe conversation it replies in:\n";
				auto spent = std::size_t{ 0U };
				auto left  = std::size_t{ 0U };
				for (const auto& post : aSubject.Thread) {
					const auto body = utf8_prefix(trimmed(post.Body), maximum_follow_up_text_bytes);
					if (left > 0U || spent + body.size() > maximum_follow_up_thread_bytes) {
						++left;
						continue;
					}
					spent += body.size();
					text += "- " + (post.Author.empty() ? std::string{ "someone" } : post.Author) + ": " + std::string{ body } + "\n";
				}
				if (left > 0U) {
					text += "(" + std::to_string(left) + " later posts were not sent.)\n";
				}
			}
			return text;
		}

		/// The rounds so far, newest kept first when they do not all fit.
		[[nodiscard]] auto transcript(const std::vector<follow_up_round>& aRounds) -> std::string
		{
			if (aRounds.empty()) {
				return { };
			}
			auto kept  = std::vector<std::string>{ };
			auto spent = std::size_t{ 0U };
			for (auto round = aRounds.rbegin(); round != aRounds.rend(); ++round) {
				auto words = "Reviewer: " + round->Question + "\nYou: " + round->Answer + "\n";
				if (spent + words.size() > maximum_follow_up_transcript_bytes) {
					break;
				}
				spent += words.size();
				kept.push_back(std::move(words));
			}
			auto text = std::string{ "\nEarlier in this conversation:\n" };
			if (kept.size() < aRounds.size()) {
				text += "(" + std::to_string(aRounds.size() - kept.size()) + " earlier rounds were not sent.)\n";
			}
			for (auto words = kept.rbegin(); words != kept.rend(); ++words) {
				text += *words;
			}
			return text;
		}

		/// The earlier rounds a message carries: none when the agent's own
		/// session holds them.
		[[nodiscard]] auto earlier(const follow_up_state& aState) -> std::string { return aState.InSession ? std::string{ } : transcript(aState.Rounds); }

		[[nodiscard]] auto instruction(const follow_up_apply aApply) -> std::string_view
		{
			switch (aApply) {
			case follow_up_apply::update_suggestion:
				// [[BPR-2376]]. Only a suggestion block is the finding's
				// Suggested change (`comment_suggestion_lines`).
				return "Rewrite the suggested comment as the conversation above settled it. Answer with only the new comment, exactly as it would be posted: no preamble, no explanation, no quotes around it. "
					   "Write a change to the commented lines as a fenced block whose info string is suggestion, holding the lines that replace them, as GitHub applies it; a block in another language is posted as code, not as a change.";
			case follow_up_apply::update_draft:
				return "Rewrite the reviewer's draft as the conversation above settled it, keeping the reviewer's voice. Answer with only the new draft, exactly as it would be posted: no preamble, no explanation, no quotes around it.";
			case follow_up_apply::draft_reply:
				return "Write a reply the reviewer could post in this conversation, in the reviewer's voice, as the conversation above settled it. Answer with only the reply, exactly as it would be posted: no preamble, no explanation, no quotes around it.";
			}
			return { };
		}

		[[nodiscard]] auto begin(follow_up_state& aState) -> std::uint64_t
		{
			aState.Pending = aState.NextId++;
			aState.Refused = follow_up_refusal::none;
			return aState.Pending;
		}

		auto refuse(follow_up_state& aState, const follow_up_refusal aRefusal) -> follow_up_step
		{
			aState.Refused = aRefusal;
			return { .Refusal = aRefusal, .Request = std::nullopt };
		}

		void settle_idle(follow_up_state& aState)
		{
			aState.Phase   = follow_up_phase::idle;
			aState.Pending = 0U;
			aState.PendingQuestion.clear();
			aState.PendingApply.reset();
			aState.PendingBase.clear();
		}
	} // namespace

	auto stable_name(const follow_up_subject_kind aKind) noexcept -> std::string_view
	{
		switch (aKind) {
		case follow_up_subject_kind::suggested_comment: return "suggested-comment";
		case follow_up_subject_kind::draft:             return "draft";
		case follow_up_subject_kind::thread:            return "thread";
		}
		return "unknown";
	}

	auto stable_name(const follow_up_apply aApply) noexcept -> std::string_view
	{
		switch (aApply) {
		case follow_up_apply::update_suggestion: return "update-suggestion";
		case follow_up_apply::update_draft:      return "update-draft";
		case follow_up_apply::draft_reply:       return "draft-reply";
		}
		return "unknown";
	}

	auto stable_name(const follow_up_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case follow_up_refusal::none:            return "none";
		case follow_up_refusal::busy:            return "busy";
		case follow_up_refusal::empty_question:  return "empty-question";
		case follow_up_refusal::not_offered:     return "not-offered";
		case follow_up_refusal::too_many_rounds: return "too-many-rounds";
		case follow_up_refusal::stale:           return "stale";
		case follow_up_refusal::no_proposal:     return "no-proposal";
		case follow_up_refusal::empty_answer:    return "empty-answer";
		case follow_up_refusal::unchanged:       return "unchanged";
		case follow_up_refusal::too_long:        return "too-long";
		case follow_up_refusal::target_changed:  return "target-changed";
		case follow_up_refusal::failed:          return "failed";
		}
		return "unknown";
	}

	auto valid(const follow_up_subject& aSubject) -> bool
	{
		if (aSubject.Key.empty() || aSubject.Key.size() > 1024U || aSubject.Path.size() > 4096U || aSubject.Title.size() > maximum_follow_up_text_bytes) {
			return false;
		}
		if (aSubject.Last != 0U && aSubject.Last < aSubject.First) {
			return false;
		}
		if (aSubject.Kind == follow_up_subject_kind::thread) {
			return aSubject.Text.empty() && !aSubject.Thread.empty();
		}
		return aSubject.Text.size() <= 4U * maximum_follow_up_text_bytes && !aSubject.Replies;
	}

	auto follow_up_offers(const follow_up_subject& aSubject) -> std::vector<follow_up_apply>
	{
		switch (aSubject.Kind) {
		case follow_up_subject_kind::suggested_comment: return { follow_up_apply::update_suggestion };
		case follow_up_subject_kind::draft:             return { follow_up_apply::update_draft };
		case follow_up_subject_kind::thread:            return aSubject.Replies ? std::vector{ follow_up_apply::draft_reply } : std::vector<follow_up_apply>{ };
		}
		return { };
	}

	auto open_follow_up(follow_up_subject aSubject) -> follow_up_state
	{
		auto state    = follow_up_state{ };
		state.Subject = std::move(aSubject);
		return state;
	}

	auto refresh_follow_up(follow_up_state& aState, follow_up_subject aSubject) -> bool
	{
		if (aSubject.Key != aState.Subject.Key || aSubject.Kind != aState.Subject.Kind) {
			return false;
		}
		aState.Subject = std::move(aSubject);
		return true;
	}

	auto restore_follow_up(follow_up_state& aState, std::vector<follow_up_round> aRounds) -> follow_up_refusal
	{
		if (aState.Phase != follow_up_phase::idle) {
			return follow_up_refusal::busy;
		}
		// A session longer than one chat keeps shows its latest rounds.
		if (aRounds.size() > maximum_follow_up_rounds) {
			aRounds.erase(aRounds.begin(), aRounds.end() - static_cast<std::ptrdiff_t>(maximum_follow_up_rounds));
		}
		aState.Rounds    = std::move(aRounds);
		aState.Proposal  = std::nullopt;
		aState.Refused   = follow_up_refusal::none;
		aState.InSession = true;
		return follow_up_refusal::none;
	}

	auto ask_follow_up(follow_up_state& aState, const std::string_view aQuestion) -> follow_up_step
	{
		if (aState.Phase != follow_up_phase::idle) {
			return refuse(aState, follow_up_refusal::busy);
		}
		const auto question = trimmed(aQuestion);
		if (question.empty() || question.size() > maximum_follow_up_text_bytes) {
			return refuse(aState, follow_up_refusal::empty_question);
		}
		if (aState.Rounds.size() >= maximum_follow_up_rounds) {
			return refuse(aState, follow_up_refusal::too_many_rounds);
		}
		const auto id          = begin(aState);
		aState.Phase           = follow_up_phase::asking;
		aState.PendingQuestion = std::string{ question };
		auto message           = item(aState.Subject) + earlier(aState);
		message += "\nThe reviewer asks:\n" + std::string{ question } + "\n\nAnswer the reviewer directly and briefly. Do not write the whole comment again unless asked; nothing is posted from this answer.\n";
		return { .Refusal = follow_up_refusal::none, .Request = follow_up_request{ .Id = id, .Message = std::move(message) } };
	}

	auto propose_follow_up(follow_up_state& aState, const follow_up_apply aApply, std::string aBase) -> follow_up_step
	{
		if (aState.Phase != follow_up_phase::idle) {
			return refuse(aState, follow_up_refusal::busy);
		}
		const auto offered = follow_up_offers(aState.Subject);
		if (std::ranges::find(offered, aApply) == offered.end()) {
			return refuse(aState, follow_up_refusal::not_offered);
		}
		const auto id       = begin(aState);
		aState.Phase        = follow_up_phase::proposing;
		aState.PendingApply = aApply;
		aState.PendingBase  = std::move(aBase);
		auto message        = item(aState.Subject) + earlier(aState);
		message += "\nbpr.follow-up." + std::string{ stable_name(aApply) } + "\n" + std::string{ instruction(aApply) } + "\n";
		return { .Refusal = follow_up_refusal::none, .Request = follow_up_request{ .Id = id, .Message = std::move(message) } };
	}

	auto settle_follow_up(follow_up_state& aState, const std::uint64_t aId, std::optional<std::string> aAnswer) -> follow_up_refusal
	{
		if (aId == 0U || aId != aState.Pending || aState.Phase == follow_up_phase::idle) {
			return follow_up_refusal::stale;
		}
		const auto phase    = aState.Phase;
		const auto question = std::move(aState.PendingQuestion);
		const auto apply    = aState.PendingApply;
		auto       base     = std::move(aState.PendingBase);
		settle_idle(aState);
		if (!aAnswer) {
			aState.Refused = follow_up_refusal::failed;
			return aState.Refused;
		}
		if (phase == follow_up_phase::asking) {
			const auto answer = trimmed(*aAnswer);
			if (answer.empty()) {
				aState.Refused = follow_up_refusal::empty_answer;
				return aState.Refused;
			}
			aState.Rounds.push_back({ .Question = question, .Answer = std::string{ answer } });
			return follow_up_refusal::none;
		}
		auto text = read_follow_up_proposal(*aAnswer);
		if (!text) {
			aState.Refused = trimmed(*aAnswer).empty() ? follow_up_refusal::empty_answer : follow_up_refusal::too_long;
			return aState.Refused;
		}
		if (apply != follow_up_apply::draft_reply && trimmed(*text) == trimmed(base)) {
			aState.Refused = follow_up_refusal::unchanged;
			return aState.Refused;
		}
		aState.Proposal = follow_up_proposal{ .Id = aId, .Apply = apply.value_or(follow_up_apply::update_suggestion), .Text = std::move(*text), .Base = std::move(base) };
		return follow_up_refusal::none;
	}

	auto apply_follow_up(follow_up_state& aState, const std::uint64_t aProposal, const std::string_view aCurrent) -> follow_up_application
	{
		auto decided = follow_up_application{ };
		if (!aState.Proposal) {
			decided.Refusal = aState.Refused = follow_up_refusal::no_proposal;
			return decided;
		}
		if (aState.Proposal->Id != aProposal) {
			decided.Refusal = aState.Refused = follow_up_refusal::stale;
			return decided;
		}
		decided.Apply = aState.Proposal->Apply;
		if (decided.Apply != follow_up_apply::draft_reply && aCurrent != aState.Proposal->Base) {
			decided.Refusal = aState.Refused = follow_up_refusal::target_changed;
			return decided;
		}
		decided.Key  = aState.Subject.Key;
		decided.Text = std::move(aState.Proposal->Text);
		if (decided.Apply != follow_up_apply::draft_reply) {
			aState.Subject.Text = decided.Text;
		}
		aState.Proposal.reset();
		aState.Refused = follow_up_refusal::none;
		++aState.Applied;
		return decided;
	}

	auto discard_follow_up(follow_up_state& aState, const std::uint64_t aProposal) -> bool
	{
		if (!aState.Proposal || aState.Proposal->Id != aProposal) {
			return false;
		}
		aState.Proposal.reset();
		return true;
	}

	auto read_follow_up_proposal(const std::string_view aAnswer) -> std::optional<std::string>
	{
		auto text = trimmed(aAnswer);
		// One plain fenced block around the whole answer is a wrapper, not
		// content; a `suggestion` block is the comment's own.
		if (text.starts_with("```") && text.ends_with("```") && text.size() > 6U) {
			const auto open  = text.find('\n');
			const auto info  = open == std::string_view::npos ? std::string_view{ } : trimmed(text.substr(3U, open - 3U));
			const auto inner = open == std::string_view::npos ? std::string_view{ } : text.substr(open + 1U, text.size() - 3U - open - 1U);
			if (open != std::string_view::npos && (info.empty() || info == "text" || info == "markdown" || info == "md") && inner.find("```") == std::string_view::npos) {
				text = trimmed(inner);
			}
		}
		if (text.empty() || characters(text) > maximum_follow_up_proposal_characters) {
			return std::nullopt;
		}
		return std::string{ text };
	}
} // namespace bpr::core
