// [[BPR-NNNN]]. Naming and deleting an agent session: the core's rules.
#include "bpr/core/agent_session_management.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {
	using namespace bpr::core;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void test_names()
	{
		expect(tidy_agent_session_name("  Review the parser  ") == std::string{ "Review the parser" }, "a name loses its outer spaces");
		expect(tidy_agent_session_name("two\n\tlines   here") == std::string{ "two lines here" }, "runs of white space and control characters become one space");
		expect(tidy_agent_session_name("   ") == std::string{ }, "blank restores the default title");
		expect(tidy_agent_session_name("") == std::string{ }, "empty restores the default title");
		expect(tidy_agent_session_name("Überprüfung – naïve") == std::string{ "Überprüfung – naïve" }, "non-ASCII words stay as typed");
		expect(tidy_agent_session_name(std::string(maximum_agent_session_name_bytes, 'a')).has_value(), "the longest name is kept");
		expect(tidy_agent_session_name(std::string(maximum_agent_session_name_bytes + 1U, 'a')).error() == agent_session_name_refusal::too_long, "a longer one is refused");
		expect(tidy_agent_session_name(std::string(maximum_agent_session_name_bytes, 'a') + "   ").has_value(), "trailing spaces do not count toward the bound");
		expect(tidy_agent_session_name("bad\xFF").error() == agent_session_name_refusal::not_text, "malformed UTF-8 is refused");
		expect(tidy_agent_session_name(std::string_view{ "nul\0x", 5U }).error() == agent_session_name_refusal::not_text, "a NUL is refused");
	}

	void test_titles()
	{
		expect(agent_session_title("Parser review", "What changed?") == "Parser review", "a name is the title");
		expect(agent_session_title("", "What changed?") == "What changed?", "without a name, the first question");
		expect(agent_session_title("", "") == "New session", "an empty session is a New session");
	}

	void test_deletion()
	{
		using refusal = agent_session_deletion_refusal;
		expect(plan_agent_session_deletion({ }).Refusal == refusal::not_found, "an unknown session is not deleted");
		expect(plan_agent_session_deletion({ .Found = true, .Running = true, .HoldsWorkspace = true }).Refusal == refusal::running, "a running session is refused");
		const auto alone = plan_agent_session_deletion({ .Found = true, .HoldsWorkspace = true });
		expect(alone.Refusal == refusal::none && alone.RemoveWorkspace && !alone.WorkspaceShared, "its own workspace goes with it");
		const auto shared = plan_agent_session_deletion({ .Found = true, .HoldsWorkspace = true, .OtherHolders = 1U });
		expect(shared.Refusal == refusal::none && !shared.RemoveWorkspace && shared.WorkspaceShared, "a workspace another session uses stays");
		const auto none = plan_agent_session_deletion({ .Found = true });
		expect(none.Refusal == refusal::none && !none.RemoveWorkspace && !none.WorkspaceShared, "a session with no workspace only loses its record");
	}
}

auto main() -> int
{
	test_names();
	test_titles();
	test_deletion();
	std::cout << "core_agent_session_management passed\n";
	return EXIT_SUCCESS;
}
