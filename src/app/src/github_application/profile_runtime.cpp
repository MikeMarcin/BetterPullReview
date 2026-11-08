#include "github_application/profile_runtime.hpp"

#include "bpr/application/github_window_read_host.hpp"
#include "bpr/application/insights_projection.hpp"
#include "bpr/application/profile_storage_authority.hpp"
#include "bpr/application/review_queue_notification.hpp"
#include "bpr/application/review_queue_refresh.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/appearance_model.hpp"
#include "bpr/core/home_open.hpp"
#include "bpr/core/pull_request_activity.hpp"
#include "bpr/core/review_queue_attention.hpp"

#include <QDesktopServices>
#include <QUrl>

#include <algorithm>
#include <limits>
#include <span>

namespace bpr::app {
	void github_profile_runtime::recent_changed()
	{
		for_each_window([](const window_observer& aObserver) { if (aObserver.RecentChanged) { aObserver.RecentChanged(); } });
	}

	auto github_profile_runtime::ledger_observer(application::workbench_pull_request_authority aAuthority, core::bpr_window_context aOrigin)
		-> std::function<void(application::ledger_change)>
	{
		auto pending = std::make_shared<pending_ledger_changes>();
		return [weak = weak_from_this(), pending, aAuthority, aOrigin](application::ledger_change aChange) {
			const auto self = weak.lock();
			if (!self) {
				return;
			}
			const auto lock = std::scoped_lock{ pending->Mutex };
			switch (aChange.Domain) {
			case application::ledger_change_domain::receipt:
				pending->Receipts = true;
				if (!aChange.File || pending->ReceiptFiles.size() >= maximum_named_receipt_changes) {
					pending->AllReceipts = true;
					pending->ReceiptFiles.clear();
				} else if (!pending->AllReceipts) {
					pending->ReceiptFiles.push_back(*aChange.File);
				}
				break;
			case application::ledger_change_domain::conversation:    pending->Conversations = true; break;
			case application::ledger_change_domain::private_records: pending->PrivateRecords = true; break;
			}
			if (pending->Scheduled) {
				return;
			}
			pending->Scheduled = true;
			self->DecisionLane.post([weak, pending, aAuthority, aOrigin] {
				auto changes = ledger_changes{ };
				{
					const auto drainLock = std::scoped_lock{ pending->Mutex };
					changes              = {
						.Receipts       = std::exchange(pending->Receipts, false),
						.AllReceipts    = std::exchange(pending->AllReceipts, false),
						.Conversations  = std::exchange(pending->Conversations, false),
						.PrivateRecords = std::exchange(pending->PrivateRecords, false),
						.ReceiptFiles   = std::exchange(pending->ReceiptFiles, { }),
						.Origin         = aOrigin,
					};
					pending->Scheduled = false;
				}
				const auto owner = weak.lock();
				if (!owner || owner->Closing || !owner->Host || owner->Host->accepted_identity_generation() != aAuthority.IdentityGeneration) {
					return;
				}
				owner->for_each_window([&](const window_observer& aObserver) {
					if (aObserver.Ledger) {
						try {
							aObserver.Ledger(aAuthority, changes);
						} catch (...) { }
					}
				});
			});
		};
	}

