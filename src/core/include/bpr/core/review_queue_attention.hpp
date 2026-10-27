#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_QUEUE_ATTENTION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_QUEUE_ATTENTION_HPP

#include "bpr/core/review_queue.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::core {
	/// \brief Names why a queue item wants the reviewer's attention.
	///
	/// \details The order is the order of attention. `initial_review_requested`
	/// first because nobody has looked at all; `quiet` last because nothing is
	/// asked of anyone. A reviewer scanning from the top is scanning by
	/// urgency, and the enumeration says so rather than leaving each surface to
	/// invent an order and disagree.
	enum class attention_bucket : std::uint8_t {
		initial_review_requested,
		changed_since_my_review,
		new_comments_or_author_replies,
		fixes_awaiting_verification,
		authored_awaiting_reviewer_action,
		observed_with_new_activity,
		quiet,
		/// [[BPR-0989]]. Enrolled, but nothing about its activity has been
		/// read: not known to be quiet, and not a reason to notify. After
		/// `quiet` so every value stored before it keeps its meaning.
		detail_unknown,
	};

	/// \brief Home's section order, independent of a row's unchanged attention bucket.
	///
	/// \details [[BPR-1683]]. `ignored` is last: the reviewer has said the
	/// row is not asking for attention, so it sorts after every other section.
	enum class home_queue_section : std::uint8_t {
		needs_you,
		my_pull_requests,
		waiting_on_others,
		following,
		ignored,
	};

	/// \brief Classifies accepted row-scoped authorship for Home without changing attention or notification eligibility.
	///
	/// \details Never `ignored`: ignoring is the reviewer's choice about a row,
	/// not a fact of its membership. Ask the item for its section to include it.
	[[nodiscard]] auto home_section_for(const queue_membership& aMembership, attention_bucket aBucket) noexcept -> home_queue_section;

	/// \brief Durable local notification choices; pause preserves enablement.
	struct notification_preferences
	{
		bool Enabled                                            = true;
		bool Paused                                             = false;
		auto operator<=>(const notification_preferences&) const = default;
	};

	[[nodiscard]] constexpr auto permits_notification(const notification_preferences& aPreferences,
													  const attention_bucket          aBucket) noexcept -> bool
	{
		return aPreferences.Enabled && !aPreferences.Paused
			&& static_cast<std::uint8_t>(aBucket) < static_cast<std::uint8_t>(attention_bucket::quiet);
	}

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(attention_bucket aBucket) noexcept
		-> std::string_view;

	/// \brief Returns the sentence explaining what a bucket asks of the reader.
	///
	/// \details A bucket name says where an item is. It does not say what the
	/// reviewer is expected to do about it, and a queue that only labelled
	/// would leave them to guess.
	[[nodiscard]] auto explanation(attention_bucket aBucket) noexcept
		-> std::string_view;

	/// \brief A bounded last accepted review/merge summary, independent of local activity.
	struct queue_waiting_facts
	{
		std::optional<std::uint32_t> Requests;
		std::uint32_t                People = 0, Teams = 0;
		std::vector<std::string>     CodeOwners, Reviewers, FailedChecks;
		std::optional<bool>          RequiredReview, MergeBlocked;
		bool                         ChecksKnown = false, NamesKnown = false;
		bool                         RequestsStale = false, AssessmentStale = false, ChecksStale = false;
		std::string                  Head, Base;
		auto                         operator<=>(const queue_waiting_facts&) const = default;
	};

	/// \brief Whether an observed summary still reports review work or an independent blocker.
	[[nodiscard]] auto review_waiting(const queue_waiting_facts& aFacts) noexcept -> bool;
	[[nodiscard]] auto merge_blocked(const queue_waiting_facts& aFacts) noexcept -> bool;

	/// \brief What the host observed about one enrolled pull request's state.
	///
	/// \details A bounded record. Every field is something the host read from
	/// the provider or from the local ledger; the core derives the bucket from
	/// these and from nothing else, and reaches nothing to obtain more.
	///
	/// There is no timestamp here, and that is deliberate. "New since I last
	/// looked" is a comparison against what this reviewer has already seen,
	/// which the ledger records; deriving it from a wall clock would make the
	/// answer depend on when the question was asked.
	struct queue_activity_observation
	{
		// Nothing about this pull request has been reviewed by this reviewer.
		bool ReviewedByMe = false;

		// The head this reviewer last reviewed differs from the current head.
		bool HeadMovedSinceMyReview = false;

		// Comments or replies exist that this reviewer has not seen.
		std::uint32_t UnseenComments = 0U;

		// Local defects in `needs verification` or `fix claimed`.
		std::uint32_t DefectsAwaitingVerification = 0U;

		// For a pull request this reviewer authored: a reviewer has been asked
		// and has not answered.
		std::uint32_t ReviewersAwaited = 0U;

		// Activity since this reviewer last looked, for an item they are here
		// for only because a rule matched.
		bool ObservedActivity = false;

		// [[BPR-0989]]. Whether the fields above were read at all. An item
		// enrolled by discovery or restored from the store has not been read
		// until a detail read or an open says what it holds, and until then
		// its quiet is unknown rather than observed.
		bool                Known = true;
		queue_waiting_facts Waiting;
		/// Profile-owned dispatch ordering; zero is a legacy local-only observation.
		std::uint64_t       FactRevision = 0;
		std::optional<bool> Authored;
		/// [[BPR-1780]]. GitHub's draft status, from the item's facts. A
		/// draft I authored waits on nobody until it is marked ready, however
		/// many reviewers it names; absent is not known and changes nothing.
		std::optional<bool> Draft;

		auto operator<=>(const queue_activity_observation&) const = default;
	};

	/// \brief One queue item, with the reason it is where it is.
	///
	/// \details The reason travels with the item. A presentation that
	/// recomputed it from the same observation might agree today and disagree
	/// after either side is edited, and the reviewer would have no way to tell
	/// which was right.
	/// \brief [[BPR-1407]]. How old and how large a pull request is, and how
	/// much has been said on it.
	///
	/// \details What the provider reported on its last read. An absent value
	/// was not read, which is not the same as zero. None of it ranks an item;
	/// the age is measured by the presentation against a clock it is given.
	struct queue_change_facts
	{
		/// When the pull request was opened, in seconds since the epoch (UTC).
		std::optional<std::int64_t>  OpenedAt;
		std::optional<std::uint64_t> Additions;
		std::optional<std::uint64_t> Deletions;
		/// Conversation comments and inline review comments together.
		std::optional<std::uint64_t> Comments;

		auto operator<=>(const queue_change_facts&) const = default;
	};

	/// \brief [[BPR-1718]]. The verdict of the reviewer's own latest
	/// submitted review of a pull request, as Home says it.
	///
	/// \details Stored by value: append only, so every stored value keeps its
	/// meaning.
	enum class my_review_verdict : std::uint8_t {
		/// No submitted review by this reviewer was read.
		none,
		/// Comments only, with no approval or change request standing.
		commented,
		changes_requested,
		approved,
		/// The verdict was dismissed and nothing was submitted after it.
		dismissed,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(my_review_verdict aVerdict) noexcept -> std::string_view;

	/// \brief [[BPR-1718]]. One review this reviewer submitted, as the
	/// host read it, in the order the provider returned them.
	struct my_submitted_review
	{
		/// Never `none`: a review that was read has a state.
		my_review_verdict State = my_review_verdict::commented;
		/// When it was submitted, in seconds since the epoch (UTC); none when
		/// not read.
		std::optional<std::int64_t> SubmittedAt;
		/// Whether its commit is the pull request's current head; none when the
		/// commit was not read or cannot be compared.
		std::optional<bool> OnHead;

		auto operator<=>(const my_submitted_review&) const = default;
	};

	/// \brief [[BPR-1718]]. The reviewer's standing verdict on a pull
	/// request, carried on its item.
	struct queue_my_review
	{
		my_review_verdict Verdict = my_review_verdict::none;
		/// When the verdict's review was submitted; none when not read.
		std::optional<std::int64_t> SubmittedAt;
		/// Commits were pushed after the verdict's review.
		bool NewCommits = false;

		auto operator<=>(const queue_my_review&) const = default;
	};

	/// \brief [[BPR-1718]]. Decides the reviewer's standing verdict from
	/// their submitted reviews, oldest first.
	///
	/// \details As GitHub keeps it: approved, changes requested and dismissed
	/// replace one another, the latest winning, and a comment-only review
	/// replaces none of them. A dismissed verdict stands for nothing, so a
	/// comment after it is the verdict and a dismissed approval is never
	/// approved. "New commits" is the verdict's own review against the current
	/// head; an unknown commit says nothing. Reads no clock and no provider.
	[[nodiscard]] auto my_review_for(std::span<const my_submitted_review> aReviews) noexcept -> queue_my_review;

	/// \brief What the host read about a pull request, carried on its item.
	///
	/// \details [[BPR-0659]]. The title and author come from the observation
	/// the host refreshed the item from; the unseen count is the activity
	/// it observed. None of it ranks the item: `bucket_for` decides that,
	/// and these are what the presentation says beside the number.
	struct queue_item_facts
	{
		std::string   Title;
		std::string   Author;
		std::uint32_t UnseenComments = 0U;
		/// Last accepted GitHub status, independent of private review drafts.
		std::optional<bool> Draft = std::nullopt;
		queue_waiting_facts Waiting;
		std::optional<bool> Authored;
		queue_change_facts  Change;
		/// [[BPR-1718]]. My standing verdict, from the last retained read.
		queue_my_review MyReview;

		auto operator<=>(const queue_item_facts&) const = default;
	};

	/// \brief Adds observed authorship and submitted-review facts to presentation without changing retained enrollment reasons.
	[[nodiscard]] auto queue_membership_with_activity(queue_membership aMembership, std::optional<bool> aAuthored, bool aReviewedByMe) noexcept -> queue_membership;

	struct queue_item
	{
		pull_request_ref Ref;
		queue_membership Membership;
		attention_bucket Bucket = attention_bucket::quiet;
		queue_item_facts Facts;
		/// [[BPR-1683]]. The reviewer ignored it on Home. Its bucket and
		/// facts are unchanged; only its section and its place in the order move.
		bool Ignored = false;

		auto operator<=>(const queue_item&) const = default;
	};

	/// \brief [[BPR-1683]]. The Home section of one decided item: `ignored`
	/// for an ignored row, otherwise its membership's section.
	[[nodiscard]] auto home_section_for(const queue_item& aItem) noexcept -> home_queue_section;

	/// \brief Whether one item is in this queue in the supplied role.
	///
	/// \details What "My role" filters by. GitHub's own `Role` is one way in;
	/// personal discovery records the same facts as reasons beside it —
	/// `Authored`, `Requested`, `Reviewed` — and a reviewer asking for the pull
	/// requests they were requested on means both. Accepted activity also
	/// contributes submitted-review membership. An item holds several roles at
	/// once, so this is a test rather than a single answer, and `none` holds
	/// exactly when no other role does.
	[[nodiscard]] auto holds_role(const queue_item& aItem, github_role aRole) noexcept -> bool;

	/// \brief Decides which bucket one enrolled pull request belongs in.
	///
	/// \details The whole of the attention policy. It reads no clock, reaches
	/// no provider, and returns a decision rather than performing one.
	///
	/// The order of the tests is the order of the enumeration, and the first
	/// that holds wins. An item can be true of several — a pull request you
	/// authored can also have unseen comments — and a queue that placed it in
	/// all of them would ask the reviewer to notice the same work repeatedly.
	/// Placing it in the most urgent is what a queue is for.
	[[nodiscard]] auto bucket_for(
		const queue_membership&           aMembership,
		const queue_activity_observation& aActivity) noexcept
		-> attention_bucket;

	/// \brief The deterministic priority relation used by complete and incremental projections.
	///
	/// \details [[BPR-1683]]. Ignored items follow every other item; within
	/// each part the order is bucket, then identity.
	[[nodiscard]] auto queue_precedes(const queue_item& aLeft, const queue_item& aRight) -> bool;

	/// \brief Builds the ordered queue from enrolled items.
	///
	/// \details Ordered by bucket, then by identity, with ignored items last. The identity is what makes
	/// the order *total*: a queue ordered by bucket alone would leave items
	/// sharing a bucket in whatever order the host happened to read them, and
	/// a queue that reshuffles between refreshes is one a reviewer cannot keep
	/// their place in.
	///
	/// Identity is a tiebreak and not a ranking. It says nothing about which of
	/// two equally urgent reviews matters more, because nothing here could say
	/// that; it only makes the answer the same every time.
	[[nodiscard]] auto ordered_queue(std::span<const queue_item> aItems)
		-> std::vector<queue_item>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_QUEUE_ATTENTION_HPP
