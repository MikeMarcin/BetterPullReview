#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_KEYBOARD_SHORTCUTS_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_KEYBOARD_SHORTCUTS_HPP

#include <QKeySequence>
#include <QList>
#include <QString>

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

class QKeyEvent;

namespace bpr::qt {
	/// \brief [[BPR-1567]]. The area of the application a shortcut belongs to,
	/// in the order Help → Keyboard Shortcuts lists them.
	enum class shortcut_group : std::uint8_t {
		navigation,
		review,
		comments,
		find,
		views,
		comparison,
		zoom,
		tours,
		home,
		review_cycles,
		application,
	};

	/// \brief [[BPR-1567]]. How a key reaches its command.
	enum class shortcut_binding : std::uint8_t {
		/// A `QAction`, `QShortcut` or button carries the key. Its objects are
		/// named in `shortcut_entry::Objects`, and the window has them.
		bound,
		/// A widget reads the key in its own key handling. `Objects` names any
		/// menu item that only shows it; such an item is never the binding.
		read,
	};

	/// \brief [[BPR-1567]]. Every keyboard command the application binds or
	/// reads. One value, one row of Help → Keyboard Shortcuts.
	enum class shortcut_command : std::uint8_t {
		previous_file,
		next_file,
		previous_change,
		next_change,
		first_change,
		last_change,
		header_source,
		next_new_comment,
		home,
		next_tab,
		previous_tab,
		close_tab,
		line_cursor,
		extend_selection,
		comparison_ends,
		/// [[BPR-1677]]. The top or bottom row of Overview's files.
		files_ends,

		accept_file,
		private_progress,
		compose_review,
		finish_review,
		open_on_github,
		fold_description,

		comment,
		add_comment,
		suggest_change,
		draft_main,
		draft_single,
		draft_to_discuss,
		leave_composer,
		label_menu,
		label_key,
		thread_open_review,
		thread_open_discuss,

		find,
		find_next,
		find_previous,
		find_in_files,
		next_result,
		previous_result,
		query_next,
		query_previous,
		query_whole_word,
		query_in_selection,

		files_drawer,
		comments_drawer,
		repositories_drawer,
		/// [[BPR-1594]]. The Agent tab, opening its drawer.
		open_agent,
		/// [[BPR-1594]]. Ask the agent about the selected lines.
		ask_agent,
		/// [[BPR-1592]]. A private note on the selected line, in Notes.
		new_note,
		review_details,
		line_numbers,
		hide_drawers,
		/// [[BPR-1588]]. The Overview's Status drawer.
		status_drawer,
		overview,
		review,
		discuss,
		threads,
		find_thread,
		show_in_discuss,
		show_in_review,
		show_in_threads,

		whole_pr,
		previous_upload,
		last_reviewed,
		load_earlier,
		return_newest,
		refresh_history,
		prune_epochs,
		delete_cache,
		cancel_refresh,
		cancel_maintenance,

		zoom_in,
		zoom_out,
		actual_size,

		prepare_tour,
		add_tour_stop,
		tour_move_up,
		tour_move_down,
		tour_duplicate,
		tour_remove,
		tour_retarget,
		tour_ready_next,
		tour_undo,
		tour_redo,

		home_open_bar,
		home_open_review,
		home_open_window,
		home_resume,
		/// [[BPR-1600]]. Home's refresh, on the Refresh key while Home shows.
		home_refresh,

		cycle_note,
		cycle_suggest,
		cycle_save,
		cycle_cancel,
		cycle_share_earlier,
		cycle_share_later,
		cycle_sheet_close,

		keyboard_shortcuts,
		settings,
		insights,
		settings_find,
		new_window,
		quit,
		connect,
		open_pull_request,
		refresh,
	};

