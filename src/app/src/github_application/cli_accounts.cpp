#include "github_application/cli_accounts.hpp"

#include "github_cli_runtime.hpp"

#include <algorithm>
#include <utility>

namespace bpr::app {
	void cli_account_usage::set(const void* const aRuntime, std::optional<cli_account_use> aUse)
	{
		const std::lock_guard lock{ Mutex };
		if (aUse) {
			Uses.insert_or_assign(aRuntime, std::move(*aUse));
		} else {
			Uses.erase(aRuntime);
		}
	}

	auto cli_account_usage::users(const std::string_view aHostname, const std::string_view aLogin) const -> std::vector<std::string>
	{
		const std::lock_guard lock{ Mutex };
		auto                  result = std::vector<std::string>{ };
		for (const auto& [runtime, use] : Uses) {
			if (use.Hostname == aHostname && use.Login == aLogin && std::ranges::find(result, use.Profile) == result.end()) {
				result.push_back(use.Profile);
			}
		}
		std::ranges::sort(result);
		return result;
	}

	auto cli_account_users() -> cli_account_usage&
	{
		static cli_account_usage usage;
		return usage;
	}

	auto cli_read_offered(
		[[maybe_unused]] const bool aInsideAutomatedTestBoundary) noexcept
		-> bool
	{
#if defined(BPR_GITHUB_CLI_PLATFORM)
	#if defined(BPR_BUILD_TESTING)
		return !aInsideAutomatedTestBoundary;
	#else
		return true;
	#endif
#else
		return false;
#endif
	}
}
