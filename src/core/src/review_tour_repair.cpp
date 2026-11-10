#include "bpr/core/review_tour_repair.hpp"

#include <algorithm>
#include <charconv>

namespace bpr::core {
	namespace {
		auto file_for(const std::span<const tour_anchor_file> aFiles, const std::string_view aPath) -> const tour_anchor_file*
		{
			const auto found = std::ranges::find(aFiles, aPath, &tour_anchor_file::Path);
			if (found == aFiles.end() || std::find_if(std::next(found), aFiles.end(), [&](const auto& aFile) { return aFile.Path == aPath; }) != aFiles.end()) {
				return nullptr;
			}
			return &*found;
		}
		auto inside_hunk(const tour_code_destination& aSource, const tour_anchor_file& aFile) -> bool
		{
			const auto clamped = nearest_tour_hunk({ aSource.FirstLine + 1U, aSource.LastLine + 1U }, aFile);
			return clamped && clamped->First == aSource.FirstLine + 1U && clamped->Last == aSource.LastLine + 1U;
		}
		auto exact_head_source(const review_tour& aTour, const tour_stop& aStop, const std::span<const tour_anchor_file> aFiles) -> std::optional<tour_source_range>
		{
			if (aStop.Mapping != tour_mapping::unchanged_material || !aStop.Mapped || !valid(*aStop.Mapped) || !aStop.Mapped->Blob
				|| aStop.Mapped->Commit != aTour.ObservedHead.value_or(aTour.Head) || aStop.Mapped->First.Side != diff_side::right) {
				return std::nullopt;
			}
			const auto* const file   = file_for(aFiles, aStop.Mapped->Path);
			const auto        source = publication_source(aStop);
			return file && source && source->LastLine - source->FirstLine == aStop.Mapped->Last.Line - aStop.Mapped->First.Line
					&& inside_hunk(destination_of(*aStop.Mapped), *file)
				? aStop.Mapped
				: std::nullopt;
		}
		auto historical_problem(const tour_mapping aMapping) -> tour_destination_problem
		{
			switch (aMapping) {
			case tour_mapping::changed_material: return tour_destination_problem::historical_changed;
			case tour_mapping::ambiguous:        return tour_destination_problem::historical_ambiguous;
			case tour_mapping::missing:          return tour_destination_problem::historical_missing;
			default:                             return tour_destination_problem::historical_unavailable;
			}
		}
		auto is_word(const char aChar) -> bool
		{
			return (aChar >= 'a' && aChar <= 'z') || (aChar >= 'A' && aChar <= 'Z') || (aChar >= '0' && aChar <= '9') || aChar == '_';
		}
		auto line_reference_prefix(std::string_view aText, const std::size_t aAt) -> std::size_t
		{
			if (aAt && is_word(aText[aAt - 1U])) {
				return 0U;
			}
			const auto tail = aText.substr(aAt);
			if (tail.starts_with("L") && tail.size() > 1U && tail[1] >= '0' && tail[1] <= '9') {
				return aAt + 1U;
			}
			if (!tail.starts_with("line") && !tail.starts_with("Line") && !tail.starts_with("LINE")) {
				return 0U;
			}
			auto next = aAt + 4U;
			if (next < aText.size() && (aText[next] == 's' || aText[next] == 'S')) {
				++next;
			}
			if (next >= aText.size() || (aText[next] != ' ' && aText[next] != '\t')) {
				return 0U;
			}
			return aText.find_first_not_of(" \t", next);
		}
		auto line_reference_end(std::string_view aTail, const std::uint32_t aFirst) -> std::uint32_t
		{
			const auto nonspace = aTail.find_first_not_of(" \t");
			if (nonspace == std::string_view::npos) {
				return aFirst;
			}
			aTail.remove_prefix(nonspace);
			for (const std::string_view separator : { "-", "–", "—", "..", "to ", "through " }) {
				if (!aTail.starts_with(separator)) {
					continue;
				}
				aTail.remove_prefix(separator.size());
				const auto next = aTail.find_first_not_of(" \t");
				if (next == std::string_view::npos) {
					return aFirst;
				}
				aTail.remove_prefix(next);
				if (aTail.starts_with("L")) {
					aTail.remove_prefix(1U);
				}
				std::uint32_t last   = 0U;
				const auto    parsed = std::from_chars(aTail.data(), aTail.data() + aTail.size(), last);
				return parsed.ec == std::errc{ } && last >= aFirst ? last : aFirst;
			}
			return aFirst;
		}
		auto references_moved_lines(const std::string_view aBody, const tour_code_destination& aBefore, const tour_source_range& aAfter) -> bool
		{
			if (aBefore.FirstLine == aAfter.First.Line && aBefore.LastLine == aAfter.Last.Line) {
				return false;
			}
			for (std::size_t at = 0U; at < aBody.size(); ++at) {
				const auto start = line_reference_prefix(aBody, at);
				if (!start || start >= aBody.size()) {
					continue;
				}
				std::uint32_t number = 0U;
				const auto    parsed = std::from_chars(aBody.data() + start, aBody.data() + aBody.size(), number);
				if (parsed.ec != std::errc{ }) {
					continue;
				}
				const auto last         = line_reference_end(aBody.substr(static_cast<std::size_t>(parsed.ptr - aBody.data())), number);
				const auto firstChanged = aBefore.FirstLine == aAfter.First.Line ? static_cast<std::uint64_t>(aAfter.Last.Line) + 2U : static_cast<std::uint64_t>(aBefore.FirstLine) + 1U;
				if (number <= aBefore.LastLine + 1U && last >= firstChanged) {
					return true;
				}
			}
			return false;
		}
	}

