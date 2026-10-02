#include "../support/test_expectation.hpp"
#include "../support/window_system_mouse.hpp"
#include "bpr/core/appearance_contrast.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/suggestion_composer.hpp"
#include "bpr/qt/suggestion_editor.hpp"

#include <QAccessible>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDir>
#include <QFontInfo>
#include <QImage>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

/// \file
/// \brief Native course for the focused suggestion editor (BPR-1156): real
/// key, IME, mouse and clipboard events through the Qt widget, with the exact
/// bytes and carets checked after each gesture.

namespace {
	using namespace bpr;
	using bpr::tests::expect;

	void settle()
	{
		QApplication::processEvents();
		QApplication::processEvents();
	}

	void key(QWidget& aWidget, const int aKey, const Qt::KeyboardModifiers aModifiers = Qt::NoModifier, const QString& aText = { })
	{
		QKeyEvent press{ QEvent::KeyPress, aKey, aModifiers, aText };
		QApplication::sendEvent(&aWidget, &press);
		QKeyEvent release{ QEvent::KeyRelease, aKey, aModifiers, aText };
		QApplication::sendEvent(&aWidget, &release);
	}

	void type(QWidget& aWidget, const QString& aText)
	{
		for (const auto character : aText) {
			key(aWidget, 0, Qt::NoModifier, QString{ character });
		}
	}

	void click(qt::suggestion_editor& aEditor, const core::text_point& aAt, const Qt::KeyboardModifiers aModifiers = Qt::NoModifier)
	{
		// The editor's own geometry names where a point is; the event goes through Qt.
		const auto  cell     = aEditor.cell_rectangle(aAt);
		const auto  position = QPointF{ cell.left() + 1.0, cell.center().y() };
		QMouseEvent press{ QEvent::MouseButtonPress, position, aEditor.viewport()->mapToGlobal(position), Qt::LeftButton, Qt::LeftButton, aModifiers };
		QApplication::sendEvent(aEditor.viewport(), &press);
		QMouseEvent release{ QEvent::MouseButtonRelease, position, aEditor.viewport()->mapToGlobal(position), Qt::LeftButton, Qt::NoButton, aModifiers };
		QApplication::sendEvent(aEditor.viewport(), &release);
	}

