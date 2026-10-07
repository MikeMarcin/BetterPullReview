#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVISIONED_REVIEW_DRAFT_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVISIONED_REVIEW_DRAFT_HPP
#include "bpr/core/private_review.hpp"
#include "bpr/core/review_round_trip.hpp"
#include <cstddef>
#include <optional>
#include <string_view>
namespace bpr::core {
	using review_draft_scope_id                                   = strong_id<struct review_draft_scope_id_tag>;
	inline constexpr std::size_t maximum_shared_draft_body_bytes  = 64U * 1024U;
	inline constexpr std::size_t maximum_shared_draft_reply_bytes = 1024U;
	struct revisioned_review_draft
	{
		review_comment_draft                   Draft;
		std::uint64_t                          Revision                                                 = 1;
		review_draft_scope_id                  Scope                                                    = { };
		std::optional<review_composition_link> Composition                                              = { };
		auto                                   operator==(const revisioned_review_draft&) const -> bool = default;
	};
	struct revisioned_review_draft_command
	{
		review_draft_command       Command;
		std::uint64_t              ExpectedRevision  = 0;
		bool                       ChangeReplyTarget = false;
		std::optional<std::string> ReplyToThread     = { };
		review_draft_scope_id      Scope             = { };
		/// New immutable candidate correlation, required when editing linked prose.
		std::optional<review_composition_link> Composition = { };
		/// [[BPR-1212]]. With an edit, sets the draft's first line to
		/// `StartLine` (none makes it one line) in the same revision as the
		/// body. The anchor, its last line, never moves; a range GitHub would
		/// refuse is a policy refusal (`review_draft_rejection::invalid_range`).
		bool                         ChangeRange = false;
		std::optional<std::uint32_t> StartLine   = { };
	};
	enum class revisioned_draft_refusal : std::uint8_t { none,
														 conflict,
														 invalid_identity,
														 invalid_command,
														 bounds,
														 exhausted,
														 policy };
	struct revisioned_draft_outcome
	{
		revisioned_review_draft  Accepted;
		revisioned_draft_refusal Refusal = revisioned_draft_refusal::none;
		review_draft_rejection   Policy  = review_draft_rejection::none;
		bool                     Changed = false;
	};
	/// \brief Copies one private item's text into the exact editable target.
	/// No source metadata or publication command can be produced by this seam.
	[[nodiscard]] auto promote_private_text(std::string_view                              aText,
											const std::optional<revisioned_review_draft>& aTarget)
		-> std::optional<revisioned_review_draft_command>;
	[[nodiscard]] auto apply(const revisioned_review_draft& aDraft, const revisioned_review_draft_command& aCommand) -> revisioned_draft_outcome;
}
#endif
