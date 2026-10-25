#include "bpr/app/profile_desktop.hpp"
#include "about_desktop.hpp"
#include "bpr/adapters/sqlite/profile_registry_store.hpp"
#include "bpr/adapters/sqlite/release_update_recovery_store.hpp"
#include "bpr/app/worker_executor.hpp"
#include "bpr/application/profile_management_host.hpp"
#include "bpr/qt/desktop_menu_bar.hpp"
#include "bpr/qt/dif_runtime.hpp"
#include "bpr/qt/event_loop_executor.hpp"
#include "bpr/qt/motion_observer.hpp"
#include "bpr/qt/profile_management_window.hpp"
#include "bpr/qt/release_update_recovery_dialog.hpp"
#include "terminal_diagnostics.hpp"
#if defined(BPR_BUILD_TESTING)
	#include "github_application_fixture_access.hpp"
#endif
#if defined(__APPLE__)
	#include "bpr/adapters/update/release_update_installation.hpp"
	#include "bpr/qt/menu_bar_status_item.hpp"
#endif
#include <QCloseEvent>
#include <QElapsedTimer>
#include <QEvent>
#include <QGuiApplication>
#include <QObject>
#include <QPointer>
#include <QSystemTrayIcon>
#include <QTimer>
#include <algorithm>
#include <exception>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string_view>
#include <utility>

namespace bpr::app {