	void github_profile_runtime::present_insights(core::bpr_window_context aContext, QWidget* aOpener, const qt::appearance_profile* aAppearance)
	{
		if (!current_window(aContext)) {
			return;
		}
		if (!Insights) {
			Insights = std::make_unique<qt::insights_window>();
		}
		InsightsOwner = aContext;
		Insights->set_profile_label(QString::fromStdString(Admission.Label));
		Insights->setPalette(aOpener->palette());
		Insights->setFont(aOpener->font());
		if (aAppearance) {
			Insights->set_chart_colors(aAppearance->color(core::appearance_role::state_accepted), aAppearance->color(core::appearance_role::state_unread));
		}
		const auto weak = weak_from_this();
		Insights->set_dispatch({
			.Refresh      = [weak](bool aStorage) {
				if (auto self = weak.lock(); self && !self->Closing && self->InsightsHost) {
					self->WorkMetrics->flush();
                        const auto work = !self->ProviderEffects.idle() || std::ranges::any_of(self->DiscoveryEffects, [](const worker_executor& aLane) { return !aLane.idle(); }) ? "GitHub requests" : !self->EpochEffects.idle() ? "Git cache work" : !self->DiffEffects.idle() ? "Computing diffs" : !self->SearchEffects.idle() ? "Searching source" : "Idle";
					self->InsightsHost->refresh(aStorage, std::string{ work } + "\nNext inbox refresh: on activation");
				} },
			.RevealData   = [weak] { if (auto self = weak.lock(); self && !self->Closing) { QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdU16String(self->ApplicationDataRoot.u16string()))); } },
			.Export       = [weak, aContext] { if (auto self = weak.lock()) { self->present_diagnostic_export(aContext, core::diagnostic_export_policy::anonymized); } },
			.ManageCaches = [weak, aContext] { if (auto self = weak.lock()) { self->present_settings(aContext, qt::settings_page::storage_and_privacy); } },
		});
		Insights->present(aOpener);
	}

	void github_profile_runtime::present_settings(core::bpr_window_context aContext, qt::settings_page aPage)
	{
		if (!current_window(aContext)) {
			return;
		}
		if (!Settings) {
			Settings = std::make_unique<qt::settings_window>();
			Settings->set_profile_label(QString::fromStdString(Admission.Label));
		}
		if (SettingsOwner && *SettingsOwner != aContext) {
			const auto previous = Observers.find(*SettingsOwner);
			if (previous != Observers.end() && previous->second.UnmountSettings) {
				previous->second.UnmountSettings();
			}
		}
		SettingsOwner    = aContext;
		const auto found = Observers.find(aContext);
		if (found != Observers.end() && found->second.Settings) {
			found->second.Settings(*Settings, aPage);
		}
		if (!Maintenance) {
			Maintenance = release_maintenance_controls::start_profile(ApplicationDataRoot, RuntimeLease, Admission.Authority.profile(), MaintenanceRetirement, std::dynamic_pointer_cast<application::diagnostic_control_port>(DiagnosticLog));
		}
		Maintenance->attach(*Settings, Admission.Label);
		if (Diagnostics) {
			Diagnostics->attach(*Settings);
		}
	}

	void github_profile_runtime::present_diagnostic_export(core::bpr_window_context aContext, core::diagnostic_export_policy aPolicy)
	{
		if (!current_window(aContext)) {
			return;
		}
		if (!Settings) {
			Settings = std::make_unique<qt::settings_window>();
			Settings->set_profile_label(QString::fromStdString(Admission.Label));
		}
		if (!Maintenance) {
			Maintenance = release_maintenance_controls::start_profile(ApplicationDataRoot, RuntimeLease, Admission.Authority.profile(), MaintenanceRetirement, std::dynamic_pointer_cast<application::diagnostic_control_port>(DiagnosticLog));
		}
		Maintenance->attach(*Settings, Admission.Label);
		Maintenance->open_export(aPolicy);
	}

	auto github_profile_runtime::current(const core::bpr_window_context& aContext) const noexcept -> bool { return current_window(aContext); }

	github_profile_runtime::epoch_contexts::epoch_contexts(github_profile_runtime& aOwner)
		: Owner{ aOwner }
	{ }

	auto github_profile_runtime::epoch_contexts::current(core::bpr_window_context aContext) const noexcept -> bool { return Owner.current_window(aContext); }

	auto github_profile_runtime::current_window(core::bpr_window_context aContext) const noexcept -> bool
	{
		if (Closing || aContext.Profile != Admission.Authority.profile() || !Observers.contains(aContext)) {
			return false;
		}
		try {
			return !WindowAdmission || WindowAdmission(aContext);
		} catch (...) {
			return false;
		}
	}

	auto github_profile_runtime::drafts(const application::shared_draft_authority& aAuthority) -> std::optional<draft_owner>
	{
		for (const auto& owner : DraftOwners) {
			if (owner.Host->authority() == aAuthority) {
				return owner;
			}
		}
		if (Closing || DraftOwners.size() >= application::maximum_shared_drafts || LastDraftScope == std::numeric_limits<std::uint64_t>::max()) {
			return std::nullopt;
		}
		auto scope = core::review_draft_scope_id{ ++LastDraftScope };
		auto owner = draft_owner{ scope, std::make_shared<application::shared_draft_application_host>(aAuthority, scope, *this) };
		DraftOwners.push_back(owner);
		return owner;
	}

	auto github_profile_runtime::markdown_image_media_factory(adapters::github::detail::github_provider::media_transport_factory aProduction)
		-> adapters::github::detail::github_provider::media_transport_factory
	{
#if defined(BPR_BUILD_TESTING)
		if (Fixture) {
			return Fixture->markdown_image_media_factory();
		}
#endif
		return aProduction;
	}

	void github_profile_runtime::publish(const application::github_presentation_update& aUpdate)
	{
		reconcile_release_access();
		// Invalidation is presented before the host retires its previous
		// accepted model. Remember only when an accepted session is delivered.
		if (aUpdate.Session) {
			observe_connection();
		}
		if (aUpdate.Transition && aUpdate.Transition->Identity && aUpdate.Transition->Identity->Generation != QueueIdentityGeneration) {
			QueueIdentityGeneration = aUpdate.Transition->Identity->Generation;
			if (QueueHost) {
				QueueHost->cancel();
			}
			if (QueueRefresh) {
				QueueRefresh->clear();
			}
			QueueActivity.reset();
			QueueActivityBaseline.reset();
			ActiveQueueNotifications.clear();
		}
		bind_device_refresh_authority(aUpdate);
		observe_restoration_session();
		if (aUpdate.Transition && aUpdate.Transition->Status) {
			recent_changed();
		}
		if (ReviewRestoration && RestorationStartup && ReviewRestoration->current_startup(RestorationStartup->Request)
			&& aUpdate.Transition && aUpdate.Transition->Status
			&& (aUpdate.Transition->Status->Status == core::github_status::session_failed || aUpdate.Transition->Status->Status == core::github_status::session_cancelled)) {
			ReviewRestoration->complete_startup(RestorationStartup->Request, std::unexpected(core::window_review_failure::unavailable));
		}
		record_cli_account_use();
		for_each_window([&](const window_observer& aObserver) { if (aObserver.Github){ aObserver.Github(aUpdate);
} });
		adopt_discovery();
		// A session change can end a window's read without a read update of its
		// own, so Home's discovery asks again whether it gives way.
		yield_discovery();
	}

	void github_profile_runtime::adopt_discovery()
	{
		// [[BPR-0990]]. The accepted account session, or none. The same session
		// again changes nothing, so every presentation may ask.
		if (Closing || Discovery == nullptr || Host == nullptr) {
			return;
		}
		const auto session = Host->access_mode() == core::github_access_mode::authenticated ? Host->accepted_profile_session() : std::nullopt;
		Discovery->adopt(session ? application::personal_discovery_session_of(*session) : std::nullopt);
		adopt_activity();
	}

	void github_profile_runtime::adopt_activity()
	{
		if (Closing || Activity == nullptr || Host == nullptr) {
			return;
		}
		const auto session = Host->access_mode() == core::github_access_mode::authenticated ? Host->accepted_profile_session() : std::nullopt;
		Activity->adopt(session ? application::personal_discovery_session_of(*session) : std::nullopt);
		present_activity();
	}

	void github_profile_runtime::present_activity()
	{
		if (Closing || Activity == nullptr) {
			return;
		}
		auto status = QString{ };
		switch (Activity->state()) {
		case application::pull_request_activity_state::off:         status = QStringLiteral("Off."); break;
		case application::pull_request_activity_state::signed_out:  status = QStringLiteral("Waiting for a GitHub sign-in."); break;
		case application::pull_request_activity_state::unsupported: status = QStringLiteral("Needs a GitHub CLI sign-in."); break;
		case application::pull_request_activity_state::watching:    status = QStringLiteral("Checking GitHub every minute."); break;
		case application::pull_request_activity_state::waiting:     status = QStringLiteral("GitHub is not answering; checking less often."); break;
		}
		const auto enabled = Activity->enabled();
		for_each_window([&](const window_observer& aObserver) {
			if (aObserver.PresentActivity) {
				aObserver.PresentActivity(enabled, status);
			}
		});
	}

	void github_profile_runtime::set_activity_enabled(const bool aEnabled)
	{
		if (Closing || Activity == nullptr) {
			return;
		}
		Activity->set_enabled(aEnabled);
		present_activity();
	}

	auto github_profile_runtime::activity_focus() -> std::vector<core::pull_request_ref>
	{
		auto focused = std::vector<core::pull_request_ref>{ };
		for_each_window([&](const window_observer& aObserver) {
			if (aObserver.FocusedPullRequest) {
				if (auto ref = aObserver.FocusedPullRequest()) {
					focused.push_back(std::move(*ref));
				}
			}
		});
		return focused;
	}

	void github_profile_runtime::activity_arrived(const std::vector<core::pull_request_ref>& aActive)
	{
		if (Closing || aActive.empty()) {
			return;
		}
		for_each_window([&](const window_observer& aObserver) {
			if (aObserver.ActivityArrived) {
				aObserver.ActivityArrived(aActive);
			}
		});
	}

	void github_profile_runtime::activate_activity()
	{
		if (Closing || Activity == nullptr) {
			return;
		}
		const auto target = Activity->activate();
		// The focused window first, then any: it opens the pull request, or for
		// a summary only comes forward on what it shows.
		auto       opened = false;
		const auto open   = [&](const window_observer& aObserver) {
			if (opened || !aObserver.OpenNotified) {
				return;
			}
			opened = aObserver.OpenNotified(target.PullRequest);
		};
		if (Focused) {
			if (const auto found = Observers.find(*Focused); found != Observers.end() && !found->second.Lifetime.expired()) {
				open(found->second);
			}
		}
		for_each_window(open);
	}

	void github_profile_runtime::yield_discovery()
	{
		if (Closing || Discovery == nullptr) {
			return;
		}
		const auto yields = [this] {
			auto lanes = std::vector<core::github_state>{ };
			for_each_window([&lanes](const window_observer& aObserver) {
				if (aObserver.ReadLane) {
					if (auto lane = aObserver.ReadLane()) {
						lanes.push_back(std::move(*lane));
					}
				}
			});
			// [[BPR-1680]]. Discovery on lanes of its own never holds up a read.
			const auto session = Host != nullptr && Host->access_mode() == core::github_access_mode::authenticated ? Host->accepted_profile_session() : std::nullopt;
			const auto beside  = Provider != nullptr && session && Provider->discovery_beside_reads(session->AccountTransport);
			return core::home_loading_yields(lanes, beside);
		};
		// Given way at once, in the turn of the gesture that asked for the
		// read, so the lane reaches that read next.
		if (yields()) {
			Discovery->yield(true);
			return;
		}
		if (!Discovery->yielding() || DiscoveryResumePosted) {
			return;
		}
		// Resumed on its own turn, so discovery never publishes into the window
		// read update being delivered; asked again then, so a read begun in
		// between keeps it waiting.
		DiscoveryResumePosted = true;
		try {
			DecisionLane.post([this, yields] {
				DiscoveryResumePosted = false;
				if (!Closing && Discovery != nullptr) {
					Discovery->yield(yields());
				}
			});
		} catch (...) {
			DiscoveryResumePosted = false;
			Discovery->yield(false);
		}
	}

	void github_profile_runtime::observe_recent_drafts(const application::queue_landing_projection& aProjection)
	{
		const auto session   = Host->access_mode() == core::github_access_mode::authenticated ? Host->accepted_profile_session() : std::nullopt;
		const auto discovery = session ? application::personal_discovery_session_of(*session) : std::nullopt;
		if (ReviewRestoration && discovery && discovery->Queue == aProjection.Session) {
			const auto observed = aProjection.UpdatedItem ? std::span<const core::queue_item>{ &*aProjection.UpdatedItem, 1U } : std::span<const core::queue_item>{ aProjection.Items };
			for (const auto& item : observed) {
				const auto split = item.Ref.Repository.find('/');
				if (item.Facts.Draft && item.Ref.Host == discovery->Queue.Host && item.Ref.Account == discovery->Queue.Account && split != std::string::npos && item.Ref.Number > 0 && item.Ref.Number <= core::maximum_pull_request_number) {
					ReviewRestoration->accepted_draft({ core::github_access_mode::authenticated, item.Ref.Host, { item.Ref.Repository.substr(0, split), item.Ref.Repository.substr(split + 1) }, core::pull_request_number{ static_cast<std::uint32_t>(item.Ref.Number) }, discovery->Viewer.Account }, item.Facts.Draft);
				}
			}
		}
	}

	void github_profile_runtime::publish(
		const application::queue_landing_projection& aProjection)
	{
		if (Closing) {
			return;
		}
		// [[BPR-0990]]. A repository saved since discovery last searched is
		// searched now. Posted, so discovery never publishes into the queue
		// projection that is being delivered; coalesced, so a burst of
		// projections asks once.
		if (!aProjection.UpdatedItem && Discovery != nullptr && !DiscoveryReconcilePosted) {
			DiscoveryReconcilePosted = true;
			try {
				DecisionLane.post([this] {
					DiscoveryReconcilePosted = false;
					if (!Closing && Discovery != nullptr) {
						Discovery->reconcile();
					}
				});
			} catch (...) {
				DiscoveryReconcilePosted = false;
			}
		}

		observe_recent_drafts(aProjection);

		// Presentation receives the exact accepted projection before any
		// notification can activate back into it.

		if (aProjection.UpdatedItem && (!QueueActivity || QueueActivity->Ref != aProjection.UpdatedItem->Ref)) {
			return;
		}
		const auto items  = aProjection.UpdatedItem ? std::span<const core::queue_item>{ &*aProjection.UpdatedItem, 1U } : std::span<const core::queue_item>{ aProjection.Items };
		auto       active = std::vector<queue_notification_transition>{ };
		active.reserve(items.size());
		for (const auto& item : items) {
			// [[BPR-1780]]. Compared as the queue decided it, with the
			// item's draft status.
			auto activity              = QueueActivity.has_value() ? QueueActivity->Activity : core::queue_activity_observation{ };
			activity.Draft             = item.Facts.Draft;
			const auto trustedActivity = QueueActivity.has_value()
				&& QueueActivity->Session == aProjection.Session
				&& QueueActivity->Ref == item.Ref
				&& item.Bucket
					== core::bucket_for(
						item.Membership, activity);
			if (!trustedActivity
				|| !application::warrants_notification(item.Bucket)) {
				continue;
			}
			const auto transition = queue_notification_transition{
				.Session = aProjection.Session,
				.Ref     = item.Ref,
				.Bucket  = item.Bucket,
			};
			active.push_back(transition);
			if (std::ranges::binary_search(
					ActiveQueueNotifications, transition)) {
				continue;
			}
#if defined(BPR_BUILD_TESTING)
			if (DisableNotificationRequestWire) {
				continue;
			}
#endif
			const auto unseen  = QueueActivity->Activity.UnseenComments;
			const auto request = application::notification_for(item, unseen);
			if (!request.has_value() || !core::permits_notification(aProjection.Preferences.Notifications, item.Bucket)) {
				continue;
			}
			++NotificationRequestsAttempted;
			if (Notifications != nullptr) {
				static_cast<void>(Notifications->deliver(*request));
			}
		}
		std::ranges::sort(active);
		const auto unique = std::ranges::unique(active);
		active.erase(unique.begin(), unique.end());
		ActiveQueueNotifications = std::move(active);
	}
}
