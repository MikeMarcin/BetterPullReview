#include "bpr/core/github_window_read.hpp"
#include <cstdlib>
int main()
{
	using namespace bpr::core;
	const bpr_window_context first{ bpr_profile_id{ 1 }, bpr_window_id{ 1 }, bpr_window_generation{ 1 } };
	github_window_read       lane{ first, application_session_id{ 1 } };
	github_window_session    session{ .Profile = bpr_profile_id{ 1 }, .Identity = { .Context = { request_id{ 1 }, application_session_id{ 1 }, host_profile_id{ 1 }, github_identity_generation{ 1 } }, .Host = git_host_id{ 1 }, .Account = github_account_id{ 1 }, .CapabilityRevision = capability_revision{ 1 } } };
	if (!lane.adopt(session)) {
		return EXIT_FAILURE;
	}
	const auto read = lane.apply(first, open_pull_request_intent{ { "owner", "repo" }, pull_request_number{ 1 } });
	if (!read.Fetch || !read.Projection.Requests.empty()) {
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
