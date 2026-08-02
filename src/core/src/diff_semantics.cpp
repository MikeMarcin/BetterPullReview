#include "bpr/core/diff_semantics.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <ranges>

namespace bpr::core {
	namespace {
		[[nodiscard]] constexpr auto valid_side(const diff_side aSide) noexcept -> bool
		{
			return aSide == diff_side::left || aSide == diff_side::right;
		}

		[[nodiscard]] constexpr auto other_side(const diff_side aSide) noexcept
			-> diff_side
		{
			return aSide == diff_side::left ? diff_side::right : diff_side::left;
		}

		[[nodiscard]] constexpr auto span_for(const alignment_segment& aSegment,
											  const diff_side          aSide) noexcept
			-> line_span
		{
			return aSide == diff_side::left ? aSegment.Left : aSegment.Right;
		}

		[[nodiscard]] auto unavailable(const computed_file_diff&    aDiff,
									   const source_line_ref        aLine,
									   const semantic_lookup_reason aReason)
			-> line_mapping
		{
			return line_mapping{
				.Kind       = line_mapping_kind::unavailable,
				.Source     = aLine,
				.Reason     = aReason,
				.Diagnostic = aDiff.Alignment.Diagnostic.Code,
			};
		}

		[[nodiscard]] auto find_segment(const file_alignment& aAlignment,
										const source_line_ref aLine)
			-> const alignment_segment*
		{
			for (const auto& segment : aAlignment.Segments) {
				const auto span = span_for(segment, aLine.Side);
				if (span.Begin <= aLine.Line && aLine.Line < span.End) {
					return &segment;
				}
			}
			return nullptr;
		}

		[[nodiscard]] auto find_pair(const file_alignment&      aAlignment,
									 const alignment_segment_id aSegment,
									 const source_line_ref      aLine) -> const line_pair*
		{
			for (const auto& pair : aAlignment.Pairs) {
				if (pair.Segment != aSegment) {
					continue;
				}
				const auto selected = aLine.Side == diff_side::left ? pair.LeftLine : pair.RightLine;
				if (selected == aLine.Line) {
					return &pair;
				}
			}
			return nullptr;
		}

		[[nodiscard]] auto find_segment_by_id(const file_alignment&      aAlignment,
											  const alignment_segment_id aId)
			-> const alignment_segment*
		{
			const auto found = std::ranges::find(aAlignment.Segments, aId, &alignment_segment::Id);
			return found == aAlignment.Segments.end() ? nullptr : &*found;
		}

		[[nodiscard]] auto find_group(const file_alignment& aAlignment,
									  const change_group_id aId) -> const change_group*
		{
			const auto found = std::ranges::find(aAlignment.Groups, aId, &change_group::Id);
			return found == aAlignment.Groups.end() ? nullptr : &*found;
		}

		[[nodiscard]] auto valid_semantic_result(const computed_file_diff& aDiff) -> bool
		{
			return validate_computed_file_diff(aDiff).valid() && aDiff.Alignment.Content == diff_content_state::text;
		}
	} // namespace

