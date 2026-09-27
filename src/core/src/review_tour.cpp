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
			return aValue ? 64U + aValue->Raw.size() + aValue->RemoteEntity.size() + aValue->ObservedVersion.size() + (aValue->DraftText ? aValue->DraftText->size() : 0U) : 1U;
		}
		auto baseline_ok(const std::optional<tour_field_baseline>& aValue) noexcept -> bool
		{
			return !aValue || (field_ok(aValue->Raw) && (!aValue->DraftText || field_ok(*aValue->DraftText)) && aValue->RemoteEntity.size() <= 1024U && aValue->ObservedVersion.size() <= 1024U && (!aValue->SourceSpan || (aValue->SourceSpan->first <= aValue->SourceSpan->second && aValue->SourceSpan->second <= aValue->Raw.size())));
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
			auto size = (aStop.Suggestion ? 32U + aStop.Suggestion->AnchorRef.size() + aStop.Suggestion->Reason.size() + aStop.Suggestion->Question.size() : 1U) + (aStop.Destination ? 192U + aStop.Destination->Path.size() : 1U) + 64U + aStop.Title.size() + aStop.Body.size() + baseline_bytes(aStop.Baseline) + remote_bytes(aStop.Remote);
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
			if (aStop.Suggestion && (!field_ok(aStop.Suggestion->AnchorRef) || !field_ok(aStop.Suggestion->Reason) || !field_ok(aStop.Suggestion->Question) || aStop.Suggestion->AnchorRef.empty() || aStop.Suggestion->AnchorRef.size() > 128 || aStop.Suggestion->Reason.empty() || aStop.Suggestion->Reason.size() > 240 || aStop.Suggestion->Question.size() > 240)) {
				return tour_refusal::bounds;
			}
			if (aStop.Suggestion && aStop.Preparation == tour_preparation::ready && !aStop.Suggestion->Visited && aStop.CompletionOrigin != tour_completion_origin::relayed) {
				return tour_refusal::incomplete;
			}
			if (aStop.CompletionOrigin > tour_completion_origin::relayed || !field_ok(aStop.Title) || !field_ok(aStop.Body) || !baseline_ok(aStop.Baseline) || !remote_ok(aStop.Remote) || aStop.Related.size() > maximum_tour_related_sources) {
				return tour_refusal::bounds;
			}
			if ((aStop.Destination && !valid(*aStop.Destination)) || (aStop.Original && !valid(*aStop.Original)) || (aStop.Mapped && !valid(*aStop.Mapped))
				|| !std::ranges::all_of(aStop.Related, [](const auto& aSource) { return valid(aSource); })) {
				return tour_refusal::invalid_source;
			}
			if (aStop.ReaffirmedHead && !aStop.ReaffirmedHead->valid()) {
				return tour_refusal::invalid_source;
			}
			if (aStop.Mapping > tour_mapping::author_selected || aStop.Preparation > tour_preparation::needs_attention) {
				return tour_refusal::invalid_command;
			}
			if (aStop.Mapping == tour_mapping::exact_original && (!aStop.Original || aStop.Mapped)) {
				return tour_refusal::invalid_source;
			}
			if (aStop.Mapping == tour_mapping::unchanged_material && ((!aStop.Original && !aStop.Destination) || !aStop.Mapped)) {
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
		auto insert_outline(review_tour& aTour, const tour_command& aCommand, tour_stop_id& aSelected) -> tour_refusal
		{
			if (aCommand.Outline.empty() || aCommand.Outline.size() > 24 || aCommand.Position > aTour.Stops.size()
				|| aCommand.Outline.size() > maximum_tour_stops - aTour.Stops.size()
				|| aCommand.Outline.size() > std::numeric_limits<std::uint64_t>::max() - aTour.LastStopId) {
				return tour_refusal::bounds;
			}
			auto position = aCommand.Position;
			for (const auto& entry : aCommand.Outline) {
				tour_stop stop{ .Id = new_id(aTour), .Title = entry.Title, .Original = entry.Source, .Mapping = entry.Source ? tour_mapping::exact_original : tour_mapping::unavailable, .Related = entry.Related, .Preparation = tour_preparation::suggested, .Suggestion = entry.Suggestion };
				stop.Suggestion->Visited = false;
				if (!aSelected.value()) {
					aSelected = stop.Id;
				}
				aTour.Stops.insert(aTour.Stops.begin() + static_cast<std::ptrdiff_t>(position++), std::move(stop));
			}
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
			if (copy.Suggestion) {
				copy.Suggestion->Visited = false;
			}
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
			auto&       stop     = aTour.Stops[aIndex];
			const auto& next     = aTour.Stops[aIndex + 1U];
			const auto  original = stop.Remote ? std::optional{ stop } : std::nullopt;
			if (!stop.Suggestion && next.Suggestion) {
				stop.Suggestion = next.Suggestion;
			}
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
			if (original) {
				aTour.Retired.push_back(*original);
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
				if (aCommand.Value.Id != found->Id || aCommand.Value.Original != found->Original || aCommand.Value.Baseline != found->Baseline || aCommand.Value.Remote != found->Remote || aCommand.Value.Destination != found->Destination || aCommand.Value.Suggestion != found->Suggestion || (found->Remote && !found->Remote->Own)) {
					return tour_refusal::identity;
				}
				*found                  = aCommand.Value;
				found->Preparation      = tour_preparation::editing;
				found->CompletionOrigin = tour_completion_origin::unspecified;
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
			case tour_command_kind::merge_next: {
				const auto refusal = merge_next(aTour, index);
				aSelected          = aTour.Stops[index].Id;
				return refusal;
			}
			case tour_command_kind::remove:
				if (found->Remote) {
					aTour.Retired.push_back(*found);
				}
				aTour.Stops.erase(found);
				aSelected = aTour.Stops.empty() ? tour_stop_id{ } : aTour.Stops[std::min(index, aTour.Stops.size() - 1U)].Id;
				return tour_refusal::none;
			case tour_command_kind::reaffirm:
				if (!nonblank(found->Title) || !nonblank(found->Body) || (!found->Original && !(found->Remote && found->Destination))) {
					return tour_refusal::incomplete;
				}
				found->ReaffirmedHead = aTour.ObservedHead.value_or(aTour.Head);
				found->Preparation    = tour_preparation::editing;
				return tour_refusal::none;
			case tour_command_kind::visit:
				if (!visible_tour_stop(*found, aCommand.Attention, false)) {
					return tour_refusal::incomplete;
				}
				if (found->Suggestion) {
					found->Suggestion->Visited = true;
					if (found->Preparation == tour_preparation::suggested) {
						found->Preparation = tour_preparation::editing;
					}
				}
				return tour_refusal::none;
			case tour_command_kind::complete:
				if (aCommand.CompletionOrigin > tour_completion_origin::relayed || !ready(*found) || (found->Suggestion && aCommand.CompletionOrigin != tour_completion_origin::relayed && !visible_tour_stop(*found, aCommand.Attention, true))) {
					return tour_refusal::incomplete;
				}
				if (found->Suggestion && aCommand.CompletionOrigin != tour_completion_origin::relayed) {
					found->Suggestion->Visited = true;
				}
				found->CompletionOrigin = aCommand.CompletionOrigin == tour_completion_origin::unspecified && aCommand.Attention.EditorVisible ? tour_completion_origin::observed : aCommand.CompletionOrigin;
				found->Preparation      = tour_preparation::ready;
				return tour_refusal::none;
			default: return tour_refusal::invalid_command;
			}
		}
		auto selection_positions(const review_tour& aTour, const std::vector<tour_stop_id>& aSelection) -> std::optional<std::vector<std::size_t>>
		{
			std::vector<std::size_t> positions;
			if (aSelection.size() > maximum_tour_stops) {
				return std::nullopt;
			}
			for (const auto id : aSelection) {
				const auto found    = std::ranges::find(aTour.Stops, id, &tour_stop::Id);
				const auto position = static_cast<std::size_t>(found - aTour.Stops.begin());
				if (found == aTour.Stops.end() || (!positions.empty() && position <= positions.back())) {
					return std::nullopt;
				}
				positions.push_back(position);
			}
			return positions;
		}
		auto move_selection(review_tour& aTour, const tour_command& aCommand, const std::vector<std::size_t>& aPositions, tour_stop_id& aSelected) -> tour_refusal
		{
			if (aCommand.Position > aTour.Stops.size() - aPositions.size()) {
				return tour_refusal::invalid_command;
			}
			std::vector<tour_stop> moved;
			moved.reserve(aPositions.size());
			for (const auto position : aPositions) {
				moved.push_back(aTour.Stops[position]);
			}
			for (auto position = aPositions.rbegin(); position != aPositions.rend(); ++position) {
				aTour.Stops.erase(aTour.Stops.begin() + static_cast<std::ptrdiff_t>(*position));
			}
			aTour.Stops.insert(aTour.Stops.begin() + static_cast<std::ptrdiff_t>(aCommand.Position), moved.begin(), moved.end());
			aSelected = std::ranges::find(aCommand.Selection, aCommand.Stop) != aCommand.Selection.end() ? aCommand.Stop : moved.front().Id;
			return tour_refusal::none;
		}
		auto mutate_selection(review_tour& aTour, const tour_command& aCommand, tour_stop_id& aSelected) -> tour_refusal
		{
			const auto positions = selection_positions(aTour, aCommand.Selection);
			if (!positions || positions->empty()) {
				return tour_refusal::invalid_command;
			}
			if (aCommand.Kind == tour_command_kind::move) {
				return move_selection(aTour, aCommand, *positions, aSelected);
			}
			if (aCommand.Kind == tour_command_kind::merge_next) {
				if (positions->size() < 2 || positions->back() - positions->front() + 1 != positions->size()) {
					return tour_refusal::invalid_command;
				}
				for (std::size_t remaining = positions->size() - 1; remaining; --remaining) {
					if (const auto refusal = merge_next(aTour, positions->front()); refusal != tour_refusal::none) {
						return refusal;
					}
				}
				aSelected = aTour.Stops[positions->front()].Id;
				return tour_refusal::none;
			}
			if (aCommand.Kind != tour_command_kind::duplicate && aCommand.Kind != tour_command_kind::remove) {
				return tour_refusal::invalid_command;
			}
			for (auto id = aCommand.Selection.rbegin(); id != aCommand.Selection.rend(); ++id) {
				const tour_command single{ .Kind = aCommand.Kind, .Stop = *id };
				if (const auto refusal = edit_stop(aTour, single, aSelected); refusal != tour_refusal::none) {
					return refusal;
				}
			}
			return tour_refusal::none;
		}
		auto mutate(review_tour& aTour, const tour_command& aCommand, tour_stop_id& aSelected) -> tour_refusal
		{
			if (!aCommand.Selection.empty()) {
				return mutate_selection(aTour, aCommand, aSelected);
			}
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
			case tour_command_kind::insert:         return insert_stop(aTour, aCommand, aSelected);
			case tour_command_kind::insert_outline: return insert_outline(aTour, aCommand, aSelected);
			case tour_command_kind::observe_head:
				if (!aCommand.Head.valid()) {
					return tour_refusal::invalid_source;
				}
				if (aCommand.Head != aTour.ObservedHead.value_or(aTour.Head)) {
					aTour.ObservedHead = aCommand.Head;
					// Authored Head and source never change merely because a new head was observed.
					for (auto& stop : aTour.Stops) {
						stop.ReaffirmedHead.reset();
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
		for (const auto& stop : aTour.IncomingRoute) {
			size += stop_bytes(stop);
		}
		for (const auto& conflict : aTour.Conflicts) {
			size += 64 + conflict.Baseline.size() + conflict.Local.size() + conflict.Remote.size() + baseline_bytes(conflict.Observation);
		}
		for (const auto& change : aTour.ObservedChanges) {
			size += 64 + change.Title.size();
		}
		return size;
	}
	auto visible_tour_stop(const tour_stop& aStop, const tour_attention& aAttention, bool aRequireSource) noexcept -> bool
	{
		if (!aAttention.EditorVisible) {
			return false;
		}
		if (!aAttention.Source) {
			return !aRequireSource && aAttention.SourceUnavailable;
		}
		const auto& expected = aStop.Mapped ? aStop.Mapped : aStop.Original;
		return expected && valid(*aAttention.Source) && expected->Commit == aAttention.Source->Commit && expected->Blob == aAttention.Source->Blob
			&& expected->Path == aAttention.Source->Path && expected->First.Side == aAttention.Source->First.Side
			&& expected->First.Line == aAttention.Source->First.Line && expected->Last.Line == aAttention.Source->Last.Line;
	}
	auto ready(const tour_stop& aStop) noexcept -> bool
	{
		const bool original   = aStop.Original && valid(*aStop.Original);
		const bool proposed   = aStop.Mapped && valid(*aStop.Mapped);
		const bool reaffirmed = aStop.ReaffirmedHead && (original || (aStop.Remote && aStop.Destination && valid(*aStop.Destination)));
		const bool mapped     = aStop.Mapping == tour_mapping::author_selected && proposed;
		const bool retained   = original && (aStop.Mapping == tour_mapping::exact_original || (aStop.Mapping == tour_mapping::unchanged_material && proposed));
		return nonblank(aStop.Title) && nonblank(aStop.Body) && (reaffirmed || mapped || retained);
	}
	auto validate(const review_tour& aTour) noexcept -> tour_refusal
	{
		if (!valid(aTour.Owner) || !aTour.Id.value() || !aTour.Revision) {
			return tour_refusal::identity;
		}
		if (!field_ok(aTour.Problem) || !field_ok(aTour.Solution) || !baseline_ok(aTour.ProblemBaseline) || !baseline_ok(aTour.SolutionBaseline)
			|| !baseline_ok(aTour.IndexBaseline) || !remote_ok(aTour.Remote) || !field_ok(aTour.ImportNotice)
			|| (aTour.ObservedHead && !aTour.ObservedHead->valid()) || aTour.ObservedChanges.size() > maximum_tour_stops * 4 + 2 || aTour.IncomingRoute.size() > maximum_tour_stops || aTour.Conflicts.size() > maximum_tour_stops + 3 || aTour.Retired.size() > maximum_tour_stops || aTour.Stops.size() > maximum_tour_stops || retained_tour_bytes(aTour) > maximum_tour_bytes) {
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
		for (const auto& stop : aTour.IncomingRoute) {
			if (stop_refusal(stop) != tour_refusal::none) {
				return tour_refusal::invalid_command;
			}
		}
		for (const auto& change : aTour.ObservedChanges) {
			if (change.Kind > tour_change_kind::source || !field_ok(change.Title)) {
				return tour_refusal::bounds;
			}
		}
		for (const auto& conflict : aTour.Conflicts) {
			if (conflict.Field > tour_conflict_field::route || !field_ok(conflict.Baseline) || !field_ok(conflict.Local) || !field_ok(conflict.Remote) || !baseline_ok(conflict.Observation)) {
				return tour_refusal::bounds;
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
