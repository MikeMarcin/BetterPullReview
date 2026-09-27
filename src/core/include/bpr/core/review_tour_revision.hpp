#ifndef BPR_CORE_REVIEW_TOUR_REVISION_HPP
#define BPR_CORE_REVIEW_TOUR_REVISION_HPP
#include "bpr/core/diff_semantics.hpp"
#include "bpr/core/review_tour.hpp"
namespace bpr::core {
	/// Reuse an admitted epoch comparison; a suggested mapping never moves a remote subject.
	[[nodiscard]] auto map_tour_revision(const review_tour&, const diff_semantic_index&, const tour_code_destination& aLeft, const tour_code_destination& aRight) -> review_tour;
	/// Compare only locally retained raw website versions, not private draft edits or inferred history.
	[[nodiscard]] auto observed_tour_changes(const review_tour&, const review_tour&) -> std::vector<tour_observed_change>;
}
#endif
