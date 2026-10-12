#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_HOME_VIEW_STATE_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_HOME_VIEW_STATE_HPP

#include "bpr/application/review_queue_enrollment.hpp"
#include "bpr/core/review_queue.hpp"

#include <compare>
#include <cstdint>
#include <optional>

namespace bpr::application {
	/// \brief [[BPR-0783]]. What Home groups its rows by.
	///
	/// \details The state order stays beneath every value: grouping changes the
	/// panels around the rows, never their order.
	enum class home_grouping : std::uint8_t {
		none,
		organization,
		repository,
		account,
	};

	/// \brief [[BPR-0783]]. Home's panels that carry no name, as bits of
	/// `home_view_state::Collapsed`.
	///
	/// \details A group panel and the state bands inside it are keyed by an
	/// owner, a repository or an account. Those keys name what was reviewed, so
	/// their collapse is not part of the durable view state; the window keeps it
	/// while it lives.
	namespace home_collapsed_panel {
		inline constexpr std::uint8_t needs_me          = 1U << 0U;
		inline constexpr std::uint8_t waiting_on_others = 1U << 1U;
		inline constexpr std::uint8_t following         = 1U << 2U;
		inline constexpr std::uint8_t recently_viewed   = 1U << 3U;
		inline constexpr std::uint8_t my_pull_requests  = 1U << 4U;
		inline constexpr std::uint8_t all               = static_cast<unsigned>(needs_me) | waiting_on_others | following | recently_viewed | my_pull_requests;
	} // namespace home_collapsed_panel

	/// \brief [[BPR-0783]]. The four typed queue preferences that survive into
	/// Home's Filter menu: My role, Added by, Quiet items and Sort within a group.
	///
	/// \details The same vocabularies as `queue_presentation_preferences`; no
	/// value is added. Repository and Bucket are not Home filters.
	struct home_filter
	{
		std::optional<core::github_role>        Role;
		std::optional<core::observer_rule_kind> AddedBy;
		queue_quiet_filter                      Quiet = queue_quiet_filter::all;
		queue_sort_order                        Sort  = queue_sort_order::core_priority;

		auto operator<=>(const home_filter&) const = default;
	};

	/// \brief [[BPR-0783]]. One window's Home view, remembered per window as
	/// RFC-BPR-0024 places window filters.
	///
	/// \details Presentation arrangement the window owns. The host validates it
	/// and persists it beside the window's layout; nothing interprets it.
	struct home_view_state
	{
		home_grouping Grouping     = home_grouping::none;
		bool          StateHeaders = true;
		/// Bits of `home_collapsed_panel`.
		std::uint8_t Collapsed = 0U;
		/// False until the window's first Filter gesture. Until then Home
		/// follows the profile's typed queue preferences.
		bool        FilterOwned = false;
		home_filter Filter;

		auto operator<=>(const home_view_state&) const = default;
	};

	/// \brief Checks that every Filter field names a value of its vocabulary.
	[[nodiscard]] constexpr auto valid(const home_filter& aFilter) noexcept -> bool
	{
		return (!aFilter.Role.has_value()
				|| static_cast<std::uint8_t>(*aFilter.Role) <= static_cast<std::uint8_t>(core::github_role::author))
			&& (!aFilter.AddedBy.has_value()
				|| static_cast<std::uint8_t>(*aFilter.AddedBy) <= static_cast<std::uint8_t>(core::observer_rule_kind::path))
			&& static_cast<std::uint8_t>(aFilter.Quiet) <= static_cast<std::uint8_t>(queue_quiet_filter::quiet)
			&& static_cast<std::uint8_t>(aFilter.Sort) <= static_cast<std::uint8_t>(queue_sort_order::pull_request_number);
	}

	/// \brief Checks the grouping, the collapsed bits and the Filter.
	[[nodiscard]] constexpr auto valid(const home_view_state& aState) noexcept -> bool
	{
		return static_cast<std::uint8_t>(aState.Grouping) <= static_cast<std::uint8_t>(home_grouping::account)
			&& (aState.Collapsed & static_cast<std::uint8_t>(~home_collapsed_panel::all)) == 0U
			&& valid(aState.Filter);
	}

	/// \brief The four surviving fields of the profile's typed preferences.
	[[nodiscard]] inline auto home_filter_of(const queue_presentation_preferences& aPreferences) -> home_filter
	{
		return home_filter{
			.Role    = aPreferences.Role,
			.AddedBy = aPreferences.ObserverReason,
			.Quiet   = aPreferences.Quiet,
			.Sort    = aPreferences.Sort,
		};
	}

	/// \brief The preferences Home filters by in one window.
	///
	/// \details The window's own Filter once it owns one, otherwise the
	/// profile's four fields; never Repository or Bucket, which Home does not
	/// filter by. Notifications are carried through unchanged.
	[[nodiscard]] inline auto home_preferences(
		const home_view_state& aState, queue_presentation_preferences aProfile) -> queue_presentation_preferences
	{
		if (aState.FilterOwned) {
			aProfile.Role           = aState.Filter.Role;
			aProfile.ObserverReason = aState.Filter.AddedBy;
			aProfile.Quiet          = aState.Filter.Quiet;
			aProfile.Sort           = aState.Filter.Sort;
		}
		aProfile.Bucket.reset();
		aProfile.Repository.clear();
		return aProfile;
	}
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_HOME_VIEW_STATE_HPP
