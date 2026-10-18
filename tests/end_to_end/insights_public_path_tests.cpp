#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/insights_window.hpp"
#include "bpr/qt/settings_window.hpp"
#include "private_data_root.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
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

	void render(qt::insights_window& aShell, app::github_application& aRuntime)
	{
		const auto folder = qEnvironmentVariable("BPR_INSIGHTS_RENDER_ROOT");
		if (folder.isEmpty()) {
			return;
		}
		expect(QDir{ }.mkpath(folder), "render folder exists");
		for (const auto& [mode, suffix] : { std::pair{ core::appearance_mode::light, QStringLiteral("light") }, std::pair{ core::appearance_mode::dark, QStringLiteral("dark") } }) {
			static_cast<void>(app::detail::github_application_fixture_access::select_appearance_mode(aRuntime, mode));
			QApplication::processEvents();
			auto* tabs = aShell.findChild<QTabWidget*>(QStringLiteral("insightsTabs"));
			for (int page = 0; page < tabs->count(); ++page) {
				tabs->setCurrentIndex(page);
				QApplication::processEvents();
				expect(aShell.grab().save(folder + QStringLiteral("/insights-%1-%2.png").arg(page).arg(suffix)), "composed Insights render saved");
			}
		}
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
	auto                     desktop = app::detail::github_application_fixture_access::start_desktop(app::detail::github_public_path_fixture::profile_window_restoration_v1, options, [](auto) { });
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
	const auto frame   = shell->frameGeometry();
	const auto content = shell->size();
	auto*      tabs    = shell->findChild<QTabWidget*>(QStringLiteral("insightsTabs"));
	expect(tabs->count() == 6 && shell->minimumSize() == shell->maximumSize(), "six pages share one non-resizable size");
	for (int page = 0; page < tabs->count(); ++page) {
		tabs->setCurrentIndex(page);
		for (auto* range : tabs->currentWidget()->findChildren<QComboBox*>()) {
			for (int index = 0; index < range->count(); ++index) {
				range->setCurrentIndex(index);
				QApplication::processEvents();
				expect(shell->frameGeometry() == frame, "every range keeps the frame");
			}
		}
		for (int refresh = 0; refresh < 3; ++refresh) {
			shell->refresh();
			QApplication::processEvents();
			expect(shell->frameGeometry() == frame && shell->size() == content, "three refreshes on each page preserve frame and content");
		}
	}
	expect(qt::insights_window::content_size({ 800, 600 }) == QSize{ 764, 564 }, "smaller screen clamps both content dimensions with an 18 px margin");
	render(*shell, *runtime);
	shell->findChild<QPushButton*>(QStringLiteral("insightsExport"))->click();
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
