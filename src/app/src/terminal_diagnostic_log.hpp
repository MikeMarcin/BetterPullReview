#ifndef BPR_APP_TERMINAL_DIAGNOSTIC_LOG_HPP
#define BPR_APP_TERMINAL_DIAGNOSTIC_LOG_HPP
#include "bpr/application/diagnostic_log_port.hpp"
#include <filesystem>
#include <memory>
namespace bpr::app {
	/// \brief [[BPR-1830]]. The diagnostic log of one `bpr` command that holds
	/// its profile's runtime lease: the profile's own bounded log, opened on
	/// the command's first record as one more cooperating session, so the
	/// desktop's export carries it.
	/// \details Null when the profile's diagnostics are off or their settings
	/// cannot be read, and in a graph that builds no logging adapter. A command
	/// that forwards to a running owner opens none: the owner records what it
	/// runs. Records are only the `tour.operation` and `cycle.round` lines.
	[[nodiscard]] auto open_terminal_diagnostic_log(const std::filesystem::path& aRoot) -> std::unique_ptr<application::diagnostic_log_port>;
}
#endif
