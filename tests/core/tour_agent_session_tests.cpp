// [[BPR-1900]]. The tour session's decisions, without Qt: which requests read
// the repository, which tool uses a read-only run may make, the transcript,
// and the whole-tour proposal with its anchors.
#include "bpr/core/review_tour.hpp"
#include "bpr/core/tour_agent_session.hpp"

#include <algorithm>
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
		expect(transcript_kind_of("Bash", "awk 'NR<3' notes.txt") == tour_transcript_kind::read && transcript_kind_of("Bash", "rm -rf src") == tour_transcript_kind::ran && transcript_kind_of("Bash", "ls src") == tour_transcript_kind::searched, "[[BPR-1912]] a command outside the list is shown by what it does");
	}

	void transcript()
	{
		auto       session = tour_agent_session{ };
		const auto first   = begin_tour_run(session, tour_agent_request::draft_tour, tour_agent_tools::read_only, "Draft the tour");
		expect(first.has_value() && running_tour_run(session) == first, "a run starts and is running");
		expect(!begin_tour_run(session, tour_agent_request::outline, tour_agent_tools::read_only, "Suggest an outline"), "the session asks one thing at a time");
		expect(record_tour_activity(session, *first, { { tour_transcript_kind::searched, "rg -n draw_scene" }, { tour_transcript_kind::read, "src/renderer.cpp" } }), "activity is recorded on a running run");
		expect(record_tour_activity(session, *first, { { tour_transcript_kind::read, "awk 'NR<3' notes.txt", tour_transcript_note::ran_outside_list } }), "[[BPR-1912]] a command that ran outside the list is recorded");
		expect(find_tour_run(session, *first)->Entries.back() == tour_transcript_entry{ tour_transcript_kind::read, "awk 'NR<3' notes.txt", tour_transcript_note::ran_outside_list } && find_tour_run(session, *first)->OutsideList == 1U, "under its usual kind, with its note, and counted");
		expect(finish_tour_run(session, *first, tour_agent_run_state::answered, "A proposal of 3 stops"), "the run finishes");
		expect(!record_tour_activity(session, *first, { { tour_transcript_kind::read, "late" } }), "nothing is added to a finished run");
		expect(!finish_tour_run(session, *first, tour_agent_run_state::failed, "again"), "a run finishes once");
		const auto* run = find_tour_run(session, *first);
		expect(run != nullptr && run->Entries.size() == 5U && run->Entries.front().Kind == tour_transcript_kind::asked && run->Entries.back().Kind == tour_transcript_kind::answered, "asked, three tool lines, answered");
		const auto second = begin_tour_run(session, tour_agent_request::rewrite, tour_agent_tools::none, "Enhance problem statement");
		expect(second && second->value() == 2U, "the next run has the next identity");
		auto many = std::vector<tour_transcript_entry>(maximum_tour_transcript_entries + 5U, { tour_transcript_kind::read, std::string(5000U, 'x') });
		static_cast<void>(record_tour_activity(session, *second, many));
		run = find_tour_run(session, *second);
		expect(run->Entries.size() == maximum_tour_transcript_entries && run->Dropped == 6U, "the transcript is bounded and counts what it dropped");
		expect(run->Entries[1].Text.size() <= maximum_tour_transcript_bytes + 3U, "an entry is bounded");
		expect(run->OutsideList == 0U, "a run with no command outside the list counts none");
		// [[BPR-1912]]. A command Claude ran outside the read-only list keeps
		// its usual kind and its note, and is counted even past the bound.
		static_cast<void>(record_tour_activity(session, *second, { { tour_transcript_kind::searched, "awk 'NR<3' notes.txt", tour_transcript_note::ran_outside_list }, { tour_transcript_kind::searched, "python3 -c x", tour_transcript_note::not_run } }));
		expect(run->OutsideList == 1U && run->Dropped == 8U, "a command that ran outside the list is counted, a denied one is not");
		expect(finish_tour_run(session, *second, tour_agent_run_state::cancelled, "Cancelled"), "a cancelled run ends");
		for (int index = 0; index < 40; ++index) {
			const auto next = begin_tour_run(session, tour_agent_request::rewrite, tour_agent_tools::none, "again");
			static_cast<void>(finish_tour_run(session, *next, tour_agent_run_state::answered, "done"));
		}
		expect(session.Runs.size() == maximum_tour_agent_runs && find_tour_run(session, *first) == nullptr, "the oldest runs are forgotten");
	}

	void proposal()
	{
		const auto files  = std::vector<tour_anchor_file>{ { "src/renderer.cpp", true, 40U, std::nullopt }, { "src/old.cpp", false, std::nullopt, std::nullopt }, { "src/huge.bin", true, std::nullopt, std::nullopt } };
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
		expect(made.Run.value() == 7U && made.Items.size() == 3U && made.LeftOut.size() == 5U, "Problem, Solution and the one stop that anchors; five left out");
		expect(made.Items[0].Part == tour_proposal_part::problem && made.Items[0].ReplacesAuthorText, "the Problem says it replaces the author's text");
		expect(made.Items[1].Part == tour_proposal_part::solution && !made.Items[1].ReplacesAuthorText, "an empty Solution replaces nothing");
		expect(made.Items[2].Verdict == tour_anchor_verdict::anchored, "real lines of a pull request file anchor");
		// Owner look v002 ("What's up with this?"): a stop that does not anchor
		// is never an item; it is left out, with why, in the agent's order.
		expect(made.LeftOut[0].Verdict == tour_anchor_verdict::not_in_pull_request && made.LeftOut[0].Stop.Path == "src/missing.cpp", "a file outside the pull request is left out");
		expect(made.LeftOut[1].Verdict == tour_anchor_verdict::beyond_file, "lines past the end are left out");
		expect(made.LeftOut[2].Verdict == tour_anchor_verdict::not_on_head, "a deleted file is left out");
		expect(made.LeftOut[3].Verdict == tour_anchor_verdict::invalid_lines, "line zero is left out");
		expect(made.LeftOut[4].Verdict == tour_anchor_verdict::unmeasured, "an unmeasured file is left out");
		expect(std::ranges::all_of(made.Items, [](const tour_proposal_item& aItem) { return acceptable(aItem); }), "every part shown can be accepted");
		const auto kept = left_out_entries(made);
		expect(kept.size() == 5U && kept[0].Kind == tour_transcript_kind::left_out && kept[0].Text == "\"Not here\", src/missing.cpp, lines 1 to 2: not a file of this pull request", "the transcript keeps each left-out stop and why");
		auto       ended = tour_agent_session{ };
		const auto ran   = begin_tour_run(ended, tour_agent_request::draft_tour, tour_agent_tools::read_only, "Draft the tour");
		expect(!note_finished_tour_run(ended, *ran, kept), "notes wait for the run to end");
		static_cast<void>(finish_tour_run(ended, *ran, tour_agent_run_state::answered, "Proposed"));
		expect(note_finished_tour_run(ended, *ran, kept) && ended.Runs[0].Entries.size() == 7U, "an ended run keeps them");
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

	/// [[BPR-1900]], owner 2026-11-06: "It should strongly prefer on hunk.
	/// I'm okay with high value off-hunk that will work on live or i can
	/// relocate to on a hunk or in the PR".
	void placement()
	{
		const auto hunks = parse_patch_head_hunks("@@ -2,4 +2,5 @@ void draw()\n \n-old\n+new\n@@ -20,3 +21,6 @@\n+a\n@@ -40,2 +46,0 @@\n-gone");
		expect(hunks && *hunks == std::vector<tour_line_span>{ { 2U, 6U }, { 21U, 26U } }, "a patch's hunks are read as head lines, a deletion-only hunk skipped");
		expect(parse_patch_head_hunks("@@ -0,0 +1 @@\n+one") == std::optional{ std::vector<tour_line_span>{ { 1U, 1U } } }, "a count of one may be left out");
		expect(!parse_patch_head_hunks("@@ -1,2 +x,3 @@"), "a header it cannot read is refused");
		expect(parse_patch_head_hunks("") == std::optional{ std::vector<tour_line_span>{ } }, "no hunks is an empty list");

		const auto file = tour_anchor_file{ .Path = "src/a.cpp", .OnHead = true, .HeadLines = 60U, .Hunks = *hunks };
		expect(place_tour_stop({ "src/a.cpp", 3U, 5U, "", "" }, file) == tour_stop_placement::on_change, "(a) wholly inside a hunk is on the change");
		expect(place_tour_stop({ "src/a.cpp", 21U, 26U, "", "" }, file) == tour_stop_placement::on_change, "a whole hunk is on the change");
		expect(place_tour_stop({ "src/a.cpp", 12U, 12U, "", "" }, file) == tour_stop_placement::near_change, "(b) one line off the change still publishes");
		expect(place_tour_stop({ "src/a.cpp", 10U, 14U, "", "" }, file) == tour_stop_placement::off_change, "(c) several lines off the change do not publish as they are");
		expect(place_tour_stop({ "src/a.cpp", 5U, 8U, "", "" }, file) == tour_stop_placement::off_change, "a range across a hunk's edge does not publish as it is");
		expect(place_tour_stop({ "src/a.cpp", 10U, 14U, "", "" }, tour_anchor_file{ .Path = "src/a.cpp", .HeadLines = 60U }) == tour_stop_placement::on_change, "unknown hunks publish natively, as publication places them");

		const auto files  = std::vector<tour_anchor_file>{ file };
		const auto answer = tour_draft_answer{ .Stops = {
												   { "src/a.cpp", 10U, 14U, "Off", "Off the change." },
												   { "src/a.cpp", 12U, 12U, "Near", "One line near." },
												   { "src/a.cpp", 3U, 5U, "On", "On the change." },
												   { "src/a.cpp", 22U, 23U, "On too", "Also on it." },
												   { "src/elsewhere.cpp", 1U, 1U, "Not here", "(d)" },
											   } };
		auto       made   = propose_tour(tour_agent_run_id{ 3 }, answer, files, review_tour{ });
		expect(made.Items.size() == 4U && made.LeftOut.size() == 1U, "(d) a file outside the pull request is left out");
		expect(made.Items[0].Stop.Title == "On" && made.Items[1].Stop.Title == "On too" && made.Items[2].Stop.Title == "Near" && made.Items[3].Stop.Title == "Off", "on the change first, then near, then off, the agent's order within each");
		expect(made.Items[2].Placement == tour_stop_placement::near_change && made.Items[3].Placement == tour_stop_placement::off_change, "each says where it is");

		// Move to nearest change.
		expect(relocate_tour_stop(made, 3U, files), "an off-change stop moves");
		expect(made.Items[3].Stop.FirstLine == 2U && made.Items[3].Stop.LastLine == 6U && made.Items[3].Placement == tour_stop_placement::on_change, "onto the nearest hunk, keeping as many lines as fit");
		expect(place_tour_stop(made.Items[3].Stop, file) == tour_stop_placement::on_change, "and it is then on the change");
		auto far = propose_tour(tour_agent_run_id{ 4 }, tour_draft_answer{ .Stops = { { "src/a.cpp", 40U, 41U, "Far", "Below." } } }, files, review_tour{ });
		expect(relocate_tour_stop(far, 0U, files) && far.Items[0].Stop.FirstLine == 25U && far.Items[0].Stop.LastLine == 26U, "below every hunk it moves to the last one's end");
		auto near = propose_tour(tour_agent_run_id{ 5 }, tour_draft_answer{ .Stops = { { "src/a.cpp", 18U, 18U, "Near", "One." } } }, files, review_tour{ });
		expect(relocate_tour_stop(near, 0U, files) && near.Items[0].Stop.FirstLine == 21U && near.Items[0].Stop.LastLine == 21U, "a near stop moves to the closest hunk");
		expect(!relocate_tour_stop(near, 0U, std::vector<tour_anchor_file>{ tour_anchor_file{ .Path = "src/a.cpp", .HeadLines = 60U } }) && !relocate_tour_stop(near, 9U, files), "no hunks or no such part: nothing moves");
		static_cast<void>(decide_tour_proposal(near, 0U, tour_proposal_decision::discarded, ""));
		expect(!relocate_tour_stop(near, 0U, files), "a decided part does not move");
	}
}

