#include "bpr/core/review_queue_attention.hpp"

#include <algorithm>
#include <utility>

namespace bpr::core {
	auto queue_membership_with_activity(queue_membership aMembership, std::optional<bool> aAuthored, bool aReviewedByMe) noexcept -> queue_membership
	{
		aMembership.OwnsPullRequest = aAuthored.value_or(false);
		if (aAuthored && !*aAuthored) {
			aMembership.Authored = false;
			if (aMembership.Role == github_role::author) {
				aMembership.Role = github_role::none;
			}
		}
		// [[BPR-1866]]. Decided after authorship: an author's own replies
		// are comment-only reviews, and a reason retained from them before
		// this rule must not make the author a reviewer either.
		const auto authored  = aMembership.authored();
		aMembership.Reviewed = !authored && (aMembership.Reviewed || reviewed_by_viewer(aReviewedByMe, authored));
		return aMembership;
	}

	auto my_review_shown(const queue_membership& aMembership, const queue_my_review& aReview) noexcept -> queue_my_review
	{
		return aMembership.authored() ? queue_my_review{ } : aReview;
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
			return membership.Role == github_role::reviewer || membership.Reviewed;
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

	auto changes_requested(const queue_waiting_facts& aFacts) noexcept -> bool
	{
		if (aFacts.Decision && *aFacts.Decision != queue_review_decision::none) {
			return *aFacts.Decision == queue_review_decision::changes_requested;
		}
		return aFacts.ReviewsKnown && std::ranges::any_of(aFacts.LatestReviews, [](const queue_reviewer_state& aReviewer) { return aReviewer.State == my_review_verdict::changes_requested; });
	}

	auto approved(const queue_waiting_facts& aFacts) noexcept -> bool
	{
		if (aFacts.Decision && *aFacts.Decision != queue_review_decision::none) {
			return *aFacts.Decision == queue_review_decision::approved;
		}
		return aFacts.ReviewsKnown && !changes_requested(aFacts)
			&& std::ranges::any_of(aFacts.LatestReviews, [](const queue_reviewer_state& aReviewer) { return aReviewer.State == my_review_verdict::approved; });
	}

	auto name(const authored_next_move aMove) noexcept -> std::string_view
	{
		switch (aMove) {
		case authored_next_move::returned:   return "returned";
		case authored_next_move::failing:    return "failing";
		case authored_next_move::blocked:    return "blocked";
		case authored_next_move::ready:      return "ready";
		case authored_next_move::unread:     return "unread";
		case authored_next_move::unassigned: return "unassigned";
		case authored_next_move::draft:      return "draft";
		case authored_next_move::waiting:    return "waiting";
		case authored_next_move::checking:   return "checking";
		case authored_next_move::quiet:      return "quiet";
		case authored_next_move::unknown:    return "unknown";
		}
		return "quiet";
	}

	auto owner_of(const authored_next_move aMove) noexcept -> next_move_owner
	{
		switch (aMove) {
		case authored_next_move::returned:
		case authored_next_move::failing:
		case authored_next_move::blocked:
		case authored_next_move::ready:
		case authored_next_move::unread:
		case authored_next_move::unassigned:
		case authored_next_move::draft:      return next_move_owner::mine;
		case authored_next_move::waiting:    return next_move_owner::theirs;
		case authored_next_move::checking:   return next_move_owner::nobody_yet;
		case authored_next_move::quiet:
		case authored_next_move::unknown:    break;
		}
		return next_move_owner::nobody;
	}

	auto next_move_for(const queue_item& aItem) noexcept -> std::optional<authored_next_move>
	{
		if (!aItem.Membership.authored()) {
			return std::nullopt;
		}
		if (aItem.Bucket == attention_bucket::detail_unknown) {
			return authored_next_move::unknown;
		}
		const auto& facts = aItem.Facts.Waiting;
		const auto  draft = aItem.Facts.Draft.value_or(false);
		// Returned outranks Draft: a reviewer's reply on a draft is still
		// mine to answer. Unread conversations count only when they were read.
		if (changes_requested(facts) || facts.AwaitingReply.value_or(0U) > 0U) {
			return authored_next_move::returned;
		}
		if (!facts.FailedChecks.empty()) {
			return authored_next_move::failing;
		}
		// A merge GitHub refuses because a review is outstanding or a check
		// is still running is explained by that wait; a conflict, or a base
		// that must be merged in first, is mine whatever else is pending.
		const auto running = facts.ChecksKnown && facts.PendingChecks > 0U;
		const auto refused = facts.Merge == queue_merge_state::unknown ? facts.MergeBlocked.value_or(false)
																	   : facts.Merge == queue_merge_state::blocked || facts.Merge == queue_merge_state::unstable;
		const auto stuck   = facts.Merge == queue_merge_state::conflicts || facts.Merge == queue_merge_state::behind;
		const auto awaited = aItem.Bucket == attention_bucket::authored_awaiting_reviewer_action || review_waiting(facts);
		if (stuck || (refused && !running && !awaited && !draft)) {
			return authored_next_move::blocked;
		}
		const auto passed    = facts.ChecksKnown && facts.PendingChecks == 0U;
		const auto mergeable = facts.Merge == queue_merge_state::clean || facts.Merge == queue_merge_state::hooks
			|| (facts.Merge == queue_merge_state::unknown && facts.MergeBlocked == std::optional{ false });
		if (!draft && approved(facts) && passed && mergeable) {
			return authored_next_move::ready;
		}
		if (aItem.Facts.UnseenComments > 0U || aItem.Bucket == attention_bucket::new_comments_or_author_replies) {
			return authored_next_move::unread;
		}
		if (!draft && aItem.Facts.Draft.has_value() && facts.Requests == std::optional{ 0U } && !facts.RequestsStale && facts.ReviewsKnown
			&& facts.LatestReviews.empty()) {
			return authored_next_move::unassigned;
		}
		if (draft) {
			return authored_next_move::draft;
		}
		if (aItem.Bucket == attention_bucket::authored_awaiting_reviewer_action) {
			return authored_next_move::waiting;
		}
		if (approved(facts) && running) {
			return authored_next_move::checking;
		}
		// Quiet is a claim that nobody is waiting, which needs the reviews to
		// have been read: a pull request a search just found has a title and
		// requests, not yet its reviews, decision or checks.
		return facts.ReviewsKnown ? authored_next_move::quiet : authored_next_move::unknown;
	}

	auto home_row_quiet(const queue_item& aItem) noexcept -> bool
	{
		if (const auto move = next_move_for(aItem); move) {
			return *move == authored_next_move::quiet;
		}
		return aItem.Bucket == attention_bucket::quiet && !merge_blocked(aItem.Facts.Waiting);
	}

	auto home_section_for(const queue_item& aItem, const home_draft_placement aDrafts) noexcept -> home_queue_section
	{
		if (aItem.Ignored) {
			return home_queue_section::ignored;
		}
		if (const auto move = next_move_for(aItem); move) {
			return *move == authored_next_move::waiting ? home_queue_section::waiting_on_others : home_queue_section::my_pull_requests;
		}
		const auto section = home_section_for(aItem.Membership, aItem.Bucket);
		// [[BPR-2326]]. Not mine (a draft of mine has its next move above) and
		// not ready for review: nothing is asked of me yet.
		if (section == home_queue_section::needs_you && draft_waits_quietly(aItem, aDrafts)) {
			return home_queue_section::following;
		}
		return section;
	}

	auto draft_waits_quietly(const queue_item& aItem, const home_draft_placement aDrafts) noexcept -> bool
	{
		// An unread draft status is not a draft.
		return aDrafts == home_draft_placement::following && aItem.Facts.Draft == std::optional{ true } && !aItem.Membership.authored();
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

	auto name(const my_review_verdict aVerdict) noexcept -> std::string_view
	{
		switch (aVerdict) {
		case my_review_verdict::commented:
			return "commented";
		case my_review_verdict::changes_requested:
			return "changes requested";
		case my_review_verdict::approved:
			return "approved";
		case my_review_verdict::dismissed:
			return "dismissed";
		case my_review_verdict::none:
			break;
		}
		return "none";
	}

	auto my_review_for(const std::span<const my_submitted_review> aReviews) noexcept -> queue_my_review
	{
		// The latest approval, change request or dismissal, and the latest
		// comment-only review. A comment never displaces a verdict: GitHub
		// keeps an approval standing through later comments.
		const my_submitted_review* verdict = nullptr;
		const my_submitted_review* comment = nullptr;
		for (const auto& review : aReviews) {
			switch (review.State) {
			case my_review_verdict::approved:
			case my_review_verdict::changes_requested:
			case my_review_verdict::dismissed:
				verdict = &review;
				comment = nullptr;
				break;
			case my_review_verdict::commented:
				comment = &review;
				break;
			case my_review_verdict::none:
				break;
			}
		}
		// A dismissed verdict stands for nothing, so a comment after it is
		// what the reviewer last said.
		const auto* const standing = verdict == nullptr || (verdict->State == my_review_verdict::dismissed && comment != nullptr) ? comment : verdict;
		if (standing == nullptr) {
			return { };
		}
		return {
			.Verdict     = standing->State,
			.SubmittedAt = standing->SubmittedAt,
			.NewCommits  = standing->State != my_review_verdict::dismissed && standing->OnHead == false,
		};
	}

	auto latest_reviewer_states(const std::span<const reviewer_submitted_review> aReviews) -> std::vector<queue_reviewer_state>
	{
		auto logins = std::vector<std::string_view>{ };
		for (const auto& review : aReviews) {
			if (!review.Login.empty() && std::ranges::find(logins, review.Login) == logins.end()) {
				logins.push_back(review.Login);
			}
		}
		auto result = std::vector<queue_reviewer_state>{ };
		auto theirs = std::vector<my_submitted_review>{ };
		for (const auto login : logins) {
			if (result.size() == maximum_queue_reviewer_states) {
				break;
			}
			theirs.clear();
			for (const auto& review : aReviews) {
				if (review.Login == login) {
					theirs.push_back(review.Review);
				}
			}
			if (const auto standing = my_review_for(theirs); standing.Verdict != my_review_verdict::none) {
				result.push_back({ .Login = std::string{ login }, .State = standing.Verdict });
			}
		}
		return result;
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

	auto head_seen(const queue_item_facts& aFacts) noexcept -> bool
	{
		return !aFacts.SeenHead.empty() && aFacts.SeenHead == aFacts.Waiting.Head && seen_followup_verdict(aFacts.MyReview.Verdict);
	}

	auto seen_activity(queue_activity_observation aActivity, const queue_item_facts& aFacts) noexcept -> queue_activity_observation
	{
		if (head_seen(aFacts)) {
			aActivity.HeadMovedSinceMyReview = false;
		}
		return aActivity;
	}

	auto review_followup_for(const queue_item& aItem) noexcept -> review_followup
	{
		if (aItem.Membership.authored() || aItem.Bucket != attention_bucket::changed_since_my_review) {
			return review_followup::none;
		}
		switch (aItem.Facts.MyReview.Verdict) {
		case my_review_verdict::approved:
		case my_review_verdict::changes_requested:
		case my_review_verdict::commented:         return aItem.Facts.Waiting.Head.empty() ? review_followup::none : review_followup::seen;
		case my_review_verdict::dismissed:         return review_followup::approve_again;
		case my_review_verdict::none:              break;
		}
		return review_followup::none;
	}

	auto review_followup_words_for(const queue_item& aItem) noexcept -> review_followup_words
	{
		auto       words   = review_followup_words{ };
		const auto verdict = aItem.Facts.MyReview.Verdict;
		words.Followup     = review_followup_for(aItem);
		switch (words.Followup) {
		case review_followup::seen:
			words.Seen = "Seen";
			switch (verdict) {
			case my_review_verdict::changes_requested:
				words.Standing = review_standing::new_commits_since_review;
				words.SeenTip  = "Marks the new commits seen until the next push. Nothing is submitted to GitHub; your change request stands.";
				words.Note     = "New commits since you requested changes. Your change request still stands on GitHub.";
				words.Stands   = "your change request stands";
				return words;
			case my_review_verdict::commented:
				words.Standing = review_standing::new_commits_since_review;
				words.SeenTip  = "Marks the new commits seen until the next push. Nothing is submitted to GitHub; your comments stand.";
				words.Note     = "New commits since your review. Your comments stand as they are.";
				words.Stands   = "your comments stand";
				return words;
			case my_review_verdict::approved:
			case my_review_verdict::dismissed:
			case my_review_verdict::none:      break;
			}
			words.Standing = review_standing::new_commits_since_approval;
			words.SeenTip  = "Marks the new commits seen. Nothing is submitted to GitHub; your approval stands.";
			words.Note     = "New commits since you approved. Your approval still stands on GitHub.";
			words.Stands   = "your approval stands";
			return words;
		case review_followup::approve_again:
			words.Standing   = review_standing::approval_dismissed;
			words.Approve    = "Approve again";
			words.ApproveTip = "Submits a new approval for the current commits.";
			words.Note       = "GitHub dismissed your approval when new commits were pushed.";
			return words;
		case review_followup::none: break;
		}
		if (!aItem.Membership.authored() && head_seen(aItem.Facts)) {
			words.Standing = review_standing::up_to_date;
			words.Note     = verdict == my_review_verdict::approved ? "Up to date: you have seen every commit since you approved."
				: verdict == my_review_verdict::changes_requested   ? "Up to date: you have seen every commit since you requested changes."
																	: "Up to date: you have seen every commit since your review.";
		}
		return words;
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
		// review that may no longer say what you meant. [[BPR-1866]]. On a
		// pull request you authored your "review" is a reply, and the code
		// moved because you pushed: neither is news to you.
		if (reviewed_by_viewer(aActivity.ReviewedByMe, aMembership.authored()) && aActivity.HeadMovedSinceMyReview) {
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
		// displace work they can. [[BPR-1780]]. A draft waits on its author:
		// nobody else is asked to act until it is marked ready.
		if (aMembership.authored() && aActivity.Draft != std::optional{ true }
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

	namespace {
		/// The rank one item sorts by before its identity. Buckets take the
		/// even ranks; [[RFC-BPR-0098]] a row of My pull requests takes the odd
		/// rank after the last bucket that needs me, then its next move.
		[[nodiscard]] auto queue_rank(const queue_item& aItem) noexcept -> std::pair<unsigned, unsigned>
		{
			if (const auto move = next_move_for(aItem); move && *move != authored_next_move::waiting) {
				return { 2U * static_cast<unsigned>(attention_bucket::fixes_awaiting_verification) + 1U, static_cast<unsigned>(*move) };
			}
			return { 2U * static_cast<unsigned>(aItem.Bucket), 0U };
		}
	} // namespace

	auto queue_precedes(const queue_item& aLeft, const queue_item& aRight) -> bool
	{
		if (aLeft.Ignored != aRight.Ignored) {
			return aRight.Ignored;
		}
		const auto left  = queue_rank(aLeft);
		const auto right = queue_rank(aRight);
		return left == right ? aLeft.Ref < aRight.Ref : left < right;
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
