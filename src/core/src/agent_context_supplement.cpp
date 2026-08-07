#include "bpr/core/agent_context_supplement.hpp"

#include <algorithm>
#include <ranges>

namespace bpr::core {
	auto build_agent_supplement(const agent_supplement_request& aRequest)
		-> std::optional<agent_context_supplement>
	{
		if (
			aRequest.SelectedNotes.size() > maximum_selected_notes
			|| aRequest.ChangedSinceReview.size() > maximum_supplement_paths) {
			return std::nullopt;
		}
		const auto oversize = std::ranges::any_of(
			aRequest.SelectedNotes, [](const supplement_note& aNote) {
				return aNote.Text.size() > maximum_note_bytes;
			});
		if (oversize) {
			return std::nullopt;
		}

		// Field for field from the request, and from nothing else. There is no
		// note store to read here and no default that produces one: a request
		// that selected no note yields a supplement with no note because there
		// was no source, rather than because a filter removed them.
		return agent_context_supplement{
			.SchemaVersion      = agent_supplement_schema_version,
			.Binding            = aRequest.Binding,
			.LastReviewedHead   = aRequest.LastReviewedHead,
			.ChangedSinceReview = aRequest.ChangedSinceReview,
			.Defects            = aRequest.Defects,
			.SelectedFile       = aRequest.SelectedFile,
			.Notes              = aRequest.SelectedNotes,
		};
	}
} // namespace bpr::core
