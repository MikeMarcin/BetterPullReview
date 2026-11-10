#ifndef BPR_APP_TOUR_PUBLICATION_PLAN_HPP
#define BPR_APP_TOUR_PUBLICATION_PLAN_HPP
#include "bpr/application/staged_review_publication.hpp"
#include "bpr/core/review_tour_repair.hpp"
namespace bpr::app {
	struct tour_publication_options
	{
		bool Compatibility = false, Description = false, Mixed = false;
	};
	/// \brief One shared desktop/terminal plan; qualification is supplied by composition.
	struct tour_plan_preparation
	{
		std::optional<application::staged_review_plan> Plan;
		std::string                                    Status;
	};
	[[nodiscard]] auto prepare_tour_plan(const core::review_tour&, const application::review_submission_live_binding&, std::string_view, tour_publication_options, bool, std::span<const core::tour_anchor_file> = { }, std::span<const core::tour_destination_observation> = { }) -> tour_plan_preparation;
	[[nodiscard]] auto tour_operation_observation(const core::review_tour&, const core::tour_stop&, bool, bool, std::span<const core::tour_destination_observation>) -> core::tour_destination_observation;
	/// \brief Safe stage failure text that preserves known receipts and unknown outcomes.
	[[nodiscard]] auto tour_publication_failure_text(const application::staged_publication_result&) -> std::string;
	[[nodiscard]] auto tour_plan_markdown(const application::staged_review_plan&) -> std::string;
}
#endif