	/// \brief [[BPR-1567]]. One keyboard command: its keys, where it acts and
	/// the objects bound with it.
	///
	/// \details The catalogue is the one place a key is written down. The
	/// window, the resident desktop menu bar and the widgets that bind a key
	/// take it from here (`shortcut_keys`), and Help → Keyboard Shortcuts is
	/// generated from here, so the page cannot list a key the application does
	/// not bind. `Keys` is Qt's portable text, `;`-separated; a standard key
	/// (`Standard`) is the platform's first binding of it, as
	/// `QKeySequence{ QKeySequence::Find }` is. `Objects` is `;`-separated
	/// object names: the window's binding first, then the resident menu bar's
	/// disabled mirror and any menu item that shows the key.
	struct shortcut_entry
	{
		shortcut_command Command;
		shortcut_group   Group;
		shortcut_binding Binding;
		std::string_view Title;
		/// Where the key acts, when that is not anywhere in the window.
		std::string_view          Place;
		std::string_view          Keys;
		QKeySequence::StandardKey Standard = QKeySequence::UnknownKey;
		/// Every platform binding of `Standard`, as `QShortcut{ StandardKey }`
		/// binds, rather than the first.
		bool EveryBinding = false;
		/// How many of the keys the page shows; the rest are equivalents,
		/// such as the keypad's Enter for Return. Zero shows them all.
		std::uint8_t Shown = 0U;
		/// The binding is made only in some states: Accept file while the
		/// file can be accepted, First and Last change outside the tour
		/// outline. A live object may then carry no key.
		bool Conditional = false;
		/// The key is left to a widget the keyboard is typing in, on every
		/// platform ([[BPR-1295]], [[BPR-1328]], [[BPR-1531]], [[BPR-1532]]).
		bool             StepsAsideWhileTyping = false;
		std::string_view Objects;
	};

	/// \brief [[BPR-1567]]. Every entry, in the page's order: by group, then as
	/// written.
	[[nodiscard]] auto shortcut_catalogue() -> std::span<const shortcut_entry>;

	/// \brief The entry of one command.
	[[nodiscard]] auto shortcut(shortcut_command aCommand) -> const shortcut_entry&;

	/// \brief The keys a command binds, on this platform, in order. Empty when
	/// the platform has none (Quit on Windows).
	[[nodiscard]] auto shortcut_keys(shortcut_command aCommand) -> QList<QKeySequence>;

	/// \brief The keys an entry binds, on this platform, in order.
	[[nodiscard]] auto shortcut_keys(const shortcut_entry& aEntry) -> QList<QKeySequence>;

	/// \brief A command's first key, which tooltips name.
	[[nodiscard]] auto shortcut_key(shortcut_command aCommand) -> QKeySequence;

	/// \brief The object names bound with, or showing, an entry's keys.
	[[nodiscard]] auto shortcut_objects(const shortcut_entry& aEntry) -> QList<QString>;

	/// \brief The heading a group has on the page.
	[[nodiscard]] auto shortcut_group_title(shortcut_group aGroup) -> QString;

	/// \brief Whether the keyboard in a text field takes an entry's key for
	/// itself on this platform: the entry says so, or, on macOS, every key is
	/// Option with a character and acts window-wide ([[BPR-1536]],
	/// [[BPR-PENDING-D]]).
	[[nodiscard]] auto steps_aside_while_typing(const shortcut_entry& aEntry) -> bool;

	/// \brief A key in words a screen reader speaks: "Option-Shift-Up Arrow"
	/// on macOS, the native text elsewhere.
	[[nodiscard]] auto spoken_key(const QKeySequence& aKey) -> QString;

	/// \brief [[BPR-1328]] [[BPR-1677]]. The start or end of a document or
	/// list a key press goes to.
	enum class document_edge : std::uint8_t {
		start,
		end,
	};

	/// \brief [[BPR-1328]] [[BPR-1677]]. The end a key press goes to, if it is
	/// the platform's document start or end.
	///
	/// \details `QKeySequence::MoveToStartOfDocument` and
	/// `MoveToEndOfDocument` (Ctrl+Home and Ctrl+End on Windows and Linux), and
	/// on macOS Command-Up and Command-Down under every platform plugin, the
	/// offscreen one too, whose key scheme is not macOS's. The code panes
	/// scroll the comparison to it and Overview's files move to their top or
	/// bottom row; the catalogue's `comparison_ends` and `files_ends` name
	/// the same keys.
	[[nodiscard]] auto document_edge_for(const QKeyEvent& aEvent) -> std::optional<document_edge>;
}
#endif
