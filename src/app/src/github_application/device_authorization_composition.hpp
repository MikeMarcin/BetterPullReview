#ifndef BPR_APP_GITHUB_APPLICATION_DEVICE_AUTHORIZATION_COMPOSITION_HPP
#define BPR_APP_GITHUB_APPLICATION_DEVICE_AUTHORIZATION_COMPOSITION_HPP
#include "bpr/application/device_authorization_application_host.hpp"
#include "bpr/application/device_authorization_projection.hpp"
#include "bpr/application/device_binding_store.hpp"
#include "bpr/application/github_credential_vault.hpp"
#include "bpr/core/device_authorization_model.hpp"
#include "bpr/core/github_model.hpp"

#include <cstddef>
#include <functional>
#include <optional>
#include <string_view>
#include <utility>

#if defined(BPR_BUILD_TESTING)
	#include <chrono>
	#include <memory>
	#include <string>
#endif

// Device-authorization sign-in as the profile runtime composes it:
// enrollment, projections, browser handoff, transport and refresh clocks.
namespace bpr::app {
#if defined(BPR_BUILD_TESTING)
	// Counts what crosses the enrollment boundary, and forwards it unchanged.
	//
	// The vault's own store count cannot answer "exactly one enrollment": the
	// GitHub host collapses a repeated enrollment for the same profile before
	// the vault ever sees a second operation, so a double call still reports
	// one store. That collapse is correct, and it is also exactly why the
	// count has to be taken here — at the boundary the RFC's criterion is
	// about — rather than downstream of something that hides duplicates.
	class counting_device_authorization_enrollment final
		: public application::device_authorization_enrollment_port
	{
	public:
		explicit counting_device_authorization_enrollment(
			application::device_authorization_enrollment_port& aInner) noexcept;

		void enroll(
			const core::host_profile_id             aProfile,
			application::github_credential_material aCredential) override;

		void enroll_grant(core::host_profile_id aProfile, application::github_credential_material aCredential, application::device_binding_lifetime aLifetime, bool aSettled) override;
		void settle_binding(application::github_credential_authority_key aAuthority, core::github_identity_generation aGeneration, application::device_binding_lifetime aLifetime) override;
		void prepare_refresh(application::github_credential_authority_key aAuthority, core::github_identity_generation aGeneration, std::function<void(bool)> aCompletion) override;
		void invalidate_grant(application::github_credential_authority_key aAuthority, core::github_identity_generation aGeneration, application::device_authorization_refresh_failure aFailure) override;

		[[nodiscard]] auto enrollments() const noexcept -> std::size_t;

	private:
		application::device_authorization_enrollment_port* Inner       = nullptr;
		std::size_t                                        Enrollments = 0U;
	};
#endif
	class setup_device_enrollment final : public application::device_authorization_enrollment_port
	{
	public:
		using enrollment = std::function<void(core::host_profile_id, application::github_credential_material, std::optional<application::device_binding_lifetime>, bool)>;
		setup_device_enrollment(application::device_authorization_enrollment_port& aInner, enrollment aEnroll);
		void enroll(core::host_profile_id aProfile, application::github_credential_material aCredential) override;
		void enroll_grant(core::host_profile_id aProfile, application::github_credential_material aCredential, application::device_binding_lifetime aLifetime, bool aSettled) override;
		void settle_binding(application::github_credential_authority_key aAuthority, core::github_identity_generation aGeneration, application::device_binding_lifetime aLifetime) override;
		void prepare_refresh(application::github_credential_authority_key aAuthority, core::github_identity_generation aGeneration, std::function<void(bool)> aCompletion) override;
		void invalidate_grant(application::github_credential_authority_key aAuthority, core::github_identity_generation aGeneration, application::device_authorization_refresh_failure aFailure) override;

	private:
		application::device_authorization_enrollment_port& Inner;
		enrollment                                         Enroll;
	};

	// Records what presentation was told. The device panel is a Qt surface
	// [[BPR-0280]] owns and tests at the adapter boundary; what composition
	// needs is the same published record, so the public self-test can state
	// the typed refusal a reviewer would see rather than infer it.
	class device_authorization_projection_sink final
		: public application::device_authorization_projection_port
	{
	public:
		void publish(
			const application::device_authorization_presentation& aPresentation)
			override;

		// The renewal lifecycle the host published last. Recorded for the
		// same reason the presentation is: a composed course has to state
		// what a reviewer would be told about renewal rather than infer it
		// from a rotation count nobody surfaced.
		void publish_refresh(
			const application::device_authorization_refresh_observation&
				aObservation) override;

		[[nodiscard]] auto last() const noexcept
			-> const application::device_authorization_presentation&;

		[[nodiscard]] auto last_refresh() const noexcept
			-> const application::device_authorization_refresh_observation&;

		[[nodiscard]] auto refresh_published() const noexcept -> std::size_t;

		[[nodiscard]] auto last_attempt() const noexcept
			-> application::device_authorization_handoff;

		[[nodiscard]] auto published() const noexcept -> std::size_t;

	private:
		application::device_authorization_presentation        Last;
		application::device_authorization_refresh_observation LastRefresh;
		application::device_authorization_handoff             LastAttempt
			= application::device_authorization_handoff::not_attempted;
		std::size_t Published        = 0U;
		std::size_t RefreshPublished = 0U;
	};

