#include "../support/source_search_fixture.hpp"

#include <functional>
#include <limits>
#include <type_traits>
#include <vector>

namespace {

	using namespace bpr::core;
	using namespace bpr::tests;

	void test_query_and_scope_admission()
	{
		search_expect(inspect_source_search_query({ }) == source_search_input::empty, "empty means no scan");
		search_expect(inspect_source_search_query("line\r\n😀\n") == source_search_input::ready, "multiline and Unicode admitted without normalization");
		search_expect(inspect_source_search_query(std::string(4096U, 'a')) == source_search_input::ready, "4096 bytes admitted");
		search_expect(inspect_source_search_query(std::string(4097U, 'a')) == source_search_input::input_limit, "4097 bytes refused");
		for (const auto& invalid : { std::string("\xc0\xaf"), std::string("\xed\xa0\x80"), std::string("\xf4\x90\x80\x80"), std::string("\xf0\x9f"), std::string(1U, '\0') }) {
			search_expect(inspect_source_search_query(invalid) == source_search_input::invalid_utf8, "invalid scalar/UTF-8 input refused");
		}
		for (const auto target : { source_search_target::left, source_search_target::right, source_search_target::unified }) {
			auto scope = search_scope(target);
			scope.Document->LeftBlob.reset();
			search_expect(valid(scope) == (target != source_search_target::left), "an absent side cannot become its neighbour");
			scope.Document->RightBlob.reset();
			search_expect(!valid(scope), "no source side means unavailable");
		}
		auto state = source_search_state{ };
		search_expect(!edit_source_search_query(state, source_search_input::ready, { }).Work && state.Status == source_search_status::unavailable, "no scope never emits work");
		const auto scoped = set_source_search_scope(state, search_scope());
		search_expect(scoped.Work.has_value(), "exact scope resumes visible valid query");
		const auto empty = edit_source_search_query(state, source_search_input::empty, { });
		search_expect(empty.Cancel && !empty.Work && !state.Pending && !state.CurrentMatch, "clearing cancels without scanning or navigation");
	}

	void test_exact_stale_and_duplicate_rejection()
	{
		auto state = source_search_state{ };
		(void)set_source_search_scope(state, search_scope());
		const auto first  = search_request(state);
		const auto second = search_request(state);
		const auto third  = search_request(state);
		search_expect(complete_source_search(state, search_terminal(second)).Reason == source_search_reason::stale_completion, "reverse completion B refused");
		search_expect(complete_source_search(state, search_terminal(first)).Reason == source_search_reason::stale_completion, "first A does not alias later A");
		using mutation       = std::function<void(source_search_scope&)>;
		const auto mutations = std::vector<mutation>{
			[](auto& aScope) { aScope.Session                               = application_session_id{ 2U }; },
			[](auto& aScope) { aScope.Authority.Account                     = github_account_id{ 2U }; },
			[](auto& aScope) { aScope.Authority.Profile                     = host_profile_id{ 2U }; },
			[](auto& aScope) { aScope.Authority.PullRequest                 = pull_request_id{ 2U }; },
			[](auto& aScope) { aScope.CatalogueGeneration                   = 2U; },
			[](auto& aScope) { aScope.Tab                                   = document_tab_id{ 2U }; },
			[](auto& aScope) { aScope.Comparison.Left.Commit                = *parse_git_commit_oid(std::string(40U, 'e')); },
			[](auto& aScope) { aScope.Comparison.Right.Identity.Incarnation = search_identity<epoch_catalog_incarnation_id>(9U); },
			[](auto& aScope) { aScope.Target                                = source_search_target::left; },
			[](auto& aScope) { aScope.Document->File                        = search_identity<file_identity>(9U); },
			[](auto& aScope) { aScope.Document->Comparison                  = search_identity<file_comparison_digest>(9U); },
			[](auto& aScope) { aScope.Document->RightBlob                   = *parse_git_blob_oid(std::string(40U, 'e')); }
		};
		for (const auto& mutate : mutations) {
			auto foreign = search_terminal(third);
			mutate(foreign.Request.Scope);
			search_expect(complete_source_search(state, foreign).Reason == source_search_reason::stale_completion && state.Pending == third, "foreign source identity changes nothing");
		}
		search_expect(complete_source_search(state, search_terminal(third)).Accepted && state.Summary.Matches == 3U, "only exact current completion accepted");
		search_expect(complete_source_search(state, search_terminal(third)).Reason == source_search_reason::stale_completion, "duplicate refused");
	}

