#include "bpr/qt/review_tour_editor.hpp"
#include "workbench_window_support.hpp"

#include "bpr/qt/draft_state_chip.hpp"
#include "bpr/qt/quiet_push_button.hpp"

#include "bpr/qt/compose_review_sheet.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/resize_handle_cursor.hpp"
#include "bpr/qt/review_cycle_view.hpp"
#include "bpr/qt/type_preview.hpp"
#include "bpr/qt/workbench_window.hpp"

#include "bpr/core/overview_tour.hpp"
#include "bpr/core/review_comment_range.hpp"

#include "conversation_glyphs.hpp"
#include "drawer_tab_bar.hpp"
#if defined(Q_OS_MACOS)
	#include "insights_menu_mac.hpp"
#endif

#include "bpr/core/review_publication_policy.hpp"
#include "bpr/core/suggestion_draft.hpp"
#include "bpr/core/thread_reply.hpp"
#include "bpr/core/window_title.hpp"
#include "github_presentation_text.hpp"

#include "bpr/qt/github_conversation_model.hpp"

#include "bpr/qt/menu_tool_button.hpp"

#include "bpr/core/file_progress_mark.hpp"

#include "bpr/core/home_filter.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/commit_filter_view.hpp"
#include "bpr/qt/destructive_question.hpp"
#include "bpr/qt/device_authorization_panel.hpp"
#include "bpr/qt/dif_runtime.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/feedback_label_controls.hpp"
#include "bpr/qt/feedback_label_settings.hpp"
#include "bpr/qt/finding_queue_view.hpp"
#include "bpr/qt/generated_files_view.hpp"
#include "bpr/qt/home_filter_field.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/identity_avatar_pictures.hpp"
#include "bpr/qt/info_glyph.hpp"
#include "bpr/qt/keyboard_shortcuts.hpp"
#include "bpr/qt/keyboard_shortcuts_window.hpp"
#include "bpr/qt/native_window_chrome.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/overview_tour_summary.hpp"
#include "bpr/qt/pending_veil.hpp"
#include "bpr/qt/private_notes_panel.hpp"
#include "bpr/qt/private_review_panel.hpp"
#include "bpr/qt/profile_management_window.hpp"
#include "bpr/qt/progress_mark.hpp"
#include "bpr/qt/quiet_tool_button.hpp"
#include "bpr/qt/refresh_note.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/side_chat_panel.hpp"
#include "bpr/qt/usage_ring.hpp"
#include <QTabWidget>

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAccessible>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFont>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QIcon>
#include <QInputMethodQueryEvent>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMoveEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QRectF>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QStringList>
#include <QStyle>
#include <QStyleOption>
#include <QTabBar>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>
#include <QWindow>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <string_view>
#include <system_error>
#include <utility>

namespace bpr::qt {

	namespace {
#if defined(_WIN32)
		// [[BPR-0653]]. On Windows the strip also carries the menu control and
		// the three 46-pixel caption buttons. With the modality segments at their
		// three-character prefixes every control is top level from 902 pixels,
		// and the owner chose that floor on 2026-09-17.
		inline constexpr int platform_floor_width = 902;
#else
#endif
		// The navigation entries are exactly the core's navigation commands in the
		// core's order, so entry `i` reads `workbench_command_id{ i }` and the
		// window needs no second mapping between the two.
		static_assert(
			static_cast<std::size_t>(core::workbench_command_id::previous_file) == 0U);
		static_assert(
			static_cast<std::size_t>(core::workbench_command_id::next_file) == 1U);
		static_assert(
			static_cast<std::size_t>(core::workbench_command_id::previous_change)
			== file_navigation_entries);
		static_assert(
			static_cast<std::size_t>(core::workbench_command_id::next_change) == 3U);
		static_assert(
			static_cast<std::size_t>(application::file_receipt_action_id::seen)
			== 0U);
		static_assert(
			static_cast<std::size_t>(application::file_receipt_action_id::excluded)
			== 3U);

#if defined(Q_OS_WIN)
		/// \brief [[BPR-1336]]. Lets an editable field keep an AltGr character.
		class typed_text_yield final : public QObject
		{
		public:
			using QObject::QObject;

		protected:
			[[nodiscard]] auto eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool override
			{
				if (aEvent->type() == QEvent::ShortcutOverride && editor_keeps_typed_text(aWatched, *static_cast<QKeyEvent*>(aEvent))) {
					aEvent->accept();
					return true;
				}
				return QObject::eventFilter(aWatched, aEvent);
			}
		};
#endif

	}

#if defined(_WIN32)
	auto workbench_window::title_row_part_at(const QPoint aWindowPoint) const -> title_row_part
	{
		if (Strip == nullptr || !Strip->isVisible()) {
			return title_row_part::client;
		}
		const auto local = Strip->mapFrom(this, aWindowPoint);
		if (!Strip->rect().contains(local)) {
			return title_row_part::client;
		}
		if (CaptionButtons != nullptr && CaptionButtons->isVisible()) {
			const auto inButtons = CaptionButtons->mapFrom(Strip, local);
			if (CaptionButtons->rect().contains(inButtons)) {
				return CaptionButtons->part_at(inButtons);
			}
		}
		return Strip->empty_surface_at(local) ? title_row_part::caption : title_row_part::client;
	}