	/// A second click where the first was, as Qt reports it: a double click.
	void double_click(qt::suggestion_editor& aEditor, const core::text_point& aAt)
	{
		click(aEditor, aAt);
		const auto  cell     = aEditor.cell_rectangle(aAt);
		const auto  position = QPointF{ cell.left() + 1.0, cell.center().y() };
		QMouseEvent twice{ QEvent::MouseButtonDblClick, position, aEditor.viewport()->mapToGlobal(position), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(aEditor.viewport(), &twice);
		QMouseEvent release{ QEvent::MouseButtonRelease, position, aEditor.viewport()->mapToGlobal(position), Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(aEditor.viewport(), &release);
	}

	/// A left-button event through the window system, at a viewport point.
	void system_mouse(qt::suggestion_editor& aEditor, const QPointF& aAt, const QEvent::Type aType, const Qt::MouseButtons aHeld, const int aAfter = 30)
	{
		tests::window_system_mouse(*aEditor.viewport(), aAt, aType, aHeld, aAfter);
	}

	/// A point `aAcross` of the way across the cell of `aAt`.
	auto point_in(const qt::suggestion_editor& aEditor, const core::text_point& aAt, const qreal aAcross = 0.5) -> QPointF
	{
		const auto cell = aEditor.cell_rectangle(aAt);
		return { cell.left() + (cell.width() * aAcross), cell.center().y() };
	}

	/// Clicks through the window system in one cell, `aAcross` of the way over.
	void system_clicks(qt::suggestion_editor& aEditor, const core::text_point& aAt, const int aCount, const qreal aDrift = 0.0, const qreal aAcross = 0.5)
	{
		tests::window_system_clicks(*aEditor.viewport(), point_in(aEditor, aAt, aAcross), aCount, aDrift);
	}

	auto heads(const qt::suggestion_editor& aEditor) -> std::vector<core::text_point>
	{
		auto found = std::vector<core::text_point>{ };
		for (const auto& caret : aEditor.model().state().Carets) {
			found.push_back(caret.Head);
		}
		return found;
	}

	void capture(QWidget& aWidget, const QString& aName)
	{
		const auto directory = qEnvironmentVariable("BPR_SUGGESTION_RENDER_DIR");
		const auto image     = aWidget.grab().toImage();
		expect(!image.isNull() && image.width() > 0 && image.height() > 0, "rendered state");
		if (!directory.isEmpty()) {
			QDir{ }.mkpath(directory);
			expect(image.save(QDir{ directory }.filePath(aName + QStringLiteral(".png"))), "render saved");
		}
	}

	/// The owner's workflow: Alt+Shift+Down twice, then Shift+End, then typing.
	void box_then_shift_end(qt::suggestion_editor& aEditor)
	{
		aEditor.set_text("auto a = 1;\r\nauto bb = 22;\r\nauto c = 3;\r\n");
		aEditor.setFocus();
		settle();
		for (int step = 0; step < 5; ++step) {
			key(aEditor, Qt::Key_Right);
		}
		key(aEditor, Qt::Key_Down, Qt::AltModifier | Qt::ShiftModifier);
		key(aEditor, Qt::Key_Down, Qt::AltModifier | Qt::ShiftModifier);
		expect(heads(aEditor) == std::vector<core::text_point>{ { 0U, 5U }, { 1U, 5U }, { 2U, 5U } }, "Alt+Shift+Down makes one caret per line");
		key(aEditor, Qt::Key_End, Qt::ShiftModifier);
		for (const auto& caret : aEditor.model().state().Carets) {
			expect(caret.Anchor.Line == caret.Head.Line && caret.Head.Column == aEditor.model().line_width(caret.Head.Line), "Shift+End stays on each caret's line");
		}
		capture(aEditor, QStringLiteral("suggestion-editor-box-shift-end"));
		type(aEditor, QStringLiteral("x"));
		expect(aEditor.text() == "auto x\r\nauto x\r\nauto x\r\n", "typed at every caret; CRLF kept");
		key(aEditor, Qt::Key_Z, Qt::ControlModifier);
		expect(aEditor.text() == "auto a = 1;\r\nauto bb = 22;\r\nauto c = 3;\r\n", "undo restores the exact bytes");
		key(aEditor, Qt::Key_Escape);
		expect(aEditor.model().state().Carets.size() == 1U, "Escape leaves one caret");
	}

	/// The owner asked for indentation on Enter like a modern editor
	/// (BPR-1156, 2026-10-02), through real key events.
	void indents_like_an_editor(qt::suggestion_editor& aEditor)
	{
		aEditor.set_text("int f() {\n}\n");
		aEditor.set_indentation(core::indentation_for("src/f.cpp", aEditor.text()));
		aEditor.setFocus();
		settle();
		key(aEditor, Qt::Key_End);
		key(aEditor, Qt::Key_Return);
		expect(aEditor.text() == "int f() {\n\t\n}\n" && heads(aEditor).front() == core::text_point{ 1U, 4U }, "Enter after a brace indents one level");
		type(aEditor, QStringLiteral("if (x) {"));
		key(aEditor, Qt::Key_Return);
		type(aEditor, QStringLiteral("y();"));
		key(aEditor, Qt::Key_Return);
		expect(aEditor.text() == "int f() {\n\tif (x) {\n\t\ty();\n\t\t\n}\n", "the level kept, one more after another brace");
		type(aEditor, QStringLiteral("}"));
		expect(aEditor.text() == "int f() {\n\tif (x) {\n\t\ty();\n\t}\n}\n", "a typed closer goes back a level");
		// Between a pair: the closer moves to its own line.
		aEditor.set_text("    call()\n");
		aEditor.set_indentation(core::indentation_for("a.py", aEditor.text()));
		key(aEditor, Qt::Key_End);
		key(aEditor, Qt::Key_Left);
		key(aEditor, Qt::Key_Return);
		expect(aEditor.text() == "    call(\n        \n    )\n" && heads(aEditor).front() == core::text_point{ 1U, 8U }, "the closer on its own line, the file's four spaces");
		capture(aEditor, QStringLiteral("suggestion-editor-auto-indent"));
	}

	/// Read-only, the same view is the cycle page's file pane: every
	/// selection gesture works and nothing changes the text.
	void reads_without_editing(qt::suggestion_editor& aEditor)
	{
		aEditor.set_text("auto value = other;\n");
		aEditor.set_read_only(true);
		aEditor.setFocus();
		settle();
		type(aEditor, QStringLiteral("x"));
		key(aEditor, Qt::Key_Return);
		key(aEditor, Qt::Key_Backspace);
		key(aEditor, Qt::Key_Delete);
		QApplication::clipboard()->setText(QStringLiteral("pasted"));
		key(aEditor, Qt::Key_V, Qt::ControlModifier);
		key(aEditor, Qt::Key_X, Qt::ControlModifier);
		QInputMethodEvent committed{ QString{ }, { } };
		committed.setCommitString(QStringLiteral("日本"));
		QApplication::sendEvent(&aEditor, &committed);
		expect(aEditor.text() == "auto value = other;\n" && !aEditor.inputMethodQuery(Qt::ImEnabled).toBool(), "nothing typed, pasted, cut or composed changes the text");
		key(aEditor, Qt::Key_End, Qt::ShiftModifier);
		key(aEditor, Qt::Key_C, Qt::ControlModifier);
		expect(QApplication::clipboard()->text() == QStringLiteral("auto value = other;"), "Shift+End selects and Ctrl+C copies");
		double_click(aEditor, { 0U, 7U });
		expect(aEditor.has_selection() && aEditor.selected_bytes() == std::pair<std::uint64_t, std::uint64_t>{ 5U, 10U }, "a double click selects the word");
		click(aEditor, { 0U, 7U });
		expect(aEditor.selected_bytes() == std::pair<std::uint64_t, std::uint64_t>{ 0U, 20U }, "a third click selects the line");
		auto* const menu  = aEditor.standard_menu(nullptr);
		auto        shown = QStringList{ };
		for (const auto* action : menu->actions()) {
			if (!action->isSeparator()) {
				shown.append(action->text());
			}
		}
		expect(shown == QStringList{ QStringLiteral("&Copy"), QStringLiteral("Select All") }, "the read-only menu offers Copy and Select All");
		delete menu;
		auto* const accessible = QAccessible::queryAccessibleInterface(&aEditor);
		auto* const reading    = accessible != nullptr ? accessible->textInterface() : nullptr;
		expect(reading != nullptr && reading->text(0, reading->characterCount()) == QStringLiteral("auto value = other;\n") && accessible->state().readOnly && accessible->state().multiLine, "a screen reader reads the text");
		int start = 0;
		int end   = 0;
		reading->selection(0, &start, &end);
		expect(reading->selectionCount() == 1 && start == 0 && end == 20, "and the selection");
		aEditor.set_read_only(false);
		auto* const editing = aEditor.standard_menu(nullptr);
		expect(editing->actions().size() > 5 && !accessible->state().readOnly, "editable: the full editing menu");
		delete editing;
	}

	void box_copy_and_paste(qt::suggestion_editor& aEditor, qt::suggestion_editor& aTarget)
	{
		aEditor.set_text("abcdef\nabcdef\nab\n");
		settle();
		key(aEditor, Qt::Key_Right);
		for (int step = 0; step < 3; ++step) {
			key(aEditor, Qt::Key_Right, Qt::AltModifier | Qt::ShiftModifier);
		}
		key(aEditor, Qt::Key_Down, Qt::AltModifier | Qt::ShiftModifier);
		key(aEditor, Qt::Key_Down, Qt::AltModifier | Qt::ShiftModifier);
		key(aEditor, Qt::Key_C, Qt::ControlModifier);
		const auto* data = QApplication::clipboard()->mimeData();
		expect(data != nullptr && data->text() == QStringLiteral("bcd\nbcd\nb  ") && data->hasFormat(QString::fromLatin1(qt::suggestion_editor::box_mime_type)), "box copied with its marker");
		aTarget.set_text("12345\n12345");
		aTarget.setFocus();
		settle();
		key(aTarget, Qt::Key_Down);
		key(aTarget, Qt::Key_Right);
		key(aTarget, Qt::Key_Right);
		key(aTarget, Qt::Key_V, Qt::ControlModifier);
		expect(aTarget.text() == "12345\n12bcd345\n  bcd\n  b  ", "box pasted as a box");
		// Cut removes exactly the selections.
		key(aTarget, Qt::Key_A, Qt::ControlModifier);
		key(aTarget, Qt::Key_X, Qt::ControlModifier);
		expect(aTarget.text().empty() && QApplication::clipboard()->text() == QStringLiteral("12345\n12bcd345\n  bcd\n  b  "), "cut");
		aEditor.setFocus();
	}

	/// \brief [[BPR-1156]]. The owner's double click (composer-layout v005,
	/// event 776), through the window system: still, and with the pointer
	/// drifting a pixel or three while the button is down, as a hand does.
	/// \details Before the fix a still double click selected the word, but a
	/// one-pixel drift turned it into the word's start up to the pointer
	/// (bytes 5 to 8 of 5 to 10), and a triple click into part of the line.
	void double_click_selects_the_word(qt::suggestion_editor& aEditor)
	{
		using bytes = std::pair<std::uint64_t, std::uint64_t>;
		for (const auto readOnly : { true, false }) {
			aEditor.set_text("auto value = other_name;\n\tnext line\n");
			aEditor.set_read_only(readOnly);
			settle();
			for (const auto drift : { 0.0, 1.0, 3.0 }) {
				system_clicks(aEditor, { 0U, 7U }, 2, drift);
				expect(aEditor.selected_bytes() == bytes{ 5U, 10U }, readOnly ? "reading: a double click selects exactly the word" : "editing: a double click selects exactly the word");
				system_clicks(aEditor, { 0U, 16U }, 3, drift);
				expect(aEditor.selected_bytes() == bytes{ 0U, 25U }, readOnly ? "reading: a triple click selects the line and its break" : "editing: a triple click selects the line and its break");
			}
			// On the right half of the word's last letter, still that word.
			system_clicks(aEditor, { 0U, 9U }, 2, 0.0, 0.85);
			expect(aEditor.selected_bytes() == bytes{ 5U, 10U }, "a double click on the right half of the last letter selects the word");
			system_clicks(aEditor, { 0U, 5U }, 2, 0.0, 0.1);
			expect(aEditor.selected_bytes() == bytes{ 5U, 10U }, "and on the left half of the first");
			// Dragging on after a double click grows by whole words.
			const auto from = point_in(aEditor, { 0U, 7U });
			system_mouse(aEditor, from, QEvent::MouseButtonPress, Qt::LeftButton, 2000);
			system_mouse(aEditor, from, QEvent::MouseButtonRelease, Qt::NoButton, 40);
			system_mouse(aEditor, from, QEvent::MouseButtonPress, Qt::LeftButton, 60);
			system_mouse(aEditor, point_in(aEditor, { 0U, 17U }), QEvent::MouseMove, Qt::LeftButton, 20);
			expect(aEditor.selected_bytes() == bytes{ 5U, 23U }, "a drag after a double click selects whole words");
			system_mouse(aEditor, point_in(aEditor, { 0U, 1U }), QEvent::MouseMove, Qt::LeftButton, 20);
			expect(aEditor.selected_bytes() == bytes{ 0U, 10U }, "and back past where it began, still keeping the first word");
			system_mouse(aEditor, point_in(aEditor, { 0U, 1U }), QEvent::MouseButtonRelease, Qt::NoButton, 40);
			// A single click still places a caret, at the nearer edge of the
			// character clicked, and a later move without the button selects nothing.
			system_clicks(aEditor, { 1U, 6U }, 1);
			expect(!aEditor.has_selection() && heads(aEditor).front() == core::text_point{ 1U, 7U }, "one click places a caret");
			system_mouse(aEditor, point_in(aEditor, { 0U, 3U }), QEvent::MouseMove, Qt::NoButton, 20);
			expect(!aEditor.has_selection(), "a move without the button selects nothing");
			expect(aEditor.text() == "auto value = other_name;\n\tnext line\n", "no gesture changed the text");
		}
		aEditor.set_read_only(false);
	}

	/// \brief Virtual space belongs to the box (owner note on
	/// composer-layout v005): a click, a drag, Right and Up and Down stay on
	/// the text; Alt+Shift+Arrow alone reaches past a line's end.
	void virtual_space_only_in_a_box(qt::suggestion_editor& aEditor)
	{
		aEditor.set_text("\tlong line here\n\tab\n");
		aEditor.setFocus();
		settle();
		key(aEditor, Qt::Key_End);
		key(aEditor, Qt::Key_Down);
		expect(heads(aEditor).front() == core::text_point{ 1U, 6U }, "Down stops at a shorter line's end");
		key(aEditor, Qt::Key_Down);
		key(aEditor, Qt::Key_Up);
		key(aEditor, Qt::Key_Up);
		expect(heads(aEditor).front() == core::text_point{ 0U, 18U }, "and Up comes back to the column it aimed for");
		key(aEditor, Qt::Key_Right);
		expect(heads(aEditor).front() == core::text_point{ 1U, 0U }, "Right at a line's end goes on to the next line");
		system_clicks(aEditor, { 1U, 15U }, 1);
		expect(heads(aEditor).front() == core::text_point{ 1U, 6U } && !aEditor.has_selection(), "a click past the end puts the caret at the end");
		const auto start = point_in(aEditor, { 0U, 5U });
		system_mouse(aEditor, start, QEvent::MouseButtonPress, Qt::LeftButton, 2000);
		system_mouse(aEditor, point_in(aEditor, { 0U, 30U }), QEvent::MouseMove, Qt::LeftButton, 20);
		system_mouse(aEditor, point_in(aEditor, { 0U, 30U }), QEvent::MouseButtonRelease, Qt::NoButton, 20);
		const auto& dragged = aEditor.model().state().Carets.front();
		expect(dragged.Head == core::text_point{ 0U, 18U }, "a drag past the end stops at the end");
		key(aEditor, Qt::Key_Down);
		type(aEditor, QStringLiteral(";"));
		expect(aEditor.text() == "\tlong line here\n\tab;\n", "typing at a short line's end adds no blanks");
		key(aEditor, Qt::Key_Up);
		key(aEditor, Qt::Key_End);
		key(aEditor, Qt::Key_Down, Qt::AltModifier | Qt::ShiftModifier);
		expect(heads(aEditor) == std::vector<core::text_point>{ { 0U, 18U }, { 1U, 18U } }, "a box reaches past a short line's end");
		type(aEditor, QStringLiteral(";"));
		expect(aEditor.text() == "\tlong line here;\n\tab;           ;\n", "and typing there pads the line with spaces");
		key(aEditor, Qt::Key_Escape);
		key(aEditor, Qt::Key_Backspace);
		key(aEditor, Qt::Key_Home);
		key(aEditor, Qt::Key_Home);
		key(aEditor, Qt::Key_Tab);
		expect(aEditor.text() == "\tlong line here;\n\t\tab;           \n", "Tab inserts a tab");
		key(aEditor, Qt::Key_Return);
		expect(aEditor.text() == "\tlong line here;\n\t\n\t\tab;           \n", "a line break keeps the indentation before the caret");
		key(aEditor, Qt::Key_Delete);
		expect(aEditor.text() == "\tlong line here;\n\t\n\tab;           \n", "forward delete");
	}

	void ime_and_mouse(qt::suggestion_editor& aEditor)
	{
		aEditor.set_text("a\nb\n");
		aEditor.setFocus();
		settle();
		click(aEditor, { 0U, 1U });
		click(aEditor, { 1U, 1U }, Qt::AltModifier);
		expect(aEditor.model().state().Carets.size() == 2U, "Alt+click adds a caret");
		QInputMethodEvent composing{ QStringLiteral("にほ"), { } };
		QApplication::sendEvent(&aEditor, &composing);
		expect(aEditor.preedit() == QStringLiteral("にほ") && aEditor.text() == "a\nb\n", "composition is shown, not inserted");
		capture(aEditor, QStringLiteral("suggestion-editor-ime-preedit"));
		QInputMethodEvent committed{ QString{ }, { } };
		committed.setCommitString(QStringLiteral("日本"));
		QApplication::sendEvent(&aEditor, &committed);
		expect(aEditor.preedit().isEmpty() && aEditor.text() == "a日本\nb日本\n", "committed text at every caret");
		expect(aEditor.inputMethodQuery(Qt::ImCursorRectangle).toRect().isValid() && aEditor.inputMethodQuery(Qt::ImEnabled).toBool(), "IME queries answered");
		expect(!aEditor.accessibleName().isEmpty() && !aEditor.accessibleDescription().isEmpty(), "accessible name and description");
	}

	/// Within a small distance of `aColour`. Not `near`: the Windows SDK
	/// defines `near` as an empty macro.
	auto colour_near(const QColor& aPixel, const QColor& aColour) -> bool
	{
		return std::abs(aPixel.red() - aColour.red()) <= 24 && std::abs(aPixel.green() - aColour.green()) <= 24 && std::abs(aPixel.blue() - aColour.blue()) <= 24;
	}

	auto pixels_near(QWidget& aWidget, const QColor& aColour) -> int
	{
		const auto image = aWidget.grab().toImage();
		auto       count = 0;
		for (int y = 0; y < image.height(); ++y) {
			for (int x = 0; x < image.width(); ++x) {
				if (colour_near(image.pixelColor(x, y), aColour)) {
					++count;
				}
			}
		}
		return count;
	}

	/// \brief Whether the first `aWord` in `aEdit` is painted in `aColour`:
	/// inside the word's own rectangle at least one pixel is near `aColour`
	/// and none is near the plain text colour.
	///
	/// \details [[BPR-1156]]. A whole-widget pixel count does not carry across
	/// scales or faces: at 1x a 9-point keyword in DejaVu Sans Mono has 8
	/// pixels near its role colour, and 154 at 2x. Anti-aliasing blends the
	/// role colour toward the background, never toward the text colour, so
	/// a painted word has no text-coloured pixel and an unpainted one has many.
	auto word_painted(QPlainTextEdit& aEdit, const QString& aWord, const QColor& aColour) -> bool
	{
		const auto found = aEdit.document()->find(aWord);
		if (found.isNull()) {
			return false;
		}
		auto start = found;
		start.setPosition(found.selectionStart());
		auto end = found;
		end.setPosition(found.selectionEnd());
		const auto left   = aEdit.cursorRect(start);
		const auto right  = aEdit.cursorRect(end);
		const auto pixmap = aEdit.viewport()->grab();
		const auto image  = pixmap.toImage();
		const auto ratio  = pixmap.devicePixelRatio();
		const auto text   = aEdit.palette().color(QPalette::Text);
		auto       role   = 0;
		auto       plain  = 0;
		for (auto y = static_cast<int>(left.top() * ratio); y < static_cast<int>((left.bottom() + 1) * ratio) && y < image.height(); ++y) {
			for (auto x = static_cast<int>(left.left() * ratio); x < static_cast<int>(right.left() * ratio) && x < image.width(); ++x) {
				const auto pixel = image.pixelColor(x, y);
				role += colour_near(pixel, aColour) ? 1 : 0;
				plain += colour_near(pixel, text) ? 1 : 0;
			}
		}
		return role > 0 && plain == 0;
	}

	/// \brief The same rule for the proposal editor, between the middles of
	/// columns `aFirst` and `aLast` of the caret's line.
	/// \details `point_at` moves to the next column at a glyph's middle, so
	/// the region holds only that word's ink, never a neighbour's.
	auto span_painted(qt::suggestion_editor& aEditor, const std::uint64_t aFirst, const std::uint64_t aLast, const QColor& aColour) -> bool
	{
		const auto line  = aEditor.caret_rectangle();
		auto       begin = -1;
		auto       end   = -1;
		for (auto x = 0; x < aEditor.viewport()->width() && end < 0; ++x) {
			const auto column = aEditor.point_at(QPoint{ x, line.center().y() }).Column;
			if (begin < 0 && column > aFirst) {
				begin = x;
			}
			if (column > aLast) {
				end = x;
			}
		}
		if (begin < 0 || end <= begin) {
			return false;
		}
		const auto pixmap = aEditor.viewport()->grab();
		const auto image  = pixmap.toImage();
		const auto ratio  = pixmap.devicePixelRatio();
		const auto text   = aEditor.palette().color(QPalette::Text);
		auto       role   = 0;
		auto       plain  = 0;
		for (auto y = static_cast<int>(line.top() * ratio); y < static_cast<int>((line.bottom() + 1) * ratio) && y < image.height(); ++y) {
			for (auto x = static_cast<int>(begin * ratio); x < static_cast<int>(end * ratio) && x < image.width(); ++x) {
				const auto pixel = image.pixelColor(x, y);
				role += colour_near(pixel, aColour) ? 1 : 0;
				plain += colour_near(pixel, text) ? 1 : 0;
			}
		}
		return role > 0 && plain == 0;
	}

	auto chromatic(const QColor& aColour) -> bool
	{
		return std::max({ aColour.red(), aColour.green(), aColour.blue() }) - std::min({ aColour.red(), aColour.green(), aColour.blue() }) > 60;
	}

	/// `aTop` laid over `aBase`, as a band with alpha is painted.
	auto over(const QColor& aTop, const QColor& aBase) -> QColor
	{
		const auto alpha = aTop.alphaF();
		return QColor::fromRgbF(static_cast<float>(aTop.redF() * alpha + aBase.redF() * (1.0 - alpha)), static_cast<float>(aTop.greenF() * alpha + aBase.greenF() * (1.0 - alpha)),
								static_cast<float>(aTop.blueF() * alpha + aBase.blueF() * (1.0 - alpha)));
	}

	/// A flat fill has no anti-aliasing: a few units of rounding at most.
	auto fill_is(const QColor& aPixel, const QColor& aColour) -> bool
	{
		return std::abs(aPixel.red() - aColour.red()) <= 3 && std::abs(aPixel.green() - aColour.green()) <= 3 && std::abs(aPixel.blue() - aColour.blue()) <= 3;
	}

	/// \brief [[BPR-1156]]. A text selection reads as one in any text box
	/// (owner note on composer-layout v005: "darker than usual"): the solid
	/// `Highlight` fill, ending at the line's text plus one cell for a
	/// selected line break, under glyphs in the selection's ink.
	void selection_reads_like_a_text_box(qt::suggestion_editor& aEditor)
	{
		aEditor.set_text("int  value = 1;\nx\n");
		aEditor.set_read_only(true);
		settle();
		const auto highlight = aEditor.palette().color(QPalette::Highlight);
		const auto base      = aEditor.palette().color(QPalette::Base);
		auto       wash      = highlight;
		wash.setAlpha(core::selection_wash_alpha);
		// The blank fourth column of "int  value", selected by a drag past the end.
		const auto press = point_in(aEditor, { 0U, 0U }) - QPointF{ aEditor.cell_rectangle({ 0U, 0U }).width() * 0.4, 0.0 };
		system_mouse(aEditor, press, QEvent::MouseButtonPress, Qt::LeftButton, 2000);
		system_mouse(aEditor, point_in(aEditor, { 0U, 40U }), QEvent::MouseMove, Qt::LeftButton, 20);
		system_mouse(aEditor, point_in(aEditor, { 0U, 40U }), QEvent::MouseButtonRelease, Qt::NoButton, 20);
		auto       image = aEditor.viewport()->grab().toImage();
		const auto at    = [&](const core::text_point& aPoint) {
			const auto middle = aEditor.cell_rectangle(aPoint).center() * image.devicePixelRatio();
			return image.pixelColor(static_cast<int>(middle.x()), static_cast<int>(middle.y()));
		};
		expect(aEditor.selected_bytes() == std::pair<std::uint64_t, std::uint64_t>{ 0U, 15U }, "the drag selected the line's text and stopped at its end");
		expect(fill_is(at({ 0U, 4U }), highlight), "the selection is the solid Highlight a text box selects with");
		expect(!fill_is(over(wash, base), highlight), "which is not the panes' line wash");
		expect(fill_is(at({ 0U, 15U }), base) && fill_is(at({ 0U, 20U }), base), "a selection to the line's end paints nothing past the text");
		key(aEditor, Qt::Key_Down, Qt::ShiftModifier);
		image = aEditor.viewport()->grab().toImage();
		expect(fill_is(at({ 0U, 15U }), highlight) && fill_is(at({ 0U, 16U }), base), "a selected line break is one cell");
		capture(aEditor, QStringLiteral("suggestion-editor-selection"));
		// The ink: a role kept where it reads on the fill, moved toward legibility where it does not.
		const auto fill  = core::appearance_color{ 0x28U, 0x51U, 0x8CU, 255U };
		const auto kept  = core::appearance_color{ 0xE6U, 0xEDU, 0xF3U, 255U };
		const auto faint = core::appearance_color{ 0x6DU, 0xA9U, 0xDBU, 255U };
		const auto moved = core::readable_selection_ink(faint, fill, core::appearance_variant::dark);
		expect(core::readable_selection_ink(kept, fill, core::appearance_variant::dark) == kept, "a role that reads on the selection keeps its colour");
		// Every built-in profile: each syntax role reads on its own selection at its variant's floor.
		for (const auto variant : { core::appearance_variant::light, core::appearance_variant::dark, core::appearance_variant::high_contrast_light, core::appearance_variant::high_contrast_dark }) {
			const auto& palette   = qt::builtin_appearance_palette(variant);
			const auto  selection = palette[core::index_of(core::appearance_role::code_selection_background)];
			for (auto index = core::index_of(core::appearance_role::syntax_normal); index < core::appearance_role_count; ++index) {
				const auto ink = core::readable_selection_ink(palette[index], selection, variant);
				expect(core::contrast_ratio_hundredths(ink, selection) >= core::minimum_contrast_hundredths(core::contrast_kind::text, variant), "every built-in role reads on its profile's selection");
			}
		}
		expect(core::contrast_ratio_hundredths(faint, fill) < core::text_contrast_minimum_hundredths && core::contrast_ratio_hundredths(moved, fill) >= core::text_contrast_minimum_hundredths && moved.Blue > moved.Red,
			   "one that does not is lightened to the floor and stays blue");
		aEditor.set_read_only(false);
	}

	/// \brief The fill painted behind the preview row `aRow`, read past the
	/// end of its text, where no glyph is.
	auto row_fill(QPlainTextEdit& aPreview, const QString& aRow) -> QColor
	{
		for (auto block = aPreview.document()->firstBlock(); block.isValid(); block = block.next()) {
			if (block.text() != aRow) {
				continue;
			}
			const auto row    = aPreview.cursorRect(QTextCursor{ block });
			const auto pixmap = aPreview.viewport()->grab();
			const auto ratio  = pixmap.devicePixelRatio();
			return pixmap.toImage().pixelColor(static_cast<int>((aPreview.viewport()->width() - 4) * ratio), static_cast<int>(row.center().y() * ratio));
		}
		return { };
	}

	/// BPR-1156: Suggest change paints classified code in the review panes'
	/// role colours: the whole file in the editor and the before/after
	/// preview, nothing from a classification of other text, and the
	/// preview's removed and added rows in the diff panes' own fills.
	void syntax(qt::suggestion_composer& aComposer)
	{
		using role             = core::appearance_role;
		const auto file        = std::string{ "/* note\r\n   still */\r\nint f()\r\n{\r\n\treturn g(1);\r\n}\r\n" };
		auto       asked       = std::vector<qt::suggestion_syntax_request>{ };
		aComposer.SyntaxWanted = [&asked](const qt::suggestion_syntax_request& aRequest) { asked.push_back(aRequest); };
		const auto profile     = qt::appearance_profile{ };
		const auto flow        = profile.color(role::syntax_control_flow);
		const auto number      = profile.color(role::syntax_number);
		expect(chromatic(flow) && chromatic(number), "the role colours are told apart from text by hue");
		aComposer.adopt_appearance(profile);
		// The caret before `return`, nothing selected.
		expect(aComposer.begin({ .Location = "src/f.cpp \xC2\xB7 R1", .Path = "src/f.cpp", .File = file, .Start = 35U, .End = 35U }), "begun at the caret");
		settle();
		expect(asked.size() == 2U && asked[0].Side == qt::suggestion_syntax_side::original && asked[1].Side == qt::suggestion_syntax_side::proposed && asked[0].Path == "src/f.cpp"
				   && asked[0].Document == file && asked[1].Document == file,
			   "both texts asked for, each the whole file");
		auto* const editor  = aComposer.editor();
		auto* const preview = aComposer.preview();
		expect(editor->text() == file && editor->caret_rectangle().isValid(), "the editor holds the whole file");
		expect(!span_painted(*editor, 4U, 9U, flow), "nothing coloured before a classification arrives");

		// What the classifier reports for the file: lines split at LF, byte columns.
		const auto spans = std::make_shared<const application::token_span_list>(application::token_span_list{
			{ .Line = 0U, .Column = 0U, .Length = 7U, .Role = role::syntax_comment },
			{ .Line = 1U, .Column = 0U, .Length = 11U, .Role = role::syntax_comment },
			{ .Line = 2U, .Column = 4U, .Length = 1U, .Role = role::syntax_function },
			{ .Line = 4U, .Column = 1U, .Length = 6U, .Role = role::syntax_control_flow },
			{ .Line = 4U, .Column = 8U, .Length = 1U, .Role = role::syntax_function },
			{ .Line = 4U, .Column = 10U, .Length = 1U, .Role = role::syntax_number },
		});
		expect(!aComposer.adopt_syntax(qt::suggestion_syntax_side::proposed, file + "x", spans), "a classification of other text is refused");
		expect(aComposer.adopt_syntax(qt::suggestion_syntax_side::original, file, spans), "the unedited file's classification is adopted");
		settle();
		// `return` is columns 4 to 9 after the tab.
		expect(span_painted(*editor, 4U, 9U, flow) && pixels_near(*editor, number) > 0, "the editor paints the keyword and the number in their role colours");

		// An edit at the end of the line: one change, asked for again.
		editor->setFocus();
		key(*editor, Qt::Key_End);
		type(*editor, QStringLiteral("x"));
		settle();
		const auto edited = std::string{ "/* note\r\n   still */\r\nint f()\r\n{\r\n\treturn g(1);x\r\n}\r\n" };
		expect(asked.size() == 3U && asked[2].Side == qt::suggestion_syntax_side::proposed && asked[2].Document == edited, "the edited file asked for after the edit");
		expect(!span_painted(*editor, 4U, 9U, flow), "a changed line drops colours that no longer describe it");
		expect(preview->toPlainText() == QStringLiteral("@@ -3,4 +3,4 @@\n  int f()\n  {\n- \treturn g(1);\n+ \treturn g(1);x\n  }"), "one change in context");
		expect(word_painted(*preview, QStringLiteral("return"), flow), "the removed row is coloured from the original's classification at once");
		const auto base    = preview->palette().color(QPalette::Base);
		const auto removed = over(profile.color(role::diff_removed_background), base);
		const auto added   = over(profile.color(role::diff_added_background), base);
		expect(!fill_is(removed, added) && !fill_is(removed, base) && !fill_is(added, base), "the diff fills differ from the background and each other");
		expect(fill_is(row_fill(*preview, QStringLiteral("- \treturn g(1);")), removed), "the removed row wears the diff panes' removed fill");
		expect(fill_is(row_fill(*preview, QStringLiteral("+ \treturn g(1);x")), added), "the added row wears the diff panes' added fill");
		expect(fill_is(row_fill(*preview, QStringLiteral("  int f()")), base), "an unchanged row wears none");
		expect(!aComposer.adopt_syntax(qt::suggestion_syntax_side::proposed, file, spans), "a late classification of the earlier text is refused");
		expect(aComposer.adopt_syntax(qt::suggestion_syntax_side::proposed, edited, spans), "the current one is adopted");
		settle();
		expect(span_painted(*editor, 4U, 9U, flow), "coloured again");
		const auto addedRow = [&] {
			for (auto block = preview->document()->firstBlock(); block.isValid(); block = block.next()) {
				if (block.text().startsWith(QLatin1Char('+'))) {
					return block;
				}
			}
			return QTextBlock{ };
		}();
		expect(std::ranges::any_of(addedRow.layout()->formats(), [&](const QTextLayout::FormatRange& aRange) { return aRange.start == 3 && aRange.length == 6 && aRange.format.foreground().color() == flow; }),
			   "the added row colours the keyword after its marker");
		capture(aComposer, QStringLiteral("suggestion-composer-syntax"));
		// A line that only moves keeps its colours until the next classification.
		key(*editor, Qt::Key_Home);
		key(*editor, Qt::Key_Home);
		key(*editor, Qt::Key_Return);
		settle();
		expect(editor->text() == "/* note\r\n   still */\r\nint f()\r\n{\r\n\r\n\treturn g(1);x\r\n}\r\n", "a line break before the line, in the file's own ending");
		expect(span_painted(*editor, 4U, 9U, flow), "the moved line keeps its colours");
		aComposer.SyntaxWanted = { };
	}

	/// Owner amendment to BPR-1156 (2026-10-01): the whole file is edited in
	/// place from a caret, and each separate change is one exact suggestion.
	void composer(qt::suggestion_composer& aComposer)
	{
		const auto file = std::string{ "int f()\r\n{\r\n\treturn caf\xC3\xA9(1);\r\n}\r\n" };
		expect(!aComposer.begin({ .Path = "big.txt", .File = std::string(core::maximum_suggestion_editor_bytes + 1U, 'a') }), "a file too large to hold is refused, not cut");
		// A selection is selected again in the editor.
		expect(aComposer.begin({ .Location = "src/f.cpp \xC2\xB7 R1", .Path = "src/f.cpp", .File = file, .Start = 20U, .End = 28U }), "begun on a selection");
		settle();
		auto* editor = aComposer.editor();
		expect(editor->text() == file && aComposer.edits().empty(), "the whole file, unchanged");
		const auto& caret = editor->model().state().Carets;
		expect(caret.size() == 1U && caret[0].Anchor == core::text_point{ 2U, 11U } && caret[0].Head == core::text_point{ 2U, 18U }, "the selection carried into the editor");
		expect(!aComposer.save_button()->isEnabled() && aComposer.summary_text().startsWith(QStringLiteral("Edit the file")), "nothing to save yet");

		// Typing replaces the selection; a second change two lines up stays separate.
		type(*editor, QStringLiteral("coffee(2)"));
		editor->apply(core::move_carets{ .Motion = core::text_motion::document_start });
		type(*editor, QStringLiteral("static "));
		settle();
		const auto edited = std::string{ "static int f()\r\n{\r\n\treturn coffee(2);\r\n}\r\n" };
		expect(editor->text() == edited, "edited in place");
		expect(aComposer.edits() == std::vector{ core::cycle_edit_hunk{ .Start = 0U, .End = 9U, .EditedStart = 0U, .Replacement = "static int f()\r\n" }, core::cycle_edit_hunk{ .Start = 12U, .End = 31U, .EditedStart = 19U, .Replacement = "\treturn coffee(2);\r\n" } },
			   "two separate changes, whole lines in the original's coordinates");
		expect(aComposer.summary_text() == QStringLiteral("2 changes · lines 1, 3") && aComposer.save_button()->isEnabled(), "summarised and savable");
		expect(aComposer.preview_text() == QStringLiteral("@@ -1,4 +1,4 @@\n- int f()\n+ static int f()\n  {\n- \treturn café(1);\n+ \treturn coffee(2);\n  }"), "nearby changes share one group");
		aComposer.set_source_status(qt::suggestion_source_status::newer_available);
		expect(aComposer.status_text().contains(QStringLiteral("revision you are editing")) && editor->text() == edited, "newer material stated; the edit stays");
		capture(aComposer, QStringLiteral("suggestion-composer-two-changes"));
		auto saved      = std::vector<core::cycle_edit_hunk>{ };
		aComposer.Saved = [&] { saved = aComposer.edits(); };
		aComposer.findChild<QCheckBox*>(QStringLiteral("suggestionIllustrative"))->setChecked(true);
		aComposer.save_button()->click();
		expect(saved.size() == 2U && aComposer.illustrative(), "save hands over every change and the intent");
		// Undo returns to the captured file, and with it nothing to save.
		key(*editor, Qt::Key_Z, Qt::ControlModifier);
		key(*editor, Qt::Key_Z, Qt::ControlModifier);
		settle();
		expect(editor->text() == file && aComposer.edits().empty() && !aComposer.save_button()->isEnabled(), "undone to the captured bytes");
	}
}

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication app{ aArgc, aArgv };
	auto         window = QWidget{ };
	auto*        editor = new qt::suggestion_editor{ &window };
	auto*        target = new qt::suggestion_editor{ &window };
	editor->setGeometry(0, 0, 520, 200);
	target->setGeometry(0, 210, 520, 200);
	window.resize(540, 420);
	window.show();
	settle();
	expect(QFontInfo{ editor->font() }.fixedPitch(), "the code font is fixed pitch");
	double_click_selects_the_word(*editor);
	box_then_shift_end(*editor);
	virtual_space_only_in_a_box(*editor);
	selection_reads_like_a_text_box(*editor);
	box_copy_and_paste(*editor, *target);
	ime_and_mouse(*editor);
	indents_like_an_editor(*editor);
	reads_without_editing(*editor);
	// The editor sits where the file is read; the composer beside it.
	auto        pane  = QWidget{ };
	auto* const code  = new qt::suggestion_editor{ &pane };
	auto* const panel = new qt::suggestion_composer{ *code, &pane };
	code->setGeometry(0, 0, 520, 360);
	panel->setGeometry(530, 0, 400, 360);
	pane.resize(940, 370);
	pane.show();
	settle();
	auto& composer = *panel;
	::composer(composer);
	syntax(composer);
	std::cout << "suggestion-editor-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
