#include "bpr/core/agent_draft_all.hpp"

#include <algorithm>

namespace bpr::core {
	auto stable_name(const draft_all_skip aSkip) noexcept -> std::string_view
	{
		switch (aSkip) {
		case draft_all_skip::already_drafted: return "already_drafted";
		case draft_all_skip::not_in_diff:     return "not_in_diff";
		case draft_all_skip::dismissed:       return "dismissed";
		case draft_all_skip::busy:            return "busy";
		}
		return "unknown";
	}

	auto draft_all_suggestions(const std::span<const draft_all_candidate> aCards) noexcept -> std::size_t
	{
		return static_cast<std::size_t>(std::ranges::count_if(aCards, &draft_all_candidate::Suggested));
	}

	auto plan_draft_all(const std::span<const draft_all_candidate> aCards) -> draft_all_plan
	{
		auto plan = draft_all_plan{ };
		for (std::size_t index = 0U; index < aCards.size(); ++index) {
			const auto& card = aCards[index];
			if (!card.Suggested) {
				continue;
			}
			switch (card.State) {
			case draft_all_card_state::drafted:   ++plan.AlreadyDrafted; continue;
			case draft_all_card_state::dismissed: ++plan.Dismissed; continue;
			case draft_all_card_state::working:   ++plan.Busy; continue;
			case draft_all_card_state::open:
			case draft_all_card_state::noted:     break;
			}
			if (!card.Placeable) {
				++plan.NotInDiff;
				continue;
			}
			plan.Draft.push_back(index);
		}
		return plan;
	}

	auto draft_all_offer(const std::span<const draft_all_candidate> aCards) -> std::size_t
	{
		if (draft_all_suggestions(aCards) < minimum_draft_all_suggestions) {
			return 0U;
		}
		return plan_draft_all(aCards).Draft.size();
	}

	auto start_draft_all(const draft_all_plan& aPlan) noexcept -> draft_all_outcome
	{
		return { .AlreadyDrafted = aPlan.AlreadyDrafted, .Dismissed = aPlan.Dismissed, .Busy = aPlan.Busy, .NotInDiff = aPlan.NotInDiff };
	}

	auto settle_draft_all(draft_all_outcome aOutcome, const draft_all_result aResult) noexcept -> draft_all_outcome
	{
		switch (aResult) {
		case draft_all_result::drafted:     ++aOutcome.Drafted; break;
		case draft_all_result::not_in_diff: ++aOutcome.NotInDiff; break;
		case draft_all_result::refused:     ++aOutcome.Refused; break;
		}
		return aOutcome;
	}
}
