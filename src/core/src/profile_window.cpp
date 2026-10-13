#include "bpr/core/profile_window.hpp"
#include "bpr/core/utf8.hpp"

#include <algorithm>
#include <limits>
#include <optional>
#include <utility>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto next_code_point(const std::string_view aText, std::size_t& aPosition) noexcept -> std::optional<std::uint32_t>
		{
			// [[BPR-1434]]. The shared utf8proc decoder; an ill-formed byte is skipped.
			auto       next  = aPosition;
			const auto value = utf8::decode(aText, next);
			aPosition        = value ? next : aPosition + 1U;
			return value ? std::optional{ static_cast<std::uint32_t>(*value) } : std::nullopt;
		}

		[[nodiscard]] auto whitespace(const std::uint32_t aPoint) noexcept -> bool
		{
			return aPoint == 0x20U || aPoint == 0xA0U || aPoint == 0x1680U
				|| (aPoint >= 0x2000U && aPoint <= 0x200AU)
				|| aPoint == 0x2028U || aPoint == 0x2029U || aPoint == 0x202FU
				|| aPoint == 0x205FU || aPoint == 0x3000U;
		}

		[[nodiscard]] auto valid_windows(const std::vector<bpr_window_record>& aWindows,
										 const std::uint64_t aLastId, std::vector<bpr_window_id>& aSeen) -> bool
		{
			for (const auto& window : aWindows) {
				if (window.Id.value() == 0U || window.Id.value() > aLastId
					|| window.Generation.value() == 0U
					|| std::ranges::find(aSeen, window.Id) != aSeen.end()) {
					return false;
				}
				aSeen.push_back(window.Id);
			}
			return true;
		}

		[[nodiscard]] auto valid_state(const profile_window_state& aState) -> bool
		{
			if (aState.Profiles.empty() || aState.Profiles.size() > maximum_bpr_profiles
				|| aState.Profiles.front().Profile.Id != default_bpr_profile_id) {
				return false;
			}
			auto profiles = std::vector<bpr_profile_id>{ };
			auto windows  = std::vector<bpr_window_id>{ };
			for (const auto& profile : aState.Profiles) {
				if (!validate_bpr_profile_label(profile.Profile.Label)
					|| profile.Profile.Id.value() == 0U || profile.Profile.Id.value() > aState.LastProfileId
					|| std::ranges::find(profiles, profile.Profile.Id) != profiles.end()
					|| profile.Live.size() + profile.Retained.size() > maximum_bpr_windows_per_profile
					|| (!profile.Live.empty() && !profile.Retained.empty())
					|| !valid_windows(profile.Live, aState.LastWindowId, windows)
					|| !valid_windows(profile.Retained, aState.LastWindowId, windows)) {
					return false;
				}
				profiles.push_back(profile.Profile.Id);
			}
			return true;
		}

		[[nodiscard]] auto context_of(const bpr_profile_id aProfile, const bpr_window_record& aWindow) -> bpr_window_context
		{
			return { aProfile, aWindow.Id, aWindow.Generation };
		}
	}

	auto validate_bpr_profile_label(const std::string_view aLabel) noexcept -> bool
	{
		if (aLabel.empty() || aLabel.size() > maximum_bpr_profile_label_bytes) {
			return false;
		}
		auto position = std::size_t{ 0U };
		bool visible  = false;
		while (position < aLabel.size()) {
			const auto point = next_code_point(aLabel, position);
			if (!point || *point < 0x20U || (*point >= 0x7FU && *point <= 0x9FU)
				|| *point == 0x2028U || *point == 0x2029U) {
				return false;
			}
			visible = visible || !whitespace(*point);
		}
		return visible;
	}

	auto profile_window_catalog::restore(profile_window_state aState)
		-> std::expected<profile_window_catalog, profile_window_failure>
	{
		if (!valid_state(aState)) {
			return std::unexpected(profile_window_failure::invalid_state);
		}
		auto result  = profile_window_catalog{ };
		result.State = std::move(aState);
		return result;
	}

	auto profile_window_catalog::state() const noexcept -> const profile_window_state&
	{
		return State;
	}

	auto profile_window_catalog::find_profile(const bpr_profile_id aProfile) noexcept -> bpr_profile_windows*
	{
		const auto found = std::ranges::find_if(State.Profiles, [aProfile](const auto& aEntry) {
			return aEntry.Profile.Id == aProfile;
		});
		return found == State.Profiles.end() ? nullptr : &*found;
	}

	auto profile_window_catalog::create_profile(const std::string_view aLabel)
		-> std::expected<bpr_profile_id, profile_window_failure>
	{
		if (!validate_bpr_profile_label(aLabel)) {
			return std::unexpected(profile_window_failure::invalid_label);
		}
		if (State.Profiles.size() == maximum_bpr_profiles) {
			return std::unexpected(profile_window_failure::profile_limit);
		}
		if (State.LastProfileId == std::numeric_limits<std::uint64_t>::max()) {
			return std::unexpected(profile_window_failure::identity_exhausted);
		}
		const auto id = bpr_profile_id{ State.LastProfileId + 1U };
		State.Profiles.push_back({ { id, std::string{ aLabel } }, { }, { } });
		State.LastProfileId = id.value();
		return id;
	}

	auto profile_window_catalog::rename_profile(const bpr_profile_id aProfile, const std::string_view aLabel)
		-> std::expected<void, profile_window_failure>
	{
		auto* profile = find_profile(aProfile);
		if (profile == nullptr) {
			return std::unexpected(profile_window_failure::unknown_profile);
		}
		if (!validate_bpr_profile_label(aLabel)) {
			return std::unexpected(profile_window_failure::invalid_label);
		}
		profile->Profile.Label = aLabel;
		return { };
	}

	auto profile_window_catalog::launch(const bpr_profile_id aProfile, const profile_launch_intent aIntent)
		-> std::expected<profile_launch_result, profile_window_failure>
	{
		auto* profile = find_profile(aProfile);
		if (profile == nullptr) {
			return std::unexpected(profile_window_failure::unknown_profile);
		}
		if (aIntent != profile_launch_intent::activate && aIntent != profile_launch_intent::new_window) {
			return std::unexpected(profile_window_failure::invalid_state);
		}
		const auto fresh = aIntent == profile_launch_intent::new_window || (profile->Live.empty() && profile->Retained.empty());
		if (profile->Live.size() + profile->Retained.size() + (fresh ? 1U : 0U) > maximum_bpr_windows_per_profile) {
			return std::unexpected(profile_window_failure::window_limit);
		}
		if ((fresh && State.LastWindowId == std::numeric_limits<std::uint64_t>::max())
			|| std::ranges::any_of(profile->Retained, [](const auto& aWindow) {
				   return aWindow.Generation.value() == std::numeric_limits<std::uint64_t>::max();
			   })) {
			return std::unexpected(profile_window_failure::identity_exhausted);
		}
		auto result = profile_launch_result{ aProfile, { }, { } };
		for (auto window : profile->Retained) {
			window.Generation = bpr_window_generation{ window.Generation.value() + 1U };
			profile->Live.push_back(window);
			result.Created.push_back({ context_of(aProfile, window), true });
		}
		profile->Retained.clear();
		if (fresh) {
			const auto window = bpr_window_record{ bpr_window_id{ State.LastWindowId + 1U }, bpr_window_generation{ 1U } };
			profile->Live.push_back(window);
			State.LastWindowId = window.Id.value();
			result.Created.push_back({ context_of(aProfile, window), false });
		}
		result.Activated = context_of(aProfile, profile->Live.back());
		return result;
	}

	auto profile_window_catalog::accepts(const bpr_window_context aContext) const noexcept -> bool
	{
		const auto profile = std::ranges::find_if(State.Profiles, [aContext](const auto& aEntry) {
			return aEntry.Profile.Id == aContext.Profile;
		});
		return profile != State.Profiles.end() && std::ranges::any_of(profile->Live, [aContext](const auto& aWindow) {
				   return aWindow.Id == aContext.Window && aWindow.Generation == aContext.Generation;
			   });
	}

	auto profile_window_catalog::focus(const bpr_window_context aContext)
		-> std::expected<void, profile_window_failure>
	{
		auto* profile = find_profile(aContext.Profile);
		if (profile == nullptr) {
			return std::unexpected(profile_window_failure::unknown_profile);
		}
		const auto found = std::ranges::find(profile->Live, bpr_window_record{ aContext.Window, aContext.Generation });
		if (found == profile->Live.end()) {
			return std::unexpected(profile_window_failure::stale_window);
		}
		std::rotate(found, found + 1, profile->Live.end());
		return { };
	}

	auto profile_window_catalog::close(const bpr_window_context aContext)
		-> std::expected<void, profile_window_failure>
	{
		auto* profile = find_profile(aContext.Profile);
		if (profile == nullptr) {
			return std::unexpected(profile_window_failure::unknown_profile);
		}
		const auto found = std::ranges::find(profile->Live, bpr_window_record{ aContext.Window, aContext.Generation });
		if (found == profile->Live.end()) {
			return std::unexpected(profile_window_failure::stale_window);
		}
		profile->Live.erase(found);
		return { };
	}

	auto profile_window_catalog::quit(const bpr_profile_id aProfile)
		-> std::expected<std::vector<bpr_window_context>, profile_window_failure>
	{
		auto* profile = find_profile(aProfile);
		if (profile == nullptr) {
			return std::unexpected(profile_window_failure::unknown_profile);
		}
		auto retired = std::vector<bpr_window_context>{ };
		for (const auto& window : profile->Live) {
			retired.push_back(context_of(aProfile, window));
		}
		if (!profile->Live.empty()) {
			profile->Retained = std::move(profile->Live);
			profile->Live.clear();
		}
		return retired;
	}
}
