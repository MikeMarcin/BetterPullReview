#include "bpr/core/tour_steps.hpp"

#include <algorithm>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto written(const std::string_view aText) noexcept -> bool
		{
			return std::ranges::any_of(aText, [](const char aCharacter) { return aCharacter != ' ' && aCharacter != '\t' && aCharacter != '\n' && aCharacter != '\r' && aCharacter != '\f' && aCharacter != '\v'; });
		}
	}

	auto stable_name(const tour_step aStep) noexcept -> std::string_view
	{
		switch (aStep) {
		case tour_step::problem:  return "problem";
		case tour_step::solution: return "solution";
		case tour_step::stops:    return "stops";
		}
		return "unknown";
	}

	auto stable_name(const tour_step_state aState) noexcept -> std::string_view
	{
		switch (aState) {
		case tour_step_state::to_do:           return "to_do";
		case tour_step_state::current:         return "current";
		case tour_step_state::done:            return "done";
		case tour_step_state::needs_attention: return "needs_attention";
		}
		return "unknown";
	}

	auto next_tour_step(const tour_step aStep) noexcept -> std::optional<tour_step>
	{
		switch (aStep) {
		case tour_step::problem:  return tour_step::solution;
		case tour_step::solution: return tour_step::stops;
		case tour_step::stops:    return std::nullopt;
		}
		return std::nullopt;
	}

	auto decide_tour_steps(const review_tour* const aDraft, const std::optional<tour_step> aOpen) noexcept -> tour_steps
	{
		auto result = tour_steps{ };
		// Progress first: what the draft holds, whichever step is open.
		auto progress = std::array{ tour_step_state::to_do, tour_step_state::to_do, tour_step_state::to_do };
		if (aDraft != nullptr) {
			result.Stops      = aDraft->Stops.size();
			result.ReadyStops = static_cast<std::size_t>(std::ranges::count(aDraft->Stops, tour_preparation::ready, &tour_stop::Preparation));
			if (written(aDraft->Problem)) {
				progress[static_cast<std::size_t>(tour_step::problem)] = tour_step_state::done;
			}
			if (written(aDraft->Solution)) {
				progress[static_cast<std::size_t>(tour_step::solution)] = tour_step_state::done;
			}
			const auto attention = !aDraft->Conflicts.empty() || std::ranges::any_of(aDraft->Stops, [](const tour_stop& aStop) { return aStop.Preparation == tour_preparation::needs_attention; });
			if (attention) {
				progress[static_cast<std::size_t>(tour_step::stops)] = tour_step_state::needs_attention;
			} else if (result.Stops > 0U && result.ReadyStops == result.Stops) {
				progress[static_cast<std::size_t>(tour_step::stops)] = tour_step_state::done;
			}
		}
		if (aOpen) {
			result.Current = *aOpen;
		} else if (progress[static_cast<std::size_t>(tour_step::problem)] != tour_step_state::done) {
			result.Current = tour_step::problem;
		} else if (progress[static_cast<std::size_t>(tour_step::solution)] != tour_step_state::done) {
			result.Current = tour_step::solution;
		} else {
			result.Current = tour_step::stops;
		}
		const auto current = static_cast<std::size_t>(result.Current);
		for (auto index = std::size_t{ 0U }; index < tour_step_count; ++index) {
			if (index == current) {
				result.States[index] = tour_step_state::current;
			} else if (progress[index] == tour_step_state::to_do && index < current) {
				result.States[index] = tour_step_state::needs_attention;
			} else {
				result.States[index] = progress[index];
			}
		}
		return result;
	}
} // namespace bpr::core
