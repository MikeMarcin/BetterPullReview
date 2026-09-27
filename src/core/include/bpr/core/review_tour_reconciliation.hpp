#ifndef BPR_CORE_REVIEW_TOUR_RECONCILIATION_HPP
#define BPR_CORE_REVIEW_TOUR_RECONCILIATION_HPP
#include "bpr/core/review_tour.hpp"
namespace bpr::core {
	/// Exact equality or two disjoint contiguous byte edits; overlapping edits remain unresolved.
	[[nodiscard]] auto merge_tour_text(std::string_view, std::string_view, std::string_view) -> std::optional<std::string>;
	/// Reconcile only the same observed root/owner. Missing/partial observations never delete local work.
	[[nodiscard]] auto reconcile_tour(const review_tour&, const review_tour&) -> review_tour;
	/// Explicit choice applies one retained version against its observed remote baseline.
	[[nodiscard]] auto resolve_tour_conflict(const review_tour&, std::size_t, bool aUseRemote) -> std::optional<review_tour>;
	/// Retain exact published spans and their editor values for later three-way comparisons.
	void retain_tour_brief_baselines(review_tour&, std::string_view, std::string_view);
	/// Map a brief into the current full description, preserving all unrelated bytes.
	[[nodiscard]] auto propose_tour_description(const review_tour&, std::string_view) -> std::optional<std::string>;
}
#endif
