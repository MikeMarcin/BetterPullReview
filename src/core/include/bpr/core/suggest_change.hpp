#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_SUGGEST_CHANGE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_SUGGEST_CHANGE_HPP

#include "bpr/core/filtered_comment.hpp"
#include "bpr/core/suggestion_text_editing.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief RFC-BPR-0118 ([[BPR-2078]]). Suggest the change: the agent writes
/// the replacement for exactly a draft comment's head lines, and the core
/// decides whether that reply may be shown as RFC-BPR-0090's in-pane
/// suggestion. Pure decisions: the material the one tool-free call carries up
/// front, the structured reply read from the answer, and the checks a reply
/// passes before it is shown (P05). Nothing here reads a file, asks an agent
/// or publishes.
namespace bpr::core {
	/// \brief The replacement a reply may carry: RFC-BPR-0090's bound for the
	/// change.
	inline constexpr std::size_t maximum_suggested_replacement_bytes = 64U * 1024U;
	/// \brief The lines asked about, as the request carries them.
	inline constexpr std::size_t maximum_suggest_lines_bytes = 32U * 1024U;
	/// \brief The draft comment's words the request carries.
	inline constexpr std::size_t maximum_suggest_comment_bytes = 16U * 1024U;
	/// \brief Each side of the surroundings the request carries.
	inline constexpr std::size_t maximum_suggest_context_bytes = 8U * 1024U;
	/// \brief How many lines of surroundings each side holds at most.
	inline constexpr std::uint32_t suggest_context_lines = 12U;
	/// \brief The longest answer a reply is read from.
	inline constexpr std::size_t maximum_suggest_answer_bytes = 256U * 1024U;
	/// \brief A path, as the subject's own bound has it.
	inline constexpr std::size_t maximum_suggest_path_bytes = 4096U;

	/// \brief One Suggest the change run's material, up front.
	/// \details Lines are one-based, as the agent reads and writes them.
	/// `Lines` is the head's exact text of lines `FirstLine` to `LastLine`
	/// with every line's own ending except the last's: the text a reply's
	/// `current` must equal byte for byte. `Before` and `After` are the lines
	/// around them, exact, each within its bound.
	struct supplement_suggest_change
	{
		std::string   Path;
		std::string   HeadCommit;
		std::uint32_t FirstLine = 0U;
		std::uint32_t LastLine  = 0U;
		std::string   Comment;
		std::string   Lines;
		std::string   Before;
		std::string   After;

		auto operator<=>(const supplement_suggest_change&) const = default;
	};

	/// \brief Whether a supplement engages exactly what it means, within its bounds.
	[[nodiscard]] auto valid(const supplement_suggest_change& aMaterial) -> bool;

	/// \brief The exact text of some lines as a reply states it: every line's
	/// own ending except the last's.
	[[nodiscard]] auto suggest_lines_text(std::span<const text_line> aLines) -> std::string;

	/// \brief What the host holds for one press, before bounds.
	struct suggest_change_material_input
	{
		std::string Path;
		std::string HeadCommit;
		/// The draft's words, without any suggestion block of its own.
		std::string Comment;
		/// The whole head file, as lines with their endings.
		std::span<const text_line> Head;
		/// The zero-based first and last lines of the draft's range.
		std::uint32_t First = 0U;
		std::uint32_t Last  = 0U;
	};

	/// \brief Bounds one press's material, deterministically, or refuses it.
	/// \details Refused (none) with no path, no head commit, a range outside
	/// the head, lines past their bound, or text that is not UTF-8. The
	/// comment is cut at a character boundary within its bound; the
	/// surroundings keep at most `suggest_context_lines` whole lines each
	/// side, nearest first, within their bound.
	[[nodiscard]] auto bound_suggest_change_material(const suggest_change_material_input& aInput) -> std::optional<supplement_suggest_change>;

	/// \brief The structured reply: the range, the text the agent says is
	/// there now, and the text to put in its place. One-based lines.
	struct suggested_replacement
	{
		std::uint32_t FirstLine = 0U;
		std::uint32_t LastLine  = 0U;
		std::string   Current;
		std::string   Replacement;

