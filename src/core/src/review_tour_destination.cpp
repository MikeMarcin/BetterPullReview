#include "bpr/core/review_tour_destination.hpp"
namespace bpr::core {
	auto plan_tour_destination(const tour_destination_observation& aObservation) noexcept -> tour_destination_plan
	{
		using support        = tour_operation_support;
		using representation = tour_representation;
		if (!valid(aObservation.Source) || !aObservation.SourceAvailable || aObservation.Subject > tour_subject::unchanged_file
			|| aObservation.Body > review_body_compatibility::requires_line_subject) {
			return { };
		}
		const auto native = [](support aSupport) -> tour_destination_plan {
			if (aSupport != support::supported) {
				return { };
			}
			return { representation::native_line, true, false, false, false };
		};
		if (aObservation.Subject == tour_subject::current_diff) {
			return native(aObservation.PendingLineBatch);
		}
		if (aObservation.Subject == tour_subject::current_off_hunk && aObservation.PendingOffHunkBatch != support::unsupported) {
			return native(aObservation.PendingOffHunkBatch);
		}
		if (aObservation.Subject == tour_subject::unknown) {
			return { };
		}
		if (aObservation.Body == review_body_compatibility::requires_line_subject) {
			return { .IncompatibleBody = true };
		}
		if (aObservation.Subject == tour_subject::current_off_hunk) {
			if (aObservation.PendingFileThread == support::supported) {
				return { representation::file_permalink, true, false, false, false };
			}
			// Denied, rate-limited and indeterminate pending operations cannot authorize
			// another visible write. Only definitive unsupported evidence admits fallback.
			if (aObservation.PendingFileThread != support::unsupported) {
				return { };
			}
			if (aObservation.StandaloneFile == support::supported) {
				return { representation::file_permalink, false, true, false, false };
			}
			return { .RequiresCompatibility = true };
		}
		if (aObservation.Conversation == support::supported) {
			return { representation::conversation_permalink, false, true, false, false };
		}
		return { .RequiresCompatibility = true };
	}
}
