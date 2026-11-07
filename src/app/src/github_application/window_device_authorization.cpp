#include "github_application/window_composition.hpp"

#include "bpr/application/device_authorization_application_host.hpp"
#include "bpr/application/github_host_authority.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/github_transition.hpp"
#include "bpr/qt/event_loop_executor.hpp"
#include "github_application/public_preview_wire_transport.hpp"
#include "github_provider.hpp"

#include <algorithm>
#include <deque>
#include <limits>
#include <variant>

#if defined(BPR_BUILD_TESTING)
	#include "github_application/device_authorization_composition.hpp"
	#include "github_application_fixture_access.hpp"
	#include "github_public_path_fixture.hpp"
	#include <fstream>
#endif

namespace bpr::app {
	auto github_application::implementation::credential_vault() noexcept -> application::github_credential_vault_port& { return Profile->credential_vault(); }

	// Registered-course evidence. Zero outside a tests-enabled build with
	// the recorded grant, because nothing else can enroll.
	auto github_application::implementation::device_vault_stores() const noexcept -> std::size_t
	{
#if defined(BPR_BUILD_TESTING)
		if (Fixture != nullptr && RegisteredDeviceCourse) {
			return Fixture->evidence().VaultStores;
		}
#endif
		return 0U;
	}

	// [[BPR-0539]]. What that one store holds. The count alone is
	// satisfied by any enrollment, and before this course stopped
	// connecting first the enrollment it counted was the fixture's own
	// connect credential -- so the identity of the stored material is
	// reported beside the count, and the comparison against the recorded
	// grant is made where both are known.
	auto github_application::implementation::device_vault_stored_digest() const -> std::string
	{
#if defined(BPR_BUILD_TESTING)
		if (Fixture != nullptr && RegisteredDeviceCourse) {
			return Fixture->evidence().VaultStoredDigest;
		}
#endif
		return { };
	}

	auto github_application::implementation::device_vault_stored_is_grant() const -> bool
	{
#if defined(BPR_BUILD_TESTING)
		if (Fixture != nullptr && RegisteredDeviceCourse) {
			return Fixture->evidence().VaultStoredIsDeviceGrant;
		}
#endif
		return false;
	}

	// Only the recording opener counts, and only a run inside the
	// automated test boundary binds one ([[BPR-0797]]). The real opener's
	// evidence is a browser window a person sees on a station, which this
	// process has no way to observe and must not claim to -- which is also
	// why a zero here is not a claim that nothing opened.
	auto github_application::implementation::device_browser_opens() const noexcept -> std::size_t
	{
#if defined(BPR_BUILD_TESTING)
		return RecordingBrowser.opens();
#else
		return 0U;
#endif
	}

	auto github_application::implementation::device_enrollments() const noexcept -> std::size_t
	{
#if defined(BPR_BUILD_TESTING)
		if (CountedDeviceEnrollment != nullptr) {
			return CountedDeviceEnrollment->enrollments();
		}
#endif
		return 0U;
	}

	// RFC-BPR-0017 evidence. Three exclusive answers, read from the host
	// rather than inferred from the presentation: the composition refusal,
	// the core policy's refusal, and the mode actually entered.
	auto github_application::implementation::public_preview_unavailable() const noexcept -> bool
	{
		if (Host == nullptr) {
			return true;
		}
		const auto& refusal = Host->public_preview_refusal();
		return refusal.has_value()
			&& refusal->Cause
			== application::github_public_preview_refusal_cause::
				public_preview_unavailable;
	}

	auto github_application::implementation::public_preview_policy_refused() const noexcept -> bool
	{
		if (Host == nullptr) {
			return false;
		}
		const auto& refusal = Host->public_preview_refusal();
		return refusal.has_value()
			&& refusal->Cause
			== application::github_public_preview_refusal_cause::
				policy_refused;
	}

	auto github_application::implementation::public_preview_signed_out() const noexcept -> bool
	{
		return Host != nullptr
			&& Host->access_mode()
			== core::github_access_mode::public_preview;
	}

	auto github_application::implementation::public_preview_requests() const noexcept -> std::size_t
	{
		return CountedAnonymousPullRequests == nullptr
			? 0U
			: CountedAnonymousPullRequests->reads();
	}

