#include "bpr/core/draft_state.hpp"

#include "bpr/core/review_role.hpp"

namespace bpr::core {
	auto name(const draft_state_change aChange) noexcept -> std::string_view
	{
		switch (aChange) {
		case draft_state_change::ready_for_review: return "ready_for_review";
		case draft_state_change::convert_to_draft: return "convert_to_draft";
		}
		return { };
	}

	auto name(const draft_state_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case draft_state_refusal::none:          return "none";
		case draft_state_refusal::not_author:    return "not_author";
		case draft_state_refusal::not_open:      return "not_open";
		case draft_state_refusal::already:       return "already";
		case draft_state_refusal::not_permitted: return "not_permitted";
		}
		return { };
	}

	auto name(const draft_state_outcome aOutcome) noexcept -> std::string_view
	{
		switch (aOutcome) {
		case draft_state_outcome::write:         return "write";
		case draft_state_outcome::already:       return "already";
		case draft_state_outcome::not_open:      return "not_open";
		case draft_state_outcome::not_permitted: return "not_permitted";
		}
		return { };
	}

	namespace {
		[[nodiscard]] auto authored(const draft_state_facts& aFacts) noexcept -> bool
		{
			return !aFacts.Author.empty() && !aFacts.Viewer.empty() && same_login(aFacts.Author, aFacts.Viewer);
		}
	} // namespace

	auto draft_state_offered(const draft_state_facts& aFacts) noexcept -> std::optional<draft_state_change>
	{
		if (!authored(aFacts) || !aFacts.Open) {
			return std::nullopt;
		}
		return aFacts.Draft ? draft_state_change::ready_for_review : draft_state_change::convert_to_draft;
	}

	auto decide_draft_state_change(const draft_state_facts& aFacts, const draft_state_change aChange) noexcept -> draft_state_refusal
	{
		if (!authored(aFacts)) {
			return draft_state_refusal::not_author;
		}
		if (!aFacts.Open) {
			return draft_state_refusal::not_open;
		}
		if (aFacts.Draft == draft_after(aChange)) {
			return draft_state_refusal::already;
		}
		return draft_state_refusal::none;
	}

	auto decide_draft_state_write(const draft_state_observation& aObservation, const draft_state_change aChange) noexcept -> draft_state_outcome
	{
		if (aObservation.Draft == draft_after(aChange)) {
			return draft_state_outcome::already;
		}
		if (!aObservation.Open) {
			return draft_state_outcome::not_open;
		}
		if (!aObservation.ViewerCanUpdate) {
			return draft_state_outcome::not_permitted;
		}
		return draft_state_outcome::write;
	}

	void begin_draft_state_change(draft_state_overlay& aOverlay, const std::uint64_t aRequest, const draft_state_change aChange) noexcept
	{
		if (aRequest == 0U) {
			return;
		}
		aOverlay.Request      = aRequest;
		aOverlay.PendingDraft = draft_after(aChange);
	}

	void settle_draft_state_change(draft_state_overlay& aOverlay, const std::uint64_t aRequest, const bool aDraft) noexcept
	{
		if (!aOverlay.pending() || aOverlay.Request != aRequest) {
			return;
		}
		aOverlay.Request       = 0U;
		aOverlay.Answered      = aDraft;
		aOverlay.Disagreements = 0U;
	}

	void abandon_draft_state_change(draft_state_overlay& aOverlay, const std::uint64_t aRequest) noexcept
	{
		if (aOverlay.pending() && aOverlay.Request == aRequest) {
			aOverlay.Request = 0U;
		}
	}

	void observe_draft_state(draft_state_overlay& aOverlay, const bool aDraft) noexcept
	{
		if (!aOverlay.Answered.has_value()) {
			return;
		}
		if (aDraft == *aOverlay.Answered) {
			aOverlay.Answered.reset();
			aOverlay.Disagreements = 0U;
			return;
		}
		++aOverlay.Disagreements;
		if (aOverlay.Disagreements >= 2U) {
			aOverlay.Answered.reset();
			aOverlay.Disagreements = 0U;
		}
	}

	auto project_draft_state(const bool aReadDraft, const draft_state_overlay& aOverlay) noexcept -> projected_draft_state
	{
		if (aOverlay.pending()) {
			return { .Draft = aOverlay.PendingDraft, .Pending = true };
		}
		return { .Draft = aOverlay.Answered.value_or(aReadDraft) };
	}
} // namespace bpr::core
