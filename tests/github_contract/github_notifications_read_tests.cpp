// [[BPR-1850]]. The notification list's request and answers: the compiled
// conditional GET, its validation, what `gh api --include` prints for a 304,
// and how 200, 304, 403, 404, 429 and 5xx answers are read.
#include "gh_cli_response.hpp"
#include "github_notifications_read.hpp"
#include "github_read_contract.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;
	using namespace bpr::adapters::github::detail;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	constexpr auto date = std::string_view{ "Fri, 30 Oct 2026 04:02:16 GMT" };

	void test_request()
	{
		const auto plain = compile_read_request({ .Server = server_contract::github_com, .Operation = read_operation::list_notifications, .Page = 1U });
		expect(plain.has_value(), "the notification list compiles");
		expect(plain->Path == "/notifications" && plain->Query == "participating=true&per_page=50" && plain->Method == http_method::get && plain->Body.empty(),
			   "one bounded page of participating threads");
		expect(std::ranges::none_of(plain->Headers, [](const http_header& aHeader) { return aHeader.Name == "If-Modified-Since"; }), "no date, no condition");

		const auto conditional = compile_read_request({ .Server = server_contract::ghes_3_18, .Operation = read_operation::list_notifications, .Page = 1U, .IfModifiedSince = std::string{ date } });
		expect(conditional.has_value() && conditional->Path == "/api/v3/notifications", "GHES reads its own API root");
		expect(std::ranges::count(conditional->Headers, http_header{ .Name = "If-Modified-Since", .Value = std::string{ date } }) == 1, "the previous Last-Modified is sent back");
		expect(validate_outbound_read_request(*conditional).has_value(), "and validates");

		auto tampered                 = *conditional;
		tampered.Headers.back().Value = "Sat, 31 Oct 2026 04:02:16 GMT";
		expect(!validate_outbound_read_request(tampered).has_value(), "a header that is not the semantic request's is refused");

		expect(!compile_read_request({ .Server = server_contract::github_com, .Operation = read_operation::list_notifications, .Page = 1U, .IfModifiedSince = "yesterday" }).has_value(),
			   "only an HTTP date is sent");
		expect(!compile_read_request({ .Server = server_contract::github_com, .Operation = read_operation::list_notifications, .Page = 1U, .IfModifiedSince = "Fri, 30 Oct 2026 04:02:16 GMT\r\nX: y" }).has_value(),
			   "no header injection");
		expect(!compile_read_request({ .Server = server_contract::github_com, .Operation = read_operation::get_viewer_identity, .Page = 1U, .IfModifiedSince = std::string{ date } }).has_value(),
			   "no other read is conditional");
		expect(!compile_read_request({ .Server = server_contract::github_com, .Operation = read_operation::list_notifications, .Owner = "octo", .Repository = "widgets", .Page = 1U }).has_value(),
			   "the list names no repository");
		expect(!compile_read_request({ .Server = server_contract::github_com, .Operation = read_operation::list_notifications, .Page = 2U }).has_value(), "one page only");
		expect(operation_name(read_operation::list_notifications) == "ListNotifications", "named");
	}

	void test_gh_cli_304()
	{
		// What gh 2.103 printed for a conditional poll on 2026-11-03, with its
		// request identities removed; gh exits 1 and says "gh: HTTP 304" on stderr.
		const auto output = std::string{
			"HTTP/2.0 304 Not Modified\n"
			"Cache-Control: private, max-age=60, s-maxage=60\r\n"
			"Date: Tue, 03 Nov 2026 23:50:00 GMT\r\n"
			"Etag: \"479ca0eabb6f1a3a8a01354c878b39c4551d03b10e9cd583c75bc884ef5bdf95\"\r\n"
			"Last-Modified: Fri, 30 Oct 2026 04:02:16 GMT\r\n"
			"X-Poll-Interval: 60\r\n"
			"X-Ratelimit-Limit: 5000\r\n"
			"X-Ratelimit-Remaining: 4998\r\n"
			"X-Ratelimit-Reset: 1793751790\r\n"
			"X-Ratelimit-Resource: core\r\n"
			"X-Ratelimit-Used: 2\r\n"
			"\r\n"
		};
		const auto kept = parse_gh_cli_response(output, false, true);
		expect(kept.has_value() && kept->Status == 304 && kept->Body.empty(), "gh surfaces the 304 and its headers");
		const auto read = read_notifications_response(*kept);
		expect(read.Poll.Outcome == core::activity_poll_outcome::unchanged && read.Poll.PollInterval == 60U && read.Poll.RateRemaining == 4998U && read.Poll.RateReset == 1793751790,
			   "a 304 is unchanged, with the poll interval and the rate limit");
		expect(read.LastModified == date, "and its date");

		const auto other = parse_gh_cli_response(output);
		expect(other.has_value() && std::ranges::none_of(other->Headers, [](const http_header& aHeader) { return aHeader.Name == "Last-Modified" || aHeader.Name == "X-Poll-Interval"; }),
			   "every other read keeps its projection without them");
	}

	auto response(const int aStatus, std::string aBody = { }, std::vector<http_header> aHeaders = { }) -> wire_response
	{
		return wire_response{ .Status = aStatus, .Headers = std::move(aHeaders), .Body = std::move(aBody), .Completeness = page_completeness::complete };
	}

	void test_answers()
	{
		const auto body = std::string{ R"([
			{"id":"101","unread":true,"reason":"author","updated_at":"2026-11-03T12:00:00Z","last_read_at":null,
			 "subject":{"title":"Secret customer name","url":"https://api.github.com/repos/octo/widgets/pulls/7","latest_comment_url":"https://api.github.com/repos/octo/widgets/issues/comments/555","type":"PullRequest"},
			 "repository":{"id":1,"full_name":"octo/widgets","owner":{"login":"octo"},"private":true}},
			{"id":"102","unread":true,"reason":"mention","updated_at":"2026-11-03T12:01:00Z","last_read_at":"2026-11-01T00:00:00Z",
			 "subject":{"title":"x","url":"https://ghe.example/api/v3/repos/octo/widgets/pulls/8","latest_comment_url":"https://ghe.example/api/v3/repos/octo/widgets/pulls/comments/77","type":"PullRequest"},
			 "repository":{"full_name":"octo/widgets"}},
			{"id":"103","unread":true,"reason":"comment","updated_at":"2026-11-03T12:02:00Z","last_read_at":null,
			 "subject":{"title":"an issue","url":"https://api.github.com/repos/octo/widgets/issues/9","latest_comment_url":null,"type":"Issue"},
			 "repository":{"full_name":"octo/widgets"}},
			{"id":"104","unread":false,"reason":"review_requested","updated_at":"2026-11-03T12:03:00Z","last_read_at":"2026-11-03T12:03:00Z",
			 "subject":{"title":"y","url":"https://api.github.com/repos/octo/widgets/pulls/10","latest_comment_url":"https://api.github.com/repos/octo/widgets/pulls/10/reviews/88","type":"PullRequest"},
			 "repository":{"full_name":"octo/widgets"}},
			{"id":"105","unread":true,"reason":"author","updated_at":"2026-11-03T12:04:00Z","last_read_at":null,
			 "subject":{"title":"z","url":"https://api.github.com/repos/other/place/pulls/11","latest_comment_url":null,"type":"PullRequest"},
			 "repository":{"full_name":"octo/widgets"}}
		])" };
		const auto read = read_notifications_response(response(200, body, { { .Name = "Last-Modified", .Value = std::string{ date } }, { .Name = "X-Poll-Interval", .Value = "60" } }));
		expect(read.Poll.Outcome == core::activity_poll_outcome::changed && read.Complete && read.LastModified == date, "a 200 lists threads, complete below a full page");
		expect(read.Threads.size() == 3U, "only pull request threads whose subject is their repository's are kept");
		expect(read.Threads[0] == core::activity_thread{ .Id = "101", .UpdatedAt = 1793707200, .Unread = true, .LastReadAt = std::nullopt, .Reason = core::activity_reason::author, .Repository = "octo/widgets", .Number = 7, .Anchor = { .Kind = core::activity_anchor_kind::issue_comment, .Id = 555U } },
			   "a thread is read whole, and nothing of its title");
		expect(read.Threads[1].Reason == core::activity_reason::mention && read.Threads[1].Anchor == core::activity_anchor{ .Kind = core::activity_anchor_kind::review_comment, .Id = 77U } && read.Threads[1].LastReadAt == 1793491200,
			   "a GHES thread, its review comment and last read");
		expect(read.Threads[2].Reason == core::activity_reason::review_requested && !read.Threads[2].Unread && read.Threads[2].Anchor.Kind == core::activity_anchor_kind::review,
			   "a read review request, with its review");

		auto full = std::string{ "[" };
		for (auto index = 0; index < 50; ++index) {
			full += std::string{ index == 0 ? "" : "," } + R"({"id":")" + std::to_string(200 + index)
				+ R"(","unread":true,"reason":"author","updated_at":"2026-11-03T12:00:00Z","subject":{"url":"https://api.github.com/repos/octo/widgets/pulls/1","type":"PullRequest"},"repository":{"full_name":"octo/widgets"}})";
		}
		full += "]";
		const auto page = read_notifications_response(response(200, full));
		expect(page.Poll.Outcome == core::activity_poll_outcome::changed && !page.Complete && page.Threads.size() == 50U, "a full page may not list everything");

		expect(read_notifications_response(response(200, "{not json")).Poll.Outcome == core::activity_poll_outcome::failed, "a 200 that does not parse fails");
		expect(read_notifications_response(response(200, R"({"message":"x"})")).Poll.Outcome == core::activity_poll_outcome::failed, "a 200 that is not a list fails");
		expect(read_notifications_response(response(403, R"({"message":"Resource not accessible by integration"})", { { .Name = "X-RateLimit-Remaining", .Value = "4000" } })).Poll.Outcome
				   == core::activity_poll_outcome::unsupported,
			   "a GitHub App token is refused the list: unsupported");
		expect(read_notifications_response(response(404, R"({"message":"Not Found"})")).Poll.Outcome == core::activity_poll_outcome::unsupported, "so is a 404");
		const auto limited = read_notifications_response(response(403, R"({"message":"API rate limit exceeded"})", { { .Name = "X-RateLimit-Remaining", .Value = "0" }, { .Name = "X-RateLimit-Reset", .Value = "1793751790" } }));
		expect(limited.Poll.Outcome == core::activity_poll_outcome::rate_limited && limited.Poll.RateReset == 1793751790, "an exhausted rate limit waits for its reset");
		const auto retry = read_notifications_response(response(429, { }, { { .Name = "Retry-After", .Value = "120" } }));
		expect(retry.Poll.Outcome == core::activity_poll_outcome::rate_limited && retry.Poll.RetryAfter == 120U, "a 429 says how long");
		expect(read_notifications_response(response(502)).Poll.Outcome == core::activity_poll_outcome::failed, "a server error fails");
		const auto duplicate = read_notifications_response(response(304, { }, { { .Name = "Last-Modified", .Value = std::string{ date } }, { .Name = "last-modified", .Value = std::string{ date } } }));
		expect(duplicate.LastModified.empty(), "a repeated date is not trusted");
		expect(notification_timestamp("2026-02-30T00:00:00Z") == std::nullopt && notification_timestamp("1970-01-01T00:00:00Z") == 0, "timestamps are exact");
	}
} // namespace

auto main() -> int
{
	test_request();
	test_gh_cli_304();
	test_answers();
	std::cout << "github-notifications-read:pass\n";
	return EXIT_SUCCESS;
}
