/// \file
/// \brief RFC-BPR-0110 ([[BPR-2026]], [[BPR-2027]]). A local draft's
/// description and tour without any file: authoring with no account or pull
/// request, stops bound to a candidate's exact lines and their
/// correspondence on a successor (exact, changed, ambiguous, missing,
/// unavailable, never a nearby line), acceptance kept only where exact,
/// proposals that go stale rather than overwrite, lines outside the
/// candidate, conversations per candidate with replay bounds, the exact
/// public preview without private preparation, the simulated publication
/// mapping, the stored form, and a draft stored while it still held evidence.

#include "bpr/core/local_draft.hpp"
#include "bpr/core/local_draft_tour.hpp"
#include "bpr/core/local_tour_view.hpp"
#include "bpr/core/tour_stop_acceptance.hpp"

#include <algorithm>
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
		draft            = ok(core::add_local_stop(draft, draft.Version, "Cancel stops", "Cancel now stops the worker.", file(first_text), core::diff_side::right, 2U, 4U, 104), "a stop on the candidate");
		const auto& stop = draft.Preparation.Tour.Stops.front();
		expect(stop.Source.Candidate == 1U && stop.Source.Excerpt == "void cancel() {\n  stop();\n}\n", "the stop binds the candidate's exact lines");
		expect(core::decide_local_stop(stop).State == core::tour_acceptance_state::not_accepted, "a new stop is not accepted");
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
		aDraft              = ok(core::add_candidate(aDraft, aDraft.Version, candidate('a', '4', true), 200), "a successor candidate");
		const auto readings = std::vector{ file(aText) };
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
		auto        next    = with_successor(draft, "int a2;\nvoid cancel() {\n  stop();\n}\nint b;\n");
		const auto& kept    = next.Preparation.Tour.Stops[0];
		const auto& changed = next.Preparation.Tour.Stops[1];
		expect(core::decide_local_stop(kept).State == core::tour_acceptance_state::accepted, "an exact stop stays Accepted");
		expect(core::decide_local_stop(changed) == core::local_stop_decision{ .State = core::tour_acceptance_state::changed, .Reason = core::local_stop_reason::changed },
			   "a changed stop is Changed since accepted, with that word");
		expect(changed.Source.Candidate == 1U && changed.Source.Excerpt == "int a;\n", "the original material is kept");
		expect(core::accept_local_stop(next, next.Version, changed.Id, 201).error() == core::preparation_refusal::lines_needed, "Accept cannot sign off lines that moved away");
		next              = ok(core::save_local_stop(next, next.Version, changed.Id, "Fields", "Renamed.", file("int a2;\nvoid cancel() {\n  stop();\n}\nint b;\n"), core::diff_side::right, 1U, 1U, 202),
							   "Save with new lines");
		const auto& saved = next.Preparation.Tour.Stops[1];
		expect(core::decide_local_stop(saved).State == core::tour_acceptance_state::accepted && saved.Earlier.size() == 1U && saved.Earlier[0].Excerpt == "int a;\n",
			   "Save signs off the new lines and keeps the earlier binding");
		auto ambiguous = with_successor(draft, "void cancel() {\n  stop();\n}\nvoid cancel() {\n  stop();\n}\n");
		expect(core::decide_local_stop(ambiguous.Preparation.Tour.Stops[0]).Reason == core::local_stop_reason::ambiguous, "ambiguous is its own reason");
		ambiguous = ok(core::add_candidate(draft, draft.Version, candidate('a', '5', true), 300), "another successor");
		ambiguous = core::carry_local_tour(ambiguous, { });
		expect(core::decide_local_stop(ambiguous.Preparation.Tour.Stops[0]).Reason == core::local_stop_reason::unavailable, "a file not read is unavailable");
		auto edited = ok(core::save_local_stop(draft, draft.Version, draft.Preparation.Tour.Stops[0].Id, "Cancel", "Stops at once.", std::nullopt, core::diff_side::right, 0U, 0U, 400),
						 "Save of text");
		expect(core::decide_local_stop(edited.Preparation.Tour.Stops[0]).State == core::tour_acceptance_state::accepted, "Save is the sign-off");
	}

	void test_preview_keeps_preparation_private()
	{
		auto draft         = make_draft();
		draft              = ok(core::add_local_stop(draft, draft.Version, "Cancel", "Stops.", file(first_text), core::diff_side::right, 2U, 4U, 101), "a stop");
		draft              = ok(core::add_note(draft, draft.Version, "PRIVATE NOTE", "", core::note_origin::author, 102), "a private note");
		auto [talked, id]  = ok(core::append_turn(draft, draft.Version, core::local_change_thread, "agent-1", "Claude", { .Text = "PRIVATE QUESTION" }, 103), "a conversation");
		draft              = std::move(talked);
		const auto drafts  = std::vector<core::local_proposal_draft>{ { .Target = core::local_proposal_target::description, .Text = "PRIVATE PROPOSAL" } };
		draft              = ok(core::record_proposals(draft, draft.Version, id, 1U, drafts, { }, "Claude", "local-discussion-v1", 104), "a proposal").first;
		const auto preview = core::compose_local_publication(draft);
		expect(preview.Tour.find("Stops.") != std::string::npos, "the tour is in the preview");
		for (const auto* text : { "PRIVATE NOTE", "PRIVATE QUESTION", "PRIVATE PROPOSAL" }) {
			expect(preview.Title.find(text) == std::string::npos && preview.Body.find(text) == std::string::npos && preview.Tour.find(text) == std::string::npos,
				   "notes, conversations and proposals stay out of the public preview");
		}
		expect(preview.PendingLinks == 1U && preview.Tour.find("link pending") != std::string::npos, "the link waits for the pull request");
	}

	/// A draft stored before RFC-BPR-0109 was deferred held evidence, stops
	/// that cited it, claim and method on every proposal and proposals of
	/// claims and checks: it reads as the same draft without any of them.
	void test_stored_evidence_is_dropped()
	{
		auto draft           = make_draft();
		draft                = ok(core::add_local_stop(draft, draft.Version, "Cancel", "Stops.", file(first_text), core::diff_side::right, 2U, 4U, 101), "a stop");
		draft                = ok(core::accept_local_stop(draft, draft.Version, draft.Preparation.Tour.Stops[0].Id, 102), "accepted");
		const auto drafts    = std::vector<core::local_proposal_draft>{ { .Target = core::local_proposal_target::description, .Text = "Agent body." } };
		auto [proposed, ids] = ok(core::record_proposals(draft, draft.Version, 1U, 1U, drafts, { }, "Claude", "v1", 103), "a proposal");
		const auto talked    = ok(core::append_turn(proposed, proposed.Version, core::local_change_thread, "agent-1", "Claude", { .Author = false, .Text = "Here.", .Proposals = ids }, 104),
								  "an answer that proposed it")
								   .first;
		const auto replace   = [](std::string& aText, const std::string_view aFrom, const std::string_view aTo) {
			const auto at = aText.find(aFrom);
			expect(at != std::string::npos, "the stored form has the expected shape");
			aText.replace(at, aFrom.size(), aTo);
		};
		auto stored = core::encode_local_draft(talked);
		replace(stored, "\"preparation\":", "\"evidence\":{\"claims\":[]},\"preparation\":");
		replace(stored, "\"earlier\":[]", "\"earlier\":[],\"citations\":[7]");
		replace(stored, "\"producer\":\"Claude\"", "\"claim\":\"other\",\"method\":\"test\",\"producer\":\"Claude\"");
		replace(stored, "\"proposals\":[{",
				"\"proposals\":[{\"id\":9,\"target\":\"claim\",\"conversation\":1,\"candidate\":1,\"stop\":0,\"base\":0,\"title\":\"\",\"text\":\"No search outlives "
				"cancel.\",\"claim\":\"bug_fix\",\"method\":\"test\",\"producer\":\"Claude\",\"version\":\"v1\",\"state\":\"pending\"},{");
		replace(stored, "\"proposals\":[" + std::to_string(ids[0]) + "]", "\"proposals\":[" + std::to_string(ids[0]) + ",9]");
		const auto read = core::decode_local_draft(stored);
		expect(read && *read == talked, "the earlier draft reads as the same draft without its evidence");
		expect(core::decide_local_stop(read->Preparation.Tour.Stops[0]).State == core::tour_acceptance_state::accepted, "its accepted stop stays accepted");
		const auto again = core::encode_local_draft(*read);
		for (const auto* key : { "\"evidence\"", "\"citations\"", "\"claim\"", "\"method\"" }) {
			expect(again.find(key) == std::string::npos, "and is written without them");
		}
	}

	void test_proposals()
	{
		auto draft        = make_draft();
		draft             = ok(core::edit_description(draft, draft.Version, "Cancel", "Author body.", 101), "body");
		const auto drafts = std::vector<core::local_proposal_draft>{
			{ .Target = core::local_proposal_target::description, .Text = "Agent body." },
			{ .Target = core::local_proposal_target::new_stop, .Title = "Worker", .Text = "The worker stops.", .Path = "src/search.cpp", .FirstLine = 3U, .LastLine = 3U },
			{ .Target = core::local_proposal_target::new_stop, .Title = "Helper", .Text = "A live helper.", .Path = "src/helper.cpp", .FirstLine = 1U, .LastLine = 2U },
			{ .Target = core::local_proposal_target::problem, .Text = "Searches outlive cancel." },
		};
		const auto readings  = std::vector{ file(first_text) };
		auto [recorded, ids] = ok(core::record_proposals(draft, draft.Version, 1U, 1U, drafts, readings, "Claude", "local-discussion-v1", 102), "proposals recorded");
		draft                = std::move(recorded);
		expect(ids.size() == 4U, "every proposal is recorded");
		expect(draft.Preparation.Proposals[2].State == core::local_proposal_state::outside_candidate, "lines outside the candidate are recorded as such");
		expect(core::accept_proposal(draft, draft.Version, ids[2], 103).error() == core::preparation_refusal::outside_candidate, "and are never accepted as written");
		expect(draft.Preparation.Tour.Problem.empty(), "nothing changes before acceptance");
		draft = ok(core::reject_proposal(draft, draft.Version, ids[3], 105), "reject one");
		expect(draft.Preparation.Proposals[3].State == core::local_proposal_state::rejected && draft.Preparation.Tour.Problem.empty(), "a rejected proposal changes nothing");
		draft = ok(core::edit_description(draft, draft.Version, "Cancel", "Author body, edited later.", 107), "a later author edit");
		expect(core::proposal_stale(draft, draft.Preparation.Proposals[0]), "the description proposal is stale");
		expect(core::accept_proposal(draft, draft.Version, ids[0], 108).error() == core::preparation_refusal::proposal_stale, "a stale proposal cannot overwrite the later edit");
		expect(draft.Preparation.Description.Body == "Author body, edited later.", "the author's edit stands");
		auto later = ok(core::add_candidate(draft, draft.Version, candidate('a', '4', true), 109), "a successor");
		expect(core::accept_proposal(later, later.Version, ids[1], 110).error() == core::preparation_refusal::proposal_stale, "a proposal from an earlier candidate is stale");
		draft = ok(core::accept_proposal(draft, draft.Version, ids[1], 111), "the new stop on its own candidate");
		expect(core::decide_local_stop(draft.Preparation.Tour.Stops.back()).State == core::tour_acceptance_state::accepted, "a stop's own proposal Accept signs it off");
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
		auto longOne = core::local_conversation{ .Id = 1U, .Thread = "change" };
		for (int index = 0; index < 20; ++index) {
			longOne.Turns.push_back({ .Text = std::string(6000U, 'x') });
		}
		const auto replay = core::replay_turns(longOne);
		expect(replay.Turns.size() == 8U && replay.Omitted == 12U, "replay keeps 48 KiB and at most 12 turns");
		expect(core::decode_local_draft(core::encode_local_draft(ended)) == ended, "conversations survive restart");
		expect(core::append_turn(d3, d3.Version, "stop:x", "agent-1", "Claude", { .Text = "?" }, 106).error() == core::preparation_refusal::invalid, "threads are well formed");
	}

	void test_publication_mapping()
	{
		auto draft         = make_draft();
		draft              = ok(core::edit_description(draft, draft.Version, "Cancel", "<!-- template:v2 -->\n## Summary\nKeep this.\n- [x] checklist\n", 101), "an externally formatted body");
		draft              = ok(core::add_local_stop(draft, draft.Version, "Cancel", "Stops.", file(first_text), core::diff_side::right, 2U, 4U, 102), "mapped stop");
		draft              = ok(core::add_local_stop(draft, draft.Version, "Fields", "Ints.", file(first_text), core::diff_side::right, 5U, 5U, 103), "unmapped stop");
		const auto result  = core::local_publication_result{ .Host   = "github.com",
															 .Owner  = "acme",
															 .Name   = "engine",
															 .Number = 42U,
															 .Head   = commit('e'),
															 .Base   = commit('c'),
															 .Body   = draft.Preparation.Description.Body,
															 .Files  = { file("int a;\nvoid cancel() {\n  stop();\n}\nint z;\n") } };
		const auto mapping = core::map_local_publication(draft, result);
		expect(mapping.DescriptionPreserved, "the accepted full description is the pull request's body");
		expect(mapping.Stops.size() == 2U && mapping.Stops[0].Anchor && mapping.Stops[0].Anchor->FirstLine == 1U && mapping.Stops[0].Anchor->LastLine == 3U
				   && mapping.Stops[0].Permalink == "https://github.com/acme/engine/blob/eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee/src/search.cpp#L2-L4",
			   "a matching stop acquires the pull request's anchor (zero-based, as a tour counts lines)");
		expect(!mapping.Stops[1].Anchor && mapping.Stops[1].Correspondence == core::local_correspondence::changed && mapping.Unresolved == 1U,
			   "a mismatched stop stays local, with its reason");
		expect(draft.Preparation.Tour.Stops.size() == 2U, "no stop is dropped");
		auto other = result;
		other.Body = "## Problem\n...";
		expect(!core::map_local_publication(draft, other).DescriptionPreserved, "a replaced body is reported");
		expect(!core::update_description_by_default(true) && core::update_description_by_default(false), "the description option starts off only for a local draft's pull request");
	}

	void test_request_and_answer()
	{
		auto draft = make_draft();
		draft      = ok(core::edit_description(draft, draft.Version, "Cancel", "Body.", 101), "body");
		// The worktree may hold anything now; only the candidate's readings are sent.
		const auto files    = std::vector{ file(first_text) };
		const auto question = core::local_question{ .Subject = core::local_subject::whole_change, .Thread = "change", .Text = "What could break?", .Instructions = "RULES" };
		const auto request  = core::compose_local_request(draft, question, nullptr, files);
		expect(request && request->Instructions.starts_with("RULES") && request->Instructions.find("bpr-proposals") != std::string::npos
				   && request->Text.find("RULES") == std::string::npos && request->Text.find("void cancel()") != std::string::npos && request->FilesSent == 1U && !request->Resumed,
			   "[[BPR-2140]] the instructions and answer format apart from the material, which carries the candidate's captured file");
		expect(request->Text.find("LIVE EDIT") == std::string::npos, "nothing else");
		auto       big     = std::vector{ core::local_file_reading{ .Path = "src/search.cpp", .Text = std::string(200U * 1024U, 'x') } };
		const auto bounded = core::compose_local_request(draft, question, nullptr, big);
		expect(bounded && bounded->FilesSent == 0U && bounded->Instructions.size() + bounded->Text.size() <= core::maximum_local_request_bytes, "a file past the bound is left out, and said so");
		// [[BPR-2138]]. A conversation with its session's handle resumes it:
		// neither the earlier turns nor the files go again. Without a handle,
		// the session starts with the earlier turns (a fresh answer: Ask more).
		{
			auto conversation   = core::local_conversation{ .Id = 1U, .Candidate = 1U, .Thread = "change", .Agent = "agent-1" };
			conversation.Turns  = { core::local_turn{ .Author = true, .Text = "Draft the description." }, core::local_turn{ .Author = false, .Text = "FRESH ANSWER" } };
			const auto seeding  = core::compose_local_request(draft, question, &conversation, files);
			conversation.Handle = "session-1";
			const auto resumed  = core::compose_local_request(draft, question, &conversation, files);
			expect(seeding && !seeding->Resumed && seeding->Replayed == 2U && seeding->Text.find("FRESH ANSWER") != std::string::npos && seeding->FilesSent == 1U,
				   "a session's first turn carries the earlier answer");
			expect(resumed && resumed->Resumed && resumed->Replayed == 0U && resumed->Text.find("FRESH ANSWER") == std::string::npos && resumed->FilesSent == 0U
					   && resumed->Text.find("void cancel()") == std::string::npos && resumed->Text.find("What could break?") != std::string::npos,
				   "a resumed turn sends the question, not the history or the files again");
		}
		const auto answer = core::parse_local_answer(
			"The timer path.\n\n```bpr-proposals\n[{\"target\":\"problem\",\"text\":\"The timer outlives cancel.\"},"
			"{\"target\":\"new_stop\",\"path\":\"src/search.cpp\",\"first\":2,\"last\":4,\"title\":\"Cancel\",\"text\":\"Here.\"}]\n```\n");
		expect(answer.Prose == "The timer path." && answer.Proposals.size() == 2U && answer.Proposals[0].Target == core::local_proposal_target::problem
				   && answer.Proposals[1].FirstLine == 2U && !answer.Malformed,
			   "the answer's proposals are read and its prose kept");
		const auto claim = core::parse_local_answer("So.\n```bpr-proposals\n[{\"target\":\"claim\",\"text\":\"It works.\"}]\n```");
		expect(claim.Proposals.empty() && claim.Malformed, "a claim or check is no longer something an answer can propose");
		expect(request->Text.find("\"claim\"") == std::string::npos && request->Text.find("evidence") == std::string::npos, "and the request offers neither");
		const auto broken = core::parse_local_answer("Hm.\n```bpr-proposals\nnot json\n```");
		expect(broken.Malformed && broken.Proposals.empty(), "a malformed block proposes nothing");
	}

	void test_transfer()
	{
		auto draft = make_draft();
		draft      = ok(core::edit_description(draft, draft.Version, "Cancel", "Body.", 101), "body");
		draft      = ok(core::edit_tour_brief(draft, draft.Version, "Problem text.", "", 102), "brief");
		draft      = ok(core::transfer_to_description(draft, draft.Version, "Problem text.", 103), "a deliberate transfer");
		expect(draft.Preparation.Description.Body == "Body.\n\nProblem text." && draft.Preparation.Tour.Problem == "Problem text.", "the transfer copies one way, on request");
	}

	/// The pull request window's Tour panel shows a local tour: rings from
	/// the local decision, presentation identities apart from every Git
	/// object, chats and proposals as the side chat reads them, and the
	/// panel's commands as the draft's own edits.
	void test_panel_view()
	{
		auto draft = make_draft();
		draft      = ok(core::edit_tour_brief(draft, draft.Version, "Searches outlived cancel.", "Cancel stops the worker.", 101), "brief");
		draft      = ok(core::add_local_stop(draft, draft.Version, "Cancel", "Stops.", file(first_text), core::diff_side::right, 2U, 4U, 102), "stop 1");
		draft      = ok(core::add_local_stop(draft, draft.Version, "Fields", "Two ints.", file(first_text), core::diff_side::right, 1U, 1U, 103), "stop 2");
		draft      = ok(core::add_local_stop(draft, draft.Version, "", "", file(first_text), core::diff_side::right, 5U, 5U, 104), "stop 3, empty");
		for (const auto index : { 0U, 1U }) {
			draft = ok(core::accept_local_stop(draft, draft.Version, draft.Preparation.Tour.Stops[index].Id, 105), "accept");
		}
		const auto ring = [](const core::local_tour_view& aView, const std::size_t aIndex) { return core::decide_tour_stop_acceptance(aView.Tour, aView.Tour.Stops[aIndex]).State; };
		auto       view = core::view_local_tour(draft);
		expect(view.Tour.Problem == "Searches outlived cancel." && view.Tour.Stops.size() == 3U && view.Files == std::vector<std::string>{ "src/search.cpp" }, "the panel reads the draft's tour");
		expect(ring(view, 0U) == core::tour_acceptance_state::accepted && ring(view, 2U) == core::tour_acceptance_state::not_accepted, "green and yellow as the draft decides");
		expect(view.Tour.Stops[0].Destination && view.Tour.Stops[0].Destination->FirstLine == 1U && view.Tour.Stops[0].Destination->LastLine == 3U
				   && view.Tour.Stops[0].Destination->Commit == core::local_tour_commit(draft.Id.Value, 1U),
			   "a stop's lines are its candidate's, zero-based as the panel counts");
		expect(core::local_tour_commit(draft.Id.Value, 1U) != core::local_tour_commit(draft.Id.Value, 2U) && core::local_tour_commit(draft.Id.Value, 1U) != draft.Candidates[0].Head,
			   "each candidate has its own presentation identity, apart from the real commit");
		expect(core::valid(view.Tour.Owner), "the panel's owner is well formed");
		expect(core::tour_stop_accept_need(view.Tour, view.Tour.Stops[2]) == core::tour_accept_need::title, "Accept asks for what the draft asks");

		auto next = with_successor(draft, "int a2;\nvoid cancel() {\n  stop();\n}\nint b;\n");
		view      = core::view_local_tour(next);
		expect(ring(view, 0U) == core::tour_acceptance_state::accepted, "an exact stop stays green after the refresh");
		expect(ring(view, 1U) == core::tour_acceptance_state::changed, "a changed one is Changed since accepted");
		expect(std::ranges::find(view.Reasons, std::pair{ next.Preparation.Tour.Stops[1].Id, core::local_stop_reason::changed }) != view.Reasons.end(), "with the draft's own reason");
		expect(core::tour_stop_accept_need(view.Tour, view.Tour.Stops[0]) == core::tour_accept_need::nothing, "the exact one can be accepted at the head");

		// Chats and proposals.
		auto [d1, conversation] = ok(core::append_turn(next, next.Version, core::local_stop_thread(next.Preparation.Tour.Stops[0].Id), "agent-1", "Claude",
													   { .Author = true, .Subject = core::local_subject::stop, .Text = "Say why." }, 106),
									 "a question on a stop");
		const auto proposals    = std::vector<core::local_proposal_draft>{ { .Target = core::local_proposal_target::problem, .Text = "Searches outlive cancel." },
																		   { .Target = core::local_proposal_target::description, .Text = "Body." } };
		const auto readings     = std::vector{ file("int a2;\nvoid cancel() {\n  stop();\n}\nint b;\n") };
		auto [d2, ids]          = ok(core::record_proposals(d1, d1.Version, conversation, 2U, proposals, readings, "Claude", "v1", 107), "proposals");
		auto [d3, same]         = ok(core::append_turn(d2, d2.Version, core::local_stop_thread(next.Preparation.Tour.Stops[0].Id), "agent-1", "Claude",
													   { .Author = false, .Text = "Because.", .Proposals = ids, .CostMicros = 20000U }, 108),
									 "the answer");
		expect(same == conversation, "one conversation");
		view = core::view_local_tour(d3, core::local_tour_asking{ .Thread = "change", .Text = "And the rest?" });
		expect(view.Session.Runs.size() == 2U && view.Session.Runs[0].Thread.Kind == core::tour_thread_kind::stop && view.Session.Runs[0].Reply == "Because."
				   && view.Session.Runs[0].Cost == 20000000U && view.Session.Runs[1].State == core::tour_agent_run_state::running,
			   "the side chat reads each turn, its cost, and the run in flight");
		expect(view.Proposal && view.Proposal->Items.size() == 1U && view.Proposal->Items[0].Part == core::tour_proposal_part::problem && view.ProposalItems == std::vector{ ids[0] }
				   && view.Proposal->Items[0].Run == view.Session.Runs[0].Id,
			   "the Problem's proposal waits on the panel with its turn; the description's belongs to Overview");
		auto edited = ok(core::edit_tour_brief(d3, d3.Version, "The author's own.", "Cancel stops the worker.", 109), "a later edit");
		view        = core::view_local_tour(edited);
		expect(view.Proposal && view.Proposal->Items[0].Decision == core::tour_proposal_decision::discarded && !view.Proposal->Items[0].Refusal.empty(),
			   "a stale proposal stays readable and refused");

		// Commands.
		const auto stop    = core::tour_stop_id{ next.Preparation.Tour.Stops[1].Id };
		auto       command = core::tour_command{ .Kind = core::tour_command_kind::solution, .Text = "New solution." };
		expect(core::translate_local_tour_command(next, command) == std::vector{ core::local_tour_edit{ .Kind = core::local_tour_edit_kind::brief, .Problem = "Searches outlived cancel.", .Solution = "New solution." } },
			   "the Solution saves with the Problem as it is");
		command = { .Kind = core::tour_command_kind::edit_stop, .Stop = stop, .Value = core::tour_stop{ .Id = stop, .Title = "Fields", .Body = "Renamed." } };
		expect(core::translate_local_tour_command(next, command).front().Kind == core::local_tour_edit_kind::stop_text, "a stop's text as typed");
		command = { .Kind = core::tour_command_kind::split, .Stop = stop };
		expect(core::translate_local_tour_command(next, command).empty(), "a split is not a local edit");
		command = { .Kind = core::tour_command_kind::accept, .Stop = core::tour_stop_id{ 99U } };
		expect(core::translate_local_tour_command(next, command).empty(), "an unknown stop changes nothing");
		auto typed = ok(core::edit_local_stop(next, next.Version, stop.value(), "Fields", "Renamed.", std::nullopt, core::diff_side::right, 0U, 0U, 110), "typed");
		expect(core::decide_local_stop(typed.Preparation.Tour.Stops[1]).State != core::tour_acceptance_state::accepted, "typing is not the sign-off");
		typed = ok(core::unaccept_local_stop(typed, typed.Version, typed.Preparation.Tour.Stops[0].Id, 111), "Mark not accepted");
		expect(core::decide_local_stop(typed.Preparation.Tour.Stops[0]).State == core::tour_acceptance_state::not_accepted, "withdrawn");
	}
}

