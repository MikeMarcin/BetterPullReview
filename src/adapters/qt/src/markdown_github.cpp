#include "markdown_github.hpp"

#include "markdown_github_html.hpp"

#include "bpr/qt/appearance_profile.hpp"

#include <QPalette>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFrame>
#include <QTextTable>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <map>
#include <optional>
#include <unordered_map>
#include <utility>

namespace bpr::qt::detail {
	namespace {
		// U+2008 PUNCTUATION SPACE marks a soft line break. It is Unicode
		// whitespace to the Markdown reader, so emphasis that closes at the end
		// of a line still closes and an autolink ends before it.
		constexpr auto break_mark = QChar{ 0x2008 };
		// Noncharacters U+FDE0 to U+FDEF, which Unicode reserves for a program's
		// own use. Qt's frames use U+FDD0 and U+FDD1, so the marks keep clear.
		constexpr auto first_reserved = char16_t{ 0xFDE0 };
		constexpr auto last_reserved  = char16_t{ 0xFDEF };
		// U+FDE1 to U+FDE5: an alert's title, by kind.
		constexpr auto alert_first    = char16_t{ 0xFDE1 };
		constexpr auto alert_end      = QChar{ 0xFDE6 };
		constexpr auto reference_mark = QChar{ 0xFDE7 };
		constexpr auto mark_close     = QChar{ 0xFDE8 };
		constexpr auto back_mark      = QChar{ 0xFDE9 };
		constexpr auto item_mark      = QChar{ 0xFDEA };
		constexpr auto mark_separator = QChar{ 0xFDEC };

		using text_range = std::pair<qsizetype, qsizetype>;

		/// One source line as the block structure sees it.
		struct source_line
		{
			qsizetype Begin       = 0;
			qsizetype End         = 0; ///< The newline, or the end of the text.
			qsizetype ContentEnd  = 0; ///< End without a carriage return.
			qsizetype Content     = 0; ///< After the quote markers.
			qsizetype Rest        = 0; ///< After the content's leading whitespace.
			int       Quotes      = 0;
			int       QuoteIndent = 0; ///< Spaces before the first quote marker.
			int       Indent      = 0; ///< Columns of whitespace before Rest.
			int       ListIndent  = 0; ///< The list content column in force.
			bool      Blank       = false;
			bool      Code        = false;
			bool      Fence       = false;
		};

		struct fence_state
		{
			QChar Character;
			int   Count  = 0;
			int   Quotes = 0;
			int   Column = 0;
		};

		struct classified_source
		{
			std::vector<source_line>   Lines;
			std::optional<fence_state> OpenFence;
		};

		auto ascii_punctuation(const QChar aChar) -> bool
		{
			const auto code = aChar.unicode();
			return (code >= 0x21 && code <= 0x2F) || (code >= 0x3A && code <= 0x40) || (code >= 0x5B && code <= 0x60) || (code >= 0x7B && code <= 0x7E);
		}

		auto line_text(const QString& aText, const source_line& aLine) -> QStringView
		{
			return QStringView{ aText }.mid(aLine.Rest, aLine.ContentEnd - aLine.Rest);
		}

		auto fence_opener(QStringView aRest) -> std::optional<std::pair<QChar, int>>
		{
			if (aRest.isEmpty() || (aRest[0] != QLatin1Char{ '`' } && aRest[0] != QLatin1Char{ '~' })) {
				return std::nullopt;
			}
			const auto character = aRest[0];
			auto       count     = qsizetype{ 0 };
			while (count < aRest.size() && aRest[count] == character) {
				++count;
			}
			if (count < 3 || (character == QLatin1Char{ '`' } && aRest.mid(count).contains(QLatin1Char{ '`' }))) {
				return std::nullopt;
			}
			return std::pair{ character, static_cast<int>(count) };
		}

		auto fence_closes(QStringView aRest, const fence_state& aFence) -> bool
		{
			auto count = qsizetype{ 0 };
			while (count < aRest.size() && aRest[count] == aFence.Character) {
				++count;
			}
			return count >= aFence.Count && aRest.mid(count).trimmed().isEmpty();
		}

		auto thematic_break(QStringView aRest) -> bool
		{
			if (aRest.isEmpty() || (aRest[0] != QLatin1Char{ '-' } && aRest[0] != QLatin1Char{ '*' } && aRest[0] != QLatin1Char{ '_' })) {
				return false;
			}
			auto count = 0;
			for (const auto ch : aRest) {
				if (ch == aRest[0]) {
					++count;
				} else if (ch != QLatin1Char{ ' ' } && ch != QLatin1Char{ '\t' }) {
					return false;
				}
			}
			return count >= 3;
		}

		auto setext_underline(QStringView aRest) -> bool
		{
			const auto trimmed = aRest.trimmed();
			if (trimmed.isEmpty() || (trimmed[0] != QLatin1Char{ '=' } && trimmed[0] != QLatin1Char{ '-' })) {
				return false;
			}
			return std::ranges::all_of(trimmed, [&](const QChar aChar) { return aChar == trimmed[0]; });
		}

		auto atx_heading(QStringView aRest) -> bool
		{
			auto count = qsizetype{ 0 };
			while (count < aRest.size() && aRest[count] == QLatin1Char{ '#' }) {
				++count;
			}
			return count >= 1 && count <= 6 && (count == aRest.size() || aRest[count] == QLatin1Char{ ' ' } || aRest[count] == QLatin1Char{ '\t' });
		}

		/// The width of a list marker and its following spaces, or zero.
		auto list_marker(QStringView aRest) -> int
		{
			if (aRest.isEmpty() || thematic_break(aRest)) {
				return 0;
			}
			auto width = qsizetype{ 0 };
			if (aRest[0] == QLatin1Char{ '-' } || aRest[0] == QLatin1Char{ '+' } || aRest[0] == QLatin1Char{ '*' }) {
				width = 1;
			} else {
				while (width < aRest.size() && width < 9 && aRest[width].isDigit() && aRest[width].unicode() < 0x80) {
					++width;
				}
				if (width == 0 || width == aRest.size() || (aRest[width] != QLatin1Char{ '.' } && aRest[width] != QLatin1Char{ ')' })) {
					return 0;
				}
				++width;
			}
			if (width == aRest.size()) {
				return static_cast<int>(width + 1);
			}
			if (aRest[width] != QLatin1Char{ ' ' } && aRest[width] != QLatin1Char{ '\t' }) {
				return 0;
			}
			auto spaces = qsizetype{ 0 };
			while (width + spaces < aRest.size() && aRest[width + spaces] == QLatin1Char{ ' ' }) {
				++spaces;
			}
			return static_cast<int>(width + (spaces >= 1 && spaces <= 4 && width + spaces < aRest.size() ? spaces : 1));
		}

		auto starts_with(const QString& aText, const qsizetype aAt, const QLatin1StringView aPrefix) -> bool
		{
			return QStringView{ aText }.mid(aAt).startsWith(aPrefix);
		}

		/// Splits one line into its quote markers and indentation.
		auto measure_line(const QString& aText, const qsizetype aBegin, const qsizetype aEnd) -> source_line
		{
			source_line line;
			line.Begin      = aBegin;
			line.End        = aEnd;
			line.ContentEnd = aEnd > aBegin && aText[aEnd - 1] == QLatin1Char{ '\r' } ? aEnd - 1 : aEnd;
			auto at         = aBegin;
			for (;;) {
				auto spaces = 0;
				while (spaces < 3 && at + spaces < line.ContentEnd && aText[at + spaces] == QLatin1Char{ ' ' }) {
					++spaces;
				}
				if (at + spaces >= line.ContentEnd || aText[at + spaces] != QLatin1Char{ '>' }) {
					break;
				}
				if (line.Quotes == 0) {
					line.QuoteIndent = spaces;
				}
				++line.Quotes;
				at += spaces + 1;
				if (at < line.ContentEnd && (aText[at] == QLatin1Char{ ' ' } || aText[at] == QLatin1Char{ '\t' })) {
					++at;
				}
			}
			line.Content = at;
			auto column  = 0;
			while (at < line.ContentEnd && (aText[at] == QLatin1Char{ ' ' } || aText[at] == QLatin1Char{ '\t' })) {
				column = aText[at] == QLatin1Char{ '\t' } ? column + 4 - column % 4 : column + 1;
				++at;
			}
			line.Rest   = at;
			line.Indent = column;
			line.Blank  = at == line.ContentEnd;
			return line;
		}

