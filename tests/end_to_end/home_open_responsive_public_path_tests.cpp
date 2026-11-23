// [[BPR-2216]]. Opening a pull request from Home is responsive, through the
// composed application on the fixture GitHub server. The owner, 2026-11-16:
// "The pull request window doesn't open very quickly. If I click home from a
// pull request that's not finished loading it doesn't change it to the return
// button and it doesn't have the active pull request near the top of home."
//
// The fixture transport holds the pull request's first answers for seconds,
// and every later answer for a while, so the read is visibly slow. A
// double-click on the Home row shows the pull request's page in the gesture's
// own turn, naming it and loading; Home pressed while it loads offers Return
// to it and lists it as the Active review, with its row marked; Return goes
// back to the loading page; and the loaded review keeps all of that. In an
// optimized build the page's first frame is painted within 100 ms of the
// double-click and no turn on the way is longer than 100 ms. Nothing reaches
// a live provider.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "account_setup_course.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/home_status_line.hpp"
#include "bpr/qt/pending_veil.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/review_landing_model.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMouseEvent>
#include <QPushButton>
#include <QRegularExpression>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QThread>
#include <QToolBar>
#include <QToolButton>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {
	using namespace bpr;
	using bpr::tests::expect;

	constexpr auto viewer      = "octo-reviewer";
	constexpr auto served      = 17U;
	constexpr auto served_name = "octo-org/review-repository#17";
	/// The pull request's first answers are held this long: long enough that
	/// nothing GitHub says can explain what the window shows meanwhile.
	constexpr auto first_hold_ms = 4000U;
	/// Every later answer of the read is held this long, so it fills in part
	/// by part.
	constexpr auto page_hold_ms = 250U;

	/// The longest single event-loop turn the course has driven, in ms.
	qint64 LongestTurn = 0;

	void turn()
	{
		QElapsedTimer one;
		one.start();
		QApplication::processEvents();
		LongestTurn = std::max(LongestTurn, one.elapsed());
	}

	void wait_for(const std::function<bool()>& aCondition, const char* aMessage, const int aTimeoutMs = 30000)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < aTimeoutMs) {
			turn();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}

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

	/// The double-click a hand makes on a row, with no event-loop turn in
	/// between or after.
	void double_click(const found_row& aRow)
	{
		const auto point   = QPointF{ aRow.View->visualRect(aRow.Index).center() };
		const auto global  = aRow.View->viewport()->mapToGlobal(point);
		auto       press   = QMouseEvent{ QEvent::MouseButtonPress, point, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		auto       release = QMouseEvent{ QEvent::MouseButtonRelease, point, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		auto       doubled = QMouseEvent{ QEvent::MouseButtonDblClick, point, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(aRow.View->viewport(), &press);
		QApplication::sendEvent(aRow.View->viewport(), &release);
		QApplication::sendEvent(aRow.View->viewport(), &doubled);
		QApplication::sendEvent(aRow.View->viewport(), &release);
	}

	/// Records the Overview's first paint and whether its header is pending.
	class first_paint final : public QObject
	{
	public:
		explicit first_paint(qt::pull_request_overview_page* aWidget)
			: Widget{ aWidget }
		{
			aWidget->installEventFilter(this);
		}

		void arm()
		{
			Painted        = false;
			AwaitingHeader = false;
			Clock.start();
			At = -1;
		}

		[[nodiscard]] auto painted() const noexcept -> bool { return Painted; }
		[[nodiscard]] auto awaiting_header() const noexcept -> bool { return AwaitingHeader; }
		[[nodiscard]] auto at() const noexcept -> qint64 { return At; }

	protected:
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override
		{
			if (aWatched == Widget && aEvent->type() == QEvent::Paint && Clock.isValid() && !Painted) {
				Painted            = true;
				At                 = Clock.elapsed();
				const auto* header = Widget->pending_veils().Header;
				AwaitingHeader     = header != nullptr && header->busy();
			}
			return false;
		}

	private:
		qt::pull_request_overview_page* Widget = nullptr;
		QElapsedTimer                   Clock;
		bool                            Painted        = false;
		bool                            AwaitingHeader = false;
		qint64                          At             = -1;
	};

	[[nodiscard]] auto home_button(qt::workbench_window& aWindow) -> QToolButton*
	{
		for (auto* const bar : aWindow.findChildren<QToolBar*>()) {
			if (auto* const button = qobject_cast<QToolButton*>(bar->widgetForAction(aWindow.home_action())); button != nullptr) {
				return button;
			}
		}
		return nullptr;
	}

	/// Every sentence the window shows: its visible labels and the words of
	/// every busy veil, either in the status bar or outside it.
	[[nodiscard]] auto shown_sentences(qt::workbench_window& aWindow, const bool aStatusBar) -> QStringList
	{
		auto        result = QStringList{ };
		auto* const bar    = aWindow.statusBar();
		const auto  inBar  = [&](QWidget* aWidget) { return aWidget == bar || bar->isAncestorOf(aWidget); };
		for (auto* const label : aWindow.findChildren<QLabel*>()) {
			if (label->isVisible() && !label->text().trimmed().isEmpty() && inBar(label) == aStatusBar) {
				result.push_back(label->text().simplified());
			}
		}
		for (auto* const widget : aWindow.findChildren<QWidget*>()) {
			auto* const veil = dynamic_cast<qt::pending_veil*>(widget);
			if (veil != nullptr && veil->isVisible() && veil->busy() && !veil->shown_text().trimmed().isEmpty() && inBar(veil) == aStatusBar) {
				result.push_back(veil->shown_text().simplified());
			}
		}
		if (aStatusBar && !bar->currentMessage().isEmpty()) {
			result.push_back(bar->currentMessage().simplified());
		}
		return result;
	}

	/// A sentence's words, without case, punctuation or counts.
	[[nodiscard]] auto gist(QString aText) -> QString
	{
		aText = aText.toLower();
		aText.remove(QRegularExpression{ QStringLiteral("[0-9]+") });
		aText.replace(QRegularExpression{ QStringLiteral("[^a-z ]") }, QStringLiteral(" "));
		return aText.simplified();
	}

	/// [[BPR-2216]]. The owner, 2026-11-16: "Seems like overview has a loading
	/// status text that looks almost 1:1 copy of the status bar's loading
	/// status text just below it." and "Seems home has a similar thing." What
	/// is loading is said once, by the status bar.
	void expect_said_once(qt::workbench_window& aWindow, const char* aWhere)
	{
		const auto page   = shown_sentences(aWindow, false);
		const auto status = shown_sentences(aWindow, true);
		std::cerr << aWhere << " status bar:";
		for (const auto& line : status) {
			std::cerr << " [" << line.toStdString() << "]";
		}
		std::cerr << "\n"
				  << aWhere << " page:";
		for (const auto& line : page) {
			std::cerr << " [" << line.toStdString() << "]";
		}
		std::cerr << '\n';
		for (const auto& line : page) {
			expect(!line.contains(QStringLiteral("Still waiting for GitHub")), std::string{ aWhere } + ": the page does not repeat the read's step: " + line.toStdString());
			for (const auto& said : status) {
				const auto a = gist(line);
				const auto b = gist(said);
				expect(a.isEmpty() || b.isEmpty() || (a != b && !(b.size() > 12 && a.contains(b)) && !(a.size() > 12 && b.contains(a))),
					   std::string{ aWhere } + ": the page does not say the status bar's sentence again: " + line.toStdString() + " / " + said.toStdString());
			}
		}
	}

	/// What Home says of the pull request it is over, for the log.
	void describe(qt::workbench_window& aWindow, const char* aWhen)
	{
		auto* const landing = aWindow.home_landing();
		auto* const button  = home_button(aWindow);
		std::cerr << aWhen << ": home=" << aWindow.showing_home() << " status=" << static_cast<int>(aWindow.session_panel()->status())
				  << " returns=" << aWindow.home_action_returns() << " home-text=" << aWindow.home_action()->text().toStdString()
				  << " checked=" << aWindow.home_action()->isChecked() << " button=" << (button != nullptr ? button->accessibleName().toStdString() : std::string{ "none" })
				  << " landing=" << static_cast<int>(landing->state()) << " landing-visible=" << landing->isVisible()
				  << " landing-title=" << landing->title_label()->text().toStdString()
				  << " reviewing=" << (aWindow.queue_landing()->reviewing().has_value() ? aWindow.queue_landing()->reviewing()->Number : 0) << '\n';
	}

	/// The Active review card names the pull request with its title and Back
	/// to the review; the toolbar's Home is the way back to it; its row is
	/// marked as the review being read.
	void expect_active(qt::workbench_window& aWindow, const char* aWhen)
	{
		describe(aWindow, aWhen);
		auto* const landing = aWindow.home_landing();
		expect(aWindow.showing_home(), "Home is shown");
		expect(aWindow.home_action_returns() && !aWindow.home_action()->isCheckable() && !aWindow.home_action()->isChecked()
				   && aWindow.home_action()->text().startsWith(QStringLiteral("Return to #17")),
			   "the Home button is Return to #17, unchecked");
		auto* const button = home_button(aWindow);
		expect(button != nullptr && button->accessibleName().startsWith(QStringLiteral("Return to #17")), "the toolbar button is named Return to #17");
		expect(landing->state() == qt::landing_state::back_to_review && landing->isVisible() && landing->title_label()->text() == QString::fromUtf8(served_name)
				   && landing->accessibleName() == QStringLiteral("Active review"),
			   "the Active review card names #17");
		expect(!landing->review_title_label()->text().isEmpty(), "the Active review card carries the title");
		expect(landing->primary_button()->isVisible() && landing->primary_button()->isEnabled(), "the Active review card offers Back to the review");
		expect(landing->mapTo(&aWindow, QPoint{ }).y() < aWindow.home_list()->mapTo(&aWindow, QPoint{ }).y(), "the Active review card stands above Home's lists");
		const auto reviewing = aWindow.queue_landing()->reviewing();
		expect(reviewing.has_value() && reviewing->Number == served && reviewing->Repository == "octo-org/review-repository", "the queue marks #17 as the review being read");
		const auto row = row_of(aWindow, served_name);
		expect(row.View != nullptr && row.Index.data(qt::home_row_role::reviewing).toBool(), "the #17 row carries the reviewing mark");
	}
} // namespace

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	const auto    renderArgument  = QApplication::arguments().indexOf(QStringLiteral("--render-directory"));
	const auto    renderDirectory = renderArgument >= 0 ? QApplication::arguments().value(renderArgument + 1) : QString{ };
	QTemporaryDir temporary;
	expect(temporary.isValid(), "private course root");
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
	const auto render = [&](const QString& aName) {
		if (renderDirectory.isEmpty()) {
			return;
		}
		expect(QDir{ }.mkpath(renderDirectory), "render directory exists");
		expect(window->grab().save(renderDirectory + QStringLiteral("/%1.png").arg(aName)), "render saved");
	};

	{
		auto plan   = e2e::account_setup_driver::plan{ };
		plan.Way    = core::account_setup_way::browser;
		auto driver = e2e::account_setup_driver{ *window, std::move(plan) };
		driver.begin();
		wait_for([&] { return window->session_panel()->status() == core::github_status::session_ready && !window->account_setup()->isVisible(); },
				 "Home sign-in reaches an account session and leaves the setup");
	}
	window->open_repository_addition();
	wait_for([&] { return window->repository_addition_draft()->isVisible() && window->saved_repository_name()->isEnabled(); }, "Settings opens the new-repository card");
	window->saved_repository_name()->setText(QStringLiteral("octo-org/review-repository"));
	window->repository_addition_draft()->findChild<QPushButton*>(QStringLiteral("repositoryAdditionAdd"))->click();
	wait_for([&] { return !window->repository_addition_draft()->isVisible() && row_of(*window, served_name).View != nullptr; }, "the saved repository's pull request is on Home");
	window->settings()->hide();
	window->home_action()->trigger();
	wait_for([&] { return window->showing_home(); }, "Home is shown");
	for (auto settle = 0; settle < 20; ++settle) {
		turn();
	}
	describe(*window, "before");
	expect(!window->home_action_returns(), "with nothing open, Home is Home");

	// ---- BPR-2216:page-at-once
	const auto delayedBefore = delay->Delayed.load();
	delay->Milliseconds      = first_hold_ms;
	delay->PageMilliseconds  = page_hold_ms;
	auto* const surface      = window->overview_page();
	auto        paint        = first_paint{ surface };
	LongestTurn              = 0;
	QElapsedTimer clicked;
	paint.arm();
	clicked.start();
	double_click(row_of(*window, served_name));
	const auto gestureMs = clicked.elapsed();
	describe(*window, "in the gesture's turn");
	const auto leftHome    = !window->showing_home();
	const auto statusAfter = window->session_panel()->status();
	wait_for([&] { return paint.painted() || clicked.elapsed() > 2000; }, "the event loop runs after the double-click");
	const auto paintMs  = paint.painted() ? paint.at() : -1;
	const auto turnsMax = std::max(LongestTurn, gestureMs);
	std::cout << "home-open-responsive:gesture-ms=" << gestureMs << " first-paint-ms=" << paintMs << " longest-turn-ms=" << turnsMax
			  << " left-home-in-turn=" << leftHome << " header-pending-at-paint=" << paint.awaiting_header() << '\n';
	render(QStringLiteral("page-at-once"));
	expect(statusAfter == core::github_status::loading_pull_request, "the read is under way");
	expect(leftHome, "the window leaves Home for the pull request's page in the gesture's own turn");
	expect(paint.painted(), "the pull request's page is painted");
	expect(paint.awaiting_header(), "the first Overview frame still awaits its header from GitHub");
	expect(window->home_open().has_value() && window->home_open()->State.has_value(), "the read is the one Home opened");
	{
		// The title row names it before GitHub does: Home listed it.
		auto* const identity = window->identity_label();
		std::cerr << "identity=" << (identity != nullptr ? identity->text().toStdString() : std::string{ }) << " visible=" << (identity != nullptr && identity->isVisible()) << '\n';
		expect(identity != nullptr && identity->isVisible() && identity->text().contains(QStringLiteral("#17")), "the title row names #17 while it loads");
	}
	// A provider worker can enter the hold after the window has painted.
	// The gesture and paint measurements above already ended.
	wait_for([&] { return delay->Delayed.load() > delayedBefore; }, "the opened pull request reaches the held transport");
	// The read's first parts arrive, slowly; what is loading is said once.
	delay->Milliseconds = 0U;
	wait_for([&] { return window->overview_page()->pending_veils().Header != nullptr && !window->overview_page()->pending_veils().Header->busy(); }, "GitHub names the pull request");
	for (auto settle = 0; settle < 10; ++settle) {
		turn();
	}
	render(QStringLiteral("page-filling-in"));
	expect_said_once(*window, "Overview while loading");
	{
		// The Updates line has no comparison to prepare yet: it does not say
		// the read again; the status bar does.
		auto* const epochs = window->epoch_selectors();
		std::cerr << "updates: readiness=" << epochs->readiness_text().toStdString() << " button=" << epochs->comparing_button()->text().toStdString()
				  << " status=" << window->status_label()->text().toStdString() << '\n';
		expect(epochs->readiness_text().isEmpty() && !epochs->comparing_button()->text().contains(QStringLiteral("Loading"))
				   && window->status_label()->text().startsWith(QStringLiteral("Loading")),
			   "the Updates line leaves the read to the status bar");
	}
	// The rest of the read is held long enough to go to Home and back.
	delay->Milliseconds     = first_hold_ms;
	delay->PageMilliseconds = 5U * first_hold_ms;
	// The owner, 2026-11-17, of look v003: "The heck now this double status
	// came back on the review screen." Every mode and every waiting pane,
	// drawers included, waits without a sentence while the status bar says
	// what the read is on.
	{
		const auto said = QRegularExpression{ QStringLiteral("loading|refreshing|still waiting|pages received|catalogue"), QRegularExpression::CaseInsensitiveOption };
		for (const auto modality : { qt::review_modality::overview, qt::review_modality::review, qt::review_modality::discuss, qt::review_modality::threads }) {
			window->choose_modality(modality);
			for (auto settle = 0; settle < 10; ++settle) {
				turn();
			}
			expect(window->session_panel()->status() == core::github_status::loading_pull_request, "the read is still under way in every mode");
			const auto name = std::to_string(static_cast<int>(modality));
			auto*      bar  = window->statusBar();
			for (auto* const widget : window->findChildren<QWidget*>()) {
				if (widget == bar || bar->isAncestorOf(widget)) {
					continue;
				}
				if (auto* const veil = dynamic_cast<qt::pending_veil*>(widget); veil != nullptr && veil->busy()) {
					expect(veil->shown_text().isEmpty(), "mode " + name + ": no waiting pane says a sentence: " + veil->objectName().toStdString() + " " + veil->shown_text().toStdString());
				}
				if (auto* const label = qobject_cast<QLabel*>(widget); label != nullptr && label->isVisible()) {
					expect(!label->text().contains(said), "mode " + name + ": no label on the page says what loads: " + label->objectName().toStdString() + " " + label->text().toStdString());
				}
				if (auto* const button = qobject_cast<QAbstractButton*>(widget); button != nullptr && button->isVisible()) {
					expect(!button->text().contains(said), "mode " + name + ": no button says what loads: " + button->text().toStdString());
				}
			}
			expect(window->status_label()->text().startsWith(QStringLiteral("Loading")), "mode " + name + ": the status bar says what loads");
			render(QStringLiteral("review-mode-%1-loading").arg(static_cast<int>(modality)));
		}
		window->choose_modality(qt::review_modality::overview);
		turn();
	}
#if defined(NDEBUG)
	expect(paintMs >= 0 && paintMs < 100, "the page's first frame is painted within 100 ms of the double-click in an optimized build");
	expect(turnsMax < 100, "no turn between the double-click and that frame takes 100 ms or more in an optimized build");
#endif

	// ---- BPR-2216:return-while-loading and :active-while-loading
	window->home_action()->trigger();
	for (auto settle = 0; settle < 5; ++settle) {
		turn();
	}
	expect(window->session_panel()->status() == core::github_status::loading_pull_request, "#17 is still loading");
	render(QStringLiteral("home-while-loading"));
	expect_active(*window, "Home while loading");
	// Return goes back to the page that is loading.
	window->home_action()->trigger();
	for (auto settle = 0; settle < 5; ++settle) {
		turn();
	}
	describe(*window, "after Return while loading");
	std::cerr << "showing-home=" << window->showing_home() << " status=" << static_cast<int>(window->session_panel()->status()) << '\n';
	expect(!window->showing_home() && window->session_panel()->status() == core::github_status::loading_pull_request, "Return goes back to #17's loading page");
	expect(!window->home_action_returns() && window->home_action()->isCheckable() && !window->home_action()->isChecked(), "over the page, Home is Home again");
	// Home's Back to the review does the same.
	window->home_action()->trigger();
	wait_for([&] { return window->showing_home(); }, "Home again while loading");
	window->home_landing()->primary_button()->click();
	for (auto settle = 0; settle < 5; ++settle) {
		turn();
	}
	expect(!window->showing_home(), "Back to the review goes back to #17's page");

	// The review arrives behind Home when Home was chosen while it loaded,
	// and keeps all of it.
	window->home_action()->trigger();
	wait_for([&] { return window->showing_home(); }, "Home over the loading review");
	delay->Milliseconds     = 0U;
	delay->PageMilliseconds = 0U;
	wait_for([&] { return window->session_panel()->status() == core::github_status::pull_request_ready; }, "#17 finishes loading");
	for (auto settle = 0; settle < 5; ++settle) {
		turn();
	}
	expect(window->showing_home(), "the loaded review arrives behind Home rather than taking the window");
	expect_active(*window, "Home once loaded");
	window->home_action()->trigger();
	for (auto settle = 0; settle < 5; ++settle) {
		turn();
	}
	expect(!window->showing_home(), "Return goes to the loaded review");
	window->home_action()->trigger();
	wait_for([&] { return window->showing_home(); }, "Home over the loaded review");
	render(QStringLiteral("home-once-loaded"));

	// ---- Home while it finds my pull requests: said once ----
	{
		const auto discovery         = [&] { return app::detail::github_application_fixture_access::personal_discovery_evidence(*owner); };
		delay->DiscoveryMilliseconds = first_hold_ms;
		wait_for([&] { return window->queue_refresh_action()->isEnabled() && discovery().Status.State == application::queue_discovery_state::current && discovery().Quiescent; },
				 "Home is current before its refresh");
		window->queue_refresh_action()->trigger();
		wait_for([&] { return discovery().Status.State == application::queue_discovery_state::refreshing; }, "Home's refresh begins");
		for (auto settle = 0; settle < 20; ++settle) {
			turn();
		}
		render(QStringLiteral("home-finding"));
		expect_said_once(*window, "Home while finding");
		// The owner, 2026-11-16: "there are still 2 differnet places status
		// messages appear. They just toggle back and forth now which is
		// terrible." A hovered action's help is the status bar's too.
		window->statusBar()->showMessage(window->queue_refresh_action()->statusTip(), 0);
		turn();
		expect(window->home_status_line()->isHidden() && window->statusBar()->isVisible() && !window->home_status_echoes_bar(),
			   "Home has one status place, the status bar, which never gives way to a box");
		render(QStringLiteral("home-hover-help"));
		window->statusBar()->clearMessage();
		const auto status = shown_sentences(*window, true);
		expect(std::ranges::any_of(status, [](const QString& aLine) { return aLine.startsWith(QStringLiteral("Refreshing")) || aLine.startsWith(QStringLiteral("Updating")); }),
			   "the status bar says Home is refreshing");
		const auto page = shown_sentences(*window, false);
		expect(std::ranges::none_of(page, [](const QString& aLine) { return aLine.startsWith(QStringLiteral("Finding my pull requests")) || aLine.startsWith(QStringLiteral("Refreshing my pull requests")); }),
			   "and Home's page does not say it again");
		delay->DiscoveryMilliseconds = 0U;
		wait_for([&] { return discovery().Status.State == application::queue_discovery_state::current; }, "Home's refresh finishes");
	}

	owner->begin_shutdown();
	wait_for([&] { return owner->status().ShutdownComplete; }, "the course drains");
	desktop.reset();
	expect(e2e::remove_course_root(temporary), "the course removes its private root");
	std::cout << "home-open-responsive-public-path:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "home-open-responsive-public-path:fail " << aError.what() << '\n';
	return EXIT_FAILURE;
}
