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
	enum class home_queue_section : std::uint8_t {
		needs_you,
		my_pull_requests,
		waiting_on_others,
		following,
	};

	/// \brief Classifies accepted row-scoped authorship for Home without changing attention or notification eligibility.
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

		auto operator<=>(const queue_activity_observation&) const = default;
	};

	/// \brief One queue item, with the reason it is where it is.
	///
	/// \details The reason travels with the item. A presentation that
	/// recomputed it from the same observation might agree today and disagree
	/// after either side is edited, and the reviewer would have no way to tell
	/// which was right.
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

		auto operator<=>(const queue_item_facts&) const = default;
	};

	/// \brief Adds accepted ownership to presentation without changing retained enrollment reasons.
	[[nodiscard]] auto queue_membership_with_authorship(queue_membership aMembership, std::optional<bool> aAuthored) noexcept -> queue_membership;

	struct queue_item
	{
		pull_request_ref Ref;
		queue_membership Membership;
		attention_bucket Bucket = attention_bucket::quiet;
		queue_item_facts Facts;

		auto operator<=>(const queue_item&) const = default;
	};

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
	[[nodiscard]] auto queue_precedes(const queue_item& aLeft, const queue_item& aRight) -> bool;

	/// \brief Builds the ordered queue from enrolled items.
	///
	/// \details Ordered by bucket, then by identity. The identity is what makes
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