		/// Classifies every line as code or not, erring toward code: a line
		/// wrongly taken for code is only left as written.
		auto classify(const QString& aText) -> classified_source
		{
			classified_source          result;
			std::optional<fence_state> fence;
			auto                       listIndent    = 0;
			auto                       paragraph     = false;
			auto                       previousBlank = true;
			for (qsizetype begin = 0; begin <= aText.size();) {
				auto end = aText.indexOf(QLatin1Char{ '\n' }, begin);
				if (end < 0) {
					end = aText.size();
				}
				auto       line = measure_line(aText, begin, end);
				const auto rest = line_text(aText, line);
				begin           = end + 1;
				if (fence && line.Quotes < fence->Quotes) {
					fence.reset();
				}
				if (fence) {
					line.Code = true;
					if (!line.Blank && fence_closes(rest, *fence)) {
						line.Fence = true;
						fence.reset();
					}
					line.ListIndent = listIndent;
					result.Lines.push_back(line);
					paragraph     = false;
					previousBlank = false;
					continue;
				}
				if (line.Blank) {
					line.ListIndent = listIndent;
					result.Lines.push_back(line);
					previousBlank = true;
					paragraph     = false;
					continue;
				}
				const auto marker = list_marker(rest);
				if (marker == 0 && previousBlank && line.Indent < listIndent) {
					listIndent = 0;
				}
				line.ListIndent = listIndent;
				previousBlank   = false;
				if (!paragraph && line.Indent >= listIndent + 4) {
					line.Code = true;
					result.Lines.push_back(line);
					continue;
				}
				auto opener = fence_opener(rest);
				auto column = line.Indent;
				if (!opener && marker > 0) {
					const auto item = rest.mid(std::min<qsizetype>(marker, rest.size()));
					opener          = fence_opener(item.trimmed());
					column          = line.Indent + marker + static_cast<int>(item.size() - item.trimmed().size());
				}
				if (marker > 0) {
					listIndent = line.Indent + marker;
				}
				if (opener) {
					fence     = fence_state{ .Character = opener->first, .Count = opener->second, .Quotes = line.Quotes, .Column = column };
					line.Code = line.Fence = true;
					paragraph              = false;
					result.Lines.push_back(line);
					continue;
				}
				paragraph = !atx_heading(rest) && !thematic_break(rest) && !(paragraph && setext_underline(rest));
				result.Lines.push_back(line);
			}
			result.OpenFence = fence;
			return result;
		}

		/// Walks [aBegin, aEnd) skipping backslash escapes and inline code
		/// spans, calling aVisit at every other character. aVisit returns how
		/// many characters it consumed, or zero to move one. Code spans are
		/// matched as CommonMark matches them: an opening run of backticks is
		/// closed by the next run of exactly the same length.
		template <typename TVisit>
		void scan_inline(const QString& aText, const qsizetype aBegin, const qsizetype aEnd, TVisit&& aVisit, std::vector<text_range>* aCode = nullptr)
		{
			std::unordered_map<qsizetype, std::vector<qsizetype>> runs;
			for (auto at = aBegin; at < aEnd;) {
				if (aText[at] != QLatin1Char{ '`' }) {
					++at;
					continue;
				}
				auto end = at;
				while (end < aEnd && aText[end] == QLatin1Char{ '`' }) {
					++end;
				}
				runs[end - at].push_back(at);
				at = end;
			}
			for (auto at = aBegin; at < aEnd;) {
				const auto ch = aText[at];
				if (ch == QLatin1Char{ '\\' } && at + 1 < aEnd && ascii_punctuation(aText[at + 1])) {
					at += 2;
					continue;
				}
				if (ch == QLatin1Char{ '`' }) {
					auto end = at;
					while (end < aEnd && aText[end] == QLatin1Char{ '`' }) {
						++end;
					}
					auto       close = qsizetype{ -1 };
					const auto found = runs.find(end - at);
					if (found != runs.end()) {
						const auto next = std::ranges::lower_bound(found->second, end);
						if (next != found->second.end()) {
							close = *next;
						}
					}
					if (close >= 0) {
						if (aCode != nullptr) {
							aCode->emplace_back(at, close + (end - at));
						}
						at = close + (end - at);
					} else {
						at = end;
					}
					continue;
				}
				const auto consumed = aVisit(at);
				at += consumed > 0 ? consumed : 1;
			}
		}

		/// The text with each range removed or replaced; ranges are sorted
		/// and do not overlap.
		auto rewrite(const QString& aText, const std::vector<std::pair<text_range, QString>>& aEdits) -> QString
		{
			QString result;
			result.reserve(aText.size());
			auto at = qsizetype{ 0 };
			for (const auto& [range, replacement] : aEdits) {
				result += QStringView{ aText }.mid(at, range.first - at);
				result += replacement;
				at = range.second;
			}
			result += QStringView{ aText }.mid(at);
			return result;
		}

		/// Lines [aFirst, last] that read as one paragraph: consecutive lines
		/// that are neither blank nor code. Returns last.
		auto paragraph_end(const std::vector<source_line>& aLines, const std::size_t aFirst, const auto& aStop) -> std::size_t
		{
			auto last = aFirst;
			while (last + 1 < aLines.size() && !aLines[last + 1].Code && !aLines[last + 1].Blank && !aStop(aLines[last + 1])) {
				++last;
			}
			return last;
		}

		// GitHub hides HTML comments. A comment that starts a line is an HTML
		// block and runs to its `-->`, across blank lines, or to the end of the
		// body when it is never closed; any other comment closes within its
		// paragraph or stays literal. Code is never touched.
		auto remove_comments(const QString& aText) -> QString
		{
			if (!aText.contains(QLatin1StringView{ "<!--" })) {
				return aText;
			}
			const auto                                  source = classify(aText);
			const auto&                                 lines  = source.Lines;
			std::vector<std::pair<text_range, QString>> edits;
			const auto                                  opens = [&aText](const source_line& aLine) { return starts_with(aText, aLine.Rest, QLatin1StringView{ "<!--" }); };
			for (std::size_t index = 0; index < lines.size();) {
				const auto& line = lines[index];
				if (line.Code || line.Blank) {
					++index;
					continue;
				}
				if (opens(line)) {
					const auto close = aText.indexOf(QLatin1StringView{ "-->" }, line.Rest + 2);
					const auto end   = close < 0 ? aText.size() : close + 3;
					edits.push_back({ { line.Rest, end }, { } });
					while (index < lines.size() && lines[index].End < end) {
						++index;
					}
					++index;
					continue;
				}
				const auto last  = paragraph_end(lines, index, opens);
				const auto limit = lines[last].End;
				auto       close = qsizetype{ -2 };
				scan_inline(aText, line.Content, limit, [&](const qsizetype aAt) -> qsizetype {
					if (!starts_with(aText, aAt, QLatin1StringView{ "<!--" })) {
						return 0;
					}
					if (close == -2 || (close >= 0 && close < aAt + 2)) {
						close = aText.indexOf(QLatin1StringView{ "-->" }, aAt + 2);
						close = close >= 0 && close + 3 <= limit ? close : -1;
					}
					if (close < 0) {
						return 4;
					}
					// Words on both sides keep one space between them, as HTML shows.
					const auto spaced = aAt > 0 && aText[aAt - 1] == QLatin1Char{ ' ' } && close + 3 < limit && aText[close + 3] == QLatin1Char{ ' ' };
					edits.push_back({ { aAt, close + (spaced ? 4 : 3) }, { } });
					return close + (spaced ? 4 : 3) - aAt;
				});
				index = last + 1;
			}
			return rewrite(aText, edits);
		}

		auto starts_block(const QString& aText, const source_line& aLine) -> bool
		{
			const auto rest = line_text(aText, aLine);
			return aLine.Quotes > 0 || atx_heading(rest) || list_marker(rest) > 0 || fence_opener(rest).has_value() || thematic_break(rest);
		}

		struct footnote
		{
			QStringList Lines;
			int         Number     = 0;
			int         References = 0;
			bool        EndsInCode = false;
		};

		const auto& definition_pattern()
		{
			static const auto pattern = QRegularExpression{ QStringLiteral(R"(^\[\^([^\]\s]{1,64})\]:[ \t]?(.*)$)") };
			return pattern;
		}

		auto footnote_label(const QString& aText, const source_line& aLine) -> QString
		{
			if (aLine.Code || aLine.Blank || aLine.Quotes != 0 || aLine.Indent > 3 || aLine.ListIndent != 0) {
				return { };
			}
			const auto match = definition_pattern().matchView(line_text(aText, aLine));
			return match.hasMatch() ? match.captured(1).toCaseFolded() : QString{ };
		}

		/// Removes footnote definitions and returns them by label, with the
		/// text that remains.
		auto take_definitions(const QString& aText, std::map<QString, footnote>& aNotes) -> QString
		{
			const auto                                  source = classify(aText);
			const auto&                                 lines  = source.Lines;
			std::vector<std::pair<text_range, QString>> edits;
			for (std::size_t index = 0; index < lines.size();) {
				const auto label = footnote_label(aText, lines[index]);
				if (label.isEmpty()) {
					++index;
					continue;
				}
				auto       note  = footnote{ };
				const auto match = definition_pattern().matchView(line_text(aText, lines[index]));
				note.Lines.push_back(match.captured(2));
				auto last = index;
				for (auto next = index + 1; next < lines.size();) {
					const auto& line = lines[next];
					if (line.Blank) {
						auto after = next;
						while (after < lines.size() && lines[after].Blank) {
							++after;
						}
						if (after == lines.size() || lines[after].Indent < 4 || lines[after].Quotes != 0) {
							break;
						}
						for (; next < after; ++next) {
							note.Lines.push_back({ });
						}
						continue;
					}
					if (!footnote_label(aText, line).isEmpty() || (line.Indent < 4 && (next != last + 1 || starts_block(aText, line)))) {
						break;
					}
					// The definition's own four columns go; anything deeper is its content.
					auto from   = line.Begin;
					auto column = 0;
					while (from < line.ContentEnd && column < 4 && (aText[from] == QLatin1Char{ ' ' } || aText[from] == QLatin1Char{ '\t' })) {
						column = aText[from] == QLatin1Char{ '\t' } ? 4 : column + 1;
						++from;
					}
					note.Lines.push_back(aText.mid(from, line.ContentEnd - from));
					last = next++;
				}
				// Back references follow the last paragraph, or stand alone after code.
				const auto content = classify(note.Lines.join(QLatin1Char{ '\n' }));
				note.EndsInCode    = !content.Lines.empty() && (content.Lines.back().Code || content.OpenFence.has_value());
				const auto end     = std::min(lines[last].End + 1, aText.size());
				edits.push_back({ { lines[index].Begin, end }, { } });
				if (!aNotes.contains(label)) {
					aNotes.emplace(label, std::move(note));
				}
				index = last + 1;
			}
			return rewrite(aText, edits);
		}

