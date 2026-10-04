#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/window_system_keys.hpp"
#include "../support/window_system_mouse.hpp"
#include "bpr/adapters/sqlite/tour_owner_channel.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/core/review_cycle_round.hpp"
#include "bpr/qt/review_cycle_share_sheet.hpp"
#include "bpr/qt/review_cycle_view.hpp"
#include "bpr/qt/suggestion_composer.hpp"
#include "bpr/qt/suggestion_editor.hpp"
#include "bpr/qt/workbench_strip.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "large_review_observers.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDialog>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QThread>
#include <QTimer>
#include <QToolButton>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <variant>

#if defined(Q_OS_MACOS)
	#include <qpa/qwindowsysteminterface.h>
#endif

#if defined(_WIN32)
	#if !defined(NOMINMAX)
		#define NOMINMAX
	#endif
	#if !defined(WIN32_LEAN_AND_MEAN)
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <windows.h>
#endif

/// \file
/// \brief [[BPR-1166]]. The reviewer's side of the complete manual two-round
/// course, in the application's own profile desktop: the production
/// composition, its window, its local Review Cycles page and its share sheet,
/// with the platform clipboard and the application's own file dialogs.
///
/// \details One process is one launch of the desktop. The course
/// (`review_cycle_two_round_contract.py`) starts it, asks for one reviewer
/// gesture at a time through a command file, edits the folder with ordinary
/// file writes while the desktop is closed or open, quits it and starts it
/// again on the same profile. Gestures go through the window system: clicks
/// and double clicks at the control's middle, a drag across the bytes a note
/// is about, Return and Shift+Return in the code, and every character of a
/// note, a suggestion and a goal typed as a key. The application's file
/// dialogs are its own Qt dialogs (no native panel can be answered by a
/// course), and the course names the path in them. Menus are chosen by their
/// words. Each reply is one line, `REPLY <id> <json>`, of what the window shows.
///
/// [[BPR-1171]]'s parity course (`review_cycle_parity_contract.py`) drives the
/// same reviewer beside the shipped `bpr cycle` and a declared harness: it adds
/// a note left unsaved, a goal typed and not entered, and waits for what other
/// interfaces wrote to reach the window.
namespace {
	using namespace bpr;
	using access = app::detail::github_application_fixture_access;

	void say(const std::string& aLine)
	{
		std::cout << aLine << '\n'
				  << std::flush;
	}

	/// \brief [[BPR-1184]]. The longest single turn of the event loop the
	/// course drives since the last reset: how long the GUI thread was busy
	/// with work it was handed, which an input event arriving meanwhile would
	/// have waited behind. One turn can deliver several events, so this is
	/// an upper bound on any one event's wait.
	auto longest_turn_ns() -> qint64&
	{
		static auto longest = qint64{ 0 };
		return longest;
	}

	void turn()
	{
		QElapsedTimer busy;
		busy.start();
		QApplication::processEvents();
		longest_turn_ns() = std::max(longest_turn_ns(), busy.nsecsElapsed());
	}

	/// The course's own clock for one wait; [[BPR-1184]]'s large projects ask for longer.
	auto wait_scale() -> int&
	{
		static auto scale = 1;
		return scale;
	}

	/// [[BPR-1184]]. How long one course command may take, from the course, and
	/// the clock of the command now running: every wait inside a command ends
	/// before the course stops waiting for its reply, so a stalled gesture
	/// replies with what the window shows instead of going silent.
	auto gesture_budget_ms() -> qint64&
	{
		static auto budget = qint64{ 0 };
		return budget;
	}

	auto gesture_clock() -> QElapsedTimer&
	{
		static auto clock = QElapsedTimer{ };
		return clock;
	}

	auto wait_for(const std::function<bool()>& aCondition, const int aMilliseconds = 30000) -> bool
	{
		QElapsedTimer elapsed;
		elapsed.start();
		auto limit = static_cast<qint64>(aMilliseconds) * wait_scale();
		if (gesture_budget_ms() > 0 && gesture_clock().isValid()) {
			limit = std::min(limit, std::max(qint64{ 0 }, gesture_budget_ms() - gesture_clock().elapsed()));
		}
		while (!aCondition() && elapsed.elapsed() < limit) {
			turn();
			QThread::msleep(1);
		}
		return aCondition();
	}

	/// \brief Whether the owner's command endpoint is bound, without taking it
	/// (as `profile_cycle_handoff_public_path_tests.cpp` explains for Windows).
	auto endpoint_bound(const std::filesystem::path& aEndpoint) -> bool
	{
#if defined(_WIN32)
		return ::WaitNamedPipeW(aEndpoint.c_str(), 1) != FALSE || ::GetLastError() == ERROR_SEM_TIMEOUT;
#else
		std::error_code code;
		return std::filesystem::exists(aEndpoint, code);
#endif
	}

	auto read_file(const std::filesystem::path& aFile) -> std::string
	{
		std::ifstream     in{ aFile, std::ios::binary };
		std::stringstream text;
		text << in.rdbuf();
		return text.str();
	}

	auto utf8(const QString& aText) -> std::string
	{
		const auto bytes = aText.toUtf8();
		return { bytes.constData(), static_cast<std::size_t>(bytes.size()) };
	}

