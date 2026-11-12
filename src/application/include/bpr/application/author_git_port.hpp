#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AUTHOR_GIT_PORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AUTHOR_GIT_PORT_HPP

#include "bpr/core/local_changes.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

/// \file
/// \brief The author's own Git, in the author's own worktree (RFC-BPR-0114 as
/// amended 2026-11-12, [[BPR-2065]]), and the one named file Save writes
/// (RFC-BPR-0113, [[BPR-2062]]).
/// \details A second Git runner, separate in code, state and configuration
/// from the isolated reader: it runs the Git executable BPR resolved, in the
/// worktree, with the application's environment, the author's global and
/// system configuration, the repository's configuration and hooks and their
/// credential helpers. Git's terminal prompt is off. Each call is bounded and
/// stoppable. Its catalogue is exactly the operations below: reads for Local
/// Changes, staging and unstaging paths or one change, a commit (with or
/// without amend) and a push of the draft's branch. There is no force,
/// delete, tag, reset of the worktree, stash or checkout.
namespace bpr::application {
	enum class author_git_failure : std::uint8_t {
		/// The runner is not qualified on this platform, or Git cannot be run.
		unavailable,
		/// Git did not start.
		start,
		/// The bounded wait passed; the child was killed.
		timeout,
		cancelled,
		/// Output past its bound.
		bounds,
		/// Git ran and failed; the output says why.
		failed,
		/// Git's answer could not be read.
		malformed,
	};
	[[nodiscard]] inline auto name(const author_git_failure aFailure) noexcept -> std::string_view
	{
		switch (aFailure) {
		case author_git_failure::unavailable: return "unavailable";
		case author_git_failure::start:       return "start";
		case author_git_failure::timeout:     return "timeout";
		case author_git_failure::cancelled:   return "cancelled";
		case author_git_failure::bounds:      return "bounds";
		case author_git_failure::failed:      return "failed";
		case author_git_failure::malformed:   return "malformed";
		}
		return "failed";
	}

	/// \brief A failure with what Git and any hook said, bounded.
	struct author_git_error
	{
		author_git_failure Failure = author_git_failure::failed;
		int                ExitCode = -1;
		/// Standard error then standard output, at most `maximum_author_git_message_bytes`.
		std::string Message;
		auto        operator<=>(const author_git_error&) const = default;
	};
	/// \brief The bound on Git's and a hook's output shown after a failure.
	inline constexpr std::size_t maximum_author_git_message_bytes = 16U * 1024U;

	template <typename T>
	using author_git_result = std::expected<T, author_git_error>;

	/// \brief One side of a comparison to read: a commit's file or the index's.
	struct author_git_blob
	{
		/// A commit, or empty for the index.
		std::optional<core::git_commit_oid> Commit;
		std::string                         Path;
	};

	/// \brief One path the comparison against a commit lists.
	struct author_git_changed_path
	{
		std::string             Path;
		std::string             Original;
		core::local_file_status Status = core::local_file_status::modified;
	};

	class author_git_port
	{
	public:
		virtual ~author_git_port() = default;

