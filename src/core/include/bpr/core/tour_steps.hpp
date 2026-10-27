#ifndef BPR_CORE_TOUR_STEPS_HPP
#define BPR_CORE_TOUR_STEPS_HPP

#include "bpr/core/review_tour.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

/// \file
/// \brief [[BPR-1784]]. The tour's three steps, Problem, Solution and Stops,
/// and where each stands.
///
/// \details The owner, 2026-10-27: "make it more clear that the steps are fill
/// out problem, fill out solution, fill out stops." The Tour tab draws the
/// steps as the agent setup draws its own (RFC-BPR-0092): a numbered circle
/// per step, the current one open, done ones folded with Change. What a step's
/// state is follows from the draft and the step the author has open; the
/// adapter presents it and keeps which step is open.
namespace bpr::core {
	/// \brief The three steps of writing a tour, in order.
	enum class tour_step : std::uint8_t {
		problem,
		solution,
		stops,
	};

	/// \brief Three. Every switch over `tour_step` omits a `default` label.
	inline constexpr std::size_t tour_step_count = 3U;

	static_assert(static_cast<std::size_t>(tour_step::stops) + 1U == tour_step_count, "tour_step_count must cover every declared step");

	/// \brief Where a step stands.
	enum class tour_step_state : std::uint8_t {
		/// Not written yet and not passed: a quiet numbered circle.
		to_do,
		/// Open for writing.
		current,
		/// Written: a green tick, folded to what was written, with Change.
		done,
		/// Passed while still unwritten, or a stop needs the author: a ring
		/// with "!" that opens the step.
		needs_attention,
	};

	/// \brief Four. Every switch over `tour_step_state` omits a `default` label.
	inline constexpr std::size_t tour_step_state_count = 4U;

	static_assert(static_cast<std::size_t>(tour_step_state::needs_attention) + 1U == tour_step_state_count, "tour_step_state_count must cover every declared state");

	struct tour_steps
	{
		/// Indexed by `tour_step`.
		std::array<tour_step_state, tour_step_count> States{ };
		tour_step                                    Current = tour_step::problem;
		/// The stops, and how many of them the author has marked Ready.
		std::size_t Stops      = 0U;
		std::size_t ReadyStops = 0U;

		[[nodiscard]] auto state(const tour_step aStep) const noexcept -> tour_step_state { return States[static_cast<std::size_t>(aStep)]; }
		auto               operator==(const tour_steps&) const -> bool = default;
	};

	[[nodiscard]] auto stable_name(tour_step aStep) noexcept -> std::string_view;
	[[nodiscard]] auto stable_name(tour_step_state aState) noexcept -> std::string_view;

	/// \brief The step after `aStep`, or none after Stops.
	[[nodiscard]] auto next_tour_step(tour_step aStep) noexcept -> std::optional<tour_step>;

	/// \brief Decides each step's state.
	///
	/// \param aDraft The tour draft, or null while none is loaded.
	/// \param aOpen The step the author has open, or none to take the first
	///        step not yet written (Stops once Problem and Solution are).
	///
	/// \details Problem and Solution are done once they hold more than
	/// whitespace. Stops are done once there is at least one and every one is
	/// marked Ready; they need attention while any stop does, or the draft
	/// holds a website conflict. The open step is current whatever its
	/// progress. A step before it that is still unwritten needs attention,
	/// since publishing needs it; one after it is still to do. Moving on is
	/// the author's choice, so no state ever forbids opening a step.
	[[nodiscard]] auto decide_tour_steps(const review_tour* aDraft, std::optional<tour_step> aOpen) noexcept -> tour_steps;
} // namespace bpr::core

#endif