	auto text_of(const std::string& aText) -> QString
	{
		return QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size()));
	}

	/// A gesture the window did not answer as the course expected.
	struct refused
	{
		std::string What;
	};

	void require(const bool aCondition, const std::string& aWhat)
	{
		if (!aCondition) {
			throw refused{ aWhat };
		}
	}

	/// The control that has focus, by name, for a refusal to say.
	auto focus_name() -> std::string
	{
		const auto* const focus  = QApplication::focusWidget();
		const auto*       active = QApplication::activeWindow();
		return (focus != nullptr ? utf8(focus->objectName()) + " (" + focus->metaObject()->className() + ")" : std::string{ "nothing" }) + " in "
			+ (active != nullptr ? utf8(active->objectName()) + " (" + active->metaObject()->className() + ")" : std::string{ "no active window" });
	}

	auto idle(const qt::review_cycle_view& aPage) -> bool
	{
		return aPage.projection().Activity == application::review_cycle_activity::idle;
	}

	/// [[BPR-1184]]. Nothing loading: idle, or only a capture running beside the reading.
	auto settled(const qt::review_cycle_view& aPage) -> bool
	{
		return idle(aPage) || aPage.projection().Activity == application::review_cycle_activity::capturing;
	}

	auto capturing(const qt::review_cycle_view& aPage) -> bool
	{
		return aPage.projection().Capturing.has_value();
	}

	/// \brief Gives `aWindow` back the keyboard after a dialog over it closed,
	/// as the window system does for a hand; offscreen leaves no window active.
	///
	/// \details On macOS a session that keeps another application in front
	/// (someone using it, or the terminal that started the course) never makes
	/// this window key, however long the course waits. The course then
	/// delivers the key-window change through the QPA entry point the cocoa
	/// plugin itself calls, and says so once; the person's own keys stay with
	/// their application.
	void reactivate(QWidget& aWindow)
	{
		if (aWindow.isActiveWindow()) {
			return;
		}
#if defined(Q_OS_MACOS)
		static auto delivered = false;
		if (!delivered) {
			aWindow.activateWindow();
			if (wait_for([&] { return aWindow.isActiveWindow(); }, 3000)) {
				return;
			}
			say("NOTE key-window changes are delivered through QPA: the session keeps another application in front");
			delivered = true;
		}
		// Someone using another application can take activation away mid-course too.
		QWindowSystemInterface::handleFocusWindowChanged(aWindow.windowHandle(), Qt::ActiveWindowFocusReason);
		wait_for([&] { return aWindow.isActiveWindow(); }, 3000);
#else
		aWindow.activateWindow();
		wait_for([&] { return aWindow.isActiveWindow(); }, 3000);
#endif
	}

	/// One click at the middle of a visible control, through the window system.
	void click(QWidget& aControl)
	{
		const auto* const button = qobject_cast<const QAbstractButton*>(&aControl);
		require(aControl.isVisible() && aControl.isEnabled(), "a visible, enabled " + utf8(aControl.objectName()) + " " + utf8(aControl.accessibleName()) + " " + (button != nullptr ? utf8(button->text()) : std::string{ }) + " (visible " + (aControl.isVisible() ? "1" : "0") + ", enabled " + (aControl.isEnabled() ? "1" : "0") + ", window " + std::to_string(aControl.window()->width()) + "x" + std::to_string(aControl.window()->height()) + ")");
		tests::window_system_clicks(aControl, QRectF{ aControl.rect() }.center(), 1);
	}

	/// \brief Types `aText` into whatever has focus, one key per character, as
	/// the window system reports a hand's keys.
	void type(QWidget& aWindow, const QString& aText)
	{
		reactivate(*aWindow.window());
		for (const auto character : aText) {
			const auto upper = character.toUpper().unicode();
			const auto key   = character == QLatin1Char(' ') ? int{ Qt::Key_Space } : int{ upper };
			const auto shift = character.isUpper() ? Qt::ShiftModifier : Qt::NoModifier;
			auto*      focus = QApplication::focusWidget();
			tests::window_system_key(focus != nullptr ? *focus : aWindow, key, shift, QString{ character });
		}
	}

	/// \brief Answers the application's next file dialog with `aPath`, as the
	/// reviewer would type it in, and accepts it.
	void answer_next_file_dialog(const QString& aPath, bool& aAnswered)
	{
		auto* const timer = new QTimer{ };
		timer->setInterval(20);
		QObject::connect(timer, &QTimer::timeout, [timer, aPath, &aAnswered] {
			auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
			for (auto* const top : QApplication::topLevelWidgets()) {
				if (dialog == nullptr && top->isVisible()) {
					dialog = qobject_cast<QFileDialog*>(top);
				}
			}
			if (dialog == nullptr || !dialog->isVisible()) {
				return;
			}
			timer->stop();
			timer->deleteLater();
			if (dialog->fileMode() == QFileDialog::Directory) {
				// Open Project: the folder itself, entered, with no name typed below it.
				dialog->setDirectory(aPath);
				dialog->selectFile(QString{ });
			} else {
				dialog->selectFile(aPath);
			}
			// QDialog's public accept, which QFileDialog overrides to check the name.
			static_cast<QDialog*>(dialog)->accept();
			aAnswered = true;
		});
		timer->start();
	}

	auto json_line(const QJsonObject& aObject) -> std::string
	{
		return utf8(QString::fromUtf8(QJsonDocument{ aObject }.toJson(QJsonDocument::Compact)));
	}

	auto item_name(const core::cycle_feedback_item_ref& aItem) -> QString
	{
		return text_of(core::cycle_wire_name(aItem.Id));
	}

	/// What the window shows of the cycle: identity, captures, rounds, notes and statements.
	auto state(qt::workbench_window& aWindow, const qt::review_cycle_view& aPage) -> QJsonObject
	{
		const auto& shown               = aPage.projection();
		auto        object              = QJsonObject{ };
		object[QStringLiteral("home")]  = aWindow.showing_home();
		object[QStringLiteral("cycle")] = aWindow.showing_local_cycle() && shown.Cycle ? text_of(core::cycle_wire_name(shown.Cycle->Subject.Cycle)) : QString{ };
		object[QStringLiteral("title")] = shown.Cycle ? text_of(shown.Cycle->Title) : QString{ };
		auto revisions                  = QJsonArray{ };
		for (const auto revision : shown.Revisions) {
			revisions.append(text_of(core::cycle_wire_name(revision)));
		}
		object[QStringLiteral("revisions")] = revisions;
		auto rounds                         = QJsonArray{ };
		for (const auto& round : shown.Facts.Rounds) {
			rounds.append(QJsonObject{ { QStringLiteral("round"), text_of(core::cycle_wire_name(round.Round)) }, { QStringLiteral("start"), text_of(core::cycle_wire_name(round.Start)) } });
		}
		object[QStringLiteral("rounds")] = rounds;
		auto notes                       = QJsonArray{ };
		for (auto row = 0; row < aPage.notes()->count() && static_cast<std::size_t>(row) < shown.Notes.size(); ++row) {
			const auto& note = shown.Notes[static_cast<std::size_t>(row)];
			notes.append(QJsonObject{
				{ QStringLiteral("item"), item_name(note.Item) },
				{ QStringLiteral("body"), text_of(note.Body) },
				{ QStringLiteral("suggestions"), static_cast<qint64>(note.Suggestions) },
				{ QStringLiteral("related"), static_cast<qint64>(note.Related.size()) },
				{ QStringLiteral("shown"), aPage.notes()->item(row)->text() },
			});
		}
		object[QStringLiteral("notes")] = notes;
		// [[BPR-1171]]. Every row the list shows, response rows after the notes included.
		auto listed = QJsonArray{ };
		for (auto row = 0; row < aPage.notes()->count(); ++row) {
			listed.append(aPage.notes()->item(row)->text());
		}
		object[QStringLiteral("listed")] = listed;
		// The note being written and not yet saved, and where the keyboard is.
		auto* const editor                  = aPage.comment_editor();
		object[QStringLiteral("editor")]    = QJsonObject{ { QStringLiteral("visible"), editor->isVisible() }, { QStringLiteral("text"), editor->toPlainText() }, { QStringLiteral("focused"), QApplication::focusWidget() == editor } };
		object[QStringLiteral("responses")] = static_cast<qint64>(shown.Responses.size());
		object[QStringLiteral("updates")]   = aPage.updates_line()->text();
		object[QStringLiteral("message")]   = text_of(shown.Message);
		object[QStringLiteral("source")]    = shown.Source ? text_of(shown.Source->Path) + QStringLiteral(" @ ") + text_of(core::cycle_wire_name(shown.Source->Revision)) : QString{ };
		object[QStringLiteral("newer")]     = shown.NewerAvailable;
		auto* const label                   = aPage.findChild<QLabel*>(QStringLiteral("cycleRevisionLabel"));
		object[QStringLiteral("label")]     = label != nullptr ? label->text() : QString{ };
		// [[BPR-1184]]. What the reading pane says in place of bytes it does not show.
		auto* const notice               = aPage.findChild<QLabel*>(QStringLiteral("cycleSourceNotice"));
		object[QStringLiteral("notice")] = notice != nullptr && notice->isVisible() ? notice->text() : QString{ };
		return object;
	}

	/// The share sheet as it stands: its round, its notes and checks, its routes and what was observed.
	auto share_state(const qt::review_cycle_view& aPage) -> QJsonObject
	{
		const auto& shown              = aPage.projection().Share;
		auto* const sheet              = aPage.share_page();
		auto        object             = QJsonObject{ };
		object[QStringLiteral("open")] = sheet->isVisible() && shown.has_value();
		if (!shown) {
			return object;
		}
		object[QStringLiteral("title")]    = sheet->title()->text();
		object[QStringLiteral("round")]    = text_of(core::cycle_wire_name(shown->Round));
		object[QStringLiteral("frozen")]   = shown->Frozen;
		object[QStringLiteral("revision")] = static_cast<qint64>(shown->Draft.Revision);
		object[QStringLiteral("field")]    = sheet->goal()->text();
		object[QStringLiteral("modified")] = sheet->goal()->isModified();
		object[QStringLiteral("goal")]     = text_of(shown->Selection.Goal);
		object[QStringLiteral("status")]   = sheet->status()->text();
		auto rows                          = QJsonArray{ };
		for (const auto& row : sheet->rows()) {
			rows.append(QJsonObject{ { QStringLiteral("item"), text_of(core::cycle_wire_name(row.Item)) }, { QStringLiteral("included"), row.Include->isChecked() } });
		}
		object[QStringLiteral("rows")] = rows;
		auto       routes              = QJsonObject{ };
		const auto offered             = [&](const core::cycle_share_route aRoute) {
			auto* const button = sheet->route_button(aRoute);
			return button != nullptr && button->isVisible() && button->isEnabled();
		};
		routes[QStringLiteral("copy")]           = offered(core::cycle_share_route::copy_document);
		routes[QStringLiteral("save")]           = offered(core::cycle_share_route::save_document);
		routes[QStringLiteral("bundle")]         = offered(core::cycle_share_route::save_bundle);
		routes[QStringLiteral("external_agent")] = offered(core::cycle_share_route::external_agent);
		routes[QStringLiteral("send_to_agent")]  = offered(core::cycle_share_route::send_to_agent);
		object[QStringLiteral("routes")]         = routes;
		auto deliveries                          = QJsonArray{ };
		for (auto row = 0; row < sheet->deliveries()->count(); ++row) {
			deliveries.append(sheet->deliveries()->item(row)->text());
		}
		object[QStringLiteral("deliveries")] = deliveries;
		object[QStringLiteral("document")]   = text_of(shown->Document);
		return object;
	}

	/// The comparison the reading pane shows: its pair, its rows and the text drawn.
	auto comparison_state(const qt::review_cycle_view& aPage) -> QJsonObject
	{
		const auto& shown                 = aPage.projection().Comparison;
		auto        object                = QJsonObject{ };
		auto* const label                 = aPage.findChild<QLabel*>(QStringLiteral("cycleRevisionLabel"));
		object[QStringLiteral("label")]   = label != nullptr ? label->text() : QString{ };
		object[QStringLiteral("visible")] = aPage.comparison()->isVisible();
		if (!shown) {
			return object;
		}
		object[QStringLiteral("path")]   = text_of(shown->Path);
		object[QStringLiteral("left")]   = shown->Pair.Left ? text_of(core::cycle_wire_name(shown->Pair.Left->Id)) : QStringLiteral("none");
		object[QStringLiteral("right")]  = text_of(core::cycle_wire_name(shown->Pair.Right.Id));
		object[QStringLiteral("pinned")] = shown->Pinned;
		auto rows                        = QJsonArray{ };
		for (const auto& row : shown->Rows) {
			const auto sign = row.Kind == core::cycle_diff_row_kind::added ? QStringLiteral("+") : row.Kind == core::cycle_diff_row_kind::removed ? QStringLiteral("-")
																																				  : QStringLiteral(" ");
			rows.append(QJsonArray{ sign, row.LeftLine ? static_cast<qint64>(*row.LeftLine) : qint64{ 0 }, row.RightLine ? static_cast<qint64>(*row.RightLine) : qint64{ 0 }, text_of(row.Text) });
		}
		object[QStringLiteral("rows")]  = rows;
		object[QStringLiteral("drawn")] = aPage.comparison()->toPlainText();
		return object;
	}

	auto row_of(const qt::review_cycle_view& aPage, const QString& aItem) -> int
	{
		const auto& notes = aPage.projection().Notes;
		for (std::size_t row = 0; row < notes.size(); ++row) {
			if (item_name(notes[row].Item) == aItem) {
				return static_cast<int>(row);
			}
		}
		throw refused{ "no note " + utf8(aItem) };
	}

	/// A double click on one entry of a list, as a reviewer opens it.
	void activate(QListWidget& aList, QListWidgetItem& aItem)
	{
		aList.scrollToItem(&aItem);
		QApplication::processEvents();
		tests::window_system_clicks(*aList.viewport(), QRectF{ aList.visualItemRect(&aItem) }.center(), 2);
	}

	/// The menu entry reached by `aPath`, word by word, through submenus.
	auto menu_entry(QMenu& aMenu, const QStringList& aPath) -> QAction*
	{
		emit  aMenu.aboutToShow();
		auto* menu = &aMenu;
		for (qsizetype step = 0; step < aPath.size(); ++step) {
			QAction* found = nullptr;
			for (auto* const action : menu->actions()) {
				if (action->text() == aPath[step]) {
					found = action;
				}
			}
			if (found == nullptr || step + 1 == aPath.size()) {
				return found;
			}
			menu = found->menu();
			if (menu == nullptr) {
				return nullptr;
			}
			emit menu->aboutToShow();
		}
		return nullptr;
	}

	class reviewer
	{
	public:
		reviewer(qt::workbench_window& aWindow, qt::review_cycle_view& aPage)
			: Window{ aWindow }
			, Page{ aPage }
		{ }

		auto run(const QJsonObject& aCommand) -> QJsonObject
		{
			const auto verb = aCommand[QStringLiteral("verb")].toString();
			// Keys reach only the active window: the sheet while it is open, else the window.
			reactivate(Page.share_page()->isVisible() ? static_cast<QWidget&>(*Page.share_page()) : static_cast<QWidget&>(Window));
			if (verb == QStringLiteral("state")) {
				return state(Window, Page);
			}
			if (verb == QStringLiteral("open-project")) {
				return open_project(aCommand[QStringLiteral("path")].toString());
			}
			if (verb == QStringLiteral("resume")) {
				return resume(aCommand[QStringLiteral("title")].toString(), static_cast<std::size_t>(aCommand[QStringLiteral("notes")].toInteger()));
			}
			if (verb == QStringLiteral("open-file")) {
				open_file(aCommand[QStringLiteral("path")].toString());
				return state(Window, Page);
			}
			if (verb == QStringLiteral("note")) {
				return note(aCommand);
			}
			if (verb == QStringLiteral("draft-note")) {
				return draft_note(aCommand);
			}
			if (verb == QStringLiteral("save-note")) {
				const auto before = Page.projection().Notes.size();
				click(*Page.findChild<QPushButton*>(QStringLiteral("cycleCommentSave")));
				require(wait_for([&] { return idle(Page) && Page.projection().Notes.size() == before + 1U; }), "the note being written is saved and listed");
				auto object                     = state(Window, Page);
				object[QStringLiteral("added")] = item_name(Page.projection().Notes.back().Item);
				return object;
			}
			if (verb == QStringLiteral("home")) {
				// [[BPR-1171]]. Home, by its shortcut, over the open cycle.
				reactivate(Window);
				tests::window_system_key(Window, Qt::Key_0, Qt::ControlModifier);
				require(wait_for([&] { return Window.showing_home(); }), "Home shows over the open cycle");
				return state(Window, Page);
			}
			if (verb == QStringLiteral("wait-notes")) {
				// [[BPR-1171]]. Until the window shows notes another interface wrote.
				const auto count = static_cast<std::size_t>(aCommand[QStringLiteral("count")].toInteger());
				require(wait_for([&] { return idle(Page) && Page.projection().Notes.size() >= count && Page.notes()->count() >= static_cast<int>(count); }), "the window shows " + std::to_string(count) + " notes");
				return state(Window, Page);
			}
			if (verb == QStringLiteral("wait-responses")) {
				const auto count = static_cast<std::size_t>(aCommand[QStringLiteral("count")].toInteger());
				require(wait_for([&] { return idle(Page) && Page.projection().Responses.size() >= count; }), "the window shows " + std::to_string(count) + " responses");
				return state(Window, Page);
			}
			if (verb == QStringLiteral("share-state")) {
				// Until the open sheet shows a draft revision, or the round frozen, another interface made.
				const auto revision = static_cast<std::uint64_t>(aCommand[QStringLiteral("revision")].toInteger());
				const auto frozen   = aCommand[QStringLiteral("frozen")].toBool();
				require(wait_for([&] {
							const auto& shown = Page.projection().Share;
							return idle(Page) && shown.has_value() && shown->Draft.Revision >= revision && (!frozen || shown->Frozen);
						}),
						"the sheet shows draft revision " + std::to_string(revision) + (frozen ? ", frozen" : ""));
				return share_state(Page);
			}
			if (verb == QStringLiteral("suggest")) {
				return suggest(aCommand);
			}
			if (verb == QStringLiteral("relate")) {
				return relate(aCommand);
			}
			if (verb == QStringLiteral("compose")) {
				click(*Page.review_bar()->widgetForAction(Page.compose_action()));
				require(wait_for([&] { return Page.projection().Share && Page.share_page()->isVisible() && idle(Page) && !Page.projection().Share->Document.empty(); }), "the share sheet opens with its document");
				return share_state(Page);
			}
			if (verb == QStringLiteral("include")) {
				return include(aCommand[QStringLiteral("item")].toString(), aCommand[QStringLiteral("on")].toBool());
			}
			if (verb == QStringLiteral("goal")) {
				return goal(aCommand[QStringLiteral("text")].toString(), !aCommand.contains(QStringLiteral("enter")) || aCommand[QStringLiteral("enter")].toBool(), aCommand[QStringLiteral("replace")].toBool());
			}
			if (verb == QStringLiteral("share")) {
				return share(aCommand);
			}
			if (verb == QStringLiteral("close-share")) {
				auto* const sheet = Page.share_page();
				tests::window_system_key(*sheet, Qt::Key_Escape, Qt::NoModifier, QStringLiteral("\x1b"));
				require(wait_for([&] { return !sheet->isVisible() && !Page.projection().Share && idle(Page); }), "Escape closes the sheet");
				return state(Window, Page);
			}
			if (verb == QStringLiteral("capture")) {
				const auto before = Page.projection().Revisions.size();
				click(*Page.review_bar()->widgetForAction(Page.capture_action()));
				require(wait_for([&] { return idle(Page) && (Page.projection().Revisions.size() > before || Page.updates_line()->text().contains(QStringLiteral("No change"))); }), "a capture is recorded or said to be unchanged");
				return state(Window, Page);
			}
			if (verb == QStringLiteral("view")) {
				return view(aCommand[QStringLiteral("path")].toString(), aCommand[QStringLiteral("menu")].toVariant().toStringList());
			}
			if (verb == QStringLiteral("activate")) {
				// Natively another application in front can keep this window from the keyboard.
				reactivate(Window);
				return QJsonObject{ { QStringLiteral("active"), Window.isActiveWindow() } };
			}
			if (verb == QStringLiteral("render")) {
				return render(aCommand[QStringLiteral("path")].toString());
			}
			if (verb == QStringLiteral("latest")) {
				return latest(aCommand[QStringLiteral("path")].toString());
			}
			if (verb == QStringLiteral("decide")) {
				return decide(aCommand[QStringLiteral("item")].toString(), aCommand[QStringLiteral("choice")].toString());
			}
			if (verb == QStringLiteral("reply")) {
				return reply(aCommand);
			}
			if (verb == QStringLiteral("next-round")) {
				click(*Page.next_round_button());
				require(wait_for([&] { return idle(Page) && Page.projection().NextRound.has_value(); }), "the next round's draft is prepared");
				const auto& next                   = *Page.projection().NextRound;
				auto        object                 = state(Window, Page);
				object[QStringLiteral("carried")]  = static_cast<qint64>(next.Carried);
				object[QStringLiteral("added")]    = static_cast<qint64>(next.Added);
				object[QStringLiteral("resolved")] = static_cast<qint64>(next.Resolved);
				object[QStringLiteral("archived")] = static_cast<qint64>(next.Archived);
				return object;
			}
			if (verb == QStringLiteral("memory")) {
				return memory();
			}
			if (verb == QStringLiteral("next-file")) {
				return next_file();
			}
			if (verb == QStringLiteral("type-during-capture")) {
				return type_during_capture(aCommand);
			}
			if (verb == QStringLiteral("navigate-during-capture")) {
				return navigate_during_capture(aCommand);
			}
			if (verb == QStringLiteral("cancel-capture")) {
				return cancel_capture(aCommand);
			}
			if (verb == QStringLiteral("drawer")) {
				return drawer();
			}
			if (verb == QStringLiteral("open-project-refusal")) {
				// [[BPR-1184]]. File > Open Project on a folder no capture can keep: what the page says.
				auto answered = false;
				answer_next_file_dialog(aCommand[QStringLiteral("path")].toString(), answered);
				Window.open_project_action()->trigger();
				require(answered, "File > Open Project… asked for a folder");
				reactivate(Window);
				require(wait_for([&] { return !Page.projection().Message.empty() && settled(Page) && !capturing(Page); }, 600000), "Open Project says why nothing was captured");
				return state(Window, Page);
			}
			if (verb == QStringLiteral("drawer-page")) {
				return drawer_page(static_cast<int>(aCommand[QStringLiteral("direction")].toInteger()));
			}
			throw refused{ "unknown verb " + utf8(verb) };
		}

	private:
		auto open_project(const QString& aPath) -> QJsonObject
		{
			auto answered = false;
			answer_next_file_dialog(aPath, answered);
			Window.open_project_action()->trigger();
			require(answered, "File > Open Project… asked for a folder");
			reactivate(Window);
			require(wait_for([&] { return Window.showing_local_cycle() && Page.projection().Cycle && !Page.projection().Revisions.empty() && idle(Page) && !Page.projection().Files.empty(); }), "the folder opens as a cycle with its first capture");
			return state(Window, Page);
		}

		/// Resumes the cycle from Home and waits for its `aNotes` notes, which
		/// are read after the cycle itself is shown.
		auto resume(const QString& aTitle, const std::size_t aNotes) -> QJsonObject
		{
			Window.choose_home(true);
			auto* list = Window.home_cycles_list();
			// A cycle another interface opened a moment ago is listed once Home reads the store again.
			const auto listed = [&] {
				for (auto row = 0; row < list->count(); ++row) {
					if (list->item(row)->text().contains(aTitle)) {
						return true;
					}
				}
				return false;
			};
			require(wait_for([&] { return Window.showing_home() && listed(); }), "Home lists the cycle " + utf8(aTitle));
			for (auto row = 0; row < list->count(); ++row) {
				if (list->item(row)->text().contains(aTitle)) {
					const auto resumed = [&] { return Window.showing_local_cycle() && Page.projection().Cycle && idle(Page) && !Page.projection().Files.empty(); };
					activate(*list, *list->item(row));
					if (!wait_for(resumed, 5000)) {
						say("NOTE a double click on Home's " + utf8(aTitle) + " opened nothing; given once more");
						reactivate(Window);
						activate(*list, *list->item(row));
					}
					require(wait_for(resumed), "the cycle resumes from Home");
					wait_for([&] { return idle(Page) && Page.projection().Notes.size() >= aNotes && Page.notes()->count() == static_cast<int>(Page.projection().Notes.size()); }, 10000);
					return state(Window, Page);
				}
			}
			throw refused{ "Home does not list " + utf8(aTitle) };
		}

		void open_file(const QString& aPath)
		{
			auto* const files = Page.files();
			for (auto row = 0; row < files->count(); ++row) {
				if (files->item(row)->data(Qt::UserRole).toString() == aPath) {
					// [[BPR-1184]]. Bytes a capture did not keep open as the notice that says so.
					auto* const notice = Page.findChild<QLabel*>(QStringLiteral("cycleSourceNotice"));
					const auto  opened = [&] {
						const auto& shown = Page.projection();
						const auto  read  = shown.Source && text_of(shown.Source->Path) == aPath && (Page.source()->isVisible() || (notice != nullptr && notice->isVisible()));
						return settled(Page) && (read || (shown.Comparison && text_of(shown.Comparison->Path) == aPath));
					};
					activate(*files, *files->item(row));
					// On a shared native desktop a person's own pointer can come
					// between the two clicks; a double click that opened nothing
					// is given once more, and said.
					if (!wait_for(opened, 5000)) {
						say("NOTE a double click on " + utf8(aPath) + " opened nothing; given once more");
						reactivate(Window);
						activate(*files, *files->item(row));
					}
					// In full source, or in the view the reading pane is in.
					require(wait_for(opened), "the file opens in the reading pane: " + utf8(aPath));
					return;
				}
			}
			throw refused{ "the drawer does not list " + utf8(aPath) };
		}

		/// A drag across bytes [aStart, aEnd) of the code, then focus stays in it.
		void select(const std::uint64_t aStart, const std::uint64_t aEnd)
		{
			auto* const code = Page.source();
			code->setFocus(Qt::MouseFocusReason);
			const auto from = code->cell_rectangle(code->model().point_of(aStart));
			const auto to   = code->cell_rectangle(code->model().point_of(aEnd));
			tests::window_system_mouse(*code->viewport(), QPointF{ from.left() + 1.0, from.center().y() }, QEvent::MouseButtonPress, Qt::LeftButton, 2000);
			tests::window_system_mouse(*code->viewport(), QPointF{ to.left() + 1.0, to.center().y() }, QEvent::MouseMove, Qt::LeftButton);
			tests::window_system_mouse(*code->viewport(), QPointF{ to.left() + 1.0, to.center().y() }, QEvent::MouseButtonRelease, Qt::NoButton);
			const auto selected = code->selected_bytes();
			require(selected == std::pair{ aStart, aEnd }, "a drag selects bytes " + std::to_string(aStart) + ".." + std::to_string(aEnd) + ", not " + std::to_string(selected.first) + ".." + std::to_string(selected.second));
		}

		auto note(const QJsonObject& aCommand) -> QJsonObject
		{
			open_file(aCommand[QStringLiteral("path")].toString());
			select(static_cast<std::uint64_t>(aCommand[QStringLiteral("start")].toInteger()), static_cast<std::uint64_t>(aCommand[QStringLiteral("end")].toInteger()));
			const auto before = Page.projection().Notes.size();
			// Return in the read-only code notes the selection ([[BPR-1157]]).
			reactivate(Window);
			tests::window_system_key(*Page.source(), Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
			require(Page.comment_editor()->isVisible() && QApplication::focusWidget() == Page.comment_editor(), "Return opens a note on the selection; focus is on " + focus_name());
			type(*Page.comment_editor(), aCommand[QStringLiteral("body")].toString());
			click(*Page.findChild<QPushButton*>(QStringLiteral("cycleCommentSave")));
			require(wait_for([&] { return idle(Page) && Page.projection().Notes.size() == before + 1U; }), "the note is saved and listed");
			auto object                     = state(Window, Page);
			object[QStringLiteral("added")] = item_name(Page.projection().Notes.back().Item);
			return object;
		}

		/// \brief [[BPR-1171]]. A note typed on a selection and not saved: the
		/// keyboard stays in it while other interfaces write the cycle.
		auto draft_note(const QJsonObject& aCommand) -> QJsonObject
		{
			open_file(aCommand[QStringLiteral("path")].toString());
			select(static_cast<std::uint64_t>(aCommand[QStringLiteral("start")].toInteger()), static_cast<std::uint64_t>(aCommand[QStringLiteral("end")].toInteger()));
			reactivate(Window);
			tests::window_system_key(*Page.source(), Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
			require(Page.comment_editor()->isVisible() && QApplication::focusWidget() == Page.comment_editor(), "Return opens a note on the selection; focus is on " + focus_name());
			type(*Page.comment_editor(), aCommand[QStringLiteral("body")].toString());
			return state(Window, Page);
		}

		auto suggest(const QJsonObject& aCommand) -> QJsonObject
		{
			open_file(aCommand[QStringLiteral("path")].toString());
			select(static_cast<std::uint64_t>(aCommand[QStringLiteral("start")].toInteger()), static_cast<std::uint64_t>(aCommand[QStringLiteral("end")].toInteger()));
			const auto before = Page.projection().Notes.size();
			// Shift+Return in the code begins Suggest change on the selection.
			reactivate(Window);
			tests::window_system_key(*Page.source(), Qt::Key_Return, Qt::ShiftModifier, QStringLiteral("\r"));
			require(Page.suggesting() && !Page.source()->read_only() && QApplication::focusWidget() == Page.source(), "Shift+Return begins Suggest change in the code");
			// Typing replaces the selection.
			type(*Page.source(), aCommand[QStringLiteral("replacement")].toString());
			auto* const composer = Page.composer();
			require(wait_for([&] { return composer->edits().size() == 1U; }), "one change, as typed");
			if (aCommand[QStringLiteral("illustrative")].toBool()) {
				click(*composer->findChild<QCheckBox*>(QStringLiteral("suggestionIllustrative")));
				require(composer->illustrative(), "marked illustrative only");
			}
			if (const auto rationale = aCommand[QStringLiteral("rationale")].toString(); !rationale.isEmpty()) {
				auto* const field = composer->findChild<QPlainTextEdit*>(QStringLiteral("suggestionRationale"));
				require(field != nullptr, "the suggestion's reason field");
				click(*field);
				type(*field, rationale);
			}
			click(*composer->save_button());
			require(wait_for([&] { return idle(Page) && !Page.suggesting() && Page.projection().Notes.size() == before + 1U; }), "the suggestion is saved as a note");
			auto object                     = state(Window, Page);
			object[QStringLiteral("added")] = item_name(Page.projection().Notes.back().Item);
			return object;
		}

		/// Links a selection in another file to a note, from the notes list.
		auto relate(const QJsonObject& aCommand) -> QJsonObject
		{
			open_file(aCommand[QStringLiteral("path")].toString());
			select(static_cast<std::uint64_t>(aCommand[QStringLiteral("start")].toInteger()), static_cast<std::uint64_t>(aCommand[QStringLiteral("end")].toInteger()));
			const auto row = row_of(Page, aCommand[QStringLiteral("item")].toString());
			Page.notes()->setCurrentRow(row);
			QApplication::processEvents();
			const auto before = Page.projection().Notes[static_cast<std::size_t>(row)].Related.size();
			click(*Page.relate_button());
			require(wait_for([&] { return idle(Page) && Page.projection().Notes[static_cast<std::size_t>(row)].Related.size() == before + 1U; }), "the location joins the note");
			return state(Window, Page);
		}

		auto include(const QString& aItem, const bool aOn) -> QJsonObject
		{
			auto* const sheet = Page.share_page();
			// Rows are rebuilt as the draft changes: each look finds the note again.
			const auto checkbox = [&]() -> QCheckBox* {
				for (const auto& row : sheet->rows()) {
					if (text_of(core::cycle_wire_name(row.Item)) == aItem) {
						return row.Include;
					}
				}
				return nullptr;
			};
			const auto selected = [&] {
				return std::ranges::any_of(Page.projection().Share->Selection.Items, [&](const core::cycle_round_item_selection& aSelected) { return text_of(core::cycle_wire_name(aSelected.Item)) == aItem; });
			};
			auto* const box = checkbox();
			require(box != nullptr, "the sheet lists " + utf8(aItem));
			if (box->isChecked() != aOn) {
				sheet->notes_area()->ensureWidgetVisible(box);
				QApplication::processEvents();
				click(*box);
			}
			require(wait_for([&] { return idle(Page) && !Page.share_page()->isHidden() && checkbox() != nullptr && checkbox()->isChecked() == aOn && selected() == aOn; }), "the note's check box changes the draft");
			return share_state(Page);
		}

		/// The goal typed into its field; with `aEnter` false it is left typed and
		/// not entered; with `aReplace` it replaces the field's text.
		auto goal(const QString& aText, const bool aEnter, const bool aReplace) -> QJsonObject
		{
			auto* const field = Page.share_page()->goal();
			click(*field);
			if (aReplace) {
				// The key reaches the field once its window has the keyboard and the click gave it focus.
				reactivate(*field->window());
				require(wait_for([&] { return field->hasFocus(); }, 5000), "the click puts the keyboard in the goal; focus is on " + focus_name());
				// End, then one Backspace per character: the first edit marks the field
				// modified, so a sheet presented again meanwhile keeps what is typed (a
				// selection alone is not an edit and does not survive that).
				tests::window_system_key(*field, Qt::Key_End);
				for (auto left = field->text().size(); left > 0; --left) {
					tests::window_system_key(*field, Qt::Key_Backspace);
				}
				require(wait_for([&] { return field->text().isEmpty(); }, 5000), "the goal is emptied, one Backspace at a time");
			}
			type(*field, aText);
			if (!aEnter) {
				require(field->isModified(), "the goal field holds typed text");
				return share_state(Page);
			}
			// Tab leaves the field, which saves it; Return would press the default route.
			tests::window_system_key(*field, Qt::Key_Tab);
			require(wait_for([&] { return idle(Page) && text_of(Page.projection().Share->Selection.Goal) == aText; }), "the goal is saved to the draft");
			return share_state(Page);
		}

		auto share(const QJsonObject& aCommand) -> QJsonObject
		{
			auto* const sheet  = Page.share_page();
			const auto  route  = aCommand[QStringLiteral("route")].toString();
			const auto  before = Page.projection().Share->Deliveries.size();
			auto        chosen = core::cycle_share_route::copy_document;
			auto        dialog = false;
			if (route == QStringLiteral("save")) {
				chosen = core::cycle_share_route::save_document;
				dialog = true;
			} else if (route == QStringLiteral("bundle")) {
				chosen = core::cycle_share_route::save_bundle;
				dialog = true;
			}
			auto answered = false;
			if (dialog) {
				answer_next_file_dialog(aCommand[QStringLiteral("path")].toString(), answered);
			}
			click(*sheet->route_button(chosen));
			require(!dialog || answered, "the save dialog asked for a file");
			if (dialog) {
				reactivate(*sheet);
			}
			require(wait_for([&] { return idle(Page) && Page.projection().Share && Page.projection().Share->Frozen && Page.projection().Share->Deliveries.size() > before; }), "the round is frozen and the share observed");
			auto object = share_state(Page);
			if (chosen == core::cycle_share_route::copy_document) {
				// What the platform clipboard now holds, byte for byte, for the course to compare.
				const auto path = std::filesystem::path{ aCommand[QStringLiteral("evidence")].toString().toStdWString() };
				std::ofstream{ path, std::ios::binary } << utf8(QGuiApplication::clipboard()->text());
				object[QStringLiteral("clipboard_written")] = true;
			}
			return object;
		}

		auto view(const QString& aPath, const QStringList& aMenu) -> QJsonObject
		{
			open_file(aPath);
			auto* const entry = menu_entry(*Page.view_button()->menu(), aMenu);
			require(entry != nullptr && entry->isEnabled(), "the view chooser offers " + utf8(aMenu.join(QStringLiteral(" > "))));
			entry->trigger();
			// The view asked for, by its kind and its round or pair, not the one the tab already showed.
			const auto asked = [&](const core::cycle_view_request& aRequest) {
				if (aMenu.front() == QStringLiteral("This round")) {
					const auto* const round = std::get_if<core::this_round_request>(&aRequest);
					return round != nullptr && aMenu.size() > 1 && aMenu[1].startsWith(text_of(core::cycle_wire_name(round->Round)) + QStringLiteral(" "));
				}
				if (aMenu.front() == QStringLiteral("All cycle changes")) {
					return std::holds_alternative<core::all_cycle_request>(aRequest);
				}
				const auto* const pair = std::get_if<core::chosen_pair_request>(&aRequest);
				return pair != nullptr && aMenu.size() > 2 && aMenu[1] == QStringLiteral("From ") + text_of(core::cycle_wire_name(pair->Left))
					&& aMenu[2] == QStringLiteral("to ") + text_of(core::cycle_wire_name(pair->Right));
			};
			require(wait_for([&] {
						const auto& shown = Page.projection().Comparison;
						return idle(Page) && shown.has_value() && text_of(shown->Path) == aPath && asked(shown->Request);
					}),
					"the comparison of " + utf8(aPath) + " is shown as " + utf8(aMenu.join(QStringLiteral(" > "))));
			return comparison_state(Page);
		}

		/// \brief The window as the reviewer sees it, the share sheet drawn where
		/// it stands over it, saved as a PNG for the record.
		auto render(const QString& aPath) -> QJsonObject
		{
			auto image   = Window.grab().toImage();
			auto painter = QPainter{ &image };
			if (auto* const sheet = Page.share_page(); sheet->isVisible()) {
				painter.drawImage(sheet->mapToGlobal(QPoint{ }) - Window.mapToGlobal(QPoint{ }), sheet->grab().toImage());
			}
			painter.end();
			require(!image.isNull() && image.save(aPath), "the window renders to " + utf8(aPath));
			return QJsonObject{ { QStringLiteral("width"), image.width() }, { QStringLiteral("height"), image.height() } };
		}

		/// The file's full source at the latest capture: Full source, then Refresh.
		auto latest(const QString& aPath) -> QJsonObject
		{
			open_file(aPath);
			if (Page.projection().Comparison) {
				auto* const entry = menu_entry(*Page.view_button()->menu(), { QStringLiteral("Full source") });
				require(entry != nullptr && entry->isEnabled(), "the view chooser offers Full source");
				entry->trigger();
				require(wait_for([&] { return idle(Page) && !Page.projection().Comparison && Page.source()->isVisible(); }), "full source is shown");
			}
			if (Page.projection().Source && Page.projection().Source->Revision != Page.projection().Revisions.back()) {
				require(Page.refresh_action()->isVisible(), "Refresh is offered for newer material");
				click(*Page.review_bar()->widgetForAction(Page.refresh_action()));
				require(wait_for([&] { return idle(Page) && Page.projection().Source && Page.projection().Source->Revision == Page.projection().Revisions.back(); }), "Refresh moves full source to the latest capture");
			}
			return state(Window, Page);
		}

		auto decide(const QString& aItem, const QString& aChoice) -> QJsonObject
		{
			const auto row  = row_of(Page, aItem);
			auto*      menu = Page.decision_menu(row);
			require(menu != nullptr, "the note's decision menu");
			QAction* found = nullptr;
			for (auto* const action : menu->actions()) {
				if (action->text().startsWith(aChoice)) {
					found = action;
				}
			}
			require(found != nullptr, "the decision menu offers " + utf8(aChoice));
			const auto offered = found->text();
			found->trigger();
			delete menu;
			require(wait_for([&] { return idle(Page); }), "the decision is recorded");
			auto object                       = state(Window, Page);
			object[QStringLiteral("offered")] = offered;
			return object;
		}

		/// A reply the reviewer was handed in prose, entered by hand against a round.
		auto reply(const QJsonObject& aCommand) -> QJsonObject
		{
			Page.notes()->setCurrentRow(row_of(Page, aCommand[QStringLiteral("item")].toString()));
			QApplication::processEvents();
			auto* const entry = menu_entry(*Page.reply_button()->menu(), { aCommand[QStringLiteral("entry")].toString() });
			require(entry != nullptr && entry->isEnabled(), "the reply menu offers " + utf8(aCommand[QStringLiteral("entry")].toString()));
			entry->trigger();
			require(Page.reply_editor()->isVisible(), "the reply page opens");
			click(*Page.reply_from());
			type(*Page.reply_from(), aCommand[QStringLiteral("from")].toString());
			click(*Page.reply_editor());
			type(*Page.reply_editor(), aCommand[QStringLiteral("text")].toString());
			const auto before = Page.projection().Responses.size();
			click(*Page.findChild<QPushButton*>(QStringLiteral("cycleReplySave")));
			require(wait_for([&] { return idle(Page) && Page.projection().Responses.size() == before + 1U; }), "the reply is recorded");
			return state(Window, Page);
		}

		/// [[BPR-1184]]. This process's memory now and its lifetime peak, by the platform's own counters.
		static auto memory() -> QJsonObject
		{
			const auto sample = e2e::large_review::native_process_memory_sample();
			return QJsonObject{
				{ QStringLiteral("resident_bytes"), static_cast<qint64>(sample.ResidentBytes) },
				{ QStringLiteral("footprint_bytes"), static_cast<qint64>(sample.FootprintBytes) },
				{ QStringLiteral("peak_footprint_bytes"), static_cast<qint64>(sample.LifetimePeakFootprintBytes) },
				{ QStringLiteral("method"), QString::fromUtf8(e2e::large_review::native_memory_method()) },
			};
		}

		auto shown_path() const -> QString
		{
			const auto& shown = Page.projection();
			return shown.Source ? text_of(shown.Source->Path) : shown.Comparison ? text_of(shown.Comparison->Path)
																				 : QString{ };
		}

		/// [[BPR-1184]]. The review bar's Next file, clicked, until the next file is read.
		auto next_file() -> QJsonObject
		{
			auto* const action = Page.findChild<QAction*>(QStringLiteral("cycleNextFile"));
			require(action != nullptr && action->isEnabled(), "Next file is offered");
			const auto before = shown_path();
			click(*Page.review_bar()->widgetForAction(action));
			require(wait_for([&] { return settled(Page) && !shown_path().isEmpty() && shown_path() != before; }), "Next file reads the next file after " + utf8(before));
			auto object                     = state(Window, Page);
			object[QStringLiteral("shown")] = shown_path();
			return object;
		}

		/// Clicks Capture revision and waits until the page says it is capturing.
		void start_capture()
		{
			click(*Page.review_bar()->widgetForAction(Page.capture_action()));
			require(wait_for([&] { return capturing(Page); }, 5000), "Capture revision starts a capture");
		}

		/// Until the capture ends: a new revision, or the page says nothing changed.
		void finish_capture(const std::size_t aBefore)
		{
			require(wait_for([&] { return idle(Page) && (Page.projection().Revisions.size() > aBefore || Page.updates_line()->text().contains(QStringLiteral("No change"))); }, 600000),
					"the capture is recorded or said to be unchanged");
		}

		/// \brief [[BPR-1184]]. A note typed key by key while a capture runs.
		///
		/// \details Return on a selection opens the note; Capture revision is
		/// clicked and then the note, to type in it again; then one key is due
		/// every `interval_ms`, cycling through `text`, until the capture ends
		/// or `max_keys` are typed. A key's latency is from when it was due to
		/// when the window had handled it: time spent waiting behind other work
		/// on the GUI thread plus its own handling. The note must hold exactly
		/// the keys typed, and is then cancelled.
		auto type_during_capture(const QJsonObject& aCommand) -> QJsonObject
		{
			open_file(aCommand[QStringLiteral("path")].toString());
			select(static_cast<std::uint64_t>(aCommand[QStringLiteral("start")].toInteger()), static_cast<std::uint64_t>(aCommand[QStringLiteral("end")].toInteger()));
			reactivate(Window);
			tests::window_system_key(*Page.source(), Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
			auto* const editor = Page.comment_editor();
			require(editor->isVisible() && QApplication::focusWidget() == editor, "Return opens a note on the selection; focus is on " + focus_name());
			const auto    before = Page.projection().Revisions.size();
			QElapsedTimer capture;
			capture.start();
			start_capture();
			// The button takes the keyboard (every Tab stop does); the reviewer clicks back into the note.
			click(*editor);
			require(QApplication::focusWidget() == editor, "a click in the note gives it the keyboard; focus is on " + focus_name());
			const auto    text     = aCommand[QStringLiteral("text")].toString();
			const auto    interval = aCommand[QStringLiteral("interval_ms")].toInteger(40) * 1000000;
			const auto    limit    = aCommand[QStringLiteral("max_keys")].toInteger(2000);
			auto          typed    = QString{ };
			auto          during   = QJsonArray{ };
			auto          after    = QJsonArray{ };
			QElapsedTimer clock;
			clock.start();
			for (qint64 key = 0; key < limit && capturing(Page) && !text.isEmpty(); ++key) {
				const auto due = key * interval;
				while (clock.nsecsElapsed() < due) {
					turn();
					QThread::usleep(200);
				}
				const auto character = text[static_cast<qsizetype>(key % text.size())];
				const auto running   = capturing(Page);
				const auto upper     = character.toUpper().unicode();
				tests::window_system_key(*editor, character == QLatin1Char(' ') ? int{ Qt::Key_Space } : int{ upper }, character.isUpper() ? Qt::ShiftModifier : Qt::NoModifier, QString{ character });
				typed.append(character);
				(running ? during : after).append(static_cast<double>(clock.nsecsElapsed() - due) / 1e6);
			}
			finish_capture(before);
			const auto captured = static_cast<double>(capture.nsecsElapsed()) / 1e6;
			require(wait_for([&] { return editor->toPlainText() == typed; }, 5000), "the note holds exactly the " + std::to_string(typed.size()) + " keys typed");
			auto* const cancel = Page.findChild<QPushButton*>(QStringLiteral("cycleCommentCancel"));
			click(*cancel);
			require(wait_for([&] { return !editor->isVisible(); }), "Cancel closes the note");
			auto object                          = state(Window, Page);
			object[QStringLiteral("during")]     = during;
			object[QStringLiteral("after")]      = after;
			object[QStringLiteral("typed")]      = static_cast<qint64>(typed.size());
			object[QStringLiteral("capture_ms")] = captured;
			return object;
		}

		/// \brief [[BPR-1184]]. Next file, clicked once every `interval_ms`
		/// while a capture runs, `count` times or until the capture ends. Each
		/// step's latency is from the click to the next file being read.
		auto navigate_during_capture(const QJsonObject& aCommand) -> QJsonObject
		{
			open_file(aCommand[QStringLiteral("path")].toString());
			auto* const   action = Page.findChild<QAction*>(QStringLiteral("cycleNextFile"));
			const auto    before = Page.projection().Revisions.size();
			QElapsedTimer capture;
			capture.start();
			start_capture();
			const auto interval = aCommand[QStringLiteral("interval_ms")].toInteger(250);
			const auto count    = aCommand[QStringLiteral("count")].toInteger(20);
			auto       during   = QJsonArray{ };
			auto       after    = QJsonArray{ };
			auto       refused  = QJsonArray{ };
			for (qint64 step = 0; step < count && capturing(Page); ++step) {
				QElapsedTimer pause;
				pause.start();
				while (pause.elapsed() < interval) {
					turn();
					QThread::msleep(1);
				}
				const auto running = capturing(Page);
				const auto from    = shown_path();
				if (!action->isEnabled()) {
					refused.append(from);
					continue;
				}
				QElapsedTimer step_clock;
				step_clock.start();
				click(*Page.review_bar()->widgetForAction(action));
				require(wait_for([&] { return !shown_path().isEmpty() && shown_path() != from && Page.projection().Source.has_value(); }, 600000), "Next file reads the file after " + utf8(from));
				(running ? during : after).append(static_cast<double>(step_clock.nsecsElapsed()) / 1e6);
			}
			finish_capture(before);
			auto object                          = state(Window, Page);
			object[QStringLiteral("during")]     = during;
			object[QStringLiteral("after")]      = after;
			object[QStringLiteral("disabled")]   = refused;
			object[QStringLiteral("capture_ms")] = static_cast<double>(capture.nsecsElapsed()) / 1e6;
			return object;
		}

		/// \brief [[BPR-1184]]. Capture revision, then Cancel capture in its
		/// place `after_ms` later: how long the cancel took to take effect,
		/// what the page then says, and that nothing was recorded.
		auto cancel_capture(const QJsonObject& aCommand) -> QJsonObject
		{
			const auto before = Page.projection().Revisions;
			start_capture();
			auto* const   button = Page.review_bar()->widgetForAction(Page.capture_action());
			QElapsedTimer pause;
			pause.start();
			while (pause.elapsed() < aCommand[QStringLiteral("after_ms")].toInteger(1000)) {
				turn();
				QThread::msleep(1);
			}
			require(capturing(Page), "the capture is still running when Cancel capture is clicked");
			const auto    offered = Page.capture_action()->text();
			const auto    line    = Page.updates_line()->text();
			QElapsedTimer cancelled;
			cancelled.start();
			click(*button);
			require(wait_for([&] { return !capturing(Page) && settled(Page); }, 600000), "Cancel capture stops the capture");
			auto object                            = state(Window, Page);
			object[QStringLiteral("cancel_ms")]    = static_cast<double>(cancelled.nsecsElapsed()) / 1e6;
			object[QStringLiteral("offered")]      = offered;
			object[QStringLiteral("progress")]     = line;
			object[QStringLiteral("unrecorded")]   = Page.projection().Revisions == before;
			object[QStringLiteral("button_after")] = Page.capture_action()->text();
			return object;
		}

		/// [[BPR-1184]]. Every row the files drawer shows, and the Updates line.
		auto drawer() -> QJsonObject
		{
			auto rows = QJsonArray{ };
			for (auto row = 0; row < Page.files()->count(); ++row) {
				rows.append(Page.files()->item(row)->text());
			}
			auto object                     = state(Window, Page);
			object[QStringLiteral("rows")]  = rows;
			object[QStringLiteral("first")] = static_cast<qint64>(Page.projection().FilesFirst);
			return object;
		}

		/// [[BPR-1184]]. The drawer's Next files (1) or Previous files (-1)
		/// row, double clicked, until the drawer lists that window.
		auto drawer_page(const int aDirection) -> QJsonObject
		{
			auto* const files  = Page.files();
			const auto  before = Page.projection().FilesFirst;
			auto* const row    = files->item(aDirection > 0 ? files->count() - 1 : 0);
			require(row != nullptr && row->data(Qt::UserRole + 1).toInt() == aDirection, "the drawer offers " + std::string{ aDirection > 0 ? "Next files" : "Previous files" });
			activate(*files, *row);
			require(wait_for([&] { return settled(Page) && Page.projection().FilesFirst != before; }), "the drawer lists the " + std::string{ aDirection > 0 ? "next" : "previous" } + " files");
			return drawer();
		}

		qt::workbench_window&  Window;
		qt::review_cycle_view& Page;
	};
}

auto main(int aArgc, char** aArgv) -> int
{
	// The application's own Qt file dialogs: a native panel cannot be answered by a course.
	QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	if (aArgc != 4) {
		std::cerr << "usage: root profile signal\n";
		return EXIT_FAILURE;
	}
	// [[BPR-1184]]. A large project's course scales every wait and the course clock.
	if (const auto scale = qEnvironmentVariableIntValue("BPR_CYCLE_COURSE_WAIT_SCALE"); scale > 1) {
		wait_scale() = scale;
	}
	if (const auto budget = qEnvironmentVariableIntValue("BPR_CYCLE_COURSE_GESTURE_BUDGET_MS"); budget > 0) {
		gesture_budget_ms() = budget;
	}
	const auto root             = std::filesystem::path{ aArgv[1] };
	const auto profile          = core::bpr_profile_id{ std::stoull(aArgv[2]) };
	const auto signal           = std::filesystem::path{ aArgv[3] };
	auto       options          = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot = root;
	options.Profile             = profile;
	auto started                = false;
	auto startedOk              = false;
	auto desktop                = access::start_desktop(app::detail::github_public_path_fixture::profile_window_restoration_v1, options, [&](auto aResult) {
		started   = true;
		startedOk = static_cast<bool>(aResult);
	});
	if (!wait_for([&] { return started; }) || !startedOk || !wait_for([&] { return !desktop->windows(profile).empty(); })) {
		std::cerr << "FAILED: the desktop starts with a window\n";
		return EXIT_FAILURE;
	}
	auto& window = *desktop->window(desktop->windows(profile).front());
	auto* page   = window.local_cycle();
	if (page == nullptr) {
		std::cerr << "FAILED: the window composes the local cycle page\n";
		return EXIT_FAILURE;
	}
	window.raise();
	reactivate(window);
	const auto endpoint = adapters::sqlite::tour_owner_endpoint(root);
	if (!endpoint || !wait_for([&] { return endpoint_bound(*endpoint); })) {
		std::cerr << "FAILED: the owner's command endpoint is bound\n";
		return EXIT_FAILURE;
	}
	// The clipboard as it was before the course, put back on the way out.
	const auto kept = QGuiApplication::clipboard()->text();
	say("DESKTOP_READY " + std::string{ QGuiApplication::platformName().toUtf8().constData() } + " qt=" + qVersion() + " active=" + (window.isActiveWindow() ? "1" : "0") + " size=" + std::to_string(window.width()) + "x"
		+ std::to_string(window.height()) + " screen=" + std::to_string(window.screen()->availableGeometry().width()) + "x" + std::to_string(window.screen()->availableGeometry().height()));
	auto          gestures = reviewer{ window, *page };
	QElapsedTimer course;
	course.start();
	const auto    command = std::filesystem::path{ signal.string() + ".command" };
	QElapsedTimer unreadable;
	while (!std::filesystem::exists(signal) && course.elapsed() < qint64{ 540000 } * wait_scale()) {
		const auto text     = std::filesystem::exists(command) ? read_file(command) : std::string{ };
		const auto document = QJsonDocument::fromJson(QByteArray::fromStdString(text));
		if (std::filesystem::exists(command) && !document.isObject()) {
			// [[BPR-1184]]. On Windows a command renamed into place a moment ago
			// can refuse the open while a scan holds it, and the read comes back
			// empty. It stays where it is and is read again, within a bound.
			if (!unreadable.isValid()) {
				unreadable.start();
			} else if (unreadable.elapsed() > 2000) {
				std::cerr << "FAILED: the course's command stays unreadable: [" << text << "]\n";
				return EXIT_FAILURE;
			}
		} else if (document.isObject()) {
			unreadable.invalidate();
			std::filesystem::remove(command);
			const auto request = document.object();
			const auto id      = request[QStringLiteral("id")].toInteger();
			auto       reply   = QJsonObject{ };
			// [[BPR-1184]]. How long the gesture took, measured here rather
			// than across the command file, and the GUI thread's longest turn.
			longest_turn_ns() = 0;
			QElapsedTimer gesture;
			gesture.start();
			gesture_clock().start();
			try {
				reply = gestures.run(request);
			} catch (const refused& aRefused) {
				// What the window shows beside the refusal, so a failure names its state.
				reply = QJsonObject{ { QStringLiteral("error"), text_of(aRefused.What) }, { QStringLiteral("state"), state(window, *page) } };
			}
			gesture_clock().invalidate();
			reply[QStringLiteral("elapsed_ms")] = static_cast<double>(gesture.nsecsElapsed()) / 1e6;
			reply[QStringLiteral("busy_ms")]    = static_cast<double>(longest_turn_ns()) / 1e6;
			say("REPLY " + std::to_string(id) + " " + json_line(reply));
		}
		QApplication::processEvents();
		QThread::msleep(1);
	}
	QGuiApplication::clipboard()->setText(kept);
	desktop->quit();
	if (!wait_for([&] { return desktop->is_quiescent(); }) || desktop->exit_code() != 0) {
		std::cerr << "FAILED: orderly drain\n";
		return EXIT_FAILURE;
	}
	say("DESKTOP_RELEASED");
	return EXIT_SUCCESS;
}
