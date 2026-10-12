#include "bpr/application/device_authorization_application_host.hpp"
#include "bpr/application/device_authorization_schedule.hpp"
#include "bpr/application/manual_executor.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	// ---------------------------------------------------------------------
	// device_authorization_schedule
	// ---------------------------------------------------------------------

	using application::device_authorization_schedule;
	using application::device_authorization_schedule_action;
	using application::device_authorization_schedule_bounds;

	[[nodiscard]] auto schedule_bounds(
		const std::uint32_t aInterval   = 5U,
		const std::uint32_t aExpires    = 900U,
		const std::uint32_t aTotalBound = 900U,
		const std::uint32_t aPolls      = 256U)
		-> device_authorization_schedule_bounds
	{
		return device_authorization_schedule_bounds{
			.IntervalSeconds   = aInterval,
			.ExpiresInSeconds  = aExpires,
			.TotalBoundSeconds = aTotalBound,
			.MaximumPolls      = aPolls,
		};
	}

	[[nodiscard]] auto action_at(
		const device_authorization_schedule& aSchedule,
		const std::uint32_t                  aElapsed)
		-> device_authorization_schedule_action
	{
		return aSchedule.decide(aElapsed).Action;
	}

	[[nodiscard]] auto wait_at(
		const device_authorization_schedule& aSchedule,
		const std::uint32_t                  aElapsed) -> std::uint32_t
	{
		return aSchedule.decide(aElapsed).WaitSeconds;
	}

	// The grant instant counts as a poll that already happened. Polling the
	// token endpoint the moment the device code arrives cannot succeed — the
	// reviewer has not seen the user code yet — and GitHub answers a burst with
	// slow_down, so the first request is the one most worth spacing correctly.
	void test_the_first_poll_waits_a_whole_interval()
	{
		const auto schedule = device_authorization_schedule{ schedule_bounds(5U) };

		expect(
			action_at(schedule, 0U) == device_authorization_schedule_action::wait,
			"nothing is polled at the grant instant");
		expect(wait_at(schedule, 0U) == 5U, "the first wait is a whole interval");
		expect(
			action_at(schedule, 4U) == device_authorization_schedule_action::wait,
			"one second short of the interval is still a wait");
		expect(wait_at(schedule, 4U) == 1U, "the wait counts down exactly");
		expect(
			action_at(schedule, 5U)
				== device_authorization_schedule_action::poll_now,
			"the first poll is due exactly one interval in");
	}

	void test_polls_are_never_closer_together_than_the_interval()
	{
		auto schedule = device_authorization_schedule{ schedule_bounds(7U) };

		schedule.record_poll(7U);
		expect(
			action_at(schedule, 7U) == device_authorization_schedule_action::wait,
			"the second poll is not due at the instant of the first");
		expect(wait_at(schedule, 7U) == 7U, "a whole interval separates them");
		expect(
			action_at(schedule, 13U) == device_authorization_schedule_action::wait,
			"one second short is still a wait");
		expect(
			action_at(schedule, 14U)
				== device_authorization_schedule_action::poll_now,
			"the second poll is due one interval after the first");

		schedule.record_poll(20U);
		expect(
			action_at(schedule, 26U) == device_authorization_schedule_action::wait,
			"a late poll moves the next one out with it");
		expect(
			action_at(schedule, 27U)
				== device_authorization_schedule_action::poll_now,
			"the interval is measured from the poll, not from the due moment");
		expect(schedule.polls() == 2U, "both polls were counted");
	}

	// A server that answers with a zero or one-second interval would otherwise
	// have the host hammering the token endpoint until the flow is throttled.
	void test_a_server_interval_below_the_floor_is_raised_to_it()
	{
		const auto floored = device_authorization_schedule{ schedule_bounds(0U) };
		expect(
			floored.interval_seconds()
				== application::minimum_device_authorization_interval_seconds,
			"a zero interval is raised to the floor");
		expect(
			action_at(floored, 4U) == device_authorization_schedule_action::wait,
			"and the floor is what the decision uses");
		expect(
			action_at(floored, 5U)
				== device_authorization_schedule_action::poll_now,
			"the floored interval is honoured exactly");

		const auto capped = device_authorization_schedule{
			schedule_bounds(100'000U)
		};
		expect(
			capped.interval_seconds()
				== application::maximum_device_authorization_interval_seconds,
			"an implausible interval is lowered to the ceiling");
	}

	void test_slow_down_only_ever_slows_the_host_down()
	{
		auto larger = device_authorization_schedule{ schedule_bounds(5U) };
		larger.slow_down(10U);
		expect(larger.interval_seconds() == 10U, "the server's larger interval wins");

		auto equal = device_authorization_schedule{ schedule_bounds(10U) };
		equal.slow_down(10U);
		expect(
			equal.interval_seconds() == 15U,
			"an equal interval still increases by one increment");

		auto smaller = device_authorization_schedule{ schedule_bounds(30U) };
		smaller.slow_down(5U);
		expect(
			smaller.interval_seconds() == 35U,
			"a smaller interval cannot speed the host up");

		auto absent = device_authorization_schedule{ schedule_bounds(20U) };
		absent.slow_down(0U);
		expect(
			absent.interval_seconds() == 25U,
			"slow_down without an interval still applies the increment");

		auto capped = device_authorization_schedule{ schedule_bounds(300U) };
		capped.slow_down(100'000U);
		expect(
			capped.interval_seconds()
				== application::maximum_device_authorization_interval_seconds,
			"slow_down is bounded by the same ceiling as the grant");
	}

	void test_slow_down_changes_the_next_poll_and_not_the_last_one()
	{
		auto schedule = device_authorization_schedule{ schedule_bounds(5U) };
		schedule.record_poll(5U);
		expect(
			action_at(schedule, 10U)
				== device_authorization_schedule_action::poll_now,
			"the next poll is due at the old interval");

		schedule.slow_down(10U);
		expect(
			action_at(schedule, 10U) == device_authorization_schedule_action::wait,
			"the new interval defers the poll that was already due");
		expect(wait_at(schedule, 10U) == 5U, "by exactly the difference");
		expect(
			action_at(schedule, 15U)
				== device_authorization_schedule_action::poll_now,
			"and the poll lands one new interval after the last one");
	}

	void test_expiry_ends_the_course_and_no_poll_is_scheduled_past_it()
	{
		auto schedule = device_authorization_schedule{ schedule_bounds(60U, 100U) };
		schedule.record_poll(60U);

		// The next poll would fall at 120 seconds, which is past the point where
		// the device code stops being usable. Waking at the deadline instead is
		// what makes the terminal exact rather than a whole interval late.
		expect(
			action_at(schedule, 60U) == device_authorization_schedule_action::wait,
			"a poll past the deadline is not issued");
		expect(
			wait_at(schedule, 60U) == 40U,
			"the wait is clamped to the deadline, not to the interval");
		expect(
			action_at(schedule, 99U) == device_authorization_schedule_action::wait,
			"one second short of expiry is still live");
		expect(
			action_at(schedule, 100U)
				== device_authorization_schedule_action::expired,
			"expiry is reported at the exact second");
		expect(
			action_at(schedule, 100'000U)
				== device_authorization_schedule_action::expired,
			"and stays reported afterwards");
	}

	void test_the_total_bound_ends_a_course_the_server_would_keep_open()
	{
		auto schedule = device_authorization_schedule{
			schedule_bounds(5U, 3'600U, 60U)
		};

		expect(
			schedule.deadline_seconds() == 60U,
			"the shorter of the two limits decides");
		expect(
			action_at(schedule, 59U)
				== device_authorization_schedule_action::poll_now,
			"the flow is live right up to the bound");
		schedule.record_poll(59U);
		expect(
			action_at(schedule, 59U) == device_authorization_schedule_action::wait,
			"a wait after that poll is clamped to the bound");
		expect(
			wait_at(schedule, 59U) == 1U,
			"by exactly the second that is left");
		expect(
			action_at(schedule, 60U)
				== device_authorization_schedule_action::bound_exceeded,
			"the host stops at its own bound even though the code is still valid");
	}

	// The two limits can land on the same second. Reporting expiry there is the
	// honest answer: the code the reviewer was given genuinely stopped working.
	void test_expiry_wins_the_tie_with_the_total_bound()
	{
		const auto schedule = device_authorization_schedule{
			schedule_bounds(5U, 300U, 300U)
		};
		expect(
			action_at(schedule, 300U)
				== device_authorization_schedule_action::expired,
			"an exact tie reports expiry");
	}

	void test_the_poll_ceiling_ends_the_course()
	{
		auto schedule = device_authorization_schedule{
			schedule_bounds(5U, 900U, 900U, 3U)
		};

		schedule.record_poll(5U);
		schedule.record_poll(10U);
		expect(
			action_at(schedule, 15U)
				== device_authorization_schedule_action::poll_now,
			"the last permitted poll is still issued");
		schedule.record_poll(15U);
		expect(schedule.polls() == 3U, "three polls were counted");
		expect(
			action_at(schedule, 20U)
				== device_authorization_schedule_action::bound_exceeded,
			"the poll after the ceiling is refused");
		expect(
			action_at(schedule, 900U)
				== device_authorization_schedule_action::expired,
			"and time still outranks the poll count when both ran out");
	}

	void test_hostile_bounds_are_clamped_into_a_usable_schedule()
	{
		const auto zeroed = device_authorization_schedule{
			schedule_bounds(0U, 0U, 0U, 0U)
		};
		expect(valid(zeroed.bounds()), "all-zero bounds become valid bounds");
		expect(
			action_at(zeroed, 1U) == device_authorization_schedule_action::expired,
			"a zero validity expires immediately rather than never");

		const auto enormous = device_authorization_schedule{
			schedule_bounds(100'000U, 100'000U, 100'000U, 100'000U)
		};
		expect(valid(enormous.bounds()), "enormous bounds become valid bounds");
		expect(
			enormous.bounds().TotalBoundSeconds
				== application::device_authorization_total_bound_seconds,
			"a server cannot lengthen the host's own bound");
		expect(
			enormous.bounds().ExpiresInSeconds
				<= application::maximum_device_authorization_validity_seconds,
			"nor claim an unbounded validity");
	}

	void test_remaining_validity_counts_to_the_nearer_terminal()
	{
		const auto byExpiry = device_authorization_schedule{
			schedule_bounds(5U, 120U, 900U)
		};
		expect(
			byExpiry.remaining_seconds(0U) == 120U,
			"the whole validity is left at the grant");
		expect(byExpiry.remaining_seconds(119U) == 1U, "and counts down exactly");
		expect(byExpiry.remaining_seconds(120U) == 0U, "reaching zero at expiry");
		expect(
			byExpiry.remaining_seconds(100'000U) == 0U,
			"and never going below it");

		const auto byBound = device_authorization_schedule{
			schedule_bounds(5U, 3'600U, 90U)
		};
		expect(
			byBound.remaining_seconds(0U) == 90U,
			"a validity longer than the bound is reported as the bound");
	}

	void test_an_out_of_order_elapsed_second_never_moves_a_poll_earlier()
	{
		auto schedule = device_authorization_schedule{ schedule_bounds(10U) };
		schedule.record_poll(30U);
		schedule.record_poll(5U);

		expect(
			action_at(schedule, 39U) == device_authorization_schedule_action::wait,
			"the later poll still governs");
		expect(
			action_at(schedule, 40U)
				== device_authorization_schedule_action::poll_now,
			"the interval runs from the latest poll observed");
	}

	// The whole point of holding no clock is that a course can be replayed from
	// a list of numbers. This is that replay.
	void test_a_whole_replayed_course_polls_at_exactly_the_expected_seconds()
	{
		auto schedule = device_authorization_schedule{
			schedule_bounds(5U, 40U, 900U)
		};
		auto polls   = std::vector<std::uint32_t>{ };
		auto expired = false;

		for (auto elapsed = std::uint32_t{ 0U }; elapsed <= 60U; ++elapsed) {
			const auto decision = schedule.decide(elapsed);
			switch (decision.Action) {
			case device_authorization_schedule_action::poll_now:
				polls.push_back(elapsed);
				schedule.record_poll(elapsed);
				// The third poll is answered with slow_down.
				if (polls.size() == 3U) {
					schedule.slow_down(10U);
				}
				break;
			case device_authorization_schedule_action::wait:
				break;
			case device_authorization_schedule_action::expired:
				expired = true;
				break;
			case device_authorization_schedule_action::bound_exceeded:
				expect(false, "the replayed course does not exhaust a host bound");
				break;
			}
			if (expired) {
				break;
			}
		}

		const auto expected = std::vector<std::uint32_t>{ 5U, 10U, 15U, 25U, 35U };
		expect(polls == expected, "the replayed course polls at exact seconds");
		expect(expired, "and ends at expiry");
	}
	// ---------------------------------------------------------------------
	// device_authorization_application_host
	// ---------------------------------------------------------------------

	using application::device_authorization_application_host;
	using application::github_credential_material;

	[[nodiscard]] auto bytes_of(const std::string_view aText)
		-> std::vector<std::byte>
	{
		auto bytes = std::vector<std::byte>{ };
		bytes.reserve(aText.size());
		for (const auto character : aText) {
			bytes.push_back(static_cast<std::byte>(character));
		}
		return bytes;
	}

	[[nodiscard]] auto text_of(const std::span<const std::byte> aBytes)
		-> std::string
	{
		auto text = std::string{ };
		text.reserve(aBytes.size());
		for (const auto byte : aBytes) {
			text.push_back(static_cast<char>(byte));
		}
		return text;
	}

	// Every byte in these tests is a synthetic stand-in. Nothing here is, or
	// resembles, material any server ever issued.
	[[nodiscard]] auto material(const std::string_view aText)
		-> github_credential_material
	{
		auto created = github_credential_material::create(bytes_of(aText));
		expect(created.has_value(), "synthetic material is created");
		return std::move(*created);
	}

	[[nodiscard]] auto initial_state() -> core::device_authorization_state
	{
		return core::device_authorization_state{
			.ApplicationSession = core::application_session_id{ 5U },
			.NextRequest        = core::request_id{ 1U },
			.IdentityGeneration = core::github_identity_generation{ 3U },
			.Generation         = core::device_authorization_generation{ },
			.Stage              = core::device_authorization_stage::idle,
			.Pending            = std::nullopt,
			.Profile            = std::nullopt,
			.TerminalReason     = core::device_authorization_reason_code::none,
		};
	}

	[[nodiscard]] auto profile() -> core::host_profile_id
	{
		return core::host_profile_id{ 7U };
	}

	// A second composed profile -- the enterprise host beside the one the
	// capability was registered for. Nothing about it is malformed; it is
	// simply not the profile this host's identity belongs to ([[BPR-0535]]).
	[[nodiscard]] auto foreign_profile() -> core::host_profile_id
	{
		return core::host_profile_id{ 9U };
	}

	class fake_clock final : public application::device_authorization_clock_port
	{
	public:
		[[nodiscard]] auto observe_monotonic() noexcept
			-> std::chrono::milliseconds override
		{
			return Now;
		}

		void advance(const std::uint32_t aSeconds)
		{
			Now += std::chrono::seconds{ aSeconds };
		}

		std::chrono::milliseconds Now{ 0 };
	};

	class fake_timer final : public application::device_authorization_timer_port
	{
	public:
		[[nodiscard]] auto schedule_after(
			const std::uint32_t   aSeconds,
			std::function<void()> aExpiry) -> bool override
		{
			if (Refuse) {
				return false;
			}
			Delays.push_back(aSeconds);
			Expiry = std::move(aExpiry);
			return true;
		}

		void cancel() noexcept override
		{
			if (Expiry) {
				++Cancellations;
			}
			Expiry = { };
		}

		[[nodiscard]] auto armed() const noexcept -> bool
		{
			return static_cast<bool>(Expiry);
		}

		[[nodiscard]] auto last_delay() const -> std::uint32_t
		{
			expect(!Delays.empty(), "a delay was scheduled");
			return Delays.back();
		}

		void fire()
		{
			auto expiry = std::exchange(Expiry, std::function<void()>{ });
			expect(expiry != nullptr, "the timer was armed before it fired");
			expiry();
		}

		std::function<void()>      Expiry;
		std::vector<std::uint32_t> Delays;
		std::size_t                Cancellations = 0U;
		bool                       Refuse        = false;
	};

	// The double answers the transport's completion types directly. Nothing
	// here opens a socket, and the bytes it observes are the exact bytes the
	// host handed it, which is how erasure at the boundary is proven.
	class scripted_transport final
		: public application::device_authorization_transport_port
	{
	public:
		struct token_request
		{
			core::device_authorization_request_context Context;
			std::string                                Code;
		};

		void acquire_device_code(
			core::device_authorization_request_context  aContext,
			application::device_code_completion_handler aCompletion) override
		{
			CodeRequests.push_back(aContext);
			CodeCompletion = std::move(aCompletion);
		}

		void exchange_device_token(
			application::device_token_exchange           aExchange,
			application::device_token_completion_handler aCompletion) override
		{
			TokenRequests.push_back({
				.Context = aExchange.Context,
				.Code    = text_of(aExchange.Code.bytes()),
			});
			TokenCompletion = std::move(aCompletion);
		}

		void refresh_access_token(
			application::device_token_exchange           aExchange,
			application::device_token_completion_handler aCompletion) override
		{
			RefreshRequests.push_back({
				.Context = aExchange.Context,
				.Code    = text_of(aExchange.Code.bytes()),
			});
			RefreshCompletion = std::move(aCompletion);
		}

		void cancel(
			core::device_authorization_request_context aContext) override
		{
			Cancellations.push_back(aContext);
		}

		void answer_code(application::device_code_completion aCompletion)
		{
			auto handler = std::exchange(
				CodeCompletion,
				application::device_code_completion_handler{ });
			expect(handler != nullptr, "a device-code request was outstanding");
			handler(std::move(aCompletion));
		}

		void answer_token(application::device_token_completion aCompletion)
		{
			auto handler = std::exchange(
				TokenCompletion,
				application::device_token_completion_handler{ });
			expect(handler != nullptr, "a token request was outstanding");
			handler(std::move(aCompletion));
		}

		[[nodiscard]] auto last_code_context() const
			-> core::device_authorization_request_context
		{
			expect(!CodeRequests.empty(), "a device code was requested");
			return CodeRequests.back();
		}

		std::vector<core::device_authorization_request_context> CodeRequests;
		std::vector<token_request>                              TokenRequests;
		std::vector<token_request>                              RefreshRequests;
		std::vector<core::device_authorization_request_context> Cancellations;
		application::device_code_completion_handler             CodeCompletion;
		application::device_token_completion_handler            TokenCompletion;
		application::device_token_completion_handler            RefreshCompletion;
	};

	class recording_enrollment final
		: public application::device_authorization_enrollment_port
	{
	public:
		void enroll(
			const core::host_profile_id aProfile,
			github_credential_material  aCredential) override
		{
			Profiles.push_back(aProfile);
			Credentials.push_back(text_of(aCredential.bytes()));
			// The material dies here, which is the whole point of the boundary.
		}

		std::vector<core::host_profile_id> Profiles;
		std::vector<std::string>           Credentials;
	};

	class recording_projections final
		: public application::device_authorization_projection_port
	{
	public:
		void publish(
			const application::device_authorization_presentation& aPresentation)
			override
		{
			if (Throw) {
				Throw = false;
				throw std::runtime_error{ "projection" };
			}
			Published.push_back(aPresentation);
		}

		void publish_refresh(
			const application::device_authorization_refresh_observation&
				aObservation) override
		{
			Refreshes.push_back(aObservation);
		}

		[[nodiscard]] auto last() const
			-> application::device_authorization_presentation
		{
			expect(!Published.empty(), "a presentation was published");
			return Published.back();
		}

		std::vector<application::device_authorization_presentation> Published;
		std::vector<application::device_authorization_refresh_observation>
			 Refreshes;
		bool Throw = false;
	};

	// Counts every call so a test can prove the browser was never reached, not
	// merely that it reported nothing useful.
	class recording_browser final
		: public application::device_authorization_browser_port
	{
	public:
		[[nodiscard]] auto open(const std::string_view aAddress)
			-> bool override
		{
			Addresses.emplace_back(aAddress);
			if (Throw) {
				throw std::runtime_error{ "browser" };
			}
			return Accept;
		}

		std::vector<std::string> Addresses;
		bool                     Accept = true;
		bool                     Throw  = false;
	};

	// The device course itself needs no refresh slot. This vault gives the
	// answer every platform without one gives, so nothing in these tests can
	// depend on refresh material existing.
	//
	// It implements the refresh-only port, which is the whole of what the
	// device host is handed. There is no base-token slot here to stub out and
	// none for the host to reach: the narrowing is what makes that a fact about
	// the types rather than a claim about the code.
	class unavailable_vault final
		: public application::github_credential_refresh_vault_port
	{
	public:
		void store_refresh(
			application::github_credential_enrollment               aEnrollment,
			application::github_credential_store_completion_handler aCompletion)
			override
		{
			++Calls;
			const auto context = aEnrollment.Context;
			aEnrollment.Credential.reset();
			aCompletion(application::github_credential_store_error{
				.Context = context,
				.Failure = application::github_credential_vault_failure::unavailable,
			});
		}

		void retrieve_refresh(
			application::github_credential_operation_context           aContext,
			application::github_credential_retrieve_completion_handler aCompletion)
			override
		{
			++Calls;
			aCompletion(application::github_credential_retrieve_error{
				.Context = aContext,
				.Failure = application::github_credential_vault_failure::unavailable,
			});
		}

		void delete_refresh(
			application::github_credential_operation_context         aContext,
			application::github_credential_delete_completion_handler aCompletion)
			override
		{
			++Calls;
			aCompletion(application::github_credential_delete_error{
				.Context = aContext,
				.Failure = application::github_credential_vault_failure::unavailable,
			});
		}

		void cancel(application::github_credential_operation_context) override
		{
		}

		// Every reached call, so a test can prove the vault was never asked
		// rather than only that it answered nothing useful ([[BPR-0535]]).
		std::size_t Calls = 0U;
	};

	// One harness for the whole matrix. Everything the host talks to is here,
	// and every one of them is driven by hand.
	struct harness
	{
		explicit harness(
			const application::device_authorization_host_limits aLimits      = { },
			const bool                                          aBindBrowser = true,
			const std::string_view                              aAuthority
			= application::default_device_verification_authority,
			const bool aRegistered = true)
			: Host{ initial_state(), Transport, Vault, Enrollment,
					Clock, Timer, Lane, Projections,
					aLimits,
					application::device_authorization_browser_handoff{
						.Browser   = aBindBrowser ? &Browser : nullptr,
						.Authority = std::string{ aAuthority },
					},
					application::device_authorization_capability{
						.Registered = aRegistered, .Profile = profile() } }
		{
		}

		void begin()
		{
			begin_for(profile());
		}

		void begin_for(const core::host_profile_id aProfile)
		{
			Host.dispatch(core::begin_device_authorization_intent{
				.Profile = aProfile,
			});
		}

		[[nodiscard]] auto acquired(
			const std::uint32_t    aExpires    = 900U,
			const std::uint32_t    aInterval   = 5U,
			const std::string_view aDeviceCode = "device-code-1")
			-> application::device_code_completion
		{
			return application::device_code_acquired{
				.Context = Transport.last_code_context(),
				.Grant   = application::device_code_acquisition{
					.DeviceCode       = material(aDeviceCode),
					.UserCode         = "WDJB-MJHT",
					.VerificationUri  = "https://github.com/login/device",
					.ExpiresInSeconds = aExpires,
					.IntervalSeconds  = aInterval,
				},
			};
		}

		// Runs the flow to the moment the reviewer is looking at a user code.
		void reach_awaiting(
			const std::uint32_t aExpires  = 900U,
			const std::uint32_t aInterval = 5U)
		{
			begin();
			Transport.answer_code(acquired(aExpires, aInterval));
			Lane.drain();
		}

		[[nodiscard]] auto flow_context() const
			-> core::device_authorization_request_context
		{
			return Transport.last_code_context();
		}

		[[nodiscard]] auto issued(
			const std::uint32_t    aLifetime = 0U,
			const std::string_view aAccess   = "access-token-1")
			-> application::device_token_completion
		{
			return application::device_token_issued{
				.Context = flow_context(),
				.Grant   = application::device_token_material{
					.AccessToken                  = material(aAccess),
					.RefreshToken                 = std::nullopt,
					.ExpiresInSeconds             = aLifetime,
					.RefreshTokenExpiresInSeconds = 0U,
				},
			};
		}

		[[nodiscard]] auto stage() const -> core::device_authorization_stage
		{
			return Host.state().Stage;
		}

		// Advances the controlled clock and lets the armed timer expire, which
		// is the only way time passes in these tests.
		void elapse_to_next_poll()
		{
			expect(Timer.armed(), "a poll is armed");
			Clock.advance(Timer.last_delay());
			Timer.fire();
			Lane.drain();
		}

		application::manual_executor Lane;
		fake_clock                   Clock;
		fake_timer                   Timer;
		scripted_transport           Transport;
		unavailable_vault            Vault;
		recording_enrollment         Enrollment;
		recording_projections        Projections;
		recording_browser            Browser;

		device_authorization_application_host Host;
	};

	void test_the_successful_course_enrolls_once_and_erases_everything()
	{
		harness world;
		world.reach_awaiting();

		expect(
			world.stage()
				== core::device_authorization_stage::awaiting_authorization,
			"the flow is waiting for the reviewer");
		expect(
			world.Projections.last().UserCode == "WDJB-MJHT",
			"the user code reaches presentation");
		expect(
			world.Projections.last().VerificationUri
				== "https://github.com/login/device",
			"and so does the address to type it into");
		expect(
			world.Projections.last().RemainingValiditySeconds == 900U,
			"with the whole validity left");

		world.elapse_to_next_poll();
		expect(
			world.Transport.TokenRequests.size() == 1U,
			"one poll was issued");
		expect(
			world.Transport.TokenRequests.front().Code == "device-code-1",
			"carrying the exact device code");

		world.Transport.answer_token(
			application::device_token_deferred{
				.Context = world.flow_context(),
				.Reason  = application::device_token_delay_reason::
					authorization_pending,
				.SlowDownIntervalSeconds = 0U,
			});
		world.Lane.drain();
		expect(
			world.stage()
				== core::device_authorization_stage::awaiting_authorization,
			"a pending answer keeps the flow alive");

		world.elapse_to_next_poll();
		world.Transport.answer_token(world.issued());
		world.Lane.drain();

		expect(
			world.stage() == core::device_authorization_stage::authorized,
			"the flow reaches authorized");
		expect(
			world.Enrollment.Profiles.size() == 1U,
			"the credential crosses the vault boundary exactly once");
		expect(
			world.Enrollment.Profiles.front() == profile(),
			"under the profile the intent named");
		expect(
			world.Enrollment.Credentials.front() == "access-token-1",
			"and carries the access token and nothing else");
		expect(
			!world.Host.holds_transient_material(),
			"success erases the device code as deterministically as failure");
		expect(
			world.Projections.last().Stage
				== core::device_authorization_stage::authorized,
			"the terminal stage is published");
		expect(
			world.Projections.last().UserCode.empty()
				&& world.Projections.last().VerificationUri.empty()
				&& world.Projections.last().RemainingValiditySeconds == 0U,
			"and the user code does not outlive the flow that issued it");
		expect(world.Host.is_quiescent(), "nothing is left outstanding");
	}

	void test_the_first_poll_waits_the_server_interval()
	{
		harness world;
		world.reach_awaiting(900U, 7U);

		expect(
			world.Transport.TokenRequests.empty(),
			"the token endpoint is not polled the instant the code arrives");
		expect(world.Timer.armed(), "a poll is armed instead");
		expect(
			world.Timer.last_delay() == 7U,
			"one server interval away");

		world.elapse_to_next_poll();
		world.Transport.answer_token(
			application::device_token_deferred{
				.Context = world.flow_context(),
				.Reason  = application::device_token_delay_reason::
					authorization_pending,
				.SlowDownIntervalSeconds = 0U,
			});
		world.Lane.drain();
		expect(
			world.Timer.last_delay() == 7U,
			"and every later poll is a whole interval after the last");
	}

	void test_slow_down_lengthens_the_interval_for_every_later_poll()
	{
		harness world;
		world.reach_awaiting(900U, 5U);
		world.elapse_to_next_poll();

		world.Transport.answer_token(
			application::device_token_deferred{
				.Context = world.flow_context(),
				.Reason  = application::device_token_delay_reason::
					slow_down,
				.SlowDownIntervalSeconds = 10U,
			});
		world.Lane.drain();
		expect(
			world.Timer.last_delay() == 10U,
			"slow_down applies the server's larger interval");

		world.elapse_to_next_poll();
		world.Transport.answer_token(
			application::device_token_deferred{
				.Context = world.flow_context(),
				.Reason  = application::device_token_delay_reason::
					authorization_pending,
				.SlowDownIntervalSeconds = 0U,
			});
		world.Lane.drain();
		expect(
			world.Timer.last_delay() == 10U,
			"and the longer interval stays in force afterwards");
	}

	void test_the_total_bound_ends_a_flow_the_server_would_keep_open()
	{
		harness world{ application::device_authorization_host_limits{
			.TotalBoundSeconds   = 30U,
			.MaximumPolls        = 256U,
			.RefreshLeadSeconds  = 300U,
			.MaximumPollFailures = 3U,
		} };
		// The server says the code is good for an hour. The host's own bound is
		// half a minute, and the host's bound is the one that decides.
		world.reach_awaiting(3'600U, 5U);

		for (auto poll = 0; poll < 20; ++poll) {
			if (world.stage()
				!= core::device_authorization_stage::awaiting_authorization) {
				break;
			}
			world.elapse_to_next_poll();
			if (world.Transport.TokenCompletion) {
				world.Transport.answer_token(
					application::device_token_deferred{
						.Context = world.flow_context(),
						.Reason  = application::device_token_delay_reason::
							authorization_pending,
						.SlowDownIntervalSeconds = 0U,
					});
				world.Lane.drain();
			}
		}

		expect(
			world.stage() == core::device_authorization_stage::failed,
			"the host stops at its own bound");
		expect(
			world.Host.state().TerminalReason
				== core::device_authorization_reason_code::timed_out,
			"and says it timed out rather than blaming the server");
		expect(
			world.Clock.Now <= std::chrono::seconds{ 35 },
			"no poll was issued past the bound");
		expect(
			!world.Host.holds_transient_material(),
			"the device code is gone");
	}

	void test_the_device_code_expiry_ends_the_flow()
	{
		harness world;
		world.reach_awaiting(8U, 5U);

		world.elapse_to_next_poll();
		world.Transport.answer_token(
			application::device_token_deferred{
				.Context = world.flow_context(),
				.Reason  = application::device_token_delay_reason::
					authorization_pending,
				.SlowDownIntervalSeconds = 0U,
			});
		world.Lane.drain();

		// The next poll would land at ten seconds, past the code's validity, so
		// the host wakes at the deadline instead.
		expect(world.Timer.last_delay() == 3U, "the wait is clamped to expiry");
		world.elapse_to_next_poll();

		expect(
			world.stage() == core::device_authorization_stage::expired,
			"the flow expires");
		expect(
			world.Transport.TokenRequests.size() == 1U,
			"and no poll was issued after the code stopped working");
	}

	void test_every_refusal_reaches_its_own_terminal()
	{
		{
			harness world;
			world.reach_awaiting();
			world.elapse_to_next_poll();
			world.Transport.answer_token(application::device_token_refused{
				.Context = world.flow_context(),
				.Reason  = application::device_token_refusal_reason::access_denied,
			});
			world.Lane.drain();
			expect(
				world.stage() == core::device_authorization_stage::denied,
				"a denied authorization is denied");
			expect(
				!world.Host.holds_transient_material(),
				"and erases the device code");
		}
		{
			harness world;
			world.reach_awaiting();
			world.elapse_to_next_poll();
			world.Transport.answer_token(application::device_token_refused{
				.Context = world.flow_context(),
				.Reason  = application::device_token_refusal_reason::expired_token,
			});
			world.Lane.drain();
			expect(
				world.stage() == core::device_authorization_stage::expired,
				"a server-reported expiry expires the flow");
		}
		{
			harness world;
			world.begin();
			world.Transport.answer_code(application::device_code_refused{
				.Context = world.Transport.last_code_context(),
			});
			world.Lane.drain();
			expect(
				world.stage() == core::device_authorization_stage::disabled,
				"a disabled device flow is reported as disabled");
			expect(
				world.Host.state().TerminalReason
					== core::device_authorization_reason_code::device_flow_disabled,
				"with the exact reason");
		}
	}

	void test_a_transport_failure_acquiring_the_code_fails_the_flow()
	{
		harness world;
		world.begin();
		world.Transport.answer_code(
			application::device_authorization_transport_error{
				.Context = world.Transport.last_code_context(),
				.Failure = application::device_authorization_transport_failure::
					certificate,
			});
		world.Lane.drain();

		expect(
			world.stage() == core::device_authorization_stage::failed,
			"the flow fails");
		expect(
			world.Host.state().TerminalReason
				== core::device_authorization_reason_code::certificate_rejected,
			"with the failure mapped exactly");
		expect(world.Host.is_quiescent(), "and nothing is left armed");
	}

	// A blip is worth another poll. An answer about who the server is, is not.
	void test_a_network_failure_mid_poll_is_retried_within_the_window()
	{
		harness world;
		world.reach_awaiting();
		world.elapse_to_next_poll();

		world.Transport.answer_token(
			application::device_authorization_transport_error{
				.Context = world.flow_context(),
				.Failure = application::device_authorization_transport_failure::
					network,
			});
		world.Lane.drain();
		expect(
			world.stage()
				== core::device_authorization_stage::awaiting_authorization,
			"one network failure does not end the flow");
		expect(world.Timer.armed(), "the next poll is armed");

		world.elapse_to_next_poll();
		world.Transport.answer_token(world.issued());
		world.Lane.drain();
		expect(
			world.stage() == core::device_authorization_stage::authorized,
			"and the reviewer's authorization still lands");
	}

	void test_a_certificate_failure_mid_poll_ends_the_flow_at_once()
	{
		harness world;
		world.reach_awaiting();
		world.elapse_to_next_poll();

		world.Transport.answer_token(
			application::device_authorization_transport_error{
				.Context = world.flow_context(),
				.Failure = application::device_authorization_transport_failure::
					certificate,
			});
		world.Lane.drain();

		expect(
			world.stage() == core::device_authorization_stage::failed,
			"a refused certificate is not retried");
		expect(
			world.Host.state().TerminalReason
				== core::device_authorization_reason_code::certificate_rejected,
			"and reports itself exactly");
	}

	void test_repeated_network_failures_end_the_flow()
	{
		harness world{ application::device_authorization_host_limits{
			.TotalBoundSeconds   = 900U,
			.MaximumPolls        = 256U,
			.RefreshLeadSeconds  = 300U,
			.MaximumPollFailures = 2U,
		} };
		world.reach_awaiting();

		for (auto attempt = 0; attempt < 3; ++attempt) {
			world.elapse_to_next_poll();
			world.Transport.answer_token(
				application::device_authorization_transport_error{
					.Context = world.flow_context(),
					.Failure = application::device_authorization_transport_failure::
						network,
				});
			world.Lane.drain();
		}

		expect(
			world.stage() == core::device_authorization_stage::failed,
			"the host stops retrying at its own tolerance");
		expect(
			world.Host.state().TerminalReason
				== core::device_authorization_reason_code::network_unavailable,
			"and reports the failure it kept seeing");
	}

	void test_cancellation_erases_first_and_confirms_afterwards()
	{
		harness world;
		world.reach_awaiting();

		world.Host.dispatch(core::cancel_device_authorization_intent{ });
		expect(
			world.stage() == core::device_authorization_stage::cancelling,
			"the core enters cancellation before the host confirms");
		expect(
			!world.Host.holds_transient_material(),
			"the device code is erased before anything is confirmed");
		expect(
			world.Transport.Cancellations.size() == 1U,
			"the transport was told to stop");
		expect(!world.Timer.armed(), "and the poll timer was withdrawn");

		world.Lane.drain();
		expect(
			world.stage() == core::device_authorization_stage::cancelled,
			"the host confirms erasure through the cancelled observation");
	}

	void test_a_token_success_arriving_after_cancellation_enrolls_nothing()
	{
		harness world;
		world.reach_awaiting();
		world.elapse_to_next_poll();

		// The reviewer authorized at the same moment the flow was cancelled.
		world.Host.dispatch(core::cancel_device_authorization_intent{ });
		world.Lane.drain();
		expect(
			world.stage() == core::device_authorization_stage::cancelled,
			"cancellation completed");

		world.Transport.answer_token(world.issued());
		world.Lane.drain();

		expect(
			world.Enrollment.Profiles.empty(),
			"a late success enrolls nothing");
		expect(
			world.stage() == core::device_authorization_stage::cancelled,
			"and cannot move the flow off its terminal");
		expect(
			!world.Host.holds_transient_material(),
			"the material it carried was released rather than kept");
	}

	void test_shutdown_cancels_erases_and_confirms()
	{
		harness world;
		world.reach_awaiting();

		world.Host.shutdown();
		expect(world.Host.is_closing(), "the host is closing");
		expect(
			world.stage() == core::device_authorization_stage::cancelled,
			"shutdown confirms the cancelled terminal rather than abandoning it");
		expect(
			!world.Host.holds_transient_material(),
			"and erases the device code");
		expect(
			world.Transport.Cancellations.size() == 1U,
			"the transport was told to stop");
		expect(!world.Timer.armed(), "no timer survives shutdown");

		world.Host.dispatch(core::begin_device_authorization_intent{
			.Profile = profile(),
		});
		expect(
			world.Transport.CodeRequests.size() == 1U,
			"ingress is closed after shutdown");
	}

	void test_a_duplicate_completion_changes_nothing()
	{
		harness world;
		world.reach_awaiting();
		world.elapse_to_next_poll();

		const auto context = world.flow_context();
		world.Transport.answer_token(world.issued());
		world.Lane.drain();
		expect(
			world.stage() == core::device_authorization_stage::authorized,
			"the flow is authorized");

		// The same terminal again, straight into the host's own ingress.
		world.Host.dispatch(core::device_authorization_authorized_event{
			.Context = context,
		});
		expect(
			world.Enrollment.Profiles.size() == 1U,
			"a duplicate observation enrolls nothing a second time");
		expect(
			world.stage() == core::device_authorization_stage::authorized,
			"and leaves the terminal where it was");
	}

	void test_a_completion_from_a_superseded_flow_is_ignored()
	{
		harness world;
		world.reach_awaiting();
		world.elapse_to_next_poll();
		const auto stale = world.flow_context();

		world.Host.dispatch(core::cancel_device_authorization_intent{ });
		world.Lane.drain();
		world.begin();
		expect(
			world.Transport.CodeRequests.size() == 2U,
			"a second flow started under a newer generation");
		world.Transport.answer_code(world.acquired(900U, 5U, "device-code-2"));
		world.Lane.drain();

		// The first flow's poll finally answers.
		world.Transport.answer_token(application::device_token_issued{
			.Context = stale,
			.Grant   = application::device_token_material{
				.AccessToken                  = material("stale-access"),
				.RefreshToken                 = std::nullopt,
				.ExpiresInSeconds             = 0U,
				.RefreshTokenExpiresInSeconds = 0U,
			},
		});
		world.Lane.drain();

		expect(
			world.Enrollment.Profiles.empty(),
			"the superseded answer enrolls nothing");
		expect(
			world.stage()
				== core::device_authorization_stage::awaiting_authorization,
			"and does not disturb the live flow");
	}

	void test_a_timer_that_cannot_be_armed_fails_the_flow()
	{
		harness world;
		world.begin();
		world.Timer.Refuse = true;
		world.Transport.answer_code(world.acquired());
		world.Lane.drain();

		expect(
			world.stage() == core::device_authorization_stage::failed,
			"a flow whose bound cannot be enforced fails instead of hanging");
		expect(
			world.Host.state().TerminalReason
				== core::device_authorization_reason_code::timed_out,
			"and says so as a timeout");
		expect(!world.Host.holds_transient_material(), "erasing as it goes");
	}

	void test_a_throwing_projection_cannot_veto_the_transition()
	{
		harness world;
		world.begin();
		world.Projections.Throw = true;
		world.Transport.answer_code(world.acquired());
		world.Lane.drain();

		expect(
			world.stage()
				== core::device_authorization_stage::awaiting_authorization,
			"the accepted transition stands");
		expect(
			world.Host.projection_delivery_failed(),
			"and the failed delivery is reported rather than swallowed");
	}

	void test_a_terminal_flow_can_start_a_fresh_one()
	{
		harness world;
		world.reach_awaiting();
		world.elapse_to_next_poll();
		world.Transport.answer_token(application::device_token_refused{
			.Context = world.flow_context(),
			.Reason  = application::device_token_refusal_reason::access_denied,
		});
		world.Lane.drain();
		expect(
			world.stage() == core::device_authorization_stage::denied,
			"the first flow was denied");

		const auto first = world.Host.state().Generation;
		world.begin();
		expect(
			world.Host.state().Generation.value() > first.value(),
			"the second flow runs under a strictly newer generation");
		world.Transport.answer_code(world.acquired(900U, 5U, "device-code-2"));
		world.Lane.drain();
		expect(
			world.stage()
				== core::device_authorization_stage::awaiting_authorization,
			"and reaches the reviewer again");
	}

	// -- capability --------------------------------------------------------

	void test_registered_connection_replacement_between_flows()
	{
		harness world;
		expect(!world.Host.replace_profile(foreign_profile(), foreign_profile()) && !world.Host.replace_profile(profile(), { }), "stale and empty replacement identities are refused");
		expect(world.Host.replace_profile(profile(), foreign_profile()), "composition can bind a fresh connection ID to its registered server between flows");
		world.begin();
		world.Lane.drain();
		expect(world.Transport.CodeRequests.empty(), "the retired connection identity cannot launch the device wire");
		world.begin_for(foreign_profile());
		world.Lane.drain();
		expect(world.Transport.CodeRequests.size() == 1U && !world.Host.replace_profile(foreign_profile(), profile()), "active flow pins the replacement capability identity");
		world.Transport.answer_code(world.acquired());
		world.Lane.drain();
		world.Host.dispatch(core::cancel_device_authorization_intent{ });
		world.Lane.drain();
		expect(world.Host.replace_profile(foreign_profile(), profile()), "settled cancellation allows the next validated replacement");
		world.Host.shutdown();
		expect(!world.Host.replace_profile(profile(), foreign_profile()), "shutdown closes profile replacement ingress");
		harness unregistered{ { }, true, application::default_device_verification_authority, false };
		expect(!unregistered.Host.replace_profile(profile(), foreign_profile()), "replacement cannot manufacture a registered capability");
	}

	void test_an_unregistered_build_refuses_before_it_builds_a_request()
	{
		harness world{ { }, true, application::default_device_verification_authority, false };
		world.begin();
		world.Lane.drain();

		expect(
			world.stage() == core::device_authorization_stage::unavailable,
			"an unregistered build reaches the unavailable terminal");
		expect(
			world.Transport.CodeRequests.empty(),
			"having constructed no device-code request");
		expect(
			world.Transport.TokenRequests.empty(),
			"and no token request either");
		expect(
			!world.Timer.armed(),
			"nothing is scheduled, so nothing will try again");
		expect(
			!world.Host.holds_transient_material(),
			"and no transient material was ever held");
		expect(
			world.Projections.last().Stage
				== core::device_authorization_stage::unavailable,
			"presentation is told, so the choice stays visible and typed");
	}

	void test_an_unregistered_build_opens_no_browser_either()
	{
		harness world{ { }, true, application::default_device_verification_authority, false };
		world.begin();
		world.Lane.drain();

		world.Host.open_verification_address();
		expect(
			world.Browser.Addresses.empty(),
			"there is no address to open, and none was invented");
	}

	void test_an_unregistered_build_can_be_asked_again_and_still_refuses()
	{
		harness world{ { }, true, application::default_device_verification_authority, false };
		for (auto attempt = 0; attempt < 3; ++attempt) {
			world.begin();
			world.Lane.drain();
			expect(
				world.stage() == core::device_authorization_stage::unavailable,
				"every attempt reaches the same typed terminal");
		}
		expect(
			world.Transport.CodeRequests.empty(),
			"and no attempt ever built a request");
	}

	void test_a_begin_for_another_profile_is_refused_before_the_transport()
	{
		harness world;
		world.begin_for(foreign_profile());
		world.Lane.drain();

		expect(
			world.stage() == core::device_authorization_stage::unavailable,
			"a begin for a profile the capability was not composed for "
			"reaches the unavailable terminal");
		expect(
			world.Transport.CodeRequests.empty(),
			"having constructed no device-code request");
		expect(
			world.Transport.TokenRequests.empty(),
			"and no token request either");
		expect(
			world.Vault.Calls == 0U,
			"the vault was never asked for anything");
		expect(
			world.Enrollment.Profiles.empty(),
			"and nothing was enrolled under a profile that issued nothing");
		expect(
			!world.Timer.armed(),
			"nothing is scheduled, so nothing will try again");
		expect(
			!world.Host.holds_transient_material(),
			"and no transient material was ever held");
		expect(
			world.Projections.last().Stage
				== core::device_authorization_stage::unavailable,
			"presentation is told, so the refusal is visible and typed");
	}

	void test_the_composed_profile_still_begins_after_a_foreign_refusal()
	{
		harness world;
		world.begin_for(foreign_profile());
		world.Lane.drain();

		world.begin();
		world.Lane.drain();
		expect(
			world.Transport.CodeRequests.size() == 1U,
			"the composed profile's begin still reaches the transport");
		expect(
			world.Transport.CodeRequests.back().Profile == profile(),
			"under the profile the capability was composed for");

		world.Transport.answer_code(world.acquired());
		world.Lane.drain();
		expect(
			world.stage()
				== core::device_authorization_stage::awaiting_authorization,
			"and runs exactly as it did before the refusal");
	}

	// -- browser handoff ---------------------------------------------------

	void test_the_authority_check_admits_only_the_compiled_host()
	{
		using application::address_matches_verification_authority;
		constexpr std::string_view authority = "github.com";

		expect(
			address_matches_verification_authority(
				"https://github.com/login/device",
				authority),
			"the exact compiled authority over HTTPS is admitted");
		expect(
			address_matches_verification_authority(
				"https://GitHub.com/login/device",
				authority),
			"host case carries no meaning and is folded");
		expect(
			address_matches_verification_authority("https://github.com", authority),
			"an address with no path is still the authority");
		expect(
			address_matches_verification_authority(
				"https://github.com?next=1",
				authority),
			"a query terminates the host just as a slash does");

		expect(
			!address_matches_verification_authority(
				"http://github.com/login/device",
				authority),
			"plain HTTP is refused even for the right host");
		expect(
			!address_matches_verification_authority(
				"https://github.com.evil.invalid/login/device",
				authority),
			"a longer host that merely starts with the authority is refused");
		expect(
			!address_matches_verification_authority(
				"https://evil.invalid/github.com",
				authority),
			"the authority appearing in the path proves nothing");
		expect(
			!address_matches_verification_authority(
				"https://github.com@evil.invalid/login",
				authority),
			"userinfo that reads as the authority is refused");
		expect(
			!address_matches_verification_authority(
				"https://github.com:8443/login/device",
				authority),
			"a port the compiled authority does not name is refused");
		expect(
			!address_matches_verification_authority(
				"https://api.github.com/login/device",
				authority),
			"a subdomain is not the authority");
		expect(
			!address_matches_verification_authority(
				"https://github.com/login/device",
				{ }),
			"an empty authority admits nothing rather than everything");
		expect(
			!address_matches_verification_authority({ }, authority),
			"an empty address is not the authority either");
	}

	void test_a_browser_handoff_opens_the_exact_presented_address()
	{
		harness world;
		world.reach_awaiting();
		expect(
			world.Projections.last().Handoff
				== application::device_authorization_handoff::not_attempted,
			"a fresh flow has attempted no handoff");

		world.Host.open_verification_address();
		expect(
			world.Browser.Addresses.size() == 1U,
			"the browser was invoked exactly once");
		expect(
			world.Browser.Addresses.front() == "https://github.com/login/device",
			"with the address the reviewer is looking at and nothing else");
		expect(
			world.Projections.last().Handoff
				== application::device_authorization_handoff::opened,
			"and presentation is told the open was accepted");
		expect(
			world.stage()
				== core::device_authorization_stage::awaiting_authorization,
			"opening a browser moves no authorization state");
	}

	void test_an_off_authority_address_is_refused_with_no_browser_call()
	{
		harness world;
		world.begin();
		world.Transport.answer_code(
			application::device_code_acquired{
				.Context = world.Transport.last_code_context(),
				.Grant   = application::device_code_acquisition{
					.DeviceCode       = material("device-code-1"),
					.UserCode         = "WDJB-MJHT",
					.VerificationUri  = "https://github.com.evil.invalid/login/device",
					.ExpiresInSeconds = 900U,
					.IntervalSeconds  = 5U,
				},
			});
		world.Lane.drain();

		world.Host.open_verification_address();
		expect(
			world.Browser.Addresses.empty(),
			"no browser was opened for an address off the compiled authority");
		expect(
			world.Projections.last().Handoff
				== application::device_authorization_handoff::refused,
			"and the refusal is typed rather than silent");
		expect(
			world.stage()
				== core::device_authorization_stage::awaiting_authorization,
			"a refused address does not fail the authorization");
		expect(
			world.Projections.last().UserCode == "WDJB-MJHT",
			"the code stays on screen so the reviewer can still act");
	}

	void test_a_build_with_no_browser_says_so_instead_of_failing()
	{
		harness world{ { }, false };
		world.reach_awaiting();

		world.Host.open_verification_address();
		expect(
			world.Projections.last().Handoff
				== application::device_authorization_handoff::unavailable,
			"a build with no browser capability reports unavailable");
		expect(
			world.stage()
				== core::device_authorization_stage::awaiting_authorization,
			"and the flow is untouched");
	}

	void test_a_failing_opener_leaves_the_flow_completable_elsewhere()
	{
		harness world;
		world.Browser.Accept = false;
		world.reach_awaiting();

		world.Host.open_verification_address();
		expect(
			world.Browser.Addresses.size() == 1U,
			"the capability was asked");
		expect(
			world.Projections.last().Handoff
				== application::device_authorization_handoff::failed,
			"and its refusal is reported as a handoff failure");
		expect(
			world.stage()
				== core::device_authorization_stage::awaiting_authorization,
			"which is not an authorization failure");
		expect(
			world.Projections.last().UserCode == "WDJB-MJHT"
				&& world.Projections.last().VerificationUri
					== "https://github.com/login/device",
			"the code and address are still presented");

		// The whole point: the reviewer authorized on their phone instead.
		world.elapse_to_next_poll();
		world.Transport.answer_token(world.issued());
		world.Lane.drain();
		expect(
			world.stage() == core::device_authorization_stage::authorized,
			"and the flow still completes when authorization happens elsewhere");
		expect(
			world.Enrollment.Profiles.size() == 1U,
			"enrolling exactly once");
	}

	void test_a_throwing_browser_cannot_disturb_the_flow()
	{
		harness world;
		world.Browser.Throw = true;
		world.reach_awaiting();

		world.Host.open_verification_address();
		expect(
			world.Projections.last().Handoff
				== application::device_authorization_handoff::failed,
			"a browser that throws is a handoff failure and nothing more");
		expect(
			world.stage()
				== core::device_authorization_stage::awaiting_authorization,
			"the flow is still live");
	}

	void test_a_handoff_result_does_not_outlive_its_stage()
	{
		harness world;
		world.Browser.Accept = false;
		world.reach_awaiting();
		world.Host.open_verification_address();
		expect(
			world.Projections.last().Handoff
				== application::device_authorization_handoff::failed,
			"the failure is reported");

		world.Host.dispatch(core::cancel_device_authorization_intent{ });
		world.Lane.drain();
		expect(
			world.Projections.last().Handoff
				== application::device_authorization_handoff::not_attempted,
			"and is cleared the moment the stage changes");
	}

	void test_nothing_opens_when_there_is_nothing_to_open()
	{
		harness world;
		world.Host.open_verification_address();
		expect(
			world.Browser.Addresses.empty(),
			"an idle host opens no browser");
		expect(
			world.Projections.Published.empty(),
			"and publishes nothing it was not asked for");

		world.reach_awaiting();
		world.Host.dispatch(core::cancel_device_authorization_intent{ });
		world.Lane.drain();
		const auto published = world.Projections.Published.size();
		world.Host.open_verification_address();
		expect(
			world.Browser.Addresses.empty(),
			"a cancelled flow opens no browser either");
		expect(
			world.Projections.Published.size() == published,
			"and republishes nothing");
	}

	void test_shutdown_closes_the_handoff_with_everything_else()
	{
		harness world;
		world.reach_awaiting();
		world.Host.shutdown();

		world.Host.open_verification_address();
		expect(
			world.Browser.Addresses.empty(),
			"a closing host opens no browser");
	}
} // namespace

auto main() -> int
{
	test_the_first_poll_waits_a_whole_interval();
	test_polls_are_never_closer_together_than_the_interval();
	test_a_server_interval_below_the_floor_is_raised_to_it();
	test_slow_down_only_ever_slows_the_host_down();
	test_slow_down_changes_the_next_poll_and_not_the_last_one();
	test_expiry_ends_the_course_and_no_poll_is_scheduled_past_it();
	test_the_total_bound_ends_a_course_the_server_would_keep_open();
	test_expiry_wins_the_tie_with_the_total_bound();
	test_the_poll_ceiling_ends_the_course();
	test_hostile_bounds_are_clamped_into_a_usable_schedule();
	test_remaining_validity_counts_to_the_nearer_terminal();
	test_an_out_of_order_elapsed_second_never_moves_a_poll_earlier();
	test_a_whole_replayed_course_polls_at_exactly_the_expected_seconds();

	test_the_successful_course_enrolls_once_and_erases_everything();
	test_the_first_poll_waits_the_server_interval();
	test_slow_down_lengthens_the_interval_for_every_later_poll();
	test_the_total_bound_ends_a_flow_the_server_would_keep_open();
	test_the_device_code_expiry_ends_the_flow();
	test_every_refusal_reaches_its_own_terminal();
	test_a_transport_failure_acquiring_the_code_fails_the_flow();
	test_a_network_failure_mid_poll_is_retried_within_the_window();
	test_a_certificate_failure_mid_poll_ends_the_flow_at_once();
	test_repeated_network_failures_end_the_flow();
	test_cancellation_erases_first_and_confirms_afterwards();
	test_a_token_success_arriving_after_cancellation_enrolls_nothing();
	test_shutdown_cancels_erases_and_confirms();
	test_a_duplicate_completion_changes_nothing();
	test_a_completion_from_a_superseded_flow_is_ignored();
	test_a_timer_that_cannot_be_armed_fails_the_flow();
	test_a_throwing_projection_cannot_veto_the_transition();
	test_a_terminal_flow_can_start_a_fresh_one();

	test_registered_connection_replacement_between_flows();
	test_an_unregistered_build_refuses_before_it_builds_a_request();
	test_an_unregistered_build_opens_no_browser_either();
	test_an_unregistered_build_can_be_asked_again_and_still_refuses();
	test_a_begin_for_another_profile_is_refused_before_the_transport();
	test_the_composed_profile_still_begins_after_a_foreign_refusal();

	test_the_authority_check_admits_only_the_compiled_host();
	test_a_browser_handoff_opens_the_exact_presented_address();
	test_an_off_authority_address_is_refused_with_no_browser_call();
	test_a_build_with_no_browser_says_so_instead_of_failing();
	test_a_failing_opener_leaves_the_flow_completable_elsewhere();
	test_a_throwing_browser_cannot_disturb_the_flow();
	test_a_handoff_result_does_not_outlive_its_stage();
	test_nothing_opens_when_there_is_nothing_to_open();
	test_shutdown_closes_the_handoff_with_everything_else();

	std::cout << "device-authorization-host-tests:pass\n";
	return EXIT_SUCCESS;
}
