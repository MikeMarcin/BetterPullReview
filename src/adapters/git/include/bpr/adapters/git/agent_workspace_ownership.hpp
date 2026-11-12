#ifndef BPR_SRC_ADAPTERS_GIT_INCLUDE_BPR_ADAPTERS_GIT_AGENT_WORKSPACE_OWNERSHIP_HPP
#define BPR_SRC_ADAPTERS_GIT_INCLUDE_BPR_ADAPTERS_GIT_AGENT_WORKSPACE_OWNERSHIP_HPP

#include "bpr/application/agent_launch.hpp"
#include "bpr/application/agent_workspace.hpp"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::adapters::git {
#if defined(BPR_WORKSPACE_OWNERSHIP_FAULT_INJECTION)
	/// Test executable supplies checkpoints; absent from production builds.
	void installed_workspace_checkpoint(std::string_view aName);
#endif

	enum class workspace_ownership_failure { unavailable,
											 repair_required,
											 capacity,
											 cancelled };
	/// \brief [[BPR-1857]]. Which check refused disposable workspaces.
	///
	/// \details The failure code alone said "needs repair or has reached its
	/// limit" for a dozen different findings, and named no folder, so nobody
	/// could act on it. The check and the folder say what to look at; nothing
	/// here weakens what is refused.
	enum class workspace_refusal_check : std::uint8_t {
		none,
		/// The registry or neighbourhood path is not absolute, normal or bounded.
		invalid_path,
		/// The folder that holds the neighbourhood is not owned by this user,
		/// or another account may write to it.
		neighbourhood_parent_shared,
		/// The neighbourhood exists but is not private to this user.
		neighbourhood_not_private,
		/// The neighbourhood could not be created.
		neighbourhood_not_created,
		/// The application data folder is not private to this user.
		data_folder_not_private,
		/// The ownership registry exists but is not private, or could not be made.
		registry_not_private,
		/// The registry holds an entry that is not a workspace record.
		registry_unexpected_entry,
		/// The neighbourhood holds a workspace no record accounts for.
		unrecorded_leftover,
		/// A record is not a private file of this user, or cannot be opened.
		record_not_private,
		/// [[BPR-2118]]. A record's line does not parse, its phases do not
		/// follow one another, or it names another record's token.
		record_malformed,
		/// [[BPR-2118]]. A folder or file a record names is not the one at its
		/// path: gone, replaced, or the same inode on another volume.
		record_moved,
		/// [[BPR-2118]]. A record was read, but it could not be rewritten in
		/// place, or its workspace could not be cleaned up exactly.
		record_cleanup_failed,
		/// More records or neighbourhood entries than the fixed limit.
		limit_reached,
		/// The registry's process lock or a record's lock could not be taken.
		lock_unavailable,
	};
	/// \brief [[BPR-2118]]. What `record_moved` found at a recorded path.
	enum class workspace_record_finding : std::uint8_t {
		none,
		/// Nothing of the recorded kind is there, or it is not this user's.
		missing,
		/// The same inode, named through a different volume.
		other_volume,
		/// A different folder or file.
		replaced,
	};
	/// \brief [[BPR-1857]]. The check that refused, the folder or file it
	/// refused, and, where the platform can say, the account involved.
	struct workspace_recovery_refusal
	{
		workspace_refusal_check Check = workspace_refusal_check::none;
		std::filesystem::path   Folder;
		/// An account name or SID the refusal is about; empty when none.
		std::string Account;
		/// [[BPR-2118]]. Every record is checked before recovery refuses.
		/// `Check`, `Folder` and `Finding` describe the first record refused;
		/// this counts all of them. Zero for a refusal that is not about a
		/// record.
		std::size_t Count = 0U;
		/// [[BPR-2118]]. For `record_moved`, what was found at the path.
		workspace_record_finding Finding = workspace_record_finding::none;
		/// [[BPR-1870]]. Checks that found one of the application's own
		/// workspace folders or records wider than private and narrowed it in
		/// place instead of refusing (this user's, a real folder or
		/// single-link file, reached through no link). Filled whether or not
		/// recovery then refused for another reason.
		std::vector<workspace_refusal_check> Repaired;
	};
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
		[[nodiscard]] virtual auto mark_ready(bool aRetained = false) -> bool = 0;
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
	/// \details [[BPR-1857]]. On refusal, \p aRefusal (when given) names the
	/// check and the folder that refused.
	[[nodiscard]] auto recover_installed_agent_workspaces(
		const std::filesystem::path& aRegistry,
		const std::filesystem::path& aNeighbourhood,
		std::stop_token              aStop    = { },
		workspace_recovery_refusal*  aRefusal = nullptr)
		-> std::expected<workspace_recovery_statistics, workspace_ownership_failure>;

	/// \brief [[BPR-1857]]. The stable name of \p aCheck, for terminal lines
	/// and logs.
	[[nodiscard]] auto workspace_refusal_check_name(workspace_refusal_check aCheck) noexcept -> std::string_view;
}
#endif
