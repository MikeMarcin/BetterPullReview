#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_APPLICATION_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_APPLICATION_HOST_HPP

#include "bpr/application/git_smart_transport_port.hpp"
#include "bpr/application/github_capability_cache.hpp"
#include "bpr/application/github_credential_vault.hpp"
#include "bpr/application/github_epoch_handoff.hpp"
#include "bpr/application/github_projection.hpp"
#include "bpr/application/github_provider_port.hpp"
#include "bpr/application/port.hpp"
#include "bpr/application/public_preview_projection.hpp"
#include "bpr/application/review_mutation_port.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/github_transition.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace bpr::application {
	inline constexpr std::size_t maximum_github_credential_bindings = 64U;

	enum class github_credential_shutdown_policy {
		retain,
		delete_retained_on_shutdown,
	};

	/// Why a public-preview activation or read was refused.
	enum class github_public_preview_refusal_cause : std::uint8_t {
		/// No anonymous read authority is composed, so the mode cannot be
		/// entered at all. This is a composition fact the host reports
		/// rather than a policy the core decided.
		public_preview_unavailable,
		/// The core policy refused. `Policy` carries its typed refusal.
		policy_refused,
	};

	/// \brief Describes a refused public-preview activation or read.
	///
	/// Sign-in is offered from every refusal, because `core::anonymous_refusal`
	/// makes that a property of the type rather than a flag a caller sets.
	struct github_public_preview_refusal
	{
		github_public_preview_refusal_cause Cause
			= github_public_preview_refusal_cause::public_preview_unavailable;
		std::optional<core::anonymous_refusal> Policy;

		auto operator<=>(const github_public_preview_refusal&) const = default;
	};

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
		[[nodiscard]] auto
			 remember(github_credential_authority_key aAuthority) noexcept -> bool;
		void forget(const github_credential_authority_key& aAuthority) noexcept;

		std::array<std::optional<github_credential_authority_key>,
				   maximum_github_credential_bindings>
			Entries{ };

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
			public_preview_projection_port* aPublicPreview = nullptr);
		~github_application_host();

		github_application_host(const github_application_host&) = delete;
		auto operator=(const github_application_host&)
			-> github_application_host&                    = delete;
		github_application_host(github_application_host&&) = delete;
		auto operator=(github_application_host&&)
			-> github_application_host& = delete;

		void dispatch(const core::github_event& aEvent);
		void enroll_profile(core::host_profile_id      aProfile,
							github_credential_material aCredential);
		// Resume asks only for a profile. The host-owned trusted binding catalog,
		// populated from prior accepted /user observations, selects the exact
		// account authority. Public account and login values never select vault
		// entries.
		void resume_profile(core::host_profile_id aProfile);
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
		/// \brief Returns the opaque transport for the currently accepted account.
		///
		/// \details The capability is absent unless the host has accepted an exact
		/// profile, viewer, and pull-request snapshot under the same authority.
		/// Credential bytes and concrete adapter types remain inaccessible.
		[[nodiscard]] auto accepted_review_mutation_transport() const noexcept
			-> review_mutation_transport;
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
