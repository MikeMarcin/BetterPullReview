#include "bpr/app/profile_desktop.hpp"
#include "bpr/adapters/sqlite/profile_registry_store.hpp"
#include "bpr/app/worker_executor.hpp"
#include "bpr/application/profile_management_host.hpp"
#include "bpr/qt/event_loop_executor.hpp"
#include "bpr/qt/profile_management_window.hpp"
#include <QEvent>
#include <QObject>
#include <QTimer>
#include <map>
#include <utility>

namespace bpr::app {
	class profile_desktop::implementation final : public QObject,
												  public application::profile_management_projection_port,
												  public std::enable_shared_from_this<implementation>
	{
		struct runtime_entry
		{
			std::optional<core::bpr_window_context>     Context;
			std::unique_ptr<github_application_startup> Startup;
			std::unique_ptr<github_application>         Runtime;
			bool                                        Closing = false;
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
		auto management_window() const noexcept -> qt::profile_management_window* { return Management.get(); }
		void quit(int aExitCode)
		{
			if (aExitCode != 0) {
				ExitCode = aExitCode;
			}
			if (Quitting) {
				return;
			}
			Quitting = true;
			if (Host) {
				Host->shutdown();
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
				if (entry.Runtime) {
					entry.Runtime->window().hide();
					entry.Runtime->begin_shutdown();
				}
			}
		}
		auto is_quiescent() const noexcept -> bool { return Quitting && Runtimes.empty() && (!Host || Host->is_quiescent()) && Effects.idle(); }
		auto exit_code() const noexcept -> int { return ExitCode; }
		void publish(const application::profile_management_projection& aProjection) override
		{
			Projection = aProjection;
			if (Management) {
				Management->adopt(aProjection);
			}
			for (auto& [profile, entry] : Runtimes) {
				if (!entry.Runtime) {
					continue;
				}
				entry.Runtime->window().adopt_profile_management(aProjection, profile);
				for (const auto& item : aProjection.Profiles) {
					if (item.Profile == profile) {
						entry.Runtime->window().set_profile_label(QString::fromStdString(item.Label));
						break;
					}
				}
			}
			if (!InitialRegistered && aProjection.Ready && !aProjection.Closing) {
				InitialRegistered  = true;
				const auto initial = Runtimes.find(Initial);
				if (initial != Runtimes.end() && initial->second.Runtime && !initial->second.Closing) {
					Host->apply(application::open_bpr_profile_intent{ Initial });
				}
			}
		}
		void perform(const application::profile_runtime_command& aCommand) override
		{
			const auto profile = aCommand.Context.Profile;
			auto       found   = Runtimes.find(profile);
			if (aCommand.Kind == application::profile_runtime_command_kind::activate) {
				if (found != Runtimes.end() && found->second.Runtime && found->second.Context == aCommand.Context && !found->second.Closing) {
					auto& window = found->second.Runtime->window();
					if (window.isMinimized()) {
						window.showNormal();
					} else {
						window.show();
					}
					window.raise();
					window.activateWindow();
				}
				return;
			}
			if (found != Runtimes.end() && found->second.Runtime && !found->second.Context && !found->second.Closing) {
				found->second.Context = aCommand.Context;
				static_cast<void>(Host->complete_open(aCommand.Context));
				return;
			}
			start_runtime(profile, aCommand.Context);
		}

	protected:
		auto eventFilter(QObject* aObject, QEvent* aEvent) -> bool override
		{
			if (aEvent->type() == QEvent::Close) {
				if (aObject == Management.get() && Runtimes.empty()) {
					quit(0);
				}
				for (auto& [profile, entry] : Runtimes) {
					static_cast<void>(profile);
					if (entry.Runtime && &entry.Runtime->window() == aObject && !entry.Closing) {
						entry.Closing = true;
						if (entry.Context && Host) {
							static_cast<void>(Host->begin_close(*entry.Context));
						}
						entry.Runtime->begin_shutdown();
						break;
					}
				}
			}
			return QObject::eventFilter(aObject, aEvent);
		}

	private:
		void wire(github_application& aRuntime, core::bpr_profile_id aProfile)
		{
			const auto weak = weak_from_this();
			aRuntime.window().installEventFilter(this);
			aRuntime.window().set_profile_management_dispatch(
				[weak](application::profile_management_intent aIntent) { if (const auto self = weak.lock(); self && self->Host && !self->Quitting) { self->Host->apply(std::move(aIntent)); } },
				[weak] { if (const auto self = weak.lock(); self && self->Management && !self->Quitting) { self->Management->present(); } },
				[weak] { if (const auto self = weak.lock()) { self->quit(0); } });
			aRuntime.set_desktop_quit_handler([weak](int aExitCode) { if (const auto self = weak.lock()) { self->quit(aExitCode); } });
			aRuntime.window().adopt_profile_management(Projection, aProfile);
		}
		void initialize_management(github_application& aRuntime)
		{
			RegistryRoot = aRuntime.desktop_registry_root();
			Scratch      = aRuntime.desktop_scratch();
			Registry     = adapters::sqlite::make_profile_registry_store(RegistryRoot);
			Management   = std::make_unique<qt::profile_management_window>();
			Management->installEventFilter(this);
			Host = std::make_unique<application::profile_management_host>(*Registry, Effects, Decisions, *this, core::application_session_id{ 1U });
			Management->set_dispatch([weak = weak_from_this()](application::profile_management_intent aIntent) {
				if (const auto self = weak.lock(); self && !self->Quitting) {
					self->Host->apply(std::move(aIntent));
				}
			});
			Host->reload();
		}
		void start_runtime(core::bpr_profile_id aProfile, std::optional<core::bpr_window_context> aContext)
		{
			auto [position, inserted] = Runtimes.try_emplace(aProfile);
			if (!inserted) {
				if (aContext && Host) {
					static_cast<void>(Host->complete_open(*aContext, std::unexpected(application::profile_management_failure::busy)));
				}
				return;
			}
			position->second.Context = aContext;
			try {
				position->second.Startup = Factory(aProfile, RegistryRoot, [weak = weak_from_this(), aProfile, aContext](github_application_startup_result aResult) mutable {
					if (const auto self = weak.lock()) {
						self->started(aProfile, aContext, std::move(aResult));
					}
				});
			} catch (...) {
				Decisions.post([weak = weak_from_this(), aProfile, aContext] { if (const auto self = weak.lock()) { self->started(aProfile, aContext, std::unexpected(github_application_startup_failure::unavailable)); } });
			}
		}
		void started(core::bpr_profile_id aProfile, std::optional<core::bpr_window_context> aContext, github_application_startup_result aResult)
		{
			auto found = Runtimes.find(aProfile);
			if (found == Runtimes.end() || found->second.Context != aContext) {
				return;
			}
			auto& entry = found->second;
			if (!aResult) {
				if (aContext && Host) {
					static_cast<void>(Host->complete_open(*aContext, std::unexpected(application::profile_management_failure::runtime_unavailable)));
				}
				entry.Context.reset();
				entry.Closing = true;
				if (aProfile == Initial && Completion) {
					auto completion = std::move(Completion);
					completion(std::unexpected(aResult.error()));
					quit(1);
				}
				return;
			}
			entry.Runtime = std::move(*aResult);
			if (Quitting || entry.Closing) {
				if (aContext && Host) {
					static_cast<void>(Host->complete_open(*aContext));
				}
				entry.Closing = true;
				entry.Runtime->begin_shutdown();
				return;
			}
			try {
				if (!Host) {
					initialize_management(*entry.Runtime);
				}
				wire(*entry.Runtime, aProfile);
				if (aContext && !Host->complete_open(*aContext)) {
					entry.Closing = true;
					entry.Runtime->begin_shutdown();
					return;
				}
				entry.Runtime->show();
				if (aProfile == Initial && Completion) {
					auto completion = std::move(Completion);
					completion(entry.Runtime.get());
				}
			} catch (...) {
				entry.Closing = true;
				entry.Runtime->begin_shutdown();
				if (aContext && Host) {
					static_cast<void>(Host->complete_open(*aContext, std::unexpected(application::profile_management_failure::runtime_unavailable)));
					entry.Context.reset();
				}
				if (aProfile == Initial && Completion) {
					auto completion = std::move(Completion);
					completion(std::unexpected(github_application_startup_failure::unavailable));
					quit(1);
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
				if (!entry.Closing || entry.Startup || (entry.Runtime && !entry.Runtime->status().ShutdownComplete)) {
					++position;
					continue;
				}
				if (entry.Runtime && entry.Runtime->status().ShutdownCleanupFailed) {
					ExitCode = 1;
				}
				const auto context = entry.Context;
				entry.Runtime.reset();
				position = Runtimes.erase(position);
				if (Runtimes.empty() && !Quitting && Management) {
					Management->present();
				}
				if (context && Host) {
					static_cast<void>(Host->complete_close(*context));
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
		std::map<core::bpr_profile_id, runtime_entry>         Runtimes;
		application::profile_management_projection            Projection;
		QTimer                                                Poll;
		std::shared_ptr<implementation>                       Lifetime;
		bool                                                  InitialRegistered = false;
		bool                                                  Quitting          = false;
		int                                                   ExitCode          = 0;
	};
	profile_desktop::profile_desktop(std::shared_ptr<implementation> aImplementation)
		: Implementation{ std::move(aImplementation) }
	{ }
	profile_desktop::~profile_desktop() { Implementation->quit(0); }
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
            return github_application::start(std::move(next), std::move(aDone)); }, std::move(aCompletion));
	}
	auto profile_desktop::runtime(core::bpr_profile_id aProfile) const noexcept -> github_application* { return Implementation->runtime(aProfile); }
	auto profile_desktop::management_window() const noexcept -> qt::profile_management_window* { return Implementation->management_window(); }
	void profile_desktop::quit(int aExitCode) { Implementation->quit(aExitCode); }
	auto profile_desktop::is_quiescent() const noexcept -> bool { return Implementation->is_quiescent(); }
	auto profile_desktop::exit_code() const noexcept -> int { return Implementation->exit_code(); }
}
