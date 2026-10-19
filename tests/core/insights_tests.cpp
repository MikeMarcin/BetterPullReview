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

	std::cout << "insights-core:pass\n";
}
