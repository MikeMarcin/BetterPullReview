#include "bpr/core/profile_data_reset.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;
	using reset  = profile_data_reset;
	using area   = profile_data_area;
	using reason = release_maintenance_reason;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	constexpr std::array resets{ reset::caches, reset::settings, reset::review_data, reset::connections, reset::everything, reset::agent_sessions, reset::saved_sources, reset::statistics, reset::agent_setup, reset::agent_workspaces };

	void test_store_areas()
	{
		expect(profile_data_store_area("epoch-catalog") == area::caches, "the comparison history catalog is a cache");
		expect(profile_data_store_area("markdown-images") == area::caches, "body images are a cache");
		expect(profile_data_store_area("review-overviews") == area::caches, "Home's summaries are a cache");
		for (const auto store : { "appearance-settings", "workbench-settings", "diagnostic-preferences", "agent-setup", "window-restoration" }) {
			expect(profile_data_store_area(store) == area::settings, "settings stores are settings");
		}
		for (const auto store : { "review-ledger", "review-tours", "review-cycles", "review-mutation-recovery", "local-drafts" }) {
			expect(profile_data_store_area(store) == area::review_data, "review stores are review data");
		}
		expect(profile_data_store_area("insights") == area::statistics, "Insights has its own lifetime statistics area");
		expect(profile_data_store_area("review-queue") == area::saved_sources, "saved repositories and rules are their own area");
		expect(profile_data_store_area("device-bindings") == area::connections, "device bindings are connections [[BPR-1858]]");
		for (const auto store : { "remembered-connections", "profile-registry", "release-update", "update-draft-recovery", "a-store-added-later", "", "review-ledger.sqlite3" }) {
			expect(profile_data_store_area(store) == area::kept, "sign-in, shared and unknown stores are kept");
		}
		for (const auto folder : { "repositories", "incoming", "locks", "quarantine", "mcp-snapshot" }) {
			expect(profile_data_folder_area(folder) == area::caches, "cache folders are caches");
		}
		expect(profile_data_folder_area("publication-git-v1") == area::review_data, "the publication store is review data");
		expect(profile_data_folder_area("local-draft-git-v1") == area::review_data, "RFC-BPR-0108: the local draft reader's store is review data");
		for (const auto folder : { "logs", "state", "profiles", "updates", "agent-workspace-ownership-v1", "home", "templates", "hooks", "work" }) {
			expect(profile_data_folder_area(folder) == area::kept, "logs, shared folders and Git isolation are kept");
		}
	}

	void test_scopes()
	{
		expect(profile_data_reset_removes(reset::caches, area::caches), "Clear caches removes caches");
		expect(!profile_data_reset_removes(reset::caches, area::settings) && !profile_data_reset_removes(reset::caches, area::review_data)
				   && !profile_data_reset_removes(reset::caches, area::saved_sources),
			   "Clear caches removes nothing else");
		expect(profile_data_reset_removes(reset::settings, area::settings), "Reset settings removes settings");
		expect(!profile_data_reset_removes(reset::settings, area::saved_sources), "Reset settings keeps saved repositories and observer rules");
		expect(!profile_data_reset_removes(reset::settings, area::caches) && !profile_data_reset_removes(reset::settings, area::review_data),
			   "Reset settings removes nothing else");
		expect(profile_data_reset_removes(reset::review_data, area::review_data), "Reset review data removes review data");
		expect(!profile_data_reset_removes(reset::review_data, area::caches) && !profile_data_reset_removes(reset::review_data, area::settings)
				   && !profile_data_reset_removes(reset::review_data, area::saved_sources),
			   "Reset review data removes nothing else");
		for (const auto narrower : { reset::caches, reset::settings, reset::review_data }) {
			expect(!profile_data_reset_removes(narrower, area::statistics), "narrower resets keep Insights");
		}
		for (const auto narrower : { reset::caches, reset::settings, reset::review_data }) {
			expect(!profile_data_reset_removes(narrower, area::connections) && !profile_data_reset_signs_out(narrower), "narrower resets keep sign-in");
		}
		expect(profile_data_reset_removes(reset::connections, area::connections) && profile_data_reset_signs_out(reset::connections), "Reset connections signs out");
		for (const auto kept : { area::caches, area::settings, area::review_data, area::saved_sources, area::statistics }) {
			expect(!profile_data_reset_removes(reset::connections, kept), "Reset connections keeps every other area");
		}
		expect(profile_data_reset_signs_out(reset::everything), "Reset everything signs out");
		for (const auto removed : { area::caches, area::settings, area::review_data, area::saved_sources, area::statistics, area::connections }) {
			expect(profile_data_reset_removes(reset::everything, removed), "Reset everything removes every resettable area");
		}
		for (const auto each : resets) {
			expect(!profile_data_reset_removes(each, area::kept), "no reset removes what is kept");
		}
		expect(!profile_data_reset_removes(static_cast<reset>(0), area::caches) && !profile_data_reset_removes(static_cast<reset>(11), area::caches),
			   "an unknown reset removes nothing");
		expect(profile_data_reset_restores_queue_display(reset::settings), "Reset settings restores the queue display choices");
		for (const auto each : { reset::caches, reset::review_data, reset::connections, reset::everything }) {
			expect(!profile_data_reset_restores_queue_display(each), "only Reset settings restores queue display in place");
		}
	}

	void test_admission()
	{
		expect(admit_profile_data_reset({ reset::caches }) == reason::none, "Clear caches needs no acknowledgement");
		expect(admit_profile_data_reset({ reset::settings }) == reason::none, "Reset settings needs no acknowledgement: agent conversations are apart [[BPR-NNNN]]");
		expect(admit_profile_data_reset({ reset::review_data }) == reason::invalid_selection, "Reset review data needs the acknowledgement");
		expect(admit_profile_data_reset({ reset::everything, false, true }) == reason::invalid_selection, "a copy does not stand in for the acknowledgement");
		expect(admit_profile_data_reset({ reset::review_data, true }) == reason::none, "acknowledged review data reset is admitted");
		expect(admit_profile_data_reset({ reset::everything, true, true }) == reason::none, "acknowledged reset with a copy is admitted");
		expect(admit_profile_data_reset({ reset::caches, false, true }) == reason::invalid_selection, "Clear caches offers no copy");
		expect(admit_profile_data_reset({ static_cast<reset>(0), true }) == reason::invalid_selection, "an unknown reset is refused");
		expect(admit_profile_data_reset({ reset::connections }) == reason::none, "Reset connections needs no acknowledgement");
		expect(admit_profile_data_reset({ reset::connections, true, true }) == reason::invalid_selection, "Reset connections offers no copy");
		expect(profile_data_reset_destructive(reset::review_data) && profile_data_reset_destructive(reset::everything)
				   && !profile_data_reset_destructive(reset::caches) && !profile_data_reset_destructive(reset::settings) && !profile_data_reset_destructive(reset::connections),
			   "review data and everything are destructive; settings, caches and connections are not");
	}

	void test_names()
	{
		for (const auto each : resets) {
			expect(profile_data_reset_from_name(profile_data_reset_name(each)) == each, "each name round-trips");
		}
		expect(profile_data_reset_name(reset::review_data) == "review-data", "the command line names review data with a hyphen");
		expect(profile_data_reset_name(reset::connections) == "connections", "the command line names Reset connections");
		expect(!profile_data_reset_from_name("review_data") && !profile_data_reset_from_name("Caches") && !profile_data_reset_from_name(""),
			   "an unknown name selects nothing");
		expect(profile_data_reset_name(static_cast<reset>(0)).empty(), "an unknown reset has no name");
	}

	/// [[BPR-NNNN]]. The Clear list: several categories in one request.
	void test_selections()
	{
		using selection = profile_data_reset_selection;
		using step      = profile_data_reset_step;
		expect(selection{ reset::everything } == selection::all(), "Reset everything is every category");
		for (const auto each : profile_data_reset_categories) {
			expect(selection::all().contains(each) && selection{ each }.single() == each, "each category is one of everything and round-trips alone");
		}
		expect(selection::all().single() == reset::everything && !selection{ }.single() && !selection{ reset::caches }.with(reset::review_data).single(),
			   "only a single category or all of them is named alone");
		expect(selection{ static_cast<reset>(0) }.empty() && selection{ static_cast<reset>(11) }.empty() && selection::from_bits(0xFFFFU) == selection::all(),
			   "unknown categories select nothing");

		const auto cachesAndReview = selection{ reset::caches }.with(reset::review_data);
		expect(profile_data_reset_removes(cachesAndReview, area::caches) && profile_data_reset_removes(cachesAndReview, area::review_data)
				   && !profile_data_reset_removes(cachesAndReview, area::settings) && !profile_data_reset_signs_out(cachesAndReview),
			   "two categories remove exactly their two areas");
		expect(profile_data_reset_removes(reset::saved_sources, area::saved_sources) && !profile_data_reset_removes(reset::saved_sources, area::settings),
			   "saved repositories is its own category");
		expect(profile_data_reset_removes(reset::statistics, area::statistics) && !profile_data_reset_removes(reset::statistics, area::review_data),
			   "Insights statistics is its own category");
		expect(profile_data_reset_removes_files(reset::agent_sessions) && profile_data_reset_removes(reset::agent_sessions, area::agent_sessions)
				   && profile_data_reset_destructive(reset::agent_sessions) && profile_data_reset_copyable(reset::agent_sessions),
			   "agent sessions removes its own store, asks the acknowledgement and may copy it");
		expect(profile_data_store_area("agent-sessions") == area::agent_sessions && profile_data_store_area("agent-setup") == area::settings,
			   "the conversations have their own store, apart from the agent setup");
		expect(!profile_data_reset_removes_files(reset::agent_setup) && !profile_data_reset_removes_files(reset::agent_workspaces) && !profile_data_reset_destructive(reset::agent_setup)
				   && profile_data_reset_destructive(reset::agent_workspaces),
			   "agent setup and agent workspaces remove no profile file; workspaces ask the acknowledgement");
		expect(!profile_data_reset_restores_queue_display(selection{ reset::settings }.with(reset::saved_sources)), "Settings with Saved repositories removes the queue store instead");

		// [[BPR-NNNN]], owner 2026-11-24 ("split sessions from settings").
		const auto settings = plan_profile_data_reset(reset::settings);
		expect(!settings.Selection.contains(reset::agent_sessions) && !settings.Selection.contains(reset::agent_workspaces), "Settings no longer takes agent sessions");
		expect(settings.Implied == selection{ reset::agent_setup }, "Settings takes Agent setup, whose store it removes");
		expect(settings.Steps == std::vector{ step::move_sessions, step::agent_setup, step::files, step::queue_display }, "the conversations move out before the agent setup store goes");
		const auto sessions = plan_profile_data_reset(reset::agent_sessions);
		expect(sessions.Implied == selection{ reset::agent_workspaces } && sessions.Steps == std::vector{ step::move_sessions, step::agent_workspaces, step::files },
			   "agent sessions takes their workspaces first, then removes its store");
		expect(plan_profile_data_reset(reset::agent_workspaces).Steps == std::vector{ step::move_sessions, step::agent_workspaces } && plan_profile_data_reset(reset::agent_workspaces).Implied.empty(),
			   "agent workspaces alone keeps the conversations");
		expect(plan_profile_data_reset(reset::agent_setup).Steps == std::vector{ step::move_sessions, step::agent_setup }, "agent setup alone edits the store it keeps");
		expect(plan_profile_data_reset(reset::connections).Steps == std::vector{ step::files, step::forget_connections }, "Connections removes the bindings, then forgets");
		expect(plan_profile_data_reset(reset::everything).Steps == std::vector{ step::move_sessions, step::agent_workspaces, step::agent_setup, step::files, step::forget_connections },
			   "everything runs every step but the queue display");
		expect(plan_profile_data_reset(reset::caches).Implied.empty(), "caches implies nothing");
		expect(profile_data_reset_held_back(1U) == selection{ reset::agent_sessions }.with(reset::agent_workspaces) && profile_data_reset_held_back(0U).empty(),
			   "a workspace left on disk keeps the conversations that record it; an unreadable catalog does not");

		expect(admit_profile_data_reset({ selection{ } }) == reason::invalid_selection, "nothing checked is refused");
		expect(admit_profile_data_reset({ cachesAndReview }) == reason::invalid_selection && admit_profile_data_reset({ cachesAndReview, true, true }) == reason::none,
			   "a destructive category in a selection asks the acknowledgement and may copy");
		expect(admit_profile_data_reset({ reset::agent_workspaces, true, true }) == reason::invalid_selection, "no copy for agent workspaces alone");
		expect(admit_profile_data_reset({ reset::agent_setup }) == reason::none, "agent setup needs no acknowledgement");
		expect(admit_profile_data_reset({ selection{ reset::caches }.with(reset::connections) }) == reason::none, "caches and connections need no acknowledgement");

		expect(profile_data_reset_selection_from_names("caches,agent-sessions") == selection{ reset::caches }.with(reset::agent_sessions), "the command line takes a list");
		expect(profile_data_reset_selection_from_names("everything") == selection::all(), "the command line still takes everything");
		expect(!profile_data_reset_selection_from_names("caches,") && !profile_data_reset_selection_from_names("") && !profile_data_reset_selection_from_names("caches,nope"),
			   "an empty or unknown name refuses the list");
	}
}

auto main() -> int
{
	test_store_areas();
	test_scopes();
	test_admission();
	test_names();
	test_selections();
	std::cout << "core_profile_data_reset passed\n";
	return EXIT_SUCCESS;
}
