#ifndef BPR_SRC_ADAPTERS_GIT_INCLUDE_BPR_ADAPTERS_GIT_INSTALLED_GIT_CACHE_HPP
#define BPR_SRC_ADAPTERS_GIT_INCLUDE_BPR_ADAPTERS_GIT_INSTALLED_GIT_CACHE_HPP

#include "bpr/application/git_cache_port.hpp"
#include "bpr/application/insights_projection.hpp"

#include <expected>
#include <filesystem>
#include <memory>
#include <optional>

namespace bpr::adapters::git {
	/// \brief Describes the installed git cache configuration record.
	struct installed_git_cache_configuration
	{
		std::filesystem::path                GitExecutable;
		std::filesystem::path                StorageRoot;
		std::optional<std::filesystem::path> WindowsSystemRoot;
		/// Opt-in durable workspace ownership; absent for legacy/native callers.
		std::optional<std::filesystem::path> WorkspaceOwnershipRegistry;
	};

	/// \brief File-byte view of the existing cache inventory; reads only, and never opens a Git process.
	[[nodiscard]] auto measure_installed_git_storage(const std::filesystem::path& aRoot) -> std::vector<application::insights_storage_row>;

	/// \brief Creates the installed git cache.
	[[nodiscard]] auto make_installed_git_cache(
		const installed_git_cache_configuration& aConfiguration)
		-> std::expected<
			std::unique_ptr<application::git_cache_port>,
			application::git_cache_error>;
}

#endif // BPR_SRC_ADAPTERS_GIT_INCLUDE_BPR_ADAPTERS_GIT_INSTALLED_GIT_CACHE_HPP
