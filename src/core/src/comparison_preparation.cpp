#include "bpr/core/comparison_preparation.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <vector>

namespace bpr::core {
	auto prepared_comparison_order(const std::size_t                aCount,
								   const std::optional<std::size_t> aCursor,
								   const std::size_t                aWindow)
		-> std::vector<std::size_t>
	{
		if (aCount == 0U || aWindow == 0U || aWindow > maximum_prepared_comparison_window
			|| (aCursor.has_value() && *aCursor >= aCount)) {
			return { };
		}
		const auto current  = aCursor.value_or(0U);
		const auto total    = std::min(aCount, aWindow);
		const auto after    = aCount - current - 1U;
		const auto before   = current;
		const auto others   = total - 1U;
		auto       backward = std::min(before, others / 4U);
		auto       forward  = std::min(after, others - backward);
		backward            = std::min(before, others - forward);

		auto order = std::vector<std::size_t>{ };
		order.reserve(total);
		order.push_back(current);
		for (std::size_t step = 1U; step <= forward; ++step) {
			order.push_back(current + step);
		}
		for (std::size_t step = 1U; step <= backward; ++step) {
			order.push_back(current - step);
		}
		return order;
	}
} // namespace bpr::core
