#ifndef BPR_SRC_ADAPTERS_QT_SRC_MARKDOWN_GITHUB_MATH_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_MARKDOWN_GITHUB_MATH_HPP

#include <QColor>
#include <QString>
#include <QStringView>

#include <optional>
#include <vector>

/// \file
/// \brief [[BPR-1792]]. The inline math GitHub renders that reads as styled
/// text: `$\color{gray}\text{…}$`, which pull request templates use for
/// coloured notes.
/// \details Not a TeX engine. It reads `\color`, `\textcolor`, the
/// text-family commands (`\text`, `\textbf`, `\textit`, `\texttt`, `\mathrm`
/// and their kin), groups, spacing commands and escaped characters, and
/// letters, digits and punctuation in math mode. Anything else (a fraction,
/// a script, a symbol) is refused whole, so the formula stays as written.
namespace bpr::qt::detail {
	/// \brief One run of equally styled text a formula shows.
	struct math_run
	{
		QString               Text;
		std::optional<QColor> Colour;
		bool                  Bold                                      = false;
		bool                  Italic                                    = false;
		bool                  Mono                                      = false;
		auto                  operator==(const math_run&) const -> bool = default;
	};

	/// \brief The runs the TeX between a formula's delimiters shows, or
	/// nothing when it holds what this reader does not read.
	[[nodiscard]] auto read_inline_math(QStringView aTeX) -> std::optional<std::vector<math_run>>;

	/// \brief The formula that starts at `aAt`, a `$`, in `aText` up to
	/// `aEnd`: GitHub's `$…$`, where the opening `$` is not followed by a
	/// space, the closing one not preceded by one nor followed by a digit, or
	/// its `` $`…`$ `` form. A `$$` is display math and is not read.
	struct math_span
	{
		qsizetype   End = 0;
		QStringView TeX;
	};
	[[nodiscard]] auto inline_math_at(const QString& aText, qsizetype aAt, qsizetype aEnd) -> std::optional<math_span>;
} // namespace bpr::qt::detail

#endif
