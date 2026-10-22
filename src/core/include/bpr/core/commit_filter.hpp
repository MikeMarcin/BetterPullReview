#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_COMMIT_FILTER_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_COMMIT_FILTER_HPP

#include "bpr/core/epoch_model.hpp"
#include "bpr/core/review_ledger.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// RFC-BPR-0089 M1. Commit filters over a pull request's review.
///
/// The core decides everything here as pure functions of host-supplied
/// facts: the commit list GitHub returned (with parents), the comparison's two
/// endpoint commits, the reviewer's retained filter and the paths each
/// first-parent commit changes. The host acquires the trees and runs the tree
/// diffs; Qt draws the Commits tab. Nothing here reads Git, a clock or Qt.
///
/// The decisions never build a tree that no commit contains. A path's
/// filtered diff is always between two commits of the pull request's
/// first-parent chain, or it is exactly the comparison's own diff.
namespace bpr::core {
	/// The REST commit list's own cap ([product contract]). A list GitHub
	/// truncated there has no commit filter.
	inline constexpr std::size_t maximum_filter_commits = 250U;
	/// Parents one listed commit may name; an octopus merge beyond this is
	/// refused rather than read partially.
	inline constexpr std::size_t maximum_filter_commit_parents = 16U;
	/// Bytes of a commit headline the filter retains to name a gone commit.
	inline constexpr std::size_t maximum_filter_headline_bytes = 256U;
	/// RFC-BPR-0089 M3. Bytes of an author's login the filter retains to
	/// offer a commit with the same headline and author after a force push.
	inline constexpr std::size_t maximum_filter_author_bytes = 256U;
	/// Hidden entries one filter may retain: every listed commit plus as many
	/// that a force push removed.
	inline constexpr std::size_t maximum_filter_hidden_commits = 2U * maximum_filter_commits;
	/// Paths one commit's first-parent diff may report. The host's tree diff
	/// stops at the same count (`maximum_git_changed_entries`).
	inline constexpr std::size_t maximum_filter_commit_paths = 10'000U;
	/// Paths a whole chain's touch sets may report together.
	inline constexpr std::size_t maximum_filter_touched_paths = 262'144U;

	/// \brief One commit as GitHub listed it, with its parents in order.
	struct filter_commit
	{
		git_commit_oid              Oid;
		std::vector<git_commit_oid> Parents;
		std::string                 Headline;
		/// RFC-BPR-0089 M3. The author's login, or empty when GitHub named none.
		std::string Author;

		auto operator<=>(const filter_commit&) const = default;
	};

	/// \brief The pull request's commit list exactly as accepted.
	///
	/// \details `Commits` is GitHub's order. `Truncated` is the provider's
	/// fact that the list stopped at its cap while the pull request reports
	/// more commits; such a list has no filter, because the chain it would
	/// imply is not the pull request's.
	struct filter_commit_list
	{
		git_commit_oid             Head;
		std::vector<filter_commit> Commits;
		bool                       Truncated = false;

		auto operator<=>(const filter_commit_list&) const = default;
	};

	/// \brief Why a pull request offers no commit filter at all.
	enum class commit_filter_unavailable : std::uint8_t {
		/// GitHub's list stopped at 250 commits.
		truncated,
		/// No commits, an invalid identity, or a list over its bounds.
		invalid_list,
		/// The accepted head is not in the list.
		head_missing,
		/// A commit is listed twice.
		duplicate_commit,
		/// A commit names more parents than the filter reads.
		too_many_parents,
		/// The chain reaches a root commit: the pull request shares no
		/// history with its base, so no first-parent diff has a left side.
		unrelated_history,
		/// The commit list named no parents (a recorded response may omit
		/// them), so the chain cannot be known.
		parents_unknown,
	};

	/// \brief A listed commit's place relative to the first-parent chain.
	enum class filter_commit_role : std::uint8_t {
		/// On the head's first-parent chain; filtered on its own.
		chain,
		/// On the chain, a merge whose other parents are the pull request's
		/// own commits (a merged topic branch).
		chain_merge,
		/// On the chain, a merge that brings in commits the pull request does
		/// not list: the base branch's work. Listed and labelled; it can be
		/// hidden, never shown on its own.
		base_merge,
		/// Not on the chain; reached through a chain merge's other parents and
		/// filtered through that merge.
		side,
	};

