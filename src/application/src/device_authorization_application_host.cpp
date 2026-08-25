#include "bpr/application/device_authorization_application_host.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace bpr::application {
	namespace {
		// Every transport failure has exactly one core failure. The mapping is a
		// table rather than a judgement so no host decision can widen what a
		// diagnostic is allowed to say.
		[[nodiscard]] auto core_failure(
			const device_authorization_transport_failure aFailure) noexcept
			-> core::device_authorization_failure
		{
			switch (aFailure) {
			case device_authorization_transport_failure::network:
				return core::device_authorization_failure::network;
			case device_authorization_transport_failure::certificate:
				return core::device_authorization_failure::certificate;
			case device_authorization_transport_failure::redirect:
				return core::device_authorization_failure::redirect;
			case device_authorization_transport_failure::timeout:
				return core::device_authorization_failure::timeout;
			case device_authorization_transport_failure::malformed_response:
				return core::device_authorization_failure::malformed_response;
			case device_authorization_transport_failure::response_too_large:
				return core::device_authorization_failure::response_too_large;
			}
			return core::device_authorization_failure::network;
		}

		// Only ASCII case is folded. A host is compared byte for byte
		// otherwise, so no locale and no Unicode equivalence can make two
		// different authorities compare equal.
		[[nodiscard]] constexpr auto ascii_lowered(const char aValue) noexcept
			-> char
		{
			return (aValue >= 'A' && aValue <= 'Z')
				? static_cast<char>(aValue - 'A' + 'a')
				: aValue;
		}

		// A blip in the network is worth another poll inside the window the
		// reviewer is already waiting through. A refused certificate, a refused
		// redirect, or a response that did not parse is not: those are answers
		// about who the server is and what it said, and repeating the request
		// cannot change either.
		[[nodiscard]] auto retryable(
			const device_authorization_transport_failure aFailure) noexcept -> bool
		{
			return aFailure == device_authorization_transport_failure::network
				|| aFailure == device_authorization_transport_failure::timeout;
		}

		[[nodiscard]] auto elapsed_seconds_between(
			const std::chrono::milliseconds aFrom,
			const std::chrono::milliseconds aTo) noexcept -> std::uint32_t
		{
			if (aTo <= aFrom) {
				return 0U;
			}
			const auto seconds = (aTo - aFrom).count() / 1'000;
			if (seconds
				>= static_cast<std::int64_t>(
					std::numeric_limits<std::uint32_t>::max())) {
				return std::numeric_limits<std::uint32_t>::max();
			}
			return static_cast<std::uint32_t>(seconds);
		}

		[[nodiscard]] auto context_of(
			const device_code_completion& aCompletion) noexcept
			-> core::device_authorization_request_context
		{
			return std::visit(
				[](const auto& aTerminal) noexcept {
					return aTerminal.Context;
				},
				aCompletion);
		}

		[[nodiscard]] auto context_of(
			const device_token_completion& aCompletion) noexcept
			-> core::device_authorization_request_context
		{
			return std::visit(
				[](const auto& aTerminal) noexcept {
					return aTerminal.Context;
				},
				aCompletion);
		}

		[[nodiscard]] auto vault_context_of(
			const github_credential_store_completion& aCompletion) noexcept
			-> github_credential_operation_context
		{
			return std::visit(
				[](const auto& aTerminal) noexcept {
					return aTerminal.Context;
				},
				aCompletion);
		}

		[[nodiscard]] auto vault_context_of(
			const github_credential_retrieve_completion& aCompletion) noexcept
			-> github_credential_operation_context
		{
			return std::visit(
				[](const auto& aTerminal) noexcept {
					return aTerminal.Context;
				},
				aCompletion);
		}

		class system_device_authorization_clock final
			: public device_authorization_clock_port
		{
		public:
			[[nodiscard]] auto observe_monotonic() noexcept
				-> std::chrono::milliseconds override
			{
				return std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::steady_clock::now().time_since_epoch());
			}
		};

		enum class timer_purpose : std::uint8_t {
			none,
			poll,
			refresh,
		};
	}

	github_host_device_authorization_enrollment::
		github_host_device_authorization_enrollment(
			github_application_host& aHost) noexcept
		: Host{ &aHost }
	{
	}

	void github_host_device_authorization_enrollment::enroll(
		const core::host_profile_id aProfile,
		github_credential_material  aCredential)
	{
		Host->enroll_profile(aProfile, std::move(aCredential));
	}

	auto make_system_device_authorization_clock()
		-> std::unique_ptr<device_authorization_clock_port>
	{
		return std::make_unique<system_device_authorization_clock>();
	}

	auto address_matches_verification_authority(
		const std::string_view aAddress,
		const std::string_view aAuthority) noexcept -> bool
	{
		constexpr std::string_view https = "https://";
		if (aAuthority.empty() || !aAddress.starts_with(https)) {
			return false;
		}

		const auto remainder = aAddress.substr(https.size());
		const auto hostEnd   = remainder.find_first_of("/?#");
		const auto host      = remainder.substr(0, hostEnd);
		// A host that carries userinfo or a port is not the compiled authority
		// even when the compiled authority is spelled somewhere inside it.
		if (host.size() != aAuthority.size()) {
			return false;
		}

		return std::ranges::equal(
			host,
			aAuthority,
			[](const char aLeft, const char aRight) noexcept {
				return ascii_lowered(aLeft) == ascii_lowered(aRight);
			});
	}

	struct device_authorization_application_host::implementation final
		: std::enable_shared_from_this<implementation>
	{
		implementation(
			core::device_authorization_state       aInitialState,
			device_authorization_transport_port&   aTransport,
			github_credential_vault_port&          aVault,
			device_authorization_enrollment_port&  aEnrollment,
			device_authorization_clock_port&       aClock,
			device_authorization_timer_port&       aTimer,
			decision_executor&                     aDecisionLane,
			device_authorization_projection_port&  aProjections,
			const device_authorization_host_limits aLimits,
			device_authorization_browser_handoff   aHandoff)
			: State{ std::move(aInitialState) }
			, Transport{ &aTransport }
			, Vault{ &aVault }
			, Enrollment{ &aEnrollment }
			, Clock{ &aClock }
			, Timer{ &aTimer }
			, DecisionLane{ &aDecisionLane }
			, Projections{ &aProjections }
			, Limits{ aLimits }
			, Handoff{ std::move(aHandoff) }
		{
		}

		// -- lane and clock ------------------------------------------------

		void post(std::function<void()> aTask)
		{
			DecisionLane->post(std::move(aTask));
		}

		// Submissions are counted rather than tracked individually because the
		// port contract already promises exactly one terminal each. The floor
		// keeps a double terminal from making a live host look quiescent.
		void release() noexcept
		{
			if (Outstanding > 0U) {
				--Outstanding;
			}
		}

		[[nodiscard]] auto now() noexcept -> std::chrono::milliseconds
		{
			return Clock->observe_monotonic();
		}

		[[nodiscard]] auto elapsed_seconds() noexcept -> std::uint32_t
		{
			return elapsed_seconds_between(GrantedAt, now());
		}

		// -- transitions ---------------------------------------------------

		void apply(const core::device_authorization_event& aEvent)
		{
			auto output = core::transition(State, aEvent);
			execute(output);
			publish(output);
		}

		void execute(const core::device_authorization_transition_output& aOutput)
		{
			for (const auto& request : aOutput.Requests) {
				std::visit(
					[this](const auto& aRequest) {
						perform(aRequest);
					},
					request);
			}
		}

		void publish(const core::device_authorization_transition_output& aOutput)
		{
			if (!aOutput.Status.has_value()) {
				return;
			}

			// A stage change is a new thing to look at, so whatever the last
			// browser attempt did stops being reported with it.
			LastHandoff = device_authorization_handoff::not_attempted;
			deliver();
		}

		// Republishes the current stage without a core transition. Only the
		// browser handoff uses this, because it is the only thing presentation
		// learns that the core never decided.
		void deliver()
		{
			auto presentation = device_authorization_presentation{
				.Stage                    = State.Stage,
				.UserCode                 = { },
				.VerificationUri          = { },
				.RemainingValiditySeconds = 0U,
				.Handoff                  = LastHandoff,
			};
			if (
				State.Stage
					== core::device_authorization_stage::awaiting_authorization
				&& Schedule.has_value()) {
				presentation.UserCode                 = UserCode;
				presentation.VerificationUri          = VerificationUri;
				presentation.RemainingValiditySeconds = Schedule->remaining_seconds(
					elapsed_seconds());
			}

			Presentation = std::move(presentation);
			try {
				Projections->publish(Presentation);
			} catch (...) {
				ProjectionDeliveryFailed = true;
			}
		}

		// -- browser handoff -----------------------------------------------

		void open_verification_address()
		{
			// Nothing is on screen to open. Saying so is more honest than
			// reporting a failure the reviewer cannot act on.
			if (
				State.Stage
					!= core::device_authorization_stage::awaiting_authorization
				|| VerificationUri.empty()) {
				return;
			}

			LastHandoff = resolve_handoff();
			deliver();
		}

		[[nodiscard]] auto resolve_handoff() -> device_authorization_handoff
		{
			// The address is checked before the capability is consulted, so a
			// build with no browser still refuses an off-authority address and
			// the refusal is never mistaken for a missing capability.
			if (
				!address_matches_verification_authority(
					VerificationUri,
					Handoff.Authority)) {
				return device_authorization_handoff::refused;
			}
			if (Handoff.Browser == nullptr) {
				return device_authorization_handoff::unavailable;
			}

			// A browser is somebody else's program. It may throw, it may
			// refuse, and neither is allowed to disturb a live grant.
			try {
				return Handoff.Browser->open(VerificationUri)
					? device_authorization_handoff::opened
					: device_authorization_handoff::failed;
			} catch (...) {
				return device_authorization_handoff::failed;
			}
		}

		// -- host requests -------------------------------------------------

		void perform(const core::begin_device_authorization_request& aRequest)
		{
			// A new flow supersedes anything the previous one left behind,
			// including refresh material that never found an authority.
			erase_flow_transients();
			PendingAccess.reset();
			PendingRefresh.reset();
			AuthorizingContext.reset();
			ConsecutiveFailures = 0U;

			Flow = aRequest.Context;
			++Outstanding;
			Transport->acquire_device_code(
				aRequest.Context,
				device_code_handler());
		}

		void perform(const core::cancel_device_authorization_request& aRequest)
		{
			const auto context = aRequest.Context;
			cancel_timer();
			Transport->cancel(context);
			erase_flow_transients();
			PendingAccess.reset();
			PendingRefresh.reset();
			AuthorizingContext.reset();

			// The core refuses every completion observation until the host says
			// erasure is done. This is that confirmation, and it is posted
			// rather than applied inline so one turn never nests inside another.
			const auto self = shared_from_this();
			post([self, context] {
				self->apply(core::device_authorization_cancelled_event{
					.Context = context,
				});
			});
		}

		// -- device code ---------------------------------------------------

		[[nodiscard]] auto device_code_handler()
			-> device_code_completion_handler
		{
			return [weak = weak_from_this()](device_code_completion aCompletion) {
				const auto self = weak.lock();
				if (self == nullptr) {
					return;
				}
				auto payload = std::make_shared<device_code_completion>(
					std::move(aCompletion));
				self->post([self, payload] {
					self->on_device_code(std::move(*payload));
				});
			};
		}

		void on_device_code(device_code_completion aCompletion)
		{
			release();
			const auto context = context_of(aCompletion);
			if (Closing || !Flow.has_value() || *Flow != context) {
				// Nothing is applied and nothing is kept. The completion owns
				// the device code, so letting it go here erases it.
				return;
			}
			if (State.Stage != core::device_authorization_stage::requested) {
				erase_flow_transients();
				return;
			}

			std::visit(
				[this, context](auto&& aTerminal) {
					accept(context, std::forward<decltype(aTerminal)>(aTerminal));
				},
				std::move(aCompletion));
		}

		void accept(
			const core::device_authorization_request_context& aContext,
			device_code_acquired&&                            aTerminal)
		{
			Schedule.emplace(device_authorization_schedule_bounds{
				.IntervalSeconds   = aTerminal.Grant.IntervalSeconds,
				.ExpiresInSeconds  = aTerminal.Grant.ExpiresInSeconds,
				.TotalBoundSeconds = Limits.TotalBoundSeconds,
				.MaximumPolls      = Limits.MaximumPolls,
			});
			GrantedAt       = now();
			DeviceCode      = std::move(aTerminal.Grant.DeviceCode);
			UserCode        = std::move(aTerminal.Grant.UserCode);
			VerificationUri = std::move(aTerminal.Grant.VerificationUri);

			apply(core::device_authorization_awaiting_event{ .Context = aContext });
			arm();
		}

		void accept(
			const core::device_authorization_request_context& aContext,
			device_code_refused&&)
		{
			erase_flow_transients();
			apply(core::device_authorization_disabled_event{ .Context = aContext });
		}

		void accept(
			const core::device_authorization_request_context& aContext,
			device_authorization_transport_error&&            aTerminal)
		{
			erase_flow_transients();
			apply(core::device_authorization_failed_event{
				.Context = aContext,
				.Failure = core_failure(aTerminal.Failure),
			});
		}

		void accept(
			const core::device_authorization_request_context& aContext,
			device_authorization_transport_cancelled&&)
		{
			erase_flow_transients();
			apply(core::device_authorization_cancelled_event{ .Context = aContext });
		}

		// -- polling -------------------------------------------------------

		void arm()
		{
			if (Closing || !Flow.has_value() || !Schedule.has_value()) {
				return;
			}

			const auto context  = *Flow;
			const auto elapsed  = elapsed_seconds();
			const auto decision = Schedule->decide(elapsed);
			switch (decision.Action) {
			case device_authorization_schedule_action::poll_now:
				poll(elapsed);
				return;
			case device_authorization_schedule_action::wait:
				if (!schedule_timer(timer_purpose::poll, decision.WaitSeconds)) {
					// Without a timer the total bound cannot be enforced, and a
					// flow nobody will ever end is worse than one that fails.
					erase_flow_transients();
					apply(core::device_authorization_failed_event{
						.Context = context,
						.Failure = core::device_authorization_failure::timeout,
					});
				}
				return;
			case device_authorization_schedule_action::expired:
				erase_flow_transients();
				apply(core::device_authorization_expired_event{
					.Context = context,
				});
				return;
			case device_authorization_schedule_action::bound_exceeded:
				erase_flow_transients();
				apply(core::device_authorization_failed_event{
					.Context = context,
					.Failure = core::device_authorization_failure::timeout,
				});
				return;
			}
		}

		void poll(const std::uint32_t aElapsedSeconds)
		{
			const auto context = *Flow;
			if (!DeviceCode.has_value()) {
				erase_flow_transients();
				apply(core::device_authorization_failed_event{
					.Context = context,
					.Failure = core::device_authorization_failure::network,
				});
				return;
			}

			auto code = github_credential_material::create(DeviceCode->bytes());
			if (!code.has_value()) {
				erase_flow_transients();
				apply(core::device_authorization_failed_event{
					.Context = context,
					.Failure = core::device_authorization_failure::network,
				});
				return;
			}

			Schedule->record_poll(aElapsedSeconds);
			++Outstanding;
			Transport->exchange_device_token(
				device_token_exchange{
					.Context = context,
					.Code    = std::move(*code),
				},
				token_handler());
		}

		[[nodiscard]] auto token_handler() -> device_token_completion_handler
		{
			return [weak = weak_from_this()](device_token_completion aCompletion) {
				const auto self = weak.lock();
				if (self == nullptr) {
					return;
				}
				auto payload = std::make_shared<device_token_completion>(
					std::move(aCompletion));
				self->post([self, payload] {
					self->on_token(std::move(*payload));
				});
			};
		}

		void on_token(device_token_completion aCompletion)
		{
			release();
			const auto context = context_of(aCompletion);
			// A completion that arrives after cancellation, after shutdown, or
			// under a superseded generation owns whatever material it carries.
			// Returning here destroys it, which is what erases it.
			if (Closing || !Flow.has_value() || *Flow != context) {
				return;
			}
			if (
				State.Stage
				!= core::device_authorization_stage::awaiting_authorization) {
				return;
			}

			std::visit(
				[this, context](auto&& aTerminal) {
					resolve(context, std::forward<decltype(aTerminal)>(aTerminal));
				},
				std::move(aCompletion));
		}

		void resolve(
			const core::device_authorization_request_context& aContext,
			device_token_issued&&                             aTerminal)
		{
			authorize(aContext, std::move(aTerminal.Grant));
		}

		void resolve(
			const core::device_authorization_request_context&,
			device_token_deferred&& aTerminal)
		{
			ConsecutiveFailures = 0U;
			if (aTerminal.Reason == device_token_delay_reason::slow_down) {
				Schedule->slow_down(aTerminal.SlowDownIntervalSeconds);
			}
			arm();
		}

		void resolve(
			const core::device_authorization_request_context& aContext,
			device_token_refused&&                            aTerminal)
		{
			erase_flow_transients();
			if (aTerminal.Reason == device_token_refusal_reason::expired_token) {
				apply(core::device_authorization_expired_event{
					.Context = aContext,
				});
				return;
			}
			apply(core::device_authorization_denied_event{ .Context = aContext });
		}

		void resolve(
			const core::device_authorization_request_context& aContext,
			device_authorization_transport_error&&            aTerminal)
		{
			if (retryable(aTerminal.Failure)) {
				++ConsecutiveFailures;
				if (ConsecutiveFailures <= Limits.MaximumPollFailures) {
					arm();
					return;
				}
			}

			erase_flow_transients();
			apply(core::device_authorization_failed_event{
				.Context = aContext,
				.Failure = core_failure(aTerminal.Failure),
			});
		}

		void resolve(
			const core::device_authorization_request_context& aContext,
			device_authorization_transport_cancelled&&)
		{
			erase_flow_transients();
			apply(core::device_authorization_cancelled_event{ .Context = aContext });
		}

		// -- authorization -------------------------------------------------

		void authorize(
			const core::device_authorization_request_context& aContext,
			device_token_material&&                           aGrant)
		{
			cancel_timer();
			// The device code has done its work. It goes before anything else
			// happens, so no later branch can be the one that forgot.
			erase_flow_transients();

			AccessLifetimeSeconds = aGrant.ExpiresInSeconds;
			PendingAccess.emplace(std::move(aGrant.AccessToken));

			const auto rotates = aGrant.RefreshToken.has_value()
				&& valid(*aGrant.RefreshToken);
			if (!rotates) {
				enroll_and_finish(aContext);
				return;
			}

			PendingRefresh.emplace(std::move(*aGrant.RefreshToken));
			if (!RefreshAuthority.has_value()) {
				// There is no slot to store it under until an authenticated
				// `/user` observation binds this profile to an account. The
				// material waits in memory for that and for nothing else.
				enroll_and_finish(aContext);
				return;
			}

			// Rotation order: the renewable material becomes durable before the
			// access token is enrolled. A crash between the two leaves a vault
			// that can still renew, which is recoverable; the other order leaves
			// one that cannot.
			AuthorizingContext = aContext;
			submit_refresh_store();
		}

		void enroll_and_finish(
			const core::device_authorization_request_context& aContext)
		{
			auto access = std::move(*PendingAccess);
			PendingAccess.reset();
			Enrollment->enroll(aContext.Profile, std::move(access));
			AuthorizedAt = now();

			apply(core::device_authorization_authorized_event{
				.Context = aContext,
			});
			schedule_refresh();
		}

		// -- refresh -------------------------------------------------------

		[[nodiscard]] auto mint_vault_context() -> github_credential_operation_context
		{
			const auto request = core::request_id{ NextHostRequest };
			++NextHostRequest;
			return github_credential_operation_context{
				.Request            = request,
				.ApplicationSession = State.ApplicationSession,
				.Authority          = *RefreshAuthority,
				.IdentityGeneration = State.IdentityGeneration,
			};
		}

		[[nodiscard]] auto mint_refresh_context()
			-> core::device_authorization_request_context
		{
			const auto request = core::request_id{ NextHostRequest };
			++NextHostRequest;
			const auto generation = core::device_authorization_generation{
				NextRefreshGeneration,
			};
			++NextRefreshGeneration;
			return core::device_authorization_request_context{
				.Request            = request,
				.ApplicationSession = State.ApplicationSession,
				.Profile            = RefreshAuthority->Profile,
				.IdentityGeneration = State.IdentityGeneration,
				.Generation         = generation,
			};
		}

		void submit_refresh_store()
		{
			auto material = std::move(*PendingRefresh);
			PendingRefresh.reset();

			const auto context  = mint_vault_context();
			RefreshStoreContext = context;
			++Outstanding;
			Vault->store_refresh(
				github_credential_enrollment{
					.Context    = context,
					.Credential = std::move(material),
				},
				refresh_store_handler());
		}

		[[nodiscard]] auto refresh_store_handler()
			-> github_credential_store_completion_handler
		{
			return [weak = weak_from_this()](
					   github_credential_store_completion aCompletion) {
				const auto self = weak.lock();
				if (self == nullptr) {
					return;
				}
				auto payload = std::make_shared<
					github_credential_store_completion>(std::move(aCompletion));
				self->post([self, payload] {
					self->on_refresh_stored(std::move(*payload));
				});
			};
		}

		void on_refresh_stored(github_credential_store_completion aCompletion)
		{
			release();
			const auto context = vault_context_of(aCompletion);
			if (
				Closing || !RefreshStoreContext.has_value()
				|| *RefreshStoreContext != context) {
				return;
			}
			RefreshStoreContext.reset();

			if (!std::holds_alternative<github_credential_store_success>(
					aCompletion)) {
				fail_storage();
				return;
			}

			RefreshDurable = true;
			if (AuthorizingContext.has_value()) {
				const auto authorizing = *AuthorizingContext;
				AuthorizingContext.reset();
				enroll_and_finish(authorizing);
				return;
			}

			if (PendingAccess.has_value()) {
				auto access = std::move(*PendingAccess);
				PendingAccess.reset();
				Enrollment->enroll(RefreshAuthority->Profile, std::move(access));
				AuthorizedAt = now();
				++Refresh.Rotations;
			}
			schedule_refresh();
		}

		void fail_storage()
		{
			PendingAccess.reset();
			PendingRefresh.reset();
			if (AuthorizingContext.has_value()) {
				// The flow itself fails. Enrolling an access token whose renewal
				// chain was never written down would look like a success and
				// expire silently later.
				const auto authorizing = *AuthorizingContext;
				AuthorizingContext.reset();
				apply(core::device_authorization_failed_event{
					.Context = authorizing,
					.Failure = core::device_authorization_failure::vault,
				});
				return;
			}
			request_reenrollment(
				device_authorization_refresh_failure::storage_failed);
		}

		void schedule_refresh()
		{
			if (
				Closing || !RefreshDurable || !RefreshAuthority.has_value()
				|| AccessLifetimeSeconds == 0U) {
				return;
			}

			const auto lead = std::min(
				Limits.RefreshLeadSeconds,
				AccessLifetimeSeconds / 2U);
			const auto due     = AccessLifetimeSeconds > lead
				? AccessLifetimeSeconds - lead
				: 0U;
			const auto elapsed = elapsed_seconds_between(AuthorizedAt, now());
			const auto wait    = due > elapsed ? due - elapsed : 0U;

			if (!schedule_timer(timer_purpose::refresh, wait)) {
				request_reenrollment(
					device_authorization_refresh_failure::transport_failed);
				return;
			}
			Refresh.State   = device_authorization_refresh_state::scheduled;
			Refresh.Failure = device_authorization_refresh_failure::none;
		}

		void begin_refresh()
		{
			if (Closing || !RefreshAuthority.has_value()) {
				return;
			}

			Refresh.State   = device_authorization_refresh_state::refreshing;
			Refresh.Failure = device_authorization_refresh_failure::none;

			const auto context     = mint_vault_context();
			RefreshRetrieveContext = context;
			++Outstanding;
			Vault->retrieve_refresh(context, refresh_retrieve_handler());
		}

		[[nodiscard]] auto refresh_retrieve_handler()
			-> github_credential_retrieve_completion_handler
		{
			return [weak = weak_from_this()](
					   github_credential_retrieve_completion aCompletion) {
				const auto self = weak.lock();
				if (self == nullptr) {
					return;
				}
				auto payload = std::make_shared<
					github_credential_retrieve_completion>(
					std::move(aCompletion));
				self->post([self, payload] {
					self->on_refresh_material(std::move(*payload));
				});
			};
		}

		void on_refresh_material(github_credential_retrieve_completion aCompletion)
		{
			release();
			const auto context = vault_context_of(aCompletion);
			if (
				Closing || !RefreshRetrieveContext.has_value()
				|| *RefreshRetrieveContext != context) {
				return;
			}
			RefreshRetrieveContext.reset();

			if (std::holds_alternative<github_credential_retrieve_absent>(
					aCompletion)) {
				request_reenrollment(
					device_authorization_refresh_failure::refresh_absent);
				return;
			}
			if (std::holds_alternative<github_credential_retrieve_cancelled>(
					aCompletion)) {
				Refresh.State = device_authorization_refresh_state::idle;
				return;
			}
			auto* success = std::get_if<github_credential_retrieve_success>(
				&aCompletion);
			if (success == nullptr) {
				request_reenrollment(
					device_authorization_refresh_failure::vault_unavailable);
				return;
			}

			const auto refreshContext = mint_refresh_context();
			RefreshContext            = refreshContext;
			++Outstanding;
			Transport->refresh_access_token(
				device_token_exchange{
					.Context = refreshContext,
					.Code    = std::move(success->Credential),
				},
				refresh_token_handler());
		}

		[[nodiscard]] auto refresh_token_handler()
			-> device_token_completion_handler
		{
			return [weak = weak_from_this()](device_token_completion aCompletion) {
				const auto self = weak.lock();
				if (self == nullptr) {
					return;
				}
				auto payload = std::make_shared<device_token_completion>(
					std::move(aCompletion));
				self->post([self, payload] {
					self->on_refresh_token(std::move(*payload));
				});
			};
		}

		void on_refresh_token(device_token_completion aCompletion)
		{
			release();
			const auto context = context_of(aCompletion);
			// A renewal answer for a superseded attempt rotates nothing. The
			// material it carries is released here.
			if (
				Closing || !RefreshContext.has_value()
				|| *RefreshContext != context) {
				return;
			}
			RefreshContext.reset();

			if (std::holds_alternative<device_authorization_transport_cancelled>(
					aCompletion)) {
				Refresh.State = device_authorization_refresh_state::idle;
				return;
			}
			auto* issued = std::get_if<device_token_issued>(&aCompletion);
			if (issued == nullptr) {
				request_reenrollment(
					std::holds_alternative<device_token_refused>(aCompletion)
						? device_authorization_refresh_failure::refresh_rejected
						: device_authorization_refresh_failure::transport_failed);
				return;
			}

			AccessLifetimeSeconds = issued->Grant.ExpiresInSeconds;
			PendingAccess.emplace(std::move(issued->Grant.AccessToken));
			if (
				issued->Grant.RefreshToken.has_value()
				&& valid(*issued->Grant.RefreshToken)) {
				PendingRefresh.emplace(std::move(*issued->Grant.RefreshToken));
				submit_refresh_store();
				return;
			}

			// The server kept the existing renewable material alive, so there is
			// nothing new to write down and the order question does not arise.
			auto access = std::move(*PendingAccess);
			PendingAccess.reset();
			Enrollment->enroll(RefreshAuthority->Profile, std::move(access));
			AuthorizedAt = now();
			++Refresh.Rotations;
			schedule_refresh();
		}

		void request_reenrollment(
			const device_authorization_refresh_failure aFailure)
		{
			cancel_timer();
			PendingAccess.reset();
			PendingRefresh.reset();
			RefreshDurable = false;
			Refresh.State  = device_authorization_refresh_state::
				reenrollment_required;
			Refresh.Failure = aFailure;
		}

		// -- timer ---------------------------------------------------------

		[[nodiscard]] auto schedule_timer(
			const timer_purpose aPurpose,
			const std::uint32_t aSeconds) -> bool
		{
			Timer->cancel();
			TimerArmed = false;
			Purpose    = aPurpose;

			const auto weak = weak_from_this();
			if (!Timer->schedule_after(aSeconds, [weak] {
					const auto self = weak.lock();
					if (self == nullptr) {
						return;
					}
					self->post([self] {
						self->on_timer();
					});
				})) {
				Purpose = timer_purpose::none;
				return false;
			}

			TimerArmed = true;
			return true;
		}

		void cancel_timer() noexcept
		{
			Timer->cancel();
			TimerArmed = false;
			Purpose    = timer_purpose::none;
		}

		void on_timer()
		{
			TimerArmed         = false;
			const auto purpose = std::exchange(Purpose, timer_purpose::none);
			if (Closing) {
				return;
			}
			switch (purpose) {
			case timer_purpose::poll:
				arm();
				return;
			case timer_purpose::refresh:
				begin_refresh();
				return;
			case timer_purpose::none:
				return;
			}
		}

		// -- erasure and lifecycle -----------------------------------------

		void erase_flow_transients() noexcept
		{
			if (DeviceCode.has_value()) {
				DeviceCode->reset();
				DeviceCode.reset();
			}
			// The user code was on screen a moment ago. Overwriting before
			// clearing means the buffer does not keep it after the flow ends.
			std::ranges::fill(UserCode, '\0');
			UserCode.clear();
			std::ranges::fill(VerificationUri, '\0');
			VerificationUri.clear();
			Schedule.reset();
			Flow.reset();
			ConsecutiveFailures = 0U;
		}

		void shutdown()
		{
			if (Closing) {
				return;
			}
			Closing = true;
			cancel_timer();

			if (Flow.has_value()) {
				const auto context = *Flow;
				Transport->cancel(context);
				erase_flow_transients();
				// Shutdown confirms the same way cancellation does, so the core
				// reaches its cancelled terminal rather than being abandoned mid
				// flow.
				apply(core::device_authorization_cancelled_event{
					.Context = context,
				});
			}
			erase_flow_transients();

			if (RefreshContext.has_value()) {
				Transport->cancel(*RefreshContext);
				RefreshContext.reset();
			}
			if (RefreshRetrieveContext.has_value()) {
				Vault->cancel(*RefreshRetrieveContext);
				RefreshRetrieveContext.reset();
			}
			if (RefreshStoreContext.has_value()) {
				Vault->cancel(*RefreshStoreContext);
				RefreshStoreContext.reset();
			}

			PendingAccess.reset();
			PendingRefresh.reset();
			AuthorizingContext.reset();
		}

		core::device_authorization_state      State;
		device_authorization_transport_port*  Transport    = nullptr;
		github_credential_vault_port*         Vault        = nullptr;
		device_authorization_enrollment_port* Enrollment   = nullptr;
		device_authorization_clock_port*      Clock        = nullptr;
		device_authorization_timer_port*      Timer        = nullptr;
		decision_executor*                    DecisionLane = nullptr;
		device_authorization_projection_port* Projections  = nullptr;
		device_authorization_host_limits      Limits;
		device_authorization_browser_handoff  Handoff;

		std::optional<core::device_authorization_request_context> Flow;
		std::optional<device_authorization_schedule>              Schedule;
		std::chrono::milliseconds                                 GrantedAt{ 0 };
		std::optional<github_credential_material>                 DeviceCode;
		std::string                                               UserCode;
		std::string                                               VerificationUri;
		std::uint32_t                                             ConsecutiveFailures = 0U;

		std::optional<github_credential_authority_key>            RefreshAuthority;
		std::optional<github_credential_material>                 PendingAccess;
		std::optional<github_credential_material>                 PendingRefresh;
		std::optional<core::device_authorization_request_context> AuthorizingContext;
		std::optional<core::device_authorization_request_context> RefreshContext;
		std::optional<github_credential_operation_context>        RefreshRetrieveContext;
		std::optional<github_credential_operation_context>        RefreshStoreContext;
		std::chrono::milliseconds                                 AuthorizedAt{ 0 };
		std::uint32_t                                             AccessLifetimeSeconds = 0U;
		bool                                                      RefreshDurable        = false;
		device_authorization_refresh_observation                  Refresh;

		device_authorization_presentation Presentation;
		device_authorization_handoff      LastHandoff
			= device_authorization_handoff::not_attempted;
		std::uint64_t NextHostRequest          = 1U;
		std::uint64_t NextRefreshGeneration    = 1U;
		std::uint32_t Outstanding              = 0U;
		timer_purpose Purpose                  = timer_purpose::none;
		bool          TimerArmed               = false;
		bool          Closing                  = false;
		bool          ProjectionDeliveryFailed = false;
	};

	device_authorization_application_host::device_authorization_application_host(
		core::device_authorization_state       aInitialState,
		device_authorization_transport_port&   aTransport,
		github_credential_vault_port&          aVault,
		device_authorization_enrollment_port&  aEnrollment,
		device_authorization_clock_port&       aClock,
		device_authorization_timer_port&       aTimer,
		decision_executor&                     aDecisionLane,
		device_authorization_projection_port&  aProjections,
		const device_authorization_host_limits aLimits,
		device_authorization_browser_handoff   aHandoff)
		: Implementation{ std::make_shared<implementation>(
			  std::move(aInitialState),
			  aTransport,
			  aVault,
			  aEnrollment,
			  aClock,
			  aTimer,
			  aDecisionLane,
			  aProjections,
			  valid(aLimits) ? aLimits : device_authorization_host_limits{ },
			  std::move(aHandoff)) }
	{
	}

	device_authorization_application_host::
		~device_authorization_application_host()
	{
		Implementation->shutdown();
	}

	void device_authorization_application_host::dispatch(
		const core::device_authorization_event& aEvent)
	{
		if (Implementation->Closing) {
			return;
		}
		Implementation->apply(aEvent);
	}

	void device_authorization_application_host::bind_refresh_authority(
		github_credential_authority_key aAuthority)
	{
		if (Implementation->Closing || !valid(aAuthority)) {
			return;
		}
		Implementation->RefreshAuthority = std::move(aAuthority);
		if (
			Implementation->PendingRefresh.has_value()
			&& !Implementation->AuthorizingContext.has_value()) {
			Implementation->submit_refresh_store();
		}
	}

	void device_authorization_application_host::resume_refresh(
		github_credential_authority_key aAuthority)
	{
		if (Implementation->Closing || !valid(aAuthority)) {
			return;
		}
		Implementation->RefreshAuthority = std::move(aAuthority);
		Implementation->RefreshDurable   = true;
		Implementation->begin_refresh();
	}

	void device_authorization_application_host::open_verification_address()
	{
		if (Implementation->Closing) {
			return;
		}
		Implementation->open_verification_address();
	}

	void device_authorization_application_host::shutdown()
	{
		Implementation->shutdown();
	}

	auto device_authorization_application_host::state() const noexcept
		-> const core::device_authorization_state&
	{
		return Implementation->State;
	}

	auto device_authorization_application_host::presentation() const noexcept
		-> const device_authorization_presentation&
	{
		return Implementation->Presentation;
	}

	auto device_authorization_application_host::refresh_observation()
		const noexcept -> const device_authorization_refresh_observation&
	{
		return Implementation->Refresh;
	}

	auto device_authorization_application_host::holds_transient_material()
		const noexcept -> bool
	{
		return Implementation->DeviceCode.has_value()
			|| Implementation->PendingAccess.has_value();
	}

	auto device_authorization_application_host::holds_pending_refresh_material()
		const noexcept -> bool
	{
		return Implementation->PendingRefresh.has_value();
	}

	auto device_authorization_application_host::is_closing() const noexcept -> bool
	{
		return Implementation->Closing;
	}

	auto device_authorization_application_host::is_quiescent() const noexcept
		-> bool
	{
		return Implementation->Outstanding == 0U && !Implementation->TimerArmed;
	}

	auto device_authorization_application_host::projection_delivery_failed()
		const noexcept -> bool
	{
		return Implementation->ProjectionDeliveryFailed;
	}
}
