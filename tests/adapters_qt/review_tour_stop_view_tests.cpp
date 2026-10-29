// [[BPR-1805]]. A tour stop opens to read: its title, its explanation
// rendered as a comment is, and a pencil that opens the title and Markdown
// source. Done, Escape or Command/Control+Return reads it again; Return on the
// read explanation writes it. A stop with nothing to read yet opens to be
// written, and text the draft has not taken keeps the editor open. Pass a
// path prefix to save light and dark renders.
#include "../support/review_tour_fixture.hpp"
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
#include <QTextEdit>
#include <QToolButton>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
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
	const auto   prefix = argc > 1 ? std::string{ argv[1] } : std::string{ };

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
