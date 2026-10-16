#include "bpr/core/update_line.hpp"

#include <algorithm>

namespace bpr::core {
	namespace {
		/// \brief The place's position along the line: the base first, then
		/// each update's base and head, oldest first.
		[[nodiscard]] constexpr auto line_order(const update_line_place& aPlace) noexcept -> std::size_t
		{
			switch (aPlace.Kind) {
			case update_line_place_kind::pull_request_base:
				return 0U;
			case update_line_place_kind::update_base:
				return (2U * aPlace.Update) + 1U;
			case update_line_place_kind::update_head:
				return (2U * aPlace.Update) + 2U;
			}
			return 0U;
		}

		[[nodiscard]] auto same_epoch(const review_epoch_id& aEpoch, const comparison_anchor_identity& aAnchor) noexcept -> bool
		{
			return aEpoch.Authority == aAnchor.Authority && aEpoch.Incarnation == aAnchor.Incarnation && aEpoch.Ordinal == aAnchor.Epoch;
		}
	} // namespace

	auto place_on_update_line(const update_line_facts& aFacts, const comparison_anchor_identity& aAnchor) noexcept
		-> std::optional<update_line_place>
	{
		if (aFacts.PullRequestBase && *aFacts.PullRequestBase == aAnchor) {
			return update_line_place{ .Kind = update_line_place_kind::pull_request_base };
		}
		if (aAnchor.Kind != comparison_anchor_kind::epoch_base && aAnchor.Kind != comparison_anchor_kind::epoch_head) {
			return std::nullopt;
		}
		for (std::size_t index = 0U; index < aFacts.Updates.size(); ++index) {
			const auto& epoch = aFacts.Updates[index].Epoch;
			if (epoch && same_epoch(*epoch, aAnchor)) {
				return update_line_place{
					.Kind   = aAnchor.Kind == comparison_anchor_kind::epoch_base ? update_line_place_kind::update_base : update_line_place_kind::update_head,
					.Update = index,
				};
			}
		}
		return std::nullopt;
	}

	auto update_line_stops(const update_line_facts& aFacts, const comparison_side aSide) -> std::vector<update_line_place>
	{
		auto stops = std::vector<update_line_place>{ };
		if (aSide == comparison_side::left) {
			if (aFacts.PullRequestBase) {
				stops.push_back({ .Kind = update_line_place_kind::pull_request_base });
			} else if (!aFacts.Updates.empty() && aFacts.Updates.front().Epoch && aFacts.Updates.front().HasBase) {
				// Before the core has named the pull request's base, the oldest
				// update's own base keeps the whole pull request one step away.
				stops.push_back({ .Kind = update_line_place_kind::update_base, .Update = 0U });
			}
		}
		for (std::size_t index = 0U; index < aFacts.Updates.size(); ++index) {
			if (aFacts.Updates[index].Epoch || aFacts.Updates[index].Importable) {
				stops.push_back({ .Kind = update_line_place_kind::update_head, .Update = index });
			}
		}
		return stops;
	}

	auto step_on_update_line(const update_line_facts&                aFacts,
							 const comparison_side                   aSide,
							 const std::optional<update_line_place>& aCurrent,
							 const int                               aDirection) -> std::optional<update_line_place>
	{
		const auto stops = update_line_stops(aFacts, aSide);
		if (stops.empty() || aDirection == 0) {
			return std::nullopt;
		}
		if (!aCurrent) {
			return aDirection < 0 ? stops.front() : stops.back();
		}
		const auto here = line_order(*aCurrent);
		if (aDirection < 0) {
			const auto before = std::ranges::find_if(stops.rbegin(), stops.rend(), [here](const update_line_place& aStop) {
				return line_order(aStop) < here;
			});
			return before == stops.rend() ? std::nullopt : std::optional{ *before };
		}
		const auto after = std::ranges::find_if(stops, [here](const update_line_place& aStop) {
			return line_order(aStop) > here;
		});
		return after == stops.end() ? std::nullopt : std::optional{ *after };
	}

	auto update_line_anchor(const update_line_facts& aFacts, const update_line_place& aPlace) noexcept
		-> std::optional<comparison_anchor_identity>
	{
		if (aPlace.Kind == update_line_place_kind::pull_request_base) {
			return aFacts.PullRequestBase;
		}
		if (aPlace.Update >= aFacts.Updates.size()) {
			return std::nullopt;
		}
		const auto& update = aFacts.Updates[aPlace.Update];
		if (!update.Epoch || (aPlace.Kind == update_line_place_kind::update_base && !update.HasBase)) {
			return std::nullopt;
		}
		return comparison_anchor_identity{
			.Authority   = update.Epoch->Authority,
			.Incarnation = update.Epoch->Incarnation,
			.Kind        = aPlace.Kind == update_line_place_kind::update_base ? comparison_anchor_kind::epoch_base : comparison_anchor_kind::epoch_head,
			.Epoch       = update.Epoch->Ordinal,
		};
	}
} // namespace bpr::core
