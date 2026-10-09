#include "bpr/core/review_submission.hpp"

#include <limits>

namespace bpr::core {
	namespace {
		using rejection = review_submission_rejection;
		using state     = review_submission_state;
		using kind      = review_submission_command_kind;

		[[nodiscard]] auto next_code_point(const std::string_view aText,
										   std::size_t&           aPosition) noexcept -> std::optional<std::uint32_t>
		{
			const auto first = static_cast<unsigned char>(aText[aPosition++]);
			if (first < 0x80U) {
				return first;
			}
			std::size_t   continuationCount = 0U;
			std::uint32_t codePoint         = 0U;
			std::uint32_t minimum           = 0U;
			if (first >= 0xC2U && first <= 0xDFU) {
				continuationCount = 1U;
				codePoint         = first & 0x1FU;
				minimum           = 0x80U;
			} else if (first >= 0xE0U && first <= 0xEFU) {
				continuationCount = 2U;
				codePoint         = first & 0x0FU;
				minimum           = 0x800U;
			} else if (first >= 0xF0U && first <= 0xF4U) {
				continuationCount = 3U;
				codePoint         = first & 0x07U;
				minimum           = 0x10000U;
			} else {
				return std::nullopt;
			}
			if (continuationCount > aText.size() - aPosition) {
				return std::nullopt;
			}
			for (std::size_t index = 0U; index < continuationCount; ++index) {
				const auto byte = static_cast<unsigned char>(aText[aPosition++]);
				if ((byte & 0xC0U) != 0x80U) {
					return std::nullopt;
				}
				codePoint = (codePoint << 6U) | (byte & 0x3FU);
			}
			if (codePoint < minimum || codePoint > 0x10FFFFU
				|| (codePoint >= 0xD800U && codePoint <= 0xDFFFU)) {
				return std::nullopt;
			}
			return codePoint;
		}

		// Unicode White_Space, fixed independently of locale and Qt versions.
		[[nodiscard]] auto whitespace(const std::uint32_t aPoint) noexcept -> bool
		{
			return (aPoint >= 0x09U && aPoint <= 0x0DU) || aPoint == 0x20U
				|| aPoint == 0x85U || aPoint == 0xA0U || aPoint == 0x1680U
				|| (aPoint >= 0x2000U && aPoint <= 0x200AU)
				|| aPoint == 0x2028U || aPoint == 0x2029U || aPoint == 0x202FU
				|| aPoint == 0x205FU || aPoint == 0x3000U;
		}

		[[nodiscard]] auto validate_body(const std::string_view aBody,
										 const bool             aRequireNonblank) noexcept -> rejection
		{
			if (aBody.size() > maximum_review_submission_body_bytes) {
				return rejection::body_too_large;
			}
			bool        blank    = true;
			std::size_t position = 0U;
			while (position < aBody.size()) {
				const auto point = next_code_point(aBody, position);
				if (!point) {
					return rejection::invalid_utf8;
				}
				blank = blank && whitespace(*point);
			}
			return aRequireNonblank && blank ? rejection::blank_body : rejection::none;
		}

		[[nodiscard]] auto valid_utf8(const std::string_view aText) noexcept -> bool
		{
			std::size_t position = 0U;
			while (position < aText.size()) {
				if (!next_code_point(aText, position)) {
					return false;
				}
			}
			return true;
		}

		/// [[BPR-1345]]. The summary's own rule, then the carried comments'.
		[[nodiscard]] auto validate_review(const std::string_view aBody, const review_disposition aDisposition,
										   const std::span<const review_submission_comment> aComments, const bool aSubmitting) noexcept -> rejection
		{
			if (const auto reason = review_comments_rejection(aBody, aComments, aSubmitting); reason != rejection::none) {
				return reason;
			}
			return validate_body(aBody, aSubmitting && summary_required(aDisposition, aComments));
		}

		[[nodiscard]] auto validate_draft_identity(const review_submission_draft& aDraft) noexcept -> rejection
		{
			if (aDraft.Id.value() == 0U || !valid(aDraft.Target)
				|| aDraft.ActingAccount.value() == 0U) {
				return rejection::invalid_identity;
			}
			if (!valid(aDraft.Disposition)) {
				return rejection::invalid_disposition;
			}
			switch (aDraft.State) {
			case state::drafting:
			case state::discarded:
				return rejection::none;
			case state::submitting:
			case state::submitted:
				if (aDraft.Attempt.value() == 0U) {
					return rejection::invalid_identity;
				}
				return rejection::none;
			}
			return rejection::invalid_state;
		}

