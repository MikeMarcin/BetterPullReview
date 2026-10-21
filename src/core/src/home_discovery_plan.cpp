#include "bpr/core/home_discovery_plan.hpp"

#include <algorithm>
#include <tuple>

namespace bpr::core {
	auto discovery_known_pull_requests(const std::span<const discovery_retained_read> aReads) -> std::vector<discovery_retained_read>
	{
		auto known = std::vector<discovery_retained_read>{ };
		for (const auto& read : aReads) {
			if (read.Open && read.Complete && valid(read.Repository) && read.Number.value() != 0U) {
				known.push_back(read);
			}
		}
		const auto key = [](const discovery_retained_read& aRead) { return std::tie(aRead.Repository, aRead.Number); };
		std::ranges::stable_sort(known, [&key](const discovery_retained_read& aLeft, const discovery_retained_read& aRight) { return key(aLeft) < key(aRight); });
		const auto [first, last] = std::ranges::unique(known, [&key](const discovery_retained_read& aLeft, const discovery_retained_read& aRight) { return key(aLeft) == key(aRight); });
		known.erase(first, last);
		return known;
	}

	auto keeps_reused_row(const discovery_search_reason aReason, const discovery_retained_read& aRead) noexcept -> bool
	{
		return aReason == discovery_search_reason::authored || aRead.RequestsViewer;
	}
} // namespace bpr::core