namespace {
	/// The window applies a carry as a restore of the pull request's empty
	/// tour draft; a refusal is said, never dropped (2026-11-13 follow-up of
	/// the carry that opened #182 with an empty tour).
	void test_carry_refusal_words()
	{
		expect(core::local_tour_carry_refusal_words(core::tour_refusal::none).empty(), "an applied carry says nothing");
		for (const auto refusal : { core::tour_refusal::identity, core::tour_refusal::conflict, core::tour_refusal::invalid_source, core::tour_refusal::invalid_command, core::tour_refusal::bounds,
									core::tour_refusal::incomplete, core::tour_refusal::exhausted }) {
			const auto words = core::local_tour_carry_refusal_words(refusal);
			expect(words.starts_with("The local draft's tour could not start this pull request's tour: ") && words.ends_with(" The local draft keeps its tour."), "each refusal is said: " + words);
		}
		expect(core::local_tour_carry_refusal_words(core::tour_refusal::invalid_source).find("a stop's lines could not be placed") != std::string::npos, "invalid_source names the stop's lines");
	}
}

auto main() -> int
{
	test_carry_refusal_words();
	test_signed_out_authoring();
	test_correspondence();
	test_refresh_keeps_only_exact();
	test_preview_keeps_preparation_private();
	test_stored_evidence_is_dropped();
	test_proposals();
	test_conversations();
	test_publication_mapping();
	test_request_and_answer();
	test_transfer();
	test_panel_view();
	std::cout << "local draft tour tests passed\n";
	return EXIT_SUCCESS;
}
