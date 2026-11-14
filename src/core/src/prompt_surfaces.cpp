#include "bpr/core/prompt_surfaces.hpp"

#include <algorithm>
#include <iterator>

namespace bpr::core {
	namespace {
		constexpr auto names = std::array<std::string_view, prompt_surface_count>{
			"pull-request-menu",
			"pull-request-chips",
			"file-menu",
			"file-chips",
			"thread-menu",
			"thread-chips",
			"tour-stop",
			"tour-part",
			"whole-tour",
		};

		[[nodiscard]] auto list(prompt_surfaces& aSurfaces, const prompt_surface aSurface) -> std::vector<std::string>&
		{
			return aSurfaces.Lists.at(static_cast<std::size_t>(aSurface));
		}

		/// The place whose variables a surface fills in.
		[[nodiscard]] auto surface_place(const prompt_surface aSurface) noexcept -> prompt_place
		{
			switch (aSurface) {
			case prompt_surface::thread_menu:
			case prompt_surface::thread_chips: return prompt_place::conversation;
			case prompt_surface::tour_stop:    return prompt_place::tour_stop;
			case prompt_surface::tour_part:    return prompt_place::tour_part;
			case prompt_surface::whole_tour:   return prompt_place::tour_whole;
			default:                           return prompt_place::agent_tab;
			}
		}

		[[nodiscard]] auto fills(const prompt_surface aSurface, const prompt_variable aVariable) noexcept -> bool
		{
			if (!prompt_variable_allowed(surface_place(aSurface), aVariable)) {
				return false;
			}
			// The pull request's surfaces have no file or lines.
			const auto fileOrLines = aVariable == prompt_variable::file || aVariable == prompt_variable::file_name || aVariable == prompt_variable::lines
				|| aVariable == prompt_variable::lines_first || aVariable == prompt_variable::lines_last;
			return !(fileOrLines && (aSurface == prompt_surface::pull_request_menu || aSurface == prompt_surface::pull_request_chips));
		}

		[[nodiscard]] auto is_tour_surface(const prompt_surface aSurface) noexcept -> bool
		{
			return aSurface == prompt_surface::tour_stop || aSurface == prompt_surface::tour_part || aSurface == prompt_surface::whole_tour;
		}
	} // namespace

	auto stable_name(const prompt_surface aSurface) noexcept -> std::string_view
	{
		const auto index = static_cast<std::size_t>(aSurface);
		return index < names.size() ? names.at(index) : std::string_view{ "pull-request-menu" };
	}

	auto find_prompt_surface(const std::string_view aName) noexcept -> std::optional<prompt_surface>
	{
		const auto found = std::ranges::find(names, aName);
		return found == names.end() ? std::nullopt : std::optional{ static_cast<prompt_surface>(std::distance(names.begin(), found)) };
	}

	auto is_menu(const prompt_surface aSurface) noexcept -> bool
	{
		return aSurface == prompt_surface::pull_request_menu || aSurface == prompt_surface::file_menu || aSurface == prompt_surface::thread_menu;
	}

	auto surface_scope(const prompt_surface aSurface) noexcept -> std::optional<prompt_scope>
	{
		switch (aSurface) {
		case prompt_surface::pull_request_menu:
		case prompt_surface::pull_request_chips: return prompt_scope::pull_request;
		case prompt_surface::file_menu:
		case prompt_surface::file_chips:         return prompt_scope::file;
		case prompt_surface::thread_menu:
		case prompt_surface::thread_chips:       return prompt_scope::thread;
		default:                                 return std::nullopt;
		}
	}

	auto shipped_surfaces(const std::span<const prompt_macro> aMacros) -> prompt_surfaces
	{
		auto surfaces = prompt_surfaces{ };
		for (const auto& macro : aMacros) {
			if (macro.Body.QuickAsk) {
				if (macro.Body.Scopes.PullRequest) {
					list(surfaces, prompt_surface::pull_request_menu).push_back(macro.Id);
				}
				if (macro.Body.Scopes.File) {
					list(surfaces, prompt_surface::file_menu).push_back(macro.Id);
				}
				if (macro.Body.Scopes.Thread) {
					list(surfaces, prompt_surface::thread_menu).push_back(macro.Id);
				}
			}
			if (macro.Action != prompt_action::none) {
				continue;
			}
			switch (macro.Place) {
			case prompt_place::agent_tab:
				list(surfaces, macro.Body.Subject == prompt_subject::pull_request ? prompt_surface::pull_request_chips : prompt_surface::file_chips).push_back(macro.Id);
				break;
			case prompt_place::conversation: list(surfaces, prompt_surface::thread_chips).push_back(macro.Id); break;
			case prompt_place::tour_stop:    list(surfaces, prompt_surface::tour_stop).push_back(macro.Id); break;
			case prompt_place::tour_part:    list(surfaces, prompt_surface::tour_part).push_back(macro.Id); break;
			case prompt_place::tour_whole:   list(surfaces, prompt_surface::whole_tour).push_back(macro.Id); break;
			}
		}
		return surfaces;
	}

