#ifndef BPR_SRC_ADAPTERS_GIT_INCLUDE_BPR_ADAPTERS_GIT_INSTALLED_AUTHOR_GIT_HPP
#define BPR_SRC_ADAPTERS_GIT_INCLUDE_BPR_ADAPTERS_GIT_INSTALLED_AUTHOR_GIT_HPP

#include "bpr/application/author_git_port.hpp"

#include <chrono>
#include <filesystem>
#include <memory>

/// \file
/// \brief The author's own installed Git (RFC-BPR-0114 as amended 2026-11-12,
/// [[BPR-2065]]) and the worktree file Save writes (RFC-BPR-0113, [[BPR-2062]]).
/// \details Separate from `installed_git_process` in code, state and
/// configuration: no isolation root, no private home, the author's
/// configuration, hooks and credential helpers apply. Each launch is its own
/// session leader with a bounded descriptor sweep, a deadline and a stop
/// check; on either its whole process group is killed and reaped. Git's
/// terminal prompt is off and variables that would point Git at another
/// repository are removed. Qualified on macOS and Linux; on Windows every
/// call answers `unavailable` and the named commands remain.
namespace bpr::adapters::git {
	/// \brief The bounds each kind of call waits for.
	struct author_git_deadlines
	{
		std::chrono::milliseconds Read{ 30'000 };
		std::chrono::milliseconds Network{ 60'000 };
		/// Commit and push, which run hooks.
		std::chrono::milliseconds Hooks{ 300'000 };
	};

	[[nodiscard]] auto make_installed_author_git(std::filesystem::path aGitExecutable, author_git_deadlines aDeadlines = { }) -> std::unique_ptr<application::author_git_port>;

	/// \brief Parses `git status --porcelain=v2 -z --branch --untracked-files=all`.
	[[nodiscard]] auto parse_author_status(std::string_view aOutput) -> std::optional<core::local_worktree_status>;

	[[nodiscard]] auto make_worktree_files() -> std::unique_ptr<application::worktree_file_port>;
}

#endif
