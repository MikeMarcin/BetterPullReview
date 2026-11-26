// BPR-2185 (the architecture review of BPR-2179): Settings › Prompts as a
// core reducer. The save names the revision its edits came from; a failed
// write keeps the edits and a stale one shows the other window's; a
// projection never replaces waiting edits; undo covers every edit and is the
// chosen layer's own; the library's bound holds; the full differences come
// from one function and read back as edited.

#include "bpr/core/prompt_editor.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	int failures = 0;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			++failures;
		}
	}

	[[nodiscard]] auto shipped_set() -> prompt_macro_set { return merge_prompt_macros(shipped_prompt_macros(), { }); }

	/// An editor that has read an empty store at revision 0.
	[[nodiscard]] auto loaded() -> prompt_editor
	{
		auto editor = make_prompt_editor(shipped_prompt_macros());
		static_cast<void>(adopt_prompt_projection(editor, shipped_set(), 0U, true, true));
		return editor;
	}

	[[nodiscard]] auto macro_of(const prompt_editor& aEditor, const std::string& aId) -> std::optional<prompt_macro>
	{
		const auto view  = prompt_edit_view(aEditor);
		const auto found = std::ranges::find(view.Library, aId, &prompt_macro::Id);
		return found == view.Library.end() ? std::nullopt : std::optional{ *found };
	}

	[[nodiscard]] auto on(const prompt_editor& aEditor, const std::string& aId) -> bool
	{
		const auto macro = macro_of(aEditor, aId);
		return macro && macro->On;
	}

	[[nodiscard]] auto core_surfaces_empty(const prompt_editor& aEditor, const std::string& aId) -> bool { return surfaces_of(prompt_edit_view(aEditor).Surfaces, aId).empty(); }

	constexpr auto breaks = std::string_view{ "agent-tab.what-could-break" };

	void test_differences_read_back()
	{
		auto editor = loaded();
		static_cast<void>(set_prompt_on(editor, std::string{ breaks }, false));
		static_cast<void>(add_prompt(editor, "own.mine", "Mine"));
		auto mine          = *macro_of(editor, "own.mine");
		mine.Body.Template = "Explain {file}";
		static_cast<void>(edit_prompt(editor, mine));
		static_cast<void>(place_prompt(editor, "own.mine", prompt_surface::file_chips));
		static_cast<void>(choose_prompt_repository(editor, "octo/render"));
		static_cast<void>(unplace_prompt(editor, "agent-tab.review-pull-request", prompt_surface::pull_request_chips));
		const auto differences = prompt_macro_differences_of(editor.Shipped, editor.Document);
		expect(differences.Repositories.size() == 1U && differences.Repositories.front().Repository == "octo/render", "review N4: the layers are part of the differences, whoever calls");
		// Read back, each place keeps its order (the library is listed place
		// by place), the surfaces and layers are as edited, and the
		// differences are the same again.
		const auto back = prompt_edit_document_of(merge_prompt_macros(editor.Shipped, differences));
		expect(back.Surfaces == editor.Document.Surfaces && back.Layers == editor.Document.Layers, "the differences read back as the surfaces and layers edited");
		expect(std::ranges::is_permutation(back.Library, editor.Document.Library) && prompt_macro_differences_of(editor.Shipped, back) == differences, "and as the library edited");
		expect(prompt_macro_differences_of(editor.Shipped, prompt_edit_document_of(shipped_set())) == prompt_macro_differences{ }, "the shipped set has no differences");
	}

	void test_a_failed_write_keeps_the_edit()
	{
		auto editor = loaded();
		static_cast<void>(set_prompt_on(editor, std::string{ breaks }, false));
		const auto request = next_prompt_save(editor);
		expect(request && request->From == 0U && prompt_edits_saving(editor), "a save is sent from the revision read");
		expect(!next_prompt_save(editor), "one write at a time");
		settle_prompt_save(editor, prompt_save_outcome::failed);
		// The host reads the store back: unchanged.
		expect(!adopt_prompt_projection(editor, shipped_set(), 0U, true, true) && !on(editor, std::string{ breaks }), "review N1(a): a failed write keeps the edit on the page");
		expect(editor.Notice == prompt_edit_notice::save_failed && editor.Held && !next_prompt_save(editor), "it says so, and is not sent again at once");
		static_cast<void>(set_prompt_on(editor, "agent-tab.review-pull-request", false));
		const auto retry = next_prompt_save(editor);
		expect(retry && retry->From == 0U && std::ranges::count_if(retry->Differences.Changes, [](const prompt_builtin_change& aChange) { return aChange.On == false; }) == 2,
			   "it saves with the next edit, both edits together");
		settle_prompt_save(editor, prompt_save_outcome::stored);
		expect(editor.Base == 1U && editor.Notice == prompt_edit_notice::none && !prompt_edits_saving(editor), "stored: the store is one revision on");
	}

	void test_a_projection_cannot_be_overwritten()
	{
		auto editor = loaded();
		static_cast<void>(set_prompt_on(editor, std::string{ breaks }, false));
		// Another window wrote revision 5 before this one's edit was sent.
		auto theirs = prompt_macro_differences{ };
		theirs.Own.push_back(prompt_macro{ .Id = "own.theirs", .Body = { .Name = "Theirs", .Template = "Theirs" } });
		const auto other = merge_prompt_macros(shipped_prompt_macros(), theirs);
		expect(!adopt_prompt_projection(editor, other, 5U, true, true) && !macro_of(editor, "own.theirs"), "a projection does not replace waiting edits");
		const auto request = next_prompt_save(editor);
		expect(request && request->From == 0U, "review N1(b): the save names the revision its set came from, not the newer one shown");
		settle_prompt_save(editor, prompt_save_outcome::stale);
		expect(editor.Notice == prompt_edit_notice::save_stale, "stale: the page says another window wrote first");
		expect(adopt_prompt_projection(editor, other, 5U, true, true) && macro_of(editor, "own.theirs").has_value() && on(editor, std::string{ breaks }) && editor.Base == 5U && !editor.Pending,
			   "and shows theirs");
		expect(editor.Undo.empty(), "the steps made over the replaced document are gone");
	}

	void test_projections_while_writing()
	{
		auto editor = loaded();
		static_cast<void>(set_prompt_on(editor, std::string{ breaks }, false));
		static_cast<void>(next_prompt_save(editor));
		static_cast<void>(set_prompt_on(editor, "agent-tab.review-pull-request", false));
		expect(!adopt_prompt_projection(editor, shipped_set(), 0U, true, true), "a write in flight settles before a projection is read");
		settle_prompt_save(editor, prompt_save_outcome::stored);
		const auto next = next_prompt_save(editor);
		expect(next && next->From == 1U, "an edit made during a write goes next, from the revision that write made");
	}

	void test_unreadable()
	{
		auto editor = make_prompt_editor(shipped_prompt_macros());
		static_cast<void>(adopt_prompt_projection(editor, shipped_set(), 0U, false, true));
		expect(editor.Notice == prompt_edit_notice::unreadable, "an unreadable store is said");
		static_cast<void>(adopt_prompt_projection(editor, shipped_set(), 0U, true, true));
		expect(editor.Notice == prompt_edit_notice::none, "and cleared once it reads");
	}

	void test_undo_covers_every_edit()
	{
		auto       editor = loaded();
		const auto id     = std::string{ breaks };
		const auto undoes = [&editor](const std::string_view aWhat, const auto aEdit) {
			const auto before = editor.Document;
			static_cast<void>(set_prompt_on(editor, "agent-tab.review-pull-request", !on(editor, "agent-tab.review-pull-request")));
			static_cast<void>(undo_prompt_edit(editor));
			expect(!editor.Redo.empty(), "a step to redo");
			const auto edited = aEdit();
			expect(edited.Changed, std::string{ aWhat } + " changes the document");
			expect(editor.Redo.empty(), std::string{ aWhat } + " clears Redo");
			const auto undone = undo_prompt_edit(editor);
			expect(undone.Changed && editor.Document == before, std::string{ aWhat } + " is undone");
			expect(redo_prompt_edit(editor).Changed && editor.Document != before, std::string{ aWhat } + " is redone");
			static_cast<void>(undo_prompt_edit(editor));
		};
		undoes("On", [&] { return set_prompt_on(editor, id, false); });
		undoes("a placement", [&] { return place_prompt(editor, id, prompt_surface::thread_chips); });
		undoes("a removed placement", [&] { return unplace_prompt(editor, "agent-tab.review-pull-request", prompt_surface::pull_request_chips); });
		undoes("a move", [&] { return move_prompt(editor, "conversation.summarize", prompt_surface::thread_menu, 1); });
		auto renamed      = *macro_of(editor, id);
		renamed.Body.Name = "What breaks?";
		static_cast<void>(edit_prompt(editor, renamed));
		undoes("Reset", [&] { return reset_prompt(editor, id); });
		undoes("Restore built-ins", [&] { return restore_built_in_prompts(editor); });
		static_cast<void>(choose_prompt_repository(editor, "octo/render"));
		auto here           = *macro_of(editor, id);
		here.Body.Template  = "In octo/render?";
		here.Body.Groups    = { { .Match = prompt_match::any, .Conditions = { { .Fact = prompt_fact::draft } } } };
		here.Body.WhenUnmet = prompt_when_unmet::hide;
		static_cast<void>(edit_prompt(editor, here));
		undoes("Use app-wide", [&] { return use_app_wide_prompt(editor, id); });
		undoes("Use app-wide conditions", [&] { return use_app_wide_field(editor, id, prompt_field::conditions); });
		const auto selected = undo_prompt_edit(editor);
		expect(selected.Prompt == id, "an undone edit names the prompt it was about");
	}

	void test_undo_is_the_layers_own()
	{
		auto editor = loaded();
		static_cast<void>(set_prompt_on(editor, std::string{ breaks }, false));
		expect(!editor.Undo.empty(), "an app-wide step");
		expect(choose_prompt_repository(editor, "octo/render") && editor.Undo.empty() && editor.Redo.empty(), "review N2: Prompts for another layer starts its own history");
		expect(undo_prompt_edit(editor).Refusal == prompt_edit_refusal::no_step && !on(editor, std::string{ breaks }), "so Undo cannot reach the app-wide step from a repository");
	}

	void test_typing_is_one_step()
	{
		auto editor = loaded();
		auto macro  = *macro_of(editor, std::string{ breaks });
		for (const auto* const text : { "W", "Wh", "Wha" }) {
			macro.Body.Name = text;
			static_cast<void>(edit_prompt(editor, macro, true));
		}
		expect(editor.Undo.size() == 1U, "a run of typing in one prompt is one step");
		end_prompt_typing(editor);
		macro.Body.Name = "What";
		static_cast<void>(edit_prompt(editor, macro, true));
		expect(editor.Undo.size() == 2U, "a pause ends the run");
	}

	void test_the_library_is_bounded()
	{
		auto editor = loaded();
		auto added  = 0;
		while (add_prompt(editor, "own." + std::to_string(added), "P").Changed) {
			++added;
		}
		const auto full = add_prompt(editor, "own.one-more", "P");
		expect(prompt_edit_view(editor).Library.size() == maximum_prompt_macros && full.Refusal == prompt_edit_refusal::library_full, "review N2: the library stops at its bound, and says why");
		expect(duplicate_prompt(editor, std::string{ breaks }, "own.copy", "Copy").Refusal == prompt_edit_refusal::library_full, "Duplicate too");
		const auto differences = prompt_macro_differences_of(editor.Shipped, editor.Document);
		expect(merge_prompt_macros(editor.Shipped, differences).Dropped.empty(), "so nothing saved is dropped when read back");
		expect(add_prompt(editor, "own.0", "Again").Refusal == prompt_edit_refusal::identity_taken && add_prompt(editor, "", "Blank").Refusal == prompt_edit_refusal::identity_taken,
			   "an identity is never reused or empty");
	}

	void test_identity_and_names()
	{
		auto editor = loaded();
		// A name cut at its bound keeps whole characters.
		auto name = std::string(119U, 'a') + "\xC3\xA9";
		static_cast<void>(duplicate_prompt(editor, std::string{ breaks }, "own.copy", name));
		const auto copy = macro_of(editor, "own.copy");
		expect(copy && copy->Body.Name == std::string(119U, 'a') && !copy->BuiltIn && core_surfaces_empty(editor, "own.copy"), "a duplicate's name is cut on a character boundary, on no surface");
		expect(delete_prompt(editor, std::string{ breaks }).Refusal == prompt_edit_refusal::built_in, "a built-in is not deleted");
		static_cast<void>(choose_prompt_repository(editor, "octo/render"));
		expect(delete_prompt(editor, "own.copy").Refusal == prompt_edit_refusal::inherited, "an app-wide prompt is not deleted in a repository");
	}

	void test_condition_room()
	{
		auto       editor  = loaded();
		const auto library = prompt_edit_view(editor).Library;
		const auto tour    = std::ranges::find_if(library, [](const prompt_macro& aMacro) { return aMacro.Place == prompt_place::tour_stop; });
		expect(tour != library.end(), "a tour prompt");
		const auto room = prompt_condition_room_of(*tour, { });
		expect(std::ranges::find(room.Facts, prompt_fact::file_open) == room.Facts.end() && std::ranges::find(room.Facts, prompt_fact::draft) != room.Facts.end(),
			   "review N3: a tour prompt is offered only the facts its chats hold");
		expect(add_prompt_condition(editor, tour->Id, { }, prompt_fact::file_open).Refusal == prompt_edit_refusal::fact_not_allowed, "and cannot be given another");
		const auto id = std::string{ "conversation.summarize" };
		static_cast<void>(add_prompt_condition_group(editor, id, { }));
		static_cast<void>(add_prompt_condition_group(editor, id, { 0U }));
		expect(!prompt_condition_room_of(*macro_of(editor, id), { 0U, 0U }).Group, "groups nest two levels below the list");
		expect(add_prompt_condition(editor, id, { 0U, 0U }, prompt_fact::draft).Changed && macro_of(editor, id)->Body.Groups.front().Groups.front().Conditions.size() == 1U, "a condition goes where its path says");
		expect(set_prompt_group_match(editor, id, { 0U }, prompt_match::none).Changed && macro_of(editor, id)->Body.Groups.front().Match == prompt_match::none, "a group's match is set");
		expect(remove_prompt_condition_group(editor, id, { }, 0U).Changed && macro_of(editor, id)->Body.Groups.empty(), "and a group removed");
	}

	void test_standing()
	{
		auto       editor = loaded();
		const auto id     = std::string{ breaks };
		static_cast<void>(choose_prompt_repository(editor, "octo/render"));
		auto here           = *macro_of(editor, id);
		here.Body.Groups    = { { .Match = prompt_match::any, .Conditions = { { .Fact = prompt_fact::draft } } } };
		here.Body.WhenUnmet = prompt_when_unmet::hide;
		static_cast<void>(edit_prompt(editor, here));
		const auto standing = prompt_standing_of(editor, *macro_of(editor, id));
		expect(standing.Differing == std::vector{ prompt_field::conditions } && standing.ChangedHere && standing.differs_here(), "review N3: groups and Hide it differ as the conditions");
		static_cast<void>(use_app_wide_field(editor, id, prompt_field::conditions));
		const auto back = *macro_of(editor, id);
		expect(back.Body.Groups.empty() && back.Body.WhenUnmet == prompt_when_unmet::disable && !prompt_standing_of(editor, back).differs_here(),
			   "Use app-wide conditions takes the groups and Hide it back too");
		expect(editor.Document.Layers.empty(), "and no layer is left");
		static_cast<void>(unplace_prompt(editor, "agent-tab.review-pull-request", prompt_surface::pull_request_chips));
		expect(prompt_surface_standing_of(editor, prompt_surface::pull_request_chips).Removed == std::vector<std::string>{ "agent-tab.review-pull-request" }, "a placement removed here");
		static_cast<void>(move_prompt(editor, "conversation.summarize", prompt_surface::thread_menu, 1));
		expect(prompt_surface_standing_of(editor, prompt_surface::thread_menu).Ordered, "an order of its own");
		expect(revert_prompt_order(editor, prompt_surface::thread_menu).Changed && !prompt_surface_standing_of(editor, prompt_surface::thread_menu).Ordered, "and its way back");
	}

	void test_placement_problems()
	{
		auto macro = prompt_macro{ .Id = "own.p", .Body = { .Name = "", .Template = "x" } };
		auto lists = prompt_surfaces{ };
		lists.Lists.at(static_cast<std::size_t>(prompt_surface::pull_request_chips)).emplace_back("own.p");
		const auto problems = prompt_placement_problems(macro, lists);
		expect(problems.size() == 1U && problems.front().Problem == prompt_problem::no_name && problems.front().Surface == prompt_surface::pull_request_chips, "a placed prompt's blocking problem, by surface");
	}
} // namespace

auto main() -> int
{
	test_differences_read_back();
	test_a_failed_write_keeps_the_edit();
	test_a_projection_cannot_be_overwritten();
	test_projections_while_writing();
	test_unreadable();
	test_undo_covers_every_edit();
	test_undo_is_the_layers_own();
	test_typing_is_one_step();
	test_the_library_is_bounded();
	test_identity_and_names();
	test_condition_room();
	test_standing();
	test_placement_problems();
	if (failures != 0) {
		return EXIT_FAILURE;
	}
	std::cout << "prompt-editor-tests:pass\n";
	return EXIT_SUCCESS;
}
