#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_SESSION_EDIT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_SESSION_EDIT_HPP

#include <cstdint>

/// \file
/// \brief [[BPR-NNNN]]. What renaming or deleting an agent session did: the
/// plain answer the host gives and the session list presents. No port, path
/// or handle.
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
		bool WorkspaceShared = false;
		/// [[BPR-NNNN]] Delete all: how many went, and how many stayed
		/// (running, or a workspace that could not be removed safely).
		std::uint32_t Deleted                                                     = 0U;
		std::uint32_t Kept                                                        = 0U;
		auto          operator==(const agent_session_edit_receipt&) const -> bool = default;
	};
}

#endif
