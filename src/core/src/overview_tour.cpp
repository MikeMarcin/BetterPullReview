#include "bpr/core/overview_tour.hpp"

#include "bpr/core/review_tour_destination.hpp"
#include "bpr/core/tour_publication_head.hpp"

namespace bpr::core {
	namespace {
		[[nodiscard]] auto begun(const review_tour& aTour) -> bool
		{
			return !aTour.Stops.empty() || aTour.Remote.has_value();
		}

		[[nodiscard]] auto summary_of(const review_tour& aTour, const overview_tour_kind aKind) -> overview_tour
		{
			auto result      = overview_tour{ .Kind = aKind };
			result.Published = aTour.Remote.has_value();
			result.Stops.reserve(aTour.Stops.size());
			for (const auto& stop : aTour.Stops) {
				auto row = overview_tour_stop{ .Id = stop.Id, .Title = stop.Title, .Preparation = stop.Preparation, .Acceptance = decide_tour_stop_acceptance(aTour, stop).State, .Lead = tour_lead_sentence(stop.Body) };
				if (const auto source = publication_source(stop)) {
					row.Path      = source->Path;
					row.FirstLine = source->FirstLine + 1U;
					row.LastLine  = source->LastLine + 1U;
				}
				result.Stops.push_back(std::move(row));
			}
			return result;
		}
	}

	auto decide_overview_tour(const review_tour* const aDraft, const review_tour* const aOffered, const bool aAuthor) -> overview_tour
	{
		if (aDraft != nullptr && begun(*aDraft)) {
			if (!aDraft->ReadOnly) {
				return summary_of(*aDraft, overview_tour_kind::writer);
			}
			if (aDraft->Remote) {
				auto result     = summary_of(*aDraft, overview_tour_kind::reader);
				result.Imported = true;
				return result;
			}
		}
		if (aOffered != nullptr && aOffered->Remote && !aOffered->Stops.empty()) {
			// My own published tour, not yet in this window, is still mine to
			// continue: Prepare opens Review's Tour tab, which offers it.
			if (aOffered->Remote->Own) {
				return { .Kind = overview_tour_kind::prepare };
			}
			return summary_of(*aOffered, overview_tour_kind::reader);
		}
		return { .Kind = aAuthor ? overview_tour_kind::prepare : overview_tour_kind::hidden };
	}
} // namespace bpr::core
