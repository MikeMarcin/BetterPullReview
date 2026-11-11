#include "github_application/window_composition.hpp"

#include "bpr/application/window_review_restoration_host.hpp"
#include "bpr/qt/event_loop_executor.hpp"

#include <QWidget>

#include <algorithm>

#if defined(BPR_BUILD_TESTING)
	#include "bpr/application/workbench_preferences_host.hpp"
#endif

namespace bpr::app {
	void github_application::implementation::begin_shutdown(window_retirement aMode)
	{
		if (Closing) {
			if (aMode == window_retirement::forced && Retirement == window_retirement::orderly_quit) {
				Retirement                  = aMode;
				Profile->RestorationOrderly = false;
				if (Profile->ReviewRestoration) {
					Profile->ReviewRestoration->shutdown();
				}
				if (WorkbenchHost) {
					WorkbenchHost->shutdown();
				}
			}
			return;
		}
		Retirement = aMode;
		if (Profile->ReviewRestoration) {
			if (aMode == window_retirement::explicit_close) {
				Profile->ReviewRestoration->close(DraftWindow);
			} else if (aMode == window_retirement::orderly_quit) {
				Profile->RestorationOrderly = true;
				Profile->ReviewRestoration->shutdown_orderly();
			} else {
				Profile->ReviewRestoration->cancel_restore(DraftWindow);
			}
		}
		PendingRestoration.reset();
		RestorationReadRequest.reset();
		Window->unmount_settings();
		if (Profile->SettingsOwner == DraftWindow) {
			if (Profile->Settings) {
				Profile->Settings->hide();
			}
			Profile->SettingsOwner.reset();
		}
		if (Profile->InsightsOwner == DraftWindow) {
			if (Profile->Insights) {
				Profile->Insights->hide();
			}
			Profile->InsightsOwner.reset();
		}
		Profile->Observers.erase(DraftWindow);
		// A window closed while it read no longer holds Home's discovery back
		// (`core::home_loading_yields`).
		Profile->yield_discovery();
		Profile->RestorationNotices.erase(DraftWindow);
		if (QueueConsumer) {
			QueueHost->detach(*QueueConsumer);
			QueueConsumer.reset();
		}
		if (SharedDrafts) {
			SharedDrafts->detach(DraftConsumer);
		}
		if (ReadDecision) {
			ReadDecision->hold(false);
		}
		if (WindowRead) {
			WindowRead->shutdown();
			Host->detach_window(DraftWindow);
		}
#if defined(BPR_AGENT_INTEGRATIONS)
		if (McpHelper) {
			McpHelper->stop();
			McpHelper->unpin();
		}
#endif
		retire_setup_effects();
		if (SetupDestination) {
			SetupDestination->cancel();
		}
		Closing = true;
		RetryableReviewDraft.reset();
#if defined(BPR_AGENT_INTEGRATIONS)
		// The one outstanding re-alignment is told to stop here rather
		// than waited on: its ports are closing, and a request that kept
		// resolving through them would be the hang the poll below reports
		// as a timeout.
		if (ContinuationRealignmentStop) {
			static_cast<void>(ContinuationRealignmentStop->request_stop());
		}
		if (AgentHost) {
			AgentHost->shutdown();
		}
		if (TourSession) {
			TourSession->shutdown();
		}
		if ((!AgentHost || AgentHost->is_quiescent()) && (!TourSession || TourSession->is_quiescent())) {
			begin_other_shutdown();
		}
#else
		begin_other_shutdown();
#endif
		schedule_shutdown_poll();
	}

