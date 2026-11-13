#ifndef BPR_SRC_ADAPTERS_QT_SRC_CONVERSATION_PREVIEW_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_CONVERSATION_PREVIEW_HPP

#include <QString>
#include <QStringList>

#include <utility>
#include <vector>

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
}

#endif
