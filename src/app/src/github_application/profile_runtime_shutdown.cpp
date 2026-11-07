#include "github_application/profile_runtime.hpp"

#include "github_application/cli_accounts.hpp"

#include <QObject>

#include <algorithm>

namespace bpr::app {
	void github_profile_runtime::note_profile_cleanup_failure(std::string aDetail)
	{
		if (CleanupDetail.empty()) {
			CleanupDetail = aDetail.empty() ? std::string{ "unnamed" } : std::move(aDetail);
		}
	}

	void github_profile_runtime::begin_shutdown()
	{
		Closing = true;
		if (EnergyTimer) {
			EnergyTimer->stop();
		}
		QObject::disconnect(EnergyActivation);
		WorkMetrics.reset();
		if (EnergySampler) {
			EnergySampler->shutdown();
		}
		if (Insights) {
			Insights->hide();
			Insights->set_dispatch({ });
		}
		if (InsightsHost) {
			InsightsHost->shutdown();
		}
		cli_account_users().set(this, std::nullopt);
#if defined(BPR_LOCAL_AUTOMATION)
		// A forwarded cycle command stops waiting and cancels its store work.
		CycleOwnerStop.request_stop();
#endif
		if (Diagnostics) {
			Diagnostics->shutdown();
		}
		if (Maintenance) {
			Maintenance->shutdown();
		}
		if (ServicesClosing) {
			return;
		}
		ServicesClosing = true;
		if (ReviewRestoration) {
			if (RestorationOrderly) {
				ReviewRestoration->shutdown_orderly();
			} else {
				ReviewRestoration->shutdown();
			}
		}
		for (auto& owner : DraftOwners) {
			owner.Host->shutdown();
		}
		if (DeviceHost) {
			DeviceHost->shutdown();
		}
		if (Connections) {
			Connections->shutdown();
		}
		if (Host) {
			Host->shutdown();
		}
		if (Activity) {
			Activity->shutdown();
		}
		if (Discovery) {
			Discovery->shutdown();
		}
		if (QueueHost) {
			QueueHost->shutdown();
		}
		if (AppearancePreferences) {
			AppearancePreferences->shutdown();
		}
		if (WorkbenchPreferences) {
			WorkbenchPreferences->shutdown();
		}
		if (ViewFilterMemory) {
			ViewFilterMemory->shutdown();
		}
		if (EpochActors) {
			EpochActors->shutdown();
		}
		if (Images) {
			Images->shutdown();
		}
	}

