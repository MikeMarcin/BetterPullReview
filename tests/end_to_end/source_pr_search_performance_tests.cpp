#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/application/source_pr_search_execution.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/files_tree.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"
#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QListView>
#include <QPersistentModelIndex>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QThread>
#include <QToolButton>
#include <QTreeView>
#include <iostream>
#if defined(_WIN32)
	#include <psapi.h>
	#include <windows.h>
#else
	#include <sys/resource.h>
#endif
namespace {
	using namespace bpr;
	using tests::expect;
	using access = app::detail::github_application_fixture_access;
	void wait_for(const std::function<bool()>& aCondition, const char* aMessage, const std::function<void()>& aObserve = { })
	{
		QElapsedTimer timer;
		timer.start();
		while (!aCondition() && timer.elapsed() < 30000) {
			QApplication::processEvents();
			if (aObserve) {
				aObserve();
			}
			QThread::msleep(1);
		}
		if (aObserve) {
			aObserve();
		}
		expect(aCondition(), aMessage);
	}
	auto peak_resident() -> std::uint64_t
	{
#if defined(_WIN32)
		PROCESS_MEMORY_COUNTERS counters{ };
		counters.cb = sizeof(counters);
		expect(GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)) != 0, "peak process memory observed");
		return counters.PeakWorkingSetSize;
#else
		rusage usage{ };
		expect(getrusage(RUSAGE_SELF, &usage) == 0, "peak process memory observed");
	#if defined(__APPLE__)
		return static_cast<std::uint64_t>(usage.ru_maxrss);
	#else
		return static_cast<std::uint64_t>(usage.ru_maxrss) * 1024U;
	#endif
