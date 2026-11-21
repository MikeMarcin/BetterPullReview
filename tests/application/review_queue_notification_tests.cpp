#include "bpr/application/review_queue_notification.hpp"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto ref() -> core::pull_request_ref
	{
		return core::pull_request_ref{
			.Host       = "github.com",
			.Account    = "reviewer",
			.Repository = "octo/app",
			.Number     = 7,
		};
	}

	// The membership carries an observer rule, and a rule pattern is a path —
	// repository content. It is the nearest thing to hand that must not reach
	// the notification, and the landing model legitimately shows it on screen,
	// so copying that behaviour here is a mistake somebody could plausibly
	// make.
	[[nodiscard]] auto item(const core::attention_bucket aBucket)
		-> core::queue_item
	{
		return core::queue_item{
			.Ref        = ref(),
			.Membership = core::queue_membership{
				.Role        = core::github_role::none,
				.MatchedRule = true,
				.Rule        = core::observer_rule{
					.Kind      = core::observer_rule_kind::path,
					.Pattern   = "src/billing/customer_records.cpp",
					.Excluding = false,
				},
			},
			.Bucket = aBucket,
		};
	}

	constexpr auto every_bucket = std::array{
		core::attention_bucket::initial_review_requested,
		core::attention_bucket::changed_since_my_review,
		core::attention_bucket::new_comments_or_author_replies,
		core::attention_bucket::fixes_awaiting_verification,
		core::attention_bucket::authored_awaiting_reviewer_action,
		core::attention_bucket::observed_with_new_activity,
		core::attention_bucket::quiet,
	};

	// The rule this card exists to hold. Every word a notification carries is
	// read here, and every word is compared against what the review actually
	// contains — a test that only looked for the absence of one string would
	// pass against a notification carrying a different string.
	void test_a_notification_carries_no_repository_content()
	{
		// Things a pull request might plausibly contain, none of which may
		// reach the notification.
		const auto forbidden = std::array<std::string_view, 6>{
			"Fix the ACME outage before Friday", // a title
			"const auto password = secret();", // a source line
			"This is wrong, see line 42", // a comment body
			"octocat", // an author login
			"src/billing/customer_records.cpp", // a path
			"feature/unreleased-pricing", // a branch
		};

		for (const auto bucket : every_bucket) {
			const auto request = application::notification_for(item(bucket), 3U);
			if (!request) {
				continue;
			}
			const auto title = application::notification_title(*request);
			const auto body  = application::notification_body(*request);
			auto       whole = title;
			whole += ' ';
			whole += body;

			for (const auto& secret : forbidden) {
				expect(
					whole.find(secret) == std::string::npos,
					"nothing a pull request contains reaches the notification");
			}

			// Everything the notification does say is accounted for: the
			// repository, the number, the bucket's own fixed sentence, and a
			// count. Reading the whole string rather than searching it for a
			// bad word is what makes this an assertion rather than a hope.
			auto expected = std::string{ request->Ref.Repository };
			expected += " #";
			expected += std::to_string(request->Ref.Number);
			expected += ' ';
			expected += core::explanation(request->Bucket);
			expected += " 3 unread comments.";
			expect(
				whole == expected,
				"and the notification is exactly its identity, its bucket's "
				"sentence, and a count");
		}
	}

	// A title routinely names the customer, the incident, or the unreleased
	// feature. It is the obvious thing to add and the first thing that must not
	// be there.
	void test_the_title_is_identity_only()
	{
		const auto request
			= application::notification_for(item(core::attention_bucket::initial_review_requested), 0U);
		expect(request.has_value(), "the request exists");
		expect(
			application::notification_title(*request) == "octo/app #7",
			"the title is the repository and the number, and nothing else");
	}

	// A count says there is something to read without saying what it is.
	void test_a_count_is_reported_without_content()
	{
		const auto one = application::notification_for(
			item(core::attention_bucket::new_comments_or_author_replies), 1U);
		expect(
			application::notification_body(*one).find("1 unread comment.")
				!= std::string::npos,
			"one comment reads as one");

		const auto many = application::notification_for(
			item(core::attention_bucket::new_comments_or_author_replies), 12U);
		expect(
			application::notification_body(*many).find("12 unread comments.")
				!= std::string::npos,
			"and twelve as twelve");

		const auto none = application::notification_for(
			item(core::attention_bucket::initial_review_requested), 0U);
		expect(
			application::notification_body(*none).find("unread")
				== std::string::npos,
			"with nothing to read, nothing is said about reading");
	}

	// Not every bucket is worth interrupting for. A tool that notified about a
	// quiet review would teach the reviewer to ignore its notifications, after
	// which the ones that matter are ignored too.
	void test_only_buckets_that_ask_something_notify()
	{
		expect(
			!application::warrants_notification(core::attention_bucket::quiet),
			"a quiet review asks nothing and interrupts nobody");
		expect(
			!application::warrants_notification(
				core::attention_bucket::authored_awaiting_reviewer_action),
			"and waiting on somebody else is not the reviewer's to act on");

		expect(
			!application::notification_for(
				 item(core::attention_bucket::quiet), 5U)
				 .has_value(),
			"so no request is built for it, even with comments waiting");

		for (const auto bucket : { core::attention_bucket::initial_review_requested,
								   core::attention_bucket::changed_since_my_review,
								   core::attention_bucket::new_comments_or_author_replies,
								   core::attention_bucket::fixes_awaiting_verification,
								   core::attention_bucket::observed_with_new_activity }) {
			expect(
				application::notification_for(item(bucket), 0U).has_value(),
				"a bucket that asks something does notify");
		}
	}

	// The link is a local reference. A URL would open a browser — the one thing
	// a native review tool exists to avoid — and a link the operating system
	// can hand to any handler is one another application can claim.
	void test_the_link_is_a_local_reference_to_the_right_review()
	{
		const auto request = application::notification_for(
			item(core::attention_bucket::changed_since_my_review), 0U);
		const auto target = application::notification_target(*request);

		expect(target.Ref == ref(), "the link resolves to the review it names");
		expect(
			target.Ref.Host == "github.com" && target.Ref.Account == "reviewer"
				&& target.Ref.Repository == "octo/app"
				&& target.Ref.Number == 7,
			"host, account, repository, and number, all four");
	}

	// Every bucket that notifies says something different. Two notifications
	// reading the same would leave the reviewer unable to tell what changed.
	void test_each_notifying_bucket_reads_differently()
	{
		auto bodies = std::vector<std::string>{ };
		for (const auto bucket : every_bucket) {
			const auto request = application::notification_for(item(bucket), 0U);
			if (!request) {
				continue;
			}
			const auto body = application::notification_body(*request);
			expect(!body.empty(), "every notification says something");
			for (const auto& other : bodies) {
				expect(other != body, "and no two say the same thing");
			}
			bodies.push_back(body);
		}
		expect(bodies.size() == 5U, "five buckets notify");
	}
} // namespace

