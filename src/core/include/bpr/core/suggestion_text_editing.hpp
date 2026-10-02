#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_SUGGESTION_TEXT_EDITING_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_SUGGESTION_TEXT_EDITING_HPP

#include "bpr/core/diff_compute.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

/// \file
/// \brief The editing model of the focused suggestion editor (RFC-BPR-0055,
/// BPR-1156): exact text, several carets, box selections, virtual space and
/// ordinary undo.
/// \details This is a deliberately small code-proposal editor, not an IDE.
/// The text is UTF-8 kept as lines with their own endings, so CRLF, lone CR,
/// tabs and a missing final newline survive every edit. Positions are visual
/// columns: a tab advances to the next tab stop and every other scalar value
/// is one column; a caret may sit beyond the end of its line in virtual
/// space, which becomes real spaces only when text is inserted there. A box
/// gesture (Alt+Shift+Arrow) makes one caret per line at the same visual
/// columns; End and Shift+End then act on each caret's own line and never
/// cross a line break. The Qt view owns painting, focus, IME composition and
/// the clipboard, and passes semantic edits here.
namespace bpr::core {
	struct text_line
	{
		std::string      Content;
		text_line_ending Ending                              = text_line_ending::none;
		auto             operator<=>(const text_line&) const = default;
	};

	/// \brief A line and a visual column, possibly beyond the line's end.
	struct text_point
	{
		std::uint64_t Line                                 = 0;
		std::uint64_t Column                               = 0;
		auto          operator<=>(const text_point&) const = default;
	};

	/// \brief One caret: a selection from `Anchor` to `Head`, empty when equal.
	struct text_caret
	{
		text_point Anchor;
		text_point Head;
		auto       operator<=>(const text_caret&) const = default;

		[[nodiscard]] auto empty() const noexcept -> bool
		{
			return Anchor == Head;
		}
	};

	inline constexpr std::uint32_t default_suggestion_tab_width = 4U;
	/// A focused editor holds small proposals.
	inline constexpr std::size_t maximum_suggestion_editor_bytes  = 64U * 1024U;
	inline constexpr std::size_t maximum_suggestion_editor_carets = 1024U;
	inline constexpr std::size_t maximum_suggestion_undo_steps    = 256U;

	struct suggestion_text_state
	{
		/// Never empty: an empty text is one empty line.
		std::vector<text_line> Lines{ text_line{ } };
		/// Sorted, non-overlapping, never empty.
		std::vector<text_caret> Carets{ text_caret{ } };
		/// The caret that owns IME composition and scrolling.
		std::size_t   Primary  = 0;
		std::uint32_t TabWidth = default_suggestion_tab_width;
		/// The fixed corner of the box selection in progress, if any.
		std::optional<text_point> BoxAnchor;
		auto                      operator<=>(const suggestion_text_state&) const = default;
	};

	enum class text_motion : std::uint8_t {
		left,
		right,
		up,
		down,
		/// Home: first non-blank column, then column 0.
		line_start,
		/// End: the end of the caret's own line.
		line_end,
		document_start,
		document_end,
		word_left,
		word_right,
	};

	/// \brief Moves every caret, extending its selection when asked.
	struct move_carets
	{
		text_motion Motion = text_motion::right;
		bool        Extend = false;
	};
	/// \brief Alt+Shift+Arrow: grows the box from its fixed corner.
	struct extend_box
	{
		text_motion Motion = text_motion::down;
	};
	/// \brief A click (or Shift+click) places the only caret.
	struct place_caret
	{
		text_point At;
		bool       Extend = false;
	};
	/// \brief Alt+click adds a caret.
	struct add_caret
	{
		text_point At;
	};
	/// \brief Typed or IME-committed text, inserted at every caret. A line
	/// break in it becomes the text's own line ending.
	struct insert_text
	{
		std::string Text;
	};
	struct insert_line_break
	{
	};
	/// \brief Paste. Several lines onto as many carets go one per caret; a box
	/// copy onto one caret becomes a box from that caret down.
	struct paste_text
	{
		std::string Text;
		bool        Box = false;
	};
	struct delete_backward
	{
	};
	struct delete_forward
	{
	};
	struct select_all
	{
	};
	/// \brief Escape: only the primary caret remains.
	struct collapse_carets
	{
	};
	struct undo_edit
	{
	};
	struct redo_edit
	{
	};
	using suggestion_edit = std::variant<move_carets, extend_box, place_caret, add_caret, insert_text, insert_line_break, paste_text, delete_backward, delete_forward,
										 select_all, collapse_carets, undo_edit, redo_edit>;

	/// \brief What copying the selections puts on the clipboard.
	struct copied_text
	{
		std::string Text;
		/// Several selections, one per line: pasting onto one caret makes a box.
		bool Box                                   = false;
		auto operator<=>(const copied_text&) const = default;
	};

	/// \brief The editor: state plus its undo history.
	class suggestion_text_editor
	{
	public:
		/// \brief An editor over exact bytes, with one caret at the start.
		explicit suggestion_text_editor(std::string_view aText = { }, std::uint32_t aTabWidth = default_suggestion_tab_width);

		/// \brief Applies one edit. Returns true when the text or carets changed.
		/// \details An edit that would exceed the byte or caret bound is refused
		/// whole and changes nothing.
		auto apply(const suggestion_edit& aEdit) -> bool;

		[[nodiscard]] auto state() const noexcept -> const suggestion_text_state&;
		/// \brief The exact bytes, with every line's own ending.
		[[nodiscard]] auto text() const -> std::string;
		/// \brief The selected text of every non-empty selection.
		[[nodiscard]] auto copy() const -> copied_text;
		[[nodiscard]] auto can_undo() const noexcept -> bool;
		[[nodiscard]] auto can_redo() const noexcept -> bool;

		/// \brief The visual width of a line, in columns.
		[[nodiscard]] auto line_width(std::uint64_t aLine) const -> std::uint64_t;
		/// \brief The byte offset of a visual column in a line and how many
		/// columns of virtual space lie beyond the line's end.
		[[nodiscard]] auto locate(const text_point& aPoint) const -> std::pair<std::size_t, std::uint64_t>;

	private:
		void remember(bool aCoalesce);

		suggestion_text_state              State;
		std::vector<suggestion_text_state> Undo;
		std::vector<suggestion_text_state> Redo;
		/// The last edit was a single typed character that later typing may join.
		bool Typing = false;
	};

	/// \brief Splits exact bytes into lines with their endings.
	[[nodiscard]] auto split_text_lines(std::string_view aText) -> std::vector<text_line>;
	/// \brief Joins lines back into exact bytes.
	[[nodiscard]] auto join_text_lines(const std::vector<text_line>& aLines) -> std::string;
	/// \brief The bytes a proposal is highlighted as: `aFile` with
	/// [`aStart`, `aEnd`) replaced by `aReplacement` (BPR-1156).
	/// \details The proposal is classified inside its whole file rather than
	/// on its own, so a selection that begins inside a comment or a string
	/// reads the way the file does. Nothing when the range lies outside the file.
	[[nodiscard]] auto splice_text(std::string_view aFile, std::uint64_t aStart, std::uint64_t aEnd, std::string_view aReplacement) -> std::optional<std::string>;
}

#endif
