#include "bpr/core/review_queue_attention.hpp"

#include <algorithm>

namespace bpr::core {
	auto queue_membership_with_authorship(queue_membership aMembership, std::optional<bool> aAuthored) noexcept -> queue_membership
	{
		aMembership.OwnsPullRequest = aAuthored.value_or(false);
		if (aAuthored && !*aAuthored) {
			aMembership.Authored = false;
			if (aMembership.Role == github_role::author) {
				aMembership.Role = github_role::none;
			}
		}
		return aMembership;
	}

	auto holds_role(const queue_item& aItem, const github_role aRole) noexcept -> bool
	{
		const auto& membership = aItem.Membership;
		switch (aRole) {
		case github_role::author:
			return membership.authored();
		case github_role::requested_reviewer:
			return membership.Role == github_role::requested_reviewer || membership.Requested;
		case github_role::reviewer:
			return membership.Role == github_role::reviewer || membership.Reviewed
				|| aItem.Bucket == attention_bucket::changed_since_my_review;
		case github_role::participant:
			return membership.Role == github_role::participant;
		case github_role::none:
			return !holds_role(aItem, github_role::author) && !holds_role(aItem, github_role::requested_reviewer)
				&& !holds_role(aItem, github_role::reviewer) && !holds_role(aItem, github_role::participant);
		}
		return false;
	}

	auto review_waiting(const queue_waiting_facts& aFacts) noexcept -> bool
	{
		return aFacts.Requests.value_or(0) > 0 || aFacts.RequiredReview.value_or(false);
	}

	auto merge_blocked(const queue_waiting_facts& aFacts) noexcept -> bool
	{
		return aFacts.MergeBlocked.value_or(false) || !aFacts.FailedChecks.empty();
	}

	auto home_section_for(const queue_membership& aMembership, const attention_bucket aBucket) noexcept -> home_queue_section
	{
		if (aMembership.authored() && aBucket != attention_bucket::authored_awaiting_reviewer_action) {
			return home_queue_section::my_pull_requests;
		}
		switch (aBucket) {
		case attention_bucket::initial_review_requested:
		case attention_bucket::changed_since_my_review:
		case attention_bucket::new_comments_or_author_replies:
		case attention_bucket::fixes_awaiting_verification:
			return home_queue_section::needs_you;
		case attention_bucket::authored_awaiting_reviewer_action:
			return home_queue_section::waiting_on_others;
		case attention_bucket::observed_with_new_activity:
		case attention_bucket::quiet:
		case attention_bucket::detail_unknown:
			return home_queue_section::following;
		}
		return home_queue_section::following;
	}

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
		case attention_bucket::detail_unknown:
			return "detail unknown";
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
		case attention_bucket::detail_unknown:
			return "Nothing about it has been read yet, so it is not known to be quiet.";
		}
		return "Nothing here is waiting on anyone.";
	}

	auto bucket_for(
		const queue_membership&           aMembership,
		const queue_activity_observation& aActivity) noexcept -> attention_bucket
	{
		// Being asked and not having answered outranks everything else. It is
		// the only state where somebody is blocked on this reviewer having done
		// nothing at all. A direct request discovery found ([[BPR-0989]]) is
		// the same fact: GitHub lists a requested reviewer only until they
		// review.
		const auto requested = aMembership.Role == github_role::requested_reviewer || aMembership.Requested;
		if (requested && !aActivity.ReviewedByMe) {
			return attention_bucket::initial_review_requested;
		}

		// [[BPR-0989]]. Everything below is read from activity. With none read,
		// the answer is that it is not known, never that it is quiet.
		if (!aActivity.Known) {
			return attention_bucket::detail_unknown;
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
		if (aMembership.authored()
			&& (aActivity.ReviewersAwaited > 0U || review_waiting(aActivity.Waiting))) {
			return attention_bucket::authored_awaiting_reviewer_action;
		}

		// Only for an item that is here because a rule matched or the reviewer
		// follows it. An item GitHub put in front of the reviewer is not
		// "observed" — saying so would hide that they were asked.
		if ((aMembership.MatchedRule || aMembership.Followed) && aMembership.Role == github_role::none
			&& !aMembership.discovered() && aActivity.ObservedActivity) {
			return attention_bucket::observed_with_new_activity;
		}

		return attention_bucket::quiet;
	}

	auto queue_precedes(const queue_item& aLeft, const queue_item& aRight) -> bool
	{
		return aLeft.Bucket == aRight.Bucket ? aLeft.Ref < aRight.Ref : aLeft.Bucket < aRight.Bucket;
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
		std::ranges::sort(ordered, queue_precedes);
		return ordered;
	}
} // namespace bpr::core