	auto placement_reason(const placement_refusal aRefusal) -> std::string_view
	{
		switch (aRefusal) {
		case placement_refusal::none:                 return "";
		case placement_refusal::unknown_prompt:       return "That prompt is not in the library.";
		case placement_refusal::action_not_chip:      return "This prompt runs an action, so it is in menus only, never a chip.";
		case placement_refusal::placeholder_unfilled: return "The pull request can't fill this prompt's {file}, {lines} or similar.";
		case placement_refusal::not_placed:           return "It is not on that surface.";
		case placement_refusal::wrong_chat:           return "It was written for another chat.";
		case placement_refusal::action_too_narrow:    return "This action asks about more than this menu's scope holds.";
		}
		return "";
	}

	auto placement_refusal_of(const prompt_macro& aMacro, const prompt_surface aSurface) -> placement_refusal
	{
		if (aMacro.Action != prompt_action::none && !is_menu(aSurface)) {
			return placement_refusal::action_not_chip;
		}
		// An action asks at its own scope, or once for each at a wider one.
		if (const auto scope = surface_scope(aSurface); aMacro.Action != prompt_action::none && scope && !prompt_scope_allowed(aMacro.Place, aMacro.Body.Subject, *scope)) {
			return placement_refusal::action_too_narrow;
		}
		// A tour chat's prompts and the review's are not interchangeable.
		const auto tourPrompt = aMacro.Place == prompt_place::tour_stop || aMacro.Place == prompt_place::tour_part || aMacro.Place == prompt_place::tour_whole;
		if (tourPrompt != is_tour_surface(aSurface)) {
			return placement_refusal::wrong_chat;
		}
		for (const auto& text : { aMacro.Body.Name, aMacro.Body.Template }) {
			// Scanned at the surface's place: a refusal or an unfilled
			// variable refuses the surface.
			const auto scan = scan_prompt_template(text, surface_place(aSurface));
			if (!scan.Refusals.empty()) {
				return placement_refusal::placeholder_unfilled;
			}
			for (const auto& piece : scan.Pieces) {
				if (piece.Variable && !fills(aSurface, *piece.Variable)) {
					return placement_refusal::placeholder_unfilled;
				}
			}
		}
		return placement_refusal::none;
	}

	auto place(prompt_surfaces& aSurfaces, const std::span<const prompt_macro> aMacros, const std::string& aId, const prompt_surface aSurface, const std::optional<std::size_t> aAt)
		-> placement_refusal
	{
		const auto found = std::ranges::find(aMacros, aId, &prompt_macro::Id);
		if (found == aMacros.end()) {
			return placement_refusal::unknown_prompt;
		}
		if (const auto refused = placement_refusal_of(*found, aSurface); refused != placement_refusal::none) {
			return refused;
		}
		auto& placed = list(aSurfaces, aSurface);
		std::erase(placed, aId);
		const auto at = std::min(aAt.value_or(placed.size()), placed.size());
		placed.insert(placed.begin() + static_cast<std::ptrdiff_t>(at), aId);
		return placement_refusal::none;
	}

	auto remove_placement(prompt_surfaces& aSurfaces, const std::string& aId, const prompt_surface aSurface) -> placement_refusal
	{
		return std::erase(list(aSurfaces, aSurface), aId) == 0U ? placement_refusal::not_placed : placement_refusal::none;
	}

