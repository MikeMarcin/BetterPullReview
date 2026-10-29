// [[BPR-1805]]. A tour stop opens to read: its title, its explanation
// rendered as a comment is, and a pencil that opens the title and Markdown
// source. Done, Escape or Command/Control+Return reads it again; Return on the
// read explanation writes it. A stop with nothing to read yet opens to be
// written, and text the draft has not taken keeps the editor open. Pass a
// path prefix to save light and dark renders.
#include "../support/review_tour_fixture.hpp"
#include "bpr/adapters/syntax/packaged_syntax_classifier.hpp"
#include "bpr/application/markdown_code_syntax.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/bundled_code_font.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include <QApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextLayout>
#include <QToolButton>
#include <cstdlib>
#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace {
	using namespace bpr;
	using mode = core::tour_stop_view_mode;

	void expect(bool aPass, const std::string& aMessage)
	{
		if (!aPass) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	/// [[BPR-1805]] repair. The composed code colouring, as the window relays
	/// it: the packaged classifier, the readers' own classification host, and
	/// every answer handed to the editor.
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
	struct composed_syntax
	{
		application::markdown_code_texts                           Texts;
		held_texts                                                 Source{ Texts };
		adapters::syntax::packaged_definition_catalog              Catalog;
		adapters::syntax::packaged_syntax_classifier               Classifier{ Catalog, Source };
		manual_executor                                            Effects;
		manual_executor                                            Decisions;
		std::vector<application::markdown_code_syntax_publication> Published;
		std::size_t                                                Requested = 0U;
		application::markdown_code_syntax_host                     Host{ Classifier, Texts, Effects, Decisions,
																		 [this](const std::string_view aSelector) -> std::optional<application::syntax_definition_version> {
														 auto definition = Catalog.definition_for_path(aSelector);
														 if (definition.Name == adapters::syntax::plain_text_definition_name) {
															 return std::nullopt;
														 }
														 return definition;
													 },
													 [this](const application::markdown_code_syntax_publication& aPublication) { Published.push_back(aPublication); } };
		void deliver(qt::review_tour_editor& aEditor)
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
				aEditor.apply_code_syntax(publication);
			}
			QApplication::processEvents();
		}
	};
	template <typename Widget>
	auto child(qt::review_tour_editor& aEditor, const char* aName) -> Widget*
	{
		auto* const value = aEditor.findChild<Widget*>(QString::fromUtf8(aName));
		expect(value != nullptr, aName);
		return value;
	}
	[[nodiscard]] auto dark_palette() -> QPalette
	{
		auto palette = QPalette{ };
		palette.setColor(QPalette::Window, QColor{ 0x1F, 0x23, 0x28 });
		palette.setColor(QPalette::WindowText, QColor{ 0xE6, 0xED, 0xF3 });
		palette.setColor(QPalette::Base, QColor{ 0x16, 0x1B, 0x22 });
		palette.setColor(QPalette::Text, QColor{ 0xE6, 0xED, 0xF3 });
		palette.setColor(QPalette::Button, QColor{ 0x2D, 0x33, 0x3B });
		palette.setColor(QPalette::ButtonText, QColor{ 0xE6, 0xED, 0xF3 });
		palette.setColor(QPalette::Highlight, QColor{ 0x2F, 0x81, 0xF7 });
		palette.setColor(QPalette::HighlightedText, QColor{ 0xFF, 0xFF, 0xFF });
		palette.setColor(QPalette::PlaceholderText, QColor{ 0x8B, 0x94, 0x9E });
		palette.setColor(QPalette::Link, QColor{ 0x58, 0xA6, 0xFF });
		return palette;
	}
	void render(qt::review_tour_editor& aEditor, const std::string& aPrefix, const char* aName)
	{
		if (aPrefix.empty()) {
			return;
		}
		const auto light = aEditor.palette();
		for (const auto& [suffix, palette] : { std::pair{ "light", light }, std::pair{ "dark", dark_palette() } }) {
			aEditor.setPalette(palette);
			QApplication::processEvents();
			expect(aEditor.grab().save(QString::fromStdString(aPrefix + aName + "-" + suffix + ".png")), "render saved");
		}
		aEditor.setPalette(light);
		QApplication::processEvents();
	}
	/// \brief The application's own light and dark themes, as the window
	/// derives them, for the read view's renders.
	void render_themed(qt::review_tour_editor& aEditor, const std::string& aPrefix, const char* aName)
	{
		if (aPrefix.empty()) {
			return;
		}
		const auto before = aEditor.palette();
		for (const auto& [suffix, appearance] : { std::pair{ "light", core::appearance_mode::light }, std::pair{ "dark", core::appearance_mode::dark } }) {
			auto profile = qt::appearance_profile{ };
			profile.select_mode(appearance);
			aEditor.setPalette(profile.derive(QApplication::palette()));
			QApplication::processEvents();
			expect(aEditor.grab().save(QString::fromStdString(aPrefix + aName + "-" + suffix + ".png")), "render saved");
		}
		aEditor.setPalette(before);
		QApplication::processEvents();
	}
	/// [[BPR-1805]] repair. A stop as an author writes one: a heading, emphasis,
	/// a list with inline code, fenced C++, a table and a link.
	constexpr auto     rich_body = R"md(#### What changed

