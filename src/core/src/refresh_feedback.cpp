#include "bpr/core/refresh_feedback.hpp"

#include <algorithm>
#include <limits>
#include <type_traits>

namespace bpr::core {
	namespace {
		/// The identities in `aAfter` that `aBefore` does not hold, counting
		/// each identity once however often it is listed.
		template <typename TValue>
		[[nodiscard]] auto count_new(std::vector<TValue> aBefore, std::vector<TValue> aAfter) -> std::uint32_t
		{
			std::ranges::sort(aBefore);
			std::ranges::sort(aAfter);
			const auto unique = std::ranges::unique(aAfter);
			aAfter.erase(unique.begin(), unique.end());
			auto count = std::uint32_t{ 0U };
			for (const auto& identity : aAfter) {
				if (!std::ranges::binary_search(aBefore, identity) && count != std::numeric_limits<std::uint32_t>::max()) {
					++count;
				}
			}
			return count;
		}

		[[nodiscard]] auto saturated(const std::uint64_t aValue) noexcept -> std::uint32_t
		{
			return aValue > std::numeric_limits<std::uint32_t>::max() ? std::numeric_limits<std::uint32_t>::max() : static_cast<std::uint32_t>(aValue);
		}

		constexpr std::int64_t minute = 60;
		constexpr std::int64_t hour   = 60 * minute;
		constexpr std::int64_t day    = 24 * hour;

		[[nodiscard]] auto settled_phase(const refresh_state& aState) noexcept -> refresh_phase
		{
			return aState.UpdatedAt.has_value() ? refresh_phase::updated : refresh_phase::never;
		}

		[[nodiscard]] auto quiet_trigger(const refresh_trigger aTrigger) noexcept -> bool
		{
			return aTrigger != refresh_trigger::reviewer;
		}
	} // namespace

	auto refresh_failure_of(const github_reason_code aReason) noexcept -> refresh_failure
	{
		switch (aReason) {
		case github_reason_code::network_unavailable:
		case github_reason_code::timed_out:
			return refresh_failure::offline;
		case github_reason_code::rate_limited:
			return refresh_failure::rate_limited;
		case github_reason_code::authentication_failed:
		case github_reason_code::permission_denied:
		case github_reason_code::session_required:
			return refresh_failure::signed_out;
		default:
			return refresh_failure::other;
		}
	}

	auto any(const refresh_changes& aChanges) noexcept -> bool
	{
		return aChanges.NewComments != 0U || aChanges.NewReviews != 0U || aChanges.NewPush || aChanges.NewCommits != 0U
			|| aChanges.Status.has_value() || aChanges.NewPullRequests != 0U;
	}

	auto compare_refresh(const pull_request_refresh_observation& aBefore, const pull_request_refresh_observation& aAfter) -> refresh_changes
	{
		auto changes        = refresh_changes{ };
		changes.NewComments = count_new(aBefore.Comments, aAfter.Comments);
		changes.NewReviews  = count_new(aBefore.Reviews, aAfter.Reviews);
		// Neither read knowing a head is not a push; a head appearing or
		// moving is.
		changes.NewPush    = aBefore.Head != aAfter.Head && !aAfter.Head.empty();
		changes.NewCommits = changes.NewPush && aAfter.Commits > aBefore.Commits ? saturated(aAfter.Commits - aBefore.Commits) : 0U;
		if (aBefore.Status != aAfter.Status) {
			changes.Status = aAfter.Status;
		}
		return changes;
	}

	auto compare_refresh(const std::vector<pull_request_ref>& aBefore, const std::vector<pull_request_ref>& aAfter) -> refresh_changes
	{
		return refresh_changes{ .NewPullRequests = count_new(aBefore, aAfter) };
	}