	void github_application::implementation::begin_other_shutdown()
	{
		if (OtherShutdownBegun) {
			return;
		}
		OtherShutdownBegun = true;
		TourSourceStop.request_stop();
		PreparationStop.request_stop();
		if (PrivateReview) {
			PrivateReview->shutdown();
		}
		Profile->WorkbenchPreferences->unsubscribe(WritingHookSubscription);
#if defined(BPR_AGENT_INTEGRATIONS)
		if (ClaudeWrapperEditor) {
			ClaudeWrapperEditor->shutdown();
		}
		AgentSetupWorkspaceStop.request_stop();
		if (AgentSetup) {
			AgentSetup->shutdown();
		}
		if (CloneLinks) {
			CloneLinks->shutdown();
		}
		if (ReviewSkill) {
			ReviewSkill->shutdown();
		}
		if (AgentConversation) {
			AgentConversation->shutdown();
		}
		if (AgentFindingCards) {
			AgentFindingCards->shutdown();
		}
#endif
		if (TourHost) {
			TourHost->shutdown();
		}
		if (CycleDesktop) {
			CycleDesktop->shutdown();
		}
		for (auto& retained : RetiredTourHosts) {
			retained.Host->shutdown();
		}
#if defined(BPR_BUILD_TESTING)
		if (ReviewSubmissionDecisionGate) {
			ReviewSubmissionDecisionGate->release();
		}
		if (ReviewLedgerDecisionGate) {
			ReviewLedgerDecisionGate->release();
		}
#endif
		if (ReviewSubmission) {
			ReviewSubmission->shutdown();
			if (TourPublication) {
				TourPublication->shutdown();
				if (ConversationReply) {
					ConversationReply->shutdown();
				}
			}
		}
		// Every composed host must be told, or the shutdown poll waits on a
		// host that was never asked to stop and the application never
		// exits. Adding a host to the composition without adding it here is
		// a hang, not a leak, and it presents as a test timeout rather than
		// as anything that names the cause.
		if (WorkbenchHost) {
			if (Retirement == window_retirement::orderly_quit) {
				WorkbenchHost->shutdown_orderly();
			} else {
				WorkbenchHost->shutdown();
			}
		}
		if (TabHost) {
			TabHost->shutdown();
		}
		if (AppearanceHost) {
			AppearanceHost->shutdown();
		}
		if (MarkdownCode) {
			MarkdownCode->shutdown();
		}
		if (MarkdownSnippets) {
			MarkdownSnippets->shutdown();
		}
		if (ReviewMutationHost) {
			ReviewMutationHost->shutdown();
		}
		for (auto& host : RetiredReviewMutationHosts) {
			host->shutdown();
		}
		if (PrFind) {
			PrFind->shutdown();
		}
		if (PaneFind) {
			PaneFind->shutdown();
		}
		DiffHost->shutdown();
		EpochHost->shutdown();
	}

	void github_application::implementation::schedule_shutdown_poll()
	{
		if (ShutdownComplete || ShutdownPollScheduled || Window == nullptr) {
			return;
		}
		ShutdownPollScheduled = true;
		QTimer::singleShot(
			0,
			Window.get(),
			[this] {
				ShutdownPollScheduled = false;
				poll_shutdown();
			});
	}

