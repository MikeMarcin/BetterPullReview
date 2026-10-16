#ifndef BPR_SRC_ADAPTERS_QT_SRC_MARKDOWN_GITHUB_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_MARKDOWN_GITHUB_HPP

#include <QColor>
#include <QFont>
#include <QString>
#include <QTextFormat>

#include <array>
#include <vector>

class QPalette;
class QTextDocument;

/// \file
/// \brief [[BPR-1543]]. What a GitHub pull request body shows that Qt's
/// Markdown reader does not: hidden HTML comments, single newlines as line
/// breaks, alerts and footnotes (RFC-BPR-0036 amendment 1).
/// \details Two halves around `QTextDocument::setMarkdown`. Preparation
/// rewrites the source line by line before the import: it removes comments,
/// marks each soft line break, moves footnote definitions to a section at the
/// end and marks alert and footnote positions with characters Unicode reserves
/// for internal use. Presentation turns those marks into native document
/// structure after the import. Code, fenced or indented, and inline code
/// spans are never rewritten. A body that already holds a reserved mark
/// character is read without that transformation, so authored text can
/// never be taken for a mark. The original body stays the source of truth;
/// only the rendered document is affected.
namespace bpr::qt::detail {
	/// \brief The five GitHub alert kinds, in GitHub's order.
	enum class markdown_alert : int {
		none = 0,
		note,
		tip,
		important,
		warning,
		caution,
	};

	/// Block property: the `markdown_alert` an alert block belongs to.
	inline constexpr auto markdown_alert_property = QTextFormat::UserProperty + 0x0B50;
	/// Block property: which alert of the document, so two alerts in a row
	/// keep their own bars.
	inline constexpr auto markdown_alert_ordinal_property = QTextFormat::UserProperty + 0x0B51;
	/// Block property: set on an alert's title block.
	inline constexpr auto markdown_alert_title_property = QTextFormat::UserProperty + 0x0B52;
	/// Block property: set on every block of the footnotes section.
	inline constexpr auto markdown_footnote_property = QTextFormat::UserProperty + 0x0B53;
	/// Character property: the anchor name an in-body link moves to. Only
	/// presentation sets it; an authored link never carries it.
	inline constexpr auto markdown_internal_target_property = QTextFormat::UserProperty + 0x0B54;

	/// \brief A body rewritten for Qt's GitHub-dialect import, and which marks
	/// it may hold.
	struct prepared_markdown
	{
		QString Text;
		/// Soft breaks are marked; false when the body itself holds the mark.
		bool Breaks = false;
		/// Alerts and footnotes are marked; false when the body holds a
		/// reserved character.
		bool Marks = false;
	};

	/// \brief Rewrites a body for Qt's GitHub-dialect import.
	[[nodiscard]] auto prepare_github_markdown(const QString& aSource) -> prepared_markdown;

	/// \brief Turns the prepared marks in an imported document into line
	/// separators, alert blocks and footnote links.
	void present_github_markdown(QTextDocument& aDocument, const prepared_markdown& aPrepared);

	/// \brief An alert's title as GitHub writes it: "Note", "Tip",
	/// "Important", "Warning" or "Caution".
	[[nodiscard]] auto markdown_alert_title(markdown_alert aAlert) -> QString;

	/// \brief An alert's colour on a palette's base: GitHub's hue for a light
	/// or dark base, moved toward the text colour until it reads at 4.5:1,
	/// or 7:1 in a high-contrast theme.
	[[nodiscard]] auto markdown_alert_colour(markdown_alert aAlert, const QPalette& aPalette) -> QColor;

	/// \brief The width of an alert's bar, in logical pixels.
	inline constexpr qreal markdown_alert_bar_width = 4.0;

	/// \brief The colours and measures GitHub's constructs are drawn with,
	/// derived from the reader's palette and font.
	struct markdown_github_style
	{
		/// Each alert kind's title and bar colour, in `markdown_alert` order.
		std::array<QColor, 5> Alerts;
		/// The footnotes' quiet ink.
		QColor Muted;
		QFont  Base;
		/// Where an alert's text starts, from the bar's left edge.
		qreal AlertIndent = 16.0;
		/// The space above and below an alert.
		qreal AlertGap = 8.0;
	};

	/// \brief The style for a palette and the reader's font.
	[[nodiscard]] auto markdown_github_style_of(const QPalette& aPalette, const QFont& aFont) -> markdown_github_style;

	/// \brief Draws alert blocks clear of their bar with a coloured title,
	/// and the footnotes section in smaller quiet type. Idempotent: run after
	/// every other format refresh.
	void style_github_markdown(QTextDocument& aDocument, const markdown_github_style& aStyle);

	/// \brief One alert's extent in a document, for its bar.
	struct markdown_alert_extent
	{
		int            First = 0;
		int            Last  = 0;
		markdown_alert Alert = markdown_alert::none;
	};

	/// \brief Every alert's first and last block number, in order.
	[[nodiscard]] auto markdown_alert_extents(const QTextDocument& aDocument) -> std::vector<markdown_alert_extent>;
} // namespace bpr::qt::detail

#endif
