/// \file
/// \brief RFC-BPR-0126 AC02 and AC04 ([[BPR-2151]], [[BPR-2181]]). The
/// session queue host owns every session's queue and drains it itself, with
/// no window: it starts the next message through the session's turn runner
/// only on the previous run's retirement event, and never on a timer. Windows
/// are subscribers and senders of commands: two of them see every change and
/// edit the one queue; an edit from an older revision, or on a message the
/// other is editing, is refused; a restored queue opens paused; a refused
/// write keeps what was queued; Steer sends or puts the message next.

#include "bpr/application/agent_queue_host.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <optional>
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

	/// The agent runtime behind the queue: no window, only what a session's
	/// agent can take and the turns it was given.
	class runner final : public application::agent_turn_runner_port
	{
	public:
		[[nodiscard]] auto state(const std::string& aSession) -> core::queue_agent override
		{
			const auto found = States.find(aSession);
			return found == States.end() ? core::queue_agent::idle : found->second;
		}
		[[nodiscard]] auto start(const std::string& aSession, const core::queued_entry& aEntry) -> bool override
		{
			if (Refuses) {
				return false;
			}
			Ran.push_back(aSession + ":" + aEntry.Input.Text);
			// A started turn keeps the session busy until its run retires.
			States[aSession] = core::queue_agent::busy;
			return true;
		}
		[[nodiscard]] auto steer(const std::string&, const core::queued_entry& aEntry) -> std::optional<std::string> override
		{
			Steered.push_back(aEntry.Input.Text);
			return SteerWhy;
		}
		/// The run retires: its own event, given to the host.
		void retire(application::agent_queue_host& aHost, const std::string& aSession, const core::queue_pause aOutcome = core::queue_pause::none)
		{
			States[aSession] = core::queue_agent::idle;
			aHost.retired(aSession, aOutcome);
		}

		std::map<std::string, core::queue_agent> States;
		std::vector<std::string>                 Ran;
		std::vector<std::string>                 Steered;
		std::optional<std::string>               SteerWhy;
		bool                                     Refuses = false;
	};

	/// A window: what it was shown.
	struct window
	{
		std::map<std::string, core::agent_queue> Shown;
		std::size_t                              Waiting = 0U;

		[[nodiscard]] auto subscriber() -> application::agent_queue_subscriber
		{
			return {
				.Changed = [this](const std::string& aSession, const core::agent_queue& aQueue) { Shown[aSession] = aQueue; },
				.Waiting = [this](const std::size_t aWaiting) { Waiting                                           = aWaiting; },
			};
		}
	};

	[[nodiscard]] auto typed(const std::string& aText) -> core::queued_input { return { .Label = aText, .Text = aText }; }

	void test_no_window_drains_on_retirement()
	{
		auto       agents  = runner{ };
		auto       host    = application::agent_queue_host{ { .Runner = &agents } };
		const auto session = std::string{ "agent-tab:github.com/o/r#1" };
		expect(host.enqueue(session, typed("one")) == core::queue_refusal::none, "queued with no window open");
		expect(agents.Ran == std::vector<std::string>{ session + ":one" }, "an idle session's message starts at once, with no window");
		static_cast<void>(host.enqueue(session, typed("two")));
		static_cast<void>(host.enqueue(session, typed("three")));
		host.agent_changed(session);
		expect(agents.Ran.size() == 1U, "nothing more starts while the turn's run has not retired");
		agents.retire(host, session);
		expect(agents.Ran.size() == 2U && agents.Ran.back() == session + ":two", "the run's retirement releases the next, by itself");
		agents.retire(host, session);
		agents.retire(host, session);
		expect(agents.Ran.size() == 3U && host.queue(session)->Entries.empty() && host.queue(session)->Paused == core::queue_pause::none, "the queue drains in order with no window");
		static_cast<void>(host.enqueue(session, typed("four")));
		static_cast<void>(host.enqueue(session, typed("five")));
		agents.retire(host, session, core::queue_pause::usage_limit);
		expect(host.queue(session)->Paused == core::queue_pause::usage_limit && agents.Ran.size() == 4U, "a run that retired on a usage limit pauses what remains");
	}

	void test_windows_show_and_edit_one_queue()
	{
		auto       agents  = runner{ };
		auto       host    = application::agent_queue_host{ { .Runner = &agents } };
		const auto session = std::string{ "agent-tab:github.com/o/r#1" };
		auto       first   = window{ };
		auto       second  = window{ };
		const auto one     = host.subscribe(first.subscriber());
		static_cast<void>(host.subscribe(second.subscriber()));
		agents.States[session] = core::queue_agent::busy;
		static_cast<void>(host.enqueue(session, typed("a")));
		static_cast<void>(host.enqueue(session, typed("b")));
		expect(first.Shown[session] == second.Shown[session] && first.Shown[session].Entries.size() == 2U && first.Waiting == 2U, "both windows are shown the one queue");
		const auto seen = second.Shown[session].Revision;
		const auto a    = second.Shown[session].Entries.front().Id;
		const auto b    = second.Shown[session].Entries.back().Id;
		expect(host.reorder(session, b, -1, first.Shown[session].Revision) == core::queue_refusal::none && second.Shown[session].Entries.front().Id == b, "one window moves a message; the other is shown it");
		expect(host.withdraw(session, a, seen) == core::queue_refusal::stale, "an edit from an older revision is refused");
		expect(host.hold(session, a, second.Shown[session].Revision) == core::queue_refusal::none && first.Shown[session].Entries.back().Held, "an edit holds it in both");
		expect(host.withdraw(session, a, first.Shown[session].Revision) == core::queue_refusal::not_editable && host.hold(session, a, first.Shown[session].Revision) == core::queue_refusal::not_editable,
			   "the other window cannot remove or edit it meanwhile");
		expect(host.release_hold(session, a, std::string{ "a, edited" }) == core::queue_refusal::none && first.Shown[session].Entries.back().Input.Text == "a, edited", "Save shows in both");
		host.unsubscribe(one);
		agents.retire(host, session);
		expect(agents.Ran.size() == 1U && agents.Ran.front() == session + ":b", "a window leaving changes nothing: the host goes on");
		host.pause_all(core::queue_pause::stopped);
		agents.retire(host, session);
		expect(second.Shown[session].Paused == core::queue_pause::stopped && agents.Ran.size() == 1U, "Stop all pauses every queue");
	}

	void test_steer()
	{
		auto       agents      = runner{ };
		auto       host        = application::agent_queue_host{ { .Runner = &agents } };
		const auto session     = std::string{ "s" };
		agents.States[session] = core::queue_agent::busy;
		static_cast<void>(host.enqueue(session, typed("x")));
		static_cast<void>(host.enqueue(session, typed("y")));
		const auto y     = host.queue(session)->Entries.back().Id;
		agents.SteerWhy  = "Steer isn't available for this agent yet.";
		const auto later = host.steer(session, y);
		expect(!later.Sent && later.Why == "Steer isn't available for this agent yet." && host.queue(session)->Entries.front().Id == y && !host.queue(session)->Entries.front().Held,
			   "not taken: it goes next and says why");
		agents.SteerWhy.reset();
		const auto sent = host.steer(session, y);
		expect(sent.Sent && host.queue(session)->Entries.size() == 1U && agents.Steered.size() == 2U, "taken: it leaves the queue");
	}

	/// A profile's store: what it holds, and whether it takes writes.
	class memory_store final : public application::agent_queue_store_port
	{
	public:
		[[nodiscard]] auto load() -> std::optional<std::map<std::string, core::agent_queue>> override { return Kept; }
		[[nodiscard]] auto store(const std::string& aSession, const core::agent_queue& aQueue) -> bool override
		{
			if (Refuses) {
				return false;
			}
			Kept[aSession] = aQueue;
			return true;
		}

		std::map<std::string, core::agent_queue> Kept;
		bool                                     Refuses = false;
	};

	void test_restored_refused_and_limits()
	{
		auto store         = memory_store{ };
		auto agents        = runner{ };
		agents.States["s"] = core::queue_agent::busy;
		{
			auto host = application::agent_queue_host{ { .Store = &store, .Runner = &agents } };
			static_cast<void>(host.enqueue("s", typed("kept as typed")));
			static_cast<void>(host.enqueue("s", typed("and this")));
		}
		agents.States["s"] = core::queue_agent::idle;
		auto host          = application::agent_queue_host{ { .Store = &store, .Runner = &agents } };
		host.load();
		host.agent_changed("s");
		expect(host.queue("s")->Paused == core::queue_pause::restarted && agents.Ran.empty(), "a restored queue opens paused and starts nothing");
		store.Refuses      = true;
		agents.States["s"] = core::queue_agent::busy;
		expect(host.enqueue("s", typed("third")) == core::queue_refusal::none && host.queue("s")->Entries.size() == 3U && host.queue("s")->Entries.front().Input.Text == "kept as typed",
			   "a refused write keeps the earlier messages and the new one as typed");
		for (auto index = host.queue("s")->Entries.size(); index < core::maximum_queued_inputs; ++index) {
			static_cast<void>(host.enqueue("s", typed("more")));
		}
		expect(host.enqueue("s", typed("one too many")) == core::queue_refusal::full && host.queue("s")->Entries.size() == core::maximum_queued_inputs, "past the limit nothing is added");
		for (std::size_t index = 1U; index < core::maximum_queued_sessions; ++index) {
			agents.States["other-" + std::to_string(index)] = core::queue_agent::busy;
			expect(host.enqueue("other-" + std::to_string(index), typed("waits")) == core::queue_refusal::none, "sessions up to the limit each hold a queue");
		}
		expect(host.enqueue("one-session-too-many", typed("refused")) == core::queue_refusal::full && host.queue("one-session-too-many") == nullptr,
			   "a session past the session limit starts no queue");
		expect(host.enqueue("other-1", typed("still taken")) == core::queue_refusal::none, "a session already holding messages still takes more");
		agents.Refuses     = true;
		agents.States["s"] = core::queue_agent::idle;
		expect(host.resume("s") == core::queue_refusal::none && host.queue("s")->Paused == core::queue_pause::failed && !host.sending("s"),
			   "a turn that did not start leaves its message unsent and pauses the queue");
	}
} // namespace

auto main() -> int
{
	test_no_window_drains_on_retirement();
	test_windows_show_and_edit_one_queue();
	test_steer();
	test_restored_refused_and_limits();
	std::cout << "agent-queue-host-tests:pass\n";
	return EXIT_SUCCESS;
}
