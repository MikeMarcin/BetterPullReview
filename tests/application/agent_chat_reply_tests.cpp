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
	expect(host.set_private_context({ .Session = next.Id, .Notes = true, .Drafts = true }).Refusal == application::agent_chat_refusal::none, "current private context is explicitly selected");
	host.reset_conversation();
	expect(host.projection() && host.projection()->PrivateContext.Notes && host.projection()->PrivateContext.Drafts, "New session retains currently selected context while clearing conversational replies");
	auto reopened = next;
	reopened.Id   = core::agent_session_id{ next.Id.value() + 1U };
	host.admitted(reopened, true);
	expect(host.projection()->PrivateContext.Session == reopened.Id && host.projection()->PrivateContext.Notes && host.projection()->Replies.empty(), "explicit history restore rebinds current context to exact restored session");
	host.clear();
	expect(!shown.Shown, "retirement clears private replies");

	// [[BPR-1817]]. A send asks the agent the composer chose, and a refused
	// send carries the one reason the composer says.
	{
		auto turns  = std::vector<application::agent_chat_turn_request>{ };
		auto custom = core::agent_profile{ .Id = core::entry_profile_id(core::agent_protocol::claude_stream_json, 1U), .DisplayName = "Fixture agent", .Executable = "/opt/agents/fixture", .Protocol = core::agent_protocol::claude_stream_json, .Workspace = core::agent_workspace_policy::disposable_review_workspace };
		auto codex  = core::agent_profile{ .Id = core::codex_jsonl_profile_id, .DisplayName = "Codex", .Executable = "codex", .Protocol = core::agent_protocol::codex_jsonl_exec, .Workspace = core::agent_workspace_policy::disposable_review_workspace };
		// A host that forbids Codex ([[BPR-2305]]: explicit GHES no longer does).
		auto policies = std::vector{ core::agent_host_policy{ .Host = core::git_host_id{ 2 }, .Allowed = { core::claude_stream_json_profile_id, custom.Id } } };
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
	// Continue at <old>, then a history reload nobody asked for (a file
	// opened): the choice stands and the next turn runs at the old revision.
	// History's pick or New session, another conversation, or the head
	// moving again starts at the revision under review, as before.
	{
		auto                                 turns = std::vector<application::agent_chat_turn_request>{ };
		auto                                 codex = core::agent_profile{ .Id = core::codex_jsonl_profile_id, .DisplayName = "Codex", .Executable = "codex", .Protocol = core::agent_protocol::codex_jsonl_exec, .Workspace = core::agent_workspace_policy::disposable_review_workspace };
		projection                           port;
		const auto                           policies = std::vector{ core::agent_host_policy{ .Host = binding.Host, .Allowed = { codex.Id } } };
		application::agent_chat_session_host chat{ { codex }, policies, codex.Id, [&turns](const application::agent_chat_turn_request& aTurn) {
													  turns.push_back(aTurn);
													  return true;
												  },
												   { },
												   port };
		auto moved               = binding;
		moved.HeadOid            = *core::parse_git_commit_oid("3333333333333333333333333333333333333333");
		const auto pr            = core::agent_chat_scope{ .Kind = core::agent_chat_scope_kind::pull_request };
		const auto continueAtOld = [&](std::string aConversation) {
			chat.clear();
			chat.observe(binding);
			chat.observe(moved);
			const auto old    = *chat.session();
			const auto chosen = chat.choose({ .Session = old.Id, .Choice = core::agent_continuation::continue_against_old_revision, .OfferedBinding = old.Binding, .Conversation = std::move(aConversation) });
			expect(chosen.Refusal == application::agent_chat_refusal::none && chat.session()->Binding == binding, "Continue at <old> keeps the session at the old revision");
		};
		continueAtOld("kept");
		expect(chat.keeps_continuation("kept") && !chat.keeps_continuation("other"), "the choice stands for the conversation it was made in only");
		expect(chat.reload_conversation("kept", false) == core::conversation_reload::keep_continuation, "a reload of the continued conversation keeps the choice");
		expect(chat.session()->Binding == binding && chat.projection() && !chat.projection()->Continuations.Required, "the reload neither moves the session nor asks again");
		const auto sent = chat.send({ .Session = chat.session()->Id, .Action = core::agent_action_intent{ core::agent_entry_point::run_ai_review }, .Scope = pr, .Text = "And now?" });
		expect(sent.sent() && turns.size() == 1U && turns.back().Session.Binding == binding, "the next turn runs at the revision the reviewer chose");

		continueAtOld("kept");
		expect(chat.reload_conversation("kept", true) == core::conversation_reload::start_at_observed && chat.session()->Binding == moved && !chat.keeps_continuation("kept"),
			   "History's pick or New session starts at the revision under review");
		continueAtOld("kept");
		expect(chat.reload_conversation("new", false) == core::conversation_reload::start_at_observed && chat.session()->Binding == moved,
			   "another conversation's history starts at the revision under review");
		continueAtOld("kept");
		auto again    = moved;
		again.HeadOid = *core::parse_git_commit_oid("4444444444444444444444444444444444444444");
		chat.observe(again);
		expect(!chat.keeps_continuation("kept") && chat.reload_conversation("kept", false) == core::conversation_reload::start_at_observed && chat.session()->Binding == again,
			   "a choice answered one head move and not the next");

		// A session still waiting for its answer: the head moved and nothing
		// was chosen, or a choice was asked again when the comparison moved.
		chat.clear();
		chat.observe(binding);
		expect(!chat.awaits_continuation(), "a session at the revision under review waits for nothing");
		chat.observe(moved);
		expect(chat.awaits_continuation(), "a session that read an earlier head waits for its answer");
		continueAtOld("kept");
		expect(!chat.awaits_continuation(), "answered, it waits no more");
		auto epochs       = moved;
		epochs.LeftEpoch  = core::epoch_ordinal{ binding.LeftEpoch.value() + 1 };
		epochs.RightEpoch = core::epoch_ordinal{ binding.RightEpoch.value() + 1 };
		chat.observe(epochs);
		expect(chat.awaits_continuation() && !chat.keeps_continuation("kept") && chat.session()->Binding == binding,
			   "a comparison that moved after the answer asks again, and the session still reads the revision it read");
		const auto past = *chat.session();
		expect(chat.choose({ .Session = past.Id, .Choice = core::agent_continuation::start_new_session, .OfferedBinding = past.Binding }).Refusal == application::agent_chat_refusal::none
				   && !chat.awaits_continuation(),
			   "New session answers it");
	}
	std::cout << "agent-chat-reply-tests:pass\n";
}
