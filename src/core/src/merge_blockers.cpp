#include "bpr/core/merge_blockers.hpp"

#include <algorithm>
#include <utility>

namespace bpr::core {
	auto order_merge_blockers(std::vector<merge_blocker> aBlockers) -> std::vector<merge_blocker>
	{
		std::ranges::stable_sort(aBlockers, { }, &merge_blocker::Kind);
		auto ordered = std::vector<merge_blocker>{ };
		ordered.reserve(aBlockers.size());
		for (auto& blocker : aBlockers) {
			if (!std::ranges::contains(ordered, blocker)) {
				ordered.push_back(std::move(blocker));
			}
		}
		return ordered;
	}

	auto decide_merge_headline(const std::span<const merge_blocker> aOrdered) -> merge_headline
	{
		if (aOrdered.empty()) {
			return { };
		}
		const auto lead  = aOrdered.front().Kind;
		const auto count = static_cast<std::size_t>(std::ranges::count(aOrdered, lead, &merge_blocker::Kind));
		return { .Lead = lead, .LeadCount = count, .Others = aOrdered.size() - count };
	}
} // namespace bpr::core
