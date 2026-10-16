// [[BPR-1560]]. Whole PR shows GitHub's Files changed through the shipped
// composition when the base commit GitHub records for the pull request is
// ahead of the merge base, and says it cannot when GitHub names no merge base.
//
// The merge-base-v1 fixture (`scripts/build_merge_base_fixture.py`): the merge
// base M, the recorded base T (the base branch moved on: it changes lines of
// a.txt and shared.txt the pull request does not, and lands the pull request's
// own b.txt change), the head H. GitHub's Files changed is M...H. Compared from
// T the review would revert the base branch's lines and show b.txt unchanged
// with the pull request's text on both sides, which is what the owner saw.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/core/diff_compute.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/files_tree.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;
	using tests::expect;
	using access = app::detail::github_application_fixture_access;
	using lines  = std::vector<std::pair<std::uint32_t, std::string>>;

	void wait_for(const std::function<bool()>& aCondition, const char* aMessage)
	{
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < 30000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		expect(aCondition(), aMessage);
	}

	auto shown(const qt::workbench_window& aWindow, const std::string_view aPath) -> bool
	{
		const auto* surface = aWindow.comparison_surface();
		return surface->current_result() && surface->current_context()
			&& surface->current_context()->Key.File == core::file_identity_of(aPath)
			&& surface->current_result()->Alignment.Key == surface->current_context()->Key && !surface->geometry_pending();
	}

	/// Every line a non-equal segment covers on one side, numbered from one.
	auto changed(const core::computed_file_diff& aDiff, const core::diff_side aSide) -> lines
	{
		const auto& document = aSide == core::diff_side::left ? aDiff.LeftDocument : aDiff.RightDocument;
		auto        result   = lines{ };
		for (const auto& segment : aDiff.Alignment.Segments) {
			if (segment.Kind == core::alignment_kind::equal) {
				continue;
			}
			const auto& span = aSide == core::diff_side::left ? segment.Left : segment.Right;
			for (auto line = span.Begin; line < span.End; ++line) {
				const auto& bytes = document->Lines[line].Content;
				result.emplace_back(line + 1U, std::string{ reinterpret_cast<const char*>(document->Bytes.data()) + bytes.Begin, bytes.End - bytes.Begin });
			}
		}
		return result;
	}

	struct course
	{
		QTemporaryDir                            Temporary{ QDir::tempPath() + QStringLiteral("/bpr-merge-base-XXXXXX") };
		std::unique_ptr<app::github_application> Application;
	};

	auto start(course& aCourse, const bool aMergeBaseUnavailable) -> qt::workbench_window&
	{
		const auto dataRoot = e2e::private_data_root(aCourse.Temporary);
		expect(dataRoot.has_value(), "private course root");
		auto options                 = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot  = dataRoot;
		options.MergeBaseUnavailable = aMergeBaseUnavailable;
		auto terminal                = false;
		auto startup                 = access::start(app::detail::github_public_path_fixture::merge_base_v1, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				aCourse.Application = std::move(*aResult);
			}
		});
		wait_for([&] { return terminal && startup->is_quiescent(); }, "composed startup terminal");
		expect(aCourse.Application != nullptr, "composition starts");
		aCourse.Application->show();
		auto& window = aCourse.Application->window();
		window.resize(1280, 860);
		window.github_connect_action()->trigger();
		wait_for([&] { return window.github_open_action()->isEnabled(); }, "fixture account connects");
		window.home_open_bar()->setText(QStringLiteral("bpr-fixtures/merge-base#1560"));
		window.home_open_action()->trigger();
		wait_for([&] { return window.showing_overview() && window.overview_page()->projection().has_value(); }, "the pull request opens on Overview");
		auto step  = app::detail::course_modality_step{ };
		step.Needs = app::detail::course_surfaces{ .Comparison = true, .FilesDrawer = true };
		wait_for([&] { return access::enter_course_modality(*aCourse.Application, step) != app::detail::course_modality_result::pending; }, "enters Review");
		return window;
	}

	void finish(course& aCourse)
	{
		aCourse.Application->begin_shutdown();
		wait_for([&] { return aCourse.Application->status().ShutdownComplete; }, "course shutdown drains");
		aCourse.Application.reset();
		expect(e2e::remove_course_root(aCourse.Temporary), "private course root removed");
	}

	void select(qt::workbench_window& aWindow, const std::string_view aPath)
	{
		const auto index = aWindow.files_drawer()->contents()->index_for_file(core::file_identity_of(aPath));
		expect(index.isValid(), "the file is in GitHub's file list");
		aWindow.files_drawer()->contents_view()->setCurrentIndex(index);
	}

	void render(qt::workbench_window& aWindow, const QString& aName)
	{
		const auto root = qEnvironmentVariable("BPR_MERGE_BASE_RENDER_ROOT");
		if (!root.isEmpty()) {
			expect(QDir{ }.mkpath(root) && aWindow.grab().save(root + QLatin1Char{ '/' } + aName + QStringLiteral(".png")), "render saved");
		}
	}

	/// GitHub's Files changed for M...H, line for line.
	void whole_pr_is_githubs_files_changed()
	{
		auto                                                 run    = course{ };
		auto&                                                window = start(run, false);
		const std::map<std::string, std::pair<lines, lines>> github{
			{ "a.txt", { { { 2U, "a line 2" }, { 12U, "a line 12" } }, { { 2U, "a line 2 PR-1" }, { 12U, "a line 12 PR-3" } } } },
			{ "b.txt", { { { 3U, "b line 3" } }, { { 3U, "b line 3 PR-2" } } } },
			{ "new.txt", { { }, { { 1U, "added by the pull request" } } } },
			{ "shared.txt", { { { 5U, "shared line 5" } }, { { 5U, "shared line 5 PR-1" } } } },
		};
		expect(window.overview_page()->projection()->Files.size() == github.size(), "the review lists exactly GitHub's four files");
		for (const auto& [path, expected] : github) {
			select(window, path);
			auto waited = QElapsedTimer{ };
			waited.start();
			while (!shown(window, path) && waited.elapsed() < 30000) {
				QApplication::processEvents();
				QThread::msleep(1);
			}
			if (!shown(window, path)) {
				std::cout << "merge-base waiting file=" << path << " epoch=" << window.epoch_selectors()->status_text().toStdString()
						  << " surface=" << window.comparison_surface()->status_text().toStdString() << '\n';
			}
			wait_for([&] { return shown(window, path); }, "the file's comparison is shown");
			const auto& result  = *window.comparison_surface()->current_result();
			const auto& context = *window.comparison_surface()->current_context();
			const auto  removed = changed(result, core::diff_side::left);
			const auto  added   = changed(result, core::diff_side::right);
			std::cout << "merge-base file=" << path << " removed=" << removed.size() << " added=" << added.size() << '\n';
			expect(context.Key.LeftAnchor.Kind == core::comparison_anchor_kind::epoch_base, "Whole PR compares from the base anchor");
			expect(removed == expected.first, "the removed lines are GitHub's: no line of the base branch's own is reverted");
			expect(added == expected.second, "the added lines are GitHub's, the pull request's change to b.txt included");
			expect(!context.Key.LeftBlob || context.Key.LeftBlob != context.Key.RightBlob, "no file shows the pull request's text on both sides");
		}
		expect(window.epoch_selectors()->whole_pr_action()->isChecked(), "the review opened on Whole PR");
		render(window, QStringLiteral("merge-base-whole-pr"));
		finish(run);
	}

	/// No merge base named: Whole PR says so and compares nothing from the
	/// recorded base.
	void whole_pr_without_a_merge_base_says_so()
	{
		auto  run    = course{ };
		auto& window = start(run, true);
		wait_for([&] { return window.epoch_selectors()->status_text().contains(QStringLiteral("GitHub did not say where this pull request branched")); },
				 "Whole PR says GitHub named no merge base");
		select(window, "b.txt");
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (elapsed.elapsed() < 3000) {
			QApplication::processEvents();
			QThread::msleep(1);
			const auto& context = window.comparison_surface()->current_context();
			expect(!context || context->Key.LeftAnchor.Kind != core::comparison_anchor_kind::epoch_base || !window.comparison_surface()->current_result(),
				   "nothing is compared from the recorded base");
		}
		std::cout << "merge-base unavailable status=" << window.epoch_selectors()->status_text().toStdString() << '\n';
		render(window, QStringLiteral("merge-base-unavailable"));
		finish(run);
	}
}

int main(int aArgc, char** aArgv)
{
	QApplication qtApplication{ aArgc, aArgv };
	whole_pr_is_githubs_files_changed();
	whole_pr_without_a_merge_base_says_so();
	std::cout << "MERGE_BASE_PUBLIC_PATH_OK qt=" << qVersion() << '\n';
	return 0;
}
