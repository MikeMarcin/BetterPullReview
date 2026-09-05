#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QLineEdit>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QThread>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
namespace {
	void expect(bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			std::cerr << aMessage << '\n';
			std::exit(1);
		}
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
{
	QApplication  qt{ aArgc, aArgv };
	QTemporaryDir temporary;
	expect(temporary.isValid(), "private test root");
	const auto                                           root = std::filesystem::path{ QFileInfo{ temporary.path() }.canonicalFilePath().toStdString() };
	bpr::app::detail::github_public_path_fixture_options options;
	options.ApplicationDataRoot    = root;
	options.RegisteredDeviceCourse = true;
	{
		auto app = bpr::app::detail::github_application_fixture_access::make(bpr::app::detail::github_public_path_fixture::github_com, options);
		app->select_device_authorization();
		wait_for([&app] { return app->window().session_panel()->status() == bpr::core::github_status::session_ready; });

		expect(!std::filesystem::exists(root / "locks"), "isolated cache lock directory not opened before PR");
		{
			std::ofstream blocker{ root / "locks" };
			blocker << "synthetic unsafe cache target";
		}
		auto* panel = app->window().session_panel();
		panel->owner_editor()->setText("octo-org");
		panel->repository_editor()->setText("review-repository");
		panel->pull_request_editor()->setValue(17);
		app->window().github_open_action()->trigger();
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
	expect(text.find("access_token") == std::string::npos && text.find("refresh_token") == std::string::npos, "authentication bodies absent");
	expect(text.find(root.string()) == std::string::npos && text.find("octo-org") == std::string::npos && text.find("synthetic unsafe cache target") == std::string::npos, "epoch logs exclude path and repository data");
	std::cout << "application-logging-tests:pass\n";
}
