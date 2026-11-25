#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_SESSION_MANAGEMENT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_SESSION_MANAGEMENT_HPP

#include "bpr/application/agent_session_edit.hpp"
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

	/// \brief [[BPR-NNNN]]. Delete all sessions…: deletes each of \p aIds in
	/// \p aScope as `delete_agent_session` does, newest first. One that is
	/// running or whose workspace could not be removed safely is kept and
	/// counted; the rest go. `done` when any went or none was left to go.
	[[nodiscard]] auto delete_agent_sessions(native_agent_session_store_port& aStore, retained_workspace_files_port* aFiles, const std::string& aScope, const std::vector<std::string>& aIds,
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
