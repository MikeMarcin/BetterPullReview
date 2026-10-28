#include "bpr/core/review_tour_update.hpp"
#include <algorithm>
#include <charconv>
namespace bpr::core {
	namespace {
		auto newline(std::string_view aRaw) -> std::string_view { return aRaw.find("\r\n") == std::string_view::npos ? "\n" : "\r\n"; }
		auto span_text(std::string_view aRaw, tour_text_span aSpan) -> std::string { return std::string{ aRaw.substr(aSpan.Begin, aSpan.End - aSpan.Begin) }; }
		auto trim(std::string_view aText) -> std::string_view
		{
			const auto begin = aText.find_first_not_of(" \t\r\n");
			if (begin == std::string_view::npos) {
				return { };
			}
			return aText.substr(begin, aText.find_last_not_of(" \t\r\n") - begin + 1);
		}
		void append_edit(std::vector<tour_text_edit>& aEdits, std::string_view aRaw, tour_text_span aSpan, std::string aDesired)
		{
			const auto before = span_text(aRaw, aSpan);
			if (before != aDesired) {
				aEdits.push_back({ aSpan, before, std::move(aDesired) });
			}
		}
		auto patch_stop(std::string_view aRaw, std::string_view aTitle, std::string_view aNarrative, std::size_t aOrdinal, std::span<const tour_markdown_link> aNavigation, bool aPreserveNavigation = false) -> std::optional<std::string>
		{
			const auto mapped   = map_tour_stop(aRaw);
			const auto rendered = render_tour_stop(aTitle, aOrdinal, aNarrative, aNavigation, { }, newline(aRaw));
			if (!rendered || !mapped.Heading || !mapped.Narrative || !mapped.Footer) {
				return { };
			}
			const auto generated = map_tour_stop(*rendered);
			if (!generated.Heading || !generated.Narrative || !generated.Footer) {
				return { };
			}
			const bool                  narrative_changed = trim(span_text(aRaw, *mapped.Narrative)) != trim(aNarrative);
			std::vector<tour_text_edit> edits;
			if (!aPreserveNavigation || mapped.Title != tour_markdown_label(aTitle) || narrative_changed) {
				append_edit(edits, aRaw, *mapped.Heading, span_text(*rendered, *generated.Heading));
			}
			if (narrative_changed) {
				append_edit(edits, aRaw, *mapped.Narrative, span_text(*rendered, *generated.Narrative));
			}
			if (!aPreserveNavigation) {
				append_edit(edits, aRaw, *mapped.Footer, span_text(*rendered, *generated.Footer));
			}
			return edit_tour_markdown(aRaw, edits);
		}
		auto unchanged_position(const review_tour& aTour, std::size_t aIndex) -> bool
		{
			const auto                map    = map_tour_markdown(aTour.IndexBaseline->Raw);
			const tour_markdown_list* before = nullptr;
			for (const auto& list : map.Lists) {
				if ((aTour.IndexBaseline->SourceSpan && list.Source == tour_text_span{ aTour.IndexBaseline->SourceSpan->first, aTour.IndexBaseline->SourceSpan->second }) || (!aTour.IndexBaseline->SourceSpan && map.Lists.size() == 1)) {
					before = &list;
				}
			}
			if (!before || aIndex >= before->Entries.size()) {
				return false;
			}
			for (const auto i : { aIndex ? aIndex - 1 : aIndex, aIndex, aIndex + 1 }) {
				if (i >= aTour.Stops.size() || i >= before->Entries.size()) {
					if ((i < aTour.Stops.size()) != (i < before->Entries.size())) {
						return false;
					}
					continue;
				}
				if (!aTour.Stops[i].Remote || needs_tour_destination(aTour.Stops[i]) || aTour.Stops[i].Remote->WebUrl != tour_link_identity(before->Entries[i].Destination)) {
					return false;
				}
			}
			return true;
		}

