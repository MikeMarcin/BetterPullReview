#include "bpr/core/cached_review_overview.hpp"
#include "bpr/core/github_window_read.hpp"
#include <cstdlib>
int main()
{
	using namespace bpr::core;
	const bpr_window_context first{ bpr_profile_id{ 1 }, bpr_window_id{ 1 }, bpr_window_generation{ 1 } };
	github_window_read       lane{ first, application_session_id{ 1 } };
	github_window_session    session{ .Profile = bpr_profile_id{ 1 }, .Identity = { .Context = { request_id{ 1 }, application_session_id{ 1 }, host_profile_id{ 1 }, github_identity_generation{ 1 } }, .Host = git_host_id{ 1 }, .Account = github_account_id{ 1 }, .CapabilityRevision = capability_revision{ 1 } } };
	if (lane.adoption() || !lane.adopt(session)) {
		return EXIT_FAILURE;
	}
	// [[BPR-1017]]. Adoption keeps what it decided, so a window attached after the
	// profile's acceptance can present the session before it reads. The lane's
	// first change of state ends that.
	const auto& adopted = lane.adoption();
	if (!adopted || !adopted->AcceptedSession || !adopted->Requests.empty()
		|| adopted->Status != github_status_delta{ .Status = github_status::session_ready }
		|| !adopted->Identity || adopted->Identity->Account != github_account_id{ 1 }
		|| !lane.adopt(session) || !lane.adoption()) {
		return EXIT_FAILURE;
	}
	const auto read = lane.apply(first, open_pull_request_intent{ { "owner", "repo" }, pull_request_number{ 1 } });
	if (!read.Fetch || !read.Projection.Requests.empty() || lane.adoption()) {
		return EXIT_FAILURE;
	}
	const auto&            cachedRequest = read.Fetch->Context;
	cached_review_overview cached{
		.Key         = { .Authority = { 1 }, .Profile = cachedRequest.Profile, .Host = cachedRequest.Host, .Account = cachedRequest.Account, .Locator = cachedRequest.Repository, .Number = cachedRequest.Number },
		.Repository  = repository_id{ 5 },
		.PullRequest = pull_request_id{ 6 },
		.BaseOid     = std::string(40, '1'),
		.HeadOid     = std::string(40, '2')
	};
	const auto admits = [&](const auto& aState, const auto& aRecord) {
		return admit_cached_overview(aState, cachedRequest, github_failure::network, cached.Key, aRecord);
	};
	if (!admits(lane.state(), cached)) {
		return EXIT_FAILURE;
	}
	for (auto failure : { github_failure::authentication, github_failure::permission, github_failure::certificate,
						  github_failure::redirect, github_failure::malformed_response, github_failure::rate_limit }) {
		if (admit_cached_overview(lane.state(), cachedRequest, failure, cached.Key, cached)) {
			return EXIT_FAILURE;
		}
	}
	const auto refuses = [&](auto aChange) { auto candidate = cached; aChange(candidate); return !admits(lane.state(), candidate); };
	if (!refuses([](auto& aValue) { aValue.Key.Account = github_account_id{ 9 }; })
		|| !refuses([](auto& aValue) { aValue.Key.Authority[0] = 9; })
		|| !refuses([](auto& aValue) { aValue.Key.Locator.Name = "foreign"; })
		|| !refuses([](auto& aValue) { aValue.Key.Number = pull_request_number{ 9 }; })
		|| !refuses([](auto& aValue) { aValue.HeadOid = "invalid"; })) {
		return EXIT_FAILURE;
	}
	auto changedState             = lane.state();
	changedState.AcceptedSnapshot = provider_snapshot_id{ 1 };
	if (admits(changedState, cached)) {
		return EXIT_FAILURE;
	}
	changedState                    = lane.state();
	changedState.IdentityGeneration = github_identity_generation{ 2 };
	if (admits(changedState, cached)) {
		return EXIT_FAILURE;
	}
	changedState = lane.state();
	changedState.PendingPullRequest.reset();
	if (admits(changedState, cached)) {
		return EXIT_FAILURE;
	}
	// Catalogue identity is the complete atomic overview identity, never a separate lookup.
	cached.Files         = 1;
	cached.FileCatalogue = std::vector<cached_review_file>{ { "src/file.cpp", "old.cpp", cached_file_change::renamed, 2, 1 } };
	if (!admits(lane.state(), cached)
		|| !refuses([](auto& aValue) { aValue.FileCatalogue->front().Path.clear(); })
		|| !refuses([](auto& aValue) { aValue.Files = 0; })
		|| !refuses([](auto& aValue) { aValue.BaseOid = "invalid"; })) {
		return EXIT_FAILURE;
	}
	auto numericRequest               = cachedRequest;
	numericRequest.ExpectedRepository = repository_id{ 99 };
	auto numericState                 = lane.state();
	numericState.PendingPullRequest   = numericRequest;
	if (admit_cached_overview(numericState, numericRequest, github_failure::network, cached.Key, cached)) {
		return EXIT_FAILURE;
	}
	numericRequest.ExpectedRepository  = cached.Repository;
	numericRequest.ExpectedPullRequest = pull_request_id{ 99 };
	numericState.PendingPullRequest    = numericRequest;
	if (admit_cached_overview(numericState, numericRequest, github_failure::network, cached.Key, cached)) {
		return EXIT_FAILURE;
	}
	const auto beforeObservation = lane.state();
	auto       wrong             = first;
	wrong.Window                 = bpr_window_id{ 99U };
	auto stale                   = read.Fetch->Context;
	stale.Request                = request_id{ 99U };
	if (lane.accept_overview_observation(wrong, read.Fetch->Context)
		|| lane.accept_overview_observation(first, stale)
		|| !lane.accept_overview_observation(first, read.Fetch->Context)
		|| lane.accept_overview_observation(first, read.Fetch->Context)
		|| lane.state() != beforeObservation) {
		return EXIT_FAILURE;
	}
	static_cast<void>(lane.apply(first, pull_request_load_cancelled_event{ read.Fetch->Context }));
	if (lane.accept_overview_observation(first, read.Fetch->Context)) {
		return EXIT_FAILURE;
	}
	const auto next = lane.apply(first, open_pull_request_intent{ { "owner", "other" }, pull_request_number{ 2U } });
	if (!next.Fetch || !lane.accept_overview_observation(first, next.Fetch->Context)
		|| lane.accept_overview_observation(first, read.Fetch->Context)) {
		return EXIT_FAILURE;
	}
	if (!lane.retire(session) || lane.adopt(session)) {
		return EXIT_FAILURE;
	}
	github_window_read unread{ first, application_session_id{ 2 } };
	if (!unread.adopt(session) || !unread.adoption() || !unread.retire(session) || unread.adoption()) {
		return EXIT_FAILURE;
	}
	const auto late = lane.apply(first, pull_request_loaded_event{ read.Fetch->Context, repository_id{ 1 }, pull_request_id{ 1 }, provider_snapshot_id{ 1 } });
	if (late.Projection.AcceptedSnapshot) {
		return EXIT_FAILURE;
	}
	github_window_read       source{ first, application_session_id{ 10 } };
	const bpr_window_context second{ first.Profile, bpr_window_id{ 2 }, first.Generation };
	github_window_read       target{ second, application_session_id{ 20 } };
	if (!source.adopt(session) || !target.adopt(session)) {
		return EXIT_FAILURE;
	}
	const auto load = [](github_window_read& aLane, const std::uint32_t aNumber, const std::uint64_t aSnapshot) {
		const auto request = aLane.apply(aLane.window(), open_pull_request_intent{ { "owner", "repo" }, pull_request_number{ aNumber } });
		return request.Fetch && aLane.apply(aLane.window(), pull_request_loaded_event{ request.Fetch->Context, repository_id{ 1 }, pull_request_id{ aNumber }, provider_snapshot_id{ aSnapshot } }).Projection.AcceptedSnapshot.has_value();
	};
	if (!load(source, 1, 11) || !load(target, 1, 22)) {
		return EXIT_FAILURE;
	}
	auto retained             = target.state();
	retained.AcceptedSnapshot = source.state().AcceptedSnapshot;
	if (!target.share_snapshot(source, 1) || target.state() != retained
		|| target.share_snapshot(source, 1) || target.share_snapshot(source, 0)) {
		return EXIT_FAILURE;
	}
	// A pending refresh is still owned by its own exact request, and pending
	// navigation, another PR/account/profile and retired sessions never borrow.
	static_cast<void>(target.apply(second, refresh_pull_request_intent{ }));
	if (target.share_snapshot(source, 2) || !target.state().PendingPullRequest) {
		return EXIT_FAILURE;
	}
	static_cast<void>(target.apply(second, pull_request_load_cancelled_event{ *target.state().PendingPullRequest }));
	if (!load(target, 2, 33)
		|| target.share_snapshot(source, 3) || !load(target, 1, 44)) {
		return EXIT_FAILURE;
	}
	auto foreignSession    = session;
	foreignSession.Profile = bpr_profile_id{ 2 };
	github_window_read foreign{ { bpr_profile_id{ 2 }, bpr_window_id{ 3 }, bpr_window_generation{ 1 } }, application_session_id{ 30 } };
	if (!foreign.adopt(foreignSession) || !load(foreign, 1, 55) || target.share_snapshot(foreign, 4)) {
		return EXIT_FAILURE;
	}
	foreignSession                  = session;
	foreignSession.Identity.Account = github_account_id{ 2 };
	github_window_read account{ { first.Profile, bpr_window_id{ 4 }, first.Generation }, application_session_id{ 40 } };
	if (!account.adopt(foreignSession) || !load(account, 1, 66) || target.share_snapshot(account, 5)
		|| !source.retire(session) || target.share_snapshot(source, 6)) {
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
