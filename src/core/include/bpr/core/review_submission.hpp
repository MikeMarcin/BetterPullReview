#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_SUBMISSION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_SUBMISSION_HPP

#include "bpr/core/review_round_trip.hpp"

#include <cstddef>

namespace bpr::core {
	using review_submission_draft_id   = strong_id<struct review_submission_draft_id_tag>;
	using review_submission_attempt_id = strong_id<struct review_submission_attempt_id_tag>;

	/// \brief Bounds decoded semantic UTF-8 bytes, independently of wire encoding limits.
	inline constexpr std::size_t maximum_review_submission_body_bytes = 64U * 1024U;

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

		auto operator<=>(const review_submission_proposal&) const = default;
	};

	/// \brief Decided ordinary actions for the current immutable PR-scoped draft.
	struct review_submission_controls
	{
		bool Edit                                                 = false;
		bool Submit                                               = false;
		bool Discard                                              = false;
		bool Reconcile                                            = false;
		bool NewReview                                            = false;
		auto operator<=>(const review_submission_controls&) const = default;
	};
	[[nodiscard]] auto controls(const review_submission_draft& aDraft) noexcept -> review_submission_controls;
	/// \brief [[BPR-1129]]. Whether a draft holds review work that is kept: a
	/// summary or a chosen verdict while drafting, or a submission not yet
	/// settled. An empty comment draft, a submitted and a discarded one keep
	/// nothing. RFC-BPR-0052's Compose review mark is this, presented.
	[[nodiscard]] auto kept(const review_submission_draft& aDraft) noexcept -> bool;

	/// \brief Checks the closed ordinary review disposition vocabulary.
	[[nodiscard]] auto valid(review_disposition aDisposition) noexcept -> bool;
	/// \brief Checks nonzero PR authority identities and a valid expected head.
	[[nodiscard]] auto valid(const review_mutation_target& aTarget) noexcept -> bool;
	/// \brief Checks draft invariants; submitted non-Approve text must be nonblank.
	[[nodiscard]] auto valid(const review_submission_draft& aDraft) noexcept -> bool;
	/// \brief Checks proposal identity, disposition and submission-ready text.
	[[nodiscard]] auto valid(const review_submission_proposal& aProposal) noexcept -> bool;
	/// \brief Applies one command; no effects or authority rebinding are possible.
	[[nodiscard]] auto apply(const review_submission_draft&   aDraft,
							 const review_submission_command& aCommand) -> review_submission_outcome;
	/// \brief Proposes only a valid submitting draft with its exact active attempt.
	[[nodiscard]] auto propose(const review_submission_draft& aDraft)
		-> std::optional<review_submission_proposal>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_SUBMISSION_HPP