	auto move_placement(prompt_surfaces& aSurfaces, const std::string& aId, const prompt_surface aSurface, const int aBy) -> placement_refusal
	{
		auto&      placed = list(aSurfaces, aSurface);
		const auto found  = std::ranges::find(placed, aId);
		if (found == placed.end()) {
			return placement_refusal::not_placed;
		}
		const auto from = std::distance(placed.begin(), found);
		const auto to   = std::clamp<std::ptrdiff_t>(from + aBy, 0, static_cast<std::ptrdiff_t>(placed.size()) - 1);
		auto       kept = std::move(placed[static_cast<std::size_t>(from)]);
		placed.erase(placed.begin() + from);
		placed.insert(placed.begin() + to, std::move(kept));
		return placement_refusal::none;
	}

	auto surfaces_of(const prompt_surfaces& aSurfaces, const std::string& aId) -> std::vector<prompt_surface>
	{
		auto on = std::vector<prompt_surface>{ };
		for (std::size_t index = 0U; index < prompt_surface_count; ++index) {
			if (std::ranges::find(aSurfaces.Lists.at(index), aId) != aSurfaces.Lists.at(index).end()) {
				on.push_back(static_cast<prompt_surface>(index));
			}
		}
		return on;
	}

	auto merge_prompt_surfaces(const std::span<const prompt_macro> aLibrary, const std::span<const prompt_surface_change> aChanges) -> prompt_surfaces
	{
		auto       surfaces = shipped_surfaces(aLibrary);
		const auto known    = [aLibrary](const std::string& aId) { return std::ranges::find(aLibrary, aId, &prompt_macro::Id) != aLibrary.end(); };
		for (const auto& change : aChanges) {
			auto& fields = list(surfaces, change.Surface);
			auto  merged = std::vector<std::string>{ };
			for (const auto& id : change.Ids) {
				if (known(id) && std::ranges::find(merged, id) == merged.end()) {
					const auto found = std::ranges::find(aLibrary, id, &prompt_macro::Id);
					if (placement_refusal_of(*found, change.Surface) == placement_refusal::none) {
						merged.push_back(id);
					}
				}
			}
			for (const auto& id : fields) {
				if (std::ranges::find(merged, id) == merged.end() && std::ranges::find(change.Removed, id) == change.Removed.end()) {
					merged.push_back(id);
				}
			}
			fields = std::move(merged);
		}
		return surfaces;
	}

	auto prompt_surface_changes_of(const std::span<const prompt_macro> aLibrary, const prompt_surfaces& aSurfaces) -> std::vector<prompt_surface_change>
	{
		const auto fields  = shipped_surfaces(aLibrary);
		auto       changes = std::vector<prompt_surface_change>{ };
		for (std::size_t index = 0U; index < prompt_surface_count; ++index) {
			const auto& was = fields.Lists.at(index);
			const auto& now = aSurfaces.Lists.at(index);
			if (was == now) {
				continue;
			}
			auto change = prompt_surface_change{ .Surface = static_cast<prompt_surface>(index), .Ids = now };
			for (const auto& id : was) {
				if (std::ranges::find(now, id) == now.end()) {
					change.Removed.push_back(id);
				}
			}
			changes.push_back(std::move(change));
		}
		return changes;
	}

	auto placed_on(const prompt_macro& aMacro, const prompt_surface aSurface) -> prompt_macro
	{
		auto placed      = aMacro;
		placed.Surface   = aSurface;
		const auto scope = surface_scope(aSurface);
		if (!scope) {
			// A tour chat: its own place, and what that place asks about.
			placed.Place = surface_place(aSurface);
			if (placed.Body.Subject != prompt_subject::selected_text || placed.Place == prompt_place::tour_whole) {
				placed.Body.Subject = placed.Place == prompt_place::tour_stop ? prompt_subject::stop : placed.Place == prompt_place::tour_part ? prompt_subject::part
																																			   : prompt_subject::tour;
			}
			placed.Body.QuickAsk = false;
			placed.Body.Scopes   = { };
			return placed;
		}
		if (is_menu(aSurface)) {
			placed.Body.QuickAsk = true;
			placed.Body.Scopes   = { };
			placed.Body.Scopes.set(*scope, true);
			// An action, or a prompt about something narrower, asks once for
			// each at a wider scope: it keeps its own About.
			if (aMacro.Action != prompt_action::none || (prompt_scope_allowed(aMacro.Place, aMacro.Body.Subject, *scope) && subject_scope(aMacro.Body.Subject) != scope)) {
				return placed;
			}
		} else {
			placed.Body.QuickAsk = false;
			placed.Body.Scopes   = { };
		}
		placed.Place = surface_place(aSurface);
		switch (*scope) {
		case prompt_scope::thread: placed.Body.Subject = prompt_subject::conversation; break;
		case prompt_scope::file:
			placed.Body.Subject = (aMacro.Body.Subject == prompt_subject::selected_lines && !is_menu(aSurface)) ? prompt_subject::selected_lines : prompt_subject::open_file;
			break;
		case prompt_scope::pull_request: placed.Body.Subject = prompt_subject::pull_request; break;
		}
		if (!prompt_ask_allowed(placed.Place, placed.Body.Subject, placed.Body.Kind)) {
			placed.Body.Kind = prompt_ask::question;
		}
		return placed;
	}

