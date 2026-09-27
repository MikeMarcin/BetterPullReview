#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_TOUR_PROJECTION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_TOUR_PROJECTION_HPP
#include "bpr/core/review_tour.hpp"
#include <memory>
namespace bpr::application {
	enum class tour_store_failure : std::uint8_t { unavailable,
												   cancelled,
												   corrupt,
												   schema_mismatch,
												   conflict,
												   bounds,
												   wrong_profile };
	/// \brief Inert private-draft and local-save projection; no database or remote capability.
	struct tour_presentation
	{
		std::shared_ptr<const core::review_tour> Draft;
		std::shared_ptr<const core::review_tour> Conflict;
		std::optional<tour_store_failure>        Failure;
		core::tour_refusal                       Refusal = core::tour_refusal::none;
		core::tour_stop_id                       Selected;
		std::uint64_t                            SavedRevision = 0;
		bool                                     Loading       = false;
		bool                                     Saving        = false;
		bool                                     CanUndo       = false;
		bool                                     CanRedo       = false;
	};
}
#endif
