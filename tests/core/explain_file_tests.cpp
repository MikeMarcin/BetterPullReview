/// \file
/// \brief RFC-BPR-0117 ([[BPR-2076]]). Explain this file: its subject rule (the
/// open file, no selection), the material its one call carries and that
/// material's bounds, and the note Keep as note makes. No Qt.

#include "bpr/core/agent_chat_scope.hpp"
#include "bpr/core/agent_context_supplement.hpp"
#include "bpr/core/agent_question.hpp"
#include "bpr/core/explain_file.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto commit(const char aDigit) -> std::string { return std::string(40U, aDigit); }

	/// AC01. Explain this file is about that file through
	/// `analyze_current_file`, which no longer needs a selection.
	void the_open_file_is_the_subject()
	{
		expect(!core::acts_on_selection(core::agent_entry_point::analyze_current_file), "analyze_current_file acts on the open file, not the selection");
		expect(core::acts_on_selection(core::agent_entry_point::ask_about_selection), "asking about the selection still needs one");
		const auto plain = core::decide_agent_question({ .Explain = "src/render.cpp" });
		expect(plain && plain->Subject == core::agent_question_subject::file && plain->EntryPoint == core::agent_entry_point::analyze_current_file && plain->File == "src/render.cpp",
			   "the press names the file and the entry point, with nothing selected");
		const auto busy = core::decide_agent_question({ .LinesAttached = true, .LinesSelected = true, .ConversationAttached = true, .OpenFile = "src/other.cpp", .Explain = "src/render.cpp" });
		expect(busy == plain, "attached lines, a conversation chip or another open file do not change it");
		expect(core::decide_agent_question({ .OpenFile = "src/render.cpp" })->EntryPoint != core::agent_entry_point::analyze_current_file, "a typed question is not an explanation");
	}

	/// The material is bounded; the text says what the head holds.
	void the_material_keeps_its_bounds()
	{
		const auto input = core::explain_material_input{ .Path = "src/render.cpp", .BaseCommit = commit('a'), .HeadCommit = commit('b'), .Change = "--- a/src/render.cpp\n+++ b/src/render.cpp\n", .Head = std::string{ "int x;\n" } };
		const auto whole = core::bound_explain_material(input);
		expect(whole && whole->ContentState == core::explain_content_state::whole && whole->Content == "int x;\n" && !whole->ChangeCut && core::valid(*whole), "a small file is given whole");

		auto big   = input;
		auto lines = std::string{ };
		while (lines.size() <= core::maximum_explain_content_bytes) {
			lines += "a line of the file\n";
		}
		big.Head           = lines;
		big.Change         = lines;
		const auto bigFile = core::bound_explain_material(big);
		expect(bigFile && bigFile->ContentState == core::explain_content_state::cut && bigFile->Content.size() <= core::maximum_explain_content_bytes && bigFile->Content.back() == '\n', "a long file is cut at a line end");
		expect(!bigFile->ChangeCut && core::valid(*bigFile), "a change within its bound is kept");
		big.Change += std::string(core::maximum_explain_change_bytes, 'x');
		const auto bigChange = core::bound_explain_material(big);
		expect(bigChange && bigChange->ChangeCut && bigChange->Change.size() <= core::maximum_explain_change_bytes && core::valid(*bigChange), "a long change is cut at a line end");

		auto gone = input;
		gone.Head.reset();
		expect(core::bound_explain_material(gone)->ContentState == core::explain_content_state::absent, "a deleted file has no text at the head");
		auto binary = input;
		binary.Head = std::string{ "a\0b", 3U };
		expect(core::bound_explain_material(binary)->ContentState == core::explain_content_state::binary && core::bound_explain_material(binary)->Content.empty(), "a binary file's bytes are not given");
		auto large     = input;
		large.TooLarge = true;
		expect(core::bound_explain_material(large)->ContentState == core::explain_content_state::too_large, "a file the read refused is said too large");

		auto noPath = input;
		noPath.Path.clear();
		expect(!core::bound_explain_material(noPath), "no path, no material");
		auto shortCommit       = input;
		shortCommit.BaseCommit = "abc";
		expect(!core::bound_explain_material(shortCommit), "both commits are named in full");

		auto forged    = *whole;
		forged.Content = std::string(core::maximum_explain_content_bytes + 1U, 'x');
		expect(!core::valid(forged), "a supplement past its bound is not valid");
	}

	/// AC03. Keep as note: the agent's origin first, then the answer, within
	/// the note's bound.
	void keep_as_note_names_its_origin()
	{
		const auto note = core::explain_note_text("Claude", "It renders.\n\n", core::maximum_note_bytes);
		expect(note == "Agent: Claude explained this file.\n\nIt renders.", "the note says it is the agent's explanation");
		expect(core::explain_note_text("Claude", " \n", core::maximum_note_bytes).empty(), "no answer, no note");
		auto long_answer = std::string{ };
		while (long_answer.size() <= core::maximum_note_bytes) {
			long_answer += "One more line of the explanation.\n";
		}
		const auto cut = core::explain_note_text("Codex", long_answer, core::maximum_note_bytes);
		expect(cut.size() <= core::maximum_note_bytes && cut.starts_with("Agent: Codex explained this file.") && cut.ends_with("\n…"), "a long answer is cut and says so");
	}
} // namespace

auto main() -> int
{
	the_open_file_is_the_subject();
	the_material_keeps_its_bounds();
	keep_as_note_names_its_origin();
	std::cout << "explain file core tests passed\n";
	return EXIT_SUCCESS;
}
