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

	void edits_in_virtual_space()
	{
		auto editor = suggestion_text_editor{ "long line here\nab\n" };
		editor.apply(place_caret{ { 0U, 10U } });
		editor.apply(move_carets{ text_motion::down });
		expect(heads(editor).front() == text_point{ 1U, 10U }, "down keeps the column beyond a short line");
		editor.apply(move_carets{ text_motion::right });
		expect(heads(editor).front() == text_point{ 1U, 11U }, "right in virtual space does not wrap");
		const auto untouched = editor.text();
		editor.apply(delete_backward{ });
		expect(editor.text() == untouched && heads(editor).front() == text_point{ 1U, 10U }, "backspace in virtual space only moves");
		editor.apply(insert_text{ "y" });
		expect(editor.text() == "long line here\nab        y\n", "virtual space becomes spaces only on insertion");
		// A zero-width box across short and long lines, then typing.
		auto box = suggestion_text_editor{ "abcdef\nab\nabcd\n" };
		box.apply(place_caret{ { 0U, 4U } });
		box.apply(extend_box{ text_motion::down });
		box.apply(extend_box{ text_motion::down });
		box.apply(insert_text{ "|" });
		expect(box.text() == "abcd|ef\nab  |\nabcd|\n", "box insertion pads short lines");
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
		expect(editor.text() == "\tx\r\n\t\t\r\n\ty\r\n", "a line break takes CRLF");
		editor.apply(delete_backward{ });
		expect(editor.text() == "\tx\r\n\t\t\ty\r\n", "backspace at a line start removes the whole CRLF");
		editor.apply(place_caret{ { 0U, 5U } });
		editor.apply(delete_forward{ });
		expect(editor.text() == "\tx\t\t\ty\r\n", "delete at a line end joins the next line");
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
	copies_and_pastes_boxes();
	handles_tabs_breaks_and_deletion();
	undoes_ordinary_steps();
	refuses_over_the_bound();
	moves_and_collapses();
	return EXIT_SUCCESS;
}
