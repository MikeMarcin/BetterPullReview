// The History menu of a chat with an agent: when New session can be chosen,
// and what the menu holds for the Agent tab and for the tour's side chat.
#include "bpr/core/chat_history.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void test_new_session()
	{
		auto used       = chat_history_facts{ };
		used.SavedTurns = 2U;
		used.ShownTurns = 2U;
		expect(decide_new_session(used) == new_session_refusal::none, "a session with turns can be left for a new one");

		// Owner, 2026-11-24: "Right now you click it and it does nothing".
		expect(decide_new_session({ }) == new_session_refusal::already_new, "a session nothing was asked in is the new session");

		auto shown       = chat_history_facts{ };
		shown.ShownTurns = 1U;
		expect(decide_new_session(shown) == new_session_refusal::none, "a turn shown and not saved (it failed) is still left behind");
		auto uncertain      = chat_history_facts{ };
		uncertain.Uncertain = true;
		expect(decide_new_session(uncertain) == new_session_refusal::none, "a session whose question may have been sent can be left");

		auto unsupported            = chat_history_facts{ };
		unsupported.NativeSupported = false;
		expect(decide_new_session(unsupported) == new_session_refusal::none, "an agent that cannot continue natively gets a session it can use");
		unsupported.CarriedOver = true;
		expect(decide_new_session(unsupported) == new_session_refusal::already_new, "and that one, untouched, is the new session");

		// Owner, 2026-11-24, of the pinned-revision card's Start a new
		// session. A session pinned to another revision is never the new
		// one: New session opens one at the revision under review.
		auto pinned   = chat_history_facts{ };
		pinned.Pinned = true;
		expect(decide_new_session(pinned) == new_session_refusal::none, "a session pinned to another revision, asked in or not, can be left for one at the revision under review");
		unsupported.Pinned = true;
		expect(decide_new_session(unsupported) == new_session_refusal::none, "so can a carried-over one nothing was asked in");
		pinned.Loading = true;
		expect(decide_new_session(pinned) == new_session_refusal::loading, "but not while the sessions are still read");
		pinned.Loading = false;
		pinned.Running = true;
		expect(decide_new_session(pinned) == new_session_refusal::running, "nor while a turn runs");

		auto running    = used;
		running.Running = true;
		expect(decide_new_session(running) == new_session_refusal::running, "a running turn holds New session back");
		auto busy = used;
		busy.Busy = true;
		expect(decide_new_session(busy) == new_session_refusal::busy, "so does the agent at work for another part");
		auto loading    = used;
		loading.Loading = true;
		loading.Running = true;
		expect(decide_new_session(loading) == new_session_refusal::loading, "and the sessions still being read, said first");
		auto closed   = loading;
		closed.Closed = true;
		expect(decide_new_session(closed) == new_session_refusal::closed, "a chat whose part is gone takes no new session");
		auto none       = closed;
		none.AgentReady = false;
		expect(decide_new_session(none) == new_session_refusal::no_agent, "no agent is said before anything else");
	}

	void test_plan()
	{
		using item          = chat_history_item;
		auto agent          = chat_history_facts{ };
		agent.SavedTurns    = 1U;
		agent.ShownTurns    = 1U;
		agent.OtherChats    = 4U;
		agent.EveryRun      = true;
		agent.ResetAgents   = true;
		agent.RevisionKnown = true;
		const auto tab      = plan_chat_history(agent);
		expect(tab.Items
				   == std::vector{ item::new_session, item::separator, item::sessions, item::separator, item::carry_over, item::delete_all, item::separator, item::show_every_run, item::separator, item::reset_agents,
								   item::separator, item::revision_read },
			   "the Agent tab's History: New session first, the sessions, Delete all, Show every run, Reset agents, the revision read last");
		expect(tab.NewSession == new_session_refusal::none && tab.CarryOver && tab.Copy, "its actions can be chosen");

		auto tour       = agent;
		tour.Kind       = chat_history_kind::tour_side_chat;
		const auto side = plan_chat_history(tour);
		expect(side.Items
				   == std::vector{ item::new_session, item::separator, item::sessions, item::separator, item::carry_over, item::delete_all, item::separator, item::other_chats, item::copy_conversation, item::separator,
								   item::revision_read },
			   "the tour chat's History: the same, with the other parts' chats and without the Agent tab's own items");
		tour.OtherChats    = 0U;
		tour.RevisionKnown = false;
		const auto bare    = plan_chat_history(tour).Items;
		expect(std::ranges::find(bare, item::other_chats) == bare.end() && bare.back() == item::copy_conversation && bare.front() == item::new_session, "no other chats and no known revision: neither is listed");

		auto plain          = chat_history_facts{ };
		plain.SavedTurns    = 1U;
		const auto shortest = plan_chat_history(plain).Items;
		expect(shortest == std::vector{ item::new_session, item::separator, item::sessions, item::separator, item::carry_over, item::delete_all }, "a group with nothing in it leaves no separator behind");

		const auto empty = chat_history_facts{ };
		expect(!plan_chat_history(empty).Copy && plan_chat_history(empty).NewSession == new_session_refusal::already_new, "an empty chat has nothing to copy and is the new session");
		auto running    = agent;
		running.Running = true;
		expect(!plan_chat_history(running).CarryOver && plan_chat_history(running).Copy, "a running turn holds carry over back; the conversation can still be copied");

		expect(chat_header_tool_for(true) == chat_header_tool::history && chat_header_tool_for(false) == chat_header_tool::copy_conversation, "History where the chat keeps sessions, Copy where it keeps none");
	}
}

auto main() -> int
{
	test_new_session();
	test_plan();
	std::cout << "chat-history-tests:pass\n";
	return EXIT_SUCCESS;
}
