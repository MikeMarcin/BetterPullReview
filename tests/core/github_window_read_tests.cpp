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
	return EXIT_SUCCESS;
}
