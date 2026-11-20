/// \file
/// \brief [[BPR-2293]]. Which retained agent workspaces expire: those of an
/// ended conversation, those unused past the days, then the oldest beyond the
/// total cap, and never one a turn holds.

#include "bpr/core/agent_workspace_expiry.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	constexpr auto day = std::int64_t{ 86400 };
	constexpr auto gb  = std::uint64_t{ 1000ULL * 1000ULL * 1000ULL };
	constexpr auto now = std::int64_t{ 1'800'000'000 };

	[[nodiscard]] auto fact(const std::int64_t aDaysAgo, const std::uint64_t aGb, const bool aInUse = false, const bool aEnded = false) -> core::retained_workspace_fact
	{
		auto value           = core::retained_workspace_fact{ };
		value.LastUseSeconds = now - (aDaysAgo * day);
		value.Bytes          = aGb * gb;
		value.InUse          = aInUse;
		value.Ended          = aEnded;
		return value;
	}

	[[nodiscard]] auto indexes(const std::vector<core::workspace_expiry_choice>& aChoices) -> std::vector<std::size_t>
	{
		auto value = std::vector<std::size_t>{ };
		for (const auto& choice : aChoices) {
			value.push_back(choice.Index);
		}
		return value;
	}

	void test_nothing_to_expire()
	{
		const auto facts = std::vector{ fact(1, 5), fact(2, 5) };
		expect(core::choose_expired_workspaces(facts, { }, now).empty(), "recent workspaces under the cap stay");
		expect(core::choose_expired_workspaces({ }, { }, now).empty(), "no workspaces, nothing chosen");
	}

	void test_unused_and_ended()
	{
		// 0: eight days unused; 1: ended yesterday; 2: recent; 3: ten days but in use.
		const auto facts  = std::vector{ fact(8, 8), fact(1, 1, false, true), fact(2, 1), fact(10, 1, true, true) };
		const auto chosen = core::choose_expired_workspaces(facts, { }, now);
		expect(indexes(chosen) == std::vector<std::size_t>{ 0U, 1U }, "the unused and the ended go, oldest first; one in use stays");
		expect(chosen[0].Reason == core::workspace_expiry_reason::unused && chosen[1].Reason == core::workspace_expiry_reason::ended, "each says why");
		auto keepAll     = core::agent_workspace_expiry_policy{ };
		keepAll.KeepDays = 0U;
		expect(indexes(core::choose_expired_workspaces(facts, keepAll, now)) == std::vector<std::size_t>{ 1U }, "zero days keeps by age; an ended one still goes");
		auto exactly = std::vector{ fact(7, 1) };
		expect(core::choose_expired_workspaces(exactly, { }, now).empty(), "exactly the kept days is not more than them");
	}

	void test_cap_oldest_first()
	{
		// 13 workspaces of 8.7 GB, as the owner found, one a day: 113 GB.
		auto facts = std::vector<core::retained_workspace_fact>{ };
		for (std::int64_t ago = 0; ago < 13; ++ago) {
			auto value  = fact(ago, 0);
			value.Bytes = 8'700'000'000ULL;
			facts.push_back(value);
		}
		auto policy     = core::agent_workspace_expiry_policy{ };
		policy.KeepDays = 0U;
		const auto cap  = core::choose_expired_workspaces(facts, policy, now);
		// 20 GB holds two of 8.7 GB: the eleven oldest go, oldest first.
		expect(cap.size() == 11U, "the cap leaves two");
		expect(cap.front().Index == 12U && cap.back().Index == 2U, "the oldest goes first, the newest two stay");
		expect(std::ranges::all_of(cap, [](const auto& aChoice) { return aChoice.Reason == core::workspace_expiry_reason::over_cap; }), "each is over the cap");
		// With the default seven days, the five older than a week go for age and
		// the rest of the cap is then met by the oldest of the others.
		const auto both = core::choose_expired_workspaces(facts, { }, now);
		expect(both.size() == 11U, "age and cap together leave two");
		expect(std::ranges::count(both, core::workspace_expiry_reason::unused, &core::workspace_expiry_choice::Reason) == 5, "the five older than seven days go for age");
	}

	void test_in_use_counts_and_stays()
	{
		// The newest is in use and alone over the cap: it stays, and every
		// other one goes.
		const auto facts  = std::vector{ fact(0, 25, true), fact(1, 1), fact(2, 1) };
		const auto chosen = core::choose_expired_workspaces(facts, { }, now);
		expect(indexes(chosen) == std::vector<std::size_t>{ 2U, 1U }, "a workspace in use counts toward the cap and is never chosen");
	}

	void test_remove_all()
	{
		const auto facts  = std::vector{ fact(0, 1, true), fact(3, 1), fact(5, 1, false, true) };
		const auto chosen = core::choose_all_idle_workspaces(facts);
		expect(indexes(chosen) == std::vector<std::size_t>{ 2U, 1U }, "Remove takes every one not in use, oldest first");
		expect(chosen[0].Reason == core::workspace_expiry_reason::removed_by_reviewer, "Remove says the reviewer removed it");
	}

	void test_deterministic_ties()
	{
		const auto facts = std::vector{ fact(9, 1), fact(9, 1), fact(9, 1) };
		expect(indexes(core::choose_expired_workspaces(facts, { }, now)) == std::vector<std::size_t>{ 0U, 1U, 2U }, "equal last use breaks by index");
		expect(core::agent_workspace_cap_bytes(0U) == gb && core::agent_workspace_cap_bytes(20U) == 20U * gb, "the cap is held to at least 1 GB");
	}
} // namespace

auto main() -> int
{
	test_nothing_to_expire();
	test_unused_and_ended();
	test_cap_oldest_first();
	test_in_use_counts_and_stays();
	test_remove_all();
	test_deterministic_ties();
	std::cout << "agent-workspace-expiry-tests:pass\n";
	return EXIT_SUCCESS;
}
