#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_COMPARISON_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_COMPARISON_HPP

#include "bpr/core/diff_model.hpp"
#include "bpr/core/review_cycle.hpp"
#include "bpr/core/review_cycle_manifest.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

/// \file
/// \brief Which immutable revisions a cycle view compares, and how two
/// manifests differ path by path (RFC-BPR-0054, RFC-BPR-0057, BPR-1151).
/// \details A view names its material exactly. Full source is one revision
/// read on its own, not a diff against nothing. An explicit revision pair is
/// pinned and never moves; the semantic views (all cycle changes, since a
/// review checkpoint, this round) resolve to a pair when opened and move only
/// when refreshed, with a newer-revision signal in between. Per-file
/// comparisons go through the shared diff engine under a cycle-derived
/// comparison identity; no pull-request epoch is invented.
namespace bpr::core {
	inline constexpr std::size_t maximum_cycle_comparison_page = 256U;

	/// \brief One revision, read as full source.
	struct full_source_view
	{
		cycle_source_revision_id Revision;
		auto                     operator<=>(const full_source_view&) const = default;
	};
	/// \brief An exact, pinned pair.
	struct revision_pair_view
	{
		cycle_source_revision_id Left;
		cycle_source_revision_id Right;
		auto                     operator<=>(const revision_pair_view&) const = default;
	};
	/// \brief The initial revision against the latest.
	struct all_cycle_view
	{
		auto operator<=>(const all_cycle_view&) const = default;
	};
	/// \brief The reviewer's explicit checkpoint against the latest. The
	/// checkpoint is a revision the reviewer marked, never the last file opened.
	struct since_checkpoint_view
	{
		cycle_source_revision_id Checkpoint;
		auto                     operator<=>(const since_checkpoint_view&) const = default;
	};
	/// \brief A round's bound starting revision against a named result, or
	/// against the latest when no result is named.
	struct this_round_view
	{
		cycle_source_revision_id                Start;
		std::optional<cycle_source_revision_id> Result;
		auto                                    operator<=>(const this_round_view&) const = default;
	};
	using cycle_comparison_selector = std::variant<full_source_view, revision_pair_view, all_cycle_view, since_checkpoint_view, this_round_view>;

	/// \brief The exact material a view shows: a right revision, and a left
	/// one unless the view is full source.
	struct cycle_comparison_pair
	{
		std::optional<cycle_source_revision_ref> Left;
		cycle_source_revision_ref                Right;
		auto                                     operator<=>(const cycle_comparison_pair&) const = default;
	};

	enum class cycle_comparison_error : std::uint8_t {
		/// The cycle has no captured revision yet.
		uncaptured,
		/// A named revision was never allocated by this cycle.
		unknown_revision,
	};

	/// \brief True for views that name every revision themselves.
	[[nodiscard]] auto explicit_view(const cycle_comparison_selector& aSelector) noexcept -> bool;
	[[nodiscard]] auto resolve_comparison(const review_cycle_state& aCycle, const cycle_comparison_selector& aSelector)
		-> std::expected<cycle_comparison_pair, cycle_comparison_error>;

	/// \brief An open comparison: what was asked and what it resolved to.
	struct cycle_comparison_view
	{
		cycle_comparison_selector Selector;
		cycle_comparison_pair     Pair;
		auto                      operator<=>(const cycle_comparison_view&) const = default;
	};
	[[nodiscard]] auto open_comparison(const review_cycle_state& aCycle, const cycle_comparison_selector& aSelector)
		-> std::expected<cycle_comparison_view, cycle_comparison_error>;
	/// \brief True when a refresh would show newer material: for an explicit
	/// view, a later revision exists; for a semantic view, it would resolve
	/// differently.
	[[nodiscard]] auto newer_available(const cycle_comparison_view& aView, const review_cycle_state& aCycle) -> bool;
	/// \brief Re-resolves a semantic view on explicit request. An explicit
	/// view is returned unchanged: its pair never moves.
	[[nodiscard]] auto refresh_comparison(const cycle_comparison_view& aView, const review_cycle_state& aCycle) -> cycle_comparison_view;

