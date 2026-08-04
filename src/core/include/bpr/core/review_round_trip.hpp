#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_ROUND_TRIP_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_ROUND_TRIP_HPP

#include "bpr/core/diff_model.hpp"
#include "bpr/core/model.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace bpr::core {
	using review_draft_id = strong_id<struct review_draft_id_tag>;

	/// \brief Names the disposition a published review carries.
	///
	/// \details These are GitHub's three ordinary dispositions and nothing
	/// else. BetterPullReview adds no fourth: a review this tool publishes must
	/// be indistinguishable from one written in the web UI.
	enum class review_disposition : std::uint8_t {
		comment,
		approve,
		request_changes,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(review_disposition aDisposition) noexcept
		-> std::string_view;

	/// \brief Names where one draft sits between composition and publication.
	///
	/// \details `publishing` exists because publication is not instantaneous
	/// and the interval is observable. Without it a draft would be either
	/// editable or published, and a second publish issued while the first was
	/// in flight would look legal.
	enum class review_draft_state : std::uint8_t {
		drafting,
		publishing,
		published,
		discarded,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(review_draft_state aState) noexcept
		-> std::string_view;

	/// \brief Holds one comment a reviewer is composing.
	///
	/// \details The anchor is RFC-BPR-0004's semantic source-line identity, not
	/// a visual row. A draft that remembered where a line appeared on screen
	/// would attach itself to the wrong text as soon as anything above it
	/// changed.
	///
	/// A draft is private until it is published. There is no state in which it
	/// is partly shared: `Shared` is a consequence of reaching `published`, and
	/// is not settable on its own.
	struct review_comment_draft
	{
		review_draft_id    Id;
		source_line_ref    Anchor;
		std::string        Body;
		review_draft_state State = review_draft_state::drafting;

		[[nodiscard]] auto shared() const noexcept -> bool
		{
			return State == review_draft_state::published;
		}

		auto operator<=>(const review_comment_draft&) const = default;
	};

	/// \brief Names one command a reviewer can issue against a draft.
	enum class review_draft_command_kind : std::uint8_t {
		edit,
		discard,
		begin_publish,
		complete_publish,
		fail_publish,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(review_draft_command_kind aKind) noexcept
		-> std::string_view;

	/// \brief Carries one draft command.
	struct review_draft_command
	{
		review_draft_id           Id;
		review_draft_command_kind Kind = review_draft_command_kind::edit;
		std::string               Body;

		auto operator<=>(const review_draft_command&) const = default;
	};

	/// \brief Names why a draft command was refused.
	///
	/// \details Each value is a different mistake with a different repair. A
	/// single "illegal" would tell a caller nothing it could act on.
	enum class review_draft_rejection : std::uint8_t {
		none,
		unknown_draft,
		empty_body,
		already_publishing,
		already_published,
		discarded_draft,
		not_publishing,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(review_draft_rejection aRejection) noexcept
		-> std::string_view;

	/// \brief Reports what one draft command did.
	struct review_draft_outcome
	{
		review_comment_draft   Draft;
		review_draft_rejection Rejection = review_draft_rejection::none;
		bool                   Changed   = false;

		auto operator<=>(const review_draft_outcome&) const = default;
	};

	/// \brief Applies one command to one draft.
	///
	/// \details This is the whole of the draft policy. It performs no request,
	/// reaches no provider, and knows nothing about HTTP. Publication is
	/// proposed here and executed elsewhere.
	[[nodiscard]] auto apply(
		const review_comment_draft& aDraft,
		const review_draft_command& aCommand) -> review_draft_outcome;

	/// \brief Describes the semantic proposal a publish produces.
	///
	/// \details The core emits this rather than a request. It names what is to
	/// be said and where, and carries no host, no endpoint, and no credential —
	/// those are the host's, and keeping them out of the core is what stops a
	/// policy decision from quietly becoming a network call.
	struct review_publication_proposal
	{
		review_draft_id    Draft;
		source_line_ref    Anchor;
		std::string        Body;
		review_disposition Disposition = review_disposition::comment;

		auto operator<=>(const review_publication_proposal&) const = default;
	};

	/// \brief Builds the proposal for a draft that has begun publishing.
	///
	/// \details Returns a proposal only in the `publishing` state. A draft
	/// still being composed has not been submitted, and one already published
	/// must not be proposed twice.
	[[nodiscard]] auto propose_publication(
		const review_comment_draft& aDraft,
		review_disposition          aDisposition)
		-> std::optional<review_publication_proposal>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_ROUND_TRIP_HPP
