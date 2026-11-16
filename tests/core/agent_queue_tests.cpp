// RFC-BPR-0126 ([[BPR-2151]], core) for RFC-BPR-0128's Ask (owner,
// 2026-11-14: "a plain click queues it"). One session's queue: order,
// revisions, limits, the edit hold, pauses, release only when idle.
#include "bpr/core/agent_queue.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {
	using namespace bpr::core;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto ask(const std::string& aLabel) -> queued_input { return { .QuickAsk = "agent-tab.what-changed", .Scope = "pull-request", .Label = aLabel }; }

	void test_order_and_release()
	{
		auto queue = agent_queue{ };
		expect(enqueue(queue, ask("What changed since my review"), 0U) == queue_refusal::none && enqueue(queue, ask("Explain this file"), 1U) == queue_refusal::none, "two asks queue");
		expect(enqueue(queue, ask("late"), 0U) == queue_refusal::stale && queue.Entries.size() == 2U, "a stale enqueue changes nothing");
		expect(!next_release(queue, queue_agent::not_set_up) && !next_release(queue, queue_agent::busy), "nothing is released while no agent is set up or a turn runs");
		const auto first = next_release(queue, queue_agent::idle);
		expect(first == queue.Entries.front().Id, "the first in order is released when idle");
		expect(mark_sending(queue, *first) == queue_refusal::none && !next_release(queue, queue_agent::idle), "one at a time");
		expect(withdraw(queue, *first, queue.Revision) == queue_refusal::not_editable, "a sending entry cannot be withdrawn");
		settle(queue, *first, queue_pause::none);
		expect(queue.Entries.size() == 1U && next_release(queue, queue_agent::idle) == queue.Entries.front().Id, "a finished turn releases the next");
	}

	void test_limits()
	{
		auto queue = agent_queue{ };
		for (auto index = 0U; index < maximum_queued_inputs; ++index) {
			expect(enqueue(queue, ask("ask"), queue.Revision) == queue_refusal::none, "within the limit");
		}
		expect(enqueue(queue, ask("one more"), queue.Revision) == queue_refusal::full && queue.Entries.size() == maximum_queued_inputs, "16 inputs at most");
		auto big = agent_queue{ };
		expect(enqueue(big, { .Label = "m", .Text = std::string(maximum_queued_text_bytes + 1U, 'x') }, 0U) == queue_refusal::too_long, "the question limit holds");
		for (auto index = 0U; index < 15U; ++index) {
			expect(enqueue(big, { .Label = "m", .Text = std::string(16U * 1024U, 'x') }, big.Revision) == queue_refusal::none, "large messages");
		}
		expect(enqueue(big, { .Label = "m", .Text = std::string(16U * 1024U, 'x') }, big.Revision) == queue_refusal::too_large, "256 KiB in all");
		expect(enqueue(big, { }, big.Revision) == queue_refusal::empty, "an empty input is refused");
	}

	void test_hold_pause_and_restore()
	{
		auto queue = agent_queue{ };
		static_cast<void>(enqueue(queue, { .Label = "Draft a reply", .Text = "draft" }, 0U));
		static_cast<void>(enqueue(queue, ask("Explain this file"), queue.Revision));
		const auto first = queue.Entries.front().Id;
		// RFC-BPR-0126 Q9 as refined by the owner: an edit holds its entry
		// without pausing; the queue does not skip past it, and on Save or
		// Cancel it continues by itself.
		expect(hold(queue, first, queue.Revision) == queue_refusal::none && queue.Paused == queue_pause::none && queue.Entries.front().Held, "an edit holds its entry, without pausing");
		expect(!next_release(queue, queue_agent::idle), "a held entry is never sent, and nothing is sent past it");
		// RFC-BPR-0126 AC04: every window showing the session edits its one
		// queue, so a message one window edits cannot be edited or removed
		// from another until Save or Cancel.
		expect(hold(queue, first, queue.Revision) == queue_refusal::not_editable && withdraw(queue, first, queue.Revision) == queue_refusal::not_editable && queue.Entries.front().Held,
			   "a held entry is not edited or removed twice");
		expect(release_hold(queue, first, std::string{ "edited" }, queue.Revision) == queue_refusal::none && queue.Entries.front().Input.Text == "edited" && queue.Paused == queue_pause::none,
			   "Save keeps the edit");
		expect(next_release(queue, queue_agent::idle) == first, "and the queue continues with it, never skipped");
		static_cast<void>(hold(queue, first, queue.Revision));
		expect(release_hold(queue, first, std::nullopt, queue.Revision) == queue_refusal::none && queue.Entries.front().Input.Text == "edited" && next_release(queue, queue_agent::idle) == first,
			   "Cancel keeps the text and continues");
		pause(queue, queue_pause::reviewer);
		static_cast<void>(hold(queue, first, queue.Revision));
		static_cast<void>(release_hold(queue, first, std::nullopt, queue.Revision));
		expect(queue.Paused == queue_pause::reviewer && !next_release(queue, queue_agent::idle), "a queue paused for another reason stays paused");
		expect(resume(queue, queue.Revision) == queue_refusal::none && next_release(queue, queue_agent::idle) == first, "until resumed");
		static_cast<void>(mark_sending(queue, first));
		settle(queue, first, queue_pause::usage_limit);
		expect(queue.Paused == queue_pause::usage_limit && !next_release(queue, queue_agent::idle), "a usage limit pauses what remains");
		expect(reorder(queue, queue.Entries.front().Id, -1, queue.Revision) == queue_refusal::none, "reorder at the edge changes nothing");
		static_cast<void>(enqueue(queue, ask("third"), queue.Revision));
		expect(reorder(queue, queue.Entries.back().Id, -1, queue.Revision) == queue_refusal::none && queue.Entries.front().Input.Label == "third", "reorder among the unsent");
		static_cast<void>(resume(queue, queue.Revision));
		const auto sending = *next_release(queue, queue_agent::idle);
		static_cast<void>(mark_sending(queue, sending));
		restore(queue);
		expect(queue.Paused == queue_pause::restarted && queue.Entries.front().State == queued_state::delivery_unknown && resume(queue, queue.Revision) == queue_refusal::not_editable,
			   "a restored queue opens paused; an interrupted send is uncertain and blocks resume");
	}
} // namespace

