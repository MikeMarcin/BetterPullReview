#include "bpr/core/diff_semantics.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <ranges>

namespace bpr::core
{
namespace
{
[[nodiscard]] constexpr auto valid_side(const diff_side side) noexcept -> bool
{
    return side == diff_side::left || side == diff_side::right;
}

[[nodiscard]] constexpr auto other_side(const diff_side side) noexcept
    -> diff_side
{
    return side == diff_side::left ? diff_side::right : diff_side::left;
}

[[nodiscard]] constexpr auto span_for(const alignment_segment& segment,
                                      const diff_side side) noexcept
    -> line_span
{
    return side == diff_side::left ? segment.Left : segment.Right;
}

[[nodiscard]] auto unavailable(const computed_file_diff& diff,
                               const source_line_ref line,
                               const semantic_lookup_reason reason)
    -> line_mapping
{
    return line_mapping{
        .Kind = line_mapping_kind::unavailable,
        .Source = line,
        .Reason = reason,
        .Diagnostic = diff.Alignment.Diagnostic.Code,
    };
}

[[nodiscard]] auto find_segment(const file_alignment& alignment,
                                const source_line_ref line)
    -> const alignment_segment*
{
    for(const auto& segment : alignment.Segments)
    {
        const auto span = span_for(segment, line.Side);
        if(span.Begin <= line.Line && line.Line < span.End)
        {
            return &segment;
        }
    }
    return nullptr;
}

[[nodiscard]] auto find_pair(const file_alignment& alignment,
                             const alignment_segment_id segment,
                             const source_line_ref line) -> const line_pair*
{
    for(const auto& pair : alignment.Pairs)
    {
        if(pair.Segment != segment)
        {
            continue;
        }
        const auto selected =
            line.Side == diff_side::left ? pair.LeftLine : pair.RightLine;
        if(selected == line.Line)
        {
            return &pair;
        }
    }
    return nullptr;
}

[[nodiscard]] auto find_segment_by_id(const file_alignment& alignment,
                                      const alignment_segment_id id)
    -> const alignment_segment*
{
    const auto found =
        std::ranges::find(alignment.Segments, id, &alignment_segment::Id);
    return found == alignment.Segments.end() ? nullptr : &*found;
}

[[nodiscard]] auto find_group(const file_alignment& alignment,
                              const change_group_id id) -> const change_group*
{
    const auto found =
        std::ranges::find(alignment.Groups, id, &change_group::Id);
    return found == alignment.Groups.end() ? nullptr : &*found;
}

[[nodiscard]] auto valid_semantic_result(const computed_file_diff& diff) -> bool
{
    return validate_computed_file_diff(diff).valid() &&
           diff.Alignment.Content == diff_content_state::text;
}
} // namespace

auto map_line(const computed_file_diff& diff, const source_line_ref line)
    -> line_mapping
{
    if(!validate_computed_file_diff(diff).valid())
    {
        return unavailable(diff, line, semantic_lookup_reason::invalid_result);
    }
    if(line.Comparison != diff.Alignment.Digest)
    {
        return unavailable(
            diff, line, semantic_lookup_reason::comparison_mismatch);
    }
    if(!valid_side(line.Side))
    {
        return unavailable(diff, line, semantic_lookup_reason::invalid_side);
    }
    if(diff.Alignment.Content != diff_content_state::text)
    {
        return unavailable(diff, line, semantic_lookup_reason::non_text);
    }
    if(!valid(
           line, diff.Alignment.LeftLineCount, diff.Alignment.RightLineCount))
    {
        return unavailable(
            diff, line, semantic_lookup_reason::line_out_of_range);
    }

    const auto* const segment = find_segment(diff.Alignment, line);
    if(segment == nullptr)
    {
        return unavailable(
            diff, line, semantic_lookup_reason::segment_not_found);
    }

    auto result = line_mapping{
        .Source = line,
        .Segment = segment->Id,
        .Group = segment->Group,
    };
    if(segment->Kind == alignment_kind::equal)
    {
        const auto selected = span_for(*segment, line.Side);
        const auto opposing = span_for(*segment, other_side(line.Side));
        const auto offset = line.Line - selected.Begin;
        result.Kind = line_mapping_kind::exact_line;
        result.Counterpart = source_line_ref{
            .Comparison = line.Comparison,
            .Side = other_side(line.Side),
            .Line = static_cast<std::uint32_t>(opposing.Begin + offset),
        };
        return result;
    }

    if(segment->Kind == alignment_kind::replacement)
    {
        if(const auto* const pair =
               find_pair(diff.Alignment, segment->Id, line);
           pair != nullptr)
        {
            result.Kind = pair->Kind == line_pair_kind::exact
                              ? line_mapping_kind::exact_line
                              : line_mapping_kind::similar_line;
            result.Counterpart = source_line_ref{
                .Comparison = line.Comparison,
                .Side = other_side(line.Side),
                .Line = line.Side == diff_side::left ? pair->RightLine
                                                     : pair->LeftLine,
            };
            result.Pair = pair->Id;
            return result;
        }

        result.Kind = line_mapping_kind::opposing_span;
        result.OpposingSpan = source_span_ref{
            .Comparison = line.Comparison,
            .Side = other_side(line.Side),
            .Lines = span_for(*segment, other_side(line.Side)),
        };
        return result;
    }

    const auto opposing = span_for(*segment, other_side(line.Side));
    result.Kind = line_mapping_kind::opposing_boundary;
    result.OpposingBoundary = source_boundary_ref{
        .Comparison = line.Comparison,
        .Side = other_side(line.Side),
        .Boundary = opposing.Begin,
    };
    return result;
}

auto move_for_line(const computed_file_diff& diff, const source_line_ref line)
    -> std::optional<move_correspondence>
{
    if(!valid_semantic_result(diff) ||
       line.Comparison != diff.Alignment.Digest || !valid_side(line.Side) ||
       !valid(
           line, diff.Alignment.LeftLineCount, diff.Alignment.RightLineCount))
    {
        return std::nullopt;
    }
    const auto* const selected_segment = find_segment(diff.Alignment, line);
    if(selected_segment == nullptr)
    {
        return std::nullopt;
    }

    for(const auto& move : diff.Alignment.Moves)
    {
        const auto selected_is_source =
            line.Side == diff_side::left &&
            selected_segment->Id == move.SourceDeletion;
        const auto selected_is_destination =
            line.Side == diff_side::right &&
            selected_segment->Id == move.DestinationInsertion;
        if(!selected_is_source && !selected_is_destination)
        {
            continue;
        }
        const auto* const source =
            find_segment_by_id(diff.Alignment, move.SourceDeletion);
        const auto* const destination =
            find_segment_by_id(diff.Alignment, move.DestinationInsertion);
        if(source == nullptr || destination == nullptr)
        {
            return std::nullopt;
        }
        const auto offset = selected_is_source
                                ? line.Line - source->Left.Begin
                                : line.Line - destination->Right.Begin;
        return move_correspondence{
            .Relation = move.Id,
            .SourceDeletion = move.SourceDeletion,
            .DestinationInsertion = move.DestinationInsertion,
            .Selected = line,
            .Counterpart =
                source_line_ref{
                    .Comparison = line.Comparison,
                    .Side =
                        selected_is_source ? diff_side::right : diff_side::left,
                    .Line = static_cast<std::uint32_t>(
                        (selected_is_source ? destination->Right.Begin
                                            : source->Left.Begin) +
                        offset),
                },
        };
    }
    return std::nullopt;
}

auto detail_for_mapping(const computed_file_diff& diff,
                        const line_mapping& mapping) -> line_detail_input
{
    auto detail = line_detail_input{
        .Selected = mapping.Source,
        .Reason = mapping.Reason,
        .Diagnostic = mapping.Diagnostic,
    };
    if(!validate_computed_file_diff(diff).valid() ||
       map_line(diff, mapping.Source) != mapping)
    {
        return line_detail_input{
            .Selected = mapping.Source,
            .Reason = semantic_lookup_reason::invalid_result,
            .Diagnostic = diff.Alignment.Diagnostic.Code,
        };
    }
    detail.Segment = mapping.Segment;
    detail.Group = mapping.Group;
    detail.Pair = mapping.Pair;
    if(mapping.Kind != line_mapping_kind::unavailable)
    {
        if(mapping.Source.Side == diff_side::left)
        {
            detail.LeftLine = mapping.Source;
        }
        else
        {
            detail.RightLine = mapping.Source;
        }
    }

    switch(mapping.Kind)
    {
    case line_mapping_kind::exact_line:
    case line_mapping_kind::similar_line:
        if(!mapping.Counterpart.has_value())
        {
            detail.Reason = semantic_lookup_reason::invalid_result;
            return detail;
        }
        detail.Kind = line_detail_kind::paired_lines;
        if(mapping.Source.Side == diff_side::left)
        {
            detail.LeftLine = mapping.Source;
            detail.RightLine = mapping.Counterpart;
        }
        else
        {
            detail.LeftLine = mapping.Counterpart;
            detail.RightLine = mapping.Source;
        }
        if(mapping.Pair.has_value())
        {
            std::ranges::copy_if(diff.Intraline,
                                 std::back_inserter(detail.Intraline),
                                 [&mapping](const intraline_edit& edit)
                                 { return edit.Pair == *mapping.Pair; });
        }
        break;
    case line_mapping_kind::opposing_span:
        detail.Kind = line_detail_kind::opposing_span;
        detail.OpposingSpan = mapping.OpposingSpan;
        break;
    case line_mapping_kind::opposing_boundary:
        detail.Kind = line_detail_kind::opposing_boundary;
        detail.OpposingBoundary = mapping.OpposingBoundary;
        break;
    case line_mapping_kind::unavailable:
        detail.Kind = line_detail_kind::unavailable;
        break;
    }
    if(const auto move = move_for_line(diff, mapping.Source); move.has_value())
    {
        detail.Move = move->Relation;
    }
    return detail;
}

auto detail_for_line(const computed_file_diff& diff, const source_line_ref line)
    -> line_detail_input
{
    return detail_for_mapping(diff, map_line(diff, line));
}

auto project_changes(const computed_file_diff& diff) -> change_projection
{
    auto result = change_projection{
        .Comparison = diff.Alignment.Digest,
        .Reason = semantic_lookup_reason::none,
        .Diagnostic = diff.Alignment.Diagnostic.Code,
    };
    if(!validate_computed_file_diff(diff).valid())
    {
        result.Reason = semantic_lookup_reason::invalid_result;
        return result;
    }
    if(diff.Alignment.Content != diff_content_state::text)
    {
        result.Reason = semantic_lookup_reason::non_text;
        return result;
    }

    result.Changes.reserve(diff.Alignment.Groups.size());
    for(const auto& group : diff.Alignment.Groups)
    {
        auto projected = semantic_change_projection{.Group = group};
        projected.Segments.reserve(group.Members.End - group.Members.Begin);
        for(auto index = group.Members.Begin; index < group.Members.End;
            ++index)
        {
            const auto& segment = diff.Alignment.Segments[index];
            projected.Segments.push_back(change_segment_projection{
                .Id = segment.Id,
                .Kind = segment.Kind,
                .Left = segment.Left,
                .Right = segment.Right,
            });
            for(const auto& move : diff.Alignment.Moves)
            {
                if(move.SourceDeletion == segment.Id ||
                   move.DestinationInsertion == segment.Id)
                {
                    if(std::ranges::find(projected.Moves, move.Id) ==
                       projected.Moves.end())
                    {
                        projected.Moves.push_back(move.Id);
                    }
                }
            }
        }
        result.Changes.push_back(std::move(projected));
    }
    return result;
}

auto navigate_change(const computed_file_diff& diff,
                     const std::optional<change_group_id> current,
                     const change_navigation_direction direction,
                     const change_navigation_policy policy)
    -> change_navigation_result
{
    if(!valid_semantic_result(diff) ||
       (direction != change_navigation_direction::previous &&
        direction != change_navigation_direction::next) ||
       (policy != change_navigation_policy::stop &&
        policy != change_navigation_policy::wrap))
    {
        return {};
    }
    const auto& groups = diff.Alignment.Groups;
    if(groups.empty())
    {
        return current.has_value()
                   ? change_navigation_result{}
                   : change_navigation_result{
                         .Outcome = change_navigation_outcome::no_changes};
    }
    if(!current.has_value())
    {
        const auto index = direction == change_navigation_direction::next
                               ? std::size_t{0U}
                               : groups.size() - 1U;
        return {
            .Outcome = change_navigation_outcome::selected,
            .Group = groups[index].Id,
            .Index = static_cast<std::uint32_t>(index),
        };
    }
    const auto found = std::ranges::find(groups, *current, &change_group::Id);
    if(found == groups.end())
    {
        return {};
    }
    const auto index =
        static_cast<std::size_t>(std::distance(groups.begin(), found));
    if(direction == change_navigation_direction::next)
    {
        if(index + 1U < groups.size())
        {
            return {
                .Outcome = change_navigation_outcome::selected,
                .Group = groups[index + 1U].Id,
                .Index = static_cast<std::uint32_t>(index + 1U),
            };
        }
        if(policy == change_navigation_policy::wrap)
        {
            return {
                .Outcome = change_navigation_outcome::wrapped,
                .Group = groups.front().Id,
                .Index = 0U,
            };
        }
        return {
            .Outcome = change_navigation_outcome::at_end,
            .Group = found->Id,
            .Index = static_cast<std::uint32_t>(index),
        };
    }

    if(index != 0U)
    {
        return {
            .Outcome = change_navigation_outcome::selected,
            .Group = groups[index - 1U].Id,
            .Index = static_cast<std::uint32_t>(index - 1U),
        };
    }
    if(policy == change_navigation_policy::wrap)
    {
        const auto last = groups.size() - 1U;
        return {
            .Outcome = change_navigation_outcome::wrapped,
            .Group = groups[last].Id,
            .Index = static_cast<std::uint32_t>(last),
        };
    }
    return {
        .Outcome = change_navigation_outcome::at_start,
        .Group = found->Id,
        .Index = 0U,
    };
}

auto select_line(const computed_file_diff& diff, const source_line_ref line)
    -> std::optional<semantic_selection>
{
    const auto mapping = map_line(diff, line);
    if(mapping.Kind == line_mapping_kind::unavailable)
    {
        return std::nullopt;
    }
    return semantic_selection{
        .Comparison = diff.Alignment.Digest,
        .Kind = semantic_selection_kind::line,
        .PreferredLine = line,
        .Segment = mapping.Segment,
        .Group = mapping.Group,
    };
}

auto preferred_group_line(const computed_file_diff& diff,
                          const change_group_id group)
    -> std::optional<source_line_ref>
{
    if(!valid_semantic_result(diff))
    {
        return std::nullopt;
    }
    const auto* const selected_group = find_group(diff.Alignment, group);
    if(selected_group == nullptr)
    {
        return std::nullopt;
    }
    const auto& first = diff.Alignment.Segments[selected_group->Members.Begin];
    if(!first.Left.empty())
    {
        return source_line_ref{
            .Comparison = diff.Alignment.Digest,
            .Side = diff_side::left,
            .Line = first.Left.Begin,
        };
    }
    return source_line_ref{
        .Comparison = diff.Alignment.Digest,
        .Side = diff_side::right,
        .Line = first.Right.Begin,
    };
}

auto select_group(const computed_file_diff& diff, const change_group_id group)
    -> std::optional<semantic_selection>
{
    const auto preferred = preferred_group_line(diff, group);
    if(!preferred.has_value())
    {
        return std::nullopt;
    }
    const auto mapping = map_line(diff, *preferred);
    if(mapping.Kind == line_mapping_kind::unavailable ||
       mapping.Group != group || !mapping.Segment.has_value())
    {
        return std::nullopt;
    }
    return semantic_selection{
        .Comparison = diff.Alignment.Digest,
        .Kind = semantic_selection_kind::group,
        .PreferredLine = *preferred,
        .Segment = mapping.Segment,
        .Group = group,
    };
}

auto reconcile_selection(const computed_file_diff& replacement,
                         const std::optional<semantic_selection>& current)
    -> selection_reconciliation_result
{
    if(!current.has_value() || !valid_semantic_result(replacement) ||
       current->Comparison != replacement.Alignment.Digest ||
       current->PreferredLine.Comparison != replacement.Alignment.Digest)
    {
        return {};
    }
    if(current->Kind == semantic_selection_kind::line)
    {
        const auto selected = select_line(replacement, current->PreferredLine);
        if(!selected.has_value() || *selected != *current)
        {
            return {};
        }
        return {
            .Outcome = selection_reconciliation_outcome::preserved,
            .Selection = current,
        };
    }
    if(current->Kind != semantic_selection_kind::group ||
       !current->Group.has_value())
    {
        return {};
    }
    const auto selected = select_group(replacement, *current->Group);
    if(!selected.has_value() || *selected != *current)
    {
        return {};
    }
    return {
        .Outcome = selection_reconciliation_outcome::preserved,
        .Selection = current,
    };
}

auto detail_for_selection(const computed_file_diff& diff,
                          const semantic_selection& selection)
    -> line_detail_input
{
    auto canonical = std::optional<semantic_selection>{};
    switch(selection.Kind)
    {
    case semantic_selection_kind::line:
        canonical = select_line(diff, selection.PreferredLine);
        break;
    case semantic_selection_kind::group:
        if(selection.Group.has_value())
        {
            canonical = select_group(diff, *selection.Group);
        }
        break;
    }
    if(!canonical.has_value() || *canonical != selection)
    {
        return line_detail_input{
            .Selected = selection.PreferredLine,
            .Reason = semantic_lookup_reason::invalid_result,
            .Diagnostic = diff.Alignment.Diagnostic.Code,
        };
    }
    return detail_for_line(diff, selection.PreferredLine);
}
} // namespace bpr::core
