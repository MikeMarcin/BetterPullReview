#include "bpr/application/github_application_host.hpp"

#include "github_projection_mapping.hpp"

#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <array>
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
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::application {
	namespace {
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

		[[nodiscard]] auto credential_key(const core::bpr_profile_id aBprProfile, const github_profile_authority& aProfile,
										  const core::github_account_id aAccount)
			-> github_credential_authority_key
		{
			return {
				.Profile         = aProfile.Profile,
				.ProfileRevision = aProfile.ProfileRevision,
				.Host            = aProfile.Host,
				.Hostname        = aProfile.Hostname,
				.Account         = aAccount,
				.ServerContract  = aProfile.ServerContract,
				.BprProfile      = aBprProfile,
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
			core::github_session_request_context  Context;
			std::optional<core::github_failure>   Failure;
			github_bootstrap_failure_stage        Stage = github_bootstrap_failure_stage::none;
			std::optional<github_host_diagnostic> Diagnostic;
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

		struct staged_enrollment
		{
			core::pending_setup_destination                                   Pending;
			std::uint64_t                                                     Attempt;
			core::github_identity_generation                                  OriginGeneration;
			github_credential_material                                        Credential;
			std::function<bool(const core::pending_setup_destination&, bool)> Accept;
			github_credential_provenance                                      Provenance;
			std::optional<device_binding_lifetime>                            Lifetime;
			bool                                                              RefreshSettled;
			std::variant<github_profile_request, github_viewer_request>       Operation;
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
			std::optional<device_binding_lifetime> Lifetime;
			bool                                   RefreshSettled = false;
		};
		struct deferred_grant_invalidation
		{
			github_credential_authority_key  Authority;
			core::github_identity_generation Generation;
			core::github_failure             Failure = core::github_failure::authentication;
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
											  deferred_grant_invalidation,
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
					   public_preview_projection_port* aPublicPreview, core::bpr_profile_id aBprProfile)
			: State{ std::move(aInitialState) }
			, Profiles{ aProfiles }
			, Viewers{ aViewers }
			, Capabilities{ aCapabilities }
			, PullRequests{ aPullRequests }
			, Credentials{ aCredentials }
			, CredentialBindings{ aCredentialBindings }
			, BprProfile{ aBprProfile }
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
			if (BprProfile.value() == 0U) {
				throw std::invalid_argument("BPR profile admission requires a nonzero identity");
			}
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
							} else if constexpr (std::is_same_v<ingress_type, deferred_grant_invalidation>) {
								invalidate_device_grant_now(aIngress.Authority, aIngress.Generation, aIngress.Failure);
							} else if constexpr (std::is_same_v<ingress_type,
																deferred_enrollment>) {
								enroll_profile_now(aIngress.Profile,
												   std::move(aIngress.Credential),
												   aIngress.Provenance, aIngress.Lifetime, aIngress.RefreshSettled);
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
				fail_withdrawn_bindings();
				TurnActive = false;
				schedule_deferred_ingress();
			} catch (...) {
				DeferredIngress.clear();
				TurnActive = false;
				// [[BPR-1112]]. A turn that ends in an exception still fails the
				// requests the lane withdrew during it, before the exception
				// reaches its caller. A later turn may never come: after
				// `Closing` almost nothing starts one.
				fail_withdrawn_bindings();
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
			cancel_setup_enrollment();
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

		void invalidate_device_grant_now(const github_credential_authority_key& aAuthority, const core::github_identity_generation aGeneration, const core::github_failure aFailure)
		{
			if (owns_launch_renewal(aAuthority, aGeneration)) {
				end_launch_renewal(aFailure);
				return;
			}
			if (!Closing && AcceptedSession && CredentialAuthority == aAuthority && CredentialSessionContext
				&& CredentialSessionContext->IdentityGeneration == aGeneration
				&& CredentialProvenance == github_credential_provenance::device_grant) {
				end_accepted_renewal(aFailure);
			}
		}

		// [[BPR-1116]]. An accepted session's renewal that gives up -- out of
		// attempts on the network, unable to reach the credential store, or
		// refused -- keeps its grant, as a launch renewal does ([[BPR-0644]]).
		// The owner's rule of 2026-09-24: a remembered grant is removed only
		// when the person removes it or a new sign-in replaces it. The session
		// ends visibly instead. It is resumed from its own remembered binding,
		// which the invalidation that goes with a resume leaves in place, and
		// the resumed session fails at once with the renewal's failure. So the
		// row and both slots stay, the credential stays selected for Sign out,
		// and the card says how the grant ends. The device host's chain has
		// ended, and is released as a launch renewal's is: left to report
		// `reenrollment_required`, its sign-in row would stand where the card
		// offers Sign out beside Sign in.
		void end_accepted_renewal(const core::github_failure aFailure)
		{
			release_launch_renewal();
			NextKeptEnding = kept_session_ending{ .Failure = aFailure, .Durable = DeviceBindingDurable };
			try {
				resume_profile_now(CredentialAuthority->Profile);
			} catch (...) {
				NextKeptEnding.reset();
				throw;
			}
			NextKeptEnding.reset();
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
			cancel_setup_enrollment();
			AcceptedSetupBootstrap.reset();
			if (const auto* open = std::get_if<core::open_pull_request_intent>(&aEvent)) {
				read(*open);
				return;
			}
			if (const auto* refresh = std::get_if<core::refresh_pull_request_intent>(&aEvent)) {
				read(*refresh);
				return;
			}
			dispatch_core(aEvent);
		}

		void enroll_profile(const core::host_profile_id                  aProfile,
							github_credential_material                   aCredential,
							const github_credential_provenance           aProvenance,
							const std::optional<device_binding_lifetime> aLifetime,
							const bool                                   aRefreshSettled)
		{
			cancel_setup_enrollment();
			AcceptedSetupBootstrap.reset();
			if (Closing) {
				return;
			}
			if (TurnActive) {
				DeferredIngress.emplace_back(deferred_enrollment{
					.Profile        = aProfile,
					.Credential     = std::move(aCredential),
					.Provenance     = aProvenance,
					.Lifetime       = aLifetime,
					.RefreshSettled = aRefreshSettled,
				});
				return;
			}
			if (DeferredIngressScheduled || !DeferredIngress.empty()) {
				DeferredIngress.emplace_back(deferred_enrollment{
					.Profile        = aProfile,
					.Credential     = std::move(aCredential),
					.Provenance     = aProvenance,
					.Lifetime       = aLifetime,
					.RefreshSettled = aRefreshSettled,
				});
				schedule_deferred_ingress();
				return;
			}
			run_turn([this, aProfile, aProvenance, aLifetime, aRefreshSettled, credential = std::move(aCredential)]() mutable { enroll_profile_now(aProfile, std::move(credential), aProvenance, aLifetime, aRefreshSettled); });
		}

		void enroll_profile_now(const core::host_profile_id                  aProfile,
								github_credential_material                   aCredential,
								const github_credential_provenance           aProvenance,
								const std::optional<device_binding_lifetime> aLifetime,
								const bool                                   aRefreshSettled)
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
				NextDeviceLifetime       = aLifetime;
				NextRefreshSettled       = aRefreshSettled;
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

		void cancel_setup_enrollment()
		{
			if (!StagedEnrollment) {
				return;
			}
			const auto operation = StagedEnrollment->Operation;
			StagedEnrollment.reset();
			try {
				std::visit([this](const auto& aRequest) {
					if constexpr (std::is_same_v<std::decay_t<decltype(aRequest)>, github_profile_request>) {
						Profiles.cancel_profile(aRequest);
					} else {
						Viewers.cancel_viewer(aRequest);
					}
				},
						   operation);
			} catch (...) { }
		}

		template <typename TCompletion, typename THandler>
		auto setup_callback(THandler aHandler) -> std::function<void(TCompletion)>
		{
			++OutstandingSetup;
			const auto gate      = std::weak_ptr<completion_gate>{ CompletionGate };
			auto*      lane      = &DecisionLane;
			const auto delivered = std::make_shared<std::atomic<bool>>(false);
			return [gate, lane, delivered, handler = std::move(aHandler)](TCompletion aCompletion) mutable {
				if (delivered->exchange(true)) {
					return;
				}
				const auto locked = gate.lock();
				if (!locked) {
					return;
				}
				const std::lock_guard barrier{ locked->Barrier };
				if (locked->Host.load() == nullptr) {
					return;
				}
				const auto payload = std::make_shared<TCompletion>(std::move(aCompletion));
				lane->post([gate, payload, laneHandler = std::move(handler)]() mutable {
					const auto target = gate.lock();
					if (!target) {
						return;
					}
					const std::lock_guard callbackBarrier{ target->Barrier };
					auto*                 host = target->Host.load();
					if (!host) {
						return;
					}
					--host->OutstandingSetup;
					host->run_turn([&] { laneHandler(*host, std::move(*payload)); });
				});
			};
		}

		void finish_setup_failure(const core::setup_request_id aRequest)
		{
			if (!StagedEnrollment || StagedEnrollment->Pending.Request != aRequest) {
				return;
			}
			auto staged = std::move(*StagedEnrollment);
			StagedEnrollment.reset();
			if (!Closing && staged.Accept) {
				static_cast<void>(staged.Accept(staged.Pending, false));
			}
		}

		void stage_setup_enrollment(core::pending_setup_destination aPending, github_credential_material aCredential,
									std::function<bool(const core::pending_setup_destination&, bool)> aAccept,
									const github_credential_provenance aProvenance, std::optional<device_binding_lifetime> aLifetime,
									const bool aRefreshSettled)
		{
			cancel_setup_enrollment();
			if (Closing || !aPending.Profile || !valid(aCredential) || !aAccept
				|| NextSetupAttempt >= (std::uint64_t{ 1U } << 63U)) {
				return;
			}
			const auto attempt = NextSetupAttempt++;
			// Setup requests use the upper request namespace and their own callback slot.
			// They cannot cancel an ordinary review operation with the same request number.
			const auto request = github_profile_request{
				.Context = { .Request            = core::request_id{ (std::uint64_t{ 1U } << 63U) | attempt },
							 .ApplicationSession = State.ApplicationSession,
							 .Profile            = *aPending.Profile,
							 .IdentityGeneration = core::github_identity_generation{ State.IdentityGeneration.value() + 1U } },
			};
			const auto identity = aPending.Request;
			StagedEnrollment.emplace(staged_enrollment{ std::move(aPending), attempt, State.IdentityGeneration,
														std::move(aCredential), std::move(aAccept), aProvenance, std::move(aLifetime), aRefreshSettled, request });
			auto callback = setup_callback<github_profile_completion>([identity, request, attempt](implementation& aHost, github_profile_completion aCompletion) {
				if (!aHost.StagedEnrollment || aHost.StagedEnrollment->Pending.Request != identity || aHost.StagedEnrollment->Attempt != attempt) {
					return;
				}
				auto* success = std::get_if<github_profile_success>(&aCompletion);
				if (aHost.Closing || aHost.State.IdentityGeneration != aHost.StagedEnrollment->OriginGeneration
					|| !success || success->Request != request || !valid(success->Authority)
					|| success->Authority.Profile != request.Context.Profile
					|| success->Authority.Hostname.value() != aHost.StagedEnrollment->Pending.Destination.Hostname
					|| success->Authority.Authentication != github_authentication_transport::native_https) {
					aHost.finish_setup_failure(identity);
					return;
				}
				aHost.stage_setup_viewer(identity, success->Authority, request.Context);
			});
			try {
				Profiles.resolve_profile(request, callback);
			} catch (...) {
				callback(github_profile_cancelled{ .Request = request });
			}
		}

		void stage_setup_viewer(const core::setup_request_id aIdentity, const github_profile_authority& aAuthority,
								const core::github_session_request_context& aContext)
		{
			auto credential = github_credential_material::create(StagedEnrollment->Credential.bytes());
			if (!credential) {
				finish_setup_failure(aIdentity);
				return;
			}
			const auto attempt          = StagedEnrollment->Attempt;
			const auto request          = github_viewer_request{ .Context = aContext, .Authority = aAuthority };
			StagedEnrollment->Operation = request;
			auto callback               = setup_callback<github_viewer_completion>([aIdentity, request, attempt](implementation& aHost, github_viewer_completion aCompletion) {
				if (!aHost.StagedEnrollment || aHost.StagedEnrollment->Pending.Request != aIdentity || aHost.StagedEnrollment->Attempt != attempt) {
					return;
				}
				auto* success = std::get_if<github_viewer_success>(&aCompletion);
				if (aHost.Closing || aHost.State.IdentityGeneration != aHost.StagedEnrollment->OriginGeneration
					|| !success || success->Request != request || success->AccountTransport
					|| !valid_viewer(request.Authority, request.Context, success->Viewer, success->RateLimits)) {
					aHost.finish_setup_failure(aIdentity);
					return;
				}
				const auto pending  = aHost.StagedEnrollment->Pending;
				const auto accept   = aHost.StagedEnrollment->Accept;
				const auto accepted = accept(pending, true);
				if (!aHost.StagedEnrollment || aHost.StagedEnrollment->Pending != pending || aHost.StagedEnrollment->Attempt != attempt) {
					return;
				}
				if (!accepted || aHost.State.IdentityGeneration != aHost.StagedEnrollment->OriginGeneration) {
					aHost.StagedEnrollment.reset();
					return;
				}
				auto staged = std::move(*aHost.StagedEnrollment);
				aHost.StagedEnrollment.reset();
				aHost.AcceptedSetupBootstrap = *success;
				aHost.enroll_profile_now(*pending.Profile, std::move(staged.Credential), staged.Provenance, staged.Lifetime, staged.RefreshSettled);
			});
			try {
				Viewers.observe_viewer({ .Request = request, .Authentication = github_bootstrap_credential{ .Credential = std::move(*credential) } }, callback);
			} catch (...) {
				callback(github_viewer_cancelled{ .Request = request });
			}
		}

		void resume_profile(const core::host_profile_id aProfile)
		{
			cancel_setup_enrollment();
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
			if (Closing || aProfile.value() == 0U || (CredentialBindings.find(aProfile) != nullptr && CredentialBindings.find(aProfile)->BprProfile != BprProfile)) {
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
			cancel_setup_enrollment();
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
			if (Closing || aProfile.value() == 0U || (CredentialBindings.find(aProfile) != nullptr && CredentialBindings.find(aProfile)->BprProfile != BprProfile)) {
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
			// [[BPR-0822]]. Name the nested type: GCC 15 destroys a braced
			// aggregate member twice when a later member initializer throws.
			accept_session(session_candidate{
				.Context            = aContext,
				.ProfileAuthority   = aAuthority,
				.Viewer             = github_viewer_record{ },
				.CapabilityEvidence = anonymous_capability_evidence(aAuthority),
				.AccountTransport   = { },
				.RateLimits         = RateLimits,
				.CacheStage         = std::nullopt,
				.Mode               = core::github_access_mode::public_preview,
			});
		}

		auto current_profile_session() const -> std::optional<github_profile_session>
		{
			if (!AcceptedSession || !AcceptedProfileAuthority || !AcceptedCapabilityEvidence) {
				return std::nullopt;
			}
			return github_profile_session{ BprProfile, State.ApplicationSession, *AcceptedProfileAuthority, *AcceptedSession,
										   State.IdentityGeneration, *AcceptedCapabilityEvidence, AccountTransport };
		}
		void retire_windows()
		{
			if (!PublishedSession) {
				return;
			}
			if (ReadWindow) {
				static_cast<void>(ReadWindow->retire(*PublishedSession));
			}
			for (auto& slot : AttachedWindows) {
				if (slot.Host->retire(*PublishedSession)) {
					// Only exact retirement authorizes this live attachment to bind
					// the profile owner's next accepted authentication identity.
					slot.Authority.reset();
					slot.Account = { };
				}
			}
			PublishedSession.reset();
		}
		void invalidate_epoch_authority() noexcept { retire_windows(); }
		void revoke_review_mutation_transport() noexcept { ReviewMutationTransport.reset(); }
		void read(const core::github_window_read_event& aEvent)
		{
			if (PrimaryReadClosed) {
				return;
			}
			const auto session = current_profile_session();
			if (!session) {
				std::visit([this](const auto& aTyped) { dispatch_core(core::github_event{ aTyped }); }, aEvent);
				return;
			}
			if (!ReadWindow && (ExplicitPrimaryAttached || AttachedWindows.size() >= 32U)) {
				return;
			}
			if (!ReadWindow) {
				ReadWindow = std::make_shared<github_window_read_host>(
					core::bpr_window_context{ BprProfile, core::bpr_window_id{ 1 }, core::bpr_window_generation{ 1 } },
					State.ApplicationSession, PullRequests, DecisionLane,
					[this](const github_window_read_update& aUpdate) {
						if (Closing) {
							return;
						}
						State                   = ReadWindow->state();
						AcceptedSnapshot        = ReadWindow->accepted_snapshot();
						ReviewMutationTransport = ReadWindow->mutation_transport();
						static_cast<void>(merge(RateLimits, ReadWindow->rate_limits()));
						if (aUpdate.PublicPreviewRefusal) {
							PublicPreviewRefusal = aUpdate.PublicPreviewRefusal;
						}
						if (aUpdate.ProviderError) {
							publish_error_advisory(*aUpdate.ProviderError);
						}
						publish_transition(aUpdate.Transition, nullptr,
										   aUpdate.Transition.AcceptedSnapshot && AcceptedSnapshot ? &*AcceptedSnapshot : nullptr);
					},
					Epochs, AnonymousPullRequests, AnonymousSmartGit);
			}
			if (!ReadWindow->adopt(*session)) {
				return;
			}
			PublishedSession = session;
			ReadWindow->advance_request_floor(State.NextRequest);
			ReadWindow->dispatch(aEvent);
		}
		struct window_slot
		{
			core::bpr_window_context                 Context;
			core::application_session_id             Session;
			std::optional<github_profile_authority>  Authority;
			core::github_account_id                  Account;
			std::shared_ptr<github_window_read_host> Host;
			bool                                     Detaching = false;
		};
		bool attach_window(const std::shared_ptr<github_window_read_host>& aWindow)
		{
			std::erase_if(AttachedWindows, [](const auto& aSlot) { return (aSlot.Detaching && aSlot.Host->is_quiescent()); });
			const auto session = current_profile_session();
			if (Closing || !aWindow || aWindow->window().Profile != BprProfile
				|| aWindow->window().Window.value() == 0U || aWindow->window().Generation.value() == 0U || aWindow->state().ApplicationSession.value() == 0U
				|| (!session && aWindow->state().ActiveProfile.has_value())
				|| AttachedWindows.size() + (ReadWindow ? 1U : 0U) >= 32U
				|| aWindow->state().ApplicationSession == State.ApplicationSession) {
				return false;
			}
			const auto restored = aWindow->window().Generation.value() > 1U;
			if (restored) {
				if (RestorationSealed || ReadWindow || RestoredWindowCount == RestoredWindows.size()
					|| std::ranges::find(RestoredWindows, aWindow->window().Window) != RestoredWindows.end()) {
					return false;
				}
			} else if (aWindow->window().Window.value() <= LastAttachedWindow.value()) {
				return false;
			}
			for (const auto& slot : AttachedWindows) {
				if (slot.Context.Window == aWindow->window().Window || slot.Session == aWindow->state().ApplicationSession) {
					return false;
				}
			}
			if (session && !aWindow->adopt(*session)) {
				return false;
			}
			AttachedWindows.push_back({ aWindow->window(), aWindow->state().ApplicationSession,
										session ? std::optional{ session->ProfileAuthority } : std::nullopt,
										session ? session->Session.Viewer.Account : core::github_account_id{ }, aWindow });
			ExplicitPrimaryAttached = ExplicitPrimaryAttached || aWindow->window().Window == core::bpr_window_id{ 1 };
			LastAttachedWindow      = std::max(LastAttachedWindow, aWindow->window().Window);
			if (restored) {
				RestoredWindows[RestoredWindowCount++] = aWindow->window().Window;
			} else {
				RestorationSealed = true;
			}
			if (session) {
				PublishedSession = session;
			}
			return true;
		}
		void observe_attached_read(const std::shared_ptr<github_window_read_host>& aWindow, const bool aFreshSnapshot)
		{
			if (Closing || !aWindow) {
				return;
			}
			const auto attached = std::ranges::find_if(AttachedWindows, [&aWindow](const auto& aSlot) { return aSlot.Host == aWindow && !aSlot.Detaching; });
			if (attached == AttachedWindows.end()) {
				return;
			}
			AcceptedSnapshot = aWindow->accepted_snapshot();
			static_cast<void>(merge(RateLimits, aWindow->rate_limits()));
			publish_public_preview();
			if (!aFreshSnapshot || Closing || SharedSnapshotSequence == std::numeric_limits<std::uint64_t>::max()) {
				return;
			}
			// Copy the bounded attachment set before publishing to any sibling:
			// its callback may detach itself or another window.
			std::vector<std::shared_ptr<github_window_read_host>> windows;
			windows.reserve(AttachedWindows.size());
			for (const auto& slot : AttachedWindows) {
				if (!slot.Detaching) {
					windows.push_back(slot.Host);
				}
			}
			aWindow->share_snapshot(windows, ++SharedSnapshotSequence);
		}
		void detach_window(const core::bpr_window_context aContext)
		{
			const auto attached = std::ranges::find_if(AttachedWindows, [aContext](const auto& aSlot) { return aSlot.Context == aContext; });
			if (attached != AttachedWindows.end()) {
				attached->Host->shutdown();
				attached->Detaching = true;
				std::erase_if(AttachedWindows, [](const auto& aSlot) { return aSlot.Detaching && aSlot.Host->is_quiescent(); });
				return;
			}
			if (!ExplicitPrimaryAttached && aContext == core::bpr_window_context{ BprProfile, core::bpr_window_id{ 1 }, core::bpr_window_generation{ 1 } }) {
				if (ReadWindow) {
					ReadWindow->shutdown();
				}
				PrimaryReadClosed   = true;
				PublicPreview       = nullptr;
				PublicPreviewSignIn = { };
				Epochs              = nullptr;
				AcceptedSnapshot.reset();
				ReviewMutationTransport.reset();
			}
		}

		void publish_profile_session()
		{
			const auto session = current_profile_session();
			if (!session) {
				return;
			}
			PublishedSession = session;
			if (ReadWindow && !PrimaryReadClosed) {
				static_cast<void>(ReadWindow->adopt(*session));
			}
			for (auto& slot : AttachedWindows) {
				if (slot.Detaching || (slot.Authority && (*slot.Authority != session->ProfileAuthority || slot.Account != session->Session.Viewer.Account))) {
					continue;
				}
				if (slot.Host->adopt(*session) && !slot.Authority) {
					slot.Authority = session->ProfileAuthority;
					slot.Account   = session->Session.Viewer.Account;
				}
			}
		}

		void dispatch_core(const core::github_event& aEvent, std::optional<github_host_diagnostic_projection> aFailureDiagnostic = std::nullopt)
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
			publish_and_execute(output, std::move(aFailureDiagnostic));
		}

		void publish_and_execute(const core::github_transition_output& aOutput, std::optional<github_host_diagnostic_projection> aFailureDiagnostic = std::nullopt)
		{
			if (Closing) {
				return;
			}
			for (const auto& request : aOutput.Requests) {
				if (std::holds_alternative<core::invalidate_github_identity_request>(request)) {
					retire_windows();
				}
			}
			publish_transition(aOutput, nullptr, nullptr, std::move(aFailureDiagnostic));
			for (const auto& request : aOutput.Requests) {
				execute(request);
			}
		}

		void publish_transition(
			const core::github_transition_output&            aOutput,
			const github_session_read_model* const           aSession           = nullptr,
			const github_pull_request_snapshot* const        aPullRequest       = nullptr,
			std::optional<github_host_diagnostic_projection> aFailureDiagnostic = std::nullopt) noexcept
		{
			try {
				std::optional<github_session_projection> sessionProjection;
				if (aSession != nullptr) {
					sessionProjection = project_github_session(*aSession, grant_projection());
				}
				std::optional<github_pull_request_projection>
					pullRequestProjection;
				if (aPullRequest != nullptr) {
					pullRequestProjection = project_github_pull_request(*aPullRequest);
				}
				std::vector<github_host_diagnostic_projection> diagnostics;
				// Only a core-accepted failure receives the call site's stage. A stale
				// terminal cannot publish context onto a replacement session.
				if (aFailureDiagnostic
					&& aOutput.Status == core::github_status_delta{ .Status = core::github_status::session_failed }
					&& !aOutput.Diagnostics.empty()) {
					aFailureDiagnostic->Reason = aOutput.Diagnostics.front().Code;
					diagnostics.push_back(std::move(*aFailureDiagnostic));
				}
				const github_presentation_update update{
					.Transition      = project_github_transition(aOutput),
					.Session         = std::move(sessionProjection),
					.PullRequest     = std::move(pullRequestProjection),
					.HostDiagnostics = std::move(diagnostics),
				};
				if (!PrimaryReadClosed) {
					Projections.publish(update);
				}
			} catch (...) {
				ProjectionDeliveryFailed = true;
			}
			publish_public_preview();
		}

		void publish_diagnostic(const github_host_diagnostic& aDiagnostic, const github_bootstrap_failure_stage aStage = github_bootstrap_failure_stage::none) noexcept
		{
			try {
				auto diagnostic           = project_github_host_diagnostic(aDiagnostic);
				diagnostic.BootstrapStage = aStage;
				const github_presentation_update update{
					.Transition      = std::nullopt,
					.Session         = std::nullopt,
					.PullRequest     = std::nullopt,
					.HostDiagnostics = { std::move(diagnostic) },
				};
				if (!PrimaryReadClosed) {
					Projections.publish(update);
				}
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
					delete_retained_on_shutdown
				|| (CredentialShutdownPolicy == github_credential_shutdown_policy::retain_accepted_device_grants && !DeviceBindingDurable);
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
						DeviceLifetime              = std::exchange(NextDeviceLifetime, std::nullopt);
						DeviceRefreshSettled        = std::exchange(NextRefreshSettled, false);
						SessionEnrollmentProvenance = NextEnrollmentProvenance;
						NextEnrollment.reset();
						NextEnrollmentProvenance = github_credential_provenance::manual;
						NextEnrollmentProfile.reset();
						SessionResumeProfile = NextResumeProfile;
						SessionKeptEnding    = std::exchange(NextKeptEnding, std::nullopt);
						auto beginsRenewal   = false;
						auto endsRenewal     = false;
						if (PendingDeviceRestore && SessionResumeProfile == PendingDeviceRestore->Authority.Profile) {
							if (PendingDeviceRestoreRenews) {
								// [[BPR-0644]]. The stored access token is past its
								// lifetime, so this session waits for the renewal
								// rather than presenting it to `/user`. One that
								// cannot renew at all -- its refresh lifetime is
								// gone, or nothing is composed to renew through --
								// is held the same way and ended at once, so the
								// reviewer is told it needs signing in again.
								LaunchRenewal         = device_launch_renewal{ .Authority = PendingDeviceRestore->Authority, .IdentityGeneration = aTypedRequest.Context.IdentityGeneration, .Lifetime = PendingDeviceRestore->Lifetime };
								LaunchRenewalEnrolled = false;
								LaunchRenewalBegun    = !PendingDeviceRestoreExpired;
								beginsRenewal         = !PendingDeviceRestoreExpired;
								endsRenewal           = PendingDeviceRestoreExpired;
							} else {
								DeviceLifetime        = PendingDeviceRestore->Lifetime;
								DeviceRefreshSettled  = true;
								DeviceBindingDurable  = true;
								RestoredDeviceBinding = true;
							}
						}
						PendingDeviceRestore.reset();
						PendingDeviceRestoreRenews  = false;
						PendingDeviceRestoreExpired = false;
						NextResumeProfile.reset();
						SessionPublicPreview = NextPublicPreview;
						NextPublicPreview    = false;
						if (ActiveVaultOperation == vault_operation::none) {
							start_deferred_session();
						}
						if (beginsRenewal) {
							begin_launch_renewal();
						}
						if (endsRenewal) {
							// Ended on its own turn, like a sink that refused:
							// this runs inside the transition that admitted the
							// held session.
							DeferredIngress.emplace_back(deferred_grant_invalidation{ LaunchRenewal->Authority, LaunchRenewal->IdentityGeneration, core::github_failure::authentication });
							schedule_deferred_ingress();
						}
					} else {
						static_cast<void>(aTypedRequest);
					}
				},
				aRequest);
		}

		void invalidate(const core::invalidate_github_identity_request& aRequest)
		{
			if (BindingLoadPending) {
				PendingRestoreCleanup = aRequest;
			}
			// [[BPR-0644]]. The rotated grant's own enrollment -- a device grant
			// for the renewal's profile whose refresh slot is already settled,
			// which a new device flow's first enrollment never is -- carries the
			// renewal on to its `/user` account check. Anything else that
			// replaces the held or enrolling session -- Sign out, another
			// enrollment, a preview -- abandons it: the device host is told so
			// that no rotated token is enrolled over that choice, and the
			// renewal's exact slot becomes the credential this invalidate erases
			// below, so no spent or rotated material stays behind the tombstone.
			if (LaunchRenewal) {
				const auto own = !LaunchRenewalEnrolled && NextEnrollment.has_value() && NextEnrollmentProvenance == github_credential_provenance::device_grant
					&& NextEnrollmentProfile == LaunchRenewal->Authority.Profile && NextRefreshSettled;
				if (own) {
					LaunchRenewalEnrolled = true;
				} else {
					const auto renewal = *LaunchRenewal;
					LaunchRenewal.reset();
					LaunchRenewalEnrolled = false;
					if (std::exchange(LaunchRenewalBegun, false)) {
						release_launch_renewal();
					}
					if (!CredentialAuthority.has_value()) {
						adopt_launch_renewal_slot(renewal.Authority, core::github_session_request_context{ .Request = aRequest.Request, .ApplicationSession = aRequest.ApplicationSession, .Profile = renewal.Authority.Profile, .IdentityGeneration = aRequest.NewGeneration });
					}
				}
			}
			++BindingGeneration;
			DeviceBindingDurable  = false;
			RestoredDeviceBinding = false;
			LaunchRenewedGrant    = false;
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
			SessionKeptEnding.reset();
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

		auto select_persistent_credential(
			const github_credential_authority_key&      aAuthority,
			const core::github_session_request_context& aContext) -> bool
		{
			if (aAuthority.BprProfile != BprProfile) {
				return false;
			}
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
			return true;
		}

		void start_deferred_session()
		{
			if (Closing || !DeferredSession.has_value()
				|| ActiveVaultOperation != vault_operation::none || BindingErasePending || BindingLoadPending || StartupCleanupOperation || (LaunchRenewal && !LaunchRenewalEnrolled)) {
				return;
			}

			const auto context = *DeferredSession;
			DeferredSession.reset();
			if (const auto* binding = CredentialBindings.find(context.Profile); binding != nullptr && binding->BprProfile != BprProfile) {
				fail_session(context, core::github_failure::vault);
				return;
			}
			if (BindingLoadUnresolved) {
				// An unread binding may still describe retained vault material.
				// Refuse replacement for this lifetime instead of overwriting it.
				binding_failure();
				fail_session(context, core::github_failure::vault);
				return;
			}
			if (SessionKeptEnding.has_value()) {
				// [[BPR-1116]]. The session an accepted renewal gave up on,
				// resumed only to fail: its binding is selected again, as a
				// restored one whose row is live when it was durable, so the
				// failure keeps it and the card offers Sign out beside Sign in.
				const auto  ending = *std::exchange(SessionKeptEnding, std::nullopt);
				const auto* kept   = CredentialBindings.find(context.Profile);
				if (kept != nullptr && select_persistent_credential(*kept, context)) {
					RestoredDeviceBinding = ending.Durable;
					DeviceBindingDurable  = ending.Durable;
				}
				fail_session(context, ending.Failure);
				return;
			}
			if (SessionResumeProfile.has_value()) {
				if (const auto* retained = CredentialBindings.find(*SessionResumeProfile);
					retained != nullptr) {
					if (!select_persistent_credential(*retained, context)) {
						fail_session(context, core::github_failure::vault);
						return;
					}
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
						if (!select_persistent_credential(*retained, context)) {
							fail_session(context, core::github_failure::vault);
							return;
						}
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
						if (!select_persistent_credential(*retained, context)) {
							fail_session(context, core::github_failure::vault);
							return;
						}
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
				if (!select_persistent_credential(resumeAuthority, context)) {
					fail_session(context, core::github_failure::vault);
					return;
				}
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
			if (AcceptedSetupBootstrap) {
				auto accepted = std::move(*AcceptedSetupBootstrap);
				AcceptedSetupBootstrap.reset();
				if (accepted.Request.Authority != aRequest.Authority) {
					fail_session(aRequest.Context, core::github_failure::malformed_response);
					return;
				}
				accepted.Request = aRequest;
				PendingProvider  = aRequest;
				const auto token = ++ProviderOperationToken;
				handle_viewer_now(token, std::move(accepted));
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
							 core::github_failure::malformed_response, false, github_bootstrap_failure_stage::account_identity);
				return;
			}

			if (auto* success = std::get_if<github_viewer_success>(&aCompletion)) {
				const auto context   = success->Request.Context;
				const auto authority = success->Request.Authority;
				PendingProvider      = std::monostate{ };
				if (!valid_viewer(
						authority, context, success->Viewer, success->RateLimits)) {
					fail_session(context, core::github_failure::malformed_response, false, github_bootstrap_failure_stage::account_identity);
					return;
				}

				if (ViewerMode == viewer_mode::bootstrap) {
					if (success->AccountTransport != nullptr) {
						fail_session(context,
									 core::github_failure::malformed_response, false, github_bootstrap_failure_stage::account_identity);
						return;
					}
					static_cast<void>(merge(RateLimits, success->RateLimits));
					BootstrapViewer    = success->Viewer;
					const auto adopted = credential_key(BprProfile, authority, success->Viewer.Account);
					// [[BPR-0644]]. A renewed grant resumes only the account its
					// binding named. Any other account's token is refused before
					// it reaches the vault.
					if (LaunchRenewal && LaunchRenewalEnrolled) {
						const auto renewal = *LaunchRenewal;
						LaunchRenewal.reset();
						LaunchRenewalEnrolled = false;
						LaunchRenewalBegun    = false;
						if (adopted != renewal.Authority) {
							// The owner's decision of 2026-09-24: material that
							// signs in as an account the binding never named is
							// kept like every other failure, not erased on its
							// own. The renewal's slot stays the remembered grant,
							// the failure says the account differs, and Sign out
							// or a new sign-in is what removes it.
							SessionEnrollment.reset();
							SessionEnrollmentProvenance = github_credential_provenance::manual;
							release_launch_renewal();
							keep_launch_renewal_slot(renewal.Authority, context);
							fail_session(context, core::github_failure::malformed_response, false, github_bootstrap_failure_stage::account_identity, account_mismatch());
							return;
						}
						// Until it is accepted, a failure keeps this grant as the
						// remembered one rather than erasing it as provisional.
						LaunchRenewedGrant = true;
					}
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
					fail_session(context, core::github_failure::malformed_response, false, github_bootstrap_failure_stage::account_identity);
					return;
				}
				if (ViewerMode == viewer_mode::account) {
					if (!CredentialAuthority.has_value()
						|| success->Viewer.Account != CredentialAuthority->Account) {
						// [[BPR-0644]]. A remembered device grant whose `/user`
						// answers another account is kept, as the owner decided
						// on 2026-09-24, and the failure says the account
						// differs. Anything else is erased as before: a
						// provisional credential never became anyone's
						// remembered sign-in.
						fail_session(context,
									 core::github_failure::malformed_response,
									 !remembered_device_grant(), github_bootstrap_failure_stage::account_identity,
									 CredentialAuthority.has_value() ? std::optional{ account_mismatch() } : std::nullopt);
						return;
					}
					if (BootstrapViewer.has_value()
						&& success->Viewer != *BootstrapViewer) {
						fail_session(context,
									 core::github_failure::malformed_response, false, github_bootstrap_failure_stage::account_identity);
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
								 : core::github_failure::malformed_response,
							 false, github_bootstrap_failure_stage::account_identity,
							 errorValid ? std::optional{ github_host_diagnostic{
											  .Reason            = core::reason_for(mapped_failure(error->Error.Failure)),
											  .RetryAfterSeconds = error->Error.RetryAfterSeconds,
											  .RateLimits        = RateLimits,
											  .RateLimitCause    = error->Error.RateLimitCause,
											  // [[BPR-0644]]. The provider's own account
											  // check refused a stored credential that
											  // answered as another account.
											  .AccountMismatch = error->Error.AccountMismatch,
										  } }
										: std::nullopt);
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
				fail_capability_session(aContext, core::github_failure::malformed_response, github_capability_diagnostic::host_epoch);
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
				fail_capability_session(aContext, core::github_failure::malformed_response, github_capability_diagnostic::host_cache_lookup);
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
				fail_capability_session(current.Context,
										core::github_failure::malformed_response, github_capability_diagnostic::host_correlation);
				return;
			}

			if (auto* success = std::get_if<github_capability_success>(&aCompletion)) {
				PendingProvider  = std::monostate{ };
				const auto epoch = next_epoch();
				if (!epoch.has_value()
					|| !valid_capability_rate_limits(
						success->Request.ProfileAuthority, success->RateLimits)) {
					fail_capability_session(success->Request.Context,
											core::github_failure::malformed_response, !epoch.has_value() ? github_capability_diagnostic::host_epoch : github_capability_diagnostic::host_rate_state);
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
					fail_capability_session(
						success->Request.Context,
						staged.Error
								== github_capability_cache_error::capacity_exhausted
							? core::github_failure::response_too_large
							: core::github_failure::malformed_response,
						github_capability_diagnostic::host_cache_stage);
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
				std::optional<github_host_diagnostic> diagnostic;
				if (errorValid) {
					diagnostic = github_host_diagnostic{
						.Reason            = core::reason_for(mapped_failure(error->Error.Failure)),
						.RetryAfterSeconds = error->Error.RetryAfterSeconds,
						.RateLimits        = RateLimits,
						.RateLimitCause    = error->Error.RateLimitCause,
					};
				}
				fail_capability_session(error->Request.Context,
										errorValid ? mapped_failure(error->Error.Failure) : core::github_failure::malformed_response,
										errorValid ? bounded_capability_diagnostic(error->CapabilityDetail) : github_capability_diagnostic::host_provider_error,
										std::move(diagnostic));
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
				[gate, lane, token, probe, reportAbsent = RestoredDeviceBinding](
					github_credential_retrieve_completion aCompletion) noexcept {
					auto* const retrieved = std::get_if<
						github_credential_retrieve_success>(&aCompletion);
					const auto present = retrieved != nullptr && valid(aCompletion) && completion_context(aCompletion) == probe;
					if (retrieved == nullptr && !reportAbsent) {
						return;
					}
					if (retrieved != nullptr) {
						retrieved->Credential.reset();
					}
					const auto callbackTarget = gate.lock();
					if (!callbackTarget) {
						return;
					}
					const std::lock_guard callbackBarrier{ callbackTarget->Barrier };
					if (callbackTarget->Host.load(std::memory_order_acquire) == nullptr) {
						return;
					}
					try {
						lane->post([gate, token, probe, present]() {
							const auto locked = gate.lock();
							if (!locked) {
								return;
							}
							const std::lock_guard completionBarrier{
								locked->Barrier
							};
							auto* const host = locked->Host.load(std::memory_order_acquire);
							if (host != nullptr) {
								host->handle_refresh_probe(token, probe, present);
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
			const github_credential_operation_context& aExpectedProbe, const bool aPresent)
		{
			run_turn([this, aToken, aExpectedProbe, aPresent]() {
				handle_refresh_probe_now(aToken, aExpectedProbe, aPresent);
			});
		}

		void handle_refresh_probe_now(
			const std::uint64_t                        aToken,
			const github_credential_operation_context& aExpectedProbe, const bool aPresent)
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
			if (!aPresent) {
				// [[BPR-1116]]. A restored grant whose refresh slot is missing
				// is not signed out and erased: its access token still works,
				// and the owner's rule of 2026-09-24 is that a remembered grant
				// is removed only when the person removes it. The session stays
				// and nothing is armed to renew it, so the card now says it
				// does not renew itself. Once its access lifetime has gone, the
				// next launch's renewal meets the missing material as a refusal
				// and keeps the grant, saying it needs signing in again.
				if (RestoredDeviceBinding) {
					RefreshSlotMissing = true;
					republish_accepted_session();
				}
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
		// [[BPR-0660]]. The grant facts the card states, from what the host
		// already holds: the provenance, the device lifetime and whether the
		// binding is durable. No credential leaves here.
		[[nodiscard]] auto grant_projection() const noexcept
			-> std::optional<github_grant_projection>
		{
			if (!AcceptedSession.has_value()) {
				return std::nullopt;
			}
			if (CredentialProvenance != github_credential_provenance::device_grant) {
				return github_grant_projection{ .Kind = github_grant_kind_projection::token };
			}
			return github_grant_projection{
				.Kind       = github_grant_kind_projection::device_grant,
				.SignedInAt = DeviceLifetime.has_value() && DeviceLifetime->IssuedAt > 0
					? std::optional<std::int64_t>{ DeviceLifetime->IssuedAt }
					: std::nullopt,
				.Renews     = DeviceLifetime.has_value() && DeviceLifetime->RefreshExpiresAt > 0 && !RefreshSlotMissing,
				.Remembered = DeviceBindingDurable,
			};
		}

		void republish_accepted_session() noexcept
		{
			if (!AcceptedSession.has_value()) {
				return;
			}
			try {
				const github_presentation_update update{
					.Transition      = std::nullopt,
					.Session         = project_github_session(*AcceptedSession, grant_projection()),
					.PullRequest     = std::nullopt,
					.HostDiagnostics = { },
				};
				if (!PrimaryReadClosed) {
					Projections.publish(update);
				}
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
							 core::github_failure::malformed_response, false, github_bootstrap_failure_stage::session_acceptance);
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
				keep_renewed_launch_grant(aCandidate.Context);
				publish_and_execute(output, github_host_diagnostic_projection{ .BootstrapStage = github_bootstrap_failure_stage::session_acceptance, .GrantKept = remembered_grant_kept() });
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
									   },
									   github_bootstrap_failure_stage::session_acceptance);
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
			LaunchRenewedGrant = false;
			persist_settled_device_binding();
			publish_profile_session();
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
				fail_session(aContext, core::github_failure::vault, false, github_bootstrap_failure_stage::local_enrollment);
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
								 core::github_failure::vault, false, github_bootstrap_failure_stage::local_enrollment);
				}
				return;
			}
			if (!valid(aCompletion)
				|| completion_context(aCompletion) != aExpectedOperation) {
				ActiveVaultOperation = vault_operation::none;
				ActiveVaultContext.reset();
				if (CredentialSessionContext.has_value()) {
					fail_session(*CredentialSessionContext,
								 core::github_failure::vault, false, github_bootstrap_failure_stage::local_enrollment);
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
			fail_session(session, core::github_failure::vault, false, github_bootstrap_failure_stage::local_enrollment);
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
			if (RestoredDeviceBinding) {
				// [[BPR-0644]]. A locked or refused keychain, a missing access
				// slot and an unreadable one all keep the remembered grant, under
				// the owner's rule of 2026-09-24: the launch says the credential
				// store failed, and the next launch tries again. Nothing resumes
				// from it meanwhile, and Sign out still erases both slots.
				fail_session(context, core::github_failure::vault);
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
							if (target->StartupCleanupOperation == aOperation) {
								target->run_turn([target] {
									target->StartupCleanupOperation.reset();
									target->start_deferred_session();
								});
							}
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

		void start_delete_unblocked(const core::github_identity_generation   aGeneration,
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
			if (StartupCleanupOperation && StartupCleanupOperation->Authority == operation.Authority) {
				// A newer invalidation may have superseded the generation while the
				// tombstone was queued. Wait for the operation actually submitted.
				StartupCleanupOperation = operation;
			}
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

		void fail_capability_session(
			const core::github_session_request_context& aContext,
			const core::github_failure                  aFailure,
			const github_capability_diagnostic          aDetail,
			std::optional<github_host_diagnostic>       aDiagnostic = std::nullopt)
		{
			auto diagnostic             = aDiagnostic.value_or(github_host_diagnostic{ });
			diagnostic.CapabilityDetail = bounded_capability_diagnostic(aDetail);
			fail_session(aContext, aFailure, false,
						 github_bootstrap_failure_stage::capability_discovery, std::move(diagnostic));
		}

		// [[BPR-0644]]. A launch renewal's rotated grant that fails before it is
		// accepted is the remembered grant, not a provisional one: its row was
		// never tombstoned and its refresh slot already holds the rotated
		// material, so the next launch renews from it. Before its `/user`
		// named the account there is no selected credential yet, and the
		// renewal's own slot is taken; after, the selected one is converted.
		// Either way the device host's chain is released.
		void keep_renewed_launch_grant(const core::github_session_request_context& aContext)
		{
			if (AcceptedSession.has_value()) {
				return;
			}
			if (LaunchRenewal && LaunchRenewalEnrolled && !CredentialAuthority.has_value()) {
				const auto renewal = *LaunchRenewal;
				LaunchRenewal.reset();
				LaunchRenewalEnrolled = false;
				LaunchRenewalBegun    = false;
				release_launch_renewal();
				keep_launch_renewal_slot(renewal.Authority, aContext);
				return;
			}
			if (std::exchange(LaunchRenewedGrant, false) && CredentialAuthority.has_value() && CredentialSessionContext.has_value()) {
				release_launch_renewal();
				CredentialLifecycle   = credential_lifecycle::persistent;
				RestoredDeviceBinding = true;
				DeviceBindingDurable  = true;
			}
		}

		// [[BPR-0644]]. A failure before acceptance erases a provisional
		// credential, and a persistent one only when its caller asks: a
		// restored device binding is no longer erased because its session
		// failed. The owner's rule of 2026-09-24 is that a remembered grant is
		// removed only when the person removes it or a new sign-in replaces
		// it. The callers that still ask are the ones whose material is not
		// the binding's: a profile the binding never named. An account it never
		// named is kept too, since the owner's decision of 2026-09-24.
		void fail_session(const core::github_session_request_context& aContext,
						  const core::github_failure                  aFailure,
						  const bool                                  aDeletePersistentCredential = false,
						  const github_bootstrap_failure_stage        aStage                      = github_bootstrap_failure_stage::none,
						  std::optional<github_host_diagnostic>       aDiagnostic                 = std::nullopt)
		{
			cancel_provider();
			if (!aDeletePersistentCredential) {
				keep_renewed_launch_grant(aContext);
			}
			if (CredentialAuthority.has_value()
				&& CredentialSessionContext.has_value()
				&& !AcceptedSession.has_value()
				&& ActiveVaultOperation == vault_operation::none
				&& ((CredentialLifecycle == credential_lifecycle::provisional && !RestoredDeviceBinding)
					|| aDeletePersistentCredential)) {
				start_delete(aContext.IdentityGeneration,
							 deferred_session_terminal{
								 .Context    = aContext,
								 .Failure    = aFailure,
								 .Stage      = aStage,
								 .Diagnostic = std::move(aDiagnostic),
							 });
				return;
			}
			fail_session_direct(aContext, aFailure, aStage, std::move(aDiagnostic));
		}

		void
		fail_session_direct(const core::github_session_request_context&  aContext,
							const core::github_failure                   aFailure,
							const github_bootstrap_failure_stage         aStage      = github_bootstrap_failure_stage::none,
							const std::optional<github_host_diagnostic>& aDiagnostic = std::nullopt)
		{
			// [[BPR-0644]]. Said with the failure, so the card can offer Sign
			// out for a grant that is still on this machine.
			const auto kept = remembered_grant_kept();
			reset_session_scratch();
			auto diagnostic           = aDiagnostic ? project_github_host_diagnostic(*aDiagnostic) : github_host_diagnostic_projection{ };
			diagnostic.BootstrapStage = aStage;
			diagnostic.GrantKept      = kept;
			dispatch_core(core::github_session_failed_event{
							  .Context = aContext,
							  .Failure = aFailure,
						  },
						  aStage == github_bootstrap_failure_stage::none && !kept ? std::nullopt : std::optional{ std::move(diagnostic) });
		}

		// [[BPR-0644]]. What a failure says when its credential signed in as an
		// account other than the one its binding named.
		[[nodiscard]] static auto account_mismatch() -> github_host_diagnostic
		{
			return github_host_diagnostic{ .Reason = core::reason_for(core::github_failure::malformed_response), .AccountMismatch = true };
		}

		// [[BPR-0644]]. The selected credential is a device grant someone
		// signed in with before this session: restored or resumed from its
		// binding, or a launch renewal's rotated grant. A failure keeps it.
		[[nodiscard]] auto remembered_device_grant() const noexcept -> bool
		{
			return CredentialAuthority.has_value() && CredentialSessionContext.has_value() && !AcceptedSession.has_value()
				&& CredentialProvenance == github_credential_provenance::device_grant
				&& (CredentialLifecycle == credential_lifecycle::persistent || LaunchRenewedGrant);
		}

		// A restored device grant that failed before acceptance and was not
		// erased: its material and its row are still on this machine.
		[[nodiscard]] auto remembered_grant_kept() const noexcept -> bool
		{
			return RestoredDeviceBinding && CredentialAuthority.has_value() && CredentialSessionContext.has_value()
				&& !AcceptedSession.has_value() && CredentialLifecycle == credential_lifecycle::persistent
				&& CredentialProvenance == github_credential_provenance::device_grant;
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
				fail_session_direct(aTerminal.Context, *aTerminal.Failure, aTerminal.Stage, aTerminal.Diagnostic);
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
			SessionKeptEnding.reset();
			SessionPublicPreview = false;
			ViewerMode           = viewer_mode::none;
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
							PullRequests.cancel_pull_request(
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
			RefreshSlotMissing          = false;
		}

		void shutdown()
		{
			RestoreIngressClosing = true;
			cancel_setup_enrollment();
			if (TurnActive) {
				DeferredIngress.emplace_back(deferred_shutdown{ });
				return;
			}
			run_turn([this] { shutdown_now(); });
		}

		void shutdown_now()
		{
			cancel_setup_enrollment();
			AcceptedSetupBootstrap.reset();
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

			if (Persistence != nullptr && !(BindingLoadPending && PendingRestoreCleanup)) {
				Persistence->cancel_pending();
			}
			LaunchRenewal.reset();
			LaunchRenewalEnrolled = false;
			LaunchRenewalBegun    = false;
			LaunchRenewedGrant    = false;

			// Wipe raw host-owned material before an external cancellation can
			// block or re-enter. PendingProvider remains intact until its exact
			// cancellation has been submitted.
			NextEnrollment.reset();
			SessionEnrollment.reset();
			SessionEnrollmentProvenance = github_credential_provenance::manual;
			NextResumeProfile.reset();
			SessionResumeProfile.reset();
			SessionKeptEnding.reset();

			// Never call an external port while holding the completion gate.
			// A cancellation implementation may wait for a worker callback that
			// itself needs the gate in order to enqueue its terminal.
			if (ReadWindow) {
				ReadWindow->shutdown();
			}
			for (auto& slot : AttachedWindows) {
				slot.Host->shutdown();
				slot.Detaching = true;
			}
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
			} else if (BindingErasePending
					   || (ActiveVaultOperation == vault_operation::delete_credential && ActiveVaultContext.has_value())) {
				// The durable barrier or exact vault cleanup already owns the
				// terminal required for shutdown quiescence.
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

		// [[BPR-1116]]. How an accepted renewal that gave up ends its session:
		// the renewal's failure, and whether the binding's row was durable.
		struct kept_session_ending
		{
			core::github_failure Failure = core::github_failure::authentication;
			bool                 Durable = false;
		};

		struct restore_completion
		{
			std::function<void(device_profile_restore_status)> Callback;
			bool                                               Scheduled = false;
		};

		void complete_restore(const std::shared_ptr<restore_completion>& aCompletion,
							  const device_profile_restore_status        aStatus) noexcept
		{
			if (!aCompletion || aCompletion->Scheduled) {
				return;
			}
			aCompletion->Scheduled = true;
			if (!aCompletion->Callback) {
				--OutstandingRestoreCompletions;
				return;
			}
			try {
				DecisionLane.post([gate = std::weak_ptr<completion_gate>{ CompletionGate }, aCompletion, aStatus] {
					const auto locked = gate.lock();
					if (!locked) {
						return;
					}
					const std::lock_guard barrier{ locked->Barrier };
					auto*                 host = locked->Host.load(std::memory_order_acquire);
					if (!host) {
						return;
					}
					auto callback = std::move(aCompletion->Callback);
					try {
						host->run_turn([&] {
							try {
								callback(host->Closing ? device_profile_restore_status::cancelled : aStatus);
							} catch (...) {
								host->ProjectionDeliveryFailed = true;
							}
						});
					} catch (...) {
						host->ProjectionDeliveryFailed = true;
					}
					--host->OutstandingRestoreCompletions;
				});
			} catch (...) {
				--OutstandingRestoreCompletions;
				ProjectionDeliveryFailed = true;
			}
		}

		template <typename TCallback>
		auto binding_callback(TCallback aCallback)
		{
			return [gate = std::weak_ptr<completion_gate>{ CompletionGate }, callback = std::move(aCallback)](auto aResult) mutable {
				const auto locked = gate.lock();
				if (!locked || locked->Host.load(std::memory_order_acquire) == nullptr) {
					return;
				}
				auto* host = locked->Host.load(std::memory_order_acquire);
				host->run_turn([&callback, &aResult] { callback(std::move(aResult)); });
			};
		}

		// [[BPR-1048]]. Calls the persistence lane with `aFinish` as the
		// request's terminal. The caller has already counted the request, and
		// may have set a pending flag. The lane throws only when it withdrew the
		// request: it could post neither the effect nor its terminal, and no
		// terminal follows ([[BPR-0884]]). A withdrawal is a failure, never a
		// success ([[BPR-0541]]), so `aFinish` gets the terminal a refused
		// effect gets, `unavailable`, and undoes the count and the flag itself.
		// The throw is not passed on: the request has had its one terminal.
		template <typename TCall>
		void call_binding_lane(TCall&& aCall, std::function<void(device_binding_result)> aFinish)
		{
			try {
				std::forward<TCall>(aCall)(binding_callback(aFinish));
			} catch (...) {
				fail_withdrawn_binding(std::move(aFinish));
			}
		}

		// Inside a turn the failure waits for the turn's end, where the lane's
		// own terminal would have found it, or for the turn's exception
		// ([[BPR-1112]]). A turn can set up after the call what that failure
		// decides by: a replacement's session is recorded after the
		// invalidation that asked for the tombstone. Outside a turn the call is
		// the only work in progress, so it fails now. Either way a throwing
		// terminal goes no further, as on the lane.
		void fail_withdrawn_binding(std::function<void(device_binding_result)> aFinish)
		{
			if (TurnActive && WithdrawnBindingCount < WithdrawnBindings.size()) {
				WithdrawnBindings.at(WithdrawnBindingCount++).emplace(std::move(aFinish));
				return;
			}
			try {
				aFinish(std::unexpected(device_binding_failure::unavailable));
			} catch (...) {
			}
		}

		// A failure may make another request that is withdrawn in turn. It
		// joins the queue and is failed before the turn ends. As on the lane,
		// a throwing terminal cannot stop the ones behind it or end the turn.
		void fail_withdrawn_bindings()
		{
			while (WithdrawnBindingCount != 0U) {
				auto finish = std::move(*WithdrawnBindings.front());
				std::move(WithdrawnBindings.begin() + 1, WithdrawnBindings.begin() + static_cast<std::ptrdiff_t>(WithdrawnBindingCount), WithdrawnBindings.begin());
				WithdrawnBindings.at(--WithdrawnBindingCount).reset();
				try {
					finish(std::unexpected(device_binding_failure::unavailable));
				} catch (...) {
				}
			}
		}

		void binding_failure()
		{
			DeviceBindingDurable = false;
			publish_diagnostic(github_host_diagnostic{ .Reason = core::github_reason_code::vault_unavailable, .RetryAfterSeconds = std::nullopt, .RateLimits = { }, .RateLimitCause = std::nullopt });
		}

		// [[BPR-0644]]. The renewal itself is the device host's existing
		// rotation: it asks `prepare_device_rotation`, which writes no
		// tombstone for a launch renewal, and ends by enrolling the rotated
		// access token or by invalidating this exact grant.
		void begin_launch_renewal()
		{
			const auto renewal = *LaunchRenewal;
			try {
				LaunchRenewalSink(renewal);
			} catch (...) {
				// Ended on its own turn: this runs inside the transition that
				// admitted the held session. Nothing was asked of the server or
				// the grant, so it is tried again at the next launch.
				DeferredIngress.emplace_back(deferred_grant_invalidation{ renewal.Authority, renewal.IdentityGeneration, core::github_failure::vault });
				schedule_deferred_ingress();
			}
		}

		// [[BPR-0644]]. A renewal that cannot finish -- refused, out of
		// attempts, unable to reach the credential store, or never able to
		// start -- keeps its grant. That is the owner's rule of 2026-09-24: a
		// remembered grant is removed only when the person removes it or a new
		// sign-in replaces it. No tombstone was written for the renewal, so its
		// row stays live and the next launch tries again. The renewal's exact
		// slot becomes the selected credential, so Sign out erases both slots
		// through the ordered path. The held session fails with the failure the
		// renewal named, which every startup observer already treats as a
		// terminal, and the device sign-in is offered beside it.
		void end_launch_renewal(const core::github_failure aFailure)
		{
			if (!DeferredSession || CredentialAuthority.has_value()) {
				// Unreachable while the session is held. Nothing is adopted
				// here, so no other credential's slot can be named.
				DeviceBindingDurable = false;
				LaunchRenewal.reset();
				LaunchRenewalBegun = false;
				dispatch_now(core::deactivate_github_identity_intent{ });
				return;
			}
			const auto renewal = *LaunchRenewal;
			const auto context = *DeferredSession;
			const auto begun   = std::exchange(LaunchRenewalBegun, false);
			LaunchRenewal.reset();
			DeferredSession.reset();
			// The device host's chain has ended; released, it neither renews nor
			// asks for a new code in this process. A renewal that never began
			// gave it nothing to release.
			if (begun) {
				release_launch_renewal();
			}
			keep_launch_renewal_slot(renewal.Authority, context);
			fail_session(context, aFailure);
		}

		// The renewal's grant, kept: selected as a restored binding whose row
		// is still live, so a failure keeps it, an orderly quit keeps it, and
		// Sign out erases it.
		void keep_launch_renewal_slot(const github_credential_authority_key& aAuthority, const core::github_session_request_context& aContext)
		{
			adopt_launch_renewal_slot(aAuthority, aContext);
			RestoredDeviceBinding = true;
			DeviceBindingDurable  = true;
		}

		// The renewal's exact slot becomes the credential this host erases.
		// Only ever the renewal's own authority, and only while no other
		// credential is selected, so nothing a replacing grant owns is named.
		void adopt_launch_renewal_slot(const github_credential_authority_key& aAuthority, const core::github_session_request_context& aContext)
		{
			CredentialAuthority         = aAuthority;
			CredentialSessionContext    = aContext;
			CredentialLifecycle         = credential_lifecycle::persistent;
			CredentialProvenance        = github_credential_provenance::device_grant;
			CredentialBindingRemembered = false;
		}

		// Releases whatever renewal chain the device host holds: a launch
		// renewal's, or an accepted session's that gave up ([[BPR-1116]]).
		void release_launch_renewal() noexcept
		{
			try {
				LaunchRenewalSink(std::nullopt);
			} catch (...) {
			}
		}

		[[nodiscard]] auto owns_launch_renewal(const github_credential_authority_key& aAuthority, const core::github_identity_generation aGeneration) const noexcept -> bool
		{
			return !Closing && LaunchRenewal.has_value() && !LaunchRenewalEnrolled && LaunchRenewal->Authority == aAuthority && LaunchRenewal->IdentityGeneration == aGeneration;
		}

		void persist_settled_device_binding()
		{
			if (Closing || Persistence == nullptr || !DeviceRefreshSettled || !DeviceLifetime || !CredentialAuthority || !CredentialSessionContext || !AcceptedSession || CredentialProvenance != github_credential_provenance::device_grant || DeviceBindingDurable || BindingStorePending) {
				return;
			}
			if (remaining_access_lifetime(*DeviceLifetime, WallClock()) == 0U) {
				binding_failure();
				return;
			}
			const auto record     = device_binding_record{ .Authority = *CredentialAuthority, .Lifetime = *DeviceLifetime };
			const auto generation = BindingGeneration;
			auto       finish     = std::function<void(device_binding_result)>{ [this, record, generation](device_binding_result aResult) {
				--OutstandingBindings;
				BindingStorePending = false;
				if (Closing || generation != BindingGeneration || CredentialAuthority != record.Authority || DeviceLifetime != record.Lifetime) {
					return;
				}
				if (!aResult || remaining_access_lifetime(record.Lifetime, WallClock()) == 0U) {
					binding_failure();
					return;
				}
				DeviceBindingDurable = true;
				// [[BPR-0660]]. "Remembered on this Mac" is a fact the
				// card states, so the session is published again once
				// it is one.
				republish_accepted_session();
			} };
			BindingStorePending = true;
			++OutstandingBindings;
			call_binding_lane([this, &record](auto aCallback) { Persistence->store(record, std::move(aCallback)); }, std::move(finish));
		}

		void start_delete(const core::github_identity_generation   aGeneration,
						  std::optional<deferred_session_terminal> aTerminal,
						  const refresh_disposition                aRefresh = refresh_disposition::erase)
		{
			if (Persistence == nullptr || !CredentialAuthority || !CredentialSessionContext) {
				start_delete_unblocked(aGeneration, std::move(aTerminal), aRefresh);
				return;
			}
			// [[BPR-1112]]. The request's terminal handler is built before any
			// state changes. If building it throws, the caller gets the
			// exception and this site has changed nothing: no durability, no
			// generation, no stored terminal, no count and no pending flag.
			const auto authority = *CredentialAuthority;
			auto       finish    = BindingErasePending ? std::function<void(device_binding_result)>{ } : std::function<void(device_binding_result)>{ [this, authority](device_binding_result aResult) {
				--OutstandingBindings;
				BindingErasePending = false;
				if (CredentialAuthority != authority) {
					binding_failure();
					return;
				}
				if (!aResult) {
					ShutdownCleanupFailed = Closing;
					if (Closing || !DeferredSession) {
						binding_failure();
						start_delete_unblocked(BindingDeleteGeneration, std::move(BindingDeleteTerminal), BindingDeleteRefresh);
						return;
					}
					StartupCleanupOperation.reset();
					DeferredTerminal = std::move(BindingDeleteTerminal);
					fail_delete_operation();
					return;
				}
				start_delete_unblocked(BindingDeleteGeneration, std::move(BindingDeleteTerminal), BindingDeleteRefresh);
			} };
			DeviceBindingDurable = false;
			++BindingGeneration;
			BindingDeleteGeneration = aGeneration;
			BindingDeleteTerminal   = std::move(aTerminal);
			BindingDeleteRefresh    = aRefresh;
			if (BindingErasePending) {
				return;
			}
			BindingErasePending = true;
			++OutstandingBindings;
			call_binding_lane([this, &authority](auto aCallback) { Persistence->erase(authority, std::move(aCallback)); }, std::move(finish));
		}

		device_binding_persistence*          Persistence = nullptr;
		std::function<std::int64_t()>        WallClock;
		std::optional<device_binding_record> PendingDeviceRestore;
		bool                                 PendingDeviceRestoreRenews  = false;
		bool                                 PendingDeviceRestoreExpired = false;
		device_launch_renewal_sink           LaunchRenewalSink;
		std::optional<device_launch_renewal> LaunchRenewal;
		// The sink was asked to renew, so the device host holds a chain for it.
		bool LaunchRenewalBegun = false;
		// The selected provisional credential is a launch renewal's rotated
		// grant, whose `/user` named the binding's account and which has not
		// been accepted yet.
		bool LaunchRenewedGrant = false;
		// The rotated grant has enrolled and waits for its `/user` account.
		bool                                                    LaunchRenewalEnrolled = false;
		std::optional<device_binding_lifetime>                  NextDeviceLifetime;
		std::optional<device_binding_lifetime>                  DeviceLifetime;
		bool                                                    NextRefreshSettled    = false;
		bool                                                    DeviceRefreshSettled  = false;
		bool                                                    DeviceBindingDurable  = false;
		bool                                                    BindingStorePending   = false;
		bool                                                    BindingLoadPending    = false;
		bool                                                    BindingLoadUnresolved = false;
		std::optional<core::invalidate_github_identity_request> PendingRestoreCleanup;
		std::optional<github_credential_operation_context>      StartupCleanupOperation;
		bool                                                    BindingErasePending   = false;
		bool                                                    RestoredDeviceBinding = false;
		std::uint64_t                                           BindingGeneration     = 0;
		std::uint64_t                                           NextSetupAttempt      = 1U;
		std::optional<staged_enrollment>                        StagedEnrollment;
		std::optional<github_viewer_success>                    AcceptedSetupBootstrap;
		std::size_t                                             OutstandingSetup = 0U;

		std::size_t                              OutstandingBindings           = 0;
		std::size_t                              OutstandingRestoreCompletions = 0;
		bool                                     RestoreIngressClosing         = false;
		core::github_identity_generation         BindingDeleteGeneration;
		std::optional<deferred_session_terminal> BindingDeleteTerminal;
		refresh_disposition                      BindingDeleteRefresh = refresh_disposition::erase;

		// [[BPR-1048]]. Terminals of binding requests the lane withdrew during
		// the current turn, failed at its end, or before its exception leaves
		// it when it ends in one ([[BPR-1112]]). Each is still counted, so the
		// host is not quiescent before they run. Queueing moves a
		// `std::function` into a free slot and allocates nothing; when every
		// slot is taken the failure is delivered at once.
		std::array<std::optional<std::function<void(device_binding_result)>>, 8> WithdrawnBindings;
		std::size_t                                                              WithdrawnBindingCount = 0U;

		std::shared_ptr<github_window_read_host> ReadWindow;
		std::vector<window_slot>                 AttachedWindows;
		std::uint64_t                            SharedSnapshotSequence = 0U;
		std::optional<github_profile_session>    PublishedSession;
		bool                                     PrimaryReadClosed       = false;
		bool                                     ExplicitPrimaryAttached = false;
		core::bpr_window_id                      LastAttachedWindow{ };
		std::array<core::bpr_window_id, 32>      RestoredWindows{ };
		std::size_t                              RestoredWindowCount = 0;
		bool                                     RestorationSealed   = false;
		core::github_state                       State;
		github_profile_port&                     Profiles;
		github_viewer_port&                      Viewers;
		github_capability_port&                  Capabilities;
		github_pull_request_port&                PullRequests;
		github_credential_vault_port&            Credentials;
		github_credential_binding_catalog&       CredentialBindings;
		const core::bpr_profile_id               BprProfile;
		github_capability_cache                  CapabilityCache;
		decision_executor&                       DecisionLane;
		github_projection_port&                  Projections;
		github_credential_shutdown_policy        CredentialShutdownPolicy = github_credential_shutdown_policy::retain;
		github_epoch_handoff_port*               Epochs                   = nullptr;
		github_pull_request_port*                AnonymousPullRequests    = nullptr;
		git_smart_transport_factory_port*        AnonymousSmartGit        = nullptr;
		public_preview_projection_port*          PublicPreview            = nullptr;
		public_preview_sign_in_sink              PublicPreviewSignIn;
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
		std::optional<kept_session_ending>                  NextKeptEnding;
		std::optional<kept_session_ending>                  SessionKeptEnding;
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
		bool                                           RefreshSlotMissing          = false;
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

		std::optional<session_candidate>             StagedSession;
		std::optional<github_pull_request_snapshot>  StagedSnapshot;
		std::optional<github_session_read_model>     AcceptedSession;
		std::optional<github_profile_authority>      AcceptedProfileAuthority;
		std::optional<github_capability_cache_entry> AcceptedCapabilityEvidence;
		std::optional<github_pull_request_snapshot>  AcceptedSnapshot;
		review_mutation_transport                    ReviewMutationTransport;
		github_rate_limit_state                      RateLimits;
		std::uint64_t                                LogicalEpoch               = 0U;
		bool                                         Closing                    = false;
		bool                                         ShutdownCleanupFailed      = false;
		bool                                         ProjectionDeliveryFailed   = false;
		bool                                         EpochHandoffDeliveryFailed = false;
		bool                                         EpochHandoffUnavailable    = false;
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
		public_preview_projection_port* const aPublicPreview, core::bpr_profile_id aBprProfile)
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
														   aPublicPreview, aBprProfile) }
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
		const core::host_profile_id                  aProfile,
		github_credential_material                   aCredential,
		const github_credential_provenance           aProvenance,
		const std::optional<device_binding_lifetime> aLifetime,
		const bool                                   aRefreshSettled)
	{
		Implementation->enroll_profile(
			aProfile, std::move(aCredential), aProvenance, aLifetime, aRefreshSettled);
	}

	void github_application_host::stage_setup_enrollment(core::pending_setup_destination                                   aPending,
														 github_credential_material                                        aCredential,
														 std::function<bool(const core::pending_setup_destination&, bool)> aAccept,
														 const github_credential_provenance aProvenance, std::optional<device_binding_lifetime> aLifetime,
														 const bool aRefreshSettled)
	{
		Implementation->stage_setup_enrollment(std::move(aPending), std::move(aCredential), std::move(aAccept), aProvenance, std::move(aLifetime), aRefreshSettled);
	}
	void github_application_host::cancel_setup_enrollment() { Implementation->cancel_setup_enrollment(); }

	void github_application_host::resume_profile(
		const core::host_profile_id aProfile)
	{
		Implementation->resume_profile(aProfile);
	}

	void github_application_host::configure_device_persistence(device_binding_persistence& aPersistence, std::function<std::int64_t()> aWallClock, device_launch_renewal_sink aLaunchRenewal)
	{
		Implementation->Persistence       = &aPersistence;
		Implementation->WallClock         = std::move(aWallClock);
		Implementation->LaunchRenewalSink = std::move(aLaunchRenewal);
	}

	auto github_application_host::restore_device_profile(const github_profile_authority                     aCompiledProfile,
														 std::function<void(device_profile_restore_status)> aCompletion, std::function<bool()> aCurrent) -> bool
	{
		auto& initialHost = *Implementation;
		if (initialHost.Closing || initialHost.RestoreIngressClosing || initialHost.OutstandingRestoreCompletions >= maximum_pending_device_profile_restores) {
			return false;
		}
		auto completion      = std::make_shared<implementation::restore_completion>();
		completion->Callback = std::move(aCompletion);
		++initialHost.OutstandingRestoreCompletions;
		if (initialHost.BindingLoadUnresolved || initialHost.Persistence == nullptr || !initialHost.WallClock || initialHost.State.ActiveProfile || initialHost.OutstandingBindings != 0U
			|| !valid(aCompiledProfile) || aCompiledProfile.Authentication != github_authentication_transport::native_https
			|| aCompiledProfile.ServerContract != github_server_contract::github_com_2022_11_28) {
			initialHost.complete_restore(completion, device_profile_restore_status::unavailable);
			return true;
		}
		initialHost.BindingLoadPending = true;
		++initialHost.OutstandingBindings;
		const auto generation = initialHost.BindingGeneration;
		try {
			initialHost.Persistence->load(initialHost.binding_callback([this, aCompiledProfile, completion, generation, current = std::move(aCurrent)](device_binding_load_result aResult) {
				struct terminal_guard
				{
					implementation&                                     Host;
					std::shared_ptr<implementation::restore_completion> Completion;
					device_profile_restore_status                       Status = device_profile_restore_status::unavailable;
					~terminal_guard() { Host.complete_restore(Completion, Status); }
				} terminal{ *Implementation, completion };
				auto& host = *Implementation;
				--host.OutstandingBindings;
				host.BindingLoadPending = false;
				const auto cleanup      = std::exchange(host.PendingRestoreCleanup, std::nullopt);
				auto       admitted     = true;
				try {
					if (current) {
						admitted = current();
					}
				} catch (...) {
					admitted = false;
				}
				const auto superseded = generation != host.BindingGeneration || host.State.ActiveProfile.has_value()
					|| host.RestoreIngressClosing || host.BindingLoadPending || !host.DeferredIngress.empty();
				if (!admitted || host.Closing || cleanup || superseded || (!aResult && aResult.error() == device_binding_failure::cancelled)) {
					terminal.Status = device_profile_restore_status::cancelled;
				}
				if ((!admitted || host.Closing || host.RestoreIngressClosing) && !cleanup) {
					return;
				}
				const auto fail = [&host, &cleanup] {
					host.BindingLoadUnresolved = true;
					host.binding_failure();
					if (host.Closing && cleanup) {
						host.ShutdownCleanupFailed = true;
					}
					if (host.DeferredSession) {
						const auto context = *host.DeferredSession;
						host.DeferredSession.reset();
						host.fail_session(context, core::github_failure::vault);
					}
				};
				if (!aResult || aResult->size() > maximum_device_bindings) {
					fail();
					return;
				}
				std::optional<device_binding_record> selected;
				for (const auto& record : *aResult) {
					if (!valid(record) || record.Authority.BprProfile != host.BprProfile) {
						fail();
						return;
					}
					if (record.Authority.Profile == aCompiledProfile.Profile) {
						if (selected) {
							fail();
							return;
						}
						selected = record;
					}
				}
				if (!selected) {
					host.start_deferred_session();
					return;
				}
				const auto& authority = selected->Authority;
				if (authority.ProfileRevision != aCompiledProfile.ProfileRevision || authority.Host != aCompiledProfile.Host
					|| authority.Hostname != aCompiledProfile.Hostname || authority.ServerContract != aCompiledProfile.ServerContract) {
					fail();
					return;
				}
				if (cleanup) {
					// No session is established. The compiled profile validates the loaded
					// authority; the core's invalidation owns this exact cleanup request.
					host.CredentialAuthority      = authority;
					host.CredentialSessionContext = core::github_session_request_context{
						.Request = cleanup->Request, .ApplicationSession = cleanup->ApplicationSession, .Profile = authority.Profile, .IdentityGeneration = cleanup->NewGeneration
					};
					host.CredentialLifecycle     = implementation::credential_lifecycle::persistent;
					host.CredentialProvenance    = github_credential_provenance::device_grant;
					host.StartupCleanupOperation = github_credential_operation_context{
						.Request = cleanup->Request, .ApplicationSession = cleanup->ApplicationSession, .Authority = authority, .IdentityGeneration = cleanup->NewGeneration
					};
					host.start_delete(cleanup->NewGeneration, std::nullopt);
					return;
				}
				if (superseded) {
					host.start_deferred_session();
					return;
				}
				if (selected->Tombstone) {
					host.start_deferred_session();
					return;
				}
				// [[BPR-0644]]. An access lifetime that elapsed while the
				// application was closed is not the end of the grant: while the
				// refresh lifetime remains, the restore renews it before the
				// session starts. An elapsed refresh lifetime, or a composition
				// with nothing to renew through, cannot renew. Under the owner's
				// rule of 2026-09-24 it is not tombstoned either: the grant stays
				// until the person removes it, and its held session fails at
				// once, saying it needs signing in again.
				const auto now     = host.WallClock();
				const auto renews  = remaining_access_lifetime(selected->Lifetime, now) == 0U;
				const auto expired = renews && (!host.LaunchRenewalSink || remaining_refresh_lifetime(selected->Lifetime, now) == 0U);
				// A renewing restore remembers nothing yet: the rotated grant's own
				// enrollment names the authority after `/user`, and a remembered
				// binding would make that enrollment delete the pair it rotated.
				if (!renews && !host.CredentialBindings.remember(authority, github_credential_provenance::device_grant)) {
					fail();
					return;
				}
				host.PendingDeviceRestore        = selected;
				host.PendingDeviceRestoreRenews  = renews;
				host.PendingDeviceRestoreExpired = expired;
				host.resume_profile(aCompiledProfile.Profile);
				terminal.Status = expired ? device_profile_restore_status::unavailable : device_profile_restore_status::ready_for_session;
			}));
		} catch (...) {
			--initialHost.OutstandingBindings;
			initialHost.BindingLoadPending = false;
			initialHost.complete_restore(completion, device_profile_restore_status::unavailable);
			initialHost.binding_failure();
		}
		return true;
	}

	void github_application_host::invalidate_device_grant(github_credential_authority_key aAuthority, const core::github_identity_generation aGeneration, const core::github_failure aFailure)
	{
		auto& host = *Implementation;
		if (host.Closing || aAuthority.BprProfile != host.BprProfile) {
			return;
		}
		if (host.TurnActive || host.DeferredIngressScheduled || !host.DeferredIngress.empty()) {
			host.DeferredIngress.emplace_back(implementation::deferred_grant_invalidation{ std::move(aAuthority), aGeneration, aFailure });
			host.schedule_deferred_ingress();
			return;
		}
		host.run_turn([&host, &aAuthority, aGeneration, aFailure] { host.invalidate_device_grant_now(aAuthority, aGeneration, aFailure); });
	}

	void github_application_host::settle_device_binding(github_credential_authority_key aAuthority, const core::github_identity_generation aGeneration, const device_binding_lifetime aLifetime)
	{
		auto& host = *Implementation;
		if (host.Closing || !host.AcceptedSession || host.CredentialAuthority != aAuthority || !host.CredentialSessionContext || host.CredentialSessionContext->IdentityGeneration != aGeneration || host.CredentialProvenance != github_credential_provenance::device_grant || host.DeviceLifetime != aLifetime) {
			return;
		}
		host.DeviceRefreshSettled = true;
		host.persist_settled_device_binding();
	}

	void github_application_host::prepare_device_rotation(github_credential_authority_key aAuthority, const core::github_identity_generation aGeneration, std::function<void(bool)> aCompletion)
	{
		const auto& host = *Implementation;
		// [[BPR-0644]], [[BPR-1116]]. The barrier writes no tombstone, for a
		// launch renewal or for an accepted session's. A tombstone written
		// before the spend meant that a renewal that then failed, or a process
		// that ended before the rotated grant's binding was stored, left a row
		// the next launch read as a removal. Under the owner's rule of
		// 2026-09-24 the grant stays until the person removes it, so the row
		// stays live until the rotated grant's binding replaces it. A crash
		// after the spend leaves a live row: the next launch spends whatever
		// the refresh slot holds, and material the server refuses asks for
		// signing in again. So the barrier only answers whether this exact
		// grant may be renewed: it changes nothing, counts nothing, and
		// answers once.
		const auto accepted = !host.Closing && host.AcceptedSession && host.CredentialAuthority == aAuthority && host.CredentialSessionContext && host.CredentialSessionContext->IdentityGeneration == aGeneration && host.CredentialProvenance == github_credential_provenance::device_grant;
		aCompletion(accepted || host.owns_launch_renewal(aAuthority, aGeneration));
	}

	auto github_application_host::accepted_device_lifetime() const noexcept -> std::optional<device_binding_lifetime>
	{
		return Implementation->AcceptedSession && Implementation->CredentialProvenance == github_credential_provenance::device_grant ? Implementation->DeviceLifetime : std::nullopt;
	}
	auto github_application_host::accepted_device_binding_durable() const noexcept -> bool
	{
		return Implementation->AcceptedSession.has_value() && Implementation->DeviceBindingDurable;
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

	auto github_application_host::accepted_profile_session() const
		-> std::optional<github_profile_session>
	{
		return Implementation->current_profile_session();
	}

	auto github_application_host::attach_window(const std::shared_ptr<github_window_read_host>& aWindow) -> bool { return Implementation->attach_window(aWindow); }
	void github_application_host::detach_window(const core::bpr_window_context aWindow) { Implementation->detach_window(aWindow); }
	void github_application_host::observe_attached_read(const std::shared_ptr<github_window_read_host>& aWindow, const bool aFreshSnapshot) { Implementation->observe_attached_read(aWindow, aFreshSnapshot); }

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

	auto github_application_host::accepted_publication_account() const
		-> std::optional<github_publication_account>
	{
		const auto& host = *Implementation;
		if (host.signed_out_mode() || host.Closing || !host.AcceptedSession.has_value()
			|| !host.AcceptedProfileAuthority.has_value()) {
			return std::nullopt;
		}
		const auto& session   = *host.AcceptedSession;
		const auto& authority = *host.AcceptedProfileAuthority;
		auto* const factory   = dynamic_cast<github_publication_session_factory_port*>(host.AccountTransport.get());
		if (factory == nullptr || session.Profile != authority.Profile || session.Host != authority.Host
			|| session.Viewer.Login.empty()) {
			return std::nullopt;
		}
		auto provider = factory->acquire_publication_session();
		if (provider == nullptr) {
			return std::nullopt;
		}
		return github_publication_account{
			.Provider = std::move(provider),
			.Server   = authority.ServerContract,
			.Account  = session.Viewer.Account,
			.Login    = session.Viewer.Login,
			.Host     = std::string{ authority.Hostname.value() },
		};
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
			&& (!Implementation->ReadWindow || Implementation->ReadWindow->is_quiescent())
			&& std::all_of(Implementation->AttachedWindows.begin(), Implementation->AttachedWindows.end(),
						   [](const auto& aSlot) { return aSlot.Host->is_quiescent(); })
			&& Implementation->OutstandingSetup == 0U
			&& Implementation->OutstandingBindings == 0U
			&& Implementation->OutstandingRestoreCompletions == 0U
			&& !Implementation->BindingErasePending
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
		return Implementation->ReadWindow && Implementation->ReadWindow->epoch_handoff_delivery_failed();
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
		return Implementation->ReadWindow && Implementation->ReadWindow->epoch_handoff_unavailable();
	}
}
