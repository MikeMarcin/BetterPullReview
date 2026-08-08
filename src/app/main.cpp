#include "bpr/app/github_application.hpp"

#if defined(BPR_BUILD_TESTING)
	#include "github_application_fixture_access.hpp"

	#if defined(__APPLE__)
		#include <CoreFoundation/CoreFoundation.h>
	#endif
#endif

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QSpinBox>
#include <QTextStream>
#include <QTimer>

#if defined(BPR_BUILD_TESTING)
	#include <QDir>
	#include <QFile>
	#include <QFileInfo>
	#include <QFont>
	#include <QGuiApplication>
	#include <QImage>
	#include <QItemSelectionModel>
	#include <QMessageBox>
	#include <QMouseEvent>
	#include <QPalette>
	#include <QPersistentModelIndex>
	#include <QPlainTextEdit>
	#include <QPushButton>
	#include <QScrollArea>
	#include <QSplitter>
	#include <QStyle>
	#include <QStyleHints>
	#include <QTreeView>
	#include <QWheelEvent>
#endif

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>

#if defined(BPR_BUILD_TESTING)
	#include <cstring>
#endif

namespace {
	// How long an automated run may take before it stops waiting, records why,
	// and shuts itself down.
	//
	// Every harness that starts this process bounds it with a CTest `TIMEOUT`,
	// and a run killed by that timeout reports nothing at all: no stage, no
	// verdict, no evidence. The run therefore has to reach its own limit first,
	// which means the limit has to be measured in the same units the harness
	// uses. Wall clock, not ticks: a tick budget is only comparable to a
	// timeout if every tick costs what it costs on an idle machine, and under
	// load the event loop stretches exactly when the harness clock does not.
	//
	// Tests state their own budget next to their `TIMEOUT` so the two numbers
	// are read together. This default is only for a run started without one,
	// and fits inside the shortest timeout in the suite (15 s) with room left
	// for process start, shutdown, and writing evidence.
	using automated_clock = std::chrono::steady_clock;

