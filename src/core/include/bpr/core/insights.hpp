#ifndef BPR_CORE_INSIGHTS_HPP
#define BPR_CORE_INSIGHTS_HPP

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace bpr::core {
	enum class insights_resource : std::uint8_t { core,
												  graphql,
												  search };
	enum class insights_category : std::uint8_t { inbox,
												  opening,
												  conversations,
												  publishing,
												  other };
	enum class insights_event_kind : std::uint8_t { request,
													process,
													problem };
	enum class insights_process_failure : std::uint8_t { none,
														 start,
														 timeout,
														 cancelled,
														 output_limit,
														 crash,
														 nonzero_exit,
														 reap,
														 other };

	/// \brief Private numeric account partition; zero denotes anonymous reads.
	struct insights_scope
	{
		std::uint64_t Host                                     = 0;
		std::uint64_t Account                                  = 0;
		auto          operator<=>(const insights_scope&) const = default;
	};
	/// \brief Content-free observation. Timestamps and measurements are supplied by the host.
	struct insights_event
	{
		insights_scope               Scope;
		std::int64_t                 UnixSeconds = 0;
		insights_event_kind          Kind        = insights_event_kind::request;
		insights_resource            Resource    = insights_resource::core;
		insights_category            Category    = insights_category::other;
		std::uint64_t                Count       = 1;
		std::optional<std::uint64_t> Limit;
		std::optional<std::uint64_t> Remaining;
		std::int64_t                 ResetUnixSeconds = 0;
		bool                         SecondaryLimited = false;
		std::optional<std::uint32_t> RetryAfterSeconds;
		insights_process_failure     ProcessFailure                                  = insights_process_failure::none;
		std::uint64_t                DurationMicroseconds                            = 0;
		bool                         Launched                                        = false;
		bool                         Quarantined                                     = false;
		bool                         Abandoned                                       = false;
		auto                         operator==(const insights_event&) const -> bool = default;
	};
	/// \brief Current resource window. A missing reading never becomes a measured zero.
	struct insights_budget
	{
		std::optional<std::uint64_t> Limit;
		std::optional<std::uint64_t> Remaining;
		std::int64_t                 ResetUnixSeconds = 0;
		std::uint64_t                OwnRequests      = 0;
		std::optional<std::uint64_t> OtherClients;
	};
	struct insights_github_point
	{
		std::int64_t          UnixSeconds = 0;
		std::optional<double> Remaining;
		double                RequestsPerMinute = 0;
		bool                  Reset             = false;
	};
	/// \brief Pure projection over retained facts for one account and one supplied time.
	struct insights_github_snapshot
	{
		bool                               Available = false;
		insights_scope                     Scope;
		std::array<insights_budget, 3>     Budgets;
		std::array<std::uint64_t, 5>       RequestsByCategory{ };
		std::array<std::uint64_t, 5>       ProblemsByCategory{ };
		std::uint64_t                      Processes = 0;
		std::array<std::uint64_t, 9>       ProcessFailures{ };
		std::uint64_t                      Quarantines = 0;
		std::uint64_t                      Abandoned   = 0;
		std::optional<std::uint64_t>       SlowestMicroseconds;
		std::vector<insights_event>        SecondaryLimits;
		std::vector<insights_github_point> Hour;
		std::vector<insights_github_point> Day;
		std::array<std::uint64_t, 5>       DayRequestsByCategory{ };
		std::uint64_t                      DayProcesses = 0;
		std::array<std::uint64_t, 9>       DayProcessFailures{ };
		std::uint64_t                      DayQuarantines = 0;
		std::uint64_t                      DayAbandoned   = 0;
		std::optional<std::uint64_t>       DaySlowestMicroseconds;
	};
	[[nodiscard]] auto build_insights_github(const std::vector<insights_event>& aEvents, insights_scope aScope, std::int64_t aNow) -> insights_github_snapshot;
	/// \brief History bucket width: raw samples for two hours, five minutes for 48 hours, daily for 93 days.
	[[nodiscard]] auto insights_history_width(std::int64_t aAt, std::int64_t aNow) noexcept -> std::int64_t;
}
#endif
