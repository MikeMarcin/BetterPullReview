#include "bpr/core/agent_answer_findings.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <vector>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto blank(const char aByte) noexcept -> bool { return aByte == ' ' || aByte == '\t' || aByte == '\r' || aByte == '\n' || aByte == '\f' || aByte == '\v'; }

		[[nodiscard]] auto trimmed(std::string_view aText) noexcept -> std::string_view
		{
			while (!aText.empty() && blank(aText.front())) {
				aText.remove_prefix(1U);
			}
			while (!aText.empty() && blank(aText.back())) {
				aText.remove_suffix(1U);
			}
			return aText;
		}

		/// One line: where it begins, where its text ends, and where the next
		/// line begins.
		struct line_span
		{
			std::size_t Begin = 0U;
			std::size_t End   = 0U;
			std::size_t Next  = 0U;
		};

		[[nodiscard]] auto line_at(const std::string_view aText, const std::size_t aBegin) noexcept -> line_span
		{
			const auto newline = aText.find('\n', aBegin);
			return newline == std::string_view::npos ? line_span{ aBegin, aText.size(), aText.size() } : line_span{ aBegin, newline, newline + 1U };
		}

		[[nodiscard]] auto opening_line(const std::string_view aLine) noexcept -> bool
		{
			auto text   = aLine;
			auto spaces = std::size_t{ 0U };
			while (!text.empty() && text.front() == ' ' && spaces < 3U) {
				text.remove_prefix(1U);
				++spaces;
			}
			return text.starts_with(agent_findings_fence) && trimmed(text.substr(agent_findings_fence.size())).empty();
		}

		/// A line of three or more backticks and nothing else.
		[[nodiscard]] auto closing_line(const std::string_view aLine) noexcept -> bool
		{
			const auto text = trimmed(aLine);
			return text.size() >= 3U && std::ranges::all_of(text, [](const char aByte) { return aByte == '`'; });
		}

		/// The first closing fence line at or after the line beginning at
		/// `aFrom`.
		[[nodiscard]] auto closing_from(const std::string_view aText, std::size_t aFrom) noexcept -> std::optional<line_span>
		{
			while (aFrom < aText.size()) {
				const auto line = line_at(aText, aFrom);
				if (closing_line(aText.substr(line.Begin, line.End - line.Begin))) {
					return line;
				}
				aFrom = line.Next;
			}
			return std::nullopt;
		}

		/// Where one JSON object or array beginning at `aBegin` ends, reading
		/// strings with their escapes, or none when it is cut short or its
		/// brackets do not match.
		[[nodiscard]] auto value_end(const std::string_view aText, const std::size_t aBegin) -> std::optional<std::size_t>
		{
			if (aBegin >= aText.size() || (aText[aBegin] != '{' && aText[aBegin] != '[')) {
				return std::nullopt;
			}
			auto open     = std::vector<char>{ };
			auto inString = false;
			auto escaped  = false;
			for (auto at = aBegin; at < aText.size(); ++at) {
				const char byte = aText[at];
				if (inString) {
					if (escaped) {
						escaped = false;
					} else if (byte == '\\') {
						escaped = true;
					} else if (byte == '"') {
						inString = false;
					}
					continue;
				}
				if (byte == '"') {
					inString = true;
				} else if (byte == '{' || byte == '[') {
					open.push_back(byte == '{' ? '}' : ']');
				} else if (byte == '}' || byte == ']') {
					if (open.empty() || open.back() != byte) {
						return std::nullopt;
					}
					open.pop_back();
					if (open.empty()) {
						return at + 1U;
					}
				}
			}
			return std::nullopt;
		}

		[[nodiscard]] auto joined(const std::string_view aBefore, const std::string_view aMiddle, const std::string_view aAfter) -> std::string
		{
			auto result = std::string{ };
			for (const auto part : { aBefore, aMiddle, aAfter }) {
				if (part.empty()) {
					continue;
				}
				if (!result.empty()) {
					result += "\n\n";
				}
				result += part;
			}
			return result;
		}

		/// The payload as a code block whose fence no run of backticks inside
		/// it can close.
		[[nodiscard]] auto fenced(const std::string_view aPayload) -> std::string
		{
			auto longest = std::size_t{ 0U };
			auto run     = std::size_t{ 0U };
			for (const char byte : aPayload) {
				run     = byte == '`' ? run + 1U : 0U;
				longest = std::max(longest, run);
			}
			const auto fence = std::string(std::max<std::size_t>(3U, longest + 1U), '`');
			return fence + "json\n" + std::string{ aPayload } + "\n" + fence;
		}
	}

	auto stable_name(const agent_findings_block aBlock) noexcept -> std::string_view
	{
		switch (aBlock) {
		case agent_findings_block::absent:
			return "absent";
		case agent_findings_block::complete:
			return "complete";
		case agent_findings_block::unreadable:
			return "unreadable";
		}
		return "unknown";
	}

	auto split_agent_answer(const std::string_view aAnswer) -> agent_answer_split
	{
		auto opener = std::optional<line_span>{ };
		for (auto at = std::size_t{ 0U }; at < aAnswer.size();) {
			const auto line = line_at(aAnswer, at);
			if (opening_line(aAnswer.substr(line.Begin, line.End - line.Begin))) {
				opener = line;
			}
			at = line.Next;
		}
		if (!opener) {
			return { .Block = agent_findings_block::absent, .Prose = std::string{ aAnswer } };
		}
		const auto before = trimmed(aAnswer.substr(0U, opener->Begin));
		auto       start  = opener->Next;
		while (start < aAnswer.size() && blank(aAnswer[start])) {
			++start;
		}
		if (const auto end = value_end(aAnswer, start)) {
			auto after = *end;
			while (after < aAnswer.size() && blank(aAnswer[after])) {
				++after;
			}
			const auto rest = aAnswer.substr(after);
			// The closing fence may follow the value on its own line or on
			// the value's last line; the end of the answer closes it too.
			auto closeEnd = std::optional<std::size_t>{ };
			if (rest.empty()) {
				closeEnd = aAnswer.size();
			} else if (rest.starts_with("```")) {
				const auto line = line_at(aAnswer, after);
				if (closing_line(aAnswer.substr(after, line.End - after))) {
					closeEnd = line.Next;
				}
			}
			if (closeEnd) {
				return {
					.Block   = agent_findings_block::complete,
					.Prose   = joined(before, { }, trimmed(aAnswer.substr(*closeEnd))),
					.Payload = std::string{ aAnswer.substr(start, *end - start) },
				};
			}
		}
		// No complete value: the block runs to the first fence line after its
		// opening, or to the end, and is shown whole as code.
		const auto closing = closing_from(aAnswer, opener->Next);
		const auto payload = trimmed(aAnswer.substr(opener->Next, (closing ? closing->Begin : aAnswer.size()) - opener->Next));
		const auto after   = closing ? trimmed(aAnswer.substr(closing->Next)) : std::string_view{ };
		const auto code    = payload.empty() ? std::string{ } : fenced(payload);
		return {
			.Block   = agent_findings_block::unreadable,
			.Prose   = joined(before, code, after),
			.Payload = std::string{ payload },
		};
	}
}
