// [[BPR-1357]]. Opening a review from Home on a slow network, through the
// composed application. The owner, 2026-10-09: "There's no obvious immediate
// indication you actually tried to open a review if there is real network
// latency on the action." The fixture transport holds a pull request's first
// answers for several seconds. A single click only selects a row ([[BPR-1364]]);
// in the same turn as the double-click or Return the row says it is opening; after the core's wait it names the step and offers Cancel; Cancel
// stops the read on the wire; a failure says why on the row and Retry asks
// again; the Home bar's opening is said in Home's status band, which also
// carries Home's general status. Nothing reaches a live provider.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "../support/window_system_mouse.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/home_status_line.hpp"
#include "bpr/qt/pending_veil.hpp"
#include "bpr/qt/review_landing_model.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAccessible>
#include <QAccessibleEvent>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMouseEvent>
#include <QPoint>
#include <QPushButton>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QThread>
#include <QToolButton>

#include <array>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {
	using namespace bpr;
	using bpr::tests::expect;

	void wait_for(const std::function<bool()>& aCondition, const char* aMessage, const std::function<void()>& aDiagnostic = { }, const int aTimeoutMs = 20000)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < aTimeoutMs) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		if (!aCondition() && aDiagnostic) {
			aDiagnostic();
		}
		expect(aCondition(), aMessage);
	}

	/// Lets the event loop run for a while, as a person waiting would.
	void pause(const int aMilliseconds)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (elapsed.elapsed() < aMilliseconds) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
	}

	constexpr auto viewer   = "octo-reviewer";
	constexpr auto served   = 17U;
	constexpr auto failing  = 4001U;
	constexpr auto delay_ms = 3000U;
	/// [[BPR-1679]]. How long the slow network holds each of Home's searches.
	constexpr auto discovery_hold_ms = 10'000U;
	constexpr auto served_name       = "octo-org/review-repository#17";
	constexpr auto failing_name      = "octo-org/review-repository#4001";

	std::vector<QString>* Announcements = nullptr;

	void capture(QAccessibleEvent* const aEvent)
	{
		if (Announcements != nullptr && aEvent != nullptr && aEvent->type() == QAccessible::Announcement) {
			const auto* const announcement = static_cast<const QAccessibleAnnouncementEvent*>(aEvent);
			if (announcement->politeness() == QAccessible::AnnouncementPoliteness::Polite) {
				Announcements->push_back(announcement->message());
			}
		}
	}

	/// The Home row with this identity, in whichever state's list holds it.
	struct found_row
	{
		QListView*  View = nullptr;
		QModelIndex Index;
	};

	[[nodiscard]] auto row_of(qt::workbench_window& aWindow, const char* aIdentity) -> found_row
	{
		for (const auto group : { qt::home_group::needs_you, qt::home_group::waiting_on_others, qt::home_group::following }) {
			auto* const view = aWindow.home_list()->state_view(group);
			for (auto row = 0; view != nullptr && view->model() != nullptr && row < view->model()->rowCount(); ++row) {
				const auto index = view->model()->index(row, 0);
				if (index.data(qt::home_row_role::identity).toString() == QString::fromUtf8(aIdentity)) {
					return { view, index };
				}
			}
		}
		return { };
	}

	/// A real click on a row: press and release on its viewport, as a pointer
	/// makes it, with no event loop turn in between or after. [[BPR-1364]]:
	/// with `aDouble`, the double-click a hand makes (press, release, the
	/// double-click, release), which is what opens a row.
	void click(const found_row& aRow, const bool aDouble = false)
	{
		const auto point   = QPointF{ aRow.View->visualRect(aRow.Index).center() };
		const auto global  = aRow.View->viewport()->mapToGlobal(point);
		auto       press   = QMouseEvent{ QEvent::MouseButtonPress, point, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		auto       release = QMouseEvent{ QEvent::MouseButtonRelease, point, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		auto       doubled = QMouseEvent{ QEvent::MouseButtonDblClick, point, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(aRow.View->viewport(), &press);
		QApplication::sendEvent(aRow.View->viewport(), &release);
		if (aDouble) {
			QApplication::sendEvent(aRow.View->viewport(), &doubled);
			QApplication::sendEvent(aRow.View->viewport(), &release);
		}
	}

	void double_click(const found_row& aRow)
	{
		click(aRow, true);
	}

	[[nodiscard]] auto row_lines(qt::workbench_window& aWindow, const found_row& aRow) -> QString
	{
		auto option = QStyleOptionViewItem{ };
		option.initFrom(aRow.View);
		option.rect   = aRow.View->visualRect(aRow.Index);
		option.font   = aRow.View->font();
		option.widget = aRow.View;
		// A wrapped line keeps its own spaces at the break, so the lines are
		// joined as they were and read as one sentence.
		return aWindow.home_list()->row_delegate()->line_texts(option, aRow.Index).join(QString{ }).simplified();
	}

	/// [[BPR-1365]]. The ground a row is painted with, in the window's render,
	/// left of the row's text where only its ground and any selection are drawn.
	[[nodiscard]] auto ground(QWidget& aWindow, const std::function<found_row()>& aRow) -> QColor
	{
		const auto image = aWindow.grab().toImage();
		const auto ratio = image.devicePixelRatio();
		const auto row   = aRow();
		const auto rect  = row.View->visualRect(row.Index);
		const auto at    = row.View->viewport()->mapTo(&aWindow, QPoint{ rect.left() + 6, rect.center().y() });
		return image.pixelColor(static_cast<int>(at.x() * ratio), static_cast<int>(at.y() * ratio));
	}

	void window_system_click(const std::function<found_row()>& aRow)
	{
		QListView* view = nullptr;
		QPoint     point;
		{
			const auto row = aRow();
			expect(row.View != nullptr && row.Index.isValid(), "the mouse target still names a Home row");
			view  = row.View;
			point = view->visualRect(row.Index).center();
			expect(view->viewport()->visibleRegion().contains(point), "the Home mouse target is inside the visible viewport");
		}
		tests::window_system_clicks(*view->viewport(), QPointF{ point }, 1);
	}

	[[nodiscard]] auto near_colour(const QColor& aActual, const QColor& aExpected, const int aTolerance) -> bool
	{
		return std::abs(aActual.red() - aExpected.red()) <= aTolerance && std::abs(aActual.green() - aExpected.green()) <= aTolerance
			&& std::abs(aActual.blue() - aExpected.blue()) <= aTolerance;
	}

	[[nodiscard]] auto phase(qt::workbench_window& aWindow) -> std::optional<core::home_open_phase>
	{
		const auto& open = aWindow.home_open();
		return open.has_value() && open->State.has_value() ? std::optional{ open->State->Phase } : std::nullopt;
	}
} // namespace

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	const auto    renderArgument  = QApplication::arguments().indexOf(QStringLiteral("--render-directory"));
	const auto    renderDirectory = renderArgument >= 0 ? QApplication::arguments().value(renderArgument + 1) : QString{ };
	QTemporaryDir temporary;
	expect(temporary.isValid(), "private home-open course root");
	const auto root = e2e::private_data_root(temporary).value_or(std::filesystem::path{ });
	expect(!root.empty(), "private desktop root under the canonical storage policy");
	auto delay                     = std::make_shared<app::detail::github_read_delay_fixture>();
	auto options                   = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot    = root;
	options.RegisteredDeviceCourse = true;
	options.IndependentWindowReads = true;
	options.RepeatReadSessions     = true;
	options.SharedDeviceVault      = app::detail::make_shared_github_fixture_vault();
	options.ReadDelay              = delay;
	options.PersonalDiscovery      = app::detail::github_personal_discovery_fixture{
		.OpenPullRequests = {
			{ .Number = served, .Author = "contributor-a", .RequestedUsers = { viewer } },
			{ .Number = failing, .Author = viewer, .RequestedUsers = { "alice" } },
		},
	};

	app::github_application* owner    = nullptr;
	auto                     terminal = false;
	auto                     desktop  = app::detail::github_application_fixture_access::start_desktop(
		app::detail::github_public_path_fixture::profile_window_restoration_v1, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				owner = *aResult;
			}
		});
	wait_for([&] { return terminal; }, "profile owner startup completes");
	expect(owner != nullptr, "profile owner admitted");
	owner->show();
	wait_for([&] { return owner->windows().size() == 1U; }, "the window is admitted");
	auto* const window = owner->window(owner->windows().front());
	window->resize(1100, 760);

	// Signed in, with the repository saved, so Home lists both pull requests.
	window->home_landing()->primary_button()->click();
	wait_for([&] { return window->session_panel()->status() == core::github_status::session_ready; }, "Home sign-in reaches an account session");
	window->open_repository_addition();
	wait_for([&] { return window->repository_addition_draft()->isVisible() && window->saved_repository_name()->isEnabled(); }, "Settings opens the new-repository card");
	window->saved_repository_name()->setText(QStringLiteral("octo-org/review-repository"));
	window->repository_addition_draft()->findChild<QPushButton*>(QStringLiteral("repositoryAdditionAdd"))->click();
	wait_for([&] { return !window->repository_addition_draft()->isVisible() && row_of(*window, served_name).View != nullptr && row_of(*window, failing_name).View != nullptr; },
			 "the saved repository's two pull requests are on Home");
	window->settings()->hide();
	window->home_action()->trigger();
	QApplication::processEvents();
	std::cerr << "after saving: band=" << !window->home_status_line()->isHidden() << " status=" << window->home_status_text()->text().toStdString()
			  << " notice=" << window->home_open_notice()->text().toStdString() << " bar=" << window->statusBar()->currentMessage().toStdString() << '\n';
	// Saving said what it did, in the status bar and so in the band, for as
	// long as the status bar says it.
	expect(window->statusBar()->currentMessage().isEmpty() || window->home_status_text()->text() == window->statusBar()->currentMessage(),
		   "what was just done is said in the band while the status bar says it");
	window->statusBar()->clearMessage();
	QApplication::processEvents();
	expect(window->showing_home() && window->home_status_line()->isHidden(), "a quiet Home shows no status band");
	expect(!window->home_open().has_value(), "nothing is being opened");

	std::vector<QString> announcements;
	Announcements       = &announcements;
	const auto previous = QAccessible::installUpdateHandler(capture);

	const auto renders = [&](const QString& aName) {
		if (renderDirectory.isEmpty()) {
			return;
		}
		expect(QDir{ }.mkpath(renderDirectory), "render directory exists");
		const auto mode = owner->status().ProfileMode;
		expect(window->grab().save(renderDirectory + QStringLiteral("/%1-%2.png").arg(QString::fromUtf8(core::stable_name(mode)), aName)), "render saved");
	};
	const auto modes = std::array{ core::appearance_mode::light, core::appearance_mode::dark };

	// [[BPR-1365]]. The owner, accepting BPR-1364: "Separate problem: the color
	// of recent is the same as the color of selected." Home over the open #17:
	// the composition marks Recently viewed's #17 as the open review, the
	// queue's #17 says it too, neither is drawn with the selection's fill, and
	// Home shows one selection, in the list that last took the keyboard.
	const auto reviewingCourse = [&](const QString& aName) {
		auto* const recent    = window->home_recent_list();
		const auto  servedQ   = QString::fromUtf8(served_name);
		const auto  recentRow = [&] {
			for (auto row = 0; row < recent->count(); ++row) {
				if (recent->item(row)->data(qt::home_row_role::identity).toString() == servedQ) {
					return row;
				}
			}
			return -1;
		};
		wait_for([&] { return recentRow() >= 0 && recent->item(recentRow())->data(qt::home_row_role::reviewing).toBool(); },
				 "Recently viewed says which of its entries is the open review");
		for (auto row = 0; row < recent->count(); ++row) {
			expect(recent->item(row)->data(qt::home_row_role::reviewing).toBool() == (row == recentRow()), "only the open review is marked in Recently viewed");
		}
		// The owner's capture: the queue's #17 selected beside the open #17. A
		// click is made in the active window, as a hand's is, on the rows Home
		// shows once it has settled.
		window->activateWindow();
		wait_for([&] { return window->isActiveWindow(); }, "the window is active");
		for (auto turn = 0; turn < 20; ++turn) {
			QApplication::processEvents();
		}
		window->home_list()->ensureWidgetVisible(recent);
		QApplication::processEvents();
		// A click or render can publish a refreshed queue projection. Its proxy
		// mappings may then be replaced, so retain the identity, never an index.
		const auto queued        = [&] { return row_of(*window, served_name); };
		const auto recentEntry   = [&] { return found_row{ recent, recent->model()->index(recentRow(), 0) }; };
		const auto queueSelected = [&] {
			const auto row = queued();
			return row.View->selectionModel()->isSelected(row.Index);
		};
		{
			const auto current = queued();
			const auto failed  = row_of(*window, failing_name);
			expect(current.View != nullptr && current.View->isVisible() && failed.View != nullptr, "both pull requests are on Home");
			expect(current.Index.data(qt::home_row_role::reviewing).toBool() && !failed.Index.data(qt::home_row_role::reviewing).toBool(),
				   "the queue's row of the open pull request says so, and no other");
			expect(current.Index.data(Qt::AccessibleDescriptionRole).toString().startsWith(qt::home_reviewing_description()),
				   "and says it to assistive technology");
		}
		window_system_click(queued);
		expect(!phase(*window).has_value() && window->showing_home(), "the click only selects");
		expect(queueSelected() && recent->selectedItems().isEmpty(),
			   "the queue's row is Home's one selection; Recently viewed selects nothing");
		const auto highlight       = recent->palette().color(QPalette::Active, QPalette::Highlight);
		const auto queuedHighlight = queued().View->palette().color(QPalette::Active, QPalette::Highlight);
		expect(near_colour(ground(*window, queued), queuedHighlight, 24),
			   "the selected row keeps the native selection colour");
		expect(!near_colour(ground(*window, recentEntry), highlight, 24), "the open review is not painted in the selection colour");
		expect(near_colour(ground(*window, recentEntry), recent->palette().color(QPalette::Active, QPalette::AlternateBase), 6),
			   "the open review sits on the theme's quiet tint");
		renders(aName);

		// The selection follows the keyboard to Recently viewed and back.
		window_system_click(recentEntry);
		expect(!phase(*window).has_value() && recent->item(recentRow())->isSelected() && !queued().View->selectionModel()->hasSelection(),
			   "a click on Recently viewed moves the one selection there");
		expect(near_colour(ground(*window, recentEntry), highlight, 24), "selected, the open review is the native selection");
		renders(aName + QStringLiteral("-recent-selected"));
		window_system_click(queued);
		expect(queueSelected() && recent->selectedItems().isEmpty(), "and back to the queue");
	};

	delay->Milliseconds = delay_ms;
	delay->FailNumber   = failing;
	for (const auto mode : modes) {
		window->appearance_settings()->choose_mode(mode);
		wait_for([&] { return owner->status().ProfileMode == mode; }, "the rendered appearance is accepted");
		window->settings()->hide();
		// Home is a toggle: with a review open behind it, triggering it while
		// Home shows would leave Home. The course asks for Home itself.
		window->choose_home(true);
		QApplication::processEvents();
		expect(window->showing_home(), "each appearance starts on Home");

		// [[BPR-1381]]. Where Home's list begins: nothing Home says or offers
		// may move it under the pointer.
		const auto listTop = [&] { return window->home_list()->mapTo(window->home_page(), QPoint{ }).y(); };
		const auto barTop  = [&] { return window->home_open_frame()->mapTo(window->home_page(), QPoint{ }).y(); };
		QApplication::processEvents();
		const auto quietList = listTop();
		const auto quietBar  = barTop();
		// [[BPR-1505]]. The public Home row's changes, age and comments
		// stay aligned while its opening action appears and disappears.
		const auto rowFacts = [&](const char* aIdentity = served_name) {
			const auto found  = row_of(*window, aIdentity);
			auto       option = QStyleOptionViewItem{ };
			option.initFrom(found.View);
			option.rect   = found.View->visualRect(found.Index);
			option.font   = found.View->font();
			option.widget = found.View;
			return window->home_list()->row_delegate()->facts_rect(option, found.Index);
		};
		const auto quietFacts = rowFacts();
		expect(!quietFacts.isEmpty(), "the composed Home row has change facts to align");
		const auto factsAligned = [&] { return rowFacts().left() == quietFacts.left() && rowFacts().right() == quietFacts.right(); };
		renders(QStringLiteral("row-idle"));

		// ---- a single click only selects ([[BPR-1364]]) -------------------
		const auto servedRow    = [&] { return row_of(*window, served_name); };
		const auto delayedFirst = delay->Delayed.load();
		click(servedRow());
		QApplication::processEvents();
		expect(!phase(*window).has_value() && window->showing_home() && delay->Delayed.load() == delayedFirst,
			   "a single click on a row asks GitHub for nothing and stays on Home");
		{
			const auto current = servedRow();
			expect(current.View->currentIndex() == current.Index && current.View->selectionModel()->isSelected(current.Index),
				   "a single click makes the row current and highlighted");
		}
		expect(!row_lines(*window, servedRow()).contains(QStringLiteral("Opening")), "a selected row does not say it is opening");

		// ---- the same turn as the double-click -----------------------------
		double_click(servedRow());
		// No event loop turn has run since the release.
		expect(phase(*window) == core::home_open_phase::opening, "in the same turn as the double-click, the core's opening reaches the window");
		expect(window->home_list()->row_delegate()->open().Identity == QString::fromUtf8(served_name)
				   && window->home_list()->row_delegate()->open().Phase == qt::home_row_open_phase::opening,
			   "in the same turn as the double-click, the double-clicked row is the open row");
		expect(row_lines(*window, servedRow()).contains(QStringLiteral("Opening #17…")), "in the same turn as the double-click, the row says Opening #17…");
		expect(window->home_list()->open_view() == servedRow().View, "the double-clicked row's own list carries it");
		expect(window->showing_home() && window->home_status_line()->isHidden(), "the row says it, so the band adds nothing");
		wait_for([&] { return delay->Delayed.load() > delayedFirst; }, "the read reaches the slow transport");
		QApplication::processEvents();
		expect(listTop() == quietList && barTop() == quietBar, "a row opening moves neither the bar nor the list");
		expect(factsAligned(), "opening keeps the Home row's facts aligned");
		renders(QStringLiteral("row-opening"));

		// A second double-click is the same request.
		const auto request = window->home_open()->State->Request;
		double_click(row_of(*window, served_name));
		expect(window->home_open()->State->Request == request, "a second double-click on the row being opened asks for nothing");

		// ---- after the wait: the step, and Cancel --------------------------
		wait_for([&] { return phase(*window) == core::home_open_phase::waiting; }, "after the core's wait the opening is waiting", { }, 3000);
		QApplication::processEvents();
		auto* const action = window->home_list()->open_action();
		expect(action->isVisible() && action->text() == QStringLiteral("Cancel") && action->accessibleName() == QStringLiteral("Cancel opening %1").arg(QString::fromUtf8(served_name)),
			   "the row offers Cancel, named for its pull request");
		expect(factsAligned() && action->geometry().right() < rowFacts().left(), "Cancel is left of the changes and history, which keep their alignment");
		expect(row_lines(*window, row_of(*window, served_name)).contains(QStringLiteral("Opening #17: asking GitHub for the pull request…")),
			   "the row names the step it is on");
		expect(std::ranges::any_of(announcements, [](const QString& aText) { return aText.startsWith(QStringLiteral("Opening octo-org/review-repository#17: ")) && aText.endsWith(QStringLiteral("Cancel is available.")); }),
			   "the step and Cancel are announced politely");
		renders(QStringLiteral("row-waiting"));

		// ---- Cancel ---------------------------------------------------------
		const auto cancelledBefore = delay->Cancelled.load();
		action->click();
		wait_for([&] { return !window->home_open().has_value() && delay->Cancelled.load() > cancelledBefore; }, "Cancel stops the read on the wire and Home is quiet again");
		expect(window->showing_home() && window->session_panel()->status() == core::github_status::pull_request_cancelled, "a cancelled opening stays on Home");
		expect(!action->isVisible() && window->home_list()->row_delegate()->open().Phase == qt::home_row_open_phase::none, "the row is itself again");
		expect(factsAligned(), "cancelling keeps the Home row's facts aligned");
		expect(!row_lines(*window, row_of(*window, served_name)).contains(QStringLiteral("Opening")), "the row says why it is listed again");

		// ---- a failure on its row, and Retry -------------------------------
		const auto failingFacts = rowFacts(failing_name);
		double_click(row_of(*window, failing_name));
		expect(phase(*window) == core::home_open_phase::opening && window->home_list()->open_view() == row_of(*window, failing_name).View, "the failing row is opening at once");
		wait_for([&] { return phase(*window) == core::home_open_phase::failed; }, "the failing read fails after its wait", { }, 15000);
		QApplication::processEvents();
		expect(window->showing_home() && action->isVisible() && action->text() == QStringLiteral("Retry"), "the failed row offers Retry");
		const auto retryFacts = rowFacts(failing_name);
		expect(retryFacts.left() == failingFacts.left() && retryFacts.right() == failingFacts.right() && action->geometry().right() < retryFacts.left(),
			   "Retry is left of the failed row's facts and keeps their alignment");
		std::cerr << "failed row: " << row_lines(*window, row_of(*window, failing_name)).toStdString() << " reason=" << static_cast<int>(window->home_open()->State->Reason) << " delayed=" << delay->Delayed.load() << " failed=" << delay->Failed.load() << " recording=" << app::detail::github_application_fixture_access::evidence(*owner).RecordingFailure << '\n';
		expect(row_lines(*window, row_of(*window, failing_name)).contains(QStringLiteral("Couldn't open #4001. The GitHub host could not be reached.")),
			   "the failed row says why, in its own words");
		expect(window->home_open_notice()->isHidden(), "the row says the failure, so the line under the bar does not");
		renders(QStringLiteral("row-failed"));
		const auto failedRequest = window->home_open()->State->Request;
		action->click();
		expect(phase(*window) == core::home_open_phase::opening && window->home_open()->State->Request != failedRequest,
			   "Retry asks for the same pull request again, in the same turn");
		wait_for([&] { return phase(*window) == core::home_open_phase::failed; }, "the retried read fails again", { }, 15000);

		// ---- the Home bar: said in the status band --------------------------
		window->home_open_bar()->setText(QStringLiteral("https://github.com/octo-org/review-repository/pull/18"));
		window->home_open_action()->trigger();
		expect(phase(*window) == core::home_open_phase::opening, "the bar's opening is decided in the same turn");
		expect(!window->home_status_line()->isHidden() && window->home_status_line()->tone() == qt::home_status_tone::busy
				   && window->home_open_notice()->text() == QStringLiteral("Opening octo-org/review-repository#18…"),
			   "with no row for it, Home's status band says the bar's opening at once");
		QApplication::processEvents();
		auto* const activity = window->home_open_frame()->findChild<QWidget*>(QStringLiteral("homeOpenActivity"));
		expect(activity != nullptr && activity->isVisible(), "the bar says in the bar that what it was given is opening");
		expect(listTop() == quietList && barTop() == quietBar, "the band docked at Home's foot moves neither the bar nor the list");
		expect(window->home_status_line()->mapTo(window->home_page(), QPoint{ }).y() > listTop(), "the band stands under the list");
		wait_for([&] { return phase(*window) == core::home_open_phase::waiting; }, "the bar's opening waits", { }, 3000);
		QApplication::processEvents();
		expect(window->home_open_cancel_button()->isVisible() && window->home_open_notice()->text().startsWith(QStringLiteral("Opening octo-org/review-repository#18: ")),
			   "the band names the step and offers Cancel");
		renders(QStringLiteral("bar-waiting"));
		window->home_open_cancel_button()->click();
		wait_for([&] { return !window->home_open().has_value(); }, "the band's Cancel stops the bar's opening");
		QApplication::processEvents();
		expect(window->home_status_line()->isHidden(), "a cancelled opening leaves the band quiet");
		expect(activity->isHidden(), "and the bar quiet");

		// ---- Home's general status, in the band ----------------------------
		window->statusBar()->showMessage(QStringLiteral("Added octo-org/review-repository."), 0);
		QApplication::processEvents();
		expect(!window->home_status_line()->isHidden() && window->home_status_text()->text() == QStringLiteral("Added octo-org/review-repository.")
				   && window->home_status_line()->tone() == qt::home_status_tone::notice,
			   "what was just done is said in the band under the bar");
		expect(listTop() == quietList && barTop() == quietBar, "a notice moves neither the bar nor the list");
		expect(window->home_status_echoes_bar(), "the status bar beneath leaves the band's sentence to the band");
		renders(QStringLiteral("status-notice"));
		window->set_agent_workspaces_unavailable(true);
		QApplication::processEvents();
		expect(window->home_status_line()->tone() == qt::home_status_tone::problem && window->home_status_text()->text().startsWith(QStringLiteral("Disposable agent workspaces are unavailable")),
			   "a problem the window has is said in the band, with the failure mark");
		expect(std::ranges::find(announcements, window->home_status_text()->text()) != announcements.end(), "the band's status is announced politely");
		expect(!window->home_status_echoes_bar(), "a problem in the band is not the status bar's message");
		renders(QStringLiteral("status-problem"));
		window->set_agent_workspaces_unavailable(false);
		window->statusBar()->clearMessage();
		QApplication::processEvents();
		expect(window->home_status_line()->isHidden(), "a quiet Home hides the band again");
		renders(QStringLiteral("idle"));

		// ---- Return opens in the same turn, and the pull request opens -----
		QListView* keyedView = nullptr;
		{
			const auto keyed = servedRow();
			keyedView        = keyed.View;
			keyedView->setCurrentIndex(keyed.Index);
		}
		keyedView->setFocus(Qt::OtherFocusReason);
		{
			auto key = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
			QApplication::sendEvent(keyedView, &key);
		}
		expect(phase(*window) == core::home_open_phase::opening && row_lines(*window, servedRow()).contains(QStringLiteral("Opening #17…")),
			   "in the same turn as Return, the row says Opening #17…");
		wait_for([&] { return !window->showing_home() && window->session_panel()->status() == core::github_status::pull_request_ready; }, "the slow read opens the pull request", { }, 20000);
		expect(!window->home_open().has_value(), "a loaded read leaves nothing for Home to say");
		window->home_action()->trigger();
		QApplication::processEvents();
		expect(window->home_landing()->state() == qt::landing_state::back_to_review && window->home_landing()->isVisible()
				   && listTop() == quietList && barTop() == quietBar,
			   "Home over the open review offers the way back and keeps the bar and the list where they were");
		reviewingCourse(QStringLiteral("reviewing"));
	}
	// [[BPR-1365]]. The high-contrast themes, over the same open review.
	delay->Milliseconds = 0U;
	for (const auto mode : { core::appearance_mode::high_contrast_light, core::appearance_mode::high_contrast_dark }) {
		window->appearance_settings()->choose_mode(mode);
		wait_for([&] { return owner->status().ProfileMode == mode; }, "the rendered appearance is accepted");
		window->settings()->hide();
		window->choose_home(true);
		QApplication::processEvents();
		expect(window->showing_home(), "each appearance shows Home over the open review");
		reviewingCourse(QStringLiteral("reviewing"));
	}

	// ---- [[BPR-1679]] an open asked for while Home is still loading goes first
	// The owner, 2026-10-20: "When I'm on the home and it's still loading prs,
	// if i double click to open a PR, even though its greyed out, it should
	// give that action priority. Right now it seems like it queues behind the
	// home load before it goes to open the PR". Home's searches are held far
	// longer than an open takes; the double-clicked pull request opens while
	// they would still be held, and Home's refresh resumes and finishes after.
	{
		const auto discovery = [&] { return app::detail::github_application_fixture_access::personal_discovery_evidence(*owner); };
		delay->FailNumber    = 0U;
		window->choose_home(true);
		QApplication::processEvents();
		wait_for([&] { return window->queue_refresh_action()->isEnabled() && discovery().Status.State == application::queue_discovery_state::current; },
				 "Home's refresh is available and Home is current");
		const auto before            = discovery().Statistics;
		delay->DiscoveryMilliseconds = discovery_hold_ms;
		window->queue_refresh_action()->trigger();
		// Two retained pull requests are read first, in one batched read
		// ([[BPR-1680]]), then the repository's two searches, which the slow
		// network holds.
		wait_for([&] { return discovery().Statistics.Submitted >= before.Submitted + 3U; }, "Home's refresh reaches its searches");
		pause(300);
		expect(discovery().Status.State == application::queue_discovery_state::refreshing && discovery().Statistics.Answered == before.Answered + 1U,
			   "Home is still loading, its searches held on the wire");
		wait_for([&] { return window->pending_veils().HomeList->look() == core::pending_look::refreshing; }, "and its list is greyed while it loads", { }, 3000);
		QElapsedTimer opening;
		opening.start();
		double_click(row_of(*window, failing_name));
		expect(phase(*window) == core::home_open_phase::opening, "the greyed row opens at once");
		expect(app::detail::github_application_fixture_access::personal_discovery_evidence(*owner).Statistics.Preempted == before.Preempted + 2U,
			   "in the same turn, Home's two reads in flight give way");
		wait_for([&] { return !window->showing_home() && window->session_panel()->status() == core::github_status::pull_request_ready; },
				 "the pull request opens", { }, static_cast<int>(2U * discovery_hold_ms));
		std::cerr << "opened while Home loaded in " << opening.elapsed() << " ms\n";
		expect(opening.elapsed() < static_cast<qint64>(discovery_hold_ms / 2U), "without waiting behind Home's searches");
		expect(discovery().Status.State == application::queue_discovery_state::refreshing, "Home's refresh is still running");
		delay->DiscoveryMilliseconds = 0U;
		wait_for([&] { return discovery().Status.State == application::queue_discovery_state::current; }, "Home's refresh resumes and finishes", { }, 30000);
		const auto after = discovery().Statistics;
		expect(after.Resubmitted == before.Resubmitted + 2U && after.Failed == before.Failed && after.CancelledRequests == before.CancelledRequests,
			   "the searches that gave way were asked again, and nothing failed");
		expect(discovery().Quiescent, "and discovery is quiet once it has");
	}

	static_cast<void>(QAccessible::installUpdateHandler(previous));
	Announcements       = nullptr;
	delay->Milliseconds = 0U;
	owner->begin_shutdown();
	wait_for([&] { return owner->status().ShutdownComplete; }, "the course drains");
	desktop.reset();
	expect(e2e::remove_course_root(temporary), "the course removes its private root");
	std::cout << "home-open-feedback-public-path:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "home-open-feedback-public-path:fail " << aError.what() << '\n';
	return EXIT_FAILURE;
}
