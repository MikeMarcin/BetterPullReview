#include "bpr/core/review_round_trip.hpp"

#include <algorithm>
#include <cctype>

namespace bpr::core {
	namespace {
		/// \brief Reports whether a body carries anything a reader could read.
		///
		/// \details Whitespace is not a comment. Publishing a body of spaces
		/// would put an empty bubble on someone's pull request, which is worse
		/// than refusing it.
		[[nodiscard]] auto meaningful(const std::string_view aBody) noexcept -> bool
		{
			return std::ranges::any_of(aBody, [](const char aCharacter) {
				return std::isspace(static_cast<unsigned char>(aCharacter)) == 0;
			});
		}
	} // namespace

	auto name(const review_disposition aDisposition) noexcept -> std::string_view
	{
		switch (aDisposition) {
		case review_disposition::comment:
			return "comment";
		case review_disposition::approve:
			return "approve";
		case review_disposition::request_changes:
			return "request_changes";
		}
		return "comment";
	}

	auto name(const review_draft_state aState) noexcept -> std::string_view
	{
		switch (aState) {
		case review_draft_state::drafting:
			return "drafting";
		case review_draft_state::publishing:
			return "publishing";
		case review_draft_state::published:
			return "published";
		case review_draft_state::discarded:
			return "discarded";
		}
		return "drafting";
	}

	auto name(const review_draft_command_kind aKind) noexcept -> std::string_view
	{
		switch (aKind) {
		case review_draft_command_kind::edit:
			return "edit";
		case review_draft_command_kind::discard:
			return "discard";
		case review_draft_command_kind::begin_publish:
			return "begin_publish";
		case review_draft_command_kind::complete_publish:
			return "complete_publish";
		case review_draft_command_kind::fail_publish:
			return "fail_publish";
		}
		return "edit";
	}

	auto name(const review_draft_rejection aRejection) noexcept -> std::string_view
	{
		switch (aRejection) {
		case review_draft_rejection::none:
			return "none";
		case review_draft_rejection::unknown_draft:
			return "unknown_draft";
		case review_draft_rejection::empty_body:
			return "empty_body";
		case review_draft_rejection::already_publishing:
			return "already_publishing";
		case review_draft_rejection::already_published:
			return "already_published";
		case review_draft_rejection::discarded_draft:
			return "discarded_draft";
		case review_draft_rejection::not_publishing:
			return "not_publishing";
		case review_draft_rejection::invalid_range:
			return "invalid_range";
		}
		return "none";
	}

	auto apply(
		const review_comment_draft& aDraft,
		const review_draft_command& aCommand) -> review_draft_outcome
	{
		const auto refuse = [&aDraft](const review_draft_rejection aRejection) {
			return review_draft_outcome{
				.Draft     = aDraft,
				.Rejection = aRejection,
				.Changed   = false,
			};
		};

		if (aCommand.Id != aDraft.Id) {
			return refuse(review_draft_rejection::unknown_draft);
		}

		// A published comment is GitHub's, not a draft. Editing it is a
		// different operation against a different authority, and letting a
		// draft command touch it here would create a second, local truth.
		if (aDraft.State == review_draft_state::published
			&& aCommand.Kind != review_draft_command_kind::complete_publish) {
			return refuse(review_draft_rejection::already_published);
		}
		if (aDraft.State == review_draft_state::discarded) {
			return refuse(review_draft_rejection::discarded_draft);
		}

		auto next = aDraft;
		switch (aCommand.Kind) {
		case review_draft_command_kind::edit:
			if (aDraft.State == review_draft_state::publishing) {
				return refuse(review_draft_rejection::already_publishing);
			}
			next.Body = aCommand.Body;
			break;
		case review_draft_command_kind::discard:
			if (aDraft.State == review_draft_state::publishing) {
				return refuse(review_draft_rejection::already_publishing);
			}
			next.State = review_draft_state::discarded;
			break;
		case review_draft_command_kind::begin_publish:
			if (aDraft.State == review_draft_state::publishing) {
				return refuse(review_draft_rejection::already_publishing);
			}
			if (!meaningful(aDraft.Body)) {
				return refuse(review_draft_rejection::empty_body);
			}
			if (!valid_comment_range(aDraft)) {
				return refuse(review_draft_rejection::invalid_range);
			}
			next.State = review_draft_state::publishing;
			break;
		case review_draft_command_kind::complete_publish:
			if (aDraft.State != review_draft_state::publishing) {
				return refuse(review_draft_rejection::not_publishing);
			}
			next.State = review_draft_state::published;
			break;
		case review_draft_command_kind::fail_publish:
			if (aDraft.State != review_draft_state::publishing) {
				return refuse(review_draft_rejection::not_publishing);
			}
			// Back to drafting rather than discarded: a failed publication
			// leaves the reviewer holding text they wrote, and throwing it away
			// because the network misbehaved would be the tool losing their
			// work.
			next.State = review_draft_state::drafting;
			break;
		}

		return review_draft_outcome{
			.Draft     = next,
			.Rejection = review_draft_rejection::none,
			.Changed   = next != aDraft,
		};
	}

	auto name(const target_revalidation aOutcome) noexcept -> std::string_view
	{
		switch (aOutcome) {
		case target_revalidation::accepted:
			return "accepted";
		case target_revalidation::head_moved:
			return "head_moved";
		case target_revalidation::target_changed:
			return "target_changed";
		case target_revalidation::head_unknown:
			return "head_unknown";
		}
		return "target_changed";
	}

	auto revalidate(
		const review_mutation_target& aPrepared,
		const review_mutation_target& aObserved) noexcept -> target_revalidation
	{
		if (aPrepared.Host != aObserved.Host
			|| aPrepared.Repository != aObserved.Repository
			|| aPrepared.PullRequest != aObserved.PullRequest) {
			return target_revalidation::target_changed;
		}
		if (!aObserved.ExpectedHead.valid() || !aPrepared.ExpectedHead.valid()) {
			return target_revalidation::head_unknown;
		}
		if (aPrepared.ExpectedHead != aObserved.ExpectedHead) {
			return target_revalidation::head_moved;
		}
		return target_revalidation::accepted;
	}

	auto publication_permitted(
		const target_revalidation aOutcome,
		const bool                aAcknowledged) noexcept -> bool
	{
		switch (aOutcome) {
		case target_revalidation::accepted:
			return true;
		case target_revalidation::head_moved:
			return aAcknowledged;
		case target_revalidation::target_changed:
		case target_revalidation::head_unknown:
			break;
		}
		return false;
	}

	auto propose_publication(
		const review_comment_draft& aDraft,
		const review_disposition    aDisposition)
		-> std::optional<review_publication_proposal>
	{
		if (aDraft.State != review_draft_state::publishing) {
			return std::nullopt;
		}
		return review_publication_proposal{
			.Draft         = aDraft.Id,
			.Anchor        = aDraft.Anchor,
			.Body          = aDraft.Body,
			.Disposition   = aDisposition,
			.ReplyToThread = aDraft.ReplyToThread,
			.StartLine     = aDraft.StartLine,
		};
	}
} // namespace bpr::core
