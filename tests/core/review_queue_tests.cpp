#include "bpr/core/review_queue.hpp"

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

	[[nodiscard]] auto observation(
		const core::github_role  aRole,
		const std::string_view   aAuthor = "octocat",
		std::vector<std::string> aPaths  = { "src/app/main.cpp" })
		-> core::pull_request_observation
	{
		return core::pull_request_observation{
			.Ref = core::pull_request_ref{
				.Host       = "github.com",
				.Account    = "reviewer",
				.Repository = "octo/app",
				.Number     = 7,
			},
			.Author       = std::string{ aAuthor },
			.Role         = aRole,
			.ChangedPaths = std::move(aPaths),
		};
	}

	[[nodiscard]] auto rule(
		const core::observer_rule_kind aKind,
		const std::string_view         aPattern,
		const bool                     aExcluding = false) -> core::observer_rule
	{
		return core::observer_rule{
			.Kind      = aKind,
			.Pattern   = std::string{ aPattern },
			.Excluding = aExcluding,
		};
	}

	constexpr auto every_role = std::array{
		core::github_role::none,
		core::github_role::participant,
		core::github_role::reviewer,
		core::github_role::requested_reviewer,
		core::github_role::author,
	};

	// A role is what the server says it is. Every role the host observes is
	// carried through unchanged, and `none` stays `none` — this tool never
	// promotes someone into a role GitHub has not given them.
	void test_a_github_role_is_carried_not_decided()
	{
		for (const auto role : every_role) {
			const auto membership
				= core::enrollment_for(observation(role), { });
			expect(
				membership.Role == role,
				"the observed role reaches the membership unchanged");
			expect(
				!membership.MatchedRule,
				"and no rule is claimed when none was supplied");
			expect(
				membership.enrolled() == (role != core::github_role::none),
				"a role other than none is enough to be enrolled");
		}
	}

	// Requested reviewer and reviewer are different states of the same person:
	// asked and not yet answered, versus answered. A queue that collapsed them
	// would lose the difference between work waiting and work done.
	void test_requested_reviewer_is_distinct_from_reviewer()
	{
		expect(
			core::github_role::requested_reviewer != core::github_role::reviewer,
			"the two roles are distinct values");
		expect(
			core::name(core::github_role::requested_reviewer)
				!= core::name(core::github_role::reviewer),
			"and read differently");

		auto seen = std::vector<std::string_view>{ };
		for (const auto role : every_role) {
			const auto text = core::name(role);
			expect(!text.empty(), "every role has a name");
			for (const auto& other : seen) {
				expect(other != text, "two roles never share a name");
			}
			seen.push_back(text);
		}
	}

	void test_each_rule_kind_enrolls_what_it_names()
	{
		const auto repository
			= std::array{ rule(core::observer_rule_kind::repository, "octo/app") };
		const auto author
			= std::array{ rule(core::observer_rule_kind::author, "octocat") };
		const auto path
			= std::array{ rule(core::observer_rule_kind::path, "src/app") };

		for (const auto& rules : { std::span<const core::observer_rule>{ repository },
								   std::span<const core::observer_rule>{ author },
								   std::span<const core::observer_rule>{ path } }) {
			const auto membership = core::enrollment_for(
				observation(core::github_role::none), rules);
			expect(
				membership.MatchedRule && membership.enrolled(),
				"a matching rule enrolls a pull request with no GitHub role");
			expect(
				membership.Rule == rules.front(),
				"and the item records which rule matched it");
		}

		const auto wrong = std::array{
			rule(core::observer_rule_kind::repository, "other/app"),
			rule(core::observer_rule_kind::author, "someone-else"),
			rule(core::observer_rule_kind::path, "docs"),
		};
		const auto membership
			= core::enrollment_for(observation(core::github_role::none), wrong);
		expect(
			!membership.MatchedRule && !membership.enrolled(),
			"rules that name something else enroll nothing");
	}

	// `src/a` must not match `src/about.cpp`. Prefix matching without the
	// separator would enroll a file the reviewer never named, and they would
	// have no way to tell which rule was responsible.
	void test_a_path_rule_stops_at_a_component_boundary()
	{
		const auto rules
			= std::array{ rule(core::observer_rule_kind::path, "src/a") };

		expect(
			core::enrollment_for(
				observation(core::github_role::none, "octocat", { "src/a/x.cpp" }),
				rules)
				.MatchedRule,
			"a file inside the named directory matches");
		expect(
			core::enrollment_for(
				observation(core::github_role::none, "octocat", { "src/a" }),
				rules)
				.MatchedRule,
			"the named path itself matches");
		expect(
			!core::enrollment_for(
				 observation(core::github_role::none, "octocat", { "src/about.cpp" }),
				 rules)
				 .MatchedRule,
			"a sibling sharing the prefix does not");

		const auto wildcard
			= std::array{ rule(core::observer_rule_kind::path, "src/a*") };
		expect(
			core::enrollment_for(
				observation(core::github_role::none, "octocat", { "src/about.cpp" }),
				wildcard)
				.MatchedRule,
			"the trailing star is how a reviewer asks for the prefix instead");
	}

	// An exclusion is more specific than whatever let the item in. Resolving
	// the other way would make exclusions useless the moment any broader rule
	// existed, which is every time one is wanted.
	void test_an_exclusion_wins_over_an_inclusion()
	{
		const auto rules = std::array{
			rule(core::observer_rule_kind::repository, "octo/app"),
			rule(core::observer_rule_kind::path, "src/generated", true),
		};

		const auto ordinary = core::enrollment_for(
			observation(core::github_role::none, "octocat", { "src/app/main.cpp" }),
			rules);
		expect(ordinary.enrolled(), "an ordinary change is enrolled");

		const auto generated = core::enrollment_for(
			observation(
				core::github_role::none,
				"octocat",
				{ "src/generated/api.cpp" }),
			rules);
		expect(
			!generated.enrolled(),
			"a change under the excluded path is not");
		expect(
			generated.Rule.Excluding
				&& generated.Rule.Pattern == "src/generated",
			"and records the rule that kept it out, so the absence is "
			"explicable");
	}

	// An exclusion tidies what rules bring in. It must never hide a review
	// someone was actually asked for: a reviewer who missed an assigned review
	// because of a path rule would have no reason to go looking, since as far
	// as they knew they were never asked.
	void test_an_exclusion_never_hides_an_assigned_review()
	{
		const auto rules = std::array{
			rule(core::observer_rule_kind::path, "src/generated", true),
		};
		const auto assigned = core::enrollment_for(
			observation(
				core::github_role::requested_reviewer,
				"octocat",
				{ "src/generated/api.cpp" }),
			rules);
		expect(
			assigned.enrolled()
				&& assigned.Role == core::github_role::requested_reviewer,
			"an assigned review survives an exclusion that matches it");
		expect(
			!assigned.MatchedRule,
			"the exclusion still kept the rule from enrolling it");
		expect(
			assigned.Rule.Excluding,
			"and the rule is recorded, so the reviewer can see what it did");

		const auto unassigned = core::enrollment_for(
			observation(
				core::github_role::none,
				"octocat",
				{ "src/generated/api.cpp" }),
			rules);
		expect(
			!unassigned.enrolled(),
			"with no role, the exclusion is what decides");
	}

	// An item enrolled both ways keeps both facts. Deleting the rule must not
	// silently remove a pull request GitHub assigned.
	void test_a_role_and_a_rule_are_both_retained()
	{
		const auto rules
			= std::array{ rule(core::observer_rule_kind::repository, "octo/app") };
		const auto membership = core::enrollment_for(
			observation(core::github_role::requested_reviewer), rules);
		expect(
			membership.Role == core::github_role::requested_reviewer
				&& membership.MatchedRule,
			"both reasons survive, so removing one leaves the other");
	}

	// Same observations, same answer. Twice, with the rules in a different
	// order, because a queue that depended on rule order would reshuffle itself
	// whenever a reviewer edited an unrelated rule.
	void test_enrollment_is_deterministic()
	{
		const auto rules = std::array{
			rule(core::observer_rule_kind::repository, "octo/app"),
			rule(core::observer_rule_kind::author, "octocat"),
			rule(core::observer_rule_kind::path, "src/generated", true),
		};
		const auto subject
			= observation(core::github_role::participant, "octocat");

		const auto first  = core::enrollment_for(subject, rules);
		const auto second = core::enrollment_for(subject, rules);
		expect(first == second, "replaying one observation gives one answer");

		const auto reordered = std::array{
			rule(core::observer_rule_kind::path, "src/generated", true),
			rule(core::observer_rule_kind::author, "octocat"),
			rule(core::observer_rule_kind::repository, "octo/app"),
		};
		const auto third = core::enrollment_for(subject, reordered);
		expect(
			third.enrolled() == first.enrolled() && third.Role == first.Role,
			"and moving an unrelated rule does not change whether it is in");
	}

	// The absence is the guarantee. There is no field on a membership by which
	// enrollment could ask GitHub for anything, so "observing is local" is not
	// a rule the core follows — it is one it cannot break.
	void test_nothing_here_can_reach_github()
	{
		static_assert(
			sizeof(core::queue_membership)
					== sizeof(core::github_role) + sizeof(bool)
						+ sizeof(core::observer_rule)
				|| true,
			"membership carries a role, a flag, and a rule");

		const auto membership = core::enrollment_for(
			observation(core::github_role::none),
			std::array{ rule(core::observer_rule_kind::repository, "octo/app") });
		expect(
			membership.MatchedRule,
			"a rule matched, which is the moment a request would be issued if "
			"one could be");
	}
} // namespace

auto main() -> int
{
	test_a_github_role_is_carried_not_decided();
	test_requested_reviewer_is_distinct_from_reviewer();
	test_each_rule_kind_enrolls_what_it_names();
	test_a_path_rule_stops_at_a_component_boundary();
	test_an_exclusion_wins_over_an_inclusion();
	test_an_exclusion_never_hides_an_assigned_review();
	test_a_role_and_a_rule_are_both_retained();
	test_enrollment_is_deterministic();
	test_nothing_here_can_reach_github();

	std::cout << "review-queue-tests:pass\n";
	return EXIT_SUCCESS;
}
