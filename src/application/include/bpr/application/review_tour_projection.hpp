#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_TOUR_PROJECTION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_TOUR_PROJECTION_HPP
#include "bpr/core/review_tour.hpp"
#include "bpr/core/review_tour_destination.hpp"
#include <memory>
#include <string>
#include <vector>
namespace bpr::application {
	enum class tour_store_failure : std::uint8_t { unavailable,
												   cancelled,
												   corrupt,
												   schema_mismatch,
												   conflict,
												   bounds,
												   wrong_profile };
	enum class tour_presentation_origin : std::uint8_t { unknown,
														 desktop,
														 terminal,
														 command_client };
	enum class tour_section : std::uint8_t { problem,
											 solution,
											 stops };
	/// \brief Optional resume position; never a mutation target or author visit.
	struct tour_resume_hint
	{
		tour_section       Section = tour_section::problem;
		core::tour_stop_id Stop;
		auto               operator==(const tour_resume_hint&) const -> bool = default;
	};
	/// \brief Inert private-draft and local-save projection; no database or remote capability.
	struct tour_stop_destination
	{
		core::tour_stop_id          Stop;
		core::tour_destination_plan Plan;
		std::string                 SourceLink;
	};
	/// \brief [[BPR-1782]]. Where the tour's own publication stands, so Publish
	/// can read as working from the press until GitHub answers.
	enum class tour_publication_phase : std::uint8_t { idle,
													   publishing,
													   published,
													   attention };
	struct tour_publication_presentation
	{
		std::string            Status = "Saved locally. Prepare a publication preview when the tour is ready.";
		std::string            Markdown;
		tour_publication_phase Phase = tour_publication_phase::idle;
		/// While publishing: what the captured plan sends, in order.
		std::vector<std::string> Stages;
		/// Once published: where GitHub shows the tour.
		std::string Link;
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
		/// [[BPR-1781]]. The first published tour GitHub offers for import, so
		/// Overview can show a reader its brief and stop titles before Start tour.
		std::shared_ptr<const core::review_tour> Offered;
		std::vector<tour_stop_destination>       Destinations;
		std::string                              ImportNotice;
		bool                                     Loading = false;
		bool                                     Saving  = false;
		bool                                     CanUndo = false;
		bool                                     CanRedo = false;
		std::string                              UndoLabel, RedoLabel;
		tour_resume_hint                         Resume{ };
	};
}
#endif
