// [[BPR-1680]]. Home discovery's core decisions: the diagnostic line a
// refresh and each of its reads record, and which pull requests a refresh's
// searches need not read again.
#include "bpr/core/discovery_diagnosis.hpp"
#include "bpr/core/home_discovery_plan.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void test_diagnosis_line()
	{
		const auto read = discovery_diagnosis{
			.Scope            = discovery_diagnosis_scope::read,
			.Refresh          = 3U,
			.Read             = 7U,
			.Reason           = discovery_diagnosis_reason::authored,
			.Repositories     = 1U,
			.Requests         = 6U,
			.WaitMilliseconds = 120U,
			.RunMilliseconds  = 3900U,
		};
		const auto text = describe(read);
		expect(text == "discovery=read refresh=3 read=7 reason=authored state=none repositories=1 pull_requests=0 reads=0 failed=0 requests=6 wait_ms=120 run_ms=3900",
			   "a read's line names its keys in their fixed order");
		expect(parse_discovery_diagnosis(text) == read, "the line reads back as the same diagnosis");

		const auto refresh = discovery_diagnosis{
			.Scope            = discovery_diagnosis_scope::refresh,
			.Refresh          = 3U,
			.State            = discovery_diagnosis_state::stale,
			.Repositories     = 5U,
			.PullRequests     = 20U,
			.Reads            = 30U,
			.Failed           = 1U,
			.Requests         = 220U,
			.WaitMilliseconds = 4100U,
			.RunMilliseconds  = 143000U,
		};
		expect(parse_discovery_diagnosis(describe(refresh)) == refresh, "a refresh's end reads back");
		for (const auto state : { discovery_diagnosis_state::none, discovery_diagnosis_state::current, discovery_diagnosis_state::partial,
								  discovery_diagnosis_state::stopped, discovery_diagnosis_state::cancelled }) {
			auto other  = refresh;
			other.State = state;
			expect(parse_discovery_diagnosis(describe(other)) == other, "every refresh state reads back");
		}
		for (const auto reason : { discovery_diagnosis_reason::retained, discovery_diagnosis_reason::requested }) {
			auto other   = read;
			other.Reason = reason;
			expect(parse_discovery_diagnosis(describe(other)) == other, "every read reason reads back");
		}

		auto invalid    = read;
		invalid.Refresh = 0U;
		expect(describe(invalid).empty(), "a refresh is numbered from 1");
		invalid      = read;
		invalid.Read = 0U;
		expect(describe(invalid).empty(), "a read is numbered within its refresh");
		invalid        = read;
		invalid.Reason = discovery_diagnosis_reason::none;
		expect(describe(invalid).empty(), "a read has a reason");
		invalid       = read;
		invalid.State = discovery_diagnosis_state::current;
		expect(describe(invalid).empty(), "a read has no refresh state");
		invalid      = refresh;
		invalid.Read = 1U;
		expect(describe(invalid).empty(), "a refresh line has no read ordinal");
		invalid        = refresh;
		invalid.Failed = 31U;
		expect(describe(invalid).empty(), "no more failures than reads");
		invalid       = refresh;
		invalid.Scope = static_cast<discovery_diagnosis_scope>(9U);
		expect(describe(invalid).empty() && name(invalid.Scope).empty(), "an unknown scope has no name and no line");

		for (const std::string_view hostile : {
				 "",
				 "discovery=read",
				 "discovery=read refresh=3 read=7 reason=authored state=none repositories=1 pull_requests=0 reads=0 failed=0 requests=6 wait_ms=120 run_ms=3900 ",
				 "discovery=read refresh=3 read=7 reason=authored state=none repositories=1 pull_requests=0 reads=0 failed=0 requests=6 wait_ms=120 run_ms=3900 host=example.com",
				 "discovery=read refresh=03 read=7 reason=authored state=none repositories=1 pull_requests=0 reads=0 failed=0 requests=6 wait_ms=120 run_ms=3900",
				 "discovery=read refresh=3 read=7 reason=octo-org/private state=none repositories=1 pull_requests=0 reads=0 failed=0 requests=6 wait_ms=120 run_ms=3900",
				 "discovery=read refresh=3 read=7 reason=authored state=none repositories=1 pull_requests=0 reads=0 failed=0 requests=6 wait_ms=120 run_ms=4294967296",
				 "discovery=read  refresh=3 read=7 reason=authored state=none repositories=1 pull_requests=0 reads=0 failed=0 requests=6 wait_ms=120 run_ms=3900",
				 "discovery=read refresh=3 reason=authored read=7 state=none repositories=1 pull_requests=0 reads=0 failed=0 requests=6 wait_ms=120 run_ms=3900",
				 "review_submission=review_not_listed post=not_sent post_class=0 http_class=0 expected=0 found=0 matched=0 candidates=0 pages=0",
			 }) {
			expect(!parse_discovery_diagnosis(hostile), "anything but exactly a line describe writes reads as none");
		}
	}
	/// Option 2: a pull request a refresh's retained read read in full stands
	/// for its search row; one it found closed, or read only in part, does not.
	void test_known_pull_requests()
	{
		const auto app   = repository_locator{ .Owner = "octo", .Name = "app" };
		const auto tools = repository_locator{ .Owner = "octo", .Name = "tools" };
		const auto read  = [](const repository_locator& aRepository, const std::uint32_t aNumber, const bool aOpen = true, const bool aComplete = true, const bool aViewer = false) {
			return discovery_retained_read{ .Repository = aRepository, .Number = pull_request_number{ aNumber }, .Open = aOpen, .Complete = aComplete, .RequestsViewer = aViewer };
		};
		const auto reads = std::vector<discovery_retained_read>{
			read(tools, 9U),
			read(app, 7U, true, true, true),
			read(app, 3U),
			read(app, 5U, false),
			read(app, 6U, true, false),
			read(repository_locator{ .Owner = "", .Name = "app" }, 8U),
			read(app, 0U),
			read(app, 3U, true, true, true),
		};
		const auto known = discovery_known_pull_requests(reads);
		expect(known.size() == 3U && known[0].Repository == app && known[0].Number.value() == 3U && !known[0].RequestsViewer
				   && known[1].Number.value() == 7U && known[2].Repository == tools && known[2].Number.value() == 9U,
			   "only open, fully read pull requests, in repository and number order, once each (the first read kept)");
		expect(discovery_known_pull_requests({ }).empty(), "a refresh with no retained read knows nothing");
		expect(keeps_reused_row(discovery_search_reason::authored, read(app, 3U)), "an authored row stays: the search matched its author");
		expect(keeps_reused_row(discovery_search_reason::requested, read(app, 7U, true, true, true)), "a requested row stays when the viewer is asked by name");
		expect(!keeps_reused_row(discovery_search_reason::requested, read(app, 3U)), "and goes when only a team is asked, as its own read would decide");
	}
	/// Option 3: one search for several saved repositories, split where the
	/// query would grow too long, and asked again alone when it cannot speak
	/// for each repository.
	void test_search_plan()
	{
		const auto repository = [](const std::string_view aOwner, const std::string_view aName) {
			return repository_locator{ .Owner = std::string{ aOwner }, .Name = std::string{ aName } };
		};
		const auto app   = repository("octo", "app");
		const auto tools = repository("octo", "tools");
		expect(discovery_search_query(discovery_search_reason::authored, "octo-reviewer", std::vector{ app }) == "is:pr is:open author:octo-reviewer repo:octo/app",
			   "a search of one repository is the query it always was");
		expect(discovery_search_query(discovery_search_reason::requested, "octo-reviewer", std::vector{ app, tools })
				   == "is:pr is:open review-requested:octo-reviewer repo:octo/app repo:octo/tools",
			   "several repositories are named in order, one qualifier each");

		const auto five = std::vector{ app, tools, repository("octo", "docs"), repository("other-org", "service"), repository("octo", "web") };
		const auto plan = plan_discovery_searches(discovery_search_reason::authored, "octo-reviewer", five);
		expect(plan.size() == 1U && plan.front() == five, "five short repositories are one search");

		auto many = std::vector<repository_locator>{ };
		for (auto index = 0; index < 25; ++index) {
			many.push_back(repository("organization-" + std::to_string(index), "repository-with-a-name"));
		}
		for (const auto reason : { discovery_search_reason::authored, discovery_search_reason::requested }) {
			const auto searches = plan_discovery_searches(reason, "octo-reviewer", many);
			auto       covered  = std::vector<repository_locator>{ };
			for (const auto& search : searches) {
				expect(!search.empty() && search.size() <= maximum_discovery_search_repositories
						   && discovery_search_query(reason, "octo-reviewer", search).size() <= maximum_discovery_search_query_bytes,
					   "every search stays inside the query and repository bounds");
				covered.insert(covered.end(), search.begin(), search.end());
			}
			expect(covered == many && searches.size() > 1U && searches.size() < many.size(), "the repositories are split into a few searches, each once, in order");
		}

		const auto alone = plan_discovery_searches(discovery_search_reason::authored, "octo-reviewer", five, std::vector{ tools, repository("octo", "web") });
		expect(alone.size() == 3U && alone[0] == std::vector{ app, repository("octo", "docs"), repository("other-org", "service") }
				   && alone[1] == std::vector{ tools } && alone[2] == std::vector{ repository("octo", "web") },
			   "a repository refused before is searched on its own, after the rest, which stay together");
		const auto longName = repository(std::string(39U, 'o'), std::string(100U, 'n'));
		const auto longOnes = plan_discovery_searches(discovery_search_reason::requested, std::string(39U, 'l'), std::vector{ app, longName, longName, tools });
		expect(longOnes.size() == 2U && longOnes[0] == std::vector{ app, longName } && longOnes[1] == std::vector{ longName, tools },
			   "two of the longest names never share a search; each shares with a short one");
		for (const auto& search : longOnes) {
			expect(discovery_search_query(discovery_search_reason::requested, std::string(39U, 'l'), search).size() <= maximum_discovery_search_query_bytes,
				   "and every search stays inside the bound");
		}
		expect(plan_discovery_searches(discovery_search_reason::authored, "octo-reviewer", { }).empty(), "no repository, no search");
		const auto cased = plan_discovery_searches(discovery_search_reason::authored, "octo-reviewer", std::vector{ repository("Octo", "App"), app, tools });
		expect(cased.size() == 2U && cased[0] == std::vector{ repository("Octo", "App") } && cased[1] == std::vector{ app, tools },
			   "two names of one repository in different case never share a search");

		for (const auto outcome : { discovery_search_outcome::refused, discovery_search_outcome::result_limit, discovery_search_outcome::detail_limit }) {
			expect(splits_discovery_search(2U, outcome) && !splits_discovery_search(1U, outcome), "a shared search that cannot speak for each repository is asked again alone");
		}
		for (const auto outcome : { discovery_search_outcome::complete, discovery_search_outcome::incomplete_results, discovery_search_outcome::failed }) {
			expect(!splits_discovery_search(5U, outcome), "a complete answer, GitHub's incomplete search and other failures stand for every repository");
		}
	}
	/// Option 4: retained pull requests read several to a query, within
	/// GitHub's node and cost bounds.
	void test_retained_batches()
	{
		const auto app    = repository_locator{ .Owner = "octo", .Name = "app" };
		const auto tools  = repository_locator{ .Owner = "octo", .Name = "tools" };
		const auto target = [](const repository_locator& aRepository, const std::uint32_t aNumber) {
			return discovery_retained_target{ .Repository = aRepository, .Number = pull_request_number{ aNumber } };
		};
		auto targets = std::vector{ target(tools, 9U), target(app, 17U), target(app, 4U), target(tools, 2U), target(app, 17U) };
		for (std::uint32_t number = 100U; number < 112U; ++number) {
			targets.push_back(target(app, number));
		}
		const auto batches = plan_retained_batches(targets);
		expect(batches.size() == 3U && batches[0].Repository == tools && batches[0].Numbers == std::vector{ pull_request_number{ 2U }, pull_request_number{ 9U } },
			   "a repository's pull requests are one read, ascending, in the order the repository first appears");
		expect(batches[1].Repository == app && batches[1].Numbers.size() == maximum_discovery_batch_pull_requests && batches[1].Numbers.front().value() == 4U
				   && batches[2].Repository == app && batches[2].Numbers.size() == 4U,
			   "a repository with more than one query's worth is cut, the first-asked first, and a repeat counts once");
		auto counted = std::size_t{ 0U };
		for (const auto& batch : batches) {
			counted += batch.Numbers.size();
		}
		expect(counted == 16U, "every pull request is read exactly once");
		const auto single = plan_retained_batches(targets, 1U);
		expect(single.size() == 16U && std::ranges::all_of(single, [](const discovery_retained_batch& aBatch) { return aBatch.Numbers.size() == 1U; }),
			   "a bound of one reads each on its own, as discovery did before");
		expect(plan_retained_batches({ }).empty(), "nothing retained, nothing read");
		expect(discovery_batch_nodes(maximum_discovery_batch_pull_requests) <= maximum_github_graphql_nodes && discovery_batch_cost(1U) == 1U
				   && discovery_batch_cost(maximum_discovery_batch_pull_requests) <= 10U,
			   "a full query stays far inside GitHub's node limit and costs a few points");
	}
} // namespace

auto main() -> int
{
	test_retained_batches();
	test_diagnosis_line();
	test_known_pull_requests();
	test_search_plan();
	std::cout << "home discovery core tests passed\n";
	return EXIT_SUCCESS;
}
