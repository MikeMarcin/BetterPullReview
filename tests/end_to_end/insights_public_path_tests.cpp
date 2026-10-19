#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/insights_chart.hpp"
#include "bpr/qt/insights_window.hpp"
#include "bpr/qt/settings_window.hpp"
#include "private_data_root.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QStackedWidget>
#include <QTabBar>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

#include <filesystem>
#include <functional>
#include <iostream>

namespace {
	using namespace bpr;
	using tests::expect;
	void until(const std::function<bool()>& aCondition, const char* aMessage)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < 30000) {
			QApplication::processEvents();
			QThread::msleep(2);
		}
		expect(aCondition(), aMessage);
	}

	auto insights() -> qt::insights_window*
	{
		for (auto* widget : QApplication::topLevelWidgets()) {
			if (auto* shell = dynamic_cast<qt::insights_window*>(widget)) {
				return shell;
			}
		}
		return nullptr;
	}

	void shared_chart_contract(QWidget& aParent)
	{
		qt::insights_chart chart{ &aParent };
		chart.resize(600, 160);
		chart.set_colors(QColor{ 20, 150, 95 }, QColor{ 40, 100, 220 });
		qt::insights_chart_data facts;
		facts.Name             = QStringLiteral("Stacked foreground and background");
		facts.Description      = QStringLiteral("Background under foreground with gaps.");
		facts.Quantity         = QStringLiteral("System capacity, %");
		facts.BackgroundName   = QStringLiteral("Background");
		facts.ForegroundName   = QStringLiteral("Foreground");
		facts.EventsName       = QStringLiteral("Events/min");
		facts.StartUnixSeconds = 0;
		facts.EndUnixSeconds   = 300;
		facts.GapSeconds       = 90;
		facts.Capacity         = 100.0;
		facts.Points           = { { 0, 10.0, 20.0, 2.0, false }, { 60, 10.0, 20.0, 3.0, false }, { 300, 10.0, 20.0, 1.0, true } };
		chart.apply(facts);
		expect(chart.quantity_maximum() == 100.0, "capacity lane stays at 100 until exceeded");
		const auto move = [&](const double aX) { const auto local = QPointF{aX,90.0}; QMouseEvent event{QEvent::MouseMove,local,chart.mapToGlobal(local.toPoint()),Qt::NoButton,Qt::NoButton,Qt::NoModifier}; QApplication::sendEvent(&chart,&event); };
		move(67.0);
		expect(chart.hovered_index() == 0, "hover stays with the old sample before its midpoint");
		move(69.0);
		expect(chart.hovered_index() == 1, "hover advances after crossing the midpoint");
		move(590.0);
		expect(chart.hovered_index() == 2 && chart.hover_card_rect().left() >= 8.0 && chart.hover_card_rect().right() <= 592.0 && chart.hover_card_rect().bottom() <= 152.0, "hover flips at the trailing and bottom edges with an eight-pixel inset");
		move(10.0);
		expect(chart.hover_card_rect().left() >= 8.0, "leading hover respects the inset");
		QEvent leave{ QEvent::Leave };
		QApplication::sendEvent(&chart, &leave);
		chart.hide();
		const auto raster = chart.grab().toImage();
		expect(raster.pixelColor(60, 115) != raster.pixelColor(300, 115), "foreground is filled above background, and the unsampled interval is a gap");
		auto empty = facts;
		for (auto& point : empty.Points) {
			point.Background.reset();
		}
		chart.apply(empty);
		const auto emptyRaster = chart.grab().toImage();
		expect(emptyRaster.pixelColor(60, 115) == emptyRaster.pixelColor(300, 115), "negative control: without quantity observations the area disappears");
		facts.Points.back().Foreground = 120.0;
		facts.EndUnixSeconds           = 600;
		chart.apply(facts);
		expect(chart.quantity_maximum() == 130.0 && !chart.hovered_index(), "exceeded capacity expands, and changing the range clears hover");
		expect(!chart.accessibleName().isEmpty() && !chart.accessibleDescription().isEmpty(), "shared chart has a range description");
	}

	void render(qt::insights_window& aShell, app::github_application& aRuntime)
	{
		const auto folder = qEnvironmentVariable("BPR_INSIGHTS_RENDER_ROOT");
		if (folder.isEmpty()) {
			return;
		}
		expect(QDir{ }.mkpath(folder), "render folder exists");
		const auto original = aShell.size();
		aShell.setFixedSize(qt::insights_window::content_size({ 1280, 1000 }));
		for (const auto& [mode, suffix] : { std::pair{ core::appearance_mode::light, QStringLiteral("light") }, std::pair{ core::appearance_mode::dark, QStringLiteral("dark") } }) {
			static_cast<void>(app::detail::github_application_fixture_access::select_appearance_mode(aRuntime, mode));
			QApplication::processEvents();
			auto* tabs = aShell.findChild<QTabBar*>(QStringLiteral("insightsTabs"));
			for (int page = 0; page < tabs->count(); ++page) {
				tabs->setCurrentIndex(page);
				QApplication::processEvents();
				expect(aShell.grab().save(folder + QStringLiteral("/insights-%1-%2.png").arg(page).arg(suffix)), "composed Insights render saved");
				if (page == 2) {
					auto* range = aShell.findChild<QTabBar*>(QStringLiteral("insightsRange"));
					for (auto* candidate : aShell.findChildren<QTabBar*>()) {
						if (candidate->accessibleName() == QStringLiteral("GitHub range")) {
							range = candidate;
						}
					}
					for (int selected = 0; selected < 2; ++selected) {
						range->setCurrentIndex(selected);
						QApplication::processEvents();
						expect(aShell.grab().save(folder + QStringLiteral("/insights-github-%1-%2.png").arg(selected).arg(suffix)), "both real GitHub ranges render");
						auto*       chart = dynamic_cast<qt::insights_chart*>(aShell.findChild<QWidget*>(QStringLiteral("insightsGithubChart")));
						const auto  local = QPointF{ static_cast<double>(chart->width()) * 0.8, static_cast<double>(chart->height()) * 0.65 };
						QMouseEvent hover{ QEvent::MouseMove, local, chart->mapToGlobal(local.toPoint()), Qt::NoButton, Qt::NoButton, Qt::NoModifier };
						QApplication::sendEvent(chart, &hover);
						expect(aShell.grab().save(folder + QStringLiteral("/insights-github-hover-%1-%2.png").arg(selected).arg(suffix)), "ATT-style hover renders on the real GitHub chart");
						QEvent leave{ QEvent::Leave };
						QApplication::sendEvent(chart, &leave);
					}
				}
			}
		}
		aShell.setFixedSize(original);
	}
}

