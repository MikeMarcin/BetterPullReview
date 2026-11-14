// BPR-2179 (docs/design/prompts-settings-v4/SPEC.md, the owner's answers on
// the card): surfaces and placements over the prompt library.

#include "bpr/core/prompt_macro.hpp"
#include "bpr/core/prompt_surfaces.hpp"
#include "bpr/core/quick_ask.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
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

	[[nodiscard]] auto shipped() -> std::vector<prompt_macro>
	{
		auto macros = std::vector<prompt_macro>{ };
		for (const auto& builtin : shipped_prompt_macros()) {
			macros.push_back(builtin.Macro);
		}
		return macros;
	}

	[[nodiscard]] auto contains(const prompt_surfaces& aSurfaces, const prompt_surface aSurface, const std::string& aId) -> bool
	{
		const auto& list = aSurfaces.Lists.at(static_cast<std::size_t>(aSurface));
		return std::ranges::find(list, aId) != list.end();
	}

	void test_shipped_surfaces()
	{
		const auto macros   = shipped();
		const auto surfaces = shipped_surfaces(macros);
		expect(contains(surfaces, prompt_surface::pull_request_menu, "agent-tab.what-changed") && !contains(surfaces, prompt_surface::pull_request_chips, "agent-tab.what-changed"),
			   "an action row is on its menu, never a chip");
		expect(contains(surfaces, prompt_surface::pull_request_menu, "agent-tab.review-pull-request") && contains(surfaces, prompt_surface::pull_request_chips, "agent-tab.review-pull-request"),
			   "a quick ask chip is on both its menu and its chips");
		expect(contains(surfaces, prompt_surface::thread_chips, "conversation.was-addressed") == false && contains(surfaces, prompt_surface::thread_menu, "conversation.was-addressed"),
			   "Was this addressed? is menu only");
		expect(!surfaces.Lists.at(static_cast<std::size_t>(prompt_surface::tour_stop)).empty(), "the tour stop's chips are a surface");
		expect(shipped_surfaces(macros) == surfaces, "deterministic");
		expect(stable_name(prompt_surface::file_chips) == "file-chips", "stable names");
	}

	void test_placing()
	{
		const auto macros   = shipped();
		auto       surfaces = shipped_surfaces(macros);
		// The owner: action rows are menu only, never chips.
		expect(place(surfaces, macros, "agent-tab.explain-file", prompt_surface::file_chips) == placement_refusal::action_not_chip, "an action row refuses a chip surface");
		// About is dropped: a surface that cannot fill the prompt's
		// placeholders refuses it, with a reason.
		auto own = prompt_macro{ .Id = "own.1", .Body = { .Name = "Explain {file}", .Template = "Explain {file} lines {lines}" } };
		auto all = macros;
		all.push_back(own);
		expect(place(surfaces, all, "own.1", prompt_surface::pull_request_menu) == placement_refusal::placeholder_unfilled, "the pull request cannot fill {file}");
		expect(placement_reason(placement_refusal::placeholder_unfilled) == "The pull request can't fill this prompt's {file}, {lines} or similar.", "the reason in words");
		expect(place(surfaces, all, "own.1", prompt_surface::file_menu) == placement_refusal::none && contains(surfaces, prompt_surface::file_menu, "own.1"), "the file can");
		expect(surfaces.Lists.at(static_cast<std::size_t>(prompt_surface::file_menu)).back() == "own.1", "at the end");
		expect(place(surfaces, all, "own.1", prompt_surface::file_menu, 0U) == placement_refusal::none && surfaces.Lists.at(static_cast<std::size_t>(prompt_surface::file_menu)).front() == "own.1",
			   "placed again where it is: moved, never twice");
		expect(std::ranges::count(surfaces.Lists.at(static_cast<std::size_t>(prompt_surface::file_menu)), std::string{ "own.1" }) == 1, "once on a surface");
		expect(place(surfaces, all, "nope", prompt_surface::file_menu) == placement_refusal::unknown_prompt, "an unknown prompt");
		expect(move_placement(surfaces, "own.1", prompt_surface::file_menu, 1) == placement_refusal::none && surfaces.Lists.at(static_cast<std::size_t>(prompt_surface::file_menu))[1] == "own.1", "moved down one");
		expect(surfaces_of(surfaces, "own.1") == std::vector<prompt_surface>{ prompt_surface::file_menu }, "on one surface");
		expect(remove_placement(surfaces, "own.1", prompt_surface::file_menu) == placement_refusal::none && surfaces_of(surfaces, "own.1").empty(), "removed, the prompt kept");
		expect(remove_placement(surfaces, "own.1", prompt_surface::file_menu) == placement_refusal::not_placed, "not placed");
		// A thread prompt's conversation variable: the thread surfaces only.
		auto thread = prompt_macro{ .Id = "own.2", .Place = prompt_place::conversation, .Body = { .Name = "Summarize", .Template = "Summarize the thread on {conversation.file}", .Subject = prompt_subject::conversation } };
		all.push_back(thread);
		expect(place(surfaces, all, "own.2", prompt_surface::thread_chips) == placement_refusal::none, "a thread surface fills the conversation's file");
		expect(place(surfaces, all, "own.2", prompt_surface::file_menu) == placement_refusal::placeholder_unfilled, "a file surface does not");
	}

	[[nodiscard]] auto at(const prompt_surfaces& aSurfaces, const prompt_surface aSurface) -> const std::vector<std::string>& { return aSurfaces.Lists.at(static_cast<std::size_t>(aSurface)); }

	// Slice 2: a profile stores each surface it changed; the merge reads the
	// fields' surfaces with the changes applied.
	void test_stored_surfaces()
	{
		auto       library = shipped();
		const auto fields  = shipped_surfaces(library);
		expect(merge_prompt_surfaces(library, { }) == fields, "nothing stored: the fields' surfaces");
		expect(prompt_surface_changes_of(library, fields).empty(), "nothing changed: nothing stored");

		auto edited = fields;
		static_cast<void>(remove_placement(edited, "agent-tab.what-could-break", prompt_surface::pull_request_chips));
		static_cast<void>(move_placement(edited, "agent-tab.what-changed", prompt_surface::pull_request_menu, 5));
		const auto changes = prompt_surface_changes_of(library, edited);
		expect(changes.size() == 2U, "two surfaces changed");
		expect(merge_prompt_surfaces(library, changes) == edited, "the changes read back as edited");
		const auto chips = std::ranges::find(changes, prompt_surface::pull_request_chips, &prompt_surface_change::Surface);
		expect(chips != changes.end() && chips->Removed == std::vector<std::string>{ "agent-tab.what-could-break" }, "what was taken off is named");

		// A built-in a later release adds goes to the end of a changed
		// surface; a removed one stays removed.
		auto later = library;
		later.push_back(prompt_macro{ .Id = "agent-tab.new-in-later-release", .BuiltIn = true, .Body = { .Name = "New", .Template = "New" } });
		const auto merged = merge_prompt_surfaces(later, changes);
		expect(at(merged, prompt_surface::pull_request_chips).back() == "agent-tab.new-in-later-release", "a later built-in joins at the end");
		expect(std::ranges::find(at(merged, prompt_surface::pull_request_chips), std::string{ "agent-tab.what-could-break" }) == at(merged, prompt_surface::pull_request_chips).end(),
			   "a removed placement stays removed");

		// A placement naming a prompt the library no longer has is dropped.
		auto gone = changes;
		gone.front().Ids.push_back("own.deleted");
		const auto dropped = merge_prompt_surfaces(library, gone);
		expect(std::ranges::find(at(dropped, gone.front().Surface), std::string{ "own.deleted" }) == at(dropped, gone.front().Surface).end(), "an unknown prompt is not placed");

		// The merged set carries its surfaces.
		const auto set = merge_prompt_macros(shipped_prompt_macros(), { .Surfaces = changes });
		expect(set.Surfaces == edited, "the merged set's surfaces");
	}

	// Slice 3: the decisions read the surfaces.
	void test_decisions_read_surfaces()
	{
		auto library = shipped();
		// Six chips of the reviewer's own on the file chips, each always shown.
		auto surfaces = shipped_surfaces(library);
		for (int index = 0; index < 6; ++index) {
			const auto id = "own." + std::to_string(index);
			library.push_back(prompt_macro{ .Id = id, .Body = { .Name = "Ask " + std::to_string(index), .Template = "Ask about {file}" } });
			expect(place(surfaces, library, id, prompt_surface::file_chips) == placement_refusal::none, "placed on the file chips");
		}
		auto facts = sample_prompt_facts(prompt_place::agent_tab);
		facts.Lines.reset();
		const auto placed = placed_prompt_macros(library, surfaces);
		const auto offers = decide_prompt_offers(placed, facts);
		const auto count  = [&offers](const std::string_view aPrefix) {
			return std::ranges::count_if(offers.Chips, [aPrefix](const prompt_offer& aOffer) { return aOffer.Id.starts_with(aPrefix); });
		};
		expect(count("agent-tab.review-pull-request") == 1 && count("agent-tab.what-could-break") == 1, "the pull request's chips");
		expect(offers.Chips.size() == 7U, "each chip surface has its own five: two of the pull request's, five of the file's");
		expect(!offers.More.empty() && offers.More.back().Id == "own.5", "the file's sixth goes under More");
		expect(std::ranges::none_of(offers.Chips, [](const prompt_offer& aOffer) { return aOffer.Id == "agent-tab.what-changed"; }), "a menu placement is never a chip");

		// A prompt of the pull request's placed on the thread chips is asked
		// about the thread.
		static_cast<void>(place(surfaces, library, "agent-tab.what-could-break", prompt_surface::thread_chips));
		auto       thread = sample_prompt_facts(prompt_place::conversation);
		const auto on     = decide_prompt_offers(placed_prompt_macros(library, surfaces), thread);
		expect(std::ranges::any_of(on.Chips, [](const prompt_offer& aOffer) { return aOffer.Id == "agent-tab.what-could-break" && aOffer.Subject == prompt_subject::conversation; }),
			   "placement decides the scope");

		// The Ask menu lists in the surface's order, not action rows first.
		auto menu = shipped_surfaces(library);
		static_cast<void>(move_placement(menu, "agent-tab.what-changed", prompt_surface::pull_request_menu, 10));
		auto quick = quick_ask_facts{ .Base = sample_prompt_facts(prompt_place::agent_tab) };
		quick.File.reset();
		quick.Base.OpenFile.reset();
		const auto decided = decide_quick_asks(placed_prompt_macros(library, menu), quick, quick_ask_anchor::review_bar);
		expect(!decided.Sections.empty() && decided.Sections.front().Scope == prompt_scope::pull_request, "the pull request's section");
		expect(!decided.Sections.empty() && decided.Sections.front().Rows.back().Id == "agent-tab.what-changed", "the surface's order");
		expect(!decided.Sections.empty() && decided.Sections.front().Rows.front().Id == "agent-tab.review-pull-request", "first as placed");
		// Taken off a menu: not listed.
		static_cast<void>(remove_placement(menu, "agent-tab.what-could-break", prompt_surface::pull_request_menu));
		const auto without = decide_quick_asks(placed_prompt_macros(library, menu), quick, quick_ask_anchor::review_bar);
		expect(!without.Sections.empty() && std::ranges::none_of(without.Sections.front().Rows, [](const quick_ask_row& aRow) { return aRow.Id == "agent-tab.what-could-break"; }), "off the menu");
		// Placed on the pull request's menu and asked there.
		auto ownMenu = prompt_macro{ .Id = "own.menu", .Place = prompt_place::conversation, .Body = { .Name = "Risky?", .Template = "Is this risky?", .Subject = prompt_subject::conversation } };
		library.push_back(ownMenu);
		expect(place(menu, library, "own.menu", prompt_surface::pull_request_menu) == placement_refusal::none, "placed on the pull request's menu");
		const auto with = decide_quick_asks(placed_prompt_macros(library, menu), quick, quick_ask_anchor::review_bar);
		expect(!with.Sections.empty() && std::ranges::any_of(with.Sections.front().Rows, [](const quick_ask_row& aRow) { return aRow.Id == "own.menu" && aRow.Each.empty(); }), "asked once, about the pull request");
		// A press on a menu-only prompt still has something to send.
		auto chipless = shipped_surfaces(library);
		static_cast<void>(remove_placement(chipless, "agent-tab.what-could-break", prompt_surface::pull_request_chips));
		const auto pressed = menu_prompt_offers(placed_prompt_macros(library, chipless), sample_prompt_facts(prompt_place::agent_tab));
		expect(std::ranges::any_of(pressed, [](const prompt_offer& aOffer) { return aOffer.Id == "agent-tab.what-could-break"; }), "a menu-only prompt expands at a press");
		// What changed since my review asks about the pull request: a file's
		// menu refuses it.
		expect(placement_refusal_of(library.at(5), prompt_surface::file_menu) == placement_refusal::action_too_narrow || library.at(5).Id != "agent-tab.what-changed", "an action keeps its scope");
	}

	// Slice 5: condition kinds, and All or Any (RFC-BPR-0128 Q1, the spec's
	// section 7 and the coordinator's default for Any).
	void test_condition_kinds()
	{
		expect(prompt_fact_kind(prompt_fact::path) == prompt_condition_kind::hides && prompt_fact_kind(prompt_fact::checks_failing) == prompt_condition_kind::greys
				   && prompt_fact_kind(prompt_fact::thread_unresolved) == prompt_condition_kind::greys,
			   "each fact has its kind");
		auto facts          = sample_prompt_facts(prompt_place::agent_tab);
		facts.ChecksFailing = false;
		const auto failing  = prompt_condition{ .Fact = prompt_fact::checks_failing };
		const auto cppOnly  = prompt_condition{ .Fact = prompt_fact::path, .Patterns = { "*.md" } };
		auto       body     = prompt_macro_body{ .Name = "Why did it fail?", .Template = "Why did the check fail?", .Conditions = { failing } };
		auto       decided  = decide_prompt_conditions(body, facts);
		expect(!decided.Holds && !decided.Hides && decided.Reason == 0U, "All: a greys condition untrue greys");
		body.Conditions = { failing, cppOnly };
		decided         = decide_prompt_conditions(body, facts);
		expect(!decided.Holds && decided.Hides, "All: a hides condition untrue hides");
		body.Match = prompt_match::any;
		decided    = decide_prompt_conditions(body, facts);
		expect(!decided.Holds && !decided.Hides && decided.Reason == 0U, "Any: hidden only when every condition hides");
		body.Conditions = { cppOnly };
		expect(decide_prompt_conditions(body, facts).Hides, "Any of hides alone: hidden");
		facts.ChecksFailing = true;
		body.Conditions     = { cppOnly, failing };
		expect(decide_prompt_conditions(body, facts).Holds, "Any: one holding is enough");
		auto macro = prompt_macro{ .Id = "own.any", .Body = body };
		expect(evaluate_prompt_macro(macro, facts).Offer.has_value(), "an Any prompt is offered when one holds");

		// An Ask menu greys the row with its condition's reason.
		auto library = shipped();
		auto greyed  = prompt_macro{ .Id = "own.check", .Body = { .Name = "Why did it fail?", .Template = "Why did the check fail?", .Conditions = { failing } } };
		library.push_back(greyed);
		auto surfaces = shipped_surfaces(library);
		static_cast<void>(place(surfaces, library, "own.check", prompt_surface::pull_request_menu));
		auto quick               = quick_ask_facts{ .Base = sample_prompt_facts(prompt_place::agent_tab) };
		quick.Base.ChecksFailing = false;
		const auto menu          = decide_quick_asks(placed_prompt_macros(library, surfaces), quick, quick_ask_anchor::review_bar);
		const auto row           = find_quick_ask(menu, "own.check", prompt_scope::pull_request);
		expect(row != nullptr && !row->Enabled && row->Reason == quick_ask_reason::condition && row->Condition == failing && row->Label == "Why did it fail?", "greyed, with its reason");

		// The newer facts: a thread that is resolved.
		auto thread = prompt_macro{ .Id = "own.open", .Body = { .Name = "Still open?", .Template = "Is it settled?", .Conditions = { { .Fact = prompt_fact::thread_unresolved } } } };
		library.push_back(thread);
		static_cast<void>(place(surfaces, library, "own.open", prompt_surface::thread_menu));
		quick.Thread        = "t1";
		quick.Threads       = { { .Identity = "t1", .Path = "src/a.cpp", .Resolved = true } };
		const auto onThread = decide_quick_asks(placed_prompt_macros(library, surfaces), quick, quick_ask_anchor::thread);
		const auto resolved = find_quick_ask(onThread, "own.open", prompt_scope::thread);
		expect(resolved != nullptr && !resolved->Enabled, "a resolved thread greys Thread is unresolved");
		quick.Threads.front().Resolved = false;
		const auto openMenu            = decide_quick_asks(placed_prompt_macros(library, surfaces), quick, quick_ask_anchor::thread);
		const auto open                = find_quick_ask(openMenu, "own.open", prompt_scope::thread);
		expect(open != nullptr && open->Enabled, "an open one enables it");
	}

	// Slice 7: a repository's layer over the app-wide prompts and surfaces.
	void test_repository_layer()
	{
		const auto library  = shipped();
		const auto surfaces = shipped_surfaces(library);
		auto       edited   = prompt_layered{ .Library = library, .Surfaces = surfaces };
		// Off here, a changed text, one of its own, one added, one removed,
		// and the thread menu reordered.
		auto found           = std::ranges::find(edited.Library, std::string{ "agent-tab.what-could-break" }, &prompt_macro::Id);
		found->On            = false;
		found->Body.Template = "What could break in this repository?";
		edited.Library.push_back(prompt_macro{ .Id = "own.only-here", .Body = { .Name = "Only here", .Template = "Ask here" } });
		static_cast<void>(place(edited.Surfaces, edited.Library, "own.only-here", prompt_surface::pull_request_chips));
		static_cast<void>(remove_placement(edited.Surfaces, "agent-tab.review-pull-request", prompt_surface::pull_request_chips));
		static_cast<void>(move_placement(edited.Surfaces, "conversation.summarize", prompt_surface::thread_menu, 2));
		const auto layer = repository_layer_of(library, surfaces, edited, "octo/render");
		expect(layer.Own.size() == 1U && layer.Overrides.size() == 1U && layer.Overrides.front().On == false && layer.Overrides.front().Template && !layer.Overrides.front().Name,
			   "each field is overridden on its own");
		expect(apply_repository_layer(library, surfaces, layer) == edited, "the layer reads back as edited");
		const auto chips = std::ranges::find(layer.Surfaces, prompt_surface::pull_request_chips, &prompt_repository_surface::Surface);
		expect(chips != layer.Surfaces.end() && chips->Added == std::vector<std::string>{ "own.only-here" } && chips->Removed == std::vector<std::string>{ "agent-tab.review-pull-request" } && !chips->Order,
			   "added and removed here, no order of its own");
		const auto thread = std::ranges::find(layer.Surfaces, prompt_surface::thread_menu, &prompt_repository_surface::Surface);
		expect(thread != layer.Surfaces.end() && thread->Order.has_value(), "a reordered surface keeps its own order");
		// A field the repository left follows later app-wide edits; a
		// placement added app-wide later goes to the end of its reordered list.
		auto later         = library;
		auto changed       = std::ranges::find(later, std::string{ "agent-tab.what-could-break" }, &prompt_macro::Id);
		changed->Body.Name = "What could break?";
		auto laterSurfaces = surfaces;
		later.push_back(prompt_macro{ .Id = "own.app-wide", .Place = prompt_place::conversation, .Body = { .Name = "Later", .Template = "Later", .Subject = prompt_subject::conversation } });
		static_cast<void>(place(laterSurfaces, later, "own.app-wide", prompt_surface::thread_menu, 0U));
		const auto applied = apply_repository_layer(later, laterSurfaces, layer);
		const auto shown   = std::ranges::find(applied.Library, std::string{ "agent-tab.what-could-break" }, &prompt_macro::Id);
		expect(shown->Body.Name == "What could break?" && shown->Body.Template == "What could break in this repository?", "an unchanged field follows the app-wide edit");
		expect(applied.Surfaces.Lists.at(static_cast<std::size_t>(prompt_surface::thread_menu)).back() == "own.app-wide", "a later app-wide placement goes to the end here");
		// A set as the repository's pull requests see it.
		const auto set = merge_prompt_macros(shipped_prompt_macros(), { .Repositories = { layer } });
		expect(prompt_set_for_repository(set, "Octo/Render").Surfaces == edited.Surfaces && prompt_set_for_repository(set, "other/repo") == set, "only its own repository");
	}
} // namespace

auto main() -> int
{
	test_shipped_surfaces();
	test_placing();
	test_stored_surfaces();
	test_decisions_read_surfaces();
	test_condition_kinds();
	test_repository_layer();
	if (failures != 0) {
		return EXIT_FAILURE;
	}
	std::cout << "prompt surfaces core tests passed\n";
	return EXIT_SUCCESS;
}