#endif
	}
	auto statistics(const application::source_search_statistics& aAfter, const application::source_search_statistics& aBefore) -> QJsonObject
	{
		return { { "compilations", static_cast<double>(aAfter.Compilations - aBefore.Compilations) }, { "read_calls", static_cast<double>(aAfter.ReadCalls - aBefore.ReadCalls) }, { "cache_hits", static_cast<double>(aAfter.CacheHits - aBefore.CacheHits) }, { "acquired_bytes", static_cast<double>(aAfter.AcquiredBytes - aBefore.AcquiredBytes) }, { "scans", static_cast<double>(aAfter.Scans - aBefore.Scans) }, { "acquisition_us", static_cast<double>(aAfter.AcquisitionMicroseconds - aBefore.AcquisitionMicroseconds) }, { "matching_us", static_cast<double>(aAfter.MatchingMicroseconds - aBefore.MatchingMicroseconds) }, { "peak_owned_bound_bytes", static_cast<double>(aAfter.PeakOwnedBound) } };
	}
}
int main(int aArgc, char** aArgv)
{
	QApplication  qt{ aArgc, aArgv };
	QTemporaryDir root;
	auto          options       = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot = e2e::private_data_root(root);
	expect(options.ApplicationDataRoot.has_value(), "isolated app root");
	std::unique_ptr<app::github_application> app;
	bool                                     ready   = false;
	auto                                     startup = access::start(app::detail::github_public_path_fixture::source_pr_search_large_v1, options, [&](auto aResult) {ready=true;if(aResult){app=std::move(*aResult);} });
	wait_for([&] { return ready && startup->is_quiescent(); }, "large shipped startup");
	expect(app != nullptr, "large fixture starts");
	app->show();
	auto& window = app->window();
	window.resize(1280, 860);
	window.github_connect_action()->trigger();
	wait_for([&] { return window.github_open_action()->isEnabled(); }, "large fixture connects");
	window.home_open_bar()->setText(QStringLiteral("bpr-fixtures/source-pr-search-large#78"));
	window.home_open_action()->trigger();
	auto* action = window.findChild<QAction*>(QStringLiteral("sourceFilesAction"));
	wait_for([&] { return window.showing_overview() && action->isEnabled(); }, "full accepted large PR");
	action->trigger();
	auto* panel   = window.pr_search();
	auto* surface = window.comparison_surface();
	wait_for([&] { return window.files_drawer()->contents()->rowCount() > 0; }, "large Files catalogue");
	const auto open = [&](std::string_view aPath) { window.files_drawer()->contents_view()->setCurrentIndex(window.files_drawer()->contents()->index_for_file(core::file_identity_of(aPath))); };
	open("bulk/000.cpp");
	wait_for([&] { return surface->current_result() && !surface->geometry_pending(); }, "ordinary initial source tab");
	window.findChild<QLineEdit*>(QStringLiteral("sourceFilesExclude"))->setText(QStringLiteral("unsupported/**"));
	panel->bar()->flush();
	const auto    aBefore   = panel->projection().Statistics;
	const auto    rssBefore = peak_resident();
	QElapsedTimer clock;
	clock.start();
	qint64 first = -1;
	panel->bar()->query_editor()->setPlainText(QStringLiteral("catalogue token"));
	panel->bar()->flush();
	const auto            generation = panel->projection().Generation;
	QPersistentModelIndex streamingRow;
	std::uint64_t         streamingMatches = 0;
	const auto            note             = [&] {
		if (first < 0 && panel->projection().Summary.Matches) {
			first = clock.elapsed();
		}
		if (!streamingRow.isValid() && panel->projection().Status == core::source_search_status::searching && panel->projection().Summary.Matches > 0) {
			streamingRow = panel->tree()->model()->index(0, 0, panel->tree()->model()->index(0, 0));
			panel->tree()->setCurrentIndex(streamingRow);
			panel->tree()->setFocus();
			streamingMatches = panel->projection().Summary.Matches;
		}
	};
	wait_for([&] { return panel->projection().Status == core::source_search_status::searching && panel->projection().Summary.EligibleFiles == 68U && panel->projection().Summary.SearchedFiles < 68U; }, "cold scan visibly active with whole original corpus", note);
	QElapsedTimer navigation;
	navigation.start();
	open("bulk/063.cpp");
	wait_for([&] { return surface->current_result() && surface->current_context()->Key.File == core::file_identity_of("bulk/063.cpp") && !surface->geometry_pending(); }, "navigation outranks cold corpus acquisition", note);
	const auto navigationMs = navigation.elapsed();
	expect(panel->projection().Generation == generation && panel->projection().Query == "catalogue token", "same active-tab navigation preserves original query generation");
	wait_for([&] { return panel->projection().Status == core::source_search_status::complete && app->status().SourceSearchQuiescent; }, "cold search resumes to full truthful coverage", note);
	const auto coldFinal = clock.elapsed();
	const auto coldFirst = first;
	expect(streamingRow.isValid() && streamingMatches < panel->projection().Summary.Matches && panel->tree()->currentIndex() == streamingRow, "incremental ordered appends preserve an already focused result row");
	const auto cold = panel->projection().Statistics;
	expect(panel->projection().Summary.SearchedFiles == 68U && panel->projection().Summary.Matches == 131U && panel->projection().Summary.SkippedSides == 0U, "navigation never abandons remaining corpus");
	expect(cold.Compilations - aBefore.Compilations == 1U && cold.AcquiredBytes > aBefore.AcquiredBytes, "cold phase compiles once and acquires actual uncached source");
	const auto retained = QPersistentModelIndex{ panel->tree()->model()->index(0, 0, panel->tree()->model()->index(0, 0)) };
	panel->tree()->setCurrentIndex(retained);
	const auto* diff     = surface->current_result().get();
	const auto* semantic = surface->current_semantic_index().get();
	surface->right_pane()->verticalScrollBar()->setValue(surface->right_pane()->verticalScrollBar()->maximum());
	for (int i = 0; i < 1000; ++i) {
		panel->tree()->setCurrentIndex(panel->tree()->model()->index(i % 2, 0, panel->tree()->model()->index(0, 0)));
	}
	expect(retained.isValid() && panel->projection().Statistics.Scans == cold.Scans && panel->projection().Statistics.ReadCalls == cold.ReadCalls && panel->projection().Statistics.Compilations == cold.Compilations && surface->current_result().get() == diff && surface->current_semantic_index().get() == semantic, "result cursor and scrolling reuse records diff and semantic index without scan compile or read");
	clock.restart();
	first = -1;
	panel->bar()->query_editor()->setPlainText(QStringLiteral("CATALOGUE TOKEN"));
	panel->bar()->flush();
	wait_for([&] { return panel->projection().Status == core::source_search_status::complete && app->status().SourceSearchQuiescent; }, "warm same corpus completes", note);
	const auto warmFinal = clock.elapsed();
	const auto warmFirst = first;
	const auto warm      = panel->projection().Statistics;
	expect(warm.Compilations - cold.Compilations == 1U && warm.AcquiredBytes == cold.AcquiredBytes && warm.CacheHits - cold.CacheHits == warm.ReadCalls - cold.ReadCalls, "warm query reuses every actual immutable read");
	panel->bar()->query_editor()->setPlainText(QStringLiteral("fixture"));
	panel->bar()->flush();
	wait_for([&] { return panel->projection().Status == core::source_search_status::result_limit && app->status().SourceSearchQuiescent; }, "public record cap terminates as qualified ordered prefix");
	expect(panel->projection().Summary.Matches == 10000U && panel->projection().Summary.SearchedFiles < panel->projection().Summary.EligibleFiles && !panel->projection().NoResults && panel->bar()->status_text().contains(QStringLiteral("narrow")), "record cap exposes prefix and narrowing prompt rather than complete no results");
	const auto scanBefore = app->status().SourceSearchScans;
	panel->bar()->findChild<QToolButton*>(QStringLiteral("sourceFindRegex"))->setChecked(true);
	panel->bar()->query_editor()->setPlainText(QStringLiteral("(fixture|unchanged|source|row).*never"));
	panel->bar()->flush();
	wait_for([&] { return app->status().SourceSearchScans > scanBefore && !app->status().SourceSearchQuiescent; }, "actual matching has begun before cancellation");
	clock.restart();
	panel->bar()->query_editor()->clear();
	panel->bar()->flush();
	wait_for([&] { return app->status().SourceSearchQuiescent; }, "clear retires matcher and all admitted search work");
	const auto cancelMs = clock.elapsed();
	expect(panel->projection().Status == core::source_search_status::idle && !panel->projection().Matches, "cancel clears obsolete result records");
	const auto rssPeak = peak_resident();
	QFile      thresholdFile{ QString::fromUtf8(BPR_SEARCH_PERFORMANCE_THRESHOLDS) };
	expect(thresholdFile.open(QIODevice::ReadOnly), "committed thresholds read");
	const auto limits = QJsonDocument::fromJson(thresholdFile.readAll()).object();
	expect(coldFirst >= 0 && coldFirst <= limits["cold_first_ms"].toInt() && warmFirst >= 0 && warmFirst <= limits["warm_first_ms"].toInt() && coldFinal <= limits["cold_final_ms"].toInt() && navigationMs <= limits["navigation_ms"].toInt() && warmFinal <= limits["warm_final_ms"].toInt() && cancelMs <= limits["cancel_ms"].toInt(), "public performance regression thresholds");
	expect(rssPeak <= static_cast<std::uint64_t>(limits["peak_resident_mib"].toInt()) * 1024U * 1024U && warm.PeakOwnedBound <= application::maximum_source_pr_search_owned_bytes, "process peak and component admission remain bounded");
	QJsonObject report{ { "platform", QSysInfo::prettyProductName() }, { "architecture", QSysInfo::currentCpuArchitecture() }, { "qt", QString::fromUtf8(qVersion()) }, { "fixture_entries", 70 }, { "eligible_files", 68 }, { "source_bytes", 11779610 }, { "cold_final_ms", coldFinal }, { "cold_first_ms", coldFirst }, { "navigation_ms", navigationMs }, { "warm_first_ms", warmFirst }, { "warm_final_ms", warmFinal }, { "cancel_ms", cancelMs }, { "rss_before_bytes", static_cast<double>(rssBefore) }, { "rss_peak_bytes", static_cast<double>(rssPeak) }, { "cold", statistics(cold, aBefore) }, { "warm", statistics(warm, cold) } };
	std::cout << "SOURCE_SEARCH_PERFORMANCE_JSON " << QJsonDocument{ report }.toJson(QJsonDocument::Compact).toStdString() << '\n'
			  << std::flush;
	app->begin_shutdown();
	wait_for([&] { return app->status().ShutdownComplete; }, "large search shutdown terminal");
	std::cout << "SOURCE_PR_SEARCH_PERFORMANCE_OK\n";
}
