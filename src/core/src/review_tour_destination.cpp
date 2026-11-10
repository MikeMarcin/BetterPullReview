#include "bpr/core/review_tour_destination.hpp"
#include "bpr/core/review_tour_update.hpp"
#include <algorithm>
namespace bpr::core {
	auto tour_reason_name(const tour_status_reason aReason) noexcept -> std::string_view
	{
		switch (aReason) {
		case tour_status_reason::source_not_visible:         return "source_not_visible";
		case tour_status_reason::lines_unconfirmed:          return "lines_unconfirmed";
		case tour_status_reason::source_changed:             return "source_changed";
		case tour_status_reason::source_ambiguous:           return "source_ambiguous";
		case tour_status_reason::source_missing:             return "source_missing";
		case tour_status_reason::source_unavailable:         return "source_unavailable";
		case tour_status_reason::title_missing:              return "title_missing";
		case tour_status_reason::explanation_missing:        return "explanation_missing";
		case tour_status_reason::generated_unvisited:        return "generated_unvisited";
		case tour_status_reason::generated_not_ready:        return "generated_not_ready";
		case tour_status_reason::preparation_attention:      return "preparation_attention";
		case tour_status_reason::explanation_not_reaffirmed: return "explanation_not_reaffirmed";
		case tour_status_reason::read_only:                  return "read_only";
		case tour_status_reason::invalid_tour:               return "invalid_tour";
		case tour_status_reason::problem_missing:            return "problem_missing";
		case tour_status_reason::solution_missing:           return "solution_missing";
		case tour_status_reason::no_stops:                   return "no_stops";
		case tour_status_reason::too_many_stops:             return "too_many_stops";
		case tour_status_reason::remote_not_owned:           return "remote_not_owned";
		case tour_status_reason::remote_stale:               return "remote_stale";
		case tour_status_reason::edit_conflict:              return "edit_conflict";
		case tour_status_reason::baseline_missing:           return "baseline_missing";
		case tour_status_reason::destination_unknown:        return "destination_unknown";
		case tour_status_reason::incompatible_body:          return "incompatible_body";
		case tour_status_reason::compatibility_required:     return "compatibility_required";
		case tour_status_reason::destination_unsupported:    return "destination_unsupported";
		case tour_status_reason::body_edits_unqualified:     return "body_edits_unqualified";
		case tour_status_reason::stop_not_owned:             return "stop_not_owned";
		case tour_status_reason::stop_baseline_missing:      return "stop_baseline_missing";
		}
		return "invalid_tour";
	}
	auto tour_mapping_name(const tour_mapping aMapping) noexcept -> std::string_view
	{
		switch (aMapping) {
		case tour_mapping::exact_original:     return "exact_original";
		case tour_mapping::unchanged_material: return "unchanged_material";
		case tour_mapping::changed_material:   return "changed_material";
		case tour_mapping::ambiguous:          return "ambiguous";
		case tour_mapping::missing:            return "missing";
		case tour_mapping::unavailable:        return "unavailable";
		case tour_mapping::author_selected:    return "author_selected";
		}
		return "unavailable";
	}
	auto project_tour_stop_status(const review_tour& aTour, const tour_stop& aStop, const tour_attention& aAttention) -> tour_stop_status
	{
		using reason          = tour_status_reason;
		auto       result     = tour_stop_status{ };
		const bool original   = aStop.Original && valid(*aStop.Original);
		const bool selected   = aStop.Mapping == tour_mapping::author_selected && aStop.Mapped && valid(*aStop.Mapped);
		const bool remote     = aStop.Remote && aStop.Destination && valid(*aStop.Destination);
		result.ConfirmedLines = original || selected || remote;
		if (!result.ConfirmedLines) {
			result.SourceReasons.push_back(reason::lines_unconfirmed);
		}
		switch (aStop.Mapping) {
		case tour_mapping::changed_material: result.SourceReasons.push_back(reason::source_changed); break;
		case tour_mapping::ambiguous:        result.SourceReasons.push_back(reason::source_ambiguous); break;
		case tour_mapping::missing:          result.SourceReasons.push_back(reason::source_missing); break;
		case tour_mapping::unavailable:      result.SourceReasons.push_back(reason::source_unavailable); break;
		case tour_mapping::author_selected:
			if (!selected) {
				result.SourceReasons.push_back(reason::source_unavailable);
			}
			break;
		case tour_mapping::exact_original:
		case tour_mapping::unchanged_material: break;
		}
		result.SourceAttention = !result.SourceReasons.empty();
		const auto blank       = [](std::string_view aText) { return aText.find_first_not_of(" \r\n\t") == std::string_view::npos; };
		if (blank(aStop.Title)) {
			result.ReadyReasons.push_back(reason::title_missing);
		}
		if (blank(aStop.Body)) {
			result.ReadyReasons.push_back(reason::explanation_missing);
		}
		const auto readiness = source_readiness(aStop);
		if (readiness != tour_source_readiness::ready) {
			result.ReadyReasons.insert(result.ReadyReasons.end(), result.SourceReasons.begin(), result.SourceReasons.end());
			if (result.SourceReasons.empty()) {
				result.ReadyReasons.push_back(readiness == tour_source_readiness::reaffirmation_required ? reason::explanation_not_reaffirmed : reason::source_unavailable);
			}
		}
		const auto sourceReadiness = result.ReadyReasons;
		if (aTour.ReadOnly) {
			result.ReadyReasons.push_back(reason::read_only);
		}
		if (aStop.Suggestion && !visible_tour_stop(aStop, aAttention, true)) {
			result.ReadyReasons.push_back(aStop.Suggestion->Visited ? reason::source_not_visible : reason::generated_unvisited);
		}
		result.ReadyEligible      = ready(aStop) && result.ReadyReasons.empty();
		result.ExplanationCurrent = !aTour.ObservedHead || aStop.ReaffirmedHead == aTour.ObservedHead;
		if ((!aTour.Remote || needs_tour_destination(aStop)) && !ready(aStop)) {
			result.PublicationReasons = sourceReadiness;
		}
		if (aStop.Suggestion && aStop.Preparation != tour_preparation::ready) {
			result.PublicationReasons.push_back(reason::generated_not_ready);
		}
		if (aStop.Preparation == tour_preparation::needs_attention) {
			result.PublicationReasons.push_back(reason::preparation_attention);
		}
		if (!result.ExplanationCurrent && std::ranges::find(result.PublicationReasons, reason::explanation_not_reaffirmed) == result.PublicationReasons.end()) {
			result.PublicationReasons.push_back(reason::explanation_not_reaffirmed);
		}
		if (aTour.Remote && !needs_tour_destination(aStop)) {
			if (!aStop.Remote->Own) {
				result.PublicationReasons.push_back(reason::stop_not_owned);
			}
			if (!aStop.Baseline) {
				result.PublicationReasons.push_back(reason::stop_baseline_missing);
			}
		}
		if (publication_source(aStop) || aStop.Mapped) {
			result.AvailableActions.emplace_back("Show in code");
		}
		if (!aTour.ReadOnly) {
			result.AvailableActions.emplace_back("Edit stop");
			if (aAttention.Source && valid(*aAttention.Source)) {
				result.AvailableActions.emplace_back("Use selected lines");
			}
			if (result.ReadyEligible && aStop.Preparation != tour_preparation::ready) {
				result.AvailableActions.emplace_back("Ready, next");
			}
			if ((!result.ExplanationCurrent || std::ranges::find(result.ReadyReasons, reason::explanation_not_reaffirmed) != result.ReadyReasons.end()) && reaffirm_need(aStop) == tour_reaffirm_need::nothing) {
				result.AvailableActions.emplace_back("Head changed");
			}
		}
		return result;
	}
	auto tour_publication_reasons(const review_tour& aTour, const std::span<const tour_destination_plan> aDestinations, const bool aCompatibility, const tour_operation_support aBodyEdits) -> std::vector<tour_status_reason>
	{
		using reason      = tour_status_reason;
		auto       result = std::vector<reason>{ };
		const auto blank  = [](std::string_view aText) { return aText.find_first_not_of(" \r\n\t") == std::string_view::npos; };
		if (validate(aTour) != tour_refusal::none || !aTour.Head.valid() || !aTour.Base.valid()) {
			result.push_back(reason::invalid_tour);
		}
		if (aTour.ReadOnly) {
			result.push_back(reason::read_only);
		}
		if (aTour.Stops.empty()) {
			result.push_back(reason::no_stops);
		}
		if (!aTour.Remote && aTour.Stops.size() > 100U) {
			result.push_back(reason::too_many_stops);
		}
		const bool newDestinations = !aTour.Remote || std::ranges::any_of(aTour.Stops, needs_tour_destination);
		if (newDestinations && blank(aTour.Problem)) {
			result.push_back(reason::problem_missing);
		}
		if (newDestinations && blank(aTour.Solution)) {
			result.push_back(reason::solution_missing);
		}
		if (!aTour.Conflicts.empty()) {
			result.push_back(reason::edit_conflict);
		}
		if (aTour.RemoteStale) {
			result.push_back(reason::remote_stale);
		}
		if (aTour.Remote) {
			if (!aTour.Remote->Own) {
				result.push_back(reason::remote_not_owned);
			}
			if (!aTour.IndexBaseline) {
				result.push_back(reason::baseline_missing);
			}
			if (aBodyEdits != tour_operation_support::supported) {
				result.push_back(reason::body_edits_unqualified);
			}
		}
		for (const auto& stop : aTour.Stops) {
			for (const auto reasonCode : project_tour_stop_status(aTour, stop).PublicationReasons) {
				if (std::ranges::find(result, reasonCode) == result.end()) {
					result.push_back(reasonCode);
				}
			}
		}
		if (aDestinations.size() != aTour.Stops.size()) {
			result.push_back(reason::destination_unknown);
		}
		for (std::size_t index = 0U; index < aDestinations.size(); ++index) {
			if (aTour.Remote && index < aTour.Stops.size() && !needs_tour_destination(aTour.Stops[index])) {
				continue;
			}
			const auto& destination = aDestinations[index];
			if (destination.NeedsQualification || destination.Representation == tour_representation::unresolved) {
				result.push_back(reason::destination_unknown);
			}
			if (destination.IncompatibleBody) {
				result.push_back(reason::incompatible_body);
			}
			if (!destination.NeedsQualification && destination.Representation != tour_representation::unresolved) {
				const bool fallback = destination.RequiresCompatibility || !destination.JoinsPendingReview;
				if (!aCompatibility && fallback) {
					result.push_back(reason::compatibility_required);
				} else if ((aCompatibility && (destination.Representation != tour_representation::conversation_permalink || !destination.RequiresCompatibility)) || (!aCompatibility && destination.Representation != tour_representation::native_line)) {
					result.push_back(reason::destination_unsupported);
				}
			}
		}
		auto unique = std::vector<reason>{ };
		for (const auto code : result) {
			if (std::ranges::find(unique, code) == unique.end()) {
				unique.push_back(code);
			}
		}
		return unique;
	}
	auto assess_tour_publication(const review_tour& aTour, std::span<const tour_destination_plan> aDestinations, bool aCompatibility) noexcept -> tour_publication_readiness
	{
		using state      = tour_publication_readiness;
		const auto blank = [](std::string_view aValue) { return aValue.find_first_not_of(" \r\n\t") == std::string_view::npos; };
		if (validate(aTour) != tour_refusal::none || aTour.ReadOnly || aTour.RemoteStale || !aTour.Conflicts.empty() || aTour.Remote || !aTour.Head.valid() || !aTour.Base.valid() || aTour.Stops.empty() || aTour.Stops.size() > 100 || blank(aTour.Problem) || blank(aTour.Solution) || std::ranges::any_of(aTour.Stops, [&](const auto& aS) { return !ready(aS) || (aS.Suggestion && aS.Preparation != tour_preparation::ready) || aS.Preparation == tour_preparation::needs_attention || (aTour.ObservedHead && aS.ReaffirmedHead != aTour.ObservedHead); })) {
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
			if (aObservation.StandaloneFile == support::unsupported && aObservation.Conversation == support::supported) {
				return { representation::conversation_permalink, false, true, false, false };
			}
			return { .RequiresCompatibility = true };
		}
		if (aObservation.Conversation == support::supported) {
			return { representation::conversation_permalink, false, true, false, false };
		}
		return { .RequiresCompatibility = true };
	}
}
