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

	/// \brief [[BPR-1715]]. What GitHub answered an attempt's one POST, as
	/// the attempt's record keeps it from the moment the POST may leave.
	/// \details `Answer` is `success`, `error_status` or `no_answer`, never
	/// `not_sent`: a record is written only for a POST that may have been
	/// sent, first as `no_answer` before it leaves, then with the answer.
	/// `StatusClass` is the hundreds digit of the answer's status (2 to 5),
	/// 0 with no answer. `RecordedUtcMs` is when the record was last written,
	/// in UTC milliseconds since the Unix epoch.
	struct review_submission_post_record
	{
		review_submission_post_answer Answer        = review_submission_post_answer::no_answer;
		std::uint8_t                  StatusClass   = 0U;
		std::int64_t                  RecordedUtcMs = 0;

		auto operator<=>(const review_submission_post_record&) const = default;
	};
	/// \brief [[BPR-1715]]. How long after a POST with no definite
	/// answer (none, a redirect or a server error) GitHub's list is trusted to
	/// show the review if GitHub created it.
	inline constexpr std::int64_t review_submission_settle_delay_ms = std::int64_t{ 10 } * 60 * 1000;
	/// \brief [[BPR-1715]]. Whether the answer is in range: a sent
	/// answer, a class that matches it (2 for success, 3 to 5 for an error
	/// status, 0 with no answer) and a time that is not negative.
	[[nodiscard]] auto valid(const review_submission_post_record& aPost) noexcept -> bool;
	/// \brief [[BPR-1715]]. Whether an unresolved attempt that no review
	/// GitHub lists can be is definitely not published, so its drafts are
	/// released, unchanged, to be submitted again.
	/// \details The caller has read GitHub's whole review list and found no
	/// review that could be this attempt. That settles it when:
	/// - GitHub refused the POST with a 4xx status: it creates a review whole
	///   or not at all, and a refused one not at all;
	/// - the record holds no answer (`aPost` none): the POST never left, as
	///   the witness that lets it leave keeps "no answer" first;
	/// - the POST had no definite answer and `review_submission_settle_delay_ms`
	///   have passed since its record was written. An attempt kept before
	///   answers were kept is migrated as no answer at time zero.
	///
	/// A POST GitHub answered with success, and an invalid record, never
	/// settle here: GitHub created a review, and only a reviewer looking at
	/// GitHub can say which. A clock earlier than the record waits.
	[[nodiscard]] auto definitely_unpublished(const std::optional<review_submission_post_record>& aPost, std::int64_t aNowUtcMs) noexcept -> bool;

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

	/// \brief [[BPR-1541]]. Which count, if any, the reviewer is told with a
	/// reason.
	enum class review_submission_unknown_count : std::uint8_t {
		none,
		/// "2 of 3 comments matched": the new review holds as many comments as
		/// this one carried, and only some match.
		matched_of_carried,
		/// "it holds 2 comments, not 3": the new review holds another number.
		held_of_carried,
	};

	/// \brief [[BPR-1541]]. What the compose page says about an unknown
	/// review: the line to show, chosen by the core, with only the numbers
	/// that line uses. Qt words it.
	struct review_submission_unknown_notice
	{
		review_submission_unknown_reason Reason = review_submission_unknown_reason::review_not_listed;
		review_submission_unknown_count  Count  = review_submission_unknown_count::none;
		std::uint16_t                    Value  = 0U;
		std::uint16_t                    Of     = 0U;
		/// For `http_status`: 4 when GitHub refused the read, 5 when its
		/// server failed, otherwise 0.
		std::uint8_t StatusClass = 0U;

		auto operator<=>(const review_submission_unknown_notice&) const = default;
	};
	/// \brief [[BPR-1541]]. The line for a valid diagnosis: its reason, the
	/// comment counts only for `comments_mismatch` that carried comments, and
	/// the status class only for `http_status` when it is 4 or 5. Nothing
	/// else in the diagnosis reaches the page.
	[[nodiscard]] auto notice(const review_submission_unknown_diagnosis& aDiagnosis) noexcept -> review_submission_unknown_notice;

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

	/// \brief [[BPR-PENDING-AD]]. How a submission run ended when its outcome
	/// is known. An unknown outcome is a `review_submission_unknown_diagnosis`.
	enum class review_submission_ending : std::uint8_t {
		/// GitHub lists the review: this run published it or found it.
		published,
		/// GitHub refused the POST with a 4xx answer; it was not published.
		refused,
		/// [[BPR-1715]]. No review GitHub lists can be it, and the kept answer
		/// (none, or no definite answer once the delay passed) settles it as
		/// not published.
		settled,
		/// It stopped before its POST could leave; it was not published.
		not_sent,
	};
	/// \brief [[BPR-PENDING-AD]]. Which run ended it: Submit, or a later
	/// Check status or restart.
	enum class review_submission_run : std::uint8_t {
		submit,
		check,
	};
	/// \brief [[BPR-PENDING-AD]]. What a refusal said, as far as it is read
	/// ([[BPR-1715]]); GitHub names no comment.
	enum class review_submission_ending_refusal : std::uint8_t {
		none,
		review,
		unplaceable_line,
	};
	/// \brief [[BPR-PENDING-AD]]. Why a run that sent nothing stopped.
	enum class review_submission_stop : std::uint8_t {
		none,
		/// The pull request's head moved on before the POST.
		stale_head,
		/// The connection found the pull request moved and refused to send.
		target_moved,
		cancelled,
		/// The account or pull request is not the one the review was written for.
		undesignated_target,
		/// A read or the local record before the POST failed.
		unavailable,
		other,
	};
	/// \brief [[BPR-PENDING-AD]]. A known ending with counts only: no
	/// host, repository, account, URL or review text.
	/// \details `PostStatusClass` is the hundreds digit of GitHub's answer to
	/// this run's POST, or 0 with none; `Comments` is how many line comments
	/// the review carried; `Duplicate` marks a review found already published
	/// rather than published by this run.
	struct review_submission_ending_diagnosis
	{
		review_submission_ending         Ending          = review_submission_ending::published;
		review_submission_run            Run             = review_submission_run::submit;
		review_submission_post_answer    PostAnswer      = review_submission_post_answer::not_sent;
		std::uint8_t                     PostStatusClass = 0U;
		review_submission_ending_refusal Refusal         = review_submission_ending_refusal::none;
		review_submission_stop           Stop            = review_submission_stop::none;
		std::uint16_t                    Comments        = 0U;
		bool                             Duplicate       = false;

		auto operator<=>(const review_submission_ending_diagnosis&) const = default;
	};
	/// \brief Whether every field is in range: known names, a POST status
	/// class (0 or 2 to 5) only with an answered POST, a refusal only for a
	/// refused review, a stop only for one not sent, a duplicate only when
	/// published, at most `maximum_review_submission_comments` comments.
	[[nodiscard]] auto valid(const review_submission_ending_diagnosis& aDiagnosis) noexcept -> bool;
	/// \brief The ending as one line of fixed keys, enums and decimal counts:
	/// `review_submission_ending=ENDING run=RUN post=ANSWER post_class=N
	/// refusal=REFUSAL stop=STOP comments=N duplicate=0|1`. Empty when not valid.
	[[nodiscard]] auto describe(const review_submission_ending_diagnosis& aDiagnosis) -> std::string;
	/// \brief Reads exactly what `describe` writes; anything else is none.
	[[nodiscard]] auto parse_review_submission_ending(std::string_view aText) -> std::optional<review_submission_ending_diagnosis>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_SUBMISSION_UNKNOWN_HPP
