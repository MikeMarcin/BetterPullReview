#include "bpr/application/agent_chat_session_host.hpp"
#include "bpr/core/agent_entry.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
namespace {
	using namespace bpr;
	void expect(bool aValue, const char* aMessage)
	{
		if (!aValue) {
			std::cerr << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	class projection final : public application::agent_chat_projection_port
	{
	public:
		std::optional<application::agent_chat_projection> Shown;
		void                                              present(const application::agent_chat_projection& aValue) override { Shown = aValue; }
		void                                              present(const application::agent_chat_outcome&) override { }
		void                                              clear() override { Shown.reset(); }
	};
}
int main()
{
	using namespace bpr;
	projection                           shown;
	application::agent_chat_session_host host{ { }, { }, core::agent_profile_id{ 1 }, { }, { }, shown };
	auto                                 binding = core::agent_revision_binding{ .Host = core::git_host_id{ 1 }, .Repository = core::repository_id{ 2 }, .PullRequest = core::pull_request_id{ 3 }, .BaseOid = *core::parse_git_commit_oid("1111111111111111111111111111111111111111"), .HeadOid = *core::parse_git_commit_oid("2222222222222222222222222222222222222222"), .LeftEpoch = core::epoch_ordinal{ 1 }, .RightEpoch = core::epoch_ordinal{ 2 } };
	host.observe(binding);
	expect(host.session().has_value(), "session observed");
	const auto session  = *host.session();
	auto       delivery = application::agent_finding_delivery{ .Session = session.Id, .Binding = session.Binding, .Findings = { { .Finding = { .Claim = "One reply claim" } }, { .Finding = { .Claim = "Second claim in the same reply" } } } };
	host.deliver(delivery);
	expect(shown.Shown && shown.Shown->Replies.size() == 1 && shown.Shown->Replies[0].Text == "One reply claim\n\nSecond claim in the same reply", "one accepted delivery presents claims only as one reply");
	const auto first = shown.Shown->Replies[0];
	for (std::size_t i = 0; i < application::maximum_visible_chat_replies; ++i) {
		host.deliver(delivery);
	}
	expect(shown.Shown->Replies.size() == application::maximum_visible_chat_replies && shown.Shown->Replies.front().Id > first.Id, "retention is bounded and identities never recycle");
	auto next = session;
	next.Id   = core::agent_session_id{ session.Id.value() + 1 };
	host.admitted(next);
	expect(shown.Shown->Replies.empty(), "new session retires old reply choices");
	host.deliver(delivery);
	expect(shown.Shown->Replies.empty(), "late old-session delivery cannot repopulate new session");
	delivery.Session         = next.Id;
	delivery.Binding.HeadOid = binding.BaseOid;
	host.deliver(delivery);
	expect(shown.Shown->Replies.empty(), "wrong binding is refused despite matching session identity");
	host.clear();
	expect(!shown.Shown, "retirement clears private replies");

	// [[BPR-1817]]. A send asks the agent the composer chose, and a refused
	// send carries the one reason the composer says.
	{
		auto turns    = std::vector<application::agent_chat_turn_request>{ };
		auto custom   = core::agent_profile{ .Id = core::entry_profile_id(core::agent_protocol::claude_stream_json, 1U), .DisplayName = "Fixture agent", .Executable = "/opt/agents/fixture", .Protocol = core::agent_protocol::claude_stream_json, .Workspace = core::agent_workspace_policy::disposable_review_workspace };
		auto codex    = core::agent_profile{ .Id = core::codex_jsonl_profile_id, .DisplayName = "Codex", .Executable = "codex", .Protocol = core::agent_protocol::codex_jsonl_exec, .Workspace = core::agent_workspace_policy::disposable_review_workspace };
		auto policies = std::vector{ core::built_in_agent_policy(core::git_host_id{ 2 }, core::built_in_agent_host_kind::explicit_ghes) };
		auto outcomes = std::vector<application::agent_chat_outcome>{ };
		class outcome_projection final : public application::agent_chat_projection_port
		{
		public:
			explicit outcome_projection(std::vector<application::agent_chat_outcome>& aOutcomes)
				: Outcomes{ aOutcomes }
			{ }
			std::optional<application::agent_chat_projection> Shown;
			void                                              present(const application::agent_chat_projection& aValue) override { Shown = aValue; }
			void                                              present(const application::agent_chat_outcome& aValue) override { Outcomes.push_back(aValue); }
			void                                              clear() override { Shown.reset(); }

		private:
			std::vector<application::agent_chat_outcome>& Outcomes;
		};
		outcome_projection                   port{ outcomes };
		application::agent_chat_session_host chat{ { codex, custom }, policies, core::codex_jsonl_profile_id, [&turns](const application::agent_chat_turn_request& aTurn) {
													  turns.push_back(aTurn);
													  return true;
												  },
												   { },
												   port };
		auto ghes = binding;
		ghes.Host = core::git_host_id{ 2 };
		chat.observe(ghes);
		const auto pr   = core::agent_chat_scope{ .Kind = core::agent_chat_scope_kind::pull_request };
		const auto send = [&](std::string aText) {
			return chat.send({ .Session = chat.session()->Id, .Action = core::agent_action_intent{ core::agent_entry_point::run_ai_review }, .Scope = pr, .Text = std::move(aText) });
		};
		// Without a choice the default profile is asked, and a server that
		// forbids it refuses the send by name rather than dropping it.
		const auto refused = send("What does this change?");
		expect(!refused.sent() && refused.Send == core::agent_send_refusal::agent_forbidden_on_host && turns.empty() && outcomes.size() == 1U,
			   "a forbidden default refuses the send with a named reason");
		expect(send("").Send == core::agent_send_refusal::empty_message, "nothing typed is named");
		// The custom agent chosen in the composer is the one asked.
		const auto started = chat.statistics().SessionsStarted;
		chat.choose_agent(custom.Id);
		expect(chat.session()->Profile == custom.Id && chat.session()->Binding == ghes && chat.statistics().SessionsStarted == started + 1U,
			   "choosing another agent starts its conversation at the revision under review");
		const auto sent = send("What does this change?");
		expect(sent.sent() && sent.Send == core::agent_send_refusal::none && turns.size() == 1U && turns.back().Session.Profile == custom.Id,
			   "the turn goes to the chosen custom agent");
		chat.choose_agent(custom.Id);
		expect(chat.statistics().SessionsStarted == started + 1U, "choosing the same agent again changes nothing");
		// An agent the host does not know is refused as unknown.
		chat.choose_agent(core::agent_profile_id{ 999U });
		expect(send("Again?").Send == core::agent_send_refusal::agent_unknown, "an agent the host does not have is named as unknown");
	}
	std::cout << "agent-chat-reply-tests:pass\n";
}