namespace {
	void test_return_unsent()
	{
		using namespace bpr::core;
		auto queue = agent_queue{ };
		static_cast<void>(enqueue(queue, { .Label = "a", .Text = "a" }, 0U));
		static_cast<void>(enqueue(queue, { .Label = "b", .Text = "b" }, queue.Revision));
		const auto a = queue.Entries.front().Id;
		static_cast<void>(mark_sending(queue, a));
		expect(return_unsent(queue, a) == queue_refusal::none && queue.Entries.front().Id == a && queue.Entries.front().State == queued_state::queued && queue.Paused == queue_pause::none,
			   "[[BPR-2181]] a message its turn never took is queued again first, the queue not paused (Q10)");
		expect(next_release(queue, queue_agent::idle) == a, "and goes next");
		expect(return_unsent(queue, a) == queue_refusal::not_editable, "only a sending message returns");
	}
} // namespace

namespace {
	/// RFC-BPR-0126 AC02 ([[BPR-2181]] slice 3): the core maps how a run
	/// ended to the queue's pause.
	void test_finish_maps_to_pause()
	{
		using namespace bpr::core;
		expect(pause_for(queued_finish::went) == queue_pause::none && pause_for(queued_finish::not_taken) == queue_pause::none && pause_for(queued_finish::failed) == queue_pause::failed
				   && pause_for(queued_finish::usage_limit) == queue_pause::usage_limit && pause_for(queued_finish::stopped) == queue_pause::stopped,
			   "each finish has its pause");
		const auto run = [](const queued_finish aFinish) {
			auto queue = agent_queue{ };
			static_cast<void>(enqueue(queue, { .Label = "a", .Text = "a" }, 0U));
			static_cast<void>(enqueue(queue, { .Label = "b", .Text = "b" }, queue.Revision));
			const auto a = queue.Entries.front().Id;
			static_cast<void>(mark_sending(queue, a));
			const auto refused = finish(queue, a, aFinish);
			return std::pair{ refused, queue };
		};
		const auto [failedRefusal, failed] = run(queued_finish::failed);
		expect(failedRefusal == queue_refusal::none && failed.Paused == queue_pause::failed && failed.Entries.size() == 1U && failed.Entries.front().Input.Label == "b" && failed.Entries.front().State == queued_state::queued,
			   "a failed run pauses the queue as failed, the second message still queued");
		expect(run(queued_finish::usage_limit).second.Paused == queue_pause::usage_limit, "a usage limit pauses as usage limit");
		expect(run(queued_finish::stopped).second.Paused == queue_pause::stopped, "Stop pauses as stopped");
		const auto [wentRefusal, went] = run(queued_finish::went);
		expect(wentRefusal == queue_refusal::none && went.Paused == queue_pause::none && went.Entries.size() == 1U, "a message that went leaves, unpaused");
		const auto [notTakenRefusal, notTaken] = run(queued_finish::not_taken);
		expect(notTakenRefusal == queue_refusal::none && notTaken.Paused == queue_pause::none && notTaken.Entries.size() == 2U && notTaken.Entries.front().State == queued_state::queued,
			   "a message not taken goes back first, unpaused");
		const auto [refusedRefusal, refused] = run(queued_finish::refused);
		expect(refusedRefusal == queue_refusal::none && refused.Paused == queue_pause::failed && refused.Entries.size() == 2U && refused.Entries.front().Input.Label == "a"
				   && refused.Entries.front().State == queued_state::queued,
			   "a refused release goes back first and pauses as failed (AC03)");

		auto queue = agent_queue{ };
		static_cast<void>(enqueue(queue, { .Label = "a", .Text = "a" }, 0U));
		const auto a = queue.Entries.front().Id;
		static_cast<void>(mark_sending(queue, a));
		pause(queue, queue_pause::lost_access);
		expect(finish(queue, a, queued_finish::stopped) == queue_refusal::none && queue.Paused == queue_pause::lost_access, "a pause in force keeps its reason");
		expect(finish(queue, a, queued_finish::went) == queue_refusal::unknown_entry, "a settled entry is not finished twice");
	}

