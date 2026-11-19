#include "bpr/core/agent_workspace_expiry.hpp"

#include <algorithm>
#include <limits>
#include <numeric>

namespace bpr::core {
	namespace {
		/// The indexes of \p aFacts, least recently used first, ties by index.
		[[nodiscard]] auto oldest_first(const std::span<const retained_workspace_fact> aFacts) -> std::vector<std::size_t>
		{
			auto order = std::vector<std::size_t>(aFacts.size());
			std::iota(order.begin(), order.end(), std::size_t{ 0U });
			std::ranges::stable_sort(order, [&](const std::size_t aLeft, const std::size_t aRight) { return aFacts[aLeft].LastUseSeconds < aFacts[aRight].LastUseSeconds; });
			return order;
		}

		[[nodiscard]] auto saturating_add(const std::uint64_t aLeft, const std::uint64_t aRight) noexcept -> std::uint64_t
		{
			return aLeft > std::numeric_limits<std::uint64_t>::max() - aRight ? std::numeric_limits<std::uint64_t>::max() : aLeft + aRight;
		}
	} // namespace

	auto choose_expired_workspaces(const std::span<const retained_workspace_fact> aFacts, const agent_workspace_expiry_policy& aPolicy, const std::int64_t aNowSeconds)
		-> std::vector<workspace_expiry_choice>
	{
		const auto order    = oldest_first(aFacts);
		const auto keepDays = std::min(aPolicy.KeepDays, maximum_agent_workspace_keep_days);
		const auto cutoff   = aNowSeconds - (static_cast<std::int64_t>(keepDays) * 86400);
		auto       chosen   = std::vector<workspace_expiry_choice>{ };
		auto       total    = std::uint64_t{ 0U };
		auto       kept     = std::vector<std::size_t>{ };
		for (const auto index : order) {
			const auto& fact = aFacts[index];
			if (!fact.InUse && fact.Ended) {
				chosen.push_back({ .Index = index, .Reason = workspace_expiry_reason::ended });
			} else if (!fact.InUse && keepDays != 0U && fact.LastUseSeconds < cutoff) {
				chosen.push_back({ .Index = index, .Reason = workspace_expiry_reason::unused });
			} else {
				total = saturating_add(total, fact.Bytes);
				if (!fact.InUse) {
					kept.push_back(index);
				}
			}
		}
		const auto cap = agent_workspace_cap_bytes(aPolicy.CapGb);
		for (const auto index : kept) {
			if (total <= cap) {
				break;
			}
			chosen.push_back({ .Index = index, .Reason = workspace_expiry_reason::over_cap });
			total -= aFacts[index].Bytes;
		}
		std::ranges::stable_sort(chosen, [&](const workspace_expiry_choice& aLeft, const workspace_expiry_choice& aRight) {
			return aFacts[aLeft.Index].LastUseSeconds != aFacts[aRight.Index].LastUseSeconds ? aFacts[aLeft.Index].LastUseSeconds < aFacts[aRight.Index].LastUseSeconds
																							 : aLeft.Index < aRight.Index;
		});
		return chosen;
	}

	auto choose_all_idle_workspaces(const std::span<const retained_workspace_fact> aFacts) -> std::vector<workspace_expiry_choice>
	{
		auto chosen = std::vector<workspace_expiry_choice>{ };
		for (const auto index : oldest_first(aFacts)) {
			if (!aFacts[index].InUse) {
				chosen.push_back({ .Index = index, .Reason = workspace_expiry_reason::removed_by_reviewer });
			}
		}
		return chosen;
	}
} // namespace bpr::core