		auto operator<=>(const suggested_replacement&) const = default;
	};

	/// \brief Why a reply is not shown as a suggestion.
	enum class suggest_change_refusal : std::uint8_t {
		none,
		/// The answer holds no reply object with the four fields.
		unreadable,
		/// The head moved the lines while the run was under way.
		head_moved,
		/// The reply's range is not the draft's.
		range_differs,
		/// The text the reply says is there differs from the head's lines.
		current_differs,
		/// The replacement is the lines as they are.
		identical,
		/// The replacement is not valid UTF-8.
		invalid_text,
		/// The replacement is longer than RFC-BPR-0090's bound for a change.
		too_large,
		/// GitHub could not take the change as a suggestion here.
		not_suggestible,
	};
	inline constexpr std::size_t suggest_change_refusal_count = 9U;
	static_assert(static_cast<std::size_t>(suggest_change_refusal::not_suggestible) + 1U == suggest_change_refusal_count,
				  "suggest_change_refusal_count must cover every declared refusal");

	[[nodiscard]] auto stable_name(suggest_change_refusal aRefusal) noexcept -> std::string_view;

	/// \brief Reads the reply from an answer, or none.
	/// \details The answer is one JSON object, alone or in one fenced block
	/// (with or without a language) and nothing else but white space:
	/// `{"first_line": n, "last_line": n, "current": "...", "replacement": "..."}`.
	/// Unknown members, a missing or mistyped one, a line number below one or
	/// past 2^32-1, invalid UTF-8 and any text outside the object are refused.
	[[nodiscard]] auto read_suggested_replacement(std::string_view aAnswer) -> std::optional<suggested_replacement>;

	/// \brief Everything one decision reads.
	struct suggest_change_input
	{
		/// The agent's answer as it arrived.
		std::string_view Answer;
		/// The zero-based first and last lines the press asked about.
		std::uint32_t First = 0U;
		std::uint32_t Last  = 0U;
		/// The head's lines as the press read them, with their endings.
		std::span<const text_line> Asked;
		/// The head the press read is still the one under review and its
		/// comparison still the draft's.
		bool SameHead = true;
		/// The whole head file now, as lines with their endings.
		std::span<const text_line> Head;
		/// The draft's words, kept as written.
		std::string_view Prose;
		/// On a filtered diff whose right side is not the head: where
		/// `place_filtered_comment` puts the anchor line. None otherwise.
		std::optional<filtered_comment_target> Filtered;
	};

	/// \brief What one decision came to.
	struct suggest_change_decision
	{
		suggest_change_refusal Refusal = suggest_change_refusal::unreadable;
		/// Accepted: the editable range's lines the in-pane edit opens with,
		/// each with the endings the edit itself would give them.
		std::vector<text_line> Span;
		/// Accepted: the block's lines, without their endings.
		std::vector<std::string> Block;

		[[nodiscard]] auto accepted() const noexcept -> bool { return Refusal == suggest_change_refusal::none; }

		auto operator<=>(const suggest_change_decision&) const = default;
	};

	/// \brief Decides whether a reply is shown as a suggestion (P05).
	/// \details In order, the first that fails is the reason: the reply reads
	/// (`unreadable`); the head the press read is the head now and holds the
	/// same lines there (`head_moved`), so nothing is re-anchored by guesswork;
	/// the reply's range is the draft's (`range_differs`); its `current`
	/// equals the head's lines byte for byte (`current_differs`); the
	/// replacement is valid UTF-8 (`invalid_text`) within its bound
	/// (`too_large`) and differs from the lines (`identical`); and the plan
	/// RFC-BPR-0090 makes of it is a placeable change whose body reads back
	/// to the same block (`not_suggestible`).
	///
	/// The replacement holds the lines without the last one's ending: a
	/// single final line ending in it is not a line of its own. In a CRLF
	/// range a CR before each line break is that line's ending, not content.
	/// An empty replacement deletes the lines.
	[[nodiscard]] auto decide_suggested_change(const suggest_change_input& aInput) -> suggest_change_decision;
} // namespace bpr::core

#endif
