/// \file
/// \brief [[BPR-2304]]. The rules a Claude stream-JSON launch's own
/// arguments keep, as the owner's harness audits established them.

#include "bpr/core/agent_run_policy.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using arguments = std::vector<std::string>;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto problem_of(const arguments& aArguments, const core::claude_argument_expectation aExpected = { }) -> std::optional<core::claude_argument_problem>
	{
		return core::inspect_claude_arguments(aArguments, aExpected);
	}

	/// The shape the audit recommends, less the prompt, which rides stdin.
	[[nodiscard]] auto isolated() -> arguments
	{
		return { "-p", "--append-system-prompt", "Answer briefly.", "--setting-sources", "user", "--mcp-config", std::string{ core::claude_empty_mcp_configuration }, "--strict-mcp-config", "--input-format",
				 "stream-json", "--output-format", "stream-json", "--verbose" };
	}

	void test_the_recommended_shape_passes()
	{
		expect(!problem_of(isolated(), { .UserSettingsOnly = true, .McpOff = true }), "the isolated shape keeps every rule");
		expect(!problem_of({ "-p", "--input-format", "stream-json", "--output-format", "stream-json", "--verbose" }), "a plain run with nothing expected passes");
		expect(!problem_of({ "--print", "--verbose" }), "--print is print mode too");
		expect(problem_of({ "--input-format", "stream-json", "--verbose" }) == core::claude_argument_problem::no_print_mode, "a launch with no -p would be interactive");
	}

	void test_instructions_are_never_given_as_a_file()
	{
		for (const auto& file : { arguments{ "--append-system-prompt-file", "/tmp/rules.md", "-p" }, arguments{ "--append-system-prompt-file=/tmp/rules.md", "-p" }, arguments{ "--system-prompt-file", "/tmp/rules.md", "-p" } }) {
			expect(problem_of(file) == core::claude_argument_problem::instructions_file, "the file form, which a wrapper drops, is refused: " + file.front());
		}
		expect(!problem_of({ "--append-system-prompt", "rules", "-p" }), "the inline form passes");
	}

	/// RFC-BPR-0104 AC11, amended 2026-11-19: the owner's harness audit
	/// found the order makes no difference, so only being closed counts.
	void test_several_valued_options_are_closed_wherever_they_stand()
	{
		for (const auto& option : core::claude_variadic_options) {
			const auto name = std::string{ option };
			expect(!problem_of({ name, "a", "b", "--verbose", "-p" }), "followed by another option before -p: " + name);
			expect(!problem_of({ name, "a", "-p" }), "closed by -p itself: " + name);
			expect(!problem_of({ "-p", name, "a", "--verbose" }), "closed after -p (RFC-BPR-0104 AC11, amended 2026-11-19): " + name);
			expect(!problem_of({ "-p", name + "=a", "--verbose" }), "the = spelling closed after -p: " + name);
			expect(problem_of({ "-p", name, "a" }) == core::claude_argument_problem::open_variadic, "left open after -p, it would take the prompt: " + name);
			expect(problem_of({ "-p", name }) == core::claude_argument_problem::open_variadic, "left open with no value yet: " + name);
			expect(problem_of({ "-p", name + "=a" }) == core::claude_argument_problem::open_variadic, "the = spelling left open too: " + name);
			expect(problem_of({ "-p", "--verbose", name, "a", "b" }) == core::claude_argument_problem::open_variadic, "the last option given is the open one: " + name);
		}
		expect(!problem_of({ "-p", "--file-limit", "3" }), "an option that only starts like one is not it");
	}

	void test_a_scoped_write_carries_its_edit_rule()
	{
		const auto write = std::string{ "Write(//tmp/scratch/**)" };
		const auto edit  = std::string{ "Edit(//tmp/scratch/**)" };
		expect(problem_of({ "--allowedTools", write, "-p" }) == core::claude_argument_problem::write_rule_without_edit, "a Write rule alone writes nothing");
		expect(problem_of({ "--allowedTools", write, "Edit(//tmp/other/**)", "-p" }) == core::claude_argument_problem::write_rule_without_edit, "the Edit rule names the same path");
		expect(!problem_of({ "--allowedTools", write, edit, "Bash(python3 /tmp/scratch/*)", "-p" }), "Write beside its Edit passes");
		expect(!problem_of({ "--allowedTools", edit, "-p" }), "Edit alone is the rule that works");
		expect(!problem_of({ "--allowed-tools", write + "," + edit, "-p" }), "comma-separated rules in one value, the other spelling");
		expect(!problem_of({ "--allowedTools", "Edit(//tmp/a b/**) Write(//tmp/a b/**)", "-p" }), "a space inside a rule's parentheses does not split it");
		expect(!problem_of({ "--allowedTools", edit, "--allowedTools", write, "-p" }), "rules from every --allowedTools count together");
		expect(!problem_of({ "--disallowedTools", "Write", "-p" }), "a denial needs no Edit rule");
	}

	void test_only_the_users_settings_load()
	{
		expect(problem_of({ "-p", "--setting-sources", "user,project" }) == core::claude_argument_problem::project_settings, "the working folder's settings never load");
		expect(problem_of({ "-p", "--setting-sources=local" }) == core::claude_argument_problem::project_settings, "the = spelling too");
		expect(!problem_of({ "-p", "--setting-sources", "user" }, { .UserSettingsOnly = true }), "user only passes");
		expect(problem_of({ "-p" }, { .UserSettingsOnly = true }) == core::claude_argument_problem::settings_not_user_only, "a checkout's run that names no source would read the project's");
		expect(problem_of({ "-p", "--setting-sources", "" }, { .UserSettingsOnly = true }) == core::claude_argument_problem::settings_not_user_only, "no source at all loses the user's sign-in");
		expect(!problem_of({ "-p" }), "the reviewer's own folder reads its settings");
	}

	void test_mcp_off_is_strict_and_empty()
	{
		const auto empty = std::string{ core::claude_empty_mcp_configuration };
		expect(!problem_of({ "--strict-mcp-config", "--mcp-config", empty, "-p" }, { .McpOff = true }), "strict and empty passes");
		expect(problem_of({ "--mcp-config", empty, "-p" }, { .McpOff = true }) == core::claude_argument_problem::mcp_not_off, "without --strict-mcp-config the agent's own servers still load");
		expect(problem_of({ "--strict-mcp-config", "-p" }, { .McpOff = true }) == core::claude_argument_problem::mcp_not_off, "strict needs the empty configuration named");
		expect(problem_of({ "--strict-mcp-config", "--mcp-config", empty, R"({"mcpServers":{"x":{}}})", "-p" }, { .McpOff = true }) == core::claude_argument_problem::mcp_not_off, "no other configuration rides along");
		expect(!problem_of({ "-p" }), "with the agent's own servers nothing is expected");
	}

	void test_the_first_problem_is_reported_deterministically()
	{
		const auto broken = arguments{ "--append-system-prompt-file", "/tmp/rules.md", "--allowedTools", "Write(//tmp/x/**)", "-p", "--add-dir", "/tmp/x", "--setting-sources", "project" };
		expect(problem_of(broken, { .UserSettingsOnly = true, .McpOff = true }) == core::claude_argument_problem::instructions_file, "the first rule in order is the one named");
		expect(problem_of(broken) == problem_of(broken), "equal arguments give equal answers");
		expect(core::stable_name(core::claude_argument_problem::open_variadic) == "open-variadic" && core::stable_name(core::claude_argument_problem::write_rule_without_edit) == "write-rule-without-edit",
			   "each problem has a stable name");
	}
} // namespace

auto main() -> int
{
	test_the_recommended_shape_passes();
	test_instructions_are_never_given_as_a_file();
	test_several_valued_options_are_closed_wherever_they_stand();
	test_a_scoped_write_carries_its_edit_rule();
	test_only_the_users_settings_load();
	test_mcp_off_is_strict_and_empty();
	test_the_first_problem_is_reported_deterministically();
	std::cout << "claude-launch-arguments-tests:pass\n";
	return EXIT_SUCCESS;
}
