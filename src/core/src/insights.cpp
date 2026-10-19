#include "bpr/core/insights.hpp"

#include <algorithm>
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
}
