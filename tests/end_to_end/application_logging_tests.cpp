#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../../src/app/src/terminal_diagnostics.hpp"
#include "../support/diagnostic_files.hpp"
#include "../support/diagnostic_json.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/logging/quill_log.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "private_data_root.hpp"
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QThread>
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
namespace {
	using bpr::tests::expect;
	[[nodiscard]] auto window_session(std::string_view aRecord) -> std::uint64_t
	{
		constexpr std::string_view field    = "\"window_session\":";
		const auto                 position = aRecord.find(field);
		expect(position != std::string_view::npos, "diagnostic record has a window session");
		const auto    value   = aRecord.substr(position + field.size());
		std::uint64_t session = 0U;
		const auto    parsed  = std::from_chars(value.data(), value.data() + value.size(), session);
		expect(parsed.ec == std::errc{ } && session != 0U
				   && (parsed.ptr == value.data() + value.size() || *parsed.ptr == ',' || *parsed.ptr == '}'),
			   "diagnostic session is a nonzero numeric identity");
		return session;
	}

	void expect_epoch_session(const std::string& aText)
	{
		const auto accepted = aText.find("\"op\":\"github.pull_request\",\"outcome\":\"succeeded\"");
		expect(accepted != std::string::npos, "a successful PR read precedes epoch preparation");
		const auto  session = window_session(std::string_view{ aText }.substr(accepted, aText.find('\n', accepted) - accepted));
		std::size_t count   = 0U;
		for (auto position = aText.find("\"op\":\"epoch\","); position != std::string::npos; position = aText.find("\"op\":\"epoch\",", position + 1U)) {
			const auto record = std::string_view{ aText }.substr(position, aText.find('\n', position) - position);
			expect(window_session(record) == session, "every composed epoch diagnostic belongs to the accepted PR window");
			++count;
		}
		expect(count != 0U, "the composed cache refusal emits epoch diagnostics");
	}