	// Authenticated transports the session selected during the run. A
	// signed-out read builds neither, so any non-zero value here is an
	// authorization header this mode is not allowed to have sent.
	auto github_application::implementation::public_preview_header_effects() const noexcept
		-> std::size_t
	{
#if defined(BPR_BUILD_TESTING)
		if (Fixture != nullptr) {
			const auto evidence = Fixture->evidence();
			return (evidence.BootstrapTransportSelected ? 1U : 0U)
				+ (evidence.AccountTransportSelected ? 1U : 0U);
		}
#endif
		return 0U;
	}

	// Credential operations that crossed the vault boundary during the
	// run. A signed-out read consults no vault, so this is zero or the
	// mode reached for a credential it must not have.
	auto github_application::implementation::public_preview_vault_effects() const noexcept
		-> std::size_t
	{
#if defined(BPR_BUILD_TESTING)
		if (Fixture != nullptr) {
			const auto evidence = Fixture->evidence();
			return evidence.VaultStores
				+ (evidence.CredentialRetrieved ? 1U : 0U)
				+ (evidence.CredentialDeleted ? 1U : 0U);
		}
#endif
		return 0U;
	}

	// [[BPR-0316]] live-candidate evidence. The claim is about what the
	// anonymous provider was built from, so it is answered by comparing
	// what that provider was constructed with against the shipped factory
	// this composition made. A composition that handed the provider any
	// other object -- a fixture seam, a loopback twin -- fails the
	// comparison without anyone having to remember to update a flag.
	auto github_application::implementation::public_preview_production_factories() const noexcept
		-> bool
	{
#if defined(BPR_BUILD_TESTING)
		return ShippedAnonymousTransports != nullptr
			&& AnonymousTransportFactory.get()
			== static_cast<adapters::github::detail::github_wire_transport_factory_port*>(
				   ShippedAnonymousTransports.get())
			&& AnonymousSmartGit != nullptr && Fixture == nullptr;
#else
		return false;
#endif
	}

	auto github_application::implementation::public_preview_wire_requests() const noexcept
		-> std::size_t
	{
#if defined(BPR_BUILD_TESTING)
		return ShippedAnonymousTransports == nullptr
			? 0U
			: ShippedAnonymousTransports->requests();
#else
		return 0U;
#endif
	}

	auto github_application::implementation::public_preview_wire_capability_probes() const noexcept
		-> std::size_t
	{
#if defined(BPR_BUILD_TESTING)
		return ShippedAnonymousTransports == nullptr
			? 0U
			: ShippedAnonymousTransports->capability_probes();
#else
		return 0U;
#endif
	}

	auto github_application::implementation::public_preview_wire_credentialed_refusals()
		const noexcept -> std::size_t
	{
#if defined(BPR_BUILD_TESTING)
		return ShippedAnonymousTransports == nullptr
			? 0U
			: ShippedAnonymousTransports->credentialed_refusals();
#else
		return 0U;
#endif
	}

	auto github_application::implementation::public_preview_wire_operations() const
		-> std::vector<public_preview_wire_operation_record>
	{
#if defined(BPR_BUILD_TESTING)
		return ShippedAnonymousTransports == nullptr
			? std::vector<public_preview_wire_operation_record>{ }
			: ShippedAnonymousTransports->operations();
#else
		return { };
#endif
	}

	// The shared hourly core budget, taken from the rate-limit headers the
	// session carries rather than from any body.
	auto github_application::implementation::public_preview_budget() const noexcept
		-> std::optional<application::github_rate_limit_record>
	{
		return WindowRead == nullptr ? std::nullopt : WindowRead->rate_limits().Core;
	}

	// The accepted snapshot's own discriminant, not the session's: a
	// snapshot accepted under an authenticated authority answers false
	// here even where the session says signed out.
	auto github_application::implementation::public_preview_snapshot_signed_out() const noexcept
		-> bool
	{
		if (Host == nullptr) {
			return false;
		}
		const auto& snapshot = WindowRead->accepted_snapshot();
		return snapshot.has_value()
			&& snapshot->Mode == core::github_access_mode::public_preview;
	}

	auto github_application::implementation::public_preview_snapshot_head_oid() const
		-> std::string
	{
		if (Host == nullptr) {
			return { };
		}
		const auto& snapshot = WindowRead->accepted_snapshot();
		return snapshot.has_value()
			? std::string{ snapshot->PullRequest.HeadOid.value() }
			: std::string{ };
	}

