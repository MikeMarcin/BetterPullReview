#include "markdown_github_math.hpp"

#include <array>
#include <utility>

namespace bpr::qt::detail {
	namespace {
		/// A formula longer than this, or nested deeper, stays as written.
		constexpr auto tex_limit   = qsizetype{ 2048 };
		constexpr auto group_limit = 16;

		struct math_style
		{
			std::optional<QColor> Colour;
			bool                  Bold    = false;
			bool                  Italic  = false;
			bool                  Mono    = false;
			bool                  Text    = false;
			bool                  Upright = false;
		};

		/// A text-family command: whether its argument is text, and the
		/// style it sets.
		struct family
		{
			QLatin1StringView Name;
			bool              Text   = false;
			bool              Bold   = false;
			bool              Italic = false;
			bool              Mono   = false;
		};

		constexpr auto families = std::array{
			family{ QLatin1StringView{ "text" }, true },
			family{ QLatin1StringView{ "textrm" }, true },
			family{ QLatin1StringView{ "textsf" }, true },
			family{ QLatin1StringView{ "textnormal" }, true },
			family{ QLatin1StringView{ "textup" }, true },
			family{ QLatin1StringView{ "textbf" }, true, true },
			family{ QLatin1StringView{ "textit" }, true, false, true },
			family{ QLatin1StringView{ "textsl" }, true, false, true },
			family{ QLatin1StringView{ "emph" }, true, false, true },
			family{ QLatin1StringView{ "texttt" }, true, false, false, true },
			family{ QLatin1StringView{ "mathrm" }, false },
			family{ QLatin1StringView{ "mathsf" }, false },
			family{ QLatin1StringView{ "mathbf" }, false, true },
			family{ QLatin1StringView{ "mathit" }, false, false, true },
			family{ QLatin1StringView{ "mathtt" }, false, false, false, true },
		};

		auto ascii_letter(const QChar aChar) -> bool
		{
			const auto code = aChar.unicode();
			return (code >= 'a' && code <= 'z') || (code >= 'A' && code <= 'Z');
		}

		class math_reader
		{
		public:
			explicit math_reader(const QStringView aTeX)
				: TeX{ aTeX }
			{ }

			auto read() -> std::optional<std::vector<math_run>>
			{
				if (!group(math_style{ }, false) || At != TeX.size()) {
					return std::nullopt;
				}
				return std::move(Runs);
			}

		private:
			QStringView           TeX;
			qsizetype             At    = 0;
			int                   Depth = 0;
			std::vector<math_run> Runs;

			void append(const QString& aText, const math_style& aStyle, const bool aItalic = false)
			{
				auto run = math_run{ .Colour = aStyle.Colour, .Bold = aStyle.Bold, .Italic = aStyle.Italic || aItalic, .Mono = aStyle.Mono };
				if (!Runs.empty()) {
					auto& last = Runs.back();
					if (last.Colour == run.Colour && last.Bold == run.Bold && last.Italic == run.Italic && last.Mono == run.Mono) {
						last.Text += aText;
						return;
					}
				}
				run.Text = aText;
				Runs.push_back(std::move(run));
			}

			void skip_spaces()
			{
				while (At < TeX.size() && TeX[At].isSpace()) {
					++At;
				}
			}

			auto command() -> QString
			{
				const auto begin = At;
				if (At < TeX.size() && ascii_letter(TeX[At])) {
					while (At < TeX.size() && ascii_letter(TeX[At])) {
						++At;
					}
				} else if (At < TeX.size()) {
					++At;
				}
				return TeX.mid(begin, At - begin).toString();
			}

			/// `{…}` without groups inside it: a colour's name.
			auto plain_argument() -> std::optional<QString>
			{
				skip_spaces();
				if (At >= TeX.size() || TeX[At] != QLatin1Char{ '{' }) {
					return std::nullopt;
				}
				const auto close = TeX.indexOf(QLatin1Char{ '}' }, At + 1);
				if (close < 0 || TeX.mid(At + 1, close - At - 1).contains(QLatin1Char{ '{' })) {
					return std::nullopt;
				}
				auto value = TeX.mid(At + 1, close - At - 1).trimmed().toString();
				At         = close + 1;
				return value;
			}

			/// `\color{…}`'s name: one CSS colour name or `#rgb` / `#rrggbb`.
			static auto colour_of(const QString& aName) -> std::optional<QColor>
			{
				if (aName.isEmpty() || aName.size() > 32) {
					return std::nullopt;
				}
				const auto colour = QColor::fromString(aName);
				if (!colour.isValid()) {
					return std::nullopt;
				}
				return colour;
			}

			/// A braced argument read in `aStyle`.
			auto braced(const math_style& aStyle) -> bool
			{
				skip_spaces();
				if (At >= TeX.size() || TeX[At] != QLatin1Char{ '{' }) {
					return false;
				}
				++At;
				return group(aStyle, true);
			}

