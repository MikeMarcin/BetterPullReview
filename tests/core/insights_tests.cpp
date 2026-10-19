#include "../support/test_expectation.hpp"
#include "bpr/core/insights.hpp"
#include <iostream>
int main()
{
	using namespace bpr;
	using tests::expect;
	const auto scope         = core::insights_scope{ 1, 7 };
	auto       request       = core::insights_event{ };
	request.Scope            = scope;
	request.UnixSeconds      = 1000;
	request.Limit            = 5000;
	request.Remaining        = 3760;
	request.ResetUnixSeconds = 4000;
	std::vector<core::insights_event> events{ request };
	request.UnixSeconds = 1010;
	request.Remaining   = 3900;
	events.push_back(request);
	request.UnixSeconds = 1020;
	request.Limit.reset();
	request.Remaining.reset();
	request.ResetUnixSeconds = 0;
	events.push_back(request);
	auto github = core::build_insights_github(events, scope, 1100);
	expect(github.Budgets[0].Remaining == 3760 && github.Budgets[0].OwnRequests == 3 && github.Budgets[0].OtherClients == 1237, "the lowest reading, rather than latest, derives other clients and headerless requests count as BPR");
	request.Scope.Account    = 8;
	request.Limit            = 5000;
	request.Remaining        = 100;
	request.ResetUnixSeconds = 4000;
	events.push_back(request);
	expect(core::build_insights_github(events, scope, 1100).Budgets[0].OtherClients == 1237, "another account cannot change this budget");
	expect(!core::build_insights_github(events, scope, 4001).Budgets[0].Limit, "expired readings produce no current-window number");
	request.Scope            = scope;
	request.UnixSeconds      = 4010;
	request.ResetUnixSeconds = 7600;
	request.Remaining        = 4999;
	events.push_back(request);
	github = core::build_insights_github(events, scope, 4020);
	expect(github.Budgets[0].OwnRequests == 1 && github.Budgets[0].OtherClients == 0, "reset crossing starts a fresh persisted request window");
	request.Resource = core::insights_resource::graphql;
	events.push_back(request);
	github = core::build_insights_github(events, scope, 4020);
	expect(github.Budgets[1].OwnRequests == 1 && !github.Budgets[1].OtherClients, "GraphQL retains request counts without a cost split");
	expect(core::insights_history_width(0, 7200) == 300 && core::insights_history_width(0, 172800) == 86400 && core::insights_history_width(0, 94 * 86400LL) == 0, "retention changes at the supplied boundaries");
	using metric = core::insights_review_metric;
	core::insights_review_event activity;
	activity.Kind  = core::insights_review_kind::accepted;
	activity.Lines = 97;
	expect(core::insights_review_delta(activity)[static_cast<std::size_t>(metric::accepted)] == 1 && core::insights_review_delta(activity)[static_cast<std::size_t>(metric::lines)] == 97, "acceptance counts changed lines once with the file event");
	activity.LocalDay = 20690;
	expect(core::insights_review_identity_day(activity) == 0, "file dedup is lifetime, independent of local day");
	activity.Kind = core::insights_review_kind::opened;
	expect(core::insights_review_identity_day(activity) == activity.LocalDay, "opening dedup is scoped to the supplied civil day");
	std::vector<core::insights_review_bucket> reviewDays;
	for (const auto day : { 20665LL, 20689LL, 20690LL, 20691LL }) {
		core::insights_review_bucket bucket;
		bucket.Day                                                 = day;
		bucket.Repository                                          = "owner/repo";
		bucket.Counts[static_cast<std::size_t>(metric::submitted)] = day == 20691 ? 999 : 2;
		reviewDays.push_back(bucket);
	}
	const auto reviews = core::build_insights_reviews(reviewDays, { { "github.com", "owner/repo", 8, 4, 120 } }, 20600, 20690);
	expect(reviews.Ranges[0][static_cast<std::size_t>(metric::submitted)] == 4 && reviews.Ranges[1][static_cast<std::size_t>(metric::submitted)] == 6 && reviews.Ranges[2][static_cast<std::size_t>(metric::submitted)] == 6, "local Monday and30-day inclusive ranges exclude future days");
	expect(reviews.Calendar.size() >= 176 && reviews.Calendar.size() <= 182 && (reviews.Calendar.front().Day + 3) % 7 == 0 && reviews.Calendar.back().Day == 20690, "calendar spans26 Monday-first weeks through today");
	expect(reviews.Biggest->Files == 120 && reviews.MostReviews->Reviews == 4 && reviews.Busiest->Counts[static_cast<std::size_t>(metric::submitted)] == 2 && reviews.MostReviewedRepository == "owner/repo", "records use submitted reviews and aggregate daily activity");

	auto sparseDays                                                   = reviewDays;
	sparseDays[1].Counts[static_cast<std::size_t>(metric::submitted)] = 100;
	const auto sparse                                                 = core::build_insights_reviews(sparseDays, { }, 20600, 20690);
	expect(sparse.Calendar[sparse.Calendar.size() - 2].Size == 1.0 && sparse.Calendar.back().Size == 0.16, "a sparse zero-percentile range uses its positive peak and preserves the minimum dot size");

	core::insights_process_counters before;
	before.SteadyMicroseconds = 1000000;
	before.UnixMicroseconds   = 1000000000;
	before.CpuMicroseconds    = 100;
	before.Wakeups            = 10;
	auto after                = before;
	after.SteadyMicroseconds += 60000000;
	after.UnixMicroseconds += 60000000;
	after.CpuMicroseconds += 100000;
	after.Wakeups    = 16;
	const auto delta = core::insights_energy_delta(before, after, false, 20690);
	expect(delta && !delta->Foreground && delta->WallMicroseconds == 60000000 && delta->CpuMicroseconds == 100000 && delta->Wakeups == 6, "a cumulative sample preserves old-state wall, CPU and wakeups");
	expect(core::insights_cpu_per_hour(*delta) == 6.0 && core::insights_wakeups_per_minute(*delta) == 6.0, "idle cost uses measured background duration, with explicit budgets");
	expect(!core::insights_energy_delta(after, before, false, 0) && !core::insights_energy_delta(before, before, false, 0), "nonpositive duration and decreasing CPU are dropped");
	auto invalid    = after;
	invalid.Wakeups = 9;
	expect(!core::insights_energy_delta(before, invalid, false, 0), "decreasing wakeup counters drop the entire interval");
	invalid                 = after;
	invalid.CpuMicroseconds = 1;
	expect(!core::insights_energy_delta(before, invalid, false, 0), "negative CPU deltas are dropped independently of duration");
	invalid = after;
	invalid.Wakeups.reset();
	expect(!core::insights_energy_delta(before, invalid, false, 0)->Wakeups, "a platform without wakeups never supplies measured zero");
	std::vector<core::insights_energy_sample> energy;
	constexpr auto                            energyNow = 20000000LL;
	for (const auto age : { 60LL, 7200LL, 7201LL, 172800LL, 172801LL, 94 * 86400LL }) {
		auto sample           = *delta;
		sample.AtMicroseconds = (energyNow - age) * 1000000;
		sample.LocalDay       = 20690 - age / 86400;
		energy.push_back(sample);
	}
	const auto retained = core::rollup_insights_energy(energy, energyNow);
	expect(retained.size() == 3 && retained.back().WidthSeconds == 1 && retained.front().WidthSeconds == 86400, "raw, five-minute and daily retention drops expired history and combines the same bucket");
	const auto projected = core::build_insights_energy(energy, energyNow, 20690);
	expect(projected.Ranges[0][0].CpuMicroseconds == 100000 && projected.Ranges[0][1].Samples == 0 && projected.Ranges[1][0].Samples == 3, "ranges keep foreground/background totals separate and exclude old samples");
	expect(projected.Calendar.size() >= 93 && (projected.Calendar.front().Day + 3) % 7 == 0 && projected.Calendar.back().Day == 20690, "energy calendar uses local days and Monday-first weeks");

	auto gapStart           = *delta;
	gapStart.AtMicroseconds = (energyNow - 180) * 1000000;
	auto gapEnd             = *delta;
	gapEnd.AtMicroseconds   = (energyNow - 30) * 1000000;
	const auto gaps         = core::build_insights_energy({ gapStart, gapEnd }, energyNow, 20690);
	expect(gaps.Hour.size() == 2 && gaps.Hour.back().GapBefore, "an actual unsampled span is preserved even when chart slot widths alone would connect it");

	auto mixedBg             = *delta;
	mixedBg.AtMicroseconds   = (energyNow - 60) * 1000000;
	mixedBg.WallMicroseconds = 30000000;
	mixedBg.CpuMicroseconds  = 33334;
	auto mixedFg             = mixedBg;
	mixedFg.Foreground       = true;
	mixedFg.CpuMicroseconds  = 1000000;
	const auto mixed         = core::build_insights_energy({ mixedBg, mixedFg }, energyNow, 20690);
	expect(mixed.Hour.front().BackgroundBudget == 1.0 && *mixed.Hour.front().Background > *mixed.Hour.front().BackgroundBudget && *core::insights_cpu_per_hour(mixedBg) > 2.0, "the background budget line uses the same measured-duration denominator as the stack, so mixed-state intervals agree with the idle tile");

	std::cout << "insights-core:pass\n";
}
