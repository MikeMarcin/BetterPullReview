#ifndef BPR_APP_GITHUB_APPLICATION_APPLICATION_STARTUP_HPP
#define BPR_APP_GITHUB_APPLICATION_APPLICATION_STARTUP_HPP
#include "bpr/app/github_application.hpp"
#include "bpr/app/profile_data_reset.hpp"
#include "bpr/app/worker_executor.hpp"
#include "bpr/application/profile_activation.hpp"
#include "bpr/application/profile_registry.hpp"
#include "bpr/application/profile_runtime_lease.hpp"
#include "bpr/application/profile_startup_admission.hpp"
#include "bpr/application/profile_storage_authority.hpp"
#include "bpr/application/release_maintenance_application_host.hpp"
#include "bpr/application/remembered_connection_store.hpp"
#include "bpr/application/review_queue_enrollment.hpp"
#include "bpr/application/tour_owner_channel.hpp"
#include "bpr/core/model.hpp"
#include "bpr/core/profile_window.hpp"
#include "bpr/core/remembered_connection.hpp"
#include "bpr/qt/event_loop_executor.hpp"
#include "tour_owner_service.hpp"

#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <system_error>
#include <utility>

#if defined(BPR_BUILD_TESTING)
	#include "github_application_fixture_access.hpp"
#endif
#if defined(BPR_LOCAL_AUTOMATION)
	#include "cycle_owner_service.hpp"
#endif

// Starting a github_application: profile admission, the startup steps and
// what they hand the first window.
namespace bpr::app {
	auto initialize_or_reset_designated_live_replay_state(
		const std::filesystem::path& aApplicationDataRoot) noexcept -> bool;

	[[nodiscard]] auto compatibility_window_context(core::bpr_profile_id aProfile) -> core::bpr_window_context;

	class github_application_startup::implementation final
		: public application::profile_registry_port,
		  public application::profile_startup_root_port,
		  public application::profile_startup_projection_port,
		  public std::enable_shared_from_this<implementation>
	{
	public:
		implementation(github_application_config aConfig, github_application_startup_completion aCompletion
#if defined(BPR_BUILD_TESTING)
					   ,
					   std::optional<detail::github_public_path_fixture> aFixture = std::nullopt,
					   detail::github_public_path_fixture_options        aOptions = { }
#endif
		);
		~implementation() override;
		void begin();
		void cancel();
		auto is_quiescent() const noexcept -> bool;
		auto load(std::stop_token aStop) -> application::profile_registry_result<application::profile_registry_snapshot> override;
		auto save(const application::profile_registry_snapshot&, std::uint64_t, std::stop_token) -> application::profile_registry_result<application::profile_registry_snapshot> override;
		auto resolve(const application::profile_storage_authority& aAuthority, std::stop_token aStop) -> application::profile_startup_result<std::filesystem::path> override;
		bool OwnerOnly = false;
		void publish(const application::profile_startup_terminal& aTerminal) override;

	private:
		void                                                    finish(github_application_startup_result aResult);
		github_application_config                               Config;
		std::shared_ptr<application::profile_runtime_lease>     RuntimeLease;
		std::shared_ptr<application::profile_activation_server> ActivationServer;
		std::shared_ptr<application::tour_owner_server>         TourOwnerServer;
		std::shared_ptr<const tour_owner_service>               TourOwnerService;
#if defined(BPR_LOCAL_AUTOMATION)
		std::shared_ptr<cycle_owner_service> CycleOwnerService;
#endif
		std::optional<github_application_startup_failure>                                  Handoff;
		std::optional<profile_data_reset_report>                                           DataResetReport;
		github_application_startup_completion                                              Completion;
		std::filesystem::path                                                              RegistryRoot;
		bool                                                                               RegistryRootNested = false;
		std::shared_ptr<void>                                                              Scratch;
		std::optional<application::durable_queue_state>                                    SavedQueue;
		std::unique_ptr<application::remembered_connection_store_port>                     ConnectionStore;
		application::remembered_connection_store_result<core::remembered_connection_state> SavedConnections = std::unexpected(application::remembered_connection_store_failure::unavailable);
#if defined(BPR_BUILD_TESTING)
		std::optional<detail::github_public_path_fixture> Fixture;
		detail::github_public_path_fixture_options        Options;
#endif
		worker_executor                   Effects;
		qt::event_loop_executor           Decisions;
		application::profile_startup_host Host;
		bool                              Cancelled = false;
		bool                              Terminal  = false;
	};
}
#endif
