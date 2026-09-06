#ifndef BPR_SRC_ADAPTERS_GIT_INCLUDE_BPR_ADAPTERS_GIT_AGENT_WORKSPACE_OWNERSHIP_HPP
#define BPR_SRC_ADAPTERS_GIT_INCLUDE_BPR_ADAPTERS_GIT_AGENT_WORKSPACE_OWNERSHIP_HPP

#include "bpr/application/agent_launch.hpp"
#include "bpr/application/agent_workspace.hpp"
#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
#include <stop_token>
#include <string>
#include <string_view>

namespace bpr::adapters::git {
#if defined(BPR_WORKSPACE_OWNERSHIP_FAULT_INJECTION)
	/// Test executable supplies checkpoints; absent from production builds.
	void installed_workspace_checkpoint(std::string_view aName);
#endif

	enum class workspace_ownership_failure { unavailable,
											 repair_required,
											 capacity,
											 cancelled };
	struct workspace_recovery_statistics
	{
		std::size_t Recovered = 0U;
		std::size_t Live      = 0U;
	};
	struct installed_agent_workspace_layout
	{
		std::filesystem::path                           Parent;
		std::filesystem::path                           Workspace;
		application::agent_workspace_directory_identity ParentIdentity;
		application::agent_workspace_directory_identity WorkspaceIdentity;
		application::agent_workspace_lifetime_guard     Guard;
		std::string                                     OwnershipMarker;
	};

	/// \brief Effect-lane ownership of one empty directory before Git materialization.
	/// Destruction releases the lock; it never silently deletes workspace data.
	class installed_agent_workspace_ownership
	{
	public:
		virtual ~installed_agent_workspace_ownership()                                                = default;
		[[nodiscard]] virtual auto layout() const noexcept -> const installed_agent_workspace_layout& = 0;
		/// \brief Requires the expected Git ownership marker before allowing agent launch.
		[[nodiscard]] virtual auto mark_ready() -> bool = 0;
		/// \brief Exclusive, journaled cleanup; false preserves unresolved ownership.
		[[nodiscard]] virtual auto cleanup() -> bool = 0;
	};

	/// \brief ParentDirectory selects the canonical private neighbourhood, outside cache.
	[[nodiscard]] auto prepare_installed_agent_workspace_ownership(
		const std::filesystem::path&                                aRegistry,
		const application::agent_workspace_materialization_request& aRequest,
		std::stop_token                                             aStop = { })
		-> std::expected<std::unique_ptr<installed_agent_workspace_ownership>, workspace_ownership_failure>;

	/// \brief Reclaims exclusively locked records with validated original identity.
	[[nodiscard]] auto recover_installed_agent_workspaces(
		const std::filesystem::path& aRegistry,
		const std::filesystem::path& aNeighbourhood,
		std::stop_token              aStop = { })
		-> std::expected<workspace_recovery_statistics, workspace_ownership_failure>;
}
#endif
