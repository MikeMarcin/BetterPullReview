#include "bpr/core/review_tour_update.hpp"
#include <algorithm>
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
		auto patch_stop(std::string_view aRaw, std::string_view aTitle, std::string_view aNarrative, std::size_t aOrdinal, std::span<const tour_markdown_link> aNavigation) -> std::optional<std::string>
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
			std::vector<tour_text_edit> edits;
			append_edit(edits, aRaw, *mapped.Heading, span_text(*rendered, *generated.Heading));
			if (trim(span_text(aRaw, *mapped.Narrative)) != trim(aNarrative)) {
				append_edit(edits, aRaw, *mapped.Narrative, span_text(*rendered, *generated.Narrative));
			}
			append_edit(edits, aRaw, *mapped.Footer, span_text(*rendered, *generated.Footer));
			return edit_tour_markdown(aRaw, edits);
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
			if (!list || std::ranges::none_of(map.Lists, [&](const auto& value) { return value.Source == *list && !value.MappingNeeded; })) {
				return { };
			}
			const auto rendered = render_tour_index(aEntries, newline(raw));
			if (!rendered) {
				return { };
			}
			const auto                  generated = map_tour_markdown(*rendered);
			std::vector<tour_text_edit> edits;
			const auto                  original = std::ranges::find(map.Lists, *list, &tour_markdown_list::Source);
			if (original->Entries != std::vector<tour_markdown_link>(aEntries.begin(), aEntries.end())) {
				append_edit(edits, raw, *list, span_text(*rendered, generated.Lists.front().Source));
			}
			for (const auto& name : { std::string{ "Problem" }, std::string{ "Solution" } }) {
				const auto count = std::ranges::count(map.Sections, name, &tour_markdown_section::Name);
				if (count > 1) {
					return { };
				}
				if (count == 1) {
					const auto  section = std::ranges::find(map.Sections, name, &tour_markdown_section::Name);
					const auto& field   = name == "Problem" ? aTour.Problem : aTour.Solution;
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
	auto needs_tour_destination(const tour_stop& aStop) -> bool
	{
		return !aStop.Remote || (aStop.Mapped && (!aStop.Original || destination_of(*aStop.Mapped) != destination_of(*aStop.Original)));
	}
	auto plan_tour_update(const review_tour& aTour, std::span<const tour_created_destination> aCreated) -> std::optional<tour_update_bodies>
	{
		if (validate(aTour) != tour_refusal::none || aTour.ReadOnly || !aTour.Remote || !aTour.Remote->Own || !aTour.IndexBaseline || aTour.Stops.empty()) {
			return { };
		}
		std::vector<tour_markdown_link> entries;
		for (const auto& stop : aTour.Stops) {
			const auto created = std::ranges::find(aCreated, stop.Id, &tour_created_destination::Stop);
			if (needs_tour_destination(stop) && created == aCreated.end()) {
				return { };
			}
			const auto& remote = created == aCreated.end() ? *stop.Remote : created->Remote;
			if (!remote.Own || remote.Entity.empty() || std::ranges::any_of(entries, [&](const auto& e) { return e.Destination == remote.WebUrl; })) {
				return { };
			}
			entries.push_back({ stop.Title, remote.WebUrl });
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
			const auto body = patch_stop(baseline, stop.Title, narrative, i + 1, navigation);
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
