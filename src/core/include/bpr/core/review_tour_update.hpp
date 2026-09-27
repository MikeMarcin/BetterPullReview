#ifndef BPR_CORE_REVIEW_TOUR_UPDATE_HPP
#define BPR_CORE_REVIEW_TOUR_UPDATE_HPP
#include "bpr/core/review_tour_markdown.hpp"
namespace bpr::core {
	/// A body-only edit to an observed owned object; the baseline is a preflight, not a remote CAS.
	struct tour_body_update
	{
		tour_remote_binding Remote;
		std::string         Baseline, Desired;
		auto                operator==(const tour_body_update&) const -> bool = default;
	};
	struct tour_created_destination
	{
		tour_stop_id        Stop;
		tour_remote_binding Remote;
		std::string         InitialBody;
	};
	/// Ordered edits keep every discussion and update the authoritative index last.
	struct tour_update_bodies
	{
		std::vector<tour_body_update> Edits;
		std::vector<std::string>      StopBodies;
		std::string                   Index;
	};
	[[nodiscard]] auto needs_tour_destination(const tour_stop&) -> bool;
	/// Pure bounded planning: exact raw spans, real returned destinations, no no-op writes.
	[[nodiscard]] auto plan_tour_update(const review_tour&, std::span<const tour_created_destination>) -> std::optional<tour_update_bodies>;
}
#endif