The parser used to rebuild the table on **every** call. Now `parse_table()` fills it *once*, and later reads borrow the cached copy.

- Reads take a `const table&`; nothing is copied
- `reset()` empties the cache, so the next read parses again
- Writers hold the same lock, so no reader sees a half-built table

```cpp
auto table_cache::get() -> const table&
{
    auto lock = std::scoped_lock{ Mutex };
    if (!Table) {
        Table = parse_table(Source); // once
    }
    return *Table;
}
```

| Call | Before | After |
|---|---|---|
| first `get()` | parses | parses |
| later `get()` | parses | cached |

See [the locking notes](https://github.com/octo/widgets/blob/main/docs/locking.md) before changing the mutex.)md";
	[[nodiscard]] auto stop(const std::uint64_t aId, std::string aTitle, std::string aBody) -> core::tour_stop
	{
		auto value        = tests::review_tour_fixture().Stops.front();
		value.Id          = core::tour_stop_id{ aId };
		value.Title       = std::move(aTitle);
		value.Body        = std::move(aBody);
		value.Preparation = core::tour_preparation::editing;
		value.Related     = { };
		return value;
	}
	void press(QWidget* aTarget, const Qt::Key aKey, const Qt::KeyboardModifiers aModifiers = Qt::NoModifier)
	{
		auto override = QKeyEvent{ QEvent::ShortcutOverride, aKey, aModifiers };
		QApplication::sendEvent(aTarget, &override);
		auto down = QKeyEvent{ QEvent::KeyPress, aKey, aModifiers };
		QApplication::sendEvent(aTarget, &down);
		QApplication::processEvents();
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	// As the composition does at startup: code reads in the bundled code font.
	expect(qt::register_bundled_code_font(), "the bundled code font registers");
	const auto prefix = argc > 1 ? std::string{ argv[1] } : std::string{ };

	// Two written stops, as the host keeps the draft: each edit applied in core and presented.
	auto draft       = tests::review_tour_fixture();
	draft.Problem    = "Readers need the change's context before its details.";
	draft.Solution   = "Explain the boundary first, then follow the source.";
	draft.Stops      = { stop(1, "Where the cache is filled", "The parser fills the cache **once**, on first use.\n\n- Later reads take the cached copy\n- `reset()` empties it"),
						 stop(2, "Where it is emptied", "A reset empties the cache so the next read parses again.") };
	draft.LastStopId = 2;
	auto selection   = std::uint64_t{ 1 };
	auto selected    = core::tour_stop_id{ 1 };
	auto refuse      = false;
	auto refusal     = core::tour_refusal::none;

	qt::review_tour_editor editor;
	const auto             show = [&] { editor.present({ .Draft = std::make_shared<const core::review_tour>(draft), .Refusal = refusal, .Selected = selected, .SelectionRevision = selection, .SavedRevision = draft.Revision }); };
	editor.set_dispatch(
		[&](const core::tour_command aCommand) {
			if (refuse) {
				// The local limit: the editor's text is kept, not saved.
				refusal = core::tour_refusal::bounds;
				show();
				return;
			}
			auto outcome = core::apply(draft, aCommand);
			refusal      = outcome.Refusal;
			if (outcome.Refusal == core::tour_refusal::none) {
				draft = std::move(outcome.Accepted);
			}
			show();
		},
		[](qt::tour_editor_action) { });
	editor.resize(420, 820);
	editor.show();
	editor.activateWindow();
	show();
	editor.show_stops();
	QApplication::processEvents();

	auto* const outline   = editor.outline();
	auto* const title     = editor.stop_title();
	auto* const body      = editor.stop_body();
	auto* const reader    = editor.stop_reader();
	auto* const pencil    = child<QToolButton>(editor, "tourStopEdit");
	auto* const done      = child<QPushButton>(editor, "tourStopDone");
	auto* const readTitle = child<QLabel>(editor, "tourStopReadTitle");
	const auto  reading   = [&] { return editor.stop_view().Mode == mode::read && reader->isVisible() && readTitle->isVisible() && pencil->isVisible() && !title->isVisible() && !body->isVisible() && !done->isVisible(); };
	const auto  writing   = [&] { return editor.stop_view().Mode == mode::edit && title->isVisible() && body->isVisible() && done->isVisible() && !reader->isVisible() && !pencil->isVisible(); };

	// A written stop opens to read: the title as text, the explanation rendered, the pencil named.
	expect(editor.stop_view().Stop == core::tour_stop_id{ 1 } && reading(), "a written stop opens to read, not in the editor");
	expect(readTitle->text() == QStringLiteral("Where the cache is filled"), "the read title is the stop's title");
	const auto rendered = reader->rendered_view()->toPlainText();
	expect(rendered.contains(QStringLiteral("once")) && !rendered.contains(QStringLiteral("**")) && !rendered.contains(QStringLiteral("- Later")), "the explanation is rendered Markdown, not its source: " + rendered.toStdString());
	expect(pencil->accessibleName() == QStringLiteral("Edit stop") && pencil->isEnabled() && (pencil->focusPolicy() & Qt::TabFocus) && !pencil->toolTip().isEmpty(), "the pencil is named, enabled and reachable by Tab");
	expect(editor.focusWidget() == pencil, "the stops step puts the keyboard on a read stop's pencil");
	render(editor, prefix, "route-read");

	// The pencil writes: title and source, the keyboard in the source.
	pencil->click();
	QApplication::processEvents();
	expect(writing() && editor.focusWidget() == body, "the pencil opens the title and source with the keyboard in the source");
	body->moveCursor(QTextCursor::End);
	body->insertPlainText(QStringLiteral(" It is never filled twice."));
	QApplication::processEvents();
	expect(draft.Stops.front().Body.ends_with(" It is never filled twice."), "an edit is saved in the draft as it is typed");
	expect(writing(), "a save presents the stop again and keeps it open for writing");
	render(editor, prefix, "route-edit");

	// Going to another stop reads it; going back reads the first again (the owner's report).
	outline->setCurrentRow(1, QItemSelectionModel::ClearAndSelect);
	QApplication::processEvents();
	expect(editor.stop_view().Stop == core::tour_stop_id{ 2 } && reading(), "another written stop opens to read");
	outline->setCurrentRow(0, QItemSelectionModel::ClearAndSelect);
	QApplication::processEvents();
	expect(editor.stop_view().Stop == core::tour_stop_id{ 1 } && reading(), "going back to a stop shows it read, not in the editor");
	expect(reader->rendered_view()->toPlainText().contains(QStringLiteral("never filled twice")), "the reader shows what was written");

	// Return on the read explanation writes; Escape reads, putting the keyboard on the pencil.
	reader->rendered_view()->setFocus(Qt::OtherFocusReason);
	press(reader->rendered_view(), Qt::Key_Return);
	expect(writing() && editor.focusWidget() == body, "Return on the read explanation opens the source, as the pencil does");
	press(body, Qt::Key_Escape);
	expect(reading() && editor.focusWidget() == pencil, "Escape in the source reads the stop, the keyboard on its pencil");
	pencil->click();
	QApplication::processEvents();
	title->setFocus(Qt::OtherFocusReason);
	press(title, Qt::Key_Return, Qt::ControlModifier);
	expect(reading(), "Command/Control+Return reads the stop, as it saves a comment");
	pencil->click();
	QApplication::processEvents();
	done->click();
	QApplication::processEvents();
	expect(reading(), "Done reads the stop");

	// Text the draft has not taken keeps the editor open, with the text.
	pencil->click();
	QApplication::processEvents();
	refuse = true;
	body->moveCursor(QTextCursor::End);
	body->insertPlainText(QStringLiteral(" Unsaved words."));
	QApplication::processEvents();
	expect(refusal == core::tour_refusal::bounds && !draft.Stops.front().Body.ends_with("Unsaved words."), "the edit is refused and not saved");
	done->click();
	QApplication::processEvents();
	expect(writing() && body->toPlainText().endsWith(QStringLiteral("Unsaved words.")), "Done keeps unsaved text in the editor");
	press(body, Qt::Key_Escape);
	expect(writing(), "so does Escape");
	refuse = false;
	body->insertPlainText(QStringLiteral(" Saved."));
	QApplication::processEvents();
	expect(refusal == core::tour_refusal::none && draft.Stops.front().Body.ends_with("Unsaved words. Saved."), "the next edit is taken whole");
	done->click();
	QApplication::processEvents();
	expect(reading() && reader->rendered_view()->toPlainText().contains(QStringLiteral("Unsaved words. Saved.")), "once taken, Done reads it");

	// A new, empty stop opens to be written, the keyboard in its title (BPR-1783).
	draft.Stops.push_back(stop(3, { }, { }));
	draft.LastStopId = 3;
	++draft.Revision;
	selected = core::tour_stop_id{ 3 };
	++selection;
	show();
	editor.show_stops();
	QApplication::processEvents();
	expect(editor.stop_view().Stop == core::tour_stop_id{ 3 } && writing() && editor.focusWidget() == title, "a new, empty stop opens to be written with the keyboard in its title");
	render(editor, prefix, "route-new");
	// Done on an unwritten stop reads it, saying there is nothing yet.
	done->click();
	QApplication::processEvents();
	expect(reading() && readTitle->text() == QStringLiteral("Untitled stop") && reader->rendered_view()->toPlainText().isEmpty(), "an unwritten stop read says it is untitled and has no explanation yet");
	render(editor, prefix, "route-empty-read");

	// The tour page reads the same way.
	outline->setCurrentRow(0, QItemSelectionModel::ClearAndSelect);
	QApplication::processEvents();
	editor.set_mode(qt::tour_editor_mode::page);
	editor.resize(1100, 760);
	editor.show_stops();
	QApplication::processEvents();
	expect(editor.stop_view().Stop == core::tour_stop_id{ 1 } && reading(), "on the tour page a stop reads too");
	render(editor, prefix, "page-read");
	pencil->click();
	QApplication::processEvents();
	expect(writing(), "and the pencil writes it there");
	render(editor, prefix, "page-edit");
	editor.set_mode(qt::tour_editor_mode::route);
	editor.resize(420, 820);
	QApplication::processEvents();

	// [[BPR-1805]] repair (owner review, 2026-10-29). A stop with headings,
	// emphasis, a list, inline and fenced C++ code, a table and a link reads
	// rendered, on the route and on the page, and nothing says it is being
	// edited while it is read.
	auto* const state = child<QLabel>(editor, "tourStopState");
	draft.Stops.push_back(stop(4, "Why the table is parsed once", rich_body));
	draft.LastStopId = 4;
	++draft.Revision;
	selected = core::tour_stop_id{ 4 };
	++selection;
	show();
	editor.show_stops();
	editor.resize(440, 1180);
	QApplication::processEvents();
	expect(editor.stop_view().Stop == core::tour_stop_id{ 4 } && reading(), "a richly written stop opens to read");
	expect(!state->isVisible(), "a read stop does not say Editing beside its pencil");
	const auto rich = reader->rendered_view()->toPlainText();
	for (const auto* const shown : { "What changed", "parse_table()", "auto table_cache::get() -> const table&", "Table = parse_table(Source); // once", "first get()", "the locking notes" }) {
		expect(rich.contains(QString::fromUtf8(shown)), std::string{ "the rendered stop shows " } + shown + ": " + rich.toStdString());
	}
	for (const auto* const hidden : { "####", "**", "```", "|---|", "](https://" }) {
		expect(!rich.contains(QString::fromUtf8(hidden)), std::string{ "the rendered stop hides its Markdown " } + hidden);
	}
	expect(reader->rendered_view()->document()->toHtml().contains(QStringLiteral("<table")), "the table renders as a table");
	// Code is set in the code typography comments use, not the platform's bare fixed font.
	const auto expectedCode = qt::to_presentation_font(qt::default_typography_settings().Code);
	auto       codeFamilies = 0;
	for (auto block = reader->rendered_view()->document()->begin(); block.isValid(); block = block.next()) {
		for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment) {
			const auto text = fragment.fragment().text();
			if (text.contains(QStringLiteral("parse_table")) || text.contains(QStringLiteral("scoped_lock"))) {
				const auto font = fragment.fragment().charFormat().font();
				expect(font.families().contains(expectedCode.family()) || font.family() == expectedCode.family(), "code is set in the default code typography: " + font.family().toStdString());
				++codeFamilies;
			}
		}
	}
	expect(codeFamilies >= 2, "inline and fenced code were both found");
	// The editor relays its readers' code to the window's syntax dispatch, and
	// the answers colour the stop's fenced C++ as a comment's are coloured.
	const auto colours = [&] {
		auto seen = std::set<QRgb>{ };
		for (auto block = reader->rendered_view()->document()->begin(); block.isValid(); block = block.next()) {
			if (!block.text().contains(QStringLiteral("scoped_lock")) && !block.text().contains(QStringLiteral("return *Table"))) {
				continue;
			}
			// The colours are the block layout's formats, as the panes' are.
			if (block.layout() != nullptr) {
				for (const auto& range : block.layout()->formats()) {
					seen.insert(range.format.foreground().color().rgb());
				}
			}
		}
		return seen.size();
	};
	const auto plainColours = colours();
	auto       syntax       = composed_syntax{ };
	editor.set_code_syntax_dispatch([&syntax](std::vector<application::markdown_code_syntax_request> aRequests) {
		syntax.Requested += aRequests.size();
		syntax.Host.request(aRequests);
	});
	syntax.deliver(editor);
	expect(syntax.Requested >= 1U, "the stop's fenced C++ asks for its colours, as a comment's does");
	expect(colours() > plainColours && colours() >= 2U, "the answers colour the stop's code");
	render_themed(editor, prefix, "route-read-rich");
	expect(reading(), "rendering does not open the editor");
	pencil->click();
	QApplication::processEvents();
	expect(writing() && state->isVisible() && state->text() == QStringLiteral("Editing"), "writing, the stop says Editing");
	render_themed(editor, prefix, "route-edit-rich");
	done->click();
	QApplication::processEvents();
	expect(reading() && !state->isVisible(), "read again, it does not");
	editor.set_mode(qt::tour_editor_mode::page);
	editor.resize(1180, 1000);
	editor.show_stops();
	QApplication::processEvents();
	expect(editor.stop_view().Stop == core::tour_stop_id{ 4 } && reading() && !state->isVisible(), "on the tour page the rich stop reads too");
	syntax.deliver(editor);
	render_themed(editor, prefix, "page-read-rich");
	editor.set_code_syntax_dispatch({ });
	editor.set_mode(qt::tour_editor_mode::route);
	editor.resize(420, 820);
	QApplication::processEvents();

	// A draft that cannot be written is read, its pencil unavailable.
	draft.ReadOnly = true;
	++draft.Revision;
	show();
	QApplication::processEvents();
	expect(reading() && !pencil->isEnabled(), "a read-only draft reads its stops and offers no pencil");
	pencil->click();
	editor.begin_stop_edit();
	QApplication::processEvents();
	expect(reading(), "and cannot be written");

	std::cout << "review-tour-stop-view:pass\n";
	return EXIT_SUCCESS;
}
