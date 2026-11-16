/// \file
/// \brief RFC-BPR-0126 AC06 ([[BPR-2152]]). Queue and steering handoff
/// survive crash, write and save races without losing or automatically
/// duplicating input. Fault injection on the profile's session queue host
/// (no window), its store and the protocol decoder:
///
/// - the exact entry is claimed (Sending, bound to its run and conversation)
///   and stored before any byte of it is written; a crash after the claim
///   restores it as Delivery unknown, identifiable, never resent;
/// - a claim the store refused, or a Stop all while it was being stored,
///   leaves it unsent, first, and writes nothing;
/// - a correction's acknowledgment (its receipt) is stored with it; late,
///   duplicate or stray answers change only the record they name, once;
/// - a run's end settles each correction by its receipt before the queue
///   goes on: taken leaves, not taken goes back first with its identity,
///   written and unnamed stays Delivery unknown and pauses the queue;
/// - a release that failed with no sign the agent took it is Delivery
///   unknown; one that failed before any write is unsent;
/// - an intermediate or repeated result never completes a steered turn
///   early, and a dropped correction cannot hold the run open.

#include "bpr/application/agent_launch.hpp"
#include "bpr/application/agent_queue_host.hpp"
#include "bpr/application/review_agent_runtime.hpp"
#include "bpr/core/agent_steering.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
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

	/// A profile's store that a crash leaves as it was last written.
	struct memory_store final : public application::agent_queue_store_port
	{
		[[nodiscard]] auto load() -> std::optional<std::map<std::string, core::agent_queue>> override { return Kept; }
		[[nodiscard]] auto store(const std::string& aSession, const core::agent_queue& aQueue) -> bool override
		{
			++Writes;
			if (Refuses) {
				return false;
			}
			Kept[aSession] = aQueue;
			return true;
		}
		std::map<std::string, core::agent_queue> Kept;
		bool                                     Refuses = false;
		std::size_t                              Writes  = 0U;
	};

	/// The agent runtime behind the queue.
	class runner final : public application::agent_turn_runner_port
	{
	public:
		[[nodiscard]] auto state(const std::string& aSession) -> core::queue_agent override
		{
			const auto found = States.find(aSession);
			return found == States.end() ? core::queue_agent::idle : found->second;
		}
		[[nodiscard]] auto start(const std::string& aSession, const core::queued_entry& aEntry) -> application::queued_admission override
		{
			Ran.push_back(aEntry.Input.Text);
			// What the store held when the turn started.
			StoredAtStart.push_back(Store != nullptr && Store->Kept.contains(aSession) ? Store->Kept[aSession] : core::agent_queue{ });
			States[aSession] = core::queue_agent::busy;
			return { .Kind = application::queued_admission_kind::started, .Run = ++LastRun };
		}
		[[nodiscard]] auto steer(const std::string& aSession, const core::queued_entry& aEntry) -> std::optional<std::string> override
		{
			Steered.push_back(aEntry.Input.Text);
			StoredAtStart.push_back(Store != nullptr && Store->Kept.contains(aSession) ? Store->Kept[aSession] : core::agent_queue{ });
			return std::nullopt;
		}
		[[nodiscard]] auto bound(const std::string&) -> core::queued_claim override { return { .Run = Running, .Conversation = 77U }; }

		memory_store*                            Store = nullptr;
		std::map<std::string, core::queue_agent> States;
		std::vector<std::string>                 Ran;
		std::vector<std::string>                 Steered;
		std::vector<core::agent_queue>           StoredAtStart;
		std::uint64_t                            LastRun = 0U;
		std::uint64_t                            Running = 0U;
	};

	/// A serial lane, or the decision lane, that runs only when told.
	class held final : public application::effect_executor, public application::decision_executor
	{
	public:
		void post(std::function<void()> aTask) override { Tasks.push_back(std::move(aTask)); }
		void run()
		{
			auto tasks = std::move(Tasks);
			Tasks.clear();
			for (auto& task : tasks) {
				task();
			}
		}
		std::vector<std::function<void()>> Tasks;
	};

	struct profile
	{
		memory_store store;
		runner       agents;
		held         writes;
		held         decisions;

		[[nodiscard]] auto host() -> application::agent_queue_host
		{
			agents.Store = &store;
			return application::agent_queue_host{ { .Store = &store, .Runner = &agents, .Effects = &writes, .DecisionLane = &decisions } };
		}
		/// The store's write runs, then its verdict reaches the decision lane.
		void settle()
		{
			while (!writes.Tasks.empty() || !decisions.Tasks.empty()) {
				writes.run();
				decisions.run();
			}
		}
	};

	[[nodiscard]] auto typed(const std::string& aText) -> core::queued_input { return { .Label = aText, .Text = aText }; }

	[[nodiscard]] auto front(const application::agent_queue_host& aHost, const std::string& aSession) -> const core::queued_entry&
	{
		return aHost.queue(aSession)->Entries.front();
	}

	void test_a_release_is_claimed_and_stored_before_its_turn_starts()
	{
		auto world = profile{ };
		auto host  = world.host();
		static_cast<void>(host.enqueue("s", typed("one")));
		expect(world.agents.Ran.empty() && front(host, "s").State == core::queued_state::sending && front(host, "s").Claim && front(host, "s").Claim->Conversation == 77U,
			   "claimed and bound to its conversation; nothing is written while its claim is stored");
		static_cast<void>(host.enqueue("s", typed("two")));
		expect(world.agents.Ran.empty() && host.queue("s")->Entries.back().State == core::queued_state::queued, "the claimed entry is no longer eligible: the next waits");
		world.settle();
		expect(world.agents.Ran == std::vector<std::string>{ "one" }, "once stored, its turn starts, once");
		const auto& atStart = world.agents.StoredAtStart.front().Entries;
		expect(!atStart.empty() && atStart.front().State == core::queued_state::sending && atStart.front().Input.Text == "one", "the store held it as Sending before the turn started");
		expect(world.store.Kept["s"].Entries.front().Claim && world.store.Kept["s"].Entries.front().Claim->Run == 1U, "its receipt, the run it was admitted as, is stored");
		world.agents.States["s"] = core::queue_agent::idle;
		host.retired("s", core::queued_finish::went);
		world.settle();
		expect(world.agents.Ran == std::vector<std::string>{ "one", "two" } && world.agents.StoredAtStart.back().Entries.size() == 1U,
			   "the settled entry was stored gone before the next turn started");
		host.retired("s", core::queued_finish::went);
		host.retired("s", core::queued_finish::went);
		world.settle();
		expect(world.agents.Ran.size() == 2U && host.queue("s")->Entries.empty(), "a repeated retirement settles nothing twice and starts nothing");
	}

	void test_a_crash_after_the_claim_is_uncertain_and_never_resent()
	{
		auto world = profile{ };
		{
			auto host = world.host();
			static_cast<void>(host.enqueue("s", typed("claimed")));
			static_cast<void>(host.enqueue("s", typed("waits")));
			// The claim is stored; the app dies before its verdict comes back.
			world.writes.run();
			world.writes.run();
			world.decisions.Tasks.clear();
			expect(world.agents.Ran.empty(), "nothing was written to the agent");
		}
		auto host = world.host();
		host.load();
		host.agent_changed("s");
		world.settle();
		const auto& restored = front(host, "s");
		expect(restored.State == core::queued_state::delivery_unknown && restored.Claim && restored.Claim->Conversation == 77U && restored.Input.Text == "claimed",
			   "restored as Delivery unknown, with its text and what it was bound to");
		expect(host.queue("s")->Paused == core::queue_pause::restarted && world.agents.Ran.empty(), "the queue opens paused; nothing is sent");
		expect(host.resume("s") == core::queue_refusal::not_editable && world.agents.Ran.empty(), "Resume cannot pass an uncertain message: it is never resent");
		expect(host.discard("s", restored.Id) == core::queue_refusal::none, "only the reviewer discards it");
		expect(host.resume("s") == core::queue_refusal::none, "then the queue resumes");
		world.settle();
		expect(world.agents.Ran == std::vector<std::string>{ "waits" }, "and only the message that waited goes");
	}

	void test_a_refused_claim_or_a_stop_while_it_is_stored_writes_nothing()
	{
		auto world               = profile{ };
		auto host                = world.host();
		world.agents.States["s"] = core::queue_agent::busy;
		static_cast<void>(host.enqueue("s", typed("first")));
		static_cast<void>(host.enqueue("s", typed("second")));
		world.settle();
		world.store.Refuses      = true;
		world.agents.States["s"] = core::queue_agent::idle;
		host.agent_changed("s");
		world.settle();
		expect(world.agents.Ran.empty() && front(host, "s").Input.Text == "first" && front(host, "s").State == core::queued_state::queued && !front(host, "s").Claim,
			   "a claim the store refused writes nothing; it stays unsent, first");
		expect(host.queue("s")->Paused == core::queue_pause::failed, "and the queue pauses rather than retrying");
		world.store.Refuses = false;
		expect(host.resume("s") == core::queue_refusal::none, "Resume");
		host.pause_all(core::queue_pause::stopped);
		world.settle();
		expect(world.agents.Ran.empty() && front(host, "s").State == core::queued_state::queued && host.queue("s")->Paused == core::queue_pause::stopped,
			   "Stop all while the claim was stored: nothing starts, it is unsent and first");
		expect(host.resume("s") == core::queue_refusal::none, "Resume again");
		host.pause("s", core::queue_pause::reviewer);
		world.settle();
		expect(world.agents.Ran == std::vector<std::string>{ "first" } && host.queue("s")->Entries.back().State == core::queued_state::queued,
			   "Pause queue stops what comes after the message shown Sending, not that message");
	}

	void test_a_correction_is_stored_before_it_is_offered_and_settles_by_receipt()
	{
		auto world               = profile{ };
		auto host                = world.host();
		world.agents.States["s"] = core::queue_agent::busy;
		world.agents.Running     = 5U;
		for (const auto* const text : { "taken", "not taken", "unnamed", "waits" }) {
			static_cast<void>(host.enqueue("s", typed(text)));
		}
		world.settle();
		auto ids = std::vector<std::uint64_t>{ };
		for (const auto& entry : host.queue("s")->Entries) {
			ids.push_back(entry.Id);
		}
		auto told = std::vector<application::agent_queue_steer_result>{ };
		for (std::size_t index = 0U; index < 3U; ++index) {
			expect(host.steer("s", ids[index], std::nullopt, [&told](const application::agent_queue_steer_result& aResult) { told.push_back(aResult); }) == core::queue_refusal::none, "Steer pressed");
		}
		expect(world.agents.Steered.empty() && told.empty(), "nothing is offered to the turn while the claims are stored");
		expect(host.steer("s", ids[0]) == core::queue_refusal::not_editable, "a claimed correction cannot be steered twice");
		world.settle();
		expect(world.agents.Steered.size() == 3U && told.size() == 3U && told.front().Sent, "once stored, each is handed to the turn, once");
		expect(world.agents.StoredAtStart.front().Entries.front().State == core::queued_state::sending && world.agents.StoredAtStart.front().Entries.front().Claim->Route == core::queued_route::steer
				   && world.agents.StoredAtStart.front().Entries.front().Claim->Run == 5U,
			   "the store held it Sending, bound to the running turn's run, before it was offered");
		host.steer_answered("s", ids[0], true, "uuid-taken");
		host.steer_answered("s", ids[1], true, "uuid-not-taken");
		host.steer_answered("s", ids[2], true, "uuid-unnamed");
		host.steer_answered("s", ids[0], true, "uuid-other");
		world.settle();
		expect(world.store.Kept["s"].Entries[0].State == core::queued_state::sent && world.store.Kept["s"].Entries[0].Claim->Receipt == "uuid-taken", "the receipt is stored; a second answer changes nothing");

		// A crash now: written corrections are uncertain, identifiable by their receipts.
		{
			auto crashed = core::agent_queue{ world.store.Kept["s"] };
			core::restore(crashed);
			expect(crashed.Entries[0].State == core::queued_state::delivery_unknown && crashed.Entries[0].Claim->Receipt == "uuid-taken" && crashed.Paused == core::queue_pause::restarted,
				   "after a crash a sent correction is Delivery unknown with its receipt");
		}

		host.settle_steers("s", 4U, { { .Receipt = "uuid-taken", .Ending = core::steered_ending::consumed } });
		expect(host.queue("s")->Entries.size() == 4U, "another run's ending settles nothing of this one");
		host.settle_steers("s", 5U, { { .Receipt = "uuid-taken", .Ending = core::steered_ending::consumed }, { .Receipt = "uuid-not-taken", .Ending = core::steered_ending::not_taken } });
		const auto* queue       = host.queue("s");
		const auto  firstUnsent = std::ranges::find(queue->Entries, core::queued_state::queued, &core::queued_entry::State);
		expect(queue->Entries.size() == 3U && firstUnsent != queue->Entries.end() && firstUnsent->Id == ids[1], "taken leaves; not taken goes back first among the unsent, with its identity");
		const auto state_of = [&host](const std::uint64_t aId) { return std::ranges::find(host.queue("s")->Entries, aId, &core::queued_entry::Id)->State; };
		expect(state_of(ids[2]) == core::queued_state::delivery_unknown && queue->Paused == core::queue_pause::delivery_unknown,
			   "written and never named by the run's end: Delivery unknown, and the queue pauses");
		world.agents.States["s"] = core::queue_agent::idle;
		host.agent_changed("s");
		world.settle();
		expect(world.agents.Ran.empty(), "nothing goes on past the uncertain correction");
		host.steer_answered("s", ids[2], false, { });
		expect(state_of(ids[2]) == core::queued_state::delivery_unknown, "a late answer for a settled record changes nothing");
	}

	void test_an_unwritten_correction_goes_back_with_its_identity()
	{
		auto world               = profile{ };
		auto host                = world.host();
		world.agents.States["s"] = core::queue_agent::busy;
		static_cast<void>(host.enqueue("s", typed("a")));
		static_cast<void>(host.enqueue("s", typed("b")));
		world.settle();
		const auto b = host.queue("s")->Entries.back().Id;
		static_cast<void>(host.steer("s", b));
		world.settle();
		host.steer_answered("s", b, false, { });
		world.settle();
		expect(front(host, "s").Id == b && front(host, "s").State == core::queued_state::queued && host.queue("s")->Entries.size() == 2U && host.queue("s")->Paused == core::queue_pause::none,
			   "the turn ended before it was written: it goes next, the same message, never duplicated");
		host.steer_answered("s", b, true, "late");
		expect(front(host, "s").State == core::queued_state::queued, "a stray later answer changes nothing");
	}

	void test_a_failed_release_with_no_sign_is_uncertain()
	{
		auto result    = application::agent_run_result{ };
		result.Outcome = application::agent_run_outcome::failed_to_start;
		expect(application::queued_finish_of(result) == core::queued_finish::refused, "a run that never started wrote nothing: unsent");
		result.Outcome = application::agent_run_outcome::workspace_unavailable;
		expect(application::queued_finish_of(result) == core::queued_finish::refused, "nor did one with no workspace");
		result.Outcome = application::agent_run_outcome::nonzero_exit;
		expect(application::queued_finish_of(result) == core::queued_finish::unknown, "a process that died with no output may have read it: unknown");
		result.Outcome = application::agent_run_outcome::timeout;
		expect(application::queued_finish_of(result) == core::queued_finish::unknown, "so may one that timed out silent");
		result.AssistantItems = { "Partial answer." };
		expect(application::queued_finish_of(result) == core::queued_finish::failed, "one that answered took it: the run failed, the message went");
		result.AssistantItems.clear();
		result.Outcome         = application::agent_run_outcome::protocol_failure;
		result.ProtocolFailure = application::agent_protocol_failure::input_too_large;
		expect(application::queued_finish_of(result) == core::queued_finish::refused, "input refused before the write: unsent");

		auto world = profile{ };
		auto host  = world.host();
		static_cast<void>(host.enqueue("s", typed("lost in a crash of the agent")));
		static_cast<void>(host.enqueue("s", typed("next")));
		world.settle();
		world.agents.States["s"] = core::queue_agent::idle;
		host.retired("s", core::queued_finish::unknown);
		world.settle();
		expect(front(host, "s").State == core::queued_state::delivery_unknown && front(host, "s").Claim && host.queue("s")->Paused == core::queue_pause::delivery_unknown && world.agents.Ran.size() == 1U,
			   "kept as Delivery unknown with its claim; the queue pauses; nothing is resent");
	}

	[[nodiscard]] auto claude_result(const std::string_view aUuid, const std::string_view aInputs, const std::string_view aQueued) -> std::string
	{
		return std::string{ R"({"type":"result","subtype":"success","session_id":"steer-s","is_error":false,"result":"done","uuid":")" } + std::string{ aUuid } + R"(","user_message_uuids":[)" + std::string{ aInputs }
		+ R"(],"queued_turn_count":)" + std::string{ aQueued } + "}\n";
	}

	[[nodiscard]] auto running_claude(application::agent_protocol_decoder& aDecoder) -> std::string
	{
		const auto init = std::string{ R"({"type":"system","subtype":"init","session_id":"steer-s","model":"m","claude_code_version":"2.1.319"})" } + "\n";
		const auto tool = std::string{ R"({"type":"assistant","session_id":"steer-s","message":{"role":"assistant","id":"m1","content":[{"type":"tool_use","id":"u1","name":"Read","input":{"file_path":"a.cpp"}}]}})" } + "\n";
		expect(aDecoder.push(init + tool).has_value(), "a Claude turn at work");
		auto frame = aDecoder.steer("Look at b.cpp", true);
		expect(frame.has_value(), "a correction is written");
		return frame->Identity;
	}

	void test_intermediate_results_and_dropped_corrections_cannot_finish_early_or_hold_the_run()
	{
		auto       decoder  = application::agent_protocol_decoder{ core::agent_protocol::claude_stream_json };
		const auto identity = running_claude(decoder);
		const auto answer   = std::string{ R"({"type":"assistant","session_id":"steer-s","message":{"role":"assistant","id":"m2","content":[{"type":"text","text":"First."}]}})" } + "\n";
		expect(decoder.push(answer + claude_result("r1", "\"first\"", "1")).has_value(), "an intermediate result");
		expect(!decoder.turn_complete() && decoder.awaiting_successor(), "an intermediate result leaving the correction open does not finish the run");
		expect(!decoder.push(claude_result("r1", "\"first\"", "1")).has_value() || !decoder.turn_complete(), "a repeated result cannot finish it either");

		// The successor never comes (a hook dropped the correction).
		auto       dropped   = application::agent_protocol_decoder{ core::agent_protocol::claude_stream_json };
		const auto droppedId = running_claude(dropped);
		expect(dropped.push(answer + claude_result("r1", "\"first\"", "1")).has_value() && dropped.awaiting_successor(), "held for the successor");
		dropped.abandon_successor();
		expect(dropped.turn_complete() && !dropped.native_connection_reusable(), "past the bound the turn ends and its connection is retired, never reused");
		expect(dropped.steer_tally() == core::agent_steer_tally{ .Unknown = 1U } && dropped.steers().front().Identity == droppedId, "the correction is unknown, by its identity, and nothing is resent");

		// The successor came: it is the same run, and it settles the correction.
		auto       taken   = application::agent_protocol_decoder{ core::agent_protocol::claude_stream_json };
		const auto takenId = running_claude(taken);
		expect(taken.push(answer + claude_result("r1", "\"first\"", "1")).has_value(), "intermediate");
		const auto init = std::string{ R"({"type":"system","subtype":"init","session_id":"steer-s","model":"m","claude_code_version":"2.1.319"})" } + "\n";
		expect(taken.push(init).has_value() && !taken.awaiting_successor(), "the successor's first frame lifts the bound");
		taken.abandon_successor();
		expect(!taken.turn_complete(), "a successor under way is not abandoned");
		expect(taken.push(claude_result("r2", "\"" + takenId + "\"", "0")).has_value() && taken.turn_complete() && taken.steer_tally() == core::agent_steer_tally{ .Consumed = 1U },
			   "its result names the correction: consumed");
		static_cast<void>(identity);
	}

	void test_claims_survive_the_store_encoding()
	{
		const auto claim = core::queued_claim{ .Route = core::queued_route::steer, .Run = 18'446'744'073'709'551'615ULL, .Conversation = 41U, .Receipt = "0f8fad5b-d9cb-469f-a165-70867728950e" };
		expect(core::decode_queued_claim(core::encode_queued_claim(claim)) == claim, "a claim reads back as it was written");
		expect(!core::decode_queued_claim("c1;9;1;1;0;") && !core::decode_queued_claim("p1;0;1;1;0;") && !core::decode_queued_claim("c1;0;1;1;5;abc"), "a value it did not write reads as none");
	}
} // namespace

auto main() -> int
{
	test_a_release_is_claimed_and_stored_before_its_turn_starts();
	test_a_crash_after_the_claim_is_uncertain_and_never_resent();
	test_a_refused_claim_or_a_stop_while_it_is_stored_writes_nothing();
	test_a_correction_is_stored_before_it_is_offered_and_settles_by_receipt();
	test_an_unwritten_correction_goes_back_with_its_identity();
	test_a_failed_release_with_no_sign_is_uncertain();
	test_intermediate_results_and_dropped_corrections_cannot_finish_early_or_hold_the_run();
	test_claims_survive_the_store_encoding();
	std::cout << "agent-queue-handoff-tests:pass\n";
	return EXIT_SUCCESS;
}
