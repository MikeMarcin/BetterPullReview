// BPR-0968: wire-level refusals traverse the real provider, read host and window.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "private_data_root.hpp"

#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QThread>
#include <QToolButton>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>

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
	const auto    requireBackoff = aArgc > 1 && std::string_view{ aArgv[1] } == "--require-backoff";
	QTemporaryDir temporary;
	expect(temporary.isValid(), "rate-limit private test root");
	const auto root = e2e::private_data_root(temporary);
	expect(root.has_value(), "owner-private data root");
	auto limited                      = std::make_shared<app::detail::github_rate_limit_fixture>();
	auto options                      = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot       = *root;
	options.RegisteredDeviceCourse    = true;
	options.RepeatReadSessions        = true;
	options.RateLimit                 = limited;
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
	auto backoffHeld = true;
	for (const auto mode : { 1U, 2U }) {
		limited->Mode     = mode;
		const auto before = limited->Responses.load();
		window->github_refresh_action()->trigger();
		wait_for([&] { return limited->Responses.load() == before + 1U && window->session_panel()->status() == core::github_status::pull_request_failed; }, "wire refusal reaches failed read");
		const auto firstResponseAt = limited->LastResponseNanoseconds.load();
		const auto diagnostic      = window->session_panel()->diagnostic_text();
		std::cout << "mode=" << mode << " diagnostic=" << diagnostic << '\n';
		expect(diagnostic.find(mode == 1U ? "rate limit reached" : "temporarily limited") != std::string::npos && diagnostic.find("2 seconds") != std::string::npos, "distinct reason and Retry-After projection");
		QApplication::processEvents();
		const auto rendered = visible_text(*window, QString::fromStdString(diagnostic));
		std::cout << "mode=" << mode << " visible_label=" << rendered << " refresh_enabled=" << window->github_refresh_action()->isEnabled() << '\n';
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
			expect(image.save(renderRoot + QStringLiteral("/rate-limit-%1.png").arg(mode)), "rate-limit raster saved");
		}
		window->settings()->close();
		QElapsedTimer interval;
		interval.start();
		window->github_refresh_action()->trigger();
		settle(250);
		const auto requests = limited->Responses.load() - before;
		backoffHeld         = backoffHeld && requests == 1U;
		std::cout << "mode=" << mode << " responses_within_ms=" << interval.elapsed() << " count=" << requests << " transport_interval_us=" << (limited->LastResponseNanoseconds.load() - firstResponseAt) / 1000 << " retry_after_ms=2000\n";
		limited->Mode = 0U;
		settle(2100);
		window->github_refresh_action()->trigger();
		wait_for([&] { return window->session_panel()->status() == core::github_status::pull_request_ready; }, "explicit retry recovers", [&] {
            const auto evidence = app::detail::github_application_fixture_access::evidence(*owner);
            std::cerr << "status=" << window->session_panel()->status_text() << " diagnostic=" << window->session_panel()->diagnostic_text() << " recording_failure=" << evidence.RecordingFailure << " last_operation=" << evidence.LastReadOperation << '\n'; });
		const auto recovered = app::detail::github_application_fixture_access::large_review_snapshot(*owner);
		expect(recovered.has_value() && recovered->PullRequest == snapshot->PullRequest && recovered->Repository == snapshot->Repository && recovered->Viewer == snapshot->Viewer && recovered->Files == snapshot->Files && recovered->ReviewThreads == snapshot->ReviewThreads, "recovery retains exact review identity and workload");
	}
	const auto finalEvidence = app::detail::github_application_fixture_access::evidence(*owner);
	expect(finalEvidence.RecordingFailure == -1, "all non-rate fixture requests match the exact recording");
	expect(app::detail::github_application_fixture_access::review_mutation_evidence(*owner).Submissions == 0U, "read recovery publishes no mutation");
	std::cout << "normal_account_requests=" << finalEvidence.AccountReadCount << " limited_requests=" << limited->Responses.load() << " elapsed_ms=" << total.elapsed() << '\n';
	owner->begin_shutdown();
	wait_for([&] { return owner->status().ShutdownComplete; }, "rate-limit course drains");
	std::cout << "rate-limit-public-path:render-and-recovery-pass backoff=" << backoffHeld << '\n';
	expect(!requireBackoff || backoffHeld, "Retry-After prevents an immediate second transport request");
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
