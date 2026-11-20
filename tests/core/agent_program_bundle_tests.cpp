/// \file
/// \brief [[BPR-2303]] (owner, 2026-11-19: "The executable I need to point
/// at is inside of an app bundle"). Which program a chosen macOS app bundle
/// runs, from its `CFBundleExecutable` and its `Contents/MacOS` listing.

#include "bpr/core/agent_program_bundle.hpp"

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

	[[nodiscard]] auto program(const std::string_view aName) -> core::app_bundle_file { return { .Name = std::string{ aName }, .Directory = false, .Executable = true }; }

	void test_what_names_a_bundle()
	{
		expect(core::names_app_bundle("/Applications/Work Agent.app"), "a .app path names a bundle");
		expect(core::names_app_bundle("/Applications/Work Agent.APP/"), "in any case, with a trailing slash");
		expect(core::names_app_bundle("Work.app"), "a bare bundle name is one too");
		expect(!core::names_app_bundle("/Applications/.app") && !core::names_app_bundle(".app"), "a name that is only the suffix is not");
		expect(!core::names_app_bundle("/Applications/Work.app/Contents/MacOS/work"), "a program inside a bundle is not the bundle");
		expect(!core::names_app_bundle("/usr/local/bin/agent") && !core::names_app_bundle("/opt/application") && !core::names_app_bundle(""), "ordinary paths are not");
	}

	void test_the_named_program()
	{
		auto       listing  = core::app_bundle_listing{ .Bundle = "/Applications/Work Agent.app/", .NamedProgram = "work-agent", .Programs = std::vector{ program("work-agent"), program("helper") } };
		const auto resolved = core::resolve_app_bundle_program(listing);
		expect(resolved && *resolved == "/Applications/Work Agent.app/Contents/MacOS/work-agent", "CFBundleExecutable picks among several programs");
		listing.Programs = std::vector{ core::app_bundle_file{ .Name = "work-agent", .Directory = false, .Executable = false } };
		expect(core::resolve_app_bundle_program(listing) == "/Applications/Work Agent.app/Contents/MacOS/work-agent", "the named file is the program whether or not it can run: that is checked where it always was");
		listing.Programs = std::vector{ program("helper") };
		expect(core::resolve_app_bundle_program(listing) == std::unexpected{ core::app_bundle_refusal::named_program_missing }, "a name Contents/MacOS does not hold is refused, not guessed past");
		listing.Programs = std::vector{ core::app_bundle_file{ .Name = "work-agent", .Directory = true, .Executable = false } };
		expect(core::resolve_app_bundle_program(listing) == std::unexpected{ core::app_bundle_refusal::named_program_missing }, "a folder of that name is not the program");
		for (const auto* const escape : { "../../../../bin/sh", ".", "..", "" }) {
			listing.NamedProgram = escape;
			listing.Programs     = std::vector{ program("work-agent") };
			expect(core::resolve_app_bundle_program(listing) == std::unexpected{ core::app_bundle_refusal::named_program_missing }, "a name that is not a plain file name never leaves Contents/MacOS");
		}
	}

	void test_without_a_name()
	{
		auto listing = core::app_bundle_listing{ .Bundle = "/Applications/Work Agent.app", .NamedProgram = std::nullopt, .Programs = std::vector{ program("work-agent") } };
		listing.Programs->push_back({ .Name = "README", .Directory = false, .Executable = false });
		listing.Programs->push_back({ .Name = "Helpers", .Directory = true, .Executable = true });
		expect(core::resolve_app_bundle_program(listing) == "/Applications/Work Agent.app/Contents/MacOS/work-agent", "with no CFBundleExecutable, the one executable file is the program");
		listing.Programs->push_back(program("helper"));
		expect(core::resolve_app_bundle_program(listing) == std::unexpected{ core::app_bundle_refusal::several_programs }, "two executables and no name: the reviewer chooses");
		listing.Programs = std::vector{ core::app_bundle_file{ .Name = "README", .Directory = false, .Executable = false } };
		expect(core::resolve_app_bundle_program(listing) == std::unexpected{ core::app_bundle_refusal::no_program }, "no executable file: nothing to run");
		listing.Programs = std::vector<core::app_bundle_file>{ };
		expect(core::resolve_app_bundle_program(listing) == std::unexpected{ core::app_bundle_refusal::no_program }, "an empty Contents/MacOS: nothing to run");
	}

	void test_no_programs_folder()
	{
		const auto named   = core::app_bundle_listing{ .Bundle = "/Applications/Odd.app", .NamedProgram = "odd", .Programs = std::nullopt };
		const auto unnamed = core::app_bundle_listing{ .Bundle = "/Applications/Odd.app", .NamedProgram = std::nullopt, .Programs = std::nullopt };
		expect(core::resolve_app_bundle_program(named) == std::unexpected{ core::app_bundle_refusal::no_programs_folder }
				   && core::resolve_app_bundle_program(unnamed) == std::unexpected{ core::app_bundle_refusal::no_programs_folder },
			   "no Contents/MacOS folder, named or not: refused");
	}

	void test_equal_inputs_equal_results()
	{
		const auto listing = core::app_bundle_listing{ .Bundle = "/A.app", .NamedProgram = std::nullopt, .Programs = std::vector{ program("b"), program("a") } };
		expect(core::resolve_app_bundle_program(listing) == core::resolve_app_bundle_program(listing), "the same listing resolves the same way");
	}
} // namespace

auto main() -> int
{
	test_what_names_a_bundle();
	test_the_named_program();
	test_without_a_name();
	test_no_programs_folder();
	test_equal_inputs_equal_results();
	std::cout << "agent-program-bundle-tests:pass\n";
	return EXIT_SUCCESS;
}
