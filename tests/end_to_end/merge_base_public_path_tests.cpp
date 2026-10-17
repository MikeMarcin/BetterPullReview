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

	auto start(course& aCourse, const bool aMergeBaseUnavailable, const bool aMergeBaseRace = false, const bool aUnauthorized = false,
			   const app::detail::github_public_path_fixture aFixture = app::detail::github_public_path_fixture::merge_base_v1, const bool aStackedRewritten = false,
			   const bool aStackedReset = false)
		-> qt::workbench_window&
	{
		const auto dataRoot = e2e::private_data_root(aCourse.Temporary);
		expect(dataRoot.has_value(), "private course root");
		auto options                  = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot   = dataRoot;
		options.MergeBaseUnavailable  = aMergeBaseUnavailable;
		options.MergeBaseRace         = aMergeBaseRace;
		options.MergeBaseUnauthorized = aUnauthorized;
		options.StackedBaseRewritten  = aStackedRewritten;
		options.StackedBaseReset      = aStackedReset;
		auto terminal                 = false;
		auto startup                  = access::start(aFixture, options, [&](auto aResult) {
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
		window.home_open_bar()->setText(aFixture == app::detail::github_public_path_fixture::merge_base_v1 ? QStringLiteral("bpr-fixtures/merge-base#1560")
																										   : QStringLiteral("bpr-fixtures/stacked-merge-base#1572"));
		window.home_open_action()->trigger();
		if (aUnauthorized) {
			return window;
		}
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

	/// GitHub's Files changed for M...H, line for line. [[BPR-1572]]: on the
	/// stacked course, for the recorded base A1...B2, though the base branch's
	/// tip B1 names itself as merge base and would hide x.txt's change.
	void whole_pr_is_githubs_files_changed(const bool aStacked = false)
	{
		auto       run    = course{ };
		auto&      window = start(run, false, false, false,
								  aStacked ? app::detail::github_public_path_fixture::stacked_merge_base_v1 : app::detail::github_public_path_fixture::merge_base_v1);
		const auto github = aStacked ? std::map<std::string, std::pair<lines, lines>>{
			{ "x.txt", { { { 3U, "x line 3" } }, { { 3U, "x line 3 B-1" } } } },
			{ "y.txt", { { { 3U, "y line 3" } }, { { 3U, "y line 3 B-2" } } } },
		}
									 : std::map<std::string, std::pair<lines, lines>>{
										   { "a.txt", { { { 2U, "a line 2" }, { 12U, "a line 12" } }, { { 2U, "a line 2 PR-1" }, { 12U, "a line 12 PR-3" } } } },
										   { "b.txt", { { { 3U, "b line 3" } }, { { 3U, "b line 3 PR-2" } } } },
										   { "new.txt", { { }, { { 1U, "added by the pull request" } } } },
										   { "shared.txt", { { { 5U, "shared line 5" } }, { { 5U, "shared line 5 PR-1" } } } },
									   };
		expect(window.overview_page()->projection()->Files.size() == github.size(), "the review lists exactly GitHub's files");
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
		render(window, aStacked ? QStringLiteral("stacked-merge-base-whole-pr") : QStringLiteral("merge-base-whole-pr"));
		finish(run);
	}

	/// [[BPR-1572]]. The stacked course with the base branch rewritten (the
	/// recorded base is not an ancestor of its tip): GitHub is replacing the
	/// recorded base, so Whole PR says GitHub's answers disagree and compares
	/// nothing from the recorded base.
	/// [[BPR-1572]]. `aReset`: the base branch only moved forward, every compare
	/// agrees with the recorded base, but GitHub replaces the recorded base
	/// with the branch's tip before the confirming read (the window after the
	/// branch is force-pushed onto a descendant): the merge base is refused, not
	/// taken for the old recorded base.
	void stacked_rewritten_base_says_answers_disagree(const bool aReset = false)
	{
		auto  run    = course{ };
		auto& window = start(run, false, false, false, app::detail::github_public_path_fixture::stacked_merge_base_v1, !aReset, aReset);
		wait_for([&] { return window.epoch_selectors()->status_text().contains(QStringLiteral("GitHub's answers about where this pull request branched disagree")); },
				 "Whole PR says GitHub's answers disagree");
		select(window, "x.txt");
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (elapsed.elapsed() < 3000) {
			QApplication::processEvents();
			QThread::msleep(1);
			const auto& context = window.comparison_surface()->current_context();
			expect(!context || context->Key.LeftAnchor.Kind != core::comparison_anchor_kind::epoch_base || !window.comparison_surface()->current_result(),
				   "nothing is compared from the recorded base");
		}
		std::cout << (aReset ? "stacked reset" : "stacked rewritten") << " status=" << window.epoch_selectors()->status_text().toStdString() << '\n';
		expect(access::evidence(*run.Application).MergeBaseReads >= 3U && access::evidence(*run.Application).MergeBaseConfirmations == (aReset ? 1U : 0U),
			   "the recorded base, the base branch and the ancestry were asked; only the reset course reaches the confirming read");
		finish(run);
	}

	/// No merge base named: Whole PR says so and compares nothing from the
	/// recorded base. Also when GitHub recorded another base for the pull
	/// request between the snapshot and the read that confirms it: the file
	/// list and the merge base would describe two states.
	void whole_pr_without_a_merge_base_says_so(const bool aRace)
	{
		auto  run    = course{ };
		auto& window = start(run, !aRace, aRace);
		// [[BPR-1572]]. The race is GitHub's answers disagreeing, said as such.
		const auto reason = aRace ? QStringLiteral("GitHub's answers about where this pull request branched disagree")
								  : QStringLiteral("GitHub did not say where this pull request branched");
		wait_for([&] { return window.epoch_selectors()->status_text().contains(reason); }, "Whole PR says why it has no merge base");
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
		render(window, aRace ? QStringLiteral("merge-base-race") : QStringLiteral("merge-base-unavailable"));
		expect(access::evidence(*run.Application).MergeBaseConfirmations == (aRace ? 1U : 0U) && access::evidence(*run.Application).MergeBaseReads >= 1U, "the race course confirmed once; the failing compare confirms nothing");
		finish(run);
	}

	/// Lost authentication on the merge-base read fails the whole read, as on
	/// every optional read: the pull request is not shown without it.
	void lost_authentication_fails_the_read()
	{
		auto  run    = course{ };
		auto& window = start(run, false, false, true);
		wait_for([&] { return access::evidence(*run.Application).MergeBaseReads >= 1U && window.github_status_text().find("Loading") == std::string::npos; },
				 "the compare read answers");
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (elapsed.elapsed() < 2000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		std::cout << "merge-base unauthorized status=" << window.github_status_text() << '\n';
		expect(window.github_status_text() != "Pull request ready." && !window.comparison_surface()->current_result(),
			   "a read whose merge base was refused for its credential is not accepted");
		finish(run);
	}
}

int main(int aArgc, char** aArgv)
{
	QApplication qtApplication{ aArgc, aArgv };
	whole_pr_is_githubs_files_changed();
	whole_pr_is_githubs_files_changed(true);
	stacked_rewritten_base_says_answers_disagree();
	stacked_rewritten_base_says_answers_disagree(true);
	whole_pr_without_a_merge_base_says_so(false);
	whole_pr_without_a_merge_base_says_so(true);
	lost_authentication_fails_the_read();
	std::cout << "MERGE_BASE_PUBLIC_PATH_OK qt=" << qVersion() << '\n';
	return 0;
}