		auto mark(const QChar aOpen, const int aNumber, const int aReference) -> QString
		{
			return aOpen + QString::number(aNumber) + mark_separator + QString::number(aReference) + mark_close;
		}

		// GitHub numbers footnotes in the order they are first referenced and
		// drops a definition nothing references. A reference with no
		// definition stays literal.
		auto move_footnotes(const QString& aText) -> QString
		{
			if (!aText.contains(QLatin1StringView{ "[^" })) {
				return aText;
			}
			std::map<QString, footnote> notes;
			auto                        text = take_definitions(aText, notes);
			if (notes.empty()) {
				return aText;
			}
			const auto                                  source = classify(text);
			const auto&                                 lines  = source.Lines;
			std::vector<std::pair<text_range, QString>> edits;
			std::vector<footnote*>                      order;
			for (std::size_t index = 0; index < lines.size();) {
				if (lines[index].Code || lines[index].Blank) {
					++index;
					continue;
				}
				const auto last = paragraph_end(lines, index, [](const source_line&) { return false; });
				scan_inline(text, lines[index].Content, lines[last].End, [&](const qsizetype aAt) -> qsizetype {
					if (!starts_with(text, aAt, QLatin1StringView{ "[^" })) {
						return 0;
					}
					const auto close = text.indexOf(QLatin1Char{ ']' }, aAt + 2);
					if (close < 0 || close - aAt > 66) {
						return 2;
					}
					const auto label = QStringView{ text }.mid(aAt + 2, close - aAt - 2);
					if (label.isEmpty() || std::ranges::any_of(label, [](const QChar aChar) { return aChar.isSpace() || aChar == QLatin1Char{ '[' }; })) {
						return 2;
					}
					const auto found = notes.find(label.toString().toCaseFolded());
					if (found == notes.end()) {
						return 2;
					}
					auto& note = found->second;
					if (note.Number == 0) {
						order.push_back(&note);
						note.Number = static_cast<int>(order.size());
					}
					edits.push_back({ { aAt, close + 1 }, mark(reference_mark, note.Number, ++note.References) });
					return close + 1 - aAt;
				});
				index = last + 1;
			}
			if (order.empty()) {
				return text;
			}
			text = rewrite(text, edits);
			// A fence the body leaves open would swallow the section.
			if (source.OpenFence && source.OpenFence->Quotes == 0) {
				text += QLatin1Char{ '\n' } + QString(source.OpenFence->Column, QLatin1Char{ ' ' }) + QString(source.OpenFence->Count, source.OpenFence->Character);
			}
			text += QStringLiteral("\n\n---\n\n");
			for (const auto* note : order) {
				const auto prefix = QString::number(note->Number) + QStringLiteral(". ");
				const auto indent = QString(prefix.size(), QLatin1Char{ ' ' });
				auto       body   = note->Lines;
				while (body.size() > 1 && body.back().trimmed().isEmpty()) {
					body.removeLast();
				}
				QString backs;
				for (auto reference = 1; reference <= note->References; ++reference) {
					backs += QLatin1Char{ ' ' } + mark(back_mark, note->Number, reference);
				}
				if (note->EndsInCode) {
					body.push_back({ });
					body.push_back(backs.trimmed());
				} else {
					body.back() += backs;
				}
				for (qsizetype line = 0; line < body.size(); ++line) {
					if (line == 0) {
						text += prefix + item_mark + QString::number(note->Number) + mark_close + body[line];
					} else if (!body[line].isEmpty()) {
						text += indent + body[line];
					}
					text += QLatin1Char{ '\n' };
				}
			}
			return text;
		}

		const auto& alert_pattern()
		{
			static const auto pattern = QRegularExpression{ QStringLiteral(R"(^\[!(NOTE|TIP|IMPORTANT|WARNING|CAUTION)\]\s*$)"), QRegularExpression::CaseInsensitiveOption };
			return pattern;
		}

		auto alert_kind(const QString& aWord) -> markdown_alert
		{
			const auto word = aWord.toUpper();
			return word == QStringLiteral("NOTE")     ? markdown_alert::note
				: word == QStringLiteral("TIP")       ? markdown_alert::tip
				: word == QStringLiteral("IMPORTANT") ? markdown_alert::important
				: word == QStringLiteral("WARNING")   ? markdown_alert::warning
													  : markdown_alert::caution;
		}

		// A top-level quote whose first line is `[!NOTE]` (or TIP, IMPORTANT,
		// WARNING, CAUTION) and that has a line after it. The title becomes its
		// own paragraph; a mark paragraph after the quote ends the alert, so a
		// heading or table inside it, which Qt does not mark as quoted, stays
		// inside.
		auto mark_alerts(const QString& aText) -> QString
		{
			if (!aText.contains(QLatin1StringView{ "[!" })) {
				return aText;
			}
			const auto                                  source = classify(aText);
			const auto&                                 lines  = source.Lines;
			std::vector<std::pair<text_range, QString>> edits;
			for (std::size_t index = 0; index + 1 < lines.size(); ++index) {
				const auto& line = lines[index];
				if (line.Code || line.Quotes != 1 || (line.ListIndent > 0 && line.QuoteIndent >= line.ListIndent)
					|| (index > 0 && lines[index - 1].Quotes > 0) || lines[index + 1].Quotes < 1) {
					continue;
				}
				const auto match = alert_pattern().matchView(line_text(aText, line));
				if (!match.hasMatch()) {
					continue;
				}
				const auto kind = alert_kind(match.captured(1));
				edits.push_back({ { line.Rest, line.ContentEnd }, QString{ QChar{ static_cast<char16_t>(alert_first + static_cast<int>(kind) - 1) } } + QStringLiteral("\n>") });
				auto last = index + 1;
				while (last + 1 < lines.size()) {
					const auto& next = lines[last + 1];
					const auto  lazy = next.Quotes == 0 && !next.Blank && !next.Code && !lines[last].Blank && !lines[last].Code && !starts_block(aText, next);
					if (next.Quotes < 1 && !lazy) {
						break;
					}
					++last;
				}
				edits.push_back({ { lines[last].End, lines[last].End }, QStringLiteral("\n\n") + alert_end + QLatin1Char{ '\n' } });
				index = last;
			}
			return rewrite(aText, edits);
		}

		auto table_delimiter(QStringView aRest) -> bool
		{
			static const auto pattern = QRegularExpression{ QStringLiteral(R"(^\|?\s*:?-+:?\s*(\|\s*:?-+:?\s*)*\|?\s*$)") };
			return aRest.contains(QLatin1Char{ '|' }) && pattern.matchView(aRest).hasMatch();
		}

		auto reserved_only(QStringView aRest) -> bool
		{
			return !aRest.isEmpty() && std::ranges::all_of(aRest, [](const QChar aChar) { return aChar.unicode() >= first_reserved && aChar.unicode() <= last_reserved; });
		}

		/// Whether a soft break after this line must stay a space: the line is
		/// a heading, rule, table row, reference definition or marker-only line,
		/// where a mark would change its meaning.
		auto keeps_soft_break(const QString& aText, const source_line& aLine) -> bool
		{
			static const auto definition = QRegularExpression{ QStringLiteral(R"(^\[[^\]]+\]:)") };
			const auto        rest       = line_text(aText, aLine);
			const auto        marker     = list_marker(rest);
			return atx_heading(rest) || thematic_break(rest) || setext_underline(rest) || definition.matchView(rest).hasMatch()
				|| (marker > 0 && rest.mid(std::min<qsizetype>(marker, rest.size())).trimmed().isEmpty()) || reserved_only(rest.trimmed());
		}

		// GitHub shows each single newline of a paragraph as a line break.
		// Every such newline gets a mark, and a hard break written as two
		// spaces or a backslash becomes the same mark, so presentation can set
		// one line separator for each.
		auto mark_breaks(const QString& aText) -> QString
		{
			if (!aText.contains(QLatin1Char{ '\n' })) {
				return aText;
			}
			const auto        source = classify(aText);
			const auto&       lines  = source.Lines;
			std::vector<bool> table(lines.size(), false);
			for (std::size_t index = 1; index < lines.size(); ++index) {
				const auto& header = lines[index - 1];
				if (!lines[index].Code && table_delimiter(line_text(aText, lines[index])) && !header.Blank && !header.Code
					&& line_text(aText, header).contains(QLatin1Char{ '|' })) {
					for (auto row = index - 1; row < lines.size() && !lines[row].Blank && !lines[row].Code; ++row) {
						table[row] = true;
					}
				}
			}
			std::vector<text_range> code;
			for (std::size_t index = 0; index < lines.size();) {
				if (lines[index].Code || lines[index].Blank) {
					++index;
					continue;
				}
				const auto last = paragraph_end(lines, index, [](const source_line&) { return false; });
				scan_inline(aText, lines[index].Content, lines[last].End, [](qsizetype) { return qsizetype{ 0 }; }, &code);
				index = last + 1;
			}
			const auto inCode = [&code](const qsizetype aAt) {
				const auto span = std::ranges::upper_bound(code, aAt, { }, &text_range::first);
				return span != code.begin() && std::prev(span)->second > aAt;
			};
			std::vector<std::pair<text_range, QString>> edits;
			for (std::size_t index = 0; index + 1 < lines.size(); ++index) {
				const auto& line = lines[index];
				const auto& next = lines[index + 1];
				if (line.Code || line.Blank || next.Code || next.Blank || table[index] || inCode(line.End) || keeps_soft_break(aText, line)) {
					continue;
				}
				auto end = line.ContentEnd;
				while (end > line.Rest && aText[end - 1] == QLatin1Char{ ' ' }) {
					--end;
				}
				// A hard break already written (two spaces or one unescaped
				// backslash) is replaced by the mark; otherwise it is added.
				auto from = line.ContentEnd;
				if (line.ContentEnd - end >= 2) {
					from = end;
				} else if (end == line.ContentEnd && end > line.Rest && aText[end - 1] == QLatin1Char{ '\\' }) {
					auto slashes = 0;
					for (auto at = end - 1; at >= line.Rest && aText[at] == QLatin1Char{ '\\' }; --at) {
						++slashes;
					}
					from = slashes % 2 == 1 ? end - 1 : from;
				}
				edits.push_back({ { from, line.ContentEnd }, QString{ break_mark } });
			}
			return rewrite(aText, edits);
		}

