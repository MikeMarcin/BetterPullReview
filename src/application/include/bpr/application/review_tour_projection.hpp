#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_TOUR_PROJECTION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_TOUR_PROJECTION_HPP
#include "bpr/core/review_tour.hpp"
#include "bpr/core/review_tour_destination.hpp"
#include "bpr/core/review_tour_repair.hpp"
#include <memory>
#include <optional>
#include <string>
#include <utility>
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
	/// [[BPR-1992]]: `stopped` is a publication left part-sent, which the
	/// head and the panel say in the error role; `attention` is one that sent
	/// nothing and did not finish.
	enum class tour_publication_phase : std::uint8_t { idle,
													   publishing,
													   published,
													   attention,
													   stopped };
	/// \brief [[BPR-1992]]. RFC-BPR-0099, steps that advance: each write's state.
	enum class tour_step_state : std::uint8_t { waiting,
												sending,
												sent,
												failed,
												not_sent };
	struct tour_publication_step_view
	{
		std::string     Label;
		tour_step_state State = tour_step_state::waiting;
		/// The provider's short reason, on the step that failed.
		std::string Reason;
		auto        operator==(const tour_publication_step_view&) const -> bool = default;
	};
	/// \brief [[BPR-1992]]. What a failure banner offers; the first is primary.
	enum class tour_publication_remedy : std::uint8_t { resume,
														check_status,
														preview_again,
														move_stops,
														other_connection };
	/// \brief [[BPR-1992]]. RFC-BPR-0099, failures that read as failures: what
	/// stopped and where, why, what is on GitHub and what is not, and the
	/// actions that move forward.
	struct tour_publication_banner
	{
		std::string                          Title, Why, OnGitHub;
		std::vector<tour_publication_remedy> Actions;
		/// The first stop Move… opens, for a refusal that names stops.
		core::tour_stop_id MoveTo;
		auto               operator==(const tour_publication_banner&) const -> bool = default;
	};
	struct tour_publication_presentation
	{
		std::string                                Status = "Saved locally. Prepare a publication preview when the tour is ready.";
		std::string                                Markdown;
		std::vector<core::tour_destination_repair> Repairs;
		tour_publication_phase                     Phase = tour_publication_phase::idle;
		/// [[BPR-1992]]. The captured plan's steps and where each stands, and
		/// the one-based step under way or stopped at, of `Total`.
		std::vector<tour_publication_step_view> Steps;
		std::size_t                             Step = 0U, Total = 0U;
		/// [[BPR-1992]]. The form the plan chose, in one sentence with the
		/// stops that forced it; and, when that is not one review, the stops
		/// to move to keep it to one.
		std::string                                             Form;
		std::vector<std::pair<core::tour_stop_id, std::string>> KeepOneReview;
		std::optional<tour_publication_banner>                  Banner;
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
		/// [[BPR-2014]]. The pull request's changed paths, as the window that
		/// shows the tour knows them now; empty while it does not. Accept
		/// resolves a stop's references against them.
		std::vector<std::string> ChangedFiles;
	};
}
#endif
