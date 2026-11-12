#ifndef BPR_SRC_APP_SRC_LOCAL_DRAFT_HANDOFF_HPP
#define BPR_SRC_APP_SRC_LOCAL_DRAFT_HANDOFF_HPP

#include "bpr/core/local_draft.hpp"

#include <QJsonObject>

#include <filesystem>
#include <optional>
#include <string>

namespace bpr::app {
	/// \brief What `bpr draft open` did, as its result document says it.
	struct local_draft_handoff
	{
		bool                                Ok = false;
		QJsonObject                         Result;
		std::optional<core::local_draft_id> Draft;
	};

	/// \brief RFC-BPR-0108 AC05. Opens or resumes the local draft of one
	/// worktree on this profile's store, exactly as Prepare Pull Request…
	/// does (`application::open_local_draft`), and describes the result. Run
	/// by the terminal that holds the profile, or by the desktop that owns it.
	/// Blocking; nothing is published.
	[[nodiscard]] auto open_local_draft_here(const std::filesystem::path& aRoot, const std::filesystem::path& aGitExecutable, const std::optional<std::filesystem::path>& aWindowsSystemRoot,
											 const std::string& aLocation, const std::optional<std::string>& aContext) -> local_draft_handoff;
}

#endif
