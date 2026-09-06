#include "../../src/app/src/github_application_fixture_access.hpp"

#include "bpr/qt/comparison_scroll_controller.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/workbench_drawer.hpp"

#include <glaze/json.hpp>

#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QListView>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QStyle>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QThread>
#include <QTimer>
#include <QWheelEvent>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace bpr::e2e::large_review {
	using namespace bpr;
	using access = app::detail::github_application_fixture_access;

	struct fixture_blob
	{
		std::string   oid;
		std::uint64_t bytes = 0;
	};
	struct fixture_file
	{
		std::string                  path;
		std::optional<std::uint64_t> additions, deletions;
		fixture_blob                 base, head;
	};
	struct fixture_manifest
	{
		std::string               schema, base;
		std::vector<std::string>  heads;
		std::uint64_t             pack_bytes = 0;
		std::vector<fixture_file> files;
	};
	struct phase_measurement
	{
		std::string Name;
		double      ElapsedMs = 0, MaxPulseIntervalMs = 0, P95PulseIntervalMs = 0;
		std::size_t Pulses = 0, PulsesWithDiffWork = 0, PulsesWithWorkbenchWork = 0;
		std::size_t PeakDiffJobs = 0, PeakWorkbenchRequests = 0;
	};
	struct benchmark_report
	{
		std::string                    Schema = "bpr.large-review-measurement.v1";
		std::string                    Platform, Architecture, QtVersion, Style, BuildConfiguration;
		std::string                    Fixture           = "large-review-v1", Base, Head;
		int                            LogicalProcessors = 0, WindowWidth = 0, WindowHeight = 0;
		double                         DevicePixelRatio = 0;
		std::uint64_t                  PackBytes = 0, ComparedBlobBytes = 0, PrimaryIndexWork = 0, MatrixCells = 0;
		std::size_t                    Files = 0, Threads = 0, Comments = 0;
		int                            AccountRequests = 0;
		std::vector<phase_measurement> Phases;
		std::vector<double>            ChangeActionMs;
		double                         WheelBatchMs = 0, SupersessionDrainMs = 0;
		std::uint64_t                  WheelEvents = 0, ScrollFrames = 0, ScrollIndexBuilds = 0, ScrollGeometryRebuilds = 0, ScrollPaneMeasurements = 0;
		std::size_t                    RapidSelections = 0, SelectionsWithOutstandingDiff = 0;
		std::size_t                    DiffCacheEntries = 0;
		std::uint64_t                  DiffCacheBytes   = 0;
		bool                           ExactLargeSource = false, ExactFinalSource = false, Rendered = false, ShutdownComplete = false;
		std::string                    TimingScope = "Debug/local observation; 5 ms event-loop timer; outstanding jobs are sampled, not continuous CPU occupancy. Wheel batch is measured after classification settles. No cross-platform latency budget.";
	};

	void require(bool aCondition, const char* aReason)
	{
		if (!aCondition) {
			throw std::runtime_error{ aReason };
		}
	}

	[[nodiscard]] auto source_path(int aIndex) -> QString
	{
		return QStringLiteral("src/file_%1.cpp").arg(aIndex, 4, 10, QChar{ '0' });
	}

	[[nodiscard]] auto selected(const qt::diff_workbench& aSurface, const QString& aPath) -> bool
	{
		return aSurface.current_result() && aSurface.current_result()->Alignment.Key.File == core::file_identity_of(aPath.toStdString());
	}

	void select_file(qt::workbench_window& aWindow, const QString& aPath)
	{
		auto* view = aWindow.files_drawer()->contents_view();
		for (int row = 0; row < view->model()->rowCount(); ++row) {
			const auto index = view->model()->index(row, 0);
			if (index.data().toString() == aPath) {
				view->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
				return;
			}
		}
		throw std::runtime_error{ "file is absent from the real drawer" };
	}

	/// Uses a real event loop, with no polling sleep added to the measured gaps.
	template <typename Action, typename Predicate>
	[[nodiscard]] auto measure_wait(app::github_application& aApplication, QElapsedTimer& aBudget,
									benchmark_report& aReport, const char* aName, Action aAction, Predicate aReady) -> bool
	{
		phase_measurement phase;
		phase.Name = aName;
		QElapsedTimer elapsed;
		elapsed.start();
		QEventLoop loop;
		QTimer     pulse, readiness;
		pulse.setTimerType(Qt::PreciseTimer);
		pulse.setInterval(5);
		readiness.setInterval(1);
		std::vector<double> intervals;
		qint64              previous = 0;
		QObject::connect(&pulse, &QTimer::timeout, &loop, [&] {
			const auto now = elapsed.nsecsElapsed();
			intervals.push_back(static_cast<double>(now - previous) / 1'000'000.0);
			previous        = now;
			const auto work = access::diff_work_evidence(aApplication);
			phase.PulsesWithDiffWork += work.OutstandingJobs != 0U ? 1U : 0U;
			phase.PulsesWithWorkbenchWork += work.WorkbenchOutstandingRequests != 0U ? 1U : 0U;
			phase.PeakDiffJobs          = std::max(phase.PeakDiffJobs, work.OutstandingJobs);
			phase.PeakWorkbenchRequests = std::max(phase.PeakWorkbenchRequests, work.WorkbenchOutstandingRequests);
		});
		bool ready = false;
		QObject::connect(&readiness, &QTimer::timeout, &loop, [&] {
			ready = aReady();
			if (ready || aBudget.elapsed() >= 90'000) {
				loop.quit();
			}
		});
		pulse.start();
		aAction();
		ready = aReady();
		if (!ready && aBudget.elapsed() < 90'000) {
			readiness.start();
			loop.exec();
		}
		phase.ElapsedMs = static_cast<double>(elapsed.nsecsElapsed()) / 1'000'000.0;
		phase.Pulses    = intervals.size();
		if (!intervals.empty()) {
			std::ranges::sort(intervals);
			phase.MaxPulseIntervalMs = intervals.back();
			phase.P95PulseIntervalMs = intervals[(intervals.size() - 1U) * 95U / 100U];
		}
		aReport.Phases.push_back(std::move(phase));
		std::cerr << "large-review phase: " << aName << " ms=" << aReport.Phases.back().ElapsedMs << std::endl;
		if (!ready) {
			const auto evidence = access::evidence(aApplication);
			const auto work     = access::diff_work_evidence(aApplication);
			const auto status   = aApplication.status();
			std::cerr << "large-review wait: " << aName << " status=" << aApplication.window().github_status_text()
					  << " account_reads=" << evidence.AccountReadCount << " replay_failure=" << evidence.RecordingFailure
					  << " diff=" << aApplication.window().comparison_surface()->status_text().toStdString()
					  << " jobs=" << work.OutstandingJobs << " workbench=" << work.WorkbenchOutstandingRequests << " pending=" << work.Pending << " quiet=" << work.Quiescent
					  << " classification=" << status.ClassificationSubmissions << ":" << status.ClassificationTerminals << '\n';
		}
		return ready;
	}

	[[nodiscard]] auto quiet(app::github_application& aApplication) -> bool
	{
		const auto work   = access::diff_work_evidence(aApplication);
		const auto status = aApplication.status();
		return work.OutstandingJobs == 0U && !work.Pending && work.WorkbenchOutstandingRequests == 0U
			&& status.ClassificationTerminals == status.ClassificationSubmissions;
	}

	void verify_snapshot(app::github_application& aApplication, const fixture_manifest& aManifest, benchmark_report& aReport)
	{
		const auto snapshot = access::large_review_snapshot(aApplication);
		require(snapshot.has_value(), "no host-accepted snapshot");
		require(snapshot->Account.value() == 800001U && snapshot->Repository.Repository.value() == 800002U
					&& snapshot->PullRequest.PullRequest.value() == 800003U && snapshot->PullRequest.Number.value() == 17U
					&& snapshot->Repository.Owner == "bpr-fixtures" && snapshot->Repository.Name == "large-review"
					&& snapshot->PullRequest.BaseOid.value() == aManifest.base && snapshot->PullRequest.HeadOid.value() == aManifest.heads.back(),
				"snapshot authority/head");
		require(aManifest.files.size() == 515U && snapshot->Files.size() == 515U && snapshot->ReviewThreads.size() == 1000U, "accepted workload counts");
		std::map<std::string, const application::github_file_record*> files;
		for (const auto& file : snapshot->Files) {
			require(files.emplace(file.Path, &file).second, "duplicate normalized file");
		}
		for (const auto& expected : aManifest.files) {
			const auto found = files.find(expected.path);
			require(found != files.end(), "missing normalized file");
			const auto& file = *found->second;
			require(file.Change == application::github_file_change::modified && file.Additions == expected.additions.value_or(0U)
						&& file.Deletions == expected.deletions.value_or(0U) && file.ViewedState == application::github_file_viewed_state::unviewed,
					"normalized file content/viewed state");
			aReport.ComparedBlobBytes += expected.base.bytes + expected.head.bytes;
		}
		std::map<std::string, const application::github_review_thread_record*> threads;
		for (const auto& thread : snapshot->ReviewThreads) {
			require(threads.emplace(std::string{ thread.Thread.value() }, &thread).second, "duplicate normalized thread");
		}
		for (int ordinal = 0; ordinal < 1000; ++ordinal) {
			const auto id    = "SYNTHETIC_COMMENT_" + std::to_string(900000 + ordinal);
			const auto found = threads.find("SYNTHETIC_THREAD_" + id);
			require(found != threads.end(), "missing normalized thread identity");
			const auto& thread = *found->second;
			const auto  path   = source_path(ordinal % 512).toStdString();
			require(thread.Path == path && thread.Line == 1U && thread.OriginalLine == 1U && !thread.Resolved && !thread.Outdated
						&& thread.Subject == application::github_review_thread_subject::line && thread.DiffSide == application::github_diff_side::right
						&& thread.Comments.size() == 1U,
					"normalized thread anchor");
			const auto& comment = thread.Comments.front();
			const auto  body    = QStringLiteral("Synthetic benchmark observation %1; no real review.").arg(ordinal, 4, 10, QChar{ '0' }).toStdString();
			require(comment.Comment.value() == id && comment.Body == body && comment.Path == path && comment.Line == 1U && comment.OriginalLine == 1U
						&& !comment.ReplyTo && !comment.Deleted && !comment.Outdated && comment.Author && comment.Author->Login == "fixture-reviewer",
					"normalized source comment");
		}
		aReport.Files       = snapshot->Files.size();
		aReport.Threads     = snapshot->ReviewThreads.size();
		aReport.Comments    = 1000U;
		const auto evidence = access::evidence(aApplication);
		require(evidence.RecordingConsumed && evidence.CapabilityProbeConsumed && evidence.BootstrapTransportSelected && evidence.AccountTransportSelected
					&& evidence.RecordingFailure == -1 && evidence.AccountReadCount == 29 && evidence.SmartAcquireCount == 1
					&& evidence.SmartDiscoveryCount == 1 && evidence.SmartLsRefsCount == 1 && evidence.SmartFetchCount == 1,
				"selected provider/Git path or replay exhaustion");
		aReport.AccountRequests = evidence.AccountReadCount;
	}

	void verify_large(qt::diff_workbench& aSurface, const fixture_manifest& aManifest)
	{
		require(selected(aSurface, QStringLiteral("large/comparison.cpp")), "large comparison identity");
		const auto& result = *aSurface.current_result();
		const auto  found  = std::ranges::find(aManifest.files, std::string{ "large/comparison.cpp" }, &fixture_file::path);
		require(found != aManifest.files.end(), "large file manifest identity");
		require(result.Alignment.Key.LeftBlob == core::parse_git_blob_oid(found->base.oid) && result.Alignment.Key.RightBlob == core::parse_git_blob_oid(found->head.oid), "installed Git selected exact blobs");
		require(result.LeftDocument && result.RightDocument && result.LeftDocument->Lines.size() == 20000U && result.RightDocument->Lines.size() == 20000U, "large source line counts");
		for (int line = 0; line < 20000; ++line) {
			const auto prefix = QStringLiteral("int sample_%1 = ").arg(line, 5, 10, QChar{ '0' });
			require(aSurface.left_pane()->document()->findBlockByNumber(line).text() == prefix + "0;"
						&& aSurface.right_pane()->document()->findBlockByNumber(line).text() == prefix + (line % 1000 == 0 ? "1;" : "0;"),
					"displayed source line differs from fixture");
		}
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	using namespace bpr::e2e::large_review;
	QApplication qtApplication{ argc, argv };
	QApplication::setQuitOnLastWindowClosed(false);
	if (argc != 4) {
		return EXIT_FAILURE;
	}
	QTemporaryDir data;
	if (!data.isValid()) {
		return EXIT_FAILURE;
	}
	benchmark_report                         report;
	std::unique_ptr<app::github_application> application;
	try {
		std::ifstream     input{ std::filesystem::path{ BPR_LARGE_REVIEW_FIXTURE_ROOT } / "manifest.json" };
		const std::string bytes{ std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{ } };
		fixture_manifest  manifest;
		require(!glz::read<glz::opts{ .error_on_unknown_keys = false }>(manifest, bytes) && manifest.schema == "bpr.large-review-fixture.v1" && manifest.heads.size() == 32U, "stored manifest");
		report.Base               = manifest.base;
		report.Head               = manifest.heads.back();
		report.PackBytes          = manifest.pack_bytes;
		report.Platform           = QSysInfo::prettyProductName().toStdString();
		report.Architecture       = QSysInfo::currentCpuArchitecture().toStdString();
		report.QtVersion          = qVersion();
		report.Style              = qtApplication.style()->objectName().toStdString();
		report.BuildConfiguration = BPR_BENCH_BUILD_CONFIGURATION;
		report.LogicalProcessors  = QThread::idealThreadCount();
		application               = access::make(app::detail::github_public_path_fixture::large_review_v1,
												 { .ApplicationDataRoot = data.path().toStdString(), .GitExecutable = std::filesystem::path{ argv[1] } });
		auto& window              = application->window();
		window.resize(1600, 1000);
		application->show();
		auto&         surface = *window.comparison_surface();
		QElapsedTimer budget;
		budget.start();
		require(measure_wait(*application, budget, report, "fixture_connect", [&] { window.github_connect_action()->trigger(); }, [&] { return window.github_status_text() == "GitHub account connected." && window.github_open_action()->isEnabled(); }), "connect timed out");
		window.session_panel()->owner_editor()->setText(QStringLiteral("bpr-fixtures"));
		window.session_panel()->repository_editor()->setText(QStringLiteral("large-review"));
		window.session_panel()->pull_request_editor()->setValue(17);
		require(measure_wait(*application, budget, report, "paged_review_and_git_load", [&] { window.github_open_action()->trigger(); }, [&] { return window.files_drawer()->contents_view()->model()->rowCount() == 515 && access::evidence(*application).SmartFetchCount == 1 && application->status().EpochStatus == core::epoch_status::ready && access::diff_work_evidence(*application).NavigationReady && quiet(*application); }), "review load timed out");
		verify_snapshot(*application, manifest, report);
		require(measure_wait(*application, budget, report, "large_file_selection", [&] { select_file(window, QStringLiteral("large/comparison.cpp")); }, [&] { return selected(surface, QStringLiteral("large/comparison.cpp")) && quiet(*application) && surface.scroll_controller()->maximum_position() > 1000.0; }), "large selection timed out");
		verify_large(surface, manifest);
		report.ExactLargeSource = true;
		report.PrimaryIndexWork = surface.current_result()->PrimaryIndexWorkConsumed;
		report.MatrixCells      = surface.current_result()->MatrixCellsConsumed;
		require(surface.selection() && surface.selection()->PreferredLine.Line == 0U, "initial selected change is line one");
		for (int change = 0; change < 2; ++change) {
			require(window.next_change_action()->isEnabled(), "change navigation action unavailable");
			QElapsedTimer gesture;
			gesture.start();
			window.next_change_action()->trigger();
			report.ChangeActionMs.push_back(static_cast<double>(gesture.nsecsElapsed()) / 1'000'000.0);
			require(surface.selection() && surface.selection()->PreferredLine.Line == static_cast<std::uint32_t>((change + 1) * 1000), "next change selected wrong source line");
		}
		auto* controller = surface.scroll_controller();
		surface.shared_scrollbar()->setValue(0);
		const auto    before               = surface.statistics();
		const auto    scrollBefore         = controller->statistics();
		const auto    classificationBefore = application->status().ClassificationSubmissions;
		QElapsedTimer wheel;
		wheel.start();
		for (int event = 0; event < 1000; ++event) {
			QWheelEvent inputEvent{ QPointF{ 20, 20 }, QPointF{ 20, 20 }, QPoint{ 0, -1 }, QPoint{ }, Qt::NoButton, Qt::NoModifier, Qt::ScrollUpdate, false };
			QApplication::sendEvent(surface.right_pane()->viewport(), &inputEvent);
		}
		report.WheelBatchMs           = static_cast<double>(wheel.nsecsElapsed()) / 1'000'000.0;
		const auto after              = surface.statistics();
		const auto scrollAfter        = controller->statistics();
		report.WheelEvents            = scrollAfter.WheelEvents - scrollBefore.WheelEvents;
		report.ScrollFrames           = scrollAfter.FramesApplied - scrollBefore.FramesApplied;
		report.ScrollIndexBuilds      = after.SemanticIndexBuilds - before.SemanticIndexBuilds;
		report.ScrollGeometryRebuilds = after.GeometryRebuilds - before.GeometryRebuilds;
		report.ScrollPaneMeasurements = after.PaneMeasurements - before.PaneMeasurements;
		require(report.WheelEvents == 1000U && report.ScrollFrames == 1000U && report.ScrollIndexBuilds == 0U && report.ScrollGeometryRebuilds == 0U && report.ScrollPaneMeasurements == 0U
					&& application->status().ClassificationSubmissions == classificationBefore && controller->position() == 1000.0,
				"scroll gesture work/position contract");
		surface.shared_scrollbar()->setValue(0);
		report.WindowWidth      = window.width();
		report.WindowHeight     = window.height();
		report.DevicePixelRatio = window.devicePixelRatioF();
		report.Rendered         = window.grab().save(QString::fromLocal8Bit(argv[3]));
		require(report.Rendered, "full-window capture failed");

		QTimer selections;
		selections.setInterval(1);
		int           next = 0;
		QString       requested;
		QElapsedTimer drain;
		QObject::connect(&selections, &QTimer::timeout, &window, [&] {
			if (next == 32) {
				selections.stop();
				return;
			}
			const auto work = access::diff_work_evidence(*application);
			if (!requested.isEmpty() && work.OutstandingJobs == 0U && !selected(surface, requested)) {
				return;
			}
			report.SelectionsWithOutstandingDiff += work.OutstandingJobs != 0U ? 1U : 0U;
			requested = source_path(next++);
			select_file(window, requested);
			++report.RapidSelections;
			if (next == 32) {
				drain.start();
			}
		});
		require(measure_wait(*application, budget, report, "rapid_selection_supersession", [&] { selections.start(); }, [&] { return next == 32 && selected(surface, source_path(31)) && quiet(*application); }), "supersession timed out");
		report.SupersessionDrainMs = static_cast<double>(drain.nsecsElapsed()) / 1'000'000.0;
		require(report.SelectionsWithOutstandingDiff != 0U, "supersession never overlapped outstanding diff work");
		report.ExactFinalSource = surface.left_pane()->document()->firstBlock().text() == "int fixture_0031 = 0;" && surface.right_pane()->document()->firstBlock().text() == "int fixture_0031 = 1;";
		require(report.ExactFinalSource, "stale source adopted after supersession");
		const auto work         = access::diff_work_evidence(*application);
		report.DiffCacheEntries = work.CacheEntries;
		report.DiffCacheBytes   = work.CacheBytes;
		require(work.FixtureDiffHelperInvocations == 0U, "fixture diff bypass was used");
		report.ShutdownComplete = measure_wait(*application, budget, report, "shutdown", [&] { application->begin_shutdown(); }, [&] { return application->status().ShutdownComplete; });
		require(report.ShutdownComplete && !application->status().ShutdownCleanupFailed, "shutdown failed");
		auto encoded = glz::write_json(report);
		require(encoded.has_value(), "measurement serialization");
		std::ofstream output{ argv[2], std::ios::binary };
		output << *encoded << '\n';
		output.close();
		require(output.good(), "measurement write");
		std::cout << "LARGE_REVIEW_COMPOSED_BENCHMARK_OK\n"
				  << *encoded << '\n';
		return EXIT_SUCCESS;
	} catch (const std::exception& error) {
		std::cerr << "large-review-benchmark-failed: " << error.what() << '\n';
		return EXIT_FAILURE;
	}
}
