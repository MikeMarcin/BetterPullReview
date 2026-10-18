#include "bpr/core/profile_data_reset.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>

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

	constexpr std::array resets{ reset::caches, reset::settings, reset::review_data, reset::everything };

	void test_store_areas()
	{
		expect(profile_data_store_area("epoch-catalog") == area::caches, "the comparison history catalog is a cache");
		expect(profile_data_store_area("markdown-images") == area::caches, "body images are a cache");
		expect(profile_data_store_area("review-overviews") == area::caches, "Home's summaries are a cache");
		for (const auto store : { "appearance-settings", "workbench-settings", "diagnostic-preferences", "window-restoration" }) {
			expect(profile_data_store_area(store) == area::settings, "settings stores are settings");
		}
		for (const auto store : { "review-ledger", "review-tours", "review-cycles", "review-mutation-recovery" }) {
			expect(profile_data_store_area(store) == area::review_data, "review stores are review data");
		}
		expect(profile_data_store_area("review-queue") == area::saved_sources, "saved repositories and rules are their own area");
		for (const auto store : { "device-bindings", "remembered-connections", "profile-registry", "release-update", "update-draft-recovery", "a-store-added-later", "", "review-ledger.sqlite3" }) {
			expect(profile_data_store_area(store) == area::kept, "sign-in, shared and unknown stores are kept");
		}
		for (const auto folder : { "repositories", "incoming", "locks", "quarantine", "mcp-snapshot" }) {
			expect(profile_data_folder_area(folder) == area::caches, "cache folders are caches");
		}
		expect(profile_data_folder_area("publication-git-v1") == area::review_data, "the publication store is review data");
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
		for (const auto removed : { area::caches, area::settings, area::review_data, area::saved_sources }) {
			expect(profile_data_reset_removes(reset::everything, removed), "Reset everything removes every resettable area");
		}
		for (const auto each : resets) {
			expect(!profile_data_reset_removes(each, area::kept), "no reset removes what is kept");
		}
		expect(!profile_data_reset_removes(static_cast<reset>(0), area::caches) && !profile_data_reset_removes(static_cast<reset>(9), area::caches),
			   "an unknown reset removes nothing");
		expect(profile_data_reset_restores_queue_display(reset::settings), "Reset settings restores the queue display choices");
		for (const auto each : { reset::caches, reset::review_data, reset::everything }) {
			expect(!profile_data_reset_restores_queue_display(each), "only Reset settings restores queue display in place");
		}
	}

	void test_admission()
	{
		expect(admit_profile_data_reset({ reset::caches }) == reason::none, "Clear caches needs no acknowledgement");
		expect(admit_profile_data_reset({ reset::settings }) == reason::none, "Reset settings needs no acknowledgement");
		expect(admit_profile_data_reset({ reset::review_data }) == reason::invalid_selection, "Reset review data needs the acknowledgement");
		expect(admit_profile_data_reset({ reset::everything, false, true }) == reason::invalid_selection, "a copy does not stand in for the acknowledgement");
		expect(admit_profile_data_reset({ reset::review_data, true }) == reason::none, "acknowledged review data reset is admitted");
		expect(admit_profile_data_reset({ reset::everything, true, true }) == reason::none, "acknowledged reset with a copy is admitted");
		expect(admit_profile_data_reset({ reset::caches, false, true }) == reason::invalid_selection, "Clear caches offers no copy");
		expect(admit_profile_data_reset({ static_cast<reset>(0), true }) == reason::invalid_selection, "an unknown reset is refused");
		expect(profile_data_reset_destructive(reset::review_data) && profile_data_reset_destructive(reset::everything)
				   && !profile_data_reset_destructive(reset::caches) && !profile_data_reset_destructive(reset::settings),
			   "only review data and everything are destructive");
	}

	void test_names()
	{
		for (const auto each : resets) {
			expect(profile_data_reset_from_name(profile_data_reset_name(each)) == each, "each name round-trips");
		}
		expect(profile_data_reset_name(reset::review_data) == "review-data", "the command line names review data with a hyphen");
		expect(!profile_data_reset_from_name("review_data") && !profile_data_reset_from_name("Caches") && !profile_data_reset_from_name(""),
			   "an unknown name selects nothing");
		expect(profile_data_reset_name(static_cast<reset>(0)).empty(), "an unknown reset has no name");
	}
}

auto main() -> int
{
	test_store_areas();
	test_scopes();
	test_admission();
	test_names();
	std::cout << "core_profile_data_reset passed\n";
	return EXIT_SUCCESS;
}
