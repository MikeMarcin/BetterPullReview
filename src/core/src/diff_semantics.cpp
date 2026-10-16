#include "bpr/core/diff_semantics.hpp"

#include "diff_step_meter.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <limits>
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
			const auto found = std::ranges::upper_bound(
				aAlignment.Segments,
				aLine.Line,
				std::ranges::less{ },
				[aLine](const alignment_segment& aSegment) {
					return span_for(aSegment, aLine.Side).End;
				});
			if (found == aAlignment.Segments.end()) {
				return nullptr;
			}
			const auto span = span_for(*found, aLine.Side);
			return span.Begin <= aLine.Line && aLine.Line < span.End
				? &*found
				: nullptr;
		}

		[[nodiscard]] auto find_pair(const file_alignment&      aAlignment,
									 const alignment_segment_id aSegment,
									 const source_line_ref      aLine) -> const line_pair*
		{
			const auto found = std::ranges::lower_bound(
				aAlignment.Pairs,
				aLine.Line,
				std::ranges::less{ },
				[aLine](const line_pair& aPair) {
					return aLine.Side == diff_side::left ? aPair.LeftLine
														 : aPair.RightLine;
				});
			if (found == aAlignment.Pairs.end()) {
				return nullptr;
			}
			const auto selected = aLine.Side == diff_side::left
				? found->LeftLine
				: found->RightLine;
			return found->Segment == aSegment && selected == aLine.Line
				? &*found
				: nullptr;
		}

		[[nodiscard]] auto find_segment_by_id(const diff_semantic_index& aIndex,
											  const alignment_segment_id aId)
			-> const alignment_segment*
		{
			const auto found = aIndex.segment_index(aId);
			return found.has_value()
				? &aIndex.diff().Alignment.Segments[*found]
				: nullptr;
		}

		[[nodiscard]] auto find_group(const diff_semantic_index& aIndex,
									  const change_group_id      aId) -> const change_group*
		{
			const auto found = aIndex.group_index(aId);
			return found.has_value()
				? &aIndex.diff().Alignment.Groups[*found]
				: nullptr;
		}

		[[nodiscard]] auto valid_semantic_result(const diff_semantic_index& aIndex) -> bool
		{
			return aIndex.diff().Alignment.Content == diff_content_state::text;
		}

		template <typename Identity>
		[[nodiscard]] auto indexed_record(
			const std::vector<std::pair<Identity, std::uint32_t>>& aEntries,
			const Identity                                         aIdentity) noexcept
			-> std::optional<std::size_t>
		{
			const auto found = std::ranges::lower_bound(
				aEntries,
				aIdentity,
				std::ranges::less{ },
				[](const auto& aEntry) { return aEntry.first; });
			return found != aEntries.end() && found->first == aIdentity
				? std::optional<std::size_t>{ found->second }
				: std::nullopt;
		}

		[[nodiscard]] auto temporary_semantic_index(
			const computed_file_diff& aDiff)
			-> std::optional<diff_semantic_index>
		{
			return build_diff_semantic_index(
				std::shared_ptr<const computed_file_diff>{
					std::shared_ptr<const computed_file_diff>{ },
					&aDiff });
		}
	} // namespace

	auto diff_semantic_index::diff() const noexcept -> const computed_file_diff&
	{
		return *Diff;
	}

	auto diff_semantic_index::matches_result(const computed_file_diff& aResult) const noexcept -> bool
	{
		return Diff.get() == &aResult;
	}

	auto diff_semantic_index::retained_bytes() const noexcept -> std::optional<std::uint64_t>
	{
		auto       total = static_cast<std::uint64_t>(sizeof(*this));
		const auto add   = [&total]<typename Value>(const std::vector<Value>& aValues) {
			const auto capacity = static_cast<std::uint64_t>(aValues.capacity());
			if (capacity > (std::numeric_limits<std::uint64_t>::max() - total) / sizeof(Value)) {
				return false;
			}
			total += capacity * sizeof(Value);
			return true;
		};
		if (!add(SegmentsById) || !add(GroupsById) || !add(PairsById)
			|| !add(MovesBySegment) || !add(IntralineByPair)) {
			return std::nullopt;
		}
		return total;
	}

	auto diff_semantic_index::statistics() const noexcept
		-> diff_semantic_index_statistics
	{
		return {
			.Segments       = Diff->Alignment.Segments.size(),
			.Pairs          = Diff->Alignment.Pairs.size(),
			.Groups         = Diff->Alignment.Groups.size(),
			.Moves          = Diff->Alignment.Moves.size(),
			.IntralineEdits = Diff->Intraline.size(),
			.IndexEntries   = SegmentsById.size() + GroupsById.size()
				+ PairsById.size() + MovesBySegment.size()
				+ IntralineByPair.size(),
		};
	}

	auto diff_semantic_index::segment_index(
		const alignment_segment_id aSegment) const noexcept
		-> std::optional<std::size_t>
	{
		return indexed_record(SegmentsById, aSegment);
	}

	auto diff_semantic_index::group_index(
		const change_group_id aGroup) const noexcept
		-> std::optional<std::size_t>
	{
		return indexed_record(GroupsById, aGroup);
	}

	auto diff_semantic_index::pair_index(const line_pair_id aPair) const noexcept
		-> std::optional<std::size_t>
	{
		return indexed_record(PairsById, aPair);
	}

	auto diff_semantic_index::move_index(
		const alignment_segment_id aSegment) const noexcept
		-> std::optional<std::size_t>
	{
		return indexed_record(MovesBySegment, aSegment);
	}

	auto diff_semantic_index::intraline_span(
		const std::size_t aPairIndex) const noexcept -> segment_index_span
	{
		return aPairIndex < IntralineByPair.size()
			? IntralineByPair[aPairIndex]
			: segment_index_span{ };
	}

	auto build_diff_semantic_index(
		std::shared_ptr<const computed_file_diff> aDiff)
		-> std::optional<diff_semantic_index>
	{
		if (!aDiff || !validate_computed_file_diff(*aDiff).valid()) {
			return std::nullopt;
		}
		return diff_semantic_index::from_validated(std::move(aDiff));
	}

	auto build_diff_semantic_index(
		std::shared_ptr<const computed_file_diff> aDiff, const diff_compute_control& aControl)
		-> diff_semantic_index_build
	{
		if (!aDiff) {
			return { };
		}
		// [[BPR-1366]] The validation is linear in the result; a stop
		// abandons it ([[BPR-1485]]).
		auto meter = detail::diff_step_meter{ aControl.Stop, aControl.CheckInterval };
		try {
			if (!detail::validate_computed_file_diff(*aDiff, meter).valid()) {
				return { };
			}
		} catch (const detail::diff_compute_stopped&) {
			return { .Index = std::nullopt, .Stopped = true };
		}
		return { .Index = diff_semantic_index::from_validated(std::move(aDiff)), .Stopped = false };
	}

	auto diff_semantic_index::from_validated(std::shared_ptr<const computed_file_diff> aDiff)
		-> diff_semantic_index
	{
		const auto& diff   = *aDiff;
		auto        result = diff_semantic_index{ };
		result.Diff        = std::move(aDiff);
		result.SegmentsById.reserve(diff.Alignment.Segments.size());
		for (std::size_t index = 0U; index < diff.Alignment.Segments.size(); ++index) {
			result.SegmentsById.emplace_back(
				diff.Alignment.Segments[index].Id,
				static_cast<std::uint32_t>(index));
		}
		result.GroupsById.reserve(diff.Alignment.Groups.size());
		for (std::size_t index = 0U; index < diff.Alignment.Groups.size(); ++index) {
			result.GroupsById.emplace_back(
				diff.Alignment.Groups[index].Id,
				static_cast<std::uint32_t>(index));
		}
		result.PairsById.reserve(diff.Alignment.Pairs.size());
		for (std::size_t index = 0U; index < diff.Alignment.Pairs.size(); ++index) {
			result.PairsById.emplace_back(
				diff.Alignment.Pairs[index].Id,
				static_cast<std::uint32_t>(index));
		}
		result.MovesBySegment.reserve(diff.Alignment.Moves.size() * 2U);
		for (std::size_t index = 0U; index < diff.Alignment.Moves.size(); ++index) {
			const auto ordinal = static_cast<std::uint32_t>(index);
			result.MovesBySegment.emplace_back(
				diff.Alignment.Moves[index].SourceDeletion, ordinal);
			result.MovesBySegment.emplace_back(
				diff.Alignment.Moves[index].DestinationInsertion, ordinal);
		}
		const auto byIdentity = [](const auto& aLeft, const auto& aRight) {
			return aLeft.first < aRight.first;
		};
		std::ranges::sort(result.SegmentsById, byIdentity);
		std::ranges::sort(result.GroupsById, byIdentity);
		std::ranges::sort(result.PairsById, byIdentity);
		std::ranges::sort(result.MovesBySegment, byIdentity);

		result.IntralineByPair.resize(diff.Alignment.Pairs.size());
		auto edit = std::size_t{ 0U };
		for (std::size_t pair = 0U; pair < diff.Alignment.Pairs.size(); ++pair) {
			const auto begin = edit;
			while (
				edit < diff.Intraline.size()
				&& diff.Intraline[edit].Pair == diff.Alignment.Pairs[pair].Id) {
				++edit;
			}
			result.IntralineByPair[pair] = {
				.Begin = static_cast<std::uint32_t>(begin),
				.End   = static_cast<std::uint32_t>(edit),
			};
		}
		return result;
	}

	auto map_line(const diff_semantic_index& aIndex, const source_line_ref aLine)
		-> line_mapping
	{
		const auto& aDiff = aIndex.diff();
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

	auto map_line(const computed_file_diff& aDiff, const source_line_ref aLine)
		-> line_mapping
	{
		const auto index = temporary_semantic_index(aDiff);
		return index.has_value()
			? map_line(*index, aLine)
			: unavailable(aDiff, aLine, semantic_lookup_reason::invalid_result);
	}

	auto move_for_line(const diff_semantic_index& aIndex, const source_line_ref aLine)
		-> std::optional<move_correspondence>
	{
		const auto& aDiff = aIndex.diff();
		if (!valid_semantic_result(aIndex) || aLine.Comparison != aDiff.Alignment.Digest || !valid_side(aLine.Side) || !valid(aLine, aDiff.Alignment.LeftLineCount, aDiff.Alignment.RightLineCount)) {
			return std::nullopt;
		}
		const auto* const selectedSegment = find_segment(aDiff.Alignment, aLine);
		if (selectedSegment == nullptr) {
			return std::nullopt;
		}

		const auto moveIndex = aIndex.move_index(selectedSegment->Id);
		if (!moveIndex.has_value()) {
			return std::nullopt;
		}
		const auto& move                  = aDiff.Alignment.Moves[*moveIndex];
		const auto  selectedIsSource      = aLine.Side == diff_side::left && selectedSegment->Id == move.SourceDeletion;
		const auto  selectedIsDestination = aLine.Side == diff_side::right && selectedSegment->Id == move.DestinationInsertion;
		if (!selectedIsSource && !selectedIsDestination) {
			return std::nullopt;
		}
		const auto* const source      = find_segment_by_id(aIndex, move.SourceDeletion);
		const auto* const destination = find_segment_by_id(aIndex, move.DestinationInsertion);
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

	auto move_for_line(const computed_file_diff& aDiff, const source_line_ref aLine)
		-> std::optional<move_correspondence>
	{
		const auto index = temporary_semantic_index(aDiff);
		return index.has_value() ? move_for_line(*index, aLine) : std::nullopt;
	}

	namespace {
		[[nodiscard]] auto detail_for_canonical_mapping(
			const diff_semantic_index& aIndex,
			const line_mapping&        aMapping) -> line_detail_input
		{
			const auto& aDiff  = aIndex.diff();
			auto        detail = line_detail_input{
				.Selected   = aMapping.Source,
				.Reason     = aMapping.Reason,
				.Diagnostic = aMapping.Diagnostic,
			};
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
					const auto pair = aIndex.pair_index(*aMapping.Pair);
					if (pair.has_value()) {
						const auto span = aIndex.intraline_span(*pair);
						detail.Intraline.insert(
							detail.Intraline.end(),
							aDiff.Intraline.begin() + span.Begin,
							aDiff.Intraline.begin() + span.End);
					}
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
			if (const auto move = move_for_line(aIndex, aMapping.Source);
				move.has_value()) {
				detail.Move = move->Relation;
			}
			return detail;
		}
	} // namespace

	auto detail_for_mapping(const diff_semantic_index& aIndex,
							const line_mapping&        aMapping) -> line_detail_input
	{
		if (map_line(aIndex, aMapping.Source) != aMapping) {
			return line_detail_input{
				.Selected   = aMapping.Source,
				.Reason     = semantic_lookup_reason::invalid_result,
				.Diagnostic = aIndex.diff().Alignment.Diagnostic.Code,
			};
		}
		return detail_for_canonical_mapping(aIndex, aMapping);
	}

	auto detail_for_mapping(const computed_file_diff& aDiff,
							const line_mapping&       aMapping) -> line_detail_input
	{
		const auto index = temporary_semantic_index(aDiff);
		return index.has_value()
			? detail_for_mapping(*index, aMapping)
			: line_detail_input{
				  .Selected   = aMapping.Source,
				  .Reason     = semantic_lookup_reason::invalid_result,
				  .Diagnostic = aDiff.Alignment.Diagnostic.Code,
			  };
	}

	auto detail_for_line(const diff_semantic_index& aIndex, const source_line_ref aLine)
		-> line_detail_input
	{
		return detail_for_canonical_mapping(aIndex, map_line(aIndex, aLine));
	}

	auto detail_for_line(const computed_file_diff& aDiff, const source_line_ref aLine)
		-> line_detail_input
	{
		const auto index = temporary_semantic_index(aDiff);
		return index.has_value()
			? detail_for_line(*index, aLine)
			: line_detail_input{
				  .Selected   = aLine,
				  .Reason     = semantic_lookup_reason::invalid_result,
				  .Diagnostic = aDiff.Alignment.Diagnostic.Code,
			  };
	}

	auto project_changes(const diff_semantic_index& aIndex) -> change_projection
	{
		const auto& aDiff  = aIndex.diff();
		auto        result = change_projection{
			.Comparison = aDiff.Alignment.Digest,
			.Reason     = semantic_lookup_reason::none,
			.Diagnostic = aDiff.Alignment.Diagnostic.Code,
		};
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
				if (const auto move = aIndex.move_index(segment.Id); move.has_value()) {
					projected.Moves.push_back(aDiff.Alignment.Moves[*move].Id);
				}
			}
			std::ranges::sort(projected.Moves);
			projected.Moves.erase(
				std::ranges::unique(projected.Moves).begin(),
				projected.Moves.end());
			result.Changes.push_back(std::move(projected));
		}
		return result;
	}

	auto project_changes(const computed_file_diff& aDiff) -> change_projection
	{
		const auto index = temporary_semantic_index(aDiff);
		return index.has_value()
			? project_changes(*index)
			: change_projection{
				  .Comparison = aDiff.Alignment.Digest,
				  .Reason     = semantic_lookup_reason::invalid_result,
				  .Diagnostic = aDiff.Alignment.Diagnostic.Code,
			  };
	}

	auto navigate_change(const diff_semantic_index&           aIndex,
						 const std::optional<change_group_id> aCurrent,
						 const change_navigation_direction    aDirection,
						 const change_navigation_policy       aPolicy)
		-> change_navigation_result
	{
		const auto& aDiff = aIndex.diff();
		if (!valid_semantic_result(aIndex) || (aDirection != change_navigation_direction::previous && aDirection != change_navigation_direction::next) || (aPolicy != change_navigation_policy::stop && aPolicy != change_navigation_policy::wrap)) {
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
		const auto found = aIndex.group_index(*aCurrent);
		if (!found.has_value()) {
			return { };
		}
		const auto index = *found;
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
				.Group   = groups[index].Id,
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
			.Group   = groups[index].Id,
			.Index   = 0U,
		};
	}

	auto navigate_outermost_change(const diff_semantic_index&           aIndex,
								   const std::optional<change_group_id> aCurrent,
								   const change_navigation_direction    aDirection)
		-> change_navigation_result
	{
		if (!valid_semantic_result(aIndex) || (aDirection != change_navigation_direction::previous && aDirection != change_navigation_direction::next)) {
			return { };
		}
		const auto& groups = aIndex.diff().Alignment.Groups;
		if (groups.empty()) {
			return { .Outcome = change_navigation_outcome::no_changes };
		}
		const auto first  = aDirection == change_navigation_direction::previous;
		const auto target = first ? std::size_t{ 0U } : groups.size() - 1U;
		const auto there  = aCurrent.has_value() && *aCurrent == groups[target].Id;
		return {
			.Outcome = !there ? change_navigation_outcome::selected
				: first       ? change_navigation_outcome::at_start
							  : change_navigation_outcome::at_end,
			.Group   = groups[target].Id,
			.Index   = static_cast<std::uint32_t>(target),
		};
	}

	auto navigate_change(const computed_file_diff&            aDiff,
						 const std::optional<change_group_id> aCurrent,
						 const change_navigation_direction    aDirection,
						 const change_navigation_policy       aPolicy)
		-> change_navigation_result
	{
		const auto index = temporary_semantic_index(aDiff);
		return index.has_value()
			? navigate_change(*index, aCurrent, aDirection, aPolicy)
			: change_navigation_result{ };
	}

	auto navigate_change(const diff_semantic_index&        aIndex,
						 const source_line_ref             aCurrent,
						 const change_navigation_direction aDirection,
						 const change_navigation_policy    aPolicy) -> change_navigation_result
	{
		const auto mapping = map_line(aIndex, aCurrent);
		if (mapping.Kind == line_mapping_kind::unavailable || !mapping.Segment
			|| (aDirection != change_navigation_direction::previous && aDirection != change_navigation_direction::next)
			|| (aPolicy != change_navigation_policy::stop && aPolicy != change_navigation_policy::wrap)) {
			return { };
		}
		if (mapping.Group) {
			return navigate_change(aIndex, mapping.Group, aDirection, aPolicy);
		}
		const auto& groups = aIndex.diff().Alignment.Groups;
		if (groups.empty()) {
			return { .Outcome = change_navigation_outcome::no_changes };
		}
		const auto segment = aIndex.segment_index(*mapping.Segment);
		if (!segment) {
			return { };
		}
		// Segment order is shared by both documents; source line numbers are not.
		const auto next    = std::ranges::upper_bound(groups, *segment,
													  std::ranges::less{ },
													  [](const change_group& aGroup) { return aGroup.Members.Begin; });
		const auto before  = static_cast<std::size_t>(std::ranges::distance(groups.begin(), next));
		const auto forward = aDirection == change_navigation_direction::next;
		const auto bounded = forward ? next == groups.end() : before == 0U;
		if (bounded && aPolicy == change_navigation_policy::stop) {
			return { .Outcome = forward ? change_navigation_outcome::at_end : change_navigation_outcome::at_start };
		}
		const auto target = bounded ? (forward ? 0U : groups.size() - 1U)
			: forward               ? before
									: before - 1U;
		return {
			.Outcome = bounded ? change_navigation_outcome::wrapped : change_navigation_outcome::selected,
			.Group   = groups[target].Id,
			.Index   = static_cast<std::uint32_t>(target),
		};
	}

	auto select_line(const diff_semantic_index& aIndex, const source_line_ref aLine)
		-> std::optional<semantic_selection>
	{
		const auto& aDiff   = aIndex.diff();
		const auto  mapping = map_line(aIndex, aLine);
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

	auto select_line(const computed_file_diff& aDiff, const source_line_ref aLine)
		-> std::optional<semantic_selection>
	{
		const auto index = temporary_semantic_index(aDiff);
		return index.has_value() ? select_line(*index, aLine) : std::nullopt;
	}

	auto preferred_group_line(const diff_semantic_index& aIndex,
							  const change_group_id      aGroup)
		-> std::optional<source_line_ref>
	{
		const auto& aDiff = aIndex.diff();
		if (!valid_semantic_result(aIndex)) {
			return std::nullopt;
		}
		const auto* const selectedGroup = find_group(aIndex, aGroup);
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

	auto preferred_group_line(const computed_file_diff& aDiff,
							  const change_group_id     aGroup)
		-> std::optional<source_line_ref>
	{
		const auto index = temporary_semantic_index(aDiff);
		return index.has_value()
			? preferred_group_line(*index, aGroup)
			: std::nullopt;
	}

	auto select_group(const diff_semantic_index& aIndex, const change_group_id aGroup)
		-> std::optional<semantic_selection>
	{
		const auto& aDiff     = aIndex.diff();
		const auto  preferred = preferred_group_line(aIndex, aGroup);
		if (!preferred.has_value()) {
			return std::nullopt;
		}
		const auto mapping = map_line(aIndex, *preferred);
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

	auto select_group(const computed_file_diff& aDiff, const change_group_id aGroup)
		-> std::optional<semantic_selection>
	{
		const auto index = temporary_semantic_index(aDiff);
		return index.has_value() ? select_group(*index, aGroup) : std::nullopt;
	}

	auto reconcile_selection(const diff_semantic_index&               aReplacement,
							 const std::optional<semantic_selection>& aCurrent)
		-> selection_reconciliation_result
	{
		const auto& diff = aReplacement.diff();
		if (!aCurrent.has_value() || !valid_semantic_result(aReplacement) || aCurrent->Comparison != diff.Alignment.Digest || aCurrent->PreferredLine.Comparison != diff.Alignment.Digest) {
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

	auto reconcile_selection(const computed_file_diff&                aReplacement,
							 const std::optional<semantic_selection>& aCurrent)
		-> selection_reconciliation_result
	{
		const auto index = temporary_semantic_index(aReplacement);
		return index.has_value()
			? reconcile_selection(*index, aCurrent)
			: selection_reconciliation_result{ };
	}

	auto detail_for_selection(const diff_semantic_index& aIndex,
							  const semantic_selection&  aSelection)
		-> line_detail_input
	{
		const auto& aDiff     = aIndex.diff();
		auto        canonical = std::optional<semantic_selection>{ };
		switch (aSelection.Kind) {
		case semantic_selection_kind::line:
			canonical = select_line(aIndex, aSelection.PreferredLine);
			break;
		case semantic_selection_kind::group:
			if (aSelection.Group.has_value()) {
				canonical = select_group(aIndex, *aSelection.Group);
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
		return detail_for_line(aIndex, aSelection.PreferredLine);
	}

	auto detail_for_selection(const computed_file_diff& aDiff,
							  const semantic_selection& aSelection)
		-> line_detail_input
	{
		const auto index = temporary_semantic_index(aDiff);
		return index.has_value()
			? detail_for_selection(*index, aSelection)
			: line_detail_input{
				  .Selected   = aSelection.PreferredLine,
				  .Reason     = semantic_lookup_reason::invalid_result,
				  .Diagnostic = aDiff.Alignment.Diagnostic.Code,
			  };
	}

	namespace {
		/// \brief The end of the shown part of a line: at most `aLimit` bytes,
		/// moved back off any UTF-8 continuation byte so a character is never
		/// split. Looks at no more than three bytes before the limit.
		[[nodiscard]] auto shown_line_end(const decoded_text_document& aDocument,
										  const byte_span              aContent,
										  const std::uint64_t          aLimit) -> std::uint64_t
		{
			const auto length = aContent.End - aContent.Begin;
			if (length <= aLimit) {
				return aContent.End;
			}
			auto end = aContent.Begin + aLimit;
			for (auto step = 0; step < 3 && end > aContent.Begin; ++step) {
				const auto byte = aDocument.Bytes[static_cast<std::size_t>(end)];
				if ((byte & 0xC0U) != 0x80U) {
					break;
				}
				--end;
			}
			return end;
		}

		[[nodiscard]] auto line_details_side_row(
			const computed_file_diff&   aDiff,
			const source_line_ref       aLine,
			const line_details_row_kind aKind,
			const line_details_limits&  aLimits) -> std::optional<line_details_row>
		{
			const auto& document = aLine.Side == diff_side::left ? aDiff.LeftDocument : aDiff.RightDocument;
			if (!document.has_value() || aLine.Line >= document->Lines.size()) {
				return std::nullopt;
			}
			const auto content = document->Lines[aLine.Line].Content;
			if (content.End < content.Begin || content.End > document->Bytes.size()) {
				return std::nullopt;
			}
			const auto end = shown_line_end(*document, content, aLimits.MaximumLineBytes);
			return line_details_row{
				.Side         = aLine.Side,
				.Kind         = aKind,
				.Line         = aLine.Line,
				.Content      = byte_span{ content.Begin, end },
				.OmittedBytes = content.End - end,
			};
		}

		void add_emphasis(line_details_row& aRow, const byte_span aSpan)
		{
			const auto shown = aRow.Content.End - aRow.Content.Begin;
			const auto begin = std::min(aSpan.Begin, shown);
			const auto end   = std::min(aSpan.End, shown);
			if (begin < end) {
				aRow.Emphasis.push_back(byte_span{ begin, end });
			}
		}

		[[nodiscard]] auto absent_row(const diff_side aSide, const line_detail_input& aDetail)
			-> line_details_row
		{
			auto row = line_details_row{ .Side = aSide, .Kind = line_details_row_kind::absent };
			if (aDetail.OpposingSpan.has_value() && aDetail.OpposingSpan->Side == aSide) {
				row.Replaces = aDetail.OpposingSpan->Lines;
			}
			if (aDetail.OpposingBoundary.has_value() && aDetail.OpposingBoundary->Side == aSide) {
				row.Boundary = aDetail.OpposingBoundary->Boundary;
			}
			return row;
		}
	} // namespace

	auto line_details(const diff_semantic_index& aIndex,
					  const diff_side            aSide,
					  const line_span            aLines,
					  const line_details_limits  aLimits) -> line_details_projection
	{
		const auto& diff   = aIndex.diff();
		auto        result = line_details_projection{ };
		const auto  count  = aSide == diff_side::left ? diff.Alignment.LeftLineCount : diff.Alignment.RightLineCount;
		if (!valid_side(aSide) || diff.Alignment.Content != diff_content_state::text || aLines.Begin >= aLines.End
			|| aLines.End > count) {
			return result;
		}
		const auto shown    = std::min<std::uint64_t>(aLines.extent(), aLimits.MaximumLines);
		result.OmittedLines = static_cast<std::uint32_t>(aLines.extent() - shown);
		result.Rows.reserve(static_cast<std::size_t>(shown) * 2U);
		for (auto line = aLines.Begin; line < aLines.Begin + shown; ++line) {
			const auto detail = detail_for_line(
				aIndex, source_line_ref{ .Comparison = diff.Alignment.Digest, .Side = aSide, .Line = line });
			if (detail.Kind == line_detail_kind::paired_lines && detail.LeftLine && detail.RightLine) {
				auto kind = line_details_row_kind::unchanged;
				if (detail.Pair.has_value()) {
					const auto pair = aIndex.pair_index(*detail.Pair);
					if (pair.has_value() && diff.Alignment.Pairs[*pair].Kind == line_pair_kind::similar) {
						kind = line_details_row_kind::changed;
					}
				}
				auto left  = line_details_side_row(diff, *detail.LeftLine, kind, aLimits);
				auto right = line_details_side_row(diff, *detail.RightLine, kind, aLimits);
				if (!left || !right) {
					continue;
				}
				for (const auto& edit : detail.Intraline) {
					add_emphasis(*left, edit.Left);
					add_emphasis(*right, edit.Right);
				}
				result.Rows.push_back(std::move(*left));
				result.Rows.push_back(std::move(*right));
				continue;
			}
			if (detail.Kind != line_detail_kind::opposing_span && detail.Kind != line_detail_kind::opposing_boundary) {
				continue;
			}
			const auto kind     = aSide == diff_side::left ? line_details_row_kind::removed : line_details_row_kind::added;
			auto       selected = line_details_side_row(diff, detail.Selected, kind, aLimits);
			if (!selected) {
				continue;
			}
			auto other = absent_row(other_side(aSide), detail);
			// A one-line replacement the comparison could not pair is still
			// read as base over head: its other side is that one line, without
			// emphasis, because no character pairing was made.
			if (other.Replaces.has_value() && other.Replaces->extent() == 1U) {
				const auto counterpart = source_line_ref{ .Comparison = diff.Alignment.Digest, .Side = other.Side, .Line = other.Replaces->Begin };
				if (auto replaced = line_details_side_row(diff, counterpart, aSide == diff_side::left ? line_details_row_kind::added : line_details_row_kind::removed, aLimits)) {
					other = std::move(*replaced);
				}
			}
			// Base first, whichever side was selected.
			auto& base = aSide == diff_side::left ? *selected : other;
			auto& head = aSide == diff_side::left ? other : *selected;
			result.Rows.push_back(std::move(base));
			result.Rows.push_back(std::move(head));
		}
		return result;
	}
} // namespace bpr::core
