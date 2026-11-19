#ifndef BPR_APP_TOUR_PUBLICATION_PLAN_HPP
#define BPR_APP_TOUR_PUBLICATION_PLAN_HPP
#include "bpr/application/review_tour_projection.hpp"
#include "bpr/application/staged_review_publication.hpp"
#include "bpr/core/review_tour_repair.hpp"
namespace bpr::app {
	struct tour_publication_options
	{
		bool Compatibility = false, Mixed = false;
		/// [[BPR-1992]]. A stop outside the diff's lines may go as a file
		/// comment without the author asking per stop: the application's
		/// chosen form. The CLI keeps the explicit request.
		bool FileComments = false;
	};
	/// \brief One shared desktop/terminal plan; qualification is supplied by composition.
	struct tour_plan_preparation
	{
		std::optional<application::staged_review_plan> Plan;
		std::string                                    Status;
		/// [[BPR-1992]]. What the attempt found: the form's readiness, and
		/// each stop's destination and problem, in route order (empty when
		/// the stop has no source).
		core::tour_publication_readiness            Readiness = core::tour_publication_readiness::incomplete;
		std::vector<core::tour_destination_plan>    Decisions;
		std::vector<core::tour_destination_problem> Problems;
		/// [[BPR-1996]], RFC-BPR-0100. The references in stop texts that will
		/// not link on GitHub, said in one sentence; empty when there are none.
		std::string Unlinked;
	};
	/// \brief [[BPR-1992]]. RFC-BPR-0099, the plan chooses the form: the first
	/// of one review, a mixed tour and compatibility comments that carries
	/// every stop, said in one sentence with the stops that forced it.
	struct tour_plan_choice
	{
		std::optional<application::staged_review_plan>          Plan;
		std::string                                             Status;
		std::string                                             Form;
		std::vector<std::pair<core::tour_stop_id, std::string>> KeepOneReview;
		/// No form carries the tour on this connection: refused before any write.
		std::optional<application::tour_publication_banner> Refusal;
	};
	[[nodiscard]] auto choose_tour_plan(const core::review_tour&, const application::review_submission_live_binding&, bool aQualified, std::span<const core::tour_anchor_file> = { }, std::span<const core::tour_destination_observation> = { }) -> tour_plan_choice;
	[[nodiscard]] auto prepare_tour_plan(const core::review_tour&, const application::review_submission_live_binding&, tour_publication_options, bool, std::span<const core::tour_anchor_file> = { }, std::span<const core::tour_destination_observation> = { }) -> tour_plan_preparation;
	[[nodiscard]] auto tour_operation_observation(const core::review_tour&, const core::tour_stop&, bool, bool, std::span<const core::tour_destination_observation>) -> core::tour_destination_observation;
	/// \brief Safe stage failure text that preserves known receipts and unknown outcomes.
	[[nodiscard]] auto tour_publication_failure_text(const application::staged_publication_result&) -> std::string;
	[[nodiscard]] auto tour_plan_markdown(const application::staged_review_plan&) -> std::string;
	/// \brief [[BPR-1992]]. The failed step's short reason for its checklist
	/// row, "Refused by GitHub (422)"; and the clause the banner says it
	/// with, "was refused by GitHub (HTTP 422)".
	[[nodiscard]] auto tour_step_failure_reason(const application::staged_publication_result&) -> std::string;
	[[nodiscard]] auto tour_step_failure_clause(const application::staged_publication_result&) -> std::string;
}
#endif