	// The device host publishes to exactly one port, and [[BPR-0526]] gave
	// the presentation a second reader without taking the first one away.
	// So this stands where the sink stood and hands every publication to
	// both: the sink first and unconditionally, then the mounted panel if
	// one was composed.
	//
	// Unconditionally is the whole point. Six executable contracts read
	// `DeviceProjections` through the self-test markers, and they have to
	// read exactly what they read before a window was in the picture --
	// including in a graph whose panel composition was cut. `publish()` is
	// synchronous and returns nothing, so a second reader cannot reorder,
	// delay, or veto what the first one sees.
	class composition_device_authorization_projection final
		: public application::device_authorization_projection_port
	{
	public:
		using presented_sink = std::function<void(
			const application::device_authorization_presentation&)>;
		using renewed_sink   = std::function<void(
			const application::device_authorization_refresh_observation&)>;

		composition_device_authorization_projection(
			application::device_authorization_projection_port& aRecorded,
			presented_sink                                     aPresented,
			renewed_sink                                       aRenewed);

		void publish(
			const application::device_authorization_presentation& aPresentation)
			override;

		// The renewal lifecycle fans out on the same terms and in the same
		// order: the sink first and unconditionally, then whoever
		// composition bound to watch renewal. [[BPR-0536]].
		void publish_refresh(
			const application::device_authorization_refresh_observation&
				aObservation) override;

	private:
		application::device_authorization_projection_port* Recorded = nullptr;
		presented_sink                                     Presented;
		renewed_sink                                       Renewed;
	};
#if defined(BPR_BUILD_TESTING)
	// The fixture profile's opener. A tests-enabled graph never binds the
	// real one: `system_browser_opener` hands the address to the desktop,
	// and a suite that launched a browser on a reviewer's machine — or on
	// a station running unattended — would be doing the one thing no test
	// here is allowed to do. This records what it was given and answers
	// yes, which is what the handoff contract needs to observe `opened`
	// without any program starting.
	class recording_device_authorization_browser final
		: public application::device_authorization_browser_port
	{
	public:
		[[nodiscard]] auto open(const std::string_view aAddress)
			-> bool override;

		[[nodiscard]] auto opens() const noexcept -> std::size_t;

		[[nodiscard]] auto last_address() const noexcept
			-> const std::string&;

	private:
		std::string LastAddress;
		std::size_t Opens = 0U;
	};
#endif
	// No registered application identity means there is nothing to talk to.
	// This is not a fixture and not a stub standing in for a real transport: it
	// is the honest shape of a build that has no client to authenticate as. The
	// host's capability gate refuses before any submission reaches it, so every
	// method here is unreachable, and each one answers a typed transport error
	// rather than succeeding quietly if the gate were ever bypassed.
	//
	// [[BPR-0318]] records a distributable identity; until it does, every
	// shipped platform composes this.
	class unregistered_device_authorization_transport final
		: public application::device_authorization_transport_port
	{
	public:
		void acquire_device_code(
			core::device_authorization_request_context  aContext,
			application::device_code_completion_handler aCompletion) override;

		void exchange_device_token(
			application::device_token_exchange           aExchange,
			application::device_token_completion_handler aCompletion) override;

		void refresh_access_token(
			application::device_token_exchange           aExchange,
			application::device_token_completion_handler aCompletion) override;

		void cancel(core::device_authorization_request_context) override;

		// Zero on every shipped build, and the public self-test says so. A
		// non-zero value would mean the capability gate let a submission
		// through, which is the exact failure this composition exists to make
		// impossible.
		[[nodiscard]] auto reached() const noexcept -> std::size_t;

	private:
		void refuse(
			const application::device_token_exchange&    aExchange,
			application::device_token_completion_handler aCompletion);

		std::size_t Reached = 0U;
	};

	// The recorded distributable production client, public by design: a
	// GitHub App client identifier carries no secret, and the shipped
	// artifact is required to contain exactly this one. Its registration,
	// permissions, and read-only course evidence live under
	// docs/verification/reviews/rfc0016-production-app-registration-v001/
	// (record commit c82e51c2c933887632fb0bb35db876375a151de9), and
	// scripts/verify_rfc0016_production_identity.py --release-binary
	// requires the enabled artifact to select it and to omit the private
	// BPR-0276 verification client.
	constexpr auto production_device_client_id
		= std::string_view{ "Iv23liitiYN1WSSbag50" };

