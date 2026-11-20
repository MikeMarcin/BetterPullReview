#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_SUGGESTION_RANGE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_SUGGESTION_RANGE_HPP

#include "bpr/core/diff_compute.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-2298]] (owner, 2026-11-19: "the comments themselves are only
/// on one line instead of the entire block that needs to be replaced by the
/// suggestion which makes GitHub's suggestion feature not work properly
/// here"). The lines an agent's suggested comment must be anchored to so that
/// GitHub, applying its ```` ```suggestion ```` block, replaces exactly the
/// lines the block rewrites.
/// \details GitHub replaces a comment's lines (`start_line` to `line`, on the
/// head) with the block's lines. An agent often names only the line at fault
/// while its block rewrites the lines around it too, so the block's unchanged
/// lines would be written a second time beside the originals. This reads the
/// one suggestion block of a comment against the head lines near the
/// agent's range and widens the range, deterministically, only when the
/// block reproduces verbatim the head lines just before or after it: the
/// window grown to those lines is explained by the block keeping them, and
/// at least one line it adds is substantive (holds a letter or digit), so a
/// lone brace or blank line never moves a range; or, failing that, when the
/// block rewrites line for line a window of its own length around the
/// agent's lines, each line the same or starting with the same indentation
/// and identifier (`\tWidth = std::max(aWidth, 0);` for `\tWidth = aWidth;`),
/// one of the lines beside the agent's among them. A range the block does
/// not clearly overrun is kept; one it overruns without such evidence is kept
/// and flagged `unverified`. Pure, and bounded by the square of the block's
/// length (at most `maximum_checked_suggestion_lines`).
namespace bpr::core {
	/// \brief What the comment's suggestion says about the agent's lines.
	enum class agent_suggestion_range_reading : std::uint8_t {
		/// The comment holds no suggestion block, or more than one: nothing
		/// to check, and the range is the agent's.
		no_suggestion,
		/// The block fits the agent's lines: it is no longer than they are,
		/// or it keeps them all and adds lines after them.
		kept,
		/// The block rewrites lines beside the agent's: the range now covers
		/// them.
		widened,
		/// The block is longer than the agent's lines, does not keep them,
		/// and no nearby lines explain it. The range is the agent's; the
		/// reviewer should check what the suggestion replaces.
		unverified,
		/// The agent's lines are not all in the head document.
		outside_document,
	};

	[[nodiscard]] auto stable_name(agent_suggestion_range_reading aReading) noexcept -> std::string_view;

	/// \brief Most lines a suggestion block may hold to be checked; a longer
	/// one is `unverified`.
	inline constexpr std::uint32_t maximum_checked_suggestion_lines = 400U;

	/// \brief One-based, inclusive head lines a suggested comment anchors to.
	struct agent_suggestion_range
	{
		agent_suggestion_range_reading Reading = agent_suggestion_range_reading::no_suggestion;
		std::uint32_t                  First   = 0U;
		std::uint32_t                  Last    = 0U;
		/// The block's line count; zero when there is no single block.
		std::uint32_t SuggestedLines = 0U;

		auto operator<=>(const agent_suggestion_range&) const = default;
	};

	/// \brief The lines of the comment's only suggestion block, without
	/// their line endings (`\r` before a break dropped); none for a comment
	/// with no block or several.
	[[nodiscard]] auto single_suggestion_lines(std::string_view aComment) -> std::optional<std::vector<std::string>>;

	/// \brief Decides the head lines `aComment`'s suggestion replaces, from
	/// the agent's one-based inclusive `aFirst`..`aLast`.
	/// \param aHead Head lines `aHeadFirst` onward (one-based), without their
	/// line endings: at least the agent's range and, to widen, up to the
	/// block's line count either side of it. `aHeadLineCount` is the
	/// document's whole line count.
	[[nodiscard]] auto decide_agent_suggestion_range(std::string_view aComment, std::uint32_t aFirst, std::uint32_t aLast, std::span<const std::string> aHead, std::uint32_t aHeadFirst,
													 std::uint32_t aHeadLineCount) -> agent_suggestion_range;

	/// \brief `decide_agent_suggestion_range` over a decoded head document,
	/// reading only the window it needs.
	[[nodiscard]] auto decide_agent_suggestion_range(std::string_view aComment, std::uint32_t aFirst, std::uint32_t aLast, const decoded_text_document& aHead)
		-> agent_suggestion_range;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_SUGGESTION_RANGE_HPP
