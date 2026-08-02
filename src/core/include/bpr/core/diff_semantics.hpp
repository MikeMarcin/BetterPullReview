#pragma once

#include "bpr/core/diff_compute.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <vector>

namespace bpr::core
{
struct source_span_ref
{
    file_comparison_digest Comparison;
    diff_side Side = diff_side::left;
    line_span Lines;

    auto operator<=>(const source_span_ref&) const = default;
};

enum class line_mapping_kind : std::uint8_t
{
    exact_line,
    similar_line,
    opposing_span,
    opposing_boundary,
    unavailable,
};

enum class semantic_lookup_reason : std::uint8_t
{
    none,
    invalid_result,
    comparison_mismatch,
    invalid_side,
    line_out_of_range,
    non_text,
    segment_not_found,
};

struct line_mapping
{
    line_mapping_kind Kind = line_mapping_kind::unavailable;
    source_line_ref Source;
    std::optional<source_line_ref> Counterpart;
    std::optional<source_span_ref> OpposingSpan;
    std::optional<source_boundary_ref> OpposingBoundary;
    std::optional<alignment_segment_id> Segment;
    std::optional<change_group_id> Group;
    std::optional<line_pair_id> Pair;
    semantic_lookup_reason Reason = semantic_lookup_reason::none;
    diff_reason_code Diagnostic = diff_reason_code::none;

    auto operator<=>(const line_mapping&) const = default;
};

struct move_correspondence
{
    move_relation_id Relation;
    alignment_segment_id SourceDeletion;
    alignment_segment_id DestinationInsertion;
    source_line_ref Selected;
    source_line_ref Counterpart;

    auto operator<=>(const move_correspondence&) const = default;
};

[[nodiscard]] auto map_line(const computed_file_diff& diff,
                            source_line_ref line) -> line_mapping;
[[nodiscard]] auto move_for_line(const computed_file_diff& diff,
                                 source_line_ref line)
    -> std::optional<move_correspondence>;

enum class line_detail_kind : std::uint8_t
{
    paired_lines,
    opposing_span,
    opposing_boundary,
    unavailable,
};

struct line_detail_input
{
    line_detail_kind Kind = line_detail_kind::unavailable;
    source_line_ref Selected;
    std::optional<source_line_ref> LeftLine;
    std::optional<source_line_ref> RightLine;
    std::optional<source_span_ref> OpposingSpan;
    std::optional<source_boundary_ref> OpposingBoundary;
    std::optional<alignment_segment_id> Segment;
    std::optional<change_group_id> Group;
    std::optional<line_pair_id> Pair;
    std::optional<move_relation_id> Move;
    std::vector<intraline_edit> Intraline;
    semantic_lookup_reason Reason = semantic_lookup_reason::none;
    diff_reason_code Diagnostic = diff_reason_code::none;

    auto operator<=>(const line_detail_input&) const = default;
};

[[nodiscard]] auto detail_for_mapping(const computed_file_diff& diff,
                                      const line_mapping& mapping)
    -> line_detail_input;
[[nodiscard]] auto detail_for_line(const computed_file_diff& diff,
                                   source_line_ref line) -> line_detail_input;

struct change_segment_projection
{
    alignment_segment_id Id;
    alignment_kind Kind = alignment_kind::equal;
    line_span Left;
    line_span Right;

    auto operator<=>(const change_segment_projection&) const = default;
};

struct semantic_change_projection
{
    change_group Group;
    std::vector<change_segment_projection> Segments;
    std::vector<move_relation_id> Moves;

    auto operator<=>(const semantic_change_projection&) const = default;
};

struct change_projection
{
    file_comparison_digest Comparison;
    std::vector<semantic_change_projection> Changes;
    semantic_lookup_reason Reason = semantic_lookup_reason::invalid_result;
    diff_reason_code Diagnostic = diff_reason_code::none;

    [[nodiscard]] constexpr auto available() const noexcept -> bool
    {
        return Reason == semantic_lookup_reason::none;
    }

    auto operator<=>(const change_projection&) const = default;
};

[[nodiscard]] auto project_changes(const computed_file_diff& diff)
    -> change_projection;

enum class change_navigation_direction : std::uint8_t
{
    previous,
    next,
};

enum class change_navigation_policy : std::uint8_t
{
    stop,
    wrap,
};

enum class change_navigation_outcome : std::uint8_t
{
    selected,
    at_start,
    at_end,
    wrapped,
    no_changes,
    invalid_selection,
};

struct change_navigation_result
{
    change_navigation_outcome Outcome =
        change_navigation_outcome::invalid_selection;
    std::optional<change_group_id> Group;
    std::optional<std::uint32_t> Index;

    auto operator<=>(const change_navigation_result&) const = default;
};

[[nodiscard]] auto navigate_change(const computed_file_diff& diff,
                                   std::optional<change_group_id> current,
                                   change_navigation_direction direction,
                                   change_navigation_policy policy)
    -> change_navigation_result;

enum class semantic_selection_kind : std::uint8_t
{
    line,
    group,
};

struct semantic_selection
{
    file_comparison_digest Comparison;
    semantic_selection_kind Kind = semantic_selection_kind::line;
    source_line_ref PreferredLine;
    std::optional<alignment_segment_id> Segment;
    std::optional<change_group_id> Group;

    auto operator<=>(const semantic_selection&) const = default;
};

[[nodiscard]] auto select_line(const computed_file_diff& diff,
                               source_line_ref line)
    -> std::optional<semantic_selection>;
[[nodiscard]] auto preferred_group_line(const computed_file_diff& diff,
                                        change_group_id group)
    -> std::optional<source_line_ref>;
[[nodiscard]] auto select_group(const computed_file_diff& diff,
                                change_group_id group)
    -> std::optional<semantic_selection>;

enum class selection_reconciliation_outcome : std::uint8_t
{
    preserved,
    cleared,
};

struct selection_reconciliation_result
{
    selection_reconciliation_outcome Outcome =
        selection_reconciliation_outcome::cleared;
    std::optional<semantic_selection> Selection;

    auto operator<=>(const selection_reconciliation_result&) const = default;
};

[[nodiscard]] auto
reconcile_selection(const computed_file_diff& replacement,
                    const std::optional<semantic_selection>& current)
    -> selection_reconciliation_result;
[[nodiscard]] auto detail_for_selection(const computed_file_diff& diff,
                                        const semantic_selection& selection)
    -> line_detail_input;
} // namespace bpr::core