	/// \brief What a manifest says about one path, including ancestry:
	/// beneath an exclusion or an unlisted directory the path is unavailable,
	/// not absent.
	[[nodiscard]] auto observe_path(const cycle_manifest& aManifest, std::string_view aPath) -> cycle_path_observation;

	/// \brief One side of one path in a comparison.
	struct cycle_path_side
	{
		cycle_path_state                     State = cycle_path_state::unknown;
		std::optional<manifest_entry_kind>   Kind;
		std::uint32_t                        Mode = 0;
		std::optional<cycle_material_digest> Content;
		std::uint64_t                        Size                                      = 0;
		material_text_kind                   Text                                      = material_text_kind::empty;
		auto                                 operator<=>(const cycle_path_side&) const = default;
	};
	/// \brief One path of a comparison page.
	struct cycle_path_comparison
	{
		std::string                    Path;
		std::optional<cycle_path_side> Left;
		cycle_path_side                Right;
		/// Absent for full source. Content-equivalent bytes are unchanged.
		std::optional<cycle_path_change> Change;
		bool                             ModeChanged                                     = false;
		bool                             KindChanged                                     = false;
		auto                             operator<=>(const cycle_path_comparison&) const = default;

		[[nodiscard]] auto changed() const noexcept -> bool
		{
			return (Change && *Change != cycle_path_change::unchanged) || ModeChanged || KindChanged;
		}
	};
	struct cycle_comparison_page
	{
		std::vector<cycle_path_comparison> Entries;
		/// Continue after this path; absent on the last page.
		std::optional<std::string> Next;
		auto                       operator<=>(const cycle_comparison_page&) const = default;
	};
	/// \brief One path compared across two manifests, or read from one.
	[[nodiscard]] auto compare_manifest_path(const cycle_manifest* aLeft, const cycle_manifest& aRight, std::string_view aPath) -> cycle_path_comparison;
	/// \brief One bounded page of paths after `aAfter`, in path-byte order,
	/// over every path either manifest names. `aLeft` is absent for full
	/// source. With `aChangedOnly`, unchanged paths are skipped.
	[[nodiscard]] auto compare_manifests(const cycle_manifest* aLeft, const cycle_manifest& aRight, const std::optional<std::string>& aAfter, std::size_t aLimit,
										 bool aChangedOnly) -> cycle_comparison_page;

	/// \brief The diff-engine identity of one path under one cycle pair,
	/// domain-separated from every pull-request comparison. Absent when
	/// neither side has content to compare.
	[[nodiscard]] auto cycle_file_comparison(const local_cycle_subject& aSubject, const cycle_comparison_pair& aPair, std::string_view aPath,
											 const std::optional<cycle_material_digest>& aLeft, const std::optional<cycle_material_digest>& aRight,
											 const diff_options& aOptions) -> std::optional<material_comparison_key>;

	/// \brief One file shown in a view: the pair and the path.
	struct cycle_file_view_key
	{
		cycle_comparison_pair Pair;
		std::string           Path;
		auto                  operator<=>(const cycle_file_view_key&) const = default;
	};
	/// \brief What a file view shows while newer material loads. The shown
	/// content stays until a newer request completes; an older completion is
	/// dropped; a failure or cancellation keeps what is shown.
	struct cycle_file_view_state
	{
		std::uint64_t                                                NextRequest = 1;
		std::optional<cycle_file_view_key>                           Shown;
		std::optional<std::pair<std::uint64_t, cycle_file_view_key>> Pending;
		auto                                                         operator<=>(const cycle_file_view_state&) const = default;
	};
	enum class cycle_file_view_result : std::uint8_t {
		/// The completion was the pending request and is now shown.
		shown,
		/// The completion was for a superseded request and is dropped.
		stale,
		/// The pending request failed; the previous content stays.
		kept,
	};
	[[nodiscard]] auto request_file(const cycle_file_view_state& aState, const cycle_file_view_key& aKey) -> std::pair<cycle_file_view_state, std::uint64_t>;
	[[nodiscard]] auto complete_file(const cycle_file_view_state& aState, std::uint64_t aRequest, bool aAvailable)
		-> std::pair<cycle_file_view_state, cycle_file_view_result>;
	[[nodiscard]] auto cancel_file(const cycle_file_view_state& aState) -> cycle_file_view_state;
}

#endif
