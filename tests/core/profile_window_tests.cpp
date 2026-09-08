#include "bpr/core/github_model.hpp"
#include "bpr/core/profile_window.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>

namespace {
	using namespace bpr::core;
	static_assert(!std::is_same_v<bpr_profile_id, host_profile_id>);
	static_assert(!std::is_convertible_v<host_profile_id, bpr_profile_id>);
	static_assert(!std::is_same_v<bpr_profile_id, bpr_window_id>);

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto test_identity_and_labels() -> void
	{
		auto catalog = profile_window_catalog{ };
		expect(catalog.state().Profiles.size() == 1U && catalog.state().Profiles.front().Profile == bpr_profile_record{ default_bpr_profile_id, "Default" }, "Default has the canonical immutable identity");
		const auto work      = catalog.create_profile("Work");
		const auto otherWork = catalog.create_profile("Work");
		expect(work && otherWork && *work != *otherWork && *work != default_bpr_profile_id, "duplicate labels create distinct environments");
		expect(catalog.rename_profile(*work, "Personal").has_value(), "rename succeeds");
		expect(catalog.state().Profiles[1].Profile == bpr_profile_record{ *work, "Personal" }, "rename changes only display data");
		expect(catalog.rename_profile(default_bpr_profile_id, "Renamed Default").has_value()
				   && catalog.state().Profiles.front().Profile.Id == default_bpr_profile_id,
			   "Default identity survives label changes");
		const auto before = catalog.state();
		for (const auto& label : { std::string{ }, std::string{ " \xC2\xA0" }, std::string{ "a\nb" }, std::string{ "a\0b", 3U }, std::string{ "\xC0\x80" }, std::string{ "\xED\xA0\x80" }, std::string{ "\xF4\x90\x80\x80" }, std::string{ "\xE2\x82" }, std::string(maximum_bpr_profile_label_bytes + 1U, 'a') }) {
			const auto created = catalog.create_profile(label);
			expect(!created && created.error() == profile_window_failure::invalid_label, "invalid labels are rejected with a typed reason");
			expect(!catalog.rename_profile(*work, label), "rename uses the same label contract");
			expect(catalog.state() == before, "invalid labels neither allocate identity nor mutate existing state");
		}
		expect(validate_bpr_profile_label("\xE5\xB7\xA5\xE4\xBD\x9C") && validate_bpr_profile_label("../Work")
				   && validate_bpr_profile_label(std::string(maximum_bpr_profile_label_bytes, 'a')),
			   "valid Unicode and path-looking display data are accepted at the byte bound");
	}

	auto test_lifecycle_and_exact_context() -> void
	{
		auto       catalog = profile_window_catalog{ };
		const auto work    = *catalog.create_profile("Work");
		const auto first   = catalog.launch(default_bpr_profile_id, profile_launch_intent::activate);
		expect(first && first->Created.size() == 1U && !first->Created.front().Restore, "first activation requests a fresh window");
		const auto one      = first->Activated;
		const auto repeated = catalog.launch(default_bpr_profile_id, profile_launch_intent::activate);
		expect(repeated && repeated->Created.empty() && repeated->Activated == one, "ordinary repeat activates without allocating");
		const auto two   = catalog.launch(default_bpr_profile_id, profile_launch_intent::new_window)->Activated;
		const auto other = catalog.launch(work, profile_launch_intent::activate)->Activated;
		expect(two.Window != one.Window && other.Window != two.Window, "new windows have independent never-shared identities");
		expect(catalog.focus(one).has_value(), "focus accepts a live exact context");
		expect(catalog.launch(default_bpr_profile_id, profile_launch_intent::activate)->Activated == one, "ordinary activation honors most recent focus");
		const auto before          = catalog.state();
		const auto wrongProfile    = bpr_window_context{ work, one.Window, one.Generation };
		const auto wrongGeneration = bpr_window_context{ one.Profile, one.Window, bpr_window_generation{ one.Generation.value() + 1U } };
		for (const auto wrong : { wrongProfile, wrongGeneration, bpr_window_context{ bpr_profile_id{ 99U }, one.Window, one.Generation } }) {
			expect(!catalog.accepts(wrong) && !catalog.focus(wrong) && !catalog.close(wrong), "wrong profile or stale generation cannot operate a live window");
			expect(catalog.state() == before, "wrong-context operations leave every profile intact");
		}
		expect(catalog.close(two).has_value() && !catalog.accepts(two), "explicit close retires exactly its window");
		const auto retired = catalog.quit(default_bpr_profile_id);
		expect(retired && *retired == std::vector{ one }, "quit reports precisely its remaining live consumers");
		expect(!catalog.accepts(one) && catalog.accepts(other), "profile quit retires its runtime but leaves the other profile usable");
		const auto quitState = catalog.state();
		expect(catalog.quit(default_bpr_profile_id)->empty() && catalog.state() == quitState, "repeated quit preserves the retained restore set");
		auto restarted = profile_window_catalog::restore(quitState);
		expect(restarted.has_value(), "bounded state can be readmitted");
		const auto restored = restarted->launch(default_bpr_profile_id, profile_launch_intent::activate);
		expect(restored && restored->Created.size() == 1U && restored->Created.front().Restore
				   && restored->Activated.Window == one.Window && restored->Activated.Generation != one.Generation,
			   "only retained windows restore with stable durable ID and a fresh runtime generation");
		expect(!restarted->accepts(one) && !restarted->close(one) && restarted->accepts(restored->Activated), "callbacks from the prior runtime cannot close the restored window");
		const auto three = restarted->launch(default_bpr_profile_id, profile_launch_intent::new_window)->Activated;
		expect(three.Window.value() > other.Window.value() && three.Window != two.Window, "explicit close never makes its ID reusable, including after restart");
		expect(restarted->close(restored->Activated).has_value() && restarted->close(three).has_value(), "all windows can close explicitly");
		const auto fresh = restarted->launch(default_bpr_profile_id, profile_launch_intent::activate);
		expect(fresh && fresh->Created.size() == 1U && !fresh->Created.front().Restore, "closing the last window removes automatic restoration without deleting the profile");
	}

