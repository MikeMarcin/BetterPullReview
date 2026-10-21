// [[BPR-1357]]. What Home shows of a pull request it was asked to open, decided
// from the GitHub transition's own read identity: opening at once, waiting
// after the application's wait, failed with its reason, and nothing once the
// read loaded or was cancelled. Late or foreign observations change nothing.
#include "bpr/core/github_transition.hpp"
#include "bpr/core/home_open.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace {
	using namespace bpr::core;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto locator(std::string aName = "review-repository") -> repository_locator
	{
		return repository_locator{ .Owner = "octo-org", .Name = std::move(aName) };
	}

	[[nodiscard]] auto ready_state() -> github_state
	{
		auto        state      = github_state{ .ApplicationSession = application_session_id{ 41 } };
		const auto  activation = transition(state, activate_host_profile_intent{ .Profile = host_profile_id{ 11 } });
		const auto* request    = std::get_if<establish_github_session_request>(&activation.Requests.at(1));
		expect(request != nullptr, "activation asks for a session");
		const auto ready = transition(state, github_session_ready_event{
												 .Context            = request->Context,
												 .Host               = git_host_id{ 21 },
												 .Account            = github_account_id{ 31 },
												 .Capabilities       = { },
												 .CapabilityRevision = capability_revision{ 41 },
											 });
		expect(ready.AcceptedSession.has_value(), "the fixture session is accepted");
		return state;
	}

	[[nodiscard]] auto fetch_of(const github_transition_output& aOutput) -> pull_request_request_context
	{
		const auto* fetch = std::get_if<fetch_pull_request_request>(&aOutput.Requests.at(0));
		expect(fetch != nullptr, "an open asks for one read");
		return fetch->Context;
	}

	// The read's own identity rides every transition that starts or ends it.
	void test_the_transition_names_each_read_stage()
	{
		auto       state  = ready_state();
		const auto opened = transition(state, open_pull_request_intent{ .Repository = locator(), .Number = pull_request_number{ 42 } });
		const auto read   = fetch_of(opened);
		expect(opened.Read.has_value() && opened.Read->Stage == pull_request_read_stage::pending,
			   "an open says its read is pending in the same transition");
		expect(opened.Read->Request == read.Request && opened.Read->Repository == locator()
				   && opened.Read->Number == pull_request_number{ 42 } && !opened.Read->Refresh,
			   "the pending read names its request, repository and number");

		const auto loaded = transition(state, pull_request_loaded_event{
												  .Context     = read,
												  .Repository  = repository_id{ 51 },
												  .PullRequest = pull_request_id{ 61 },
												  .Snapshot    = provider_snapshot_id{ 71 },
											  });
		expect(loaded.Read.has_value() && loaded.Read->Stage == pull_request_read_stage::loaded && loaded.Read->Request == read.Request,
			   "an accepted read says it loaded");

		const auto refreshed = transition(state, refresh_pull_request_intent{ });
		expect(refreshed.Read.has_value() && refreshed.Read->Refresh, "a refresh is marked as one");
		const auto failed = transition(state, pull_request_load_failed_event{ .Context = fetch_of(refreshed), .Failure = github_failure::network });
		expect(failed.Read.has_value() && failed.Read->Stage == pull_request_read_stage::failed
				   && failed.Read->Reason == github_reason_code::network_unavailable,
			   "a failed read says why");

		const auto second    = transition(state, open_pull_request_intent{ .Repository = locator(), .Number = pull_request_number{ 43 } });
		const auto cancelled = transition(state, pull_request_load_cancelled_event{ .Context = fetch_of(second) });
		expect(cancelled.Read.has_value() && cancelled.Read->Stage == pull_request_read_stage::cancelled,
			   "a cancelled read says so");

		const auto third     = transition(state, open_pull_request_intent{ .Repository = locator(), .Number = pull_request_number{ 44 } });
		const auto abandoned = transition(state, deactivate_github_identity_intent{ });
		expect(abandoned.Read.has_value() && abandoned.Read->Stage == pull_request_read_stage::cancelled
				   && abandoned.Read->Request == fetch_of(third).Request,
			   "signing out ends the pending read it abandons");
	}

	// Opening at once, waiting after the wait, gone once loaded.
	void test_an_open_is_shown_from_the_gesture_until_it_loads()
	{
		auto       state  = ready_state();
		const auto opened = transition(state, open_pull_request_intent{ .Repository = locator(), .Number = pull_request_number{ 42 } });
		const auto read   = fetch_of(opened);
		auto       home   = apply_home_open(std::nullopt, opened);
		expect(home.has_value() && home->Phase == home_open_phase::opening && home->Request == read.Request
				   && home->Number == pull_request_number{ 42 },
			   "the gesture's own transition shows the pull request opening");
		expect(home_open_in_flight(home, locator(), pull_request_number{ 42 }), "a second gesture for it is in flight");
		expect(!home_open_in_flight(home, locator(), pull_request_number{ 43 }), "another pull request is not");
		expect(!home_open_cancellable(home), "Cancel waits for the wait, so an ordinary open stays quiet");

		home = apply_home_open(home, home_open_waited{ .Request = request_id{ read.Request.value() + 99U } });
		expect(home->Phase == home_open_phase::opening, "another read's wait changes nothing");
		home = apply_home_open(home, home_open_waited{ .Request = read.Request });
		expect(home->Phase == home_open_phase::waiting, "this read's wait shows the step and Cancel");
		expect(home_open_cancellable(home) && admit_home_open_cancel(home, state, read.Request),
			   "Cancel is offered and admitted for the pending read");
		expect(!admit_home_open_cancel(home, state, request_id{ read.Request.value() + 1U }),
			   "a cancel naming another read is refused");

		const auto loaded = transition(state, pull_request_loaded_event{
												  .Context     = read,
												  .Repository  = repository_id{ 51 },
												  .PullRequest = pull_request_id{ 61 },
												  .Snapshot    = provider_snapshot_id{ 71 },
											  });
		home              = apply_home_open(home, loaded);
		expect(!home.has_value(), "a loaded read leaves nothing to show");
	}

	// Failed with its reason and Retry; a later open replaces it.
	void test_a_failure_stays_with_its_reason_until_the_next_open()
	{
		auto       state  = ready_state();
		const auto opened = transition(state, open_pull_request_intent{ .Repository = locator(), .Number = pull_request_number{ 42 } });
		auto       home   = apply_home_open(std::nullopt, opened);
		const auto failed = transition(state, pull_request_load_failed_event{ .Context = fetch_of(opened), .Failure = github_failure::network });
		home              = apply_home_open(home, failed);
		expect(home.has_value() && home->Phase == home_open_phase::failed && home->Reason == github_reason_code::network_unavailable,
			   "a failed read is kept with its reason");
		expect(home_open_retryable(home), "a network failure offers Retry");
		expect(!home_open_cancellable(home) && !home_open_in_flight(home, locator(), pull_request_number{ 42 }),
			   "a failure offers no Cancel and does not block a second gesture");
		expect(reconcile_home_open(home, state) == home, "a failure outlives the lane's pending read");
		home = apply_home_open(home, home_open_waited{ .Request = fetch_of(opened).Request });
		expect(home->Phase == home_open_phase::failed, "a late wait does not revive it");

		auto refused    = home;
		refused->Reason = github_reason_code::invalid_pull_request_number;
		expect(!home_open_retryable(refused), "a refused name offers no Retry");

		const auto again = transition(state, open_pull_request_intent{ .Repository = locator("other"), .Number = pull_request_number{ 7 } });
		home             = apply_home_open(home, again);
		expect(home.has_value() && home->Phase == home_open_phase::opening && home->Repository == locator("other"),
			   "the next open replaces the failure");
	}

	// Cancelled ends it; a refresh and another read's end change nothing.
	void test_cancel_refresh_and_foreign_reads()
	{
		auto       state  = ready_state();
		const auto opened = transition(state, open_pull_request_intent{ .Repository = locator(), .Number = pull_request_number{ 42 } });
		auto       home   = apply_home_open(std::nullopt, opened);

		auto foreign          = opened;
		foreign.Read          = opened.Read;
		foreign.Read->Request = request_id{ opened.Read->Request.value() + 7U };
		foreign.Read->Stage   = pull_request_read_stage::failed;
		expect(apply_home_open(home, foreign) == home, "another read's failure changes nothing");

		auto refresh          = opened;
		refresh.Read->Refresh = true;
		refresh.Read->Request = request_id{ opened.Read->Request.value() + 8U };
		expect(apply_home_open(home, refresh) == home, "a refresh is not an opening");
		expect(apply_home_open(home, github_transition_output{ }) == home, "a transition with no read changes nothing");

		const auto cancelled = transition(state, pull_request_load_cancelled_event{ .Context = fetch_of(opened) });
		expect(!apply_home_open(home, cancelled).has_value(), "a cancelled read leaves nothing to show");

		// The lane can drop its read without a transition: a retired session.
		const auto reopened = transition(state, open_pull_request_intent{ .Repository = locator(), .Number = pull_request_number{ 42 } });
		home                = apply_home_open(std::nullopt, reopened);
		expect(reconcile_home_open(home, state) == home, "the lane's own pending read is kept");
		auto reset = state;
		reset.PendingPullRequest.reset();
		expect(!reconcile_home_open(home, reset).has_value(), "an opening the lane no longer holds ends");
		expect(!admit_home_open_cancel(apply_home_open(home, home_open_waited{ .Request = fetch_of(reopened).Request }), reset, fetch_of(reopened).Request),
			   "a cancel for a read the lane no longer holds is refused");
	}

	// [[BPR-1679]]. Home's loading gives way while any window waits on a read,
	// an open or a reread, and resumes once every read has ended.
	void test_home_loading_yields_to_a_window_read()
	{
		auto lanes = std::array{ ready_state(), ready_state() };
		expect(!home_loading_yields(lanes), "with no read pending Home keeps loading");
		expect(!home_loading_yields(std::span<const github_state>{ }), "and with no window at all");

		const auto opened = transition(lanes[1], open_pull_request_intent{ .Repository = locator(), .Number = pull_request_number{ 42 } });
		expect(home_loading_yields(lanes), "an open in the second window makes Home give way at once");
		expect(!home_loading_yields(lanes, true), "[[BPR-1680]] unless discovery reads on lanes of its own, beside the window's");
		const auto loaded = transition(lanes[1], pull_request_loaded_event{
													 .Context     = fetch_of(opened),
													 .Repository  = repository_id{ 51 },
													 .PullRequest = pull_request_id{ 61 },
													 .Snapshot    = provider_snapshot_id{ 71 },
												 });
		expect(loaded.Read.has_value() && loaded.Read->Stage == pull_request_read_stage::loaded && !home_loading_yields(lanes),
			   "and resumes once that read has loaded");

		const auto refreshed = transition(lanes[1], refresh_pull_request_intent{ });
		expect(home_loading_yields(lanes), "a reread is waited for too");
		static_cast<void>(transition(lanes[1], pull_request_load_failed_event{ .Context = fetch_of(refreshed), .Failure = github_failure::network }));
		expect(!home_loading_yields(lanes), "a failed read ends the wait");

		const auto first  = transition(lanes[0], open_pull_request_intent{ .Repository = locator(), .Number = pull_request_number{ 43 } });
		const auto second = transition(lanes[1], open_pull_request_intent{ .Repository = locator(), .Number = pull_request_number{ 44 } });
		static_cast<void>(transition(lanes[0], pull_request_load_cancelled_event{ .Context = fetch_of(first) }));
		expect(home_loading_yields(lanes), "one window's cancel leaves Home giving way to the other's read");
		const auto abandoned = transition(lanes[1], deactivate_github_identity_intent{ });
		expect(abandoned.Read.has_value() && abandoned.Read->Request == fetch_of(second).Request && !home_loading_yields(lanes),
			   "signing out abandons the read and Home resumes");
	}
} // namespace

auto main() -> int
{
	test_the_transition_names_each_read_stage();
	test_an_open_is_shown_from_the_gesture_until_it_loads();
	test_a_failure_stays_with_its_reason_until_the_next_open();
	test_cancel_refresh_and_foreign_reads();
	test_home_loading_yields_to_a_window_read();
	std::cout << "core home open tests passed\n";
	return EXIT_SUCCESS;
}
