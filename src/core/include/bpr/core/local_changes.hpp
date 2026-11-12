#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_LOCAL_CHANGES_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_LOCAL_CHANGES_HPP

#include "bpr/core/diff_compute.hpp"
#include "bpr/core/diff_model.hpp"
#include "bpr/core/git_objects.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief Author state's Local Changes (RFC-BPR-0113 and RFC-BPR-0114 as
/// amended by the owner on 2026-11-12): the worktree's staged and unstaged
/// files, the three comparisons, the changes of one file and the exact text a
/// Stage Change or Unstage Change writes to the index, the commit and push
/// decisions, the Edit view's editable-file and save rules, and the agent's
/// commit-message request.
/// \details Everything here is a decision over observations the host supplies
/// (Git's status, blob bytes, the hosted branch's tip). Nothing runs Git,
/// reads a file or talks to GitHub.
namespace bpr::core {
	// --- The worktree's status ---------------------------------------------------

	/// \brief One file's change as Git's status reports it.
	enum class local_file_status : std::uint8_t {
		added,
		modified,
		deleted,
		renamed,
		type_changed,
		/// A merge conflict: nothing commits while one remains.
		unmerged,
		/// Not tracked: listed under Unstaged as an added file.
		untracked,
	};
	[[nodiscard]] auto name(local_file_status aStatus) noexcept -> std::string_view;
	/// \brief The one-letter badge a row shows (A, M, D, R, T, U, A).
	[[nodiscard]] auto badge(local_file_status aStatus) noexcept -> char;

	struct local_status_entry
	{
		/// The path, relative to the worktree root, with forward slashes.
		std::string Path;
		/// The path before a rename; empty otherwise.
		std::string       Original;
		local_file_status Status = local_file_status::modified;
		/// Lines added and removed in this list's comparison, when counted.
		std::uint32_t Added   = 0U;
		std::uint32_t Removed = 0U;
		/// Git reported binary content; no lines are counted.
		bool Binary                                          = false;
		auto operator<=>(const local_status_entry&) const = default;
	};

	/// \brief The worktree as the author's Git reports it.
	struct local_worktree_status
	{
		/// The checked-out branch without `refs/heads/`; empty when detached.
		std::string Branch;
		/// The head commit; none on an unborn branch.
		std::optional<git_commit_oid> Head;
		/// The index against the head: what a commit takes.
		std::vector<local_status_entry> Staged;
		/// The worktree against the index, untracked files included.
		std::vector<local_status_entry> Unstaged;
		auto operator<=>(const local_worktree_status&) const = default;
	};
	/// \brief A path is unmerged in either list.
	[[nodiscard]] auto has_unmerged(const local_worktree_status& aStatus) noexcept -> bool;
	/// \brief The distinct paths with any uncommitted change, staged or not.
	[[nodiscard]] auto uncommitted_paths(const local_worktree_status& aStatus) -> std::vector<std::string>;
	/// \brief Sorts both lists by path and drops exact duplicates, so equal
	/// observations present equally.
	[[nodiscard]] auto normalized(local_worktree_status aStatus) -> local_worktree_status;

	// --- The three comparisons -------------------------------------------------

	/// \brief The left side of a comparison; the right side is always the files on disk.
	enum class local_comparison : std::uint8_t {
		/// Left: the last commit.
		uncommitted,
		/// Left: the pull request's head on GitHub, or before a pull request
		/// the branch on GitHub.
		not_pushed,
		/// Left: the target at the merge base.
		whole_pull_request,
	};
	[[nodiscard]] auto name(local_comparison aComparison) noexcept -> std::string_view;

	/// \brief The material key of one file in one comparison, from the bytes'
	/// own digests: equal bytes compare equally wherever they came from.
	[[nodiscard]] auto local_change_comparison(local_comparison aComparison, std::string_view aPath, std::optional<std::span<const std::uint8_t>> aLeft,
											   std::optional<std::span<const std::uint8_t>> aRight, const diff_options& aOptions) -> std::optional<material_comparison_key>;

	// --- Changes within a file and staging one of them ---------------------------

	/// \brief One change: a run of adjacent unequal alignment segments, as
	/// zero-based half-open line ranges on each side.
	struct local_change_hunk
	{
		std::uint32_t LeftBegin  = 0U;
		std::uint32_t LeftEnd    = 0U;
		std::uint32_t RightBegin = 0U;
		std::uint32_t RightEnd   = 0U;
		auto          operator<=>(const local_change_hunk&) const = default;
	};
	/// \brief The changes of a comparison, in order.
	[[nodiscard]] auto change_hunks(const computed_material_diff& aDiff) -> std::vector<local_change_hunk>;

	enum class local_stage_refusal : std::uint8_t {
		/// The change is not one of the comparison's changes (the file moved on).
		stale,
		/// A side is missing, binary or not text.
		not_text,
		/// The decoded lines do not rebuild the bytes exactly, or the changes
		/// do not rebuild the right side (line endings differ in an equal line):
		/// only the whole file can be staged.
		inexact,
	};
	[[nodiscard]] auto name(local_stage_refusal aRefusal) noexcept -> std::string_view;

