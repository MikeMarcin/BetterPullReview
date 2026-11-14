// BPR-2179 (docs/design/prompts-settings-v4/SPEC.md, the owner's answers on
// the card): surfaces and placements over the prompt library.

#include "bpr/core/prompt_macro.hpp"
#include "bpr/core/prompt_surfaces.hpp"

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
} // namespace

auto main() -> int
{
	test_shipped_surfaces();
	test_placing();
	if (failures != 0) {
		return EXIT_FAILURE;
	}
	std::cout << "prompt surfaces core tests passed\n";
	return EXIT_SUCCESS;
}