		// [[BPR-1544]]. A `:name:` GitHub knows becomes its emoji. Like GitHub's
		// filter it needs a boundary on both sides, so `a:b:c` and a time stay
		// as written; code, URLs, link destinations and HTML tags are skipped.
		auto replace_emoji(const QString& aText) -> QString
		{
			if (!aText.contains(QLatin1Char{ ':' })) {
				return aText;
			}
			const auto                                  source = classify(aText);
			const auto&                                 lines  = source.Lines;
			std::vector<std::pair<text_range, QString>> edits;
			const auto                                  word = [](const QChar aChar) { return aChar.isLetterOrNumber() || aChar == QLatin1Char{ '_' } || aChar == QLatin1Char{ ':' }; };
			const auto                                  name = [](const QChar aChar) {
				const auto code = aChar.unicode();
				return (code >= 'a' && code <= 'z') || (code >= '0' && code <= '9') || code == '_' || code == '+' || code == '-';
			};
			for (std::size_t index = 0; index < lines.size();) {
				if (lines[index].Code || lines[index].Blank) {
					++index;
					continue;
				}
				const auto last  = paragraph_end(lines, index, [](const source_line&) { return false; });
				const auto begin = lines[index].Content;
				const auto limit = lines[last].End;
				scan_inline(aText, begin, limit, [&](const qsizetype aAt) -> qsizetype {
					if (aText[aAt] == QLatin1Char{ '<' }) {
						const auto tag = html_tag_at(QStringView{ aText }.mid(aAt, limit - aAt));
						return tag ? tag->Length : 0;
					}
					if (aText[aAt] != QLatin1Char{ ':' } || (aAt > 0 && word(aText[aAt - 1]))) {
						return 0;
					}
					auto end = aAt + 1;
					while (end < limit && end - aAt <= 64 && name(aText[end])) {
						++end;
					}
					if (end == aAt + 1 || end >= limit || aText[end] != QLatin1Char{ ':' } || (end + 1 < aText.size() && word(aText[end + 1]))) {
						return 0;
					}
					auto token = aAt;
					while (token > begin && aAt - token < 512 && !aText[token - 1].isSpace()) {
						--token;
					}
					const auto before = QStringView{ aText }.mid(token, aAt - token);
					if (before.contains(QLatin1StringView{ "://" }) || before.startsWith(QLatin1StringView{ "www." }, Qt::CaseInsensitive) || before.contains(QLatin1StringView{ "](" })) {
						return 0;
					}
					const auto emoji = markdown_emoji(QStringView{ aText }.mid(aAt + 1, end - aAt - 1));
					if (!emoji) {
						return 0;
					}
					edits.push_back({ { aAt, end + 1 }, *emoji });
					return end + 1 - aAt;
				});
				index = last + 1;
			}
			return rewrite(aText, edits);
		}

		// U+FDE0 opens a region paragraph: `1` a closed `<details>`, `2` an
		// open one, `0` its end, `a` an aligned region and `z` its end.
		// U+FDEF names a sanitized HTML block.
		constexpr auto region_mark = QChar{ 0xFDE0 };
		constexpr auto block_mark  = QChar{ 0xFDEF };

		auto region(const QChar aKind, const QString& aText = { }) -> QString
		{
			return QLatin1Char{ '\n' } + region_mark + aKind + html_mark_close + aText + QStringLiteral("\n\n");
		}

		struct summary_split
		{
			QString Summary;
			QString After;
			bool    Found = false;
		};

		/// A `<summary>…</summary>` at the start of `aRest`, and what follows it.
		auto split_summary(const QStringView aRest) -> summary_split
		{
			const auto rest   = aRest.trimmed();
			auto       result = summary_split{ };
			const auto tag    = rest.startsWith(QLatin1Char{ '<' }) ? html_tag_at(rest) : std::nullopt;
			if (!tag || tag->Closing || tag->Name != QStringLiteral("summary")) {
				result.After = rest.toString();
				return result;
			}
			const auto inner = rest.mid(tag->Length);
			const auto close = inner.indexOf(QLatin1StringView{ "</summary>" }, 0, Qt::CaseInsensitive);
			result.Found     = true;
			result.Summary   = (close < 0 ? inner : inner.mid(0, close)).trimmed().toString();
			result.After     = close < 0 ? QString{ } : inner.mid(close + 10).trimmed().toString();
			return result;
		}

		auto closes_details(const QString& aText, const source_line& aLine) -> bool
		{
			return line_text(aText, aLine).trimmed().compare(QLatin1StringView{ "</details>" }, Qt::CaseInsensitive) == 0;
		}

		// [[BPR-1544]]. Top-level HTML blocks, as CommonMark starts them:
		// `<details>` and `</details>` become region paragraphs, so the
		// Markdown between them still reads; a lone `<div align>` or
		// `<p align>` followed by a blank line opens an aligned region its
		// lone closing tag ends; any other block of structure tags (to the
		// blank line, or to `</pre>`) is sanitized and named by a mark.
		auto mark_html_blocks(const QString& aText, prepared_markdown& aPrepared) -> QString
		{
			if (!aText.contains(QLatin1Char{ '<' })) {
				return aText;
			}
			const auto                                  source = classify(aText);
			const auto&                                 lines  = source.Lines;
			std::vector<std::pair<text_range, QString>> edits;
			auto                                        details = 0;
			std::vector<QString>                        aligned;
			const auto                                  top = [](const source_line& aLine) { return !aLine.Code && !aLine.Blank && aLine.Quotes == 0 && aLine.ListIndent == 0 && aLine.Indent <= 3; };
			for (std::size_t index = 0; index < lines.size(); ++index) {
				const auto& line = lines[index];
				if (!top(line) || aText[line.Rest] != QLatin1Char{ '<' }) {
					continue;
				}
				const auto rest = line_text(aText, line).trimmed();
				const auto tag  = html_tag_at(rest);
				if (!tag) {
					continue;
				}
				const auto alone     = tag->Length == rest.size();
				const auto nextBlank = index + 1 >= lines.size() || lines[index + 1].Blank;
				if (tag->Name == QStringLiteral("details") && !tag->Closing) {
					auto split = split_summary(rest.mid(tag->Length));
					auto last  = index;
					if (!split.Found && split.After.isEmpty() && index + 1 < lines.size() && top(lines[index + 1])) {
						if (auto next = split_summary(line_text(aText, lines[index + 1])); next.Found) {
							split = std::move(next);
							last  = index + 1;
						}
					}
					// GitHub calls a section without a summary "Details".
					auto replacement = region(tag->attribute(QStringLiteral("open")) ? QLatin1Char{ '2' } : QLatin1Char{ '1' }, split.Found ? split.Summary : QStringLiteral("Details"));
					if (!split.After.isEmpty()) {
						replacement += split.After + QLatin1Char{ '\n' };
					}
					edits.push_back({ { line.Begin, lines[last].End }, replacement });
					++details;
					index = last;
					continue;
				}
				if (tag->Name == QStringLiteral("details") && tag->Closing && alone && details > 0) {
					edits.push_back({ { line.Begin, line.End }, region(QLatin1Char{ '0' }) });
					--details;
					continue;
				}
				if ((tag->Name == QStringLiteral("div") || tag->Name == QStringLiteral("p")) && !tag->Closing && alone && nextBlank) {
					if (const auto align = html_alignment(*tag); !align.isEmpty()) {
						edits.push_back({ { line.Begin, line.End }, region(QLatin1Char{ 'a' }, align.left(1)) });
						aligned.push_back(tag->Name);
						continue;
					}
				}
				if (tag->Closing && alone && !aligned.empty() && aligned.back() == tag->Name) {
					edits.push_back({ { line.Begin, line.End }, region(QLatin1Char{ 'z' }) });
					aligned.pop_back();
					continue;
				}
				if (html_kind_of(tag->Name) != html_kind::block) {
					continue;
				}
				auto last = index;
				if (tag->Name == QStringLiteral("pre") && !tag->Closing) {
					while (last + 1 < lines.size() && !QStringView{ aText }.mid(lines[last].Begin, lines[last].End - lines[last].Begin).contains(QLatin1StringView{ "</pre>" }, Qt::CaseInsensitive)) {
						++last;
					}
				} else {
					while (last + 1 < lines.size() && !lines[last + 1].Blank && !closes_details(aText, lines[last + 1])) {
						++last;
					}
				}
				auto html        = sanitize_html_block(QStringView{ aText }.mid(line.Rest, lines[last].ContentEnd - line.Rest), aPrepared.Hrefs);
				auto replacement = QString{ QLatin1Char{ '\n' } };
				if (html_shows_anything(html)) {
					replacement = QLatin1Char{ '\n' } + block_mark + QString::number(aPrepared.Html.size()) + html_mark_close + QStringLiteral("\n\n");
					aPrepared.Html.push_back(std::move(html));
				}
				edits.push_back({ { line.Begin, lines[last].End }, replacement });
				index = last;
			}
			return rewrite(aText, edits);
		}

