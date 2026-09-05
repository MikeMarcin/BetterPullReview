#include "bpr/application/github_application_host.hpp"

#include "github_projection_mapping.hpp"

#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <expected>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::application {
	namespace {
		void append_u64(std::vector<std::uint8_t>& aBytes,
						const std::uint64_t        aValue)
		{
			for (std::size_t shift = 8U; shift != 0U; --shift) {
				aBytes.push_back(
					static_cast<std::uint8_t>(aValue >> ((shift - 1U) * 8U)));
			}
		}

		void append_text(std::vector<std::uint8_t>& aBytes,
						 const std::string_view     aValue)
		{
			append_u64(aBytes, aValue.size());
			aBytes.insert(aBytes.end(), aValue.begin(), aValue.end());
		}

		[[nodiscard]] auto host_authority_fingerprint(
			const github_profile_authority& aProfile)
			-> core::host_authority_fingerprint
		{
			auto bytes = std::vector<std::uint8_t>{ };
			bytes.reserve(128U + aProfile.Hostname.value().size());
			append_text(bytes, "BetterPullReview/github-host-authority/v1");
			append_u64(bytes, aProfile.Profile.value());
			append_u64(bytes, aProfile.ProfileRevision.value());
			append_u64(bytes, aProfile.Host.value());
			append_text(bytes, aProfile.Hostname.value());
			append_u64(bytes, static_cast<std::uint8_t>(aProfile.ServerContract));
			append_u64(bytes, static_cast<std::uint8_t>(aProfile.Authentication));
			append_u64(bytes, aProfile.ProbeContractRevision.value());
			return core::host_authority_fingerprint{ core::sha256(bytes) };
		}

		[[nodiscard]] auto base_branch_identity(
			const core::repository_id  aRepository,
			const git_base_branch_ref& aRef) -> core::base_branch_id
		{
			auto bytes = std::vector<std::uint8_t>{ };
			bytes.reserve(96U + aRef.view().size());
			append_text(bytes, "BetterPullReview/base-branch/v1");
			append_u64(bytes, aRepository.value());
			append_text(bytes, aRef.view());
			return core::base_branch_id{ core::sha256(bytes) };
		}

		[[nodiscard]] constexpr auto epoch_review_state(
			const github_review_state aState) noexcept
			-> std::optional<core::github_review_state>
		{
			switch (aState) {
			case github_review_state::pending:
				return core::github_review_state::pending;
			case github_review_state::approved:
				return core::github_review_state::approved;
			case github_review_state::changes_requested:
				return core::github_review_state::changes_requested;
			case github_review_state::commented:
				return core::github_review_state::commented;
			case github_review_state::dismissed:
				return core::github_review_state::dismissed;
			case github_review_state::uninitialized:
				return std::nullopt;
			}
			return std::nullopt;
		}

		struct smart_authority_state final
		{
			void retain(const git_smart_transport& aTransport)
			{
				const std::scoped_lock lock{ Mutex };
				std::erase_if(Transports,
							  [](const auto& aCandidate) { return aCandidate.expired(); });
				Transports.emplace_back(aTransport);
			}

			void revoke() noexcept
			{
				if (Revoked.exchange(true, std::memory_order_acq_rel)) {
					return;
				}
				auto transports = std::vector<
					std::weak_ptr<git_smart_transport_port>>{ };
				{
					const std::scoped_lock lock{ Mutex };
					transports.swap(Transports);
				}
				for (const auto& candidate : transports) {
					if (const auto transport = candidate.lock()) {
						transport->revoke();
					}
				}
			}

			std::atomic_bool                                     Revoked = false;
			std::mutex                                           Mutex;
			std::vector<std::weak_ptr<git_smart_transport_port>> Transports;
		};

		class bound_git_smart_authority final : public git_smart_authority_lease
		{
		public:
			bound_git_smart_authority(git_smart_authority_binding            aBinding,
									  github_account_transport               aAccount,
									  std::shared_ptr<smart_authority_state> aState)
				: Binding{ std::move(aBinding) }
				, Account{ std::move(aAccount) }
				, State{ std::move(aState) }
			{
			}

			// The credential-free binding. Composition owns the anonymous
			// factory for the host's lifetime, so the lease borrows it rather
			// than sharing an account transport it must never hold.
			bound_git_smart_authority(git_smart_authority_binding            aBinding,
									  git_smart_transport_factory_port*      aAnonymous,
									  std::shared_ptr<smart_authority_state> aState)
				: Binding{ std::move(aBinding) }
				, Anonymous{ aAnonymous }
				, State{ std::move(aState) }
			{
			}

			~bound_git_smart_authority() override { revoke(); }

			[[nodiscard]] auto binding() const noexcept
				-> const git_smart_authority_binding& override
			{
				return Binding;
			}

			[[nodiscard]] auto acquire(const git_smart_request_context& aContext)
				-> git_smart_transport_result<git_smart_transport> override
			{
				if (State->Revoked.load(std::memory_order_acquire)
					|| !application::valid(aContext)
					|| aContext.ApplicationSession != Binding.ApplicationSession
					|| aContext.Authority.Profile != Binding.Profile
					|| aContext.Authority.ProfileRevision != Binding.ProfileRevision
					|| aContext.Authority.Host != Binding.Host
					|| aContext.Authority.HostAuthority != Binding.HostAuthority
					|| aContext.Authority.Account != Binding.Account
					|| aContext.Authority.Repository != Binding.Repository
					|| aContext.Repository != Binding.Locator
					|| aContext.Revision.Base.algorithm() != Binding.ObjectFormat
					|| aContext.Revision.Head.algorithm() != Binding.ObjectFormat) {
					return std::unexpected{ git_smart_transport_error{
						.Failure = git_smart_transport_failure::invalid_request } };
				}
				auto* const factory = Account == nullptr
					? Anonymous
					: dynamic_cast<git_smart_transport_factory_port*>(Account.get());
				if (factory == nullptr) {
					return std::unexpected{ git_smart_transport_error{
						.Failure = git_smart_transport_failure::unavailable } };
				}
				auto acquired = factory->acquire(aContext);
				if (!acquired) {
					return std::unexpected{ acquired.error() };
				}
				State->retain(*acquired);
				if (State->Revoked.load(std::memory_order_acquire)) {
					(*acquired)->revoke();
					return std::unexpected{ git_smart_transport_error{
						.Failure = git_smart_transport_failure::cancelled } };
				}
				return std::move(*acquired);
			}

			void revoke() noexcept override { State->revoke(); }

			[[nodiscard]] auto revoked() const noexcept -> bool override
			{
				return State->Revoked.load(std::memory_order_acquire);
			}

			[[nodiscard]] auto state() const noexcept
				-> std::shared_ptr<smart_authority_state>
			{
				return State;
			}

		private:
			git_smart_authority_binding            Binding;
			github_account_transport               Account;
			git_smart_transport_factory_port*      Anonymous = nullptr;
			std::shared_ptr<smart_authority_state> State;
		};

		[[nodiscard]] constexpr auto
		retryable(const github_provider_error& aError) noexcept -> bool
		{
			if (!valid(aError)
				|| (aError.RetryAfterSeconds.has_value()
					&& *aError.RetryAfterSeconds != 0U)
				|| is_limited(aError.RateLimits)) {
				return false;
			}
			return aError.Failure == github_provider_failure::network
				|| aError.Failure == github_provider_failure::timeout
				|| aError.Failure == github_provider_failure::unavailable;
		}

		[[nodiscard]] constexpr auto
		mapped_failure(const github_provider_failure aFailure) noexcept
			-> core::github_failure
		{
			switch (aFailure) {
			case github_provider_failure::authentication:
				return core::github_failure::authentication;
			case github_provider_failure::permission:
				return core::github_failure::permission;
			case github_provider_failure::network:
			case github_provider_failure::process_startup:
			case github_provider_failure::unavailable:
				return core::github_failure::network;
			case github_provider_failure::api_version:
				return core::github_failure::api_version;
			case github_provider_failure::unsupported_capability:
				return core::github_failure::capability;
			case github_provider_failure::rate_limit:
				return core::github_failure::rate_limit;
			case github_provider_failure::certificate:
				return core::github_failure::certificate;
			case github_provider_failure::redirect:
				return core::github_failure::redirect;
			case github_provider_failure::timeout:
				return core::github_failure::timeout;
			case github_provider_failure::cli_unavailable:
				return core::github_failure::cli_unavailable;
			case github_provider_failure::vault:
				return core::github_failure::vault;
			case github_provider_failure::malformed_response:
				return core::github_failure::malformed_response;
			case github_provider_failure::response_limit:
				return core::github_failure::response_too_large;
			}
			return core::github_failure::malformed_response;
		}

		[[nodiscard]] auto capability_key(const github_profile_authority& aProfile,
										  const core::github_account_id   aAccount)
			-> github_capability_authority_key
		{
			return {
				.Profile               = aProfile.Profile,
				.ProfileRevision       = aProfile.ProfileRevision,
				.Host                  = aProfile.Host,
				.Account               = aAccount,
				.ServerContract        = aProfile.ServerContract,
				.ProbeContractRevision = aProfile.ProbeContractRevision,
			};
		}

		[[nodiscard]] auto credential_key(const github_profile_authority& aProfile,
										  const core::github_account_id   aAccount)
			-> github_credential_authority_key
		{
			return {
				.Profile         = aProfile.Profile,
				.ProfileRevision = aProfile.ProfileRevision,
				.Host            = aProfile.Host,
				.Hostname        = aProfile.Hostname,
				.Account         = aAccount,
				.ServerContract  = aProfile.ServerContract,
			};
		}

		[[nodiscard]] constexpr auto
		valid_rate_limit_resources(const github_rate_limit_state& aState,
								   const bool                     aAllowCore,
								   const bool                     aAllowSearch,
								   const bool                     aAllowGraphql) noexcept -> bool
		{
			return valid(aState) && (aAllowCore || !aState.Core.has_value())
				&& (aAllowSearch || !aState.Search.has_value())
				&& (aAllowGraphql || !aState.Graphql.has_value());
		}

		[[nodiscard]] constexpr auto
		requires_rate_limit_observations(const github_server_contract aContract) noexcept
			-> bool
		{
			return aContract == github_server_contract::github_com_2022_11_28;
		}

		[[nodiscard]] auto
		valid_viewer(const github_profile_authority&             aAuthority,
					 const core::github_session_request_context& aContext,
					 const github_viewer_record&                 aViewer,
					 const github_rate_limit_state&              aRateLimits) noexcept -> bool
		{
			if (!valid_rate_limit_resources(aRateLimits, true, false, false)
				|| is_secondary_limited(aRateLimits)
				|| (requires_rate_limit_observations(aAuthority.ServerContract)
					&& !aRateLimits.Core.has_value())) {
				return false;
			}
			const github_session_read_model model{
				.Profile            = aAuthority.Profile,
				.Host               = aAuthority.Host,
				.Viewer             = aViewer,
				.CapabilityEvidence = {
					.Capabilities    = { },
					.Revision        = core::capability_revision{ 1U },
					.ReadShape       = { },
					.ContractVersion = 1U,
				},
				.RateLimits = aRateLimits,
			};
			return validate(model, aContext).valid();
		}

		[[nodiscard]] constexpr auto valid_capability_rate_limits(
			const github_profile_authority& aAuthority,
			const github_rate_limit_state&  aRateLimits) noexcept -> bool
		{
			return valid_rate_limit_resources(aRateLimits, false, false, true)
				&& !is_secondary_limited(aRateLimits)
				&& (!requires_rate_limit_observations(aAuthority.ServerContract)
					|| aRateLimits.Graphql.has_value());
		}

		[[nodiscard]] constexpr auto valid_snapshot_rate_limits(
			const github_profile_authority&     aAuthority,
			const github_pull_request_snapshot& aSnapshot) noexcept -> bool
		{
			if (!valid(aSnapshot.RateLimits) || is_secondary_limited(aSnapshot.RateLimits)) {
				return false;
			}
			const auto usesGraphql = aSnapshot.CapabilityEvidence.ReadShape.ReviewThreads
				|| aSnapshot.CapabilityEvidence.ReadShape.ViewedFileState;
			if (aSnapshot.RateLimits.Graphql.has_value() && !usesGraphql) {
				return false;
			}
			if (!requires_rate_limit_observations(aAuthority.ServerContract)) {
				return true;
			}
			return aSnapshot.RateLimits.Core.has_value()
				&& aSnapshot.RateLimits.Search.has_value()
				&& aSnapshot.RateLimits.Graphql.has_value() == usesGraphql;
		}

		[[nodiscard]] constexpr auto
		valid_provider_error_resources(const github_provider_error& aError,
									   const bool                   aAllowCore,
									   const bool                   aAllowSearch,
									   const bool                   aAllowGraphql) noexcept -> bool
		{
			return valid(aError)
				&& valid_rate_limit_resources(
					   aError.RateLimits, aAllowCore, aAllowSearch, aAllowGraphql);
		}

		[[nodiscard]] auto projected_capability_evidence(
			const github_capability_cache_entry& aEntry) noexcept
			-> github_capability_evidence
		{
			return {
				.Capabilities    = aEntry.Capabilities,
				.Revision        = aEntry.Revision,
				.ReadShape       = aEntry.ReadShape,
				.ContractVersion = 1U,
			};
		}

		template <typename Completion>
		[[nodiscard]] auto completion_context(const Completion& aCompletion)
		{
			return std::visit([](const auto& aTerminal) { return aTerminal.Context; },
							  aCompletion);
		}

		// [[BPR-0540]]. Whether a refresh-slot erasure left renewal material
		// in the vault. A deleted slot and an absent one are the same
		// outcome: nothing renews this credential any more. Every error is
		// the other outcome, `unavailable` included -- since BPR-0542 and
		// BPR-0543 every shipped platform has a real second slot, so an
		// `unavailable` answer means the store could not be reached, exactly
		// as it does for the base slot beside it, and material this host
		// meant to destroy may still be resident under an authority the
		// reviewer has finished with. So is a terminal for the wrong
		// operation and a cancellation this host never asks for.
		[[nodiscard]] auto erasure_left_material(
			const github_credential_delete_completion& aCompletion,
			const github_credential_operation_context& aOperation) noexcept -> bool
		{
			if (aCompletion.valueless_by_exception() || !valid(aCompletion)
				|| completion_context(aCompletion) != aOperation) {
				return true;
			}
			return !std::holds_alternative<github_credential_delete_success>(
				aCompletion);
		}

		template <typename Completion>
		[[nodiscard]] decltype(auto)
		provider_completion_request(const Completion& aCompletion)
		{
			return std::visit([](const auto& aTerminal) -> const auto& { return aTerminal.Request; },
							  aCompletion);
		}

		[[nodiscard]] auto create_host_capability_cache() -> github_capability_cache
		{
			auto cache = github_capability_cache::create({ });
			if (!cache.has_value()) {
				throw std::bad_alloc{ };
			}
			return std::move(*cache);
		}

	}

	github_credential_binding_catalog::~github_credential_binding_catalog() = default;

	auto github_credential_binding_catalog::find(
		const core::host_profile_id aProfile) const noexcept
		-> const github_credential_authority_key*
	{
		if (aProfile.value() == 0U) {
			return nullptr;
		}
		for (const auto& entry : Entries) {
			if (entry.has_value() && entry->Profile == aProfile) {
				return &*entry;
			}
		}
		return nullptr;
	}

	auto github_credential_binding_catalog::provenance(
		const core::host_profile_id aProfile) const noexcept
		-> github_credential_provenance
	{
		if (aProfile.value() == 0U) {
			return github_credential_provenance::manual;
		}
		for (std::size_t index = 0U; index < Entries.size(); ++index) {
			const auto& entry = Entries.at(index);
			if (entry.has_value() && entry->Profile == aProfile) {
				return Provenances.at(index);
			}
		}
		return github_credential_provenance::manual;
	}

	auto github_credential_binding_catalog::remember(
		const github_credential_authority_key aAuthority,
		const github_credential_provenance    aProvenance) noexcept -> bool
	{
		if (!valid(aAuthority)) {
			return false;
		}
		for (std::size_t index = 0U; index < Entries.size(); ++index) {
			auto& entry = Entries.at(index);
			if (entry.has_value() && entry->Profile == aAuthority.Profile) {
				if (*entry != aAuthority) {
					return false;
				}
				// The same slot, named again. A grant that re-enrolls over a
				// pasted token upgrades the origin; a paste over a grant
				// demotes it, which is the case this whole rule exists for.
				Provenances.at(index) = aProvenance;
				return true;
			}
		}
		for (std::size_t index = 0U; index < Entries.size(); ++index) {
			auto& entry = Entries.at(index);
			if (!entry.has_value()) {
				entry                 = aAuthority;
				Provenances.at(index) = aProvenance;
				return true;
			}
		}
		return false;
	}

	void github_credential_binding_catalog::forget(
		const github_credential_authority_key& aAuthority) noexcept
	{
		if (!valid(aAuthority)) {
			return;
		}
		for (std::size_t index = 0U; index < Entries.size(); ++index) {
			auto& entry = Entries.at(index);
			if (entry == aAuthority) {
				entry.reset();
				Provenances.at(index) = github_credential_provenance::manual;
				return;
			}
		}
	}

	auto github_credential_binding_catalog::size() const noexcept -> std::size_t
	{
		std::size_t result = 0U;
		for (const auto& entry : Entries) {
			if (entry.has_value()) {
				++result;
			}
		}
		return result;
	}

	struct github_application_host::implementation final
	{
		enum class viewer_mode : std::uint8_t {
			none,
			cli,
			bootstrap,
			account,
			// Signed out. No viewer is ever requested in this mode, so the
			// enumerator names the absence of a viewer course rather than a
			// fourth way of obtaining one.
			public_preview,
		};

		enum class vault_operation : std::uint8_t {
			none,
			store,
			retrieve,
			delete_credential,
		};

		// [[BPR-0547]]. Whether an occasion's credential delete carries the
		// refresh slot's erasure with it. Erasing is what [[BPR-0540]]
		// established and what every occasion asks for but one, so it is the
		// default and the exception is named where it is known.
		enum class refresh_disposition : std::uint8_t {
			erase,
			retain,
		};

		enum class credential_lifecycle : std::uint8_t {
			none,
			provisional,
			persistent,
		};

		struct deferred_session_terminal
		{
			core::github_session_request_context Context;
			std::optional<core::github_failure>  Failure;
		};

		struct session_candidate
		{
			core::github_session_request_context         Context;
			github_profile_authority                     ProfileAuthority;
			github_viewer_record                         Viewer;
			github_capability_cache_entry                CapabilityEvidence;
			github_account_transport                     AccountTransport;
			github_rate_limit_state                      RateLimits;
			std::optional<github_capability_cache_stage> CacheStage;
			core::github_access_mode                     Mode{ };
		};

		struct deferred_dispatch
		{
			core::github_event Event;
		};

		struct deferred_enrollment
		{
			core::host_profile_id        Profile;
			github_credential_material   Credential;
			github_credential_provenance Provenance
				= github_credential_provenance::manual;
		};

		struct deferred_resume
		{
			core::host_profile_id Profile;
		};

		struct deferred_public_preview
		{
			core::host_profile_id Profile;
		};

		struct deferred_shutdown
		{
		};

		using deferred_ingress = std::variant<deferred_dispatch,
											  deferred_enrollment,
											  deferred_resume,
											  deferred_public_preview,
											  deferred_shutdown>;

		using pending_provider_request = std::variant<std::monostate,
													  github_profile_request,
													  github_viewer_request,
													  github_capability_request,
													  github_pull_request_provider_request>;

		struct completion_gate
		{
			std::recursive_mutex         Barrier;
			std::atomic<implementation*> Host = nullptr;
		};

		implementation(core::github_state                 aInitialState,
					   github_profile_port&               aProfiles,
					   github_viewer_port&                aViewers,
					   github_capability_port&            aCapabilities,
					   github_pull_request_port&          aPullRequests,
					   github_credential_vault_port&      aCredentials,
					   github_credential_binding_catalog& aCredentialBindings,
					   decision_executor&                 aDecisionLane,
					   github_projection_port&            aProjections,
					   const github_credential_shutdown_policy
														 aCredentialShutdownPolicy,
					   github_epoch_handoff_port* const  aEpochs,
					   github_pull_request_port* const   aAnonymousPullRequests,
					   git_smart_transport_factory_port* aAnonymousSmartGit,
					   public_preview_projection_port*   aPublicPreview)
			: State{ std::move(aInitialState) }
			, Profiles{ aProfiles }
			, Viewers{ aViewers }
			, Capabilities{ aCapabilities }
			, PullRequests{ aPullRequests }
			, Credentials{ aCredentials }
			, CredentialBindings{ aCredentialBindings }
			, CapabilityCache{ create_host_capability_cache() }
			, DecisionLane{ aDecisionLane }
			, Projections{ aProjections }
			, CredentialShutdownPolicy{ aCredentialShutdownPolicy }
			, Epochs{ aEpochs }
			, AnonymousPullRequests{ aAnonymousPullRequests }
			, AnonymousSmartGit{ aAnonymousSmartGit }
			, PublicPreview{ aPublicPreview }
			, CompletionGate{ std::make_shared<completion_gate>() }
		{
			CompletionGate->Host.store(this, std::memory_order_release);
		}

		~implementation()
		{
			const std::lock_guard barrier{ CompletionGate->Barrier };
			CompletionGate->Host.store(nullptr, std::memory_order_release);
		}

		template <typename Action>
		void run_turn(Action&& aAction)
		{
			TurnActive = true;
			try {
				std::forward<Action>(aAction)();
				const auto deferredCount = DeferredIngress.size();
				for (std::size_t index = 0U; index < deferredCount; ++index) {
					auto deferred = std::move(DeferredIngress.front());
					DeferredIngress.pop_front();
					std::visit(
						[this](auto&& aIngress) {
							using ingress_type = std::decay_t<decltype(aIngress)>;
							if constexpr (std::is_same_v<ingress_type,
														 deferred_dispatch>) {
								dispatch_now(aIngress.Event);
							} else if constexpr (std::is_same_v<ingress_type,
																deferred_enrollment>) {
								enroll_profile_now(aIngress.Profile,
												   std::move(aIngress.Credential),
												   aIngress.Provenance);
							} else if constexpr (std::is_same_v<ingress_type,
																deferred_resume>) {
								resume_profile_now(aIngress.Profile);
							} else if constexpr (std::is_same_v<ingress_type,
																deferred_public_preview>) {
								activate_public_preview_now(aIngress.Profile);
							} else {
								shutdown_now();
							}
						},
						std::move(deferred));
				}
				TurnActive = false;
				schedule_deferred_ingress();
			} catch (...) {
				DeferredIngress.clear();
				TurnActive = false;
				throw;
			}
		}

		void schedule_deferred_ingress() noexcept
		{
			if (DeferredIngress.empty() || DeferredIngressScheduled) {
				return;
			}
			DeferredIngressScheduled = true;
			const auto gate          = std::weak_ptr<completion_gate>{ CompletionGate };
			try {
				DecisionLane.post(
					[gate] {
						const auto locked = gate.lock();
						if (!locked) {
							return;
						}
						const std::lock_guard completionBarrier{ locked->Barrier };
						auto* const           host = locked->Host.load(std::memory_order_acquire);
						if (host == nullptr) {
							return;
						}
						host->DeferredIngressScheduled = false;
						host->run_turn([] { });
					});
			} catch (...) {
				DeferredIngressScheduled = false;
				DeferredIngress.clear();
				ProjectionDeliveryFailed = true;
			}
		}

		void dispatch(const core::github_event& aEvent)
		{
			if (Closing) {
				return;
			}
			if (TurnActive) {
				DeferredIngress.emplace_back(deferred_dispatch{ aEvent });
				return;
			}
			if (DeferredIngressScheduled || !DeferredIngress.empty()) {
				DeferredIngress.emplace_back(deferred_dispatch{ aEvent });
				schedule_deferred_ingress();
				return;
			}
			run_turn([this, &aEvent] { dispatch_now(aEvent); });
		}

		void dispatch_now(const core::github_event& aEvent)
		{
			if (Closing) {
				return;
			}
			const auto intent = std::holds_alternative<core::activate_host_profile_intent>(aEvent)
				|| std::holds_alternative<core::deactivate_github_identity_intent>(
									aEvent)
				|| std::holds_alternative<core::open_pull_request_intent>(aEvent)
				|| std::holds_alternative<core::refresh_pull_request_intent>(aEvent);
			if (!intent) {
				return;
			}
			if (std::holds_alternative<core::activate_host_profile_intent>(aEvent)) {
				NextEnrollment.reset();
				NextResumeProfile.reset();
			}
			dispatch_core(aEvent);
		}

		void enroll_profile(const core::host_profile_id        aProfile,
							github_credential_material         aCredential,
							const github_credential_provenance aProvenance)
		{
			if (Closing) {
				return;
			}
			if (TurnActive) {
				DeferredIngress.emplace_back(deferred_enrollment{
					.Profile    = aProfile,
					.Credential = std::move(aCredential),
					.Provenance = aProvenance,
				});
				return;
			}
			if (DeferredIngressScheduled || !DeferredIngress.empty()) {
				DeferredIngress.emplace_back(deferred_enrollment{
					.Profile    = aProfile,
					.Credential = std::move(aCredential),
					.Provenance = aProvenance,
				});
				schedule_deferred_ingress();
				return;
			}
			run_turn([this, aProfile, aProvenance, credential = std::move(aCredential)]() mutable { enroll_profile_now(aProfile, std::move(credential), aProvenance); });
		}

		void enroll_profile_now(const core::host_profile_id        aProfile,
								github_credential_material         aCredential,
								const github_credential_provenance aProvenance)
		{
			if (Closing) {
				return;
			}
			NextEnrollment.reset();
			NextEnrollmentProvenance = github_credential_provenance::manual;
			NextEnrollmentProfile.reset();
			NextResumeProfile.reset();
			if (valid(aCredential)) {
				NextEnrollment.emplace(std::move(aCredential));
				NextEnrollmentProvenance = aProvenance;
				NextEnrollmentProfile    = aProfile;
			}
			try {
				dispatch_core(
					core::activate_host_profile_intent{ .Profile = aProfile });
			} catch (...) {
				NextEnrollment.reset();
				NextEnrollmentProvenance = github_credential_provenance::manual;
				NextEnrollmentProfile.reset();
				throw;
			}
			// A valid activation synchronously moved this material into the
			// session scratch state. An invalid activation emitted no request, so
			// retaining it here would let a later activation consume the wrong
			// enrollment secret.
			NextEnrollment.reset();
			NextEnrollmentProvenance = github_credential_provenance::manual;
			NextEnrollmentProfile.reset();
		}

		void resume_profile(const core::host_profile_id aProfile)
		{
			if (Closing) {
				return;
			}
			if (TurnActive) {
				DeferredIngress.emplace_back(deferred_resume{
					.Profile = aProfile,
				});
				return;
			}
			if (DeferredIngressScheduled || !DeferredIngress.empty()) {
				DeferredIngress.emplace_back(deferred_resume{
					.Profile = aProfile,
				});
				schedule_deferred_ingress();
				return;
			}
			run_turn([this, aProfile] { resume_profile_now(aProfile); });
		}

		void resume_profile_now(const core::host_profile_id aProfile)
		{
			if (Closing || aProfile.value() == 0U) {
				return;
			}
			NextEnrollment.reset();
			NextResumeProfile = aProfile;
			try {
				dispatch_core(core::activate_host_profile_intent{
					.Profile = aProfile,
				});
			} catch (...) {
				NextResumeProfile.reset();
				throw;
			}
			NextResumeProfile.reset();
		}

		void activate_public_preview(const core::host_profile_id aProfile)
		{
			if (Closing) {
				return;
			}
			if (TurnActive) {
				DeferredIngress.emplace_back(deferred_public_preview{
					.Profile = aProfile,
				});
				return;
			}
			if (DeferredIngressScheduled || !DeferredIngress.empty()) {
				DeferredIngress.emplace_back(deferred_public_preview{
					.Profile = aProfile,
				});
				schedule_deferred_ingress();
				return;
			}
			run_turn([this, aProfile] { activate_public_preview_now(aProfile); });
		}

		void activate_public_preview_now(const core::host_profile_id aProfile)
		{
			if (Closing || aProfile.value() == 0U) {
				return;
			}
			PublicPreviewRefusal.reset();
			PublicPreviewProfile = aProfile;
			if (AnonymousPullRequests == nullptr) {
				// Composition supplied no credential-free read authority, so
				// the mode cannot be entered. Refuse before the core is asked
				// anything: no identity generation is abandoned, no profile is
				// resolved, and nothing observable was attempted.
				PublicPreviewRefusal = github_public_preview_refusal{
					.Cause = github_public_preview_refusal_cause::
						public_preview_unavailable,
					.Policy = std::nullopt,
				};
				publish_public_preview();
				return;
			}
			// A signed-out activation carries no material of any kind. Any
			// enrollment or resume left staged by an earlier turn is dropped
			// here rather than consumed by a session that has no account to
			// bind it to.
			NextEnrollment.reset();
			NextResumeProfile.reset();
			NextPublicPreview = true;
			try {
				dispatch_core(core::activate_host_profile_intent{
					.Profile = aProfile,
					.Mode    = core::github_access_mode::public_preview,
				});
			} catch (...) {
				NextPublicPreview = false;
				throw;
			}
			NextPublicPreview = false;
		}

		[[nodiscard]] auto signed_out_mode() const noexcept -> bool
		{
			return State.AccessMode
				== core::github_access_mode::public_preview;
		}

		[[nodiscard]] auto is_public_preview() const noexcept -> bool
		{
			return AcceptedSession.has_value()
				&& AcceptedSession->Mode
				== core::github_access_mode::public_preview;
		}

		[[nodiscard]] auto anonymous_capability_evidence(
			const github_profile_authority& aAuthority) const noexcept
			-> github_capability_cache_entry
		{
			// Synthesised, never observed and never cached. Every capability
			// is unsupported by policy rather than unknown, and the read shape
			// claims nothing, so no later reader can mistake this for evidence
			// a probe produced. The authority names no account, so this entry
			// can never be a valid capability-cache key.
			github_capability_cache_entry entry{ };
			entry.Authority = github_capability_authority_key{
				.Profile               = aAuthority.Profile,
				.ProfileRevision       = aAuthority.ProfileRevision,
				.Host                  = aAuthority.Host,
				.Account               = { },
				.ServerContract        = aAuthority.ServerContract,
				.ProbeContractRevision = aAuthority.ProbeContractRevision,
			};
			entry.Capabilities = core::anonymous_capabilities();
			entry.ReadShape    = { };
			entry.Revision     = core::capability_revision{ 1U };
			return entry;
		}

		void begin_public_preview(
			const core::github_session_request_context& aContext,
			const github_profile_authority&             aAuthority)
		{
			if (aAuthority.ServerContract
				!= github_server_contract::github_com_2022_11_28) {
				// Public preview is limited to the GitHub.com contract. Refuse
				// GHES before constructing an anonymous request.
				PublicPreviewRefusal = github_public_preview_refusal{
					.Cause  = github_public_preview_refusal_cause::policy_refused,
					.Policy = core::refuse(
						core::anonymous_refusal_reason::ghes_unavailable),
				};
				fail_session(aContext, core::github_failure::capability);
				return;
			}
			if (AnonymousPullRequests == nullptr) {
				PublicPreviewRefusal = github_public_preview_refusal{
					.Cause = github_public_preview_refusal_cause::
						public_preview_unavailable,
					.Policy = std::nullopt,
				};
				fail_session(aContext, core::github_failure::capability);
				return;
			}
			ViewerMode = viewer_mode::public_preview;
			accept_session(session_candidate{
				.Context            = aContext,
				.ProfileAuthority   = aAuthority,
				.Viewer             = { },
				.CapabilityEvidence = anonymous_capability_evidence(aAuthority),
				.AccountTransport   = { },
				.RateLimits         = RateLimits,
				.CacheStage         = std::nullopt,
				.Mode               = core::github_access_mode::public_preview,
			});
		}

		[[nodiscard]] auto make_epoch_handoff(
			const github_pull_request_snapshot& aSnapshot)
			-> std::optional<accepted_pull_request_handoff>
		{
			if (!AcceptedSession || !AcceptedProfileAuthority
				|| aSnapshot.Profile != AcceptedProfileAuthority->Profile
				|| aSnapshot.Host != AcceptedProfileAuthority->Host
				|| aSnapshot.Account != AcceptedSession->Viewer.Account
				|| aSnapshot.Viewer.Actor != AcceptedSession->Viewer.Node) {
				return std::nullopt;
			}

			const auto base = core::parse_git_commit_oid(
				aSnapshot.PullRequest.BaseOid.value());
			const auto head = core::parse_git_commit_oid(
				aSnapshot.PullRequest.HeadOid.value());
			auto baseRefText = std::string{ "refs/heads/" };
			baseRefText += aSnapshot.PullRequest.BaseRef;
			const auto baseRef = parse_git_base_branch_ref(baseRefText);
			if (!base || !head || !baseRef
				|| base->algorithm() != head->algorithm()) {
				return std::nullopt;
			}

			// A signed-out read has no reader, so the handoff names no account
			// and no viewer. The zero here is the authority's discriminant,
			// not a missing value: `core::pull_request_authority` refuses a
			// public-preview authority that carries either.
			const auto anonymous = aSnapshot.Mode
				== core::github_access_mode::public_preview;
			const auto viewer        = anonymous
				? core::github_viewer_actor_id{ }
				: core::github_viewer_actor_id{ aSnapshot.Account.value() };
			auto       currentReview = std::optional<core::current_viewer_review_candidate>{ };
			for (std::size_t index = 0U; !anonymous && index < aSnapshot.Reviews.size();
				 ++index) {
				const auto& review = aSnapshot.Reviews[index];
				const auto  state  = epoch_review_state(review.State);
				const auto  commit = review.Commit
					? core::parse_git_commit_oid(review.Commit->value())
					: std::nullopt;
				if (review.DatabaseId == 0U || !review.Author || !state || !commit
					|| review.Author->Actor != aSnapshot.Viewer.Actor
					|| *state == core::github_review_state::pending
					|| commit->algorithm() != base->algorithm()
					|| index >= core::maximum_github_review_order) {
					continue;
				}
				currentReview = core::current_viewer_review_candidate{
					.Review = core::github_review_id{ review.DatabaseId },
					.Actor  = viewer,
					.Order  = core::github_review_order{
						static_cast<std::uint16_t>(index + 1U) },
					.State  = *state,
					.Commit = *commit,
				};
			}

			auto handoff = accepted_pull_request_handoff{
				.ApplicationSession = State.ApplicationSession,
				.Profile            = aSnapshot.Profile,
				.ProfileRevision    = core::github_profile_revision{
					AcceptedProfileAuthority->ProfileRevision.value() },
				.Host           = aSnapshot.Host,
				.HostAuthority  = host_authority_fingerprint(*AcceptedProfileAuthority),
				.Hostname       = AcceptedProfileAuthority->Hostname,
				.ServerContract = AcceptedProfileAuthority->ServerContract,
				.Account        = aSnapshot.Account,
				.Viewer         = viewer,
				.Repository     = aSnapshot.Repository.Repository,
				.Locator        = {
					.Owner = aSnapshot.Repository.Owner,
					.Name  = aSnapshot.Repository.Name,
				},
				.PullRequest           = aSnapshot.PullRequest.PullRequest,
				.Number                = aSnapshot.PullRequest.Number,
				.Snapshot              = aSnapshot.Snapshot,
				.IdentityGeneration    = State.IdentityGeneration,
				.PullRequestGeneration = State.PullRequestGeneration,
				.Origin                = core::pull_request_revision_origin::live,
				.ObjectFormat          = base->algorithm(),
				.BaseBranch            = base_branch_identity(aSnapshot.Repository.Repository, *baseRef),
				.BaseRef               = *baseRef,
				.Base                  = *base,
				.Head                  = *head,
				.Mode                  = aSnapshot.Mode,
				.CurrentViewerReview   = std::move(currentReview),
			};

			if (anonymous) {
				// The anonymous authority reaches Git only through the
				// credential-free transport composition supplied. Without it
				// there is no repository to materialize, so the caller keeps
				// the handoff unavailable rather than sending an authority
				// that could not fetch.
				if (AnonymousSmartGit != nullptr) {
					auto state       = std::make_shared<smart_authority_state>();
					handoff.SmartGit = std::make_unique<bound_git_smart_authority>(
						authority_binding(handoff), AnonymousSmartGit, state);
				}
			} else if (dynamic_cast<git_smart_transport_factory_port*>(
						   AccountTransport.get())
					   != nullptr) {
				auto state       = std::make_shared<smart_authority_state>();
				handoff.SmartGit = std::make_unique<bound_git_smart_authority>(
					authority_binding(handoff), AccountTransport, state);
			}
			return valid(handoff) ? std::optional{ std::move(handoff) }
								  : std::nullopt;
		}

		void revoke_review_mutation_transport() noexcept
		{
			if (ReviewMutationTransport != nullptr) {
				ReviewMutationTransport->revoke();
				ReviewMutationTransport.reset();
			}
		}

		void renew_review_mutation_transport() noexcept
		{
			revoke_review_mutation_transport();
			if (AccountTransport == nullptr) {
				return;
			}
			auto* const factory = dynamic_cast<review_mutation_transport_factory_port*>(
				AccountTransport.get());
			if (factory == nullptr) {
				return;
			}
			ReviewMutationTransport = factory->acquire_review_mutation_transport();
			if (ReviewMutationTransport != nullptr
				&& !ReviewMutationTransport->active()) {
				ReviewMutationTransport.reset();
			}
		}

		[[nodiscard]] static auto invalidation_for(
			const accepted_pull_request_handoff& aHandoff)
			-> pull_request_authority_invalidation
		{
			return {
				.ApplicationSession = aHandoff.ApplicationSession,
				.Profile            = aHandoff.Profile,
				.ProfileRevision    = aHandoff.ProfileRevision,
				.Host               = aHandoff.Host,
				.HostAuthority      = aHandoff.HostAuthority,
				.Account            = aHandoff.Account,
				.Viewer             = aHandoff.Viewer,
				.Repository         = aHandoff.Repository,
				.PullRequest        = aHandoff.PullRequest,
				.IdentityGeneration = aHandoff.IdentityGeneration,
				.Mode               = aHandoff.Mode,
			};
		}

		void invalidate_epoch_authority() noexcept
		{
			if (ActiveEpochSmartAuthority) {
				ActiveEpochSmartAuthority->revoke();
			}
			if (Epochs && ActiveEpochAuthority) {
				try {
					Epochs->invalidate(*ActiveEpochAuthority);
				} catch (...) {
					EpochHandoffDeliveryFailed = true;
				}
			}
			ActiveEpochSmartAuthority.reset();
			ActiveEpochAuthority.reset();
		}

		void deliver_epoch_handoff(
			const github_pull_request_snapshot& aSnapshot) noexcept
		{
			if (Epochs == nullptr) {
				return;
			}
			EpochHandoffUnavailable = false;
			if (is_public_preview() && AnonymousSmartGit == nullptr) {
				// Composition supplied no credential-free Git authority, so
				// the signed-out mode opens no catalog and no cache session:
				// it reports the unavailable state and sends nothing. Sending
				// nothing here is the designed answer, not a delivery that
				// failed.
				invalidate_epoch_authority();
				EpochHandoffUnavailable = true;
				return;
			}
			try {
				auto handoff = make_epoch_handoff(aSnapshot);
				if (!handoff) {
					EpochHandoffDeliveryFailed = true;
					return;
				}
				const auto authority = invalidation_for(*handoff);
				if (ActiveEpochAuthority && *ActiveEpochAuthority != authority) {
					invalidate_epoch_authority();
				}
				auto smartState = std::shared_ptr<smart_authority_state>{ };
				if (auto* lease = dynamic_cast<bound_git_smart_authority*>(
						handoff->SmartGit.get())) {
					smartState = lease->state();
				}
				Epochs->accept(std::move(*handoff));
				ActiveEpochAuthority      = authority;
				ActiveEpochSmartAuthority = std::move(smartState);
			} catch (...) {
				EpochHandoffDeliveryFailed = true;
			}
		}

		void dispatch_core(const core::github_event& aEvent)
		{
			auto output = core::transition(State, aEvent);
			for (const auto& request : output.Requests) {
				const auto* fetch = std::get_if<core::fetch_pull_request_request>(&request);
				if (fetch != nullptr
					&& !fetch->Context.ExpectedRepository.has_value()
					&& !fetch->Context.ExpectedPullRequest.has_value()) {
					invalidate_epoch_authority();
					StagedSnapshot.reset();
					revoke_review_mutation_transport();
					AcceptedSnapshot.reset();
				}
			}
			publish_and_execute(output);
		}

		void publish_and_execute(const core::github_transition_output& aOutput)
		{
			if (Closing) {
				return;
			}
			publish_transition(aOutput);
			for (const auto& request : aOutput.Requests) {
				execute(request);
			}
		}

		void publish_transition(
			const core::github_transition_output&     aOutput,
			const github_session_read_model* const    aSession     = nullptr,
			const github_pull_request_snapshot* const aPullRequest = nullptr) noexcept
		{
			try {
				std::optional<github_session_projection> sessionProjection;
				if (aSession != nullptr) {
					sessionProjection = project_github_session(*aSession);
				}
				std::optional<github_pull_request_projection>
					pullRequestProjection;
				if (aPullRequest != nullptr) {
					pullRequestProjection = project_github_pull_request(*aPullRequest);
				}
				const github_presentation_update update{
					.Transition      = project_github_transition(aOutput),
					.Session         = std::move(sessionProjection),
					.PullRequest     = std::move(pullRequestProjection),
					.HostDiagnostics = { },
				};
				Projections.publish(update);
			} catch (...) {
				ProjectionDeliveryFailed = true;
			}
			publish_public_preview();
		}

		void publish_diagnostic(const github_host_diagnostic& aDiagnostic) noexcept
		{
			try {
				const github_presentation_update update{
					.Transition      = std::nullopt,
					.Session         = std::nullopt,
					.PullRequest     = std::nullopt,
					.HostDiagnostics = { project_github_host_diagnostic(aDiagnostic) },
				};
				Projections.publish(update);
			} catch (...) {
				ProjectionDeliveryFailed = true;
			}
			publish_public_preview();
		}

		[[nodiscard]] static auto projected_refusal_reason(
			const github_public_preview_refusal& aRefusal) noexcept
			-> public_preview_refusal_reason
		{
			if (aRefusal.Cause
					!= github_public_preview_refusal_cause::policy_refused
				|| !aRefusal.Policy.has_value()) {
				return public_preview_refusal_reason::mode_unavailable;
			}
			switch (aRefusal.Policy->Reason) {
			case core::anonymous_refusal_reason::rate_limited:
				return public_preview_refusal_reason::budget_exhausted;
			case core::anonymous_refusal_reason::private_or_missing:
				return public_preview_refusal_reason::private_or_missing;
			case core::anonymous_refusal_reason::ghes_unavailable:
				return public_preview_refusal_reason::enterprise_host_unavailable;
			case core::anonymous_refusal_reason::graphql_refused:
				return public_preview_refusal_reason::review_threads_unavailable;
			case core::anonymous_refusal_reason::viewer_identity_refused:
				return public_preview_refusal_reason::viewer_identity_unavailable;
			case core::anonymous_refusal_reason::mutation_refused:
				break;
			}
			return public_preview_refusal_reason::triage_or_publishing_refused;
		}

		[[nodiscard]] static auto project_review_comment(
			const github_review_comment_record& aComment)
			-> public_preview_review_comment
		{
			return public_preview_review_comment{
				.Id          = std::string{ aComment.Comment.value() },
				.AuthorLogin = aComment.Author.has_value()
					? aComment.Author->Login
					: std::string{ },
				.Body        = aComment.Body,
				.Path        = aComment.Path,
				.Line        = aComment.Line,
			};
		}

		[[nodiscard]] static auto project_review_comment_group(
			const github_review_comment_group_record& aGroup)
			-> public_preview_review_comment_group
		{
			public_preview_review_comment_group group{
				.Root    = project_review_comment(aGroup.Root),
				.Replies = { },
			};
			group.Replies.reserve(aGroup.Replies.size());
			for (const auto& reply : aGroup.Replies) {
				group.Replies.push_back(project_review_comment(reply));
			}
			return group;
		}

		[[nodiscard]] auto project_public_preview() const
			-> public_preview_presentation
		{
			public_preview_presentation presentation{ };
			presentation.Mode = signed_out_mode()
				? public_preview_mode::signed_out
				: public_preview_mode::signed_in;
			if (RateLimits.Core.has_value()) {
				presentation.Budget = public_preview_budget{
					.Limit            = RateLimits.Core->Limit,
					.Remaining        = RateLimits.Core->Remaining,
					.ResetUnixSeconds = RateLimits.Core->ResetUnixSeconds,
				};
			}
			if (PublicPreviewRefusal.has_value()) {
				auto refusal = public_preview_refusal_presentation{
					.Reason = projected_refusal_reason(*PublicPreviewRefusal),
					.Budget = std::nullopt,
				};
				if (PublicPreviewRefusal->Policy.has_value()
					&& PublicPreviewRefusal->Policy->Budget.has_value()) {
					const auto& fact = *PublicPreviewRefusal->Policy->Budget;
					refusal.Budget   = public_preview_budget{
						.Limit            = fact.Limit,
						.Remaining        = 0U,
						.ResetUnixSeconds = fact.Reset,
					};
				}
				presentation.Refusal = refusal;
			}
			if (AcceptedSnapshot.has_value()
				&& AcceptedSnapshot->ReviewCommentsAvailability
					== github_collection_availability::available) {
				presentation.ReviewComments
					= public_preview_observation::observed;
				presentation.ReviewCommentGroups.reserve(
					AcceptedSnapshot->ReviewCommentGroups.size());
				for (const auto& group : AcceptedSnapshot->ReviewCommentGroups) {
					presentation.ReviewCommentGroups.push_back(
						project_review_comment_group(group));
				}
			}
			return presentation;
		}

		// Publication is by change on the four occasions the port promises:
		// activation, every budget observation, every refusal, and
		// invalidation. Each of those runs through a transition, a diagnostic,
		// an observed provider error, or the explicit call in `invalidate`.
		void publish_public_preview() noexcept
		{
			if (PublicPreview == nullptr) {
				return;
			}
			try {
				auto presentation = project_public_preview();
				if (PublishedPublicPreview.has_value()
					&& *PublishedPublicPreview == presentation) {
					return;
				}
				PublishedPublicPreview = presentation;
				PublicPreview->publish(presentation);
			} catch (...) {
				ProjectionDeliveryFailed = true;
			}
		}

		void observe_provider_error(const github_provider_error& aError)
		{
			if (valid(aError)) {
				static_cast<void>(merge(RateLimits, aError.RateLimits));
				publish_public_preview();
			}
		}

		void publish_error_advisory(const github_provider_error& aError)
		{
			if (valid(aError)
				&& (aError.RetryAfterSeconds.has_value()
					|| has_observations(aError.RateLimits))) {
				publish_diagnostic(github_host_diagnostic{
					.Reason            = core::reason_for(mapped_failure(aError.Failure)),
					.RetryAfterSeconds = aError.RetryAfterSeconds,
					.RateLimits        = RateLimits,
					.RateLimitCause    = aError.RateLimitCause,
				});
			}
		}

		[[nodiscard]] auto requires_shutdown_credential_cleanup() const noexcept
			-> bool
		{
			if (!CredentialAuthority.has_value()
				|| !CredentialSessionContext.has_value()) {
				return false;
			}
			return CredentialLifecycle == credential_lifecycle::provisional
				|| CredentialShutdownPolicy
				== github_credential_shutdown_policy::
					delete_retained_on_shutdown;
		}

		void execute(const core::github_host_request& aRequest)
		{
			std::visit(
				[this](const auto& aTypedRequest) {
					using request_type = std::decay_t<decltype(aTypedRequest)>;
					if constexpr (std::is_same_v<
									  request_type,
									  core::invalidate_github_identity_request>) {
						invalidate(aTypedRequest);
					} else if constexpr (std::is_same_v<
											 request_type,
											 core::establish_github_session_request>) {
						DeferredSession             = aTypedRequest.Context;
						SessionEnrollment           = std::move(NextEnrollment);
						SessionEnrollmentProvenance = NextEnrollmentProvenance;
						NextEnrollment.reset();
						NextEnrollmentProvenance = github_credential_provenance::manual;
						NextEnrollmentProfile.reset();
						SessionResumeProfile = NextResumeProfile;
						NextResumeProfile.reset();
						SessionPublicPreview = NextPublicPreview;
						NextPublicPreview    = false;
						if (ActiveVaultOperation == vault_operation::none) {
							start_deferred_session();
						}
					} else {
						start_pull_request(aTypedRequest.Context);
					}
				},
				aRequest);
		}

		void invalidate(const core::invalidate_github_identity_request& aRequest)
		{
			invalidate_epoch_authority();
			cancel_provider();
			// The answer described the identity being invalidated. Keeping it
			// would let a renewal be armed under a key this host has already
			// let go of.
			cancel_refresh_probe();
			// [[BPR-0549]]. A retention window still open at an invalidate
			// belongs to a session this invalidate is discarding before it
			// ever named an authority, so nothing is coming to spend the slot
			// it holds. The window this invalidate may open for itself is
			// opened later, when its own credential delete answers.
			erase_retained_refresh();
			const auto* resumeBinding          = NextResumeProfile.has_value()
				? CredentialBindings.find(*NextResumeProfile)
				: nullptr;
			const auto  preserveForExactResume = CredentialLifecycle == credential_lifecycle::persistent
				&& CredentialAuthority.has_value()
				&& CredentialSessionContext.has_value() && resumeBinding != nullptr
				&& *resumeBinding == *CredentialAuthority;
			// [[BPR-0547]]. The one occasion whose credential delete must not
			// carry the refresh slot with it. A renewal ends by storing the
			// rotated refresh material and then enrolling the rotated access
			// token through the device-grant enrollment boundary; that
			// enrollment activates the profile, and this invalidate is the
			// activation's other half. Erasing here destroys the slot the
			// rotation filled a moment earlier on this same lane, and the next
			// renewal answers `refresh_absent` -- renewal works exactly once.
			//
			// Retaining is not orphaning, which is the test [[BPR-0540]] set.
			// The enrollment superseding this session is itself a device grant
			// for the profile the slot is keyed under, so the session about to
			// be established binds that same authority and the renewal that
			// spends the slot is armed on it. Every other occasion still
			// erases: an orderly shutdown, a sign-out, an enrollment from any
			// other source, and a grant for another profile all end with
			// nobody able to spend what the slot holds.
			//
			// [[BPR-0549]] narrows that premise, which is only provisional
			// here: the incoming enrollment's profile is known at this point
			// and its account is not, so a grant for a different account on
			// this same profile satisfies every condition below. The slot is
			// held rather than kept, and the bootstrap `/user` that names the
			// incoming account either confirms this or erases it then.
			const auto supersededByItsOwnRenewal = NextEnrollment.has_value()
				&& NextEnrollmentProvenance
					== github_credential_provenance::device_grant
				&& CredentialProvenance
					== github_credential_provenance::device_grant
				&& CredentialAuthority.has_value()
				&& NextEnrollmentProfile.has_value()
				&& *NextEnrollmentProfile == CredentialAuthority->Profile;
			const auto invalidationRefresh   = supersededByItsOwnRenewal
				? refresh_disposition::retain
				: refresh_disposition::erase;
			const auto vaultOperationRunning = ActiveVaultOperation == vault_operation::store
				|| ActiveVaultOperation == vault_operation::retrieve;
			if (vaultOperationRunning) {
				if (ActiveVaultContext.has_value()) {
					try {
						Credentials.cancel(*ActiveVaultContext);
					} catch (...) {
					}
				}
				DeferredInvalidationGeneration          = aRequest.NewGeneration;
				DeferredInvalidationPreservesCredential = preserveForExactResume;
				DeferredInvalidationRetainsRefresh
					= invalidationRefresh == refresh_disposition::retain;
			} else {
				cancel_vault();
				DeferredInvalidationGeneration.reset();
				DeferredInvalidationPreservesCredential = false;
				DeferredInvalidationRetainsRefresh      = false;
			}
			StagedSession.reset();
			StagedSnapshot.reset();
			revoke_review_mutation_transport();
			AcceptedSnapshot.reset();
			RateLimits = { };
			AccountTransport.reset();
			CurrentProfile.reset();
			BootstrapViewer.reset();
			SessionEnrollment.reset();
			SessionEnrollmentProvenance = github_credential_provenance::manual;
			SessionResumeProfile.reset();
			SessionPublicPreview = false;
			PublicPreviewRefusal.reset();
			EpochHandoffUnavailable = false;
			ViewerMode              = viewer_mode::none;
			DeferredTerminal.reset();
			DeferredSession.reset();
			publish_public_preview();

			if (aRequest.Previous.Profile.has_value()
				&& aRequest.Previous.Host.has_value()
				&& aRequest.Previous.Account.has_value()) {
				static_cast<void>(
					CapabilityCache.invalidate_identity(*aRequest.Previous.Profile,
														*aRequest.Previous.Host,
														*aRequest.Previous.Account));
			} else if (aRequest.Previous.Profile.has_value()) {
				static_cast<void>(
					CapabilityCache.invalidate_profile(*aRequest.Previous.Profile));
			}

			AcceptedSession.reset();
			AcceptedProfileAuthority.reset();
			AcceptedCapabilityEvidence.reset();
			if (CredentialAuthority.has_value()
				&& CredentialSessionContext.has_value()) {
				if (!vaultOperationRunning && !preserveForExactResume) {
					start_delete(aRequest.NewGeneration,
								 std::nullopt,
								 invalidationRefresh);
				}
			}
		}

		void select_persistent_credential(
			const github_credential_authority_key&      aAuthority,
			const core::github_session_request_context& aContext)
		{
			resolve_retained_refresh(aAuthority);
			CredentialAuthority      = aAuthority;
			CredentialSessionContext = aContext;
			CredentialLifecycle      = credential_lifecycle::persistent;
			// The origin travels with the binding rather than with the
			// gesture. A resume names a profile and nothing else, so this is
			// the only place a restored session can learn that what it is
			// about to be established from came from a device grant.
			CredentialProvenance        = CredentialBindings.provenance(aAuthority.Profile);
			CredentialBindingRemembered = true;
		}

		void start_deferred_session()
		{
			if (Closing || !DeferredSession.has_value()
				|| ActiveVaultOperation != vault_operation::none) {
				return;
			}

			const auto context = *DeferredSession;
			DeferredSession.reset();
			if (SessionResumeProfile.has_value()) {
				if (const auto* retained = CredentialBindings.find(*SessionResumeProfile);
					retained != nullptr) {
					select_persistent_credential(*retained, context);
				}
			}
			const github_profile_request request{
				.Context = context,
				.Attempt = github_provider_attempt{ 1U },
			};
			submit_profile(request);
		}

		void submit_profile(const github_profile_request& aRequest)
		{
			PendingProvider      = aRequest;
			const auto  token    = ++ProviderOperationToken;
			const auto  gate     = std::weak_ptr<completion_gate>{ CompletionGate };
			auto* const lane     = &DecisionLane;
			const auto  callback = [gate, lane, token, aRequest](
									   github_profile_completion aCompletion) noexcept {
				const auto callbackTarget = gate.lock();
				if (!callbackTarget) {
					return;
				}
				const std::lock_guard callbackBarrier{ callbackTarget->Barrier };
				if (callbackTarget->Host.load(std::memory_order_acquire) == nullptr) {
					return;
				}
				try {
					auto payload = std::make_shared<github_profile_completion>(
						std::move(aCompletion));
					lane->post(
						[gate, payload, token]() mutable {
							const auto locked = gate.lock();
							if (!locked) {
								return;
							}
							const std::lock_guard completionBarrier{
								locked->Barrier
							};
							auto* const host = locked->Host.load(std::memory_order_acquire);
							if (host != nullptr) {
								host->handle_profile(token, std::move(*payload));
							}
						});
				} catch (...) {
					try {
						lane->post(
							[gate, token, aRequest]() mutable {
								const auto locked = gate.lock();
								if (!locked) {
									return;
								}
								const std::lock_guard completionBarrier{
									locked->Barrier
								};
								auto* const host = locked->Host.load(std::memory_order_acquire);
								if (host != nullptr) {
									host->handle_profile(
										token,
										github_profile_error{
											.Request = aRequest,
											.Error   = {
												.Failure = github_provider_failure::
													response_limit,
												.RetryAfterSeconds = std::nullopt,
												.RateLimits        = { },
												.RateLimitCause    = std::nullopt,
											},
										});
								}
							});
					} catch (...) {
					}
				}
			};
			try {
				Profiles.resolve_profile(aRequest, callback);
			} catch (...) {
				callback(github_profile_error{
					.Request = aRequest,
					.Error   = {
						.Failure           = github_provider_failure::response_limit,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					},
				});
			}
		}

		void handle_profile(const std::uint64_t       aToken,
							github_profile_completion aCompletion)
		{
			run_turn([this, aToken, completion = std::move(aCompletion)]() mutable { handle_profile_now(aToken, std::move(completion)); });
		}

		void handle_profile_now(const std::uint64_t       aToken,
								github_profile_completion aCompletion)
		{
			if (Closing || aToken != ProviderOperationToken) {
				return;
			}
			const auto* pending = std::get_if<github_profile_request>(&PendingProvider);
			if (pending == nullptr) {
				return;
			}
			const auto current = *pending;
			if (aCompletion.valueless_by_exception()
				|| provider_completion_request(aCompletion) != current) {
				PendingProvider = std::monostate{ };
				fail_session(current.Context,
							 core::github_failure::malformed_response);
				return;
			}

			if (auto* success = std::get_if<github_profile_success>(&aCompletion)) {
				const auto context = success->Request.Context;
				PendingProvider    = std::monostate{ };
				if (!valid(success->Authority)
					|| success->Authority.Profile != context.Profile) {
					fail_session(context, core::github_failure::malformed_response);
					return;
				}
				CurrentProfile = success->Authority;

				if (SessionPublicPreview) {
					// The signed-out course ends here. No viewer is requested,
					// no capability is probed, and no vault is consulted: the
					// authority the profile resolved to is the whole of what
					// this mode observes.
					begin_public_preview(context, success->Authority);
					return;
				}

				if (success->Authority.Authentication
					== github_authentication_transport::github_cli) {
					const auto hasNativeMaterial = SessionEnrollment.has_value()
						|| SessionResumeProfile.has_value();
					if (const auto* retained = CredentialBindings.find(context.Profile);
						retained != nullptr) {
						select_persistent_credential(*retained, context);
						if (hasNativeMaterial) {
							SessionEnrollment.reset();
							SessionEnrollmentProvenance = github_credential_provenance::manual;
							SessionResumeProfile.reset();
							fail_session(context,
										 core::github_failure::malformed_response,
										 true);
						} else {
							DeferredSession = context;
							start_delete(context.IdentityGeneration, std::nullopt);
						}
						return;
					}
					if (hasNativeMaterial) {
						SessionEnrollment.reset();
						SessionEnrollmentProvenance = github_credential_provenance::manual;
						SessionResumeProfile.reset();
						fail_session(context,
									 core::github_failure::malformed_response);
						return;
					}
					ViewerMode = viewer_mode::cli;
					submit_viewer(
						github_viewer_request{
							.Context   = context,
							.Authority = success->Authority,
							.Attempt   = github_provider_attempt{ 1U },
						},
						std::monostate{ });
					return;
				}

				if (SessionEnrollment.has_value()
					&& SessionResumeProfile.has_value()) {
					fail_session(context, core::github_failure::malformed_response);
					return;
				}
				if (SessionEnrollment.has_value()) {
					if (const auto* retained = CredentialBindings.find(context.Profile);
						retained != nullptr) {
						select_persistent_credential(*retained, context);
						DeferredSession = context;
						start_delete(context.IdentityGeneration, std::nullopt);
						return;
					}
					ViewerMode = viewer_mode::bootstrap;
					submit_bootstrap_viewer(github_viewer_request{
						.Context   = context,
						.Authority = success->Authority,
						.Attempt   = github_provider_attempt{ 1U },
					});
					return;
				}
				if (!SessionResumeProfile.has_value()
					|| *SessionResumeProfile != context.Profile) {
					fail_session(context, core::github_failure::vault);
					return;
				}

				const auto* retained = CredentialBindings.find(context.Profile);
				if (retained == nullptr) {
					fail_session(context, core::github_failure::vault);
					return;
				}
				const auto resumeAuthority = *retained;
				select_persistent_credential(resumeAuthority, context);
				SessionResumeProfile.reset();
				if (resumeAuthority.Profile != success->Authority.Profile
					|| resumeAuthority.ProfileRevision
						!= success->Authority.ProfileRevision
					|| resumeAuthority.Host != success->Authority.Host
					|| resumeAuthority.Hostname != success->Authority.Hostname
					|| resumeAuthority.ServerContract
						!= success->Authority.ServerContract) {
					fail_session(
						context, core::github_failure::malformed_response, true);
					return;
				}
				ViewerMode = viewer_mode::account;
				start_retrieve(github_provider_attempt{ 1U });
				return;
			}

			if (auto* error = std::get_if<github_profile_error>(&aCompletion)) {
				const auto errorValid = valid(error->Error)
					&& !has_observations(error->Error.RateLimits);
				if (errorValid && retryable(error->Error)
					&& error->Request.Attempt.value()
						< maximum_github_provider_attempts) {
					auto retry    = error->Request;
					retry.Attempt = github_provider_attempt{
						static_cast<std::uint8_t>(retry.Attempt.value() + 1U)
					};
					submit_profile(retry);
					return;
				}
				PendingProvider = std::monostate{ };
				if (errorValid) {
					publish_error_advisory(error->Error);
				}
				fail_session(error->Request.Context,
							 errorValid
								 ? mapped_failure(error->Error.Failure)
								 : core::github_failure::malformed_response);
				return;
			}

			const auto& cancelled = std::get<github_profile_cancelled>(aCompletion);
			PendingProvider       = std::monostate{ };
			cancel_session(cancelled.Request.Context);
		}

		void submit_bootstrap_viewer(const github_viewer_request& aRequest)
		{
			if (!SessionEnrollment.has_value()) {
				fail_session(aRequest.Context, core::github_failure::vault);
				return;
			}
			auto copy = github_credential_material::create(SessionEnrollment->bytes());
			if (!copy.has_value()) {
				fail_session(aRequest.Context, core::github_failure::vault);
				return;
			}
			submit_viewer(aRequest,
						  github_bootstrap_credential{
							  .Credential = std::move(*copy),
						  });
		}

		void submit_viewer(const github_viewer_request& aRequest,
						   github_viewer_authentication aAuthentication)
		{
			PendingProvider      = aRequest;
			const auto  token    = ++ProviderOperationToken;
			const auto  gate     = std::weak_ptr<completion_gate>{ CompletionGate };
			auto* const lane     = &DecisionLane;
			const auto  callback = [gate, lane, token, aRequest](
									   github_viewer_completion aCompletion) noexcept {
				const auto callbackTarget = gate.lock();
				if (!callbackTarget) {
					return;
				}
				const std::lock_guard callbackBarrier{ callbackTarget->Barrier };
				if (callbackTarget->Host.load(std::memory_order_acquire) == nullptr) {
					return;
				}
				try {
					auto payload = std::make_shared<github_viewer_completion>(
						std::move(aCompletion));
					lane->post(
						[gate, payload, token]() mutable {
							const auto locked = gate.lock();
							if (!locked) {
								return;
							}
							const std::lock_guard completionBarrier{
								locked->Barrier
							};
							auto* const host = locked->Host.load(std::memory_order_acquire);
							if (host != nullptr) {
								host->handle_viewer(token, std::move(*payload));
							}
						});
				} catch (...) {
					try {
						lane->post(
							[gate, token, aRequest]() mutable {
								const auto locked = gate.lock();
								if (!locked) {
									return;
								}
								const std::lock_guard completionBarrier{
									locked->Barrier
								};
								auto* const host = locked->Host.load(std::memory_order_acquire);
								if (host != nullptr) {
									host->handle_viewer(
										token,
										github_viewer_error{
											.Request = aRequest,
											.Error   = {
												.Failure = github_provider_failure::
													response_limit,
												.RetryAfterSeconds = std::nullopt,
												.RateLimits        = { },
												.RateLimitCause    = std::nullopt,
											},
										});
								}
							});
					} catch (...) {
					}
				}
			};
			try {
				Viewers.observe_viewer(
					github_viewer_submission{
						.Request        = aRequest,
						.Authentication = std::move(aAuthentication),
					},
					callback);
			} catch (...) {
				callback(github_viewer_error{
					.Request = aRequest,
					.Error   = {
						.Failure           = github_provider_failure::response_limit,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					},
				});
			}
		}

		void handle_viewer(const std::uint64_t      aToken,
						   github_viewer_completion aCompletion)
		{
			run_turn([this, aToken, completion = std::move(aCompletion)]() mutable { handle_viewer_now(aToken, std::move(completion)); });
		}

		void handle_viewer_now(const std::uint64_t      aToken,
							   github_viewer_completion aCompletion)
		{
			if (Closing || aToken != ProviderOperationToken) {
				return;
			}
			const auto* pending = std::get_if<github_viewer_request>(&PendingProvider);
			if (pending == nullptr) {
				return;
			}
			const auto current = *pending;
			if (aCompletion.valueless_by_exception()
				|| provider_completion_request(aCompletion) != current) {
				PendingProvider = std::monostate{ };
				fail_session(current.Context,
							 core::github_failure::malformed_response);
				return;
			}

			if (auto* success = std::get_if<github_viewer_success>(&aCompletion)) {
				const auto context   = success->Request.Context;
				const auto authority = success->Request.Authority;
				PendingProvider      = std::monostate{ };
				if (!valid_viewer(
						authority, context, success->Viewer, success->RateLimits)) {
					fail_session(context, core::github_failure::malformed_response);
					return;
				}

				if (ViewerMode == viewer_mode::bootstrap) {
					if (success->AccountTransport != nullptr) {
						fail_session(context,
									 core::github_failure::malformed_response);
						return;
					}
					static_cast<void>(merge(RateLimits, success->RateLimits));
					BootstrapViewer    = success->Viewer;
					const auto adopted = credential_key(authority, success->Viewer.Account);
					resolve_retained_refresh(adopted);
					CredentialAuthority      = adopted;
					CredentialSessionContext = context;
					CredentialLifecycle      = credential_lifecycle::provisional;
					CredentialProvenance     = SessionEnrollmentProvenance;
					if (!CredentialBindings.remember(*CredentialAuthority,
													 CredentialProvenance)) {
						fail_session(context, core::github_failure::vault);
						return;
					}
					CredentialBindingRemembered = true;
					start_store(context);
					return;
				}

				if (success->AccountTransport == nullptr) {
					fail_session(context, core::github_failure::malformed_response);
					return;
				}
				if (ViewerMode == viewer_mode::account) {
					if (!CredentialAuthority.has_value()
						|| success->Viewer.Account != CredentialAuthority->Account) {
						fail_session(context,
									 core::github_failure::malformed_response,
									 true);
						return;
					}
					if (BootstrapViewer.has_value()
						&& success->Viewer != *BootstrapViewer) {
						fail_session(context,
									 core::github_failure::malformed_response);
						return;
					}
				}

				static_cast<void>(merge(RateLimits, success->RateLimits));
				AccountTransport = success->AccountTransport;
				begin_capabilities(
					context, authority, success->Viewer, success->AccountTransport);
				return;
			}

			if (auto* error = std::get_if<github_viewer_error>(&aCompletion)) {
				const auto errorValid = valid_provider_error_resources(
					error->Error, true, false, false);
				if (errorValid) {
					observe_provider_error(error->Error);
				}
				if (errorValid && retryable(error->Error)
					&& error->Request.Attempt.value()
						< maximum_github_provider_attempts) {
					auto retry    = error->Request;
					retry.Attempt = github_provider_attempt{
						static_cast<std::uint8_t>(retry.Attempt.value() + 1U)
					};
					if (ViewerMode == viewer_mode::bootstrap) {
						submit_bootstrap_viewer(retry);
					} else if (ViewerMode == viewer_mode::account) {
						PendingProvider = std::monostate{ };
						start_retrieve(retry.Attempt);
					} else {
						submit_viewer(retry, std::monostate{ });
					}
					return;
				}
				PendingProvider = std::monostate{ };
				if (errorValid) {
					publish_error_advisory(error->Error);
				}
				fail_session(error->Request.Context,
							 errorValid
								 ? mapped_failure(error->Error.Failure)
								 : core::github_failure::malformed_response);
				return;
			}

			const auto& cancelled = std::get<github_viewer_cancelled>(aCompletion);
			PendingProvider       = std::monostate{ };
			cancel_session(cancelled.Request.Context);
		}

		[[nodiscard]] auto next_epoch() -> std::optional<github_freshness_epoch>
		{
			if (LogicalEpoch == std::numeric_limits<std::uint64_t>::max()) {
				return std::nullopt;
			}
			++LogicalEpoch;
			return github_freshness_epoch{ LogicalEpoch };
		}

		void begin_capabilities(const core::github_session_request_context& aContext,
								const github_profile_authority&             aProfile,
								const github_viewer_record&                 aViewer,
								const github_account_transport&             aTransport)
		{
			const auto epoch = next_epoch();
			if (!epoch.has_value()) {
				fail_session(aContext, core::github_failure::malformed_response);
				return;
			}
			const auto key    = capability_key(aProfile, aViewer.Account);
			const auto lookup = CapabilityCache.lookup(key, *epoch);
			if (lookup.hit()) {
				accept_session(session_candidate{
					.Context            = aContext,
					.ProfileAuthority   = aProfile,
					.Viewer             = aViewer,
					.CapabilityEvidence = *lookup.Entry,
					.AccountTransport   = aTransport,
					.RateLimits         = RateLimits,
					.CacheStage         = std::nullopt,
				});
				return;
			}
			if (lookup.Status != github_capability_cache_lookup_status::miss
				&& lookup.Status != github_capability_cache_lookup_status::expired) {
				fail_session(aContext, core::github_failure::malformed_response);
				return;
			}

			submit_capability(github_capability_request{
				.Context          = aContext,
				.ProfileAuthority = aProfile,
				.Viewer           = aViewer,
				.AccountTransport = aTransport,
				.Attempt          = github_provider_attempt{ 1U },
			});
		}

		void submit_capability(const github_capability_request& aRequest)
		{
			PendingProvider   = aRequest;
			const auto  token = ++ProviderOperationToken;
			const auto  gate  = std::weak_ptr<completion_gate>{ CompletionGate };
			auto* const lane  = &DecisionLane;
			const auto  callback =
				[gate, lane, token, aRequest](
					github_capability_completion aCompletion) noexcept {
					const auto callbackTarget = gate.lock();
					if (!callbackTarget) {
						return;
					}
					const std::lock_guard callbackBarrier{ callbackTarget->Barrier };
					if (callbackTarget->Host.load(std::memory_order_acquire) == nullptr) {
						return;
					}
					try {
						auto payload = std::make_shared<github_capability_completion>(
							std::move(aCompletion));
						lane->post(
							[gate, payload, token]() mutable {
								const auto locked = gate.lock();
								if (!locked) {
									return;
								}
								const std::lock_guard completionBarrier{
									locked->Barrier
								};
								auto* const host = locked->Host.load(std::memory_order_acquire);
								if (host != nullptr) {
									host->handle_capability(token, std::move(*payload));
								}
							});
					} catch (...) {
						try {
							lane->post(
								[gate, token, aRequest]() mutable {
									const auto locked = gate.lock();
									if (!locked) {
										return;
									}
									const std::lock_guard completionBarrier{
										locked->Barrier
									};
									auto* const host = locked->Host.load(std::memory_order_acquire);
									if (host != nullptr) {
										host->handle_capability(
											token,
											github_capability_error{
												.Request = aRequest,
												.Error   = {
													.Failure = github_provider_failure::
														response_limit,
													.RetryAfterSeconds = std::nullopt,
													.RateLimits        = { },
													.RateLimitCause    = std::nullopt,
												},
											});
									}
								});
						} catch (...) {
						}
					}
				};
			try {
				Capabilities.probe_capabilities(aRequest, callback);
			} catch (...) {
				callback(github_capability_error{
					.Request = aRequest,
					.Error   = {
						.Failure           = github_provider_failure::response_limit,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					},
				});
			}
		}

		void handle_capability(const std::uint64_t          aToken,
							   github_capability_completion aCompletion)
		{
			run_turn([this, aToken, completion = std::move(aCompletion)]() mutable { handle_capability_now(aToken, std::move(completion)); });
		}

		void handle_capability_now(const std::uint64_t          aToken,
								   github_capability_completion aCompletion)
		{
			if (Closing || aToken != ProviderOperationToken) {
				return;
			}
			const auto* pending = std::get_if<github_capability_request>(&PendingProvider);
			if (pending == nullptr) {
				return;
			}
			const auto current = *pending;
			if (aCompletion.valueless_by_exception()
				|| provider_completion_request(aCompletion) != current) {
				PendingProvider = std::monostate{ };
				fail_session(current.Context,
							 core::github_failure::malformed_response);
				return;
			}

			if (auto* success = std::get_if<github_capability_success>(&aCompletion)) {
				PendingProvider  = std::monostate{ };
				const auto epoch = next_epoch();
				if (!epoch.has_value()
					|| !valid_capability_rate_limits(
						success->Request.ProfileAuthority, success->RateLimits)) {
					fail_session(success->Request.Context,
								 core::github_failure::malformed_response);
					return;
				}
				const github_capability_probe_observation observation{
					.Authority   = capability_key(success->Request.ProfileAuthority,
												  success->Request.Viewer.Account),
					.ObservedAt  = *epoch,
					.Facts       = success->Facts,
					.SchemaFacts = success->SchemaFacts,
				};
				const auto staged = CapabilityCache.stage(
					github_capability_probe_result{ observation });
				if (!staged.staged()) {
					fail_session(
						success->Request.Context,
						staged.Error
								== github_capability_cache_error::capacity_exhausted
							? core::github_failure::response_too_large
							: core::github_failure::malformed_response);
					return;
				}
				static_cast<void>(merge(RateLimits, success->RateLimits));
				accept_session(session_candidate{
					.Context            = success->Request.Context,
					.ProfileAuthority   = success->Request.ProfileAuthority,
					.Viewer             = success->Request.Viewer,
					.CapabilityEvidence = staged.Stage->entry(),
					.AccountTransport   = success->Request.AccountTransport,
					.RateLimits         = RateLimits,
					.CacheStage         = staged.Stage,
				});
				return;
			}

			if (auto* error = std::get_if<github_capability_error>(&aCompletion)) {
				const auto errorValid = valid_provider_error_resources(
					error->Error, false, false, true);
				if (errorValid) {
					observe_provider_error(error->Error);
				}
				if (errorValid && retryable(error->Error)
					&& error->Request.Attempt.value()
						< maximum_github_provider_attempts) {
					auto retry    = error->Request;
					retry.Attempt = github_provider_attempt{
						static_cast<std::uint8_t>(retry.Attempt.value() + 1U)
					};
					submit_capability(retry);
					return;
				}
				PendingProvider = std::monostate{ };
				if (errorValid) {
					publish_error_advisory(error->Error);
				}
				fail_session(error->Request.Context,
							 errorValid
								 ? mapped_failure(error->Error.Failure)
								 : core::github_failure::malformed_response);
				return;
			}

			const auto& cancelled = std::get<github_capability_cancelled>(aCompletion);
			PendingProvider       = std::monostate{ };
			cancel_session(cancelled.Request.Context);
		}

		// The one question composition asks about restored material: can this
		// session renew itself, or would telling its panel to renew end in a
		// vault that answers `absent` and a reviewer told to authorize again
		// for no reason? Only the device flow ever writes a refresh entry, so
		// the entry's existence under this authority is the whole answer.
		//
		// Retrieval is the only presence question this port answers, so the
		// probe retrieves and then destroys what it got, on the vault's own
		// callback, before anything crosses onto the decision lane. This host
		// has no business holding renewal material and after this does not.
		void start_refresh_probe()
		{
			if (Closing || !CredentialAuthority.has_value()
				|| !CredentialSessionContext.has_value()) {
				return;
			}
			const github_credential_operation_context probe{
				.Request            = CredentialSessionContext->Request,
				.ApplicationSession = CredentialSessionContext->ApplicationSession,
				.Authority          = *CredentialAuthority,
				.IdentityGeneration = CredentialSessionContext->IdentityGeneration,
			};
			RefreshProbeContext = probe;
			const auto  token   = ++RefreshProbeToken;
			const auto  gate    = std::weak_ptr<completion_gate>{ CompletionGate };
			auto* const lane    = &DecisionLane;
			const auto  callback =
				[gate, lane, token, probe](
					github_credential_retrieve_completion aCompletion) noexcept {
					auto* const retrieved = std::get_if<
						github_credential_retrieve_success>(&aCompletion);
					if (retrieved == nullptr) {
						// Absent, unavailable, refused, cancelled: all of them
						// mean "nothing renews this credential", which is what
						// the host already believes. Nothing is posted, so a
						// negative answer costs the decision lane no turn and
						// changes no order anything else observes.
						return;
					}
					retrieved->Credential.reset();
					const auto callbackTarget = gate.lock();
					if (!callbackTarget) {
						return;
					}
					const std::lock_guard callbackBarrier{ callbackTarget->Barrier };
					if (callbackTarget->Host.load(std::memory_order_acquire) == nullptr) {
						return;
					}
					try {
						lane->post([gate, token, probe]() {
							const auto locked = gate.lock();
							if (!locked) {
								return;
							}
							const std::lock_guard completionBarrier{
								locked->Barrier
							};
							auto* const host = locked->Host.load(std::memory_order_acquire);
							if (host != nullptr) {
								host->handle_refresh_probe(token, probe);
							}
						});
					} catch (...) {
					}
				};
			try {
				Credentials.retrieve_refresh(probe, callback);
			} catch (...) {
				RefreshProbeContext.reset();
			}
		}

		void handle_refresh_probe(
			const std::uint64_t                        aToken,
			const github_credential_operation_context& aExpectedProbe)
		{
			run_turn([this, aToken, aExpectedProbe]() {
				handle_refresh_probe_now(aToken, aExpectedProbe);
			});
		}

		void handle_refresh_probe_now(
			const std::uint64_t                        aToken,
			const github_credential_operation_context& aExpectedProbe)
		{
			if (aToken != RefreshProbeToken || !RefreshProbeContext.has_value()
				|| *RefreshProbeContext != aExpectedProbe) {
				return;
			}
			RefreshProbeContext.reset();
			// Anything that invalidated the identity while the vault was
			// answering makes the answer about a session that no longer
			// exists, and a stale `true` here would arm renewal under a key
			// this host has already forgotten.
			if (Closing || !AcceptedSession.has_value()
				|| !CredentialAuthority.has_value()
				|| *CredentialAuthority != aExpectedProbe.Authority) {
				return;
			}
			// [[BPR-0540]]. Presence is not the whole answer. Both slots are
			// keyed by the same authority, so a token the reviewer pasted for
			// an account a device grant once enrolled lands beside whatever
			// that grant left behind, and spending it would take the reviewer
			// off the session they asked for and onto one they did not. The
			// entry renews *this* credential only when this credential itself
			// came from a grant.
			if (CredentialProvenance != github_credential_provenance::device_grant) {
				return;
			}
			AcceptedCredentialRenewable = true;
			republish_accepted_session();
		}

		// Renewability is a vault answer and the vault answers after the
		// session was published. Rather than hold an acceptance back on a
		// keychain round trip, the host publishes the accepted session once
		// more when it learns. Every reader keyed on the transition sees
		// nothing new -- there is none -- and the one reader that asks whether
		// this credential can renew itself now gets a settled answer.
		void republish_accepted_session() noexcept
		{
			if (!AcceptedSession.has_value()) {
				return;
			}
			try {
				const github_presentation_update update{
					.Transition      = std::nullopt,
					.Session         = project_github_session(*AcceptedSession),
					.PullRequest     = std::nullopt,
					.HostDiagnostics = { },
				};
				Projections.publish(update);
			} catch (...) {
				ProjectionDeliveryFailed = true;
			}
		}

		void accept_session(session_candidate aCandidate)
		{
			const github_session_read_model model{
				.Profile            = aCandidate.ProfileAuthority.Profile,
				.Host               = aCandidate.ProfileAuthority.Host,
				.Viewer             = aCandidate.Viewer,
				.CapabilityEvidence = {
					.Capabilities    = aCandidate.CapabilityEvidence.Capabilities,
					.Revision        = aCandidate.CapabilityEvidence.Revision,
					.ReadShape       = aCandidate.CapabilityEvidence.ReadShape,
					.ContractVersion = 1U,
				},
				.RateLimits = aCandidate.RateLimits,
				.Mode       = aCandidate.Mode,
			};
			if (!validate(model, aCandidate.Context).valid()) {
				fail_session(aCandidate.Context,
							 core::github_failure::malformed_response);
				return;
			}
			if (CredentialAuthority.has_value() && !CredentialBindingRemembered) {
				if (!CredentialBindings.remember(*CredentialAuthority,
												 CredentialProvenance)) {
					SessionEnrollment.reset();
					SessionEnrollmentProvenance = github_credential_provenance::manual;
					fail_session(aCandidate.Context, core::github_failure::vault);
					return;
				}
				CredentialBindingRemembered = true;
			}

			StagedSession = aCandidate;
			auto output   = core::transition(
				State,
				core::github_session_ready_event{
					.Context            = aCandidate.Context,
					.Host               = aCandidate.ProfileAuthority.Host,
					.Account            = aCandidate.Viewer.Account,
					.Capabilities       = aCandidate.CapabilityEvidence.Capabilities,
					.CapabilityRevision = aCandidate.CapabilityEvidence.Revision,
				});
			const core::accepted_session_delta expected{
				.Context            = aCandidate.Context,
				.Host               = aCandidate.ProfileAuthority.Host,
				.Account            = aCandidate.Viewer.Account,
				.Capabilities       = aCandidate.CapabilityEvidence.Capabilities,
				.CapabilityRevision = aCandidate.CapabilityEvidence.Revision,
			};
			if (!output.AcceptedSession.has_value()
				|| *output.AcceptedSession != expected) {
				StagedSession.reset();
				publish_and_execute(output);
				if (CredentialLifecycle == credential_lifecycle::provisional
					&& CredentialAuthority.has_value()
					&& CredentialSessionContext.has_value()
					&& ActiveVaultOperation == vault_operation::none) {
					start_delete(aCandidate.Context.IdentityGeneration,
								 std::nullopt);
				}
				return;
			}

			if (aCandidate.CacheStage.has_value()) {
				const auto committed = CapabilityCache.commit(*aCandidate.CacheStage);
				if (!committed.stored()
					|| committed.Entry != aCandidate.CapabilityEvidence) {
					StagedSession.reset();
					publish_diagnostic(github_host_diagnostic{
						.Reason            = core::github_reason_code::malformed_response,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					});
					auto rollback = core::transition(
						State, core::deactivate_github_identity_intent{ });
					publish_and_execute(rollback);
					return;
				}
			}

			AcceptedSession            = model;
			AcceptedProfileAuthority   = aCandidate.ProfileAuthority;
			AcceptedCapabilityEvidence = aCandidate.CapabilityEvidence;
			AccountTransport           = aCandidate.AccountTransport;
			RateLimits                 = aCandidate.RateLimits;
			StagedSession.reset();
			SessionEnrollment.reset();
			SessionEnrollmentProvenance = github_credential_provenance::manual;
			SessionResumeProfile.reset();
			BootstrapViewer.reset();
			ViewerMode = viewer_mode::none;
			if (CredentialAuthority.has_value()) {
				CredentialLifecycle = credential_lifecycle::persistent;
			}
			publish_transition(output, &*AcceptedSession);
			// Asked on every acceptance that has a slot, rather than only on
			// the ones a restart produced. The host cannot tell a credential
			// restored from a device grant from one restored from a pasted
			// token -- that is the whole reason the question is put to the
			// vault -- and a grant this run just authorized has not stored its
			// refresh material yet, so it answers `absent` and binds instead.
			if (CredentialAuthority.has_value()) {
				start_refresh_probe();
			}
		}

		void start_store(const core::github_session_request_context& aContext)
		{
			if (!CredentialAuthority.has_value() || !SessionEnrollment.has_value()) {
				fail_session(aContext, core::github_failure::vault);
				return;
			}
			const github_credential_operation_context operation{
				.Request            = aContext.Request,
				.ApplicationSession = aContext.ApplicationSession,
				.Authority          = *CredentialAuthority,
				.IdentityGeneration = aContext.IdentityGeneration,
			};
			ActiveVaultOperation = vault_operation::store;
			ActiveVaultContext   = operation;
			const auto  token    = ++VaultOperationToken;
			const auto  gate     = std::weak_ptr<completion_gate>{ CompletionGate };
			auto* const lane     = &DecisionLane;
			const auto  callback =
				[gate, lane, token, operation](
					github_credential_store_completion aCompletion) noexcept {
					const auto callbackTarget = gate.lock();
					if (!callbackTarget) {
						return;
					}
					const std::lock_guard callbackBarrier{ callbackTarget->Barrier };
					if (callbackTarget->Host.load(std::memory_order_acquire) == nullptr) {
						return;
					}
					try {
						auto payload = std::make_shared<github_credential_store_completion>(
							std::move(aCompletion));
						lane->post(
							[gate, payload, token, operation]() mutable {
								const auto locked = gate.lock();
								if (!locked) {
									return;
								}
								const std::lock_guard completionBarrier{
									locked->Barrier
								};
								auto* const host = locked->Host.load(std::memory_order_acquire);
								if (host != nullptr) {
									host->handle_store(
										token, operation, std::move(*payload));
								}
							});
					} catch (...) {
						try {
							lane->post(
								[gate, token, operation]() mutable {
									const auto locked = gate.lock();
									if (!locked) {
										return;
									}
									const std::lock_guard completionBarrier{
										locked->Barrier
									};
									auto* const host = locked->Host.load(std::memory_order_acquire);
									if (host != nullptr) {
										host->handle_store(
											token,
											operation,
											github_credential_store_error{
												.Context = operation,
												.Failure = github_credential_vault_failure::
													allocation_failure,
											});
									}
								});
						} catch (...) {
						}
					}
				};
			try {
				Credentials.store(
					github_credential_enrollment{
						.Context    = operation,
						.Credential = std::move(*SessionEnrollment),
					},
					callback);
			} catch (...) {
				callback(github_credential_store_error{
					.Context = operation,
					.Failure = github_credential_vault_failure::operation_failed,
				});
			}
			SessionEnrollment.reset();
			SessionEnrollmentProvenance = github_credential_provenance::manual;
		}

		void
		handle_store(const std::uint64_t                        aToken,
					 const github_credential_operation_context& aExpectedOperation,
					 github_credential_store_completion         aCompletion)
		{
			run_turn(
				[this,
				 aToken,
				 aExpectedOperation,
				 completion = std::move(aCompletion)]() mutable {
					handle_store_now(
						aToken, aExpectedOperation, std::move(completion));
				});
		}

		void handle_store_now(
			const std::uint64_t                        aToken,
			const github_credential_operation_context& aExpectedOperation,
			github_credential_store_completion         aCompletion)
		{
			if (aToken != VaultOperationToken) {
				return;
			}
			++VaultOperationToken;
			if (Closing) {
				if (ActiveVaultOperation == vault_operation::store
					&& ActiveVaultContext == aExpectedOperation) {
					ActiveVaultOperation = vault_operation::none;
					ActiveVaultContext.reset();
					DeferredInvalidationGeneration.reset();
					if (requires_shutdown_credential_cleanup()) {
						start_delete(aExpectedOperation.IdentityGeneration,
									 std::nullopt);
					}
				} else {
					ActiveVaultOperation = vault_operation::none;
					ActiveVaultContext.reset();
					ShutdownCleanupFailed = true;
					publish_diagnostic(github_host_diagnostic{
						.Reason            = core::github_reason_code::vault_unavailable,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					});
				}
				return;
			}
			if (DeferredInvalidationGeneration.has_value()) {
				const auto generation = *DeferredInvalidationGeneration;
				DeferredInvalidationGeneration.reset();
				const auto preserve = std::exchange(DeferredInvalidationPreservesCredential, false);
				// [[BPR-0547]]. The disposition was decided at the invalidate,
				// where the incoming enrollment is known; deferring the delete
				// past a running vault operation must not decide it again.
				const auto refresh   = std::exchange(DeferredInvalidationRetainsRefresh, false)
					? refresh_disposition::retain
					: refresh_disposition::erase;
				ActiveVaultOperation = vault_operation::none;
				ActiveVaultContext.reset();
				if (preserve) {
					start_deferred_session();
				} else {
					start_delete(generation, std::nullopt, refresh);
				}
				return;
			}
			if (ActiveVaultOperation != vault_operation::store
				|| !ActiveVaultContext.has_value()
				|| *ActiveVaultContext != aExpectedOperation
				|| !CredentialSessionContext.has_value()) {
				ActiveVaultOperation = vault_operation::none;
				ActiveVaultContext.reset();
				publish_diagnostic(github_host_diagnostic{
					.Reason            = core::github_reason_code::vault_unavailable,
					.RetryAfterSeconds = std::nullopt,
					.RateLimits        = { },
					.RateLimitCause    = std::nullopt,
				});
				if (CredentialSessionContext.has_value()) {
					fail_session(*CredentialSessionContext,
								 core::github_failure::vault);
				}
				return;
			}
			if (!valid(aCompletion)
				|| completion_context(aCompletion) != aExpectedOperation) {
				ActiveVaultOperation = vault_operation::none;
				ActiveVaultContext.reset();
				if (CredentialSessionContext.has_value()) {
					fail_session(*CredentialSessionContext,
								 core::github_failure::vault);
				} else {
					publish_diagnostic(github_host_diagnostic{
						.Reason            = core::github_reason_code::vault_unavailable,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					});
				}
				return;
			}
			const auto session   = *CredentialSessionContext;
			ActiveVaultOperation = vault_operation::none;
			ActiveVaultContext.reset();

			if (std::holds_alternative<github_credential_store_success>(aCompletion)) {
				ViewerMode = viewer_mode::account;
				start_retrieve(github_provider_attempt{ 1U });
				return;
			}
			if (std::holds_alternative<github_credential_store_cancelled>(
					aCompletion)) {
				cancel_session(session);
				return;
			}
			fail_session(session, core::github_failure::vault);
		}

		void start_retrieve(const github_provider_attempt aAttempt)
		{
			if (!CredentialAuthority.has_value()
				|| !CredentialSessionContext.has_value()) {
				return;
			}
			const auto&                               session = *CredentialSessionContext;
			const github_credential_operation_context operation{
				.Request            = session.Request,
				.ApplicationSession = session.ApplicationSession,
				.Authority          = *CredentialAuthority,
				.IdentityGeneration = session.IdentityGeneration,
			};
			PendingRetrieveAttempt = aAttempt;
			ActiveVaultOperation   = vault_operation::retrieve;
			ActiveVaultContext     = operation;
			const auto  token      = ++VaultOperationToken;
			const auto  gate       = std::weak_ptr<completion_gate>{ CompletionGate };
			auto* const lane       = &DecisionLane;
			const auto  callback =
				[gate, lane, token, operation](
					github_credential_retrieve_completion aCompletion) noexcept {
					const auto callbackTarget = gate.lock();
					if (!callbackTarget) {
						return;
					}
					const std::lock_guard callbackBarrier{ callbackTarget->Barrier };
					if (callbackTarget->Host.load(std::memory_order_acquire) == nullptr) {
						return;
					}
					try {
						auto payload = std::make_shared<github_credential_retrieve_completion>(
							std::move(aCompletion));
						lane->post(
							[gate, payload, token, operation]() mutable {
								const auto locked = gate.lock();
								if (!locked) {
									return;
								}
								const std::lock_guard completionBarrier{
									locked->Barrier
								};
								auto* const host = locked->Host.load(std::memory_order_acquire);
								if (host != nullptr) {
									host->handle_retrieve(
										token, operation, std::move(*payload));
								}
							});
					} catch (...) {
						try {
							lane->post(
								[gate, token, operation]() mutable {
									const auto locked = gate.lock();
									if (!locked) {
										return;
									}
									const std::lock_guard completionBarrier{
										locked->Barrier
									};
									auto* const host = locked->Host.load(std::memory_order_acquire);
									if (host != nullptr) {
										host->handle_retrieve(
											token,
											operation,
											github_credential_retrieve_error{
												.Context = operation,
												.Failure = github_credential_vault_failure::
													allocation_failure,
											});
									}
								});
						} catch (...) {
						}
					}
				};
			try {
				Credentials.retrieve(operation, callback);
			} catch (...) {
				callback(github_credential_retrieve_error{
					.Context = operation,
					.Failure = github_credential_vault_failure::operation_failed,
				});
			}
		}

		void handle_retrieve(
			const std::uint64_t                        aToken,
			const github_credential_operation_context& aExpectedOperation,
			github_credential_retrieve_completion      aCompletion)
		{
			run_turn(
				[this,
				 aToken,
				 aExpectedOperation,
				 completion = std::move(aCompletion)]() mutable {
					handle_retrieve_now(
						aToken, aExpectedOperation, std::move(completion));
				});
		}

		void handle_retrieve_now(
			const std::uint64_t                        aToken,
			const github_credential_operation_context& aExpectedOperation,
			github_credential_retrieve_completion      aCompletion)
		{
			if (aToken != VaultOperationToken) {
				return;
			}
			++VaultOperationToken;
			if (Closing) {
				if (ActiveVaultOperation == vault_operation::retrieve
					&& ActiveVaultContext == aExpectedOperation) {
					ActiveVaultOperation = vault_operation::none;
					ActiveVaultContext.reset();
					DeferredInvalidationGeneration.reset();
					if (requires_shutdown_credential_cleanup()) {
						start_delete(aExpectedOperation.IdentityGeneration,
									 std::nullopt);
					}
				} else {
					ActiveVaultOperation = vault_operation::none;
					ActiveVaultContext.reset();
					ShutdownCleanupFailed = true;
					publish_diagnostic(github_host_diagnostic{
						.Reason            = core::github_reason_code::vault_unavailable,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					});
				}
				return;
			}
			if (DeferredInvalidationGeneration.has_value()) {
				const auto generation = *DeferredInvalidationGeneration;
				DeferredInvalidationGeneration.reset();
				const auto preserve = std::exchange(DeferredInvalidationPreservesCredential, false);
				// [[BPR-0547]]. The disposition was decided at the invalidate,
				// where the incoming enrollment is known; deferring the delete
				// past a running vault operation must not decide it again.
				const auto refresh   = std::exchange(DeferredInvalidationRetainsRefresh, false)
					? refresh_disposition::retain
					: refresh_disposition::erase;
				ActiveVaultOperation = vault_operation::none;
				ActiveVaultContext.reset();
				if (preserve) {
					start_deferred_session();
				} else {
					start_delete(generation, std::nullopt, refresh);
				}
				return;
			}
			if (ActiveVaultOperation != vault_operation::retrieve
				|| !ActiveVaultContext.has_value()
				|| *ActiveVaultContext != aExpectedOperation
				|| !CurrentProfile.has_value()
				|| !CredentialSessionContext.has_value()) {
				ActiveVaultOperation = vault_operation::none;
				ActiveVaultContext.reset();
				publish_diagnostic(github_host_diagnostic{
					.Reason            = core::github_reason_code::vault_unavailable,
					.RetryAfterSeconds = std::nullopt,
					.RateLimits        = { },
					.RateLimitCause    = std::nullopt,
				});
				if (CredentialSessionContext.has_value()) {
					fail_session(*CredentialSessionContext,
								 core::github_failure::vault);
				}
				return;
			}
			if (!valid(aCompletion)
				|| completion_context(aCompletion) != aExpectedOperation) {
				const auto context   = *CredentialSessionContext;
				ActiveVaultOperation = vault_operation::none;
				ActiveVaultContext.reset();
				fail_session(context, core::github_failure::vault);
				return;
			}
			ActiveVaultOperation = vault_operation::none;
			ActiveVaultContext.reset();
			const auto context = *CredentialSessionContext;

			if (auto* success = std::get_if<github_credential_retrieve_success>(&aCompletion)) {
				submit_viewer(
					github_viewer_request{
						.Context   = context,
						.Authority = *CurrentProfile,
						.Attempt   = PendingRetrieveAttempt,
					},
					std::move(*success));
				return;
			}
			if (std::holds_alternative<github_credential_retrieve_absent>(
					aCompletion)) {
				if (CredentialBindingRemembered) {
					CredentialBindings.forget(aExpectedOperation.Authority);
				}
				CredentialAuthority.reset();
				CredentialSessionContext.reset();
				CredentialLifecycle         = credential_lifecycle::none;
				CredentialProvenance        = github_credential_provenance::manual;
				CredentialBindingRemembered = false;
				fail_session_direct(context, core::github_failure::vault);
				return;
			}
			if (std::holds_alternative<github_credential_retrieve_cancelled>(
					aCompletion)) {
				cancel_session(context);
				return;
			}
			fail_session(context, core::github_failure::vault);
		}

		// The refresh slot's own erasure. It carries no answer back on the
		// ordinary occasions: an erased slot, an absent one, and a platform
		// whose store has no second slot are all "nothing renews this
		// credential any more", which is what the host already believes and
		// what the reviewer asked for. Only a failure crosses onto the
		// decision lane, because a failure means material this host meant to
		// destroy is still in the vault and somebody has to be told.
		//
		// The count, not the answer, is what quiescence reads: composition
		// holds this host and the vault alive until the terminal has been
		// delivered, exactly as it does for the credential delete beside it.
		void start_refresh_delete(
			const github_credential_operation_context& aOperation)
		{
			OutstandingRefreshDeletes.fetch_add(1U, std::memory_order_acq_rel);
			const auto  gate = std::weak_ptr<completion_gate>{ CompletionGate };
			auto* const lane = &DecisionLane;
			const auto  callback =
				[gate, lane, aOperation](
					github_credential_delete_completion aCompletion) noexcept {
					const auto callbackTarget = gate.lock();
					if (!callbackTarget) {
						return;
					}
					const std::lock_guard callbackBarrier{ callbackTarget->Barrier };
					auto* const           host = callbackTarget->Host.load(std::memory_order_acquire);
					if (host == nullptr) {
						return;
					}
					const auto leftMaterial
						= erasure_left_material(aCompletion, aOperation);
					// [[BPR-0545]], residual R1. The count is released on the
					// decision lane after the terminal has been applied, never
					// here beside the answer: released here, quiescence could
					// be observed between the release and the report, so an
					// orderly shutdown finished believing its cleanup was
					// exact while the report saying material is still in the
					// vault was still queued on the lane.
					try {
						lane->post([gate, aOperation, leftMaterial]() {
							const auto locked = gate.lock();
							if (!locked) {
								return;
							}
							const std::lock_guard completionBarrier{
								locked->Barrier
							};
							auto* const target = locked->Host.load(std::memory_order_acquire);
							if (target == nullptr) {
								return;
							}
							if (leftMaterial) {
								target->report_refresh_erasure_failure(aOperation);
							}
							target->OutstandingRefreshDeletes.fetch_sub(
								1U, std::memory_order_acq_rel);
						});
					} catch (...) {
						// The lane refused the post, so no terminal will ever
						// be applied. Releasing the count here is what keeps a
						// shutdown from waiting on an answer that can no
						// longer arrive.
						host->OutstandingRefreshDeletes.fetch_sub(
							1U, std::memory_order_acq_rel);
					}
				};
			try {
				Credentials.delete_refresh(aOperation, callback);
			} catch (...) {
				callback(github_credential_delete_error{
					.Context = aOperation,
					.Failure = github_credential_vault_failure::operation_failed,
				});
			}
		}

		// [[BPR-0549]]. The window [[BPR-0547]]'s retention opens, and the
		// three ways out of it. The retention is decided at the invalidate,
		// where the incoming enrollment's provenance and profile are known but
		// its account is not: that arrives with the bootstrap `/user`
		// observation the enrollment goes on to make. So the slot is held --
		// with the exact operation context that would have erased it -- only
		// for as long as the incoming session can still turn out to name the
		// authority the slot is keyed under.
		//
		// Reachability is the whole test, and it is decided by the entire
		// authority: every platform vault keys the slot by the whole key, so
		// one that differs anywhere -- another account on this profile above
		// all -- is a slot the incoming session's own refresh probe cannot
		// name. Equality is therefore exactly "something may still spend
		// this", which is the rule [[BPR-0540]] set, and inequality erases
		// under the outgoing authority rather than the incoming one.
		void hold_retained_refresh(
			const github_credential_operation_context& aOperation)
		{
			// A shutdown that arrives while the deferred delete is still in
			// flight has already run its cleanup by the time this answers, so
			// there is no later occasion to hold the slot for.
			if (Closing) {
				start_refresh_delete(aOperation);
				return;
			}
			RetainedRefreshOperation = aOperation;
		}

		void erase_retained_refresh()
		{
			if (!RetainedRefreshOperation.has_value()) {
				return;
			}
			const auto operation = *RetainedRefreshOperation;
			RetainedRefreshOperation.reset();
			start_refresh_delete(operation);
		}

		void resolve_retained_refresh(
			const github_credential_authority_key& aAdopted)
		{
			if (!RetainedRefreshOperation.has_value()) {
				return;
			}
			if (RetainedRefreshOperation->Authority == aAdopted) {
				RetainedRefreshOperation.reset();
				return;
			}
			erase_retained_refresh();
		}

		void report_refresh_erasure_failure(
			const github_credential_operation_context& aOperation)
		{
			run_turn([this, aOperation] {
				static_cast<void>(aOperation);
				if (Closing) {
					ShutdownCleanupFailed = true;
				}
				publish_diagnostic(github_host_diagnostic{
					.Reason            = core::github_reason_code::vault_unavailable,
					.RetryAfterSeconds = std::nullopt,
					.RateLimits        = { },
					.RateLimitCause    = std::nullopt,
				});
			});
		}

		void start_delete(const core::github_identity_generation   aGeneration,
						  std::optional<deferred_session_terminal> aTerminal,
						  const refresh_disposition                aRefresh
						  = refresh_disposition::erase)
		{
			if (!CredentialAuthority.has_value()
				|| !CredentialSessionContext.has_value()) {
				if (aTerminal.has_value()) {
					finish_deferred_terminal(*aTerminal);
				}
				return;
			}
			DeferredTerminal                                  = std::move(aTerminal);
			const auto&                               session = *CredentialSessionContext;
			const github_credential_operation_context operation{
				.Request            = session.Request,
				.ApplicationSession = session.ApplicationSession,
				.Authority          = *CredentialAuthority,
				.IdentityGeneration = aGeneration,
			};
			ActiveVaultOperation = vault_operation::delete_credential;
			ActiveVaultContext   = operation;
			// [[BPR-0540]]. The refresh slot leaves with the credential it
			// renews, under the same authority, through the same port, on the
			// same occasion -- an orderly shutdown that deletes what it
			// retained, a sign-out, or an enrollment superseding the session
			// that stored it. Deleting one and keeping the other is what
			// orphaned six months of renewal material behind a session the
			// reviewer had already ended.
			//
			// [[BPR-0545]]. It is issued from this delete's own terminal
			// rather than beside it. Every shipped vault keys its pending
			// operations by the exact `github_credential_operation_context`
			// and answers a duplicate `invalid_request` before queuing, so
			// submitting both halves under one context lost whichever arrived
			// second -- ordinarily the base credential's own delete, because
			// the main thread wins the race to the second call. Sequencing
			// rather than a second context because request identity is the
			// core's to allocate and this host has no allocator of its own:
			// inventing one here to tell two halves of one occasion apart
			// would move identity into the host. What sequencing costs is a
			// crash between the two, which leaves renewable material behind an
			// erased credential; nothing can spend it, because the resume
			// answers only for device-grant provenance ([[BPR-0540]]).
			const auto  token = ++VaultOperationToken;
			const auto  gate  = std::weak_ptr<completion_gate>{ CompletionGate };
			auto* const lane  = &DecisionLane;
			const auto  callback =
				[gate, lane, token, operation, aRefresh](
					github_credential_delete_completion aCompletion) noexcept {
					const auto callbackTarget = gate.lock();
					if (!callbackTarget) {
						return;
					}
					const std::lock_guard callbackBarrier{ callbackTarget->Barrier };
					if (callbackTarget->Host.load(std::memory_order_acquire) == nullptr) {
						return;
					}
					try {
						auto payload = std::make_shared<github_credential_delete_completion>(
							std::move(aCompletion));
						lane->post(
							[gate, payload, token, operation, aRefresh]() mutable {
								const auto locked = gate.lock();
								if (!locked) {
									return;
								}
								const std::lock_guard completionBarrier{
									locked->Barrier
								};
								auto* const host = locked->Host.load(std::memory_order_acquire);
								if (host != nullptr) {
									host->handle_delete(token,
														operation,
														aRefresh,
														std::move(*payload));
								}
							});
					} catch (...) {
						try {
							lane->post(
								[gate, token, operation, aRefresh]() mutable {
									const auto locked = gate.lock();
									if (!locked) {
										return;
									}
									const std::lock_guard completionBarrier{
										locked->Barrier
									};
									auto* const host = locked->Host.load(std::memory_order_acquire);
									if (host != nullptr) {
										host->handle_delete(
											token,
											operation,
											aRefresh,
											github_credential_delete_error{
												.Context = operation,
												.Failure = github_credential_vault_failure::
													allocation_failure,
											});
									}
								});
						} catch (...) {
						}
					}
				};
			try {
				Credentials.delete_credential(operation, callback);
			} catch (...) {
				callback(github_credential_delete_error{
					.Context = operation,
					.Failure = github_credential_vault_failure::operation_failed,
				});
			}
		}

		void
		handle_delete(const std::uint64_t                        aToken,
					  const github_credential_operation_context& aExpectedOperation,
					  const refresh_disposition                  aRefresh,
					  github_credential_delete_completion        aCompletion)
		{
			run_turn(
				[this,
				 aToken,
				 aExpectedOperation,
				 aRefresh,
				 completion = std::move(aCompletion)]() mutable {
					handle_delete_now(aToken,
									  aExpectedOperation,
									  aRefresh,
									  std::move(completion));
				});
		}

		void handle_delete_now(
			const std::uint64_t                        aToken,
			const github_credential_operation_context& aExpectedOperation,
			const refresh_disposition                  aRefresh,
			github_credential_delete_completion        aCompletion)
		{
			if (aToken != VaultOperationToken) {
				return;
			}
			++VaultOperationToken;
			// [[BPR-0545]]. The occasion's other erasure, under the same
			// context, now that the credential's delete has left the vault's
			// pending map. It runs in every branch below, the failing ones
			// included: a credential delete that failed says nothing about
			// whether renewal material is still beside it, and the occasion
			// asked for both to go. The count is raised inside this turn, so
			// no quiescence window opens between the two operations.
			//
			// [[BPR-0547]]. Except on the occasion that decided at its
			// invalidate that the slot outlives this credential, because the
			// enrollment replacing it renews from that same slot.
			if (aRefresh == refresh_disposition::erase) {
				start_refresh_delete(aExpectedOperation);
			} else {
				hold_retained_refresh(aExpectedOperation);
			}
			if (Closing) {
				if (ActiveVaultOperation != vault_operation::delete_credential
					|| !ActiveVaultContext.has_value()
					|| *ActiveVaultContext != aExpectedOperation) {
					ActiveVaultOperation = vault_operation::none;
					ActiveVaultContext.reset();
					DeferredTerminal.reset();
					ShutdownCleanupFailed = true;
					publish_diagnostic(github_host_diagnostic{
						.Reason            = core::github_reason_code::vault_unavailable,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					});
					return;
				}

				if (!valid(aCompletion)
					|| completion_context(aCompletion) != aExpectedOperation) {
					ActiveVaultOperation = vault_operation::none;
					ActiveVaultContext.reset();
					DeferredTerminal.reset();
					ShutdownCleanupFailed = true;
					publish_diagnostic(github_host_diagnostic{
						.Reason            = core::github_reason_code::vault_unavailable,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					});
					return;
				}

				ActiveVaultOperation = vault_operation::none;
				ActiveVaultContext.reset();
				DeferredTerminal.reset();
				if (std::holds_alternative<github_credential_delete_success>(
						aCompletion)) {
					if (CredentialBindingRemembered) {
						CredentialBindings.forget(aExpectedOperation.Authority);
					}
					CredentialAuthority.reset();
					CredentialSessionContext.reset();
					CredentialLifecycle         = credential_lifecycle::none;
					CredentialProvenance        = github_credential_provenance::manual;
					CredentialBindingRemembered = false;
				} else {
					ShutdownCleanupFailed = true;
					publish_diagnostic(github_host_diagnostic{
						.Reason            = core::github_reason_code::vault_unavailable,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					});
				}
				return;
			}
			if (ActiveVaultOperation != vault_operation::delete_credential
				|| !ActiveVaultContext.has_value()
				|| *ActiveVaultContext != aExpectedOperation || !valid(aCompletion)
				|| completion_context(aCompletion) != aExpectedOperation) {
				fail_delete_operation();
				return;
			}
			if (!std::holds_alternative<github_credential_delete_success>(
					aCompletion)) {
				fail_delete_operation();
				return;
			}

			const auto terminal = DeferredTerminal;
			DeferredTerminal.reset();
			ActiveVaultOperation = vault_operation::none;
			ActiveVaultContext.reset();
			if (CredentialBindingRemembered) {
				CredentialBindings.forget(aExpectedOperation.Authority);
			}
			CredentialAuthority.reset();
			CredentialSessionContext.reset();
			CredentialLifecycle         = credential_lifecycle::none;
			CredentialProvenance        = github_credential_provenance::manual;
			CredentialBindingRemembered = false;
			if (terminal.has_value()) {
				finish_deferred_terminal(*terminal);
			} else {
				start_deferred_session();
			}
		}

		void fail_delete_operation()
		{
			const auto terminal = DeferredTerminal;
			DeferredTerminal.reset();
			ActiveVaultOperation = vault_operation::none;
			ActiveVaultContext.reset();
			publish_diagnostic(github_host_diagnostic{
				.Reason            = core::github_reason_code::vault_unavailable,
				.RetryAfterSeconds = std::nullopt,
				.RateLimits        = { },
				.RateLimitCause    = std::nullopt,
			});
			if (terminal.has_value()) {
				finish_deferred_terminal(deferred_session_terminal{
					.Context = terminal->Context,
					.Failure = core::github_failure::vault,
				});
			} else if (DeferredSession.has_value()) {
				const auto context = *DeferredSession;
				DeferredSession.reset();
				fail_session_direct(context, core::github_failure::vault);
			}
		}

		void fail_session(const core::github_session_request_context& aContext,
						  const core::github_failure                  aFailure,
						  const bool                                  aDeletePersistentCredential = false)
		{
			cancel_provider();
			if (CredentialAuthority.has_value()
				&& CredentialSessionContext.has_value()
				&& !AcceptedSession.has_value()
				&& ActiveVaultOperation == vault_operation::none
				&& (CredentialLifecycle == credential_lifecycle::provisional
					|| aDeletePersistentCredential)) {
				start_delete(aContext.IdentityGeneration,
							 deferred_session_terminal{
								 .Context = aContext,
								 .Failure = aFailure,
							 });
				return;
			}
			fail_session_direct(aContext, aFailure);
		}

		void
		fail_session_direct(const core::github_session_request_context& aContext,
							const core::github_failure                  aFailure)
		{
			reset_session_scratch();
			dispatch_core(core::github_session_failed_event{
				.Context = aContext,
				.Failure = aFailure,
			});
		}

		void cancel_session(const core::github_session_request_context& aContext)
		{
			cancel_provider();
			if (CredentialAuthority.has_value()
				&& CredentialSessionContext.has_value()
				&& !AcceptedSession.has_value()
				&& ActiveVaultOperation == vault_operation::none
				&& CredentialLifecycle == credential_lifecycle::provisional) {
				start_delete(aContext.IdentityGeneration,
							 deferred_session_terminal{
								 .Context = aContext,
								 .Failure = std::nullopt,
							 });
				return;
			}
			reset_session_scratch();
			dispatch_core(core::github_session_cancelled_event{
				.Context = aContext,
			});
		}

		void finish_deferred_terminal(const deferred_session_terminal& aTerminal)
		{
			if (aTerminal.Failure.has_value()) {
				fail_session_direct(aTerminal.Context, *aTerminal.Failure);
			} else {
				reset_session_scratch();
				dispatch_core(core::github_session_cancelled_event{
					.Context = aTerminal.Context,
				});
			}
		}

		void reset_session_scratch()
		{
			// [[BPR-0549]]. The window's failure exit. Every session terminal
			// and the orderly shutdown pass through here, and each of them
			// means the session that was going to name an authority never
			// will -- so a slot held for it can no longer be spent by
			// anything, and the erasure that was deferred happens now. It is
			// deliberately not a way to leak material by failing at the right
			// moment.
			erase_retained_refresh();
			PendingProvider = std::monostate{ };
			StagedSession.reset();
			CurrentProfile.reset();
			BootstrapViewer.reset();
			AccountTransport.reset();
			SessionEnrollment.reset();
			SessionEnrollmentProvenance = github_credential_provenance::manual;
			SessionResumeProfile.reset();
			SessionPublicPreview = false;
			ViewerMode           = viewer_mode::none;
		}

		void start_pull_request(const core::pull_request_request_context& aContext)
		{
			// Signed out there is no account transport to hold, and holding a
			// null one is the point rather than an omission. The credential-
			// free authority is the port itself.
			const auto anonymousRead  = is_public_preview();
			const auto transportReady = anonymousRead
				? (AnonymousPullRequests != nullptr && AccountTransport == nullptr)
				: AccountTransport != nullptr;
			if (!AcceptedSession.has_value() || !AcceptedProfileAuthority.has_value()
				|| !AcceptedCapabilityEvidence.has_value() || !transportReady
				|| aContext.Profile != AcceptedProfileAuthority->Profile
				|| aContext.Host != AcceptedProfileAuthority->Host
				|| aContext.Account != AcceptedSession->Viewer.Account
				|| aContext.Capabilities != AcceptedCapabilityEvidence->Revision
				|| AcceptedCapabilityEvidence->Authority.Account != aContext.Account) {
				dispatch_core(core::pull_request_load_failed_event{
					.Context = aContext,
					.Failure = core::github_failure::malformed_response,
				});
				return;
			}
			submit_pull_request(github_pull_request_provider_request{
				.Context            = aContext,
				.ProfileAuthority   = *AcceptedProfileAuthority,
				.Viewer             = AcceptedSession->Viewer,
				.CapabilityEvidence = *AcceptedCapabilityEvidence,
				.AccountTransport   = AccountTransport,
				.Attempt            = github_provider_attempt{ 1U },
				.Mode               = AcceptedSession->Mode,
			});
		}

		// One request names one authority. The mode on the request selects the
		// port, so a signed-out read can never reach the authenticated
		// provider and an authenticated read can never reach the anonymous
		// one, whichever of them a caller composed.
		[[nodiscard]] auto pull_request_port_for(
			const github_pull_request_provider_request& aRequest) noexcept
			-> github_pull_request_port&
		{
			if (aRequest.Mode == core::github_access_mode::public_preview
				&& AnonymousPullRequests != nullptr) {
				return *AnonymousPullRequests;
			}
			return PullRequests;
		}

		void
		submit_pull_request(const github_pull_request_provider_request& aRequest)
		{
			PendingProvider   = aRequest;
			const auto  token = ++ProviderOperationToken;
			const auto  gate  = std::weak_ptr<completion_gate>{ CompletionGate };
			auto* const lane  = &DecisionLane;
			const auto  callback =
				[gate, lane, token, aRequest](
					github_pull_request_completion aCompletion) noexcept {
					const auto callbackTarget = gate.lock();
					if (!callbackTarget) {
						return;
					}
					const std::lock_guard callbackBarrier{ callbackTarget->Barrier };
					if (callbackTarget->Host.load(std::memory_order_acquire) == nullptr) {
						return;
					}
					try {
						auto payload = std::make_shared<github_pull_request_completion>(
							std::move(aCompletion));
						lane->post(
							[gate, payload, token]() mutable {
								const auto locked = gate.lock();
								if (!locked) {
									return;
								}
								const std::lock_guard completionBarrier{
									locked->Barrier
								};
								auto* const host = locked->Host.load(std::memory_order_acquire);
								if (host != nullptr) {
									host->handle_pull_request(token,
															  std::move(*payload));
								}
							});
					} catch (...) {
						try {
							lane->post(
								[gate, token, aRequest]() mutable {
									const auto locked = gate.lock();
									if (!locked) {
										return;
									}
									const std::lock_guard completionBarrier{
										locked->Barrier
									};
									auto* const host = locked->Host.load(std::memory_order_acquire);
									if (host != nullptr) {
										host->handle_pull_request(
											token,
											github_pull_request_error{
												.Request = aRequest,
												.Error   = {
													.Failure = github_provider_failure::
														response_limit,
													.RetryAfterSeconds = std::nullopt,
													.RateLimits        = { },
													.RateLimitCause    = std::nullopt,
												},
											});
									}
								});
						} catch (...) {
						}
					}
				};
			try {
				pull_request_port_for(aRequest).fetch_pull_request(aRequest, callback);
			} catch (...) {
				callback(github_pull_request_error{
					.Request = aRequest,
					.Error   = {
						.Failure           = github_provider_failure::response_limit,
						.RetryAfterSeconds = std::nullopt,
						.RateLimits        = { },
						.RateLimitCause    = std::nullopt,
					},
				});
			}
		}

		void handle_pull_request(const std::uint64_t            aToken,
								 github_pull_request_completion aCompletion)
		{
			run_turn([this, aToken, completion = std::move(aCompletion)]() mutable { handle_pull_request_now(aToken, std::move(completion)); });
		}

		void handle_pull_request_now(const std::uint64_t            aToken,
									 github_pull_request_completion aCompletion)
		{
			if (Closing || aToken != ProviderOperationToken) {
				return;
			}
			const auto* pending = std::get_if<github_pull_request_provider_request>(&PendingProvider);
			if (pending == nullptr) {
				return;
			}
			const auto current = *pending;
			if (aCompletion.valueless_by_exception()
				|| provider_completion_request(aCompletion) != current) {
				PendingProvider = std::monostate{ };
				dispatch_core(core::pull_request_load_failed_event{
					.Context = current.Context,
					.Failure = core::github_failure::malformed_response,
				});
				return;
			}

			if (auto* success = std::get_if<github_pull_request_success>(&aCompletion)) {
				PendingProvider = std::monostate{ };
				const github_actor_record expectedViewer{
					.Actor = success->Request.Viewer.Node,
					.Login = success->Request.Viewer.Login,
				};
				// The mode is the host's own fact about the request it issued,
				// so it is stamped here rather than read back from the
				// provider. A provider cannot promote a read into a mode it
				// was not asked for, and a snapshot cannot arrive claiming
				// one.
				auto observed = success->Snapshot;
				observed.Mode = success->Request.Mode;
				if (observed.Viewer != expectedViewer
					|| observed.CapabilityEvidence
						!= projected_capability_evidence(
							success->Request.CapabilityEvidence)
					|| !AcceptedProfileAuthority.has_value()
					|| !valid_snapshot_rate_limits(*AcceptedProfileAuthority,
												   observed)
					|| !validate(observed, success->Request.Context).valid()) {
					dispatch_core(core::pull_request_load_failed_event{
						.Context = success->Request.Context,
						.Failure = core::github_failure::malformed_response,
					});
					return;
				}
				const core::accepted_snapshot_delta expected{
					.Context     = success->Request.Context,
					.Repository  = observed.Repository.Repository,
					.PullRequest = observed.PullRequest.PullRequest,
					.Snapshot    = observed.Snapshot,
				};
				StagedSnapshot = std::move(observed);
				auto output    = core::transition(
					State,
					core::pull_request_loaded_event{
						.Context     = expected.Context,
						.Repository  = expected.Repository,
						.PullRequest = expected.PullRequest,
						.Snapshot    = expected.Snapshot,
					});
				if (output.AcceptedSnapshot.has_value()
					&& *output.AcceptedSnapshot == expected) {
					AcceptedSnapshot = std::move(StagedSnapshot);
					StagedSnapshot.reset();
					if (!is_public_preview()) {
						// Signed out there is nothing to mutate with. The
						// absence of a lease is the refusal.
						renew_review_mutation_transport();
					}
					static_cast<void>(
						merge(RateLimits, AcceptedSnapshot->RateLimits));
					publish_transition(
						output,
						nullptr,
						&*AcceptedSnapshot);
					deliver_epoch_handoff(*AcceptedSnapshot);
				} else {
					StagedSnapshot.reset();
					publish_and_execute(output);
				}
				return;
			}

			if (auto* error = std::get_if<github_pull_request_error>(&aCompletion)) {
				const auto usesGraphql = error->Request.CapabilityEvidence.ReadShape.ReviewThreads
					|| error->Request.CapabilityEvidence.ReadShape.ViewedFileState;
				const auto errorValid = valid_provider_error_resources(
					error->Error, true, true, usesGraphql);
				if (errorValid) {
					observe_provider_error(error->Error);
				}
				if (error->Request.Mode
						== core::github_access_mode::public_preview
					&& error->Error.Failure == github_provider_failure::rate_limit) {
					// The signed-out budget is the mode's own limit, not a
					// network fault and not a missing credential. It is
					// reported once, with the limit and the reset the server
					// named, and it offers sign-in. Nothing retries past it.
					PendingProvider      = std::monostate{ };
					const auto budget    = error->Error.RateLimitCause;
					PublicPreviewRefusal = github_public_preview_refusal{
						.Cause  = github_public_preview_refusal_cause::policy_refused,
						.Policy = core::refuse_rate_limited(
							core::anonymous_rate_limit_fact{
								.Limit = budget.has_value()
									? static_cast<std::uint32_t>(budget->Limit)
									: 0U,
								.Reset = budget.has_value()
									? budget->ResetUnixSeconds
									: 0U,
							}),
					};
					if (errorValid) {
						publish_error_advisory(error->Error);
					}
					dispatch_core(core::pull_request_load_failed_event{
						.Context = error->Request.Context,
						.Failure = core::github_failure::rate_limit,
					});
					return;
				}
				if (errorValid && retryable(error->Error)
					&& error->Request.Attempt.value()
						< maximum_github_provider_attempts) {
					auto retry    = error->Request;
					retry.Attempt = github_provider_attempt{
						static_cast<std::uint8_t>(retry.Attempt.value() + 1U)
					};
					submit_pull_request(retry);
					return;
				}
				PendingProvider = std::monostate{ };
				if (errorValid) {
					publish_error_advisory(error->Error);
				}
				dispatch_core(core::pull_request_load_failed_event{
					.Context = error->Request.Context,
					.Failure = errorValid
						? mapped_failure(error->Error.Failure)
						: core::github_failure::malformed_response,
				});
				return;
			}

			const auto& cancelled = std::get<github_pull_request_cancelled>(aCompletion);
			PendingProvider       = std::monostate{ };
			dispatch_core(core::pull_request_load_cancelled_event{
				.Context = cancelled.Request.Context,
			});
		}

		void cancel_provider()
		{
			std::visit(
				[this](const auto& aRequest) {
					using request_type = std::decay_t<decltype(aRequest)>;
					if constexpr (std::is_same_v<request_type,
												 github_profile_request>) {
						try {
							Profiles.cancel_profile(aRequest);
						} catch (...) {
						}
					} else if constexpr (std::is_same_v<request_type,
														github_viewer_request>) {
						try {
							Viewers.cancel_viewer(aRequest);
						} catch (...) {
						}
					} else if constexpr (std::is_same_v<request_type,
														github_capability_request>) {
						try {
							Capabilities.cancel_capabilities(aRequest);
						} catch (...) {
						}
					} else if constexpr (std::is_same_v<
											 request_type,
											 github_pull_request_provider_request>) {
						try {
							pull_request_port_for(aRequest).cancel_pull_request(
								aRequest);
						} catch (...) {
						}
					}
				},
				PendingProvider);
			PendingProvider = std::monostate{ };
			++ProviderOperationToken;
		}

		void cancel_vault()
		{
			if (ActiveVaultContext.has_value()) {
				try {
					Credentials.cancel(*ActiveVaultContext);
				} catch (...) {
				}
			}
			++VaultOperationToken;
			ActiveVaultOperation = vault_operation::none;
			ActiveVaultContext.reset();
		}

		// Not cancelled at the vault, deliberately. Only an affirmative answer
		// is ever carried back, so the host cannot tell an outstanding probe
		// from a finished one, and cancelling on that guess would submit a
		// cancellation for work that already ended. There is nothing to
		// reclaim either way: the probe holds no material past its own
		// callback, and the completion gate drops a late one.
		void cancel_refresh_probe()
		{
			++RefreshProbeToken;
			RefreshProbeContext.reset();
			AcceptedCredentialRenewable = false;
		}

		void shutdown()
		{
			if (TurnActive) {
				DeferredIngress.emplace_back(deferred_shutdown{ });
				return;
			}
			run_turn([this] { shutdown_now(); });
		}

		void shutdown_now()
		{
			{
				const std::lock_guard barrier{ CompletionGate->Barrier };
				if (Closing) {
					return;
				}

				if (State.ActiveProfile.has_value()
					|| State.PendingSession.has_value()
					|| State.PendingPullRequest.has_value()) {
					static_cast<void>(core::transition(
						State, core::deactivate_github_identity_intent{ }));
				}

				Closing = true;
			}

			// Wipe raw host-owned material before an external cancellation can
			// block or re-enter. PendingProvider remains intact until its exact
			// cancellation has been submitted.
			NextEnrollment.reset();
			SessionEnrollment.reset();
			SessionEnrollmentProvenance = github_credential_provenance::manual;
			NextResumeProfile.reset();
			SessionResumeProfile.reset();

			// Never call an external port while holding the completion gate.
			// A cancellation implementation may wait for a worker callback that
			// itself needs the gate in order to enqueue its terminal.
			invalidate_epoch_authority();
			cancel_provider();
			cancel_refresh_probe();
			if ((ActiveVaultOperation == vault_operation::store
				 || ActiveVaultOperation == vault_operation::retrieve)
				&& ActiveVaultContext.has_value()) {
				try {
					Credentials.cancel(*ActiveVaultContext);
				} catch (...) {
				}
			} else if (ActiveVaultOperation == vault_operation::delete_credential
					   && ActiveVaultContext.has_value()) {
				// An exact cleanup is already in flight. Its terminal remains the
				// only authority for shutdown quiescence.
			} else if (requires_shutdown_credential_cleanup()) {
				start_delete(CredentialSessionContext->IdentityGeneration,
							 std::nullopt);
			} else {
				cancel_vault();
			}
			static_cast<void>(CapabilityCache.clear());
			DeferredSession.reset();
			revoke_review_mutation_transport();
			reset_session_scratch();
			StagedSnapshot.reset();
			AcceptedSession.reset();
			AcceptedSnapshot.reset();
			AcceptedProfileAuthority.reset();
			AcceptedCapabilityEvidence.reset();
		}

		core::github_state                 State;
		github_profile_port&               Profiles;
		github_viewer_port&                Viewers;
		github_capability_port&            Capabilities;
		github_pull_request_port&          PullRequests;
		github_credential_vault_port&      Credentials;
		github_credential_binding_catalog& CredentialBindings;
		github_capability_cache            CapabilityCache;
		decision_executor&                 DecisionLane;
		github_projection_port&            Projections;
		github_credential_shutdown_policy  CredentialShutdownPolicy = github_credential_shutdown_policy::retain;
		github_epoch_handoff_port*         Epochs                   = nullptr;
		github_pull_request_port*          AnonymousPullRequests    = nullptr;
		git_smart_transport_factory_port*  AnonymousSmartGit        = nullptr;
		public_preview_projection_port*    PublicPreview            = nullptr;
		public_preview_sign_in_sink        PublicPreviewSignIn;
		// The profile the signed-out session was activated for, kept so a
		// sign-in gesture names the host the reviewer is already reading
		// rather than whichever one a later picker change would resolve to.
		core::host_profile_id PublicPreviewProfile{ };
		// The last presentation delivered. Publication is by change, so the
		// four occasions the port promises are the occasions something
		// actually differs on, and a repeated observation is not a repeated
		// announcement.
		std::optional<public_preview_presentation> PublishedPublicPreview;
		std::shared_ptr<completion_gate>           CompletionGate;
		std::deque<deferred_ingress>               DeferredIngress;
		bool                                       TurnActive               = false;
		bool                                       DeferredIngressScheduled = false;

		pending_provider_request                            PendingProvider;
		std::uint64_t                                       ProviderOperationToken = 0U;
		std::optional<github_credential_material>           NextEnrollment;
		github_credential_provenance                        NextEnrollmentProvenance = github_credential_provenance::manual;
		std::optional<core::host_profile_id>                NextEnrollmentProfile;
		std::optional<github_credential_material>           SessionEnrollment;
		github_credential_provenance                        SessionEnrollmentProvenance = github_credential_provenance::manual;
		std::optional<core::host_profile_id>                NextResumeProfile;
		std::optional<core::host_profile_id>                SessionResumeProfile;
		bool                                                NextPublicPreview    = false;
		bool                                                SessionPublicPreview = false;
		std::optional<github_public_preview_refusal>        PublicPreviewRefusal;
		std::optional<core::github_session_request_context> DeferredSession;
		std::optional<github_profile_authority>             CurrentProfile;
		std::optional<github_viewer_record>                 BootstrapViewer;
		viewer_mode                                         ViewerMode = viewer_mode::none;
		github_account_transport                            AccountTransport;

		vault_operation                                    ActiveVaultOperation = vault_operation::none;
		std::optional<github_credential_operation_context> ActiveVaultContext;
		std::uint64_t                                      VaultOperationToken = 0U;
		github_provider_attempt                            PendingRetrieveAttempt{ 1U };
		// The renewability probe runs beside the vault operation state rather
		// than inside it. It answers a question about a session that has
		// already been accepted, so it can neither advance nor abandon one,
		// and giving it its own token keeps the single-operation invariant the
		// establishment path depends on exactly as it was.
		std::optional<github_credential_operation_context> RefreshProbeContext;
		std::uint64_t                                      RefreshProbeToken = 0U;
		// The refresh slot's erasure runs beside the credential delete rather
		// than inside it, for the same reason the probe does: the
		// single-operation invariant the establishment path depends on is
		// about the credential slot. It is counted rather than tracked by
		// context because the only question anything asks of it is whether
		// the vault still owes this host a terminal, and quiescence is asked
		// from outside the decision lane. Decremented under the completion
		// gate's barrier, which is the same place every other vault terminal
		// establishes that this host is still alive.
		std::atomic<std::uint32_t>                     OutstandingRefreshDeletes   = 0U;
		bool                                           AcceptedCredentialRenewable = false;
		std::optional<github_credential_authority_key> CredentialAuthority;
		std::optional<core::github_session_request_context>
												 CredentialSessionContext;
		credential_lifecycle                     CredentialLifecycle         = credential_lifecycle::none;
		github_credential_provenance             CredentialProvenance        = github_credential_provenance::manual;
		bool                                     CredentialBindingRemembered = false;
		std::optional<deferred_session_terminal> DeferredTerminal;
		std::optional<core::github_identity_generation>
			 DeferredInvalidationGeneration;
		bool DeferredInvalidationPreservesCredential = false;
		bool DeferredInvalidationRetainsRefresh      = false;
		// [[BPR-0549]]. The outgoing refresh slot whose erasure the retention
		// above deferred, carried as the exact context that would have erased
		// it until the incoming session's authority is known.
		std::optional<github_credential_operation_context>
			RetainedRefreshOperation;

		std::optional<session_candidate>                   StagedSession;
		std::optional<github_pull_request_snapshot>        StagedSnapshot;
		std::optional<github_session_read_model>           AcceptedSession;
		std::optional<github_profile_authority>            AcceptedProfileAuthority;
		std::optional<github_capability_cache_entry>       AcceptedCapabilityEvidence;
		std::optional<github_pull_request_snapshot>        AcceptedSnapshot;
		review_mutation_transport                          ReviewMutationTransport;
		github_rate_limit_state                            RateLimits;
		std::uint64_t                                      LogicalEpoch               = 0U;
		bool                                               Closing                    = false;
		bool                                               ShutdownCleanupFailed      = false;
		bool                                               ProjectionDeliveryFailed   = false;
		bool                                               EpochHandoffDeliveryFailed = false;
		bool                                               EpochHandoffUnavailable    = false;
		std::optional<pull_request_authority_invalidation> ActiveEpochAuthority;
		std::shared_ptr<smart_authority_state>             ActiveEpochSmartAuthority;
	};

	github_application_host::github_application_host(
		core::github_state                      aInitialState,
		github_profile_port&                    aProfiles,
		github_viewer_port&                     aViewers,
		github_capability_port&                 aCapabilities,
		github_pull_request_port&               aPullRequests,
		github_credential_vault_port&           aCredentials,
		github_credential_binding_catalog&      aCredentialBindings,
		decision_executor&                      aDecisionLane,
		github_projection_port&                 aProjections,
		const github_credential_shutdown_policy aCredentialShutdownPolicy,
		github_epoch_handoff_port* const        aEpochs,
		github_pull_request_port* const         aAnonymousPullRequests,
		git_smart_transport_factory_port* const aAnonymousSmartGit,
		public_preview_projection_port* const   aPublicPreview)
		: Implementation{ std::make_unique<implementation>(std::move(aInitialState),
														   aProfiles,
														   aViewers,
														   aCapabilities,
														   aPullRequests,
														   aCredentials,
														   aCredentialBindings,
														   aDecisionLane,
														   aProjections,
														   aCredentialShutdownPolicy,
														   aEpochs,
														   aAnonymousPullRequests,
														   aAnonymousSmartGit,
														   aPublicPreview) }
	{
	}

	github_application_host::~github_application_host()
	{
		shutdown();
	}

	void github_application_host::dispatch(const core::github_event& aEvent)
	{
		Implementation->dispatch(aEvent);
	}

	void github_application_host::enroll_profile(
		const core::host_profile_id        aProfile,
		github_credential_material         aCredential,
		const github_credential_provenance aProvenance)
	{
		Implementation->enroll_profile(
			aProfile, std::move(aCredential), aProvenance);
	}

	void github_application_host::resume_profile(
		const core::host_profile_id aProfile)
	{
		Implementation->resume_profile(aProfile);
	}

	void github_application_host::activate_public_preview(
		const core::host_profile_id aProfile)
	{
		Implementation->activate_public_preview(aProfile);
	}

	void github_application_host::shutdown()
	{
		Implementation->shutdown();
	}

	auto github_application_host::state() const noexcept
		-> const core::github_state&
	{
		return Implementation->State;
	}

	auto github_application_host::accepted_session() const noexcept
		-> const std::optional<github_session_read_model>&
	{
		return Implementation->AcceptedSession;
	}

	auto github_application_host::accepted_snapshot() const noexcept
		-> const std::optional<github_pull_request_snapshot>&
	{
		return Implementation->AcceptedSnapshot;
	}

	auto github_application_host::accepted_credential_authority() const noexcept
		-> std::optional<github_credential_authority_key>
	{
		// Gated on acceptance rather than on the key existing. A key is minted
		// while a session is still being established and is dropped again if
		// any later step refuses it, so answering from the key alone would
		// name a slot for a session that never happened.
		if (!Implementation->AcceptedSession.has_value()) {
			return std::nullopt;
		}
		return Implementation->CredentialAuthority;
	}

	auto github_application_host::accepted_credential_renewable() const noexcept
		-> bool
	{
		if (!Implementation->AcceptedSession.has_value()) {
			return false;
		}
		return Implementation->AcceptedCredentialRenewable;
	}

	auto github_application_host::accepted_identity_generation() const noexcept
		-> core::github_identity_generation
	{
		if (!Implementation->AcceptedSession.has_value()) {
			return { };
		}
		return Implementation->State.IdentityGeneration;
	}

	auto github_application_host::accepted_review_mutation_transport() const noexcept
		-> review_mutation_transport
	{
		const auto& host = *Implementation;
		// Signed out there is no transport at all: none was ever acquired,
		// and this reports its absence rather than an inactive lease.
		if (host.signed_out_mode() || host.Closing
			|| host.ReviewMutationTransport == nullptr
			|| !host.ReviewMutationTransport->active()
			|| !host.AcceptedSession.has_value()
			|| !host.AcceptedProfileAuthority.has_value()
			|| !host.AcceptedSnapshot.has_value()) {
			return { };
		}
		const auto& session   = *host.AcceptedSession;
		const auto& authority = *host.AcceptedProfileAuthority;
		const auto& snapshot  = *host.AcceptedSnapshot;
		if (session.Profile != authority.Profile
			|| session.Host != authority.Host
			|| snapshot.Profile != session.Profile
			|| snapshot.Host != session.Host
			|| snapshot.Account != session.Viewer.Account
			|| snapshot.Viewer.Actor != session.Viewer.Node
			|| snapshot.Viewer.Login != session.Viewer.Login) {
			return { };
		}
		return host.ReviewMutationTransport;
	}

	auto github_application_host::rate_limits() const noexcept
		-> const github_rate_limit_state&
	{
		return Implementation->RateLimits;
	}

	auto github_application_host::capability_cache_size() const noexcept
		-> std::size_t
	{
		return Implementation->CapabilityCache.size();
	}

	auto github_application_host::is_closing() const noexcept -> bool
	{
		return Implementation->Closing;
	}

	auto github_application_host::is_quiescent() const noexcept -> bool
	{
		return Implementation->Closing
			&& std::holds_alternative<std::monostate>(
				   Implementation->PendingProvider)
			&& Implementation->ActiveVaultOperation
			== implementation::vault_operation::none
			// [[BPR-0540]]. The refresh slot's erasure is part of the exact
			// cleanup, so composition keeps this host and the vault alive
			// until the vault has answered for it too.
			&& Implementation->OutstandingRefreshDeletes.load(
				   std::memory_order_acquire)
			== 0U;
	}

	auto github_application_host::shutdown_cleanup_failed() const noexcept -> bool
	{
		return Implementation->ShutdownCleanupFailed;
	}

	auto github_application_host::projection_delivery_failed() const noexcept
		-> bool
	{
		return Implementation->ProjectionDeliveryFailed;
	}

	auto github_application_host::epoch_handoff_delivery_failed() const noexcept
		-> bool
	{
		return Implementation->EpochHandoffDeliveryFailed;
	}

	auto github_application_host::access_mode() const noexcept
		-> core::github_access_mode
	{
		return Implementation->State.AccessMode;
	}

	void github_application_host::set_public_preview_sign_in(
		public_preview_sign_in_sink aSignIn)
	{
		Implementation->PublicPreviewSignIn = std::move(aSignIn);
	}

	void github_application_host::request_public_preview_sign_in()
	{
		// The gesture is a handoff, not a read: it is honoured while closing
		// and after a refusal, because a refusal that could not be acted on
		// would be a dead end rather than an offer.
		if (!Implementation->PublicPreviewSignIn) {
			return;
		}
		Implementation->PublicPreviewSignIn(Implementation->PublicPreviewProfile);
	}

	auto github_application_host::public_preview_refusal() const noexcept
		-> const std::optional<github_public_preview_refusal>&
	{
		return Implementation->PublicPreviewRefusal;
	}

	auto github_application_host::public_preview_legality(
		const core::anonymous_read_intent aIntent) const noexcept
		-> core::anonymous_intent_legality
	{
		return core::legality(Implementation->State.AccessMode, aIntent);
	}

	auto github_application_host::epoch_handoff_unavailable() const noexcept
		-> bool
	{
		return Implementation->EpochHandoffUnavailable;
	}
}