	auto map_line(const computed_file_diff& aDiff, const source_line_ref aLine)
		-> line_mapping
	{
		if (!validate_computed_file_diff(aDiff).valid()) {
			return unavailable(aDiff, aLine, semantic_lookup_reason::invalid_result);
		}
		if (aLine.Comparison != aDiff.Alignment.Digest) {
			return unavailable(
				aDiff, aLine, semantic_lookup_reason::comparison_mismatch);
		}
		if (!valid_side(aLine.Side)) {
			return unavailable(aDiff, aLine, semantic_lookup_reason::invalid_side);
		}
		if (aDiff.Alignment.Content != diff_content_state::text) {
			return unavailable(aDiff, aLine, semantic_lookup_reason::non_text);
		}
		if (!valid(
				aLine, aDiff.Alignment.LeftLineCount, aDiff.Alignment.RightLineCount)) {
			return unavailable(
				aDiff, aLine, semantic_lookup_reason::line_out_of_range);
		}

		const auto* const segment = find_segment(aDiff.Alignment, aLine);
		if (segment == nullptr) {
			return unavailable(
				aDiff, aLine, semantic_lookup_reason::segment_not_found);
		}

		auto result = line_mapping{
			.Source  = aLine,
			.Segment = segment->Id,
			.Group   = segment->Group,
		};
		if (segment->Kind == alignment_kind::equal) {
			const auto selected = span_for(*segment, aLine.Side);
			const auto opposing = span_for(*segment, other_side(aLine.Side));
			const auto offset   = aLine.Line - selected.Begin;
			result.Kind         = line_mapping_kind::exact_line;
			result.Counterpart  = source_line_ref{
				.Comparison = aLine.Comparison,
				.Side       = other_side(aLine.Side),
				.Line       = static_cast<std::uint32_t>(opposing.Begin + offset),
			};
			return result;
		}

		if (segment->Kind == alignment_kind::replacement) {
			if (const auto* const pair = find_pair(aDiff.Alignment, segment->Id, aLine);
				pair != nullptr) {
				result.Kind        = pair->Kind == line_pair_kind::exact
					? line_mapping_kind::exact_line
					: line_mapping_kind::similar_line;
				result.Counterpart = source_line_ref{
					.Comparison = aLine.Comparison,
					.Side       = other_side(aLine.Side),
					.Line       = aLine.Side == diff_side::left ? pair->RightLine
																: pair->LeftLine,
				};
				result.Pair = pair->Id;
				return result;
			}

			result.Kind         = line_mapping_kind::opposing_span;
			result.OpposingSpan = source_span_ref{
				.Comparison = aLine.Comparison,
				.Side       = other_side(aLine.Side),
				.Lines      = span_for(*segment, other_side(aLine.Side)),
			};
			return result;
		}

		const auto opposing     = span_for(*segment, other_side(aLine.Side));
		result.Kind             = line_mapping_kind::opposing_boundary;
		result.OpposingBoundary = source_boundary_ref{
			.Comparison = aLine.Comparison,
			.Side       = other_side(aLine.Side),
			.Boundary   = opposing.Begin,
		};
		return result;
	}

	auto move_for_line(const computed_file_diff& aDiff, const source_line_ref aLine)
		-> std::optional<move_correspondence>
	{
		if (!valid_semantic_result(aDiff) || aLine.Comparison != aDiff.Alignment.Digest || !valid_side(aLine.Side) || !valid(aLine, aDiff.Alignment.LeftLineCount, aDiff.Alignment.RightLineCount)) {
			return std::nullopt;
		}
		const auto* const selectedSegment = find_segment(aDiff.Alignment, aLine);
		if (selectedSegment == nullptr) {
			return std::nullopt;
		}

		for (const auto& move : aDiff.Alignment.Moves) {
			const auto selectedIsSource      = aLine.Side == diff_side::left && selectedSegment->Id == move.SourceDeletion;
			const auto selectedIsDestination = aLine.Side == diff_side::right && selectedSegment->Id == move.DestinationInsertion;
			if (!selectedIsSource && !selectedIsDestination) {
				continue;
			}
			const auto* const source      = find_segment_by_id(aDiff.Alignment, move.SourceDeletion);
			const auto* const destination = find_segment_by_id(aDiff.Alignment, move.DestinationInsertion);
			if (source == nullptr || destination == nullptr) {
				return std::nullopt;
			}
			const auto offset = selectedIsSource
				? aLine.Line - source->Left.Begin
				: aLine.Line - destination->Right.Begin;
			return move_correspondence{
				.Relation             = move.Id,
				.SourceDeletion       = move.SourceDeletion,
				.DestinationInsertion = move.DestinationInsertion,
				.Selected             = aLine,
				.Counterpart          = source_line_ref{
					.Comparison = aLine.Comparison,
					.Side       = selectedIsSource ? diff_side::right : diff_side::left,
					.Line       = static_cast<std::uint32_t>(
						(selectedIsSource ? destination->Right.Begin
										  : source->Left.Begin)
						+ offset),
				},
			};
		}
		return std::nullopt;
	}

