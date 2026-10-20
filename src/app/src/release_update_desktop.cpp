#include "bpr/app/release_update_desktop.hpp"

#include "bpr/adapters/sqlite/release_update_store.hpp"
#include "bpr/adapters/update/release_update_installation.hpp"
#include "bpr/adapters/update/release_update_staging.hpp"
#include "bpr/app/github_application.hpp"
#include "bpr/app/worker_executor.hpp"
#include "bpr/qt/event_loop_executor.hpp"
#include "bpr/qt/release_update_panel.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "github_release_update.hpp"
#include "system_browser_opener.hpp"

#include <QCoreApplication>
#include <QGroupBox>
#include <QLayout>
#include <QPointer>
#include <QTimer>
#include <QVBoxLayout>

#include <chrono>
#include <limits>
#include <optional>

namespace bpr::app {
	namespace {
		struct update_clock final : application::release_update_clock_port
		{
			auto now() noexcept -> core::release_update_time override
			{
				return { std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count(),
						 static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count()) };
			}
		};
	}
	struct release_update_desktop::implementation final : QObject, application::release_update_projection_port, std::enable_shared_from_this<implementation>
	{
		worker_executor                                          Effects;
		worker_executor                                          Cleanup;
		qt::event_loop_executor                                  Decisions;
		release_update_components                                Components;
		std::unique_ptr<application::release_update_service>     Service;
		std::unique_ptr<QWidget>                                 Dialog;
		std::unique_ptr<qt::release_update_menu>                 Menu;
		QPointer<qt::release_update_panel>                       MainPanel;
		std::vector<QPointer<qt::release_update_panel>>          Panels;
		std::vector<release_update_account>                      Accounts;
		std::optional<release_update_account>                    Public;
		std::shared_ptr<application::release_update_access_port> Access;
		QTimer                                                   Timer;
		bool                                                     Initializing = false;
		bool                                                     Closing      = false;
		bool                                                     Acquiring    = false;
		bool                                                     Offered      = false;
		std::string                                              RequestedBinding;
		std::optional<std::string>                               QueuedBinding;
		std::uint64_t                                            AccessGeneration = 0;

		~implementation() override
		{
			Timer.stop();
			Service.reset();
			Access.reset();
			Effects.close_and_drain();
			Cleanup.close_and_drain();
		}
		void start(release_update_components aComponents)
		{
			Initializing = false;
			if (Closing) {
				return;
			}
			Components = std::move(aComponents);
			if (!Components.Clock) {
				Components.Clock = std::make_shared<update_clock>();
			}
			Service = std::make_unique<application::release_update_service>(Components.Installed, Components.Capability,
																			*Components.Clock, Decisions, Effects, Components.Settings, Components.Verifier, *this, Components.Installer);
			Service->start();
			Timer.setInterval(1000);
			QObject::connect(&Timer, &QTimer::timeout, this, [this] {
				if (!Service || Closing) {
					return;
				}
				if (Access && Access->binding().value() == 0U) {
					Access.reset();
					Service->revoke_access();
					RequestedBinding.clear();
				}
				restore_access();
				Service->tick();
			});
			Timer.start();
		}
		void start_native(std::filesystem::path aRoot)
		{
			Initializing         = true;
			const auto installed = std::filesystem::path{ QCoreApplication::applicationDirPath().toStdString() }.parent_path().parent_path();
			const auto version   = QCoreApplication::applicationVersion().toStdString();
			// Inside the automated test boundary nothing is installed and no
			// browser opens; the composition's one reader answers, so a
			// shipped build has no boundary to be inside ([[BPR-0983]]).
			const bool boundary = inside_automated_test_boundary();
			auto       staging  = adapters::update::make_release_update_staging(aRoot, installed, Cleanup);
			auto       store    = adapters::sqlite::make_release_update_store(aRoot);
			Effects.post([self = shared_from_this(), installed, version, staging, store, boundary, root = std::move(aRoot)] {
				const auto                                                capability = boundary ? core::release_update_capability::untrusted_installation : adapters::update::release_update_installation_capability(installed, { });
				std::shared_ptr<application::release_update_install_port> installer;
#if defined(__APPLE__)
				if (capability == core::release_update_capability::eligible) {
					installer = adapters::update::make_release_update_installer(installed, root);
				}
#else
				static_cast<void>(root);
#endif
				self->Decisions.post([self, version, staging, store, boundary, capability, installer] {
					self->Public = release_update_account{ "public:github.com", "Public releases (no sign-in)", [](core::release_update_access_id aIdentity, std::shared_ptr<application::release_update_archive_store> aStore, std::function<void(std::shared_ptr<application::release_update_access_port>)> aDone) {
															  aDone(adapters::github::detail::make_public_release_update_access(aIdentity, std::move(aStore)));
														  } };
					self->Accounts.insert(self->Accounts.begin(), *self->Public);
					self->start({ version, capability, std::make_shared<update_clock>(), store, staging, staging,
								  [boundary](std::string aAddress) { if (!boundary) { system_browser_opener opener; static_cast<void>(opener.open(aAddress)); } }, installer });
				});
			});
		}
		auto dispatch() -> application::release_update_dispatch
		{
			const auto weak = weak_from_this();
			return {
				[weak](core::release_update_intent aIntent) { if (auto self = weak.lock(); self && self->Service && !self->Closing) { self->Service->dispatch(std::move(aIntent)); } },
				[weak](bool aEnabled) { if (auto self = weak.lock(); self && self->Service) { self->Service->set_automatic_checks(aEnabled); } },
				[weak] { if (auto self = weak.lock(); self && self->Components.Browser) {
                    const auto current = self->Service ? self->Service->projection().Candidate : std::nullopt;
                    self->Components.Browser(adapters::github::detail::release_update_browser_address(current ? current->Tag : std::string_view{}));
                } },
			};
		}
		auto add_panel(QWidget* aParent) -> qt::release_update_panel*
		{
			auto* panel = new qt::release_update_panel{ aParent };
			panel->set_dispatch(dispatch());
			Panels.emplace_back(panel);
			if (Service) {
				panel->adopt(Service->projection(), Service->durable());
			}
			return panel;
		}
		void present()
		{
			if (Closing) {
				return;
			}
			if (!Dialog) {
				Dialog = std::make_unique<QWidget>(nullptr, Qt::Window);
				Dialog->setWindowTitle(QStringLiteral("BetterPullReview Updates"));
				auto* column = new QVBoxLayout{ Dialog.get() };
				column->setContentsMargins(20, 18, 20, 18);
				// The window follows the panel, so release notes and progress
				// get room when they appear and give it back when they go.
				column->setSizeConstraint(QLayout::SetFixedSize);
				MainPanel = add_panel(Dialog.get());
				MainPanel->setFixedWidth(480);
				column->addWidget(MainPanel);
			}
			Dialog->show();
			Dialog->raise();
			Dialog->activateWindow();
		}
		void publish(const application::release_update_projection& aProjection, bool) override
		{
			std::erase_if(Panels, [](const auto& aPanel) { return aPanel.isNull(); });
			for (const auto& panel : Panels) {
				panel->adopt(aProjection, Service && Service->durable());
			}
			// Release access is chosen as soon as the service can take it, so
			// the first gesture already has it ([[BPR-1671]]).
			restore_access();
			const auto newOffer = aProjection.OfferVisible && !Offered;
			Offered             = aProjection.OfferVisible;
			if (newOffer) {
				present();
			}
		}
		void restore_access()
		{
			if (!Service || !Service->ready() || !Service->projection().AutomaticAvailable || Acquiring || Access || Closing) {
				return;
			}
			// No one picks an account: the stored binding is kept while it is
			// still offered, and otherwise the first offer is taken. Natively
			// that is the public releases, which need no sign-in, so a
			// GHE-only setup still updates ([[BPR-1669]], [[BPR-1671]]).
			auto binding = Service->account_binding();
			if (binding.empty() || std::ranges::find(Accounts, binding, &release_update_account::Binding) == Accounts.end()) {
				if (Accounts.empty()) {
					return;
				}
				binding = Accounts.front().Binding;
			}
			if (RequestedBinding != binding) {
				select(binding);
			}
		}
		void select(std::string aBinding)
		{
			if (!Service || !Service->ready() || !Service->projection().AutomaticAvailable || AccessGeneration == std::numeric_limits<std::uint64_t>::max()) {
				return;
			}
			if (Acquiring) {
				QueuedBinding = std::move(aBinding);
				return;
			}
			const auto generation = ++AccessGeneration;
			Access.reset();
			// Recorded before the service publishes, so the restore that
			// publication runs does not select the same binding again.
			RequestedBinding = aBinding;
			Acquiring        = false;
			Service->select_access(aBinding, { });
			const auto selected = std::ranges::find(Accounts, aBinding, &release_update_account::Binding);
			if (selected == Accounts.end() || !selected->Acquire) {
				return;
			}
			Acquiring = true;
			try {
				selected->Acquire(core::release_update_access_id{ generation }, Components.Archives,
								  [weak = weak_from_this(), generation, binding = std::move(aBinding)](std::shared_ptr<application::release_update_access_port> aAccess) {
									  if (auto self = weak.lock(); self && self->Acquiring && generation == self->AccessGeneration) {
										  self->Acquiring = false;
										  if (self->Closing || !self->Service) {
											  return;
										  }
										  if (self->QueuedBinding) {
											  auto next = std::move(*self->QueuedBinding);
											  self->QueuedBinding.reset();
											  self->select(std::move(next));
											  return;
										  }
										  self->Access = std::move(aAccess);
										  self->Service->select_access(binding, self->Access);
									  }
								  });
			} catch (...) {
				Acquiring = false;
				QueuedBinding.reset();
			}
		}
		void shutdown()
		{
			Closing = true;
			Timer.stop();
			if (Dialog) {
				Dialog->hide();
			}
			if (Service) {
				Service->shutdown();
			}
			Access.reset();
		}
	};
	release_update_desktop::release_update_desktop(std::shared_ptr<implementation> aImpl)
		: Impl{ std::move(aImpl) }
	{ }
	release_update_desktop::~release_update_desktop() { Impl->shutdown(); }
	auto release_update_desktop::start_native(std::filesystem::path aRoot) -> std::unique_ptr<release_update_desktop>
	{
		auto impl   = std::make_shared<implementation>();
		auto result = std::unique_ptr<release_update_desktop>{ new release_update_desktop{ impl } };
		impl->start_native(std::move(aRoot));
		return result;
	}
	auto release_update_desktop::start(release_update_components aComponents) -> std::unique_ptr<release_update_desktop>
	{
		auto impl   = std::make_shared<implementation>();
		auto result = std::unique_ptr<release_update_desktop>{ new release_update_desktop{ impl } };
		impl->start(std::move(aComponents));
		return result;
	}
	void release_update_desktop::attach(qt::workbench_window& aWindow)
	{
		attach(*aWindow.menuBar());
		if (auto* settings = aWindow.settings()) {
			attach(*settings);
		}
	}
	void release_update_desktop::attach(qt::settings_window& aSettings)
	{
		if (!aSettings.findChild<QWidget*>(QStringLiteral("releaseUpdatePanel"))) {
			// Among other General settings the headline needs its subject,
			// named the way the Diagnostics section names its own.
			auto* group = new QGroupBox{ QStringLiteral("Software Update") };
			group->setObjectName(QStringLiteral("releaseUpdateGroup"));
			auto* column = new QVBoxLayout{ group };
			column->addWidget(Impl->add_panel(group));
			aSettings.adopt_panel(qt::settings_page::general, group);
		}
	}
	void release_update_desktop::attach(QMenuBar& aMenu)
	{
		if (!Impl->Menu) {
			Impl->Menu = std::make_unique<qt::release_update_menu>([weak = Impl->weak_from_this()] {
				if (auto self = weak.lock(); self && !self->Closing) {
					self->present();
					if (self->Service) {
						self->Service->dispatch({ core::release_update_command::check, { } });
					}
				}
			});
		}
		Impl->Menu->attach(aMenu);
	}
	void release_update_desktop::set_accounts(std::vector<release_update_account> aAccounts)
	{
		Impl->Accounts = std::move(aAccounts);
		if (Impl->Public) {
			Impl->Accounts.insert(Impl->Accounts.begin(), *Impl->Public);
		}
		Impl->restore_access();
	}
	void release_update_desktop::shutdown() { Impl->shutdown(); }
	auto release_update_desktop::quiescent() const -> bool { return !Impl->Initializing && !Impl->Acquiring && (!Impl->Service || Impl->Service->quiescent()) && Impl->Effects.idle() && Impl->Cleanup.idle(); }
	auto release_update_desktop::durable() const -> bool { return !Impl->Initializing && (!Impl->Service || Impl->Service->durable()); }
	auto release_update_desktop::observe_installation_result(bool aInstalled) -> bool { return Impl->Service && Impl->Service->observe_installation_result(aInstalled); }
	auto release_update_desktop::panel() const -> qt::release_update_panel* { return Impl->MainPanel; }
	auto release_update_desktop::projection() const -> application::release_update_projection { return Impl->Service ? Impl->Service->projection() : application::release_update_projection{ }; }
}
