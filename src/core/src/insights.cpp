#include "bpr/core/insights.hpp"

#include <algorithm>
#include <cmath>
#include <map>

namespace bpr::core {
	auto insights_history_width(const std::int64_t aAt, const std::int64_t aNow) noexcept -> std::int64_t
	{
		const auto age = aNow - aAt;
		if (age > 93 * 86400LL || age < 0) {
			return 0;
		}
		return age < 7200 ? 1 : (age < 172800 ? 300 : 86400);
	}
	namespace {
		auto points(const std::vector<insights_event>& aEvents, const insights_scope aScope, const std::int64_t aNow, const std::int64_t aRange, const std::int64_t aStep) -> std::vector<insights_github_point>
		{
			std::map<std::int64_t, insights_github_point> buckets;
			std::int64_t                                  previousReset = 0;
			for (const auto& event : aEvents) {
				if (event.Scope != aScope || event.Kind != insights_event_kind::request || event.Resource != insights_resource::core || event.UnixSeconds < aNow - aRange || event.UnixSeconds > aNow) {
					continue;
				}
				const auto at     = event.UnixSeconds / aStep * aStep;
				auto&      point  = buckets[at];
				point.UnixSeconds = at;
				point.RequestsPerMinute += static_cast<double>(event.Count) * 60.0 / static_cast<double>(aStep);
				if (event.Remaining && event.Limit) {
					point.Remaining = static_cast<double>(*event.Remaining);
					point.Reset     = point.Reset || (previousReset != 0 && event.ResetUnixSeconds != previousReset);
					previousReset   = event.ResetUnixSeconds;
				}
			}
			std::vector<insights_github_point> result;
			result.reserve(buckets.size());
			for (const auto& [at, point] : buckets) {
				result.push_back(point);
			}
			return result;
		}
		void budgets(insights_github_snapshot& aResult, const std::vector<insights_event>& aEvents, const std::int64_t aNow)
		{
			for (const auto& event : aEvents) {
				if (event.Scope != aResult.Scope || event.Kind != insights_event_kind::request || event.UnixSeconds > aNow || !event.Limit || !event.Remaining || event.ResetUnixSeconds <= aNow || *event.Remaining > *event.Limit) {
					continue;
				}
				auto& budget = aResult.Budgets[static_cast<std::size_t>(event.Resource)];
				if (event.ResetUnixSeconds > budget.ResetUnixSeconds) {
					budget.Limit            = event.Limit;
					budget.Remaining        = event.Remaining;
					budget.ResetUnixSeconds = event.ResetUnixSeconds;
				} else if (event.ResetUnixSeconds == budget.ResetUnixSeconds) {
					budget.Limit     = event.Limit;
					budget.Remaining = std::min(*budget.Remaining, *event.Remaining);
				}
			}
			for (const auto& event : aEvents) {
				if (event.Scope != aResult.Scope || event.Kind != insights_event_kind::request || event.UnixSeconds > aNow) {
					continue;
				}
				auto&      budget = aResult.Budgets[static_cast<std::size_t>(event.Resource)];
				const auto period = event.Resource == insights_resource::search ? 60 : 3600;
				if (budget.Limit && (event.ResetUnixSeconds == budget.ResetUnixSeconds || (event.ResetUnixSeconds == 0 && event.UnixSeconds >= budget.ResetUnixSeconds - period))) {
					budget.OwnRequests += event.Count;
				}
			}
			auto& core = aResult.Budgets[0];
			if (core.Limit && core.Remaining) {
				const auto used   = *core.Limit > *core.Remaining ? *core.Limit - *core.Remaining : 0;
				core.OtherClients = used > core.OwnRequests ? used - core.OwnRequests : 0;
			}
		}
	}
	auto build_insights_github(const std::vector<insights_event>& aEvents, const insights_scope aScope, const std::int64_t aNow) -> insights_github_snapshot
	{
		insights_github_snapshot result;
		result.Available = true;
		result.Scope     = aScope;
		budgets(result, aEvents, aNow);
		for (const auto& event : aEvents) {
			if (event.UnixSeconds > aNow || event.UnixSeconds < aNow - 86400) {
				continue;
			}
			const auto category = static_cast<std::size_t>(event.Category);
			if (event.Kind == insights_event_kind::problem) {
				result.ProblemsByCategory[category] += event.Count;
			}
			if (event.Scope != aScope && event.Kind != insights_event_kind::process) {
				continue;
			}
			const auto hour = event.UnixSeconds >= aNow - 3600;
			if (event.Kind == insights_event_kind::request) {
				result.DayRequestsByCategory[category] += event.Count;
				if (hour) {
					result.RequestsByCategory[category] += event.Count;
				}
				if ((event.SecondaryLimited || event.RetryAfterSeconds) && result.SecondaryLimits.size() < 128) {
					result.SecondaryLimits.push_back(event);
				}
			}
			if (event.Kind == insights_event_kind::process) {
				result.DayProcesses += event.Launched ? event.Count : 0;
				result.DayProcessFailures[static_cast<std::size_t>(event.ProcessFailure)] += event.Count;
				result.DayQuarantines += event.Quarantined ? event.Count : 0;
				result.DayAbandoned += event.Abandoned ? event.Count : 0;
				if (event.DurationMicroseconds > 0) {
					result.DaySlowestMicroseconds = std::max(result.DaySlowestMicroseconds.value_or(0), event.DurationMicroseconds);
				}
				if (hour) {
					result.Processes += event.Launched ? event.Count : 0;
					result.ProcessFailures[static_cast<std::size_t>(event.ProcessFailure)] += event.Count;
					result.Quarantines += event.Quarantined ? event.Count : 0;
					result.Abandoned += event.Abandoned ? event.Count : 0;
					if (event.DurationMicroseconds > 0) {
						result.SlowestMicroseconds = std::max(result.SlowestMicroseconds.value_or(0), event.DurationMicroseconds);
					}
				}
			}
		}
		result.Hour = points(aEvents, aScope, aNow, 3600, 60);
		result.Day  = points(aEvents, aScope, aNow, 86400, 300);
		return result;
	}
	auto insights_review_identity_day(const insights_review_event& aEvent) noexcept -> std::int64_t
	{
		return aEvent.Kind == insights_review_kind::opened ? aEvent.LocalDay : 0;
	}
	auto insights_review_delta(const insights_review_event& aEvent) noexcept -> insights_review_counts
	{
		insights_review_counts result{ };
		const auto             add = [&](insights_review_metric aMetric, std::uint64_t aCount = 1) { result[static_cast<std::size_t>(aMetric)] += aCount; };
		using enum insights_review_kind;
		using metric = insights_review_metric;
		switch (aEvent.Kind) {
		case opened: add(metric::opened); break;
		case submitted:
			add(metric::submitted);
			add(aEvent.Disposition == 0 ? metric::approved : aEvent.Disposition == 1 ? metric::changes
																					 : metric::commented);
			break;
		case comment:
			add(metric::comments);
			if (aEvent.Suggestion) {
				add(metric::suggestions);
			}
			break;
		case reply:
			add(metric::replies);
			if (aEvent.Suggestion) {
				add(metric::suggestions);
			}
			break;
		case resolved: add(metric::resolved); break;
		case seen:     add(metric::seen); break;
		case accepted:
			add(metric::accepted);
			add(metric::lines, aEvent.Lines);
			break;
		case note:   add(metric::notes); break;
		case defect: add(metric::defects); break;
		case round:  add(metric::rounds); break;
		}
		return result;
	}
	auto build_insights_reviews(const std::vector<insights_review_bucket>& aBuckets, const std::vector<insights_review_pr>& aPullRequests, const std::int64_t aStartedDay, const std::int64_t aToday) -> insights_reviews_snapshot
	{
		insights_reviews_snapshot result;
		result.Available                             = true;
		result.StartedDay                            = aStartedDay;
		result.Today                                 = aToday;
		const auto                            monday = aToday - ((aToday + 3) % 7 + 7) % 7;
		std::map<std::int64_t, std::uint64_t> days;
		std::map<std::string, std::uint64_t>  repositories;
		constexpr auto                        submitted = static_cast<std::size_t>(insights_review_metric::submitted);
		for (const auto& bucket : aBuckets) {
			if (bucket.Day > aToday) {
				continue;
			}
			for (std::size_t index = 0; index < bucket.Counts.size(); ++index) {
				result.Ranges[2][index] += bucket.Counts[index];
				if (bucket.Day >= aToday - 29) {
					result.Ranges[1][index] += bucket.Counts[index];
				}
				if (bucket.Day >= monday) {
					result.Ranges[0][index] += bucket.Counts[index];
				}
			}
			days[bucket.Day] += bucket.Counts[submitted];
			repositories[bucket.Repository] += bucket.Counts[submitted];
		}
		std::vector<std::uint64_t> amounts;
		const auto                 first = monday - 25 * 7;
		for (auto day = first; day <= aToday; ++day) {
			amounts.push_back(days[day]);
		}
		auto sorted = amounts;
		std::ranges::sort(sorted);
		const auto percentile = sorted.empty() ? 0 : sorted[(sorted.size() * 9 + 9) / 10 - 1];
		for (auto day = first; day <= aToday; ++day) {
			const auto value = days[day];
			result.Calendar.push_back({ day, value, std::clamp(std::sqrt(static_cast<double>(value) / static_cast<double>(std::max<std::uint64_t>(1, percentile))), 0.16, 1.0) });
		}
		for (const auto& [day, count] : days) {
			if (count && (!result.Busiest || count > result.Busiest->Counts[submitted])) {
				insights_review_bucket busiest;
				busiest.Day               = day;
				busiest.Counts[submitted] = count;
				result.Busiest            = busiest;
			}
		}
		std::uint64_t most = 0;
		for (const auto& [repository, count] : repositories) {
			if (count > most) {
				most                          = count;
				result.MostReviewedRepository = repository;
			}
		}
		for (const auto& pull : aPullRequests) {
			if (!pull.Reviews) {
				continue;
			}
			if (!result.Biggest || pull.Files > result.Biggest->Files) {
				result.Biggest = pull;
			}
			if (!result.MostReviews || pull.Reviews > result.MostReviews->Reviews) {
				result.MostReviews = pull;
			}
		}
		return result;
	}

}
