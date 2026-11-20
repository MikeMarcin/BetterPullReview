#ifndef BPR_SRC_ADAPTERS_QT_SRC_CONVERSATION_PREVIEW_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_CONVERSATION_PREVIEW_HPP

#include <QFont>
#include <QPoint>
#include <QString>
#include <QStringList>

#include <utility>
#include <vector>

class QColor;
class QPainter;

/// \file
/// \brief [[BPR-2171]]. What a closed note shows of its body: its first
/// paragraph as words, and one word for each kind of block that is not words.
///
/// \details The owner, 2026-11-13: "it's only the collapsed cards with
/// formatting issues". A closed note printed its Markdown source on one
/// line. It now reads the body as the open card does (Qt's GitHub dialect
/// after `import_github_markdown`'s preparation) and keeps the first
/// paragraph's words: emphasis, link and heading marks are dropped and their
/// words kept, inline code keeps the code font. A body that opens with a
/// heading shows the heading in semibold, then its first paragraph; one that
/// opens with a list shows its first item. Each block that is not words is
/// named once per kind, in order: List, Quote, Image, Table, Details,
/// Suggestion, or the code's language. Discuss, Review's Comments and
/// Overview's Comments all draw closed notes with it.
namespace bpr::qt::detail {
	/// \brief A closed note's preview.
	struct note_preview
	{
		/// The words, on one line.
		QString Text;
		/// Ranges of `Text` in the code font, as start and length.
		std::vector<std::pair<int, int>> Code;
		/// Ranges of `Text` in semibold: a Conventional Comments header, a
		/// leading heading.
		std::vector<std::pair<int, int>> Strong;
		/// One word per kind of block that is not words, in order.
		QStringList Tokens;
		/// Words were left out after `Text`: it ends with an ellipsis.
		bool Omitted = false;

		auto operator==(const note_preview&) const -> bool = default;
	};

	/// \brief The preview of a body, \p aHeaderLength characters of which are
	/// its recognized Conventional Comments header.
	[[nodiscard]] auto preview_of(const QString& aBody, qsizetype aHeaderLength) -> note_preview;

	/// \brief The token naming a fenced code block's language: "C++" for
	/// `cpp`, "Suggestion" for GitHub's suggested change, "Code" for none.
	[[nodiscard]] auto code_token(const QString& aLanguage) -> QString;

	/// \brief [[BPR-2300]]. A run of a preview's words on one line: plain,
	/// inline code, semibold, or a search's match.
	struct preview_run
	{
		QString Text;
		bool    Code   = false;
		bool    Strong = false;
		bool    Match  = false;

		auto operator==(const preview_run&) const -> bool = default;
	};

	/// \brief A preview's words as runs, split at its code and semibold
	/// ranges, in order.
	[[nodiscard]] auto preview_runs(const note_preview& aPreview) -> std::vector<preview_run>;

	/// \brief [[BPR-2300]]. The runs that one line \p aWidth wide shows,
	/// measured by advance: inline code in \p aCodeFont, semibold in
	/// \p aFont's demibold, the rest in \p aFont. Cut at a word boundary,
	/// never inside inline code unless it opens the line, the ellipsis in
	/// place of a sentence's last stop. It ends with an ellipsis when words
	/// were cut or when \p aOmitted says the preview left words out.
	[[nodiscard]] auto fit_preview_line(std::vector<preview_run> aRuns, bool aOmitted, const QFont& aFont, const QFont& aCodeFont, int aWidth) -> std::vector<preview_run>;

	/// \brief [[BPR-2171]]'s quiet tokens on one line: their font, the room
	/// the chips take (without `preview_token_lead`, the gap after words)
	/// and their painting from \p aLeft, centred on \p aMiddle.
	[[nodiscard]] auto   preview_token_font(const QFont& aFont) -> QFont;
	[[nodiscard]] auto   preview_tokens_width(const QStringList& aTokens, const QFont& aFont) -> int;
	inline constexpr int preview_token_lead = 6;
	void                 paint_preview_tokens(QPainter& aPainter, const QStringList& aTokens, const QFont& aFont, int aLeft, int aMiddle, const QColor& aRule, const QColor& aInk);
}

#endif
