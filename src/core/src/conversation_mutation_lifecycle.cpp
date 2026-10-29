#include "bpr/core/conversation_mutation_lifecycle.hpp"

#include <utility>

namespace bpr::core {

	auto adopt_conversation_mutations(conversation_mutation_state& aState, conversation_overlay& aOverlay,
									  conversation_mutation_scope aScope, const bool aBindingCurrent) -> conversation_mutation_retirement
	{
		if (aBindingCurrent && aState.Scope == aScope) {
			return conversation_mutation_retirement::drain;
		}
		retire_conversation_mutations(aState, aOverlay);
		aState.Scope = std::move(aScope);
		return conversation_mutation_retirement::cancel;
	}

	void retire_conversation_mutations(conversation_mutation_state& aState, conversation_overlay& aOverlay)
	{
		aState   = { };
		aOverlay = { };
	}

	auto retain_conversation_mutation(conversation_mutation_state& aState, const conversation_mutation_request aRequest,
									  pending_conversation_mutation aChange) -> bool
	{
		return aState.Scope && aRequest.Request.value() != 0U && aRequest.Generation.value() != 0U
			&& aRequest.Session == aState.Scope->Session && !aChange.Subject.empty()
			&& aState.Pending.emplace(aRequest, std::move(aChange)).second;
	}

	auto settle_conversation_mutation(conversation_mutation_state& aState, conversation_overlay& aOverlay,
									  const conversation_mutation_request aRequest, const pending_conversation_mutation& aChange, const bool aSucceeded,
									  std::optional<reaction_summary> aAnswered) -> bool
	{
		const auto found = aState.Pending.find(aRequest);
		if (found == aState.Pending.end() || found->second != aChange) {
			return false;
		}
		aState.Pending.erase(found);
		if (aChange.Kind == conversation_mutation_kind::resolution) {
			if (aSucceeded) {
				confirm_resolution_change(aOverlay, aChange.Subject);
			} else {
				abandon_resolution_change(aOverlay, aChange.Subject);
			}
		} else if (aSucceeded) {
			confirm_reaction_change(aOverlay, aChange.Subject, aChange.Content, std::move(aAnswered));
		} else {
			abandon_reaction_change(aOverlay, aChange.Subject, aChange.Content);
		}
		return true;
	}
}
