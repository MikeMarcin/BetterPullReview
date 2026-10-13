#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_GIT_METADATA_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_GIT_METADATA_HPP

#include "bpr/application/workspace_source_port.hpp"
#include "bpr/core/review_cycle_git.hpp"

#include <expected>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>

/// \file
/// \brief A Git root's config and references read as data through the
/// workspace port, shared by capture and outgoing resolution (RFC-BPR-0054,
/// RFC-BPR-0083). Git is never run.
namespace bpr::application {
	/// \brief Why a Git root's metadata was not read: the port's failure, and
	/// for a repository the readers refuse, the named reason.
	struct git_metadata_failure
	{
		workspace_failure                           Workspace = workspace_failure::unsupported;
		std::optional<core::git_repository_problem> Repository;
	};

	/// \brief Reads the shared `config`, and the worktree's `config.worktree`
	/// when `extensions.worktreeConfig` is set. A missing file reads as empty.
	[[nodiscard]] auto read_git_repository_format(workspace_root_session& aSession, std::stop_token aStop)
		-> std::expected<core::git_repository_format, git_metadata_failure>;

	/// \brief What `HEAD` names and the commit it resolves to.
	struct git_head_state
	{
		/// HEAD's own symbolic target, recorded as provenance.
		std::optional<std::string> Reference;
		/// Absent for an unborn branch or a reference that names no commit.
		std::optional<core::git_commit_oid> Commit;
	};

	/// \brief Reads `HEAD` and resolves it like `resolve_reference`. A read
	/// failure other than `missing` is reported, never replaced by an older
	/// `packed-refs` entry.
	[[nodiscard]] auto read_git_head(workspace_root_session& aSession, core::git_hash_algorithm aAlgorithm, std::stop_token aStop)
		-> std::expected<git_head_state, workspace_failure>;

	/// \brief Resolves a reference name to a commit, reading only the
	/// checkout's `HEAD`, loose references and `packed-refs` as data.
	/// \details `HEAD` and names under `refs/` are followed through symbolic
	/// references as deep as Git follows them (five reads in all).
	/// `packed-refs` is consulted only when the loose file is missing. Nothing
	/// (with no failure) means the name resolves to no commit.
	[[nodiscard]] auto resolve_reference(workspace_root_session& aSession, std::string_view aName, core::git_hash_algorithm aAlgorithm, std::stop_token aStop)
		-> std::expected<core::git_commit_oid, std::optional<workspace_failure>>;
}

#endif
