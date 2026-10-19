#ifndef BPR_APPLICATION_REVIEW_TOUR_SOURCE_HPP
#define BPR_APPLICATION_REVIEW_TOUR_SOURCE_HPP
#include "bpr/application/diff_runtime_ports.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/review_tour.hpp"
#include <expected>
namespace bpr::application {
	class epoch_application_host;
	/// Decision-lane admission of one pinned commit, including a file absent on the other PR side.
	[[nodiscard]] auto review_tour_source_context(epoch_application_host&, const workbench_pull_request_authority&, const core::tour_code_destination&) -> std::optional<workbench_selected_diff_context>;
	struct review_tour_source_material
	{
		core::tour_source_range Source;
		std::string             Text;
	};
	enum class review_tour_source_failure { unavailable,
											range_unavailable,
											too_large,
											cancelled };
	/// Runs on a source worker with exact endpoint resolution and a sealed epoch source read.
	/// Never reads a working tree or substitutes text for an unproved pinned endpoint.
	[[nodiscard]] auto materialize_review_tour_source(workbench_selected_diff_port&, epoch_application_host&,
													  const workbench_selected_diff_request&, const core::tour_code_destination&, std::stop_token)
		-> std::expected<review_tour_source_material, review_tour_source_failure>;
}
#endif
