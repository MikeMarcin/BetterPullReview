#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_WHITE_SPACE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_WHITE_SPACE_HPP

#include "bpr/core/appearance_model.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace bpr::core {
	/// \brief What one character of a code line is, as far as white space goes.
	enum class white_space_character : std::uint8_t {
		other,
		space,
		tab,
	};

	/// \brief Whether the drawing choice can mark any character on this line.
	/// \details Hosts can avoid preparing text and geometry when policy draws
	/// nothing, including an unselected line under the default selection mode.
	[[nodiscard]] constexpr auto white_space_may_draw(const white_space_drawing aDrawing, const bool aSelected) noexcept -> bool
	{
		return valid(aDrawing) && aDrawing != white_space_drawing::none
			&& (aDrawing != white_space_drawing::selection || aSelected);
	}

	/// \brief Decides which white space characters of one line are drawn.
	///
	/// \details [[BPR-1107]]. The answer has one entry per character, and only a
	/// space or a tab can be true. Leading white space runs from the start of the
	/// line to its first other character, and trailing white space from its last
	/// other character to the end. A line of white space alone is both.
	/// Everything else between two other characters is enclosed, which only
	/// `all` draws, and `selection` draws everything on a line that is selected.
	///
	/// The core decides which characters; how a dot or a line looks, and where it
	/// lands, is presentation's.
	[[nodiscard]] auto white_space_drawn(
		std::span<const white_space_character> aLine,
		white_space_drawing                    aDrawing,
		bool                                   aSelected) -> std::vector<bool>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_WHITE_SPACE_HPP