	// macOS is registered under the recorded distributable identity since
	// [[BPR-0318]], Linux since [[BPR-0310]], and Windows since [[BPR-0311]];
	// each flipped its own platform in its own slice, against its own
	// target-native course, and neither flipped another's. Moving a card
	// between folders does not enable a capability, and neither does
	// building on a different operating system: this constant is the only
	// thing that does.
	[[nodiscard]] constexpr auto shipped_device_authorization_capability() noexcept
		-> application::device_authorization_capability
	{
#if defined(__APPLE__) || defined(__linux__) || defined(_WIN32)
		return { .Registered = true };
#else
		return { .Registered = false };
#endif
	}

	[[nodiscard]] auto initial_device_authorization_state()
		-> core::device_authorization_state;
#if defined(BPR_BUILD_TESTING)
	// How many renewals the refresh course means to drive, which is how many
	// waits the clock below is allowed to skip. Two, because one rotation
	// succeeds whether or not the rotation keeps the material it stored, and
	// only the second says the renewal chain survived the first.
	constexpr auto device_refresh_course_rotations = std::size_t{ 2U };

	// How far one of those steps moves the clock: the whole wait a renewal is
	// armed for, less one second. The wait is the fixture grant's eight-hour
	// access-token lifetime less the host's default five-minute renewal lead,
	// so a step of that size exactly leaves the renewal due a second from now.
	//
	// [[BPR-0547]]. The second is deliberate and is the difference between
	// testing the lifecycle and timing it. A rotation enrolls its new access
	// token and arms the next renewal in the same turn, while the enrollment it
	// just made is still travelling: the profile activates, the old session is
	// invalidated, and the vault operations that follow decide what is left in
	// the refresh slot. A renewal made due in that same instant outruns all of
	// it and reads a slot nothing has had the chance to touch yet, which is a
	// race the shipped product never runs -- there the next renewal is eight
	// hours after the supersession has long settled. A second is a thousandfold
	// margin over the few posts that supersession costs, and it costs the two
	// rotations this course drives two seconds.
	//
	// If either constant it is derived from moves, this becomes an ordinary
	// step: too large and the renewal is due at once and races again, too small
	// and the course waits out its budget and says so.
	constexpr auto device_refresh_course_step = std::chrono::seconds{
		28'800 - 300 - 1,
	};

	// A one-way step forward, applied once per renewal the host arms and at
	// most `Steps` times. The port requires a monotonic reading -- a clock that
	// stepped back would let a poll outrun its interval -- so each offset
	// latches and stays rather than being added and removed around a call.
	//
	// [[BPR-0547]]. It steps per arming rather than once, because a course that
	// proves renewal works more than once needs a second wait skipped and the
	// second one is armed by the host itself rather than by composition. Every
	// re-arm runs the same three steps in one turn: capture `AuthorizedAt` from
	// this clock, raise the rotation count, then read this clock again to work
	// out how much of the new lifetime is already spent. So the armings this
	// composition can count -- the first bind, and every rotation after it --
	// each fall strictly inside that window, and the offset that lands there
	// makes the second read a whole lifetime past the capture beside it: the
	// renewal is due now. Stepping on every rotation forever would rotate for
	// as long as the run lasts and no count could be pinned, so the steps are
	// capped at what the course drives; the arming after the last one keeps its
	// real eight hours, which is what leaves the run reporting `scheduled`
	// instead of being read mid-rotation.
	class composition_device_refresh_clock final
		: public application::device_authorization_clock_port
	{
	public:
		composition_device_refresh_clock(
			std::unique_ptr<application::device_authorization_clock_port> aInner,
			std::function<std::size_t()>                                  aArmings,
			const std::chrono::milliseconds                               aOffset,
			const std::size_t                                             aSteps);

		[[nodiscard]] auto observe_wall_seconds() noexcept -> std::int64_t override;
		[[nodiscard]] auto observe_monotonic() noexcept
			-> std::chrono::milliseconds override;

	private:
		std::unique_ptr<application::device_authorization_clock_port> Inner;
		std::function<std::size_t()>                                  Armings;
		std::chrono::milliseconds                                     Offset{ 0 };
		std::size_t                                                   Steps   = 0U;
		std::size_t                                                   Applied = 0U;
	};

	class composition_device_wall_clock final : public application::device_authorization_clock_port
	{
	public:
		composition_device_wall_clock(std::unique_ptr<application::device_authorization_clock_port> aInner, std::int64_t aOffset);
		[[nodiscard]] auto observe_monotonic() noexcept -> std::chrono::milliseconds override;
		[[nodiscard]] auto observe_wall_seconds() noexcept -> std::int64_t override;

	private:
		std::unique_ptr<application::device_authorization_clock_port> Inner;
		std::int64_t                                                  Offset;
	};
#endif
}
#endif
