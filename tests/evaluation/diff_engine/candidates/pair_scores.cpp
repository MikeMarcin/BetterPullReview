// [[RFC-BPR-0082]] [[BPR-1466]]. The core shape's pair similarity:
// rapidfuzz-cpp 3.3.4's normalized Indel, which has the engine's formula,
// 2·LCS/(L+R). The score is computed in integers from Indel's distance so it
// rounds exactly as the engine does. A distance past the threshold's cutoff
// is not needed exactly: such a pair is never paired.

#include "candidates/candidates.hpp"

#include "rapidfuzz/distance/Indel.hpp"

#include <cstdint>
#include <vector>

namespace bpr::evaluation {
	auto rapidfuzz_scores(const core::diff_pairing_request& aRequest, core::diff_component_meter* const aMeter) -> std::vector<std::uint16_t>
	{
		constexpr auto maximum = std::uint64_t{ core::maximum_similarity_threshold_permille };
		auto           result  = std::vector<std::uint16_t>{ };
		result.reserve(aRequest.Left.size() * aRequest.Right.size());
		for (const auto left : aRequest.Left) {
			const auto cached = rapidfuzz::CachedIndel<std::uint32_t>{ left.begin(), left.end() };
			for (const auto right : aRequest.Right) {
				const auto total = static_cast<std::uint64_t>(left.size()) + static_cast<std::uint64_t>(right.size());
				if (aMeter != nullptr) {
					aMeter->tick(1U + total);
				}
				if (total == 0U) {
					result.push_back(core::maximum_similarity_threshold_permille);
					continue;
				}
				if (left.empty() || right.empty()) {
					result.push_back(0U);
					continue;
				}
				// The largest distance that still reaches the threshold.
				const auto cutoff   = total * (maximum - aRequest.ThresholdPermille) / maximum + 1U;
				const auto distance = static_cast<std::uint64_t>(cached.distance(right.begin(), right.end(), static_cast<std::size_t>(cutoff)));
				const auto score    = distance > cutoff ? std::uint64_t{ 0U } : (total - distance) * maximum / total;
				result.push_back(static_cast<std::uint16_t>(score));
			}
		}
		return result;
	}
} // namespace bpr::evaluation
