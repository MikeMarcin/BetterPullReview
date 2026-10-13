#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_OUTGOING_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_OUTGOING_HPP

#include "bpr/core/git_objects.hpp"
#include "bpr/core/review_cycle.hpp"
#include "bpr/core/review_cycle_manifest.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief Selected outgoing content over an explicit Git base (RFC-BPR-0059,
/// BPR-1172).
/// \details Three baselines stay separate. `R0` is the source first
/// inspected, possibly dirty or partial; `Rn` the selected final capture; `P`
/// an exact Git commit whose complete tree the outgoing candidate `C` starts
/// from, observed together with a target branch `T`. The candidate is P's
/// tree with only explicitly selected paths replaced, added or removed: a
/// path a capture did not observe keeps P's entry, so a module-only capture
/// never becomes the deletion of the rest of the repository.
///
/// Each changed path is listed with its starting work (`P -> R0`), the
/// cycle's improvement (`R0 -> Rn`) and what goes out (`P -> C`). Two
/// starting choices propose a selection; neither proposes starting work that
/// nothing in the cycle touched, and an improvement that depends on omitted
/// starting work is a decision for the reviewer, never a falsely clean
/// cherry-pick. Nothing here reads a file, runs Git or contacts a provider.
namespace bpr::core {
	inline constexpr std::size_t maximum_outgoing_reference_bytes = 1024U;
	inline constexpr std::size_t maximum_outgoing_overrides       = 4096U;

	/// \brief `P`: an exact commit and its complete tree, as the host listed it.
	struct publication_base
	{
		git_commit_oid Commit;
		git_tree_oid   Tree;
		/// Every entry other than a tree, sorted by path bytes.
		std::vector<git_tree_entry> Entries;
		/// What the reviewer named (`HEAD`, `refs/remotes/origin/main`), as
		/// data; empty for an exact commit.
		std::string Reference;
	};

	enum class publication_target_source : std::uint8_t {
		/// A reference in the local repository, read as data. It says what
		/// this checkout last knew, not where the remote branch is now.
		local_reference,
		/// The provider's answer for the destination branch.
		provider,
	};

	/// \brief `T`: where the result is meant to go and the head observed there.
	struct publication_target
	{
		std::string               Reference;
		publication_target_source Source = publication_target_source::local_reference;
		git_commit_oid            Head;
		/// The target's complete entries when its head is not P, so a selected
		/// path the target has since changed can be named.
		std::optional<std::vector<git_tree_entry>> Entries;
	};

	/// \brief The Git identities of one captured content object.
	struct outgoing_identity
	{
		cycle_material_digest Content;
		/// The blob of exactly the captured bytes.
		git_entry_oid Raw;
		/// The blob after CRLF line endings become LF, when that changes the
		/// bytes. A comparison aid and a named transformation, never applied
		/// silently.
		std::optional<git_entry_oid> LineFeed;
		auto                         operator<=>(const outgoing_identity&) const = default;
	};

	enum class outgoing_selection_mode : std::uint8_t {
		/// The relevant starting work plus the cycle's improvements.
		starting_work_and_improvements,
		/// Only the cycle's improvements.
		improvements_only,
	};

	enum class outgoing_choice : std::uint8_t {
		include,
		exclude,
	};

	/// \brief The reviewer's explicit choice for one path.
	struct outgoing_override
	{
		std::string     Path;
		outgoing_choice Choice                                      = outgoing_choice::include;
		auto            operator<=>(const outgoing_override&) const = default;
	};

	struct outgoing_request
	{
		outgoing_selection_mode  Mode = outgoing_selection_mode::starting_work_and_improvements;
		cycle_source_revision_id StartRevision;
		cycle_source_revision_id FinalRevision;
		cycle_manifest           Start;
		cycle_manifest           Final;
		publication_base         Base;
		/// Absent when no target was observed.
		std::optional<publication_target> Target;
		/// Identities of every captured content object of both manifests, in
		/// the repository's object format.
		std::vector<outgoing_identity> Identities;
		/// Paths and folders a note names: starting work there is relevant.
		std::vector<std::string>       ReviewedPaths;
		std::vector<outgoing_override> Overrides;
	};

	enum class outgoing_state : std::uint8_t {
		absent,
		present,
		/// A tracked path the capture found missing.
		deleted,
		/// Inside the capture's scope but not enumerated by a partial capture.
		unknown,
		outside_scope,
		/// Seen but not read: too large, sparse, unreadable, a boundary.
		unavailable,
	};

	/// \brief One path on one side.
	struct outgoing_side
	{
		outgoing_state               State = outgoing_state::absent;
		std::uint32_t                Mode  = 0;
		std::optional<git_entry_oid> Object;
		/// The captured bytes' digest and size (captures only).
		std::optional<cycle_material_digest> Content;
		std::uint64_t                        Size = 0;
		/// Tracked by the checkout's index (captures only).
		bool Tracked = false;
		/// A Git filter (such as LFS) turns these bytes into the committed object.
		bool FilterManaged = false;
		/// [[BPR-1244]]. Why the capture saw this path and did not read it;
		/// absent when it was read, or not seen at all.
		std::optional<manifest_availability> Unread;
		auto                                 operator<=>(const outgoing_side&) const = default;
	};