	void github_profile_runtime::poll_shutdown()
	{
		if (!ServicesClosing || ShutdownComplete) {
			return;
		}
		if (InsightsInitializing || !InsightsEffects.idle() || (InsightsHost && !InsightsHost->quiescent()) || (Diagnostics && !Diagnostics->quiescent()) || (Maintenance && !Maintenance->quiescent())) {
			return;
		}
		if ((Connections && !Connections->quiescent()) || ReleaseAccessPending != 0U || (ReviewRestoration && !ReviewRestoration->is_quiescent()) || (Host && !Host->is_quiescent()) || (DeviceHost && !DeviceHost->is_quiescent())
			|| (DevicePersistence && !DevicePersistence->is_quiescent()) || (QueueHost && !QueueHost->is_quiescent()) || (Discovery && !Discovery->is_quiescent())
			|| (AppearancePreferences && !AppearancePreferences->is_quiescent()) || (WorkbenchPreferences && !WorkbenchPreferences->is_quiescent())
			|| (ViewFilterMemory && !ViewFilterMemory->quiescent())
			|| (EpochActors && !EpochActors->is_quiescent()) || AgentWorkspaceRecoveryPending.load(std::memory_order_acquire)
			|| (Images && !Images->quiescent())) {
			return;
		}
		if (!ImageDecodeEffects.idle() || std::ranges::any_of(ImageFetchEffects, [](const worker_executor& aLane) { return !aLane.idle(); })
			|| std::ranges::any_of(DiscoveryEffects, [](const worker_executor& aLane) { return !aLane.idle(); })) {
			return;
		}
		if (!ConnectionEffects.idle() || !ProviderEffects.idle() || !VaultEffects.idle() || !DeviceBindingEffects.idle() || !DeviceTransportEffects.idle()
			|| !EpochEffects.idle() || !DiffEffects.idle() || !SearchEffects.idle() || !PrepareEffects.idle()
			|| !WorkbenchEffects.idle() || !AppearanceEffects.idle() || !QueueEffects.idle()
			|| !ReviewMutationEffects.idle() || !ReviewSubmissionEffects.idle() || !ReviewLedgerEffects.idle()) {
			return;
		}
#if defined(BPR_AGENT_INTEGRATIONS)
		if (!AgentEffects.idle() || !McpEffects.idle() || !TourAgentEffects.idle()) {
			return;
		}
#endif
#if defined(BPR_LOCAL_AUTOMATION)
		if (!CycleOwnerEffects.idle()) {
			return;
		}
#endif
		if (!AgentWorkspaceCleanupAttempted) {
			AgentWorkspaceCleanupAttempted = true;
			if (AgentWorkspaceParent
				&& !adapters::git::remove_installed_agent_workspace_parent(*AgentWorkspaceParent)) {
				// [[BPR-0875]]. Distinct from the recovery refusal above: this
				// one is a removal at quit, and only this one is about a path
				// that would not go away.
				note_profile_cleanup_failure(
					std::string{ "agent-workspace-parent-remove:" } + AgentWorkspaceParent->Path.string());
				AgentWorkspaceCleanupFailed = true;
			}
		}
		// [[BPR-0875]]. The profile's own flag folds five sources together,
		// and a real quit turns it into exit 1, so each one is named here as
		// it is folded in. The window host's own detail is taken whole: it
		// already says which of its three cleanups refused and why.
		if (ReviewRestoration && ReviewRestoration->projection_delivery_failed()) {
			note_profile_cleanup_failure("review-restoration-projection-delivery");
			CleanupFailed = true;
		}
		if (UpdateShutdown && ((ReviewRestoration && !ReviewRestoration->durable()) || (AppearancePreferences && !AppearancePreferences->durable()) || (WorkbenchPreferences && WorkbenchPreferences->delivery_failed()))) {
			note_profile_cleanup_failure("update-state-flush");
			CleanupFailed = true;
		}
		// [[BPR-0817]]. Only the quit's own workspace cleanup reaches this fold:
		// the removal above, or a recovery that could not even be posted. Each
		// was named where it was raised. A recovery refusal does not reach it.
		// The refusal is decided at startup and stated in every window, and
		// under RFC-BPR-0033's owner-delegated amendment an orderly quit still
		// exits 0.
		if (AgentWorkspaceCleanupFailed) {
			note_profile_cleanup_failure("agent-workspace-cleanup:unnamed-origin");
			CleanupFailed = true;
		}
		if (AgentWorkspaceRecoveryPostFailed.load(std::memory_order_acquire)) {
			note_profile_cleanup_failure("agent-workspace-recovery-post");
			CleanupFailed = true;
		}
		if (Host && Host->shutdown_cleanup_failed()) {
			note_profile_cleanup_failure("application-host-shutdown-cleanup");
			CleanupFailed = true;
		}
		Provider->shutdown();
		AnonymousProvider->shutdown();
		ProfileCatalog->shutdown();
		SearchEffects.close_and_drain();
		DiffEffects.close_and_drain();
		PrepareEffects.close_and_drain();
		EpochEffects.close_and_drain();
#if defined(BPR_AGENT_INTEGRATIONS)
		AgentEffects.close_and_drain();
		McpEffects.close_and_drain();
		TourAgentEffects.close_and_drain();
#endif
		InsightsEffects.close_and_drain();
		ConnectionEffects.close_and_drain();
		WorkbenchEffects.close_and_drain();
		AppearanceEffects.close_and_drain();
		QueueEffects.close_and_drain();
		ReviewMutationEffects.close_and_drain();
		ReviewSubmissionEffects.close_and_drain();
		ReviewLedgerEffects.close_and_drain();
		for (auto& lane : ImageFetchEffects) {
			lane.close_and_drain();
		}
		ImageDecodeEffects.close_and_drain();
		for (auto& lane : DiscoveryEffects) {
			lane.close_and_drain();
		}
		ProviderEffects.close_and_drain();
		VaultEffects.close_and_drain();
		DeviceTransportEffects.close_and_drain();
		DeviceBindingEffects.close_and_drain();
		// [[BPR-1831]]. Held-back storage, image and search ends first.
		StorageDiagnostics.reset();
		if (OperationAggregate) {
			OperationAggregate->flush();
		}
		if (DiagnosticLog) {
			static_cast<void>(DiagnosticLog->record({ .Operation = application::diagnostic_operation::application, .Outcome = application::diagnostic_outcome::shutdown }));
		}
		if (OwnedDiagnosticLog) {
			OwnedDiagnosticLog->close();
		}
		ShutdownComplete = true;
	}
}