int main(int aArgc, char** aArgv)
try {
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	QTemporaryDir temporary{ qEnvironmentVariable("QUALIFICATION_TARGET_ROOT", QDir::tempPath()) + QStringLiteral("/insights-XXXXXX") };
	expect(temporary.isValid(), "private course parent created");
	const auto root = e2e::private_data_root(temporary).value_or(std::filesystem::path{ });
	expect(!root.empty(), "private course profile root admitted");
	app::detail::github_public_path_fixture_options options;
	options.ApplicationDataRoot      = root;
	options.RegisteredDeviceCourse   = true;
	auto                     desktop = app::detail::github_application_fixture_access::start_desktop(app::detail::github_public_path_fixture::insights_budget_v1, options, [](auto) { });
	app::github_application* runtime = nullptr;
	until([&] { runtime = desktop->runtime(core::default_bpr_profile_id); return runtime && !runtime->windows().empty(); }, "profile runtime and window composed");
	auto* window = runtime->window(runtime->windows().front());
	window->show();
	auto* action = window->findChild<QAction*>(QStringLiteral("openInsightsAction"));
	expect(action && action->shortcut() == QKeySequence{ Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_I }, "the public menu entry carries the agreed shortcut");
#if defined(Q_OS_MACOS)
	expect(action->menuRole() == QAction::ApplicationSpecificRole, "macOS moves Insights into the application menu");
#endif
	action->trigger();
	until([] { return insights() && insights()->isVisible(); }, "menu entry opens the real Insights window");
	auto* shell = insights();
	action->trigger();
	expect(insights() == shell, "repeated open reuses one per-profile instance");
	auto* table = shell->findChild<QTableWidget*>(QStringLiteral("insightsStores"));
	until([&] { return table->rowCount() > 0; }, "the composed snapshot reaches a populated storage ledger");
	expect(std::filesystem::exists(root / "state/v1/insights.sqlite3"), "first launch composed its real Insights store");
	bool foundInsights = false;
	for (int row = 0; row < table->rowCount(); ++row) {
		foundInsights = foundInsights || table->item(row, 0)->text() == QStringLiteral("insights.sqlite3");
	}
	expect(foundInsights && !shell->findChild<QLabel*>(QStringLiteral("insightsLocalData"))->text().contains(QStringLiteral("unavailable")), "Summary and Storage display real data, including Insights itself");
	auto* budget = shell->findChild<QWidget*>(QStringLiteral("insightsBudget"));
	until([&] { return budget->property("ownRequests").toULongLong() == 412; }, "seeded real store reaches the Summary budget");
	expect(budget->accessibleDescription().contains(QStringLiteral("1,240")) && budget->accessibleDescription().contains(QStringLiteral("3,348")), "two-client split uses persisted BPR count and the lowest remaining response");
	expect(shell->findChild<QLabel*>(QStringLiteral("insightsProblems"))->text() == QStringLiteral("2"), "Problems arrives from the same local snapshot");
	shared_chart_contract(*window);
	const auto beforeRefresh = app::detail::github_application_fixture_access::evidence(*runtime);

	const auto frame     = shell->frameGeometry();
	const auto content   = shell->size();
	auto*      tabs      = shell->findChild<QTabBar*>(QStringLiteral("insightsTabs"));
	auto*      pages     = shell->findChild<QStackedWidget*>(QStringLiteral("insightsPages"));
	auto*      toolbar   = shell->findChild<QWidget*>(QStringLiteral("insightsToolbar"));
	auto*      header    = shell->findChild<QWidget*>(QStringLiteral("insightsHeader"));
	const auto headerBox = header->geometry();
	expect(toolbar->geometry().top() == 0 && toolbar->height() == 40 && shell->windowFlags().testFlag(Qt::ExpandedClientAreaHint) && shell->windowFlags().testFlag(Qt::NoTitleBarBackgroundHint) && !shell->testAttribute(Qt::WA_ContentsMarginsRespectsSafeArea), "tabs and actions occupy the expanded native title row without an extra safe-area row");
	expect(toolbar->geometry().bottom() < headerBox.top() && headerBox.bottom() < pages->geometry().top(), "the unified title row precedes a fixed title/status header and page body");
	expect(qt::insights_window::content_size({ 1280, 1000 }) == QSize{ 900, 542 } && content == qt::insights_window::content_size(shell->screen()->availableGeometry().size()), "the mockup dimensions clamp to the opening screen");
	expect(tabs->count() == 6 && shell->minimumSize() == shell->maximumSize(), "six pages share one non-resizable size");
	for (int page = 0; page < tabs->count(); ++page) {
		tabs->setCurrentIndex(page);
		if (page == 2) {
			auto* chart      = dynamic_cast<qt::insights_chart*>(shell->findChild<QWidget*>(QStringLiteral("insightsGithubChart")));
			auto* categories = shell->findChild<QTableWidget*>(QStringLiteral("insightsRequestCategories"));
			expect(chart->height() > 200 && categories->mapTo(shell, QPoint{ }).x() > chart->mapTo(shell, QPoint{ }).x() + chart->width(), "ATT composition gives the chart height and keeps bounded facts beside it");
		}
		for (auto* range : pages->currentWidget()->findChildren<QTabBar*>()) {
			for (int index = 0; index < range->count(); ++index) {
				range->setCurrentIndex(index);
				QApplication::processEvents();
				expect(shell->frameGeometry() == frame, "every range keeps the frame");
			}
		}
		for (int refresh = 0; refresh < 3; ++refresh) {
			shell->refresh();
			QApplication::processEvents();
			expect(shell->frameGeometry() == frame && shell->size() == content && header->geometry() == headerBox, "three refreshes on each page preserve frame and content");
		}
	}
	const auto afterRefresh = app::detail::github_application_fixture_access::evidence(*runtime);
	expect(beforeRefresh.AccountReadCount == afterRefresh.AccountReadCount && beforeRefresh.SmartAcquireCount == afterRefresh.SmartAcquireCount && beforeRefresh.BootstrapFactoryAttempts == afterRefresh.BootstrapFactoryAttempts, "all local refreshes issue no GitHub, Git or credential acquisition");
	window->github_connect_action()->trigger();
	until([&] { return window->session_panel()->status() == core::github_status::session_ready; }, "normal fixture account connects");
	until([&] { shell->refresh(); return budget->property("ownRequests").toULongLong() > 412; }, "real provider transport observations join the persisted current-window count");
	expect(qt::insights_window::content_size({ 800, 500 }) == QSize{ 764, 464 }, "smaller screen clamps both content dimensions with an 18 px margin");
	shell->setFixedSize(qt::insights_window::content_size({ 800, 500 }));
	for (int page = 0; page < tabs->count(); ++page) {
		tabs->setCurrentIndex(page);
		QApplication::processEvents();
		for (auto* item : pages->currentWidget()->findChildren<QWidget*>(QString{ }, Qt::FindDirectChildrenOnly)) {
			expect(pages->currentWidget()->rect().contains(item->geometry()), "every page fits the clamped content without page scrolling");
		}
	}
	shell->setFixedSize(content);
	render(*shell, *runtime);
	shell->findChild<QAbstractButton*>(QStringLiteral("insightsExport"))->click();
	QDialog* exportDialog = nullptr;
	until([&] {
		for (auto* widget : QApplication::topLevelWidgets()) {
			if (widget->objectName() == QStringLiteral("diagnosticExportDialog") && widget->isVisible()) {
				exportDialog = qobject_cast<QDialog*>(widget);
			}
		}
		return exportDialog && exportDialog->findChild<QPlainTextEdit*>(QStringLiteral("releaseMaintenancePreview"))->toPlainText().contains(QStringLiteral("\"section\":\"insights\""));
	},
		  "the header action opens the existing frozen export preview with byte-only Insights data");
	exportDialog->reject();

	tabs->setCurrentIndex(3);
	shell->findChild<QPushButton*>(QStringLiteral("insightsManageCaches"))->click();
	expect(window->settings() && window->settings()->isVisible() && window->settings()->current_page() == qt::settings_page::storage_and_privacy, "Manage cached repositories opens existing cache controls");
	window->settings()->hide();
	QKeyEvent escape{ QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier };
	QApplication::sendEvent(shell, &escape);
	expect(!shell->isVisible(), "Escape closes Insights");
	for (auto* timer : shell->findChildren<QTimer*>()) {
		expect(!timer->isActive(), "hidden window has no running refresh or status timer");
	}
	shell->refresh();
	expect(!shell->isVisible(), "hidden refresh cannot reopen the window");
	desktop->quit();
	until([&] { return desktop->is_quiescent(); }, "profile drains local Insights effects and completions before retiring");
	desktop.reset();
	expect(e2e::remove_course_root(temporary), "course profile removed");
	std::cout << "insights-public-path:pass menu singleton real-storage all-tabs all-ranges three-refreshes fixed-frame manage-existing-controls escape hidden-timers shutdown\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
