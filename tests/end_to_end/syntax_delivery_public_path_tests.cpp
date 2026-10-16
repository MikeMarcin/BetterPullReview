// [[BPR-1558]]. Both comparison panes end classified, whatever order the
// classification and the diff arrive in, through the shipped composition.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/files_tree.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QListView>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextLayout>
#include <QThread>

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;
	using tests::expect;
	using access = app::detail::github_application_fixture_access;

	void wait_for(const std::function<bool()>& aCondition, const std::string& aMessage)
	{
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < 20'000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}

	auto shown(const qt::workbench_window& aWindow, const std::string_view aPath) -> bool
	{
		const auto* surface = aWindow.comparison_surface();
		return surface->current_result() && surface->current_context()
			&& surface->current_context()->Key.File == core::file_identity_of(aPath) && !surface->geometry_pending();
	}

	void open(app::github_application& aApplication, const std::string_view aPath)
	{
		auto&      window = aApplication.window();
		const auto index  = window.files_drawer()->contents()->index_for_file(core::file_identity_of(aPath));
		expect(index.isValid(), "requested path belongs to the accepted catalogue");
		window.files_drawer()->contents_view()->setCurrentIndex(index);
		wait_for([&] { return shown(window, aPath); }, "Git/cache comparison displays " + std::string{ aPath });
	}

	/// Every classification the host was asked for has answered.
	auto classification_settled(const app::github_application& aApplication) -> bool
	{
		const auto status = aApplication.status();
		return access::held_syntax_blob_reads(aApplication) == 0U && status.ClassificationTerminals == status.ClassificationSubmissions;
	}

	/// Whether a line on screen in the pane carries the classifier's colours.
	auto painted(QPlainTextEdit& aPane) -> bool
	{
		const auto last = aPane.cursorForPosition(QPoint{ 0, aPane.viewport()->height() }).block().blockNumber();
		for (auto block = aPane.cursorForPosition(QPoint{ 0, 0 }).block(); block.isValid() && block.blockNumber() <= last; block = block.next()) {
			if (block.layout() != nullptr && !block.layout()->formats().isEmpty()) {
				return true;
			}
		}
		return false;
	}

	/// \details `aColoured` asks for painted spans as well. A file of plain
	/// identifiers classifies to an empty list, which colours nothing.
	void expect_both_classified(app::github_application& aApplication, const std::string_view aPath, const std::string_view aCourse, const bool aColoured)
	{
		auto& window = aApplication.window();
		wait_for([&] { return classification_settled(aApplication); }, std::string{ aCourse } + ": every classification answered");
		QApplication::processEvents();
		const auto  status  = aApplication.status();
		auto* const surface = window.comparison_surface();
		std::cout << "syntax-delivery " << aCourse << " left=" << status.DisplayedLeftSpans << " right=" << status.DisplayedRightSpans
				  << " submissions=" << status.ClassificationSubmissions << " coalesced=" << status.CoalescedClassifications
				  << " failures=" << status.ClassificationFailures << " adopted=" << status.AdoptedClassifications
				  << " dropped=" << status.DroppedClassifications << '\n';
		expect(shown(window, aPath), std::string{ aCourse } + ": the comparison is still the one on screen");
		expect(status.DisplayedLeftClassified, std::string{ aCourse } + ": the left pane carries its classification");
		expect(status.DisplayedRightClassified, std::string{ aCourse } + ": the right pane carries its classification");
		if (aColoured) {
			expect(status.DisplayedLeftSpans > 0U && status.DisplayedRightSpans > 0U, std::string{ aCourse } + ": both classifications carry spans");
			expect(painted(*surface->left_pane()) && painted(*surface->right_pane()), std::string{ aCourse } + ": both panes' text layouts carry syntax colours");
		}
	}
}

int main(int aArgc, char** aArgv)
{
	QApplication  qtApplication{ aArgc, aArgv };
	QTemporaryDir temporary{ QDir::tempPath() + QStringLiteral("/bpr-syntax-delivery-XXXXXX") };
	const auto    dataRoot = e2e::private_data_root(temporary);
	expect(dataRoot.has_value(), "private course root");
	auto options                = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot = dataRoot;
	std::unique_ptr<app::github_application> application;
	bool                                     terminal = false;
	auto                                     startup  = access::start(app::detail::github_public_path_fixture::source_pr_search_v1, options, [&](auto aResult) {
		terminal = true;
		if (aResult) {
			application = std::move(*aResult);
		}
	});
	wait_for([&] { return terminal && startup->is_quiescent(); }, "composed startup terminal");
	expect(application != nullptr, "composition starts");
	application->show();
	auto& window = application->window();
	window.resize(1280, 860);
	window.github_connect_action()->trigger();
	wait_for([&] { return window.github_open_action()->isEnabled(); }, "fixture account connects");
	window.home_open_bar()->setText(QStringLiteral("bpr-fixtures/source-pr-search#78"));
	window.home_open_action()->trigger();
	wait_for([&] { return window.showing_overview() && window.overview_page()->projection().has_value(); }, "pull request opens on Overview");
	auto step  = app::detail::course_modality_step{ };
	step.Needs = app::detail::course_surfaces{ .Comparison = true, .FilesDrawer = true };
	wait_for([&] { return access::enter_course_modality(*application, step) != app::detail::course_modality_result::pending; }, "enters Review");

	// A pure rename: both sides are the same blob, classified once. The
	// classification answers after the diff is on screen, so it reaches the
	// panes through its own publication and not through the display's cache
	// read, and that one publication colours both panes.
	access::hold_syntax_blob_reads(*application, true);
	open(*application, "src/renamed.cpp");
	wait_for([&] { return access::held_syntax_blob_reads(*application) == 1U; }, "identical sides: the one classification is reading");
	{
		const auto& key = window.comparison_surface()->current_context()->Key;
		expect(key.LeftBlob.has_value() && key.LeftBlob == key.RightBlob, "the pure rename compares one blob with itself");
	}
	expect(!application->status().DisplayedLeftClassified && !application->status().DisplayedRightClassified, "identical sides: neither pane is classified before the read answers");
	access::hold_syntax_blob_reads(*application, false);
	expect_both_classified(*application, "src/renamed.cpp", "identical-sides", false);
	expect(window.comparison_surface()->token_spans(core::diff_side::left) == window.comparison_surface()->token_spans(core::diff_side::right), "identical sides: both panes share the one immutable classification");

	// The reviewer leaves a file whose classification is still reading and
	// comes back to it before the left read could answer. The departure ends
	// that read; the return must classify the file again rather than wait on
	// the read that was ended.
	access::hold_syntax_blob_reads(*application, true);
	open(*application, "bulk/000.cpp");
	wait_for([&] { return access::held_syntax_blob_reads(*application) == 1U; }, "departure: the first file's classification is reading");
	open(*application, "bulk/001.cpp");
	open(*application, "bulk/000.cpp");
	access::hold_syntax_blob_reads(*application, false);
	expect_both_classified(*application, "bulk/000.cpp", "return-after-departure", true);

	application->begin_shutdown();
	wait_for([&] { return application->status().ShutdownComplete; }, "shutdown drains");
	application.reset();
	expect(e2e::remove_course_root(temporary), "private root removed");
	std::cout << "SYNTAX_DELIVERY_PUBLIC_PATH_OK qt=" << qVersion() << '\n';
	return 0;
}
