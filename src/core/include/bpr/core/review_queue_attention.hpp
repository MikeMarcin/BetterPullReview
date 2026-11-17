#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_QUEUE_ATTENTION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_QUEUE_ATTENTION_HPP

#include "bpr/core/review_queue.hpp"

#include <compare>
#include <cstddef>
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

	/// \brief [[RFC-BPR-0098]]. GitHub's review decision on a pull request,
	/// as last read.
	///
	/// \details Stored by value: append only. `none` is GitHub reporting no
	/// decision, which it does when no review is required; a decision that was
	/// not read is an absent value, not `none`.
	enum class queue_review_decision : std::uint8_t {
		none,
		review_required,
		changes_requested,
		approved,
	};

	/// \brief [[RFC-BPR-0098]]. GitHub's merge state of a pull request, as
	/// last read. Stored by value: append only.
	enum class queue_merge_state : std::uint8_t {
		/// Not read, or GitHub had not computed it.
		unknown,
		clean,
		/// Branch protection refuses the merge (a review, a check, a rule).
		blocked,
		/// The head is behind a base that must be merged in first.
		behind,
		/// The head conflicts with the base.
		conflicts,
		draft,
		/// Mergeable, with hooks that run on merge.
		hooks,
		/// Mergeable, but a commit status is not passing.
		unstable,
	};

	/// \brief [[RFC-BPR-0098]]. One reviewer's standing verdict on a pull
	/// request, by login, as `my_review_for` decides one from their reviews.
	struct queue_reviewer_state
	{
		std::string       Login;
		my_review_verdict State = my_review_verdict::commented;

		auto operator<=>(const queue_reviewer_state&) const = default;
	};

	/// \brief [[RFC-BPR-0098]]. At most this many reviewers' verdicts are
	/// carried; the sentence that names them names fewer.
	inline constexpr std::size_t maximum_queue_reviewer_states = 8U;

	/// \brief [[RFC-BPR-0098]]. At most this many review threads of an
	/// authored pull request are read for conversations awaiting a reply.
	inline constexpr std::uint32_t maximum_queue_review_threads = 50U;

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
		/// [[RFC-BPR-0098]]. The review decision and merge state the two
		/// booleans above are reduced from; stale with `AssessmentStale`.
		std::optional<queue_review_decision> Decision;
		queue_merge_state                    Merge = queue_merge_state::unknown;
		/// [[RFC-BPR-0098]]. Checks still running on the head; known with
		/// `ChecksKnown`, stale with `ChecksStale`.
		std::uint32_t PendingChecks = 0U;
		/// [[RFC-BPR-0098]]. Every other reviewer's standing verdict, at most
		/// `maximum_queue_reviewer_states`, in the order they first
		/// reviewed. Never the viewer's own: on their pull request that is a
		/// reply. Meaningful only when `ReviewsKnown`.
		std::vector<queue_reviewer_state> LatestReviews;
		bool                              ReviewsKnown = false, ReviewsStale = false;
		/// [[RFC-BPR-0098]]. Unresolved review conversations whose latest
		/// comment is not the viewer's, among the last
		/// `maximum_queue_review_threads` threads read. Absent when the
		/// host could not read them. `MoreConversations` says the pull
		/// request has more threads than were read.
		std::optional<std::uint32_t> AwaitingReply;
		bool                         MoreConversations = false, ConversationsStale = false;
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

	/// \brief [[RFC-BPR-0098]]. One review another reviewer submitted, by
	/// login, in the order the provider returned them.
	struct reviewer_submitted_review
	{
		std::string         Login;
		my_submitted_review Review;

		auto operator<=>(const reviewer_submitted_review&) const = default;
	};

	/// \brief [[RFC-BPR-0098]]. Each reviewer's standing verdict, as
	/// `my_review_for` decides one, in the order they first reviewed.
	///
	/// \details At most `maximum_queue_reviewer_states`; a reviewer past
	/// that is left out. Logins compare exactly, as the provider wrote them.
	[[nodiscard]] auto latest_reviewer_states(std::span<const reviewer_submitted_review> aReviews) -> std::vector<queue_reviewer_state>;

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
		/// [[BPR-2048]]. The last read of this pull request alone exceeded the
		/// response limits, so its facts are what was last read, if anything.
		/// Home says so on its row; the rest of the refresh is unaffected.
		bool TooLarge = false;
		/// [[BPR-2049]]. The head the reviewer said Seen on, with their
		/// approval standing: commits up to it are not news to them. Local
		/// only; nothing is written to GitHub. Empty when never said.
		std::string SeenHead;

		auto operator<=>(const queue_item_facts&) const = default;
	};

	/// \brief [[BPR-1866]]. Whether the viewer's own submitted reviews make
	/// them a reviewer of a pull request.
	///
	/// \details Never on one they authored. GitHub records an author's reply
	/// in a review thread, and their comment on a line of their own diff, as
	/// a comment-only review by them; it cannot be an approval or a change
	/// request. That is the author speaking, not reviewing, and the commits
	/// they push after it are their own, so neither is news to them: no
	/// "changed since my review", no verdict, no reviewer role.
	[[nodiscard]] constexpr auto reviewed_by_viewer(const bool aReviewedByMe, const bool aAuthored) noexcept -> bool
	{
		return aReviewedByMe && !aAuthored;
	}

	/// \brief [[BPR-1866]]. The viewer's standing verdict as Home says it:
	/// none on a pull request they authored (see `reviewed_by_viewer`).
	[[nodiscard]] auto my_review_shown(const queue_membership& aMembership, const queue_my_review& aReview) noexcept -> queue_my_review;

	/// \brief Adds observed authorship and submitted-review facts to presentation without changing retained enrollment reasons.
	///
	/// \details [[BPR-1866]]. A pull request the viewer authored is never
	/// one they reviewed, whatever the retained reasons say.
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

	/// \brief [[RFC-BPR-0098]]. The one thing a pull request the viewer
	/// authored is waiting for.
	///
	/// \details The order is the decision table's: the first that holds wins,
	/// and My pull requests lists its rows in this order. Derived on every
	/// read from the item's facts and never stored.
	enum class authored_next_move : std::uint8_t {
		/// A reviewer requested changes, or a conversation awaits my reply.
		returned,
		/// A check failed.
		failing,
		/// GitHub refuses the merge and no failed check, running check or
		/// outstanding review explains it.
		blocked,
		/// Approved, every check passed, mergeable.
		ready,
		/// Comments I have not read.
		unread,
		/// Not a draft; no reviewer is requested and nobody has reviewed.
		unassigned,
		draft,
		/// Reviewers are awaited: the row's bucket is
		/// `authored_awaiting_reviewer_action`.
		waiting,
		/// Approved; checks still running.
		checking,
		quiet,
		/// Nothing about the pull request has been read yet
		/// (`attention_bucket::detail_unknown`), or none of the above holds
		/// and its reviews have not been read, so it is not known to be quiet.
		unknown,
	};

	/// \brief Whose move a next move says it is.
	enum class next_move_owner : std::uint8_t {
		mine,
		theirs,
		nobody_yet,
		nobody,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(authored_next_move aMove) noexcept -> std::string_view;

	/// \brief Whose move one next move is, by the decision table.
	[[nodiscard]] auto owner_of(authored_next_move aMove) noexcept -> next_move_owner;

	/// \brief Whether the facts say a reviewer's change request stands.
	///
	/// \details GitHub's decision when it gave one; when it reports none
	/// (no review is required), any reviewer whose standing verdict is a
	/// change request.
	[[nodiscard]] auto changes_requested(const queue_waiting_facts& aFacts) noexcept -> bool;

	/// \brief Whether the facts say the pull request is approved, as
	/// `changes_requested` reads them.
	[[nodiscard]] auto approved(const queue_waiting_facts& aFacts) noexcept -> bool;

	/// \brief [[RFC-BPR-0098]]. Decides the next move of a pull request the
	/// viewer authored; none for one they did not.
	///
	/// \details The whole of the decision table. Reads the item's bucket,
	/// draft status, unseen count and waiting facts and nothing else. An
	/// absent fact never matches a row that needs it: a pull request whose
	/// conversations could not be read is Returned only on a change request,
	/// and one whose reviews were not read is never Unassigned.
	[[nodiscard]] auto next_move_for(const queue_item& aItem) noexcept -> std::optional<authored_next_move>;

	/// \brief [[RFC-BPR-0098]]. Whether Home's "Hide quiet" hides the row:
	/// a Quiet next move for a pull request the viewer authored, otherwise a
	/// quiet bucket with no merge blocker.
	[[nodiscard]] auto home_row_quiet(const queue_item& aItem) noexcept -> bool;

	/// \brief [[BPR-1683]]. The Home section of one decided item: `ignored`
	/// for an ignored row, otherwise its membership's section.
	///
	/// \details [[RFC-BPR-0098]]. An authored row is in Waiting on others
	/// exactly when its next move is Waiting, and in My pull requests
	/// otherwise.
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

	/// \brief [[BPR-2049]]. Whether the reviewer said Seen on exactly the
	/// current head while their approval still stands.
	///
	/// \details A later push is news again: the head no longer matches. A
	/// verdict other than approved is never seen away.
	[[nodiscard]] auto head_seen(const queue_item_facts& aFacts) noexcept -> bool;

	/// \brief [[BPR-2049]]. The activity a pull request shows once the
	/// reviewer's Seen is taken into account: commits seen are not commits
	/// that moved since their review.
	[[nodiscard]] auto seen_activity(queue_activity_observation aActivity, const queue_item_facts& aFacts) noexcept -> queue_activity_observation;

	/// \brief [[BPR-2049]]. What a row back in Needs me for new commits
	/// offers the reviewer who already reviewed it.
	///
	/// \details The owner, 2026-11-10: "if I approved something already and
	/// it gets new commits it moves back to needs me which makes sense but
	/// then I don't know how to say okay I still accept it so it clears new
	/// commits and moves back to following." While GitHub still holds the
	/// approval, `seen` records the new commits as seen here and writes
	/// nothing. When GitHub dismissed it (a stale-review dismissal),
	/// `approve_again` submits a new approval. Never on a pull request of
	/// mine, and there is deliberately no way to withdraw an approval here.
	enum class review_followup : std::uint8_t {
		none,
		seen,
		approve_again,
	};

	[[nodiscard]] auto review_followup_for(const queue_item& aItem) noexcept -> review_followup;

	/// \brief [[BPR-2217]]. Where my approval stands against the commits of
	/// the pull request, as the review of it says.
	enum class review_standing : std::uint8_t {
		/// Nothing to say: not approved by me, or nothing moved since.
		none,
		/// My approval still stands on GitHub and commits followed it.
		new_commits_since_approval,
		/// GitHub dismissed my approval when commits were pushed.
		approval_dismissed,
		/// I said Seen on every commit since my approval.
		up_to_date,
	};

	/// \brief [[BPR-2217]]. What the review of a pull request says and
	/// offers about my approval and the commits since it.
	///
	/// \details The owner, 2026-11-16, about [[BPR-2049]]: "Oh well its dumb
	/// to put that on home, I'm going to look at the new commits from within
	/// the review." The review offers Seen beside Approve while my approval
	/// stands, names its Approve "Approve again" once GitHub dismissed it, and
	/// reads as up to date after Seen. Every word here is the core's; the
	/// presentation shows them as given. An empty word is nothing to show.
	struct review_followup_words
	{
		review_followup Followup = review_followup::none;
		review_standing Standing = review_standing::none;
		/// The approving verdict's name: "Approve", or "Approve again".
		std::string_view Approve = "Approve";
		/// What an enabled Approve says it does; empty keeps the review's own.
		std::string_view ApproveTip;
		/// Seen's name and what it does; empty when it is not offered.
		std::string_view Seen;
		std::string_view SeenTip;
		/// The one line the review says about my approval; empty for none.
		std::string_view Note;

		auto operator<=>(const review_followup_words&) const = default;
	};

	[[nodiscard]] auto review_followup_words_for(const queue_item& aItem) noexcept -> review_followup_words;

	/// \brief The deterministic priority relation used by complete and incremental projections.
	///
	/// \details [[BPR-1683]]. Ignored items follow every other item; within
	/// each part the order is bucket, then identity. [[RFC-BPR-0098]]. A row
	/// of My pull requests ranks by its next move instead of its bucket, after
	/// every row that needs me and before every row waiting on others.
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
