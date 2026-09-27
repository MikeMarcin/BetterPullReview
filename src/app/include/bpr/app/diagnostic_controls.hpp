#ifndef BPR_APP_DIAGNOSTIC_CONTROLS_HPP
#define BPR_APP_DIAGNOSTIC_CONTROLS_HPP
#include "bpr/application/diagnostic_settings.hpp"
#include <filesystem>
#include <memory>
namespace bpr::qt {
	class settings_window;
}
namespace bpr::app {
	/// \brief Profile-owned concrete settings effects, panel and bounded control lifetime.
	class diagnostic_controls final
	{
	public:
		diagnostic_controls(std::filesystem::path aRoot, std::shared_ptr<application::diagnostic_control_port> aControl, application::diagnostic_preference_load aLoaded);
		~diagnostic_controls();
		void               attach(qt::settings_window& aSettings);
		void               shutdown();
		[[nodiscard]] auto quiescent() const -> bool;

	private:
		struct implementation;
		std::unique_ptr<implementation> Impl;
	};
}
#endif