		// [[BPR-1544]]. Inline HTML in every paragraph, outside code spans:
		// formatting and kept links become marks, `<br>` a break mark, other
		// tags are dropped with their text kept, and the tag filter's tags,
		// the image tags and an `<a>` around an image stay as written.
		auto mark_html_inline(const QString& aText, prepared_markdown& aPrepared) -> QString
		{
			if (!aText.contains(QLatin1Char{ '<' })) {
				return aText;
			}
			const auto                                  source = classify(aText);
			const auto&                                 lines  = source.Lines;
			std::vector<std::pair<text_range, QString>> edits;
			for (std::size_t index = 0; index < lines.size();) {
				if (lines[index].Code || lines[index].Blank) {
					++index;
					continue;
				}
				const auto        last  = paragraph_end(lines, index, [](const source_line&) { return false; });
				const auto        limit = lines[last].End;
				std::vector<bool> links;
				auto              literalLinks = 0;
				scan_inline(aText, lines[index].Content, limit, [&](const qsizetype aAt) -> qsizetype {
					if (aText[aAt] != QLatin1Char{ '<' }) {
						return 0;
					}
					const auto tag = html_tag_at(QStringView{ aText }.mid(aAt, limit - aAt));
					if (!tag) {
						return 0;
					}
					const auto kind = html_kind_of(tag->Name);
					const auto end  = aAt + tag->Length;
					if (kind == html_kind::literal) {
						return tag->Length;
					}
					if (kind == html_kind::link && !tag->Closing && html_link_holds_image(QStringView{ aText }.mid(end, limit - end))) {
						++literalLinks;
						return tag->Length;
					}
					if (kind == html_kind::link && tag->Closing && literalLinks > 0) {
						--literalLinks;
						return tag->Length;
					}
					const auto replacement = kind == html_kind::block || kind == html_kind::drop ? QString{ } : inline_html_mark(*tag, kind, aPrepared.Hrefs, links);
					edits.push_back({ { aAt, end }, replacement });
					return tag->Length;
				});
				index = last + 1;
			}
			return rewrite(aText, edits);
		}

		auto holds_reserved(const QString& aText) -> bool
		{
			return std::ranges::any_of(aText, [](const QChar aChar) { return aChar.unicode() >= first_reserved && aChar.unicode() <= last_reserved; });
		}

		auto format_at(QTextDocument& aDocument, const int aPosition) -> QTextCharFormat
		{
			auto cursor = QTextCursor{ &aDocument };
			cursor.setPosition(aPosition + 1);
			return cursor.charFormat();
		}

		auto code_block(const QTextBlock& aBlock) -> bool
		{
			const auto format = aBlock.blockFormat();
			return format.hasProperty(QTextFormat::BlockCodeFence) || format.hasProperty(QTextFormat::BlockCodeLanguage);
		}

		// Qt's GitHub dialect reads `_text_` as underline, which nothing else
		// in an import produces; GitHub reads it as emphasis.
		void present_underscore_emphasis(QTextDocument& aDocument)
		{
			std::vector<std::pair<int, int>> spans;
			for (auto block = aDocument.begin(); block.isValid(); block = block.next()) {
				for (auto it = block.begin(); !it.atEnd(); ++it) {
					const auto format = it.fragment().charFormat();
					if (format.fontUnderline() && !format.isAnchor()) {
						spans.emplace_back(it.fragment().position(), it.fragment().length());
					}
				}
			}
			auto cursor   = QTextCursor{ &aDocument };
			auto emphasis = QTextCharFormat{ };
			emphasis.setFontUnderline(false);
			emphasis.setFontItalic(true);
			for (const auto& [position, length] : spans) {
				cursor.setPosition(position);
				cursor.setPosition(position + length, QTextCursor::KeepAnchor);
				cursor.mergeCharFormat(emphasis);
			}
		}

		void present_breaks(QTextDocument& aDocument)
		{
			for (auto cursor = aDocument.find(QString{ break_mark }); !cursor.isNull(); cursor = aDocument.find(QString{ break_mark }, cursor.position())) {
				const auto at = cursor.selectionStart();
				if (!code_block(cursor.block()) && aDocument.characterAt(at + 1) == QLatin1Char{ ' ' }) {
					const auto format = format_at(aDocument, at);
					cursor.setPosition(at);
					cursor.setPosition(at + 2, QTextCursor::KeepAnchor);
					cursor.insertText(QString{ QChar::LineSeparator }, format);
				} else {
					cursor.removeSelectedText();
				}
			}
		}

		/// One mark's fields: its number and reference, and where it ends.
		struct parsed_mark
		{
			int Number    = 0;
			int Reference = 0;
			int End       = 0;
		};

		auto parse_mark(const QTextDocument& aDocument, const int aAt) -> std::optional<parsed_mark>
		{
			auto result = parsed_mark{ };
			auto field  = &result.Number;
			for (auto at = aAt + 1; at < aDocument.characterCount() && at < aAt + 24; ++at) {
				const auto ch = aDocument.characterAt(at);
				if (ch == mark_close) {
					result.End = at + 1;
					return result.Number > 0 ? std::optional{ result } : std::nullopt;
				}
				if (ch == mark_separator) {
					field = &result.Reference;
				} else if (ch.isDigit() && ch.unicode() < 0x80) {
					*field = *field * 10 + (ch.unicode() - '0');
				} else {
					return std::nullopt;
				}
			}
			return std::nullopt;
		}

		auto reference_name(const int aNumber, const int aReference) -> QString
		{
			return aReference <= 1 ? QStringLiteral("fnref-%1").arg(aNumber) : QStringLiteral("fnref-%1-%2").arg(aNumber).arg(aReference);
		}

		/// A mark's replacement, with the start of any raised suffix.
		struct marked_text
		{
			QString   Text;
			qsizetype Raised = -1;
		};

		/// Replaces each mark that opens with aOpen by a link made by aLink.
		template <typename TLink>
		void present_marks(QTextDocument& aDocument, const QChar aOpen, TLink&& aLink)
		{
			for (auto cursor = aDocument.find(QString{ aOpen }); !cursor.isNull(); cursor = aDocument.find(QString{ aOpen }, cursor.position())) {
				const auto at    = cursor.selectionStart();
				const auto found = parse_mark(aDocument, at);
				if (!found) {
					cursor.removeSelectedText();
					continue;
				}
				auto       format = format_at(aDocument, at);
				const auto text   = aLink(*found, format);
				cursor.setPosition(at);
				cursor.setPosition(found->End, QTextCursor::KeepAnchor);
				cursor.insertText(text.Text, format);
				if (text.Raised >= 0) {
					// GitHub sets a second back reference's number raised: "↩²".
					cursor.setPosition(at + static_cast<int>(text.Raised));
					cursor.setPosition(at + static_cast<int>(text.Text.size()), QTextCursor::KeepAnchor);
					auto raised = QTextCharFormat{ };
					raised.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
					cursor.mergeCharFormat(raised);
				}
			}
		}

		void present_footnotes(QTextDocument& aDocument)
		{
			present_marks(aDocument, reference_mark, [](const parsed_mark& aMark, QTextCharFormat& aFormat) {
				aFormat.setAnchor(true);
				aFormat.setAnchorHref(QStringLiteral("#fn-%1").arg(aMark.Number));
				aFormat.setAnchorNames({ reference_name(aMark.Number, aMark.Reference) });
				aFormat.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
				aFormat.setProperty(markdown_internal_target_property, QStringLiteral("fnback-%1-%2").arg(aMark.Number).arg(aMark.Reference));
				// GitHub's stylesheet brackets the raised number: "[1]".
				return marked_text{ .Text = QLatin1Char{ '[' } + QString::number(aMark.Number) + QLatin1Char{ ']' } };
			});
			present_marks(aDocument, back_mark, [](const parsed_mark& aMark, QTextCharFormat& aFormat) {
				aFormat.setAnchor(true);
				aFormat.setAnchorHref(QLatin1Char{ '#' } + reference_name(aMark.Number, aMark.Reference));
				aFormat.setAnchorNames({ QStringLiteral("fnback-%1-%2").arg(aMark.Number).arg(aMark.Reference) });
				aFormat.setProperty(markdown_internal_target_property, reference_name(aMark.Number, aMark.Reference));
				// U+21A9 LEFTWARDS ARROW WITH HOOK, as GitHub writes it.
				if (aMark.Reference <= 1) {
					return marked_text{ .Text = QString{ QChar{ 0x21A9 } } };
				}
				return marked_text{ .Text = QChar{ 0x21A9 } + QString::number(aMark.Reference), .Raised = 1 };
			});
			auto first = QTextBlock{ };
			for (auto cursor = aDocument.find(QString{ item_mark }); !cursor.isNull(); cursor = aDocument.find(QString{ item_mark }, cursor.position())) {
				const auto found = parse_mark(aDocument, cursor.selectionStart());
				cursor.setPosition(found ? found->End : cursor.selectionEnd(), QTextCursor::KeepAnchor);
				cursor.removeSelectedText();
				if (!first.isValid()) {
					first = cursor.block();
				}
			}
			for (auto block = first; block.isValid(); block = block.next()) {
				auto cursor = QTextCursor{ block };
				auto format = block.blockFormat();
				format.setProperty(markdown_footnote_property, true);
				cursor.setBlockFormat(format);
			}
		}

