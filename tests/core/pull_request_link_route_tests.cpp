/// \brief A clicked link into the open pull request opens in the
/// application: a published tour's link opens the tour, and a place the
/// window shows itself opens there. Every other link stays external and
/// keeps its question.
#include "bpr/core/pull_request_link_route.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(const bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			std::cerr << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	/// A link as `application::parse_setup_destination` reads it.
	auto link(const pull_request_location_kind aKind, const std::uint64_t aIdentity = 0U, std::string aLocation = "#x") -> setup_destination
	{
		return { .Hostname = "github.com", .Repository = { "o", "r" }, .Number = pull_request_number{ 7U }, .Location = std::move(aLocation), .Target = { .Kind = aKind, .Identity = aIdentity } };
	}
}

int main()
{
	const std::array                       commits{ std::string{ "abcdef1111111111111111111111111111111111" }, std::string{ "abcdef2222222222222222222222222222222222" } };
	const auto                             scope = pull_request_link_scope{ .Hostname = "github.com", .Owner = "o", .Repository = "r", .Number = pull_request_number{ 7U }, .Commits = commits };
	const std::vector<tour_remote_binding> roots{
		{ .Kind = tour_remote_kind::review, .DatabaseId = 9, .Entity = "review9", .WebUrl = "https://github.com/o/r/pull/7#pullrequestreview-9" },
		{ .Kind = tour_remote_kind::conversation_comment, .DatabaseId = 8, .Entity = "comment8", .WebUrl = "https://github.com/o/r/pull/7#issuecomment-8" },
		{ .Kind = tour_remote_kind::review, .DatabaseId = 9, .Entity = "review9", .WebUrl = "https://github.com/o/r/pull/7#pullrequestreview-9" },
	};
	const std::array paths{ std::string{ "src/a.cpp" } };
	const std::array reviews{ pull_request_location_conversation{ 9U, "review:PRR_9" }, pull_request_location_conversation{ 11U, "review:PRR_11" } };
	const std::array comments{ pull_request_location_conversation{ 21U, "review-comment:PRRC_21" } };
	const auto       facts = pull_request_location_facts{ .ChangedPaths = paths, .Reviews = reviews, .ReviewComments = comments, .RightIsHead = true };
	const auto       route = [&](const setup_destination& aLink) { return route_pull_request_link(aLink, scope, roots, facts); };

	// The description's [Review tour](…) and a stop's Tour overview.
	expect(route(link(pull_request_location_kind::review, 9U)) == pull_request_link_decision{ .Route = pull_request_link_route::tour, .Tour = 0U },
		   "a review tour link opens the first tour of its index review, before the review itself");
	expect(route(link(pull_request_location_kind::issue_comment, 8U)) == pull_request_link_decision{ .Route = pull_request_link_route::tour, .Tour = 1U },
		   "a compatibility tour link opens the tour of its index conversation comment");
	auto mixedCase             = link(pull_request_location_kind::review, 9U);
	mixedCase.Hostname         = "GitHub.COM";
	mixedCase.Repository.Owner = "O";
	mixedCase.Repository.Name  = "R";
	expect(route(mixedCase).Route == pull_request_link_route::tour, "host, owner and repository compare without ASCII case");

	// Another pull request, repository or host: the window cannot know it is a tour.
	auto other   = link(pull_request_location_kind::review, 9U);
	other.Number = pull_request_number{ 8U };
	expect(route(other).Route == pull_request_link_route::external, "the same review identity on another pull request is external");
	other                 = link(pull_request_location_kind::review, 9U);
	other.Repository.Name = "other";
	expect(route(other).Route == pull_request_link_route::external, "another repository is external");
	other                  = link(pull_request_location_kind::review, 9U);
	other.Repository.Owner = "other";
	expect(route(other).Route == pull_request_link_route::external, "another owner is external");
	other          = link(pull_request_location_kind::review, 9U);
	other.Hostname = "github.com.evil.example";
	expect(route(other).Route == pull_request_link_route::external, "a look-alike host is external");
	other          = link(pull_request_location_kind::review, 9U);
	other.Hostname = "";
	expect(route(other).Route == pull_request_link_route::external, "shorthand without a host is external");
	other      = link(pull_request_location_kind::review, 9U);
	other.Kind = setup_destination_kind::repository;
	expect(route(other).Route == pull_request_link_route::external, "a repository link is external");
	auto unaccepted   = scope;
	unaccepted.Number = pull_request_number{ };
	expect(route_pull_request_link(link(pull_request_location_kind::review, 9U), unaccepted, roots, facts).Route == pull_request_link_route::external,
		   "no accepted pull request routes nothing in");

	// Not a tour: the wrong kind for a root's identity, or no tours offered.
	expect(route(link(pull_request_location_kind::issue_comment, 9U)).Route == pull_request_link_route::external, "a conversation comment with a review root's identity is no tour");
	expect(route_pull_request_link(link(pull_request_location_kind::review, 9U), scope, { }, facts)
			   == pull_request_link_decision{ .Route = pull_request_link_route::location, .Target = { .Action = pull_request_location_action::reveal_conversation, .Conversation = "review:PRR_9" } },
		   "with no tour offered, a review the read holds opens in place");

	// The pull request's own pages and places the window shows itself.
	expect(route(link(pull_request_location_kind::pull_request, 0U, "")).Route == pull_request_link_route::conversation_page, "the page itself is its conversation");
	expect(route(link(pull_request_location_kind::pull_request, 0U, "/files")).Route == pull_request_link_route::files_page, "Files changed is Review");
	expect(route(link(pull_request_location_kind::review, 11U)) == pull_request_link_decision{ .Route = pull_request_link_route::location, .Target = { .Action = pull_request_location_action::reveal_conversation, .Conversation = "review:PRR_11" } },
		   "a review that indexes no tour opens in place");
	expect(route(link(pull_request_location_kind::review_comment, 21U)).Target.Conversation == "review-comment:PRRC_21", "a review comment the read identifies opens in place");
	auto file         = link(pull_request_location_kind::file, 0U, "/files#diff-x");
	file.Target       = { .Kind = pull_request_location_kind::file, .FileDigest = pull_request_file_digest("src/a.cpp"), .Side = diff_side::right, .Line = 3U };
	const auto opened = route(file);
	expect(opened.Route == pull_request_link_route::location && opened.Target.Action == pull_request_location_action::select_file && opened.Target.Path == "src/a.cpp",
		   "a changed file's link opens that file");

	// The Commits page and one of the pull request's commits open in the window.
	expect(route(link(pull_request_location_kind::pull_request, 0U, "/commits")).Route == pull_request_link_route::commits_page, "the commits page is the Commits tab");
	const auto commitLink = [&](const char* aName) {
		auto result          = link(pull_request_location_kind::commit, 0U, "/commits/x");
		result.Target.Kind   = pull_request_location_kind::commit;
		result.Target.Commit = aName;
		return result;
	};
	const auto whole = route(commitLink("abcdef2222222222222222222222222222222222"));
	expect(whole.Route == pull_request_link_route::commit && whole.Commit == commits[1], "a commit of the pull request opens filtered to it");
	const auto shortName = route(commitLink("abcdef11"));
	expect(shortName.Route == pull_request_link_route::commit && shortName.Commit == commits[0], "an unambiguous abbreviation names its full commit");
	expect(route(commitLink("abcdef")).Route == pull_request_link_route::external && route(commitLink("abcdef1")).Route == pull_request_link_route::commit,
		   "fewer than seven digits names nothing; seven unambiguous digits name one");
	expect(route(commitLink("abcde")).Route == pull_request_link_route::external, "a short name is external");
	auto             ambiguous = scope;
	const std::array twins{ std::string{ "abcdef1111111111111111111111111111111111" }, std::string{ "abcdef1122222222222222222222222222222222" } };
	ambiguous.Commits = twins;
	expect(route_pull_request_link(commitLink("abcdef1"), ambiguous, roots, facts).Route == pull_request_link_route::external, "an ambiguous abbreviation is external");
	expect(route(commitLink("0123456789abcdef")).Route == pull_request_link_route::external, "a commit the pull request does not hold is external");

	// What the window would only explain stays a link the browser can show.
	expect(route(link(pull_request_location_kind::pull_request, 0U, "/checks")).Route == pull_request_link_route::external, "the checks page is external");
	expect(route(link(pull_request_location_kind::pull_request, 0U, "#")).Route == pull_request_link_route::external, "an empty fragment is external");
	expect(route(link(pull_request_location_kind::issue_comment, 30U)).Route == pull_request_link_route::external, "a conversation comment that indexes no tour is external");
	expect(route(link(pull_request_location_kind::review, 99U)).Route == pull_request_link_route::external, "a review the read does not hold is external");
	expect(route(link(pull_request_location_kind::review_comment, 99U)).Route == pull_request_link_route::external, "a review comment the read does not identify is external");
	expect(route(link(pull_request_location_kind::unrecognized)).Route == pull_request_link_route::external, "an unknown fragment is external");
	auto missing   = link(pull_request_location_kind::file, 0U, "/files#diff-y");
	missing.Target = { .Kind = pull_request_location_kind::file, .FileDigest = pull_request_file_digest("src/gone.cpp") };
	expect(route(missing).Route == pull_request_link_route::external, "a file not in the pull request is external");
	std::cout << "pull-request-link-route:pass\n";
	return EXIT_SUCCESS;
}
