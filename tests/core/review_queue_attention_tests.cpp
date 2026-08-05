#include "bpr/core/review_queue_attention.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
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

	[[nodiscard]] auto membership(
		const core::github_role aRole,
		const bool              aMatchedRule = false) -> core::queue_membership
	{
		return core::queue_membership{
			.Role        = aRole,
			.MatchedRule = aMatchedRule,
			.Rule        = { },
		};
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

	constexpr auto every_bucket = std::array{
		core::attention_bucket::initial_review_requested,
		core::attention_bucket::changed_since_my_review,
		core::attention_bucket::new_comments_or_author_replies,
		core::attention_bucket::fixes_awaiting_verification,
		core::attention_bucket::authored_awaiting_reviewer_action,
		core::attention_bucket::observed_with_new_activity,
		core::attention_bucket::quiet,
	};

	// Seven buckets, and every one of them is reachable. A bucket nothing can
	// land in is a promise the queue does not keep.
	void test_every_bucket_is_reachable()
	{
		const auto cases = std::array<
			std::pair<core::attention_bucket, core::queue_item>,
			7>{ {
			{ core::attention_bucket::initial_review_requested,
			  { .Membership = membership(core::github_role::requested_reviewer) } },
			{ core::attention_bucket::changed_since_my_review,
			  { .Membership = membership(core::github_role::reviewer) } },
			{ core::attention_bucket::new_comments_or_author_replies,
			  { .Membership = membership(core::github_role::participant) } },
			{ core::attention_bucket::fixes_awaiting_verification,
			  { .Membership = membership(core::github_role::reviewer) } },
			{ core::attention_bucket::authored_awaiting_reviewer_action,
			  { .Membership = membership(core::github_role::author) } },
			{ core::attention_bucket::observed_with_new_activity,
			  { .Membership = membership(core::github_role::none, true) } },
			{ core::attention_bucket::quiet,
			  { .Membership = membership(core::github_role::participant) } },
		} };

		const auto activities = std::array<core::queue_activity_observation, 7>{ {
			{ },
			{ .ReviewedByMe = true, .HeadMovedSinceMyReview = true },
			{ .UnseenComments = 3U },
			{ .DefectsAwaitingVerification = 1U },
			{ .ReviewersAwaited = 2U },
			{ .ObservedActivity = true },
			{ },
		} };

		for (auto index = std::size_t{ 0U }; index < cases.size(); ++index) {
			const auto bucket = core::bucket_for(
				cases[index].second.Membership, activities[index]);
			expect(
				bucket == cases[index].first,
				"each bucket is reached by the state it is named for");
		}
	}

	// Being asked and not having answered is the only state where somebody is
	// blocked on this reviewer having done nothing at all, so it outranks
	// everything that could also be true at the same time.
	void test_an_unanswered_request_outranks_everything()
	{
		const auto crowded = core::queue_activity_observation{
			.ReviewedByMe                = false,
			.HeadMovedSinceMyReview      = true,
			.UnseenComments              = 9U,
			.DefectsAwaitingVerification = 4U,
			.ReviewersAwaited            = 2U,
			.ObservedActivity            = true,
		};
		expect(
			core::bucket_for(
				membership(core::github_role::requested_reviewer, true), crowded)
				== core::attention_bucket::initial_review_requested,
			"an unanswered request wins against every other signal at once");
	}

	// Waiting on somebody else is not work this reviewer can do. It must not
	// displace work they can.
	void test_authored_never_displaces_work_the_reviewer_can_do()
	{
		const auto waiting = core::queue_activity_observation{
			.UnseenComments   = 1U,
			.ReviewersAwaited = 3U,
		};
		expect(
			core::bucket_for(membership(core::github_role::author), waiting)
				== core::attention_bucket::new_comments_or_author_replies,
			"something to read outranks waiting on a reviewer");

		const auto onlyWaiting
			= core::queue_activity_observation{ .ReviewersAwaited = 3U };
		expect(
			core::bucket_for(membership(core::github_role::author), onlyWaiting)
				== core::attention_bucket::authored_awaiting_reviewer_action,
			"with nothing to read, the wait is what the item is about");
	}

	// An item GitHub put in front of the reviewer is not "observed". Saying so
	// would hide that they were asked.
	void test_observed_is_only_for_items_no_role_brought_in()
	{
		const auto activity
			= core::queue_activity_observation{ .ObservedActivity = true };

		expect(
			core::bucket_for(membership(core::github_role::none, true), activity)
				== core::attention_bucket::observed_with_new_activity,
			"a rule-enrolled item with activity is observed");
		expect(
			core::bucket_for(
				membership(core::github_role::participant, true), activity)
				!= core::attention_bucket::observed_with_new_activity,
			"an item with a GitHub role is not, even when a rule also matched");
	}

	// A name says where an item is. It does not say what is being asked, and a
	// queue that only labelled would leave the reviewer to guess.
	void test_every_bucket_names_and_explains_itself_distinctly()
	{
		auto names        = std::vector<std::string_view>{ };
		auto explanations = std::vector<std::string_view>{ };
		for (const auto bucket : every_bucket) {
			const auto text  = core::name(bucket);
			const auto sense = core::explanation(bucket);
			expect(!text.empty(), "every bucket has a name");
			expect(!sense.empty(), "every bucket has an explanation");
			for (const auto& other : names) {
				expect(other != text, "two buckets never share a name");
			}
			for (const auto& other : explanations) {
				expect(other != sense, "two buckets never share an explanation");
			}
			names.push_back(text);
			explanations.push_back(sense);
		}
	}

	// The enumeration order is the order of attention, and the queue follows
	// it. If the two disagreed, every surface reading the enumeration would
	// present a different order from the one the queue produced.
	void test_the_queue_is_ordered_by_attention()
	{
		auto items = std::vector<core::queue_item>{ };
		for (auto index = every_bucket.size(); index > 0U; --index) {
			items.push_back(core::queue_item{
				.Ref        = ref(static_cast<std::int64_t>(index)),
				.Membership = membership(core::github_role::participant),
				.Bucket     = every_bucket[index - 1U],
			});
		}

		const auto ordered = core::ordered_queue(items);
		expect(ordered.size() == items.size(), "nothing is lost in ordering");
		for (auto index = std::size_t{ 1U }; index < ordered.size(); ++index) {
			expect(
				ordered[index - 1U].Bucket <= ordered[index].Bucket,
				"the queue runs from most to least urgent");
		}
		expect(
			ordered.front().Bucket
				== core::attention_bucket::initial_review_requested,
			"and starts where nobody has looked at all");
	}

	// The order is total, not merely grouped. A queue ordered by bucket alone
	// leaves items sharing a bucket wherever the host happened to read them,
	// and a queue that reshuffles between refreshes is one a reviewer cannot
	// keep their place in.
	//
	// Enough items that a sort actually has to partition, and supplied in
	// several different orders, because an assertion that passes only because
	// the input was small is not an assertion.
	void test_the_order_is_total_whatever_order_the_host_supplies()
	{
		constexpr auto count = std::int64_t{ 64 };

		const auto build = [](const bool aReversed) {
			auto items = std::vector<core::queue_item>{ };
			for (auto index = std::int64_t{ 0 }; index < count; ++index) {
				const auto number = aReversed ? count - 1 - index : index;
				items.push_back(core::queue_item{
					.Ref        = ref(number),
					.Membership = membership(core::github_role::participant),
					// Two buckets only, so most items share one and the
					// tiebreak is what decides.
					.Bucket = number % 2 == 0
						? core::attention_bucket::new_comments_or_author_replies
						: core::attention_bucket::quiet,
				});
			}
			return items;
		};

		const auto ascending  = core::ordered_queue(build(false));
		const auto descending = core::ordered_queue(build(true));
		expect(
			ascending == descending,
			"the same items in a different supply order give the same queue");

		auto rotated = build(false);
		std::rotate(rotated.begin(), rotated.begin() + 17, rotated.end());
		expect(
			core::ordered_queue(rotated) == ascending,
			"and so does any other supply order");

		for (auto index = std::size_t{ 1U }; index < ascending.size(); ++index) {
			const auto& previous = ascending[index - 1U];
			const auto& current  = ascending[index];
			expect(
				previous.Bucket < current.Bucket
					|| (previous.Bucket == current.Bucket
						&& previous.Ref < current.Ref),
				"every adjacent pair is ordered by bucket then identity, so no "
				"two items are left unranked");
		}
	}

	// Same observations, same queue — identical, not merely equivalent.
	void test_ordering_is_repeatable()
	{
		auto items = std::vector<core::queue_item>{ };
		for (auto index = std::int64_t{ 0 }; index < 6; ++index) {
			items.push_back(core::queue_item{
				.Ref        = ref(index),
				.Membership = membership(
					index % 2 == 0 ? core::github_role::reviewer
								   : core::github_role::participant),
				.Bucket = index < 3
					? core::attention_bucket::new_comments_or_author_replies
					: core::attention_bucket::quiet,
			});
		}

		expect(
			core::ordered_queue(items) == core::ordered_queue(items),
			"replaying one queue gives one answer");
	}

	// The reason travels with the item. A presentation recomputing it might
	// agree today and disagree after either side is edited, and the reviewer
	// would have no way to tell which was right.
	void test_the_reason_travels_with_the_item()
	{
		const auto item = core::queue_item{
			.Ref        = ref(11),
			.Membership = membership(core::github_role::requested_reviewer),
			.Bucket     = core::attention_bucket::initial_review_requested,
		};
		const auto ordered = core::ordered_queue(std::array{ item });
		expect(
			ordered.front().Bucket == item.Bucket
				&& ordered.front().Membership == item.Membership,
			"ordering carries the bucket and the membership, and derives "
			"neither");
	}
} // namespace

auto main() -> int
{
	test_every_bucket_is_reachable();
	test_an_unanswered_request_outranks_everything();
	test_authored_never_displaces_work_the_reviewer_can_do();
	test_observed_is_only_for_items_no_role_brought_in();
	test_every_bucket_names_and_explains_itself_distinctly();
	test_the_queue_is_ordered_by_attention();
	test_the_order_is_total_whatever_order_the_host_supplies();
	test_ordering_is_repeatable();
	test_the_reason_travels_with_the_item();

	std::cout << "review-queue-attention-tests:pass\n";
	return EXIT_SUCCESS;
}
