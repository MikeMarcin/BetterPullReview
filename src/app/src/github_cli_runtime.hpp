#ifndef BPR_APP_GITHUB_CLI_RUNTIME_HPP
#define BPR_APP_GITHUB_CLI_RUNTIME_HPP
#include "gh_cli_transport.hpp"
#include "github_provider.hpp"
// RFC-BPR-0015, [[BPR-1791]]. The platforms whose production build composes
// the GitHub CLI way in: macOS since RFC-BPR-0015, Windows and Linux since the
// owner extended it on 2026-10-27. This is the one switch; the composition,
// its consent gates and the `bpr` remote reader all ask it.
#if defined(__APPLE__) || defined(_WIN32) || defined(__linux__)
	#define BPR_GITHUB_CLI_PLATFORM 1
#endif
namespace bpr::app {
#if defined(BPR_GITHUB_CLI_PLATFORM)
	enum class gh_cli_version_probe_failure : std::uint8_t { process_failed,
															 unsupported_version };
	/// \brief The explicitly opted-in CLI transport composition.
	[[nodiscard]] auto make_opted_in_cli_transport_factory() -> std::shared_ptr<adapters::github::detail::github_wire_transport_factory_port>;
	[[nodiscard]] auto gh_cli_executable_available() -> bool;
	[[nodiscard]] auto live_gh_cli_configuration() -> std::optional<adapters::github::detail::validated_gh_cli_configuration>;
	[[nodiscard]] auto probe_supported_gh_cli_version(adapters::github::detail::github_cli_process_port&, const adapters::github::detail::validated_gh_cli_configuration&) -> std::expected<void, gh_cli_version_probe_failure>;
#endif
}
#endif