		/// Removes one whole block, keeping its neighbours' formats.
		void remove_block(QTextDocument& aDocument, const QTextBlock& aBlock)
		{
			auto cursor = QTextCursor{ &aDocument };
			if (aBlock.previous().isValid()) {
				cursor.setPosition(aBlock.position() - 1);
				cursor.setPosition(aBlock.position() + aBlock.length() - 1, QTextCursor::KeepAnchor);
			} else {
				cursor.setPosition(aBlock.position());
				cursor.setPosition(aBlock.position() + aBlock.length() - 1, QTextCursor::KeepAnchor);
			}
			cursor.removeSelectedText();
		}

		void present_alerts(QTextDocument& aDocument)
		{
			auto             ordinal = 0;
			std::vector<int> ends;
			for (auto block = aDocument.begin(); block.isValid(); block = block.next()) {
				const auto text = block.text();
				if (text.size() == 1 && text[0] == alert_end) {
					ends.push_back(block.blockNumber());
					continue;
				}
				if (text.size() != 1 || text[0].unicode() < alert_first || text[0].unicode() >= alert_first + 5) {
					continue;
				}
				const auto kind = static_cast<int>(text[0].unicode() - alert_first) + 1;
				++ordinal;
				auto cursor = QTextCursor{ block };
				cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
				cursor.insertText(markdown_alert_title(static_cast<markdown_alert>(kind)));
				for (auto member = block; member.isValid(); member = member.next()) {
					const auto memberText = member.text();
					if (member != block && memberText.size() == 1 && memberText[0] == alert_end) {
						break;
					}
					auto format = member.blockFormat();
					format.setProperty(markdown_alert_property, kind);
					format.setProperty(markdown_alert_ordinal_property, ordinal);
					format.setProperty(markdown_alert_title_property, member == block);
					QTextCursor{ member }.setBlockFormat(format);
				}
			}
			for (auto it = ends.rbegin(); it != ends.rend(); ++it) {
				remove_block(aDocument, aDocument.findBlockByNumber(*it));
			}
		}

		/// A mark that an unexpected import shape left unpresented (one Qt
		/// split, or one inside a link's text) is removed, never shown.
		void remove_leftover_marks(QTextDocument& aDocument, const prepared_markdown& aPrepared)
		{
			const auto leftover = [&aPrepared](const QChar aChar) {
				return (aPrepared.Breaks && aChar == break_mark) || (aPrepared.Marks && aChar.unicode() >= first_reserved && aChar.unicode() <= last_reserved);
			};
			auto cursor = QTextCursor{ &aDocument };
			for (auto at = aDocument.characterCount() - 1; at >= 0; --at) {
				if (leftover(aDocument.characterAt(at))) {
					cursor.setPosition(at);
					cursor.setPosition(at + 1, QTextCursor::KeepAnchor);
					cursor.removeSelectedText();
				}
			}
		}

		// [[BPR-1544]]. Each HTML block mark becomes the sanitized HTML it
		// names, through Qt's HTML import; the reader's resource denial still
		// answers any request, and the HTML names no resource.
		void present_html_blocks(QTextDocument& aDocument, const prepared_markdown& aPrepared)
		{
			std::vector<std::pair<int, std::size_t>> found;
			for (auto block = aDocument.begin(); block.isValid(); block = block.next()) {
				const auto text = block.text();
				if (text.size() < 3 || text.front() != block_mark || text.back() != html_mark_close) {
					continue;
				}
				auto       number = false;
				const auto index  = text.mid(1, text.size() - 2).toUInt(&number);
				if (number && index < aPrepared.Html.size()) {
					found.emplace_back(block.blockNumber(), index);
				}
			}
			for (auto it = found.rbegin(); it != found.rend(); ++it) {
				const auto before = aDocument.blockCount();
				auto       cursor = QTextCursor{ aDocument.findBlockByNumber(it->first) };
				cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
				// Qt merges the first inserted paragraph into the block it is
				// inserted at, which would lose its alignment or heading; a
				// sentinel paragraph takes that merge and is removed.
				cursor.insertHtml(QStringLiteral("<p>") + block_mark + QStringLiteral("</p>") + aPrepared.Html[it->second]);
				if (const auto sentinel = aDocument.findBlockByNumber(it->first); sentinel.text() == QString{ block_mark }) {
					remove_block(aDocument, sentinel);
				}
				// An HTML <pre> reads as a code block, in the code font and fill.
				const auto inserted = aDocument.blockCount() - before + 1;
				for (auto block = aDocument.findBlockByNumber(it->first); block.isValid() && block.blockNumber() < it->first + inserted; block = block.next()) {
					if (auto format = block.blockFormat(); format.nonBreakableLines() && !format.hasProperty(QTextFormat::BlockCodeLanguage)) {
						format.setProperty(QTextFormat::BlockCodeLanguage, QString{ });
						QTextCursor{ block }.setBlockFormat(format);
					}
				}
			}
		}

		struct inline_mark
		{
			int  Start = 0;
			int  End   = 0;
			bool Open  = false;
			int  Kind  = 0;
			int  Field = -1;
		};

		/// The inline mark at `aAt`: its kind, optional field and end.
		auto read_inline_mark(const QTextDocument& aDocument, const int aAt) -> std::optional<inline_mark>
		{
			auto result = inline_mark{ .Start = aAt, .Open = aDocument.characterAt(aAt) == html_open_mark };
			auto field  = &result.Kind;
			auto digits = 0;
			for (auto at = aAt + 1; at < aDocument.characterCount() && at < aAt + 24; ++at) {
				const auto ch = aDocument.characterAt(at);
				if (ch == html_mark_close) {
					result.End = at + 1;
					return digits > 0 ? std::optional{ result } : std::nullopt;
				}
				if (ch == html_mark_field && field == &result.Kind) {
					result.Field = 0;
					field        = &result.Field;
				} else if (ch.isDigit() && ch.unicode() < 0x80) {
					*field = *field * 10 + (ch.unicode() - '0');
					++digits;
				} else {
					return std::nullopt;
				}
			}
			return std::nullopt;
		}

		auto inline_format(const int aKind, const int aField, const prepared_markdown& aPrepared) -> QTextCharFormat
		{
			auto format = QTextCharFormat{ };
			switch (static_cast<html_kind>(aKind)) {
			case html_kind::bold:
				format.setFontWeight(QFont::Bold);
				break;
			case html_kind::italic:
				format.setFontItalic(true);
				break;
			case html_kind::strike:
				format.setFontStrikeOut(true);
				break;
			case html_kind::underline:
				format.setFontUnderline(true);
				break;
			case html_kind::subscript:
				format.setVerticalAlignment(QTextCharFormat::AlignSubScript);
				break;
			case html_kind::superscript:
				format.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
				break;
			case html_kind::keyboard:
				format.setFontFixedPitch(true);
				format.setProperty(markdown_keyboard_property, true);
				break;
			case html_kind::code:
				format.setFontFixedPitch(true);
				break;
			case html_kind::link:
				if (aField >= 0 && static_cast<std::size_t>(aField) < aPrepared.Hrefs.size()) {
					format.setAnchor(true);
					format.setAnchorHref(aPrepared.Hrefs[static_cast<std::size_t>(aField)]);
				}
				break;
			default:
				break;
			}
			return format;
		}

		// [[BPR-1544]]. Inline HTML marks become character formats over the
		// text between an opening mark and its closing one (to the end of the
		// block when unclosed, as a browser closes it with its paragraph), and
		// `<br>` becomes a line separator.
		void present_inline_html(QTextDocument& aDocument, const prepared_markdown& aPrepared)
		{
			for (auto cursor = aDocument.find(QString{ html_break_mark }); !cursor.isNull(); cursor = aDocument.find(QString{ html_break_mark }, cursor.position())) {
				cursor.insertText(QString{ QChar::LineSeparator }, format_at(aDocument, cursor.selectionStart()));
			}
			std::vector<inline_mark> marks;
			for (const auto open : { html_open_mark, html_close_mark }) {
				for (auto cursor = aDocument.find(QString{ open }); !cursor.isNull(); cursor = aDocument.find(QString{ open }, cursor.position())) {
					if (const auto found = read_inline_mark(aDocument, cursor.selectionStart())) {
						marks.push_back(*found);
					}
				}
			}
			if (marks.empty()) {
				return;
			}
			std::ranges::sort(marks, { }, &inline_mark::Start);
			struct span
			{
				int             Start = 0;
				int             End   = 0;
				QTextCharFormat Format;
			};
			std::vector<span>        spans;
			std::vector<std::size_t> open;
			for (std::size_t index = 0; index < marks.size(); ++index) {
				const auto& item = marks[index];
				if (item.Open) {
					open.push_back(index);
					continue;
				}
				const auto match = std::ranges::find_if(open.rbegin(), open.rend(), [&](const std::size_t aOpen) { return marks[aOpen].Kind == item.Kind; });
				if (match == open.rend()) {
					continue;
				}
				const auto& start = marks[*match];
				spans.push_back({ .Start = start.End, .End = item.Start, .Format = inline_format(start.Kind, start.Field, aPrepared) });
				open.erase(std::next(match).base());
			}
			for (const auto index : open) {
				const auto& start = marks[index];
				const auto  block = aDocument.findBlock(start.Start);
				spans.push_back({ .Start = start.End, .End = block.position() + block.length() - 1, .Format = inline_format(start.Kind, start.Field, aPrepared) });
			}
			// Where a position lands once every mark before it is gone.
			std::vector<int> removedBefore{ 0 };
			for (const auto& item : marks) {
				removedBefore.push_back(removedBefore.back() + item.End - item.Start);
			}
			const auto shifted = [&](const int aPosition) {
				const auto after = std::ranges::upper_bound(marks, aPosition, { }, &inline_mark::End);
				return aPosition - removedBefore[static_cast<std::size_t>(after - marks.begin())];
			};
			for (auto& item : spans) {
				item.Start = shifted(item.Start);
				item.End   = shifted(item.End);
			}
			auto cursor = QTextCursor{ &aDocument };
			for (auto it = marks.rbegin(); it != marks.rend(); ++it) {
				cursor.setPosition(it->Start);
				cursor.setPosition(it->End, QTextCursor::KeepAnchor);
				cursor.removeSelectedText();
			}
			for (const auto& item : spans) {
				if (item.End > item.Start) {
					cursor.setPosition(item.Start);
					cursor.setPosition(item.End, QTextCursor::KeepAnchor);
					cursor.mergeCharFormat(item.Format);
				}
			}
		}

