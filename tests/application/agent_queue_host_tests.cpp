/// \file
/// \brief RFC-BPR-0126 AC04 ([[BPR-2151]]), Q10 as overridden by the owner
/// ("Queue must be tied to session not active UI windows"). The queue host
/// with two windows showing one session: both are shown every change and
/// edit the one queue, an edit made from an older revision is refused, a
/// message one window edits is not edited or removed from the other, the
/// next entry starts only when no window's turn runs, and closing a window
/// neither pauses the queue nor keeps it from draining in the other.

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

	/// One window: what it shows of the session, and the turns it ran.
	struct window
	{
		std::string                      session;
		core::queue_agent                agent = core::queue_agent::idle;
		std::vector<std::string>         ran;
		std::optional<core::agent_queue> shown;
		std::uint64_t                    token = 0U;

		[[nodiscard]] auto viewer() -> application::agent_queue_viewer
		{
			return {
				.Shows = [this](const std::string& aSession) { return aSession == session; },
				.Agent = [this](const std::string&) { return agent; },
				.Release = [this](const std::string&, const core::queued_entry& aEntry) {
					ran.push_back(aEntry.Input.Text);
					agent = core::queue_agent::busy;
					return true; },
				.Changed = [this](const std::string&, const core::agent_queue& aQueue) { shown = aQueue; },
			};
		}
	};

	[[nodiscard]] auto typed(const std::string& aText) -> core::queued_input { return { .Label = aText, .Text = aText }; }

	void test_two_windows_show_and_edit_one_queue()
	{
		auto       host    = application::agent_queue_host{ { } };
		const auto session = std::string{ "agent-tab:github.com/o/r#1" };
		auto       first   = window{ .session = session, .agent = core::queue_agent::busy };
		auto       second  = window{ .session = session };
		auto       other   = window{ .session = "agent-tab:github.com/o/r#2" };
		first.token        = host.attach(first.viewer());
		second.token       = host.attach(second.viewer());
		other.token        = host.attach(other.viewer());
		expect(host.enqueue(session, typed("one")) == core::queue_refusal::none && host.enqueue(session, typed("two")) == core::queue_refusal::none, "queued from the first window");
		expect(first.shown && second.shown && first.shown == second.shown && first.shown->Entries.size() == 2U && !other.shown,
			   "both windows showing the session show it; a window on another session does not");
		const auto seen = second.shown->Revision;
		const auto one  = second.shown->Entries.front().Id;
		const auto two  = second.shown->Entries.back().Id;
		expect(host.reorder(session, two, -1, first.shown->Revision) == core::queue_refusal::none && first.shown->Entries.front().Id == two && second.shown->Entries.front().Id == two,
			   "the first window moves a message; both show it");
		expect(host.withdraw(session, one, seen) == core::queue_refusal::stale && second.shown->Entries.size() == 2U, "an edit from what a window showed before is refused, and nothing changes");
		expect(host.hold(session, one, second.shown->Revision) == core::queue_refusal::none && first.shown->Entries.back().Held, "the second window edits a message; the first shows it held");
		expect(host.hold(session, one, first.shown->Revision) == core::queue_refusal::not_editable && host.withdraw(session, one, first.shown->Revision) == core::queue_refusal::not_editable,
			   "the first window cannot edit or remove it meanwhile");
		host.drain(session);
		expect(first.ran.empty() && second.ran.empty(), "a turn in the first window keeps the session busy: the second does not start one beside it");
		first.agent = core::queue_agent::idle;
		host.drain(session);
		expect(first.ran.size() == 1U && first.ran.front() == "two" && host.dispatcher(session) == first.token, "free, the next message starts in a free window");
		expect(host.release_hold(session, one, std::string{ "one, edited" }) == core::queue_refusal::none, "Save lifts the hold");
		// The first window closes while its turn runs: the message went, the
		// queue is not paused and is not handed to anyone.
		host.detach(first.token);
		expect(host.queue(session)->Paused == core::queue_pause::none && std::ranges::none_of(host.queue(session)->Entries, [](const core::queued_entry& aEntry) { return aEntry.Input.Text == "two"; }), "closing the window running the turn does not pause the queue");
		expect(second.ran.size() == 1U && second.ran.front() == "one, edited" && host.dispatcher(session) == second.token, "the other window showing the session goes on draining it");
		host.settle(session, core::queue_pause::none);
		expect(second.shown->Entries.empty(), "settled, it leaves the queue in every window");
	}

	void test_a_queue_no_window_shows_waits()
	{
		auto       host    = application::agent_queue_host{ { } };
		const auto session = std::string{ "agent-tab:github.com/o/r#1" };
		auto       only    = window{ .session = session };
		only.token         = host.attach(only.viewer());
		host.detach(only.token);
		expect(host.enqueue(session, typed("later")) == core::queue_refusal::none, "a session's queue is kept with no window on it");
		host.drain(session);
		expect(host.queue(session)->Paused == core::queue_pause::none && host.queue(session)->Entries.front().State == core::queued_state::queued, "it waits, unpaused");
		auto again  = window{ .session = session };
		again.token = host.attach(again.viewer());
		host.show(session);
		host.drain(session);
		expect(again.shown && again.ran.size() == 1U && again.ran.front() == "later", "a window that shows it later shows and drains it");
	}

	void test_a_refused_start_pauses()
	{
		auto       host    = application::agent_queue_host{ { } };
		const auto session = std::string{ "s" };
		auto       refuses = application::agent_queue_viewer{
			.Shows = [](const std::string&) { return true; },
			.Agent = [](const std::string&) { return core::queue_agent::idle; },
			.Release = [](const std::string&, const core::queued_entry&) { return false; },
			.Changed = { },
		};
		static_cast<void>(host.attach(std::move(refuses)));
		static_cast<void>(host.enqueue(session, typed("x")));
		host.drain(session);
		expect(host.queue(session)->Paused == core::queue_pause::failed && host.queue(session)->Entries.front().State == core::queued_state::queued && !host.dispatcher(session),
			   "a turn that did not start leaves its message unsent and the queue paused");
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

	/// RFC-BPR-0126 AC02: a restored queue opens paused and starts nothing,
	/// and a store that refuses a write leaves what was queued, as typed.
	void test_restored_and_refused()
	{
		auto store = memory_store{ };
		{
			auto host = application::agent_queue_host{ { .Store = &store } };
			static_cast<void>(host.enqueue("s", typed("kept as typed")));
			static_cast<void>(host.enqueue("s", typed("and this")));
		}
		auto host   = application::agent_queue_host{ { .Store = &store } };
		auto shows  = window{ .session = "s" };
		shows.token = host.attach(shows.viewer());
		host.load();
		host.show("s");
		host.drain("s");
		expect(host.queue("s")->Paused == core::queue_pause::restarted && shows.ran.empty() && shows.shown && shows.shown->Entries.size() == 2U,
			   "a restored queue is shown paused and starts nothing until resumed");
		store.Refuses = true;
		expect(host.enqueue("s", typed("third")) == core::queue_refusal::none && host.queue("s")->Entries.size() == 3U && host.queue("s")->Entries.front().Input.Text == "kept as typed",
			   "a refused write keeps the earlier messages and the new one as typed");
		for (auto index = host.queue("s")->Entries.size(); index < core::maximum_queued_inputs; ++index) {
			static_cast<void>(host.enqueue("s", typed("more")));
		}
		expect(host.enqueue("s", typed("one too many")) == core::queue_refusal::full && host.queue("s")->Entries.size() == core::maximum_queued_inputs && host.queue("s")->Entries.front().Input.Text == "kept as typed",
			   "past the limit nothing is added and nothing earlier changes");
	}
} // namespace

auto main() -> int
{
	test_two_windows_show_and_edit_one_queue();
	test_a_queue_no_window_shows_waits();
	test_a_refused_start_pauses();
	test_restored_and_refused();
	std::cout << "agent-queue-host-tests:pass\n";
	return EXIT_SUCCESS;
}
