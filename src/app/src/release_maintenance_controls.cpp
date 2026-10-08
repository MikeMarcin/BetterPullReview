#include "bpr/app/release_maintenance_controls.hpp"

#include "bpr/adapters/sqlite/device_binding_store.hpp"
#include "bpr/adapters/sqlite/release_maintenance_files.hpp"
#include "bpr/app/diagnostic_export_directory.hpp"
#include "bpr/app/native_filesystem_path.hpp"
#include "bpr/app/worker_executor.hpp"
#include "bpr/qt/event_loop_executor.hpp"
#include "bpr/qt/release_maintenance_panel.hpp"
#include "bpr/qt/settings_window.hpp"
#include "platform_credential_vault.hpp"

#include <QPointer>
#include <QRandomGenerator>

namespace bpr::app {
	namespace {
		using reason = core::release_maintenance_reason;

		class local_output final : public application::release_diagnostic_output
		{
		public:
			explicit local_output(std::filesystem::path aPath)
				: Path{ std::move(aPath) }
			{ }
			auto write(std::string_view aBytes, std::stop_token aStop, const std::function<bool()>& aBeginCommit)
				-> application::release_maintenance_result override
			{
				const auto output = adapters::sqlite::admit_release_diagnostic_output(Path);
				return output ? (*output)->write(aBytes, aStop, aBeginCommit) : std::unexpected(output.error());
			}

		private:
			std::filesystem::path Path;
		};

		class profile_storage final : public application::release_maintenance_storage_port
		{
		public:
			profile_storage(std::filesystem::path aRoot, std::shared_ptr<application::profile_runtime_lease> aLease,
							core::bpr_profile_id aProfile, core::application_session_id aSession, bool aRetirement, std::shared_ptr<application::diagnostic_control_port> aDiagnostics = { })
				: Vault{ make_platform_vault(VaultEffects, aProfile) }
				, Credentials{ Vault ? std::make_unique<application::release_profile_credentials>(aProfile, aSession, [root = aRoot, aProfile] { return adapters::sqlite::make_device_binding_store(root, aProfile); }, *Vault) : nullptr }
				, Files{ std::move(aRoot), std::move(aLease), aProfile, aSession, Credentials.get(), std::move(aDiagnostics) }
				, Retirement{ aRetirement }
			{ }
			auto authority() const noexcept -> core::release_maintenance_authority override { return Files.authority(); }
			auto capabilities() const noexcept -> core::release_maintenance_capabilities override
			{
				auto result  = Files.capabilities();
				result.Clear = result.Clear && Retirement;
				result.Purge = result.Purge && Retirement && Vault != nullptr;
				return result;
			}
			auto prepare(const core::release_maintenance_context& aContext, std::stop_token aStop)
				-> std::expected<std::shared_ptr<const application::release_maintenance_preparation>, reason> override
			{
				if (aContext.Operation != core::release_maintenance_command::preview_diagnostics && !Retirement) {
					return std::unexpected(reason::unavailable);
				}
				return Files.prepare(aContext, aStop);
			}
			auto revalidate(const core::release_maintenance_context& aContext, const application::release_maintenance_preparation& aPreparation)
				-> application::release_maintenance_result override { return Files.revalidate(aContext, aPreparation); }
			auto erase(const core::release_maintenance_context& aContext, const application::release_maintenance_preparation& aPreparation,
					   std::stop_token aStop, const std::function<bool()>& aBeginCommit) -> application::release_maintenance_result override
			{
				return Retirement ? Files.erase(aContext, aPreparation, aStop, aBeginCommit) : std::unexpected(reason::unavailable);
			}
			auto recover() -> application::release_maintenance_result { return Files.recover(); }

		private:
			worker_executor                                            VaultEffects;
			std::unique_ptr<application::github_credential_vault_port> Vault;
			std::unique_ptr<application::release_profile_credentials>  Credentials;
			adapters::sqlite::release_maintenance_files                Files;
			bool                                                       Retirement;
		};

