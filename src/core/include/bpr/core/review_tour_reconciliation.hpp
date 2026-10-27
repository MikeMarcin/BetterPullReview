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
	/// [[BPR-1787]]. The tour comment's text ahead of its route. A brief that goes
	/// in the description leaves the comment one line pointing there; otherwise
	/// the comment carries Problem and Solution itself.
	inline constexpr std::string_view tour_brief_in_description_note = "The problem and solution are in the pull request description.";
	[[nodiscard]] auto                tour_comment_lead(const review_tour&, bool aBriefInDescription) -> std::string;
	/// [[BPR-1787]]. A published tour's brief stays where it was first
	/// published: its own comment (every tour before BPR-1787, and any tour
	/// published with the description left alone), the description, or both.
	[[nodiscard]] auto tour_brief_in_comment(const review_tour&) -> bool;
	[[nodiscard]] auto tour_brief_in_description(const review_tour&) -> bool;
}
#endif
