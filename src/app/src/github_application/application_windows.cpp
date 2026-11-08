#include "bpr/application/appearance_application_host.hpp"
#include "bpr/application/device_authorization_projection.hpp"
#include "bpr/application/epoch_application_host.hpp"
#include "bpr/application/github_read_model.hpp"
#include "bpr/application/notification_delivery.hpp"
#include "bpr/application/profile_startup_admission.hpp"
#include "bpr/application/profile_storage_authority.hpp"
#include "bpr/application/public_preview_projection.hpp"
#include "bpr/application/source_search_projection.hpp"
#include "bpr/application/tour_owner_channel.hpp"
#include "bpr/application/window_review_restoration_host.hpp"
#include "bpr/core/appearance_model.hpp"
#include "bpr/core/device_authorization_model.hpp"
#include "bpr/core/diff_model.hpp"
#include "bpr/core/epoch_model.hpp"
#include "bpr/core/model.hpp"
#include "bpr/core/profile_window.hpp"
#include "bpr/core/window_review_restoration.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/device_authorization_panel.hpp"
#include "bpr/qt/dif_runtime.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "github_application/cli_accounts.hpp"
#include "github_application/device_authorization_composition.hpp"
#include "github_application/github_presentation_projections.hpp"
#include "github_application/profile_runtime.hpp"
#include "github_application/public_preview_wire_transport.hpp"
#include "github_application/tour_owner_replies.hpp"
#include "github_application/window_composition.hpp"
#include "github_application/window_review_lanes.hpp"
#include "github_host_profile.hpp"
#include "github_provider.hpp"
#include "pr_search_controller.hpp"
#include "profile_window_layout_port.hpp"
#include "review_cycle_desktop.hpp"
#include "review_submission_workflow.hpp"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFlags>
#include <QWidget>

#include <algorithm>
#include <cstddef>
#include <deque>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#if defined(BPR_BUILD_TESTING)
	#include "bpr/application/github_window_read_host.hpp"
#endif

namespace bpr::app {
	github_application::github_application(std::shared_ptr<github_profile_runtime> aProfile) noexcept
		: Profile{ std::move(aProfile) }
	{ }

	github_application::~github_application()
	{
		// [[BPR-0827]]. First, before shutdown begins and before the drain below
		// pumps events, so a poller tick that the drain delivers finds the token
		// expired rather than re-entering windows that are already shutting down.
		// BPR-0705 cleared it after the drain, and every such tick still found
		// the application usable.
		Liveness.reset();
		begin_shutdown();
		// The existing fallback drain now belongs to the profile facade, after all
		// window sessions have been told to stop. Normal desktop close is already quiescent.
		while (!profile_shutdown_complete()) {
			QCoreApplication::processEvents(QEventLoop::AllEvents);
			std::this_thread::yield();
		}
		SessionCloseLifetime.reset();
	}

	auto github_application::liveness() const noexcept -> std::weak_ptr<const void> { return Liveness; }

	void github_application::show()
	{
		if (!InitialRetired && Implementation) {
			Implementation->Window->show();
		}
	}

	void github_application::set_window_admission(std::function<bool(core::bpr_window_context)> aAdmission) { Profile->WindowAdmission = std::move(aAdmission); }

	void github_application::set_review_window_request(std::function<bool(const core::window_review_destination&)> aRequest) { Profile->ReviewWindowRequest = std::move(aRequest); }

	void github_application::open_window_review(core::bpr_window_context aContext, const core::window_review_destination& aDestination)
	{
		const auto found = Sessions.find(aContext);
		if (found == Sessions.end() || found->second->Closing) {
			return;
		}
		found->second->open_review_destination(aDestination);
	}