	auto workbench_window::caption_buttons() const noexcept -> windows_caption_buttons*
	{
		return CaptionButtons;
	}

	auto workbench_window::strip_menu_button() const noexcept -> QToolButton*
	{
		return StripMenuButton;
	}

	auto workbench_window::open_strip_menu() -> QMenu*
	{
		// [[BPR-1028]]. This used to be a QMenu on this handler's stack, run
		// with exec(). An accessible invoke runs a menu action without closing
		// the menu first, so File > Quit destroyed the window inside that nested
		// loop. The window deleted the stack menu as one of its children, and
		// the Debug CRT asserted _CrtIsValidHeapPointer. A heap child that
		// deletes itself on close, opened without a nested loop, has neither
		// problem. The window deletes an open menu along with its other
		// children.
		auto* const menu = new QMenu{ this };
		menu->setObjectName(QStringLiteral("workbenchStripMenu"));
		menu->setAttribute(Qt::WA_DeleteOnClose);
		for (auto* const action : PrimaryMenuBar->actions()) {
			if (action->isVisible()) {
				// [[BPR-1136]]. Qt caches a menu item's accessible interface by its
				// action, and these actions outlive each strip menu. A menu shown
				// again would find the item its deleted predecessor made, invalid
				// and parented to nothing, and UI Automation saw an empty menu.
				// Dropping it here lets this menu make its own.
				if (auto* const stale = QAccessible::queryAccessibleInterface(action); stale != nullptr) {
					QAccessible::deleteAccessibleInterface(QAccessible::uniqueId(stale));
				}
				menu->addAction(action);
			}
		}
		menu->popup(StripMenuButton->mapToGlobal(QPoint{ 0, StripMenuButton->height() }));
		return menu;
	}

	void workbench_window::open_strip_menu_from_keyboard()
	{
		// The first menu is highlighted, not opened, as a menu bar entered
		// from the keyboard highlights File. Down is the menu's own gesture
		// for that: it selects the first entry and opens no submenu, where
		// setActiveAction would open File at once.
		auto* const menu = open_strip_menu();
		auto        down = QKeyEvent{ QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier };
		QCoreApplication::sendEvent(menu, &down);
	}

	auto workbench_window::read_strip_menu_key(const QEvent& aEvent) -> bool
	{
		// [[BPR-1022]]. A chord's own modifiers other than Alt: Alt held with
		// any of these is a chord (Ctrl with Alt is AltGr), never Alt alone.
		constexpr auto other = Qt::ControlModifier | Qt::ShiftModifier | Qt::MetaModifier;
		const auto     ready = [this] {
			return QApplication::activePopupWidget() == nullptr && StripMenuButton != nullptr
				&& StripMenuButton->isVisible();
		};
		const auto type = aEvent.type();
		if (type == QEvent::KeyPress || type == QEvent::ShortcutOverride || type == QEvent::KeyRelease) {
			const auto& key = static_cast<const QKeyEvent&>(aEvent);
			if (key.key() == Qt::Key_Alt) {
				// A held Alt repeats its press; only the first press arms, and
				// the release opens the menu only if nothing came between.
				if (type == QEvent::KeyPress && !key.isAutoRepeat()) {
					StripMenuAltAlone = (key.modifiers() & other) == 0 && ready();
				} else if (type == QEvent::KeyRelease && !key.isAutoRepeat() && std::exchange(StripMenuAltAlone, false)
						   && ready()) {
					open_strip_menu_from_keyboard();
				}
				return false;
			}
			if (type == QEvent::KeyRelease) {
				return false;
			}
			// Every other key, chord or shortcut makes a held Alt a modifier.
			// A shortcut is offered as ShortcutOverride and, when it matches,
			// never arrives as a press, so both are read.
			StripMenuAltAlone = false;
			if (type == QEvent::KeyPress && key.key() == Qt::Key_F10 && !key.isAutoRepeat()
				&& (key.modifiers() & (other | Qt::AltModifier)) == 0 && ready()) {
				open_strip_menu_from_keyboard();
				return true;
			}
			return false;
		}
		switch (type) {
		case QEvent::MouseButtonPress:
		case QEvent::MouseButtonDblClick:
		case QEvent::NonClientAreaMouseButtonPress:
		case QEvent::NonClientAreaMouseButtonDblClick:
		case QEvent::Wheel:
		case QEvent::TouchBegin:
		case QEvent::TabletPress:
		case QEvent::FocusIn:
		case QEvent::FocusOut:
		case QEvent::Hide:
			// An Alt-click, an Alt-wheel, or the window losing the keyboard
			// between Alt's press and release (Alt+Tab) is not Alt alone.
			StripMenuAltAlone = false;
			break;
		default:
			break;
		}
		return false;
	}

#endif

}
