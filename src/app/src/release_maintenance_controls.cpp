#include "bpr/app/release_maintenance_controls.hpp"

#include "bpr/adapters/sqlite/release_maintenance_files.hpp"
#include "bpr/app/native_filesystem_path.hpp"
#include "bpr/app/worker_executor.hpp"
#include "bpr/qt/event_loop_executor.hpp"
#include "bpr/qt/release_maintenance_panel.hpp"
#include "bpr/qt/settings_window.hpp"

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

		/// The live runtime has no destructive retirement barrier until M6. Restrict the
		/// capability at composition, before core command projection and gesture admission.
		class diagnostic_storage final : public application::release_maintenance_storage_port
		{
		public:
			diagnostic_storage(std::filesystem::path aRoot, std::shared_ptr<application::profile_runtime_lease> aLease,
							   core::bpr_profile_id aProfile, core::application_session_id aSession)
				: Files{ std::move(aRoot), std::move(aLease), aProfile, aSession }
			{ }
			auto authority() const noexcept -> core::release_maintenance_authority override { return Files.authority(); }
			auto capabilities() const noexcept -> core::release_maintenance_capabilities override
			{
				auto result  = Files.capabilities();
				result.Clear = false;
				result.Purge = false;
				return result;
			}
			auto prepare(const core::release_maintenance_context& aContext, std::stop_token aStop)
				-> std::expected<std::shared_ptr<const application::release_maintenance_preparation>, reason> override
			{
				if (aContext.Operation != core::release_maintenance_command::preview_diagnostics) {
					return std::unexpected(reason::unavailable);
				}
				return Files.prepare(aContext, aStop);
			}
			auto revalidate(const core::release_maintenance_context& aContext, const application::release_maintenance_preparation& aPreparation)
				-> application::release_maintenance_result override { return Files.revalidate(aContext, aPreparation); }
			auto erase(const core::release_maintenance_context&, const application::release_maintenance_preparation&,
					   std::stop_token, const std::function<bool()>&) -> application::release_maintenance_result override
			{
				return std::unexpected(reason::unavailable);
			}

		private:
			adapters::sqlite::release_maintenance_files Files;
		};

		class unavailable_lifecycle final : public application::release_maintenance_lifecycle_port
		{
			void quiesce(const core::release_maintenance_context&, std::stop_token,
						 std::function<void(application::release_maintenance_result)> aCompletion) override
			{
				aCompletion(std::unexpected(reason::unavailable));
			}
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
		bool                                                               Initializing = false;
		bool                                                               Closing      = false;

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
		void start_diagnostics(std::filesystem::path aRoot, std::shared_ptr<application::profile_runtime_lease> aLease, core::bpr_profile_id aProfile)
		{
			Initializing = true;
			Effects.post([self = shared_from_this(), root = std::move(aRoot), lease = std::move(aLease), aProfile] {
				std::shared_ptr<application::release_maintenance_storage_port> storage;
				try {
					const auto session = QRandomGenerator::system()->generate64();
					if (session != 0U) {
						storage = std::make_shared<diagnostic_storage>(root, lease, aProfile, core::application_session_id{ session });
					}
				} catch (...) { }
				self->Decisions.post([self, admitted = std::move(storage)] { self->start(admitted, std::make_shared<unavailable_lifecycle>()); });
			});
		}
		void attach(qt::settings_window& aSettings, const std::string& aLabel)
		{
			if (!Panel) {
				Panel = new qt::release_maintenance_panel;
				Panel->set_dispatch({ .Intent = [weak = weak_from_this()](core::release_maintenance_intent aIntent) {
						if (const auto self = weak.lock(); self && self->Host && !self->Closing) {
							self->Host->dispatch(aIntent);
						} },
									  .Export = [weak = weak_from_this()](core::release_maintenance_intent aIntent, std::string aPath) {
						if (const auto self = weak.lock(); self && self->Host && !self->Closing) {
							self->Host->dispatch(aIntent, std::make_shared<local_output>(native_filesystem_path(QString::fromStdString(aPath))));
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
	auto release_maintenance_controls::start_diagnostics(std::filesystem::path                               aRoot,
														 std::shared_ptr<application::profile_runtime_lease> aLease, core::bpr_profile_id aProfile) -> std::unique_ptr<release_maintenance_controls>
	{
		auto impl = std::make_shared<implementation>();
		impl->start_diagnostics(std::move(aRoot), std::move(aLease), aProfile);
		return std::unique_ptr<release_maintenance_controls>{ new release_maintenance_controls{ std::move(impl) } };
	}
	release_maintenance_controls::~release_maintenance_controls()
	{
		shutdown();
		if (Impl->Panel) {
			Impl->Panel->set_dispatch({ });
		}
	}
	void release_maintenance_controls::attach(qt::settings_window& aSettings, const std::string& aLabel) { Impl->attach(aSettings, aLabel); }
	void release_maintenance_controls::shutdown()
	{
		Impl->Closing = true;
		if (Impl->Host) {
			Impl->Host->shutdown();
		}
	}
	auto release_maintenance_controls::quiescent() const -> bool
	{
		return !Impl->Initializing && (!Impl->Host || Impl->Host->is_quiescent()) && Impl->Effects.idle();
	}
}