	auto github_application::open_window(core::bpr_window_context aContext) -> bool
	{
		if (Profile->Closing || aContext.Profile != Profile->Admission.Authority.profile() || aContext.Window.value() == 0 || aContext.Generation.value() == 0 || Sessions.contains(aContext)) {
			return false;
		}
		if (Profile->WindowAdmission && !Profile->WindowAdmission(aContext)) {
			return false;
		}
		for (const auto& [context, session] : Sessions) {
			if (context.Window == aContext.Window) {
				return false;
			}
		}
		try {
			auto session = std::make_shared<implementation>(Profile, aContext);
			Sessions.emplace(aContext, session);
			// Not `liveness()`: this callback erases the session and starts profile
			// shutdown, so it has to run inside the destructor's own drain, after
			// that token has expired (BPR-0827).
			auto weak         = std::weak_ptr<const void>{ SessionCloseLifetime };
			session->OnClosed = [this, weak, aContext] {
				if (weak.expired()) {
					return;
				}
				const auto found = Sessions.find(aContext);
				if (found == Sessions.end()) {
					return;
				}
				if (found->second->ShutdownCleanupFailed) {
					// [[BPR-0875]]. A window's cleanup refusal becomes the
					// profile's; its name travels with it.
					Profile->note_profile_cleanup_failure(found->second->ShutdownCleanupDetail);
					Profile->CleanupFailed = true;
				}
				Sessions.erase(found);
				if (Profile->Closing && Sessions.empty()) {
					Profile->begin_shutdown();
				}
			};
			session->start();
			// [[BPR-1273]]. Add Notes to Review Tour edits a pull request's tour
			// where its draft lives, through this profile's tour owner, as
			// `bpr tour` does: the window holding that draft, or the stored one.
			if (session->CycleDesktop) {
				session->CycleDesktop->set_tour_route([this, live = std::weak_ptr<const int>{ Liveness }](application::tour_channel_bytes aRequest, std::function<void(application::tour_channel_bytes)> aRespond) {
					if (live.expired()) {
						aRespond(tour_owner_error("owner_unavailable"));
						return;
					}
					desktop_tour_owner_route(std::move(aRequest), std::move(aRespond));
				});
			}
			if (Profile->AgentWorkspaceRecoveryRefused) {
				session->present_agent_workspace_repair();
			}
			if (Profile->ReviewRestoration) {
				static_cast<void>(Profile->ReviewRestoration->attach(aContext));
				if (Profile->RestorationGlobalNotice) {
					session->restoration_notice(*Profile->RestorationGlobalNotice);
				}
				if (const auto notice = Profile->RestorationNotices.find(aContext); notice != Profile->RestorationNotices.end()) {
					session->restoration_notice(notice->second);
				}
				Profile->ReviewRestoration->mark_window_ready(aContext);
				session->publish_recent();
			}
			if (!Implementation) {
				Implementation       = std::move(session);
				Profile->FirstWindow = aContext;
			}
			return true;
		} catch (...) {
			Profile->Observers.erase(aContext);
			const auto found = Sessions.find(aContext);
			if (found != Sessions.end()) {
				found->second->begin_shutdown();
			}
			return false;
		}
	}

	auto github_application::configure_desktop_layouts(const std::vector<core::bpr_window_context>& aWindows) -> bool
	{
		if (Profile->WindowLayouts || Profile->ReviewRestoration) {
			return true;
		}
		if (Profile->FirstWindow || aWindows.empty() || aWindows.size() > core::maximum_bpr_windows_per_profile) {
			return false;
		}
		auto keep = std::vector<core::bpr_window_id>{ };
		for (const auto& context : aWindows) {
			if (context.Profile != Profile->Admission.Authority.profile() || context.Window.value() == 0 || context.Generation.value() == 0 || std::ranges::find(keep, context.Window) != keep.end()) {
				return false;
			}
			keep.push_back(context.Window);
		}
		// Per-window durable layouts only where composition found the store
		// usable. Otherwise every window takes the session defaults, as the
		// profile's own settings port already does, so a store that cannot open
		// yields the composed defaults instead of a window that never publishes
		// a layout and reports no drag (BPR-0718).
		if (Profile->WorkbenchPersistenceAvailable) {
			Profile->WindowLayouts = std::make_shared<profile_window_layout_service>(Profile->ApplicationDataRoot, std::move(keep));
		}
		auto hosts = std::vector<std::string>{ };
		for (const auto& profile : Profile->Profiles) {
			hosts.emplace_back(profile.hostname());
		}
		Profile->ReviewRestorationStore = std::make_unique<effect_window_review_store>(Profile->ApplicationDataRoot);
#if defined(BPR_BUILD_TESTING)
		auto& restorationDecisions = Profile->RestorationDecisionGate;
#else
		auto& restorationDecisions = Profile->DecisionLane;
#endif
		Profile->ReviewRestoration = std::make_unique<application::window_review_restoration_host>(Profile->Admission.Authority.profile(), aWindows, std::move(hosts), *Profile->ReviewRestorationStore, Profile->WorkbenchEffects, restorationDecisions, *Profile);
		Profile->ReviewRestoration->start();
		return true;
	}

