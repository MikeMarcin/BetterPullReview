/// \file
/// \brief [[BPR-2281]]. Draft with agent's host: one run on the agent lane,
/// its answer read on the decision lane as a description, a failure or a
/// stop; a run's late answer is dropped; nothing runs twice at once.
#include "bpr/application/description_draft_host.hpp"

#include <cstdlib>
#include <deque>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			std::cerr << "description-draft-host-tests: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	/// A lane that keeps its tasks until the test runs them.
	struct held_lane final : application::effect_executor, application::decision_executor
	{
		std::deque<std::function<void()>> Tasks;
		void                              post(std::function<void()> aTask) override { Tasks.push_back(std::move(aTask)); }
		void                              drain()
		{
			while (!Tasks.empty()) {
				auto task = std::move(Tasks.front());
				Tasks.pop_front();
				task();
			}
		}
	};
} // namespace

auto main() -> int
{
	held_lane  agentLane;
	held_lane  decisions;
	auto       published = std::vector<application::description_draft_projection>{ };
	auto       host      = application::description_draft_host{ static_cast<application::effect_executor&>(agentLane), static_cast<application::decision_executor&>(decisions),
																[&published](const application::description_draft_projection& aProjection) { published.push_back(aProjection); } };
	const auto material  = core::description_draft_material{ .Title = "Clamp", .Body = "", .Files = { { .Path = "a.cpp", .Change = "modified", .Patch = std::string{ "+x" } } } };
	const auto agent     = application::description_draft_agent{ .Agent = "fixture", .AgentName = "Fixture agent", .Profile = 1U, .Instructions = "Write the description." };

	expect(!host.available() && !host.draft(agent, material), "with no runner nothing starts");

	auto asked  = std::vector<std::pair<std::string, std::string>>{ };
	auto answer = std::expected<std::string, std::string>{ "```markdown\n## Summary\n\nIt clamps.\n```" };
	host.set_runner([&](const application::description_draft_agent& aAgent, const std::string& aMessage, std::stop_token) {
		asked.emplace_back(aAgent.Instructions, aMessage);
		return answer;
	});
	expect(host.available(), "a runner makes it available");
	expect(host.draft(agent, material) && host.projection().Running && published.back().Running, "a draft starts and says it runs");
	expect(!host.draft(agent, material), "one draft at a time");
	agentLane.drain();
	expect(host.projection().Running, "the answer is read on the decision lane");
	decisions.drain();
	expect(asked.size() == 1U && asked[0].first == "Write the description." && asked[0].second == core::compose_description_draft(material), "the system prompt and the composed material");
	expect(!host.projection().Running && host.projection().Draft == std::optional<std::string>{ "## Summary\n\nIt clamps." } && host.projection().DraftRevision == 1U
			   && host.projection().Failure.empty(),
		   "the answer is the description, unwrapped");

	// A failure says why; an empty answer is a failure too.
	answer = std::unexpected{ std::string{ "The agent did not answer (timeout)." } };
	expect(host.draft(agent, material), "another draft starts");
	agentLane.drain();
	decisions.drain();
	expect(host.projection().Failure == "The agent did not answer (timeout)." && host.projection().DraftRevision == 1U, "a failed run keeps the last draft and says why");
	answer = std::string{ "  \n" };
	expect(host.draft(agent, material), "and another");
	agentLane.drain();
	decisions.drain();
	expect(host.projection().Failure.find("empty") != std::string::npos && host.projection().DraftRevision == 1U, "an empty answer drafts nothing");

	// Stop: the run's answer is not a draft.
	answer = std::string{ "## Summary\n\nLate." };
	expect(host.draft(agent, material), "a run to stop");
	host.stop();
	agentLane.drain();
	decisions.drain();
	expect(host.projection().Stopped && !host.projection().Running && host.projection().DraftRevision == 1U, "a stopped run drafts nothing");

	// A refusal reads as a failure.
	host.refuse("Choose an agent with the agent picker.");
	expect(host.projection().Failure == "Choose an agent with the agent picker." && !host.projection().Stopped, "a refusal says why");

	// Shutdown drops an answer still on its way.
	expect(host.draft(agent, material), "a last run");
	host.shutdown();
	agentLane.drain();
	decisions.drain();
	expect(host.projection().Running && host.is_quiescent(), "after shutdown an answer changes nothing, and the host is quiescent");
	std::cout << "description-draft-host-tests:pass\n";
	return EXIT_SUCCESS;
}
