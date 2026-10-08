#include "bpr/core/feedback_workshop_candidate.hpp"

#include "bpr/core/suggestion_draft.hpp"

namespace bpr::core {
	namespace {
		/// \brief The same header semantics: status, label, decorations, and
		/// for a label the catalog does not know, the token itself.
		[[nodiscard]] auto same_header(const workshop_label_facts& aBefore, const workshop_label_facts& aAfter) -> bool
		{
			return aBefore.Status == aAfter.Status && aBefore.Label == aAfter.Label && aBefore.Expectation == aAfter.Expectation
				&& aBefore.Decorations == aAfter.Decorations
				&& (aBefore.Status == feedback_prefix_status::recognized || aBefore.RawLabel == aAfter.RawLabel);
		}

		[[nodiscard]] auto has_header(const workshop_label_facts& aFacts) noexcept -> bool
		{
			return aFacts.Status != feedback_prefix_status::plain;
		}

		[[nodiscard]] auto closing_punctuation(const char aCharacter) noexcept -> bool
		{
			return aCharacter == ')' || aCharacter == ']' || aCharacter == '"' || aCharacter == '\'' || aCharacter == '*' || aCharacter == '_';
		}

		/// \brief Whether a line opens or closes a fence: three or more
		/// backticks or tildes after at most three spaces.
		[[nodiscard]] auto fence_marker(const std::string_view aLine) noexcept -> char
		{
			auto at = std::size_t{ 0U };
			while (at < aLine.size() && at < 3U && aLine[at] == ' ') {
				++at;
			}
			if (at + 3U > aLine.size()) {
				return '\0';
			}
			const auto marker = aLine[at];
			if ((marker != '`' && marker != '~') || aLine[at + 1U] != marker || aLine[at + 2U] != marker) {
				return '\0';
			}
			return marker;
		}
	} // namespace

	auto stable_name(const workshop_label_change aChange) noexcept -> std::string_view
	{
		switch (aChange) {
		case workshop_label_change::none:
			return "none";
		case workshop_label_change::kept:
			return "kept";
		case workshop_label_change::added:
			return "added";
		case workshop_label_change::removed:
			return "removed";
		case workshop_label_change::changed:
			return "changed";
		}
		return "unknown";
	}

	auto stable_name(const workshop_question_change aChange) noexcept -> std::string_view
	{
		switch (aChange) {
		case workshop_question_change::none:
			return "none";
		case workshop_question_change::kept:
			return "kept";
		case workshop_question_change::added:
			return "added";
		case workshop_question_change::removed:
			return "removed";
		}
		return "unknown";
	}

	auto stable_name(const workshop_patch_change aChange) noexcept -> std::string_view
	{
		switch (aChange) {
		case workshop_patch_change::none:
			return "none";
		case workshop_patch_change::kept:
			return "kept";
		case workshop_patch_change::added:
			return "added";
		case workshop_patch_change::removed:
			return "removed";
		case workshop_patch_change::changed:
			return "changed";
		}
		return "unknown";
	}

	auto workshop_label_facts_of(const std::string_view aBody) -> workshop_label_facts
	{
		const auto annotation = recognize_feedback_prefix(aBody);
		auto       facts      = workshop_label_facts{ };
		facts.Status          = annotation.Status;
		facts.Label           = annotation.Label;
		facts.Expectation     = annotation.Expectation;
		if (annotation.RawLabel) {
			facts.RawLabel = std::string{ feedback_text(aBody, *annotation.RawLabel) };
		}
		for (const auto& decoration : annotation.Decorations) {
			facts.Decorations.emplace_back(feedback_text(aBody, decoration.Raw));
		}
		return facts;
	}

	auto workshop_asks_question(const std::string_view aBody) -> bool
	{
		auto fence = '\0';
		auto start = std::size_t{ 0U };
		while (start <= aBody.size()) {
			const auto end    = aBody.find('\n', start);
			const auto line   = aBody.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
			const auto marker = fence_marker(line);
			if (fence != '\0') {
				if (marker == fence) {
					fence = '\0';
				}
			} else if (marker != '\0') {
				fence = marker;
			} else {
				auto code = false;
				for (auto index = std::size_t{ 0U }; index < line.size(); ++index) {
					if (line[index] == '`') {
						code = !code;
					} else if (!code && line[index] == '?') {
						const auto next = index + 1U < line.size() ? line[index + 1U] : '\n';
						if (next == ' ' || next == '\t' || next == '\r' || next == '\n' || closing_punctuation(next)) {
							return true;
						}
					}
				}
			}
			if (end == std::string_view::npos) {
				break;
			}
			start = end + 1U;
		}
		return false;
	}

	auto compare_workshop_candidate(const std::string_view aOriginal, const std::string_view aCandidate) -> workshop_candidate_change
	{
		auto change   = workshop_candidate_change{ };
		change.Before = workshop_label_facts_of(aOriginal);
		change.After  = workshop_label_facts_of(aCandidate);
		if (!has_header(change.Before) && !has_header(change.After)) {
			change.Label = workshop_label_change::none;
		} else if (!has_header(change.Before)) {
			change.Label = workshop_label_change::added;
		} else if (!has_header(change.After)) {
			change.Label = workshop_label_change::removed;
		} else {
			change.Label = same_header(change.Before, change.After) ? workshop_label_change::kept : workshop_label_change::changed;
		}

		const auto askedBefore = workshop_asks_question(aOriginal);
		const auto askedAfter  = workshop_asks_question(aCandidate);
		change.Question        = askedBefore ? (askedAfter ? workshop_question_change::kept : workshop_question_change::removed)
											 : (askedAfter ? workshop_question_change::added : workshop_question_change::none);

		const auto before = suggestion_blocks(aOriginal);
		const auto after  = suggestion_blocks(aCandidate);
		if (before.empty() && after.empty()) {
			change.Patch = workshop_patch_change::none;
		} else if (before.empty()) {
			change.Patch = workshop_patch_change::added;
		} else if (after.empty()) {
			change.Patch = workshop_patch_change::removed;
		} else {
			change.Patch = before == after ? workshop_patch_change::kept : workshop_patch_change::changed;
		}
		return change;
	}
} // namespace bpr::core