		[[nodiscard]] auto validate_draft(const review_submission_draft& aDraft) noexcept -> rejection
		{
			if (const auto reason = validate_draft_identity(aDraft); reason != rejection::none) {
				return reason;
			}
			const auto pendingOrSubmitted = aDraft.State == state::submitting || aDraft.State == state::submitted;
			return validate_review(aDraft.Body, aDraft.Disposition, aDraft.Comments, pendingOrSubmitted);
		}

		[[nodiscard]] auto validate_command(const review_submission_command& aCommand) noexcept -> rejection
		{
			if (aCommand.Id.value() == 0U) {
				return rejection::invalid_identity;
			}
			if (!valid(aCommand.Disposition)) {
				return rejection::invalid_disposition;
			}
			switch (aCommand.Kind) {
			case kind::edit:
			case kind::choose_disposition:
			case kind::discard:
			case kind::begin_submit:
			case kind::set_comments:
				return aCommand.Attempt.value() == 0U ? rejection::none : rejection::invalid_identity;
			case kind::complete_submit:
			case kind::fail_submit:
				return aCommand.Attempt.value() != 0U ? rejection::none : rejection::invalid_identity;
			}
			return rejection::invalid_command;
		}

		[[nodiscard]] auto command_rejection(const review_submission_draft&   aDraft,
											 const review_submission_command& aCommand) noexcept -> rejection
		{
			// Correlation is independent of summary size, before content validation.
			if (aDraft.Id.value() != 0U && aCommand.Id.value() != 0U && aCommand.Id != aDraft.Id) {
				return rejection::unknown_draft;
			}
			if (aDraft.State == state::submitting && aDraft.Attempt.value() != 0U
				&& aCommand.Attempt.value() != 0U && aCommand.Attempt != aDraft.Attempt
				&& (aCommand.Kind == kind::complete_submit || aCommand.Kind == kind::fail_submit)) {
				return rejection::stale_attempt;
			}
			if (const auto reason = validate_draft_identity(aDraft); reason != rejection::none) {
				return reason;
			}
			if (const auto reason = validate_command(aCommand); reason != rejection::none) {
				return reason;
			}
			if (aDraft.State == state::submitted) {
				return rejection::submitted_draft;
			}
			if (aDraft.State == state::discarded) {
				return rejection::discarded_draft;
			}
			if (aCommand.Kind == kind::complete_submit || aCommand.Kind == kind::fail_submit) {
				if (aDraft.State != state::submitting) {
					return rejection::not_submitting;
				}
				return validate_review(aDraft.Body, aDraft.Disposition, aDraft.Comments, true);
			}
			if (aDraft.State == state::submitting) {
				return rejection::already_submitting;
			}
			if (const auto reason = validate_body(aDraft.Body, false); reason != rejection::none) {
				return reason;
			}
			if (aCommand.Kind == kind::edit) {
				if (const auto reason = validate_body(aCommand.Body, false); reason != rejection::none) {
					return reason;
				}
				return review_comments_rejection(aCommand.Body, aDraft.Comments, false) == rejection::review_too_large ? rejection::review_too_large : rejection::none;
			}
			if (aCommand.Kind == kind::set_comments) {
				return review_comments_rejection(aDraft.Body, aCommand.Comments, false);
			}
			if (aCommand.Kind == kind::begin_submit) {
				if (aDraft.Attempt.value() == std::numeric_limits<std::uint64_t>::max()) {
					return rejection::attempt_exhausted;
				}
				return validate_review(aDraft.Body, aDraft.Disposition, aDraft.Comments, true);
			}
			return rejection::none;
		}
	} // namespace

	auto summary_required(const review_disposition aDisposition, const std::span<const review_submission_comment> aComments) noexcept -> bool
	{
		return aDisposition == review_disposition::request_changes || (aDisposition == review_disposition::comment && aComments.empty());
	}

