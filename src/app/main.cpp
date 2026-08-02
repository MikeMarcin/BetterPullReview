#include "bpr/app/github_application.hpp"

#if defined(BPR_BUILD_TESTING)
	#include "github_application_fixture_access.hpp"

	#if defined(__APPLE__)
		#include <CoreFoundation/CoreFoundation.h>
	#endif
#endif

#include <QAction>
#include <QApplication>
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
	#include <QGuiApplication>
	#include <QImage>
	#include <QItemSelectionModel>
	#include <QMessageBox>
	#include <QPalette>
	#include <QPersistentModelIndex>
	#include <QPushButton>
	#include <QStyle>
	#include <QStyleHints>
	#include <QTreeView>
#endif

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#if defined(BPR_BUILD_TESTING)
	#include <cstring>
#endif

namespace {
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

		stage                       Stage                     = stage::start;
		std::size_t                 Attempts                  = 0U;
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
			bool                             Enabled                      = false;
			bool                             WaitingForOwnerFocusReported = false;
			bool                             SignalObserved               = false;
			bool                             StatePassed                  = false;
			bool                             PixelsDiffer                 = false;
			std::size_t                      SettleAttempts               = 0U;
			std::size_t                      GlobalPollTicks              = 0U;
			Qt::ColorScheme                  InitialColorScheme           = Qt::ColorScheme::Unknown;
			Qt::ColorScheme                  ObservedColorScheme          = Qt::ColorScheme::Unknown;
			Qt::ColorScheme                  InitialGlobalColorScheme     = Qt::ColorScheme::Unknown;
			Qt::ColorScheme                  ObservedGlobalColorScheme    = Qt::ColorScheme::Unknown;
			QPalette                         InitialPalette;
			QString                          InitialPlatformName;
			QString                          InitialStyleClass;
			QPersistentModelIndex            Thread;
			bool                             InitialWindowActive = false;
			QWidget*                         InitialFocus        = nullptr;
			std::size_t                      InitialModelResets  = 0U;
			QString                          ExpectedEpochLeftOid;
			QString                          ExpectedEpochRightOid;
			QPersistentModelIndex            EpochLeft;
			QPersistentModelIndex            EpochRight;
			QString                          EpochLeftIdentity;
			QString                          EpochRightIdentity;
			QPalette                         InitialEpochPalette;
			std::size_t                      InitialEpochModelResets       = 0U;
			std::size_t                      InitialEpochAppearanceUpdates = 0U;
			QImage                           InitialRender;
			QImage                           ChangedRender;
			native_appearance_evidence_paths EvidencePaths;
		};

		bool                                         Fixture = false;
		std::string                                  FixtureName;
		std::string                                  ExpectedActiveHostText;
		bpr::app::detail::github_epoch_fixture_phase EpochPhase = bpr::app::detail::github_epoch_fixture_phase::single;
		enum class epoch_maintenance_probe {
			none,
			prune,
			verify_pruned_and_delete,
			verify_deleted,
		};
		epoch_maintenance_probe       MaintenanceProbe = epoch_maintenance_probe::none;
		bpr::core::review_epoch_id    MaintenanceTarget;
		int                           MaintenanceLeftRow       = -1;
		int                           MaintenanceRightRow      = -1;
		int                           MaintenanceAccountReads  = 0;
		int                           MaintenanceSmartAcquires = 0;
		std::size_t                   MaintenanceModelResets   = 0U;
		int                           EpochLeftRow             = -1;
		int                           EpochRightRow            = -1;
		native_appearance_probe_state Appearance;
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
		const auto& finalPalette       = aWindow.palette();
		const auto  initialWindow      = aRun.Appearance.InitialPalette.color(QPalette::Window).lightness();
		const auto  initialBase        = aRun.Appearance.InitialPalette.color(QPalette::Base).lightness();
		const auto  initialText        = aRun.Appearance.InitialPalette.color(QPalette::Text).lightness();
		const auto  finalWindow        = finalPalette.color(QPalette::Window).lightness();
		const auto  finalBase          = finalPalette.color(QPalette::Base).lightness();
		const auto  finalText          = finalPalette.color(QPalette::Text).lightness();
		const auto  initialEpoch       = aRun.Appearance.InitialEpochPalette;
		const auto  finalEpoch         = aWindow.epoch_control()->palette();
		const auto  initialEpochWindow = initialEpoch.color(QPalette::Window).lightness();
		const auto  initialEpochBase   = initialEpoch.color(QPalette::Base).lightness();
		const auto  initialEpochText   = initialEpoch.color(QPalette::Text).lightness();
		const auto  finalEpochWindow   = finalEpoch.color(QPalette::Window).lightness();
		const auto  finalEpochBase     = finalEpoch.color(QPalette::Base).lightness();
		const auto  finalEpochText     = finalEpoch.color(QPalette::Text).lightness();

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
			&& aWindow.epoch_control()->statistics().AppearanceUpdates
			> aRun.Appearance.InitialEpochAppearanceUpdates
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
		view->setCurrentIndex(thread);
		view->selectionModel()->select(
			thread,
			QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
		view->collapse(thread);
		QApplication::processEvents();
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
#endif
		if (aRun.RenderPath.has_value()) {
			stream << (renderSaved ? "RENDER_OK\n" : "RENDER_FAILED\n");
		}
#if defined(BPR_BUILD_TESTING)
		if (aRun.Appearance.Enabled) {
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
		aRun.PresentationPassed = false;
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

		auto tick = std::make_shared<std::function<void()>>();
		*tick     = [&aApplication, aRun, tick] {
			auto&      window = aApplication.window();
			const auto maximumAttempts =
#if defined(BPR_BUILD_TESTING)
				aRun->Fixture
					&& (aRun->Stage == automated_run::stage::start
						|| aRun->Stage == automated_run::stage::connect
						|| aRun->Stage == automated_run::stage::open
						|| aRun->Stage == automated_run::stage::epoch_history
						|| aRun->Stage == automated_run::stage::epoch_refresh
						|| aRun->Stage
							== automated_run::stage::epoch_maintenance_history
						|| aRun->Stage
							== automated_run::stage::epoch_maintenance_prune_result
						|| aRun->Stage
							== automated_run::stage::epoch_maintenance_delete_result
						|| aRun->Stage == automated_run::stage::appearance)
				? 6'000U
				:
#endif
				1'000U;
			if (aRun->Stage != automated_run::stage::shutdown
				&& ++aRun->Attempts > maximumAttempts) {
				capture_automated_timeout_evidence(aApplication, *aRun);
			}

			switch (aRun->Stage) {
			case automated_run::stage::start:
#if defined(BPR_BUILD_TESTING)
				if (aRun->Fixture) {
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
						const auto waitingForOwnerFocus        = QGuiApplication::platformName() == "cocoa"
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
							aRun->Stage = automated_run::stage::appearance;
						} else {
							fail_automated_run(aApplication, *aRun);
						}
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
					== aRun->MaintenanceLeftRow) {
					control->right_selector()->setCurrentIndex(
						aRun->MaintenanceRightRow);
					advance_fixture_stage(
						*aRun, automated_run::stage::epoch_maintenance_prune);
				}
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
						== aRun->MaintenanceModelResets) {
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
					aRun->PresentationPassed = aRun->Appearance.StatePassed
						&& aRun->Appearance.PixelsDiffer;
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
#if defined(BPR_BUILD_TESTING)
		const auto nativeAppearanceProbe = has_argument(argc, argv, "--native-appearance-probe");
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
		if (selfTest || renderPath.has_value()
#if defined(BPR_BUILD_TESTING)
			|| !fixtureName.empty()
#endif
		) {
			schedule_automated_run(
				*application,
				std::make_shared<automated_run>(
					automated_run{
						.Stage                     = automated_run::stage::start,
						.Attempts                  = 0U,
						.PresentationPassed        = false,
						.RenderSaved               = true,
						.VerifyNativePlatformVault = verifyNativePlatformVault,
						.Cli                       = automated_run::cli_unavailable_probe_state{
							.Enabled            = verifyCliUnavailable,
							.EnterpriseHostname = std::move(enterpriseHostname),
						},
#if defined(BPR_BUILD_TESTING)
						.Fixture                = !fixtureName.empty(),
						.FixtureName            = std::move(fixtureName),
						.ExpectedActiveHostText = std::move(expectedActiveHostText),
						.EpochPhase             = fixtureOptions.EpochPhase,
						.MaintenanceProbe       = selectedMaintenanceProbe,
						.Appearance             = automated_run::native_appearance_probe_state{
							.Enabled       = nativeAppearanceProbe,
							.EvidencePaths = nativeAppearancePaths.value_or(native_appearance_evidence_paths{ }),
						},
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