	class profile_desktop::implementation final : public QObject,
												  public application::profile_management_projection_port,
												  public std::enable_shared_from_this<implementation>
	{
#if defined(BPR_BUILD_TESTING)
		friend void detail::github_application_fixture_access::poll_desktop(profile_desktop&);
#endif
		struct window_entry
		{
			core::bpr_window_context       Context;
			bool                           Opened  = false;
			bool                           Closing = false;
			bool                           Restore = false;
			QPointer<qt::workbench_window> Presented;
			/// [[BPR-1342]]. The pull request this fresh window opens after Home.
			std::optional<core::window_review_destination> Review{ };
		};
		struct runtime_entry
		{
			std::unique_ptr<github_application_startup>            Startup;
			std::unique_ptr<github_application>                    Runtime;
			std::unique_ptr<qt::release_update_recovery_dialog>    Recovery;
			bool                                                   RecoveryReady     = false;
			bool                                                   RecoveryPresented = false;
			std::map<core::bpr_window_id, window_entry>            Windows;
			std::vector<core::bpr_window_context>                  Order;
			std::uint64_t                                          StartupSerial        = 0;
			bool                                                   Started              = false;
			bool                                                   Closing              = false;
			bool                                                   MaintenanceClosing   = false;
			bool                                                   WindowPublished      = false;
			bool                                                   ActivationPolling    = false;
			bool                                                   TourOwnerPolling     = false;
			bool                                                   ActivationSubmitting = false;
			std::shared_ptr<application::profile_activation_reply> ActivationReply;
			core::profile_launch_intent                            ActivationIntent = core::profile_launch_intent::activate;
		};
		struct maintenance_entry
		{
			std::unique_ptr<release_maintenance_controls>                Controls;
			std::function<void(application::release_maintenance_result)> Completion;
			bool                                                         Retired = false;
			bool                                                         Failed  = false;
			auto                                                         result() const -> application::release_maintenance_result
			{
				if (Failed) {
					return std::unexpected(core::release_maintenance_reason::recovery_required);
				}
				return { };
			}
		};

		/// \brief [[BPR-1602]]. One confirmed reset: its windows are closing, then
		/// the profile reopens with it.
		struct reset_entry
		{
			core::profile_data_reset_request Request;
			std::set<core::bpr_window_id>    Asked;
			/// Started when confirmed; a window still open after
			/// `reset_close_limit_ms` abandons the reset, removing nothing.
			QElapsedTimer Closing;
		};
		static constexpr qint64 reset_close_limit_ms = 30000;

	public:
		implementation(core::bpr_profile_id aInitial, startup_factory aFactory, profile_desktop_startup_completion aCompletion, core::profile_launch_intent aIntent, empty_desktop_policy aPolicy, std::shared_ptr<QSystemTrayIcon> aStatusItem)
			: StatusItem{ std::move(aStatusItem) }
			, Initial{ aInitial }
			, Factory{ std::move(aFactory) }
			, Completion{ std::move(aCompletion) }
			, InitialIntent{ aIntent }
			, Policy{ aPolicy ? std::move(aPolicy) : native_empty_desktop_policy() }
			, LastUsedProfile{ aInitial }
		{
			Poll.setInterval(10);
			QObject::connect(&Poll, &QTimer::timeout, this, [this] { poll(); });
		}
		~implementation() override
		{
			Poll.stop();
			Effects.close_and_drain();
		}
		void begin(bool aNativeLifetime)
		{
			Lifetime = shared_from_this();
			// RFC-BPR-0033 B2. Activating a resident desktop with no window reopens the
			// most recently used profile. The signal, rather than an application-wide
			// event filter, because every event in the process would pass through one.
			if (auto* const application = qobject_cast<QGuiApplication*>(QCoreApplication::instance()); application != nullptr) {
				QObject::connect(application, &QGuiApplication::applicationStateChanged, this,
								 [this](const Qt::ApplicationState aState) { application_state_changed(aState); });
			}
			Poll.start();
#if defined(__APPLE__)
			if (aNativeLifetime && begin_update_lifetime()) {
				return;
			}
#else
			static_cast<void>(aNativeLifetime);
#endif
			start_runtime(Initial);
		}
		auto runtime(core::bpr_profile_id aProfile) const noexcept -> github_application*
		{
			const auto found = Runtimes.find(aProfile);
			if (found == Runtimes.end() || !found->second.WindowPublished) {
				return nullptr;
			}
			return found->second.Runtime.get();
		}
		auto windows(core::bpr_profile_id aProfile) const -> std::vector<core::bpr_window_context>
		{
			std::vector<core::bpr_window_context> result;
			const auto                            found = Runtimes.find(aProfile);
			if (found != Runtimes.end()) {
				for (const auto context : found->second.Order) {
					const auto selected = found->second.Windows.find(context.Window);
					if (selected != found->second.Windows.end() && selected->second.Context == context && selected->second.Opened && !selected->second.Closing) {
						result.push_back(context);
					}
				}
			}
			return result;
		}
		auto window(core::bpr_window_context aContext) const noexcept -> qt::workbench_window*
		{
			const auto found = Runtimes.find(aContext.Profile);
			if (found == Runtimes.end() || !found->second.Runtime) {
				return nullptr;
			}
			const auto selected = found->second.Windows.find(aContext.Window);
			if (selected == found->second.Windows.end() || selected->second.Context != aContext || selected->second.Closing) {
				return nullptr;
			}
			return found->second.Runtime->window(aContext);
		}
		auto current(core::bpr_window_context aContext) const noexcept -> bool
		{
			const auto found = Runtimes.find(aContext.Profile);
			if (Quitting || UpdatePreparing || !Host || !Host->current(aContext) || found == Runtimes.end() || found->second.Closing || found->second.MaintenanceClosing) {
				return false;
			}
			const auto selected = found->second.Windows.find(aContext.Window);
			return selected != found->second.Windows.end() && selected->second.Context == aContext && !selected->second.Closing;
		}
		auto management_window() const noexcept -> qt::profile_management_window* { return Management.get(); }
		auto desktop_menu() const noexcept -> qt::desktop_menu_bar* { return DesktopMenu.get(); }
		auto status_item() const noexcept -> QSystemTrayIcon* { return StatusItem.get(); }
		auto last_empty_desktop_outcome() const noexcept -> std::optional<empty_desktop_outcome> { return LastEmptyOutcome; }
		void quit(int aExitCode)
		{
			if (aExitCode != 0) {
				ExitCode = aExitCode;
			}
			if (Quitting || UpdatePreparing) {
				return;
			}
			if (!Host) {
				force_shutdown(aExitCode);
				return;
			}
			// [[BPR-1045]]. A failure quit reports a verdict, so leaving the windows
			// usable after a refusal would keep a process running that has already
			// said it failed. The host refuses a Quit made while it delivers, which
			// is where the startup completion runs, so the poll retries it.
			if (!Host->request_quit() && aExitCode != 0) {
				FailureQuit = true;
			}
		}
		void quit_for_update(std::function<void(bool)> aPrepared)
		{
			if (Quitting || UpdatePreparing || !Maintenance.empty() || !Host || !Projection.Ready || Projection.Busy || PendingLaunch
				|| std::ranges::any_of(Runtimes, [](const auto& aEntry) { return !aEntry.second.Runtime || !aEntry.second.RecoveryReady || aEntry.second.Closing || !aEntry.second.Runtime->desktop_update_ready(); })) {
				if (aPrepared) {
					aPrepared(false);
				}
				return;
			}
			UpdatePreparing = true;
			UpdatePrepared  = std::move(aPrepared);
			set_update_enabled(false);
			std::vector<std::pair<std::filesystem::path, std::vector<application::release_update_saved_draft>>> snapshots;
			try {
				for (const auto& [profile, entry] : Runtimes) {
					static_cast<void>(profile);
					snapshots.emplace_back(entry.Runtime->desktop_update_recovery_root(), entry.Runtime->desktop_update_drafts());
				}
				Effects.post([self = shared_from_this(), pendingSnapshots = std::move(snapshots)] {
					bool saved = true;
					try {
						for (const auto& [root, drafts] : pendingSnapshots) {
							if (!adapters::sqlite::make_release_update_recovery_store(root)->save(drafts)) {
								saved = false;
								break;
							}
						}
					} catch (...) {
						saved = false;
					}
					self->Decisions.post([self, saved] {
						if (!self->UpdatePreparing) {
							return;
						}
						if (!saved || self->Quitting || !self->Host || !self->Host->request_quit()) {
							self->complete_update_prepare(false);
						}
					});
				});
			} catch (...) {
				complete_update_prepare(false);
			}
		}
		void force_shutdown(int aExitCode)
		{
			drain(aExitCode);
			if (Host) {
				Host->shutdown();
			}
		}
		void drain(int aExitCode, bool aOrderly = false)
		{
			if (aExitCode != 0) {
				ExitCode = aExitCode;
			}
			if (Quitting) {
				if (!aOrderly) {
					for (auto& [profile, entry] : Runtimes) {
						static_cast<void>(profile);
						if (!entry.Runtime) {
							continue;
						}
						for (const auto& [id, selected] : entry.Windows) {
							static_cast<void>(id);
							entry.Runtime->close_window(selected.Context);
						}
					}
				}
				return;
			}
			Quitting = true;
			for (auto& [profile, maintenance] : Maintenance) {
				static_cast<void>(profile);
				maintenance.Controls->shutdown();
			}
			if (Updater) {
				Updater->shutdown();
			}
			for (auto& [profile, entry] : Runtimes) {
				static_cast<void>(profile);
				complete_activation(entry, application::profile_activation_outcome::refused);
			}
			if (Management) {
				Management->hide();
			}
			for (auto& [profile, entry] : Runtimes) {
				static_cast<void>(profile);
				entry.Closing = true;
				if (entry.Startup) {
					entry.Startup->cancel();
				}
				for (auto& [id, selected] : entry.Windows) {
					static_cast<void>(id);
					selected.Closing = true;
					if (entry.Runtime && selected.Opened) {
						entry.Runtime->close_window(selected.Context, aOrderly ? github_application::window_retirement::orderly_quit : github_application::window_retirement::forced);
					}
				}
				if (entry.Runtime && entry.Windows.empty()) {
					entry.Runtime->begin_shutdown();
				}
			}
			finish_initial(std::unexpected(github_application_startup_failure::cancelled));
		}
		auto base_quiescent() const noexcept -> bool { return Quitting && Runtimes.empty() && Maintenance.empty() && (!Host || Host->is_quiescent()) && Effects.idle() && (!Updater || Updater->quiescent()); }
		auto is_quiescent() const noexcept -> bool { return base_quiescent() && !NativeBusy && (UpdateShutdownTransaction.empty() || UpdateAcknowledged); }
		auto exit_code() const noexcept -> int { return ExitCode; }
		void publish(const application::profile_management_projection& aProjection) override
		{
			Projection = aProjection;
			if (aProjection.Failure && aProjection.Failure->Profile && aProjection.Failure->Operation == application::profile_management_operation::close
				&& aProjection.Failure->Reason != application::profile_management_failure::busy) {
				const auto pending = Maintenance.find(*aProjection.Failure->Profile);
				if (pending != Maintenance.end()) {
					pending->second.Failed = true;
				}
			}
			if (UpdatePreparing && aProjection.Failure && aProjection.Failure->Operation == application::profile_management_operation::quit) {
				complete_update_prepare(false);
			}
			if (aProjection.Failure && aProjection.Failure->Profile) {
				const auto failed = Runtimes.find(*aProjection.Failure->Profile);
				if (failed != Runtimes.end() && (failed->second.ActivationSubmitting || aProjection.Failure->Reason != application::profile_management_failure::busy)
					&& aProjection.Failure->Operation == (failed->second.ActivationIntent == core::profile_launch_intent::new_window ? application::profile_management_operation::new_window : application::profile_management_operation::open)) {
					complete_activation(failed->second, application::profile_activation_outcome::refused);
				}
			}
			if (PendingLaunch && aProjection.Failure && aProjection.Failure->Operation == application::profile_management_operation::reload) {
				const auto launch = std::exchange(PendingLaunch, std::nullopt);
				const auto found  = Runtimes.find(launch->first);
				if (found != Runtimes.end() && found->second.Runtime) {
					found->second.Closing = true;
					found->second.Runtime->begin_shutdown();
				}
			}
			if (aProjection.Failure && aProjection.Failure->Profile && aProjection.Failure->Reason != application::profile_management_failure::busy) {
				const auto found = Runtimes.find(*aProjection.Failure->Profile);
				if (found != Runtimes.end() && found->second.Runtime && found->second.Windows.empty()) {
					found->second.Closing = true;
					found->second.Runtime->begin_shutdown();
				}
			}
			if (Management) {
				// [[BPR-0778]]. The projection carries a runtime state and no count, so
				// the composition that owns the windows supplies what a row says.
				Management->set_window_counts(window_counts());
				Management->adopt(aProjection);
			}
			if (DesktopMenu) {
				DesktopMenu->adopt(aProjection, most_recent_profile());
			}
			for (auto& [profile, entry] : Runtimes) {
				if (!entry.Runtime) {
					continue;
				}
				for (const auto& item : aProjection.Profiles) {
					if (item.Profile == profile) {
						entry.Runtime->set_profile_label(item.Label);
						break;
					}
				}
				for (const auto context : windows(profile)) {
					auto* presented = window(context);
					if (!presented) {
						continue;
					}
					presented->adopt_profile_management(aProjection, profile);
				}
			}
			// RFC-BPR-0033 B3. Every live window has just adopted this projection and
			// reports the refusal itself, so the chooser is presented for a failure only
			// when no window exists to carry it - the startup case RFC-BPR-0024
			// describes. While the chooser is already open, its own notice says it.
			if (aProjection.Failure && Management && !Quitting && !Management->isVisible() && !presented_window_exists()) {
				Management->set_current_profile(most_recent_profile());
				Management->present();
			}
			if (!InitialRegistered && aProjection.Ready && !aProjection.Closing && !Quitting) {
				InitialRegistered = true;
				if (InitialIntent == core::profile_launch_intent::new_window) {
					Host->apply(application::new_bpr_window_intent{ Initial });
				} else {
					Host->apply(application::open_bpr_profile_intent{ Initial });
				}
			} else if (!InitialRegistered && aProjection.Failure && Completion && !Quitting) {
				// [[BPR-0882]]. The profile startup's own mapping also reports
				// `registry`; this is the desktop's registry host, so it says so.
				note_initial_refusal("desktop-profile-registry");
				finish_initial(std::unexpected(github_application_startup_failure::registry));
				force_shutdown(1);
			} else if (PendingLaunch && !aProjection.Failure && aProjection.Ready && !aProjection.Busy && !Quitting) {
				const auto found = Runtimes.find(PendingLaunch->first);
				if (found != Runtimes.end() && found->second.Runtime && !found->second.Closing) {
					const auto launch = std::exchange(PendingLaunch, std::nullopt);
					if (launch->second == core::profile_launch_intent::new_window) {
						Host->apply(application::new_bpr_window_intent{ launch->first });
					} else {
						Host->apply(application::open_bpr_profile_intent{ launch->first });
					}
				}
			}
		}
		void perform(const application::profile_runtime_command& aCommand) override
		{
			if (aCommand.Kind == application::profile_runtime_command_kind::quit) {
				drain(ExitCode, true);
				complete_update_prepare(true);
				return;
			}
			if (aCommand.Kind == application::profile_runtime_command_kind::close) {
				const auto found = Runtimes.find(aCommand.Context.Profile);
				if (found == Runtimes.end()) {
					return;
				}
				const auto selected = found->second.Windows.find(aCommand.Context.Window);
				if (selected == found->second.Windows.end() || selected->second.Context != aCommand.Context || selected->second.Closing) {
					return;
				}
				selected->second.Closing = true;
				if (found->second.Runtime && selected->second.Opened) {
					if (selected->second.Presented) {
						selected->second.Presented->hide();
					}
					found->second.Runtime->close_window(aCommand.Context, github_application::window_retirement::explicit_close);
				}
				return;
			}

			if (Quitting || !Host || !Host->current(aCommand.Context)) {
				return;
			}
			const auto profile = aCommand.Context.Profile;
			auto       found   = Runtimes.find(profile);
			if (aCommand.Kind == application::profile_runtime_command_kind::activate) {
				if (current(aCommand.Context) && found != Runtimes.end() && found->second.Runtime) {
					found->second.Runtime->focus_window(aCommand.Context);
					LastUsedProfile = profile;
					complete_activation(found->second, application::profile_activation_outcome::queued);
				}
				return;
			}
			if (found == Runtimes.end()) {
				static_cast<void>(Host->complete_open(aCommand.Context, std::unexpected(application::profile_management_failure::runtime_unavailable)));
				return;
			}
			auto& entry = found->second;
			if (entry.Closing || entry.MaintenanceClosing || entry.Windows.contains(aCommand.Context.Window)) {
				static_cast<void>(Host->complete_open(aCommand.Context, std::unexpected(application::profile_management_failure::busy)));
				return;
			}
			entry.Windows.emplace(aCommand.Context.Window, window_entry{ aCommand.Context, false, false, aCommand.Restore, { }, aCommand.Review });
			entry.Order.push_back(aCommand.Context);
			if (entry.Runtime) {
				open_window(aCommand.Context);
			}
		}

	protected:
		auto eventFilter(QObject* aObject, QEvent* aEvent) -> bool override
		{
			if (aEvent->type() == QEvent::Close && aObject == Management.get() && Runtimes.empty() && !Quitting) {
				// Closing the chooser leaves the desktop empty again, so the same named
				// policy decides. [[BPR-0696]] quit here unconditionally, which is what
				// kept an invisible process from being left behind; on macOS staying
				// resident with the menu bar is now that platform's answer.
				if (empty_desktop_decision() == empty_desktop_outcome::quit) {
					static_cast<QCloseEvent*>(aEvent)->ignore();
					quit_empty_desktop();
					return true;
				}
				present_desktop_menu();
				return QObject::eventFilter(aObject, aEvent);
			}
			if (aEvent->type() != QEvent::Close && aEvent->type() != QEvent::WindowActivate) {
				return QObject::eventFilter(aObject, aEvent);
			}
			for (auto& [profile, entry] : Runtimes) {
				static_cast<void>(profile);
				if (!entry.Runtime) {
					continue;
				}
				for (auto& [id, selected] : entry.Windows) {
					static_cast<void>(id);
					if (selected.Closing || !selected.Opened || entry.Runtime->window(selected.Context) != aObject) {
						continue;
					}
					const auto context = selected.Context;
					if (aEvent->type() == QEvent::Close) {
						static_cast<QCloseEvent*>(aEvent)->ignore();
						if (Host && !Quitting && !UpdatePreparing) {
							static_cast<void>(Host->begin_close(context));
						}
						return true;
					} else if (!Focusing && current(context)) {
						// [[BPR-0796]]. A profile window is active, so the desktop's own
						// menu bar steps aside. Retired here rather than when the window
						// opens, because this is the moment the window's own menu bar
						// takes the macOS menu bar: retiring it earlier would leave the
						// bar with nothing installed until the window became key.
						retire_desktop_menu();
						Focusing        = true;
						LastUsedProfile = context.Profile;
						static_cast<void>(Host->focus(context));
						if (current(context)) {
							entry.Runtime->focus_window(context);
						}
						Focusing = false;
					}
					return QObject::eventFilter(aObject, aEvent);
				}
			}
			return QObject::eventFilter(aObject, aEvent);
		}

	private:
#if defined(__APPLE__)
		auto begin_update_lifetime() -> bool
		{
			const auto arguments = QCoreApplication::arguments();
			const auto flag      = QStringLiteral("--bpr-update-transaction");
			const auto index     = arguments.indexOf(flag);
			const auto installed = std::filesystem::path{ QCoreApplication::applicationDirPath().toStdString() }.parent_path().parent_path();
			// [[BPR-0983]]. A suite takes no update lifetime; the composition's
			// one reader says whether this is one.
			if (index < 0 && (installed.filename() != "BetterPullReview.app" || inside_automated_test_boundary())) {
				return false;
			}
			if ((index >= 0 && (arguments.count(flag) != 1 || index + 1 >= arguments.size())) || installed.filename() != "BetterPullReview.app") {
				note_initial_refusal("update-transaction-argument");
				finish_initial(std::unexpected(github_application_startup_failure::unavailable));
				force_shutdown(1);
				return true;
			}
			UpdateStartupTransaction = index >= 0 ? arguments[index + 1].toStdString() : std::string{ };
			NativeBusy               = true;
			Effects.post([self = shared_from_this(), installed, transaction = UpdateStartupTransaction, version = QCoreApplication::applicationVersion().toStdString()] {
				auto                                                           joined = adapters::update::release_update_installation::join(installed, transaction, version);
				std::shared_ptr<adapters::update::release_update_installation> lease;
				const auto                                                     failure = joined ? core::release_update_reason::none : joined.error();
				if (joined) {
					lease = std::move(*joined);
					lease->keep_until_process_exit();
				}
				self->Decisions.post([self, failure, accepted = std::move(lease)] {
					self->NativeBusy     = false;
					self->UpdateLifetime = accepted;
					if (accepted && accepted->startup_root()) {
						self->RegistryRoot = *accepted->startup_root();
					}
					if (self->Quitting) {
						return;
					}
					// Read-only and managed installations retain their manual startup.
					// They never receive an installer capability in native composition.
					if ((!accepted && (failure != core::release_update_reason::read_only || !self->UpdateStartupTransaction.empty()))
						|| (!self->UpdateStartupTransaction.empty() && (!accepted || !accepted->startup_root()))) {
						self->note_initial_refusal("update-lifetime-join");
						self->finish_initial(std::unexpected(github_application_startup_failure::unavailable));
						self->force_shutdown(1);
					} else {
						self->start_runtime(self->Initial);
					}
				});
			});
			return true;
		}
		void poll_update_lifetime()
		{
			if (!UpdateLifetime || NativeBusy) {
				return;
			}
			if (PendingHealthyStart) {
				if (HealthyStartWait.elapsed() >= 30000) {
					UpdateReceiptAttempted = true;
					PendingHealthyStart.reset();
					note_initial_refusal("update-healthy-start-timeout");
					finish_initial(std::unexpected(github_application_startup_failure::unavailable));
					force_shutdown(1);
					return;
				}
				if (!Updater || !Updater->durable() || std::ranges::any_of(Runtimes, [](const auto& aEntry) { return !aEntry.second.Runtime || !aEntry.second.RecoveryReady || !aEntry.second.Runtime->desktop_update_ready(); })) {
					return;
				}
				const auto ready       = *std::exchange(PendingHealthyStart, std::nullopt);
				UpdateReceiptAttempted = true;
				NativeBusy             = true;
				Effects.post([self = shared_from_this(), lease = UpdateLifetime, version = QCoreApplication::applicationVersion().toStdString(), ready] {
					const bool recorded = lease->healthy_start(version);
					self->Decisions.post([self, recorded, ready] {
						self->NativeBusy = false;
						if (!recorded) {
							self->note_initial_refusal("update-healthy-start-receipt");
						}
						self->finish_initial(recorded ? ready : std::unexpected(github_application_startup_failure::unavailable));
						if (!recorded) {
							self->force_shutdown(1);
						}
					});
				});
				return;
			}
			if (!UpdateShutdownTransaction.empty() && !UpdateAcknowledged && base_quiescent()) {
				NativeBusy         = true;
				const bool durable = ExitCode == 0 && (!Updater || Updater->durable());
				Effects.post([self = shared_from_this(), lease = UpdateLifetime, transaction = UpdateShutdownTransaction, durable] {
					const bool acknowledged = lease->acknowledge_shutdown(transaction, durable);
					self->Decisions.post([self, acknowledged, durable] {
						self->NativeBusy         = false;
						self->UpdateAcknowledged = true;
						if (!acknowledged || !durable) {
							self->ExitCode = 1;
						}
					});
				});
				return;
			}
			if (Quitting || UpdatePreparing || ++UpdateLifetimePoll < 20U) {
				return;
			}
			UpdateLifetimePoll = 0U;
			NativeBusy         = true;
			Effects.post([self = shared_from_this(), lease = UpdateLifetime] {
				const auto request = lease->shutdown_requested();
				const auto outcome = lease->last_result();
				self->Decisions.post([self, request, outcome] {
					self->NativeBusy = false;
					if (outcome && outcome->Transaction != self->LastUpdateOutcome && self->Updater && self->Updater->observe_installation_result(outcome->Installed)) {
						self->LastUpdateOutcome = outcome->Transaction;
					}
					if (!request || self->LastUpdateRequest == *request || self->Quitting) {
						return;
					}
					self->LastUpdateRequest = *request;
					self->quit_for_update([self, transaction = *request](bool aAccepted) {
						if (aAccepted) {
							self->UpdateShutdownTransaction = transaction;
						} else {
							self->Effects.post([refusedLease = self->UpdateLifetime, transaction] { static_cast<void>(refusedLease->acknowledge_shutdown(transaction, false)); });
						}
					});
				});
			});
		}
#endif
		void set_update_enabled(bool aEnabled)
		{
			if (Management) {
				Management->setEnabled(aEnabled);
			}
			if (DesktopMenu) {
				DesktopMenu->setEnabled(aEnabled);
			}
			for (auto& [profile, entry] : Runtimes) {
				static_cast<void>(profile);
				if (entry.Runtime) {
					entry.Runtime->desktop_update_freeze(!aEnabled);
				}
				for (auto& [id, selected] : entry.Windows) {
					static_cast<void>(id);
					if (selected.Presented) {
						selected.Presented->setEnabled(aEnabled);
					}
				}
			}
		}
		void complete_update_prepare(bool aAccepted)
		{
			if (!UpdatePreparing) {
				return;
			}
			UpdatePreparing = false;
			if (!aAccepted) {
				set_update_enabled(true);
			}
			if (auto callback = std::exchange(UpdatePrepared, { })) {
				callback(aAccepted);
			}
		}
		/// \brief How many windows each profile has on screen, for the chooser's rows.
		[[nodiscard]] auto window_counts() const -> std::map<core::bpr_profile_id, int>
		{
			auto counts = std::map<core::bpr_profile_id, int>{ };
			for (const auto& [profile, entry] : Runtimes) {
				static_cast<void>(entry);
				counts.emplace(profile, static_cast<int>(windows(profile).size()));
			}
			return counts;
		}
		/// \brief Whether any profile has a window on screen to carry a refusal.
		[[nodiscard]] auto presented_window_exists() const -> bool
		{
			return std::ranges::any_of(Runtimes, [this](const auto& aEntry) { return !windows(aEntry.first).empty(); });
		}
		/// \brief The profile a reopened window belongs to.
		///
		/// \details The last profile whose window was opened or focused in this
		/// process. Nothing new is persisted for it: an empty desktop only exists
		/// after a window has been open, and a fresh launch opens its initial profile.
		[[nodiscard]] auto most_recent_profile() const -> core::bpr_profile_id
		{
			const auto known = std::ranges::any_of(Projection.Profiles, [this](const auto& aProfile) { return aProfile.Profile == LastUsedProfile; });
			if (known) {
				return LastUsedProfile;
			}
			return Projection.Profiles.empty() ? Initial : Projection.Profiles.front().Profile;
		}
		/// \brief Records and returns the composition's answer for an empty desktop.
		[[nodiscard]] auto empty_desktop_decision() -> empty_desktop_outcome
		{
			const auto outcome = Policy ? Policy() : empty_desktop_outcome::quit;
			LastEmptyOutcome   = outcome;
			return outcome;
		}
		/// \brief RFC-BPR-0033 B1. A close is a close: no window and no chooser is
		/// presented because a window closed, and the policy decides what remains.
		void desktop_emptied()
		{
			if (empty_desktop_decision() == empty_desktop_outcome::quit) {
				EmptyDesktopQuit = true;
				quit_empty_desktop();
				return;
			}
			present_desktop_menu();
		}
		/// \brief Ends the process on a platform where no window means gone.
		///
		/// \details The durable Quit path is preferred, so profile records are saved
		/// exactly as an explicit Quit saves them. It refuses while the registry is
		/// still saving the close that emptied the desktop, so the poll retries it.
		/// Only a persistently refused request forces the shutdown, because a process
		/// with no window and no way to reach one is the defect [[BPR-0696]] fixed by
		/// presenting the chooser. Nothing is on screen and no live window set needs
		/// preserving by then.
		void quit_empty_desktop()
		{
			if (quit_within_bound(EmptyDesktopQuitAttempts)) {
				EmptyDesktopQuit = false;
			}
		}
		/// \brief Requests the durable Quit, and forces the shutdown once it has been
		/// refused for the whole bound.
		///
		/// \details Used where nothing is left for a person to keep: the empty desktop,
		/// and a failure quit whose verdict is already reported ([[BPR-1045]]). An
		/// admitted request saves the profile records as an explicit Quit does.
		/// \returns True once no retry is owed.
		[[nodiscard]] auto quit_within_bound(unsigned& aAttempts) -> bool
		{
			if (Quitting || (Host && Host->request_quit())) {
				return true;
			}
			if (!Host || ++aAttempts >= refused_quit_attempts) {
				force_shutdown(ExitCode);
				return true;
			}
			return false;
		}
		/// \brief Keeps Profiles, New Window and Quit reachable while no window exists.
		void present_desktop_menu()
		{
			if (!DesktopMenu) {
				DesktopMenu = std::make_unique<qt::desktop_menu_bar>();
				if (Updater) {
					Updater->attach(*DesktopMenu);
				}
				// [[BPR-1720]]. After the updater, so About ends Help.
				if (About) {
					About->attach(*DesktopMenu);
				}
				DesktopMenu->set_dispatch(
					[weak = weak_from_this()](application::profile_management_intent aIntent) {if (const auto self=weak.lock();self && !self->Quitting){ self->dispatch(std::move(aIntent));
} },
					[weak = weak_from_this()] {if (const auto self=weak.lock();self && self->Management && !self->Quitting){ if (self->Host && !self->Projection.Busy && !self->Projection.Closing) { self->Host->reload(); } self->Management->set_current_profile(self->most_recent_profile()); self->Management->present();
} },
					[weak = weak_from_this()] {if (const auto self=weak.lock()){ self->quit(0);
} });
			}
			DesktopMenu->adopt(Projection, most_recent_profile());
		}
		/// \brief [[BPR-0796]]. Gives the menu bar back once a window has one of its own.
		///
		/// \details Qt hands a parentless menu bar to any focused window that carries
		/// none, so a desktop menu bar left alive after a window reopened would take
		/// the macOS menu bar the moment the reviewer focused the workbench's Settings
		/// window - offering that window's own commands disabled while the window is
		/// right behind it. The desktop's bar belongs to a desktop with no window;
		/// `present_desktop_menu()` builds it again when the desktop empties. Deleted
		/// through the event loop, because the gesture that opened the window may be an
		/// action this bar owns.
		void retire_desktop_menu()
		{
			if (auto* const retired = DesktopMenu.release()) {
				retired->deleteLater();
			}
		}
		/// \brief RFC-BPR-0033 B2. Opens the most recently used profile when a resident
		/// desktop with no window is activated, through the existing open intent.
		void application_state_changed(const Qt::ApplicationState aState)
		{
			if (aState != Qt::ApplicationActive || Quitting || !Runtimes.empty() || !Maintenance.empty() || !Host || PendingLaunch) {
				return;
			}
			if (LastEmptyOutcome != empty_desktop_outcome::stay_resident || !Projection.Ready || Projection.Busy || Projection.Closing) {
				return;
			}
			dispatch(application::open_bpr_profile_intent{ most_recent_profile() });
		}
		void complete_activation(runtime_entry& aEntry, application::profile_activation_outcome aOutcome)
		{
			if (auto reply = std::exchange(aEntry.ActivationReply, { })) {
				Effects.post([pending = std::move(reply), aOutcome] { pending->complete(aOutcome); });
			}
		}
		void receive_activation(core::bpr_profile_id aProfile, std::uint64_t aSerial, std::optional<application::profile_activation_request> aRequest)
		{
			const auto found = Runtimes.find(aProfile);
			if (found == Runtimes.end() || found->second.StartupSerial != aSerial) {
				if (aRequest) {
					Effects.post([reply = std::move(aRequest->Reply)] { reply->complete(application::profile_activation_outcome::refused); });
				}
				return;
			}
			auto& entry             = found->second;
			entry.ActivationPolling = false;
			if (!aRequest) {
				return;
			}
			entry.ActivationReply  = std::move(aRequest->Reply);
			entry.ActivationIntent = aRequest->Intent;
			if (Quitting || UpdatePreparing || entry.Closing || entry.MaintenanceClosing || !entry.WindowPublished || !Host || !Projection.Ready || Projection.Busy || Projection.Closing || PendingLaunch) {
				complete_activation(entry, application::profile_activation_outcome::refused);
				return;
			}
			entry.ActivationSubmitting = true;
			if (aRequest->Intent == core::profile_launch_intent::new_window) {
				Host->apply(application::new_bpr_window_intent{ aProfile });
			} else {
				Host->apply(application::open_bpr_profile_intent{ aProfile });
			}
			entry.ActivationSubmitting = false;
		}
		/// One bounded step of the separate tour command endpoint. The transport is read on the effect
		/// lane; the command itself is routed on the decision lane, where the draft's window lives.
		void poll_tour_owner(core::bpr_profile_id aProfile, runtime_entry& aEntry)
		{
			if (Quitting || aEntry.Closing || aEntry.MaintenanceClosing || !aEntry.Runtime || aEntry.TourOwnerPolling) {
				return;
			}
			auto endpoint = aEntry.Runtime->desktop_tour_owner_server();
			if (!endpoint) {
				return;
			}
			aEntry.TourOwnerPolling = true;
			Effects.post([self = shared_from_this(), server = std::move(endpoint), aProfile, serial = aEntry.StartupSerial] {
				std::optional<application::tour_owner_message> message;
				try {
					message = server->receive();
				} catch (...) { }
				self->Decisions.post([owner = self, aProfile, serial, received = std::move(message)]() mutable {
					const auto found = owner->Runtimes.find(aProfile);
					if (found == owner->Runtimes.end() || found->second.StartupSerial != serial) {
						if (received) {
							owner->Effects.post([reply = std::move(received->Reply)] { reply->complete({ }); });
						}
						return;
					}
					found->second.TourOwnerPolling = false;
					if (!received) {
						return;
					}
					found->second.Runtime->desktop_tour_owner_route(std::move(received->Payload), [owner, reply = std::move(received->Reply)](application::tour_channel_bytes aAnswer) {
						owner->Effects.post([reply, answer = std::move(aAnswer)] { reply->complete(answer); });
					});
				});
			});
		}
		void poll_activation(core::bpr_profile_id aProfile, runtime_entry& aEntry)
		{
			if (Quitting || aEntry.Closing || aEntry.MaintenanceClosing || !aEntry.Runtime || aEntry.ActivationPolling || aEntry.ActivationReply) {
				return;
			}
			auto endpoint = aEntry.Runtime->desktop_activation_server();
			if (!endpoint) {
				return;
			}
			aEntry.ActivationPolling = true;
			Effects.post([self = shared_from_this(), server = std::move(endpoint), aProfile, serial = aEntry.StartupSerial] {
				std::optional<application::profile_activation_request> request;
				try {
					request = server->receive();
				} catch (...) { }
				self->Decisions.post([self, aProfile, serial, received = std::move(request)]() mutable {
					self->receive_activation(aProfile, serial, std::move(received));
				});
			});
		}
		void dispatch(application::profile_management_intent aIntent)
		{
			if (!Host || Quitting || UpdatePreparing) {
				return;
			}
			auto profile = std::optional<core::bpr_profile_id>{ };
			auto launch  = core::profile_launch_intent::activate;
			if (const auto* open = std::get_if<application::open_bpr_profile_intent>(&aIntent)) {
				profile = open->Profile;
			} else if (const auto* create = std::get_if<application::new_bpr_window_intent>(&aIntent)) {
				profile = create->Profile;
				launch  = core::profile_launch_intent::new_window;
			}
			if (profile && Maintenance.contains(*profile)) {
				Host->report_runtime_admission_failure(*profile, application::profile_management_failure::busy);
				return;
			}
			if (profile && !Runtimes.contains(*profile) && Projection.Ready && !Projection.Busy && !PendingLaunch) {
				// Admission precedes the management host's durable generation reservation.
				PendingLaunch = std::pair{ *profile, launch };
				start_runtime(*profile);
				return;
			}
			if (PendingLaunch) {
				Host->report_runtime_admission_failure(PendingLaunch->first, application::profile_management_failure::busy);
				return;
			}
			if (profile) {
				const auto found = Runtimes.find(*profile);
				if (found != Runtimes.end() && found->second.ActivationReply) {
					return;
				}
				if (found != Runtimes.end() && (found->second.Closing || !found->second.Runtime)) {
					Host->report_runtime_admission_failure(*profile, application::profile_management_failure::busy);
					return;
				}
			}
			Host->apply(std::move(aIntent));
		}
		void finish_initial(profile_desktop_startup_result aResult)
		{
			if (!Completion) {
				return;
			}
#if defined(__APPLE__)
			if (aResult && UpdateLifetime && !UpdateStartupTransaction.empty() && !UpdateReceiptAttempted) {
				if (!PendingHealthyStart) {
					PendingHealthyStart = aResult;
					HealthyStartWait.start();
				}
				return;
			}
			PendingHealthyStart.reset();
#endif
			auto completion = std::exchange(Completion, { });
			completion(std::move(aResult));
		}
		// [[BPR-0882]]. Every arm below that refuses the initial profile as
		// `unavailable` names itself here first, with the exception it caught if
		// it caught one. Only while the initial completion is still owed, so the
		// name recorded is the one that completion reports.
		void note_initial_refusal(std::string_view aSource, const std::exception_ptr& aCause = nullptr) const noexcept
		{
			if (Completion) {
				detail::note_startup_failure(aSource, aCause);
			}
		}
		void wire(github_application& aRuntime, core::bpr_window_context aContext)
		{
			auto* presented = aRuntime.window(aContext);
			if (!presented) {
				return;
			}
			const auto owner = Runtimes.find(aContext.Profile);
			if (owner == Runtimes.end()) {
				return;
			}
			const auto selected = owner->second.Windows.find(aContext.Window);
			if (selected == owner->second.Windows.end() || selected->second.Context != aContext || selected->second.Closing) {
				return;
			}
			selected->second.Presented = presented;
			if (Updater) {
				Updater->attach(*presented);
			}
			if (About) {
				About->attach(*presented->menuBar());
			}
			const auto weak = weak_from_this();
			presented->installEventFilter(this);
			presented->set_profile_management_dispatch(
				[weak, aContext](application::profile_management_intent aIntent) {if (const auto self=weak.lock();self && self->current(aContext)){ self->dispatch(std::move(aIntent));
} },
				[weak, aContext] {if (const auto self=weak.lock();self && self->current(aContext) && self->Management){ if (!self->Projection.Busy && !self->Projection.Closing) { self->Host->reload(); } self->Management->set_current_profile(aContext.Profile); self->Management->present();
} },
				[weak, aContext] {if (const auto self=weak.lock();self && self->current(aContext)){ self->quit(0);
} });
			presented->adopt_profile_management(Projection, aContext.Profile);
			// [[BPR-1602]]. A confirmed Troubleshooting reset closes this
			// profile's windows here and reopens it with the reset.
			presented->set_profile_data_reset_dispatch([weak, aContext](core::profile_data_reset_request aRequest) {
				if (const auto self = weak.lock(); self && self->current(aContext)) {
					self->request_data_reset(aContext, aRequest);
				}
			});
			for (const auto& item : Projection.Profiles) {
				if (item.Profile == aContext.Profile) {
					aRuntime.set_profile_label(item.Label);
					break;
				}
			}
		}
		void initialize_management(github_application& aRuntime)
		{
			RegistryRoot = aRuntime.desktop_registry_root();
			Updater      = release_update_desktop::start_native(RegistryRoot);
			About        = make_about_menu();
			Scratch      = aRuntime.desktop_scratch();
			Registry     = adapters::sqlite::make_profile_registry_store(RegistryRoot);
			Management   = std::make_unique<qt::profile_management_window>();
			Management->installEventFilter(this);
			// [[BPR-0777]]. The appended recovery notice and Reload Profiles button are
			// gone: RFC-BPR-0033 puts one failure notice with Try again inside the
			// chooser, over the same reload intent this dispatch already carries.
			Host = std::make_unique<application::profile_management_host>(*Registry, Effects, Decisions, *this, core::application_session_id{ 1U });
			Management->set_dispatch([weak = weak_from_this()](application::profile_management_intent aIntent) {if (const auto self=weak.lock();self && !self->Quitting){ self->dispatch(std::move(aIntent));
} });
			Host->reload();
		}
		void open_window(core::bpr_window_context aContext)
		{
			auto found = Runtimes.find(aContext.Profile);
			if (found == Runtimes.end() || !found->second.Runtime) {
				return;
			}
			auto& entry    = found->second;
			auto  selected = entry.Windows.find(aContext.Window);
			if (selected == entry.Windows.end() || selected->second.Context != aContext || selected->second.Opened) {
				return;
			}
			// [[BPR-0882]]. Three refusals leave the window unopened, so each step
			// names itself before it runs, and a throw is kept rather than dropped.
			bool opened  = false;
			auto refusal = std::string_view{ "desktop-window-not-current" };
			auto cause   = std::exception_ptr{ };
			try {
				if (current(aContext)) {
					refusal = "desktop-window-layout";
					if (entry.Runtime->configure_desktop_layouts(Host->live_windows(aContext.Profile))) {
						refusal = "desktop-window-open";
						opened  = entry.Runtime->open_window(aContext);
					}
				}
			} catch (...) {
				cause = std::current_exception();
			}
			if (!opened) {
				selected->second.Closing = true;
				if (Host) {
					static_cast<void>(Host->complete_open(aContext, std::unexpected(application::profile_management_failure::runtime_unavailable)));
				}
				if (aContext.Profile == Initial && Completion) {
					note_initial_refusal(refusal, cause);
					finish_initial(std::unexpected(github_application_startup_failure::unavailable));
					force_shutdown(1);
				}
				return;
			}
			selected->second.Opened = true;
			try {
				wire(*entry.Runtime, aContext);
				if (auto* restored = entry.Runtime->window(aContext)) {
					restored->choose_home(true);
				}
				if (!Host->complete_open(aContext) || !current(aContext)) {
					selected->second.Closing = true;
					entry.Runtime->close_window(aContext);
					if (aContext.Profile == Initial && Completion) {
						finish_initial(std::unexpected(github_application_startup_failure::cancelled));
						force_shutdown(0);
					}
					return;
				}
				entry.WindowPublished = true;
				LastUsedProfile       = aContext.Profile;
				entry.Runtime->focus_window(aContext);
				if (auto report = entry.Runtime->take_profile_data_reset_report()) {
					if (auto* const reopened = entry.Runtime->window(aContext)) {
						const auto folder = report->CopyFolder ? QString::fromStdString(report->CopyFolder->filename().string()) : QString{ };
						reopened->present_profile_data_reset(report->Outcome, folder);
					}
				}
				// [[BPR-1342]]. A window asked for by Open Review in New Window
				// opens Home, as every new window does, and then that pull
				// request.
				if (const auto fresh = entry.Windows.find(aContext.Window); fresh != entry.Windows.end() && fresh->second.Review) {
					const auto review = *std::exchange(fresh->second.Review, std::nullopt);
					entry.Runtime->open_window_review(aContext, review);
				}
				if (entry.Recovery && !entry.RecoveryPresented) {
					entry.RecoveryPresented = true;
					entry.Recovery->show();
				}
				complete_activation(entry, application::profile_activation_outcome::queued);
				if (aContext.Profile == Initial && Completion) {
					if (current(aContext)) {
						finish_initial(entry.Runtime.get());
					} else {
						finish_initial(std::unexpected(github_application_startup_failure::cancelled));
						force_shutdown(0);
					}
				}
			} catch (...) {
				const auto wiringCause   = std::current_exception();
				selected->second.Closing = true;
				entry.Runtime->close_window(aContext);
				static_cast<void>(Host->complete_open(aContext, std::unexpected(application::profile_management_failure::runtime_unavailable)));
				static_cast<void>(Host->begin_close(aContext));
				if (aContext.Profile == Initial && Completion) {
					note_initial_refusal("desktop-window-wiring", wiringCause);
					finish_initial(std::unexpected(github_application_startup_failure::unavailable));
					force_shutdown(1);
				}
			}
		}
		void start_runtime(core::bpr_profile_id aProfile)
		{
			if (NextStartup == std::numeric_limits<std::uint64_t>::max()) {
				if (PendingLaunch && PendingLaunch->first == aProfile) {
					PendingLaunch.reset();
					Host->report_runtime_admission_failure(aProfile, application::profile_management_failure::identity_exhausted);
				}
				if (aProfile == Initial && Completion) {
					note_initial_refusal("desktop-startup-serial-exhausted");
					finish_initial(std::unexpected(github_application_startup_failure::unavailable));
					force_shutdown(1);
				}
				return;
			}
			auto [position, inserted] = Runtimes.try_emplace(aProfile);
			if (!inserted) {
				return;
			}
			auto&      entry    = position->second;
			const auto serial   = ++NextStartup;
			entry.StartupSerial = serial;
			try {
				const auto intent = PendingLaunch && PendingLaunch->first == aProfile ? PendingLaunch->second : InitialIntent;
				// A pending reset reaches the first runtime started after the
				// last one closed, whichever launch starts it, and only that one.
				auto reset = std::optional<core::profile_data_reset_request>{ };
				if (const auto queued = Resets.find(aProfile); queued != Resets.end()) {
					reset = queued->second.Request;
					Resets.erase(queued);
				}
				entry.Startup = Factory(aProfile, RegistryRoot, intent, std::move(reset), [weak = weak_from_this(), aProfile, serial](github_application_startup_result aResult) {if (const auto self=weak.lock()){ self->started(aProfile,serial,std::move(aResult));
} });
				if (!entry.Startup && !entry.Started) {
					if (aProfile == Initial) {
						note_initial_refusal("desktop-runtime-factory-empty");
					}
					Decisions.post([weak = weak_from_this(), aProfile, serial] {if (const auto self=weak.lock()){ self->started(aProfile,serial,std::unexpected(github_application_startup_failure::unavailable));
} });
				}
			} catch (...) {
				if (aProfile == Initial) {
					note_initial_refusal("desktop-runtime-factory", std::current_exception());
				}
				Decisions.post([weak = weak_from_this(), aProfile, serial] {if (const auto self=weak.lock()){ self->started(aProfile,serial,std::unexpected(github_application_startup_failure::unavailable));
} });
			}
		}
		void started(core::bpr_profile_id aProfile, std::uint64_t aSerial, github_application_startup_result aResult)
		{
			auto found = Runtimes.find(aProfile);
			if (found == Runtimes.end() || found->second.StartupSerial != aSerial || found->second.Started) {
				return;
			}
			auto& entry   = found->second;
			entry.Started = true;
			if (!aResult) {
				entry.Closing = true;
				if (PendingLaunch && PendingLaunch->first == aProfile) {
					PendingLaunch.reset();
					if (aResult.error() != github_application_startup_failure::activation_queued) {
						Host->report_runtime_admission_failure(aProfile, aResult.error() == github_application_startup_failure::owner_busy ? application::profile_management_failure::owner_busy : application::profile_management_failure::runtime_unavailable);
					}
				}
				for (auto& [id, selected] : entry.Windows) {
					static_cast<void>(id);
					selected.Closing = true;
					if (Host) {
						static_cast<void>(Host->complete_open(selected.Context, std::unexpected(application::profile_management_failure::runtime_unavailable)));
					}
				}
				if (aProfile == Initial && Completion) {
					finish_initial(std::unexpected(aResult.error()));
					force_shutdown(aResult.error() == github_application_startup_failure::activation_queued ? 0 : 1);
				}
				return;
			}
			entry.Runtime   = std::move(*aResult);
			const auto root = entry.Runtime->desktop_update_recovery_root();
			Effects.post([self = shared_from_this(), aProfile, aSerial, root] {
				application::release_update_saved_drafts drafts = std::unexpected(application::release_update_recovery_failure::unavailable);
				auto                                     cause  = std::exception_ptr{ };
				try {
					drafts = adapters::sqlite::make_release_update_recovery_store(root)->load();
				} catch (...) {
					cause = std::current_exception();
				}
				self->Decisions.post([self, aProfile, aSerial, loaded = std::move(drafts), cause]() mutable { self->runtime_ready(aProfile, aSerial, std::move(loaded), cause); });
			});
		}
		void runtime_ready(core::bpr_profile_id aProfile, std::uint64_t aSerial, application::release_update_saved_drafts aDrafts, const std::exception_ptr& aDraftsCause)
		{
			const auto found = Runtimes.find(aProfile);
			if (found == Runtimes.end() || found->second.StartupSerial != aSerial || !found->second.Runtime) {
				return;
			}
			auto& entry         = found->second;
			entry.RecoveryReady = aDrafts.has_value();
			if (!aDrafts) {
				entry.Closing = true;
				entry.Runtime->begin_shutdown();
				if (aProfile == Initial) {
					note_initial_refusal("desktop-update-recovery-drafts", aDraftsCause);
					finish_initial(std::unexpected(github_application_startup_failure::unavailable));
					force_shutdown(1);
				}
				return;
			}
			if (!aDrafts->empty()) {
				entry.Recovery = std::make_unique<qt::release_update_recovery_dialog>(std::move(*aDrafts));
			}
			const auto weak = weak_from_this();
			entry.Runtime->set_window_admission([weak](core::bpr_window_context aContext) {const auto self=weak.lock();return self && self->current(aContext); });
			// [[BPR-1342]]. Open Review in New Window is the profile's New Window
			// carrying one admitted destination: the same intent, the same
			// management host and this one runtime owner.
			entry.Runtime->set_review_window_request([weak, aProfile](const core::window_review_destination& aDestination) {
				const auto self = weak.lock();
				if (!self || self->Quitting || !self->Host) {
					return false;
				}
				self->dispatch(application::new_bpr_window_intent{ aProfile, aDestination });
				return true;
			});
			entry.Runtime->set_desktop_quit_handler([weak](int aExitCode) {if (const auto self=weak.lock()){ self->quit(aExitCode);
} });
			entry.Runtime->set_maintenance_retirement([weak, aProfile](auto aCompletion) {
				if (const auto self = weak.lock()) {
					self->retire_for_maintenance(aProfile, std::move(aCompletion));
				} else {
					aCompletion(std::unexpected(core::release_maintenance_reason::unavailable));
				}
			});
			if (Quitting || entry.Closing) {
				entry.Closing = true;
				if (entry.Windows.empty()) {
					entry.Runtime->begin_shutdown();
				}
				return;
			}
			try {
				if (!Host) {
					initialize_management(*entry.Runtime);
				} else if (PendingLaunch && PendingLaunch->first == aProfile) {
					// Refresh the global allocator after ownership; another profile may have advanced it.
					Host->reload();
				}
				const auto pending = entry.Order;
				for (const auto context : pending) {
					open_window(context);
				}
			} catch (...) {
				const auto cause = std::current_exception();
				entry.Closing    = true;
				for (auto& [id, selected] : entry.Windows) {
					static_cast<void>(id);
					selected.Closing = true;
					if (selected.Opened) {
						entry.Runtime->close_window(selected.Context);
					}
					if (Host) {
						static_cast<void>(Host->complete_open(selected.Context, std::unexpected(application::profile_management_failure::runtime_unavailable)));
						static_cast<void>(Host->begin_close(selected.Context));
					}
				}
				if (entry.Windows.empty()) {
					entry.Runtime->begin_shutdown();
				}
				if (aProfile == Initial && Completion) {
					note_initial_refusal("desktop-profile-management", cause);
					finish_initial(std::unexpected(github_application_startup_failure::unavailable));
					force_shutdown(1);
				}
			}
		}
		/// \brief [[BPR-1602]]. Admits one confirmed reset and starts closing
		/// the profile's windows; `poll_resets` reopens it with the reset once
		/// its runtime, and with it the lease and every store, has gone.
		void request_data_reset(core::bpr_window_context aContext, core::profile_data_reset_request aRequest)
		{
			const auto refuse = [this, aContext](const QString& aWhy) {
				if (auto* const presented = window(aContext)) {
					presented->refuse_profile_data_reset(aWhy);
				}
			};
			if (core::admit_profile_data_reset(aRequest) != core::release_maintenance_reason::none) {
				refuse(QStringLiteral("That reset was not confirmed. Nothing was removed."));
				return;
			}
			const auto found = Runtimes.find(aContext.Profile);
			if (Quitting || UpdatePreparing || !Host || !Projection.Ready || Projection.Busy || PendingLaunch || found == Runtimes.end() || found->second.Closing
				|| found->second.MaintenanceClosing || Maintenance.contains(aContext.Profile) || Resets.contains(aContext.Profile)) {
				refuse(QStringLiteral("BetterPullReview is busy with this profile. Try again in a moment; nothing was removed."));
				return;
			}
			auto& entry = Resets.emplace(aContext.Profile, reset_entry{ aRequest, { }, { } }).first->second;
			entry.Closing.start();
		}
		/// \brief Closes each window of a profile with a pending reset once,
		/// then reopens the profile when its runtime is gone.
		void poll_resets()
		{
			for (auto position = Resets.begin(); position != Resets.end();) {
				const auto profile = position->first;
				auto&      reset   = position->second;
				if (Quitting) {
					position = Resets.erase(position);
					continue;
				}
				const auto runtime = Runtimes.find(profile);
				if (runtime != Runtimes.end() && reset.Closing.elapsed() > reset_close_limit_ms) {
					// A window that would not close keeps the profile running and
					// nothing is removed; the reset is not left waiting for a
					// later close to set it off.
					for (const auto& [id, selected] : runtime->second.Windows) {
						static_cast<void>(id);
						if (auto* const presented = !selected.Closing ? window(selected.Context) : nullptr) {
							presented->refuse_profile_data_reset(QStringLiteral("A window of this profile did not close, so nothing was reset. Close it and try again."));
						}
					}
					position = Resets.erase(position);
					continue;
				}
				if (runtime != Runtimes.end()) {
					if (Host && Projection.Ready && !Projection.Busy) {
						for (auto& [id, selected] : runtime->second.Windows) {
							if (selected.Closing || reset.Asked.contains(id)) {
								continue;
							}
							reset.Asked.insert(id);
							if (!Host->begin_close(selected.Context)) {
								reset.Asked.erase(id);
							}
							break;
						}
					}
					++position;
					continue;
				}
				if (!Host || !Projection.Ready || Projection.Busy || PendingLaunch) {
					++position;
					continue;
				}
				PendingLaunch = std::pair{ profile, core::profile_launch_intent::activate };
				start_runtime(profile);
				position = Resets.upper_bound(profile);
			}
		}
		void retire_for_maintenance(core::bpr_profile_id aProfile, std::function<void(application::release_maintenance_result)> aCompletion)
		{
			using reason = core::release_maintenance_reason;
			if (Quitting || UpdatePreparing || !Host || Projection.Busy) {
				aCompletion(std::unexpected(reason::busy));
				return;
			}
			const auto retained = Maintenance.find(aProfile);
			if (retained != Maintenance.end()) {
				if (retained->second.Completion) {
					aCompletion(std::unexpected(reason::busy));
					return;
				}
				if (retained->second.Retired) {
					aCompletion(retained->second.result());
					return;
				}
				retained->second.Failed     = false;
				retained->second.Completion = std::move(aCompletion);
				return;
			}
			const auto found = Runtimes.find(aProfile);
			if (found == Runtimes.end() || !found->second.Runtime || found->second.Closing) {
				aCompletion(std::unexpected(reason::unavailable));
				return;
			}
			auto controls = found->second.Runtime->take_maintenance();
			if (!controls) {
				aCompletion(std::unexpected(reason::unavailable));
				return;
			}
			Maintenance.emplace(aProfile, maintenance_entry{ std::move(controls), std::move(aCompletion) });
			found->second.MaintenanceClosing = true;
			complete_activation(found->second, application::profile_activation_outcome::refused);
		}
		void poll_maintenance()
		{
			for (auto position = Maintenance.begin(); position != Maintenance.end();) {
				auto&      maintenance = position->second;
				const auto runtime     = Runtimes.find(position->first);
				if (runtime == Runtimes.end()) {
					maintenance.Retired = true;
				}
				if (maintenance.Completion && (maintenance.Retired || maintenance.Failed)) {
					auto completion = std::exchange(maintenance.Completion, { });
					completion(maintenance.result());
				} else if (maintenance.Completion && !Quitting && !Projection.Busy && Host) {
					const auto selected = std::ranges::find_if(runtime->second.Windows, [](const auto& aWindow) { return !aWindow.second.Closing; });
					if (selected != runtime->second.Windows.end() && !Host->begin_close(selected->second.Context)) {
						maintenance.Failed = true;
					}
				}
				if (maintenance.Controls->closing() && maintenance.Controls->quiescent() && !maintenance.Completion && maintenance.Retired) {
					position = Maintenance.erase(position);
					if (Maintenance.empty() && Resets.empty() && Runtimes.empty() && !Quitting) {
						desktop_emptied();
					}
				} else {
					++position;
				}
			}
		}
		void poll()
		{
			const auto keepAlive = shared_from_this();
#if defined(__APPLE__)
			poll_update_lifetime();
#endif
			if (Updater && !Quitting && ++UpdateAccountPoll >= 100U) {
				UpdateAccountPoll = 0U;
				std::vector<release_update_account> accounts;
				for (auto& [id, entry] : Runtimes) {
					static_cast<void>(id);
					if (entry.Runtime && !entry.Closing) {
						if (auto account = entry.Runtime->desktop_release_account()) {
							accounts.push_back(std::move(*account));
						}
					}
				}
				Updater->set_accounts(std::move(accounts));
			}
			for (auto position = Runtimes.begin(); position != Runtimes.end();) {
				auto& entry = position->second;
				poll_activation(position->first, entry);
				poll_tour_owner(position->first, entry);
				if (entry.Runtime) {
					entry.Runtime->desktop_tour_owner_tick();
				}
				if (entry.Startup && entry.Startup->is_quiescent()) {
					entry.Startup.reset();
				}
				for (auto selected = entry.Windows.begin(); selected != entry.Windows.end();) {
					if (!selected->second.Closing || (entry.Runtime && selected->second.Opened && !entry.Runtime->window_quiescent(selected->second.Context))) {
						++selected;
						continue;
					}
					const auto context = selected->second.Context;
					selected           = entry.Windows.erase(selected);
					std::erase(entry.Order, context);
					if (entry.Windows.empty()) {
						entry.Closing = true;
						if (entry.Runtime) {
							entry.Runtime->begin_shutdown();
						}
					}
					if (Host) {
						static_cast<void>(Host->complete_close(context));
					}
				}
				if (!entry.Closing || entry.Startup || entry.ActivationPolling || entry.TourOwnerPolling || (entry.Runtime && !entry.Runtime->profile_shutdown_complete())) {
					++position;
					continue;
				}
				if (entry.Runtime && entry.Runtime->profile_shutdown_cleanup_failed()) {
					if (auto maintenance = Maintenance.find(position->first); maintenance != Maintenance.end()) {
						maintenance->second.Failed = true;
					}
					// [[BPR-0875]]. This is the exit 1 a person gets from a real
					// quit, so the cleanup that caused it is named on the way out
					// rather than left to a bisect.
					const auto detail = entry.Runtime->profile_shutdown_cleanup_detail();
					bpr::app::detail::terminal_text_stream{ } << "PROFILE_QUIT_CLEANUP_SOURCE:"
															  << QString::fromStdString(detail.empty() ? std::string{ "unnamed" } : detail) << '\n';
					ExitCode = 1;
				}
				position = Runtimes.erase(position);
				if (Runtimes.empty() && !Quitting) {
					if (Maintenance.empty() && Resets.empty()) {
						desktop_emptied();
					}
#if defined(__APPLE__)
					else {
						// Retained Settings has no menu of its own after the last
						// workbench retires. Keep the real Quit action reachable.
						present_desktop_menu();
					}
#endif
				}
			}
			poll_maintenance();
			poll_resets();
			if (EmptyDesktopQuit && !Quitting && Runtimes.empty() && Maintenance.empty() && Resets.empty()) {
				quit_empty_desktop();
			}
			if (FailureQuit && quit_within_bound(FailureQuitAttempts)) {
				FailureQuit = false;
			}
			if (is_quiescent()) {
				Poll.stop();
				Lifetime.reset();
			}
		}
		/// [[BPR-1250]]. Released after the runtimes declared below, which share it:
		/// Dif stays in the menu bar for as long as the desktop runs.
		std::shared_ptr<QSystemTrayIcon>                                            StatusItem;
		std::shared_ptr<void>                                                       Scratch;
		core::bpr_profile_id                                                        Initial;
		startup_factory                                                             Factory;
		profile_desktop_startup_completion                                          Completion;
		core::profile_launch_intent                                                 InitialIntent;
		empty_desktop_policy                                                        Policy;
		core::bpr_profile_id                                                        LastUsedProfile;
		std::optional<empty_desktop_outcome>                                        LastEmptyOutcome;
		std::filesystem::path                                                       RegistryRoot;
		worker_executor                                                             Effects;
		qt::event_loop_executor                                                     Decisions;
		std::unique_ptr<application::profile_registry_port>                         Registry;
		std::unique_ptr<application::profile_management_host>                       Host;
		std::unique_ptr<qt::profile_management_window>                              Management;
		std::unique_ptr<qt::desktop_menu_bar>                                       DesktopMenu;
		std::unique_ptr<release_update_desktop>                                     Updater;
		std::unique_ptr<qt::about_menu>                                             About;
		unsigned                                                                    UpdateAccountPoll = 0U;
		std::map<core::bpr_profile_id, runtime_entry>                               Runtimes;
		std::map<core::bpr_profile_id, maintenance_entry>                           Maintenance;
		std::map<core::bpr_profile_id, reset_entry>                                 Resets;
		application::profile_management_projection                                  Projection;
		std::optional<std::pair<core::bpr_profile_id, core::profile_launch_intent>> PendingLaunch;
		QTimer                                                                      Poll;
		std::shared_ptr<implementation>                                             Lifetime;
		/// Two seconds of 10 ms polls: long enough for the close that emptied the
		/// desktop to finish saving, or for the delivery a failure quit was made in
		/// to end, and short enough not to leave a process with nothing left to do.
		static constexpr unsigned refused_quit_attempts    = 200U;
		unsigned                  EmptyDesktopQuitAttempts = 0U;
		bool                      EmptyDesktopQuit         = false;
		unsigned                  FailureQuitAttempts      = 0U;
		bool                      FailureQuit              = false;
		std::uint64_t             NextStartup              = 0;
		bool                      InitialRegistered = false, Quitting = false, Focusing = false;
		bool                      UpdatePreparing = false;
		std::function<void(bool)> UpdatePrepared;
		bool                      NativeBusy = false, UpdateAcknowledged = false;
		std::string               UpdateShutdownTransaction;
#if defined(__APPLE__)
		std::shared_ptr<adapters::update::release_update_installation> UpdateLifetime;
		std::string                                                    UpdateStartupTransaction, LastUpdateRequest, LastUpdateOutcome;
		unsigned                                                       UpdateLifetimePoll     = 0U;
		bool                                                           UpdateReceiptAttempted = false;
		std::optional<profile_desktop_startup_result>                  PendingHealthyStart;
		QElapsedTimer                                                  HealthyStartWait;
#endif
		int ExitCode = 0;
	};

