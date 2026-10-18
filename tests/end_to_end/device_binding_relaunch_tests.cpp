// [[BPR-0644]]. One process lifetime of a remembered device grant, driven by
// `device_binding_relaunch_contract.py`. The synthetic fixture vault slots
// travel between processes only through this process's standard input and
// standard output pipes; the metadata store is the real SQLite store under the
// root the driver owns.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/app/native_filesystem_path.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QString>
#include <QThread>

#include <array>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {
	using namespace bpr;
	using app::detail::github_application_fixture_access;

	std::string Phase;

	void expect(bool aCondition, std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED(" << Phase << "): " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	void wait_for(const std::function<bool()>& aCondition, std::string_view aMessage, int aTimeoutMs = 15000)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < aTimeoutMs) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}
	auto read_vault() -> std::string
	{
		std::array<char, 2049U> bytes{ };
		std::cin.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
		const auto size = std::cin.gcount();
		expect(size <= 2048, "vault input bounded");
		return { bytes.data(), static_cast<std::size_t>(size) };
	}
	void write_vault(const app::detail::github_public_path_fixture_options& aOptions)
	{
		const auto exported = app::detail::export_github_fixture_vault(aOptions.SharedDeviceVault);
		expect(!exported.empty(), "the retained synthetic vault exports");
		std::cout << exported << '\n'
				  << std::flush;
	}
	void shutdown(app::github_application& aApp)
	{
		aApp.begin_shutdown();
		wait_for([&aApp] { return aApp.status().ShutdownComplete; }, "all composed hosts quiesce");
	}
}

auto main(int aArgc, char** aArgv) -> int
{
	expect(aArgc == 3, "phase and application data root required");
	Phase             = aArgv[1];
	const auto seed   = Phase == "seed";
	const auto locked = Phase == "locked";
	expect(seed || locked || Phase == "quit" || Phase == "terminate" || Phase == "verify", "known phase");
	const auto vault = seed ? std::string{ } : read_vault();

	QApplication                                    qt{ aArgc, aArgv };
	app::detail::github_public_path_fixture_options options;
	options.ApplicationDataRoot = app::native_filesystem_path(QFileInfo{ QString::fromLocal8Bit(aArgv[2]) }.canonicalFilePath());
	expect(adapters::sqlite::make_storage_root_owner_private(*options.ApplicationDataRoot), "private application root under the canonical storage policy");
	options.RegisteredDeviceCourse = true;
	options.PersistDeviceBindings  = true;
	options.LockedDeviceVault      = locked;
	options.SharedDeviceVault      = seed ? app::detail::make_shared_github_fixture_vault() : app::detail::import_github_fixture_vault(vault);
	expect(options.SharedDeviceVault != nullptr, "the retained synthetic vault decodes");

	std::unique_ptr<app::github_application> application;
	auto                                     terminal = false;
	auto                                     startup  = github_application_fixture_access::start(app::detail::github_public_path_fixture::github_com, options, [&](auto aResult) {
		terminal = true;
		if (aResult) {
			application = std::move(*aResult);
		}
	});
	wait_for([&] { return terminal && startup && startup->is_quiescent(); }, "application startup reaches a terminal");
	// [[BPR-0877]]. A refusal here used to say only that startup failed. The
	// profile refusal that actually fired is named beside it, so a seed that
	// cannot start says why in its own output.
	if (application == nullptr) {
		std::cerr << "SEED_STARTUP_PROFILE_REFUSAL:" << bpr::app::profile_startup_refusal_name() << '\n'
				  << std::flush;
		// [[BPR-0882]]. And which composition arm refused, with what it caught.
		std::cerr << "SEED_STARTUP_FAILURE_SOURCE:" << bpr::app::startup_failure_source() << '\n';
	}
	expect(application != nullptr, "application startup succeeds");

	if (seed) {
		application->select_device_authorization();
		wait_for([&application] { return github_application_fixture_access::device_binding_durable(*application); }, "the seeding lifetime persists a settled grant");
	} else if (locked) {
		// [[BPR-0644]]. The owner's rule of 2026-09-24 replaces BPR-0541's
		// fail-closed erasure here: a restored grant whose keychain will not
		// answer fails the session visibly and keeps both slots and its live
		// row, so the next launch tries it again. Only Sign out removes it.
		wait_for([&application] { return application->window().session_panel()->status() == core::github_status::session_failed; },
				 "a locked vault fails the restored session visibly");
		wait_for([&application] { return github_application_fixture_access::device_binding_store_idle(*application); }, "the locked launch completes its metadata work");
		const auto evidence = github_application_fixture_access::evidence(*application);
		expect(evidence.VaultDeleteAttempts == 0U && evidence.RefreshDeleteAttempts == 0U && evidence.SecretRetained && evidence.RefreshRetained,
			   "a locked vault erases neither slot of the restored grant");
		expect(evidence.DeviceTransportSubmissions == 0U && evidence.LastEnrollmentDigest.empty(), "a locked vault spends and prompts for nothing");
		expect(application->window().session_panel()->kept_grant_reason() == std::optional{ core::github_reason_code::vault_unavailable },
			   "the failure says the credential store did not answer and that the grant was kept");
	} else {
		wait_for([&application] { return application->window().session_panel()->status() == core::github_status::session_ready; },
				 "the remembered grant restores the session with hours of access left");
		wait_for([&application] { return application->status().DeviceAuthorizationRefreshResumes == 1U && github_application_fixture_access::device_binding_durable(*application); },
				 "renewal is re-armed and the binding stays durable");
		const auto evidence = github_application_fixture_access::evidence(*application);
		expect(evidence.DeviceTransportSubmissions == 0U && application->status().DeviceAuthorizationRefreshRotations == 0U, "a grant with hours left is resumed, not rotated");
		expect(evidence.LastEnrollmentDigest.empty(), "no pasted credential was taken");
	}

	if (Phase == "terminate") {
		// The driver ends this process with a termination signal once it has
		// read the vault line and the readiness line. No shutdown runs, which
		// is the case the card's observation records.
		write_vault(options);
		std::cout << "device-binding-relaunch:terminate:ready" << '\n'
				  << std::flush;
		wait_for([] { return false; }, "the driver terminates this process", 120000);
		return EXIT_FAILURE;
	}

	shutdown(*application);
	const auto closed = github_application_fixture_access::evidence(*application);
	expect(closed.SecretRetained && closed.RefreshRetained, locked ? "an orderly quit after a locked launch keeps the grant" : "an orderly quit retains the accepted device pair");
	write_vault(options);
	std::cerr << "device-binding-relaunch:" << Phase << ":pass\n";
	return EXIT_SUCCESS;
}