	/// \brief One listed commit with its role decided.
	struct filter_commit_entry
	{
		git_commit_oid Oid;
		/// Position in GitHub's list.
		std::size_t        Listed = 0U;
		filter_commit_role Role   = filter_commit_role::chain;
		/// Position on the chain, oldest first; absent for a side commit.
		std::optional<std::size_t> Chain;
		/// The chain merge a side commit is filtered through. A side commit no
		/// chain merge reaches has none and is never filtered.
		std::optional<git_commit_oid> Merge;

		auto operator<=>(const filter_commit_entry&) const = default;
	};

	/// \brief The first-parent chain and every listed commit's role.
	///
	/// \details `Chain` runs oldest first and ends at the head. `Fork` is the
	/// first parent of the chain's oldest commit, the commit the chain grew
	/// from.
	struct filter_commit_graph
	{
		git_commit_oid                   Head;
		std::vector<git_commit_oid>      Chain;
		git_commit_oid                   Fork;
		std::vector<filter_commit_entry> Entries;
		/// Headlines in `Entries` order, retained to name hidden commits.
		std::vector<std::string> Headlines;
		/// RFC-BPR-0089 M3. Authors in `Entries` order, retained the same way.
		std::vector<std::string> Authors;

		auto operator<=>(const filter_commit_graph&) const = default;
	};

	/// \brief Decides the chain and every commit's role from GitHub's list.
	[[nodiscard]] auto build_filter_commit_graph(const filter_commit_list& aList)
		-> std::expected<filter_commit_graph, commit_filter_unavailable>;

	/// \brief The listed entry for one commit, or none.
	[[nodiscard]] auto find_filter_commit(const filter_commit_graph& aGraph, const git_commit_oid& aOid) noexcept
		-> const filter_commit_entry*;

	/// \brief The comparison a filter is applied to, by its endpoint commits.
	///
	/// \details `WholePullRequest` spans every chain commit whatever the two
	/// commits are: Whole PR compares from GitHub's merge base, which a merge
	/// from the base branch moves off the chain.
	struct filter_comparison
	{
		bool           WholePullRequest = true;
		git_commit_oid Left;
		git_commit_oid Right;

		auto operator<=>(const filter_comparison&) const = default;
	};

	/// \brief [[BPR-1707]]. What the window knows of its comparison while it
	/// may be resolving it again.
	struct filter_comparison_reading
	{
		/// The comparison the window accepted last, still the one on screen.
		std::optional<filter_comparison> Accepted;
		/// A resolution of the comparison is in flight.
		bool Resolving = false;
		/// That resolution asks for the comparison already accepted (a
		/// refresh), not one the reviewer chose since.
		bool SameRequest = false;

		auto operator<=>(const filter_comparison_reading&) const = default;
	};

	/// \brief [[BPR-1707]]. The comparison a commit filter applies to.
	///
	/// \details A refresh accepts the pull request again and then resolves
	/// the same comparison. Until it lands the accepted comparison is the one
	/// on screen, so the filter keeps applying to it: the filtered file set
	/// does not lapse to the whole pull request and come back (which retired
	/// the reviewer's chosen file), and a gesture made meanwhile is decided
	/// against it instead of being refused as suspended. A comparison the
	/// reviewer chose since applies once it is resolved, as before.
	[[nodiscard]] constexpr auto filter_comparison_in_force(const filter_comparison_reading& aReading) noexcept
		-> std::optional<filter_comparison>
	{
		return !aReading.Resolving || aReading.SameRequest ? aReading.Accepted : std::nullopt;
	}

	/// \brief Why a filter does not apply to the current comparison.
	enum class commit_filter_suspension : std::uint8_t {
		none,
		/// An endpoint is not on the pull request's first-parent chain, for
		/// example a pair across a force push.
		not_first_parent_range,
		/// The left endpoint is newer than the right.
		reversed,
		/// Both endpoints are the same commit; nothing is compared.
		empty_comparison,
	};