	void wait_for(const std::function<bool()>& aCondition)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < 15000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), "composed logging course timed out");
	}
}
int main(int aArgc, char** aArgv)
try {
	bpr::app::detail::terminal_diagnostics terminalDiagnostics;
	QApplication                           qt{ aArgc, aArgv };
	QTemporaryDir                          temporary{ QDir::tempPath() + QStringLiteral("/BPR_DIAGNOSTIC_PRIVATE_CANARY-XXXXXX") };
	expect(temporary.isValid(), "private test root");
	const auto root = bpr::e2e::private_data_root(temporary).value_or(std::filesystem::path{ });
	expect(!root.empty(), "native private root before composed startup");
	const auto                                         scenario = aArgc == 2 ? std::string_view{ aArgv[1] } : std::string_view{ };
	std::unique_ptr<bpr::adapters::logging::quill_log> protectedLog;
	std::filesystem::path                              protectedFile;
	if (scenario == "--recovery-log") {
		std::ofstream registry{ root / "agent-workspace-ownership-v1" };
		registry << "BPR_DIAGNOSTIC_PRIVATE_CANARY malformed synthetic registry";
		registry.close();
		std::filesystem::permissions(root / "agent-workspace-ownership-v1", std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, std::filesystem::perm_options::replace);
	} else if (scenario == "--full-log") {
		protectedLog = std::make_unique<bpr::adapters::logging::quill_log>(root);
		expect(protectedLog->status() == bpr::application::diagnostic_log_status::available, "protected concurrent session admitted");
		protectedFile = bpr::tests::diagnostic_files(root).front();
		// An active foreign session consumes the entire aggregate allowance.
		std::filesystem::resize_file(protectedFile, 50U * 1024U * 1024U);
	} else if (scenario == "--unavailable-log") {
		std::ofstream unavailable{ root / "logs" };
		unavailable << "synthetic logger obstruction";
	}

	bpr::app::detail::github_public_path_fixture_options options;
	options.ApplicationDataRoot    = root;
	options.RegisteredDeviceCourse = true;
	{
		std::unique_ptr<bpr::app::github_application> app;
		auto                                          startupTerminal = false;
		auto                                          startup         = bpr::app::detail::github_application_fixture_access::start(bpr::app::detail::github_public_path_fixture::github_com, options, [&](auto aResult) {
			startupTerminal = true;
			if (aResult) {
				app = std::move(*aResult);
			}
		});
		if (scenario == "--cancel-startup") {
			startup->cancel();
		}
		wait_for([&] { return startupTerminal && startup && startup->is_quiescent(); });
		if (scenario == "--cancel-startup") {
			expect(!app, "cancelled startup cannot publish an application");
			std::cout << "application-logging-cancel:pass\n";
			return EXIT_SUCCESS;
		}
		expect(app != nullptr, "application startup succeeds");
		app->select_device_authorization();
		wait_for([&app] { return app->window().session_panel()->status() == bpr::core::github_status::session_ready; });

		expect(!std::filesystem::exists(root / "locks"), "isolated cache lock directory not opened before PR");
		{
			std::ofstream blocker{ root / "locks" };
			blocker << "synthetic unsafe cache target";
		}
		// [[BPR-0771]]. A pull request is named in the Home bar; the locator is retired.
		app->window().home_open_bar()->setText(QStringLiteral("octo-org/review-repository#17"));
		app->window().home_open_action()->trigger();
		wait_for([&app] { return app->status().EpochStatus == bpr::core::epoch_status::failed || app->status().EpochStatus == bpr::core::epoch_status::unavailable; });
		expect(!app->status().DisplayedComparison, "failed cache preparation cannot display a comparison");
		app->begin_shutdown();
		wait_for([&app] { return app->status().ShutdownComplete; });
	}
	if (!scenario.empty() && scenario != "--recovery-log") {
		expect(scenario == "--full-log" || scenario == "--unavailable-log", "known failure scenario");
		expect(scenario == "--full-log" ? std::filesystem::file_size(protectedFile) == 50U * 1024U * 1024U : std::filesystem::is_regular_file(root / "logs"), "refused diagnostic target is unchanged after authenticated workflow and shutdown");
		std::cout << "application-logging-failure-isolation:pass\n";
		return EXIT_SUCCESS;
	}
	const auto text = bpr::tests::read_diagnostic_logs(root);
	for (const auto* marker : { "\"op\":\"application\",\"outcome\":\"started\"", "\"op\":\"github.viewer\",\"outcome\":\"succeeded\"", "\"op\":\"github.capability\"", "\"op\":\"application\",\"outcome\":\"shutdown\"", "\"epoch_stage\":\"cache_open\",\"failure_domain\":\"cache\",\"failure\":\"invalid_path\"" }) {
		expect(text.find(marker) != std::string::npos, "startup/provider/shutdown persisted through real composition");
	}
	expect(text.find("\"op\":\"github.pull_request\",\"outcome\":\"started\"") != std::string::npos
			   && text.find("\"op\":\"github.pull_request\",\"outcome\":\"response_received\"") != std::string::npos
			   && text.find("\"http_status\":200") != std::string::npos
			   && text.find("\"page_operation\":\"ListPullRequestFiles\"") != std::string::npos
			   && text.find("\"op\":\"github.pull_request\",\"outcome\":\"succeeded\"") != std::string::npos,
		   "recorded fixture load emits typed paged responses and accepted outcome in composed log");
	expect(text.find("\"op\":\"github.pull_request\",\"outcome\":\"succeeded\"") == text.rfind("\"op\":\"github.pull_request\",\"outcome\":\"succeeded\""),
		   "one accepted load has one succeeded entry");
	expect(text.find("access_token") == std::string::npos && text.find("refresh_token") == std::string::npos, "authentication bodies absent");
	expect(text.find(root.string()) == std::string::npos && text.find("octo-org") == std::string::npos && text.find("synthetic unsafe cache target") == std::string::npos, "epoch logs exclude path and repository data");
	expect_epoch_session(text);
	const auto records = bpr::tests::diagnostic_json_records(text);
	expect(std::ranges::all_of(records, [&](const auto& aRecord) { return aRecord.value("session") == records.front().value("session"); }), "the profile owns one logger session from startup through shutdown");
	// [[BPR-0891]]. The native spelling above is not the only one a log could
	// use: a Windows path can also be written with forward slashes or in
	// another case. The root's random leaf is unique to this run, so finding it
	// in any case finds the root however it was spelled. The repository is
	// looked for by its own name as well as its owner's.
	const auto folded = [](std::string aText) {
		std::ranges::transform(aText, aText.begin(), [](const unsigned char aCharacter) {
			return static_cast<char>(std::tolower(aCharacter));
		});
		return aText;
	};
	const auto leaf = folded(root.filename().string());
	expect(!leaf.empty() && folded(text).find(leaf) == std::string::npos, "logs exclude the data root however it is spelled");
	expect(text.find("review-repository") == std::string::npos, "logs exclude the repository's own name");
	std::cout << "application-logging-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
