#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_SUGGESTION_TEXT_EDITING_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_SUGGESTION_TEXT_EDITING_HPP

#include "bpr/core/diff_compute.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

/// \file
/// \brief The editing model of the focused suggestion editor (RFC-BPR-0055,
/// BPR-1156): exact text, several carets, box selections, virtual space and
/// ordinary undo.
/// \details This is a deliberately small code-proposal editor, not an IDE.
/// The text is UTF-8 kept as lines with their own endings, so CRLF, LF,
/// tabs and a missing final newline survive every edit. As in Git, only LF
/// ends a line and a lone CR is content ([[BPR-1432]]). Positions are display
/// columns of extended grapheme clusters ([[BPR-1438]]): a tab advances to
/// the next tab stop, a wide cluster (CJK, fullwidth forms, emoji) takes two
/// columns, any other cluster one, and a combining mark or joiner adds none
/// to its cluster. Carets step, select and delete whole clusters, and a
/// column inside a wide cluster or a tab means its start. Virtual space, a column beyond the end of a line, belongs
/// to the box gesture alone (Alt+Shift+Arrow), which makes one caret per line
/// at the same visual columns however short a line is; typing there pads the
/// line with spaces. Every other gesture keeps a caret on its line's text: a
/// click past the end lands at the end, Right at the end goes to the next
/// line, and Up and Down aim for the column they started from but stop at a
/// shorter line's end (owner note on composer-layout v005, 2026-10-02). End
/// and Shift+End act on each caret's own line and never cross a line break. Enter keeps the line's indentation and adds a level
/// after an opener (`text_indentation`). The Qt view owns painting, focus,
/// IME composition and the clipboard, and passes semantic edits here.
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
	/// Suggest change edits the whole file in place (owner amendment to
	/// BPR-1156, 2026-10-01), so the editor holds a source file; a larger
	/// file is refused before it is opened, never truncated. The bound is on
	/// editing: a larger text is held whole and read, and no edit applies to it.
	inline constexpr std::size_t maximum_suggestion_editor_bytes  = 1024U * 1024U;
	inline constexpr std::size_t maximum_suggestion_editor_carets = 1024U;
	inline constexpr std::size_t maximum_suggestion_undo_steps    = 256U;
	/// Undo keeps whole snapshots; older steps go once these hold about this many bytes.
	inline constexpr std::size_t maximum_suggestion_undo_bytes = 64U * 1024U * 1024U;

	/// \brief How Enter indents the new line, and how a typed closer outdents.
	/// \details The owner asked for indentation on Enter "like a modern text
	/// editor" (BPR-1156, 2026-10-02). The new line keeps the leading blanks
	/// of the line before the caret; a line whose text before the caret ends
	/// in an opener gets one more `Unit`, and Enter between an opening bracket
	/// and its closer puts the closer on its own line at the old indentation.
	/// Typing a closing bracket on a line of blanks that ends in one `Unit`
	/// removes that `Unit`. Nothing else is reformatted.
	struct text_indentation
	{
		/// One level: a tab, or the file's own run of spaces.
		std::string Unit = "\t";
		/// `{`, `[` and `(` open a level.
		bool Brackets = true;
		/// `:` opens a level (Python, YAML).
		bool Colon                                      = false;
		auto operator<=>(const text_indentation&) const = default;
	};

	/// \brief The indentation Enter uses in one file.
	/// \details The openers come from the path's extension: prose (Markdown,
	/// reStructuredText, AsciiDoc, plain text) opens no level, Python and YAML
	/// open one after `:` as well as after a bracket, and anything else after
	/// a bracket. The unit is the one the text itself uses: a tab when more
	/// lines start with a tab than with a space, otherwise the most common
	/// step by which a line's leading spaces exceed the previous line's. A
	/// text with no indented line uses four spaces for Python, two for YAML
	/// and a tab otherwise.
	[[nodiscard]] auto indentation_for(std::string_view aPath, std::string_view aText) -> text_indentation;

	/// \brief One extended grapheme cluster of a line and the columns it takes.
	struct text_cluster
	{
		std::size_t   Offset = 0;
		std::size_t   Length = 0;
		std::uint64_t Column = 0;
		/// Columns: a tab's run to its stop, otherwise 1 or 2.
		std::uint64_t Width                                  = 0;
		auto          operator<=>(const text_cluster&) const = default;
	};

	/// \brief The clusters of a line's content in order, with their columns.
	/// \details The view paints by these, so its cells are the editor's columns.
	[[nodiscard]] auto text_clusters(std::string_view aContent, std::uint32_t aTabWidth) -> std::vector<text_cluster>;

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
		/// The column each caret aims for while Up and Down move it, kept
		/// across lines too short to reach it; empty after any other gesture.
		std::vector<std::uint64_t> Goals;
		auto                       operator<=>(const suggestion_text_state&) const = default;
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
	/// \brief A double click: the word, run of blanks or punctuation mark at
	/// a point becomes the only selection.
	struct select_word_at
	{
		text_point At;
	};
	/// \brief A triple click: the line at a point, with its line break,
	/// becomes the only selection.
	struct select_line_at
	{
		text_point At;
	};
	/// \brief What a drag after a press selects by.
	enum class selection_unit : std::uint8_t {
		/// After a click: characters.
		character,
		/// After a double click: whole words.
		word,
		/// After a triple click: whole lines.
		line,
	};
	/// \brief The pointer moving with the button held after a press.
	/// \details The selection runs from what the press selected (`Origin`)
	/// to the unit under the pointer, and always keeps all of `Origin`; so a
	/// double-clicked word stays selected while the pointer wanders inside
	/// it, and dragging on grows it by whole words.
	struct drag_selection
	{
		text_caret     Origin;
		text_point     To;
		selection_unit Unit = selection_unit::character;
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
	using suggestion_edit = std::variant<move_carets, extend_box, place_caret, add_caret, select_word_at, select_line_at, drag_selection, insert_text, insert_line_break, paste_text,
										 delete_backward, delete_forward, select_all, collapse_carets, undo_edit, redo_edit>;

	/// \brief Whether an edit can change the text, as opposed to only the carets.
	[[nodiscard]] auto changes_text(const suggestion_edit& aEdit) noexcept -> bool;

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

		/// \brief How Enter indents from now on; the text is unchanged.
		void               set_indentation(text_indentation aIndentation);
		[[nodiscard]] auto indentation() const noexcept -> const text_indentation&;

		/// \brief Applies one edit. Returns true when the text or carets changed.
		/// \details An edit that would exceed the byte or caret bound is refused
		/// whole and changes nothing. A gesture that only moves carets costs
		/// the carets, not the text.
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
		/// \brief The point of byte `aOffset` of the text. An offset inside a
		/// line ending is the end of its line; one past the text is its end.
		[[nodiscard]] auto point_of(std::size_t aOffset) const -> text_point;
		/// \brief The columns of the cluster at a point, from its first column to
		/// the one after it; at or beyond the line's end, the point's column twice.
		[[nodiscard]] auto cluster_span(const text_point& aPoint) const -> std::pair<std::uint64_t, std::uint64_t>;
		/// \brief The byte offset of a point in the text; a point in virtual
		/// space is the end of its line.
		[[nodiscard]] auto byte_offset_of(const text_point& aPoint) const -> std::size_t;

	private:
		void remember(bool aCoalesce);

		suggestion_text_state              State;
		text_indentation                   Indentation;
		std::vector<suggestion_text_state> Undo;
		std::vector<suggestion_text_state> Redo;
		/// The last edit was a single typed character that later typing may join.
		bool Typing = false;
	};

	/// \brief Splits exact bytes into lines with their endings.
	[[nodiscard]] auto split_text_lines(std::string_view aText) -> std::vector<text_line>;
	/// \brief Joins lines back into exact bytes.
	[[nodiscard]] auto join_text_lines(const std::vector<text_line>& aLines) -> std::string;
}

#endif
