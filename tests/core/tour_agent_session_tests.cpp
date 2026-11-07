// [[BPR-1900]]. The tour session's decisions, without Qt: which requests read
// the repository, which tool uses a read-only run may make, the transcript,
// and the whole-tour proposal with its anchors.
#include "bpr/core/review_tour.hpp"
#include "bpr/core/tour_agent_session.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {
	using namespace bpr;
	using namespace bpr::core;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void tools_split()
	{
		expect(tools_for(tour_agent_request::draft_tour) == tour_agent_tools::read_only, "drafting the tour reads the repository");
		expect(tools_for(tour_agent_request::draft_field) == tour_agent_tools::read_only, "Draft from PR context reads the repository");
		expect(tools_for(tour_agent_request::outline) == tour_agent_tools::read_only, "Suggest an outline reads the repository");
		expect(tools_for(tour_agent_request::rewrite) == tour_agent_tools::none, "a small rewrite stays tool-free");
		expect(tour_request_of(writing_hook::problem, writing_scope::empty_field) == tour_agent_request::draft_field, "an empty Problem is Draft from PR context");
		expect(tour_request_of(writing_hook::problem, writing_scope::field) == tour_agent_request::rewrite, "Enhance is a rewrite");
		expect(tour_request_of(writing_hook::solution, writing_scope::selection) == tour_agent_request::rewrite, "a selection is a rewrite");
		expect(tour_request_of(writing_hook::comment, writing_scope::empty_field) == tour_agent_request::rewrite, "Generate comment is a rewrite");
		expect(tour_request_of(writing_hook::outline, writing_scope::field) == tour_agent_request::outline, "the outline is the outline");
	}

	void read_only_tools()
	{
		using refusal = tour_tool_refusal;
		expect(decide_read_only_tool("Read", "") == refusal::none, "Read is permitted");
		expect(decide_read_only_tool("Grep", "") == refusal::none, "Grep is permitted");
		expect(decide_read_only_tool("Glob", "") == refusal::none, "Glob is permitted");
		for (const auto* tool : { "Edit", "Write", "MultiEdit", "NotebookEdit", "WebFetch", "WebSearch", "Task", "mcp__github__create_issue" }) {
			expect(decide_read_only_tool(tool, "") == refusal::tool, "a tool that writes, reaches the network or is MCP is refused");
		}
		expect(decide_read_only_tool("Bash", "rg -n \"draw_scene(\" src") == refusal::none, "a quoted search is permitted");
		expect(decide_read_only_tool("Bash", "rg -n 'clear|present' src | head -20") == refusal::none, "a search through head is permitted");
		expect(decide_read_only_tool("Bash", "git log --oneline -5") == refusal::none, "git log is permitted");
		expect(decide_read_only_tool("Bash", "git blame -L 1,20 src/renderer.cpp") == refusal::none, "git blame is permitted");
		expect(decide_read_only_tool("Bash", "git diff HEAD~1 -- src") == refusal::none, "git diff is permitted");
		expect(decide_read_only_tool("Bash", "find . -name renderer.cpp") == refusal::none, "find is permitted");
		expect(decide_read_only_tool("Bash", "rm -rf src") == refusal::program, "rm is refused");
		expect(decide_read_only_tool("Bash", "echo x > file") == refusal::shell_syntax, "a redirection is refused");
		expect(decide_read_only_tool("Bash", "rg x; rm y") == refusal::program, "a sequence is read command by command, and rm is refused");
		// [[BPR-1900]], installed Claude Code 2.1.310's own chains.
		expect(decide_read_only_tool("Bash", "git show --stat HEAD && git show HEAD | head -400") == refusal::none, "a chain of read-only commands is permitted");
		expect(decide_read_only_tool("Bash", "git log --oneline | wc -l && git ls-files") == refusal::none, "a chain through wc is permitted");
		expect(decide_read_only_tool("Bash", "rg -n draw src 2>/dev/null || grep -rn draw src") == refusal::none, "silenced errors and a fallback are permitted");
		expect(decide_read_only_tool("Bash", "ls src/*.cpp; wc -l src/renderer.cpp;") == refusal::none, "a glob and a trailing semicolon are permitted");
		expect(decide_read_only_tool("Bash", "rg x 2>&1 | head") == refusal::none, "2>&1 is permitted");
		expect(decide_read_only_tool("Bash", "rg x > out.txt") == refusal::shell_syntax, "a redirection to a file is refused");
		expect(decide_read_only_tool("Bash", R"x(git ls-files; grep -rn "draw_scene\|uExposure" . --exclude-dir=.git; cat -n src/engine/renderer.cpp)x") == refusal::none, "a backslash in double quotes, as installed Claude writes a grep alternation, is permitted");
		expect(decide_read_only_tool("Bash", R"x(grep -rn draw\|clear src)x") == refusal::none, "an escaped character outside quotes is literal");
		expect(decide_read_only_tool("Bash", R"x(rg "\$(rm -rf src)")x") == refusal::none, "an escaped dollar is literal text");
		expect(decide_read_only_tool("Bash", R"x(rg "$(rm -rf src)")x") == refusal::shell_syntax, "an unescaped substitution in double quotes is refused");
		expect(decide_read_only_tool("Bash", R"x(rg x \> out)x") == refusal::none, "an escaped > is an argument, not a redirection");
		expect(decide_read_only_tool("Bash", "rg x 2>errors.txt") == refusal::shell_syntax, "an error redirection to a file is refused");
		expect(decide_read_only_tool("Bash", "rg x &") == refusal::shell_syntax, "a background job is refused");
		expect(decide_read_only_tool("Bash", "cd /w/workspace && git log --stat | head -50 && git status --short | head") == refusal::none, "cd into the workspace and git status, as Claude runs them, are permitted");
		expect(decide_read_only_tool("Bash", "sed -n 183,200p core/CMakeLists.txt; ls core | head") == refusal::none, "sed that only prints lines is permitted");
		expect(decide_read_only_tool("Bash", "sed -i s/a/b/ file") == refusal::argument, "sed -i is refused");
		expect(decide_read_only_tool("Bash", "sed -n 'w out' file") == refusal::argument, "a sed script that writes is refused");
		expect(decide_read_only_tool("Bash", "awk '{print}' file") == refusal::program, "awk, which can run commands, is refused");
		expect(decide_read_only_tool("Bash", "git branch topic") == refusal::program, "git branch is refused");
		expect(decide_read_only_tool("Bash", "rg x && git commit -m y") == refusal::program, "a chain that commits is refused");
		expect(decide_read_only_tool("Bash", "&& rg x") == refusal::shell_syntax, "an empty command is refused");
		expect(decide_read_only_tool("Bash", "cat $(which x)") == refusal::shell_syntax, "a substitution is refused");
		expect(decide_read_only_tool("Bash", "rg \"$HOME\"") == refusal::shell_syntax, "an expansion inside double quotes is refused");
		expect(decide_read_only_tool("Bash", "ls *") == refusal::none, "a glob only lists file names");
		expect(decide_read_only_tool("Bash", "rg x\nrm y") == refusal::shell_syntax, "a second line is refused");
		expect(decide_read_only_tool("Bash", "rg 'unterminated") == refusal::shell_syntax, "an open quote is refused");
		expect(decide_read_only_tool("Bash", "find . -delete") == refusal::argument, "find -delete is refused");
		expect(decide_read_only_tool("Bash", "find . -exec rm {} +") != refusal::none, "find -exec is refused");
		expect(decide_read_only_tool("Bash", "sort -o out.txt in.txt") == refusal::argument, "sort -o is refused");
		expect(decide_read_only_tool("Bash", "rg --pre=sh x") == refusal::argument, "rg --pre is refused");
		expect(decide_read_only_tool("Bash", "git commit -m x") == refusal::program, "git commit is refused");
		expect(decide_read_only_tool("Bash", "git push") == refusal::program, "git push is refused");
		expect(decide_read_only_tool("Bash", "git -c core.pager=sh log") == refusal::argument, "an option before the git command is refused");
		expect(decide_read_only_tool("Bash", "git diff --output=x") == refusal::argument, "git --output is refused");
		expect(decide_read_only_tool("Bash", "GIT_DIR=/x git log") == refusal::argument, "an environment assignment is refused");
		expect(decide_read_only_tool("Bash", "curl https://example.com") == refusal::program, "the network is refused");
		expect(transcript_kind_of("Grep", "") == tour_transcript_kind::searched && transcript_kind_of("Read", "") == tour_transcript_kind::read, "Grep searches and Read reads");
		expect(transcript_kind_of("Bash", "git log -3") == tour_transcript_kind::ran && transcript_kind_of("Bash", "rg x") == tour_transcript_kind::searched && transcript_kind_of("Bash", "head -5 a") == tour_transcript_kind::read, "shell commands are shown by what they do");
	}

	void transcript()
	{
		auto       session = tour_agent_session{ };
		const auto first   = begin_tour_run(session, tour_agent_request::draft_tour, tour_agent_tools::read_only, "Draft the tour");
		expect(first.has_value() && running_tour_run(session) == first, "a run starts and is running");
		expect(!begin_tour_run(session, tour_agent_request::outline, tour_agent_tools::read_only, "Suggest an outline"), "the session asks one thing at a time");
		expect(record_tour_activity(session, *first, { { tour_transcript_kind::searched, "rg -n draw_scene" }, { tour_transcript_kind::read, "src/renderer.cpp" } }), "activity is recorded on a running run");
		expect(finish_tour_run(session, *first, tour_agent_run_state::answered, "A proposal of 3 stops"), "the run finishes");
		expect(!record_tour_activity(session, *first, { { tour_transcript_kind::read, "late" } }), "nothing is added to a finished run");
		expect(!finish_tour_run(session, *first, tour_agent_run_state::failed, "again"), "a run finishes once");
		const auto* run = find_tour_run(session, *first);
		expect(run != nullptr && run->Entries.size() == 4U && run->Entries.front().Kind == tour_transcript_kind::asked && run->Entries.back().Kind == tour_transcript_kind::answered, "asked, two tool lines, answered");
		const auto second = begin_tour_run(session, tour_agent_request::rewrite, tour_agent_tools::none, "Enhance problem statement");
		expect(second && second->value() == 2U, "the next run has the next identity");
		auto many = std::vector<tour_transcript_entry>(maximum_tour_transcript_entries + 5U, { tour_transcript_kind::read, std::string(5000U, 'x') });
		static_cast<void>(record_tour_activity(session, *second, many));
		run = find_tour_run(session, *second);
		expect(run->Entries.size() == maximum_tour_transcript_entries && run->Dropped == 6U, "the transcript is bounded and counts what it dropped");
		expect(run->Entries[1].Text.size() <= maximum_tour_transcript_bytes + 3U, "an entry is bounded");
		expect(finish_tour_run(session, *second, tour_agent_run_state::cancelled, "Cancelled"), "a cancelled run ends");
		for (int index = 0; index < 40; ++index) {
			const auto next = begin_tour_run(session, tour_agent_request::rewrite, tour_agent_tools::none, "again");
			static_cast<void>(finish_tour_run(session, *next, tour_agent_run_state::answered, "done"));
		}
		expect(session.Runs.size() == maximum_tour_agent_runs && find_tour_run(session, *first) == nullptr, "the oldest runs are forgotten");
	}

	void proposal()
	{
		const auto files  = std::vector<tour_anchor_file>{ { "src/renderer.cpp", true, 40U }, { "src/old.cpp", false, std::nullopt }, { "src/huge.bin", true, std::nullopt } };
		auto       draft  = review_tour{ };
		draft.Problem     = "The author's own problem.";
		const auto answer = tour_draft_answer{
			.Problem  = std::string{ "Two views drew over each other." },
			.Solution = std::string{ "Clear each view before drawing it." },
			.Stops    = {
				{ "src/renderer.cpp", 2U, 5U, "Where drawing starts", "draw() is the one entry point." },
				{ "src/missing.cpp", 1U, 2U, "Not here", "Invented." },
				{ "src/renderer.cpp", 39U, 41U, "Past the end", "Runs off." },
				{ "src/old.cpp", 1U, 1U, "Deleted", "Gone." },
				{ "src/renderer.cpp", 0U, 3U, "Line zero", "Bad." },
				{ "src/huge.bin", 1U, 2U, "Unmeasured", "Unknown." },
			},
		};
		auto made = propose_tour(tour_agent_run_id{ 7 }, answer, files, draft);
		expect(made.Run.value() == 7U && made.Items.size() == 8U, "Problem, Solution and six stops");
		expect(made.Items[0].Part == tour_proposal_part::problem && made.Items[0].ReplacesAuthorText, "the Problem says it replaces the author's text");
		expect(made.Items[1].Part == tour_proposal_part::solution && !made.Items[1].ReplacesAuthorText, "an empty Solution replaces nothing");
		expect(made.Items[2].Verdict == tour_anchor_verdict::anchored, "real lines of a pull request file anchor");
		expect(made.Items[3].Verdict == tour_anchor_verdict::not_in_pull_request, "a file outside the pull request is flagged");
		expect(made.Items[4].Verdict == tour_anchor_verdict::beyond_file, "lines past the end are flagged");
		expect(made.Items[5].Verdict == tour_anchor_verdict::not_on_head, "a deleted file is flagged");
		expect(made.Items[6].Verdict == tour_anchor_verdict::invalid_lines, "line zero is flagged");
		expect(made.Items[7].Verdict == tour_anchor_verdict::unmeasured, "an unmeasured file is flagged");
		expect(!acceptable(made.Items[3]) && !decide_tour_proposal(made, 3U, tour_proposal_decision::accepted, "x"), "a stop that does not anchor cannot be accepted");
		expect(decide_tour_proposal(made, 3U, tour_proposal_decision::discarded, ""), "it can be discarded");
		expect(decide_tour_proposal(made, 0U, tour_proposal_decision::accepted, "Edited by the author."), "the Problem is accepted with the author's edit");
		expect(made.Items[0].Text == "Edited by the author." && !decide_tour_proposal(made, 0U, tour_proposal_decision::discarded, ""), "an item is decided once");
		expect(!decide_tour_proposal(made, 1U, tour_proposal_decision::accepted, "   "), "blank text is not accepted");
		expect(decide_tour_proposal(made, 2U, tour_proposal_decision::accepted, "draw() starts every frame."), "an anchored stop is accepted");
		const auto stop = tour_stop_of(made.Items[2].Stop, *parse_git_commit_oid("1111111111111111111111111111111111111111"));
		expect(stop.Destination && stop.Destination->FirstLine == 1U && stop.Destination->LastLine == 4U && stop.Destination->Side == diff_side::right && stop.Body == "draw() starts every frame." && stop.Title == "Where drawing starts", "the stop lands at its head lines, zero-based");
		expect(valid(*stop.Destination) && !stop.Original, "its destination is valid and it has no source yet");
		auto empty = propose_tour(tour_agent_run_id{ 8 }, tour_draft_answer{ .Problem = std::string{ "  " } }, files, draft);
		expect(empty.Items.empty(), "blank fields are left out");
	}
}

int main()
{
	tools_split();
	read_only_tools();
	transcript();
	proposal();
	std::cout << "tour-agent-session:pass\n";
	return EXIT_SUCCESS;
}
