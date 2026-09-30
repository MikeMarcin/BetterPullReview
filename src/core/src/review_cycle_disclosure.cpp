#include "bpr/core/review_cycle_disclosure.hpp"

#include <algorithm>
#include <optional>

namespace bpr::core {
	auto disclose_feedback(const cycle_feedback_thread& aThread, const std::span<const cycle_feedback_revision> aHistory, const cycle_disclosure_selection& aSelection)
		-> std::expected<cycle_disclosed_feedback, cycle_refusal>
	{
		if (!valid(aThread)) {
			return std::unexpected{ cycle_refusal::invalid_identity };
		}
		if (aThread.Lineage.Lifecycle == private_thread_lifecycle::archived) {
			return std::unexpected{ cycle_refusal::closed };
		}
		// The whole history, in order: revision n is the nth vertex of the thread.
		if (aHistory.size() != aThread.Lineage.LastOrder.value()) {
			return std::unexpected{ cycle_refusal::missing_source };
		}
		for (std::size_t index = 0; index < aHistory.size(); ++index) {
			const auto& revision = aHistory[index];
			if (!valid(revision) || revision.Identity.Item != aThread.Id || revision.Identity.Position.Revision.value() != index + 1U) {
				return std::unexpected{ cycle_refusal::missing_source };
			}
		}
		const auto named = [&](const private_review_turn_revision_id aId) { return std::ranges::find(aSelection.Revisions, aId) != aSelection.Revisions.end(); };
		if (!std::ranges::all_of(aSelection.Revisions, [&](const private_review_turn_revision_id aId) { return valid_private_id(aId) && aId.value() <= aHistory.size(); })
			|| !std::ranges::all_of(aSelection.PrivateReferences, [&](const std::uint32_t aIndex) {
				   return aIndex > 0U && aIndex <= aThread.Related.size() && aThread.Related[aIndex - 1U].Disclosure == cycle_reference_disclosure::private_only;
			   })) {
			return std::unexpected{ cycle_refusal::missing_source };
		}
		// The note's current wording: the latest revision of its first turn.
		auto wording = std::optional<private_review_turn_revision_id>{ };
		for (const auto& revision : aHistory) {
			if (revision.Identity.Position.Turn == private_review_turn_id{ 1 }) {
				wording = revision.Identity.Position.Revision;
			}
		}
		if (!wording) {
			return std::unexpected{ cycle_refusal::missing_source };
		}

		auto disclosed = cycle_disclosed_feedback{ .Item = aThread.Id, .Primary = aThread.Primary };
		for (std::size_t index = 0; index < aThread.Related.size(); ++index) {
			const auto& reference = aThread.Related[index];
			if (reference.Disclosure == cycle_reference_disclosure::included
				|| std::ranges::find(aSelection.PrivateReferences, static_cast<std::uint32_t>(index + 1U)) != aSelection.PrivateReferences.end()) {
				disclosed.Related.push_back(reference);
			}
		}
		for (const auto& revision : aHistory) {
			const auto id = revision.Identity.Position.Revision;
			if (id != *wording && !named(id)) {
				continue;
			}
			auto entry = cycle_disclosed_revision{ .Revision = id, .Provenance = revision.Author.Provenance, .Body = revision.Body };
			if (aSelection.AuthorDetails && (revision.Author.Adapter || revision.Author.Provider || revision.Author.Model || revision.Author.Effort)) {
				entry.Details = revision.Author;
			}
			disclosed.Revisions.push_back(std::move(entry));
		}
		return disclosed;
	}
}
