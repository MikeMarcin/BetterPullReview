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
		}
		return "";
	}

	auto placement_refusal_of(const prompt_macro& aMacro, const prompt_surface aSurface) -> placement_refusal
	{
		if (aMacro.Action != prompt_action::none && !is_menu(aSurface)) {
			return placement_refusal::action_not_chip;
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
} // namespace bpr::core
