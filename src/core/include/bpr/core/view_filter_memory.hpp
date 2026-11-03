#ifndef BPR_CORE_VIEW_FILTER_MEMORY_HPP
#define BPR_CORE_VIEW_FILTER_MEMORY_HPP

#include <cstdint>
#include <optional>

/// \file
/// \brief [[BPR-1844]]. The filters Discuss, Threads and Review's Comments
/// open with, remembered for the profile.
/// \details The owner, 2026-11-03: "I want my Discuss and Threads view and
/// Review Comments view filter settings to be persistent across PRs and
/// launches (although still allow windows/tabs to diverge active filters)."
///
/// Two things are kept apart here. A window's own filters
/// (`view_filter_state`) are what each of its three views shows now; nothing
/// outside the window changes them. The profile's memory
/// (`view_filter_defaults`) is each view's last choice, made in whichever
/// window chose last. A window takes the memory when it opens a pull request,
/// and a reviewer's own choice in a window is remembered for the next one.
/// A choice is never pushed into another window that is already open.
///
/// What is remembered is Only new and Hide resolved. A view's search, the
/// Threads label filter, Discuss's scope and file and an Overview count's
/// listing name things in one pull request and are not carried to the next.
namespace bpr::core {
	/// \brief The views whose filters are remembered.
	enum class filter_view : std::uint8_t {
		/// Discuss: every thread in the pull request, in file order.
		discuss,
		/// Threads: the finder over every conversation.
		threads,
		/// Review's Comments, beside the code.
		review_comments,
	};

	/// \brief One view's two remembered filters.
	struct view_filter_choice
	{
		/// Only threads with a post newer than what I have read.
		bool OnlyNew = false;
		/// Hide resolved threads.
		bool HideResolved = false;

		auto operator<=>(const view_filter_choice&) const = default;
	};

	/// \brief The profile's memory: each view's last choice, or none when no
	/// reviewer has chosen there yet.
	struct view_filter_defaults
	{
		std::optional<view_filter_choice> Discuss;
		std::optional<view_filter_choice> Threads;
		std::optional<view_filter_choice> ReviewComments;

		auto operator<=>(const view_filter_defaults&) const = default;
	};

	/// \brief One window's filters, view by view.
	struct view_filter_state
	{
		view_filter_choice Discuss;
		view_filter_choice Threads;
		view_filter_choice ReviewComments;

		auto operator<=>(const view_filter_state&) const = default;
	};

	/// \brief A choice to remember: the view and what was chosen there.
	struct view_filter_remember
	{
		filter_view        View = filter_view::discuss;
		view_filter_choice Choice;

		auto operator<=>(const view_filter_remember&) const = default;
	};

	/// \brief A window's filters after a choice, and what to remember.
	struct view_filter_transition
	{
		view_filter_state                   State;
		std::optional<view_filter_remember> Remember;

		auto operator<=>(const view_filter_transition&) const = default;
	};

	/// \brief One view's filters as a window holds them.
	[[nodiscard]] auto filters_of(const view_filter_state& aState, filter_view aView) noexcept -> view_filter_choice;

	/// \brief A window opening a pull request: each view takes its remembered
	/// choice, or nothing filtered.
	/// \details Review's Comments without a remembered choice takes
	/// \p aReviewCommentsFirst, the Settings preference that named its first
	/// filter before choices were remembered ([[BPR-0661]]).
	[[nodiscard]] auto open_view_filters(const view_filter_defaults& aDefaults, view_filter_choice aReviewCommentsFirst) noexcept -> view_filter_state;

	/// \brief A reviewer's own choice in one view of one window.
	/// \details The window shows it and it is remembered, even when the window
	/// already showed it: the last reviewer to choose decides what the next
	/// window opens with.
	[[nodiscard]] auto choose_view_filter(const view_filter_state& aState, filter_view aView, view_filter_choice aChoice) noexcept -> view_filter_transition;

	/// \brief A change the window made by itself, such as clearing a filter
	/// that hid the thread a link asked for. Shown, never remembered.
	[[nodiscard]] auto present_view_filter(const view_filter_state& aState, filter_view aView, view_filter_choice aChoice) noexcept -> view_filter_state;

	/// \brief The memory after one choice; every other view keeps its own.
	[[nodiscard]] auto remember(const view_filter_defaults& aDefaults, const view_filter_remember& aChoice) noexcept -> view_filter_defaults;

	/// \brief The stored form's version this build reads and writes.
	inline constexpr std::int64_t view_filter_memory_version = 1;

	/// \brief One view's choice as stored: bit 0 Only new, bit 1 Hide resolved.
	[[nodiscard]] auto stored_view_filter(view_filter_choice aChoice) noexcept -> std::int64_t;

	/// \brief A stored choice, or none for a value no version wrote.
	[[nodiscard]] auto read_stored_view_filter(std::int64_t aStored) noexcept -> std::optional<view_filter_choice>;

	/// \brief The stored row as a store reads it: its version and each view's
	/// value, none where the view has nothing stored.
	struct stored_view_filter_row
	{
		std::int64_t                Version = view_filter_memory_version;
		std::optional<std::int64_t> Discuss;
		std::optional<std::int64_t> Threads;
		std::optional<std::int64_t> ReviewComments;

		auto operator<=>(const stored_view_filter_row&) const = default;
	};

	/// \brief What reading a stored row gave.
	struct view_filter_memory_read
	{
		view_filter_defaults Defaults;
		/// Something stored could not be read and was left out: a damaged
		/// row, an unknown value or a version older than any this build knows.
		bool Recovered = false;
		/// False when a newer build wrote the row: it is read as nothing
		/// remembered and kept as it is, so that build finds it again.
		bool Writable = true;

		auto operator<=>(const view_filter_memory_read&) const = default;
	};

	/// \brief Reads a stored row; none when nothing is stored.
	/// \details Every value is checked on its own: one view's unknown value
	/// leaves that view with nothing remembered and the others as stored.
	[[nodiscard]] auto read_view_filter_memory(const std::optional<stored_view_filter_row>& aRow) noexcept -> view_filter_memory_read;

	/// \brief The row this build writes for the memory.
	[[nodiscard]] auto stored_view_filter_memory(const view_filter_defaults& aDefaults) noexcept -> stored_view_filter_row;
}

#endif
