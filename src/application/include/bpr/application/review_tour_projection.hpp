#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_TOUR_PROJECTION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_TOUR_PROJECTION_HPP
#include "bpr/core/review_tour.hpp"
#include "bpr/core/review_tour_destination.hpp"
#include <memory>
namespace bpr::application {
	enum class tour_store_failure : std::uint8_t { unavailable,
												   cancelled,
												   corrupt,
												   schema_mismatch,
												   conflict,
												   bounds,
												   wrong_profile };
	/// \brief Inert private-draft and local-save projection; no database or remote capability.
	struct tour_stop_destination
	{
		core::tour_stop_id          Stop;
		core::tour_destination_plan Plan;
		std::string                 SourceLink;
	};
	struct tour_publication_presentation
	{
		std::string Status = "Saved locally. Prepare a publication preview when the tour is ready.";
		std::string Markdown;
		bool        Publish = false, Resume = false, Reconcile = false, Stop = false;
	};
	struct tour_presentation
	{
		std::shared_ptr<const core::review_tour> Draft;
		std::shared_ptr<const core::review_tour> Conflict;
		std::optional<tour_store_failure>        Failure;
		core::tour_refusal                       Refusal = core::tour_refusal::none;
		core::tour_stop_id                       Selected;
		std::uint64_t                            SelectionRevision = 0;
		std::uint64_t                            SavedRevision     = 0;
		std::vector<std::string>                 ImportChoices;
		std::vector<tour_stop_destination>       Destinations;
		std::string                              ImportNotice;
		bool                                     Loading = false;
		bool                                     Saving  = false;
		bool                                     CanUndo = false;
		bool                                     CanRedo = false;
		std::string                              UndoLabel, RedoLabel;
	};
}
#endif
