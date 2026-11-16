/// \file
/// \brief RFC-BPR-0120 ([[BPR-2189]]). A check on a local draft without any
/// file or agent: the plan an answer holds (words or none), Run's reasons,
/// the report a run ends in (each outcome, commands out of the plan, files
/// changed in the copy, an older candidate), the report as its tab reads it,
/// the requests, and the stored form with the draft's grants.

#include "bpr/core/local_draft.hpp"
#include "bpr/core/local_draft_check.hpp"
#include "bpr/core/local_draft_tour.hpp"
#include "bpr/core/local_tour_view.hpp"

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

	auto holds(const std::string_view aText, const std::string_view aPart) -> bool { return aText.find(aPart) != std::string_view::npos; }

	constexpr auto plan_answer = std::string_view{ "I'll build and run the parser tests.\n\n```bpr-check-plan\n"
												   R"({"commands":[{"command":"cmake --build build --target parser_tests","shows":"it still builds"},)"
												   R"({"command":"ctest --test-dir build -R parser","shows":"the parser tests pass"}],"needs":["builds"],"not_covered":"The GUI."})"
												   "\n```\n" };

	void plans_are_read()
	{
		const auto read = core::read_check_plan(plan_answer, 3U);
		expect(read.Plan.has_value(), "a plan block is read");
		expect(read.Plan->Steps.size() == 2U && read.Plan->Steps[1].Command == "ctest --test-dir build -R parser", "its commands, in order");
		expect(read.Plan->Steps[0].Shows == "it still builds", "what each shows");
		expect(read.Plan->NeedsBuilds && !read.Plan->NeedsScratch, "the grants it needs");
		expect(read.Plan->NotCovered == "The GUI." && read.Plan->Candidate == 3U && read.Plan->State == core::local_check_state::planned, "what it won't cover, its candidate, planned");
		expect(read.Prose == "I'll build and run the parser tests.", "the block leaves the prose");

		expect(!core::read_check_plan("No block here.", 3U).Plan, "no block plans nothing");
		expect(!core::read_check_plan("```bpr-check-plan\n{\"commands\":[]}\n```", 3U).Plan, "a plan with no command plans nothing");
		const auto malformed = core::read_check_plan("text\n```bpr-check-plan\nnot json\n```", 3U);
		expect(!malformed.Plan && holds(malformed.Prose, "not json"), "a malformed block stays in the prose");
	}

	void run_is_decided()
	{
		auto plan  = *core::read_check_plan(plan_answer, 3U).Plan;
		auto facts = core::local_check_run_facts{ .Grants = { .Agent = "a", .Builds = true } };
		expect(core::decide_check_run(plan, facts) == core::local_check_block::none, "with its grants, Run is available");
		facts.Grants.Builds = false;
		expect(core::decide_check_run(plan, facts) == core::local_check_block::builds_off, "builds off");
		expect(holds(core::check_block_words(core::local_check_block::builds_off), "Allow build and run tests"), "the reason names the toggle");
		plan.NeedsScratch = true;
		expect(core::decide_check_run(plan, facts) == core::local_check_block::scratch_off, "scratch off first");
		expect(holds(core::check_block_words(core::local_check_block::scratch_off), "Use scratch folder"), "it names the scratch toggle");
		facts.Running = true;
		expect(core::decide_check_run(plan, facts) == core::local_check_block::running, "another check running");
		expect(holds(core::check_block_words(core::local_check_block::running), "Another check is running"), "said so");
		facts.Candidate = false;
		expect(core::decide_check_run(plan, facts) == core::local_check_block::no_candidate, "no candidate");
		plan.State = core::local_check_state::discarded;
		expect(core::decide_check_run(plan, facts) == core::local_check_block::discarded, "a discarded plan does not run");
		plan.State = core::local_check_state::ran;
		expect(core::decide_check_run(plan, facts) == core::local_check_block::ran, "a plan that ran does not run twice");
	}

	void reports_are_read()
	{
		const auto  plan   = *core::read_check_plan(plan_answer, 3U).Plan;
		const auto  failed = core::read_check_report("The parser test failed.\n```bpr-check-report\n"
													 R"({"outcome":"failed","summary":"One parser test failed.","commands":[{"command":"cmake --build build --target parser_tests","exit":0},)"
													 R"({"command":"ctest --test-dir build -R parser","exit":8}],"output":["parser_tests: expected 3, got 2"],)"
													 R"("references":[{"path":"src/parser.cpp","line":42}],"not_covered":"The GUI and the other tests."})"
													 "\n```",
													 plan, 3U, { .Commands = { "ctest --test-dir build -R parser", "ls build" }, .Changed = { "src/generated.hpp" } });
		const auto& report = failed.Report;
		expect(report.Outcome == core::local_check_outcome::failed && report.Candidate == 3U, "the agent's word and the candidate");
		expect(report.Commands.size() == 3U && report.Commands[1].Exit == 8 && !report.Commands[1].OutOfPlan, "reported commands with their exit status");
		expect(report.Commands[2].Command == "ls build" && report.Commands[2].OutOfPlan && !report.Commands[2].Exit, "an observed command not in the plan is marked");
		expect(report.Output.size() == 1U && report.References[0].Path == "src/parser.cpp" && report.References[0].Line == 42U, "failure lines and references");
		expect(report.Changed == std::vector<std::string>{ "src/generated.hpp" }, "tracked files changed in the copy");
		expect(report.Plan.Steps == plan.Steps && report.Plan.State == core::local_check_state::ran, "the plan it ran, for Run again");

		const auto stopped = core::read_check_report("partial", plan, 3U, { .Commands = { "cmake --build build --target parser_tests" }, .Stopped = true });
		expect(stopped.Report.Outcome == core::local_check_outcome::stopped && stopped.Report.Commands.size() == 1U, "Stop makes it Stopped and keeps what ran");
		expect(core::read_check_report("I could not run anything.", plan, 3U, { }).Report.Outcome == core::local_check_outcome::didnt_run, "nothing ran: Didn't run");
		expect(core::read_check_report("Ran it.", plan, 3U, { .Commands = { "make" } }).Report.Outcome == core::local_check_outcome::couldnt_tell, "no block: Couldn't tell");
		expect(core::read_check_report("```bpr-check-report\n{\"outcome\":\"passed\"}\n```", plan, 3U, { }).Report.Outcome == core::local_check_outcome::passed, "Passed as stated");
		expect(core::read_check_report("x", plan, 3U, { }).Report.NotCovered == "The GUI.", "the plan's gaps when the report names none");

		const auto shown = core::render_check_report(report, 4U);
		expect(shown.starts_with("# Failed\n"), "the outcome word leads");
		expect(holds(shown, "Checked candidate 3; the draft is now at candidate 4"), "an older candidate says so");
		expect(holds(shown, "the agent's report of its run"), "stated as the agent's report");
		expect(holds(shown, "`ctest --test-dir build -R parser` exited 8"), "each command and its exit status");
		expect(holds(shown, "`ls build` (not in the plan)"), "out-of-plan commands marked");
		expect(holds(shown, "parser_tests: expected 3, got 2"), "the failure's output");
		expect(holds(shown, "(bpr-check:src/parser.cpp:42)"), "a code reference is a link");
		expect(holds(shown, "not tested as written") && holds(shown, "`src/generated.hpp`"), "changed tracked files listed");
		expect(holds(shown, "## Not covered\n\nThe GUI and the other tests."), "what was not covered");
		expect(!holds(core::render_check_report(report, 3U), "now at candidate"), "the latest candidate says nothing more");
		expect(core::offers_run_again(report, 4U) && !core::offers_run_again(report, 3U), "Run again only on a newer candidate");

		const auto link = core::read_check_reference_link(core::check_reference_link({ .Path = "src/a b.cpp", .Line = 7U }));
		expect(link && link->Path == "src/a b.cpp" && link->Line == 7U, "a reference link reads back");
		expect(!core::read_check_reference_link("https://example.com"), "other links are not references");
	}

	void requests_are_composed()
	{
		const auto paths  = std::vector<std::string>{ "src/parser.cpp" };
		const auto chosen = core::compose_check_plan_request("Parser fix", 3U, paths, "  ", "Plan carefully.", "local-draft-run-check-v1");
		expect(holds(chosen.Text, "choose the checks that matter most"), "no words: the agent chooses");
		expect(holds(chosen.Text, "candidate 3") && holds(chosen.Text, "- src/parser.cpp"), "the candidate and its paths");
		expect(holds(chosen.Instructions, "Plan carefully.") && holds(chosen.Instructions, "bpr-check-plan") && holds(chosen.Instructions, "local-draft.run-check"),
			   "the prompt's text, the format and its purpose");
		expect(holds(core::compose_check_plan_request("t", 3U, paths, "run the parser tests", "", "v").Text, "The author asks: run the parser tests"), "the author's words");
		const auto run = core::compose_check_run_request("Parser fix", 3U, paths, *core::read_check_plan(plan_answer, 3U).Plan, "Run carefully.", "v");
		expect(holds(run.Text, "1. `cmake --build build --target parser_tests`") && holds(run.Instructions, "bpr-check-report"), "the run names the plan and the report format");
		expect(core::check_plan_question("") == "Plan a check (you choose what to check).", "the author's turn without words");
		expect(core::check_run_question(4U, true) == "Run the same plan again on candidate 4.", "Run again's turn");
	}

	auto draft_with_candidate() -> core::local_draft
	{
		auto draft    = core::local_draft{ };
		draft.Id      = core::local_draft_id{ "0123456789abcdef0123456789abcdef" };
		draft.Title   = "Parser fix";
		draft.Version = 1U;
		draft.Candidates.push_back({ .Ordinal = 1U });
		return draft;
	}

	void checks_are_kept()
	{
		auto       draft  = draft_with_candidate();
		const auto plan   = core::read_check_plan(plan_answer, 1U);
		auto       opened = core::append_turn(draft, draft.Version, core::local_change_thread, "agent", "Agent", { .Text = "Plan a check (you choose what to check)." }, 10);
		expect(opened.has_value(), "the conversation opens");
		draft                   = opened->first;
		draft                   = *core::append_turn(draft, draft.Version, core::local_change_thread, "agent", "Agent", { .Author = false, .Text = plan.Prose, .Check = core::local_check{ .Plan = plan.Plan } }, 11)
									   .transform([](auto aPair) { return aPair.first; });
		const auto conversation = opened->second;
		expect(!core::settle_check_plan(draft, draft.Version, conversation, 0U, core::local_check_state::ran, 12).has_value(), "the question is no plan");
		draft = *core::settle_check_plan(draft, draft.Version, conversation, 1U, core::local_check_state::ran, 12);
		expect(!core::settle_check_plan(draft, draft.Version, conversation, 1U, core::local_check_state::discarded, 12).has_value(), "a plan decided once stays decided");

		// A newer candidate arrives while the check runs: the report stays with
		// its conversation, on the candidate it checked.
		draft.Candidates.push_back({ .Ordinal = 2U });
		auto report = core::read_check_report("```bpr-check-report\n{\"outcome\":\"passed\"}\n```", *plan.Plan, 1U, { .Commands = { "make" } });
		draft       = *core::append_check_turns(draft, draft.Version, conversation, { .Text = core::check_run_question(1U, false) },
												{ .Text = report.Prose, .Check = core::local_check{ .Report = report.Report } }, 13);
		draft       = *core::remember_check_grants(draft, draft.Version, { .Agent = "agent", .Scratch = true, .Builds = true, .Asked = true }, 14);
		expect(core::grants_for(draft.Preparation.CheckGrants, "agent").Builds && !core::grants_for(draft.Preparation.CheckGrants, "other").Builds, "grants per agent, off for another");

		const auto stored = core::decode_draft_preparation(core::encode_draft_preparation(draft.Preparation));
		expect(stored.has_value() && *stored == draft.Preparation, "plans, reports and grants are stored and read back exactly");
		expect(core::decode_draft_preparation(core::encode_draft_preparation(core::local_draft_preparation{ })).has_value(), "a draft with no check reads as before");

		const auto view = core::view_local_tour(draft);
		expect(view.Session.Runs.size() == 2U, "the plan and the run are turns of the conversation");
		expect(view.Session.Runs[0].Check && view.Session.Runs[0].Check->Plan && view.Session.Runs[0].Check->Plan->State == core::local_check_state::ran, "the plan card");
		expect(view.Session.Runs[1].Check && view.Session.Runs[1].Check->Report && view.Session.Runs[1].CheckLatest == 2U, "the report knows the latest candidate");
	}
}

auto main() -> int
{
	plans_are_read();
	run_is_decided();
	reports_are_read();
	requests_are_composed();
	checks_are_kept();
	std::cout << "local draft check core tests passed\n";
	return EXIT_SUCCESS;
}
