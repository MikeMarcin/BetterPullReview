/// \file
/// \brief [[BPR-1301]]. What the text of a Claude process wrapper path alone
/// refuses, under each platform's rules, and that each reason is its own.

#include "bpr/core/agent_wrapper.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using core::agent_wrapper_platform;
	using core::agent_wrapper_refusal;
	using core::claude_found_in;
	using core::claude_search_candidate;

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

	[[nodiscard]] auto candidates(const std::string_view aPath, const std::vector<std::string>& aFolders, const agent_wrapper_platform aPlatform)
		-> std::vector<claude_search_candidate>
	{
		return core::claude_search_candidates(aPath, aFolders, aPlatform);
	}

	/// [[BPR-1301]], owner 2026-10-07. Where a launch with no wrapper looks
	/// for claude, in order, before any file is read.
	void test_posix_search_order()
	{
		const auto folders = core::usual_claude_folders("/Users/me", agent_wrapper_platform::posix);
		expect(folders
				   == std::vector<std::string>{ "/Users/me/.local/bin", "/Users/me/.claude/local", "/opt/homebrew/bin", "/usr/local/bin", "/Users/me/.npm-global/bin" },
			   "the usual folders are the native installer's, the old local install, Homebrew's two and npm's global folder, in that order");
		expect(core::usual_claude_folders("/Users/me/", agent_wrapper_platform::posix).front() == "/Users/me/.local/bin", "a home ending in / joins once");
		for (const auto* const home : { "", "relative/home", "~", "~/me" }) {
			expect(core::usual_claude_folders(home, agent_wrapper_platform::posix) == std::vector<std::string>{ "/opt/homebrew/bin", "/usr/local/bin" },
				   "a home that is not absolute adds none of its folders");
		}

		const auto found = candidates("/usr/bin::.:bin:./tools:~/bin:/opt/homebrew/bin/:/usr/bin:/odd\\dir", folders, agent_wrapper_platform::posix);
		const auto want  = std::vector<claude_search_candidate>{
			{ .Path = "/usr/bin/claude", .Origin = claude_found_in::path },
			{ .Path = "/opt/homebrew/bin/claude", .Origin = claude_found_in::path },
			{ .Path = "/odd\\dir/claude", .Origin = claude_found_in::path },
			{ .Path = "/Users/me/.local/bin/claude", .Origin = claude_found_in::usual_folder },
			{ .Path = "/Users/me/.claude/local/claude", .Origin = claude_found_in::usual_folder },
			{ .Path = "/usr/local/bin/claude", .Origin = claude_found_in::usual_folder },
			{ .Path = "/Users/me/.npm-global/bin/claude", .Origin = claude_found_in::usual_folder },
		};
		expect(found == want, "PATH in order, empty, relative, . and ~ entries skipped, then the usual folders, nothing listed twice");
		expect(candidates("", { }, agent_wrapper_platform::posix).empty(), "an empty PATH and no folders give nothing to look at");
		expect(candidates(".:bin:", { "relative", "" }, agent_wrapper_platform::posix).empty(), "relative entries and the current directory are never searched");
		expect(candidates("/a\nb:/ok", { }, agent_wrapper_platform::posix) == std::vector<claude_search_candidate>{ { .Path = "/ok/claude" } },
			   "an entry with a control character is skipped");
		const auto longest = "/" + std::string(core::maximum_agent_wrapper_bytes - 8U, 'a');
		expect(candidates(longest + ":" + longest + "a", { }, agent_wrapper_platform::posix) == std::vector<claude_search_candidate>{ { .Path = longest + "/claude" } },
			   "a result over the wrapper bound is skipped, one at the bound is kept");
		expect(candidates("/opt/x", { }, agent_wrapper_platform::posix).front().Path.ends_with("/claude"), "POSIX looks for claude, never claude.exe");
	}

	void test_windows_search_order()
	{
		const auto folders = core::usual_claude_folders(R"(C:\Users\me)", agent_wrapper_platform::windows);
		expect(folders == std::vector<std::string>{ R"(C:\Users\me\.local\bin)" }, "Windows adds only the native installer's folder");
		expect(core::usual_claude_folders("relative", agent_wrapper_platform::windows).empty(), "a relative home adds nothing on Windows");
		const auto found = candidates(R"(C:\Windows;;.;C:relative;\tools;\\server\share\;C:/tools/;"C:\quoted")", folders, agent_wrapper_platform::windows);
		const auto want  = std::vector<claude_search_candidate>{
			{ .Path = R"(C:\Windows\claude.exe)", .Origin = claude_found_in::path },
			{ .Path = R"(\\server\share\claude.exe)", .Origin = claude_found_in::path },
			{ .Path = "C:/tools/claude.exe", .Origin = claude_found_in::path },
			{ .Path = R"(C:\Users\me\.local\bin\claude.exe)", .Origin = claude_found_in::usual_folder },
		};
		expect(found == want, "Windows splits on ;, keeps drive, UNC and slash paths, skips empty, ., drive-relative, root-relative and quoted entries");
		expect(std::ranges::all_of(found, [](const claude_search_candidate& aFound) { return !core::inspect_agent_wrapper_text(aFound.Path, agent_wrapper_platform::windows); }),
			   "every Windows candidate is a path a wrapper could be: an absolute .exe");
	}
}

auto main() -> int
{
	test_posix_paths();
	test_windows_paths();
	test_reasons_and_storage();
	test_posix_search_order();
	test_windows_search_order();
	std::cout << "agent-wrapper-tests:pass\n";
	return EXIT_SUCCESS;
}
