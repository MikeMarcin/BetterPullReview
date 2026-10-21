#include "bpr/qt/keyboard_shortcuts.hpp"

#include <QKeyCombination>
#include <QKeyEvent>
#include <QStringList>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>

namespace bpr::qt {
	namespace {
		using enum shortcut_group;
		using enum shortcut_binding;
		using command = shortcut_command;

		/// \brief Where the Updates line's history keys act: the epoch control
		/// binds them with widget-with-children scope.
		constexpr auto in_updates   = std::string_view{ "In the Updates control" };
		constexpr auto in_code      = std::string_view{ "In a code pane" };
		constexpr auto in_files     = std::string_view{ "In Overview's files" };
		constexpr auto in_draft     = std::string_view{ "In a comment draft" };
		constexpr auto in_query     = std::string_view{ "In the Find query" };
		constexpr auto in_outline   = std::string_view{ "In the tour outline" };
		constexpr auto in_editor    = std::string_view{ "In the tour editor" };
		constexpr auto in_cycle     = std::string_view{ "In a Review Cycle's code" };
		constexpr auto in_cycle_box = std::string_view{ "In a Review Cycle note or suggestion" };

		// [[BPR-1678]]. The Files and Repositories drawers' second keys,
		// for automation: their first keys' 1 and R with the platform's command
		// modifier rather than Option, so no text field takes them while you
		// type. Control-Command on macOS, where Command-Shift-3 to 6 are the
		// system's screenshots; Ctrl+Shift elsewhere, where Ctrl+Alt is AltGr.
#if defined(Q_OS_MACOS)
		constexpr auto files_drawer_keys        = std::string_view{ "Alt+Shift+1;Ctrl+Meta+1" };
		constexpr auto repositories_drawer_keys = std::string_view{ "Alt+Shift+R;Ctrl+Meta+R" };
#else
		constexpr auto files_drawer_keys        = std::string_view{ "Alt+Shift+1;Ctrl+Shift+1" };
		constexpr auto repositories_drawer_keys = std::string_view{ "Alt+Shift+R;Ctrl+Shift+R" };
#endif

