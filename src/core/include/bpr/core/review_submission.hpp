#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_SUBMISSION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_SUBMISSION_HPP

#include "bpr/core/review_round_trip.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace bpr::core {
	using review_submission_draft_id   = strong_id<struct review_submission_draft_id_tag>;
	using review_submission_attempt_id = strong_id<struct review_submission_attempt_id_tag>;

	/// \brief Bounds decoded semantic UTF-8 bytes, independently of wire encoding limits.
	inline constexpr std::size_t maximum_review_submission_body_bytes = 64U * 1024U;
	/// \brief [[BPR-1345]]. At most this many line comments ride one review.
	inline constexpr std::size_t maximum_review_submission_comments = 50U;
	/// \brief [[BPR-1345]]. The summary, every comment's body and every path
	/// together, so the review's one request stays inside the transports'
	/// 64 KiB request limit with room for its JSON.
	inline constexpr std::size_t maximum_review_submission_content_bytes = 56U * 1024U;

	/// \brief [[BPR-1345]]. One pending draft the review carries as a GitHub
	/// review comment.
	/// \details `Line` is the zero-based line of the draft's anchor (GitHub's
	/// `line` is one more), `StartLine` the first line of a range on the same
	/// side (GitHub's `start_line`), and `Side` GitHub's `side`. `Draft` and
	/// `DraftRevision` are the exact private draft and the revision whose text
	/// `Body` is; only they may be marked published by this review.
	struct review_submission_comment
	{
		review_draft_id              Draft;
		std::uint64_t                DraftRevision = 0U;
		std::string                  Path;
		diff_side                    Side = diff_side::right;
		std::uint32_t                Line = 0U;
		std::optional<std::uint32_t> StartLine;
		std::string                  Body;

		auto operator<=>(const review_submission_comment&) const = default;
	};

	/// \brief Names the private draft, pending submission, and terminal states.
	enum class review_submission_state : std::uint8_t {
		drafting,
		submitting,
		submitted,
		discarded,
	};

	/// \brief Holds one whole-PR review without a source-line anchor.
	/// \details Creation binds Id, Target and ActingAccount; commands never rebind
	/// them. Attempt starts at zero and retains the last issued identity after a
	/// definitive failure. Callers retain the returned draft on the decision lane.
	struct review_submission_draft
	{
		review_submission_draft_id   Id;
		review_mutation_target       Target;
		github_account_id            ActingAccount;
		std::string                  Body;
		review_disposition           Disposition = review_disposition::comment;
		review_submission_state      State       = review_submission_state::drafting;
		review_submission_attempt_id Attempt;
		/// [[BPR-1345]]. The pending drafts the review carries, set while it
		/// is written and frozen by Submit.
		std::vector<review_submission_comment> Comments = { };

		auto operator<=>(const review_submission_draft&) const = default;
	};

	/// \brief Names commands and exact, host-observed submission terminals.
	enum class review_submission_command_kind : std::uint8_t {
		edit,
		choose_disposition,
		discard,
		begin_submit,
		complete_submit,
		fail_submit,
		/// [[BPR-1345]]. Replaces the carried comments while drafting.
		set_comments,
	};

	/// \brief Carries one command without any authority replacement fields.
	/// \details Terminals require the exact nonzero Attempt; other commands use
	/// zero. fail_submit means definitively not published. An indeterminate write
	/// must remain submitting until host reconciliation establishes its outcome;
	/// a timeout alone cannot authorize fail_submit or a retry.
	struct review_submission_command
	{
		review_submission_draft_id     Id;
		review_submission_command_kind Kind = review_submission_command_kind::edit;
		std::string                    Body;
		review_disposition             Disposition = review_disposition::comment;
		review_submission_attempt_id   Attempt;
		/// [[BPR-1345]]. The comments `set_comments` carries.
		std::vector<review_submission_comment> Comments = { };

		auto operator<=>(const review_submission_command&) const = default;
	};

	/// \brief Names fail-closed validation and command refusal reasons.
	enum class review_submission_rejection : std::uint8_t {
		none,
		invalid_identity,
		unknown_draft,
		invalid_state,
		invalid_command,
		invalid_disposition,
		invalid_utf8,
		body_too_large,
		blank_body,
		already_submitting,
		submitted_draft,
		discarded_draft,
		not_submitting,
		stale_attempt,
		attempt_exhausted,
		/// [[BPR-1345]]. A comment with no draft, revision or path, invalid
		/// text, a side GitHub has not, a range that does not end below its
		/// start, or one draft carried twice.
		invalid_comment,
		too_many_comments,
		/// A carried comment with no words.
		blank_comment,
		/// The summary, the comments and their paths together are too long
		/// for one request.
		review_too_large,
	};

	/// \brief Reports accepted state or a refusal without copying the input.
	/// \details Draft is absent on refusal; the caller retains its existing state.
	/// Accepted no-op commands still return a draft with Changed false.
	struct review_submission_outcome
	{
		std::optional<review_submission_draft> Draft;
		review_submission_rejection            Rejection = review_submission_rejection::none;
		bool                                   Changed   = false;

		auto operator<=>(const review_submission_outcome&) const = default;
	};

	/// \brief Carries the exact semantic content of an active submission attempt.
	struct review_submission_proposal
	{
		review_submission_draft_id   Draft;
		review_submission_attempt_id Attempt;
		review_mutation_target       Target;
		github_account_id            ActingAccount;
		std::string                  Body;
		review_disposition           Disposition = review_disposition::comment;
		/// [[BPR-1345]]. Sent as the review's comments, in this order.
		std::vector<review_submission_comment> Comments = { };

		auto operator<=>(const review_submission_proposal&) const = default;
	};

	/// \brief Decided ordinary actions for the current immutable PR-scoped draft.
	struct review_submission_controls
	{
		bool Edit      = false;
		bool Submit    = false;
		bool Discard   = false;
		bool Reconcile = false;
		bool NewReview = false;
		/// [[BPR-1345]] v002. Whether the review can be submitted now as each
		/// verdict, in the order Comment, Approve, Request changes: the three
		/// buttons that finish a review submit directly, each its own verdict.
		std::array<bool, 3> SubmitAs                                             = { };
		auto                operator<=>(const review_submission_controls&) const = default;
	};
	[[nodiscard]] auto controls(const review_submission_draft& aDraft) noexcept -> review_submission_controls;
	/// \brief [[BPR-1345]] v002. Whether `aControls` let the review be
	/// submitted as `aDisposition`.
	[[nodiscard]] auto submits_as(const review_submission_controls& aControls, review_disposition aDisposition) noexcept -> bool;
	/// \brief [[BPR-1129]]. Whether a draft holds review work that is kept: a
	/// summary or a chosen verdict while drafting, or a submission not yet
	/// settled. An empty comment draft, a submitted and a discarded one keep
	/// nothing. RFC-BPR-0052's Compose review mark is this, presented.
	[[nodiscard]] auto kept(const review_submission_draft& aDraft) noexcept -> bool;

	/// \brief Checks the closed ordinary review disposition vocabulary.
	[[nodiscard]] auto valid(review_disposition aDisposition) noexcept -> bool;
	/// \brief Checks nonzero PR authority identities and a valid expected head.
	[[nodiscard]] auto valid(const review_mutation_target& aTarget) noexcept -> bool;
	/// \brief [[BPR-1345]]. Whether a review with this disposition and these
	/// comments needs summary text: Request changes always, Comment only when
	/// it carries no line comment (GitHub refuses that with no text, probed on
	/// `MikeMarcin/BPR-Test#41`), Approve never.
	[[nodiscard]] auto summary_required(review_disposition aDisposition, std::span<const review_submission_comment> aComments) noexcept -> bool;
	/// \brief [[BPR-1345]]. Why these comments cannot ride a review with this
	/// summary, or none. With `aSubmitting` every comment must have words.
	[[nodiscard]] auto review_comments_rejection(std::string_view aSummary, std::span<const review_submission_comment> aComments, bool aSubmitting) noexcept
		-> review_submission_rejection;
	/// \brief Checks draft invariants; submitted text must be nonblank where
	/// `summary_required` says so.
	[[nodiscard]] auto valid(const review_submission_draft& aDraft) noexcept -> bool;
	/// \brief Checks proposal identity, disposition and submission-ready text.
	[[nodiscard]] auto valid(const review_submission_proposal& aProposal) noexcept -> bool;
	/// \brief Applies one command; no effects or authority rebinding are possible.
	[[nodiscard]] auto apply(const review_submission_draft&   aDraft,
							 const review_submission_command& aCommand) -> review_submission_outcome;
	/// \brief Proposes only a valid submitting draft with its exact active attempt.
	[[nodiscard]] auto propose(const review_submission_draft& aDraft)
		-> std::optional<review_submission_proposal>;

	/// \brief [[BPR-1816]]. Where a review finished with one verdict leaves
	/// the reviewer once GitHub confirms it.
	enum class review_finish_return : std::uint8_t {
		/// The review closes and Home shows: the default (owner, 2026-10-29).
		home,
		/// The page closes and the pull request stays open, as before.
		pull_request,
	};

	/// \brief [[BPR-1816]]. One choice per finishing verdict, each its own
	/// Settings row: the three buttons that finish a review are three
	/// separate gestures, so none shares another's choice.
	struct review_finish_settings
	{
		review_finish_return Comment        = review_finish_return::home;
		review_finish_return Approve        = review_finish_return::home;
		review_finish_return RequestChanges = review_finish_return::home;

		auto operator<=>(const review_finish_settings&) const = default;
	};
	[[nodiscard]] auto valid(const review_finish_settings& aSettings) noexcept -> bool;
	/// \brief [[BPR-1816]]. The choice for `aDisposition`; an unknown verdict
	/// stays on the pull request.
	[[nodiscard]] auto finish_return(const review_finish_settings& aSettings, review_disposition aDisposition) noexcept -> review_finish_return;

	/// \brief [[BPR-1816]]. What GitHub's answer to a submission established.
	enum class review_finish_answer : std::uint8_t {
		/// Published, with a receipt that matches the exact request.
		published,
		/// Definitely not published: the draft is kept for another try.
		not_published,
		/// Unknown or partial: Check status settles it; nothing is retried.
		unknown,
	};
	/// \brief [[BPR-1816]]. What the answer answers: the reviewer's own
	/// Submit, or a recovery (Check status, or an unfinished review found on
	/// opening the pull request).
	enum class review_finish_origin : std::uint8_t {
		submit,
		recovery,
	};
	struct review_finish_event
	{
		review_disposition   Disposition = review_disposition::comment;
		review_finish_answer Answer      = review_finish_answer::unknown;
		review_finish_origin Origin      = review_finish_origin::submit;
		/// Another of this pull request's submissions still awaits its answer,
		/// and the page shows it next.
		bool AnotherUnsettled = false;

		auto operator<=>(const review_finish_event&) const = default;
	};
	/// \brief [[BPR-1816]]. What the window does with a finished review.
	enum class review_finish_transition : std::uint8_t {
		/// The page stays, with its summary, verdict and comments.
		keep_review,
		/// The page closes and the pull request stays open ([[BPR-1345]] v003).
		return_to_pull_request,
		/// The page closes, the review closes and Home shows.
		close_review_and_go_home,
	};
	/// \brief [[BPR-1816]]. Only a published answer to the reviewer's own
	/// Submit leaves the pull request, and only when that verdict's choice is
	/// Home and no other submission is waiting to be settled on it. Any other
	/// published answer returns to the pull request; an unpublished or
	/// unknown one keeps the review.
	[[nodiscard]] auto decide_review_finish(const review_finish_settings& aSettings, const review_finish_event& aEvent) noexcept -> review_finish_transition;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_SUBMISSION_HPP
