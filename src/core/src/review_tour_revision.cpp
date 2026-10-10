#include "bpr/core/review_tour_revision.hpp"
#include "bpr/core/review_tour_markdown.hpp"
#include <algorithm>
namespace bpr::core {
	namespace {
		auto source_of(const tour_stop& aStop) -> std::optional<tour_code_destination>
		{
			auto source = aStop.Original ? std::optional{ destination_of(*aStop.Original) } : aStop.Destination;
			if (source) {
				source->Blob.reset();
			}
			return source;
		}
		auto route(const review_tour& aTour) -> std::vector<std::string>
		{
			if (!aTour.IndexBaseline) {
				return { };
			}
			const auto map = map_tour_markdown(aTour.IndexBaseline->Raw);
			for (const auto& list : map.Lists) {
				if ((aTour.IndexBaseline->SourceSpan && list.Source == tour_text_span{ aTour.IndexBaseline->SourceSpan->first, aTour.IndexBaseline->SourceSpan->second }) || (!aTour.IndexBaseline->SourceSpan && map.Lists.size() == 1)) {
					std::vector<std::string> result;
					result.reserve(list.Entries.size());
					for (const auto& entry : list.Entries) {
						result.push_back(tour_link_identity(entry.Destination));
					}
					return result;
				}
			}
			return { };
		}
		auto counterpart(const diff_semantic_index& aIndex, source_line_ref aLine) -> line_mapping
		{
			auto mapped = map_line(aIndex, aLine);
			if (!mapped.Counterpart) {
				if (const auto move = move_for_line(aIndex, aLine)) {
					mapped.Counterpart = move->Counterpart;
					mapped.Kind        = line_mapping_kind::exact_line;
				}
			}
			return mapped;
		}
	}
	auto map_tour_revision(const review_tour& aTour, const diff_semantic_index& aIndex, const tour_code_destination& aLeft, const tour_code_destination& aRight) -> review_tour
	{
		auto result = aTour;
		if (!aTour.ObservedHead || !valid(aLeft) || !valid(aRight) || aLeft.Side != diff_side::left || aRight.Side != diff_side::right || aIndex.diff().Alignment.Key.LeftBlob != aLeft.Blob || aIndex.diff().Alignment.Key.RightBlob != aRight.Blob) {
			return result;
		}
		const auto& authority = aIndex.diff().Alignment.Key.Authority;
		if (authority.Host != aTour.Owner.Host || authority.HostAuthority != aTour.Owner.HostAuthority || authority.Account != aTour.Owner.Account || authority.Repository != aTour.Owner.Repository || authority.PullRequest != aTour.Owner.PullRequest) {
			return result;
		}
		for (auto& stop : result.Stops) {
			if (stop.Mapping == tour_mapping::author_selected) {
				continue;
			}
			const auto original = stop.Original ? std::optional{ destination_of(*stop.Original) } : stop.Destination;
			if (!original) {
				continue;
			}
			const auto  matches = [&](const auto& aEndpoint) { return original->Commit == aEndpoint.Commit && original->Path == aEndpoint.Path && (!original->Blob || original->Blob == aEndpoint.Blob); };
			const auto* from    = matches(aLeft) ? &aLeft : matches(aRight) ? &aRight
																			: nullptr;
			if (!from) {
				continue;
			}
			const auto& to = from == &aLeft ? aRight : aLeft;
			if (to.Commit != *aTour.ObservedHead || to.Commit == original->Commit) {
				continue;
			}
			stop.Mapped.reset();
			stop.Mapping = tour_mapping::unavailable;
			if (!to.Blob || !from->Blob || !aIndex.diff().LeftDocument || !aIndex.diff().RightDocument) {
				continue;
			}
			if (aIndex.diff().AlignmentApproximate) {
				stop.Mapping = tour_mapping::ambiguous;
				continue;
			}
			const auto                     digest = aIndex.diff().Alignment.Digest;
			std::optional<source_line_ref> first, last;
			bool                           changed = false, failed = false;
			const auto                     count = static_cast<std::uint64_t>(original->LastLine) - original->FirstLine + 1;
			if (count > 200000) {
				continue;
			}
			for (std::uint64_t n = 0; n < count; ++n) {
				const auto mapped = counterpart(aIndex, { digest, from->Side, static_cast<std::uint32_t>(original->FirstLine + n) });
				if (!mapped.Counterpart) {
					stop.Mapping = mapped.OpposingSpan && !mapped.OpposingSpan->Lines.empty() ? tour_mapping::ambiguous : tour_mapping::missing;
					failed       = true;
					break;
				}
				if (last && (mapped.Counterpart->Side != last->Side || mapped.Counterpart->Line != last->Line + 1)) {
					stop.Mapping = tour_mapping::ambiguous;
					failed       = true;
					break;
				}
				if (!first) {
					first = mapped.Counterpart;
				}
				last    = mapped.Counterpart;
				changed = changed || mapped.Kind != line_mapping_kind::exact_line;
			}
			if (!failed && first && last) {
				stop.Mapped  = tour_source_range{ *first, *last, to.Commit, to.Blob, to.Path };
				stop.Mapping = changed ? tour_mapping::changed_material : tour_mapping::unchanged_material;
			}
			// Even exact selected bytes do not prove an explanation about another file or contract.
			if (stop.ReaffirmedHead != aTour.ObservedHead) {
				stop.Preparation = tour_preparation::needs_attention;
			}
		}
		return validate(result) == tour_refusal::none ? result : aTour;
	}
	auto observed_tour_changes(const review_tour& aBefore, const review_tour& aAfter) -> std::vector<tour_observed_change>
	{
		std::vector<tour_observed_change> result;
		if (aBefore.Owner != aAfter.Owner || !aBefore.Remote || !aAfter.Remote || aBefore.Remote->Entity != aAfter.Remote->Entity || !aBefore.IndexBaseline || !aAfter.IndexBaseline) {
			return result;
		}
		const auto oldRoute = route(aBefore), newRoute = route(aAfter);
		auto       previous = aBefore.Stops;
		previous.insert(previous.end(), aBefore.Retired.begin(), aBefore.Retired.end());
		auto nextId = aBefore.LastStopId;
		for (const auto& stop : aAfter.Stops) {
			if (!stop.Remote) {
				continue;
			}
			const auto old = std::ranges::find_if(previous, [&](const auto& aValue) { return aValue.Remote && aValue.Remote->Entity == stop.Remote->Entity; });
			if (old == previous.end() || std::ranges::find(oldRoute, stop.Remote->WebUrl) == oldRoute.end()) {
				result.push_back({ tour_change_kind::added, tour_stop_id{ ++nextId }, stop.Title });
				continue;
			}
			if (std::ranges::find(oldRoute, stop.Remote->WebUrl) - oldRoute.begin() != std::ranges::find(newRoute, stop.Remote->WebUrl) - newRoute.begin()) {
				result.push_back({ tour_change_kind::reordered, old->Id, stop.Title });
			}
			if (old->Baseline && stop.Baseline && old->Baseline->Raw != stop.Baseline->Raw) {
				result.push_back({ tour_change_kind::text, old->Id, stop.Title });
			}
			if (source_of(*old) != source_of(stop)) {
				result.push_back({ tour_change_kind::source, old->Id, stop.Title });
			}
		}
		for (const auto& stop : previous) {
			if (stop.Remote && std::ranges::find(oldRoute, stop.Remote->WebUrl) != oldRoute.end() && std::ranges::find(newRoute, stop.Remote->WebUrl) == newRoute.end()) {
				result.push_back({ tour_change_kind::removed, stop.Id, stop.Title });
			}
		}
		for (const auto problem : { true, false }) {
			const auto& old = problem ? aBefore.ProblemBaseline : aBefore.SolutionBaseline;
			const auto& now = problem ? aAfter.ProblemBaseline : aAfter.SolutionBaseline;
			// A local manual mapping can differ from the importer's heading span.
			// Identical remote bytes are never a website edit.
			if (old && now && old->Raw != now->Raw && old->SourceSpan && now->SourceSpan && old->Raw.substr(old->SourceSpan->first, old->SourceSpan->second - old->SourceSpan->first) != now->Raw.substr(now->SourceSpan->first, now->SourceSpan->second - now->SourceSpan->first)) {
				result.push_back({ tour_change_kind::text, { }, problem ? "Problem" : "Solution" });
			}
		}
		return result;
	}
}
