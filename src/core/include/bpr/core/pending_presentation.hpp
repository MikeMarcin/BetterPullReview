#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_PENDING_PRESENTATION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_PENDING_PRESENTATION_HPP

/// \file
/// \brief [[BPR-1576]]. How a pane that waits on data says so: the
/// application's one loading-state pattern.
///
/// \details The owner, 2026-10-17: "We need a clearer visual signal in panes
/// when data is not yet ready. Be that dimming what's there or showing loading
/// spinners or whatever you think the base visual UX design is." And: "This is
/// especially true of the overview page when opening a PR."
///
/// Every pane that waits on a read, a preparation or a decode describes its
/// wait with the same few facts: whether it is loading or failed, whether it
/// already shows accepted content, whether that content is only part of what
/// is coming, and how long the wait has lasted by the presenter's monotonic
/// clock. This decides, from those facts alone, which of a fixed set of looks
/// the pane takes, so every pane in the application reads the same way. The
/// facts come from real pending and ready state in the core and host
/// projections; nothing here invents progress, and the elapsed time only
/// decides when a wait that is really happening is said.

#include <compare>
#include <cstdint>

namespace bpr::core {
	/// \brief How long a first load stays quiet, showing only its placeholders,
	/// before its turning mark and words appear. A load that finishes sooner
	/// never flashes a spinner. The presenter measures it; the core names it.
	inline constexpr std::uint32_t pending_reveal_milliseconds = 400U;

	/// \brief How long a wait lasts before its words say what it still waits on.
	inline constexpr std::uint32_t pending_slow_milliseconds = 6'000U;

	/// \brief What the pane's own data is doing.
	enum class pending_activity : std::uint8_t {
		/// Nothing is being read for this pane.
		idle,
		/// A read, preparation or decode for this pane is under way.
		loading,
		/// The last read for this pane failed and nothing has replaced it.
		failed,
	};

	/// \brief One pane's wait, as the presenter observes it.
	struct pending_facts
	{
		pending_activity Activity = pending_activity::idle;
		/// Accepted content is on screen: a previous read kept through a
		/// refresh or a failure, or the part of this read that has arrived.
		bool HasContent = false;
		/// What is on screen is only part of what this read brings, so the
		/// pane says what is still coming rather than dimming what arrived.
		bool Partial = false;
		/// Since the activity became `loading`, by the presenter's monotonic
		/// clock. Ignored for any other activity.
		std::uint32_t ElapsedMilliseconds = 0U;

		auto operator<=>(const pending_facts&) const = default;
	};

	/// \brief The look a waiting pane takes. Every pane uses the same set.
	enum class pending_look : std::uint8_t {
		/// Nothing is pending: the content as it is.
		ready,
		/// A first load before the reveal: placeholders where the content will
		/// stand, and nothing else, so a fast load does not flash.
		placeholder,
		/// A first load after the reveal: placeholders, the turning mark and
		/// one line naming what is loading.
		loading,
		/// A first load past the slow mark: as `loading`, and the line says what
		/// the read still waits on.
		loading_slow,
		/// Content is on screen and more is coming, before the reveal: the
		/// content exactly as it is. A refresh's own control has already
		/// acknowledged the gesture, so a fast refresh does not flash the pane.
		quiet,
		/// Part of the content is shown; one quiet line with the turning mark
		/// says what is still coming. What arrived is never dimmed.
		partial,
		/// Retained content is being replaced: kept readable under a light veil,
		/// with a thin progress rule along the pane's top edge. Nothing is blocked.
		refreshing,
		/// As `refreshing`, past the slow mark: the rule carries words that say
		/// what the read still waits on.
		refreshing_slow,
		/// Nothing to show and the read failed: one sentence where the content
		/// would be, and Retry.
		failed,
		/// Retained content and the read that would replace it failed: the
		/// content at full strength, a failure strip above it, and Retry.
		failed_retained,
	};

	/// \brief Decides the look of one waiting pane from its facts.
	[[nodiscard]] auto decide_pending_look(const pending_facts& aFacts) noexcept -> pending_look;

	/// \brief Whether the pane is busy, as assistive technology is told.
	/// \details True for every look of a loading pane, including the quiet
	/// ones: a screen reader is told the truth from the first moment even
	/// though nothing is drawn yet.
	[[nodiscard]] auto pending_busy(pending_look aLook) noexcept -> bool;

	/// \brief Whether the turning mark is drawn: only once a wait is revealed.
	[[nodiscard]] auto pending_turning(pending_look aLook) noexcept -> bool;

	/// \brief Whether retained content is drawn under the veil.
	[[nodiscard]] auto pending_veiled(pending_look aLook) noexcept -> bool;

	/// \brief Whether the look offers Retry.
	[[nodiscard]] auto pending_retry(pending_look aLook) noexcept -> bool;

	/// \brief Whether the words say what the wait is still on.
	[[nodiscard]] auto pending_slow(pending_look aLook) noexcept -> bool;

	/// \brief What one change of look says to a screen reader, at most once.
	enum class pending_announcement : std::uint8_t {
		none,
		/// A wait was revealed: "Loading files".
		started,
		/// A revealed wait became slow: what it still waits on.
		slow,
		/// A revealed wait ended with content: "Files loaded".
		finished,
		/// The wait ended in failure: why, and that Retry is offered.
		failed,
	};

	/// \brief The announcement one change of look makes.
	/// \details A wait that never showed (a fast load) is never announced,
	/// neither its start nor its end, so a reader is not told about something
	/// a sighted reviewer never saw. A failure is always announced.
	[[nodiscard]] auto pending_announcement_between(pending_look aBefore, pending_look aAfter) noexcept
		-> pending_announcement;

	/// \brief How long, from `aFacts`, until the look would change by time
	/// alone; zero when no later look depends on time.
	/// \details Lets a presenter wake exactly when the reveal or the slow mark
	/// is reached instead of polling.
	[[nodiscard]] auto pending_next_change_milliseconds(const pending_facts& aFacts) noexcept -> std::uint32_t;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_PENDING_PRESENTATION_HPP