	/// RFC-BPR-0126 AC06 ([[BPR-2181]]): a queue's session is typed; its key
	/// is the stable name of that type, read back as itself.
	void test_typed_session()
	{
		using namespace bpr::core;
		const auto tab = queued_session{ .Kind = queued_session_kind::agent_tab, .Host = "github.com", .Owner = "o", .Repository = "r", .Number = 7U };
		expect(session_key(tab) == "agent-tab:github.com/o/r#7" && parse_session_key(session_key(tab)) == std::optional{ tab }, "an Agent tab's session round-trips");
		const auto tour = with_kind(tab, queued_session_kind::tour_feedback);
		expect(session_key(tour) == "tour-feedback:github.com/o/r#7" && parse_session_key(session_key(tour)) == std::optional{ tour }, "the tour feedback session of the same pull request");
		expect(session_key(queued_session{ }).empty() && !parse_session_key("agent-tab:github.com/o#7") && !parse_session_key("agent-tab:github.com/o/r#x")
				   && !parse_session_key("other:github.com/o/r#7") && !parse_session_key("agent-tab:github.com/o/r#07"),
			   "anything else names no session");
		auto queue = agent_queue{ };
		expect(enqueue(queue, { .QuickAsk = "was-addressed", .Scope = "file", .Threads = { "t1", "t2" }, .Label = "Were these addressed?" }, 0U) == queue_refusal::none
				   && queue.Entries.front().Input.Threads.size() == 2U,
			   "an ask carries its threads typed");
	}

	/// RFC-BPR-0126 AC10 ([[BPR-2181]]): queue-versus-send is the core's,
	/// over the queue and the run; a Return in the gap after a release and
	/// before its turn shows is queued.
	void test_follow_up_decision()
	{
		using namespace bpr::core;
		expect(decide_follow_up({ }) == follow_up::send, "a free session sends");
		expect(decide_follow_up({ .TurnShown = true }) == follow_up::queue && decide_follow_up({ .TurnShown = true, .SteerByDefault = true }) == follow_up::steer
				   && decide_follow_up({ .TurnShown = true, .Opposite = true }) == follow_up::steer && decide_follow_up({ .TurnShown = true, .SteerByDefault = true, .Opposite = true }) == follow_up::queue,
			   "while a turn runs it is a follow-up, by the setting and the modifier");
		auto queue = agent_queue{ };
		static_cast<void>(enqueue(queue, { .Label = "a", .Text = "a" }, 0U));
		static_cast<void>(mark_sending(queue, queue.Entries.front().Id));
		expect(decide_follow_up({ .TurnShown = false, .QueueHolds = holds_messages(queue) }) == follow_up::queue, "released, its turn not shown yet: the Return is queued");
		expect(decide_follow_up({ .TurnShown = true, .Empty = true }) == follow_up::send, "nothing typed goes to the send, which refuses it");
	}

