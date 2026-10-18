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
	std::cout << "insights-core:pass\n";
}