		// Reads: no optional locks and no fsmonitor; no hook runs.
		virtual auto status(const std::string& aWorktree, std::stop_token aStop) -> author_git_result<core::local_worktree_status> = 0;
		/// \brief A blob's exact bytes, without filters; none when the path is
		/// not in that commit or the index. Refused as bounds past `aMaximum`.
		virtual auto read_blob(const std::string& aWorktree, const author_git_blob& aBlob, std::uint64_t aMaximum, std::stop_token aStop)
			-> author_git_result<std::optional<std::vector<std::uint8_t>>> = 0;
		/// \brief The paths that differ between a commit and the worktree.
		virtual auto changed_since(const std::string& aWorktree, const core::git_commit_oid& aCommit, std::stop_token aStop) -> author_git_result<std::vector<author_git_changed_path>> = 0;
		/// \brief The commit is among the worktree's objects.
		virtual auto has_commit(const std::string& aWorktree, const core::git_commit_oid& aCommit, std::stop_token aStop) -> author_git_result<bool> = 0;
		/// \brief Commits reachable from `aFrom` and not from `aExclude`.
		virtual auto count_commits(const std::string& aWorktree, const core::git_commit_oid& aFrom, const core::git_commit_oid& aExclude, std::stop_token aStop)
			-> author_git_result<std::uint32_t> = 0;
		/// \brief The branch's latest commit subjects, newest first.
		virtual auto recent_subjects(const std::string& aWorktree, std::size_t aCount, std::stop_token aStop) -> author_git_result<std::vector<std::string>> = 0;
		/// \brief `git diff --cached` as text, without colour, external diff or textconv.
		virtual auto staged_diff(const std::string& aWorktree, std::uint64_t aMaximum, std::stop_token aStop) -> author_git_result<std::string> = 0;
		/// \brief The path has a `filter` attribute, so its bytes on disk are not Git's.
		virtual auto filtered(const std::string& aWorktree, const std::string& aPath, std::stop_token aStop) -> author_git_result<bool> = 0;
		/// \brief The hosted branch's tip through the author's Git and credentials
		/// (`ls-remote`); none when the branch is not there.
		virtual auto hosted_tip(const std::string& aWorktree, const std::string& aUrl, const std::string& aBranch, std::stop_token aStop)
			-> author_git_result<std::optional<core::git_commit_oid>> = 0;

		// Writes, each on the author's press.
		virtual auto stage_paths(const std::string& aWorktree, const std::vector<std::string>& aPaths, std::stop_token aStop) -> author_git_result<void>   = 0;
		virtual auto unstage_paths(const std::string& aWorktree, const std::vector<std::string>& aPaths, std::stop_token aStop) -> author_git_result<void> = 0;
		/// \brief Writes one index entry with exactly these bytes, keeping the
		/// entry's mode (a new entry is a regular file). Stage Change and
		/// Unstage Change.
		virtual auto write_index_entry(const std::string& aWorktree, const std::string& aPath, const std::vector<std::uint8_t>& aBytes, std::stop_token aStop)
			-> author_git_result<void> = 0;
		/// \brief Commits the index with this message; hooks and signing apply.
		virtual auto commit(const std::string& aWorktree, const core::local_commit_plan& aPlan, std::stop_token aStop) -> author_git_result<void> = 0;
		/// \brief A remote's configured URL in a clone (`git config --get`), or none.
		virtual auto remote_url(const std::string& aClone, const std::string& aRemote, std::stop_token aStop) -> author_git_result<std::optional<std::string>> = 0;
		/// \brief Runs exactly `aCommand`'s arguments in its directory.
		virtual auto push(const core::local_push_command& aCommand, std::stop_token aStop) -> author_git_result<void> = 0;
	};

	enum class worktree_file_failure : std::uint8_t {
		outside,
		link,
		not_a_file,
		missing,
		too_large,
		io,
		/// The file changed between reading and writing.
		changed,
	};

	/// \brief One file beneath a worktree root, by its root-relative path.
	struct worktree_file
	{
		bool                      Exists  = false;
		bool                      Regular = false;
		bool                      Link    = false;
		bool                      Inside  = false;
		std::vector<std::uint8_t> Bytes;
	};

	/// \brief RFC-BPR-0113's narrow write port: reads and writes one named
	/// regular file beneath a worktree root, follows no link, refuses any
	/// other path, and writes completely or not at all.
	class worktree_file_port
	{
	public:
		virtual ~worktree_file_port() = default;

		virtual auto read(const std::string& aRoot, const std::string& aPath, std::uint64_t aMaximum) -> std::expected<worktree_file, worktree_file_failure> = 0;
		/// \brief Writes `aBytes` only when the file still holds `aExpected`; keeps its mode.
		virtual auto write(const std::string& aRoot, const std::string& aPath, const std::vector<std::uint8_t>& aExpected, const std::vector<std::uint8_t>& aBytes)
			-> std::expected<void, worktree_file_failure> = 0;
	};
}

#endif