	/// RFC-BPR-0126 AC04 ([[BPR-2181]] slice 4): a message whose delivery is
	/// not known after a crash can be discarded.
	void test_discard_uncertain()
	{
		using namespace bpr::core;
		auto queue = agent_queue{ };
		static_cast<void>(enqueue(queue, { .Label = "a", .Text = "a" }, 0U));
		static_cast<void>(enqueue(queue, { .Label = "b", .Text = "b" }, queue.Revision));
		const auto a = queue.Entries.front().Id;
		const auto b = queue.Entries.back().Id;
		static_cast<void>(mark_sending(queue, a));
		restore(queue);
		expect(discard_uncertain(queue, b, queue.Revision) == queue_refusal::not_editable, "only an uncertain message is discarded this way");
		expect(discard_uncertain(queue, a, queue.Revision - 1U) == queue_refusal::stale, "from an older revision nothing changes");
		expect(discard_uncertain(queue, a, queue.Revision) == queue_refusal::none && queue.Entries.size() == 1U && queue.Entries.front().Id == b && queue.Paused == queue_pause::restarted,
			   "discarded, it leaves; the queue stays paused");
		expect(resume(queue, queue.Revision) == queue_refusal::none && next_release(queue, queue_agent::idle) == b, "and Resume goes on with the next");
	}

	/// [[BPR-2181]] (AC13). A queued suggested prompt keeps the offer decided
	/// at its press, as the store writes and reads it.
	void test_decided_prompt()
	{
		auto offer      = prompt_offer{ .Id = "own.look-again", .Label = "Ready to look again?", .Place = prompt_place::agent_tab, .Subject = prompt_subject::open_file, .Kind = prompt_ask::review };
		offer.Expansion = prompt_expansion{ .Text = "Has \xc3\xa9 {x} changed?\nYes;3:", .Cursor = 4U, .Values = { { .Begin = 4U, .End = 6U }, { .Begin = 8U, .End = 9U } } };
		const auto kept = decode_queued_prompt(encode_queued_prompt(offer));
		expect(kept && *kept == offer, "every part of the offer is read back as written");
		offer.Expansion.Cursor.reset();
		offer.Expansion.Values.clear();
		expect(decode_queued_prompt(encode_queued_prompt(offer)) == std::optional{ offer }, "no cursor and no values read back as none");
		const auto text = encode_queued_prompt(offer);
		expect(!decode_queued_prompt(text.substr(0U, text.size() - 1U)) && !decode_queued_prompt(text + "x") && !decode_queued_prompt("p2;" + text.substr(3U)) && !decode_queued_prompt(""),
			   "a cut, padded or other version's value is none");
		auto queue   = agent_queue{ };
		auto input   = ask("Ready to look again?");
		input.Prompt = offer;
		input.Prompt->Expansion.Text.assign(maximum_queued_text_bytes + 1U, 'a');
		expect(enqueue(queue, input, 0U) == queue_refusal::too_long, "an expansion past the text bound is refused");
		input.Prompt = offer;
		expect(enqueue(queue, input, 0U) == queue_refusal::none && queue.Entries.front().Input.Prompt == std::optional{ offer }, "queued with its decided offer");
	}
} // namespace

