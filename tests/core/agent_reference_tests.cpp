/// \file
/// \brief [[BPR-1998]], RFC-BPR-0101. One recogniser for the code references
/// of every agent reply.

#include "bpr/core/agent_reference.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
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

	struct form
	{
		std::string_view Written;
		std::uint32_t    First  = 0U;
		std::uint32_t    Last   = 0U;
		bool             Before = false;
	};

	const auto files = core::agent_reference_files{ .Changed = { "src/engine/renderer.cpp", "src/engine/frame_pool.cpp", "src/a/util.hpp", "src/b/util.hpp" } };

	void test_every_listed_form_is_recognised()
	{
		const auto forms = std::vector<form>{
			{ "src/engine/renderer.cpp:12", 12U, 12U },
			{ "src/engine/renderer.cpp:12-34", 12U, 34U },
			{ "src/engine/renderer.cpp:12\xE2\x80\x93"
			  "34",
			  12U, 34U },
			{ "src/engine/renderer.cpp:12:5", 12U, 12U },
			{ "src/engine/renderer.cpp#L12", 12U, 12U },
			{ "src/engine/renderer.cpp#L12-L34", 12U, 34U },
			{ "src/engine/renderer.cpp line 12", 12U, 12U },
			{ "src/engine/renderer.cpp lines 12\xE2\x80\x93"
			  "34",
			  12U, 34U },
			{ "src/engine/renderer.cpp lines 12 to 34", 12U, 34U },
			{ "src/engine/renderer.cpp lines 12-34", 12U, 34U },
			{ "src/engine/renderer.cpp (lines 12\xE2\x80\x93"
			  "34)",
			  12U, 34U },
			{ "src/engine/renderer.cpp:12 before the change", 12U, 12U, true },
			{ "src/engine/renderer.cpp lines 12 to 34 before the change", 12U, 34U, true },
			{ "renderer.cpp:27-29", 27U, 29U },
		};
		for (const auto& [written, first, last, before] : forms) {
			const auto prose = "See " + std::string{ written } + ", which is wrong.";
			const auto found = core::find_agent_references(prose, false);
			expect(found.size() == 1U && found.front().Begin == 4U && found.front().Length == written.size() && found.front().First == first && found.front().Last == last
					   && found.front().Before == before,
				   "a listed form is one reference covering what was written: " + std::string{ written });
		}
		expect(core::find_agent_references("in line 12 and lines 3 to 4", false).empty(), "a bare line names nothing without a file");
		const auto bare = core::find_agent_references("in line 12 and lines 3 to 4", true);
		expect(bare.size() == 2U && bare[0].Path.empty() && bare[0].First == 12U && bare[1].First == 3U && bare[1].Last == 4U, "with one file, bare lines are references to it");
		expect(core::find_agent_references("version 1.2:3 and foo.cpp:abc and see.cpp:0", false).empty(), "a word with a dot, letters after a colon, and line zero are not references");
		expect(core::find_agent_references("example.com/a.cpp:12", false).size() == 1U, "a dotted folder is still a path");
	}

	void test_placement_in_markdown()
	{
		const auto markdown = std::string{ "Prose src/engine/renderer.cpp:12, **renderer.cpp:13** and _renderer.cpp:14_.\n"
										   "Whole span `renderer.cpp:15` and mixed `call(renderer.cpp:16)`.\n"
										   "```\nrenderer.cpp:17\n```\n"
										   "Already [a link](renderer.cpp:18) and <renderer.cpp:19>." };
		const auto links    = core::agent_markdown_reference_links(markdown, files, { });
		auto       lines    = std::vector<std::uint32_t>{ };
		for (const auto& link : links) {
			lines.push_back(link.First);
			expect(link.Paths == std::vector<std::string>{ "src/engine/renderer.cpp" }, "each opens the changed file");
		}
		expect(lines == std::vector<std::uint32_t>{ 12U, 13U, 14U, 15U }, "prose, emphasis and a whole code span link; a mixed span, fenced code and existing links do not");
		const auto span = std::string_view{ markdown }.substr(links[3].Begin, links[3].Length);
		expect(span == "`renderer.cpp:15`", "a whole code span's link covers the span");
		const auto emphasis = std::string_view{ markdown }.substr(links[2].Begin, links[2].Length);
		expect(emphasis == "renderer.cpp:14", "underscore emphasis is not part of the path");
	}

	void test_resolution()
	{
		expect(core::resolve_agent_reference("src/engine/renderer.cpp", files) == std::vector<std::string>{ "src/engine/renderer.cpp" }, "an exact changed path");
		expect(core::resolve_agent_reference("engine/renderer.cpp", files) == std::vector<std::string>{ "src/engine/renderer.cpp" }, "a unique suffix");
		expect(core::resolve_agent_reference("util.hpp", files) == std::vector<std::string>{ "src/a/util.hpp", "src/b/util.hpp" }, "a shared name offers both");
		expect(core::resolve_agent_reference("gine/renderer.cpp", files).empty(), "a suffix is whole path segments");
		expect(core::resolve_agent_reference("missing.cpp", files).empty(), "a path that matches nothing");
		auto head = files;
		head.Head = { "src/engine/unchanged.cpp" };
		expect(core::resolve_agent_reference("unchanged.cpp", head).empty(), "an unchanged file that cannot be shown is not a link");
		head.UnchangedOpenable = true;
		expect(core::resolve_agent_reference("unchanged.cpp", head) == std::vector<std::string>{ "src/engine/unchanged.cpp" }, "one that can be shown opens");
		const auto links = core::agent_markdown_reference_links("util.hpp:3, missing.cpp:4 and line 9", files, "src/engine/frame_pool.cpp");
		expect(links.size() == 2U && links[0].Paths.size() == 2U && links[1].Paths == std::vector<std::string>{ "src/engine/frame_pool.cpp" } && links[1].First == 9U,
			   "a shared name is a choice, nothing is plain, a bare line opens the context file");
	}
} // namespace

auto main() -> int
{
	test_every_listed_form_is_recognised();
	test_placement_in_markdown();
	test_resolution();
	std::cout << "agent-reference-tests:pass\n";
	return EXIT_SUCCESS;
}
