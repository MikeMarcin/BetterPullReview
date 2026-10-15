#include "bpr/core/white_space.hpp"

#include <algorithm>
#include <cstddef>

namespace bpr::core {
	auto white_space_drawn(
		const std::span<const white_space_character> aLine,
		const white_space_drawing                    aDrawing,
		const bool                                   aSelected) -> std::vector<bool>
	{
		auto drawn = std::vector<bool>(aLine.size(), false);
		if (!white_space_may_draw(aDrawing, aSelected)) {
			return drawn;
		}
		const auto isOther = [](const white_space_character aCharacter) {
			return aCharacter == white_space_character::other;
		};
		// The first and one past the last other character; on a line of white
		// space alone both are the line's end, so every character is leading and
		// every one is trailing.
		const auto first = static_cast<std::size_t>(
			std::ranges::find_if(aLine, isOther) - aLine.begin());
		const auto last = static_cast<std::size_t>(
			aLine.size()
			- static_cast<std::size_t>(std::ranges::find_if(aLine.rbegin(), aLine.rend(), isOther) - aLine.rbegin()));
		const auto whole = first == aLine.size();
		for (auto index = std::size_t{ 0U }; index < aLine.size(); ++index) {
			if (isOther(aLine[index])) {
				continue;
			}
			const auto leading  = whole || index < first;
			const auto trailing = whole || index >= last;
			switch (aDrawing) {
			case white_space_drawing::none:
				break;
			case white_space_drawing::selection:
			case white_space_drawing::all:
				drawn[index] = true;
				break;
			case white_space_drawing::leading:
				drawn[index] = leading;
				break;
			case white_space_drawing::trailing:
				drawn[index] = trailing;
				break;
			case white_space_drawing::boundary:
				drawn[index] = leading || trailing;
				break;
			}
		}
		return drawn;
	}
} // namespace bpr::core
