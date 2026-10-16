#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_DIFF_SEMANTICS_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_DIFF_SEMANTICS_HPP

#include "bpr/core/diff_compute.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace bpr::core {
	/// \brief Records the immutable records retained by one validated semantic index.
	struct diff_semantic_index_statistics
	{
		std::size_t Segments       = 0U;
		std::size_t Pairs          = 0U;
		std::size_t Groups         = 0U;
		std::size_t Moves          = 0U;
		std::size_t IntralineEdits = 0U;
		std::size_t IndexEntries   = 0U;

		auto operator<=>(const diff_semantic_index_statistics&) const = default;
	};

	struct diff_semantic_index_build;

	/// \brief Binds indexed semantic queries to one fully validated immutable result.
	///
	/// The index shares ownership of the immutable result for its full lifetime.
	class diff_semantic_index final
	{
	public:
		diff_semantic_index(const diff_semantic_index&)     = default;
		diff_semantic_index(diff_semantic_index&&) noexcept = default;
		auto operator=(const diff_semantic_index&)
			-> diff_semantic_index& = default;
		auto operator=(diff_semantic_index&&) noexcept
			-> diff_semantic_index& = default;

		[[nodiscard]] auto diff() const noexcept -> const computed_file_diff&;
		/// \brief Checks exact immutable-result ownership without repeating validation.
		[[nodiscard]] auto matches_result(const computed_file_diff& aResult) const noexcept -> bool;
		/// \brief Counts this index object and vector capacities, excluding the shared result.
		[[nodiscard]] auto retained_bytes() const noexcept -> std::optional<std::uint64_t>;
		[[nodiscard]] auto statistics() const noexcept
			-> diff_semantic_index_statistics;
		[[nodiscard]] auto segment_index(alignment_segment_id aSegment) const noexcept
			-> std::optional<std::size_t>;
		[[nodiscard]] auto group_index(change_group_id aGroup) const noexcept
			-> std::optional<std::size_t>;
		[[nodiscard]] auto pair_index(line_pair_id aPair) const noexcept
			-> std::optional<std::size_t>;
		[[nodiscard]] auto move_index(alignment_segment_id aSegment) const noexcept
			-> std::optional<std::size_t>;
		[[nodiscard]] auto intraline_span(std::size_t aPairIndex) const noexcept
			-> segment_index_span;

	private:
		friend auto build_diff_semantic_index(
			std::shared_ptr<const computed_file_diff>)
			-> std::optional<diff_semantic_index>;
		friend auto build_diff_semantic_index(
			std::shared_ptr<const computed_file_diff>, const diff_compute_control&)
			-> diff_semantic_index_build;
		diff_semantic_index() = default;
		/// Builds the lookup tables of a result that has passed validation.
		[[nodiscard]] static auto from_validated(std::shared_ptr<const computed_file_diff> aDiff)
			-> diff_semantic_index;

		std::shared_ptr<const computed_file_diff>                   Diff;
		std::vector<std::pair<alignment_segment_id, std::uint32_t>> SegmentsById;
		std::vector<std::pair<change_group_id, std::uint32_t>>      GroupsById;
		std::vector<std::pair<line_pair_id, std::uint32_t>>         PairsById;
		std::vector<std::pair<alignment_segment_id, std::uint32_t>> MovesBySegment;
		std::vector<segment_index_span>                             IntralineByPair;
	};

	/// \brief Validates once and builds bounded lookup indexes for repeated queries.
	[[nodiscard]] auto build_diff_semantic_index(
		std::shared_ptr<const computed_file_diff> aDiff)
		-> std::optional<diff_semantic_index>;

	/// \brief The outcome of a semantic index build that may be stopped.
	struct diff_semantic_index_build
	{
		/// The index; absent when the result is invalid or the build stopped.
		std::optional<diff_semantic_index> Index;
		/// The caller's stop ended the build, which is abandoned ([[BPR-1485]]).
		bool Stopped = false;
	};

	/// \brief `build_diff_semantic_index`, polling `aControl`'s stop through
	/// its validation. [[BPR-1366]]
	[[nodiscard]] auto build_diff_semantic_index(
		std::shared_ptr<const computed_file_diff> aDiff, const diff_compute_control& aControl)
		-> diff_semantic_index_build;

	/// \brief Describes the source span ref record.
	struct source_span_ref
	{
		file_comparison_digest Comparison;
		diff_side              Side = diff_side::left;
		line_span              Lines;

		auto operator<=>(const source_span_ref&) const = default;
	};

	enum class line_mapping_kind : std::uint8_t {
		exact_line,
		similar_line,
		opposing_span,
		opposing_boundary,
		unavailable,
	};

	enum class semantic_lookup_reason : std::uint8_t {
		none,
		invalid_result,
		comparison_mismatch,
		invalid_side,
		line_out_of_range,
		non_text,
		segment_not_found,
	};

	/// \brief Describes the line mapping record.
	struct line_mapping
	{
		line_mapping_kind                   Kind = line_mapping_kind::unavailable;
		source_line_ref                     Source;
		std::optional<source_line_ref>      Counterpart;
		std::optional<source_span_ref>      OpposingSpan;
		std::optional<source_boundary_ref>  OpposingBoundary;
		std::optional<alignment_segment_id> Segment;
		std::optional<change_group_id>      Group;
		std::optional<line_pair_id>         Pair;
		semantic_lookup_reason              Reason     = semantic_lookup_reason::none;
		diff_reason_code                    Diagnostic = diff_reason_code::none;

		auto operator<=>(const line_mapping&) const = default;
	};

	/// \brief Describes the move correspondence record.
	struct move_correspondence
	{
		move_relation_id     Relation;
		alignment_segment_id SourceDeletion;
		alignment_segment_id DestinationInsertion;
		source_line_ref      Selected;
		source_line_ref      Counterpart;

		auto operator<=>(const move_correspondence&) const = default;
	};

	/// \brief Computes the map line result.
	[[nodiscard]] auto map_line(const computed_file_diff& aDiff,
								source_line_ref           aLine) -> line_mapping;
	[[nodiscard]] auto map_line(const diff_semantic_index& aIndex,
								source_line_ref            aLine) -> line_mapping;
	/// \brief Computes the move for line result.
	[[nodiscard]] auto move_for_line(const computed_file_diff& aDiff,
									 source_line_ref           aLine)
		-> std::optional<move_correspondence>;
	[[nodiscard]] auto move_for_line(const diff_semantic_index& aIndex,
									 source_line_ref            aLine)
		-> std::optional<move_correspondence>;

	enum class line_detail_kind : std::uint8_t {
		paired_lines,
		opposing_span,
		opposing_boundary,
		unavailable,
	};

	/// \brief Describes the line detail input record.
	struct line_detail_input
	{
		line_detail_kind                    Kind = line_detail_kind::unavailable;
		source_line_ref                     Selected;
		std::optional<source_line_ref>      LeftLine;
		std::optional<source_line_ref>      RightLine;
		std::optional<source_span_ref>      OpposingSpan;
		std::optional<source_boundary_ref>  OpposingBoundary;
		std::optional<alignment_segment_id> Segment;
		std::optional<change_group_id>      Group;
		std::optional<line_pair_id>         Pair;
		std::optional<move_relation_id>     Move;
		std::vector<intraline_edit>         Intraline;
		semantic_lookup_reason              Reason     = semantic_lookup_reason::none;
		diff_reason_code                    Diagnostic = diff_reason_code::none;

		auto operator<=>(const line_detail_input&) const = default;
	};

	/// \brief Computes the detail for mapping result.
	[[nodiscard]] auto detail_for_mapping(const computed_file_diff& aDiff,
										  const line_mapping&       aMapping)
		-> line_detail_input;
	[[nodiscard]] auto detail_for_mapping(const diff_semantic_index& aIndex,
										  const line_mapping&        aMapping)
		-> line_detail_input;
	/// \brief Computes the detail for line result.
	[[nodiscard]] auto detail_for_line(const computed_file_diff& aDiff,
									   source_line_ref           aLine) -> line_detail_input;
	[[nodiscard]] auto detail_for_line(const diff_semantic_index& aIndex,
									   source_line_ref            aLine) -> line_detail_input;

	/// \brief Describes the change segment projection record.
	struct change_segment_projection
	{
		alignment_segment_id Id;
		alignment_kind       Kind = alignment_kind::equal;
		line_span            Left;
		line_span            Right;

		auto operator<=>(const change_segment_projection&) const = default;
	};

	/// \brief Describes the semantic change projection record.
	struct semantic_change_projection
	{
		change_group                           Group;
		std::vector<change_segment_projection> Segments;
		std::vector<move_relation_id>          Moves;

		auto operator<=>(const semantic_change_projection&) const = default;
	};

	/// \brief Describes the change projection record.
	struct change_projection
	{
		file_comparison_digest                  Comparison;
		std::vector<semantic_change_projection> Changes;
		semantic_lookup_reason                  Reason     = semantic_lookup_reason::invalid_result;
		diff_reason_code                        Diagnostic = diff_reason_code::none;

		[[nodiscard]] constexpr auto available() const noexcept -> bool
		{
			return Reason == semantic_lookup_reason::none;
		}

		auto operator<=>(const change_projection&) const = default;
	};

	/// \brief Computes the project changes result.
	[[nodiscard]] auto project_changes(const computed_file_diff& aDiff)
		-> change_projection;
	[[nodiscard]] auto project_changes(const diff_semantic_index& aIndex)
		-> change_projection;

	enum class change_navigation_direction : std::uint8_t {
		previous,
		next,
	};

	enum class change_navigation_policy : std::uint8_t {
		stop,
		wrap,
	};

	/// \brief [[BPR-1532]]. How far one change-navigation gesture reaches.
	///
	/// \details `adjacent` is Previous and Next change: the neighbouring
	/// change, bounded or wrapped by the policy. `outermost` is First and Last
	/// change: the first change for `previous` and the last for `next`, from
	/// any selection; the policy does not apply.
	enum class change_navigation_reach : std::uint8_t {
		adjacent,
		outermost,
	};

	enum class change_navigation_outcome : std::uint8_t {
		selected,
		at_start,
		at_end,
		wrapped,
		no_changes,
		invalid_selection,
	};

	/// \brief Describes the change navigation result record.
	struct change_navigation_result
	{
		change_navigation_outcome      Outcome = change_navigation_outcome::invalid_selection;
		std::optional<change_group_id> Group;
		std::optional<std::uint32_t>   Index;

		auto operator<=>(const change_navigation_result&) const = default;
	};

	/// \brief Computes the navigate change result.
	[[nodiscard]] auto navigate_change(const computed_file_diff&      aDiff,
									   std::optional<change_group_id> aCurrent,
									   change_navigation_direction    aDirection,
									   change_navigation_policy       aPolicy)
		-> change_navigation_result;
	[[nodiscard]] auto navigate_change(const diff_semantic_index&     aIndex,
									   std::optional<change_group_id> aCurrent,
									   change_navigation_direction    aDirection,
									   change_navigation_policy       aPolicy)
		-> change_navigation_result;

	/// \brief [[BPR-1532]]. The first or the last change of this comparison.
	///
	/// \details `previous` reaches the first change group and `next` the last,
	/// whatever `aCurrent` is. When `aCurrent` already names that group the
	/// outcome is `at_start` or `at_end` and still carries it, so the caller
	/// can keep and reveal it. A comparison without changes answers
	/// `no_changes`; an invalid result or direction fails closed.
	[[nodiscard]] auto navigate_outermost_change(const diff_semantic_index&     aIndex,
												 std::optional<change_group_id> aCurrent,
												 change_navigation_direction    aDirection)
		-> change_navigation_result;

	/// \brief Navigates from an exact source line in this immutable comparison.
	/// \details A changed line steps from its group. An unchanged line chooses the
	/// nearest group on the requested side of its alignment segment, including
	/// one-sided changes at its boundaries. An unchanged line at a stop bound
	/// carries no target; wrap remains explicit. Invalid/stale lines fail closed.
	[[nodiscard]] auto navigate_change(const diff_semantic_index&  aIndex,
									   source_line_ref             aCurrent,
									   change_navigation_direction aDirection,
									   change_navigation_policy    aPolicy)
		-> change_navigation_result;

	enum class semantic_selection_kind : std::uint8_t {
		line,
		group,
	};

	/// \brief Describes the semantic selection record.
	struct semantic_selection
	{
		file_comparison_digest              Comparison;
		semantic_selection_kind             Kind = semantic_selection_kind::line;
		source_line_ref                     PreferredLine;
		std::optional<alignment_segment_id> Segment;
		std::optional<change_group_id>      Group;

		auto operator<=>(const semantic_selection&) const = default;
	};

	/// \brief Computes the select line result.
	[[nodiscard]] auto select_line(const computed_file_diff& aDiff,
								   source_line_ref           aLine)
		-> std::optional<semantic_selection>;
	[[nodiscard]] auto select_line(const diff_semantic_index& aIndex,
								   source_line_ref            aLine)
		-> std::optional<semantic_selection>;
	/// \brief Computes the preferred group line result.
	[[nodiscard]] auto preferred_group_line(const computed_file_diff& aDiff,
											change_group_id           aGroup)
		-> std::optional<source_line_ref>;
	[[nodiscard]] auto preferred_group_line(const diff_semantic_index& aIndex,
											change_group_id            aGroup)
		-> std::optional<source_line_ref>;
	/// \brief Computes the select group result.
	[[nodiscard]] auto select_group(const computed_file_diff& aDiff,
									change_group_id           aGroup)
		-> std::optional<semantic_selection>;
	[[nodiscard]] auto select_group(const diff_semantic_index& aIndex,
									change_group_id            aGroup)
		-> std::optional<semantic_selection>;

	enum class selection_reconciliation_outcome : std::uint8_t {
		preserved,
		cleared,
	};

	/// \brief Describes the selection reconciliation result record.
	struct selection_reconciliation_result
	{
		selection_reconciliation_outcome  Outcome = selection_reconciliation_outcome::cleared;
		std::optional<semantic_selection> Selection;

		auto operator<=>(const selection_reconciliation_result&) const = default;
	};

	[[nodiscard]] auto
	reconcile_selection(const computed_file_diff&                aReplacement,
						const std::optional<semantic_selection>& aCurrent)
		-> selection_reconciliation_result;
	[[nodiscard]] auto
	reconcile_selection(const diff_semantic_index&               aReplacement,
						const std::optional<semantic_selection>& aCurrent)
		-> selection_reconciliation_result;
	/// \brief Computes the detail for selection result.
	[[nodiscard]] auto detail_for_selection(const computed_file_diff& aDiff,
											const semantic_selection& aSelection)
		-> line_detail_input;
	[[nodiscard]] auto detail_for_selection(const diff_semantic_index& aIndex,
											const semantic_selection&  aSelection)
		-> line_detail_input;

	/// \brief [[BPR-1334]]. What one row of Line details stands for.
	enum class line_details_row_kind : std::uint8_t {
		/// Both versions of the line are the same text.
		unchanged,
		/// A paired line whose text differs; its emphasis is the change.
		changed,
		/// A base line with no line in head.
		removed,
		/// A head line with no line in base.
		added,
		/// The side that has no version of the selected line.
		absent,
	};

	/// \brief [[BPR-1334]]. One side's version of one line, ready to draw.
	///
	/// \details `Content` is the shown part of the line's bytes in its side's
	/// document. A line longer than the limit is cut at a character boundary;
	/// `OmittedBytes` counts the bytes after the cut, which needs no walk over
	/// them, so a very long line costs no more than a short one. Each
	/// `Emphasis` span is relative to `Content.Begin`, nonempty, ascending and
	/// inside the shown part, so a highlight never reaches into the cut.
	///
	/// An `absent` row has no line and no content. It says where the other
	/// side's line belongs on its side: the lines it replaces, or the boundary
	/// it is inserted at.
	struct line_details_row
	{
		diff_side                    Side = diff_side::left;
		line_details_row_kind        Kind = line_details_row_kind::unchanged;
		std::optional<std::uint32_t> Line;
		byte_span                    Content;
		std::uint64_t                OmittedBytes = 0U;
		std::vector<byte_span>       Emphasis;
		std::optional<line_span>     Replaces;
		std::optional<std::uint32_t> Boundary;

		auto operator<=>(const line_details_row&) const = default;
	};

	/// \brief [[BPR-1334]]. The bounds Line details works within.
	///
	/// \details The defaults are the product's. A selection longer than
	/// `MaximumLines` shows its first lines and counts the rest; a line longer
	/// than `MaximumLineBytes` is cut. Both keep one projection's work and its
	/// painted text bounded however large the comparison is.
	struct line_details_limits
	{
		std::uint32_t MaximumLines     = 16U;
		std::uint64_t MaximumLineBytes = 4096U;

		auto operator<=>(const line_details_limits&) const = default;
	};

	/// \brief [[BPR-1334]]. Line details for a selection: base over head.
	///
	/// \details Two rows per shown line, base first. `OmittedLines` counts the
	/// selected lines past `MaximumLines`. A line the comparison cannot map
	/// adds no rows. An unpaired line that replaces exactly one line shows
	/// that line as its other row, removed over added, with no emphasis; one
	/// that replaces several lines shows an absent row naming them.
	struct line_details_projection
	{
		std::vector<line_details_row> Rows;
		std::uint32_t                 OmittedLines = 0U;

		auto operator<=>(const line_details_projection&) const = default;
	};

	/// \brief [[BPR-1334]]. Projects the lines `aLines` of side `aSide`.
	///
	/// \details Uses the index's line mapping and the intraline edits the
	/// comparison already computed; it computes no new difference. An empty
	/// span, a span past the side's end or a non-text comparison yields no
	/// rows. Work is bounded by `aLimits`, not by the comparison's size.
	[[nodiscard]] auto line_details(const diff_semantic_index& aIndex,
									diff_side                  aSide,
									line_span                  aLines,
									line_details_limits        aLimits = { })
		-> line_details_projection;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_DIFF_SEMANTICS_HPP
