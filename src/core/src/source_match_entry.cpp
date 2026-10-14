#include "source_matcher_internal.hpp"

#include <new>
#include <tuple>

namespace bpr::core {
	namespace {
		using namespace source_matching;
		struct line_index
		{
			std::vector<std::size_t> Starts;
			struct column_checkpoint
			{
				std::size_t   Offset;
				std::uint32_t Column;
			};
			std::vector<column_checkpoint> Columns;
			auto                           line(std::uint64_t aOffset) const -> std::uint32_t
			{
				return static_cast<std::uint32_t>(std::ranges::upper_bound(Starts, aOffset) - Starts.begin() - 1);
			}
		};
		auto index_lines(std::string_view aText, const source_match_limits& aLimits, budget& aWork) -> line_index
		{
			auto result = line_index{ };
			result.Starts.reserve(std::min(aLimits.MaximumLines + 1U, aText.size() + 1U));
			result.Starts.push_back(0U);
			result.Columns.reserve(aText.size() / 256U + 1U);
			auto column  = 0U;
			auto ordinal = 0U;
			for (auto offset = std::size_t{ 0U }; offset < aText.size();) {
				if ((ordinal++ % 256U) == 0U) {
					result.Columns.push_back({ offset, column });
				}
				const auto value = scalar(aText, offset);
				aWork.tick();
				++column;
				// [[BPR-1432]]. Only LF (or CRLF) ends a line; a lone CR is a column.
				const auto crlf = value == U'\r' && offset < aText.size() && aText[offset] == '\n';
				if (crlf) {
					++offset;
				}
				if (crlf || value == U'\n') {
					column = 0U;
					if (result.Starts.size() >= aLimits.MaximumLines + 1U) {
						throw stopped{ source_search_status::resource_limit };
					}
					result.Starts.push_back(offset);
				}
			}
			return result;
		}
		struct row_plan
		{
			std::array<std::vector<std::uint32_t>, 2> Rows;
			std::vector<const alignment_segment*>     Equal;
			std::vector<byte_span>                    LeftRanges;
		};
		auto make_rows(const computed_file_diff& aDiff, budget& aWork) -> row_plan
		{
			auto        result    = row_plan{ };
			const auto& alignment = aDiff.Alignment;
			result.Rows[0].resize(static_cast<std::size_t>(alignment.LeftLineCount) + 1U);
			result.Rows[1].resize(static_cast<std::size_t>(alignment.RightLineCount) + 1U);
			result.Equal.reserve(alignment.Segments.size());
			auto represented = std::vector<bool>(alignment.LeftLineCount, true);
			auto row         = 0U;
			auto group       = std::size_t{ 0U };
			for (auto index = std::size_t{ 0U }; index < alignment.Segments.size();) {
				const auto& segment = alignment.Segments[index];
				if (segment.Kind == alignment_kind::equal) {
					result.Equal.push_back(&segment);
					for (auto offset = 0U; offset < segment.Left.extent(); ++offset) {
						aWork.tick();
						const auto leftLine  = segment.Left.Begin + offset;
						const auto rightLine = segment.Right.Begin + offset;
						const auto left      = aDiff.LeftDocument->Lines[leftLine].Content;
						const auto right     = aDiff.RightDocument->Lines[rightLine].Content;
						auto       equal     = left.End - left.Begin == right.End - right.Begin;
						for (auto byte = std::uint64_t{ 0U }; equal && byte < left.End - left.Begin; ++byte) {
							aWork.tick();
							equal = aDiff.LeftDocument->Bytes[left.Begin + byte] == aDiff.RightDocument->Bytes[right.Begin + byte];
						}
						represented[leftLine]                        = equal;
						result.Rows[0][segment.Left.Begin + offset]  = row;
						result.Rows[1][segment.Right.Begin + offset] = row++;
					}
					++index;
				} else {
					const auto& block = alignment.Groups[group++];
					for (auto line = block.Left.Begin; line < block.Left.End; ++line) {
						aWork.tick();
						result.Rows[0][line] = row++;
					}
					for (auto line = block.Right.Begin; line < block.Right.End; ++line) {
						aWork.tick();
						result.Rows[1][line] = row++;
					}
					index = block.Members.End;
				}
			}
			result.LeftRanges.reserve(represented.size() + 1U);
			if (aDiff.LeftDocument && represented.empty()) {
				result.LeftRanges.push_back({ 0U, 0U });
			}
			for (auto line = std::size_t{ 0U }; line < represented.size(); ++line) {
				aWork.tick();
				if (!represented[line]) {
					continue;
				}
				const auto& source = aDiff.LeftDocument->Lines[line];
				const auto  end    = std::max(source.Content.End, source.Terminator.End);
				if (!result.LeftRanges.empty() && result.LeftRanges.back().End == source.Content.Begin) {
					result.LeftRanges.back().End = end;
				} else {
					result.LeftRanges.push_back({ source.Content.Begin, end });
				}
			}
			result.Rows[0].back() = row;
			result.Rows[1].back() = row;
			return result;
		}
		auto location(diff_side aSide, const source_match_side& aMaterial, byte_span aSpan, const line_index& aIndex, budget& aWork) -> source_match_location
		{
			auto result           = source_match_location{ };
			result.Side           = aSide;
			result.Blob           = aMaterial.Blob;
			result.Span           = aSpan;
			result.Line           = aIndex.line(aSpan.Begin);
			auto       offset     = aIndex.Starts[result.Line];
			const auto checkpoint = std::ranges::upper_bound(aIndex.Columns, aSpan.Begin, { }, &line_index::column_checkpoint::Offset);
			if (checkpoint != aIndex.Columns.begin() && std::prev(checkpoint)->Offset >= offset) {
				offset        = std::prev(checkpoint)->Offset;
				result.Column = std::prev(checkpoint)->Column;
			}
			for (; offset < aSpan.Begin;) {
				aWork.tick();
				(void)scalar(aMaterial.Bytes, offset);
				++result.Column;
			}
			return result;
		}
		auto counterpart(byte_span aSpan, const source_match_side& aLeft, const source_match_side& aRight, const line_index& aLeftLines, const line_index& aRightLines, const row_plan& aPlan, budget& aWork) -> std::optional<byte_span>
		{
			auto first = aLeftLines.line(aSpan.Begin);
			if (aSpan.empty() && aSpan.Begin == aLeft.Bytes.size() && first > 0U && aLeftLines.Starts[first] == aSpan.Begin) {
				--first;
			}
			const auto last  = aSpan.empty() ? first : aLeftLines.line(aSpan.End - 1U);
			const auto found = std::ranges::upper_bound(aPlan.Equal, first, { }, [](const alignment_segment* aSegment) { return aSegment->Left.Begin; });
			if (found == aPlan.Equal.begin()) {
				return { };
			}
			const auto& segment = **std::prev(found);
			if (first >= segment.Left.End || last >= segment.Left.End) {
				return { };
			}
			const auto rightFirst  = segment.Right.Begin + (first - segment.Left.Begin);
			auto       leftOffset  = static_cast<std::size_t>(aSpan.Begin);
			auto       rightOffset = aRightLines.Starts[rightFirst];
			const auto startInLine = leftOffset - aLeftLines.Starts[first];
			if (rightOffset + startInLine > aRight.Bytes.size()) {
				return { };
			}
			if (aSpan.empty() && aSpan.Begin == aLeft.Bytes.size() && segment.Right.End + 1U >= aRightLines.Starts.size()) {
				return byte_span{ aRight.Bytes.size(), aRight.Bytes.size() };
			}
			rightOffset += startInLine;
			const auto begin = rightOffset;
			// Exact LF-view equality also protects whitespace-normalized equal rows.
			while (leftOffset < aSpan.End) {
				aWork.tick();
				auto left = scalar(aLeft.Bytes, leftOffset);
				if (rightOffset == aRight.Bytes.size()) {
					return { };
				}
				auto right = scalar(aRight.Bytes, rightOffset);
				if (left == U'\r' && leftOffset < aLeft.Bytes.size() && aLeft.Bytes[leftOffset] == '\n') {
					++leftOffset;
					left = U'\n';
				}
				if (right == U'\r' && rightOffset < aRight.Bytes.size() && aRight.Bytes[rightOffset] == '\n') {
					++rightOffset;
					right = U'\n';
				}
				if (left != right) {
					return { };
				}
			}
			return byte_span{ begin, rightOffset };
		}
		auto material(const std::optional<decoded_text_document>& aDocument, const std::optional<git_blob_oid>& aBlob) -> std::optional<source_match_side>
		{
			if (!aDocument || !aBlob) {
				return { };
			}
			return source_match_side{ *aBlob, std::string_view{ reinterpret_cast<const char*>(aDocument->Bytes.data()), aDocument->Bytes.size() } };
		}
		auto emit_record(source_match_entry_result& aResult, source_match_record aRecord, const source_match_limits& aLimits, std::size_t& aResultBytes) -> bool
		{
			if (aResult.Matches.size() == aLimits.MaximumMatches) {
				aResult.Status                  = source_search_status::result_limit;
				aResult.AdditionalMatchObserved = true;
				return false;
			}
			// Charge a conservative 1 KiB per preview plus its fixed record. This
			// includes string capacity rounding and the terminator (240 scalars).
			if (sizeof(source_match_record) + 1024U > aLimits.MaximumResultBytes - aResultBytes) {
				aResult.Status = source_search_status::resource_limit;
				return false;
			}
			aResultBytes += sizeof(source_match_record) + 1024U;
			aResult.Matches.push_back(std::move(aRecord));
			return true;
		}
		void collect_records(source_match_entry_result& aResult, const std::array<std::optional<source_match_side>, 2>& aSides, const std::array<source_match_scan, 2>& aScans, const std::array<line_index, 2>& aLines, const row_plan& aPlan, bool aUnified, bool aIdentical, const source_match_limits& aLimits, budget& aWork)
		{
			auto       cursors  = std::array<std::size_t, 2>{ };
			auto       consumed = std::vector<bool>(aScans[1].Matches.size(), false);
			auto       bytes    = std::size_t{ 0U };
			const auto row      = [&](std::size_t aSide, byte_span aSpan) { return aUnified ? aPlan.Rows[aSide][aLines[aSide].line(aSpan.Begin)] : 0U; };
			while (cursors[0] < aScans[0].Matches.size() || cursors[1] < aScans[1].Matches.size()) {
				aWork.tick();
				while (cursors[1] < consumed.size() && consumed[cursors[1]]) {
					aWork.tick();
					++cursors[1];
				}
				if (cursors[0] == aScans[0].Matches.size() && cursors[1] == aScans[1].Matches.size()) {
					break;
				}
				auto side = std::size_t{ 0U };
				if (cursors[0] == aScans[0].Matches.size() || (cursors[1] < aScans[1].Matches.size() && aUnified && row(1U, aScans[1].Matches[cursors[1]]) < row(0U, aScans[0].Matches[cursors[0]]))) {
					side = 1U;
				}
				const auto span        = aScans[side].Matches[cursors[side]++];
				auto       record      = source_match_record{ };
				record.Locations[side] = location(side == 0U ? diff_side::left : diff_side::right, *aSides[side], span, aLines[side], aWork);
				record.UnifiedRow      = row(side, span);
				record.Preview         = source_match_preview(aSides[side]->Bytes, span);
				record.PreviewMatch    = source_match_preview_span(aSides[side]->Bytes, span);
				for (auto offset = std::size_t{ 0U }; offset < record.Preview.size();) {
					aWork.tick();
					(void)scalar(record.Preview, offset);
				}
				if (side == 0U && aIdentical) {
					record.Locations[1] = location(diff_side::right, *aSides[1], span, aLines[1], aWork);
				} else if (side == 0U && aUnified && aSides[1]) {
					const auto other = counterpart(span, *aSides[0], *aSides[1], aLines[0], aLines[1], aPlan, aWork);
					if (other) {
						const auto found = std::ranges::lower_bound(aScans[1].Matches, *other);
						if (found != aScans[1].Matches.end() && *found == *other) {
							consumed[static_cast<std::size_t>(found - aScans[1].Matches.begin())] = true;
							record.Locations[1]                                                   = location(diff_side::right, *aSides[1], *other, aLines[1], aWork);
						}
					}
				}
				if (!emit_record(aResult, std::move(record), aLimits, bytes)) {
					break;
				}
			}
		}
	}
	auto match_source_entry(const source_match_program& aProgram, const source_match_entry& aEntry, diff_compute_stop* aStop) -> source_match_entry_result
	{
		auto result        = source_match_entry_result{ };
		result.File        = aEntry.File;
		const auto& limits = source_match_access::get(aProgram).Limits;
		try {
			auto work = budget{ limits.MaximumScanSteps, aStop };
			try {
				if (!aEntry.File.valid() || (aEntry.Target != source_search_target::left && aEntry.Target != source_search_target::right && aEntry.Target != source_search_target::unified && aEntry.Target != source_search_target::files)
					|| (aEntry.Left && !aEntry.Left->Blob.valid()) || (aEntry.Right && !aEntry.Right->Blob.valid())
					|| (aEntry.Selection && (aEntry.Selection->first != diff_side::left && aEntry.Selection->first != diff_side::right))) {
					throw stopped{ source_search_status::invalid_input };
				}
				auto       sides    = std::array<std::optional<source_match_side>, 2>{ aEntry.Left, aEntry.Right };
				auto       plan     = row_plan{ };
				const auto capacity = std::min(static_cast<std::size_t>(limits.MaximumMatches), limits.MaximumResultBytes / (sizeof(source_match_record) + 1024U));
				const auto extra    = capacity * (sizeof(source_match_record) + 1024U)
					+ (limits.MaximumLines + 1U) * (2U * sizeof(std::size_t) + 4U * sizeof(std::uint32_t) + 2U * sizeof(const alignment_segment*) + sizeof(byte_span))
					+ limits.MaximumMatches * (2U * sizeof(byte_span) + 1U)
					+ 2U * (limits.MaximumBytes / 256U + 1U) * sizeof(line_index::column_checkpoint);
				const auto worst = extra + aProgram.retained_bytes() + limits.MaximumBytes + 1U + limits.MaximumLines * 2U * sizeof(std::size_t);
				if (worst > limits.MaximumOwnedBytes) {
					throw stopped{ source_search_status::resource_limit };
				}
				const auto unified = aEntry.Target == source_search_target::unified;
				if (unified) {
					if (!aEntry.Unified || aEntry.Unified->diff().Alignment.Key.File != aEntry.File) {
						throw stopped{ source_search_status::unavailable };
					}
					const auto& diff = aEntry.Unified->diff();
					if (diff.Alignment.LeftLineCount > limits.MaximumLines || diff.Alignment.RightLineCount > limits.MaximumLines) {
						throw stopped{ source_search_status::resource_limit };
					}
					sides = { material(diff.LeftDocument, diff.Alignment.Key.LeftBlob), material(diff.RightDocument, diff.Alignment.Key.RightBlob) };
					plan  = make_rows(diff, work);
					if (aEntry.Selection) {
						const auto side = aEntry.Selection->first == diff_side::left ? 0U : 1U;
						if (!sides[side]) {
							throw stopped{ source_search_status::unavailable };
						}
						const auto selectedLines = index_lines(sides[side]->Bytes, limits, work);
						const auto span          = aEntry.Selection->second;
						if (span.Begin >= span.End || span.End > sides[side]->Bytes.size()) {
							throw stopped{ source_search_status::invalid_input };
						}
						const auto first = selectedLines.line(span.Begin);
						const auto last  = selectedLines.line(span.End - 1U);
						if ((side == 0U && !std::ranges::any_of(plan.LeftRanges, [&](byte_span aRange) { return aRange.Begin <= span.Begin && aRange.End >= span.End; })) || plan.Rows[side][last] - plan.Rows[side][first] != last - first) {
							throw stopped{ source_search_status::unavailable };
						}
					}
				}
				if (aEntry.Target == source_search_target::left) {
					sides[1].reset();
				}
				if (aEntry.Target == source_search_target::right) {
					sides[0].reset();
				}
				if (aEntry.Selection) {
					sides[aEntry.Selection->first == diff_side::left ? 1U : 0U].reset();
				}
				if (!sides[0] && !sides[1]) {
					throw stopped{ source_search_status::unavailable };
				}
				auto identical = false;
				if (sides[0] && sides[1] && sides[0]->Blob == sides[1]->Blob) {
					if (sides[0]->Bytes.size() != sides[1]->Bytes.size()) {
						throw stopped{ source_search_status::invalid_input };
					}
					for (auto offset = std::size_t{ 0U }; offset < sides[0]->Bytes.size(); ++offset) {
						work.tick();
						if (sides[0]->Bytes[offset] != sides[1]->Bytes[offset]) {
							throw stopped{ source_search_status::invalid_input };
						}
					}
					identical = true;
				}
				auto scans = std::array<source_match_scan, 2>{ };
				auto lines = std::array<line_index, 2>{ };
				result.Matches.reserve(capacity);
				for (auto side = std::size_t{ 0U }; side < 2U; ++side) {
					if (!sides[side]) {
						continue;
					}
					if (identical && side == 1U) {
						lines[1] = lines[0];
						continue;
					}
					const auto bounds = aEntry.Selection ? std::optional<byte_span>{ aEntry.Selection->second } : std::nullopt;
					if (!unified || side != 0U || !plan.LeftRanges.empty()) {
						scans[side] = scan_with_budget(aProgram, sides[side]->Bytes, bounds, work, unified && side == 0U ? std::span<const byte_span>{ plan.LeftRanges } : std::span<const byte_span>{ });
					}
					++result.ScannedSides;
					result.PeakOwnedBytes = std::max(result.PeakOwnedBytes, scans[side].PeakOwnedBytes + extra);
					if (result.PeakOwnedBytes > limits.MaximumOwnedBytes) {
						throw stopped{ source_search_status::resource_limit };
					}
					lines[side] = index_lines(sides[side]->Bytes, limits, work);
					if (scans[side].Status != source_search_status::complete && scans[side].Status != source_search_status::result_limit) {
						result.Status = scans[side].Status;
					}
					result.AdditionalMatchObserved |= scans[side].AdditionalMatchObserved;
				}
				collect_records(result, sides, scans, lines, plan, unified, identical, limits, work);
				if (result.Status == source_search_status::complete && result.AdditionalMatchObserved) {
					result.Status = source_search_status::result_limit;
				}
				if (result.Status != source_search_status::result_limit || result.Matches.size() != limits.MaximumMatches) {
					result.AdditionalMatchObserved = false;
				}
			} catch (const stopped& stop) {
				result.Status = stop.Status;
			}
			result.Steps = work.Steps;
		} catch (const stopped& stop) {
			result.Status = stop.Status;
		} catch (const std::bad_alloc&) {
			result.Status = source_search_status::resource_limit;
		}
		return result;
	}
}