	auto github_application::implementation::device_transport_submissions() const noexcept
		-> std::size_t
	{
#if defined(BPR_BUILD_TESTING)
		if (Fixture != nullptr && RegisteredDeviceCourse) {
			return Fixture->evidence().DeviceTransportSubmissions;
		}
#endif
		return 0U;
	}

	// Every regular file under the application data root that contains the
	// enrolled token. The vault is the only place a credential belongs, so
	// the expected answer is none; settings and SQLite are the two places a
	// second copy would plausibly appear, and reading every file is
	// cheaper than guessing which.
	auto github_application::implementation::device_durable_copies() const -> std::size_t
	{
#if defined(BPR_BUILD_TESTING)
		if (Fixture == nullptr || !RegisteredDeviceCourse) {
			return 0U;
		}
		const auto token = Fixture->device_authorization_access_token();
		if (token.empty()) {
			return 0U;
		}
		constexpr auto  unavailable = std::numeric_limits<std::size_t>::max();
		const auto&     root        = Fixture->application_data_root();
		std::error_code error;
		if (root.empty() || !std::filesystem::is_directory(std::filesystem::symlink_status(root, error)) || error) {
			return unavailable;
		}
		// [[BPR-0877]]. The one file this scan is blinded by is the one this
		// runtime holds open itself: the profile's lease lock, opened with
		// zero sharing, which Windows then refuses to every reader including
		// this one. Answering `unavailable` for the whole root because of it
		// is what made four device-authorization rows report a sentinel
		// instead of their count.
		//
		// Only that exact path is excluded, and the fail-closed answer stays
		// for every other unreadable file: a file this scan cannot read
		// could hold a durable copy of the token, and counting it as absent
		// would understate the number in the dangerous direction. A lease
		// lock never holds a copy of anything.
		//
		// Excluded on every platform rather than behind a Windows guard. It
		// is not a copy on POSIX either; POSIX merely fails to notice,
		// because a locked file still opens for reading there. One rule is
		// easier to keep true, and it keeps macOS and Linux on the same path.
		const auto lease  = (root / "profile-runtime.lock").lexically_normal();
		auto       copies = std::size_t{ 0 };
		auto       files  = std::size_t{ 0 };
		auto       bytes  = std::uintmax_t{ 0 };
		for (auto iterator = std::filesystem::recursive_directory_iterator{ root, std::filesystem::directory_options::none, error };
			 iterator != std::filesystem::recursive_directory_iterator{ } && !error; iterator.increment(error)) {
			const auto& entry = *iterator;
			const auto  type  = entry.symlink_status(error);
			if (error) {
				return unavailable;
			}
			if (!std::filesystem::is_regular_file(type)) {
				continue;
			}
			if (entry.path().lexically_normal() == lease) {
				continue;
			}
			const auto size = entry.file_size(error);
			if (error || ++files > 4096U || size > 1024U * 1024U || bytes + size > 16U * 1024U * 1024U) {
				return unavailable;
			}
			bytes += size;
			auto file = std::ifstream{ entry.path(), std::ios::binary };
			if (!file) {
				return unavailable;
			}
			auto contents = std::string(static_cast<std::size_t>(size), '\0');
			file.read(contents.data(), static_cast<std::streamsize>(contents.size()));
			const auto readBytes = file.gcount();
			const auto trailing  = file.peek();
			if (readBytes != static_cast<std::streamsize>(contents.size()) || file.bad() || trailing != std::char_traits<char>::eof()) {
				return unavailable;
			}
			if (contents.find(token) != std::string::npos) {
				++copies;
			}
		}
		return error ? unavailable : copies;
#else
		return 0U;
#endif
	}

