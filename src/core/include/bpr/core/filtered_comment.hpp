#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_FILTERED_COMMENT_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_FILTERED_COMMENT_HPP

#include "bpr/core/diff_model.hpp"
#include "bpr/core/epoch_model.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

/// RFC-BPR-0089 M4. Comments and thread markers on a commit-filtered diff.
///
/// A filtered diff's sides are two commits of the pull request's first-parent
/// chain, not the merge base and the head GitHub's own diff has. The core
/// decides here, as pure functions of host-supplied facts, where GitHub is
/// told to put a comment written on such a diff (owner decision Q1) and which
/// review threads are marked on it. The host reads the blobs and computes the
/// diffs whose unchanged lines these decisions use; nothing here reads Git.
///
/// GitHub was measured on BPR-Test #116 (2026-10-21): a comment whose
/// `commit_id` is an earlier commit at a line a later commit changed is
/// accepted and outdated at once; a line no later commit changed is moved to
/// the head by GitHub itself; a `LEFT` line is counted in the merge base's
/// file whatever the commit; and a line outside every hunk of the merge base
/// to that commit is refused with 422.
namespace bpr::core {
	/// Lines of context GitHub's diff keeps around a change, as Git's does. A
	/// comment on a commit resolves only on a line within this distance of a
	/// line the merge base to that commit changes.
	inline constexpr std::uint32_t github_comment_context_lines = 3U;
	/// Unchanged runs one correspondence may hold: the diff engine's own
	/// segment bound.
	inline constexpr std::size_t maximum_correspondence_runs = 1'000'000U;

	/// \brief Lines one diff leaves unchanged: the left document's line
	/// `Left + i` is the right document's line `Right + i`, for `i < Length`.
	struct unchanged_run
	{
		std::uint32_t Left   = 0U;
		std::uint32_t Right  = 0U;
		std::uint32_t Length = 0U;

		auto operator<=>(const unchanged_run&) const = default;
	};

	/// \brief One text diff reduced to which lines it leaves unchanged.
	///
	/// \details `Unchanged` is in document order on both sides and the runs
	/// neither touch nor overlap on either side: every line outside them is a
	/// line the diff removes (left) or adds (right).
	struct line_correspondence
	{
		std::uint32_t              LeftLines  = 0U;
		std::uint32_t              RightLines = 0U;
		std::vector<unchanged_run> Unchanged;

		auto operator<=>(const line_correspondence&) const = default;
	};

	/// \brief Checks the runs are in order, within both documents and apart.
	[[nodiscard]] auto valid(const line_correspondence& aLines) noexcept -> bool;

	/// \brief The unchanged lines of a computed text alignment: its equal
	/// segments. None for a binary or invalid alignment.
	[[nodiscard]] auto correspondence_of(const file_alignment& aAlignment) -> std::optional<line_correspondence>;

	/// \brief The right document's line a left line is, when the diff leaves
	/// it unchanged; none when the diff removes it or it is out of range.
	[[nodiscard]] auto corresponding_right_line(const line_correspondence& aLines, std::uint32_t aLeftLine) noexcept
		-> std::optional<std::uint32_t>;

	/// \brief The left document's line a right line is, when the diff leaves
	/// it unchanged; none when the diff adds it or it is out of range.
	[[nodiscard]] auto corresponding_left_line(const line_correspondence& aLines, std::uint32_t aRightLine) noexcept
		-> std::optional<std::uint32_t>;

	/// \brief Whether a line of one side lies inside one of the diff's hunks:
	/// within `github_comment_context_lines` of a line that side changes, or of
	/// the place a change leaves no line on it. GitHub resolves a line comment
	/// only there.
	[[nodiscard]] auto line_in_change_context(const line_correspondence& aLines, diff_side aSide, std::uint32_t aLine) noexcept
		-> bool;

	/// \brief What the host read about a filtered diff's lines.
	///
	/// \details Each correspondence is absent when its diff is not needed
	/// (its two documents are the same commit) or could not be computed (a
	/// side too large or not text).
	struct filtered_comment_lines
	{
		/// Left: the filtered diff's right document. Right: the head's.
		std::optional<line_correspondence> RightToHead;
		/// Left: the merge base's document. Right: the filtered diff's right
		/// document. Its hunks are the lines GitHub resolves on that commit.
		std::optional<line_correspondence> BaseToRight;
		/// Left: the filtered diff's left document. Right: the merge base's.
		std::optional<line_correspondence> LeftToBase;
		/// Left: the merge base's document. Right: the head's. Its hunks are
		/// the lines GitHub resolves at the head (measured on BPR-Test #117).
		std::optional<line_correspondence> BaseToHead;

		auto operator<=>(const filtered_comment_lines&) const = default;
	};