	void github_application::implementation::poll_shutdown()
	{
		if (!Closing || ShutdownComplete) {
			return;
		}
#if defined(BPR_AGENT_INTEGRATIONS)
		if (AgentHost && !AgentHost->is_quiescent()) {
			AgentHost->retry_cleanup();
			schedule_shutdown_poll();
			return;
		}
		if (TourSession && !TourSession->is_quiescent()) {
			TourSession->retry_cleanup();
			schedule_shutdown_poll();
			return;
		}
		if ((McpHelper && McpHelper->running())
			|| ContinuationRealignmentsInFlight.load(std::memory_order_acquire) != 0U) {
			begin_other_shutdown();
			schedule_shutdown_poll();
			return;
		}
#endif
		begin_other_shutdown();
		if (
			!DiffHost->is_quiescent() || !EpochHost->is_quiescent() || (PaneFind && !PaneFind->is_quiescent()) || (PrFind && !PrFind->is_quiescent()) || ReadingAdvanceQueued
			|| TourSourceReadsInFlight.load(std::memory_order_acquire) != 0U
			|| PreparationsInFlight.load(std::memory_order_acquire) != 0U
			|| (WindowRead && !WindowRead->is_quiescent())
			|| (WorkbenchHost && !WorkbenchHost->is_quiescent())
			|| (TabHost && !TabHost->is_quiescent()) || TabAdvanceQueued
			|| (AppearanceHost && !AppearanceHost->is_quiescent())
			|| (MarkdownCode && !MarkdownCode->is_quiescent())
			|| (MarkdownSnippets && !MarkdownSnippets->is_quiescent())
			|| (ReviewSubmission && !ReviewSubmission->is_quiescent())
			|| (TourPublication && !TourPublication->is_quiescent())
			|| (ConversationReply && !ConversationReply->is_quiescent())
			|| (TourHost && !TourHost->is_quiescent())
			|| (CycleDesktop && !CycleDesktop->is_quiescent())
#if defined(BPR_AGENT_INTEGRATIONS)
			|| (ClaudeWrapperEditor && !ClaudeWrapperEditor->is_quiescent())
			|| (AgentSetup && !AgentSetup->is_quiescent())
			|| (CloneLinks && !CloneLinks->is_quiescent())
			|| (ReviewSkill && !ReviewSkill->is_quiescent())
			|| (AgentConversation && !AgentConversation->is_quiescent())
			|| (AgentFindingCards && !AgentFindingCards->is_quiescent())
#endif
			|| std::ranges::any_of(RetiredTourHosts, [](const auto& aRetained) { return !aRetained.Host->is_quiescent(); })
			|| (PrivateReview && !PrivateReview->is_quiescent())
			|| (ReviewMutationHost && !ReviewMutationHost->is_quiescent())
			|| std::ranges::any_of(
				RetiredReviewMutationHosts,
				[](const auto& aHost) { return !aHost->is_quiescent(); })) {
			schedule_shutdown_poll();
			return;
		}

		if (Profile->ReviewRestoration && Retirement == window_retirement::explicit_close && !Profile->ReviewRestoration->window_quiescent(DraftWindow)) {
			schedule_shutdown_poll();
			return;
		}
		if (DurableWindowLayout && Retirement == window_retirement::orderly_quit
			&& DurableWindowLayout->write_failed()) {
			// BPR-0899: the host above drains the active and latest coalesced
			// writes before cleanup reads their final result. BPR-0875's
			// counters name real refusals; in-flight-at-drain is unreachable.
			const auto failure = DurableWindowLayout->last_write_failure();
			note_cleanup_failure(
				std::string{ "durable-layout-write:window=" }
				+ std::to_string(DraftWindow.Window.value()) + ":attempted="
				+ std::to_string(DurableWindowLayout->writes_attempted()) + ":completed="
				+ std::to_string(DurableWindowLayout->writes_completed()) + ":"
				+ (failure == application::workbench_settings_failure::cancelled
					   ? "cancelled"
					   : "unavailable"));
			ShutdownCleanupFailed = true;
		}
		if (Profile->WindowLayouts && Retirement == window_retirement::explicit_close && !LayoutEraseComplete) {
			if (!LayoutEraseStarted) {
				LayoutEraseStarted = true;
				auto  weak         = std::weak_ptr<const int>{ WindowLifetime };
				auto  service      = Profile->WindowLayouts;
				auto  context      = DraftWindow;
				auto* lane         = &DecisionLane;
				auto  delivery     = LayoutEraseDeliveryFailed;
				try {
					WorkbenchEffects.post([this, weak, service, context, lane, delivery] {
						auto success = false;
						// [[BPR-0875]]. The erase's own refusal, carried back so the
						// shutdown can name it: the failure it answered, or the
						// exception it threw.
						auto refusal = std::string{ "threw" };
						try {
							const auto erased = service->erase(context.Window);
							success           = erased.has_value();
							if (!success) {
								refusal = erased.error().Failure == application::workbench_settings_failure::cancelled
									? "cancelled"
									: "unavailable";
							}
						} catch (...) { }
						try {
							lane->post([this, weak, context, success, refusal] {
								if (weak.expired() || DraftWindow != context) {
									return;
								}
								if (!success) {
									note_cleanup_failure(
										std::string{ "layout-erase:window=" }
										+ std::to_string(context.Window.value()) + ":" + refusal);
									ShutdownCleanupFailed = true;
								}
								LayoutEraseComplete = true;
							});
						} catch (...) {
							delivery->store(true, std::memory_order_release);
						}
					});
				} catch (...) {
					LayoutEraseComplete = true;
					note_cleanup_failure("layout-erase:post-refused");
					ShutdownCleanupFailed = true;
				}
			}
			if (LayoutEraseDeliveryFailed->load(std::memory_order_acquire)) {
				LayoutEraseComplete = true;
				note_cleanup_failure("layout-erase:delivery-refused");
				ShutdownCleanupFailed = true;
			}
			if (!LayoutEraseComplete) {
				schedule_shutdown_poll();
				return;
			}
		}

		EpochProjectionDeliveryFailed = EpochHost->projection_delivery_failed();
		ReviewSubmission.reset();
		TourPublication.reset();
		ConversationReply.reset();
		ReviewMutationHost.reset();
		RetiredReviewMutationHosts.clear();
#if defined(BPR_AGENT_INTEGRATIONS)
		if (!McpSnapshotRoot.empty()) {
			std::error_code error;
			std::filesystem::remove_all(McpSnapshotRoot, error);
			if (error) {
				// [[BPR-0875]]. The path and the platform's own words: a
				// directory still held open and a read-only entry are
				// different faults with the same flag.
				note_cleanup_failure(
					std::string{ "mcp-snapshot-remove:" } + McpSnapshotRoot.string() + ":"
					+ std::to_string(error.value()) + " " + error.message());
				ShutdownCleanupFailed = true;
			}
		}
#endif
		ShutdownComplete = true;
		if (OnClosed) {
#if defined(BPR_BUILD_TESTING)
			Profile->WindowCloseDecisionGate.post(std::exchange(OnClosed, { }));
#else
			DecisionLane.post(std::exchange(OnClosed, { }));
#endif
		}
	}

	void github_application::implementation::note_cleanup_failure(std::string aDetail)
	{
		if (ShutdownCleanupDetail.empty()) {
			ShutdownCleanupDetail = std::move(aDetail);
		}
	}
}
