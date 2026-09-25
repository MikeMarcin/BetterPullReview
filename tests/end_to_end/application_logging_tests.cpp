#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "private_data_root.hpp"
#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QThread>
#include <algorithm>
#include <cctype>
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
	QApplication  qt{ aArgc, aArgv };
	QTemporaryDir temporary;
	expect(temporary.isValid(), "private test root");
	const auto root = bpr::e2e::private_data_root(temporary).value_or(std::filesystem::path{ });
	expect(!root.empty(), "native private root before composed startup");
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
		wait_for([&] { return startupTerminal && startup && startup->is_quiescent(); });
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
	std::ifstream     stream{ root / "logs/diagnostics.log" };
	const std::string text{ std::istreambuf_iterator<char>{ stream }, { } };
	for (const auto* marker : { "op=application outcome=started", "op=github.viewer outcome=succeeded", "op=github.capability", "op=application outcome=shutdown", "epoch_stage=cache_open failure_domain=cache failure=invalid_path" }) {
		expect(text.find(marker) != std::string::npos, "startup/provider/shutdown persisted through real composition");
	}
	expect(text.find("op=github.pull_request outcome=started") != std::string::npos
			   && text.find("op=github.pull_request outcome=response_received") != std::string::npos
			   && text.find("http_status=200") != std::string::npos
			   && text.find("page_operation=ListPullRequestFiles") != std::string::npos
			   && text.find("op=github.pull_request outcome=succeeded") != std::string::npos,
		   "recorded fixture load emits typed paged responses and accepted outcome in composed log");
	expect(text.find("op=github.pull_request outcome=succeeded") == text.rfind("op=github.pull_request outcome=succeeded"),
		   "one accepted load has one succeeded entry");
	expect(text.find("access_token") == std::string::npos && text.find("refresh_token") == std::string::npos, "authentication bodies absent");
	expect(text.find(root.string()) == std::string::npos && text.find("octo-org") == std::string::npos && text.find("synthetic unsafe cache target") == std::string::npos, "epoch logs exclude path and repository data");
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