	auto placed_prompt_macros(const std::span<const prompt_macro> aLibrary, const prompt_surfaces& aSurfaces) -> std::vector<prompt_macro>
	{
		auto placed = std::vector<prompt_macro>{ };
		for (std::size_t index = 0U; index < prompt_surface_count; ++index) {
			for (const auto& id : aSurfaces.Lists.at(index)) {
				if (const auto found = std::ranges::find(aLibrary, id, &prompt_macro::Id); found != aLibrary.end()) {
					placed.push_back(placed_on(*found, static_cast<prompt_surface>(index)));
				}
			}
		}
		return placed;
	}

	auto placed_prompt_macros(const prompt_macro_set& aSet) -> std::vector<prompt_macro> { return placed_prompt_macros(aSet.macros(), aSet.Surfaces); }

	auto menu_prompt_offers(const std::span<const prompt_macro> aPlaced, const prompt_facts& aFacts) -> std::vector<prompt_offer>
	{
		auto offers = std::vector<prompt_offer>{ };
		for (const auto& macro : aPlaced) {
			if (!macro.Surface || !is_menu(*macro.Surface) || macro.Action != prompt_action::none || macro.Place != aFacts.Place
				|| std::ranges::find(offers, macro.Id, &prompt_offer::Id) != offers.end()) {
				continue;
			}
			if (auto evaluation = evaluate_prompt_macro(macro, aFacts); evaluation.Offer) {
				offers.push_back(std::move(*evaluation.Offer));
			}
		}
		return offers;
	}

	namespace {
		[[nodiscard]] auto contains(const std::vector<std::string>& aIds, const std::string& aId) -> bool { return std::ranges::find(aIds, aId) != aIds.end(); }

		/// A surface as a repository's change makes it from the app-wide list.
		[[nodiscard]] auto layered_list(const std::vector<std::string>& aApp, const prompt_repository_surface* const aChange) -> std::vector<std::string>
		{
			if (aChange == nullptr) {
				return aApp;
			}
			auto kept = std::vector<std::string>{ };
			for (const auto& id : aApp) {
				if (!contains(aChange->Removed, id)) {
					kept.push_back(id);
				}
			}
			for (const auto& id : aChange->Added) {
				if (!contains(kept, id)) {
					kept.push_back(id);
				}
			}
			if (!aChange->Order) {
				return kept;
			}
			auto ordered = std::vector<std::string>{ };
			for (const auto& id : *aChange->Order) {
				if (contains(kept, id) && !contains(ordered, id)) {
					ordered.push_back(id);
				}
			}
			// Placed app-wide after the repository reordered: at the end.
			for (const auto& id : kept) {
				if (!contains(ordered, id)) {
					ordered.push_back(id);
				}
			}
			return ordered;
		}
	} // namespace

	auto apply_repository_layer(const std::span<const prompt_macro> aLibrary, const prompt_surfaces& aSurfaces, const prompt_repository_layer& aLayer) -> prompt_layered
	{
		auto layered = prompt_layered{ .Library = { aLibrary.begin(), aLibrary.end() } };
		for (auto& macro : layered.Library) {
			const auto found = std::ranges::find(aLayer.Overrides, macro.Id, &prompt_override::Id);
			if (found == aLayer.Overrides.end()) {
				continue;
			}
			if (found->Name) {
				macro.Body.Name = *found->Name;
			}
			if (found->Template) {
				macro.Body.Template = *found->Template;
			}
			if (found->Kind) {
				macro.Body.Kind = *found->Kind;
			}
			if (found->Conditions) {
				macro.Body.Conditions = *found->Conditions;
			}
			if (found->Match) {
				macro.Body.Match = *found->Match;
			}
			if (found->On) {
				macro.On = *found->On;
			}
		}
		for (const auto& own : aLayer.Own) {
			if (!own.BuiltIn && own.Action == prompt_action::none && std::ranges::find(layered.Library, own.Id, &prompt_macro::Id) == layered.Library.end()
				&& layered.Library.size() < maximum_prompt_macros * 2U) {
				layered.Library.push_back(own);
			}
		}
		for (std::size_t index = 0U; index < prompt_surface_count; ++index) {
			const auto surface = static_cast<prompt_surface>(index);
			const auto change  = std::ranges::find(aLayer.Surfaces, surface, &prompt_repository_surface::Surface);
			auto       list    = layered_list(aSurfaces.Lists.at(index), change == aLayer.Surfaces.end() ? nullptr : &*change);
			std::erase_if(list, [&layered](const std::string& aId) { return std::ranges::find(layered.Library, aId, &prompt_macro::Id) == layered.Library.end(); });
			layered.Surfaces.Lists.at(index) = std::move(list);
		}
		return layered;
	}