	// [[BPR-0536]]. The one place the device host learns which vault slot
	// its renewable material belongs to, and the only caller that may name
	// one: the key is minted inside `github_application_host` when an
	// authenticated `/user` observation is accepted, and composition reads
	// it rather than assembling one.
	//
	// Bound only after this run's device flow authorized. A profile signed
	// in with a token the reviewer pasted has no renewable material and no
	// renewal chain to lose, and telling its panel to renew would end in a
	// vault that answers `absent` and a reviewer told to authorize again
	// for no reason.
	//
	// An acceptance with no authorized stage behind it is the restart: the
	// credential was re-established from what the vault already held, so
	// renewal continues from the entry rather than starting over -- but
	// only once the host says an entry is there. The guard is exact key
	// equality against a key we actually acted on, so a republication that
	// names the same slot does nothing once armed and everything before.
	// [[BPR-0550]]. The memo `bind_device_refresh_authority` keeps records
	// which slot this composition already bound the device host to, so a
	// republication naming the same one costs nothing. A second grant in one
	// run establishes its own session, and for the same account that session
	// names the same slot -- the memo would early return, and the material
	// the new grant parked in the host would never be flushed, because the
	// host released the previous binding when the new flow began.
	//
	// Reaching `authorized` is the moment a grant exists that no binding has
	// been made for. The memo is dropped there, on the transition rather
	// than on the level, so the acceptance that follows rebinds exactly once
	// and a stage that merely stays authorized rebinds nothing.
	void github_application::implementation::observe_device_stage(
		const application::device_authorization_presentation& aPresentation)
	{
		const auto authorized = aPresentation.Stage
			== core::device_authorization_stage::authorized;
		if (authorized && !DeviceStageAuthorized) {
			DeviceRefreshAuthority.reset();
		}
		DeviceStageAuthorized = authorized;
		if (SetupDeviceRequest && SetupDestination && !authorized && core::terminal(aPresentation.Stage)) {
			const auto request = *SetupDeviceRequest;
			SetupDeviceRequest.reset();
			if (aPresentation.Stage == core::device_authorization_stage::unavailable || aPresentation.Stage == core::device_authorization_stage::disabled) {
				DecisionLane.post([this, request] {
					if (!Closing && SetupDestination->pending() && SetupDestination->pending()->Request == request) {
						authenticate_manual(*SetupDestination->pending());
					}
				});
			} else {
				SetupDestination->cancel();
			}
		}
	}

	void github_application::implementation::bind_device_refresh_authority(const application::github_presentation_update& aUpdate) { Profile->bind_device_refresh_authority(aUpdate); }

	// Renewal, as presentation is told it. The panel is the reader that
	// matters; the counters are what a composed course states.
	void github_application::implementation::observe_device_refresh(
		const application::device_authorization_refresh_observation&
			aObservation)
	{
		DeviceRefreshObservation = aObservation;
		++DeviceRefreshPublications;
		if (DevicePanel != nullptr) {
			DevicePanel->apply_refresh(aObservation);
		}
	}

	// The mounted panel's one way out. Beginning and cancelling are core
	// intents and reach the host's transition lane; opening a browser is
	// not an intent and reaches the host's handoff directly. Qt never opens
	// an address itself, so this is the only place a device gesture becomes
	// an effect.
	void github_application::implementation::dispatch_device_authorization_gesture(
		const application::device_authorization_gesture& aGesture)
	{
		if (Closing || DeviceHost == nullptr) {
			return;
		}
		if (const auto* const begin
			= std::get_if<core::begin_device_authorization_intent>(
				&aGesture)) {
			if (Host == nullptr || !core::github_connection_setup_available(Host->state())) {
				return;
			}
			if (GithubCliDiscoveryPending) {
				retire_cli_discovery();
			}
			cancel_restoration(true);
			retire_setup_effects();
			if (SetupDestination && SetupDestination->pending()
				&& SetupDestination->begin_authentication(SetupDestination->pending()->Request, begin->Profile)) {
				authenticate(*SetupDestination->pending());
				return;
			}
			if (SetupDestination) {
				SetupDestination->cancel();
			}
			DeviceHost->dispatch(core::begin_device_authorization_intent{ .Profile = native_sign_in_profile(begin->Profile) });
			return;
		}
		if (const auto* const cancel
			= std::get_if<core::cancel_device_authorization_intent>(
				&aGesture)) {
			retire_setup_effects();
			if (SetupDestination) {
				SetupDestination->cancel();
			}
			DeviceHost->dispatch(*cancel);
			return;
		}
		DeviceHost->open_verification_address();
	}

