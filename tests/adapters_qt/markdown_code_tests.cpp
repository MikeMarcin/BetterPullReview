// RFC-BPR-0036 owner amendment 2: fenced code in the reader as GitHub draws
// it. [[BPR-1562]] language colours through the composed classification path
// (packaged definitions, the reader's own classification host, the reader),
// arriving without moving anything; [[BPR-1563]] diff blocks; [[BPR-1564]]
// suggested changes. GitHub's answers are the fixtures under
// tests/fixtures/markdown/code-blocks-v1: its Markdown API rendering of each
// body, and the suggested-change rows of a review on MikeMarcin/BPR-Test#80.

#include "../../src/adapters/qt/src/markdown_code.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/syntax/packaged_syntax_classifier.hpp"
#include "bpr/application/markdown_code_syntax.hpp"
#include "bpr/core/markdown_code.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/code_sample_painter.hpp"
#include "bpr/qt/markdown_body_view.hpp"

#include <QAbstractButton>
#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextLayout>
#include <QVBoxLayout>

#include <array>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#ifndef BPR_MARKDOWN_CODE_FIXTURES
	#error "BPR_MARKDOWN_CODE_FIXTURES must name the code-blocks fixture directory"
#endif

namespace {
	using namespace bpr;
	using bpr::tests::expect;

	// The private block properties markdown_code.hpp gives presentation.
	constexpr auto ordinal_property = QTextFormat::UserProperty + 0x0B60;
	constexpr auto row_property     = QTextFormat::UserProperty + 0x0B62;
	constexpr auto number_property  = QTextFormat::UserProperty + 0x0B63;

	void settle()
	{
		QApplication::processEvents();
		QApplication::processEvents();
	}

	auto read_file(const QString& aName) -> QByteArray
	{
		QFile file{ QStringLiteral(BPR_MARKDOWN_CODE_FIXTURES "/") + aName };
		expect(file.open(QIODevice::ReadOnly), "fixture opens: " + aName.toStdString());
		return file.readAll();
	}

	auto fixture(const QString& aName) -> std::string { return read_file(aName + QStringLiteral(".md")).toStdString(); }

	auto unescape(QString aText) -> QString
	{
		return aText.replace(QStringLiteral("&lt;"), QStringLiteral("<")).replace(QStringLiteral("&gt;"), QStringLiteral(">")).replace(QStringLiteral("&quot;"), QStringLiteral("\"")).replace(QStringLiteral("&#39;"), QStringLiteral("'")).replace(QStringLiteral("&amp;"), QStringLiteral("&"));
	}

	struct theme
	{
		core::appearance_mode    Mode;
		core::appearance_variant Variant;
		const char*              Name;
	};
	constexpr auto themes = std::array{
		theme{ core::appearance_mode::light, core::appearance_variant::light, "light" },
		theme{ core::appearance_mode::dark, core::appearance_variant::dark, "dark" },
		theme{ core::appearance_mode::high_contrast_light, core::appearance_variant::high_contrast_light, "high-contrast-light" },
		theme{ core::appearance_mode::high_contrast_dark, core::appearance_variant::high_contrast_dark, "high-contrast-dark" },
	};

	auto palette_of(const core::appearance_mode aMode) -> QPalette
	{
		auto profile = qt::appearance_profile{ };
		profile.select_mode(aMode);
		return profile.derive(QApplication::palette());
	}

	auto code_theme(const core::appearance_variant aVariant) -> qt::markdown_code_theme
	{
		return { .Colors = qt::code_role_colors_of(qt::builtin_appearance_palette(aVariant)), .Variant = aVariant };
	}

	void use_theme(qt::markdown_body_view& aView, const theme& aTheme)
	{
		aView.setPalette(palette_of(aTheme.Mode));
		aView.set_code_theme(code_theme(aTheme.Variant));
	}

	auto contrast(const QColor& aFirst, const QColor& aSecond) -> double
	{
		return qt::contrast_ratio(qt::to_appearance_color(aFirst), qt::to_appearance_color(aSecond));
	}

	auto document_of(qt::markdown_body_view& aView) -> QTextDocument& { return *aView.rendered_view()->document(); }

	auto ordinal_blocks(QTextDocument& aDocument, const int aOrdinal) -> std::vector<QTextBlock>
	{
		auto blocks = std::vector<QTextBlock>{ };
		for (auto block = aDocument.begin(); block.isValid(); block = block.next()) {
			if (block.blockFormat().hasProperty(ordinal_property) && block.blockFormat().intProperty(ordinal_property) == aOrdinal) {
				blocks.push_back(block);
			}
		}
		return blocks;
	}

	auto formats_of(const QTextBlock& aBlock) -> QList<QTextLayout::FormatRange>
	{
		return aBlock.layout() == nullptr ? QList<QTextLayout::FormatRange>{ } : aBlock.layout()->formats();
	}

	/// \brief The reader's texts read by the composed classifier: the
	/// composition's blob source, over the host's held texts.
	class held_texts final : public adapters::syntax::blob_source
	{
	public:
		explicit held_texts(application::markdown_code_texts& aTexts)
			: Texts{ aTexts }
		{
		}
		auto blob_bytes(const core::git_blob_oid& aBlob) -> std::optional<std::string> override { return Texts.bytes(aBlob); }

