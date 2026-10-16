#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_SUBMISSION_UNKNOWN_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_SUBMISSION_UNKNOWN_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace bpr::core {
	/// \brief [[BPR-1540]]. Why a submitted review's outcome is still unknown:
	/// the first read that could not establish it. A closed set; the host
	/// names the one that stopped its reconciliation.
	/// \details None of these carries text from the server or the review: no
	/// host, repository, URL, account name, token or comment words. They are
	/// recorded with the attempt and in the diagnostics log, where a protected
	/// GitHub Enterprise Server's names must not appear.
	enum class review_submission_unknown_reason : std::uint8_t {
		/// No new review of mine at the original head with this summary and
		/// verdict is listed. GitHub may still be creating it.
		review_not_listed,
		/// More than one new review of mine matches; none is chosen.
		several_reviews_listed,
		/// The review list could not be read as GitHub's list.
		review_list_unreadable,
		/// The review list was larger than its bound.
		review_list_too_large,
		/// The review list continues on another page (over 100 reviews).
		review_list_paged,
		/// A page of the pull request's review comments could not be read.
		comment_page_unreadable,
		/// A page of review comments was larger than its bound.
		comment_page_too_large,
		/// A page named a next page that is not this list's.
		comment_page_link_refused,
		/// The review comments continue past the pages read.
		comment_collection_too_long,
		/// The new review does not hold exactly this review's comments; the
		/// counts say how many it holds and how many matched.
		comments_mismatch,
		/// A comment of the review was moved off the review's head and the
		/// server names no original position for it.
		comment_moved_without_original_position,
		/// A read was answered with a status other than success.
		http_status,
		rate_limited,
		timeout,
		network,
		/// The signed-in account is not the one the review was submitted as.
		account_mismatch,
		/// The pull request read back is not the one the review was sent to.
		pull_request_mismatch,
		/// No connection, credential or permission to read now.
		connection_unavailable,
		/// The local record of the attempt could not be read or written.
		local_record_unavailable,
		cancelled,
	};
	inline constexpr std::size_t review_submission_unknown_reason_count = 20U;

	/// \brief [[BPR-1540]]. What GitHub answered to the POST in the run that
	/// recorded the reason; `not_sent` when it was Check status or a restart.
	enum class review_submission_post_answer : std::uint8_t {
		not_sent,
		success,
		error_status,
		no_answer,
	};

	/// \brief [[BPR-1540]]. One reason with counts only.
	/// \details `HttpStatusClass` and `PostStatusClass` are the hundreds digit
	/// of a status (2 to 5), or 0 when none was observed. The counts are of
	/// comments the review carries, the new review holds and that matched
	/// before the match stopped; of new reviews of mine outside the baseline;
	/// and of review comment pages read.
	struct review_submission_unknown_diagnosis
	{
		review_submission_unknown_reason Reason           = review_submission_unknown_reason::review_not_listed;
		review_submission_post_answer    PostAnswer       = review_submission_post_answer::not_sent;
		std::uint8_t                     PostStatusClass  = 0U;
		std::uint8_t                     HttpStatusClass  = 0U;
		std::uint16_t                    CommentsExpected = 0U;
		std::uint16_t                    CommentsFound    = 0U;
		std::uint16_t                    CommentsMatched  = 0U;
		std::uint16_t                    CandidateReviews = 0U;
		std::uint16_t                    PagesRead        = 0U;

		auto operator<=>(const review_submission_unknown_diagnosis&) const = default;
	};

	/// \brief The stable lower-case name of a reason, empty for a value
	/// outside the set.
	[[nodiscard]] auto name(review_submission_unknown_reason aReason) noexcept -> std::string_view;
	/// \brief The stable name of a POST answer, empty outside the set.
	[[nodiscard]] auto name(review_submission_post_answer aAnswer) noexcept -> std::string_view;
	/// \brief The reason a stable name names.
	[[nodiscard]] auto review_submission_unknown_reason_named(std::string_view aName) noexcept -> std::optional<review_submission_unknown_reason>;
	/// \brief Whether every field is in range: a known reason and answer,
	/// status classes 0 or 2 to 5, a POST status class only with an answered
	/// POST, at most `maximum_review_submission_comments` comments, matched
	/// no more than found or expected.
	[[nodiscard]] auto valid(const review_submission_unknown_diagnosis& aDiagnosis) noexcept -> bool;
	/// \brief The diagnosis as one line of fixed keys, enums and decimal
	/// counts, for the diagnostics log:
	/// `review_submission=REASON post=ANSWER post_class=N http_class=N
	/// expected=N found=N matched=N candidates=N pages=N`. Empty when the
	/// diagnosis is not valid.
	[[nodiscard]] auto describe(const review_submission_unknown_diagnosis& aDiagnosis) -> std::string;
	/// \brief Reads exactly what `describe` writes; anything else is none.
	[[nodiscard]] auto parse_review_submission_unknown(std::string_view aText) -> std::optional<review_submission_unknown_diagnosis>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_SUBMISSION_UNKNOWN_HPP
