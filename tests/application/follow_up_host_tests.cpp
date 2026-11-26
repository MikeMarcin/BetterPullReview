/// \file
/// \brief [[BPR-2286]]. Ask follow-up's host: each round one run on the agent
/// lane, its answer settled on the decision lane by the core; follow-ups kept
/// by item; Stop, a failure and shutdown; nothing applied by the host.
/// [[BPR-2322]]: each question, proposal and Use is one `agent.side_chat`
/// line with lengths and outcomes, and none of the words.
/// [[BPR-2370]]: a launcher that answers later (the window's read-only run in
/// the pull request's workspace) starts each round on the decision lane, in
/// place of the runner; its answer, a refusal and a stop settle as the
/// runner's do.
#include "bpr/application/follow_up_host.hpp"
#include "bpr/core/operation_diagnosis.hpp"

#include <memory>

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <expected>
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

	/// The lines the host logged, as written.
	class recording_log final : public application::diagnostic_log_port
	{
	public:
		[[nodiscard]] auto record(const application::diagnostic_event& aEvent) noexcept -> application::diagnostic_log_status override
		{
			Events.push_back(aEvent);
			return application::diagnostic_log_status::available;
		}
		[[nodiscard]] auto                         status() const noexcept -> application::diagnostic_log_status override { return application::diagnostic_log_status::available; }
		std::vector<application::diagnostic_event> Events;

		/// The side chat lines that ended (not started), parsed.
		[[nodiscard]] auto ended() const -> std::vector<core::operation_diagnosis>
		{
			auto lines = std::vector<core::operation_diagnosis>{ };
			for (const auto& event : Events) {
				if (event.Operation == application::diagnostic_operation::side_chat && event.Outcome != application::diagnostic_outcome::started) {
					if (auto line = core::parse_operation_diagnosis(event.Summary.text())) {
						lines.push_back(*line);
					}
				}
			}
			return lines;
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
	const auto log       = std::make_shared<recording_log>();
	host.set_log(log);
	const auto agent = application::follow_up_agent{ .Agent = "fixture", .AgentName = "Fixture agent", .Profile = 1U, .Instructions = "Answer the reviewer." };

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
	{
		// [[BPR-2322]]. Asked: started when sent, then how it ended, with the
		// question's and the answer's lengths; a refused question says why.
		const auto lines = log->ended();
		expect(lines.size() == 2U && lines[0].Action == core::operation_action::side_chat_ask && lines[0].SideChat->Refusal == core::follow_up_refusal::busy && lines[0].Failure == core::operation_failure::refused,
			   "the busy question is logged as refused");
		const auto& trace = *lines[1].SideChat;
		expect(lines[1].Failure == core::operation_failure::none && trace.Item == core::follow_up_subject_kind::suggested_comment && trace.Round == 1U && trace.QuestionChars == 4U
				   && trace.AnswerChars == 40U && !trace.Apply,
			   "the answered question is logged with its round and lengths");
		const auto started = std::ranges::count_if(log->Events, [](const application::diagnostic_event& aEvent) { return aEvent.Operation == application::diagnostic_operation::side_chat && aEvent.Outcome == application::diagnostic_outcome::started; });
		expect(started == 1, "a question sent is logged as started");
	}

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
	{
		// [[BPR-2322]]. The proposal, the refused Use and the Use applied,
		// with before and after lengths.
		const auto lines = log->ended();
		expect(lines.size() == 5U, "a proposal and two Uses are logged");
		expect(lines[2].Action == core::operation_action::side_chat_propose && lines[2].SideChat->Apply == core::follow_up_apply::update_suggestion && lines[2].SideChat->BeforeChars == 27U
				   && lines[2].SideChat->AfterChars == 58U && lines[2].Failure == core::operation_failure::none,
			   "the proposal shown is logged with the lengths of what it replaces and what it proposes");
		expect(lines[3].Action == core::operation_action::side_chat_apply && lines[3].SideChat->Refusal == core::follow_up_refusal::target_changed && lines[3].Failure == core::operation_failure::refused,
			   "a refused Use is logged with why");
		expect(lines[4].Action == core::operation_action::side_chat_apply && lines[4].Failure == core::operation_failure::none && lines[4].SideChat->Apply == core::follow_up_apply::update_suggestion
				   && lines[4].SideChat->BeforeChars == 27U && lines[4].SideChat->AfterChars == 58U,
			   "the Use applied is logged with its before and after lengths");
	}

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
	{
		const auto lines = log->ended();
		expect(lines.size() == 7U && lines[5].Failure == core::operation_failure::operation_failed && lines[6].Failure == core::operation_failure::cancelled, "a failed and a stopped question are logged as such");
		// No words: not the question, the answer, the proposal, the item's
		// text, its title or its path.
		for (const auto& event : log->Events) {
			const auto text = std::string{ event.Summary.text() };
			for (const auto* const word : { "Why", "Because", "step()", "loop runs", "src/a.cpp", "And now", "Stop me", "timeout" }) {
				expect(text.find(word) == std::string::npos, "no line holds any of the words");
			}
		}
	}

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

	// [[BPR-2370]]. A launcher: each round starts at once, answers later.
	{
		held_lane lane;
		held_lane decided;
		auto      shown    = std::vector<application::follow_up_projection>{ };
		auto      launched = application::follow_up_host{ static_cast<application::effect_executor&>(lane), static_cast<application::decision_executor&>(decided),
														  [&shown](const application::follow_up_projection& aProjection) { shown.push_back(aProjection); } };
		auto      pending  = std::vector<application::follow_up_answer>{ };
		auto      tokens   = std::vector<std::stop_token>{ };
		auto      messages = std::vector<std::string>{ };
		auto      refuse   = false;
		// The round's Writing style and the skills its question named reach
		// the launcher with it.
		auto skilled         = agent;
		skilled.WritingStyle = "Write plainly.";
		skilled.Skills       = { core::supplement_skill{ .Name = "fixture-item-style", .Source = "fixture", .Origin = "fixture", .Text = "Be brief." } };
		launched.set_launcher([&](const application::follow_up_agent& aAgent, const std::string& aMessage, const std::stop_token aStop, application::follow_up_answer aAnswer) {
			expect(aAgent.Instructions == "Answer the reviewer." && aAgent.WritingStyle == "Write plainly." && aAgent.Skills == skilled.Skills, "the launcher is given the round's agent, its style and skills");
			messages.push_back(aMessage);
			if (refuse) {
				aAnswer(std::unexpected{ std::string{ "The side chat is still answering another item." } });
				return;
			}
			tokens.push_back(aStop);
			pending.push_back(std::move(aAnswer));
		});
		expect(launched.available(), "a launcher makes the side chat available");
		expect(launched.open(suggestion("finding:9")), "an item opens");
		expect(launched.ask(skilled, "Does it read the code?") == core::follow_up_refusal::none && lane.Tasks.empty() && pending.size() == 1U, "the round starts at once, not on the agent lane");
		expect(messages.back().starts_with(core::follow_up_marker) && !launched.is_quiescent() && shown.back().Open->Phase == core::follow_up_phase::asking, "the core's message goes, and the round is outstanding");
		pending.back()(std::string{ "Yes: renderer.cpp clamps only the width." });
		expect(!launched.is_quiescent(), "the answer settles on the decision lane");
		decided.drain();
		expect(launched.is_quiescent() && shown.back().Open->Rounds.size() == 1U && shown.back().Open->Rounds[0].Answer == "Yes: renderer.cpp clamps only the width.", "the answer is a round");

		// Stop: the token says so, and the late answer counts as stopped.
		static_cast<void>(launched.ask(skilled, "And the height?"));
		expect(pending.size() == 2U && !tokens.back().stop_requested(), "the second round starts");
		launched.stop();
		expect(tokens.back().stop_requested(), "Stop reaches the launched run");
		pending.back()(std::string{ "late" });
		decided.drain();
		expect(shown.back().RunFailure == "Stopped." && shown.back().Open->Rounds.size() == 1U && launched.is_quiescent(), "a stopped round adds nothing and says Stopped");

		// Refused before it starts: answered at once, said in words.
		refuse = true;
		static_cast<void>(launched.ask(skilled, "Again?"));
		decided.drain();
		expect(shown.back().RunFailure == "The side chat is still answering another item." && shown.back().Open->Phase == core::follow_up_phase::idle && launched.is_quiescent(), "a refused round says why and ends");

		// RFC-BPR-0130 stage 7. The item chat's History chose a session: its
		// rounds are the chat's, the failure goes, and the next round names
		// the session and what it asked, and carries none of the rounds.
		refuse     = false;
		auto named = std::vector<std::string>{ };
		launched.set_launcher([&](const application::follow_up_agent& aAgent, const std::string& aMessage, const std::stop_token aStop, application::follow_up_answer aAnswer) {
			named.push_back(aAgent.Session + "|" + aAgent.Asked);
			messages.push_back(aMessage);
			tokens.push_back(aStop);
			pending.push_back(std::move(aAnswer));
		});
		expect(launched.restore({ { .Question = "Is it read?", .Answer = "Yes." } }) == core::follow_up_refusal::none && shown.back().Open->Rounds.size() == 1U && shown.back().Open->InSession && shown.back().RunFailure.empty(),
			   "a chosen session's rounds are the chat's, and the last failure goes");
		auto inSession    = skilled;
		inSession.Session = "item:finding:9";
		static_cast<void>(launched.ask(inSession, "And the width?"));
		expect(named.back() == "item:finding:9|And the width?" && messages.back().find("Reviewer: Is it read?") == std::string::npos, "the round names its session and question, and carries no earlier round");
		expect(launched.restore({ }) == core::follow_up_refusal::busy, "no session is chosen while a round runs");
		pending.back()(std::string{ "Both." });
		decided.drain();
		static_cast<void>(launched.propose(inSession, core::follow_up_apply::update_suggestion, "issue: the loop runs twice."));
		expect(named.back() == "item:finding:9|Update suggested comment", "a proposal is asked as its action's words");
		pending.back()(std::string{ "issue: it runs once per branch." });
		decided.drain();
		expect(launched.restore({ }) == core::follow_up_refusal::none && shown.back().Open->Rounds.empty() && !shown.back().Open->Proposal, "New session empties the chat");

		// Shutdown: an answer after it is dropped.
		refuse = false;
		static_cast<void>(launched.ask(skilled, "Last?"));
		const auto count = shown.size();
		launched.shutdown();
		expect(tokens.back().stop_requested(), "shutdown stops the launched run");
		pending.back()(std::string{ "after" });
		decided.drain();
		expect(shown.size() == count && launched.is_quiescent(), "after shutdown nothing is published, and the host is quiescent");
	}
	std::cout << "follow-up-host-tests:pass\n";
	return EXIT_SUCCESS;
}