	auto detail_for_mapping(const computed_file_diff& aDiff,
							const line_mapping&       aMapping) -> line_detail_input
	{
		auto detail = line_detail_input{
			.Selected   = aMapping.Source,
			.Reason     = aMapping.Reason,
			.Diagnostic = aMapping.Diagnostic,
		};
		if (!validate_computed_file_diff(aDiff).valid() || map_line(aDiff, aMapping.Source) != aMapping) {
			return line_detail_input{
				.Selected   = aMapping.Source,
				.Reason     = semantic_lookup_reason::invalid_result,
				.Diagnostic = aDiff.Alignment.Diagnostic.Code,
			};
		}
		detail.Segment = aMapping.Segment;
		detail.Group   = aMapping.Group;
		detail.Pair    = aMapping.Pair;
		if (aMapping.Kind != line_mapping_kind::unavailable) {
			if (aMapping.Source.Side == diff_side::left) {
				detail.LeftLine = aMapping.Source;
			} else {
				detail.RightLine = aMapping.Source;
			}
		}

		switch (aMapping.Kind) {
		case line_mapping_kind::exact_line:
		case line_mapping_kind::similar_line:
			if (!aMapping.Counterpart.has_value()) {
				detail.Reason = semantic_lookup_reason::invalid_result;
				return detail;
			}
			detail.Kind = line_detail_kind::paired_lines;
			if (aMapping.Source.Side == diff_side::left) {
				detail.LeftLine  = aMapping.Source;
				detail.RightLine = aMapping.Counterpart;
			} else {
				detail.LeftLine  = aMapping.Counterpart;
				detail.RightLine = aMapping.Source;
			}
			if (aMapping.Pair.has_value()) {
				std::ranges::copy_if(aDiff.Intraline,
									 std::back_inserter(detail.Intraline),
									 [&aMapping](const intraline_edit& aEdit) { return aEdit.Pair == *aMapping.Pair; });
			}
			break;
		case line_mapping_kind::opposing_span:
			detail.Kind         = line_detail_kind::opposing_span;
			detail.OpposingSpan = aMapping.OpposingSpan;
			break;
		case line_mapping_kind::opposing_boundary:
			detail.Kind             = line_detail_kind::opposing_boundary;
			detail.OpposingBoundary = aMapping.OpposingBoundary;
			break;
		case line_mapping_kind::unavailable:
			detail.Kind = line_detail_kind::unavailable;
			break;
		}
		if (const auto move = move_for_line(aDiff, aMapping.Source); move.has_value()) {
			detail.Move = move->Relation;
		}
		return detail;
	}

	auto detail_for_line(const computed_file_diff& aDiff, const source_line_ref aLine)
		-> line_detail_input
	{
		return detail_for_mapping(aDiff, map_line(aDiff, aLine));
	}

