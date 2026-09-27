#include "bpr/core/review_tour.hpp"

#include <algorithm>
#include <limits>
#include <ranges>

namespace bpr::core {
	namespace {
		auto nonblank(std::string_view aText) noexcept -> bool
		{
			return aText.find_first_not_of(" \t\r\n") != std::string_view::npos;
		}
		auto field_ok(std::string_view aText) noexcept -> bool
		{
			return aText.size() <= maximum_tour_field_bytes && aText.find('\0') == std::string_view::npos;
		}
		auto source_bytes(const tour_source_range& aSource) noexcept -> std::size_t { return 192U + aSource.Path.size(); }
		auto baseline_bytes(const std::optional<tour_field_baseline>& aValue) noexcept -> std::size_t
		{
			return aValue ? 64U + aValue->Raw.size() + aValue->RemoteEntity.size() + aValue->ObservedVersion.size() : 1U;
		}
		auto baseline_ok(const std::optional<tour_field_baseline>& aValue) noexcept -> bool
		{
			return !aValue || (field_ok(aValue->Raw) && aValue->RemoteEntity.size() <= 1024U && aValue->ObservedVersion.size() <= 1024U && (!aValue->SourceSpan || (aValue->SourceSpan->first <= aValue->SourceSpan->second && aValue->SourceSpan->second <= aValue->Raw.size())));
		}
		auto remote_bytes(const std::optional<tour_remote_binding>& aValue) noexcept -> std::size_t
		{
			return aValue ? 64U + aValue->Entity.size() + aValue->Actor.size() + aValue->WebUrl.size() + aValue->Review.size() : 1U;
		}
		auto remote_ok(const std::optional<tour_remote_binding>& aValue) noexcept -> bool
		{
			return !aValue || (aValue->Kind <= tour_remote_kind::conversation_comment && aValue->Entity.size() <= 1024U && aValue->Actor.size() <= 1024U && aValue->Review.size() <= 1024U && aValue->WebUrl.size() <= maximum_tour_path_bytes);
		}
		auto stop_bytes(const tour_stop& aStop) noexcept -> std::size_t
		{
			auto size = (aStop.Destination ? 192U + aStop.Destination->Path.size() : 1U) + 64U + aStop.Title.size() + aStop.Body.size() + baseline_bytes(aStop.Baseline) + remote_bytes(aStop.Remote);
			if (aStop.Original) {
				size += source_bytes(*aStop.Original);
			}
			if (aStop.Mapped) {
				size += source_bytes(*aStop.Mapped);
			}
			for (const auto& related : aStop.Related) {
				size += source_bytes(related);
			}
			return size;
		}
		auto stop_refusal(const tour_stop& aStop) noexcept -> tour_refusal
		{
			if (!field_ok(aStop.Title) || !field_ok(aStop.Body) || !baseline_ok(aStop.Baseline) || !remote_ok(aStop.Remote) || aStop.Related.size() > maximum_tour_related_sources) {
				return tour_refusal::bounds;
			}
			if ((aStop.Destination && !valid(*aStop.Destination)) || (aStop.Original && !valid(*aStop.Original)) || (aStop.Mapped && !valid(*aStop.Mapped))
				|| !std::ranges::all_of(aStop.Related, [](const auto& aSource) { return valid(aSource); })) {
				return tour_refusal::invalid_source;
			}
			if (aStop.Mapping > tour_mapping::author_selected || aStop.Preparation > tour_preparation::needs_attention) {
				return tour_refusal::invalid_command;
			}
			if (aStop.Mapping == tour_mapping::exact_original && (!aStop.Original || aStop.Mapped)) {
				return tour_refusal::invalid_source;
			}
			if (aStop.Mapping == tour_mapping::unchanged_material && (!aStop.Original || !aStop.Mapped)) {
				return tour_refusal::invalid_source;
			}
			if (aStop.Preparation == tour_preparation::ready && !ready(aStop)) {
				return tour_refusal::incomplete;
			}
			return tour_refusal::none;
		}
		auto new_id(review_tour& aTour) -> tour_stop_id { return tour_stop_id{ ++aTour.LastStopId }; }
		auto utf8_boundary(std::string_view aText, std::size_t aPosition) -> bool
		{
			return aPosition <= aText.size() && (aPosition == aText.size() || (static_cast<unsigned char>(aText[aPosition]) & 0xc0U) != 0x80U);
		}
		auto insert_stop(review_tour& aTour, const tour_command& aCommand, tour_stop_id& aSelected) -> tour_refusal
		{
			if (aCommand.Position > aTour.Stops.size()) {
				return tour_refusal::invalid_command;
			}
			if (aTour.Stops.size() >= maximum_tour_stops || aTour.LastStopId == std::numeric_limits<std::uint64_t>::max()) {
				return tour_refusal::bounds;
			}
			auto value        = aCommand.Value;
			value.Id          = new_id(aTour);
			value.Preparation = tour_preparation::editing;
			aSelected         = value.Id;
			aTour.Stops.insert(aTour.Stops.begin() + static_cast<std::ptrdiff_t>(aCommand.Position), std::move(value));
			return tour_refusal::none;
		}
		auto duplicate_or_split(review_tour& aTour, const tour_command& aCommand, std::size_t aIndex, tour_stop_id& aSelected) -> tour_refusal
		{
			if (aTour.Stops.size() >= maximum_tour_stops || aTour.LastStopId >= std::numeric_limits<std::uint64_t>::max() - 1) {
				return tour_refusal::bounds;
			}
			auto copy = aTour.Stops[aIndex];
			copy.Id   = new_id(aTour);
			copy.Baseline.reset();
			copy.Remote.reset();
			copy.Preparation = tour_preparation::editing;
			if (aCommand.Kind == tour_command_kind::split) {
				if (!utf8_boundary(copy.Body, aCommand.SplitByte)) {
					return tour_refusal::invalid_command;
				}
				if (aTour.Stops[aIndex].Remote) {
					aTour.Retired.push_back(aTour.Stops[aIndex]);
					aTour.Stops[aIndex].Remote.reset();
					aTour.Stops[aIndex].Baseline.reset();
					aTour.Stops[aIndex].Id = new_id(aTour);
				}
				copy.Body = copy.Body.substr(aCommand.SplitByte);
				aTour.Stops[aIndex].Body.resize(aCommand.SplitByte);
				aTour.Stops[aIndex].Preparation = tour_preparation::editing;
			}
			aSelected = copy.Id;
			aTour.Stops.insert(aTour.Stops.begin() + static_cast<std::ptrdiff_t>(aIndex + 1U), std::move(copy));
			return tour_refusal::none;
		}
		void retain_source(tour_stop& aStop, const tour_source_range& aSource)
		{
			if (aStop.Original != aSource && std::ranges::find(aStop.Related, aSource) == aStop.Related.end()) {
				aStop.Related.push_back(aSource);
			}
		}
		auto merge_next(review_tour& aTour, std::size_t aIndex) -> tour_refusal
		{
			if (aIndex + 1U >= aTour.Stops.size()) {
				return tour_refusal::invalid_command;
			}
			auto&       stop = aTour.Stops[aIndex];
			const auto& next = aTour.Stops[aIndex + 1U];
			// Byte concatenation makes split followed by merge an exact inverse for prose.
			stop.Body += next.Body;
			if (next.Original) {
				retain_source(stop, *next.Original);
			}
			if (next.Mapped) {
				retain_source(stop, *next.Mapped);
			}
			for (const auto& related : next.Related) {
				retain_source(stop, related);
			}
			if (stop.Remote) {
				auto original = stop;
				original.Body.resize(original.Body.size() - next.Body.size());
				aTour.Retired.push_back(original);
			}
			if (stop.Remote || next.Remote) {
				if (aTour.LastStopId == std::numeric_limits<std::uint64_t>::max()) {
					return tour_refusal::bounds;
				}
				stop.Id = new_id(aTour);
				stop.Remote.reset();
				stop.Baseline.reset();
			}
			stop.Preparation = tour_preparation::editing;
			if (next.Remote) {
				aTour.Retired.push_back(next);
			}
			aTour.Stops.erase(aTour.Stops.begin() + static_cast<std::ptrdiff_t>(aIndex + 1U));
			return tour_refusal::none;
		}
		auto edit_stop(review_tour& aTour, const tour_command& aCommand, tour_stop_id& aSelected) -> tour_refusal
		{
			const auto found = std::ranges::find(aTour.Stops, aCommand.Stop, &tour_stop::Id);
			if (found == aTour.Stops.end()) {
				return tour_refusal::invalid_command;
			}
			const auto index = static_cast<std::size_t>(found - aTour.Stops.begin());
			aSelected        = found->Id;
			switch (aCommand.Kind) {
			case tour_command_kind::edit_stop:
				if (aCommand.Value.Id != found->Id || aCommand.Value.Original != found->Original || aCommand.Value.Baseline != found->Baseline || aCommand.Value.Remote != found->Remote || aCommand.Value.Destination != found->Destination || (found->Remote && !found->Remote->Own)) {
					return tour_refusal::identity;
				}
				*found             = aCommand.Value;
				found->Preparation = tour_preparation::editing;
				return tour_refusal::none;
			case tour_command_kind::move: {
				if (aCommand.Position >= aTour.Stops.size()) {
					return tour_refusal::invalid_command;
				}
				auto stop = std::move(*found);
				aTour.Stops.erase(found);
				aTour.Stops.insert(aTour.Stops.begin() + static_cast<std::ptrdiff_t>(aCommand.Position), std::move(stop));
				return tour_refusal::none;
			}
			case tour_command_kind::duplicate:
			case tour_command_kind::split:      return duplicate_or_split(aTour, aCommand, index, aSelected);
			case tour_command_kind::merge_next: return merge_next(aTour, index);
			case tour_command_kind::remove:
				if (found->Remote) {
					aTour.Retired.push_back(*found);
				}
				aTour.Stops.erase(found);
				aSelected = aTour.Stops.empty() ? tour_stop_id{ } : aTour.Stops[std::min(index, aTour.Stops.size() - 1U)].Id;
				return tour_refusal::none;
			case tour_command_kind::complete:
				if (!ready(*found)) {
					return tour_refusal::incomplete;
				}
				found->Preparation = tour_preparation::ready;
				return tour_refusal::none;
			default: return tour_refusal::invalid_command;
			}
		}
		auto mutate(review_tour& aTour, const tour_command& aCommand, tour_stop_id& aSelected) -> tour_refusal
		{
			switch (aCommand.Kind) {
			case tour_command_kind::problem:
			case tour_command_kind::solution: {
				auto& field    = aCommand.Kind == tour_command_kind::problem ? aTour.Problem : aTour.Solution;
				auto& baseline = aCommand.Kind == tour_command_kind::problem ? aTour.ProblemBaseline : aTour.SolutionBaseline;
				if (aCommand.Baseline) {
					const auto& observed = *aCommand.Baseline;
					if (!baseline_ok(observed) || !observed.SourceSpan) {
						return tour_refusal::invalid_command;
					}
					const auto [begin, end] = *observed.SourceSpan;
					if (!utf8_boundary(observed.Raw, begin) || !utf8_boundary(observed.Raw, end) || observed.Raw.substr(begin, end - begin) != aCommand.Text) {
						return tour_refusal::invalid_command;
					}
					baseline = observed;
				}
				field = aCommand.Text;
				return tour_refusal::none;
			}
			case tour_command_kind::insert: return insert_stop(aTour, aCommand, aSelected);
			case tour_command_kind::observe_head:
				if (!aCommand.Head.valid()) {
					return tour_refusal::invalid_source;
				}
				if (aCommand.Head != aTour.Head) {
					// Authored Head and source never change merely because a new head was observed.
					for (auto& stop : aTour.Stops) {
						stop.Preparation = tour_preparation::needs_attention;
						stop.Mapping     = tour_mapping::unavailable;
						stop.Mapped.reset();
					}
				}
				return tour_refusal::none;
			case tour_command_kind::restore: {
				if (!aCommand.Restore || aCommand.Restore->Owner != aTour.Owner || aCommand.Restore->Id != aTour.Id || aCommand.Restore->Head != aTour.Head || aCommand.Restore->Base != aTour.Base) {
					return tour_refusal::identity;
				}
				const auto revision  = aTour.Revision;
				const auto highWater = std::max(aTour.LastStopId, aCommand.Restore->LastStopId);
				aTour                = *aCommand.Restore;
				aTour.Revision       = revision;
				aTour.LastStopId     = highWater;
				return tour_refusal::none;
			}
			default: return edit_stop(aTour, aCommand, aSelected);
			}
		}
	}
	auto valid(const tour_code_destination& aSource) noexcept -> bool
	{
		if (!aSource.Commit.valid() || (aSource.Blob && (!aSource.Blob->valid() || aSource.Blob->algorithm() != aSource.Commit.algorithm()))
			|| aSource.Path.empty() || aSource.Path.size() > maximum_tour_path_bytes || aSource.Path.front() == '/'
			|| aSource.Side > diff_side::right || aSource.LastLine < aSource.FirstLine || aSource.LastLine == std::numeric_limits<std::uint32_t>::max()) {
			return false;
		}
		if (std::ranges::any_of(aSource.Path, [](unsigned char aByte) { return aByte < 0x20 || aByte == 0x7f || aByte == '\\'; })) {
			return false;
		}
		std::size_t at = 0;
		while (at <= aSource.Path.size()) {
			const auto end     = aSource.Path.find('/', at);
			const auto segment = std::string_view{ aSource.Path }.substr(at, end == std::string::npos ? end : end - at);
			if (segment.empty() || segment == "." || segment == "..") {
				return false;
			}
			if (end == std::string::npos) {
				break;
			}
			at = end + 1;
		}
		return true;
	}
	auto destination_of(const tour_source_range& aSource) -> tour_code_destination
	{
		return { aSource.Commit, aSource.Blob, aSource.Path, aSource.First.Side, aSource.First.Line, aSource.Last.Line };
	}

