#ifndef BPR_APP_RELEASE_MAINTENANCE_CONTROLS_HPP
#define BPR_APP_RELEASE_MAINTENANCE_CONTROLS_HPP

#include "bpr/application/diagnostic_log_port.hpp"
#include "bpr/application/profile_runtime_lease.hpp"
#include "bpr/application/release_maintenance_application_host.hpp"

#include <filesystem>

namespace bpr::qt {
	class settings_window;
}

namespace bpr::app {
	using release_maintenance_retirement = std::function<void(std::function<void(application::release_maintenance_result)>)>;
	/// \brief Reconciles an exact durable cleanup before any profile store or producer opens.
	/// \details Runs on the startup effect lane under the already acquired profile lease.
	[[nodiscard]] auto recover_release_profile(const std::filesystem::path&                        aRoot,
											   std::shared_ptr<application::profile_runtime_lease> aLease, core::bpr_profile_id aProfile)
		-> application::release_maintenance_result;
	/// \brief Composes the maintenance host and Settings panel on owned effect/decision lanes.
	/// \details Native diagnostic access is independent of sign-in. Destruction requires a
	/// desktop retirement barrier; the independent vault worker and root lease outlive it.
	class release_maintenance_controls final
	{
	public:
		[[nodiscard]] static auto start_profile(std::filesystem::path                               aRoot,
												std::shared_ptr<application::profile_runtime_lease> aLease, core::bpr_profile_id aProfile,
												release_maintenance_retirement aRetire = { }, std::shared_ptr<application::diagnostic_control_port> aDiagnostics = { })
			-> std::unique_ptr<release_maintenance_controls>;
		~release_maintenance_controls();
		void attach(qt::settings_window& aSettings, const std::string& aLabel);
		/// \brief Retains the maintenance-only shell while the invoking windows retire.
		void               retain_settings(std::unique_ptr<qt::settings_window> aSettings, const std::string& aLabel,
										   std::function<void()> aUnmount);
		void               shutdown();
		[[nodiscard]] auto quiescent() const -> bool;
		[[nodiscard]] auto closing() const -> bool;

	private:
		struct implementation;
		explicit release_maintenance_controls(std::shared_ptr<implementation> aImpl);
		std::shared_ptr<implementation> Impl;
	};
}
#endif
