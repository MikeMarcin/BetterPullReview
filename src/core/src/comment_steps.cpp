#include "bpr/core/comment_steps.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

namespace bpr::core {
	auto decide_comment_steps(std::vector<comment_step_stop> aStops, const std::string_view aOpen) -> comment_steps
	{
		std::ranges::stable_sort(aStops, [](const comment_step_stop& aLeft, const comment_step_stop& aRight) {
			if (aLeft.Shown.has_value() != aRight.Shown.has_value()) {
				return aLeft.Shown.has_value();
			}
			if (aLeft.Shown.has_value()) {
				return std::pair{ *aLeft.Shown, aLeft.Order } < std::pair{ *aRight.Shown, aRight.Order };
			}
			return std::pair{ aLeft.Line, aLeft.Order } < std::pair{ aRight.Line, aRight.Order };
		});
		auto result = comment_steps{ };
		result.Stops.reserve(aStops.size());
		std::ranges::transform(aStops, std::back_inserter(result.Stops), &comment_step_stop::Id);
		if (result.Stops.empty()) {
			return result;
		}
		const auto open = aOpen.empty() ? result.Stops.end() : std::ranges::find(result.Stops, aOpen);
		if (open == result.Stops.end()) {
			result.Previous = result.Stops.back();
			result.Next     = result.Stops.front();
			return result;
		}
		if (open != result.Stops.begin()) {
			result.Previous = *std::prev(open);
		}
		if (std::next(open) != result.Stops.end()) {
			result.Next = *std::next(open);
		}
		return result;
	}
} // namespace bpr::core