	/// \brief The chain commits a comparison spans, as `[Begin, End)`.
	struct filter_commit_span
	{
		std::size_t Begin = 0U;
		std::size_t End   = 0U;

		[[nodiscard]] constexpr auto contains(const std::size_t aChain) const noexcept -> bool
		{
			return aChain >= Begin && aChain < End;
		}

		[[nodiscard]] constexpr auto size() const noexcept -> std::size_t
		{
			return End > Begin ? End - Begin : 0U;
		}

		auto operator<=>(const filter_commit_span&) const = default;
	};

	/// \brief The first-parent commits after the left endpoint, up to the right.
	[[nodiscard]] auto filter_span_for(const filter_commit_graph& aGraph, const filter_comparison& aComparison)
		-> std::expected<filter_commit_span, commit_filter_suspension>;

	/// \brief RFC-BPR-0089 M3. One path a commit's first-parent diff changes,
	/// with its blob on each side; a side the path is absent from has none.
	///
	/// \details `Path` is the touch key (`filter_commit_touch`): the path after
	/// the change, or before it for a deletion. `PreviousPath` names a rename's
	/// or copy's source.
	struct filter_path_change
	{
		std::string                 Path;
		std::optional<std::string>  PreviousPath;
		std::optional<git_blob_oid> Left;
		std::optional<git_blob_oid> Right;

		auto operator<=>(const filter_path_change&) const = default;
	};

	/// \brief RFC-BPR-0089 M3. Everything one listed commit's first-parent diff
	/// changes, sorted by path and unique. Two commits make "the same change"
	/// when these are equal: the same paths, each between the same two blobs.
	struct filter_commit_changes
	{
		git_commit_oid                  Commit;
		std::vector<filter_path_change> Changes;

		auto operator<=>(const filter_commit_changes&) const = default;
	};

	/// \brief One commit the reviewer hid, named so it can be named once gone.
	struct filter_hidden_commit
	{
		git_commit_oid Oid;
		std::string    Headline;
		/// RFC-BPR-0089 M3. Recorded while the commit is listed: its author and
		/// what it changes, so that after a force push a new commit carrying the
		/// same change can be offered in its place. `Changes` is absent until
		/// the chain's trees are read.
		std::string                                    Author;
		std::optional<std::vector<filter_path_change>> Changes;

		auto operator<=>(const filter_hidden_commit&) const = default;
	};

	/// \brief The reviewer's commit filter, retained by the window.
	///
	/// \details The filter is the set of commits the reviewer hid, sorted by
	/// commit and unique. Everything else in a comparison's span is shown, so
	/// a commit a later push adds is shown until the reviewer hides it: no
	/// gesture ever hides a commit the reviewer did not name. An entry whose
	/// commit is no longer listed (a force push rewrote it) stops applying and
	/// is kept only to be named, until Clear all filters.
	struct commit_filter_state
	{
		std::vector<filter_hidden_commit> Hidden;

		auto operator<=>(const commit_filter_state&) const = default;
	};

	/// \brief Checks the state's own shape: sorted, unique, bounded, valid.
	[[nodiscard]] auto valid(const commit_filter_state& aState) noexcept -> bool;

	enum class commit_filter_operation_kind : std::uint8_t {
		/// The shown commits become exactly the selection.
		show_only,
		/// The selected commits leave the shown set.
		hide,
		/// The selected hidden commits return to the shown set.
		show,
		/// Every commit is shown again and every entry is forgotten.
		clear,
	};

	/// \brief One context-menu gesture over a selection of listed commits.
	struct commit_filter_operation
	{
		commit_filter_operation_kind Kind = commit_filter_operation_kind::show_only;
		std::vector<git_commit_oid>  Selection;

		auto operator<=>(const commit_filter_operation&) const = default;
	};

