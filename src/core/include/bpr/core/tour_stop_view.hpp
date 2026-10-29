#ifndef BPR_CORE_TOUR_STOP_VIEW_HPP
#define BPR_CORE_TOUR_STOP_VIEW_HPP

#include "bpr/core/review_tour.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

/// \file
/// \brief [[BPR-1805]]. Whether the open tour stop is read or written.
///
/// \details The owner, 2026-10-28: "Tour tabs default view of a stop seems to
/// be edit when you go back to it but read with proper rendering and a pencil
/// to edit would be much more useful". A stop opens as its rendered
/// explanation, the way a comment reads, with a pencil that opens its title
/// and Markdown source; Done goes back to reading. A stop with nothing to read
/// yet opens to be written, and text the draft has not taken keeps the stop
/// open for writing, so nothing typed is ever hidden behind the reader. The
/// adapter presents the mode and keeps the editors' text.
namespace bpr::core {
	/// \brief How the open stop is shown.
	enum class tour_stop_view_mode : std::uint8_t {
		/// The rendered explanation, with a pencil.
		read,
		/// The title and Markdown source, with Done.
		edit,
	};

	/// \brief Two. Every switch over `tour_stop_view_mode` omits a `default` label.
	inline constexpr std::size_t tour_stop_view_mode_count = 2U;

	static_assert(static_cast<std::size_t>(tour_stop_view_mode::edit) + 1U == tour_stop_view_mode_count, "tour_stop_view_mode_count must cover every declared mode");

	/// \brief What moved the view.
	enum class tour_stop_view_event : std::uint8_t {
		/// A stop was opened, or the open one presented again.
		open,
		/// The pencil, or its key.
		begin_edit,
		/// Done, or its key.
		finish_edit,
	};

	/// \brief Three. Every switch over `tour_stop_view_event` omits a `default` label.
	inline constexpr std::size_t tour_stop_view_event_count = 3U;

	static_assert(static_cast<std::size_t>(tour_stop_view_event::finish_edit) + 1U == tour_stop_view_event_count, "tour_stop_view_event_count must cover every declared event");

	/// \brief The open stop and how it is shown.
	struct tour_stop_view
	{
		/// Zero while no stop is open.
		tour_stop_id        Stop;
		tour_stop_view_mode Mode = tour_stop_view_mode::read;

		auto operator==(const tour_stop_view&) const -> bool = default;
	};

	/// \brief The title and explanation an editor holds for the stop.
	struct tour_stop_held_text
	{
		std::string_view Title;
		std::string_view Body;
	};

	struct tour_stop_view_input
	{
		/// The stop to show, or null when there is none.
		const tour_stop*     Stop  = nullptr;
		tour_stop_view_event Event = tour_stop_view_event::open;
		/// The draft can be written: loaded, not read-only.
		bool Editable = true;
		/// What the editors hold for `Stop`, when they hold its text.
		std::optional<tour_stop_held_text> Held;
		/// An input method holds uncommitted preedit text in the editors.
		bool Composing = false;
	};

	[[nodiscard]] auto stable_name(tour_stop_view_mode aMode) noexcept -> std::string_view;
	[[nodiscard]] auto stable_name(tour_stop_view_event aEvent) noexcept -> std::string_view;

	/// \brief True when `aStop`'s explanation holds nothing but whitespace:
	/// there is nothing to read yet.
	[[nodiscard]] auto tour_stop_unwritten(const tour_stop& aStop) noexcept -> bool;

	/// \brief True when the editors hold text the draft has not taken: it
	/// differs from the stop's title or explanation, or is still being
	/// composed.
	[[nodiscard]] auto tour_stop_text_unsaved(const tour_stop_view_input& aInput) noexcept -> bool;

	/// \brief Decides the open stop and its mode after `aInput.Event`.
	///
	/// \details No stop: nothing is open. A draft that cannot be written is
	/// always read. Unsaved text keeps the stop open for writing whatever the
	/// event, so Done and reopening never hide it. Opening the stop already
	/// open keeps its mode, as a save presents it again; opening another stop
	/// reads it, unless its explanation is still unwritten, when it opens to
	/// be written. The pencil writes, Done reads.
	[[nodiscard]] auto decide_tour_stop_view(const tour_stop_view& aCurrent, const tour_stop_view_input& aInput) noexcept -> tour_stop_view;
} // namespace bpr::core

#endif
