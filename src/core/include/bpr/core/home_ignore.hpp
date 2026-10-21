#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_HOME_IGNORE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_HOME_IGNORE_HPP

#include "bpr/core/review_queue.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace bpr::core {
	/// \brief [[BPR-1683]]. The most pull requests one profile can hold ignored.
	///
	/// \details An ignore is only accepted for a pull request on Home, and Home
	/// holds at most this many enrollments, so the bound is the same one.
	inline constexpr std::size_t maximum_home_ignores = 16384U;

	/// \brief [[BPR-1683]]. What Home last observed about one pull request.
	///
	/// \details Read by the host from the queue's retained enrollment. `closed`
	/// is a pull request GitHub reported closed or merged; `left` is one that is
	/// off Home for any other reason the host observed (no longer found by a
	/// complete discovery, no longer readable, gone, or removed by the reviewer).
	/// A pull request the host has no record of is not observed at all and is
	/// never reported as `left`.
	enum class home_tracking : std::uint8_t {
		open,
		closed,
		left,
	};

	/// \brief One observation of one pull request's place on Home.
	struct home_tracking_observation
	{
		pull_request_ref Ref;
		home_tracking    Tracking = home_tracking::open;

		auto operator<=>(const home_tracking_observation&) const = default;
	};

	enum class home_ignore_action : std::uint8_t {
		ignore,
		unignore,
	};

	/// \brief [[BPR-1683]]. The reviewer's Ignore or Unignore on one Home row.
	struct home_ignore_intent
	{
		pull_request_ref   Ref;
		home_ignore_action Action = home_ignore_action::ignore;

		auto operator<=>(const home_ignore_intent&) const = default;
	};

	/// \brief What one transition did to the ignored list.
	enum class home_ignore_change : std::uint8_t {
		/// The reviewer ignored an open pull request on Home.
		ignored,
		/// The reviewer unignored it; it returns to its own section.
		unignored,
		/// It was closed or merged, so the ignore has nothing left to hide.
		pruned_closed,
		/// It left Home for another reason; the ignore goes with it.
		pruned_left,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(home_ignore_change aChange) noexcept -> std::string_view;

	/// \brief One semantic change to the ignored list, for the store and the projection.
	struct home_ignore_delta
	{
		pull_request_ref   Ref;
		home_ignore_change Change = home_ignore_change::ignored;

		auto operator<=>(const home_ignore_delta&) const = default;
	};

	/// \brief Whether a reference can name an ignored pull request at all.
	[[nodiscard]] auto ignorable(const pull_request_ref& aRef) noexcept -> bool;

	/// \brief [[BPR-1683]]. The pull requests the reviewer chose to ignore on Home.
	///
	/// \details Keyed by the whole pull request identity, so one ignore is
	/// scoped to exactly one host, account, repository and number, and never
	/// hides the same number under another account. Holds no clock, provider or
	/// store; every change is returned as a delta the host performs.
	///
	/// An ignore lasts as long as its pull request is on Home. Once the host
	/// observes it closed, merged or otherwise gone from Home, `observe` prunes
	/// it, so a pull request that comes back later (reopened, or requested
	/// again) arrives as news rather than already hidden.
	class home_ignore_list final
	{
	public:
		home_ignore_list() = default;

		/// \brief Adopts a restored list; unusable references and duplicates are dropped.
		explicit home_ignore_list(std::span<const pull_request_ref> aRefs);

		[[nodiscard]] auto contains(const pull_request_ref& aRef) const -> bool;
		/// \brief Every ignored pull request, in identity order.
		[[nodiscard]] auto refs() const noexcept -> std::span<const pull_request_ref>;
		[[nodiscard]] auto size() const noexcept -> std::size_t;

		/// \brief Applies the reviewer's gesture to a pull request Home observed as `aTracking`.
		///
		/// \details Ignoring is accepted only for an open pull request that is
		/// not already ignored, within the bound. Unignoring is accepted for any
		/// ignored pull request, wherever it is. Anything else changes nothing
		/// and returns nothing.
		[[nodiscard]] auto apply(const home_ignore_intent& aIntent, home_tracking aTracking)
			-> std::optional<home_ignore_delta>;

		/// \brief Prunes every ignored pull request observed closed or off Home.
		///
		/// \details The deltas are in identity order. A pull request with
		/// several observations is pruned when any of them is not `open`, and
		/// reported closed when any of them says so. Observations of pull
		/// requests that are not ignored, and ignored pull requests with no
		/// observation, are left as they are.
		[[nodiscard]] auto observe(std::span<const home_tracking_observation> aObservations)
			-> std::vector<home_ignore_delta>;

		auto operator<=>(const home_ignore_list&) const = default;

	private:
		std::vector<pull_request_ref> Refs;
	};
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_HOME_IGNORE_HPP