	auto valid(const tour_owner& aOwner) noexcept -> bool
	{
		return aOwner.Profile.value() && aOwner.Host.value() && aOwner.HostAuthority.valid() && aOwner.Account.value() && aOwner.Repository.value() && aOwner.PullRequest.value();
	}
	auto valid(const tour_source_range& aRange) noexcept -> bool
	{
		return aRange.First.Comparison.valid() && aRange.First.Comparison == aRange.Last.Comparison
			&& (aRange.First.Side == diff_side::left || aRange.First.Side == diff_side::right) && aRange.First.Side == aRange.Last.Side
			&& aRange.Last.Line < std::numeric_limits<std::uint32_t>::max() && aRange.Last.Line >= aRange.First.Line && aRange.Commit.valid()
			&& (!aRange.Blob || aRange.Blob->valid()) && !aRange.Path.empty() && aRange.Path.size() <= maximum_tour_path_bytes
			&& aRange.Path.front() != '/' && aRange.Path.find('\0') == std::string::npos;
	}
	auto retained_tour_bytes(const review_tour& aTour) noexcept -> std::size_t
	{
		auto size = 256U + aTour.Problem.size() + aTour.Solution.size() + baseline_bytes(aTour.ProblemBaseline) + baseline_bytes(aTour.SolutionBaseline) + baseline_bytes(aTour.IndexBaseline) + remote_bytes(aTour.Remote) + aTour.ImportNotice.size();
		for (const auto& stop : aTour.Stops) {
			size += stop_bytes(stop);
		}
		for (const auto& stop : aTour.Retired) {
			size += stop_bytes(stop);
		}
		return size;
	}
	auto ready(const tour_stop& aStop) noexcept -> bool
	{
		return nonblank(aStop.Title) && nonblank(aStop.Body) && ((aStop.Mapping == tour_mapping::author_selected && aStop.Mapped && valid(*aStop.Mapped)) || (aStop.Original && valid(*aStop.Original) && (aStop.Mapping == tour_mapping::exact_original || (aStop.Mapping == tour_mapping::unchanged_material && aStop.Mapped && valid(*aStop.Mapped)))));
	}
	auto validate(const review_tour& aTour) noexcept -> tour_refusal
	{
		if (!valid(aTour.Owner) || !aTour.Id.value() || !aTour.Revision) {
			return tour_refusal::identity;
		}
		if (!field_ok(aTour.Problem) || !field_ok(aTour.Solution) || !baseline_ok(aTour.ProblemBaseline) || !baseline_ok(aTour.SolutionBaseline)
			|| !baseline_ok(aTour.IndexBaseline) || !remote_ok(aTour.Remote) || !field_ok(aTour.ImportNotice)
			|| aTour.Retired.size() > maximum_tour_stops || aTour.Stops.size() > maximum_tour_stops || retained_tour_bytes(aTour) > maximum_tour_bytes) {
			return tour_refusal::bounds;
		}
		for (std::size_t index = 0; index < aTour.Stops.size(); ++index) {
			const auto& stop = aTour.Stops[index];
			if (!stop.Id.value() || stop.Id.value() > aTour.LastStopId || std::find_if(aTour.Stops.begin(), aTour.Stops.begin() + static_cast<std::ptrdiff_t>(index), [&](const auto& aOther) { return aOther.Id == stop.Id; }) != aTour.Stops.begin() + static_cast<std::ptrdiff_t>(index)) {
				return tour_refusal::identity;
			}
			if (const auto refusal = stop_refusal(stop); refusal != tour_refusal::none) {
				return refusal;
			}
		}
		for (const auto& stop : aTour.Retired) {
			if (!stop.Remote || stop_refusal(stop) != tour_refusal::none) {
				return tour_refusal::invalid_command;
			}
		}
		return tour_refusal::none;
	}
	auto apply(const review_tour& aTour, const tour_command& aCommand) -> tour_outcome
	{
		auto result = tour_outcome{ .Accepted = aTour };
		if (aCommand.Draft != aTour.Id || aCommand.Owner != aTour.Owner) {
			result.Refusal = tour_refusal::identity;
			return result;
		}
		if (aCommand.ExpectedRevision != aTour.Revision) {
			result.Refusal = tour_refusal::conflict;
			return result;
		}
		if (aTour.Revision == std::numeric_limits<std::uint64_t>::max()) {
			result.Refusal = tour_refusal::exhausted;
			return result;
		}
		if (const auto refusal = validate(aTour); refusal != tour_refusal::none) {
			result.Refusal = refusal;
			return result;
		}
		if (aTour.ReadOnly && aCommand.Kind != tour_command_kind::observe_head) {
			result.Refusal = tour_refusal::incomplete;
			return result;
		}
		auto candidate = aTour;
		result.Refusal = mutate(candidate, aCommand, result.Selected);
		if (result.Refusal == tour_refusal::none) {
			result.Refusal = validate(candidate);
		}
		if (result.Refusal == tour_refusal::none && candidate != aTour) {
			++candidate.Revision;
			result.Accepted = std::move(candidate);
			result.Changed  = true;
		}
		return result;
	}
}
