#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_HOME_OPEN_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_HOME_OPEN_HPP

/// \file
/// \brief [[BPR-1357]]. What Home shows of a pull request it was asked to open.
///
/// \details The owner, 2026-10-09: "There's no obvious immediate indication you
/// actually tried to open a review if there is real network latency on the
/// action." A first read stays on Home until GitHub answers ([[BPR-1200]]), so
/// with a slow network a click on a row changed nothing the reviewer could see.
///
/// This is the decision behind what Home shows instead. It follows one read by
/// the request identity the GitHub transition gave it: opening from the gesture
/// on, waiting once the application's clock says the read has taken longer than
/// `home_open_wait_milliseconds`, and failed with its reason. A loaded or
/// cancelled read leaves nothing to show. Every observation names its request,
/// so a late one for another read changes nothing.

#include "bpr/core/github_model.hpp"

#include <compare>
#include <cstdint>
#include <optional>

namespace bpr::core {
	/// \brief How long a read may take before Home says which step it is on and
	/// offers Cancel. The application measures it; the core only names it.
	inline constexpr std::uint32_t home_open_wait_milliseconds = 800U;

	/// \brief Where a pull request Home asked for stands.
	enum class home_open_phase : std::uint8_t {
		/// Requested; said at once on the control that asked.
		opening,
		/// Still unanswered after the wait: the step it is on, and Cancel.
		waiting,
		/// The read failed: the reason, and Retry where it can help.
		failed,
	};

	/// \brief One pull request Home asked for, by its read's request identity.
	struct home_open_state
	{
		request_id          Request;
		repository_locator  Repository;
		pull_request_number Number;
		home_open_phase     Phase  = home_open_phase::opening;
		github_reason_code  Reason = github_reason_code::none;

		auto operator<=>(const home_open_state&) const = default;
	};

	/// \brief The application's clock: the wait for one read has passed.
	struct home_open_waited
	{
		request_id Request;

		auto operator<=>(const home_open_waited&) const = default;
	};

	/// \brief Applies one GitHub transition's read to what Home shows.
	///
	/// \details A first read's pending stage starts a new opening and replaces
	/// whatever was shown. Its loaded or cancelled stage ends it; its failed
	/// stage keeps it as a failure. A refresh is not an opening and changes
	/// nothing, and a stage for another request changes nothing.
	[[nodiscard]] auto apply_home_open(
		const std::optional<home_open_state>& aState,
		const github_transition_output&       aTransition) -> std::optional<home_open_state>;

	/// \brief Applies the application's wait: an opening of that same request
	/// becomes waiting; anything else is unchanged.
	[[nodiscard]] auto apply_home_open(
		const std::optional<home_open_state>& aState,
		const home_open_waited&               aWaited) -> std::optional<home_open_state>;

	/// \brief Keeps what Home shows true to the read lane it follows.
	///
	/// \details A lane can drop its pending read without a transition of its
	/// own: a retired session resets it. An opening or a waiting whose request
	/// is no longer the lane's pending read ends; a failure is kept.
	[[nodiscard]] auto reconcile_home_open(
		const std::optional<home_open_state>& aState,
		const github_state&                   aLane) -> std::optional<home_open_state>;

	/// \brief Whether Cancel is offered: only while waiting, so the first
	/// moments of an ordinary open stay quiet.
	[[nodiscard]] auto home_open_cancellable(const std::optional<home_open_state>& aState) noexcept -> bool;

	/// \brief Whether a cancel naming `aRequest` is for the read Home shows,
	/// and that read is still the lane's pending one.
	[[nodiscard]] auto admit_home_open_cancel(
		const std::optional<home_open_state>& aState,
		const github_state&                   aLane,
		request_id                            aRequest) noexcept -> bool;

	/// \brief Whether Retry is offered for a failure: not when the name itself
	/// was refused, since asking again cannot change that answer.
	[[nodiscard]] auto home_open_retryable(const std::optional<home_open_state>& aState) noexcept -> bool;

	/// \brief Whether a gesture naming this pull request is the one already
	/// being opened, so a second click or Return asks for nothing.
	[[nodiscard]] auto home_open_in_flight(
		const std::optional<home_open_state>& aState,
		const repository_locator&             aRepository,
		pull_request_number                   aNumber) noexcept -> bool;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_HOME_OPEN_HPP
