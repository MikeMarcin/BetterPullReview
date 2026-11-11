#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_DRAFT_ALL_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_DRAFT_ALL_HPP

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-2061]]. Draft all: every suggested comment one agent reply
/// carries, drafted at once as local drafts.
/// \details Owner, 2026-11-10: "Can we get a draft all suggested comments from
/// an agent response with several." Which of a reply's suggested comments the
/// action drafts, which it passes over and why, and what the reply then says,
/// decided here from the cards' own facts. Drafting each one is the host's,
/// through the card's own Add as draft comment path; nothing here reaches
/// GitHub, a store or a clock. Pure: equal inputs give equal plans.
namespace bpr::core {
	/// \brief Where one card of the reply stands.
	enum class draft_all_card_state : std::uint8_t {
		/// Not acted on, or its last action could not be done.
		open,
		/// An action on it is under way.
		working,
		/// It is a draft already.
		drafted,
		/// Saved as a private note; it may still be drafted.
		noted,
		/// The reviewer dismissed it.
		dismissed,
	};

	/// \brief One card, as Draft all reads it.
	struct draft_all_candidate
	{
		draft_all_card_state State = draft_all_card_state::open;
		/// The finding carries a suggested comment that is not blank.
		bool Suggested = false;
		/// Its file is in the pull request at the current head, and its
		/// lines are within that file as far as the host knows now.
		bool Placeable = false;

		auto operator==(const draft_all_candidate&) const -> bool = default;
	};

	/// \brief Why a suggested comment is not drafted.
	enum class draft_all_skip : std::uint8_t {
		/// It is a draft already.
		already_drafted,
		/// Its lines are not in the pull request at the current head.
		not_in_diff,
		/// The reviewer dismissed it.
		dismissed,
		/// An action on it is under way.
		busy,
	};

	[[nodiscard]] auto stable_name(draft_all_skip aSkip) noexcept -> std::string_view;

	/// \brief What one press of Draft all does, in the cards' order.
	struct draft_all_plan
	{
		/// The cards to draft, by position, first to last.
		std::vector<std::size_t> Draft;
		/// Suggested comments passed over, by reason.
		std::size_t AlreadyDrafted = 0U;
		std::size_t NotInDiff      = 0U;
		std::size_t Dismissed      = 0U;
		std::size_t Busy           = 0U;

		auto operator==(const draft_all_plan&) const -> bool = default;
	};

	/// \brief The fewest suggested comments a reply carries for it to offer
	/// Draft all: one is its card's own Add as draft comment.
	inline constexpr std::size_t minimum_draft_all_suggestions = 2U;

	/// \brief How many of the cards carry a suggested comment.
	[[nodiscard]] auto draft_all_suggestions(std::span<const draft_all_candidate> aCards) noexcept -> std::size_t;

	/// \brief Plans one press: each card with a suggested comment that is
	/// open (or saved as a note) and placeable is drafted; every other
	/// suggested comment is counted under its reason. A card with no
	/// suggested comment is neither drafted nor counted.
	[[nodiscard]] auto plan_draft_all(std::span<const draft_all_candidate> aCards) -> draft_all_plan;

	/// \brief Whether the reply offers Draft all, and the count its button
	/// shows: at least `minimum_draft_all_suggestions` suggested comments,
	/// and at least one the plan would draft. Zero when not offered.
	[[nodiscard]] auto draft_all_offer(std::span<const draft_all_candidate> aCards) -> std::size_t;

	/// \brief How one press ended, for the reply's sentence.
	struct draft_all_outcome
	{
		std::size_t Drafted = 0U;
		/// Passed over before drafting, as the plan counted them.
		std::size_t AlreadyDrafted = 0U;
		std::size_t Dismissed      = 0U;
		std::size_t Busy           = 0U;
		/// Not in the pull request at the current head: counted by the plan,
		/// and those the card's own path found gone while drafting.
		std::size_t NotInDiff = 0U;
		/// Refused while drafting for another reason (a draft open at that
		/// line, a file that did not open).
		std::size_t Refused = 0U;

		[[nodiscard]] constexpr auto skipped() const noexcept -> std::size_t { return AlreadyDrafted + Dismissed + Busy + NotInDiff + Refused; }
		auto                         operator==(const draft_all_outcome&) const -> bool = default;
	};

	/// \brief The outcome a plan starts from, before any card is drafted.
	[[nodiscard]] auto start_draft_all(const draft_all_plan& aPlan) noexcept -> draft_all_outcome;

	/// \brief How one planned card's drafting ended.
	enum class draft_all_result : std::uint8_t {
		drafted,
		/// Its lines turned out not to be in the pull request at the head.
		not_in_diff,
		refused,
	};

	/// \brief The outcome with one more planned card settled.
	[[nodiscard]] auto settle_draft_all(draft_all_outcome aOutcome, draft_all_result aResult) noexcept -> draft_all_outcome;
}

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_DRAFT_ALL_HPP
