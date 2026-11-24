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
#include "bpr/application/agent_session_runner.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <functional>
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
		[[nodiscard]] auto start(const std::string& aSession, const core::queued_entry& aEntry) -> application::queued_admission override
		{
			if (Refuses) {
				return { };
			}
			Ran.push_back(aSession + ":" + aEntry.Input.Text);
			// A slash command is answered with no run.
			if (aEntry.Input.Text.starts_with('/')) {
				return { .Kind = application::queued_admission_kind::answered, .Run = ++LastRun };
			}
			// A started turn keeps the session busy until its run retires.
			States[aSession] = core::queue_agent::busy;
			return { .Kind = Awaits ? application::queued_admission_kind::awaiting : application::queued_admission_kind::started, .Run = ++LastRun };
		}
		[[nodiscard]] auto steer(const std::string&, const core::queued_entry& aEntry) -> std::optional<std::string> override
		{
			Steered.push_back(aEntry.Input.Text);
			return SteerWhy;
		}
		/// The run retires: its own event, given to the host.
		void retire(application::agent_queue_host& aHost, const std::string& aSession, const core::queued_finish aOutcome = core::queued_finish::went)
		{
			States[aSession] = core::queue_agent::idle;
			aHost.retired(aSession, aOutcome);
		}

		std::map<std::string, core::queue_agent> States;
		std::vector<std::string>                 Ran;
		std::vector<std::string>                 Steered;
		std::optional<std::string>               SteerWhy;
		bool                                     Refuses = false;
		bool                                     Awaits  = false;
		std::uint64_t                            LastRun = 0U;
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
		agents.retire(host, session, core::queued_finish::usage_limit);
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
		const auto y      = host.queue(session)->Entries.back().Id;
		agents.SteerWhy   = "Steer isn't available for this agent yet.";
		auto       later  = std::optional<application::agent_queue_steer_result>{ };
		const auto stored = [&later](const application::agent_queue_steer_result& aResult) { later = aResult; };
		expect(host.steer(session, y, std::nullopt, stored) == core::queue_refusal::none, "the press is taken");
		expect(later && !later->Sent && later->Why == "Steer isn't available for this agent yet." && host.queue(session)->Entries.front().Id == y
				   && host.queue(session)->Entries.front().State == core::queued_state::queued && !host.queue(session)->Entries.front().Held,
			   "not taken: it goes next and says why");
		agents.SteerWhy.reset();
		later.reset();
		expect(host.steer(session, y, std::nullopt, stored) == core::queue_refusal::none && later && later->Sent && agents.Steered.size() == 2U, "handed to the turn");
		expect(host.queue(session)->Entries.front().State == core::queued_state::sending, "[[BPR-2152]] it stays, Sending, until the runtime answers");
		host.steer_answered(session, y, true, "steer-1");
		host.settle_steers(session, 0U, { { .Receipt = "steer-1", .Ending = core::steered_ending::consumed } });
		expect(host.queue(session)->Entries.size() == 1U, "taken: the run's end settles it and it leaves the queue");
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
		expect(host.enqueue("s", typed("third")) == core::queue_refusal::not_saved && host.queue("s")->Entries.size() == 2U && host.queue("s")->Entries.front().Input.Text == "kept as typed",
			   "a refused write is a refusal: the earlier messages stay and nothing was queued (AC05)");
		store.Refuses = false;
		expect(host.enqueue("s", typed("third")) == core::queue_refusal::none && host.queue("s")->Entries.size() == 3U, "kept, it is queued");
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

	/// A serial lane that runs only when told.
	class held_lane final : public application::effect_executor
	{
	public:
		void post(std::function<void()> aTask) override { Tasks.push_back(std::move(aTask)); }
		void run()
		{
			for (auto index = std::size_t{ 0U }; index < Tasks.size(); ++index) {
				Tasks[index]();
			}
			Tasks.clear();
		}
		std::vector<std::function<void()>> Tasks;
	};

	/// The decision lane a refused write's verdict comes back on.
	class held_decisions final : public application::decision_executor
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

	/// [[BPR-2181]] (AC05). With a lane, a queue's writes run on it, in the
	/// order the queue changed, and not where it changed.
	/// RFC-BPR-0126 AC04 ([[BPR-2181]] slice 4). A message that was sending
	/// when the app stopped is uncertain after the restart; Discard removes it
	/// (a core transition), the change is stored, and Resume goes on.
	void test_discard_an_uncertain_message_after_a_crash()
	{
		auto store  = memory_store{ };
		auto agents = runner{ };
		{
			auto host = application::agent_queue_host{ { .Store = &store, .Runner = &agents } };
			static_cast<void>(host.enqueue("s", typed("was sending")));
			agents.States["s"] = core::queue_agent::busy;
			static_cast<void>(host.enqueue("s", typed("waits")));
			expect(host.sending("s").has_value(), "the first message is sending when the app stops");
		}
		agents.States["s"] = core::queue_agent::idle;
		auto host          = application::agent_queue_host{ { .Store = &store, .Runner = &agents } };
		host.load();
		const auto* queue = host.queue("s");
		expect(queue->Paused == core::queue_pause::restarted && queue->Entries.front().State == core::queued_state::delivery_unknown && host.resume("s") == core::queue_refusal::not_editable,
			   "after the crash the message is uncertain and Resume is refused");
		const auto uncertain = queue->Entries.front().Id;
		expect(host.discard("s", queue->Entries.back().Id) == core::queue_refusal::not_editable, "a queued message is not discarded as uncertain");
		expect(host.discard("s", uncertain) == core::queue_refusal::none && host.queue("s")->Entries.size() == 1U && store.Kept["s"].Entries.size() == 1U, "Discard removes it, and the store keeps that");
		expect(host.resume("s") == core::queue_refusal::none && agents.Ran.size() == 2U && agents.Ran.back() == "s:waits", "Resume sends what was waiting");
	}

	void test_writes_run_on_the_effect_lane()
	{
		auto store         = memory_store{ };
		auto agents        = runner{ };
		auto lane          = held_lane{ };
		agents.States["s"] = core::queue_agent::busy;
		auto host          = application::agent_queue_host{ { .Store = &store, .Runner = &agents, .Effects = &lane } };
		static_cast<void>(host.enqueue("s", typed("first")));
		static_cast<void>(host.enqueue("s", typed("second")));
		expect(store.Kept.empty() && lane.Tasks.size() == 2U, "nothing is written where the queue changed");
		lane.run();
		expect(store.Kept.contains("s") && store.Kept["s"].Entries.size() == 2U && store.Kept["s"].Entries.back().Input.Text == "second", "the lane writes each change in order, the last as committed");
	}
	/// RFC-BPR-0126 AC05 ([[BPR-2181]]). The store refuses a write on the
	/// effect lane: the message that write queued leaves the queue and is
	/// handed back as asked, to return to the composer; a later write that
	/// changes nothing new hands nothing back.
	void test_a_refused_write_on_the_effect_lane_hands_the_message_back()
	{
		auto store  = memory_store{ };
		auto agents = runner{ };
		auto lane   = held_decisions{ };
		auto writes = held_lane{ };
		auto host   = application::agent_queue_host{ { .Store = &store, .Runner = &agents, .Effects = &writes, .DecisionLane = &lane } };
		auto shown  = window{ };
		auto back   = std::vector<std::string>{ };
		auto sub    = shown.subscriber();
		sub.Unsaved = [&back](const std::string& aSession, const core::queued_input& aInput) { back.push_back(aSession + ":" + aInput.Text); };
		static_cast<void>(host.subscribe(std::move(sub)));
		agents.States["s"] = core::queue_agent::busy;
		static_cast<void>(host.enqueue("s", typed("kept")));
		writes.run();
		lane.run();
		store.Refuses = true;
		expect(host.enqueue("s", typed("typed in the composer")) == core::queue_refusal::none && host.queue("s")->Entries.size() == 2U, "queued at once; the write runs on the lane");
		writes.run();
		lane.run();
		expect(back == std::vector<std::string>{ "s:typed in the composer" } && host.queue("s")->Entries.size() == 1U && host.queue("s")->Entries.front().Input.Text == "kept"
				   && shown.Shown["s"].Entries.size() == 1U,
			   "the refused message leaves the queue and comes back as typed");
		host.pause("s", core::queue_pause::reviewer);
		writes.run();
		lane.run();
		expect(back.size() == 1U, "a refused write that queued nothing hands nothing back");
	}

	/// RFC-BPR-0126 AC03 ([[BPR-2181]]). A release's admission settles it:
	/// one answered with no run (a slash command) went at once and the queue
	/// goes on; one refused goes back first, paused; one whose turn a later
	/// pass sends stays sending until that run retires. None is left sending.
	void test_admissions_settle_every_release()
	{
		auto agents        = runner{ };
		auto host          = application::agent_queue_host{ { .Runner = &agents } };
		agents.States["s"] = core::queue_agent::busy;
		static_cast<void>(host.enqueue("s", typed("/model opus")));
		static_cast<void>(host.enqueue("s", typed("after the command")));
		agents.States["s"] = core::queue_agent::idle;
		host.agent_changed("s");
		expect(agents.Ran == std::vector<std::string>{ "s:/model opus", "s:after the command" } && host.queue("s")->Entries.size() == 1U && host.sending("s"),
			   "a slash command is answered, leaves the queue, and the next goes");
		agents.retire(host, "s");
		expect(host.queue("s")->Entries.empty() && !host.sending("s"), "the next settles on its run's retirement");

		agents.Refuses = true;
		static_cast<void>(host.enqueue("s", typed("refused")));
		expect(!host.sending("s") && host.queue("s")->Paused == core::queue_pause::failed && host.queue("s")->Entries.front().State == core::queued_state::queued,
			   "a refused release is never left sending: it is queued again, paused");
		agents.Refuses = false;
		agents.Awaits  = true;
		expect(host.resume("s") == core::queue_refusal::none && host.sending("s"), "an awaiting release is sending");
		expect(host.resume("s") == core::queue_refusal::none && agents.Ran.size() == 3U, "a second Resume sends nothing more");
		agents.retire(host, "s", core::queued_finish::refused);
		expect(!host.sending("s") && host.queue("s")->Paused == core::queue_pause::failed && host.queue("s")->Entries.size() == 1U,
			   "its refusal settles it: back first, paused, never sending");
	}

	/// A released message whose turn began is shown off the pending list
	/// (`core::listed`), its claim kept until its run retires; only its own
	/// run's beginning counts, and one that began after it settled is nothing.
	void test_a_begun_turn_leaves_the_pending_list()
	{
		auto       agents  = runner{ };
		auto       host    = application::agent_queue_host{ { .Runner = &agents } };
		auto       shown   = window{ };
		const auto session = std::string{ "agent-tab:github.com/o/r#3" };
		static_cast<void>(host.subscribe(shown.subscriber()));
		const auto listed = [&] { return std::ranges::count_if(shown.Shown[session].Entries, [](const core::queued_entry& aEntry) { return core::listed(aEntry); }); };
		agents.Awaits     = true;
		static_cast<void>(host.enqueue(session, typed("review")));
		static_cast<void>(host.enqueue(session, typed("next")));
		expect(host.sending(session) && listed() == 2 && host.queue(session)->Entries.front().State == core::queued_state::sending, "released and awaiting its turn: listed as Sending");
		host.began(session, agents.LastRun + 1U);
		expect(listed() == 2, "another run's beginning is not this message's");
		host.began(session, agents.LastRun);
		expect(listed() == 1 && host.queue(session)->Entries.front().State == core::queued_state::sent && host.sending(session) && agents.Ran.size() == 1U,
			   "its turn began: off the list, still holding the session");
		agents.retire(host, session);
		expect(agents.Ran.size() == 2U && host.queue(session)->Entries.size() == 1U && listed() == 1, "its retirement settles it and the next goes");
		agents.retire(host, session);
		host.began(session, agents.LastRun);
		expect(host.queue(session)->Entries.empty() && !host.sending(session), "a beginning after it settled changes nothing");
	}
	/// [[BPR-2368]]. The tour feedback, tour part and local draft
	/// discussion queues run on window runtimes offered to the profile's
	/// session runner, whose start shows the message as the running turn at
	/// once: each reports its turn begun inside its own start, and the
	/// message leaves the pending list (as [[BPR-2368]]'s Agent tab release)
	/// while it still holds the session until its run retires.
	void test_a_window_runtime_turn_begun_inside_its_start_leaves_the_pending_list()
	{
		const auto pullRequest = core::queued_session{ .Host = "github.com", .Owner = "o", .Repository = "r", .Number = 9U };
		const auto stop        = core::tour_thread{ .Kind = core::tour_thread_kind::stop, .Stop = core::tour_stop_id{ 2U } };
		for (const auto& session : { core::session_key(core::with_kind(pullRequest, core::queued_session_kind::tour_feedback)), core::session_key(core::tour_part_session(pullRequest, core::tour_thread{ })),
									 core::session_key(core::tour_part_session(pullRequest, stop)), core::session_key(core::local_draft_session("draft-1", core::tour_thread{ })) }) {
			expect(!session.empty(), "the session is named");
			auto       store   = memory_store{ };
			auto       agents  = application::agent_session_runner{ };
			auto       host    = application::agent_queue_host{ { .Store = &store, .Runner = &agents } };
			auto       shown   = window{ };
			auto       busy    = false;
			auto       lastRun = std::uint64_t{ 0U };
			auto       started = std::vector<std::string>{ };
			auto       begins  = true;
			auto       token   = std::uint64_t{ 0U };
			const auto at      = session;
			agents.bind(host);
			static_cast<void>(host.subscribe(shown.subscriber()));
			token             = agents.offer(application::agent_session_runtime{
				.Runs  = [at](const std::string& aSession) { return aSession == at; },
				.State = [&busy](const std::string&) { return busy ? core::queue_agent::busy : core::queue_agent::idle; },
				.Start =
					[&](const std::string& aSession, const core::queued_entry& aEntry) {
						started.push_back(aEntry.Input.Text);
						busy           = true;
						const auto run = ++lastRun;
						// The chat shows it as its running turn before the start returns.
						if (begins) {
							agents.began(token, aSession, run);
						}
						return application::queued_admission{ .Kind = application::queued_admission_kind::started, .Run = run };
					},
			});
			const auto listed = [&] { return std::ranges::count_if(shown.Shown[session].Entries, [](const core::queued_entry& aEntry) { return core::listed(aEntry); }); };
			static_cast<void>(host.enqueue(session, typed("first")));
			static_cast<void>(host.enqueue(session, typed("second")));
			const auto& front = host.queue(session)->Entries.front();
			expect(started == std::vector<std::string>{ "first" } && host.sending(session) && front.State == core::queued_state::sent && front.Claim && front.Claim->Run == lastRun,
				   "its turn began inside its start: Sent, bound to its run, still holding the session");
			expect(listed() == 1 && shown.Shown[session].Entries.back().Input.Text == "second", "the pending list shows only what waits: " + session);
			expect(store.Kept[session].Entries.front().State == core::queued_state::sent && store.Kept[session].Entries.front().Claim->Run == lastRun, "the begun turn is stored");
			agents.began(token + 1U, session, lastRun);
			agents.began(token, session, lastRun + 1U);
			expect(listed() == 1 && host.queue(session)->Entries.size() == 2U, "another runtime's or run's beginning changes nothing");
			// Its retirement settles it; the next goes and begins.
			busy = false;
			agents.retired(token, session, lastRun, core::queued_finish::went);
			expect(started.size() == 2U && host.queue(session)->Entries.size() == 1U && listed() == 0 && host.sending(session), "the run's end settles it, and the next begins its turn");
			// A begun turn that fails goes as before: what remains pauses.
			static_cast<void>(host.enqueue(session, typed("third")));
			busy = false;
			agents.retired(token, session, lastRun, core::queued_finish::failed);
			expect(host.queue(session)->Paused == core::queue_pause::failed && host.queue(session)->Entries.size() == 1U && listed() == 1 && !host.sending(session),
				   "a begun turn that failed leaves, and what remains pauses, listed");
			// A run refused at once goes back first, listed, as before.
			begins = false;
			static_cast<void>(host.resume(session));
			expect(host.queue(session)->Entries.front().State == core::queued_state::sending && listed() == 1, "one that has not begun stays Sending, listed");
			busy = false;
			agents.retired(token, session, lastRun, core::queued_finish::refused);
			expect(host.queue(session)->Entries.front().State == core::queued_state::queued && host.queue(session)->Paused == core::queue_pause::failed && listed() == 1,
				   "a refused release goes back first, listed");
			// A crash while a begun turn ran: restored, it is uncertain, listed with Discard, never resent.
			begins = true;
			static_cast<void>(host.resume(session));
			expect(host.queue(session)->Entries.front().State == core::queued_state::sent && listed() == 0, "begun again");
			auto again    = application::agent_session_runner{ };
			auto restored = application::agent_queue_host{ { .Store = &store, .Runner = &again } };
			restored.load();
			expect(restored.queue(session) != nullptr && restored.queue(session)->Entries.front().State == core::queued_state::delivery_unknown && core::listed(restored.queue(session)->Entries.front())
					   && restored.queue(session)->Paused != core::queue_pause::none,
				   "after a crash a begun turn is Delivery unknown, listed, and the queue opens paused");
		}
	}

	/// [[BPR-2245]] (owner, 2026-11-17: "stop all is showing up in the status
	/// bar after that even though no agent stuff is running"). A release the
	/// agent refused returns its message and pauses the queue; nothing starts
	/// until Resume, so nothing waits to go and Stop all has nothing to stop.
	void test_a_paused_queue_leaves_nothing_waiting_to_go()
	{
		auto       agents  = runner{ };
		auto       host    = application::agent_queue_host{ { .Runner = &agents } };
		const auto session = std::string{ "agent-tab:github.com/o/r#1" };
		auto       shown   = window{ };
		static_cast<void>(host.subscribe(shown.subscriber()));
		agents.States[session] = core::queue_agent::busy;
		static_cast<void>(host.enqueue(session, typed("Review this pull request")));
		expect(shown.Waiting == 1U && host.waiting_count() == 1U, "a message queued behind a busy agent waits to go");
		agents.States[session] = core::queue_agent::idle;
		agents.Refuses         = true;
		host.agent_changed(session);
		expect(host.queue(session)->Paused == core::queue_pause::failed && host.queue(session)->Entries.size() == 1U, "a refused release returns the message and pauses the queue");
		expect(shown.Waiting == 0U && host.waiting_count() == 0U, "a paused queue's message does not wait to go: " + std::to_string(shown.Waiting));
		agents.Refuses         = false;
		agents.States[session] = core::queue_agent::busy;
		expect(host.resume(session, std::nullopt) == core::queue_refusal::none && shown.Waiting == 1U, "Resume makes it wait to go again");
		host.pause_all(core::queue_pause::stopped);
		expect(shown.Waiting == 0U, "Stop all pauses it, and nothing is left waiting to go");
	}

	/// [[BPR-2245]] (owner, 2026-11-17: "When I opened a different PR there was
	/// a review this pull request in the queue", and "Stop all still shows in
	/// the footer even after deleting that from the queue"). A window that
	/// opens another pull request is shown that session's queue even when it
	/// holds none, so it does not keep showing the last one's message; and a
	/// message deleted where it is queued leaves nothing waiting.
	void test_another_session_is_shown_its_own_empty_queue()
	{
		auto       agents = runner{ };
		auto       host   = application::agent_queue_host{ { .Runner = &agents } };
		const auto first  = std::string{ "agent-tab:github.com/o/r#1" };
		const auto second = std::string{ "agent-tab:github.com/o/r#2" };
		auto       shown  = window{ };
		static_cast<void>(host.subscribe(shown.subscriber()));
		agents.States[first] = core::queue_agent::busy;
		static_cast<void>(host.enqueue(first, typed("Review this pull request")));
		expect(shown.Shown[first].Entries.size() == 1U && shown.Waiting == 1U, "the first pull request's review waits");
		host.show(second);
		expect(shown.Shown.contains(second) && shown.Shown[second].Entries.empty() && host.queue(second) == nullptr, "another pull request with no queue is shown an empty one, and none is made for it");
		const auto id = shown.Shown[first].Entries.front().Id;
		expect(host.withdraw(second, id, std::nullopt) != core::queue_refusal::none && shown.Waiting == 1U, "a delete under another session removes nothing");
		expect(host.withdraw(first, id, std::nullopt) == core::queue_refusal::none && shown.Waiting == 0U && host.waiting_count() == 0U, "deleted where it is queued, nothing waits and Stop all has nothing to show for");
	}
} // namespace

auto main() -> int
{
	test_admissions_settle_every_release();
	test_a_begun_turn_leaves_the_pending_list();
	test_a_window_runtime_turn_begun_inside_its_start_leaves_the_pending_list();
	test_a_paused_queue_leaves_nothing_waiting_to_go();
	test_another_session_is_shown_its_own_empty_queue();
	test_a_refused_write_on_the_effect_lane_hands_the_message_back();
	test_no_window_drains_on_retirement();
	test_windows_show_and_edit_one_queue();
	test_steer();
	test_restored_refused_and_limits();
	test_discard_an_uncertain_message_after_a_crash();
	test_writes_run_on_the_effect_lane();
	std::cout << "agent-queue-host-tests:pass\n";
	return EXIT_SUCCESS;
}
