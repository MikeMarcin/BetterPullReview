#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_IDENTITY_AVATAR_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_IDENTITY_AVATAR_HPP

#include "bpr/core/appearance_model.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace bpr::core {
	/// \brief [[BPR-1583]]. How many tones a person's avatar can take.
	///
	/// \details Ten hues spaced round the wheel with the muddy yellow band left
	/// out, so two tones that meet in one list are told apart at a glance. More
	/// tones would collide less often and resemble each other more.
	inline constexpr std::size_t identity_tone_count = 10U;

	/// \brief [[BPR-1583]]. One avatar's disc and the initials drawn on it.
	struct identity_tone
	{
		appearance_color Fill;
		appearance_color Ink;

		auto operator<=>(const identity_tone&) const = default;
	};

	/// \brief [[BPR-1583]]. Which half of a tone a ground takes.
	///
	/// \details A light ground takes the deep fill with white initials; a dark
	/// ground takes the light fill with initials in the hue's own near-black.
	enum class identity_ground : std::uint8_t {
		light,
		dark,
	};

	/// \brief [[BPR-1583]]. The tone slot one identity keeps on every surface.
	///
	/// \details FNV-1a over the identity's bytes with ASCII letters folded to
	/// lower case, because GitHub logins are case-insensitive (`MikeMarcin` and
	/// `mikemarcin` are one person and take one slot), then MurmurHash3's
	/// 32-bit finalizer, modulo the tone count.
	///
	/// \returns No value for an empty identity, which takes the neutral tone.
	[[nodiscard]] auto identity_tone_slot(std::string_view aIdentity) noexcept
		-> std::optional<std::size_t>;

	/// \brief [[BPR-1583]]. One slot's tone on one kind of ground, or the
	/// neutral tone for no slot.
	///
	/// \details The table holds every fill at 7:1 or more against its initials,
	/// so a tone reads at the high-contrast text minimum as well as the
	/// standard one, and at 3:1 or more against the built-in grounds.
	[[nodiscard]] auto identity_tone_of(std::optional<std::size_t> aSlot, identity_ground aGround) noexcept
		-> identity_tone;

	/// \brief [[BPR-1583]]. Whether initials on this ground read better in white
	/// or in black, which decides the half of the tone it takes.
	[[nodiscard]] auto identity_ground_of(appearance_color aGround) noexcept -> identity_ground;

	/// \brief [[BPR-1583]]. The tone one identity is drawn in over these grounds.
	///
	/// \details The first ground decides the half. A ground the tabled fill does
	/// not clear at the indicator minimum (a custom theme's) moves the fill a
	/// sixteenth at a time towards black on a light ground or white on a dark
	/// one until every ground is cleared, which only raises the initials'
	/// contrast. With no grounds the light half is taken unchanged.
	[[nodiscard]] auto identity_tone_for(std::string_view aIdentity, std::span<const appearance_color> aGrounds) noexcept
		-> identity_tone;

	/// \brief [[BPR-1583]]. The initials an avatar shows for a name.
	///
	/// \details The name is split into words at every ASCII character that is
	/// not a letter or digit, after a trailing `[bot]` is dropped. The first
	/// character of the first word, and of the second when there is one, are
	/// the initials, ASCII letters upper-cased: `tbutler` is T, `reviewer-four`
	/// is RF, `dependabot[bot]` is D. A character outside ASCII is kept whole.
	///
	/// \returns `?` when the name has no word.
	[[nodiscard]] auto identity_initials(std::string_view aName) -> std::string;
} // namespace bpr::core

#endif
