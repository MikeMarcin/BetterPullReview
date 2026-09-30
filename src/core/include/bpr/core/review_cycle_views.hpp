#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_VIEWS_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_VIEWS_HPP

#include "bpr/core/diff_compute.hpp"
#include "bpr/core/review_cycle.hpp"
#include "bpr/core/review_cycle_comparison.hpp"
#include "bpr/core/review_cycle_workspace.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

/// \file
/// \brief The reviewer's named comparison views, explicit review checkpoints
/// and the separate chronologies of a cycle's revisions (RFC-BPR-0057,
/// BPR-1163).
/// \details A named view says what the reviewer asked for; it resolves to an
/// exact pair of captured revisions from actual observations only: the
/// cycle's captures, the reviewer's own checkpoints and each round's bound
/// starting revision. Nothing here waits for, or reads, a delivery receipt, a
/// response, an agent or a commit. A chosen pair, and a round with a named
/// result, are pinned and never move; the other views move only when the
/// reviewer refreshes them, and say when a refresh would show newer material.
///
/// The history keeps three orders apart: the order this profile captured
/// revisions in, what the workspace's Git metadata said at each capture, and
/// the revisions a sender named in rounds attached from elsewhere. None of
/// them is a provider's upload order, which a local cycle never observes.
namespace bpr::core {
	/// \brief The reviewer's explicit mark: "I have reviewed through this
	/// revision". Opening a file or reading a response never makes one.
	struct cycle_review_checkpoint
	{
		/// One-based, in the order the reviewer marked them.
		std::uint64_t            Ordinal = 0;
		cycle_source_revision_id Revision;
		auto                     operator<=>(const cycle_review_checkpoint&) const = default;
	};
	enum class cycle_checkpoint_problem : std::uint8_t {
		/// Nothing is captured yet.
		uncaptured,
		/// The revision was never captured by this cycle.
		unknown_revision,
		/// The latest checkpoint already names this revision.
		unchanged,
	};
	[[nodiscard]] auto stable_name(cycle_checkpoint_problem aValue) noexcept -> std::string_view;
	/// \brief The next checkpoint for marking `aRevision` as reviewed.
	[[nodiscard]] auto mark_reviewed(const review_cycle_state& aCycle, const std::optional<cycle_review_checkpoint>& aLatest, cycle_source_revision_id aRevision)
		-> std::expected<cycle_review_checkpoint, cycle_checkpoint_problem>;

	/// \brief What a frozen round was bound to: the revision under review.
	struct cycle_round_bounds
	{
		cycle_round_id           Round;
		cycle_source_revision_id Start;
		auto                     operator<=>(const cycle_round_bounds&) const = default;
	};
	/// \brief Reads the local revision a round's payload names (`R<n>`).
	/// Nothing when it names none this cycle could have allocated.
	[[nodiscard]] auto round_start(std::string_view aSourceRevision) -> std::optional<cycle_source_revision_id>;

	/// \brief The reviewer's own observations the named views resolve from.
	struct cycle_view_facts
	{
		std::optional<cycle_review_checkpoint> LastReview;
		std::vector<cycle_round_bounds>        Rounds;
		auto                                   operator<=>(const cycle_view_facts&) const = default;
	};

	/// \brief This round: what changed from the round's bound revision to a
	/// named result, or to the latest capture while no result is named.
	struct this_round_request
	{
		cycle_round_id                          Round;
		std::optional<cycle_source_revision_id> Result;
		auto                                    operator<=>(const this_round_request&) const = default;
	};
	/// \brief Since my last review: the latest explicit checkpoint against
	/// the latest capture.
	struct since_last_review_request
	{
		auto operator<=>(const since_last_review_request&) const = default;
	};
	/// \brief All cycle changes: the initial capture against the latest.
	struct all_cycle_request
	{
		auto operator<=>(const all_cycle_request&) const = default;
	};
	/// \brief Choose revisions: this exact pair, pinned.
	struct chosen_pair_request
	{
		cycle_source_revision_id Left;
		cycle_source_revision_id Right;
		auto                     operator<=>(const chosen_pair_request&) const = default;
	};
	using cycle_view_request = std::variant<this_round_request, since_last_review_request, all_cycle_request, chosen_pair_request>;

