#ifndef BPR_SRC_ADAPTERS_GIT_INCLUDE_BPR_ADAPTERS_GIT_INSTALLED_GIT_PROCESS_HPP
#define BPR_SRC_ADAPTERS_GIT_INCLUDE_BPR_ADAPTERS_GIT_INSTALLED_GIT_PROCESS_HPP

#include "bpr/application/git_process_port.hpp"

#include "bpr/application/insights_observer.hpp"
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <stop_token>

namespace bpr::adapters::git {
	/// \brief Exact owner capability for the disposable agent-workspace root.
	struct installed_agent_workspace_parent
	{
		std::filesystem::path                           Path;
		application::agent_workspace_directory_identity DirectoryIdentity;

		auto operator<=>(const installed_agent_workspace_parent&) const = default;
	};

	/// \brief Captures an owner-private workspace root before children exist.
	[[nodiscard]] auto capture_installed_agent_workspace_parent(
		const std::filesystem::path& aPath) noexcept
		-> std::optional<installed_agent_workspace_parent>;

	/// \brief Removes only the captured, still-empty workspace root.
	///
	/// A false result retains the capability, including a quarantine path when
	/// an exact directory moved but could not be removed, for explicit recovery.
	[[nodiscard]] auto remove_installed_agent_workspace_parent(
		installed_agent_workspace_parent& aParent) noexcept -> bool;

	/// \brief [[BPR-1842]]. The installed Git executables this process has
	/// already admitted: resolved through their own exec path, version checked
	/// and capability probed, each against one isolation root.
	///
	/// \details Admission launches about a dozen Git processes and took about
	/// a second on the owner's Mac, on every pull request opened. An owner that
	/// opens many processes over one configuration (the review cache) holds one
	/// of these, so the second and later processes skip the launches while the
	/// executable, the Git it resolved and the isolation root are the exact
	/// files they were: same canonical path, size and modification time. Any
	/// difference admits again from nothing. Nothing is remembered across runs.
	class installed_git_admissions;
	[[nodiscard]] auto make_installed_git_admissions()
		-> std::shared_ptr<installed_git_admissions>;

	/// \brief Describes the installed git process configuration record.
	struct installed_git_process_configuration
	{
		std::filesystem::path                GitExecutable;
		std::filesystem::path                IsolationRoot;
		std::optional<std::filesystem::path> WindowsSystemRoot;
		/// Opt-in durable workspace ownership; absent for legacy/native callers.
		std::optional<std::filesystem::path>            WorkspaceOwnershipRegistry;
		std::shared_ptr<application::insights_observer> Insights;
		/// [[BPR-1842]]. Admissions to reuse and record; none admits every
		/// process from nothing.
		std::shared_ptr<installed_git_admissions> Admissions;
	};

	/// \brief Creates the installed git process.
	[[nodiscard]] auto make_installed_git_process(
		const installed_git_process_configuration& aConfiguration,
		std::stop_token                            aStop = { })
		-> std::expected<
			std::unique_ptr<application::git_process_port>,
			application::git_process_error>;
}

#endif // BPR_SRC_ADAPTERS_GIT_INCLUDE_BPR_ADAPTERS_GIT_INSTALLED_GIT_PROCESS_HPP
