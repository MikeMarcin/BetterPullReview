#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_PROFILE_WINDOW_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_PROFILE_WINDOW_HPP

#include "bpr/core/model.hpp"

#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::core {
	struct bpr_profile_id_tag;
	struct bpr_window_id_tag;
	struct bpr_window_generation_tag;
	using bpr_profile_id        = strong_id<bpr_profile_id_tag>;
	using bpr_window_id         = strong_id<bpr_window_id_tag>;
	using bpr_window_generation = strong_id<bpr_window_generation_tag>;

	inline constexpr bpr_profile_id default_bpr_profile_id{ 1U };
	inline constexpr std::size_t    maximum_bpr_profiles            = 64U;
	inline constexpr std::size_t    maximum_bpr_windows_per_profile = 32U;
	inline constexpr std::size_t    maximum_bpr_profile_label_bytes = 128U;

	/// \brief A local environment identity and its display-only label.
	struct bpr_profile_record
	{
		bpr_profile_id Id;
		std::string    Label;
		auto           operator==(const bpr_profile_record&) const -> bool = default;
	};

	enum class profile_window_failure : std::uint8_t {
		invalid_label,
		unknown_profile,
		stale_window,
		profile_limit,
		window_limit,
		identity_exhausted,
		invalid_state
	};

	/// \brief Accepts bounded, nonblank UTF-8 labels without control characters.
	/// \details Labels are never interpreted as paths or authority keys. Duplicate labels are legal.
	[[nodiscard]] auto validate_bpr_profile_label(std::string_view aLabel) noexcept -> bool;

	enum class profile_launch_intent : std::uint8_t {
		activate,
		new_window
	};

	/// \brief Exact runtime authority; generation changes when a retained window resumes.
	struct bpr_window_context
	{
		bpr_profile_id        Profile;
		bpr_window_id         Window;
		bpr_window_generation Generation;
		auto                  operator==(const bpr_window_context&) const -> bool = default;
	};

	/// \brief Stable session identity and its latest admitted runtime generation.
	struct bpr_window_record
	{
		bpr_window_id         Id;
		bpr_window_generation Generation;
		auto                  operator==(const bpr_window_record&) const -> bool = default;
	};

	/// \brief Runtime windows are ordered least to most recently focused.
	/// \details Quit moves live windows to Retained; explicit close removes its window.
	struct bpr_profile_windows
	{
		bpr_profile_record             Profile;
		std::vector<bpr_window_record> Live;
		std::vector<bpr_window_record> Retained;
		auto                           operator==(const bpr_profile_windows&) const -> bool = default;
	};

	/// \brief Bounded lifecycle snapshot; counters are durable high-water marks, never reused.
	struct profile_window_state
	{
		std::vector<bpr_profile_windows> Profiles{ { { default_bpr_profile_id, "Default" }, { }, { } } };
		std::uint64_t                    LastProfileId                                         = 1U;
		std::uint64_t                    LastWindowId                                          = 0U;
		auto                             operator==(const profile_window_state&) const -> bool = default;
	};

	/// \brief A newly admitted runtime window and whether its durable session should restore.
	struct admitted_bpr_window
	{
		bpr_window_context Context;
		bool               Restore                                              = false;
		auto               operator==(const admitted_bpr_window&) const -> bool = default;
	};

	/// \brief Host work requested by launch; an empty Created list means activation only.
	struct profile_launch_result
	{
		bpr_profile_id                   Profile;
		bpr_window_context               Activated;
		std::vector<admitted_bpr_window> Created;
	};

	/// \brief Pure lifecycle policy for local environments and their runtime windows.
	/// \details No service admission, persistence or window effects occur here. Hosts must
	/// bind completions to both identities and retire consumers following close or quit.
	class profile_window_catalog final
	{
	public:
		profile_window_catalog() = default;
		/// \brief Admits a bounded snapshot without repairing malformed identities or counters.
		[[nodiscard]] static auto restore(profile_window_state aState)
			-> std::expected<profile_window_catalog, profile_window_failure>;
		[[nodiscard]] auto state() const noexcept -> const profile_window_state&;
		/// \brief Creates an empty environment; identity allocation is independent of its label.
		[[nodiscard]] auto create_profile(std::string_view aLabel)
			-> std::expected<bpr_profile_id, profile_window_failure>;
		/// \brief Changes only a label, including Default's label; its identity remains immutable.
		[[nodiscard]] auto rename_profile(bpr_profile_id aProfile, std::string_view aLabel)
			-> std::expected<void, profile_window_failure>;
		/// \brief Activates the most recent live window or restores a stopped profile.
		/// \details New Window additionally creates a fresh window. Every restored window
		/// keeps its identity with a new generation, rejecting prior-runtime completions.
		[[nodiscard]] auto launch(bpr_profile_id aProfile, profile_launch_intent aIntent)
			-> std::expected<profile_launch_result, profile_window_failure>;
		/// \brief Moves an exact live window to most-recent focus without changing identity.
		[[nodiscard]] auto focus(bpr_window_context aContext)
			-> std::expected<void, profile_window_failure>;
		/// \brief Permanently retires an exact live window from automatic restoration.
		[[nodiscard]] auto close(bpr_window_context aContext)
			-> std::expected<void, profile_window_failure>;
		/// \brief Retains all live windows in focus order and retires their runtime contexts.
		[[nodiscard]] auto quit(bpr_profile_id aProfile)
			-> std::expected<std::vector<bpr_window_context>, profile_window_failure>;
		/// \brief Tests exact live ownership; retained and closed identities are not live.
		[[nodiscard]] auto accepts(bpr_window_context aContext) const noexcept -> bool;

	private:
		profile_window_state State;
		[[nodiscard]] auto   find_profile(bpr_profile_id aProfile) noexcept -> bpr_profile_windows*;
	};
}

#endif