	auto plan_tour_stop_repair(const review_tour& aTour, const tour_stop& aStop, const tour_destination_observation& aObservation, const std::span<const tour_anchor_file> aFiles) -> tour_destination_repair
	{
		auto result         = tour_destination_repair{ };
		result.Stop         = aStop.Id;
		result.Title        = aStop.Title;
		const auto position = std::ranges::find(aTour.Stops, aStop.Id, &tour_stop::Id);
		result.Number       = position == aTour.Stops.end() ? 0U : static_cast<std::size_t>(position - aTour.Stops.begin()) + 1U;
		const auto source   = publication_source(aStop);
		if (!result.Number || !source || *source != aObservation.Source || !aObservation.SourceAvailable || !valid(*source)) {
			result.Problem = tour_destination_problem::source_unavailable;
			return result;
		}
		auto       observed = aObservation;
		const auto head     = aTour.ObservedHead.value_or(aTour.Head);
		const bool current  = source->Commit == (source->Side == diff_side::right ? head : aTour.Base);
		if (!current) {
			observed.Subject = tour_subject::historical;
		} else if (source->Side == diff_side::right && observed.Subject == tour_subject::current_diff) {
			const auto* const file = file_for(aFiles, source->Path);
			if (file && file->OnHead && file->Hunks && !inside_hunk(*source, *file)) {
				observed.Subject = tour_subject::current_off_hunk;
			}
		}
		result.Destination = plan_tour_destination(observed);
		if (result.Destination.JoinsPendingReview && !result.Destination.NeedsQualification && !result.Destination.IncompatibleBody) {
			return result;
		}
		if (observed.Subject == tour_subject::historical) {
			result.Repin   = exact_head_source(aTour, aStop, aFiles);
			result.Problem = result.Repin ? tour_destination_problem::historical_exact : historical_problem(aStop.Mapping);
		} else if (observed.Subject == tour_subject::current_off_hunk) {
			result.Problem             = tour_destination_problem::off_hunk;
			const auto* const file     = file_for(aFiles, source->Path);
			const auto        selected = aStop.Mapping == tour_mapping::author_selected && aStop.Mapped ? aStop.Mapped : aStop.Original;
			if (source->Side == diff_side::right && source->Commit == head && file && selected && selected->Blob) {
				if (const auto range = nearest_tour_hunk({ source->FirstLine + 1U, source->LastLine + 1U }, *file)) {
					result.Relocation             = *selected;
					result.Relocation->First.Line = range->First - 1U;
					result.Relocation->Last.Line  = range->Last - 1U;
				}
			}
			using support      = tour_operation_support;
			result.FileComment = observed.Body == review_body_compatibility::portable && observed.PendingOffHunkBatch == support::unsupported
				&& (observed.PendingFileThread == support::supported || (observed.PendingFileThread == support::unsupported && observed.StandaloneFile == support::supported));
		} else {
			result.Problem = result.Destination.IncompatibleBody   ? tour_destination_problem::incompatible_body
				: observed.Subject == tour_subject::unchanged_file ? tour_destination_problem::unchanged_file
																   : tour_destination_problem::operation_unavailable;
		}
		return result;
	}