	auto apply_refresh(const refresh_state& aState, const refresh_event& aEvent) -> refresh_state
	{
		return std::visit(
			[&aState](const auto& aValue) -> refresh_state {
				using event_type = std::decay_t<decltype(aValue)>;
				auto next        = aState;
				if constexpr (std::is_same_v<event_type, refresh_started>) {
					if (aState.Phase == refresh_phase::refreshing) {
						// One read is under way; the reviewer's gesture outranks
						// the application's own.
						if (aValue.Trigger == refresh_trigger::reviewer) {
							next.Trigger = refresh_trigger::reviewer;
						}
						return next;
					}
					next.Phase   = refresh_phase::refreshing;
					next.Trigger = aValue.Trigger;
					return next;
				} else if constexpr (std::is_same_v<event_type, refresh_succeeded>) {
					next.Phase             = refresh_phase::updated;
					next.Changes           = aValue.Changes;
					next.RetryAfterSeconds = std::nullopt;
					next.UpdatedAt         = aValue.At;
					next.SettledAt         = aValue.At;
					return next;
				} else if constexpr (std::is_same_v<event_type, refresh_failed>) {
					next.Phase             = refresh_phase::failed;
					next.Changes           = refresh_changes{ };
					next.Failure           = aValue.Failure;
					next.RetryAfterSeconds = aValue.RetryAfterSeconds;
					next.SettledAt         = aValue.At;
					return next;
				} else {
					if (aState.Phase == refresh_phase::refreshing) {
						next.Phase = settled_phase(aState);
					}
					return next;
				}
			},
			aEvent);
	}

	auto refresh_age_at(const std::int64_t aUpdatedAt, const std::int64_t aNow) noexcept -> refresh_age
	{
		// A clock that went back reads as just now rather than as a negative age.
		const auto elapsed = aNow > aUpdatedAt ? aNow - aUpdatedAt : 0;
		if (elapsed < minute) {
			return { .Unit = refresh_age_unit::just_now, .Count = 0U };
		}
		if (elapsed < hour) {
			return { .Unit = refresh_age_unit::minutes, .Count = static_cast<std::uint32_t>(elapsed / minute) };
		}
		if (elapsed < day) {
			return { .Unit = refresh_age_unit::hours, .Count = static_cast<std::uint32_t>(elapsed / hour) };
		}
		return { .Unit = refresh_age_unit::days, .Count = saturated(static_cast<std::uint64_t>(elapsed / day)) };
	}

	auto refresh_age_next_change_seconds(const std::int64_t aUpdatedAt, const std::int64_t aNow) noexcept -> std::int64_t
	{
		const auto elapsed = aNow > aUpdatedAt ? aNow - aUpdatedAt : 0;
		const auto step    = elapsed < hour ? minute : elapsed < day ? hour
																	 : day;
		return step - (elapsed % step);
	}

	auto refresh_announcement_between(const refresh_state& aBefore, const refresh_state& aAfter) noexcept -> refresh_announcement
	{
		if (aAfter.Phase == refresh_phase::refreshing) {
			const auto began    = aBefore.Phase != refresh_phase::refreshing;
			const auto promoted = !began && quiet_trigger(aBefore.Trigger) && aAfter.Trigger == refresh_trigger::reviewer;
			return (began || promoted) && aAfter.Trigger == refresh_trigger::reviewer ? refresh_announcement::started : refresh_announcement::none;
		}
		if (aAfter.SettledAt == aBefore.SettledAt && aAfter.Phase == aBefore.Phase) {
			return refresh_announcement::none;
		}
		if (aAfter.Phase == refresh_phase::failed) {
			return refresh_announcement::failed;
		}
		if (aAfter.Phase == refresh_phase::updated) {
			const auto quiet = aBefore.Phase == refresh_phase::refreshing ? quiet_trigger(aBefore.Trigger) : quiet_trigger(aAfter.Trigger);
			return !quiet || any(aAfter.Changes) ? refresh_announcement::updated : refresh_announcement::none;
		}
		return refresh_announcement::none;
	}
} // namespace bpr::core