		class desktop_lifecycle final : public application::release_maintenance_lifecycle_port
		{
		public:
			explicit desktop_lifecycle(release_maintenance_retirement aRetire)
				: Retire{ std::move(aRetire) }
			{ }
			void quiesce(const core::release_maintenance_context&, std::stop_token,
						 std::function<void(application::release_maintenance_result)> aCompletion) override
			{
				if (Retire) {
					Retire(std::move(aCompletion));
				} else {
					aCompletion(std::unexpected(reason::unavailable));
				}
			}

		private:
			release_maintenance_retirement Retire;
		};
	}

	struct release_maintenance_controls::implementation final : application::release_maintenance_projection_port,
																std::enable_shared_from_this<implementation>
	{
		worker_executor                                                    Effects;
		qt::event_loop_executor                                            Decisions;
		std::shared_ptr<application::release_maintenance_storage_port>     Storage;
		std::shared_ptr<application::release_maintenance_lifecycle_port>   Lifecycle;
		std::unique_ptr<application::release_maintenance_application_host> Host;
		application::release_maintenance_projection                        Projection;
		QPointer<qt::release_maintenance_panel>                            Panel;
		std::unique_ptr<qt::settings_window>                               RetainedSettings;
		bool                                                               Transferring = false;
		bool                                                               Initializing = false;
		bool                                                               Closing      = false;

		void shutdown()
		{
			Closing = true;
			if (Host) {
				Host->shutdown();
			}
		}

		void publish(const application::release_maintenance_projection& aProjection) override
		{
			Projection = aProjection;
			if (Panel) {
				Panel->adopt(Projection);
			}
		}
		void start(std::shared_ptr<application::release_maintenance_storage_port>   aStorage,
				   std::shared_ptr<application::release_maintenance_lifecycle_port> aLifecycle)
		{
			Initializing = false;
			if (Closing) {
				return;
			}
			if (!aStorage || !aLifecycle) {
				const auto unavailable = core::make_release_maintenance_state({ }, { });
				publish({ unavailable, core::release_maintenance_commands(unavailable), { }, { }, reason::unavailable });
				return;
			}
			Storage   = std::move(aStorage);
			Lifecycle = std::move(aLifecycle);
			Host      = std::make_unique<application::release_maintenance_application_host>(*Storage, *Lifecycle, Effects, Decisions, *this);
			publish({ Host->state(), core::release_maintenance_commands(Host->state()), { }, { }, reason::none });
		}
		void start_profile(std::filesystem::path aRoot, std::shared_ptr<application::profile_runtime_lease> aLease, core::bpr_profile_id aProfile, release_maintenance_retirement aRetire, std::shared_ptr<application::diagnostic_control_port> aDiagnostics)
		{
			Initializing = true;
			Effects.post([self = shared_from_this(), root = std::move(aRoot), lease = std::move(aLease), aProfile, retire = std::move(aRetire), diagnostics = std::move(aDiagnostics)] {
				std::shared_ptr<application::release_maintenance_storage_port> storage;
				try {
					const auto session = QRandomGenerator::securelySeeded().generate64();
					if (session != 0U) {
						storage = std::make_shared<profile_storage>(root, lease, aProfile, core::application_session_id{ session }, static_cast<bool>(retire), diagnostics);
					}
				} catch (...) { }
				self->Decisions.post([self, admitted = std::move(storage), retire] { self->start(admitted, std::make_shared<desktop_lifecycle>(retire)); });
			});
		}
		void observe_export_directory(core::release_maintenance_intent aIntent)
		{
			const auto panel = Panel;
			Effects.post([self = shared_from_this(), panel, aIntent] {
				application::diagnostic_export_directory directory = std::unexpected(reason::unavailable);
				try {
					directory = resolve_diagnostic_export_directory(diagnostic_export_locations());
				} catch (...) { }
				self->Decisions.post([self, panel, aIntent, initialDirectory = std::move(directory)] {
					if (panel && !self->Closing && !self->Transferring) {
						panel->present_export_directory(aIntent, initialDirectory);
					}
				});
			});
		}
		void attach(qt::settings_window& aSettings, const std::string& aLabel)
		{
			if (!Panel) {
				Panel = new qt::release_maintenance_panel;
				Panel->set_dispatch({ .Intent                = [weak = weak_from_this()](core::release_maintenance_intent aIntent) {
						if (const auto self = weak.lock(); self && self->Host && !self->Closing && !self->Transferring) {
							self->Host->dispatch(aIntent);
						} },
									  .Export                = [weak = weak_from_this()](core::release_maintenance_intent aIntent, std::string aPath) {
						if (const auto self = weak.lock(); self && self->Host && !self->Closing && !self->Transferring) {
							self->Host->dispatch(aIntent, std::make_shared<local_output>(native_filesystem_path(QString::fromStdString(aPath))));
						} },
									  .ChooseExportDirectory = [weak = weak_from_this()](core::release_maintenance_intent aIntent) {
							if (const auto self = weak.lock(); self && self->Host && !self->Closing && !self->Transferring) {
								self->observe_export_directory(aIntent);
							} } });
			}
			// The profile Settings shell borrows the invoking window's pages. Keep the
			// profile-owned surface with that shell when those pages are exchanged.
			if (Panel->parentWidget() != aSettings.page_widget(qt::settings_page::storage_and_privacy)) {
				aSettings.adopt_panel(qt::settings_page::storage_and_privacy, Panel);
			}
			Panel->set_profile_label(QString::fromStdString(aLabel));
			if (!Initializing) {
				Panel->adopt(Projection);
			}
		}
	};