	auto tour_destination_repairs(const review_tour& aTour, const std::span<const tour_destination_observation> aObservations, const std::span<const tour_anchor_file> aFiles) -> std::vector<tour_destination_repair>
	{
		auto result = std::vector<tour_destination_repair>{ };
		for (std::size_t index = 0U; index < aTour.Stops.size(); ++index) {
			auto repair = plan_tour_stop_repair(aTour, aTour.Stops[index], index < aObservations.size() ? aObservations[index] : tour_destination_observation{ }, aFiles);
			if (repair.Problem != tour_destination_problem::none) {
				result.push_back(std::move(repair));
			}
		}
		return result;
	}

	auto prepare_tour_file_comment(const review_tour& aTour, tour_stop_id aStop, const tour_destination_observation& aObservation, std::span<const tour_anchor_file> aFiles) -> std::optional<tour_command>
	{
		const auto found = std::ranges::find(aTour.Stops, aStop, &tour_stop::Id);
		if (aTour.ReadOnly || aTour.Remote || validate(aTour) != tour_refusal::none || found == aTour.Stops.end() || found->Remote || found->FileCommentRequested) {
			return { };
		}
		const auto repair = plan_tour_stop_repair(aTour, *found, aObservation, aFiles);
		if (!repair.FileComment || aObservation.PendingFileThread != tour_operation_support::unsupported || aObservation.StandaloneFile != tour_operation_support::supported) {
			return { };
		}
		auto candidate                                                                              = aTour;
		candidate.Stops[static_cast<std::size_t>(found - aTour.Stops.begin())].FileCommentRequested = true;
		return tour_command{ .Draft = aTour.Id, .Owner = aTour.Owner, .ExpectedRevision = aTour.Revision, .Kind = tour_command_kind::restore, .Stop = aStop, .Restore = std::move(candidate) };
	}
	auto prepare_tour_source_repair(const review_tour& aTour, const tour_stop_id aStop, const tour_source_repair_kind aKind, const tour_destination_observation& aObservation, const std::span<const tour_anchor_file> aFiles) -> std::optional<tour_command>
	{
		const auto found = std::ranges::find(aTour.Stops, aStop, &tour_stop::Id);
		if (aTour.ReadOnly || aTour.Remote || validate(aTour) != tour_refusal::none || found == aTour.Stops.end() || found->Remote) {
			return std::nullopt;
		}
		const auto repair      = plan_tour_stop_repair(aTour, *found, aObservation, aFiles);
		const auto source      = publication_source(*found);
		const auto replacement = aKind == tour_source_repair_kind::repin_head ? repair.Repin : aKind == tour_source_repair_kind::nearest_hunk ? repair.Relocation
																																			  : std::nullopt;
		if (!source || !replacement || destination_of(*replacement) == *source) {
			return std::nullopt;
		}
		auto  candidate = aTour;
		auto& stop      = candidate.Stops[static_cast<std::size_t>(found - aTour.Stops.begin())];
		stop.Mapped     = replacement;
		stop.Mapping    = tour_mapping::author_selected;
		if (references_moved_lines(stop.Body, *source, *replacement)) {
			stop.ReaffirmedHead.reset();
			stop.Preparation = tour_preparation::needs_attention;
		} else {
			stop.ReaffirmedHead = aTour.ObservedHead.value_or(aTour.Head);
			if (stop.Preparation == tour_preparation::needs_attention) {
				stop.Preparation = stop.Suggestion ? tour_preparation::editing : tour_preparation::ready;
			}
		}
		if (validate(candidate) != tour_refusal::none) {
			return std::nullopt;
		}
		return tour_command{ .Draft = aTour.Id, .Owner = aTour.Owner, .ExpectedRevision = aTour.Revision, .Kind = tour_command_kind::restore, .Stop = aStop, .Restore = std::move(candidate) };
	}
}