namespace {
	/// [[BPR-2330]], the owner, 2026-11-20: "Yes, stay quiet." Another
	/// author's draft in Following never notifies; marked ready, its pending
	/// request notifies then, once; with Needs me it notifies as before.
	void test_a_draft_waiting_in_following_stays_quiet()
	{
		using placement   = core::home_draft_placement;
		auto draft        = core::queue_item{ .Ref = ref(), .Membership = { .Role = core::github_role::requested_reviewer }, .Bucket = core::attention_bucket::initial_review_requested };
		draft.Facts.Draft = true;
		for (const auto bucket : every_bucket) {
			auto any   = draft;
			any.Bucket = bucket;
			expect(!application::notification_for(any, 2U).has_value() && !application::notification_active(any, placement::following),
				   "no bucket of another author's draft in Following notifies");
			expect(application::notification_active(any, placement::needs_me) == application::warrants_notification(bucket),
				   "with Needs me, the bucket decides as before");
		}

		const auto session = application::queue_session_id{ .Host = "github.com", .Account = "reviewer", .Generation = 1U };
		auto       active  = std::vector<application::queue_notification_key>{ };
		for (auto refresh = 0; refresh < 3; ++refresh) {
			auto step = application::step_queue_notifications(active, session, std::array{ draft }, placement::following);
			expect(step.Due.empty() && step.Active.empty(), "refresh after refresh, the draft's request stays quiet");
			active = std::move(step.Active);
		}
		auto ready        = draft;
		ready.Facts.Draft = false;
		auto marked       = application::step_queue_notifications(active, session, std::array{ ready }, placement::following);
		expect(marked.Due == std::vector<std::size_t>{ 0U } && marked.Active.size() == 1U, "marked ready, its pending request is due");
		expect(application::notification_for(ready, 0U).has_value(), "and builds a request");
		active = std::move(marked.Active);
		for (auto refresh = 0; refresh < 3; ++refresh) {
			auto step = application::step_queue_notifications(active, session, std::array{ ready }, placement::following);
			expect(step.Due.empty() && step.Active.size() == 1U, "once: later refreshes in the same state do not notify again");
			active = std::move(step.Active);
		}

		const auto needsMe = application::step_queue_notifications({ }, session, std::array{ draft }, placement::needs_me);
		expect(needsMe.Due == std::vector<std::size_t>{ 0U } && application::notification_for(draft, 0U, placement::needs_me).has_value(),
			   "with Needs me, the draft's request notifies as before");

		auto mine            = draft;
		mine.Membership.Role = core::github_role::author;
		mine.Bucket          = core::attention_bucket::new_comments_or_author_replies;
		expect(application::notification_active(mine, placement::following), "comments on my own draft still notify");
		auto ignored    = ready;
		ignored.Ignored = true;
		expect(application::step_queue_notifications({ }, session, std::array{ ignored }, placement::needs_me).Due.empty(), "an ignored row stays quiet");
	}
} // namespace

auto main() -> int
{
	test_a_notification_carries_no_repository_content();
	test_the_title_is_identity_only();
	test_a_count_is_reported_without_content();
	test_only_buckets_that_ask_something_notify();
	test_the_link_is_a_local_reference_to_the_right_review();
	test_each_notifying_bucket_reads_differently();
	test_a_draft_waiting_in_following_stays_quiet();

	std::cout << "review-queue-notification-tests:pass\n";
	return EXIT_SUCCESS;
}
