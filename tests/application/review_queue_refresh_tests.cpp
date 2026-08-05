#include "bpr/application/review_queue_refresh.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {
	using namespace bpr;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto session(
		const std::string_view aAccount    = "reviewer",
		const std::uint64_t    aGeneration = 1U) -> application::queue_session_id
	{
		return application::queue_session_id{
			.Host       = "github.com",
			.Account    = std::string{ aAccount },
			.Generation = aGeneration,
		};
	}

	[[nodiscard]] auto ref(const std::int64_t aNumber = 7)
		-> core::pull_request_ref
	{
		return core::pull_request_ref{
			.Host       = "github.com",
			.Account    = "reviewer",
			.Repository = "octo/app",
			.Number     = aNumber,
		};
	}

	[[nodiscard]] auto answer(
		const application::queue_session_id&    aSession,
		const application::queue_refresh_status aStatus,
		const std::optional<std::string>        aValidator  = std::nullopt,
		const std::optional<std::uint32_t>      aRetryAfter = std::nullopt)
		-> application::queue_refresh_completion
	{
		return application::queue_refresh_completion{
			.Session           = aSession,
			.Ref               = ref(),
			.Status            = aStatus,
			.Validator         = aValidator,
			.Observation       = std::nullopt,
			.RetryAfterSeconds = aRetryAfter,
		};
	}

	// The first request cannot be conditional — there is no validator yet. The
	// second must be, or an unchanged pull request costs a full transfer every
	// time the queue refreshes.
	void test_a_validator_makes_the_next_request_conditional()
	{
		auto scheduler = application::queue_refresh_scheduler{ session() };

		const auto first = scheduler.request_for(ref());
		expect(
			!first.Validator.has_value(),
			"the first request has nothing to be conditional on");

		expect(
			scheduler.apply(answer(
				session(), application::queue_refresh_status::changed, "etag-1"))
				== application::refresh_disposition::applied,
			"the answer is applied");

		const auto second = scheduler.request_for(ref());
		expect(
			second.Validator == std::optional<std::string>{ "etag-1" },
			"the next request carries the validator the answer supplied");
		expect(
			scheduler.statistics().ConditionalAsked == 1U,
			"and exactly one conditional request has been made");
	}

	// An unchanged answer costs nothing further and must not disturb anything.
	void test_unchanged_keeps_the_validator_and_clears_nothing()
	{
		auto scheduler = application::queue_refresh_scheduler{ session() };
		static_cast<void>(scheduler.request_for(ref()));
		static_cast<void>(scheduler.apply(answer(
			session(), application::queue_refresh_status::changed, "etag-1")));

		expect(
			scheduler.apply(
				answer(session(), application::queue_refresh_status::unchanged))
				== application::refresh_disposition::unchanged,
			"an unchanged answer is accepted and changes nothing");
		expect(
			scheduler.validator_for(ref())
				== std::optional<std::string>{ "etag-1" },
			"the validator survives, or the next request would transfer "
			"everything again");
	}

	// The failure this scheduler is really about. A completion asked under a
	// session that has since been replaced must be refused by identity, not by
	// happening to arrive in a convenient order.
	void test_a_completion_from_a_replaced_session_is_refused()
	{
		auto       scheduler = application::queue_refresh_scheduler{ session() };
		const auto issued    = scheduler.request_for(ref());

		scheduler.adopt(session("someone-else", 2U));

		expect(
			scheduler.apply(answer(
				issued.Session,
				application::queue_refresh_status::changed,
				"etag-from-the-old-account"))
				== application::refresh_disposition::stale_session,
			"the answer to the old session's question is refused");
		expect(
			scheduler.statistics().RefusedAsStale == 1U,
			"and the refusal is counted");
		expect(
			!scheduler.validator_for(ref()).has_value(),
			"the stale answer left no validator behind");
	}

	// A stale completion must not be able to change the backoff or the online
	// state either. Those belong to the session that is current, and an answer
	// from a session that is gone knows nothing about it.
	void test_a_stale_completion_changes_nothing_at_all()
	{
		auto       scheduler = application::queue_refresh_scheduler{ session() };
		const auto old       = session();
		scheduler.adopt(session("someone-else", 2U));

		static_cast<void>(scheduler.apply(answer(
			old,
			application::queue_refresh_status::rate_limited,
			std::nullopt,
			600U)));
		expect(
			scheduler.backoff_seconds() == 0U,
			"a stale rate limit does not hold back the current session");

		static_cast<void>(scheduler.apply(
			answer(old, application::queue_refresh_status::offline)));
		expect(
			scheduler.online(),
			"and a stale offline answer does not take it offline");
	}

	// Same generation, different account, is still a different session. An
	// account switch that reused the generation would otherwise let one
	// account's answers land under another's authority.
	void test_the_account_is_part_of_the_session()
	{
		auto scheduler = application::queue_refresh_scheduler{ session("first") };
		scheduler.adopt(session("second"));

		expect(
			scheduler.apply(answer(
				session("first"), application::queue_refresh_status::changed))
				== application::refresh_disposition::stale_session,
			"an answer for another account is refused even at the same "
			"generation");
	}

	// Validators belong to the account that obtained them. Carrying one across
	// would send it under a different authority, and "not modified" would then
	// be an answer about a pull request this account may not be able to read.
	void test_switching_account_discards_validators()
	{
		auto scheduler = application::queue_refresh_scheduler{ session("first") };
		static_cast<void>(scheduler.request_for(ref()));
		static_cast<void>(scheduler.apply(answer(
			session("first"),
			application::queue_refresh_status::changed,
			"etag-1")));
		expect(scheduler.validator_for(ref()).has_value(), "a validator is held");

		scheduler.adopt(session("second"));
		expect(
			!scheduler.validator_for(ref()).has_value(),
			"and is gone once the account changes");
		expect(
			!scheduler.request_for(ref()).Validator.has_value(),
			"so the next request under the new account is unconditional");
	}

	// Backoff doubles and is capped, so a host that keeps refusing is asked
	// less and less often rather than forever more urgently.
	void test_backoff_doubles_and_is_capped()
	{
		auto scheduler = application::queue_refresh_scheduler{ session() };

		auto previous = std::uint32_t{ 0U };
		for (auto attempt = 0; attempt < 12; ++attempt) {
			expect(
				scheduler.apply(answer(
					session(), application::queue_refresh_status::rate_limited))
					== application::refresh_disposition::deferred,
				"a rate-limited answer defers");
			const auto current = scheduler.backoff_seconds();
			expect(current > 0U, "and holds the scheduler back");
			expect(
				current >= previous,
				"each consecutive refusal waits at least as long as the last");
			expect(
				current
					<= application::queue_refresh_scheduler::MaximumBackoffSeconds,
				"and never longer than the cap");
			previous = current;
		}
		expect(
			previous
				== application::queue_refresh_scheduler::MaximumBackoffSeconds,
			"twelve refusals reach the cap");
	}

	// The provider knows when its allowance resets and this does not, so its
	// own figure is preferred over the doubling.
	void test_the_providers_retry_after_is_preferred()
	{
		auto scheduler = application::queue_refresh_scheduler{ session() };
		static_cast<void>(scheduler.apply(answer(
			session(),
			application::queue_refresh_status::secondary_rate_limited,
			std::nullopt,
			42U)));
		expect(
			scheduler.backoff_seconds() == 42U,
			"the provider's figure is used as given");
	}

	// A successful answer clears the hold. Otherwise the queue would stay slow
	// long after the host started answering again.
	void test_a_successful_answer_clears_the_backoff()
	{
		auto scheduler = application::queue_refresh_scheduler{ session() };
		static_cast<void>(scheduler.apply(
			answer(session(), application::queue_refresh_status::rate_limited)));
		expect(scheduler.backoff_seconds() > 0U, "the hold is in force");

		static_cast<void>(scheduler.apply(answer(
			session(), application::queue_refresh_status::changed, "etag-1")));
		expect(scheduler.backoff_seconds() == 0U, "and is released");
	}

	// Offline is a state the reviewer is told about, not a silent stall.
	void test_offline_is_reported_and_recovered()
	{
		auto scheduler = application::queue_refresh_scheduler{ session() };
		expect(scheduler.online(), "a new scheduler believes it is online");

		expect(
			scheduler.apply(
				answer(session(), application::queue_refresh_status::offline))
				== application::refresh_disposition::deferred,
			"an offline answer defers");
		expect(!scheduler.online(), "and the state says so");

		static_cast<void>(scheduler.apply(answer(
			session(), application::queue_refresh_status::changed, "etag-1")));
		expect(scheduler.online(), "an answer that arrives restores it");
	}

	// Cancelling publishes nothing and is not the host's fault. Pushing it into
	// backoff would let a reviewer switching quickly between reviews slow their
	// own queue down.
	void test_cancellation_publishes_nothing_and_does_not_back_off()
	{
		auto scheduler = application::queue_refresh_scheduler{ session() };
		expect(
			scheduler.apply(
				answer(session(), application::queue_refresh_status::cancelled))
				== application::refresh_disposition::unchanged,
			"a cancelled request changes nothing");
		expect(
			scheduler.backoff_seconds() == 0U && scheduler.online(),
			"and is not treated as the host refusing");
		expect(
			scheduler.statistics().Applied == 0U,
			"nothing was applied");
	}

	// Losing permission or having the pull request transferred away must be an
	// explicit state. A pull request that simply vanished would leave the
	// reviewer wondering whether they imagined it.
	void test_losing_access_withdraws_the_item_explicitly()
	{
		for (const auto status :
			 { application::queue_refresh_status::forbidden,
			   application::queue_refresh_status::gone }) {
			auto scheduler = application::queue_refresh_scheduler{ session() };
			static_cast<void>(scheduler.apply(answer(
				session(),
				application::queue_refresh_status::changed,
				"etag-1")));

			expect(
				scheduler.apply(answer(session(), status))
					== application::refresh_disposition::withdrawn,
				"the item is withdrawn rather than quietly dropped");
			expect(
				!scheduler.validator_for(ref()).has_value(),
				"and nothing is kept for a pull request that is gone");
		}
	}

	// Every status has a name, and no two share one. A log that reported the
	// same word for a rate limit and an outage would send the reader to the
	// wrong place.
	void test_every_status_is_named_distinctly()
	{
		const auto statuses = {
			application::queue_refresh_status::changed,
			application::queue_refresh_status::unchanged,
			application::queue_refresh_status::offline,
			application::queue_refresh_status::rate_limited,
			application::queue_refresh_status::secondary_rate_limited,
			application::queue_refresh_status::cancelled,
			application::queue_refresh_status::forbidden,
			application::queue_refresh_status::gone,
		};
		auto seen = std::vector<std::string_view>{ };
		for (const auto status : statuses) {
			const auto text = application::name(status);
			expect(!text.empty(), "every status has a name");
			for (const auto& other : seen) {
				expect(other != text, "two statuses never share a name");
			}
			seen.push_back(text);
		}

		const auto dispositions = {
			application::refresh_disposition::applied,
			application::refresh_disposition::unchanged,
			application::refresh_disposition::stale_session,
			application::refresh_disposition::deferred,
			application::refresh_disposition::withdrawn,
		};
		auto shown = std::vector<std::string_view>{ };
		for (const auto disposition : dispositions) {
			const auto text = application::name(disposition);
			expect(!text.empty(), "every disposition has a name");
			for (const auto& other : shown) {
				expect(other != text, "two dispositions never share a name");
			}
			shown.push_back(text);
		}
	}
} // namespace

auto main() -> int
{
	test_a_validator_makes_the_next_request_conditional();
	test_unchanged_keeps_the_validator_and_clears_nothing();
	test_a_completion_from_a_replaced_session_is_refused();
	test_a_stale_completion_changes_nothing_at_all();
	test_the_account_is_part_of_the_session();
	test_switching_account_discards_validators();
	test_backoff_doubles_and_is_capped();
	test_the_providers_retry_after_is_preferred();
	test_a_successful_answer_clears_the_backoff();
	test_offline_is_reported_and_recovered();
	test_cancellation_publishes_nothing_and_does_not_back_off();
	test_losing_access_withdraws_the_item_explicitly();
	test_every_status_is_named_distinctly();

	std::cout << "review-queue-refresh-tests:pass\n";
	return EXIT_SUCCESS;
}