	auto native_empty_desktop_policy() -> empty_desktop_policy
	{
#if defined(__APPLE__)
		return [] { return empty_desktop_outcome::stay_resident; };
#else
		return [] { return empty_desktop_outcome::quit; };
#endif
	}

	auto native_menu_bar_status_item() -> std::shared_ptr<QSystemTrayIcon>
	{
#if defined(__APPLE__)
		return qt::make_menu_bar_status_item();
#else
		return { };
#endif
	}

	profile_desktop::profile_desktop(std::shared_ptr<implementation> aImplementation)
		: Implementation{ std::move(aImplementation) }
	{ }
	profile_desktop::~profile_desktop() { Implementation->force_shutdown(0); }
	auto profile_desktop::start_with_factory(core::bpr_profile_id aProfile, startup_factory aFactory, profile_desktop_startup_completion aCompletion, core::profile_launch_intent aIntent, empty_desktop_policy aPolicy, bool aNativeLifetime, std::shared_ptr<QSystemTrayIcon> aStatusItem) -> std::unique_ptr<profile_desktop>
	{
		auto state   = std::make_shared<implementation>(aProfile, std::move(aFactory), std::move(aCompletion), aIntent, std::move(aPolicy), std::move(aStatusItem));
		auto desktop = std::unique_ptr<profile_desktop>{ new profile_desktop{ state } };
		state->begin(aNativeLifetime);
		return desktop;
	}
	auto profile_desktop::configuration_for_profile(const github_application_config& aConfig, core::bpr_profile_id aProfile, const std::filesystem::path& aRoot) -> github_application_config
	{
		auto next = aConfig;
		if (aProfile != aConfig.Profile) {
			next.DiagnosticLog.reset();
			next.EnterpriseHostname.reset();
			next.ExistingCheckout.reset();
			next.AttendedDesignatedLiveApproval.reset();
			next.AttendedSourceSha256.clear();
			next.AttendedSupervisorSha256.clear();
			next.AttendedInventorySha256.clear();
		}
		if (!aRoot.empty()) {
			next.ApplicationDataRoot = aRoot;
		}
		next.Profile = aProfile;
		return next;
	}

