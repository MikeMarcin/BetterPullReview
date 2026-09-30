#include "../support/test_expectation.hpp"
#include "bpr/qt/suggestion_composer.hpp"
#include "bpr/qt/suggestion_editor.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDir>
#include <QFontInfo>
#include <QImage>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>

#include <cstdlib>
#include <iostream>
#include <string>

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
		auto editor = core::suggestion_text_editor{ aEditor.text() };
		static_cast<void>(editor);
		const auto  rectangle = aEditor.caret_rectangle();
		const auto  width     = aEditor.fontMetrics().horizontalAdvance(QLatin1Char('M'));
		const auto  height    = rectangle.height();
		const auto  position  = QPointF{ 6.0 + static_cast<double>(aAt.Column) * width + 1.0, static_cast<double>(aAt.Line) * height + height / 2.0 };
		QMouseEvent press{ QEvent::MouseButtonPress, position, aEditor.viewport()->mapToGlobal(position), Qt::LeftButton, Qt::LeftButton, aModifiers };
		QApplication::sendEvent(aEditor.viewport(), &press);
		QMouseEvent release{ QEvent::MouseButtonRelease, position, aEditor.viewport()->mapToGlobal(position), Qt::LeftButton, Qt::NoButton, aModifiers };
		QApplication::sendEvent(aEditor.viewport(), &release);
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

	void virtual_space_tabs_and_deletion(qt::suggestion_editor& aEditor)
	{
		aEditor.set_text("\tlong line here\n\tab\n");
		settle();
		key(aEditor, Qt::Key_End);
		key(aEditor, Qt::Key_Down);
		expect(heads(aEditor).front() == core::text_point{ 1U, 18U }, "down keeps the column in virtual space");
		type(aEditor, QStringLiteral(";"));
		expect(aEditor.text() == "\tlong line here\n\tab            ;\n", "virtual space filled with spaces on typing");
		key(aEditor, Qt::Key_Backspace);
		key(aEditor, Qt::Key_Home);
		key(aEditor, Qt::Key_Home);
		key(aEditor, Qt::Key_Tab);
		expect(aEditor.text() == "\tlong line here\n\t\tab            \n", "Tab inserts a tab");
		key(aEditor, Qt::Key_Return);
		key(aEditor, Qt::Key_Delete);
		expect(aEditor.text() == "\tlong line here\n\t\nab            \n", "line break and forward delete");
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

	void composer(qt::suggestion_composer& aComposer)
	{
		aComposer.begin({
			.Location = "src/f.cpp \xC2\xB7 line 3 \xC2\xB7 R1",
			.Before   = "int f()\r\n{\r\n\treturn ",
			.Original = "caf\xC3\xA9(1)",
			.After    = ";\r\n}\r\n",
		});
		settle();
		expect(aComposer.replacement() == "caf\xC3\xA9(1)", "prefilled with the exact selected bytes");
		auto* editor = aComposer.editor();
		key(*editor, Qt::Key_End);
		key(*editor, Qt::Key_Left);
		key(*editor, Qt::Key_Backspace);
		type(*editor, QStringLiteral("2"));
		expect(aComposer.replacement() == "caf\xC3\xA9(2)", "edited");
		expect(aComposer.preview_text() == QStringLiteral("  int f()\n  {\n- \treturn café(1);\n+ \treturn café(2);\n  }\n  \n"), "before and after in context");
		aComposer.set_source_status(qt::suggestion_source_status::stale);
		expect(aComposer.status_text().contains(QStringLiteral("original bytes")) && aComposer.replacement() == "caf\xC3\xA9(2)", "stale source stated; the edit stays");
		capture(aComposer, QStringLiteral("suggestion-composer-stale"));
		auto saved      = std::string{ };
		aComposer.Saved = [&] { saved = aComposer.replacement(); };
		aComposer.findChild<QCheckBox*>(QStringLiteral("suggestionIllustrative"))->setChecked(true);
		aComposer.save_button()->click();
		expect(saved == "caf\xC3\xA9(2)" && aComposer.illustrative(), "save hands over the exact bytes and intent");
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
	box_then_shift_end(*editor);
	virtual_space_tabs_and_deletion(*editor);
	box_copy_and_paste(*editor, *target);
	ime_and_mouse(*editor);
	auto composer = qt::suggestion_composer{ };
	composer.resize(560, 520);
	composer.show();
	settle();
	::composer(composer);
	std::cout << "suggestion-editor-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
