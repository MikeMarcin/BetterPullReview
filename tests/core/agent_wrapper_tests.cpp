/// \file
/// \brief [[BPR-1301]]. What the text of a Claude process wrapper path alone
/// refuses, under each platform's rules, and that each reason is its own.

#include "bpr/core/agent_wrapper.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;
	using core::agent_wrapper_platform;
	using core::agent_wrapper_refusal;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto posix(const std::string_view aPath) -> std::optional<agent_wrapper_refusal>
	{
		return core::inspect_agent_wrapper_text(aPath, agent_wrapper_platform::posix);
	}

	[[nodiscard]] auto windows(const std::string_view aPath) -> std::optional<agent_wrapper_refusal>
	{
		return core::inspect_agent_wrapper_text(aPath, agent_wrapper_platform::windows);
	}

	void test_posix_paths()
	{
		expect(!posix("/opt/homebrew/bin/claude"), "an absolute path is admitted by its text");
		expect(!posix("/Users/me/bin/claude gateway.sh"), "a space is part of the path, never a separator");
		expect(!posix("/usr/local/bin/claude.cmd"), "a .cmd name is only a Windows refusal");
		expect(posix("claude") == agent_wrapper_refusal::relative, "the bare name is relative");
		expect(posix("bin/claude") == agent_wrapper_refusal::relative, "a relative path is refused");
		expect(posix("./claude") == agent_wrapper_refusal::relative, "a dot path is refused");
		expect(posix("") == agent_wrapper_refusal::relative, "empty text asked anyway is relative");
		expect(posix("~/bin/claude") == agent_wrapper_refusal::home_relative, "a ~ path has its own reason");
		expect(posix("~") == agent_wrapper_refusal::home_relative, "a lone ~ is home-relative");
		expect(posix("/bin/claude\n--dangerously") == agent_wrapper_refusal::control_character, "a line break is refused");
		expect(posix("/bin/claude\r") == agent_wrapper_refusal::control_character, "a carriage return is refused");
		expect(posix(std::string_view{ "/bin/cla\0ude", 12U }) == agent_wrapper_refusal::control_character, "a NUL is refused");
		expect(posix("/bin/claude\t") == agent_wrapper_refusal::control_character, "a tab is refused");
		const auto longest = "/" + std::string(core::maximum_agent_wrapper_bytes - 1U, 'a');
		expect(!posix(longest), "the bound itself is admitted");
		expect(posix(longest + "a") == agent_wrapper_refusal::too_long, "one byte over the bound is refused");
		expect(posix("~" + std::string(core::maximum_agent_wrapper_bytes, 'a')) == agent_wrapper_refusal::too_long, "length is read before the ~");
	}

	void test_windows_paths()
	{
		expect(!windows(R"(C:\Users\me\.local\bin\claude.exe)"), "a drive path to an .exe is admitted");
		expect(!windows("C:/tools/claude.EXE"), "forward slashes and any case are admitted");
		expect(!windows(R"(\\server\share\claude.exe)"), "a UNC path is absolute");
		expect(!windows(R"(\\?\C:\tools\claude.exe)"), "a device path is absolute");
		expect(windows(R"(C:\tools\claude.cmd)") == agent_wrapper_refusal::windows_script, "a .cmd runs through cmd.exe");
		expect(windows(R"(C:\tools\claude.bat)") == agent_wrapper_refusal::windows_script, "a .bat runs through cmd.exe");
		expect(windows(R"(C:\tools\CLAUDE.CMD)") == agent_wrapper_refusal::windows_script, "the script check ignores case");
		expect(windows(R"(C:\tools\claude.ps1)") == agent_wrapper_refusal::windows_not_program, "a PowerShell script is not a program");
		expect(windows(R"(C:\tools\claude)") == agent_wrapper_refusal::windows_not_program, "a name without .exe is refused");
		expect(windows(R"(C:\tools\claude.exe.)") == agent_wrapper_refusal::windows_not_program, "a trailing dot Windows would trim is refused");
		expect(windows(R"(C:\tools\claude.exe )") == agent_wrapper_refusal::windows_not_program, "a trailing space Windows would trim is refused");
		expect(windows(R"(C:\tools\claude.cmd:x.exe)") == agent_wrapper_refusal::windows_not_program, "a stream of a script is refused");
		expect(windows(R"(C:\tools\claude.exe:hidden.exe)") == agent_wrapper_refusal::windows_not_program, "any alternate data stream is refused");
		expect(windows(R"(C:\tools.exe\claude)") == agent_wrapper_refusal::windows_not_program, "only the final name's extension counts");
		expect(windows(R"(claude.exe)") == agent_wrapper_refusal::relative, "a bare .exe is relative");
		expect(windows(R"(C:claude.exe)") == agent_wrapper_refusal::relative, "a drive-relative path is relative");
		expect(windows(R"(\tools\claude.exe)") == agent_wrapper_refusal::relative, "a root-relative path is relative");
		expect(windows("/usr/bin/claude.exe") == agent_wrapper_refusal::relative, "a POSIX root is not absolute on Windows");
		expect(windows(R"(~\claude.exe)") == agent_wrapper_refusal::home_relative, "a ~ path has its own reason on Windows");
	}

	void test_reasons_and_storage()
	{
		auto lines = std::set<std::string_view>{ };
		for (auto index = 0; index <= static_cast<int>(agent_wrapper_refusal::not_executable); ++index) {
			const auto line = core::describe(static_cast<agent_wrapper_refusal>(index));
			expect(!line.empty() && line.find('\n') == std::string_view::npos, "each reason is one line");
			lines.insert(line);
		}
		expect(lines.size() == static_cast<std::size_t>(agent_wrapper_refusal::not_executable) + 1U, "each refusal has its own reason");
		expect(core::describe(agent_wrapper_refusal::windows_script).find("cmd.exe") != std::string_view::npos, "the script reason names the shell");
		expect(core::storable_agent_wrapper(""), "no wrapper is storable");
		expect(core::storable_agent_wrapper("relative/is/storable"), "storage bounds text, not meaning");
		expect(!core::storable_agent_wrapper("/a\nb"), "a line break is never stored");
		expect(!core::storable_agent_wrapper(std::string(core::maximum_agent_wrapper_bytes + 1U, 'a')), "an over-long path is never stored");
	}
}

auto main() -> int
{
	test_posix_paths();
	test_windows_paths();
	test_reasons_and_storage();
	std::cout << "agent-wrapper-tests:pass\n";
	return EXIT_SUCCESS;
}
