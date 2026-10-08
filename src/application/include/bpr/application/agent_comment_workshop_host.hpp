#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_COMMENT_WORKSHOP_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_COMMENT_WORKSHOP_HOST_HPP

#include "bpr/core/agent_chat_scope.hpp"
#include "bpr/core/feedback_workshop_candidate.hpp"
#include "bpr/core/review_round_trip.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace bpr::application {
	/// \brief One proposed revision of a comment the reviewer already wrote.
	///
	/// \details Two strings rather than one diff, for the reason
	/// `qt::comment_workshop_proposal` keeps them apart on screen: `Original`
	/// is the reviewer's own text as it stood when the proposal was made, and
	/// `Proposed` is what the agent offered instead. A single revised string
	/// would leave nothing to compare a rejection against.
	///
	/// `Draft` is the draft the proposal was made against, and it travels with
	/// the two texts so an acceptance names that draft rather than whichever
	/// one is on screen when the reviewer gets to it.
	///
	/// `Operation` is which of the six comment-workshop operations produced
	/// the proposal, and is absent when none was asked for — a finding
	/// promoted to a comment is a proposal nobody workshopped, and naming an
	/// operation for it would claim an operation ran. Engaged, it is what lets
	/// the before/after say *why* the after differs, rather than leaving the
	/// reviewer to infer which of six things was asked for.
	struct agent_comment_workshop_proposal
	{
		core::review_draft_id                   Draft;
		std::string                             Original;
		std::string                             Proposed;
		std::optional<core::workshop_operation> Operation;
		/// [[BPR-1091]]. What `Proposed` changes against `Original` besides
		/// wording: the label and decorations the shared recognizer reads on
		/// each, whether it still asks a question, and the suggested-change
		/// bytes. Computed from the two texts when the proposal is made, so
		/// it describes exactly the bytes an acceptance would write.
		core::workshop_candidate_change Change;

		auto operator<=>(const agent_comment_workshop_proposal&) const = default;
	};

	/// \brief Why a proposed revision was not offered to the reviewer.
	///
	/// \details Five separable mistakes with five different repairs. "Nothing
	/// appeared" is the one answer a workshop must never give, so every path
	/// that produces no proposal produces one of these instead.
	enum class agent_comment_workshop_refusal : std::uint8_t {
		/// Not a refusal. Exactly one proposal was offered.
		none,
		/// The command names another draft. A revision applied to the wrong
		/// comment is worse than one applied to none, so it is not applied to
		/// the draft that happens to be open.
		different_draft,
		/// The command is not an `edit`. A workshop revises text; it does not
		/// discard, publish, or settle a draft, and there is no branch here
		/// that could.
		not_an_edit,
		/// The proposed text is empty, and an empty revision is a deletion the
		/// agent did not say it wanted.
		empty_revision,
		/// The proposal is byte-identical to the original. Offering it would
		/// ask the reviewer to accept a change that is not one.
		unchanged,
	};

	/// \brief The number of declared workshop refusals.
	inline constexpr std::size_t agent_comment_workshop_refusal_count = 5U;

	static_assert(
		static_cast<std::size_t>(agent_comment_workshop_refusal::unchanged) + 1U
			== agent_comment_workshop_refusal_count,
		"agent_comment_workshop_refusal_count must cover every declared reason");

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(agent_comment_workshop_refusal aRefusal) noexcept
		-> std::string_view;

	/// \brief Records directly observable comment-workshop activity.
	struct agent_comment_workshop_statistics
	{
		std::uint64_t Proposals = 0U;
		std::uint64_t Refusals  = 0U;

		auto operator<=>(const agent_comment_workshop_statistics&) const
			= default;
	};

	/// \brief Where a proposed revision is shown, and where a refusal is said.
	class agent_comment_workshop_projection_port
	{
	public:
		agent_comment_workshop_projection_port() = default;
		agent_comment_workshop_projection_port(
			const agent_comment_workshop_projection_port&)
			= delete;
		auto operator=(const agent_comment_workshop_projection_port&)
			-> agent_comment_workshop_projection_port& = delete;
		agent_comment_workshop_projection_port(
			agent_comment_workshop_projection_port&&)
			= delete;
		auto operator=(agent_comment_workshop_projection_port&&)
			-> agent_comment_workshop_projection_port&    = delete;
		virtual ~agent_comment_workshop_projection_port() = default;

		virtual void present(const agent_comment_workshop_proposal& aProposal)
			= 0;
		virtual void refuse(agent_comment_workshop_refusal aRefusal) = 0;
	};

	/// \brief Turns one proposed draft edit into a reviewable before and after.
	///
	/// \details One port and no store. The host holds the reviewer's original
	/// nowhere: it is read from the draft it is handed, at the moment the
	/// proposal is made, so a proposal can never be compared against a body the
	/// reviewer has since changed.
	///
	/// **Nothing here applies anything.** The proposal is presented and the
	/// host returns; the edit reaches the draft only if the reviewer accepts it
	/// on the surface, which is `qt::review_draft_editor`'s already-proven
	/// behaviour. There is no publication path, no mutation port, and no branch
	/// that writes.
	class agent_comment_workshop_host final
	{
	public:
		explicit agent_comment_workshop_host(
			agent_comment_workshop_projection_port& aProjection);

		agent_comment_workshop_host(const agent_comment_workshop_host&) = delete;
		auto operator=(const agent_comment_workshop_host&)
			-> agent_comment_workshop_host&                        = delete;
		agent_comment_workshop_host(agent_comment_workshop_host&&) = delete;
		auto operator=(agent_comment_workshop_host&&)
			-> agent_comment_workshop_host& = delete;
		~agent_comment_workshop_host()      = default;

		/// \brief Offers one proposed revision of one exact draft.
		///
		/// \details `aOperation` names which of the six comment-workshop
		/// operations asked for this revision, and is carried into the
		/// proposal unchanged. It is a parameter rather than a field on the
		/// command because a `core::review_draft_command` is every draft
		/// gesture in the tree — publish, discard, edit — and an operation
		/// there would be a workshop concept riding on gestures that have
		/// nothing to do with one. It defaults to absent so that the paths
		/// that revise a comment without being asked for one of the six — the
		/// promoted finding is the shipped one — keep proposing exactly what
		/// they proposed before.
		///
		/// Nothing about the decision depends on the operation: a refusal is
		/// refused for the same four reasons whichever operation asked, and an
		/// undeclared operation cannot make an otherwise good revision
		/// refusable.
		///
		/// \returns `none` when a proposal was presented, and otherwise the
		/// exact reason none was, which the projection is told either way.
		[[nodiscard]] auto propose(
			const core::review_comment_draft&       aDraft,
			const core::review_draft_command&       aCommand,
			std::optional<core::workshop_operation> aOperation = std::nullopt)
			-> agent_comment_workshop_refusal;

		[[nodiscard]] auto statistics() const noexcept
			-> agent_comment_workshop_statistics;

	private:
		agent_comment_workshop_projection_port& Projection;
		agent_comment_workshop_statistics       Statistics;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_COMMENT_WORKSHOP_HOST_HPP
