#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_ANSWER_FINDINGS_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_ANSWER_FINDINGS_HPP

#include <cstdint>
#include <string>
#include <string_view>

/// \file
/// \brief [[BPR-1861]]. An agent's answer split into the prose the reply
/// shows and the `bpr-findings` block the review skill asks it to end with
/// ([[BPR-1753]]).
/// \details The block's end used to be the first three backticks after its
/// opening line. A suggested comment holding a fenced code block puts three
/// backticks inside a JSON string, so the block was cut there: the decoder
/// read a torn half and made no cards, and the reply showed the rest of the
/// payload as Markdown, its `\n` escapes literal and its closing `"}]}`
/// visible. The block now ends where its JSON value ends, read with string
/// escapes honoured, so backticks, quotes, braces and brackets inside a
/// string are text. Nothing here parses the findings themselves; the
/// application's bounded decoder does that with `Payload`.
namespace bpr::core {
	/// \brief The opening fence line's text, at the start of a line.
	inline constexpr std::string_view agent_findings_fence = "```bpr-findings";

	enum class agent_findings_block : std::uint8_t {
		/// No line opens a `bpr-findings` block: the answer is all prose.
		absent,
		/// One complete JSON object or array, then the closing fence or the
		/// end of the answer. The reply shows the prose without it.
		complete,
		/// The block holds no complete JSON value: it was cut short, its
		/// brackets do not balance, or text follows the value inside the
		/// fence. The reply shows it, whole, as a code block of its own after
		/// the prose, so no fragment of it is ever read as Markdown.
		unreadable,
	};

	[[nodiscard]] auto stable_name(agent_findings_block aBlock) noexcept -> std::string_view;

	struct agent_answer_split
	{
		agent_findings_block Block = agent_findings_block::absent;
		/// What the reply shows. With no block, the answer exactly as given;
		/// otherwise the text before and after the block, trimmed, and for an
		/// unreadable block that block as a fenced code block.
		std::string Prose;
		/// The block's JSON text: from its first byte to the end of its value
		/// when complete, to its closing fence or the end when unreadable.
		/// Empty when absent.
		std::string Payload;

		auto operator==(const agent_answer_split&) const -> bool = default;
	};

	/// \brief Split one answer. The last line that is the opening fence
	/// (after at most three spaces, with nothing but white space after it)
	/// opens the block. Linear in the answer's length; no effects.
	[[nodiscard]] auto split_agent_answer(std::string_view aAnswer) -> agent_answer_split;
}

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_ANSWER_FINDINGS_HPP
