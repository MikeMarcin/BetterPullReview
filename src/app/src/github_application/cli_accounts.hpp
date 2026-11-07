#ifndef BPR_APP_GITHUB_APPLICATION_CLI_ACCOUNTS_HPP
#define BPR_APP_GITHUB_APPLICATION_CLI_ACCOUNTS_HPP
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Which pinned GitHub CLI account each profile uses, and whether this
// graph offers the CLI read at all (RFC-BPR-0015, BPR-1373).
namespace bpr::app {
	/// [[BPR-1373]]. Which BPR profile in this run uses which pinned gh
	/// account: one entry per profile runtime with an accepted session on a
	/// pinned GitHub CLI profile. Composition state, read for Accounts &
	/// hosts' "Used by"; it holds no credential.
	struct cli_account_use
	{
		std::string Profile;
		std::string Hostname;
		std::string Login;
	};

	class cli_account_usage final
	{
	public:
		void               set(const void* const aRuntime, std::optional<cli_account_use> aUse);
		[[nodiscard]] auto users(const std::string_view aHostname, const std::string_view aLogin) const -> std::vector<std::string>;

	private:
		mutable std::mutex                     Mutex;
		std::map<const void*, cli_account_use> Uses;
	};

	[[nodiscard]] auto cli_account_users() -> cli_account_usage&;

	// [[BPR-0797]]. Whether this run offers the GitHub CLI way in, rather
	// than a profile row labelled `(unavailable)` with nothing behind it.
	//
	// macOS, Windows and Linux are the platforms whose CLI read is enabled
	// (RFC-BPR-0015; Windows and Linux since [[BPR-1791]]), and the offer
	// used to be withheld from every tests-enabled build. That is
	// every build made with the CMake default, so a developer running their
	// own application was shown a dead row and told nothing about why. It is
	// withheld now only from a run inside the automated test boundary, where
	// an unattended suite must not be able to spend somebody's `gh` sign-in.
	//
	// Nothing this decides starts `gh`. `opted_in_cli_transport_factory`
	// holds a native factory and answers `supports_cli()`; the process is
	// reached only through `make_cli`, which the host calls after the
	// account row's explicit opt-in. A tests-enabled graph on a platform
	// without the CLI never asks it (cli_read_offered_here answers false
	// there), hence maybe_unused.
	[[nodiscard, maybe_unused]] auto cli_read_offered(
		[[maybe_unused]] const bool aInsideAutomatedTestBoundary) noexcept
		-> bool;
}
#endif
