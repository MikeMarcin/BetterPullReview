#include "bpr/core/epoch_transition.hpp"
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
	template <typename T>
	auto identity(std::uint8_t aByte) -> T
	{
		std::array<std::uint8_t, 32> bytes{ };
		bytes[0] = aByte;
		return T{ bytes };
	}
	auto position() -> window_review_position
	{
		const auto scope       = identity<pull_request_authority_scope_id>(1);
		const auto incarnation = identity<epoch_catalog_incarnation_id>(2);
		return { .Comparison = {
					 .Left  = { { scope, incarnation, comparison_anchor_kind::epoch_base, epoch_ordinal{ 1 }, { } }, *parse_git_commit_oid("1111111111111111111111111111111111111111"), anchor_availability::available },
					 .Right = { { scope, incarnation, comparison_anchor_kind::epoch_head, epoch_ordinal{ 1 }, { } }, *parse_git_commit_oid("2222222222222222222222222222222222222222"), anchor_availability::available } },
				 .File      = identity<file_identity>(3),
				 .Selection = { } };
	}
	void test_reading_identity_and_replacement()
	{
		window_review_restoration policy{ default_bpr_profile_id, { context(1), context(2) }, { "github.com" } };
		const auto                saved = position();
		expect(valid(saved), "exact same-catalog position is valid");
		expect(!policy.accepted_position(context(1), destination(), saved), "position cannot invent a PR destination");
		expect(policy.accepted_read(context(1), destination()) && policy.accepted_position(context(1), destination(), saved), "accepted position attaches to exact destination");
		expect(!policy.accepted_position(context(1, 3), destination(), saved), "stale generation cannot save position");
		expect(!policy.accepted_read(context(1), destination()) && policy.records()[0].Position == saved, "same PR refresh preserves reading identity");
		auto other   = destination();
		other.Number = pull_request_number{ 28 };
		expect(!policy.accepted_position(context(1), other, saved), "different destination cannot save position");
		expect(policy.accepted_read(context(1), other) && !policy.records()[0].Position, "accepted replacement clears old reading identity");
		const auto actions = policy.load({ { bpr_window_id{ 2 }, destination(), saved } });
		expect(actions.Startup.has_value(), "retained position participates in existing startup");
		static_cast<void>(policy.mark_window_ready(context(2)));
		const auto opened = policy.complete_startup(actions.Startup->Request, window_review_authority{ github_access_mode::authenticated, "github.com" });
		expect(opened.Opens.size() == 1 && opened.Opens[0].Position == saved, "exact complete payload carried to matching ready window");
		auto invalid                                = saved;
		invalid.Comparison.Right.Identity.Authority = identity<pull_request_authority_scope_id>(9);
		expect(!valid(invalid), "cross-authority pair refused");
		invalid                                       = saved;
		invalid.Comparison.Right.Identity.Incarnation = identity<epoch_catalog_incarnation_id>(9);
		expect(!valid(invalid), "cross-incarnation pair refused");
		invalid           = saved;
		invalid.Selection = semantic_selection{ };
		expect(!valid(invalid), "invalid semantic digest refused");
	}
	void test_reading_course_refuses_substitution()
	{
		const auto                 saved = position();
		auto                       epoch = epoch_state{ };
		window_reading_restoration course{ saved };
		using enum window_reading_restore_action;
		expect(course.advance(epoch, { }) == wait, "reading waits for catalog");
		epoch.Authority        = pull_request_authority{ };
		epoch.Authority->Scope = saved.Comparison.Left.Identity.Authority;
		epoch.Catalog          = epoch_catalog_identity{ saved.Comparison.Left.Identity.Incarnation, epoch_catalog_revision{ 1 } };
		epoch.Status           = epoch_status::ready;
		expect(course.advance(epoch, { }) == select_comparison, "exact explicit comparison requested once");
		epoch.AcceptedComparison = saved.Comparison;
		expect(course.advance(epoch, { }) == wait, "accepted comparison waits for exact file observation");
		expect(course.advance(epoch, { .FilesReady = true, .FileAvailable = true }) == select_file, "accepted file selected once");
		expect(course.advance(epoch, { }) == wait, "file resolution alone is not computed diff evidence");
		expect(course.advance(epoch, { .DiffReady = true }) == complete, "computed exact file completes position without semantic anchor");
		window_reading_restoration wrongPair{ saved };
		static_cast<void>(wrongPair.advance(epoch, { }));
		std::swap(epoch.AcceptedComparison->Left, epoch.AcceptedComparison->Right);
		expect(wrongPair.advance(epoch, { }) == unavailable, "reversed or fallback comparison never substitutes");
		epoch.AcceptedComparison = saved.Comparison;
		window_reading_restoration missingFile{ saved };
		static_cast<void>(missingFile.advance(epoch, { }));
		expect(missingFile.advance(epoch, { .FilesReady = true }) == unavailable, "removed file never becomes neighboring file");
		window_reading_restoration wrongCatalog{ saved };
		epoch.Catalog->Incarnation = identity<epoch_catalog_incarnation_id>(8);
		expect(wrongCatalog.advance(epoch, { }) == unavailable, "recreated catalog does not reuse epoch ordinal");
	}
	void test_exact_comparison_cancellation()
	{
		auto epoch               = epoch_state{ };
		epoch.PendingComparison  = pending_comparison_resolution{ epoch_request_id{ 7 }, comparison_generation{ 3 } };
		epoch.Status             = epoch_status::resolving;
		const auto saved         = position();
		epoch.RequestedSelection = explicit_selection(saved.Comparison.Left.Identity, saved.Comparison.Right.Identity);
		static_cast<void>(transition(epoch, cancel_comparison_resolution_intent{ epoch_request_id{ 6 }, comparison_generation{ 3 } }));
		expect(epoch.PendingComparison.has_value(), "foreign cancellation cannot retire newer comparison");
		const auto retired = transition(epoch, cancel_comparison_resolution_intent{ epoch_request_id{ 7 }, comparison_generation{ 3 } });
		expect(!epoch.PendingComparison && retired.Requests.size() == 1 && std::holds_alternative<cancel_epoch_work_request>(retired.Requests[0]) && epoch.Status == epoch_status::cancelled,
			   "exact cancellation retires identity before advisory host work cancellation");
		expect(epoch.RequestedSelection == named_selection(named_comparison::whole_pr) && !epoch.AcceptedComparison, "cancelled saved pair cannot silently reapply on refresh or become accepted fallback");
		const auto duplicate = transition(epoch, cancel_comparison_resolution_intent{ epoch_request_id{ 7 }, comparison_generation{ 3 } });
		expect(duplicate.Requests.empty(), "duplicate cancellation produces no work");
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
	test_reading_identity_and_replacement();
	test_reading_course_refuses_substitution();
	test_exact_comparison_cancellation();
	std::cout << "window-review-restoration-tests:pass\n";
}
