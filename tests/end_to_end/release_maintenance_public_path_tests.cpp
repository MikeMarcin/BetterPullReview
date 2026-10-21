#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/diagnostic_files.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/insights_storage.hpp"
#include "bpr/adapters/sqlite/profile_runtime_lease.hpp"
#include "bpr/adapters/sqlite/release_maintenance_files.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/app/diagnostic_export_directory.hpp"
#include "bpr/app/native_filesystem_path.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/core/review_cycle_zip.hpp"
#include "bpr/logging/quill_log.hpp"
#include "bpr/qt/desktop_menu_bar.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/release_maintenance_panel.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "private_data_root.hpp"

#include <QAbstractItemModel>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

namespace {
	using namespace bpr;
	using command = core::release_maintenance_command;
	using bpr::tests::expect;
	void until(const std::function<bool()>& aCondition, const char* aMessage, const std::function<void()>& aReport = { })
	{
		QElapsedTimer timer;
		timer.start();
		while (!aCondition() && timer.elapsed() < 15000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		if (!aCondition() && aReport) {
			aReport();
		}
		expect(aCondition(), aMessage);
	}
	auto read(const std::filesystem::path& aPath) -> std::string
	{
		std::ifstream file{ aPath, std::ios::binary };
		return { std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{ } };
	}
	/// \brief [[BPR-PENDING-AG]]. The text of a saved export: its archive's
	/// one entry, named for the file with `.txt`; empty otherwise.
	auto unzipped(const QString& aArchive) -> std::string
	{
		const auto bytes   = read(app::native_filesystem_path(aArchive));
		const auto members = core::read_cycle_zip({ reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size() });
		const auto entry   = QFileInfo{ aArchive }.completeBaseName() + QStringLiteral(".txt");
		return members && members->size() == 1U && members->front().Name == entry.toStdString() ? members->front().Bytes : std::string{ };
	}
	/// \brief [[BPR-PENDING-AG]]. The platform's own archive tools accept the
	/// saved file: Info-ZIP's `unzip` tests every CRC and extracts the entry,
	/// and on macOS `ditto -x -k`, Archive Utility's extractor, does too.
	void native_archive_check(const QString& aArchive, const std::string& aText, const QString& aScratch)
	{
		const auto run = [](const QString& aProgram, const QStringList& aArguments) {
			QProcess process;
			process.start(aProgram, aArguments);
			expect(process.waitForFinished(30000) && process.exitStatus() == QProcess::NormalExit, "the archive tool finishes");
			return std::pair{ process.exitCode(), process.readAllStandardOutput() };
		};
		const auto entry = QFileInfo{ aArchive }.completeBaseName() + QStringLiteral(".txt");
		unsigned   tools = 0U;
		if (const auto unzip = QStandardPaths::findExecutable(QStringLiteral("unzip")); !unzip.isEmpty()) {
			++tools;
			const auto [tested, report] = run(unzip, { QStringLiteral("-t"), aArchive });
			expect(tested == 0 && report.contains("No errors detected"), "unzip -t finds the archive and its CRC sound");
			const auto [listed, listing] = run(unzip, { QStringLiteral("-Z1"), aArchive });
			expect(listed == 0 && QString::fromUtf8(listing).trimmed() == entry, "unzip lists exactly the one .txt entry under its UTF-8 name");
			const auto [extracted, text] = run(unzip, { QStringLiteral("-p"), aArchive });
			expect(extracted == 0 && text.toStdString() == aText, "unzip extracts exactly the previewed text");
		}
#if defined(__APPLE__)
		if (const auto ditto = QStandardPaths::findExecutable(QStringLiteral("ditto")); !ditto.isEmpty()) {
			++tools;
			const auto into = aScratch + QStringLiteral("/ditto-") + QString::number(QDateTime::currentMSecsSinceEpoch());
			expect(run(ditto, { QStringLiteral("-x"), QStringLiteral("-k"), aArchive, into }).first == 0, "ditto -x -k, as Archive Utility, extracts the archive");
			expect(QDir{ into }.entryList(QDir::AllEntries | QDir::NoDotAndDotDot) == QStringList{ entry } && read(app::native_filesystem_path(into + QStringLiteral("/") + entry)) == aText,
				   "into exactly the one .txt file holding the previewed text");
			const auto mode = std::filesystem::status(app::native_filesystem_path(into + QStringLiteral("/") + entry)).permissions();
			expect((mode & std::filesystem::perms::all) == (std::filesystem::perms::owner_read | std::filesystem::perms::owner_write), "the extracted text is owner-only, as the saved archive is");
		}
		expect(tools == 2U, "both macOS archive tools are present to check the archive");
#else
		static_cast<void>(aScratch);
		static_cast<void>(tools);
#endif
	}
	// [[BPR-1615]]. Each private Insights family is populated before the real
	// frozen-preview/export course; an empty store cannot prove privacy.
	void seed_insights(const std::filesystem::path& aRoot)
	{
		auto                        store = adapters::sqlite::make_insights_storage(aRoot, [] {
			return std::vector<application::insights_storage_row>{ { "BPR_INSIGHTS_REPOSITORY_61B9", 6789, 65536, true } };
		});
		const auto                  now   = QDateTime::currentSecsSinceEpoch();
		core::insights_review_event review;
		review.Kind         = core::insights_review_kind::submitted;
		review.Host         = "BPR_INSIGHTS_ACCOUNT_84D2";
		review.Repository   = "BPR_INSIGHTS_REPOSITORY_61B9";
		review.Identity     = "BPR_INSIGHTS_BODY_93C7";
		review.Head         = "BPR_INSIGHTS_HEAD_2AE6";
		review.PullRequest  = 794613852U;
		review.FilesChanged = 17;
		review.UnixSeconds  = now;
		review.LocalDay     = application::local_insights_day(now);
		store->record_reviews({ review });
		expect(store->reviews(now).MostReviewedRepository == review.Repository, "identifying review canaries actually populate the private store");
		core::insights_work_sample work;
		work.At                   = now;
		work.Kind                 = core::insights_work_kind::diff;
		work.DurationMicroseconds = 250000;
		work.Path                 = "BPR_INSIGHTS_PATH_05F8/private.cpp";
		store->record_work({ work });
		const auto workSnapshot = store->work(now);
		expect(workSnapshot.Ranges[0].Slowest.size() == 1 && workSnapshot.Ranges[0].Slowest.front().Path == work.Path, "the private Work path reaches the snapshot");
		core::insights_event request;
		request.Scope            = { 231759846U, 879246013U };
		request.UnixSeconds      = now;
		request.Limit            = 5000;
		request.Remaining        = 4321;
		request.ResetUnixSeconds = now + 1800;
		store->record({ request }, request.Scope);
		const auto githubSnapshot = store->github(now);
		expect(githubSnapshot.Scope == request.Scope && githubSnapshot.Budgets[0].OwnRequests == 1, "numeric account identity and budget canaries reach GitHub history");
		core::insights_energy_sample energy;
		energy.AtMicroseconds         = now * 1000000;
		energy.LocalDay               = review.LocalDay;
		energy.Samples                = 1;
		energy.WallMicroseconds       = 60000000;
		energy.CpuMicroseconds        = 12345;
		energy.Wakeups                = 3;
		energy.WakeupWallMicroseconds = energy.WallMicroseconds;
		store->record_energy({ energy });
		expect(store->energy(now).Ranges[0][0].CpuMicroseconds == 12345, "Energy durations and wakeups reach their private family");
		expect(adapters::sqlite::store_insights_repository_label(aRoot, std::string(64, 'c'), review.Repository), "private cache display label is stored");
		expect(store->measure().TotalBytes > 0, "measured byte totals exist before export");
		const auto persisted = read(aRoot / "state/v1/insights.sqlite3") + read(aRoot / "state/v1/insights.sqlite3-wal");
		for (const auto canary : { "BPR_INSIGHTS_ACCOUNT_84D2", "BPR_INSIGHTS_REPOSITORY_61B9", "BPR_INSIGHTS_BODY_93C7", "BPR_INSIGHTS_HEAD_2AE6", "BPR_INSIGHTS_PATH_05F8" }) {
			expect(persisted.find(canary) != std::string::npos, "every identifying text canary is present in the selected persisted fixture store");
		}
	}

	void check_insights_export(const std::string& aBytes, bool aAnonymized)
	{
		for (const auto canary : { "BPR_INSIGHTS_", "794613852", "231759846", "879246013" }) {
			expect(aBytes.find(canary) == std::string::npos, "all export sections exclude private Insights repository, account, PR, path and body canaries");
		}
		unsigned sections = 0;
		for (const auto& line : QByteArray::fromStdString(aBytes).split('\n')) {
			const auto object = QJsonDocument::fromJson(line).object();
			if (object.value(QStringLiteral("section")).toString() != QStringLiteral("insights")) {
				continue;
			}
			++sections;
			expect(object.size() == 3 && object.value(QStringLiteral("local_store_bytes")).isDouble()
					   && object.value(QStringLiteral("git_cache_bytes")).isDouble(),
				   "the actual Insights section admits exactly its two numeric byte fields");
		}
		expect(sections == (aAnonymized ? 1U : 0U), "anonymized export includes one Insights section; identifying export adds none");
	}

	auto save_dialog(qt::release_maintenance_panel& aPanel) -> QFileDialog*
	{
		for (auto* candidate : aPanel.findChildren<QFileDialog*>(QStringLiteral("releaseMaintenanceSaveDialog"))) {
			if (candidate->isVisible()) {
				return candidate;
			}
		}
		return nullptr;
	}
	auto open_save(qt::release_maintenance_panel& aPanel) -> QFileDialog*
	{
		aPanel.action(command::export_diagnostics)->trigger();
		until([&] { return save_dialog(aPanel); }, "effect-lane folder observation returns to the native chooser projection");
		auto*      dialog  = save_dialog(aPanel);
		const auto initial = app::resolve_diagnostic_export_directory(app::diagnostic_export_locations());
		expect(initial.has_value(), "OS standard folder observation is available");
		expect(dialog->directory().absolutePath() == QString::fromStdString(*initial), "composed chooser explicitly starts in admitted OS folder despite cwd/history");
		// [[BPR-1722]]. Named for the local date and time, and the version when there is one.
		expect(QRegularExpression{ QStringLiteral("^BetterPullReview-diagnostics-[0-9]{4}-[0-9]{2}-[0-9]{2}-[0-9]{4}(-[0-9.]+)?\\.zip$") }.match(QFileInfo{ dialog->selectedFiles().front() }.fileName()).hasMatch(),
			   "chooser suggests a dated filename");
		return dialog;
	}
	void controlled_directories(const QString& aRoot)
	{
		namespace fs = std::filesystem;
		std::array<fs::path, 3>      locations;
		const std::array<QString, 3> names{ QStringLiteral("Téléchargements déplacés"), QStringLiteral("文書"), QStringLiteral("home") };
		for (std::size_t index = 0U; index < locations.size(); ++index) {
			locations[index] = app::native_filesystem_path(aRoot + QStringLiteral("/") + names[index]);
			expect(fs::create_directory(locations[index]) && adapters::sqlite::make_storage_root_owner_private(locations[index]), "controlled OS-location observation is owner controlled");
		}
		const auto observed = [&] {
			std::vector<std::pair<fs::path, fs::perms>> entries;
			for (const auto& entry : fs::recursive_directory_iterator{ app::native_filesystem_path(aRoot) }) {
				entries.emplace_back(entry.path(), entry.status().permissions());
			}
			std::ranges::sort(entries);
			return entries;
		};
		const auto before = observed();
		expect(app::resolve_diagnostic_export_directory(locations) == application::diagnostic_export_directory{ (aRoot + QStringLiteral("/") + names[0]).toStdString() }, "redirected localized Downloads is retained verbatim");
		locations[0] = app::native_filesystem_path(aRoot + QStringLiteral("/missing-downloads"));
		expect(app::resolve_diagnostic_export_directory(locations) == application::diagnostic_export_directory{ (aRoot + QStringLiteral("/") + names[1]).toStdString() }, "missing Downloads falls back to localized Documents");
		locations[1].clear();
		expect(app::resolve_diagnostic_export_directory(locations) == application::diagnostic_export_directory{ (aRoot + QStringLiteral("/") + names[2]).toStdString() }, "empty Documents falls back to home");
#if !defined(_WIN32)
		const auto home = locations[2];
		fs::permissions(home, fs::perms::owner_read | fs::perms::owner_exec);
		expect(!app::resolve_diagnostic_export_directory(locations), "unwritable home supplies typed total unavailability");
		fs::permissions(home, fs::perms::owner_all);
		fs::permissions(home, fs::perms::owner_all | fs::perms::others_write);
		expect(!app::resolve_diagnostic_export_directory(locations), "directory refused by output policy cannot be a default");
		fs::permissions(home, fs::perms::owner_all);
#endif
		locations[2] = app::native_filesystem_path(aRoot + QStringLiteral("/missing-home"));
		expect(!app::resolve_diagnostic_export_directory(locations), "complete absence returns no arbitrary location");
		locations = { fs::path{ }, fs::path{ "relative" }, fs::current_path().root_path() };
		expect(!app::resolve_diagnostic_export_directory(locations), "empty, relative and volume-root locations never substitute for user folders");
		expect(observed() == before, "discovery creates no file/directory and changes no permissions");
	}
	void choose(qt::release_maintenance_panel& aPanel, const QString& aDestination, bool aTrigger = true)
	{
		QFileDialog* dialog = aTrigger ? open_save(aPanel) : save_dialog(aPanel);
		expect(dialog && dialog->isVisible(), "public Export action opens save selection");
		dialog->setDirectory(QFileInfo{ aDestination }.absolutePath());
		auto* filename = dialog->findChild<QLineEdit*>(QStringLiteral("fileNameEdit"));
		expect(filename, "deterministic save chooser exposes its filename field");
		filename->setText(QFileInfo{ aDestination }.fileName());
		expect(dialog->selectedFiles() == QStringList{ aDestination }, "save chooser retains the exact selected filename");
		static_cast<QDialog*>(dialog)->accept();
	}
	void confirm(qt::release_maintenance_panel& aPanel, command aCommand)
	{
		aPanel.action(aCommand)->trigger();
		auto* dialog = aPanel.findChild<QMessageBox*>(QStringLiteral("releaseMaintenanceConfirmation"));
		expect(dialog && dialog->isVisible(), "public destructive action requires an explicit confirmation");
		for (auto* button : dialog->buttons()) {
			if (dialog->buttonRole(button) == QMessageBox::DestructiveRole) {
				button->click();
				return;
			}
		}
		expect(false, "destructive confirmation button exists");
	}
}

auto main(int aArgc, char** aArgv) -> int
try {
	const bool attended = aArgc > 1 && std::string_view{ aArgv[1] } == "--attended";
	if (!attended) {
		QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
	}
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	QTemporaryDir temporary{ qEnvironmentVariable("QUALIFICATION_TARGET_ROOT", QDir::tempPath()) + QStringLiteral("/maintenance-XXXXXX") };
	expect(temporary.isValid(), "disposable app root exists");
	// [[BPR-1050]]. One owned level down, so the agent workspace neighbourhood
	// beside the root has a private parent.
	const auto root = e2e::private_data_root(temporary).value_or(std::filesystem::path{ });
	expect(!root.empty(), "private application root");
	seed_insights(root);
	controlled_directories(QFileInfo{ temporary.path() }.canonicalFilePath());
	const auto standardLocations = app::diagnostic_export_locations();
	expect(standardLocations[0] == app::native_filesystem_path(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation))
			   && standardLocations[1] == app::native_filesystem_path(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
			   && standardLocations[2] == app::native_filesystem_path(QStandardPaths::writableLocation(QStandardPaths::HomeLocation)),
		   "production discovery asks OS standard Downloads, Documents and home in order");
	expect(QDir::setCurrent(QDir::rootPath()), "composed export starts with disk-root working directory");
	{
		application::diagnostic_preferences preferences;
		preferences.Privacy = application::diagnostic_privacy::include_locally;
		adapters::logging::quill_log  logger{ root, { }, preferences };
		application::diagnostic_event event{ .Operation = application::diagnostic_operation::capability, .Outcome = application::diagnostic_outcome::failed, .Request = 998877U, .WindowSession = 887766U, .HttpStatus = 403U };
		constexpr std::string_view    explanation = "message=\"Unknown schema field fields\" code=\"[omitted:unclassified-code]\" ";
		std::copy(explanation.begin(), explanation.end(), event.Summary.Text.begin());
		event.Summary.Size  = static_cast<std::uint16_t>(explanation.size());
		event.Summary.State = application::diagnostic_summary_state::sanitized;
		expect(logger.record(event) == application::diagnostic_log_status::available, "retained session records classified identifying facts");
		expect(logger.flush_for_export(false).has_value(), "identifying record flushes before privacy barrier");
		preferences.Privacy = application::diagnostic_privacy::anonymized;
		expect(logger.configure(preferences), "retained session changes capture privacy");
		expect(logger.record(event) == application::diagnostic_log_status::available, "same session records anonymized facts");
		logger.close();
	}
	constexpr auto hostileLegacyLines = std::string_view{ "2026-09-21T01:00:00Z v=1 op=application outcome=failed request=99 http_status=0 summary=sanitized dropped=0 detail=BPR_CANARY_SECRET_1 BPR_CANARY_SOURCE_1 BPR_CANARY_COMMENT_1\nlegacy BPR_CANARY_SOURCE_2\n" };
#if defined(_WIN32)
	// [[BPR-1030]]. On Windows the running logger is the log's only writer: it
	// holds diagnostics.log shared for reading alone, so an append beside it is
	// refused. The legacy lines go in before the desktop starts, into a log the
	// product's own logger created with its own descriptor.
	{
		adapters::logging::quill_log created{ root };
		expect(created.open_failure() == adapters::logging::diagnostic_file_failure::none, "the product logger creates the native log");
		created.close();
		std::filesystem::rename(tests::diagnostic_files(root).front(), root / "logs/diagnostics.log");
	}
	{
		std::ofstream hostile{ root / "logs/diagnostics.log", std::ios::app | std::ios::binary };
		hostile << hostileLegacyLines;
		expect(hostile.good(), "hostile legacy diagnostics are written while no logger holds the log");
	}
#endif
	app::detail::github_public_path_fixture_options options;
	options.ApplicationDataRoot      = root;
	options.RegisteredDeviceCourse   = true;
	options.IndependentWindowReads   = true;
	options.RepeatReadSessions       = true;
	app::github_application* owner   = nullptr;
	auto                     desktop = app::detail::github_application_fixture_access::start_desktop(
		app::detail::github_public_path_fixture::profile_window_restoration_v1, options,
		[&](auto aResult) { if (aResult) { owner = *aResult; } });
	until([&] { return owner && !owner->windows().empty(); }, "real profile desktop starts");
	const auto first  = owner->windows().front();
	auto*      window = owner->window(first);
	expect(window, "real workbench available");
	window->show();
	if (attended) {
		std::cout << "DIAGNOSTIC_EXPORT_ATTENDED_READY signed-out synthetic-profile cwd=root native-dialogs=enabled\n"
				  << std::flush;
		QTimer shutdown;
		QObject::connect(&shutdown, &QTimer::timeout, &application, [&] {
			if (desktop->is_quiescent()) {
				QApplication::quit();
			}
		});
		shutdown.start(100);
		const auto result = QApplication::exec();
		desktop->quit();
		until([&] { return desktop->is_quiescent(); }, "attended desktop drains native maintenance");
		desktop.reset();
		expect(e2e::remove_course_root(temporary), "attended synthetic profile removed");
		return result;
	}
	const auto helpExport = [&](const char* aName) {
		auto* action = window->findChild<QAction*>(QString::fromUtf8(aName));
		expect(action && action->isEnabled(), "Help export action is available while signed out");
		action->trigger();
	};
	helpExport("exportAnonymizedDiagnosticsAction");
	QDialog* directDialog = nullptr;
	for (auto* widget : QApplication::topLevelWidgets()) {
		if (widget->objectName() == QStringLiteral("diagnosticExportDialog")) {
			directDialog = qobject_cast<QDialog*>(widget);
		}
	}
	expect(directDialog && directDialog->isVisible(), "Help opens export without navigating Settings");
	auto* directPanel = dynamic_cast<qt::release_maintenance_panel*>(directDialog->parentWidget());
	expect(directPanel && !directPanel->window()->isVisible(), "Settings stays closed for direct Help export");
	until([&] { return directPanel->action(command::export_diagnostics)->isEnabled(); }, "first Help export waits for native startup and prepares exact preview");
	const auto helpFrozen = directPanel->findChild<QPlainTextEdit*>(QStringLiteral("releaseMaintenancePreview"))->toPlainText();
	check_insights_export(helpFrozen.toStdString(), true);
	open_save(*directPanel)->reject();
	QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
	expect(directPanel->findChild<QPlainTextEdit*>(QStringLiteral("releaseMaintenancePreview"))->toPlainText() == helpFrozen && directPanel->action(command::export_diagnostics)->isEnabled(), "signed-out Help cancellation retains exact frozen preview");
	directDialog->reject();
	until([&] { return !directPanel->action(command::export_diagnostics)->isEnabled(); }, "Cancel discards the direct export preview");
	window->open_settings(qt::settings_page::storage_and_privacy);
	auto* settings = window->settings();
	expect(settings && settings->isVisible(), "Storage and privacy is reachable in real Settings");
	auto* panel = dynamic_cast<qt::release_maintenance_panel*>(settings->findChild<QWidget*>(QStringLiteral("releaseMaintenancePanel")));
	expect(panel, "real composition selects maintenance panel");
	until([&] { return panel->action(command::preview_diagnostics)->isEnabled(); }, "native diagnostic capability is admitted independently of sign-in");
	expect(panel->action(command::prepare_clear)->isEnabled() && panel->action(command::prepare_purge)->isEnabled(), "desktop composes the retirement and native cleanup capabilities");
	unsigned                             resets = 0U;
	std::vector<QMetaObject::Connection> observers;
	for (auto* model : window->findChildren<QAbstractItemModel*>()) {
		observers.push_back(QObject::connect(model, &QAbstractItemModel::modelReset, window, [&] { ++resets; }));
	}
#if !defined(_WIN32)
	{
		std::ofstream hostile{ root / "logs/diagnostics.log", std::ios::app };
		hostile << hostileLegacyLines;
		hostile.close();
		std::filesystem::permissions(root / "logs/diagnostics.log", std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, std::filesystem::perm_options::replace);
	}
#endif
	expect(read(root / "logs/diagnostics.log").find("BPR_CANARY_SOURCE_2") != std::string::npos, "hostile legacy diagnostics actually reach the selected native log");
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
	check_insights_export(bytes, true);
	expect(bytes.starts_with("{\"format\":\"bpr-diagnostics-v2\"") && bytes.find("998877") == std::string::npos && bytes.find("887766") == std::string::npos, "actual mixed-mode log exports with fresh anonymized identities");
	expect(bytes.find("Unknown schema field fields") != std::string::npos, "useful safe explanation survives the public preview");
	const auto originals        = tests::read_diagnostic_logs(root);
	auto*      cancelledChooser = open_save(*panel);
	cancelledChooser->setDirectory(temporary.path());
	cancelledChooser->reject();
	QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
	expect(preview->toPlainText().toStdString() == bytes, "alternate chooser navigation and cancel preserve exact preview");
	// Appends after preparation must not change the confirmed export.
	{
		std::ofstream append{ root / "logs/diagnostics.log", std::ios::app };
		append << hostileLegacyLines;
	}
	expect(!settings->grab().isNull(), "full composed Settings preview renders");
	expect(bytes.find("BPR_CANARY_") == std::string::npos && bytes.find("legacy BPR") == std::string::npos, "public preview excludes secret, source, comment and malformed legacy bodies");
	settings->resize(960, 680);
	QApplication::processEvents();
	const auto capture = qEnvironmentVariable("BPR_MAINTENANCE_PUBLIC_RENDER");
	if (!capture.isEmpty()) {
		expect(settings->grab().save(capture), "actual composed Settings screenshot");
		expect(directDialog->grab().save(capture + QStringLiteral("-export.png")), "actual composed export screenshot");
	}
	const auto destination = QFileInfo{ temporary.path() }.canonicalFilePath() + QStringLiteral("/diagnostic-é.zip");
	choose(*panel, destination);
	auto* status = panel->findChild<QLabel*>(QStringLiteral("releaseMaintenanceStatus"));
	until([&] { return status->text().contains(QStringLiteral("saved locally")); }, "native export terminal returns to actual Settings");
	// [[BPR-PENDING-AG]]. The saved file is a ZIP archive whose one entry,
	// diagnostic-é.txt, is the frozen preview; the preview still shows text.
	const auto exported = unzipped(destination);
	if (exported != bytes) {
		std::cerr << "diagnostic-export-mismatch: preview-bytes=" << bytes.size() << " exported-bytes=" << exported.size()
				  << " destination-exists=" << QFileInfo::exists(destination) << '\n';
	}
	expect(exported == bytes, "the archive's entry equals the exact frozen preview despite later append and non-ASCII save path");
	native_archive_check(destination, bytes, QFileInfo{ temporary.path() }.canonicalFilePath());
	const auto savedArchive = read(app::native_filesystem_path(destination));
	panel->action(command::preview_diagnostics)->trigger();
	until([&] { return panel->action(command::export_diagnostics)->isEnabled(); }, "fresh preview prepares stale chooser course");
	QPointer<QFileDialog> staleChooser     = open_save(*panel);
	const auto            staleDestination = QFileInfo{ temporary.path() }.canonicalFilePath() + QStringLiteral("/stale.zip");
	staleChooser->setDirectory(temporary.path());
	staleChooser->findChild<QLineEdit*>(QStringLiteral("fileNameEdit"))->setText(QStringLiteral("stale.zip"));
	directDialog->reject();
	until([&] { return !staleChooser || !staleChooser->isVisible(); }, "preview cancellation dismisses the exact stale chooser after its serialized terminal");
	if (staleChooser) {
		static_cast<QDialog*>(staleChooser)->accept();
	}
	QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
	expect(!QFileInfo::exists(staleDestination), "late stale chooser acceptance cannot write");
	expect(tests::read_diagnostic_logs(root).size() >= originals.size(), "export leaves original records intact");
	panel->action(command::preview_diagnostics)->trigger();
	until([&] { return panel->action(command::export_diagnostics)->isEnabled(); }, "new explicit preview is required for another export");
	choose(*panel, destination);
	until([&] { return status->text().contains(QStringLiteral("export was refused")); }, "existing destination is refused with actionable local-file guidance");
	expect(read(app::native_filesystem_path(destination)) == savedArchive, "failed export cannot overwrite an existing destination");
	auto* scope       = panel->findChild<QComboBox*>(QStringLiteral("diagnosticExportScope"));
	auto* recent      = panel->findChild<QCheckBox*>(QStringLiteral("diagnosticExportRecent"));
	auto* identifying = panel->findChild<QPushButton*>(QStringLiteral("diagnosticIdentifyingPreview"));
	expect(scope && recent && identifying && !recent->isChecked(), "explicit session and recent-context controls default safely");
	identifying->click();
	until([&] { return panel->action(command::export_diagnostics)->isEnabled(); }, "identifying preview prepared separately");
	const auto identifyingBytes = preview->toPlainText().toStdString();
	check_insights_export(identifyingBytes, false);
	open_save(*panel)->reject();
	QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
	expect(identifyingBytes.find("\"request\":998877") != std::string::npos && identifyingBytes.find("\"request\":0") != std::string::npos, "identifying preview retains captured identifiers without reversing capture redaction");
	expect(identifyingBytes.find("BPR_CANARY_") == std::string::npos, "identifying mode cannot release forbidden content");
	auto* disclosure    = panel->findChild<QDialog*>(QStringLiteral("diagnosticExportDialog"));
	auto* consentText   = panel->findChild<QLabel*>(QStringLiteral("diagnosticExportDisclosure"));
	auto* dialogButtons = disclosure->findChild<QDialogButtonBox*>();
	expect(disclosure->isVisible() && consentText->text().contains(QStringLiteral("cannot be recovered")) && dialogButtons->button(QDialogButtonBox::Cancel)->isDefault(), "identifying preview discloses capture omissions and defaults to Cancel");
	dialogButtons->button(QDialogButtonBox::Cancel)->click();
	QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
	expect(!panel->action(command::export_diagnostics)->isEnabled(), "cancel identifying preview grants no export");
	helpExport("exportIdentifyingDiagnosticsAction");
	until([&] { return panel->action(command::export_diagnostics)->isEnabled(); }, "Help prepares a fresh identifying preview");
	const auto confirmedBytes = preview->toPlainText().toStdString();
	check_insights_export(confirmedBytes, false);
	open_save(*panel);
	const auto identifyingDestination = QFileInfo{ temporary.path() }.canonicalFilePath() + QStringLiteral("/identifying.zip");
	choose(*panel, identifyingDestination, false);
	until([&] { return status->text().contains(QStringLiteral("saved locally")); }, "confirmed identifying write completed");
	expect(unzipped(identifyingDestination) == confirmedBytes, "confirmed identifying output matches exact preview");
	scope->setCurrentIndex(1);
	panel->action(command::preview_diagnostics)->trigger();
	until([&] { return panel->action(command::export_diagnostics)->isEnabled(); }, "current-session preview prepared");
	expect(!preview->toPlainText().contains(QStringLiteral("Unknown schema field fields")), "current session excludes retained prior session");
	scope->setCurrentIndex(0);
	expect(!panel->action(command::export_diagnostics)->isEnabled(), "changing scope invalidates preview before export");
	expect(resets == 0U, "ordinary maintenance publications reset no workbench model");
	for (const auto& observer : observers) {
		QObject::disconnect(observer);
	}
	panel->action(command::preview_diagnostics)->trigger();
	settings->close();
	until([&] { return status->text().contains(QStringLiteral("Cancelled")); }, "closing Settings cancels pending preview through exact identity");
	window->home_landing()->primary_button()->click();
	until([&] { return window->session_panel()->status() == core::github_status::session_ready; }, "public sign-in reaches the fixture account");
	window->home_open_bar()->setText(QStringLiteral("https://github.com/octo-org/review-repository/pull/17"));
	window->home_open_action()->trigger();
	until([&] { return owner->status(first)->EpochStatus == core::epoch_status::ready; }, "real Git producer publishes an exact epoch");
	window->modality()->action(qt::review_modality::review)->trigger();
	auto* files = window->files_drawer()->contents_view();
	auto* model = window->files_drawer()->contents();
	until([&] { return model->index_for_file(core::file_identity_of("review.cpp")).isValid(); }, "exact fixture file is selectable");
	files->selectionModel()->setCurrentIndex(model->index_for_file(core::file_identity_of("review.cpp")), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
	auto* comparison = window->comparison_surface();
	until([&] { return comparison->current_result() && comparison->current_semantic_index() && !comparison->geometry_pending() && comparison->isVisible(); }, "selected fixture diff reaches the real workbench");
	expect(comparison->current_result()->Alignment.Key.File == core::file_identity_of("review.cpp") && comparison->link_map(), "one accepted alignment drives the comparison and DiffLinkMap");
	window->open_settings(qt::settings_page::storage_and_privacy);
	recent->setChecked(true);
	panel->action(command::preview_diagnostics)->trigger();
	until([&] { return panel->action(command::export_diagnostics)->isEnabled(); }, "live Standard ring is explicitly selected through UI");
	const auto ringPreview = preview->toPlainText().toStdString();
	expect(ringPreview.find("\"source\":\"recent_context\"") != std::string::npos, "unpersisted intermediate context reaches selected public export");
	const auto ringDestination = QFileInfo{ temporary.path() }.canonicalFilePath() + QStringLiteral("/recent-context.zip");
	choose(*panel, ringDestination);
	until([&] { return status->text().contains(QStringLiteral("saved locally")); }, "selected ring export saves locally");
	expect(unzipped(ringDestination) == ringPreview, "ring save equals frozen preview");
	settings->close();
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
	const auto lifetime = owner->liveness();
	confirm(*panel, command::prepare_clear);
	until([&] { return panel->action(command::clear_diagnostics)->isEnabled() || status->text().contains(QStringLiteral("failed")) || status->text().contains(QStringLiteral("prevents")); }, "native preparation drains two actual windows and all profile producers");
	if (!panel->action(command::clear_diagnostics)->isEnabled()) {
		std::cerr << status->text().toStdString() << '\n';
	}
	expect(panel->action(command::clear_diagnostics)->isEnabled(), "actual log inventory is prepared after retirement");
	expect(lifetime.expired() && desktop->runtime(core::default_bpr_profile_id) == nullptr, "entire original runtime and every store owner are destroyed before confirmation");
	expect(settings->isVisible() && panel->isVisible(), "maintenance-only Settings survives window retirement");
#if defined(__APPLE__)
	expect(desktop->desktop_menu() && desktop->desktop_menu()->quit_action()->isEnabled(), "maintenance-only Settings retains the native Quit action");
#endif
	expect(!adapters::sqlite::acquire_profile_runtime_lease(root), "maintenance retains exclusive native profile ownership");
	panel->action(command::cancel)->trigger();
	expect(status->text().contains(QStringLiteral("Cancelled")) && desktop->runtime(core::default_bpr_profile_id) == nullptr, "cancel does not restart producers");
	confirm(*panel, command::prepare_clear);
	until([&] { return panel->action(command::clear_diagnostics)->isEnabled(); }, "retired profile can prepare a fresh inventory");
	{
		std::ofstream changed{ root / "logs/diagnostics.log", std::ios::app };
		changed << "changed after preparation\n";
	}
	confirm(*panel, command::clear_diagnostics);
	until([&] { return status->text().contains(QStringLiteral("changed")); }, "real confirmation revalidates the prepared inventory");
	expect(std::filesystem::exists(root / "logs/diagnostics.log"), "changed inventory deletes nothing");
	confirm(*panel, command::prepare_clear);
	until([&] { return panel->action(command::clear_diagnostics)->isEnabled(); }, "explicit retry prepares the changed inventory");
	confirm(*panel, command::clear_diagnostics);
	until([&] { return status->text().contains(QStringLiteral("files were cleared")); }, "real diagnostic deletion reaches a truthful terminal");
	expect(!std::filesystem::exists(root / "logs" / "diagnostics.log") && std::filesystem::exists(root / "state" / "v1" / "review-queue.sqlite3"), "clear removes only diagnostics");
	confirm(*panel, command::prepare_purge);
	// [[BPR-1038]]. A privacy refusal is a returned inventory too, and says so.
	until([&] { return panel->action(command::purge_profile)->isEnabled() || status->text().contains(QStringLiteral("prevents")) || status->text().contains(QStringLiteral("partial")) || status->text().contains(QStringLiteral("not private")); }, "native full-profile inventory returns");
	if (!panel->action(command::purge_profile)->isEnabled()) {
		std::cerr << status->text().toStdString() << '\n';
		for (const auto& entry : std::filesystem::recursive_directory_iterator{ root }) {
			std::cerr << entry.path().lexically_relative(root) << '\n';
		}
	}
	expect(panel->action(command::purge_profile)->isEnabled(), "complete real profile can be purged");
	confirm(*panel, command::purge_profile);
	until(
		[&] { return status->text().contains(QStringLiteral("data was purged")); }, "exact profile purge completes through real Settings",
		[&] {
			// [[BPR-1038]]. A purge that stops says where, as the inventory above does.
			std::cerr << status->text().toStdString() << '\n';
			for (const auto& entry : std::filesystem::recursive_directory_iterator{ root }) {
				std::cerr << entry.path().lexically_relative(root) << '\n';
			}
		});
	expect(!std::filesystem::exists(root / "state" / "v1" / "device-bindings.sqlite3") && !std::filesystem::exists(root / "state" / "v1" / "review-queue.sqlite3"), "private stores and binding metadata are removed");
	expect(read(app::native_filesystem_path(destination)) == savedArchive && unzipped(destination) == bytes, "exported user file survives profile purge");
#if defined(__APPLE__)
	desktop->desktop_menu()->quit_action()->trigger();
#else
	settings->close();
	desktop->quit();
#endif
	until([&] { return desktop->is_quiescent(); }, "desktop drains maintenance before releasing root and workers");
	expect(desktop->exit_code() == 0, "orderly real desktop shutdown succeeds");
	desktop.reset();
	expect(adapters::sqlite::acquire_profile_runtime_lease(root).has_value(), "closing maintenance releases the retained lease");
	{
		std::ofstream journal{ root / ".bpr-maintenance-v1", std::ios::binary };
		journal << "interrupted invalid journal";
	}
	std::filesystem::permissions(root / ".bpr-maintenance-v1", std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
	bool refused = false;
	desktop      = app::detail::github_application_fixture_access::start_desktop(
		app::detail::github_public_path_fixture::profile_window_restoration_v1, options,
		[&](auto aResult) {
			// [[BPR-1121]]. The event loop calls this, and a throw may not cross it, so a failure here exits.
			const bpr::tests::exit_on_failure inTheEventLoop;
			expect(!aResult, "unrecovered profile must never be admitted");
			refused = true;
		});
	until([&] { return refused && desktop->is_quiescent(); }, "startup executes maintenance recovery before any fresh producer");
	expect(!std::filesystem::exists(root / "logs/diagnostics.log") && !std::filesystem::exists(root / "state/v1/review-queue.sqlite3"), "failed recovery opens neither profile stores nor logger");
	expect(read(root / ".bpr-maintenance-v1") == "interrupted invalid journal", "failed recovery preserves evidence without fallback deletion");
	desktop.reset();
	std::cout << "release-maintenance-public-path:pass real-settings signed-out-preview exact-local-export exclusive-destination no-model-reset selected-fixture-diff shared-settings-owner two-window-retirement cancel-no-restart stale-inventory clear purge retained-lease startup-recovery-refusal\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
