#include "bpr/core/insights.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>

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
		// Sparse calendars have a zero 90th percentile. Use their positive peak
		// rather than saturating every active day at the same dot size.
		const auto scale = percentile == 0 && !sorted.empty() ? sorted.back() : percentile;
		for (auto day = first; day <= aToday; ++day) {
			const auto value = days[day];
			result.Calendar.push_back({ day, value, std::clamp(std::sqrt(static_cast<double>(value) / static_cast<double>(std::max<std::uint64_t>(1, scale))), 0.16, 1.0) });
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

	namespace {
		void add_energy(insights_energy_sample& aTo, const insights_energy_sample& aFrom)
		{
			aTo.Samples += aFrom.Samples;
			aTo.WallMicroseconds += aFrom.WallMicroseconds;
			aTo.CpuMicroseconds += aFrom.CpuMicroseconds;
			if (aFrom.Wakeups) {
				aTo.Wakeups = aTo.Wakeups.value_or(0) + *aFrom.Wakeups;
				aTo.WakeupWallMicroseconds += aFrom.WakeupWallMicroseconds;
			}
			for (std::size_t index = 0; index < 2; ++index) {
				aTo.ChildCpuMicroseconds[index] += aFrom.ChildCpuMicroseconds[index];
				aTo.Children[index] += aFrom.Children[index];
				aTo.UnmeasuredChildren[index] += aFrom.UnmeasuredChildren[index];
			}
			aTo.PeakLoad = std::max(aTo.PeakLoad, aFrom.PeakLoad);
		}
		auto energy_points(const std::vector<insights_energy_sample>& aSamples, std::int64_t aNow, std::int64_t aRange, std::int64_t aWidth) -> std::vector<insights_energy_point>
		{
			std::map<std::int64_t, std::array<insights_energy_sample, 2>> buckets;
			std::map<std::int64_t, std::pair<std::int64_t, std::int64_t>> spans;
			for (const auto& sample : aSamples) {
				const auto at = sample.AtMicroseconds / 1000000;
				if (at > aNow || at <= aNow - aRange || sample.WallMicroseconds == 0) {
					continue;
				}
				const auto bucketAt = at / aWidth * aWidth;
				add_energy(buckets[bucketAt][sample.Foreground ? 1 : 0], sample);
				const auto begin      = sample.WidthSeconds == 1 ? sample.AtMicroseconds - static_cast<std::int64_t>(sample.WallMicroseconds) : sample.AtMicroseconds;
				const auto end        = sample.WidthSeconds == 1 ? sample.AtMicroseconds : sample.AtMicroseconds + sample.WidthSeconds * 1000000;
				auto [span, inserted] = spans.try_emplace(bucketAt, std::pair{ begin, end });
				if (!inserted) {
					span->second.first  = std::min(span->second.first, begin);
					span->second.second = std::max(span->second.second, end);
				}
			}
			std::vector<insights_energy_point> result;
			std::optional<std::int64_t>        previousEnd;
			for (const auto& [at, sides] : buckets) {
				insights_energy_point point;
				point.UnixSeconds = at;
				point.GapBefore   = previousEnd && spans.at(at).first > *previousEnd;
				previousEnd       = spans.at(at).second;
				const auto wall   = sides[0].WallMicroseconds + sides[1].WallMicroseconds;
				// One common measured duration makes the two quantities a truthful stack.
				point.Background = static_cast<double>(sides[0].CpuMicroseconds) * 3600.0 / static_cast<double>(wall);
				point.Foreground = static_cast<double>(sides[1].CpuMicroseconds) * 3600.0 / static_cast<double>(wall);
				if (sides[0].WallMicroseconds) {
					point.BackgroundBudget = 2.0 * static_cast<double>(sides[0].WallMicroseconds) / static_cast<double>(wall);
				}
				auto total = sides[0];
				add_energy(total, sides[1]);
				point.WakeupsPerMinute = insights_wakeups_per_minute(total);
				result.push_back(point);
			}
			return result;
		}
	}
	auto insights_energy_delta(const insights_process_counters& aBefore, const insights_process_counters& aAfter, const bool aForeground, const std::int64_t aLocalDay) -> std::optional<insights_energy_sample>
	{
		if (aAfter.SteadyMicroseconds <= aBefore.SteadyMicroseconds || aAfter.CpuMicroseconds < aBefore.CpuMicroseconds || (aBefore.Wakeups && aAfter.Wakeups && *aAfter.Wakeups < *aBefore.Wakeups)) {
			return std::nullopt;
		}
		insights_energy_sample sample;
		sample.AtMicroseconds   = aAfter.UnixMicroseconds;
		sample.LocalDay         = aLocalDay;
		sample.Foreground       = aForeground;
		sample.Samples          = 1;
		sample.WallMicroseconds = static_cast<std::uint64_t>(aAfter.SteadyMicroseconds - aBefore.SteadyMicroseconds);
		sample.CpuMicroseconds  = aAfter.CpuMicroseconds - aBefore.CpuMicroseconds;
		sample.PeakLoad         = static_cast<double>(sample.CpuMicroseconds) / static_cast<double>(sample.WallMicroseconds);
		if (aBefore.Wakeups && aAfter.Wakeups) {
			sample.Wakeups                = *aAfter.Wakeups - *aBefore.Wakeups;
			sample.WakeupWallMicroseconds = sample.WallMicroseconds;
		}
		return sample;
	}
	auto insights_cpu_per_hour(const insights_energy_sample& aSample) noexcept -> std::optional<double>
	{
		return aSample.WallMicroseconds ? std::optional{ static_cast<double>(aSample.CpuMicroseconds) * 3600.0 / static_cast<double>(aSample.WallMicroseconds) } : std::nullopt;
	}
	auto insights_wakeups_per_minute(const insights_energy_sample& aSample) noexcept -> std::optional<double>
	{
		return aSample.Wakeups && aSample.WakeupWallMicroseconds ? std::optional{ static_cast<double>(*aSample.Wakeups) * 60000000.0 / static_cast<double>(aSample.WakeupWallMicroseconds) } : std::nullopt;
	}
	auto rollup_insights_energy(const std::vector<insights_energy_sample>& aSamples, const std::int64_t aNow) -> std::vector<insights_energy_sample>
	{
		std::map<std::tuple<std::int64_t, bool, std::int64_t>, insights_energy_sample> buckets;
		for (const auto& sample : aSamples) {
			const auto width = insights_history_width(sample.AtMicroseconds / 1000000, aNow);
			if (!width) {
				continue;
			}
			// Raw keeps microsecond transition identities; older buckets retain the measured sums.
			const auto at         = width == 1 ? sample.AtMicroseconds : sample.AtMicroseconds / (width * 1000000) * (width * 1000000);
			auto&      bucket     = buckets[{ at, sample.Foreground, sample.LocalDay }];
			bucket.AtMicroseconds = at;
			bucket.LocalDay       = sample.LocalDay;
			bucket.WidthSeconds   = width;
			bucket.Foreground     = sample.Foreground;
			add_energy(bucket, sample);
		}
		std::vector<insights_energy_sample> result;
		result.reserve(buckets.size());
		for (const auto& [key, sample] : buckets) {
			result.push_back(sample);
		}
		return result;
	}
	auto build_insights_energy(const std::vector<insights_energy_sample>& aSamples, const std::int64_t aNow, const std::int64_t aToday) -> insights_energy_snapshot
	{
		insights_energy_snapshot result;
		result.Available = !aSamples.empty();
		result.Now       = aNow;
		result.Today     = aToday;
		std::map<std::int64_t, std::array<insights_energy_sample, 2>> days;
		for (const auto& sample : aSamples) {
			const auto at = sample.AtMicroseconds / 1000000;
			if (at > aNow || at < aNow - 93 * 86400LL) {
				continue;
			}
			for (std::size_t range = 0; range < 3; ++range) {
				const auto seconds = range == 0 ? 3600LL : range == 1 ? 86400LL
																	  : 93 * 86400LL;
				if (at > aNow - seconds) {
					add_energy(result.Ranges[range][sample.Foreground ? 1 : 0], sample);
				}
			}
			if (sample.LocalDay <= aToday && sample.LocalDay >= aToday - 92) {
				add_energy(days[sample.LocalDay][sample.Foreground ? 1 : 0], sample);
			}
		}
		result.Hour = energy_points(aSamples, aNow, 3600, 60);
		result.Day  = energy_points(aSamples, aNow, 86400, 300);
		std::vector<double> amounts;
		const auto          first = aToday - 92 - ((aToday - 92 + 3) % 7 + 7) % 7;
		for (auto day = first; day <= aToday; ++day) {
			const auto&         sides = days[day];
			insights_energy_day point;
			point.Day             = day;
			point.CpuMicroseconds = sides[0].CpuMicroseconds + sides[1].CpuMicroseconds;
			point.HasSamples      = sides[0].Samples + sides[1].Samples != 0;
			if (const auto idle = insights_cpu_per_hour(sides[0])) {
				point.BackgroundIntensity = std::clamp(*idle / 2.0, 0.0, 1.0);
			}
			amounts.push_back(static_cast<double>(point.CpuMicroseconds));
			result.Calendar.push_back(point);
		}
		std::ranges::sort(amounts);
		auto percentile = amounts.empty() ? 0.0 : amounts[(amounts.size() - 1) * 9 / 10];
		if (percentile == 0 && !amounts.empty()) {
			percentile = amounts.back();
		}
		for (auto& point : result.Calendar) {
			point.Size = point.CpuMicroseconds && percentile > 0 ? std::clamp(std::sqrt(static_cast<double>(point.CpuMicroseconds) / percentile), 0.16, 1.0) : 0.16;
		}
		return result;
	}

}
