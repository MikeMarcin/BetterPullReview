#include "bpr/core/overview_tour.hpp"
#include "bpr/core/review_comment_range.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/feedback_label_controls.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/home_filter_field.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/keyboard_shortcuts.hpp"
#include "bpr/qt/native_window_chrome.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/private_notes_panel.hpp"
#include "bpr/qt/profile_management_window.hpp"
#include "bpr/qt/progress_mark.hpp"
#include "bpr/qt/refresh_note.hpp"
#include "bpr/qt/review_cycle_view.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "conversation_glyphs.hpp"
#include "github_presentation_text.hpp"
#include "insights_menu_mac.hpp"
#include "workbench_window_support.hpp"

#include <QAbstractListModel>
#include <QAccessible>
#include <QAccessibleAnnouncementEvent>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QFlags>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QIcon>
#include <QInputMethodQueryEvent>
#include <QKeyCombination>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QList>
#include <QListSpecialMethodsBase>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMetaObject>
#include <QModelIndex>
#include <QMoveEvent>
#include <QObject>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QRect>
#include <QResizeEvent>
#include <QScreen>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSize>
#include <QStatusBar>
#include <QString>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVariant>
#include <QWidget>
#include <QWindow>

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::qt {
	namespace {
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

	namespace {
		/// \brief [[BPR-1328]]. The owner's final keys, 2026-10-08: "Okay final
		/// choice: option cmd down next file. Option down next change".
		/// Option-Up and Option-Down (Alt+Up and Alt+Down elsewhere) are
		/// Previous and Next change; Option-Command-Up and Option-Command-Down
		/// (Ctrl+Alt+Up and Ctrl+Alt+Down) are Previous and Next file.
		[[nodiscard]] auto previous_change_key() -> QKeySequence
		{
			return shortcut_key(shortcut_command::previous_change);
		}

		[[nodiscard]] auto next_change_key() -> QKeySequence
		{
			return shortcut_key(shortcut_command::next_change);
		}

		/// \brief [[BPR-1328]]. The keys an editor being written keeps for its
		/// own caret: Option-Up and Option-Down, its paragraph moves on macOS,
		/// and Command-Up and Command-Down, its document start and end there.
		/// Command-Up and Command-Down are no window command, but the code
		/// panes scroll on them, and an editor inside a pane must not hand an
		/// ignored press on to the pane.
		[[nodiscard]] auto editor_native_keys() -> QList<QKeySequence>
		{
			return { previous_change_key(),
					 next_change_key(),
					 QKeySequence{ QStringLiteral("Ctrl+Up") },
					 QKeySequence{ QStringLiteral("Ctrl+Down") } };
		}

		/// \brief [[BPR-1531]] [[BPR-1536]]. Gives a widget in the window
		/// that takes typed text the keys that type there.
		///
		/// \details In `ShortcutOverride` it claims Accept file's Option-A
		/// (Alt+A) and, on macOS, every Option+character key, so no window,
		/// button or widget shortcut fires and the key goes on to the widget
		/// and the text input system as "å" or "®". The claim is made when the
		/// key arrives, so it also holds for a widget that begins taking text
		/// while it has focus: the Review Cycle reader becomes the suggestion
		/// editor in place. That change reaches Qt as an input method query
		/// for `ImEnabled` on the focused widget, on which `aRefresh` re-reads
		/// the window's typing state; it returns at once when nothing changed.
		class typed_text_key_claim final : public QObject
		{
		public:
			typed_text_key_claim(QWidget* const aWindow, std::function<void()> aRefresh)
				: QObject{ aWindow }
				, Window{ aWindow }
				, Refresh{ std::move(aRefresh) }
			{
			}

		protected:
			[[nodiscard]] auto eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool override
			{
				if (aEvent->type() == QEvent::ShortcutOverride) {
					const auto combination = static_cast<QKeyEvent*>(aEvent)->keyCombination();
					if (QKeySequence{ combination } == accept_file_key() || (option_types_text && option_character_key(combination))) {
						const auto* const widget = qobject_cast<const QWidget*>(aWatched);
						if (widget != nullptr && Window->isAncestorOf(widget) && takes_typed_text(widget)) {
							aEvent->accept();
							return true;
						}
					}
				} else if (aEvent->type() == QEvent::InputMethodQuery
						   && static_cast<QInputMethodQueryEvent*>(aEvent)->queries().testFlag(Qt::ImEnabled)
						   && aWatched == QApplication::focusWidget()) {
					Refresh();
				}
				return QObject::eventFilter(aWatched, aEvent);
			}

		private:
			QWidget*              Window;
			std::function<void()> Refresh;
		};

		/// \brief [[BPR-1295]]. Keeps Hide Drawers' key in an editable
		/// multi-line editor.
		///
		/// \details A window shortcut fires inside an editor unless the editor
		/// claims the key in `ShortcutOverride`, and some editors live in the
		/// Conversations drawer, so a key that won there would hide the text
		/// being written. The editor claims Command-B (Ctrl+B) and does
		/// nothing with it for now: Markdown bold would be its own card.
		/// Claiming it in `ShortcutOverride` also keeps the native macOS menu
		/// item from taking it, because Qt asks the focus object first.
		///
		/// [[BPR-1328]]. Option-Down and Option-Up (Alt+Down and Alt+Up) are
		/// Next and Previous change, and in an editor they are its own
		/// paragraph moves (macOS). The editor claims them, and Command-Down
		/// and Command-Up, in `ShortcutOverride` too, and hands the key press
		/// to the editor alone, so the caret moves as it always did.
		class editor_key_yield final : public QObject
		{
		public:
			editor_key_yield(QObject* const aParent, const QAction* const aAction, QList<QKeySequence> aNative)
				: QObject{ aParent }
				, Action{ aAction }
				, Native{ std::move(aNative) }
			{
			}

		protected:
			[[nodiscard]] auto eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool override
			{
				if ((aEvent->type() == QEvent::ShortcutOverride || aEvent->type() == QEvent::KeyPress)
					&& editable_multi_line(aWatched)) {
					// macOS marks the arrow keys as keypad keys; the shortcut
					// map looks past that, and so does the yield.
					const auto* const key      = static_cast<QKeyEvent*>(aEvent);
					const auto        sequence = QKeySequence{ QKeyCombination{ key->modifiers() & ~Qt::KeypadModifier, static_cast<Qt::Key>(key->key()) } };
					if (Action != nullptr && sequence == Action->shortcut()) {
						aEvent->accept();
						return true;
					}
					if (Native.contains(sequence)) {
						// The editor claims the key against the window's command,
						// acts on the press as it always did, and the press
						// stops there: one it ignores, such as Option-Up on
						// the first line, would otherwise travel on to the
						// window or the code pane around it. A press handed on is not spontaneous, so Qt
						// asks the shortcut map again, and the claim answers.
						if (aEvent->type() == QEvent::KeyPress) {
							if (Forwarding) {
								return false;
							}
							Forwarding = true;
							QCoreApplication::sendEvent(aWatched, aEvent);
							Forwarding = false;
						}
						aEvent->accept();
						return true;
					}
				}
				return QObject::eventFilter(aWatched, aEvent);
			}

		private:
			QPointer<const QAction> Action;
			QList<QKeySequence>     Native;
			bool                    Forwarding = false;
		};
	}

	void workbench_window::apply_refresh_event(const bool aHome, const core::refresh_event& aEvent)
	{
		auto&      state   = aHome ? HomeRefresh : PullRefresh;
		const auto before  = state;
		state              = core::apply_refresh(before, aEvent);
		auto* const action = aHome ? QueueRefreshAction : GithubRefreshAction;
		auto* const button = PrimaryToolbar != nullptr && action != nullptr ? PrimaryToolbar->widgetForAction(action) : nullptr;
		auto*       timer  = aHome ? HomeSlowTimer : RefreshSlowTimer;
		if (state.Phase == core::refresh_phase::refreshing && before.Phase != core::refresh_phase::refreshing && timer != nullptr) {
			timer->start();
		}
		// The tick is the end of one read only: a new read or a failure takes it away.
		if (state.Phase != core::refresh_phase::updated) {
			(aHome ? HomeSettledCue : PullSettledCue) = false;
		}
		if (state.Phase == core::refresh_phase::updated && before.Phase == core::refresh_phase::refreshing) {
			(aHome ? HomeSettledCue : PullSettledCue) = true;
			if (auto* const cue = aHome ? HomeCueTimer : PullCueTimer; cue != nullptr) {
				cue->start();
			}
		}
		// The note speaks for the surface on screen only.
		const auto shown = aHome == showing_home();
		if (RefreshNote != nullptr && shown) {
			switch (state.Phase) {
			case core::refresh_phase::refreshing:
				if (state.Trigger == core::refresh_trigger::reviewer && (before.Phase != core::refresh_phase::refreshing || before.Trigger != core::refresh_trigger::reviewer)) {
					RefreshNote->present(refresh_note_tone::busy, aHome ? QStringLiteral("Finding my pull requests…") : QStringLiteral("Refreshing the pull request…"), button);
				}
				break;
			case core::refresh_phase::updated:
				if (before.Phase == core::refresh_phase::refreshing || state.SettledAt != before.SettledAt) {
					if (state.Trigger == core::refresh_trigger::reviewer || core::any(state.Changes)) {
						RefreshNote->present(refresh_note_tone::settled, refresh_result_text(aHome, state), button, 5'000);
					} else if (RefreshNote->tone() == refresh_note_tone::busy) {
						RefreshNote->dismiss();
					}
				}
				break;
			case core::refresh_phase::failed:
				if (state.SettledAt != before.SettledAt || before.Phase != core::refresh_phase::failed) {
					RefreshNote->present(refresh_note_tone::failed, refresh_result_text(aHome, state), button, 12'000);
				}
				break;
			case core::refresh_phase::never:
				if (RefreshNote->isVisible() && RefreshNote->tone() == refresh_note_tone::busy) {
					RefreshNote->dismiss();
				}
				break;
			}
		}
		auto said = QString{ };
		switch (core::refresh_announcement_between(before, state)) {
		case core::refresh_announcement::none:    break;
		case core::refresh_announcement::started: said = aHome ? QStringLiteral("Finding my pull requests.") : QStringLiteral("Refreshing the pull request."); break;
		case core::refresh_announcement::updated: said = refresh_result_text(aHome, state); break;
		case core::refresh_announcement::failed:  said = refresh_result_text(aHome, state) + QStringLiteral(" Retry is available."); break;
		}
		if (!said.isEmpty() && shown) {
			QAccessibleAnnouncementEvent event{ button != nullptr ? static_cast<QObject*>(button) : static_cast<QObject*>(this), said };
			event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&event);
		}
		present_refresh();
	}

	void workbench_window::present_refresh()
	{
		if (PrimaryToolbar == nullptr || RefreshFrames == nullptr) {
			return;
		}
		const auto full    = pending_full_motion();
		const auto failure = PendingFailureInk.isValid() ? PendingFailureInk : palette().color(QPalette::WindowText);
		const auto present = [&](QAction* const aAction, const core::refresh_state& aState, const bool aCue, QTimer* const aSlow, const bool aHome) {
			if (aAction == nullptr) {
				return;
			}
			const auto busy = aState.Phase == core::refresh_phase::refreshing;
			const auto look = busy ? (full ? strip_refresh_look::turning : strip_refresh_look::waiting)
				: aCue             ? strip_refresh_look::settled
								   : strip_refresh_look::idle;
			aAction->setIcon(strip_refresh_icon(look, RefreshFrame, palette()));
			auto* const button = PrimaryToolbar->widgetForAction(aAction);
			if (button == nullptr) {
				return;
			}
			const auto alert = aState.Phase == core::refresh_phase::failed ? failure : QColor{ };
			if (button->property(strip_style::busy_property).toBool() != busy || button->property(strip_style::alert_property).value<QColor>() != alert) {
				button->setProperty(strip_style::busy_property, busy);
				button->setProperty(strip_style::alert_property, alert);
				button->update();
			}
			auto describe = QString{ };
			switch (aState.Phase) {
			case core::refresh_phase::refreshing:
				describe = aHome ? QStringLiteral("Finding my pull requests in saved repositories…") : QStringLiteral("Refreshing the pull request…");
				break;
			case core::refresh_phase::failed:
				describe = refresh_result_text(aHome, aState) + QStringLiteral(" Press to try again.");
				break;
			case core::refresh_phase::updated:
			case core::refresh_phase::never:   {
				const auto base = aHome ? QStringLiteral("Find my pull requests in saved repositories again; this never changes GitHub.")
										: QStringLiteral("Reload the open pull request from its host.");
				describe        = aState.UpdatedAt.has_value()
					? QStringLiteral("%1 Updated %2.").arg(base, refresh_age_words(core::refresh_age_at(*aState.UpdatedAt, refresh_clock())))
					: base;
				break;
			}
			}
			button->setAccessibleDescription(describe);
			// Past the slow mark the note says what the read still waits on.
			if (busy && aSlow != nullptr && !aSlow->isActive() && RefreshNote != nullptr && RefreshNote->isVisible()
				&& RefreshNote->tone() == refresh_note_tone::busy && RefreshNote->anchor() == button) {
				const auto stage = aHome ? QString{ } : read_stage_words();
				const auto said  = aHome ? QStringLiteral("Still waiting for GitHub to search %1.")
											   .arg(QueueProjection.Discovery.Repositories == 1U ? QStringLiteral("1 saved repository")
																								 : QStringLiteral("%1 saved repositories").arg(QueueProjection.Discovery.Repositories))
					: stage.isEmpty()    ? QStringLiteral("Still waiting for GitHub…")
										 : QStringLiteral("Still waiting for GitHub: %1…").arg(stage);
				if (RefreshNote->text() != said) {
					RefreshNote->present(refresh_note_tone::busy, said, button);
				}
			}
		};
		present(GithubRefreshAction, PullRefresh, PullSettledCue, RefreshSlowTimer, false);
		present(QueueRefreshAction, HomeRefresh, HomeSettledCue, HomeSlowTimer, true);
		const auto turning = full && (PullRefresh.Phase == core::refresh_phase::refreshing || HomeRefresh.Phase == core::refresh_phase::refreshing);
		if (turning && !RefreshFrames->isActive()) {
			RefreshFrames->start();
		} else if (!turning) {
			RefreshFrames->stop();
		}
		if (RefreshNote != nullptr) {
			RefreshNote->set_full_motion(full);
			RefreshNote->set_failure_ink(failure);
		}
		if (RefreshFreshness != nullptr) {
			const auto  words  = refresh_freshness_text();
			const auto& state  = showing_home() ? HomeRefresh : PullRefresh;
			const auto  failed = state.Phase == core::refresh_phase::failed && !words.isEmpty();
			const auto  text   = failed ? QStringLiteral("<span style=\"color:%1\">%2</span> · <a href=\"retry\">Retry</a>").arg(failure.name(), words.toHtmlEscaped())
										: words.toHtmlEscaped();
			if (RefreshFreshness->text() != text) {
				RefreshFreshness->setText(text);
			}
			// One width for every phrase it says, the widest, so neither it
			// nor the status line beside it moves as the words change.
			const auto widest = QFontMetrics{ RefreshFreshness->font() }.horizontalAdvance(QStringLiteral("Not refreshed · updated 59 min ago · Retry")) + 8;
			if (RefreshFreshness->width() != widest) {
				RefreshFreshness->setFixedWidth(widest);
			}
			RefreshFreshness->setToolTip(failed ? refresh_result_text(showing_home(), state) : words);
			RefreshFreshness->setAccessibleDescription(RefreshFreshness->toolTip());
			RefreshFreshness->setVisible(!words.isEmpty());
			// The words age on their own: woken exactly when they would change.
			if (state.UpdatedAt.has_value() && state.Phase != core::refresh_phase::refreshing && RefreshAgeTimer != nullptr) {
				const auto next = core::refresh_age_next_change_seconds(*state.UpdatedAt, refresh_clock());
				RefreshAgeTimer->start(static_cast<int>(std::clamp<std::int64_t>(next, 1, 86'400) * 1'000));
			}
		}
	}

	void workbench_window::present(const application::home_open_projection& aProjection)
	{
		HomeOpen = aProjection.State.has_value() ? std::optional{ aProjection } : std::nullopt;
		refresh_home_open();
		// [[BPR-1843]]. The open is over: the held place goes.
		if (!HomeOpen.has_value() && HomeLandingHold > 0) {
			update_home_landing();
		}
	}

	void workbench_window::dispatch_home_open_action()
	{
		if (!HomeOpenDispatch || !HomeOpen.has_value() || !HomeOpen->State.has_value()) {
			return;
		}
		// The request the projection named; the application admits it against
		// the core's state, so a stale press asks for nothing.
		const auto request = HomeOpen->State->Request;
		if (HomeOpen->State->Phase == core::home_open_phase::failed) {
			if (HomeOpen->Retryable) {
				HomeOpenDispatch(application::retry_home_open{ request });
			}
			return;
		}
		if (HomeOpen->Cancellable) {
			HomeOpenDispatch(application::cancel_home_open{ request });
		}
	}

	auto workbench_window::home_open_text(const bool aRow) const -> QString
	{
		if (!HomeOpen.has_value() || !HomeOpen->State.has_value()) {
			return { };
		}
		const auto& state = *HomeOpen->State;
		const auto  name  = aRow ? QStringLiteral("#%1").arg(state.Number.value())
								 : QStringLiteral("%1/%2#%3")
									   .arg(QString::fromStdString(state.Repository.Owner), QString::fromStdString(state.Repository.Name))
									   .arg(state.Number.value());
		switch (state.Phase) {
		case core::home_open_phase::opening:
			return QStringLiteral("Opening %1…").arg(name);
		case core::home_open_phase::waiting: {
			// The step the read is on, from its own bounded progress.
			auto step = QStringLiteral("asking GitHub for the pull request");
			if (ReadProgress.has_value()) {
				switch (ReadProgress->Phase) {
				case application::github_read_phase::metadata:
					if (ReadProgress->PagesReceived != 0U) {
						step = QStringLiteral("reading the pull request");
					}
					break;
				case application::github_read_phase::files:                 step = QStringLiteral("reading changed files"); break;
				case application::github_read_phase::commits:               step = QStringLiteral("reading commits"); break;
				case application::github_read_phase::reviews:               step = QStringLiteral("reading reviews"); break;
				case application::github_read_phase::review_threads:        step = QStringLiteral("reading review threads"); break;
				case application::github_read_phase::comments:              step = QStringLiteral("reading comments"); break;
				case application::github_read_phase::checks:                step = QStringLiteral("reading checks"); break;
				case application::github_read_phase::related_pull_requests: step = QStringLiteral("reading related pull requests"); break;
				case application::github_read_phase::update_history:        step = QStringLiteral("reading update history"); break;
				}
			}
			return QStringLiteral("Opening %1: %2…").arg(name, step);
		}
		case core::home_open_phase::failed: {
			const auto reason = detail::reason_text(state.Reason, std::nullopt, std::nullopt);
			return reason.isEmpty() ? QStringLiteral("Couldn't open %1.").arg(name)
									: QStringLiteral("Couldn't open %1. %2").arg(name, reason);
		}
		}
		return { };
	}

	void workbench_window::refresh_home_open()
	{
		auto open = home_row_open{ };
		if (HomeOpen.has_value() && HomeOpen->State.has_value()) {
			const auto& state = *HomeOpen->State;
			open.Identity     = QStringLiteral("%1/%2#%3")
									.arg(QString::fromStdString(state.Repository.Owner), QString::fromStdString(state.Repository.Name))
									.arg(state.Number.value());
			open.Phase        = state.Phase == core::home_open_phase::opening ? home_row_open_phase::opening
				: state.Phase == core::home_open_phase::waiting               ? home_row_open_phase::waiting
																			  : home_row_open_phase::failed;
			open.Line         = home_open_text(true);
			open.Action       = HomeOpen->Cancellable ? tr("Cancel") : HomeOpen->Retryable ? tr("Retry")
																						   : QString{ };
		}
		if (HomeList != nullptr) {
			HomeList->set_open(open);
			HomeOpenOnRow = HomeList->open_view() != nullptr;
		} else {
			HomeOpenOnRow = false;
		}
		update_home_open_notice();
		// One polite announcement for each thing the opening says, wherever it
		// is shown; a reader hears the step and the failure as a sighted
		// reviewer reads them.
		const auto announcement = open.Phase == home_row_open_phase::none ? QString{ }
			: open.Phase == home_row_open_phase::waiting && HomeOpen->Cancellable
			? home_open_text(false) + QStringLiteral(" Cancel is available.")
			: home_open_text(false);
		if (!announcement.isEmpty() && announcement != LastHomeOpenAnnouncement && HomePage != nullptr) {
			LastHomeOpenAnnouncement = announcement;
			QAccessibleAnnouncementEvent event{ HomePage, announcement };
			event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&event);
		}
		if (announcement.isEmpty()) {
			LastHomeOpenAnnouncement.clear();
		}
	}

	void workbench_window::update_home_status()
	{
		if (HomeStatusText == nullptr || HomeOpenNotice == nullptr || SessionPanel == nullptr) {
			return;
		}
		// [[BPR-1357]]. Home's general status, said in the band when the bar's
		// own line is not: a problem the window has, a connection under way, or
		// what was just done. The session line is idle and stays in the status
		// bar, so a quiet Home adds nothing.
		auto text = QString{ };
		auto tone = home_status_tone::notice;
		if (Failure) {
			text = failure_text(*Failure, EpochSelectors);
			tone = home_status_tone::problem;
		} else if (ProfileFailure) {
			text = profile_management_failure_text(*ProfileFailure);
			tone = home_status_tone::problem;
		} else if (AgentWorkspacesUnavailable) {
			text = agent_workspaces_problem_text();
			tone = home_status_tone::problem;
		} else if (SavedLayoutUnavailable) {
			text = failure_text(application::workbench_host_failure::settings_unavailable);
			tone = home_status_tone::problem;
		} else if (SessionPanel->status() == core::github_status::establishing_session) {
			const auto host = QString::fromStdString(SessionPanel->active_host_text());
			text            = host.isEmpty() ? QStringLiteral("Connecting to GitHub…") : QStringLiteral("Connecting to %1…").arg(host);
			tone            = home_status_tone::busy;
		} else if (const auto message = statusBar() != nullptr ? statusBar()->currentMessage() : QString{ }; !message.isEmpty()) {
			text = message;
		}
		const auto shown = !text.isEmpty() && HomeOpenNotice->isHidden();
		HomeStatusText->setText(shown ? text : QString{ });
		if (shown && HomeStatus != nullptr) {
			HomeStatus->set_tone(tone);
		}
		HomeStatusText->setVisible(shown);
		statusBar()->update();
		const auto announcement = shown && showing_home() ? text : QString{ };
		if (!announcement.isEmpty() && announcement != LastHomeStatusAnnouncement && HomeStatus != nullptr) {
			LastHomeStatusAnnouncement = announcement;
			QAccessibleAnnouncementEvent event{ HomeStatus, announcement };
			event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&event);
		}
		if (announcement.isEmpty()) {
			LastHomeStatusAnnouncement.clear();
		}
	}

	auto workbench_window::select_queue_item(
		const core::pull_request_ref& aRef) -> bool
	{
		// [[BPR-0776]]. The row is found in whichever state's panel holds it,
		// which is expanded if the reviewer had collapsed it.
		if (HomeList != nullptr && HomeList->select(aRef, true)) {
			update_queue_status(QueueProjection);
			return true;
		}

		const auto present = [this, &aRef] {
			for (auto row = 0; row < QueueLanding->rowCount(); ++row) {
				const auto item = QueueLanding->item_at(QueueLanding->index(row, 0));
				if (item.has_value() && item->Ref == aRef) {
					return true;
				}
			}
			return false;
		}();
		QueueStatus->setText(
			present
				? tr("The notified review is hidden by the active queue filters.")
				: tr("The notified review is no longer in the local review queue."));
		QueueStatusNotable = true;
		update_home_surface();
		return false;
	}

	auto workbench_window::queue_role_filter() const noexcept -> QComboBox*
	{
		return QueueRoleFilter;
	}

	auto workbench_window::queue_observer_filter() const noexcept -> QComboBox*
	{
		return QueueObserverFilter;
	}

	auto workbench_window::queue_bucket_filter() const noexcept -> QComboBox*
	{
		return QueueBucketFilter;
	}

	auto workbench_window::queue_repository_filter() const noexcept -> QComboBox*
	{
		return QueueRepositoryFilter;
	}

	auto workbench_window::queue_quiet_filter() const noexcept -> QComboBox*
	{
		return QueueQuietFilter;
	}

	auto workbench_window::queue_sort_selector() const noexcept -> QComboBox*
	{
		return QueueSort;
	}

	auto workbench_window::queue_rule_kind_selector() const noexcept -> QComboBox*
	{
		return QueueRuleKind;
	}

	auto workbench_window::queue_rule_pattern_editor() const noexcept -> QLineEdit*
	{
		return QueueRulePattern;
	}

	auto workbench_window::queue_rule_exclusion() const noexcept -> QCheckBox*
	{
		return QueueRuleExcluding;
	}

	auto workbench_window::add_queue_rule_action() const noexcept -> QAction*
	{
		return AddQueueRule;
	}

	auto workbench_window::remove_queue_rule_action() const noexcept -> QAction*
	{
		return RemoveQueueRule;
	}

	auto workbench_window::queue_status_label() const noexcept -> QLabel*
	{
		return QueueStatus;
	}

	auto workbench_window::files_drawer_toggle_action() const noexcept -> QAction*
	{
		return Drawers[0].Toggle;
	}

	auto workbench_window::conversations_drawer_toggle_action() const noexcept
		-> QAction*
	{
		return Drawers[1].Toggle;
	}

	auto workbench_window::review_details_toggle_action() const noexcept -> QAction*
	{
		return ReviewDetailsAction;
	}

	auto workbench_window::epoch_selectors() const noexcept
		-> epoch_comparison_control*
	{
		return EpochSelectors;
	}

	auto workbench_window::focus_order() const -> QList<QWidget*>
	{
		auto       order  = QList<QWidget*>{ };
		const auto append = [&order](QWidget* const aWidget) {
			// A stop that cannot take focus is not an interactive stop. Skipping
			// it keeps the declared order equal to the order a keyboard user can
			// actually walk.
			if (aWidget != nullptr && aWidget->focusPolicy() != Qt::NoFocus) {
				order.push_back(aWidget);
			}
		};

		// The title row's stops in its reading order: Home, the drawer
		// toggles, the four modalities, Home's own controls, refresh, comments,
		// account, settings ([[BPR-1129]]).
		const auto presenter = [this](QAction* const aAction) {
			return qobject_cast<QToolButton*>(
				PrimaryToolbar->widgetForAction(aAction));
		};
#if defined(_WIN32)
		// [[BPR-0653]]. The compact menu control reads first, as it sits.
		append(StripMenuButton);
#endif
		append(presenter(HomeAction));
		append(presenter(Drawers[2].Toggle));
		append(presenter(Drawers[0].Toggle));
		if (Modality != nullptr) {
			for (std::size_t index = 0U; index < review_modality_count; ++index) {
				append(Modality->button(static_cast<review_modality>(index)));
			}
		}
		// [[BPR-0783]], [[BPR-1580]]. Home's View control, in the strip's
		// reading order, before the refresh glyph it stands with.
		append(HomeViewButton);
		append(presenter(GithubRefreshAction));
		append(presenter(QueueRefreshAction));
		append(presenter(Drawers[1].Toggle));
		append(presenter(AccountAction));
		append(presenter(SettingsAction));
#if defined(_WIN32)
		// [[BPR-0653]]. The caption buttons are the strip's last three stops.
		if (CaptionButtons != nullptr) {
			for (auto* const button : CaptionButtons->buttons()) {
				append(button);
			}
		}
#endif
		// [[BPR-1129]]. The review bar under the title row, in its reading
		// order: the comparison's menu, the change pair, the file pair, private
		// progress, Compose review and Comment.
		if (ReviewBar != nullptr && EpochSelectors != nullptr) {
			const auto barPresenter = [this](QAction* const aAction) {
				return qobject_cast<QToolButton*>(ReviewBar->widgetForAction(aAction));
			};
			append(UpdatesDisclosure);
			append(EpochSelectors->comparing_button());
			append(barPresenter(Navigation[0].Action));
			append(barPresenter(Navigation[1].Action));
			append(barPresenter(Navigation[2].Action));
			append(barPresenter(Navigation[3].Action));
			append(SeenStepButton);
			append(ReadStepButton);
			append(AcceptFileButton);
			append(ReviewControlsButton);
			// [[BPR-1815]]. The view controls, as they sit.
			append(barPresenter(OnePaneAction));
			append(barPresenter(HeaderSourceAction));
			append(barPresenter(ComposeReviewAction));
			append(barPresenter(CommentAction));
		}
		// The review header sits under the toolbars and above the workbench, so
		// its stops are declared there. Reading order and focus order agree.
		// The session chrome and the device sign-in are the Accounts and
		// hosts page of the Settings window, another top-level window with
		// its own tab chain; the workbench declares nothing across windows.
		// [[BPR-0733]] [[BPR-0776]]. Home's list: each shown panel's header, then
		// its rows or its sentence, in state order and Recently viewed last. A
		// panel that is not shown is no stop, and neither is a collapsed
		// panel's content.
		if (HomeList != nullptr) {
			for (auto* const stop : HomeList->focus_chain()) {
				append(stop);
			}
		}
		// [[BPR-0783]]. The six typed queue controls are Settings' Queue defaults
		// now, in another top-level window with its own tab chain, so the
		// workbench declares none of them — as it declares none of the rule
		// editor's ([[BPR-0652]]).
		// The files drawer's column, top to bottom: sign-in, the session
		// chrome, the queue, the pull request, then the files list.
		if (PullRequestPanel != nullptr) {
			append(PullRequestPanel->technical_details_button());
			append(PullRequestPanel->body_editor());
		}
		// A collapsed drawer's rail is its one stop; hidden while expanded.
		// [[BPR-0981]]. Each drawer's filter, its Clear filter (shown only in
		// the empty state) and, for files, the order selector precede its tree.
		append(Drawers[2].Panel->rail());
		append(RepositoriesFilter);
		if (RepositoriesFilterNotice != nullptr) {
			append(RepositoriesFilterNotice->clear_button());
		}
		append(Drawers[2].Panel->focus_stop());
		append(Drawers[0].Panel->rail());
		append(FilesOrderSelector);
		append(FilesFilter);
		if (FilesFilterNotice != nullptr) {
			append(FilesFilterNotice->clear_button());
		}
		append(Drawers[0].Panel->focus_stop());
		// The line first (its comparing menu is the review bar's now), then
		// the two selectors on one grid row above their two exact OIDs, the row
		// order the composed control already walks.
		if (HomeLanding != nullptr) {
			// [[BPR-1865]]. The filter, under the bar, before what it filters.
			if (HomeFilter != nullptr) {
				append(HomeFilter->line_edit());
			}
			append(HomeOnboardingButton);
			append(HomeAddRepositoryButton);
			append(HomeLanding->primary_button());
			append(HomeLanding->close_button());
			append(HomeLanding->secondary_button());
			append(HomeLanding->tertiary_button());
		}
		append(EpochSelectors->line());
		append(EpochSelectors->left_selector());
		append(EpochSelectors->right_selector());
		append(EpochSelectors->left_oid_editor());
		append(EpochSelectors->right_oid_editor());
		// [[BPR-1132]]. The comparison's header: the tabs, the count of the
		// ones the width floor leaves out, and the new-tab button.
		if (DocumentTabs != nullptr) {
			append(DocumentTabs->tab_bar());
			append(DocumentTabs->new_tab_button());
		}
		// One stop stands for the whole composed comparison surface: its last.
		// Everything before it inside that surface is RFC-BPR-0004's declared
		// order, which the shell reports through that card rather than restating
		// here.
		append(ComparisonSurface->detail_pane());
		append(Drawers[1].Panel->rail());
		// [[BPR-1134]]. The tray's tabs, then the Comments tab's tool and its
		// conversation; the Notes tab's stops follow.
		append(TrayTabs);
		append(NextNewCommentButton);
		append(PreviousDefectButton);
		append(NextDefectButton);
		append(Drawers[1].Panel->focus_stop());
		if (PrivateNotes != nullptr) {
			// [[BPR-1592]]. The notes, then the open note's field.
			append(PrivateNotes);
			append(PrivateNotes->body_editor());
		}
		if (ReviewEditor != nullptr) {
			append(ReviewEditor->body_editor());
		}
		append(StatusLabel);
		return order;
	}

	auto workbench_window::comparison_surface() const noexcept -> diff_workbench*
	{
		return ComparisonSurface;
	}

	auto workbench_window::left_conversation_endpoint_current() const noexcept -> bool
	{
		return LeftConversationEndpointCurrent;
	}

	void workbench_window::set_conversation_endpoint_placement(
		const bool aLeftHasCurrentDiffRevision,
		const bool aRightHasCurrentDiffRevision)
	{
		LeftConversationEndpointCurrent  = aLeftHasCurrentDiffRevision;
		RightConversationEndpointCurrent = aRightHasCurrentDiffRevision;
		if (ConversationPanel == nullptr) {
			return;
		}
		ApplyingConversationAnchor = true;
		ConversationPanel->set_current_endpoint_placement(
			aLeftHasCurrentDiffRevision, aRightHasCurrentDiffRevision);
		ApplyingConversationAnchor = false;
		synchronize_selected_conversation(chosen_conversation());
		update_defect_surface();
		update_comment_target();
		update_older_pair_notice();
		// [[BPR-1046]]. The composition places the endpoints once a
		// comparison has arrived, which is when a waiting reply can select
		// its thread's line.
		resume_deferred_reply();
		resume_conversation_pick();
		resume_pending_comment_pick();
	}

	auto workbench_window::detail_pane() const noexcept -> QPlainTextEdit*
	{
		return ComparisonSurface->detail_pane();
	}

	auto workbench_window::status_text() const -> QString
	{
		return StatusLabel->text();
	}

	auto workbench_window::statistics() const noexcept
		-> workbench_window_statistics
	{
		return Statistics;
	}

	void workbench_window::resizeEvent(QResizeEvent* const aEvent)
	{
		QMainWindow::resizeEvent(aEvent);
		update_discovery_controls();
		apply_strip_width();
		update_dif_presentation();
		dispatch_geometry();
	}

	void workbench_window::moveEvent(QMoveEvent* const aEvent)
	{
		QMainWindow::moveEvent(aEvent);
		dispatch_geometry();
	}

	void workbench_window::set_geometry_dispatch(std::function<void(application::workbench_window_geometry)> aDispatch)
	{
		GeometryDispatch = std::move(aDispatch);
	}

	void workbench_window::restore_geometry(const application::workbench_window_geometry& aGeometry)
	{
		if (GeometryReady) {
			return;
		}
		ApplyingGeometry = true;
		if (application::valid(aGeometry) && aGeometry.Width != 0) {
			auto  rectangle = QRect{ aGeometry.X, aGeometry.Y, aGeometry.Width, aGeometry.Height };
			auto* target    = QGuiApplication::primaryScreen();
			int   overlap   = 0;
			for (auto* screen : QGuiApplication::screens()) {
				const auto visible = rectangle.intersected(screen->availableGeometry());
				const auto area    = visible.width() * visible.height();
				if (area > overlap) {
					target  = screen;
					overlap = area;
				}
			}
			if (target != nullptr) {
				const auto available = target->availableGeometry();
				// Keep the title/resize affordances reachable after a display is
				// removed. The shell's minimum size remains authoritative on tiny
				// displays; placement still keeps its upper edge visible.
				rectangle.setWidth(std::clamp(rectangle.width(), minimumWidth(), std::max(minimumWidth(), available.width())));
				rectangle.setHeight(std::clamp(rectangle.height(), minimumHeight(), std::max(minimumHeight(), available.height() - 32)));
				rectangle.moveLeft(std::clamp(rectangle.x(), available.left(), std::max(available.left(), available.right() - rectangle.width() + 1)));
				rectangle.moveTop(std::clamp(rectangle.y(), available.top() + 32, std::max(available.top() + 32, available.bottom() - rectangle.height() + 1)));
			}
			setGeometry(rectangle);
			if (aGeometry.Maximized) {
				setWindowState(windowState() | Qt::WindowMaximized);
			}
		}
		ApplyingGeometry = false;
		GeometryReady    = true;
		dispatch_geometry();
	}

	void workbench_window::dispatch_geometry()
	{
		if (!GeometryReady || ApplyingGeometry || !GeometryDispatch || !isVisible() || isMinimized() || isFullScreen()) {
			return;
		}
		const auto normal      = normalGeometry();
		const auto observation = application::workbench_window_geometry{ normal.x(), normal.y(), normal.width(), normal.height(), isMaximized() };
		if (application::valid(observation) && observation.Width != 0) {
			GeometryDispatch(observation);
		}
	}

	void workbench_window::showEvent(QShowEvent* const aEvent)
	{
		QMainWindow::showEvent(aEvent);
#if defined(_WIN32)
		// [[BPR-1022]]. The native window exists once shown, and a window that
		// is made again is shown again; installing twice installs once.
		if (auto* const handle = windowHandle(); handle != nullptr) {
			handle->installEventFilter(this);
		}
#endif
		// [[BPR-0732]]. The first showing decides where a signed-out Home
		// starts: at the bar.
		if (!HomeFocused) {
			update_home_landing();
		}
		// The native window exists once shown; the title text goes then.
		hide_native_title_text(*this);
		update_dif_presentation();
		dispatch_geometry();
	}

	void workbench_window::update_rail_summaries()
	{
		// The rails restate what the drawers hold: the selected file's place
		// among the actionable files, and how many review threads there are.
		// The unread count the design names waits for the projection that
		// carries it; nothing is invented meanwhile.
		auto files = QString{ };
		if (Cursor && Cursor->ActionableCount > 0U) {
			files = Cursor->Ordinal > 0U
				? QStringLiteral("%1 of %2")
					  .arg(Cursor->Ordinal)
					  .arg(Cursor->ActionableCount)
				: QString::number(Cursor->ActionableCount);
		}
		Drawers[0].Panel->rail()->set_summary(files);
		Drawers[1].Panel->rail()->set_summary(
			ReviewThreadTotal > 0U ? QString::number(ReviewThreadTotal)
								   : QString{ });
		// The repositories rail: how many pull requests need me.
		const auto needsMe = RepositoriesTree != nullptr
			? RepositoriesTree->tree_model()->needs_me_count()
			: std::size_t{ 0U };
		Drawers[2].Panel->rail()->set_summary(
			needsMe > 0U ? QString::number(needsMe) : QString{ });
	}

	void workbench_window::infer_modality_from_drawers()
	{
		// A restored layout arrives before any modality was chosen in this
		// window, so the segment shows the preset the drawers already are.
		// Once a modality has been chosen here, a manual toggle leaves the
		// segment alone, as the accepted design requires.
		if (Modality == nullptr || ModalityChosen) {
			return;
		}
		// [[BPR-0727]]. Overview is chosen, never inferred: its preset is every
		// drawer collapsed, which Review with its drawers hidden looks exactly
		// like, and inferring it would make the core refuse a toggle the strip
		// never promised to refuse. [[BPR-1295]]: with Read merged into Review,
		// every restored shape is Review's, including Read's old one (Files
		// and Repositories collapsed, Comments open), which keeps the room it
		// had. Discuss is never inferred either; a window already in it stays.
		// [[BPR-1293]]. Nor is Threads, which is never stored.
		if (Modality->current() != review_modality::discuss && Modality->current() != review_modality::threads) {
			Modality->adopt(review_modality::review);
		}
		apply_modality_surface();
	}

	void workbench_window::apply_review_details_visibility()
	{
		if (ReviewDetailsAction == nullptr) {
			return;
		}
		const auto visible = ReviewDetailsAction->isChecked();
		// The epoch line is chrome and stays; the exact anchors, OIDs and
		// history maintenance beneath it are the review details. [[BPR-1334]]:
		// the strip under the comparison is Line details now, with its own
		// View menu entry, so this no longer shows or hides it.
		if (EpochSelectors != nullptr) {
			EpochSelectors->set_details_visible(visible);
		}
		const auto description = visible
			? QStringLiteral(
				  "Hide the exact review anchors and give their height to the comparison.")
			: QStringLiteral(
				  "Show the exact review anchors.");
		ReviewDetailsAction->setToolTip(description);
		ReviewDetailsAction->setStatusTip(description);
		// [[BPR-1130]]. The details sit in the Updates line's control, which a
		// fold would otherwise hide with the timeline.
		apply_updates_line();
	}

	void workbench_window::apply_strip_width()
	{
		// The title row never folds a control into an overflow menu: a control
		// that disappears at one width and returns at another is exactly the
		// chrome the accepted design rejects. What yields is text: the name's
		// title and owner, then the modality's shortcuts, then the name.
		if (IdentityAction == nullptr) {
			return;
		}
		const auto available = width();
		// [[BPR-0722]], [[BPR-1129]]. With a pull request open the name stays
		// as far down as its number fits, taking whatever the title row's other
		// items leave (see update_strip_identity). With nothing open there is
		// nothing to identify, and the label takes no width.
		IdentityAction->setVisible((!StripHome && (!IdentityText.isEmpty() || LoadingIdentity.has_value())) || (LocalCycleShown && !showing_home()));
		// [[BPR-1338]]. Open on GitHub goes with an accepted GitHub pull
		// request's name, never a local cycle's, and is measured with it below.
		// It is shown only where it acts: nothing in the title row reads as
		// disabled.
		const auto showPage = [this] {
			if (OpenOnGithubButtonAction != nullptr) {
				OpenOnGithubButtonAction->setVisible(IdentityAction->isVisible() && !(LocalCycleShown && !showing_home()) && PageContext.has_value());
			}
		};
		showPage();
		// [[BPR-1852]]. The draft chip goes with a GitHub pull request's name.
		if (StripDraftChipAction != nullptr) {
			const auto loadingName = IdentityText.isEmpty() && LoadingIdentity.has_value();
			const auto draft       = loadingName ? LoadingIdentity->Draft : (!IdentityText.isEmpty() && IdentityDraft);
			StripDraftChipAction->setVisible(IdentityAction->isVisible() && !(LocalCycleShown && !showing_home()) && draft);
		}
		const auto identityShort = [this] {
			return IdentityAction->isVisible() && strip_identity_budget() < strip_identity_minimum();
		};
		const auto remeasure = [this] {
			if (auto* const layout = PrimaryToolbar->layout(); layout != nullptr) {
				layout->invalidate();
			}
		};
		if (Modality != nullptr) {
			// The segments keep their names and lose the shortcut beside them.
			// [[BPR-0824]]. Decided from the width first, then compact only if
			// the number is still short, with the toolbar remeasured after each,
			// so a resize never alternates between the two forms.
			// [[BPR-1129]]. The title row holds no navigation now, so the
			// shortcuts stay down to the width the old strip needed for them.
			Modality->set_abbreviated(false);
			// [[BPR-1251]], [[BPR-1324]]. The title and then the owner yield
			// before the shortcuts, so whether each shows is decided from the
			// room it has beside the whole modality. That room only shrinks as
			// the window narrows, so the room a compact modality frees can
			// never bring either back.
			Modality->set_compact(false);
			remeasure();
			IdentityTitleBudget = strip_identity_budget();
#if defined(_WIN32)
			// [[BPR-1324]]. Windows' title row also holds the menu control and
			// the caption buttons, so with four modalities the shortcuts go
			// below 1300 there, which keeps the whole name at 1280 as on every
			// platform. Each segment's tooltip names its key.
			constexpr auto compactBelow = 1'300;
#else
			constexpr auto compactBelow = 1'100;
#endif
			Modality->set_compact(available < compactBelow);
			remeasure();
			if (identityShort()) {
				Modality->set_compact(true);
				remeasure();
			}
		}
		if (identityShort()) {
			IdentityAction->setVisible(false);
			showPage();
		}
		update_strip_identity();
		// [[BPR-0653]]. The last yield, the owner's: when the row still asks
		// for more than the window after every step above, the modality
		// segments show three-character prefixes. It is taken only then, so a
		// row that already fits never shortens. Measured after the identity has
		// been elided to its budget, or the text it is about to give up would
		// be counted as overflow.
		if (Modality != nullptr && PrimaryToolbar != nullptr) {
			remeasure();
			if (PrimaryToolbar->sizeHint().width() > available) {
				Modality->set_abbreviated(true);
				remeasure();
				// The room the prefixes free is the identity's to use.
				update_strip_identity();
			}
		}
		apply_review_bar_width();
	}

	void workbench_window::apply_review_bar_width()
	{
		if (ReviewBar == nullptr || EpochSelectors == nullptr) {
			return;
		}
		// [[BPR-1129]]. Every control on the review bar stays top level. As the
		// window narrows the comparison's dates go first, then the positions'
		// connective words ("Changes 3/11" for "Changes 3 of 11"), then the
		// numbers. File and Changes always remain; full positions stay in their
		// tooltips. Each width starts whole and measures, so nothing returns at a narrower
		// width and a resize never alternates.
		const auto remeasure = [this] {
			if (auto* const layout = ReviewBar->layout(); layout != nullptr) {
				layout->invalidate();
			}
		};
		// [[BPR-1255]]. The floor follows the bar, the owner's choice of
		// 2026-10-06: the window is never narrower than the bar with everything
		// yielded, so nothing folds in a larger font either. At the default
		// font the yielded bar is narrower than the platform floor, which
		// stands. A window below a raised floor is widened to it.
		EpochSelectors->set_comparing_dates_shown(false);
		ReviewBarCompact        = true;
		ReviewBarPositionsShown = false;
		update_strip_positions();
		// [[BPR-1578]]. The progress track's own yield is the last: Seen and
		// Read keep their marks and lose their words.
		if (ProgressTrack != nullptr) {
			ProgressTrack->set_compact(true);
		}
		// [[BPR-1700]]. Without Compose review's words, so the floor is
		// the bar with its icon, as before they were added.
		ComposeReviewWords = compose_words::none;
		present_compose_entry();
		remeasure();
		const auto inset = ReviewBar->isVisible() ? std::max(0, width() - ReviewBar->width()) : 0;
		const auto floor = std::max(platform_floor_width, ReviewBar->sizeHint().width() + inset);
		if (minimumWidth() != floor) {
			setMinimumWidth(floor);
		}
		const auto available = width();
		EpochSelectors->set_comparing_dates_shown(true);
		ReviewBarCompact        = false;
		ReviewBarPositionsShown = true;
		update_strip_positions();
		if (ProgressTrack != nullptr) {
			ProgressTrack->set_compact(false);
		}
		// [[BPR-1700]]. Compose review's words shorten first, to
		// "Approve…", the verdict a newcomer looks for, and that goes last of
		// all, after the dates, the positions' words and the steps' names.
		ComposeReviewWords = compose_words::full;
		present_compose_entry();
		remeasure();
		if (ReviewBar->sizeHint().width() > available) {
			ComposeReviewWords = compose_words::brief;
			present_compose_entry();
			remeasure();
		}
		if (ReviewBar->sizeHint().width() > available) {
			EpochSelectors->set_comparing_dates_shown(false);
			remeasure();
		}
		if (ReviewBar->sizeHint().width() > available) {
			ReviewBarCompact = true;
			update_strip_positions();
			remeasure();
		}
		if (ReviewBar->sizeHint().width() > available) {
			ReviewBarPositionsShown = false;
			update_strip_positions();
			remeasure();
		}
		if (ReviewBar->sizeHint().width() > available && ProgressTrack != nullptr) {
			ProgressTrack->set_compact(true);
			remeasure();
		}
		if (ReviewBar->sizeHint().width() > available) {
			ComposeReviewWords = compose_words::none;
			present_compose_entry();
			remeasure();
		}
	}

	void workbench_window::changeEvent(QEvent* const aEvent)
	{
		QMainWindow::changeEvent(aEvent);
		switch (aEvent->type()) {
		case QEvent::WindowStateChange:
			dispatch_geometry();
			break;
		case QEvent::ActivationChange:
			update_dif_presentation();
			// [[BPR-0990]]. Coming back to a window that shows Home is the
			// other activation discovery hears.
			if (isActiveWindow() && showing_home() && HomeAccount && QueueDiscoveryDispatch) {
				QueueDiscoveryDispatch(application::queue_discovery_intent::activate);
			}
			// [[BPR-1867]]. Coming back to a pull request it shows: read again
			// by itself once its update has aged.
			if (isActiveWindow() && PullRequestReturnDispatch && shown_pull_request()) {
				PullRequestReturnDispatch(refresh_clock());
			}
			break;
		case QEvent::ApplicationPaletteChange:
		case QEvent::PaletteChange:
			// Settings is a separate top-level window, so Qt does not inherit
			// the workbench's profile palette into either shell (BPR-1513).
			if (SettingsWindow != nullptr) {
				SettingsWindow->setPalette(palette());
			}
			if (MountedSettings) {
				MountedSettings->setPalette(palette());
			}
			// [[BPR-1566]]. A link's colour is set, not inherited: take the new
			// palette's, readable on its ground. Mark colours, beside it, too.
			for (auto* const link : { ConventionalCommentsLink, MarkColoursLink, WorkshopGuidanceLink }) {
				if (link != nullptr && link->parentWidget() != nullptr) {
					auto linkPalette = link->palette();
					linkPalette.setColor(QPalette::ButtonText, readable_link_colour(link->parentWidget()->palette()));
					link->setPalette(linkPalette);
				}
			}
			[[fallthrough]];
		case QEvent::ApplicationFontChange:
		case QEvent::FontChange:
		case QEvent::StyleChange:
		case QEvent::ThemeChange:
		case QEvent::DevicePixelRatioChange:
			refresh_toolbar_icons();
			// [[BPR-1592]]. The Agent tab's spark and the Notes tools follow
			// the theme.
			if (TrayTabs != nullptr && TrayTabs->count() > tray_findings_tab) {
				TrayTabs->setTabIcon(tray_findings_tab, drawer_icon(drawer_glyph::agent, palette()));
				fit_tray_tabs(true);
			}
			if (PrivateNotes != nullptr) {
				PrivateNotes->new_line_action()->setIcon(conversation_icon(conversation_glyph::comment, palette()));
				PrivateNotes->elsewhere_action()->setIcon(drawer_icon(drawer_glyph::pull_request, palette()));
			}
			// [[BPR-1255]]. A font, style or theme can move the review bar's
			// yielded width, and with it the window's floor.
			apply_strip_width();
			// The pull-request title and identity carry derived emphasis fonts,
			// which stop tracking the application font unless they are re-derived
			// here. `github_window` did this; the shell must too, or a reviewer
			// who enlarges the system font gets a title that does not grow.
			if (PullRequestPanel != nullptr) {
				PullRequestPanel->refresh_native_appearance();
			}
			if (ConversationPanel != nullptr) {
				ConversationPanel->relayout();
			}
			break;
		default:
			break;
		}
	}

	void workbench_window::set_review_navigation_dispatch(std::function<void(std::optional<core::workbench_command_id>)> aDispatch)
	{
		ReviewNavigationDispatch = std::move(aDispatch);
	}

	void workbench_window::refuse_review_navigation(const core::workbench_command_id aCommand)
	{
		if (showing_home() || showing_local_cycle() || current_modality() != review_modality::review) {
			return;
		}
		// A refused press says so for as long as a bound does: an arrow that
		// silently does nothing reads as broken.
		statusBar()->showMessage(QStringLiteral("This tab's file is unavailable. Choose another file or comparison."),
								 bounded_navigation_message_ms);
		focus_review_navigation(aCommand);
	}

	void workbench_window::dispatch_navigation(const std::size_t aIndex)
	{
		if (showing_home() || showing_local_cycle()) {
			return;
		}
		const auto id = static_cast<core::workbench_command_id>(aIndex);
		if (const auto offered = command(id); !offered || !offered->Enabled) {
			return;
		}
		// The core's normal entry restores Review's saved drawers. Bounds are
		// evaluated only after its active tab's exact source has returned.
		if (current_modality() != review_modality::review) {
			choose_modality(review_modality::review);
		}
		focus_review_navigation(id);
		if (ReviewNavigationDispatch) {
			ReviewNavigationDispatch(id);
		} else {
			perform_review_navigation(id);
		}
	}

	void workbench_window::focus_review_navigation(const core::workbench_command_id aCommand)
	{
		if (showing_home() || showing_local_cycle() || current_modality() != review_modality::review) {
			return;
		}
		if (ComparisonSurface->current_result() && ComparisonSurface->isVisible()) {
			ComparisonSurface->focus_source();
		} else if (const auto index = static_cast<std::size_t>(aCommand); index < Navigation.size()) {
			if (auto* const control = ReviewBar->widgetForAction(Navigation[index].Action); control != nullptr && control->isVisible()) {
				control->setFocus(Qt::ShortcutFocusReason);
			}
		}
	}

	void workbench_window::perform_review_navigation(const core::workbench_command_id aCommand)
	{
		const auto index = static_cast<std::size_t>(aCommand);
		const auto ends  = aCommand == core::workbench_command_id::first_change || aCommand == core::workbench_command_id::last_change;
		if ((index >= Navigation.size() && !ends) || showing_home() || showing_local_cycle() || current_modality() != review_modality::review) {
			return;
		}
		if (const auto offered = command(aCommand);
			!offered || !offered->Enabled || offered->Bound != core::workbench_reason_code::none) {
			// [[BPR-1532]]. First and Last change are disabled in Review where
			// nothing can be reached, so a press that entered Review from
			// elsewhere meets a disabled command and names its reason too. An
			// arrow is disabled only without a workbench, which names nothing.
			const auto reason = !offered ? core::workbench_reason_code::no_active_workbench
				: offered->Enabled       ? offered->Bound
										 : offered->DisabledReason;
			if (reason != core::workbench_reason_code::no_active_workbench) {
				statusBar()->showMessage(index < file_navigation_entries ? reason_text(reason) : change_reason_text(reason),
										 bounded_navigation_message_ms);
			}
			focus_review_navigation(aCommand);
			return;
		}
		if (index < file_navigation_entries) {
			if (Dispatch) {
				Dispatch(core::navigate_workbench_file_intent{
					.Direction = index == 0U
						? core::change_navigation_direction::previous
						: core::change_navigation_direction::next,
				});
			}
			focus_review_navigation(aCommand);
			return;
		}
		const auto direction = aCommand == core::workbench_command_id::previous_change || aCommand == core::workbench_command_id::first_change
			? core::change_navigation_direction::previous
			: core::change_navigation_direction::next;
		if (!ChangeDispatch) {
			return;
		}
		// The gesture repeats the policy the projection offered it, so the
		// emitted intent and the enablement that offered it cannot disagree about
		// stop versus wrap. First and Last change reach the end of the order.
		ChangeDispatch(core::navigate_diff_change_intent{
			.Direction = direction,
			.Policy    = Commands.Policy,
			.Reach     = ends ? core::change_navigation_reach::outermost : core::change_navigation_reach::adjacent,
		});
		focus_review_navigation(aCommand);
	}

	/// \brief Builds the window's menu bar and its View menu.
	///
	/// \details This window had no menu bar until BPR-0457. The owner asked
	/// for a "view menu option" and was given the choice between a real menu
	/// bar and a checkable entry in the existing toolbar popup; they chose the
	/// menu bar, which is the option that matches the words and, on macOS,
	/// merges into the system bar and costs the window no height. On Windows
	/// and Linux it takes a row, which is the price of the same answer being
	/// true everywhere.
	void workbench_window::build_menu_bar()
	{
		PrimaryMenuBar = new QMenuBar{ this };
		PrimaryMenuBar->setObjectName(QStringLiteral("workbenchMenuBar"));
		PrimaryMenuBar->setAccessibleName(QStringLiteral("Workbench menu bar"));
		PrimaryMenuBar->setAccessibleDescription(
			QStringLiteral("Presentation menus for the review workbench"));
		setMenuBar(PrimaryMenuBar);

		auto* fileMenu = PrimaryMenuBar->addMenu(QStringLiteral("&File"));
		fileMenu->setAccessibleName(QStringLiteral("File"));
		// [[BPR-1863]]. New Window opens another window of this window's profile,
		// first in File as on every desktop; Profiles keeps the profiles and their
		// management. Its key is the catalogue's, the one the windowless bar binds.
		NewBprWindowAction = fileMenu->addAction(QStringLiteral("New Window"));
		NewBprWindowAction->setObjectName(QStringLiteral("newBprWindowAction"));
		NewBprWindowAction->setShortcuts(shortcut_keys(shortcut_command::new_window));
		NewBprWindowAction->setEnabled(false);
		connect(NewBprWindowAction, &QAction::triggered, this, [this] {
			if (ProfileManagementDispatch && CurrentBprProfile.value() != 0U) {
				ProfileManagementDispatch(application::new_bpr_window_intent{ CurrentBprProfile });
			}
		});
		fileMenu->addSeparator();
		ProfilesMenu = PrimaryMenuBar->addMenu(QStringLiteral("&Profiles"));
		ProfilesMenu->setObjectName(QStringLiteral("workbenchProfilesMenu"));
		ProfilesMenu->setAccessibleName(QStringLiteral("Profiles"));
		ManageProfilesAction = ProfilesMenu->addAction(QStringLiteral("Manage Profiles…"));
		ManageProfilesAction->setObjectName(QStringLiteral("manageProfilesAction"));
		ManageProfilesAction->setEnabled(false);
		connect(ManageProfilesAction, &QAction::triggered, this, [this] {
			if (ManageProfiles) {
				ManageProfiles();
			}
		});
		// [[BPR-0777]]. RFC-BPR-0033 B3 reports a profile refusal in a window that
		// exists rather than presenting the chooser to carry it. Shown only after a
		// refusal, so at every other moment the menu is the profiles and their
		// two actions.
		ProfileStatusSeparator = ProfilesMenu->addSeparator();
		ProfileStatusSeparator->setVisible(false);
		ProfileStatusAction = ProfilesMenu->addAction(QString{ });
		ProfileStatusAction->setObjectName(QStringLiteral("profileManagementStatusAction"));
		ProfileStatusAction->setEnabled(false);
		ProfileStatusAction->setVisible(false);
		// [[BPR-1157]]. The local Review Cycles entry, before Quit.
		OpenProjectAction = fileMenu->addAction(QStringLiteral("Open Project…"));
		OpenProjectAction->setObjectName(QStringLiteral("openProjectAction"));
		OpenProjectAction->setToolTip(QStringLiteral("Review a local folder or file as a Review Cycle"));
		OpenProjectAction->setEnabled(false);
		connect(OpenProjectAction, &QAction::triggered, this, [this] {
			if (OpenProjectDispatch) {
				OpenProjectDispatch();
			}
		});
		fileMenu->addSeparator();
		QuitProfilesAction = fileMenu->addAction(QStringLiteral("Quit"));
		QuitProfilesAction->setObjectName(QStringLiteral("quitProfilesAction"));
		QuitProfilesAction->setMenuRole(QAction::QuitRole);
		QuitProfilesAction->setShortcuts(shortcut_keys(shortcut_command::quit));
		QuitProfilesAction->setEnabled(false);
		connect(QuitProfilesAction, &QAction::triggered, this, [this] {
			if (QuitProfiles) {
				QuitProfiles();
			}
		});

		// [[BPR-0990]]. The open pull request's two local gestures. Neither
		// reaches GitHub: Follow keeps it in my queue, Save Repository keeps
		// its repository for discovery.
		PullRequestMenu = PrimaryMenuBar->addMenu(QStringLiteral("P&ull Request"));
		PullRequestMenu->setObjectName(QStringLiteral("workbenchPullRequestMenu"));
		PullRequestMenu->setAccessibleName(QStringLiteral("Pull Request"));
		PullRequestMenu->setAccessibleDescription(
			QStringLiteral("Open the pull request on GitHub, keep it or its repository in my queue, or mark its comments read; nothing changes on GitHub"));
		FollowPullRequest = PullRequestMenu->addAction(QStringLiteral("&Follow"));
		FollowPullRequest->setObjectName(QStringLiteral("followPullRequestAction"));
		FollowPullRequest->setCheckable(true);
		FollowPullRequest->setEnabled(false);
		connect(FollowPullRequest, &QAction::triggered, this, [this] {
			const auto ref = open_queue_ref();
			if (!ref || !QueueFollowDispatch) {
				update_pull_request_queue_actions();
				return;
			}
			const auto item     = QueueLanding->item_for(*ref);
			const auto followed = item && item->Membership.Followed;
			QueueFollowDispatch(application::queue_follow_intent{ .Ref = *ref, .Title = OpenQueueTitle, .Follow = !followed });
			// The check mark is the queue's to decide.
			update_pull_request_queue_actions();
		});
		SavePullRequestRepository = PullRequestMenu->addAction(QStringLiteral("&Save Repository"));
		SavePullRequestRepository->setObjectName(QStringLiteral("savePullRequestRepositoryAction"));
		SavePullRequestRepository->setEnabled(false);
		connect(SavePullRequestRepository, &QAction::triggered, this, [this] {
			const auto ref = open_queue_ref();
			if (!ref || !RepositoryAdditionDispatch || RepositoryGesture == std::numeric_limits<std::uint64_t>::max()) {
				return;
			}
			RepositoryPendingGesture       = ++RepositoryGesture;
			RepositoryDisplayedDestination = QueueProjection.Session;
			RepositoryAdditionDispatch({ .Gesture = RepositoryPendingGesture, .Destination = RepositoryDisplayedDestination, .Text = ref->Repository });
		});
		// [[BPR-1699]]. Mark All Comments as Read: my read state for
		// every comment on the open pull request, in one durable change.
		// Like Follow it is local; GitHub is not told.
		PullRequestMenu->addSeparator();
		MarkAllCommentsRead = PullRequestMenu->addAction(QStringLiteral("&Mark All Comments as Read"));
		MarkAllCommentsRead->setObjectName(QStringLiteral("markAllCommentsReadAction"));
		MarkAllCommentsRead->setStatusTip(QStringLiteral("Mark every comment on this pull request as read. Only your own read state changes; nothing changes on GitHub."));
		MarkAllCommentsRead->setToolTip(MarkAllCommentsRead->statusTip());
		MarkAllCommentsRead->setEnabled(false);
		connect(MarkAllCommentsRead, &QAction::triggered, this, [this] {
			const auto dispatch = ConversationsReadDispatch;
			if (dispatch) {
				dispatch(std::nullopt);
			}
		});

		ViewMenu = PrimaryMenuBar->addMenu(QStringLiteral("&View"));
		ViewMenu->setObjectName(QStringLiteral("workbenchViewMenu"));
		ViewMenu->setAccessibleName(QStringLiteral("View"));
		auto* findMenu = ViewMenu->addMenu(tr("&Find"));
		findMenu->setObjectName(QStringLiteral("sourceFindMenu"));
		const auto findTitles = std::array{ tr("Find"), tr("Next pane match"), tr("Previous pane match") };
		const auto findNames  = std::array{ QStringLiteral("sourceFindAction"), QStringLiteral("sourceFindNextAction"), QStringLiteral("sourceFindPreviousAction") };
		for (std::size_t index = 0; index < SourceFindActions.size(); ++index) {
			auto* action = new QAction{ findTitles[index], this };
			action->setObjectName(findNames[index]);
			action->setShortcutContext(Qt::WindowShortcut);
			action->setShortcuts(shortcut_keys(std::array{ shortcut_command::find, shortcut_command::find_next, shortcut_command::find_previous }[index]));
			SourceFindActions[index] = action;
			action->setEnabled(false);
			addAction(action);
			findMenu->addAction(action);
			connect(action, &QAction::triggered, this, [this, index] {
				update_source_find_actions();
				if (!SourceFindActions[index]->isEnabled()) {
					return;
				}
				if (index == 0U) {
					ComparisonSurface->open_source_find();
				} else {
					ComparisonSurface->navigate_source_find(index == 1U ? core::change_navigation_direction::next : core::change_navigation_direction::previous);
				}
			});
		}
		const auto fileTitles = std::array{ tr("Find in Files"), tr("Next PR result"), tr("Previous PR result") };
		const auto fileNames  = std::array{ QStringLiteral("sourceFilesAction"), QStringLiteral("sourceFilesNextAction"), QStringLiteral("sourceFilesPreviousAction") };
		for (std::size_t index = 0; index < SourceFilesActions.size(); ++index) {
			auto* action = new QAction{ fileTitles[index], this };
			action->setObjectName(fileNames[index]);
			action->setShortcuts(shortcut_keys(std::array{ shortcut_command::find_in_files, shortcut_command::next_result, shortcut_command::previous_result }[index]));
			action->setShortcutContext(Qt::WindowShortcut);
			action->setEnabled(false);
			SourceFilesActions[index] = action;
			addAction(action);
			findMenu->addAction(action);
			connect(action, &QAction::triggered, this, [this, index] {update_source_find_actions();if(!SourceFilesActions[index]->isEnabled() || !PrSearchDispatch){return;}if(index==0){if(!SearchPanel->projection().Open){SearchWasCollapsed=DrawerStates.Files==core::workbench_drawer_state::collapsed;SearchPreviousTab=FileSearchTabs->currentIndex();}if(current_modality()!=review_modality::review || CompositionPage){choose_modality(review_modality::review);}if(Dispatch){Dispatch(core::set_workbench_drawer_intent{core::workbench_drawer::files,core::workbench_drawer_state::expanded});}FileSearchTabs->setCurrentWidget(SearchPanel);PrSearchDispatch(source_find_gesture::document_changed);}else{if(current_modality()!=review_modality::review || CompositionPage){choose_modality(review_modality::review);}PrSearchDispatch(index==1?source_find_gesture::next:source_find_gesture::previous);} });
		}
		connect(findMenu, &QMenu::aboutToShow, this, [this] { update_source_find_actions(); });
		connect(qApp, &QApplication::focusChanged, this, [this](QWidget*, QWidget*) { update_source_find_actions(); });

		ViewMenu->setAccessibleDescription(
			QStringLiteral("Presentation options for the comparison surface"));
		HeaderSourceAction = new QAction{ tr("Go to header/source"), this };
		HeaderSourceAction->setObjectName(QStringLiteral("goToHeaderSourceAction"));
		HeaderSourceAction->setShortcuts(shortcut_keys(shortcut_command::header_source));
		HeaderSourceAction->setShortcutContext(Qt::WindowShortcut);
		addAction(HeaderSourceAction);
		ViewMenu->addAction(HeaderSourceAction);
		ViewMenu->addSeparator();
		connect(HeaderSourceAction, &QAction::triggered, this, [this] {
			if (HeaderSourceAction->isEnabled() && HeaderSourceDispatch) {
				HeaderSourceDispatch();
			}
		});

		LineNumbersAction
			= new QAction{ QStringLiteral("&Line numbers"), this };
		LineNumbersAction->setObjectName(
			QStringLiteral("toggleWorkbenchLineNumbersAction"));
		LineNumbersAction->setCheckable(true);
		LineNumbersAction->setChecked(true);
		LineNumbersAction->setShortcuts(shortcut_keys(shortcut_command::line_numbers));
		LineNumbersAction->setShortcutContext(Qt::WindowShortcut);
		addAction(LineNumbersAction);
		ViewMenu->addAction(LineNumbersAction);
		// `triggered` rather than `toggled`: the checked state is the
		// projection's to decide, and a toggled handler would also fire when
		// the projection sets it, turning one gesture into two.
		connect(LineNumbersAction, &QAction::triggered, this, [this] {
			dispatch_line_numbers_toggle();
		});
		OnePaneAction = new QAction{ QStringLiteral("&One pane"), this };
		OnePaneAction->setObjectName(QStringLiteral("toggleWorkbenchOnePaneAction"));
		OnePaneAction->setCheckable(true);
		// [[BPR-1815]]. Its key, and the review bar's layout toggle.
		OnePaneAction->setShortcuts(shortcut_keys(shortcut_command::one_pane));
		OnePaneAction->setShortcutContext(Qt::WindowShortcut);
		addAction(OnePaneAction);
		ViewMenu->addAction(OnePaneAction);
		connect(OnePaneAction, &QAction::triggered, this, [this] {
			dispatch_one_pane_toggle();
		});
		// [[BPR-1334]]. Line details under the comparison, shown by default.
		// The choice is the profile's review behaviour; the checked mark is
		// what the host published, so a refused save puts it back.
		LineDetailsAction = new QAction{ QStringLiteral("Line &Details"), this };
		LineDetailsAction->setObjectName(QStringLiteral("toggleWorkbenchLineDetailsAction"));
		LineDetailsAction->setCheckable(true);
		LineDetailsAction->setChecked(Behaviour.LineDetails == application::review_line_details::shown);
		LineDetailsAction->setToolTip(QStringLiteral("Show the current line's base and head versions in full under the comparison"));
		LineDetailsAction->setStatusTip(LineDetailsAction->toolTip());
		addAction(LineDetailsAction);
		ViewMenu->addAction(LineDetailsAction);
		connect(LineDetailsAction, &QAction::triggered, this, [this] {
			dispatch_line_details_toggle();
		});
		// [[BPR-1295]]. Hide Drawers. Command-B (Ctrl+B elsewhere), the
		// owner's left-hand key and the one Visual Studio Code uses for its
		// side bar. Checked while the drawers are hidden. Its presenter is the
		// title row's button right of the Comments toggle.
		ToggleDrawersAction = new QAction{ QStringLiteral("Hide &Drawers"), this };
		ToggleDrawersAction->setObjectName(QStringLiteral("toggleWorkbenchDrawersAction"));
		ToggleDrawersAction->setIconText(QStringLiteral("Hide drawers"));
		ToggleDrawersAction->setCheckable(true);
		ToggleDrawersAction->setShortcuts(shortcut_keys(shortcut_command::hide_drawers));
		ToggleDrawersAction->setShortcutContext(Qt::WindowShortcut);
		addAction(ToggleDrawersAction);
		ViewMenu->addAction(ToggleDrawersAction);
		connect(ToggleDrawersAction, &QAction::triggered, this, [this] {
			dispatch_drawers_toggle();
		});
		// [[BPR-1293]]. Command-J (Ctrl+J), unbound before: straight to
		// Threads from Overview, Review or Discuss, and back from it.
		ShowThreadsAction = new QAction{ QStringLiteral("&Threads"), this };
		ShowThreadsAction->setObjectName(QStringLiteral("showWorkbenchThreadsAction"));
		ShowThreadsAction->setShortcuts(shortcut_keys(shortcut_command::find_thread));
		ShowThreadsAction->setShortcutContext(Qt::WindowShortcut);
		ShowThreadsAction->setEnabled(false);
		{
			const auto tip = QStringLiteral("Find a thread in the pull request (%1)").arg(ShowThreadsAction->shortcut().toString(QKeySequence::NativeText));
			ShowThreadsAction->setToolTip(tip);
			ShowThreadsAction->setStatusTip(tip);
		}
		addAction(ShowThreadsAction);
		ViewMenu->addAction(ShowThreadsAction);
		connect(ShowThreadsAction, &QAction::triggered, this, [this] { toggle_threads(); });
		// [[BPR-1581]]. The selected thread in the other view: Option-Command-3
		// beside Command-3 for Discuss, Option-Command-2 beside Command-2 for
		// Review. Ctrl+Alt is AltGr on Windows, which types a character with
		// these digits, so both step aside while typing.
		ShowInDiscussAction = new QAction{ QStringLiteral("Show Thread in Discuss"), this };
		ShowInDiscussAction->setObjectName(QStringLiteral("showThreadInDiscussAction"));
		ShowInDiscussAction->setShortcuts(shortcut_keys(shortcut_command::show_in_discuss));
		ShowInDiscussAction->setShortcutContext(Qt::WindowShortcut);
		ShowInReviewAction = new QAction{ QStringLiteral("Show Thread in Review"), this };
		ShowInReviewAction->setObjectName(QStringLiteral("showThreadInReviewAction"));
		ShowInReviewAction->setShortcuts(shortcut_keys(shortcut_command::show_in_review));
		ShowInReviewAction->setShortcutContext(Qt::WindowShortcut);
		for (auto* const action : { ShowInDiscussAction, ShowInReviewAction }) {
			const auto tip = action == ShowInDiscussAction ? QStringLiteral("Show the selected thread in Discuss (%1)") : QStringLiteral("Show the selected thread in Review, beside its code (%1)");
			action->setToolTip(tip.arg(action->shortcut().toString(QKeySequence::NativeText)));
			action->setStatusTip(action->toolTip());
			action->setEnabled(false);
			addAction(action);
			ViewMenu->addAction(action);
		}
		connect(ShowInDiscussAction, &QAction::triggered, this, [this] { static_cast<void>(show_selected_thread(core::workbench_thread_destination::discuss)); });
		connect(ShowInReviewAction, &QAction::triggered, this, [this] { static_cast<void>(show_selected_thread(core::workbench_thread_destination::review)); });
		// [[BPR-1586]]. Option-Command-4 beside Command-4 for Threads.
		ShowInThreadsAction = new QAction{ QStringLiteral("Show Thread in Threads"), this };
		ShowInThreadsAction->setObjectName(QStringLiteral("showThreadInThreadsAction"));
		ShowInThreadsAction->setShortcuts(shortcut_keys(shortcut_command::show_in_threads));
		ShowInThreadsAction->setShortcutContext(Qt::WindowShortcut);
		ShowInThreadsAction->setToolTip(QStringLiteral("Show the selected thread's row in Threads (%1)").arg(ShowInThreadsAction->shortcut().toString(QKeySequence::NativeText)));
		ShowInThreadsAction->setStatusTip(ShowInThreadsAction->toolTip());
		ShowInThreadsAction->setEnabled(false);
		addAction(ShowInThreadsAction);
		ViewMenu->addAction(ShowInThreadsAction);
		connect(ShowInThreadsAction, &QAction::triggered, this, [this] { static_cast<void>(show_selected_thread(core::workbench_thread_destination::threads)); });
		EditorKeyYield = new editor_key_yield{ this, ToggleDrawersAction, editor_native_keys() };
		connect(qApp, &QApplication::focusChanged, this, [this](QWidget* const aOld, QWidget* const aNow) {
			update_header_source_action();
			update_source_find_actions();
			update_typing_keys();
			if (aNow != nullptr && editable_multi_line(aNow) && isAncestorOf(aNow)) {
				aNow->installEventFilter(EditorKeyYield);
			}
			// The command is disabled while an editor here has the keyboard,
			// which is what keeps the native macOS menu item from taking the
			// key: AppKit fires a menu key equivalent without asking Qt's
			// focus object (qt_hide_drawers_native_menu_key).
			if ((aOld != nullptr && isAncestorOf(aOld)) || (aNow != nullptr && isAncestorOf(aNow))) {
				apply_drawers_toggle();
			}
		});
		// [[BPR-1130]]. The fold's command, which the menu offers by keyboard as
		// the review bar's disclosure offers it by pointer. It takes no key of
		// its own: RFC-BPR-0052 decided the window's keys, and none is the fold.
		UpdatesLineAction = new QAction{ QStringLiteral("&Updates Line"), this };
		UpdatesLineAction->setObjectName(QStringLiteral("toggleUpdatesLineAction"));
		UpdatesLineAction->setCheckable(true);
		addAction(UpdatesLineAction);
		ViewMenu->addAction(UpdatesLineAction);
		connect(UpdatesLineAction, &QAction::triggered, this, [this] {
			dispatch_updates_line_toggle();
		});
		// [[BPR-1336]]. The comparison's zoom. Command-= and Command-+ (Ctrl+=
		// and Ctrl++) zoom in and Command-minus zooms out, as every browser and
		// editor does. [[BPR-1535]]: Actual Size is Command-0 (Ctrl+0), which
		// was Home's until the owner moved Home to Command-Shift-H; it replaces
		// Command-Option-0 rather than adding to it. Inside the open label menu
		// Command-0 is still Plain: the popup claims it before the window.
		ViewMenu->addSeparator();
		const auto zoomAction = [this](const QString& aText, const QString& aName, const shortcut_command aKeys, const core::code_zoom_step aStep) {
			auto* const action = new QAction{ aText, this };
			action->setObjectName(aName);
			action->setShortcuts(shortcut_keys(aKeys));
			action->setShortcutContext(Qt::WindowShortcut);
			action->setEnabled(false);
			addAction(action);
			ViewMenu->addAction(action);
			connect(action, &QAction::triggered, this, [this, aStep] { dispatch_code_zoom(aStep); });
			return action;
		};
		ZoomInAction     = zoomAction(tr("Zoom In"), QStringLiteral("zoomInCodeAction"), shortcut_command::zoom_in, core::code_zoom_step::in);
		ZoomOutAction    = zoomAction(tr("Zoom Out"), QStringLiteral("zoomOutCodeAction"), shortcut_command::zoom_out, core::code_zoom_step::out);
		ActualSizeAction = zoomAction(tr("Actual Size"), QStringLiteral("actualSizeCodeAction"), shortcut_command::actual_size, core::code_zoom_step::actual_size);
		apply_code_zoom();
#if defined(Q_OS_WIN)
		qApp->installEventFilter(new typed_text_yield{ this });
#endif
		qApp->installEventFilter(new typed_text_key_claim{ this, [this] { update_typing_keys(); } });
		ViewMenu->addSeparator();
		// [[BPR-1132]]. The tab keys, window-owned: Ctrl+Tab and Ctrl+Shift+Tab
		// move between the tabs (the Control key on macOS too, where Qt names
		// it Meta), and Cmd-W (Ctrl+W) closes the active tab. Nothing had
		// Cmd-W before, so no window command moves to Cmd-Shift-W.
		const auto tabAction = [this](const QString& aText, const QString& aName, const shortcut_command aKey) {
			auto* const action = new QAction{ aText, this };
			action->setObjectName(aName);
			action->setShortcuts(shortcut_keys(aKey));
			action->setShortcutContext(Qt::WindowShortcut);
			action->setEnabled(false);
			addAction(action);
			return action;
		};
		NextTabAction     = tabAction(tr("Next Tab"), QStringLiteral("nextDocumentTabAction"), shortcut_command::next_tab);
		PreviousTabAction = tabAction(tr("Previous Tab"), QStringLiteral("previousDocumentTabAction"), shortcut_command::previous_tab);
		CloseTabAction    = tabAction(tr("Close Tab"), QStringLiteral("closeDocumentTabAction"), shortcut_command::close_tab);
		ViewMenu->addAction(NextTabAction);
		ViewMenu->addAction(PreviousTabAction);
		fileMenu->addAction(CloseTabAction);
		connect(NextTabAction, &QAction::triggered, this, [this] {
			if (DocumentTabDispatch) {
				DocumentTabDispatch(core::activate_adjacent_tab_intent{ .Direction = core::change_navigation_direction::next });
			}
		});
		connect(PreviousTabAction, &QAction::triggered, this, [this] {
			if (DocumentTabDispatch) {
				DocumentTabDispatch(core::activate_adjacent_tab_intent{ .Direction = core::change_navigation_direction::previous });
			}
		});
		connect(CloseTabAction, &QAction::triggered, this, [this] {
			if (!DocumentTabDispatch || !TabsProjection) {
				return;
			}
			const auto active = std::ranges::find(TabsProjection->Tabs, true, &application::document_tab_entry::Active);
			if (active != TabsProjection->Tabs.end()) {
				DocumentTabDispatch(core::close_tab_intent{ .Tab = active->Id });
			}
		});
		// [[BPR-0646]]. Settings is a window, reached by the platform's
		// preferences shortcut; on macOS the role places it in the app menu.
		auto* diagnosticMenu = menuBar()->addMenu(QStringLiteral("&Help"));
		diagnosticMenu->setObjectName(QStringLiteral("releaseUpdateMenu"));
		// [[BPR-1567]]. Keyboard Shortcuts, first: Command-/ (Ctrl+/), the key
		// Slack and Google's editors list their shortcuts with. Command-? is
		// macOS's own Help-menu search, so it is not taken.
		KeyboardShortcutsAction = new QAction{ QStringLiteral("&Keyboard Shortcuts"), this };
		KeyboardShortcutsAction->setObjectName(QStringLiteral("keyboardShortcutsAction"));
		KeyboardShortcutsAction->setShortcuts(shortcut_keys(shortcut_command::keyboard_shortcuts));
		KeyboardShortcutsAction->setShortcutContext(Qt::WindowShortcut);
		KeyboardShortcutsAction->setStatusTip(QStringLiteral("List every keyboard shortcut, grouped by area and searchable"));
		addAction(KeyboardShortcutsAction);
		diagnosticMenu->addAction(KeyboardShortcutsAction);
		diagnosticMenu->addSeparator();
		connect(KeyboardShortcutsAction, &QAction::triggered, this, [this] { show_keyboard_shortcuts(); });
		auto* insights = diagnosticMenu->addAction(QStringLiteral("Insights…"));
		insights->setObjectName(QStringLiteral("openInsightsAction"));
#if defined(Q_OS_MACOS)
		static_cast<void>(use_native_insights_menu(*insights, *menuBar()));
#endif
		insights->setShortcuts(shortcut_keys(shortcut_command::insights));
		addAction(insights);
		connect(insights, &QAction::triggered, this, [this] { if (InsightsPresenter) { InsightsPresenter(); } });
		auto* diagnostics = diagnosticMenu->addAction(QStringLiteral("Diagnostics…"));
		diagnostics->setObjectName(QStringLiteral("openDiagnosticsAction"));
		connect(diagnostics, &QAction::triggered, this, [this] { open_settings(settings_page::storage_and_privacy); });
		diagnosticMenu->addSeparator();
		const auto addExport = [this, diagnosticMenu](const QString& aLabel, const QString& aName, core::diagnostic_export_policy aPolicy) {
			auto* action = diagnosticMenu->addAction(aLabel);
			action->setObjectName(aName);
			connect(action, &QAction::triggered, this, [this, aPolicy] { if (DiagnosticExporter) { DiagnosticExporter(aPolicy); } });
		};
		addExport(QStringLiteral("Export anonymized diagnostics…"), QStringLiteral("exportAnonymizedDiagnosticsAction"), core::diagnostic_export_policy::anonymized);
		addExport(QStringLiteral("Export with identifying details…"), QStringLiteral("exportIdentifyingDiagnosticsAction"), core::diagnostic_export_policy::identifying);
		SettingsAction = new QAction{ QStringLiteral("&Settings…"), this };
		SettingsAction->setObjectName(QStringLiteral("openSettingsAction"));
		SettingsAction->setMenuRole(QAction::PreferencesRole);
		// Preferences has no standard binding on Windows/GNOME. Qt maps CTRL
		// to Command on macOS; keep the explicit product shortcut on every host.
		SettingsAction->setShortcuts(shortcut_keys(shortcut_command::settings));
		// Each workbench owns this action. Application scope makes sibling
		// windows' identical shortcuts ambiguous instead of selecting the caller.
		SettingsAction->setShortcutContext(Qt::WindowShortcut);
		addAction(SettingsAction);
		ViewMenu->addSeparator();
		ViewMenu->addAction(SettingsAction);
		connect(SettingsAction, &QAction::triggered, this, [this] {
			open_settings(settings_page::appearance);
		});
	}
}
