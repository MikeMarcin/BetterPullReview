#include "../../src/app/src/github_application_fixture_access.hpp"

#include "bpr/qt/comparison_scroll_controller.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/workbench_drawer.hpp"

#include <glaze/json.hpp>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
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

#if defined(__APPLE__)
	#include <mach/mach.h>
#endif

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
		std::size_t PeakDiffJobs = 0, PeakWorkbenchRequests = 0, PeakEpochJobs = 0;
	};
	struct resource_sample
	{
		std::string   Phase;
		std::uint64_t ResidentBytes = 0, PhysicalFootprintBytes = 0, DiffCacheBytes = 0;
		std::size_t   DiffCacheEntries = 0, DiffJobs = 0, EpochJobs = 0;
	};

	struct benchmark_report
	{
		std::string   Schema           = "bpr.large-review-measurement.v1";
		bool          ResourceCourse   = false;
		bool          CacheEntryCourse = false, EntryEvictedDiff = false, EntryEvictedIndex = false, EntryRevisitExact = false;
		std::size_t   EntrySelections = 0U, EntryPeakCacheEntries = 0U;
		std::uint64_t EntryPeakCacheBytes = 0U;
		bool          LoadedCloseCourse = false, CloseStartedWithLiveComparison = false, ClosePreservedEpochStorage = false;
		int           CloseLeftDocumentBlocks = 0, CloseRightDocumentBlocks = 0;
		bool          ShutdownReleasedDiff = false, ShutdownReleasedIndex = false, DestroyReleasedWindow = false, DestroyReleasedDocuments = false;
		bool          HistoryCourse  = false;
		std::size_t   EpochsObserved = 0U, RetainedEpochsVerified = 0U, EpochSelections = 0U, EpochSelectionsWithOutstandingWork = 0U;
		std::size_t   HistoryPeakRows = 0U, HistoryModelResets = 0U;
		// Same M4 Pro / Debug course: refresh max input interval 67.903 ms,
		// refresh 9.165 s, source selection 1.113 s before these thresholds.
		double                       HistoryInputIntervalLimitMs = 100.0;
		double                       HistoryRefreshLimitMs = 12'000.0, HistorySourceSelectionLimitMs = 2'000.0;
		std::vector<resource_sample> Resources;
		// Provisional Mac fixture budgets from the measured Debug course. These
		// bound sampled growth, not unsampled peaks or other platforms.
		std::uint64_t                  SampledFootprintGrowthLimitBytes = 160U * 1024U * 1024U;
		std::uint64_t                  CleanupFootprintGrowthLimitBytes = 96U * 1024U * 1024U;
		bool                           WindowCloseObserved              = false;
		bool                           PruneReleasedDiff = false, PruneReleasedIndex = false, PruneClearedProjection = false;
		bool                           DeleteReleasedDiff = false, DeleteReleasedIndex = false, DestroyReleasedDiff = false, DestroyReleasedIndex = false;
		bool                           UnavailableCourse = false, UnavailableReasonVerified = false, BinaryStatusVerified = false;
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
		std::uint64_t                  WheelEvents = 0, ScrollFrames = 0, ScrollIndexAdoptions = 0, ScrollGeometryRebuilds = 0, ScrollPaneMeasurements = 0;
		std::size_t                    RapidSelections = 0, SelectionsWithOutstandingDiff = 0;
		std::uint64_t                  LargeMeasurementBatches = 0, InputsDuringMeasurement = 0;
		std::uint32_t                  MaximumMeasurementBatchLines  = 0;
		double                         MaxMeasurementInputIntervalMs = 0, P95MeasurementInputIntervalMs = 0;
		std::size_t                    DiffCacheEntries                      = 0;
		std::size_t                    PeakConversationRootsDuringSelections = 0;
		std::uint64_t                  DiffCacheBytes                        = 0;
		bool                           ExactLargeSource = false, ExactFinalSource = false, Rendered = false, ShutdownComplete = false;
		std::string                    TimingScope = "Debug/local observation; 5 ms event-loop timer; outstanding jobs are sampled, not continuous CPU occupancy. Measurement input gaps span consecutive 1 ms timer callbacks after native batching has started; initial publication delay remains in the phase timer. Wheel batch is measured after classification settles. No cross-platform latency budget.";
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

	void sample_resources(benchmark_report& aReport, app::github_application* aApplication, const char* aPhase)
	{
		if (!aReport.ResourceCourse && !aReport.CacheEntryCourse) {
			return;
		}
		resource_sample sample;
		sample.Phase = aPhase;
#if defined(__APPLE__)
		task_vm_info_data_t    information{ };
		mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
		require(task_info(mach_task_self(), TASK_VM_INFO, reinterpret_cast<task_info_t>(&information), &count) == KERN_SUCCESS,
				"native process memory sample failed");
		sample.ResidentBytes          = information.resident_size;
		sample.PhysicalFootprintBytes = information.phys_footprint;
#else
		throw std::runtime_error{ "the native memory resource course is currently Mac-only" };
#endif
		if (aApplication) {
			const auto work         = access::diff_work_evidence(*aApplication);
			sample.DiffCacheBytes   = work.CacheBytes;
			sample.DiffCacheEntries = work.CacheEntries;
			sample.DiffJobs         = work.OutstandingJobs;
			sample.EpochJobs        = work.EpochOutstandingJobs;
			require(work.CacheBytes <= application::diff_host_limits{ }.CacheBytes
						&& work.CacheEntries <= application::diff_host_limits{ }.CacheEntries,
					"configured diff cache budget exceeded");
		}
		if (!aReport.Resources.empty()) {
			const auto baseline = aReport.Resources.front().PhysicalFootprintBytes;
			const auto growth   = sample.PhysicalFootprintBytes > baseline
				? sample.PhysicalFootprintBytes - baseline
				: 0U;
			require(growth <= aReport.SampledFootprintGrowthLimitBytes,
					"sampled Mac fixture footprint growth exceeded its provisional budget");
			if (sample.Phase == "after_prune" || sample.Phase == "after_cache_delete"
				|| sample.Phase == "shutdown_drained" || sample.Phase == "application_destroyed") {
				require(growth <= aReport.CleanupFootprintGrowthLimitBytes,
						"Mac cleanup footprint growth exceeded its provisional budget");
				require(sample.DiffCacheBytes == 0U && sample.DiffCacheEntries == 0U
							&& sample.DiffJobs == 0U && sample.EpochJobs == 0U,
						"completed cleanup retains derived cache or outstanding work");
			}
		}
		aReport.Resources.push_back(sample);
		std::cerr << "resource-sample:" << *glz::write_json(sample) << '\n';
	}

	void confirm_fixture_maintenance(QAction& aAction, const QString& aDialogName)
	{
		require(aAction.isEnabled(), "public maintenance action is unavailable");
		QTimer::singleShot(0, qApp, [aDialogName] {
			auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
			require(dialog && dialog->objectName() == aDialogName, "unexpected local maintenance confirmation");
			auto* button = dialog->findChild<QPushButton*>(aDialogName + QStringLiteral("ConfirmButton"));
			require(button != nullptr, "local maintenance confirmation button is absent");
			button->click();
		});
		aAction.trigger();
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
			phase.PeakEpochJobs         = std::max(phase.PeakEpochJobs, work.EpochOutstandingJobs);
			phase.PeakWorkbenchRequests = std::max(phase.PeakWorkbenchRequests, work.WorkbenchOutstandingRequests);
		});
		bool ready = false;
		QObject::connect(&readiness, &QTimer::timeout, &loop, [&] {
			ready = aReady();
			if (ready || aBudget.elapsed() >= (aReport.HistoryCourse ? 540'000 : aReport.CacheEntryCourse ? 180'000
																										  : 90'000)) {
				loop.quit();
			}
		});
		pulse.start();
		aAction();
		ready = aReady();
		if (!ready && aBudget.elapsed() < (aReport.HistoryCourse ? 540'000 : aReport.CacheEntryCourse ? 180'000
																									  : 90'000)) {
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
		return !aApplication.window().comparison_surface()->geometry_pending()
			&& work.OutstandingJobs == 0U && !work.Pending && work.WorkbenchOutstandingRequests == 0U
			&& status.ClassificationTerminals == status.ClassificationSubmissions;
	}

	void verify_snapshot(app::github_application& aApplication, const fixture_manifest& aManifest, benchmark_report& aReport)
	{
		const auto snapshot = access::large_review_snapshot(aApplication);
		require(snapshot.has_value(), "no host-accepted snapshot");
		require(snapshot->Account.value() == 800001U && snapshot->Repository.Repository.value() == 800002U
					&& snapshot->PullRequest.PullRequest.value() == 800003U && snapshot->PullRequest.Number.value() == 17U
					&& snapshot->Repository.Owner == "bpr-fixtures" && snapshot->Repository.Name == "large-review"
					&& snapshot->PullRequest.BaseOid.value() == aManifest.base && snapshot->PullRequest.HeadOid.value() == aReport.Head,
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
					&& evidence.RecordingFailure == -1 && evidence.AccountReadCount == static_cast<int>(1U + 28U * std::max(std::size_t{ 1U }, aReport.EpochsObserved)) && evidence.SmartAcquireCount == 1
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

	void select_epoch(qt::epoch_comparison_control& aControl, std::int64_t aOrdinal, core::comparison_side aSide)
	{
		auto* const selector = aSide == core::comparison_side::left ? aControl.left_selector() : aControl.right_selector();
		require(selector->isEnabled(), "requested epoch selector is disabled");
		const auto* model = aControl.history_model();
		const auto  kind  = aSide == core::comparison_side::left ? core::comparison_anchor_kind::epoch_base : core::comparison_anchor_kind::epoch_head;
		for (int row = 0; row < model->rowCount(); ++row) {
			const auto anchor = model->anchor_at(model->index(row, 0));
			if (anchor && anchor->Identity.Epoch.value() == aOrdinal && anchor->Identity.Kind == kind) {
				selector->setCurrentIndex(row);
				return;
			}
		}
		throw std::runtime_error{ "requested retained epoch is absent from public history" };
	}

	void run_history(app::github_application& aApplication, const fixture_manifest& aManifest,
					 benchmark_report& aReport, QElapsedTimer& aBudget, const char* aImage)
	{
		auto&      window        = aApplication.window();
		auto&      surface       = *window.comparison_surface();
		auto&      controls      = *window.epoch_selectors();
		auto*      history       = controls.history_model();
		const auto initialResets = history->statistics().ModelResets;
		const auto settled       = [&] {
			return quiet(aApplication) && access::diff_work_evidence(aApplication).EpochOutstandingJobs == 0U;
		};
		const auto source_matches = [&](int aFile, std::int64_t aEpoch, int aValue) {
			return selected(surface, source_path(aFile)) && surface.current_result()->Alignment.Key.RightAnchor.Epoch.value() == aEpoch
				&& surface.left_pane()->document()->firstBlock().text() == QStringLiteral("int fixture_%1 = 0;").arg(aFile, 4, 10, QChar{ '0' })
				&& surface.right_pane()->document()->firstBlock().text() == QStringLiteral("int fixture_%1 = %2;").arg(aFile, 4, 10, QChar{ '0' }).arg(aValue);
		};
		for (std::size_t index = 0U; index < aManifest.heads.size(); ++index) {
			const auto ordinal = static_cast<std::int64_t>(index + 1U);
			if (index != 0U) {
				const auto expectedHead = QString::fromStdString(aManifest.heads[index]);
				require(window.github_refresh_action()->isEnabled(), "public refresh unavailable");
				const auto refreshPhase = "epoch_refresh_" + std::to_string(ordinal);
				require(measure_wait(aApplication, aBudget, aReport, refreshPhase.c_str(), [&] { window.github_refresh_action()->trigger(); }, [&] { return controls.right_oid_editor()->text() == expectedHead && settled(); }), "epoch refresh did not adopt the exact stored head");
			}
			aReport.EpochsObserved = index + 1U;
			aReport.Head           = aManifest.heads[index];
			const auto evidence    = access::evidence(aApplication);
			require(evidence.RecordingConsumed && evidence.RecordingFailure == -1
						&& evidence.AccountReadCount == static_cast<int>(1 + 28 * ordinal),
					"refresh did not exhaust exactly one recorded snapshot");
			const auto file           = index == 0U ? 0 : static_cast<int>(index - 1U);
			const auto value          = static_cast<int>(ordinal);
			const auto selectionPhase = "epoch_source_selection_" + std::to_string(ordinal);
			require(measure_wait(aApplication, aBudget, aReport, selectionPhase.c_str(), [&] { select_file(window, source_path(file)); }, [&] { return source_matches(file, ordinal, value) && settled(); }), "selected source did not match the new epoch's independent recipe");
			if (index == 0U) {
				const auto& selectedAnchor = surface.current_result()->Alignment.Key.RightAnchor;
				const auto  initialStored  = access::maintenance_evidence(aApplication, core::review_epoch_id{ selectedAnchor.Authority, selectedAnchor.Incarnation, selectedAnchor.Epoch });
				std::cerr << "initial-retained-epoch:" << *glz::write_json(initialStored) << '\n';
				require(initialStored.TargetBaseRefOid == aManifest.base && initialStored.TargetHeadRefOid == aManifest.heads.front()
							&& initialStored.TargetBaseCatalogOid == aManifest.base && initialStored.TargetHeadCatalogOid == aManifest.heads.front(),
						"initial retained catalog/ref evidence mismatch");
			}
			aReport.HistoryPeakRows = std::max(aReport.HistoryPeakRows, static_cast<std::size_t>(history->rowCount()));
			const auto cache        = access::diff_work_evidence(aApplication);
			require(cache.CacheEntries <= application::diff_host_limits{ }.CacheEntries && cache.CacheBytes <= application::diff_host_limits{ }.CacheBytes,
					"epoch refresh exceeded configured immutable diff cache budget");
			std::cerr << "epoch-ready:" << ordinal << " account_reads=" << evidence.AccountReadCount << " cache=" << cache.CacheEntries << ':' << cache.CacheBytes << '\n';
		}
		require(controls.refresh_action()->isEnabled(), "public history refresh unavailable");
		require(measure_wait(aApplication, aBudget, aReport, "load_epoch_history", [&] { controls.refresh_action()->trigger(); }, [&] { return history->epoch_count() == 32U && settled(); }), "retained history did not expose all 32 epochs");
		aReport.HistoryPeakRows    = std::max(aReport.HistoryPeakRows, static_cast<std::size_t>(history->rowCount()));
		aReport.HistoryModelResets = history->statistics().ModelResets - initialResets;
		require(aReport.HistoryModelResets == 0U, "bounded epoch refresh reset the history model");
		const auto anchor = history->anchor_at(history->index(controls.right_selector()->currentIndex(), 0));
		require(anchor.has_value(), "selected epoch has no typed public anchor");
		for (std::size_t index = 0U; index < aManifest.heads.size(); ++index) {
			const auto stored = access::maintenance_evidence(aApplication, core::review_epoch_id{ anchor->Identity.Authority, anchor->Identity.Incarnation, core::epoch_ordinal{ static_cast<std::int64_t>(index + 1U) } });
			std::cerr << "retained-epoch:" << index + 1U << ':' << *glz::write_json(stored) << '\n';
			require(stored.TargetBaseRefPresent && stored.TargetHeadRefPresent
						&& stored.TargetBaseRefOid == aManifest.base && stored.TargetHeadRefOid == aManifest.heads[index]
						&& stored.TargetBaseCatalogOid == aManifest.base && stored.TargetHeadCatalogOid == aManifest.heads[index],
					"retained catalog/ref pair differs from the independently stored Git head");
			++aReport.RetainedEpochsVerified;
		}
		const auto historicalBaseReady = [&] {
			return source_matches(30, 32, 32) && surface.current_result()->Alignment.Key.LeftAnchor.Epoch.value() == 1 && settled();
		};
		require(measure_wait(aApplication, aBudget, aReport, "historical_base_selection", [&] { select_epoch(controls, 1, core::comparison_side::left); }, historicalBaseReady), "historical base selection did not settle");
		require(measure_wait(aApplication, aBudget, aReport, "historical_first_head", [&] { select_epoch(controls, 1, core::comparison_side::right); }, [&] { return source_matches(30, 1, 1) && settled(); }), "historical first-head source was replaced by current content");
		require(window.grab().save(QString::fromLocal8Bit(aImage) + QStringLiteral(".historical.png")), "historical window render failed");
		QTimer selections;
		selections.setInterval(1);
		std::int64_t next = 2;
		QObject::connect(&selections, &QTimer::timeout, &window, [&] {
			if (next > 32U) {
				selections.stop();
				return;
			}
			if (!controls.right_selector()->isEnabled()) {
				return;
			}
			const auto work = access::diff_work_evidence(aApplication);
			aReport.EpochSelectionsWithOutstandingWork += work.OutstandingJobs != 0U || work.EpochOutstandingJobs != 0U || work.WorkbenchOutstandingRequests != 0U ? 1U : 0U;
			select_epoch(controls, next++, core::comparison_side::right);
			++aReport.EpochSelections;
		});
		require(measure_wait(aApplication, aBudget, aReport, "epoch_selection_supersession", [&] { selections.start(); }, [&] { return next == 33U && source_matches(30, 32U, 32) && settled(); }), "late epoch result replaced final source");
		require(aReport.EpochSelections == 31U && aReport.EpochSelectionsWithOutstandingWork != 0U,
				"epoch selection did not exercise supersession during active work");
		const auto previousUploadReady = [&] {
			return selected(surface, source_path(30)) && surface.current_result()->Alignment.Key.LeftAnchor.Epoch.value() == 31
				&& surface.current_result()->Alignment.Key.RightAnchor.Epoch.value() == 32 && settled();
		};
		require(controls.previous_upload_action()->isEnabled(), "previous upload action is disabled");
		require(measure_wait(aApplication, aBudget, aReport, "previous_upload_comparison", [&] { controls.previous_upload_action()->trigger(); }, previousUploadReady), "previous upload did not select the two newest retained heads");
		require(surface.left_pane()->document()->firstBlock().text() == "int fixture_0030 = 1;"
					&& surface.right_pane()->document()->firstBlock().text() == "int fixture_0030 = 32;",
				"previous upload lost its exact earlier/later source");
		aReport.WindowWidth      = window.width();
		aReport.WindowHeight     = window.height();
		aReport.DevicePixelRatio = window.devicePixelRatioF();
		aReport.Rendered         = window.grab().save(QString::fromLocal8Bit(aImage));
		require(aReport.Rendered, "many-epoch public window render failed");
		const auto work          = access::diff_work_evidence(aApplication);
		aReport.DiffCacheEntries = work.CacheEntries;
		aReport.DiffCacheBytes   = work.CacheBytes;
		aReport.AccountRequests  = access::evidence(aApplication).AccountReadCount;
		require(work.FixtureDiffHelperInvocations == 0U, "many-epoch course used a fixture diff bypass");
		for (const auto& phase : aReport.Phases) {
			if (phase.Name.starts_with("epoch_refresh_")) {
				require(phase.MaxPulseIntervalMs <= aReport.HistoryInputIntervalLimitMs && phase.ElapsedMs <= aReport.HistoryRefreshLimitMs,
						"epoch refresh exceeded the provisional measured Mac budget");
			} else if (phase.Name.starts_with("epoch_source_selection_")) {
				require(phase.MaxPulseIntervalMs <= aReport.HistoryInputIntervalLimitMs && phase.ElapsedMs <= aReport.HistorySourceSelectionLimitMs,
						"epoch source selection exceeded the provisional measured Mac budget");
			}
		}
	}

	void run_cache_entries(app::github_application& aApplication, const fixture_manifest& aManifest,
						   benchmark_report& aReport, QElapsedTimer& aBudget, const char* aImage)
	{
		auto&      window  = aApplication.window();
		auto&      surface = *window.comparison_surface();
		const auto limits  = application::diff_host_limits{ };
		require(limits.CacheEntries == 64U && access::diff_work_evidence(aApplication).CacheEntries == 0U,
				"entry-pressure fixture requires the default empty 64-entry cache");
		std::weak_ptr<const core::computed_file_diff>  firstDiff;
		std::weak_ptr<const core::diff_semantic_index> firstIndex;
		const auto                                     ready = [&](int aFile) {
			return selected(surface, source_path(aFile)) && quiet(aApplication)
				&& access::diff_work_evidence(aApplication).EpochOutstandingJobs == 0U;
		};
		const auto verify = [&](int aFile) {
			const auto  path  = source_path(aFile).toStdString();
			const auto  found = std::ranges::find(aManifest.files, path, &fixture_file::path);
			const auto& key   = surface.current_result()->Alignment.Key;
			require(found != aManifest.files.end() && key.LeftBlob == core::parse_git_blob_oid(found->base.oid)
						&& key.RightBlob == core::parse_git_blob_oid(found->head.oid),
					"entry-pressure selected wrong stored blobs");
			for (int line = 0; line < 8; ++line) {
				const auto common = QStringLiteral("// synthetic source %1 line %2").arg(aFile, 4, 10, QChar{ '0' }).arg(line + 1);
				const auto prefix = QStringLiteral("int fixture_%1 = ").arg(aFile, 4, 10, QChar{ '0' });
				require(surface.left_pane()->document()->findBlockByNumber(line).text() == (line == 0 ? prefix + "0;" : common)
							&& surface.right_pane()->document()->findBlockByNumber(line).text() == (line == 0 ? prefix + "1;" : common),
						"entry-pressure source differs from the independent fixture recipe");
			}
		};
		for (std::size_t index = 0U; index <= limits.CacheEntries; ++index) {
			const auto file  = 100 + static_cast<int>(index);
			const auto phase = "cache_entry_selection_" + std::to_string(index + 1U);
			require(measure_wait(aApplication, aBudget, aReport, phase.c_str(), [&] { select_file(window, source_path(file)); }, [&] { return ready(file); }), "entry-pressure selection timed out");
			verify(file);
			const auto work = access::diff_work_evidence(aApplication);
			require(work.CacheEntries == std::min(index + 1U, limits.CacheEntries) && work.CacheBytes < limits.CacheBytes,
					"entry pressure exceeded its capacity or encountered byte pressure instead");
			aReport.EntryPeakCacheEntries = std::max(aReport.EntryPeakCacheEntries, work.CacheEntries);
			aReport.EntryPeakCacheBytes   = std::max(aReport.EntryPeakCacheBytes, work.CacheBytes);
			++aReport.EntrySelections;
			if (index == 0U) {
				firstDiff  = surface.current_result();
				firstIndex = surface.current_semantic_index();
			}
			if (index < limits.CacheEntries) {
				require(!firstDiff.expired() && !firstIndex.expired(), "oldest entry was evicted before capacity pressure");
			}
			if (index % 16U == 0U) {
				sample_resources(aReport, &aApplication, phase.c_str());
			}
		}
		aReport.EntryEvictedDiff  = firstDiff.expired();
		aReport.EntryEvictedIndex = firstIndex.expired();
		require(aReport.EntryEvictedDiff && aReport.EntryEvictedIndex, "entry eviction still retains the oldest raw diff or prepared index");
		require(measure_wait(aApplication, aBudget, aReport, "cache_entry_revisit", [&] { select_file(window, source_path(100)); }, [&] { return ready(100); }), "evicted file revisit timed out");
		verify(100);
		aReport.EntryRevisitExact = aReport.Phases.back().PulsesWithDiffWork != 0U;
		require(aReport.EntryRevisitExact, "evicted comparison did not run the normal diff materialization job");
		const auto work = access::diff_work_evidence(aApplication);
		require(work.FixtureDiffHelperInvocations == 0U && work.CacheEntries == limits.CacheEntries && work.CacheBytes < limits.CacheBytes,
				"revisit bypassed composition or exceeded default limits");
		aReport.DiffCacheEntries = work.CacheEntries;
		aReport.DiffCacheBytes   = work.CacheBytes;
		aReport.ExactFinalSource = true;
		aReport.WindowWidth      = window.width();
		aReport.WindowHeight     = window.height();
		aReport.DevicePixelRatio = window.devicePixelRatioF();
		aReport.Rendered         = window.grab().save(QString::fromLocal8Bit(aImage));
		require(aReport.Rendered, "entry-pressure public window render failed");
	}

	auto run_benchmark(QApplication& qtApplication, int argc, char** argv) -> int
	{
		if (argc != 4 && (argc != 5 || (std::string_view{ argv[4] } != "--unavailable" && std::string_view{ argv[4] } != "--resources" && std::string_view{ argv[4] } != "--history" && std::string_view{ argv[4] } != "--loaded-close" && std::string_view{ argv[4] } != "--cache-entries"))) {
			return EXIT_FAILURE;
		}
		QTemporaryDir data;
		if (!data.isValid()) {
			return EXIT_FAILURE;
		}
		benchmark_report report;
		report.UnavailableCourse                                           = argc == 5 && std::string_view{ argv[4] } == "--unavailable";
		report.LoadedCloseCourse                                           = argc == 5 && std::string_view{ argv[4] } == "--loaded-close";
		report.ResourceCourse                                              = report.LoadedCloseCourse || (argc == 5 && std::string_view{ argv[4] } == "--resources");
		report.HistoryCourse                                               = argc == 5 && std::string_view{ argv[4] } == "--history";
		report.CacheEntryCourse                                            = argc == 5 && std::string_view{ argv[4] } == "--cache-entries";
		const bool                                     observeResources    = report.ResourceCourse || report.CacheEntryCourse;
		const bool                                     observeLoadedOwners = report.LoadedCloseCourse || report.CacheEntryCourse;
		std::weak_ptr<const core::computed_file_diff>  resourceDiff;
		std::weak_ptr<const core::diff_semantic_index> resourceIndex;
		QPointer<QWidget>                              resourceWindow;
		QPointer<QTextDocument>                        resourceLeftDocument, resourceRightDocument;
		std::optional<core::review_epoch_id>           closeTarget;
		std::optional<core::file_comparison_key>       closeComparison;
		std::unique_ptr<app::github_application>       application;
		try {
			std::ifstream     input{ std::filesystem::path{ BPR_LARGE_REVIEW_FIXTURE_ROOT } / "manifest.json" };
			const std::string bytes{ std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{ } };
			fixture_manifest  manifest;
			require(!glz::read<glz::opts{ .error_on_unknown_keys = false }>(manifest, bytes) && manifest.schema == "bpr.large-review-fixture.v1" && manifest.heads.size() == 32U, "stored manifest");
			report.Base                       = manifest.base;
			report.Head                       = report.HistoryCourse ? manifest.heads.front() : manifest.heads.back();
			report.PackBytes                  = manifest.pack_bytes;
			report.Platform                   = QSysInfo::prettyProductName().toStdString();
			report.Architecture               = QSysInfo::currentCpuArchitecture().toStdString();
			report.QtVersion                  = qVersion();
			report.Style                      = qtApplication.style()->objectName().toStdString();
			report.BuildConfiguration         = BPR_BENCH_BUILD_CONFIGURATION;
			report.LogicalProcessors          = QThread::idealThreadCount();
			auto fixtureOptions               = app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = data.path().toStdString(), .GitExecutable = std::filesystem::path{ argv[1] } };
			fixtureOptions.LargeReviewHistory = report.HistoryCourse;
			if (report.UnavailableCourse) {
				// Retain the production matrix ceiling and disable only its optional
				// ordered index, so the stored 20,000-line file exceeds real limits.
				fixtureOptions.DiffComputeLimits.MaximumPrimaryIndexWork = 0U;
			}
			sample_resources(report, nullptr, "before_application");
			auto startupTerminal = false;
			auto startupHandle   = access::start(app::detail::github_public_path_fixture::large_review_v1, std::move(fixtureOptions), [&](auto aResult) {
				startupTerminal = true;
				if (aResult) {
					application = std::move(*aResult);
				}
			});
			{
				QElapsedTimer startupBudget;
				startupBudget.start();
				while (!(startupTerminal && startupHandle && startupHandle->is_quiescent())) {
					require(startupBudget.elapsed() < 60'000, "application startup reaches a terminal");
					QApplication::processEvents(QEventLoop::AllEvents, 5);
				}
			}
			require(application != nullptr, "application startup succeeds");
			auto& window = application->window();
			window.resize(1600, 1000);
			application->show();
			auto&         surface = *window.comparison_surface();
			QElapsedTimer budget;
			budget.start();
			sample_resources(report, application.get(), "application_constructed");
			require(measure_wait(*application, budget, report, "fixture_connect", [&] { window.github_connect_action()->trigger(); }, [&] { return window.github_status_text() == "GitHub account connected." && window.github_open_action()->isEnabled(); }), "connect timed out");
			window.session_panel()->owner_editor()->setText(QStringLiteral("bpr-fixtures"));
			window.session_panel()->repository_editor()->setText(QStringLiteral("large-review"));
			window.session_panel()->pull_request_editor()->setValue(17);
			require(measure_wait(*application, budget, report, "paged_review_and_git_load", [&] { window.github_open_action()->trigger(); }, [&] { return window.files_drawer()->contents_view()->model()->rowCount() == 515 && access::evidence(*application).SmartFetchCount == 1 && application->status().EpochStatus == core::epoch_status::ready && access::diff_work_evidence(*application).NavigationReady && quiet(*application); }), "review load timed out");
			verify_snapshot(*application, manifest, report);
			sample_resources(report, application.get(), "review_loaded");
			if (report.CacheEntryCourse) {
				run_cache_entries(*application, manifest, report, budget, argv[3]);
			} else if (report.HistoryCourse) {
				run_history(*application, manifest, report, budget, argv[3]);
			} else if (report.UnavailableCourse) {
				require(measure_wait(*application, budget, report, "unavailable_file_selection", [&] { select_file(window, QStringLiteral("large/comparison.cpp")); }, [&] { return selected(surface, QStringLiteral("large/comparison.cpp")) && quiet(*application); }), "unavailable selection timed out");
				const auto& alignment = surface.current_result()->Alignment;
				require(alignment.Content == core::diff_content_state::unavailable && alignment.Diagnostic.Code == core::diff_reason_code::compute_limit, "real host result must exceed the retained comparison limit");
				require(surface.status_text() == QStringLiteral("Comparison unavailable — file exceeds the comparison work limit"), "unavailable comparison falsely reports success");
				require(surface.left_pane()->toPlainText().isEmpty() && surface.right_pane()->toPlainText().isEmpty() && !surface.selection(), "unavailable comparison retains stale source");
				auto* label = surface.findChild<QLabel*>(QStringLiteral("diffWorkbenchStatus"));
				require(label && label->isVisible() && label->textInteractionFlags().testFlag(Qt::TextSelectableByKeyboard) && label->textInteractionFlags().testFlag(Qt::TextSelectableByMouse), "unavailable message must support copying");
				report.UnavailableReasonVerified = true;
				report.WindowWidth               = window.width();
				report.WindowHeight              = window.height();
				report.DevicePixelRatio          = window.devicePixelRatioF();
				report.Rendered                  = window.grab().save(QString::fromLocal8Bit(argv[3]));
				require(report.Rendered, "unavailable full-window capture failed");
				require(measure_wait(*application, budget, report, "binary_file_selection", [&] { select_file(window, QStringLiteral("assets/data.bin")); }, [&] { return selected(surface, QStringLiteral("assets/data.bin")) && quiet(*application); }), "binary selection timed out");
				require(surface.current_result()->Alignment.Content == core::diff_content_state::binary && surface.status_text() == QStringLiteral("Binary file comparison"), "binary status must be distinct");
				report.BinaryStatusVerified = true;
				require(measure_wait(*application, budget, report, "supported_file_recovery", [&] { select_file(window, source_path(31)); }, [&] { return selected(surface, source_path(31)) && quiet(*application); }), "supported recovery timed out");
				report.ExactFinalSource = surface.left_pane()->document()->firstBlock().text() == "int fixture_0031 = 0;" && surface.right_pane()->document()->firstBlock().text() == "int fixture_0031 = 1;";
				require(report.ExactFinalSource && surface.status_text().startsWith(QStringLiteral("Diff ready — 1 semantic changes")), "supported file must restore exact normal display");
				require(access::diff_work_evidence(*application).FixtureDiffHelperInvocations == 0U, "unavailable course bypassed the real diff host");
			} else {
				const auto          beforeMeasurement = surface.statistics().MeasurementBatches;
				QTimer              inputDuringMeasurement;
				QElapsedTimer       measurementInputClock;
				std::vector<double> measurementInputIntervals;
				inputDuringMeasurement.setInterval(1);
				QObject::connect(&inputDuringMeasurement, &QTimer::timeout, &window, [&] {
					if (!surface.geometry_pending() || surface.statistics().MeasurementBatches <= beforeMeasurement
						|| !selected(surface, QStringLiteral("large/comparison.cpp"))) {
						return;
					}
					if (measurementInputClock.isValid()) {
						measurementInputIntervals.push_back(static_cast<double>(measurementInputClock.nsecsElapsed()) / 1'000'000.0);
					}
					measurementInputClock.start();
					const auto  frames = surface.statistics().FrameApplications;
					QWheelEvent inputEvent{ QPointF{ 20, 20 }, QPointF{ 20, 20 }, QPoint{ 0, -1 }, QPoint{ }, Qt::NoButton, Qt::NoModifier, Qt::ScrollUpdate, false };
					QApplication::sendEvent(surface.right_pane()->viewport(), &inputEvent);
					require(inputEvent.isAccepted() && frames == surface.statistics().FrameApplications,
							"input during measurement must not apply partial geometry");
					++report.InputsDuringMeasurement;
				});
				inputDuringMeasurement.start();
				require(measure_wait(*application, budget, report, "large_file_selection", [&] { select_file(window, QStringLiteral("large/comparison.cpp")); }, [&] { return selected(surface, QStringLiteral("large/comparison.cpp")) && quiet(*application) && surface.scroll_controller()->maximum_position() > 1000.0; }), "large selection timed out");
				inputDuringMeasurement.stop();
				if (!measurementInputIntervals.empty()) {
					std::ranges::sort(measurementInputIntervals);
					report.MaxMeasurementInputIntervalMs = measurementInputIntervals.back();
					report.P95MeasurementInputIntervalMs = measurementInputIntervals[(measurementInputIntervals.size() - 1U) * 95U / 100U];
				}
				report.LargeMeasurementBatches      = surface.statistics().MeasurementBatches - beforeMeasurement;
				report.MaximumMeasurementBatchLines = surface.statistics().MaximumMeasurementBatchLines;
				require(report.InputsDuringMeasurement > 0U && report.LargeMeasurementBatches > 1U
							&& report.MaximumMeasurementBatchLines <= 512U,
						"large native measurement must yield bounded batches to real input");
				verify_large(surface, manifest);
				sample_resources(report, application.get(), "large_file_ready");
				if (report.ResourceCourse) {
					resourceDiff  = surface.current_result();
					resourceIndex = surface.current_semantic_index();
				}
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
				report.ScrollIndexAdoptions   = after.SemanticIndexAdoptions - before.SemanticIndexAdoptions;
				report.ScrollGeometryRebuilds = after.GeometryRebuilds - before.GeometryRebuilds;
				report.ScrollPaneMeasurements = after.PaneMeasurements - before.PaneMeasurements;
				require(report.WheelEvents == 1000U && report.ScrollFrames == 1000U && report.ScrollIndexAdoptions == 0U && report.ScrollGeometryRebuilds == 0U && report.ScrollPaneMeasurements == 0U
							&& application->status().ClassificationSubmissions == classificationBefore && controller->position() == 1000.0,
						"scroll gesture work/position contract");
				surface.shared_scrollbar()->setValue(0);
				report.WindowWidth      = window.width();
				report.WindowHeight     = window.height();
				report.DevicePixelRatio = window.devicePixelRatioF();
				report.Rendered         = window.grab().save(QString::fromLocal8Bit(argv[3]));
				require(report.Rendered, "full-window capture failed");

				if (!report.ResourceCourse) {
					auto* conversationModel = window.conversation_model();
					QObject::connect(conversationModel, &QAbstractItemModel::rowsInserted, &window,
									 [&report, conversationModel] {
										 report.PeakConversationRootsDuringSelections = std::max(report.PeakConversationRootsDuringSelections,
																								 static_cast<std::size_t>(conversationModel->rowCount()));
									 });
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
					require(report.PeakConversationRootsDuringSelections <= 2U, "file switching temporarily widened conversations to unrelated threads");
					report.SupersessionDrainMs = static_cast<double>(drain.nsecsElapsed()) / 1'000'000.0;
					require(report.SelectionsWithOutstandingDiff != 0U, "supersession never overlapped outstanding diff work");
					report.ExactFinalSource = surface.left_pane()->document()->firstBlock().text() == "int fixture_0031 = 0;" && surface.right_pane()->document()->firstBlock().text() == "int fixture_0031 = 1;";
					require(report.ExactFinalSource, "stale source adopted after supersession");
				}
				const auto work         = access::diff_work_evidence(*application);
				report.DiffCacheEntries = work.CacheEntries;
				report.DiffCacheBytes   = work.CacheBytes;
				require(work.FixtureDiffHelperInvocations == 0U, "fixture diff bypass was used");
			}
			if (report.ResourceCourse && !report.LoadedCloseCourse) {
				const auto key              = surface.current_result()->Alignment.Key;
				const auto target           = core::review_epoch_id{ .Authority   = key.LeftAnchor.Authority,
																	 .Incarnation = key.LeftAnchor.Incarnation,
																	 .Ordinal     = key.LeftAnchor.Epoch };
				const auto maintenanceReady = [&] {
					const auto work = access::diff_work_evidence(*application);
					return !work.EpochMaintenancePending && work.EpochOutstandingJobs == 0U && quiet(*application);
				};
				require(measure_wait(*application, budget, report, "public_prune", [&] { confirm_fixture_maintenance(*window.epoch_selectors()->prune_action(), QStringLiteral("pruneSelectedEpochsConfirmationDialog")); }, maintenanceReady), "public prune timed out");
				const auto pruned = access::maintenance_evidence(*application, target);
				require(pruned.PruneIntentDispatchCount == 1U && !pruned.TargetBaseRefPresent && !pruned.TargetHeadRefPresent
							&& pruned.TargetBaseAvailability == static_cast<int>(core::anchor_availability::pruned)
							&& pruned.TargetHeadAvailability == static_cast<int>(core::anchor_availability::pruned),
						"public prune did not remove exact refs and preserve pruned catalog identities");
				report.PruneReleasedDiff      = resourceDiff.expired();
				report.PruneReleasedIndex     = resourceIndex.expired();
				report.PruneClearedProjection = !surface.current_result() && surface.left_pane()->toPlainText().isEmpty() && surface.right_pane()->toPlainText().isEmpty();
				sample_resources(report, application.get(), "after_prune");
				require(window.grab().save(QString::fromLocal8Bit(argv[3]) + QStringLiteral(".pruned.png")), "pruned window render failed");
				std::cerr << "prune-ownership: diff=" << report.PruneReleasedDiff << " index=" << report.PruneReleasedIndex << " projection=" << report.PruneClearedProjection << '\n';
				require(measure_wait(*application, budget, report, "public_cache_delete", [&] { confirm_fixture_maintenance(*window.epoch_selectors()->delete_cache_action(), QStringLiteral("deleteRepositoryCacheConfirmationDialog")); }, maintenanceReady), "public cache deletion timed out");
				const auto deleted = access::maintenance_evidence(*application, target);
				require(deleted.DeleteIntentDispatchCount == 1U && deleted.CacheDeletionMarkerPresent
							&& !deleted.RepositoryPresent && deleted.TargetBaseCacheDeleted && deleted.TargetHeadCacheDeleted,
						"public deletion did not retire exact disposable cache/catalog anchors");
				report.DeleteReleasedDiff  = resourceDiff.expired();
				report.DeleteReleasedIndex = resourceIndex.expired();
				sample_resources(report, application.get(), "after_cache_delete");
			}
			if (observeLoadedOwners) {
				if (report.CacheEntryCourse) {
					resourceDiff  = surface.current_result();
					resourceIndex = surface.current_semantic_index();
				}
				const auto& key                 = surface.current_result()->Alignment.Key;
				closeComparison                 = key;
				closeTarget                     = core::review_epoch_id{ key.LeftAnchor.Authority, key.LeftAnchor.Incarnation, key.LeftAnchor.Epoch };
				resourceWindow                  = &window;
				resourceLeftDocument            = surface.left_pane()->document();
				resourceRightDocument           = surface.right_pane()->document();
				report.CloseLeftDocumentBlocks  = resourceLeftDocument->blockCount();
				report.CloseRightDocumentBlocks = resourceRightDocument->blockCount();
				// The fixture's final newline adds an empty native Qt block.
				const auto expectedBlocks             = report.CacheEntryCourse ? 9 : 20'001;
				const auto expectedPath               = report.CacheEntryCourse ? source_path(100) : QStringLiteral("large/comparison.cpp");
				report.CloseStartedWithLiveComparison = !resourceDiff.expired() && !resourceIndex.expired()
					&& selected(surface, expectedPath)
					&& report.CloseLeftDocumentBlocks == expectedBlocks && report.CloseRightDocumentBlocks == expectedBlocks
					&& resourceLeftDocument->lastBlock().text().isEmpty() && resourceRightDocument->lastBlock().text().isEmpty();
				require(report.CloseStartedWithLiveComparison, "loaded-close course lost the large comparison before closing");
				sample_resources(report, application.get(), "before_loaded_close");
			}
			if (observeResources) {
				// Match main's last-window-close -> application shutdown handoff;
				// measure_wait drives the existing shutdown poll to completion.
				QObject::connect(&qtApplication, &QGuiApplication::lastWindowClosed, &window, [&] {
					report.WindowCloseObserved = true;
					application->begin_shutdown();
				});
			}
			report.ShutdownComplete = measure_wait(*application, budget, report, "shutdown", [&] {
			if (observeResources) { window.close(); } else { application->begin_shutdown(); } }, [&] { return application->status().ShutdownComplete; });
			require(report.ShutdownComplete && !application->status().ShutdownCleanupFailed, "shutdown failed");
			if (observeResources) {
				require(report.WindowCloseObserved && !window.isVisible(), "public window close did not start shutdown");
				sample_resources(report, application.get(), "shutdown_drained");
				report.ShutdownReleasedDiff  = resourceDiff.expired();
				report.ShutdownReleasedIndex = resourceIndex.expired();
				if (closeTarget) {
					const auto retained               = access::maintenance_evidence(*application, *closeTarget, closeComparison);
					report.ClosePreservedEpochStorage = retained.PruneIntentDispatchCount == 0U && retained.DeleteIntentDispatchCount == 0U
						&& retained.RepositoryPresent && retained.TargetBaseRefPresent && retained.TargetHeadRefPresent
						&& retained.TargetBaseRefOid == manifest.base && retained.TargetBaseCatalogOid == manifest.base
						&& retained.TargetHeadRefOid == manifest.heads.back() && retained.TargetHeadCatalogOid == manifest.heads.back();
					require(report.ClosePreservedEpochStorage, "window close changed retained Git/catalog data or performed maintenance");
				}
				application.reset();
				QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
				QApplication::processEvents();
				report.DestroyReleasedDiff  = resourceDiff.expired();
				report.DestroyReleasedIndex = resourceIndex.expired();
				if (observeLoadedOwners) {
					report.DestroyReleasedWindow    = resourceWindow.isNull();
					report.DestroyReleasedDocuments = resourceLeftDocument.isNull() && resourceRightDocument.isNull();
				}
				sample_resources(report, nullptr, "application_destroyed");
				std::cerr << "resource-verdict:" << *glz::write_json(report) << '\n';
				if (report.ResourceCourse && !report.LoadedCloseCourse) {
					require(report.PruneReleasedDiff && report.PruneReleasedIndex && report.PruneClearedProjection && report.DeleteReleasedDiff && report.DeleteReleasedIndex,
							"maintenance retains the raw diff, prepared index or native source");
				}
				require(report.DestroyReleasedDiff && report.DestroyReleasedIndex, "closed comparison retains raw diff or prepared index");
				require(!observeLoadedOwners || (report.DestroyReleasedWindow && report.DestroyReleasedDocuments),
						"closed application retains the window or large source documents");
			}
			auto encoded = glz::write_json(report);
			require(encoded.has_value(), "measurement serialization");
			std::ofstream output{ argv[2], std::ios::binary };
			output << *encoded << '\n';
			output.close();
			require(output.good(), "measurement write");
			std::cout << (report.CacheEntryCourse ? "LARGE_REVIEW_CACHE_ENTRIES_OK\n" : report.LoadedCloseCourse ? "LARGE_REVIEW_LOADED_CLOSE_OK\n"
							  : report.HistoryCourse                                                             ? "LARGE_REVIEW_HISTORY_OK\n"
							  : report.ResourceCourse                                                            ? "LARGE_REVIEW_RESOURCES_OK\n"
							  : report.UnavailableCourse                                                         ? "UNAVAILABLE_DIFF_COMPOSED_OK\n"
																												 : "LARGE_REVIEW_COMPOSED_BENCHMARK_OK\n")
					  << *encoded << '\n';
			return EXIT_SUCCESS;
		} catch (const std::exception& error) {
			std::cerr << "large-review-benchmark-failed: " << error.what() << '\n';
			if (const auto partial = glz::write_json(report); partial) {
				std::cerr << "large-review-partial:" << *partial << '\n';
			}
			return EXIT_FAILURE;
		}
	}

} // namespace

int main(int argc, char** argv)
{
	QApplication qtApplication{ argc, argv };
	QApplication::setQuitOnLastWindowClosed(false);
	// Native last-window-close delivery needs QApplication's main event loop,
	// even though individual measurement phases use nested event loops.
	QTimer::singleShot(0, &qtApplication, [&] {
		qtApplication.exit(bpr::e2e::large_review::run_benchmark(qtApplication, argc, argv));
	});
	return qtApplication.exec();
}