	auto project_changes(const computed_file_diff& aDiff) -> change_projection
	{
		auto result = change_projection{
			.Comparison = aDiff.Alignment.Digest,
			.Reason     = semantic_lookup_reason::none,
			.Diagnostic = aDiff.Alignment.Diagnostic.Code,
		};
		if (!validate_computed_file_diff(aDiff).valid()) {
			result.Reason = semantic_lookup_reason::invalid_result;
			return result;
		}
		if (aDiff.Alignment.Content != diff_content_state::text) {
			result.Reason = semantic_lookup_reason::non_text;
			return result;
		}

		result.Changes.reserve(aDiff.Alignment.Groups.size());
		for (const auto& group : aDiff.Alignment.Groups) {
			auto projected = semantic_change_projection{ .Group = group };
			projected.Segments.reserve(group.Members.End - group.Members.Begin);
			for (auto index = group.Members.Begin; index < group.Members.End;
				 ++index) {
				const auto& segment = aDiff.Alignment.Segments[index];
				projected.Segments.push_back(change_segment_projection{
					.Id    = segment.Id,
					.Kind  = segment.Kind,
					.Left  = segment.Left,
					.Right = segment.Right,
				});
				for (const auto& move : aDiff.Alignment.Moves) {
					if (move.SourceDeletion == segment.Id || move.DestinationInsertion == segment.Id) {
						if (std::ranges::find(projected.Moves, move.Id) == projected.Moves.end()) {
							projected.Moves.push_back(move.Id);
						}
					}
				}
			}
			result.Changes.push_back(std::move(projected));
		}
		return result;
	}

	auto navigate_change(const computed_file_diff&            aDiff,
						 const std::optional<change_group_id> aCurrent,
						 const change_navigation_direction    aDirection,
						 const change_navigation_policy       aPolicy)
		-> change_navigation_result
	{
		if (!valid_semantic_result(aDiff) || (aDirection != change_navigation_direction::previous && aDirection != change_navigation_direction::next) || (aPolicy != change_navigation_policy::stop && aPolicy != change_navigation_policy::wrap)) {
			return { };
		}
		const auto& groups = aDiff.Alignment.Groups;
		if (groups.empty()) {
			return aCurrent.has_value()
				? change_navigation_result{ }
				: change_navigation_result{
					  .Outcome = change_navigation_outcome::no_changes
				  };
		}
		if (!aCurrent.has_value()) {
			const auto index = aDirection == change_navigation_direction::next
				? std::size_t{ 0U }
				: groups.size() - 1U;
			return {
				.Outcome = change_navigation_outcome::selected,
				.Group   = groups[index].Id,
				.Index   = static_cast<std::uint32_t>(index),
			};
		}
		const auto found = std::ranges::find(groups, *aCurrent, &change_group::Id);
		if (found == groups.end()) {
			return { };
		}
		const auto index = static_cast<std::size_t>(std::distance(groups.begin(), found));
		if (aDirection == change_navigation_direction::next) {
			if (index + 1U < groups.size()) {
				return {
					.Outcome = change_navigation_outcome::selected,
					.Group   = groups[index + 1U].Id,
					.Index   = static_cast<std::uint32_t>(index + 1U),
				};
			}
			if (aPolicy == change_navigation_policy::wrap) {
				return {
					.Outcome = change_navigation_outcome::wrapped,
					.Group   = groups.front().Id,
					.Index   = 0U,
				};
			}
			return {
				.Outcome = change_navigation_outcome::at_end,
				.Group   = found->Id,
				.Index   = static_cast<std::uint32_t>(index),
			};
		}

		if (index != 0U) {
			return {
				.Outcome = change_navigation_outcome::selected,
				.Group   = groups[index - 1U].Id,
				.Index   = static_cast<std::uint32_t>(index - 1U),
			};
		}
		if (aPolicy == change_navigation_policy::wrap) {
			const auto last = groups.size() - 1U;
			return {
				.Outcome = change_navigation_outcome::wrapped,
				.Group   = groups[last].Id,
				.Index   = static_cast<std::uint32_t>(last),
			};
		}
		return {
			.Outcome = change_navigation_outcome::at_start,
			.Group   = found->Id,
			.Index   = 0U,
		};
	}

	auto select_line(const computed_file_diff& aDiff, const source_line_ref aLine)
		-> std::optional<semantic_selection>
	{
		const auto mapping = map_line(aDiff, aLine);
		if (mapping.Kind == line_mapping_kind::unavailable) {
			return std::nullopt;
		}
		return semantic_selection{
			.Comparison    = aDiff.Alignment.Digest,
			.Kind          = semantic_selection_kind::line,
			.PreferredLine = aLine,
			.Segment       = mapping.Segment,
			.Group         = mapping.Group,
		};
	}

