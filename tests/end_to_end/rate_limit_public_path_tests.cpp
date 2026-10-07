// BPR-0968: wire-level refusals traverse the real provider, read host and window.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "private_data_root.hpp"

#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPixmap>
#include <QPushButton>
#include <QScrollBar>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QThread>
#include <QToolButton>

#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <thread>

namespace {
	using namespace bpr;
	using tests::expect;

	void wait_for(const std::function<bool()>& aCondition, const char* aMessage, const std::function<void()>& aDiagnostic = { })
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < 30000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		if (!aCondition() && aDiagnostic) {
			aDiagnostic();
		}
		expect(aCondition(), aMessage);
	}

	void settle(const int aMilliseconds)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (elapsed.elapsed() < aMilliseconds) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
	}

	auto visible_text(QWidget& aWindow, const QString& aText) -> bool
	{
		for (auto* label : aWindow.findChildren<QLabel*>()) {
			if (label->isVisible() && label->text().contains(aText)) {
				return true;
			}
		}
		return false;
	}
}

auto main(int aArgc, char** aArgv) -> int
try {
	std::cout.setf(std::ios::unitbuf);
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	QElapsedTimer total;
	total.start();
	std::cout << "platform=" << QSysInfo::productType().toStdString() << " version=" << QSysInfo::productVersion().toStdString() << " arch=" << QSysInfo::currentCpuArchitecture().toStdString() << " qt=" << qVersion() << " backend=" << QGuiApplication::platformName().toStdString() << " files=515 threads=1000\n";
	expect(aArgc == 1 || (aArgc == 2 && std::string_view{ aArgv[1] } == "--require-backoff"), "only the named backoff requirement mode is accepted");

	QTemporaryDir temporary;
	expect(temporary.isValid(), "rate-limit private test root");
	const auto root = e2e::private_data_root(temporary);
	expect(root.has_value(), "owner-private data root");
	auto limited                   = std::make_shared<app::detail::github_rate_limit_fixture>();
	auto options                   = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot    = *root;
	options.RegisteredDeviceCourse = true;
	options.RepeatReadSessions     = true;
	options.RateLimit              = limited;
	auto holdRead                  = std::atomic_bool{ false };
	auto readEntered               = std::atomic_bool{ false };
	auto readCancelled             = std::atomic_bool{ false };
	options.BeforeFilesPage        = [&](std::stop_token aStop) {
		if (!holdRead.load()) {
			return;
		}
		readEntered = true;
		while (holdRead.load() && !aStop.stop_requested()) {
			std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
		}
		readCancelled = aStop.stop_requested();
	};
	app::github_application* owner    = nullptr;
	auto                     terminal = false;
	auto                     desktop  = app::detail::github_application_fixture_access::start_desktop(
		app::detail::github_public_path_fixture::large_review_v1, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				owner = *aResult;
			}
		});
	wait_for([&] { return terminal; }, "profile startup terminal");
	expect(owner != nullptr, "rate-limit profile admitted");
	owner->show();
	wait_for([&] { return owner->windows().size() == 1U; }, "one public window");
	auto* window = owner->window(owner->windows().front());
	expect(window != nullptr, "window exists");
	window->resize(1500, 950);
	window->home_landing()->primary_button()->click();
	wait_for([&] { return window->session_panel()->status() == core::github_status::session_ready; }, "safe fixture sign-in");
	window->home_open_bar()->setText(QStringLiteral("https://github.com/bpr-fixtures/large-review/pull/17"));
	window->home_open_action()->trigger();
	wait_for([&] { return window->session_panel()->status() == core::github_status::pull_request_ready; }, "large review ready");
	const auto snapshot = app::detail::github_application_fixture_access::large_review_snapshot(*owner);
	expect(snapshot.has_value() && snapshot->Files.size() == 515U && snapshot->ReviewThreads.size() == 1000U, "exact large review input");
	wait_for([&] { return owner->status().EpochStatus == core::epoch_status::ready; }, "real Git epoch ready");
	window->modality()->action(qt::review_modality::review)->trigger();
	const auto file  = core::file_identity_of("large/comparison.cpp");
	auto*      files = window->files_drawer();
	wait_for([&] { return files->contents()->index_for_file(file).isValid(); }, "large file appears in actual drawer");
	files->contents_view()->selectionModel()->setCurrentIndex(files->contents()->index_for_file(file), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
	auto* surface = window->comparison_surface();
	wait_for([&] { return surface->current_result() && surface->current_result()->Alignment.Key.File == file && surface->current_semantic_index() && !surface->geometry_pending() && surface->isVisible(); }, "20,000-line comparison renders through installed Git");
	expect(surface->select_conversation_line(core::diff_side::right, 100U), "exact line is selectable");
	expect(surface->shared_scrollbar()->maximum() > 0, "course has real scroll extent");
	surface->shared_scrollbar()->setValue(surface->shared_scrollbar()->maximum() / 3);
	settle(50);
	const auto selected   = surface->selection();
	const auto comparison = surface->current_result()->Alignment.Key;
	const auto scroll     = surface->shared_scrollbar()->value();
	expect(selected.has_value() && scroll > 0, "nontrivial semantic selection and scroll captured");
	const auto exactReading = [&] {
		const auto exact = surface->current_result() && surface->current_result()->Alignment.Key == comparison
			&& surface->selection() == selected && surface->shared_scrollbar()->value() == scroll;
		if (!exact) {
			std::cerr << "reading-mismatch:comparison=" << (surface->current_result() && surface->current_result()->Alignment.Key == comparison)
					  << " selection=" << (surface->selection() == selected)
					  << " selected-line=" << (surface->selection() ? surface->selection()->PreferredLine.Line : 0U)
					  << " scroll=" << surface->shared_scrollbar()->value() << " expected-scroll=" << scroll << '\n';
		}
		return exact;
	};
	const auto readingQuiet = [&] {
		const auto work = app::detail::github_application_fixture_access::diff_work_evidence(*owner);
		return work.NavigationReady && !work.TabPresentationPending && work.WorkbenchOutstandingRequests == 0U && work.EpochOutstandingJobs == 0U && work.OutstandingJobs == 0U;
	};
	const auto firstContext = owner->windows().front();
	auto       backoffHeld  = true;
	for (const auto scenario : { 1U, 2U, 3U }) {
		const auto mode            = scenario == 3U ? 1U : scenario;
		limited->RetryAfterSeconds = scenario == 3U ? 1U : 2U;
		limited->ResetAfterSeconds = scenario == 3U ? 4U : 1U;
		auto* newWindow            = window->findChild<QAction*>(QStringLiteral("newBprWindowAction"));
		expect(newWindow && newWindow->isEnabled(), "public New Window action");
		newWindow->trigger();
		wait_for([&] { return owner->windows().size() == 2U; }, "sibling window shares profile runtime");
		const auto contexts       = owner->windows();
		const auto siblingContext = contexts.front() == firstContext ? contexts.back() : contexts.front();
		auto*      sibling        = owner->window(siblingContext);
		expect(sibling != nullptr, "sibling exists");
		const auto ordinaryBefore = app::detail::github_application_fixture_access::evidence(*owner).AccountReadCount;
		limited->Mode             = mode;
		const auto before         = limited->Responses.load();
		const auto requestsBefore = limited->Requests.load();
		window->github_refresh_action()->trigger();
		wait_for([&] { return limited->Responses.load() == before + 1U && window->session_panel()->status() == core::github_status::pull_request_failed; }, "wire refusal reaches failed read");
		const auto firstResponseAt = limited->LastResponseNanoseconds.load();
		expect(exactReading(), "rate refusal preserves selected comparison, line and scroll");
		const auto diagnostic = window->session_panel()->diagnostic_text();
		std::cout << "scenario=" << scenario << " mode=" << mode << " diagnostic=" << diagnostic << '\n';
		expect(diagnostic.find(mode == 1U ? "rate limit reached" : "temporarily limited") != std::string::npos && (scenario == 3U || diagnostic.find("2 seconds") != std::string::npos), "distinct reason and Retry-After projection");
		QApplication::processEvents();
		const auto rendered = visible_text(*window, QString::fromStdString(diagnostic));
		std::cout << "scenario=" << scenario << " mode=" << mode << " visible_label=" << rendered << " refresh_enabled=" << window->github_refresh_action()->isEnabled() << '\n';
		window->account_action()->trigger();
		auto* card = window->server_card_for(*window->session_panel()->active_profile());
		expect(card != nullptr, "the accepted host has a server card");
		if (!card->details_button()->isChecked()) {
			card->details_button()->click();
		}
		wait_for([&] { return card->last_problem_label()->isVisible(); }, "account details displays the refusal");
		expect(card->last_problem_label()->text().contains(QString::fromStdString(diagnostic)), "details renders the exact diagnostic");
		const auto image = window->settings()->grab();
		expect(!image.isNull(), "full window rendered");
		const auto renderRoot = qEnvironmentVariable("BPR_RATE_LIMIT_RENDER_DIR");
		if (!renderRoot.isEmpty()) {
			expect(image.save(renderRoot + QStringLiteral("/rate-limit-%1.png").arg(scenario)), "rate-limit raster saved");
		}
		window->settings()->close();
		QElapsedTimer interval;
		interval.start();
		for (int click = 0; click < 10; ++click) {
			window->github_refresh_action()->trigger();
			sibling->home_open_bar()->setText(QStringLiteral("https://github.com/bpr-fixtures/large-review/pull/17"));
			sibling->home_open_action()->trigger();
		}
		settle(250);
		const auto requests = limited->Responses.load() - before;
		backoffHeld         = backoffHeld && requests == 1U;
		std::cout << "scenario=" << scenario << " mode=" << mode << " responses_within_ms=" << interval.elapsed() << " count=" << requests << " transport_interval_us=" << (limited->LastResponseNanoseconds.load() - firstResponseAt) / 1000 << " retry_after_ms=" << limited->RetryAfterSeconds * 1000 << " reset_after_ms=" << limited->ResetAfterSeconds * 1000 << "\n";
		expect(requests == 1U && app::detail::github_application_fixture_access::evidence(*owner).AccountReadCount == ordinaryBefore, "twenty explicit Refresh/Open actions spend no wire request during cooldown");
		expect(exactReading(), "blocked retries preserve exact reading position");
		if (scenario == 3U) {
			settle(1250);
			window->github_refresh_action()->trigger();
			expect(limited->Requests.load() == requestsBefore + 1U, "primary reset still gates after shorter Retry-After has passed");
		}
		// Closing a window is the public cancellation gesture for its read lane.
		// No request or retry may survive to run at the deadline.
		sibling->close();
		wait_for([&] { return owner->windows().size() == 1U; }, "closed waiting window retires its read authority");
		limited->Mode = 0U;
		settle(scenario == 3U ? 4100 : 2100);
		expect(limited->Responses.load() == before + 1U && app::detail::github_application_fixture_access::evidence(*owner).AccountReadCount == ordinaryBefore, "zero automatic retries after deadline or window cancellation");
		window->github_refresh_action()->trigger();
		wait_for([&] { return window->session_panel()->status() == core::github_status::pull_request_ready; }, "explicit retry recovers", [&] {
            const auto evidence = app::detail::github_application_fixture_access::evidence(*owner);
            std::cerr << "status=" << window->session_panel()->status_text() << " diagnostic=" << window->session_panel()->diagnostic_text() << " recording_failure=" << evidence.RecordingFailure << " last_operation=" << evidence.LastReadOperation << '\n'; });
		wait_for([&] { return owner->status().EpochStatus == core::epoch_status::ready && !surface->geometry_pending() && readingQuiet(); }, "recovered comparison settles");
		expect(exactReading(), "successful same-head recovery preserves exact comparison, line selection and scroll");
		expect(!window->status_text().contains(QStringLiteral("not ready")), "settled recovery status agrees with the available comparison");
		const auto wireInterval = (limited->LastRequestNanoseconds.load() - firstResponseAt) / 1000;
		expect(limited->Requests.load() == requestsBefore + 2U && wireInterval >= static_cast<std::int64_t>((scenario == 3U ? 3U : 2U) * 1000000U), "one explicit recovery request arrives after the effective minimum");
		std::cout << "scenario=" << scenario << " mode=" << mode << " wire_recovery_interval_us=" << wireInterval << " recovery_since_refusal_us=" << (std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() - firstResponseAt) / 1000 << " blocked_gestures=20 auto_retries=0 selection_line=100 scroll=" << scroll << '\n';
		const auto recoveredRenderRoot = qEnvironmentVariable("BPR_RATE_LIMIT_RENDER_DIR");
		if (!recoveredRenderRoot.isEmpty()) {
			expect(window->grab().save(recoveredRenderRoot + QStringLiteral("/recovered-reading-%1.png").arg(scenario)), "full rendered selected recovery saved");
		}
		const auto recovered = app::detail::github_application_fixture_access::large_review_snapshot(*owner);
		expect(recovered.has_value() && recovered->PullRequest == snapshot->PullRequest && recovered->Repository == snapshot->Repository && recovered->Viewer == snapshot->Viewer && recovered->Files == snapshot->Files && recovered->ReviewThreads == snapshot->ReviewThreads, "recovery retains exact review identity and workload");
	}
	auto* newWindow = window->findChild<QAction*>(QStringLiteral("newBprWindowAction"));
	newWindow->trigger();
	wait_for([&] { return owner->windows().size() == 2U; }, "cancellation sibling created");
	const auto contexts         = owner->windows();
	const auto cancelledContext = contexts.front() == firstContext ? contexts.back() : contexts.front();
	auto*      cancelledWindow  = owner->window(cancelledContext);
	holdRead                    = true;
	cancelledWindow->home_open_bar()->setText(QStringLiteral("https://github.com/bpr-fixtures/large-review/pull/17"));
	cancelledWindow->home_open_action()->trigger();
	wait_for([&] { return readEntered.load(); }, "actual provider blocks at files page");
	cancelledWindow->close();
	wait_for([&] { return readCancelled.load() && owner->windows().size() == 1U; }, "public close cancels real transport and retires window");
	holdRead = false;
	settle(50);
	expect(exactReading(), "cancelled sibling cannot replace surviving window's selection or scroll");
	std::cout << "held_transport_cancelled=1 retired_window_absent=1 surviving_selection_exact=1\n";
	const auto finalEvidence = app::detail::github_application_fixture_access::evidence(*owner);
	expect(finalEvidence.RecordingFailure == -1, "all non-rate fixture requests match the exact recording");
	expect(app::detail::github_application_fixture_access::review_mutation_evidence(*owner).Submissions == 0U, "read recovery publishes no mutation");
	std::cout << "normal_account_requests=" << finalEvidence.AccountReadCount << " repository_requests=" << limited->Requests.load() << " limited_requests=" << limited->Responses.load() << " elapsed_ms=" << total.elapsed() << '\n';
	owner->begin_shutdown();
	wait_for([&] { return owner->status().ShutdownComplete; }, "rate-limit course drains");
	std::cout << "rate-limit-public-path:full-recovery-pass backoff=" << backoffHeld << '\n';
	expect(backoffHeld, "Retry-After prevents an immediate second transport request");
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
