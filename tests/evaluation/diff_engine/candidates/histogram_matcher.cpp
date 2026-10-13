// [[RFC-BPR-0082]] [[BPR-1466]]. The core shape's histogram matcher, written
// for the evaluation from the published description of histogram diff (as in
// JGit and Git): trim the region's common ends, find the common run whose
// rarest left line occurs least often in the left region (at most 64 times),
// keep it, and recurse on both sides of it. A region with no such line falls
// back to O(NP). It is first-party code, not a port of JGit's source, so its
// choices may differ from Git's histogram where several runs tie.

#include "candidates/candidates.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace bpr::evaluation {
	namespace {
		inline constexpr std::uint32_t maximum_chain = 64U;

		struct region
		{
			std::uint32_t LeftBegin  = 0U;
			std::uint32_t LeftEnd    = 0U;
			std::uint32_t RightBegin = 0U;
			std::uint32_t RightEnd   = 0U;
		};

		class step_counter
		{
		public:
			explicit step_counter(core::diff_component_meter* const aMeter) noexcept
				: Meter{ aMeter }
			{
			}

			void add(const std::uint64_t aSteps)
			{
				Pending += aSteps;
				if (Meter != nullptr && Pending >= 4096U) {
					Meter->tick(Pending);
					Pending = 0U;
				}
			}

			void flush()
			{
				if (Meter != nullptr) {
					Meter->tick(Pending);
				}
				Pending = 0U;
			}

		private:
			core::diff_component_meter* Meter   = nullptr;
			std::uint64_t               Pending = 0U;
		};
	} // namespace

	auto histogram_matches(const std::span<const std::uint32_t> aLeft, const std::span<const std::uint32_t> aRight, core::diff_component_meter* const aMeter)
		-> std::vector<core::diff_match>
	{
		auto result  = std::vector<core::diff_match>{ };
		auto pending = std::vector<region>{ { .LeftEnd = static_cast<std::uint32_t>(aLeft.size()), .RightEnd = static_cast<std::uint32_t>(aRight.size()) } };
		auto steps   = step_counter{ aMeter };
		auto counts  = std::unordered_map<std::uint32_t, std::vector<std::uint32_t>>{ };
		while (!pending.empty()) {
			auto current = pending.back();
			pending.pop_back();
			while (current.LeftBegin < current.LeftEnd && current.RightBegin < current.RightEnd && aLeft[current.LeftBegin] == aRight[current.RightBegin]) {
				result.push_back({ .Left = current.LeftBegin++, .Right = current.RightBegin++ });
				steps.add(1U);
			}
			while (current.LeftBegin < current.LeftEnd && current.RightBegin < current.RightEnd && aLeft[current.LeftEnd - 1U] == aRight[current.RightEnd - 1U]) {
				result.push_back({ .Left = --current.LeftEnd, .Right = --current.RightEnd });
				steps.add(1U);
			}
			if (current.LeftBegin == current.LeftEnd || current.RightBegin == current.RightEnd) {
				continue;
			}
			counts.clear();
			for (auto line = current.LeftBegin; line < current.LeftEnd; ++line) {
				counts[aLeft[line]].push_back(line);
				steps.add(2U);
			}
			auto bestCount  = maximum_chain + 1U;
			auto bestLength = std::uint32_t{ 0U };
			auto bestLeft   = std::uint32_t{ 0U };
			auto bestRight  = std::uint32_t{ 0U };
			for (auto right = current.RightBegin; right < current.RightEnd;) {
				const auto found = counts.find(aRight[right]);
				steps.add(2U);
				if (found == counts.end() || found->second.size() > maximum_chain || found->second.size() > bestCount) {
					++right;
					continue;
				}
				auto next = right + 1U;
				for (const auto left : found->second) {
					auto leftBegin  = left;
					auto rightBegin = right;
					while (leftBegin > current.LeftBegin && rightBegin > current.RightBegin && aLeft[leftBegin - 1U] == aRight[rightBegin - 1U]) {
						--leftBegin;
						--rightBegin;
					}
					auto leftEnd  = left + 1U;
					auto rightEnd = right + 1U;
					while (leftEnd < current.LeftEnd && rightEnd < current.RightEnd && aLeft[leftEnd] == aRight[rightEnd]) {
						++leftEnd;
						++rightEnd;
					}
					auto rarest = maximum_chain + 1U;
					for (auto line = leftBegin; line < leftEnd; ++line) {
						rarest = std::min<std::uint32_t>(rarest, static_cast<std::uint32_t>(counts[aLeft[line]].size()));
					}
					steps.add(2U * (leftEnd - leftBegin) + 1U);
					const auto length = leftEnd - leftBegin;
					if (rarest < bestCount || (rarest == bestCount && length > bestLength)) {
						bestCount  = rarest;
						bestLength = length;
						bestLeft   = leftBegin;
						bestRight  = rightBegin;
					}
					next = std::max(next, rightEnd);
				}
				right = next;
			}
			if (bestLength == 0U) {
				const auto fallback = onp_matches(aLeft.subspan(current.LeftBegin, current.LeftEnd - current.LeftBegin),
												  aRight.subspan(current.RightBegin, current.RightEnd - current.RightBegin), aMeter);
				for (const auto& match : fallback) {
					result.push_back({ .Left = current.LeftBegin + match.Left, .Right = current.RightBegin + match.Right });
				}
				continue;
			}
			for (std::uint32_t offset = 0U; offset < bestLength; ++offset) {
				result.push_back({ .Left = bestLeft + offset, .Right = bestRight + offset });
			}
			pending.push_back({ .LeftBegin = bestLeft + bestLength, .LeftEnd = current.LeftEnd, .RightBegin = bestRight + bestLength, .RightEnd = current.RightEnd });
			pending.push_back({ .LeftBegin = current.LeftBegin, .LeftEnd = bestLeft, .RightBegin = current.RightBegin, .RightEnd = bestRight });
		}
		steps.flush();
		std::ranges::sort(result);
		return result;
	}
} // namespace bpr::evaluation
