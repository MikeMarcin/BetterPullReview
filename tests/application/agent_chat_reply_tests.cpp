#include "bpr/application/agent_chat_session_host.hpp"
#include <cstdlib>
#include <iostream>
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
	std::cout << "agent-chat-reply-tests:pass\n";
}
