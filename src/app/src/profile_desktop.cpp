#include "bpr/app/profile_desktop.hpp"
#include "bpr/adapters/sqlite/profile_registry_store.hpp"
#include "bpr/app/worker_executor.hpp"
#include "bpr/application/profile_management_host.hpp"
#include "bpr/qt/event_loop_executor.hpp"
#include "bpr/qt/profile_management_window.hpp"
#include <QCloseEvent>
#include <QEvent>
#include <QLabel>
#include <QLayout>
#include <QObject>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <algorithm>
#include <limits>
#include <map>
#include <utility>

namespace bpr::app {

	class profile_desktop::implementation final : public QObject,
												  public application::profile_management_projection_port,
												  public std::enable_shared_from_this<implementation>
	{
		struct window_entry
		{
			core::bpr_window_context       Context;
			bool                           Opened  = false;
			bool                           Closing = false;
			bool                           Restore = false;
			QPointer<qt::workbench_window> Presented;
		};
		struct runtime_entry
		{
			std::unique_ptr<github_application_startup> Startup;
			std::unique_ptr<github_application>         Runtime;
			std::map<core::bpr_window_id, window_entry> Windows;
			std::vector<core::bpr_window_context>       Order;
			std::uint64_t                               StartupSerial = 0;
			bool                                        Started       = false;
			bool                                        Closing       = false;
		};

	public:
		implementation(core::bpr_profile_id aInitial, startup_factory aFactory, profile_desktop_startup_completion aCompletion)
			: Initial{ aInitial }
			, Factory{ std::move(aFactory) }
			, Completion{ std::move(aCompletion) }
		{
			Poll.setInterval(10);
			QObject::connect(&Poll, &QTimer::timeout, this, [this] { poll(); });
		}
		~implementation() override
		{
			Poll.stop();
			Effects.close_and_drain();
		}
		void begin()
		{
			Lifetime = shared_from_this();
			Poll.start();
			start_runtime(Initial, std::nullopt);
		}
		auto runtime(core::bpr_profile_id aProfile) const noexcept -> github_application*
		{
			const auto found = Runtimes.find(aProfile);
			return found == Runtimes.end() ? nullptr : found->second.Runtime.get();
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
			if (Quitting || !Host || !Host->current(aContext) || found == Runtimes.end() || found->second.Closing) {
				return false;
			}
			const auto selected = found->second.Windows.find(aContext.Window);
			return selected != found->second.Windows.end() && selected->second.Context == aContext && !selected->second.Closing;
		}
		auto management_window() const noexcept -> qt::profile_management_window* { return Management.get(); }
		void quit(int aExitCode)
		{
			if (aExitCode != 0) {
				ExitCode = aExitCode;
			}
			if (Quitting) {
				return;
			}
			if (!Host) {
				force_shutdown(aExitCode);
				return;
			}
			static_cast<void>(Host->request_quit());
		}
		void force_shutdown(int aExitCode)
		{
			drain(aExitCode);
			if (Host) {
				Host->shutdown();
			}
		}
		void drain(int aExitCode)
		{
			if (aExitCode != 0) {
				ExitCode = aExitCode;
			}
			if (Quitting) {
				return;
			}
			Quitting = true;
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
						entry.Runtime->close_window(selected.Context);
					}
				}
				if (entry.Runtime && entry.Windows.empty()) {
					entry.Runtime->begin_shutdown();
				}
			}
			finish_initial(std::unexpected(github_application_startup_failure::cancelled));
		}
		auto is_quiescent() const noexcept -> bool { return Quitting && Runtimes.empty() && (!Host || Host->is_quiescent()) && Effects.idle(); }
		auto exit_code() const noexcept -> int { return ExitCode; }
		void publish(const application::profile_management_projection& aProjection) override
		{
			Projection = aProjection;
			if (Management) {
				Management->adopt(aProjection);
				ReloadProfiles->setEnabled(!aProjection.Busy && !aProjection.Closing && !Quitting);
				RecoveryNotice->setVisible(aProjection.Failure.has_value());
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
			if (aProjection.Failure && Management && !Quitting && !Management->isVisible()) {
				Management->present();
			}
			if (!InitialRegistered && aProjection.Ready && !aProjection.Closing && !Quitting) {
				InitialRegistered = true;
				Host->apply(application::open_bpr_profile_intent{ Initial });
			} else if (!InitialRegistered && aProjection.Failure && Completion && !Quitting) {
				finish_initial(std::unexpected(github_application_startup_failure::registry));
				force_shutdown(1);
			}
		}
		void perform(const application::profile_runtime_command& aCommand) override
		{
			if (aCommand.Kind == application::profile_runtime_command_kind::quit) {
				drain(ExitCode);
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
					found->second.Runtime->close_window(aCommand.Context);
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
				}
				return;
			}
			if (found == Runtimes.end()) {
				start_runtime(profile, aCommand.Context, aCommand.Restore);
				return;
			}
			auto& entry = found->second;
			if (entry.Closing || entry.Windows.contains(aCommand.Context.Window)) {
				static_cast<void>(Host->complete_open(aCommand.Context, std::unexpected(application::profile_management_failure::busy)));
				return;
			}
			entry.Windows.emplace(aCommand.Context.Window, window_entry{ aCommand.Context, false, false, aCommand.Restore, { } });
			entry.Order.push_back(aCommand.Context);
			if (entry.Runtime) {
				open_window(aCommand.Context);
			}
		}

	protected:
		auto eventFilter(QObject* aObject, QEvent* aEvent) -> bool override
		{
			if (aEvent->type() == QEvent::Close && aObject == Management.get() && Runtimes.empty() && !Quitting) {
				static_cast<QCloseEvent*>(aEvent)->ignore();
				quit(0);
				return true;
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
						if (Host && !Quitting) {
							static_cast<void>(Host->begin_close(context));
						}
						return true;
					} else if (!Focusing && current(context)) {
						Focusing = true;
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
		void finish_initial(profile_desktop_startup_result aResult)
		{
			if (!Completion) {
				return;
			}
			auto completion = std::move(Completion);
			completion(std::move(aResult));
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
			const auto weak            = weak_from_this();
			presented->installEventFilter(this);
			presented->set_profile_management_dispatch(
				[weak, aContext](application::profile_management_intent aIntent) {if (const auto self=weak.lock();self && self->current(aContext)){ self->Host->apply(std::move(aIntent));
} },
				[weak, aContext] {if (const auto self=weak.lock();self && self->current(aContext) && self->Management){ if (!self->Projection.Busy && !self->Projection.Closing) { self->Host->reload(); } self->Management->present();
} },
				[weak, aContext] {if (const auto self=weak.lock();self && self->current(aContext)){ self->quit(0);
} });
			presented->adopt_profile_management(Projection, aContext.Profile);
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
			Scratch      = aRuntime.desktop_scratch();
			Registry     = adapters::sqlite::make_profile_registry_store(RegistryRoot);
			Management   = std::make_unique<qt::profile_management_window>();
			Management->installEventFilter(this);
			RecoveryNotice = new QLabel{ QStringLiteral("If a profile operation fails, reload profiles, then retry the operation. Your windows remain open until Close or Quit is saved."), Management.get() };
			RecoveryNotice->setObjectName(QStringLiteral("profileRecoveryNotice"));
			RecoveryNotice->setWordWrap(true);
			RecoveryNotice->hide();
			Management->layout()->addWidget(RecoveryNotice);
			ReloadProfiles = new QPushButton{ QStringLiteral("Reload Profiles"), Management.get() };
			ReloadProfiles->setObjectName(QStringLiteral("reloadProfiles"));
			ReloadProfiles->setAccessibleName(QStringLiteral("Reload profiles after a failed operation"));
			Management->layout()->addWidget(ReloadProfiles);
			QObject::connect(ReloadProfiles, &QPushButton::clicked, this, [weak = weak_from_this()] {
				if (const auto self = weak.lock(); self && !self->Quitting && self->Host && !self->Projection.Busy && !self->Projection.Closing) {
					self->Host->reload();
				}
			});
			Host = std::make_unique<application::profile_management_host>(*Registry, Effects, Decisions, *this, core::application_session_id{ 1U });
			Management->set_dispatch([weak = weak_from_this()](application::profile_management_intent aIntent) {if (const auto self=weak.lock();self && !self->Quitting){ self->Host->apply(std::move(aIntent));
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
			bool opened = false;
			try {
				if (current(aContext)) {
					opened = entry.Runtime->open_window(aContext);
				}
			} catch (...) { }
			if (!opened) {
				selected->second.Closing = true;
				if (Host) {
					static_cast<void>(Host->complete_open(aContext, std::unexpected(application::profile_management_failure::runtime_unavailable)));
				}
				if (aContext.Profile == Initial && Completion) {
					finish_initial(std::unexpected(github_application_startup_failure::unavailable));
					force_shutdown(1);
				}
				return;
			}
			selected->second.Opened = true;
			try {
				wire(*entry.Runtime, aContext);
				if (selected->second.Restore) {
					if (auto* restored = entry.Runtime->window(aContext)) {
						restored->choose_home(true);
						if (auto* notice = restored->home_open_notice()) {
							notice->setText(QStringLiteral("Window restored. Its previous review content is unavailable; choose a review to continue."));
							notice->show();
						}
					}
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
				entry.Runtime->focus_window(aContext);
				if (aContext.Profile == Initial && Completion) {
					if (current(aContext)) {
						finish_initial(entry.Runtime.get());
					} else {
						finish_initial(std::unexpected(github_application_startup_failure::cancelled));
						force_shutdown(0);
					}
				}
			} catch (...) {
				selected->second.Closing = true;
				entry.Runtime->close_window(aContext);
				static_cast<void>(Host->complete_open(aContext, std::unexpected(application::profile_management_failure::runtime_unavailable)));
				static_cast<void>(Host->begin_close(aContext));
				if (aContext.Profile == Initial && Completion) {
					finish_initial(std::unexpected(github_application_startup_failure::unavailable));
					force_shutdown(1);
				}
			}
		}
		void start_runtime(core::bpr_profile_id aProfile, std::optional<core::bpr_window_context> aContext, bool aRestore = false)
		{
			if (NextStartup == std::numeric_limits<std::uint64_t>::max()) {
				if (aContext && Host) {
					static_cast<void>(Host->complete_open(*aContext, std::unexpected(application::profile_management_failure::identity_exhausted)));
				}
				if (aProfile == Initial && Completion) {
					finish_initial(std::unexpected(github_application_startup_failure::unavailable));
					force_shutdown(1);
				}
				return;
			}
			auto [position, inserted] = Runtimes.try_emplace(aProfile);
			if (!inserted) {
				return;
			}
			auto& entry = position->second;
			if (aContext) {
				entry.Windows.emplace(aContext->Window, window_entry{ *aContext, false, false, aRestore, { } });
				entry.Order.push_back(*aContext);
			}
			const auto serial   = ++NextStartup;
			entry.StartupSerial = serial;
			try {
				entry.Startup = Factory(aProfile, RegistryRoot, [weak = weak_from_this(), aProfile, serial](github_application_startup_result aResult) {if (const auto self=weak.lock()){ self->started(aProfile,serial,std::move(aResult));
} });
				if (!entry.Startup && !entry.Started) {
					Decisions.post([weak = weak_from_this(), aProfile, serial] {if (const auto self=weak.lock()){ self->started(aProfile,serial,std::unexpected(github_application_startup_failure::unavailable));
} });
				}
			} catch (...) {
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
				for (auto& [id, selected] : entry.Windows) {
					static_cast<void>(id);
					selected.Closing = true;
					if (Host) {
						static_cast<void>(Host->complete_open(selected.Context, std::unexpected(application::profile_management_failure::runtime_unavailable)));
					}
				}
				if (aProfile == Initial && Completion) {
					finish_initial(std::unexpected(aResult.error()));
					force_shutdown(1);
				}
				return;
			}
			entry.Runtime   = std::move(*aResult);
			const auto weak = weak_from_this();
			entry.Runtime->set_window_admission([weak](core::bpr_window_context aContext) {const auto self=weak.lock();return self && self->current(aContext); });
			entry.Runtime->set_desktop_quit_handler([weak](int aExitCode) {if (const auto self=weak.lock()){ self->quit(aExitCode);
} });
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
				}
				const auto pending = entry.Order;
				for (const auto context : pending) {
					open_window(context);
				}
			} catch (...) {
				entry.Closing = true;
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
					finish_initial(std::unexpected(github_application_startup_failure::unavailable));
					force_shutdown(1);
				}
			}
		}
		void poll()
		{
			const auto keepAlive = shared_from_this();
			for (auto position = Runtimes.begin(); position != Runtimes.end();) {
				auto& entry = position->second;
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
				if (!entry.Closing || entry.Startup || (entry.Runtime && !entry.Runtime->profile_shutdown_complete())) {
					++position;
					continue;
				}
				if (entry.Runtime && entry.Runtime->profile_shutdown_cleanup_failed()) {
					ExitCode = 1;
				}
				position = Runtimes.erase(position);
				if (Runtimes.empty() && !Quitting && Management) {
					Management->present();
				}
			}
			if (is_quiescent()) {
				Poll.stop();
				Lifetime.reset();
			}
		}
		std::shared_ptr<void>                                 Scratch;
		core::bpr_profile_id                                  Initial;
		startup_factory                                       Factory;
		profile_desktop_startup_completion                    Completion;
		std::filesystem::path                                 RegistryRoot;
		worker_executor                                       Effects;
		qt::event_loop_executor                               Decisions;
		std::unique_ptr<application::profile_registry_port>   Registry;
		std::unique_ptr<application::profile_management_host> Host;
		std::unique_ptr<qt::profile_management_window>        Management;
		QLabel*                                               RecoveryNotice = nullptr;
		QPushButton*                                          ReloadProfiles = nullptr;
		std::map<core::bpr_profile_id, runtime_entry>         Runtimes;
		application::profile_management_projection            Projection;
		QTimer                                                Poll;
		std::shared_ptr<implementation>                       Lifetime;
		std::uint64_t                                         NextStartup       = 0;
		bool                                                  InitialRegistered = false, Quitting = false, Focusing = false;
		int                                                   ExitCode = 0;
	};

	profile_desktop::profile_desktop(std::shared_ptr<implementation> aImplementation)
		: Implementation{ std::move(aImplementation) }
	{ }
	profile_desktop::~profile_desktop() { Implementation->force_shutdown(0); }
	auto profile_desktop::start_with_factory(core::bpr_profile_id aProfile, startup_factory aFactory, profile_desktop_startup_completion aCompletion) -> std::unique_ptr<profile_desktop>
	{
		auto state   = std::make_shared<implementation>(aProfile, std::move(aFactory), std::move(aCompletion));
		auto desktop = std::unique_ptr<profile_desktop>{ new profile_desktop{ state } };
		state->begin();
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

	auto profile_desktop::start(github_application_config aConfig, profile_desktop_startup_completion aCompletion) -> std::unique_ptr<profile_desktop>
	{
		const auto profile = aConfig.Profile;
		return start_with_factory(profile, [config = std::move(aConfig)](core::bpr_profile_id aProfile, const std::filesystem::path& aRoot, github_application_startup_completion aDone) {
            auto next = configuration_for_profile(config, aProfile, aRoot);
            return github_application::start_profile(std::move(next), std::move(aDone)); }, std::move(aCompletion));
	}
	auto profile_desktop::runtime(core::bpr_profile_id aProfile) const noexcept -> github_application* { return Implementation->runtime(aProfile); }
	auto profile_desktop::windows(core::bpr_profile_id aProfile) const -> std::vector<core::bpr_window_context> { return Implementation->windows(aProfile); }
	auto profile_desktop::window(core::bpr_window_context aContext) const noexcept -> qt::workbench_window* { return Implementation->window(aContext); }
	auto profile_desktop::management_window() const noexcept -> qt::profile_management_window* { return Implementation->management_window(); }
	void profile_desktop::quit(int aExitCode) { Implementation->quit(aExitCode); }
	auto profile_desktop::is_quiescent() const noexcept -> bool { return Implementation->is_quiescent(); }
	auto profile_desktop::exit_code() const noexcept -> int { return Implementation->exit_code(); }
}
