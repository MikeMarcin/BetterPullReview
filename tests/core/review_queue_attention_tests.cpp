#include "bpr/core/review_queue_attention.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
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
		core::attention_bucket::detail_unknown,
	};

	void test_home_ownership_preserves_attention()
	{
		for (const auto value : every_bucket) {
			for (const auto role : { core::github_role::author, core::github_role::reviewer }) {
				auto own            = membership(role, true);
				own.Authored        = role != core::github_role::author;
				own.Requested       = true;
				own.Followed        = true;
				const auto expected = value == core::attention_bucket::authored_awaiting_reviewer_action
					? core::home_queue_section::waiting_on_others
					: core::home_queue_section::my_pull_requests;
				expect(core::home_section_for(own, value) == expected, "either accepted authorship source separates all attention states despite coexisting reasons");
			}
		}
		auto       authored = membership(core::github_role::author);
		const auto unread   = core::queue_activity_observation{ .UnseenComments = 3U, .ReviewersAwaited = 2U };
		const auto bucket   = core::bucket_for(authored, unread);
		expect(bucket == core::attention_bucket::new_comments_or_author_replies, "authored unread activity still outranks waiting");
		expect(core::home_section_for(authored, bucket) == core::home_queue_section::my_pull_requests, "unread authored activity belongs to My pull requests");
		const auto otherAccount = membership(core::github_role::reviewer);
		expect(core::home_section_for(otherAccount, bucket) == core::home_queue_section::needs_you, "another account without its own authorship still needs me");
		expect(core::permits_notification({ }, bucket), "Home separation leaves the existing notification eligibility intact");
		expect(core::home_section_for(authored, core::bucket_for(authored, { .ReviewersAwaited = 2U })) == core::home_queue_section::waiting_on_others, "read activity returns to waiting-only placement");
	}

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

	// [[BPR-0989]]. Discovery's reasons decide exactly as the GitHub roles do,
	// a follow is observed like a rule, and an unread item is not quiet.
	void test_discovery_reasons_and_unknown_detail()
	{
		const auto requested = core::queue_membership{ .Requested = true };
		const auto authored  = core::queue_membership{ .Authored = true };
		const auto followed  = core::queue_membership{ .Followed = true };
		const auto unknown   = core::queue_activity_observation{ .Known = false };
		expect(core::bucket_for(requested, { }) == core::attention_bucket::initial_review_requested
				   && core::bucket_for(requested, unknown) == core::attention_bucket::initial_review_requested,
			   "a discovered direct request needs me, read or not: GitHub lists it only until I review");
		expect(core::bucket_for(authored, { .ReviewersAwaited = 1U }) == core::attention_bucket::authored_awaiting_reviewer_action,
			   "my discovered pull request awaiting a reviewer waits on others");
		expect(core::bucket_for(authored, unknown) == core::attention_bucket::detail_unknown
				   && core::bucket_for(membership(core::github_role::reviewer), unknown) == core::attention_bucket::detail_unknown,
			   "with nothing read, the bucket says so instead of quiet");
		expect(core::bucket_for(authored, { }) == core::attention_bucket::quiet, "read and asking nothing is quiet");
		expect(core::bucket_for(followed, { .ObservedActivity = true }) == core::attention_bucket::observed_with_new_activity
				   && core::bucket_for(authored, { .ObservedActivity = true }) == core::attention_bucket::quiet,
			   "a follow is observed like a rule; a discovered pull request is not merely observed");
		expect(!core::permits_notification({ }, core::attention_bucket::detail_unknown), "not knowing is never a notification");
		expect(followed.enrolled() && authored.enrolled() && core::queue_membership{ .Reviewed = true }.enrolled()
				   && authored.discovered() && requested.discovered() && !followed.discovered() && !core::queue_membership{ }.enrolled(),
			   "each reason enrolls on its own");
		const auto items   = std::array{ core::queue_item{ .Ref = ref(1), .Bucket = core::attention_bucket::detail_unknown },
										 core::queue_item{ .Ref = ref(2), .Bucket = core::attention_bucket::quiet } };
		const auto ordered = core::ordered_queue(items);
		expect(ordered.front().Ref == ref(2), "unknown detail sorts after quiet, where no stored value moves");
	}

	// "My role" holds discovery's reasons as well as GitHub's own Role, which a
	// signed-in account's queue never fills: requested and reviewed rows were
	// hidden by Reviewer and Requested before this.
	void test_a_role_is_held_by_any_of_its_reasons()
	{
		const auto item = [](const core::queue_membership aMembership, const core::attention_bucket aBucket = core::attention_bucket::quiet) {
			return core::queue_item{ .Ref = ref(1), .Membership = aMembership, .Bucket = aBucket, .Facts = { } };
		};
		const auto requested = item({ .Requested = true });
		const auto reviewed  = item({ .Reviewed = true });
		const auto authored  = item({ .Authored = true });
		const auto ruled     = item(membership(core::github_role::none, true));
		const auto observed  = core::queue_membership_with_activity(membership(core::github_role::none, true), std::nullopt, true);
		expect(core::holds_role(requested, core::github_role::requested_reviewer), "a discovered request is Requested");
		expect(!core::holds_role(requested, core::github_role::reviewer) && !core::holds_role(requested, core::github_role::none),
			   "a discovered request is neither Reviewer nor No role");
		expect(core::holds_role(reviewed, core::github_role::reviewer), "an observed review is Reviewer");
		expect(core::holds_role(item(observed), core::github_role::reviewer)
				   && core::holds_role(item(observed, core::attention_bucket::changed_since_my_review), core::github_role::reviewer),
			   "an observed submitted review is Reviewer regardless of attention bucket");
		expect(core::holds_role(authored, core::github_role::author) && !core::holds_role(authored, core::github_role::none),
			   "a discovered authored item is Author");
		expect(core::holds_role(ruled, core::github_role::none), "a rule's item with no role of its own is No role");
		const auto both = item({ .Requested = true, .Reviewed = true });
		expect(core::holds_role(both, core::github_role::requested_reviewer) && core::holds_role(both, core::github_role::reviewer),
			   "an item holds every role it has a reason for");
		for (const auto role : { core::github_role::participant, core::github_role::reviewer, core::github_role::requested_reviewer, core::github_role::author }) {
			expect(core::holds_role(item(membership(role)), role), "GitHub's own Role still holds");
		}
	}

	/// [[BPR-PENDING-X]]. The reviewer's standing verdict, its staleness and
	/// its dismissal, from their submitted reviews oldest first.
	void test_my_standing_review_verdict()
	{
		using verdict  = core::my_review_verdict;
		const auto rev = [](const verdict aState, const std::int64_t aAt, const std::optional<bool> aOnHead = true) {
			return core::my_submitted_review{ .State = aState, .SubmittedAt = aAt, .OnHead = aOnHead };
		};
		const auto of = [](const std::vector<core::my_submitted_review>& aReviews) { return core::my_review_for(aReviews); };

		expect(of({ }) == core::queue_my_review{ }, "no review read is no verdict");
		expect(of({ rev(verdict::approved, 100) }) == core::queue_my_review{ .Verdict = verdict::approved, .SubmittedAt = 100, .NewCommits = false },
			   "an approval on the current head is approved, with its time");
		expect(of({ rev(verdict::approved, 100, false) }) == core::queue_my_review{ .Verdict = verdict::approved, .SubmittedAt = 100, .NewCommits = true },
			   "an approval of an earlier head has new commits after it");
		expect(!of({ rev(verdict::approved, 100, std::nullopt) }).NewCommits, "an unknown commit says nothing about new commits");
		expect(of({ rev(verdict::approved, 100, false), rev(verdict::commented, 200) }) == core::queue_my_review{ .Verdict = verdict::approved, .SubmittedAt = 100, .NewCommits = true },
			   "a later comment keeps the approval standing, and its staleness is the approval's own");
		expect(of({ rev(verdict::commented, 50), rev(verdict::changes_requested, 100) }).Verdict == verdict::changes_requested, "a change request replaces a comment");
		expect(of({ rev(verdict::approved, 100), rev(verdict::changes_requested, 200) }) == core::queue_my_review{ .Verdict = verdict::changes_requested, .SubmittedAt = 200 },
			   "the latest verdict wins");
		expect(of({ rev(verdict::changes_requested, 100), rev(verdict::approved, 200) }).Verdict == verdict::approved, "an approval after a change request is approved");
		expect(of({ rev(verdict::commented, 70, false) }) == core::queue_my_review{ .Verdict = verdict::commented, .SubmittedAt = 70, .NewCommits = true },
			   "comments alone are commented, and can be overtaken by commits");
		expect(of({ rev(verdict::commented, 70), rev(verdict::commented, 90, false) }).SubmittedAt == std::optional<std::int64_t>{ 90 }, "the latest of several comments is the one said");

		// Dismissal: never approved.
		const auto dismissed = of({ rev(verdict::approved, 100), rev(verdict::dismissed, 150, false) });
		expect(dismissed == core::queue_my_review{ .Verdict = verdict::dismissed, .SubmittedAt = 150, .NewCommits = false },
			   "a dismissed review after an approval is dismissed, never approved, and has no staleness");
		expect(of({ rev(verdict::dismissed, 100), rev(verdict::commented, 150) }) == core::queue_my_review{ .Verdict = verdict::commented, .SubmittedAt = 150 },
			   "a comment after a dismissal is what was last said");
		expect(of({ rev(verdict::commented, 50), rev(verdict::dismissed, 100) }).Verdict == verdict::dismissed, "a comment before a dismissal does not outlast it");
		expect(of({ rev(verdict::dismissed, 100), rev(verdict::approved, 200) }).Verdict == verdict::approved, "approving again after a dismissal is approved");
		expect(of({ core::my_submitted_review{ .State = verdict::approved } }) == core::queue_my_review{ .Verdict = verdict::approved }, "an unread time is no time");

		auto names = std::vector<std::string_view>{ };
		for (const auto value : { verdict::none, verdict::commented, verdict::changes_requested, verdict::approved, verdict::dismissed }) {
			names.push_back(core::name(value));
		}
		std::ranges::sort(names);
		expect(std::ranges::adjacent_find(names) == names.end() && std::ranges::none_of(names, &std::string_view::empty), "every verdict has its own name");
	}
} // namespace

auto main() -> int
{
	test_a_role_is_held_by_any_of_its_reasons();
	test_home_ownership_preserves_attention();
	test_discovery_reasons_and_unknown_detail();
	for (const auto enabled : { false, true }) {
		for (const auto paused : { false, true }) {
			for (auto bucket = 0U; bucket < 8U; ++bucket) {
				expect(core::permits_notification({ enabled, paused }, static_cast<core::attention_bucket>(bucket))
						   == (enabled && !paused && bucket < 6U),
					   "notification choices suppress quiet and unknown buckets without changing attention");
			}
		}
	}
	test_every_bucket_is_reachable();
	test_an_unanswered_request_outranks_everything();
	test_authored_never_displaces_work_the_reviewer_can_do();
	test_observed_is_only_for_items_no_role_brought_in();
	test_every_bucket_names_and_explains_itself_distinctly();
	test_the_queue_is_ordered_by_attention();
	test_the_order_is_total_whatever_order_the_host_supplies();
	test_ordering_is_repeatable();
	test_the_reason_travels_with_the_item();
	test_my_standing_review_verdict();

	std::cout << "review-queue-attention-tests:pass\n";
	return EXIT_SUCCESS;
}
