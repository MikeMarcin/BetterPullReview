/// \file
/// \brief [[BPR-1999]], RFC-BPR-0101. Cost in dollars and cents, per turn and
/// per session.

#include "bpr/core/agent_cost.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void test_the_format()
	{
		// AC04.
		expect(core::agent_cost_words(*core::parse_agent_cost("0.4123567")) == "$0.41", "0.4123567 is $0.41");
		expect(core::agent_cost_words(*core::parse_agent_cost("0.004")) == "< $0.01", "0.004 is < $0.01");
		expect(core::agent_cost_words(*core::parse_agent_cost("12")) == "$12.00", "12 is $12.00");
		expect(core::agent_cost_words(*core::parse_agent_cost("0.005")) == "$0.01", "half a cent rounds up");
		expect(core::agent_cost_words(*core::parse_agent_cost("1.995")) == "$2.00", "and carries");
		expect(core::agent_cost_words(*core::parse_agent_cost("0")) == "$0.00", "a reported zero is said, not invented");
		expect(core::parse_agent_cost("0.006319170000000001") == core::agent_cost{ 6'319'170U }, "the ninth place is kept");
		expect(!core::parse_agent_cost("") && !core::parse_agent_cost("-1") && !core::parse_agent_cost("1e3") && !core::parse_agent_cost("1.") && !core::parse_agent_cost(".5")
				   && !core::parse_agent_cost("12345678901"),
			   "anything but a plain bounded decimal is no figure");
	}

	void test_the_ledger()
	{
		// The figures installed Claude Code 2.1.319 reported for three turns of
		// one resumed session.
		const auto first  = core::account_agent_turn_cost({ }, core::parse_agent_cost("0.00279231"), false);
		const auto second = core::account_agent_turn_cost(first.Ledger, core::parse_agent_cost("0.00549827"), true);
		const auto third  = core::account_agent_turn_cost(second.Ledger, core::parse_agent_cost("0.00631917"), true);
		expect(first.Turn == core::agent_cost{ 2'792'310U } && second.Turn == core::agent_cost{ 2'705'960U } && third.Turn == core::agent_cost{ 820'900U },
			   "a resumed turn cost the difference of consecutive session figures");
		expect(third.Ledger.Total == *first.Turn + *second.Turn + *third.Turn && third.Ledger.Total == *core::parse_agent_cost("0.00631917") && third.Ledger.Known,
			   "the session's total is the sum of its turns, and equals the runtime's last figure");
		const auto none = core::account_agent_turn_cost(third.Ledger, std::nullopt, true);
		expect(!none.Turn && none.Ledger == third.Ledger, "a run that reported nothing shows nothing and changes nothing");
		const auto unknown = core::account_agent_turn_cost({ }, core::parse_agent_cost("0.5"), true);
		expect(!unknown.Turn && unknown.Ledger.Reported == core::parse_agent_cost("0.5") && !unknown.Ledger.Known, "a continued session with no earlier figure shows no turn cost");
		const auto restarted = core::account_agent_turn_cost(third.Ledger, core::parse_agent_cost("0.001"), true);
		expect(restarted.Turn == core::agent_cost{ 1'000'000U }, "a figure below the last is the runtime counting again");
		const auto fresh = core::account_agent_turn_cost(third.Ledger, core::parse_agent_cost("0.25"), false);
		expect(fresh.Turn == core::parse_agent_cost("0.25") && fresh.Ledger.Total == third.Ledger.Total + *fresh.Turn, "a turn that started its own runtime session cost its figure");
	}
} // namespace

auto main() -> int
{
	test_the_format();
	test_the_ledger();
	std::cout << "agent-cost-tests:pass\n";
	return EXIT_SUCCESS;
}