	/// \brief Stage Change: the left side (the index) with exactly `aHunk`
	/// taken from the right side (the worktree).
	[[nodiscard]] auto apply_change(const computed_material_diff& aDiff, const local_change_hunk& aHunk) -> std::expected<std::vector<std::uint8_t>, local_stage_refusal>;
	/// \brief Unstage Change: the right side (the index) with exactly `aHunk`
	/// returned to the left side (the last commit).
	[[nodiscard]] auto revert_change(const computed_material_diff& aDiff, const local_change_hunk& aHunk) -> std::expected<std::vector<std::uint8_t>, local_stage_refusal>;

	/// \brief One change of the Uncommitted comparison (last commit against
	/// the worktree) and what its band offers: Stage Change acts on the
	/// index-to-worktree change it overlaps, Unstage Change on the
	/// commit-to-index change it overlaps.
	struct local_change_band
	{
		local_change_hunk Shown;
		/// Some of it is staged.
		bool Staged = false;
		/// Some of it is not staged.
		bool                             Unstaged = false;
		std::optional<local_change_hunk> Stage;
		std::optional<local_change_hunk> Unstage;
		auto                             operator<=>(const local_change_band&) const = default;
	};
	/// \brief The bands of a file's Uncommitted comparison from its three
	/// comparisons' changes. A file with nothing staged has no `Unstage`; a
	/// file wholly staged has no `Stage`.
	[[nodiscard]] auto uncommitted_bands(std::span<const local_change_hunk> aHeadToWorktree, std::span<const local_change_hunk> aHeadToIndex,
										 std::span<const local_change_hunk> aIndexToWorktree) -> std::vector<local_change_band>;

	/// \brief Added and removed lines of a comparison.
	struct local_line_counts
	{
		std::uint32_t Added                                  = 0U;
		std::uint32_t Removed                                = 0U;
		auto          operator<=>(const local_line_counts&) const = default;
	};
	[[nodiscard]] auto line_counts(const computed_material_diff& aDiff) -> local_line_counts;

	// --- Commit ----------------------------------------------------------------

	struct local_commit_request
	{
		std::string Subject;
		std::string Description;
		bool        Amend                                     = false;
		auto        operator<=>(const local_commit_request&) const = default;
	};

	enum class local_commit_refusal : std::uint8_t {
		nothing_staged,
		no_subject,
		detached,
		/// Amend needs a commit to amend.
		unborn,
		unmerged,
		/// The last commit is on GitHub: amending would need a forced push.
		amend_pushed,
	};
	[[nodiscard]] auto name(local_commit_refusal aRefusal) noexcept -> std::string_view;

	struct local_commit_plan
	{
		/// The message exactly as Git receives it.
		std::string   Message;
		bool          Amend = false;
		std::uint32_t Files = 0U;
		auto          operator<=>(const local_commit_plan&) const = default;
	};

	/// \brief The message Git records: the subject's one line, a blank line
	/// and the description, each trimmed, ending in one newline.
	[[nodiscard]] auto commit_message(std::string_view aSubject, std::string_view aDescription) -> std::string;
	/// \brief Commit N Files: offered when the request can run, with the exact
	/// message. `aHeadOnHost` says the last commit is reachable from GitHub.
	[[nodiscard]] auto decide_commit(const local_worktree_status& aStatus, const local_commit_request& aRequest, bool aHeadOnHost)
		-> std::expected<local_commit_plan, local_commit_refusal>;
	/// \brief Amend is offered: there is a last commit and GitHub does not have it.
	[[nodiscard]] auto amend_offered(const local_worktree_status& aStatus, bool aHeadOnHost) noexcept -> bool;

	// --- Push --------------------------------------------------------------------

	/// \brief What GitHub has for the branch, observed by Check Again.
	struct local_hosted_branch
	{
		/// GitHub answered (signed in or through the author's Git).
		bool Observed = false;
		/// The hosted branch's tip; none when the branch is not on GitHub.
		std::optional<git_commit_oid> Tip;
		/// The tip is among the worktree's objects, so it can be counted against.
		bool TipKnown = false;
		/// Commits the branch has that the tip does not, and the reverse.
		std::uint32_t Ahead  = 0U;
		std::uint32_t Behind = 0U;
		/// The pull request the branch belongs to, when one exists.
		std::optional<std::uint64_t> PullRequest;
		auto                         operator<=>(const local_hosted_branch&) const = default;
	};

	enum class local_push_state : std::uint8_t {
		/// GitHub was not read yet, or could not be.
		unobserved,
		/// The branch is not on GitHub: Push creates it.
		missing,
		ahead,
		up_to_date,
		/// GitHub has commits the branch does not: refused before anything runs.
		behind,
		/// Both have commits the other lacks: refused before anything runs.
		diverged,
		/// GitHub's tip is not among the worktree's objects: refused, it has
		/// commits the branch does not.
		unknown_tip,
	};
	[[nodiscard]] auto name(local_push_state aState) noexcept -> std::string_view;