	// The same typed intent the device panel dispatches. Composition owns
	// the profile it names, because the panel is given one rather than
	// choosing one.
	void github_application::implementation::select_device_authorization()
	{
		if (Closing || DeviceHost == nullptr || Profiles.empty()) {
			return;
		}
		DeviceHost->dispatch(core::begin_device_authorization_intent{
			.Profile = Profiles.front().profile(),
		});
	}

	// [[BPR-0771]]. Automation sends the intent the retired "Read without
	// signing in" button sent, for the same host, through the window's own
	// dispatch path. Select native HTTPS explicitly; CLI remains unavailable.
	void github_application::implementation::activate_public_preview(const bool aEnterprise)
	{
		if (Closing || Host == nullptr || Window == nullptr) {
			return;
		}
		const auto selected = std::ranges::find_if(
			Profiles,
			[aEnterprise](const adapters::github::validated_github_host_profile& aProfile) {
				const auto enterprise = aProfile.server_contract()
					!= application::github_server_contract::github_com_2022_11_28;
				return enterprise == aEnterprise
					&& aProfile.authentication_transport()
					== application::github_authentication_transport::native_https;
			});
		if (selected == Profiles.end() || Window->server_card_for(selected->profile()) == nullptr) {
			return;
		}
		dispatch_intent(application::github_intent{
			core::activate_host_profile_intent{
				.Profile = selected->profile(),
				.Mode    = core::github_access_mode::public_preview,
			},
		});
	}

	// One write and one triage intent, offered to the mode and answered by
	// the core policy alone. `public_preview_legality` is a pure question:
	// it reaches no provider, asks for no transport and builds no request,
	// so the refusals counted here cost exactly the zero requests the
	// markers beside them report.
	void github_application::implementation::attempt_public_preview_writes()
	{
		if (Host == nullptr) {
			return;
		}
		constexpr std::array intents{
			core::anonymous_read_intent::publish_review_comment,
			core::anonymous_read_intent::set_viewed_file_state,
		};
		for (const auto intent : intents) {
			if (std::holds_alternative<core::anonymous_intent_refused>(
					Host->public_preview_legality(intent))) {
				++PublicPreviewWriteRefusals;
			}
		}
	}

	// The instant the signed-out presentation is measured from. The window
	// reads no clock of its own, so composition hands it one on every
	// publication; a presentation that carries the host's own instant
	// overrides it. Seconds since the epoch, because that is what the
	// budget's reset is expressed in.
	auto github_application::implementation::public_preview_now() noexcept -> std::uint64_t
	{
		const auto now = std::chrono::duration_cast<std::chrono::seconds>(
							 std::chrono::system_clock::now()
								 .time_since_epoch())
							 .count();
		return now <= 0 ? 0U : static_cast<std::uint64_t>(now);
	}

	// Where the switch-to-sign-in gesture lands. The host names the
	// profile the signed-out session was activated for, so the flow that
	// begins is the one that returns the reviewer to the pull request they
	// were already reading. It is the same typed intent the device panel
	// dispatches; nothing here knows how the flow works.
	void github_application::implementation::begin_public_preview_sign_in(const core::host_profile_id aProfile)
	{
		if (Host == nullptr || !core::github_connection_setup_available(Host->state())) {
			return;
		}
		cancel_restoration(true);
		++PublicPreviewSignInRequests;
		if (Closing || DeviceHost == nullptr || aProfile.value() == 0U) {
			return;
		}
		DeviceHost->dispatch(core::begin_device_authorization_intent{
			.Profile = aProfile,
		});
	}

	// The same typed intent the device panel dispatches for the user's
	// cancel. The probe uses it to end a registered platform's real flow
	// at `awaiting_authorization`, which no unregistered composition can
	// reach.
	void github_application::implementation::cancel_device_authorization()
	{
		if (Closing || DeviceHost == nullptr) {
			return;
		}
		DeviceHost->dispatch(core::cancel_device_authorization_intent{ });
	}

	// The same request the device panel's "Open browser" action makes.
	// Not a core intent: opening a browser decides nothing about the
	// grant, so it reaches the host directly and the host republishes
	// what happened without a transition ([[BPR-0280]]).
	void github_application::implementation::open_device_verification_address()
	{
		if (Closing || DeviceHost == nullptr) {
			return;
		}
		DeviceHost->open_verification_address();
	}
}
