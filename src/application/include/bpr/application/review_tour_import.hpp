#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_TOUR_IMPORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_TOUR_IMPORT_HPP
#include "bpr/application/github_read_model.hpp"
#include "bpr/core/review_tour_import.hpp"
#include <functional>
namespace bpr::application {
	/// \brief Whether all required published collections and raw bodies are observable.
	[[nodiscard]] auto review_tour_observation_complete(const github_pull_request_snapshot&) -> bool;
	/// \brief Adapt one fully accepted paged snapshot; never fetch destinations found in Markdown.
	[[nodiscard]] auto observe_review_tours(const core::review_tour& aSeed, const github_pull_request_snapshot& aSnapshot,
											std::string_view aHostname, const std::function<std::optional<core::tour_code_destination>(std::string_view)>& aPinnedSource = { }) -> std::vector<core::tour_import_candidate>;
}
#endif
