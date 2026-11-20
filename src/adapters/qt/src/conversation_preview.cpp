#include "conversation_preview.hpp"

#include "markdown_github.hpp"

#include "bpr/core/markdown_code.hpp"

#include <QColor>
#include <QFontMetrics>
#include <QPainter>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextFrame>
#include <QTextList>
#include <QTextTable>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace bpr::qt::detail {
	namespace {
		// A token chip's measures, Discuss's ([[BPR-2171]]).
		constexpr int token_pad    = 5;
		constexpr int token_gap    = 4;
		constexpr int token_height = 15;

		/// \brief Whether a body can hold no Markdown but inline code: one
		/// line, none of the characters a block or an emphasis starts with.
		[[nodiscard]] auto plain(const QString& aBody) -> bool
		{
			static constexpr auto marks = std::array{ u'\n', u'\r', u'*', u'_', u'[', u']', u'<', u'>', u'!', u'#', u'|', u'~', u'\\', u'&' };
			const auto            text  = QStringView{ aBody }.trimmed();
			if (text.isEmpty()) {
				return true;
			}
			if (text.front() == u'-' || text.front() == u'+' || text.front().isDigit()) {
				return false;
			}
			if (std::ranges::any_of(aBody, [](const QChar aCharacter) { return std::ranges::find(marks, aCharacter.unicode()) != marks.end(); })) {
				return false;
			}
			// [[BPR-2300]]. A `:name:` may be an emoji, which only the GitHub
			// import replaces.
			for (auto at = aBody.indexOf(QLatin1Char{ ':' }); at >= 0; at = aBody.indexOf(QLatin1Char{ ':' }, at + 1)) {
				auto end = at + 1;
				while (end < aBody.size() && (aBody.at(end).isLower() || aBody.at(end).isDigit() || QStringView{ u"_+-" }.contains(aBody.at(end)))) {
					++end;
				}
				if (end > at + 1 && end < aBody.size() && aBody.at(end) == QLatin1Char{ ':' }) {
					return false;
				}
			}
			return true;
		}

		/// \brief Collapses spaces, line breaks and object marks to single
		/// spaces.
		[[nodiscard]] auto spaced(const QChar aCharacter) -> bool
		{
			return aCharacter.isSpace() || aCharacter == QChar::LineSeparator || aCharacter == QChar::ParagraphSeparator || aCharacter == QChar::ObjectReplacementCharacter
				|| aCharacter == QChar{ 0x00A0 };
		}

		class builder
		{
		public:
			note_preview Preview;

			void token(const QString& aWord)
			{
				if (!Preview.Tokens.contains(aWord)) {
					Preview.Tokens.push_back(aWord);
				}
			}

			/// Appends words, collapsing space; \p aCode sets them in the code
			/// font, \p aStrong in semibold.
			void words(const QString& aText, const bool aCode, const bool aStrong)
			{
				const auto start = static_cast<int>(Preview.Text.size());
				for (const auto character : aText) {
					if (spaced(character)) {
						if (!Preview.Text.isEmpty() && !Preview.Text.back().isSpace()) {
							Preview.Text += QLatin1Char{ ' ' };
						}
						continue;
					}
					Preview.Text += character;
				}
				const auto length = static_cast<int>(Preview.Text.size()) - start;
				if (length <= 0) {
					return;
				}
				// A range starts at its first character, not the space before it.
				auto from = start;
				while (from < start + length && Preview.Text.at(from).isSpace()) {
					++from;
				}
				if (from < start + length && aCode) {
					Preview.Code.emplace_back(from, start + length - from);
				} else if (from < start + length && aStrong) {
					Preview.Strong.emplace_back(from, start + length - from);
				}
			}

			void space()
			{
				if (!Preview.Text.isEmpty() && !Preview.Text.back().isSpace()) {
					Preview.Text += QLatin1Char{ ' ' };
				}
			}

			/// Takes one block's words; images in it name a token.
			void take(const QTextBlock& aBlock, const bool aStrong)
			{
				for (auto fragment = aBlock.begin(); !fragment.atEnd(); ++fragment) {
					const auto part = fragment.fragment();
					if (!part.isValid()) {
						continue;
					}
					const auto format = part.charFormat();
					if (format.isImageFormat()) {
						token(QStringLiteral("Image"));
						continue;
					}
					words(part.text(), format.fontFixedPitch(), aStrong || format.hasProperty(markdown_summary_property));
				}
			}

			void finish()
			{
				while (!Preview.Text.isEmpty() && Preview.Text.back().isSpace()) {
					Preview.Text.chop(1);
				}
				for (auto* const ranges : { &Preview.Code, &Preview.Strong }) {
					for (auto& [start, length] : *ranges) {
						length = std::max(0, std::min(start + length, static_cast<int>(Preview.Text.size())) - start);
					}
					std::erase_if(*ranges, [](const auto& aRange) { return aRange.second <= 0; });
				}
			}
		};

		/// \brief Whether a block holds words: not only images and spaces.
		[[nodiscard]] auto has_words(const QTextBlock& aBlock) -> bool
		{
			for (auto fragment = aBlock.begin(); !fragment.atEnd(); ++fragment) {
				const auto part = fragment.fragment();
				if (part.isValid() && !part.charFormat().isImageFormat() && std::ranges::any_of(part.text(), [](const QChar aCharacter) { return !spaced(aCharacter); })) {
					return true;
				}
			}
			return false;
		}

		[[nodiscard]] auto has_image(const QTextBlock& aBlock) -> bool
		{
			for (auto fragment = aBlock.begin(); !fragment.atEnd(); ++fragment) {
				if (fragment.fragment().isValid() && fragment.fragment().charFormat().isImageFormat()) {
					return true;
				}
			}
			return false;
		}

		struct walk
		{
			builder Build;
			/// The first paragraph (or list item) has been taken.
			bool Taken = false;
			/// A leading heading has been taken; its paragraph comes next.
			bool Headed = false;

			void block(const QTextBlock& aBlock)
			{
				const auto format = aBlock.blockFormat();
				if (format.boolProperty(markdown_hidden_property) || format.boolProperty(markdown_footnote_property)) {
					return;
				}
				if (format.hasProperty(markdown_details_property) || !format.stringProperty(markdown_details_path_property).isEmpty()) {
					Build.token(QStringLiteral("Details"));
					return;
				}
				if (format.hasProperty(QTextFormat::BlockCodeFence) || format.hasProperty(QTextFormat::BlockCodeLanguage) || format.nonBreakableLines()) {
					Build.token(code_token(format.stringProperty(QTextFormat::BlockCodeLanguage)));
					return;
				}
				if (format.intProperty(QTextFormat::BlockQuoteLevel) > 0 || format.hasProperty(markdown_alert_property)) {
					Build.token(QStringLiteral("Quote"));
					return;
				}
				if (format.hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth)) {
					return;
				}
				const auto words = has_words(aBlock);
				if (!words) {
					if (has_image(aBlock)) {
						Build.token(QStringLiteral("Image"));
					}
					return;
				}
				if (aBlock.textList() != nullptr) {
					// A body that opens with a list shows its first item; the
					// list's token names the rest.
					Build.token(QStringLiteral("List"));
					if (!Taken) {
						Build.space();
						Build.take(aBlock, false);
						Taken = true;
					}
					return;
				}
				if (format.headingLevel() > 0) {
					if (!Taken && !Headed) {
						Build.take(aBlock, true);
						Headed = true;
						return;
					}
					Build.Preview.Omitted = true;
					return;
				}
				if (!Taken) {
					Build.space();
					Build.take(aBlock, false);
					Taken = true;
					return;
				}
				Build.Preview.Omitted = true;
			}

			void frame(const QTextFrame& aFrame)
			{
				for (auto at = aFrame.begin(); !at.atEnd(); ++at) {
					if (auto* const child = at.currentFrame(); child != nullptr) {
						if (qobject_cast<QTextTable*>(child) != nullptr) {
							Build.token(QStringLiteral("Table"));
							continue;
						}
						frame(*child);
						continue;
					}
					block(at.currentBlock());
				}
			}
		};

		/// \brief A plain body: inline code between backticks in the code
		/// font, the rest as words.
		[[nodiscard]] auto plain_preview(const QString& aBody) -> note_preview
		{
			auto build = builder{ };
			auto at    = qsizetype{ 0 };
			while (at < aBody.size()) {
				const auto open  = aBody.indexOf(QLatin1Char{ '`' }, at);
				const auto close = open < 0 ? -1 : aBody.indexOf(QLatin1Char{ '`' }, open + 1);
				if (open < 0 || close < 0) {
					build.words(aBody.mid(at), false, false);
					break;
				}
				build.words(aBody.mid(at, open - at), false, false);
				build.words(aBody.mid(open + 1, close - open - 1), true, false);
				at = close + 1;
			}
			build.finish();
			return build.Preview;
		}
	}

	auto code_token(const QString& aLanguage) -> QString
	{
		// GitHub's Linguist names, as the open card's code blocks read them.
		const auto info     = aLanguage.trimmed().toStdString();
		const auto language = core::markdown_code_language_of(info);
		switch (language.Kind) {
		case core::markdown_code_kind::suggestion:
			return QStringLiteral("Suggestion");
		case core::markdown_code_kind::diff:
			return QStringLiteral("Diff");
		case core::markdown_code_kind::language:
			return QString::fromUtf8(language.Name.data(), static_cast<qsizetype>(language.Name.size()));
		case core::markdown_code_kind::plain:
			break;
		}
		const auto word = aLanguage.trimmed().section(QLatin1Char{ ' ' }, 0, 0).section(QLatin1Char{ ',' }, 0, 0);
		return word.isEmpty() ? QStringLiteral("Code") : word;
	}

	auto preview_of(const QString& aBody, const qsizetype aHeaderLength) -> note_preview
	{
		auto preview = note_preview{ };
		if (plain(aBody)) {
			preview = plain_preview(aBody);
		} else {
			auto document = QTextDocument{ };
			import_github_markdown(document, aBody);
			auto reading = walk{ };
			reading.frame(*document.rootFrame());
			reading.Build.finish();
			preview = std::move(reading.Build.Preview);
		}
		// The recognized header, as written but without its emphasis marks,
		// in semibold when the words open with it.
		if (aHeaderLength > 0) {
			auto header = aBody.left(aHeaderLength);
			header.remove(QLatin1Char{ '*' });
			header.remove(QLatin1Char{ '_' });
			header = header.simplified();
			if (!header.isEmpty() && preview.Text.startsWith(header)) {
				std::erase_if(preview.Strong, [&header](const auto& aRange) { return aRange.first < header.size(); });
				preview.Strong.insert(preview.Strong.begin(), { 0, static_cast<int>(header.size()) });
			}
		}
		return preview;
	}

	auto preview_runs(const note_preview& aPreview) -> std::vector<preview_run>
	{
		const auto size  = static_cast<int>(aPreview.Text.size());
		auto       kinds = std::vector<std::uint8_t>(static_cast<std::size_t>(size), 0U);
		for (const auto& [start, length] : aPreview.Strong) {
			for (auto at = std::max(0, start); at < std::min(size, start + length); ++at) {
				kinds[static_cast<std::size_t>(at)] = 2U;
			}
		}
		for (const auto& [start, length] : aPreview.Code) {
			for (auto at = std::max(0, start); at < std::min(size, start + length); ++at) {
				kinds[static_cast<std::size_t>(at)] = 1U;
			}
		}
		auto runs = std::vector<preview_run>{ };
		for (auto at = 0; at < size;) {
			auto end = at;
			while (end < size && kinds[static_cast<std::size_t>(end)] == kinds[static_cast<std::size_t>(at)]) {
				++end;
			}
			runs.push_back({ .Text = aPreview.Text.mid(at, end - at), .Code = kinds[static_cast<std::size_t>(at)] == 1U, .Strong = kinds[static_cast<std::size_t>(at)] == 2U });
			at = end;
		}
		return runs;
	}

	auto fit_preview_line(std::vector<preview_run> aRuns, const bool aOmitted, const QFont& aFont, const QFont& aCodeFont, const int aWidth) -> std::vector<preview_run>
	{
		auto strong = aFont;
		strong.setWeight(QFont::DemiBold);
		const auto metrics = [&](const preview_run& aRun) { return QFontMetricsF{ aRun.Code ? aCodeFont : aRun.Strong ? strong
																													  : aFont }; };
		auto       total   = 0.0;
		for (const auto& run : aRuns) {
			total += metrics(run).horizontalAdvance(run.Text);
		}
		const auto ellipsis = QString{ QChar{ 0x2026 } };
		if (total <= aWidth && !aOmitted) {
			return aRuns;
		}
		// Where the words stop: the last character that fits beside the
		// ellipsis, as an index into the runs' joined text.
		auto       text  = QString{ };
		auto       first = std::vector<int>{ };
		const auto room  = aWidth - QFontMetricsF{ aFont }.horizontalAdvance(ellipsis);
		auto       cut   = 0;
		auto       x     = 0.0;
		auto       full  = true;
		for (const auto& run : aRuns) {
			first.push_back(static_cast<int>(text.size()));
			text += run.Text;
		}
		for (std::size_t index = 0U; index < aRuns.size() && full; ++index) {
			const auto& run     = aRuns[index];
			const auto  measure = metrics(run);
			const auto  advance = measure.horizontalAdvance(run.Text);
			if (x + advance <= room) {
				x += advance;
				cut = first[index] + static_cast<int>(run.Text.size());
				continue;
			}
			// The longest prefix of this run that fits.
			auto low  = 0;
			auto high = static_cast<int>(run.Text.size());
			while (low < high) {
				const auto middle = (low + high + 1) / 2;
				if (x + measure.horizontalAdvance(run.Text.left(middle)) <= room) {
					low = middle;
				} else {
					high = middle - 1;
				}
			}
			cut  = first[index] + low;
			full = false;
		}
		if (!full) {
			// Back to a word boundary, never inside inline code but when it
			// opens the line.
			auto boundary = cut;
			while (boundary > 0 && !text.at(boundary - 1).isSpace() && !text.at(boundary).isSpace()) {
				--boundary;
			}
			if (boundary > 0) {
				cut = boundary;
			}
			for (std::size_t index = 0U; index < aRuns.size(); ++index) {
				const auto end = first[index] + static_cast<int>(aRuns[index].Text.size());
				if (aRuns[index].Code && first[index] > 0 && cut > first[index] && cut < end) {
					cut = first[index];
				}
			}
		}
		// The ellipsis takes the place of a sentence's last stop.
		while (cut > 0 && (text.at(cut - 1).isSpace() || QStringView{ u".,;:" }.contains(text.at(cut - 1)))) {
			--cut;
		}
		auto result = std::vector<preview_run>{ };
		for (std::size_t index = 0U; index < aRuns.size() && first[index] < cut; ++index) {
			auto run = std::move(aRuns[index]);
			run.Text.truncate(cut - first[index]);
			result.push_back(std::move(run));
		}
		result.push_back({ .Text = ellipsis });
		return result;
	}

	auto preview_token_font(const QFont& aFont) -> QFont
	{
		auto font = aFont;
		if (font.pointSizeF() > 0.0) {
			font.setPointSizeF(font.pointSizeF() * 0.8);
		} else if (font.pixelSize() > 0) {
			font.setPixelSize(std::max(1, static_cast<int>(std::lround(font.pixelSize() * 0.8))));
		}
		return font;
	}

	auto preview_tokens_width(const QStringList& aTokens, const QFont& aFont) -> int
	{
		if (aTokens.isEmpty()) {
			return 0;
		}
		const auto metrics = QFontMetrics{ preview_token_font(aFont) };
		auto       width   = 0;
		for (const auto& token : aTokens) {
			width += metrics.horizontalAdvance(token) + 2 * token_pad + token_gap;
		}
		return width - token_gap;
	}

	void paint_preview_tokens(QPainter& aPainter, const QStringList& aTokens, const QFont& aFont, const int aLeft, const int aMiddle, const QColor& aRule, const QColor& aInk)
	{
		const auto tokenFont = preview_token_font(aFont);
		const auto metrics   = QFontMetrics{ tokenFont };
		auto       x         = aLeft;
		const auto top       = aMiddle - token_height / 2;
		aPainter.save();
		aPainter.setFont(tokenFont);
		for (const auto& token : aTokens) {
			const auto chip = QRectF{ static_cast<qreal>(x) + 0.5, static_cast<qreal>(top) + 0.5, static_cast<qreal>(metrics.horizontalAdvance(token) + 2 * token_pad) - 1.0, static_cast<qreal>(token_height) - 1.0 };
			aPainter.setPen(QPen{ aRule, 1.0 });
			aPainter.setBrush(Qt::NoBrush);
			aPainter.drawRoundedRect(chip, 3.0, 3.0);
			aPainter.setPen(aInk);
			aPainter.drawText(chip, Qt::AlignCenter, token);
			x += metrics.horizontalAdvance(token) + 2 * token_pad + token_gap;
		}
		aPainter.restore();
	}
}