	release_maintenance_controls::release_maintenance_controls(std::shared_ptr<implementation> aImpl)
		: Impl{ std::move(aImpl) }
	{ }
	auto release_maintenance_controls::start_profile(std::filesystem::path                               aRoot,
													 std::shared_ptr<application::profile_runtime_lease> aLease, core::bpr_profile_id aProfile, release_maintenance_retirement aRetire, std::shared_ptr<application::diagnostic_control_port> aDiagnostics) -> std::unique_ptr<release_maintenance_controls>
	{
		auto impl = std::make_shared<implementation>();
		impl->start_profile(std::move(aRoot), std::move(aLease), aProfile, std::move(aRetire), std::move(aDiagnostics));
		return std::unique_ptr<release_maintenance_controls>{ new release_maintenance_controls{ std::move(impl) } };
	}
	release_maintenance_controls::~release_maintenance_controls()
	{
		shutdown();
		if (Impl->Panel) {
			Impl->Panel->set_dispatch({ });
		}
		if (Impl->RetainedSettings) {
			Impl->RetainedSettings->set_close_handler({ });
		}
	}
	void release_maintenance_controls::attach(qt::settings_window& aSettings, const std::string& aLabel) { Impl->attach(aSettings, aLabel); }
	void release_maintenance_controls::open_export(core::diagnostic_export_policy aPolicy)
	{
		if (Impl->Panel && !Impl->Closing) {
			Impl->Panel->open_export(aPolicy);
		}
	}
	void release_maintenance_controls::retain_settings(std::unique_ptr<qt::settings_window> aSettings, const std::string& aLabel, std::function<void()> aUnmount)
	{
		Impl->Transferring = true;
		if (aUnmount) {
			aUnmount();
		}
		Impl->attach(*aSettings, aLabel);
		Impl->RetainedSettings = std::move(aSettings);
		Impl->RetainedSettings->set_close_handler([weak = std::weak_ptr{ Impl }] {
			if (const auto self = weak.lock()) {
				self->shutdown();
			}
		});
		Impl->RetainedSettings->present(qt::settings_page::storage_and_privacy);
		Impl->Transferring = false;
	}
	auto release_maintenance_controls::closing() const -> bool { return Impl->Closing; }
	void release_maintenance_controls::shutdown()
	{
		Impl->shutdown();
	}
	auto release_maintenance_controls::quiescent() const -> bool
	{
		return !Impl->Initializing && (!Impl->Host || Impl->Host->is_quiescent()) && Impl->Effects.idle();
	}
	auto recover_release_profile(const std::filesystem::path& aRoot, std::shared_ptr<application::profile_runtime_lease> aLease, core::bpr_profile_id aProfile) -> application::release_maintenance_result
	{
		try {
			profile_storage storage{ aRoot, std::move(aLease), aProfile, core::application_session_id{ 1U }, true };
			return storage.recover();
		} catch (...) {
			return std::unexpected(reason::recovery_required);
		}
	}
}