namespace {
	/// [[BPR-1914]]. The tour conversation: an ask turn keeps its passage,
	/// question and reply; the next turn replays the earlier ones, bounded;
	/// a change comes back as a proposal, merged with what is still pending.
	void conversation()
	{
		expect(tools_for(tour_agent_request::ask) == tour_agent_tools::read_only, "an ask turn reads the repository");
		auto session = tour_agent_session{ };
		expect(!begin_tour_ask(session, tour_thread{ }, std::nullopt, "  \n"), "a blank question is not asked");
		expect(!begin_tour_ask(session, tour_thread{ }, tour_passage{ .Field = tour_passage_field::solution, .Text = " " }, "What?"), "a blank passage is not asked about");
		expect(!begin_tour_ask(session, tour_thread{ }, std::nullopt, std::string(maximum_tour_question_bytes + 1U, 'x')), "an oversized question is refused");
		const auto passage = tour_passage{ .Field = tour_passage_field::solution, .Text = "between begin and present" };
		const auto first   = begin_tour_ask(session, tour_thread{ }, passage, "What does this mean?");
		expect(first.has_value() && session.Runs.back().Request == tour_agent_request::ask && session.Runs.back().Passage == passage && session.Runs.back().Question == "What does this mean?", "an ask turn keeps its passage and question");
		expect(!begin_tour_ask(session, tour_thread{ }, std::nullopt, "And?"), "one turn at a time");
		expect(tour_conversation_of(session).Turns.empty(), "a running turn is not replayed");
		expect(answer_tour_ask(session, *first, "It is the frame window: `begin_frame()` to `present()`.", tour_proposal_tally{ }), "the reply ends the turn");
		expect(session.Runs.back().State == tour_agent_run_state::answered && session.Runs.back().Reply == "It is the frame window: `begin_frame()` to `present()`.", "the reply is kept as written");
		expect(!answer_tour_ask(session, *first, "again", { }), "an answered turn is not answered again");
		// A failed turn and a rewrite are not replayed; a draft is, by what it proposed.
		const auto failed = begin_tour_ask(session, tour_thread{ }, std::nullopt, "Broken?");
		static_cast<void>(finish_tour_run(session, *failed, tour_agent_run_state::failed, "It ended."));
		const auto rewrite = begin_tour_run(session, tour_agent_request::rewrite, tour_agent_tools::none, "Enhance solution");
		static_cast<void>(finish_tour_run(session, *rewrite, tour_agent_run_state::answered, "rewritten"));
		const auto draft = begin_tour_run(session, tour_agent_request::draft_tour, tour_agent_tools::read_only, "Draft the tour");
		static_cast<void>(finish_tour_run(session, *draft, tour_agent_run_state::answered, "Proposed a Solution and 2 stops."));
		const auto replayed = tour_conversation_of(session);
		expect(replayed.Turns.size() == 2U && replayed.Dropped == 0U, "two turns are replayed");
		expect(replayed.Turns[0] == tour_conversation_turn{ "What does this mean?", passage, "It is the frame window: `begin_frame()` to `present()`.", tour_thread{ } }, "the ask turn comes back whole, oldest first");
		expect(replayed.Turns[1].Asked == "Draft the tour" && replayed.Turns[1].Reply == "Proposed a Solution and 2 stops.", "a draft comes back by what it proposed");
		// Bounded: the newest turns, by count and by bytes.
		auto many = tour_agent_session{ };
		for (std::size_t index = 0U; index < maximum_replayed_tour_turns + 3U; ++index) {
			const auto run = begin_tour_ask(many, tour_thread{ }, std::nullopt, "Q" + std::to_string(index));
			static_cast<void>(answer_tour_ask(many, *run, "A" + std::to_string(index), { }));
		}
		const auto kept = tour_conversation_of(many);
		expect(kept.Turns.size() == maximum_replayed_tour_turns && kept.Dropped == 3U && kept.Turns.front().Asked == "Q3" && kept.Turns.back().Asked == "Q14", "the newest turns are kept, the oldest counted as left out");
		auto large = tour_agent_session{ };
		for (std::size_t index = 0U; index < 6U; ++index) {
			const auto run = begin_tour_ask(large, tour_thread{ }, std::nullopt, "Q" + std::to_string(index));
			static_cast<void>(answer_tour_ask(large, *run, std::string(maximum_tour_reply_bytes, 'r'), { }));
		}
		const auto bounded = tour_conversation_of(large);
		expect(bounded.Turns.size() == 2U && bounded.Dropped == 4U && bounded.Turns.back().Asked == "Q5", "replies past the byte bound are left out, oldest first");
		auto       longReply = tour_agent_session{ };
		const auto cut       = begin_tour_ask(longReply, tour_thread{ }, std::nullopt, "Long?");
		static_cast<void>(answer_tour_ask(longReply, *cut, std::string(maximum_tour_reply_bytes + 10U, 'x'), { }));
		expect(longReply.Runs.back().Reply.size() == maximum_tour_reply_bytes, "a reply is cut to its bound");

		// A change asked for in words: a Solution, a new stop, and a new
		// explanation for stop 1; a rewrite of a stop that does not exist is dropped.
		auto tour     = review_tour{ };
		tour.Solution = "The author's own solution.";
		auto stop     = tour_stop{ .Id = tour_stop_id{ 7 }, .Title = "Where it starts", .Body = "Old words." };
		tour.Stops.push_back(stop);
		const auto files   = std::vector<tour_anchor_file>{ { "src/a.cpp", true, 40U, std::nullopt } };
		const auto answer  = tour_ask_answer{ .Reply     = "Proposed.",
											  .Changes   = { .Problem = std::string{ }, .Solution = std::string{ "Each view clears its own target." }, .Stops = { { "src/a.cpp", 2U, 4U, "New", "A new stop." } } },
											  .StopTexts = { { 1U, "New words for stop one." }, { 5U, "No such stop." }, { 1U, "A second rewrite of it." } } };
		auto       changes = propose_tour_changes(tour_agent_run_id{ 3 }, answer, files, tour);
		expect(changes.Items.size() == 3U && changes.Items[0].Part == tour_proposal_part::solution && changes.Items[0].ReplacesAuthorText, "the Solution is proposed, and says it replaces the author's");
		expect(changes.Items[1].Part == tour_proposal_part::stop && changes.Items[2].Part == tour_proposal_part::stop_text && changes.Items[2].StopId == tour_stop_id{ 7 } && changes.Items[2].Text == "New words for stop one." && changes.Items[2].Stop.Title == "Where it starts" && changes.Items[2].ReplacesAuthorText, "a stop's new explanation names that stop; a missing stop and a second rewrite are dropped");
		expect(tally_of(changes) == tour_proposal_tally{ .Problem = false, .Solution = true, .Stops = 1U, .StopTexts = 1U }, "the tally counts what it proposes");
		expect(acceptable(changes.Items[2]) && decide_tour_proposal(changes, 2U, tour_proposal_decision::accepted, "Edited words.") && changes.Items[2].Text == "Edited words.", "a stop's new explanation is accepted with the author's edit");
		// Merged: a new Solution supersedes the pending one; the rest stays.
		auto pending  = propose_tour_changes(tour_agent_run_id{ 3 }, answer, files, tour);
		auto incoming = propose_tour_changes(tour_agent_run_id{ 4 }, tour_ask_answer{ .Reply = "Again.", .Changes = { .Solution = std::string{ "A better solution." } } }, files, tour);
		static_cast<void>(decide_tour_proposal(pending, 1U, tour_proposal_decision::discarded, ""));
		const auto merged = merge_tour_proposal(pending, incoming);
		expect(merged && merged->Run == tour_agent_run_id{ 4 } && merged->Items.size() == 2U && merged->Items[0].Text == "A better solution." && merged->Items[1].Part == tour_proposal_part::stop_text, "the new Solution leads; the pending stop text is kept; the old Solution and decided parts go");
		expect(merge_tour_proposal(pending, tour_proposal{ .Run = tour_agent_run_id{ 5 } }) == std::optional{ pending }, "a turn that proposes nothing leaves the proposal as it is");
		expect(merge_tour_proposal(std::nullopt, incoming) == std::optional{ incoming }, "with nothing pending it is the proposal");

		// [[BPR-1914]] v002. Threads: each turn belongs to the part it was
		// asked from; a turn's proposed parts know their run.
		auto       threads    = tour_agent_session{ };
		const auto stopThread = tour_thread{ .Kind = tour_thread_kind::stop, .Stop = tour_stop_id{ 7 } };
		const auto code       = tour_passage{ .Field = tour_passage_field::stop_code, .Text = "clear();", .Path = "src/a.cpp", .FirstLine = 4U, .LastLine = 4U };
		const auto onStop     = begin_tour_ask(threads, stopThread, code, "Why clear here?");
		static_cast<void>(answer_tour_ask(threads, *onStop, "Because.", { }));
		const auto onSolution = begin_tour_ask(threads, tour_thread{ .Kind = tour_thread_kind::solution }, std::nullopt, "And this?");
		static_cast<void>(answer_tour_ask(threads, *onSolution, "That.", { }));
		expect(thread_runs(threads, stopThread).size() == 1U && thread_runs(threads, stopThread).front()->Passage == code, "a stop's thread holds its turns, with the code they asked about");
		expect(thread_runs(threads, tour_thread{ .Kind = tour_thread_kind::problem }).empty(), "another part's thread is empty");
		// [[BPR-1925]]. The Tour tab's badge: each part with a chat, once.
		expect(chat_threads(threads) == std::vector<tour_thread>{ stopThread, tour_thread{ .Kind = tour_thread_kind::solution } }, "the parts with chats, each once, in the order of their first turn");
		expect(tour_conversation_of(threads).Turns.front().Thread == stopThread, "the replay says which thread each turn was in");
		expect(!begin_tour_ask(threads, stopThread, tour_passage{ .Field = tour_passage_field::stop_code, .Text = "x", .FirstLine = 5U, .LastLine = 4U }, "Bad lines?"), "code lines out of order are refused");
		const auto asked = propose_tour_changes(*onStop, answer, files, tour);
		expect(std::ranges::all_of(asked.Items, [&](const tour_proposal_item& aItem) { return aItem.Run == *onStop && proposed_in_thread(threads, aItem); }), "an ask turn's parts are its thread's");
		const auto drafted = begin_tour_run(threads, tour_agent_request::draft_tour, tour_agent_tools::read_only, "Draft the tour");
		expect(!proposed_in_thread(threads, propose_tour(*drafted, answer.Changes, files, tour).Items.front()), "a draft's parts are not");

		// [[BPR-1914]] v003. One side chat is one thread: it replays only its
		// own turns; Update turns propose only their part; the host's timing.
		const auto own = tour_conversation_of(threads, stopThread);
		expect(own.Turns.size() == 1U && own.Turns.front().Asked == "Why clear here?", "a stop's chat replays its own turns only");
		expect(tour_conversation_of(threads, tour_thread{ .Kind = tour_thread_kind::solution }).Turns.front().Asked == "And this?", "the Solution's chat its own");
		expect(tour_conversation_of(threads, tour_thread{ .Kind = tour_thread_kind::problem }).Turns.empty(), "an empty chat replays nothing");
		static_cast<void>(finish_tour_run(threads, *drafted, tour_agent_run_state::answered, "Proposed 1 stop."));
		expect(tour_conversation_of(threads, tour_thread{ }).Turns.size() == 1U && tour_conversation_of(threads, tour_thread{ }).Turns.front().Asked == "Draft the tour", "the tour's chat has the whole-tour drafts");
		const auto update = begin_tour_ask(threads, stopThread, std::nullopt, "Update this stop from our chat", true);
		expect(update && threads.Runs.back().Update, "an Update turn says so");
		const auto everything = propose_tour_changes(*update, answer, files, tour);
		const auto partOnly   = keep_thread_part(everything, stopThread);
		expect(partOnly.Items.size() == 1U && partOnly.Items.front().Part == tour_proposal_part::stop_text && partOnly.Items.front().StopId == tour_stop_id{ 7 }, "an Update keeps only its stop's new explanation");
		expect(keep_thread_part(everything, tour_thread{ .Kind = tour_thread_kind::solution }).Items.front().Part == tour_proposal_part::solution, "a Solution chat's Update keeps the Solution");
		expect(keep_thread_part(everything, tour_thread{ }).Items.size() == everything.Items.size(), "the tour's chat keeps everything");
		expect(!time_tour_run(threads, *update, 12U), "a running turn has no time yet");
		static_cast<void>(answer_tour_ask(threads, *update, "Done.", { }));
		expect(time_tour_run(threads, *update, 12U) && threads.Runs.back().Seconds == 12U, "an ended turn keeps how long it worked");
	}
}

int main()
{
	conversation();
	placement();
	tools_split();
	read_only_tools();
	transcript();
	proposal();
	std::cout << "tour-agent-session:pass\n";
	return EXIT_SUCCESS;
}
