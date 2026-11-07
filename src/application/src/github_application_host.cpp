#include "bpr/application/github_application_host.hpp"
#include "bpr/application/operation_diagnostics.hpp"
#include "github_host_implementation.hpp"
#include "github_host_support.hpp"

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
		const bool                                   aRefreshSettled,
		github_enrollment_rebind                     aRebind)
	{
		Implementation->enroll_profile(
			aProfile, std::move(aCredential), aProvenance, aLifetime, aRefreshSettled, std::move(aRebind));
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

	void github_application_host::set_diagnostic_log(std::shared_ptr<diagnostic_log_port> aLog)
	{
		Implementation->DiagnosticLog = std::move(aLog);
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

	auto github_application_host::pending_session_presentation() const -> std::optional<github_presentation_update>
	{
		const auto& current = Implementation->State;
		if (Implementation->Closing || !current.PendingSession) {
			return std::nullopt;
		}
		return github_presentation_update{ .Transition = github_transition_projection{
											   .Changed      = core::github_changed_domain::identity | core::github_changed_domain::capabilities | core::github_changed_domain::pull_request | core::github_changed_domain::status | core::github_changed_domain::commands,
											   .Commands     = core::github_commands(current),
											   .Status       = core::github_status_delta{ .Status = core::github_status::establishing_session },
											   .Identity     = core::github_identity_delta{ .Profile = current.ActiveProfile, .Host = current.Host, .Account = current.Account, .Generation = current.IdentityGeneration },
											   .Capabilities = core::github_capability_delta{ .Capabilities = current.Capabilities, .Revision = current.CapabilityRevision },
											   .PullRequest  = core::pull_request_summary_delta{ .Summary = std::nullopt },
										   } };
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

	auto github_application_host::accepted_device_connection() const noexcept -> bool
	{
		return Implementation->AcceptedSession && Implementation->CredentialAuthority
			&& Implementation->CredentialBindings.provenance(Implementation->AcceptedSession->Profile) == github_credential_provenance::device_grant;
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
		return Implementation->Closing && Implementation->outstanding_work_settled();
	}

	auto github_application_host::is_running_idle() const noexcept -> bool
	{
		return !Implementation->Closing && Implementation->outstanding_work_settled()
			&& !Implementation->TurnActive && Implementation->DeferredIngress.empty()
			&& !Implementation->DeferredIngressScheduled;
	}

	auto github_application_host::implementation::outstanding_work_settled() const noexcept -> bool
	{
		return (!ReadWindow || ReadWindow->is_quiescent())
			&& std::all_of(AttachedWindows.begin(), AttachedWindows.end(),
						   [](const auto& aSlot) { return aSlot.Host->is_quiescent(); })
			&& OutstandingSetup == 0U
			&& OutstandingBindings == 0U
			&& OutstandingRestoreCompletions == 0U
			&& !BindingErasePending
			&& std::holds_alternative<std::monostate>(PendingProvider)
			&& ActiveVaultOperation == vault_operation::none
			// [[BPR-0540]]. The refresh slot's erasure is part of the exact
			// cleanup, so composition keeps this host and the vault alive
			// until the vault has answered for it too.
			&& OutstandingRefreshDeletes.load(std::memory_order_acquire) == 0U;
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