	auto review_comments_rejection(const std::string_view aSummary, const std::span<const review_submission_comment> aComments, const bool aSubmitting) noexcept
		-> review_submission_rejection
	{
		if (aComments.size() > maximum_review_submission_comments) {
			return rejection::too_many_comments;
		}
		auto content = aSummary.size();
		for (auto index = std::size_t{ 0U }; index < aComments.size(); ++index) {
			const auto& comment = aComments[index];
			const auto  side    = comment.Side == diff_side::left || comment.Side == diff_side::right;
			if (comment.Draft.value() == 0U || comment.DraftRevision == 0U || comment.Path.empty() || comment.Path.size() > maximum_pending_mark_path_bytes
				|| comment.Path.find('\0') != std::string::npos || !valid_utf8(comment.Path) || !side
				|| (comment.StartLine && *comment.StartLine >= comment.Line)
				|| comment.Body.size() > maximum_review_submission_body_bytes || !valid_utf8(comment.Body)) {
				return rejection::invalid_comment;
			}
			for (auto earlier = std::size_t{ 0U }; earlier < index; ++earlier) {
				if (aComments[earlier].Draft == comment.Draft) {
					return rejection::invalid_comment;
				}
			}
			content += comment.Body.size() + comment.Path.size();
		}
		// A summary alone keeps its own 64 KiB bound.
		if (!aComments.empty() && content > maximum_review_submission_content_bytes) {
			return rejection::review_too_large;
		}
		if (aSubmitting) {
			for (const auto& comment : aComments) {
				if (validate_body(comment.Body, true) != rejection::none) {
					return rejection::blank_comment;
				}
			}
		}
		return rejection::none;
	}

	auto kept(const review_submission_draft& aDraft) noexcept -> bool
	{
		switch (aDraft.State) {
		case review_submission_state::drafting:
			return !aDraft.Body.empty() || aDraft.Disposition != review_disposition::comment;
		case review_submission_state::submitting:
			return true;
		case review_submission_state::submitted:
		case review_submission_state::discarded:
			return false;
		}
		return false;
	}

	auto controls(const review_submission_draft& aDraft) noexcept -> review_submission_controls
	{
		if (!valid(aDraft)) {
			return { };
		}
		const auto editing = aDraft.State == state::drafting;
		return {
			.Edit      = editing,
			.Submit    = editing && command_rejection(aDraft, { .Id = aDraft.Id, .Kind = kind::begin_submit }) == rejection::none,
			.Discard   = editing,
			.Reconcile = aDraft.State == state::submitting,
			.NewReview = aDraft.State == state::submitted || aDraft.State == state::discarded
		};
	}

	auto valid(const review_disposition aDisposition) noexcept -> bool
	{
		switch (aDisposition) {
		case review_disposition::comment:
		case review_disposition::approve:
		case review_disposition::request_changes:
			return true;
		}
		return false;
	}

	auto valid(const review_mutation_target& aTarget) noexcept -> bool
	{
		return aTarget.Host.value() != 0U && aTarget.Repository.value() != 0U
			&& aTarget.PullRequest.value() != 0U && aTarget.ExpectedHead.valid();
	}

	auto valid(const review_submission_draft& aDraft) noexcept -> bool
	{
		return validate_draft(aDraft) == rejection::none;
	}

	auto valid(const review_submission_proposal& aProposal) noexcept -> bool
	{
		return aProposal.Draft.value() != 0U && aProposal.Attempt.value() != 0U
			&& valid(aProposal.Target) && aProposal.ActingAccount.value() != 0U
			&& valid(aProposal.Disposition)
			&& validate_review(aProposal.Body, aProposal.Disposition, aProposal.Comments, true) == rejection::none;
	}

	auto apply(const review_submission_draft&   aDraft,
			   const review_submission_command& aCommand) -> review_submission_outcome
	{
		const auto reason = command_rejection(aDraft, aCommand);
		if (reason != rejection::none) {
			return { .Rejection = reason };
		}
		auto outcome = review_submission_outcome{ .Draft = aDraft };
		switch (aCommand.Kind) {
		case kind::edit:
			outcome.Draft->Body = aCommand.Body;
			break;
		case kind::choose_disposition:
			outcome.Draft->Disposition = aCommand.Disposition;
			break;
		case kind::discard:
			outcome.Draft->State = state::discarded;
			break;
		case kind::begin_submit:
			outcome.Draft->Attempt = review_submission_attempt_id{ aDraft.Attempt.value() + 1U };
			outcome.Draft->State   = state::submitting;
			break;
		case kind::complete_submit:
			outcome.Draft->State = state::submitted;
			break;
		case kind::fail_submit:
			outcome.Draft->State = state::drafting;
			break;
		case kind::set_comments:
			outcome.Draft->Comments = aCommand.Comments;
			break;
		}
		outcome.Changed = *outcome.Draft != aDraft;
		return outcome;
	}

	auto propose(const review_submission_draft& aDraft) -> std::optional<review_submission_proposal>
	{
		if (aDraft.State != state::submitting || !valid(aDraft)) {
			return std::nullopt;
		}
		return review_submission_proposal{
			.Draft = aDraft.Id, .Attempt = aDraft.Attempt, .Target = aDraft.Target, .ActingAccount = aDraft.ActingAccount, .Body = aDraft.Body, .Disposition = aDraft.Disposition, .Comments = aDraft.Comments
		};
	}
} // namespace bpr::core
