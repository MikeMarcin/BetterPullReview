#include "bpr/application/review_queue_enrollment.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>
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

	[[nodiscard]] auto ref(const std::int64_t aNumber) -> core::pull_request_ref
	{
		return core::pull_request_ref{
			.Host       = "github.com",
			.Account    = "reviewer",
			.Repository = "octo/app",
			.Number     = aNumber,
		};
	}

	[[nodiscard]] auto path_rule(const std::string_view aPattern)
		-> core::observer_rule
	{
		return core::observer_rule{
			.Kind      = core::observer_rule_kind::path,
			.Pattern   = std::string{ aPattern },
			.Excluding = false,
		};
	}

	[[nodiscard]] auto by_rule(const std::string_view aPattern)
		-> core::queue_membership
	{
		return core::queue_membership{
			.Role        = core::github_role::none,
			.MatchedRule = true,
			.Rule        = path_rule(aPattern),
		};
	}

	[[nodiscard]] auto by_role(const core::github_role aRole)
		-> core::queue_membership
	{
		return core::queue_membership{
			.Role        = aRole,
			.MatchedRule = false,
			.Rule        = { },
		};
	}

	[[nodiscard]] auto withdrawal(
		const core::pull_request_ref&           aRef,
		const application::queue_refresh_status aStatus)
		-> application::queue_refresh_completion
	{
		return application::queue_refresh_completion{
			.Session = { },
			.Ref     = aRef,
			.Status  = aStatus,
		};
	}

	// The reason a reviewer is watching does not stop being true when the path
	// that matched leaves the pull request. It is why they wanted to see the
	// change in the first place, and a queue that recomputed membership on
	// every refresh would drop exactly the pull requests a path rule exists to
	// catch.
	void test_enrollment_survives_the_path_that_matched()
	{
		auto records = application::queue_enrollment_register{ };
		records.enroll(ref(7), by_rule("src/parser"));

		// A later refresh sees a pull request that no longer touches the path.
		// Nothing re-enrolls it, and nothing needs to.
		const auto retained = records.record_for(ref(7));
		expect(retained.has_value(), "the enrollment is retained");
		expect(retained->present(), "and is still in the queue");
		expect(
			retained->Membership.MatchedRule
				&& retained->Membership.Rule.Pattern == "src/parser",
			"with the rule that brought it in, so the reviewer can still see "
			"why");

		const auto queue = records.project({ });
		expect(queue.size() == 1U, "and it is projected");
	}

	// Both reasons are kept. A rule match must not overwrite a role, or
	// removing the rule would silently drop a pull request GitHub assigned; a
	// role must not overwrite the rule, or the reviewer loses the answer to why
	// an unfamiliar item is in their queue.
	void test_re_enrolling_keeps_both_reasons()
	{
		auto records = application::queue_enrollment_register{ };
		records.enroll(ref(7), by_rule("src/parser"));
		records.enroll(ref(7), by_role(core::github_role::requested_reviewer));

		const auto both = records.record_for(ref(7));
		expect(both.has_value(), "one record, not two");
		expect(records.records().size() == 1U, "the item was not duplicated");
		expect(
			both->Membership.Role == core::github_role::requested_reviewer,
			"the role that arrived is kept");
		expect(
			both->Membership.MatchedRule
				&& both->Membership.Rule.Pattern == "src/parser",
			"and so is the rule that was already there");

		records.enroll(ref(7), by_rule("src/other"));
		const auto after = records.record_for(ref(7));
		expect(
			after->Membership.Role == core::github_role::requested_reviewer,
			"a later rule match does not clear the role");
		expect(
			after->Membership.Rule.Pattern == "src/parser",
			"and does not replace the rule that first brought it in");
	}

	// Forbidden and gone are different facts with different next steps: one is
	// a permission the reviewer may be able to ask for, the other is a pull
	// request that is not there any more. Collapsing them would send somebody
	// to the wrong person.
	void test_losing_access_and_being_gone_are_distinguished()
	{
		auto records = application::queue_enrollment_register{ };
		records.enroll(ref(7), by_role(core::github_role::reviewer));
		records.enroll(ref(8), by_role(core::github_role::reviewer));

		records.observe(
			withdrawal(ref(7), application::queue_refresh_status::forbidden),
			application::refresh_disposition::withdrawn);
		records.observe(
			withdrawal(ref(8), application::queue_refresh_status::gone),
			application::refresh_disposition::withdrawn);

		expect(
			records.record_for(ref(7))->Departed
				== application::enrollment_departure::access_lost,
			"a forbidden pull request is recorded as access lost");
		expect(
			records.record_for(ref(8))->Departed
				== application::enrollment_departure::gone,
			"and one that is gone is recorded as gone");
	}

	// A departure is retained so the reviewer can be told what happened, and
	// dropped from the queue because a closed pull request is not asking for
	// attention.
	void test_a_departed_item_is_retained_but_not_projected()
	{
		auto records = application::queue_enrollment_register{ };
		records.enroll(ref(7), by_role(core::github_role::reviewer));
		records.enroll(ref(8), by_role(core::github_role::reviewer));
		records.depart(ref(7), application::enrollment_departure::closed);

		expect(
			records.records().size() == 2U,
			"the closed item is still retained");
		expect(
			records.record_for(ref(7))->Departed
				== application::enrollment_departure::closed,
			"with the reason it left");

		const auto queue = records.project({ });
		expect(queue.size() == 1U, "but the queue shows only what is present");
		expect(queue.front().Ref == ref(8), "and it is the right one");
	}

	// Reopening is enrolling again. The reviewer gets the pull request back
	// with the reason they had it, rather than a second record beside the first.
	void test_reopening_restores_the_same_record()
	{
		auto records = application::queue_enrollment_register{ };
		records.enroll(ref(7), by_rule("src/parser"));
		records.depart(ref(7), application::enrollment_departure::closed);
		records.enroll(ref(7), by_rule("src/parser"));

		expect(records.records().size() == 1U, "one record, not two");
		expect(records.record_for(ref(7))->present(), "and it is back");
		expect(
			records.record_for(ref(7))->Membership.Rule.Pattern == "src/parser",
			"with the reason it had");
	}

	// Only a withdrawal moves an item out. A deferred or refused refresh says
	// nothing about whether the pull request is still there, and treating it as
	// a departure would empty the queue during an outage.
	void test_only_a_withdrawal_moves_an_item_out()
	{
		const auto dispositions = std::array{
			application::refresh_disposition::applied,
			application::refresh_disposition::unchanged,
			application::refresh_disposition::stale_session,
			application::refresh_disposition::deferred,
		};

		for (const auto disposition : dispositions) {
			auto records = application::queue_enrollment_register{ };
			records.enroll(ref(7), by_role(core::github_role::reviewer));
			records.observe(
				withdrawal(ref(7), application::queue_refresh_status::gone),
				disposition);
			expect(
				records.record_for(ref(7))->present(),
				"a disposition other than withdrawn leaves the item alone");
		}
	}

	// Every departure has a distinct name and a sentence the reviewer reads. A
	// pull request that vanished with no reason would leave them wondering
	// whether they imagined it.
	void test_every_departure_names_and_explains_itself()
	{
		const auto departures = std::array{
			application::enrollment_departure::none,
			application::enrollment_departure::closed,
			application::enrollment_departure::access_lost,
			application::enrollment_departure::gone,
			application::enrollment_departure::removed_by_reviewer,
		};

		auto names        = std::vector<std::string_view>{ };
		auto explanations = std::vector<std::string_view>{ };
		for (const auto departure : departures) {
			const auto text  = application::name(departure);
			const auto sense = application::explanation(departure);
			expect(!text.empty(), "every departure has a name");
			expect(!sense.empty(), "every departure has an explanation");
			for (const auto& other : names) {
				expect(other != text, "two departures never share a name");
			}
			for (const auto& other : explanations) {
				expect(
					other != sense, "two departures never share an explanation");
			}
			names.push_back(text);
			explanations.push_back(sense);
		}
	}

	// The projection is the core's, ordered by the core's rule. The host
	// decides what is retained; it does not decide what is urgent.
	void test_the_projection_is_ordered_by_attention()
	{
		auto records = application::queue_enrollment_register{ };
		records.enroll(ref(7), by_role(core::github_role::participant));
		records.enroll(ref(8), by_role(core::github_role::requested_reviewer));

		const auto activity = std::array{
			core::queue_activity_observation{ },
			core::queue_activity_observation{ },
		};
		const auto queue = records.project(activity);

		expect(queue.size() == 2U, "both are projected");
		expect(
			queue.front().Bucket
				== core::attention_bucket::initial_review_requested,
			"and the unanswered request comes first");
		expect(queue.front().Ref == ref(8), "which is the one that was asked");
	}

	// An item the host has not read yet is quiet, not guessed at. Inventing an
	// observation would put a pull request in a bucket on no evidence.
	void test_an_unobserved_item_is_quiet()
	{
		auto records = application::queue_enrollment_register{ };
		records.enroll(ref(7), by_role(core::github_role::reviewer));

		const auto queue = records.project({ });
		expect(queue.size() == 1U, "it is still in the queue");
		expect(
			queue.front().Bucket == core::attention_bucket::quiet,
			"and asks for nothing until something has been read about it");
	}
} // namespace

auto main() -> int
{
	test_enrollment_survives_the_path_that_matched();
	test_re_enrolling_keeps_both_reasons();
	test_losing_access_and_being_gone_are_distinguished();
	test_a_departed_item_is_retained_but_not_projected();
	test_reopening_restores_the_same_record();
	test_only_a_withdrawal_moves_an_item_out();
	test_every_departure_names_and_explains_itself();
	test_the_projection_is_ordered_by_attention();
	test_an_unobserved_item_is_quiet();

	std::cout << "review-queue-enrollment-tests:pass\n";
	return EXIT_SUCCESS;
}
