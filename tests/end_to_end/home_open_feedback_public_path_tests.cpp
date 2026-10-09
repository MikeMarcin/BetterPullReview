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
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/home_status_line.hpp"
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
#include <QMouseEvent>
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

	constexpr auto viewer       = "octo-reviewer";
	constexpr auto served       = 17U;
	constexpr auto failing      = 4001U;
	constexpr auto delay_ms     = 3000U;
	constexpr auto served_name  = "octo-org/review-repository#17";
	constexpr auto failing_name = "octo-org/review-repository#4001";

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

		// ---- a single click only selects ([[BPR-1364]]) -------------------
		const auto row          = row_of(*window, served_name);
		const auto delayedFirst = delay->Delayed.load();
		click(row);
		QApplication::processEvents();
		expect(!phase(*window).has_value() && window->showing_home() && delay->Delayed.load() == delayedFirst,
			   "a single click on a row asks GitHub for nothing and stays on Home");
		expect(row.View->currentIndex() == row.Index && row.View->selectionModel()->isSelected(row.Index),
			   "a single click makes the row current and highlighted");
		expect(!row_lines(*window, row).contains(QStringLiteral("Opening")), "a selected row does not say it is opening");

		// ---- the same turn as the double-click -----------------------------
		double_click(row);
		// No event loop turn has run since the release.
		expect(phase(*window) == core::home_open_phase::opening, "in the same turn as the double-click, the core's opening reaches the window");
		expect(window->home_list()->row_delegate()->open().Identity == QString::fromUtf8(served_name)
				   && window->home_list()->row_delegate()->open().Phase == qt::home_row_open_phase::opening,
			   "in the same turn as the double-click, the double-clicked row is the open row");
		expect(row_lines(*window, row).contains(QStringLiteral("Opening #17…")), "in the same turn as the double-click, the row says Opening #17…");
		expect(window->home_list()->open_view() == row.View, "the double-clicked row's own list carries it");
		expect(window->showing_home() && window->home_status_line()->isHidden(), "the row says it, so the band adds nothing");
		wait_for([&] { return delay->Delayed.load() > delayedFirst; }, "the read reaches the slow transport");
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
		expect(!row_lines(*window, row_of(*window, served_name)).contains(QStringLiteral("Opening")), "the row says why it is listed again");

		// ---- a failure on its row, and Retry -------------------------------
		const auto failingRow = row_of(*window, failing_name);
		double_click(failingRow);
		expect(phase(*window) == core::home_open_phase::opening && window->home_list()->open_view() == failingRow.View, "the failing row is opening at once");
		wait_for([&] { return phase(*window) == core::home_open_phase::failed; }, "the failing read fails after its wait", { }, 15000);
		QApplication::processEvents();
		expect(window->showing_home() && action->isVisible() && action->text() == QStringLiteral("Retry"), "the failed row offers Retry");
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
		wait_for([&] { return phase(*window) == core::home_open_phase::waiting; }, "the bar's opening waits", { }, 3000);
		QApplication::processEvents();
		expect(window->home_open_cancel_button()->isVisible() && window->home_open_notice()->text().startsWith(QStringLiteral("Opening octo-org/review-repository#18: ")),
			   "the band names the step and offers Cancel");
		renders(QStringLiteral("bar-waiting"));
		window->home_open_cancel_button()->click();
		wait_for([&] { return !window->home_open().has_value(); }, "the band's Cancel stops the bar's opening");
		QApplication::processEvents();
		expect(window->home_status_line()->isHidden(), "a cancelled opening leaves the band quiet");

		// ---- Home's general status, in the band ----------------------------
		window->statusBar()->showMessage(QStringLiteral("Added octo-org/review-repository."), 0);
		QApplication::processEvents();
		expect(!window->home_status_line()->isHidden() && window->home_status_text()->text() == QStringLiteral("Added octo-org/review-repository.")
				   && window->home_status_line()->tone() == qt::home_status_tone::notice,
			   "what was just done is said in the band under the bar");
		renders(QStringLiteral("status-notice"));
		window->set_agent_workspaces_unavailable(true);
		QApplication::processEvents();
		expect(window->home_status_line()->tone() == qt::home_status_tone::problem && window->home_status_text()->text().startsWith(QStringLiteral("Disposable agent workspaces are unavailable")),
			   "a problem the window has is said in the band, with the failure mark");
		expect(std::ranges::find(announcements, window->home_status_text()->text()) != announcements.end(), "the band's status is announced politely");
		renders(QStringLiteral("status-problem"));
		window->set_agent_workspaces_unavailable(false);
		window->statusBar()->clearMessage();
		QApplication::processEvents();
		expect(window->home_status_line()->isHidden(), "a quiet Home hides the band again");
		renders(QStringLiteral("idle"));

		// ---- Return opens in the same turn, and the pull request opens -----
		const auto keyed = row_of(*window, served_name);
		keyed.View->setCurrentIndex(keyed.Index);
		keyed.View->setFocus(Qt::OtherFocusReason);
		{
			auto key = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
			QApplication::sendEvent(keyed.View, &key);
		}
		expect(phase(*window) == core::home_open_phase::opening && row_lines(*window, keyed).contains(QStringLiteral("Opening #17…")),
			   "in the same turn as Return, the row says Opening #17…");
		wait_for([&] { return !window->showing_home() && window->session_panel()->status() == core::github_status::pull_request_ready; }, "the slow read opens the pull request", { }, 20000);
		expect(!window->home_open().has_value(), "a loaded read leaves nothing for Home to say");
		window->home_action()->trigger();
		QApplication::processEvents();
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