	auto preferred_group_line(const computed_file_diff& aDiff,
							  const change_group_id     aGroup)
		-> std::optional<source_line_ref>
	{
		if (!valid_semantic_result(aDiff)) {
			return std::nullopt;
		}
		const auto* const selectedGroup = find_group(aDiff.Alignment, aGroup);
		if (selectedGroup == nullptr) {
			return std::nullopt;
		}
		const auto& first = aDiff.Alignment.Segments[selectedGroup->Members.Begin];
		if (!first.Left.empty()) {
			return source_line_ref{
				.Comparison = aDiff.Alignment.Digest,
				.Side       = diff_side::left,
				.Line       = first.Left.Begin,
			};
		}
		return source_line_ref{
			.Comparison = aDiff.Alignment.Digest,
			.Side       = diff_side::right,
			.Line       = first.Right.Begin,
		};
	}

	auto select_group(const computed_file_diff& aDiff, const change_group_id aGroup)
		-> std::optional<semantic_selection>
	{
		const auto preferred = preferred_group_line(aDiff, aGroup);
		if (!preferred.has_value()) {
			return std::nullopt;
		}
		const auto mapping = map_line(aDiff, *preferred);
		if (mapping.Kind == line_mapping_kind::unavailable || mapping.Group != aGroup || !mapping.Segment.has_value()) {
			return std::nullopt;
		}
		return semantic_selection{
			.Comparison    = aDiff.Alignment.Digest,
			.Kind          = semantic_selection_kind::group,
			.PreferredLine = *preferred,
			.Segment       = mapping.Segment,
			.Group         = aGroup,
		};
	}

	auto reconcile_selection(const computed_file_diff&                aReplacement,
							 const std::optional<semantic_selection>& aCurrent)
		-> selection_reconciliation_result
	{
		if (!aCurrent.has_value() || !valid_semantic_result(aReplacement) || aCurrent->Comparison != aReplacement.Alignment.Digest || aCurrent->PreferredLine.Comparison != aReplacement.Alignment.Digest) {
			return { };
		}
		if (aCurrent->Kind == semantic_selection_kind::line) {
			const auto selected = select_line(aReplacement, aCurrent->PreferredLine);
			if (!selected.has_value() || *selected != *aCurrent) {
				return { };
			}
			return {
				.Outcome   = selection_reconciliation_outcome::preserved,
				.Selection = aCurrent,
			};
		}
		if (aCurrent->Kind != semantic_selection_kind::group || !aCurrent->Group.has_value()) {
			return { };
		}
		const auto selected = select_group(aReplacement, *aCurrent->Group);
		if (!selected.has_value() || *selected != *aCurrent) {
			return { };
		}
		return {
			.Outcome   = selection_reconciliation_outcome::preserved,
			.Selection = aCurrent,
		};
	}

	auto detail_for_selection(const computed_file_diff& aDiff,
							  const semantic_selection& aSelection)
		-> line_detail_input
	{
		auto canonical = std::optional<semantic_selection>{ };
		switch (aSelection.Kind) {
		case semantic_selection_kind::line:
			canonical = select_line(aDiff, aSelection.PreferredLine);
			break;
		case semantic_selection_kind::group:
			if (aSelection.Group.has_value()) {
				canonical = select_group(aDiff, *aSelection.Group);
			}
			break;
		}
		if (!canonical.has_value() || *canonical != aSelection) {
			return line_detail_input{
				.Selected   = aSelection.PreferredLine,
				.Reason     = semantic_lookup_reason::invalid_result,
				.Diagnostic = aDiff.Alignment.Diagnostic.Code,
			};
		}
		return detail_for_line(aDiff, aSelection.PreferredLine);
	}
} // namespace bpr::core