		auto disclosure(const bool aOpen) -> QString
		{
			// U+25BE and U+25B8, the small triangles a browser draws.
			return aOpen ? QStringLiteral("▾ ") : QStringLiteral("▸ ");
		}

		auto summary_format(const int aId, const bool aOpen) -> QTextCharFormat
		{
			auto format = QTextCharFormat{ };
			format.setAnchor(true);
			format.setAnchorHref(aOpen ? QStringLiteral("Hide details") : QStringLiteral("Show details"));
			format.setProperty(markdown_internal_target_property, markdown_details_target + QString::number(aId));
			format.setProperty(markdown_summary_property, aId);
			return format;
		}

		auto region_alignment(const QChar aCode) -> Qt::Alignment
		{
			return aCode == QLatin1Char{ 'c' } ? Qt::AlignHCenter : aCode == QLatin1Char{ 'r' } ? Qt::AlignRight
				: aCode == QLatin1Char{ 'j' }                                                   ? Qt::AlignJustify
																								: Qt::AlignLeft;
		}

		// [[BPR-1544]]. Region paragraphs: each `<details>` summary gets its
		// disclosure triangle and becomes the in-body link that toggles it,
		// and every block until its end records the `<details>` it is in; an
		// aligned region aligns its blocks and tables. The region paragraphs
		// themselves go.
		void present_regions(QTextDocument& aDocument)
		{
			auto                       next = 0;
			std::vector<int>           details;
			std::vector<Qt::Alignment> aligned;
			std::vector<int>           marks;
			for (auto block = aDocument.begin(); block.isValid(); block = block.next()) {
				const auto text = block.text();
				if (text.size() >= 3 && text[0] == region_mark) {
					const auto close = text.indexOf(html_mark_close);
					const auto code  = text[1];
					if (code == QLatin1Char{ '1' } || code == QLatin1Char{ '2' }) {
						const auto id     = ++next;
						const auto isOpen = code == QLatin1Char{ '2' };
						auto       format = block.blockFormat();
						format.setProperty(markdown_details_property, id);
						format.setProperty(markdown_details_open_property, isOpen);
						if (!details.empty()) {
							auto path = QStringList{ };
							for (const auto parent : details) {
								path.push_back(QString::number(parent));
							}
							format.setProperty(markdown_details_path_property, path.join(QLatin1Char{ '/' }));
						}
						auto cursor = QTextCursor{ block };
						cursor.setBlockFormat(format);
						cursor.setPosition(block.position());
						cursor.setPosition(block.position() + static_cast<int>(close) + 1, QTextCursor::KeepAnchor);
						cursor.insertText(disclosure(isOpen), format_at(aDocument, block.position() + static_cast<int>(close) + 1));
						cursor.setPosition(block.position());
						cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
						cursor.mergeCharFormat(summary_format(id, isOpen));
						details.push_back(id);
						continue;
					}
					if (code == QLatin1Char{ '0' } && !details.empty()) {
						details.pop_back();
					} else if (code == QLatin1Char{ 'a' }) {
						aligned.push_back(region_alignment(text.size() > 3 ? text[3] : QLatin1Char{ 'l' }));
					} else if (code == QLatin1Char{ 'z' } && !aligned.empty()) {
						aligned.pop_back();
					}
					marks.push_back(block.blockNumber());
					continue;
				}
				auto cursor = QTextCursor{ block };
				auto format = block.blockFormat();
				if (!details.empty()) {
					auto path = QStringList{ };
					for (const auto parent : details) {
						path.push_back(QString::number(parent));
					}
					format.setProperty(markdown_details_path_property, path.join(QLatin1Char{ '/' }));
				}
				if (!aligned.empty()) {
					if (auto* table = cursor.currentTable(); table != nullptr) {
						auto tableFormat = table->format();
						tableFormat.setAlignment(aligned.back());
						table->setFormat(tableFormat);
					} else {
						format.setAlignment(aligned.back());
					}
				}
				if (!details.empty() || !aligned.empty()) {
					cursor.setBlockFormat(format);
				}
			}
			for (auto it = marks.rbegin(); it != marks.rend(); ++it) {
				remove_block(aDocument, aDocument.findBlockByNumber(*it));
			}
			apply_markdown_details(aDocument);
		}

		void collapse_hidden_tables(QTextFrame& aFrame)
		{
			for (auto* frame : aFrame.childFrames()) {
				if (auto* table = qobject_cast<QTextTable*>(frame); table != nullptr && !table->cellAt(0, 0).firstCursorPosition().block().isVisible()) {
					auto format = table->format();
					format.setBorder(0);
					format.setBorderStyle(QTextFrameFormat::BorderStyle_None);
					format.setCellPadding(0);
					format.setCellSpacing(0);
					format.setMargin(0);
					table->setFormat(format);
				}
				collapse_hidden_tables(*frame);
			}
		}

		struct alert_hue
		{
			QRgb Light;
			QRgb Dark;
		};
		// GitHub's own alert colours (Primer fg.accent, success, done,
		// attention and danger) on its light and dark themes.
		constexpr auto alert_hues = std::array{
			alert_hue{ 0x0969da, 0x4493f8 },
			alert_hue{ 0x1a7f37, 0x3fb950 },
			alert_hue{ 0x8250df, 0xab7df8 },
			alert_hue{ 0x9a6700, 0xd29922 },
			alert_hue{ 0xd1242f, 0xf85149 },
		};

		auto contrast(const QColor& aFirst, const QColor& aSecond) -> double
		{
			return contrast_ratio(to_appearance_color(aFirst), to_appearance_color(aSecond));
		}
	} // namespace

	auto prepare_github_markdown(const QString& aSource) -> prepared_markdown
	{
		auto prepared   = prepared_markdown{ };
		prepared.Text   = replace_emoji(remove_comments(aSource));
		prepared.Marks  = !holds_reserved(aSource);
		prepared.Breaks = !aSource.contains(break_mark);
		if (prepared.Marks) {
			prepared.Text = mark_html_inline(mark_html_blocks(prepared.Text, prepared), prepared);
			prepared.Text = mark_alerts(move_footnotes(prepared.Text));
		}
		if (prepared.Breaks) {
			prepared.Text = mark_breaks(prepared.Text);
		}
		return prepared;
	}

	void present_github_markdown(QTextDocument& aDocument, const prepared_markdown& aPrepared)
	{
		// Qt's underline for `_text_` is converted before any `<ins>` adds one.
		present_underscore_emphasis(aDocument);
		if (aPrepared.Marks) {
			present_html_blocks(aDocument, aPrepared);
			present_inline_html(aDocument, aPrepared);
		}
		if (aPrepared.Breaks) {
			present_breaks(aDocument);
		}
		if (aPrepared.Marks) {
			present_footnotes(aDocument);
			present_alerts(aDocument);
			present_regions(aDocument);
		}
		remove_leftover_marks(aDocument, aPrepared);
	}

	void apply_markdown_details(QTextDocument& aDocument)
	{
		std::map<int, bool> open;
		for (auto block = aDocument.begin(); block.isValid(); block = block.next()) {
			if (const auto format = block.blockFormat(); format.hasProperty(markdown_details_property)) {
				open[format.intProperty(markdown_details_property)] = format.boolProperty(markdown_details_open_property);
			}
		}
		if (open.empty()) {
			return;
		}
		auto changed = false;
		for (auto block = aDocument.begin(); block.isValid(); block = block.next()) {
			const auto path    = block.blockFormat().stringProperty(markdown_details_path_property);
			auto       visible = true;
			for (const auto& id : QStringView{ path }.split(QLatin1Char{ '/' }, Qt::SkipEmptyParts)) {
				visible = visible && open[id.toInt()];
			}
			if (block.isVisible() != visible) {
				auto handle = block;
				handle.setVisible(visible);
				changed = true;
			}
		}
		collapse_hidden_tables(*aDocument.rootFrame());
		if (changed) {
			aDocument.markContentsDirty(0, aDocument.characterCount());
		}
	}

	auto toggle_markdown_details(QTextDocument& aDocument, const int aId) -> std::optional<bool>
	{
		for (auto block = aDocument.begin(); block.isValid(); block = block.next()) {
			auto format = block.blockFormat();
			if (!format.hasProperty(markdown_details_property) || format.intProperty(markdown_details_property) != aId) {
				continue;
			}
			const auto isOpen = !format.boolProperty(markdown_details_open_property);
			format.setProperty(markdown_details_open_property, isOpen);
			auto cursor = QTextCursor{ block };
			cursor.setBlockFormat(format);
			cursor.setPosition(block.position());
			cursor.setPosition(block.position() + 2, QTextCursor::KeepAnchor);
			cursor.insertText(disclosure(isOpen), format_at(aDocument, block.position()));
			cursor.setPosition(block.position());
			cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
			cursor.mergeCharFormat(summary_format(aId, isOpen));
			apply_markdown_details(aDocument);
			return isOpen;
		}
		return std::nullopt;
	}