	private:
		application::markdown_code_texts& Texts;
	};

	class manual_executor final : public application::decision_executor, public application::effect_executor
	{
	public:
		void                              post(std::function<void()> aTask) override { Tasks.push_back(std::move(aTask)); }
		std::deque<std::function<void()>> Tasks;
	};

	/// \brief The composed path: packaged definitions and classifier, the
	/// reader's own classification host, answers delivered to the reader.
	struct composed
	{
		application::markdown_code_texts                           Texts;
		held_texts                                                 Source{ Texts };
		adapters::syntax::packaged_definition_catalog              Catalog;
		adapters::syntax::packaged_syntax_classifier               Classifier{ Catalog, Source };
		manual_executor                                            Effects;
		manual_executor                                            Decisions;
		std::vector<application::markdown_code_syntax_publication> Published;
		std::size_t                                                Dispatches = 0U;
		application::markdown_code_syntax_host                     Host{ Classifier, Texts, Effects, Decisions,
																		 [this](const std::string_view aSelector) -> std::optional<application::syntax_definition_version> {
														 auto definition = Catalog.definition_for_path(aSelector);
														 if (definition.Name == adapters::syntax::plain_text_definition_name) {
															 return std::nullopt;
														 }
														 return definition;
													 },
													 [this](const application::markdown_code_syntax_publication& aPublication) { Published.push_back(aPublication); } };

		void bind(qt::markdown_body_view& aView)
		{
			aView.set_code_syntax_dispatch([this](std::vector<application::markdown_code_syntax_request> aRequests) {
				++Dispatches;
				Host.request(aRequests);
			});
		}

		/// Runs the lanes, then hands every answer to every reader, as the
		/// window hands each answer to all of its readers.
		void deliver_all(const std::vector<qt::markdown_body_view*>& aViews)
		{
			while (!Effects.Tasks.empty() || !Decisions.Tasks.empty()) {
				for (auto* lane : { &Effects, &Decisions }) {
					while (!lane->Tasks.empty()) {
						auto task = std::move(lane->Tasks.front());
						lane->Tasks.pop_front();
						task();
					}
				}
			}
			for (const auto& publication : std::exchange(Published, { })) {
				for (auto* view : aViews) {
					view->apply_code_syntax(publication);
				}
			}
			settle();
		}

		/// Runs the lanes, then hands every answer to the reader.
		void deliver(qt::markdown_body_view& aView)
		{
			while (!Effects.Tasks.empty() || !Decisions.Tasks.empty()) {
				for (auto* lane : { &Effects, &Decisions }) {
					while (!lane->Tasks.empty()) {
						auto task = std::move(lane->Tasks.front());
						lane->Tasks.pop_front();
						task();
					}
				}
			}
			for (const auto& publication : std::exchange(Published, { })) {
				aView.apply_code_syntax(publication);
			}
			settle();
		}
	};

	void show(qt::markdown_body_view& aView, const std::string& aBody, const std::string& aIdentity)
	{
		aView.set_body(aBody, aIdentity);
		settle();
		expect(aView.rendered_available() && !aView.source_visible(), "body renders: " + aIdentity + " " + aView.fallback_reason().toStdString());
	}

