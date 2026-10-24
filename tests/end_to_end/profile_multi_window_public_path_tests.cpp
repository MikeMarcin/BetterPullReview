#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/diagnostic_files.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/logging/quill_log.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/conversation_finder.hpp"
#include "bpr/qt/defect_card_view.hpp"
#include "bpr/qt/dif_runtime.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/profile_management_window.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "private_data_root.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMouseEvent>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QThread>
#include <QToolButton>
#include <QTreeView>

#include <sqlite3.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>

#ifdef __APPLE__
	#include <mach/mach.h>
#endif

namespace {
	using namespace bpr;
	using bpr::tests::expect;
	void wait_for(const std::function<bool()>& aCondition, const char* aMessage, const std::function<void()>& aDiagnostic = { }, const int aTimeoutMs = 15000)
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
	auto action(qt::workbench_window& aWindow, const char* aName) -> QAction*
	{
		auto* result = aWindow.findChild<QAction*>(QString::fromUtf8(aName));
		expect(result != nullptr, "public profile action exists");
		wait_for([&] { return result->isEnabled(); }, "public profile action becomes enabled");
		return result;
	}
	auto visible_settings() -> std::size_t
	{
		auto count = std::size_t{ 0U };
		for (auto* widget : QApplication::topLevelWidgets()) {
			if (dynamic_cast<qt::settings_window*>(widget) != nullptr && widget->isVisible()) {
				++count;
			}
		}
		return count;
	}
	auto draft_at_selection(const qt::workbench_window& aWindow) -> bool
	{
		const auto& draft     = aWindow.review_editor()->shown_draft();
		const auto& selection = aWindow.comparison_surface()->selection();
		return draft && selection && draft->Anchor == selection->PreferredLine;
	}
	void capture(qt::workbench_window& aWindow, const QString& aName)
	{
		const auto root = qEnvironmentVariable("BPR_MULTI_WINDOW_ARTIFACT_ROOT");
		if (!root.isEmpty()) {
			expect(QDir{ }.mkpath(root), "multiwindow artifact directory");
			expect(aWindow.grab().save(root + QLatin1Char{ '/' } + aName), "multiwindow rendered artifact");
			if (aName.startsWith(QStringLiteral("defect-"))) {
				expect(aWindow.defect_card()->grab().save(root + QStringLiteral("/card-") + aName), "exact accepted defect card rendered artifact");
			}
		}
	}
	auto memory_sample() -> QJsonObject
	{
#ifdef __APPLE__
		task_vm_info_data_t info{ };
		auto                count = mach_msg_type_number_t{ TASK_VM_INFO_COUNT };
		if (task_info(mach_task_self(), TASK_VM_INFO, reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS) {
			return { { "available", true }, { "resident_bytes", static_cast<qint64>(info.resident_size) }, { "physical_footprint_bytes", static_cast<qint64>(info.phys_footprint) } };
		}
#endif
		return { { "available", false }, { "resident_bytes", QJsonValue::Null }, { "physical_footprint_bytes", QJsonValue::Null } };
	}
	void run_interval(const int aMilliseconds)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (elapsed.elapsed() < aMilliseconds) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
	}
	auto dif_sample(const qt::dif_widget& aWidget) -> QJsonObject
	{
		const auto stats = aWidget.statistics();
		const auto frame = aWidget.current_frame();
		return { { "TimerWakeups", static_cast<qint64>(stats.TimerWakeups) },
				 { "Paints", static_cast<qint64>(stats.Paints) },
				 { "PlaybackStarts", static_cast<qint64>(stats.PlaybackStarts) },
				 { "CompletedCalmLoops", stats.CompletedCalmLoops },
				 { "Greeted", stats.Greeted },
				 { "clip", static_cast<qint64>(frame.Clip) },
				 { "frame", frame.Frame } };
	}
	void measure_dif(app::github_application& aOwner, qt::workbench_window& aWindow, qt::workbench_window& aSibling, QJsonObject& aReport)
	{
		if (qEnvironmentVariableIsEmpty("BPR_DIF_INTEGRATION_REPORT_PATH")) {
			return;
		}
		aWindow.choose_home(true);
		aWindow.open_settings(qt::settings_page::appearance);
		auto* motion = aWindow.appearance_settings()->motion_selector();
		motion->setCurrentIndex(motion->findData(static_cast<int>(core::motion_mode::full)));
		for (auto* widget : QApplication::topLevelWidgets()) {
			if (dynamic_cast<qt::settings_window*>(widget) != nullptr) {
				widget->close();
			}
		}
		aWindow.raise();
		aWindow.activateWindow();
		wait_for([&] { return aWindow.dif()->current_frame().NextMs.has_value(); }, "ordinary Full motion activates Home playback");
		run_interval(200);
		const auto* cache = aWindow.dif()->cache_identity();
		expect(cache != nullptr && cache == aSibling.dif()->cache_identity(), "measurement retains shared production cache");
		aReport["cache"] = QJsonObject{ { "pixel_bytes", static_cast<qint64>(cache->pixel_bytes()) },
										{ "retained_bytes", static_cast<qint64>(cache->retained_bytes()) },
										{ "conversion_peak_bytes", static_cast<qint64>(cache->conversion_peak_bytes()) },
										{ "shared_identity", true },
										{ "identity", "production-catalog-1" } };
		QJsonArray captures;
		const auto captureFrame = [&](const QString& aLabel) {
			const auto name = QStringLiteral("dif-%1-%2.png").arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMddTHHmmsszzzZ")), aLabel);
			capture(aWindow, name);
			if (!qEnvironmentVariableIsEmpty("BPR_MULTI_WINDOW_ARTIFACT_ROOT")) {
				captures.append(QJsonObject{ { "path", QDir(qEnvironmentVariable("BPR_MULTI_WINDOW_ARTIFACT_ROOT")).absoluteFilePath(name) }, { "sample", dif_sample(*aWindow.dif()) } });
			}
		};
		captureFrame(QStringLiteral("full-before"));
		const auto    fullBefore        = dif_sample(*aWindow.dif());
		const auto    siblingFullBefore = dif_sample(*aSibling.dif());
		const auto    diffBefore        = aWindow.comparison_surface()->statistics();
		QElapsedTimer elapsed;
		elapsed.start();
		run_interval(1800);
		const auto fullMs           = elapsed.elapsed();
		const auto fullAfter        = dif_sample(*aWindow.dif());
		const auto siblingFullAfter = dif_sample(*aSibling.dif());
		const auto wakeups          = fullAfter["TimerWakeups"].toInteger() - fullBefore["TimerWakeups"].toInteger();
		expect(wakeups > 0 && wakeups <= fullMs / 84 + 2, "active playback has bounded nonzero timer work");
		expect(siblingFullBefore == siblingFullAfter, "inactive sibling remains static during Full playback");
		const auto diffAfter = aWindow.comparison_surface()->statistics();
		expect(diffAfter.GeometryRebuilds == diffBefore.GeometryRebuilds && diffAfter.Publications == diffBefore.Publications
				   && diffAfter.PaneMeasurements == diffBefore.PaneMeasurements && diffAfter.SemanticIndexAdoptions == diffBefore.SemanticIndexAdoptions,
			   "Home animation interval causes no diff publication, layout or semantic adoption");
		aReport["full"] = QJsonObject{ { "elapsed_ms", fullMs }, { "before", fullBefore }, { "after", fullAfter }, { "sibling_before", siblingFullBefore }, { "sibling_after", siblingFullAfter } };
		captureFrame(QStringLiteral("full-after"));
		aWindow.open_settings(qt::settings_page::appearance);
		motion->setCurrentIndex(motion->findData(static_cast<int>(core::motion_mode::reduced)));
		for (auto* widget : QApplication::topLevelWidgets()) {
			if (dynamic_cast<qt::settings_window*>(widget) != nullptr) {
				widget->close();
			}
		}
		aWindow.activateWindow();
		wait_for([&] { return !aWindow.dif()->current_frame().NextMs.has_value(); }, "ordinary Reduced motion removes frame deadline");
		run_interval(250);
		captureFrame(QStringLiteral("reduced-before"));
		const auto reducedBefore        = dif_sample(*aWindow.dif());
		const auto siblingReducedBefore = dif_sample(*aSibling.dif());
		const auto reducedDiffBefore    = aWindow.comparison_surface()->statistics();
		elapsed.restart();
		run_interval(1200);
		const auto reducedMs           = elapsed.elapsed();
		const auto reducedAfter        = dif_sample(*aWindow.dif());
		const auto siblingReducedAfter = dif_sample(*aSibling.dif());
		expect(siblingReducedBefore == siblingReducedAfter, "reduced sibling remains static");
		expect(reducedBefore == reducedAfter, "reduced interval has no timer, paint, playback or frame change");
		expect(reducedDiffBefore == aWindow.comparison_surface()->statistics(), "reduced interval causes no diff work");
		aReport["reduced"] = QJsonObject{ { "elapsed_ms", reducedMs }, { "before", reducedBefore }, { "after", reducedAfter }, { "sibling_before", siblingReducedBefore }, { "sibling_after", siblingReducedAfter } };
		captureFrame(QStringLiteral("reduced-after"));
		aReport["captures"]            = captures;
		aReport["diff_work_unchanged"] = true;
		aReport["diff_work"]           = QJsonObject{ { "geometry_rebuilds_before", static_cast<qint64>(diffBefore.GeometryRebuilds) },
													  { "geometry_rebuilds_after", static_cast<qint64>(diffAfter.GeometryRebuilds) },
													  { "publications_before", static_cast<qint64>(diffBefore.Publications) },
													  { "publications_after", static_cast<qint64>(diffAfter.Publications) },
													  { "pane_measurements_before", static_cast<qint64>(diffBefore.PaneMeasurements) },
													  { "pane_measurements_after", static_cast<qint64>(diffAfter.PaneMeasurements) },
													  { "semantic_adoptions_before", static_cast<qint64>(diffBefore.SemanticIndexAdoptions) },
													  { "semantic_adoptions_after", static_cast<qint64>(diffAfter.SemanticIndexAdoptions) } };
		expect(aOwner.status().DifCatalogLoadAttempts == 1U && aOwner.status().DifCatalogDecodedClips == 10U
				   && aWindow.dif()->cache_identity() == cache && aSibling.dif()->cache_identity() == cache,
			   "intervals do not decode a second catalog");
		aReport["load_attempts"] = static_cast<qint64>(aOwner.status().DifCatalogLoadAttempts);
		aReport["decoded_clips"] = static_cast<qint64>(aOwner.status().DifCatalogDecodedClips);
		const auto reportPath    = qEnvironmentVariable("BPR_DIF_INTEGRATION_REPORT_PATH");
		expect(QDir{ }.mkpath(QFileInfo(reportPath).absolutePath()), "Dif report parent directory");
		QFile report{ reportPath };
		expect(report.open(QIODevice::WriteOnly | QIODevice::Truncate), "Dif report opens");
		const auto bytes = QJsonDocument{ aReport }.toJson(QJsonDocument::Indented);
		expect(report.write(bytes) == bytes.size() && report.flush(), "complete Dif JSON report written");
		std::cout << "dif-integration-report:pass\n";
		aWindow.modality()->action(qt::review_modality::review)->trigger();
	}
	void select_file(app::github_application& aOwner, qt::workbench_window& aWindow)
	{
		wait_for([&] {
			for (const auto context : aOwner.windows()) {
				if (aOwner.window(context) == &aWindow) {
					const auto status = aOwner.status(context);
					return status && status->EpochStatus == core::epoch_status::ready;
				}
			}
			return false; },
				 "exact window comparison epoch becomes ready", [&] {
            for (const auto context : aOwner.windows()) {
                if (aOwner.window(context) != &aWindow){ continue;
}
                const auto status = aOwner.status(context);
                const auto work = app::detail::github_application_fixture_access::diff_work_evidence(aOwner, context);
                std::cerr << "epoch=" << (status ? static_cast<int>(status->EpochStatus) : -1)
                          << " outstanding=" << work.EpochOutstandingJobs
                          << " navigation=" << work.NavigationReady
                          << " workbench=" << work.WorkbenchOutstandingRequests
                          << " status=" << aWindow.status_text().toStdString() << '\n';
            } }, 60000);
		aWindow.modality()->action(qt::review_modality::review)->trigger();
		auto* files = aWindow.files_drawer()->contents_view();
		auto* model = aWindow.files_drawer()->contents();
		wait_for([&] { return model->index_for_file(core::file_identity_of("review.cpp")).isValid(); }, "accepted review lists the exact requested file");
		files->selectionModel()->clearCurrentIndex();
		files->selectionModel()->setCurrentIndex(model->index_for_file(core::file_identity_of("review.cpp")), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
		wait_for([&] {
			auto* surface = aWindow.comparison_surface();
			return surface->current_result() && surface->current_result()->Alignment.Key.File == core::file_identity_of("review.cpp") && surface->current_semantic_index() && !surface->geometry_pending() && surface->isVisible(); },
				 "selected file produces an actual comparison", [&] {
                auto* surface = aWindow.comparison_surface();
                std::cerr << "comparison status=" << aWindow.status_text().toStdString()
                          << " result=" << bool(surface->current_result())
                          << " semantic=" << bool(surface->current_semantic_index())
                          << " geometry-pending=" << surface->geometry_pending()
                          << " visible=" << surface->isVisible() << '\n';
                capture(aWindow, QStringLiteral("comparison-failure.png")); }, 60000);
	}
	void submit(qt::workbench_window& aWindow, const int aNumber)
	{
		if (!aWindow.showing_home()) {
			aWindow.home_action()->trigger();
		}
		aWindow.home_open_bar()->setText(QStringLiteral("https://github.com/octo-org/review-repository/pull/%1").arg(aNumber));
		aWindow.home_open_action()->trigger();
	}
	void mark_conversation_read(qt::conversation_cards_view& aCards, const std::string& aIdentity)
	{
		// [[BPR-1290]]. The thread opens in place; Mark read is in its header.
		aCards.open_thread(aIdentity);
		QApplication::processEvents();
		auto* const markRead = aCards.open_card() != nullptr ? aCards.open_card()->findChild<QAbstractButton*>(QStringLiteral("threadMarkRead")) : nullptr;
		expect(markRead != nullptr && markRead->isEnabled(), "recorded unread conversation offers its public mark-read gesture");
		markRead->click();
	}
}

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	QTemporaryDir temporary;
	expect(temporary.isValid(), "private multiwindow test root");
	const auto root = e2e::private_data_root(temporary).value_or(std::filesystem::path{ });
	expect(!root.empty(), "private desktop test root under the canonical storage policy");
	auto options                   = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot    = root;
	options.RegisteredDeviceCourse = true;
	options.IndependentWindowReads = true;
	options.RepeatReadSessions     = true;
	QJsonObject difReport{
		{ "schema", "bpr.dif-integration.v1" },
		{ "qt_platform", QGuiApplication::platformName() },
		{ "memory_scope", "whole-process Mach TASK_VM_INFO" },
		{ "before_show", memory_sample() },
	};
	app::github_application* owner    = nullptr;
	auto                     terminal = false;
	auto                     desktop  = app::detail::github_application_fixture_access::start_desktop(
		app::detail::github_public_path_fixture::profile_window_restoration_v1, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				owner = *aResult;
			} else {
				std::cerr << "startup failure: " << static_cast<int>(aResult.error()) << '\n';
			}
		});
	wait_for([&] { return terminal; }, "profile owner startup completes");
	expect(owner != nullptr, "profile owner admitted");
	owner->show();
	wait_for([&] { return owner->windows().size() == 1U; }, "first authoritative window admitted");
	const auto first   = owner->windows().front();
	auto*      windowA = owner->window(first);
	expect(windowA != nullptr && windowA->isVisible(), "first public window visible");
	difReport["one_window"] = memory_sample();
	// [[BPR-0732]]. Where preview is composed a pasted public link no longer
	// stops at sign-in: it opens signed out, and the fixture answers the
	// anonymous read of one pull request once, not the independent and
	// repeated reads this course makes from three windows. The account course
	// signs in from the landing card first, as the restoration and layout
	// courses do; an account opens the destination without a stop.
	// Only the first window signs in, and it signs in before its first paste:
	// `PublicPreviewSignedOut` is still false until a paste activates the
	// shipped signed-out session, and a sibling that signs in later enrols a
	// second grant whose identity change retires every window's session.
	expect(windowA->home_landing()->state() == qt::landing_state::sign_in && !windowA->home_list()->queue_shown(), "first public window lands signed out with no queue");
	windowA->home_landing()->primary_button()->click();
	wait_for([&] { return windowA->session_panel()->status() == core::github_status::session_ready && !owner->status().PublicPreviewSignedOut; }, "Home sign-in reaches an account session", [&] {
		const auto evidence = app::detail::github_application_fixture_access::evidence(*owner);
		std::cerr << "sign-in status=" << windowA->session_panel()->status_text()
			<< " diagnostic=" << windowA->session_panel()->diagnostic_text()
			<< " preview-signed-out=" << owner->status().PublicPreviewSignedOut
			<< " vault=" << evidence.VaultStores << " bootstrap=" << evidence.BootstrapTransportSelected
			<< " account=" << evidence.AccountTransportSelected << '\n'; });
	expect(windowA->home_list()->queue_shown(), "an account shows the queue");
	submit(*windowA, 17);
	auto* signIn = windowA->findChild<QPushButton*>(QStringLiteral("homeSetupContinue"));
	expect(signIn != nullptr && signIn->isHidden(), "an account opens the first review without a sign-in stop");
	wait_for([&] { return windowA->session_panel()->status() == core::github_status::pull_request_ready; }, "first review opens through public setup", [&] {
		const auto evidence = app::detail::github_application_fixture_access::evidence(*owner);
		std::cerr << "status=" << windowA->session_panel()->status_text()
			<< " diagnostic=" << windowA->session_panel()->diagnostic_text()
			<< " notice=" << windowA->home_open_notice()->text().toStdString()
			<< " vault=" << evidence.VaultStores << " bootstrap=" << evidence.BootstrapTransportSelected
			<< " account=" << evidence.AccountTransportSelected << " reads=" << evidence.AccountReadCount
			<< " last-read=" << evidence.LastReadOperation << " wire-error=" << evidence.RecordingFailure << '\n'; });

	action(*windowA, "newBprWindowAction")->trigger();
	wait_for([&] { return owner->windows().size() == 2U; }, "New Window admits a second session");
	const auto contexts = owner->windows();
	const auto second   = contexts.front() == first ? contexts.back() : contexts.front();
	expect(second != first && second.Profile == first.Profile, "distinct windows share exact profile identity");
	auto* windowB = owner->window(second);
	expect(windowB != nullptr && windowB != windowA && windowB->isVisible(), "second real Qt window visible");
	expect(windowB->showing_home(), "new window starts on Home without copying sibling review");
	expect(desktop->runtime(first.Profile) == owner && desktop->window(second) == windowB, "one profile owner routes both exact contexts");
	expect(windowA->dif() != nullptr && windowB->dif() != nullptr
			   && windowA->dif()->cache_identity() == windowB->dif()->cache_identity(),
		   "both profile windows use the same decoded production catalog");
	expect(owner->status(first)->DifCatalogLoadAttempts == 1U
			   && owner->status(first)->DifCatalogDecodedClips == 10U
			   && owner->status(second)->DifCatalogLoadAttempts == 1U
			   && owner->status(second)->DifCatalogDecodedClips == 10U,
		   "the shared provider decodes all ten accepted strips exactly once");
	difReport["two_windows"] = memory_sample();
	windowA->resize(1100, 700);
	windowB->resize(1400, 900);
	QApplication::processEvents();
	const auto sizeA = windowA->size();
	const auto sizeB = windowB->size();
	expect(sizeA != sizeB, "window geometry is independent");

	windowA->open_settings(qt::settings_page::appearance);
	expect(visible_settings() == 1U, "first Settings gesture opens one shell");
	windowB->open_settings(qt::settings_page::appearance);
	expect(visible_settings() == 1U, "second Settings gesture reuses profile shell");
	// [[BPR-0790]]. The theme tiles are the control; choosing one is this gesture.
	windowB->appearance_settings()->choose_mode(
		core::appearance_mode::high_contrast_dark);
	auto* motion = windowB->appearance_settings()->motion_selector();
	motion->setCurrentIndex(motion->findData(static_cast<int>(core::motion_mode::full)));
	wait_for([&] {
		const auto statusA = owner->status(first);
		const auto statusB = owner->status(second);
		return statusA && statusB && statusA->ProfileMode == core::appearance_mode::high_contrast_dark
			&& statusB->ProfileMode == statusA->ProfileMode
			&& statusA->DifResolution.Preferences.Motion == core::motion_mode::full
			&& statusB->DifResolution == statusA->DifResolution;
	},
			 "accepted appearance and motion preferences reach both windows");
	windowB->activateWindow();
	QApplication::processEvents();
	const auto inactiveWakeups = windowA->dif()->statistics().TimerWakeups;
	QThread::msleep(350);
	QApplication::processEvents();
	expect(windowA->dif()->statistics().TimerWakeups == inactiveWakeups,
		   "the inactive sibling schedules no decorative timer wakeups");
	expect(windowA->size() == sizeA && windowB->size() == sizeB, "shared preferences preserve independent geometry");
	submit(*windowB, 18);
	wait_for([&] { return windowB->session_panel()->status() == core::github_status::pull_request_ready; }, "second window opens distinct recorded PR using shared authentication");
	expect(windowA->session_panel()->status() == core::github_status::pull_request_ready, "second read preserves first review");

	expect(app::detail::github_application_fixture_access::evidence(*owner).VaultStores == 1U, "second window reuses accepted authentication without enrolling again");
	const auto snapshotA = app::detail::github_application_fixture_access::large_review_snapshot(*owner, first);
	const auto snapshotB = app::detail::github_application_fixture_access::large_review_snapshot(*owner, second);
	expect(snapshotA && snapshotB && snapshotA->PullRequest.Number.value() == 17U && snapshotB->PullRequest.Number.value() == 18U,
		   "accepted read snapshots retain distinct PR identities");
	expect(snapshotA->Account == snapshotB->Account && snapshotA->Profile == snapshotB->Profile, "both reviews use shared account authority");
	select_file(*owner, *windowA);
	select_file(*owner, *windowB);
	const auto comparisonA = windowA->comparison_surface()->current_result();
	const auto comparisonB = windowB->comparison_surface()->current_result();
	expect(comparisonA->Alignment.Key.PullRequest == snapshotA->PullRequest.PullRequest && comparisonB->Alignment.Key.PullRequest == snapshotB->PullRequest.PullRequest, "each displayed comparison names its accepted PR");
	expect(comparisonA->Alignment.Key != comparisonB->Alignment.Key, "different PR windows retain distinct exact comparison keys");
	measure_dif(*owner, *windowB, *windowA, difReport);
	const auto selectionB = windowB->comparison_surface()->selection();
	expect(windowA->comparison_surface()->select_conversation_line(core::diff_side::right, 2U), "first window supports exact semantic line selection");
	expect(windowA->comparison_surface()->selection().has_value(), "first window receives semantic selection");
	windowA->comparison_surface()->shared_scrollbar()->setValue(
		windowA->comparison_surface()->shared_scrollbar()->maximum());
	windowA->comparison_surface()->shared_scrollbar()->setFocus(Qt::OtherFocusReason);
	QApplication::processEvents();
	const auto  retainedSelection  = windowA->comparison_surface()->selection();
	const auto  retainedScroll     = windowA->comparison_surface()->shared_scrollbar()->value();
	const auto  retainedLeftEpoch  = windowA->epoch_selectors()->left_selector()->currentIndex();
	const auto  retainedRightEpoch = windowA->epoch_selectors()->right_selector()->currentIndex();
	auto* const retainedFocus      = QApplication::focusWidget();
	windowA->choose_home(true);
	QApplication::processEvents();
	expect(windowA->showing_home() && windowA->comparison_surface()->current_result() == comparisonA,
		   "Home covers the live review without replacing its comparison");
	windowA->modality()->action(qt::review_modality::review)->trigger();
	QApplication::processEvents();
	expect(!windowA->showing_home()
			   && windowA->comparison_surface()->current_result() == comparisonA
			   && windowA->comparison_surface()->selection() == retainedSelection
			   && windowA->comparison_surface()->shared_scrollbar()->value() == retainedScroll
			   && windowA->epoch_selectors()->left_selector()->currentIndex() == retainedLeftEpoch
			   && windowA->epoch_selectors()->right_selector()->currentIndex() == retainedRightEpoch
			   && QApplication::focusWidget() == retainedFocus,
		   "returning from Home preserves comparison, semantic selection, scroll, epochs, and focus");
	expect(windowB->comparison_surface()->selection() == selectionB && windowB->comparison_surface()->current_result() == comparisonB,
		   "navigation in first window preserves second comparison and selection");
	expect(windowA->comparison_surface()->current_result() == comparisonA, "sibling file selection preserved first comparison");
	capture(*windowA, QStringLiteral("window-a.png"));
	capture(*windowB, QStringLiteral("window-b.png"));
	const auto unrelatedReceiptB      = windowB->review_editor()->shown_receipt();
	const auto unrelatedPublicationsB = windowB->comparison_surface()->statistics().Publications;
	const auto unrelatedLedgerB       = app::detail::github_application_fixture_access::scaling_evidence(*owner, second).Ledger;
	action(*windowA, "toggleSelectedFileReadAction")->trigger();
	wait_for([&] { return windowA->review_editor()->shown_receipt() && windowA->review_editor()->shown_receipt()->Read == core::file_read_state::read; }, "first review accepts the public receipt action");
	QApplication::processEvents();
	if (windowB->review_editor()->shown_receipt() != unrelatedReceiptB
		|| windowB->comparison_surface()->current_result() != comparisonB
		|| windowB->comparison_surface()->selection() != selectionB) {
		const auto  identity         = app::detail::github_application_fixture_access::epoch_refresh_identity(*owner, second);
		const auto& currentReceipt   = windowB->review_editor()->shown_receipt();
		const auto& currentSelection = windowB->comparison_surface()->selection();
		const auto  currentLedger    = app::detail::github_application_fixture_access::scaling_evidence(*owner, second).Ledger;
		std::cerr << "MULTI_WINDOW_RECEIPT_FAILURE receipt_before=" << unrelatedReceiptB.has_value()
				  << " receipt_after=" << currentReceipt.has_value() << " receipt_equal=" << (currentReceipt == unrelatedReceiptB)
				  << " before_read=" << (unrelatedReceiptB ? static_cast<int>(unrelatedReceiptB->Read) : -1)
				  << " after_read=" << (currentReceipt ? static_cast<int>(currentReceipt->Read) : -1)
				  << " material_equal=" << (unrelatedReceiptB && currentReceipt && unrelatedReceiptB->Material == currentReceipt->Material)
				  << " publications_before=" << unrelatedPublicationsB << " publications_after=" << windowB->comparison_surface()->statistics().Publications
				  << " ledger_operations_before=" << unrelatedLedgerB.Operations << " ledger_operations_after=" << currentLedger.Operations
				  << " ledger_completions_before=" << unrelatedLedgerB.Completions << " ledger_completions_after=" << currentLedger.Completions
				  << " comparison_same=" << (windowB->comparison_surface()->current_result() == comparisonB)
				  << " selection_equal=" << (currentSelection == selectionB)
				  << " before_line=" << (selectionB ? selectionB->PreferredLine.Line : 99999U)
				  << " after_line=" << (currentSelection ? currentSelection->PreferredLine.Line : 99999U)
				  << " tab_pending=" << identity.TabPending << " tab_load=" << identity.TabLoad
				  << " displayed=" << identity.PublicationDisplayed << " request_match=" << identity.DiffRequestMatches << '\n';
	}
	expect(windowB->review_editor()->shown_receipt() == unrelatedReceiptB
			   && windowB->comparison_surface()->current_result() == comparisonB
			   && windowB->comparison_surface()->selection() == selectionB,
		   "receipt writes in PR17 do not change PR18's receipt or comparison");
	// The existing private-note seed writes through A's composed ledger host.
	// Observe the resulting public overview in another session of the same PR.
	submit(*windowB, 17);
	wait_for([&] { return windowB->session_panel()->status() == core::github_status::pull_request_ready; }, "second window temporarily opens first review for note fanout");
	select_file(*owner, *windowB);
	const auto noteComparisonB = windowB->comparison_surface()->current_result();
	const auto noteSelectionB  = windowB->comparison_surface()->selection();
	const auto noteSelectionA  = windowA->comparison_surface()->selection();
	const auto noteScrollB     = windowB->comparison_surface()->shared_scrollbar()->value();
	expect(app::detail::github_application_fixture_access::seed_agent_private_context_canary(*owner, "First shared private fixture note", "Second shared private fixture note"), "private notes persist through the composed ledger");
	wait_for([&] {
		return windowA->overview_page()->projection() && windowB->overview_page()->projection()
			&& windowA->overview_page()->projection()->Progress.PrivateNotes == 2U
			&& windowB->overview_page()->projection()->Progress.PrivateNotes == 2U;
	},
			 "accepted private notes refresh both exact-review overviews");
	expect(windowA->overview_page()->value_label(qt::overview_value::private_notes)->text() == QStringLiteral("2")
			   && windowB->overview_page()->value_label(qt::overview_value::private_notes)->text() == QStringLiteral("2"),
		   "both overview labels render the accepted private-note count");
	expect(windowA->comparison_surface()->current_result() == comparisonA
			   && windowA->comparison_surface()->selection() == noteSelectionA
			   && windowB->comparison_surface()->current_result() == noteComparisonB
			   && windowB->comparison_surface()->selection() == noteSelectionB
			   && windowB->comparison_surface()->shared_scrollbar()->value() == noteScrollB,
		   "private-note fanout preserves independent comparisons and reading position");
	// Change only fixture wire responses, then use the shipped Refresh gesture.
	// The other window must update its rendered model without issuing a read or
	// replaying its comparison/selection/scroll restoration.
	const auto serverSnapshot = app::detail::github_application_fixture_access::large_review_snapshot(*owner, second);
	expect(serverSnapshot && !serverSnapshot->ReviewThreads.empty()
			   && !serverSnapshot->ReviewThreads.front().Comments.empty(),
		   "server fanout course has an existing thread and root post");
	const auto serverThread         = std::string{ serverSnapshot->ReviewThreads.front().Thread.value() };
	const auto serverRoot           = "review-comment:" + std::string{ serverSnapshot->ReviewThreads.front().Comments.front().Comment.value() };
	const auto serverThreadIdentity = "review-thread:" + serverThread;
	// The retained Review selection is deliberately a different source line.
	// Discuss shows the whole conversation, not only threads on that line.
	windowA->modality()->action(qt::review_modality::discuss)->trigger();
	expect(windowA->conversation_model()->index_for_id(serverRoot).isValid(), "Discuss renders the existing server root before refresh");
	const auto siblingScroll = windowA->comparison_surface()->shared_scrollbar()->value();
	const auto modelResets   = windowA->conversation_model()->statistics().ModelResets;
	for (const auto addedAndResolved : { true, false }) {
		if (!addedAndResolved) {
			windowA->choose_home(true);
		}
		app::detail::github_application_fixture_access::stage_server_conversation(*owner, serverThread, addedAndResolved);
		windowB->github_refresh_action()->trigger();
		wait_for([&] {
			const auto* model       = windowA->conversation_model();
			const auto  rootIndex   = model->index_for_id(serverRoot);
			const auto  threadIndex = model->index_for_id(serverThreadIdentity);
			return windowB->session_panel()->status() == core::github_status::pull_request_ready
				&& rootIndex.isValid() && threadIndex.isValid()
				&& model->data(rootIndex, qt::github_conversation_model::body_role).toString()
				== (addedAndResolved ? QStringLiteral("Edited server root") : QStringLiteral("Reopened server root"))
				&& model->data(threadIndex, qt::github_conversation_model::resolved_role).toBool() == addedAndResolved
				&& model->index_for_id("review-comment:PRRC_shared_server_reply").isValid() == addedAndResolved; },
				 "public Refresh fans out added/edited/deleted posts and resolved/reopened state to rendered sibling", [&] {
			const auto source = app::detail::github_application_fixture_access::large_review_snapshot(*owner, second);
			const auto target = app::detail::github_application_fixture_access::large_review_snapshot(*owner, first);
			std::cerr << "fanout phase=" << addedAndResolved << " source status=" << windowB->session_panel()->status_text()
				<< " source=" << (source ? source->Snapshot.value() : 0U) << " target=" << (target ? target->Snapshot.value() : 0U)
				<< " root=" << windowA->conversation_model()->data(windowA->conversation_model()->index_for_id(serverRoot), qt::github_conversation_model::body_role).toString().toStdString()
				<< " thread=" << serverThread << '\n';
			if (source && !source->ReviewThreads.empty()) {
				std::cerr << "source root=" << source->ReviewThreads.front().Comments.front().Body
					<< " resolved=" << source->ReviewThreads.front().Resolved << " posts=" << source->ReviewThreads.front().Comments.size() << '\n';
			} });
		expect(windowA->comparison_surface()->current_result() == comparisonA
				   && windowA->comparison_surface()->selection() == noteSelectionA
				   && windowA->comparison_surface()->shared_scrollbar()->value() == siblingScroll
				   && windowA->conversation_model()->statistics().ModelResets == modelResets,
			   "server fanout preserves sibling exact comparison, reading position and incremental model");
		expect(windowA->showing_home() == !addedAndResolved,
			   "shared server observations retain the sibling's explicit Home or Discuss destination");
	}
	submit(*windowB, 18);
	wait_for([&] { return windowB->session_panel()->status() == core::github_status::pull_request_ready; }, "second window returns to its independent review");
	select_file(*owner, *windowB);
	expect(windowB->overview_page()->projection() && windowB->overview_page()->projection()->Progress.PrivateNotes == 0U, "replacement review does not inherit another PR's private notes");
	app::detail::github_application_fixture_access::hold_window_read_terminal(*owner, second, true);
	windowB->github_refresh_action()->trigger();
	wait_for([&] { return app::detail::github_application_fixture_access::held_window_read_terminals(*owner, second) > 0U; }, "surviving window has a real read completion pending");
	windowA->close();
	wait_for([&] { return owner->window(first) == nullptr && owner->windows().size() == 1U; }, "closing first window retires only its session");
	expect(desktop->runtime(first.Profile) == owner && owner->window(second) == windowB, "profile services survive first window close");
	expect(!windowA->isVisible(), "accepted close hides initial retained shell while sibling survives");
	app::detail::github_application_fixture_access::hold_window_read_terminal(*owner, second, false);
	wait_for([&] { return windowB->session_panel()->status() == core::github_status::pull_request_ready; }, "surviving read completes after first window closes");
	const auto resumed = app::detail::github_application_fixture_access::large_review_snapshot(*owner, second);
	expect(resumed && resumed->PullRequest.Number.value() == 18U, "surviving completion retains exact PR identity");
	select_file(*owner, *windowB);
	windowB->open_settings(qt::settings_page::appearance);
	expect(visible_settings() == 1U, "profile Settings remains usable after first window retirement");
	// Closing hides the shell before durable retirement and its queued
	// session removal complete. Observe that public completion boundary.
	wait_for([&] { return !owner->status(first); }, "retired exact context completes durable shutdown");
	expect(!owner->status(first).has_value(), "retired exact context has no status");
	auto stale       = second;
	stale.Generation = core::bpr_window_generation{ second.Generation.value() + 1U };
	expect(owner->window(stale) == nullptr && !owner->status(stale), "foreign generation cannot address surviving window");
	action(*windowB, "manageProfilesAction")->trigger();
	wait_for([&] { return desktop->management_window() != nullptr; }, "management remains reachable from survivor");
	auto& management = *desktop->management_window();
	wait_for([&] { return management.open_button()->isEnabled(); }, "the chooser's filled action is available for the current profile");
	// [[BPR-0778]]. Return on the row activates an open profile; the footer's filled
	// action now offers that profile a new window instead, so activation is the row's.
	expect(management.open_button()->text() == QStringLiteral("New window"),
		   "an open profile is offered a new window, not a second activation");
	auto activate = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
	QApplication::sendEvent(management.profiles(), &activate);
	QApplication::processEvents();
	expect(owner->windows().size() == 1U && owner->window(second) == windowB, "activating the survivor adds no duplicate owner or session");
	action(*windowB, "newBprWindowAction")->trigger();
	wait_for([&] { return owner->windows().size() == 2U; }, "New Window remains usable after initial retirement");
	qt::workbench_window* windowC = nullptr;
	for (const auto context : owner->windows()) {
		expect(context != first, "retired context is never reused");
		if (context != second) {
			expect(owner->window(context) != nullptr && owner->window(context)->showing_home(), "replacement is a fresh Home session");
			windowC = owner->window(context);
		}
	}
	expect(windowC != nullptr, "replacement window is reachable by its context");
	submit(*windowC, 18);
	wait_for([&] { return windowC->session_panel()->status() == core::github_status::pull_request_ready; }, "replacement opens sibling PR without another sign-in");
	select_file(*owner, *windowC);
	wait_for([&] { return windowB->review_editor()->shown_receipt() && windowC->review_editor()->shown_receipt(); }, "same-review windows receive selected-file receipts");
	expect(windowB->comparison_surface()->select_conversation_line(core::diff_side::right, 1U)
			   && windowC->comparison_surface()->select_conversation_line(core::diff_side::right, 3U),
		   "same-review windows choose distinct recorded source lines before receipt fanout");
	expect(windowC->comparison_surface()->selection().has_value()
			   && windowC->comparison_surface()->selection() != windowB->comparison_surface()->selection(),
		   "same-review receipt course starts with distinct semantic reading positions");
	const auto receiptComparisonB = windowB->comparison_surface()->current_result();
	const auto receiptComparisonC = windowC->comparison_surface()->current_result();
	const auto receiptSelectionB  = windowB->comparison_surface()->selection();
	const auto receiptSelectionC  = windowC->comparison_surface()->selection();
	const auto receiptScrollB     = windowB->comparison_surface()->shared_scrollbar()->value();
	const auto receiptScrollC     = windowC->comparison_surface()->shared_scrollbar()->value();
	expect(windowB->review_editor()->shown_receipt()->Read != core::file_read_state::read
			   && windowB->review_editor()->shown_receipt()->File == windowC->review_editor()->shown_receipt()->File
			   && windowB->review_editor()->shown_receipt()->Material == windowC->review_editor()->shown_receipt()->Material,
		   "receipt course starts with the same exact unread file material");
	auto receiptContextC = core::bpr_window_context{ };
	for (const auto context : owner->windows()) {
		if (owner->window(context) == windowC) {
			receiptContextC = context;
		}
	}
	expect(receiptContextC.Window.value() != 0U, "sibling scaling sample binds an exact live context");
	const auto receiptWorkB = app::detail::github_application_fixture_access::scaling_evidence(*owner, second);
	const auto receiptWorkC = app::detail::github_application_fixture_access::scaling_evidence(*owner, receiptContextC);
	action(*windowB, "toggleSelectedFileReadAction")->trigger();
	wait_for([&] {
		return windowB->review_editor()->shown_receipt() && windowC->review_editor()->shown_receipt()
			&& windowB->review_editor()->shown_receipt()->Read == core::file_read_state::read
			&& windowC->review_editor()->shown_receipt() == windowB->review_editor()->shown_receipt();
	},
			 "public read action refreshes the sibling's accepted receipt");
	const auto receiptAfterB = app::detail::github_application_fixture_access::scaling_evidence(*owner, second);
	const auto receiptAfterC = app::detail::github_application_fixture_access::scaling_evidence(*owner, receiptContextC);
	expect(receiptAfterB.Workbench.ActionabilityObservations == receiptWorkB.Workbench.ActionabilityObservations
			   && receiptAfterC.Workbench.ActionabilityObservations == receiptWorkC.Workbench.ActionabilityObservations
			   && receiptAfterB.Workbench.AcceptedObservationRows == receiptWorkB.Workbench.AcceptedObservationRows
			   && receiptAfterC.Workbench.AcceptedObservationRows == receiptWorkC.Workbench.AcceptedObservationRows
			   && receiptAfterB.Workbench.TargetedActionabilityUpdates == receiptWorkB.Workbench.TargetedActionabilityUpdates + 1U
			   && receiptAfterC.Workbench.TargetedActionabilityUpdates == receiptWorkC.Workbench.TargetedActionabilityUpdates + 1U,
		   "writer and sibling each apply one receipt delta without reobserving their catalogues");
	expect(windowB->comparison_surface()->current_result() == receiptComparisonB
			   && windowC->comparison_surface()->current_result() == receiptComparisonC
			   && windowB->comparison_surface()->selection() == receiptSelectionB
			   && windowC->comparison_surface()->selection() == receiptSelectionC
			   && windowB->comparison_surface()->shared_scrollbar()->value() == receiptScrollB
			   && windowC->comparison_surface()->shared_scrollbar()->value() == receiptScrollC,
		   "receipt fanout preserves each exact comparison, semantic selection, and scroll");
	expect(receiptComparisonB->Alignment.Key.PullRequest == snapshotB->PullRequest.PullRequest
			   && receiptComparisonC->Alignment.Key.PullRequest == snapshotB->PullRequest.PullRequest,
		   "receipt delivery retains both windows' selected PR identity");
	{
		// [[BPR-0972]]. A failed exact-material reload must retain the accepted
		// actionability. Temporarily hide this fixture's table after the writer's
		// public commit, before the queued sibling notification is delivered.
		const auto retainedReceiptC   = windowC->review_editor()->shown_receipt();
		const auto beforeFailedReload = app::detail::github_application_fixture_access::scaling_evidence(*owner, receiptContextC);
		sqlite3*   ledgerRaw          = nullptr;
		expect(sqlite3_open_v2((root / "state/v1/review-ledger.sqlite3").string().c_str(), &ledgerRaw, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK, "fixture opens its own receipt database for a bounded read fault");
		const auto ledgerConnection = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>{ ledgerRaw, sqlite3_close };
		// Preserve canonical schema SQL: renaming back adds quotes, which the
		// adapter correctly refuses when a later PR opens a new ledger scope.
		sqlite3_stmt* schemaRaw = nullptr;
		expect(sqlite3_prepare_v2(ledgerRaw, "SELECT sql FROM sqlite_schema WHERE name = 'file_receipt'", -1, &schemaRaw, nullptr) == SQLITE_OK, "fixture reads its canonical receipt schema");
		const auto schemaStatement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>{ schemaRaw, sqlite3_finalize };
		expect(sqlite3_step(schemaRaw) == SQLITE_ROW, "receipt schema exists before the fault");
		const auto receiptSchema = std::string{ reinterpret_cast<const char*>(sqlite3_column_text(schemaRaw, 0)) };
		expect(sqlite3_step(schemaRaw) == SQLITE_DONE, "fixture finishes the schema read before its write");
		// [[BPR-1211]]. The writer's commit runs on the profile's ledger lane.
		// Holding that lane behind it lets the commit and its sibling
		// notification land while the sibling's reload waits, so the fault
		// is injected between the two as it was when the commit was inline.
		action(*windowB, "toggleSelectedFileReadAction")->trigger();
		const auto release = app::detail::github_application_fixture_access::hold_review_ledger_effects(*owner);
		wait_for([&] {
			return windowB->review_editor()->shown_receipt()->Read == core::file_read_state::seen
				&& app::detail::github_application_fixture_access::scaling_evidence(*owner, receiptContextC).Ledger.ReceiptLoads > beforeFailedReload.Ledger.ReceiptLoads;
		},
				 "writer commits and the sibling's reload is queued behind the held ledger lane");
		expect(windowB->review_editor()->shown_receipt()->Read == core::file_read_state::seen, "writer commits before the sibling read fault");
		expect(sqlite3_exec(ledgerRaw, "ALTER TABLE file_receipt RENAME TO fixture_hidden_receipt", nullptr, nullptr, nullptr) == SQLITE_OK, "fixture hides only its own receipt table");
		release();
		wait_for([&] { return app::detail::github_application_fixture_access::scaling_evidence(*owner, receiptContextC).Ledger.StoreFailures > beforeFailedReload.Ledger.StoreFailures; }, "queued sibling refresh observes the actual SQLite read failure");
		const auto afterFailedReload = app::detail::github_application_fixture_access::scaling_evidence(*owner, receiptContextC);
		expect(windowC->review_editor()->shown_receipt() == retainedReceiptC
				   && afterFailedReload.Workbench.TargetedActionabilityUpdates == beforeFailedReload.Workbench.TargetedActionabilityUpdates
				   && afterFailedReload.Workbench.ActionabilityObservations == beforeFailedReload.Workbench.ActionabilityObservations,
			   "failed exact-material reload preserves accepted receipt and withheld order");
		const auto restoreReceipt = "BEGIN IMMEDIATE; " + receiptSchema + "; INSERT INTO file_receipt SELECT * FROM fixture_hidden_receipt; DROP TABLE fixture_hidden_receipt; COMMIT;";
		expect(sqlite3_exec(ledgerRaw, restoreReceipt.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK, "fixture restores exact receipt schema and rows before retry");
		action(*windowB, "toggleSelectedFileReadAction")->trigger();
		wait_for([&] { return app::detail::github_application_fixture_access::scaling_evidence(*owner, receiptContextC).Ledger.Publications > afterFailedReload.Ledger.Publications; }, "next public commit successfully refreshes the sibling after recovery");
		const auto afterRecoveredReload = app::detail::github_application_fixture_access::scaling_evidence(*owner, receiptContextC);
		expect(windowC->review_editor()->shown_receipt() == retainedReceiptC
				   && afterRecoveredReload.Workbench.TargetedActionabilityUpdates == beforeFailedReload.Workbench.TargetedActionabilityUpdates,
			   "equal accepted receipt after recovery keeps the already-withheld order");
	}
	windowB->modality()->action(qt::review_modality::discuss)->trigger();
	windowC->modality()->action(qt::review_modality::discuss)->trigger();
	windowB->conversation_panel()->scope_whole_pull_request_button()->click();
	windowC->conversation_panel()->scope_whole_pull_request_button()->click();
	const auto threadIdentity = std::string{ "review-thread:PRRT_fixture_18_reading_review.cpp" };
	auto*      cardsB         = windowB->conversation_panel()->cards();
	auto*      cardsC         = windowC->conversation_panel()->cards();
	const auto unreadBefore   = cardsC->unread_post_count();
	std::cerr << "conversation unread B=" << cardsB->unread_post_count() << " C=" << unreadBefore << " cards B=" << cardsB->thread_count() << " C=" << cardsC->thread_count() << '\n';
	expect(unreadBefore > 0U && cardsB->unread_post_count() == unreadBefore, "same-review conversation course begins with accepted unread posts");
	mark_conversation_read(*cardsB, threadIdentity);
	wait_for([&] { return cardsC->unread_post_count() < unreadBefore && cardsC->unread_post_count() == cardsB->unread_post_count(); }, "conversation read receipt refreshes sibling unread presentation");
	// [[BPR-1582]]. Threads' row menu, through the composition and the
	// ledger: Mark as Unread makes the thread new again in both windows, and
	// Mark as Read reads it again. Window B counts in Discuss, as C does.
	{
		const auto readCount = cardsC->unread_post_count();
		const auto rowMenu   = [&](const QString& aExpected) {
			windowB->show_threads_action()->trigger();
			QApplication::processEvents();
			auto* const finder = windowB->finder();
			finder->refresh();
			const auto keys = finder->shown_keys();
			const auto row  = std::ranges::find_if(keys, [&threadIdentity](const std::string& aKey) { return aKey.starts_with("row:" + threadIdentity + "/"); });
			expect(row != keys.end(), "Threads lists the conversation");
			const auto box = finder->row_rect(*row);
			finder->popup_row_menu(*row, QPoint{ box.left() + 80, box.center().y() });
			expect(finder->read_action() != nullptr && finder->read_action()->text() == aExpected && finder->read_action()->isEnabled(),
				   "the row menu offers " + aExpected.toStdString());
			finder->row_menu()->hide();
			finder->read_action()->trigger();
			windowB->modality()->action(qt::review_modality::discuss)->trigger();
			QApplication::processEvents();
		};
		rowMenu(QStringLiteral("Mark as Unread"));
		wait_for([&] { return cardsC->unread_post_count() > readCount && cardsB->unread_post_count() == cardsC->unread_post_count(); },
				 "Mark as Unread's receipt makes the conversation new again in both windows");
		rowMenu(QStringLiteral("Mark as Read"));
		wait_for([&] { return cardsC->unread_post_count() == readCount && cardsB->unread_post_count() == readCount; }, "Mark as Read reads it again in both windows");
	}
	expect(windowB->comparison_surface()->current_result() == receiptComparisonB
			   && windowC->comparison_surface()->current_result() == receiptComparisonC
			   && windowB->comparison_surface()->selection() == receiptSelectionB
			   && windowC->comparison_surface()->selection() == receiptSelectionC
			   && windowB->comparison_surface()->shared_scrollbar()->value() == receiptScrollB
			   && windowC->comparison_surface()->shared_scrollbar()->value() == receiptScrollC,
		   "conversation receipt fanout preserves independent comparisons, selections, and scroll");
	windowB->modality()->action(qt::review_modality::review)->trigger();
	windowC->modality()->action(qt::review_modality::review)->trigger();
	// Returning from Discuss restores the active tab on the next decision
	// turn. Finish that public transition before choosing a new reading line.
	QApplication::processEvents();
	wait_for([&] {
		const auto ready = [](const qt::workbench_window& aWindow) {
			return std::ranges::any_of(aWindow.document_tabs()->tabs(), [](const auto& aTab) { return aTab.Active && aTab.State == core::document_tab_view_state::ready; })
				&& !aWindow.comparison_surface()->geometry_pending();
		};
		return ready(*windowB) && ready(*windowC);
	},
			 "returning Review tabs finish restoring before choosing the draft line");
	expect(windowB->comparison_surface()->select_conversation_line(core::diff_side::right, 2U)
			   && windowC->comparison_surface()->select_conversation_line(core::diff_side::right, 2U),
		   "both windows select the same review anchor");
	wait_for([&] { return draft_at_selection(*windowB) && draft_at_selection(*windowC); }, "both actual editors receive the selected local draft");
	expect(windowB->review_editor()->shown_draft()->Anchor == windowC->review_editor()->shown_draft()->Anchor, "shared draft course uses identical comparison and line");
	expect(windowB->review_editor()->shown_draft()->Id != windowC->review_editor()->shown_draft()->Id
			   && windowB->review_editor()->shown_draft()->Id.value() > core::maximum_private_identity
			   && windowC->review_editor()->shown_draft()->Id.value() > core::maximum_private_identity,
		   "untouched same-anchor selections have distinct runtime-only identities");
	windowB->review_editor()->body_editor()->setPlainText(QStringLiteral("Shared profile draft from surviving window"));
	wait_for([&] { return windowC->review_editor()->body_editor()->toPlainText() == QStringLiteral("Shared profile draft from surviving window"); }, "accepted draft edit reaches sibling editor");
	expect(windowB->review_editor()->shown_draft()->Id == windowC->review_editor()->shown_draft()->Id
			   && windowB->review_editor()->shown_draft()->Id.value() <= core::maximum_private_identity
			   && windowB->review_editor()->shown_draft() == windowC->review_editor()->shown_draft(),
		   "untouched sibling adopts the exact durable draft after first save");
	windowC->review_editor()->body_editor()->setPlainText(QStringLiteral("Shared profile draft edited in replacement"));
	wait_for([&] { return windowB->review_editor()->body_editor()->toPlainText() == QStringLiteral("Shared profile draft edited in replacement")
					   && !windowB->review_editor()->rejected_candidate() && !windowC->review_editor()->rejected_candidate(); }, "replacement edit returns through shared owner");
	const auto sharedDraft = *windowB->review_editor()->shown_draft();
	// [[BPR-1521]]. An unrelated untouched anchor must stay untouched.
	expect(windowB->comparison_surface()->select_conversation_line(core::diff_side::right, 1U)
			   && windowC->comparison_surface()->select_conversation_line(core::diff_side::right, 3U),
		   "first-save isolation course selects different anchors");
	wait_for([&] { return draft_at_selection(*windowB) && draft_at_selection(*windowC); }, "different-anchor editors finish selecting before typing");
	const auto untouchedSibling = *windowC->review_editor()->shown_draft();
	windowB->review_editor()->body_editor()->setPlainText(QStringLiteral("Comment at a different anchor"));
	wait_for([&] { return windowB->review_editor()->shown_draft()->Body == "Comment at a different anchor" && !windowB->review_editor()->rejected_candidate(); }, "different-anchor comment commits");
	expect(windowC->review_editor()->shown_draft() == untouchedSibling && windowC->review_editor()->body_editor()->toPlainText().isEmpty(), "different-anchor publication leaves sibling placeholder unchanged");
	// Both first saves wait on the real persistence lane. Delivering either
	// durable record must leave the other's candidate and acknowledgement alone.
	expect(windowB->comparison_surface()->select_conversation_line(core::diff_side::right, 3U), "both authors select the same previously untouched anchor");
	wait_for([&] { return draft_at_selection(*windowB) && draft_at_selection(*windowC); }, "concurrent authors finish selecting before their first saves");
	const auto releaseDraftSaves = app::detail::github_application_fixture_access::hold_private_review_effects(*owner);
	windowB->review_editor()->body_editor()->setPlainText(QStringLiteral("Independent first-save body B\nexact later line"));
	windowC->review_editor()->body_editor()->setPlainText(QStringLiteral("Independent first-save body C\nother exact line"));
	expect(windowB->review_editor()->rejected_candidate() && windowC->review_editor()->rejected_candidate()
			   && windowB->review_editor()->rejected_candidate()->Scope == windowC->review_editor()->rejected_candidate()->Scope
			   && windowB->review_editor()->rejected_candidate()->Command.Id != windowC->review_editor()->rejected_candidate()->Command.Id,
		   "same-authority concurrent authors retain separate pending placeholder commands");
	releaseDraftSaves();
	wait_for([&] {
		return windowB->review_editor()->shown_draft()->Id.value() <= core::maximum_private_identity
			&& windowC->review_editor()->shown_draft()->Id.value() <= core::maximum_private_identity
			&& windowB->review_editor()->shown_draft()->Body == "Independent first-save body B\nexact later line"
			&& windowC->review_editor()->shown_draft()->Body == "Independent first-save body C\nother exact line"
			&& !windowB->review_editor()->rejected_candidate() && !windowC->review_editor()->rejected_candidate();
	},
			 "both concurrent first saves acknowledge their own exact durable content");
	const auto independentB = *windowB->review_editor()->shown_draft();
	const auto independentC = *windowC->review_editor()->shown_draft();
	expect(independentB.Id != independentC.Id && independentB.Anchor == independentC.Anchor, "authored siblings keep distinct durable identities at the same anchor");
	windowB->review_editor()->body_editor()->setPlainText(QStringLiteral("Independent B edited again"));
	wait_for([&] { return windowB->review_editor()->shown_draft()->Body == "Independent B edited again" && !windowB->review_editor()->rejected_candidate(); }, "independent author can edit after the first-save race");
	expect(windowC->review_editor()->shown_draft() == independentC && windowC->review_editor()->body_editor()->toPlainText() == QStringLiteral("Independent first-save body C\nother exact line"), "later sibling edit cannot replace independently authored text");
	expect(windowB->comparison_surface()->select_conversation_line(core::diff_side::right, 2U)
			   && windowC->comparison_surface()->select_conversation_line(core::diff_side::right, 2U),
		   "both authors return to the original shared comment");
	wait_for([&] { return windowB->review_editor()->shown_draft() == sharedDraft && windowC->review_editor()->shown_draft() == sharedDraft; }, "original shared identity and content remain selectable after the race");
	// Queue the sibling write before starting a different-file request. Observe
	// action transitions throughout delivery, including before Git completes.
	auto*      pendingReceiptAction            = action(*windowC, "toggleSelectedFileReadAction");
	auto       receiptEnabledWithoutComparison = false;
	auto       observingPendingReceipt         = false;
	const auto pendingReceiptConnection        = QObject::connect(pendingReceiptAction, &QAction::changed, windowC, [&] {
		if (observingPendingReceipt && pendingReceiptAction->isEnabled() && !windowC->comparison_surface()->current_result()) {
			receiptEnabledWithoutComparison = true;
		}
	});
	const auto pendingComparisonB              = windowB->comparison_surface()->current_result();
	const auto pendingSelectionB               = windowB->comparison_surface()->selection();
	auto*      pendingFiles                    = windowC->files_drawer()->contents_view();
	expect(windowC->files_drawer()->contents()->index_for_file(core::file_identity_of("secondary.cpp")).isValid(), "pending selection course offers the other recorded file");
	action(*windowB, "toggleSelectedFileReadAction")->trigger();
	pendingFiles->selectionModel()->setCurrentIndex(windowC->files_drawer()->contents()->index_for_file(core::file_identity_of("secondary.cpp")), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
	expect(!windowC->comparison_surface()->current_result() && !windowC->review_editor()->shown_receipt()
			   && !pendingReceiptAction->isEnabled() && !windowC->review_editor()->mark_read_action()->isEnabled()
			   && !windowC->review_editor()->accept_action()->isEnabled() && !windowC->review_editor()->exclude_action()->isEnabled(),
		   "beginning the second-file comparison immediately clears the old receipt and actions");
	// Arm after synchronous presentation has finished changing labels and
	// disabling the actions; no queued ledger or Git terminal has run yet.
	observingPendingReceipt = true;
	wait_for([&] {
		const auto result = windowC->comparison_surface()->current_result();
		return result && result->Alignment.Key.File == core::file_identity_of("secondary.cpp")
			&& windowC->review_editor()->shown_receipt() && !windowC->comparison_surface()->geometry_pending();
	},
			 "second recorded file completes after sibling receipt delivery");
	QObject::disconnect(pendingReceiptConnection);
	expect(!receiptEnabledWithoutComparison, "sibling fanout never enables a receipt against an unresolved comparison");
	expect(windowC->review_editor()->shown_receipt()->File == windowC->comparison_surface()->current_result()->Alignment.Key.File
			   && windowC->review_editor()->shown_receipt()->Read != core::file_read_state::read,
		   "second-file receipt belongs to its own material rather than the sibling's file");

	expect(windowB->comparison_surface()->current_result() == pendingComparisonB
			   && windowB->comparison_surface()->selection() == pendingSelectionB,
		   "sibling file replacement preserves the writer's comparison and reading position");
	// Classify while the recipient reads another file: delivery must project
	// the writer's accepted defect without correlating it against that file.
	windowB->conversation_panel()->set_anchor_line(std::nullopt);
	const auto threadIndex = windowB->conversation_model()->index_for_id(threadIdentity);
	expect(threadIndex.isValid(), "classification uses an accepted provider thread");
	windowB->conversation_view()->setCurrentIndex(threadIndex);
	expect(windowB->classify_defect_action()->isEnabled(), "public thread classification is available");
	const auto defectComparisonB = windowB->comparison_surface()->current_result();
	const auto defectComparisonC = windowC->comparison_surface()->current_result();
	const auto defectSelectionB  = windowB->comparison_surface()->selection();
	const auto defectSelectionC  = windowC->comparison_surface()->selection();
	const auto defectScrollB     = windowB->comparison_surface()->shared_scrollbar()->value();
	const auto defectScrollC     = windowC->comparison_surface()->shared_scrollbar()->value();
	windowB->classify_defect_action()->trigger();
	// [[BPR-1211]]. The classification is shown once its write is durable.
	wait_for([&] { return windowB->defect_count() == 1U; }, "the writer's classification is durable");
	const auto defectContext = windowB->defect_card()->context_label()->text();
	expect(defectContext == QStringLiteral("Shared context available at the retained semantic anchor."), "writer classification starts with available retained context");
	wait_for([&] { return windowB->defect_count() == 1U && windowC->defect_count() == 1U; }, "accepted classification reaches the sibling on a different file");
	expect(windowB->defect_card()->shown_defect() && windowC->defect_card()->shown_defect() == windowB->defect_card()->shown_defect(), "sibling displays the exact accepted defect record");
	expect(windowB->defect_card()->context_label()->text() == defectContext
			   && windowC->defect_card()->context_label()->text() == defectContext,
		   "different-file classification delivery preserves accepted anchor availability");
	expect(windowB->defect_card()->reclassify_action()->isEnabled(), "classified defect offers the public reclassify command");
	windowB->defect_card()->severity_selector()->setCurrentIndex(windowB->defect_card()->severity_selector()->findData(static_cast<int>(core::defect_severity::critical)));
	windowB->defect_card()->reclassify_action()->trigger();
	wait_for([&] { return windowB->defect_card()->shown_defect()->Severity == core::defect_severity::critical && windowC->defect_card()->shown_defect() == windowB->defect_card()->shown_defect(); }, "accepted defect command refreshes the sibling card");
	expect(windowB->defect_card()->context_label()->text() == defectContext
			   && windowC->defect_card()->context_label()->text() == defectContext,
		   "different-file command delivery does not correlate accepted context into unavailable history");
	expect(windowB->comparison_surface()->current_result() == defectComparisonB
			   && windowC->comparison_surface()->current_result() == defectComparisonC
			   && windowB->comparison_surface()->selection() == defectSelectionB
			   && windowC->comparison_surface()->selection() == defectSelectionC
			   && windowB->comparison_surface()->shared_scrollbar()->value() == defectScrollB
			   && windowC->comparison_surface()->shared_scrollbar()->value() == defectScrollC,
		   "defect fanout preserves different files, semantic selection, and scroll");
	capture(*windowB, QStringLiteral("defect-writer.png"));
	capture(*windowC, QStringLiteral("defect-sibling-different-file.png"));
	select_file(*owner, *windowC);
	wait_for([&] { return windowC->review_editor()->shown_receipt() && windowC->review_editor()->shown_receipt()->Read == core::file_read_state::seen; }, "returning to the first file loads its accepted shared receipt");
	action(*windowB, "toggleSelectedFileReadAction")->trigger();
	wait_for([&] { return windowB->review_editor()->shown_receipt() && windowC->review_editor()->shown_receipt()
					   && windowB->review_editor()->shown_receipt()->Read == core::file_read_state::read
					   && windowC->review_editor()->shown_receipt() == windowB->review_editor()->shown_receipt(); }, "same-file receipt state is restored before PR replacement");
	// Replace C's review before queued receipt notifications can be delivered.
	expect(windowB->defect_card()->withdraw_action()->isEnabled(), "open defect can be withdrawn before sibling replacement");
	windowB->defect_card()->withdraw_action()->trigger();
	action(*windowB, "toggleSelectedFileReadAction")->trigger();
	submit(*windowC, 17);
	wait_for([&] { return windowC->session_panel()->status() == core::github_status::pull_request_ready; }, "replacement review opens with a sibling receipt notification pending");
	select_file(*owner, *windowC);
	wait_for([&] { return windowB->review_editor()->shown_receipt() && windowB->review_editor()->shown_receipt()->Read == core::file_read_state::seen; }, "surviving PR18 accepts its receipt toggle");
	expect(windowC->comparison_surface()->current_result()->Alignment.Key.PullRequest == snapshotA->PullRequest.PullRequest
			   && windowC->review_editor()->shown_receipt() && windowC->review_editor()->shown_receipt()->Read == core::file_read_state::read
			   && windowC->overview_page()->projection() && windowC->overview_page()->projection()->Progress.PrivateNotes == 2U,
		   "late PR18 receipt delivery cannot replace PR17's accepted receipt or private notes");
	expect(windowC->defect_count() == 0U && windowB->defect_card()->shown_defect()
			   && windowB->defect_card()->shown_defect()->State == core::defect_state::withdrawn,
		   "queued PR18 defect delivery cannot expose old defects in replacement PR17");
	action(*windowB, "quitProfilesAction")->trigger();
	wait_for([&] { return desktop->is_quiescent(); }, "Quit drains all sessions and shared profile services");
	desktop.reset();
	// M4 supplies the same owner-private sink on every shipped platform.
	const auto sinkStatus = [&] {
		expect(adapters::sqlite::create_storage_root_owner_private(root / "diagnostic-sink-probe"), "private sink probe root");
		adapters::logging::quill_log probe{ root / "diagnostic-sink-probe" };
		const auto                   status = probe.status();
		probe.close();
		return status;
	}();
	expect(sinkStatus == application::diagnostic_log_status::available, "this platform reports its diagnostics file sink");
	if (sinkStatus != application::diagnostic_log_status::available) {
		expect(bpr::tests::diagnostic_files(root).empty(), "no diagnostics file is written where the platform has no file sink");
	} else {
		const auto log     = bpr::tests::read_diagnostic_logs(root);
		const auto started = log.find("\"op\":\"application\",\"outcome\":\"started\"");
		const auto stopped = log.find("\"op\":\"application\",\"outcome\":\"shutdown\"");
		expect(started != std::string::npos && log.find("\"op\":\"application\",\"outcome\":\"started\"", started + 1U) == std::string::npos, "one profile services startup");
		expect(stopped != std::string::npos && log.find("\"op\":\"application\",\"outcome\":\"shutdown\"", stopped + 1U) == std::string::npos, "one profile services shutdown");
	}
	std::cout << "profile-multi-window-public-path:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
