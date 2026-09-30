#include "bpr/core/review_cycle_tabs.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const char* aWhat)
	{
		if (!aValue) {
			std::cerr << "review cycle tabs failure: " << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto cycle() -> review_cycle_state
	{
		auto incarnation     = std::array<std::uint8_t, 32>{ };
		incarnation[0]       = 1;
		const auto authority = local_cycle_authority{ .Profile = bpr_profile_id{ 1 }, .Incarnation = cycle_catalog_incarnation{ incarnation } };
		auto       state     = create({ .Authority = authority }, { .Authority = authority, .Title = "Tabs", .Scope = { } })->Cycle;
		for (std::uint8_t material = 1; material <= 2; ++material) {
			auto digest = std::array<std::uint8_t, 32>{ };
			digest[0]   = material;
			state       = apply(state, { .Subject = state.Subject, .ExpectedRevision = state.Revision, .Change = record_cycle_capture{ .Capture = { .ScopeVersion = 1, .Material = cycle_material_digest{ digest }, .Entries = 2, .Coverage = cycle_capture_coverage::complete } } }).Accepted;
		}
		return state;
	}

	void opens_switches_and_restores_tabs()
	{
		const auto source = cycle();
		const auto paths  = std::vector<std::string>{ "src/a.cpp", "src/b.cpp", "docs/c.md" };
		auto       tabs   = cycle_tabs_state{ };
		const auto file   = [&](const std::string& aPath) { return cycle_file_identity(source.Subject, aPath); };
		expect(transition(tabs, source.Subject, open_in_active_tab_intent{ file("src/a.cpp") }, paths).Reason == document_tab_reason::no_window_comparison, "no revision, no tab");
		tabs.Revision = cycle_source_revision_id{ 1 };
		expect(transition(tabs, source.Subject, open_in_active_tab_intent{ file("src/a.cpp") }, paths).Changed && tabs.Tabs.size() == 1U, "first tab");
		expect(transition(tabs, source.Subject, open_in_new_tab_intent{ file("src/b.cpp") }, paths).Changed && tabs.Tabs.size() == 2U && active_tab(tabs)->View.Path == "src/a.cpp", "background tab");
		expect(transition(tabs, source.Subject, open_in_active_tab_intent{ file("missing.cpp") }, paths).Reason == document_tab_reason::invalid_file, "unknown file");
		expect(cycle_file_identity(source.Subject, "src/a.cpp") != cycle_file_identity(source.Subject, "src/b.cpp"), "distinct identities");

		// A newer capture does not move a tab: only a gesture changes what the tab shows.
		tabs.Revision = cycle_source_revision_id{ 2 };
		expect(active_tab(tabs)->View.Revision == cycle_source_revision_id{ 1 }, "tabs keep their revision");
		expect(observe_line(tabs, *tabs.Active, 40).Changed, "reading line");
		expect(transition(tabs, source.Subject, activate_adjacent_tab_intent{ change_navigation_direction::next }, paths).Changed && active_tab(tabs)->View.Path == "src/b.cpp", "next tab");
		expect(transition(tabs, source.Subject, duplicate_tab_intent{ *tabs.Active }, paths).Changed && tabs.Tabs.size() == 3U, "duplicate");
		expect(transition(tabs, source.Subject, select_tab_comparison_intent{ }, paths).Reason == document_tab_reason::invalid_comparison, "no comparison gestures on full source");

		// Persist and restore: exact views, reading lines and the active tab.
		const auto record   = persisted(tabs);
		auto       restored = cycle_tabs_state{ };
		expect(restore(restored, source, record).Changed, "restored");
		expect(persisted(restored) == record && restored.Tabs[0].View.Line == 40U, "exact views after restore");
		auto future              = record;
		future.Views[0].Revision = cycle_source_revision_id{ 9 };
		expect(restore(restored, source, future).Reason == document_tab_reason::invalid_restoration && persisted(restored) == record, "a revision the cycle never had is refused whole");

		expect(transition(tabs, source.Subject, close_tab_intent{ *tabs.Active }, paths).Changed && tabs.Tabs.size() == 2U && tabs.Active, "close keeps a neighbour active");
		expect(transition(tabs, source.Subject, close_other_tabs_intent{ tabs.Tabs[0].Id }, paths).Changed && tabs.Tabs.size() == 1U, "close others");
		expect(open_path(tabs, "docs/c.md", false).Changed && active_tab(tabs)->View.Path == "docs/c.md" && active_tab(tabs)->View.Line == 0U, "open a path in the active tab");
	}
}

auto main() -> int
{
	opens_switches_and_restores_tabs();
	return EXIT_SUCCESS;
}