	void test_scope_refresh_and_authority_ownership()
	{
		auto state = source_search_state{ };
		auto scope = search_scope(source_search_target::files);
		(void)set_source_search_scope(state, scope);
		const auto request = search_request(state);
		(void)complete_source_search(state, search_terminal(request));
		search_expect(!set_source_search_scope(state, scope).Changed && state.Summary.Matches == 3U, "same exact pinned scope needs no scan on refresh");
		scope.Comparison.Right.Commit = *parse_git_commit_oid(std::string(40U, 'e'));
		const auto retarget           = set_source_search_scope(state, scope);
		search_expect(retarget.Work && !retarget.ClearQuery && state.Input == source_search_input::ready && state.Summary.Matches == 0U, "same authority comparison change retains query and clears obsolete hits");
		(void)set_source_search_scope(state, std::nullopt);
		scope.Authority.Account = github_account_id{ 2U };
		const auto foreign      = set_source_search_scope(state, scope);
		search_expect(foreign.ClearQuery && !foreign.Work && state.Input == source_search_input::empty, "unavailable-scope gap cannot carry query into another account");
		(void)search_request(state);
		search_expect(close_source_search(state).Cancel && state.Input == source_search_input::ready, "close cancels and keeps input/options");
		search_expect(open_source_search(state).Work.has_value(), "reopen submits retained query under same exact authority");
		search_expect(reset_source_search(state).ClearQuery && !state.Scope && !state.QueryOwner && state.Input == source_search_input::empty, "reset revokes query and ownership");
		state.Generation = std::numeric_limits<std::uint64_t>::max();
		(void)set_source_search_scope(state, search_scope());
		search_expect(!edit_source_search_query(state, source_search_input::ready, { }).Work && state.Status == source_search_status::identity_exhausted, "exhaustion fails closed without reusing generation");
	}

	void test_qualified_coverage_and_navigation()
	{
		for (const auto status : { source_search_status::complete, source_search_status::partial, source_search_status::cancelled, source_search_status::resource_limit, source_search_status::unavailable, source_search_status::failure, source_search_status::invalid_pattern, source_search_status::unsupported_pattern }) {
			auto state = source_search_state{ };
			(void)set_source_search_scope(state, search_scope());
			auto terminal   = search_terminal(search_request(state), 0U);
			terminal.Status = status;
			search_expect(complete_source_search(state, terminal).Accepted, "typed terminal accepted");
			search_expect(source_search_no_results(state) == (status == source_search_status::complete), "only complete empty is No results");
		}
		auto state = source_search_state{ };
		(void)set_source_search_scope(state, search_scope());
		auto terminal                  = search_terminal(search_request(state), 0U);
		terminal.Summary.SearchedFiles = 1U;
		(void)complete_source_search(state, terminal);
		search_expect(state.Status == source_search_status::partial && !source_search_no_results(state), "unsearched files cannot count as zero");
		terminal                      = search_terminal(search_request(state), 0U);
		terminal.Summary.SkippedSides = 1U;
		(void)complete_source_search(state, terminal);
		search_expect(state.Status == source_search_status::partial && !source_search_no_results(state), "skipped sides qualify empty coverage");
		terminal                      = search_terminal(search_request(state));
		terminal.Status               = source_search_status::partial;
		terminal.Summary.InitialMatch = 2U;
		(void)complete_source_search(state, terminal);
		search_expect(navigate_source_search(state, change_navigation_direction::next).Wrapped && state.CurrentMatch == 0U, "next wraps usable partial prefix");
		search_expect(navigate_source_search(state, change_navigation_direction::previous).Wrapped && state.CurrentMatch == 2U, "previous wraps in source order");
		terminal                                 = search_terminal(search_request(state), maximum_source_search_matches);
		terminal.Status                          = source_search_status::result_limit;
		terminal.Summary.AdditionalMatchObserved = true;
		search_expect(complete_source_search(state, terminal).Accepted && state.Summary.AdditionalMatchObserved, "plus needs observed additional match at exact cap");
		terminal                                 = search_terminal(search_request(state), 1U);
		terminal.Summary.AdditionalMatchObserved = true;
		search_expect(complete_source_search(state, terminal).Reason == source_search_reason::invalid_completion && state.Summary.Matches == 0U, "false plus refused");
		terminal = search_terminal(search_request(state), maximum_source_search_matches + 1U);
		search_expect(complete_source_search(state, terminal).Reason == source_search_reason::invalid_completion, "oversized count refused");
		terminal                       = search_terminal(search_request(state), 1U);
		terminal.Summary.SearchedFiles = 0U;
		search_expect(complete_source_search(state, terminal).Reason == source_search_reason::invalid_completion, "hits without any searched file refused");
		terminal        = search_terminal(search_request(state), 0U);
		terminal.Status = source_search_status::searching;
		search_expect(complete_source_search(state, terminal).Reason == source_search_reason::invalid_completion && !state.Pending, "nonterminal status fails once instead of leaving searching stuck");
	}
}

int main()
{
	static_assert(std::is_trivially_copyable_v<source_search_state>);
	test_query_and_scope_admission();
	test_exact_stale_and_duplicate_rejection();
	test_scope_refresh_and_authority_ownership();
	test_qualified_coverage_and_navigation();
	std::cout << "source-search-core:pass state-bytes=" << sizeof(source_search_state) << '\n';
}
