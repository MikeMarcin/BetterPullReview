#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_UPDATE_LINE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_UPDATE_LINE_HPP

#include "bpr/core/epoch_model.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

/// \file
/// \brief Where the Updates line's handles may rest and where a compared
/// anchor sits on it ([[BPR-1557]]).
/// \details RFC-BPR-0028's Updates line is one node per update with two
/// handles, the compared anchors. The line starts with the pull request's
/// base: the anchor Whole PR compares from, which is the latest live epoch's
/// base (`resolve_comparison`), published by the core with every resolution
/// as `comparison_resolution_delta::PullRequestBase`. It has no update of its
/// own, so it is its own place at the line's start, and the left handle rests
/// there by default. An older update's own base, which "Show this update
/// alone" compares from, keeps its place just before that update.
///
/// The order along the line is the base, then each update's base and head,
/// oldest first. The left handle's stops are the base and every update's head
/// it can take; the right handle's are the heads. A step moves to the next
/// stop in line order from wherever the handle is, including a place that is
/// not itself a stop. The presentation draws; it decides none of this.
namespace bpr::core {
	/// \brief One update on the line, oldest first, as the presentation holds it.
	struct update_line_update
	{
		/// The retained epoch whose head this update is, if this install has it.
		std::optional<review_epoch_id> Epoch;
		/// An update GitHub recorded that this install may import when chosen.
		bool Importable = false;
		/// Its own base anchor can be compared from.
		bool HasBase = false;

		auto operator<=>(const update_line_update&) const = default;
	};

	enum class update_line_place_kind : std::uint8_t {
		/// The pull request's base, at the line's start.
		pull_request_base,
		/// An update's own observed base, just before the update.
		update_base,
		/// An update's head, on its node.
		update_head,
	};

	/// \brief A place on the line: the pull request's base, or an update's
	/// base or head.
	struct update_line_place
	{
		update_line_place_kind Kind   = update_line_place_kind::pull_request_base;
		std::size_t            Update = 0U;

		auto operator<=>(const update_line_place&) const = default;
	};

	/// \brief What the line knows: the pull request's base anchor, when the
	/// core has published one, and the updates, oldest first.
	struct update_line_facts
	{
		std::optional<comparison_anchor_identity> PullRequestBase;
		std::span<const update_line_update>       Updates;
	};

	/// \brief Where an exact compared anchor sits, or nothing when the line
	/// does not hold it (a review commit, another authority, a paged-out epoch).
	[[nodiscard]] auto place_on_update_line(
		const update_line_facts&          aFacts,
		const comparison_anchor_identity& aAnchor) noexcept -> std::optional<update_line_place>;

	/// \brief The places a handle can be put on, in line order.
	[[nodiscard]] auto update_line_stops(
		const update_line_facts& aFacts,
		comparison_side          aSide) -> std::vector<update_line_place>;

	/// \brief The stop one step from `aCurrent` towards the line's start
	/// (`aDirection` < 0) or its end; nothing at the end of the line. With no
	/// current place the step goes to the first or the last stop.
	[[nodiscard]] auto step_on_update_line(
		const update_line_facts&                aFacts,
		comparison_side                         aSide,
		const std::optional<update_line_place>& aCurrent,
		int                                     aDirection) -> std::optional<update_line_place>;

	/// \brief The exact anchor a place stands for, when it is retained: the
	/// pull request's base, or a retained epoch's base or head. An update GitHub
	/// recorded and this install has not imported has none.
	[[nodiscard]] auto update_line_anchor(
		const update_line_facts& aFacts,
		const update_line_place& aPlace) noexcept -> std::optional<comparison_anchor_identity>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_UPDATE_LINE_HPP