		// One row per command, in the enumeration's order (checked below).
		// Platform differences are written here and nowhere else.
		constexpr auto catalogue = std::array{
			// Navigation.
			shortcut_entry{ .Command = command::previous_file, .Group = navigation, .Binding = bound, .Title = "Previous file", .Keys = "Ctrl+Alt+Up", .Objects = "previousWorkbenchFileAction;desktopPreviousWorkbenchFileAction" },
			shortcut_entry{ .Command = command::next_file, .Group = navigation, .Binding = bound, .Title = "Next file", .Keys = "Ctrl+Alt+Down", .Objects = "nextWorkbenchFileAction;desktopNextWorkbenchFileAction" },
			shortcut_entry{ .Command = command::previous_change, .Group = navigation, .Binding = bound, .Title = "Previous change", .Keys = "Alt+Up;Alt+PgUp", .StepsAsideWhileTyping = true, .Objects = "previousWorkbenchChangeAction;desktopPreviousWorkbenchChangeAction" },
			shortcut_entry{ .Command = command::next_change, .Group = navigation, .Binding = bound, .Title = "Next change", .Keys = "Alt+Down;Alt+PgDown", .StepsAsideWhileTyping = true, .Objects = "nextWorkbenchChangeAction;desktopNextWorkbenchChangeAction" },
			shortcut_entry{ .Command = command::first_change, .Group = navigation, .Binding = bound, .Title = "First change in the file", .Keys = "Alt+Shift+Up", .Conditional = true, .StepsAsideWhileTyping = true, .Objects = "firstWorkbenchChangeAction;desktopFirstWorkbenchChangeAction" },
			shortcut_entry{ .Command = command::last_change, .Group = navigation, .Binding = bound, .Title = "Last change in the file", .Keys = "Alt+Shift+Down", .Conditional = true, .StepsAsideWhileTyping = true, .Objects = "lastWorkbenchChangeAction;desktopLastWorkbenchChangeAction" },
			shortcut_entry{ .Command = command::header_source, .Group = navigation, .Binding = bound, .Title = "Go to header/source", .Keys = "Alt+O", .Objects = "goToHeaderSourceAction;desktopGoToHeaderSourceAction" },
			shortcut_entry{ .Command = command::next_new_comment, .Group = navigation, .Binding = bound, .Title = "Next new comment", .Keys = "Alt+N", .Objects = "nextNewCommentAction" },
			shortcut_entry{ .Command = command::home, .Group = navigation, .Binding = bound, .Title = "Home, or back to the review", .Keys = "Ctrl+Shift+H", .Objects = "workbenchHomeAction" },
#if defined(Q_OS_MACOS)
			// Control-Tab on macOS too, which Qt names Meta.
			shortcut_entry{ .Command = command::next_tab, .Group = navigation, .Binding = bound, .Title = "Next tab", .Keys = "Meta+Tab", .Objects = "nextDocumentTabAction;desktopNextDocumentTabAction" },
			shortcut_entry{ .Command = command::previous_tab, .Group = navigation, .Binding = bound, .Title = "Previous tab", .Keys = "Meta+Shift+Tab", .Objects = "previousDocumentTabAction;desktopPreviousDocumentTabAction" },
#else
			shortcut_entry{ .Command = command::next_tab, .Group = navigation, .Binding = bound, .Title = "Next tab", .Keys = "Ctrl+Tab", .Objects = "nextDocumentTabAction;desktopNextDocumentTabAction" },
			shortcut_entry{ .Command = command::previous_tab, .Group = navigation, .Binding = bound, .Title = "Previous tab", .Keys = "Ctrl+Shift+Tab", .Objects = "previousDocumentTabAction;desktopPreviousDocumentTabAction" },
#endif
			shortcut_entry{ .Command = command::close_tab, .Group = navigation, .Binding = bound, .Title = "Close the tab", .Keys = "Ctrl+W", .Objects = "closeDocumentTabAction;desktopCloseDocumentTabAction" },
			shortcut_entry{ .Command = command::line_cursor, .Group = navigation, .Binding = read, .Title = "Move the line cursor", .Place = in_code, .Keys = "Up;Down" },
			shortcut_entry{ .Command = command::extend_selection, .Group = navigation, .Binding = read, .Title = "Extend the selected lines", .Place = in_code, .Keys = "Shift+Up;Shift+Down" },
#if defined(Q_OS_MACOS)
			shortcut_entry{ .Command = command::comparison_ends, .Group = navigation, .Binding = read, .Title = "Top or bottom of the comparison", .Place = in_code, .Keys = "Ctrl+Up;Ctrl+Down" },
			shortcut_entry{ .Command = command::files_ends, .Group = navigation, .Binding = read, .Title = "Top or bottom of the files", .Place = in_files, .Keys = "Ctrl+Up;Ctrl+Down" },
#else
			shortcut_entry{ .Command = command::comparison_ends, .Group = navigation, .Binding = read, .Title = "Top or bottom of the comparison", .Place = in_code, .Keys = "Ctrl+Home;Ctrl+End" },
			shortcut_entry{ .Command = command::files_ends, .Group = navigation, .Binding = read, .Title = "Top or bottom of the files", .Place = in_files, .Keys = "Ctrl+Home;Ctrl+End" },
#endif

			// Review and files.
			shortcut_entry{ .Command = command::accept_file, .Group = review, .Binding = bound, .Title = "Accept the file", .Keys = "Alt+A", .Conditional = true, .StepsAsideWhileTyping = true, .Objects = "toggleSelectedFileAcceptanceAction" },
			shortcut_entry{ .Command = command::private_progress, .Group = review, .Binding = bound, .Title = "Private progress: seen, read, accepted, excluded", .Keys = "Alt+Shift+4", .Objects = "workbenchReviewControlsButton" },
			shortcut_entry{ .Command = command::compose_review, .Group = review, .Binding = bound, .Title = "Compose the review", .Keys = "Alt+R", .Objects = "composeReviewAction;desktopComposeReviewAction" },
			shortcut_entry{ .Command = command::finish_review, .Group = review, .Binding = bound, .Title = "Finish the review", .Keys = "Ctrl+Alt+Return;Ctrl+Alt+Enter", .Shown = 1U, .Objects = "finishReviewAction" },
			shortcut_entry{ .Command = command::open_on_github, .Group = review, .Binding = bound, .Title = "Open the pull request on GitHub", .Keys = "Ctrl+Shift+O", .Objects = "openOnGithubAction;desktopOpenOnGithubAction" },
			shortcut_entry{ .Command = command::fold_description, .Group = review, .Binding = bound, .Title = "Fold the description", .Place = "On Overview", .Keys = "Escape", .Objects = "overviewDescriptionEscape" },

			// Comments and labels.
			shortcut_entry{ .Command = command::comment, .Group = comments, .Binding = bound, .Title = "Comment, or reply to the selected thread", .Keys = "Alt+C", .Objects = "composeInlineDraftAction;desktopComposeInlineDraftAction" },
			shortcut_entry{ .Command = command::add_comment, .Group = comments, .Binding = read, .Title = "Comment on the selected lines", .Place = in_code, .Keys = "Return", .Objects = "addCommentAction" },
			shortcut_entry{ .Command = command::suggest_change, .Group = comments, .Binding = read, .Title = "Suggest a change to the selected lines", .Place = in_code, .Keys = "Shift+Return", .Objects = "suggestChangeAction" },
			shortcut_entry{ .Command = command::draft_main, .Group = comments, .Binding = read, .Title = "Start a review, or add the comment to it", .Place = in_draft, .Keys = "Ctrl+Return" },
			shortcut_entry{ .Command = command::draft_single, .Group = comments, .Binding = read, .Title = "Post the comment on its own", .Place = in_draft, .Keys = "Ctrl+Shift+Return", .Objects = "threadReviewDraftSingle" },
			shortcut_entry{ .Command = command::draft_to_discuss, .Group = comments, .Binding = bound, .Title = "Open the draft in Discuss, or back to the code", .Place = in_draft, .Keys = "Ctrl+Shift+M", .Objects = "composeReviewPageRestore;composeInlineDraftPageRestore;composeReviewSheetExpandKey" },
			shortcut_entry{ .Command = command::leave_composer, .Group = comments, .Binding = bound, .Title = "Back from the composer", .Place = "In Discuss's composer", .Keys = "Escape", .Objects = "composeReviewPageEscape;composeInlineDraftPageEscape;workbenchWorkshopHeaderEscape;composeReviewSheetEscape;workbenchTourPageEscape" },
			shortcut_entry{ .Command = command::label_menu, .Group = comments, .Binding = read, .Title = "Choose the comment's label", .Place = "In a comment editor", .Keys = "Ctrl+L" },
			shortcut_entry{ .Command = command::label_key, .Group = comments, .Binding = bound, .Title = "Show the label key", .Place = "On Overview", .Keys = "Alt+K", .Objects = "overviewLabelKeyShortcut" },
			shortcut_entry{ .Command = command::thread_open_review, .Group = comments, .Binding = read, .Title = "Open the thread in Review", .Place = "In Threads", .Keys = "Return", .Objects = "threadFinderOpenInReviewAction" },
			shortcut_entry{ .Command = command::thread_open_discuss, .Group = comments, .Binding = read, .Title = "Open the thread in Discuss", .Place = "In Threads", .Keys = "Shift+Return", .Objects = "threadFinderOpenInDiscussAction" },

			// Find.
			shortcut_entry{ .Command = command::find, .Group = find, .Binding = bound, .Title = "Find in the file", .Standard = QKeySequence::Find, .Objects = "sourceFindAction;desktopSourceFindAction" },
#if defined(Q_OS_MACOS)
			shortcut_entry{ .Command = command::find_next, .Group = find, .Binding = bound, .Title = "Next match", .Keys = "F3;Ctrl+G", .Objects = "sourceFindNextAction;desktopSourceFindNextAction" },
			shortcut_entry{ .Command = command::find_previous, .Group = find, .Binding = bound, .Title = "Previous match", .Keys = "Shift+F3;Ctrl+Shift+G", .Objects = "sourceFindPreviousAction;desktopSourceFindPreviousAction" },
#else
			shortcut_entry{ .Command = command::find_next, .Group = find, .Binding = bound, .Title = "Next match", .Keys = "F3", .Objects = "sourceFindNextAction;desktopSourceFindNextAction" },
			shortcut_entry{ .Command = command::find_previous, .Group = find, .Binding = bound, .Title = "Previous match", .Keys = "Shift+F3", .Objects = "sourceFindPreviousAction;desktopSourceFindPreviousAction" },
#endif
			shortcut_entry{ .Command = command::find_in_files, .Group = find, .Binding = bound, .Title = "Find in files", .Keys = "Ctrl+Shift+F", .Objects = "sourceFilesAction;desktopSourceFilesAction" },
			shortcut_entry{ .Command = command::next_result, .Group = find, .Binding = bound, .Title = "Next result in files", .Keys = "F4", .Objects = "sourceFilesNextAction;desktopSourceFilesNextAction" },
			shortcut_entry{ .Command = command::previous_result, .Group = find, .Binding = bound, .Title = "Previous result in files", .Keys = "Shift+F4", .Objects = "sourceFilesPreviousAction;desktopSourceFilesPreviousAction" },
			shortcut_entry{ .Command = command::query_next, .Group = find, .Binding = read, .Title = "Next match", .Place = in_query, .Keys = "Return" },
			shortcut_entry{ .Command = command::query_previous, .Group = find, .Binding = read, .Title = "Previous match", .Place = in_query, .Keys = "Shift+Return" },
			shortcut_entry{ .Command = command::query_whole_word, .Group = find, .Binding = read, .Title = "Whole word on or off", .Place = in_query, .Keys = "Alt+W" },
			shortcut_entry{ .Command = command::query_in_selection, .Group = find, .Binding = read, .Title = "Find in selection on or off", .Place = in_query, .Keys = "Alt+L" },

			// Views and drawers.
			shortcut_entry{ .Command = command::files_drawer, .Group = views, .Binding = bound, .Title = "Files drawer", .Keys = files_drawer_keys, .Objects = "toggleWorkbenchFilesDrawerAction" },
			shortcut_entry{ .Command = command::comments_drawer, .Group = views, .Binding = bound, .Title = "Comments drawer", .Keys = "Alt+Shift+2", .Objects = "toggleWorkbenchConversationsDrawerAction" },
			shortcut_entry{ .Command = command::repositories_drawer, .Group = views, .Binding = bound, .Title = "Repositories drawer", .Keys = repositories_drawer_keys, .Objects = "toggleWorkbenchRepositoriesDrawerAction" },
			shortcut_entry{ .Command = command::open_agent, .Group = views, .Binding = bound, .Title = "The Agent tab", .Keys = "Alt+Shift+A", .Objects = "openAgentTabAction" },
			shortcut_entry{ .Command = command::ask_agent, .Group = views, .Binding = bound, .Title = "Ask the agent about the selected lines", .Keys = "Ctrl+I", .Objects = "askAgentAboutLinesAction" },
			shortcut_entry{ .Command = command::new_note, .Group = comments, .Binding = bound, .Title = "A private note on the selected line", .Place = "In Notes", .Keys = "N", .Objects = "newLineNoteAction" },
			shortcut_entry{ .Command = command::review_details, .Group = views, .Binding = bound, .Title = "Review details", .Keys = "Alt+Shift+3", .Objects = "toggleWorkbenchReviewDetailsAction" },
			shortcut_entry{ .Command = command::line_numbers, .Group = views, .Binding = bound, .Title = "Line numbers", .Keys = "Alt+Shift+5", .Objects = "toggleWorkbenchLineNumbersAction;desktopToggleLineNumbersAction" },
			shortcut_entry{ .Command = command::hide_drawers, .Group = views, .Binding = bound, .Title = "Hide or show every drawer", .Keys = "Ctrl+B", .StepsAsideWhileTyping = true, .Objects = "toggleWorkbenchDrawersAction;desktopToggleDrawersAction" },
			// [[BPR-1588]]. Alt+Shift+1 to 5 are the other drawers and views.
			shortcut_entry{ .Command = command::status_drawer, .Group = views, .Binding = bound, .Title = "Status drawer", .Place = "On Overview", .Keys = "Alt+Shift+6", .Objects = "toggleOverviewStatusAction" },
			shortcut_entry{ .Command = command::overview, .Group = views, .Binding = bound, .Title = "Overview", .Keys = "Ctrl+1", .Objects = "reviewModalityOverviewAction" },
			shortcut_entry{ .Command = command::review, .Group = views, .Binding = bound, .Title = "Review", .Keys = "Ctrl+2", .Objects = "reviewModalityReviewAction" },
			shortcut_entry{ .Command = command::discuss, .Group = views, .Binding = bound, .Title = "Discuss", .Keys = "Ctrl+3", .Objects = "reviewModalityDiscussAction" },
			shortcut_entry{ .Command = command::threads, .Group = views, .Binding = bound, .Title = "Threads", .Keys = "Ctrl+4", .Objects = "reviewModalityThreadsAction" },
			shortcut_entry{ .Command = command::find_thread, .Group = views, .Binding = bound, .Title = "Find a thread, or go back", .Keys = "Ctrl+J", .Objects = "showWorkbenchThreadsAction;desktopShowThreadsAction" },
			// [[BPR-1581]]. The selected thread in the other view, beside ⌘3 and
			// ⌘2. Ctrl+Alt is AltGr on Windows, so both step aside while typing.
			shortcut_entry{ .Command = command::show_in_discuss, .Group = views, .Binding = bound, .Title = "Show the selected thread in Discuss", .Place = "In Review", .Keys = "Ctrl+Alt+3", .StepsAsideWhileTyping = true, .Objects = "showThreadInDiscussAction;desktopShowThreadInDiscussAction;codeContextMenuShowInDiscuss;threadShowInDiscussAction;postShowInDiscuss" },
			shortcut_entry{ .Command = command::show_in_review, .Group = views, .Binding = bound, .Title = "Show the selected thread in Review", .Place = "In Discuss", .Keys = "Ctrl+Alt+2", .StepsAsideWhileTyping = true, .Objects = "showThreadInReviewAction;desktopShowThreadInReviewAction;threadOpenInReviewAction;postOpenInReview" },
			// [[BPR-1586]]. Its row in Threads, beside ⌘4.
			shortcut_entry{ .Command = command::show_in_threads, .Group = views, .Binding = bound, .Title = "Show the selected thread in Threads", .Place = "In Review", .Keys = "Ctrl+Alt+4", .StepsAsideWhileTyping = true, .Objects = "showThreadInThreadsAction;desktopShowThreadInThreadsAction;codeContextMenuShowInThreads;threadShowInThreadsAction;postShowInThreads;draftShowInThreadsAction" },

			// The comparison and its history.
			shortcut_entry{ .Command = command::whole_pr, .Group = comparison, .Binding = bound, .Title = "Whole pull request", .Place = in_updates, .Keys = "Alt+1", .Objects = "wholePrComparisonAction" },
			shortcut_entry{ .Command = command::previous_upload, .Group = comparison, .Binding = bound, .Title = "Previous upload to latest", .Place = in_updates, .Keys = "Alt+2", .Objects = "previousUploadComparisonAction" },
			shortcut_entry{ .Command = command::last_reviewed, .Group = comparison, .Binding = bound, .Title = "My last reviewed to latest", .Place = in_updates, .Keys = "Alt+3", .Objects = "lastReviewedComparisonAction" },
			shortcut_entry{ .Command = command::load_earlier, .Group = comparison, .Binding = bound, .Title = "Load earlier updates", .Place = in_updates, .Keys = "Ctrl+Alt+PgDown", .Objects = "loadEpochHistoryAction" },
			shortcut_entry{ .Command = command::return_newest, .Group = comparison, .Binding = bound, .Title = "Back to the newest updates", .Place = in_updates, .Keys = "Ctrl+Alt+PgUp", .Objects = "returnToNewestEpochsAction" },
			shortcut_entry{ .Command = command::refresh_history, .Group = comparison, .Binding = bound, .Title = "Refresh the review history", .Place = in_updates, .Keys = "Ctrl+Alt+R", .Objects = "refreshEpochsAction" },
			shortcut_entry{ .Command = command::prune_epochs, .Group = comparison, .Binding = bound, .Title = "Prune the selected updates", .Place = in_updates, .Keys = "Ctrl+Alt+P", .Objects = "pruneSelectedEpochsAction" },
			shortcut_entry{ .Command = command::delete_cache, .Group = comparison, .Binding = bound, .Title = "Delete the repository cache", .Place = in_updates, .Keys = "Ctrl+Alt+Shift+D", .Objects = "deleteRepositoryCacheAction" },
			shortcut_entry{ .Command = command::cancel_refresh, .Group = comparison, .Binding = bound, .Title = "Cancel the history refresh", .Place = in_updates, .Keys = "Escape", .Objects = "cancelEpochRefreshAction" },
			shortcut_entry{ .Command = command::cancel_maintenance, .Group = comparison, .Binding = bound, .Title = "Cancel history maintenance", .Place = in_updates, .Keys = "Ctrl+Alt+Escape", .Objects = "cancelEpochMaintenanceAction" },

			// Zoom ([[BPR-1336]], [[BPR-1535]]).
			shortcut_entry{ .Command = command::zoom_in, .Group = zoom, .Binding = bound, .Title = "Zoom in on the code", .Keys = "Ctrl+=;Ctrl++", .Objects = "zoomInCodeAction;desktopZoomInCodeAction" },
			shortcut_entry{ .Command = command::zoom_out, .Group = zoom, .Binding = bound, .Title = "Zoom out", .Keys = "Ctrl+-", .Objects = "zoomOutCodeAction;desktopZoomOutCodeAction" },
			shortcut_entry{ .Command = command::actual_size, .Group = zoom, .Binding = bound, .Title = "Actual size", .Keys = "Ctrl+0", .Objects = "actualSizeCodeAction;desktopActualSizeCodeAction" },

			// Review tours.
			shortcut_entry{ .Command = command::prepare_tour, .Group = tours, .Binding = bound, .Title = "Prepare a review tour", .Keys = "Alt+T", .Objects = "prepareReviewTourAction;desktopPrepareReviewTourAction" },
			shortcut_entry{ .Command = command::add_tour_stop, .Group = tours, .Binding = bound, .Title = "Add a tour stop", .Keys = "Alt+Shift+T", .Objects = "addTourStopAction;desktopAddTourStopAction" },
			shortcut_entry{ .Command = command::tour_move_up, .Group = tours, .Binding = bound, .Title = "Move the stop up", .Place = in_outline, .Keys = "Alt+Shift+Up", .Objects = "tourMoveUp" },
			shortcut_entry{ .Command = command::tour_move_down, .Group = tours, .Binding = bound, .Title = "Move the stop down", .Place = in_outline, .Keys = "Alt+Shift+Down", .Objects = "tourMoveDown" },
			shortcut_entry{ .Command = command::tour_duplicate, .Group = tours, .Binding = bound, .Title = "Duplicate the stop", .Place = in_outline, .Keys = "Ctrl+D", .Objects = "tourDuplicate" },
			shortcut_entry{ .Command = command::tour_remove, .Group = tours, .Binding = bound, .Title = "Remove the stop", .Place = in_outline, .Keys = "Del;Backspace", .Objects = "tourRemove" },
			shortcut_entry{ .Command = command::tour_retarget, .Group = tours, .Binding = bound, .Title = "Point the stop at the selected lines", .Place = in_outline, .Keys = "Alt+Shift+U", .Objects = "tourRetarget" },
			shortcut_entry{ .Command = command::tour_ready_next, .Group = tours, .Binding = bound, .Title = "Ready, and the next stop", .Place = in_editor, .Keys = "Alt+Return", .Objects = "tourComplete" },
			shortcut_entry{ .Command = command::tour_undo, .Group = tours, .Binding = bound, .Title = "Undo a change to the stops", .Place = in_editor, .Standard = QKeySequence::Undo, .Objects = "tourUndo" },
			shortcut_entry{ .Command = command::tour_redo, .Group = tours, .Binding = bound, .Title = "Redo a change to the stops", .Place = in_editor, .Standard = QKeySequence::Redo, .Objects = "tourRedo" },

			// Home.
			shortcut_entry{ .Command = command::home_open_bar, .Group = home, .Binding = bound, .Title = "Go to the Open bar", .Place = "On Home", .Keys = "Ctrl+L", .Objects = "homeFocusBarAction" },
			shortcut_entry{ .Command = command::home_open_review, .Group = home, .Binding = read, .Title = "Open the review", .Place = "On a Home row", .Keys = "Return", .Objects = "homeOpenReviewAction" },
			shortcut_entry{ .Command = command::home_open_window, .Group = home, .Binding = read, .Title = "Open the review in a new window", .Place = "On a Home row", .Keys = "Ctrl+Return", .Objects = "homeOpenReviewInNewWindowAction" },
			shortcut_entry{ .Command = command::home_resume, .Group = home, .Binding = read, .Title = "Resume where you were", .Place = "In Recently viewed", .Keys = "Shift+Return", .Objects = "homeRecentResumeAction" },
			// [[BPR-1683]]. Delete, or Backspace as macOS lists take it, as
			// Recently viewed's Remove is; on an ignored row it unignores.
			shortcut_entry{ .Command = command::home_ignore, .Group = home, .Binding = read, .Title = "Ignore or unignore the pull request", .Place = "On a Home pull request row", .Keys = "Del;Backspace", .Objects = "homeIgnoreAction" },
			// [[BPR-1600]]. The window's Refresh key does whichever refresh is
			// showing: the key is on this action while Home shows and on the pull
			// request's otherwise, never on both. Like the pull request's, it
			// acts while the keyboard is in Home's Open bar.
			shortcut_entry{ .Command = command::home_refresh, .Group = home, .Binding = bound, .Title = "Refresh my pull requests", .Place = "On Home", .Standard = QKeySequence::Refresh, .Conditional = true, .Objects = "refreshQueueAction" },

		// Review Cycles: a local project's own keys ([[BPR-1157]], [[BPR-1280]]).
		// The code view reads Return as macOS names it and Enter as Windows
		// and Linux do.
#if defined(Q_OS_MACOS)
			shortcut_entry{ .Command = command::cycle_note, .Group = review_cycles, .Binding = read, .Title = "Note the line", .Place = in_cycle, .Keys = "Return", .Objects = "cycleSourceComment" },
			shortcut_entry{ .Command = command::cycle_suggest, .Group = review_cycles, .Binding = read, .Title = "Suggest a change here", .Place = in_cycle, .Keys = "Shift+Return", .Objects = "cycleSourceSuggest" },
#else
			shortcut_entry{ .Command = command::cycle_note, .Group = review_cycles, .Binding = read, .Title = "Note the line", .Place = in_cycle, .Keys = "Enter", .Objects = "cycleSourceComment" },
			shortcut_entry{ .Command = command::cycle_suggest, .Group = review_cycles, .Binding = read, .Title = "Suggest a change here", .Place = in_cycle, .Keys = "Shift+Enter", .Objects = "cycleSourceSuggest" },
#endif
			shortcut_entry{ .Command = command::cycle_save, .Group = review_cycles, .Binding = bound, .Title = "Save the note or suggestion", .Place = in_cycle_box, .Keys = "Ctrl+Return;Ctrl+Enter", .Shown = 1U, .Objects = "cycleComposerSaveKeys" },
#if defined(Q_OS_MACOS)
			// Control-C, which Qt names Meta; Command-C stays Copy.
			shortcut_entry{ .Command = command::cycle_cancel, .Group = review_cycles, .Binding = bound, .Title = "Discard the note or suggestion", .Place = in_cycle_box, .Keys = "Escape;Meta+C", .Objects = "cycleComposerCancelKeys" },
#else
			shortcut_entry{ .Command = command::cycle_cancel, .Group = review_cycles, .Binding = bound, .Title = "Discard the note or suggestion", .Place = in_cycle_box, .Keys = "Escape", .Objects = "cycleComposerCancelKeys" },
#endif

			shortcut_entry{ .Command = command::cycle_share_earlier, .Group = review_cycles, .Binding = bound, .Title = "Move the note earlier", .Place = "In the share sheet", .Keys = "Alt+Up", .Objects = "cycleShareMoveEarlier" },
			shortcut_entry{ .Command = command::cycle_share_later, .Group = review_cycles, .Binding = bound, .Title = "Move the note later", .Place = "In the share sheet", .Keys = "Alt+Down", .Objects = "cycleShareMoveLater" },
#if defined(Q_OS_MACOS)
			// Elsewhere Ctrl+C is Copy, so the sheets bind nothing.
			shortcut_entry{ .Command = command::cycle_sheet_close, .Group = review_cycles, .Binding = bound, .Title = "Close the sheet", .Place = "In a Review Cycle sheet", .Keys = "Meta+C", .Objects = "cycleSheetCloseKeys" },
#else
			shortcut_entry{ .Command = command::cycle_sheet_close, .Group = review_cycles, .Binding = bound, .Title = "Close the sheet", .Place = "In a Review Cycle sheet", .Objects = "cycleSheetCloseKeys" },
#endif

			// The application.
			shortcut_entry{ .Command = command::keyboard_shortcuts, .Group = application, .Binding = bound, .Title = "Keyboard shortcuts", .Keys = "Ctrl+/", .Objects = "keyboardShortcutsAction;desktopKeyboardShortcutsAction" },
			// Preferences has no standard binding on Windows or GNOME, so the
			// product's own key is bound on every platform.
			shortcut_entry{ .Command = command::settings, .Group = application, .Binding = bound, .Title = "Settings", .Keys = "Ctrl+,", .Objects = "openSettingsAction;desktopOpenSettingsAction" },
			shortcut_entry{ .Command = command::insights, .Group = application, .Binding = bound, .Title = "Insights", .Keys = "Ctrl+Alt+Shift+I", .Objects = "openInsightsAction;desktopOpenInsightsAction" },
			shortcut_entry{ .Command = command::settings_find, .Group = application, .Binding = bound, .Title = "Search Settings", .Place = "In Settings", .Standard = QKeySequence::Find, .EveryBinding = true, .Shown = 1U, .Objects = "settingsFindShortcut" },
			shortcut_entry{ .Command = command::new_window, .Group = application, .Binding = bound, .Title = "New window", .Place = "With no window open", .Standard = QKeySequence::New, .Objects = "desktopNewBprWindowAction" },
			shortcut_entry{ .Command = command::quit, .Group = application, .Binding = bound, .Title = "Quit", .Standard = QKeySequence::Quit, .Objects = "quitProfilesAction;desktopQuitProfilesAction" },
			shortcut_entry{ .Command = command::connect, .Group = application, .Binding = bound, .Title = "Sign in to GitHub", .Keys = "Ctrl+Shift+C", .Objects = "connectGithubAction" },
			shortcut_entry{ .Command = command::open_pull_request, .Group = application, .Binding = bound, .Title = "Open a pull request", .Standard = QKeySequence::Open, .Objects = "openPullRequestAction" },
			shortcut_entry{ .Command = command::refresh, .Group = application, .Binding = bound, .Title = "Refresh the pull request", .Place = "With a pull request showing", .Standard = QKeySequence::Refresh, .Conditional = true, .Objects = "refreshPullRequestAction" },
		};

