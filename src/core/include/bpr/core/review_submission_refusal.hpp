#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_SUBMISSION_REFUSAL_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_SUBMISSION_REFUSAL_HPP

#include "bpr/core/review_submission.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace bpr::core {
	/// \brief [[BPR-2263]]. Which check refused a review before it could be
	/// sent: the window's workflow, the application host, or the GitHub
	/// session.
	enum class review_submission_refusal_check : std::uint8_t {
		workflow,
		host,
		session,
	};

	/// \brief [[BPR-2263]]. Why a review was refused before its one request
	/// could leave. A closed set: every early return names one.
	/// \details None of these carries text from the review, the server or the
	/// account, so each may be logged.
	enum class review_submission_refusal_reason : std::uint8_t {
		/// The request was stopped (cancelled, or the window closed) first.
		request_stopped,
		/// The one-use authority the Submit gesture issued was no longer live.
		grant_inactive,
		/// The submission provider could not be built for this connection.
		provider_unavailable,
		/// The connection is for another account than the review's.
		identity_mismatch_account,
		/// The connection is for another pull request than the review's.
		identity_mismatch_pull_request,
		/// The review cannot be written as GitHub's request; the part says why.
		invalid_request,
		/// The local record that keeps a review from being sent twice could
		/// not be read or written.
		recovery_record_unavailable,
		/// The connection lease the submission holds was no longer live.
		connection_lease_inactive,
		/// No connection or credential could carry the request now.
		connection_unavailable,
		/// The pull request's base moved since the review was written.
		target_moved,
		/// The pull request's head moved since the review was written.
		stale_head,
		/// GitHub's review list, read before sending, could not be asked for.
		review_list_unavailable,
		/// The account or pull request the window shows is no longer the one
		/// the review was written for.
		binding_changed,
		/// The pending comments could not be held for this review.
		comments_not_held,
		/// The host did not accept the request (busy, closing or unauthorized).
		admission_refused,
	};
	inline constexpr std::size_t review_submission_refusal_reason_count = 15U;

	/// \brief [[BPR-2263]]. Which part of the review makes it invalid.
	enum class review_submission_refusal_part : std::uint8_t {
		none,
		/// The verdict needs a summary and it has none.
		summary_missing,
		summary_too_long,
		/// The summary is not text GitHub takes.
		summary_not_text,
		comment_body_missing,
		comment_body_too_long,
		comment_not_text,
		/// The comment's file path is not one GitHub takes.
		comment_path,
		/// The comment's line range does not hold together, or it rides twice.
		comment_place,
		too_many_comments,
		review_too_large,
		/// Something else in the request (its commit, account or pull
		/// request) could not be written.
		request,
	};
	inline constexpr std::size_t review_submission_refusal_part_count = 12U;

	/// \brief [[BPR-2263]]. One refusal before sending: the check, its
	/// reason, and for an invalid request the part and the comment (1-based
	/// in the review's order, 0 for none).
	struct review_submission_refusal
	{
		review_submission_refusal_check  Check   = review_submission_refusal_check::session;
		review_submission_refusal_reason Reason  = review_submission_refusal_reason::request_stopped;
		review_submission_refusal_part   Part    = review_submission_refusal_part::none;
		std::uint16_t                    Comment = 0U;

		auto operator<=>(const review_submission_refusal&) const = default;
	};

	/// \brief Stable lower-case names, empty outside each set.
	[[nodiscard]] auto name(review_submission_refusal_check aCheck) noexcept -> std::string_view;
	[[nodiscard]] auto name(review_submission_refusal_reason aReason) noexcept -> std::string_view;
	[[nodiscard]] auto name(review_submission_refusal_part aPart) noexcept -> std::string_view;
	[[nodiscard]] auto review_submission_refusal_check_named(std::string_view aName) noexcept -> std::optional<review_submission_refusal_check>;
	[[nodiscard]] auto review_submission_refusal_reason_named(std::string_view aName) noexcept -> std::optional<review_submission_refusal_reason>;
	[[nodiscard]] auto review_submission_refusal_part_named(std::string_view aName) noexcept -> std::optional<review_submission_refusal_part>;
	/// \brief Known names; a part only with an invalid request; a comment
	/// only with a comment's part, at most `maximum_review_submission_comments`.
	[[nodiscard]] auto valid(const review_submission_refusal& aRefusal) noexcept -> bool;

	/// \brief [[BPR-2263]]. Which part of this review GitHub's request
	/// cannot carry, by the rules `review_comments_rejection` and the
	/// summary's own rule apply: an invalid request (checked by the
	/// session) whose part is `none` when every rule holds.
	[[nodiscard]] auto review_submission_content_refusal(std::string_view aSummary, review_disposition aDisposition,
														 std::span<const review_submission_comment> aComments) noexcept -> review_submission_refusal;

	/// \brief [[BPR-2263]]. What the window does about a refusal before
	/// telling the reviewer.
	enum class review_submission_repair : std::uint8_t {
		/// Nothing: the reviewer is told why.
		none,
		/// The connection is the application's own: build the submission
		/// connection again for the current binding and send the same
		/// request, under the same idempotency key, once.
		reconnect_and_retry,
		/// The commit under the review changed: read the pull request again
		/// and let the reviewer confirm. Never sent without them.
		reread_and_confirm,
	};
	/// \brief [[BPR-2263]]. The repair for a refusal.
	/// \details `aRepaired` is true when this attempt was already repaired
	/// once: it is never retried again. `aSameAuthority` is true when the
	/// review's account and pull request are the ones the window holds now;
	/// only then is a connection or identity mismatch the application's own.
	[[nodiscard]] auto decide_review_submission_repair(const review_submission_refusal& aRefusal, bool aRepaired, bool aSameAuthority) noexcept -> review_submission_repair;

	/// \brief [[BPR-2263]]. What the sentence is about.
	enum class review_submission_refusal_subject : std::uint8_t {
		/// A whole review from Compose review.
		review,
		/// One comment published inline.
		comment,
	};
	/// \brief [[BPR-2263]]. What a sentence may name besides the reason: the
	/// review's comments (for the one an invalid part names) and whether
	/// the pull request is being read again.
	struct review_submission_refusal_context
	{
		review_submission_refusal_subject          Subject = review_submission_refusal_subject::review;
		std::span<const review_submission_comment> Comments;
		bool                                       Rereading = false;
	};
	/// \brief [[BPR-2263]]. The one sentence the page shows for a refusal:
	/// what happened and what the reviewer can do. It never blames the
	/// review's contents for a cause outside them, and a cause the page
	/// cannot explain points to Copy diagnostics.
	[[nodiscard]] auto sentence(const review_submission_refusal& aRefusal, const review_submission_refusal_context& aContext) -> std::string;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_SUBMISSION_REFUSAL_HPP
