// [[BPR-2286]]. A follow-up's conversation, proposals and apply decisions,
// without Qt, a host or an agent.
#include "bpr/core/agent_follow_up.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using core::follow_up_apply;
	using core::follow_up_phase;
	using core::follow_up_refusal;
	using core::follow_up_subject_kind;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto contains(const std::string& aText, const std::string_view aPart) -> bool { return aText.find(aPart) != std::string::npos; }

	auto suggestion() -> core::follow_up_subject
	{
		auto subject  = core::follow_up_subject{ };
		subject.Kind  = follow_up_subject_kind::suggested_comment;
		subject.Key   = "finding:7:0";
		subject.Path  = "src/engine/renderer.cpp";
		subject.First = 12U;
		subject.Last  = 13U;
		subject.Title = "A zero height still reaches the swap chain.";
		subject.Text  = "issue (blocking): clamp both.";
		return subject;
	}

	void test_offers_follow_the_item()
	{
		auto subject = suggestion();
		expect(core::valid(subject), "a suggestion with a key and text is valid");
		expect(core::follow_up_offers(subject) == std::vector{ follow_up_apply::update_suggestion }, "a suggestion offers Update suggested comment");
		subject.Kind = follow_up_subject_kind::draft;
		expect(core::follow_up_offers(subject) == std::vector{ follow_up_apply::update_draft }, "a draft offers Update draft");
		subject.Kind = follow_up_subject_kind::thread;
		subject.Text.clear();
		subject.Thread = { { .Author = "octo-author", .Body = "Why clamp?" } };
		expect(core::valid(subject) && core::follow_up_offers(subject).empty(), "a thread that takes no replies offers nothing to apply");
		subject.Replies = true;
		expect(core::follow_up_offers(subject) == std::vector{ follow_up_apply::draft_reply }, "a thread that takes replies offers Draft reply");
		subject.Thread.clear();
		expect(!core::valid(subject), "a thread with no posts is not an item");
		auto keyless = suggestion();
		keyless.Key.clear();
		expect(!core::valid(keyless), "an item has a key");
		expect(core::stable_name(follow_up_apply::draft_reply) == "draft-reply" && core::stable_name(follow_up_subject_kind::suggested_comment) == "suggested-comment"
				   && core::stable_name(follow_up_refusal::target_changed) == "target-changed",
			   "stable names");
	}

	void test_rounds_carry_the_item_and_the_conversation()
	{
		auto state = core::open_follow_up(suggestion());
		auto first = core::ask_follow_up(state, "  Why is this blocking?  ");
		expect(first.Refusal == follow_up_refusal::none && first.Request && first.Request->Id == 1U, "a question starts run 1");
		const auto& message = first.Request->Message;
		expect(message.starts_with(core::follow_up_marker), "the message opens with the marker");
		expect(contains(message, "Where: src/engine/renderer.cpp lines 12-13") && contains(message, "The finding: A zero height") && contains(message, "issue (blocking): clamp both."),
			   "it carries the place, the finding and the suggestion");
		expect(contains(message, "The reviewer asks:\nWhy is this blocking?\n") && !contains(message, "Earlier in this conversation"), "the first round has the question and no transcript");
		expect(state.Phase == follow_up_phase::asking && state.Pending == 1U, "a question's run is going");
		expect(core::ask_follow_up(state, "Again?").Refusal == follow_up_refusal::busy, "one run at a time");
		expect(core::settle_follow_up(state, 9U, std::string{ "late" }) == follow_up_refusal::stale && state.Pending == 1U, "another run's answer is stale and changes nothing");
		expect(core::settle_follow_up(state, 1U, std::string{ "  It asserts on minimise.\n" }) == follow_up_refusal::none, "the answer settles the run");
		expect(state.Phase == follow_up_phase::idle && state.Rounds.size() == 1U && state.Rounds[0].Question == "Why is this blocking?" && state.Rounds[0].Answer == "It asserts on minimise.",
			   "it becomes a round, trimmed");
		expect(core::settle_follow_up(state, 1U, std::string{ "twice" }) == follow_up_refusal::stale, "a run settles once");

		const auto second = core::ask_follow_up(state, "Softer?");
		expect(second.Request && second.Request->Id == 2U && contains(second.Request->Message, "Earlier in this conversation:\nReviewer: Why is this blocking?\nYou: It asserts on minimise.\n"),
			   "the next round carries the conversation so far");
		auto twin = core::open_follow_up(suggestion());
		static_cast<void>(core::ask_follow_up(twin, "Why is this blocking?"));
		static_cast<void>(core::settle_follow_up(twin, 1U, std::string{ "It asserts on minimise." }));
		expect(core::ask_follow_up(twin, "Softer?") == second, "equal state, equal question: equal run and message");
		expect(core::settle_follow_up(state, 2U, std::nullopt) == follow_up_refusal::failed && state.Rounds.size() == 1U && state.Phase == follow_up_phase::idle, "a failed run adds no round");
		expect(core::ask_follow_up(state, " \n ").Refusal == follow_up_refusal::empty_question, "a blank question is not sent");
		expect(core::ask_follow_up(state, std::string(core::maximum_follow_up_text_bytes + 1U, 'q')).Refusal == follow_up_refusal::empty_question, "nor one too long");
	}

	void test_old_rounds_are_bounded()
	{
		auto state = core::open_follow_up(suggestion());
		for (auto round = 0U; round < core::maximum_follow_up_rounds; ++round) {
			const auto step = core::ask_follow_up(state, "Question " + std::to_string(round));
			expect(step.Request.has_value(), "each round is asked");
			static_cast<void>(core::settle_follow_up(state, step.Request->Id, std::string(4096U, 'a')));
		}
		expect(core::ask_follow_up(state, "One more").Refusal == follow_up_refusal::too_many_rounds, "a follow-up holds a bounded number of rounds");
		const auto propose = core::propose_follow_up(state, follow_up_apply::update_suggestion, state.Subject.Text);
		expect(propose.Request && contains(propose.Request->Message, "earlier rounds were not sent") && contains(propose.Request->Message, "Question 31"),
			   "the newest rounds are kept and the rest said to be left out");
		expect(propose.Request->Message.size() < core::maximum_follow_up_transcript_bytes + 8U * 1024U, "the message stays bounded");
	}

	void test_update_suggestion_is_seen_then_used()
	{
		auto state = core::open_follow_up(suggestion());
		expect(core::propose_follow_up(state, follow_up_apply::draft_reply, { }).Refusal == follow_up_refusal::not_offered, "a suggestion cannot draft a reply");
		auto step = core::propose_follow_up(state, follow_up_apply::update_suggestion, state.Subject.Text);
		expect(step.Request && contains(step.Request->Message, "bpr.follow-up.update-suggestion\nRewrite the suggested comment"), "Update asks for the new comment by name");
		expect(state.Phase == follow_up_phase::proposing, "a proposal's run is going");
		expect(core::settle_follow_up(state, step.Request->Id, std::string{ "```text\nissue (blocking): clamp both.\n```" }) == follow_up_refusal::unchanged && !state.Proposal,
			   "the same text is not a proposal");
		step = core::propose_follow_up(state, follow_up_apply::update_suggestion, state.Subject.Text);
		expect(core::settle_follow_up(state, step.Request->Id, std::string{ "```markdown\nissue (blocking): clamp the height too.\n```" }) == follow_up_refusal::none && state.Proposal,
			   "a new text is a proposal");
		expect(state.Proposal->Text == "issue (blocking): clamp the height too." && state.Proposal->Apply == follow_up_apply::update_suggestion, "one plain fence around it is unwrapped");
		expect(state.Subject.Text == "issue (blocking): clamp both.", "nothing is replaced before Use");
		const auto id = state.Proposal->Id;
		expect(core::apply_follow_up(state, id + 1U, state.Subject.Text).Refusal == follow_up_refusal::stale, "Use names the proposal shown");
		expect(core::apply_follow_up(state, id, "someone edited it").Refusal == follow_up_refusal::target_changed && state.Proposal, "a suggestion changed since is not replaced");
		const auto used = core::apply_follow_up(state, id, "issue (blocking): clamp both.");
		expect(used.Refusal == follow_up_refusal::none && used.Key == "finding:7:0" && used.Text == "issue (blocking): clamp the height too." && used.Apply == follow_up_apply::update_suggestion,
			   "Use puts the proposal in place of the suggestion it names");
		expect(!state.Proposal && state.Applied == 1U && state.Subject.Text == used.Text, "the used text is the item's text from now on");
		expect(core::apply_follow_up(state, id, used.Text).Refusal == follow_up_refusal::no_proposal, "a proposal is used once");
	}

	void test_update_draft_and_draft_reply()
	{
		auto draft = suggestion();
		draft.Kind = follow_up_subject_kind::draft;
		draft.Key  = "draft:41";
		draft.Text = "this seems wrong?";
		auto state = core::open_follow_up(draft);
		auto step  = core::propose_follow_up(state, follow_up_apply::update_draft, draft.Text);
		expect(step.Request && contains(step.Request->Message, "The reviewer's draft:\n```text\nthis seems wrong?") && contains(step.Request->Message, "bpr.follow-up.update-draft"), "Update draft asks about the draft");
		static_cast<void>(core::settle_follow_up(state, step.Request->Id, std::string{ "question: does a zero height reach the swap chain?" }));
		expect(core::discard_follow_up(state, state.Proposal->Id) && !state.Proposal, "Discard drops the proposal, unused");
		expect(core::apply_follow_up(state, 1U, draft.Text).Refusal == follow_up_refusal::no_proposal, "nothing is left to use");

		auto thread     = core::follow_up_subject{ };
		thread.Kind     = follow_up_subject_kind::thread;
		thread.Key      = "review-thread:PRRT_1";
		thread.Path     = "src/engine/renderer.cpp";
		thread.First    = 12U;
		thread.Replies  = true;
		thread.Thread   = { { .Author = "octo-reviewer", .Body = "Should the height clamp too?" }, { .Author = "octo-author", .Body = "Why would it?" } };
		auto       talk = core::open_follow_up(thread);
		const auto ask  = core::ask_follow_up(talk, "Who is right?");
		expect(ask.Request && contains(ask.Request->Message, "The conversation:\n- octo-reviewer: Should the height clamp too?\n- octo-author: Why would it?\n") && contains(ask.Request->Message, "Where: src/engine/renderer.cpp line 12"),
			   "a thread's posts go with each round");
		static_cast<void>(core::settle_follow_up(talk, ask.Request->Id, std::string{ "The reviewer." }));
		step = core::propose_follow_up(talk, follow_up_apply::draft_reply, "my words");
		expect(step.Request && contains(step.Request->Message, "bpr.follow-up.draft-reply\nWrite a reply"), "Draft reply asks for a reply");
		static_cast<void>(core::settle_follow_up(talk, step.Request->Id, std::string{ "A minimised window reports zero height, so yes." }));
		const auto used = core::apply_follow_up(talk, talk.Proposal->Id, "whatever the composer holds now");
		expect(used.Refusal == follow_up_refusal::none && used.Apply == follow_up_apply::draft_reply && used.Key == "review-thread:PRRT_1" && talk.Subject.Text.empty(),
			   "a reply goes in the thread's composer whatever it holds; the thread is unchanged");
	}

	void test_refresh_and_proposals_are_read()
	{
		auto state = core::open_follow_up(suggestion());
		auto other = suggestion();
		other.Key  = "finding:7:1";
		expect(!core::refresh_follow_up(state, other), "another item is not this follow-up");
		auto newer = suggestion();
		newer.Text = "issue (blocking): edited.";
		expect(core::refresh_follow_up(state, newer) && state.Subject.Text == "issue (blocking): edited.", "asked again, the item's text is as it is now");
		expect(core::read_follow_up_proposal("```suggestion\nx\n```") == std::optional<std::string>{ "```suggestion\nx\n```" }, "a suggestion block is the comment's own");
		expect(core::read_follow_up_proposal("```\na\n```\nb\n```\nc\n```") == std::optional<std::string>{ "```\na\n```\nb\n```\nc\n```" }, "two blocks are not a wrapper");
		expect(!core::read_follow_up_proposal(" \n") && !core::read_follow_up_proposal(std::string(core::maximum_follow_up_proposal_characters + 1U, 'a')), "empty or too long is nothing");
		auto step = core::propose_follow_up(state, follow_up_apply::update_suggestion, state.Subject.Text);
		expect(core::settle_follow_up(state, step.Request->Id, std::string(core::maximum_follow_up_proposal_characters + 1U, 'a')) == follow_up_refusal::too_long, "a too-long answer is refused");
		step = core::propose_follow_up(state, follow_up_apply::update_suggestion, state.Subject.Text);
		expect(core::settle_follow_up(state, step.Request->Id, std::string{ "  " }) == follow_up_refusal::empty_answer, "an empty one too");
	}

	/// RFC-BPR-0130 stage 7. The item chat's rounds are a session of the
	/// agent's own: choosing one shows its rounds, and each later message
	/// continues it instead of carrying them again.
	void test_a_session_holds_the_rounds()
	{
		auto state = core::open_follow_up(suggestion());
		expect(!state.InSession, "a follow-up starts as fresh runs");
		auto asked = core::ask_follow_up(state, "Why?");
		expect(core::restore_follow_up(state, { }) == follow_up_refusal::busy && !state.InSession && state.Phase == follow_up_phase::asking, "a session is not chosen while a run is going");
		expect(core::settle_follow_up(state, asked.Request->Id, std::string{ "Because." }) == follow_up_refusal::none, "the run settles");
		const auto saved = std::vector<core::follow_up_round>{ { .Question = "Is it blocking?", .Answer = "Yes." }, { .Question = "Why?", .Answer = "It asserts." } };
		expect(core::restore_follow_up(state, saved) == follow_up_refusal::none && state.InSession && state.Rounds == saved && state.Refused == follow_up_refusal::none, "the chosen session's rounds are the conversation");
		const auto next = core::ask_follow_up(state, "And the width?");
		expect(next.Request && contains(next.Request->Message, "The reviewer asks:\nAnd the width?\n") && contains(next.Request->Message, "issue (blocking): clamp both."), "a message still carries the item and the question");
		expect(!contains(next.Request->Message, "Earlier in this conversation") && !contains(next.Request->Message, "Reviewer: Is it blocking?"), "and not the rounds the session holds");
		expect(core::settle_follow_up(state, next.Request->Id, std::string{ "Both." }) == follow_up_refusal::none && state.Rounds.size() == 3U, "the round joins the session's");
		const auto proposed = core::propose_follow_up(state, follow_up_apply::update_suggestion, "issue (blocking): clamp both.");
		expect(proposed.Request && !contains(proposed.Request->Message, "Reviewer: ") && contains(proposed.Request->Message, "bpr.follow-up.update-suggestion"), "a proposal continues the session too");
		expect(core::settle_follow_up(state, proposed.Request->Id, std::string{ "issue (blocking): clamp the height too." }) == follow_up_refusal::none && state.Proposal, "its proposal waits");
		expect(core::restore_follow_up(state, { }) == follow_up_refusal::none && state.Rounds.empty() && !state.Proposal && state.InSession, "a new session empties the chat and lets the proposal go");
		auto longest            = std::vector<core::follow_up_round>(core::maximum_follow_up_rounds + 3U, core::follow_up_round{ .Question = "Q", .Answer = "A" });
		longest.back().Question = "Last";
		expect(core::restore_follow_up(state, longest) == follow_up_refusal::none && state.Rounds.size() == core::maximum_follow_up_rounds && state.Rounds.back().Question == "Last",
			   "a session longer than one chat keeps shows its latest rounds");
	}
} // namespace

auto main() -> int
{
	test_offers_follow_the_item();
	test_rounds_carry_the_item_and_the_conversation();
	test_old_rounds_are_bounded();
	test_update_suggestion_is_seen_then_used();
	test_update_draft_and_draft_reply();
	test_refresh_and_proposals_are_read();
	test_a_session_holds_the_rounds();
	std::cout << "agent-follow-up-core-tests:pass\n";
	return EXIT_SUCCESS;
}
