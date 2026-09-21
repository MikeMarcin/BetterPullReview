#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/app/native_filesystem_path.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/release_maintenance_panel.hpp"
#include "bpr/qt/settings_window.hpp"

#include <QAbstractItemModel>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>

#include <cstdlib>
#include <fstream>
#include <iostream>

namespace {
	using namespace bpr;
	using command = core::release_maintenance_command;
	void expect(bool aValue, const char* aMessage)
	{
		if (!aValue) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	void until(const std::function<bool()>& aCondition, const char* aMessage)
	{
		QElapsedTimer timer;
		timer.start();
		while (!aCondition() && timer.elapsed() < 15000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}
	auto read(const std::filesystem::path& aPath) -> std::string
	{
		std::ifstream file{ aPath, std::ios::binary };
		return { std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{ } };
	}
	void choose(qt::release_maintenance_panel& aPanel, const QString& aDestination)
	{
		aPanel.action(command::export_diagnostics)->trigger();
		QFileDialog* dialog = nullptr;
		for (auto* candidate : aPanel.findChildren<QFileDialog*>(QStringLiteral("releaseMaintenanceSaveDialog"))) {
			if (candidate->isVisible()) {
				dialog = candidate;
			}
		}
		expect(dialog && dialog->isVisible(), "public Export action opens save selection");
		dialog->setDirectory(QFileInfo{ aDestination }.absolutePath());
		auto* filename = dialog->findChild<QLineEdit*>(QStringLiteral("fileNameEdit"));
		expect(filename, "deterministic save chooser exposes its filename field");
		filename->setText(QFileInfo{ aDestination }.fileName());
		expect(dialog->selectedFiles() == QStringList{ aDestination }, "save chooser retains the exact selected filename");
		static_cast<QDialog*>(dialog)->accept();
	}
}

auto main(int aArgc, char** aArgv) -> int
{
	QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	QTemporaryDir temporary{ qEnvironmentVariable("QUALIFICATION_TARGET_ROOT", QDir::tempPath()) + QStringLiteral("/maintenance-XXXXXX") };
	expect(temporary.isValid(), "disposable app root exists");
	const auto parent = app::native_filesystem_path(QFileInfo{ temporary.path() }.canonicalFilePath());
	const auto root   = parent / "data";
	expect(adapters::sqlite::create_storage_root_owner_private(root), "private application root");
	app::detail::github_public_path_fixture_options options;
	options.ApplicationDataRoot      = root;
	app::github_application* owner   = nullptr;
	auto                     desktop = app::detail::github_application_fixture_access::start_desktop(
		app::detail::github_public_path_fixture::profile_window_restoration_v1, options,
		[&](auto aResult) { if (aResult) { owner = *aResult; } });
	until([&] { return owner && !owner->windows().empty(); }, "real profile desktop starts");
	const auto first  = owner->windows().front();
	auto*      window = owner->window(first);
	expect(window, "real workbench available");
	window->show();
	window->open_settings(qt::settings_page::storage_and_privacy);
	auto* settings = window->settings();
	expect(settings && settings->isVisible(), "Storage and privacy is reachable in real Settings");
	auto* panel = dynamic_cast<qt::release_maintenance_panel*>(settings->findChild<QWidget*>(QStringLiteral("releaseMaintenancePanel")));
	expect(panel, "real composition selects maintenance panel");
	until([&] { return panel->action(command::preview_diagnostics)->isEnabled(); }, "native diagnostic capability is admitted independently of sign-in");
	expect(!panel->action(command::prepare_clear)->isEnabled() && !panel->action(command::prepare_purge)->isEnabled(), "missing production lifecycle keeps destructive commands unavailable");
	unsigned                             resets = 0U;
	std::vector<QMetaObject::Connection> observers;
	for (auto* model : window->findChildren<QAbstractItemModel*>()) {
		observers.push_back(QObject::connect(model, &QAbstractItemModel::modelReset, window, [&] { ++resets; }));
	}
	auto* button = panel->findChild<QPushButton*>(QStringLiteral("releaseMaintenanceCommand0"));
	button->setFocus();
	QKeyEvent press{ QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier };
	QKeyEvent release{ QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier };
	QApplication::sendEvent(button, &press);
	QApplication::sendEvent(button, &release);
	until([&] { return panel->action(command::export_diagnostics)->isEnabled(); }, "keyboard preview reaches native host and accepted projection");
	auto* preview = panel->findChild<QPlainTextEdit*>(QStringLiteral("releaseMaintenancePreview"));
	expect(preview && preview->isReadOnly(), "exact export text is selectable read-only preview");
	const auto bytes = preview->toPlainText().toStdString();
	expect(bytes.starts_with("bpr-diagnostics-v1\n") && bytes.find("request=") == std::string::npos && bytes.find("detail=") == std::string::npos, "actual log is reduced to metadata");
	settings->resize(960, 680);
	QApplication::processEvents();
	const auto capture = qEnvironmentVariable("BPR_MAINTENANCE_PUBLIC_RENDER");
	if (!capture.isEmpty()) {
		expect(settings->grab().save(capture), "actual composed Settings screenshot");
	}
	const auto destination = QFileInfo{ temporary.path() }.canonicalFilePath() + QStringLiteral("/diagnostic-é.txt");
	choose(*panel, destination);
	auto* status = panel->findChild<QLabel*>(QStringLiteral("releaseMaintenanceStatus"));
	until([&] { return status->text().contains(QStringLiteral("saved locally")); }, "native export terminal returns to actual Settings");
	const auto exported = read(app::native_filesystem_path(destination));
	if (exported != bytes) {
		std::cerr << "diagnostic-export-mismatch: preview-bytes=" << bytes.size() << " exported-bytes=" << exported.size()
				  << " destination-exists=" << QFileInfo::exists(destination) << '\n';
	}
	expect(exported == bytes, "export bytes equal exact preview including non-ASCII save path");
	panel->action(command::preview_diagnostics)->trigger();
	until([&] { return panel->action(command::export_diagnostics)->isEnabled(); }, "new explicit preview is required for another export");
	choose(*panel, destination);
	until([&] { return status->text().contains(QStringLiteral("export was refused")); }, "existing destination is refused with actionable local-file guidance");
	expect(read(app::native_filesystem_path(destination)) == bytes, "failed export cannot overwrite an existing destination");
	expect(resets == 0U, "ordinary maintenance publications reset no workbench model");
	for (const auto& observer : observers) {
		QObject::disconnect(observer);
	}
	panel->action(command::preview_diagnostics)->trigger();
	settings->close();
	until([&] { return status->text().contains(QStringLiteral("Cancelled")); }, "closing Settings cancels pending preview through exact identity");
	auto* newWindow = window->findChild<QAction*>(QStringLiteral("newBprWindowAction"));
	expect(newWindow && newWindow->isEnabled(), "New Window remains available after diagnostics");
	newWindow->trigger();
	until([&] { return owner->windows().size() == 2U; }, "same profile admits another real window");
	const auto contexts = owner->windows();
	const auto second   = contexts.front() == first ? contexts.back() : contexts.front();
	auto*      sibling  = owner->window(second);
	expect(sibling, "second workbench is available");
	sibling->open_settings(qt::settings_page::storage_and_privacy);
	expect(sibling->settings() == settings && panel->parentWidget() == settings->page_widget(qt::settings_page::storage_and_privacy) && panel->isVisible(), "profile panel follows the shared Settings shell to a new window owner");
	panel->action(command::preview_diagnostics)->trigger();
	until([&] { return panel->action(command::export_diagnostics)->isEnabled(); }, "transferred panel still reaches the profile host");
	window->open_settings(qt::settings_page::storage_and_privacy);
	expect(panel->parentWidget() == settings->page_widget(qt::settings_page::storage_and_privacy) && panel->isVisible(), "returning Settings ownership does not lose or duplicate the panel");
	expect(settings->findChildren<QWidget*>(QStringLiteral("releaseMaintenancePanel")).size() == 1, "shared Settings has one maintenance surface");
	desktop->quit();
	until([&] { return desktop->is_quiescent(); }, "desktop drains maintenance before releasing root and workers");
	expect(desktop->exit_code() == 0, "orderly real desktop shutdown succeeds");
	desktop.reset();
	std::cout << "release-maintenance-public-path:pass real-settings signed-out-preview exact-local-export exclusive-destination no-model-reset shared-settings-owner cancel-shutdown\n";
	return EXIT_SUCCESS;
}
