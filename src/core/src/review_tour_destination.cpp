#include "bpr/core/review_tour_destination.hpp"
#include <algorithm>
namespace bpr::core {
	auto assess_tour_publication(const review_tour& aTour, std::span<const tour_destination_plan> aDestinations, bool aCompatibility) noexcept -> tour_publication_readiness
	{
		using state      = tour_publication_readiness;
		const auto blank = [](std::string_view value) { return value.find_first_not_of(" \r\n\t") == std::string_view::npos; };
		if (validate(aTour) != tour_refusal::none || aTour.ReadOnly || aTour.Remote || !aTour.Head.valid() || !aTour.Base.valid() || aTour.Stops.empty() || aTour.Stops.size() > 100 || blank(aTour.Problem) || blank(aTour.Solution) || std::ranges::any_of(aTour.Stops, [](const auto& s) { return !ready(s); })) {
			return state::incomplete;
		}
		if (aDestinations.size() != aTour.Stops.size() || std::ranges::any_of(aDestinations, [](const auto& d) { return d.NeedsQualification || d.IncompatibleBody || d.Representation == tour_representation::unresolved; })) {
			return state::unavailable;
		}
		const bool fallback = std::ranges::any_of(aDestinations, [](const auto& d) { return d.RequiresCompatibility || !d.JoinsPendingReview; });
		if (fallback && !aCompatibility) {
			return state::compatibility_choice;
		}
		if (aCompatibility) {
			if (std::ranges::any_of(aDestinations, [](const auto& d) { return d.Representation != tour_representation::conversation_permalink || !d.RequiresCompatibility; })) {
				return state::unavailable;
			}
			return state::compatibility_comments;
		}
		if (std::ranges::any_of(aDestinations, [](const auto& d) { return d.Representation != tour_representation::native_line; })) {
			return state::unavailable;
		}
		return state::single_review;
	}

	auto plan_tour_destination(const tour_destination_observation& aObservation) noexcept -> tour_destination_plan
	{
		using support        = tour_operation_support;
		using representation = tour_representation;
		if (!valid(aObservation.Source) || !aObservation.SourceAvailable || aObservation.Subject > tour_subject::unchanged_file
			|| aObservation.Body > review_body_compatibility::requires_line_subject) {
			return { };
		}
		if (aObservation.CompatibilityRequested) {
			if (aObservation.Body == review_body_compatibility::requires_line_subject) {
				return { .IncompatibleBody = true };
			}
			return aObservation.Conversation == support::supported ? tour_destination_plan{ representation::conversation_permalink, false, true, false, false } : tour_destination_plan{ };
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
