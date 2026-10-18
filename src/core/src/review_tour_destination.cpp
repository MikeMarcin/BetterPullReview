#include "bpr/core/review_tour_destination.hpp"
#include "bpr/core/review_tour_update.hpp"
#include <algorithm>
namespace bpr::core {
	auto assess_tour_publication(const review_tour& aTour, std::span<const tour_destination_plan> aDestinations, bool aCompatibility) noexcept -> tour_publication_readiness
	{
		using state      = tour_publication_readiness;
		const auto blank = [](std::string_view aValue) { return aValue.find_first_not_of(" \r\n\t") == std::string_view::npos; };
		if (validate(aTour) != tour_refusal::none || aTour.ReadOnly || aTour.Remote || !aTour.Head.valid() || !aTour.Base.valid() || aTour.Stops.empty() || aTour.Stops.size() > 100 || blank(aTour.Problem) || blank(aTour.Solution) || std::ranges::any_of(aTour.Stops, [&](const auto& aS) { return !ready(aS) || (aS.Suggestion && aS.Preparation != tour_preparation::ready) || aS.Preparation == tour_preparation::needs_attention || (aTour.ObservedHead && aS.ReaffirmedHead != aTour.ObservedHead); })) {
			return state::incomplete;
		}
		if (aDestinations.size() != aTour.Stops.size() || std::ranges::any_of(aDestinations, [](const auto& aD) { return aD.NeedsQualification || aD.IncompatibleBody || aD.Representation == tour_representation::unresolved; })) {
			return state::unavailable;
		}
		const bool fallback = std::ranges::any_of(aDestinations, [](const auto& aD) { return aD.RequiresCompatibility || !aD.JoinsPendingReview; });
		if (fallback && !aCompatibility) {
			return state::compatibility_choice;
		}
		if (aCompatibility) {
			if (std::ranges::any_of(aDestinations, [](const auto& aD) { return aD.Representation != tour_representation::conversation_permalink || !aD.RequiresCompatibility; })) {
				return state::unavailable;
			}
			return state::compatibility_comments;
		}
		if (std::ranges::any_of(aDestinations, [](const auto& aD) { return aD.Representation != tour_representation::native_line; })) {
			return state::unavailable;
		}
		return state::single_review;
	}

	auto assess_tour_update(const review_tour& aTour, std::span<const tour_destination_plan> aDestinations, bool aCompatibility, tour_operation_support aBodyEdits) -> tour_publication_readiness
	{
		using state = tour_publication_readiness;
		if (validate(aTour) != tour_refusal::none || aTour.RemoteStale || !aTour.Conflicts.empty() || aTour.ReadOnly || !aTour.Remote || !aTour.Remote->Own || !aTour.IndexBaseline || aTour.Stops.empty() || aDestinations.size() != aTour.Stops.size()) {
			return state::incomplete;
		}
		if (aBodyEdits != tour_operation_support::supported) {
			return state::unavailable;
		}
		review_tour fresh = aTour;
		fresh.Remote.reset();
		fresh.Stops.clear();
		fresh.Retired.clear();
		std::vector<tour_destination_plan> destinations;
		for (std::size_t i = 0; i < aTour.Stops.size(); ++i) {
			const auto& stop = aTour.Stops[i];
			if ((stop.Suggestion && stop.Preparation != tour_preparation::ready) || (aTour.ObservedHead && stop.ReaffirmedHead != aTour.ObservedHead)) {
				return state::incomplete;
			}
			if (needs_tour_destination(stop)) {
				auto added = stop;
				added.Remote.reset();
				fresh.Stops.push_back(added);
				destinations.push_back(aDestinations[i]);
			} else if (!stop.Remote->Own || !stop.Baseline || stop.Preparation == tour_preparation::needs_attention) {
				return state::incomplete;
			}
		}
		return fresh.Stops.empty() ? (aCompatibility ? state::compatibility_comments : state::single_review) : assess_tour_publication(fresh, destinations, aCompatibility);
	}
	auto publication_source(const tour_stop& aStop) -> std::optional<tour_code_destination>
	{
		if (aStop.Mapping == tour_mapping::author_selected && aStop.Mapped) {
			return destination_of(*aStop.Mapped);
		}
		return aStop.Original ? std::optional{ destination_of(*aStop.Original) } : aStop.Destination;
	}
	auto publication_observation(const tour_code_destination& aSource, const git_commit_oid& aHead, const git_commit_oid& aBase, const std::string_view aBody, const bool aCompatibility, const bool aQualified) noexcept
		-> tour_destination_observation
	{
		const auto current  = aSource.Commit == (aSource.Side == diff_side::right ? aHead : aBase);
		auto       observed = tour_destination_observation{ .Source = aSource, .Subject = current ? tour_subject::current_diff : tour_subject::historical, .SourceAvailable = aSource.Blob.has_value(), .Body = classify_review_body(aBody), .CompatibilityRequested = aCompatibility };
		if (aQualified) {
			observed.PendingLineBatch = tour_operation_support::supported;
			observed.Conversation     = tour_operation_support::supported;
		}
		return observed;
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
