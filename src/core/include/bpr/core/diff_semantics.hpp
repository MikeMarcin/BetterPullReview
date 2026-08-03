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
		diff_semantic_index() = default;

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
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_DIFF_SEMANTICS_HPP
