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
		// Look prompts-v003 (the coordinator's proposal on the owner's note):
		// the prompt, not each condition, says Hide it or Show it disabled;
		// a disabled row gives the first unmet condition's reason.
		expect(migrated_when_unmet({ { .Fact = prompt_fact::path } }, { }) == prompt_when_unmet::hide && migrated_when_unmet({ { .Fact = prompt_fact::checks_failing } }, { }) == prompt_when_unmet::disable,
			   "a stored prompt migrates: any hiding lean hides, else disabled");
		auto facts          = sample_prompt_facts(prompt_place::agent_tab);
		facts.ChecksFailing = false;
		const auto failing  = prompt_condition{ .Fact = prompt_fact::checks_failing };
		const auto cppOnly  = prompt_condition{ .Fact = prompt_fact::path, .Patterns = { "*.md" } };
		auto       body     = prompt_macro_body{ .Name = "Why did it fail?", .Template = "Why did the check fail?", .Conditions = { failing } };
		auto       decided  = decide_prompt_conditions(body, facts);
		expect(!decided.Holds && !decided.Hides && decided.Reason == failing, "disabled, with the unmet condition's reason");
		body.Conditions = { cppOnly, failing };
		decided         = decide_prompt_conditions(body, facts);
		expect(!decided.Holds && !decided.Hides && decided.Reason == cppOnly, "the first unmet condition gives the reason, whatever it is about");
		body.WhenUnmet = prompt_when_unmet::hide;
		expect(decide_prompt_conditions(body, facts).Hides && !decide_prompt_conditions(body, facts).Reason, "Hide it hides");
		body.WhenUnmet = prompt_when_unmet::disable;
		body.Match     = prompt_match::any;
		decided        = decide_prompt_conditions(body, facts);
		expect(!decided.Holds && !decided.Hides && decided.Reason == cppOnly, "Any: the first condition's reason");
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

	// Look prompts-v001 (owner: "You should be able to nest conditionals"):
	// All, Any and None groups that nest, each leaf keeping its kind.
	void test_nested_conditions()
	{
		auto facts          = sample_prompt_facts(prompt_place::agent_tab);
		facts.ChecksFailing = false;
		facts.Draft         = true;
		const auto failing  = prompt_condition{ .Fact = prompt_fact::checks_failing };
		const auto draft    = prompt_condition{ .Fact = prompt_fact::draft };
		const auto markdown = prompt_condition{ .Fact = prompt_fact::path, .Patterns = { "*.md" } };
		// draft AND (failing OR markdown): the group is untrue.
		auto body    = prompt_macro_body{ .Name = "N", .Template = "N", .Conditions = { draft }, .Groups = { { .Match = prompt_match::any, .Conditions = { failing, markdown } } } };
		auto decided = decide_prompt_conditions(body, facts);
		expect(!decided.Holds && !decided.Hides && decided.Reason == failing, "an untrue Any group is disabled with its first condition's reason");
		facts.ChecksFailing = true;
		expect(decide_prompt_conditions(body, facts).Holds, "and holds when one of it holds");
		// NONE of (draft): untrue while a draft; the reason is draft turned.
		body             = prompt_macro_body{ .Name = "N", .Template = "N", .Groups = { { .Match = prompt_match::none, .Conditions = { draft } } } };
		decided          = decide_prompt_conditions(body, facts);
		auto notDraft    = draft;
		notDraft.Negated = true;
		expect(!decided.Holds && !decided.Hides && decided.Reason == notDraft, "None of a true condition: its reason turned");
		// Two levels down, the unmet condition still gives the reason.
		body = prompt_macro_body{ .Name = "N", .Template = "N", .Groups = { { .Match = prompt_match::all, .Groups = { { .Match = prompt_match::all, .Conditions = { markdown } } } } } };
		expect(decide_prompt_conditions(body, facts).Reason == markdown, "a condition two levels down gives the reason");
		// Validation bounds the tree.
		auto deep        = prompt_macro{ .Id = "own.deep", .Body = { .Name = "Deep", .Template = "x" } };
		deep.Body.Groups = { { .Groups = { { .Groups = { { .Conditions = { draft } } } } } } };
		expect(!prompt_macro_valid(deep), "three levels below the list are too deep");
		auto fine = prompt_macro{ .Id = "own.fine", .Body = { .Name = "Fine", .Template = "x", .Groups = { { .Match = prompt_match::none, .Groups = { { .Conditions = { draft } } } } } } };
		expect(prompt_macro_valid(fine), "two levels are fine");
		// A repository's layer carries the groups.
		const auto library                               = std::vector<prompt_macro>{ fine };
		const auto surfaces                              = prompt_surfaces{ };
		auto       edited                                = prompt_layered{ .Library = library, .Surfaces = surfaces };
		edited.Library.front().Body.Groups.front().Match = prompt_match::any;
		const auto layer                                 = repository_layer_of(library, surfaces, edited, "octo/render");
		expect(layer.Overrides.size() == 1U && layer.Overrides.front().Groups && apply_repository_layer(library, surfaces, layer) == edited, "a repository overrides the groups");
	}

	// [[BPR-2185]] (review N5): three-valued conditions, a stable leaf
	// index, bounded nesting, Hide at a wider scope, and the decisions the
	// page reads.
	void test_review_n5()
	{
		auto       facts = sample_prompt_facts(prompt_place::agent_tab);
		const auto draft = prompt_condition{ .Fact = prompt_fact::draft };
		facts.Draft.reset();
		auto body = prompt_macro_body{ .Name = "N", .Template = "N", .Groups = { { .Match = prompt_match::none, .Conditions = { draft } } } };
		expect(!decide_prompt_conditions(body, facts).Holds, "None of a condition whose fact is unknown does not hold: unknown satisfies neither polarity");
		facts.Draft = false;
		expect(decide_prompt_conditions(body, facts).Holds, "None holds once the fact is known untrue");
		facts.Draft = true;
		expect(!decide_prompt_conditions(body, facts).Holds, "and not once it is true");
		facts.Draft.reset();
		body.Groups = { { .Match = prompt_match::any, .Conditions = { draft, { .Fact = prompt_fact::file_open } } } };
		expect(decide_prompt_conditions(body, facts).Holds, "Any holds on one true child whatever the others are");
		body.Groups = { { .Match = prompt_match::all, .Conditions = { { .Fact = prompt_fact::file_open, .Negated = true }, draft } } };
		expect(!decide_prompt_conditions(body, facts).Holds, "All of a false child does not hold, unknowns or not");

		// The failing leaf's index is its place in the tree, Hide or not.
		facts.ChecksFailing  = false;
		auto       macro     = prompt_macro{ .Id = "own.leaf", .Body = { .Name = "L", .Template = "L", .Conditions = { { .Fact = prompt_fact::file_open } }, .Groups = { { .Conditions = { { .Fact = prompt_fact::checks_failing } } } } } };
		const auto shown     = evaluate_prompt_macro(macro, facts);
		macro.Body.WhenUnmet = prompt_when_unmet::hide;
		const auto hidden    = evaluate_prompt_macro(macro, facts);
		expect(shown.Unmet == prompt_unmet::condition && shown.Condition == 1U && hidden.Condition == 1U, "the unmet leaf is numbered the same whatever the prompt does while unmet");

		// Nested group count is bounded at each level.
		auto wide        = prompt_macro{ .Id = "own.wide", .Body = { .Name = "W", .Template = "x" } };
		wide.Body.Groups = { { .Groups = std::vector<prompt_condition_group>(maximum_prompt_conditions + 1U) } };
		expect(!prompt_macro_valid(wide), "a group holding five groups is too many");
		wide.Body.Groups.front().Groups.resize(maximum_prompt_conditions);
		expect(prompt_macro_valid(wide), "four are fine");

		// Hide at a wider scope: no thread matches, so the row is hidden.
		auto library         = shipped();
		auto open            = prompt_macro{ .Id = "own.still-open", .Place = prompt_place::conversation, .Body = { .Name = "Still open?", .Template = "Still open?", .Subject = prompt_subject::conversation } };
		open.Body.Conditions = { { .Fact = prompt_fact::thread_unresolved } };
		open.Body.WhenUnmet  = prompt_when_unmet::hide;
		library.push_back(open);
		auto surfaces = shipped_surfaces(library);
		static_cast<void>(place(surfaces, library, "own.still-open", prompt_surface::pull_request_menu));
		auto quick    = quick_ask_facts{ .Base = sample_prompt_facts(prompt_place::agent_tab) };
		quick.Threads = { { .Identity = "t1", .Path = "src/a.cpp", .Resolved = true }, { .Identity = "t2", .Path = "src/b.cpp", .Resolved = true } };
		auto menu     = decide_quick_asks(placed_prompt_macros(library, surfaces), quick, quick_ask_anchor::review_bar);
		expect(find_quick_ask(menu, "own.still-open", prompt_scope::pull_request) == nullptr && std::ranges::any_of(menu.Hidden, [](const prompt_withheld& aHidden) { return aHidden.Id == "own.still-open" && aHidden.Unmet == prompt_unmet::condition; }),
			   "Q1: a Hide prompt at a wider scope with no matching thread is hidden");
		library.back().Body.WhenUnmet = prompt_when_unmet::disable;
		menu                          = decide_quick_asks(placed_prompt_macros(library, surfaces), quick, quick_ask_anchor::review_bar);
		const auto* const greyed      = find_quick_ask(menu, "own.still-open", prompt_scope::pull_request);
		expect(greyed != nullptr && !greyed->Enabled && greyed->Reason == quick_ask_reason::no_threads_match, "Show it disabled lists it disabled");
		quick.Threads.back().Resolved = false;
		library.back().Body.WhenUnmet = prompt_when_unmet::hide;
		menu                          = decide_quick_asks(placed_prompt_macros(library, surfaces), quick, quick_ask_anchor::review_bar);
		const auto* const one         = find_quick_ask(menu, "own.still-open", prompt_scope::pull_request);
		expect(one != nullptr && one->Enabled && one->Each == std::vector<std::string>{ "t2" }, "and asks for the one that matches");

		// What the page reads.
		// Named: GCC 15 misreads a braced temporary inside a && chain as dangling (BPR-2215).
		const auto nested = prompt_macro_body{ .Groups = { { } } };
		expect(prompt_has_conditions(nested) && !prompt_has_conditions({ }), "a nested group is a condition");
		const auto activity = prompt_unmet_reading_of({ .Fact = prompt_fact::thread_activity, .Negated = true, .Activity = prompt_activity_value::pushes });
		expect(activity.Present && activity.Activity == prompt_activity_value::pushes, "an unmet \"is not\" thread activity says the activity is there");
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
	test_nested_conditions();
	test_review_n5();
	if (failures != 0) {
		return EXIT_FAILURE;
	}
	std::cout << "prompt surfaces core tests passed\n";
	return EXIT_SUCCESS;
}
