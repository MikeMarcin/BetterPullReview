#include "bpr/core/device_authorization_transition.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>
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

	using stage  = core::device_authorization_stage;
	using reason = core::device_authorization_reason_code;
	using cmd    = core::device_authorization_command_id;

	[[nodiscard]] auto fresh_state() -> core::device_authorization_state
	{
		auto state               = core::device_authorization_state{ };
		state.ApplicationSession = core::application_session_id{ 7U };
		state.IdentityGeneration = core::github_identity_generation{ 3U };
		return state;
	}

	[[nodiscard]] auto begun(core::device_authorization_state& aState)
		-> core::device_authorization_request_context
	{
		const auto output = core::transition(
			aState,
			core::begin_device_authorization_intent{
				.Profile = core::host_profile_id{ 11U } });
		expect(
			aState.Stage == stage::requested && aState.Pending.has_value(),
			"a begin intent enters requested with a pending context");
		expect(
			output.Requests.size() == 1U
				&& std::holds_alternative<
					core::begin_device_authorization_request>(
					output.Requests.front()),
			"a begin intent emits exactly the begin host request");
		return *aState.Pending;
	}

	[[nodiscard]] auto awaiting(core::device_authorization_state& aState)
		-> core::device_authorization_request_context
	{
		const auto context = begun(aState);
		const auto output  = core::transition(
			aState,
			core::device_authorization_awaiting_event{ .Context = context });
		expect(
			aState.Stage == stage::awaiting_authorization
				&& output.Status.has_value()
				&& output.Status->Stage == stage::awaiting_authorization,
			"an awaiting observation moves requested to awaiting");
		return context;
	}

	[[nodiscard]] auto only_diagnostic(
		const core::device_authorization_transition_output& aOutput,
		const reason                                        aReason) -> bool
	{
		return aOutput.Requests.empty() && !aOutput.Status.has_value()
			&& aOutput.Diagnostics.size() == 1U
			&& aOutput.Diagnostics.front().Code == aReason;
	}

	[[nodiscard]] auto event_with(
		const std::size_t                                 aIndex,
		const core::device_authorization_request_context& aContext)
		-> core::device_authorization_event
	{
		switch (aIndex) {
		case 0U:
			return core::device_authorization_awaiting_event{
				.Context = aContext,
			};
		case 1U:
			return core::device_authorization_authorized_event{
				.Context = aContext,
			};
		case 2U:
			return core::device_authorization_denied_event{
				.Context = aContext,
			};
		case 3U:
			return core::device_authorization_expired_event{
				.Context = aContext,
			};
		case 4U:
			return core::device_authorization_disabled_event{
				.Context = aContext,
			};
		case 5U:
			return core::device_authorization_unavailable_event{
				.Context = aContext,
			};
		case 6U:
			return core::device_authorization_failed_event{
				.Context = aContext,
			};
		default:
			return core::device_authorization_cancelled_event{
				.Context = aContext,
			};
		}
	}

	inline constexpr std::size_t observation_count = 8U;

	void test_the_successful_course_runs_end_to_end()
	{
		auto       state   = fresh_state();
		const auto context = awaiting(state);

		const auto output = core::transition(
			state,
			core::device_authorization_authorized_event{ .Context = context });
		expect(
			state.Stage == stage::authorized && !state.Pending.has_value()
				&& state.TerminalReason == reason::none,
			"an authorized observation is a clean terminal");
		expect(
			output.Status.has_value()
				&& output.Status->Stage == stage::authorized
				&& output.Diagnostics.empty(),
			"authorization succeeds without a diagnostic");
	}

	void test_each_terminal_observation_reaches_its_stage()
	{
		constexpr auto expected = std::array{
			std::pair{ 1U, std::pair{ stage::authorized, reason::none } },
			std::pair{
				2U,
				std::pair{ stage::denied, reason::authorization_denied } },
			std::pair{
				3U,
				std::pair{ stage::expired, reason::authorization_expired } },
			std::pair{
				4U,
				std::pair{ stage::disabled, reason::device_flow_disabled } },
			std::pair{
				5U,
				std::pair{
					stage::unavailable, reason::device_flow_unavailable } },
			std::pair{ 6U, std::pair{ stage::failed, reason::network_unavailable } },
			std::pair{ 7U, std::pair{ stage::cancelled, reason::cancelled } },
		};

		for (const auto& [index, outcome] : expected) {
			auto       state   = fresh_state();
			const auto context = awaiting(state);
			const auto output
				= core::transition(state, event_with(index, context));
			expect(
				state.Stage == outcome.first
					&& state.TerminalReason == outcome.second
					&& !state.Pending.has_value(),
				"each terminal observation reaches exactly its stage");
			expect(
				core::terminal(state.Stage),
				"every resolved stage reports terminal");
			expect(
				output.Status.has_value() && output.Status->Stage == state.Stage,
				"every terminal publishes its status delta");
		}
	}

	void test_every_failure_maps_to_a_distinct_reason()
	{
		constexpr auto failures = std::array{
			std::pair{
				core::device_authorization_failure::network,
				reason::network_unavailable },
			std::pair{
				core::device_authorization_failure::certificate,
				reason::certificate_rejected },
			std::pair{
				core::device_authorization_failure::redirect,
				reason::redirect_rejected },
			std::pair{
				core::device_authorization_failure::timeout,
				reason::timed_out },
			std::pair{
				core::device_authorization_failure::malformed_response,
				reason::malformed_response },
			std::pair{
				core::device_authorization_failure::response_too_large,
				reason::response_too_large },
			std::pair{
				core::device_authorization_failure::vault,
				reason::vault_unavailable },
		};
		for (const auto& [failure, mapped] : failures) {
			auto       state   = fresh_state();
			const auto context = awaiting(state);
			const auto output  = core::transition(
				state,
				core::device_authorization_failed_event{
					.Context = context,
					.Failure = failure });
			expect(
				state.Stage == stage::failed && state.TerminalReason == mapped
					&& output.Diagnostics.size() == 1U
					&& output.Diagnostics.front().Code == mapped,
				"each failure maps to its own terminal reason");
		}
	}

	void test_a_stale_context_never_changes_state()
	{
		auto       state   = fresh_state();
		const auto context = awaiting(state);

		auto stale       = context;
		stale.Generation = core::device_authorization_generation{
			stale.Generation.value() + 1U,
		};
		for (std::size_t index = 0U; index < observation_count; ++index) {
			const auto before = state;
			const auto output = core::transition(state, event_with(index, stale));
			expect(
				state == before && only_diagnostic(output, reason::stale_completion),
				"an observation with a foreign generation is rejected whole");
		}

		auto wrongRequest    = context;
		wrongRequest.Request = core::request_id{ 99U };
		const auto before    = state;
		const auto output
			= core::transition(state, event_with(1U, wrongRequest));
		expect(
			state == before && only_diagnostic(output, reason::stale_completion),
			"an observation with a foreign request id is rejected whole");
	}

	void test_a_duplicate_terminal_is_stale()
	{
		auto       state   = fresh_state();
		const auto context = awaiting(state);
		(void)core::transition(
			state,
			core::device_authorization_denied_event{ .Context = context });

		for (std::size_t index = 0U; index < observation_count; ++index) {
			const auto before = state;
			const auto output
				= core::transition(state, event_with(index, context));
			expect(
				state == before
					&& only_diagnostic(output, reason::stale_completion),
				"after a terminal, every replayed observation is stale");
		}
	}

	void test_observations_without_a_flow_are_stale()
	{
		auto state         = fresh_state();
		auto orphanContext = core::device_authorization_request_context{
			.Request            = core::request_id{ 1U },
			.ApplicationSession = state.ApplicationSession,
			.Profile            = core::host_profile_id{ 11U },
			.IdentityGeneration = state.IdentityGeneration,
			.Generation         = core::device_authorization_generation{ 1U },
		};
		for (std::size_t index = 0U; index < observation_count; ++index) {
			const auto before = state;
			const auto output
				= core::transition(state, event_with(index, orphanContext));
			expect(
				state == before
					&& only_diagnostic(output, reason::stale_completion),
				"with no pending flow every observation is stale");
		}
	}

	void test_an_awaiting_observation_is_legal_only_once()
	{
		auto       state   = fresh_state();
		const auto context = awaiting(state);
		const auto before  = state;
		const auto output  = core::transition(
			state,
			core::device_authorization_awaiting_event{ .Context = context });
		expect(
			state == before && only_diagnostic(output, reason::stale_completion),
			"a second awaiting observation is stale, not a state change");
	}

	void test_begin_is_illegal_while_a_flow_is_active()
	{
		auto requested = fresh_state();
		(void)begun(requested);
		const auto beforeRequested = requested;
		auto       output          = core::transition(
			requested,
			core::begin_device_authorization_intent{
				.Profile = core::host_profile_id{ 11U } });
		expect(
			requested == beforeRequested
				&& only_diagnostic(output, reason::flow_in_progress),
			"begin while requested is rejected");

		auto waiting = fresh_state();
		(void)awaiting(waiting);
		const auto beforeWaiting = waiting;
		output                   = core::transition(
			waiting,
			core::begin_device_authorization_intent{
				.Profile = core::host_profile_id{ 11U } });
		expect(
			waiting == beforeWaiting
				&& only_diagnostic(output, reason::flow_in_progress),
			"begin while awaiting is rejected");
	}

	void test_cancel_is_legal_only_while_a_flow_is_active()
	{
		auto       idle       = fresh_state();
		const auto beforeIdle = idle;
		auto       output
			= core::transition(idle, core::cancel_device_authorization_intent{ });
		expect(
			idle == beforeIdle && only_diagnostic(output, reason::no_active_flow),
			"cancel with no flow is rejected");

		auto       state   = fresh_state();
		const auto context = awaiting(state);
		output
			= core::transition(state, core::cancel_device_authorization_intent{ });
		expect(
			state.Stage == stage::cancelling && state.Pending.has_value(),
			"cancel while awaiting enters cancelling and keeps the context");
		expect(
			output.Requests.size() == 1U
				&& std::holds_alternative<
					core::cancel_device_authorization_request>(
					output.Requests.front()),
			"cancel emits exactly the cancel host request");

		const auto beforeSecond = state;
		output
			= core::transition(state, core::cancel_device_authorization_intent{ });
		expect(
			state == beforeSecond
				&& only_diagnostic(output, reason::cancellation_in_progress),
			"a second cancel while cancelling is rejected");

		const auto confirmed = core::transition(
			state,
			core::device_authorization_cancelled_event{ .Context = context });
		expect(
			state.Stage == stage::cancelled && !state.Pending.has_value()
				&& state.TerminalReason == reason::cancelled
				&& confirmed.Status.has_value()
				&& confirmed.Status->Stage == stage::cancelled,
			"the host's cancellation confirmation reaches the cancelled terminal");
	}

	void test_a_completion_race_never_beats_a_cancellation()
	{
		for (std::size_t index = 0U; index < observation_count - 1U; ++index) {
			auto       state   = fresh_state();
			const auto context = awaiting(state);
			(void)core::transition(
				state,
				core::cancel_device_authorization_intent{ });
			const auto before = state;
			const auto output
				= core::transition(state, event_with(index, context));
			expect(
				state == before
					&& only_diagnostic(
						output, reason::cancellation_in_progress),
				"no completion observation resolves a flow being cancelled");
		}
	}

	void test_host_initiated_cancellation_ends_an_active_flow()
	{
		auto       state   = fresh_state();
		const auto context = begun(state);
		const auto output  = core::transition(
			state,
			core::device_authorization_cancelled_event{ .Context = context });
		expect(
			state.Stage == stage::cancelled
				&& state.TerminalReason == reason::cancelled
				&& output.Status.has_value(),
			"a host-side cancellation, as at shutdown, is accepted while active");
	}

	void test_every_terminal_permits_a_fresh_begin()
	{
		for (std::size_t index = 1U; index < observation_count; ++index) {
			auto       state   = fresh_state();
			const auto context = awaiting(state);
			(void)core::transition(state, event_with(index, context));
			const auto previousGeneration = state.Generation;

			const auto restarted = begun(state);
			expect(
				restarted.Generation.value() > previousGeneration.value()
					&& state.TerminalReason == reason::none,
				"a restart after any terminal issues a fresh generation");

			const auto before = state;
			const auto output = core::transition(state, event_with(1U, context));
			expect(
				state == before
					&& only_diagnostic(output, reason::stale_completion),
				"the prior flow's completion stays stale after a restart");
		}
	}

	void test_commands_track_every_stage()
	{
		const auto commandFor = [](const core::device_authorization_state& aState,
								   const cmd                               aId)
			-> core::device_authorization_command_projection {
			for (const auto& projection :
				 core::device_authorization_commands(aState)) {
				if (projection.Id == aId) {
					return projection;
				}
			}
			return { };
		};

		auto idle = fresh_state();
		expect(
			commandFor(idle, cmd::begin_device_authorization).Enabled
				&& !commandFor(idle, cmd::cancel_device_authorization).Enabled
				&& commandFor(idle, cmd::cancel_device_authorization)
						.DisabledReason
					== reason::no_active_flow,
			"idle offers begin and withholds cancel");

		auto active = fresh_state();
		(void)awaiting(active);
		expect(
			!commandFor(active, cmd::begin_device_authorization).Enabled
				&& commandFor(active, cmd::begin_device_authorization)
						.DisabledReason
					== reason::flow_in_progress
				&& commandFor(active, cmd::cancel_device_authorization).Enabled,
			"an active flow offers cancel and withholds begin");

		(void)core::transition(
			active,
			core::cancel_device_authorization_intent{ });
		expect(
			!commandFor(active, cmd::begin_device_authorization).Enabled
				&& !commandFor(active, cmd::cancel_device_authorization).Enabled
				&& commandFor(active, cmd::begin_device_authorization)
						.DisabledReason
					== reason::cancellation_in_progress,
			"cancelling withholds both commands");

		auto       resolved = fresh_state();
		const auto context  = awaiting(resolved);
		(void)core::transition(
			resolved,
			core::device_authorization_denied_event{ .Context = context });
		expect(
			commandFor(resolved, cmd::begin_device_authorization).Enabled
				&& !commandFor(resolved, cmd::cancel_device_authorization)
						.Enabled,
			"a terminal offers begin again");
	}

	void test_transitions_publish_commands_and_status_together()
	{
		auto       state  = fresh_state();
		const auto output = core::transition(
			state,
			core::begin_device_authorization_intent{
				.Profile = core::host_profile_id{ 11U } });
		expect(
			core::has_changed(
				output.Changed,
				core::device_authorization_changed_domain::commands)
				&& core::has_changed(
					output.Changed,
					core::device_authorization_changed_domain::status)
				&& !output.Commands.empty(),
			"an accepted transition republishes commands with its status");
	}

	void test_rejections_leave_requests_unsent()
	{
		auto       state = fresh_state();
		const auto output
			= core::transition(state, core::cancel_device_authorization_intent{ });
		expect(
			output.Requests.empty()
				&& core::has_changed(
					output.Changed,
					core::device_authorization_changed_domain::diagnostics),
			"a rejected event emits diagnostics and no host request");
	}
}

auto main() -> int
{
	test_the_successful_course_runs_end_to_end();
	test_each_terminal_observation_reaches_its_stage();
	test_every_failure_maps_to_a_distinct_reason();
	test_a_stale_context_never_changes_state();
	test_a_duplicate_terminal_is_stale();
	test_observations_without_a_flow_are_stale();
	test_an_awaiting_observation_is_legal_only_once();
	test_begin_is_illegal_while_a_flow_is_active();
	test_cancel_is_legal_only_while_a_flow_is_active();
	test_a_completion_race_never_beats_a_cancellation();
	test_host_initiated_cancellation_ends_an_active_flow();
	test_every_terminal_permits_a_fresh_begin();
	test_commands_track_every_stage();
	test_transitions_publish_commands_and_status_together();
	test_rejections_leave_requests_unsent();
	std::cout << "device authorization core tests passed\n";
	return EXIT_SUCCESS;
}
