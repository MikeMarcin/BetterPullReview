#ifndef BPR_CORE_REVIEW_TOUR_DESTINATION_HPP
#define BPR_CORE_REVIEW_TOUR_DESTINATION_HPP
#include "bpr/core/review_publication_policy.hpp"
#include "bpr/core/review_tour.hpp"
namespace bpr::core {
	enum class tour_subject : std::uint8_t { unknown,
											 current_diff,
											 current_off_hunk,
											 historical,
											 unchanged_file };
	enum class tour_operation_support : std::uint8_t { unknown,
													   supported,
													   unsupported,
													   denied,
													   rate_limited,
													   indeterminate };
	enum class tour_representation : std::uint8_t { unresolved,
													native_line,
													file_permalink,
													conversation_permalink };
	/// Evidence belongs to these exact operations, source range and acting scope.
	/// Standalone line creation and website support never qualify a pending batch.
	struct tour_destination_observation
	{
		tour_code_destination     Source;
		tour_subject              Subject             = tour_subject::unknown;
		bool                      SourceAvailable     = false;
		review_body_compatibility Body                = review_body_compatibility::portable;
		tour_operation_support    PendingLineBatch    = tour_operation_support::unknown;
		tour_operation_support    PendingOffHunkBatch = tour_operation_support::unknown;
		tour_operation_support    PendingFileThread   = tour_operation_support::unknown;
		tour_operation_support    StandaloneFile      = tour_operation_support::unknown;
		tour_operation_support    Conversation        = tour_operation_support::unknown;
	};
	struct tour_destination_plan
	{
		tour_representation Representation                                         = tour_representation::unresolved;
		bool                JoinsPendingReview                                     = false;
		bool                RequiresCompatibility                                  = false;
		bool                NeedsQualification                                     = true;
		bool                IncompatibleBody                                       = false;
		auto                operator==(const tour_destination_plan&) const -> bool = default;
	};
	[[nodiscard]] auto plan_tour_destination(const tour_destination_observation& aObservation) noexcept -> tour_destination_plan;
}
#endif