	/// \brief The push exactly as it runs: the branch to the hosted
	/// repository's URL, never forced, no tags, no upstream written.
	struct local_push_command
	{
		/// The worktree it runs in.
		std::string Directory;
		std::string Url;
		std::string Branch;
		/// Git's arguments after `git`, in order.
		std::vector<std::string> Arguments;
		/// The command as the push line shows it.
		std::string Text;
		auto        operator<=>(const local_push_command&) const = default;
	};
	[[nodiscard]] auto make_push_command(std::string_view aDirectory, std::string_view aUrl, std::string_view aBranch) -> std::optional<local_push_command>;

	struct local_push_decision
	{
		local_push_state State = local_push_state::unobserved;
		/// Push can be pressed.
		bool Offered = false;
		/// Push is the window's next step (blue): nothing is uncommitted.
		bool Default                                         = false;
		auto operator<=>(const local_push_decision&) const = default;
	};
	[[nodiscard]] auto decide_push(const local_worktree_status& aStatus, const local_hosted_branch& aHosted) -> local_push_decision;
	/// \brief The last commit is on GitHub (Amend's rule): the tip is the head
	/// or descends from it, as the counts say.
	[[nodiscard]] auto head_on_host(const local_worktree_status& aStatus, const local_hosted_branch& aHosted) noexcept -> bool;

	// --- Edit and Save (RFC-BPR-0113) ----------------------------------------------

	/// \brief The editing model's bound: a larger file is refused, never truncated.
	inline constexpr std::uint64_t maximum_editable_bytes = 1024U * 1024U;

	struct local_edit_facts
	{
		bool          Exists    = false;
		bool          Regular   = false;
		bool          Link      = false;
		bool          Submodule = false;
		bool          Inside    = false;
		bool          Filtered  = false;
		std::uint64_t Size      = 0U;
	};
	enum class local_edit_refusal : std::uint8_t {
		deleted,
		not_a_file,
		link,
		submodule,
		outside,
		too_large,
		not_utf8,
		nul_byte,
		filtered,
	};
	[[nodiscard]] auto name(local_edit_refusal aRefusal) noexcept -> std::string_view;
	/// \brief Why a file cannot be edited, or nothing. `aBytes` is the file's
	/// content when it was read (it may be empty when the facts already refuse).
	[[nodiscard]] auto decide_editable(const local_edit_facts& aFacts, std::span<const std::uint8_t> aBytes) -> std::optional<local_edit_refusal>;

	enum class local_save_decision : std::uint8_t {
		/// The disk holds what the editor read or last wrote: write.
		write,
		/// The disk changed since: write nothing; offer Reload.
		changed_on_disk,
		/// The editor holds the disk's bytes: nothing to write.
		unchanged,
	};
	[[nodiscard]] auto name(local_save_decision aDecision) noexcept -> std::string_view;
	[[nodiscard]] auto decide_save(std::span<const std::uint8_t> aRead, std::span<const std::uint8_t> aDisk, std::span<const std::uint8_t> aEdited) -> local_save_decision;

	/// \brief A gutter mark of the Edit view, on a line of the edited text.
	enum class local_line_mark_kind : std::uint8_t {
		added,
		changed,
		/// Lines went before this line (on the last line + 1 when at the end).
		removed_before,
	};
	struct local_line_mark
	{
		/// Zero-based line of the right side.
		std::uint32_t        Line = 0U;
		local_line_mark_kind Kind = local_line_mark_kind::added;
		/// The left side's lines a click shows, half-open.
		std::uint32_t LeftBegin                           = 0U;
		std::uint32_t LeftEnd                             = 0U;
		auto          operator<=>(const local_line_mark&) const = default;
	};
	[[nodiscard]] auto line_marks(const computed_material_diff& aDiff) -> std::vector<local_line_mark>;

	// --- The agent fills the commit ---------------------------------------------

	/// \brief The prompt purpose of the agent's commit message.
	inline constexpr std::string_view commit_message_purpose = "local.commit-message";
	/// \brief The staged diff's share of a request; more is cut with a note.
	inline constexpr std::size_t maximum_commit_message_diff_bytes = 96U * 1024U;
	inline constexpr std::size_t maximum_commit_message_subjects   = 10U;

	struct commit_message_request
	{
		/// The purpose's effective instructions.
		std::string Instructions;
		std::string Branch;
		/// `git diff --cached` as text.
		std::string StagedDiff;
		/// The branch's recent commit subjects, newest first.
		std::vector<std::string> RecentSubjects;
	};
	/// \brief The one-shot prompt: instructions, the subjects for style and the
	/// staged diff, bounded, with an exact note of what was cut.
	[[nodiscard]] auto compose_commit_message_prompt(const commit_message_request& aRequest) -> std::string;

	struct commit_message_suggestion
	{
		std::string Subject;
		std::string Description;
		auto        operator<=>(const commit_message_suggestion&) const = default;
	};
	/// \brief Reads an answer: an optional fenced block is unwrapped, a
	/// `Subject:` label dropped; the first non-empty line is the subject and
	/// the text after the next blank line the description. None when empty.
	[[nodiscard]] auto read_commit_message_answer(std::string_view aAnswer) -> std::optional<commit_message_suggestion>;
}

#endif