	/// [[BPR-1562]] AC1. Every info string of the fixture against GitHub's
	/// decision, through the composed classifier.
	void check_languages(qt::markdown_body_view& aView, composed& aPath)
	{
		use_theme(aView, themes[0]);
		show(aView, fixture(QStringLiteral("languages")), "languages:1");
		aPath.deliver(aView);
		const auto  html     = QString::fromUtf8(read_file(QStringLiteral("languages.github.html")));
		const auto* store    = aView.code_store();
		const auto& blocks   = store->blocks();
		auto&       doc      = document_of(aView);
		const auto  roles    = code_theme(core::appearance_variant::light).Colors;
		auto        entry    = QRegularExpression{ QStringLiteral(R"re(<p dir="auto"><code class="notranslate">([^<]*)</code>(?: \(form\))?</p>\s*<(div class="highlight highlight-([^"]+)"|pre|section|math-renderer))re") };
		auto        index    = std::size_t{ 0U };
		auto        coloured = 0;
		auto        missing  = QStringList{ };
		auto        quiet    = QStringList{ };
		for (auto it = entry.globalMatch(html); it.hasNext(); ++index) {
			const auto match = it.next();
			const auto info  = unescape(match.captured(1));
			const auto scope = match.captured(3);
			expect(index < blocks.size() && blocks[index].Placed, "every fixture block is placed: " + info.toStdString());
			const auto& block         = blocks[index];
			const auto  lines         = ordinal_blocks(doc, static_cast<int>(block.Fence.Ordinal));
			const auto  formats       = lines.empty() ? QList<QTextLayout::FormatRange>{ } : formats_of(lines.front());
			const auto  githubColours = !scope.isEmpty() && scope != QStringLiteral("source-diff");
			if (!githubColours || scope == QStringLiteral("source-diff")) {
				if (scope != QStringLiteral("source-diff")) {
					expect(formats.isEmpty() && !block.Spans, "GitHub leaves " + info.toStdString() + " plain, and so does the reader");
				}
				continue;
			}
			if (block.Language.Kind != core::markdown_code_kind::language) {
				// GitHub colours it; no packaged definition does (console, txt).
				missing.push_back(info + QStringLiteral(" (") + scope + QLatin1Char{ ')' });
				expect(formats.isEmpty(), "an uncovered language stays plain: " + info.toStdString());
				continue;
			}
			if (!block.Spans) {
				missing.push_back(info + QStringLiteral(" (") + scope + QStringLiteral(", no packaged definition)"));
				expect(formats.isEmpty(), "a language without a packaged definition stays plain: " + info.toStdString());
				continue;
			}
			if (block.Spans->empty() || formats.isEmpty()) {
				// Classified, but this snippet holds no token the language colours.
				quiet.push_back(info);
				continue;
			}
			// Each coloured run is the panes' colour for its span's role.
			const auto& spans = *block.Spans;
			for (const auto& range : formats) {
				const auto colour = range.format.foreground().color();
				const auto found  = std::ranges::any_of(spans, [&](const application::token_span& aSpan) {
					return aSpan.Line == 0U && roles[static_cast<std::size_t>(aSpan.Role)] == colour;
				});
				expect(found, "a coloured run wears its role's colour: " + info.toStdString());
			}
			expect(!formats.isEmpty(), "a classified block is coloured: " + info.toStdString());
			++coloured;
		}
		std::cout << "languages: " << index << " info strings, " << coloured << " coloured; GitHub colours but the packaged definitions do not: " << missing.join(QStringLiteral(", ")).toStdString() << "; classified with nothing to colour in the snippet: " << quiet.join(QStringLiteral(", ")).toStdString() << '\n';
		expect(index == 147U, "the fixture's info strings were all compared");
		for (const auto* required : { "cpp", "c++", "py", "js", "ts", "rust", "go", "json", "yaml", "sh", "bash", "cmake" }) {
			const auto found = std::ranges::find_if(blocks, [&](const qt::detail::markdown_code_store::block& aBlock) { return aBlock.Fence.Info == required; });
			expect(found != blocks.end() && found->Spans && !found->Spans->empty(), std::string{ "the owner's languages are coloured: " } + required);
		}
		// Indented code and inline code are never coloured.
		const auto indented = [&doc] {
			for (auto block = doc.begin(); block.isValid(); block = block.next()) {
				if (block.text() == QStringLiteral("int main() { return 0; }")) {
					return block;
				}
			}
			return QTextBlock{ };
		}();
		expect(indented.isValid() && !indented.blockFormat().hasProperty(ordinal_property) && formats_of(indented).isEmpty(), "indented code is not coloured");
	}

	/// [[BPR-1562]] AC2. Colour arrives with nothing moved; the cache answers
	/// a second reader; a theme change classifies nothing.
	void check_arrival(qt::markdown_body_view& aView, composed& aPath)
	{
		use_theme(aView, themes[0]);
		auto body = std::string{ };
		for (auto index = 0; index < 60; ++index) {
			body += "Paragraph " + std::to_string(index) + " of prose that the reader is reading.\n\n";
			if (index == 30 || index == 59) {
				body += "```cpp\ntemplate <typename T> auto twice(T aValue) -> T { return aValue * 2; } // " + std::to_string(index) + "\n```\n\n";
			}
		}
		aView.resize(600, 300);
		const auto before = aPath.Host.classifications();
		show(aView, body, "arrival:1");
		auto* const reader = aView.rendered_view();
		reader->verticalScrollBar()->setValue(reader->verticalScrollBar()->maximum() / 2);
		auto cursor = reader->textCursor();
		cursor.setPosition(200);
		cursor.setPosition(260, QTextCursor::KeepAnchor);
		reader->setTextCursor(cursor);
		settle();
		const auto scroll = reader->verticalScrollBar()->value();
		const auto height = reader->document()->size().height();
		const auto text   = reader->document()->toRawText();
		expect(!aPath.Effects.Tasks.empty(), "the reader asked for its code to be classified");
		expect(formats_of(ordinal_blocks(document_of(aView), 0).front()).isEmpty(), "plain until the colours arrive");
		aPath.deliver(aView);
		expect(!formats_of(ordinal_blocks(document_of(aView), 0).front()).isEmpty(), "coloured when they arrive");
		expect(reader->verticalScrollBar()->value() == scroll && reader->document()->size().height() == height, "the reading position and height do not move");
		expect(reader->textCursor().selectionStart() == 200 && reader->textCursor().selectionEnd() == 260 && reader->document()->toRawText() == text, "the selection and text are unchanged");
		const auto classified = aPath.Host.classifications() - before;
		expect(classified == 2U, "two texts, two classifications: " + std::to_string(classified));

		// A second reader of the same code is answered from the cache.
		auto second = qt::markdown_body_view{ };
		second.set_code_theme(code_theme(core::appearance_variant::light));
		aPath.bind(second);
		const auto hits = aPath.Host.cache_hits();
		second.set_body("```cpp\ntemplate <typename T> auto twice(T aValue) -> T { return aValue * 2; } // 30\n```\n", "arrival:second");
		aPath.deliver(second);
		expect(aPath.Host.classifications() - before == 2U && aPath.Host.cache_hits() == hits + 1U, "the same text and language is classified once");
		expect(!formats_of(ordinal_blocks(document_of(second), 0).front()).isEmpty(), "the cached answer colours the second reader");

		// A theme change recolours from the kept spans and classifies nothing.
		const auto dispatches = aPath.Dispatches;
		use_theme(aView, themes[1]);
		settle();
		const auto dark  = code_theme(core::appearance_variant::dark).Colors;
		const auto first = formats_of(ordinal_blocks(document_of(aView), 0).front());
		expect(!first.isEmpty() && std::ranges::all_of(first, [&dark](const QTextLayout::FormatRange& aRange) { return std::ranges::find(dark, aRange.format.foreground().color()) != dark.end(); }),
			   "a theme change recolours in the new theme's role colours");
		expect(aPath.Dispatches == dispatches && aPath.Effects.Tasks.empty(), "a theme change asks for nothing");

		// Negative controls: an answer for other text or another language
		// colours nothing; without a dispatch nothing is asked for.
		show(aView, "```py\nx = 1\n```\n", "arrival:negative");
		aView.apply_code_syntax({ .Selector = "code.py", .Text = std::make_shared<const std::string>("x = 2"), .Spans = std::make_shared<const application::token_span_list>(application::token_span_list{ { .Line = 0U, .Column = 0U, .Length = 1U, .Role = core::appearance_role::syntax_keyword } }) });
		aView.apply_code_syntax({ .Selector = "code.cpp", .Text = std::make_shared<const std::string>("x = 1"), .Spans = std::make_shared<const application::token_span_list>(application::token_span_list{ { .Line = 0U, .Column = 0U, .Length = 1U, .Role = core::appearance_role::syntax_keyword } }) });
		expect(formats_of(ordinal_blocks(document_of(aView), 0).front()).isEmpty(), "an answer for another text or language colours nothing");
		aPath.deliver(aView);
		auto lone = qt::markdown_body_view{ };
		lone.set_body("```py\nx = 1\n```\n", "arrival:lone");
		expect(!lone.code_store()->requests().empty() && aPath.Effects.Tasks.empty(), "a reader without a dispatch asks for nothing");
		use_theme(aView, themes[0]);
	}

	/// [[BPR-PENDING-AQ6]], owner look v004 ("Not drawing inline comment with
	/// syntax highlighing"). A body that asks colours its inline code as the
	/// named file's language, through the same host; a keyboard key and every
	/// body that does not ask stay plain.
	void check_inline(composed& aPath)
	{
		auto view = qt::markdown_body_view{ };
		view.set_in_place(true);
		view.set_code_theme(code_theme(core::appearance_variant::light));
		aPath.bind(view);
		view.resize(600, 300);
		view.show();
		const auto body = std::string{ "Line 34 returns `diagnostic::invalid_calibration` when `validate(scale, bias, gamma)` fails.\n\nPress <kbd>Ctrl</kbd> to stop.\n\n```cpp\nint x = 1;\n```\n" };
		show(view, body, "inline:1");
		expect(view.code_store()->inline_code_texts().empty() && view.code_store()->requests().size() == 1U, "a body that does not ask leaves inline code plain and asks for its fenced code alone");
		aPath.deliver(view);
		const auto paragraph = document_of(view).begin();
		expect(formats_of(paragraph).isEmpty(), "plain inline code is not coloured");
		const auto text = document_of(view).toRawText();
		view.set_inline_code_selector("code.cpp");
		settle();
		const auto& texts = view.code_store()->inline_code_texts();
		expect(texts.size() == 2U && *texts[0].Text == "diagnostic::invalid_calibration" && *texts[1].Text == "validate(scale, bias, gamma)", "asked, each inline code span is held once, a keyboard key not at all");
		aPath.deliver(view);
		const auto coloured = formats_of(document_of(view).begin());
		const auto line     = document_of(view).begin().text();
		const auto first    = line.indexOf(QStringLiteral("diagnostic"));
		const auto second   = line.indexOf(QStringLiteral("validate"));
		expect(!coloured.isEmpty() && std::ranges::all_of(coloured, [&](const QTextLayout::FormatRange& aRange) {
			return (aRange.start >= first && aRange.start + aRange.length <= first + 31) || (aRange.start >= second && aRange.start + aRange.length <= second + 28);
		}),
			   "its colours arrive, and only inside the code spans");
		expect(document_of(view).toRawText() == text, "the text is unchanged");
		auto keyed = false;
		for (auto block = document_of(view).begin(); block.isValid(); block = block.next()) {
			keyed = keyed || (block.text().contains(QStringLiteral("Ctrl")) && !formats_of(block).isEmpty());
		}
		expect(!keyed, "a keyboard key is not coloured");
		view.set_inline_code_selector({ });
		settle();
		expect(formats_of(document_of(view).begin()).isEmpty() && view.code_store()->inline_code_texts().empty(), "and without the language it is plain again");
		aPath.deliver(view);
	}

	/// [[BPR-1563]]. Each diff line's marks, colours and contrast.
	void check_diff(qt::markdown_body_view& aView)
	{
		// A diff line's tab after its sign is kept by Qt's import; the body
		// renders and keeps the tab.
		show(aView, "```diff\n-\told();\n+\tnew();\n \tsame();\n```\n", "diff:tabs");
		expect(document_of(aView).toRawText().contains(QStringLiteral("-\told();")) && document_of(aView).toRawText().contains(QStringLiteral(" \tsame();")), "a diff's tabs are kept");
		for (const auto& theme : themes) {
			use_theme(aView, theme);
			show(aView, fixture(QStringLiteral("diff")) + "\n" + fixture(QStringLiteral("diff-probe")), std::string{ "diff:" } + theme.Name);
			auto&      doc     = document_of(aView);
			const auto colours = code_theme(theme.Variant).Colors;
			const auto high    = theme.Variant == core::appearance_variant::high_contrast_light || theme.Variant == core::appearance_variant::high_contrast_dark;
			const auto target  = high ? 7.0 : 4.5;
			auto       checked = 0;
			const auto back    = aView.rendered_view()->palette().color(QPalette::Active, QPalette::Window);
			for (const auto& block : aView.code_store()->blocks()) {
				if (block.Language.Kind != core::markdown_code_kind::diff) {
					continue;
				}
				const auto lines = ordinal_blocks(doc, static_cast<int>(block.Fence.Ordinal));
				expect(lines.size() == block.Fence.Lines.size(), "every diff line placed");
				for (std::size_t index = 0U; index < lines.size(); ++index) {
					const auto marks   = core::markdown_diff_marks(block.Fence.Lines[index]);
					const auto formats = formats_of(lines[index]);
					expect(static_cast<std::size_t>(formats.size()) == marks.size(), "one format per GitHub mark: " + block.Fence.Lines[index]);
					for (qsizetype at = 0; at < formats.size(); ++at) {
						const auto& format = formats[at].format;
						const auto  kind   = marks[static_cast<std::size_t>(at)].Kind;
						const auto  fill   = format.background().style() == Qt::NoBrush ? back : format.background().color();
						if (kind == core::markdown_diff_mark_kind::inserted) {
							expect(fill == colours[static_cast<std::size_t>(core::appearance_role::diff_added_background)], "an added line wears the added fill");
						} else if (kind == core::markdown_diff_mark_kind::deleted) {
							expect(fill == colours[static_cast<std::size_t>(core::appearance_role::diff_removed_background)], "a removed line wears the removed fill");
						} else if (kind == core::markdown_diff_mark_kind::range || kind == core::markdown_diff_mark_kind::separator) {
							expect(format.fontWeight() == QFont::Bold, "a range or separator is bold");
						}
						const auto ratio = contrast(format.foreground().color(), fill);
						expect(ratio >= target, "a diff mark reads at " + std::to_string(target) + " in " + theme.Name + ": " + block.Fence.Lines[index] + " " + std::to_string(ratio));
						++checked;
					}
				}
			}
			expect(checked >= 30, "diff marks were checked");
			// The fill is drawn: a pixel at the start of an added line's text.
			const auto added = [&doc] {
				for (auto block = doc.begin(); block.isValid(); block = block.next()) {
					if (block.text() == QStringLiteral("+new line")) {
						return block;
					}
				}
				return QTextBlock{ };
			}();
			auto* const reader = aView.rendered_view();
			reader->ensureCursorVisible();
			auto cursor = QTextCursor{ added };
			reader->setTextCursor(cursor);
			reader->ensureCursorVisible();
			settle();
			const auto image = reader->viewport()->grab().toImage();
			const auto at    = reader->cursorRect(cursor);
			const auto pixel = image.pixelColor((QPointF{ static_cast<qreal>(at.left()) + 1.5, static_cast<qreal>(at.top()) + 1.5 } * image.devicePixelRatio()).toPoint());
			const auto want  = colours[static_cast<std::size_t>(core::appearance_role::diff_added_background)];
			expect(std::abs(pixel.red() - want.red()) <= 4 && std::abs(pixel.green() - want.green()) <= 4 && std::abs(pixel.blue() - want.blue()) <= 4,
				   std::string{ "the added fill is painted in " } + theme.Name);
			// Indented diff text stays plain.
			for (auto block = doc.begin(); block.isValid(); block = block.next()) {
				if (block.text() == QStringLiteral("-not removed")) {
					expect(formats_of(block).isEmpty() && !block.blockFormat().hasProperty(ordinal_property), "an indented diff is ordinary code");
				}
			}
		}
		use_theme(aView, themes[0]);
	}

	/// The preview budgets still bound the work: a body near 64 KiB of fenced
	/// code in three kinds renders, is placed and is classified within the
	/// reader's one-second local ceiling.
	void check_bounds(qt::markdown_body_view& aView, composed& aPath)
	{
		use_theme(aView, themes[0]);
		aView.set_suggestions(qt::markdown_suggestion_presentation{ .Path = "src/a.cpp", .Lines = core::markdown_suggestion_context{ .FirstLine = 1U, .Lines = { "int a = 0;" } } });
		auto body   = std::string{ };
		auto blocks = 0;
		// About 60 KB in under 2,048 lines: blocks of eight long lines.
		while (body.size() < 58'000U) {
			const auto n    = std::to_string(blocks);
			const auto kind = blocks % 3;
			body += kind == 0 ? "```cpp\n" : kind == 1 ? "```diff\n@@ -1,4 +1,4 @@\n"
													   : "```suggestion\n";
			for (auto line = 0; line < 8; ++line) {
				const auto text = std::string{ "auto value" }.append(n).append("_").append(std::to_string(line)).append(" = compute(first, second, third) + offset; // note ").append(n);
				body.append(kind == 1 ? (line % 2 == 0 ? "-" : "+") : "").append(text).append("\n");
			}
			body += "```\n\n";
			++blocks;
		}
		auto timer = QElapsedTimer{ };
		timer.start();
		aView.set_body(body, "bounds:1");
		settle();
		const auto rendered = timer.elapsed();
		aPath.deliver(aView);
		const auto coloured = timer.elapsed();
		std::cout << "bounds: " << body.size() << " bytes, " << blocks << " blocks, rendered in " << rendered << " ms, coloured by " << coloured << " ms\n";
		expect(aView.rendered_available(), "a 58 KB body of fenced code renders: " + aView.fallback_reason().toStdString());
		expect(rendered < 1000, "it renders within the one-second ceiling");
		expect(static_cast<int>(aView.code_store()->blocks().size()) == blocks && std::ranges::all_of(aView.code_store()->blocks(), &qt::detail::markdown_code_store::block::Placed),
			   "every block is placed");
		aView.set_suggestions(std::nullopt);
	}

	struct github_row
	{
		bool    Added  = false;
		int     Number = 0;
		QString Text;
		QString Marked;
	};

	/// GitHub's rows of each suggested change, from the captured page.
	auto github_suggestions() -> std::vector<std::vector<github_row>>
	{
		const auto html  = QString::fromUtf8(read_file(QStringLiteral("suggestions.github.html")));
		auto       boxes = std::vector<std::vector<github_row>>{ };
		for (const auto& box : html.split(QStringLiteral("js-suggested-changes-blob"), Qt::SkipEmptyParts).mid(1)) {
			auto rows = std::vector<github_row>{ };
			auto row  = QRegularExpression{ QStringLiteral(R"re(data-line-number="(\d+)"></td>\s*<td class="[^"]*blob-code-(deletion|addition)[^"]*">([\s\S]*?)</td>)re") };
			for (auto it = row.globalMatch(box); it.hasNext();) {
				const auto match = it.next();
				auto       item  = github_row{ .Added = match.captured(2) == QStringLiteral("addition"), .Number = match.captured(1).toInt() };
				const auto cell  = match.captured(3);
				if (const auto marked = QRegularExpression{ QStringLiteral(R"re(<span class="x[^"]*">([^<]*)</span>)re") }.match(cell); marked.hasMatch()) {
					item.Marked = unescape(marked.captured(1));
				}
				auto text = cell;
				text.remove(QRegularExpression{ QStringLiteral("<[^>]*>") });
				item.Text = unescape(text);
				rows.push_back(item);
			}
			boxes.push_back(rows);
		}
		return boxes;
	}

	/// [[BPR-1564]]. The review's four suggestions, as GitHub shows them.
	void check_suggestions(qt::markdown_body_view& aView)
	{
		use_theme(aView, themes[0]);
		const auto file     = QString::fromUtf8(read_file(QStringLiteral("suggestions-file.cpp"))).split(QLatin1Char{ '\n' });
		const auto review   = QJsonDocument::fromJson(read_file(QStringLiteral("suggestions.json"))).object();
		const auto comments = review[QStringLiteral("comments")].toArray();
		const auto github   = github_suggestions();
		expect(github.size() == 4U && static_cast<std::size_t>(comments.size()) == github.size(), "four suggestions on both sides");
		for (qsizetype index = 0; index < comments.size(); ++index) {
			const auto comment = comments[index].toObject();
			const auto first   = comment[QStringLiteral("start_line")].toInt();
			const auto last    = comment[QStringLiteral("line")].toInt();
			auto       context = core::markdown_suggestion_context{ .FirstLine = static_cast<std::uint32_t>(first) };
			for (auto line = first; line <= last; ++line) {
				context.Lines.push_back(file[line - 1].toStdString());
			}
			aView.set_suggestions(qt::markdown_suggestion_presentation{ .Path = review[QStringLiteral("path")].toString().toStdString(), .Lines = context });
			show(aView, comment[QStringLiteral("body")].toString().toStdString(), "suggestion:" + std::to_string(index));
			auto& doc    = document_of(aView);
			auto  rows   = std::vector<github_row>{ };
			auto  header = 0;
			for (auto block = doc.begin(); block.isValid(); block = block.next()) {
				const auto row = block.blockFormat().intProperty(row_property);
				if (row == 3) {
					++header;
					expect(block.text() == QStringLiteral("Suggested change"), "the box is headed Suggested change");
				} else if (row == 1 || row == 2) {
					auto item = github_row{ .Added = row == 2, .Number = block.blockFormat().intProperty(number_property), .Text = block.text() };
					for (const auto& format : formats_of(block)) {
						if (format.format.background().style() != Qt::NoBrush) {
							item.Marked = block.text().mid(format.start, format.length);
						}
					}
					rows.push_back(item);
				}
			}
			expect(header == 1, "one header per suggestion");
			const auto& expected = github[static_cast<std::size_t>(index)];
			expect(rows.size() == expected.size(), "as many rows as GitHub: suggestion " + std::to_string(index));
			for (std::size_t at = 0U; at < rows.size(); ++at) {
				const auto& ours   = rows[at];
				const auto& theirs = expected[at];
				expect(ours.Added == theirs.Added && ours.Number == theirs.Number && ours.Text == theirs.Text && ours.Marked == theirs.Marked,
					   "row " + std::to_string(at) + " of suggestion " + std::to_string(index) + " is GitHub's: " + ours.Text.toStdString() + " [" + ours.Marked.toStdString() + "] vs " + theirs.Text.toStdString() + " [" + theirs.Marked.toStdString() + "]");
			}
			expect(!doc.toRawText().contains(QChar{ 0xE0E0 }), "no anchor mark reaches the reader");
		}

		// The fills are painted under the rows, the gutter darker.
		{
			const auto& comment = comments[0].toObject();
			aView.set_suggestions(qt::markdown_suggestion_presentation{ .Path = "src/cache.cpp", .Lines = core::markdown_suggestion_context{ .FirstLine = 9U, .Lines = { file[8].toStdString() } } });
			show(aView, comment[QStringLiteral("body")].toString().toStdString(), "suggestion:paint");
			auto* const reader  = aView.rendered_view();
			auto&       doc     = document_of(aView);
			const auto  colours = code_theme(core::appearance_variant::light).Colors;
			reader->verticalScrollBar()->setValue(0);
			settle();
			const auto image = reader->viewport()->grab().toImage();
			for (auto block = doc.begin(); block.isValid(); block = block.next()) {
				const auto row = block.blockFormat().intProperty(row_property);
				if (row != 1 && row != 2) {
					continue;
				}
				const auto rect  = doc.documentLayout()->blockBoundingRect(block);
				const auto y     = rect.center().y();
				const auto right = image.pixelColor((QPointF{ static_cast<qreal>(reader->viewport()->width()) - 12.0, y } * image.devicePixelRatio()).toPoint());
				const auto want  = colours[static_cast<std::size_t>(row == 2 ? core::appearance_role::diff_added_background : core::appearance_role::diff_removed_background)];
				expect(std::abs(right.red() - want.red()) <= 4 && std::abs(right.green() - want.green()) <= 4 && std::abs(right.blue() - want.blue()) <= 4, "a row's fill runs to the box's edge");
				const auto gutter = image.pixelColor((QPointF{ doc.documentMargin() + 3.0, y } * image.devicePixelRatio()).toPoint());
				expect(gutter != right, "the number gutter is its own tint");
			}
		}

		// A body that opens with its suggestion keeps the box's gap above it.
		{
			auto post = qt::markdown_body_view{ };
			post.set_in_place(true);
			post.set_suggestions(qt::markdown_suggestion_presentation{ .Path = "src/cache.cpp", .Lines = core::markdown_suggestion_context{ .FirstLine = 1U, .Lines = { "a" } } });
			post.set_body("```suggestion\nb\n```\n", "suggestion:first");
			auto&      doc   = document_of(post);
			const auto first = doc.begin();
			expect(first.blockFormat().intProperty(row_property) == 3 && first.layout()->lineCount() > 0 && doc.documentLayout()->blockBoundingRect(first).top() + first.layout()->lineAt(0).y() >= 14.0,
				   "a body opening with a suggestion keeps the box's gap above its header");
		}
		// Without the commented lines: the added rows alone, unnumbered.
		aView.set_suggestions(qt::markdown_suggestion_presentation{ .Path = "src/cache.cpp" });
		show(aView, "Outdated:\n\n```suggestion\n\tauto next = read();\n```\n", "suggestion:no-context");
		auto added = 0;
		for (auto block = document_of(aView).begin(); block.isValid(); block = block.next()) {
			const auto row = block.blockFormat().intProperty(row_property);
			expect(row != 1, "no removed rows without the commented lines");
			if (row == 2) {
				++added;
				expect(!block.blockFormat().hasProperty(number_property), "unnumbered without the commented lines");
			}
		}
		expect(added == 1, "the added row alone");

		// Outside a line comment a suggestion is plain code, as on GitHub.
		aView.set_suggestions(std::nullopt);
		show(aView, "Description:\n\n```suggestion\n\tauto next = read();\n```\n\n```suggestion\n```\n", "suggestion:plain");
		for (auto block = document_of(aView).begin(); block.isValid(); block = block.next()) {
			expect(block.blockFormat().intProperty(row_property) == 0 && formats_of(block).isEmpty(), "a suggestion outside a line comment is plain code");
		}
		expect(!document_of(aView).toRawText().contains(QStringLiteral("Suggested change")), "no header outside a line comment");
		// No apply action exists: the reader offers no control beyond its own.
		expect(aView.findChildren<QAbstractButton*>().size() == 2, "no apply or commit control is added");
	}

	/// Renders each fixture in each theme, for review beside GitHub's page.
	void capture(qt::markdown_body_view& aView, composed& aPath)
	{
		const auto root = qEnvironmentVariable("BPR_MARKDOWN_RENDER_DIR");
		if (root.isEmpty()) {
			return;
		}
		expect(QDir{ }.mkpath(root), "create capture directory");
		// A real monospaced face for the review sheets, where the platform has
		// one; the offscreen default fixed font is not monospaced on macOS.
		auto code = QFontDatabase::systemFont(QFontDatabase::FixedFont);
		if (QFontDatabase::hasFamily(QStringLiteral("Menlo"))) {
			code = QFont{ QStringLiteral("Menlo"), 12 };
		}
		const auto tab = 4.0 * QFontMetricsF{ code }.horizontalAdvance(QLatin1Char{ ' ' });
		aView.set_code_font(code, tab);
		for (const auto& theme : themes) {
			for (const auto& page : { QStringLiteral("pr-sample"), QStringLiteral("diff") }) {
				aView.set_suggestions(std::nullopt);
				use_theme(aView, theme);
				aView.resize(760, 400);
				show(aView, fixture(page), (page + QLatin1Char{ ':' } + QString::fromLatin1(theme.Name)).toStdString());
				aPath.deliver(aView);
				auto* const reader = aView.rendered_view();
				reader->document()->setTextWidth(reader->viewport()->width());
				aView.resize(760, static_cast<int>(reader->document()->size().height()) + (aView.height() - reader->viewport()->height()) + 8);
				settle();
				expect(aView.grab().save(root + QLatin1Char{ '/' } + page + QLatin1Char{ '-' } + QString::fromLatin1(theme.Name) + QStringLiteral(".png")), "save capture");
			}
		}
		// In place, as posts of the review read in the Comments drawer.
		const auto file     = QString::fromUtf8(read_file(QStringLiteral("suggestions-file.cpp"))).split(QLatin1Char{ '\n' });
		const auto comments = QJsonDocument::fromJson(read_file(QStringLiteral("suggestions.json"))).object()[QStringLiteral("comments")].toArray();
		for (const auto& theme : themes) {
			auto holder = QWidget{ };
			holder.setPalette(palette_of(theme.Mode));
			holder.setAutoFillBackground(true);
			holder.show();
			auto posts = std::vector<qt::markdown_body_view*>{ };
			for (const auto& value : comments) {
				const auto comment = value.toObject();
				auto*      post    = new qt::markdown_body_view{ &holder };
				post->set_in_place(true);
				post->set_code_font(code, tab);
				post->set_code_theme(code_theme(theme.Variant));
				aPath.bind(*post);
				auto context = core::markdown_suggestion_context{ .FirstLine = static_cast<std::uint32_t>(comment[QStringLiteral("start_line")].toInt()) };
				for (auto line = comment[QStringLiteral("start_line")].toInt(); line <= comment[QStringLiteral("line")].toInt(); ++line) {
					context.Lines.push_back(file[line - 1].toStdString());
				}
				post->set_suggestions(qt::markdown_suggestion_presentation{ .Path = "src/cache.cpp", .Lines = context });
				post->set_body(comment[QStringLiteral("body")].toString().toStdString(), "capture:" + std::to_string(posts.size()));
				post->show();
				posts.push_back(post);
			}
			aPath.deliver_all(posts);
			auto top = 12;
			for (auto* post : posts) {
				const auto height = post->heightForWidth(600);
				post->setGeometry(16, top, 600, height);
				top += height + 16;
			}
			holder.resize(632, top);
			settle();
			expect(holder.grab().save(root + QStringLiteral("/suggestions-") + QString::fromLatin1(theme.Name) + QStringLiteral(".png")), "save suggestions capture");
		}
	}
} // namespace

int main(int aArgc, char** aArgv)
try {
	QApplication                app{ aArgc, aArgv };
	composed                    path;
	bpr::qt::markdown_body_view view;
	path.bind(view);
	view.resize(760, 900);
	view.show();
	settle();
	check_languages(view, path);
	check_arrival(view, path);
	check_inline(path);
	check_diff(view);
	check_suggestions(view);
	check_bounds(view, path);
	capture(view, path);
	path.Host.shutdown();
	std::cout << "markdown-code-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