	enum class outgoing_change : std::uint8_t {
		unchanged,
		added,
		modified,
		/// Same bytes, another mode (the executable bit, or file to link).
		mode_changed,
		deleted,
		/// One side does not know the path; nothing is claimed.
		unknown,
	};

	/// \brief Why a path needs a decision or cannot be built natively.
	enum class outgoing_gap : std::uint8_t {
		none,
		/// Improvements only, but this improvement changes a path whose
		/// starting work is omitted. Including it carries that starting work.
		depends_on_starting_work,
		/// The final bytes differ from P but the starting capture did not see
		/// the path (a later scope, or a partial start): origin unknown.
		unknown_start,
		/// The final capture did not read the path; P's entry stays.
		final_unknown,
		/// [[BPR-1244]]. The final capture saw the path but could not read it
		/// (the side's `Unread` says why); P's entry, or its absence, stays.
		final_unavailable,
		/// A filter such as LFS makes the committed object; not built here.
		filter_managed,
		/// A submodule boundary changed; not built here.
		gitlink,
		/// A file and a folder would share this path in the candidate.
		path_conflict,
	};

	struct outgoing_row
	{
		std::string   Path;
		outgoing_side Base;
		outgoing_side Start;
		outgoing_side Final;
		/// `P -> R0`.
		outgoing_change StartingWork = outgoing_change::unchanged;
		/// `R0 -> Rn`.
		outgoing_change Improvement = outgoing_change::unchanged;
		/// `P -> C` after selection.
		outgoing_change Outgoing = outgoing_change::unchanged;
		/// The captured bytes differ from P only by CRLF line endings.
		bool LineEndingsOnly = false;
		/// Starting work nothing in the cycle touched and no note names.
		bool Unrelated = false;
		/// What the starting choice proposed.
		bool Proposed = false;
		/// In the candidate.
		bool                           Selected = false;
		std::optional<outgoing_choice> Choice;
		outgoing_gap                   Gap = outgoing_gap::none;
		/// A deletion whose P bytes the candidate adds at this other path.
		std::optional<std::string> RenamedTo;
		/// The target changed this selected path since P.
		bool Drifted                                = false;
		auto operator<=>(const outgoing_row&) const = default;
	};

	struct outgoing_plan
	{
		outgoing_selection_mode                  Mode = outgoing_selection_mode::starting_work_and_improvements;
		cycle_source_revision_id                 StartRevision;
		cycle_source_revision_id                 FinalRevision;
		git_commit_oid                           BaseCommit;
		git_tree_oid                             BaseTree;
		std::string                              BaseReference;
		std::optional<publication_target_source> TargetSource;
		std::string                              TargetReference;
		std::optional<git_commit_oid>            TargetHead;
		/// T's head is not P: the candidate's parent is stale against it.
		bool TargetMoved   = false;
		bool StartComplete = false;
		bool FinalComplete = false;
		/// Every path whose starting work, improvement or outgoing change is
		/// not `unchanged`, or that carries a gap, in path order.
		std::vector<outgoing_row> Rows;
		/// The complete outgoing listing: P's entries with only selected rows changed.
		std::vector<git_tree_entry> Candidate;
		git_tree_oid                CandidateTree;
		/// The candidate's tree is P's: there is nothing to publish.
		bool NoChange = false;
		/// Rows whose gap asks for an explicit choice and have none.
		std::size_t Undecided = 0;
		/// Selected rows this build cannot turn into objects natively.
		std::size_t Blocked = 0;
		/// [[BPR-1244]]. Rows the final capture saw but could not read: they
		/// are listed, and the candidate carries P's entry for them.
		std::size_t Unread                                  = 0;
		auto        operator<=>(const outgoing_plan&) const = default;

		/// \brief True when a native candidate may be built from the plan.
		[[nodiscard]] auto buildable() const noexcept -> bool
		{
			return !NoChange && Undecided == 0U && Blocked == 0U;
		}
	};

	enum class outgoing_refusal : std::uint8_t {
		/// A plain folder or file: publication needs a Git checkout mapped to
		/// an existing repository. No synthetic root commit is made.
		repository_required,
		/// P, T and the captures are not all of one object format.
		object_format,
		/// P's listing is unsorted, malformed, or does not rebuild P's own
		/// tree (for example a non-canonical legacy mode).
		invalid_base,
		/// A capture's manifest does not validate, or the final capture
		/// precedes the starting one.
		invalid_capture,
		/// A captured content object has no identity.
		missing_identity,
		/// An override names a path no row lists, or names one twice.
		unknown_override,
		bounds,
		/// SHA-1 collision detection refused an object the plan needs an
		/// identity for: P's tree, a commit object, or captured bytes
		/// ([[BPR-1450]]).
		collision_detected,
	};
	[[nodiscard]] auto name(outgoing_refusal aRefusal) noexcept -> std::string_view;
	[[nodiscard]] auto name(outgoing_gap aGap) noexcept -> std::string_view;
	[[nodiscard]] auto name(outgoing_change aChange) noexcept -> std::string_view;

	/// \brief Plans the candidate. The same request always gives the same plan.
	[[nodiscard]] auto plan_outgoing(const outgoing_request& aRequest) -> std::expected<outgoing_plan, outgoing_refusal>;

	/// \brief The plan as readable text: the three baselines, what goes out,
	/// what waits for a decision, what stays out and why, and base drift.
	[[nodiscard]] auto render_outgoing_preview(const outgoing_plan& aPlan) -> std::string;
}

#endif
