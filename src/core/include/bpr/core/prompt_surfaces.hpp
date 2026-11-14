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
	enum class prompt_surface : std::uint8_t {
		pull_request_menu,
		pull_request_chips,
		file_menu,
		file_chips,
		thread_menu,
		thread_chips,
		tour_stop,
		tour_part,
		whole_tour,
	};
	inline constexpr std::size_t prompt_surface_count = 9U;
	/// \brief `pull-request-menu`, `file-chips`, `tour-stop` and so on.
	[[nodiscard]] auto stable_name(prompt_surface aSurface) noexcept -> std::string_view;
	[[nodiscard]] auto find_prompt_surface(std::string_view aName) noexcept -> std::optional<prompt_surface>;
	/// \brief An Ask menu (as opposed to suggested chips).
	[[nodiscard]] auto is_menu(prompt_surface aSurface) noexcept -> bool;
	/// \brief The scope an Ask menu or chip surface asks at; none for a tour chat.
	[[nodiscard]] auto surface_scope(prompt_surface aSurface) noexcept -> std::optional<prompt_scope>;

	/// \brief Every surface's placements, in order, by prompt identity.
	struct prompt_surfaces
	{
		std::array<std::vector<std::string>, prompt_surface_count> Lists;

		auto operator<=>(const prompt_surfaces&) const = default;
	};

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
} // namespace bpr::core

#endif
