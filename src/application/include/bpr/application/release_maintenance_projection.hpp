#ifndef BPR_APPLICATION_RELEASE_MAINTENANCE_PROJECTION_HPP
#define BPR_APPLICATION_RELEASE_MAINTENANCE_PROJECTION_HPP

#include "bpr/core/release_maintenance.hpp"

#include <expected>
#include <functional>
#include <memory>
#include <string>

namespace bpr::application {
	/// \brief An admitted initial directory, or no eligible standard user folder.
	using diagnostic_export_directory = std::expected<std::string, core::release_maintenance_reason>;
	/// \brief Bounded maintenance presentation; no storage, vault, output capability or effect request.
	struct release_maintenance_projection
	{
		core::release_maintenance_state                             State;
		std::array<core::release_maintenance_command_projection, 7> Commands;
		std::shared_ptr<const std::string>                          Preview;
		std::optional<core::release_maintenance_command>            Operation;
		core::release_maintenance_reason                            Diagnostic = core::release_maintenance_reason::none;
	};

	/// \brief User gestures only; a chosen local filename grants no storage capability.
	struct release_maintenance_dispatch
	{
		std::function<void(core::release_maintenance_intent)>              Intent;
		std::function<void(core::release_maintenance_intent, std::string)> Export;
		std::function<void(core::release_maintenance_intent)>              ChooseExportDirectory;
	};
}
#endif
