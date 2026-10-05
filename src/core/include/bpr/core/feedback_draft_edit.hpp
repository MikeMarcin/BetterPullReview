#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_FEEDBACK_DRAFT_EDIT_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_FEEDBACK_DRAFT_EDIT_HPP

#include "bpr/core/feedback_header_edit.hpp"
#include "bpr/core/review_tour.hpp"
#include "bpr/core/revisioned_review_draft.hpp"

#include <array>
#include <cstdint>
#include <optional>

/// \file
/// \brief Conventional Comments header edits bound to the existing revisioned
/// drafts (RFC-BPR-0048 R09 to R11, R42; RFC-BPR-0051 R41).
/// \details A plan turns one explicit header edit into the command the draft's
/// own owner already accepts: an ordinary `edit` of a comment draft, or an
/// `edit_stop` of a local tour stop. The command carries the whole new body
/// and the revision the author was shown, so the owner's compare-and-swap,
/// conflict candidate, durable store and undo stay exactly as they are. No
/// label, decoration or annotation is stored beside the body: the Markdown is
/// the only authority, and every reader recognizes it again.
namespace bpr::core {
	/// \brief What the author was shown when they chose the edit.
	struct feedback_draft_basis
	{
		/// The draft's revision, or the tour's for a tour stop.
		std::uint64_t Revision = 0;
		/// `BodyDigest` of the body's annotation.
		std::array<std::uint8_t, 32> Digest{ };
		std::uint32_t                ParserRevision                                        = feedback_parser_revision;
		auto                         operator==(const feedback_draft_basis&) const -> bool = default;
	};

	enum class feedback_draft_edit_refusal : std::uint8_t {
		none,
		/// The draft is not the one the author was shown: another revision,
		/// body, parser revision, identity or tour stop. Nothing is applied;
		/// plan again from the current draft.
		stale_draft,
		/// The draft is publishing, published or discarded, or the tour is
		/// read-only. Its bytes belong to that state; a prepared publication
		/// keeps the exact body it was prepared with.
		not_editable,
		/// The header decision refused; `Decision.Refusal` says why.
		header,
	};
	[[nodiscard]] auto stable_name(feedback_draft_edit_refusal aRefusal) noexcept -> std::string_view;

	/// \brief `decide_feedback_header_edit`, and a body that is still only a
	/// header can take, change or lose one.
	/// \details A comment may be classified before it is written. A blank
	/// ordinary body takes the header alone, as in `issue: `, and a body that
	/// is exactly such a header with nothing after it (an empty subject) can
	/// be relabelled, redecorated or made plain again. The result reads back
	/// as that header once a subject follows it. Anything else, and every
	/// managed envelope, is decided by `decide_feedback_header_edit`
	/// unchanged.
	[[nodiscard]] auto decide_feedback_composition_edit(const feedback_header_edit_request& aRequest) -> feedback_header_edit_decision;

	/// \brief The basis an author is shown for a comment draft.
	[[nodiscard]] auto feedback_draft_basis_of(const revisioned_review_draft& aDraft) -> feedback_draft_basis;

	/// \brief One header edit as a comment draft command.
	/// \details `Command` is set only when `Decision.Changed`. A request that
	/// changes nothing is `none` with no command, so the draft keeps its
	/// revision. On a refusal `Decision.Annotation` is the current body's.
	struct feedback_draft_edit_plan
	{
		feedback_draft_edit_refusal                    Refusal = feedback_draft_edit_refusal::none;
		feedback_header_edit_decision                  Decision;
		std::optional<revisioned_review_draft_command> Command = std::nullopt;
	};
	/// \brief Plan one header edit of the current comment draft.
	/// \details The command is an ordinary whole-body `edit` with the draft's
	/// scope and the shown revision, and no composition link: the durable
	/// owner records a linked draft's reviewer successor itself. Only a
	/// `drafting` draft is edited.
	[[nodiscard]] auto plan_feedback_draft_edit(const revisioned_review_draft& aCurrent, const feedback_draft_basis& aShown, const feedback_header_edit& aEdit) -> feedback_draft_edit_plan;

	/// \brief The basis an author is shown for one local tour stop's body, or
	/// none when the tour has no such stop.
	[[nodiscard]] auto feedback_tour_stop_basis(const review_tour& aTour, tour_stop_id aStop) -> std::optional<feedback_draft_basis>;

	/// \brief One header edit as a tour command.
	struct feedback_tour_stop_edit_plan
	{
		feedback_draft_edit_refusal   Refusal = feedback_draft_edit_refusal::none;
		feedback_header_edit_decision Decision;
		std::optional<tour_command>   Command = std::nullopt;
	};
	/// \brief Plan one header edit of a local tour stop's body.
	/// \details The body is the stop's raw narrative field, an ordinary body;
	/// the published envelope is generated later and is not edited here. The
	/// command is the `edit_stop` the desktop and the `bpr tour` owner already
	/// send, with only the body changed, so the tour owner's revision check,
	/// undo journal and request receipts apply unchanged.
	[[nodiscard]] auto plan_feedback_tour_stop_edit(const review_tour& aCurrent, tour_stop_id aStop, const feedback_draft_basis& aShown, const feedback_header_edit& aEdit) -> feedback_tour_stop_edit_plan;
}

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_FEEDBACK_DRAFT_EDIT_HPP
