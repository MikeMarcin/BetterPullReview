#include "bpr/core/markdown_navigation.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

namespace {
	using namespace bpr::core;
	/// [[BPR-1338]]. An admitted page's parts in their address order, without a scheme.
	auto page_parts(const pull_request_page& aPage) -> std::string
	{
		return aPage.Hostname + "/" + aPage.Owner + "/" + aPage.Repository + "/pull/" + std::to_string(aPage.Number.value());
	}
	void expect(const bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			std::cerr << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
}
int main()
{
	using namespace bpr::core;
	const markdown_context          context{ { default_bpr_profile_id, bpr_window_id{ 2 }, bpr_window_generation{ 3 } }, application_session_id{ 4 }, github_identity_generation{ 5 }, provider_snapshot_id{ 6 }, 7 };
	const markdown_link_observation observation{ context, true, true, "ghe.example.invalid", "owner", "repository", "2222222222222222222222222222222222222222", pull_request_number{ 42 } };
	for (const auto* address : { "https://example.invalid/path?q=safe#section", "http://example.invalid:8080/doc", "HTTPS://EXAMPLE.INVALID/read", "https://example.invalid/%E6%97%A5" }) {
		const auto admitted = admit_markdown_link({ context, { }, address }, observation);
		expect(admitted && admitted->Kind == markdown_destination_kind::absolute_web && admitted->Target == address, "explicit web destination is preserved exactly");
	}
	for (const auto* address : { "", "javascript:alert(1)", "file:///tmp/a", "data:text/html,x", "qrc:/a", "mailto:person@example.invalid", "https://user:password@example.invalid/a", "https://user@example.invalid/a", "https://example.invalid\\@evil.invalid", "https://%65xample.invalid/a", "https:///path", "https://example.invalid:0/a", "https://example.invalid:65536/a", "https://example.invalid/%00", "https://example.invalid/%0d%0a", "https://example.invalid/?access_token=x", "https://example.invalid/?%74oken=x", "https://example.invalid/?client_secret=x", "https://example.invalid/a b", "https://example.invalid/%xz", "//evil.invalid/a", "/absolute/ambiguous", "../outside.md", "./%2e%2e/outside", "?query=ambiguous" }) {
		const auto admitted = admit_markdown_link({ context, { }, address }, observation);
		expect(!admitted && admitted.error() == markdown_link_result::refused, address);
	}
	const auto relative = admit_markdown_link({ context, "comment", "./docs/../guide.md#start" }, observation);
	expect(relative && relative->Kind == markdown_destination_kind::repository_path && relative->Target == "guide.md#start", "relative destination is normalized only with an exact accepted repository/head");
	const auto fragment = admit_markdown_link({ context, { }, "#discussion" }, observation);
	expect(fragment && fragment->Kind == markdown_destination_kind::pull_request_fragment && fragment->Target == "#discussion", "fragment identifies the exact accepted PR");
	auto missing     = observation;
	missing.Revision = { };
	expect(!admit_markdown_link({ context, { }, "readme.md" }, missing), "relative paths require a full exact revision");
	auto stale = context;
	++stale.Revision;
	expect(admit_markdown_link({ stale, { }, "https://example.invalid" }, observation).error() == markdown_link_result::stale, "stale publication refuses");
	stale               = context;
	stale.Window.Window = bpr_window_id{ 99 };
	expect(admit_markdown_link({ stale, { }, "https://example.invalid" }, observation).error() == markdown_link_result::stale, "sibling window refuses");
	missing               = observation;
	missing.BodyAvailable = false;
	expect(admit_markdown_link({ context, "missing", "https://example.invalid" }, missing).error() == markdown_link_result::stale, "missing body refuses");
	missing                  = observation;
	missing.BrowserAvailable = false;
	expect(admit_markdown_link({ context, { }, "https://example.invalid" }, missing).error() == markdown_link_result::unavailable, "absent capability is explicit");
	expect(!admit_markdown_link({ context, { }, std::string(4097, 'a') }, observation), "destination input is bounded");
	// [[BPR-1338]]. The pull request's own page is built from accepted parts
	// only, with the same authority and name rules as a relative link.
	const pull_request_page_observation page{ true, "ghe.example.invalid", "owner", "repository.name", pull_request_number{ 42 } };
	const auto                          opened = admit_pull_request_page(page);
	expect(opened && page_parts(*opened) == "ghe.example.invalid/owner/repository.name/pull/42", "the page is the canonical pull request's parts");
	auto ported     = page;
	ported.Hostname = "ghe.example.invalid:8443";
	expect(admit_pull_request_page(ported).transform(page_parts).value_or("") == "ghe.example.invalid:8443/owner/repository.name/pull/42", "a compiled port is kept");
	for (const auto* hostname : { "", "user@ghe.example.invalid", "ghe.example.invalid/evil", "ghe..invalid", ".ghe.invalid", "ghe_example.invalid", "ghe.example.invalid:0", "ghe.example.invalid:x", "evil.invalid#ghe" }) {
		auto unsafe     = page;
		unsafe.Hostname = hostname;
		expect(admit_pull_request_page(unsafe).error() == markdown_link_result::refused, hostname);
	}
	for (const auto* name : { "", ".", "..", "a/b", "a b", "a%2fb", "a?b", "a#b", "a@b", "javascript:x" }) {
		auto owner  = page;
		owner.Owner = name;
		expect(admit_pull_request_page(owner).error() == markdown_link_result::refused, "an unsafe owner refuses");
		auto repository       = page;
		repository.Repository = name;
		expect(admit_pull_request_page(repository).error() == markdown_link_result::refused, "an unsafe repository refuses");
	}
	auto numberless   = page;
	numberless.Number = pull_request_number{ };
	expect(admit_pull_request_page(numberless).error() == markdown_link_result::refused, "a page needs a number");
	numberless.Number = pull_request_number{ maximum_pull_request_number + 1U };
	expect(admit_pull_request_page(numberless).error() == markdown_link_result::refused, "a page's number is bounded");
	auto browserless             = page;
	browserless.BrowserAvailable = false;
	expect(admit_pull_request_page(browserless).error() == markdown_link_result::unavailable, "an absent browser is explicit for the page too");
	// [[BPR-1338]]. A Home queue row opens on the accepted session's host,
	// for the accepted account only, with the accepted hostname in the address.
	const pull_request_ref row{ .Host = "github.com", .Account = "octo-reviewer", .Repository = "octo-org/app", .Number = 101 };
	expect(admit_queue_pull_request_page(row, "github.com", "octo-reviewer", true).transform(page_parts).value_or("") == "github.com/octo-org/app/pull/101", "a queue row opens its canonical page");
	expect(admit_queue_pull_request_page(row, "github.com", "octo-reviewer", false).error() == markdown_link_result::unavailable, "a queue row needs a browser");
	expect(admit_queue_pull_request_page(row, "ghe.example.invalid", "octo-reviewer", true).error() == markdown_link_result::refused, "another host's row is refused");
	expect(admit_queue_pull_request_page(row, "github.com", "someone-else", true).error() == markdown_link_result::refused, "another account's row is refused");
	expect(admit_queue_pull_request_page(row, "", "", true).error() == markdown_link_result::refused, "no accepted session, no page");
	for (const auto* repository : { "octo-org", "octo-org/app/extra", "/app", "octo-org/", "evil.invalid@octo-org/app", "octo-org/a?b", "../app" }) {
		auto odd       = row;
		odd.Repository = repository;
		expect(admit_queue_pull_request_page(odd, "github.com", "octo-reviewer", true).error() == markdown_link_result::refused, repository);
	}
	for (const auto number : { std::int64_t{ 0 }, std::int64_t{ -1 }, std::int64_t{ 4'294'967'297 } }) {
		auto odd   = row;
		odd.Number = number;
		expect(admit_queue_pull_request_page(odd, "github.com", "octo-reviewer", true).error() == markdown_link_result::refused, "a queue row needs a pull request number in range");
	}
	// [[BPR-1338]]. A Recently viewed row: public readings on the session's
	// host, an account's reading under that account's exact authority.
	window_review_destination publicRead;
	publicRead.Mode       = github_access_mode::public_preview;
	publicRead.Hostname   = "github.com";
	publicRead.Repository = { "octo-org", "tools" };
	publicRead.Number     = pull_request_number{ 7 };
	const window_review_authority signedIn{ github_access_mode::authenticated, "github.com" };
	expect(admit_recent_pull_request_page(publicRead, signedIn, github_account_id{ 1 }, true).transform(page_parts).value_or("") == "github.com/octo-org/tools/pull/7", "a public reading opens its page");
	auto mineRead    = publicRead;
	mineRead.Mode    = github_access_mode::authenticated;
	mineRead.Account = github_account_id{ 1 };
	expect(admit_recent_pull_request_page(mineRead, signedIn, github_account_id{ 1 }, true).transform(page_parts).value_or("") == "github.com/octo-org/tools/pull/7", "my reading opens under my session");
	expect(admit_recent_pull_request_page(mineRead, signedIn, github_account_id{ 2 }, true).error() == markdown_link_result::refused, "another account's reading is refused");
	auto elsewhere     = publicRead;
	elsewhere.Hostname = "ghe.example.invalid";
	expect(admit_recent_pull_request_page(elsewhere, signedIn, github_account_id{ 1 }, true).error() == markdown_link_result::refused, "a reading on another host is refused");
	expect(admit_recent_pull_request_page(publicRead, signedIn, github_account_id{ 1 }, false).error() == markdown_link_result::unavailable, "a reading needs a browser");
	auto unsafe            = publicRead;
	unsafe.Repository.Name = "tools/../../evil";
	expect(admit_recent_pull_request_page(unsafe, signedIn, github_account_id{ 1 }, true).error() == markdown_link_result::refused, "a stored name that is not a name is refused");
	// [[BPR-1342]]. Open Review in New Window: a queue row becomes the
	// destination a new window of this profile opens, on the accepted host and
	// for the accepted account only.
	const auto queued = admit_queue_review_window(row, "github.com", "octo-reviewer", github_account_id{ 1 });
	expect(queued && queued->Mode == github_access_mode::authenticated && queued->Hostname == "github.com" && queued->Repository.Owner == "octo-org"
			   && queued->Repository.Name == "app" && queued->Number.value() == 101U && queued->Account == github_account_id{ 1 },
		   "a queue row is a new window's destination on the accepted host and account");
	expect(admit_queue_review_window(row, "ghe.example.invalid", "octo-reviewer", github_account_id{ 1 }).error() == markdown_link_result::refused, "a queue row on another host does not open a new window");
	expect(admit_queue_review_window(row, "github.com", "someone-else", github_account_id{ 1 }).error() == markdown_link_result::refused, "another account's queue row does not open a new window");
	expect(admit_queue_review_window(row, "github.com", "octo-reviewer", github_account_id{ }).error() == markdown_link_result::refused, "a queue row needs an accepted account");
	auto unsafeRow       = row;
	unsafeRow.Repository = "octo-org/a?b";
	expect(admit_queue_review_window(unsafeRow, "github.com", "octo-reviewer", github_account_id{ 1 }).error() == markdown_link_result::refused, "a queue row whose name is not a name does not open a new window");
	// A Recently viewed row: the rule its Open follows in this window.
	expect(admit_recent_review_window(publicRead, std::nullopt, github_account_id{ }).value_or(window_review_destination{ }) == publicRead, "a public reading opens a new window signed out");
	expect(admit_recent_review_window(publicRead, signedIn, github_account_id{ 1 }).value_or(window_review_destination{ }) == publicRead, "a public reading opens a new window signed in");
	expect(admit_recent_review_window(mineRead, signedIn, github_account_id{ 1 }).value_or(window_review_destination{ }) == mineRead, "my reading opens a new window under my session");
	expect(admit_recent_review_window(mineRead, signedIn, github_account_id{ 2 }).error() == markdown_link_result::refused, "another account's reading does not open a new window");
	expect(admit_recent_review_window(mineRead, std::nullopt, github_account_id{ }).error() == markdown_link_result::refused, "an account's reading needs that account's session");
	auto mineElsewhere     = mineRead;
	mineElsewhere.Hostname = "ghe.example.invalid";
	expect(admit_recent_review_window(mineElsewhere, signedIn, github_account_id{ 1 }).error() == markdown_link_result::refused, "an account's reading on another host does not open a new window");
	expect(admit_recent_review_window(unsafe, signedIn, github_account_id{ 1 }).error() == markdown_link_result::refused, "a stored name that is not a name does not open a new window");
	// [[BPR-1544]]. An HTML link keeps its link only when it could ever be admitted.
	for (const auto* address : { "https://example.invalid/docs", "HTTP://example.invalid:8080/a", "#section", "docs/guide.md", "./guide.md#start", "/root/path", "path/with:colon" }) {
		expect(markdown_html_link_kept(address), address);
	}
	for (const auto* address : { "", "javascript:alert(1)", "JavaScript:alert(1)", "vbscript:x", "data:text/html,x", "file:///etc/passwd", "mailto:a@example.invalid", "jav\tascript:x", " javascript:x", "https://user:password@example.invalid/a", "https://example.invalid/?access_token=x", "https://example.invalid/a b", "https://example.invalid\\@evil.invalid" }) {
		expect(!markdown_html_link_kept(address), address);
	}
	std::cout << "Markdown navigation policy passed\n";
}
