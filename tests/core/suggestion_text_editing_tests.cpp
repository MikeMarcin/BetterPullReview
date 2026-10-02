#include "bpr/core/suggestion_text_editing.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const char* aWhat)
	{
		if (!aValue) {
			std::cerr << "suggestion text editing failure: " << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto heads(const suggestion_text_editor& aEditor) -> std::vector<text_point>
	{
		auto found = std::vector<text_point>{ };
		for (const auto& caret : aEditor.state().Carets) {
			found.push_back(caret.Head);
		}
		return found;
	}

	void keeps_exact_bytes()
	{
		for (const auto text : { std::string_view{ "" }, std::string_view{ "a\r\nb\rc\nd" }, std::string_view{ "\tint x;\r\n}\r\n" }, std::string_view{ "caf\xC3\xA9\n\xF0\x9F\x98\x80" } }) {
			const auto editor = suggestion_text_editor{ text };
			expect(editor.text() == text, "round trip");
		}
		const auto lines = split_text_lines("a\r\nb\rc\nd");
		expect(lines.size() == 4 && lines[0].Ending == text_line_ending::crlf && lines[1].Ending == text_line_ending::cr && lines[2].Ending == text_line_ending::lf && lines[3].Ending == text_line_ending::none, "line endings kept");
	}

	/// Owner amendment to BPR-1156: Suggest change opens the whole file at the
	/// reader's caret, so a byte offset of the file names its point.
	void locates_byte_offsets()
	{
		const auto editor = suggestion_text_editor{ "int f()\r\n{\r\n\treturn caf\xC3\xA9(1);\r\n}" };
		expect(editor.point_of(0U) == text_point{ 0U, 0U } && editor.point_of(7U) == text_point{ 0U, 7U }, "the first line");
		expect(editor.point_of(8U) == text_point{ 0U, 7U }, "inside a CRLF: the end of its line");
		expect(editor.point_of(12U) == text_point{ 2U, 0U } && editor.point_of(13U) == text_point{ 2U, 4U }, "a tab advances to its stop");
		expect(editor.point_of(23U) == text_point{ 2U, 14U } && editor.point_of(25U) == text_point{ 2U, 15U }, "a two-byte character is one column");
		expect(editor.point_of(31U) == text_point{ 3U, 0U } && editor.point_of(99U) == text_point{ 3U, 1U }, "the last line and past the end");
	}

	/// Alt+Shift+Down twice, then Shift+End: one caret per line, each
	/// selecting to its own line end and never across a line break.
	void box_then_shift_end_stays_on_each_line()
	{
		auto editor = suggestion_text_editor{ "auto a = 1;\r\nauto bb = 22;\r\nauto c = 3;\r\n" };
		editor.apply(place_caret{ { 0U, 5U } });
		expect(editor.apply(extend_box{ text_motion::down }) && editor.apply(extend_box{ text_motion::down }), "box grows down");
		expect(heads(editor) == std::vector<text_point>{ { 0U, 5U }, { 1U, 5U }, { 2U, 5U } } && editor.state().Primary == 2U, "three carets at one column");
		editor.apply(move_carets{ .Motion = text_motion::line_end, .Extend = true });
		const auto& carets = editor.state().Carets;
		expect(carets.size() == 3U, "carets kept");
		for (std::uint64_t line = 0; line < 3U; ++line) {
			expect(carets[line].Anchor == text_point{ line, 5U } && carets[line].Head == text_point{ line, editor.line_width(line) } && carets[line].Head.Line == line, "each selection ends on its own line");
		}
		expect(editor.copy() == copied_text{ .Text = "a = 1;\r\nbb = 22;\r\nc = 3;", .Box = true }, "box copy joined by the text's ending");
		editor.apply(insert_text{ "x" });
		expect(editor.text() == "auto x\r\nauto x\r\nauto x\r\n", "typing replaces each line's tail; CRLF kept");
		expect(heads(editor) == std::vector<text_point>{ { 0U, 6U }, { 1U, 6U }, { 2U, 6U } }, "carets after the typed text");
		editor.apply(collapse_carets{ });
		expect(editor.state().Carets.size() == 1U && editor.state().Carets[0].Head == text_point{ 2U, 6U }, "Escape keeps the primary caret");
	}

	/// Virtual space belongs to the box (owner note on composer-layout v005,
	/// 2026-10-02): every other gesture keeps a caret on its line's text.
	void edits_in_virtual_space()
	{
		auto editor = suggestion_text_editor{ "long line here\nab\n\nlonger line here\n" };
		editor.apply(place_caret{ { 0U, 10U } });
		editor.apply(move_carets{ text_motion::down });
		expect(heads(editor).front() == text_point{ 1U, 2U }, "down stops at a short line's end");
		editor.apply(move_carets{ text_motion::down });
		expect(heads(editor).front() == text_point{ 2U, 0U } && editor.state().Goals == std::vector<std::uint64_t>{ 10U }, "an empty line, the column still aimed for");
		editor.apply(move_carets{ .Motion = text_motion::down, .Extend = true });
		expect(editor.state().Carets[0] == text_caret{ { 2U, 0U }, { 3U, 10U } }, "and reached again on a long enough line, extending too");
		editor.apply(move_carets{ text_motion::up });
		editor.apply(move_carets{ text_motion::up });
		expect(heads(editor).front() == text_point{ 1U, 2U }, "up stops at the end as well");
		editor.apply(move_carets{ text_motion::right });
		expect(heads(editor).front() == text_point{ 2U, 0U } && editor.state().Goals.empty(), "right at a line's end goes on to the next line and forgets the column");
		editor.apply(place_caret{ { 1U, 9U } });
		expect(heads(editor).front() == text_point{ 1U, 2U }, "a click past the end lands at the end");
		editor.apply(place_caret{ { 0U, 3U } });
		editor.apply(place_caret{ .At = { 1U, 9U }, .Extend = true });
		expect(editor.state().Carets[0] == text_caret{ { 0U, 3U }, { 1U, 2U } }, "so does a Shift+click or a drag");
		editor.apply(add_caret{ { 2U, 7U } });
		expect(heads(editor).back() == text_point{ 2U, 0U }, "and an Alt+click");
		editor.apply(collapse_carets{ });
		editor.apply(place_caret{ { 1U, 9U } });
		editor.apply(insert_text{ "y" });
		expect(editor.text() == "long line here\naby\n\nlonger line here\n", "typing after a click past the end adds no blanks");
		// The box alone reaches past a line's end: a zero-width box across
		// short and long lines, then typing, pads the short ones.
		auto box = suggestion_text_editor{ "abcdef\nab\nabcd\n" };
		box.apply(place_caret{ { 0U, 4U } });
		box.apply(extend_box{ text_motion::down });
		box.apply(extend_box{ text_motion::down });
		expect(heads(box) == std::vector<text_point>{ { 0U, 4U }, { 1U, 4U }, { 2U, 4U } }, "box carets stand in virtual space");
		const auto boxed = box.text();
		box.apply(delete_backward{ });
		expect(box.text() != boxed && heads(box)[1] == text_point{ 1U, 3U }, "backspace in a box's virtual space moves left there and deletes elsewhere");
		box.apply(undo_edit{ });
		box.apply(insert_text{ "|" });
		expect(box.text() == "abcd|ef\nab  |\nabcd|\n", "box insertion pads short lines");
		auto back = suggestion_text_editor{ "abcdef\nab\n" };
		back.apply(place_caret{ { 0U, 5U } });
		back.apply(extend_box{ text_motion::down });
		back.apply(move_carets{ text_motion::left });
		expect(heads(back) == std::vector<text_point>{ { 0U, 4U }, { 1U, 2U } }, "after the box, Left brings a caret in virtual space back to its line's end");
	}

	/// A drag with the button held after a double or triple click grows by
	/// whole words or lines and never loses what the clicks selected (owner
	/// note on composer-layout v005: a double click must select the word).
	void drags_by_words_and_lines()
	{
		auto       editor = suggestion_text_editor{ "auto value = other_name;\nnext line\nlast\n" };
		const auto word   = text_caret{ { 0U, 5U }, { 0U, 10U } };
		editor.apply(select_word_at{ { 0U, 7U } });
		expect(editor.state().Carets[0] == word, "the double-clicked word");
		// The character under the pointer: any of the word's five.
		for (const auto column : { 5U, 7U, 9U }) {
			editor.apply(drag_selection{ .Origin = word, .To = { 0U, column }, .Unit = selection_unit::word });
			expect(editor.state().Carets[0] == word, "a pointer drifting inside the word keeps the word");
		}
		editor.apply(drag_selection{ .Origin = word, .To = { 0U, 15U }, .Unit = selection_unit::word });
		expect(editor.state().Carets[0] == text_caret{ { 0U, 5U }, { 0U, 23U } }, "forward by whole words");
		editor.apply(drag_selection{ .Origin = word, .To = { 0U, 1U }, .Unit = selection_unit::word });
		expect(editor.state().Carets[0] == text_caret{ { 0U, 10U }, { 0U, 0U } }, "backward by whole words, from the word's far end");
		const auto line = text_caret{ { 1U, 0U }, { 2U, 0U } };
		editor.apply(select_line_at{ { 1U, 3U } });
		expect(editor.state().Carets[0] == line, "the triple-clicked line");
		editor.apply(drag_selection{ .Origin = line, .To = { 1U, 8U }, .Unit = selection_unit::line });
		expect(editor.state().Carets[0] == line, "a pointer drifting inside the line keeps it");
		editor.apply(drag_selection{ .Origin = line, .To = { 2U, 1U }, .Unit = selection_unit::line });
		expect(editor.state().Carets[0] == text_caret{ { 1U, 0U }, { 3U, 0U } }, "down by whole lines");
		editor.apply(drag_selection{ .Origin = line, .To = { 0U, 30U }, .Unit = selection_unit::line });
		expect(editor.state().Carets[0] == text_caret{ { 2U, 0U }, { 0U, 0U } }, "up by whole lines");
		editor.apply(drag_selection{ .Origin = text_caret{ { 0U, 2U }, { 0U, 2U } }, .To = { 1U, 30U }, .Unit = selection_unit::character });
		expect(editor.state().Carets[0] == text_caret{ { 0U, 2U }, { 1U, 9U } }, "by characters, stopping at a line's end");
		expect(!changes_text(drag_selection{ }), "a drag changes no text");
	}

	void copies_and_pastes_boxes()
	{
		auto editor = suggestion_text_editor{ "abcdef\nabcdef\nab\n" };
		editor.apply(place_caret{ { 0U, 1U } });
		for (int step = 0; step < 3; ++step) {
			editor.apply(extend_box{ text_motion::right });
		}
		editor.apply(extend_box{ text_motion::down });
		editor.apply(extend_box{ text_motion::down });
		const auto copied = editor.copy();
		expect(copied == copied_text{ .Text = "bcd\nbcd\nb  ", .Box = true }, "box copy pads virtual space");
		// Onto one caret: a box from that caret down, adding lines as needed.
		auto target = suggestion_text_editor{ "12345\n12345" };
		target.apply(place_caret{ { 1U, 2U } });
		target.apply(paste_text{ copied.Text, copied.Box });
		expect(target.text() == "12345\n12bcd345\n  bcd\n  b  ", "box paste");
		// Onto as many carets: one line each.
		auto spread = suggestion_text_editor{ "x\ny\nz\n" };
		spread.apply(place_caret{ { 0U, 1U } });
		spread.apply(extend_box{ text_motion::down });
		spread.apply(extend_box{ text_motion::down });
		spread.apply(paste_text{ "1\n2\n3\n" });
		expect(spread.text() == "x1\ny2\nz3\n", "one line per caret");
		// Onto a CRLF text, pasted line breaks take the text's ending.
		auto crlf = suggestion_text_editor{ "a\r\nb\r\n" };
		crlf.apply(place_caret{ { 0U, 1U } });
		crlf.apply(paste_text{ "1\n2" });
		expect(crlf.text() == "a1\r\n2\r\nb\r\n", "pasted breaks follow the text");
	}

	void handles_tabs_breaks_and_deletion()
	{
		auto editor = suggestion_text_editor{ "\tx\r\n\t\ty\r\n" };
		expect(editor.line_width(0U) == 5U && editor.line_width(1U) == 9U, "tab stops");
		editor.apply(place_caret{ { 1U, 6U } });
		expect(heads(editor).front() == text_point{ 1U, 4U }, "a column inside a tab snaps to its start");
		editor.apply(insert_text{ "\t" });
		expect(editor.text() == "\tx\r\n\t\t\ty\r\n", "tab inserted as a tab");
		editor.apply(insert_line_break{ });
		expect(editor.text() == "\tx\r\n\t\t\r\n\t\t\ty\r\n" && heads(editor).front() == text_point{ 2U, 8U }, "a line break takes CRLF and keeps the indentation");
		editor.apply(place_caret{ { 2U, 0U } });
		editor.apply(delete_backward{ });
		expect(editor.text() == "\tx\r\n\t\t\t\t\ty\r\n", "backspace at a line start removes the whole CRLF");
		editor.apply(place_caret{ { 0U, 5U } });
		editor.apply(delete_forward{ });
		expect(editor.text() == "\tx\t\t\t\t\ty\r\n", "delete at a line end joins the next line");
		editor.apply(select_all{ });
		editor.apply(delete_forward{ });
		expect(editor.text().empty(), "deleting everything");
		auto unicode = suggestion_text_editor{ "caf\xC3\xA9" };
		unicode.apply(move_carets{ text_motion::document_end });
		unicode.apply(delete_backward{ });
		expect(unicode.text() == "caf", "backspace removes a whole scalar");
		unicode.apply(insert_text{ "\xE6\x97\xA5\xE6\x9C\xAC" });
		expect(unicode.text() == "caf\xE6\x97\xA5\xE6\x9C\xAC", "committed IME text inserted exactly");
	}

	/// The owner asked for indentation on Enter like a modern editor
	/// (BPR-1156, 2026-10-02): the line's blanks, a level after an opener,
	/// a bracket's closer on its own line, and a typed closer outdents.
	void indents_on_enter()
	{
		auto editor = suggestion_text_editor{ "\tif (x) {\n\t}\n" };
		editor.apply(place_caret{ { 0U, 13U } });
		expect(editor.apply(insert_line_break{ }) && editor.text() == "\tif (x) {\n\t\t\n\t}\n" && heads(editor).front() == text_point{ 1U, 8U }, "a level more after an opening brace");
		editor.apply(insert_text{ "y();" });
		editor.apply(insert_line_break{ });
		expect(editor.text() == "\tif (x) {\n\t\ty();\n\t\t\n\t}\n", "the same level after an ordinary line");
		editor.apply(insert_text{ "}" });
		expect(editor.text() == "\tif (x) {\n\t\ty();\n\t}\n\t}\n" && heads(editor).front() == text_point{ 2U, 5U }, "a closer typed on blanks takes a level away");
		expect(editor.apply(undo_edit{ }) && editor.text() == "\tif (x) {\n\t\ty();\n\t\t\n\t}\n", "and undoes as typing");

		// Between a bracket and its closer: the closer goes on its own line.
		auto pair = suggestion_text_editor{ "    call(\r\n" };
		pair.set_indentation({ .Unit = "    " });
		pair.apply(place_caret{ { 0U, 9U } });
		pair.apply(insert_text{ ")" });
		pair.apply(move_carets{ text_motion::left });
		expect(pair.apply(insert_line_break{ }) && pair.text() == "    call(\r\n        \r\n    )\r\n" && heads(pair).front() == text_point{ 1U, 8U }, "the closer on its own line, CRLF kept");

		// The caret inside the indentation keeps only what lies before it.
		auto inside = suggestion_text_editor{ "\t\tx\n" };
		inside.apply(place_caret{ { 0U, 4U } });
		inside.apply(insert_line_break{ });
		expect(inside.text() == "\t\n\t\tx\n", "only the blanks before the caret");

		// Every caret indents for its own line; virtual space adds no blanks.
		auto carets = suggestion_text_editor{ "a {\n  b\n" };
		carets.apply(place_caret{ { 0U, 3U } });
		carets.apply(add_caret{ { 1U, 9U } });
		carets.apply(insert_line_break{ });
		expect(carets.text() == "a {\n\t\n  b\n  \n", "each caret its own indentation");

		// A colon opens a level only where the language says so.
		auto python = suggestion_text_editor{ "def f():\n" };
		python.set_indentation(indentation_for("tool/run.py", python.text()));
		python.apply(place_caret{ { 0U, 8U } });
		python.apply(insert_line_break{ });
		expect(python.text() == "def f():\n    \n", "Python: four spaces after a colon");
		auto cpp = suggestion_text_editor{ "public:\n" };
		cpp.set_indentation(indentation_for("a.hpp", cpp.text()));
		cpp.apply(place_caret{ { 0U, 7U } });
		cpp.apply(insert_line_break{ });
		expect(cpp.text() == "public:\n\n", "C++: no level after a colon");
		auto prose = suggestion_text_editor{ "  See (\n" };
		prose.set_indentation(indentation_for("README.md", prose.text()));
		prose.apply(place_caret{ { 0U, 7U } });
		prose.apply(insert_line_break{ });
		expect(prose.text() == "  See (\n  \n", "prose: the indentation only");
	}

	void chooses_the_indentation_unit()
	{
		expect(indentation_for("a.cpp", "int f()\n{\n\treturn 1;\n}\n") == text_indentation{ .Unit = "\t" }, "tabs");
		expect(indentation_for("a.ts", "f() {\n  if (x) {\n    y();\n  }\n}\n").Unit == "  ", "two spaces");
		expect(indentation_for("a.ts", "f() {\n    if (x) {\n        y();\n           z;\n    }\n}\n").Unit == "    ", "the common step, not an alignment");
		expect(indentation_for("CONFIG.YML", "a:\n").Unit == "  " && indentation_for("CONFIG.YML", "a:\n").Colon, "YAML by extension, any case");
		expect(indentation_for("dir.v2/Makefile", "all:\n").Unit == "\t" && !indentation_for("dir.v2/Makefile", "all:\n").Colon, "a dot in a folder is no extension");
		expect(!indentation_for("notes.txt", "x\n").Brackets, "prose opens nothing");
	}

	void selects_words_and_lines()
	{
		auto editor = suggestion_text_editor{ "\tint caf\xC3\xA9_x = a->b;\r\nnext\r\n" };
		editor.apply(select_word_at{ { 0U, 10U } });
		expect(editor.state().Carets.size() == 1U && editor.state().Carets[0] == text_caret{ { 0U, 8U }, { 0U, 14U } }, "a double click selects the word, multi-byte letters included");
		expect(editor.copy().Text == "caf\xC3\xA9_x", "the word's bytes");
		editor.apply(select_word_at{ { 0U, 18U } });
		expect(editor.copy().Text == "-", "one punctuation mark");
		editor.apply(select_word_at{ { 0U, 2U } });
		expect(editor.copy().Text == "\t", "a run of blanks");
		editor.apply(select_word_at{ { 0U, 40U } });
		expect(editor.copy().Text == ";", "past the end: the last thing on the line");
		editor.apply(select_line_at{ { 0U, 3U } });
		expect(editor.copy().Text == "\tint caf\xC3\xA9_x = a->b;\r\n", "a triple click selects the line with its break");
		editor.apply(select_line_at{ { 9U, 0U } });
		expect(editor.state().Carets[0] == text_caret{ { 2U, 0U }, { 2U, 0U } }, "the last, empty line");
		expect(!changes_text(select_word_at{ }) && !changes_text(place_caret{ }) && changes_text(insert_line_break{ }) && changes_text(undo_edit{ }), "which edits change text");
		expect(editor.byte_offset_of({ 0U, 14U }) == 12U && editor.byte_offset_of({ 1U, 2U }) == 24U && editor.byte_offset_of({ 1U, 30U }) == 26U, "byte offsets of points; virtual space is the line end");
	}

	void undoes_ordinary_steps()
	{
		auto editor = suggestion_text_editor{ "return 1;\n" };
		editor.apply(place_caret{ { 0U, 7U } });
		editor.apply(move_carets{ .Motion = text_motion::right, .Extend = true });
		for (const auto* text : { "4", "2" }) {
			editor.apply(insert_text{ text });
		}
		expect(editor.text() == "return 42;\n", "typed");
		expect(editor.apply(undo_edit{ }) && editor.text() == "return 1;\n", "typing is one undo step");
		expect(editor.state().Carets[0] == text_caret{ { 0U, 7U }, { 0U, 8U } }, "undo restores the selection");
		expect(editor.apply(redo_edit{ }) && editor.text() == "return 42;\n", "redo");
		editor.apply(undo_edit{ });
		editor.apply(insert_text{ "0" });
		expect(!editor.can_redo(), "a new edit clears redo");
		expect(!editor.apply(redo_edit{ }), "nothing to redo");
	}

	void refuses_over_the_bound()
	{
		auto editor = suggestion_text_editor{ std::string(maximum_suggestion_editor_bytes - 1U, 'a') };
		editor.apply(move_carets{ text_motion::document_end });
		const auto before = editor.state();
		expect(!editor.apply(insert_text{ "bc" }) && editor.state() == before && !editor.can_undo(), "over the bound changes nothing");
		expect(editor.apply(insert_text{ "b" }) && editor.text().size() == maximum_suggestion_editor_bytes, "up to the bound");
		// The cycle page reads larger files in the same view: held whole, read, never edited.
		auto large = suggestion_text_editor{ std::string(maximum_suggestion_editor_bytes + 10U, 'a') + "\nb" };
		expect(large.text().size() == maximum_suggestion_editor_bytes + 12U, "a larger text is held whole");
		expect(large.apply(move_carets{ text_motion::document_end }) && heads(large).front() == text_point{ 1U, 1U } && large.apply(select_word_at{ { 0U, 3U } }), "and its carets move");
		large.apply(place_caret{ { 1U, 1U } });
		expect(!large.apply(insert_text{ "x" }) && large.text().size() == maximum_suggestion_editor_bytes + 12U, "but an edit that keeps it over the bound is refused");
	}

	void moves_and_collapses()
	{
		auto editor = suggestion_text_editor{ "  int value = 1;\nx\n" };
		editor.apply(place_caret{ { 0U, 9U } });
		editor.apply(move_carets{ text_motion::line_start });
		expect(heads(editor).front() == text_point{ 0U, 2U }, "Home: first non-blank");
		editor.apply(move_carets{ text_motion::line_start });
		expect(heads(editor).front() == text_point{ 0U, 0U }, "Home again: column 0");
		editor.apply(move_carets{ text_motion::word_right });
		expect(heads(editor).front() == text_point{ 0U, 2U }, "word right over blanks");
		editor.apply(move_carets{ text_motion::word_right });
		expect(heads(editor).front() == text_point{ 0U, 6U }, "word right over a word");
		editor.apply(move_carets{ .Motion = text_motion::word_right, .Extend = true });
		editor.apply(move_carets{ text_motion::left });
		expect(editor.state().Carets[0] == text_caret{ { 0U, 6U }, { 0U, 6U } }, "left collapses a selection to its start");
		editor.apply(add_caret{ { 1U, 1U } });
		expect(editor.state().Carets.size() == 2U && editor.state().Primary == 1U, "Alt+click adds a primary caret");
		editor.apply(add_caret{ { 1U, 1U } });
		expect(editor.state().Carets.size() == 2U, "a caret at the same place merges");
	}
}

auto main() -> int
{
	keeps_exact_bytes();
	locates_byte_offsets();
	box_then_shift_end_stays_on_each_line();
	edits_in_virtual_space();
	drags_by_words_and_lines();
	copies_and_pastes_boxes();
	handles_tabs_breaks_and_deletion();
	indents_on_enter();
	chooses_the_indentation_unit();
	selects_words_and_lines();
	undoes_ordinary_steps();
	refuses_over_the_bound();
	moves_and_collapses();
	return EXIT_SUCCESS;
}