	enum class commit_filter_refusal : std::uint8_t {
		none,
		/// The pull request offers no filter (`commit_filter_unavailable`).
		unavailable,
		/// The comparison is not a first-parent range.
		suspended,
		/// Nothing selected, or nothing selected the comparison spans.
		empty_selection,
		/// A selected commit is not listed.
		unknown_commit,
		/// Show selected without a hidden commit in the selection.
		nothing_hidden,
		/// The result would show only merges from the base branch.
		base_merge_alone,
		/// The state is malformed or would exceed its bound.
		invalid_state,
		/// RFC-BPR-0089 M3. A rematch the core does not offer now.
		not_offered,
	};

	/// \brief Applies one gesture to the filter under a comparison's span.
	///
	/// \details Each gesture acts on the commits the span covers; a selected
	/// side commit acts through its merge, and selected commits outside the
	/// span are ignored. Entries outside the span, and gone ones, are kept.
	/// Clear needs no graph and always succeeds on a valid state.
	[[nodiscard]] auto apply_commit_filter(const commit_filter_state& aState, const filter_commit_graph& aGraph,
										   const filter_commit_span& aSpan, const commit_filter_operation& aOperation)
		-> std::expected<commit_filter_state, commit_filter_refusal>;

	/// \brief Whether the gesture would be accepted, for menu enablement.
	[[nodiscard]] auto commit_filter_operation_available(const commit_filter_state& aState, const filter_commit_graph& aGraph,
														 const filter_commit_span& aSpan, const commit_filter_operation& aOperation)
		-> bool;

	/// \brief A listed commit's state under the filter and the comparison.
	enum class filter_commit_state : std::uint8_t {
		shown,
		hidden,
		/// The comparison does not span it; greyed in the list.
		outside_comparison,
	};

	/// \brief Whether a listed commit is hidden by the filter (directly or
	/// through its merge), shown, or outside the span.
	[[nodiscard]] auto commit_state(const commit_filter_state& aState, const filter_commit_graph& aGraph,
									const filter_commit_span& aSpan, const filter_commit_entry& aEntry) noexcept
		-> filter_commit_state;

	enum class commit_filter_status : std::uint8_t {
		/// No hidden commit applies: the review is exactly the comparison.
		inactive,
		/// At least one hidden commit in the span applies.
		active,
		/// The state names commits, but the comparison is not a first-parent
		/// range; the review is the comparison and the chip says why.
		suspended,
		/// The pull request offers no filter; the tab says why.
		unavailable,
	};

	/// \brief What the chip and the Commits tab say about a filter.
	///
	/// \details `Shown` and `Spanned` count listed commits the comparison
	/// spans, side commits through their merges ("2 of 3 commits shown").
	/// `Gone` names hidden entries no longer listed, in the state's order.
	struct commit_filter_summary
	{
		commit_filter_status                     Status = commit_filter_status::inactive;
		commit_filter_suspension                 Suspension{ };
		std::optional<commit_filter_unavailable> Unavailable;
		std::size_t                              Shown   = 0U;
		std::size_t                              Spanned = 0U;
		std::vector<filter_hidden_commit>        Gone;

		auto operator<=>(const commit_filter_summary&) const = default;
	};

	/// \brief Summarizes the filter for the chip and the tab.
	[[nodiscard]] auto summarize_commit_filter(const commit_filter_state&                                                        aState,
											   const std::expected<filter_commit_graph, commit_filter_unavailable>&              aGraph,
											   const std::optional<std::expected<filter_commit_span, commit_filter_suspension>>& aSpan)
		-> commit_filter_summary;

	/// \brief The paths one chain commit's first-parent diff changes.
	///
	/// \details Keyed by the path after the change, or before it for a
	/// deletion; an exact rename counts under its new path. Sorted and
	/// unique. The host derives it from a local tree diff.
	struct filter_commit_touch
	{
		git_commit_oid           Commit;
		std::vector<std::string> Paths;

		auto operator<=>(const filter_commit_touch&) const = default;
	};

