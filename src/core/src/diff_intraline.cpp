#include "diff_intraline.hpp"

#include <dtl/dtl.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

// [[BPR-1481]] The only core source that names dtl (architecture_boundaries).
namespace bpr::core::detail {
	namespace {
		static_assert(intraline_dtl_cells_limit < dtl::MAX_CORDINATES_SIZE,
					  "dtl must never reach its restart past MAX_CORDINATES_SIZE path points");

		/// Scalar comparisons between two step increments, so the meter's
		/// stop check stays out of dtl's inner loop.
		inline constexpr std::uint64_t comparison_batch = 4'096U;

		/// What the comparator shares with its caller: dtl copies the
		/// comparator, so its counts live here.
		struct comparison_state
		{
			diff_step_meter* Meter   = nullptr;
			std::uint64_t    Pending = 0U;
			bool             Stopped = false;
		};

		/// Counts dtl's comparisons through the engine's meter. Once a stop is
		/// requested, every pair compares equal: dtl's snakes then run to the
		/// grid's edge and it finishes in one more pass, with a result that is
		/// dropped. The stop is caught here, so it never unwinds through dtl's
		/// plain `new[]`.
		class metered_compare final : public dtl::Compare<std::uint32_t>
		{
		public:
			metered_compare() = default;

			explicit metered_compare(comparison_state* const aState) noexcept
				: State{ aState }
			{
			}

			[[nodiscard]] auto impl(const std::uint32_t& aLeft, const std::uint32_t& aRight) const -> bool override
			{
				if (State->Stopped) {
					return true;
				}
				if (++State->Pending >= comparison_batch) {
					const auto pending = State->Pending;
					State->Pending     = 0U;
					try {
						State->Meter->tick(pending);
					} catch (const diff_compute_stopped&) {
						State->Stopped = true;
						return true;
					}
				}
				return aLeft == aRight;
			}

		private:
			comparison_state* State = nullptr;
		};
	} // namespace

	auto intraline_common_scalars(const std::span<const std::uint32_t> aLeft,
								  const std::span<const std::uint32_t> aRight,
								  diff_step_meter&                     aMeter)
		-> std::optional<std::vector<std::pair<std::size_t, std::size_t>>>
	{
		if (aLeft.empty() || aRight.empty()) {
			return std::vector<std::pair<std::size_t, std::size_t>>{ };
		}
		const auto rows    = static_cast<std::uint64_t>(aLeft.size()) + 1U;
		const auto columns = static_cast<std::uint64_t>(aRight.size()) + 1U;
		if (rows > intraline_dtl_cells_limit || columns > intraline_dtl_cells_limit / rows) {
			return std::nullopt;
		}
		using sequence = std::vector<std::uint32_t>;
		auto state     = comparison_state{ .Meter = &aMeter };
		auto diff      = dtl::Diff<std::uint32_t, sequence, metered_compare>{
			sequence{ aLeft.begin(), aLeft.end() },
			sequence{ aRight.begin(), aRight.end() },
			metered_compare{ &state },
		};
		diff.compose();
		if (state.Stopped) {
			throw diff_compute_stopped{ };
		}
		aMeter.tick(state.Pending + aLeft.size() + aRight.size());
		const auto script = diff.getSes().getSequence();
		auto       result = std::vector<std::pair<std::size_t, std::size_t>>{ };
		for (const auto& [value, info] : script) {
			static_cast<void>(value);
			if (info.type != dtl::SES_COMMON) {
				continue;
			}
			// A defect in dtl must never become a span: a common scalar out of
			// order, out of range or unequal gives the pair the matrix table.
			const auto left  = static_cast<std::size_t>(info.beforeIdx - 1);
			const auto right = static_cast<std::size_t>(info.afterIdx - 1);
			const auto after = result.empty() || (left > result.back().first && right > result.back().second);
			if (info.beforeIdx < 1 || info.afterIdx < 1 || left >= aLeft.size() || right >= aRight.size() || !after || aLeft[left] != aRight[right]) {
				return std::nullopt;
			}
			result.emplace_back(left, right);
		}
		aMeter.tick(script.size());
		return result;
	}
} // namespace bpr::core::detail
