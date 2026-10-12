#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DEVICE_AUTHORIZATION_APPLICATION_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DEVICE_AUTHORIZATION_APPLICATION_HOST_HPP

#include "bpr/application/device_authorization_projection.hpp"
#include "bpr/application/device_authorization_schedule.hpp"
#include "bpr/application/github_application_host.hpp"
#include "bpr/application/github_credential_vault.hpp"
#include "bpr/application/port.hpp"
#include "bpr/core/device_authorization_transition.hpp"

#include <chrono>
#include <compare>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace bpr::application {
	// Every failure the transport can report about a device-authorization
	// exchange. Each maps onto exactly one core failure, so the host translates
	// rather than interprets, and no response byte can travel with any of them.
	enum class device_authorization_transport_failure : std::uint8_t {
		network,
		certificate,
		redirect,
		timeout,
		malformed_response,
		response_too_large,
	};

	enum class device_token_delay_reason : std::uint8_t {
		authorization_pending,
		slow_down,
	};

	enum class device_token_refusal_reason : std::uint8_t {
		access_denied,
		expired_token,
	};

	/// \brief Describes the device code acquisition record.
	///
	/// \details The device code is `github_credential_material` rather than text
	/// because it is bearer material for the length of the poll window: it is
	/// move-only, it cannot be copied by accident, and it overwrites itself when
	/// it goes away. The user code and the address are the two fields that exist
	/// to be read aloud, and they are the two that reach presentation.
	struct device_code_acquisition
	{
		github_credential_material DeviceCode;
		std::string                UserCode;
		std::string                VerificationUri;
		std::uint32_t              ExpiresInSeconds = 0U;
		std::uint32_t              IntervalSeconds  = 0U;
	};

	/// \brief Describes the device token material record.
	struct device_token_material
	{
		github_credential_material AccessToken;
		// Empty when the server issues no refresh material. An OAuth App answers
		// that way and the host then schedules no rotation at all.
		std::optional<github_credential_material> RefreshToken;
		std::uint32_t                             ExpiresInSeconds             = 0U;
		std::uint32_t                             RefreshTokenExpiresInSeconds = 0U;
	};

	/// \brief Describes the device code acquired record.
	struct device_code_acquired
	{
		core::device_authorization_request_context Context;
		device_code_acquisition                    Grant;
	};

	/// \brief Describes the device code refused record.
	struct device_code_refused
	{
		core::device_authorization_request_context Context;
	};

	/// \brief Describes the device authorization transport error record.
	struct device_authorization_transport_error
	{
		core::device_authorization_request_context Context;
		device_authorization_transport_failure     Failure
			= device_authorization_transport_failure::network;
	};

	/// \brief Describes the device authorization transport cancelled record.
	struct device_authorization_transport_cancelled
	{
		core::device_authorization_request_context Context;
	};

	using device_code_completion = std::variant<
		device_code_acquired,
		device_code_refused,
		device_authorization_transport_error,
		device_authorization_transport_cancelled>;

	/// \brief Describes the device token issued record.
	struct device_token_issued
	{
		core::device_authorization_request_context Context;
		device_token_material                      Grant;
	};

	/// \brief Describes the device token deferred record.
	struct device_token_deferred
	{
		core::device_authorization_request_context Context;
		device_token_delay_reason                  Reason
			= device_token_delay_reason::authorization_pending;
		std::uint32_t SlowDownIntervalSeconds = 0U;
	};

	/// \brief Describes the device token refused record.
	struct device_token_refused
	{
		core::device_authorization_request_context Context;
		device_token_refusal_reason                Reason
			= device_token_refusal_reason::access_denied;
	};

	using device_token_completion = std::variant<
		device_token_issued,
		device_token_deferred,
		device_token_refused,
		device_authorization_transport_error,
		device_authorization_transport_cancelled>;

	using device_code_completion_handler
		= std::function<void(device_code_completion)>;
	using device_token_completion_handler
		= std::function<void(device_token_completion)>;

	/// \brief Describes the device token exchange record.
	struct device_token_exchange
	{
		core::device_authorization_request_context Context;
		github_credential_material                 Code;
	};

	// Submissions return without waiting for a worker. A submission that returns
	// normally invokes its completion exactly once, from any thread; the host
	// marshals that terminal onto its decision lane itself. Cancellation is
	// advisory and does not suppress the terminal. Every submission takes owned
	// material rather than a view of it, so nothing the transport holds outlives
	// the buffer it was handed.
	/// \brief Provides the device authorization transport port contract.
	class device_authorization_transport_port
	{
	public:
		device_authorization_transport_port()          = default;
		virtual ~device_authorization_transport_port() = default;

		device_authorization_transport_port(
			const device_authorization_transport_port&) = delete;
		auto operator=(const device_authorization_transport_port&)
			-> device_authorization_transport_port& = delete;
		device_authorization_transport_port(
			device_authorization_transport_port&&) = delete;
		auto operator=(device_authorization_transport_port&&)
			-> device_authorization_transport_port& = delete;

		virtual void acquire_device_code(
			core::device_authorization_request_context aContext,
			device_code_completion_handler             aCompletion) = 0;
		virtual void exchange_device_token(
			device_token_exchange           aExchange,
			device_token_completion_handler aCompletion) = 0;
		// Renewal reuses the token endpoint with stored refresh material. The
		// answer shape is the token answer, because the server's is.
		virtual void refresh_access_token(
			device_token_exchange           aExchange,
			device_token_completion_handler aCompletion) = 0;
		virtual void cancel(
			core::device_authorization_request_context aContext) = 0;
	};

	/// \brief Provides the device authorization clock port contract.
	///
	/// \details Active deadlines use monotonic time so wall-clock changes cannot
	/// alter polling or renewal intervals. UTC is observed separately to record
	/// bounded grant lifetimes that can be validated after a restart.
	class device_authorization_clock_port
	{
	public:
		virtual ~device_authorization_clock_port() = default;

		[[nodiscard]] virtual auto observe_monotonic() noexcept
			-> std::chrono::milliseconds = 0;
		/// \brief UTC epoch seconds for durable lifetime facts; zero means unavailable.
		[[nodiscard]] virtual auto observe_wall_seconds() noexcept -> std::int64_t { return 0; }
	};

	/// \brief Creates the system device authorization clock.
	[[nodiscard]] auto make_system_device_authorization_clock()
		-> std::unique_ptr<device_authorization_clock_port>;

	/// \brief Provides the device authorization timer port contract.
	///
	/// \details One pending expiry at a time. Scheduling again replaces the
	/// pending expiry, and `cancel` withdraws it. The expiry may run on any
	/// thread; the host wraps it so the work lands on its decision lane.
	///
	/// [[BPR-0554]]. A poll and a renewal do overlap -- a reviewer with a live
	/// session can start a second flow, and a device code outlives the renewal
	/// lead -- but that is the host's to resolve, not this port's. The host
	/// holds both deadlines and gives this port the earlier one, so a second
	/// timer would be a second thing for every platform to compose, adapt, and
	/// shut down in order to answer a question one deadline comparison already
	/// answers.
	class device_authorization_timer_port
	{
	public:
		virtual ~device_authorization_timer_port() = default;

		// False means nothing was scheduled and nothing was retained. The host
		// treats that as a failed flow rather than waiting for an expiry that
		// will not arrive.
		[[nodiscard]] virtual auto schedule_after(
			std::uint32_t         aSeconds,
			std::function<void()> aExpiry) -> bool = 0;
		virtual void cancel() noexcept             = 0;
	};

	/// \brief Provides the device authorization browser port contract.
	///
	/// \details The narrowest capability the flow needs: hand one already
	/// validated address to whatever the operating system considers the
	/// browser, and say whether the launch was accepted. It receives no
	/// credential, no device code, no client identifier, and no endpoint, and
	/// it is never asked to open an address the host has not just checked
	/// against the compiled profile authority.
	///
	/// Returning `false` is an ordinary answer, not an error: the flow stays
	/// live and the reviewer finishes it wherever they like. An implementation
	/// must not start a listener, a broker, or an in-process browser, and must
	/// not block the decision lane waiting for the launched program.
	class device_authorization_browser_port
	{
	public:
		virtual ~device_authorization_browser_port() = default;

		[[nodiscard]] virtual auto open(std::string_view aAddress) -> bool = 0;
	};

	/// \brief Describes the device authorization browser handoff record.
	///
	/// \details A null `Browser` is the honest representation of a build with
	/// no browser capability at all, and the host reports `unavailable` for it
	/// rather than pretending an open failed. `Authority` is supplied by
	/// composition from the compiled host profile, so a profile that is not
	/// GitHub.com cannot silently inherit GitHub.com's authority.
	struct device_authorization_browser_handoff
	{
		device_authorization_browser_port* Browser = nullptr;
		std::string                        Authority
			= std::string{ default_device_verification_authority };
	};

	/// \brief Reports whether the address sits under the expected authority.
	///
	/// \details Exact host match over HTTPS and nothing else. No subdomain is
	/// accepted, because `github.com.example.invalid` is a subdomain of nothing
	/// the reviewer meant; no userinfo is accepted, because `github.com@evil`
	/// reads as the authority to a human and resolves elsewhere; no port is
	/// accepted, because the compiled authority names no port. The comparison
	/// is ASCII case-insensitive on the host only, which is the one component
	/// where case genuinely carries no meaning.
	[[nodiscard]] auto address_matches_verification_authority(
		std::string_view aAddress,
		std::string_view aAuthority) noexcept -> bool;

	// The one place authorized material leaves this host. It is a port rather
	// than a direct call so the host can be driven in a test without a whole
	// GitHub host behind it; `github_host_device_authorization_enrollment` below
	// is the production implementation and calls exactly `enroll_profile`.
	/// \brief Provides the device authorization enrollment port contract.
	class device_authorization_enrollment_port
	{
	public:
		virtual ~device_authorization_enrollment_port() = default;

		virtual void enroll(
			core::host_profile_id      aProfile,
			github_credential_material aCredential) = 0;
		/// \brief Carries issued lifetime facts alongside the existing enrollment.
		virtual void enroll_grant(core::host_profile_id aProfile, github_credential_material aCredential, device_binding_lifetime, bool) { enroll(aProfile, std::move(aCredential)); }
		/// \brief Confirms a stored refresh slot for the exact accepted access grant.
		virtual void settle_binding(github_credential_authority_key, core::github_identity_generation, device_binding_lifetime) { }
		/// \brief Invalidates durable metadata before spending refresh material.
		virtual void prepare_refresh(github_credential_authority_key, core::github_identity_generation, std::function<void(bool)> aCompletion) { aCompletion(true); }
		/// \brief Ends the grant when renewal becomes terminal, saying why.
		///
		/// \details [[BPR-0644]]. The reason decides what the GitHub host shows
		/// for a launch renewal that could not finish, whose grant it keeps: a
		/// network or credential-store failure is tried again at the next
		/// launch, and a refused or missing refresh needs signing in again.
		virtual void invalidate_grant(github_credential_authority_key, core::github_identity_generation, device_authorization_refresh_failure) { }
	};

	/// \brief Enrolls through the exact GitHub host vault boundary.
	class github_host_device_authorization_enrollment final
		: public device_authorization_enrollment_port
	{
	public:
		explicit github_host_device_authorization_enrollment(
			github_application_host& aHost) noexcept;

		void enroll(
			core::host_profile_id      aProfile,
			github_credential_material aCredential) override;
		void enroll_grant(core::host_profile_id aProfile, github_credential_material aCredential, device_binding_lifetime aLifetime, bool aRefreshSettled) override;
		void settle_binding(github_credential_authority_key aAuthority, core::github_identity_generation aGeneration, device_binding_lifetime aLifetime) override;
		void prepare_refresh(github_credential_authority_key aAuthority, core::github_identity_generation aGeneration, std::function<void(bool)> aCompletion) override;
		void invalidate_grant(github_credential_authority_key aAuthority, core::github_identity_generation aGeneration, device_authorization_refresh_failure aFailure) override;

	private:
		github_application_host* Host;
	};

	/// \brief Describes the device authorization capability record.
	///
	/// \details Whether this build has a registered application identity to
	/// run the flow under, and which profile that identity belongs to. It
	/// carries no client identifier, because the host does not need the value
	/// to know whether one exists and a value that never enters the host
	/// cannot leak from it.
	///
	/// The profile is here because a registered identity is registered
	/// somewhere: the composed wire is bound to one host and the client is
	/// that host's. A begin naming any other profile would run this host's
	/// flow under a foreign authority and enroll the resulting token under
	/// the profile that did not issue it, so the host refuses it exactly as
	/// it refuses an unregistered build ([[BPR-0535]]). It defaults to the
	/// zero identity, which no composed profile ever has, so an
	/// unbound capability admits nothing.
	///
	/// It defaults to unregistered on purpose. Every shipped build is
	/// unregistered until [[BPR-0318]] records a distributable identity, and a
	/// capability that defaults to available would make forgetting to compose
	/// one look like enabling the flow. Composition states the exception; the
	/// default states the rule.
	///
	/// When it is false the host constructs no request at all — the transport
	/// port is never reached — so "zero requests sent" is a structural property
	/// rather than a count that happens to be zero.
	struct device_authorization_capability
	{
		bool Registered = false;
		// The one profile this capability was composed for. Every other
		// profile is refused before a request is built.
		core::host_profile_id Profile;

		auto operator<=>(const device_authorization_capability&) const = default;
	};

	/// \brief Describes the device authorization host limits record.
	struct device_authorization_host_limits
	{
		std::uint32_t TotalBoundSeconds = device_authorization_total_bound_seconds;
		std::uint32_t MaximumPolls      = maximum_device_authorization_polls;
		// How far before the access token expires the renewal is attempted. A
		// lifetime shorter than twice the lead renews at its own midpoint
		// instead, because a lead longer than the lifetime would renew in the
		// past.
		std::uint32_t RefreshLeadSeconds = 300U;
		// Consecutive transport failures tolerated within one poll window before
		// the flow fails. Refusals of the server's identity or shape are not
		// counted here; those end the flow immediately.
		std::uint32_t MaximumPollFailures = 3U;

		auto operator<=>(const device_authorization_host_limits&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const device_authorization_host_limits& aLimits) noexcept -> bool
	{
		return aLimits.TotalBoundSeconds > 0U
			&& aLimits.TotalBoundSeconds <= device_authorization_total_bound_seconds
			&& aLimits.MaximumPolls > 0U
			&& aLimits.MaximumPolls <= maximum_device_authorization_polls
			&& aLimits.RefreshLeadSeconds
			<= maximum_device_authorization_validity_seconds
			&& aLimits.MaximumPollFailures > 0U
			&& aLimits.MaximumPollFailures <= 16U;
	}

	/// \brief Provides the device authorization application host contract.
	///
	/// \details Owns the whole device-authorization effect course: acquiring a
	/// device code, the poll schedule, the server interval and `slow_down`, the
	/// total bound, cancellation, shutdown, renewal before expiry, and the
	/// deterministic erasure that follows every terminal including the
	/// successful one. Legality, staleness, and which stage the flow is in stay
	/// in the core; this host executes the requests the core emits and feeds it
	/// back exactly one typed observation per request.
	///
	/// Authorized material passes through once: the access token becomes a vault
	/// entry through `enroll_profile` and this host keeps no copy, and the
	/// refresh material becomes a refresh-slot entry and is read back from the
	/// vault when it is needed rather than held.
	class device_authorization_application_host final
	{
	public:
		// Semantic ingress and inspection are confined to the serialized
		// decision lane supplied at construction. Transport, vault, and timer
		// callbacks may arrive on any thread and are marshalled onto that lane.
		// shutdown() closes ingress, cancels the timer and any outstanding
		// submission, and erases every transient without waiting. Composition
		// keeps every referenced port alive until is_quiescent().
		device_authorization_application_host(
			core::device_authorization_state      aInitialState,
			device_authorization_transport_port&  aTransport,
			github_credential_refresh_vault_port& aVault,
			device_authorization_enrollment_port& aEnrollment,
			device_authorization_clock_port&      aClock,
			device_authorization_timer_port&      aTimer,
			decision_executor&                    aDecisionLane,
			device_authorization_projection_port& aProjections,
			device_authorization_host_limits      aLimits     = { },
			device_authorization_browser_handoff  aHandoff    = { },
			device_authorization_capability       aCapability = { });
		~device_authorization_application_host();

		device_authorization_application_host(
			const device_authorization_application_host&) = delete;
		auto operator=(const device_authorization_application_host&)
			-> device_authorization_application_host& = delete;
		device_authorization_application_host(
			device_authorization_application_host&&) = delete;
		auto operator=(device_authorization_application_host&&)
			-> device_authorization_application_host& = delete;

		void dispatch(const core::device_authorization_event& aEvent);

		// The authority a refresh entry is keyed by is only knowable once an
		// authenticated `/user` observation has bound the profile to an account,
		// which happens after enrollment. Until it arrives the host holds new
		// refresh material in memory and stores it the moment it can; every
		// terminal, cancellation, and shutdown erases it instead.
		//
		// [[BPR-0546]]. The identity generation arrives with the authority
		// because only the GitHub host knows it. Every platform vault refuses
		// a context whose generation is zero (`valid()` in
		// `github_credential_vault.hpp`, checked before anything is queued),
		// and this host's own `State.IdentityGeneration` is the device flow's
		// copy of a generation composition seeds before any identity exists --
		// zero, for the whole run. A zero generation is refused here rather
		// than minted into a context the vault would refuse later.
		void bind_refresh_authority(
			github_credential_authority_key  aAuthority,
			core::github_identity_generation aIdentityGeneration);
		// The restart path schedules renewal from the validated remaining lifetime.
		// Missing or expired lifetime facts require re-enrollment.
		void resume_refresh(
			github_credential_authority_key        aAuthority,
			core::github_identity_generation       aIdentityGeneration,
			std::optional<device_binding_lifetime> aLifetime = std::nullopt);
		/// \brief [[BPR-0644]]. Renews a restored grant whose access lifetime
		/// elapsed while the application was closed.
		/// \details The existing rotation, begun at once rather than from a
		/// timer and bounded by the stored refresh lifetime instead of the
		/// elapsed access lifetime. It still tombstones the binding through
		/// `prepare_refresh` before spending anything, and ends by enrolling the
		/// rotated access token or by invalidating this exact grant.
		void renew_restored_grant(
			github_credential_authority_key  aAuthority,
			core::github_identity_generation aIdentityGeneration,
			device_binding_lifetime          aLifetime);
		/// \brief Cancels renewal when the selected accepted session is signed out or replaced manually.
		void release_refresh_authority();
		// The browser handoff, and the only thing in this host that changes no
		// core state at all. It validates the retained address against the
		// compiled authority, invokes the browser capability only if that
		// passes, and republishes the presentation with what happened. Every
		// outcome, including refusal, leaves the flow exactly where it was:
		// a browser that will not open is not a reviewer who will not
		// authorize.
		void open_verification_address();
		void shutdown();
		/// Composition may replace the connection ID of the same registered
		/// native server between flows. It must validate that server first.
		[[nodiscard]] auto replace_profile(core::host_profile_id aExpected, core::host_profile_id aReplacement) noexcept -> bool;

		[[nodiscard]] auto state() const noexcept
			-> const core::device_authorization_state&;
		[[nodiscard]] auto presentation() const noexcept
			-> const device_authorization_presentation&;
		[[nodiscard]] auto refresh_observation() const noexcept
			-> const device_authorization_refresh_observation&;
		// True while the host holds a device code or an access token that has
		// not reached the vault boundary yet. Every terminal, including the
		// successful one, leaves it false.
		[[nodiscard]] auto holds_transient_material() const noexcept -> bool;
		// True only between an authorization that produced refresh material and
		// the authority binding that gives it a slot to be stored under.
		[[nodiscard]] auto holds_pending_refresh_material() const noexcept
			-> bool;
		[[nodiscard]] auto is_closing() const noexcept -> bool;
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;
		[[nodiscard]] auto projection_delivery_failed() const noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DEVICE_AUTHORIZATION_APPLICATION_HOST_HPP