			auto group(math_style aStyle, const bool aBraced) -> bool
			{
				if (++Depth > group_limit) {
					return false;
				}
				auto spaced = false;
				while (At < TeX.size()) {
					const auto ch = TeX[At];
					if (ch == QLatin1Char{ '}' }) {
						if (!aBraced) {
							return false;
						}
						++At;
						--Depth;
						return true;
					}
					if (ch == QLatin1Char{ '{' }) {
						++At;
						if (!group(aStyle, true)) {
							return false;
						}
						continue;
					}
					if (ch.isSpace()) {
						++At;
						// Math mode drops spaces; text mode folds them to one.
						if (aStyle.Text && !spaced) {
							append(QStringLiteral(" "), aStyle);
							spaced = true;
						}
						continue;
					}
					spaced = false;
					if (ch == QLatin1Char{ '~' }) {
						++At;
						append(QString{ QChar::Nbsp }, aStyle);
						continue;
					}
					if (ch == QLatin1Char{ '\\' }) {
						++At;
						if (!control(aStyle)) {
							return false;
						}
						continue;
					}
					// Scripts, alignment and a nested formula are TeX this
					// reader does not draw.
					if (ch == QLatin1Char{ '^' } || ch == QLatin1Char{ '_' } || ch == QLatin1Char{ '&' } || ch == QLatin1Char{ '$' } || ch == QLatin1Char{ '#' } || ch == QLatin1Char{ '%' }) {
						return false;
					}
					++At;
					append(QString{ ch }, aStyle, !aStyle.Text && !aStyle.Upright && ch.isLetter());
				}
				--Depth;
				return !aBraced;
			}

			auto control(math_style& aStyle) -> bool
			{
				const auto name = command();
				if (name.isEmpty()) {
					return false;
				}
				if (name == QStringLiteral("color")) {
					const auto value  = plain_argument();
					const auto colour = value ? colour_of(*value) : std::nullopt;
					if (!colour) {
						return false;
					}
					// The rest of the group takes the colour.
					aStyle.Colour = colour;
					return true;
				}
				if (name == QStringLiteral("textcolor")) {
					const auto value  = plain_argument();
					const auto colour = value ? colour_of(*value) : std::nullopt;
					if (!colour) {
						return false;
					}
					auto inner   = aStyle;
					inner.Colour = colour;
					return braced(inner);
				}
				for (const auto& entry : families) {
					if (name != entry.Name) {
						continue;
					}
					auto inner    = aStyle;
					inner.Text    = entry.Text;
					inner.Upright = !entry.Text;
					inner.Bold    = aStyle.Bold || entry.Bold;
					inner.Italic  = aStyle.Italic || entry.Italic;
					inner.Mono    = aStyle.Mono || entry.Mono;
					return braced(inner);
				}
				if (name == QStringLiteral(" ") || name == QStringLiteral(",") || name == QStringLiteral(":") || name == QStringLiteral(";") || name == QStringLiteral(">")
					|| name == QStringLiteral("quad") || name == QStringLiteral("qquad")) {
					append(QStringLiteral(" "), aStyle);
					return true;
				}
				if (name == QStringLiteral("!")) {
					return true;
				}
				if (name.size() == 1 && QStringLiteral("{}$%&#_").contains(name.front())) {
					append(name, aStyle);
					return true;
				}
				return false;
			}
		};
	} // namespace

	auto read_inline_math(const QStringView aTeX) -> std::optional<std::vector<math_run>>
	{
		if (aTeX.isEmpty() || aTeX.size() > tex_limit) {
			return std::nullopt;
		}
		return math_reader{ aTeX }.read();
	}

	auto inline_math_at(const QString& aText, const qsizetype aAt, const qsizetype aEnd) -> std::optional<math_span>
	{
		const auto dollar = QLatin1Char{ '$' };
		if (aAt + 2 >= aEnd || aText[aAt] != dollar || (aAt > 0 && aText[aAt - 1] == dollar) || aText[aAt + 1] == dollar) {
			return std::nullopt;
		}
		// `` $`…`$ ``: a code span's backticks between the dollars.
		if (aText[aAt + 1] == QLatin1Char{ '`' }) {
			auto run = aAt + 1;
			while (run < aEnd && aText[run] == QLatin1Char{ '`' }) {
				++run;
			}
			const auto ticks = QString{ run - aAt - 1, QLatin1Char{ '`' } };
			const auto close = aText.indexOf(ticks + dollar, run);
			if (close < 0 || close + ticks.size() + 1 > aEnd || close == run || QStringView{ aText }.mid(run, close - run).contains(QLatin1Char{ '\n' })) {
				return std::nullopt;
			}
			return math_span{ .End = close + ticks.size() + 1, .TeX = QStringView{ aText }.mid(run, close - run) };
		}
		if (aText[aAt + 1].isSpace()) {
			return std::nullopt;
		}
		for (auto at = aAt + 1; at < aEnd && at - aAt <= tex_limit; ++at) {
			const auto ch = aText[at];
			if (ch == QLatin1Char{ '\\' }) {
				++at;
				continue;
			}
			// A formula neither crosses a code span nor a line.
			if (ch == QLatin1Char{ '`' } || ch == QLatin1Char{ '\n' }) {
				return std::nullopt;
			}
			if (ch != dollar || aText[at - 1].isSpace() || (at + 1 < aEnd && aText[at + 1].isDigit())) {
				continue;
			}
			return math_span{ .End = at + 1, .TeX = QStringView{ aText }.mid(aAt + 1, at - aAt - 1) };
		}
		return std::nullopt;
	}
} // namespace bpr::qt::detail