	constexpr auto default_automated_run_budget = std::chrono::milliseconds{ 8'000 };

	auto has_argument(
		const int              aArgc,
		char* const* const     aArgv,
		const std::string_view aExpected) -> bool
	{
		return std::any_of(
			aArgv + 1,
			aArgv + aArgc,
			[aExpected](const char* const aArgument) { return std::string_view{ aArgument } == aExpected; });
	}

	auto argument_value(
		const int              aArgc,
		char* const* const     aArgv,
		const std::string_view aExpected) -> std::optional<std::string_view>
	{
		for (auto index = 1; index + 1 < aArgc; ++index) {
			if (std::string_view{ aArgv[index] } == aExpected) {
				return std::string_view{ aArgv[index + 1] };
			}
		}
		return std::nullopt;
	}

	auto save_render_evidence(
		bpr::qt::github_window&                aWindow,
		const std::optional<std::string_view>& aRenderPath) -> bool
	{
		if (!aRenderPath.has_value()) {
			return true;
		}
		const auto path = QString::fromUtf8(
			aRenderPath->data(),
			static_cast<qsizetype>(aRenderPath->size()));
		return aWindow.grab().save(path, "PNG");
	}

#if defined(BPR_BUILD_TESTING)
	struct native_appearance_evidence_paths
	{
		QString Initial;
		QString Changed;
	};

	[[nodiscard]] auto resolved_fresh_render_path(
		const std::string_view aRawPath) -> std::optional<QString>
	{
		if (aRawPath.empty()) {
			return std::nullopt;
		}

		const auto path = QString::fromUtf8(
			aRawPath.data(),
			static_cast<qsizetype>(aRawPath.size()));
		const auto supplied = QFileInfo{ path };
		if (supplied.fileName().isEmpty()) {
			return std::nullopt;
		}

		const auto parent = supplied.dir().canonicalPath();
		if (parent.isEmpty()) {
			return std::nullopt;
		}

		auto       resolved    = QDir::cleanPath(QDir{ parent }.filePath(supplied.fileName()));
		const auto destination = QFileInfo{ resolved };
		if (destination.exists() || destination.isSymbolicLink()) {
			return std::nullopt;
		}
		return resolved;
	}

	[[nodiscard]] auto resolve_native_appearance_evidence_paths(
		const std::optional<std::string_view>& aInitial,
		const std::optional<std::string_view>& aChanged)
		-> std::optional<native_appearance_evidence_paths>
	{
		if (!aInitial.has_value() || !aChanged.has_value()) {
			return std::nullopt;
		}

		const auto resolvedInitial = resolved_fresh_render_path(*aInitial);
		const auto resolvedChanged = resolved_fresh_render_path(*aChanged);
		if (
			!resolvedInitial.has_value() || !resolvedChanged.has_value()
			|| *resolvedInitial == *resolvedChanged) {
			return std::nullopt;
		}

		return native_appearance_evidence_paths{
			.Initial = *resolvedInitial,
			.Changed = *resolvedChanged,
		};
	}

	[[nodiscard]] auto capture_normalized_render(
		bpr::qt::github_window& aWindow) -> QImage
	{
		auto image = aWindow.grab().toImage();
		if (image.isNull()) {
			return { };
		}

		const auto devicePixelRatio = image.devicePixelRatio();
		image                       = image.convertToFormat(QImage::Format_RGBA8888);
		image.setDevicePixelRatio(devicePixelRatio);
		return image;
	}

	[[nodiscard]] auto same_render_pixels(
		const QImage& aLeft,
		const QImage& aRight) -> bool
	{
		if (aLeft.isNull() || aRight.isNull() || aLeft.size() != aRight.size()) {
			return false;
		}

		const auto normalizedLeft  = aLeft.convertToFormat(QImage::Format_RGBA8888);
		const auto normalizedRight = aRight.convertToFormat(QImage::Format_RGBA8888);
		const auto rowSize         = static_cast<std::size_t>(normalizedLeft.width()) * 4U;
		for (auto row = 0; row < normalizedLeft.height(); ++row) {
			if (
				std::memcmp(
					normalizedLeft.constScanLine(row),
					normalizedRight.constScanLine(row),
					rowSize)
				!= 0) {
				return false;
			}
		}
		return true;
	}

	[[nodiscard]] auto save_new_png(
		const QImage&  aImage,
		const QString& aPath) -> bool
	{
		QFile output{ aPath };
		if (
			aImage.isNull()
			|| !output.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
			return false;
		}

		const auto saved = aImage.save(&output, "PNG") && output.flush();
		output.close();
		if (!saved) {
			output.remove();
		}
		return saved;
	}

	[[nodiscard]] auto persist_native_appearance_evidence(
		const QImage&                           aInitial,
		const QImage&                           aChanged,
		const native_appearance_evidence_paths& aPaths) -> bool
	{
		if (!save_new_png(aInitial, aPaths.Initial)) {
			return false;
		}
		if (!save_new_png(aChanged, aPaths.Changed)) {
			QFile::remove(aPaths.Initial);
			return false;
		}

		const auto persistedInitial = QImage{ aPaths.Initial, "PNG" };
		const auto persistedChanged = QImage{ aPaths.Changed, "PNG" };
		const auto passed           = same_render_pixels(persistedInitial, aInitial)
			&& same_render_pixels(persistedChanged, aChanged)
			&& !same_render_pixels(persistedInitial, persistedChanged);
		if (!passed) {
			QFile::remove(aPaths.Initial);
			QFile::remove(aPaths.Changed);
		}
		return passed;
	}

	[[nodiscard]] auto macos_global_color_scheme() noexcept
		-> Qt::ColorScheme
	{
	#if defined(__APPLE__)
		if (!CFPreferencesSynchronize(
				kCFPreferencesAnyApplication,
				kCFPreferencesCurrentUser,
				kCFPreferencesAnyHost)) {
			return Qt::ColorScheme::Unknown;
		}

		// Qt's effective appearance can be overridden per application, so the
		// test gate independently reads macOS's global appearance domain.
		const auto value = CFPreferencesCopyValue(
			CFSTR("AppleInterfaceStyle"),
			kCFPreferencesAnyApplication,
			kCFPreferencesCurrentUser,
			kCFPreferencesAnyHost);
		if (value == nullptr) {
			return Qt::ColorScheme::Light;
		}

		auto scheme = Qt::ColorScheme::Unknown;
		if (
			CFGetTypeID(value) == CFStringGetTypeID()
			&& CFEqual(value, CFSTR("Dark"))) {
			scheme = Qt::ColorScheme::Dark;
		}
		CFRelease(value);
		return scheme;
	#else
		return Qt::ColorScheme::Unknown;
	#endif
	}
#endif

	struct automated_run
	{
		enum class stage {
			start,
			connect,
			open,
#if defined(BPR_BUILD_TESTING)
			epoch_history,
			epoch_refresh,
			epoch_whole_pr,
			epoch_previous_upload,
			epoch_last_reviewed,
			epoch_arbitrary_left,
			epoch_arbitrary,
			epoch_reverse_left,
			epoch_reverse,
			epoch_identical_left,
			epoch_identical,
			epoch_maintenance_history,
			epoch_maintenance_select_right,
			epoch_maintenance_prune,
			epoch_maintenance_prune_result,
			epoch_maintenance_delete_result,
			appearance,
#endif
			shutdown,
		};

		struct cli_unavailable_probe_state
		{
			bool                       Enabled = false;
			std::optional<std::string> EnterpriseHostname;
			int                        ActiveRow                      = -1;
			std::size_t                Expected                       = 0U;
			std::size_t                Observed                       = 0U;
			std::size_t                IdentityUpdatesBeforeConnect   = 0U;
			std::size_t                DiagnosticUpdatesBeforeConnect = 0U;
			std::string                ExpectedActiveHostText;
		};

		stage Stage = stage::start;
		// Ticks spent in the current stage. Not the bound — the bound is
		// `Budget` below — but reported alongside it, because a run that
		// exhausted its budget after a handful of ticks was blocked, and one
		// that exhausted it after thousands was waiting for something that
		// never became true. Those need different investigations.
		std::size_t Attempts = 0U;
		// When the run started, and how long it may take. See
		// `default_automated_run_budget`.
		automated_clock::time_point Started;
		std::chrono::milliseconds   Budget                    = default_automated_run_budget;
		bool                        BudgetExpired             = false;
		bool                        PresentationPassed        = false;
		bool                        RenderSaved               = true;
		bool                        VerifyNativePlatformVault = false;
		QString                     LastEpochStatus;
		QString                     LastEpochComparison;
		QString                     LastEpochLeft;
		QString                     LastEpochRight;
		int                         LastEpochCoreStatus     = -1;
		int                         LastEpochLeftRow        = -1;
		int                         LastEpochRightRow       = -1;
		std::size_t                 LastEpochCount          = 0U;
		bool                        LastEpochHistoryEnabled = false;
		int                         FailedStage             = -1;
		cli_unavailable_probe_state Cli;
#if defined(BPR_BUILD_TESTING)
		struct native_appearance_probe_state
		{
			bool                                         Enabled                      = false;
			bool                                         WaitingForOwnerFocusReported = false;
			bool                                         SignalObserved               = false;
			bool                                         StatePassed                  = false;
			bool                                         PixelsDiffer                 = false;
			std::size_t                                  SettleAttempts               = 0U;
			std::size_t                                  GlobalPollTicks              = 0U;
			Qt::ColorScheme                              InitialColorScheme           = Qt::ColorScheme::Unknown;
			Qt::ColorScheme                              ObservedColorScheme          = Qt::ColorScheme::Unknown;
			Qt::ColorScheme                              InitialGlobalColorScheme     = Qt::ColorScheme::Unknown;
			Qt::ColorScheme                              ObservedGlobalColorScheme    = Qt::ColorScheme::Unknown;
			QPalette                                     InitialPalette;
			QString                                      InitialPlatformName;
			QString                                      InitialStyleClass;
			QPersistentModelIndex                        Thread;
			bool                                         InitialWindowActive = false;
			QWidget*                                     InitialFocus        = nullptr;
			std::size_t                                  InitialModelResets  = 0U;
			QString                                      ExpectedEpochLeftOid;
			QString                                      ExpectedEpochRightOid;
			QPersistentModelIndex                        EpochLeft;
			QPersistentModelIndex                        EpochRight;
			QString                                      EpochLeftIdentity;
			QString                                      EpochRightIdentity;
			QPalette                                     InitialEpochPalette;
			std::size_t                                  InitialEpochModelResets       = 0U;
			std::size_t                                  InitialEpochAppearanceUpdates = 0U;
			QImage                                       InitialRender;
			QImage                                       ChangedRender;
			std::optional<bpr::core::semantic_selection> InitialDiffSelection;
			QPalette                                     InitialDiffPalette;
			QPalette                                     ObservedDiffPalette;
			// BPR-0114's own evidence, distinct from the palette assertions
			// above. Those prove the toolkit follows the operating system;
			// these prove *this product's* appearance model does, and that an
			// explicit mode does not.
			bool                             ProfileFollowsPassed  = false;
			bool                             ExplicitIgnoresPassed = false;
			int                              VariantBeforeChange   = -1;
			int                              VariantAfterChange    = -1;
			int                              VariantUnderExplicit  = -1;
			int                              VariantBackOnSystem   = -1;
			QFont                            InitialDiffFont;
			QFont                            ObservedDiffFont;
			std::uint64_t                    InitialDiffGeometryRebuilds = 0U;
			std::uint64_t                    DiffGeometryRebuildDelta    = 0U;
			native_appearance_evidence_paths EvidencePaths;
		};

		bool Fixture           = false;
		bool DiffProbe         = false;
		bool DiffShutdownProbe = false;
		// A stage that never becomes ready. Every real readiness condition in
		// an automated run is one the fixture eventually satisfies, so without
		// a deliberate stall there is no way to reach the run's own timeout and
		// show that it fires before the harness kills the process.
		bool StallProbe                    = false;
		bool DiffInteractionEvidencePassed = false;
		bool DiffRequested                 = false;
		// Whether a theme change on the composed application repainted without
		// costing a complete-blob pass. The token-span cache is keyed by blob
		// identity and definition version alone, so a submission count that
		// moved across an appearance change means the key acquired a colour.
		bool SyntaxThemeRepaintPassed = false;
		// Whether this run should drive the composed settings surface. Off by
		// default so every other probe keeps reporting the settings the record
		// held, unchanged by anything this process did.
		bool                                         AppearanceGestureProbe  = false;
		bool                                         AppearanceGesturePassed = false;
		bool                                         AppearanceRefusalPassed = false;
		bool                                         LedgerPresentOnEntry    = false;
		bool                                         LedgerRoundTripped      = false;
		std::string                                  FixtureName;
		std::string                                  ExpectedActiveHostText;
		bpr::app::detail::github_epoch_fixture_phase EpochPhase = bpr::app::detail::github_epoch_fixture_phase::single;
		enum class epoch_maintenance_probe {
			none,
			prune,
			verify_pruned_and_delete,
			verify_deleted,
		};
		epoch_maintenance_probe         MaintenanceProbe = epoch_maintenance_probe::none;
		bpr::core::review_epoch_id      MaintenanceTarget;
		int                             MaintenanceLeftRow       = -1;
		int                             MaintenanceRightRow      = -1;
		int                             MaintenanceAccountReads  = 0;
		int                             MaintenanceSmartAcquires = 0;
		std::size_t                     MaintenanceModelResets   = 0U;
		int                             EpochLeftRow             = -1;
		int                             EpochRightRow            = -1;
		native_appearance_probe_state   Appearance;
		std::optional<std::string_view> DiffEvidenceDirectory;
		/// The appearance the diff captures are taken under, when one was
		/// asked for. Absent leaves the run on whatever composition resolved.
		std::optional<bpr::core::appearance_mode> DiffEvidenceAppearance;
#endif
		std::optional<std::string_view> RenderPath;
	};

#if defined(BPR_BUILD_TESTING)
	[[nodiscard]] auto color_scheme_name(
		const Qt::ColorScheme aScheme) noexcept -> const char*
	{
		switch (aScheme) {
		case Qt::ColorScheme::Unknown:
			return "Unknown";
		case Qt::ColorScheme::Light:
			return "Light";
		case Qt::ColorScheme::Dark:
			return "Dark";
		}
		return "Unknown";
	}

	[[nodiscard]] auto native_appearance_context_passed(
		const bpr::qt::github_window& aWindow,
		const automated_run&          aRun) -> bool
	{
		const auto* const view          = aWindow.conversation_view();
		const auto* const model         = aWindow.conversation_model();
		const auto* const control       = aWindow.epoch_control();
		const auto* const epochModel    = control->history_model();
		const auto* const leftSelector  = control->left_selector();
		const auto* const rightSelector = control->right_selector();
		const auto* const leftOid       = control->left_oid_editor();
		const auto* const rightOid      = control->right_oid_editor();
		const auto* const workbench     = aWindow.diff_comparison_workbench();

		const auto epochContextPreserved = aRun.Appearance.EpochLeft.isValid()
			&& aRun.Appearance.EpochRight.isValid()
			&& leftSelector->currentIndex()
				== aRun.Appearance.EpochLeft.row()
			&& rightSelector->currentIndex()
				== aRun.Appearance.EpochRight.row()
			&& epochModel->index(leftSelector->currentIndex(), 0)
				== aRun.Appearance.EpochLeft
			&& epochModel->index(rightSelector->currentIndex(), 0)
				== aRun.Appearance.EpochRight
			&& aRun.Appearance.EpochLeft.data(
											bpr::qt::epoch_history_model::identity_role)
					.toString()
				== aRun.Appearance.EpochLeftIdentity
			&& aRun.Appearance.EpochRight.data(
											 bpr::qt::epoch_history_model::identity_role)
					.toString()
				== aRun.Appearance.EpochRightIdentity
			&& leftOid->text() == aRun.Appearance.ExpectedEpochLeftOid
			&& rightOid->text() == aRun.Appearance.ExpectedEpochRightOid
			&& leftOid->isReadOnly() && rightOid->isReadOnly()
			&& control->whole_pr_action()->isChecked()
			&& epochModel->statistics().ModelResets
				== aRun.Appearance.InitialEpochModelResets
			&& !leftSelector->accessibleName().isEmpty()
			&& !leftSelector->accessibleDescription().isEmpty()
			&& !rightSelector->accessibleName().isEmpty()
			&& !rightSelector->accessibleDescription().isEmpty()
			&& !leftOid->accessibleName().isEmpty()
			&& !leftOid->accessibleDescription().isEmpty()
			&& !rightOid->accessibleName().isEmpty()
			&& !rightOid->accessibleDescription().isEmpty()
			&& !control->testAttribute(Qt::WA_SetPalette)
			&& !control->testAttribute(Qt::WA_SetStyle)
			&& !control->testAttribute(Qt::WA_SetFont)
			&& !leftSelector->testAttribute(Qt::WA_SetPalette)
			&& !rightSelector->testAttribute(Qt::WA_SetPalette)
			&& !leftOid->testAttribute(Qt::WA_SetPalette)
			&& !rightOid->testAttribute(Qt::WA_SetPalette)
			&& control->styleSheet().isEmpty()
			&& leftSelector->styleSheet().isEmpty()
			&& rightSelector->styleSheet().isEmpty()
			&& leftOid->styleSheet().isEmpty()
			&& rightOid->styleSheet().isEmpty();
		const auto diffContextPreserved = workbench != nullptr
			&& workbench->current_result() != nullptr
			&& workbench->selection() == aRun.Appearance.InitialDiffSelection
			&& workbench->status_text().startsWith(QStringLiteral("Diff ready"))
			&& !workbench->accessibleName().isEmpty()
			&& !workbench->left_pane()->accessibleDescription().isEmpty()
			&& !workbench->right_pane()->accessibleDescription().isEmpty()
			&& !workbench->detail_pane()->accessibleDescription().isEmpty()
			&& !workbench->testAttribute(Qt::WA_SetPalette)
			&& !workbench->testAttribute(Qt::WA_SetStyle)
			&& !workbench->testAttribute(Qt::WA_SetFont)
			&& workbench->styleSheet().isEmpty();

		return aRun.Appearance.Thread.isValid()
			&& QGuiApplication::platformName()
			== aRun.Appearance.InitialPlatformName
			&& QString::fromLatin1(
				   QApplication::style()->metaObject()->className())
			== aRun.Appearance.InitialStyleClass
			&& view->currentIndex() == aRun.Appearance.Thread
			&& view->selectionModel()->isSelected(aRun.Appearance.Thread)
			&& !view->isExpanded(aRun.Appearance.Thread)
			&& aRun.Appearance.InitialWindowActive
			&& aRun.Appearance.InitialFocus != nullptr
			&& aRun.Appearance.InitialFocus->window() == &aWindow
			&& aWindow.isActiveWindow()
			&& QApplication::activeWindow() == &aWindow
			&& QApplication::focusWidget() == aRun.Appearance.InitialFocus
			&& epochContextPreserved
			&& diffContextPreserved
			&& model->statistics().ModelResets
			== aRun.Appearance.InitialModelResets
			&& aWindow.statistics().FullResets == 0U
			&& !aWindow.testAttribute(Qt::WA_SetPalette)
			&& !aWindow.testAttribute(Qt::WA_SetStyle)
			&& !aWindow.testAttribute(Qt::WA_SetFont)
			&& aWindow.styleSheet().isEmpty();
	}

	[[nodiscard]] auto native_palette_readable(
		const QPalette& aPalette) noexcept -> bool
	{
		const auto contrast = [&aPalette](
								  const QPalette::ColorRole aBackground,
								  const QPalette::ColorRole aForeground) {
			return std::abs(
					   aPalette.color(aBackground).lightness()
					   - aPalette.color(aForeground).lightness())
				>= 80;
		};
		return contrast(QPalette::Window, QPalette::WindowText)
			&& contrast(QPalette::Base, QPalette::Text);
	}

	[[nodiscard]] auto native_palette_fingerprint(
		const QPalette& aPalette) -> QString
	{
		return aPalette.color(QPalette::Window).name(QColor::HexArgb)
			+ u':' + aPalette.color(QPalette::Base).name(QColor::HexArgb)
			+ u':' + aPalette.color(QPalette::Text).name(QColor::HexArgb)
			+ u':' + aPalette.color(QPalette::WindowText).name(QColor::HexArgb);
	}

	void settle_automated_events()
	{
		for (auto count = 0; count < 4; ++count) {
			QApplication::processEvents();
		}
	}

	[[nodiscard]] auto native_appearance_light_state_passed(
		const bpr::qt::github_window& aWindow,
		const automated_run&          aRun) -> bool
	{
		const auto* const hints = QGuiApplication::styleHints();
		return hints != nullptr && !aRun.Appearance.SignalObserved
			&& aRun.Appearance.InitialColorScheme == Qt::ColorScheme::Light
			&& aRun.Appearance.ObservedColorScheme == Qt::ColorScheme::Light
			&& aRun.Appearance.InitialGlobalColorScheme
			== Qt::ColorScheme::Light
			&& macos_global_color_scheme() == Qt::ColorScheme::Light
			&& hints->colorScheme() == Qt::ColorScheme::Light
			&& aWindow.palette() == aRun.Appearance.InitialPalette
			&& aWindow.epoch_control()->palette()
			== aRun.Appearance.InitialEpochPalette
			&& native_palette_readable(aWindow.epoch_control()->palette())
			&& aWindow.epoch_control()->statistics().AppearanceUpdates
			== aRun.Appearance.InitialEpochAppearanceUpdates
			&& native_appearance_context_passed(aWindow, aRun);
	}

	[[nodiscard]] auto native_appearance_dark_state_passed(
		const bpr::qt::github_window& aWindow,
		const automated_run&          aRun) -> bool
	{
		const auto&       finalPalette       = aWindow.palette();
		const auto        initialWindow      = aRun.Appearance.InitialPalette.color(QPalette::Window).lightness();
		const auto        initialBase        = aRun.Appearance.InitialPalette.color(QPalette::Base).lightness();
		const auto        initialText        = aRun.Appearance.InitialPalette.color(QPalette::Text).lightness();
		const auto        finalWindow        = finalPalette.color(QPalette::Window).lightness();
		const auto        finalBase          = finalPalette.color(QPalette::Base).lightness();
		const auto        finalText          = finalPalette.color(QPalette::Text).lightness();
		const auto        initialEpoch       = aRun.Appearance.InitialEpochPalette;
		const auto        finalEpoch         = aWindow.epoch_control()->palette();
		const auto        initialEpochWindow = initialEpoch.color(QPalette::Window).lightness();
		const auto        initialEpochBase   = initialEpoch.color(QPalette::Base).lightness();
		const auto        initialEpochText   = initialEpoch.color(QPalette::Text).lightness();
		const auto        finalEpochWindow   = finalEpoch.color(QPalette::Window).lightness();
		const auto        finalEpochBase     = finalEpoch.color(QPalette::Base).lightness();
		const auto        finalEpochText     = finalEpoch.color(QPalette::Text).lightness();
		const auto* const workbench          = aWindow.diff_comparison_workbench();
		const auto&       finalDiffPalette   = workbench->palette();

		return aRun.Appearance.SignalObserved
			&& aRun.Appearance.InitialColorScheme == Qt::ColorScheme::Light
			&& aRun.Appearance.ObservedColorScheme == Qt::ColorScheme::Dark
			&& aRun.Appearance.InitialGlobalColorScheme
			== Qt::ColorScheme::Light
			&& aRun.Appearance.ObservedGlobalColorScheme
			== Qt::ColorScheme::Dark
			&& macos_global_color_scheme() == Qt::ColorScheme::Dark
			&& QGuiApplication::styleHints()->colorScheme()
			== Qt::ColorScheme::Dark
			&& finalWindow < initialWindow && finalBase < initialBase
			&& finalText > initialText && finalWindow < finalText
			&& finalBase < finalText
			&& finalEpochWindow < initialEpochWindow
			&& finalEpochBase < initialEpochBase
			&& finalEpochText > initialEpochText
			&& finalEpochWindow < finalEpochText
			&& finalEpochBase < finalEpochText
			&& native_palette_readable(finalEpoch)
			&& finalDiffPalette != aRun.Appearance.InitialDiffPalette
			&& native_palette_readable(finalDiffPalette)
			&& workbench->font() == aRun.Appearance.InitialDiffFont
			&& aWindow.epoch_control()->statistics().AppearanceUpdates
			> aRun.Appearance.InitialEpochAppearanceUpdates
			&& workbench->statistics().GeometryRebuilds
			== aRun.Appearance.InitialDiffGeometryRebuilds + 1U
			&& native_appearance_context_passed(aWindow, aRun);
	}

	[[nodiscard]] auto prepare_native_appearance_probe(
		bpr::qt::github_window& aWindow,
		automated_run&          aRun) -> bool
	{
		aRun.RenderSaved       = false;
		auto* const hints      = QGuiApplication::styleHints();
		auto* const view       = aWindow.conversation_view();
		auto* const model      = aWindow.conversation_model();
		auto* const control    = aWindow.epoch_control();
		auto* const epochModel = control->history_model();
		auto* const workbench  = aWindow.diff_comparison_workbench();
		const auto  thread     = model->index_for_id("review-thread:PRRT_fixture_1");
		const auto  leftEpoch  = epochModel->index(
			control->left_selector()->currentIndex(), 0);
		const auto rightEpoch = epochModel->index(
			control->right_selector()->currentIndex(), 0);
		if (hints != nullptr) {
			aRun.Appearance.InitialColorScheme  = hints->colorScheme();
			aRun.Appearance.ObservedColorScheme = hints->colorScheme();
		}
		aRun.Appearance.InitialGlobalColorScheme  = macos_global_color_scheme();
		aRun.Appearance.ObservedGlobalColorScheme = aRun.Appearance.InitialGlobalColorScheme;
		aRun.Appearance.InitialPalette            = aWindow.palette();
		aRun.Appearance.InitialPlatformName       = QGuiApplication::platformName();
		aRun.Appearance.InitialStyleClass         = QString::fromLatin1(
			QApplication::style()->metaObject()->className());
		const auto initialText  = aRun.Appearance.InitialPalette.color(QPalette::Text).lightness();
		const auto lightPalette = aRun.Appearance.InitialPalette.color(QPalette::Window).lightness()
				> initialText
			&& aRun.Appearance.InitialPalette.color(QPalette::Base).lightness()
				> initialText;
		if (
			hints == nullptr || aRun.Appearance.InitialPlatformName != "cocoa"
			|| aRun.Appearance.InitialStyleClass != "QMacStyle"
			|| hints->colorScheme() != Qt::ColorScheme::Light
			|| aRun.Appearance.InitialGlobalColorScheme
				!= Qt::ColorScheme::Light
			|| !lightPalette || !thread.isValid()
			|| !leftEpoch.isValid() || !rightEpoch.isValid()
			|| aRun.Appearance.ExpectedEpochLeftOid.isEmpty()
			|| aRun.Appearance.ExpectedEpochRightOid.isEmpty()
			|| control->left_oid_editor()->text()
				!= aRun.Appearance.ExpectedEpochLeftOid
			|| control->right_oid_editor()->text()
				!= aRun.Appearance.ExpectedEpochRightOid
			|| !control->whole_pr_action()->isChecked()
			|| workbench == nullptr || workbench->current_result() == nullptr
			|| !workbench->selection().has_value()
			|| !aRun.RenderPath.has_value()
			|| aRun.Appearance.EvidencePaths.Initial.isEmpty()
			|| aRun.Appearance.EvidencePaths.Changed.isEmpty()) {
			QTextStream{ stdout }
				<< "NATIVE_APPEARANCE_PRECONDITION:"
				<< (hints != nullptr) << ':'
				<< (aRun.Appearance.InitialPlatformName == "cocoa") << ':'
				<< (aRun.Appearance.InitialStyleClass == "QMacStyle") << ':'
				<< (hints != nullptr
					&& hints->colorScheme() == Qt::ColorScheme::Light)
				<< ':'
				<< (aRun.Appearance.InitialGlobalColorScheme
					== Qt::ColorScheme::Light)
				<< ':'
				<< lightPalette << ':' << thread.isValid() << ':'
				<< leftEpoch.isValid() << ':' << rightEpoch.isValid() << ':'
				<< (!aRun.Appearance.ExpectedEpochLeftOid.isEmpty()) << ':'
				<< (!aRun.Appearance.ExpectedEpochRightOid.isEmpty()) << ':'
				<< (control->left_oid_editor()->text()
					== aRun.Appearance.ExpectedEpochLeftOid)
				<< ':'
				<< (control->right_oid_editor()->text()
					== aRun.Appearance.ExpectedEpochRightOid)
				<< ':'
				<< control->whole_pr_action()->isChecked() << ':'
				<< aRun.RenderPath.has_value() << ':'
				<< (!aRun.Appearance.EvidencePaths.Initial.isEmpty()) << ':'
				<< (!aRun.Appearance.EvidencePaths.Changed.isEmpty()) << '\n';
			return false;
		}

		aRun.Appearance.Thread                        = QPersistentModelIndex{ thread };
		aRun.Appearance.InitialModelResets            = model->statistics().ModelResets;
		aRun.Appearance.EpochLeft                     = QPersistentModelIndex{ leftEpoch };
		aRun.Appearance.EpochRight                    = QPersistentModelIndex{ rightEpoch };
		aRun.Appearance.EpochLeftIdentity             = leftEpoch.data(
																	 bpr::qt::epoch_history_model::identity_role)
															.toString();
		aRun.Appearance.EpochRightIdentity            = rightEpoch.data(
																	  bpr::qt::epoch_history_model::identity_role)
															.toString();
		aRun.Appearance.InitialEpochModelResets       = epochModel->statistics().ModelResets;
		aRun.Appearance.InitialEpochPalette           = control->palette();
		aRun.Appearance.InitialEpochAppearanceUpdates = control->statistics().AppearanceUpdates;
		aRun.Appearance.InitialDiffSelection          = workbench->selection();
		aRun.Appearance.InitialDiffPalette            = workbench->palette();
		aRun.Appearance.ObservedDiffPalette           = workbench->palette();
		aRun.Appearance.InitialDiffFont               = workbench->font();
		aRun.Appearance.ObservedDiffFont              = workbench->font();
		view->setCurrentIndex(thread);
		view->selectionModel()->select(
			thread,
			QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
		view->collapse(thread);
		aWindow.resize(1'280, 1'000);
		workbench->left_pane()->setFocus(Qt::OtherFocusReason);
		if (auto* const scroll = qobject_cast<QScrollArea*>(aWindow.centralWidget())) {
			scroll->ensureWidgetVisible(workbench, 24, 24);
		}
		settle_automated_events();
		aRun.Appearance.InitialDiffGeometryRebuilds
			= workbench->statistics().GeometryRebuilds;
		aRun.Appearance.InitialWindowActive = aWindow.isActiveWindow();
		aRun.Appearance.InitialFocus        = QApplication::focusWidget();
		if (!native_appearance_light_state_passed(aWindow, aRun)) {
			const auto* const focus = QApplication::focusWidget();
			QTextStream{ stdout }
				<< "NATIVE_APPEARANCE_EPOCH_CONTEXT:"
				<< (QApplication::focusWidget()
					== aRun.Appearance.InitialFocus)
				<< ':'
				<< (control->palette()
					== aRun.Appearance.InitialEpochPalette)
				<< ':'
				<< native_palette_readable(control->palette()) << ':'
				<< (control->statistics().AppearanceUpdates
					== aRun.Appearance.InitialEpochAppearanceUpdates)
				<< ':'
				<< native_appearance_context_passed(aWindow, aRun) << '\n'
				<< "NATIVE_APPEARANCE_FOCUS:"
				<< aWindow.isActiveWindow() << ':' << view->isVisible() << ':'
				<< control->isVisible() << ':'
				<< control->right_oid_editor()->isVisible() << ':'
				<< (focus != nullptr
						? QString::fromLatin1(
							  focus->metaObject()->className())
						: QStringLiteral("none"))
				<< ':'
				<< (focus != nullptr
						? focus->objectName()
						: QStringLiteral("none"))
				<< ':' << static_cast<int>(view->focusPolicy()) << '\n';
			return false;
		}

		auto initialRender = capture_normalized_render(aWindow);
		if (
			initialRender.isNull()
			|| !native_appearance_light_state_passed(aWindow, aRun)) {
			return false;
		}
		aRun.Appearance.InitialRender = std::move(initialRender);

		QTextStream stream{ stdout };
		stream << "NATIVE_APPEARANCE_READY:"
			   << color_scheme_name(aRun.Appearance.InitialColorScheme) << '\n'
			   << "NATIVE_APPEARANCE_GLOBAL_READY:"
			   << color_scheme_name(
					  aRun.Appearance.InitialGlobalColorScheme)
			   << '\n';
		stream.flush();
		return true;
	}

	template <typename Identity>
	[[nodiscard]] auto diff_evidence_identity_hex(
		const Identity& aIdentity) -> QString
	{
		static constexpr auto digits  = std::string_view{ "0123456789abcdef" };
		auto                  encoded = QByteArray{ };
		for (const auto byte : aIdentity.bytes()) {
			encoded.append(digits[byte >> 4U]);
			encoded.append(digits[byte & 0x0FU]);
		}
		return QString::fromLatin1(encoded);
	}

	template <typename Identity>
	[[nodiscard]] auto diff_evidence_json_identity(
		const std::optional<Identity>& aIdentity) -> QString
	{
		return aIdentity.has_value()
			? QStringLiteral("\"") + diff_evidence_identity_hex(*aIdentity)
				+ QStringLiteral("\"")
			: QStringLiteral("null");
	}

	[[nodiscard]] auto diff_evidence_json_string(QString aValue) -> QString
	{
		aValue.replace(u'\\', QStringLiteral("\\\\"));
		aValue.replace(u'\"', QStringLiteral("\\\""));
		aValue.replace(u'\n', QStringLiteral("\\n"));
		aValue.replace(u'\r', QStringLiteral("\\r"));
		return QStringLiteral("\"") + aValue + QStringLiteral("\"");
	}

	[[nodiscard]] auto send_diff_evidence_wheel(
		QWidget&      aSurface,
		const QPoint& aPixel) -> bool
	{
		auto event = QWheelEvent{
			QPointF{ 8.0, 8.0 },
			QPointF{ 8.0, 8.0 },
			aPixel,
			{ },
			Qt::NoButton,
			Qt::NoModifier,
			Qt::NoScrollPhase,
			false,
		};
		event.ignore();
		QCoreApplication::sendEvent(&aSurface, &event);
		return event.isAccepted();
	}

	void send_diff_evidence_click(
		QWidget&       aSurface,
		const QPointF& aPosition)
	{
		auto press = QMouseEvent{
			QEvent::MouseButtonPress,
			aPosition,
			aPosition,
			aPosition,
			Qt::LeftButton,
			Qt::LeftButton,
			Qt::NoModifier,
		};
		auto release = QMouseEvent{
			QEvent::MouseButtonRelease,
			aPosition,
			aPosition,
			aPosition,
			Qt::LeftButton,
			Qt::NoButton,
			Qt::NoModifier,
		};
		QCoreApplication::sendEvent(&aSurface, &press);
		QCoreApplication::sendEvent(&aSurface, &release);
	}

	[[nodiscard]] auto notification_permission_name(
		const bpr::application::notification_permission aPermission) -> QString
	{
		switch (aPermission) {
		case bpr::application::notification_permission::not_determined:
			return QStringLiteral("not-determined");
		case bpr::application::notification_permission::granted:
			return QStringLiteral("granted");
		case bpr::application::notification_permission::refused:
			return QStringLiteral("refused");
		}
		return QStringLiteral("unknown");
	}

	[[nodiscard]] auto capture_diff_interaction_evidence(
		bpr::qt::github_window&                aWindow,
		const std::optional<std::string_view>& aRawDirectory) -> bool
	{
		if (!aRawDirectory.has_value() || aRawDirectory->empty()) {
			return false;
		}
		const auto directoryPath = QString::fromUtf8(
			aRawDirectory->data(), static_cast<qsizetype>(aRawDirectory->size()));
		if (!QDir{ }.mkpath(directoryPath)) {
			return false;
		}
		const auto directory = QDir{ directoryPath };
		auto       trace     = QFile{
			directory.filePath(QStringLiteral("interaction-trace.jsonl"))
		};
		if (!trace.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
			return false;
		}

		auto* const workbench = aWindow.diff_comparison_workbench();
		if (workbench == nullptr || workbench->current_result() == nullptr
			|| !workbench->current_context().has_value()
			|| !workbench->selection().has_value()) {
			return false;
		}
		// Records the appearance the frames are taken under, next to the
		// frames. The end-of-run APPEARANCE_APPLIED marker cannot serve: the
		// appearance-gesture assertion deliberately dispatches an explicit
		// dark mode later in the same process to prove a gesture reaches
		// presentation, so the marker reports a mode adopted after every
		// raster here was written. BPR-0201 spent an investigation on that
		// gap, reading a light capture beside a dark marker as a composition
		// defect when the two were describing different moments.
		{
			auto stamp = QFile{
				directory.filePath(QStringLiteral("appearance-at-capture.json"))
			};
			if (!stamp.open(
					QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
				return false;
			}
			auto out = QTextStream{ &stamp };
			out << "{\"base\":\""
				<< aWindow.palette().color(QPalette::Base).name()
				<< "\",\"highlight\":\""
				<< aWindow.palette().color(QPalette::Highlight).name()
				<< "\",\"window_text\":\""
				<< aWindow.palette().color(QPalette::WindowText).name()
				<< "\"}\n";
			out.flush();
			if (stamp.error() != QFileDevice::NoError) {
				return false;
			}

			// And the stamp is held to the pixels, so it cannot drift into
			// describing an appearance the panes are not painting. Sampling
			// the commonest colour rather than one point keeps a band or a
			// glyph from deciding the answer.
			auto* const pane = workbench->left_pane();
			if (pane == nullptr) {
				return false;
			}
			const auto raster = pane->viewport()->grab().toImage();
			auto       tally  = std::map<QRgb, int>{ };
			for (auto y = 0; y < raster.height(); ++y) {
				for (auto x = 0; x < raster.width(); ++x) {
					++tally[raster.pixel(x, y)];
				}
			}
			if (tally.empty()) {
				return false;
			}
			const auto commonest = std::ranges::max_element(
				tally, { }, [](const auto& aEntry) { return aEntry.second; });
			if (QColor{ commonest->first }
				!= aWindow.palette().color(QPalette::Base)) {
				return false;
			}
		}

		auto* const controller = workbench->scroll_controller();
		const auto& alignment  = workbench->current_result()->Alignment;
		const auto  findFirst  = [&alignment](const bpr::core::alignment_kind aKind)
			-> const bpr::core::alignment_segment* {
			const auto found = std::ranges::find_if(
				alignment.Segments,
				[aKind](const auto& aSegment) { return aSegment.Kind == aKind; });
			return found != alignment.Segments.end() ? &*found : nullptr;
		};
		const auto findLast = [&alignment](const bpr::core::alignment_kind aKind)
			-> const bpr::core::alignment_segment* {
			const auto found = std::ranges::find_if(
				alignment.Segments.rbegin(),
				alignment.Segments.rend(),
				[aKind](const auto& aSegment) { return aSegment.Kind == aKind; });
			return found != alignment.Segments.rend() ? &*found : nullptr;
		};
		const auto* const matching    = findLast(bpr::core::alignment_kind::equal);
		const auto* const replacement = findFirst(
			bpr::core::alignment_kind::replacement);
		const auto* const deletion  = findFirst(bpr::core::alignment_kind::left_only);
		const auto* const insertion = findFirst(bpr::core::alignment_kind::right_only);
		if (matching == nullptr || replacement == nullptr || deletion == nullptr
			|| insertion == nullptr) {
			return false;
		}

		const auto initialContext = *workbench->current_context();
		const auto initialStats   = workbench->statistics();
		const auto capture        = [&](const std::string_view aStep) {
			settle_automated_events();
			const auto& retained  = controller->current_frame();
			const auto& rendered  = workbench->link_map()->current_frame();
			const auto& selection = workbench->selection();
			if (!retained.has_value() || !rendered.has_value()
				|| !workbench->current_context().has_value()
				|| !selection.has_value()) {
				return false;
			}
			const auto step = QString::fromLatin1(
				aStep.data(), static_cast<qsizetype>(aStep.size()));
			const auto imageName = step + QStringLiteral(".png");
			if (!aWindow.grab().save(directory.filePath(imageName), "PNG")) {
				return false;
			}
			const auto& frame     = *retained;
			const auto  stats     = workbench->statistics();
			const auto  linkStats = workbench->link_map()->statistics();
			auto        stream    = QTextStream{ &trace };
			stream << "{\"step\":" << diff_evidence_json_string(step) << ','
				   << "\"render_file\":" << diff_evidence_json_string(imageName)
				   << ",\"qt_platform\":"
				   << diff_evidence_json_string(QGuiApplication::platformName())
				   << ",\"qt_style\":"
				   << diff_evidence_json_string(QString::fromLatin1(
						  QApplication::style()->metaObject()->className()))
				   << ",\"device_pixel_ratio\":"
				   << QString::number(aWindow.devicePixelRatioF(), 'f', 6)
				   << ",\"font_family\":"
				   << diff_evidence_json_string(aWindow.font().family())
				   << ",\"font_point_size\":"
				   << QString::number(aWindow.font().pointSizeF(), 'f', 6)
				   << ",\"window_width\":" << aWindow.width()
				   << ",\"window_height\":" << aWindow.height()
				   << ",\"comparison_position\":"
				   << QString::number(frame.Position, 'f', 6)
				   << ",\"active_segment_id\":"
				   << diff_evidence_json_identity(frame.Segment)
				   << ",\"left_projected_document_y\":"
				   << QString::number(frame.Left.ProjectedDocumentY, 'f', 6)
				   << ",\"right_projected_document_y\":"
				   << QString::number(frame.Right.ProjectedDocumentY, 'f', 6)
				   << ",\"left_requested_origin_y\":"
				   << QString::number(frame.Left.DocumentOriginY, 'f', 6)
				   << ",\"right_requested_origin_y\":"
				   << QString::number(frame.Right.DocumentOriginY, 'f', 6)
				   << ",\"left_applied_origin_y\":"
				   << QString::number(rendered->Left.DocumentOriginY, 'f', 6)
				   << ",\"right_applied_origin_y\":"
				   << QString::number(rendered->Right.DocumentOriginY, 'f', 6)
				   << ",\"left_leading_clamped\":"
				   << (frame.Left.LeadingClamped ? "true" : "false")
				   << ",\"left_trailing_clamped\":"
				   << (frame.Left.TrailingClamped ? "true" : "false")
				   << ",\"right_leading_clamped\":"
				   << (frame.Right.LeadingClamped ? "true" : "false")
				   << ",\"right_trailing_clamped\":"
				   << (frame.Right.TrailingClamped ? "true" : "false")
				   << ",\"selected_group_id\":"
				   << diff_evidence_json_identity(selection->Group)
				   << ",\"selected_segment_id\":"
				   << diff_evidence_json_identity(selection->Segment)
				   << ",\"visible_ribbon_count\":"
				   << static_cast<qulonglong>(workbench->link_map()->visible_links().size())
				   << ",\"link_map_width\":" << workbench->link_map()->width()
				   << ",\"link_map_height\":" << workbench->link_map()->height()
				   << ",\"link_map_visible\":"
				   << (workbench->link_map()->isVisible() ? "true" : "false")
				   << ",\"link_map_retained_changes\":"
				   << static_cast<qulonglong>(linkStats.RetainedChangedEntries)
				   << ",\"link_map_candidate_entries\":"
				   << static_cast<qulonglong>(linkStats.LastCandidateEntries)
				   << ",\"link_map_frame_updates\":" << linkStats.FrameUpdates
				   << ",\"link_map_selection_callbacks\":"
				   << linkStats.SelectionCallbacks
				   << ",\"link_map_rejected_frames\":"
				   << linkStats.RejectedStaleFrames
				   << ",\"geometry_generation\":" << stats.GeometryRebuilds
				   << ",\"semantic_diff_generation\":"
				   << workbench->current_context()->Generation.value()
				   << ",\"semantic_index_builds\":" << stats.SemanticIndexBuilds
				   << "}\n";
			stream.flush();
			return trace.error() == QFileDevice::NoError;
		};

		if (!capture("01-selected-composition")
			|| !controller->reveal_segment(matching->Id, 0.2)) {
			return false;
		}
		const auto matchingEarly = *controller->current_frame();
		if (!capture("02-matching-early")
			|| !controller->reveal_segment(matching->Id, 0.8)) {
			return false;
		}
		const auto matchingLate = *controller->current_frame();
		if (matchingLate.Left.ProjectedDocumentY
				<= matchingEarly.Left.ProjectedDocumentY
			|| matchingLate.Right.ProjectedDocumentY
				<= matchingEarly.Right.ProjectedDocumentY
			|| !capture("03-matching-late")) {
			return false;
		}

		const auto inputOrigin = controller->maximum_position() * 0.45;
		controller->set_position(inputOrigin);
		if (!send_diff_evidence_wheel(
				*workbench->left_pane()->viewport(), QPoint{ 0, -32 })) {
			return false;
		}
		const auto leftInputPosition = controller->position();
		// The receiver-equivalence steps below compare the right pane and the
		// divider against this value. Require the wheel to have actually moved
		// the retained position first, otherwise all three receivers would
		// agree trivially at the origin even if none of them handled the event.
		if (leftInputPosition <= inputOrigin || !capture("04-left-pane-input")) {
			return false;
		}

		if (!controller->reveal_segment(insertion->Id, 0.2)) {
			return false;
		}
		const auto insertionEarly = *controller->current_frame();
		if (!capture("05-insertion-early")
			|| !controller->reveal_segment(insertion->Id, 0.8)) {
			return false;
		}
		const auto insertionLate = *controller->current_frame();
		if (std::abs(insertionLate.Left.ProjectedDocumentY
					 - insertionEarly.Left.ProjectedDocumentY)
				> 0.000'001
			|| insertionLate.Right.ProjectedDocumentY
				<= insertionEarly.Right.ProjectedDocumentY
			|| !capture("06-insertion-late")) {
			return false;
		}
		const auto insertionIterator = std::ranges::find(
			alignment.Segments,
			insertion->Id,
			[](const auto& aSegment) { return aSegment.Id; });
		if (insertionIterator == alignment.Segments.end()) {
			return false;
		}
		const auto continuation = std::ranges::find_if(
			insertionIterator + 1,
			alignment.Segments.end(),
			[](const auto& aSegment) {
				return aSegment.Kind == bpr::core::alignment_kind::equal;
			});
		if (continuation == alignment.Segments.end()
			|| !controller->reveal_segment(continuation->Id, 0.25)
			|| !capture("07-matching-continuation")
			|| !controller->reveal_segment(insertion->Id, 0.8)
			|| !controller->reveal_segment(insertion->Id, 0.2)) {
			return false;
		}
		const auto insertionReversed = *controller->current_frame();
		if (std::abs(insertionReversed.Left.ProjectedDocumentY
					 - insertionEarly.Left.ProjectedDocumentY)
				> 0.000'001
			|| std::abs(insertionReversed.Right.ProjectedDocumentY
						- insertionEarly.Right.ProjectedDocumentY)
				> 0.000'001
			|| !capture("08-insertion-reversed")) {
			return false;
		}

		controller->set_position(inputOrigin);
		if (!send_diff_evidence_wheel(
				*workbench->right_pane()->viewport(), QPoint{ 0, -32 })) {
			return false;
		}
		const auto rightInputPosition = controller->position();
		if (std::abs(rightInputPosition - leftInputPosition) > 0.000'001
			|| rightInputPosition <= inputOrigin
			|| !capture("09-right-pane-input")) {
			return false;
		}
		controller->set_position(inputOrigin);
		if (std::abs(controller->position() - inputOrigin) > 0.000'001
			|| !send_diff_evidence_wheel(*workbench->link_map(), QPoint{ 0, -32 })) {
			return false;
		}
		const auto dividerInputPosition = controller->position();
		if (std::abs(dividerInputPosition - leftInputPosition) > 0.000'001
			|| dividerInputPosition <= inputOrigin
			|| !capture("10-divider-input")) {
			return false;
		}

		if (!controller->reveal_segment(deletion->Id, 0.2)) {
			return false;
		}
		const auto deletionEarly = *controller->current_frame();
		if (!capture("11-deletion-early")
			|| !controller->reveal_segment(deletion->Id, 0.8)) {
			return false;
		}
		const auto deletionLate = *controller->current_frame();
		if (deletionLate.Left.ProjectedDocumentY
				<= deletionEarly.Left.ProjectedDocumentY
			|| std::abs(deletionLate.Right.ProjectedDocumentY
						- deletionEarly.Right.ProjectedDocumentY)
				> 0.000'001
			|| !capture("12-deletion-late")) {
			return false;
		}

		if (!controller->reveal_segment(replacement->Id, 0.2)) {
			return false;
		}
		const auto replacementEarly = *controller->current_frame();
		if (!capture("13-replacement-early")
			|| !controller->reveal_segment(replacement->Id, 0.8)) {
			return false;
		}
		const auto replacementLate = *controller->current_frame();
		const auto leftAdvance     = replacementLate.Left.ProjectedDocumentY
			- replacementEarly.Left.ProjectedDocumentY;
		const auto rightAdvance = replacementLate.Right.ProjectedDocumentY
			- replacementEarly.Right.ProjectedDocumentY;
		if (leftAdvance <= 0.0 || rightAdvance <= leftAdvance
			|| !capture("14-replacement-late")) {
			return false;
		}

		if (!controller->reveal_segment(replacement->Id, 0.5)) {
			return false;
		}
		settle_automated_events();
		const auto visible  = workbench->link_map()->visible_links();
		const auto ribbonIt = std::ranges::find(
			visible,
			replacement->Id,
			[](const bpr::qt::visible_diff_link& aLink) {
				return aLink.Identity.Segment;
			});
		if (ribbonIt == visible.end() || ribbonIt->Path.isEmpty()) {
			return false;
		}
		const auto& ribbon = *ribbonIt;

		// The replacement is the fixture's first group and the workbench
		// selects it when it publishes, so clicking it from that state would
		// pass even if selection replacement were broken. Move the selection to
		// a different visible group first and require the click to replace it.
		const auto otherIt = std::ranges::find_if(
			visible, [&ribbon](const bpr::qt::visible_diff_link& aLink) {
				return aLink.Identity.Group != ribbon.Identity.Group
					&& !aLink.Path.isEmpty();
			});
		if (otherIt == visible.end()) {
			return false;
		}
		const auto& otherLink  = *otherIt;
		const auto  otherPoint = otherLink.Path.boundingRect().center();
		if (!otherLink.Path.contains(otherPoint)) {
			return false;
		}
		send_diff_evidence_click(*workbench->link_map(), otherPoint);
		settle_automated_events();
		if (!workbench->selection().has_value()
			|| workbench->selection()->Group != otherLink.Identity.Group
			|| workbench->selection()->Group == ribbon.Identity.Group
			|| !capture("15-other-ribbon-selected")) {
			return false;
		}

		const auto point = ribbon.Path.boundingRect().center();
		if (!ribbon.Path.contains(point)) {
			return false;
		}
		const auto callbacksBeforeClick
			= workbench->link_map()->statistics().SelectionCallbacks;
		send_diff_evidence_click(*workbench->link_map(), point);
		settle_automated_events();
		if (!workbench->selection().has_value()
			|| workbench->selection()->Group != ribbon.Identity.Group
			|| !workbench->link_map()->selected().has_value()
			|| workbench->link_map()->selected()->Group
				!= workbench->selection()->Group
			|| workbench->link_map()->statistics().SelectionCallbacks
				!= callbacksBeforeClick + 1U
			|| !workbench->detail_pane()->toPlainText().startsWith(
				QStringLiteral("Change "))
			|| !capture("16-ribbon-selection")) {
			return false;
		}

		auto* const vertical = workbench->findChild<QSplitter*>(
			QStringLiteral("diffWorkbenchVerticalSplitter"));
		if (vertical == nullptr) {
			return false;
		}
		const auto selectionBeforeResize = workbench->selection();
		const auto rebuildsBeforeResize  = workbench->statistics().GeometryRebuilds;
		const auto sizes                 = vertical->sizes();
		const auto total                 = std::accumulate(sizes.begin(), sizes.end(), 0);
		const auto smallerTop            = std::max(140, total / 3);
		vertical->setSizes({ smallerTop, total - smallerTop });
		settle_automated_events();
		if (workbench->statistics().GeometryRebuilds != rebuildsBeforeResize + 1U
			|| workbench->selection() != selectionBeforeResize
			|| !capture("17-divider-resized")) {
			return false;
		}

		controller->home();
		if (std::abs(controller->position() - controller->minimum_position())
				> 0.000'001
			|| !capture("18-comparison-start")) {
			return false;
		}
		controller->end();
		if (std::abs(controller->position() - controller->maximum_position())
				> 0.000'001
			|| !capture("19-comparison-end")) {
			return false;
		}

		const auto finalStats = workbench->statistics();
		return workbench->current_context() == initialContext
			&& finalStats.SemanticIndexBuilds == initialStats.SemanticIndexBuilds
			&& workbench->shared_scrollbar()->isEnabled()
			&& workbench->left_pane()->verticalScrollBarPolicy()
			== Qt::ScrollBarAlwaysOff
			&& workbench->right_pane()->verticalScrollBarPolicy()
			== Qt::ScrollBarAlwaysOff
			&& trace.error() == QFileDevice::NoError;
	}
#endif

	[[nodiscard]] constexpr auto native_platform_vault_expected() noexcept
		-> bool
	{
#if defined(__APPLE__) || defined(_WIN32) || defined(__linux__)
		return true;
#else
		return false;
#endif
	}

#if defined(BPR_BUILD_TESTING)
	[[nodiscard]] auto fixture_presentation_passed(
		bpr::qt::github_window& aWindow,
		const std::string_view  aExpectedActiveHostText) -> bool
	{
		const auto* const title = aWindow.findChild<QLabel*>(QStringLiteral("pullRequestTitle"));
		return aWindow.status_text() == "Pull request ready."
			&& aWindow.active_account_text() == "Octo Reviewer (@octo-reviewer)"
			&& aWindow.active_host_text() == aExpectedActiveHostText
			&& aWindow.open_action()->isEnabled()
			&& aWindow.refresh_action()->isEnabled()
			&& title != nullptr
			&& title->text()
			== QStringLiteral("Keep review snapshots deterministic")
			&& aWindow.conversation_model()->rowCount() == 3
			&& aWindow.conversation_model()
				   ->index_for_id("review-thread:PRRT_fixture_1")
				   .isValid()
			&& aWindow.statistics().FullResets == 0U;
	}

	[[nodiscard]] auto fixture_epoch_presentation_passed(
		const bpr::qt::github_window& aWindow,
		const std::size_t             aMinimumEpochs = 1U) -> bool
	{
		const auto* const control = aWindow.epoch_control();
		return control->status_text().startsWith(
				   QStringLiteral("Review history is ready."))
			&& control->comparison_text().endsWith(
				QStringLiteral("Ready for diff."))
			&& control->history_model()->epoch_count() >= aMinimumEpochs
			&& !control->left_oid_editor()->text().isEmpty()
			&& !control->right_oid_editor()->text().isEmpty();
	}

	[[nodiscard]] auto fixture_epoch_pair_passed(
		const bpr::qt::github_window& aWindow,
		const std::string&            aLeft,
		const std::string&            aRight,
		const std::size_t             aMinimumEpochs = 2U) -> bool
	{
		const auto* const control = aWindow.epoch_control();
		return fixture_epoch_presentation_passed(aWindow, aMinimumEpochs)
			&& control->left_oid_editor()->text()
			== QString::fromStdString(aLeft)
			&& control->right_oid_editor()->text()
			== QString::fromStdString(aRight);
	}

	[[nodiscard]] auto fixture_anchor_row(
		const bpr::qt::epoch_history_model&     aModel,
		const std::uint64_t                     aOrdinal,
		const bpr::core::comparison_anchor_kind aKind) -> int
	{
		for (auto row = 0; row < aModel.rowCount(); ++row) {
			const auto index = aModel.index(row, 0);
			if (index.data(bpr::qt::epoch_history_model::epoch_ordinal_role)
						.toULongLong()
					== aOrdinal
				&& index.data(bpr::qt::epoch_history_model::anchor_kind_role)
						.toInt()
					== static_cast<int>(aKind)) {
				return row;
			}
		}
		return -1;
	}

	[[nodiscard]] auto fixture_epoch_cause_passed(
		const bpr::qt::epoch_history_model&  aModel,
		const std::uint64_t                  aOrdinal,
		const bpr::core::epoch_primary_cause aExpected) -> bool
	{
		const auto row = fixture_anchor_row(
			aModel,
			aOrdinal,
			bpr::core::comparison_anchor_kind::epoch_head);
		return row >= 0
			&& aModel.index(row, 0)
				   .data(bpr::qt::epoch_history_model::primary_cause_role)
				   .toInt()
			== static_cast<int>(aExpected);
	}

	[[nodiscard]] auto fixture_epoch_id(
		const bpr::qt::epoch_history_model& aModel,
		const std::uint64_t                 aOrdinal) -> std::optional<bpr::core::review_epoch_id>
	{
		const auto row = fixture_anchor_row(
			aModel, aOrdinal, bpr::core::comparison_anchor_kind::epoch_base);
		if (row < 0) {
			return std::nullopt;
		}
		const auto anchor = aModel.anchor_at(aModel.index(row, 0));
		if (!anchor) {
			return std::nullopt;
		}
		return bpr::core::review_epoch_id{
			.Authority   = anchor->Identity.Authority,
			.Incarnation = anchor->Identity.Incarnation,
			.Ordinal     = anchor->Identity.Epoch,
		};
	}

	[[nodiscard]] auto fixture_epoch_availability_is(
		const bpr::qt::epoch_history_model&  aModel,
		const std::uint64_t                  aOrdinal,
		const bpr::core::anchor_availability aExpected) -> bool
	{
		const auto base = fixture_anchor_row(
			aModel, aOrdinal, bpr::core::comparison_anchor_kind::epoch_base);
		const auto head = fixture_anchor_row(
			aModel, aOrdinal, bpr::core::comparison_anchor_kind::epoch_head);
		return base >= 0 && head >= 0
			&& aModel.index(base, 0)
				   .data(bpr::qt::epoch_history_model::availability_role)
				   .toInt()
			== static_cast<int>(aExpected)
			&& aModel.index(head, 0)
				   .data(bpr::qt::epoch_history_model::availability_role)
				   .toInt()
			== static_cast<int>(aExpected);
	}

	[[nodiscard]] auto fixture_all_epoch_availability_is(
		const bpr::qt::epoch_history_model&  aModel,
		const bpr::core::anchor_availability aExpected) -> bool
	{
		if (aModel.rowCount() == 0) {
			return false;
		}
		for (auto row = 0; row < aModel.rowCount(); ++row) {
			if (aModel.index(row, 0)
					.data(bpr::qt::epoch_history_model::availability_role)
					.toInt()
				!= static_cast<int>(aExpected)) {
				return false;
			}
		}
		return true;
	}

	void accept_maintenance_confirmation(const QString& aObjectName)
	{
		auto attempts = std::make_shared<std::size_t>(0U);
		auto poll     = std::make_shared<std::function<void()>>();
		*poll         = [aObjectName, attempts, poll] {
			auto* const dialog = qobject_cast<QMessageBox*>(
				QApplication::activeModalWidget());
			if (dialog != nullptr && dialog->objectName() == aObjectName) {
				auto* const confirm = dialog->findChild<QPushButton*>(
					aObjectName + QStringLiteral("ConfirmButton"));
				if (confirm != nullptr) {
					confirm->click();
					return;
				}
			}
			if (++*attempts <= 100U) {
				QTimer::singleShot(1, qApp, *poll);
			}
		};
		QTimer::singleShot(0, qApp, *poll);
	}

	void advance_fixture_stage(
		automated_run& aRun, automated_run::stage aNext) noexcept;

	[[nodiscard]] auto begin_epoch_maintenance_probe(
		bpr::app::github_application& aApplication,
		automated_run&                aRun) -> bool
	{
		auto* const control = aApplication.window().epoch_control();
		auto* const model   = control->history_model();
		if (aRun.MaintenanceProbe
			== automated_run::epoch_maintenance_probe::verify_deleted) {
			const auto provider = bpr::app::detail::
				github_application_fixture_access::evidence(aApplication);
			const auto maintenance = bpr::app::detail::
				github_application_fixture_access::maintenance_evidence(
					aApplication, { });
			return provider.SmartAcquireCount == 1
				&& provider.SmartDiscoveryCount == 1
				&& provider.SmartLsRefsCount == 1
				&& provider.SmartFetchCount == 1
				&& !maintenance.RetiredRepositoryPresent
				&& maintenance.RetiredCacheDeletionMarkerPresent
				&& maintenance.PruneIntentDispatchCount == 0U
				&& maintenance.DeleteIntentDispatchCount == 0U;
		}
		if (model->epoch_count() < 2U) {
			if (control->return_to_newest_action()->isEnabled()) {
				control->return_to_newest_action()->trigger();
			} else if (control->load_history_action()->isEnabled()) {
				control->load_history_action()->trigger();
			} else {
				return false;
			}
			advance_fixture_stage(
				aRun, automated_run::stage::epoch_maintenance_history);
			return true;
		}

		const auto target = fixture_epoch_id(*model, 1U);
		if (!target) {
			return false;
		}
		aRun.MaintenanceTarget = *target;
		const auto provider    = bpr::app::detail::
			github_application_fixture_access::evidence(aApplication);
		const auto maintenance = bpr::app::detail::
			github_application_fixture_access::maintenance_evidence(
				aApplication, *target);

		switch (aRun.MaintenanceProbe) {
		case automated_run::epoch_maintenance_probe::prune:
			aRun.MaintenanceLeftRow = fixture_anchor_row(
				*model, 1U, bpr::core::comparison_anchor_kind::epoch_base);
			aRun.MaintenanceRightRow = fixture_anchor_row(
				*model, 1U, bpr::core::comparison_anchor_kind::epoch_head);
			if (aRun.MaintenanceLeftRow < 0 || aRun.MaintenanceRightRow < 0
				|| !maintenance.TargetBaseRefPresent
				|| !maintenance.TargetHeadRefPresent
				|| !maintenance.NonTargetBaseRefPresent
				|| !maintenance.NonTargetHeadRefPresent
				|| !maintenance.RepositoryPresent
				|| maintenance.CacheDeletionMarkerPresent) {
				return false;
			}
			aRun.MaintenanceAccountReads  = provider.AccountReadCount;
			aRun.MaintenanceSmartAcquires = provider.SmartAcquireCount;
			aRun.MaintenanceModelResets   = model->statistics().ModelResets;
			control->left_selector()->setCurrentIndex(aRun.MaintenanceLeftRow);
			advance_fixture_stage(
				aRun, automated_run::stage::epoch_maintenance_select_right);
			return true;

		case automated_run::epoch_maintenance_probe::verify_pruned_and_delete:
			if (!fixture_epoch_availability_is(*model,
											   1U,
											   bpr::core::anchor_availability::pruned)
				|| maintenance.TargetBaseRefPresent
				|| maintenance.TargetHeadRefPresent
				|| !maintenance.NonTargetBaseRefPresent
				|| !maintenance.NonTargetHeadRefPresent
				|| !maintenance.RepositoryPresent
				|| maintenance.CacheDeletionMarkerPresent
				|| maintenance.PruneIntentDispatchCount != 0U
				|| maintenance.DeleteIntentDispatchCount != 0U
				|| !control->delete_cache_action()->isEnabled()) {
				return false;
			}
			if (!aRun.DiffRequested) {
				aRun.DiffRequested = bpr::app::detail::
					github_application_fixture_access::request_fixture_diff(
						aApplication, true);
				return aRun.DiffRequested;
			}
			if (aApplication.window().diff_comparison_workbench()->current_result()
				== nullptr) {
				return true;
			}
			aRun.MaintenanceAccountReads  = provider.AccountReadCount;
			aRun.MaintenanceSmartAcquires = provider.SmartAcquireCount;
			aRun.MaintenanceModelResets   = model->statistics().ModelResets;
			accept_maintenance_confirmation(
				QStringLiteral("deleteRepositoryCacheConfirmationDialog"));
			control->delete_cache_action()->trigger();
			advance_fixture_stage(
				aRun, automated_run::stage::epoch_maintenance_delete_result);
			return true;

		case automated_run::epoch_maintenance_probe::verify_deleted:
		case automated_run::epoch_maintenance_probe::none:
			break;
		}
		return false;
	}

	[[nodiscard]] constexpr auto expected_fixture_smart_calls(
		const bpr::app::detail::github_epoch_fixture_phase aPhase) noexcept
		-> int
	{
		switch (aPhase) {
		case bpr::app::detail::github_epoch_fixture_phase::single:
			return 1U;
		case bpr::app::detail::github_epoch_fixture_phase::seed:
			return 2U;
		case bpr::app::detail::github_epoch_fixture_phase::reuse:
			return 0U;
		}
		return 0U;
	}

	void advance_fixture_stage(
		automated_run& aRun, const automated_run::stage aNext) noexcept
	{
		aRun.Stage    = aNext;
		aRun.Attempts = 0U;
	}
#endif

	[[nodiscard]] auto cli_fail_closed_presentation_passed(
		const bpr::qt::github_window& aWindow,
		const automated_run&          aRun) -> bool
	{
		const auto  statistics = aWindow.statistics();
		const auto& cli        = aRun.Cli;
		return aWindow.profile_selector()->currentIndex()
			== cli.ActiveRow
			&& aWindow.active_host_text() == cli.ExpectedActiveHostText
			&& statistics.IdentityUpdates
			== cli.IdentityUpdatesBeforeConnect + 1U
			&& statistics.DiagnosticUpdates
			== cli.DiagnosticUpdatesBeforeConnect + 1U
			&& aWindow.status_text() == "The GitHub connection failed."
			&& aWindow.diagnostic_text()
			== "The selected GitHub CLI connection is unavailable."
			&& aWindow.active_account_text() == "Not authenticated"
			&& !aWindow.open_action()->isEnabled()
			&& statistics.FullResets == 0U;
	}

	[[nodiscard]] auto is_cli_unavailable_profile(
		const QComboBox& aSelector,
		const int        aRow) -> bool
	{
		return aSelector.itemText(aRow).endsWith(
			QStringLiteral("GitHub CLI (unavailable)"));
	}

	[[nodiscard]] auto cli_unavailable_profile_count(
		const QComboBox& aSelector) -> std::size_t
	{
		auto count = std::size_t{ 0U };
		for (auto row = 0; row < aSelector.count(); ++row) {
			if (is_cli_unavailable_profile(aSelector, row)) {
				++count;
			}
		}
		return count;
	}

	[[nodiscard]] auto next_cli_unavailable_profile(
		const QComboBox& aSelector,
		const int        aAfterRow) -> int
	{
		for (auto row = aAfterRow + 1; row < aSelector.count(); ++row) {
			if (is_cli_unavailable_profile(aSelector, row)) {
				return row;
			}
		}
		return -1;
	}

	[[nodiscard]] auto expected_cli_active_host_text(
		const QComboBox&                  aSelector,
		const int                         aRow,
		const std::optional<std::string>& aEnterpriseHostname)
		-> std::optional<std::string>
	{
		const auto label = aSelector.itemText(aRow);
		if (label == QStringLiteral("GitHub.com — GitHub CLI (unavailable)")) {
			return std::string{ "Active host: github.com (GitHub.com)" };
		}
		if (
			label == QStringLiteral("GHES 3.18 — GitHub CLI (unavailable)")
			&& aEnterpriseHostname.has_value()) {
			return std::string{ "Active host: " } + *aEnterpriseHostname
				+ " (GitHub Enterprise Server)";
		}
		return std::nullopt;
	}

	[[nodiscard]] auto activate_cli_unavailable_profile(
		bpr::qt::github_window& aWindow,
		automated_run&          aRun,
		const int               aRow) -> bool
	{
		auto& cli          = aRun.Cli;
		auto  expectedHost = expected_cli_active_host_text(
			*aWindow.profile_selector(), aRow, cli.EnterpriseHostname);
		if (!expectedHost.has_value()) {
			return false;
		}

		aWindow.profile_selector()->setCurrentIndex(aRow);
		if (
			aWindow.profile_selector()->currentIndex() != aRow
			|| !aWindow.connect_action()->isEnabled()) {
			return false;
		}

		const auto statistics              = aWindow.statistics();
		cli.ActiveRow                      = aRow;
		cli.IdentityUpdatesBeforeConnect   = statistics.IdentityUpdates;
		cli.DiagnosticUpdatesBeforeConnect = statistics.DiagnosticUpdates;
		cli.ExpectedActiveHostText         = std::move(*expectedHost);
		aWindow.connect_action()->trigger();
		return true;
	}

	void finish_automated_run(
		bpr::app::github_application& aApplication,
		const automated_run&          aRun)
	{
		const auto status                    = aApplication.status();
		const auto nativeVaultEvidencePassed = !aRun.VerifyNativePlatformVault
			|| status.NativePlatformVaultSelected
				== native_platform_vault_expected();
		const auto cliUnavailableEvidencePassed = !aRun.Cli.Enabled
			|| (aRun.Cli.Expected > 0U
				&& aRun.Cli.Observed == aRun.Cli.Expected);
		auto passed = aRun.PresentationPassed && status.ShutdownComplete
			&& !status.ShutdownCleanupFailed
			&& !status.EpochProjectionDeliveryFailed
			&& !status.EpochHandoffDeliveryFailed
			&& nativeVaultEvidencePassed
			&& cliUnavailableEvidencePassed;
		auto renderSaved = aRun.RenderSaved;

		// `passed` is narrowed in four places. Reporting only the first group
		// meant a failure in any later one printed a verdict in which
		// everything looked fine, which is exactly what made BPR-0133
		// unattributable.
		const auto passedCoreFacts   = passed;
		auto       passedFixture     = passed;
		auto       passedMaintenance = passed;

#if defined(BPR_BUILD_TESTING)
		const auto fixtureEvidence = aRun.Fixture
			? bpr::app::detail::github_application_fixture_access::evidence(
				  aApplication)
			: bpr::app::detail::github_public_path_fixture_evidence{ };
		if (aRun.Fixture) {
			const auto expectedSmartCalls = aRun.MaintenanceProbe
					== automated_run::epoch_maintenance_probe::verify_deleted
				? 1
				: expected_fixture_smart_calls(aRun.EpochPhase);
			passed                        = passed && fixtureEvidence.RecordingConsumed
				&& fixtureEvidence.CapabilityProbeConsumed
				&& fixtureEvidence.BootstrapTransportSelected
				&& fixtureEvidence.AccountTransportSelected
				&& fixtureEvidence.CredentialStored
				&& fixtureEvidence.CredentialRetrieved
				&& fixtureEvidence.CredentialDeleted
				&& !fixtureEvidence.SecretRetained
				&& fixtureEvidence.SmartAcquireCount
					== expectedSmartCalls
				&& fixtureEvidence.SmartDiscoveryCount
					== expectedSmartCalls
				&& fixtureEvidence.SmartLsRefsCount
					== expectedSmartCalls
				&& fixtureEvidence.SmartFetchCount
					== expectedSmartCalls;
			passedFixture     = passed;
			passedMaintenance = passed;
			if (aRun.MaintenanceProbe
				== automated_run::epoch_maintenance_probe::verify_deleted) {
				const auto maintenance = bpr::app::detail::
					github_application_fixture_access::maintenance_evidence(
						aApplication, { });
				passed = passed && maintenance.TargetBaseCacheDeleted
					&& maintenance.TargetHeadCacheDeleted
					&& !maintenance.RetiredRepositoryPresent
					&& maintenance.RetiredCacheDeletionMarkerPresent
					&& maintenance.PruneIntentDispatchCount == 0U
					&& maintenance.DeleteIntentDispatchCount == 0U;
				passedMaintenance = passed;
			}
		}
		if (aRun.Appearance.Enabled) {
			const auto appearancePassed = aRun.Appearance.SignalObserved
				&& aRun.Appearance.StatePassed
				&& aRun.Appearance.PixelsDiffer;
			renderSaved = passed && appearancePassed
				&& persist_native_appearance_evidence(
							  aRun.Appearance.InitialRender,
							  aRun.Appearance.ChangedRender,
							  aRun.Appearance.EvidencePaths);
			passed = passed && appearancePassed && renderSaved;
		} else {
			passed = passed && renderSaved;
		}
#else
		passed = passed && renderSaved;
#endif

		QTextStream stream{ stdout };
#if defined(BPR_BUILD_TESTING)
		if (aRun.Fixture) {
			stream << "GITHUB_FIXTURE_QT_PLATFORM:"
				   << QGuiApplication::platformName() << '\n'
				   << "GITHUB_FIXTURE_QT_STYLE:"
				   << aApplication.window().style()->metaObject()->className()
				   << '\n';
		}
#endif
		stream << (passed ? "PUBLIC_PATH_OK\n" : "PUBLIC_PATH_FAILED\n");
		if (aRun.VerifyNativePlatformVault) {
			stream << "NATIVE_PLATFORM_VAULT_SELECTED:"
				   << (status.NativePlatformVaultSelected ? "true" : "false")
				   << '\n';
		}
		if (aRun.Cli.Enabled) {
			stream << (passed ? "CLI_UNAVAILABLE_OK\n"
							  : "CLI_UNAVAILABLE_FAILED\n")
				   << "CLI_UNAVAILABLE_COUNT:"
				   << static_cast<unsigned long long>(
						  aRun.Cli.Observed)
				   << '\n';
		}
#if defined(BPR_BUILD_TESTING)
		if (aRun.Fixture
			&& aRun.EpochPhase
				!= bpr::app::detail::github_epoch_fixture_phase::single) {
			const auto phase = aRun.EpochPhase
					== bpr::app::detail::github_epoch_fixture_phase::seed
				? QStringLiteral("SEED")
				: QStringLiteral("REUSE");
			stream << "EPOCH_FIXTURE_" << phase << (passed ? "_OK\n" : "_FAILED\n");
		}
		if (aRun.Fixture
			&& aRun.MaintenanceProbe
				!= automated_run::epoch_maintenance_probe::none) {
			const auto marker = aRun.MaintenanceProbe
					== automated_run::epoch_maintenance_probe::prune
				? QStringLiteral("EPOCH_MAINTENANCE_PRUNE")
				: aRun.MaintenanceProbe
					== automated_run::epoch_maintenance_probe::
						verify_pruned_and_delete
				? QStringLiteral("EPOCH_MAINTENANCE_PRUNED_RESTART_DELETE")
				: QStringLiteral("EPOCH_MAINTENANCE_DELETED_RESTART");
			stream << marker << (passed ? "_OK\n" : "_FAILED\n");
		}
#endif
		if (!passed) {
			const auto& window = aApplication.window();
			stream << "PUBLIC_PATH_STAGE:"
				   << (aRun.FailedStage >= 0 ? aRun.FailedStage
											 : static_cast<int>(aRun.Stage))
				   << '\n'
				   // The verdict is a conjunction of six facts, and five of
				   // them used to be invisible. A failure that reported only
				   // the stage named `shutdown` every time, because that is
				   // where the run ends regardless of which fact was false.
				   << "PUBLIC_PATH_VERDICT:"
				   << "presentation=" << (aRun.PresentationPassed ? 1 : 0)
				   << " shutdown_complete=" << (status.ShutdownComplete ? 1 : 0)
				   << " cleanup_failed=" << (status.ShutdownCleanupFailed ? 1 : 0)
				   << " epoch_projection_failed="
				   << (status.EpochProjectionDeliveryFailed ? 1 : 0)
				   << " epoch_handoff_failed="
				   << (status.EpochHandoffDeliveryFailed ? 1 : 0) << '\n'
				   // Which of the four narrowing groups rejected the run. The
				   // first `0` names the group to look in.
				   << "PUBLIC_PATH_GROUPS:"
				   << "core=" << (passedCoreFacts ? 1 : 0)
				   << " fixture=" << (passedFixture ? 1 : 0)
				   << " maintenance=" << (passedMaintenance ? 1 : 0)
				   << " render=" << (renderSaved ? 1 : 0) << '\n'
				   // Whether the run stopped itself or failed a check, with
				   // the numbers to set against the harness timeout. A run
				   // killed from outside prints none of this, so the presence
				   // of the line is itself the evidence that the process
				   // reached its own limit first.
				   << "PUBLIC_PATH_BUDGET:"
				   << "expired=" << (aRun.BudgetExpired ? 1 : 0)
				   << " budget_ms="
				   << static_cast<long long>(aRun.Budget.count())
				   << " elapsed_ms="
				   << static_cast<long long>(
						  std::chrono::duration_cast<std::chrono::milliseconds>(
							  automated_clock::now() - aRun.Started)
							  .count())
				   << " stage_ticks="
				   << static_cast<unsigned long long>(aRun.Attempts) << '\n'
				   << "PUBLIC_PATH_STATUS:"
				   << QString::fromStdString(window.status_text()) << '\n'
				   << "PUBLIC_PATH_DIAGNOSTIC:"
				   << QString::fromStdString(window.diagnostic_text()) << '\n'
				   << "PUBLIC_PATH_ACCOUNT:"
				   << QString::fromStdString(window.active_account_text()) << '\n'
				   << "PUBLIC_PATH_EPOCH_STATUS:"
				   << (aRun.LastEpochStatus.isEmpty()
						   ? window.epoch_control()->status_text()
						   : aRun.LastEpochStatus)
				   << '\n'
				   << "PUBLIC_PATH_EPOCH_COMPARISON:"
				   << (aRun.LastEpochComparison.isEmpty()
						   ? window.epoch_control()->comparison_text()
						   : aRun.LastEpochComparison)
				   << '\n'
				   << "PUBLIC_PATH_EPOCH_PAIR:"
				   << (aRun.LastEpochLeft.isEmpty()
						   ? window.epoch_control()->left_oid_editor()->text()
						   : aRun.LastEpochLeft)
				   << ':'
				   << (aRun.LastEpochRight.isEmpty()
						   ? window.epoch_control()->right_oid_editor()->text()
						   : aRun.LastEpochRight)
				   << ':'
				   << (aRun.LastEpochLeftRow >= 0
						   ? aRun.LastEpochLeftRow
						   : window.epoch_control()->left_selector()->currentIndex())
				   << ':'
				   << (aRun.LastEpochRightRow >= 0
						   ? aRun.LastEpochRightRow
						   : window.epoch_control()->right_selector()->currentIndex())
				   << ':'
				   << static_cast<unsigned long long>(
						  aRun.LastEpochCount != 0U
							  ? aRun.LastEpochCount
							  : window.epoch_control()
									->history_model()
									->epoch_count())
				   << ':'
				   << (aRun.LastEpochHistoryEnabled
							   || window.epoch_control()
									  ->load_history_action()
									  ->isEnabled()
						   ? 1
						   : 0)
				   << '\n'
				   << "PUBLIC_PATH_EPOCH_CORE_STATUS:"
				   << aRun.LastEpochCoreStatus << '\n';
#if defined(BPR_BUILD_TESTING)
			if (aRun.MaintenanceProbe
				!= automated_run::epoch_maintenance_probe::none) {
				const auto maintenance = bpr::app::detail::
					github_application_fixture_access::maintenance_evidence(
						aApplication, aRun.MaintenanceTarget);
				stream << "PUBLIC_PATH_MAINTENANCE_EVIDENCE:"
					   << maintenance.PruneIntentDispatchCount << ':'
					   << maintenance.DeleteIntentDispatchCount << ':'
					   << maintenance.CatalogRevision << ':'
					   << maintenance.TargetBaseRefPresent << ':'
					   << maintenance.TargetHeadRefPresent << ':'
					   << maintenance.NonTargetBaseRefPresent << ':'
					   << maintenance.NonTargetHeadRefPresent << ':'
					   << maintenance.RepositoryPresent << ':'
					   << maintenance.CacheDeletionMarkerPresent << ':'
					   << maintenance.TargetBaseCacheDeleted << ':'
					   << maintenance.TargetHeadCacheDeleted << ':'
					   << maintenance.RetiredRepositoryPresent << ':'
					   << maintenance.RetiredCacheDeletionMarkerPresent << ':'
					   << maintenance.TargetBaseAvailability << ':'
					   << maintenance.TargetHeadAvailability << '\n';
			}
#endif
		}
#if defined(BPR_BUILD_TESTING)
		if (aRun.Fixture) {
			stream << (passed ? "GITHUB_FIXTURE_OK:" : "GITHUB_FIXTURE_FAILED:")
				   << QString::fromStdString(aRun.FixtureName) << '\n';
			if (!passed) {
				stream << "GITHUB_FIXTURE_EVIDENCE:"
					   << fixtureEvidence.RecordingConsumed
					   << fixtureEvidence.CapabilityProbeConsumed
					   << fixtureEvidence.BootstrapTransportSelected
					   << fixtureEvidence.AccountTransportSelected
					   << fixtureEvidence.CredentialStored
					   << fixtureEvidence.CredentialRetrieved
					   << fixtureEvidence.CredentialDeleted
					   << fixtureEvidence.SecretRetained << ':'
					   << fixtureEvidence.RecordingFailure << ':'
					   << fixtureEvidence.AccountReadCount << ':'
					   << fixtureEvidence.LastReadOperation << '\n';
				stream << "GITHUB_FIXTURE_SMART_EVIDENCE:"
					   << fixtureEvidence.SmartAcquireCount << ':'
					   << fixtureEvidence.SmartDiscoveryCount << ':'
					   << fixtureEvidence.SmartLsRefsCount << ':'
					   << fixtureEvidence.SmartFetchCount << '\n';
			}
		}
		if (aRun.DiffProbe) {
			stream << (passed ? "DIFF_WORKBENCH_OK\n"
							  : "DIFF_WORKBENCH_FAILED\n");
			stream << (aRun.DiffInteractionEvidencePassed
						   ? "DIFF_INTERACTION_EVIDENCE_OK\n"
						   : "DIFF_INTERACTION_EVIDENCE_FAILED\n");
			stream << (aRun.SyntaxThemeRepaintPassed
						   ? "SYNTAX_THEME_REPAINT_OK\n"
						   : "SYNTAX_THEME_REPAINT_FAILED\n");
		}
		if (aRun.DiffShutdownProbe) {
			stream << (passed ? "DIFF_WORKBENCH_SHUTDOWN_OK\n"
							  : "DIFF_WORKBENCH_SHUTDOWN_FAILED\n");
		}
#endif
		if (aRun.RenderPath.has_value()) {
			stream << (renderSaved ? "RENDER_OK\n" : "RENDER_FAILED\n");
		}
		if (aRun.Fixture) {
			// After the render marker rather than inside its block: the
			// public-path expectation matches its markers contiguously, so a
			// line in the middle breaks a contract that has nothing to do with
			// the ledger — and the render block is skipped entirely by the
			// epoch phases, which is where the restart evidence comes from.
			//
			// Reported from the run that composed the ledger, so the evidence
			// is about the database the application actually opened. The
			// present-on-entry flag is what lets a later process against the
			// same application data root tell persistence from a fresh write.
			stream << "REVIEW_LEDGER_PRESENT_ON_ENTRY:"
				   << (aRun.LedgerPresentOnEntry ? 1 : 0) << '\n'
				   << (aRun.LedgerRoundTripped ? "REVIEW_LEDGER_OK\n"
											   : "REVIEW_LEDGER_FAILED\n");
		}
		// The runtime half of the composed-host evidence. `nm` proves the
		// workbench host is linked; only the running application can say its
		// constructor ran. Disabling the construction leaves every symbol in
		// place, so a symbol check alone would still report success.
		stream << (aApplication.status().WorkbenchHostComposed
					   ? "WORKBENCH_HOST_COMPOSED_OK\n"
					   : "WORKBENCH_HOST_COMPOSED_FAILED\n")
			   << (aApplication.status().AppearanceHostComposed
					   ? "APPEARANCE_HOST_COMPOSED_OK\n"
					   : "APPEARANCE_HOST_COMPOSED_FAILED\n")
			   << (aApplication.status().SyntaxClassifierComposed
					   ? "SYNTAX_CLASSIFIER_COMPOSED_OK\n"
					   : "SYNTAX_CLASSIFIER_COMPOSED_FAILED\n");
		// Classifications the panes took, and the complete-blob passes the
		// host had to run to produce them. Reported together because they
		// answer different questions: the first says a surface received an
		// answer, the second says how much lexical work that cost.
		stream << "SYNTAX_CLASSIFICATION:"
			   << aApplication.status().AdoptedClassifications << ':'
			   << aApplication.status().ClassificationSubmissions << '\n';
		// The settings in effect. A composition starts from `system` and a tab
		// width of four, so a run that made no gesture and reports anything
		// else read it from the durable record - which is the restart claim.
		stream << "APPEARANCE_SETTINGS:"
			   << QString::fromStdString(aApplication.status().AppearanceMode)
			   << ':' << aApplication.status().AppearanceTabWidth << '\n';
		// Must stay zero. The host and the presentation profile validate with
		// the same core function, so a non-zero count means they disagree about
		// legality - reported rather than hidden, because a divergence nobody
		// prints is a divergence nobody finds.
		// Composed delivery path, and what the desktop allows. A refused or
		// absent notification surface is a normal outcome and is reported as
		// one; only a missing delivery path is a failure.
		stream << (aApplication.status().NotificationDeliveryComposed
					   ? "NOTIFICATION_DELIVERY_COMPOSED_OK\n"
					   : "NOTIFICATION_DELIVERY_COMPOSED_FAILED\n");
		stream << "NOTIFICATION_PERMISSION:"
			   << notification_permission_name(
					  aApplication.status().NotificationPermission)
			   << '\n';
		stream << "APPEARANCE_DIVERGENCE:"
			   << aApplication.status().AppearanceDivergences << '\n';
#if defined(BPR_BUILD_TESTING)
		if (aRun.AppearanceGestureProbe) {
			stream << (aRun.AppearanceGesturePassed
						   ? "APPEARANCE_GESTURE_OK\n"
						   : "APPEARANCE_GESTURE_FAILED\n")
				   << (aRun.AppearanceRefusalPassed
						   ? "APPEARANCE_REFUSAL_OK\n"
						   : "APPEARANCE_REFUSAL_FAILED\n");
		}
#endif
		// Reports the colours actually on the window, so a reader can check
		// that presentation came from the appearance palette rather than from
		// the platform style.
		stream << "APPEARANCE_APPLIED:"
			   << QString::fromStdString(
					  aApplication.status().AppliedSelectionColor)
			   << ':'
			   << QString::fromStdString(aApplication.status().AppliedBaseColor)
			   << (aApplication.status().AppliedFromProfile ? ":from-profile"
															: ":from-platform")
			   << '\n';
#if defined(BPR_BUILD_TESTING)
		if (aRun.Appearance.Enabled) {
			stream << (aRun.Appearance.ProfileFollowsPassed
						   ? "APPEARANCE_SYSTEM_FOLLOWS_OK\n"
						   : "APPEARANCE_SYSTEM_FOLLOWS_FAILED\n")
				   << (aRun.Appearance.ExplicitIgnoresPassed
						   ? "APPEARANCE_EXPLICIT_IGNORES_OK\n"
						   : "APPEARANCE_EXPLICIT_IGNORES_FAILED\n")
				   << "APPEARANCE_VARIANTS:"
				   << aRun.Appearance.VariantBeforeChange << ':'
				   << aRun.Appearance.VariantAfterChange << ':'
				   << aRun.Appearance.VariantUnderExplicit << ':'
				   << aRun.Appearance.VariantBackOnSystem << '\n';
			stream << "NATIVE_APPEARANCE_TRANSITION:"
				   << color_scheme_name(
						  aRun.Appearance.InitialColorScheme)
				   << "->"
				   << color_scheme_name(
						  aRun.Appearance.ObservedColorScheme)
				   << '\n'
				   << "NATIVE_APPEARANCE_GLOBAL_TRANSITION:"
				   << color_scheme_name(
						  aRun.Appearance.InitialGlobalColorScheme)
				   << "->"
				   << color_scheme_name(
						  aRun.Appearance.ObservedGlobalColorScheme)
				   << '\n'
				   << "NATIVE_APPEARANCE_PLATFORM:"
				   << aRun.Appearance.InitialPlatformName << '\n'
				   << "NATIVE_APPEARANCE_STYLE:"
				   << aRun.Appearance.InitialStyleClass << '\n'
				   << (aRun.Appearance.InitialWindowActive
							   && aRun.Appearance.InitialFocus != nullptr
							   && aRun.Appearance.StatePassed
						   ? "NATIVE_APPEARANCE_ACTIVE_FOCUS_OK\n"
						   : "NATIVE_APPEARANCE_ACTIVE_FOCUS_FAILED\n")
				   << (aRun.Appearance.SignalObserved
						   ? "NATIVE_APPEARANCE_SIGNAL_OK\n"
						   : "NATIVE_APPEARANCE_SIGNAL_FAILED\n")
				   << (aRun.Appearance.StatePassed
						   ? "NATIVE_APPEARANCE_STATE_OK\n"
						   : "NATIVE_APPEARANCE_STATE_FAILED\n")
				   << (aRun.Appearance.PixelsDiffer
						   ? "NATIVE_APPEARANCE_PIXELS_DIFFER_OK\n"
						   : "NATIVE_APPEARANCE_PIXELS_DIFFER_FAILED\n")
				   << "NATIVE_APPEARANCE_DIFF_PALETTE_BEFORE:"
				   << native_palette_fingerprint(
						  aRun.Appearance.InitialDiffPalette)
				   << '\n'
				   << "NATIVE_APPEARANCE_DIFF_PALETTE_AFTER:"
				   << native_palette_fingerprint(
						  aRun.Appearance.ObservedDiffPalette)
				   << '\n'
				   << "NATIVE_APPEARANCE_DIFF_FONT_BEFORE:"
				   << aRun.Appearance.InitialDiffFont.toString() << '\n'
				   << "NATIVE_APPEARANCE_DIFF_FONT_AFTER:"
				   << aRun.Appearance.ObservedDiffFont.toString() << '\n'
				   << "NATIVE_APPEARANCE_DIFF_GEOMETRY_REBUILD_DELTA:"
				   << aRun.Appearance.DiffGeometryRebuildDelta << '\n'
				   << (aRun.Appearance.InitialDiffPalette
								   != aRun.Appearance.ObservedDiffPalette
							   && aRun.Appearance.StatePassed
						   ? "NATIVE_APPEARANCE_DIFF_PALETTE_INHERITED_OK\n"
						   : "NATIVE_APPEARANCE_DIFF_PALETTE_INHERITED_FAILED\n")
				   << (aRun.Appearance.InitialDiffFont
								   == aRun.Appearance.ObservedDiffFont
							   && aRun.Appearance.StatePassed
						   ? "NATIVE_APPEARANCE_DIFF_FONT_INHERITED_OK\n"
						   : "NATIVE_APPEARANCE_DIFF_FONT_INHERITED_FAILED\n")
				   << (aRun.Appearance.DiffGeometryRebuildDelta == 1U
							   && aRun.Appearance.StatePassed
						   ? "NATIVE_APPEARANCE_DIFF_GEOMETRY_REBUILD_OK\n"
						   : "NATIVE_APPEARANCE_DIFF_GEOMETRY_REBUILD_FAILED\n")
				   << (aRun.Appearance.StatePassed
						   ? "NATIVE_APPEARANCE_DIFF_SELECTION_OK\n"
						   : "NATIVE_APPEARANCE_DIFF_SELECTION_FAILED\n")
				   << (renderSaved
						   ? "NATIVE_APPEARANCE_CHANGED_RENDER_OK\n"
						   : "NATIVE_APPEARANCE_CHANGED_RENDER_FAILED\n");
		}
#endif
		QCoreApplication::exit(passed ? EXIT_SUCCESS : EXIT_FAILURE);
	}

	void transition_to_shutdown(
		bpr::app::github_application& aApplication,
		automated_run&                aRun)
	{
		aApplication.begin_shutdown();
		aRun.Stage = automated_run::stage::shutdown;
	}

	void fail_automated_run(
		bpr::app::github_application& aApplication,
		automated_run&                aRun)
	{
		// The stage is recorded here as well as on timeout. Without it a failed
		// check reported `shutdown`, because the printed stage falls back to
		// the current one and shutdown is always where the run ends up. Every
		// failure then looked identical and named the one stage that could not
		// have caused it.
		aRun.PresentationPassed = false;
		aRun.FailedStage        = static_cast<int>(aRun.Stage);
		transition_to_shutdown(aApplication, aRun);
	}

	void capture_automated_timeout_evidence(
		bpr::app::github_application& aApplication,
		automated_run&                aRun)
	{
		auto& window                 = aApplication.window();
		aRun.PresentationPassed      = false;
		aRun.FailedStage             = static_cast<int>(aRun.Stage);
		aRun.LastEpochStatus         = window.epoch_control()->status_text();
		aRun.LastEpochComparison     = window.epoch_control()->comparison_text();
		aRun.LastEpochLeft           = window.epoch_control()->left_oid_editor()->text();
		aRun.LastEpochRight          = window.epoch_control()->right_oid_editor()->text();
		aRun.LastEpochLeftRow        = window.epoch_control()->left_selector()->currentIndex();
		aRun.LastEpochRightRow       = window.epoch_control()->right_selector()->currentIndex();
		aRun.LastEpochCount          = window.epoch_control()->history_model()->epoch_count();
		aRun.LastEpochHistoryEnabled = window.epoch_control()->load_history_action()->isEnabled();
		aRun.LastEpochCoreStatus     = static_cast<int>(aApplication.status().EpochStatus);
#if defined(BPR_BUILD_TESTING)
		if (!aRun.Appearance.Enabled)
#endif
		{
			aRun.RenderSaved = save_render_evidence(window, aRun.RenderPath);
		}
		transition_to_shutdown(aApplication, aRun);
	}

	void schedule_automated_run(
		bpr::app::github_application&  aApplication,
		std::shared_ptr<automated_run> aRun)
	{
#if defined(BPR_BUILD_TESTING)
		if (aRun->Appearance.Enabled) {
			aRun->RenderSaved = false;
			QObject::connect(
				QGuiApplication::styleHints(),
				&QStyleHints::colorSchemeChanged,
				&aApplication.window(),
				[aRun](const Qt::ColorScheme aScheme) {
					aRun->Appearance.SignalObserved      = true;
					aRun->Appearance.ObservedColorScheme = aScheme;
					aRun->Appearance.SettleAttempts      = 0U;
					aRun->Appearance.GlobalPollTicks     = 0U;
				});
		}
#endif

		aRun->Started = automated_clock::now();

		auto tick = std::make_shared<std::function<void()>>();
		*tick     = [&aApplication, aRun, tick] {
			auto& window = aApplication.window();
			++aRun->Attempts;
			// One budget for the whole run rather than one per stage. What has
			// to be provably smaller than the harness `TIMEOUT` is the
			// process's own lifetime, and per-stage budgets sum: a run of
			// fourteen stages each allowed a minute is a run the harness kills
			// long before any single stage gives up.
			if (aRun->Stage != automated_run::stage::shutdown
				&& automated_clock::now() - aRun->Started
					> aRun->Budget) {
				aRun->BudgetExpired = true;
				capture_automated_timeout_evidence(aApplication, *aRun);
			}

			switch (aRun->Stage) {
			case automated_run::stage::start:
#if defined(BPR_BUILD_TESTING)
				if (aRun->Fixture) {
					// Read before writing. On a second run against the same
					// application data root this is what distinguishes a
					// receipt that persisted from one this run just wrote.
					aRun->LedgerPresentOnEntry = bpr::app::detail::
						github_application_fixture_access::
							review_ledger_receipt_present(aApplication);
					aRun->LedgerRoundTripped = bpr::app::detail::
						github_application_fixture_access::
							review_ledger_round_trip(aApplication);
					if (aRun->AppearanceGestureProbe) {
						// Here rather than in a later stage because this is the
						// one point every fixture path passes through, seed and
						// reuse included. The gesture needs the window and the
						// appearance host, both of which exist from
						// construction; it needs no pull request and no
						// comparison.
						//
						// Driven through the composed controls, and judged on
						// the window rather than on the host: the claim is that
						// a gesture reaches presentation, and a host that
						// adopted a mode nothing applied would satisfy a
						// host-only check.
						const auto before  = aApplication.status().AppliedBaseColor;
						const auto applied = bpr::app::detail::
							github_application_fixture_access::
								apply_appearance_gesture(
									aApplication,
									bpr::core::appearance_mode::dark,
									8U);
						QApplication::processEvents();
						aRun->AppearanceGesturePassed = applied
							&& aApplication.status().AppliedBaseColor != before
							&& aApplication.status().AppliedFromProfile;
						aRun->AppearanceRefusalPassed = bpr::app::detail::
							github_application_fixture_access::
								refuse_appearance_gesture(aApplication);
						QApplication::processEvents();
					}
					window.connect_action()->trigger();
					advance_fixture_stage(
						*aRun, automated_run::stage::connect);
				} else
#endif
				{
					const auto visibleCliProfiles = cli_unavailable_profile_count(
						*window.profile_selector());
					const auto expectedCliProfiles = aRun->Cli.EnterpriseHostname.has_value() ? 2U : 1U;
					if (
						aRun->Cli.Enabled
						&& (visibleCliProfiles != expectedCliProfiles
							|| window.profile_selector()->count()
								!= static_cast<int>(
									expectedCliProfiles * 2U))) {
						fail_automated_run(aApplication, *aRun);
						break;
					}
					aRun->Cli.Expected         = aRun->Cli.Enabled
						? expectedCliProfiles
						: 1U;
					const auto firstCliProfile = next_cli_unavailable_profile(
						*window.profile_selector(), -1);
					if (
						firstCliProfile < 0
						|| !activate_cli_unavailable_profile(
							window, *aRun, firstCliProfile)) {
						fail_automated_run(aApplication, *aRun);
						break;
					}
					aRun->Stage = automated_run::stage::connect;
				}
				break;

			case automated_run::stage::connect:
#if defined(BPR_BUILD_TESTING)
				if (aRun->Fixture) {
					if (window.status_text() != "GitHub account connected."
						|| !window.open_action()->isEnabled()) {
						break;
					}
					window.owner_editor()->setText(QStringLiteral("octo-org"));
					window.repository_editor()->setText(
						QStringLiteral("review-repository"));
					window.pull_request_editor()->setValue(17);
					window.open_action()->trigger();
					advance_fixture_stage(*aRun, automated_run::stage::open);
				} else if (cli_fail_closed_presentation_passed(window, *aRun))
#else
				if (cli_fail_closed_presentation_passed(window, *aRun))
#endif
				{
					++aRun->Cli.Observed;
					const auto nextCliProfile = aRun->Cli.Enabled
						? next_cli_unavailable_profile(
							  *window.profile_selector(),
							  aRun->Cli.ActiveRow)
						: -1;
					if (nextCliProfile >= 0) {
						if (!activate_cli_unavailable_profile(
								window, *aRun, nextCliProfile)) {
							fail_automated_run(aApplication, *aRun);
						}
					} else {
						aRun->PresentationPassed = aRun->Cli.Observed == aRun->Cli.Expected;
						aRun->RenderSaved        = save_render_evidence(window, aRun->RenderPath);
						transition_to_shutdown(aApplication, *aRun);
					}
				}
				break;

			case automated_run::stage::open:
#if defined(BPR_BUILD_TESTING)
				// The deliberate stall. `open` is where every long fixture
				// wait lives, so a stall here is the shape of the hang this
				// budget exists to catch.
				if (aRun->StallProbe) {
					break;
				}
				if (aRun->MaintenanceProbe
					== automated_run::epoch_maintenance_probe::verify_deleted) {
					if (begin_epoch_maintenance_probe(aApplication, *aRun)) {
						aRun->PresentationPassed = true;
						transition_to_shutdown(aApplication, *aRun);
					}
					break;
				}
				if (fixture_presentation_passed(
						window, aRun->ExpectedActiveHostText)) {
					const auto evidence = bpr::app::detail::
						github_application_fixture_access::evidence(aApplication);
					const auto persistentPhase = aRun->EpochPhase
						!= bpr::app::detail::
							github_epoch_fixture_phase::single;
					const auto initialHead   = aRun->EpochPhase
							== bpr::app::detail::
								github_epoch_fixture_phase::reuse
						? evidence.HeadTwoOid
						: evidence.HeadOneOid;
					const auto initialEpochs = aRun->EpochPhase
							== bpr::app::detail::
								github_epoch_fixture_phase::reuse
						? 0U
						: 1U;
					if (aRun->MaintenanceProbe
						!= automated_run::epoch_maintenance_probe::none) {
						if (begin_epoch_maintenance_probe(aApplication, *aRun)) {
						}
						break;
					}
					if (!fixture_epoch_pair_passed(
							window,
							evidence.BaseOid,
							initialHead,
							initialEpochs)) {
						break;
					}
					if (aRun->EpochPhase
						== bpr::app::detail::github_epoch_fixture_phase::seed) {
						window.refresh_action()->trigger();
						advance_fixture_stage(
							*aRun, automated_run::stage::epoch_refresh);
					} else if (persistentPhase) {
						auto* const control = window.epoch_control();
						if (control->history_model()->epoch_count() < 2U) {
							if (control->return_to_newest_action()->isEnabled()) {
								control->return_to_newest_action()->trigger();
							} else if (control->load_history_action()->isEnabled()) {
								control->load_history_action()->trigger();
							} else {
								break;
							}
							advance_fixture_stage(
								*aRun, automated_run::stage::epoch_history);
						} else {
							control->whole_pr_action()->trigger();
							advance_fixture_stage(
								*aRun, automated_run::stage::epoch_whole_pr);
						}
					} else if (aRun->Appearance.Enabled) {
						aRun->Appearance.ExpectedEpochLeftOid  = QString::fromStdString(evidence.BaseOid);
						aRun->Appearance.ExpectedEpochRightOid = QString::fromStdString(evidence.HeadOneOid);
						if (!aRun->DiffRequested) {
							aRun->DiffRequested = bpr::app::detail::
								github_application_fixture_access::request_fixture_diff(
									aApplication);
							if (!aRun->DiffRequested) {
								fail_automated_run(aApplication, *aRun);
							}
							break;
						}
						if (window.diff_comparison_workbench()->current_result() == nullptr) {
							break;
						}
						const auto waitingForOwnerFocus = QGuiApplication::platformName() == "cocoa"
							&& (!window.isActiveWindow()
								|| QApplication::focusWidget() == nullptr);
						if (waitingForOwnerFocus) {
							if (!aRun->Appearance.WaitingForOwnerFocusReported) {
								QTextStream stream{ stdout };
								stream
									<< "NATIVE_APPEARANCE_WAITING_FOR_OWNER_FOCUS\n";
								stream.flush();
								aRun->Appearance.WaitingForOwnerFocusReported = true;
							}
							break;
						}
						if (prepare_native_appearance_probe(window, *aRun)) {
							// Read here rather than inside the preparation,
							// which sees the window and not the application.
							// This is the variant the composed profile
							// resolved while the operating system still
							// reported Light.
							aRun->Appearance.VariantBeforeChange
								= static_cast<int>(
									aApplication.status().ProfileVariant);
							aRun->Stage = automated_run::stage::appearance;
						} else {
							fail_automated_run(aApplication, *aRun);
						}
					} else if (aRun->DiffShutdownProbe) {
						aRun->DiffRequested = bpr::app::detail::
							github_application_fixture_access::request_fixture_diff(
								aApplication);
						aRun->PresentationPassed = aRun->DiffRequested;
						if (aRun->DiffRequested) {
							transition_to_shutdown(aApplication, *aRun);
						} else {
							fail_automated_run(aApplication, *aRun);
						}
					} else if (aRun->DiffProbe) {
						if (!aRun->DiffRequested) {
							aRun->DiffRequested = bpr::app::detail::
								github_application_fixture_access::request_fixture_diff(
									aApplication);
							if (!aRun->DiffRequested) {
								fail_automated_run(aApplication, *aRun);
							}
							break;
						}
						auto* const workbench = window.diff_comparison_workbench();
						if (workbench->current_result() == nullptr) {
							break;
						}
						const auto& segments = workbench->current_result()->Alignment.Segments;
						const auto  hasKind  = [&segments](
												   const bpr::core::alignment_kind aKind) {
							return std::ranges::any_of(
								segments,
								[aKind](const auto& aSegment) {
									return aSegment.Kind == aKind;
								});
						};
						const auto ready = !workbench->left_pane()->toPlainText().isEmpty()
							&& !workbench->right_pane()->toPlainText().isEmpty()
							&& hasKind(bpr::core::alignment_kind::equal)
							&& hasKind(bpr::core::alignment_kind::replacement)
							&& hasKind(bpr::core::alignment_kind::left_only)
							&& hasKind(bpr::core::alignment_kind::right_only)
							&& workbench->selection().has_value()
							&& workbench->link_map()->selected().has_value()
							&& workbench->detail_pane()->toPlainText().startsWith(
								QStringLiteral("Change "));
						// An accepted result does not mean the state derived
						// from it has arrived. Selection, the link map, and the
						// detail pane are populated by a scheduled geometry
						// rebuild, so they lag the result by at least one tick.
						// Sampling them in the tick that first sees a result
						// made this probe fail about one run in six, with no
						// retry and nothing in the output naming the cause.
						// Waiting is what the branch above already does for the
						// result itself; the attempt budget still bounds it.
						if (!ready) {
							break;
						}
						window.resize(1'280, 1'000);
						if (auto* const scroll = qobject_cast<QScrollArea*>(window.centralWidget())) {
							scroll->ensureWidgetVisible(workbench, 24, 24);
						}
						QApplication::processEvents();
						// Selected before the rasters are written, not after.
						// BPR-0201: the appearance-gesture assertion changes
						// the mode later in this same process, so a capture
						// only shows the scheme it was taken under and the
						// end-of-run marker is no guide to it.
						if (aRun->DiffEvidenceAppearance.has_value()) {
							static_cast<void>(
								bpr::app::detail::
									github_application_fixture_access::
										apply_appearance_gesture(
											aApplication,
											*aRun->DiffEvidenceAppearance,
											8U));
							QApplication::processEvents();
						}
						aRun->DiffInteractionEvidencePassed = ready
							&& capture_diff_interaction_evidence(
																  window, aRun->DiffEvidenceDirectory);
						aRun->RenderSaved = aRun->DiffInteractionEvidencePassed
							&& save_render_evidence(window, aRun->RenderPath);

						// Measured here, while a classified comparison is on
						// screen and before the reaccept sequence clears it.
						// A theme change must repaint from the retained spans;
						// if it reparses, the submission count says so.
						{
							const auto adoptedBefore   = aApplication.status()
															 .AdoptedClassifications;
							const auto submittedBefore = aApplication.status()
															 .ClassificationSubmissions;
							const auto switched        = bpr::app::detail::
								github_application_fixture_access::
									select_other_appearance_mode(aApplication);
							QApplication::processEvents();
							aRun->SyntaxThemeRepaintPassed = switched
								&& adoptedBefore > 0U
								&& aApplication.status().ClassificationSubmissions
									== submittedBefore;
						}

						const auto retainedResult    = workbench->current_result();
						const auto retainedSelection = workbench->selection();
						const auto retainedContext   = workbench->current_context();
						const auto unchangedAccepted = aRun->DiffInteractionEvidencePassed
							&& bpr::app::detail::github_application_fixture_access::
								reaccept_fixture_revision(aApplication, false);
						QApplication::processEvents();
						const auto unchangedPreserved = unchangedAccepted
							&& workbench->current_result() == retainedResult
							&& workbench->selection() == retainedSelection
							&& workbench->current_context() == retainedContext;
						const auto changedAccepted = unchangedPreserved
							&& bpr::app::detail::github_application_fixture_access::
								reaccept_fixture_revision(aApplication, true);
						QApplication::processEvents();
						aRun->PresentationPassed = aRun->DiffInteractionEvidencePassed
							&& changedAccepted
							&& workbench->current_result() == nullptr
							&& !workbench->current_context().has_value()
							&& workbench->status_text()
								== QStringLiteral("Select a file comparison");
						transition_to_shutdown(aApplication, *aRun);
					} else {
						aRun->PresentationPassed = true;
						aRun->RenderSaved        = save_render_evidence(window, aRun->RenderPath);
						transition_to_shutdown(aApplication, *aRun);
					}
				}
#else
				fail_automated_run(aApplication, *aRun);
#endif
				break;

#if defined(BPR_BUILD_TESTING)
			case automated_run::stage::epoch_history: {
				const auto evidence = bpr::app::detail::
					github_application_fixture_access::evidence(aApplication);
				if (fixture_epoch_pair_passed(
						window, evidence.BaseOid, evidence.HeadTwoOid)
					&& fixture_epoch_cause_passed(
						*window.epoch_control()->history_model(),
						2U,
						bpr::core::epoch_primary_cause::force_push_rebase)) {
					window.epoch_control()->whole_pr_action()->trigger();
					advance_fixture_stage(
						*aRun, automated_run::stage::epoch_whole_pr);
				}
				break;
			}

			case automated_run::stage::epoch_refresh: {
				const auto evidence = bpr::app::detail::
					github_application_fixture_access::evidence(aApplication);
				if (fixture_presentation_passed(
						window, aRun->ExpectedActiveHostText)
					&& fixture_epoch_pair_passed(
						window,
						evidence.BaseOid,
						evidence.HeadTwoOid)
					&& fixture_epoch_cause_passed(
						*window.epoch_control()->history_model(),
						2U,
						bpr::core::epoch_primary_cause::force_push_rebase)) {
					window.epoch_control()->whole_pr_action()->trigger();
					advance_fixture_stage(
						*aRun, automated_run::stage::epoch_whole_pr);
				}
				break;
			}

			case automated_run::stage::epoch_whole_pr: {
				const auto evidence = bpr::app::detail::
					github_application_fixture_access::evidence(aApplication);
				auto* const control = window.epoch_control();
				if (fixture_epoch_pair_passed(
						window, evidence.BaseOid, evidence.HeadTwoOid)
					&& control->whole_pr_action()->isChecked()) {
					control->previous_upload_action()->trigger();
					advance_fixture_stage(
						*aRun,
						automated_run::stage::epoch_previous_upload);
				}
				break;
			}

			case automated_run::stage::epoch_previous_upload: {
				const auto evidence = bpr::app::detail::
					github_application_fixture_access::evidence(aApplication);
				auto* const control = window.epoch_control();
				if (fixture_epoch_pair_passed(
						window, evidence.HeadOneOid, evidence.HeadTwoOid)
					&& control->previous_upload_action()->isChecked()) {
					control->last_reviewed_action()->trigger();
					advance_fixture_stage(
						*aRun,
						automated_run::stage::epoch_last_reviewed);
				}
				break;
			}

			case automated_run::stage::epoch_last_reviewed: {
				const auto evidence = bpr::app::detail::
					github_application_fixture_access::evidence(aApplication);
				auto* const control = window.epoch_control();
				if (fixture_epoch_pair_passed(
						window, evidence.HeadOneOid, evidence.HeadTwoOid)
					&& control->last_reviewed_action()->isChecked()) {
					aRun->EpochLeftRow = fixture_anchor_row(
						*control->history_model(),
						1U,
						bpr::core::comparison_anchor_kind::epoch_head);
					aRun->EpochRightRow = fixture_anchor_row(
						*control->history_model(),
						2U,
						bpr::core::comparison_anchor_kind::epoch_base);
					if (aRun->EpochLeftRow < 0 || aRun->EpochRightRow < 0) {
						fail_automated_run(aApplication, *aRun);
						break;
					}
					control->left_selector()->setCurrentIndex(
						aRun->EpochLeftRow);
					advance_fixture_stage(
						*aRun, automated_run::stage::epoch_arbitrary_left);
				}
				break;
			}

			case automated_run::stage::epoch_arbitrary_left: {
				const auto evidence = bpr::app::detail::
					github_application_fixture_access::evidence(aApplication);
				auto* const control = window.epoch_control();
				if (fixture_epoch_pair_passed(
						window, evidence.HeadOneOid, evidence.HeadTwoOid)
					&& control->left_selector()->currentIndex()
						== aRun->EpochLeftRow
					&& !control->whole_pr_action()->isChecked()
					&& !control->previous_upload_action()->isChecked()
					&& !control->last_reviewed_action()->isChecked()) {
					control->right_selector()->setCurrentIndex(
						aRun->EpochRightRow);
					advance_fixture_stage(
						*aRun, automated_run::stage::epoch_arbitrary);
				}
				break;
			}

			case automated_run::stage::epoch_arbitrary: {
				const auto evidence = bpr::app::detail::
					github_application_fixture_access::evidence(aApplication);
				auto* const control = window.epoch_control();
				if (fixture_epoch_pair_passed(
						window, evidence.HeadOneOid, evidence.BaseOid)
					&& control->left_selector()->currentIndex()
						== aRun->EpochLeftRow
					&& control->right_selector()->currentIndex()
						== aRun->EpochRightRow
					&& !control->whole_pr_action()->isChecked()
					&& !control->previous_upload_action()->isChecked()
					&& !control->last_reviewed_action()->isChecked()) {
					std::swap(aRun->EpochLeftRow, aRun->EpochRightRow);
					control->left_selector()->setCurrentIndex(
						aRun->EpochLeftRow);
					advance_fixture_stage(
						*aRun, automated_run::stage::epoch_reverse_left);
				}
				break;
			}

			case automated_run::stage::epoch_reverse_left: {
				const auto evidence = bpr::app::detail::
					github_application_fixture_access::evidence(aApplication);
				auto* const control = window.epoch_control();
				if (fixture_epoch_pair_passed(
						window, evidence.BaseOid, evidence.BaseOid)
					&& control->left_selector()->currentIndex()
						== aRun->EpochLeftRow
					&& control->right_selector()->currentIndex()
						== aRun->EpochLeftRow) {
					control->right_selector()->setCurrentIndex(
						aRun->EpochRightRow);
					advance_fixture_stage(
						*aRun, automated_run::stage::epoch_reverse);
				}
				break;
			}

			case automated_run::stage::epoch_reverse: {
				const auto evidence = bpr::app::detail::
					github_application_fixture_access::evidence(aApplication);
				auto* const control = window.epoch_control();
				if (fixture_epoch_pair_passed(
						window, evidence.BaseOid, evidence.HeadOneOid)
					&& control->left_selector()->currentIndex()
						== aRun->EpochLeftRow
					&& control->right_selector()->currentIndex()
						== aRun->EpochRightRow) {
					const auto identical = fixture_anchor_row(
						*control->history_model(),
						2U,
						bpr::core::comparison_anchor_kind::epoch_head);
					if (identical < 0) {
						fail_automated_run(aApplication, *aRun);
						break;
					}
					aRun->EpochLeftRow  = identical;
					aRun->EpochRightRow = identical;
					control->left_selector()->setCurrentIndex(identical);
					advance_fixture_stage(
						*aRun, automated_run::stage::epoch_identical_left);
				}
				break;
			}

			case automated_run::stage::epoch_identical_left: {
				const auto evidence = bpr::app::detail::
					github_application_fixture_access::evidence(aApplication);
				auto* const control = window.epoch_control();
				if (fixture_epoch_pair_passed(
						window, evidence.HeadTwoOid, evidence.HeadOneOid)
					&& control->left_selector()->currentIndex()
						== aRun->EpochLeftRow) {
					control->right_selector()->setCurrentIndex(
						aRun->EpochRightRow);
					advance_fixture_stage(
						*aRun, automated_run::stage::epoch_identical);
				}
				break;
			}

			case automated_run::stage::epoch_identical: {
				const auto evidence = bpr::app::detail::
					github_application_fixture_access::evidence(aApplication);
				auto* const control = window.epoch_control();
				if (fixture_epoch_pair_passed(
						window, evidence.HeadTwoOid, evidence.HeadTwoOid)
					&& control->left_selector()->currentIndex()
						== aRun->EpochLeftRow
					&& control->right_selector()->currentIndex()
						== aRun->EpochRightRow) {
					aRun->PresentationPassed = true;
					aRun->RenderSaved        = save_render_evidence(window, aRun->RenderPath);
					transition_to_shutdown(aApplication, *aRun);
				}
				break;
			}

			case automated_run::stage::epoch_maintenance_history:
				if (window.epoch_control()->history_model()->epoch_count() >= 2U) {
					if (!begin_epoch_maintenance_probe(aApplication, *aRun)) {
						fail_automated_run(aApplication, *aRun);
					} else if (aRun->MaintenanceProbe
								   == automated_run::epoch_maintenance_probe::
									   verify_deleted
							   && aRun->Stage
								   == automated_run::stage::epoch_maintenance_history) {
						aRun->PresentationPassed = true;
						transition_to_shutdown(aApplication, *aRun);
					}
				}
				break;

			case automated_run::stage::epoch_maintenance_select_right: {
				auto* const control = window.epoch_control();
				if (control->left_selector()->currentIndex()
					!= aRun->MaintenanceLeftRow) {
					// The previous stage set this index and then advanced. An
					// epoch projection arriving in between repopulates the
					// model and clears the selection, and waiting for an index
					// that was already discarded never ends — which is what
					// made this contract fail about one run in seven.
					//
					// Re-asserting is what the selection needs: the row is
					// still the right one, it merely stopped being selected.
					// The attempt budget still bounds the wait if the row
					// genuinely cannot be selected.
					control->left_selector()->setCurrentIndex(
						aRun->MaintenanceLeftRow);
					break;
				}
				control->right_selector()->setCurrentIndex(
					aRun->MaintenanceRightRow);
				advance_fixture_stage(
					*aRun, automated_run::stage::epoch_maintenance_prune);
				break;
			}

			case automated_run::stage::epoch_maintenance_prune: {
				auto* const control = window.epoch_control();
				if (control->left_selector()->currentIndex()
						== aRun->MaintenanceLeftRow
					&& control->right_selector()->currentIndex()
						== aRun->MaintenanceRightRow
					&& control->prune_action()->isEnabled()) {
					accept_maintenance_confirmation(
						QStringLiteral("pruneSelectedEpochsConfirmationDialog"));
					control->prune_action()->trigger();
					advance_fixture_stage(
						*aRun,
						automated_run::stage::epoch_maintenance_prune_result);
				}
				break;
			}

			case automated_run::stage::epoch_maintenance_prune_result: {
				auto* const control  = window.epoch_control();
				const auto  provider = bpr::app::detail::
					github_application_fixture_access::evidence(aApplication);
				const auto maintenance = bpr::app::detail::
					github_application_fixture_access::maintenance_evidence(
						aApplication, aRun->MaintenanceTarget);
				if (fixture_epoch_availability_is(*control->history_model(),
												  1U,
												  bpr::core::anchor_availability::pruned)
					&& maintenance.PruneIntentDispatchCount == 1U
					&& maintenance.DeleteIntentDispatchCount == 0U
					&& maintenance.CatalogRevision >= 3
					&& !maintenance.TargetBaseRefPresent
					&& !maintenance.TargetHeadRefPresent
					&& maintenance.NonTargetBaseRefPresent
					&& maintenance.NonTargetHeadRefPresent
					&& maintenance.RepositoryPresent
					&& !maintenance.CacheDeletionMarkerPresent
					&& provider.AccountReadCount == aRun->MaintenanceAccountReads
					&& provider.SmartAcquireCount
						== aRun->MaintenanceSmartAcquires
					&& control->history_model()->statistics().ModelResets
						== aRun->MaintenanceModelResets) {
					aRun->PresentationPassed = true;
					transition_to_shutdown(aApplication, *aRun);
				}
				break;
			}

			case automated_run::stage::epoch_maintenance_delete_result: {
				auto* const control  = window.epoch_control();
				const auto  provider = bpr::app::detail::
					github_application_fixture_access::evidence(aApplication);
				const auto maintenance = bpr::app::detail::
					github_application_fixture_access::maintenance_evidence(
						aApplication, aRun->MaintenanceTarget);
				auto* const workbench = window.diff_comparison_workbench();
				if (fixture_all_epoch_availability_is(*control->history_model(),
													  bpr::core::anchor_availability::cache_deleted)
					&& maintenance.PruneIntentDispatchCount == 0U
					&& maintenance.DeleteIntentDispatchCount == 1U
					&& !maintenance.TargetBaseRefPresent
					&& !maintenance.TargetHeadRefPresent
					&& !maintenance.NonTargetBaseRefPresent
					&& !maintenance.NonTargetHeadRefPresent
					&& !maintenance.RepositoryPresent
					&& maintenance.CacheDeletionMarkerPresent
					&& maintenance.TargetBaseCacheDeleted
					&& maintenance.TargetHeadCacheDeleted
					&& provider.AccountReadCount == aRun->MaintenanceAccountReads
					&& provider.SmartAcquireCount
						== aRun->MaintenanceSmartAcquires
					&& control->history_model()->statistics().ModelResets
						== aRun->MaintenanceModelResets
					&& workbench->current_result() == nullptr
					&& !workbench->current_context().has_value()
					&& workbench->status_text()
						== QStringLiteral("Comparison invalidated")) {
					aRun->PresentationPassed = true;
					transition_to_shutdown(aApplication, *aRun);
				}
				break;
			}

			case automated_run::stage::appearance:
				if (
					aRun->Appearance.SignalObserved
					&& aRun->Appearance.ObservedColorScheme
						== Qt::ColorScheme::Dark
					&& ++aRun->Appearance.GlobalPollTicks >= 10U) {
					aRun->Appearance.GlobalPollTicks = 0U;
					const auto globalScheme          = macos_global_color_scheme();
					if (
						globalScheme
						!= aRun->Appearance.ObservedGlobalColorScheme) {
						aRun->Appearance.SettleAttempts = 0U;
					}
					aRun->Appearance.ObservedGlobalColorScheme = globalScheme;
				}
				if (
					aRun->Appearance.SignalObserved
					&& aRun->Appearance.ObservedColorScheme
						== Qt::ColorScheme::Dark
					&& aRun->Appearance.ObservedGlobalColorScheme
						== Qt::ColorScheme::Dark
					&& QGuiApplication::styleHints()->colorScheme()
						== Qt::ColorScheme::Dark
					&& ++aRun->Appearance.SettleAttempts >= 50U) {
					const auto* const appearanceWorkbench
						= window.diff_comparison_workbench();
					aRun->Appearance.ObservedDiffPalette
						= appearanceWorkbench->palette();
					aRun->Appearance.ObservedDiffFont
						= appearanceWorkbench->font();
					const auto finalGeometryRebuilds
						= appearanceWorkbench->statistics().GeometryRebuilds;
					aRun->Appearance.DiffGeometryRebuildDelta
						= finalGeometryRebuilds
							>= aRun->Appearance.InitialDiffGeometryRebuilds
						? finalGeometryRebuilds
							- aRun->Appearance.InitialDiffGeometryRebuilds
						: 0U;
					const auto stateBeforeRender = native_appearance_dark_state_passed(window, *aRun);
					const auto changedRender     = stateBeforeRender
						? capture_normalized_render(window)
						: QImage{ };
					const auto stateAfterRender  = stateBeforeRender && !changedRender.isNull()
						&& native_appearance_dark_state_passed(window, *aRun);
					aRun->Appearance.StatePassed  = stateAfterRender;
					aRun->Appearance.PixelsDiffer = aRun->Appearance.StatePassed
						&& aRun->Appearance.InitialRender.size()
							== changedRender.size()
						&& aRun->Appearance.InitialRender.devicePixelRatio()
							== changedRender.devicePixelRatio()
						&& !same_render_pixels(
														aRun->Appearance.InitialRender,
														changedRender);
					if (aRun->Appearance.PixelsDiffer) {
						aRun->Appearance.ChangedRender = changedRender;
					}
					// BPR-0114 proper. The scheme has moved and the mode is
					// still System, so the profile's variant must have moved
					// with it.
					aRun->Appearance.VariantAfterChange
						= static_cast<int>(aApplication.status().ProfileVariant);
					aRun->Appearance.ProfileFollowsPassed
						= aRun->Appearance.VariantBeforeChange
							== static_cast<int>(bpr::core::appearance_variant::light)
						&& aRun->Appearance.VariantAfterChange
							== static_cast<int>(bpr::core::appearance_variant::dark)
						&& aApplication.status().ProfileMode
							== bpr::core::appearance_mode::system;

					// And the other half, which needs no further help from the
					// owner: with the operating system still reporting Dark,
					// select an explicit Light mode and require the variant to
					// be light. An explicit mode that merely happened to agree
					// with the scheme would prove nothing, so this is asserted
					// while the two disagree.
					const auto explicitSelected = bpr::app::detail::
						github_application_fixture_access::select_appearance_mode(
							aApplication, bpr::core::appearance_mode::light);
					QApplication::processEvents();
					aRun->Appearance.VariantUnderExplicit
						= static_cast<int>(aApplication.status().ProfileVariant);
					const auto restored = bpr::app::detail::
						github_application_fixture_access::select_appearance_mode(
							aApplication, bpr::core::appearance_mode::system);
					QApplication::processEvents();
					aRun->Appearance.VariantBackOnSystem
						= static_cast<int>(aApplication.status().ProfileVariant);
					aRun->Appearance.ExplicitIgnoresPassed = explicitSelected
						&& restored
						&& QGuiApplication::styleHints()->colorScheme()
							== Qt::ColorScheme::Dark
						&& aRun->Appearance.VariantUnderExplicit
							== static_cast<int>(bpr::core::appearance_variant::light)
						&& aRun->Appearance.VariantBackOnSystem
							== static_cast<int>(bpr::core::appearance_variant::dark);

					aRun->PresentationPassed = aRun->Appearance.StatePassed
						&& aRun->Appearance.PixelsDiffer
						&& aRun->Appearance.ProfileFollowsPassed
						&& aRun->Appearance.ExplicitIgnoresPassed;
					transition_to_shutdown(aApplication, *aRun);
				}
				break;
#endif

			case automated_run::stage::shutdown:
				if (aApplication.status().ShutdownComplete) {
					finish_automated_run(aApplication, *aRun);
					return;
				}
				break;
			}
			QTimer::singleShot(10, &window, *tick);
		};
		QTimer::singleShot(0, &aApplication.window(), *tick);
	}

	void schedule_interactive_shutdown(
		bpr::app::github_application& aApplication)
	{
		aApplication.begin_shutdown();
		auto poll = std::make_shared<std::function<void()>>();
		*poll     = [&aApplication, poll] {
			const auto status = aApplication.status();
			if (status.ShutdownComplete) {
				QCoreApplication::exit(
					status.ShutdownCleanupFailed ? EXIT_FAILURE : EXIT_SUCCESS);
				return;
			}
			QTimer::singleShot(10, &aApplication.window(), *poll);
		};
		QTimer::singleShot(0, &aApplication.window(), *poll);
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication qtApplication{ argc, argv };
	QApplication::setQuitOnLastWindowClosed(false);
	QCoreApplication::setApplicationName(QStringLiteral("BetterPullReview"));
	QCoreApplication::setApplicationVersion(QStringLiteral(BPR_VERSION));
	QCoreApplication::setOrganizationName(
		QStringLiteral("BetterPullReview"));

	try {
		auto config = bpr::app::github_application_config{
			.EnterpriseHostname = std::nullopt,
		};
		if (const auto host = argument_value(argc, argv, "--ghes-host");
			host.has_value()) {
			config.EnterpriseHostname = std::string{ *host };
		}
		auto enterpriseHostname = config.EnterpriseHostname;

#if defined(BPR_BUILD_TESTING)
		auto fixtureName            = std::string{ };
		auto expectedActiveHostText = std::string{ };
		auto selectedFixture        = std::optional<
			bpr::app::detail::github_public_path_fixture>{ };
		auto fixtureOptions = bpr::app::detail::github_public_path_fixture_options{ };
		if (const auto fixture = argument_value(argc, argv, "--github-fixture");
			fixture.has_value()) {
			if (*fixture == "github-com") {
				selectedFixture        = bpr::app::detail::github_public_path_fixture::github_com;
				fixtureName            = "github.com";
				expectedActiveHostText = "Active host: github.com (GitHub.com)";
			} else if (*fixture == "ghes-3.18") {
				selectedFixture        = bpr::app::detail::github_public_path_fixture::ghes_3_18;
				fixtureName            = "ghes-3.18";
				expectedActiveHostText = "Active host: ghe.example.invalid "
										 "(GitHub Enterprise Server)";
			} else {
				QTextStream{ stdout } << "PUBLIC_PATH_FAILED\n";
				return EXIT_FAILURE;
			}
		}
		const auto epochFixtureRoot      = argument_value(argc, argv, "--epoch-fixture-root");
		const auto epochFixturePhase     = argument_value(argc, argv, "--epoch-fixture-phase");
		const auto epochMaintenanceProbe = argument_value(argc, argv, "--epoch-maintenance-probe");
		if (epochFixtureRoot.has_value() != epochFixturePhase.has_value()
			|| (epochFixtureRoot.has_value()
				&& !selectedFixture.has_value())) {
			QTextStream{ stdout }
				<< "EPOCH_FIXTURE_CONFIGURATION_FAILED\n";
			return EXIT_FAILURE;
		}
		if (epochFixtureRoot) {
			fixtureOptions.ApplicationDataRoot = std::filesystem::path{ std::string{ *epochFixtureRoot } };
			if (*epochFixturePhase == "seed") {
				fixtureOptions.EpochPhase = bpr::app::detail::github_epoch_fixture_phase::seed;
			} else if (*epochFixturePhase == "reuse") {
				fixtureOptions.EpochPhase = bpr::app::detail::github_epoch_fixture_phase::reuse;
			} else if (*epochFixturePhase == "single") {
				// A persistent application data root with no epoch phase. The
				// enum already had this state and it was the default; naming
				// it is what lets a probe that cares about durable settings
				// rather than about epochs choose a root, without a second
				// flag meaning the same thing or a weakened root/phase gate.
				fixtureOptions.EpochPhase = bpr::app::detail::github_epoch_fixture_phase::single;
			} else {
				QTextStream{ stdout }
					<< "EPOCH_FIXTURE_CONFIGURATION_FAILED\n";
				return EXIT_FAILURE;
			}
		}
		auto selectedMaintenanceProbe = automated_run::epoch_maintenance_probe::none;
		if (epochMaintenanceProbe) {
			if (!epochFixtureRoot
				|| fixtureOptions.EpochPhase
					!= bpr::app::detail::github_epoch_fixture_phase::reuse) {
				QTextStream{ stdout }
					<< "EPOCH_MAINTENANCE_CONFIGURATION_FAILED\n";
				return EXIT_FAILURE;
			}
			if (*epochMaintenanceProbe == "prune") {
				selectedMaintenanceProbe = automated_run::epoch_maintenance_probe::prune;
			} else if (*epochMaintenanceProbe == "verify-pruned-delete") {
				selectedMaintenanceProbe = automated_run::
					epoch_maintenance_probe::verify_pruned_and_delete;
			} else if (*epochMaintenanceProbe == "verify-deleted") {
				selectedMaintenanceProbe = automated_run::epoch_maintenance_probe::verify_deleted;
			} else {
				QTextStream{ stdout }
					<< "EPOCH_MAINTENANCE_CONFIGURATION_FAILED\n";
				return EXIT_FAILURE;
			}
		}
#endif

		const auto selfTest   = has_argument(argc, argv, "--self-test");
		const auto renderPath = argument_value(argc, argv, "--render-evidence");

		// The harness that starts this process knows its own timeout; this is
		// how it tells the run to stop first. Refused rather than clamped when
		// it does not parse: a budget silently replaced by the default is one
		// the harness believes it set, and the first thing anyone would learn
		// is a timeout with nothing reported — the failure this exists to end.
		auto automatedRunBudget = default_automated_run_budget;
		if (const auto budget
			= argument_value(argc, argv, "--automated-run-budget-ms");
			budget.has_value()) {
			auto              milliseconds = 0LL;
			const auto* const first        = budget->data();
			const auto* const last         = first + budget->size();
			const auto        parsed       = std::from_chars(first, last, milliseconds);
			if (parsed.ec != std::errc{ } || parsed.ptr != last
				|| milliseconds <= 0) {
				QTextStream{ stdout }
					<< "AUTOMATED_RUN_BUDGET_CONFIGURATION_FAILED\n";
				return EXIT_FAILURE;
			}
			automatedRunBudget = std::chrono::milliseconds{ milliseconds };
		}
#if defined(BPR_BUILD_TESTING)
		const auto automatedStallProbe = has_argument(
			argc, argv, "--automated-stall-probe");
		const auto nativeAppearanceProbe      = has_argument(argc, argv, "--native-appearance-probe");
		const auto attendedLaunch             = has_argument(argc, argv, "--attended-launch");
		const auto diffWorkbenchProbe         = has_argument(argc, argv, "--diff-workbench-probe");
		const auto diffWorkbenchShutdownProbe = has_argument(
			argc, argv, "--diff-workbench-shutdown-probe");
		const auto appearanceGestureProbe = has_argument(
			argc, argv, "--appearance-gesture-probe");
		const auto diffEvidenceAppearanceName = argument_value(
			argc, argv, "--diff-evidence-appearance");
		// Named rather than boolean, so adding a scheme later is a new name and
		// not a new flag. An unrecognised name is refused rather than ignored:
		// a capture silently taken under the wrong appearance is exactly the
		// confusion BPR-0201 cost a session to unpick.
		const auto diffEvidenceAppearance
			= [&diffEvidenceAppearanceName]()
			-> std::optional<std::optional<bpr::core::appearance_mode>> {
			if (!diffEvidenceAppearanceName.has_value()) {
				return std::optional<bpr::core::appearance_mode>{ };
			}
			if (*diffEvidenceAppearanceName == "light") {
				return bpr::core::appearance_mode::light;
			}
			if (*diffEvidenceAppearanceName == "dark") {
				return bpr::core::appearance_mode::dark;
			}
			if (*diffEvidenceAppearanceName == "system") {
				return bpr::core::appearance_mode::system;
			}
			if (*diffEvidenceAppearanceName == "high-contrast-light") {
				return bpr::core::appearance_mode::high_contrast_light;
			}
			if (*diffEvidenceAppearanceName == "high-contrast-dark") {
				return bpr::core::appearance_mode::high_contrast_dark;
			}
			if (*diffEvidenceAppearanceName == "high-contrast-system") {
				return bpr::core::appearance_mode::high_contrast_system;
			}
			return std::nullopt;
		}();
		if (!diffEvidenceAppearance.has_value()) {
			QTextStream{ stdout }
				<< "DIFF_EVIDENCE_APPEARANCE_CONFIGURATION_FAILED\n";
			return EXIT_FAILURE;
		}

		const auto diffEvidenceDirectory = argument_value(
			argc, argv, "--diff-interaction-evidence");
		const auto changedRenderPath     = argument_value(argc, argv, "--changed-render-evidence");
		auto       nativeAppearancePaths = std::optional<native_appearance_evidence_paths>{ };
		if (nativeAppearanceProbe) {
			nativeAppearancePaths = resolve_native_appearance_evidence_paths(
				renderPath,
				changedRenderPath);
		}
		if (
			nativeAppearanceProbe
			&& (!selectedFixture.has_value()
				|| *selectedFixture
					!= bpr::app::detail::
						github_public_path_fixture::github_com
				|| !nativeAppearancePaths.has_value())) {
			QTextStream{ stdout }
				<< "NATIVE_APPEARANCE_CONFIGURATION_FAILED\n";
			return EXIT_FAILURE;
		}
		if (!nativeAppearanceProbe && changedRenderPath.has_value()) {
			QTextStream{ stdout }
				<< "NATIVE_APPEARANCE_CONFIGURATION_FAILED\n";
			return EXIT_FAILURE;
		}
		if (nativeAppearanceProbe
			&& fixtureOptions.EpochPhase
				!= bpr::app::detail::github_epoch_fixture_phase::single) {
			QTextStream{ stdout }
				<< "EPOCH_FIXTURE_CONFIGURATION_FAILED\n";
			return EXIT_FAILURE;
		}
		// An attended launch and an automated run are contradictory
		// instructions about who is driving, so the pair is refused here rather
		// than letting one quietly win. It also requires a fixture: without one
		// the composition takes the real credentialed path, which is not a
		// thing to hold open for inspection.
		if (attendedLaunch
			&& (selfTest || renderPath.has_value() || fixtureName.empty()
				|| diffWorkbenchProbe || diffWorkbenchShutdownProbe
				|| appearanceGestureProbe || automatedStallProbe
				|| epochMaintenanceProbe || nativeAppearanceProbe
				|| diffEvidenceDirectory.has_value())) {
			QTextStream{ stdout } << "ATTENDED_LAUNCH_CONFIGURATION_FAILED\n";
			return EXIT_FAILURE;
		}

		if (
			(diffWorkbenchProbe && diffWorkbenchShutdownProbe)
			|| ((diffWorkbenchProbe || diffWorkbenchShutdownProbe)
				&& (!selectedFixture.has_value()
					|| *selectedFixture
						!= bpr::app::detail::github_public_path_fixture::github_com
					|| fixtureOptions.EpochPhase
						!= bpr::app::detail::github_epoch_fixture_phase::single))) {
			QTextStream{ stdout }
				<< "DIFF_WORKBENCH_CONFIGURATION_FAILED\n";
			return EXIT_FAILURE;
		}
		if (
			(diffWorkbenchProbe && !diffEvidenceDirectory.has_value())
			|| (!diffWorkbenchProbe && diffEvidenceDirectory.has_value())) {
			QTextStream{ stdout }
				<< "DIFF_WORKBENCH_CONFIGURATION_FAILED\n";
			return EXIT_FAILURE;
		}
		// The gesture probe drives the composed settings surface, which only
		// exists on the fixture window. Refused rather than ignored, so a
		// mistyped invocation cannot report a silent success.
		if (appearanceGestureProbe && !selectedFixture.has_value()) {
			QTextStream{ stdout }
				<< "APPEARANCE_GESTURE_CONFIGURATION_FAILED\n";
			return EXIT_FAILURE;
		}
#endif

		auto application =
#if defined(BPR_BUILD_TESTING)
			selectedFixture.has_value()
			? bpr::app::detail::github_application_fixture_access::make(
				  *selectedFixture, fixtureOptions)
			:
#endif
			std::make_unique<bpr::app::github_application>(
				std::move(config));
		application->show();

		QObject::connect(
			&qtApplication,
			&QGuiApplication::lastWindowClosed,
			&application->window(),
			[&application] { schedule_interactive_shutdown(*application); });

		auto verifyNativePlatformVault = selfTest;
		auto verifyCliUnavailable      = selfTest;
#if defined(BPR_BUILD_TESTING)
		verifyNativePlatformVault = verifyNativePlatformVault && fixtureName.empty();
		verifyCliUnavailable      = verifyCliUnavailable && fixtureName.empty();
#endif
		// The person is the run. Everything else about the composition is
		// unchanged - same fixture-backed application, same window - so what
		// is inspected is what the automated courses drive.
		//
		// Guarded, not merely false, in a tests-disabled build: the flag does
		// not exist there and neither does the fixture it requires, so the
		// seam leaves no trace for the Release composition contract to find.
#if defined(BPR_BUILD_TESTING)
		const auto heldOpenForInspection = attendedLaunch;
#else
		constexpr auto heldOpenForInspection = false;
#endif
		const auto scheduleAutomatedRun = !heldOpenForInspection
			&& (selfTest || renderPath.has_value()
#if defined(BPR_BUILD_TESTING)
				|| !fixtureName.empty()
#endif
			);
		if (scheduleAutomatedRun) {
			schedule_automated_run(
				*application,
				std::make_shared<automated_run>(
					automated_run{
						.Stage                     = automated_run::stage::start,
						.Attempts                  = 0U,
						.Budget                    = automatedRunBudget,
						.PresentationPassed        = false,
						.RenderSaved               = true,
						.VerifyNativePlatformVault = verifyNativePlatformVault,
						.Cli                       = automated_run::cli_unavailable_probe_state{
							.Enabled            = verifyCliUnavailable,
							.EnterpriseHostname = std::move(enterpriseHostname),
						},
#if defined(BPR_BUILD_TESTING)
						.Fixture                = !fixtureName.empty(),
						.AppearanceGestureProbe = appearanceGestureProbe,
						.DiffProbe              = diffWorkbenchProbe,
						.DiffShutdownProbe      = diffWorkbenchShutdownProbe,
						.StallProbe             = automatedStallProbe,
						.FixtureName            = std::move(fixtureName),
						.ExpectedActiveHostText = std::move(expectedActiveHostText),
						.EpochPhase             = fixtureOptions.EpochPhase,
						.MaintenanceProbe       = selectedMaintenanceProbe,
						.Appearance             = automated_run::native_appearance_probe_state{
							.Enabled       = nativeAppearanceProbe,
							.EvidencePaths = nativeAppearancePaths.value_or(native_appearance_evidence_paths{ }),
						},
						.DiffEvidenceDirectory  = diffEvidenceDirectory,
						.DiffEvidenceAppearance = *diffEvidenceAppearance,
#endif
						.RenderPath = renderPath,
					}));
		}

		return QApplication::exec();
	} catch (...) {
		QTextStream{ stdout } << "STARTUP_FAILED\n";
		return EXIT_FAILURE;
	}
}
