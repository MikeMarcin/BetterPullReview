#ifndef BPR_CORE_INSIGHTS_HPP
#define BPR_CORE_INSIGHTS_HPP

#include <array>
#include <cstdint>
#include <optional>
#include <string>
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
	/// \brief Successful local activity; all identity stays in the private profile store.
	enum class insights_review_kind : std::uint8_t { opened,
													 submitted,
													 comment,
													 reply,
													 resolved,
													 seen,
													 accepted,
													 note,
													 defect,
													 round };
	enum class insights_review_metric : std::uint8_t { opened,
													   submitted,
													   approved,
													   changes,
													   commented,
													   comments,
													   replies,
													   suggestions,
													   resolved,
													   seen,
													   accepted,
													   lines,
													   notes,
													   defects,
													   rounds,
													   count };
	using insights_review_counts = std::array<std::uint64_t, static_cast<std::size_t>(insights_review_metric::count)>;
	/// \brief Event identity is scoped by host, repository and PR; file events also bind path and head.
	struct insights_review_event
	{
		insights_review_kind Kind = insights_review_kind::opened;
		std::string          Host, Repository, Identity, Head;
		std::uint64_t        PullRequest = 0, FilesChanged = 0, Lines = 0;
		std::int64_t         UnixSeconds = 0, LocalDay = 0;
		std::uint8_t         Disposition = 2;
		bool                 Suggestion  = false;
	};
	/// \brief Once-per-day applies only to openings; other durable identities never expire.
	[[nodiscard]] auto insights_review_identity_day(const insights_review_event& aEvent) noexcept -> std::int64_t;
	[[nodiscard]] auto insights_review_delta(const insights_review_event& aEvent) noexcept -> insights_review_counts;
	/// \brief Aggregated day/repository facts and submitted-PR records, supplied by storage.
	struct insights_review_bucket
	{
		std::int64_t           Day = 0;
		std::string            Repository;
		insights_review_counts Counts{ };
	};
	struct insights_review_pr
	{
		std::string   Host, Repository;
		std::uint64_t Number = 0, Reviews = 0, Files = 0;
	};
	struct insights_review_calendar_day
	{
		std::int64_t  Day     = 0;
		std::uint64_t Reviews = 0;
		double        Size    = 0.0;
	};
	/// \brief Lifetime and local-calendar ranges. Day numbers are Gregorian days since Unix epoch.
	struct insights_reviews_snapshot
	{
		bool                                      Available  = false;
		std::int64_t                              StartedDay = 0, Today = 0;
		std::array<insights_review_counts, 3>     Ranges{ };
		std::vector<insights_review_calendar_day> Calendar;
		std::optional<insights_review_pr>         Biggest, MostReviews;
		std::optional<insights_review_bucket>     Busiest;
		std::string                               MostReviewedRepository;
	};
	[[nodiscard]] auto build_insights_reviews(const std::vector<insights_review_bucket>& aBuckets, const std::vector<insights_review_pr>& aPullRequests, std::int64_t aStartedDay, std::int64_t aToday) -> insights_reviews_snapshot;

	/// \brief Cumulative counters supplied by a native adapter. The host supplies both clocks.
	struct insights_process_counters
	{
		std::int64_t                 SteadyMicroseconds = 0, UnixMicroseconds = 0;
		std::uint64_t                CpuMicroseconds = 0;
		std::optional<std::uint64_t> Wakeups;
	};
	/// \brief A retained interval or rollup. Child facts have no application wall time or samples.
	struct insights_energy_sample
	{
		std::int64_t                 AtMicroseconds = 0, LocalDay = 0, WidthSeconds = 1;
		bool                         Foreground = false;
		std::uint64_t                Samples = 0, WallMicroseconds = 0, CpuMicroseconds = 0;
		std::optional<std::uint64_t> Wakeups;
		std::uint64_t                WakeupWallMicroseconds = 0;
		std::array<std::uint64_t, 2> ChildCpuMicroseconds{ }, Children{ }, UnmeasuredChildren{ };
		double                       PeakLoad = 0;
	};
	[[nodiscard]] auto insights_energy_delta(const insights_process_counters& aBefore, const insights_process_counters& aAfter, bool aForeground, std::int64_t aLocalDay) -> std::optional<insights_energy_sample>;
	[[nodiscard]] auto rollup_insights_energy(const std::vector<insights_energy_sample>& aSamples, std::int64_t aNow) -> std::vector<insights_energy_sample>;
	[[nodiscard]] auto insights_cpu_per_hour(const insights_energy_sample& aSample) noexcept -> std::optional<double>;
	[[nodiscard]] auto insights_wakeups_per_minute(const insights_energy_sample& aSample) noexcept -> std::optional<double>;
	struct insights_energy_point
	{
		std::int64_t          UnixSeconds = 0;
		std::optional<double> Background, Foreground, WakeupsPerMinute, BackgroundBudget;
		bool                  GapBefore = false;
	};
	struct insights_energy_day
	{
		std::int64_t          Day             = 0;
		std::uint64_t         CpuMicroseconds = 0;
		bool                  HasSamples      = false;
		double                Size            = 0;
		std::optional<double> BackgroundIntensity;
	};
	struct insights_energy_snapshot
	{
		bool         Available = false;
		std::int64_t Now = 0, Today = 0;
		// Each range pairs background [0] and foreground [1].
		std::array<std::array<insights_energy_sample, 2>, 3> Ranges{ };
		std::vector<insights_energy_point>                   Hour, Day;
		std::vector<insights_energy_day>                     Calendar;
	};
	[[nodiscard]] auto build_insights_energy(const std::vector<insights_energy_sample>& aSamples, std::int64_t aNow, std::int64_t aToday) -> insights_energy_snapshot;

	/// \brief Passive work observations; paths are local display data and never exported.
	enum class insights_work_kind : std::uint8_t { latency,
												   diff,
												   syntax_hit,
												   syntax_miss,
												   syntax_eviction,
												   search };
	struct insights_work_sample
	{
		std::int64_t       At    = 0;
		insights_work_kind Kind  = insights_work_kind::latency;
		std::uint64_t      Count = 1, DurationMicroseconds = 0;
		std::string        Path;
		bool               WorkLimit = false, SegmentLimit = false, Approximate = false;
	};
	struct insights_slow_file
	{
		std::string   Path;
		std::uint64_t Microseconds = 0;
	};
	/// \brief Exact weighted duration distributions over retained observation buckets.
	struct insights_work_range
	{
		std::uint64_t                   Latencies = 0, Stalls = 0, LongestMicroseconds = 0;
		std::uint64_t                   Files = 0, DiffMicroseconds = 0, WorkLimits = 0, SegmentLimits = 0, Approximate = 0;
		std::uint64_t                   SyntaxHits = 0, SyntaxMisses = 0, SyntaxEvictions = 0, Searches = 0;
		std::optional<std::uint64_t>    P95Microseconds, SearchMedianMicroseconds;
		std::vector<insights_slow_file> Slowest;
	};
	struct insights_work_snapshot
	{
		std::array<insights_work_range, 2> Ranges;
	};
	/// \brief Keeps five-minute history for48h and daily history through93days.
	[[nodiscard]] auto rollup_insights_work(const std::vector<insights_work_sample>& aSamples, std::int64_t aNow) -> std::vector<insights_work_sample>;
	[[nodiscard]] auto build_insights_work(const std::vector<insights_work_sample>& aSamples, std::int64_t aNow) -> insights_work_snapshot;

}
#endif
