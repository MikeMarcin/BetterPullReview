// [[RFC-BPR-0082]] [[BPR-1466]]. The core shape's O(NP) matcher: dtl 1.21
// (Wu, Manber and Myers) over the engine's line classes or a pair's scalars.
// Its comparisons are counted through the engine's meter, so a stop unwinds
// it from inside its loops. dtl allocates its furthest-point array with a
// plain new[], which such an unwinding leaks; the report records it.

#include "candidates/candidates.hpp"

#include "dtl/dtl.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace bpr::evaluation {
	namespace {
		/// Counts comparisons in batches, so the meter's virtual call stays
		/// out of dtl's inner loop.
		class metered_compare : public dtl::Compare<std::uint32_t>
		{
		public:
			metered_compare() = default;

			metered_compare(core::diff_component_meter* const aMeter, std::uint64_t* const aPending) noexcept
				: Meter{ aMeter }
				, Pending{ aPending }
			{
			}

			[[nodiscard]] auto impl(const std::uint32_t& aLeft, const std::uint32_t& aRight) const -> bool override
			{
				if (Meter != nullptr && ++*Pending >= 4096U) {
					Meter->tick(*Pending);
					*Pending = 0U;
				}
				return aLeft == aRight;
			}

		private:
			core::diff_component_meter* Meter   = nullptr;
			std::uint64_t*              Pending = nullptr;
		};
	} // namespace

	auto onp_matches(const std::span<const std::uint32_t> aLeft, const std::span<const std::uint32_t> aRight, core::diff_component_meter* const aMeter)
		-> std::vector<core::diff_match>
	{
		using sequence = std::vector<std::uint32_t>;
		auto pending   = std::uint64_t{ 0U };
		auto diff      = dtl::Diff<std::uint32_t, sequence, metered_compare>{
			sequence{ aLeft.begin(), aLeft.end() },
			sequence{ aRight.begin(), aRight.end() },
			metered_compare{ aMeter, &pending },
		};
		diff.compose();
		if (aMeter != nullptr) {
			aMeter->tick(pending + aLeft.size() + aRight.size());
		}
		auto result = std::vector<core::diff_match>{ };
		for (const auto& [value, info] : diff.getSes().getSequence()) {
			static_cast<void>(value);
			if (info.type == dtl::SES_COMMON) {
				result.push_back({ .Left = static_cast<std::uint32_t>(info.beforeIdx - 1), .Right = static_cast<std::uint32_t>(info.afterIdx - 1) });
			}
		}
		return result;
	}
} // namespace bpr::evaluation
