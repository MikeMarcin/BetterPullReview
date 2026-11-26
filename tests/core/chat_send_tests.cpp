// The one Send and Stop rule of every chat with an agent.
#include "bpr/core/chat_send.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {
	using namespace bpr::core;
	using face = chat_send_face;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void test_idle()
	{
		expect(decide_chat_send({ }) == chat_send_controls{ .Send = face::send, .StopBeside = false }, "an idle chat sends");
		expect(decide_chat_send({ .Typed = true }) == chat_send_controls{ .Send = face::send, .StopBeside = false }, "words typed in an idle chat start a run");
	}

	void test_running()
	{
		// Nothing typed could follow the turn: Send is Stop.
		expect(decide_chat_send({ .Running = true }) == chat_send_controls{ .Send = face::stop, .StopBeside = false }, "a running turn with an empty field: Send becomes Stop");
		expect(decide_chat_send({ .Running = true, .FollowsUp = true }) == chat_send_controls{ .Send = face::stop, .StopBeside = false }, "a chat that could follow up, with nothing typed: Send becomes Stop");
		// A chat that takes no follow-ups (an item's) stops whatever is typed.
		expect(decide_chat_send({ .Running = true, .Typed = true }) == chat_send_controls{ .Send = face::stop, .StopBeside = false }, "an item's chat: Send is Stop while its round runs, words typed or not");
		// Words that would follow the turn: Send says how, Stop stands beside.
		expect(decide_chat_send({ .Running = true, .FollowsUp = true, .Typed = true }) == chat_send_controls{ .Send = face::queue, .StopBeside = true }, "typed while it runs: Queue, with Stop beside");
		expect(decide_chat_send({ .Running = true, .FollowsUp = true, .Typed = true, .SteerFirst = true }) == chat_send_controls{ .Send = face::steer, .StopBeside = true }, "and Steer where the reviewer chose it");
	}

	void test_queue_without_a_turn()
	{
		// The queue holds messages while no turn shows yet: typed words wait
		// behind them, and there is nothing to stop.
		expect(decide_chat_send({ .FollowsUp = true, .Typed = true }) == chat_send_controls{ .Send = face::queue, .StopBeside = false }, "a queue that holds messages: Queue, and no Stop");
		expect(decide_chat_send({ .FollowsUp = true }) == chat_send_controls{ .Send = face::send, .StopBeside = false }, "and with nothing typed, Send");
	}
}

auto main() -> int
{
	test_idle();
	test_running();
	test_queue_without_a_turn();
	static_assert(decide_chat_send({ .Running = true }).Send == face::stop);
	std::cout << "chat-send-tests:pass\n";
	return EXIT_SUCCESS;
}
