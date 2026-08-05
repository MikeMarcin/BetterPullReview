#include "bpr/core/review_queue_attention.hpp"

#include <algorithm>

namespace bpr::core {
	auto name(const attention_bucket aBucket) noexcept -> std::string_view
	{
		switch (aBucket) {
		case attention_bucket::initial_review_requested:
			return "initial review requested";
		case attention_bucket::changed_since_my_review:
			return "changed since my review";
		case attention_bucket::new_comments_or_author_replies:
			return "new comments or author replies";
		case attention_bucket::fixes_awaiting_verification:
			return "fixes awaiting verification";
		case attention_bucket::authored_awaiting_reviewer_action:
			return "authored, awaiting reviewer action";
		case attention_bucket::observed_with_new_activity:
			return "observed, with new activity";
		case attention_bucket::quiet:
			return "quiet";
		}
		return "quiet";
	}

	auto explanation(const attention_bucket aBucket) noexcept -> std::string_view
	{
		switch (aBucket) {
		case attention_bucket::initial_review_requested:
			return "You have been asked to review this and have not yet.";
		case attention_bucket::changed_since_my_review:
			return "The code moved after you reviewed it.";
		case attention_bucket::new_comments_or_author_replies:
			return "There is something here you have not read.";
		case attention_bucket::fixes_awaiting_verification:
			return "Somebody says a defect you filed is fixed. You have not "
				   "checked it.";
		case attention_bucket::authored_awaiting_reviewer_action:
			return "You are waiting on a reviewer, not the other way round.";
		case attention_bucket::observed_with_new_activity:
			return "Something happened on a pull request you watch.";
		case attention_bucket::quiet:
			return "Nothing here is waiting on anyone.";
		}
		return "Nothing here is waiting on anyone.";
	}

	auto bucket_for(
		const queue_membership&           aMembership,
		const queue_activity_observation& aActivity) noexcept -> attention_bucket
	{
		// Being asked and not having answered outranks everything else. It is
		// the only state where somebody is blocked on this reviewer having done
		// nothing at all.
		if (aMembership.Role == github_role::requested_reviewer
			&& !aActivity.ReviewedByMe) {
			return attention_bucket::initial_review_requested;
		}

		// A review you already gave against code that has since moved is a
		// review that may no longer say what you meant.
		if (aActivity.ReviewedByMe && aActivity.HeadMovedSinceMyReview) {
			return attention_bucket::changed_since_my_review;
		}

		if (aActivity.UnseenComments > 0U) {
			return attention_bucket::new_comments_or_author_replies;
		}

		if (aActivity.DefectsAwaitingVerification > 0U) {
			return attention_bucket::fixes_awaiting_verification;
		}

		// Authored comes after the reviewer-facing buckets on purpose. Waiting
		// on somebody else is not work this reviewer can do, so it must not
		// displace work they can.
		if (aMembership.Role == github_role::author
			&& aActivity.ReviewersAwaited > 0U) {
			return attention_bucket::authored_awaiting_reviewer_action;
		}

		// Only for an item that is here because a rule matched. An item GitHub
		// put in front of the reviewer is not "observed" — saying so would hide
		// that they were asked.
		if (aMembership.MatchedRule && aMembership.Role == github_role::none
			&& aActivity.ObservedActivity) {
			return attention_bucket::observed_with_new_activity;
		}

		return attention_bucket::quiet;
	}

	auto ordered_queue(const std::span<const queue_item> aItems)
		-> std::vector<queue_item>
	{
		auto ordered = std::vector<queue_item>{ aItems.begin(), aItems.end() };
		// Bucket first, then identity. Sorting by bucket alone would leave the
		// order within a bucket to whatever the host happened to read first,
		// and a queue that reshuffles between refreshes is one a reviewer
		// cannot keep their place in. Identity is a tiebreak rather than a
		// ranking: it makes the answer the same every time and claims nothing
		// about which review matters more.
		std::ranges::sort(
			ordered,
			[](const queue_item& aLeft, const queue_item& aRight) {
				if (aLeft.Bucket != aRight.Bucket) {
					return aLeft.Bucket < aRight.Bucket;
				}
				return aLeft.Ref < aRight.Ref;
			});
		return ordered;
	}
} // namespace bpr::core