	auto test_bounds_and_exhaustion() -> void
	{
		auto catalog = profile_window_catalog{ };
		for (std::size_t index = 1U; index < maximum_bpr_profiles; ++index) {
			expect(catalog.create_profile("Profile").has_value(), "profiles admit up to the bound");
		}
		const auto fullProfiles   = catalog.state();
		const auto refusedProfile = catalog.create_profile("Overflow");
		expect(!refusedProfile && refusedProfile.error() == profile_window_failure::profile_limit && catalog.state() == fullProfiles, "profile bound refuses without mutation");
		for (std::size_t index = 0U; index < maximum_bpr_windows_per_profile; ++index) {
			expect(catalog.launch(default_bpr_profile_id, profile_launch_intent::new_window).has_value(), "windows admit up to the bound");
		}
		const auto fullWindows   = catalog.state();
		const auto refusedWindow = catalog.launch(default_bpr_profile_id, profile_launch_intent::new_window);
		expect(!refusedWindow && refusedWindow.error() == profile_window_failure::window_limit && catalog.state() == fullWindows, "window bound refuses without mutation");
		expect(catalog.launch(default_bpr_profile_id, profile_launch_intent::activate)->Created.empty(), "activation still works at window capacity");
		expect(catalog.quit(default_bpr_profile_id).has_value(), "full profile quits");
		const auto retained = catalog.state();
		expect(!catalog.launch(default_bpr_profile_id, profile_launch_intent::new_window) && catalog.state() == retained, "restore plus explicit extra window checks capacity before restoring anything");
		const auto restored = catalog.launch(default_bpr_profile_id, profile_launch_intent::activate);
		expect(restored && restored->Created.size() == maximum_bpr_windows_per_profile, "ordinary activation can restore the entire bounded set");
		auto exhaustedState          = profile_window_state{ };
		exhaustedState.LastProfileId = std::numeric_limits<std::uint64_t>::max();
		exhaustedState.LastWindowId  = std::numeric_limits<std::uint64_t>::max();
		auto exhausted               = profile_window_catalog::restore(exhaustedState);
		expect(exhausted.has_value(), "valid exhausted high-water counters are representable");
		const auto noProfile = exhausted->create_profile("New");
		const auto noWindow  = exhausted->launch(default_bpr_profile_id, profile_launch_intent::activate);
		expect(!noProfile && noProfile.error() == profile_window_failure::identity_exhausted
				   && !noWindow && noWindow.error() == profile_window_failure::identity_exhausted
				   && exhausted->state() == exhaustedState,
			   "identity exhaustion never wraps or partially changes state");
		exhaustedState.Profiles.front().Retained.push_back({ bpr_window_id{ 1U }, bpr_window_generation{ std::numeric_limits<std::uint64_t>::max() } });
		exhausted               = profile_window_catalog::restore(exhaustedState);
		const auto noGeneration = exhausted->launch(default_bpr_profile_id, profile_launch_intent::activate);
		expect(!noGeneration && noGeneration.error() == profile_window_failure::identity_exhausted && exhausted->state() == exhaustedState, "generation exhaustion refuses before altering retained windows");
	}

	auto test_invalid_snapshots() -> void
	{
		auto state = profile_window_state{ };
		state.Profiles.clear();
		expect(!profile_window_catalog::restore(state), "Default cannot disappear");
		state                             = profile_window_state{ };
		state.Profiles.front().Profile.Id = bpr_profile_id{ 2U };
		expect(!profile_window_catalog::restore(state), "a different ID cannot replace Default");
		state = profile_window_state{ };
		state.Profiles.push_back(state.Profiles.front());
		expect(!profile_window_catalog::restore(state), "duplicate profile IDs are refused");
		state = profile_window_state{ };
		state.Profiles.front().Live.push_back({ bpr_window_id{ 1U }, bpr_window_generation{ 1U } });
		expect(!profile_window_catalog::restore(state), "a counter below an allocated window is refused");
		state.LastWindowId              = 1U;
		state.Profiles.front().Retained = state.Profiles.front().Live;
		expect(!profile_window_catalog::restore(state), "live and retained authority cannot overlap");
	}
}

auto main() -> int
{
	test_identity_and_labels();
	test_lifecycle_and_exact_context();
	test_bounds_and_exhaustion();
	test_invalid_snapshots();
	std::cout << "profile-window-tests:pass\n";
	return EXIT_SUCCESS;
}
