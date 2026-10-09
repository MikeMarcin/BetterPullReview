// [[BPR-1357]]. The application owns the clock behind Home's opening: one wait
// per read, armed in the gesture's own turn, delivered to the core by request
// identity, and dropped when it elapses for a read the core no longer shows or
// after the host is gone. Cancel and Retry are admitted against the core's
// state, never against what a widget remembers.
#include "bpr/application/home_open_host.hpp"
#include "bpr/core/github_transition.hpp"

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {
	using namespace bpr;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	/// A manual clock: waits are held until the test lets them elapse.
	struct manual_clock
	{
		struct wait
		{
			std::uint32_t         Milliseconds = 0U;
			std::function<void()> Elapsed;
		};
		std::vector<wait> Waits;

		[[nodiscard]] auto port() -> application::home_open_wait
		{
			return [this](const std::uint32_t aMilliseconds, std::function<void()> aElapsed) {
				Waits.push_back({ aMilliseconds, std::move(aElapsed) });
			};
		}
	};

	[[nodiscard]] auto ready_lane() -> core::github_state
	{
		auto        state      = core::github_state{ .ApplicationSession = core::application_session_id{ 41 } };
		const auto  activation = core::transition(state, core::activate_host_profile_intent{ .Profile = core::host_profile_id{ 11 } });
		const auto* request    = std::get_if<core::establish_github_session_request>(&activation.Requests.at(1));
		static_cast<void>(core::transition(state, core::github_session_ready_event{
													  .Context            = request->Context,
													  .Host               = core::git_host_id{ 21 },
													  .Account            = core::github_account_id{ 31 },
													  .Capabilities       = { },
													  .CapabilityRevision = core::capability_revision{ 41 },
												  }));
		return state;
	}

	[[nodiscard]] auto open(core::github_state& aLane, const std::uint32_t aNumber) -> core::github_transition_output
	{
		return core::transition(aLane, core::open_pull_request_intent{
										   .Repository = core::repository_locator{ .Owner = "octo-org", .Name = "review-repository" },
										   .Number     = core::pull_request_number{ aNumber },
									   });
	}

	[[nodiscard]] auto context_of(const core::github_transition_output& aOutput) -> core::pull_request_request_context
	{
		return std::get<core::fetch_pull_request_request>(aOutput.Requests.at(0)).Context;
	}

	void test_opening_is_published_in_the_gestures_turn_and_waits_on_the_clock()
	{
		manual_clock                                   clock;
		std::vector<application::home_open_projection> published;
		application::home_open_host                    host{ clock.port(), [&](const application::home_open_projection& aProjection) { published.push_back(aProjection); } };
		auto                                           lane   = ready_lane();
		const auto                                     opened = open(lane, 42);
		host.observe(opened, lane);
		expect(published.size() == 1U && published.back().State.has_value()
				   && published.back().State->Phase == core::home_open_phase::opening && !published.back().Cancellable,
			   "the opening is published in the gesture's own turn, without Cancel");
		expect(clock.Waits.size() == 1U && clock.Waits.front().Milliseconds == core::home_open_wait_milliseconds,
			   "one wait of the core's length is armed for the read");
		host.observe(core::github_transition_output{ }, lane);
		expect(published.size() == 1U && clock.Waits.size() == 1U, "an unrelated transition publishes and arms nothing");

		clock.Waits.front().Elapsed();
		expect(published.size() == 2U && published.back().State->Phase == core::home_open_phase::waiting && published.back().Cancellable,
			   "the elapsed wait shows the step and offers Cancel");
		expect(host.admit_cancel(context_of(opened).Request, lane), "Cancel for the shown read is admitted");
		expect(!host.admit_cancel(core::request_id{ context_of(opened).Request.value() + 1U }, lane), "Cancel for another read is refused");

		const auto cancelled = core::transition(lane, core::pull_request_load_cancelled_event{ .Context = context_of(opened) });
		host.observe(cancelled, lane);
		expect(published.size() == 3U && !published.back().State.has_value(), "a cancelled read leaves Home quiet");
		clock.Waits.front().Elapsed();
		expect(published.size() == 3U, "a wait that elapses after its read ended changes nothing");
	}

	void test_a_late_wait_for_a_replaced_read_changes_nothing()
	{
		manual_clock                clock;
		auto                        publications = 0;
		application::home_open_host host{ clock.port(), [&](const application::home_open_projection&) { ++publications; } };
		auto                        lane  = ready_lane();
		const auto                  first = open(lane, 42);
		host.observe(first, lane);
		host.observe(core::transition(lane, core::pull_request_load_failed_event{ .Context = context_of(first), .Failure = core::github_failure::network }), lane);
		expect(host.state()->Phase == core::home_open_phase::failed, "the first read failed");
		const auto retry = host.admit_retry(context_of(first).Request);
		expect(retry.has_value() && retry->Number == core::pull_request_number{ 42 }, "Retry asks for the same pull request again");
		expect(!host.admit_retry(core::request_id{ 999 }).has_value(), "Retry naming another read is refused");

		const auto second = open(lane, 42);
		host.observe(second, lane);
		expect(clock.Waits.size() == 2U && host.waits_armed() == 2U, "the second read arms its own wait");
		clock.Waits.front().Elapsed();
		expect(host.state()->Phase == core::home_open_phase::opening, "the first read's wait does not move the second");
		clock.Waits.back().Elapsed();
		expect(host.state()->Phase == core::home_open_phase::waiting, "the second read's own wait does");
		expect(!host.admit_retry(context_of(second).Request).has_value(), "nothing in flight is retried");

		// A retired session drops the lane's read without a transition.
		auto retired = lane;
		retired.PendingPullRequest.reset();
		host.reconcile(retired);
		expect(!host.state().has_value(), "reconciling with a lane that no longer holds the read ends it");
		expect(publications == 5, "each change was published once: opening, failed, opening, waiting, ended");
	}

	void test_no_wait_outlives_the_host()
	{
		manual_clock clock;
		auto         publications = 0;
		{
			application::home_open_host host{ clock.port(), [&](const application::home_open_projection&) { ++publications; } };
			auto                        lane = ready_lane();
			host.observe(open(lane, 42), lane);
		}
		clock.Waits.front().Elapsed();
		expect(publications == 1, "a wait elapsing after the host is gone does nothing");
	}
} // namespace

auto main() -> int
{
	test_opening_is_published_in_the_gestures_turn_and_waits_on_the_clock();
	test_a_late_wait_for_a_replaced_read_changes_nothing();
	test_no_wait_outlives_the_host();
	std::cout << "application home open host tests passed\n";
	return EXIT_SUCCESS;
}