	auto repository_layer_of(const std::span<const prompt_macro> aLibrary, const prompt_surfaces& aSurfaces, const prompt_layered& aEdited, std::string aRepository) -> prompt_repository_layer
	{
		auto layer = prompt_repository_layer{ .Repository = std::move(aRepository) };
		for (const auto& macro : aEdited.Library) {
			const auto app = std::ranges::find(aLibrary, macro.Id, &prompt_macro::Id);
			if (app == aLibrary.end()) {
				if (!macro.BuiltIn && macro.Action == prompt_action::none) {
					layer.Own.push_back(macro);
				}
				continue;
			}
			auto change = prompt_override{ .Id = macro.Id };
			if (macro.Body.Name != app->Body.Name) {
				change.Name = macro.Body.Name;
			}
			if (macro.Body.Template != app->Body.Template) {
				change.Template = macro.Body.Template;
			}
			if (macro.Body.Kind != app->Body.Kind) {
				change.Kind = macro.Body.Kind;
			}
			if (macro.Body.Conditions != app->Body.Conditions) {
				change.Conditions = macro.Body.Conditions;
			}
			if (macro.Body.Match != app->Body.Match) {
				change.Match = macro.Body.Match;
			}
			if (macro.On != app->On) {
				change.On = macro.On;
			}
			if (change != prompt_override{ .Id = macro.Id }) {
				layer.Overrides.push_back(std::move(change));
			}
		}
		for (std::size_t index = 0U; index < prompt_surface_count; ++index) {
			const auto& app    = aSurfaces.Lists.at(index);
			const auto& edited = aEdited.Surfaces.Lists.at(index);
			if (app == edited) {
				continue;
			}
			auto change = prompt_repository_surface{ .Surface = static_cast<prompt_surface>(index) };
			for (const auto& id : app) {
				if (!contains(edited, id)) {
					change.Removed.push_back(id);
				}
			}
			for (const auto& id : edited) {
				if (!contains(app, id)) {
					change.Added.push_back(id);
				}
			}
			if (layered_list(app, &change) != edited) {
				change.Order = edited;
			}
			layer.Surfaces.push_back(std::move(change));
		}
		return layer;
	}

	auto prompt_set_for_repository(const prompt_macro_set& aSet, const std::string_view aRepository) -> prompt_macro_set
	{
		const auto layer = std::ranges::find_if(aSet.Repositories, [aRepository](const prompt_repository_layer& aLayer) {
			return aLayer.Repository.size() == aRepository.size()
				&& std::ranges::equal(aLayer.Repository, aRepository, [](const char aLeft, const char aRight) { return (aLeft | 0x20) == (aRight | 0x20); });
		});
		if (aRepository.empty() || layer == aSet.Repositories.end()) {
			return aSet;
		}
		const auto layered = apply_repository_layer(aSet.macros(), aSet.Surfaces, *layer);
		auto       set     = aSet;
		set.Entries.clear();
		for (const auto& macro : layered.Library) {
			const auto was = std::ranges::find_if(aSet.Entries, [&macro](const prompt_set_entry& aEntry) { return aEntry.Macro.Id == macro.Id; });
			set.Entries.push_back({ .Macro = macro, .Changed = was != aSet.Entries.end() && was->Changed, .Outdated = was != aSet.Entries.end() && was->Outdated });
		}
		set.Surfaces = layered.Surfaces;
		return set;
	}
} // namespace bpr::core
