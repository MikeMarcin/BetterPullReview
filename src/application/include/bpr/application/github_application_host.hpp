#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_APPLICATION_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_APPLICATION_HOST_HPP

#include "bpr/application/github_window_read_host.hpp"
#include "bpr/core/setup_destination.hpp"

#include "bpr/application/device_binding_store.hpp"
#include "bpr/application/git_smart_transport_port.hpp"
#include "bpr/application/github_capability_cache.hpp"
#include "bpr/application/github_credential_vault.hpp"
#include "bpr/application/github_epoch_handoff.hpp"
#include "bpr/application/github_projection.hpp"
#include "bpr/application/github_provider_port.hpp"
#include "bpr/application/github_publication_port.hpp"
#include "bpr/application/port.hpp"
#include "bpr/application/public_preview_projection.hpp"
#include "bpr/application/review_mutation_port.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/github_transition.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace bpr::application {
	inline constexpr std::size_t maximum_github_credential_bindings      = 64U;
	inline constexpr std::size_t maximum_pending_device_profile_restores = 32U;

	/// \brief Startup binding selection only; ready does not mean an accepted session.
	enum class device_profile_restore_status { ready_for_session,
											   unavailable,
											   cancelled };

	/// \brief [[BPR-0644]]. A restored device grant whose access lifetime elapsed
	/// while the application was closed and whose refresh lifetime remains.
	/// \details Names the exact authority, the identity generation of the
	/// restore activation that is held until the renewal settles, and the
	/// stored lifetime. It carries no credential material.
	struct device_launch_renewal
	{
		github_credential_authority_key  Authority;
		core::github_identity_generation IdentityGeneration;
		device_binding_lifetime          Lifetime;
		auto                             operator<=>(const device_launch_renewal&) const = default;
	};
	/// \brief Begins a launch renewal, or with no value releases the device
	/// host's renewal chain: the launch renewal begun, or an accepted
	/// session's that gave up ([[BPR-1116]]).
	using device_launch_renewal_sink = std::function<void(const std::optional<device_launch_renewal>&)>;

	enum class github_credential_shutdown_policy {
		retain,
		delete_retained_on_shutdown,
		retain_accepted_device_grants,
	};

	/// \brief Names where the material behind an accepted credential came from.
	///
	/// \details Presence of a refresh entry beside a credential is not by
	/// itself evidence that the credential renews itself: the vault keys both
	/// slots by the same authority, so a token the reviewer pasted for an
	/// account a past device grant once enrolled lands on the very slot that
	/// grant's material sits beside. Spending that material would replace the
	/// pasted session with a device one the reviewer never asked for. So the
	/// host carries what it knows about the material's origin and answers the
	/// renewability question with both facts rather than with presence alone.
	/// `manual` is the default everywhere, because a caller that does not say
	/// where its material came from has not established that it came from a
	/// grant.
	enum class github_credential_provenance : std::uint8_t {
		/// A token the reviewer supplied. Nothing renews it.
		manual,
		/// A device grant this host enrolled, or a binding restored from one.
		device_grant,
	};

	/// Why a public-preview activation or read was refused.
	/// Where a switch-to-sign-in gesture goes. The host does not own the
	/// device-authorization flow, so it forwards the profile the signed-out
	/// session was activated for and lets composition begin the flow that
	/// returns the reviewer to the same pull request.
	using public_preview_sign_in_sink
		= std::function<void(core::host_profile_id)>;

	/// \brief Provides the github credential binding catalog contract.
	class github_credential_binding_catalog final
	{
	public:
		github_credential_binding_catalog() = default;
		~github_credential_binding_catalog(); // NOLINT(performance-trivially-destructible): the out-of-line destructor is a tested lifetime boundary.

		github_credential_binding_catalog(
			const github_credential_binding_catalog&) = delete;
		auto operator=(const github_credential_binding_catalog&)
			-> github_credential_binding_catalog&                              = delete;
		github_credential_binding_catalog(github_credential_binding_catalog&&) = delete;
		auto operator=(github_credential_binding_catalog&&)
			-> github_credential_binding_catalog& = delete;

		[[nodiscard]] auto size() const noexcept -> std::size_t;

	private:
		[[nodiscard]] auto find(core::host_profile_id aProfile) const noexcept
			-> const github_credential_authority_key*;
		/// \brief Returns where the material behind a remembered binding came
		/// from, or `manual` when nothing is remembered for the profile.
		[[nodiscard]] auto provenance(core::host_profile_id aProfile) const noexcept
			-> github_credential_provenance;
		[[nodiscard]] auto
			 remember(github_credential_authority_key aAuthority,
					  github_credential_provenance    aProvenance) noexcept -> bool;
		void forget(const github_credential_authority_key& aAuthority) noexcept;

		std::array<std::optional<github_credential_authority_key>,
				   maximum_github_credential_bindings>
			Entries{ };
		// Kept beside the key rather than inside it, because the key names a
		// vault slot and two different origins can name the same slot. A
		// resume inside this process reads it back, which is what lets a
		// device grant this host enrolled still renew after the session was
		// re-established from the vault.
		std::array<github_credential_provenance,
				   maximum_github_credential_bindings>
			Provenances{ };

		friend class github_application_host;
	};

	/// \brief Provides the github application host contract.
	class github_application_host final
	{
	public:
		// shutdown() closes ingress but does not wait for remote work. When
		// is_quiescent() is false, composition keeps this host and its referenced
		// ports, decision lane, and projections alive while draining callbacks.
		// delete_retained_on_shutdown makes an exact vault delete terminal part of
		// quiescence for the credential authority retained by this host.
		// Destruction alone cannot guarantee cleanup of an extracted vault store.
		github_application_host(
			core::github_state                 aInitialState,
			github_profile_port&               aProfiles,
			github_viewer_port&                aViewers,
			github_capability_port&            aCapabilities,
			github_pull_request_port&          aPullRequests,
			github_credential_vault_port&      aCredentials,
			github_credential_binding_catalog& aCredentialBindings,
			decision_executor&                 aDecisionLane,
			github_projection_port&            aProjections,
			github_credential_shutdown_policy  aCredentialShutdownPolicy = github_credential_shutdown_policy::retain,
			github_epoch_handoff_port*         aEpochs                   = nullptr,
			// The credential-free read authority. Null until composition
			// supplies one, which is what refusing public preview means
			// today: the mode is unavailable rather than degraded.
			github_pull_request_port* aAnonymousPullRequests = nullptr,
			// The credential-free smart-Git authority. Composition owns it and
			// passes an instance that can carry no credential: it is the only
			// way a public-preview session reaches a Git repository at all.
			// Null keeps the signed-out epoch handoff unavailable, which is
			// what a composition without it means.
			git_smart_transport_factory_port* aAnonymousSmartGit = nullptr,
			// The signed-out presentation sink. Null until composition
			// supplies one, which leaves the mode working but unpresented
			// rather than silently authenticated.
			public_preview_projection_port* aPublicPreview = nullptr,
			core::bpr_profile_id            aBprProfile    = core::default_bpr_profile_id);
		~github_application_host();

		github_application_host(const github_application_host&) = delete;
		auto operator=(const github_application_host&)
			-> github_application_host&                    = delete;
		github_application_host(github_application_host&&) = delete;
		auto operator=(github_application_host&&)
			-> github_application_host& = delete;

		void dispatch(const core::github_event& aEvent);
		/// \brief Attaches an exact read window before or after authentication; accepted identity binds it until exact retirement.
		/// Startup restoration may attach up to 32 generation>1 stable IDs once,
		/// in any order. Composition delivers all restored contexts before fresh
		/// generation1 windows; the first fresh admission seals restoration.
		/// Fresh IDs increase beyond every admitted ID; retired IDs never reattach.
		[[nodiscard]] auto attach_window(const std::shared_ptr<github_window_read_host>& aWindow) -> bool;
		/// \brief Retires an explicit attachment before considering the standalone legacy read slot.
		void detach_window(core::bpr_window_context aWindow);
		/// \brief Adopts an attached window's accepted read for the host-level projections.
		///
		/// \details [[BPR-0715]]. An explicitly attached primary window reads
		/// through its own `github_window_read_host`; the public-preview
		/// presentation is still the host's, so the window's accepted
		/// snapshot and rate limits are adopted here after each read update.
		/// Ignored for a window that is not attached.
		/// Fresh provider acceptance also shares its server snapshot with
		/// exact-authority siblings, leaving their epoch/navigation untouched.
		void observe_attached_read(const std::shared_ptr<github_window_read_host>& aWindow, bool aFreshSnapshot = false);
		// The provenance is the caller's statement about where the material
		// came from, and only the device-authorization enrollment boundary
		// says `device_grant`. It decides nothing about the session: it
		// decides whether the vault's answer about a refresh entry beside
		// this credential may be believed later.
		void enroll_profile(core::host_profile_id        aProfile,
							github_credential_material   aCredential,
							github_credential_provenance aProvenance
							= github_credential_provenance::manual,
							std::optional<device_binding_lifetime> aLifetime       = std::nullopt,
							bool                                   aRefreshSettled = false);
		/// \brief Validates setup credentials without replacing the active review or writing the vault.
		/// The decision-lane callback must accept the exact still-current owner/request before commit.
		void stage_setup_enrollment(core::pending_setup_destination                                   aPending,
									github_credential_material                                        aCredential,
									std::function<bool(const core::pending_setup_destination&, bool)> aAccept,
									github_credential_provenance                                      aProvenance     = github_credential_provenance::manual,
									std::optional<device_binding_lifetime>                            aLifetime       = std::nullopt,
									bool                                                              aRefreshSettled = false);
		void cancel_setup_enrollment();

		// Resume asks only for a profile. The host-owned trusted binding catalog,
		// populated from prior accepted /user observations, selects the exact
		// account authority. Public account and login values never select vault
		// entries.
		void resume_profile(core::host_profile_id aProfile);
		/// \brief Configures metadata persistence before any enrollment or startup restore.
		/// [[BPR-0644]]. With a renewal sink, a restored binding whose access
		/// lifetime elapsed but whose refresh lifetime remains is renewed before
		/// its session starts. The sink runs on the decision lane. The renewal
		/// asks `prepare_device_rotation`, which writes no tombstone for it, and
		/// ends when the rotated grant's enrollment names the binding's exact
		/// account.
		///
		/// The owner's rule of 2026-09-24 decides every other ending: a
		/// remembered grant is removed only when the person removes it (Sign
		/// out) or a new sign-in replaces it. So `invalidate_device_grant`, a
		/// refresh-expired binding and a composition with no sink fail the held
		/// session visibly with the failure they name and keep the grant: the
		/// row stays live, both slots stay, and the next launch tries again.
		/// A rotated grant whose `/user` names a different account is kept the
		/// same way (the owner's decision of 2026-09-24): its failure says the
		/// account differs, and the Accounts page shows it as an error until
		/// the person signs out or signs in again.
		void configure_device_persistence(device_binding_persistence& aPersistence, std::function<std::int64_t()> aWallClock, device_launch_renewal_sink aLaunchRenewal = { });
		/// \brief Loads the trusted device binding for a compiled native profile, then resumes normally.
		/// \details An admitted observer receives one terminal on the decision lane
		/// after the current turn. Ready means a trusted binding selected normal
		/// resume; session acceptance remains a separate projection. Observers and
		/// the lane must outlive quiescence. Observer/post exceptions set delivery
		/// failure; a rejected lane cannot promise delivery, but does not strand shutdown.
		/// Returns false without a callback or post when closing or all 32 slots are
		/// occupied. Slots include pending loads and queued/active terminal delivery.
		/// Optional read-only aCurrent binds startup to the coordinator request;
		/// false or throwing cancels before remembering a binding or queuing resume.
		auto restore_device_profile(github_profile_authority                           aCompiledProfile,
									std::function<void(device_profile_restore_status)> aCompletion = { },
									std::function<bool()>                              aCurrent    = { }) -> bool;
		/// \brief Ends the session whose device grant's renewal gave up, keeping the grant.
		/// \details A launch renewal fails its held session with `aFailure`
		/// ([[BPR-0644]]). An accepted session is resumed from its own binding
		/// and that session fails at once with `aFailure` ([[BPR-1116]]). Either
		/// way the grant is kept: the row stays live, both slots stay, the
		/// failure says so, and Sign out or a new sign-in removes it.
		void invalidate_device_grant(github_credential_authority_key aAuthority, core::github_identity_generation aGeneration, core::github_failure aFailure);
		/// \brief Records the exact refresh slot's successful store after access acceptance.
		void settle_device_binding(github_credential_authority_key aAuthority, core::github_identity_generation aGeneration, device_binding_lifetime aLifetime);
		/// \brief Answers once whether the exact accepted or launch-renewing grant may rotate.
		/// \details Writes no tombstone and changes no state, for a launch
		/// renewal ([[BPR-0644]]) or an accepted session ([[BPR-1116]]): the row
		/// stays live until the rotated grant's binding is stored, so a renewal
		/// that cannot finish, or a process that ends before that store, leaves
		/// the grant for the next launch.
		void               prepare_device_rotation(github_credential_authority_key aAuthority, core::github_identity_generation aGeneration, std::function<void(bool)> aCompletion);
		[[nodiscard]] auto accepted_device_lifetime() const noexcept -> std::optional<device_binding_lifetime>;
		[[nodiscard]] auto accepted_device_binding_durable() const noexcept -> bool;

		// Activates the profile signed out. No credential is read, no vault is
		// consulted, no `/user` request is issued, and no capability is
		// probed: the mode supports none of them, so nothing may look as
		// though it asked. The anonymous scope is the profile, its revision,
		// the host, the mode, and the identity generation; it carries no
		// account, not even a zero one.
		void activate_public_preview(core::host_profile_id aProfile);
		void shutdown();

		[[nodiscard]] auto state() const noexcept -> const core::github_state&;
		[[nodiscard]] auto accepted_session() const noexcept
			-> const std::optional<github_session_read_model>&;
		/// \brief [[BPR-0990]]. The accepted session's exact read authority.
		///
		/// \details What a pull request read of this session carries: the
		/// profile authority, the viewer, the capability evidence and the
		/// opaque account transport, under the session's identity generation.
		/// Absent until all of them are accepted together. For composition,
		/// which hands it to profile-wide reads such as personal discovery;
		/// no credential byte is reachable through it.
		[[nodiscard]] auto accepted_profile_session() const
			-> std::optional<github_profile_session>;
		[[nodiscard]] auto accepted_snapshot() const noexcept
			-> const std::optional<github_pull_request_snapshot>&;
		/// \brief Returns the vault authority the accepted session is keyed by.
		///
		/// \details Absent until an authenticated `/user` observation has been
		/// accepted under an exact account, which is the first moment the key
		/// exists at all: it is minted from the profile authority and the
		/// account the viewer reported, and nothing outside this host may
		/// assemble one. Absent for every public-preview session, which has no
		/// account and no vault entry. It names a slot, not a credential; no
		/// credential byte is reachable through it.
		[[nodiscard]] auto accepted_credential_authority() const noexcept
			-> std::optional<github_credential_authority_key>;
		/// \brief Returns whether the accepted credential can renew itself.
		///
		/// \details True once the vault has answered that a refresh entry sits
		/// beside the accepted credential under the same authority *and* the
		/// accepted credential's own provenance is a device grant this host
		/// enrolled or resumed. Presence alone is not the answer: both slots
		/// are keyed by the same authority, so a pasted token for an account
		/// a past grant enrolled reaches the slot that grant's material sits
		/// beside, and spending it would silently replace the reviewer's
		/// session. False for a credential the reviewer pasted, whatever the
		/// vault holds beside it; false for a grant
		/// this run's device flow is still holding refresh material for, which
		/// binds rather than resumes; and false until the vault has answered,
		/// after which the session is published again so the answer is read.
		/// No refresh byte is reachable through it: the probe destroys the
		/// material it retrieved on the vault's own callback and carries only
		/// the answer onto the decision lane.
		[[nodiscard]] auto accepted_credential_renewable() const noexcept -> bool;
		/// \brief Returns the identity generation the accepted session runs under.
		///
		/// \details Zero when no session has been accepted, and nonzero for
		/// every accepted one: the generation advances on each identity
		/// activation. It is the generation this host stamps on its own vault
		/// operations for the same authority, so a second host storing beside
		/// this credential names the same identity rather than an epoch of its
		/// own ([[BPR-0546]]). It identifies an epoch, not a secret.
		[[nodiscard]] auto accepted_identity_generation() const noexcept
			-> core::github_identity_generation;
		/// \brief Returns the opaque transport for the currently accepted account.
		///
		/// \details The capability is absent unless the host has accepted an exact
		/// profile, viewer, and pull-request snapshot under the same authority.
		/// Credential bytes and concrete adapter types remain inaccessible.
		[[nodiscard]] auto accepted_review_mutation_transport() const noexcept
			-> review_mutation_transport;
		/// \brief Returns the publication catalog for the accepted account ([[BPR-1177]]).
		///
		/// \details Absent signed out, under public preview, while closing,
		/// and when the accepted account's transport cannot carry publication.
		/// Unlike the review-mutation transport it needs no pull request
		/// snapshot: a local cycle publishes into a repository it names. The
		/// catalog is bound to exactly this account's authority and refuses any
		/// other host, account or server contract itself.
		[[nodiscard]] auto accepted_publication_account() const
			-> std::optional<github_publication_account>;
		[[nodiscard]] auto rate_limits() const noexcept
			-> const github_rate_limit_state&;
		[[nodiscard]] auto capability_cache_size() const noexcept -> std::size_t;
		[[nodiscard]] auto is_closing() const noexcept -> bool;
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;
		[[nodiscard]] auto shutdown_cleanup_failed() const noexcept -> bool;
		[[nodiscard]] auto projection_delivery_failed() const noexcept -> bool;
		[[nodiscard]] auto epoch_handoff_delivery_failed() const noexcept -> bool;
		/// \brief Returns the mode the active profile was activated in.
		[[nodiscard]] auto access_mode() const noexcept -> core::github_access_mode;
		/// \brief Returns the refusal that ended the last public-preview
		/// activation or read, if one did.
		[[nodiscard]] auto public_preview_refusal() const noexcept
			-> const std::optional<github_public_preview_refusal>&;
		/// \brief Decides the intent's legality in the current mode.
		///
		/// \details Under public preview this is the core policy's answer, so
		/// a mutation, triage, queue-role, viewed-state, or resolution intent
		/// is refused here rather than approximated by a request nobody can
		/// answer. Under the authenticated mode every intent is legal here and
		/// the capability rules decide the rest.
		[[nodiscard]] auto public_preview_legality(
			core::anonymous_read_intent aIntent) const noexcept
			-> core::anonymous_intent_legality;
		/// \brief Adopts the sink a switch-to-sign-in gesture leaves through.
		void set_public_preview_sign_in(public_preview_sign_in_sink aSignIn);
		/// \brief Forwards a switch-to-sign-in gesture to that sink.
		///
		/// \details Names the profile the signed-out session was activated
		/// for, so the code the reviewer is about to read aloud belongs to the
		/// host they are already reading, and sign-in returns them to it.
		void request_public_preview_sign_in();
		/// \brief Reports whether the last accepted snapshot handed the epoch
		/// authority nothing because the mode has no cache or epoch authority.
		[[nodiscard]] auto epoch_handoff_unavailable() const noexcept -> bool;

	private:
		struct implementation;
		std::unique_ptr<implementation> Implementation;
	};
}

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_APPLICATION_HOST_HPP
