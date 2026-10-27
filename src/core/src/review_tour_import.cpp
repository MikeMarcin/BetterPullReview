#include "bpr/core/review_tour_import.hpp"
#include <algorithm>
#include <unordered_set>

namespace bpr::core {
	namespace {
		auto baseline(std::string_view aRaw, std::string_view aEntity, std::string_view aVersion, std::optional<tour_text_span> aSpan) -> tour_field_baseline
		{
			return { std::string{ aRaw }, std::string{ aEntity }, std::string{ aVersion }, aSpan ? std::optional{ std::pair{ aSpan->Begin, aSpan->End } } : std::nullopt, 0 };
		}
		// [[BPR-1787]]. The brief is read from the description first, where a
		// tour publishes it by default, then from the tour's own comment, where
		// tours published before BPR-1787 (or with the description left alone)
		// carry it.
		void map_brief(review_tour& aDraft, std::string_view aRaw, std::string_view aEntity, const tour_remote_object& aRoot)
		{
			const auto mapping = map_tour_markdown(aRaw);
			const auto comment = map_tour_markdown(aRoot.Raw);
			for (const auto name : { "Problem", "Solution" }) {
				const auto exact           = [&](const tour_markdown_map& aMap) { return std::ranges::count(aMap.Sections, std::string{ name }, &tour_markdown_section::Name) == 1; };
				const bool fromDescription = exact(mapping);
				if (!fromDescription && !exact(comment)) {
					aDraft.ImportNotice += std::string{ name } + " requires source mapping. ";
					continue;
				}
				const auto& source   = fromDescription ? mapping : comment;
				const auto  raw      = fromDescription ? aRaw : std::string_view{ aRoot.Raw };
				const auto  span     = std::ranges::find(source.Sections, std::string{ name }, &tour_markdown_section::Name)->Content;
				auto&       field    = std::string_view{ name } == "Problem" ? aDraft.Problem : aDraft.Solution;
				auto&       original = std::string_view{ name } == "Problem" ? aDraft.ProblemBaseline : aDraft.SolutionBaseline;
				field                = raw.substr(span.Begin, span.End - span.Begin);
				original             = fromDescription ? baseline(aRaw, aEntity, { }, span) : baseline(aRoot.Raw, aRoot.Binding.Entity, aRoot.Version, span);
			}
		}
		void add_entries(review_tour& aDraft, const tour_markdown_list& aList, std::span<const tour_remote_object> aObjects)
		{
			std::unordered_set<std::string> seen;
			for (const auto& entry : aList.Entries) {
				tour_stop  stop{ .Id = tour_stop_id{ ++aDraft.LastStopId }, .Title = entry.Label };
				const auto named   = [identity = tour_link_identity(entry.Destination)](const auto& aObject) { return aObject.ValidDestination && aObject.Binding.WebUrl == identity; };
				const auto object  = std::ranges::find_if(aObjects, named);
				const auto matches = std::ranges::count_if(aObjects, named);
				if (object == aObjects.end() || matches != 1 || !object->Available || object->Binding.Entity == aDraft.Remote->Entity) {
					stop.Body       = "Unresolved tour entry: " + entry.Destination;
					aDraft.ReadOnly = true;
					aDraft.ImportNotice += "A missing, ambiguous, foreign or self link requires resolution. ";
				} else {
					stop.Remote              = object->Binding;
					const auto mapping       = map_tour_stop(object->Raw);
					stop.Baseline            = baseline(object->Raw, object->Binding.Entity, object->Version, mapping.Narrative);
					stop.Body                = mapping.Narrative ? object->Raw.substr(mapping.Narrative->Begin, mapping.Narrative->End - mapping.Narrative->Begin) : object->Raw;
					stop.Baseline->DraftText = stop.Body;
					stop.Destination         = object->Source;
					stop.Mapping             = tour_mapping::unavailable;
					if (!mapping.Narrative) {
						aDraft.ImportNotice += "Custom stop layout is retained as a whole comment. ";
					}
					if (!stop.Remote->Own || stop.Remote->Actor != aDraft.Remote->Actor) {
						aDraft.ReadOnly = true;
						aDraft.ImportNotice += "Third-party or mixed authorship: read-only. ";
					}
					if (!seen.insert(stop.Remote->Entity).second) {
						aDraft.ReadOnly = true;
						aDraft.ImportNotice += "Duplicate comment identity: separate route entries retained. ";
					}
				}
				aDraft.Stops.push_back(std::move(stop));
			}
		}
	}
	auto discover_tours(const review_tour& aSeed, std::string_view aDescription, std::string_view aDescriptionEntity,
						std::span<const tour_remote_object> aObjects, bool aComplete) -> std::vector<tour_import_candidate>
	{
		std::vector<tour_import_candidate> result;
		if (validate(aSeed) != tour_refusal::none || aDescription.size() > maximum_tour_field_bytes || aObjects.size() > 4096) {
			return result;
		}
		const auto brief = map_tour_markdown(aDescription);
		for (const auto& root : aObjects) {
			if (!root.Root || !root.Available || !root.ValidDestination) {
				continue;
			}
			const auto mapping = map_tour_markdown(root.Raw);
			for (std::size_t index = 0; index < mapping.Lists.size(); ++index) {
				const auto& list = mapping.Lists[index];
				if (list.Entries.empty() || list.Entries.size() > maximum_tour_stops || result.size() == 64) {
					continue;
				}
				auto draft = aSeed;
				draft.Stops.clear();
				draft.LastStopId    = 0;
				draft.Remote        = root.Binding;
				draft.IndexBaseline = baseline(root.Raw, root.Binding.Entity, root.Version, list.Source);
				draft.ReadOnly      = !aComplete || list.MappingNeeded || !root.Binding.Own;
				if (!aComplete) {
					draft.ImportNotice += "Partial read: absence is not deletion. ";
				}
				if (list.MappingNeeded) {
					draft.ImportNotice += "Index list needs mapping. ";
				}
				if (!root.Binding.Own) {
					draft.ImportNotice += "Index belongs to another author: read-only. ";
				}
				map_brief(draft, aDescription, aDescriptionEntity, root);
				add_entries(draft, list, aObjects);
				if (validate(draft) != tour_refusal::none) {
					continue;
				}
				const bool linked = std::ranges::any_of(brief.Links, [&](const auto& aLink) { return aLink.Destination == root.Binding.WebUrl; });
				result.push_back({ root.Binding.Actor + " · " + root.Binding.Entity + " · list " + std::to_string(index + 1), std::move(draft), linked });
			}
		}
		std::stable_sort(result.begin(), result.end(), [](const auto& aLeft, const auto& aRight) { return aLeft.Preferred && !aRight.Preferred; });
		return result;
	}
}