namespace {
	/// RFC-BPR-0126 AC06 ([[BPR-2152]]): the claim of the exact entry and
	/// revision, its receipt, and how a run's corrections settle.
	void test_claims_and_settled_corrections()
	{
		using namespace bpr::core;
		auto queue = agent_queue{ };
		static_cast<void>(enqueue(queue, { .Label = "a", .Text = "a" }, 0U));
		static_cast<void>(enqueue(queue, { .Label = "b", .Text = "b" }, queue.Revision));
		static_cast<void>(enqueue(queue, { .Label = "c", .Text = "c" }, queue.Revision));
		const auto a    = queue.Entries[0].Id;
		const auto b    = queue.Entries[1].Id;
		const auto c    = queue.Entries[2].Id;
		const auto seen = queue.Revision;
		expect(claim(queue, a, { .Route = queued_route::steer, .Run = 3U }, seen - 1U) == queue_refusal::stale, "a claim from an older revision is refused");
		static_cast<void>(hold(queue, b, queue.Revision));
		expect(claim(queue, b, { .Run = 3U }, queue.Revision) == queue_refusal::not_editable, "a held entry is never released");
		expect(claim(queue, b, { .Route = queued_route::steer, .Run = 3U, .Receipt = "forged" }, queue.Revision) == queue_refusal::none && queue.Entries[1].State == queued_state::sending
				   && !queue.Entries[1].Held && queue.Entries[1].Claim->Receipt.empty(),
			   "a Steer takes it from its editor; a claim carries no receipt until the runtime gives one");
		expect(claim(queue, b, { .Route = queued_route::steer, .Run = 3U }, queue.Revision) == queue_refusal::not_editable && withdraw(queue, b, queue.Revision) == queue_refusal::not_editable,
			   "a claimed entry is neither claimed again nor removed");
		expect(claim(queue, c, { .Route = queued_route::steer, .Run = 3U }, queue.Revision) == queue_refusal::none && claim(queue, a, { .Route = queued_route::steer, .Run = 3U }, queue.Revision) == queue_refusal::none,
			   "two more claimed for the same run");
		expect(next_release(queue, queue_agent::idle) == std::nullopt, "nothing is released while any is in flight");
		expect(acknowledge(queue, b, "rb") == queue_refusal::none && acknowledge(queue, b, "again") == queue_refusal::not_editable && queue.Entries[1].Claim->Receipt == "rb",
			   "one acknowledgment, kept on its own record");
		expect(acknowledge(queue, c, std::string(maximum_claim_receipt_bytes + 1U, 'x')) == queue_refusal::none && queue.Entries[2].Claim->Receipt.empty(), "an identity too long to keep is not kept");
		static_cast<void>(acknowledge(queue, a, "ra"));
		settle_steers(queue, 3U, { { .Receipt = "rb", .Ending = steered_ending::consumed }, { .Receipt = "ra", .Ending = steered_ending::not_taken } });
		expect(queue.Entries.size() == 2U && queue.Entries.back().Id == a && queue.Entries.back().State == queued_state::queued && !queue.Entries.back().Claim,
			   "consumed leaves; not taken is unsent again, first among the unsent, unbound");
		expect(queue.Entries.front().Id == c && queue.Entries.front().State == queued_state::delivery_unknown && queue.Paused == queue_pause::delivery_unknown,
			   "one its run never named stays uncertain and pauses the queue");

		auto released = agent_queue{ };
		static_cast<void>(enqueue(released, { .Label = "r", .Text = "r" }, 0U));
		const auto r = released.Entries.front().Id;
		expect(claim(released, r, { .Conversation = 9U }, released.Revision) == queue_refusal::none && bind_run(released, r, 4U) == queue_refusal::none && released.Entries.front().Claim->Run == 4U,
			   "a release's claim names the run it was admitted as");
		expect(acknowledge(released, r, "x") == queue_refusal::not_editable, "a release has no correction receipt");
		expect(finish(released, r, queued_finish::unknown) == queue_refusal::none && released.Entries.front().State == queued_state::delivery_unknown && released.Entries.front().Claim
				   && released.Paused == queue_pause::delivery_unknown,
			   "a release with no sign of delivery is kept uncertain, with its claim");

		auto crashed = agent_queue{ };
		static_cast<void>(enqueue(crashed, { .Label = "s", .Text = "s" }, 0U));
		const auto sId = crashed.Entries.front().Id;
		static_cast<void>(claim(crashed, sId, { .Route = queued_route::steer, .Run = 1U }, crashed.Revision));
		static_cast<void>(acknowledge(crashed, sId, "rs"));
		restore(crashed);
		expect(crashed.Entries.front().State == queued_state::delivery_unknown && crashed.Entries.front().Claim->Receipt == "rs", "a sent correction restores as uncertain, identifiable");
	}
} // namespace

auto main() -> int
{
	test_claims_and_settled_corrections();
	test_finish_maps_to_pause();
	test_follow_up_decision();
	test_typed_session();
	test_discard_uncertain();
	test_return_unsent();
	test_order_and_release();
	test_limits();
	test_hold_pause_and_restore();
	test_decided_prompt();
	std::cout << "agent-queue-tests:pass\n";
	return EXIT_SUCCESS;
}