	void github_application::close_window(core::bpr_window_context aContext, window_retirement aMode)
	{
		const auto found = Sessions.find(aContext);
		if (found == Sessions.end()) {
			return;
		}
		if (found->second == Implementation) {
			InitialRetired = true;
		}
		found->second->begin_shutdown(aMode);
	}

	void github_application::focus_window(core::bpr_window_context aContext)
	{
		auto* selected = window(aContext);
		if (!selected) {
			return;
		}
		Profile->Focused = aContext;
		selected->show();
		if (!selected->isActiveWindow()) {
			selected->raise();
			selected->activateWindow();
		}
	}

	auto github_application::window_quiescent(core::bpr_window_context aContext) const noexcept -> bool
	{
		const auto found = Sessions.find(aContext);
		// The queued close callback still folds cleanup results and retires the
		// session. Profile shutdown must not force that orderly window while
		// this ownership handoff is pending.
		return found == Sessions.end();
	}

	auto github_application::windows() const -> std::vector<core::bpr_window_context>
	{
		auto result = std::vector<core::bpr_window_context>{ };
		for (const auto& [context, session] : Sessions) {
			if (!session->Closing) {
				result.push_back(context);
			}
		}
		return result;
	}

	auto github_application::window(core::bpr_window_context aContext) noexcept -> qt::workbench_window*
	{
		const auto found = Sessions.find(aContext);
		return found == Sessions.end() || found->second->Closing || !Profile->current_window(aContext) ? nullptr : found->second->Window.get();
	}

	auto github_application::window() -> qt::workbench_window&
	{
		if (InitialRetired || !Implementation || !Implementation->Window) {
			throw std::logic_error{ "initial window retired" };
		}
		return *Implementation->Window;
	}

	void github_application::begin_shutdown()
	{
		if (Profile->Closing) {
			return;
		}
		Profile->Closing = true;
		cli_account_users().set(Profile.get(), std::nullopt);
		if (Profile->ReviewRestoration && !Profile->RestorationOrderly) {
			Profile->ReviewRestoration->shutdown();
		}
		for (const auto& [context, session] : Sessions) {
			session->begin_shutdown();
		}
		if (Sessions.empty()) {
			Profile->begin_shutdown();
		}
	}

	auto github_application::profile_shutdown_complete() const noexcept -> bool
	{
		Profile->poll_shutdown();
		return Profile->ShutdownComplete;
	}

	auto github_application::profile_shutdown_cleanup_failed() const noexcept -> bool { return Profile->CleanupFailed; }

	// [[BPR-0875]]. What the profile's flag above is about, for the desktop's
	// exit code and for a person reading a real quit.
	auto github_application::profile_shutdown_cleanup_detail() const -> std::string { return Profile->CleanupDetail; }

	// [[BPR-0875]]. The profile's name once it has one, else this window's.
	auto github_application::shutdown_cleanup_detail() const -> std::string
	{
		if (!Profile->CleanupDetail.empty()) {
			return Profile->CleanupDetail;
		}
		return Implementation ? Implementation->ShutdownCleanupDetail : std::string{ };
	}