	/// \brief How one path's filtered diff is formed.
	enum class filtered_path_kind : std::uint8_t {
		/// No hidden commit changes the path: its diff is exactly the
		/// comparison's own, with identical material.
		unchanged,
		/// One exact diff from the path at `Left` (the first shown commit's
		/// parent) to the path at `Right` (the last shown commit).
		exact,
		/// A hidden commit changes the path between two shown ones: one exact
		/// diff per shown commit, with the hidden ones named between
		/// (owner, Q2). Each is read on its own, as its own material.
		split,
	};

	/// \brief One exact per-commit diff of a split path.
	struct filtered_path_segment
	{
		git_commit_oid Left;
		git_commit_oid Right;
		/// Hidden commits that change the path after this segment and before
		/// the next one, oldest first.
		std::vector<git_commit_oid> HiddenAfter;

		auto operator<=>(const filtered_path_segment&) const = default;
	};

	/// \brief One path in the filtered set and how its diff is formed.
	struct filtered_path
	{
		std::string        Path;
		filtered_path_kind Kind = filtered_path_kind::unchanged;
		/// For `exact`: the two commits the diff is between.
		git_commit_oid Left;
		git_commit_oid Right;
		/// For `split`: the per-commit diffs in chain order.
		std::vector<filtered_path_segment> Segments;

		auto operator<=>(const filtered_path&) const = default;
	};

	/// \brief The filtered file set, sorted by path.
	///
	/// \details `Excluded` counts paths the span's commits change only in
	/// hidden commits.
	struct filtered_path_set
	{
		std::vector<filtered_path> Paths;
		std::size_t                Excluded = 0U;

		auto operator<=>(const filtered_path_set&) const = default;
	};

	enum class filtered_path_refusal : std::uint8_t {
		/// The touch sets do not cover exactly the span's chain commits in
		/// order, or a set is unsorted, repeated or over its bound.
		invalid_touches,
		/// The filter is not active under this span.
		inactive,
	};

	/// \brief Decides the filtered file set and each path's diff.
	///
	/// \details `aTouches` covers exactly the span's chain commits, oldest
	/// first. A path is in the set when a shown commit changes it. Its diff is
	/// the comparison's own when no hidden commit changes it, one exact diff
	/// from the first shown commit's parent to the last shown commit when no
	/// hidden commit changes it between them, and split otherwise.
	[[nodiscard]] auto filter_paths(const commit_filter_state& aState, const filter_commit_graph& aGraph,
									const filter_commit_span& aSpan, std::span<const filter_commit_touch> aTouches)
		-> std::expected<filtered_path_set, filtered_path_refusal>;

	/// \brief The first parent of a chain commit: the chain's previous commit,
	/// or the fork for the oldest. Invalid for a position off the chain.
	[[nodiscard]] auto chain_parent(const filter_commit_graph& aGraph, std::size_t aChain) noexcept
		-> git_commit_oid;

	/// \brief RFC-BPR-0089 M3. Records on each hidden entry still listed its
	/// author and, when `aChanges` names its commit and it has none yet, what
	/// it changes. Entries that are gone, or not named, keep what they hold.
	///
	/// \details `aChanges` is any set of listed commits' changes, each sorted by
	/// path and unique; one that is not is ignored. Applied while the chain's
	/// trees are read, so a later force push still knows what a gone commit did.
	[[nodiscard]] auto record_hidden_changes(const commit_filter_state& aState, const filter_commit_graph& aGraph,
											 std::span<const filter_commit_changes> aChanges) -> commit_filter_state;

	/// \brief RFC-BPR-0089 M3. Why a commit is offered in place of a gone one.
	enum class filter_rematch_basis : std::uint8_t {
		/// It changes exactly the paths the gone commit changed, each between
		/// the same two blobs.
		same_change,
		/// Only its headline and author match: offered and labelled a guess.
		same_headline_and_author,
	};

	/// \brief RFC-BPR-0089 M3. A listed commit the reviewer may hide in place
	/// of a hidden commit a force push removed. Offered, never applied.
	struct filter_rematch_offer
	{
		git_commit_oid       Gone;
		git_commit_oid       Candidate;
		filter_rematch_basis Basis = filter_rematch_basis::same_change;

