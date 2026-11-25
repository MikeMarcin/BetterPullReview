#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_SESSION_MANAGEMENT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_SESSION_MANAGEMENT_HPP

#include "bpr/application/agent_workspace_expiry.hpp"
#include "bpr/application/native_agent_session.hpp"

#include <cstdint>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-NNNN]]. Renaming and deleting one of BetterPullReview's agent
/// sessions, and clearing them all, as blocking effects for an effect worker.
/// \details Core decides (`bpr/core/agent_session_management.hpp`); these
/// observe, take the turn leases and perform. Only BetterPullReview's own
/// records and the retained workspaces it made are touched: the agent's own
/// native session files (Claude Code's or Codex's transcripts under their
/// home folders) are never read, moved or deleted.
namespace bpr::application {
	/// \brief How a rename or deletion ended.
	enum class agent_session_edit_result : std::uint8_t {
		done,
		/// No such session in the scope; nothing changed.
		not_found,
		/// A turn is running on it or on its workspace; nothing changed.
		running,
		/// The name is longer than the bound; nothing changed.
		name_too_long,
		/// The name is not text; nothing changed.
		name_not_text,
		/// Its workspace's ownership could not be verified or its removal
		/// failed; the session and its workspace are left as they were.
		workspace_refused,
		/// The catalog could not be read or written; nothing changed.
		unavailable,
	};

	/// \brief What one rename or deletion did.
	struct agent_session_edit_receipt
	{
		agent_session_edit_result Result = agent_session_edit_result::unavailable;
		/// The deleted session's retained workspace was removed.
		bool WorkspaceRemoved = false;
		/// The deleted session's workspace stays: another session uses it.
		bool WorkspaceShared                                             = false;
		auto operator==(const agent_session_edit_receipt&) const -> bool = default;
	};

	/// \brief Names the session \p aId in \p aScope with what was typed, tidied
	/// by core; empty restores its default title.
	[[nodiscard]] auto rename_agent_session(native_agent_session_store_port& aStore, const std::string& aScope, const std::string& aId, std::string_view aTyped)
		-> agent_session_edit_receipt;

	/// \brief Deletes the session \p aId in \p aScope.
	/// \details Refuses, changing nothing, while \p aTurnActive (a turn of the
	/// asking window is running) or while any turn holds a lease of its
	/// workspace (its path, a key that leads to it, or one of \p aLeaseKeys,
	/// the asking window's own pull request key). When no other session uses
	/// its retained workspace, removes that workspace through \p aFiles
	/// first; if that fails nothing is forgotten. Then forgets the session.
	[[nodiscard]] auto delete_agent_session(native_agent_session_store_port& aStore, retained_workspace_files_port* aFiles, const std::string& aScope, const std::string& aId,
											const std::vector<std::string>& aLeaseKeys, bool aTurnActive, std::stop_token aStop) -> agent_session_edit_receipt;

	/// \brief What clearing every session did.
	struct agent_sessions_clear_report
	{
		/// Every session was forgotten.
		bool Cleared = false;
		/// The retained workspaces removed, and those left.
		retained_workspace_summary Workspaces;
	};

	/// \brief The Clear list's Agent sessions and workspaces: removes every
	/// retained workspace not in use, then, only when none is left, forgets
	/// every session. With one left, the sessions stay, so the workspace keeps
	/// its record and a later clear or the expiry policy can remove it.
	[[nodiscard]] auto clear_agent_sessions(native_agent_session_store_port& aStore, retained_workspace_files_port* aFiles, std::stop_token aStop) -> agent_sessions_clear_report;
}

#endif
