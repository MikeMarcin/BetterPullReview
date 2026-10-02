#include "../support/test_expectation.hpp"
#include "bpr/qt/appearance_profile.hpp"
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

	/// Pixels of the rendered widget within a small distance of `aColour`.
	auto near(const QColor& aPixel, const QColor& aColour) -> bool
	{
		return std::abs(aPixel.red() - aColour.red()) <= 24 && std::abs(aPixel.green() - aColour.green()) <= 24 && std::abs(aPixel.blue() - aColour.blue()) <= 24;
	}

	auto pixels_near(QWidget& aWidget, const QColor& aColour) -> int
	{
		const auto image = aWidget.grab().toImage();
		auto       count = 0;
		for (int y = 0; y < image.height(); ++y) {
			for (int x = 0; x < image.width(); ++x) {
				if (near(image.pixelColor(x, y), aColour)) {
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
				role += near(pixel, aColour) ? 1 : 0;
				plain += near(pixel, text) ? 1 : 0;
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
				role += near(pixel, aColour) ? 1 : 0;
				plain += near(pixel, text) ? 1 : 0;
			}
		}
		return role > 0 && plain == 0;
	}

	auto chromatic(const QColor& aColour) -> bool
	{
		return std::max({ aColour.red(), aColour.green(), aColour.blue() }) - std::min({ aColour.red(), aColour.green(), aColour.blue() }) > 60;
	}

	/// BPR-1156: Suggest change paints classified code in the review panes'
	/// role colours: the proposal inside its whole file, the editor and the
	/// before/after preview, and nothing from a classification of other text.
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
		aComposer.begin({ .Location = "src/f.cpp \xC2\xB7 line 5 \xC2\xB7 R1", .Before = "{\r\n\t", .Original = "return g(1)", .After = ";\r\n}\r\n", .Path = "src/f.cpp", .File = file, .Start = 35U });
		settle();
		expect(asked.size() == 2U && asked[0].Side == qt::suggestion_syntax_side::original && asked[1].Side == qt::suggestion_syntax_side::proposed && asked[0].Path == "src/f.cpp"
				   && asked[0].Document == file && asked[1].Document == file,
			   "both texts asked for, each the whole file");
		auto* const editor  = aComposer.editor();
		auto* const preview = aComposer.findChild<QPlainTextEdit*>(QStringLiteral("suggestionPreview"));
		expect(pixels_near(*editor, flow) == 0, "nothing coloured before a classification arrives");

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
		expect(aComposer.adopt_syntax(qt::suggestion_syntax_side::original, file, spans) && aComposer.adopt_syntax(qt::suggestion_syntax_side::proposed, file, spans), "both adopted");
		settle();
		expect(span_painted(*editor, 0U, 5U, flow) && pixels_near(*editor, number) > 0, "the editor paints the keyword and the number in their role colours");
		capture(aComposer, QStringLiteral("suggestion-composer-syntax"));
		const auto removed = preview->document()->findBlockByNumber(1);
		expect(removed.text() == QStringLiteral("- \treturn g(1);"), "the removed row");
		const auto formats = removed.layout()->formats();
		expect(std::ranges::any_of(formats, [&](const QTextLayout::FormatRange& aRange) { return aRange.start == 3 && aRange.length == 6 && aRange.format.foreground().color() == flow; }),
			   "the preview colours the keyword after its marker");
		expect(word_painted(*preview, QStringLiteral("return"), flow), "and paints it");

		// An edit asks again; the edited line is plain until its own classification arrives.
		editor->setFocus();
		key(*editor, Qt::Key_End);
		type(*editor, QStringLiteral("x"));
		settle();
		const auto edited = std::string{ "/* note\r\n   still */\r\nint f()\r\n{\r\n\treturn g(1)x;\r\n}\r\n" };
		expect(asked.size() == 3U && asked[2].Side == qt::suggestion_syntax_side::proposed && asked[2].Document == edited, "the proposal asked for inside its file after the edit");
		expect(pixels_near(*editor, flow) == 0, "a changed line drops colours that no longer describe it");
		expect(!aComposer.adopt_syntax(qt::suggestion_syntax_side::proposed, file, spans), "a late classification of the earlier text is refused");
		expect(aComposer.adopt_syntax(qt::suggestion_syntax_side::proposed, edited, spans), "the current one is adopted");
		settle();
		expect(span_painted(*editor, 0U, 5U, flow), "coloured again");
		// A line that only moves keeps its colours until the next classification.
		key(*editor, Qt::Key_Home);
		key(*editor, Qt::Key_Home);
		key(*editor, Qt::Key_Return);
		settle();
		expect(editor->text() == "\r\nreturn g(1)x" || editor->text() == "\nreturn g(1)x", "a line break before the line");
		expect(span_painted(*editor, 0U, 5U, flow), "the moved line keeps its colours");
		aComposer.SyntaxWanted = { };
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
	syntax(composer);
	std::cout << "suggestion-editor-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