		[[nodiscard]] consteval auto in_command_order() -> bool
		{
			for (std::size_t index = 0U; index < catalogue.size(); ++index) {
				if (static_cast<std::size_t>(catalogue[index].Command) != index) {
					return false;
				}
			}
			return catalogue.back().Command == command::refresh;
		}
		static_assert(in_command_order(), "the catalogue has one entry per command, in the enumeration's order");

		[[nodiscard]] auto split(const std::string_view aText) -> QStringList
		{
			return QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size())).split(QLatin1Char{ ';' }, Qt::SkipEmptyParts);
		}

		/// \brief [[BPR-1536]]. Option, alone or with Shift, and a character
		/// key: the presses that type text on macOS.
		[[nodiscard, maybe_unused]] auto option_character(const QKeySequence& aKey) -> bool
		{
			if (aKey.count() != 1) {
				return false;
			}
			const auto modifiers = aKey[0].keyboardModifiers() & ~Qt::KeypadModifier;
			const auto key       = aKey[0].key();
			return (modifiers == Qt::AltModifier || modifiers == (Qt::AltModifier | Qt::ShiftModifier))
				&& key >= Qt::Key_Space && key < Qt::Key_Escape;
		}

		/// \brief The words macOS uses for a key that is not a character.
		[[nodiscard, maybe_unused]] auto spoken_key_name(const Qt::Key aKey) -> QString
		{
			switch (aKey) {
			case Qt::Key_Up:        return QStringLiteral("Up Arrow");
			case Qt::Key_Down:      return QStringLiteral("Down Arrow");
			case Qt::Key_Left:      return QStringLiteral("Left Arrow");
			case Qt::Key_Right:     return QStringLiteral("Right Arrow");
			case Qt::Key_PageUp:    return QStringLiteral("Page Up");
			case Qt::Key_PageDown:  return QStringLiteral("Page Down");
			case Qt::Key_Return:    return QStringLiteral("Return");
			case Qt::Key_Enter:     return QStringLiteral("Enter");
			case Qt::Key_Escape:    return QStringLiteral("Escape");
			case Qt::Key_Backspace: return QStringLiteral("Delete");
			case Qt::Key_Delete:    return QStringLiteral("Forward Delete");
			case Qt::Key_Tab:       return QStringLiteral("Tab");
			case Qt::Key_Home:      return QStringLiteral("Home");
			case Qt::Key_End:       return QStringLiteral("End");
			case Qt::Key_Plus:      return QStringLiteral("Plus");
			case Qt::Key_Minus:     return QStringLiteral("Minus");
			case Qt::Key_Equal:     return QStringLiteral("Equals");
			case Qt::Key_Comma:     return QStringLiteral("Comma");
			case Qt::Key_Slash:     return QStringLiteral("Slash");
			default:                return QKeySequence{ aKey }.toString(QKeySequence::PortableText);
			}
		}
	}

	auto shortcut_catalogue() -> std::span<const shortcut_entry>
	{
		return catalogue;
	}

	auto shortcut(const shortcut_command aCommand) -> const shortcut_entry&
	{
		return catalogue.at(static_cast<std::size_t>(aCommand));
	}

	auto shortcut_keys(const shortcut_entry& aEntry) -> QList<QKeySequence>
	{
		if (aEntry.Standard != QKeySequence::UnknownKey && aEntry.EveryBinding) {
			return QKeySequence::keyBindings(aEntry.Standard);
		}
		if (aEntry.Standard != QKeySequence::UnknownKey) {
			// The platform's first binding, as `QKeySequence{ StandardKey }`
			// gives and `QAction::setShortcut(StandardKey)` binds.
			const auto key = QKeySequence{ aEntry.Standard };
			return key.isEmpty() ? QList<QKeySequence>{ } : QList<QKeySequence>{ key };
		}
		auto keys = QList<QKeySequence>{ };
		for (const auto& text : split(aEntry.Keys)) {
			keys.push_back(QKeySequence::fromString(text, QKeySequence::PortableText));
		}
		return keys;
	}

	auto shortcut_keys(const shortcut_command aCommand) -> QList<QKeySequence>
	{
		return shortcut_keys(shortcut(aCommand));
	}

	auto shortcut_key(const shortcut_command aCommand) -> QKeySequence
	{
		const auto keys = shortcut_keys(aCommand);
		return keys.isEmpty() ? QKeySequence{ } : keys.front();
	}

	auto shortcut_objects(const shortcut_entry& aEntry) -> QList<QString>
	{
		return split(aEntry.Objects);
	}

	auto shortcut_group_title(const shortcut_group aGroup) -> QString
	{
		switch (aGroup) {
		case navigation:    return QStringLiteral("Navigation");
		case review:        return QStringLiteral("Review and files");
		case comments:      return QStringLiteral("Comments and labels");
		case find:          return QStringLiteral("Find");
		case views:         return QStringLiteral("Views and drawers");
		case comparison:    return QStringLiteral("Comparison and history");
		case zoom:          return QStringLiteral("Zoom");
		case tours:         return QStringLiteral("Review tours");
		case home:          return QStringLiteral("Home");
		case review_cycles: return QStringLiteral("Review Cycles");
		case application:   return QStringLiteral("Application");
		}
		std::abort();
	}

	auto steps_aside_while_typing(const shortcut_entry& aEntry) -> bool
	{
		if (aEntry.StepsAsideWhileTyping) {
			return true;
		}
#if defined(Q_OS_MACOS)
		// A window-wide Option+character key; a key acting in one place is
		// that place's own, and the Find query's toggles are the query's.
		// [[BPR-1678]]. Only when every key is one: a text field claims
		// each Option key alone, so a command key beside it still acts there.
		const auto keys = shortcut_keys(aEntry);
		return aEntry.Place.empty() && !keys.isEmpty() && std::ranges::all_of(keys, option_character);
#else
		return false;
#endif
	}

	auto spoken_key(const QKeySequence& aKey) -> QString
	{
#if defined(Q_OS_MACOS)
		if (aKey.count() != 1) {
			return aKey.toString(QKeySequence::NativeText);
		}
		const auto combination = aKey[0];
		const auto modifiers   = combination.keyboardModifiers();
		auto       words       = QStringList{ };
		// macOS's own order, Control Option Shift Command; Qt names Command
		// Control and Control Meta.
		if (modifiers.testFlag(Qt::MetaModifier)) {
			words.push_back(QStringLiteral("Control"));
		}
		if (modifiers.testFlag(Qt::AltModifier)) {
			words.push_back(QStringLiteral("Option"));
		}
		if (modifiers.testFlag(Qt::ShiftModifier)) {
			words.push_back(QStringLiteral("Shift"));
		}
		if (modifiers.testFlag(Qt::ControlModifier)) {
			words.push_back(QStringLiteral("Command"));
		}
		words.push_back(spoken_key_name(combination.key()));
		return words.join(QLatin1Char{ '-' });
#else
		return aKey.toString(QKeySequence::NativeText);
#endif
	}

	auto document_edge_for(const QKeyEvent& aEvent) -> std::optional<document_edge>
	{
		if (aEvent.matches(QKeySequence::MoveToStartOfDocument)) {
			return document_edge::start;
		}
		if (aEvent.matches(QKeySequence::MoveToEndOfDocument)) {
			return document_edge::end;
		}
#if defined(Q_OS_MACOS)
		// macOS marks the arrow keys as keypad keys.
		if ((aEvent.modifiers() & ~Qt::KeypadModifier) == Qt::ControlModifier) {
			if (aEvent.key() == Qt::Key_Up) {
				return document_edge::start;
			}
			if (aEvent.key() == Qt::Key_Down) {
				return document_edge::end;
			}
		}
#endif
		return std::nullopt;
	}
}
