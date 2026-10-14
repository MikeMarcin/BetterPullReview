// [[RFC-BPR-0082]] [[BPR-1466]]. The core shape's hunk-sliding post-pass,
// written for the evaluation. A pure insertion or deletion inside a run of
// equal lines can often move up or down without changing the edit (its first
// and last lines are equal); this chooses the position a reader expects: a
// block preceded by a blank line, ending in a blank line, starting at the
// least indentation and followed by no deeper indentation. Ties go to the
// lowest position, as Git's compaction slides down. It reads only the
// engine's classes and the decoded lines; it is not Git's indent heuristic.

#include "candidates/candidates.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

namespace bpr::evaluation {
	namespace {
		struct side_view
		{
			const core::decoded_text_document* Document = nullptr;
			std::span<const std::uint32_t>     Classes;

			[[nodiscard]] auto lines() const noexcept -> std::uint32_t
			{
				return static_cast<std::uint32_t>(Classes.size());
			}

			[[nodiscard]] auto blank(const std::uint32_t aLine) const -> bool
			{
				const auto& line = Document->Lines[aLine];
				for (auto byte = line.Content.Begin; byte < line.Content.End; ++byte) {
					const auto value = Document->Bytes[static_cast<std::size_t>(byte)];
					if (value != ' ' && value != '\t') {
						return false;
					}
				}
				return true;
			}

			[[nodiscard]] auto indent(const std::uint32_t aLine) const -> std::int64_t
			{
				const auto& line  = Document->Lines[aLine];
				auto        width = std::int64_t{ 0 };
				for (auto byte = line.Content.Begin; byte < line.Content.End; ++byte) {
					const auto value = Document->Bytes[static_cast<std::size_t>(byte)];
					if (value == ' ') {
						++width;
					} else if (value == '\t') {
						width += 8 - width % 8;
					} else {
						return width;
					}
				}
				return 1'000'000;
			}

			/// A reader's preference for a block of aLength lines at aBegin.
			[[nodiscard]] auto score(const std::uint32_t aBegin, const std::uint32_t aLength) const -> std::int64_t
			{
				const auto end   = aBegin + aLength;
				auto       value = -std::min<std::int64_t>(indent(aBegin), 1'000);
				if (aBegin == 0U || blank(aBegin - 1U)) {
					value += 200;
				}
				if (blank(end - 1U)) {
					value += 200;
				}
				if (end == lines() || indent(end) <= indent(aBegin)) {
					value += 100;
				}
				return value;
			}
		};
	} // namespace

	auto slide_matches(const core::diff_primary_request& aRequest, const std::span<const core::diff_match> aMatches, core::diff_component_meter* const aMeter)
		-> std::vector<core::diff_match>
	{
		auto       matches = std::vector<core::diff_match>{ aMatches.begin(), aMatches.end() };
		const auto left    = side_view{ .Document = aRequest.Left, .Classes = aRequest.LeftClasses };
		const auto right   = side_view{ .Document = aRequest.Right, .Classes = aRequest.RightClasses };
		auto       steps   = std::uint64_t{ 0U };
		// Gap k lies between matches k-1 and k; the virtual matches before the
		// first and after the last line bound it.
		for (std::size_t gap = 0U; gap <= matches.size(); ++gap) {
			const auto leftBegin  = gap == 0U ? 0U : matches[gap - 1U].Left + 1U;
			const auto rightBegin = gap == 0U ? 0U : matches[gap - 1U].Right + 1U;
			const auto leftEnd    = gap == matches.size() ? left.lines() : matches[gap].Left;
			const auto rightEnd   = gap == matches.size() ? right.lines() : matches[gap].Right;
			const auto insertion  = leftBegin == leftEnd && rightBegin < rightEnd;
			const auto deletion   = rightBegin == rightEnd && leftBegin < leftEnd;
			if (!insertion && !deletion) {
				continue;
			}
			const auto& side   = insertion ? right : left;
			const auto  begin  = insertion ? rightBegin : leftBegin;
			const auto  length = insertion ? rightEnd - rightBegin : leftEnd - leftBegin;
			const auto  on     = [insertion](const core::diff_match& aMatch) { return insertion ? aMatch.Right : aMatch.Left; };
			// How far the block can move up: through the contiguous equal run
			// above it, while its last line equals the line above its first.
			auto up = std::uint32_t{ 0U };
			while (up < gap) {
				const auto above = gap - 1U - up;
				if (on(matches[above]) != begin - 1U - up) {
					break;
				}
				if (above + 1U < gap && (matches[above].Left + 1U != matches[above + 1U].Left || matches[above].Right + 1U != matches[above + 1U].Right)) {
					break;
				}
				if (side.Classes[begin - 1U - up] != side.Classes[begin + length - 1U - up]) {
					break;
				}
				++up;
				++steps;
			}
			auto down = std::uint32_t{ 0U };
			while (gap + down < matches.size()) {
				const auto below = gap + down;
				if (on(matches[below]) != begin + length + down) {
					break;
				}
				if (below > gap && (matches[below - 1U].Left + 1U != matches[below].Left || matches[below - 1U].Right + 1U != matches[below].Right)) {
					break;
				}
				if (side.Classes[begin + down] != side.Classes[begin + length + down]) {
					break;
				}
				++down;
				++steps;
			}
			if (up == 0U && down == 0U) {
				continue;
			}
			auto best      = std::int64_t{ 0 };
			auto bestScore = side.score(begin, length);
			for (auto offset = -static_cast<std::int64_t>(up); offset <= static_cast<std::int64_t>(down); ++offset) {
				const auto value = side.score(static_cast<std::uint32_t>(static_cast<std::int64_t>(begin) + offset), length);
				steps += 4U;
				if (value > bestScore || (value == bestScore && offset > best)) {
					bestScore = value;
					best      = offset;
				}
			}
			if (best < 0) {
				// The equal lines above the block move below it.
				for (auto moved = static_cast<std::size_t>(-best); moved != 0U; --moved) {
					auto& match = matches[gap - moved];
					(insertion ? match.Right : match.Left) += length;
				}
			} else if (best > 0) {
				for (std::size_t moved = 0U; moved < static_cast<std::size_t>(best); ++moved) {
					auto& match = matches[gap + moved];
					(insertion ? match.Right : match.Left) -= length;
				}
			}
			if (aMeter != nullptr && steps >= 4096U) {
				aMeter->tick(steps);
				steps = 0U;
			}
		}
		if (aMeter != nullptr) {
			aMeter->tick(steps);
		}
		return matches;
	}
} // namespace bpr::evaluation
