/// \file
/// \brief [[BPR-2286]]. Ask follow-up's host: each round one run on the agent
/// lane, its answer settled on the decision lane by the core; follow-ups kept
/// by item; Stop, a failure and shutdown; nothing applied by the host.
#include "bpr/application/follow_up_host.hpp"

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
			std::cerr << "follow-up-host-tests: " << aMessage << '\n';
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

	auto suggestion(const std::string& aKey) -> core::follow_up_subject
	{
		auto subject  = core::follow_up_subject{ };
		subject.Kind  = core::follow_up_subject_kind::suggested_comment;
		subject.Key   = aKey;
		subject.Path  = "src/a.cpp";
		subject.First = 3U;
		subject.Last  = 3U;
		subject.Title = "The loop runs twice.";
		subject.Text  = "issue: the loop runs twice.";
		return subject;
	}
} // namespace

auto main() -> int
{
	held_lane  agentLane;
	held_lane  decisions;
	auto       published = std::vector<application::follow_up_projection>{ };
	auto       host      = application::follow_up_host{ static_cast<application::effect_executor&>(agentLane), static_cast<application::decision_executor&>(decisions),
														[&published](const application::follow_up_projection& aProjection) { published.push_back(aProjection); } };
	const auto agent     = application::follow_up_agent{ .Agent = "fixture", .AgentName = "Fixture agent", .Profile = 1U, .Instructions = "Answer the reviewer." };

	expect(!host.available(), "with no runner nothing is available");
	expect(host.open(suggestion("finding:1")) && published.back().Open && published.back().Open->Subject.Key == "finding:1", "open shows the item's follow-up");
	expect(host.ask(agent, "Why?") == core::follow_up_refusal::failed && agentLane.Tasks.empty(), "with no runner nothing runs");

	auto asked  = std::vector<std::pair<std::string, std::string>>{ };
	auto answer = std::expected<std::string, std::string>{ "Because it is called from both branches." };
	host.set_runner([&](const application::follow_up_agent& aAgent, const std::string& aMessage, std::stop_token) {
		asked.emplace_back(aAgent.Instructions, aMessage);
		return answer;
	});
	expect(host.ask(agent, "Why?") == core::follow_up_refusal::none && published.back().Open->Phase == core::follow_up_phase::asking, "a question runs and says so");
	expect(host.ask(agent, "Again?") == core::follow_up_refusal::busy, "one run at a time in a follow-up");
	agentLane.drain();
	expect(!host.is_quiescent(), "the answer is still on its way to the decision lane");
	decisions.drain();
	expect(host.is_quiescent() && asked.size() == 1U && asked[0].first == "Answer the reviewer." && asked[0].second.starts_with(core::follow_up_marker), "the instructions and the core's message went to the agent");
	expect(published.back().Open->Rounds.size() == 1U && published.back().Open->Rounds[0].Answer == "Because it is called from both branches.", "the answer is a round");

	// Another item: its own follow-up; the first is kept for its key.
	expect(host.open(suggestion("finding:2")) && published.back().Open->Rounds.empty(), "another item opens its own follow-up");
	expect(host.open(suggestion("finding:1")) && published.back().Open->Rounds.size() == 1U, "the first comes back with its rounds");

	// A proposal: shown, not applied, until Use.
	answer = std::expected<std::string, std::string>{ "issue: `step()` runs twice; call it once after the branch." };
	expect(host.propose(agent, core::follow_up_apply::update_suggestion, "issue: the loop runs twice.") == core::follow_up_refusal::none, "Update asks for a proposal");
	agentLane.drain();
	decisions.drain();
	const auto proposal = published.back().Open->Proposal;
	expect(proposal && proposal->Text == "issue: `step()` runs twice; call it once after the branch.", "the proposal is shown");
	expect(host.held("finding:1")->Subject.Text == "issue: the loop runs twice.", "nothing replaced before Use");
	expect(host.apply(proposal->Id, "changed meanwhile").Refusal == core::follow_up_refusal::target_changed, "a text changed since is refused");
	const auto used = host.apply(proposal->Id, "issue: the loop runs twice.");
	expect(used.Refusal == core::follow_up_refusal::none && used.Text == proposal->Text && used.Key == "finding:1" && !published.back().Open->Proposal, "Use returns the decision; the window puts it in place");

	// A failure and a stop say why; a stopped run's late answer is dropped.
	answer = std::unexpected{ std::string{ "The agent did not answer (timeout)." } };
	static_cast<void>(host.ask(agent, "And now?"));
	agentLane.drain();
	decisions.drain();
	expect(published.back().RunFailure == "The agent did not answer (timeout)." && published.back().Open->Refused == core::follow_up_refusal::failed, "a failure is said");
	answer = std::expected<std::string, std::string>{ "late" };
	static_cast<void>(host.ask(agent, "Stop me"));
	host.stop();
	agentLane.drain();
	decisions.drain();
	expect(published.back().RunFailure == "Stopped." && published.back().Open->Rounds.size() == 1U && published.back().Open->Phase == core::follow_up_phase::idle, "Stop ends the run, adding no round");

	// Closed, nothing is shown; shutdown drops whatever comes back.
	host.close();
	expect(!published.back().Open && host.held("finding:1") != nullptr, "close hides it and keeps it");
	expect(host.open(suggestion("finding:1")), "open again");
	static_cast<void>(host.ask(agent, "Last"));
	const auto before = published.size();
	host.shutdown();
	agentLane.drain();
	decisions.drain();
	expect(published.size() == before && host.is_quiescent(), "after shutdown nothing is published and the host is quiescent");
	auto bad = suggestion("");
	expect(!host.open(bad), "after shutdown, and for an invalid item, nothing opens");
	std::cout << "follow-up-host-tests:pass\n";
	return EXIT_SUCCESS;
}
