#ifndef BPR_SRC_ADAPTERS_QT_SRC_MARKDOWN_GITHUB_HTML_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_MARKDOWN_GITHUB_HTML_HPP

#include <QString>
#include <QStringView>

#include <optional>
#include <utility>
#include <vector>

/// \file
/// \brief [[BPR-1544]]. The HTML a GitHub body may carry, as the reader treats
/// it, and the emoji shortcode table (RFC-BPR-0036 amendment 1).
/// \details A bounded tag recognizer and GitHub's allow-list, not an HTML
/// parser. Nothing here loads a resource or decides a link: an `<a href>` is
/// kept only when core says its destination could ever be admitted, and its
/// activation still goes through the application's admission. `<img>`,
/// `<picture>` and `<source>` stay literal text for RFC-BPR-0084, which finds
/// them in the document, and so does an `<a>` around one of them.
namespace bpr::qt::detail {
	/// \brief One recognized tag: an opening tag with its attributes, or a
	/// closing tag. CommonMark's raw HTML shape, at most 8 KiB long.
	struct html_tag
	{
		qsizetype                                Length = 0;
		QString                                  Name;
		bool                                     Closing     = false;
		bool                                     SelfClosing = false;
		std::vector<std::pair<QString, QString>> Attributes;

		[[nodiscard]] auto attribute(const QString& aName) const -> std::optional<QString>;
	};

	/// \brief The tag that starts `aText`, which begins with `<`.
	[[nodiscard]] auto html_tag_at(QStringView aText) -> std::optional<html_tag>;

	/// \brief An `align` GitHub keeps (`left`, `center`, `right`, `justify`),
	/// lowercased, or empty.
	[[nodiscard]] auto html_alignment(const html_tag& aTag) -> QString;

	/// \brief How the reader treats a tag, by name.
	enum class html_kind : int {
		/// Left as written: GFM's tag filter (`script`, `style`, `iframe` and
		/// the rest) and the image tags RFC-BPR-0084 reads.
		literal,
		/// Removed, its text kept, as GitHub's sanitizer does.
		drop,
		bold,
		italic,
		strike,
		underline,
		subscript,
		superscript,
		keyboard,
		code,
		link,
		line_break,
		/// Structure: paragraphs, divisions, headings, rules, quotes, lists,
		/// tables, preformatted text.
		block,
		/// `<mark>`, highlighted as GitHub's attention tint.
		highlight,
		/// [[BPR-1792]]. A formula's `\color`; never a tag's.
		colour,
	};

	[[nodiscard]] auto html_kind_of(const QString& aName) -> html_kind;

	/// \brief Character references in an attribute value, decoded.
	[[nodiscard]] auto decode_html_entities(QStringView aValue) -> QString;

	/// \brief The destination an `<a>` keeps, when core says it could ever be
	/// admitted.
	[[nodiscard]] auto kept_html_link(const html_tag& aTag) -> std::optional<QString>;

	/// \brief Whether the text after an opening `<a>` is one of the image
	/// tags RFC-BPR-0084 links.
	[[nodiscard]] auto html_link_holds_image(QStringView aAfter) -> bool;

	/// \brief The inline mark for a formatting tag, or for `<br>`; `aHrefs`
	/// collects a kept link's destination.
	/// \returns The replacement text: a mark, or empty to drop the tag.
	[[nodiscard]] auto inline_html_mark(const html_tag& aTag, html_kind aKind, std::vector<QString>& aHrefs, std::vector<bool>& aLinks) -> QString;

	/// \brief One HTML block, sanitized: structure as canonical HTML with only
	/// allowed attributes, formatting as the reader's inline marks, text and
	/// every other tag escaped or dropped. The result is safe for Qt's HTML
	/// import: no resource, style, script, form or event attribute.
	[[nodiscard]] auto sanitize_html_block(QStringView aBlock, std::vector<QString>& aHrefs) -> QString;

	/// \brief Whether sanitized HTML shows anything: text or a rule.
	[[nodiscard]] auto html_shows_anything(const QString& aHtml) -> bool;

	/// \brief The emoji for a GitHub shortcode name (`+1`, `tada`), from the
	/// generated gemoji table.
	[[nodiscard]] auto markdown_emoji(QStringView aName) -> std::optional<QString>;

	// The inline marks, Unicode noncharacters the reader reserves.
	inline constexpr auto html_open_mark  = QChar{ 0xFDED };
	inline constexpr auto html_close_mark = QChar{ 0xFDEE };
	inline constexpr auto html_break_mark = QChar{ 0xFDEB };
	inline constexpr auto html_mark_close = QChar{ 0xFDE8 };
	inline constexpr auto html_mark_field = QChar{ 0xFDEC };
} // namespace bpr::qt::detail

#endif
