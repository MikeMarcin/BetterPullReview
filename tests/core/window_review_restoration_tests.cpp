#include "bpr/core/window_review_restoration.hpp"
#include <cstdlib>
#include <iostream>

namespace {
	using namespace bpr::core;
	void expect(bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			std::cerr << aMessage << '\n';
			std::exit(1);
		}
	}
	auto context(std::uint64_t aId, std::uint64_t aGeneration = 2) -> bpr_window_context { return { default_bpr_profile_id, bpr_window_id{ aId }, bpr_window_generation{ aGeneration } }; }
	auto destination(github_access_mode aMode = github_access_mode::authenticated, std::string aHost = "github.com") -> window_review_destination
	{
		return { aMode, std::move(aHost), { "owner", "repo" }, pull_request_number{ 27 } };
	}
	void test_selection_and_exact_readiness()
	{
		window_review_restoration policy{ default_bpr_profile_id, { context(2), context(1), context(3, 1) }, { "github.com", "enterprise.example" } };
		auto                      actions = policy.load({ { bpr_window_id{ 1 }, destination(github_access_mode::public_preview) }, { bpr_window_id{ 2 }, destination() }, { bpr_window_id{ 3 }, destination() } });
		expect(actions.Startup && actions.Startup->Authority->Mode == github_access_mode::public_preview, "last retained MRU selects one shared public authority; fresh window ignored");
		expect(actions.Failures.size() == 1 && actions.Failures[0].Context == context(2), "conflicting authority remains unavailable");
		expect(policy.mark_window_ready(context(1)).Opens.empty(), "readiness alone cannot authorize read before session");
		expect(policy.complete_startup(request_id{ 99 }, window_review_authority{ github_access_mode::public_preview, "github.com" }).Opens.empty(), "foreign startup terminal ignored");
		actions = policy.complete_startup(actions.Startup->Request, window_review_authority{ github_access_mode::public_preview, "github.com" });
		expect(actions.Opens.size() == 1 && actions.Opens[0].Context == context(1), "only exact ready matching destination opens");
		const auto request = actions.Opens[0];
		policy.cancel_restore(context(1));
		expect(!policy.current_restore(request.Context, request.Request) && policy.complete_restore(request.Context, request.Request, { }).Failures.empty(), "Home cancels exact late read terminal");
		expect(policy.records().size() == 3, "Home never deletes accepted destinations");
	}
	void test_home_does_not_cancel_sibling_and_missing_host_no_fallback()
	{
		window_review_restoration policy{ default_bpr_profile_id, { context(1), context(2) }, { "github.com" } };
		policy.cancel_restore(context(1));
		auto actions = policy.load({ { bpr_window_id{ 1 }, destination() }, { bpr_window_id{ 2 }, destination() } });
		expect(actions.Startup.has_value(), "one Home gesture does not suppress sibling authority selection");
		actions = policy.complete_startup(actions.Startup->Request, window_review_authority{ github_access_mode::authenticated, "github.com" });
		expect(actions.Opens.empty(), "accepted auth waits for window wiring");
		expect(policy.mark_window_ready(context(2)).Opens.size() == 1, "ready sibling restores after accepted session");
		window_review_restoration missing{ default_bpr_profile_id, { context(1) }, { "github.com" } };
		actions = missing.load({ { bpr_window_id{ 1 }, destination(github_access_mode::authenticated, "missing.example") } });
		expect(!actions.Startup && actions.Failures.size() == 1 && actions.Failures[0].Reason == window_review_failure::missing_server, "unconfigured saved host never fabricates normal sign-in fallback");
		window_review_restoration empty{ default_bpr_profile_id, { context(1) }, { "github.com" } };
		actions = empty.load({ });
		expect(actions.Startup && !actions.Startup->Authority, "no saved destination selects normal remembered startup explicitly");
	}
	void test_user_authority_and_persistence_scope()
	{
		window_review_restoration policy{ default_bpr_profile_id, { context(1) }, { "github.com" } };
		policy.cancel_startup();
		expect(!policy.load({ { bpr_window_id{ 1 }, destination() } }).Startup, "user authority activity suppresses delayed startup choice");
		auto chosen   = destination();
		chosen.Number = pull_request_number{ 42 };
		expect(policy.accepted_read(context(1), chosen), "trusted exact accepted read changes destination");
		expect(!policy.accepted_read(context(1, 3), destination()), "wrong generation cannot replace stored destination");
		expect(policy.close(context(1)) && policy.records().empty(), "exact close removes only its row");
		expect(!policy.attach(context(1)) && policy.attach(context(2, 1)), "retired identity cannot reattach; fresh monotonic identity accepted");
		auto invalid     = chosen;
		invalid.Hostname = "GitHub.com";
		expect(!valid(invalid), "noncanonical hostname rejected");
		expect(!valid_window_review_records({ { bpr_window_id{ 2 }, chosen }, { bpr_window_id{ 1 }, chosen } }), "unsorted storage records rejected");
	}
}
int main()
{
	test_selection_and_exact_readiness();
	test_home_does_not_cancel_sibling_and_missing_host_no_fallback();
	test_user_authority_and_persistence_scope();
	std::cout << "window-review-restoration-tests:pass\n";
}
