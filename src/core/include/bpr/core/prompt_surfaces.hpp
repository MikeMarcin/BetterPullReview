#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_PROMPT_SURFACES_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_PROMPT_SURFACES_HPP

#include "bpr/core/prompt_macro.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-2179]] (docs/design/prompts-settings-v4/SPEC.md and the
/// owner's answers on the card, 2026-11-14). Where prompts appear: a surface
/// is an ordered list of placements, one prompt on one surface at most once.
/// The Ask menus and suggested chips of the pull request, a file and a
/// thread, and the tour's three chats. Action rows are menu only, never
/// chips; About is dropped, so a surface whose scope cannot fill a prompt's
/// placeholders refuses it. Pure decisions over the library.
namespace bpr::core {
	/// \brief `pull-request-menu`, `file-chips`, `tour-stop` and so on.
	[[nodiscard]] auto stable_name(prompt_surface aSurface) noexcept -> std::string_view;
	[[nodiscard]] auto find_prompt_surface(std::string_view aName) noexcept -> std::optional<prompt_surface>;
	/// \brief An Ask menu (as opposed to suggested chips).
	[[nodiscard]] auto is_menu(prompt_surface aSurface) noexcept -> bool;
	/// \brief The scope an Ask menu or chip surface asks at; none for a tour chat.
	[[nodiscard]] auto surface_scope(prompt_surface aSurface) noexcept -> std::optional<prompt_scope>;

	/// \brief The surfaces today's fields describe, in the library's order:
	/// a quick ask's scopes are its menus; a chip's place and About its chip
	/// surface; an action row is never a chip.
	[[nodiscard]] auto shipped_surfaces(std::span<const prompt_macro> aMacros) -> prompt_surfaces;

	enum class placement_refusal : std::uint8_t {
		none,
		unknown_prompt,
		/// The owner: action rows are menu only, never chips.
		action_not_chip,
		/// A placeholder this surface cannot fill.
		placeholder_unfilled,
		not_placed,
		/// A chip surface for one kind of chat (a tour's) and a prompt
		/// written for another.
		wrong_chat,
		/// An action on a menu narrower than what it asks about (What
		/// changed since my review on a file's menu).
		action_too_narrow,
	};
	/// \brief The refusal in words, for a drop or an Add to item.
	[[nodiscard]] auto placement_reason(placement_refusal aRefusal) -> std::string_view;
	/// \brief Whether `aMacro` may go on `aSurface`.
	[[nodiscard]] auto placement_refusal_of(const prompt_macro& aMacro, prompt_surface aSurface) -> placement_refusal;
	/// \brief Places `aId` on `aSurface` at `aAt` (the end by default); a
	/// prompt already there moves. Nothing changes on a refusal.
	[[nodiscard]] auto place(prompt_surfaces& aSurfaces, std::span<const prompt_macro> aMacros, const std::string& aId, prompt_surface aSurface, std::optional<std::size_t> aAt = std::nullopt)
		-> placement_refusal;
	/// \brief Removes a placement; the prompt stays in the library.
	[[nodiscard]] auto remove_placement(prompt_surfaces& aSurfaces, const std::string& aId, prompt_surface aSurface) -> placement_refusal;
	/// \brief Moves a placement `aBy` places within its surface.
	[[nodiscard]] auto move_placement(prompt_surfaces& aSurfaces, const std::string& aId, prompt_surface aSurface, int aBy) -> placement_refusal;
	/// \brief The surfaces `aId` is on, in surface order.
	[[nodiscard]] auto surfaces_of(const prompt_surfaces& aSurfaces, const std::string& aId) -> std::vector<prompt_surface>;

	/// \brief The surfaces `aLibrary`'s fields describe (`shipped_surfaces`)
	/// with `aChanges` applied: a changed surface is its stored list, less
	/// prompts not in the library, then any prompt the fields put there that
	/// the list neither names nor took off (one a later release adds).
	[[nodiscard]] auto merge_prompt_surfaces(std::span<const prompt_macro> aLibrary, std::span<const prompt_surface_change> aChanges) -> prompt_surfaces;
	/// \brief The changes that make `aSurfaces` from what `aLibrary`'s fields
	/// describe: one for each surface that differs.
	[[nodiscard]] auto prompt_surface_changes_of(std::span<const prompt_macro> aLibrary, const prompt_surfaces& aSurfaces) -> std::vector<prompt_surface_change>;

	/// \brief `aMacro` as it is offered on `aSurface`: placement decides the
	/// scope. On a menu it is a quick ask at that menu's scope alone; on chips
	/// it is a chip of the surface's chat. A prompt about something narrower
	/// than a menu's scope keeps its About, so it asks once for each thread
	/// or file. Findings, where the surface cannot take them, is asked as a
	/// question.
	[[nodiscard]] auto placed_on(const prompt_macro& aMacro, prompt_surface aSurface) -> prompt_macro;
	/// \brief Every placement as `placed_on` makes it, surface by surface in
	/// order: the list `decide_prompt_offers` and `decide_quick_asks` read,
	/// so each surface's order is the order offered, and each chip surface
	/// has its own `maximum_prompt_chips`.
	[[nodiscard]] auto placed_prompt_macros(std::span<const prompt_macro> aLibrary, const prompt_surfaces& aSurfaces) -> std::vector<prompt_macro>;
	/// \brief `placed_prompt_macros` over a merged set's own surfaces.
	[[nodiscard]] auto placed_prompt_macros(const prompt_macro_set& aSet) -> std::vector<prompt_macro>;
	/// \brief [[BPR-2179]] (spec section 11). `aLibrary` and `aSurfaces` with a
	/// repository's layer applied: each overridden field, its own prompts
	/// after the app-wide ones, and each surface's additions, removals and
	/// order. Placements naming a prompt neither layer holds are dropped.
	struct prompt_layered
	{
		std::vector<prompt_macro> Library;
		prompt_surfaces           Surfaces;

		auto operator<=>(const prompt_layered&) const = default;
	};
	[[nodiscard]] auto apply_repository_layer(std::span<const prompt_macro> aLibrary, const prompt_surfaces& aSurfaces, const prompt_repository_layer& aLayer) -> prompt_layered;
	/// \brief The layer that makes `aEdited` from the app-wide library and
	/// surfaces: what `apply_repository_layer` reads back as `aEdited`.
	[[nodiscard]] auto repository_layer_of(std::span<const prompt_macro> aLibrary, const prompt_surfaces& aSurfaces, const prompt_layered& aEdited, std::string aRepository)
		-> prompt_repository_layer;
	/// \brief A merged set as `aRepository`'s pull requests see it; the set
	/// itself with no layer for it.
	[[nodiscard]] auto prompt_set_for_repository(const prompt_macro_set& aSet, std::string_view aRepository) -> prompt_macro_set;

	/// \brief What a press on an Ask menu row sends where the window is now:
	/// the menu placements of `aPlaced` decided as offers at `aFacts.Place`,
	/// with no cap. A prompt only on a menu is never a chip, yet its press
	/// still expands against the facts at the press.
	[[nodiscard]] auto menu_prompt_offers(std::span<const prompt_macro> aPlaced, const prompt_facts& aFacts) -> std::vector<prompt_offer>;
} // namespace bpr::core

#endif