	auto github_application::implementation::status() const noexcept -> github_application_status
	{
		// How many spans the pane on screen carries right now. Read here rather
		// than counted as it is adopted, because the question is about the
		// present state of the surface and a count kept alongside would only
		// agree with it by luck. BPR-0390.
		const auto displayedSpanCount
			= [this](const core::diff_side aSide) noexcept -> std::size_t {
			if (this->Window == nullptr) {
				return 0U;
			}
			const auto& spans
				= this->Window->comparison_surface()->token_spans(aSide);
			return spans ? spans->size() : 0U;
		};
		return {
			.Closing                       = this->Closing,
			.ShutdownComplete              = this->ShutdownComplete,
			.SourceSearchQuiescent         = !PrFind || PrFind->is_quiescent(),
			.SourceSearchScans             = PrFind ? PrFind->statistics().Scans : 0U,
			.ShutdownCleanupFailed         = this->ShutdownCleanupFailed,
			.AgentWorkspaceCleanupFailed   = this->AgentWorkspaceCleanupFailed,
			.AgentWorkspaceRecoveryRefused = this->Profile->AgentWorkspaceRecoveryRefused,
			.NativePlatformVaultSelected   = this->NativePlatformVaultSelected,
			.EpochProjectionDeliveryFailed = this->EpochProjectionDeliveryFailed,
			.EpochHandoffDeliveryFailed    = this->WindowRead && this->WindowRead->epoch_handoff_delivery_failed(),
			.WorkbenchHostComposed         = this->WorkbenchHost != nullptr,
			.AppearanceHostComposed        = this->AppearanceHost != nullptr,
#if defined(BPR_AGENT_INTEGRATIONS)
			.AgentHostComposed = this->AgentHost != nullptr,
#else
			.AgentHostComposed = false,
#endif
			.AppearancePersistenceAvailable = this->AppearancePersistenceAvailable,
			.AppearanceDurableWrites        = this->AppearanceHost != nullptr
				? this->AppearanceHost->persisted_settings()
				: 0U,
			.AppliedSelectionColor          = this->AppliedSelectionColor,
			.AppliedBaseColor               = this->AppliedBaseColor,
			.AppliedFromProfile             = this->AppliedFromProfile,
			.ObservedScheme                 = this->AppearanceProfile
				? this->AppearanceProfile->observation().Scheme
				: core::os_color_scheme::unknown,
			.ResolvedVariant                = this->AppearanceProfile
				? this->AppearanceProfile->variant()
				: core::appearance_variant::light,
			.CustomisedRoles                = this->AppearanceProfile
				? this->AppearanceProfile->customised_roles()
				: 0U,
			.SyntaxClassifierComposed       = this->SyntaxClassification != nullptr,
			.AdoptedClassifications         = this->AdoptedClassifications,
			.ClassificationSubmissions      = this->AppearanceHost != nullptr
				? this->AppearanceHost->classification_submissions()
				: 0U,
			.ClassificationCacheHits        = this->AppearanceHost != nullptr
				? this->AppearanceHost->cache_hits()
				: 0U,
			.CoalescedClassifications       = this->AppearanceHost != nullptr
				? this->AppearanceHost->coalesced_classifications()
				: 0U,
			.DroppedClassifications         = this->DroppedClassifications,
			.ClassificationFailures         = this->AppearanceHost != nullptr
				? this->AppearanceHost->classification_failures()
				: 0U,
			.ClassificationTerminals        = this->AppearanceHost != nullptr
				? this->AppearanceHost->classification_terminals()
				: 0U,
			.DisplayedLeftClassified        = this->Window != nullptr
				&& static_cast<bool>(this->Window->comparison_surface()
										 ->token_spans(core::diff_side::left)),
			.DisplayedRightClassified = this->Window != nullptr
				&& static_cast<bool>(this->Window->comparison_surface()
										 ->token_spans(core::diff_side::right)),
			.DisplayedLeftSpans = displayedSpanCount(
				core::diff_side::left),
			.DisplayedRightSpans = displayedSpanCount(
				core::diff_side::right),
			.DisplayedComparison = this->Window != nullptr
				&& static_cast<bool>(this->Window->comparison_surface()
										 ->current_result()),
			.DisplayAdoptedClassifications
			= this->DisplayAdoptedClassifications,
			.AppearanceMode                = this->AppearanceHost != nullptr
				? std::string{ core::stable_name(
					  this->AppearanceHost->settings().Mode) }
				: std::string{ },
			.AppearanceTabWidth            = this->AppearanceHost != nullptr
				? this->AppearanceHost->settings().Typography.Code.TabWidth
				: 0U,
			.DifResolution                 = this->AppearanceHost != nullptr
				? this->AppearanceHost->dif_resolution()
				: core::dif_resolution{ },
			.DifCatalogLoadAttempts        = this->Profile->Config.DifCatalog
				? this->Profile->Config.DifCatalog->load_attempts()
				: 0U,
			.DifCatalogDecodedClips        = this->Profile->Config.DifCatalog
				? this->Profile->Config.DifCatalog->decoded_count()
				: 0U,
			.AppearanceDivergences         = this->AppearanceDivergences,
			.ProfileMode                   = this->AppearanceProfile
				? this->AppearanceProfile->mode()
				: core::appearance_mode::system,
			.ProfileVariant                = this->AppearanceProfile
				? this->AppearanceProfile->variant()
				: core::appearance_variant::light,
			.NotificationDeliveryComposed  = this->Notifications != nullptr,
			.NotificationPermission        = this->NotificationSink != nullptr
				? this->NotificationSink->permission()
				: application::notification_permission::refused,
			.NotificationRequestsAttempted = this->NotificationRequestsAttempted,
			.NotificationRequestsSubmitted = this->Notifications != nullptr
				? this->Notifications->statistics().Submitted
				: 0U,
			.NotificationRequestsSuppressedUnattributable
			= this->Notifications != nullptr
				? this->Notifications->statistics()
					  .SuppressedUnattributableActivation
				: 0U,
			.NotificationActivations       = this->Notifications != nullptr
				? this->Notifications->statistics().Activations
				: 0U,
			.NotificationSelections        = this->NotificationSelections,
			.NotificationSelectionFailures = this->NotificationSelectionFailures,
			.EpochStatus                   = this->EpochHost->state().Status,
			.DeviceAuthorizationHostComposed
			= this->DeviceHost != nullptr,
			.DeviceAuthorizationStage
			= this->DeviceProjections.last().Stage,
			.DeviceAuthorizationCapabilityRegistered
			= this->DeviceCapabilityRegistered,
			.DeviceAuthorizationTransportReached
			= this->UnregisteredDeviceTransport.reached(),
			.DeviceAuthorizationPublications
			= this->DeviceProjections.published(),
			.DeviceAuthorizationBrowserComposed
			= this->DeviceBrowserComposed,
			.DeviceAuthorizationBrowser = this->ComposedDeviceBrowser,
			// [[BPR-0797]]. Asked of the factory this run holds, so a fixture
			// run reports the fixture's answer and a shipped-selection run
			// reports the one the account row is offering.
			.ComposedCliReadAvailable = this->TransportFactory != nullptr
				&& this->TransportFactory->supports_cli(),
#if defined(BPR_BUILD_TESTING)
			// [[BPR-0802]]. The composition's own answer, from its one reader.
			.InsideAutomatedTestBoundary
			= this->Profile->InsideAutomatedTestBoundary,
#endif
			.DeviceAuthorizationHandoff
			= this->DeviceProjections.last_attempt(),
			.DeviceAuthorizationBrowserOpens
			= this->device_browser_opens(),
			.DeviceAuthorizationVaultStores
			= this->device_vault_stores(),
			.DeviceAuthorizationDurableCopies
			= this->device_durable_copies(),
			.DeviceAuthorizationTransportSubmissions
			= this->device_transport_submissions(),
			.DeviceAuthorizationVaultStoredDigest
			= this->device_vault_stored_digest(),
			.DeviceAuthorizationVaultStoredIsGrant
			= this->device_vault_stored_is_grant(),
			.DeviceAuthorizationEnrollments
			= this->device_enrollments(),
			.DeviceAuthorizationPanelComposed
			= this->DevicePanel != nullptr,
			.DeviceAuthorizationRefreshRotations
			= this->DeviceRefreshObservation.Rotations,
			.DeviceAuthorizationRefreshState
			= this->DeviceRefreshObservation.State,
			.DeviceAuthorizationRefreshBindings
			= this->DeviceRefreshBindings,
			.DeviceAuthorizationRefreshResumes
			= this->DeviceRefreshResumes,
			.PublicPreviewComposed = this->PublicPreviewComposed,
			.PublicPreviewUnavailable
			= this->public_preview_unavailable(),
			.PublicPreviewPolicyRefused
			= this->public_preview_policy_refused(),
			.PublicPreviewSignedOut = this->public_preview_signed_out(),
			.PublicPreviewPublications
			= this->PublicPreviewProjections.published(),
			.PublicPreviewRequests = this->public_preview_requests(),
			.PublicPreviewHeaderEffects
			= this->public_preview_header_effects(),
			.PublicPreviewVaultEffects
			= this->public_preview_vault_effects(),
			.PublicPreviewSmartAcquisitions
			= this->AnonymousSmartGit == nullptr
				? 0U
				: this->AnonymousSmartGit->acquisitions(),
			.PublicPreviewSignInRequests
			= this->PublicPreviewSignInRequests,
			.PublicPreviewReviewCommentsObserved
			= this->PublicPreviewProjections.last().ReviewComments
				== application::public_preview_observation::observed,
			.PublicPreviewReviewCommentGroups
			= this->PublicPreviewProjections.last()
				  .ReviewCommentGroups.size(),
			.PublicPreviewWriteRefusals
			= this->PublicPreviewWriteRefusals,
			.PublicPreviewLiveCandidateProductionFactories
			= this->public_preview_production_factories(),
			.PublicPreviewWireRequests
			= this->public_preview_wire_requests(),
			.PublicPreviewWireCapabilityProbes
			= this->public_preview_wire_capability_probes(),
			.PublicPreviewWireCredentialedRefusals
			= this->public_preview_wire_credentialed_refusals(),
			.PublicPreviewBudgetObserved
			= this->public_preview_budget().has_value(),
			.PublicPreviewBudgetLimit
			= this->public_preview_budget().has_value()
				? this->public_preview_budget()->Limit
				: 0U,
			.PublicPreviewBudgetRemaining
			= this->public_preview_budget().has_value()
				? this->public_preview_budget()->Remaining
				: 0U,
			.PublicPreviewBudgetResetUnixSeconds
			= this->public_preview_budget().has_value()
				? this->public_preview_budget()->ResetUnixSeconds
				: 0U,
			.PublicPreviewSnapshotPublicPreview
			= this->public_preview_snapshot_signed_out(),
			.PublicPreviewSnapshotHeadOid
			= this->public_preview_snapshot_head_oid(),
		};
	}

