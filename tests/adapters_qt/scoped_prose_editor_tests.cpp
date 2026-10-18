#include "../support/review_tour_fixture.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include <QApplication>
#include <QInputMethodEvent>
#include <QPushButton>
#include <QTabWidget>
#include <cstdlib>
#include <iostream>
namespace {
	using namespace bpr;
	void expect(bool aPass, const char* aMessage)
	{
		if (!aPass) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	auto target(const qt::prose_capture& aC) -> core::writing_target
	{
		const auto t = tests::review_tour_fixture();
		return { .Owner = t.Owner, .Draft = t.Id, .Hook = core::writing_hook::problem, .Window = { core::bpr_profile_id{ 1 }, core::bpr_window_id{ 2 }, core::bpr_window_generation{ 1 } }, .Session = core::application_session_id{ 1 }, .Agent = core::agent_profile_id{ 1 }, .HookRevision = 1, .TextGeneration = aC.TextGeneration, .EditorGeneration = aC.EditorGeneration };
	}
	auto rewrite(qt::scoped_prose_editor& aE, std::string aText) -> bool
	{
		const auto c = aE.capture();
		expect(c.has_value(), "capture maps Qt UTF-16 to canonical bytes");
		core::scoped_writing_state state;
		const auto                 request = state.begin(target(*c), c->Raw, c->Selection);
		expect(request.has_value(), "captured native request accepted");
		auto result = state.complete(request->Id, request->Target, aE.raw(), std::move(aText));
		return result && aE.apply(*result);
	}
	void drain() { QApplication::processEvents(); }
	void native_history()
	{
		qt::scoped_prose_editor e;
		e.show();
		e.setFocus();
		const std::string original = "# 👩‍💻 café é\r\n\tBody\r\nEnd\n";
		e.present(original);
		e.moveCursor(QTextCursor::End);
		e.insertPlainText("before");
		const auto before = e.raw();
		auto       cursor = e.textCursor();
		cursor.setPosition(16);
		cursor.setPosition(2, QTextCursor::KeepAnchor);
		e.setTextCursor(cursor);
		const auto capture = *e.capture();
		expect(rewrite(e, "**new**\r\n😀"), "selected replacement applies");
		const auto after = e.raw();
		expect(after == before.substr(0, capture.Selection.Begin) + "**new**\r\n😀" + before.substr(capture.Selection.End), "prefix/suffix preserve exact CRLF and Unicode");
		e.insertPlainText("after");
		e.undo();
		drain();
		expect(e.raw() == after, "typing after rewrite is a distinct native undo");
		static_cast<QPlainTextEdit*>(&e)->undo();
		drain();
		expect(e.raw() == before, "one native undo restores raw bytes");
		expect(e.textCursor().anchor() == capture.Selection.Anchor && e.textCursor().position() == capture.Selection.Cursor, "undo restores reversed selection");
		e.undo();
		drain();
		expect(e.raw() == original, "prior typing remains a distinct native undo");
		e.redo();
		e.redo();
		drain();
		expect(e.raw() == after, "native redo replays received raw bytes");
		auto generation = e.capture()->TextGeneration;
		e.present(after);
		expect(e.capture()->TextGeneration == generation, "save ACK preserves native document/generation");
		e.undo();
		drain();
		expect(e.raw() == before, "save ACK preserves undo");
		expect(e.capture()->TextGeneration > capture.TextGeneration, "undo-back does not revive captured request");
		e.redo();
		drain();
		e.selectAll();
		expect(rewrite(e, "second\r\n"), "repeated enhance accepted");
		e.undo();
		drain();
		expect(e.raw() == after, "second enhance has its own native undo");
		e.redo();
		drain();
		e.selectAll();
		expect(rewrite(e, ""), "empty replacement removes captured text");
		e.undo();
		drain();
		expect(e.raw() == "second\r\n", "empty replacement undoes exactly");
		e.selectAll();
		const auto steps = e.document()->availableUndoSteps();
		expect(!rewrite(e, e.raw()), "equal replacement is no-op");
		expect(e.document()->availableUndoSteps() == steps, "equal creates no native undo entry");
	}
	void unicode_paragraphs()
	{
		qt::scoped_prose_editor editor;
		const std::string       raw = "A B C D\r\nE";
		editor.present(raw);
		auto cursor = editor.textCursor();
		cursor.setPosition(2);
		cursor.setPosition(7, QTextCursor::KeepAnchor);
		editor.setTextCursor(cursor);
		const auto capture = editor.capture();
		expect(capture.has_value(), "Unicode separators and NBSP have exact raw mapping");
		expect(rewrite(editor, "X Y"), "Unicode separator selection is replaceable");
		expect(editor.raw() == raw.substr(0, capture->Selection.Begin) + "X Y" + raw.substr(capture->Selection.End), "Unicode raw prefix/suffix remain exact");
		editor.undo();
		drain();
		expect(editor.raw() == raw, "Unicode separator/space undo restores exact raw bytes");
	}
	void moved_caret_and_stale()
	{
		qt::scoped_prose_editor e;
		e.present("abc\r\ndef");
		e.moveCursor(QTextCursor::End);
		auto                       c = *e.capture();
		core::scoped_writing_state state;
		auto                       request = state.begin(target(c), c.Raw, c.Selection);
		expect(request.has_value(), "whole field captured");
		e.moveCursor(QTextCursor::Start);
		auto result = state.complete(request->Id, request->Target, e.raw(), "ABC\r\nDEF");
		expect(result && e.apply(*result), "caret movement alone does not retarget replacement");
		expect(e.textCursor().position() == 0, "completion retains moved caret");
		c       = *e.capture();
		request = state.begin(target(c), c.Raw, c.Selection);
		QInputMethodEvent preedit{ QStringLiteral("仮"), { } };
		QApplication::sendEvent(&e, &preedit);
		result = state.complete(request->Id, target(*e.capture()), e.raw(), "late");
		expect(!result, "IME preedit invalidates old request before text commit");
		QInputMethodEvent commit;
		commit.setCommitString(QStringLiteral("確"));
		QApplication::sendEvent(&e, &commit);
		expect(e.raw().starts_with("確"), "native IME commit retained");
		c       = *e.capture();
		request = state.begin(target(c), c.Raw, c.Selection);
		e.present(e.raw(), true);
		result = state.complete(request->Id, target(*e.capture()), e.raw(), "other owner");
		expect(!result, "identical owner replacement invalidates editor identity");
	}
	/// [[BPR-1062]]. Text another presentation changed keeps this author's caret, selection and focus.
	void remote_change_keeps_the_authors_place()
	{
		qt::scoped_prose_editor e;
		e.show();
		e.setFocus();
		e.present("alpha beta gamma");
		const auto place = [&](int aNchor, int aCaret) {
			auto cursor = e.textCursor();
			cursor.setPosition(aNchor);
			cursor.setPosition(aCaret, QTextCursor::KeepAnchor);
			e.setTextCursor(cursor);
		};
		place(2, 2);
		e.present("alpha beta gamma!");
		expect(e.textCursor().position() == 2 && !e.textCursor().hasSelection(), "an appended remote edit leaves an earlier caret alone");
		place(14, 14);
		e.present("XX alpha beta gamma!");
		expect(e.textCursor().position() == 17, "text inserted before the caret shifts it with the text it followed");
		place(3, 9);
		e.present("XX alpha beta gamma!!");
		expect(e.textCursor().anchor() == 3 && e.textCursor().position() == 9, "a selection survives an edit after it");
		place(11, 11);
		e.present("XX alpha B gamma!!");
		expect(e.textCursor().position() == 9 && e.raw() == "XX alpha B gamma!!", "a caret inside replaced text goes to where the replacement starts");
		const auto owner = e.hasFocus();
		e.present("something else entirely");
		expect(e.hasFocus() == owner && e.textCursor().position() >= 0 && e.textCursor().position() <= 23, "a remote replacement never takes or drops focus");
		place(4, 4);
		e.present("new draft", true);
		expect(e.textCursor().position() == 0, "another owner's text starts at its beginning, not at the previous caret");
	}
	void composing_is_visible_to_other_presentations()
	{
		qt::review_tour_editor         editor;
		application::tour_presentation p;
		p.Draft = std::make_shared<const core::review_tour>(tests::review_tour_fixture());
		editor.present(p);
		editor.show();
		editor.show_problem();
		auto* problem = editor.problem_editor();
		problem->setFocus();
		expect(!editor.composing(), "an idle editor holds no preedit");
		QInputMethodEvent preedit{ QStringLiteral("仮"), { } };
		QApplication::sendEvent(problem, &preedit);
		expect(editor.composing(), "preedit text is composing, not committed");
		QInputMethodEvent commit;
		commit.setCommitString(QStringLiteral("確"));
		QApplication::sendEvent(problem, &commit);
		expect(!editor.composing() && problem->toPlainText().contains(QStringLiteral("確")), "commit ends composing and the text is committed");
		QApplication::sendEvent(problem, &preedit);
		expect(editor.composing(), "a second composition is seen");
		QInputMethodEvent cancel{ QString{ }, { } };
		QApplication::sendEvent(problem, &cancel);
		expect(!editor.composing(), "cancelling composition clears it");
	}
	void real_tour_buttons()
	{
		qt::review_tour_editor         editor;
		application::tour_presentation p;
		auto                           draft = tests::review_tour_fixture();
		draft.Problem                        = "A\r\n  \tB";
		p.Draft                              = std::make_shared<const core::review_tour>(draft);
		editor.present(p);
		editor.show();
		editor.show_problem();
		unsigned                         calls         = 0;
		unsigned                         cancellations = 0;
		std::optional<qt::prose_capture> captured;
		editor.set_writing_dispatch([&](core::writing_hook aHook, qt::prose_capture aC) {expect(aHook==core::writing_hook::problem,"clicked field owns request");++calls;captured=std::move(aC); }, [&] { ++cancellations; });
		auto* prose  = editor.writing_editor(core::writing_hook::problem);
		auto  cursor = prose->textCursor();
		cursor.setPosition(2);
		cursor.setPosition(5, QTextCursor::KeepAnchor);
		prose->setTextCursor(cursor);
		auto* magic = editor.findChild<QPushButton*>(QStringLiteral("tourProblemMagic"));
		expect(magic && magic->text() == "Enhance selection", "whitespace has selection label");
		magic->click();
		expect(calls == 1 && captured && captured->Raw.substr(captured->Selection.Begin, captured->Selection.End - captured->Selection.Begin) == "  \t", "toolbar preserves captured whitespace selection");
		editor.roadmap()->setCurrentIndex(1);
		editor.present(p);
		expect(calls == 1, "navigation and projection never invoke assistance");
		std::string persisted;
		editor.set_dispatch([&](core::tour_command aCmd) { persisted = aCmd.Text; }, [](qt::tour_editor_action) { });
		editor.show_problem();
		prose->moveCursor(QTextCursor::End);
		prose->insertPlainText("x");
		expect(persisted == "A\r\n  \tBx", "manual native edits persist untouched CRLF");
		QWidget other;
		editor.activateWindow();
		drain();
		const auto previous = cancellations;
		other.show();
		other.activateWindow();
		drain();
		expect(cancellations > previous && calls == 1, "switching windows cancels without invoking assistance");
	}
}
int main(int argc, char** argv)
{
	QApplication app(argc, argv);
	native_history();
	unicode_paragraphs();
	moved_caret_and_stale();
	real_tour_buttons();
	remote_change_keeps_the_authors_place();
	composing_is_visible_to_other_presentations();
	std::cout << "scoped-prose-editor-tests:pass\n";
}