	enum class cycle_view_problem : std::uint8_t {
		uncaptured,
		unknown_revision,
		/// The reviewer has marked no review yet; nothing is guessed from
		/// files opened or responses read.
		no_review_checkpoint,
		/// The round is not a frozen round of this cycle.
		unknown_round,
	};
	[[nodiscard]] auto stable_name(cycle_view_problem aValue) noexcept -> std::string_view;
	/// \brief True for views that name every revision themselves and so never move.
	[[nodiscard]] auto pinned(const cycle_view_request& aRequest) noexcept -> bool;
	/// \brief The exact pair a named view shows now.
	[[nodiscard]] auto resolve_view(const review_cycle_state& aCycle, const cycle_view_facts& aFacts, const cycle_view_request& aRequest)
		-> std::expected<cycle_comparison_pair, cycle_view_problem>;
	/// \brief True when a refresh would show other material: for a pinned
	/// view, a later capture exists; otherwise the view would resolve to
	/// another pair.
	[[nodiscard]] auto view_newer_available(const review_cycle_state& aCycle, const cycle_view_facts& aFacts, const cycle_view_request& aRequest,
											const cycle_comparison_pair& aShown) -> bool;

	/// \brief One displayed row of a file comparison, in reading order.
	enum class cycle_diff_row_kind : std::uint8_t {
		context,
		removed,
		added,
	};
	struct cycle_diff_row
	{
		cycle_diff_row_kind Kind = cycle_diff_row_kind::context;
		/// One-based line numbers on each side the row shows.
		std::optional<std::uint32_t> LeftLine;
		std::optional<std::uint32_t> RightLine;
		/// The line's bytes without its terminator.
		std::string Text;
		auto        operator<=>(const cycle_diff_row&) const = default;
	};
	/// \brief The shared engine's alignment as rows: equal lines once as
	/// context, a replacement as its removed lines then its added lines.
	[[nodiscard]] auto diff_rows(const computed_material_diff& aDiff) -> std::vector<cycle_diff_row>;

	/// \brief How one capture relates to the one captured before it, from the
	/// workspace's own metadata at each capture.
	enum class cycle_revision_step : std::uint8_t {
		/// The first capture of the cycle.
		initial,
		/// The same checkout head: working-tree changes only, or none.
		same_head,
		/// The same branch at another commit. Any commits in between were
		/// not captured, and whether one descends from the other was not
		/// checked.
		head_moved,
		/// Another branch, or a detached head: the work may have diverged.
		branch_changed,
		/// Neither capture read repository metadata (a plain folder or file),
		/// or one of them did not; only the bytes are known.
		no_repository,
		/// The capture before it is not part of this listing.
		previous_unlisted,
	};
	[[nodiscard]] auto stable_name(cycle_revision_step aValue) noexcept -> std::string_view;

	/// \brief What one capture recorded about its source, beside its bytes.
	struct cycle_revision_source
	{
		cycle_source_revision Revision;
		/// The checkout's `HEAD` at capture: a ref name or a detached commit.
		std::optional<std::string>    HeadReference;
		std::optional<git_commit_oid> HeadCommit;
		bool                          Repository = false;
	};
	/// \brief A sender's revision name that an attached round mapped here.
	struct cycle_imported_revision
	{
		std::string              ForeignCycle;
		std::string              ForeignRound;
		std::string              ForeignRevision;
		cycle_source_revision_id Local;
		auto                     operator<=>(const cycle_imported_revision&) const = default;
	};
	/// \brief One revision in the history, with its three chronologies kept apart.
	struct cycle_history_entry
	{
		cycle_source_revision_id                Revision;
		std::optional<cycle_source_revision_id> Parent;
		/// The parent recorded for this capture is not the capture made just
		/// before it, so the local order and the recorded lineage differ.
		bool                          ParentGap = false;
		cycle_revision_step           Step      = cycle_revision_step::initial;
		std::optional<std::string>    HeadReference;
		std::optional<git_commit_oid> HeadCommit;
		/// The bytes equal the previous capture's: a deliberate handoff observation.
		bool                   SameMaterial = false;
		cycle_capture_coverage Coverage     = cycle_capture_coverage::partial;
		cycle_capture_quality  Quality      = cycle_capture_quality::observed_stable;
		/// Sender revisions mapped to this one, as the attaching reviewer chose.
		std::vector<cycle_imported_revision> Imported;
		auto                                 operator<=>(const cycle_history_entry&) const = default;
	};
	struct cycle_history
	{
		/// In local capture order.
		std::vector<cycle_history_entry> Entries;
		/// Captures whose record was not supplied (an earlier page, or a
		/// gap in the revisions read): the history does not claim they are absent.
		std::uint64_t Unlisted = 0;
		/// A local cycle holds no provider upload observations; an upload
		/// order is never inferred from capture order.
		bool ProviderChronology                      = false;
		auto operator<=>(const cycle_history&) const = default;
	};
	/// \brief Describes consecutive captures (in increasing identity) and the
	/// sender revisions mapped onto them.
	[[nodiscard]] auto describe_history(std::span<const cycle_revision_source> aRevisions, std::span<const cycle_imported_revision> aImported) -> cycle_history;
}

#endif