	auto github_application::status() const noexcept -> github_application_status
	{
		if (Implementation) {
			auto result = Implementation->status();
			if (Profile->Closing) {
				result.ShutdownComplete      = profile_shutdown_complete();
				result.ShutdownCleanupFailed = Profile->CleanupFailed;
			}
			return result;
		}
		return { .Closing = true, .ShutdownComplete = true };
	}

	auto github_application::status(core::bpr_window_context aContext) const noexcept -> std::optional<github_application_status>
	{
		const auto found = Sessions.find(aContext);
		if (found == Sessions.end()) {
			return std::nullopt;
		}
		return found->second->status();
	}

	void github_application::select_device_authorization()
	{
		Implementation->select_device_authorization();
	}

	void github_application::cancel_device_authorization()
	{
		Implementation->cancel_device_authorization();
	}

	void github_application::open_device_verification_address()
	{
		Implementation->open_device_verification_address();
	}

	void github_application::activate_public_preview(const bool aEnterprise)
	{
		Implementation->activate_public_preview(aEnterprise);
	}

	void github_application::attempt_public_preview_writes()
	{
		Implementation->attempt_public_preview_writes();
	}

	auto github_application::public_preview_wire_operations() const
		-> std::vector<public_preview_wire_operation_record>
	{
		return Implementation->public_preview_wire_operations();
	}
}