		auto root_body(const review_tour& aTour, std::span<const tour_markdown_link> aEntries) -> std::optional<std::string>
		{
			const auto& raw = aTour.IndexBaseline->Raw;
			const auto  map = map_tour_markdown(raw);
			if (!map.Bounded) {
				return { };
			}
			std::optional<tour_text_span> list;
			if (aTour.IndexBaseline->SourceSpan) {
				const auto [begin, end] = *aTour.IndexBaseline->SourceSpan;
				list                    = tour_text_span{ begin, end };
			} else if (map.Lists.size() == 1 && !map.Lists.front().MappingNeeded) {
				list = map.Lists.front().Source;
			}
			if (!list || std::ranges::none_of(map.Lists, [&](const auto& aValue) { return aValue.Source == *list && !aValue.MappingNeeded; })) {
				return { };
			}
			const auto rendered = render_tour_index(aEntries, newline(raw));
			if (!rendered) {
				return { };
			}
			const auto                  generated = map_tour_markdown(*rendered);
			std::vector<tour_text_edit> edits;
			const auto                  original = std::ranges::find(map.Lists, *list, &tour_markdown_list::Source);
			// [[BPR-1222]]. A route written with Conversation links is unchanged
			// while it names the same comments in the same order.
			const auto same = [](const tour_markdown_link& aLeft, const tour_markdown_link& aRight) { return aLeft.Label == aRight.Label && tour_link_identity(aLeft.Destination) == tour_link_identity(aRight.Destination); };
			if (!std::ranges::equal(original->Entries, aEntries, same)) {
				append_edit(edits, raw, *list, span_text(*rendered, generated.Lists.front().Source));
			}
			for (const auto& name : { std::string{ "Problem" }, std::string{ "Solution" } }) {
				const auto count = std::ranges::count(map.Sections, name, &tour_markdown_section::Name);
				if (count > 1) {
					return { };
				}
				if (count == 1) {
					const auto  section  = std::ranges::find(map.Sections, name, &tour_markdown_section::Name);
					const auto& field    = name == "Problem" ? aTour.Problem : aTour.Solution;
					const auto& baseline = name == "Problem" ? aTour.ProblemBaseline : aTour.SolutionBaseline;
					if (baseline) {
						const auto observed = baseline->DraftText.value_or(baseline->SourceSpan ? baseline->Raw.substr(baseline->SourceSpan->first, baseline->SourceSpan->second - baseline->SourceSpan->first) : baseline->Raw);
						if (field == observed) {
							continue;
						}
					}

					if (trim(span_text(raw, section->Content)) != trim(field)) {
						append_edit(edits, raw, section->Content, std::string{ newline(raw) } + std::string{ trim(field) } + std::string{ newline(raw) } + std::string{ newline(raw) });
					}
				}
			}
			return edit_tour_markdown(raw, edits);
		}
		void retain_edit(tour_update_bodies& aResult, const tour_remote_binding& aRemote, const std::string& aBaseline, const std::string& aDesired)
		{
			if (aBaseline != aDesired) {
				aResult.Edits.push_back({ aRemote, aBaseline, aDesired });
			}
		}
	}
	auto propose_tour_stop_text(const tour_stop& aStop) -> std::optional<std::string>
	{
		if (!aStop.Baseline) {
			return { };
		}
		const auto& raw     = aStop.Baseline->Raw;
		const auto  mapping = map_tour_stop(raw);
		if (!mapping.Narrative || !mapping.Heading || !mapping.Footer) {
			return aStop.Body == raw || aStop.Baseline->DraftText == std::optional{ raw } ? std::optional{ aStop.Body } : std::nullopt;
		}
		const auto heading = span_text(raw, *mapping.Heading);
		const auto begin   = heading.find(" · ");
		if (begin == std::string::npos) {
			return { };
		}
		std::size_t ordinal = 0;
		const auto  parsed  = std::from_chars(heading.data() + begin + 4, heading.data() + heading.size(), ordinal);
		if (parsed.ec != std::errc{ }) {
			return { };
		}
		const auto footer = map_tour_markdown(span_text(raw, *mapping.Footer));
		return patch_stop(raw, aStop.Title, aStop.Body, ordinal, footer.Links, true);
	}
	auto needs_tour_destination(const tour_stop& aStop) -> bool
	{
		return !aStop.Remote || (aStop.Mapping == tour_mapping::author_selected && aStop.Mapped && (!aStop.Original || destination_of(*aStop.Mapped) != destination_of(*aStop.Original)));
	}
	auto plan_tour_update(const review_tour& aTour, std::span<const tour_created_destination> aCreated) -> std::optional<tour_update_bodies>
	{
		if (validate(aTour) != tour_refusal::none || aTour.RemoteStale || !aTour.Conflicts.empty() || aTour.ReadOnly || !aTour.Remote || !aTour.Remote->Own || !aTour.IndexBaseline || aTour.Stops.empty() || std::ranges::any_of(aTour.Stops, [](const auto& aStop) { return aStop.Suggestion && aStop.Preparation != tour_preparation::ready; })) {
			return { };
		}
		std::vector<tour_markdown_link> entries;
		for (const auto& stop : aTour.Stops) {
			const auto created = std::ranges::find(aCreated, stop.Id, &tour_created_destination::Stop);
			if (needs_tour_destination(stop) && created == aCreated.end()) {
				return { };
			}
			const auto& remote = created == aCreated.end() ? *stop.Remote : created->Remote;
			const auto  link   = tour_stop_link(remote.WebUrl);
			if (!remote.Own || remote.Entity.empty() || std::ranges::any_of(entries, [&](const auto& aEntry) { return aEntry.Destination == link; })) {
				return { };
			}
			entries.push_back({ stop.Title, link });
		}
		tour_update_bodies result;
		for (std::size_t i = 0; i < aTour.Stops.size(); ++i) {
			const auto& stop    = aTour.Stops[i];
			const auto  created = std::ranges::find(aCreated, stop.Id, &tour_created_destination::Stop);
			if (created == aCreated.end() && !stop.Baseline) {
				return { };
			}
			const auto&                     baseline = created == aCreated.end() ? stop.Baseline->Raw : created->InitialBody;
			std::vector<tour_markdown_link> navigation;
			if (i) {
				navigation.push_back({ "Previous", entries[i - 1].Destination });
			}
			navigation.push_back({ "Tour overview", aTour.Remote->WebUrl });
			if (i + 1 < entries.size()) {
				navigation.push_back({ "Next", entries[i + 1].Destination });
			}
			auto narrative = stop.Body;
			if (created != aCreated.end() && stop.Remote) {
				narrative += "\n\nReplaces [original discussion](" + stop.Remote->WebUrl + ").";
			}
			// [[BPR-1785]]. A stop published before the navigation note gains it here.
			auto body = created == aCreated.end() && unchanged_position(aTour, i) ? propose_tour_stop_text(stop) : patch_stop(baseline, stop.Title, narrative, i + 1, navigation);
			if (body) {
				body = with_tour_navigation_note(*body);
			}
			if (!body) {
				return { };
			}
			result.StopBodies.push_back(*body);
			if (created == aCreated.end()) {
				retain_edit(result, *stop.Remote, baseline, *body);
			} else if (stop.Remote && stop.Baseline) {
				const auto old = stop.Baseline->Raw + "\n\nContinued at [replacement stop](" + created->Remote.WebUrl + "). The original discussion remains here.\n";
				retain_edit(result, *stop.Remote, stop.Baseline->Raw, old);
			}
		}
		for (const auto& stop : aTour.Retired) {
			if (!stop.Remote || !stop.Remote->Own || !stop.Baseline) {
				return { };
			}
			const auto& raw    = stop.Baseline->Raw;
			const auto  mapped = map_tour_stop(raw);
			if (!mapped.Footer) {
				return { };
			}
			const auto desired = edit_tour_markdown(raw, std::vector<tour_text_edit>{ { *mapped.Footer, span_text(raw, *mapped.Footer), "[Tour overview](" + aTour.Remote->WebUrl + ")" + std::string{ newline(raw) } } });
			if (!desired) {
				return { };
			}
			retain_edit(result, *stop.Remote, raw, *desired);
		}
		const auto root = root_body(aTour, entries);
		if (!root) {
			return { };
		}
		result.Index = *root;
		retain_edit(result, *aTour.Remote, aTour.IndexBaseline->Raw, *root);
		return result;
	}
}
