/// \file
/// \brief RFC-BPR-0110 ([[BPR-2026]], [[BPR-2027]]). A local draft's
/// description and tour without any file: authoring with no account or pull
/// request, stops bound to a candidate's exact lines and their
/// correspondence on a successor (exact, changed, ambiguous, missing,
/// unavailable, never a nearby line), acceptance kept only where exact, a
/// cited result that needs recheck, proposals that go stale rather than
/// overwrite, lines outside the candidate, claims and checks that enter the
/// evidence record only through acceptance and with their agent origin,
/// conversations per candidate with replay bounds, the exact public preview,
/// the simulated publication mapping and the stored form.

#include "bpr/core/local_draft.hpp"
#include "bpr/core/local_draft_evidence.hpp"
#include "bpr/core/local_draft_tour.hpp"

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

	template <typename T>
	auto ok(T aResult, const std::string_view aMessage)
	{
		expect(aResult.has_value(), aMessage);
		return std::move(*aResult);
	}

	auto commit(const char aDigit) -> core::git_commit_oid { return *core::parse_git_commit_oid(std::string(40U, aDigit)); }
	auto entry_oid(const char aDigit) -> core::git_entry_oid { return *core::parse_git_entry_oid(std::string(40U, aDigit)); }

	auto candidate(const char aHead, const char aRight, const bool aWorking) -> core::local_draft_candidate
	{
		return { .Head      = commit(aHead),
				 .TargetTip = commit('b'),
				 .Base      = commit('c'),
				 .Included  = aWorking ? std::vector<std::string>{ "src/search.cpp" } : std::vector<std::string>{ },
				 .Changes   = { { .Path   = "src/search.cpp",
								  .Change = core::draft_change::modified,
								  .Left   = core::draft_side{ .Mode = 0100644, .Object = entry_oid('2') },
								  .Right  = core::draft_side{ .Mode   = 0100644,
															  .Object = entry_oid(aRight),
															  .Origin = aWorking ? core::draft_material_origin::working : core::draft_material_origin::committed } } } };
	}

	auto make_draft() -> core::local_draft
	{
		return ok(core::create_local_draft({ }, { std::string(32U, '1') },
										   { .Location = "/work/search", .Repository = { .Volume = 1, .ObjectLow = 10 }, .Worktree = { .Volume = 1, .ObjectLow = 11 } }, "feature/search",
										   { .Target = { .Remote = "origin", .Repository = { .Host = "github.com", .Owner = "acme", .Name = "engine" }, .Branch = "main" } }, { },
										   candidate('a', '3', true), 100),
				  "a draft is created");
	}

	constexpr auto first_text = std::string_view{ "int a;\nvoid cancel() {\n  stop();\n}\nint b;\n" };

	auto file(const std::string_view aText) -> core::local_file_reading { return { .Path = "src/search.cpp", .Text = std::string{ aText } }; }

	void test_signed_out_authoring()
	{
		auto draft = make_draft();
		draft      = ok(core::edit_description(draft, draft.Version, "Cancel searches promptly", "## Why\n\nSearches kept running.\n", 101), "the description saves");
		expect(draft.Title == "Cancel searches promptly" && draft.Preparation.Description.Revision == 2U, "title and body are one explicit edit");
		expect(core::edit_description(draft, draft.Version, "t", std::string(core::maximum_local_description_bytes + 1U, 'x'), 102).error() == core::preparation_refusal::too_long,
			   "a body past GitHub's bound is refused, never cut");
		draft = ok(core::edit_tour_brief(draft, draft.Version, "Searches outlived cancel.", "Cancel stops the worker.", 103), "the brief saves");
		expect(draft.Preparation.Description.Body == "## Why\n\nSearches kept running.\n", "the tour's brief never writes the description");
		draft = ok(core::add_local_stop(draft, draft.Version, "Cancel stops", "Cancel now stops the worker.", file(first_text), core::diff_side::right, 2U, 4U, 104), "a stop on the candidate");
		const auto& stop = draft.Preparation.Tour.Stops.front();
		expect(stop.Source.Candidate == 1U && stop.Source.Excerpt == "void cancel() {\n  stop();\n}\n", "the stop binds the candidate's exact lines");
		expect(core::decide_local_stop(draft, stop).State == core::tour_acceptance_state::not_accepted, "a new stop is not accepted");
		expect(core::add_local_stop(draft, draft.Version, "x", "y", file(first_text), core::diff_side::right, 5U, 9U, 105).error() == core::preparation_refusal::outside_candidate,
			   "lines past the file are refused");
		draft = ok(core::accept_local_stop(draft, draft.Version, stop.Id, 106), "Accept");
		expect(core::decide_local_tour_head(draft) == core::local_tour_head{ .Stops = 1U, .ToAccept = 0U }, "the head counts stops and those to accept");
		expect(!draft.Association, "nothing names a pull request");
		const auto text = core::encode_local_draft(draft);
		expect(text.find("account") == std::string::npos, "the stored form names no account");
		const auto back = core::decode_local_draft(text);
		expect(back && *back == draft, "the preparation survives a restart through the stored form");
		draft.Preparation.Description.Body = std::string(core::maximum_local_description_bytes + 1U, 'x');
		expect(!core::decode_local_draft(core::encode_local_draft(draft)), "an out-of-bounds stored form is refused");
	}

	void test_correspondence()
	{
		const auto binding = core::local_source_binding{ .Candidate = 1U, .Path = "src/search.cpp", .FirstLine = 2U, .LastLine = 4U, .Excerpt = "void cancel() {\n  stop();\n}\n" };
		expect(core::correspond(binding, 2U, file(first_text)).Correspondence == core::local_correspondence::exact, "the same lines in place are exact");
		const auto moved = core::correspond(binding, 2U, file("// header\nint a;\nvoid cancel() {\n  stop();\n}\n"));
		expect(moved.Correspondence == core::local_correspondence::exact && moved.FirstLine == 3U && moved.LastLine == 5U, "the same lines at one other place are exact there");
		expect(core::correspond(binding, 2U, file("void cancel() {\n  stop();\n  join();\n}\n")).Correspondence == core::local_correspondence::changed,
			   "similar nearby lines are changed, not taken");
		expect(core::correspond(binding, 2U, file("x\ny\nvoid cancel() {\n  stop();\n}\nvoid cancel() {\n  stop();\n}\n")).Correspondence == core::local_correspondence::ambiguous,
			   "two places are ambiguous");
		expect(core::correspond(binding, 2U, { .Path = "src/search.cpp", .Deleted = true }).Correspondence == core::local_correspondence::missing, "a deleted file is missing");
		expect(core::correspond(binding, 2U, { .Path = "src/search.cpp" }).Correspondence == core::local_correspondence::unavailable, "unreadable bytes are unavailable");
	}

	auto with_successor(core::local_draft aDraft, const std::string_view aText) -> core::local_draft
	{
		aDraft                    = ok(core::add_candidate(aDraft, aDraft.Version, candidate('a', '4', true), 200), "a successor candidate");
		const auto readings       = std::vector{ file(aText) };
		return core::carry_local_tour(aDraft, readings);
	}

	void test_refresh_keeps_only_exact()
	{
		auto draft = make_draft();
		draft      = ok(core::add_local_stop(draft, draft.Version, "Cancel", "Stops.", file(first_text), core::diff_side::right, 2U, 4U, 101), "stop 1");
		draft      = ok(core::add_local_stop(draft, draft.Version, "Fields", "Two ints.", file(first_text), core::diff_side::right, 1U, 1U, 102), "stop 2");
		for (const auto& stop : std::vector{ draft.Preparation.Tour.Stops }) {
			draft = ok(core::accept_local_stop(draft, draft.Version, stop.Id, 103), "accept");
		}
		auto next = with_successor(draft, "int a2;\nvoid cancel() {\n  stop();\n}\nint b;\n");
		const auto& kept    = next.Preparation.Tour.Stops[0];
		const auto& changed = next.Preparation.Tour.Stops[1];
		expect(core::decide_local_stop(next, kept).State == core::tour_acceptance_state::accepted, "an exact stop stays Accepted");
		expect(core::decide_local_stop(next, changed) == core::local_stop_decision{ .State = core::tour_acceptance_state::changed, .Reason = core::local_stop_reason::changed },
			   "a changed stop is Changed since accepted, with that word");
		expect(changed.Source.Candidate == 1U && changed.Source.Excerpt == "int a;\n", "the original material is kept");
		expect(core::accept_local_stop(next, next.Version, changed.Id, 201).error() == core::preparation_refusal::lines_needed, "Accept cannot sign off lines that moved away");
		next = ok(core::save_local_stop(next, next.Version, changed.Id, "Fields", "Renamed.", file("int a2;\nvoid cancel() {\n  stop();\n}\nint b;\n"), core::diff_side::right, 1U, 1U, 202),
				  "Save with new lines");
		const auto& saved = next.Preparation.Tour.Stops[1];
		expect(core::decide_local_stop(next, saved).State == core::tour_acceptance_state::accepted && saved.Earlier.size() == 1U && saved.Earlier[0].Excerpt == "int a;\n",
			   "Save signs off the new lines and keeps the earlier binding");
		auto ambiguous = with_successor(draft, "void cancel() {\n  stop();\n}\nvoid cancel() {\n  stop();\n}\n");
		expect(core::decide_local_stop(ambiguous, ambiguous.Preparation.Tour.Stops[0]).Reason == core::local_stop_reason::ambiguous, "ambiguous is its own reason");
		ambiguous = ok(core::add_candidate(draft, draft.Version, candidate('a', '5', true), 300), "another successor");
		ambiguous = core::carry_local_tour(ambiguous, { });
		expect(core::decide_local_stop(ambiguous, ambiguous.Preparation.Tour.Stops[0]).Reason == core::local_stop_reason::unavailable, "a file not read is unavailable");
		auto edited = ok(core::save_local_stop(draft, draft.Version, draft.Preparation.Tour.Stops[0].Id, "Cancel", "Stops at once.", std::nullopt, core::diff_side::right, 0U, 0U, 400),
						 "Save of text");
		expect(core::decide_local_stop(edited, edited.Preparation.Tour.Stops[0]).State == core::tour_acceptance_state::accepted, "Save is the sign-off");
	}

	void test_evidence_recheck_and_selection()
	{
		auto draft       = make_draft();
		draft            = ok(core::add_claim(draft, draft.Version, core::claim_kind::bug_fix, "Cancel stops the worker.", 101), "a claim");
		const auto claim = draft.Evidence.Claims[0].Id;
		draft            = ok(core::add_check(draft, draft.Version, core::check_method::test, "cancel_test", "ctest -R cancel", { claim }, { }, 102), "a check");
		const auto check = draft.Evidence.Checks[0].Id;
		draft = ok(core::record_result(draft, draft.Version,
									   { .Check       = check,
										 .Outcome     = core::evidence_outcome::pass,
										 .Candidate   = 1U,
										 .Inputs      = { "src/search.cpp" },
										 .Limitations = "Linux only." },
									   103),
				   "a reported result");
		const auto result = draft.Evidence.Results[0].Id;
		draft = ok(core::add_question(draft, draft.Version, "Private: is the timer path the same?", claim, 104), "an unselected question");
		draft = ok(core::add_local_stop(draft, draft.Version, "Cancel", "Stops.", file(first_text), core::diff_side::right, 2U, 4U, 105), "a stop");
		const auto stop = draft.Preparation.Tour.Stops[0].Id;
		expect(core::cite_evidence(draft, draft.Version, stop, result, true, 106).error() == core::preparation_refusal::unknown, "an unselected result cannot be cited");
		draft = ok(core::select_evidence(draft, draft.Version, result, true, 107), "select the result");
		draft = ok(core::cite_evidence(draft, draft.Version, stop, result, true, 108), "cite it");
		draft = ok(core::accept_local_stop(draft, draft.Version, stop, 109), "accept");
		draft = ok(core::add_note(draft, draft.Version, "PRIVATE NOTE", "", core::note_origin::author, 110), "a private note");
		const auto preview = core::compose_local_publication(draft);
		expect(preview.Tour.find("cancel_test") != std::string::npos && preview.Tour.find("Linux only.") != std::string::npos && preview.Tour.find("reported by author") != std::string::npos,
			   "a selected result enters with its origin, scope and limitations");
		expect(preview.Tour.find("Private:") == std::string::npos && preview.Tour.find("PRIVATE NOTE") == std::string::npos && preview.Body.find("PRIVATE") == std::string::npos,
			   "unselected records and notes stay out of the public preview");
		expect(preview.PendingLinks == 1U && preview.Tour.find("link pending") != std::string::npos, "the link waits for the pull request");
		auto next = with_successor(draft, "int a;\nvoid cancel() {\n  stop();\n}\nint c;\n");
		expect(core::decide_local_stop(next, next.Preparation.Tour.Stops[0]) == core::local_stop_decision{ .State = core::tour_acceptance_state::changed, .Reason = core::local_stop_reason::evidence_recheck },
			   "a cited result that needs recheck makes the stop Changed since accepted");
	}

	void test_proposals()
	{
		auto draft = make_draft();
		draft      = ok(core::edit_description(draft, draft.Version, "Cancel", "Author body.", 101), "body");
		const auto drafts = std::vector<core::local_proposal_draft>{
			{ .Target = core::local_proposal_target::description, .Text = "Agent body." },
			{ .Target = core::local_proposal_target::new_stop, .Title = "Worker", .Text = "The worker stops.", .Path = "src/search.cpp", .FirstLine = 3U, .LastLine = 3U },
			{ .Target = core::local_proposal_target::new_stop, .Title = "Helper", .Text = "A live helper.", .Path = "src/helper.cpp", .FirstLine = 1U, .LastLine = 2U },
			{ .Target = core::local_proposal_target::check, .Title = "cancel_while_scanning", .Text = "ctest -R cancel_scan", .Method = core::check_method::test },
			{ .Target = core::local_proposal_target::claim, .Text = "No search outlives cancel.", .Claim = core::claim_kind::bug_fix },
			{ .Target = core::local_proposal_target::check, .Title = "fuzz_cancel", .Text = "", .Method = core::check_method::fuzz },
		};
		const auto readings = std::vector{ file(first_text) };
		auto [recorded, ids] = ok(core::record_proposals(draft, draft.Version, 1U, 1U, drafts, readings, "Claude", "local-discussion-v1", 102), "proposals recorded");
		draft                = std::move(recorded);
		expect(ids.size() == 6U, "every proposal is recorded");
		expect(draft.Preparation.Proposals[2].State == core::local_proposal_state::outside_candidate, "lines outside the candidate are recorded as such");
		expect(core::accept_proposal(draft, draft.Version, ids[2], 103).error() == core::preparation_refusal::outside_candidate, "and are never accepted as written");
		expect(draft.Evidence.Checks.empty() && draft.Evidence.Claims.empty(), "nothing reaches the evidence record before acceptance");
		draft = ok(core::accept_proposal(draft, draft.Version, ids[3], 104), "accept one check");
		draft = ok(core::reject_proposal(draft, draft.Version, ids[5], 105), "reject another");
		draft = ok(core::accept_proposal(draft, draft.Version, ids[4], 106), "accept a claim");
		expect(draft.Evidence.Checks.size() == 1U && draft.Evidence.Checks[0].Title == "cancel_while_scanning" && draft.Evidence.Checks[0].Origin.Source == core::evidence_source::agent
				   && draft.Evidence.Checks[0].Origin.Producer == "Claude",
			   "the accepted check is in the record with its agent origin");
		expect(draft.Evidence.Results.empty(), "and never as a result");
		expect(draft.Evidence.Claims.size() == 1U && draft.Evidence.Claims[0].Origin.Source == core::evidence_source::agent, "the accepted claim too");
		draft = ok(core::edit_description(draft, draft.Version, "Cancel", "Author body, edited later.", 107), "a later author edit");
		expect(core::proposal_stale(draft, draft.Preparation.Proposals[0]), "the description proposal is stale");
		expect(core::accept_proposal(draft, draft.Version, ids[0], 108).error() == core::preparation_refusal::proposal_stale, "a stale proposal cannot overwrite the later edit");
		expect(draft.Preparation.Description.Body == "Author body, edited later.", "the author's edit stands");
		auto later = ok(core::add_candidate(draft, draft.Version, candidate('a', '4', true), 109), "a successor");
		expect(core::accept_proposal(later, later.Version, ids[1], 110).error() == core::preparation_refusal::proposal_stale, "a proposal from an earlier candidate is stale");
		draft = ok(core::accept_proposal(draft, draft.Version, ids[1], 111), "the new stop on its own candidate");
		expect(core::decide_local_stop(draft, draft.Preparation.Tour.Stops.back()).State == core::tour_acceptance_state::accepted, "a stop's own proposal Accept signs it off");
		expect(core::valid(draft), "the draft stays valid");
	}

	void test_conversations()
	{
		auto draft     = make_draft();
		auto [d1, one] = ok(core::append_turn(draft, draft.Version, core::local_change_thread, "agent-1", "Claude", { .Author = true, .Text = "What is risky?" }, 101), "a question");
		auto [d2, two] = ok(core::append_turn(d1, d1.Version, core::local_change_thread, "agent-1", "Claude", { .Author = false, .Text = "The timer.", .CostMicros = 12000U }, 102), "an answer");
		expect(one == two && d2.Preparation.Conversations.size() == 1U, "one conversation per candidate, thread and agent");
		expect(core::conversation_cost(d2.Preparation.Conversations[0]) == 12000U, "the session's cost");
		const auto scope = core::local_session_scope(d2.Id.Value, 1U, core::local_change_thread, "agent-1");
		expect(scope.find("github") == std::string::npos && scope.find("candidate:1") != std::string::npos, "the scope names the draft and candidate, no account or host");
		auto refreshed = ok(core::add_candidate(d2, d2.Version, candidate('a', '4', true), 103), "an accepted refresh");
		expect(core::current_conversation(refreshed, core::local_change_thread, "agent-1") == nullptr, "a successor candidate has no open conversation");
		auto [d3, three] = ok(core::append_turn(refreshed, refreshed.Version, core::local_change_thread, "agent-1", "Claude", { .Text = "And now?" }, 104), "a new session");
		expect(three != one && d3.Preparation.Conversations.size() == 2U && d3.Preparation.Conversations[0].Turns.size() == 2U, "the earlier conversation stays readable");
		auto ended = ok(core::end_conversation(d3, d3.Version, three, 105), "New session");
		expect(core::current_conversation(ended, core::local_change_thread, "agent-1") == nullptr, "New session ends the open one");
		auto long_one = core::local_conversation{ .Id = 1U, .Thread = "change" };
		for (int index = 0; index < 20; ++index) {
			long_one.Turns.push_back({ .Text = std::string(6000U, 'x') });
		}
		const auto replay = core::replay_turns(long_one);
		expect(replay.Turns.size() == 8U && replay.Omitted == 12U, "replay keeps 48 KiB and at most 12 turns");
		expect(core::decode_local_draft(core::encode_local_draft(ended)) == ended, "conversations survive restart");
		expect(core::append_turn(d3, d3.Version, "stop:x", "agent-1", "Claude", { .Text = "?" }, 106).error() == core::preparation_refusal::invalid, "threads are well formed");
	}

	void test_publication_mapping()
	{
		auto draft = make_draft();
		draft      = ok(core::edit_description(draft, draft.Version, "Cancel", "<!-- template:v2 -->\n## Summary\nKeep this.\n- [x] checklist\n", 101), "an externally formatted body");
		draft      = ok(core::add_local_stop(draft, draft.Version, "Cancel", "Stops.", file(first_text), core::diff_side::right, 2U, 4U, 102), "mapped stop");
		draft      = ok(core::add_local_stop(draft, draft.Version, "Fields", "Ints.", file(first_text), core::diff_side::right, 5U, 5U, 103), "unmapped stop");
		const auto result = core::local_publication_result{ .Host   = "github.com",
															.Owner  = "acme",
															.Name   = "engine",
															.Number = 42U,
															.Head   = commit('e'),
															.Base   = commit('c'),
															.Body   = draft.Preparation.Description.Body,
															.Files  = { file("int a;\nvoid cancel() {\n  stop();\n}\nint z;\n") } };
		const auto mapping = core::map_local_publication(draft, result);
		expect(mapping.DescriptionPreserved, "the accepted full description is the pull request's body");
		expect(mapping.Stops.size() == 2U && mapping.Stops[0].Anchor && mapping.Stops[0].Anchor->FirstLine == 2U
				   && mapping.Stops[0].Permalink == "https://github.com/acme/engine/blob/eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee/src/search.cpp#L2-L4",
			   "a matching stop acquires the pull request's anchor");
		expect(!mapping.Stops[1].Anchor && mapping.Stops[1].Correspondence == core::local_correspondence::changed && mapping.Unresolved == 1U,
			   "a mismatched stop stays local, with its reason");
		expect(draft.Preparation.Tour.Stops.size() == 2U, "no stop is dropped");
		auto other = result;
		other.Body = "## Problem\n...";
		expect(!core::map_local_publication(draft, other).DescriptionPreserved, "a replaced body is reported");
		expect(!core::update_description_by_default(true) && core::update_description_by_default(false), "the description option starts off only for a local draft's pull request");
	}

	void test_transfer()
	{
		auto draft = make_draft();
		draft      = ok(core::edit_description(draft, draft.Version, "Cancel", "Body.", 101), "body");
		draft      = ok(core::edit_tour_brief(draft, draft.Version, "Problem text.", "", 102), "brief");
		draft      = ok(core::transfer_to_description(draft, draft.Version, "Problem text.", 103), "a deliberate transfer");
		expect(draft.Preparation.Description.Body == "Body.\n\nProblem text." && draft.Preparation.Tour.Problem == "Problem text.", "the transfer copies one way, on request");
	}
}

auto main() -> int
{
	test_signed_out_authoring();
	test_correspondence();
	test_refresh_keeps_only_exact();
	test_evidence_recheck_and_selection();
	test_proposals();
	test_conversations();
	test_publication_mapping();
	test_transfer();
	std::cout << "local draft tour tests passed\n";
	return EXIT_SUCCESS;
}