	auto profile_desktop::start(github_application_config aConfig, profile_desktop_startup_completion aCompletion, empty_desktop_policy aPolicy) -> std::unique_ptr<profile_desktop>
	{
		if (!aConfig.MotionObserver) {
			aConfig.MotionObserver = std::make_shared<qt::motion_observer>(qt::make_native_motion_source());
		}
		if (!aConfig.DifCatalog) {
			aConfig.DifCatalog = std::make_shared<qt::dif_catalog_provider>();
		}
		if (!aConfig.StatusItem) {
			aConfig.StatusItem = native_menu_bar_status_item();
		}
		const auto profile    = aConfig.Profile;
		const auto intent     = aConfig.ProfileLaunchIntent;
		auto       statusItem = aConfig.StatusItem;
		// [[BPR-1602]]. A reset on the command line runs once, for the first
		// start of the profile it names; a later reopen carries its own.
		auto initialReset = std::make_shared<std::optional<core::profile_data_reset_request>>(std::exchange(aConfig.DataReset, std::nullopt));
		return start_with_factory(profile, [config = std::move(aConfig), initialReset](core::bpr_profile_id aProfile, const std::filesystem::path& aRoot, core::profile_launch_intent aIntent, std::optional<core::profile_data_reset_request> aReset, github_application_startup_completion aDone) {
            auto next = configuration_for_profile(config, aProfile, aRoot);
            next.ProfileLaunchIntent = aIntent;
            next.DataReset = aReset ? aReset : aProfile == config.Profile ? std::exchange(*initialReset, std::nullopt) : std::nullopt;
            return github_application::start_profile(std::move(next), std::move(aDone)); }, std::move(aCompletion), intent, std::move(aPolicy), true, std::move(statusItem));
	}
	auto profile_desktop::runtime(core::bpr_profile_id aProfile) const noexcept -> github_application* { return Implementation->runtime(aProfile); }
	auto profile_desktop::windows(core::bpr_profile_id aProfile) const -> std::vector<core::bpr_window_context> { return Implementation->windows(aProfile); }
	auto profile_desktop::window(core::bpr_window_context aContext) const noexcept -> qt::workbench_window* { return Implementation->window(aContext); }
	auto profile_desktop::management_window() const noexcept -> qt::profile_management_window* { return Implementation->management_window(); }
	auto profile_desktop::desktop_menu() const noexcept -> qt::desktop_menu_bar* { return Implementation->desktop_menu(); }
	auto profile_desktop::status_item() const noexcept -> QSystemTrayIcon* { return Implementation->status_item(); }
	auto profile_desktop::last_empty_desktop_outcome() const noexcept -> std::optional<empty_desktop_outcome> { return Implementation->last_empty_desktop_outcome(); }
	void profile_desktop::quit(int aExitCode) { Implementation->quit(aExitCode); }
	void profile_desktop::quit_for_update(std::function<void(bool)> aPrepared) { Implementation->quit_for_update(std::move(aPrepared)); }
	auto profile_desktop::is_quiescent() const noexcept -> bool { return Implementation->is_quiescent(); }
	auto profile_desktop::exit_code() const noexcept -> int { return Implementation->exit_code(); }
#if defined(BPR_BUILD_TESTING)
	void detail::github_application_fixture_access::poll_desktop(profile_desktop& aDesktop) { aDesktop.Implementation->poll(); }
#endif
}
