// [[RFC-BPR-0098]]. The next move of a pull request the viewer authored:
// one fixture per row of the decision table, the first of several rows
// winning, the facts that are absent, placement and order.

#include "bpr/core/review_queue_attention.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using move = core::authored_next_move;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	/// A pull request of mine with every fact read and nothing asked of
	/// anyone: not a draft, one reviewer requested, no review yet, checks
	/// passed, mergeable, every conversation answered.
	[[nodiscard]] auto mine(const std::int64_t aNumber = 1) -> core::queue_item
	{
		auto item                = core::queue_item{ };
		item.Ref                 = { .Host = "github.com", .Account = "me", .Repository = "octo/app", .Number = aNumber };
		item.Membership.Role     = core::github_role::author;
		item.Membership.Authored = true;
		item.Bucket              = core::attention_bucket::quiet;
		item.Facts.Draft         = false;
		auto& facts              = item.Facts.Waiting;
		facts.Requests           = 0U;
		facts.NamesKnown         = true;
		facts.RequiredReview     = false;
		facts.MergeBlocked       = false;
		facts.Decision           = core::queue_review_decision::none;
		facts.Merge              = core::queue_merge_state::clean;
		facts.ChecksKnown        = true;
		facts.ReviewsKnown       = true;
		facts.LatestReviews      = { { .Login = "alice", .State = core::my_review_verdict::commented } };
		facts.AwaitingReply      = 0U;
		return item;
	}

	[[nodiscard]] auto awaited(core::queue_item aItem) -> core::queue_item
	{
		aItem.Bucket                  = core::attention_bucket::authored_awaiting_reviewer_action;
		aItem.Facts.Waiting.Requests  = 1U;
		aItem.Facts.Waiting.Reviewers = { "@bo" };
		aItem.Facts.Waiting.People    = 1U;
		return aItem;
	}

	[[nodiscard]] auto approve(core::queue_item aItem) -> core::queue_item
	{
		aItem.Facts.Waiting.Decision      = core::queue_review_decision::approved;
		aItem.Facts.Waiting.LatestReviews = { { .Login = "alice", .State = core::my_review_verdict::approved } };
		return aItem;
	}

	void test_each_row_of_the_table()
	{
		// Returned: a change request, by GitHub's decision.
		auto returned                       = mine();
		returned.Facts.Waiting.Decision     = core::queue_review_decision::changes_requested;
		returned.Facts.Waiting.MergeBlocked = true;
		returned.Facts.Waiting.Merge        = core::queue_merge_state::blocked;
		expect(core::next_move_for(returned) == move::returned, "a change request returns the pull request");
		// Returned: a conversation whose latest comment is not mine.
		auto replies                        = mine();
		replies.Facts.Waiting.AwaitingReply = 2U;
		expect(core::next_move_for(replies) == move::returned, "a conversation awaiting my reply returns it");

		auto failing                       = approve(mine());
		failing.Facts.Waiting.FailedChecks = { "build-linux" };
		failing.Facts.Waiting.Merge        = core::queue_merge_state::unstable;
		expect(core::next_move_for(failing) == move::failing, "a failed check is Failing");

		auto blocked                       = mine();
		blocked.Facts.Waiting.Merge        = core::queue_merge_state::blocked;
		blocked.Facts.Waiting.MergeBlocked = true;
		expect(core::next_move_for(blocked) == move::blocked, "a block nothing explains is Blocked");
		auto conflicting                = awaited(mine());
		conflicting.Facts.Waiting.Merge = core::queue_merge_state::conflicts;
		expect(core::next_move_for(conflicting) == move::blocked, "a conflict is Blocked even while reviewers are awaited");

		expect(core::next_move_for(approve(mine())) == move::ready, "approved, passed and clean is Ready");

		auto unread                 = mine();
		unread.Bucket               = core::attention_bucket::new_comments_or_author_replies;
		unread.Facts.UnseenComments = 3U;
		expect(core::next_move_for(unread) == move::unread, "unread comments with nothing above are Unread");

		auto unassigned                        = mine();
		unassigned.Facts.Waiting.LatestReviews = { };
		expect(core::next_move_for(unassigned) == move::unassigned, "no reviewer requested and none reviewed is Unassigned");

		auto draft                        = mine();
		draft.Facts.Draft                 = true;
		draft.Facts.Waiting.Merge         = core::queue_merge_state::draft;
		draft.Facts.Waiting.LatestReviews = { };
		expect(core::next_move_for(draft) == move::draft, "a draft with nothing above is Draft");

		auto waiting                         = awaited(mine());
		waiting.Facts.Waiting.Decision       = core::queue_review_decision::review_required;
		waiting.Facts.Waiting.RequiredReview = true;
		waiting.Facts.Waiting.Merge          = core::queue_merge_state::blocked;
		waiting.Facts.Waiting.MergeBlocked   = true;
		expect(core::next_move_for(waiting) == move::waiting, "a block explained by an awaited review is Waiting, not Blocked");

		auto checking                        = approve(mine());
		checking.Facts.Waiting.PendingChecks = 2U;
		checking.Facts.Waiting.Merge         = core::queue_merge_state::blocked;
		checking.Facts.Waiting.MergeBlocked  = true;
		expect(core::next_move_for(checking) == move::checking, "approved with checks running is Checking, not Blocked");

		expect(core::next_move_for(mine()) == move::quiet, "nothing above is Quiet");

		auto unknown   = mine();
		unknown.Bucket = core::attention_bucket::detail_unknown;
		expect(core::next_move_for(unknown) == move::unknown, "nothing read is not known to be quiet");

		auto theirs                = approve(mine());
		theirs.Membership.Role     = core::github_role::requested_reviewer;
		theirs.Membership.Authored = false;
		expect(!core::next_move_for(theirs).has_value(), "a pull request I did not author has no next move");

		expect(core::owner_of(move::returned) == core::next_move_owner::mine && core::owner_of(move::draft) == core::next_move_owner::mine
				   && core::owner_of(move::waiting) == core::next_move_owner::theirs && core::owner_of(move::checking) == core::next_move_owner::nobody_yet
				   && core::owner_of(move::quiet) == core::next_move_owner::nobody,
			   "whose move each is follows the table");
	}

	void test_the_first_row_wins()
	{
		// Returned, Failing, Blocked, Unread and Draft at once.
		auto all                       = approve(mine());
		all.Facts.Draft                = true;
		all.Facts.UnseenComments       = 1U;
		all.Facts.Waiting.Decision     = core::queue_review_decision::changes_requested;
		all.Facts.Waiting.FailedChecks = { "lint" };
		all.Facts.Waiting.Merge        = core::queue_merge_state::conflicts;
		expect(core::next_move_for(all) == move::returned, "Returned outranks every other row, Draft included");
		all.Facts.Waiting.Decision      = core::queue_review_decision::none;
		all.Facts.Waiting.LatestReviews = { };
		expect(core::next_move_for(all) == move::failing, "then Failing");
		all.Facts.Waiting.FailedChecks = { };
		expect(core::next_move_for(all) == move::blocked, "then Blocked");
		all.Facts.Waiting.Merge = core::queue_merge_state::draft;
		expect(core::next_move_for(all) == move::unread, "then Unread, ahead of Draft");
		all.Facts.UnseenComments = 0U;
		expect(core::next_move_for(all) == move::draft, "then Draft, ahead of Unassigned's not-a-draft");

		// Ready outranks Unread and Waiting.
		auto ready                 = approve(awaited(mine()));
		ready.Facts.UnseenComments = 2U;
		expect(core::next_move_for(ready) == move::ready, "Ready outranks Unread and Waiting");
		// Failing outranks Waiting, and moves the row to My pull requests.
		auto failing                       = awaited(mine());
		failing.Facts.Waiting.FailedChecks = { "build" };
		expect(core::next_move_for(failing) == move::failing && core::home_section_for(failing) == core::home_queue_section::my_pull_requests,
			   "a failing pull request that awaits reviewers is mine to fix");
		// A change request without GitHub's decision: a reviewer's verdict.
		auto verdict                        = mine();
		verdict.Facts.Waiting.LatestReviews = { { .Login = "alice", .State = core::my_review_verdict::approved }, { .Login = "bo", .State = core::my_review_verdict::changes_requested } };
		expect(core::next_move_for(verdict) == move::returned && !core::approved(verdict.Facts.Waiting),
			   "with no required review, a reviewer's change request returns it and it is not approved");
		verdict.Facts.Waiting.Decision = core::queue_review_decision::approved;
		expect(core::next_move_for(verdict) == move::ready, "GitHub's decision, when it gives one, is the decision");
	}

	void test_absent_facts_claim_nothing()
	{
		// AC04: no conversations read.
		auto changes                        = mine();
		changes.Facts.Waiting.AwaitingReply = std::nullopt;
		changes.Facts.Waiting.Decision      = core::queue_review_decision::changes_requested;
		expect(core::next_move_for(changes) == move::returned, "a change request is Returned without conversations");
		auto only                        = mine();
		only.Facts.Waiting.AwaitingReply = std::nullopt;
		expect(core::next_move_for(only) != move::returned, "unknown conversations never claim Returned");

		auto unreviewed                        = mine();
		unreviewed.Facts.Waiting.LatestReviews = { };
		unreviewed.Facts.Waiting.ReviewsKnown  = false;
		expect(core::next_move_for(unreviewed) == move::unknown, "reviews not read are never Unassigned, nor Quiet");
		unreviewed.Facts.Waiting.ReviewsKnown = true;
		unreviewed.Facts.Waiting.Requests     = std::nullopt;
		expect(core::next_move_for(unreviewed) == move::quiet, "requests not read are never Unassigned");

		auto unchecked                      = approve(mine());
		unchecked.Facts.Waiting.ChecksKnown = false;
		expect(core::next_move_for(unchecked) != move::ready, "checks not read are never passed");
		auto unmerged                       = approve(mine());
		unmerged.Facts.Waiting.Merge        = core::queue_merge_state::unknown;
		unmerged.Facts.Waiting.MergeBlocked = std::nullopt;
		expect(core::next_move_for(unmerged) != move::ready, "a merge state not read is never mergeable");
		// A row stored before the merge state was carried still reads its
		// boolean.
		unmerged.Facts.Waiting.MergeBlocked = false;
		expect(core::next_move_for(unmerged) == move::ready, "the stored boolean says mergeable");
	}

	void test_placement_and_order()
	{
		// AC02: authored rows in My pull requests in the table's order,
		// Waiting rows in Waiting on others, none in Needs me.
		auto returned                          = mine(9);
		returned.Facts.Waiting.AwaitingReply   = 1U;
		auto ready                             = approve(mine(8));
		auto failing                           = mine(7);
		failing.Facts.Waiting.FailedChecks     = { "build" };
		auto unread                            = mine(6);
		unread.Bucket                          = core::attention_bucket::new_comments_or_author_replies;
		unread.Facts.UnseenComments            = 1U;
		auto unassigned                        = mine(5);
		unassigned.Facts.Waiting.LatestReviews = { };
		auto quiet                             = mine(4);
		auto waiting                           = awaited(mine(3));
		auto requested                         = mine(2);
		requested.Membership                   = { .Role = core::github_role::requested_reviewer };
		requested.Bucket                       = core::attention_bucket::initial_review_requested;
		auto following                         = mine(1);
		following.Membership                   = { .Role = core::github_role::none, .Followed = true };
		following.Bucket                       = core::attention_bucket::observed_with_new_activity;

		const auto items   = std::vector{ quiet, waiting, unassigned, following, unread, requested, failing, ready, returned };
		const auto ordered = core::ordered_queue(items);
		auto       numbers = std::vector<std::int64_t>{ };
		for (const auto& item : ordered) {
			numbers.push_back(item.Ref.Number);
		}
		expect(numbers == std::vector<std::int64_t>{ 2, 9, 7, 8, 6, 5, 4, 3, 1 },
			   "Needs me, then My pull requests by next move, then Waiting on others, then Following");
		for (const auto& item : ordered) {
			const auto section = core::home_section_for(item);
			if (item.Membership.authored()) {
				expect(section != core::home_queue_section::needs_you, "no authored row is in Needs me");
				expect((section == core::home_queue_section::waiting_on_others) == (core::next_move_for(item) == move::waiting),
					   "an authored row waits on others exactly when its move is Waiting");
			}
		}
		// The bucket, and with it notification, is unchanged.
		expect(failing.Bucket == core::attention_bucket::quiet && core::permits_notification({ }, unread.Bucket) && !core::permits_notification({ }, ready.Bucket),
			   "the next move changes no bucket and so no notification");
		// Hide quiet hides only Quiet rows.
		expect(core::home_row_quiet(quiet) && !core::home_row_quiet(ready) && !core::home_row_quiet(unassigned) && !core::home_row_quiet(failing),
			   "Hide quiet hides a Quiet row and keeps a Ready one");
		// The relation stays a strict weak order across every pair.
		for (const auto& left : items) {
			expect(!core::queue_precedes(left, left), "irreflexive");
			for (const auto& right : items) {
				expect(!(core::queue_precedes(left, right) && core::queue_precedes(right, left)), "asymmetric");
			}
		}
	}
	void test_latest_reviewer_states()
	{
		using verdict  = core::my_review_verdict;
		const auto by  = [](const char* aLogin, const verdict aState) { return core::reviewer_submitted_review{ .Login = aLogin, .Review = { .State = aState } }; };
		const auto all = std::vector{ by("alice", verdict::changes_requested), by("bo", verdict::approved), by("alice", verdict::commented), by("cy", verdict::approved), by("cy", verdict::dismissed), by("alice", verdict::approved) };
		expect(core::latest_reviewer_states(all)
				   == std::vector<core::queue_reviewer_state>{ { .Login = "alice", .State = verdict::approved }, { .Login = "bo", .State = verdict::approved }, { .Login = "cy", .State = verdict::dismissed } },
			   "each reviewer's standing verdict, in the order they first reviewed");
		auto many = std::vector<core::reviewer_submitted_review>{ };
		for (char login = 'a'; login <= 'l'; ++login) {
			many.push_back({ .Login = std::string(1U, login), .Review = { .State = verdict::commented } });
		}
		expect(core::latest_reviewer_states(many).size() == core::maximum_queue_reviewer_states, "at most eight reviewers are carried");
	}
} // namespace

auto main() -> int
{
	test_latest_reviewer_states();
	test_each_row_of_the_table();
	test_the_first_row_wins();
	test_absent_facts_claim_nothing();
	test_placement_and_order();
	return EXIT_SUCCESS;
}
