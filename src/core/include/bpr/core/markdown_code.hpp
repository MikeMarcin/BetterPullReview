#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_MARKDOWN_CODE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_MARKDOWN_CODE_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief RFC-BPR-0036 owner amendment 2 ([[BPR-1562]], [[BPR-1563]],
/// [[BPR-1564]]). Fenced code in a Markdown body, as GitHub draws it: which
/// blocks a body holds, which language an info string names, how a `diff`
/// block's lines are marked, and what a suggested change's rows are.
/// \details Pure text decisions. Nothing here classifies a language, reads a
/// file or draws: the application host classifies a block's text, and the
/// reader draws what these functions decide. Indented code and inline code
/// are never fenced code, so nothing here names them.
namespace bpr::core {
	/// \brief Bounds of the fence scan. A body past them keeps its first
	/// blocks; the rest stay plain.
	inline constexpr std::size_t markdown_code_blocks_per_body = 512U;
	inline constexpr std::size_t markdown_code_container_depth = 16U;
	/// \brief The most bytes of one block's text that is classified.
	inline constexpr std::size_t markdown_code_classified_bytes = 65'536U;

	/// \brief One fenced code block, in the order a body holds them.
	struct markdown_fenced_code
	{
		/// The block's place among the body's fenced blocks, from zero.
		std::uint32_t Ordinal = 0U;
		/// The info string as written after the fence, trimmed.
		std::string Info;
		/// The block's lines, with their container prefixes and the fence's
		/// indentation removed and without line endings.
		std::vector<std::string> Lines;
		/// Whether a closing fence ended it; an unclosed block runs to the end
		/// of its container or of the body.
		bool Closed = false;

		/// \brief The lines joined by LF, which is the text that is classified.
		[[nodiscard]] auto text() const -> std::string;

		auto operator<=>(const markdown_fenced_code&) const = default;
	};

	/// \brief Every fenced code block of a body, as CommonMark finds them
	/// inside block quotes and list items, in document order.
	/// \details A bounded line scan, not a Markdown parser: it follows block
	/// quote markers, list item indentation and fences, and nothing else. A
	/// caller that needs to know a block is the one its renderer drew compares
	/// the lines. At most `markdown_code_blocks_per_body` blocks are returned;
	/// containers nested past `markdown_code_container_depth` hold no fence.
	[[nodiscard]] auto scan_markdown_fenced_code(std::string_view aBody) -> std::vector<markdown_fenced_code>;

	/// \brief What a fenced block is drawn as.
	enum class markdown_code_kind : std::uint8_t {
		/// No language, or one GitHub does not colour.
		plain,
		/// A language whose text is classified.
		language,
		/// `diff`, `patch`, `udiff`: marked line by line.
		diff,
		/// `suggestion`: a suggested change in a line comment.
		suggestion,
	};

	/// \brief The language an info string names, as GitHub reads it.
	/// \details `Selector` is a file name whose packaged syntax definition is
	/// the language's (`code.cpp`, `CMakeLists.txt`), so the reader's code is
	/// classified by the same definition a file of that language is in the
	/// review panes. Empty for every kind but `language`.
	struct markdown_code_language
	{
		markdown_code_kind Kind = markdown_code_kind::plain;
		std::string_view   Name;
		std::string_view   Selector;

		auto operator<=>(const markdown_code_language&) const = default;
	};

	/// \brief The language of an info string: its first word, ASCII
	/// case-insensitive, cut at its first comma, looked up among GitHub's
	/// Linguist names and aliases; a leading dot names an extension.
	[[nodiscard]] auto markdown_code_language_of(std::string_view aInfo) -> markdown_code_language;

	/// \brief [[BPR-PENDING-AQ6]], owner look v004. The file name inline
	/// code about a change is coloured as: the selector of the language most
	/// of `aPaths` are in, by extension, leaving out prose and data
	/// (Markdown, JSON, YAML, TOML, INI, XML, reStructuredText, AsciiDoc).
	/// A `.h` header counts as C++ when any C++ file is there. The earliest
	/// language wins a tie; empty when no path is in a coloured language.
	[[nodiscard]] auto markdown_inline_code_selector(std::span<const std::string> aPaths) -> std::string;

	/// \brief How GitHub's diff grammar marks a run of a `diff` block's line.
	enum class markdown_diff_mark_kind : std::uint8_t {
		/// `+`, `>` and `+++ file`: GitHub's inserted text.
		inserted,
		/// `-`, `<` and `--- file`/`*** file`: GitHub's deleted text.
		deleted,
		/// `!`: GitHub's changed text.
		changed,
		/// `@@ … @@`, `1,2c3`, `*** 1,2 ****`: the range, in bold.
		range,
		/// `diff --git …`, `diff -u …`: a header, in the constant colour.
		header,
		/// `---`, `***************`, 67 `=`: a separator, in bold.
		separator,
	};

	/// \brief One marked run of a line, in bytes.
	struct markdown_diff_mark
	{
		std::size_t             Begin  = 0U;
		std::size_t             Length = 0U;
		markdown_diff_mark_kind Kind   = markdown_diff_mark_kind::inserted;

		auto operator<=>(const markdown_diff_mark&) const = default;
	};

	/// \brief The marked runs of one line of a `diff` block, in order; none
	/// for a context line or anything the grammar does not name.
	/// \details GitHub's `source.diff` grammar, rule for rule and in its
	/// order: a line takes the first rule that matches it.
	[[nodiscard]] auto markdown_diff_marks(std::string_view aLine) -> std::vector<markdown_diff_mark>;

	/// \brief One row of a suggested change.
	struct markdown_suggestion_row
	{
		bool Added = false;
		/// The file line the row stands for, from one; none without the
		/// commented lines.
		std::optional<std::uint32_t> Number;
		std::string                  Text;
		/// The changed words, in bytes of `Text`; empty when none is marked.
		std::size_t ChangedBegin = 0U;
		std::size_t ChangedEnd   = 0U;

		auto operator<=>(const markdown_suggestion_row&) const = default;
	};

	/// \brief The commented lines of a suggestion, when the reader has them.
	struct markdown_suggestion_context
	{
		/// The first commented line, from one.
		std::uint32_t            FirstLine = 1U;
		std::vector<std::string> Lines;

		auto operator<=>(const markdown_suggestion_context&) const = default;
	};

	/// \brief GitHub's rows of a suggested change: every commented line
	/// removed, then every suggested line added, both numbered from the first
	/// commented line; the changed words marked when as many lines are added
	/// as removed. Without the commented lines, the added rows alone,
	/// unnumbered.
	[[nodiscard]] auto markdown_suggestion_rows(std::span<const std::string> aSuggested, const std::optional<markdown_suggestion_context>& aContext)
		-> std::vector<markdown_suggestion_row>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_MARKDOWN_CODE_HPP
