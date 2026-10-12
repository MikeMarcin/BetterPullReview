#include "bpr/application/device_authorization_application_host.hpp"

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

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

		// [[BPR-0644]]. What a renewal that could not finish says to the GitHub
		// host, in its own vocabulary. A failure to reach the server or the
		// credential store says nothing about the grant, so it is tried again;
		// a refused or missing refresh is a grant that needs signing in again.
		[[nodiscard]] auto session_failure(
			const device_authorization_refresh_failure aFailure) noexcept
			-> core::github_failure
		{
			switch (aFailure) {
			case device_authorization_refresh_failure::transport_failed:
				return core::github_failure::network;
			case device_authorization_refresh_failure::vault_unavailable:
			case device_authorization_refresh_failure::storage_failed:
				return core::github_failure::vault;
			case device_authorization_refresh_failure::none:
			case device_authorization_refresh_failure::refresh_absent:
			case device_authorization_refresh_failure::refresh_rejected:
				return core::github_failure::authentication;
			}
			return core::github_failure::authentication;
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
			[[nodiscard]] auto observe_wall_seconds() noexcept -> std::int64_t override
			{
				return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			}
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
		// [[BPR-0540]]. The one boundary that may say a credential came from
		// a device grant. Everything else that enrolls is a token the reviewer
		// supplied, and only this origin lets the host believe a refresh entry
		// under the same authority belongs to the credential beside it.
		Host->enroll_profile(aProfile,
							 std::move(aCredential),
							 github_credential_provenance::device_grant);
	}

	void github_host_device_authorization_enrollment::enroll_grant(core::host_profile_id aProfile, github_credential_material aCredential, device_binding_lifetime aLifetime, bool aRefreshSettled)
	{
		Host->enroll_profile(aProfile, std::move(aCredential), github_credential_provenance::device_grant, aLifetime, aRefreshSettled);
	}
	void github_host_device_authorization_enrollment::settle_binding(github_credential_authority_key aAuthority, core::github_identity_generation aGeneration, device_binding_lifetime aLifetime)
	{
		Host->settle_device_binding(std::move(aAuthority), aGeneration, aLifetime);
	}
	void github_host_device_authorization_enrollment::prepare_refresh(github_credential_authority_key aAuthority, core::github_identity_generation aGeneration, std::function<void(bool)> aCompletion)
	{
		Host->prepare_device_rotation(std::move(aAuthority), aGeneration, std::move(aCompletion));
	}
	void github_host_device_authorization_enrollment::invalidate_grant(github_credential_authority_key aAuthority, core::github_identity_generation aGeneration, const device_authorization_refresh_failure aFailure)
	{
		Host->invalidate_device_grant(std::move(aAuthority), aGeneration, session_failure(aFailure));
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
			github_credential_refresh_vault_port&  aVault,
			device_authorization_enrollment_port&  aEnrollment,
			device_authorization_clock_port&       aClock,
			device_authorization_timer_port&       aTimer,
			decision_executor&                     aDecisionLane,
			device_authorization_projection_port&  aProjections,
			const device_authorization_host_limits aLimits,
			device_authorization_browser_handoff   aHandoff,
			const device_authorization_capability  aCapability)
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
			, Capability{ aCapability }
		{
			// The command set presentation sees before any transition. It is
			// the core's answer for the state this host was constructed with,
			// asked for rather than restated, so the first publication carries
			// the same legality every later one does.
			Commands = core::device_authorization_commands(State);
		}

		// -- lane and clock ------------------------------------------------

		void post(std::function<void()> aTask)
		{
			// Every renewal state change happens inside one of these turns, and
			// there are a dozen places that make one. Publishing here rather
			// than at each of them is what makes "the observation reaches
			// presentation" a property of the lane instead of a list somebody
			// has to keep complete; `deliver_refresh` publishes only on a
			// change, so a turn that renewed nothing costs a comparison.
			DecisionLane->post(
				[weak = weak_from_this(), task = std::move(aTask)]() mutable {
					task();
					if (const auto self = weak.lock(); self != nullptr) {
						self->deliver_refresh();
					}
				});
		}

		// Republishes the renewal lifecycle when, and only when, it changed.
		// Renewal is observational in the same sense the presentation is: a
		// reader cannot veto a rotation, and an exception on the way out is
		// contained here rather than allowed to abandon material mid-flight.
		void deliver_refresh()
		{
			if (Closing || Refresh == PublishedRefresh) {
				return;
			}
			PublishedRefresh = Refresh;
			try {
				Projections->publish_refresh(PublishedRefresh);
			} catch (...) {
				ProjectionDeliveryFailed = true;
			}
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
			if (output.Status.has_value()) {
				release_flow_deadline();
			}
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
			// The core decides which commands are legal and says so in the
			// output it just produced. The host carries that decision to
			// presentation unchanged; nothing downstream re-derives it, and a
			// republication with no transition behind it reports the last
			// answer the core gave rather than a fresh guess.
			if (core::has_changed(
					aOutput.Changed,
					core::device_authorization_changed_domain::commands)) {
				Commands = aOutput.Commands;
			}
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
				.Commands                 = Commands,
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
			// [[BPR-0553]]. The slot the previous grant was keyed by is marked
			// here, not released. It belongs to whatever session preceded this
			// flow, and that session stays the live one until this flow
			// produces a grant of its own: released at the gesture that starts
			// a flow ([[BPR-0550]]) the armed renewal died the moment a
			// reviewer pressed *Get a code*, which the panel offers in every
			// terminal stage, and a flow they then abandoned, denied, or
			// failed left a working access token with nothing to renew it and
			// an observation reading `idle`, so nothing was said. The release
			// belongs at `authorize()`, where this flow's own material is
			// about to be written under whatever slot is bound.
			RefreshBindingPrecedesFlow = RefreshAuthority.has_value();
			ConsecutiveFailures        = 0U;

			Flow = aRequest.Context;
			if (!Capability.Registered
				|| aRequest.Context.Profile != Capability.Profile) {
				// No registered identity, or one registered for a different
				// profile than the one this begin names: either way no
				// request is built and the transport is never reached.
				//
				// The second condition is the same refusal for the same
				// reason. The capability is composed for exactly one profile
				// -- the wire is bound to that host and the client identity
				// is that host's -- so a begin arriving for another profile,
				// as one does the moment the mounted panel is bound to
				// whichever profile the picker names, would run this host's
				// flow under a foreign authority and then enroll the issued
				// token under the profile that did not issue it
				// ([[BPR-0535]]).
				//
				// The refusal is posted rather than applied inline because
				// this runs inside the transition that produced the request.
				post([self = shared_from_this(), context = aRequest.Context] {
					if (self->Closing || self->Flow != context) {
						return;
					}
					self->apply(core::device_authorization_unavailable_event{
						.Context = context,
					});
				});
				return;
			}

			++Outstanding;
			Transport->acquire_device_code(
				aRequest.Context,
				device_code_handler());
		}

		void perform(const core::cancel_device_authorization_request& aRequest)
		{
			const auto context = aRequest.Context;
			// [[BPR-0554]]. The flow's deadline, not the timer. A renewal
			// queued behind this poll belongs to the session the reviewer
			// still holds, and stopping a flow they started by mistake is not
			// a reason to stop renewing it.
			drop_deadline(timer_purpose::poll);
			Transport->cancel(context);
			erase_flow_transients();
			// The same forgetting `shutdown()` does, for the same reason. A
			// store left outstanding across a cancel still answers, and the
			// answer arrives into a flow that no longer exists: a success
			// would mark material durable and arm a renewal nobody asked
			// for, and a failure would tell the reviewer to authorize again
			// after they chose to stop. Cancelling the slot and dropping the
			// context makes the late answer land on the guard in
			// `on_refresh_stored` instead.
			if (RefreshStoreContext.has_value()) {
				Vault->cancel(*RefreshStoreContext);
				RefreshStoreContext.reset();
			}
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
			// The poll is over. Anything queued behind it takes the timer,
			// and the release below decides whether a renewal is still one of
			// those things.
			drop_deadline(timer_purpose::poll);
			// The device code has done its work. It goes before anything else
			// happens, so no later branch can be the one that forgot.
			erase_flow_transients();

			// [[BPR-0550]], [[BPR-0553]]. This is the point past which a
			// binding older than this flow must not be used: every branch below
			// either writes this grant's renewable material down or enrolls its
			// access token, and both are wrong under the previous session's
			// slot. The write files a renewal chain under an account that never
			// authenticated it, and a renewal left armed against the old
			// material would enroll the old account's token over the one just
			// granted. Released rather than replaced: the material then parks
			// in memory until this grant's own authenticated `/user` binds it,
			// which is the first-grant path both composed courses drive.
			//
			// Only a binding this flow inherited. One made while the flow ran
			// is this grant's own, and it is spent below rather than dropped.
			if (RefreshBindingPrecedesFlow) {
				release_refresh_binding();
			}

			AccessLifetimeSeconds = aGrant.ExpiresInSeconds;
			RenewalWindowSeconds  = AccessLifetimeSeconds;
			observe_lifetime(aGrant);
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
			Enrollment->enroll_grant(aContext.Profile, std::move(access), Lifetime, RefreshDurable);

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
			// [[BPR-0546]]. The generation is the one the GitHub host bound
			// this authority under, not `State.IdentityGeneration`. The state's
			// copy is whatever composition seeded the device flow with -- zero
			// in the composed product, which is exactly what every platform
			// vault's `valid()` refuses, so every store and retrieve from this
			// host came back `invalid_request` and the lifecycle was inert.
			return github_credential_operation_context{
				.Request            = request,
				.ApplicationSession = State.ApplicationSession,
				.Authority          = *RefreshAuthority,
				.IdentityGeneration = RefreshIdentityGeneration,
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
			if (!PendingAccess && RefreshAuthority) {
				Enrollment->settle_binding(*RefreshAuthority, RefreshIdentityGeneration, Lifetime);
			}
			if (AuthorizingContext.has_value()) {
				const auto authorizing = *AuthorizingContext;
				AuthorizingContext.reset();
				enroll_and_finish(authorizing);
				return;
			}

			if (PendingAccess.has_value()) {
				auto access = std::move(*PendingAccess);
				PendingAccess.reset();
				Enrollment->enroll_grant(RefreshAuthority->Profile, std::move(access), Lifetime, true);
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

		// [[BPR-0553]], [[BPR-0554]]. A flow that has reached a terminal owes no
		// more polls, so its deadline goes and a renewal waiting behind it
		// takes the timer for whatever it has left.
		//
		// BPR-0553 re-armed the renewal here instead, from `AuthorizedAt`, and
		// that was the whole of the repair: it came back late by however long
		// the flow had run, and read `scheduled` with nothing behind it in
		// between. It no longer needs re-arming, because it was never
		// displaced -- only queued behind an earlier deadline, and only while
		// the poll's was in fact the earlier one.
		void release_flow_deadline()
		{
			if (!core::terminal(State.Stage)) {
				return;
			}
			drop_deadline(timer_purpose::poll);
		}

		void observe_lifetime(const device_token_material& aGrant)
		{
			const auto observed         = Clock->observe_wall_seconds();
			const auto issued           = observed > 0 && observed <= std::numeric_limits<std::int64_t>::max() - std::numeric_limits<std::uint32_t>::max() ? observed : 0;
			const auto oldRefreshExpiry = Lifetime.RefreshExpiresAt;
			Lifetime                    = { .IssuedAt = issued, .AccessExpiresAt = issued + aGrant.ExpiresInSeconds, .RefreshExpiresAt = aGrant.RefreshToken ? issued + aGrant.RefreshTokenExpiresInSeconds : oldRefreshExpiry };
			AuthorizedAt                = now();
			RefreshFailures             = 0U;
		}

		void begin_refresh()
		{
			if (Closing || !RefreshAuthority) {
				return;
			}
			if (elapsed_seconds_between(AuthorizedAt, now()) >= RenewalWindowSeconds) {
				request_reenrollment(device_authorization_refresh_failure::refresh_rejected);
				return;
			}
			const auto authority  = *RefreshAuthority;
			const auto generation = RefreshIdentityGeneration;
			++Outstanding;
			Enrollment->prepare_refresh(authority, generation, [self = shared_from_this(), authority, generation](const bool aReady) {
				self->post([self, authority, generation, aReady] {
					self->release();
					if (self->Closing || self->RefreshAuthority != authority || self->RefreshIdentityGeneration != generation) {
						return;
					}
					if (!aReady) {
						self->request_reenrollment(device_authorization_refresh_failure::storage_failed);
						return;
					}
					self->retrieve_refresh();
				});
			});
		}

		void retrieve_refresh()
		{
			if (Closing || !RefreshAuthority.has_value()) {
				return;
			}

			if (elapsed_seconds_between(AuthorizedAt, now()) >= RenewalWindowSeconds) {
				request_reenrollment(device_authorization_refresh_failure::refresh_rejected);
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

			if (elapsed_seconds_between(AuthorizedAt, now()) >= RenewalWindowSeconds) {
				request_reenrollment(device_authorization_refresh_failure::refresh_rejected);
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
			if (const auto* error = std::get_if<device_authorization_transport_error>(&aCompletion); error != nullptr && retryable(error->Failure)) {
				const auto elapsed = elapsed_seconds_between(AuthorizedAt, now());
				if (++RefreshFailures <= Limits.MaximumPollFailures && elapsed < RenewalWindowSeconds) {
					const auto wait = std::min(std::uint32_t{ 5U }, RenewalWindowSeconds - elapsed);
					if (schedule_timer(timer_purpose::refresh, wait)) {
						Refresh.State   = device_authorization_refresh_state::scheduled;
						Refresh.Failure = device_authorization_refresh_failure::transport_failed;
						return;
					}
				}
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
			RenewalWindowSeconds  = AccessLifetimeSeconds;
			observe_lifetime(issued->Grant);
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
			Enrollment->enroll_grant(RefreshAuthority->Profile, std::move(access), Lifetime, true);
			++Refresh.Rotations;
			schedule_refresh();
		}

		// [[BPR-0554]]. The renewal's deadline only. A flow polling alongside a
		// renewal chain that just failed keeps its own: what broke is the
		// chain, not the flow, and taking the flow's timer would leave it with
		// nothing to end it.
		void request_reenrollment(
			const device_authorization_refresh_failure aFailure)
		{
			const auto authority  = RefreshAuthority;
			const auto generation = RefreshIdentityGeneration;
			report_reenrollment(aFailure);
			drop_deadline(timer_purpose::refresh);
			if (authority) {
				Enrollment->invalidate_grant(*authority, generation, aFailure);
			}
		}

		// The state half, with no timer work at all, so the arming path can
		// reach it without arming again from inside itself.
		void report_reenrollment(
			const device_authorization_refresh_failure aFailure)
		{
			PendingAccess.reset();
			PendingRefresh.reset();
			RefreshDurable = false;
			Refresh.State  = device_authorization_refresh_state::
				reenrollment_required;
			Refresh.Failure = aFailure;
		}

		// -- timer ---------------------------------------------------------

		// [[BPR-0554]]. The host holds both deadlines and the port holds the
		// earlier one. The port's contract is unchanged -- one pending expiry,
		// scheduling replaces it -- because a second port would need a second
		// timer composed, adapted, and shut down on every platform to answer a
		// question the host can answer itself, and re-arming the renewal
		// immediately would simply take the timer back from the poll and hand
		// the same contention to the flow instead.
		//
		// A deadline is an absolute monotonic instant rather than a remaining
		// count, so a deadline that waits behind another is re-armed for the
		// time it has left rather than for the time it was first given.
		[[nodiscard]] auto deadline_of(const timer_purpose aPurpose) noexcept
			-> std::optional<std::chrono::milliseconds>&
		{
			// Only the two purposes that name a deadline reach here; `none`
			// is filtered by every caller that can hold it.
			return aPurpose == timer_purpose::poll ? PollDueAt : RefreshDueAt;
		}

		[[nodiscard]] auto seconds_until(
			const std::chrono::milliseconds aDeadline) noexcept -> std::uint32_t
		{
			const auto current = now();
			if (aDeadline <= current) {
				return 0U;
			}
			// Rounded up. Truncation would arm a poll fractionally inside the
			// server's interval, which is the one direction the interval is
			// there to prevent.
			return static_cast<std::uint32_t>(
				std::chrono::ceil<std::chrono::seconds>(aDeadline - current)
					.count());
		}

		[[nodiscard]] auto earliest_deadline() noexcept -> timer_purpose
		{
			if (!PollDueAt.has_value()) {
				return RefreshDueAt.has_value() ? timer_purpose::refresh
												: timer_purpose::none;
			}
			if (!RefreshDueAt.has_value()) {
				return timer_purpose::poll;
			}
			// A tie goes to the renewal. The poll has an interval to spare and
			// the renewal has a token expiring behind it.
			return *RefreshDueAt <= *PollDueAt ? timer_purpose::refresh
											   : timer_purpose::poll;
		}

		// False means the port refused. The deadlines are left as they were, so
		// the caller that owns the one that could not be armed can answer for
		// it rather than leaving an observation nothing stands behind.
		[[nodiscard]] auto arm_earliest_deadline() -> bool
		{
			const auto purpose = earliest_deadline();
			if (purpose == timer_purpose::none) {
				Timer->cancel();
				TimerArmed = false;
				Purpose    = timer_purpose::none;
				return true;
			}

			const auto due = *deadline_of(purpose);
			if (TimerArmed && Purpose == purpose && ArmedDueAt == due) {
				// Already holding exactly this instant. Re-arming would only
				// re-round the wait and churn the port.
				return true;
			}

			Timer->cancel();
			TimerArmed = false;
			Purpose    = purpose;
			ArmedDueAt = due;

			const auto weak = weak_from_this();
			if (!Timer->schedule_after(seconds_until(due), [weak] {
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

		// A port that refuses is saying the expiry will never arrive, so the
		// deadline it refused is dropped and whoever was owed it hears about
		// it -- an observation with nothing behind it is precisely what this
		// card removes. `aReported` names the one purpose whose refusal the
		// caller answers for itself, so no refusal is answered twice. The loop
		// runs at most twice: each turn drops a deadline.
		void arm_or_answer(const timer_purpose aReported = timer_purpose::none)
		{
			while (!arm_earliest_deadline()) {
				const auto refused = earliest_deadline();
				deadline_of(refused).reset();
				if (refused == aReported) {
					continue;
				}
				if (refused == timer_purpose::refresh) {
					report_reenrollment(
						device_authorization_refresh_failure::transport_failed);
					continue;
				}
				// Without a timer the total bound cannot be enforced, and a
				// flow nobody will ever end is worse than one that fails.
				fail_unbounded_flow();
			}
		}

		[[nodiscard]] auto schedule_timer(
			const timer_purpose aPurpose,
			const std::uint32_t aSeconds) -> bool
		{
			deadline_of(aPurpose) = now() + std::chrono::seconds{ aSeconds };
			arm_or_answer(aPurpose);
			// The caller's deadline is still there unless the port refused it,
			// which is the answer both call sites already have one for.
			return deadline_of(aPurpose).has_value();
		}

		// A deadline that is no longer owed. Whatever is left takes the timer,
		// which is the whole of what this card fixes: a flow that ends, is
		// cancelled, or authorizes hands the timer straight back to a renewal
		// that was waiting behind it.
		void drop_deadline(const timer_purpose aPurpose)
		{
			deadline_of(aPurpose).reset();
			arm_or_answer();
		}

		void fail_unbounded_flow()
		{
			if (!Flow.has_value()) {
				return;
			}
			const auto context = *Flow;
			erase_flow_transients();
			apply(core::device_authorization_failed_event{
				.Context = context,
				.Failure = core::device_authorization_failure::timeout,
			});
		}

		void cancel_timer() noexcept
		{
			PollDueAt.reset();
			RefreshDueAt.reset();
			Timer->cancel();
			TimerArmed = false;
			Purpose    = timer_purpose::none;
		}

		void on_timer()
		{
			TimerArmed         = false;
			const auto purpose = std::exchange(Purpose, timer_purpose::none);
			if (purpose != timer_purpose::none) {
				// Spent. Only the deadline that just arrived is cleared; the
				// other one is still owed.
				deadline_of(purpose).reset();
			}
			if (Closing) {
				return;
			}
			switch (purpose) {
			case timer_purpose::poll:
				arm();
				break;
			case timer_purpose::refresh:
				begin_refresh();
				break;
			case timer_purpose::none:
				break;
			}
			// The deadline that waited behind this one takes the timer now.
			// This is where a renewal armed behind a poll gets it back, and
			// where a poll goes back on behind a renewal that just fired.
			arm_or_answer();
		}

		// -- erasure and lifecycle -----------------------------------------

		// [[BPR-0550]]. A refresh binding belongs to the session that named it
		// and outlives no flow. Released rather than replaced: the next grant's
		// material then parks in memory until its own authenticated `/user`
		// binds it, which is the first-grant path both composed courses drive,
		// instead of being stored under the previous account's key, called
		// durable, and renewed against a slot the incoming session erases.
		//
		// A renewal in flight goes with the binding. Every step of that
		// lifecycle mints its context from the authority, so an answer arriving
		// after the release would either name a slot this host no longer stands
		// behind or find no authority to name one with at all.
		void release_refresh_binding()
		{
			// [[BPR-0554]]. The renewal's deadline goes with the binding it
			// was armed under; a poll waiting behind it does not.
			drop_deadline(timer_purpose::refresh);
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
			RefreshAuthority.reset();
			RefreshIdentityGeneration = core::github_identity_generation{ };
			RefreshDurable            = false;
			// No binding is left, so none of it precedes anything.
			RefreshBindingPrecedesFlow = false;
			Refresh.State              = device_authorization_refresh_state::idle;
			Refresh.Failure            = device_authorization_refresh_failure::none;
		}

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
		github_credential_refresh_vault_port* Vault        = nullptr;
		device_authorization_enrollment_port* Enrollment   = nullptr;
		device_authorization_clock_port*      Clock        = nullptr;
		device_authorization_timer_port*      Timer        = nullptr;
		decision_executor*                    DecisionLane = nullptr;
		device_authorization_projection_port* Projections  = nullptr;
		device_authorization_host_limits      Limits;
		device_authorization_browser_handoff  Handoff;
		device_authorization_capability       Capability;

		std::optional<core::device_authorization_request_context> Flow;
		std::optional<device_authorization_schedule>              Schedule;
		std::chrono::milliseconds                                 GrantedAt{ 0 };
		std::optional<github_credential_material>                 DeviceCode;
		std::string                                               UserCode;
		std::string                                               VerificationUri;
		std::uint32_t                                             ConsecutiveFailures = 0U;

		std::optional<github_credential_authority_key>            RefreshAuthority;
		core::github_identity_generation                          RefreshIdentityGeneration;
		std::optional<github_credential_material>                 PendingAccess;
		std::optional<github_credential_material>                 PendingRefresh;
		std::optional<core::device_authorization_request_context> AuthorizingContext;
		std::optional<core::device_authorization_request_context> RefreshContext;
		std::optional<github_credential_operation_context>        RefreshRetrieveContext;
		std::optional<github_credential_operation_context>        RefreshStoreContext;
		std::chrono::milliseconds                                 AuthorizedAt{ 0 };
		std::uint32_t                                             AccessLifetimeSeconds = 0U;
		// How long after `AuthorizedAt` renewal may still spend refresh
		// material. The access lifetime for a live grant; the remaining refresh
		// lifetime for a grant restored after its access expired ([[BPR-0644]]).
		std::uint32_t           RenewalWindowSeconds = 0U;
		bool                    RefreshDurable       = false;
		device_binding_lifetime Lifetime;
		std::uint32_t           RefreshFailures = 0U;
		// [[BPR-0553]]. Whether the binding above was in place before the
		// current flow began, which is what makes it the previous session's
		// rather than this grant's. Recorded at the begin and answered at the
		// authorization, so a flow that never produces a grant answers nothing.
		bool                                     RefreshBindingPrecedesFlow = false;
		device_authorization_refresh_observation Refresh;
		// What presentation was last told. The host publishes renewal on
		// change, and the change is measured against what left the host, not
		// against what the last turn happened to set.
		device_authorization_refresh_observation PublishedRefresh;

		device_authorization_presentation Presentation;
		// The last command legality the core published. Held rather than
		// recomputed so a republication cannot answer differently from the
		// transition that produced it.
		std::vector<core::device_authorization_command_projection> Commands;
		device_authorization_handoff                               LastHandoff
			= device_authorization_handoff::not_attempted;
		std::uint64_t NextHostRequest          = 1U;
		std::uint64_t NextRefreshGeneration    = 1U;
		std::uint32_t Outstanding              = 0U;
		timer_purpose Purpose                  = timer_purpose::none;
		bool          TimerArmed               = false;
		bool          Closing                  = false;
		bool          ProjectionDeliveryFailed = false;

		// [[BPR-0554]]. Both deadlines the host owes, as absolute monotonic
		// instants. The port holds the earlier one; `Purpose` says which, and
		// `ArmedDueAt` the instant it was armed for, so a deadline that moves
		// while it is armed is re-armed rather than left on the old expiry.
		std::optional<std::chrono::milliseconds> PollDueAt;
		std::optional<std::chrono::milliseconds> RefreshDueAt;
		std::chrono::milliseconds                ArmedDueAt{ 0 };
	};

	device_authorization_application_host::device_authorization_application_host(
		core::device_authorization_state       aInitialState,
		device_authorization_transport_port&   aTransport,
		github_credential_refresh_vault_port&  aVault,
		device_authorization_enrollment_port&  aEnrollment,
		device_authorization_clock_port&       aClock,
		device_authorization_timer_port&       aTimer,
		decision_executor&                     aDecisionLane,
		device_authorization_projection_port&  aProjections,
		const device_authorization_host_limits aLimits,
		device_authorization_browser_handoff   aHandoff,
		const device_authorization_capability  aCapability)
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
			  std::move(aHandoff),
			  aCapability) }
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
		github_credential_authority_key        aAuthority,
		const core::github_identity_generation aIdentityGeneration)
	{
		// [[BPR-0546]]. A generation of zero names no identity and every
		// platform vault refuses a context carrying one, so binding under it
		// would arm a lifecycle whose every operation is refused. Refusing the
		// binding leaves the material in memory, which the next terminal
		// erases, rather than parking it against a slot it can never reach.
		if (
			Implementation->Closing || !valid(aAuthority)
			|| aIdentityGeneration.value() == 0U) {
			return;
		}
		// [[BPR-0550]]. A binding names the slot this host's material is kept
		// in. One arriving under a different authority -- the session changed
		// account, or composition rebound this host to a token the reviewer
		// pasted -- says nothing about what that slot holds, so the durability
		// and the renewal armed for the old one are released rather than
		// carried across. Carried across, the next renewal reads the new key
		// for material stored under the old, finds nothing, and asks a
		// reviewer holding a working credential to enroll again.
		if (
			Implementation->RefreshAuthority.has_value()
			&& !(*Implementation->RefreshAuthority == aAuthority)) {
			Implementation->release_refresh_binding();
		}
		Implementation->RefreshAuthority          = std::move(aAuthority);
		Implementation->RefreshIdentityGeneration = aIdentityGeneration;
		// A same-chain generation rebind may be the older grant's renewal while
		// another device flow is pending. It does not authenticate that flow.
		// A different authority already cleared this marker above.
		if (
			Implementation->PendingRefresh.has_value()
			&& !Implementation->AuthorizingContext.has_value()) {
			Implementation->submit_refresh_store();
		}
		if (Implementation->RefreshDurable) {
			Implementation->Enrollment->settle_binding(*Implementation->RefreshAuthority, aIdentityGeneration, Implementation->Lifetime);
		}
		Implementation->deliver_refresh();
	}

	void device_authorization_application_host::resume_refresh(
		github_credential_authority_key              aAuthority,
		const core::github_identity_generation       aIdentityGeneration,
		const std::optional<device_binding_lifetime> aLifetime)
	{
		if (
			Implementation->Closing || !valid(aAuthority)
			|| aIdentityGeneration.value() == 0U) {
			return;
		}
		Implementation->RefreshAuthority          = std::move(aAuthority);
		Implementation->RefreshIdentityGeneration = aIdentityGeneration;
		Implementation->RefreshDurable            = true;
		// A restored grant belongs to the earlier session, even when its
		// startup validation completes after another device flow has begun.
		Implementation->RefreshBindingPrecedesFlow = Implementation->Flow.has_value() && !core::terminal(Implementation->State.Stage);
		const auto remaining                       = aLifetime ? remaining_access_lifetime(*aLifetime, Implementation->Clock->observe_wall_seconds()) : 0U;
		if (remaining == 0U) {
			Implementation->request_reenrollment(device_authorization_refresh_failure::refresh_rejected);
		} else {
			Implementation->Lifetime              = *aLifetime;
			Implementation->AccessLifetimeSeconds = remaining;
			Implementation->RenewalWindowSeconds  = remaining;
			Implementation->AuthorizedAt          = Implementation->now();
			Implementation->schedule_refresh();
		}
		Implementation->deliver_refresh();
	}

	void device_authorization_application_host::renew_restored_grant(
		github_credential_authority_key        aAuthority,
		const core::github_identity_generation aIdentityGeneration,
		const device_binding_lifetime          aLifetime)
	{
		auto& host = *Implementation;
		if (host.Closing) {
			return;
		}
		if (!valid(aAuthority) || aIdentityGeneration.value() == 0U) {
			host.Enrollment->invalidate_grant(std::move(aAuthority), aIdentityGeneration, device_authorization_refresh_failure::none);
			return;
		}
		if (host.RefreshAuthority.has_value() && !(*host.RefreshAuthority == aAuthority)) {
			host.release_refresh_binding();
		}
		host.RefreshAuthority           = std::move(aAuthority);
		host.RefreshIdentityGeneration  = aIdentityGeneration;
		host.RefreshDurable             = true;
		host.RefreshBindingPrecedesFlow = host.Flow.has_value() && !core::terminal(host.State.Stage);
		host.Lifetime                   = aLifetime;
		// Nothing is scheduled from an access lifetime that has already gone.
		host.AccessLifetimeSeconds = 0U;
		host.RenewalWindowSeconds  = remaining_refresh_lifetime(aLifetime, host.Clock->observe_wall_seconds());
		host.AuthorizedAt          = host.now();
		host.RefreshFailures       = 0U;
		if (host.RenewalWindowSeconds == 0U) {
			host.request_reenrollment(device_authorization_refresh_failure::refresh_rejected);
			host.deliver_refresh();
			return;
		}
		// Begun on its own turn: the GitHub host asks from inside the
		// transition that holds the restored session.
		const auto authority = *host.RefreshAuthority;
		host.post([self = host.shared_from_this(), authority, aIdentityGeneration] {
			if (self->Closing || self->RefreshAuthority != authority || self->RefreshIdentityGeneration != aIdentityGeneration) {
				return;
			}
			self->begin_refresh();
		});
	}

	void device_authorization_application_host::release_refresh_authority()
	{
		Implementation->release_refresh_binding();
		Implementation->PendingAccess.reset();
		Implementation->PendingRefresh.reset();
		Implementation->deliver_refresh();
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

	auto device_authorization_application_host::replace_profile(const core::host_profile_id aExpected, const core::host_profile_id aReplacement) noexcept -> bool
	{
		auto& host = *Implementation;
		if (host.Closing || !host.Capability.Registered || host.Capability.Profile != aExpected || aReplacement.value() == 0U
			|| host.State.Stage == core::device_authorization_stage::requested || host.State.Stage == core::device_authorization_stage::awaiting_authorization || host.State.Stage == core::device_authorization_stage::cancelling) {
			return false;
		}
		host.Capability.Profile = aReplacement;
		return true;
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
