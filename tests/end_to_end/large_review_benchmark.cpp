#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../../src/app/src/github_public_path_fixture.hpp"
#include "../support/diagnostic_files.hpp"
#include "large_review_observers.hpp"
#include "private_data_root.hpp"

#include "bpr/application/epoch_runtime_ports.hpp"
#include "bpr/application/release_diagnostic_snapshot.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/qt/comparison_scroll_controller.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/conversation_finder.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/document_tab_strip.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_strip.hpp"

#include <glaze/json.hpp>

#include <QAccessible>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QPixmap>
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
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::e2e::large_review {
	using namespace bpr;
	using access = app::detail::github_application_fixture_access;

	/// [[BPR-0967]]. The numeric limits each platform enforces, every one
	/// justified by that platform's optimized measurements. The Mac basis is
	/// the M4 Pro report; Windows and Linux are the i9-10900KF station runs
	/// (docs/verification/BPR-0967-station). A Mac limit is kept where the
	/// platform's highest optimized observation is at most two thirds of it
	/// (memory: at most 1/1.1 of it); otherwise the platform's limit is that
	/// observation plus half (memory: plus a tenth, rounded up to 8 MiB).
	/// The 100 ms input target is the same everywhere. A phase measured at or
	/// above 90 ms of it on a platform is named in KnownInputMisses: reported
	/// in every report and in InputBudgetFailures when over, not enforced,
	/// and owned by a follow-up card. No limit is raised to absorb a miss.
	///
	/// A Debug build enforces nothing it lacks a Debug justification for:
	/// on macOS the historical Debug limits ([[BPR-1247]]), elsewhere none.
	/// Exact source, cache, eviction, release and lifecycle assertions are
	/// enforced on every platform and configuration.
	inline constexpr bool OptimizedBuild = std::string_view{ BPR_BENCH_BUILD_CONFIGURATION } != "Debug";
	// [[BPR-1265]]. The exit code the cache-bytes row reads as Skipped (SKIP_RETURN_CODE).
	inline constexpr int           cache_bytes_debug_skip_exit_code = 77;
	inline constexpr std::uint64_t MiB                              = 1024U * 1024U;
#if defined(__APPLE__)
	inline constexpr bool             PlatformBudgetsEnforced                    = true;
	inline constexpr std::string_view PlatformLimitBasis                         = "macOS arm64 M4 Pro 48 GiB, Qt 6.11.2, Apple clang 21 Release at 5a26d6b1";
	inline constexpr std::uint64_t    PlatformFootprintGrowthLimitBytes          = 160U * MiB;
	inline constexpr std::uint64_t    PlatformCacheByteFootprintGrowthLimitBytes = 184U * MiB;
	inline constexpr double           PlatformActiveCloseQuiescenceLimitMs       = 100.0;
	// [[BPR-1264]]. approximate_file_selection (96.3-111.1, the work-limited
	// approximate alignment course) was the publication that replaced both
	// panes' text in one GUI turn. Filled over bounded turns it measures
	// 18.6-18.9, and every macOS phase gates the target.
	inline constexpr std::array<std::string_view, 0> PlatformKnownInputMisses{ };
	inline constexpr int                             RunBudgetScale = 1;
#elif defined(_WIN32)
	inline constexpr bool             PlatformBudgetsEnforced = OptimizedBuild;
	inline constexpr std::string_view PlatformLimitBasis      = "Windows 11 x64 i9-10900KF 128 GiB, Qt 6.11.2, MSVC 19.51 /O2 Release at 682ea1392";
	// Private commit growth: 120.9 MiB lifetime peak in resources and
	// loaded-close keeps 160; the byte course's 185.43 MiB peak gives 208.
	inline constexpr std::uint64_t PlatformFootprintGrowthLimitBytes          = 160U * MiB;
	inline constexpr std::uint64_t PlatformCacheByteFootprintGrowthLimitBytes = 208U * MiB;
	// Public close reached quiescence in 98.854 ms: 150.
	inline constexpr double PlatformActiveCloseQuiescenceLimitMs = 150.0;
	// [[BPR-1264]]. large_file_selection, large_source_revisit_render,
	// approximate_file_selection and first_tab_file were the publication that
	// replaced both panes' text in one GUI turn: 94.5-191.3 without the bounded
	// fill, 18.6-39.1 with it. shutdown, files_drawer_open (whole phase),
	// active_window_close and paged_review_and_git_load no longer reproduce
	// with or without it (at most 28.2, 81.0, 28.2 and 58.4). Every Windows
	// phase gates the target.
	inline constexpr std::array<std::string_view, 0> PlatformKnownInputMisses{ };
	// Windows Debug runs the courses several times slower than Release: the
	// resources course outran its 90 s clock (105.94 s row), and the byte
	// course then took 1,626 s of a doubled 1,680 s clock. Three times keeps
	// that course at about two thirds of its clock.
	inline constexpr int RunBudgetScale = OptimizedBuild ? 1 : 3;
#else
	inline constexpr bool             PlatformBudgetsEnforced = OptimizedBuild;
	inline constexpr std::string_view PlatformLimitBasis      = "Linux x64 (WSL2) i9-10900KF, Qt 6.11.2, GCC 15.2 -O3 Release at e257b236a";
	// Resident-set growth: 147.04 MiB in loaded-close gives 168; the byte
	// course's 196.01 MiB high-water mark gives 216.
	inline constexpr std::uint64_t PlatformFootprintGrowthLimitBytes          = 168U * MiB;
	inline constexpr std::uint64_t PlatformCacheByteFootprintGrowthLimitBytes = 216U * MiB;
	inline constexpr double        PlatformActiveCloseQuiescenceLimitMs       = 100.0;
	// [[BPR-1264]]. The four named phases (large_file_selection 94.4-108.0,
	// large_source_revisit_render 90.4-115.3, approximate_file_selection
	// 117.4-163.4, first_tab_file 92.7-103.3) were one publication replacing
	// both panes' text in a GUI turn. Filled over bounded turns, they measure
	// 23.4-36.6, and every Linux phase gates the target.
	inline constexpr std::array<std::string_view, 0> PlatformKnownInputMisses{ };
	inline constexpr int                             RunBudgetScale = 1;
#endif

	[[nodiscard]] auto known_input_miss(std::string_view aPhase) -> bool
	{
		return std::ranges::find(PlatformKnownInputMisses, aPhase) != PlatformKnownInputMisses.end();
	}

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
		double      ElapsedMs = 0, ActionMs = 0, FinalIntervalMs = 0, MaxPulseIntervalMs = 0, P95PulseIntervalMs = 0;
		std::size_t Pulses = 0, PulsesWithDiffWork = 0, PulsesWithWorkbenchWork = 0;
		std::size_t PeakDiffJobs = 0, PeakWorkbenchRequests = 0, PeakEpochJobs = 0;
	};
	struct resource_sample
	{
		std::string   Phase;
		std::uint64_t ResidentBytes = 0, PhysicalFootprintBytes = 0, LifetimePeakFootprintBytes = 0, LiveHeapBytes = 0, DiffCacheBytes = 0;
		// Above the course's first (before_application) sample.
		std::uint64_t FootprintGrowthBytes = 0, LifetimePeakGrowthBytes = 0, LiveHeapGrowthBytes = 0;
		std::size_t   DiffCacheEntries = 0, DiffJobs = 0, EpochJobs = 0;
	};

	struct latency_budget
	{
		std::string Name;
		double      MeasuredMs = 0, LimitMs = 0;
		bool        Passed = false;
		/// [[BPR-1247]]. False for a limit justified only by optimized
		/// measurements, in a Debug build: measured and reported, not a gate.
		bool Enforced = true;
	};
	struct active_input_measurement
	{
		std::string Path;
		double      PostedAtMs = 0, DeliveredAfterMs = 0, AppliedAfterMs = 0;
		std::size_t DiffAtPost = 0, DiffAtDelivery = 0, EpochAtDelivery = 0, WorkbenchAtDelivery = 0;
		bool        PendingAtDelivery = false, Delivered = false, Applied = false;
	};

	// Observe the queued event before the visible file tree handles it. Host counters cover
	// admitted work through decision delivery, not continuous worker occupancy.
	class active_input_observer final : public QObject
	{
	public:
		active_input_observer(app::github_application& aApplication, std::vector<active_input_measurement>& aInputs, std::function<void(const active_input_measurement&)> aOnDelivery)
			: Application{ aApplication }
			, Inputs{ aInputs }
			, OnDelivery{ std::move(aOnDelivery) }
		{ }
		QElapsedTimer Clock;
		bool          Armed = false;

	protected:
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override
		{
			if (Armed && aEvent->type() == QEvent::KeyPress) {
				auto&      input          = Inputs.back();
				const auto work           = access::diff_work_evidence(Application);
				input.DeliveredAfterMs    = static_cast<double>(Clock.nsecsElapsed()) / 1'000'000.0;
				input.DiffAtDelivery      = work.OutstandingJobs;
				input.EpochAtDelivery     = work.EpochOutstandingJobs;
				input.WorkbenchAtDelivery = work.WorkbenchOutstandingRequests;
				input.PendingAtDelivery   = work.Pending;
				input.Delivered           = true;
				Armed                     = false;
				OnDelivery(input);
			}
			return QObject::eventFilter(aWatched, aEvent);
		}

	private:
		app::github_application&                             Application;
		std::vector<active_input_measurement>&               Inputs;
		std::function<void(const active_input_measurement&)> OnDelivery;
	};

	struct extended_measurement
	{
		bool                  AllocationProfile = false;
		live_heap_observation Heap;
		std::uint64_t         SampledPeakHeapGrowthLimitBytes = 0;
		double                ProvisionalCaptureMs = 0, AcceptedCaptureMs = 0;
		double                QApplicationStartupMs = 0, MainToHomeRenderMs = 0, MainToFirstSourceRenderMs = 0;
		double                ProvisionalMetadataRenderMs = 0, AcceptedMetadataRenderMs = 0;
		bool                  ProvisionalMetadataObserved = false, AcceptedMetadataObserved = false;
		std::string           MetadataTitle;
		bool                  ProviderCancelCourse = false, ProviderStopped = false;
		std::size_t           ProviderHeldInputs = 0;
		double                ProviderHeldMs = 0, ProviderCancelAckMs = 0, ProviderExitMs = 0;
		bool                  PathologicalCourse = false, ExactGeneratedSource = false;
		// [[BPR-0967]]. BudgetsEnforced is false where no numeric limit is
		// justified for this platform and configuration; each limit missed
		// there is named in UnenforcedLimitMisses. KnownInputMisses names the
		// platform's phases at or over 90 ms of the 100 ms target. Failure is
		// the exception that ended the course; WaitExpiry says which wait ended
		// unready and why. (benchmark_report is at glaze's reflected-member limit.)
		bool                     BudgetsEnforced = PlatformBudgetsEnforced;
		std::vector<std::string> UnenforcedLimitMisses;
		std::vector<std::string> KnownInputMisses{ PlatformKnownInputMisses.begin(), PlatformKnownInputMisses.end() };
		std::string              MemoryMethod     = native_memory_method(), LimitBasis{ PlatformLimitBasis }, Failure, WaitExpiry;
		int                      CourseClockScale = RunBudgetScale;
	};

	struct benchmark_report
	{
		extended_measurement                  Extended;
		std::string                           Schema               = "bpr.large-review-measurement.v1";
		bool                                  ResourceCourse       = false;
		bool                                  ResponsivenessCourse = false, StartupRendered = false;
		bool                                  ActiveWorkCourse = false, ActiveFinalHeldExact = false, ActiveCloseDrained = false;
		std::vector<active_input_measurement> ActiveInputs;
		std::size_t                           ActiveSourceObservations = 0, ActiveCloseDiffJobs = 0, ActiveCloseEpochJobs = 0;
		double                                ActiveCloseDispatchMs = 0, ActiveCloseDrainMs = 0;
		double                                CompositionStartupMs = 0, MaxWheelEventMs = 0;
		std::size_t                           InteractionFrames = 0;
		// The inherited 100 ms target is evaluated for every complete phase.
		// A successful measurement course is not acceptance of these failures.
		double                      InputIntervalLimitMs = 100.0;
		std::vector<std::string>    InputBudgetFailures;
		std::vector<latency_budget> CompletionBudgets;
		bool                        FirstTabCourse = false, FirstTabExact = false;
		bool                        CacheByteCourse = false, ByteEvictedDiff = false, ByteEvictedIndex = false, ByteRevisitExact = false, ByteSupersessionHeldExact = false;
		std::size_t                 ByteHeldSourceObservations = 0U;
		std::size_t                 ByteSelections = 0U, BytePeakCacheEntries = 0U, ByteSupersessionsWithWork = 0U;
		std::uint64_t               ByteEntryBytes = 0U, ByteAdmittedBytes = 0U, BytePeakCacheBytes = 0U;
		bool                        CacheEntryCourse = false, EntryEvictedDiff = false, EntryEvictedIndex = false, EntryRevisitExact = false;
		std::size_t                 EntrySelections = 0U, EntryPeakCacheEntries = 0U;
		std::uint64_t               EntryPeakCacheBytes = 0U;
		bool                        LoadedCloseCourse = false, CloseStartedWithLiveComparison = false, ClosePreservedEpochStorage = false;
		int                         CloseLeftDocumentBlocks = 0, CloseRightDocumentBlocks = 0;
		bool                        ShutdownReleasedDiff = false, ShutdownReleasedIndex = false, DestroyReleasedWindow = false, DestroyReleasedDocuments = false;
		bool                        HistoryCourse  = false;
		std::size_t                 EpochsObserved = 0U, RetainedEpochsVerified = 0U, EpochSelections = 0U, EpochSelectionsWithOutstandingWork = 0U;
		std::size_t                 HistoryPeakRows = 0U, HistoryModelResets = 0U;
		// Historical Debug thresholds are retained without relaxation and
		// revalidated by the optimized Mac BPR-0967 integration course.
		double                       HistoryInputIntervalLimitMs = 100.0;
		double                       HistoryRefreshLimitMs = 12'000.0, HistorySourceSelectionLimitMs = 2'000.0;
		std::vector<resource_sample> Resources;
		// [[BPR-0893]] / [[BPR-1032]] historical Debug diagnosis below is
		// provenance only. BPR-0967 revalidates these unchanged ceilings on
		// Release, M4 Pro (48 GiB), Qt 6.11.2. They bound sampled and lifetime-peak footprint growth above the
		// before_application sample, not peak live heap or other platforms.
		//
		// The peak bounds physical footprint at every sample. Qt 6.11 keeps a
		// HarfBuzz buffer in every shaped QTextLayout until the layout is
		// destroyed (qtbase 8209078e0e), and a pane keeps one layout per block,
		// so measuring the large comparison natively held about 70 MiB more
		// live than under Qt 6.10.3 and the peak rose from 138 to 203 MiB.
		// [[BPR-1032]] measures unwrapped panes in transient layouts instead.
		// Live growth at large_file_ready fell from 164.2 to 101.1 MiB, and the
		// highest peak of the resources, loaded-close and cache-entry courses
		// fell to 138.7 MiB, where Qt 6.10.3 left it, so the original 160 MiB
		// returns for them. The cache-byte course fills the diff cache to its
		// byte budget on purpose. It peaked at 165.5-166.7 MiB (176.6-177.5
		// before the change, and 168-172 under Qt 6.10.3, when it already failed
		// 160), so it has its own bound: 184 MiB, 10% above its peak, the rule
		// BPR-0893 used for the single 224 MiB bound these two replace.
		// [[BPR-1296]]: by 2026-10-06 the constructed window held 9.6 MiB more
		// live heap than at 5a26d6b1b, from glyph icons rendered in advance at
		// every mode, size and scale; the course's footprint median rose from
		// 158 to 176.5 MiB and missed 184 in 1 of 20 runs. The icons paint on
		// request now (median 157.0, highest 166.4); the limit is unchanged.
		// [[BPR-0967]]: other platforms' values are in the platform table above.
		std::uint64_t SampledFootprintGrowthLimitBytes   = PlatformFootprintGrowthLimitBytes;
		std::uint64_t CacheByteFootprintGrowthLimitBytes = PlatformCacheByteFootprintGrowthLimitBytes;
		// Cleanup bounds the live heap (bytes malloc has in use), not the
		// footprint. The allocator keeps freed pages dirty, so after cleanup
		// the footprint stays at 99-107 MiB while the live heap is back to
		// 66-68 MiB with the application open and 13-14 MiB once it is
		// destroyed. Since [[BPR-1032]] the loaded comparison holds about
		// 34 MiB live rather than 100, so a cleanup that kept it would leave
		// 101-110 MiB. 84 MiB sits halfway between 67.6 and 101.1; 96 would
		// still catch such a cleanup, but by only 5 MiB.
		std::uint64_t                  CleanupLiveHeapGrowthLimitBytes   = 84U * 1024U * 1024U;
		std::uint64_t                  DestroyedLiveHeapGrowthLimitBytes = 32U * 1024U * 1024U;
		bool                           WindowCloseObserved               = false;
		bool                           PruneReleasedDiff = false, PruneReleasedIndex = false, PruneClearedProjection = false;
		bool                           DeleteReleasedDiff = false, DeleteReleasedIndex = false, DestroyReleasedDiff = false, DestroyReleasedIndex = false;
		bool                           UnavailableCourse = false, UnavailableReasonVerified = false, BinaryStatusVerified = false;
		std::uint32_t                  UnavailableSegmentLimit = 0U;
		std::size_t                    RecoverySegments        = 0U;
		bool                           ApproximateCourse = false, ApproximateAlignmentVerified = false;
		std::size_t                    ApproximateGroups = 0U, ApproximateSegments = 0U;
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
		std::string                    TimingScope = "BuildConfiguration identifies the measured configuration; Debug is diagnostic only. 5 ms event-loop timer plus the final interval through readiness. MaxPulseIntervalMs includes synchronous-ready work; P95 uses delivered pulses only. Outstanding jobs are sampled admitted work through decision delivery (including deferred ingress/local terminals), not continuous worker or CPU occupancy. Active-work keys record queue-to-event-filter delivery and queue-to-next-tick verified selection; no OS input-delivery clock is observed. The separate held-provider course observes continuous fixture entry, stop-token callback and provider exit on a steady clock; it models waiting work, not CPU occupancy. Active close measures the public close through whole-application quiescence, not stop-token acknowledgement. Measurement input gaps span consecutive 1 ms timer callbacks after native batching has started; initial publication delay remains in the phase timer. Wheel batch is measured after classification settles. CompositionStartupMs excludes process and QApplication launch. Responsiveness phase readiness includes synchronous QWidget rendering but excludes image encoding/writes; this is not display-compositor presentation or OS input-delivery latency. InputBudgetFailures retain unmet 100 ms phases; successful measurement does not accept them. The independent 1 ms all-zone live-heap sampler reports a lower-bound peak and its largest sample interval/cost; it cannot exclude unsampled peaks. No cross-platform latency budget.";
	};

	void require(bool aCondition, const char* aReason)
	{
		if (!aCondition) {
			throw std::runtime_error{ aReason };
		}
	}

	/// A numeric limit gates only where BudgetsEnforced; elsewhere its miss is
	/// named once in UnenforcedLimitMisses and the course continues.
	void require_limit(benchmark_report& aReport, bool aWithin, const std::string& aReason)
	{
		if (aWithin) {
			return;
		}
		if (aReport.Extended.BudgetsEnforced) {
			throw std::runtime_error{ aReason };
		}
		if (std::ranges::find(aReport.Extended.UnenforcedLimitMisses, aReason) == aReport.Extended.UnenforcedLimitMisses.end()) {
			aReport.Extended.UnenforcedLimitMisses.push_back(aReason);
		}
	}

	[[nodiscard]] auto budget_met(const latency_budget& aBudget) -> bool
	{
		return aBudget.Passed || !aBudget.Enforced;
	}

	/// The 100 ms input target and every enforced completion limit. Where no
	/// limit is justified, InputBudgetFailures and the misses stay reported;
	/// a platform's KnownInputMisses are reported, not enforced.
	[[nodiscard]] auto input_target_met(const benchmark_report& aReport) -> bool
	{
		return !aReport.Extended.BudgetsEnforced
			|| std::ranges::all_of(aReport.InputBudgetFailures, [](const std::string& aPhase) { return known_input_miss(aPhase); });
	}

	[[nodiscard]] auto budgets_met(const benchmark_report& aReport) -> bool
	{
		return input_target_met(aReport) && std::ranges::all_of(aReport.CompletionBudgets, budget_met);
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
		if (!aReport.ResourceCourse && !aReport.CacheEntryCourse && !aReport.CacheByteCourse) {
			return;
		}
		resource_sample sample;
		sample.Phase                      = aPhase;
		const auto memory                 = native_process_memory_sample();
		sample.ResidentBytes              = memory.ResidentBytes;
		sample.PhysicalFootprintBytes     = memory.FootprintBytes;
		sample.LifetimePeakFootprintBytes = memory.LifetimePeakFootprintBytes;
		sample.LiveHeapBytes              = native_live_heap_bytes();
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
		const auto growth = [](std::uint64_t aValue, std::uint64_t aBaseline) -> std::uint64_t {
			return aValue > aBaseline ? aValue - aBaseline : 0U;
		};
		if (!aReport.Resources.empty()) {
			const auto& baseline        = aReport.Resources.front();
			sample.FootprintGrowthBytes = growth(sample.PhysicalFootprintBytes, baseline.PhysicalFootprintBytes);
			// The native peak is process-lifetime, so this is a conservative
			// upper bound on growth since the before_application sample. It
			// includes any earlier startup peak above that sampled baseline.
			sample.LifetimePeakGrowthBytes = growth(sample.LifetimePeakFootprintBytes, baseline.PhysicalFootprintBytes);
			sample.LiveHeapGrowthBytes     = growth(sample.LiveHeapBytes, baseline.LiveHeapBytes);
		}
		std::cerr << "resource-sample:" << *glz::write_json(sample) << '\n';
		if (!aReport.Resources.empty()) {
			const auto footprintLimit = aReport.CacheByteCourse ? aReport.CacheByteFootprintGrowthLimitBytes
																: aReport.SampledFootprintGrowthLimitBytes;
			require_limit(aReport, aReport.Extended.AllocationProfile || sample.FootprintGrowthBytes <= footprintLimit,
						  "sampled fixture footprint growth exceeded its platform budget");
			require_limit(aReport, aReport.Extended.AllocationProfile || sample.LifetimePeakGrowthBytes <= footprintLimit,
						  "fixture lifetime peak footprint growth exceeded its platform budget");
			if (sample.Phase == "after_prune" || sample.Phase == "after_cache_delete"
				|| sample.Phase == "shutdown_drained" || sample.Phase == "application_destroyed") {
				const auto cleanupLimit = sample.Phase == "application_destroyed"
					? aReport.DestroyedLiveHeapGrowthLimitBytes
					: aReport.CleanupLiveHeapGrowthLimitBytes;
				require_limit(aReport, aReport.Extended.AllocationProfile || sample.LiveHeapGrowthBytes <= cleanupLimit,
							  std::string{ "cleanup left live heap growth above its platform budget at " } + aPhase);
				require(sample.DiffCacheBytes == 0U && sample.DiffCacheEntries == 0U
							&& sample.DiffJobs == 0U && sample.EpochJobs == 0U,
						"completed cleanup retains derived cache or outstanding work");
			}
		}
		aReport.Resources.push_back(sample);
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
		// The course clock: a run budget for the whole course, not a
		// performance limit. [[BPR-0967]] scales it in Windows Debug.
		const qint64 courseClockMs = qint64{ aReport.Extended.CourseClockScale }
			* (aReport.CacheByteCourse ? 840'000 : aReport.HistoryCourse ? 540'000
				   : aReport.CacheEntryCourse                            ? 180'000
																		 : 90'000);
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
			ready                       = aReady();
			const auto epochStatus      = aReport.CacheByteCourse ? aApplication.status().EpochStatus : core::epoch_status::inactive;
			const auto byteCourseFailed = aReport.CacheByteCourse
				&& (epochStatus == core::epoch_status::failed || epochStatus == core::epoch_status::unavailable || epochStatus == core::epoch_status::cancelled);
			if (ready || byteCourseFailed || aBudget.elapsed() >= courseClockMs) {
				loop.quit();
			}
		});
		pulse.start();
		aAction();
		phase.ActionMs = static_cast<double>(elapsed.nsecsElapsed()) / 1'000'000.0;
		ready          = aReady();
		if (!ready && aBudget.elapsed() < courseClockMs) {
			readiness.start();
			loop.exec();
		}
		const auto finished = elapsed.nsecsElapsed();
		phase.ElapsedMs     = static_cast<double>(finished) / 1'000'000.0;
		// Readiness can finish synchronously or stall after the last timer. Both
		// intervals delay real input even though no subsequent pulse is delivered.
		phase.FinalIntervalMs    = static_cast<double>(finished - previous) / 1'000'000.0;
		phase.MaxPulseIntervalMs = phase.FinalIntervalMs;
		phase.Pulses             = intervals.size();
		if (!intervals.empty()) {
			std::ranges::sort(intervals);
			phase.MaxPulseIntervalMs = std::max(phase.MaxPulseIntervalMs, intervals.back());
			phase.P95PulseIntervalMs = intervals[(intervals.size() - 1U) * 95U / 100U];
		}
		aReport.Phases.push_back(std::move(phase));
		std::cerr << "large-review phase: " << aName << " ms=" << aReport.Phases.back().ElapsedMs << std::endl;
		if (!ready) {
			const auto evidence = access::evidence(aApplication);
			const auto work     = access::diff_work_evidence(aApplication);
			const auto status   = aApplication.status();
			// [[BPR-0967]]. Which wait ended unready, and whether the course
			// clock or the epoch's own terminal ended it.
			aReport.Extended.WaitExpiry = std::string{ aName } + ": course_clock_ms=" + std::to_string(courseClockMs)
				+ " elapsed_ms=" + std::to_string(aBudget.elapsed()) + " epoch_status=" + std::to_string(static_cast<int>(status.EpochStatus))
				+ " clock_expired=" + (aBudget.elapsed() >= courseClockMs ? "1" : "0");
			std::cerr << "large-review wait: " << aName << " status=" << aApplication.window().github_status_text()
					  << " account_reads=" << evidence.AccountReadCount << " replay_failure=" << evidence.RecordingFailure
					  << " diff=" << aApplication.window().comparison_surface()->status_text().toStdString()
					  << " jobs=" << work.OutstandingJobs << " workbench=" << work.WorkbenchOutstandingRequests << " pending=" << work.Pending << " quiet=" << work.Quiescent
					  << " epoch_status=" << static_cast<int>(status.EpochStatus)
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

	void verify_initial_read_identities(app::github_application&                         aApplication,
										const application::github_pull_request_snapshot& aSnapshot)
	{
		const auto* cards = aApplication.window().conversation_panel()->cards();
		auto        byId  = std::map<std::string, const qt::thread_card*>{ };
		for (const auto& card : cards->cards()) {
			require(byId.emplace(card.Id, &card).second, "duplicate presented conversation identity");
		}
		for (const auto& thread : aSnapshot.ReviewThreads) {
			const auto found = byId.find("review-thread:" + std::string{ thread.Thread.value() });
			require(found != byId.end(), "accepted thread missing from initial read projection");
			auto unread = std::size_t{ 0U };
			for (const auto& comment : thread.Comments) {
				if (comment.Deleted) {
					continue;
				}
				const auto& posts = found->second->Posts;
				const auto  post  = std::ranges::find(posts, "review-comment:" + std::string{ comment.Comment.value() }, &qt::card_post::Id);
				const auto  mine  = comment.Author && comment.Author->Login == aSnapshot.Viewer.Login;
				require(post != posts.end() && post->Unread == !mine, "read state joined to wrong exact post identity");
				unread += mine ? 0U : 1U;
			}
			require(found->second->UnreadCount == unread, "thread unread count lost exact post state");
		}
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
		verify_initial_read_identities(aApplication, *snapshot);
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
		std::cerr << "large-review-provider-evidence: consumed=" << evidence.RecordingConsumed
				  << " capability=" << evidence.CapabilityProbeConsumed << " bootstrap=" << evidence.BootstrapTransportSelected
				  << " account=" << evidence.AccountTransportSelected << " failure=" << evidence.RecordingFailure
				  << " reads=" << evidence.AccountReadCount << " smart=" << evidence.SmartAcquireCount << ':'
				  << evidence.SmartDiscoveryCount << ':' << evidence.SmartLsRefsCount << ':' << evidence.SmartFetchCount << '\n';
		require(evidence.RecordingConsumed && evidence.CapabilityProbeConsumed && evidence.BootstrapTransportSelected && evidence.AccountTransportSelected
					&& evidence.RecordingFailure == -1 && evidence.AccountReadCount == static_cast<int>(1U + 29U * std::max(std::size_t{ 1U }, aReport.EpochsObserved)) && evidence.SmartAcquireCount == 1
					&& evidence.SmartDiscoveryCount == 1 && evidence.SmartLsRefsCount == 1 && evidence.SmartFetchCount == 1,
				"selected provider/Git path or replay exhaustion");
		aReport.AccountRequests = evidence.AccountReadCount;
	}

	void verify_large(qt::diff_workbench& aSurface, const fixture_manifest& aManifest)
	{
		require(selected(aSurface, QStringLiteral("large/comparison.cpp")), "large comparison identity");
		const auto* unified = aSurface.findChild<QPlainTextEdit*>(QStringLiteral("diffUnifiedSource"));
		require(aSurface.one_pane_enabled() || (unified && unified->document()->isEmpty()),
				"two-pane comparison constructed a hidden third source document");
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

	void verify_generated(qt::diff_workbench& aSurface, const fixture_manifest& aManifest)
	{
		require(selected(aSurface, QStringLiteral("generated/table.inc")), "generated comparison identity");
		const auto& result = *aSurface.current_result();
		const auto  found  = std::ranges::find(aManifest.files, std::string{ "generated/table.inc" }, &fixture_file::path);
		require(found != aManifest.files.end() && result.Alignment.Content == core::diff_content_state::text
					&& result.Alignment.Key.LeftBlob == core::parse_git_blob_oid(found->base.oid)
					&& result.Alignment.Key.RightBlob == core::parse_git_blob_oid(found->head.oid)
					&& result.LeftDocument && result.RightDocument
					&& result.LeftDocument->Lines.size() == 8192U && result.RightDocument->Lines.size() == 8192U,
				"generated content lost exact blobs or lines");
		require(aSurface.left_pane()->document()->firstBlock().text() == QStringLiteral("// Generated fixture version 0; do not edit.")
					&& aSurface.right_pane()->document()->firstBlock().text() == QStringLiteral("// Generated fixture version 1; do not edit."),
				"generated header mismatch");
		for (int line = 1; line < 8192; ++line) {
			const auto expected = QStringLiteral("ENTRY(%1, %2)").arg(line, 5, 10, QChar{ '0' }).arg(line % 17);
			require(aSurface.left_pane()->document()->findBlockByNumber(line).text() == expected
						&& aSurface.right_pane()->document()->findBlockByNumber(line).text() == expected,
					"generated source line mismatch");
		}
	}

	/// [[BPR-0858]]. The review opens on Overview, which shows the pull request
	/// in place of the comparison, so a course that reads the comparison's own
	/// status label first enters Review through the strip's public modality. It
	/// states Review and the comparison it needs through the step every course
	/// shares ([[BPR-0863]]), which names the modality and what it withholds
	/// when it cannot.
	void enter_review(app::github_application& aApplication, benchmark_report& aReport, QElapsedTimer& aBudget)
	{
		auto       step    = app::detail::course_modality_step{ .Modality = qt::review_modality::review, .Needs = { .Comparison = true } };
		auto       outcome = app::detail::course_modality_result::pending;
		const auto advance = [&] { outcome = access::enter_course_modality(aApplication, step); };
		const auto settled = [&] {
			if (outcome == app::detail::course_modality_result::pending) {
				advance();
			}
			return outcome == app::detail::course_modality_result::failed
				|| (outcome == app::detail::course_modality_result::entered && quiet(aApplication));
		};
		require(measure_wait(aApplication, aBudget, aReport, "review_modality", advance, settled)
					&& outcome == app::detail::course_modality_result::entered,
				"Review modality did not present the comparison");
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
						&& evidence.AccountReadCount == static_cast<int>(1 + 29 * ordinal),
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
				require_limit(aReport, phase.MaxPulseIntervalMs <= aReport.HistoryInputIntervalLimitMs && phase.ElapsedMs <= aReport.HistoryRefreshLimitMs,
							  "epoch refresh exceeded its measured platform budget");
			} else if (phase.Name.starts_with("epoch_source_selection_")) {
				require_limit(aReport, phase.MaxPulseIntervalMs <= aReport.HistoryInputIntervalLimitMs && phase.ElapsedMs <= aReport.HistorySourceSelectionLimitMs,
							  "epoch source selection exceeded its measured platform budget");
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

	void report_tab_comparison(app::github_application& aApplication, const char* aPhase)
	{
		const auto evidence = access::tab_comparison_evidence(aApplication);
		const auto pair     = [](const char* aName, const std::optional<core::comparison_pair>& aPair) {
			std::cerr << ' ' << aName << '=';
			if (aPair) {
				std::cerr << aPair->Left.Identity.Epoch.value() << ':' << aPair->Right.Identity.Epoch.value();
			} else {
				std::cerr << "none";
			}
		};
		std::cerr << "first-tab-comparison:" << aPhase;
		pair("accepted", evidence.Accepted);
		pair("window", evidence.Window);
		pair("tab", evidence.ActiveTab);
		if (const auto result = aApplication.window().comparison_surface()->current_result()) {
			std::cerr << " displayed=" << result->Alignment.Key.LeftAnchor.Epoch.value() << ':' << result->Alignment.Key.RightAnchor.Epoch.value();
		}
		std::cerr << '\n';
	}

	/// \brief First-file selection must keep an epoch gesture made with no tab.
	void run_first_tab(app::github_application& aApplication, const fixture_manifest& aManifest,
					   benchmark_report& aReport, QElapsedTimer& aBudget, const char* aImage)
	{
		auto&      window   = aApplication.window();
		auto&      controls = *window.epoch_selectors();
		auto&      surface  = *window.comparison_surface();
		const auto settled  = [&] { return quiet(aApplication) && access::diff_work_evidence(aApplication).EpochOutstandingJobs == 0U; };
		require(measure_wait(aApplication, aBudget, aReport, "first_tab_second_head", [&] { window.github_refresh_action()->trigger(); }, [&] { return controls.right_oid_editor()->text().toStdString() == aManifest.heads[1] && settled(); }), "second head refresh timed out");
		aReport.EpochsObserved = 2U;
		enter_review(aApplication, aReport, aBudget);
		require(measure_wait(aApplication, aBudget, aReport, "first_tab_history", [&] { controls.refresh_action()->trigger(); }, [&] { return controls.history_model()->epoch_count() == 2U && settled(); }), "first-tab history timed out");
		require(measure_wait(aApplication, aBudget, aReport, "first_tab_base", [&] { select_epoch(controls, 1, core::comparison_side::left); }, settled), "first-tab base selection timed out");
		require(measure_wait(aApplication, aBudget, aReport, "first_tab_head", [&] { select_epoch(controls, 1, core::comparison_side::right); }, settled), "first-tab head selection timed out");
		const auto expected = access::tab_comparison_evidence(aApplication);
		report_tab_comparison(aApplication, "before-file");
		require(expected.Accepted && !expected.ActiveTab && expected.Accepted->Left.Identity.Epoch.value() == 1 && expected.Accepted->Right.Identity.Epoch.value() == 1, "epoch gesture accepted exact first pair before any tab opens");
		require(measure_wait(aApplication, aBudget, aReport, "first_tab_file", [&] { select_file(window, QStringLiteral("large/comparison.cpp")); }, [&] { return selected(surface, QStringLiteral("large/comparison.cpp")) && settled(); }), "first-tab file selection timed out");
		report_tab_comparison(aApplication, "after-file");
		const auto  actual    = access::tab_comparison_evidence(aApplication);
		const auto& key       = surface.current_result()->Alignment.Key;
		aReport.FirstTabExact = actual.Accepted == expected.Accepted && actual.Window == expected.Accepted && actual.ActiveTab == expected.Accepted
			&& key.LeftAnchor == expected.Accepted->Left.Identity && key.RightAnchor == expected.Accepted->Right.Identity;
		require(aReport.FirstTabExact, "first file replaced the accepted comparison with a stale window pair");
		verify_large(surface, aManifest);
		aReport.ExactLargeSource = true;
		aReport.ExactFinalSource = true;
		aReport.WindowWidth      = window.width();
		aReport.WindowHeight     = window.height();
		aReport.DevicePixelRatio = window.devicePixelRatioF();
		aReport.Rendered         = window.grab().save(QString::fromLocal8Bit(aImage));
		require(aReport.Rendered, "first-tab public window render failed");
	}

	void run_cache_bytes(app::github_application& aApplication, const fixture_manifest& aManifest,
						 benchmark_report& aReport, QElapsedTimer& aBudget, const char* aImage)
	{
		auto&      window   = aApplication.window();
		auto&      surface  = *window.comparison_surface();
		auto&      controls = *window.epoch_selectors();
		const auto limits   = application::diff_host_limits{ };
		const auto settled  = [&] {
			return quiet(aApplication) && access::diff_work_evidence(aApplication).EpochOutstandingJobs == 0U;
		};
		require(limits.CacheBytes == 64U * 1024U * 1024U && limits.CacheEntries == 64U
					&& access::diff_work_evidence(aApplication).CacheEntries == 0U,
				"byte-pressure course requires unchanged default empty cache");
		// Observe stored heads through the provider first, without selecting a file.
		// The subsequent public epoch gestures create distinct immutable keys.
		for (std::size_t index = 1U; index < aManifest.heads.size(); ++index) {
			const auto phase = "byte_epoch_refresh_" + std::to_string(index + 1U);
			require(window.github_refresh_action()->isEnabled(), "byte-pressure public refresh unavailable");
			require(measure_wait(aApplication, aBudget, aReport, phase.c_str(), [&] { window.github_refresh_action()->trigger(); }, [&] { return controls.right_oid_editor()->text().toStdString() == aManifest.heads[index] && settled(); }), "byte-pressure stored head refresh timed out");
			aReport.EpochsObserved = index + 1U;
			aReport.Head           = aManifest.heads[index];
			const auto evidence    = access::evidence(aApplication);
			require(evidence.RecordingConsumed && evidence.RecordingFailure == -1 && evidence.AccountReadCount == static_cast<int>(1U + 29U * (index + 1U)),
					"byte-pressure refresh did not exhaust exact recorded snapshot");
		}
		require(measure_wait(aApplication, aBudget, aReport, "byte_load_history", [&] { controls.refresh_action()->trigger(); }, [&] { return controls.history_model()->epoch_count() == aManifest.heads.size() && settled(); }), "byte-pressure retained history unavailable");
		require(measure_wait(aApplication, aBudget, aReport, "byte_base_selection", [&] { select_epoch(controls, 1, core::comparison_side::left); }, settled), "byte-pressure base selection timed out");
		require(measure_wait(aApplication, aBudget, aReport, "byte_first_head_selection", [&] { select_epoch(controls, 1, core::comparison_side::right); }, settled), "byte-pressure first head selection timed out");
		report_tab_comparison(aApplication, "cache-before-file");
		const auto ready = [&](std::int64_t aOrdinal) {
			return selected(surface, QStringLiteral("large/comparison.cpp"))
				&& surface.current_result()->Alignment.Key.LeftAnchor.Epoch.value() == 1
				&& surface.current_result()->Alignment.Key.RightAnchor.Epoch.value() == aOrdinal && settled();
		};
		const auto verify = [&](std::int64_t aOrdinal) {
			verify_large(surface, aManifest);
			const auto& key = surface.current_result()->Alignment.Key;
			require(key.LeftAnchor.Authority == key.Authority.Scope && key.RightAnchor.Authority == key.Authority.Scope
						&& key.LeftAnchor.Incarnation == key.RightAnchor.Incarnation
						&& key.LeftAnchor.Kind == core::comparison_anchor_kind::epoch_base
						&& key.RightAnchor.Kind == core::comparison_anchor_kind::epoch_head
						&& controls.left_oid_editor()->text().toStdString() == aManifest.base
						&& controls.right_oid_editor()->text().toStdString() == aManifest.heads.at(static_cast<std::size_t>(aOrdinal - 1)),
					"byte-pressure comparison anchor/OID mismatch");
		};
		QTimer inputDuringMeasurement;
		inputDuringMeasurement.setInterval(1);
		QElapsedTimer       inputClock;
		std::vector<double> inputIntervals;
		const auto          beforeMeasurement = surface.statistics().MeasurementBatches;
		QObject::connect(&inputDuringMeasurement, &QTimer::timeout, &window, [&] {
			if (!surface.geometry_pending() || !selected(surface, QStringLiteral("large/comparison.cpp"))) {
				inputClock.invalidate();
				return;
			}
			if (inputClock.isValid()) {
				inputIntervals.push_back(static_cast<double>(inputClock.nsecsElapsed()) / 1'000'000.0);
			}
			inputClock.start();
			const auto  frames = surface.statistics().FrameApplications;
			QWheelEvent event{ QPointF{ 20, 20 }, QPointF{ 20, 20 }, QPoint{ 0, -1 }, QPoint{ }, Qt::NoButton, Qt::NoModifier, Qt::ScrollUpdate, false };
			QApplication::sendEvent(surface.right_pane()->viewport(), &event);
			require(event.isAccepted() && frames == surface.statistics().FrameApplications,
					"byte-pressure input applied incomplete geometry");
			++aReport.InputsDuringMeasurement;
		});
		inputDuringMeasurement.start();
		std::weak_ptr<const core::computed_file_diff>  firstDiff;
		std::weak_ptr<const core::diff_semantic_index> firstIndex;
		std::optional<core::file_comparison_key>       firstKey;
		for (std::int64_t ordinal = 1; ordinal <= static_cast<std::int64_t>(aManifest.heads.size()); ++ordinal) {
			const auto phase = "cache_byte_selection_" + std::to_string(ordinal);
			require(measure_wait(aApplication, aBudget, aReport, phase.c_str(), [&] {
				if (ordinal == 1) {
					select_file(window, QStringLiteral("large/comparison.cpp"));
				} else {
					select_epoch(controls, ordinal, core::comparison_side::right);
				} }, [&] { return ready(ordinal); }), "byte-pressure large selection timed out");
			verify(ordinal);
			if (ordinal == 1) {
				report_tab_comparison(aApplication, "cache-after-file");
			}
			const auto work = access::diff_work_evidence(aApplication);
			if (ordinal == 1) {
				require(work.CacheEntries == 1U && work.CacheBytes > 0U, "first large key was not cached");
				aReport.ByteEntryBytes = work.CacheBytes;
				firstDiff              = surface.current_result();
				firstIndex             = surface.current_semantic_index();
				firstKey               = surface.current_result()->Alignment.Key;
			}
			++aReport.ByteSelections;
			// Every head has identical large-file bytes and geometry. Measure one
			// admitted key's accounting rather than duplicate the host formula.
			aReport.ByteAdmittedBytes += aReport.ByteEntryBytes;
			aReport.BytePeakCacheEntries = std::max(aReport.BytePeakCacheEntries, work.CacheEntries);
			aReport.BytePeakCacheBytes   = std::max(aReport.BytePeakCacheBytes, work.CacheBytes);
			require(work.CacheBytes == work.CacheEntries * aReport.ByteEntryBytes
						&& work.CacheEntries == std::min(aReport.ByteSelections, static_cast<std::size_t>(limits.CacheBytes / aReport.ByteEntryBytes))
						&& work.CacheBytes <= limits.CacheBytes && aReport.ByteSelections < limits.CacheEntries,
					"large keys failed default byte accounting or reached entry pressure");
			if (aReport.ByteAdmittedBytes <= limits.CacheBytes) {
				require(!firstDiff.expired() && !firstIndex.expired(), "large key evicted before byte pressure");
			}
			sample_resources(aReport, &aApplication, phase.c_str());
			if (firstDiff.expired() && firstIndex.expired()) {
				break;
			}
		}
		aReport.ByteEvictedDiff  = firstDiff.expired();
		aReport.ByteEvictedIndex = firstIndex.expired();
		require(aReport.ByteAdmittedBytes > limits.CacheBytes && aReport.ByteEvictedDiff && aReport.ByteEvictedIndex,
				"default byte pressure did not release oldest raw diff and semantic index");
		// Hold B selected through A's completion drain. Observe native source
		// changes synchronously so a transient stale A publication cannot hide
		// behind a later correct B (or the final return to A).
		const auto foundB = std::ranges::find(aManifest.files, source_path(100).toStdString(), &fixture_file::path);
		require(foundB != aManifest.files.end(), "superseding file is absent from manifest");
		auto expectedB      = *firstKey;
		expectedB.File      = core::file_identity_of(source_path(100).toStdString());
		expectedB.LeftBlob  = core::parse_git_blob_oid(foundB->base.oid);
		expectedB.RightBlob = core::parse_git_blob_oid(foundB->head.oid);
		bool       holdingB = false, stalePublication = false;
		const auto observeB = [&](QTextDocument* aDocument, const QString& aFirstLine) {
			if (!holdingB) {
				return;
			}
			++aReport.ByteHeldSourceObservations;
			stalePublication = stalePublication || (surface.current_result() && surface.current_result()->Alignment.Key != expectedB)
				|| (!aDocument->isEmpty() && aDocument->firstBlock().text() != aFirstLine);
		};
		QObject    observationLifetime;
		const auto leftObservation  = QObject::connect(surface.left_pane()->document(), &QTextDocument::contentsChanged, &observationLifetime, [&] {
			observeB(surface.left_pane()->document(), QStringLiteral("int fixture_0100 = 0;"));
		});
		const auto rightObservation = QObject::connect(surface.right_pane()->document(), &QTextDocument::contentsChanged, &observationLifetime, [&] {
			observeB(surface.right_pane()->document(), QStringLiteral("int fixture_0100 = 1;"));
		});
		QTimer     supersede;
		supersede.setInterval(1);
		QObject::connect(&supersede, &QTimer::timeout, &window, [&] {
			if (!holdingB && access::diff_work_evidence(aApplication).OutstandingJobs != 0U) {
				holdingB = true;
				++aReport.ByteSupersessionsWithWork;
				select_file(window, source_path(100));
				supersede.stop();
			}
		});
		const auto drainedB = measure_wait(aApplication, aBudget, aReport, "cache_byte_supersession_held_b", [&] {
			select_epoch(controls, 1, core::comparison_side::right);
			supersede.start(); }, [&] { return stalePublication || (holdingB && selected(surface, source_path(100)) && settled()); });
		supersede.stop();
		QObject::disconnect(leftObservation);
		QObject::disconnect(rightObservation);
		require(drainedB && !stalePublication && aReport.ByteHeldSourceObservations != 0U
					&& selected(surface, source_path(100)) && surface.current_result()->Alignment.Key == expectedB && settled(),
				"stale comparison published while superseding file B remained selected");
		for (int line = 0; line < 8; ++line) {
			const auto common = QStringLiteral("// synthetic source 0100 line %1").arg(line + 1);
			require(surface.left_pane()->document()->findBlockByNumber(line).text() == (line == 0 ? QStringLiteral("int fixture_0100 = 0;") : common)
						&& surface.right_pane()->document()->findBlockByNumber(line).text() == (line == 0 ? QStringLiteral("int fixture_0100 = 1;") : common),
					"superseding file B lost exact source after all prior work drained");
		}
		aReport.ByteSupersessionHeldExact = true;
		require(measure_wait(aApplication, aBudget, aReport, "cache_byte_final_revisit", [&] { select_file(window, QStringLiteral("large/comparison.cpp")); }, [&] { return ready(1); }), "evicted key final revisit timed out");
		inputDuringMeasurement.stop();
		if (!inputIntervals.empty()) {
			std::ranges::sort(inputIntervals);
			aReport.MaxMeasurementInputIntervalMs = inputIntervals.back();
			aReport.P95MeasurementInputIntervalMs = inputIntervals[(inputIntervals.size() - 1U) * 95U / 100U];
		}
		aReport.LargeMeasurementBatches      = surface.statistics().MeasurementBatches - beforeMeasurement;
		aReport.MaximumMeasurementBatchLines = surface.statistics().MaximumMeasurementBatchLines;
		require(aReport.InputsDuringMeasurement != 0U && aReport.LargeMeasurementBatches > 1U && aReport.MaximumMeasurementBatchLines <= 512U,
				"byte-pressure native measurement did not yield bounded batches to input");
		verify(1);
		aReport.ByteRevisitExact = surface.current_result()->Alignment.Key == *firstKey
			&& aReport.ByteSupersessionsWithWork != 0U && aReport.ByteSupersessionHeldExact;
		require(aReport.ByteRevisitExact, "evicted key was not exactly rematerialized after live supersession");
		const auto work = access::diff_work_evidence(aApplication);
		require(work.FixtureDiffHelperInvocations == 0U && work.CacheBytes <= limits.CacheBytes && work.CacheEntries < limits.CacheEntries,
				"byte-pressure revisit bypassed composition or exceeded default budget");
		aReport.DiffCacheEntries     = work.CacheEntries;
		aReport.DiffCacheBytes       = work.CacheBytes;
		aReport.BytePeakCacheEntries = std::max(aReport.BytePeakCacheEntries, work.CacheEntries);
		aReport.BytePeakCacheBytes   = std::max(aReport.BytePeakCacheBytes, work.CacheBytes);
		aReport.AccountRequests      = access::evidence(aApplication).AccountReadCount;
		aReport.ExactLargeSource     = true;
		aReport.ExactFinalSource     = true;
		aReport.WindowWidth          = window.width();
		aReport.WindowHeight         = window.height();
		aReport.DevicePixelRatio     = window.devicePixelRatioF();
		aReport.Rendered             = window.grab().save(QString::fromLocal8Bit(aImage));
		require(aReport.Rendered, "byte-pressure public window render failed");
		for (const auto& phase : aReport.Phases) {
			require(phase.PeakDiffJobs <= limits.OutstandingJobs
						&& phase.PeakEpochJobs <= application::epoch_host_limits{ }.OutstandingJobs
						&& phase.PeakWorkbenchRequests <= application::workbench_host_limits{ }.OutstandingRequests,
					"byte-pressure course exceeded configured job admission bounds");
		}
	}

	void verify_responsiveness_budgets(benchmark_report& aReport)
	{
		if (!aReport.ResponsivenessCourse) {
			return;
		}
		// BPR-0967: optimized M4 Pro / 48 GiB / Qt 6.11.2 observations
		// revalidate the existing completion limits and justify the new startup
		// and metadata limits. Debug measurements are diagnostic only. All
		// observed phase gaps must also meet the unchanged 100 ms target.
		//
		// [[BPR-1247]]. The limits BPR-0967 introduced from optimized
		// measurements alone ("no Debug budget justification") gate optimized
		// builds. A Debug build still measures and reports them, marked not
		// enforced; the historical Debug limits gate every build.
		//
		// [[BPR-0967]]. No limit is enforced where none is justified for the
		// platform; misses are still reported.
		const auto optimized = aReport.BuildConfiguration != "Debug";
		const auto record    = [&](const std::string& aName, double aMeasured, double aLimit, bool aEnforced = true) {
			aReport.CompletionBudgets.push_back({ aName, aMeasured, aLimit, aMeasured <= aLimit, aEnforced && aReport.Extended.BudgetsEnforced });
		};
		const auto optimized_record = [&](const std::string& aName, double aMeasured, double aLimit) {
			record(aName, aMeasured, aLimit, optimized);
		};
		const auto phase = [&](const char* aName, double aLimit, bool aEnforced = true) {
			const auto found = std::ranges::find(aReport.Phases, aName, &phase_measurement::Name);
			require(found != aReport.Phases.end(), "required responsiveness phase missing");
			record(aName, found->ElapsedMs, aLimit, aEnforced);
		};
		record("composition_startup_render", aReport.CompositionStartupMs, 1'000.0);
		optimized_record("qapplication_startup", aReport.Extended.QApplicationStartupMs, 50.0);
		optimized_record("main_to_home_render", aReport.Extended.MainToHomeRenderMs, 1'000.0);
		optimized_record("provisional_metadata_render", aReport.Extended.ProvisionalMetadataRenderMs, 100.0);
		optimized_record("accepted_metadata_render", aReport.Extended.AcceptedMetadataRenderMs, 250.0);
		optimized_record("main_to_first_source_render", aReport.Extended.MainToFirstSourceRenderMs, 15'000.0);
		phase("paged_review_and_git_load", 12'000.0);
		phase("large_file_selection", 2'500.0);
		phase("binary_fallback_render", 1'250.0);
		phase("generated_source_render", 2'500.0, optimized);
		phase("large_source_revisit_render", 1'000.0);
		// These complete intervals meet the original target after the source,
		// Overview and repeated conversation-label measurement repairs.
		for (const auto* name : { "paged_review_and_git_load", "large_file_selection", "large_source_revisit_render", "generated_source_render" }) {
			const auto found = std::ranges::find(aReport.Phases, name, &phase_measurement::Name);
			record(std::string{ name } + "_input_gap", found->MaxPulseIntervalMs, aReport.InputIntervalLimitMs, !known_input_miss(name));
		}
		phase("rapid_selection_supersession", 10'000.0);
		record("supersession_final_drain", aReport.SupersessionDrainMs, 1'250.0);
		// [[BPR-1295]]. read_modality became review_modality when Read merged
		// into Review; drawers_hide and drawers_show are Command-B both ways.
		for (const auto* name : { "review_modality", "files_drawer_open", "files_drawer_close", "conversations_drawer_open", "conversations_drawer_close", "drawers_hide", "drawers_show", "appearance_settings_open", "theme_light", "theme_dark" }) {
			phase(name, aReport.InputIntervalLimitMs, !known_input_miss(name));
		}
		// [[BPR-1293]]. Conversations: in, a search, its clearing, out and a
		// pick, each one input; the pick's file opening as a file selection.
		for (const auto* name : { "conversations_enter", "conversations_search", "conversations_search_clear", "conversations_leave", "conversations_pick" }) {
			phase(name, aReport.InputIntervalLimitMs, !known_input_miss(name));
		}
		phase("conversations_pick_open", 2'500.0);
		{
			const auto found = std::ranges::find(aReport.Phases, "conversations_pick_open", &phase_measurement::Name);
			record("conversations_pick_open_input_gap", found->MaxPulseIntervalMs, aReport.InputIntervalLimitMs, !known_input_miss("conversations_pick_open"));
		}
		// [[BPR-1089]]. The Overview's count switch and sort over 515 files and
		// 1,000 threads, each one input against the unchanged 100 ms target.
		for (const auto* name : { "overview_counts_all", "overview_counts_open", "overview_sort_by_comments", "overview_file_order" }) {
			phase(name, aReport.InputIntervalLimitMs, !known_input_miss(name));
		}
		require(!aReport.ChangeActionMs.empty() && aReport.InputsDuringMeasurement != 0U && aReport.WheelEvents == 1000U,
				"responsiveness budgets lack real navigation and input observations");
		record("next_change_dispatch", std::ranges::max(aReport.ChangeActionMs), aReport.InputIntervalLimitMs);
		record("wheel_dispatch", aReport.MaxWheelEventMs, aReport.InputIntervalLimitMs);
		record("measurement_input_gap", aReport.MaxMeasurementInputIntervalMs, aReport.InputIntervalLimitMs);
		require(budgets_met(aReport), "Mac fixture responsiveness budget exceeded");
	}

	void verify_active_work_budgets(benchmark_report& aReport)
	{
		if (!aReport.ActiveWorkCourse) {
			return;
		}
		require(aReport.ActiveInputs.size() == 31U
					&& std::ranges::all_of(aReport.ActiveInputs, [](const auto& aInput) { return aInput.Delivered && aInput.Applied; })
					&& std::ranges::any_of(aReport.ActiveInputs, [](const auto& aInput) { return aInput.DiffAtPost != 0U && aInput.DiffAtDelivery != 0U && aInput.PendingAtDelivery; })
					&& aReport.ActiveCloseDiffJobs != 0U && aReport.ActiveFinalHeldExact && aReport.ActiveCloseDrained,
				"active-work course lacks delivered input or public close during observed pending work");
		const auto record = [&](const std::string& aName, double aMeasured, double aLimit, bool aEnforced = true) {
			aReport.CompletionBudgets.push_back({ aName, aMeasured, aLimit, aMeasured <= aLimit, aEnforced && aReport.Extended.BudgetsEnforced });
		};
		const auto phase = [&](const char* aName) -> const phase_measurement& {
			const auto found = std::ranges::find(aReport.Phases, aName, &phase_measurement::Name);
			require(found != aReport.Phases.end(), "active-work phase missing");
			return *found;
		};
		const auto& inputs = phase("active_queued_selection_supersession");
		record("active_selection_input_gap", inputs.MaxPulseIntervalMs, aReport.InputIntervalLimitMs, !known_input_miss("active_queued_selection_supersession"));
		record("active_close_input_gap", phase("active_window_close").MaxPulseIntervalMs, aReport.InputIntervalLimitMs, !known_input_miss("active_window_close"));
		record("active_queued_key_delivery", std::ranges::max(aReport.ActiveInputs, { }, &active_input_measurement::DeliveredAfterMs).DeliveredAfterMs, aReport.InputIntervalLimitMs);
		record("active_queued_key_applied", std::ranges::max(aReport.ActiveInputs, { }, &active_input_measurement::AppliedAfterMs).AppliedAfterMs, aReport.InputIntervalLimitMs);
		record("active_close_dispatch", aReport.ActiveCloseDispatchMs, aReport.InputIntervalLimitMs);
		// Release revalidates the existing fixture completion and 100 ms
		// close-to-quiescence limits; these are not universal I/O bounds.
		record("active_selection_completion", inputs.ElapsedMs, 10'000.0);
		record("active_supersession_final_drain", aReport.SupersessionDrainMs, 1'250.0);
		record("active_close_quiescence", aReport.ActiveCloseDrainMs, PlatformActiveCloseQuiescenceLimitMs);
		require(budgets_met(aReport), "Mac active-work input or drain budget exceeded");
	}

	void run_responsiveness_interactions(app::github_application& aApplication, const fixture_manifest& aManifest,
										 benchmark_report& aReport, QElapsedTimer& aBudget, const char* aImage)
	{
		auto&      window         = aApplication.window();
		auto&      surface        = *window.comparison_surface();
		const auto comparison     = surface.current_result()->Alignment.Key;
		const auto classification = aApplication.status().ClassificationSubmissions;
		QPixmap    frame;
		const auto rendered = [&] {
			frame = window.grab();
			require(surface.isVisible() && !frame.isNull(), "interaction frame failed");
			++aReport.InteractionFrames;
			return true;
		};
		const auto save_frame = [&](const char* aPhase) {
			require(frame.save(QString::fromLocal8Bit(aImage) + QLatin1Char('.') + QString::fromLatin1(aPhase) + QStringLiteral(".png")), "interaction frame write failed");
		};
		const auto toggle = [&](QAction* aAction, qt::workbench_drawer_panel* aDrawer, const char* aPhase) {
			const auto expanded = !aAction->isChecked();
			require(aAction->isEnabled(), "public drawer action unavailable");
			require(measure_wait(aApplication, aBudget, aReport, aPhase, [&] { aAction->trigger(); }, [&] { return aAction->isChecked() == expanded
																												&& aDrawer->state() == (expanded ? core::workbench_drawer_state::expanded : core::workbench_drawer_state::collapsed)
																												&& (!expanded || (aDrawer->isVisible() && aDrawer->width() >= 100))
																												&& quiet(aApplication) && rendered(); }), "drawer did not reach its rendered state");
			save_frame(aPhase);
		};
		// [[BPR-1295]]. The window's first entry to Review applies its preset.
		require(window.files_drawer_toggle_action()->isChecked() && window.conversations_drawer_toggle_action()->isChecked(),
				"Review starts with Files and Conversations open");
		toggle(window.files_drawer_toggle_action(), window.files_drawer(), "files_drawer_close");
		toggle(window.files_drawer_toggle_action(), window.files_drawer(), "files_drawer_open");
		toggle(window.conversations_drawer_toggle_action(), window.conversations_drawer(), "conversations_drawer_close");
		toggle(window.conversations_drawer_toggle_action(), window.conversations_drawer(), "conversations_drawer_open");
		// [[BPR-1295]]. Hide Drawers closes both open drawers and shows them
		// again at their widths, each within the same input budget.
		auto* const hideDrawers = window.toggle_drawers_action();
		require(hideDrawers->isEnabled() && !hideDrawers->isChecked(), "Hide Drawers is offered with drawers open");
		const auto widths = std::pair{ window.files_drawer()->width(), window.conversations_drawer()->width() };
		require(measure_wait(aApplication, aBudget, aReport, "drawers_hide", [&] { hideDrawers->trigger(); }, [&] { return hideDrawers->isChecked()
																														&& !window.files_drawer()->isVisible() && !window.conversations_drawer()->isVisible()
																														&& quiet(aApplication) && rendered(); }), "Hide Drawers did not reach its rendered state");
		save_frame("drawers_hide");
		require(measure_wait(aApplication, aBudget, aReport, "drawers_show", [&] { hideDrawers->trigger(); }, [&] { return !hideDrawers->isChecked()
																														&& window.files_drawer()->isVisible() && window.conversations_drawer()->isVisible()
																														&& std::pair{ window.files_drawer()->width(), window.conversations_drawer()->width() } == widths
																														&& quiet(aApplication) && rendered(); }), "Show Drawers did not restore both drawers at their widths");
		save_frame("drawers_show");
		require(measure_wait(aApplication, aBudget, aReport, "appearance_settings_open", [&] { window.open_settings(qt::settings_page::appearance); }, [&] { return window.settings() && window.settings()->isVisible() && window.appearance_settings()->isVisible()
																																								 && !window.settings()->grab().isNull(); }), "Appearance settings did not render");
		for (const auto mode : { core::appearance_mode::light, core::appearance_mode::dark }) {
			const auto* phase  = mode == core::appearance_mode::light ? "theme_light" : "theme_dark";
			const auto  writes = aApplication.status().AppearanceDurableWrites;
			require(measure_wait(aApplication, aBudget, aReport, phase, [&] { window.appearance_settings()->choose_mode(mode); }, [&] { return aApplication.status().ProfileMode == mode && aApplication.status().AppearanceDurableWrites > writes
																																			&& quiet(aApplication) && rendered(); }), "theme did not persist and render");
			save_frame(phase);
		}
		window.settings()->close();
		require(surface.current_result()->Alignment.Key == comparison
					&& aApplication.status().ClassificationSubmissions == classification,
				"drawer/theme interaction changed comparison identity or reclassified source");
		verify_large(surface, aManifest);
		// [[BPR-1293]]. Conversations over 1,000 threads in 512 files: Command-J
		// in and out, a search and its clearing, each one input against the
		// 100 ms target; then a pick, whose input is measured alone and whose
		// file load and thread opening are measured as a file selection is.
		{
			auto* const finder  = window.finder();
			auto* const enter   = window.show_conversations_action();
			const auto  rows    = [finder] { return std::ranges::count_if(finder->shown_keys(), [](const std::string& aKey) { return aKey.starts_with("row:"); }); };
			const auto  widthsC = std::pair{ window.files_drawer()->width(), window.conversations_drawer()->width() };
			require(finder != nullptr && enter != nullptr && enter->isEnabled(), "Conversations is offered");
			// The page, not the code, is what Conversations renders.
			const auto page = [&] {
				frame = window.grab();
				require(finder->isVisible() && !frame.isNull(), "Conversations frame failed");
				++aReport.InteractionFrames;
				return true;
			};
			require(measure_wait(aApplication, aBudget, aReport, "conversations_enter", [&] { enter->trigger(); }, [&] { return window.current_modality() == qt::review_modality::conversations && finder->isVisible()
																															 && rows() == 1000 && !window.files_drawer()->isVisible() && page(); }), "Conversations did not render every conversation");
			save_frame("conversations_enter");
			require(measure_wait(aApplication, aBudget, aReport, "conversations_search", [&] { finder->search_field()->setText(QStringLiteral("observation 0042")); }, [&] { return rows() == 1 && page(); }), "the search did not find its one conversation");
			require(measure_wait(aApplication, aBudget, aReport, "conversations_search_clear", [&] { finder->search_field()->clear(); }, [&] { return rows() == 1000 && page(); }), "clearing the search did not show every conversation");
			require(measure_wait(aApplication, aBudget, aReport, "conversations_leave", [&] { enter->trigger(); }, [&] { return window.current_modality() == qt::review_modality::review && !finder->isVisible()
																															 && window.files_drawer()->isVisible() && window.conversations_drawer()->isVisible()
																															 && std::pair{ window.files_drawer()->width(), window.conversations_drawer()->width() } == widthsC
																															 && quiet(aApplication) && rendered(); }), "Command-J did not return to Review as it was");
			enter->trigger();
			finder->search_field()->setText(QStringLiteral("observation 0042"));
			QApplication::processEvents();
			require(rows() == 1, "the pick's conversation is the one shown");
			const auto thread = std::string{ "review-thread:SYNTHETIC_THREAD_SYNTHETIC_COMMENT_900042" };
			auto*      cards  = window.conversation_panel()->cards();
			require(measure_wait(aApplication, aBudget, aReport, "conversations_pick", [&] {
				auto ret = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
				QApplication::sendEvent(finder->search_field(), &ret); }, [&] { return window.current_modality() == qt::review_modality::review && !finder->isVisible() && window.conversations_drawer()->isVisible() && rendered(); }), "a pick did not enter Review");
			require(measure_wait(aApplication, aBudget, aReport, "conversations_pick_open", [] { }, [&] { return selected(surface, source_path(42)) && cards->open_thread_id() == thread
																											  && cards->picked_post() == "review-comment:SYNTHETIC_COMMENT_900042" && cards->open_card() != nullptr
																											  && quiet(aApplication) && rendered(); }), "the picked conversation did not open in place on its file");
			save_frame("conversations_pick_open");
			finder->search_field()->clear();
		}
		require(measure_wait(aApplication, aBudget, aReport, "generated_source_render", [&] { select_file(window, QStringLiteral("generated/table.inc")); }, [&] { return selected(surface, QStringLiteral("generated/table.inc")) && quiet(aApplication) && rendered(); }), "generated source timed out");
		verify_generated(surface, aManifest);
		aReport.Extended.ExactGeneratedSource = true;
		save_frame("generated_source");
		require(measure_wait(aApplication, aBudget, aReport, "binary_fallback_render", [&] { select_file(window, QStringLiteral("assets/data.bin")); }, [&] { return selected(surface, QStringLiteral("assets/data.bin")) && quiet(aApplication) && rendered(); }), "binary fallback timed out");
		save_frame("binary_fallback");
		require(surface.current_result()->Alignment.Content == core::diff_content_state::binary
					&& surface.status_text() == QStringLiteral("Binary file comparison")
					&& surface.left_pane()->toPlainText().isEmpty() && surface.right_pane()->toPlainText().isEmpty(),
				"binary fallback retained stale source or hid its reason");
		aReport.BinaryStatusVerified = true;
		require(measure_wait(aApplication, aBudget, aReport, "large_source_revisit_render", [&] { select_file(window, QStringLiteral("large/comparison.cpp")); }, [&] { return selected(surface, QStringLiteral("large/comparison.cpp")) && quiet(aApplication) && rendered(); }), "source recovery timed out");
		save_frame("large_source_revisit");
		verify_large(surface, aManifest);
		require(surface.current_result()->Alignment.Key == comparison, "fallback recovery changed exact anchors");
		// [[BPR-1212]]. The keyboard line cursor on the large source: one
		// Down, one Shift+Down, and one Down from a line out of view, which
		// reveals the next through the linked scroll.
		{
			auto* const right = surface.right_pane();
			const auto  key   = [right](const Qt::Key aKey, const Qt::KeyboardModifiers aModifiers) {
				auto press = QKeyEvent{ QEvent::KeyPress, aKey, aModifiers };
				QApplication::sendEvent(right, &press);
			};
			const auto line = [&surface]() -> std::uint32_t {
				return surface.selection() ? surface.selection()->PreferredLine.Line : 0xFFFF'FFFFU;
			};
			right->setFocus(Qt::OtherFocusReason);
			require(surface.reveal_conversation_line(core::diff_side::right, 40U), "the large source takes a selected line");
			require(measure_wait(aApplication, aBudget, aReport, "keyboard_line_down", [&] { key(Qt::Key_Down, Qt::NoModifier); }, [&] { return line() == 41U && quiet(aApplication) && rendered(); }), "Down did not move the selected line");
			require(measure_wait(aApplication, aBudget, aReport, "keyboard_line_extend", [&] { key(Qt::Key_Down, Qt::ShiftModifier); }, [&] {
				const auto range = surface.line_draft_range();
				return line() == 42U && range && range->first.Line == 41U && range->second.Line == 42U && quiet(aApplication) && rendered(); }), "Shift+Down did not extend the selection");
			const auto far = surface.current_result()->Alignment.RightLineCount / 2U;
			require(surface.select_conversation_line(core::diff_side::right, far) && !surface.source_line_visible(core::diff_side::right, far + 1U), "a line out of view is selected");
			require(measure_wait(aApplication, aBudget, aReport, "keyboard_line_reveal", [&] { key(Qt::Key_Down, Qt::NoModifier); }, [&] { return line() == far + 1U && surface.source_line_visible(core::diff_side::right, far + 1U) && quiet(aApplication) && rendered(); }), "Down from a line out of view did not reveal the next");
			save_frame("keyboard_line_reveal");
		}
	}

	void close_active_work(app::github_application& aApplication, const fixture_manifest& aManifest,
						   const core::file_comparison_key& aComparison, benchmark_report& aReport, QElapsedTimer& aBudget)
	{
		auto&      window = aApplication.window();
		QObject    lifetime;
		const auto target = core::review_epoch_id{ aComparison.LeftAnchor.Authority, aComparison.LeftAnchor.Incarnation, aComparison.LeftAnchor.Epoch };
		QObject::connect(qApp, &QGuiApplication::lastWindowClosed, &lifetime, [&] { aReport.WindowCloseObserved = true; aApplication.begin_shutdown(); });
		QElapsedTimer close, admission;
		bool          admissionFailed = false;
		QTimer        closeWhenPending;
		closeWhenPending.setInterval(1);
		QObject::connect(&closeWhenPending, &QTimer::timeout, &lifetime, [&] {
			if (admission.elapsed() >= 2'000) {
				admissionFailed = true;
				closeWhenPending.stop();
				return;
			}
			const auto work = access::diff_work_evidence(aApplication);
			if (work.OutstandingJobs == 0U || !work.Pending) {
				return;
			}
			aReport.ActiveCloseDiffJobs  = work.OutstandingJobs;
			aReport.ActiveCloseEpochJobs = work.EpochOutstandingJobs;
			closeWhenPending.stop();
			close.start();
			window.close();
			aReport.ActiveCloseDispatchMs = static_cast<double>(close.nsecsElapsed()) / 1'000'000.0;
		});
		require(measure_wait(aApplication, aBudget, aReport, "active_window_close", [&] {
			admission.start(); select_file(window, source_path(200)); closeWhenPending.start(); }, [&] { return admissionFailed || aApplication.status().ShutdownComplete; }), "active close failed to drain");
		require(!admissionFailed && close.isValid(), "active close did not observe outstanding cold-file work");
		aReport.ActiveCloseDrainMs = static_cast<double>(close.nsecsElapsed()) / 1'000'000.0;
		const auto closed          = access::diff_work_evidence(aApplication);
		aReport.ActiveCloseDrained = aReport.WindowCloseObserved && !window.isVisible() && closed.Quiescent && !closed.Pending
			&& closed.OutstandingJobs == 0U && closed.WorkbenchOutstandingRequests == 0U && closed.EpochOutstandingJobs == 0U
			&& closed.CacheEntries == 0U && closed.CacheBytes == 0U && !aApplication.status().ShutdownCleanupFailed;
		require(aReport.ActiveCloseDrained, "active close retained jobs, cache, window or cleanup failure");
		const auto retained                = access::maintenance_evidence(aApplication, target, aComparison);
		aReport.ClosePreservedEpochStorage = retained.PruneIntentDispatchCount == 0U && retained.DeleteIntentDispatchCount == 0U
			&& retained.RepositoryPresent && retained.TargetBaseRefPresent && retained.TargetHeadRefPresent
			&& retained.TargetBaseRefOid == aManifest.base && retained.TargetBaseCatalogOid == aManifest.base
			&& retained.TargetHeadRefOid == aManifest.heads.back() && retained.TargetHeadCatalogOid == retained.TargetHeadRefOid;
		require(aReport.ClosePreservedEpochStorage, "active close changed exact retained Git/catalog anchors");
	}

	void run_active_work(app::github_application& aApplication, const fixture_manifest& aManifest,
						 benchmark_report& aReport, QElapsedTimer& aBudget, const char* aImage)
	{
		auto& window  = aApplication.window();
		auto& surface = *window.comparison_surface();
		require(measure_wait(aApplication, aBudget, aReport, "active_files_drawer", [&] {
			if (!window.files_drawer_toggle_action()->isChecked()) { window.files_drawer_toggle_action()->trigger(); } }, [&] { return window.files_tree()->isVisible() && quiet(aApplication); }), "active-work drawer unavailable");
		auto* view  = window.files_tree();
		auto* order = window.findChild<QComboBox*>(QStringLiteral("filesOrderSelector"));
		require(order && order->isVisible(), "visible file-order selector unavailable");
		order->setCurrentIndex(order->findData(static_cast<int>(qt::files_order::flat)));
		view->setFocus();
		const auto finalPath = source_path(31);
		const auto found     = std::ranges::find(aManifest.files, finalPath.toStdString(), &fixture_file::path);
		require(found != aManifest.files.end(), "active-work final file absent from manifest");
		auto expected           = surface.current_result()->Alignment.Key;
		expected.File           = core::file_identity_of(finalPath.toStdString());
		expected.LeftBlob       = core::parse_git_blob_oid(found->base.oid);
		expected.RightBlob      = core::parse_git_blob_oid(found->head.oid);
		bool       holdingFinal = false, stale = false;
		QObject    lifetime;
		const auto observe = [&](QTextDocument* aDocument, const QString& aFirst) {
			if (holdingFinal) {
				++aReport.ActiveSourceObservations;
				stale = stale || (surface.current_result() && surface.current_result()->Alignment.Key != expected)
					|| (!aDocument->isEmpty() && aDocument->firstBlock().text() != aFirst);
			}
		};
		QObject::connect(surface.left_pane()->document(), &QTextDocument::contentsChanged, &lifetime, [&] { observe(surface.left_pane()->document(), QStringLiteral("int fixture_0031 = 0;")); });
		QObject::connect(surface.right_pane()->document(), &QTextDocument::contentsChanged, &lifetime, [&] { observe(surface.right_pane()->document(), QStringLiteral("int fixture_0031 = 1;")); });
		active_input_observer observer{ aApplication, aReport.ActiveInputs, [&](const auto& aInput) {
										   // Prior work may finish between posting and delivery. The actual
										   // final gesture, not its queue insertion, starts stale-source checks.
										   holdingFinal = aInput.Path == finalPath.toStdString();
									   } };
		view->installEventFilter(&observer);
		QElapsedTimer course, drain;
		QTimer        selections;
		selections.setTimerType(Qt::PreciseTimer);
		selections.setInterval(1);
		int         next          = 0;
		bool        awaitingInput = false;
		QString     requested;
		const char* inputFailure = nullptr;
		const auto  failInput    = [&](const char* aReason) { inputFailure = aReason; selections.stop(); };
		QObject::connect(&selections, &QTimer::timeout, &lifetime, [&] {
			if (awaitingInput) {
				if (observer.Clock.elapsed() >= 1'000) {
					failInput("queued active-work input was not applied within watchdog");
					return;
				}
				auto& input = aReport.ActiveInputs.back();
				if (!input.Delivered) {
					return;
				}
				input.Applied        = view->currentIndex().data(qt::files_tree_model::path_role).toString() == requested;
				input.AppliedAfterMs = static_cast<double>(observer.Clock.nsecsElapsed()) / 1'000'000.0;
				if (!input.Applied) {
					failInput("queued key did not select the exact next file");
					return;
				}
				awaitingInput = false;
				++next;
			}
			if (next == 32) {
				selections.stop();
				return;
			}
			const auto work = access::diff_work_evidence(aApplication);
			if (!requested.isEmpty() && work.OutstandingJobs == 0U && !selected(surface, requested)) {
				return;
			}
			if (work.OutstandingJobs > application::diff_host_limits{ }.OutstandingJobs) {
				failInput("active-work diff admission bound exceeded");
				return;
			}
			aReport.SelectionsWithOutstandingDiff += work.OutstandingJobs != 0U ? 1U : 0U;
			requested = source_path(next);
			++aReport.RapidSelections;
			if (next == 0) {
				select_file(window, requested);
				++next;
				return;
			}
			if (view->model()->index(view->currentIndex().row() + 1, 0).data(qt::files_tree_model::path_role).toString() != requested) {
				failInput("fixture drawer order cannot deliver exact Down key target");
				return;
			}
			aReport.ActiveInputs.push_back({ .Path = requested.toStdString(), .PostedAtMs = static_cast<double>(course.nsecsElapsed()) / 1'000'000.0, .DiffAtPost = work.OutstandingJobs });
			observer.Clock.start();
			observer.Armed = true;
			awaitingInput  = true;
			if (next == 31) {
				drain.start();
			}
			QApplication::postEvent(view, new QKeyEvent{ QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier });
			QApplication::postEvent(view, new QKeyEvent{ QEvent::KeyRelease, Qt::Key_Down, Qt::NoModifier });
		});
		require(measure_wait(aApplication, aBudget, aReport, "active_queued_selection_supersession", [&] { course.start(); selections.start(); }, [&] { return inputFailure || stale || (next == 32 && selected(surface, finalPath) && quiet(aApplication) && access::diff_work_evidence(aApplication).EpochOutstandingJobs == 0U); }), "active-work supersession timed out");
		selections.stop();
		require(inputFailure == nullptr, inputFailure);
		aReport.SupersessionDrainMs = static_cast<double>(drain.nsecsElapsed()) / 1'000'000.0;
		require(!stale && aReport.ActiveSourceObservations != 0U && surface.current_result()->Alignment.Key == expected,
				"stale source or authority adopted while final keyboard target was held");
		for (int line = 0; line < 8; ++line) {
			const auto common = QStringLiteral("// synthetic source 0031 line %1").arg(line + 1);
			require(surface.left_pane()->document()->findBlockByNumber(line).text() == (line == 0 ? QStringLiteral("int fixture_0031 = 0;") : common)
						&& surface.right_pane()->document()->findBlockByNumber(line).text() == (line == 0 ? QStringLiteral("int fixture_0031 = 1;") : common),
					"active-work final source differs from fixture");
		}
		require(surface.left_pane()->blockCount() == 9 && surface.right_pane()->blockCount() == 9
					&& surface.left_pane()->document()->lastBlock().text().isEmpty() && surface.right_pane()->document()->lastBlock().text().isEmpty(),
				"active-work final source length differs from fixture");
		aReport.ActiveFinalHeldExact = true;
		aReport.ExactFinalSource     = true;
		holdingFinal                 = false;
		require(window.grab().save(QString::fromLocal8Bit(aImage) + QStringLiteral(".active-final.png")), "active-work final window capture failed");
		const auto cache         = access::diff_work_evidence(aApplication);
		aReport.DiffCacheEntries = cache.CacheEntries;
		aReport.DiffCacheBytes   = cache.CacheBytes;
		require(cache.FixtureDiffHelperInvocations == 0U && cache.CacheEntries <= application::diff_host_limits{ }.CacheEntries
					&& cache.CacheBytes <= application::diff_host_limits{ }.CacheBytes,
				"active-work bypassed the composed path or exceeded cache bounds");
		close_active_work(aApplication, aManifest, expected, aReport, aBudget);
	}

	void run_provider_cancellation(app::github_application& aApplication, held_provider_read& aRead,
								   benchmark_report& aReport, QElapsedTimer& aBudget)
	{
		auto& window = aApplication.window();
		require(measure_wait(aApplication, aBudget, aReport, "held_provider_started", [&] { window.home_open_action()->trigger(); }, [&] { return aRead.EnteredAt.load() != 0 && aRead.ExitedAt.load() == 0 && window.showing_overview()
																																			   && window.overview_page()->summary_label()->text().contains(QStringLiteral("Provisional metadata")); }), "provider gate or provisional metadata unavailable");
		window.home_action()->trigger();
		auto* field = window.home_open_bar();
		require(field->isVisible() && field->isEnabled(), "held provider lacks visible input surface");
		field->clear();
		field->setFocus();
		for (int index = 1; index <= 20; ++index) {
			require(aRead.ExitedAt.load() == 0 && aRead.CancelledAt.load() == 0, "provider ended before queued input");
			require(measure_wait(aApplication, aBudget, aReport, "held_provider_input", [&] {
				QApplication::postEvent(field, new QKeyEvent{ QEvent::KeyPress, Qt::Key_X, Qt::NoModifier, QStringLiteral("x") });
				QApplication::postEvent(field, new QKeyEvent{ QEvent::KeyRelease, Qt::Key_X, Qt::NoModifier }); }, [&] { return field->text() == QString(index, QChar{ 'x' }); }), "queued provider-work input did not apply");
			require(aRead.ExitedAt.load() == 0 && aRead.CancelledAt.load() == 0, "provider ended during queued input");
			++aReport.Extended.ProviderHeldInputs;
		}
		QObject closeLifetime;
		QObject::connect(qApp, &QGuiApplication::lastWindowClosed, &closeLifetime, [&] {
			aReport.WindowCloseObserved = true;
			aApplication.begin_shutdown();
		});
		const auto closingAt = held_provider_read::now_ns();
		require(measure_wait(aApplication, aBudget, aReport, "held_provider_close", [&] { window.close(); }, [&] { return aRead.ExitedAt.load() != 0 && aApplication.status().ShutdownComplete; }), "public close did not cancel held provider and drain application");
		aReport.Extended.ProviderStopped     = aRead.Stopped.load();
		aReport.Extended.ProviderHeldMs      = static_cast<double>(closingAt - aRead.EnteredAt.load()) / 1'000'000.0;
		aReport.Extended.ProviderCancelAckMs = static_cast<double>(aRead.CancelledAt.load() - closingAt) / 1'000'000.0;
		aReport.Extended.ProviderExitMs      = static_cast<double>(aRead.ExitedAt.load() - closingAt) / 1'000'000.0;
		require(aReport.WindowCloseObserved && aReport.Extended.ProviderStopped && aRead.CancelledAt.load() >= closingAt && !window.isVisible()
					&& aApplication.status().ShutdownComplete && !aApplication.status().ShutdownCleanupFailed
					&& access::diff_work_evidence(aApplication).CacheEntries == 0U
					&& !access::large_review_snapshot(aApplication).has_value(),
				"cancelled provider published state or retained resources");
		const auto record = [&](const std::string& aName, double aMeasured, bool aEnforced = true) {
			aReport.CompletionBudgets.push_back({ aName, aMeasured, 100.0, aMeasured <= 100.0, aEnforced && aReport.Extended.BudgetsEnforced });
		};
		for (const auto& phase : aReport.Phases) {
			record(phase.Name + "_input_gap", phase.MaxPulseIntervalMs, !known_input_miss(phase.Name));
		}
		record("provider_cancel_ack", aReport.Extended.ProviderCancelAckMs);
		record("provider_cancel_exit", aReport.Extended.ProviderExitMs);
		require(std::ranges::all_of(aReport.CompletionBudgets, budget_met), "held provider input/cancellation budget exceeded");
		aReport.ShutdownComplete = true;
	}

	auto run_benchmark(QApplication& qtApplication, int argc, char** argv,
					   const QElapsedTimer& aMainClock, double aQApplicationStartupMs) -> int
	{
		if (argc == 5 && std::string_view{ argv[4] } == "--history-session-race") {
			if (!app::detail::verify_large_review_history_session_interleaving()) {
				return EXIT_FAILURE;
			}
			std::cout << "LARGE_REVIEW_HISTORY_SESSION_RACE_OK\n";
			return EXIT_SUCCESS;
		}
		if (argc != 4 && (argc != 5 || (std::string_view{ argv[4] } != "--unavailable" && std::string_view{ argv[4] } != "--approximate" && std::string_view{ argv[4] } != "--resources" && std::string_view{ argv[4] } != "--history" && std::string_view{ argv[4] } != "--loaded-close" && std::string_view{ argv[4] } != "--cache-entries" && std::string_view{ argv[4] } != "--cache-bytes" && std::string_view{ argv[4] } != "--first-tab" && std::string_view{ argv[4] } != "--timing-control" && std::string_view{ argv[4] } != "--responsiveness" && std::string_view{ argv[4] } != "--active-work" && std::string_view{ argv[4] } != "--pathological" && std::string_view{ argv[4] } != "--provider-cancel" && std::string_view{ argv[4] } != "--allocation-profile"))) {
			return EXIT_FAILURE;
		}
		// [[BPR-1265]]. The byte-pressure course runs only in an optimized
		// build, on every platform. A Debug build is diagnostic only (see
		// TimingScope), and the course takes about ten minutes there even with
		// [[BPR-1297]]'s bounded heap sampler (before it, the sampler's lock
		// made a Windows Debug selection take 135-372 s). The row reports
		// Skipped, with this reason, not a pass.
		if (!OptimizedBuild && argc == 5 && std::string_view{ argv[4] } == "--cache-bytes") {
			std::cout << "LARGE_REVIEW_CACHE_BYTES_SKIPPED: the byte-pressure course runs only in an optimized build; "
					  << "a " << BPR_BENCH_BUILD_CONFIGURATION << " build is diagnostic only\n";
			return cache_bytes_debug_skip_exit_code;
		}
		QTemporaryDir data;
		if (!data.isValid()) {
			return EXIT_FAILURE;
		}
		// [[BPR-0856]], [[BPR-0874]]. The application creates its agent workspace
		// neighbourhood beside its data root, and only under a parent this user
		// owns and nobody else can write. Under any other parent workspace recovery
		// refuses, and shutdown reports a cleanup failure. The macOS runner's
		// `TMPDIR` is such a parent; Linux's network sandbox sets none, and shared
		// `/tmp` is not one. On Windows the bare temporary folder carries %TEMP%'s
		// inherited entries, and the profile registry refuses it as a root
		// (registry_unavailable). So on every platform the data root sits in the
		// private temporary directory rather than being it, as in [[BPR-0812]]'s
		// agent courses, and the neighbourhood is removed with it. [[BPR-1050]]:
		// the shared helper makes that arrangement, canonical and named natively.
		const auto privateRoot = e2e::private_data_root(data);
		if (!privateRoot) {
			std::cerr << "large-review-benchmark-failed: private data root parent\n";
			return EXIT_FAILURE;
		}
		const auto& dataRoot = *privateRoot;

		benchmark_report report;
		report.Extended.AllocationProfile                                  = argc == 5 && std::string_view{ argv[4] } == "--allocation-profile";
		report.Extended.QApplicationStartupMs                              = aQApplicationStartupMs;
		report.Extended.ProviderCancelCourse                               = argc == 5 && std::string_view{ argv[4] } == "--provider-cancel";
		auto heldRead                                                      = std::make_shared<held_provider_read>();
		report.ActiveWorkCourse                                            = argc == 5 && std::string_view{ argv[4] } == "--active-work";
		report.ResponsivenessCourse                                        = argc == 5 && std::string_view{ argv[4] } == "--responsiveness";
		report.Extended.PathologicalCourse                                 = argc == 5 && std::string_view{ argv[4] } == "--pathological";
		report.UnavailableCourse                                           = report.Extended.PathologicalCourse || (argc == 5 && std::string_view{ argv[4] } == "--unavailable");
		report.ApproximateCourse                                           = argc == 5 && std::string_view{ argv[4] } == "--approximate";
		report.LoadedCloseCourse                                           = report.Extended.AllocationProfile || (argc == 5 && std::string_view{ argv[4] } == "--loaded-close");
		report.ResourceCourse                                              = report.LoadedCloseCourse || (argc == 5 && std::string_view{ argv[4] } == "--resources");
		report.HistoryCourse                                               = argc == 5 && std::string_view{ argv[4] } == "--history";
		report.CacheByteCourse                                             = argc == 5 && std::string_view{ argv[4] } == "--cache-bytes";
		report.FirstTabCourse                                              = argc == 5 && std::string_view{ argv[4] } == "--first-tab";
		report.CacheEntryCourse                                            = argc == 5 && std::string_view{ argv[4] } == "--cache-entries";
		const bool                                     observeResources    = report.ResourceCourse || report.CacheEntryCourse || report.CacheByteCourse;
		const bool                                     observeLoadedOwners = report.LoadedCloseCourse || report.CacheEntryCourse || report.CacheByteCourse;
		std::weak_ptr<const core::computed_file_diff>  resourceDiff;
		std::weak_ptr<const core::diff_semantic_index> resourceIndex;
		QPointer<QWidget>                              resourceWindow;
		QPointer<QTextDocument>                        resourceLeftDocument, resourceRightDocument;
		std::optional<core::review_epoch_id>           closeTarget;
		std::optional<core::file_comparison_key>       closeComparison;
		std::unique_ptr<app::github_application>       application;
		live_heap_sampler                              heapSampler{ observeResources };
		try {
			std::ifstream     input{ std::filesystem::path{ BPR_LARGE_REVIEW_FIXTURE_ROOT } / "manifest.json" };
			const std::string bytes{ std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{ } };
			fixture_manifest  manifest;
			require(!glz::read<glz::opts{ .error_on_unknown_keys = false }>(manifest, bytes) && manifest.schema == "bpr.large-review-fixture.v1" && manifest.heads.size() == 32U, "stored manifest");
			report.Base                       = manifest.base;
			report.Head                       = (report.HistoryCourse || report.CacheByteCourse || report.FirstTabCourse) ? manifest.heads.front() : manifest.heads.back();
			report.PackBytes                  = manifest.pack_bytes;
			report.Platform                   = QSysInfo::prettyProductName().toStdString();
			report.Architecture               = QSysInfo::currentCpuArchitecture().toStdString();
			report.QtVersion                  = qVersion();
			report.Style                      = qtApplication.style()->objectName().toStdString();
			report.BuildConfiguration         = BPR_BENCH_BUILD_CONFIGURATION;
			report.LogicalProcessors          = QThread::idealThreadCount();
			auto fixtureOptions               = app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = dataRoot, .GitExecutable = std::filesystem::path{ argv[1] } };
			fixtureOptions.LargeReviewHistory = report.HistoryCourse || report.CacheByteCourse || report.FirstTabCourse;
			if (report.Extended.ProviderCancelCourse) {
				fixtureOptions.BeforeFilesPage = [heldRead](std::stop_token aStop) { heldRead->wait(aStop); };
			}
			if (report.Extended.PathologicalCourse) {
				fixtureOptions.DiffComputeLimits.MaximumLinesPerSide = 4096U;
			} else if (report.UnavailableCourse) {
				// [[BPR-0858]]. Since BPR-0719 a work limit only degrades the alignment;
				// a segment overflow is what still refuses a file. The stored 20,000-line
				// file aligns in 40 segments and the one-line recovery file in 2, so a
				// limit of 2 refuses the first and still computes the second. A limit of
				// 1 refuses both.
				report.UnavailableSegmentLimit                   = 2U;
				fixtureOptions.DiffComputeLimits.MaximumSegments = report.UnavailableSegmentLimit;
			}
			if (report.ApproximateCourse) {
				// [[BPR-0719]] [[BPR-0858]]. Without its ordered line index the
				// stored 20,000-line file exceeds the production matrix ceiling, and
				// the host degrades the alignment instead of refusing the file.
				fixtureOptions.DiffComputeLimits.MaximumPrimaryIndexWork = 0U;
			}
			sample_resources(report, nullptr, "before_application");
			QElapsedTimer compositionStartup;
			compositionStartup.start();
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
			if (application == nullptr) {
				// [[BPR-0874]]. Which profile refusal the startup saw, so a failed
				// start names its cause rather than needing a temporary print.
				std::cerr << "LARGE_REVIEW_STARTUP_REFUSAL:" << app::profile_startup_refusal_name() << '\n';
				// [[BPR-0882]]. And which composition arm refused, with what it caught.
				std::cerr << "LARGE_REVIEW_STARTUP_SOURCE:" << app::startup_failure_source() << '\n';
			}
			require(application != nullptr, "application startup succeeds");
			if (argc == 5 && std::string_view{ argv[4] } == "--timing-control") {
				QElapsedTimer controlBudget;
				controlBudget.start();
				require(measure_wait(*application, controlBudget, report, "synchronous_timing_control", [] { QThread::msleep(120); }, [] { return true; }), "synchronous timing control failed");
				bool terminal = false;
				require(measure_wait(*application, controlBudget, report, "terminal_timing_control", [&] { QTimer::singleShot(30, application->window().comparison_surface(), [&] { terminal = true; }); }, [&] {
					if (terminal) { QThread::msleep(120); }
					return terminal; }), "terminal timing control failed");
				const auto synchronous = report.Phases[0];
				const auto trailing    = report.Phases[1];
				require(measure_wait(*application, controlBudget, report, "shutdown", [&] { application->begin_shutdown(); }, [&] { return application->status().ShutdownComplete; }), "timing control shutdown failed");
				std::cerr << "timing-controls:" << *glz::write_json(report.Phases) << '\n';
				require(synchronous.Pulses == 0U && synchronous.MaxPulseIntervalMs >= 100.0,
						"synchronous-ready phase hid its 120 ms event-loop stall");
				require(trailing.Pulses != 0U && trailing.MaxPulseIntervalMs >= 100.0,
						"terminal phase hid its final 120 ms event-loop stall");
#if BPR_LARGE_REVIEW_NATIVE_MEMORY
				live_heap_sampler heapControl{ true };
				{
					const auto     storage = std::make_unique<char[]>(32U * 1024U * 1024U);
					volatile char* touched = storage.get();
					for (std::size_t offset = 0; offset < 32U * 1024U * 1024U; offset += 4096U) {
						touched[offset] = 1;
					}
					QThread::msleep(50);
				}
				const auto heapObservation = heapControl.finish();
				std::cerr << "heap-control:" << *glz::write_json(heapObservation) << '\n';
				require(heapObservation.Samples > 1U && heapObservation.PeakBytes >= heapObservation.BaselineBytes + 32U * 1024U * 1024U
							&& heapObservation.FinalBytes + 16U * 1024U * 1024U < heapObservation.PeakBytes,
						"independent heap sampler missed held allocation or release");
#endif
				std::cout << "LARGE_REVIEW_TIMING_CONTROL_OK\n";
				return EXIT_SUCCESS;
			}
			auto& window = application->window();
			window.resize(1600, 1000);
			application->show();
			report.WindowWidth      = window.width();
			report.WindowHeight     = window.height();
			report.DevicePixelRatio = window.devicePixelRatioF();
			if (report.ResponsivenessCourse) {
				const auto startupFrame            = window.grab();
				report.StartupRendered             = !startupFrame.isNull();
				report.CompositionStartupMs        = static_cast<double>(compositionStartup.nsecsElapsed()) / 1'000'000.0;
				report.Extended.MainToHomeRenderMs = static_cast<double>(aMainClock.nsecsElapsed()) / 1'000'000.0;
				std::cout << "LARGE_REVIEW_HOME_RENDERED\n"
						  << std::flush;
				require(report.StartupRendered && startupFrame.save(QString::fromLocal8Bit(argv[3]) + QStringLiteral(".startup.png")), "composition startup did not render Home");
			}
			auto&         surface = *window.comparison_surface();
			QElapsedTimer budget;
			budget.start();
			sample_resources(report, application.get(), "application_constructed");
			require(measure_wait(*application, budget, report, "fixture_connect", [&] { window.github_connect_action()->trigger(); }, [&] { return window.github_status_text() == "GitHub account connected." && window.github_open_action()->isEnabled(); }), "connect timed out");
			// [[BPR-0771]]. A pull request is named in the Home bar; the locator is retired.
			window.home_open_bar()->setText(QStringLiteral("bpr-fixtures/large-review#17"));
			if (report.Extended.ProviderCancelCourse) {
				report.Fixture = "large-review-v1 held provider read";
				run_provider_cancellation(*application, *heldRead, report, budget);
				const auto encoded = glz::write_json(report);
				require(encoded.has_value(), "provider cancellation report encoding");
				std::ofstream output{ argv[2], std::ios::binary };
				output << *encoded << '\n';
				output.close();
				require(output.good(), "provider cancellation report write");
				std::cout << "LARGE_REVIEW_PROVIDER_CANCEL_OK\n"
						  << *encoded << '\n';
				return EXIT_SUCCESS;
			}
			QElapsedTimer metadataClock;
			QTimer        metadataObserver;
			QPixmap       provisionalFrame, acceptedFrame;
			metadataObserver.setInterval(1);
			QObject::connect(&metadataObserver, &QTimer::timeout, &window, [&] {
				auto* page = window.overview_page();
				if (!window.showing_overview() || !page->isVisible()) {
					return;
				}
				if (!report.Extended.ProvisionalMetadataObserved && page->summary_label()->text().contains(QStringLiteral("Provisional metadata"))
					&& !page->entry_button()->isEnabled() && page->file_model()->file_count() == 0U) {
					const auto captureStart                     = metadataClock.nsecsElapsed();
					provisionalFrame                            = window.grab();
					report.Extended.ProvisionalCaptureMs        = static_cast<double>(metadataClock.nsecsElapsed() - captureStart) / 1'000'000.0;
					report.Extended.ProvisionalMetadataObserved = !provisionalFrame.isNull();
					report.Extended.ProvisionalMetadataRenderMs = static_cast<double>(metadataClock.nsecsElapsed()) / 1'000'000.0;
					report.Extended.MetadataTitle               = page->title_label()->text().toStdString();
				}
				if (!report.Extended.AcceptedMetadataObserved && page->projection().has_value() && page->file_model()->file_count() == 515U) {
					const auto captureStart                  = metadataClock.nsecsElapsed();
					acceptedFrame                            = window.grab();
					report.Extended.AcceptedCaptureMs        = static_cast<double>(metadataClock.nsecsElapsed() - captureStart) / 1'000'000.0;
					report.Extended.AcceptedMetadataObserved = !acceptedFrame.isNull()
						&& page->title_label()->text().toStdString() == report.Extended.MetadataTitle
						&& report.Extended.MetadataTitle == "Synthetic large review benchmark";
					report.Extended.AcceptedMetadataRenderMs = static_cast<double>(metadataClock.nsecsElapsed()) / 1'000'000.0;
				}
			});
			if (report.ResponsivenessCourse) {
				metadataClock.start();
				metadataObserver.start();
			}
			require(measure_wait(*application, budget, report, "paged_review_and_git_load", [&] { window.home_open_action()->trigger(); }, [&] { return window.files_drawer()->contents_view()->model()->rowCount() == 515 && access::evidence(*application).SmartFetchCount == 1 && application->status().EpochStatus == core::epoch_status::ready && access::diff_work_evidence(*application).NavigationReady && quiet(*application); }), "review load timed out");
			metadataObserver.stop();
			if (report.ResponsivenessCourse) {
				require(report.Extended.ProvisionalMetadataObserved && report.Extended.AcceptedMetadataObserved
							&& report.Extended.ProvisionalMetadataRenderMs < report.Extended.AcceptedMetadataRenderMs,
						"distinct provisional and accepted metadata did not render");
				require(provisionalFrame.save(QString::fromLocal8Bit(argv[3]) + QStringLiteral(".provisional.png"))
							&& acceptedFrame.save(QString::fromLocal8Bit(argv[3]) + QStringLiteral(".accepted.png")),
						"metadata frame write failed");
			}
			verify_snapshot(*application, manifest, report);
			sample_resources(report, application.get(), "review_loaded");
			if (report.FirstTabCourse) {
				run_first_tab(*application, manifest, report, budget, argv[3]);
			} else if (report.CacheByteCourse) {
				run_cache_bytes(*application, manifest, report, budget, argv[3]);
			} else if (report.CacheEntryCourse) {
				run_cache_entries(*application, manifest, report, budget, argv[3]);
			} else if (report.HistoryCourse) {
				run_history(*application, manifest, report, budget, argv[3]);
			} else if (report.UnavailableCourse) {
				enter_review(*application, report, budget);
				const auto limitedPath = report.Extended.PathologicalCourse ? QStringLiteral("generated/table.inc") : QStringLiteral("large/comparison.cpp");
				require(measure_wait(*application, budget, report, "unavailable_file_selection", [&] { select_file(window, limitedPath); }, [&] { return selected(surface, limitedPath) && quiet(*application) && !window.grab().isNull(); }), "unavailable selection timed out");
				const auto alignment   = surface.current_result()->Alignment;
				const auto limitedFile = std::ranges::find(manifest.files, limitedPath.toStdString(), &fixture_file::path);
				require(limitedFile != manifest.files.end() && alignment.Key.LeftBlob == core::parse_git_blob_oid(limitedFile->base.oid)
							&& alignment.Key.RightBlob == core::parse_git_blob_oid(limitedFile->head.oid),
						"fallback lost exact stored blob identity");
				require(alignment.Content == core::diff_content_state::unavailable && alignment.Diagnostic.Code == (report.Extended.PathologicalCourse ? core::diff_reason_code::materialization_limit : core::diff_reason_code::compute_limit), "real host result must exceed the retained comparison limit");
				require(report.Extended.PathologicalCourse ? surface.status_text() == QStringLiteral("File exceeds the comparison materialization limit") : surface.status_text() == QStringLiteral("Comparison unavailable — file exceeds the comparison work limit"), "unavailable comparison falsely reports success");
				require(surface.left_pane()->toPlainText().isEmpty() && surface.right_pane()->toPlainText().isEmpty() && !surface.selection(), "unavailable comparison retains stale source");
				const auto  explanation = surface.status_text();
				auto* const tabStatus   = window.document_tabs()->status_label();
				require(tabStatus->isVisible() && tabStatus->text() == explanation && tabStatus->toolTip() == explanation
							&& tabStatus->accessibleDescription() == explanation,
						"compact tab status retains the complete unavailable reason");
				auto* label = surface.findChild<QLabel*>(QStringLiteral("diffWorkbenchStatus"));
				require(label && label->isVisible() && label->textInteractionFlags().testFlag(Qt::TextSelectableByKeyboard) && label->textInteractionFlags().testFlag(Qt::TextSelectableByMouse), "unavailable message must support copying");
				auto* const accessible = QAccessible::queryAccessibleInterface(label);
				require(label->text() == explanation && label->wordWrap() && accessible
							&& accessible->text(QAccessible::Description) == explanation && !accessible->state().invisible,
						"visible unavailable explanation is complete and accessible");
				label->setSelection(0, static_cast<int>(explanation.size()));
				const auto copyKey = QKeySequence{ QKeySequence::Copy }[0];
				auto       copy    = QKeyEvent{ QEvent::KeyPress, copyKey.key(), copyKey.keyboardModifiers() };
				QApplication::clipboard()->clear();
				QApplication::sendEvent(label, &copy);
				require(QApplication::clipboard()->text() == explanation, "Copy puts the complete unavailable reason on the clipboard");
				label->setSelection(0, 0);
				report.UnavailableReasonVerified = true;
				report.WindowWidth               = window.width();
				report.WindowHeight              = window.height();
				report.DevicePixelRatio          = window.devicePixelRatioF();
				report.Rendered                  = window.grab().save(QString::fromLocal8Bit(argv[3]));
				require(report.Rendered, "unavailable full-window capture failed");
				require(measure_wait(*application, budget, report, "binary_file_selection", [&] { select_file(window, QStringLiteral("assets/data.bin")); }, [&] { return selected(surface, QStringLiteral("assets/data.bin")) && quiet(*application); }), "binary selection timed out");
				require(surface.current_result()->Alignment.Content == core::diff_content_state::binary && surface.status_text() == QStringLiteral("Binary file comparison"), "binary status must be distinct");
				require(label->isVisible() && label->text() == surface.status_text() && tabStatus->isVisible(), "binary comparison keeps its full explanation visible");
				report.BinaryStatusVerified = true;
				require(measure_wait(*application, budget, report, "supported_file_recovery", [&] { select_file(window, source_path(31)); }, [&] { return selected(surface, source_path(31)) && quiet(*application); }), "supported recovery timed out");
				const auto  recovered    = std::ranges::find(manifest.files, source_path(31).toStdString(), &fixture_file::path);
				const auto& recoveredKey = surface.current_result()->Alignment.Key;
				report.ExactFinalSource  = recovered != manifest.files.end()
					&& recoveredKey.Authority == alignment.Key.Authority && recoveredKey.LeftAnchor == alignment.Key.LeftAnchor && recoveredKey.RightAnchor == alignment.Key.RightAnchor
					&& recoveredKey.LeftBlob == core::parse_git_blob_oid(recovered->base.oid)
					&& recoveredKey.RightBlob == core::parse_git_blob_oid(recovered->head.oid)
					&& surface.left_pane()->blockCount() == 9 && surface.right_pane()->blockCount() == 9;
				for (int line = 0; line < 8; ++line) {
					const auto common       = QStringLiteral("// synthetic source 0031 line %1").arg(line + 1);
					report.ExactFinalSource = report.ExactFinalSource
						&& surface.left_pane()->document()->findBlockByNumber(line).text() == (line == 0 ? QStringLiteral("int fixture_0031 = 0;") : common)
						&& surface.right_pane()->document()->findBlockByNumber(line).text() == (line == 0 ? QStringLiteral("int fixture_0031 = 1;") : common);
				}
				report.ExactFinalSource = report.ExactFinalSource && surface.left_pane()->document()->lastBlock().text().isEmpty()
					&& surface.right_pane()->document()->lastBlock().text().isEmpty();
				require(report.ExactFinalSource && surface.status_text().startsWith(QStringLiteral("Diff ready — 1 semantic changes")), "supported file must restore exact normal display");
				require(label->isHidden() && tabStatus->isHidden(), "exact text recovery restores the compact header and reading area");
				report.RecoverySegments = surface.current_result()->Alignment.Segments.size();
				require((report.Extended.PathologicalCourse || report.RecoverySegments <= report.UnavailableSegmentLimit) && !surface.current_result()->AlignmentApproximate, "supported file must compute exactly within the segment limit");
				require(access::diff_work_evidence(*application).FixtureDiffHelperInvocations == 0U, "unavailable course bypassed the real diff host");
			} else if (report.ApproximateCourse) {
				enter_review(*application, report, budget);
				require(measure_wait(*application, budget, report, "approximate_file_selection", [&] { select_file(window, QStringLiteral("large/comparison.cpp")); }, [&] { return selected(surface, QStringLiteral("large/comparison.cpp")) && quiet(*application); }), "approximate selection timed out");
				const auto& result = *surface.current_result();
				require(result.Alignment.Content == core::diff_content_state::text && result.Alignment.Diagnostic.Code == core::diff_reason_code::none, "work-limited host result must still publish a text comparison");
				require(result.AlignmentApproximate, "work-limited host result must be marked approximate alignment");
				report.ApproximateGroups   = result.Alignment.Groups.size();
				report.ApproximateSegments = result.Alignment.Segments.size();
				report.PrimaryIndexWork    = result.PrimaryIndexWorkConsumed;
				report.MatrixCells         = result.MatrixCellsConsumed;
				// The exact status fixes the prefix, the count and the approximate suffix, and
				// refuses a cached publication for this first selection.
				require(report.ApproximateGroups != 0U && surface.status_text() == QStringLiteral("Diff ready — %1 semantic changes (approximate alignment)").arg(report.ApproximateGroups), "approximate comparison must say so in its status");
				// Both panes hold the exact 20,000-line source, so neither is empty.
				verify_large(surface, manifest);
				report.ExactLargeSource             = true;
				report.ApproximateAlignmentVerified = true;
				report.WindowWidth                  = window.width();
				report.WindowHeight                 = window.height();
				report.DevicePixelRatio             = window.devicePixelRatioF();
				report.Rendered                     = window.grab().save(QString::fromLocal8Bit(argv[3]));
				require(report.Rendered, "approximate full-window capture failed");
				require(access::diff_work_evidence(*application).FixtureDiffHelperInvocations == 0U, "approximate course bypassed the real diff host");
			} else {
				// [[BPR-1089]]. The Overview the review opens on counts its 1,000
				// threads by label; Open to All and back, and a sort by every
				// conversation and back to file order, are each one input
				// answered from the kept label buckets.
				if (report.ResponsivenessCourse) {
					auto* const page     = window.overview_page();
					const auto  answered = [&](const core::feedback_resolution_mode aMode, const bool aSorted) {
						const auto& feedback = page->file_model()->feedback();
						return feedback.has_value() && feedback->Request.Mode == aMode && feedback->Request.Sort.has_value() == aSorted
							&& feedback->Order.empty() != aSorted && quiet(*application);
					};
					require(window.showing_overview() && answered(core::feedback_resolution_mode::unresolved, false)
								&& page->file_model()->feedback()->PullRequest.Counts.ScopeTotal == 1000U,
							"the Overview counts all 1,000 threads before any input");
					require(measure_wait(*application, budget, report, "overview_counts_all", [&] { page->all_counts_button()->click(); }, [&] { return answered(core::feedback_resolution_mode::all, false); }), "Overview All timed out");
					require(measure_wait(*application, budget, report, "overview_counts_open", [&] { page->open_counts_button()->click(); }, [&] { return answered(core::feedback_resolution_mode::unresolved, false); }), "Overview Open timed out");
					require(measure_wait(*application, budget, report, "overview_sort_by_comments", [&] { page->choose_sort(core::feedback_count_target{ .Kind = core::feedback_count_kind::included }); }, [&] { return answered(core::feedback_resolution_mode::unresolved, true); }), "Overview sort timed out");
					require(measure_wait(*application, budget, report, "overview_file_order", [&] { page->choose_sort(std::nullopt); }, [&] { return answered(core::feedback_resolution_mode::unresolved, false); }), "Overview file order timed out");
				}
				// [[BPR-0940]]. The ordinary course reads the large file where a
				// reviewer does: in Review, so its measurement, navigation and
				// wheel run on the visible comparison and its render shows it.
				enter_review(*application, report, budget);
				const auto          beforeMeasurement = surface.statistics().MeasurementBatches;
				QTimer              inputDuringMeasurement;
				QElapsedTimer       measurementInputClock;
				std::vector<double> measurementInputIntervals;
				QPixmap             firstSourceFrame;
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
				require(measure_wait(*application, budget, report, "large_file_selection", [&] { select_file(window, QStringLiteral("large/comparison.cpp")); }, [&] {
					if (!selected(surface, QStringLiteral("large/comparison.cpp")) || !quiet(*application) || surface.scroll_controller()->maximum_position() - surface.scroll_controller()->minimum_position() <= 1000.0) { return false; }
					if (report.ResponsivenessCourse) {
						require(surface.isVisible() && surface.left_pane()->isVisible() && surface.right_pane()->isVisible(), "first source render is visible");
						firstSourceFrame = window.grab();
						report.Extended.MainToFirstSourceRenderMs = static_cast<double>(aMainClock.nsecsElapsed()) / 1'000'000.0;
						require(!firstSourceFrame.isNull(), "first source render failed");
						++report.InteractionFrames;
					}
					return true; }), "large selection timed out");
				inputDuringMeasurement.stop();
				if (report.ResponsivenessCourse) {
					require(firstSourceFrame.save(QString::fromLocal8Bit(argv[3]) + QStringLiteral(".first-source.png")), "first source frame write failed");
				}
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
				// [[BPR-1291]]. The bar's start is where the panes start, which is
				// past the comparison's first position by the synchronization rail.
				const auto    start                = controller->position();
				const auto    before               = surface.statistics();
				const auto    scrollBefore         = controller->statistics();
				const auto    classificationBefore = application->status().ClassificationSubmissions;
				QElapsedTimer wheel;
				wheel.start();
				for (int event = 0; event < 1000; ++event) {
					QElapsedTimer eventTimer;
					eventTimer.start();
					QWheelEvent inputEvent{ QPointF{ 20, 20 }, QPointF{ 20, 20 }, QPoint{ 0, -1 }, QPoint{ }, Qt::NoButton, Qt::NoModifier, Qt::ScrollUpdate, false };
					QApplication::sendEvent(surface.right_pane()->viewport(), &inputEvent);
					report.MaxWheelEventMs = std::max(report.MaxWheelEventMs, static_cast<double>(eventTimer.nsecsElapsed()) / 1'000'000.0);
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
							&& application->status().ClassificationSubmissions == classificationBefore
							&& start == controller->minimum_position() && std::abs(controller->position() - (start + 1000.0)) < 0.000'001,
						"scroll gesture work/position contract");
				surface.shared_scrollbar()->setValue(0);
				QApplication::processEvents();
				const auto topLine = [](const QPlainTextEdit* const aPane) {
					return aPane->cursorForPosition(QPoint{ 0, 0 }).block().text();
				};
				require(window.modality()->current() == qt::review_modality::review && surface.isVisible()
							&& surface.left_pane()->isVisible() && surface.right_pane()->isVisible()
							&& selected(surface, QStringLiteral("large/comparison.cpp"))
							&& topLine(surface.left_pane()) == QStringLiteral("int sample_00000 = 0;")
							&& topLine(surface.right_pane()) == QStringLiteral("int sample_00000 = 1;"),
						"the full-window render shows Review with the large file's first source lines in both panes");
				report.WindowWidth      = window.width();
				report.WindowHeight     = window.height();
				report.DevicePixelRatio = window.devicePixelRatioF();
				report.Rendered         = window.grab().save(QString::fromLocal8Bit(argv[3]));
				require(report.Rendered, "full-window capture failed");

				if (report.ResponsivenessCourse) {
					run_responsiveness_interactions(*application, manifest, report, budget, argv[3]);
				}
				if (report.ActiveWorkCourse) {
					run_active_work(*application, manifest, report, budget, argv[3]);
				} else if (!report.ResourceCourse) {
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
				const auto work = access::diff_work_evidence(*application);
				if (!report.ActiveWorkCourse) {
					report.DiffCacheEntries = work.CacheEntries;
					report.DiffCacheBytes   = work.CacheBytes;
				}
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
				if (report.CacheEntryCourse || report.CacheByteCourse) {
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
						&& retained.TargetHeadRefOid == (report.CacheByteCourse ? manifest.heads.at(static_cast<std::size_t>(closeTarget->Ordinal.value() - 1)) : manifest.heads.back()) && retained.TargetHeadCatalogOid == retained.TargetHeadRefOid;
					require(report.ClosePreservedEpochStorage, "window close changed retained Git/catalog data or performed maintenance");
				}
				if (report.CacheByteCourse) {
					for (std::size_t index = 0; index < manifest.heads.size(); ++index) {
						const auto target   = core::review_epoch_id{ closeTarget->Authority, closeTarget->Incarnation, core::epoch_ordinal{ static_cast<std::int64_t>(index + 1U) } };
						const auto retained = access::maintenance_evidence(*application, target, closeComparison);
						require(retained.RepositoryPresent && retained.TargetBaseRefPresent && retained.TargetHeadRefPresent
									&& retained.TargetBaseRefOid == manifest.base && retained.TargetBaseCatalogOid == manifest.base
									&& retained.TargetHeadRefOid == manifest.heads[index] && retained.TargetHeadCatalogOid == manifest.heads[index],
								"byte pressure or shutdown changed a retained Git/catalog pair");
						++report.RetainedEpochsVerified;
					}
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
			for (const auto& phase : report.Phases) {
				if (phase.MaxPulseIntervalMs > report.InputIntervalLimitMs) {
					report.InputBudgetFailures.push_back(phase.Name);
				}
			}
			report.Extended.Heap = heapSampler.finish();
			if (observeResources) {
				report.Extended.SampledPeakHeapGrowthLimitBytes = report.CacheByteCourse
					? report.CacheByteFootprintGrowthLimitBytes
					: report.SampledFootprintGrowthLimitBytes;
				require(report.Extended.Heap.Samples > 1U, "live-heap sampler took no samples");
				require_limit(report, report.Extended.AllocationProfile || report.Extended.Heap.PeakBytes <= report.Extended.Heap.BaselineBytes + report.Extended.SampledPeakHeapGrowthLimitBytes,
							  "sampled live-heap peak growth exceeded the platform fixture ceiling");
			}
			if (report.Extended.PathologicalCourse) {
				for (const auto* name : { "unavailable_file_selection", "binary_file_selection", "supported_file_recovery" }) {
					const auto found = std::ranges::find(report.Phases, name, &phase_measurement::Name);
					require(found != report.Phases.end(), "pathological fallback phase missing");
					report.CompletionBudgets.push_back({ name, found->ElapsedMs, 1'500.0, found->ElapsedMs <= 1'500.0, report.Extended.BudgetsEnforced });
				}
				require(budgets_met(report), "pathological fallback completion/input budget exceeded");
			}
			application.reset();
			const auto diagnostics = bpr::tests::read_diagnostic_logs(dataRoot);
			require(diagnostics.find("\"op\":\"application\",\"outcome\":\"shutdown\"") != std::string::npos, "large public course closes the actual diagnostic file");
			const auto exported = application::prepare_release_diagnostics(diagnostics, core::release_maintenance_revision{ 1U });
			require(exported && exported->Descriptor.Records != 0U, "large public course reaches the actual export classifier");
			for (const auto* marker : { "int fixture_", "// synthetic source", "int sample_", "large/comparison.cpp", "access_token", "refresh_token" }) {
				require(diagnostics.find(marker) == std::string::npos && exported->Bytes.find(marker) == std::string::npos, "large synthetic source or request content escaped into diagnostics");
			}
			require(diagnostics.find(dataRoot.string()) == std::string::npos, "large diagnostics exclude local root");
			verify_responsiveness_budgets(report);
			verify_active_work_budgets(report);
			// [[BPR-0967]]. In an optimized build every course gates the 100 ms
			// input target on all its phases, except the four resource courses:
			// their 1 ms heap sampler perturbs the GUI thread it measures
			// (Windows load gaps of 208-315 ms with it, 39-60 ms without).
			require(!OptimizedBuild || observeResources || input_target_met(report), "fixture input target missed outside the platform's named phases");
			auto encoded = glz::write_json(report);
			require(encoded.has_value(), "measurement serialization");
			std::ofstream output{ argv[2], std::ios::binary };
			output << *encoded << '\n';
			output.close();
			require(output.good(), "measurement write");
			if (report.Extended.AllocationProfile) {
				// Diagnostic only: preserve source, cache and lifecycle assertions,
				// but instrumentation overhead cannot pass/fail native budgets.
				// Allow a bounded external no-content allocation-history capture
				// after application destruction. Never print an acceptance marker.
				std::cout << "LARGE_REVIEW_ALLOCATION_CAPTURE_READY\n"
						  << std::flush;
				QThread::sleep(10);
				return EXIT_SUCCESS;
			}
			std::cout << (report.ActiveWorkCourse ? "LARGE_REVIEW_ACTIVE_WORK_MEASURED\n" : report.ResponsivenessCourse ? "LARGE_REVIEW_RESPONSIVENESS_MEASURED\n"
							  : report.FirstTabCourse                                                                   ? "FIRST_TAB_COMPARISON_OK\n"
							  : report.CacheByteCourse                                                                  ? "LARGE_REVIEW_CACHE_BYTES_OK\n"
							  : report.CacheEntryCourse                                                                 ? "LARGE_REVIEW_CACHE_ENTRIES_OK\n"
							  : report.LoadedCloseCourse                                                                ? "LARGE_REVIEW_LOADED_CLOSE_OK\n"
							  : report.HistoryCourse                                                                    ? "LARGE_REVIEW_HISTORY_OK\n"
							  : report.ResourceCourse                                                                   ? "LARGE_REVIEW_RESOURCES_OK\n"
							  : report.UnavailableCourse                                                                ? "UNAVAILABLE_DIFF_COMPOSED_OK\n"
							  : report.ApproximateCourse                                                                ? "APPROXIMATE_DIFF_COMPOSED_OK\n"
																														: "LARGE_REVIEW_COMPOSED_BENCHMARK_OK\n")
					  << *encoded << '\n';
			return EXIT_SUCCESS;
		} catch (const std::exception& error) {
			report.Extended.Heap    = heapSampler.finish();
			report.Extended.Failure = error.what();
			std::cerr << "large-review-benchmark-failed: " << error.what() << '\n';
			// [[BPR-1265]]. The status says only that an epoch failed; the
			// application's own sanitized diagnostic log says which failure.
			// Close it, then relay its failure records before the root goes.
			try {
				application.reset();
				std::istringstream       records{ bpr::tests::read_diagnostic_logs(dataRoot) };
				std::vector<std::string> unusual;
				for (std::string record; std::getline(records, record);) {
					if (record.find("\"severity\":\"info\"") == std::string::npos || record.find("\"failure\":\"none\"") == std::string::npos) {
						unusual.push_back(std::move(record));
					}
				}
				const auto first = unusual.size() > 30U ? unusual.size() - 30U : 0U;
				for (auto index = first; index < unusual.size(); ++index) {
					std::cerr << "large-review-diagnostic:" << unusual[index] << '\n';
				}
			} catch (...) {
				std::cerr << "large-review-diagnostic: unavailable\n";
			}
			if (const auto partial = glz::write_json(report); partial) {
				std::cerr << "large-review-partial:" << *partial << '\n';
				// [[BPR-0967]]. A failed course leaves its partial report, not a
				// stale earlier success, at the report path.
				std::ofstream output{ argv[2], std::ios::binary };
				output << *partial << '\n';
			}
			return EXIT_FAILURE;
		}
	}

} // namespace

int main(int argc, char** argv)
{
	QElapsedTimer mainClock;
	mainClock.start();
	std::cout << "LARGE_REVIEW_MAIN_ENTERED\n"
			  << std::flush;
	QApplication qtApplication{ argc, argv };
	const auto   qtStartupMs = static_cast<double>(mainClock.nsecsElapsed()) / 1'000'000.0;
	QApplication::setQuitOnLastWindowClosed(false);
	// Native last-window-close delivery needs QApplication's main event loop,
	// even though individual measurement phases use nested event loops.
	QTimer::singleShot(0, &qtApplication, [&] {
		qtApplication.exit(bpr::e2e::large_review::run_benchmark(qtApplication, argc, argv, mainClock, qtStartupMs));
	});
	return qtApplication.exec();
}
