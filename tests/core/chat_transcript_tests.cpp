// A chat's conversation as one value, what changed between two, and the one
// ending line (RFC-BPR-0130 stage 3).
#include "bpr/core/chat_transcript.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {
	using namespace bpr::core;
	using kind = chat_change_kind;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto turn(const std::string_view aId, const std::string_view aQuestion, const std::string_view aReply, const bool aRunning) -> chat_turn
	{
		auto made     = chat_turn{ };
		made.Id       = std::string{ aId };
		made.Question = std::string{ aQuestion };
		made.Reply    = std::string{ aReply };
		made.Running  = aRunning;
		return made;
	}

	[[nodiscard]] auto ending(const std::string_view aAgent, const agent_turn_outcome aOutcome, const std::optional<std::int64_t> aSeconds, const std::optional<agent_cost> aCost) -> chat_ending
	{
		auto made    = chat_ending{ };
		made.Agent   = std::string{ aAgent };
		made.Outcome = aOutcome;
		made.Seconds = aSeconds;
		made.Cost    = aCost;
		return made;
	}

	void test_the_one_ending_line()
	{
		using outcome = agent_turn_outcome;
		expect(chat_ending_words(ending("Claude Code", outcome::answered, 42, std::nullopt)) == "Claude answered in 0:42.", "who and how long, a full stop on a line of one part");
		expect(chat_ending_words(ending("Codex", outcome::answered, 31, 140'000'000ULL)) == "Codex answered in 0:31 · $0.14", "with what it cost, and no full stop");
		expect(chat_ending_words(ending("", outcome::answered, std::nullopt, std::nullopt)) == "The agent answered.", "an agent not known, and a turn not timed");
		expect(chat_ending_words(ending("Fixture agent", outcome::answered, 3725, std::nullopt)) == "Fixture agent answered in 1:02:05.", "a name without Code is said whole; an hour reads h:mm:ss");
		expect(chat_ending_words(ending("Claude Code", outcome::stopped, 12, std::nullopt)) == "Stopped after 0:12.", "a stopped run");
		expect(chat_ending_words(ending("Claude Code", outcome::exited_with_error, std::nullopt, 10'000'000ULL)) == "Stopped · $0.01", "a failed run says Stopped, with its cost");
		expect(chat_ending_words(ending("Claude Code", outcome::closed_with_app, 1006, std::nullopt)) == "Stopped when the app closed, after 16:46.", "a run the app's closing stopped");
		auto review     = ending("Claude Code", outcome::answered, 72, 410'000'000ULL);
		review.Findings = 2U;
		expect(chat_ending_words(review) == "Claude answered in 1:12 · $0.41 · 2 findings", "the Agent tab's findings follow the cost");
		review.Findings = 1U;
		review.Cost.reset();
		expect(chat_ending_words(review) == "Claude answered in 1:12 · 1 finding", "one finding");
		expect(chat_agent_short_name("Claude Code") == "Claude" && chat_agent_short_name("Code") == "Code" && chat_agent_short_name("") == "The agent", "the short name");
		expect(chat_elapsed_words(-4) == "0:00", "a negative time reads as none");
	}

	void test_equal_transcripts_change_nothing()
	{
		auto one = chat_transcript{ };
		one.Turns.push_back(turn("a", "Why?", "Because.", false));
		one.Turns.push_back(turn("b", "And?", "", true));
		expect(chat_transcript_delta(one, one).empty(), "equal transcripts yield no change");
		expect(chat_transcript_delta({ }, { }).empty(), "nor do two empty ones");
		expect(one.find("b") == &one.Turns[1] && one.find("c") == nullptr, "a turn is found by its identity");
	}

	void test_a_growing_reply_names_its_turn()
	{
		auto before = chat_transcript{ };
		before.Turns.push_back(turn("a", "Why?", "Because.", false));
		before.Turns.push_back(turn("b", "And?", "It", true));
		auto after           = before;
		after.Turns[1].Reply = "It asserts.";
		const auto delta     = chat_transcript_delta(before, after);
		const auto expected  = chat_delta{ { .Kind = kind::reply_grew, .Turn = "b", .Index = 1U } };
		expect(delta == expected, "a reply that grows yields one change, naming its turn");

		auto settled             = after;
		settled.Turns[1].Running = false;
		settled.Turns[1].Ending  = ending("Codex", agent_turn_outcome::answered, 4, std::nullopt);
		settled.Turns[1].Reply += " Clamp it.";
		expect(chat_transcript_delta(after, settled) == chat_delta{ { .Kind = kind::settled, .Turn = "b", .Index = 1U } }, "a run that ends settles its turn, even as its reply grows");

		auto changed                  = settled;
		changed.Turns[0].Actions.Copy = true;
		changed.Turns[1].Attachments.push_back({ .Key = "proposal", .Payload = chat_follow_up_proposal{ } });
		const auto both = chat_transcript_delta(settled, changed);
		expect(both == chat_delta{ { .Kind = kind::changed, .Turn = "a", .Index = 0U }, { .Kind = kind::changed, .Turn = "b", .Index = 1U } }, "an action or an attachment changes its turn, one change a turn");
	}

	void test_turns_come_and_go()
	{
		auto before = chat_transcript{ };
		before.Turns.push_back(turn("a", "One", "1", false));
		before.Turns.push_back(turn("b", "Two", "2", false));
		before.Turns.push_back(turn("c", "Three", "3", false));
		auto removed = before;
		removed.Turns.erase(removed.Turns.begin() + 1);
		expect(chat_transcript_delta(before, removed) == chat_delta{ { .Kind = kind::removed, .Turn = "b", .Index = 1U } }, "a removed turn yields one removal at its earlier place");

		auto inserted = before;
		inserted.Turns.insert(inserted.Turns.begin() + 2, turn("d", "Four", "", true));
		expect(chat_transcript_delta(before, inserted) == chat_delta{ { .Kind = kind::appended, .Turn = "d", .Index = 2U } }, "a new turn before the last is appended at its place");

		auto moved = before;
		std::swap(moved.Turns[0], moved.Turns[2]);
		const auto delta = chat_transcript_delta(before, moved);
		// c, b, a after a, b, c: c stays in order, b and a moved.
		expect(delta == chat_delta{ { .Kind = kind::removed, .Turn = "a", .Index = 0U }, { .Kind = kind::removed, .Turn = "b", .Index = 1U }, { .Kind = kind::appended, .Turn = "b", .Index = 1U }, { .Kind = kind::appended, .Turn = "a", .Index = 2U } },
			   "turns out of their order are removed and appended again");

		expect(chat_transcript_delta({ }, before).size() == 3U && chat_transcript_delta(before, { }).size() == 3U, "every turn appended from nothing, removed to nothing");
	}

	[[nodiscard]] auto subject(const std::string_view aKey) -> follow_up_subject
	{
		auto made  = follow_up_subject{ };
		made.Kind  = follow_up_subject_kind::suggested_comment;
		made.Key   = std::string{ aKey };
		made.Path  = "src/renderer.cpp";
		made.First = 12U;
		made.Last  = 13U;
		made.Title = "A zero height reaches the swap chain.";
		made.Text  = "issue (blocking): clamp the height.";
		return made;
	}

	void test_the_item_chat_projects_its_rounds()
	{
		auto       state = open_follow_up(subject("finding:7:0"));
		const auto facts = follow_up_thread_facts{ .Agent = "Claude Code", .Seconds = { } };
		expect(project_follow_up(state, facts).Turns.empty(), "a new item's chat has no turns");

		const auto asked = ask_follow_up(state, "Why is this blocking?");
		expect(asked.Request.has_value(), "the question runs");
		const auto asking = project_follow_up(state, facts);
		expect(asking.Turns.size() == 1U && asking.Turns[0].Running && asking.Turns[0].Question == "Why is this blocking?" && !asking.Turns[0].Ending, "the question going is a running turn");

		static_cast<void>(settle_follow_up(state, state.Pending, std::string{ "It asserts on **minimise**." }));
		const auto answered = project_follow_up(state, follow_up_thread_facts{ .Agent = "Claude Code", .Seconds = { 42 } });
		expect(answered.Turns.size() == 1U && !answered.Turns[0].Running && answered.Turns[0].Reply == "It asserts on **minimise**." && answered.Turns[0].Actions.Copy, "the answer settles the same turn, with Copy");
		expect(answered.Turns[0].Ending && chat_ending_words(*answered.Turns[0].Ending) == "Claude answered in 0:42.", "its ending line: who and how long");
		expect(chat_transcript_delta(asking, answered) == chat_delta{ { .Kind = kind::settled, .Turn = "finding:7:0/round/0", .Index = 0U } }, "the answer is one settled change");

		const auto proposed = propose_follow_up(state, follow_up_apply::update_suggestion, "issue (blocking): clamp the height.");
		expect(proposed.Request.has_value(), "the update runs");
		const auto proposing = project_follow_up(state, facts);
		expect(proposing.Turns.size() == 2U && proposing.Turns[1].Running && proposing.Turns[1].Question == "Update suggested comment", "the update's run is a turn saying what it asked for");
		const auto run = state.Pending;
		static_cast<void>(settle_follow_up(state, run, std::string{ "issue (blocking): clamp the height too." }));
		const auto waiting = project_follow_up(state, facts);
		expect(waiting.Turns.size() == 2U && waiting.Turns[1].Id == proposing.Turns[1].Id && waiting.Turns[1].Question.empty() && !waiting.Turns[1].Running, "once written, the same turn is only the proposal");
		expect(waiting.Turns[1].Attachments.size() == 1U, "which is its attachment");
		const auto& attached = std::get<chat_follow_up_proposal>(waiting.Turns[1].Attachments[0].Payload);
		expect(attached.Proposal.Text == "issue (blocking): clamp the height too." && attached.Usable, "the proposal, usable while nothing runs");

		static_cast<void>(ask_follow_up(state, "Is that enough?"));
		const auto again = project_follow_up(state, facts);
		expect(again.Turns.size() == 3U && again.Turns[1].Running && again.Turns[2].Id == waiting.Turns[1].Id, "a question asked while the proposal waits goes before it");
		expect(!std::get<chat_follow_up_proposal>(again.Turns[2].Attachments[0].Payload).Usable, "and the proposal waits unusable while it runs");
		const auto delta = chat_transcript_delta(waiting, again);
		expect(delta == chat_delta{ { .Kind = kind::appended, .Turn = "finding:7:0/round/1", .Index = 1U }, { .Kind = kind::changed, .Turn = waiting.Turns[1].Id, .Index = 2U } }, "one turn appended, the proposal changed");

		static_cast<void>(settle_follow_up(state, state.Pending, std::nullopt));
		expect(discard_follow_up(state, run), "Discard");
		const auto discarded = project_follow_up(state, facts);
		expect(chat_transcript_delta(again, discarded) == chat_delta{ { .Kind = kind::removed, .Turn = "finding:7:0/round/1", .Index = 1U }, { .Kind = kind::removed, .Turn = waiting.Turns[1].Id, .Index = 2U } }, "a failed question and a discarded proposal leave");

		const auto other = project_follow_up(open_follow_up(subject("finding:8:0")), facts);
		auto       next  = open_follow_up(subject("finding:8:0"));
		static_cast<void>(ask_follow_up(next, "Why?"));
		expect(project_follow_up(next, facts).Turns[0].Id != discarded.Turns[0].Id && other.Turns.empty(), "another item's chat shares no turn with this one");
	}

	[[nodiscard]] auto tour_run(const std::uint64_t aId, const tour_thread& aThread, const tour_agent_run_state aState, const std::string_view aQuestion, const std::string_view aReply) -> tour_agent_run
	{
		auto run     = tour_agent_run{ };
		run.Id       = tour_agent_run_id{ aId };
		run.Request  = tour_agent_request::ask;
		run.Thread   = aThread;
		run.State    = aState;
		run.Question = std::string{ aQuestion };
		run.Reply    = std::string{ aReply };
		return run;
	}

	void test_the_tour_chat_projects_its_runs()
	{
		const auto stop    = tour_thread{ .Kind = tour_thread_kind::stop, .Stop = tour_stop_id{ 4U } };
		const auto tour    = tour_thread{ .Kind = tour_thread_kind::tour, .Stop = tour_stop_id{ } };
		auto       session = tour_agent_session{ };
		session.Runs.push_back(tour_run(1U, stop, tour_agent_run_state::answered, "What does this mean?", "It clamps."));
		session.Runs.back().Seconds = 7U;
		session.Runs.back().Cost    = 140'000'000ULL;
		session.Runs.push_back(tour_run(2U, tour, tour_agent_run_state::answered, "Fewer stops", "Merged two."));
		auto failed = tour_run(3U, stop, tour_agent_run_state::failed, "And then?", "");
		failed.Entries.push_back({ .Kind = tour_transcript_kind::ended, .Text = "  exit 1: no workspace\nmore detail  ", .Note = tour_transcript_note::none });
		session.Runs.push_back(failed);

		auto facts    = tour_thread_facts{ };
		facts.Agents  = { { 1U, "Claude Code" } };
		facts.Deletes = true;
		facts.Activity.emplace(1U, chat_activity{ .Activity = agent_turn_activity{ }, .Seconds = 7 });
		const auto part = project_tour_thread(session, stop, facts);
		expect(part.Turns.size() == 2U && part.Turns[0].Id == "tour-run:1" && part.Turns[1].Id == "tour-run:3", "a part's chat holds its own runs, identity the run's");
		expect(part.Turns[0].Reply == "It clamps." && part.Turns[0].Ending && chat_ending_words(*part.Turns[0].Ending) == "Claude answered in 0:07 · $0.14", "an answer and the one ending line");
		expect(part.Turns[0].Actions.Copy && part.Turns[0].Actions.Recovery == agent_turn_recovery::none && part.Turns[0].Activity && part.Turns[0].Activity->Seconds == 7, "Copy, and what the run did");
		expect(part.Turns[0].Attachments.empty(), "an answer that is not the newest offers no update line");
		const auto& why = part.Turns[1];
		expect(why.Reply.empty() && why.Failure == "Didn't finish: exit 1: no workspace" && why.FailureDetail == "exit 1: no workspace\nmore detail", "a run that did not finish says why, and Copy takes its words");
		expect(why.Ending && chat_ending_words(*why.Ending) == "Stopped." && why.Ending->Agent == "The tour agent", "it ended as stopped, by the tour agent when none is known");
		expect(why.Actions.Recovery == agent_turn_recovery::resend && why.Actions.Delete, "the newest failure offers Resend and Delete");
		facts.Deletes   = false;
		facts.Running   = true;
		const auto busy = project_tour_thread(session, stop, facts);
		expect(busy.Turns[1].Actions.Recovery == agent_turn_recovery::none && !busy.Turns[1].Actions.Delete, "while a run goes, or without a Delete dispatch, neither");

		// The newest answer that changed nothing offers the update.
		session.Runs.pop_back();
		facts.Running       = false;
		const auto answered = project_tour_thread(session, stop, facts);
		expect(answered.Turns.size() == 1U && answered.Turns[0].Attachments.size() == 1U && answered.Turns[0].Attachments[0].Key == "unchanged", "the newest answer that changed nothing hangs the update line");

		// A part's proposal hangs on the turn that proposed it.
		auto proposal = tour_proposal{ };
		proposal.Run  = tour_agent_run_id{ 1U };
		auto item     = tour_proposal_item{ };
		item.Part     = tour_proposal_part::stop_text;
		item.Text     = "A clearer explanation.";
		item.Run      = tour_agent_run_id{ 1U };
		proposal.Items.push_back(item);
		session.Runs[0].Proposed.StopTexts = 1U;
		facts.Proposal                     = proposal;
		const auto proposed                = project_tour_thread(session, stop, facts);
		expect(proposed.Turns[0].Attachments.size() == 1U && proposed.Turns[0].Attachments[0].Key == "proposal:0" && std::get<chat_tour_proposal>(proposed.Turns[0].Attachments[0].Payload).Item.Text == "A clearer explanation.", "each part the turn proposed is its attachment");
		facts.Proposal->Items[0].Decision = tour_proposal_decision::accepted;
		expect(chat_transcript_delta(proposed, project_tour_thread(session, stop, facts)) == chat_delta{ { .Kind = chat_change_kind::changed, .Turn = "tour-run:1", .Index = 0U } }, "deciding it changes that turn alone");

		// The whole tour: the revision, the parts refused, the focus, and the
		// feedback waiting for the draft as its last turn.
		session.Runs[1].Proposed.Merges = 1U;
		session.Runs[1].Proposed.Refused.push_back({ .Kind = tour_revision_kind::merge, .Stops = { 2U, 9U }, .Reason = tour_revision_refusal::not_in_tour });
		session.Runs[1].FocusSet = "the renderer";
		session.Focus            = "the renderer";
		auto merge               = tour_proposal_item{ };
		merge.Part               = tour_proposal_part::merge_stops;
		merge.Run                = tour_agent_run_id{ 2U };
		facts.Proposal->Items.push_back(merge);
		session.Waiting  = "Keep the order.";
		const auto whole = project_tour_thread(session, tour, facts);
		expect(whole.Turns.size() == 2U && whole.Turns[1].Id == "tour-waiting" && whole.Turns[1].Question == "Keep the order." && whole.Turns[1].QuestionNote == "Sent when the draft is ready, to revise it." && !whole.Turns[1].Ending, "the feedback waiting is the last turn, with when it goes");
		const auto& shape = whole.Turns[0].Attachments;
		expect(shape.size() == 3U && shape[0].Key == "focus" && shape[1].Key == "revision" && shape[2].Key == "refused", "the focus, the revision and the parts refused, in that order");
		expect(std::get<chat_tour_focus>(shape[0].Payload).Undoable && std::get<chat_tour_revision>(shape[1].Payload).Items == std::vector<std::size_t>{ 1U }, "the focus can be undone; the revision names its own parts");

		// A local draft's check is what its reply is.
		session.Runs[0].Check = local_check{ };
		const auto checked    = project_tour_thread(session, stop, facts);
		expect(checked.Turns[0].Attachments.front().Key == "check" && checked.Turns[0].Attachments.front().Place == chat_attachment_place::after_reply, "a check card stands under its reply");

		// A running run is a running turn.
		session.Runs.push_back(tour_run(5U, stop, tour_agent_run_state::running, "Still there?", ""));
		const auto running = project_tour_thread(session, stop, facts);
		expect(running.Turns.back().Running && !running.Turns.back().Ending && running.Turns.back().Attachments.empty(), "a running run is a running turn");
	}
}

auto main() -> int
{
	test_the_one_ending_line();
	test_equal_transcripts_change_nothing();
	test_a_growing_reply_names_its_turn();
	test_turns_come_and_go();
	test_the_item_chat_projects_its_rounds();
	test_the_tour_chat_projects_its_runs();
	std::cout << "chat-transcript-tests:pass\n";
	return EXIT_SUCCESS;
}
