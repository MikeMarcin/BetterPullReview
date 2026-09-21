#ifndef BPR_APP_RELEASE_MAINTENANCE_CONTROLS_HPP
#define BPR_APP_RELEASE_MAINTENANCE_CONTROLS_HPP

#include "bpr/application/profile_runtime_lease.hpp"
#include "bpr/application/release_maintenance_application_host.hpp"

#include <filesystem>

namespace bpr::qt {
	class settings_window;
}

namespace bpr::app {
	/// \brief Composes the maintenance host and Settings panel on owned effect/decision lanes.
	/// \details Native diagnostic access is independent of sign-in. This composition cannot
	/// enable destructive capabilities. Shutdown must drain before releasing profile ownership.
	class release_maintenance_controls final
	{
	public:
		[[nodiscard]] static auto start_diagnostics(std::filesystem::path                               aRoot,
													std::shared_ptr<application::profile_runtime_lease> aLease, core::bpr_profile_id aProfile)
			-> std::unique_ptr<release_maintenance_controls>;
		~release_maintenance_controls();
		void               attach(qt::settings_window& aSettings, const std::string& aLabel);
		void               shutdown();
		[[nodiscard]] auto quiescent() const -> bool;

	private:
		struct implementation;
		explicit release_maintenance_controls(std::shared_ptr<implementation> aImpl);
		std::shared_ptr<implementation> Impl;
	};
}
#endif