		auto operator<=>(const filter_rematch_offer&) const = default;
	};

	/// \brief RFC-BPR-0089 M3. The commits each gone hidden entry may be
	/// matched to, in the state's order and then chain order.
	///
	/// \details A candidate is a listed chain commit (not a merge from the base
	/// branch, not a side commit) the filter does not hide. A gone entry whose
	/// recorded changes some candidates' changes in `aChanges` equal is offered
	/// those; otherwise, when it recorded a headline and an author, the
	/// candidates with both equal, as a guess. Nothing else is offered.
	[[nodiscard]] auto rematch_offers(const commit_filter_state& aState, const filter_commit_graph& aGraph,
									  std::span<const filter_commit_changes> aChanges) -> std::vector<filter_rematch_offer>;

	/// \brief RFC-BPR-0089 M3. Applies one offer the reviewer chose: the gone
	/// entry is forgotten and the candidate is hidden in its place.
	///
	/// \details Refused with `not_offered` unless `rematch_offers` offers it
	/// now, so nothing is ever rematched without its own offer.
	[[nodiscard]] auto apply_rematch(const commit_filter_state& aState, const filter_commit_graph& aGraph,
									 std::span<const filter_commit_changes> aChanges, const filter_rematch_offer& aOffer)
		-> std::expected<commit_filter_state, commit_filter_refusal>;

	/// \brief RFC-BPR-0089 M3. A split file's receipt for its row, from its
	/// per-commit diffs' own receipts in chain order.
	///
	/// \details Each per-commit diff is its own material (owner, Q2), so the
	/// file is only as far read as its least read diff: a diff with no receipt
	/// counts as unread, and none at all gives none. It is accepted, or
	/// excluded, only when every diff is. `aFile` names the row; the material
	/// is the first diff's.
	[[nodiscard]] auto combined_split_receipt(const file_identity& aFile, std::span<const std::optional<file_receipt>> aSegments)
		-> std::optional<file_receipt>;

	/// RFC-BPR-0089 M2. A commit whose first-parent diff changes at least this
	/// many files is labelled bulk. The label is a hint and filters nothing.
	inline constexpr std::size_t bulk_commit_files = 100U;

	/// \brief Whether a commit that changes `aFiles` files is labelled bulk.
	[[nodiscard]] constexpr auto bulk_commit(const std::size_t aFiles) noexcept -> bool
	{
		return aFiles >= bulk_commit_files;
	}

	/// \brief RFC-BPR-0089 M2. The reviewer's file filters: paths hidden by
	/// "Hide files these commits changed", sorted and unique.
	///
	/// \details A file filter names paths, not commits, so it hides a path in
	/// every comparison and under every commit filter, and a force push leaves
	/// it as it was. Clear all filters forgets it.
	struct file_filter_state
	{
		std::vector<std::string> Hidden;

		auto operator<=>(const file_filter_state&) const = default;
	};

	/// \brief Checks the file filter's shape: sorted, unique, bounded paths.
	[[nodiscard]] auto valid(const file_filter_state& aState) noexcept -> bool;

	/// \brief Whether the file filter hides `aPath`.
	[[nodiscard]] auto file_filter_hides(const file_filter_state& aState, std::string_view aPath) noexcept -> bool;

	/// \brief "Hide files these commits changed": adds every path the selected
	/// commits change to the file filter.
	///
	/// \details `aTouches` covers exactly the span's chain commits, oldest
	/// first, as for `filter_paths`. A selected side commit acts through its
	/// merge; selected commits the span does not cover are ignored. The
	/// commits' shown or hidden state does not matter: the paths are hidden in
	/// every shown commit. A path already hidden stays hidden.
	[[nodiscard]] auto hide_files_changed_by(const file_filter_state& aState, const filter_commit_graph& aGraph,
											 const filter_commit_span& aSpan, std::span<const filter_commit_touch> aTouches,
											 std::span<const git_commit_oid> aSelection)
		-> std::expected<file_filter_state, commit_filter_refusal>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_COMMIT_FILTER_HPP