	/// \brief The facts one comment placement is decided from.
	struct filtered_comment_source
	{
		/// The accepted pull request's head.
		git_commit_oid Head;
		/// The merge base GitHub names; a `LEFT` line is counted in its file.
		git_commit_oid MergeBase;
		/// The two commits the filtered diff is between.
		filtered_diff_endpoints Diff;
		/// The accepted pull request's first-parent chain as GitHub listed
		/// it, oldest first and ending at the head, and the commit it grew from.
		std::vector<git_commit_oid> Chain;
		git_commit_oid              Fork;
		/// Absent while the host is still reading them.
		std::optional<filtered_comment_lines> Lines;

		auto operator<=>(const filtered_comment_source&) const = default;
	};

	/// \brief Whether a placement needs `Lines` at all: a diff whose right side
	/// is the head and whose left side is the merge base is GitHub's own.
	[[nodiscard]] auto filtered_comment_needs_lines(const filtered_comment_source& aSource) noexcept -> bool;

	enum class filtered_comment_placement : std::uint8_t {
		/// Published at the head, as today, at `Line` on `Side`.
		head,
		/// Published on `Commit`, an earlier commit of the chain, at `Line` on
		/// the right: GitHub shows it outdated. The editor warns first.
		earlier_commit,
		/// Not published; `Refusal` says why.
		refused,
	};

	enum class filtered_comment_refusal : std::uint8_t {
		none,
		/// A side of the diff is not a commit of the accepted pull request's
		/// first-parent chain.
		outside_pull_request,
		/// The host has not read the lines yet.
		lines_pending,
		/// The lines could not be read or compared.
		lines_unavailable,
		/// A range on a diff whose side is not GitHub's own.
		range,
		/// The line changed after the diff's commit and lies outside every
		/// hunk of the merge base to that commit, so GitHub resolves it nowhere.
		not_on_commit,
		/// A left-side line the merge base does not have unchanged.
		base_line_changed,
		/// The line moves to the head, where it lies outside every hunk of the
		/// merge base to the head, so GitHub resolves it nowhere (422).
		outside_github_diff,
	};

	/// \brief Where a comment on a filtered diff goes (owner decision Q1).
	struct filtered_comment_target
	{
		filtered_comment_placement Placement = filtered_comment_placement::refused;
		filtered_comment_refusal   Refusal   = filtered_comment_refusal::lines_pending;
		git_commit_oid             Commit;
		diff_side                  Side = diff_side::right;
		std::uint32_t              Line = 0U;

		auto operator<=>(const filtered_comment_target&) const = default;
	};

	/// \brief Decides where GitHub is told to put a comment on a filtered
	/// diff's line.
	///
	/// \details A right-side line unchanged at the head goes to the head at
	/// the head's line. Otherwise it goes to the diff's right commit at the
	/// same line, when the line lies inside a hunk of the merge base to that
	/// commit; GitHub shows it outdated. A left-side line is counted in the
	/// merge base's file (`LEFT` at the head) when the merge base has it
	/// unchanged; otherwise it is refused. A line moved to the head must lie
	/// inside a hunk of the merge base to the head, on its side, where GitHub
	/// resolves it; a line of a side that is GitHub's own is published as
	/// today. A range is published only where no
	/// line moves: a right side that is the head, or a left side that is the
	/// merge base. Both commits must be of the chain (or its fork or the merge
	/// base on the left), so a commit outside the pull request is refused
	/// before anything is written.
	[[nodiscard]] auto place_filtered_comment(const filtered_comment_source& aSource, diff_side aSide, std::uint32_t aLine,
											  std::optional<std::uint32_t> aStartLine) -> filtered_comment_target;

	/// \brief One review thread's line facts, as GitHub reported them.
	struct filtered_thread_anchor
	{
		diff_side                     Side = diff_side::right;
		std::optional<std::uint32_t>  Line;
		std::optional<std::uint32_t>  OriginalLine;
		std::optional<git_commit_oid> Commit;
		std::optional<git_commit_oid> OriginalCommit;

		auto operator<=>(const filtered_thread_anchor&) const = default;
	};

	/// \brief The zero-based line a thread is marked at on a filtered diff, or
	/// none, when it stays in the conversation drawer.
	///
	/// \details A right-side thread is marked when its `commit_id` is the
	/// diff's right commit (at its current line) or its `original_commit_id`
	/// is (at its original line). A `LEFT` thread counts its line in the merge
	/// base, so it is marked only when the diff's left commit is the merge
	/// base, by the same rule. `aAnchor`'s lines are one-based, as GitHub's.
	[[nodiscard]] auto filtered_thread_line(const filtered_thread_anchor& aAnchor, const filtered_diff_endpoints& aDiff,
											const git_commit_oid& aMergeBase) noexcept -> std::optional<std::uint32_t>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_FILTERED_COMMENT_HPP
