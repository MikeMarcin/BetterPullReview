#include "conversation_preview.hpp"

#include "markdown_github.hpp"

#include <QTextBlock>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextFrame>
#include <QTextList>
#include <QTextTable>

#include <algorithm>
#include <array>

namespace bpr::qt::detail {
	namespace {
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
			return std::ranges::none_of(aBody, [](const QChar aCharacter) { return std::ranges::find(marks, aCharacter.unicode()) != marks.end(); });
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
		const auto language = aLanguage.trimmed().section(QLatin1Char{ ' ' }, 0, 0).toLower();
		struct name
		{
			const char* Written;
			const char* Shown;
		};
		static constexpr auto names = std::array{
			name{ "suggestion", "Suggestion" }, name{ "cpp", "C++" }, name{ "c++", "C++" }, name{ "cxx", "C++" }, name{ "cc", "C++" }, name{ "hpp", "C++" }, name{ "h", "C++" },
			name{ "c", "C" }, name{ "objc", "Objective-C" }, name{ "py", "Python" }, name{ "python", "Python" }, name{ "js", "JavaScript" }, name{ "javascript", "JavaScript" },
			name{ "ts", "TypeScript" }, name{ "typescript", "TypeScript" }, name{ "sh", "Shell" }, name{ "bash", "Shell" }, name{ "zsh", "Shell" }, name{ "shell", "Shell" },
			name{ "console", "Shell" }, name{ "powershell", "PowerShell" }, name{ "ps1", "PowerShell" }, name{ "json", "JSON" }, name{ "yaml", "YAML" }, name{ "yml", "YAML" },
			name{ "toml", "TOML" }, name{ "diff", "Diff" }, name{ "patch", "Diff" }, name{ "rust", "Rust" }, name{ "rs", "Rust" }, name{ "go", "Go" }, name{ "java", "Java" },
			name{ "kotlin", "Kotlin" }, name{ "swift", "Swift" }, name{ "cs", "C#" }, name{ "csharp", "C#" }, name{ "cmake", "CMake" }, name{ "sql", "SQL" }, name{ "html", "HTML" },
			name{ "xml", "XML" }, name{ "css", "CSS" }, name{ "md", "Markdown" }, name{ "markdown", "Markdown" }, name{ "text", "Text" }, name{ "txt", "Text" },
			name{ "plaintext", "Text" }, name{ "log", "Log" }, name{ "mermaid", "Diagram" }, name{ "qml", "QML" },
		};
		if (language.isEmpty()) {
			return QStringLiteral("Code");
		}
		for (const auto& one : names) {
			if (language == QLatin1StringView{ one.Written }) {
				return QString::fromLatin1(one.Shown);
			}
		}
		return aLanguage.trimmed().section(QLatin1Char{ ' ' }, 0, 0);
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
}