	auto markdown_details_open(const QTextDocument& aDocument, const int aId) -> bool
	{
		for (auto block = aDocument.begin(); block.isValid(); block = block.next()) {
			const auto format = block.blockFormat();
			if (format.hasProperty(markdown_details_property) && format.intProperty(markdown_details_property) == aId) {
				return format.boolProperty(markdown_details_open_property);
			}
		}
		return false;
	}

	auto markdown_keyboard_spans(const QTextDocument& aDocument) -> std::vector<std::pair<int, int>>
	{
		std::vector<std::pair<int, int>> spans;
		for (auto block = aDocument.begin(); block.isValid(); block = block.next()) {
			for (auto it = block.begin(); !it.atEnd(); ++it) {
				const auto fragment = it.fragment();
				if (!fragment.charFormat().boolProperty(markdown_keyboard_property)) {
					continue;
				}
				if (!spans.empty() && spans.back().second == fragment.position()) {
					spans.back().second += fragment.length();
				} else {
					spans.emplace_back(fragment.position(), fragment.position() + fragment.length());
				}
			}
		}
		return spans;
	}

	auto markdown_alert_title(const markdown_alert aAlert) -> QString
	{
		switch (aAlert) {
		case markdown_alert::note:
			return QStringLiteral("Note");
		case markdown_alert::tip:
			return QStringLiteral("Tip");
		case markdown_alert::important:
			return QStringLiteral("Important");
		case markdown_alert::warning:
			return QStringLiteral("Warning");
		case markdown_alert::caution:
			return QStringLiteral("Caution");
		case markdown_alert::none:
			break;
		}
		return { };
	}

	auto markdown_alert_colour(const markdown_alert aAlert, const QPalette& aPalette) -> QColor
	{
		const auto index = std::clamp(static_cast<int>(aAlert) - 1, 0, static_cast<int>(alert_hues.size()) - 1);
		const auto base  = aPalette.color(QPalette::Active, QPalette::Base);
		const auto ink   = aPalette.color(QPalette::Active, QPalette::Text);
		const auto hue   = QColor{ base.lightnessF() < 0.5 ? alert_hues[static_cast<std::size_t>(index)].Dark : alert_hues[static_cast<std::size_t>(index)].Light };
		// A high-contrast theme puts its text at the extreme of its base.
		const auto target = contrast(ink, base) >= 18.0 ? 7.0 : 4.5;
		auto       colour = hue;
		for (auto step = 1; step <= 10 && contrast(colour, base) < target; ++step) {
			const auto amount = step / 10.0;
			colour            = QColor::fromRgbF(static_cast<float>(hue.redF() + (ink.redF() - hue.redF()) * amount), static_cast<float>(hue.greenF() + (ink.greenF() - hue.greenF()) * amount),
												 static_cast<float>(hue.blueF() + (ink.blueF() - hue.blueF()) * amount));
		}
		return colour;
	}

	auto markdown_github_style_of(const QPalette& aPalette, const QFont& aFont) -> markdown_github_style
	{
		auto style = markdown_github_style{ };
		for (std::size_t index = 0; index < style.Alerts.size(); ++index) {
			style.Alerts[index] = markdown_alert_colour(static_cast<markdown_alert>(index + 1), aPalette);
		}
		// The text colour 32% of the way to the base: at least 4.5:1, as the
		// notes' quiet marks are drawn.
		const auto ink  = aPalette.color(QPalette::Active, QPalette::Text);
		const auto base = aPalette.color(QPalette::Active, QPalette::Base);
		const auto mix  = [](const float aInk, const float aBase) { return aInk + (aBase - aInk) * 0.32F; };
		style.Muted     = QColor::fromRgbF(mix(ink.redF(), base.redF()), mix(ink.greenF(), base.greenF()), mix(ink.blueF(), base.blueF()));
		style.Base      = aFont;
		// [[BPR-1544]]. A key cap: the base 6% toward the text, edged by the
		// text 70% toward the base, as GitHub's subtle key in either theme.
		const auto toward = [](const QColor& aFrom, const QColor& aTo, const float aAmount) {
			return QColor::fromRgbF(aFrom.redF() + (aTo.redF() - aFrom.redF()) * aAmount, aFrom.greenF() + (aTo.greenF() - aFrom.greenF()) * aAmount, aFrom.blueF() + (aTo.blueF() - aFrom.blueF()) * aAmount);
		};
		style.KeyFill = toward(base, ink, 0.06F);
		style.KeyEdge = toward(ink, base, 0.7F);
		style.Ink     = ink;
		return style;
	}

	void style_github_markdown(QTextDocument& aDocument, const markdown_github_style& aStyle)
	{
		struct fragment_style
		{
			int             Position = 0;
			int             Length   = 0;
			QTextCharFormat Format;
		};
		// GitHub sets footnotes at 12 px beside 14 px prose.
		auto small = QTextCharFormat{ };
		if (aStyle.Base.pointSizeF() > 0.0) {
			small.setFontPointSize(aStyle.Base.pointSizeF() * 0.86);
		} else if (aStyle.Base.pixelSize() > 0) {
			small.setProperty(QTextFormat::FontPixelSize, static_cast<int>(std::lround(aStyle.Base.pixelSize() * 0.86)));
		}
		auto key = QTextCharFormat{ };
		if (aStyle.Base.pointSizeF() > 0.0) {
			key.setFontPointSize(aStyle.Base.pointSizeF() * 0.85);
		} else if (aStyle.Base.pixelSize() > 0) {
			key.setProperty(QTextFormat::FontPixelSize, static_cast<int>(std::lround(aStyle.Base.pixelSize() * 0.85)));
		}
		key.setBackground(aStyle.KeyFill);
		std::vector<fragment_style> fragments;
		for (auto block = aDocument.begin(); block.isValid(); block = block.next()) {
			auto       format   = block.blockFormat();
			const auto alert    = format.intProperty(markdown_alert_property);
			const auto footnote = format.boolProperty(markdown_footnote_property);
			auto       cursor   = QTextCursor{ block };
			if (alert > 0 && cursor.currentTable() == nullptr) {
				// No quote fill: the bar alone marks an alert, as on GitHub.
				if (!code_block(block)) {
					format.clearBackground();
				}
				format.setLeftMargin(aStyle.AlertIndent + 40.0 * std::max(0, format.intProperty(QTextFormat::BlockQuoteLevel) - 1));
				// An alert stands apart from what is around it, so two alerts
				// in a row show two bars.
				const auto ordinal = format.intProperty(markdown_alert_ordinal_property);
				if (format.boolProperty(markdown_alert_title_property) && block.previous().isValid()) {
					format.setTopMargin(std::max(format.topMargin(), aStyle.AlertGap));
				}
				if (block.next().isValid() && block.next().blockFormat().intProperty(markdown_alert_ordinal_property) != ordinal) {
					format.setBottomMargin(std::max(format.bottomMargin(), aStyle.AlertGap));
				}
				cursor.setBlockFormat(format);
			}
			const auto title = alert > 0 && format.boolProperty(markdown_alert_title_property);
			fragments.clear();
			for (auto it = block.begin(); !it.atEnd(); ++it) {
				const auto fragment = it.fragment();
				auto       chars    = fragment.charFormat();
				if (title) {
					chars.setForeground(aStyle.Alerts[static_cast<std::size_t>(std::clamp(alert, 1, 5) - 1)]);
					chars.setFontWeight(QFont::DemiBold);
				}
				if (footnote && !chars.fontFixedPitch()) {
					chars.merge(small);
					if (!chars.isAnchor()) {
						chars.setForeground(aStyle.Muted);
					}
				}
				// [[BPR-1544]]. A summary reads as text, as GitHub's does; its
				// triangle says it opens. A key is a small cap.
				if (chars.hasProperty(markdown_summary_property)) {
					chars.setForeground(aStyle.Ink);
					chars.setFontUnderline(false);
				}
				if (chars.boolProperty(markdown_keyboard_property)) {
					chars.merge(key);
				}
				if (chars == fragment.charFormat()) {
					continue;
				}
				fragments.push_back({ .Position = fragment.position(), .Length = fragment.length(), .Format = chars });
			}
			for (const auto& fragment : fragments) {
				cursor.setPosition(fragment.Position);
				cursor.setPosition(fragment.Position + fragment.Length, QTextCursor::KeepAnchor);
				cursor.setCharFormat(fragment.Format);
			}
		}
		for (auto* frame : aDocument.rootFrame()->childFrames()) {
			auto* table = qobject_cast<QTextTable*>(frame);
			if (table != nullptr && table->cellAt(0, 0).firstCursorPosition().block().blockFormat().intProperty(markdown_alert_property) > 0) {
				auto format = table->format();
				format.setLeftMargin(aStyle.AlertIndent);
				table->setFormat(format);
			}
		}
	}

	auto markdown_alert_extents(const QTextDocument& aDocument) -> std::vector<markdown_alert_extent>
	{
		std::vector<markdown_alert_extent> extents;
		auto                               ordinal = 0;
		for (auto block = aDocument.begin(); block.isValid(); block = block.next()) {
			const auto format = block.blockFormat();
			if (!format.hasProperty(markdown_alert_property)) {
				continue;
			}
			const auto own = format.intProperty(markdown_alert_ordinal_property);
			if (own != ordinal) {
				ordinal = own;
				extents.push_back({ .First = block.blockNumber(), .Last = block.blockNumber(), .Alert = static_cast<markdown_alert>(format.intProperty(markdown_alert_property)) });
			}
			extents.back().Last = block.blockNumber();
		}
		return extents;
	}
} // namespace bpr::qt::detail
