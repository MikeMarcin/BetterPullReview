#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/app/github_application.hpp"
#include "bpr/app/native_filesystem_path.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/bundled_code_font.hpp"
#include "bpr/qt/device_authorization_panel.hpp"
#include "bpr/qt/diff_link_map.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/epoch_history_model.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/line_details_pane.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/review_landing_model.hpp"
#include "bpr/qt/review_submission_editor.hpp"
#include "bpr/qt/suggestion_mark.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "src/installed_resources.hpp"
#include "src/installed_runtime.hpp"
#include "src/terminal_diagnostics.hpp"
#if defined(__linux__)
	#include "src/xcb_activation_host.hpp"
#endif

#if defined(BPR_BUILD_TESTING)
	#include "bpr/core/review_tour_markdown.hpp"
	#include "bpr/qt/finding_queue_view.hpp"
	#include "bpr/qt/workshop_candidate_line.hpp"
	#include "bpr/qt/writing_hooks_panel.hpp"
	#include "github_application_fixture_access.hpp"
	#include "src/private_review_fixture_course.hpp"
	#include "src/review_submission_fixture.hpp"
	#include "src/startup_diagnostic.hpp"
#endif

#if defined(__APPLE__)
	// Outside the tests-enabled guard because the unguarded evidence block
	// reports the running process's bundle identity, so both configurations
	// compile a CoreFoundation call. Both also link the framework.
	#include <CoreFoundation/CoreFoundation.h>
#endif

#if defined(__APPLE__) && !defined(BPR_BUILD_TESTING)
	#include "designated_live_macos_provenance.hpp"
#endif

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontInfo>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QSize>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QWindow>

#if defined(BPR_BUILD_TESTING)
	#include <QFont>
	#include <QGuiApplication>
	#include <QImage>
	#include <QItemSelectionModel>
	#include <QKeyEvent>
	#include <QListView>
	#include <QMessageBox>
	#include <QMouseEvent>
	#include <QPalette>
	#include <QPersistentModelIndex>
	#include <QPlainTextEdit>
	#include <QScrollArea>
	#include <QSplitter>
	#include <QStyle>
	#include <QStyleHints>
	#include <QSystemTrayIcon>
	#include <QTextBlock>
	#include <QTextCursor>
	#include <QTreeView>
	#include <QWheelEvent>
#endif

#include <algorithm>
#include <array>
#include <cerrno>
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
#include <system_error>
#include <vector>

#if defined(__APPLE__) && !defined(BPR_BUILD_TESTING)
	#include <unistd.h>
#endif

#if defined(BPR_BUILD_TESTING)
	#include <cstring>
#endif

namespace {
	/// \brief [[BPR-1334]]. Whether Line details shows the selection's line.
	[[maybe_unused]] [[nodiscard]] auto line_details_show_selection(const bpr::qt::diff_workbench& aWorkbench) -> bool
	{
		const auto& selection = aWorkbench.selection();
		if (!selection.has_value() || aWorkbench.line_details() == nullptr) {
			return false;
		}
		return std::ranges::any_of(aWorkbench.line_details()->projection().Rows, [&selection](const bpr::core::line_details_row& aRow) {
			return aRow.Side == selection->PreferredLine.Side && aRow.Line == std::optional{ selection->PreferredLine.Line };
		});
	}

	struct profile_launch_arguments
	{
		bpr::core::bpr_profile_id        Profile = bpr::core::default_bpr_profile_id;
		bpr::core::profile_launch_intent Intent  = bpr::core::profile_launch_intent::activate;
	};

	[[nodiscard]] auto parse_profile_launch_arguments(int aArgc, char** aArgv) -> std::optional<profile_launch_arguments>
	{
		auto result      = profile_launch_arguments{ };
		bool profileSeen = false;
		bool windowSeen  = false;
		for (int index = 1; index < aArgc; ++index) {
			const auto argument = std::string_view{ aArgv[index] };
			if (argument == "--new-window") {
				if (windowSeen) {
					return std::nullopt;
				}
				windowSeen    = true;
				result.Intent = bpr::core::profile_launch_intent::new_window;
			} else if (argument == "--bpr-profile") {
				if (profileSeen || ++index == aArgc) {
					return std::nullopt;
				}
				profileSeen      = true;
				const auto value = std::string_view{ aArgv[index] };
				auto       id    = std::uint64_t{ };
				if (value.empty() || !std::ranges::all_of(value, [](char aDigit) { return aDigit >= '0' && aDigit <= '9'; })) {
					return std::nullopt;
				}
				const auto parsed = std::from_chars(value.data(), value.data() + value.size(), id);
				if (parsed.ec != std::errc{ } || parsed.ptr != value.data() + value.size() || id == 0) {
					return std::nullopt;
				}
				result.Profile = bpr::core::bpr_profile_id{ id };
			} else if (argument.starts_with("--bpr-profile=") || argument.starts_with("--new-window=")) {
				return std::nullopt;
			}
		}
		return result;
	}
	using bpr::app::native_filesystem_path;

#if defined(__APPLE__) && !defined(BPR_BUILD_TESTING)
	constexpr auto designated_live_child_channel = 3;
	constexpr auto designated_live_probe_argument
		= std::string_view{ "--designated-live-preflight-probe" };
	constexpr auto designated_live_probe_acknowledgement
		= std::string_view{ "BPR_DESIGNATED_LIVE_PREFLIGHT_OK\n" };

	[[nodiscard]] auto run_designated_live_preflight_probe(
		const int argc, char** argv) noexcept -> std::optional<int>
	{
		if (argc != 2 || argv == nullptr || argv[1] == nullptr
			|| std::string_view{ argv[1] } != designated_live_probe_argument) {
			return std::nullopt;
		}
		auto challenge = std::array<char, 65>{ };
		auto received  = std::size_t{ 0U };
		while (received != challenge.size()) {
			const auto count = ::read(
				designated_live_child_channel,
				challenge.data() + received,
				challenge.size() - received);
			if (count < 0 && errno == EINTR) {
				continue;
			}
			if (count <= 0) {
				return EXIT_FAILURE;
			}
			received += static_cast<std::size_t>(count);
		}
		const auto expected = std::string{ BPR_ATTENDED_REVIEW_SOURCE_SHA256 } + "\n";
		if (std::string_view{ challenge.data(), challenge.size() } != expected) {
			return EXIT_FAILURE;
		}
		auto written = std::size_t{ 0U };
		while (written != designated_live_probe_acknowledgement.size()) {
			const auto count = ::write(
				designated_live_child_channel,
				designated_live_probe_acknowledgement.data() + written,
				designated_live_probe_acknowledgement.size() - written);
			if (count < 0 && errno == EINTR) {
				continue;
			}
			if (count <= 0) {
				return EXIT_FAILURE;
			}
			written += static_cast<std::size_t>(count);
		}
		return EXIT_SUCCESS;
	}
#endif

	// Defined outside the tests-enabled guard because the evidence block that
	// prints NOTIFICATION_PERMISSION is itself unguarded: a Release build with
	// tests disabled still reports it. It lived inside the guard, so that build
	// did not compile — unnoticed, because the contract that would have caught
	// it could not fail.
	[[nodiscard]] auto notification_permission_name(
		const bpr::application::notification_permission aPermission) -> QString
	{
		switch (aPermission) {
		case bpr::application::notification_permission::not_determined:
			return QStringLiteral("not-determined");
		case bpr::application::notification_permission::granted:
			return QStringLiteral("granted");
		case bpr::application::notification_permission::platform_managed:
			return QStringLiteral("platform-managed");
		case bpr::application::notification_permission::refused:
			return QStringLiteral("refused");
		}
		return QStringLiteral("unknown");
	}

	/// \brief The bundle identifier of the running process, or why there is none.
	/// \details macOS Notification Center registers applications by bundle
	/// identity: a process without one cannot request authorization, never gains
	/// a System Settings entry, and its delivery requests go nowhere. The
	/// permission above is therefore not the whole precondition, and reporting it
	/// alone hid an entire attended course behind a plausible
	/// `not-determined` (BPR-0392). Reported rather than asserted, because the
	/// deterministic course substitutes the display boundary and so cannot
	/// notice. Unguarded for the same reason as `notification_permission_name`,
	/// and answered on every platform so a contract can match it without
	/// branching: `not-applicable` where the question does not arise.
	[[nodiscard]] auto notification_bundle_identity() -> QString
	{
#if defined(__APPLE__)
		const auto bundle = CFBundleGetMainBundle();
		if (bundle == nullptr) {
			return QStringLiteral("none");
		}
		// Non-null for an unbundled executable too - CoreFoundation synthesizes a
		// main bundle from the executable's directory - but with no identifier,
		// which is exactly the condition that cannot reach Notification Center.
		const auto identifier = CFBundleGetIdentifier(bundle);
		if (identifier == nullptr) {
			return QStringLiteral("none");
		}
		// The direct pointer is an optimization CoreFoundation may decline for
		// any internal representation, so the bounded copy is a normal path and
		// not a malformed-input fallback.
		if (const auto* const borrowed
			= CFStringGetCStringPtr(identifier, kCFStringEncodingUTF8);
			borrowed != nullptr) {
			return QString::fromUtf8(borrowed);
		}
		auto buffer = std::array<char, 512>{ };
		if (CFStringGetCString(
				identifier,
				buffer.data(),
				static_cast<CFIndex>(buffer.size()),
				kCFStringEncodingUTF8)
			== 0) {
			return QStringLiteral("none");
		}
		return QString::fromUtf8(buffer.data());
#else
		return QStringLiteral("not-applicable");
#endif
	}

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

	[[nodiscard]] auto application_icon() -> QIcon
	{
		struct icon_resource
		{
			const char* Path;
			int         Size;
		};
		constexpr auto resources = std::array{
			icon_resource{ ":/icons/16x16/better_pull_review.png", 16 },
			icon_resource{ ":/icons/24x24/better_pull_review.png", 24 },
			icon_resource{ ":/icons/32x32/better_pull_review.png", 32 },
			icon_resource{ ":/icons/40x40/better_pull_review.png", 40 },
			icon_resource{ ":/icons/48x48/better_pull_review.png", 48 },
			icon_resource{ ":/icons/64x64/better_pull_review.png", 64 },
			icon_resource{ ":/icons/128x128/better_pull_review.png", 128 },
			icon_resource{ ":/icons/256x256/better_pull_review.png", 256 },
			icon_resource{ ":/icons/512x512/better_pull_review.png", 512 },
		};
		auto icon = QIcon{ };
		for (const auto& resource : resources) {
			icon.addFile(
				QString::fromLatin1(resource.Path),
				QSize{ resource.Size, resource.Size });
		}
		return icon;
	}

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

	/// \brief Whether this launch is an automated run that composes its own
	/// private temporary profile rather than the default application data root.
	///
	/// \details [[BPR-0820]]. An automated run (`--self-test` or
	/// `--render-evidence`) is never a person's session, so it must not compete
	/// for the interactive owner's profile lease or read and write that
	/// profile's stores. This used to hold only for a tests-disabled build; a
	/// tests-enabled self-test with no fixture composed the default root,
	/// printed `PROFILE_ALREADY_OPEN` while a person's application was open,
	/// and otherwise opened that person's profile.
	///
	/// The exceptions each name their root some other way. A tests-enabled
	/// fixture composes its own (a temporary one, or `--epoch-fixture-root`),
	/// and `--test-application-data-root` is the persistent root a restart
	/// contract chooses. A tests-disabled attended approval deliberately uses
	/// the default root, which is where its replay state lives.
	[[nodiscard]] auto automated_run_uses_private_profile(
		const int          aArgc,
		char* const* const aArgv) -> bool
	{
		if (!has_argument(aArgc, aArgv, "--self-test")
			&& !has_argument(aArgc, aArgv, "--render-evidence")) {
			return false;
		}
#if defined(BPR_BUILD_TESTING)
		return !has_argument(aArgc, aArgv, "--github-fixture")
			&& !has_argument(aArgc, aArgv, "--test-application-data-root");
#else
		return !has_argument(aArgc, aArgv, "--attended-designated-live-approval");
#endif
	}

#if defined(BPR_BUILD_TESTING)
	/// \brief Reads a fixed run of values that follow one flag.
	///
	/// \details [[BPR-0316]]'s live candidate names an owner, a repository and
	/// a number in one place, so the three are read together or not at all: a
	/// partial run is no candidate rather than a candidate with a default in
	/// it.
	auto argument_values(
		const int              aArgc,
		char* const* const     aArgv,
		const std::string_view aExpected,
		const int              aCount)
		-> std::optional<std::vector<std::string_view>>
	{
		for (auto index = 1; index < aArgc; ++index) {
			if (std::string_view{ aArgv[index] } != aExpected) {
				continue;
			}
			if (index + aCount >= aArgc) {
				return std::nullopt;
			}
			auto values = std::vector<std::string_view>{ };
			values.reserve(static_cast<std::size_t>(aCount));
			for (auto offset = 1; offset <= aCount; ++offset) {
				values.emplace_back(aArgv[index + offset]);
			}
			return values;
		}
		return std::nullopt;
	}
#endif

	/// \brief Saves the evidence render, optionally with the files drawer shut.
	///
	/// \details Collapsing happens through the shipped toggle action, so the
	/// capture shows a state the product can actually be in rather than one
	/// the harness arranged behind the core's back. Requested for BPR-0414's
	/// second attended round, where the owner asked for the drawer out of the
	/// way so more source is visible.
	auto save_render_evidence(
		bpr::qt::workbench_window&             aWindow,
		const std::optional<std::string_view>& aRenderPath,
		const bool                             aCollapseFilesDrawer) -> bool
	{
		if (!aRenderPath.has_value()) {
			return true;
		}
		if (aCollapseFilesDrawer) {
			if (auto* const toggle = aWindow.files_drawer_toggle_action();
				toggle != nullptr && toggle->isChecked()) {
				toggle->trigger();
				QApplication::processEvents();
			}
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

	enum class native_appearance_capture_disposition {
		not_attempted_state_precondition,
		window_grab_null,
		normalization_failed,
		captured,
	};

	struct native_appearance_capture_result
	{
		QImage                                Image;
		native_appearance_capture_disposition Disposition
			= native_appearance_capture_disposition::not_attempted_state_precondition;
	};

	enum class native_appearance_persistence_disposition {
		not_attempted,
		initial_write_failed,
		changed_write_failed,
		persisted_readback_failed,
		persisted,
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
		bpr::qt::workbench_window& aWindow)
		-> native_appearance_capture_result
	{
		const auto windowGrab = aWindow.grab();
		if (windowGrab.isNull()) {
			return {
				.Disposition
				= native_appearance_capture_disposition::window_grab_null,
			};
		}

		auto image = windowGrab.toImage();
		if (image.isNull()) {
			return {
				.Disposition
				= native_appearance_capture_disposition::normalization_failed,
			};
		}
		const auto devicePixelRatio = image.devicePixelRatio();
		image                       = image.convertToFormat(QImage::Format_RGBA8888);
		if (image.isNull()) {
			return {
				.Disposition
				= native_appearance_capture_disposition::normalization_failed,
			};
		}
		image.setDevicePixelRatio(devicePixelRatio);
		return {
			.Image       = std::move(image),
			.Disposition = native_appearance_capture_disposition::captured,
		};
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
		const native_appearance_evidence_paths& aPaths)
		-> native_appearance_persistence_disposition
	{
		if (!save_new_png(aInitial, aPaths.Initial)) {
			return native_appearance_persistence_disposition::initial_write_failed;
		}
		if (!save_new_png(aChanged, aPaths.Changed)) {
			QFile::remove(aPaths.Initial);
			return native_appearance_persistence_disposition::changed_write_failed;
		}

		const auto persistedInitial = QImage{ aPaths.Initial, "PNG" };
		const auto persistedChanged = QImage{ aPaths.Changed, "PNG" };
		const auto passed           = same_render_pixels(persistedInitial, aInitial)
			&& same_render_pixels(persistedChanged, aChanged)
			&& !same_render_pixels(persistedInitial, persistedChanged);
		if (!passed) {
			QFile::remove(aPaths.Initial);
			QFile::remove(aPaths.Changed);
			return native_appearance_persistence_disposition::persisted_readback_failed;
		}
		return native_appearance_persistence_disposition::persisted;
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

	/// \brief What one fixture's run opens, and what it then expects to see.
	///
	/// \details The connect stage used to type one pull request's coordinates as
	/// literals, and the presentation check asserted that same pull request's
	/// title, viewer, and conversation shape. Two fixtures could share those
	/// literals because they serve the same synthetic pull request; a recorded
	/// one serves a different one, so the pair is named here and both sites read
	/// the run's own expectations rather than a constant.
	///
	/// `ExpectedThreadId` is the one optional field. A fixture whose recording
	/// carries no review thread has no row to find by identity, and an absent
	/// expectation is not a relaxed one - it is the honest statement that there
	/// is nothing there. `ExpectedConversationRows` still fixes the count
	/// exactly, so an unexpected row is still a failure.
	struct fixture_run_expectations
	{
		std::string                Owner;
		std::string                Repository;
		int                        Number = 0;
		std::string                ExpectedTitle;
		std::string                ExpectedAccountText;
		int                        ExpectedConversationRows = 0;
		std::vector<std::string>   ExpectedConversationIdentities;
		int                        ExpectedFocusedConversationRows = 0;
		std::vector<std::string>   ExpectedFocusedConversationIdentities;
		std::optional<std::string> ExpectedThreadId;
	};

	/// \brief States what the synthetic pull request both hand-written fixtures
	/// serve looks like.
	///
	/// \details `github-com` and `ghes-3.18` are the same pull request on two
	/// hosts, which is the whole point of the pair, so they answer with one
	/// value rather than two copies of it.
	[[nodiscard]] auto synthetic_fixture_expectations()
		-> fixture_run_expectations
	{
		return {
			.Owner                          = "octo-org",
			.Repository                     = "review-repository",
			.Number                         = 17,
			.ExpectedTitle                  = "Keep review snapshots deterministic",
			.ExpectedAccountText            = "Octo Reviewer (@octo-reviewer)",
			.ExpectedConversationRows       = 6,
			.ExpectedConversationIdentities = {
				"review:PRR_fixture_1",
				"review-thread:PRRT_fixture_1",
				"review-comment:PRRC_fixture_1",
				"review-thread:PRRT_fixture_3",
				"review-comment:PRRC_fixture_4",
				"review-comment:PRRC_fixture_5",
				"review-thread:PRRT_fixture_2",
				"review-comment:PRRC_fixture_2",
				"review-comment:PRRC_fixture_3",
				"review-thread:PRRT_fixture_file",
				"review-comment:PRRC_fixture_file",
				"issue-comment:IC_fixture_1",
			},
			// A selected review.cpp comparison keeps all four threads anchored to
			// that file and their six comments, while omitting the review-wide
			// review and issue-comment roots.
			.ExpectedFocusedConversationRows       = 4,
			.ExpectedFocusedConversationIdentities = {
				"review-thread:PRRT_fixture_1",
				"review-comment:PRRC_fixture_1",
				"review-thread:PRRT_fixture_3",
				"review-comment:PRRC_fixture_4",
				"review-comment:PRRC_fixture_5",
				"review-thread:PRRT_fixture_2",
				"review-comment:PRRC_fixture_2",
				"review-comment:PRRC_fixture_3",
				"review-thread:PRRT_fixture_file",
				"review-comment:PRRC_fixture_file",
			},
			.ExpectedThreadId = "review-thread:PRRT_fixture_1",
		};
	}
#endif

#if defined(BPR_BUILD_TESTING)
	enum class native_appearance_precondition_refusal {
		none,
		selected_diff_refused,
		selected_diff_budget_exhausted,
		owner_focus_required,
		style_hints_unavailable,
		platform_not_cocoa,
		native_style_unavailable,
		startup_scheme_not_light,
		global_scheme_not_light,
		light_palette_unavailable,
		fixture_thread_unavailable,
		epoch_pair_mismatch,
		evidence_paths_unavailable,
		selected_diff_presentation_lost,
		comparison_not_visible,
		light_context_refused,
		initial_render_refused,
		automated_budget_exhausted,
	};
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
			overview_since_review,
			review_behaviour,
			saved_server,
#endif
			shutdown,
		};

		// [[BPR-0982]]. [[BPR-0909]] retired the precomposed GitHub CLI rows:
		// the CLI is a discovered way in, reached only through Use GitHub CLI,
		// which composition wires only where it offers the read. The course
		// checks that entry and the cards once, before any gesture, and
		// presses nothing, because pressing Use GitHub CLI launches `gh`.
		struct cli_entry_probe_state
		{
			bool                       Enabled = false;
			std::optional<std::string> EnterpriseHostname;
			std::size_t                Expected = 0U;
			std::size_t                Observed = 0U;
			// How many server cards the one observation found, none of them
			// carrying a GitHub CLI profile.
			std::size_t Cards = 0U;
			// [[BPR-0797]]. Whether the composition this run holds offers the
			// GitHub CLI read, read off the composed factory rather than the
			// build. It decides whether the entry must be offered or absent.
			bool ReadOffered = false;
		};

		// [[BPR-0540]]. Ticks the orphan course keeps running after the host's
		// refresh probe has been answered, at the 10 ms poll the automated run
		// already uses. The course's claim is a negative, so its terminal has
		// to be a moment by which the positive would have happened: the
		// vault's callback, the decision lane's turn, the republication and
		// composition's read of it. Its adequacy is not argued -- reverting
		// the provenance gate makes this course report a resume inside it.
#if defined(BPR_BUILD_TESTING)
		static constexpr std::size_t orphan_course_settle_ticks = 20U;
#endif

		// RFC-BPR-0016. Selecting the device path once, from the shipped
		// composition, and reporting what came back. An unregistered platform
		// answers with a typed refusal and nothing sent; a registered platform
		// starts a real flow under the recorded distributable client.
		struct device_authorization_probe_state
		{
			bool Enabled  = false;
			bool Selected = false;
			// The registered course enrolls, so its terminal is `authorized`
			// rather than `unavailable` and it has vault evidence to report.
			bool Registered = false;
			// One cancel, dispatched when a registered platform's real flow
			// reaches `awaiting_authorization`. The probe proves the refusal
			// is gone, not that a human authorizes within the budget, so it
			// ends the flow at the first stage only a registered composition
			// can reach.
			bool CancelDispatched = false;
			// RFC-BPR-0016 browser handoff. One "Open browser" request, made
			// when the flow reaches `awaiting_authorization` because that is
			// the only stage at which there is an address to open. The
			// composed opener under a tests-enabled build is the recording
			// one, so this proves the request reaches a bound capability and
			// starts no program.
			bool BrowserProbe   = false;
			bool OpenDispatched = false;
			// [[BPR-0526]]. Starts the flow the way a reviewer does: through
			// the panel the window mounts, found by the accessible name it
			// announces rather than through any accessor the composition
			// holds. A window that stopped mounting it, or a composition that
			// stopped adopting it, finds nothing and reports so.
			bool PanelCourse = false;
			bool PanelBegan  = false;
			// [[BPR-0535]]. The enterprise negative. Drives the picker onto a
			// GHES profile, which is what the mounted panel binds its begin
			// to, and presses the same "Get a code" the course above does.
			// A capability composed for GitHub.com must refuse it before a
			// request is built, so this course's terminal is `unavailable`
			// while the panel course beside it enrolls.
			bool GhesCourse = false;
			bool GhesBegan  = false;
			// [[BPR-0536]]. The registered course carried past its terminal.
			// Where that one ends when the grant reaches the vault, this one
			// waits for the renewal the grant armed to rotate the material
			// under the same authority, which is the only place the claim
			// "refreshes before expiry under existing vault authority" can be
			// observed through the composed graph rather than at the host.
			bool RefreshCourse = false;
			// [[BPR-0540]]. The renewal course's composition driven the other
			// way round: the refresh slot already holds a past grant's
			// material, and the session this run establishes is the reviewer's
			// own pasted credential. Nothing here may renew, because nothing
			// here was granted -- so the course connects rather than selecting
			// the device path, and reports the counters that would move if the
			// orphan were spent.
			bool        OrphanCourse      = false;
			std::size_t OrphanSettleTicks = 0U;
		};

		// RFC-BPR-0017. Activating the signed-out preview once, from the
		// shipped composition, and reporting what came back. A platform whose
		// selection composed no credential-free read authority answers with a
		// typed refusal and nothing sent; the tests-enabled courses force the
		// selection available so the composed authority can be driven.
		struct public_preview_probe_state
		{
			bool Enabled = false;
			// The fixture course that forces the selection available and opens
			// the recorded public pull request through the anonymous
			// authority.
			bool                                    Course                       = false;
			bool                                    HistoricalCourse             = false;
			bool                                    EmptyTimeline                = false;
			bool                                    MissingHistoricalBase        = false;
			bool                                    MissingHistoricalBaseShown   = false;
			bool                                    RewrittenBase                = false;
			bool                                    RewrittenBaseShown           = false;
			bool                                    RewrittenBaseGestureSent     = false;
			bool                                    RewrittenBaseSelectionDenied = false;
			QString                                 RewrittenBaseGate;
			std::optional<bpr::qt::epoch_line_stop> RewrittenBaseLeft;
			std::optional<bpr::qt::epoch_line_stop> RewrittenBaseRight;
			bool                                    HistoricalGestureSent = false;
			bool                                    HistoricalHandleMoved = false;
			// [[BPR-1340]]. Drags each of the line's two handles to another
			// update through the window, as a pointer does, and waits for the
			// comparison the core publishes back.
			bool                                    DragCourse     = false;
			bool                                    DragLineOpened = false;
			int                                     DragStage      = 0;
			std::optional<bpr::qt::epoch_line_stop> DragTarget;
			bool                                    DragRightMoved = false;
			bool                                    DragLeftMoved  = false;
			bool                                    DragKeyMoved   = false;
			QStringList                             DragSteps;
			QString                                 DragGate;
			// [[BPR-0829]]. Whether the plain enabled course saw every force-push
			// occurrence on the line retained before it offered the writes.
			bool HistoricalRetained = false;
			// Only the plain enabled course waits for that. The historical course
			// waits for a retained head before its gesture; an empty timeline, a
			// missing historical base and a rewritten base leave occurrences
			// unretained by design, and the enterprise course is refused before
			// any lease.
			[[nodiscard]] auto awaits_historical_retention() const noexcept -> bool
			{
				return Course && !HistoricalCourse && !EmptyTimeline
					&& !MissingHistoricalBase && !RewrittenBase && !Enterprise;
			}
			std::size_t HistoricalNode = 0U;
			// The same override activated on an enterprise profile, where the
			// core policy refuses rather than the composition.
			bool Enterprise = false;
			bool Activated  = false;
			bool Opened     = false;
			// Whether the run has offered the mode its refused write and
			// triage intents. Offered exactly once, so the counted refusals
			// are the two the course asked for and not a tick's worth.
			bool WritesOffered = false;
			// [[BPR-0715]]. The epoch status the course settled on before
			// the writes were offered; shutdown returns the host to
			// `inactive`, so the verdict reads this rather than the live one.
			bpr::core::epoch_status SettledEpoch             = bpr::core::epoch_status::inactive;
			std::size_t             SettledOccurrenceCount   = 0U;
			bool                    SettledHistoryIncomplete = false;
			QString                 SettledHistoryText;
			// [[BPR-1340]]. Every node's date label, oldest first, so a course
			// reads what the line says under each update.
			QStringList SettledLineLabels;
			// [[BPR-0316]]. The forced selection with no fixture under it.
			// `SelectionForced` alone activates the mode and stops -- the
			// verdict for a shipped enabled selection, which reaches zero
			// requests -- and `LiveCandidate` additionally opens the named
			// pull request through the production anonymous authority.
			// `DryRun` composes the live candidate and reports its wiring
			// without issuing anything.
			bool SelectionForced = false;
			bool LiveCandidate   = false;
			bool DryRun          = false;
			// Read from the live status while the session is open, because
			// shutdown closes it before the markers are printed.
			bool          SignedOutObserved        = false;
			bool          ProductionFactories      = false;
			bool          SnapshotSignedOut        = false;
			bool          BudgetObserved           = false;
			std::size_t   WireRequests             = 0U;
			std::size_t   WireCapabilityProbes     = 0U;
			std::size_t   WireCredentialedRefusals = 0U;
			std::uint64_t BudgetLimit              = 0U;
			std::uint64_t BudgetRemaining          = 0U;
			std::uint64_t BudgetReset              = 0U;
			std::string   HeadOid;
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
		std::chrono::milliseconds   Budget             = default_automated_run_budget;
		bool                        BudgetExpired      = false;
		bool                        PresentationPassed = false;
		// [[BPR-0647]]. The overview course over the fixture: the page shown
		// with every file, and Continue review back in Review.
		bool OverviewShown    = false;
		bool OverviewReturned = false;
		// [[BPR-0919]]. The recorded public path keeps Start review distinct
		// from Continue preferences and observes that the gesture issues no
		// review mutation.
		bool StartReviewAttempted               = false;
		bool StartReviewEnteredReview           = false;
		bool StartReviewPublishedNothing        = false;
		bool ReviewBehaviourPreferencesSelected = false;
		bool ContinueReviewEnteredReview        = false;
		bool ContinueReviewPublishedNothing     = false;
		// [[BPR-0654]]. The head I have marked reviewed, the head moved to the
		// fixture's second head, and the overview naming exactly the files
		// that differ between the two; asked for by `OverviewSinceReviewRequested`.
		bool OverviewSinceReviewPassed = false;
		bool RenderSaved               = true;
		bool VerifyNativePlatformVault = false;
		// [[BPR-0797]]. Report the identity of the two capabilities this run
		// composed. Off by default and named by its own flag, so no existing
		// row's output changes and no row can pass this by accident.
		bool                                      CapabilityProbe = false;
		QString                                   LastEpochRefreshIdentity;
		bool                                      LastEpochRefreshFocused = false;
		QString                                   LastEpochStatus;
		QString                                   LastEpochComparison;
		QString                                   LastEpochLeft;
		QString                                   LastEpochRight;
		int                                       LastEpochCoreStatus     = -1;
		int                                       LastEpochLeftRow        = -1;
		int                                       LastEpochRightRow       = -1;
		std::size_t                               LastEpochCount          = 0U;
		bool                                      LastEpochHistoryEnabled = false;
		int                                       LastConversationRows    = -1;
		std::vector<std::string>                  LastConversationIdentities;
		std::optional<bpr::core::source_line_ref> LastConversationAnchor;
		bool                                      LastConversationFileAnchored       = false;
		bool                                      LastConversationFileMatchesFixture = false;
		int                                       FailedStage                        = -1;
		cli_entry_probe_state                     Cli;
		device_authorization_probe_state          Device;
		public_preview_probe_state                PublicPreview;
#if defined(BPR_BUILD_TESTING)
		struct native_appearance_probe_state
		{
			bool                                  Enabled                      = false;
			bool                                  StageEntered                 = false;
			bool                                  WaitingForOwnerFocusReported = false;
			bool                                  WaitingForOwnerReturn        = false;
			bool                                  OwnerReturnReported          = false;
			bool                                  OwnerReturnEvaluated         = false;
			bool                                  OwnerWindowActive            = false;
			bool                                  OwnerApplicationActiveWindow = false;
			bool                                  OwnerExactFocus              = false;
			bool                                  SignalObserved               = false;
			bool                                  StatePassed                  = false;
			bool                                  PixelsDiffer                 = false;
			std::size_t                           OwnerWaitEpisode             = 0U;
			std::size_t                           SettleAttempts               = 0U;
			std::size_t                           GlobalPollTicks              = 0U;
			Qt::ColorScheme                       InitialColorScheme           = Qt::ColorScheme::Unknown;
			Qt::ColorScheme                       ObservedColorScheme          = Qt::ColorScheme::Unknown;
			Qt::ColorScheme                       InitialGlobalColorScheme     = Qt::ColorScheme::Unknown;
			Qt::ColorScheme                       ObservedGlobalColorScheme    = Qt::ColorScheme::Unknown;
			QPalette                              InitialPalette;
			QString                               InitialPlatformName;
			QString                               InitialStyleClass;
			QPersistentModelIndex                 Thread;
			bool                                  InitialWindowActive = false;
			QWidget*                              InitialFocus        = nullptr;
			std::size_t                           InitialModelResets  = 0U;
			QString                               ExpectedEpochLeftOid;
			QString                               ExpectedEpochRightOid;
			QPersistentModelIndex                 EpochLeft;
			QPersistentModelIndex                 EpochRight;
			QString                               EpochLeftIdentity;
			QString                               EpochRightIdentity;
			QPalette                              InitialEpochPalette;
			std::size_t                           InitialEpochModelResets       = 0U;
			std::size_t                           InitialEpochAppearanceUpdates = 0U;
			QImage                                InitialRender;
			QImage                                ChangedRender;
			native_appearance_capture_disposition InitialCaptureDisposition
				= native_appearance_capture_disposition::not_attempted_state_precondition;
			native_appearance_capture_disposition ChangedCaptureDisposition
				= native_appearance_capture_disposition::not_attempted_state_precondition;
			std::optional<bpr::core::semantic_selection> InitialDiffSelection;
			QPalette                                     InitialDiffPalette;
			QPalette                                     ObservedDiffPalette;
			// BPR-0114's own evidence, distinct from the palette assertions
			// above. Those prove the toolkit follows the operating system;
			// these prove *this product's* appearance model does, and that an
			// explicit mode does not.
			bool          ProfileFollowsPassed  = false;
			bool          ExplicitIgnoresPassed = false;
			int           VariantBeforeChange   = -1;
			int           VariantAfterChange    = -1;
			int           VariantUnderExplicit  = -1;
			int           VariantBackOnSystem   = -1;
			QFont         InitialDiffFont;
			QFont         ObservedDiffFont;
			std::uint64_t InitialDiffGeometryRebuilds        = 0U;
			std::uint64_t DiffGeometryRebuildDelta           = 0U;
			std::size_t   InitialDiffGeometryHistorySize     = 0U;
			bool          InitialDiffGeometryHistoryOverflow = false;
			std::array<bpr::qt::diff_geometry_rebuild_cause, 8U>
						DiffGeometryRebuildCauses{ };
			std::size_t DiffGeometryRebuildCauseCount   = 0U;
			bool        DiffGeometryAttributionComplete = false;
			bpr::app::detail::native_appearance_selected_diff_probe_result
				SelectedDiffResult = bpr::app::detail::
					native_appearance_selected_diff_probe_result::pending;
			bpr::app::detail::native_appearance_selected_diff_probe_evidence
												   SelectedDiffEvidence;
			native_appearance_precondition_refusal PreconditionRefusal
				= native_appearance_precondition_refusal::none;
			native_appearance_evidence_paths EvidencePaths;
		};

		bool Fixture                     = false;
		bool WorkbenchCompositionProbe   = false;
		bool WorkbenchCompositionStarted = false;
		bool WorkbenchCompositionPassed  = false;
		bpr::app::detail::workbench_composition_probe_evidence
			 WorkbenchCompositionEvidence;
		bool UIforEtwSelectedDiffProbe    = false;
		bool UIforEtwSelectedDiffStarted  = false;
		bool UIforEtwSelectedDiffPassed   = false;
		bool UIforEtwSelectedDiffCaptured = false;
		bpr::app::detail::uiforetw_selected_diff_probe_evidence
										UIforEtwSelectedDiffEvidence;
		std::optional<std::string_view> UIforEtwSelectedDiffDirectory;
		std::optional<bpr::app::detail::review_ledger_composition_probe>
			 ReviewLedgerCompositionProbe;
		bool ReviewLedgerCompositionPassed = false;
		bpr::app::detail::review_ledger_composition_probe_evidence
			 ReviewLedgerCompositionEvidence;
		bool ReviewRoundTripCompositionProbe   = false;
		bool ReviewRoundTripCompositionStarted = false;
		bool ReviewRoundTripCompositionPassed  = false;
		bpr::app::detail::review_round_trip_composition_probe_evidence
			ReviewRoundTripCompositionEvidence;
		// [[BPR-0657]]. `--threaded-reply-probe`.
		bool                                            ThreadedReplyProbe   = false;
		bool                                            ThreadedReplyStarted = false;
		bool                                            ThreadedReplyPassed  = false;
		bpr::app::detail::threaded_reply_probe_evidence ThreadedReplyEvidence;
		std::string                                     ThreadedReplyDiagnostic;
		// [[BPR-1083]]. `--label-authoring-probe`, renders into
		// `--label-authoring-evidence` when it names a folder.
		bool                                             LabelAuthoringProbe  = false;
		bool                                             LabelAuthoringPassed = false;
		bpr::app::detail::label_authoring_probe_evidence LabelAuthoringEvidence;
		std::string                                      LabelAuthoringDiagnostic;
		std::optional<std::string>                       LabelAuthoringEvidenceDirectory;
		// [[BPR-1326]]. `--published-file-switch-probe`, and
		// `--published-file-switch-timing settled|late|draft`.
		bool                                                   PublishedFileSwitchProbe  = false;
		bpr::app::detail::published_file_switch_timing         PublishedFileSwitchTiming = bpr::app::detail::published_file_switch_timing::settled;
		bool                                                   PublishedFileSwitchPassed = false;
		bpr::app::detail::published_file_switch_probe_evidence PublishedFileSwitchEvidence;
		std::string                                            PublishedFileSwitchDiagnostic;
		// [[BPR-1345]]. `--start-review-course pending|finish|refused|uncertain|again`.
		std::optional<bpr::app::detail::start_review_course> StartReviewCourse;
		bool                                                 StartReviewPassed = false;
		bpr::app::detail::start_review_probe_evidence        StartReviewEvidence;
		std::string                                          StartReviewDiagnostic;
		// [[BPR-1374]]. `--reply-draft-course refresh|edited|write|restart|deleted|kept|resolved`.
		std::optional<bpr::app::detail::reply_draft_course> ReplyDraftCourse;
		bool                                                ReplyDraftPassed = false;
		bpr::app::detail::reply_draft_probe_evidence        ReplyDraftEvidence;
		std::string                                         ReplyDraftDiagnostic;
		// [[BPR-1331]]. `--review-progress-push-probe before|after`.
		std::optional<bpr::app::detail::review_progress_push_course> ReviewProgressPushProbe;
		bool                                                         ReviewProgressPushPassed = false;
		std::string                                                  ReviewProgressPushMarks;
		std::string                                                  ReviewProgressPushDiagnostic;
		// [[BPR-1212]]. `--line-draft-course`: only the course's line-draft phases.
		bool LineDraftCourse = false;
		// [[BPR-1212]]. `--cancel-first-base-numbering`: the first base-numbering
		// answer applies as cancelled; armed on the course's first tick.
		bool CancelFirstBaseNumbering       = false;
		bool BaseNumberingCancellationArmed = false;
		std::optional<bpr::app::detail::defect_composition_probe>
			DefectCompositionProbe;
		// [[BPR-0654]]. `--overview-since-review`.
		bool OverviewSinceReviewRequested = false;
		// [[BPR-0661]]. `--review-behaviour`.
		bool ReviewBehaviourRequested = false;
		// [[BPR-0660]]. `--saved-server`.
		bool SavedServerRequested     = false;
		bool DefectCompositionStarted = false;
		bool DefectCompositionPassed  = false;
		bpr::app::detail::defect_composition_probe_evidence
			 DefectCompositionEvidence;
		bool DiffProbe         = false;
		bool DiffShutdownProbe = false;
		// [[BPR-0823]], [[BPR-0863]]. The diff probe loads, measures and
		// captures the comparison, so it states Review and needs the
		// comparison shown.
		bpr::app::detail::course_modality_step DiffModality{
			.Modality = bpr::qt::review_modality::review,
			.Needs    = { .Comparison = true },
		};
		// A stage that never becomes ready. Every real readiness condition in
		// an automated run is one the fixture eventually satisfies, so without
		// a deliberate stall there is no way to reach the run's own timeout and
		// show that it fires before the harness kills the process.
		bool StallProbe                       = false;
		bool DiffInteractionEvidencePassed    = false;
		bool ConversationAnchorEvidencePassed = false;
		bool DiffRequested                    = false;
		// Whether a theme change on the composed application repainted without
		// costing a complete-blob pass. The token-span cache is keyed by blob
		// identity and definition version alone, so a submission count that
		// moved across an appearance change means the key acquired a colour.
		bool                       SyntaxThemeRepaintPassed = false;
		std::optional<std::string> ThemeCustomizationPath;
		bool                       ThemeCustomizationPassed = false;
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
		fixture_run_expectations                     Expectations;
		bpr::app::detail::github_epoch_fixture_phase EpochPhase = bpr::app::detail::github_epoch_fixture_phase::single;
		enum class epoch_maintenance_probe {
			none,
			prune,
			verify_pruned_and_delete,
			verify_deleted,
		};
		epoch_maintenance_probe MaintenanceProbe = epoch_maintenance_probe::none;
		/// \brief The epoch every maintenance-evidence query in this run names.
		///
		/// \details `prune` and `verify_pruned_and_delete` assign this from the
		/// catalog binding they captured. `verify_deleted` deliberately leaves
		/// it default-constructed: it runs in a fresh process after the whole
		/// cache was deleted, so there is no surviving binding to name, and an
		/// invalid target asks `maintenance_evidence` about ordinal one of the
		/// catalog it can currently derive. That is the intended question — but
		/// the gate, the assertion and the diagnostic must all ask it through
		/// this one field, or they are not talking about the same epoch and
		/// nothing says they would agree. BPR-0345.
		bpr::core::review_epoch_id MaintenanceTarget;
		/// \brief The maintenance condition the probe is still waiting on.
		///
		/// \details Every condition `begin_epoch_maintenance_probe` checks is
		/// established asynchronously, so "not yet" and "wrong" look identical
		/// from one tick. The stage used to read them as "wrong" and fail the
		/// run on the first refusal, which reported `PUBLIC_PATH_STAGE:14` and
		/// nothing else — the one number that could not say which of ten
		/// conditions was unmet. The stage now waits, and this records what it
		/// is waiting for so a budget that expires names it. BPR-0447.
		const char* MaintenanceGate = nullptr;
		/// \brief The per-target deleted flags, as the probe observed them.
		///
		/// \details `maintenance_evidence` is a live observation: for a
		/// retired target it reopens the catalog and reads the epoch's anchors
		/// back out of it. Asking it again once shutdown has torn that catalog
		/// down does not re-check the claim, it asks a question of an object
		/// that is gone, and the honest answer to that is the struct's `false`
		/// default. The closing assertion did exactly that and failed about
		/// half the time on state that had been correct throughout the run.
		/// So the probe records what it saw at the moment it was satisfied,
		/// and the assertion reads the record. BPR-0345.
		bool                            MaintenanceObserved               = false;
		bool                            MaintenanceTargetBaseCacheDeleted = false;
		bool                            MaintenanceTargetHeadCacheDeleted = false;
		int                             MaintenanceLeftRow                = -1;
		int                             MaintenanceRightRow               = -1;
		int                             MaintenanceAccountReads           = 0;
		int                             MaintenanceSmartAcquires          = 0;
		std::size_t                     MaintenanceModelResets            = 0U;
		int                             EpochLeftRow                      = -1;
		int                             EpochRightRow                     = -1;
		native_appearance_probe_state   Appearance;
		std::optional<std::string_view> DiffEvidenceDirectory;
		/// The appearance the diff captures are taken under, when one was
		/// asked for. Absent leaves the run on whatever composition resolved.
		std::optional<bpr::core::appearance_mode> DiffEvidenceAppearance;
#endif
		std::optional<std::string_view> RenderPath;
		bool                            RenderCollapsesFilesDrawer = false;
		/// The application's status as the course ended, captured at the
		/// shutdown transition; the displayed-comparison lines print from it.
		std::optional<bpr::app::github_application_status> CourseEndStatus;
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

	[[nodiscard]] auto native_appearance_capture_disposition_name(
		const native_appearance_capture_disposition aDisposition) noexcept
		-> const char*
	{
		switch (aDisposition) {
		case native_appearance_capture_disposition::not_attempted_state_precondition:
			return "not_attempted_state_precondition";
		case native_appearance_capture_disposition::window_grab_null:
			return "window_grab_null";
		case native_appearance_capture_disposition::normalization_failed:
			return "normalization_failed";
		case native_appearance_capture_disposition::captured:
			return "captured";
		}
		return "unknown";
	}

	[[nodiscard]] auto native_appearance_persistence_disposition_name(
		const native_appearance_persistence_disposition aDisposition) noexcept
		-> const char*
	{
		switch (aDisposition) {
		case native_appearance_persistence_disposition::not_attempted:
			return "not_attempted";
		case native_appearance_persistence_disposition::initial_write_failed:
			return "initial_write_failed";
		case native_appearance_persistence_disposition::changed_write_failed:
			return "changed_write_failed";
		case native_appearance_persistence_disposition::persisted_readback_failed:
			return "persisted_readback_failed";
		case native_appearance_persistence_disposition::persisted:
			return "persisted";
		}
		return "unknown";
	}

	[[nodiscard]] auto observed_boolean_name(
		const bool aObserved,
		const bool aValue) noexcept -> const char*
	{
		if (!aObserved) {
			return "not_observed";
		}
		return aValue ? "true" : "false";
	}

	/// \brief Names a typed startup refusal for a developer build's cause line.
	///
	/// \details [[BPR-0843]]. A refusal from the composition printed only
	/// `STARTUP_FAILED`, so a non-canonical fixture root on macOS and a
	/// contained course on Windows ([[BPR-0834]]) each needed a throwaway
	/// diagnostic to read which one it was. The switch has no default, so a
	/// new failure fails the build here instead of printing a guess.
	[[nodiscard]] auto startup_failure_name(
		const bpr::app::github_application_startup_failure aFailure) noexcept
		-> const char*
	{
		using enum bpr::app::github_application_startup_failure;
		switch (aFailure) {
		case owner_busy:
			return "owner_busy";
		case activation_queued:
			return "activation_queued";
		case activation_refused:
			return "activation_refused";
		case activation_indeterminate:
			return "activation_indeterminate";
		case invalid_profile:
			return "invalid_profile";
		case registry:
			return "registry";
		case root:
			return "root";
		case unavailable:
			return "unavailable";
		case cancelled:
			return "cancelled";
		}
		return "unknown";
	}

	[[nodiscard]] auto native_selected_diff_reason_name(
		const bpr::app::detail::native_appearance_selected_diff_reason aReason) noexcept -> const char*
	{
		using reason
			= bpr::app::detail::native_appearance_selected_diff_reason;
		switch (aReason) {
		case reason::none:
			return "none";
		case reason::awaiting_workbench_observation:
			return "awaiting_workbench_observation";
		case reason::awaiting_selection_projection:
			return "awaiting_selection_projection";
		case reason::awaiting_selected_diff_publication:
			return "awaiting_selected_diff_publication";
		case reason::awaiting_diff_presentation:
			return "awaiting_diff_presentation";
		case reason::composition_unavailable:
			return "composition_unavailable";
		case reason::fixture_authority_mismatch:
			return "fixture_authority_mismatch";
		case reason::fixture_material_mismatch:
			return "fixture_material_mismatch";
		case reason::file_projection_mismatch:
			return "file_projection_mismatch";
		case reason::selection_wire_unavailable:
			return "selection_wire_unavailable";
		case reason::selected_diff_failed:
			return "selected_diff_failed";
		case reason::selected_diff_mismatch:
			return "selected_diff_mismatch";
		case reason::fixture_diff_helper_invoked:
			return "fixture_diff_helper_invoked";
		}
		return "unknown";
	}

	[[nodiscard]] auto native_selected_diff_result_marker(
		const bpr::app::detail::native_appearance_selected_diff_probe_result
			aResult) noexcept -> const char*
	{
		using result = bpr::app::detail::
			native_appearance_selected_diff_probe_result;
		switch (aResult) {
		case result::pending:
			return "NATIVE_APPEARANCE_SELECTED_DIFF_PENDING\n";
		case result::ready:
			return "NATIVE_APPEARANCE_SELECTED_DIFF_READY\n";
		case result::refused:
			return "NATIVE_APPEARANCE_SELECTED_DIFF_REFUSED\n";
		}
		return "NATIVE_APPEARANCE_SELECTED_DIFF_REFUSED\n";
	}

	[[nodiscard]] auto native_appearance_precondition_name(
		const native_appearance_precondition_refusal aRefusal) noexcept
		-> const char*
	{
		switch (aRefusal) {
		case native_appearance_precondition_refusal::none:
			return "none";
		case native_appearance_precondition_refusal::selected_diff_refused:
			return "selected_diff_refused";
		case native_appearance_precondition_refusal::selected_diff_budget_exhausted:
			return "selected_diff_budget_exhausted";
		case native_appearance_precondition_refusal::owner_focus_required:
			return "owner_focus_required";
		case native_appearance_precondition_refusal::style_hints_unavailable:
			return "style_hints_unavailable";
		case native_appearance_precondition_refusal::platform_not_cocoa:
			return "platform_not_cocoa";
		case native_appearance_precondition_refusal::native_style_unavailable:
			return "native_style_unavailable";
		case native_appearance_precondition_refusal::startup_scheme_not_light:
			return "startup_scheme_not_light";
		case native_appearance_precondition_refusal::global_scheme_not_light:
			return "global_scheme_not_light";
		case native_appearance_precondition_refusal::light_palette_unavailable:
			return "light_palette_unavailable";
		case native_appearance_precondition_refusal::fixture_thread_unavailable:
			return "fixture_thread_unavailable";
		case native_appearance_precondition_refusal::epoch_pair_mismatch:
			return "epoch_pair_mismatch";
		case native_appearance_precondition_refusal::evidence_paths_unavailable:
			return "evidence_paths_unavailable";
		case native_appearance_precondition_refusal::selected_diff_presentation_lost:
			return "selected_diff_presentation_lost";
		case native_appearance_precondition_refusal::comparison_not_visible:
			return "comparison_not_visible";
		case native_appearance_precondition_refusal::light_context_refused:
			return "light_context_refused";
		case native_appearance_precondition_refusal::initial_render_refused:
			return "initial_render_refused";
		case native_appearance_precondition_refusal::automated_budget_exhausted:
			return "automated_budget_exhausted";
		}
		return "unknown";
	}

	/// \brief Reports which half of the light-state context held.
	///
	/// \details `native_appearance_context_passed` is one conjunction of about
	/// forty terms, and it reported a single aggregate. A run that failed it
	/// said `0` and nothing else, which located the failure no better than not
	/// checking. The two halves are the ones that can fail for product reasons
	/// -- the epoch selectors' resolved pair, and the comparison surface's
	/// result -- so they are reported separately.
	struct native_appearance_context_detail
	{
		bool Epoch = false;
		bool Diff  = false;
		/// \brief The conversation selection the probe set up is still in place.
		bool Thread = false;
		/// \brief The window is active, frontmost, and holding its focus widget.
		bool Window = false;
		/// \brief Nothing overrode palette, style, font or stylesheet.
		bool Clean = false;
		bool All   = false;
	};

	[[nodiscard]] auto native_appearance_context_detail_of(
		const bpr::qt::workbench_window& aWindow,
		const automated_run&             aRun) -> native_appearance_context_detail
	{
		const auto* const view          = aWindow.conversation_view();
		const auto* const model         = aWindow.conversation_model();
		const auto* const control       = aWindow.epoch_selectors();
		const auto* const epochModel    = control->history_model();
		const auto* const leftSelector  = control->left_selector();
		const auto* const rightSelector = control->right_selector();
		const auto* const leftOid       = control->left_oid_editor();
		const auto* const rightOid      = control->right_oid_editor();
		const auto* const workbench     = aWindow.comparison_surface();

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

		const auto threadPreserved = aRun.Appearance.Thread.isValid()
			&& view->currentIndex() == aRun.Appearance.Thread
			&& view->selectionModel()->isSelected(aRun.Appearance.Thread)
			&& !view->isExpanded(aRun.Appearance.Thread)
			&& model->statistics().ModelResets
				== aRun.Appearance.InitialModelResets;
		const auto windowPreserved = aRun.Appearance.InitialWindowActive
			&& aRun.Appearance.InitialFocus != nullptr
			&& aRun.Appearance.InitialFocus->window() == &aWindow
			&& aWindow.isActiveWindow()
			&& QApplication::activeWindow() == &aWindow
			&& QApplication::focusWidget() == aRun.Appearance.InitialFocus;
		const auto presentationClean = QGuiApplication::platformName()
				== aRun.Appearance.InitialPlatformName
			&& QString::fromLatin1(
				   QApplication::style()->metaObject()->className())
				== aRun.Appearance.InitialStyleClass
			&& aWindow.statistics().GithubFullResets == 0U
			// Deliberately not `!testAttribute(WA_SetPalette)`. That term was
			// written when nothing composed an appearance profile, so the
			// window's palette could only have come from the platform and "no
			// palette was set" meant "nothing bypassed the model". Since
			// BPR-0183 the model sets the window's palette itself, which is the
			// opposite of a bypass, and the term became an assertion that the
			// feature is absent.
			//
			// The provenance question it was reaching for is answered better
			// and elsewhere: `APPEARANCE_APPLIED` reports `from-profile` by
			// comparing the window's palette against the profile's semantic
			// roles, which is a stronger claim than nobody having set one.
			// Style, font and stylesheet stay, because nothing legitimately
			// sets those on the window.
			&& !aWindow.testAttribute(Qt::WA_SetStyle)
			&& !aWindow.testAttribute(Qt::WA_SetFont)
			&& aWindow.styleSheet().isEmpty();
		return {
			.Epoch  = epochContextPreserved,
			.Diff   = diffContextPreserved,
			.Thread = threadPreserved,
			.Window = windowPreserved,
			.Clean  = presentationClean,
			.All    = epochContextPreserved && diffContextPreserved
				&& threadPreserved && windowPreserved && presentationClean,
		};
	}

	[[nodiscard]] auto native_appearance_context_passed(
		const bpr::qt::workbench_window& aWindow,
		const automated_run&             aRun) -> bool
	{
		return native_appearance_context_detail_of(aWindow, aRun).All;
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
		const bpr::qt::workbench_window& aWindow,
		const automated_run&             aRun) -> bool
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
			&& aWindow.epoch_selectors()->palette()
			== aRun.Appearance.InitialEpochPalette
			&& native_palette_readable(aWindow.epoch_selectors()->palette())
			&& aWindow.epoch_selectors()->statistics().AppearanceUpdates
			== aRun.Appearance.InitialEpochAppearanceUpdates
			&& native_appearance_context_passed(aWindow, aRun);
	}

	/// Whether the owner's focus has come back to the window the probe measures.
	///
	/// The one-shot dark measurement requires the window to be active, to be the
	/// application's active window, and to hold focus on the exact widget the
	/// Light baseline recorded. A person changing the appearance in System
	/// Settings necessarily surrenders all three, and cannot restore them within
	/// the half-second the settle counter allows, so measuring as soon as the
	/// schemes read Dark makes the attended procedure humanly impossible rather
	/// than merely strict. This is the same question the Light side already
	/// waits on, asked at the other end of the transition. BPR-0391.
	///
	/// The terms are the run's own baseline facts, not conditions waiting can
	/// change: `prepare_native_appearance_probe` refuses before the stage is
	/// entered unless the window was active with a focused widget, so a run that
	/// reaches here has both.
	[[nodiscard]] auto native_appearance_owner_returned(
		const bpr::qt::workbench_window& aWindow,
		const automated_run&             aRun) -> bool
	{
		return aRun.Appearance.InitialFocus != nullptr
			&& aWindow.isActiveWindow()
			&& QApplication::activeWindow() == &aWindow
			&& QApplication::focusWidget() == aRun.Appearance.InitialFocus;
	}

	[[nodiscard]] auto native_appearance_dark_state_passed(
		const bpr::qt::workbench_window& aWindow,
		const automated_run&             aRun) -> bool
	{
		const auto&       finalPalette       = aWindow.palette();
		const auto        initialWindow      = aRun.Appearance.InitialPalette.color(QPalette::Window).lightness();
		const auto        initialBase        = aRun.Appearance.InitialPalette.color(QPalette::Base).lightness();
		const auto        initialText        = aRun.Appearance.InitialPalette.color(QPalette::Text).lightness();
		const auto        finalWindow        = finalPalette.color(QPalette::Window).lightness();
		const auto        finalBase          = finalPalette.color(QPalette::Base).lightness();
		const auto        finalText          = finalPalette.color(QPalette::Text).lightness();
		const auto        initialEpoch       = aRun.Appearance.InitialEpochPalette;
		const auto        finalEpoch         = aWindow.epoch_selectors()->palette();
		const auto        initialEpochWindow = initialEpoch.color(QPalette::Window).lightness();
		const auto        initialEpochBase   = initialEpoch.color(QPalette::Base).lightness();
		const auto        initialEpochText   = initialEpoch.color(QPalette::Text).lightness();
		const auto        finalEpochWindow   = finalEpoch.color(QPalette::Window).lightness();
		const auto        finalEpochBase     = finalEpoch.color(QPalette::Base).lightness();
		const auto        finalEpochText     = finalEpoch.color(QPalette::Text).lightness();
		const auto* const workbench          = aWindow.comparison_surface();
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
			&& aWindow.epoch_selectors()->statistics().AppearanceUpdates
			> aRun.Appearance.InitialEpochAppearanceUpdates
			&& workbench->statistics().GeometryRebuilds
			== aRun.Appearance.InitialDiffGeometryRebuilds
			&& native_appearance_context_passed(aWindow, aRun);
	}

	/// \brief Whether the comparison is wholly inside the window and unclipped.
	///
	/// \details Replaces a `qobject_cast<QScrollArea*>(centralWidget())` that
	/// had been dead since BPR-0191 made the shell's central widget a plain
	/// `QWidget`. The guarded `ensureWidgetVisible` never ran, and measurement
	/// showed it had nothing to do: at the probe's 1280x1000 the comparison sits
	/// at 260,475 760x443 inside a 1280x918 central widget, fully contained and
	/// with its visible region covering its whole rect.
	///
	/// There is also no scroller it could have used. The shell's only
	/// `QScrollArea` holds the review header, is capped at 220 pixels, and does
	/// not contain the comparison; the comparison lives in a splitter that the
	/// layout always fits inside the central widget.
	///
	/// So the call is gone and the property it was reaching for is asserted
	/// instead. A cast inside an `if` cannot fail loudly, which is why nobody
	/// noticed for as long as nobody did.
	[[nodiscard]] auto comparison_is_wholly_visible(
		const bpr::qt::workbench_window& aWindow,
		const QWidget&                   aComparison) -> bool
	{
		const auto* const central = aWindow.centralWidget();
		if (central == nullptr) {
			return false;
		}
		const auto placed = QRect{
			aComparison.mapTo(central, QPoint{ 0, 0 }),
			aComparison.size(),
		};
		// [[BPR-1256]]. `visibleRegion()` leaves out what the widget's own
		// opaque children cover, so the comparison's shared scroll bar, 16
		// pixels wide under the Windows style the Linux rows use, read as
		// clipping. The comparison shows what it and its descendants show.
		auto shown = aComparison.visibleRegion();
		for (const auto* const child : aComparison.findChildren<QWidget*>()) {
			if (child->isVisible()) {
				shown += child->visibleRegion().translated(child->mapTo(&aComparison, QPoint{ 0, 0 }));
			}
		}
		return central->rect().contains(placed)
			&& shown.boundingRect() == aComparison.rect();
	}

	[[nodiscard]] auto prepare_native_appearance_probe(
		bpr::qt::workbench_window& aWindow,
		automated_run&             aRun)
		-> native_appearance_precondition_refusal
	{
		aRun.RenderSaved       = false;
		auto* const hints      = QGuiApplication::styleHints();
		auto* const view       = aWindow.conversation_view();
		auto* const model      = aWindow.conversation_model();
		auto* const control    = aWindow.epoch_selectors();
		auto* const epochModel = control->history_model();
		auto* const workbench  = aWindow.comparison_surface();
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
		if (hints == nullptr) {
			return native_appearance_precondition_refusal::style_hints_unavailable;
		}
		if (aRun.Appearance.InitialPlatformName != "cocoa") {
			return native_appearance_precondition_refusal::platform_not_cocoa;
		}
		if (aRun.Appearance.InitialStyleClass != "QMacStyle") {
			return native_appearance_precondition_refusal::native_style_unavailable;
		}
		if (hints->colorScheme() != Qt::ColorScheme::Light) {
			return native_appearance_precondition_refusal::startup_scheme_not_light;
		}
		if (aRun.Appearance.InitialGlobalColorScheme != Qt::ColorScheme::Light) {
			return native_appearance_precondition_refusal::global_scheme_not_light;
		}
		if (!lightPalette) {
			return native_appearance_precondition_refusal::light_palette_unavailable;
		}
		if (!thread.isValid()) {
			return native_appearance_precondition_refusal::fixture_thread_unavailable;
		}
		if (!leftEpoch.isValid() || !rightEpoch.isValid()
			|| aRun.Appearance.ExpectedEpochLeftOid.isEmpty()
			|| aRun.Appearance.ExpectedEpochRightOid.isEmpty()
			|| control->left_oid_editor()->text()
				!= aRun.Appearance.ExpectedEpochLeftOid
			|| control->right_oid_editor()->text()
				!= aRun.Appearance.ExpectedEpochRightOid
			|| !control->whole_pr_action()->isChecked()) {
			return native_appearance_precondition_refusal::epoch_pair_mismatch;
		}
		if (workbench == nullptr || workbench->current_result() == nullptr
			|| !workbench->selection().has_value()) {
			return native_appearance_precondition_refusal::
				selected_diff_presentation_lost;
		}
		if (!aRun.RenderPath.has_value()
			|| aRun.Appearance.EvidencePaths.Initial.isEmpty()
			|| aRun.Appearance.EvidencePaths.Changed.isEmpty()) {
			return native_appearance_precondition_refusal::
				evidence_paths_unavailable;
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
		settle_automated_events();
		if (!comparison_is_wholly_visible(aWindow, *workbench)) {
			return native_appearance_precondition_refusal::comparison_not_visible;
		}
		const auto geometryStatistics = workbench->statistics();
		aRun.Appearance.InitialDiffGeometryRebuilds
			= geometryStatistics.GeometryRebuilds;
		aRun.Appearance.InitialDiffGeometryHistorySize
			= geometryStatistics.GeometryRebuildHistorySize;
		aRun.Appearance.InitialDiffGeometryHistoryOverflow
			= geometryStatistics.GeometryRebuildHistoryOverflow;
		aRun.Appearance.InitialWindowActive = aWindow.isActiveWindow();
		aRun.Appearance.InitialFocus        = QApplication::focusWidget();
		if (!native_appearance_light_state_passed(aWindow, aRun)) {
			const auto contextDetail
				= native_appearance_context_detail_of(aWindow, aRun);
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
				<< "NATIVE_APPEARANCE_CONTEXT_DETAIL:" << contextDetail.Epoch
				<< ':' << contextDetail.Diff << ':' << contextDetail.Thread
				<< ':' << contextDetail.Window << ':' << contextDetail.Clean
				<< '\n'
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
			return native_appearance_precondition_refusal::light_context_refused;
		}

		auto initialCapture                       = capture_normalized_render(aWindow);
		aRun.Appearance.InitialCaptureDisposition = initialCapture.Disposition;
		if (
			initialCapture.Image.isNull()
			|| !native_appearance_light_state_passed(aWindow, aRun)) {
			return native_appearance_precondition_refusal::initial_render_refused;
		}
		aRun.Appearance.InitialRender = std::move(initialCapture.Image);

		QTextStream stream{ stdout };
		stream << "NATIVE_APPEARANCE_READY:"
			   << color_scheme_name(aRun.Appearance.InitialColorScheme) << '\n'
			   << "NATIVE_APPEARANCE_GLOBAL_READY:"
			   << color_scheme_name(
					  aRun.Appearance.InitialGlobalColorScheme)
			   << '\n';
		stream.flush();
		return native_appearance_precondition_refusal::none;
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

	/// \brief [[BPR-1340]]. A left-button drag on `aWidget` from `aFrom` to
	/// `aTo`, delivered to its top-level window as the platform delivers a
	/// pointer: the window picks the widget under the press and keeps sending
	/// it the moves and the release, through every filter on the way.
	void send_window_drag(QWidget& aWidget, const QPoint& aFrom, const QPoint& aTo)
	{
		auto* const top    = aWidget.window();
		auto* const target = top != nullptr ? top->windowHandle() : nullptr;
		if (target == nullptr) {
			return;
		}
		const auto send_mouse_event = [&](const QEvent::Type aType, const QPoint& aAt, const Qt::MouseButton aButton, const Qt::MouseButtons aButtons) {
			const auto local = QPointF{ aWidget.mapTo(top, aAt) };
			auto       event = QMouseEvent{ aType, local, local, QPointF{ aWidget.mapToGlobal(aAt) }, aButton, aButtons, Qt::NoModifier };
			QCoreApplication::sendEvent(target, &event);
		};
		send_mouse_event(QEvent::MouseMove, aFrom, Qt::NoButton, Qt::NoButton);
		send_mouse_event(QEvent::MouseButtonPress, aFrom, Qt::LeftButton, Qt::LeftButton);
		constexpr auto steps = 4;
		for (auto step = 1; step <= steps; ++step) {
			const auto at = aFrom + ((aTo - aFrom) * step) / steps;
			send_mouse_event(QEvent::MouseMove, at, Qt::NoButton, Qt::LeftButton);
		}
		send_mouse_event(QEvent::MouseButtonRelease, aTo, Qt::LeftButton, Qt::NoButton);
	}

	[[nodiscard]] auto capture_diff_interaction_evidence(
		bpr::qt::workbench_window&             aWindow,
		const std::optional<std::string_view>& aRawDirectory) -> bool
	{
		auto       failureReported = false;
		const auto fail            = [&failureReported](const QString& aPredicate) -> bool {
			if (!failureReported) {
				auto stream = QTextStream{ stdout };
				stream << "DIFF_INTERACTION_FAILURE:" << aPredicate << '\n';
				stream.flush();
				failureReported = true;
			}
			return false;
		};
		if (!aRawDirectory.has_value() || aRawDirectory->empty()) {
			return fail(QStringLiteral("evidence-directory-missing"));
		}
		const auto directoryPath = QString::fromUtf8(
			aRawDirectory->data(), static_cast<qsizetype>(aRawDirectory->size()));
		if (!QDir{ }.mkpath(directoryPath)) {
			return fail(QStringLiteral("evidence-directory-create"));
		}
		const auto     directory     = QDir{ directoryPath };
		constexpr auto evidenceFiles = std::array<std::string_view, 21>{
			"appearance-at-capture.json",
			"interaction-trace.jsonl",
			"01-selected-composition.png",
			"02-matching-early.png",
			"03-matching-late.png",
			"04-left-pane-input.png",
			"05-insertion-early.png",
			"06-insertion-late.png",
			"07-matching-continuation.png",
			"08-insertion-reversed.png",
			"09-right-pane-input.png",
			"10-divider-input.png",
			"11-deletion-early.png",
			"12-deletion-late.png",
			"13-replacement-early.png",
			"14-replacement-late.png",
			"15-other-ribbon-selected.png",
			"16-ribbon-selection.png",
			"17-divider-resized.png",
			"18-comparison-start.png",
			"19-comparison-end.png",
		};
		auto staleEvidenceRemovalFailed = false;
		for (const auto evidenceFile : evidenceFiles) {
			const auto fileName = QString::fromLatin1(
				evidenceFile.data(), static_cast<qsizetype>(evidenceFile.size()));
			const auto filePath = directory.filePath(fileName);
			if (QFile::exists(filePath) && !QFile::remove(filePath)) {
				staleEvidenceRemovalFailed = true;
			}
		}
		if (staleEvidenceRemovalFailed) {
			return fail(QStringLiteral("stale-evidence-remove"));
		}
		auto trace = QFile{
			directory.filePath(QStringLiteral("interaction-trace.jsonl"))
		};
		if (!trace.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
			return fail(QStringLiteral("interaction-trace-open"));
		}

		auto* const workbench = aWindow.comparison_surface();
		if (workbench == nullptr || workbench->current_result() == nullptr
			|| !workbench->current_context().has_value()
			|| !workbench->selection().has_value()) {
			return fail(QStringLiteral("initial-workbench-state"));
		}
		auto* const vertical = workbench->findChild<QSplitter*>(
			QStringLiteral("diffWorkbenchVerticalSplitter"));
		if (vertical == nullptr) {
			return fail(QStringLiteral("vertical-splitter-missing"));
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
				return fail(QStringLiteral("appearance-stamp-open"));
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
				return fail(QStringLiteral("appearance-stamp-write"));
			}

			// And the stamp is held to the pixels, so it cannot drift into
			// describing an appearance the panes are not painting. Sampling
			// the commonest colour rather than one point keeps a band or a
			// glyph from deciding the answer.
			auto* const pane = workbench->left_pane();
			if (pane == nullptr) {
				return fail(QStringLiteral("appearance-pane-missing"));
			}
			const auto raster = pane->viewport()->grab().toImage();
			auto       tally  = std::map<QRgb, int>{ };
			for (auto y = 0; y < raster.height(); ++y) {
				for (auto x = 0; x < raster.width(); ++x) {
					++tally[raster.pixel(x, y)];
				}
			}
			if (tally.empty()) {
				return fail(QStringLiteral("appearance-raster-empty"));
			}
			const auto commonest = std::ranges::max_element(
				tally, { }, [](const auto& aEntry) { return aEntry.second; });
			if (QColor{ commonest->first }
				!= aWindow.palette().color(QPalette::Base)) {
				// The divergent pixel's identity is the diagnosis on a style
				// that paints its own chrome inside the viewport, so the
				// failure names what it saw beside what it expected, with each
				// colour's share of the grab (BPR-0475).
				const auto base = aWindow.palette().color(QPalette::Base);
				const auto baseCount
					= tally.contains(base.rgb()) ? tally.at(base.rgb()) : 0;
				const auto total = static_cast<double>(raster.width())
					* static_cast<double>(raster.height());
				return fail(
					QStringLiteral("appearance-raster-palette observed=%1 "
								   "share=%2% expected=%3 expected-share=%4%")
						.arg(QColor{ commonest->first }.name())
						.arg(commonest->second * 100.0 / total, 0, 'f', 1)
						.arg(base.name())
						.arg(baseCount * 100.0 / total, 0, 'f', 1));
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
		if (matching == nullptr) {
			return fail(QStringLiteral("alignment-matching-missing"));
		}
		if (replacement == nullptr) {
			return fail(QStringLiteral("alignment-replacement-missing"));
		}
		if (deletion == nullptr) {
			return fail(QStringLiteral("alignment-deletion-missing"));
		}
		if (insertion == nullptr) {
			return fail(QStringLiteral("alignment-insertion-missing"));
		}

		const auto initialContext = *workbench->current_context();
		const auto initialStats   = workbench->statistics();
		const auto capture        = [&](const std::string_view aStep) {
			// [[BPR-1102]]. The link map has a frame only once the panes'
			// time-sliced measurement completes, and a resize restarts it. Four
			// passes are enough where a 4 ms batch measures the whole fixture;
			// on the Windows Debug build a batch measures at most 32 lines. Wait
			// for the measurement itself, bounded, then drain as before, as
			// [[BPR-1033]]'s capture does. An expiry reads geometry_pending=1
			// in the facts below.
			const auto measuring = std::chrono::steady_clock::now();
			while (workbench->geometry_pending()
				   && std::chrono::steady_clock::now() - measuring < std::chrono::seconds{ 5 }) {
				QApplication::processEvents();
			}
			settle_automated_events();
			const auto step = QString::fromLatin1(
				aStep.data(), static_cast<qsizetype>(aStep.size()));
			const auto& retained  = controller->current_frame();
			const auto& rendered  = workbench->link_map()->current_frame();
			const auto& selection = workbench->selection();
			if (!retained.has_value() || !rendered.has_value()
				|| !workbench->current_context().has_value()
				|| !selection.has_value()) {
				fail(QStringLiteral("capture-state"));
				// [[BPR-1102]]. One term per clause, then what the frames wait
				// on, printed after the marker so every earlier line keeps its
				// place.
				const auto  pending = workbench->statistics();
				const auto* linkMap = workbench->link_map();
				auto        facts   = QTextStream{ stdout };
				facts << "DIFF_INTERACTION_CAPTURE_STATE_FACTS: step=" << step
					  << " retained_frame=" << retained.has_value()
					  << " link_map_frame=" << rendered.has_value()
					  << " context=" << workbench->current_context().has_value()
					  << " selection=" << selection.has_value()
					  << " geometry_pending=" << workbench->geometry_pending()
					  << " geometry_rebuilds=" << pending.GeometryRebuilds
					  << " geometry_failures=" << pending.GeometryFailures
					  << " measurement_batches=" << pending.MeasurementBatches
					  << " measurement_restarts=" << pending.MeasurementRestarts
					  << " max_batch_lines=" << pending.MaximumMeasurementBatchLines
					  << " link_map=visible:" << linkMap->isVisible() << ",size:"
					  << linkMap->width() << 'x' << linkMap->height()
					  << ",links:" << linkMap->visible_links().size() << '\n';
				facts.flush();
				return false;
			}
			const auto imageName = step + QStringLiteral(".png");
			if (!aWindow.grab().save(directory.filePath(imageName), "PNG")) {
				return fail(QStringLiteral("capture-image-write"));
			}
			const auto& frame         = *retained;
			const auto  stats         = workbench->statistics();
			const auto  linkStats     = workbench->link_map()->statistics();
			const auto  splitterSizes = vertical->sizes();
			if (splitterSizes.size() != 2) {
				return fail(QStringLiteral("capture-splitter-shape"));
			}
			auto stream = QTextStream{ &trace };
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
				   << ",\"comparison_viewport_height\":"
				   << workbench->left_pane()->viewport()->height()
				   << ",\"detail_viewport_height\":"
				   << workbench->detail_pane()->viewport()->height()
				   << ",\"vertical_splitter_top_size\":" << splitterSizes.front()
				   << ",\"vertical_splitter_detail_size\":" << splitterSizes.back()
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
				   << ",\"semantic_index_adoptions\":" << stats.SemanticIndexAdoptions
				   << "}\n";
			stream.flush();
			return trace.error() == QFileDevice::NoError
				? true
				: fail(QStringLiteral("capture-trace-write"));
		};

		if (!capture("01-selected-composition")) {
			return fail(QStringLiteral("capture-selected-composition"));
		}
		if (!controller->reveal_segment(matching->Id, 0.2)) {
			return fail(QStringLiteral("matching-early-reveal"));
		}
		if (!controller->current_frame().has_value()) {
			return fail(QStringLiteral("matching-early-frame"));
		}
		const auto matchingEarly = *controller->current_frame();
		if (!capture("02-matching-early")) {
			return fail(QStringLiteral("capture-matching-early"));
		}
		if (!controller->reveal_segment(matching->Id, 0.8)) {
			return fail(QStringLiteral("matching-late-reveal"));
		}
		if (!controller->current_frame().has_value()) {
			return fail(QStringLiteral("matching-late-frame"));
		}
		const auto matchingLate = *controller->current_frame();
		if (matchingLate.Left.ProjectedDocumentY
			<= matchingEarly.Left.ProjectedDocumentY) {
			return fail(QStringLiteral("matching-left-projection-stationary"));
		}
		if (matchingLate.Right.ProjectedDocumentY
			<= matchingEarly.Right.ProjectedDocumentY) {
			return fail(QStringLiteral("matching-right-projection-stationary"));
		}
		if (!capture("03-matching-late")) {
			return fail(QStringLiteral("capture-matching-late"));
		}

		const auto inputOrigin = controller->maximum_position() * 0.45;
		controller->set_position(inputOrigin);
		if (!send_diff_evidence_wheel(
				*workbench->left_pane()->viewport(), QPoint{ 0, -32 })) {
			return fail(QStringLiteral("left-pane-wheel-rejected"));
		}
		const auto leftInputPosition = controller->position();
		// The receiver-equivalence steps below compare the right pane and the
		// divider against this value. Require the wheel to have actually moved
		// the retained position first, otherwise all three receivers would
		// agree trivially at the origin even if none of them handled the event.
		if (leftInputPosition <= inputOrigin) {
			return fail(QStringLiteral("left-pane-wheel-stationary"));
		}
		if (!capture("04-left-pane-input")) {
			return fail(QStringLiteral("capture-left-pane-input"));
		}

		if (!controller->reveal_segment(insertion->Id, 0.2)) {
			return fail(QStringLiteral("insertion-early-reveal"));
		}
		if (!controller->current_frame().has_value()) {
			return fail(QStringLiteral("insertion-early-frame"));
		}
		const auto insertionEarly = *controller->current_frame();
		if (!capture("05-insertion-early")) {
			return fail(QStringLiteral("capture-insertion-early"));
		}
		if (!controller->reveal_segment(insertion->Id, 0.8)) {
			return fail(QStringLiteral("insertion-late-reveal"));
		}
		if (!controller->current_frame().has_value()) {
			return fail(QStringLiteral("insertion-late-frame"));
		}
		const auto insertionLate = *controller->current_frame();
		if (std::abs(insertionLate.Left.ProjectedDocumentY
					 - insertionEarly.Left.ProjectedDocumentY)
			> 0.000'001) {
			return fail(QStringLiteral("insertion-left-projection-moved"));
		}
		if (insertionLate.Right.ProjectedDocumentY
			<= insertionEarly.Right.ProjectedDocumentY) {
			return fail(QStringLiteral("insertion-right-projection-stationary"));
		}
		if (!capture("06-insertion-late")) {
			return fail(QStringLiteral("capture-insertion-late"));
		}
		const auto insertionIterator = std::ranges::find(
			alignment.Segments,
			insertion->Id,
			[](const auto& aSegment) { return aSegment.Id; });
		if (insertionIterator == alignment.Segments.end()) {
			return fail(QStringLiteral("insertion-segment-not-indexed"));
		}
		const auto continuation = std::ranges::find_if(
			insertionIterator + 1,
			alignment.Segments.end(),
			[](const auto& aSegment) {
				return aSegment.Kind == bpr::core::alignment_kind::equal;
			});
		if (continuation == alignment.Segments.end()) {
			return fail(QStringLiteral("insertion-continuation-missing"));
		}
		if (!controller->reveal_segment(continuation->Id, 0.25)) {
			return fail(QStringLiteral("insertion-continuation-reveal"));
		}
		if (!capture("07-matching-continuation")) {
			return fail(QStringLiteral("capture-matching-continuation"));
		}
		if (!controller->reveal_segment(insertion->Id, 0.8)) {
			return fail(QStringLiteral("insertion-reverse-late-reveal"));
		}
		if (!controller->reveal_segment(insertion->Id, 0.2)) {
			return fail(QStringLiteral("insertion-reverse-early-reveal"));
		}
		if (!controller->current_frame().has_value()) {
			return fail(QStringLiteral("insertion-reversed-frame"));
		}
		const auto insertionReversed = *controller->current_frame();
		if (std::abs(insertionReversed.Left.ProjectedDocumentY
					 - insertionEarly.Left.ProjectedDocumentY)
			> 0.000'001) {
			return fail(QStringLiteral("insertion-reversed-left-mismatch"));
		}
		if (std::abs(insertionReversed.Right.ProjectedDocumentY
					 - insertionEarly.Right.ProjectedDocumentY)
			> 0.000'001) {
			return fail(QStringLiteral("insertion-reversed-right-mismatch"));
		}
		if (!capture("08-insertion-reversed")) {
			return fail(QStringLiteral("capture-insertion-reversed"));
		}

		controller->set_position(inputOrigin);
		if (!send_diff_evidence_wheel(
				*workbench->right_pane()->viewport(), QPoint{ 0, -32 })) {
			return fail(QStringLiteral("right-pane-wheel-rejected"));
		}
		const auto rightInputPosition = controller->position();
		if (std::abs(rightInputPosition - leftInputPosition) > 0.000'001) {
			return fail(QStringLiteral("right-pane-wheel-not-equivalent"));
		}
		if (rightInputPosition <= inputOrigin) {
			return fail(QStringLiteral("right-pane-wheel-stationary"));
		}
		if (!capture("09-right-pane-input")) {
			return fail(QStringLiteral("capture-right-pane-input"));
		}
		controller->set_position(inputOrigin);
		if (std::abs(controller->position() - inputOrigin) > 0.000'001) {
			return fail(QStringLiteral("divider-wheel-origin-mismatch"));
		}
		if (!send_diff_evidence_wheel(*workbench->link_map(), QPoint{ 0, -32 })) {
			return fail(QStringLiteral("divider-wheel-rejected"));
		}
		const auto dividerInputPosition = controller->position();
		if (std::abs(dividerInputPosition - leftInputPosition) > 0.000'001) {
			return fail(QStringLiteral("divider-wheel-not-equivalent"));
		}
		if (dividerInputPosition <= inputOrigin) {
			return fail(QStringLiteral("divider-wheel-stationary"));
		}
		if (!capture("10-divider-input")) {
			return fail(QStringLiteral("capture-divider-input"));
		}

		if (!controller->reveal_segment(deletion->Id, 0.2)) {
			return fail(QStringLiteral("deletion-early-reveal"));
		}
		if (!controller->current_frame().has_value()) {
			return fail(QStringLiteral("deletion-early-frame"));
		}
		const auto deletionEarly = *controller->current_frame();
		if (!capture("11-deletion-early")) {
			return fail(QStringLiteral("capture-deletion-early"));
		}
		if (!controller->reveal_segment(deletion->Id, 0.8)) {
			return fail(QStringLiteral("deletion-late-reveal"));
		}
		if (!controller->current_frame().has_value()) {
			return fail(QStringLiteral("deletion-late-frame"));
		}
		const auto deletionLate = *controller->current_frame();
		if (deletionLate.Left.ProjectedDocumentY
			<= deletionEarly.Left.ProjectedDocumentY) {
			return fail(QStringLiteral("deletion-left-projection-stationary"));
		}
		if (std::abs(deletionLate.Right.ProjectedDocumentY
					 - deletionEarly.Right.ProjectedDocumentY)
			> 0.000'001) {
			return fail(QStringLiteral("deletion-right-projection-moved"));
		}
		if (!capture("12-deletion-late")) {
			return fail(QStringLiteral("capture-deletion-late"));
		}

		if (!controller->reveal_segment(replacement->Id, 0.2)) {
			return fail(QStringLiteral("replacement-early-reveal"));
		}
		if (!controller->current_frame().has_value()) {
			return fail(QStringLiteral("replacement-early-frame"));
		}
		const auto replacementEarly = *controller->current_frame();
		if (!capture("13-replacement-early")) {
			return fail(QStringLiteral("capture-replacement-early"));
		}
		if (!controller->reveal_segment(replacement->Id, 0.8)) {
			return fail(QStringLiteral("replacement-late-reveal"));
		}
		if (!controller->current_frame().has_value()) {
			return fail(QStringLiteral("replacement-late-frame"));
		}
		const auto replacementLate = *controller->current_frame();
		const auto leftAdvance     = replacementLate.Left.ProjectedDocumentY
			- replacementEarly.Left.ProjectedDocumentY;
		const auto rightAdvance = replacementLate.Right.ProjectedDocumentY
			- replacementEarly.Right.ProjectedDocumentY;
		if (leftAdvance <= 0.0) {
			return fail(QStringLiteral("replacement-left-projection-stationary"));
		}
		if (rightAdvance <= leftAdvance) {
			return fail(QStringLiteral("replacement-right-projection-not-greater"));
		}
		if (!capture("14-replacement-late")) {
			return fail(QStringLiteral("capture-replacement-late"));
		}

		if (!controller->reveal_segment(replacement->Id, 0.5)) {
			return fail(QStringLiteral("replacement-center-reveal"));
		}
		settle_automated_events();
		const auto visible  = workbench->link_map()->visible_links();
		const auto ribbonIt = std::ranges::find(
			visible,
			replacement->Id,
			[](const bpr::qt::visible_diff_link& aLink) {
				return aLink.Identity.Segment;
			});
		if (ribbonIt == visible.end()) {
			return fail(QStringLiteral("replacement-ribbon-missing"));
		}
		if (ribbonIt->Path.isEmpty()) {
			return fail(QStringLiteral("replacement-ribbon-path-empty"));
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
			return fail(QStringLiteral("other-ribbon-missing"));
		}
		const auto& otherLink  = *otherIt;
		const auto  otherPoint = otherLink.Path.boundingRect().center();
		if (!otherLink.Path.contains(otherPoint)) {
			return fail(QStringLiteral("other-ribbon-center-outside"));
		}
		send_diff_evidence_click(*workbench->link_map(), otherPoint);
		settle_automated_events();
		if (!workbench->selection().has_value()) {
			return fail(QStringLiteral("other-ribbon-selection-missing"));
		}
		if (workbench->selection()->Group != otherLink.Identity.Group) {
			return fail(QStringLiteral("other-ribbon-selection-mismatch"));
		}
		if (workbench->selection()->Group == ribbon.Identity.Group) {
			return fail(QStringLiteral("other-ribbon-selection-not-distinct"));
		}
		if (!capture("15-other-ribbon-selected")) {
			return fail(QStringLiteral("capture-other-ribbon-selected"));
		}

		const auto point = ribbon.Path.boundingRect().center();
		if (!ribbon.Path.contains(point)) {
			return fail(QStringLiteral("replacement-ribbon-center-outside"));
		}
		const auto callbacksBeforeClick
			= workbench->link_map()->statistics().SelectionCallbacks;
		send_diff_evidence_click(*workbench->link_map(), point);
		settle_automated_events();
		if (!workbench->selection().has_value()) {
			return fail(QStringLiteral("replacement-ribbon-selection-missing"));
		}
		if (workbench->selection()->Group != ribbon.Identity.Group) {
			return fail(QStringLiteral("replacement-ribbon-selection-mismatch"));
		}
		if (!workbench->link_map()->selected().has_value()) {
			return fail(QStringLiteral("link-map-selection-missing"));
		}
		if (workbench->link_map()->selected()->Group
			!= workbench->selection()->Group) {
			return fail(QStringLiteral("link-map-selection-mismatch"));
		}
		if (workbench->link_map()->statistics().SelectionCallbacks
			!= callbacksBeforeClick + 1U) {
			return fail(QStringLiteral("ribbon-selection-callback-count"));
		}
		// [[BPR-1334]]. Line details follows the selection to its line.
		if (!line_details_show_selection(*workbench)) {
			return fail(QStringLiteral("ribbon-selection-detail-missing"));
		}
		if (!capture("16-ribbon-selection")) {
			return fail(QStringLiteral("capture-ribbon-selection"));
		}

		// [[BPR-1334]]. Line details is shown by default, so the splitter
		// that bounds it is there to drag without a review-details gesture.
		if (!workbench->line_details_visible()) {
			return fail(QStringLiteral("line-details-hidden-by-default"));
		}
		const auto selectionBeforeResize  = workbench->selection();
		const auto statisticsBeforeResize = workbench->statistics();
		const auto sizes                  = vertical->sizes();
		if (sizes.size() != 2) {
			return fail(QStringLiteral("vertical-splitter-shape"));
		}
		const auto total = std::accumulate(sizes.begin(), sizes.end(), 0);
		if (total <= 0) {
			return fail(QStringLiteral("vertical-splitter-empty"));
		}
		auto minimumTop = 0;
		auto maximumTop = 0;
		vertical->getRange(1, &minimumTop, &maximumTop);
		const auto currentTop = sizes.front();
		const auto targetTop  = currentTop < maximumTop
			? std::min(maximumTop, currentTop + 32)
			: std::max(minimumTop, currentTop - 32);
		if (targetTop == currentTop) {
			return fail(QStringLiteral("vertical-splitter-no-legal-target"));
		}
		const auto viewportBeforeResize = workbench->left_pane()->viewport()->height();
		vertical->setSizes({ targetTop, total - targetTop });
		const auto rebuildDeadline
			= std::chrono::steady_clock::now() + std::chrono::milliseconds{ 500 };
		while (workbench->statistics().GeometryRebuilds
				   == statisticsBeforeResize.GeometryRebuilds
			   && std::chrono::steady_clock::now() < rebuildDeadline) {
			QApplication::processEvents();
		}
		settle_automated_events();
		const auto sizesAfterResize = vertical->sizes();
		if (sizesAfterResize.size() != 2) {
			return fail(QStringLiteral("vertical-splitter-result-shape"));
		}
		if (sizesAfterResize == sizes) {
			return fail(QStringLiteral("vertical-splitter-size-unchanged"));
		}
		if (sizesAfterResize.front() != targetTop
			|| sizesAfterResize.back() != total - targetTop) {
			return fail(QStringLiteral("vertical-splitter-target-not-applied"));
		}
		if (workbench->left_pane()->viewport()->height() == viewportBeforeResize) {
			return fail(QStringLiteral("vertical-splitter-viewport-unchanged"));
		}
		const auto statisticsAfterResize = workbench->statistics();
		if (statisticsAfterResize.GeometryRebuilds
			!= statisticsBeforeResize.GeometryRebuilds + 1U) {
			return fail(QStringLiteral("vertical-splitter-rebuild-count"));
		}
		if (statisticsAfterResize.GeometryRebuildHistoryOverflow
			|| statisticsAfterResize.GeometryRebuildHistorySize
				!= statisticsBeforeResize.GeometryRebuildHistorySize + 1U) {
			return fail(QStringLiteral("vertical-splitter-rebuild-history"));
		}
		const auto rebuildTrigger
			= statisticsAfterResize
				  .GeometryRebuildHistory[statisticsAfterResize.GeometryRebuildHistorySize - 1U]
				  .Triggers;
		if ((rebuildTrigger & bpr::qt::diff_geometry_rebuild_trigger::pane_viewport_resize)
			== bpr::qt::diff_geometry_rebuild_trigger::none) {
			return fail(QStringLiteral("vertical-splitter-rebuild-attribution"));
		}
		if (workbench->selection() != selectionBeforeResize) {
			return fail(QStringLiteral("vertical-splitter-selection-changed"));
		}
		if (!capture("17-divider-resized")) {
			return fail(QStringLiteral("capture-divider-resized"));
		}

		// [[BPR-1291]]. The ends of the shared bar are where the panes stop:
		// both first lines at the top, then each last line at the top, as
		// VS Code scrolls past the end.
		const auto topLine = [](QPlainTextEdit& aPane) {
			return aPane.cursorForPosition(QPoint{ 0, 1 }).block().blockNumber();
		};
		const auto lastLine = [](QPlainTextEdit& aPane) {
			return aPane.document()->blockCount() - 1;
		};
		controller->home();
		if (std::abs(controller->position() - controller->minimum_position())
			> 0.000'001) {
			return fail(QStringLiteral("comparison-start-position"));
		}
		if (!capture("18-comparison-start")) {
			return fail(QStringLiteral("capture-comparison-start"));
		}
		if (topLine(*workbench->left_pane()) != 0 || topLine(*workbench->right_pane()) != 0) {
			return fail(QStringLiteral("comparison-start-not-first-line"));
		}
		controller->end();
		if (std::abs(controller->position() - controller->maximum_position())
			> 0.000'001) {
			return fail(QStringLiteral("comparison-end-position"));
		}
		if (!capture("19-comparison-end")) {
			return fail(QStringLiteral("capture-comparison-end"));
		}
		if (topLine(*workbench->left_pane()) != lastLine(*workbench->left_pane())
			|| topLine(*workbench->right_pane()) != lastLine(*workbench->right_pane())) {
			return fail(QStringLiteral("comparison-end-last-line-not-at-top"));
		}

		const auto finalStats = workbench->statistics();
		if (workbench->current_context() != initialContext) {
			return fail(QStringLiteral("final-context-changed"));
		}
		if (finalStats.SemanticIndexAdoptions != initialStats.SemanticIndexAdoptions) {
			return fail(QStringLiteral("final-semantic-index-rebuilt"));
		}
		if (!workbench->shared_scrollbar()->isEnabled()) {
			return fail(QStringLiteral("final-shared-scrollbar-disabled"));
		}
		if (workbench->left_pane()->verticalScrollBarPolicy()
			!= Qt::ScrollBarAlwaysOff) {
			return fail(QStringLiteral("final-left-independent-scrollbar"));
		}
		if (workbench->right_pane()->verticalScrollBarPolicy()
			!= Qt::ScrollBarAlwaysOff) {
			return fail(QStringLiteral("final-right-independent-scrollbar"));
		}
		return trace.error() == QFileDevice::NoError
			? true
			: fail(QStringLiteral("final-trace-write"));
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
	void append_fixture_conversation_identities(
		const bpr::qt::github_conversation_model& aModel,
		const QModelIndex&                        aParent,
		std::vector<std::string>&                 aIdentities)
	{
		for (auto row = 0; row < aModel.rowCount(aParent); ++row) {
			const auto index = aModel.index(row, 0, aParent);
			aIdentities.push_back(aModel.id_for_index(index));
			append_fixture_conversation_identities(aModel, index, aIdentities);
		}
	}

	[[nodiscard]] auto fixture_conversation_identities(
		const bpr::qt::github_conversation_model& aModel)
		-> std::vector<std::string>
	{
		auto identities = std::vector<std::string>{ };
		append_fixture_conversation_identities(aModel, { }, identities);
		return identities;
	}

	[[nodiscard]] auto fixture_diagnostic_text(QString aValue) -> QString
	{
		aValue.replace(u'\\', QStringLiteral("\\\\"));
		aValue.replace(u'\r', QStringLiteral("\\r"));
		aValue.replace(u'\n', QStringLiteral("\\n"));
		return aValue;
	}

	[[nodiscard]] auto fixture_epoch_refresh_identity_text(
		const bpr::app::detail::epoch_refresh_identity_evidence& aIdentity,
		const bpr::qt::workbench_window&                         aWindow) -> QString
	{
		const auto file = [](const std::optional<bpr::core::file_identity>& aFile) {
			if (!aFile) {
				return QStringLiteral("none");
			}
			return diff_evidence_identity_hex(*aFile);
		};
		const auto pair = [](const std::optional<bpr::core::comparison_pair>& aPair) {
			if (!aPair) {
				return QStringLiteral("none");
			}
			const auto oid = [](const bpr::core::git_commit_oid& aOid) {
				const auto canonical = bpr::core::canonical_hex(aOid);
				const auto text      = canonical.view();
				return QString::fromLatin1(text.data(), static_cast<qsizetype>(text.size()));
			};
			return oid(aPair->Left.Commit) + QLatin1Char{ '/' } + oid(aPair->Right.Commit);
		};
		const auto context = aWindow.comparison_surface()->current_context();
		const auto anchor  = aWindow.conversation_panel()->anchor_line();
		return QStringLiteral("host_file=%1 tab_id=%2 tab_file=%3 tab_pair=%4 tab_state=%5 tab_pending=%6 accepted_pair=%7 comparison_pending=%8 published_file=%9 published_pair=%10 displayed_file=%11 publication_displayed=%12 anchor_file=%13 anchor_line=%14 displayed_pair=%15 exact_tab_pair=%16 exact_published_pair=%17 tab_modality=%18 window_modality=%19")
			.arg(file(aIdentity.SelectedFile))
			.arg(aIdentity.ActiveTab ? QString::number(aIdentity.ActiveTab->Id.value()) : QStringLiteral("none"))
			.arg(file(aIdentity.ActiveTab ? aIdentity.ActiveTab->View.File : std::nullopt))
			.arg(pair(aIdentity.ActiveTab ? std::optional{ aIdentity.ActiveTab->View.Comparison } : std::nullopt))
			.arg(aIdentity.ActiveTab ? static_cast<int>(aIdentity.ActiveTab->State) : -1)
			.arg(aIdentity.TabPending)
			.arg(pair(aIdentity.AcceptedComparison))
			.arg(aIdentity.ComparisonPending)
			.arg(file(aIdentity.PublishedFile))
			.arg(pair(aIdentity.PublishedComparison))
			.arg(file(context ? std::optional{ context->Key.File } : std::nullopt))
			.arg(aIdentity.PublicationDisplayed)
			.arg(file(aWindow.conversation_panel()->anchor_file()))
			.arg(anchor ? QStringLiteral("%1/%2").arg(static_cast<int>(anchor->Side)).arg(anchor->Line) : QStringLiteral("none"))
			.arg(pair(aIdentity.PublicationDisplayed ? aIdentity.PublishedComparison : std::nullopt))
			.arg(aIdentity.ActiveTab && aIdentity.AcceptedComparison == std::optional{ aIdentity.ActiveTab->View.Comparison })
			.arg(aIdentity.PublishedComparison.has_value() && aIdentity.PublishedComparison == aIdentity.AcceptedComparison)
			.arg(aIdentity.TabModality)
			.arg(static_cast<int>(aWindow.current_modality()));
	}

	/// \brief [[BPR-1198]]. A refreshed tab must finish presenting its exact
	/// view; the temporary empty gap is not an unselected terminal.
	[[nodiscard]] auto fixture_epoch_refresh_identity_passed(
		const bpr::app::detail::epoch_refresh_identity_evidence& aIdentity,
		const bpr::qt::workbench_window&                         aWindow) -> bool
	{
		const auto& context     = aWindow.comparison_surface()->current_context();
		const auto* panel       = aWindow.conversation_panel();
		const auto  tabModality = aWindow.current_modality() == bpr::qt::review_modality::review;
		if (aIdentity.ComparisonPending || panel->anchor_line() || tabModality != aIdentity.TabModality) {
			return false;
		}
		// Overview and Discuss retain the tab collection without presenting
		// it. Their unselected refresh still owes all review-wide roots.
		if (!aIdentity.TabModality || !aIdentity.ActiveTab) {
			return !aIdentity.SelectedFile && !aIdentity.PublishedFile
				&& !context && !panel->anchor_file();
		}
		const auto& tab = *aIdentity.ActiveTab;
		return !aIdentity.TabPending && tab.State == bpr::core::document_tab_view_state::ready
			&& tab.View.File == std::optional{ bpr::core::file_identity_of("review.cpp") }
		&& aIdentity.SelectedFile == tab.View.File
			&& aIdentity.PublishedFile == aIdentity.SelectedFile
			&& context && context->Key.File == aIdentity.SelectedFile
			&& aIdentity.AcceptedComparison == std::optional{ tab.View.Comparison }
		&& aIdentity.PublishedComparison == aIdentity.AcceptedComparison
			&& aIdentity.PublicationDisplayed
			&& panel->anchor_file() == aIdentity.SelectedFile;
	}

	[[nodiscard]] auto fixture_presentation_passed(
		bpr::qt::workbench_window&      aWindow,
		const std::string_view          aExpectedActiveHostText,
		const fixture_run_expectations& aExpected,
		const bool                      aFocused = false) -> bool
	{
		const auto* const title              = aWindow.findChild<QLabel*>(QStringLiteral("pullRequestTitle"));
		const auto        expectedRows       = aFocused
			? aExpected.ExpectedFocusedConversationRows
			: aExpected.ExpectedConversationRows;
		const auto&       expectedIdentities = aFocused
			? aExpected.ExpectedFocusedConversationIdentities
			: aExpected.ExpectedConversationIdentities;
		return aWindow.github_status_text() == "Pull request ready."
			&& aWindow.active_account_text() == aExpected.ExpectedAccountText
			&& aWindow.active_host_text() == aExpectedActiveHostText
			&& aWindow.github_open_action()->isEnabled()
			&& aWindow.github_refresh_action()->isEnabled()
			&& title != nullptr
			&& title->text() == QString::fromStdString(aExpected.ExpectedTitle)
			&& aWindow.conversation_model()->rowCount() == expectedRows
			&& fixture_conversation_identities(*aWindow.conversation_model())
			== expectedIdentities
			&& (!aExpected.ExpectedThreadId.has_value()
				|| aWindow.conversation_model()
					   ->index_for_id(*aExpected.ExpectedThreadId)
					   .isValid())
			&& aWindow.statistics().GithubFullResets == 0U;
	}

	[[nodiscard]] auto fixture_epoch_presentation_passed(
		const bpr::qt::workbench_window& aWindow,
		const std::size_t                aMinimumEpochs = 1U) -> bool
	{
		const auto* const control = aWindow.epoch_selectors();
		return control->status_text().startsWith(
				   QStringLiteral("Review history is ready."))
			&& control->comparison_text().endsWith(
				QStringLiteral("Ready for diff."))
			&& control->history_model()->epoch_count() >= aMinimumEpochs
			&& !control->left_oid_editor()->text().isEmpty()
			&& !control->right_oid_editor()->text().isEmpty();
	}

	[[nodiscard]] auto fixture_epoch_pair_passed(
		const bpr::qt::workbench_window& aWindow,
		const std::string&               aLeft,
		const std::string&               aRight,
		const std::size_t                aMinimumEpochs = 2U) -> bool
	{
		const auto* const control = aWindow.epoch_selectors();
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

	void trigger_settings_maintenance(bpr::qt::workbench_window& aWindow, QAction* aAction)
	{
		aWindow.open_settings(bpr::qt::settings_page::storage_and_privacy);
		auto* const page = aWindow.settings()->page_widget(bpr::qt::settings_page::storage_and_privacy);
		for (auto* button : page->findChildren<QToolButton*>()) {
			if (button->defaultAction() == aAction) {
				button->click();
				return;
			}
		}
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
		auto* const control = aApplication.window().epoch_selectors();
		auto* const model   = control->history_model();
		if (aRun.MaintenanceProbe
			== automated_run::epoch_maintenance_probe::verify_deleted) {
			const auto provider = bpr::app::detail::
				github_application_fixture_access::evidence(aApplication);
			const auto maintenance = bpr::app::detail::
				github_application_fixture_access::maintenance_evidence(
					aApplication, aRun.MaintenanceTarget);
			// Every condition the closing assertion checks, the two per-target
			// deleted flags included. Returning true here is what ends the
			// polling and begins shutdown, so a condition this gate omits is
			// one the assertion then reads from whatever state happened to
			// exist when the run stopped waiting.
			//
			// Those two flags settle later than the retired marker beside
			// them. The marker is written by the deletion itself, while the
			// flags are read back from a catalog this process — a fresh one,
			// started after the delete — must first rediscover and rehydrate.
			// Polling on the marker alone therefore declared success while the
			// flags were still false, and the assertion failed about half the
			// time, a quarter second into a sixty-second budget it never came
			// close to expiring. BPR-0345.
			const auto settled = provider.SmartAcquireCount == 1
				&& provider.SmartDiscoveryCount == 1
				&& provider.SmartLsRefsCount == 1
				&& provider.SmartFetchCount == 1
				&& maintenance.TargetBaseCacheDeleted
				&& maintenance.TargetHeadCacheDeleted
				&& !maintenance.RetiredRepositoryPresent
				&& maintenance.RetiredCacheDeletionMarkerPresent
				&& maintenance.PruneIntentDispatchCount == 0U
				&& maintenance.DeleteIntentDispatchCount == 0U;
			if (settled) {
				aRun.MaintenanceObserved               = true;
				aRun.MaintenanceTargetBaseCacheDeleted = maintenance.TargetBaseCacheDeleted;
				aRun.MaintenanceTargetHeadCacheDeleted = maintenance.TargetHeadCacheDeleted;
			}
			aRun.MaintenanceGate = settled ? nullptr : "deleted-state-unsettled";
			return settled;
		}
		if (model->epoch_count() < 2U) {
			if (control->return_to_newest_action()->isEnabled()) {
				control->return_to_newest_action()->trigger();
			} else if (control->load_history_action()->isEnabled()) {
				control->load_history_action()->trigger();
			} else {
				aRun.MaintenanceGate = "history-unavailable";
				return false;
			}
			aRun.MaintenanceGate = nullptr;
			advance_fixture_stage(
				aRun, automated_run::stage::epoch_maintenance_history);
			return true;
		}

		const auto target = fixture_epoch_id(*model, 1U);
		if (!target) {
			aRun.MaintenanceGate = "target-epoch-absent";
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
				aRun.MaintenanceGate = "seeded-state";
				return false;
			}
			aRun.MaintenanceAccountReads  = provider.AccountReadCount;
			aRun.MaintenanceSmartAcquires = provider.SmartAcquireCount;
			aRun.MaintenanceModelResets   = model->statistics().ModelResets;
			aRun.MaintenanceGate          = nullptr;
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
				aRun.MaintenanceGate = "pruned-state";
				return false;
			}
			if (!aRun.DiffRequested) {
				// Refused while the restarted process has not accepted an
				// exact comparison yet: `request_fixture_diff` reads
				// `AcceptedComparison` and returns false while the resolution
				// is still in flight. That is the state this run is waiting
				// for, not a wrong one. BPR-0447.
				aRun.DiffRequested = bpr::app::detail::
					github_application_fixture_access::request_fixture_diff(
						aApplication, true);
				aRun.MaintenanceGate = aRun.DiffRequested
					? nullptr
					: "comparison-not-resolved";
				return aRun.DiffRequested;
			}
			if (aApplication.window().comparison_surface()->current_result()
				== nullptr) {
				aRun.MaintenanceGate = "diff-result-pending";
				return true;
			}
			aRun.MaintenanceAccountReads  = provider.AccountReadCount;
			aRun.MaintenanceSmartAcquires = provider.SmartAcquireCount;
			aRun.MaintenanceModelResets   = model->statistics().ModelResets;
			aRun.MaintenanceGate          = nullptr;
			accept_maintenance_confirmation(
				QStringLiteral("deleteRepositoryCacheConfirmationDialog"));
			trigger_settings_maintenance(aApplication.window(), control->delete_cache_action());
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

	// [[BPR-0526]]. Starts the device flow the way a reviewer does, and finds
	// the control the way an assistive reader does: by walking the shipped
	// window for the accessible name the panel announces. Nothing here reaches
	// through a composition accessor, so a window that stopped mounting the
	// panel, a panel that stopped announcing itself, or a composition that
	// never wired the panel's dispatch each end here with `false` rather than
	// with a flow that started by another route.
	[[nodiscard]] auto begin_through_device_panel(
		const bpr::qt::workbench_window& aWindow) -> bool
	{
		for (auto* const widget : aWindow.findChildren<QWidget*>()) {
			if (widget->accessibleName()
				!= QStringLiteral("Sign in with a device code")) {
				continue;
			}
			for (auto* const button : widget->findChildren<QPushButton*>()) {
				if (button->accessibleName()
					!= QStringLiteral("Get a device sign-in code")) {
					continue;
				}
				if (!button->isEnabled()) {
					return false;
				}
				// Exercise the reviewer entry, not the second button that the
				// old flow required after opening Settings.
				auto* const homeSignIn = aWindow.findChild<QPushButton*>(
					QStringLiteral("workbenchHomePrimaryButton"));
				if (homeSignIn == nullptr || !homeSignIn->isEnabled()
					|| homeSignIn->accessibleName() != QStringLiteral("Sign in to GitHub")) {
					return false;
				}
				homeSignIn->click();
				const auto* dialog = aWindow.sign_in_dialog();
				return dialog && dialog->isVisible() && dialog->isAncestorOf(button)
					&& !aWindow.settings()->isVisible();
			}
			return false;
		}
		return false;
	}

	// [[BPR-0535]], repointed by [[BPR-0771]] and [[BPR-0772]]. A begin pressed
	// for an enterprise host must end in the typed refusal with nothing built.
	// The picker that once chose that host is retired, and an enterprise
	// server's card offers no device sign-in, so the course first requires
	// exactly that absence on the GHES card, then binds the still-mounted
	// device panel to that server's native profile and presses its begin. A
	// window with no GHES card, a GHES card that offers the device flow, or no
	// mounted panel ends here with `false`.
	[[nodiscard]] auto begin_through_ghes_device_panel(
		const bpr::qt::workbench_window& aWindow) -> bool
	{
		for (auto* const card : aWindow.server_cards()) {
			const auto* const native = card->native_option();
			if (
				native == nullptr
				|| native->Product != bpr::application::github_profile_product::github_enterprise_server) {
				continue;
			}
			auto* const panel = aWindow.device_authorization();
			if (card->offers_device_sign_in() || panel == nullptr) {
				return false;
			}
			panel->bind_profile(native->Profile);
			auto* const begin = panel->begin_button();
			if (begin == nullptr || !begin->isEnabled()) {
				return false;
			}
			begin->click();
			return true;
		}
		return false;
	}

	// [[BPR-0771]], [[BPR-0772]]. The CLI host profile is a way in to its
	// server's card, chosen by that card's GitHub CLI offer. The card the run
	// activated, or nothing.
	// [[BPR-0797]]. The three answers, in the words a course pins.
	[[nodiscard]] auto composed_device_browser_text(
		const bpr::app::composed_device_browser aBrowser) -> const char*
	{
		switch (aBrowser) {
		case bpr::app::composed_device_browser::none:
			return "absent";
		case bpr::app::composed_device_browser::system:
			return "system";
		case bpr::app::composed_device_browser::recording:
			return "recording";
		}
		return "absent";
	}

	// [[BPR-0982]]. What every composition owes before any gesture, now that
	// GitHub CLI is discovered rather than precomposed ([[BPR-0909]]): no
	// server card carries a GitHub CLI profile or offer, nothing is signed in,
	// and the Use GitHub CLI entry is offered exactly when the composed run
	// offers the read. Inside the automated test boundary that means absent,
	// so no gesture in any course can reach `gh`; in a macOS product run it
	// means offered, and the course still presses nothing.
	[[nodiscard]] auto cli_entry_presentation_passed(
		const bpr::qt::workbench_window& aWindow,
		const automated_run&             aRun) -> bool
	{
		const auto        cards     = aWindow.server_cards();
		const auto* const entry     = aWindow.use_github_cli_button();
		const auto        offered   = entry != nullptr && !entry->isHidden() && entry->isEnabled();
		const auto        noCliRow  = std::ranges::none_of(cards, [](const bpr::qt::server_card* const aCard) {
			return aCard->cli_option() != nullptr || aCard->cli_offer_button() != nullptr;
		});
		const auto        untouched = aWindow.active_account_text() == "Not authenticated"
			&& aWindow.statistics().GithubFullResets == 0U;
		return !cards.empty() && noCliRow && offered == aRun.Cli.ReadOffered && untouched;
	}

	[[nodiscard]] auto os_color_scheme_name(
		const bpr::core::os_color_scheme aScheme) -> QString
	{
		switch (aScheme) {
		case bpr::core::os_color_scheme::light:
			return QStringLiteral("light");
		case bpr::core::os_color_scheme::dark:
			return QStringLiteral("dark");
		case bpr::core::os_color_scheme::unknown:
			return QStringLiteral("unknown");
		}
		return QStringLiteral("unknown");
	}

	[[nodiscard]] auto appearance_variant_name(
		const bpr::core::appearance_variant aVariant) -> QString
	{
		switch (aVariant) {
		case bpr::core::appearance_variant::light:
			return QStringLiteral("light");
		case bpr::core::appearance_variant::dark:
			return QStringLiteral("dark");
		case bpr::core::appearance_variant::high_contrast_light:
			return QStringLiteral("high-contrast-light");
		case bpr::core::appearance_variant::high_contrast_dark:
			return QStringLiteral("high-contrast-dark");
		}
		return QStringLiteral("light");
	}

	[[nodiscard]] auto device_authorization_stage_text(
		const bpr::core::device_authorization_stage aStage) -> const char*
	{
		switch (aStage) {
		case bpr::core::device_authorization_stage::idle:
			return "idle";
		case bpr::core::device_authorization_stage::requested:
			return "requested";
		case bpr::core::device_authorization_stage::awaiting_authorization:
			return "awaiting_authorization";
		case bpr::core::device_authorization_stage::cancelling:
			return "cancelling";
		case bpr::core::device_authorization_stage::authorized:
			return "authorized";
		case bpr::core::device_authorization_stage::denied:
			return "denied";
		case bpr::core::device_authorization_stage::expired:
			return "expired";
		case bpr::core::device_authorization_stage::disabled:
			return "disabled";
		case bpr::core::device_authorization_stage::unavailable:
			return "unavailable";
		case bpr::core::device_authorization_stage::cancelled:
			return "cancelled";
		case bpr::core::device_authorization_stage::failed:
			return "failed";
		}
		return "idle";
	}

	[[nodiscard]] auto device_authorization_handoff_text(
		const bpr::application::device_authorization_handoff aHandoff)
		-> const char*
	{
		switch (aHandoff) {
		case bpr::application::device_authorization_handoff::not_attempted:
			return "not_attempted";
		case bpr::application::device_authorization_handoff::opened:
			return "opened";
		case bpr::application::device_authorization_handoff::refused:
			return "refused";
		case bpr::application::device_authorization_handoff::failed:
			return "failed";
		case bpr::application::device_authorization_handoff::unavailable:
			return "unavailable";
		}
		return "not_attempted";
	}

	// [[BPR-0536]]. Printed by name rather than by number: a course whose
	// renewal landed in `reenrollment_required` and one that never armed at
	// all are different failures, and an integer marker would read the same.
	[[nodiscard]] auto device_refresh_state_name(
		const bpr::application::device_authorization_refresh_state aState)
		-> const char*
	{
		switch (aState) {
		case bpr::application::device_authorization_refresh_state::idle:
			return "idle";
		case bpr::application::device_authorization_refresh_state::scheduled:
			return "scheduled";
		case bpr::application::device_authorization_refresh_state::refreshing:
			return "refreshing";
		case bpr::application::device_authorization_refresh_state::
			reenrollment_required:
			return "reenrollment_required";
		}
		return "idle";
	}

	void finish_automated_run(
		bpr::app::github_application& aApplication,
		const automated_run&          aRun)
	{
		const auto status                    = aApplication.status();
		const auto nativeVaultEvidencePassed = !aRun.VerifyNativePlatformVault
			|| status.NativePlatformVaultSelected
				== native_platform_vault_expected();
		const auto cliEntryEvidencePassed = !aRun.Cli.Enabled
			|| (aRun.Cli.Expected > 0U
				&& aRun.Cli.Observed == aRun.Cli.Expected);
		auto passed = aRun.PresentationPassed && status.ShutdownComplete
			&& !status.ShutdownCleanupFailed
			&& !status.EpochProjectionDeliveryFailed
			&& !status.EpochHandoffDeliveryFailed
			&& nativeVaultEvidencePassed
			&& cliEntryEvidencePassed;
		auto renderSaved = aRun.RenderSaved;
#if defined(BPR_BUILD_TESTING)
		auto nativeAppearancePersistence
			= native_appearance_persistence_disposition::not_attempted;
#endif

		// `passed` is narrowed in four places. Reporting only the first group
		// meant a failure in any later one printed a verdict in which
		// everything looked fine, which is exactly what made BPR-0133
		// unattributable.
		const auto passedCoreFacts   = passed;
		auto       passedFixture     = passed;
		auto       passedMaintenance = passed;
#if defined(BPR_BUILD_TESTING)
		// True when the fixture has nothing to say, which is the honest reading
		// for a run that never engaged one: the fixture did not fail.
		//
		// Declared inside the guard because every use of it is: a tests-
		// disabled build has no fixture to have facts about, and an unused
		// local is an error under this tree's warning policy.
		auto fixtureFactsPassed = true;
#endif

#if defined(BPR_BUILD_TESTING)
		const auto fixtureEvidence = aRun.Fixture
			? bpr::app::detail::github_application_fixture_access::evidence(
				  aApplication)
			: bpr::app::detail::github_public_path_fixture_evidence{ };
		// The registered device course composes the fixture for its recorded
		// grant and ends the run at the grant's terminal, so the fixture's
		// pull-request course deliberately never runs. Judging it on evidence
		// it was never asked to produce would report a failure that is a
		// description of the run rather than a defect in it.
		// The public-preview courses run against the fixture without opening
		// it the way the pull-request course does, so the pull-request
		// expectations below are not theirs to satisfy.
		// [[BPR-0540]]. The orphan course ends at its accepted session for the
		// same reason: it never opens the fixture's pull request, so the
		// pull-request expectations are not its to satisfy.
		const auto fixtureCourseRan = aRun.Fixture && !aRun.Device.Registered
			&& !aRun.Device.OrphanCourse && !aRun.PublicPreview.Enabled;
		if (fixtureCourseRan) {
			// [[BPR-1331]]. The push course's first run reads one head, so the
			// pushed head is fetched once when it arrives, as a reviewer's is.
			// [[BPR-1345]]. The start-a-review course's restarted run reads the
			// same head over the same data root, whose objects are already held.
			// [[BPR-1374]]. So does the unsent-reply course's restart.
			const auto expectedSmartCalls = aRun.StartReviewCourse == std::optional{ bpr::app::detail::start_review_course::finish }
					|| aRun.ReplyDraftCourse == std::optional{ bpr::app::detail::reply_draft_course::restart }
				? 0
				: aRun.MaintenanceProbe
						== automated_run::epoch_maintenance_probe::verify_deleted
					|| aRun.ReviewProgressPushProbe.has_value()
				? 1
				: expected_fixture_smart_calls(aRun.EpochPhase);
			// The fixture's own verdict, computed from the fixture's own facts
			// and nothing else. Folding it into `passed` and then printing that
			// under a fixture-shaped marker is what made BPR-0154 take three
			// occurrences and two wrong hypotheses to attribute: a run whose
			// presentation failed printed GITHUB_FIXTURE_FAILED while the
			// fixture had been working perfectly throughout.
			//
			// The run-level verdict is `PUBLIC_PATH_OK` / `PUBLIC_PATH_FAILED`,
			// and every gate that matches the fixture marker already requires
			// that one beside it, so naming this honestly costs no assertion.
			fixtureFactsPassed = aRun.ReviewLedgerCompositionProbe.has_value()
				? aRun.ReviewLedgerCompositionPassed
					&& fixtureEvidence.CapabilityProbeConsumed
					&& fixtureEvidence.BootstrapTransportSelected
					&& fixtureEvidence.AccountTransportSelected
					&& fixtureEvidence.CredentialStored
					&& fixtureEvidence.CredentialRetrieved
					&& fixtureEvidence.CredentialDeleted
					&& !fixtureEvidence.SecretRetained
				: aRun.DefectCompositionProbe.has_value()
				? aRun.DefectCompositionPassed
					&& fixtureEvidence.CapabilityProbeConsumed
					&& fixtureEvidence.BootstrapTransportSelected
					&& fixtureEvidence.AccountTransportSelected
					&& fixtureEvidence.CredentialStored
					&& fixtureEvidence.CredentialRetrieved
					&& fixtureEvidence.CredentialDeleted
					&& !fixtureEvidence.SecretRetained
				: fixtureEvidence.RecordingConsumed
					&& fixtureEvidence.CapabilityProbeConsumed
					&& fixtureEvidence.BootstrapTransportSelected
					&& fixtureEvidence.AccountTransportSelected
					&& fixtureEvidence.CredentialStored
					&& fixtureEvidence.CredentialRetrieved
					&& fixtureEvidence.CredentialDeleted
					// A retained secret is the defect, so this reads false when the
					// credential correctly did not survive its deletion. `11111110`
					// is the all-good pattern, which is the other half of what
					// misled BPR-0154.
					&& !fixtureEvidence.SecretRetained
					&& fixtureEvidence.SmartAcquireCount == expectedSmartCalls
					&& fixtureEvidence.SmartDiscoveryCount == expectedSmartCalls
					&& fixtureEvidence.SmartLsRefsCount == expectedSmartCalls
					&& fixtureEvidence.SmartFetchCount == expectedSmartCalls;
			passed             = passed && fixtureFactsPassed;
			passedFixture      = passed;
			passedMaintenance  = passed;
			if (aRun.MaintenanceProbe
				== automated_run::epoch_maintenance_probe::verify_deleted) {
				const auto maintenance = bpr::app::detail::
					github_application_fixture_access::maintenance_evidence(
						aApplication, aRun.MaintenanceTarget);
				// The two deleted flags come from the probe's record rather
				// than from this call. `MaintenanceObserved` is what keeps that
				// from being a tautology: it is set only where the probe
				// actually saw both flags true, so a run whose probe never
				// settled — one that ended on its budget instead — arrives here
				// with the record empty and fails. The remaining conditions are
				// re-read, because they survive shutdown.
				passed = passed && aRun.MaintenanceObserved
					&& aRun.MaintenanceTargetBaseCacheDeleted
					&& aRun.MaintenanceTargetHeadCacheDeleted
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
			if (passed && appearancePassed) {
				nativeAppearancePersistence
					= persist_native_appearance_evidence(
						aRun.Appearance.InitialRender,
						aRun.Appearance.ChangedRender,
						aRun.Appearance.EvidencePaths);
			}
			renderSaved = nativeAppearancePersistence
				== native_appearance_persistence_disposition::persisted;
			passed = passed && appearancePassed && renderSaved;
		} else {
			passed = passed && renderSaved;
		}
#else
		passed = passed && renderSaved;
#endif

#if defined(BPR_BUILD_TESTING)
		auto dormantSelection = std::optional<
			bpr::app::detail::dormant_live_provider_selection_evidence>{ };
		if (aRun.ReviewRoundTripCompositionProbe) {
			dormantSelection = bpr::app::detail::github_application_fixture_access::
				dormant_live_provider_selection_evidence_for(aApplication);
			passed = passed && dormantSelection->ExactTupleAccepted
				&& dormantSelection->MismatchedTuplesRejected
					== dormantSelection->ExpectedMismatchRejections
				&& dormantSelection->FixtureProviderSelected
				&& dormantSelection->LiveProviderAbsent;
		}
#endif

		// [[BPR-0540]]. Lifted out of the tests-enabled block above so the
		// device rows below can report the refresh slot inside the one stream
		// chain they already build. A tests-disabled build composes no
		// fixture, has no second slot, and runs none of the courses that read
		// these lines.
		auto refreshRetained  = false;
		auto refreshStores    = std::size_t{ 0U };
		auto refreshDeletes   = std::size_t{ 0U };
		auto refreshRetrieves = std::size_t{ 0U };
		auto pastedDigest     = std::string{ };
		auto refreshDigest    = std::string{ };
#if defined(BPR_BUILD_TESTING)
		refreshRetained  = fixtureEvidence.RefreshRetained;
		refreshStores    = fixtureEvidence.RefreshStores;
		refreshDeletes   = fixtureEvidence.RefreshDeletes;
		refreshRetrieves = fixtureEvidence.RefreshRetrieveAttempts;
		pastedDigest     = fixtureEvidence.LastEnrollmentDigest;
		refreshDigest    = fixtureEvidence.RefreshStoredDigest;
#endif

		QTextStream stream{ stdout };
#if defined(BPR_BUILD_TESTING)
		if (aRun.Fixture) {
			const auto providerMarker = bpr::app::detail::
				github_application_fixture_access::review_mutation_provider_marker(
					aApplication);
			stream << "REVIEW_MUTATION_PROVIDER:"
				   << QByteArray{ providerMarker.data(),
								  static_cast<qsizetype>(providerMarker.size()) }
				   << '\n'
				   << "GITHUB_FIXTURE_QT_PLATFORM:"
				   << QGuiApplication::platformName() << '\n'
				   << "GITHUB_FIXTURE_QT_STYLE:"
				   << aApplication.window().style()->metaObject()->className()
				   << '\n';
		}
#endif
		const auto elapsedMs = static_cast<long long>(
			std::chrono::duration_cast<std::chrono::milliseconds>(
				automated_clock::now() - aRun.Started)
				.count());
		const auto budgetMs = static_cast<long long>(aRun.Budget.count());
		// BPR-0555. A run that ran out of clock says so as a clock, before
		// anything attributes the failure to whatever it happened to be doing.
		// Four rows of the RFC-BPR-0016 closeout matrix reported only
		// `EPOCH_FIXTURE_SEED_FAILED` on a host that was reindexing, which reads
		// as a defect in the epoch path; the elapsed time against the bound was
		// present, but in the verdict block, which prints after the marker that
		// had already named the wrong thing.
		if (aRun.BudgetExpired) {
			stream << "RUN_BUDGET_EXPIRED:"
				   << "elapsed_ms=" << elapsedMs << " budget_ms=" << budgetMs
				   << " over_ms=" << (elapsedMs - budgetMs) << " stage="
				   << (aRun.FailedStage >= 0 ? aRun.FailedStage
											 : static_cast<int>(aRun.Stage))
				   << " status=\""
				   << QString::fromStdString(
						  aApplication.window().github_status_text())
				   << "\"\n";
		}
		// BPR-0905. Passing runs expose their margin too. The flag records an
		// expiry during the course; elapsed time also includes its shutdown.
		stream << "PUBLIC_PATH_BUDGET:"
			   << "expired=" << (aRun.BudgetExpired ? 1 : 0)
			   << " budget_ms=" << budgetMs << " elapsed_ms=" << elapsedMs
			   << " stage_ticks="
			   << static_cast<unsigned long long>(aRun.Attempts) << '\n';
		stream << (passed ? "PUBLIC_PATH_OK\n" : "PUBLIC_PATH_FAILED\n");
		if (aRun.VerifyNativePlatformVault) {
			stream << "NATIVE_PLATFORM_VAULT_SELECTED:"
				   << (status.NativePlatformVaultSelected ? "true" : "false")
				   << '\n';
		}
		if (aRun.CapabilityProbe) {
			// [[BPR-0797]]. Which two capabilities this run composed, named
			// rather than counted. Every other course here either injects its
			// own opener or asserts widgets and sentences, so every one of them
			// passed while the button a person presses reached a recorder that
			// answers `true` without opening anything -- and the account row
			// said "Opened in your browser." about it.
			//
			// This starts nothing. The browser is named, not called; the CLI
			// factory is asked whether it can serve a read, not asked to serve
			// one. The address that would be opened does not exist in a run
			// that never reaches `awaiting_authorization`.
			//
			// [[BPR-0982]]. The CLI way in is the Use GitHub CLI entry since
			// [[BPR-0909]]; no card carries a precomposed CLI label to report.
			const auto* const cliEntry = aApplication.window().use_github_cli_button();
			const auto        cliEntryOffered
				= cliEntry != nullptr && !cliEntry->isHidden() && cliEntry->isEnabled();
			stream << "COMPOSITION_TEST_BOUNDARY:"
				   << (status.InsideAutomatedTestBoundary ? "inside" : "outside")
				   << "\nCOMPOSITION_BROWSER:"
				   << composed_device_browser_text(
						  status.DeviceAuthorizationBrowser)
				   << "\nCOMPOSITION_CLI_READ:"
				   << (status.ComposedCliReadAvailable ? "available"
													   : "unavailable")
				   << "\nCOMPOSITION_CLI_ENTRY:"
				   << (cliEntryOffered ? "offered" : "absent") << '\n';
		}
		if (aRun.Cli.Enabled) {
			// [[BPR-0797]], [[BPR-0982]]. The marker names the behaviour this
			// run's composition owes, a fact about the run rather than the
			// build: the Use GitHub CLI entry offered where the read is, absent
			// elsewhere, and no server card carrying a GitHub CLI row either way.
			if (aRun.Cli.ReadOffered) {
				stream << (passed ? "CLI_ENTRY_OFFERED_OK\n"
								  : "CLI_ENTRY_OFFERED_FAILED\n");
			} else {
				stream << (passed ? "CLI_ENTRY_ABSENT_OK\n"
								  : "CLI_ENTRY_ABSENT_FAILED\n");
			}
			stream << "CLI_ENTRY_CARDS:"
				   << static_cast<unsigned long long>(aRun.Cli.Cards)
				   << '\n';
		}
		if (aRun.Device.Enabled) {
			// Reported as separate facts rather than one verdict, because "the
			// host was composed", "the choice was presented", "the answer was
			// the typed refusal", and "nothing was sent" fail for different
			// reasons and a single OK would hide which.
			stream << "DEVICE_FLOW_HOST_COMPOSED:"
				   << (status.DeviceAuthorizationHostComposed ? "true" : "false")
				   << "\nDEVICE_FLOW_PRESENTED:"
				   << (status.DeviceAuthorizationPublications > 0U ? "true"
																   : "false")
				   << "\nDEVICE_FLOW_STAGE:"
				   << device_authorization_stage_text(
						  status.DeviceAuthorizationStage)
				   << "\nDEVICE_FLOW_REQUESTS:"
				   << static_cast<unsigned long long>(
						  status.DeviceAuthorizationTransportReached)
				   << '\n';
			if (aRun.Device.BrowserProbe) {
				// Three separate facts again. Whether a capability was bound
				// at all, how many addresses reached it, and what the host
				// made of the attempt: the construction cut turns the first
				// false and the third `unavailable` while leaving every line
				// above this untouched, which is what makes it a cut rather
				// than a different run.
				stream << "DEVICE_FLOW_BROWSER_COMPOSED:"
					   << (status.DeviceAuthorizationBrowserComposed ? "true"
																	 : "false")
					   << "\nDEVICE_FLOW_BROWSER_OPENS:"
					   << static_cast<unsigned long long>(
							  status.DeviceAuthorizationBrowserOpens)
					   << "\nDEVICE_FLOW_HANDOFF:"
					   << device_authorization_handoff_text(
							  status.DeviceAuthorizationHandoff)
					   << '\n';
			}
			if (aRun.Device.OrphanCourse) {
				// [[BPR-0540]]. The renewal course's composition with the
				// grant taken out of it: the refresh slot was seeded before
				// the run began, and the session this run has is the one the
				// reviewer pasted. Every counter a spent orphan would move is
				// reported rather than a single verdict, because "the resume
				// did not happen", "no rotation followed one" and "the vault
				// holds what the reviewer pasted" fail for different reasons.
				// The identity is the load-bearing one: a run that resumed
				// would supersede the pasted session with the grant's, and
				// only the digest can tell those apart.
				const auto& orphanStoredDigest
					= status.DeviceAuthorizationVaultStoredDigest;
				const auto storedIsPasted = !orphanStoredDigest.empty()
					&& !pastedDigest.empty()
					&& orphanStoredDigest == pastedDigest;
				const auto orphanIntact
					= status.DeviceAuthorizationRefreshResumes == 0U
					&& status.DeviceAuthorizationRefreshRotations == 0U
					&& status.DeviceAuthorizationVaultStores == 1U
					&& storedIsPasted;
				stream << "DEVICE_FLOW_REFRESH_RESUMES:"
					   << static_cast<unsigned long long>(
							  status.DeviceAuthorizationRefreshResumes)
					   << "\nDEVICE_FLOW_ROTATIONS:"
					   << static_cast<unsigned long long>(
							  status.DeviceAuthorizationRefreshRotations)
					   << "\nDEVICE_FLOW_VAULT_STORES:"
					   << static_cast<unsigned long long>(
							  status.DeviceAuthorizationVaultStores)
					   << "\nDEVICE_FLOW_REFRESH_STORES:"
					   << static_cast<unsigned long long>(refreshStores)
					   << "\nDEVICE_FLOW_REFRESH_RETAINED:"
					   << (refreshRetained ? "true" : "false")
					   << "\nDEVICE_FLOW_VAULT_STORED_TOKEN:"
					   << QByteArray::fromStdString(orphanStoredDigest)
					   << "\nDEVICE_FLOW_PASTED_TOKEN:"
					   << QByteArray::fromStdString(pastedDigest)
					   << "\nDEVICE_FLOW_STORED_IS_PASTED:"
					   << (storedIsPasted ? "true" : "false") << '\n'
					   << (orphanIntact ? "DEVICE_FLOW_ORPHAN_OK\n"
										: "DEVICE_FLOW_ORPHAN_FAILED\n");
			} else if (aRun.Device.RefreshCourse) {
				// [[BPR-0536]]. The registered course's claim, carried one
				// step further: the grant reached the vault, composition armed
				// renewal from the accepted session, and the renewal rotated
				// the material before the access token expired -- through the
				// real host, the real vault authority and a real timer. The
				// vault stores count rises to two because a rotation enrolls a
				// new access token under the same authority, which is the
				// rotation being durable rather than a second enrollment;
				// `DEVICE_FLOW_DURABLE_COPIES` stays zero, because nothing the
				// renewal writes goes anywhere but the vault.
				// [[BPR-0547]]. Two, because the first rotation is not the claim.
				// A rotation that erased the material it had just stored still
				// reports one rotation and `scheduled`; only the renewal that
				// follows has to spend what the first one wrote.
				const auto rotated
					= status.DeviceAuthorizationRefreshRotations >= 2U;
				const auto bound
					= status.DeviceAuthorizationRefreshBindings >= 1U;
				const auto refreshed = status.DeviceAuthorizationHostComposed
					&& status.DeviceAuthorizationStage
						== bpr::core::device_authorization_stage::authorized
					&& bound && rotated
					&& status.DeviceAuthorizationRefreshState
						== bpr::application::
							device_authorization_refresh_state::scheduled
					&& status.DeviceAuthorizationVaultStores >= 3U
					&& status.DeviceAuthorizationDurableCopies == 0U;
				stream << "DEVICE_FLOW_ROTATIONS:"
					   << static_cast<unsigned long long>(
							  status.DeviceAuthorizationRefreshRotations)
					   << "\nDEVICE_FLOW_REFRESH_STATE:"
					   << device_refresh_state_name(
							  status.DeviceAuthorizationRefreshState)
					   << "\nDEVICE_FLOW_VAULT_STORES:"
					   << static_cast<unsigned long long>(
							  status.DeviceAuthorizationVaultStores)
					   << "\nDEVICE_FLOW_DURABLE_COPIES:"
					   << static_cast<unsigned long long>(
							  status.DeviceAuthorizationDurableCopies)
					   << "\nDEVICE_FLOW_REFRESH_BOUND:"
					   << (bound ? "true" : "false")
					   << "\nDEVICE_FLOW_REFRESH_RESUMES:"
					   << static_cast<unsigned long long>(
							  status.DeviceAuthorizationRefreshResumes)
					   // [[BPR-0540]]. What the vault holds after the orderly
					   // shutdown, and how often the slot was written. The
					   // course deleted the access token on the way out and
					   // left the material keyed beside it behind, which no
					   // row could see: the retained flag beside the base
					   // one's is where that shows.
					   << "\nDEVICE_FLOW_REFRESH_RETAINED:"
					   << (refreshRetained ? "true" : "false")
					   << "\nDEVICE_FLOW_REFRESH_STORES:"
					   << static_cast<unsigned long long>(refreshStores)
					   // [[BPR-0547]]. Which erasure the retained flag above
					   // is reporting. This course rotates, and a rotation
					   // ends by enrolling the rotated access token, which
					   // supersedes the session the grant established: a
					   // build that carries the slot's erasure along with
					   // that supersession erases the material its own
					   // rotation stored a moment earlier, and the shutdown's
					   // erasure afterwards leaves the same retained flag
					   // behind. One erasure is the shutdown's; two is the
					   // renewal chain destroying itself.
					   << "\nDEVICE_FLOW_REFRESH_DELETES:"
					   << static_cast<unsigned long long>(refreshDeletes)
					   // [[BPR-0547]]. Which rotation's material the slot last held.
					   // The fixture's renewal answers carry the renewal's number in
					   // them, so this digest names the rotation that wrote the slot
					   // rather than merely counting that something did; read beside
					   // the single erasure above, it is the second rotation's
					   // material being durable until the orderly shutdown took it.
					   << "\nDEVICE_FLOW_REFRESH_STORED_TOKEN:"
					   << QByteArray::fromStdString(refreshDigest)
					   // [[BPR-0551]]. Whether the orderly shutdown's erasure did what
					   // it was asked. The host carries the answer and
					   // `PUBLIC_PATH_VERDICT` prints it, but only on a run that had
					   // already failed for some other reason, so no row could fail on
					   // it: a delete the refresh erasure displaced left the access
					   // token in the vault and both device rows still green. Printed
					   // here unconditionally, and pinned, it is a marker a red run
					   // cannot omit.
					   << "\nDEVICE_FLOW_CLEANUP_FAILED:"
					   << (status.ShutdownCleanupFailed ? "true" : "false")
					   << '\n'
					   << (refreshed ? "DEVICE_FLOW_REFRESH_OK\n"
									 : "DEVICE_FLOW_REFRESH_FAILED\n")
					   // [[BPR-0902]]. How often the refresh slot was read: by
					   // each renewal, and by the session's own presence probe.
					   // The counts above are all short by one whether the
					   // second renewal was never armed, gave up before it
					   // read the slot, or read it and was refused; this count
					   // tells those apart. Measured on Windows, a run whose
					   // two rotations both complete reads it four times, and
					   // one whose second renewal is refused before reading
					   // reads it twice. It follows the verdict, so no row
					   // pins it.
					   << "DEVICE_FLOW_REFRESH_RETRIEVES:"
					   << static_cast<unsigned long long>(refreshRetrieves)
					   << '\n';
			} else if (aRun.Device.Registered) {
				// The registered course makes the opposite claim: the grant
				// must reach the existing vault exactly once, and the token
				// must exist nowhere else the application wrote.
				//
				// [[BPR-0539]]. Which is a claim about identity and not only
				// about arithmetic: one store counted the fixture's own
				// connect credential just as readily as the grant, and did,
				// until the course stopped connecting first. So the identity
				// of the stored material is reported as its own fact and the
				// verdict below depends on it. Printed ahead of the counts
				// rather than beside the verdict because the browser and
				// panel cuts read this block through the same stream: the
				// panel rows begin reading at the enrollment count, so a line
				// above it disturbs nothing they pin, while the browser rows
				// read the handoff and that count as adjacent -- which is why
				// a browser probe reports the counts alone, exactly as it did.
				const auto identityReported = !aRun.Device.BrowserProbe;
				const auto storedIsGrant
					= status.DeviceAuthorizationVaultStoredIsGrant;
				const auto enrolled = status.DeviceAuthorizationHostComposed
					&& status.DeviceAuthorizationStage
						== bpr::core::device_authorization_stage::authorized
					&& status.DeviceAuthorizationEnrollments == 1U
					&& status.DeviceAuthorizationVaultStores == 1U
					&& status.DeviceAuthorizationDurableCopies == 0U
					&& (!identityReported || storedIsGrant);
				if (identityReported) {
					stream << "DEVICE_FLOW_VAULT_STORED_TOKEN:"
						   << QByteArray::fromStdString(
								  status.DeviceAuthorizationVaultStoredDigest)
						   << "\nDEVICE_FLOW_STORED_IS_GRANT:"
						   << (storedIsGrant ? "true" : "false")
						   // [[BPR-0540]]. Reported inside the identity block
						   // rather than beside the counts, because the browser
						   // and panel rows read those counts as adjacent
						   // lines; this course composes no refresh slot at
						   // all, so what it states is that the shutdown left
						   // nothing keyed beside the credential it deleted.
						   << "\nDEVICE_FLOW_REFRESH_RETAINED:"
						   << (refreshRetained ? "true" : "false")
						   // [[BPR-0551]]. The same always-emitted cleanup verdict the
						   // renewal course prints, inside the identity block for the
						   // reason the retained flag is: the browser and panel rows
						   // read the counts below as adjacent lines.
						   << "\nDEVICE_FLOW_CLEANUP_FAILED:"
						   << (status.ShutdownCleanupFailed ? "true" : "false")
						   << '\n';
				}
				stream << "DEVICE_FLOW_ENROLLMENTS:"
					   << static_cast<unsigned long long>(
							  status.DeviceAuthorizationEnrollments)
					   << "\nDEVICE_FLOW_VAULT_STORES:"
					   << static_cast<unsigned long long>(
							  status.DeviceAuthorizationVaultStores)
					   << "\nDEVICE_FLOW_DURABLE_COPIES:"
					   << static_cast<unsigned long long>(
							  status.DeviceAuthorizationDurableCopies)
					   << "\nDEVICE_FLOW_TRANSPORT_SUBMISSIONS:"
					   << static_cast<unsigned long long>(
							  status.DeviceAuthorizationTransportSubmissions)
					   << '\n'
					   << (enrolled ? "DEVICE_FLOW_ENROLLED_OK\n"
									: "DEVICE_FLOW_ENROLLED_FAILED\n");
			} else if (aRun.Device.GhesCourse) {
				// [[BPR-0535]]. The enterprise negative, and the one course
				// whose claim is about which profile asked. The picker named a
				// GHES profile, the mounted panel was bound to it, and its
				// "Get a code" was pressed: the composed capability belongs to
				// GitHub.com, so the answer must be the typed refusal with
				// nothing built. Reported as two facts, because "the reviewer
				// pressed it and was refused" and "every other condition of
				// the refusal held" fail for different reasons.
				const auto ghesBeginRefused = aRun.Device.GhesBegan
					&& status.DeviceAuthorizationStage
						== bpr::core::device_authorization_stage::unavailable;
				const auto ghesRefusalPassed = ghesBeginRefused
					&& status.DeviceAuthorizationHostComposed
					&& status.DeviceAuthorizationPanelComposed
					&& status.DeviceAuthorizationPublications > 0U
					&& status.DeviceAuthorizationTransportReached == 0U;
				stream << "DEVICE_FLOW_GHES_BEGIN_REFUSED:"
					   << (ghesBeginRefused ? "true" : "false") << '\n'
					   << (ghesRefusalPassed
							   ? "DEVICE_FLOW_GHES_REFUSAL_OK\n"
							   : "DEVICE_FLOW_GHES_REFUSAL_FAILED\n");
			} else if (status.DeviceAuthorizationCapabilityRegistered) {
				// The registered claim is the refusal's mirror: the flow left
				// `unavailable` behind and the refusing transport was never
				// touched. Which terminal it reached depends on the
				// environment — cancelled with a network, failed without —
				// and the selector asserts the exact stage printed beside
				// this.
				const auto deviceStarted
					= status.DeviceAuthorizationHostComposed
					&& status.DeviceAuthorizationPublications > 0U
					&& status.DeviceAuthorizationStage
						!= bpr::core::device_authorization_stage::unavailable
					&& status.DeviceAuthorizationTransportReached == 0U;
				stream << (deviceStarted
							   ? "DEVICE_FLOW_REGISTERED_SELECTION_OK\n"
							   : "DEVICE_FLOW_REGISTERED_SELECTION_FAILED\n");
			} else {
				const auto deviceRefused
					= status.DeviceAuthorizationHostComposed
					&& status.DeviceAuthorizationPublications > 0U
					&& status.DeviceAuthorizationStage
						== bpr::core::device_authorization_stage::unavailable
					&& status.DeviceAuthorizationTransportReached == 0U;
				stream << (deviceRefused ? "DEVICE_FLOW_UNAVAILABLE_OK\n"
										 : "DEVICE_FLOW_UNAVAILABLE_FAILED\n");
			}
			// Last, and deliberately outside every line above it. [[BPR-0526]]
			// added a second reader of the same publications, and the point of
			// the marker is that adding or cutting it changes nothing the
			// existing contracts read — so it is printed after all of them
			// rather than between any two.
			if (aRun.Device.PanelCourse) {
				stream << "DEVICE_FLOW_PANEL_BEGAN:"
					   << (aRun.Device.PanelBegan ? "true" : "false") << '\n';
			}
			stream << "DEVICE_FLOW_PANEL_COMPOSED:"
				   << (status.DeviceAuthorizationPanelComposed ? "true"
															   : "false")
				   << '\n';
		}
		if (aRun.PublicPreview.Enabled) {
			// Separate facts again, and for the same reason: "the authority
			// was composed", "which answer the activation produced", and
			// "nothing was sent, no header was built, no vault was touched"
			// fail independently, and one verdict would hide which.
			// The mode lives on the accepted session, and a run reports its
			// markers after shutdown has closed that session. So the fact is
			// taken from what the run observed while the session was live --
			// `Opened` is set only once the status said signed out -- with the
			// live answer still leading, for the rows that never open one.
			const auto signedOut = status.PublicPreviewSignedOut
				|| aRun.PublicPreview.Opened
				|| aRun.PublicPreview.SignedOutObserved;
			const auto stageText = signedOut
				? "enabled"
				: (status.PublicPreviewPolicyRefused ? "refused"
													 : "unavailable");
			stream << "PUBLIC_PREVIEW_COMPOSED:"
				   << (status.PublicPreviewComposed ? "true" : "false")
				   << "\nPUBLIC_PREVIEW_STAGE:" << stageText
				   << "\nPUBLIC_PREVIEW_REQUESTS:"
				   << static_cast<unsigned long long>(
						  status.PublicPreviewRequests)
				   << "\nPUBLIC_PREVIEW_HEADER_EFFECTS:"
				   << static_cast<unsigned long long>(
						  status.PublicPreviewHeaderEffects)
				   << "\nPUBLIC_PREVIEW_VAULT_EFFECTS:"
				   << static_cast<unsigned long long>(
						  status.PublicPreviewVaultEffects)
				   << "\nPUBLIC_PREVIEW_SIGN_IN_REQUESTS:"
				   << static_cast<unsigned long long>(
						  status.PublicPreviewSignInRequests)
				   << '\n';
#if defined(BPR_BUILD_TESTING)
			if (aRun.PublicPreview.LiveCandidate) {
				// [[BPR-0316]]. The live candidate reports what the shipped
				// composition did, in the same shape the fixture course
				// reports it, plus the four facts only a real read can carry:
				// what reached the wire, what the shared budget said, which
				// mode the accepted snapshot was read in, and which head the
				// candidate resolved to. No body and no comment text: this
				// output goes on a card.
				stream << "PUBLIC_PREVIEW_REVIEW_COMMENT_GROUPS:"
					   << static_cast<unsigned long long>(
							  status.PublicPreviewReviewCommentGroups)
					   << "\nPUBLIC_PREVIEW_WRITE_REFUSALS:"
					   << static_cast<unsigned long long>(
							  status.PublicPreviewWriteRefusals)
					   << "\nPUBLIC_PREVIEW_LIVE_CANDIDATE_PRODUCTION_FACTORIES:"
					   << (aRun.PublicPreview.ProductionFactories ? "true"
																  : "false")
					   << '\n';
				// [[BPR-0316]]. What the count above was spent on: one line
				// per request the signed-out transport forwarded, in the order
				// it forwarded them, with the operation's contract name, the
				// page, how it ended and how long it took. Read from the
				// transport now rather than from the tick snapshot, so a run
				// whose budget expired mid-read still reports every request it
				// had already made -- an expired run is the case this exists
				// for. Never a body, never a URL, and no header beyond the
				// rate-limit triple a failure itself carried.
				const auto wireOperations
					= aApplication.public_preview_wire_operations();
				for (const auto& operation : wireOperations) {
					stream << "PUBLIC_PREVIEW_WIRE_OPERATION:"
						   << QString::fromStdString(operation.Operation) << ':'
						   << static_cast<unsigned long long>(operation.Page)
						   << ':' << QString::fromStdString(operation.Outcome)
						   << ':'
						   << static_cast<unsigned long long>(
								  operation.ElapsedMilliseconds)
						   << '\n';
				}
				// Costs nothing outside the process and says whether the
				// mechanism that bounds this read to one attempt is in place,
				// so the isolated dry run proves it and the authorized read
				// carries the same proof beside its own numbers.
				const auto singleAttempt
					= bpr::app::public_preview_single_attempt_self_check();
				stream << "PUBLIC_PREVIEW_WIRE_REQUESTS:"
					   << static_cast<unsigned long long>(
							  aRun.PublicPreview.WireRequests)
					   << "\nPUBLIC_PREVIEW_WIRE_CAPABILITY_PROBES:"
					   << static_cast<unsigned long long>(
							  aRun.PublicPreview.WireCapabilityProbes)
					   << "\nPUBLIC_PREVIEW_WIRE_CREDENTIALED_REFUSALS:"
					   << static_cast<unsigned long long>(
							  aRun.PublicPreview.WireCredentialedRefusals)
					   << "\nPUBLIC_PREVIEW_WIRE_SINGLE_ATTEMPT:"
					   << (singleAttempt ? "true" : "false")
					   << "\nPUBLIC_PREVIEW_BUDGET:"
					   << static_cast<unsigned long long>(
							  aRun.PublicPreview.BudgetLimit)
					   << '/'
					   << static_cast<unsigned long long>(
							  aRun.PublicPreview.BudgetRemaining)
					   << '/'
					   << static_cast<unsigned long long>(
							  aRun.PublicPreview.BudgetReset)
					   << "\nPUBLIC_PREVIEW_SNAPSHOT_MODE:"
					   << (aRun.PublicPreview.SnapshotSignedOut
							   ? "public_preview"
							   : "authenticated")
					   << "\nPUBLIC_PREVIEW_CANDIDATE:"
					   << QString::fromStdString(aRun.Expectations.Owner) << '/'
					   << QString::fromStdString(aRun.Expectations.Repository)
					   << '#' << aRun.Expectations.Number << ' '
					   << QString::fromStdString(aRun.PublicPreview.HeadOid)
					   // How long the whole course took. The run bound itself
					   // is the caller's business -- `--automated-run-budget-ms`
					   // stays required -- and this is what it was spent on.
					   << "\nPUBLIC_PREVIEW_LIVE_ELAPSED_MS:"
					   << static_cast<long long>(
							  std::chrono::duration_cast<
								  std::chrono::milliseconds>(
								  automated_clock::now() - aRun.Started)
								  .count())
					   << '\n';
				// The wiring claim, which a dry run makes on its own: the
				// signed-out authority was composed from the production
				// factories, no credentialed transport was even asked for, and
				// neither a header nor the vault was involved. The read claim
				// is added only when a read was asked for.
				auto livePassed = status.PublicPreviewComposed
					&& aRun.PublicPreview.ProductionFactories && singleAttempt
					&& !status.PublicPreviewUnavailable
					&& !status.PublicPreviewPolicyRefused
					&& aRun.PublicPreview.WireCredentialedRefusals == 0U
					&& aRun.PublicPreview.WireCapabilityProbes == 0U
					&& status.PublicPreviewHeaderEffects == 0U
					&& status.PublicPreviewVaultEffects == 0U
					&& status.PublicPreviewSignInRequests == 0U
					&& status.PublicPreviewSmartAcquisitions == 0U
					&& status.PublicPreviewPublications > 0U;
				if (aRun.PublicPreview.DryRun) {
					// The wiring claim on its own: the mode was entered over
					// the production authority and the run stopped there, with
					// nothing on the wire and no snapshot accepted.
					livePassed = livePassed && signedOut
						&& status.PublicPreviewRequests == 0U
						&& aRun.PublicPreview.WireRequests == 0U
						&& !aRun.PublicPreview.SnapshotSignedOut
						&& aRun.PublicPreview.HeadOid.empty();
				} else {
					livePassed = livePassed && signedOut
						&& status.PublicPreviewRequests >= 1U
						&& aRun.PublicPreview.WireRequests >= 1U
						&& aRun.PublicPreview.SnapshotSignedOut
						&& aRun.PublicPreview.BudgetObserved
						&& !aRun.PublicPreview.HeadOid.empty();
				}
				stream << (livePassed ? "PUBLIC_PREVIEW_LIVE_CANDIDATE_OK\n"
									  : "PUBLIC_PREVIEW_LIVE_CANDIDATE_FAILED\n");
			} else
#endif
				if (aRun.PublicPreview.Course) {
				// The enabled claim: the mode was entered, the read went
				// through the credential-free authority, the inline
				// conversation was observed, and neither an authorization
				// header nor the vault was involved in any of it.
				// [[BPR-0715]]. The read is not enough: the signed-out open
				// must reach an exact comparison, over the credential-free
				// smart-Git lease, before the mode counts as working.
				const auto enabled = status.PublicPreviewComposed && signedOut
					&& !status.PublicPreviewUnavailable
					&& status.PublicPreviewRequests >= 1U
					&& status.PublicPreviewHeaderEffects == 0U
					&& status.PublicPreviewVaultEffects == 0U
					&& status.PublicPreviewReviewCommentsObserved
					&& status.PublicPreviewWriteRefusals == 2U
					&& status.PublicPreviewPublications > 0U
					&& aRun.PublicPreview.SettledEpoch == bpr::core::epoch_status::ready
					&& aRun.PublicPreview.SettledOccurrenceCount >= (aRun.PublicPreview.EmptyTimeline ? 1U : 3U)
					&& aRun.PublicPreview.SettledHistoryIncomplete
					&& (!aRun.PublicPreview.awaits_historical_retention()
						|| aRun.PublicPreview.HistoricalRetained);
				stream << "PUBLIC_PREVIEW_REVIEW_COMMENT_GROUPS:"
					   << static_cast<unsigned long long>(
							  status.PublicPreviewReviewCommentGroups)
					   << "\nPUBLIC_PREVIEW_WRITE_REFUSALS:"
					   << static_cast<unsigned long long>(
							  status.PublicPreviewWriteRefusals)
					   << "\nPUBLIC_PREVIEW_EPOCH_STATUS:"
					   << static_cast<unsigned>(aRun.PublicPreview.SettledEpoch)
					   << "\nPUBLIC_PREVIEW_OCCURRENCES:"
					   << static_cast<unsigned long long>(aRun.PublicPreview.SettledOccurrenceCount)
					   << "\nPUBLIC_PREVIEW_HISTORY_INCOMPLETE:"
					   << (aRun.PublicPreview.SettledHistoryIncomplete ? "true" : "false")
					   << "\nPUBLIC_PREVIEW_EPOCH_LINE_HISTORY:"
					   << aRun.PublicPreview.SettledHistoryText
					   << "\nPUBLIC_PREVIEW_SMART_ACQUISITIONS:"
					   << static_cast<unsigned long long>(
							  status.PublicPreviewSmartAcquisitions)
					   << '\n'
					   << (enabled ? "PUBLIC_PREVIEW_ENABLED_OK\n"
								   : "PUBLIC_PREVIEW_ENABLED_FAILED\n");
				// [[BPR-0829]]. Named when the run ended still waiting, so an expired
				// budget reads as the retention it waited on.
				if (aRun.PublicPreview.awaits_historical_retention()
					&& !aRun.PublicPreview.HistoricalRetained) {
					stream << "PUBLIC_PREVIEW_WAITING_FOR:retained force-push occurrences\n";
				}
				if (aRun.PublicPreview.MissingHistoricalBase) {
					stream << (aRun.PublicPreview.MissingHistoricalBaseShown ? "PUBLIC_PREVIEW_MISSING_BASE_OK\n" : "PUBLIC_PREVIEW_MISSING_BASE_FAILED\n");
				}
				if (aRun.PublicPreview.RewrittenBase) {
					stream << "PUBLIC_PREVIEW_REWRITTEN_BASE_GATE:" << aRun.PublicPreview.RewrittenBaseGate << '\n'
						   << (aRun.PublicPreview.RewrittenBaseShown && aRun.PublicPreview.RewrittenBaseSelectionDenied
								   ? "PUBLIC_PREVIEW_REWRITTEN_BASE_UNAVAILABLE_OK\n"
								   : "PUBLIC_PREVIEW_REWRITTEN_BASE_UNAVAILABLE_FAILED\n");
				}
				// [[BPR-1340]]. After the enabled verdict, whose rows the public
				// path matches in one run of lines.
				stream << "PUBLIC_PREVIEW_EPOCH_LINE_LABELS:" << aRun.PublicPreview.SettledLineLabels.join(QStringLiteral(" | ")) << '\n';
				if (aRun.PublicPreview.DragCourse) {
					stream << "PUBLIC_PREVIEW_HANDLE_DRAG_GATE:" << aRun.PublicPreview.DragGate << '\n'
						   << "PUBLIC_PREVIEW_HANDLE_DRAG_STEPS:" << aRun.PublicPreview.DragSteps.join(QStringLiteral(" | ")) << '\n'
						   << "PUBLIC_PREVIEW_HANDLE_DRAG_MOVED:left=" << (aRun.PublicPreview.DragLeftMoved ? 1 : 0)
						   << ":right=" << (aRun.PublicPreview.DragRightMoved ? 1 : 0)
						   << ":key=" << (aRun.PublicPreview.DragKeyMoved ? 1 : 0) << '\n'
						   << (aRun.PublicPreview.DragRightMoved && aRun.PublicPreview.DragLeftMoved && aRun.PublicPreview.DragKeyMoved
								   ? "PUBLIC_PREVIEW_HANDLE_DRAG_OK\n"
								   : "PUBLIC_PREVIEW_HANDLE_DRAG_FAILED\n");
				}
				if (aRun.PublicPreview.HistoricalCourse) {
					stream << (aRun.PublicPreview.HistoricalHandleMoved
								   ? "PUBLIC_PREVIEW_HISTORICAL_HANDLE_OK\n"
								   : "PUBLIC_PREVIEW_HISTORICAL_HANDLE_FAILED\n");
				}
			} else if (aRun.PublicPreview.Enterprise && status.PublicPreviewComposed) {
				// The enterprise claim is the composition refusal's mirror:
				// the authority was composed and the core policy still refused
				// the mode, before a request was built.
				const auto refused = status.PublicPreviewComposed
					&& status.PublicPreviewPolicyRefused && !signedOut
					&& status.PublicPreviewRequests == 0U
					&& status.PublicPreviewHeaderEffects == 0U
					&& status.PublicPreviewVaultEffects == 0U
					&& status.PublicPreviewPublications > 0U;
				stream << (refused ? "PUBLIC_PREVIEW_REFUSED_OK\n"
								   : "PUBLIC_PREVIEW_REFUSED_FAILED\n");
			} else if (status.PublicPreviewComposed) {
				// [[BPR-0316]]. A selection that is enabled activates with no
				// request at all: the mode is entered from the composed
				// authority and nothing is read until a reviewer asks for
				// something. So the enabled verdict for a plain probe is
				// exactly the opposite of the fixture course's -- zero
				// requests rather than at least one -- and a build whose
				// selection ships enabled reports it without a course flag.
				const auto enabled = signedOut
					&& !status.PublicPreviewUnavailable
					&& !status.PublicPreviewPolicyRefused
					&& status.PublicPreviewRequests == 0U
					&& status.PublicPreviewHeaderEffects == 0U
					&& status.PublicPreviewVaultEffects == 0U
					&& status.PublicPreviewSmartAcquisitions == 0U
					&& status.PublicPreviewPublications > 0U;
				stream << (enabled ? "PUBLIC_PREVIEW_ENABLED_OK\n"
								   : "PUBLIC_PREVIEW_ENABLED_FAILED\n");
			} else {
				// The shipped selection where it is unavailable. Composed
				// nothing, answered `public_preview_unavailable`, said so
				// where a reviewer can see it, and sent nothing of any kind.
				const auto unavailable = !status.PublicPreviewComposed
					&& status.PublicPreviewUnavailable && !signedOut
					&& status.PublicPreviewRequests == 0U
					&& status.PublicPreviewHeaderEffects == 0U
					&& status.PublicPreviewVaultEffects == 0U
					&& status.PublicPreviewSmartAcquisitions == 0U
					&& status.PublicPreviewPublications > 0U;
				stream << (unavailable ? "PUBLIC_PREVIEW_UNAVAILABLE_OK\n"
									   : "PUBLIC_PREVIEW_UNAVAILABLE_FAILED\n");
			}
		}
#if defined(BPR_BUILD_TESTING)
		if (aRun.Fixture && !aRun.ReviewLedgerCompositionProbe.has_value()
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
				   << (status.EpochHandoffDeliveryFailed ? 1 : 0)
				   // [[BPR-0647]]. Last, because the native appearance contract
				   // matches the first three facts as one literal.
				   << " overview=" << (aRun.OverviewShown && aRun.OverviewReturned ? 1 : 0) << '\n'
				   // [[BPR-0875]]. Which cleanup refused, printed after the
				   // verdict so no existing expectation moves, and only when one
				   // did: `<source>:<path-or-window>:<error>`.
				   << QString::fromStdString(
						  status.ShutdownCleanupFailed
							  ? "PUBLIC_PATH_CLEANUP_SOURCE:"
								  + (aApplication.shutdown_cleanup_detail().empty()
										 ? std::string{ "unnamed" }
										 : aApplication.shutdown_cleanup_detail())
								  + "\n"
							  : std::string{ })
				   // Which of the four narrowing groups rejected the run. The
				   // first `0` names the group to look in.
				   << "PUBLIC_PATH_GROUPS:"
				   << "core=" << (passedCoreFacts ? 1 : 0)
				   << " fixture=" << (passedFixture ? 1 : 0)
				   << " maintenance=" << (passedMaintenance ? 1 : 0)
				   << " render=" << (renderSaved ? 1 : 0) << '\n'
				   << "PUBLIC_PATH_STATUS:"
				   << QString::fromStdString(window.github_status_text()) << '\n'
				   << "PUBLIC_PATH_DIAGNOSTIC:"
				   << QString::fromStdString(window.github_diagnostic_text()) << '\n'
				   << "PUBLIC_PATH_ACCOUNT:"
				   << QString::fromStdString(window.active_account_text()) << '\n'
				   << "PUBLIC_PATH_CONVERSATIONS:roots="
				   << aRun.LastConversationRows << ":identities=";
			for (const auto& identity : aRun.LastConversationIdentities) {
				stream << QString::fromStdString(identity) << ',';
			}
			stream << ":file=" << aRun.LastConversationFileAnchored << ':'
				   << aRun.LastConversationFileMatchesFixture << ":anchor=";
			if (aRun.LastConversationAnchor.has_value()) {
				stream << (aRun.LastConversationAnchor->Side
								   == bpr::core::diff_side::left
							   ? "left"
							   : "right")
					   << ':' << aRun.LastConversationAnchor->Line;
			} else {
				stream << "none";
			}
			stream << '\n'
				   << "PUBLIC_PATH_EPOCH_STATUS:"
				   << (aRun.LastEpochStatus.isEmpty()
						   ? window.epoch_selectors()->status_text()
						   : aRun.LastEpochStatus)
				   << '\n'
				   << "PUBLIC_PATH_EPOCH_COMPARISON:"
				   << (aRun.LastEpochComparison.isEmpty()
						   ? window.epoch_selectors()->comparison_text()
						   : aRun.LastEpochComparison)
				   << '\n'
				   << "PUBLIC_PATH_EPOCH_PAIR:"
				   << (aRun.LastEpochLeft.isEmpty()
						   ? window.epoch_selectors()->left_oid_editor()->text()
						   : aRun.LastEpochLeft)
				   << ':'
				   << (aRun.LastEpochRight.isEmpty()
						   ? window.epoch_selectors()->right_oid_editor()->text()
						   : aRun.LastEpochRight)
				   << ':'
				   << (aRun.LastEpochLeftRow >= 0
						   ? aRun.LastEpochLeftRow
						   : window.epoch_selectors()->left_selector()->currentIndex())
				   << ':'
				   << (aRun.LastEpochRightRow >= 0
						   ? aRun.LastEpochRightRow
						   : window.epoch_selectors()->right_selector()->currentIndex())
				   << ':'
				   << static_cast<unsigned long long>(
						  aRun.LastEpochCount != 0U
							  ? aRun.LastEpochCount
							  : window.epoch_selectors()
									->history_model()
									->epoch_count())
				   << ':'
				   << (aRun.LastEpochHistoryEnabled
							   || window.epoch_selectors()
									  ->load_history_action()
									  ->isEnabled()
						   ? 1
						   : 0)
				   << '\n'
				   << "PUBLIC_PATH_EPOCH_CORE_STATUS:"
				   << aRun.LastEpochCoreStatus << '\n';
#if defined(BPR_BUILD_TESTING)
			if (aRun.FailedStage
				== static_cast<int>(automated_run::stage::epoch_refresh)) {
				const auto* const title = window.findChild<QLabel*>(
					QStringLiteral("pullRequestTitle"));
				const auto identities = fixture_conversation_identities(
					*window.conversation_model());
				const auto& expectedIdentities
					= aRun.LastEpochRefreshFocused
					? aRun.Expectations.ExpectedFocusedConversationIdentities
					: aRun.Expectations.ExpectedConversationIdentities;
				const auto expectedThreadValid
					= !aRun.Expectations.ExpectedThreadId.has_value()
					|| window.conversation_model()
						   ->index_for_id(*aRun.Expectations.ExpectedThreadId)
						   .isValid();
				const auto causeRow = fixture_anchor_row(
					*window.epoch_selectors()->history_model(),
					2U,
					bpr::core::comparison_anchor_kind::epoch_head);
				const auto causeActual = causeRow >= 0
					? window.epoch_selectors()
						  ->history_model()
						  ->index(causeRow, 0)
						  .data(bpr::qt::epoch_history_model::primary_cause_role)
						  .toInt()
					: -1;
				const auto report      = [&stream](
											 const std::string_view aTerm,
											 const auto&            aExpected,
											 const auto&            aActual) {
					stream << "EPOCH_REFRESH_PRESENTATION_TERM:"
						   << QByteArray{ aTerm.data(), static_cast<qsizetype>(aTerm.size()) }
						   << ":expected=" << aExpected << ":actual=" << aActual << '\n';
				};
				report(
					"status",
					QStringLiteral("Pull request ready."),
					fixture_diagnostic_text(QString::fromStdString(
						window.github_status_text())));
				report(
					"account",
					QString::fromStdString(aRun.Expectations.ExpectedAccountText),
					fixture_diagnostic_text(QString::fromStdString(
						window.active_account_text())));
				report(
					"host",
					QString::fromStdString(aRun.ExpectedActiveHostText),
					fixture_diagnostic_text(QString::fromStdString(
						window.active_host_text())));
				report("open-action", true, window.github_open_action()->isEnabled());
				report(
					"refresh-action",
					true,
					window.github_refresh_action()->isEnabled());
				report("title-present", true, title != nullptr);
				report(
					"title-text",
					QString::fromStdString(aRun.Expectations.ExpectedTitle),
					fixture_diagnostic_text(
						title != nullptr ? title->text() : QString{ }));
				report(
					"conversation-roots",
					aRun.LastEpochRefreshFocused ? aRun.Expectations.ExpectedFocusedConversationRows : aRun.Expectations.ExpectedConversationRows,
					window.conversation_model()->rowCount());
				report(
					"expected-thread-valid",
					true,
					expectedThreadValid);
				report(
					"github-full-resets",
					0U,
					window.statistics().GithubFullResets);
				report(
					"epoch-cause",
					static_cast<int>(
						bpr::core::epoch_primary_cause::force_push_rebase),
					causeActual);
				stream << "EPOCH_REFRESH_IDENTITY:" << aRun.LastEpochRefreshIdentity << '\n';
				stream << "EPOCH_REFRESH_PRESENTATION_IDENTITIES:expected=";
				for (const auto& identity : expectedIdentities) {
					stream << QString::fromStdString(identity) << ',';
				}
				stream << ":actual=";
				for (const auto& identity : identities) {
					stream << QString::fromStdString(identity) << ',';
				}
				stream << ":passed=" << (identities == expectedIdentities)
					   << ":cause_row=" << causeRow << '\n';
			}
			if (aRun.MaintenanceProbe
				!= automated_run::epoch_maintenance_probe::none) {
				const auto maintenance = bpr::app::detail::
					github_application_fixture_access::maintenance_evidence(
						aApplication, aRun.MaintenanceTarget);
				// What the probe was still waiting for. Without it a stalled
				// maintenance run reported only `PUBLIC_PATH_STAGE:14`, which
				// names the stage every probe waits in and none of the ten
				// conditions it waits on. BPR-0447.
				stream << "PUBLIC_PATH_MAINTENANCE_GATE:"
					   << (aRun.MaintenanceGate != nullptr ? aRun.MaintenanceGate
														   : "none")
					   << '\n';
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
		if (fixtureCourseRan) {
			stream << (aRun.ReviewLedgerCompositionProbe.has_value()
						   ? fixtureFactsPassed
							   ? "GITHUB_FIXTURE_REVIEW_LEDGER_OK:"
							   : "GITHUB_FIXTURE_REVIEW_LEDGER_FAILED:"
						   : fixtureFactsPassed ? "GITHUB_FIXTURE_OK:"
												: "GITHUB_FIXTURE_FAILED:")
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
			stream << (aRun.ConversationAnchorEvidencePassed
						   ? "CONVERSATION_ANCHOR_EVIDENCE_OK\n"
						   : "CONVERSATION_ANCHOR_EVIDENCE_FAILED\n");
			stream << (aRun.SyntaxThemeRepaintPassed
						   ? "SYNTAX_THEME_REPAINT_OK\n"
						   : "SYNTAX_THEME_REPAINT_FAILED\n");
			if (aRun.ThemeCustomizationPath.has_value()) {
				stream << (aRun.ThemeCustomizationPassed
							   ? "APPEARANCE_THEME_CUSTOMIZATION_OK\n"
							   : "APPEARANCE_THEME_CUSTOMIZATION_FAILED\n");
			}
		}
		if (aRun.DiffShutdownProbe) {
			stream << (passed ? "DIFF_WORKBENCH_SHUTDOWN_OK\n"
							  : "DIFF_WORKBENCH_SHUTDOWN_FAILED\n");
		}
		if (aRun.WorkbenchCompositionProbe) {
			const auto& evidence = aRun.WorkbenchCompositionEvidence;
			stream
				<< (evidence.ExactAuthority
						? "WORKBENCH_AUTHORITY_WIRE_OK\n"
						: "WORKBENCH_AUTHORITY_WIRE_FAILED\n")
				<< (evidence.BoundedObservation
						? "WORKBENCH_BOUNDED_OBSERVATION_OK\n"
						: "WORKBENCH_BOUNDED_OBSERVATION_FAILED\n")
				<< (evidence.IntentDispatch
						? "WORKBENCH_INTENT_WIRE_OK\n"
						: "WORKBENCH_INTENT_WIRE_FAILED\n")
				<< (evidence.ProjectionDelivery
						? "WORKBENCH_PROJECTION_WIRE_OK\n"
						: "WORKBENCH_PROJECTION_WIRE_FAILED\n")
				<< (evidence.FileGesture
						? "WORKBENCH_FILE_GESTURE_OK\n"
						: "WORKBENCH_FILE_GESTURE_FAILED\n")
				<< (evidence.SelectedDiffRequest
						? "WORKBENCH_SELECTED_DIFF_REQUEST_OK\n"
						: "WORKBENCH_SELECTED_DIFF_REQUEST_FAILED\n")
				<< (evidence.ExactComparison
						? "WORKBENCH_EXACT_COMPARISON_OK\n"
						: "WORKBENCH_EXACT_COMPARISON_FAILED\n")
				<< (evidence.ChangeDispatch
						? "WORKBENCH_CHANGE_WIRE_OK\n"
						: "WORKBENCH_CHANGE_WIRE_FAILED\n")
				<< (evidence.DrawerWidthDispatch
						? "WORKBENCH_DRAWER_WIDTH_WIRE_OK\n"
						: "WORKBENCH_DRAWER_WIDTH_WIRE_FAILED\n")
				<< (evidence.SessionSettingsFallback
						? "WORKBENCH_SESSION_FALLBACK_OK\n"
						: "WORKBENCH_SESSION_FALLBACK_FAILED\n")
				<< (evidence.MismatchPreserved
						? "WORKBENCH_IDENTITY_MISMATCH_OK\n"
						: "WORKBENCH_IDENTITY_MISMATCH_FAILED\n")
				<< (evidence.InvalidationSuperseded
						? "WORKBENCH_INVALIDATION_WIRE_OK\n"
						: "WORKBENCH_INVALIDATION_WIRE_FAILED\n")
				<< (evidence.FailureDelivery
						? "WORKBENCH_FAILURE_WIRE_OK\n"
						: "WORKBENCH_FAILURE_WIRE_FAILED\n")
				<< (evidence.FixtureDiffHelperUnused
						? "WORKBENCH_FIXTURE_DIFF_HELPER_UNUSED_OK\n"
						: "WORKBENCH_FIXTURE_DIFF_HELPER_UNUSED_FAILED\n")
				<< (aRun.WorkbenchCompositionPassed
						? "WORKBENCH_COMPOSITION_TRACE_OK\n"
						: "WORKBENCH_COMPOSITION_TRACE_FAILED\n");
		}
		if (aRun.UIforEtwSelectedDiffProbe) {
			const auto& evidence = aRun.UIforEtwSelectedDiffEvidence;
			stream
				<< (evidence.ExactCommitPair
						? "UIFORETW_EXACT_COMMIT_PAIR_OK\n"
						: "UIFORETW_EXACT_COMMIT_PAIR_FAILED\n")
				<< (evidence.ResourceGesture
						? "UIFORETW_RESOURCE_GESTURE_OK\n"
						: "UIFORETW_RESOURCE_GESTURE_FAILED\n")
				<< (evidence.ResourceExactBlobs
						? "UIFORETW_RESOURCE_BLOBS_OK\n"
						: "UIFORETW_RESOURCE_BLOBS_FAILED\n")
				<< (evidence.ResourceIntralinePair
						? "UIFORETW_RESOURCE_INTRALINE_OK\n"
						: "UIFORETW_RESOURCE_INTRALINE_FAILED\n")
				<< (evidence.ResourceRolePaint
						? "UIFORETW_RESOURCE_ROLE_OK\n"
						: "UIFORETW_RESOURCE_ROLE_FAILED\n")
				<< (evidence.RcGesture ? "UIFORETW_RC_GESTURE_OK\n"
									   : "UIFORETW_RC_GESTURE_FAILED\n")
				<< (evidence.RcExactBlobs ? "UIFORETW_RC_BLOBS_OK\n"
										  : "UIFORETW_RC_BLOBS_FAILED\n")
				<< (evidence.RcIntralinePairs
						? "UIFORETW_RC_INTRALINE_OK\n"
						: "UIFORETW_RC_INTRALINE_FAILED\n")
				<< (evidence.RcRolePaint ? "UIFORETW_RC_ROLE_OK\n"
										 : "UIFORETW_RC_ROLE_FAILED\n")
				<< (evidence.FocusedReviewLayout
						? "UIFORETW_FOCUSED_REVIEW_LAYOUT_OK\n"
						: "UIFORETW_FOCUSED_REVIEW_LAYOUT_FAILED\n")
				<< (evidence.FixtureDiffHelperUnused
						? "UIFORETW_FIXTURE_DIFF_HELPER_UNUSED_OK\n"
						: "UIFORETW_FIXTURE_DIFF_HELPER_UNUSED_FAILED\n")
				<< (aRun.UIforEtwSelectedDiffPassed
						? "UIFORETW_SELECTED_DIFF_TRACE_OK\n"
						: "UIFORETW_SELECTED_DIFF_TRACE_FAILED\n")
				<< (aRun.UIforEtwSelectedDiffCaptured
						? "UIFORETW_SELECTED_DIFF_EVIDENCE_OK\n"
						: "UIFORETW_SELECTED_DIFF_EVIDENCE_FAILED\n");
		}
		if (aRun.ReviewLedgerCompositionProbe.has_value()) {
			const auto  course   = *aRun.ReviewLedgerCompositionProbe;
			const auto  name     = course
					== bpr::app::detail::review_ledger_composition_probe::seed
				? QStringLiteral("SEED")
				: course == bpr::app::detail::review_ledger_composition_probe::reopen
				? QStringLiteral("REOPEN")
				: QStringLiteral("UNAVAILABLE");
			const auto& evidence = aRun.ReviewLedgerCompositionEvidence;
			stream << "REVIEW_LEDGER_COMPOSITION_" << name
				   << (aRun.ReviewLedgerCompositionPassed ? "_OK\n" : "_FAILED\n")
				   << "REVIEW_LEDGER_COMPOSITION_EVIDENCE:"
				   << evidence.ExactAuthority << evidence.HostComposed
				   << evidence.RealFileGesture << evidence.RealSelectedDiff
				   << evidence.ExactMaterial
				   << evidence.ExpectedPreGesturePersistence
				   << evidence.ReceiptLoadedOnEntry << evidence.GestureSequence
				   << evidence.DurableReceipt << evidence.ActionabilityProjection
				   << evidence.PrivateRecords << evidence.PublicPathAvailable
				   << evidence.UnavailableVisible << evidence.HostOnlyProbe
				   << evidence.ReadStateObserved << '\n';
			if (!evidence.ProgressMark.empty()) {
				stream << "REVIEW_LEDGER_PROGRESS_MARK:" << QString::fromStdString(evidence.ProgressMark) << '\n';
			}
		}
		if (aRun.ThreadedReplyProbe) {
			const auto& evidence = aRun.ThreadedReplyEvidence;
			stream << (aRun.ThreadedReplyPassed ? "THREADED_REPLY_OK\n" : "THREADED_REPLY_FAILED\n")
				   << "THREADED_REPLY_EVIDENCE:" << evidence.ReplyDrafted << evidence.ReplyPublished
				   << evidence.ReplyUnderThread << evidence.ResolveAsked << evidence.ThreadResolved << '\n';
			stream << "THREADED_REPLY_DIAGNOSTIC:" << QString::fromStdString(aRun.ThreadedReplyDiagnostic) << '\n';
		}
		if (aRun.LabelAuthoringProbe) {
			const auto& evidence = aRun.LabelAuthoringEvidence;
			stream << (aRun.LabelAuthoringPassed ? "LABEL_AUTHORING_OK\n" : "LABEL_AUTHORING_FAILED\n")
				   << "LABEL_AUTHORING_EVIDENCE:" << evidence.ReviewEmptyDirected << evidence.FileShown << evidence.ThreadMarksDrawn << evidence.CommentEditable
				   << evidence.ShortcodeApplied << evidence.ChipApplied << evidence.ShortcutApplied << evidence.Published
				   << evidence.PublishedMarked << evidence.PatchMarkedNative << evidence.PatchPublishedExact << evidence.ProseUnmarkedExact
				   << evidence.RefusedTextKept << evidence.RefusedMarkedTextOnly << evidence.ReplyMarkedNative << evidence.IssueOffersTracking
				   << evidence.TrackedOnlyByGesture << evidence.RelabelKeptDefect << evidence.ResolveOnlyEvidence << evidence.TourStopLabelled << '\n';
			// [[BPR-1212]]. The keyboard line cursor and Return in the code.
			stream << "LINE_DRAFT_EVIDENCE:" << evidence.LeftLineRefused << evidence.SingleLineOpenedPlain << evidence.KeyboardRangeSelected
				   << evidence.SuggestionPrefilled << evidence.RangePublishedExact << evidence.NoLineExplained << '\n';
			// [[BPR-1212]] v002. A comment on a base line, as `side: LEFT`.
			stream << "BASE_LINE_DRAFT_EVIDENCE:" << evidence.BaseNumberedKnown << evidence.BaseLineOpened << evidence.BasePublishedLeft << '\n';
			stream << "LABEL_AUTHORING_DIAGNOSTIC:" << QString::fromStdString(aRun.LabelAuthoringDiagnostic) << '\n';
		}
		if (aRun.PublishedFileSwitchProbe) {
			const auto& evidence = aRun.PublishedFileSwitchEvidence;
			stream << (aRun.PublishedFileSwitchPassed ? "PUBLISHED_FILE_SWITCH_OK\n" : "PUBLISHED_FILE_SWITCH_FAILED\n")
				   << "PUBLISHED_FILE_SWITCH_EVIDENCE:" << evidence.Written << evidence.ShownBefore << evidence.OtherFileShown
				   << evidence.Kept << evidence.ShownAfterReturn << '\n';
			stream << "PUBLISHED_FILE_SWITCH_DIAGNOSTIC:" << QString::fromStdString(aRun.PublishedFileSwitchDiagnostic) << '\n';
		}
		if (aRun.ReplyDraftCourse.has_value()) {
			const auto& evidence = aRun.ReplyDraftEvidence;
			stream << (aRun.ReplyDraftPassed ? "REPLY_DRAFT_OK\n" : "REPLY_DRAFT_FAILED\n")
				   << "REPLY_DRAFT_EVIDENCE:" << evidence.Written << evidence.ServerShown << evidence.Kept << evidence.Presented << evidence.Flagged
				   << evidence.Acted << evidence.NothingSent << '\n';
			stream << "REPLY_DRAFT_DIAGNOSTIC:" << QString::fromStdString(aRun.ReplyDraftDiagnostic) << '\n';
		}
		if (aRun.StartReviewCourse.has_value()) {
			const auto& evidence = aRun.StartReviewEvidence;
			stream << (aRun.StartReviewPassed ? "START_REVIEW_OK\n" : "START_REVIEW_FAILED\n");
			switch (*aRun.StartReviewCourse) {
			case bpr::app::detail::start_review_course::pending:
				stream << "START_REVIEW_PENDING_EVIDENCE:" << evidence.Started << evidence.Added << evidence.Drawn << evidence.SingleAlone << evidence.NavigationKept << '\n';
				break;
			case bpr::app::detail::start_review_course::finish:
				stream << "START_REVIEW_FINISH_EVIDENCE:" << evidence.Restored << evidence.FinishOpened << evidence.CommentWithoutSummary << evidence.SubmittedExact << evidence.MarkedPublished << '\n';
				break;
			case bpr::app::detail::start_review_course::refused:
				stream << "START_REVIEW_REFUSED_EVIDENCE:" << evidence.Started << evidence.FinishOpened << evidence.RefusedKept << evidence.DiscardDeleted << '\n';
				break;
			case bpr::app::detail::start_review_course::again:
				stream << "START_REVIEW_AGAIN_EVIDENCE:" << evidence.Started << evidence.Added << evidence.FinishOpened << evidence.RowRevealed << evidence.RowDeleted << evidence.DiscardDeleted
					   << evidence.SecondDiscard << evidence.AgainStarted << evidence.AgainOffered << evidence.AgainSubmitted << '\n';
				break;
			case bpr::app::detail::start_review_course::uncertain:
				stream << "START_REVIEW_UNCERTAIN_EVIDENCE:" << evidence.Started << evidence.FinishOpened << evidence.UncertainHeld << evidence.Reconciled << '\n';
				break;
			}
			stream << "START_REVIEW_DIAGNOSTIC:" << QString::fromStdString(aRun.StartReviewDiagnostic) << '\n';
		}
		if (aRun.ReviewProgressPushProbe.has_value()) {
			stream << (aRun.ReviewProgressPushPassed ? "REVIEW_PROGRESS_PUSH_OK\n" : "REVIEW_PROGRESS_PUSH_FAILED\n")
				   << "REVIEW_PROGRESS_PUSH_MARKS:" << QString::fromStdString(aRun.ReviewProgressPushMarks) << '\n'
				   << "REVIEW_PROGRESS_PUSH_DIAGNOSTIC:" << QString::fromStdString(aRun.ReviewProgressPushDiagnostic) << '\n';
		}
		if (aRun.ReviewRoundTripCompositionProbe) {
			const auto& evidence = aRun.ReviewRoundTripCompositionEvidence;
			stream << (aRun.ReviewRoundTripCompositionPassed
						   ? "REVIEW_ROUND_TRIP_COMPOSITION_OK\n"
						   : "REVIEW_ROUND_TRIP_COMPOSITION_FAILED\n")
				   << "REVIEW_ROUND_TRIP_COMPOSITION_EVIDENCE:"
				   << evidence.ExactDesignatedTarget << evidence.EditorComposed
				   << evidence.SemanticEdit << evidence.StaleHeadRefused
				   << evidence.LostResultRefreshed << evidence.SelectionDeferred
				   << evidence.LatestSelectionWins << evidence.PendingCleared
				   << evidence.DeferredDraftExact
				   << evidence.StaleTerminalWithheld << evidence.RetryIdempotent
				   << evidence.CapabilityFallback << evidence.ServerAuthority
				   << evidence.TypedQtProjection << '\n';
			if (dormantSelection.has_value()) {
				stream << "DORMANT_LIVE_PROVIDER_SELECTION:"
					   << dormantSelection->ExactTupleAccepted << ':'
					   << static_cast<unsigned long long>(
							  dormantSelection->MismatchedTuplesRejected)
					   << '/'
					   << static_cast<unsigned long long>(
							  dormantSelection->ExpectedMismatchRejections)
					   << ':' << dormantSelection->FixtureProviderSelected
					   << ':' << dormantSelection->LiveProviderAbsent
					   << '\n';
			}
		}
		if (aRun.DefectCompositionProbe.has_value()) {
			const auto  name     = *aRun.DefectCompositionProbe
					== bpr::app::detail::defect_composition_probe::seed
				? QStringLiteral("SEED")
				: QStringLiteral("REOPEN");
			const auto& evidence = aRun.DefectCompositionEvidence;
			stream << "DEFECT_COMPOSITION_" << name
				   << (aRun.DefectCompositionPassed ? "_OK\n" : "_FAILED\n")
				   << "DEFECT_COMPOSITION_EVIDENCE:" << evidence.ExactThreads
				   << evidence.LocalClassification << evidence.CorrelatedEvidence
				   << evidence.ExplicitVerification << evidence.DurableHistory
				   << evidence.MultiDefectNavigation << evidence.SharedActionSeparate
				   << evidence.RestartRestored << evidence.ZeroReopenWrites
				   << evidence.VisibleProjection
				   << evidence.PersistFailureFailsClosed << '\n';
		}
#endif
		auto reportRender = aRun.RenderPath.has_value();
#if defined(BPR_BUILD_TESTING)
		reportRender = reportRender
			&& (!aRun.Appearance.Enabled || aRun.Appearance.StageEntered);
#endif
		if (reportRender) {
			stream << (renderSaved ? "RENDER_OK\n" : "RENDER_FAILED\n");
		}
#if defined(BPR_BUILD_TESTING)
		// `automated_run::Fixture` and the ledger fields only exist in a
		// tests-enabled build, so this block cannot be compiled without the
		// guard. It was outside one, and the tests-disabled Release
		// composition has not compiled since; `application_composition_contract`
		// reported Passed throughout because its `PASS_REGULAR_EXPRESSION "OK"`
		// matched the `REVIEW_LEDGER_OK` literal a few lines below.
		if (fixtureCourseRan && !aRun.WorkbenchCompositionProbe
			&& !aRun.UIforEtwSelectedDiffProbe
			&& !aRun.Appearance.Enabled
			&& !aRun.ReviewRoundTripCompositionProbe && !aRun.ThreadedReplyProbe
			&& !aRun.LabelAuthoringProbe && !aRun.PublishedFileSwitchProbe
			&& !aRun.StartReviewCourse.has_value()
			&& !aRun.ReplyDraftCourse.has_value()
			&& !aRun.ReviewProgressPushProbe.has_value()
			&& !aRun.DefectCompositionProbe.has_value()) {
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
#endif
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
		// Where the deliveries that did not reach a pane went, and whether the
		// comparison on screen right now carries spans per side (BPR-0368).
		// The counters above are history; the reviewer looks at the present,
		// which is how a monochrome pane hid behind an all-green marker set.
		stream << "SYNTAX_DELIVERY:"
			   << aApplication.status().ClassificationCacheHits << ':'
			   << aApplication.status().CoalescedClassifications << ':'
			   << aApplication.status().DroppedClassifications << ':'
			   << aApplication.status().ClassificationFailures << ':'
			   << aApplication.status().ClassificationTerminals << '\n';
		// "Right now" is the moment the course ended, not after teardown: the
		// closing window releases its read host and with it the comparison,
		// so the status captured at the shutdown transition is the one that
		// still describes the screen (BPR-0723).
		const auto& displayed = aRun.CourseEndStatus ? *aRun.CourseEndStatus : status;
		stream << "SYNTAX_DISPLAYED:"
			   << displayed.DisplayedLeftClassified << ':'
			   << displayed.DisplayedRightClassified << '\n';
		// How many spans those panes carry, and how many adoptions were taken on
		// the display turn rather than on delivery. Presence is too weak to be
		// the check: `definition_for_path` falls back to plain text rather than
		// failing, and a plain-text classification still publishes a non-null
		// list, so a pane can hold a span list and be entirely uncoloured. A
		// count separates "classified" from "a list arrived" (BPR-0390).
		stream << "SYNTAX_DISPLAYED_SPANS:"
			   << displayed.DisplayedLeftSpans << ':'
			   << displayed.DisplayedRightSpans << ':'
			   << displayed.DisplayAdoptedClassifications << ':'
			   << (displayed.DisplayedComparison ? 1 : 0) << '\n';
		// The settings in effect. A composition starts from `system` and a tab
		// width of four, so a run that made no gesture and reports anything
		// else read it from the durable record - which is the restart claim.
		stream << "APPEARANCE_SETTINGS:"
			   << QString::fromStdString(aApplication.status().AppearanceMode)
			   << ':' << aApplication.status().AppearanceTabWidth << '\n';
		const auto dif = aApplication.status().DifResolution;
		stream << "DIF_PREFERENCES:"
			   << (dif.Preferences.Visibility == bpr::core::dif_visibility::shown
					   ? "shown"
					   : "hidden")
			   << ':'
			   << (dif.Preferences.Motion == bpr::core::motion_mode::system
					   ? "system"
					   : dif.Preferences.Motion == bpr::core::motion_mode::full
					   ? "full"
					   : "reduced")
			   << ':'
			   << (dif.EffectiveMotion == bpr::core::motion_mode::full
					   ? "full"
					   : "reduced")
			   << '\n';
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
		// The precondition the permission above cannot express. A `none` here
		// makes every other notification marker moot: nothing the desktop could
		// have shown was ever addressed to a registered application.
		stream << "NOTIFICATION_BUNDLE_IDENTITY:"
			   << notification_bundle_identity() << '\n';
		stream << "APPEARANCE_DIVERGENCE:"
			   << aApplication.status().AppearanceDivergences << '\n';
		// What was observed and what it resolved to, beside what was applied.
		// Without this a System-mode window painted light on a dark desktop is
		// indistinguishable from a correctly resolved light one, which is how
		// BPR-0216 survived an entirely green marker set.
		stream << "APPEARANCE_OBSERVED:"
			   << os_color_scheme_name(aApplication.status().ObservedScheme)
			   << ':'
			   << appearance_variant_name(aApplication.status().ResolvedVariant)
			   << ':' << aApplication.status().CustomisedRoles << '\n';
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
		stream << (aApplication.status().AppearancePersistenceAvailable
					   ? "APPEARANCE_PERSISTENCE_AVAILABLE\n"
					   : "APPEARANCE_PERSISTENCE_UNAVAILABLE\n");
		stream << "APPEARANCE_DURABLE_WRITES:"
			   << static_cast<unsigned long long>(
					  aApplication.status().AppearanceDurableWrites)
			   << '\n';
#if defined(BPR_BUILD_TESTING)
		if (aRun.Appearance.Enabled) {
			const auto& selected = aRun.Appearance.SelectedDiffEvidence;
			stream << native_selected_diff_result_marker(
				aRun.Appearance.SelectedDiffResult)
				   << "NATIVE_APPEARANCE_SELECTED_DIFF_REASON:"
				   << native_selected_diff_reason_name(selected.Reason) << '\n'
				   << "NATIVE_APPEARANCE_SELECTED_DIFF_BINDING:"
				   << selected.ExactAuthority << selected.ExactRevisions
				   << selected.ExactFile << selected.ExactBlobs
				   << selected.ExactOptions << selected.PresentationReady << '\n'
				   << (selected.SelectionGesture && selected.SelectionIntent
						   ? "NATIVE_APPEARANCE_SELECTION_WIRE_OK\n"
						   : "NATIVE_APPEARANCE_SELECTION_WIRE_FAILED\n")
				   << (selected.FixtureDiffHelperUnused
						   ? "NATIVE_APPEARANCE_FIXTURE_DIFF_HELPER_UNUSED_OK\n"
						   : "NATIVE_APPEARANCE_FIXTURE_DIFF_HELPER_UNUSED_FAILED\n");
			if (aRun.Appearance.PreconditionRefusal
				!= native_appearance_precondition_refusal::none) {
				stream << "NATIVE_APPEARANCE_PRECONDITION_REFUSED:"
					   << native_appearance_precondition_name(
							  aRun.Appearance.PreconditionRefusal)
					   << ':' << native_selected_diff_reason_name(selected.Reason)
					   << '\n';
			}
			if (aRun.Appearance.InitialCaptureDisposition
				!= native_appearance_capture_disposition::
					not_attempted_state_precondition) {
				stream << "NATIVE_APPEARANCE_INITIAL_CAPTURE:"
					   << native_appearance_capture_disposition_name(
							  aRun.Appearance.InitialCaptureDisposition)
					   << '\n';
			}
			if (aRun.Appearance.StageEntered) {
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
					   << "NATIVE_APPEARANCE_OWNER_WINDOW_ACTIVE:"
					   << observed_boolean_name(
							  aRun.Appearance.OwnerReturnEvaluated,
							  aRun.Appearance.OwnerWindowActive)
					   << '\n'
					   << "NATIVE_APPEARANCE_OWNER_APPLICATION_ACTIVE_WINDOW:"
					   << observed_boolean_name(
							  aRun.Appearance.OwnerReturnEvaluated,
							  aRun.Appearance.OwnerApplicationActiveWindow)
					   << '\n'
					   << "NATIVE_APPEARANCE_OWNER_EXACT_FOCUS:"
					   << observed_boolean_name(
							  aRun.Appearance.OwnerReturnEvaluated,
							  aRun.Appearance.OwnerExactFocus)
					   << '\n'
					   << "NATIVE_APPEARANCE_CHANGED_CAPTURE:"
					   << native_appearance_capture_disposition_name(
							  aRun.Appearance.ChangedCaptureDisposition)
					   << '\n'
					   << "NATIVE_APPEARANCE_PERSISTENCE:"
					   << native_appearance_persistence_disposition_name(
							  nativeAppearancePersistence)
					   << '\n'
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
					   << aRun.Appearance.DiffGeometryRebuildDelta << '\n';
				stream << "NATIVE_APPEARANCE_DIFF_GEOMETRY_REBUILD_CAUSES:"
					   << static_cast<unsigned long long>(
							  aRun.Appearance.DiffGeometryRebuildCauseCount);
				for (
					auto index = std::size_t{ 0U };
					index < aRun.Appearance.DiffGeometryRebuildCauseCount;
					++index) {
					const auto& cause
						= aRun.Appearance.DiffGeometryRebuildCauses[index];
					stream << ':'
						   << static_cast<std::uint32_t>(cause.Changes) << '/'
						   << static_cast<std::uint32_t>(cause.Triggers);
				}
				stream << '\n'
					   << (aRun.Appearance.DiffGeometryAttributionComplete
							   ? "NATIVE_APPEARANCE_DIFF_GEOMETRY_ATTRIBUTION_OK\n"
							   : "NATIVE_APPEARANCE_DIFF_GEOMETRY_ATTRIBUTION_FAILED\n")
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
					   << (aRun.Appearance.DiffGeometryRebuildDelta == 0U
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
		}
#endif
		aApplication.request_quit(passed ? EXIT_SUCCESS : EXIT_FAILURE);
	}

	void transition_to_shutdown(
		bpr::app::github_application& aApplication,
		automated_run&                aRun)
	{
		// What the course left on screen, read before shutdown. Since
		// BPR-0697 and BPR-0705 a closing window releases its read host,
		// which invalidates the comparison it held, so a status read after
		// teardown reports an empty surface for every course (BPR-0723).
		aRun.CourseEndStatus = aApplication.status();
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

#if defined(BPR_BUILD_TESTING)
	void advance_workbench_composition_probe(
		bpr::app::github_application& aApplication,
		automated_run&                aRun)
	{
		const auto outcome = bpr::app::detail::github_application_fixture_access::
			exercise_workbench_composition(aApplication);
		aRun.WorkbenchCompositionEvidence = bpr::app::detail::
			github_application_fixture_access::workbench_composition_evidence(
				aApplication);
		if (outcome
			== bpr::app::detail::workbench_composition_probe_result::pending) {
			return;
		}
		aRun.WorkbenchCompositionPassed = outcome
			== bpr::app::detail::workbench_composition_probe_result::passed;
		aRun.PresentationPassed = aRun.WorkbenchCompositionPassed;
		if (aRun.WorkbenchCompositionPassed) {
			transition_to_shutdown(aApplication, aRun);
		} else {
			fail_automated_run(aApplication, aRun);
		}
	}

	// [[BPR-1083]]. `--attended-open-file`: before a person looks, the
	// fixture's account signs in, the pull request opens from the Home bar and
	// the named file is selected in Review, the way a reviewer would get there.
	// It then leaves the window to the person and prints one line saying
	// whether it got there. [[BPR-1287]]. With no path
	// (`--attended-open-pull-request`) it stops once the pull request is open.
	void schedule_attended_preflight_look(bpr::app::github_application& aApplication, std::string aLook);
	void schedule_attended_workshop_look(bpr::app::github_application& aApplication);

	void schedule_attended_reveal(
		bpr::app::github_application& aApplication,
		std::string                   aPath,
		QString                       aPullRequest,
		std::optional<std::string>    aLook         = std::nullopt,
		std::optional<std::string>    aReadState    = std::nullopt,
		const bool                    aOpenOnGithub = false)
	{
		enum class step : std::uint8_t {
			connect,
			open,
			reveal,
		};
		struct reveal_state
		{
			step                        Step = step::connect;
			automated_clock::time_point Started;
		};
		auto        state = std::make_shared<reveal_state>(reveal_state{ .Started = automated_clock::now() });
		auto* const timer = new QTimer{ &aApplication.window() };
		timer->setInterval(50);
		QObject::connect(timer, &QTimer::timeout, timer, [&aApplication, timer, state, path = std::move(aPath), pullRequest = std::move(aPullRequest), look = std::move(aLook), readState = std::move(aReadState), aOpenOnGithub, alive = aApplication.liveness()] {
			if (alive.expired()) {
				return;
			}
			auto&      window = aApplication.window();
			const auto finish = [timer](const QString& aLine) {
				QTextStream{ stdout } << aLine << '\n';
				timer->stop();
				timer->deleteLater();
			};
			if (automated_clock::now() - state->Started > std::chrono::seconds{ 90 }) {
				finish(QStringLiteral("ATTENDED_FILE_FAILED:timeout:step=%1").arg(static_cast<int>(state->Step)));
				return;
			}
			switch (state->Step) {
			case step::connect:
				if (window.github_connect_action()->isEnabled()) {
					window.github_connect_action()->trigger();
					state->Step = step::open;
				}
				return;
			case step::open:
				if (window.github_status_text() == "GitHub account connected." && window.github_open_action()->isEnabled()) {
					window.home_open_bar()->setText(pullRequest);
					window.home_open_action()->trigger();
					state->Step = step::reveal;
				}
				return;
			case step::reveal:
				if (path.empty()) {
					if (!window.showing_home() && (!aOpenOnGithub || window.open_on_github_action()->isEnabled())) {
						if (aOpenOnGithub) {
							// [[BPR-1338]]. The window's own action, as the title
							// row's button triggers it; the status bar says a
							// refusal, and nothing when the page was handed over.
							window.open_on_github_action()->trigger();
							QTextStream{ stdout } << "ATTENDED_OPEN_ON_GITHUB_TRIGGERED:status=" << window.statusBar()->currentMessage() << '\n';
						}
						finish(QStringLiteral("ATTENDED_PULL_REQUEST_OPENED"));
					}
					return;
				}
				switch (bpr::app::detail::github_application_fixture_access::reveal_fixture_file(aApplication, path)) {
				case bpr::app::detail::fixture_file_reveal_result::pending:
					return;
				case bpr::app::detail::fixture_file_reveal_result::shown:
					// [[BPR-1293]]. The fixture's read state, recorded through the
					// review ledger before the person looks.
					if (readState) {
						const auto marked = bpr::app::detail::github_application_fixture_access::apply_fixture_read_state(aApplication, *readState);
						QTextStream{ stdout } << (marked ? QStringLiteral("ATTENDED_READ_STATE:%1").arg(*marked) : QStringLiteral("ATTENDED_READ_STATE_FAILED")) << '\n';
					}
					// [[BPR-1331]]. Which head is shown, so a launcher can tell
					// the first head from a pushed one.
					QTextStream{ stdout } << "ATTENDED_HEAD:" << QString::fromStdString(bpr::app::detail::github_application_fixture_access::accepted_head(aApplication)) << '\n';
					finish(QStringLiteral("ATTENDED_FILE_SHOWN:%1").arg(QString::fromStdString(path)));
					if (look == "workshop") {
						schedule_attended_workshop_look(aApplication);
					} else if (look) {
						schedule_attended_preflight_look(aApplication, *look);
					}
					return;
				case bpr::app::detail::fixture_file_reveal_result::failed:
					finish(QStringLiteral("ATTENDED_FILE_FAILED:%1").arg(QString::fromStdString(path)));
					return;
				}
				return;
			}
		});
		timer->start();
	}

	// [[BPR-1357]]. `--attended-home-look`: before a person looks, the
	// fixture's account signs in, opens its pull request once from the Home
	// bar (so Recently viewed holds it), saves its repository from Settings ›
	// Repositories (so discovery lists Home's rows), and returns to Home. Only
	// then is the slow network armed, so everything the person opens from Home
	// waits `aDelay` milliseconds on each of its first two answers. One line
	// says whether it got there.
	void schedule_attended_home_look(
		bpr::app::github_application&                                aApplication,
		QString                                                      aPullRequest,
		std::shared_ptr<bpr::app::detail::github_read_delay_fixture> aDelay,
		const std::uint32_t                                          aMilliseconds)
	{
		enum class step : std::uint8_t {
			connect,
			open,
			opened,
			save,
			saved,
		};
		struct look_state
		{
			step                        Step = step::connect;
			automated_clock::time_point Started;
		};
		auto        state = std::make_shared<look_state>(look_state{ .Started = automated_clock::now() });
		auto* const timer = new QTimer{ &aApplication.window() };
		timer->setInterval(50);
		QObject::connect(timer, &QTimer::timeout, timer, [&aApplication, timer, state, pullRequest = std::move(aPullRequest), delay = std::move(aDelay), aMilliseconds, alive = aApplication.liveness()] {
			if (alive.expired()) {
				return;
			}
			auto&      window = aApplication.window();
			const auto finish = [timer](const QString& aLine) {
				QTextStream{ stdout } << aLine << '\n';
				timer->stop();
				timer->deleteLater();
			};
			if (automated_clock::now() - state->Started > std::chrono::seconds{ 90 }) {
				finish(QStringLiteral("ATTENDED_HOME_FAILED:timeout:step=%1").arg(static_cast<int>(state->Step)));
				return;
			}
			const auto rows = [&window] {
				auto count = 0;
				for (const auto group : { bpr::qt::home_group::needs_you, bpr::qt::home_group::my_pull_requests, bpr::qt::home_group::waiting_on_others, bpr::qt::home_group::following }) {
					if (auto* const model = window.home_list()->state_model(group); model != nullptr) {
						count += model->rowCount();
					}
				}
				return count;
			};
			switch (state->Step) {
			case step::connect:
				if (window.github_connect_action()->isEnabled()) {
					window.github_connect_action()->trigger();
					state->Step = step::open;
				}
				return;
			case step::open:
				if (window.github_status_text() == "GitHub account connected." && window.github_open_action()->isEnabled()) {
					window.home_open_bar()->setText(pullRequest);
					window.home_open_action()->trigger();
					state->Step = step::opened;
				}
				return;
			case step::opened:
				if (!window.showing_home() && window.github_status_text() == "Pull request ready.") {
					window.home_action()->trigger();
					state->Step = step::save;
				}
				return;
			case step::save:
				if (window.showing_home() && window.add_saved_repository_action() != nullptr) {
					window.open_repository_addition();
					window.saved_repository_name()->setText(pullRequest.section(QLatin1Char{ '#' }, 0, 0));
					if (auto* const add = window.repository_addition_draft()->findChild<QPushButton*>(QStringLiteral("repositoryAdditionAdd")); add != nullptr && add->isEnabled()) {
						add->click();
						state->Step = step::saved;
					}
				}
				return;
			case step::saved:
				if (!window.repository_addition_draft()->isVisible() && rows() >= 2) {
					window.settings()->hide();
					window.home_action()->trigger();
					delay->Milliseconds = aMilliseconds;
					finish(QStringLiteral("ATTENDED_HOME_SHOWN:rows=%1:delay=%2ms").arg(rows()).arg(aMilliseconds));
				}
				return;
			}
		});
		timer->start();
	}

	// [[BPR-1289]]. `--attended-open-settings`: before a person looks,
	// Settings opens over the window through the workbench's own Settings
	// action, as Command-comma opens it, and one line says whether it showed.
	void schedule_attended_settings(bpr::app::github_application& aApplication)
	{
		QTimer::singleShot(0, &aApplication.window(), [&aApplication, alive = aApplication.liveness()] {
			if (alive.expired()) {
				return;
			}
			auto& window = aApplication.window();
			window.settings_action()->trigger();
			QTextStream{ stdout } << (window.settings()->isVisible() ? "ATTENDED_SETTINGS_SHOWN" : "ATTENDED_SETTINGS_FAILED") << '\n';
		});
	}

	// [[BPR-1084]] and [[BPR-1085]]. `--attended-preflight-look`: on
	// conventional-comments-v1, once the file is shown, a new blocking comment
	// carrying a suggested change is written on a head line through the
	// window's own Comment action, so its suggestion mark shows beside
	// Publish (`native`). `text-only` writes one the fixture refuses an inline
	// comment for and presses Publish once: the fixture's synthetic 422 comes
	// back, the text is kept and the mark says text only. `review` then opens
	// Compose review, whose preflight counts the two blocking threads the
	// launch staged as the fixture account's own and the unsent draft. Nothing
	// reaches GitHub; one line says where it got.
	void schedule_attended_preflight_look(bpr::app::github_application& aApplication, std::string aLook)
	{
		const auto  refused = aLook == "text-only";
		const auto  body    = refused ? std::string{ "issue (blocking): Clamp the width to one pixel. BPR-1084 off-hunk\n\n```suggestion\n\tconst auto width = std::max(1, requested);\n\treturn width;\n```\n" }
									  : std::string{ "issue (blocking): Clamp the width to one pixel.\n\n```suggestion\n\tconst auto width = std::max(1, requested);\n\treturn width;\n```\n" };
		auto* const timer   = new QTimer{ &aApplication.window() };
		timer->setInterval(50);
		const auto started   = std::make_shared<automated_clock::time_point>(automated_clock::now());
		const auto published = std::make_shared<bool>(false);
		QObject::connect(timer, &QTimer::timeout, timer, [&aApplication, timer, started, published, refused, body, look = std::move(aLook), alive = aApplication.liveness()] {
			if (alive.expired()) {
				return;
			}
			const auto finish = [timer](const QString& aLine) {
				QTextStream{ stdout } << aLine << '\n';
				timer->stop();
				timer->deleteLater();
			};
			if (automated_clock::now() - *started > std::chrono::seconds{ 60 }) {
				finish(QStringLiteral("ATTENDED_LOOK_FAILED:timeout"));
				return;
			}
			auto&       window = aApplication.window();
			const auto* mark   = window.review_editor()->suggestion_mark_widget();
			const auto  shown  = [&window, mark] { return mark != nullptr && mark->isVisibleTo(window.review_editor()) ? mark->property("suggestionApplicability").toString() : QStringLiteral("hidden"); };
			if (!*published) {
				switch (bpr::app::detail::github_application_fixture_access::seed_attended_comment_draft(aApplication, bpr::core::diff_side::right, body)) {
				case bpr::app::detail::fixture_file_reveal_result::pending:
					return;
				case bpr::app::detail::fixture_file_reveal_result::failed:
					finish(QStringLiteral("ATTENDED_LOOK_FAILED:draft"));
					return;
				case bpr::app::detail::fixture_file_reveal_result::shown:
					break;
				}
				if (refused) {
					if (!window.review_editor()->publish_action()->isEnabled()) {
						return;
					}
					window.review_editor()->publish_action()->trigger();
					*published = true;
					return;
				}
			} else if (shown() != QStringLiteral("textual_only")) {
				return;
			}
			if (look == "review") {
				window.open_review_composition();
				QCoreApplication::processEvents();
				finish(QStringLiteral("ATTENDED_LOOK_SHOWN:review:mark=%1:chip=%2").arg(shown(), window.review_submission_editor_view()->preflight_button()->toolTip().replace(QLatin1Char{ '\n' }, QStringLiteral(" / "))));
				return;
			}
			finish(QStringLiteral("ATTENDED_LOOK_SHOWN:%1:mark=%2").arg(QString::fromStdString(look), shown()));
		});
		timer->start();
	}

	// [[BPR-1091]]. `--attended-workshop-agent`: once the file is shown, a
	// labelled question is written on a head line through the window's own
	// Comment action, Review feedback's guidance is customized on the shipped
	// Settings row (the label helper is on), and Clarify is pressed once. The
	// fixture agent named on the command line answers with a blocking issue,
	// so the workshop's before, after and candidate line are on screen when
	// the person looks. Nothing reaches GitHub; one line says where it got.
	void schedule_attended_workshop_look(bpr::app::github_application& aApplication)
	{
		enum class step : std::uint8_t {
			draft,
			guidance,
			saved,
			press,
			convert,
			proposal,
		};
		struct look_state
		{
			step                        Step = step::draft;
			automated_clock::time_point Started;
			bool                        Themed = false;
		};
		static constexpr auto original = std::string_view{ "question: Is the exhausted retry budget reported to the caller?" };
		static constexpr auto guidance = std::string_view{ "Label every comment. State a blocking problem plainly rather than asking about it." };
		auto                  state    = std::make_shared<look_state>(look_state{ .Started = automated_clock::now() });
		auto* const           timer    = new QTimer{ &aApplication.window() };
		timer->setInterval(50);
		QObject::connect(timer, &QTimer::timeout, timer, [&aApplication, timer, state, alive = aApplication.liveness()] {
			if (alive.expired()) {
				return;
			}
			const auto finish = [timer](const QString& aLine) {
				QTextStream{ stdout } << aLine << '\n';
				timer->stop();
				timer->deleteLater();
			};
			if (automated_clock::now() - state->Started > std::chrono::seconds{ 90 }) {
				const auto* const shown   = aApplication.window().review_editor();
				const auto        finding = bpr::app::detail::github_application_fixture_access::agent_finding_evidence(aApplication);
				const auto        invoked = bpr::app::detail::github_application_fixture_access::agent_invocation_evidence(aApplication);
				finish(QStringLiteral("ATTENDED_WORKSHOP_FAILED:timeout:step=%1:%2:pending=%3:bound=%4:starts=%5:panel=%6")
						   .arg(static_cast<int>(state->Step))
						   .arg(shown != nullptr ? shown->workshop_operation_text() : QString{ })
						   .arg(QString::fromStdString(finding.WorkshopPendingOperation))
						   .arg(finding.WorkshopPendingSessionBound ? 1 : 0)
						   .arg(invoked.Statistics.ProcessStarts)
						   .arg(QString::fromStdString(finding.AgentPanelStatusText)));
				return;
			}
			auto&       window = aApplication.window();
			auto* const editor = window.review_editor();
			auto* const hooks  = window.writing_hooks();
			auto* const choice = hooks != nullptr ? hooks->findChild<QComboBox*>(QStringLiteral("writingHookChoice")) : nullptr;
			auto* const text   = hooks != nullptr ? hooks->findChild<QPlainTextEdit*>(QStringLiteral("writingHookInstructions")) : nullptr;
			auto* const save   = hooks != nullptr ? hooks->findChild<QPushButton*>(QStringLiteral("writingHookSave")) : nullptr;
			auto* const source = hooks != nullptr ? hooks->findChild<QLabel*>(QStringLiteral("writingHookSource")) : nullptr;
			auto* const scope  = hooks != nullptr ? hooks->findChild<QLabel*>(QStringLiteral("writingHookScope")) : nullptr;
			if (editor == nullptr || choice == nullptr || text == nullptr || save == nullptr || source == nullptr || scope == nullptr) {
				finish(QStringLiteral("ATTENDED_WORKSHOP_FAILED:surfaces"));
				return;
			}
			switch (state->Step) {
			case step::draft:
				// An offscreen look at the result in a named appearance, for
				// the person preparing the session; a person's look leaves
				// the appearance alone.
				if (const auto theme = qEnvironmentVariable("BPR_ATTENDED_WORKSHOP_THEME"); !theme.isEmpty() && !state->Themed) {
					state->Themed = bpr::app::detail::github_application_fixture_access::apply_appearance_gesture(
						aApplication, theme == QStringLiteral("dark") ? bpr::core::appearance_mode::dark : bpr::core::appearance_mode::light, 4U);
					return;
				}
				switch (bpr::app::detail::github_application_fixture_access::seed_attended_comment_draft(aApplication, bpr::core::diff_side::right, std::string{ original })) {
				case bpr::app::detail::fixture_file_reveal_result::pending:
					return;
				case bpr::app::detail::fixture_file_reveal_result::failed:
					finish(QStringLiteral("ATTENDED_WORKSHOP_FAILED:draft"));
					return;
				case bpr::app::detail::fixture_file_reveal_result::shown:
					break;
				}
				choice->setCurrentIndex(choice->findData(QVariant::fromValue(static_cast<int>(bpr::core::writing_hook::feedback))));
				state->Step = step::guidance;
				return;
			case step::guidance:
				if (!scope->text().startsWith(QStringLiteral("On."))) {
					return;
				}
				text->setPlainText(QString::fromUtf8(guidance.data(), static_cast<qsizetype>(guidance.size())));
				save->click();
				state->Step = step::saved;
				return;
			case step::saved:
				if (source->text().startsWith(QStringLiteral("Profile override"))) {
					state->Step = step::press;
				}
				return;
			case step::press:
				if (editor->clarify_action() == nullptr || !editor->clarify_action()->isEnabled()) {
					return;
				}
				editor->clarify_action()->trigger();
				state->Step = step::convert;
				return;
			case step::convert:
				// The turn's answer arrives as a finding; converting it, on the
				// queue as a reviewer would, puts the proposal beside the draft.
				if (auto* const queue = bpr::app::detail::github_application_fixture_access::finding_queue(aApplication); queue != nullptr && queue->finding_count() != 0U) {
					queue->select(0U);
					if (auto* const convert = queue->action(bpr::qt::finding_queue_action::convert_to_draft_comment); convert != nullptr && convert->isEnabled()) {
						convert->trigger();
						state->Step = step::proposal;
					}
				}
				return;
			case step::proposal:
				if (!editor->shown_proposal().has_value()) {
					if (editor->workshop_operation_text().startsWith(QStringLiteral("No revision"))) {
						finish(QStringLiteral("ATTENDED_WORKSHOP_FAILED:press:%1").arg(editor->workshop_operation_text()));
					}
					return;
				}
				if (const auto render = qEnvironmentVariable("BPR_ATTENDED_WORKSHOP_RENDER"); !render.isEmpty()) {
					QCoreApplication::processEvents();
					static_cast<void>(window.grab().save(render));
					static_cast<void>(editor->grab().save(render + QStringLiteral(".editor.png")));
				}
				finish(QStringLiteral("ATTENDED_WORKSHOP_SHOWN:%1").arg(editor->workshop_candidate()->text()));
				return;
			}
		});
		timer->start();
	}

	// [[BPR-1083]]. One tick of the label-authoring course; ends the run when
	// the course does.
	void advance_label_authoring_probe(
		bpr::app::github_application& aApplication,
		automated_run&                aRun)
	{
		if (aRun.CancelFirstBaseNumbering && !aRun.BaseNumberingCancellationArmed) {
			bpr::app::detail::github_application_fixture_access::cancel_base_numbering_answers(aApplication, 1U);
			aRun.BaseNumberingCancellationArmed = true;
		}
		const auto result = bpr::app::detail::github_application_fixture_access::exercise_label_authoring(
			aApplication, aRun.LabelAuthoringEvidenceDirectory, aRun.LineDraftCourse);
		aRun.LabelAuthoringDiagnostic = bpr::app::detail::github_application_fixture_access::label_authoring_diagnostic(aApplication);
		if (result == bpr::app::detail::label_authoring_probe_result::pending) {
			return;
		}
		aRun.LabelAuthoringEvidence = bpr::app::detail::github_application_fixture_access::label_authoring_evidence(aApplication);
		aRun.LabelAuthoringPassed   = result == bpr::app::detail::label_authoring_probe_result::passed;
		if (!aRun.LabelAuthoringPassed) {
			fail_automated_run(aApplication, aRun);
			return;
		}
		aRun.PresentationPassed = true;
		transition_to_shutdown(aApplication, aRun);
	}

	// [[BPR-1326]]. One tick of the published-comment file-switch course.
	void advance_published_file_switch_probe(
		bpr::app::github_application& aApplication,
		automated_run&                aRun)
	{
		const auto result = bpr::app::detail::github_application_fixture_access::exercise_published_file_switch(
			aApplication, aRun.PublishedFileSwitchTiming);
		aRun.PublishedFileSwitchDiagnostic = bpr::app::detail::github_application_fixture_access::published_file_switch_diagnostic(aApplication);
		if (result == bpr::app::detail::published_file_switch_probe_result::pending) {
			return;
		}
		aRun.PublishedFileSwitchEvidence = bpr::app::detail::github_application_fixture_access::published_file_switch_evidence(aApplication);
		aRun.PublishedFileSwitchPassed   = result == bpr::app::detail::published_file_switch_probe_result::passed;
		if (!aRun.PublishedFileSwitchPassed) {
			fail_automated_run(aApplication, aRun);
			return;
		}
		aRun.PresentationPassed = true;
		transition_to_shutdown(aApplication, aRun);
	}

	// [[BPR-1374]]. One tick of the unsent-reply course.
	void advance_reply_draft_course(
		bpr::app::github_application& aApplication,
		automated_run&                aRun)
	{
		const auto result = bpr::app::detail::github_application_fixture_access::exercise_reply_draft(
			aApplication, *aRun.ReplyDraftCourse);
		aRun.ReplyDraftDiagnostic = bpr::app::detail::github_application_fixture_access::reply_draft_diagnostic(aApplication);
		if (result == bpr::app::detail::reply_draft_probe_result::pending) {
			return;
		}
		aRun.ReplyDraftEvidence = bpr::app::detail::github_application_fixture_access::reply_draft_evidence(aApplication);
		aRun.ReplyDraftPassed   = result == bpr::app::detail::reply_draft_probe_result::passed;
		if (!aRun.ReplyDraftPassed) {
			fail_automated_run(aApplication, aRun);
			return;
		}
		aRun.PresentationPassed = true;
		transition_to_shutdown(aApplication, aRun);
	}

	// [[BPR-1345]]. One tick of the start-a-review course.
	void advance_start_review_course(
		bpr::app::github_application& aApplication,
		automated_run&                aRun)
	{
		const auto result = bpr::app::detail::github_application_fixture_access::exercise_start_review(
			aApplication, *aRun.StartReviewCourse);
		aRun.StartReviewDiagnostic = bpr::app::detail::github_application_fixture_access::start_review_diagnostic(aApplication);
		if (result == bpr::app::detail::start_review_probe_result::pending) {
			return;
		}
		aRun.StartReviewEvidence = bpr::app::detail::github_application_fixture_access::start_review_evidence(aApplication);
		aRun.StartReviewPassed   = result == bpr::app::detail::start_review_probe_result::passed;
		if (!aRun.StartReviewPassed) {
			fail_automated_run(aApplication, aRun);
			return;
		}
		aRun.PresentationPassed = true;
		transition_to_shutdown(aApplication, aRun);
	}

	// [[BPR-1331]]. One tick of the private progress course across a push.
	void advance_review_progress_push_probe(
		bpr::app::github_application& aApplication,
		automated_run&                aRun)
	{
		using access                      = bpr::app::detail::github_application_fixture_access;
		const auto state                  = access::exercise_review_progress_push(aApplication, *aRun.ReviewProgressPushProbe);
		aRun.ReviewProgressPushMarks      = access::review_progress_push_marks(aApplication);
		aRun.ReviewProgressPushDiagnostic = access::review_progress_push_diagnostic(aApplication);
		if (state == bpr::app::detail::review_progress_push_result::pending) {
			return;
		}
		aRun.ReviewProgressPushPassed = state == bpr::app::detail::review_progress_push_result::passed;
		if (!aRun.ReviewProgressPushPassed) {
			fail_automated_run(aApplication, aRun);
			return;
		}
		aRun.PresentationPassed = true;
		transition_to_shutdown(aApplication, aRun);
	}

	void advance_uiforetw_selected_diff_probe(
		bpr::app::github_application& aApplication,
		automated_run&                aRun)
	{
		const auto outcome = bpr::app::detail::github_application_fixture_access::
			exercise_uiforetw_selected_diff(aApplication);
		aRun.UIforEtwSelectedDiffEvidence = bpr::app::detail::
			github_application_fixture_access::uiforetw_selected_diff_evidence(
				aApplication);
		if (outcome
			== bpr::app::detail::uiforetw_selected_diff_probe_result::pending) {
			return;
		}
		aRun.UIforEtwSelectedDiffPassed = outcome
			== bpr::app::detail::uiforetw_selected_diff_probe_result::passed;
		if (!aRun.UIforEtwSelectedDiffPassed
			|| !aRun.UIforEtwSelectedDiffDirectory.has_value()
			|| !aRun.DiffEvidenceAppearance.has_value()) {
			fail_automated_run(aApplication, aRun);
			return;
		}
		aRun.UIforEtwSelectedDiffCaptured
			= bpr::app::detail::github_application_fixture_access::
				capture_uiforetw_selected_diff_evidence(
					aApplication,
					*aRun.UIforEtwSelectedDiffDirectory,
					*aRun.DiffEvidenceAppearance);
		aRun.PresentationPassed = aRun.UIforEtwSelectedDiffCaptured;
		if (aRun.PresentationPassed) {
			transition_to_shutdown(aApplication, aRun);
		} else {
			fail_automated_run(aApplication, aRun);
		}
	}
#endif

	void capture_automated_timeout_evidence(
		bpr::app::github_application& aApplication,
		automated_run&                aRun)
	{
		auto& window                 = aApplication.window();
		aRun.PresentationPassed      = false;
		aRun.FailedStage             = static_cast<int>(aRun.Stage);
		aRun.LastEpochStatus         = window.epoch_selectors()->status_text();
		aRun.LastEpochComparison     = window.epoch_selectors()->comparison_text();
		aRun.LastEpochLeft           = window.epoch_selectors()->left_oid_editor()->text();
		aRun.LastEpochRight          = window.epoch_selectors()->right_oid_editor()->text();
		aRun.LastEpochLeftRow        = window.epoch_selectors()->left_selector()->currentIndex();
		aRun.LastEpochRightRow       = window.epoch_selectors()->right_selector()->currentIndex();
		aRun.LastEpochCount          = window.epoch_selectors()->history_model()->epoch_count();
		aRun.LastEpochHistoryEnabled = window.epoch_selectors()->load_history_action()->isEnabled();
		aRun.LastEpochCoreStatus     = static_cast<int>(aApplication.status().EpochStatus);
#if defined(BPR_BUILD_TESTING)
		aRun.LastConversationRows       = window.conversation_model()->rowCount();
		aRun.LastConversationIdentities = fixture_conversation_identities(
			*window.conversation_model());
		aRun.LastConversationAnchor = window.conversation_panel()->anchor_line();
		aRun.LastConversationFileAnchored
			= window.conversation_panel()->anchor_file().has_value();
		aRun.LastConversationFileMatchesFixture
			= window.conversation_panel()->anchor_file()
			== std::optional{ bpr::core::file_identity_of("review.cpp") };
		if (aRun.Appearance.Enabled && !aRun.Appearance.StageEntered) {
			aRun.Appearance.SelectedDiffEvidence = bpr::app::detail::
				github_application_fixture_access::
					native_appearance_selected_diff_evidence(aApplication);
			if (aRun.Appearance.SelectedDiffResult
				== bpr::app::detail::
					native_appearance_selected_diff_probe_result::pending) {
				aRun.Appearance.PreconditionRefusal
					= native_appearance_precondition_refusal::
						selected_diff_budget_exhausted;
			} else if (aRun.Appearance.PreconditionRefusal
					   == native_appearance_precondition_refusal::none) {
				aRun.Appearance.PreconditionRefusal
					= native_appearance_precondition_refusal::
						automated_budget_exhausted;
			}
		}
		if (!aRun.Appearance.Enabled)
#endif
		{
			aRun.RenderSaved = save_render_evidence(
				window, aRun.RenderPath, aRun.RenderCollapsesFilesDrawer);
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
		*tick     = [&aApplication, aRun, tick,
					 alive = aApplication.liveness()] {
			// A tick still armed when `exec()` returns is delivered by the pump
			// inside `~implementation`, at which point reaching into the
			// application reads off a null pointer. BPR-0217.
			if (alive.expired()) {
				return;
			}
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
				// [[BPR-0797]]. Read once, here, because the composition
				// exists by now and this is the one stage every path passes
				// through. Which of the two GitHub CLI behaviours this run owes
				// follows from the factory it is actually holding.
				aRun->Cli.ReadOffered
					= aApplication.status().ComposedCliReadAvailable;
				if (aRun->CapabilityProbe) {
					// [[BPR-0797]]. The probe's whole question is answered by
					// the composition that now exists: which browser it bound
					// and whether the factory it holds can serve a CLI read.
					// It ends here, before any course connects, because every
					// stage past this one needs the platform vault or begins
					// a flow, and the probe is meant to run wherever the
					// composition can be built -- including a station whose
					// keychain is locked, where the CLI course cannot finish.
					// The report prints the identity either way; the verdict
					// here is that the composition stood up and shut down.
					aRun->PresentationPassed = true;
					transition_to_shutdown(aApplication, *aRun);
					break;
				}
#if defined(BPR_BUILD_TESTING)
				if (aRun->Fixture) {
					// RFC-BPR-0017. The signed-out courses take the fixture
					// without the account: connecting one would enroll a
					// credential and select an authenticated transport, which
					// is exactly what these courses claim did not happen. So
					// they activate the mode here and go straight to the
					// stage that reads its answer.
					if (aRun->PublicPreview.Course
						|| aRun->PublicPreview.Enterprise) {
						aRun->PublicPreview.Activated = true;
						// [[BPR-0771]]. Reading without an account is the Home bar's: pasting a
						// public link activates the signed-out mode and opens it in one gesture
						// (below). The enterprise refusal course opens nothing, so it still asks
						// for the mode directly.
						if (aRun->PublicPreview.Enterprise) {
							aApplication.activate_public_preview(true);
						}
						advance_fixture_stage(
							*aRun, automated_run::stage::connect);
						break;
					}
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
					// [[BPR-0536]], widened by [[BPR-0539]]. No course that
					// runs the recorded grant connects first, for the same
					// reason the signed-out courses above do not:
					// `github_connect_action` enrolls the fixture's own
					// credential, and that enrollment is a whole session -- it
					// takes the vault slot, consumes the recorded bootstrap
					// exchange, and leaves the device grant to arrive as a
					// second establishment that the strict recording cannot
					// serve and that the product must first delete its way out
					// of. The registered course counted that connect
					// credential as its one vault store and called it the
					// grant; the renewal course beside it had already stopped
					// connecting, so the two are one rule here rather than one
					// each way. The grant is the only session these courses
					// want, so they go straight to the stage that starts the
					// flow, and every store, read and probe the run then
					// observes belongs to the grant.
					if (aRun->Device.Registered) {
						advance_fixture_stage(
							*aRun, automated_run::stage::connect);
						break;
					}
					window.github_connect_action()->trigger();
					advance_fixture_stage(
						*aRun, automated_run::stage::connect);
				} else
#endif
				{
					// [[BPR-0772]], [[BPR-0982]]. One card per configured
					// server, and none of them a precomposed GitHub CLI row.
					const auto expectedCards = aRun->Cli.EnterpriseHostname.has_value() ? 2U : 1U;
					if (aRun->Cli.Enabled && window.server_cards().size() != expectedCards) {
						fail_automated_run(aApplication, *aRun);
						break;
					}
					aRun->Cli.Expected = 1U;
					aRun->Stage        = automated_run::stage::connect;
				}
				break;

			case automated_run::stage::connect:
				// Once the device path has been selected, this stage is
				// waiting for its typed terminal and nothing else. Checked
				// before the CLI presentation below, because that check counts
				// the profiles it passes and would count them again on every
				// tick spent waiting here.
				if (aRun->PublicPreview.Enabled
					&& aRun->PublicPreview.Activated) {
					// Read while the session is open. Shutdown closes it
					// before the markers are printed, so a run that never
					// recorded this could not tell an enabled selection from
					// an unavailable one. [[BPR-0316]].
					//
					// Recorded only once the typed answer has been published
					// and only when that answer was not a refusal. Activation
					// names the mode before the core has answered, so a poll
					// that read the mode alone would see the enterprise
					// course's refused activation as a signed-out session for
					// the tick between the two.
					if (aApplication.status().PublicPreviewPublications > 0U
						&& aApplication.status().PublicPreviewSignedOut
						&& !aApplication.status().PublicPreviewPolicyRefused
						&& !aApplication.status().PublicPreviewUnavailable) {
						aRun->PublicPreview.SignedOutObserved = true;
					}
#if defined(BPR_BUILD_TESTING)
					if (aRun->PublicPreview.LiveCandidate) {
						const auto live = aApplication.status();
						aRun->PublicPreview.ProductionFactories
							= live.PublicPreviewLiveCandidateProductionFactories;
						aRun->PublicPreview.WireRequests
							= live.PublicPreviewWireRequests;
						aRun->PublicPreview.WireCapabilityProbes
							= live.PublicPreviewWireCapabilityProbes;
						aRun->PublicPreview.WireCredentialedRefusals
							= live.PublicPreviewWireCredentialedRefusals;
						if (live.PublicPreviewBudgetObserved) {
							aRun->PublicPreview.BudgetObserved = true;
							aRun->PublicPreview.BudgetLimit
								= live.PublicPreviewBudgetLimit;
							aRun->PublicPreview.BudgetRemaining
								= live.PublicPreviewBudgetRemaining;
							aRun->PublicPreview.BudgetReset
								= live.PublicPreviewBudgetResetUnixSeconds;
						}
						if (live.PublicPreviewSnapshotPublicPreview) {
							aRun->PublicPreview.SnapshotSignedOut = true;
							aRun->PublicPreview.HeadOid
								= live.PublicPreviewSnapshotHeadOid;
						}
						// Read before anything is opened, so a composition
						// that reached for a credentialed transport, built an
						// authorization header, touched the vault, or was not
						// built from the production factories issues no
						// request at all. The run then ends on its own
						// publication wait and reports the refusal.
						const auto composed
							= live.PublicPreviewLiveCandidateProductionFactories
							&& live.PublicPreviewWireCredentialedRefusals == 0U
							&& live.PublicPreviewHeaderEffects == 0U
							&& live.PublicPreviewVaultEffects == 0U;
						if (composed && !aRun->PublicPreview.DryRun) {
							// The mode first, the pull request second, exactly
							// as the fixture course below: opening before the
							// session is signed out would read it on whatever
							// authority the host had.
							if (!aRun->PublicPreview.Opened) {
								if (!live.PublicPreviewSignedOut) {
									break;
								}
								aRun->PublicPreview.Opened = true;
								// [[BPR-0771]]. A pull request is named in the Home bar; the locator is retired.
								window.home_open_bar()->setText(QStringLiteral("%1/%2#%3").arg(QString::fromStdString(aRun->Expectations.Owner), QString::fromStdString(aRun->Expectations.Repository), QString::number(aRun->Expectations.Number)));
								window.home_open_action()->trigger();
								break;
							}
							if (live.PublicPreviewRequests == 0U
								|| !live.PublicPreviewSnapshotPublicPreview) {
								break;
							}
						}
					}
					if (aRun->PublicPreview.Course) {
						// The mode first, the pull request second. Opening
						// before the session is signed out would read it on
						// whatever authority the host had, which is the one
						// thing this course must not do.
						if (!aRun->PublicPreview.Opened) {
							// [[BPR-0771]]. Without the enterprise hook, the bar gesture below is
							// what activates the signed-out mode for a public link, so there is no
							// mode to wait for and nothing is read on an account's authority.
							if (aRun->PublicPreview.Enterprise
								&& !aApplication.status().PublicPreviewSignedOut) {
								break;
							}
							aRun->PublicPreview.Opened = true;
							// [[BPR-0771]]. Reading without an account is the Home bar's; the button is retired.
							const auto previewLink = aRun->PublicPreview.Enterprise
								? QStringLiteral("%1/%2#%3")
								: QStringLiteral("https://github.com/%1/%2/pull/%3");
							window.home_open_bar()->setText(previewLink.arg(QString::fromStdString(aRun->Expectations.Owner), QString::fromStdString(aRun->Expectations.Repository), QString::number(aRun->Expectations.Number)));
							window.home_open_action()->trigger();
							break;
						}
						if (aApplication.status().PublicPreviewRequests == 0U
							|| !aApplication.status()
									.PublicPreviewReviewCommentsObserved) {
							break;
						}
						// [[BPR-0715]]. Wait for the epoch to settle either
						// way; a failed epoch is reported by the verdict
						// below rather than by the run budget expiring.
						if (const auto epoch = aApplication.status().EpochStatus;
							epoch != bpr::core::epoch_status::ready
							&& epoch != bpr::core::epoch_status::unavailable
							&& epoch != bpr::core::epoch_status::failed
							&& epoch != bpr::core::epoch_status::cancelled) {
							break;
						} else {
							aRun->PublicPreview.SettledEpoch             = epoch;
							const auto* const line                       = window.epoch_selectors()->line();
							aRun->PublicPreview.SettledOccurrenceCount   = line->update_count();
							aRun->PublicPreview.SettledHistoryText       = line->history_text();
							aRun->PublicPreview.SettledHistoryIncomplete = line->history_text().startsWith(QStringLiteral("Incomplete history"));
							aRun->PublicPreview.SettledLineLabels.clear();
							for (std::size_t index = 0U; index < line->update_count(); ++index) {
								aRun->PublicPreview.SettledLineLabels.append(line->label_text(index));
							}
						}
						if (aRun->PublicPreview.MissingHistoricalBase) {
							const auto* line                               = window.epoch_selectors()->line();
							const auto  tooltip                            = line->node_tooltip(0);
							aRun->PublicPreview.MissingHistoricalBaseShown = tooltip.contains(QStringLiteral("Head 67c9afbd"))
								&& tooltip.contains(QStringLiteral("Historical base unknown"))
								&& tooltip.contains(QStringLiteral("Derived comparison base unavailable"))
								&& tooltip.contains(QStringLiteral("Comparison unavailable"));
						}
						if (aRun->PublicPreview.RewrittenBase) {
							auto* const                line = window.epoch_selectors()->line();
							std::optional<std::size_t> rewrittenNode;
							for (std::size_t index = 0U; index < line->update_count(); ++index) {
								const auto tooltip = line->node_tooltip(index);
								if (tooltip.contains(QStringLiteral("Head 6458d96b"))
									&& tooltip.contains(QStringLiteral("Historical base unknown"))
									&& tooltip.contains(QStringLiteral("Derived comparison base unavailable"))
									&& tooltip.contains(QStringLiteral("Comparison unavailable"))) {
									rewrittenNode = index;
								}
							}
							aRun->PublicPreview.RewrittenBaseGate = QStringLiteral("event 91702 unavailable tooltip and live selection");
							if (!rewrittenNode || !line->selection_offered()
								|| !line->left_stop() || !line->right_stop()
								|| line->left_stop()->Update == *rewrittenNode || line->right_stop()->Update == *rewrittenNode) {
								break;
							}
							aRun->PublicPreview.RewrittenBaseShown = true;
							if (!aRun->PublicPreview.RewrittenBaseGestureSent) {
								aRun->PublicPreview.RewrittenBaseGestureSent = true;
								aRun->PublicPreview.RewrittenBaseLeft        = line->left_stop();
								aRun->PublicPreview.RewrittenBaseRight       = line->right_stop();
								send_diff_evidence_click(*line, line->node_centre(*rewrittenNode));
								break;
							}
							aRun->PublicPreview.RewrittenBaseSelectionDenied = line->left_stop() == aRun->PublicPreview.RewrittenBaseLeft
								&& line->right_stop() == aRun->PublicPreview.RewrittenBaseRight;
							aRun->PublicPreview.RewrittenBaseGate = QStringLiteral("event 91702 click leaves live handles unchanged");
						}
						// A retained, selectable historical head: its own base is unknown and
						// its comparison is derived against the live base.
						const auto retainedHistorical = [](const QString& aTooltip) {
							return aTooltip.contains(QStringLiteral("Historical base unknown"))
								&& aTooltip.contains(QStringLiteral("Derived comparison base "))
								&& !aTooltip.contains(QStringLiteral("Comparison unavailable"));
						};
						if (aRun->PublicPreview.HistoricalCourse) {
							auto* const                line = window.epoch_selectors()->line();
							std::optional<std::size_t> historicalNode;
							for (std::size_t index = 0U; index < line->update_count(); ++index) {
								const auto tooltip = line->node_tooltip(index);
								// The first displayed occurrence can itself be a retained,
								// selectable historical head.
								if (retainedHistorical(tooltip)) {
									historicalNode = index;
									break;
								}
							}
							if (!historicalNode || !line->selection_offered()) {
								break;
							}
							if (!aRun->PublicPreview.HistoricalGestureSent) {
								aRun->PublicPreview.HistoricalGestureSent = true;
								aRun->PublicPreview.HistoricalNode        = *historicalNode;
								send_diff_evidence_click(*line, line->node_centre(*historicalNode));
								break;
							}
							const auto left = line->left_stop();
							if (!left || left->Update != aRun->PublicPreview.HistoricalNode
								|| left->Kind != bpr::core::comparison_anchor_kind::epoch_head
								|| aApplication.status().EpochStatus != bpr::core::epoch_status::ready) {
								break;
							}
							aRun->PublicPreview.HistoricalHandleMoved = true;
						}
						// [[BPR-0829]]. The live epoch's commit starts background retention of
						// the timeline's force-push after-heads: one import at a time, each on
						// its own smart-Git lease, the next submitted as the last completes.
						// The writes below end the run and shutdown cancels the import in
						// flight, so offering them on the tick the epoch settled let the lease
						// count read 1, 2 or 3 by how far the chain had got. Waiting until
						// every occurrence is retained makes the count a fact.
						if (aRun->PublicPreview.awaits_historical_retention()) {
							const auto* const line     = window.epoch_selectors()->line();
							auto              events   = std::size_t{ 0U };
							auto              retained = std::size_t{ 0U };
							for (std::size_t index = 0U; index < line->update_count(); ++index) {
								const auto tooltip = line->node_tooltip(index);
								// Retained means the occurrence has a local epoch, which is
								// exactly when the line omits "Comparison unavailable"; its
								// historical base may be known or unknown either way.
								if (tooltip.contains(QStringLiteral("Event time "))) {
									++events;
									retained += tooltip.contains(QStringLiteral("Comparison unavailable")) ? 0U : 1U;
								}
							}
							aRun->PublicPreview.HistoricalRetained = events > 0U && retained == events;
							if (!aRun->PublicPreview.HistoricalRetained) {
								break;
							}
						}
						// [[BPR-1340]]. Each handle is dragged, through the window, to
						// another update, then the right handle steps by key; the run
						// waits each time until the comparison the core publishes back
						// puts that handle there and is ready again. The course starts
						// from the whole-PR pair, whose two handles share the newest
						// node, which is where the owner could not take the left one.
						if (aRun->PublicPreview.DragCourse && !aRun->PublicPreview.DragKeyMoved) {
							using bpr::core::comparison_anchor_kind;
							using bpr::qt::epoch_line_stop;
							auto* const line  = window.epoch_selectors()->line();
							const auto  ready = aApplication.status().EpochStatus == bpr::core::epoch_status::ready;
							const auto  left  = line->left_stop();
							const auto  right = line->right_stop();
							const auto  named = [](const std::optional<epoch_line_stop>& aStop) {
								return aStop ? QStringLiteral("%1%2").arg(aStop->Kind == comparison_anchor_kind::epoch_base ? QStringLiteral("base") : QStringLiteral("head")).arg(aStop->Update) : QStringLiteral("none");
							};
							aRun->PublicPreview.DragGate = QStringLiteral("stage %1 visible %2 offered %3 ready %4 left %5 right %6 target %7")
															   .arg(aRun->PublicPreview.DragStage)
															   .arg(line->isVisible() ? 1 : 0)
															   .arg(line->selection_offered() ? 1 : 0)
															   .arg(ready ? 1 : 0)
															   .arg(named(left), named(right), named(aRun->PublicPreview.DragTarget));
							// The Updates line opens as the reviewer opens it, from the
							// review bar's disclosure, once.
							if (!line->isVisible() && !aRun->PublicPreview.DragLineOpened) {
								if (auto* const disclosure = window.findChild<QToolButton*>(QStringLiteral("reviewBarUpdates")); disclosure != nullptr) {
									aRun->PublicPreview.DragLineOpened = true;
									disclosure->click();
								}
								break;
							}
							if (!ready || !line->isVisible() || !line->selection_offered() || !left || !right || line->update_count() < 3U) {
								break;
							}
							const auto drag = [&](const epoch_line_stop& aFrom, const epoch_line_stop& aTo) {
								aRun->PublicPreview.DragTarget = aTo;
								aRun->PublicPreview.DragSteps.append(QStringLiteral("%1->%2").arg(named(aFrom), named(aTo)));
								send_window_drag(*line, line->stop_position(aFrom), line->stop_position(aTo));
								++aRun->PublicPreview.DragStage;
							};
							switch (aRun->PublicPreview.DragStage) {
							case 0:
								if (left->Update != right->Update) {
									aRun->PublicPreview.DragGate.append(QStringLiteral(" (expected the whole-PR pair on one node)"));
									break;
								}
								// The left handle, off the node it shares with the right one,
								// to the oldest update's head.
								drag(*left, { .Update = 0U, .Kind = comparison_anchor_kind::epoch_head });
								break;
							case 1:
								if (left != aRun->PublicPreview.DragTarget) {
									break;
								}
								aRun->PublicPreview.DragLeftMoved = true;
								// The right handle, from the newest head to the update before it.
								drag(*right, { .Update = right->Update - 1U, .Kind = comparison_anchor_kind::epoch_head });
								break;
							case 2: {
								if (right != aRun->PublicPreview.DragTarget) {
									break;
								}
								aRun->PublicPreview.DragRightMoved = true;
								// The keyboard alternative: Shift+Right steps the right handle
								// back to the newest head, to the line that has focus.
								aRun->PublicPreview.DragTarget = epoch_line_stop{ .Update = right->Update + 1U, .Kind = comparison_anchor_kind::epoch_head };
								aRun->PublicPreview.DragSteps.append(QStringLiteral("Shift+Right %1->%2").arg(named(right), named(aRun->PublicPreview.DragTarget)));
								auto key = QKeyEvent{ QEvent::KeyPress, Qt::Key_Right, Qt::ShiftModifier };
								// The press that started the drag gave the line focus; the
								// window hands the key to its focus widget.
								if (auto* const handle = line->window()->windowHandle(); handle != nullptr && line->window()->focusWidget() == line) {
									QCoreApplication::sendEvent(handle, &key);
								}
								++aRun->PublicPreview.DragStage;
								break;
							}
							default:
								if (right == aRun->PublicPreview.DragTarget) {
									aRun->PublicPreview.DragKeyMoved = true;
								}
								break;
							}
							if (!aRun->PublicPreview.DragKeyMoved) {
								break;
							}
						}
						// Offered once the read is in, so the refusals are
						// answered by a live signed-out session rather than by
						// an absent one. Nothing here can send: the marker
						// beside it still reports one anonymous read and no
						// header, which is what makes "refused before a
						// request" a reading of the evidence rather than a
						// claim about it.
						if (!aRun->PublicPreview.WritesOffered) {
							aRun->PublicPreview.WritesOffered = true;
							aApplication.attempt_public_preview_writes();
						}
					}
#endif
					// The refusal is posted to the decision lane, and the
					// presentation it produces is the answer this probe
					// reports; shutdown closes that lane, so the run waits for
					// the publication rather than racing it.
					if (aApplication.status().PublicPreviewPublications == 0U) {
						break;
					}
					aRun->PresentationPassed
						= aRun->Cli.Observed == aRun->Cli.Expected;
					aRun->RenderSaved = save_render_evidence(
						window,
						aRun->RenderPath,
						aRun->RenderCollapsesFilesDrawer);
					transition_to_shutdown(aApplication, *aRun);
					break;
				}
				if (aRun->Device.Enabled && aRun->Device.Selected) {
					if (!bpr::core::terminal(
							aApplication.status()
								.DeviceAuthorizationStage)) {
						// One "Open browser" request, at the only stage that
						// has an address to open. Made once rather than every
						// tick: the claim is that the request reaches the
						// composed capability, not that it can be repeated.
						// A tests-enabled graph composes the recording opener,
						// so nothing starts.
						if (aRun->Device.BrowserProbe
							&& aApplication.status().DeviceAuthorizationStage
								== bpr::core::device_authorization_stage::
									awaiting_authorization
							&& !aRun->Device.OpenDispatched) {
							aRun->Device.OpenDispatched = true;
							aApplication.open_device_verification_address();
						}
						// A registered platform's real flow parks in
						// `awaiting_authorization` until a human answers,
						// which the probe neither needs nor has budget for:
						// one cancel carries it to a terminal the shutdown
						// wait accepts. The fixture's registered course is
						// exempt — its recorded grant authorizes on its own
						// and that terminal is the course's whole claim.
						if (aApplication.status().DeviceAuthorizationStage
								== bpr::core::device_authorization_stage::
									awaiting_authorization
							&& !aRun->Device.Registered
							&& !aRun->Device.CancelDispatched) {
							aRun->Device.CancelDispatched = true;
							aApplication.cancel_device_authorization();
						}
						break;
					}
					// The registered course enrolls through the real vault,
					// which is an off-lane worker. Its store lands after the
					// stage is terminal, so the run waits for the count rather
					// than reading it too early and calling zero a failure.
					if (aRun->Device.Registered
						&& aApplication.status().DeviceAuthorizationVaultStores
							== 0U) {
						break;
					}
					// [[BPR-0536]]. The renewal is armed on a real timer off
					// this run's lane, so the course waits for it by the count
					// exactly as the store above is waited for, and waits past
					// `refreshing` so the state it reports is a settled one
					// rather than whichever instant the poll happened to land
					// on.
					//
					// [[BPR-0544]]. Waiting for the rotation is not waiting for
					// what the rotation is claimed to have done. The rotation
					// counter and `scheduled` are published on this lane the
					// moment the exchange settles, while the new access token
					// reaches the vault through an off-lane worker, and on the
					// JaegAW Windows station that store lands about 138 ms
					// later -- fourteen 10 ms polls after the run had already
					// decided it was finished. So the run read
					// `DEVICE_FLOW_VAULT_STORES:1` and failed the row, except
					// on the first run after a relink, when the cold image was
					// slow enough for the store to win the race. The count is
					// waited for here for the same reason the registered
					// course waits for its first store, which makes the row
					// deterministic without raising the budget: the bound was
					// never the cause, and a 60,000 ms budget produced the same
					// failure in the same 6.6 s.
					//
					// `reenrollment_required` is the typed refusal that ends
					// the wait when the store cannot land at all -- a vault
					// that answered a failure takes the host down that path --
					// so a broken store is still reported rather than waited
					// out to the budget.
					//
					// [[BPR-0547]]. Two rotations, not one. One rotation succeeds
					// whether or not it keeps the material it just stored, because
					// nothing has had to spend that material yet; the renewal after
					// it is the one that reads the slot the first rotation wrote.
					// So the run waits for the second rotation and for the second
					// rotation's own access-token store to land, by the same count
					// and for the same off-lane reason as the first.
					//
					// `reenrollment_required` ends the wait wherever it is reached,
					// not only at the store: a chain that broke after the first
					// rotation reports the count it got to and fails the row on it,
					// rather than being waited out to the budget and failing as a
					// timeout that names nothing.
					if (aRun->Device.RefreshCourse) {
						const auto refreshStatus = aApplication.status();
						const auto refreshBroken
							= refreshStatus.DeviceAuthorizationRefreshState
							== bpr::application::
								device_authorization_refresh_state::
									reenrollment_required;
						const auto refreshSettled
							= refreshStatus.DeviceAuthorizationRefreshRotations >= 2U
							&& refreshStatus.DeviceAuthorizationRefreshState
								!= bpr::application::
									device_authorization_refresh_state::refreshing
							&& refreshStatus.DeviceAuthorizationVaultStores >= 3U;
						if (!refreshBroken && !refreshSettled) {
							break;
						}
					}
					aRun->PresentationPassed
						= aRun->Cli.Observed == aRun->Cli.Expected;
					aRun->RenderSaved
						= save_render_evidence(
							window, aRun->RenderPath, aRun->RenderCollapsesFilesDrawer);
					transition_to_shutdown(aApplication, *aRun);
					break;
				}
				// The registered course is about the grant reaching the vault,
				// not about the pull-request course beside it, so it selects
				// the device path here and ends the run at its terminal rather
				// than continuing into the fixture's own stages.
				if (aRun->Device.Enabled && aRun->Device.Registered) {
					aRun->Device.Selected = true;
					// The one difference between this course and the panel
					// course beside it: which control starts the flow. Every
					// stage, terminal, vault store and enrollment below is the
					// same, so a difference in the evidence is a difference in
					// the mount rather than in the flow.
					if (aRun->Device.PanelCourse) {
						aRun->Device.PanelBegan
							= begin_through_device_panel(window);
					} else {
						aApplication.select_device_authorization();
					}
					break;
				}

#if defined(BPR_BUILD_TESTING)
				// [[BPR-0540]]. The orphan course's session is the connect
				// above -- the reviewer's pasted credential -- and its claim
				// is about what the host did *not* do with the material
				// already sitting beside it. The acceptance starts the host's
				// one-time refresh probe, and the fixture's retrieve count is
				// that probe's own record of having asked, so the run waits
				// for the question before reporting the answer. It then keeps
				// ticking, because the answer still has two lane hops and a
				// republication to travel before composition could resume: a
				// course that stopped at the retrieve would report zero
				// resumes for a build that resumes.
				if (aRun->Device.Enabled && aRun->Device.OrphanCourse) {
					if (window.github_status_text()
						!= "GitHub account connected.") {
						break;
					}
					const auto orphanEvidence = bpr::app::detail::
						github_application_fixture_access::evidence(
							aApplication);
					if (orphanEvidence.RefreshRetrieves == 0U) {
						break;
					}
					if (aRun->Device.OrphanSettleTicks
						< automated_run::orphan_course_settle_ticks) {
						++aRun->Device.OrphanSettleTicks;
						break;
					}
					aRun->PresentationPassed
						= aRun->Cli.Observed == aRun->Cli.Expected;
					aRun->RenderSaved = save_render_evidence(
						window,
						aRun->RenderPath,
						aRun->RenderCollapsesFilesDrawer);
					transition_to_shutdown(aApplication, *aRun);
					break;
				}
				if (aRun->Fixture) {
					if (window.github_status_text() != "GitHub account connected."
						|| !window.github_open_action()->isEnabled()) {
						break;
					}
					// [[BPR-0771]]. A pull request is named in the Home bar; the locator is retired.
					window.home_open_bar()->setText(QStringLiteral("%1/%2#%3").arg(QString::fromStdString(aRun->Expectations.Owner), QString::fromStdString(aRun->Expectations.Repository), QString::number(aRun->Expectations.Number)));
					window.home_open_action()->trigger();
					advance_fixture_stage(*aRun, automated_run::stage::open);
				} else if (aRun->Cli.Observed < aRun->Cli.Expected)
#else
				if (aRun->Cli.Observed < aRun->Cli.Expected)
#endif
				{
					// [[BPR-0982]]. Observed once, then the run moves on to the
					// course it came for; a failed observation is left
					// uncounted and the verdict says so.
					if (cli_entry_presentation_passed(window, *aRun)) {
						++aRun->Cli.Observed;
						aRun->Cli.Cards = window.server_cards().size();
					}
					if (aRun->Cli.Observed < aRun->Cli.Expected) {
						fail_automated_run(aApplication, *aRun);
					} else if (aRun->PublicPreview.Enabled) {
						// Activate once. The typed answer is posted to the
						// decision lane and has to be allowed to land before
						// the run may transition to shutdown; the wait is at
						// the top of this stage.
						aRun->PublicPreview.Activated = true;
						aApplication.activate_public_preview(
							aRun->PublicPreview.Enterprise);
					} else if (aRun->Device.Enabled && aRun->Device.GhesCourse) {
						// [[BPR-0535]]. The same "select once, then wait for
						// the typed answer" as below, begun the way a reviewer
						// looking at an enterprise host begins it: through the
						// picker and the mounted panel rather than through
						// `select_device_authorization`, which always names
						// the front profile and so could never reach this
						// case.
						aRun->Device.GhesBegan
							= begin_through_ghes_device_panel(window);
						aRun->Device.Selected = aRun->Device.GhesBegan;
						if (!aRun->Device.GhesBegan) {
							// Nothing was pressed, so no answer is coming.
							// Report the false marker now rather than
							// spending the budget waiting for a terminal that
							// cannot arrive.
							aRun->PresentationPassed
								= aRun->Cli.Observed == aRun->Cli.Expected;
							aRun->RenderSaved = save_render_evidence(
								window,
								aRun->RenderPath,
								aRun->RenderCollapsesFilesDrawer);
							transition_to_shutdown(aApplication, *aRun);
						}
					} else if (aRun->Device.Enabled) {
						// Select once. The refusal is posted to the decision
						// lane and has to be allowed to land before the run may
						// transition to shutdown, which closes that lane; the
						// wait is at the top of this stage.
						aRun->Device.Selected = true;
						aApplication.select_device_authorization();
					} else {
						aRun->PresentationPassed = aRun->Cli.Observed == aRun->Cli.Expected;
						aRun->RenderSaved        = save_render_evidence(
							window, aRun->RenderPath, aRun->RenderCollapsesFilesDrawer);
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
				if (aRun->WorkbenchCompositionProbe
					&& aRun->WorkbenchCompositionStarted) {
					advance_workbench_composition_probe(aApplication, *aRun);
					break;
				}
				if (aRun->UIforEtwSelectedDiffProbe
					&& aRun->UIforEtwSelectedDiffStarted) {
					advance_uiforetw_selected_diff_probe(aApplication, *aRun);
					break;
				}
				// [[BPR-1083]]. The course opens its own file in Review, so it
				// starts as soon as the pull request is being read rather than
				// after a presentation check that needs a file selected first.
				if (aRun->LabelAuthoringProbe) {
					advance_label_authoring_probe(aApplication, *aRun);
					break;
				}
				if (aRun->PublishedFileSwitchProbe) {
					advance_published_file_switch_probe(aApplication, *aRun);
					break;
				}
				if (aRun->StartReviewCourse.has_value()) {
					advance_start_review_course(aApplication, *aRun);
					break;
				}
				if (aRun->ReplyDraftCourse.has_value()) {
					advance_reply_draft_course(aApplication, *aRun);
					break;
				}
				if (aRun->ReviewProgressPushProbe.has_value()) {
					advance_review_progress_push_probe(aApplication, *aRun);
					break;
				}
				if (aRun->ThreadedReplyProbe && aRun->ThreadedReplyStarted
					&& !aRun->ThreadedReplyPassed) {
					const auto result = bpr::app::detail::github_application_fixture_access::
						exercise_threaded_reply(aApplication);
					aRun->ThreadedReplyDiagnostic = bpr::app::detail::
						github_application_fixture_access::threaded_reply_diagnostic(aApplication);
					if (result == bpr::app::detail::threaded_reply_probe_result::pending) {
						break;
					}
					aRun->ThreadedReplyEvidence = bpr::app::detail::
						github_application_fixture_access::threaded_reply_evidence(aApplication);
					aRun->ThreadedReplyPassed = result == bpr::app::detail::threaded_reply_probe_result::passed;
					if (!aRun->ThreadedReplyPassed) {
						fail_automated_run(aApplication, *aRun);
						break;
					}
					aRun->PresentationPassed = true;
					transition_to_shutdown(aApplication, *aRun);
					break;
				}
				if (aRun->ReviewRoundTripCompositionProbe
					&& aRun->ReviewRoundTripCompositionStarted
					&& !aRun->ReviewRoundTripCompositionPassed) {
					const auto result = bpr::app::detail::
						github_application_fixture_access::
							exercise_review_round_trip_composition(aApplication);
					if (result
						== bpr::app::detail::
							review_round_trip_composition_probe_result::pending) {
						break;
					}
					aRun->ReviewRoundTripCompositionEvidence
						= bpr::app::detail::github_application_fixture_access::
							review_round_trip_composition_evidence(aApplication);
					aRun->ReviewRoundTripCompositionPassed
						= result == bpr::app::detail::review_round_trip_composition_probe_result::passed;
					if (!aRun->ReviewRoundTripCompositionPassed) {
						fail_automated_run(aApplication, *aRun);
						break;
					}
					aRun->PresentationPassed = true;
					transition_to_shutdown(aApplication, *aRun);
					break;
				}
				if (aRun->DefectCompositionProbe.has_value()
					&& aRun->DefectCompositionStarted
					&& !aRun->DefectCompositionPassed) {
					const auto result = bpr::app::detail::
						github_application_fixture_access::exercise_defect_composition(
							aApplication, *aRun->DefectCompositionProbe);
					if (result
						== bpr::app::detail::defect_composition_probe_result::pending) {
						break;
					}
					aRun->DefectCompositionEvidence = bpr::app::detail::
						github_application_fixture_access::defect_composition_evidence(
							aApplication);
					aRun->DefectCompositionPassed = result
						== bpr::app::detail::defect_composition_probe_result::passed;
					if (!aRun->DefectCompositionPassed) {
						fail_automated_run(aApplication, *aRun);
						break;
					}
					aRun->PresentationPassed = true;
					transition_to_shutdown(aApplication, *aRun);
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
						window,
						aRun->ExpectedActiveHostText,
						aRun->Expectations,
						window.comparison_surface()->current_context().has_value())) {
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
						// [[BPR-0725]]. The core now loads the newest page as
						// soon as the revision settles, so a restarted run can
						// already hold both epochs here and skip the
						// Return-to-newest hop. A probe step that returns true
						// without leaving `open` (the pruned run's diff request)
						// must continue in the history stage, which polls the
						// probe alone, rather than be re-gated on this stage's
						// unfocused presentation once the diff is displayed.
						if (begin_epoch_maintenance_probe(aApplication, *aRun)
							&& aRun->Stage == automated_run::stage::open) {
							advance_fixture_stage(
								*aRun, automated_run::stage::epoch_maintenance_history);
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
					if (aRun->ReviewBehaviourRequested
						&& aRun->EpochPhase
							== bpr::app::detail::github_epoch_fixture_phase::seed
						&& !aRun->StartReviewAttempted) {
						// [[BPR-0919]]. Exercise the natural first-visit projection before
						// the ledger probe below creates a receipt and turns it into Continue.
						window.choose_modality(bpr::qt::review_modality::overview);
						QApplication::processEvents();
						auto* const overview = window.overview_page();
						if (overview == nullptr || !overview->presented()
							|| overview->projection()->Entry
								!= bpr::application::overview_entry_kind::start_review
							|| !overview->projection()->EntryPath.has_value()
							|| !overview->entry_button()->isEnabled()) {
							break;
						}
						const auto before = bpr::app::detail::
							github_application_fixture_access::review_mutation_evidence(
								aApplication);
						overview->entry_button()->click();
						QApplication::processEvents();
						const auto after = bpr::app::detail::
							github_application_fixture_access::review_mutation_evidence(
								aApplication);
						aRun->StartReviewAttempted     = true;
						aRun->StartReviewEnteredReview = window.current_modality()
							== bpr::qt::review_modality::review;
						aRun->StartReviewPublishedNothing
							= before.Submissions == after.Submissions
							&& before.StoredComments == after.StoredComments
							&& before.Replies == after.Replies
							&& before.Resolutions == after.Resolutions;
						if (!aRun->StartReviewEnteredReview
							|| !aRun->StartReviewPublishedNothing) {
							fail_automated_run(aApplication, *aRun);
						}
						break;
					}
					if (aRun->ReviewLedgerCompositionProbe.has_value()
						&& !aRun->ReviewLedgerCompositionPassed) {
						const auto result = bpr::app::detail::
							github_application_fixture_access::
								exercise_review_ledger_composition(
									aApplication,
									*aRun->ReviewLedgerCompositionProbe);
						if (result
							== bpr::app::detail::
								review_ledger_composition_probe_result::pending) {
							break;
						}
						aRun->ReviewLedgerCompositionEvidence
							= bpr::app::detail::github_application_fixture_access::
								review_ledger_composition_evidence(aApplication);
						aRun->ReviewLedgerCompositionPassed
							= result == bpr::app::detail::review_ledger_composition_probe_result::passed;
						aRun->LedgerPresentOnEntry
							= aRun->ReviewLedgerCompositionEvidence.ReceiptLoadedOnEntry;
						aRun->LedgerRoundTripped
							= aRun->ReviewLedgerCompositionEvidence.DurableReceipt;
						if (!aRun->ReviewLedgerCompositionPassed) {
							fail_automated_run(aApplication, *aRun);
							break;
						}
						aRun->PresentationPassed = true;
						transition_to_shutdown(aApplication, *aRun);
						break;
					}
					if (aRun->ThreadedReplyProbe && !aRun->ThreadedReplyPassed) {
						aRun->ThreadedReplyStarted = true;
						const auto result          = bpr::app::detail::
							github_application_fixture_access::exercise_threaded_reply(aApplication);
						aRun->ThreadedReplyDiagnostic = bpr::app::detail::
							github_application_fixture_access::threaded_reply_diagnostic(aApplication);
						if (result == bpr::app::detail::threaded_reply_probe_result::pending) {
							break;
						}
						aRun->ThreadedReplyEvidence = bpr::app::detail::
							github_application_fixture_access::threaded_reply_evidence(aApplication);
						aRun->ThreadedReplyPassed = result == bpr::app::detail::threaded_reply_probe_result::passed;
						if (!aRun->ThreadedReplyPassed) {
							fail_automated_run(aApplication, *aRun);
							break;
						}
						aRun->PresentationPassed = true;
						transition_to_shutdown(aApplication, *aRun);
						break;
					}
					if (aRun->ReviewRoundTripCompositionProbe
						&& !aRun->ReviewRoundTripCompositionPassed) {
						aRun->ReviewRoundTripCompositionStarted = true;
						const auto result                       = bpr::app::detail::
							github_application_fixture_access::
								exercise_review_round_trip_composition(aApplication);
						if (result == bpr::app::detail::review_round_trip_composition_probe_result::pending) {
							break;
						}
						aRun->ReviewRoundTripCompositionEvidence
							= bpr::app::detail::github_application_fixture_access::
								review_round_trip_composition_evidence(aApplication);
						aRun->ReviewRoundTripCompositionPassed
							= result == bpr::app::detail::review_round_trip_composition_probe_result::passed;
						if (!aRun->ReviewRoundTripCompositionPassed) {
							fail_automated_run(aApplication, *aRun);
							break;
						}
						aRun->PresentationPassed = true;
						transition_to_shutdown(aApplication, *aRun);
						break;
					}
					if (aRun->DefectCompositionProbe.has_value()
						&& !aRun->DefectCompositionPassed) {
						aRun->DefectCompositionStarted = true;
						const auto result              = bpr::app::detail::
							github_application_fixture_access::exercise_defect_composition(
								aApplication, *aRun->DefectCompositionProbe);
						if (result
							== bpr::app::detail::defect_composition_probe_result::pending) {
							break;
						}
						aRun->DefectCompositionEvidence = bpr::app::detail::
							github_application_fixture_access::defect_composition_evidence(
								aApplication);
						aRun->DefectCompositionPassed = result
							== bpr::app::detail::defect_composition_probe_result::passed;
						if (!aRun->DefectCompositionPassed) {
							fail_automated_run(aApplication, *aRun);
							break;
						}
						aRun->PresentationPassed = true;
						transition_to_shutdown(aApplication, *aRun);
						break;
					}
					if (!aRun->WorkbenchCompositionProbe
						&& !aRun->UIforEtwSelectedDiffProbe
						&& !aRun->Appearance.Enabled
						&& !aRun->LedgerRoundTripped) {
						aRun->LedgerRoundTripped = bpr::app::detail::
							github_application_fixture_access::
								review_ledger_round_trip(aApplication);
						if (!aRun->LedgerRoundTripped) {
							break;
						}
						aRun->LedgerPresentOnEntry = bpr::app::detail::
							github_application_fixture_access::
								review_ledger_receipt_present(aApplication);
					}
					if (aRun->EpochPhase
						== bpr::app::detail::github_epoch_fixture_phase::seed) {
						window.github_refresh_action()->trigger();
						advance_fixture_stage(
							*aRun, automated_run::stage::epoch_refresh);
					} else if (persistentPhase) {
						auto* const control = window.epoch_selectors();
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
					} else if (aRun->WorkbenchCompositionProbe) {
						aRun->WorkbenchCompositionStarted = true;
						advance_workbench_composition_probe(aApplication, *aRun);
					} else if (aRun->UIforEtwSelectedDiffProbe) {
						if (!bpr::app::detail::github_application_fixture_access::
								apply_appearance_gesture(
									aApplication,
									*aRun->DiffEvidenceAppearance,
									8U)) {
							fail_automated_run(aApplication, *aRun);
							break;
						}
						aRun->UIforEtwSelectedDiffStarted = true;
					} else if (aRun->Appearance.Enabled) {
						aRun->Appearance.ExpectedEpochLeftOid  = QString::fromStdString(evidence.BaseOid);
						aRun->Appearance.ExpectedEpochRightOid = QString::fromStdString(evidence.HeadOneOid);
						aRun->Appearance.SelectedDiffResult
							= bpr::app::detail::github_application_fixture_access::
								exercise_native_appearance_selected_diff(aApplication);
						aRun->Appearance.SelectedDiffEvidence
							= bpr::app::detail::github_application_fixture_access::
								native_appearance_selected_diff_evidence(aApplication);
						if (aRun->Appearance.SelectedDiffResult
							== bpr::app::detail::
								native_appearance_selected_diff_probe_result::pending) {
							break;
						}
						if (aRun->Appearance.SelectedDiffResult
							== bpr::app::detail::
								native_appearance_selected_diff_probe_result::refused) {
							aRun->Appearance.PreconditionRefusal
								= native_appearance_precondition_refusal::
									selected_diff_refused;
							fail_automated_run(aApplication, *aRun);
							break;
						}
						const auto waitingForOwnerFocus = QGuiApplication::platformName() == "cocoa"
							&& (!window.isActiveWindow()
								|| QApplication::focusWidget() == nullptr);
						if (waitingForOwnerFocus) {
							aRun->Appearance.PreconditionRefusal
								= native_appearance_precondition_refusal::
									owner_focus_required;
							if (!aRun->Appearance.WaitingForOwnerFocusReported) {
								QTextStream stream{ stdout };
								stream
									<< "NATIVE_APPEARANCE_WAITING_FOR_OWNER_FOCUS\n";
								stream.flush();
								aRun->Appearance.WaitingForOwnerFocusReported = true;
							}
							break;
						}
						aRun->Appearance.PreconditionRefusal
							= prepare_native_appearance_probe(window, *aRun);
						if (aRun->Appearance.PreconditionRefusal
							== native_appearance_precondition_refusal::none) {
							aRun->Appearance.StageEntered = true;
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
						// [[BPR-0823]]. Since [[BPR-0771]] the course names its
						// pull request in the Home bar, and a pull request opened
						// that way lands on Overview, as RFC-BPR-0028 and
						// [[BPR-0661]]'s default opening modality say it does.
						// The centre then shows the overview page, so the
						// comparison this probe loads, measures and captures is
						// hidden. A reviewer reaches it by leaving Overview, so
						// the course does that first: Review, the modality the
						// window showed on this path before the Home bar
						// carried it, entered through the step every course
						// shares ([[BPR-0863]]).
						const auto modality = bpr::app::detail::
							github_application_fixture_access::enter_course_modality(
								aApplication, aRun->DiffModality);
						if (modality
							== bpr::app::detail::course_modality_result::pending) {
							break;
						}
						if (modality
							== bpr::app::detail::course_modality_result::failed) {
							fail_automated_run(aApplication, *aRun);
							break;
						}
						if (!aRun->DiffRequested) {
							aRun->DiffRequested = bpr::app::detail::
								github_application_fixture_access::request_fixture_diff(
									aApplication);
							if (!aRun->DiffRequested) {
								fail_automated_run(aApplication, *aRun);
							}
							break;
						}
						auto* const workbench = window.comparison_surface();
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
							&& line_details_show_selection(*workbench);
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
						if (aRun->ThemeCustomizationPath.has_value()
							&& !aRun->ThemeCustomizationPassed) {
							const auto outcome = bpr::app::detail::
								github_application_fixture_access::
									exercise_appearance_theme_customization(
										aApplication,
										*aRun->ThemeCustomizationPath);
							if (outcome == bpr::app::detail::appearance_theme_customization_probe_result::pending) {
								break;
							}
							if (outcome == bpr::app::detail::appearance_theme_customization_probe_result::failed) {
								fail_automated_run(aApplication, *aRun);
								break;
							}
							aRun->ThemeCustomizationPassed = true;
						}
						window.resize(1'280, 1'000);
						QApplication::processEvents();
						// The same assertion the appearance probe makes, in the
						// surface that writes the captures. A capture of a
						// clipped comparison is evidence of the wrong thing,
						// and the code that used to sit here could not notice:
						// it was a `qobject_cast` in an `if`, so a shell whose
						// structure had changed simply skipped the call.
						//
						// Fails the run rather than waiting, because a structural
						// fault does not resolve by retrying. The failure moves
						// the run to shutdown, so the tick must still be rearmed
						// for shutdown to finish: returning here left the process
						// running until the harness timeout, which reported this
						// failure as a timeout ([[BPR-0823]]).
						if (!comparison_is_wholly_visible(window, *workbench)) {
							// [[BPR-0863]]. Named with the modality and what it
							// withholds, so a course in the wrong one says so.
							QTextStream{ stdout }
								<< "DIFF_EVIDENCE_COMPARISON_VISIBLE_FAILED:"
								<< bpr::app::detail::github_application_fixture_access::
									   course_modality_facts(aApplication)
								<< '\n';
							fail_automated_run(aApplication, *aRun);
							break;
						}
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
						if (aRun->DiffInteractionEvidencePassed
							&& aRun->Expectations.ExpectedThreadId.has_value()) {
							const auto thread = window.conversation_model()->index_for_id(
								*aRun->Expectations.ExpectedThreadId);
							window.conversation_view()->setCurrentIndex(thread);
							QApplication::processEvents();
							const auto& anchor                     = workbench->conversation_anchor();
							aRun->ConversationAnchorEvidencePassed = thread.isValid()
								&& anchor.has_value()
								&& anchor->Side == bpr::core::diff_side::right
								&& anchor->Line == 41U
								&& window.conversation_model()->rowCount() == 1
								&& window.conversation_model()
									   ->index_for_id(*aRun->Expectations.ExpectedThreadId)
									   .isValid()
								&& !workbench
										->conversation_anchor_marker_rect(
											bpr::core::diff_side::right)
										.isEmpty();
						}
						aRun->RenderSaved = aRun->ConversationAnchorEvidencePassed
							&& save_render_evidence(
												window,
												aRun->RenderPath,
												aRun->RenderCollapsesFilesDrawer);

						// Measured here, while a classified comparison is on
						// screen and before the reaccept sequence clears it.
						// A theme change must repaint from the retained spans;
						// if it reparses, the submission count says so.
						{
							if (aRun->ThemeCustomizationPath.has_value()) {
								// The customization course already changed colours
								// twice on this selected classified comparison and
								// compared the host submission count exactly. A
								// second mode gesture here would add an unrelated
								// durable write and weaken the one-write contract.
								aRun->SyntaxThemeRepaintPassed
									= aRun->ThemeCustomizationPassed;
							} else {
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
						}

						const auto retainedResult    = workbench->current_result();
						const auto retainedSelection = workbench->selection();
						const auto retainedContext   = workbench->current_context();
						const auto unchangedAccepted = aRun->DiffInteractionEvidencePassed
							&& aRun->ConversationAnchorEvidencePassed
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
						// [[BPR-0647]]. The overview over the fixture: Overview
						// shows the host-decided page with every file of the
						// pull request and its one action enabled; Continue
						// review returns to Review. The modality the course had
						// is restored before the render evidence, so that
						// evidence is of the state the course reached.
						const auto modalityBefore = window.current_modality();
						window.choose_modality(bpr::qt::review_modality::overview);
						QApplication::processEvents();
						auto* const overview = window.overview_page();
						aRun->OverviewShown  = overview != nullptr && window.showing_overview()
							&& overview->presented()
							&& overview->file_model()->file_count()
								== static_cast<std::size_t>(
									window.files_drawer()->contents()->rowCount())
							&& overview->entry_button()->isEnabled();
						if (overview != nullptr) {
							overview->entry_button()->click();
						}
						QApplication::processEvents();
						aRun->OverviewReturned = !window.showing_overview()
							&& window.current_modality() == bpr::qt::review_modality::review;
						window.choose_modality(modalityBefore);
						QApplication::processEvents();
						aRun->PresentationPassed = aRun->OverviewShown && aRun->OverviewReturned;
						if (!aRun->PresentationPassed) {
							QTextStream{ stdout }
								<< "OVERVIEW_COURSE_FAILED shown="
								<< (aRun->OverviewShown ? 1 : 0)
								<< " returned=" << (aRun->OverviewReturned ? 1 : 0)
								<< " files=" << static_cast<unsigned long long>(overview != nullptr ? overview->file_model()->file_count() : 0U)
								<< " drawer=" << window.files_drawer()->contents()->rowCount()
								<< '\n';
						}
						aRun->RenderSaved = save_render_evidence(
							window, aRun->RenderPath, aRun->RenderCollapsesFilesDrawer);
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
						*window.epoch_selectors()->history_model(),
						2U,
						bpr::core::epoch_primary_cause::force_push_rebase)) {
					window.epoch_selectors()->whole_pr_action()->trigger();
					advance_fixture_stage(
						*aRun, automated_run::stage::epoch_whole_pr);
				}
				break;
			}

			case automated_run::stage::epoch_refresh: {
				const auto identity            = bpr::app::detail::github_application_fixture_access::epoch_refresh_identity(aApplication);
				aRun->LastEpochRefreshIdentity = fixture_epoch_refresh_identity_text(identity, window);
				aRun->LastEpochRefreshFocused  = identity.TabModality && identity.ActiveTab.has_value();
				const auto evidence            = bpr::app::detail::
					github_application_fixture_access::evidence(aApplication);
				if (fixture_epoch_refresh_identity_passed(identity, window)
					&& fixture_presentation_passed(window, aRun->ExpectedActiveHostText,
												   aRun->Expectations, aRun->LastEpochRefreshFocused)
					&& fixture_epoch_pair_passed(
						window,
						evidence.BaseOid,
						evidence.HeadTwoOid)
					&& fixture_epoch_cause_passed(
						*window.epoch_selectors()->history_model(),
						2U,
						bpr::core::epoch_primary_cause::force_push_rebase)) {
					QTextStream{ stdout }
						<< "EPOCH_REFRESH_PRESENTATION_OK:focused=" << aRun->LastEpochRefreshFocused
						<< ":roots=" << window.conversation_model()->rowCount() << '\n'
						<< "EPOCH_REFRESH_IDENTITY:" << aRun->LastEpochRefreshIdentity << '\n';
					window.epoch_selectors()->whole_pr_action()->trigger();
					advance_fixture_stage(
						*aRun, automated_run::stage::epoch_whole_pr);
				}
				break;
			}

			case automated_run::stage::epoch_whole_pr: {
				const auto evidence = bpr::app::detail::
					github_application_fixture_access::evidence(aApplication);
				auto* const control = window.epoch_selectors();
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
				auto* const control = window.epoch_selectors();
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
				auto* const control = window.epoch_selectors();
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
				auto* const control = window.epoch_selectors();
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
				auto* const control = window.epoch_selectors();
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
				auto* const control = window.epoch_selectors();
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
				auto* const control = window.epoch_selectors();
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
				auto* const control = window.epoch_selectors();
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
				auto* const control = window.epoch_selectors();
				if (fixture_epoch_pair_passed(
						window, evidence.HeadTwoOid, evidence.HeadTwoOid)
					&& control->left_selector()->currentIndex()
						== aRun->EpochLeftRow
					&& control->right_selector()->currentIndex()
						== aRun->EpochRightRow) {
					aRun->PresentationPassed = true;
					aRun->RenderSaved        = save_render_evidence(
						window, aRun->RenderPath, aRun->RenderCollapsesFilesDrawer);
					// [[BPR-0654]]. Asked for, the epoch course goes on: the seed
					// run marks the first head, the one it opened before its
					// refresh brought the second, as reviewed and stops; the
					// reuse run, open at the second head over the retained cache
					// with both epochs in the history, asks the overview which
					// files changed between the two. Left in Overview so the page
					// is the one publishing.
					if (aRun->OverviewSinceReviewRequested
						&& aRun->EpochPhase
							== bpr::app::detail::github_epoch_fixture_phase::seed) {
						if (bpr::app::detail::github_application_fixture_access::
								mark_head_reviewed(aApplication, evidence.HeadOneOid)) {
							QTextStream{ stdout } << "OVERVIEW_SINCE_REVIEW_SEEDED\n";
						} else {
							aRun->PresentationPassed = false;
							QTextStream{ stdout } << "OVERVIEW_SINCE_REVIEW_FAILED:seed\n";
						}
						transition_to_shutdown(aApplication, *aRun);
					} else if (aRun->OverviewSinceReviewRequested
							   && aRun->EpochPhase
								   == bpr::app::detail::github_epoch_fixture_phase::reuse) {
						window.choose_modality(bpr::qt::review_modality::overview);
						QApplication::processEvents();
						advance_fixture_stage(
							*aRun, automated_run::stage::overview_since_review);
					} else if (aRun->ReviewBehaviourRequested) {
						// [[BPR-0661]]. The seed run chooses two preferences
						// on the page as a reviewer would; the reuse run opens
						// over the same store and must find them honoured.
						advance_fixture_stage(*aRun, automated_run::stage::review_behaviour);
					} else if (aRun->SavedServerRequested) {
						// [[BPR-0660]], [[BPR-0774]]. The seed run keeps an
						// enterprise server on Accounts & hosts as a reviewer
						// would, through Add enterprise server and its card in
						// the edit state; the reuse run opens over the same store
						// and must find that server's card remembering it.
						if (aRun->EpochPhase
							== bpr::app::detail::github_epoch_fixture_phase::seed) {
							auto* const add = window.add_enterprise_server_button();
							if (add != nullptr) {
								add->click();
								window.enterprise_server_hostname()->setText(QStringLiteral("ghe.kept.example"));
								window.enterprise_server_account()->setText(QStringLiteral("reviewer"));
								window.enterprise_server_add()->click();
							}
						}
						advance_fixture_stage(*aRun, automated_run::stage::saved_server);
					} else {
						transition_to_shutdown(aApplication, *aRun);
					}
				}
				break;
			}

			case automated_run::stage::epoch_maintenance_history:
				// Poll, do not race. `stage::open` calls this same predicate
				// and discards a false return, and it is the same predicate:
				// every condition inside it is established asynchronously, so
				// a refusal on one tick means "not yet", not "wrong". Reading
				// it as "wrong" failed the restarted pruned-delete run 163 ms
				// into a 60-second budget it never touched, while the epoch
				// selectors still read "Resolving the exact comparison…" —
				// about one run in three under a loaded matrix, and never in
				// isolation. The budget is what bounds a condition that never
				// becomes true, and `MaintenanceGate` is what names it.
				// BPR-0447.
				if (window.epoch_selectors()->history_model()->epoch_count() >= 2U) {
					if (begin_epoch_maintenance_probe(aApplication, *aRun)
						&& aRun->MaintenanceProbe
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
				auto* const control = window.epoch_selectors();
				// [[BPR-0664]]. The control blanks the left selector the moment
				// the core starts resolving the anchor the previous stage
				// dispatched, so reading a blank selector as "not applied" and
				// re-asserting on the next 10 ms tick superseded every
				// resolution slower than a tick: each completion arrived one
				// generation stale and the course never left `resolving`.
				// Wait while the core resolves; only a settled core showing
				// another row is asked again (BPR-0655's cure for the defect
				// probe).
				if (aApplication.status().EpochStatus == bpr::core::epoch_status::resolving) {
					break;
				}
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
				auto* const control = window.epoch_selectors();
				if (control->left_selector()->currentIndex()
						== aRun->MaintenanceLeftRow
					&& control->right_selector()->currentIndex()
						== aRun->MaintenanceRightRow
					&& control->prune_action()->isEnabled()) {
					accept_maintenance_confirmation(
						QStringLiteral("pruneSelectedEpochsConfirmationDialog"));
					trigger_settings_maintenance(window, control->prune_action());
					advance_fixture_stage(
						*aRun,
						automated_run::stage::epoch_maintenance_prune_result);
				}
				break;
			}

			case automated_run::stage::epoch_maintenance_prune_result: {
				auto* const control  = window.epoch_selectors();
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
				auto* const control  = window.epoch_selectors();
				const auto  provider = bpr::app::detail::
					github_application_fixture_access::evidence(aApplication);
				const auto maintenance = bpr::app::detail::
					github_application_fixture_access::maintenance_evidence(
						aApplication, aRun->MaintenanceTarget);
				auto* const workbench = window.comparison_surface();
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

			case automated_run::stage::overview_since_review: {
				// [[BPR-0654]]. Poll, do not race: the second head is ingested
				// over the seed's retained cache and looked up asynchronously, and
				// the budget bounds a set that never resolves. Two epochs in the
				// history say both heads are retained; a row still "unknown" says
				// the set is not yet decided.
				auto* const overview = window.overview_page();
				if (overview == nullptr || !overview->presented()
					|| window.epoch_selectors()->history_model()->epoch_count() < 2U) {
					break;
				}
				const auto& projection = *overview->projection();
				const auto  undecided  = std::ranges::any_of(
					projection.Files, [](const bpr::application::overview_file_row& aRow) {
						return aRow.Changed == bpr::application::overview_change_since_review::unknown;
					});
				if (undecided || !projection.SinceLastReview.FilesChanged.has_value()) {
					break;
				}
				auto changed = QString{ };
				for (const auto& row : projection.Files) {
					if (row.Changed != bpr::application::overview_change_since_review::unchanged) {
						changed += (changed.isEmpty() ? QString{ } : QStringLiteral(","))
							+ QString::fromStdString(row.Path);
					}
				}
				const auto entry                = QString::fromStdString(projection.EntryPath.value_or(""));
				aRun->OverviewSinceReviewPassed = *projection.SinceLastReview.FilesChanged == 1U
					&& changed == QStringLiteral("review.cpp") && entry == changed
					&& projection.Entry == bpr::application::overview_entry_kind::continue_review;
				QTextStream{ stdout } << "OVERVIEW_SINCE_REVIEW:files="
									  << static_cast<unsigned long long>(*projection.SinceLastReview.FilesChanged)
									  << ":changed=" << changed << ":entry=" << entry << '\n';
				aRun->PresentationPassed = aRun->PresentationPassed && aRun->OverviewSinceReviewPassed;
				transition_to_shutdown(aApplication, *aRun);
				break;
			}

			case automated_run::stage::review_behaviour: {
				// [[BPR-0661]]. Poll, do not race: the host stores the record
				// and publishes it back, and the budget bounds a store that
				// never answers. The seed run retains the original composed Continue
				// gesture after the first-visit Start probe. The reuse run requires
				// the drawer to open on the restored unread filter.
				const auto seed = aRun->EpochPhase
					== bpr::app::detail::github_epoch_fixture_phase::seed;
				// [[BPR-1295]]. "Continue review enters" went with Read: Continue
				// review enters Review with no preference, and the course still
				// proves it does.
				if (seed && !aRun->ReviewBehaviourPreferencesSelected) {
					auto* const unread
						= window.findChild<QToolButton*>(QStringLiteral("commentsFilterUnread"));
					if (unread == nullptr || window.findChild<QToolButton*>(QStringLiteral("continueModalityReview")) != nullptr) {
						fail_automated_run(aApplication, *aRun);
						break;
					}
					unread->click();
					aRun->ReviewBehaviourPreferencesSelected = true;
					break;
				}
				const auto expected = bpr::application::review_behaviour_settings{
					.CommentsFilter = bpr::application::review_comments_filter::unread,
				};
				if (window.behaviour() != expected) {
					break;
				}
				auto passed = false;
				if (seed) {
					window.choose_modality(bpr::qt::review_modality::overview);
					QApplication::processEvents();
					auto* const overview = window.overview_page();
					if (overview == nullptr || !overview->presented()
						|| overview->projection()->Entry
							!= bpr::application::overview_entry_kind::continue_review
						|| !overview->projection()->EntryPath.has_value()
						|| !overview->entry_button()->isEnabled()) {
						break;
					}
					const auto before = bpr::app::detail::
						github_application_fixture_access::review_mutation_evidence(
							aApplication);
					overview->entry_button()->click();
					QApplication::processEvents();
					const auto after = bpr::app::detail::
						github_application_fixture_access::review_mutation_evidence(
							aApplication);
					aRun->ContinueReviewEnteredReview = window.current_modality()
						== bpr::qt::review_modality::review;
					aRun->ContinueReviewPublishedNothing
						= before.Submissions == after.Submissions
						&& before.StoredComments == after.StoredComments
						&& before.Replies == after.Replies
						&& before.Resolutions == after.Resolutions;
					passed = aRun->StartReviewEnteredReview
						&& aRun->StartReviewPublishedNothing
						&& aRun->ContinueReviewEnteredReview
						&& aRun->ContinueReviewPublishedNothing;
				} else {
					passed = window.conversation_panel()->filter_unread_button()->isChecked()
						&& !window.conversation_panel()->filter_unresolved_button()->isChecked();
				}
				QTextStream{ stdout } << (seed ? "REVIEW_BEHAVIOUR_SEEDED" : "REVIEW_BEHAVIOUR")
									  << ":continue=review:filter=unread:honoured="
									  << (passed ? 1 : 0);
				if (seed) {
					QTextStream{ stdout }
						<< ":start=review:start-publication=none:continue-publication=none";
				}
				QTextStream{ stdout } << '\n';
				aRun->PresentationPassed = aRun->PresentationPassed && passed;
				transition_to_shutdown(aApplication, *aRun);
				break;
			}

			case automated_run::stage::saved_server: {
				// [[BPR-0660]], [[BPR-0774]]. Poll, do not race: the queue host
				// stores the server and publishes it back. The seed run requires
				// exactly the stored record to reach the window; the reuse run
				// opens over the same store and requires the server offered as a
				// host, its card remembering exactly that record, which is what a
				// kept server is for.
				if (window.add_enterprise_server_button() == nullptr) {
					transition_to_shutdown(aApplication, *aRun);
					break;
				}
				const auto seed = aRun->EpochPhase
					== bpr::app::detail::github_epoch_fixture_phase::seed;
				const auto expected = bpr::application::saved_server{
					.Hostname = "ghe.kept.example",
					.Account  = "reviewer",
				};
				const auto cards      = window.server_cards();
				const auto card       = std::ranges::find_if(cards, [](const bpr::qt::server_card* const aCard) {
					return aCard->hostname() == "ghe.kept.example";
				});
				const auto offered    = card != cards.end();
				const auto remembered = offered && (*card)->presentation().Remembered == expected;
				const auto kept       = seed
					? std::ranges::find(window.kept_servers(), expected) != window.kept_servers().end()
					: remembered;
				if (!kept) {
					break;
				}
				const auto passed = kept && (seed || offered);
				QTextStream{ stdout } << (seed ? "SAVED_SERVER_SEEDED" : "SAVED_SERVER")
									  << ":host=ghe.kept.example:kept=" << (kept ? 1 : 0)
									  << ":offered=" << (offered ? 1 : 0) << '\n';
				aRun->PresentationPassed = aRun->PresentationPassed && passed;
				transition_to_shutdown(aApplication, *aRun);
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
					// The schemes have settled; the owner has not necessarily
					// come back. Wait for the focus the measurement below
					// requires, rather than measuring into its absence and
					// reporting the owner's trip to System Settings as a state
					// failure. Bounded only by the run budget, which is checked
					// at the top of every tick before this switch, so the wait
					// cannot outlive the run. BPR-0391.
					aRun->Appearance.OwnerReturnEvaluated         = true;
					aRun->Appearance.OwnerWindowActive            = window.isActiveWindow();
					aRun->Appearance.OwnerApplicationActiveWindow = QApplication::activeWindow() == &window;
					aRun->Appearance.OwnerExactFocus              = QApplication::focusWidget()
						== aRun->Appearance.InitialFocus;
					if (!native_appearance_owner_returned(window, *aRun)) {
						if (!aRun->Appearance.WaitingForOwnerReturn) {
							++aRun->Appearance.OwnerWaitEpisode;
							QTextStream stream{ stdout };
							stream
								<< "NATIVE_APPEARANCE_WAITING_FOR_OWNER_RETURN:"
								<< static_cast<unsigned long long>(
									   aRun->Appearance.OwnerWaitEpisode)
								<< '\n'
								<< "NATIVE_APPEARANCE_OWNER_WINDOW_ACTIVE:"
								<< (aRun->Appearance.OwnerWindowActive ? "true" : "false")
								<< '\n'
								<< "NATIVE_APPEARANCE_OWNER_APPLICATION_ACTIVE_WINDOW:"
								<< (aRun->Appearance.OwnerApplicationActiveWindow ? "true" : "false")
								<< '\n'
								<< "NATIVE_APPEARANCE_OWNER_EXACT_FOCUS:"
								<< (aRun->Appearance.OwnerExactFocus ? "true" : "false")
								<< '\n';
							stream.flush();
							aRun->Appearance.WaitingForOwnerReturn = true;
						}
						break;
					}
					if (!aRun->Appearance.OwnerReturnReported) {
						QTextStream stream{ stdout };
						stream << "NATIVE_APPEARANCE_OWNER_RETURNED\n";
						stream.flush();
						aRun->Appearance.OwnerReturnReported = true;
					}
					// A later loss starts a new indexed wait episode instead of
					// silently re-arming the old one.
					aRun->Appearance.WaitingForOwnerReturn = false;
					const auto* const appearanceWorkbench
						= window.comparison_surface();
					aRun->Appearance.ObservedDiffPalette
						= appearanceWorkbench->palette();
					aRun->Appearance.ObservedDiffFont
						= appearanceWorkbench->font();
					const auto finalGeometryStatistics
						= appearanceWorkbench->statistics();
					const auto finalGeometryRebuilds
						= finalGeometryStatistics.GeometryRebuilds;
					aRun->Appearance.DiffGeometryRebuildDelta
						= finalGeometryRebuilds
							>= aRun->Appearance.InitialDiffGeometryRebuilds
						? finalGeometryRebuilds
							- aRun->Appearance.InitialDiffGeometryRebuilds
						: 0U;
					const auto historyStart
						= aRun->Appearance.InitialDiffGeometryHistorySize;
					const auto historyEnd
						= finalGeometryStatistics.GeometryRebuildHistorySize;
					const auto historyCount                        = historyEnd >= historyStart
						? historyEnd - historyStart
						: 0U;
					aRun->Appearance.DiffGeometryRebuildCauseCount = std::min(
						historyCount,
						aRun->Appearance.DiffGeometryRebuildCauses.size());
					for (
						auto index = std::size_t{ 0U };
						index < aRun->Appearance.DiffGeometryRebuildCauseCount;
						++index) {
						aRun->Appearance.DiffGeometryRebuildCauses[index]
							= finalGeometryStatistics.GeometryRebuildHistory[historyStart + index];
					}
					aRun->Appearance.DiffGeometryAttributionComplete
						= !aRun->Appearance.InitialDiffGeometryHistoryOverflow
						&& !finalGeometryStatistics.GeometryRebuildHistoryOverflow
						&& historyEnd >= historyStart
						&& historyCount
							== aRun->Appearance.DiffGeometryRebuildDelta
						&& historyCount
							<= aRun->Appearance.DiffGeometryRebuildCauses.size();
					const auto stateBeforeRender = native_appearance_dark_state_passed(window, *aRun);
					auto       changedCapture    = stateBeforeRender
						? capture_normalized_render(window)
						: native_appearance_capture_result{ };
					aRun->Appearance.ChangedCaptureDisposition
						= changedCapture.Disposition;
					const auto stateAfterRender = stateBeforeRender
						&& !changedCapture.Image.isNull()
						&& native_appearance_dark_state_passed(window, *aRun);
					aRun->Appearance.StatePassed  = stateAfterRender;
					aRun->Appearance.PixelsDiffer = aRun->Appearance.StatePassed
						&& aRun->Appearance.InitialRender.size()
							== changedCapture.Image.size()
						&& aRun->Appearance.InitialRender.devicePixelRatio()
							== changedCapture.Image.devicePixelRatio()
						&& !same_render_pixels(
														aRun->Appearance.InitialRender,
														changedCapture.Image);
					if (aRun->Appearance.PixelsDiffer) {
						aRun->Appearance.ChangedRender
							= std::move(changedCapture.Image);
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

#if defined(BPR_BUILD_TESTING)
	enum class native_notification_stage {
		connect,
		await_connection,
		await_rule_surface,
		await_quiet_baseline,
		await_changed_head,
		await_submission,
		await_activation,
		shutdown,
	};

	struct native_notification_run
	{
		native_notification_stage   Stage = native_notification_stage::connect;
		std::chrono::milliseconds   Budget{ };
		automated_clock::time_point Started;
		bool                        Attended                  = false;
		bool                        RequestCut                = false;
		bool                        ActivationCut             = false;
		bool                        SignalObservationCut      = false;
		bool                        Failed                    = false;
		std::size_t                 PlatformActivationSignals = 0U;
		QSystemTrayIcon*            PlatformTray              = nullptr;
	};

	[[nodiscard]] auto notification_fixture_ref() -> bpr::core::pull_request_ref
	{
		return {
			.Host       = "github.com",
			.Account    = "octo-reviewer",
			.Repository = "octo-org/review-repository",
			.Number     = 17,
		};
	}

	void fail_native_notification_run(
		bpr::app::github_application& aApplication,
		native_notification_run&      aRun,
		const std::string_view        aReason)
	{
		QTextStream{ stdout }
			<< "NATIVE_NOTIFICATION_REFUSED:"
			<< QString::fromUtf8(aReason.data(), static_cast<qsizetype>(aReason.size()))
			<< '\n'
			<< Qt::flush;
		aRun.Failed = true;
		aRun.Stage  = native_notification_stage::shutdown;
		aApplication.begin_shutdown();
	}

	[[nodiscard]] auto exact_notification_queue_item(
		bpr::qt::workbench_window&        aWindow,
		const bpr::core::attention_bucket aBucket) -> bool
	{
		if (aWindow.queue_landing()->rowCount() != 1) {
			return false;
		}
		const auto item = aWindow.queue_landing()->item_at(
			aWindow.queue_landing()->index(0));
		return item.has_value() && item->Ref == notification_fixture_ref()
			&& item->Membership.Role == bpr::core::github_role::none
			&& item->Membership.MatchedRule
			&& item->Membership.Rule
			== bpr::core::observer_rule{
				   .Kind    = bpr::core::observer_rule_kind::repository,
				   .Pattern = "octo-org/review-repository",
			   }
		&& item->Bucket == aBucket;
	}

	void schedule_native_notification_run(
		bpr::app::github_application&                  aApplication,
		const std::shared_ptr<native_notification_run> aRun)
	{
		aRun->Started = automated_clock::now();
		// [[BPR-0861]]. The profile runtime owns the sink (BPR-0705), so its tray
		// is no child of the window; ask the runtime for it.
		aRun->PlatformTray = bpr::app::detail::github_application_fixture_access::notification_tray(
			aApplication);
		if (!aRun->SignalObservationCut) {
			if (aRun->PlatformTray == nullptr) {
				fail_native_notification_run(
					aApplication, *aRun, "platform_tray_not_composed");
			} else {
				QObject::connect(
					aRun->PlatformTray,
					&QSystemTrayIcon::messageClicked,
					&aApplication.window(),
					[aWeakRun = std::weak_ptr<native_notification_run>{ aRun }] {
						if (const auto aLockedRun = aWeakRun.lock()) {
							++aLockedRun->PlatformActivationSignals;
						}
					});
			}
		}
		auto tick = std::make_shared<std::function<void()>>();
		*tick     = [&aApplication,
					 aRun,
					 tick,
					 alive = aApplication.liveness()] {
			if (alive.expired()) {
				return;
			}
			if (aRun->Stage != native_notification_stage::shutdown
				&& automated_clock::now() - aRun->Started >= aRun->Budget) {
				fail_native_notification_run(
					aApplication, *aRun, "automated_budget_exhausted");
			}

			auto& window = aApplication.window();
			switch (aRun->Stage) {
			case native_notification_stage::connect: {
				// Before anything is armed, because this is the precondition
				// every later marker silently assumed. macOS Notification
				// Center registers applications by bundle identity; a process
				// without one cannot request authorization, never gains a
				// System Settings entry, and its delivery requests go nowhere.
				// The course substitutes the display boundary, so it is exactly
				// the fact the course cannot otherwise notice, and an entire
				// attended packet was built on a bare Mach-O that no banner
				// could ever have been addressed to. Refused rather than
				// reported: a preflight that passes here is claiming a banner is
				// possible. BPR-0392.
				const auto identity = notification_bundle_identity();
				QTextStream{ stdout }
					<< "NATIVE_NOTIFICATION_BUNDLE_IDENTITY:" << identity << '\n'
					<< Qt::flush;
				if (identity == QStringLiteral("none")) {
					fail_native_notification_run(
						aApplication, *aRun, "bundle_identity_absent");
					break;
				}
				window.github_connect_action()->trigger();
				aRun->Stage = native_notification_stage::await_connection;
				break;
			}

			case native_notification_stage::await_connection:
				if (window.github_status_text() == "GitHub account connected."
					&& window.github_open_action()->isEnabled()) {
					// [[BPR-0771]]. A pull request is named in the Home bar; the locator is retired.
					window.home_open_bar()->setText(QStringLiteral("%1/%2#%3").arg(QStringLiteral("octo-org"), QStringLiteral("review-repository"), QString::number(17)));
					window.home_open_action()->trigger();
					window.queue_rule_kind_selector()->setCurrentIndex(
						window.queue_rule_kind_selector()->findData(static_cast<int>(
							bpr::core::observer_rule_kind::repository)));
					window.queue_rule_pattern_editor()->setText(
						QStringLiteral("octo-org/review-repository"));
					aRun->Stage = native_notification_stage::await_rule_surface;
				}
				break;

			case native_notification_stage::await_rule_surface:
				if (window.add_queue_rule_action()->isEnabled()) {
					window.add_queue_rule_action()->trigger();
					aRun->Stage = native_notification_stage::await_quiet_baseline;
				}
				break;

			case native_notification_stage::await_quiet_baseline: {
				const auto evidence = bpr::app::detail::
					github_application_fixture_access::queue_notification_evidence(
						aApplication);
				if (!window.remove_queue_rule_action()->isEnabled()
					|| !exact_notification_queue_item(
						window, bpr::core::attention_bucket::quiet)) {
					break;
				}
				if (evidence.RequestsAttempted != 0U || evidence.Submitted != 0U
					|| evidence.DisplaySubmissions != 0U
					|| aRun->PlatformActivationSignals != 0U
					|| evidence.LastSubmitted.has_value()) {
					fail_native_notification_run(
						aApplication, *aRun, "quiet_baseline_requested_notification");
					break;
				}
				QTextStream{ stdout }
					<< "NATIVE_NOTIFICATION_QUIET_BASELINE_OK\n"
					<< Qt::flush;
				window.github_refresh_action()->trigger();
				aRun->Stage = native_notification_stage::await_changed_head;
				break;
			}

			case native_notification_stage::await_changed_head:
				if (!exact_notification_queue_item(
						window,
						bpr::core::attention_bucket::changed_since_my_review)) {
					break;
				}
				if (aRun->RequestCut) {
					const auto evidence = bpr::app::detail::
						github_application_fixture_access::queue_notification_evidence(
							aApplication);
					if (evidence.RequestsAttempted != 0U || evidence.Submitted != 0U
						|| evidence.DisplaySubmissions != 0U
						|| aRun->PlatformActivationSignals != 0U
						|| evidence.LastSubmitted.has_value()) {
						fail_native_notification_run(
							aApplication, *aRun, "request_cut_reached_sink");
						break;
					}
					aRun->Stage = native_notification_stage::shutdown;
					aApplication.begin_shutdown();
					break;
				}
				aRun->Stage = native_notification_stage::await_submission;
				break;

			case native_notification_stage::await_submission: {
				const auto evidence = bpr::app::detail::
					github_application_fixture_access::queue_notification_evidence(
						aApplication);
				const auto expected = bpr::application::delivered_notification{
					.Title  = "octo-org/review-repository #17",
					.Body   = "The code moved after you reviewed it.",
					.Target = bpr::application::queue_notification_target{
						.Ref = notification_fixture_ref(),
					},
				};
				if (evidence.RequestsAttempted != 1U || evidence.Submitted != 1U
					|| evidence.LastSubmitted != std::optional{ expected }) {
					break;
				}
				if (!aRun->Attended && evidence.DisplaySubmissions != 1U) {
					fail_native_notification_run(
						aApplication, *aRun, "display_boundary_not_reached");
					break;
				}
				if (aRun->PlatformActivationSignals != 0U
					|| evidence.Activations != 0U || evidence.Selections != 0U) {
					fail_native_notification_run(
						aApplication, *aRun, "activation_signal_before_click");
					break;
				}
				QTextStream{ stdout }
					<< "NATIVE_NOTIFICATION_REQUEST_SUBMITTED_OK\n"
					<< "NATIVE_NOTIFICATION_PAYLOAD:octo-org/review-repository #17|"
					<< "The code moved after you reviewed it.\n"
					<< (aRun->Attended
							? "NATIVE_NOTIFICATION_AWAITING_PLATFORM_CLICK\n"
							: "NATIVE_NOTIFICATION_DISPLAY_BOUNDARY_OK\n")
					<< Qt::flush;
				if (!aRun->Attended) {
					// Cocoa may keep an unattended command-line test application
					// natively inactive. Establish only Qt's logical active window;
					// the real notification selection path must still acquire focus.
					QT_WARNING_PUSH
					QT_WARNING_DISABLE_DEPRECATED
					QApplication::setActiveWindow(&window);
					QT_WARNING_POP
					QApplication::processEvents();
					if (QApplication::activeWindow() != &window) {
						fail_native_notification_run(
							aApplication,
							*aRun,
							"qt_active_window_not_acquired");
						break;
					}
					QTextStream{ stdout }
						<< "NATIVE_NOTIFICATION_QT_ACTIVE_WINDOW_OK\n"
						<< Qt::flush;
					if (aRun->PlatformTray == nullptr) {
						fail_native_notification_run(
							aApplication, *aRun, "platform_tray_not_composed");
						break;
					}
					aRun->PlatformTray->messageClicked();
				}
				aRun->Stage = native_notification_stage::await_activation;
				break;
			}

			case native_notification_stage::await_activation: {
				const auto evidence = bpr::app::detail::
					github_application_fixture_access::queue_notification_evidence(
						aApplication);
				if (evidence.Activations != 1U) {
					break;
				}
				if (aRun->PlatformActivationSignals != 1U) {
					fail_native_notification_run(
						aApplication, *aRun, "tray_signal_count_mismatch");
					break;
				}
				QTextStream{ stdout }
					<< "NATIVE_NOTIFICATION_TRAY_SIGNAL_OK\n"
					<< Qt::flush;
				if (aRun->ActivationCut) {
					if (evidence.Selections != 0U) {
						fail_native_notification_run(
							aApplication, *aRun, "activation_cut_selected_row");
						break;
					}
				} else {
					const auto selected = window.selected_queue_item();
					if (evidence.Selections != 1U || !selected.has_value()
						|| selected->Ref != notification_fixture_ref()
						|| QApplication::focusWidget() != window.home_list()->current_view()) {
						break;
					}
					QTextStream{ stdout }
						<< "NATIVE_NOTIFICATION_ACTIVATION_OK\n"
						<< "NATIVE_NOTIFICATION_EXACT_LANDING_SELECTION_OK\n"
						<< Qt::flush;
				}
				aRun->Stage = native_notification_stage::shutdown;
				aApplication.begin_shutdown();
				break;
			}

			case native_notification_stage::shutdown:
				if (!aApplication.status().ShutdownComplete) {
					break;
				}
				if (!aRun->Failed
					&& !aApplication.status().ShutdownCleanupFailed) {
					QTextStream{ stdout } << "NATIVE_NOTIFICATION_SHUTDOWN_OK\n";
					if (aRun->RequestCut) {
						QTextStream{ stdout }
							<< "NATIVE_NOTIFICATION_REQUEST_CUT_OK\n";
					} else if (aRun->ActivationCut) {
						QTextStream{ stdout }
							<< "NATIVE_NOTIFICATION_ACTIVATION_CUT_OK\n";
					} else if (aRun->Attended) {
						QTextStream{ stdout }
							<< "NATIVE_NOTIFICATION_ATTENDED_RUN_OK\n";
					} else {
						QTextStream{ stdout }
							<< "NATIVE_NOTIFICATION_ATTENDED_CONTRACT_OK\n";
					}
				} else if (!aRun->Failed) {
					// [[BPR-0875]]. The refusal keeps its wording; the source it
					// refused on follows it.
					const auto detail = aApplication.shutdown_cleanup_detail();
					QTextStream{ stdout }
						<< "NATIVE_NOTIFICATION_REFUSED:shutdown_cleanup_failed\n"
						<< "NATIVE_NOTIFICATION_CLEANUP_SOURCE:"
						<< QString::fromStdString(detail.empty() ? std::string{ "unnamed" } : detail)
						<< "\n";
				}
				aApplication.request_quit(
					aRun->Failed || aApplication.status().ShutdownCleanupFailed
						? EXIT_FAILURE
						: EXIT_SUCCESS);
				return;
			}
			QTimer::singleShot(10, &window, *tick);
		};
		QTimer::singleShot(0, &aApplication.window(), *tick);
	}
#endif

}

auto main(int argc, char** argv) -> int
{
	bpr::app::detail::terminal_diagnostics terminalDiagnostics;
	const auto                             profileLaunch = parse_profile_launch_arguments(argc, argv);
	if (!profileLaunch) {
		QTextStream{ stdout } << "PROFILE_LAUNCH_ARGUMENTS_INVALID\n";
		return EXIT_FAILURE;
	}
#if defined(__APPLE__) && !defined(BPR_BUILD_TESTING)
	if (const auto preflight = run_designated_live_preflight_probe(argc, argv);
		preflight.has_value()) {
		return *preflight;
	}
	// BPR-0262 fact_4, and the reason it has to be here rather than anywhere
	// more convenient: attended authority is refused unless the exact bound
	// supervisor launched this process, and that has to be settled before
	// QApplication exists and before any public observation. Everything below
	// this line has already decided the process is allowed to run.
	//
	// Only the flag's presence is read here. Whether the approval itself is
	// well-formed is settled further down, because a malformed approval and an
	// unauthenticated parent are different refusals and answering the second
	// first is what keeps a direct launch from learning anything.
	if (has_argument(argc, argv, "--attended-designated-live-approval")) {
		const auto image      = bpr::app::designated_live_provenance::running_executable_path();
		const auto supervisor = image.has_value()
			? bpr::app::designated_live_provenance::supervisor_image_for(*image)
			: std::nullopt;
		if (!supervisor.has_value()
			|| !bpr::app::designated_live_provenance::authenticate_supervisor(
				*supervisor)) {
			// Deliberately the same words a malformed approval produces. A
			// caller that cannot authenticate learns that it failed, not which
			// check refused it.
			std::fputs("STARTUP_FAILED\n", stdout);
			return EXIT_FAILURE;
		}
	}
#endif
	if (!bpr::app::restrict_installed_runtime(argc, argv)) {
		bpr::app::detail::record_terminal_diagnostic("INSTALLED_RUNTIME_REFUSED\n");
		return EXIT_FAILURE;
	}
	// [[BPR-0974]]. A tests-disabled Windows image keeps its Qt resources in a
	// bundle beside it. Registered here, before anything reads a resource, and
	// declared before QApplication so the registration outlives every Qt
	// object. A missing or different bundle ends startup instead of leaving
	// the window without its icon, Dif or syntax highlighting. A graph that
	// embeds its resources gets an empty owner.
	const auto installedResources = bpr::app::load_image_resources();
	if (!installedResources) {
		const auto file    = bpr::app::installed_resources_file_name;
		const auto refusal = bpr::app::installed_resources_refusal_name(installedResources.error());
		bpr::app::detail::terminal_text_stream{ } << "INSTALLED_RESOURCES_REFUSED:"
												  << QLatin1StringView{ file.data(), static_cast<qsizetype>(file.size()) } << ':'
												  << QLatin1StringView{ refusal.data(), static_cast<qsizetype>(refusal.size()) } << '\n';
		return EXIT_FAILURE;
	}
	QApplication qtApplication{ argc, argv };
#if defined(__linux__)
	// [[BPR-1024]]. Keys go to the window the X server focused, not to one
	// whose activation the window manager may have refused.
	bpr::app::xcb_activation_host activationHost;
#endif
	QApplication::setQuitOnLastWindowClosed(false);
	QCoreApplication::setApplicationName(QStringLiteral("BetterPullReview"));
	QGuiApplication::setApplicationDisplayName(
		QStringLiteral("BetterPullReview"));
	QCoreApplication::setApplicationVersion(QStringLiteral(BPR_VERSION));
	QCoreApplication::setOrganizationName(
		QStringLiteral("BetterPullReview"));
	QApplication::setWindowIcon(application_icon());
	// [[BPR-1270]]. The bundled default code font, before anything composes
	// an appearance. A failure is not fatal: the default code font is then
	// the platform's fixed font.
	[[maybe_unused]] const auto bundledCodeFont = bpr::qt::register_bundled_code_font();

#if defined(BPR_BUILD_TESTING)
	if (has_argument(argc, argv, "--code-font-probe")) {
		// The fresh profile's code font as the composition would apply it,
		// resolved by this process's font database.
		const auto defaults = bpr::qt::default_typography_settings();
		const auto font     = bpr::qt::to_presentation_font(defaults.Code);
		const auto resolved = QFontInfo{ font };
		const auto family   = QString::fromStdString(defaults.Code.Font.Family);
		const auto valid    = bundledCodeFont
			&& family == bpr::qt::bundled_code_font_family()
			&& resolved.family() == family && resolved.fixedPitch();
		QTextStream{ stdout }
			<< "CODE_FONT_PROBE registered=" << (bundledCodeFont ? "yes" : "no")
			<< " default=\"" << family << "\" resolved=\"" << resolved.family()
			<< "\" fixed_pitch=" << (resolved.fixedPitch() ? "yes" : "no") << '\n'
			<< (valid ? "CODE_FONT_OK\n" : "CODE_FONT_INVALID\n");
		return valid ? EXIT_SUCCESS : EXIT_FAILURE;
	}
	if (has_argument(argc, argv, "--application-icon-probe")) {
		const auto icon  = QApplication::windowIcon();
		const auto valid = !icon.isNull()
			&& !icon.pixmap(QSize{ 16, 16 }).isNull()
			&& !icon.pixmap(QSize{ 256, 256 }).isNull()
			&& !icon.pixmap(QSize{ 512, 512 }).isNull();
		QTextStream{ stdout }
			<< (valid ? "APPLICATION_ICON_OK\n" : "APPLICATION_ICON_INVALID\n");
		return valid ? EXIT_SUCCESS : EXIT_FAILURE;
	}
#endif

	try {
		auto config = bpr::app::github_application_config{
			.EnterpriseHostname = std::nullopt,
		};
		// Automated evidence runs exercise the real host with a private temporary
		// profile, so they never compete for the interactive owner's lease. In
		// every build ([[BPR-0820]]); `automated_run_uses_private_profile` names
		// the runs that choose their root some other way. The directory is owned
		// by this scope and outlives `desktop`, which is declared after it.
		auto automatedData        = std::unique_ptr<QTemporaryDir>{ };
		auto automatedProfileRoot = std::optional<std::filesystem::path>{ };
		if (automated_run_uses_private_profile(argc, argv)) {
			automatedData = std::make_unique<QTemporaryDir>(
				QDir::temp().filePath(QStringLiteral("bpr-automated-profile-XXXXXX")));
			if (!automatedData->isValid()) {
				QTextStream{ stdout } << "STARTUP_FAILED\n";
				return EXIT_FAILURE;
			}
			const auto automatedParent = native_filesystem_path(
				QFileInfo{ automatedData->path() }.canonicalFilePath());
			// Narrowed with the stores' own definition of owner-private, so the
			// seam adds no platform refusal of its own: `mkdtemp`'s `0700`
			// already is that on POSIX, but a Windows temporary directory
			// inherits its parent's access-control list. The `data` leaf does
			// not exist yet, and the composition creates it under the same
			// rule, as it creates any default root.
			if (!bpr::adapters::sqlite::make_storage_root_owner_private(
					automatedParent)) {
				QTextStream{ stdout } << "AUTOMATED_PROFILE_PRIVATE_FAILED\n";
				return EXIT_FAILURE;
			}
			automatedProfileRoot       = automatedParent / "data";
			config.ApplicationDataRoot = automatedProfileRoot;
		}
		config.Profile             = profileLaunch->Profile;
		config.ProfileLaunchIntent = profileLaunch->Intent;
		auto attendedApprovalCount = std::size_t{ 0U };
		auto replayResetCount      = std::size_t{ 0U };
		for (auto index = 1; index < argc; ++index) {
			if (std::string_view{ argv[index] }
				== "--attended-designated-live-approval") {
				++attendedApprovalCount;
			}
			if (std::string_view{ argv[index] }
				== "--initialize-or-reset-designated-live-replay-state") {
				++replayResetCount;
			}
		}
#if defined(BPR_BUILD_TESTING)
		if (attendedApprovalCount != 0U || replayResetCount != 0U) {
			QTextStream{ stdout } << "STARTUP_FAILED\n";
			return EXIT_FAILURE;
		}
#else
		if (attendedApprovalCount > 1U || replayResetCount > 1U
			|| (attendedApprovalCount != 0U && replayResetCount != 0U)) {
			QTextStream{ stdout } << "STARTUP_FAILED\n";
			return EXIT_FAILURE;
		}
		const auto applicationDataRoot = native_filesystem_path(
											 QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
											 .lexically_normal();
		if (replayResetCount == 1U) {
			if (argc != 2
				|| !bpr::app::initialize_or_reset_designated_live_replay_state(
					applicationDataRoot)) {
				QTextStream{ stdout } << "STARTUP_FAILED\n";
				return EXIT_FAILURE;
			}
			QTextStream{ stdout } << "DESIGNATED_LIVE_REPLAY_STATE_READY\n";
			return EXIT_SUCCESS;
		}
		if (attendedApprovalCount == 1U) {
			const auto approval = argument_value(
				argc, argv, "--attended-designated-live-approval");
			if (!approval.has_value()) {
				QTextStream{ stdout } << "STARTUP_FAILED\n";
				return EXIT_FAILURE;
			}
			const auto approvalPath = std::filesystem::path{ std::string{ *approval } };
			if (!approvalPath.is_absolute()
				|| approvalPath.lexically_normal() != approvalPath) {
				QTextStream{ stdout } << "STARTUP_FAILED\n";
				return EXIT_FAILURE;
			}
			const auto executableUtf8             = QCoreApplication::applicationFilePath().toUtf8();
			config.AttendedDesignatedLiveApproval = approvalPath;
			config.ApplicationDataRoot            = applicationDataRoot;
			config.RunningExecutable              = std::filesystem::path{ std::string{
				executableUtf8.constData(), static_cast<std::size_t>(executableUtf8.size()) } };
			config.AttendedSourceSha256           = BPR_ATTENDED_REVIEW_SOURCE_SHA256;
			config.BuildConfiguration             = BPR_BUILD_CONFIGURATION;
			config.BuildTesting                   = false;
	#if !defined(__APPLE__)
			// The capsule, its supervisor, and the provenance handshake are the
			// macOS designated-live path (BPR-0352); no other platform ships
			// one, so an attended approval here has nothing to verify against
			// and refuses. A tests-off build on Windows or Linux did not
			// compile this branch at all until the refusal was written
			// (BPR-0440).
			QTextStream{ stdout } << "STARTUP_FAILED\n";
			return EXIT_FAILURE;
	#else
			// fact_5. The supervisor digest is computed here rather than
			// accepted, from the fixed position stage one occupies inside this
			// capsule -- the same derivation the handshake above already used
			// to decide whose reply to believe.
			const auto supervisorImage
				= bpr::app::designated_live_provenance::supervisor_image_for(
					config.RunningExecutable);
			if (!supervisorImage.has_value()) {
				QTextStream{ stdout } << "STARTUP_FAILED\n";
				return EXIT_FAILURE;
			}
			auto supervisorFile = QFile{ QString::fromStdString(
				supervisorImage->string()) };
			if (!supervisorFile.open(QIODevice::ReadOnly)) {
				QTextStream{ stdout } << "STARTUP_FAILED\n";
				return EXIT_FAILURE;
			}
			auto supervisorHash = QCryptographicHash{ QCryptographicHash::Sha256 };
			if (!supervisorHash.addData(&supervisorFile)) {
				QTextStream{ stdout } << "STARTUP_FAILED\n";
				return EXIT_FAILURE;
			}
			config.AttendedSupervisorSha256
				= supervisorHash.result().toHex().toStdString();
			// The inventory describes the capsule and therefore cannot live
			// inside it, so this one is supplied by the supervisor that
			// verified it. A wrong value renders a subject the operator never
			// approved, which refuses; it cannot make a bad capsule pass.
			const auto inventoryDigest = argument_value(
				argc, argv, "--attended-designated-live-inventory-sha256");
			if (!inventoryDigest.has_value()) {
				QTextStream{ stdout } << "STARTUP_FAILED\n";
				return EXIT_FAILURE;
			}
			config.AttendedInventorySha256 = std::string{ *inventoryDigest };
	#endif
		}
#endif
		if (const auto host = argument_value(argc, argv, "--ghes-host");
			host.has_value()) {
			config.EnterpriseHostname = std::string{ *host };
		}
		auto enterpriseHostname = config.EnterpriseHostname;

#if defined(BPR_BUILD_TESTING)
		if (has_argument(argc, argv, "--network-isolation-probe")) {
			const auto denied = bpr::app::detail::
				github_application_fixture_access::network_isolation_probe();
			QTextStream{ stdout }
				<< (denied ? "APPLICATION_NETWORK_ISOLATION_DENIED\n"
						   : "APPLICATION_NETWORK_ISOLATION_NOT_DENIED\n");
			return denied ? EXIT_SUCCESS : EXIT_FAILURE;
		}
		// [[BPR-0515]]. Tests-enabled only, and deliberately so: this is the
		// one flag that changes where the shipped device-authorization wire
		// dials, and a released binary must not carry a way to redirect its
		// own egress from a command line. The string exists nowhere else, so
		// the RFC-BPR-0017 fixture-seam scan can prove its absence from a
		// tests-disabled Release by looking for it.
		//
		// What it buys is one gate's determinism.
		// `application_device_authorization_unavailable` is exempt from the
		// Windows AppContainer ([[BPR-0362]]), so on Windows nothing denied
		// the connect and the gate's expectation depended on reaching
		// github.com. With this the denial is the application's own, on every
		// platform, and the expectation is the local `failed` macOS and Linux
		// already recorded.
		config.NetworkDenied               = has_argument(argc, argv, "--network-denied");
		const auto testApplicationDataRoot = argument_value(
			argc, argv, "--test-application-data-root");
		if (testApplicationDataRoot.has_value()) {
			config.ApplicationDataRoot = std::filesystem::path{
				std::string{ *testApplicationDataRoot }
			};
		}
		auto fixtureName            = std::string{ };
		auto expectedActiveHostText = std::string{ };
		auto fixtureExpectations    = fixture_run_expectations{ };
		auto selectedFixture        = std::optional<
			bpr::app::detail::github_public_path_fixture>{ };
		auto fixtureOptions                = bpr::app::detail::github_public_path_fixture_options{ };
		fixtureOptions.Profile             = profileLaunch->Profile;
		fixtureOptions.ProfileLaunchIntent = profileLaunch->Intent;
		// Read here rather than beside the other device flag further down,
		// because the fixture options are assembled before that point.
		fixtureOptions.RegisteredDeviceCourse = has_argument(
			argc,
			argv,
			"--device-authorization-registered-course");
		// [[BPR-0536]]. The renewal course composes the registered grant and
		// two more seams: a real second vault slot and the documented renewal
		// answer. Read here for the same reason as the line above it.
		fixtureOptions.DeviceRefreshCourse = has_argument(
			argc,
			argv,
			"--device-authorization-refresh-course");
		// [[BPR-0540]]. The orphan course composes the renewal course's graph
		// -- the registered capability and the second vault slot -- and adds
		// the one thing no run of this process could produce: material already
		// in that slot when the run starts. Read here with the two above it
		// because the fixture options are assembled before the run flags.
		fixtureOptions.SeedOrphanedRefreshMaterial = has_argument(
			argc,
			argv,
			"--device-authorization-orphan-course");
		fixtureOptions.DeviceRefreshCourse = fixtureOptions.DeviceRefreshCourse
			|| fixtureOptions.SeedOrphanedRefreshMaterial;
		// [[BPR-0657]]. The threaded-reply course refreshes the pull request
		// after publishing, so the fixture replays its read script.
		fixtureOptions.ResolvesThreads = has_argument(argc, argv, "--threaded-reply-probe");
		// [[BPR-0932]]. Attended Home/Recent navigation reopens the same
		// recording through either public preview or the fixture account.
		fixtureOptions.RepeatReadSessions = fixtureOptions.ResolvesThreads
			|| has_argument(argc, argv, "--attended-launch");
		// The renewal course is the registered course plus a renewal, so it
		// composes the registered capability rather than a second one. Every
		// stage up to the vault store is the same graph, which is what makes
		// the rotation after it evidence about the shipped composition.
		fixtureOptions.RegisteredDeviceCourse
			= fixtureOptions.RegisteredDeviceCourse
			|| fixtureOptions.DeviceRefreshCourse;
		// RFC-BPR-0016 construction cut. Composes the device host with no
		// browser capability, which is what every build had before
		// [[BPR-0516]] composed one.
		fixtureOptions.PublicPreviewEnabledCourse
			= has_argument(argc, argv, "--public-preview-enabled-course");
		fixtureOptions.PublicPreviewGhesCourse
			= has_argument(argc, argv, "--public-preview-ghes-course");
		fixtureOptions.TimelineCourse = argument_value(argc, argv, "--public-preview-history-course").value_or("");
		fixtureOptions.EmptyHistoricalTimeline
			= has_argument(argc, argv, "--public-preview-empty-timeline-course");
		fixtureOptions.UnfetchableFirstHistoricalHead
			= has_argument(argc, argv, "--public-preview-unfetchable-first-head-course");
		// [[BPR-1083]]. The label-authoring course types `:qu:`, so it needs
		// the helper, and reads the pull request again after publishing.
		const auto labelAuthoringProbe    = has_argument(argc, argv, "--label-authoring-probe");
		fixtureOptions.LabelHelper        = has_argument(argc, argv, "--label-helper") || labelAuthoringProbe;
		fixtureOptions.RepeatReadSessions = fixtureOptions.RepeatReadSessions || labelAuthoringProbe;
		// [[BPR-1331]]. The private progress course across a push: `before`
		// on the first head, `after` on the pushed one, over one data root.
		const auto reviewProgressPushProbe = argument_value(argc, argv, "--review-progress-push-probe");
		// [[BPR-1326]]. The file-switch course reads again after publishing.
		const auto publishedFileSwitchProbe  = has_argument(argc, argv, "--published-file-switch-probe");
		const auto publishedFileSwitchTiming = argument_value(argc, argv, "--published-file-switch-timing");
		fixtureOptions.RepeatReadSessions    = fixtureOptions.RepeatReadSessions || publishedFileSwitchProbe;
		// [[BPR-1345]]. The start-a-review course reads again after publishing.
		const auto startReviewCourse      = argument_value(argc, argv, "--start-review-course");
		fixtureOptions.RepeatReadSessions = fixtureOptions.RepeatReadSessions || startReviewCourse.has_value();
		// [[BPR-1374]]. The unsent-reply course reads again on its Refresh.
		const auto replyDraftCourseName = argument_value(argc, argv, "--reply-draft-course");
		const auto replyDraftCourse     = [&]() -> std::optional<bpr::app::detail::reply_draft_course> {
			using course = bpr::app::detail::reply_draft_course;
			for (const auto& [name, value] : std::array{ std::pair{ std::string_view{ "refresh" }, course::refresh }, std::pair{ std::string_view{ "edited" }, course::edited },
														 std::pair{ std::string_view{ "write" }, course::write }, std::pair{ std::string_view{ "restart" }, course::restart },
														 std::pair{ std::string_view{ "deleted" }, course::deleted }, std::pair{ std::string_view{ "kept" }, course::kept },
														 std::pair{ std::string_view{ "resolved" }, course::resolved } }) {
				if (replyDraftCourseName == std::optional{ name }) {
					return value;
				}
			}
			return std::nullopt;
		}();
		fixtureOptions.RepeatReadSessions    = fixtureOptions.RepeatReadSessions || replyDraftCourse.has_value();
		fixtureOptions.SuppressDeviceBrowser = has_argument(
			argc,
			argv,
			"--device-authorization-browser-absent");
		// [[BPR-0526]] construction cut. The window still mounts the device
		// sign-in panel; composition adopts neither of its edges, so it is
		// drawn and inert while the flow stays reachable through this
		// self-test.
		fixtureOptions.DisableDeviceAuthorizationPanelComposition = has_argument(
			argc,
			argv,
			"--device-authorization-panel-absent");
		if (const auto fixture = argument_value(argc, argv, "--github-fixture");
			fixture.has_value()) {
			if (*fixture == "github-com") {
				selectedFixture        = bpr::app::detail::github_public_path_fixture::github_com;
				fixtureName            = "github.com";
				expectedActiveHostText = "github.com";
				fixtureExpectations    = synthetic_fixture_expectations();
			} else if (*fixture == "ghes-3.18") {
				selectedFixture        = bpr::app::detail::github_public_path_fixture::ghes_3_18;
				fixtureName            = "ghes-3.18";
				expectedActiveHostText = "ghe.example.invalid";
				fixtureExpectations    = synthetic_fixture_expectations();
			} else if (*fixture == "uiforetw-74") {
				selectedFixture = bpr::app::detail::github_public_path_fixture::uiforetw_74;
				fixtureName     = "uiforetw-74";
				// The recorded pull request is on github.com, so the active-host
				// text is github.com's. Only the served pull request differs.
				expectedActiveHostText = "github.com";
				fixtureExpectations    = {
					.Owner               = "google",
					.Repository          = "UIforETW",
					.Number              = 74,
					.ExpectedTitle       = "add CLR runtime tracing support",
					.ExpectedAccountText = "MikeMarcin (@MikeMarcin)",
					// Measured against the recording, not assumed: it carries no
					// review, no review thread and no issue comment, and the
					// conversation model's roots are exactly those three kinds.
					.ExpectedConversationRows = 0,
					.ExpectedThreadId         = std::nullopt,
				};
			} else if (*fixture == "nested-folders-v1") {
				// [[BPR-1213]]. Constructed, not recorded: several folders,
				// nested subfolders and a root file, for judging the Overview's
				// folder rows. It carries no review, thread or comment.
				selectedFixture        = bpr::app::detail::github_public_path_fixture::nested_folders_v1;
				fixtureName            = "nested-folders-v1";
				expectedActiveHostText = "github.com";
				fixtureExpectations    = {
					.Owner                    = "bpr-fixtures",
					.Repository               = "nested-folders",
					.Number                   = 21,
					.ExpectedTitle            = "Move renderer, shaders and widgets into nested folders",
					.ExpectedAccountText      = "fixture-author (@fixture-author)",
					.ExpectedConversationRows = 0,
					.ExpectedThreadId         = std::nullopt,
				};
			} else if (*fixture == "line-details-v1") {
				// [[BPR-1334]]. Constructed long changed lines for judging Line
				// details; see `scripts/build_line_details_fixture.py`.
				selectedFixture        = bpr::app::detail::github_public_path_fixture::line_details_v1;
				fixtureName            = "line-details-v1";
				expectedActiveHostText = "github.com";
				fixtureExpectations    = {
					.Owner                    = "bpr-fixtures",
					.Repository               = "line-details",
					.Number                   = 34,
					.ExpectedTitle            = "Explain export failures in one sentence and widen the weekly totals",
					.ExpectedAccountText      = "fixture-author (@fixture-author)",
					.ExpectedConversationRows = 0,
					.ExpectedThreadId         = std::nullopt,
				};
			} else if (*fixture == "conventional-comments-v1") {
				// [[BPR-1083]]. The nested-folders pull request with labelled
				// review threads; see `scripts/build_conventional_comments_fixture.py`.
				selectedFixture        = bpr::app::detail::github_public_path_fixture::conventional_comments_v1;
				fixtureName            = "conventional-comments-v1";
				expectedActiveHostText = "github.com";
				fixtureExpectations    = {
					.Owner                    = "bpr-fixtures",
					.Repository               = "nested-folders",
					.Number                   = 21,
					.ExpectedTitle            = "Move renderer, shaders and widgets into nested folders",
					.ExpectedAccountText      = "fixture-author (@fixture-author)",
					.ExpectedConversationRows = 6,
					.ExpectedThreadId         = std::nullopt,
				};
			} else if (*fixture == "overview-labels-v1") {
				// [[BPR-1089]]. The nested-folders pull request with many
				// labelled conversations; see `scripts/build_overview_labels_fixture.py`.
				selectedFixture        = bpr::app::detail::github_public_path_fixture::overview_labels_v1;
				fixtureName            = "overview-labels-v1";
				expectedActiveHostText = "github.com";
				fixtureExpectations    = {
					.Owner                    = "bpr-fixtures",
					.Repository               = "nested-folders",
					.Number                   = 21,
					.ExpectedTitle            = "Move renderer, shaders and widgets into nested folders",
					.ExpectedAccountText      = "fixture-author (@fixture-author)",
					.ExpectedConversationRows = 39,
					.ExpectedThreadId         = std::nullopt,
				};
			} else if (*fixture == "inline-drawer-v1") {
				// [[BPR-1290]]. conventional-comments-v1's threads plus a long
				// file with eight; see `scripts/build_inline_drawer_fixture.py`.
				selectedFixture        = bpr::app::detail::github_public_path_fixture::inline_drawer_v1;
				fixtureName            = "inline-drawer-v1";
				expectedActiveHostText = "github.com";
				// Fourteen threads, each its root then its posts in order.
				auto identities = std::vector<std::string>{ };
				for (auto thread = 1; const auto posts : { 4, 3, 2, 2, 2, 2, 3, 1, 4, 5, 8, 3, 3, 2 }) {
					identities.push_back("review-thread:PRRT_cc_" + std::to_string(thread));
					for (auto post = 1; post <= posts; ++post) {
						identities.push_back("review-comment:PRRC_cc_" + std::to_string(thread) + "_" + std::to_string(post));
					}
					++thread;
				}
				fixtureExpectations = {
					.Owner                          = "bpr-fixtures",
					.Repository                     = "inline-drawer",
					.Number                         = 22,
					.ExpectedTitle                  = "Keep the conversation cards in the ui folder",
					.ExpectedAccountText            = "fixture-author (@fixture-author)",
					.ExpectedConversationRows       = 14,
					.ExpectedConversationIdentities = std::move(identities),
					.ExpectedThreadId               = std::nullopt,
				};
			} else if (*fixture == "conversations-v1") {
				// [[BPR-1293]]. Prototype v2's pull request; see
				// `scripts/build_conversations_fixture.py`.
				selectedFixture        = bpr::app::detail::github_public_path_fixture::conversations_v1;
				fixtureName            = "conversations-v1";
				expectedActiveHostText = "github.com";
				fixtureExpectations    = {
					.Owner                    = "bpr-fixtures",
					.Repository               = "conversations",
					.Number                   = 418,
					.ExpectedTitle            = "Keep the selected conversation across a rebuild",
					.ExpectedAccountText      = "fixture-author (@fixture-author)",
					.ExpectedConversationRows = 21,
					.ExpectedThreadId         = std::nullopt,
				};
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
		// Seed, reuse, and every maintenance probe compare a second head against
		// a first. A fixture that records one head cannot run a reduced version
		// of those courses, so the pairing is refused here rather than degraded
		// into the single-head course, which would print a phase marker for a
		// phase the run never took. Asked of the serving descriptor, so a
		// fixture added later answers for itself.
		if (selectedFixture.has_value()
			&& (fixtureOptions.EpochPhase
					!= bpr::app::detail::github_epoch_fixture_phase::single
				|| epochMaintenanceProbe.has_value())
			&& !bpr::app::detail::
				   github_public_path_fixture_serves_second_head(
					   *selectedFixture)) {
			QTextStream{ stdout } << "EPOCH_FIXTURE_CONFIGURATION_FAILED\n";
			return EXIT_FAILURE;
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

		const auto selfTest = has_argument(argc, argv, "--self-test");
		const auto deviceProbe
			= has_argument(argc, argv, "--device-authorization-probe");
		// [[BPR-0537]]. Tests-enabled only, like the browser probe below.
		// In a released image this flag suppressed the device probe's own
		// cancel, so `--self-test --device-authorization-probe
		// --device-authorization-registered-course` polled GitHub until the
		// run budget expired. Every registration that passes it is a
		// tests-enabled row, so gating it costs no course; what it buys is
		// the string's absence from a tests-disabled image, which is how the
		// RFC-BPR-0016 inventory scan proves the flag is gone rather than
		// merely inert.
		const auto registeredDeviceCourse =
#if defined(BPR_BUILD_TESTING)
			has_argument(argc, argv, "--device-authorization-registered-course");
#else
			false;
#endif
		// [[BPR-0536]]. Tests-enabled only, exactly like the registered course
		// it extends: it forces a registered capability against the recorded
		// fixture, and a released image must not be able to do that from a
		// command line. Its absence from a tests-disabled binary is what the
		// RFC-BPR-0016 inventory scan reads.
		const auto deviceRefreshCourse =
#if defined(BPR_BUILD_TESTING)
			has_argument(argc, argv, "--device-authorization-refresh-course");
#else
			false;
#endif
		// [[BPR-0540]]. Tests-enabled only, exactly like the renewal course
		// whose composition it borrows: it forces a registered capability and
		// seeds a vault slot against the recorded fixture, and a released
		// image must not be able to do either from a command line. Its
		// absence from a tests-disabled binary is what the RFC-BPR-0016
		// inventory scan reads.
		const auto deviceOrphanCourse =
#if defined(BPR_BUILD_TESTING)
			has_argument(argc, argv, "--device-authorization-orphan-course");
#else
			false;
#endif
		// Tests-enabled only. A shipped binary opens a browser when the
		// reviewer asks it to and at no other time: the stations run the
		// release courses unattended, and a command-line flag that could start
		// a browser in one of those runs is a failure nobody would be watching
		// for. The composed opener is proven in a shipped graph by linkage and
		// construction instead.
		const auto deviceBrowserProbe =
#if defined(BPR_BUILD_TESTING)
			has_argument(argc, argv, "--device-authorization-browser-probe");
#else
			false;
#endif
		// [[BPR-0797]]. Tests-enabled only, like every other probe here, and
		// for a plainer reason than most: it exists to be run with the
		// automated test boundary taken away, which is the configuration a
		// person's own application runs in, and a released image has no
		// boundary to take away.
		const auto capabilityProbe =
#if defined(BPR_BUILD_TESTING)
			has_argument(argc, argv, "--composition-capability-probe");
#else
			false;
#endif
		// [[BPR-0526]]. Starts the flow through the mounted panel rather than
		// through `select_device_authorization`, which is the only difference
		// between this course and the registered one beside it, and
		// tests-enabled only for the same reason ([[BPR-0537]]).
		const auto devicePanelCourse =
#if defined(BPR_BUILD_TESTING)
			has_argument(argc, argv, "--device-authorization-panel-course");
#else
			false;
#endif
		// [[BPR-0535]]. Tests-enabled only, like the forced courses below.
		// It drives the picker onto an enterprise profile and presses the
		// mounted panel's begin, which is a reviewer's gesture; a shipped
		// binary offers it from the window and from no command line.
		const auto deviceGhesCourse =
#if defined(BPR_BUILD_TESTING)
			has_argument(argc, argv, "--device-authorization-ghes-course");
#else
			false;
#endif
		// RFC-BPR-0017. Unconditional, exactly like the device probe beside
		// it: a shipped binary reports its selected signed-out mode and sends
		// nothing merely by activating it. A flag that
		// existed only in a tests-enabled build could not make it.
		const auto publicPreviewProbe
			= has_argument(argc, argv, "--public-preview-probe");
		// The two forced courses are tests-enabled only. They override the
		// shipped selection, which is the one thing a released binary must
		// never be able to do from a command line.
		const auto publicPreviewEnabledCourse =
#if defined(BPR_BUILD_TESTING)
			has_argument(argc, argv, "--public-preview-enabled-course");
#else
			false;
#endif
		const auto publicPreviewHistoricalCourse =
#if defined(BPR_BUILD_TESTING)
			has_argument(argc, argv, "--public-preview-historical-course");
#else
			false;
#endif
		const auto publicPreviewDragCourse =
#if defined(BPR_BUILD_TESTING)
			has_argument(argc, argv, "--public-preview-handle-drag-course");
#else
			false;
#endif
		const auto publicPreviewGhesCourse =
#if defined(BPR_BUILD_TESTING)
			has_argument(argc, argv, "--public-preview-ghes-course");
#else
			false;
#endif
		// [[BPR-0316]]. The forced selection with no fixture underneath it.
		// Tests-enabled only, for the reason the two courses above are: a
		// released binary must never be able to override its own selection
		// from a command line. `--public-preview-selection-enabled-probe`
		// activates the mode and stops, which is what a shipped build whose
		// selection is enabled does; the live candidate additionally opens the
		// named pull request through the production anonymous authority, and
		// its dry-run twin reports the same wiring having issued nothing.
		auto publicPreviewSelectionForced = false;
		auto publicPreviewLiveCandidate   = false;
		auto publicPreviewLiveDryRun      = false;
#if defined(BPR_BUILD_TESTING)
		publicPreviewLiveDryRun = has_argument(
			argc, argv, "--public-preview-live-candidate-dry-run");
		if (const auto candidate = argument_values(
				argc, argv, "--public-preview-live-candidate", 3);
			candidate.has_value()) {
			auto              number = 0;
			const auto* const first  = (*candidate)[2U].data();
			const auto* const last   = first + (*candidate)[2U].size();
			const auto        parsed = std::from_chars(first, last, number);
			if (parsed.ec != std::errc{ } || parsed.ptr != last || number <= 0
				|| (*candidate)[0U].empty() || (*candidate)[1U].empty()) {
				QTextStream{ stdout }
					<< "PUBLIC_PREVIEW_LIVE_CANDIDATE_CONFIGURATION_FAILED\n";
				return EXIT_FAILURE;
			}
			publicPreviewLiveCandidate = true;
			// The same three fields the fixture courses drive the session
			// panel from, so the live candidate opens through the shipped
			// control rather than through a path of its own.
			fixtureExpectations.Owner.assign((*candidate)[0U]);
			fixtureExpectations.Repository.assign((*candidate)[1U]);
			fixtureExpectations.Number = number;
		}
		if (publicPreviewLiveDryRun && !publicPreviewLiveCandidate) {
			QTextStream{ stdout }
				<< "PUBLIC_PREVIEW_LIVE_CANDIDATE_CONFIGURATION_FAILED\n";
			return EXIT_FAILURE;
		}
		publicPreviewSelectionForced = publicPreviewLiveCandidate
			|| has_argument(
										   argc, argv, "--public-preview-selection-enabled-probe");
		// Refused here, before a graph exists and so before anything could be
		// sent. A profile that is not GitHub.com has no anonymous read
		// authority at all; a fixture would replace the production factories
		// this is composed to exercise; a course flag would override the
		// selection a second time; and the run bound is mandatory, because
		// this is the one self-test whose requests leave the machine.
		if (publicPreviewSelectionForced
			&& (!selfTest || selectedFixture.has_value()
				|| enterpriseHostname.has_value() || publicPreviewEnabledCourse
				|| publicPreviewGhesCourse || deviceProbe
				|| registeredDeviceCourse
				|| !has_argument(argc, argv, "--automated-run-budget-ms"))) {
			QTextStream{ stdout }
				<< "PUBLIC_PREVIEW_LIVE_CANDIDATE_CONFIGURATION_FAILED\n";
			return EXIT_FAILURE;
		}
#endif
		const auto renderPath                 = argument_value(argc, argv, "--render-evidence");
		const auto renderCollapsesFilesDrawer = has_argument(
			argc, argv, "--render-evidence-collapse-files");

#if defined(BPR_BUILD_TESTING)
		// An explicit root without a fixture. Restart contracts pass it with
		// `--self-test` so a second process reuses the first one's stores, and
		// `runtime_process_surface` passes it to an interactive-shaped launch so
		// the idle application it inspects never reaches the default profile
		// ([[BPR-0820]]). It is accepted without `--self-test` because the flag
		// exists only in a tests-enabled build and is explicit rather than
		// silent: a launch is redirected only when its command line names the
		// root. A fixture still refuses it, because a fixture owns its root.
		if (testApplicationDataRoot.has_value() && selectedFixture.has_value()) {
			QTextStream{ stdout }
				<< "TEST_APPLICATION_DATA_ROOT_CONFIGURATION_FAILED\n";
			return EXIT_FAILURE;
		}
#endif

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
		const auto nativeAppearanceProbe            = has_argument(argc, argv, "--native-appearance-probe");
		const auto nativeAppearanceCutSelectionWire = has_argument(
			argc, argv, "--native-appearance-cut-selection-wire");
		const auto attendedLaunch = has_argument(argc, argv, "--attended-launch");
		// [[BPR-1338]]. A person is watching an attended look, so the window's
		// own opens reach their default browser. Never inside the automated
		// test boundary: a CTest row that passes `--attended-launch` keeps the
		// recorder.
		fixtureOptions.AttendedWindowBrowser = attendedLaunch && !bpr::app::inside_automated_test_boundary();
		const auto nativeNotificationProbe   = has_argument(
			argc, argv, "--native-notification-probe");
		const auto nativeNotificationAttended = has_argument(
			argc, argv, "--native-notification-attended");
		const auto nativeNotificationRequestCut = has_argument(
			argc, argv, "--native-notification-cut-request-wire");
		const auto nativeNotificationActivationCut = has_argument(
			argc, argv, "--native-notification-cut-activation-wire");
		const auto nativeNotificationSignalObservationCut = has_argument(
			argc, argv, "--native-notification-cut-signal-observation");
		const auto nativeNotificationCourse
			= nativeNotificationProbe || nativeNotificationAttended;
		fixtureOptions.CaptureSystemNotificationDisplay
			= nativeNotificationProbe && !nativeNotificationRequestCut;
		fixtureOptions.DisableNotificationRequestWire
			= nativeNotificationRequestCut;
		fixtureOptions.DisableNotificationSelectionWire
			= nativeNotificationActivationCut;
		const auto diffWorkbenchProbe         = has_argument(argc, argv, "--diff-workbench-probe");
		const auto diffWorkbenchShutdownProbe = has_argument(
			argc, argv, "--diff-workbench-shutdown-probe");
		const auto workbenchCompositionProbe = has_argument(
			argc, argv, "--workbench-composition-probe");
		const auto workbenchCompositionCutSelectionWire = has_argument(
			argc, argv, "--workbench-composition-cut-selection-wire");
		const auto uiforetwSelectedDiffDirectory = argument_value(
			argc, argv, "--uiforetw-selected-diff-evidence");
		const auto uiforetwSelectedDiffProbe
			= uiforetwSelectedDiffDirectory.has_value();
		const auto uiforetwSelectedDiffCutSelectionWire = has_argument(
			argc, argv, "--uiforetw-selected-diff-cut-selection-wire");
		fixtureOptions.DisableWorkbenchSelectionWire
			= workbenchCompositionCutSelectionWire
			|| uiforetwSelectedDiffCutSelectionWire
			|| nativeAppearanceCutSelectionWire;
		const auto reviewLedgerCompositionProbeName = argument_value(
			argc, argv, "--review-ledger-composition-probe");
		auto reviewLedgerCompositionProbe = std::optional<
			bpr::app::detail::review_ledger_composition_probe>{ };
		if (reviewLedgerCompositionProbeName == "seed") {
			reviewLedgerCompositionProbe
				= bpr::app::detail::review_ledger_composition_probe::seed;
		} else if (reviewLedgerCompositionProbeName == "reopen") {
			reviewLedgerCompositionProbe
				= bpr::app::detail::review_ledger_composition_probe::reopen;
		} else if (reviewLedgerCompositionProbeName == "unavailable") {
			reviewLedgerCompositionProbe
				= bpr::app::detail::review_ledger_composition_probe::unavailable;
		} else if (reviewLedgerCompositionProbeName.has_value()) {
			QTextStream{ stdout }
				<< "REVIEW_LEDGER_COMPOSITION_CONFIGURATION_FAILED\n";
			return EXIT_FAILURE;
		}
		const auto privateReviewCourse         = argument_value(argc, argv, "--private-review-course");
		const auto privateReviewRepresentation = argument_value(argc, argv, "--private-review-representation").value_or("native");
		if (privateReviewCourse) {
			const auto agent = argument_value(argc, argv, "--private-review-agent");
			if (!selectedFixture || !agent || (*privateReviewCourse != "seed" && *privateReviewCourse != "compose" && *privateReviewCourse != "publish") || (privateReviewRepresentation != "native" && privateReviewRepresentation != "file")) {
				return EXIT_FAILURE;
			}
			fixtureOptions.CodexAgentExecutable = std::filesystem::path{ std::string{ *agent } };
			fixtureOptions.RepeatReadSessions   = true;
		}
		const auto reviewRoundTripCompositionProbe = has_argument(
			argc, argv, "--review-round-trip-composition-probe");
		const auto threadedReplyProbe         = has_argument(argc, argv, "--threaded-reply-probe");
		const auto defectCompositionProbeName = argument_value(
			argc, argv, "--defect-composition-probe");
		auto defectCompositionProbe
			= std::optional<bpr::app::detail::defect_composition_probe>{ };
		if (defectCompositionProbeName == "seed") {
			defectCompositionProbe = bpr::app::detail::defect_composition_probe::seed;
		} else if (defectCompositionProbeName == "reopen") {
			defectCompositionProbe = bpr::app::detail::defect_composition_probe::reopen;
		} else if (defectCompositionProbeName.has_value()) {
			QTextStream{ stdout } << "DEFECT_COMPOSITION_CONFIGURATION_FAILED\n";
			return EXIT_FAILURE;
		}
		fixtureOptions.LoseFirstReviewMutationResult
			= reviewRoundTripCompositionProbe || (privateReviewCourse && has_argument(argc, argv, "--private-review-lose-first-result"));
		const auto appearanceGestureProbe = has_argument(
			argc, argv, "--appearance-gesture-probe");
		const auto overviewSinceReview = has_argument(
			argc, argv, "--overview-since-review");
		const auto reviewBehaviour                  = has_argument(argc, argv, "--review-behaviour");
		const auto savedServerCourse                = has_argument(argc, argv, "--saved-server");
		const auto appearanceThemeCustomizationPath = argument_value(
			argc, argv, "--appearance-theme-customization-probe");
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
		if (nativeAppearanceCutSelectionWire && !nativeAppearanceProbe) {
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
				|| workbenchCompositionProbe
				|| uiforetwSelectedDiffProbe
				|| appearanceGestureProbe
				|| appearanceThemeCustomizationPath.has_value()
				|| reviewRoundTripCompositionProbe
				|| defectCompositionProbe.has_value()
				|| automatedStallProbe
				|| epochMaintenanceProbe || nativeAppearanceProbe
				|| diffEvidenceDirectory.has_value())) {
			QTextStream{ stdout } << "ATTENDED_LAUNCH_CONFIGURATION_FAILED\n";
			return EXIT_FAILURE;
		}
		// [[BPR-1083]]. An attended look that should start on a file, signed in
		// with the fixture's account: without it the window opens on Home,
		// where the Home bar's public-preview reading is read-only.
		const auto attendedOpenFile = argument_value(argc, argv, "--attended-open-file");
		// [[BPR-1287]]. The same, stopping at the pull request's own opening
		// page, so a person can go to Review with no file chosen.
		const auto attendedOpenPullRequest = has_argument(argc, argv, "--attended-open-pull-request");
		// [[BPR-1338]]. Once the pull request is open, press its title row's
		// Open on GitHub once, so one native check needs no hand on the desktop.
		const auto attendedOpenOnGithub = has_argument(argc, argv, "--attended-open-on-github");
		// [[BPR-1289]]. Or opening Settings over Home, for a look at Settings.
		const auto attendedOpenSettings = has_argument(argc, argv, "--attended-open-settings");
		// [[BPR-1084]] and [[BPR-1085]]. Or, on conventional-comments-v1 after
		// the file, a seeded suggestion draft and, for `review`, the preflight.
		const auto attendedPreflightLook = argument_value(argc, argv, "--attended-preflight-look");
		// [[BPR-1293]]. On conversations-v1 after the file, its read state.
		const auto attendedReadState = has_argument(argc, argv, "--attended-read-state");
		// [[BPR-1086]]. On conventional-comments-v1 after the file, renderer.cpp's
		// line 12 thread as a published tour stop whose narrative is labelled.
		const auto attendedTourStop = has_argument(argc, argv, "--attended-tour-stop");
		// [[BPR-1091]]. After the file, a workshop proposal from the fixture
		// agent named here, with its candidate line.
		const auto attendedWorkshopAgent = argument_value(argc, argv, "--attended-workshop-agent");
		if (attendedWorkshopAgent.has_value()) {
			fixtureOptions.CodexAgentExecutable = std::filesystem::path{ std::string{ *attendedWorkshopAgent } };
			fixtureOptions.LabelHelper          = true;
		}
		// [[BPR-1357]]. A slow network for a pull request's first answers, and
		// a pull request whose reads fail after it; with `--attended-home-look`
		// it is armed only once Home is ready for the person.
		const auto attendedHomeLook = has_argument(argc, argv, "--attended-home-look");
		const auto parse_count      = [](const std::optional<std::string_view> aText) -> std::optional<std::uint32_t> {
			if (!aText.has_value()) {
				return std::nullopt;
			}
			auto       value  = std::uint32_t{ 0U };
			const auto parsed = std::from_chars(aText->data(), aText->data() + aText->size(), value);
			return parsed.ec == std::errc{ } && parsed.ptr == aText->data() + aText->size() ? std::optional{ value } : std::nullopt;
		};
		const auto readDelayText    = argument_value(argc, argv, "--fixture-read-delay-ms");
		const auto readFailText     = argument_value(argc, argv, "--fixture-read-fail-number");
		const auto readDelay        = parse_count(readDelayText);
		const auto readFailNumber   = parse_count(readFailText);
		auto       readDelayFixture = std::shared_ptr<bpr::app::detail::github_read_delay_fixture>{ };
		if ((readDelayText.has_value() && !readDelay.has_value()) || (readFailText.has_value() && !readFailNumber.has_value())
			|| (attendedHomeLook && (!attendedLaunch || attendedOpenPullRequest || attendedOpenSettings || attendedPreflightLook.has_value() || attendedTourStop || attendedWorkshopAgent.has_value() || attendedReadState))) {
			QTextStream{ stdout } << "ATTENDED_LAUNCH_CONFIGURATION_FAILED\n";
			return EXIT_FAILURE;
		}
		if (readDelay.has_value() || readFailNumber.has_value()) {
			readDelayFixture               = std::make_shared<bpr::app::detail::github_read_delay_fixture>();
			readDelayFixture->Milliseconds = attendedHomeLook ? 0U : readDelay.value_or(0U);
			readDelayFixture->FailNumber   = readFailNumber.value_or(0U);
			fixtureOptions.ReadDelay       = readDelayFixture;
		}
		if (attendedHomeLook) {
			// Home's rows: the fixture's own pull request asks for my review,
			// and one of mine waits on a reviewer; the second is the one whose
			// reads fail when `--fixture-read-fail-number` names it.
			fixtureOptions.RepeatReadSessions                       = true;
			fixtureOptions.ReviewSubmissionServer                   = std::make_shared<bpr::app::detail::review_submission_fixture_state>();
			fixtureOptions.ReviewSubmissionServer->DraftPullRequest = true;
			fixtureOptions.PersonalDiscovery                        = bpr::app::detail::github_personal_discovery_fixture{
				.OpenPullRequests = {
					{ .Number = 4002U, .Author = "contributor-a", .RequestedUsers = { "octo-reviewer" }, .Draft = std::make_shared<std::atomic<bool>>(false) },
					{ .Number = readFailNumber.value_or(4001U), .Author = "octo-reviewer", .RequestedUsers = { "alice" }, .Draft = std::make_shared<std::atomic<bool>>(true) },
					{ .Number = 4003U, .Author = "octo-reviewer", .Draft = std::make_shared<std::atomic<bool>>(true) },
				},
			};
		}
		if ((attendedOpenFile.has_value() && (!attendedLaunch || attendedOpenFile->empty()))
			|| (attendedOpenPullRequest && (!attendedLaunch || attendedOpenFile.has_value()))
			|| (attendedOpenOnGithub && !attendedOpenPullRequest)
			|| (attendedOpenSettings && (!attendedLaunch || attendedOpenFile.has_value() || attendedOpenPullRequest))
			|| (attendedPreflightLook.has_value()
				&& (!attendedOpenFile.has_value() || fixtureName != "conventional-comments-v1"
					|| (*attendedPreflightLook != "native" && *attendedPreflightLook != "text-only" && *attendedPreflightLook != "review")))
			|| (attendedReadState && (!attendedOpenFile.has_value() || fixtureName != "conversations-v1"))
			|| (attendedTourStop && (!attendedOpenFile.has_value() || fixtureName != "conventional-comments-v1" || attendedPreflightLook.has_value()))
			|| (attendedWorkshopAgent.has_value() && (!attendedOpenFile.has_value() || attendedWorkshopAgent->empty() || attendedPreflightLook.has_value() || attendedTourStop))) {
			QTextStream{ stdout } << "ATTENDED_LAUNCH_CONFIGURATION_FAILED\n";
			return EXIT_FAILURE;
		}
		const auto attendedPullRequest      = QStringLiteral("%1/%2#%3").arg(QString::fromStdString(fixtureExpectations.Owner), QString::fromStdString(fixtureExpectations.Repository), QString::number(fixtureExpectations.Number));
		const auto labelAuthoringEvidence   = argument_value(argc, argv, "--label-authoring-evidence");
		const auto lineDraftCourse          = has_argument(argc, argv, "--line-draft-course");
		const auto cancelFirstBaseNumbering = has_argument(argc, argv, "--cancel-first-base-numbering");
		if ((labelAuthoringProbe && (fixtureName != "conventional-comments-v1" || attendedLaunch || selfTest || renderPath.has_value()))
			|| (labelAuthoringEvidence.has_value() && !labelAuthoringProbe) || (lineDraftCourse && !labelAuthoringProbe)
			|| (cancelFirstBaseNumbering && !lineDraftCourse)) {
			QTextStream{ stdout } << "LABEL_AUTHORING_CONFIGURATION_FAILED\n";
			return EXIT_FAILURE;
		}
		if ((publishedFileSwitchProbe && (fixtureName != "conventional-comments-v1" || attendedLaunch || selfTest || renderPath.has_value() || labelAuthoringProbe))
			|| (publishedFileSwitchTiming.has_value() && (!publishedFileSwitchProbe || (*publishedFileSwitchTiming != "settled" && *publishedFileSwitchTiming != "late" && *publishedFileSwitchTiming != "draft")))) {
			QTextStream{ stdout } << "PUBLISHED_FILE_SWITCH_CONFIGURATION_FAILED\n";
			return EXIT_FAILURE;
		}
		// [[BPR-1331]]. `before` runs on the first head (`single`), `after`
		// on the pushed one (`reuse`), both over one epoch data root.
		if (reviewProgressPushProbe.has_value()
			&& (fixtureName != "conventional-comments-v1" || attendedLaunch || selfTest || renderPath.has_value() || labelAuthoringProbe || publishedFileSwitchProbe
				|| !epochFixtureRoot.has_value()
				|| !((*reviewProgressPushProbe == "before" && fixtureOptions.EpochPhase == bpr::app::detail::github_epoch_fixture_phase::single)
					 || (*reviewProgressPushProbe == "after" && fixtureOptions.EpochPhase == bpr::app::detail::github_epoch_fixture_phase::reuse)))) {
			QTextStream{ stdout } << "REVIEW_PROGRESS_PUSH_CONFIGURATION_FAILED\n";
			return EXIT_FAILURE;
		}
		if ((nativeNotificationProbe && nativeNotificationAttended)
			|| (nativeNotificationRequestCut
				&& nativeNotificationActivationCut)
			|| ((nativeNotificationRequestCut
				 || nativeNotificationActivationCut)
				&& !nativeNotificationProbe)
			|| (nativeNotificationCourse
				&& (!selectedFixture.has_value()
					|| *selectedFixture
						!= bpr::app::detail::
							github_public_path_fixture::github_com
					|| fixtureOptions.EpochPhase
						!= bpr::app::detail::github_epoch_fixture_phase::seed
					|| attendedLaunch || selfTest || renderPath.has_value()
					|| nativeAppearanceProbe || diffWorkbenchProbe
					|| diffWorkbenchShutdownProbe
					|| workbenchCompositionProbe
					|| uiforetwSelectedDiffProbe
					|| appearanceGestureProbe
					|| appearanceThemeCustomizationPath.has_value()
					|| reviewRoundTripCompositionProbe
					|| defectCompositionProbe.has_value()
					|| automatedStallProbe || epochMaintenanceProbe
					|| diffEvidenceDirectory.has_value()))) {
			QTextStream{ stdout }
				<< "NATIVE_NOTIFICATION_CONFIGURATION_FAILED\n";
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
		if ((workbenchCompositionProbe
			 && (!selectedFixture.has_value()
				 || *selectedFixture
					 != bpr::app::detail::
						 github_public_path_fixture::github_com
				 || !epochFixtureRoot.has_value()
				 || fixtureOptions.EpochPhase
					 != bpr::app::detail::github_epoch_fixture_phase::single
				 || diffWorkbenchProbe || diffWorkbenchShutdownProbe
				 || nativeAppearanceProbe || appearanceGestureProbe
				 || appearanceThemeCustomizationPath.has_value()
				 || epochMaintenanceProbe.has_value()
				 || automatedStallProbe || defectCompositionProbe.has_value()))
			|| (workbenchCompositionCutSelectionWire
				&& !workbenchCompositionProbe)) {
			QTextStream{ stdout }
				<< "WORKBENCH_COMPOSITION_CONFIGURATION_FAILED\n";
			return EXIT_FAILURE;
		}
		if ((uiforetwSelectedDiffProbe
			 && (!selectedFixture.has_value()
				 || *selectedFixture
					 != bpr::app::detail::github_public_path_fixture::uiforetw_74
				 || !epochFixtureRoot.has_value()
				 || fixtureOptions.EpochPhase
					 != bpr::app::detail::github_epoch_fixture_phase::single
				 || !diffEvidenceAppearanceName.has_value()
				 || !diffEvidenceAppearance->has_value()
				 || workbenchCompositionProbe || diffWorkbenchProbe
				 || diffWorkbenchShutdownProbe || nativeAppearanceProbe
				 || appearanceGestureProbe
				 || appearanceThemeCustomizationPath.has_value()
				 || epochMaintenanceProbe.has_value() || automatedStallProbe
				 || reviewLedgerCompositionProbe.has_value()
				 || reviewRoundTripCompositionProbe
				 || defectCompositionProbe.has_value()))
			|| (uiforetwSelectedDiffCutSelectionWire
				&& !uiforetwSelectedDiffProbe)) {
			QTextStream{ stdout }
				<< "UIFORETW_SELECTED_DIFF_CONFIGURATION_FAILED\n";
			return EXIT_FAILURE;
		}
		if (reviewLedgerCompositionProbe.has_value()
			&& (!selectedFixture.has_value()
				|| *selectedFixture
					!= bpr::app::detail::github_public_path_fixture::github_com
				|| !epochFixtureRoot.has_value()
				|| (*reviewLedgerCompositionProbe
							== bpr::app::detail::review_ledger_composition_probe::seed
						? fixtureOptions.EpochPhase
							!= bpr::app::detail::github_epoch_fixture_phase::seed
						: *reviewLedgerCompositionProbe
							== bpr::app::detail::review_ledger_composition_probe::reopen
						? fixtureOptions.EpochPhase
							!= bpr::app::detail::github_epoch_fixture_phase::reuse
						: fixtureOptions.EpochPhase
							!= bpr::app::detail::github_epoch_fixture_phase::single)
				|| workbenchCompositionProbe || diffWorkbenchProbe
				|| diffWorkbenchShutdownProbe || nativeAppearanceProbe
				|| appearanceGestureProbe
				|| appearanceThemeCustomizationPath.has_value()
				|| epochMaintenanceProbe.has_value() || automatedStallProbe
				|| defectCompositionProbe.has_value())) {
			QTextStream{ stdout }
				<< "REVIEW_LEDGER_COMPOSITION_CONFIGURATION_FAILED\n";
			return EXIT_FAILURE;
		}
		if (reviewRoundTripCompositionProbe
			&& (!selectedFixture.has_value()
				|| *selectedFixture
					!= bpr::app::detail::github_public_path_fixture::github_com
				|| !epochFixtureRoot.has_value()
				|| fixtureOptions.EpochPhase
					!= bpr::app::detail::github_epoch_fixture_phase::single
				|| reviewLedgerCompositionProbe.has_value()
				|| workbenchCompositionProbe || diffWorkbenchProbe
				|| diffWorkbenchShutdownProbe || nativeAppearanceProbe
				|| appearanceGestureProbe
				|| appearanceThemeCustomizationPath.has_value()
				|| epochMaintenanceProbe.has_value() || automatedStallProbe
				|| defectCompositionProbe.has_value())) {
			QTextStream{ stdout }
				<< "REVIEW_ROUND_TRIP_COMPOSITION_CONFIGURATION_FAILED\n";
			return EXIT_FAILURE;
		}
		if (defectCompositionProbe.has_value()
			&& (!selectedFixture.has_value()
				|| (*selectedFixture
						!= bpr::app::detail::github_public_path_fixture::github_com
					&& *selectedFixture
						!= bpr::app::detail::github_public_path_fixture::ghes_3_18)
				|| !epochFixtureRoot.has_value()
				|| (*defectCompositionProbe
							== bpr::app::detail::defect_composition_probe::seed
						? fixtureOptions.EpochPhase
							!= bpr::app::detail::github_epoch_fixture_phase::seed
						: fixtureOptions.EpochPhase
							!= bpr::app::detail::github_epoch_fixture_phase::reuse)
				|| reviewLedgerCompositionProbe.has_value()
				|| reviewRoundTripCompositionProbe || workbenchCompositionProbe
				|| diffWorkbenchProbe || diffWorkbenchShutdownProbe
				|| nativeAppearanceProbe || appearanceGestureProbe
				|| appearanceThemeCustomizationPath.has_value()
				|| epochMaintenanceProbe.has_value() || automatedStallProbe)) {
			QTextStream{ stdout } << "DEFECT_COMPOSITION_CONFIGURATION_FAILED\n";
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
		if (appearanceThemeCustomizationPath.has_value()
			&& (!diffWorkbenchProbe || !selectedFixture.has_value())) {
			QTextStream{ stdout }
				<< "APPEARANCE_THEME_CUSTOMIZATION_CONFIGURATION_FAILED\n";
			return EXIT_FAILURE;
		}
#endif

		bpr::app::github_application*              application = nullptr;
		std::unique_ptr<bpr::app::profile_desktop> desktop;
		auto                                       completed = [&](bpr::app::profile_desktop_startup_result aResult) {
			if (!aResult) {
				if (aResult.error() == bpr::app::github_application_startup_failure::activation_queued) {
					QTextStream{ stdout } << "PROFILE_ACTIVATION_QUEUED\n";
					return;
				}
				if (aResult.error() == bpr::app::github_application_startup_failure::activation_indeterminate) {
					QTextStream{ stdout } << "PROFILE_ACTIVATION_INDETERMINATE_NO_RETRY\n";
					desktop->quit(EXIT_FAILURE);
					return;
				}
				if (aResult.error() == bpr::app::github_application_startup_failure::activation_refused) {
					QTextStream{ stdout } << "PROFILE_ACTIVATION_REFUSED\n";
					desktop->quit(EXIT_FAILURE);
					return;
				}
#if defined(BPR_BUILD_TESTING)
				// [[BPR-0843]]. As for the exception below ([[BPR-0435]]): the
				// verdict on stdout stays the one word every reader matches, and
				// a developer build names the refusal on stderr. A tests-disabled
				// build compiles this away and says no more than before.
				if (aResult.error() != bpr::app::github_application_startup_failure::owner_busy) {
					bpr::app::detail::terminal_text_stream{ } << "STARTUP_FAILED_CAUSE: startup_failure=" << startup_failure_name(aResult.error()) << '\n';
					// [[BPR-0877]]. `registry` above is a catch-all arm, so the
					// profile refusal that actually fired follows on its own
					// line: the line above keeps the exact wording a contract
					// already matches.
					if (const auto refusal = bpr::app::profile_startup_refusal_name(); !refusal.empty()) {
						bpr::app::detail::terminal_text_stream{ } << "STARTUP_PROFILE_REFUSAL:" << QString::fromStdString(refusal) << '\n';
					}
					// [[BPR-0882]]. `unavailable` is what every step between
					// admission and the first window reports, so the arm that
					// refused, and what it caught, follow on their own line.
					if (const auto source = bpr::app::startup_failure_source(); !source.empty()) {
						bpr::app::detail::terminal_text_stream{ } << "STARTUP_FAILURE_SOURCE:" << QString::fromStdString(source) << '\n';
					}
				}
#endif
				QTextStream{ stdout } << (aResult.error() == bpr::app::github_application_startup_failure::owner_busy ? "PROFILE_ALREADY_OPEN\n" : "STARTUP_FAILED\n");
				desktop->quit(EXIT_FAILURE);
				return;
			}
			application = *aResult;
			if (automatedProfileRoot) {
				std::error_code error;
				const auto      rooted = std::filesystem::is_directory(*automatedProfileRoot, error) && !error;
				QTextStream{ stdout } << (rooted ? "AUTOMATED_PROFILE_PRIVATE_OK\n" : "AUTOMATED_PROFILE_PRIVATE_FAILED\n");
				if (!rooted) {
					desktop->quit(EXIT_FAILURE);
					return;
				}
			}
			try {
				application->show();

				auto       verifyNativePlatformVault = selfTest;
				auto       verifyCliEntry            = selfTest;
				const auto verifyDeviceAuthorization = selfTest && deviceProbe;
				const auto verifyPublicPreview       = selfTest
					&& (publicPreviewProbe || publicPreviewSelectionForced);
				const auto previewEnterprise = publicPreviewGhesCourse
					|| (publicPreviewProbe && enterpriseHostname.has_value());
#if defined(BPR_BUILD_TESTING)
				verifyNativePlatformVault = verifyNativePlatformVault && fixtureName.empty();
				verifyCliEntry            = verifyCliEntry && fixtureName.empty();
				// [[BPR-0797]]. The capability probe ends its run at the start
				// stage, so it never reaches the CLI entry course and must not
				// print that course's verdict as if it had.
				verifyCliEntry = verifyCliEntry && !capabilityProbe;
#endif
				// The person is the run. Everything else about the composition is
				// unchanged - same fixture-backed application, same window - so what
				// is inspected is what the automated courses drive.
				//
				// Guarded, not merely false, in a tests-disabled build: the flag does
				// not exist there and neither does the fixture it requires, so the
				// seam leaves no trace for the Release composition contract to find.
#if defined(BPR_BUILD_TESTING)
				const auto heldOpenForInspection         = attendedLaunch;
				const auto scheduleNativeNotificationRun = nativeNotificationCourse;
#else
				constexpr auto heldOpenForInspection         = false;
				constexpr auto scheduleNativeNotificationRun = false;
#endif
				const auto scheduleAutomatedRun = !heldOpenForInspection
					&& !scheduleNativeNotificationRun
					&& (selfTest || renderPath.has_value()
#if defined(BPR_BUILD_TESTING)
						|| !fixtureName.empty()
#endif
					);
#if defined(BPR_BUILD_TESTING)
				if (privateReviewCourse) {
					bpr::app::detail::schedule_private_review_course(*application, std::string{ *privateReviewCourse }, std::string{ privateReviewRepresentation });
				} else if (scheduleNativeNotificationRun) {
					schedule_native_notification_run(
						*application,
						std::make_shared<native_notification_run>(native_notification_run{
							.Budget        = automatedRunBudget,
							.Attended      = nativeNotificationAttended,
							.RequestCut    = nativeNotificationRequestCut,
							.ActivationCut = nativeNotificationActivationCut,
							.SignalObservationCut
							= nativeNotificationSignalObservationCut,
						}));
				} else
#endif
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
								.CapabilityProbe           = capabilityProbe,
								.Cli                       = automated_run::cli_entry_probe_state{
									.Enabled            = verifyCliEntry,
									.EnterpriseHostname = std::move(enterpriseHostname),
								},
								.Device = automated_run::device_authorization_probe_state{
									.Enabled       = verifyDeviceAuthorization,
									.Registered    = registeredDeviceCourse || deviceRefreshCourse,
									.BrowserProbe  = verifyDeviceAuthorization && deviceBrowserProbe,
									.PanelCourse   = verifyDeviceAuthorization && devicePanelCourse,
									.GhesCourse    = verifyDeviceAuthorization && deviceGhesCourse,
									.RefreshCourse = verifyDeviceAuthorization && deviceRefreshCourse,
									.OrphanCourse  = verifyDeviceAuthorization && deviceOrphanCourse,
								},
								.PublicPreview = automated_run::public_preview_probe_state{
									.Enabled          = verifyPublicPreview,
									.Course           = verifyPublicPreview && publicPreviewEnabledCourse,
									.HistoricalCourse = verifyPublicPreview && publicPreviewHistoricalCourse,
#if defined(BPR_BUILD_TESTING)
									.EmptyTimeline         = fixtureOptions.EmptyHistoricalTimeline || fixtureOptions.TimelineCourse == "failed" || fixtureOptions.TimelineCourse == "refused" || fixtureOptions.TimelineCourse == "capped",
									.MissingHistoricalBase = fixtureOptions.TimelineCourse == "missing-base",
									.RewrittenBase         = fixtureOptions.TimelineCourse == "rewritten-base" && fixtureOptions.EpochPhase == bpr::app::detail::github_epoch_fixture_phase::reuse,
#endif
									.DragCourse      = verifyPublicPreview && publicPreviewDragCourse,
									.Enterprise      = verifyPublicPreview && previewEnterprise,
									.SelectionForced = publicPreviewSelectionForced,
									.LiveCandidate   = publicPreviewLiveCandidate,
									.DryRun          = publicPreviewLiveDryRun,
								},
#if defined(BPR_BUILD_TESTING)
								.Fixture                         = !fixtureName.empty(),
								.WorkbenchCompositionProbe       = workbenchCompositionProbe,
								.UIforEtwSelectedDiffProbe       = uiforetwSelectedDiffProbe,
								.UIforEtwSelectedDiffDirectory   = uiforetwSelectedDiffDirectory,
								.ReviewLedgerCompositionProbe    = reviewLedgerCompositionProbe,
								.ReviewRoundTripCompositionProbe = reviewRoundTripCompositionProbe,
								.ThreadedReplyProbe              = threadedReplyProbe,
								.LabelAuthoringProbe             = labelAuthoringProbe,
								.LabelAuthoringEvidenceDirectory = labelAuthoringEvidence.has_value() ? std::optional<std::string>{ std::string{ *labelAuthoringEvidence } } : std::nullopt,
								.PublishedFileSwitchProbe        = publishedFileSwitchProbe,
								.PublishedFileSwitchTiming       = publishedFileSwitchTiming == std::optional<std::string_view>{ "late" } ? bpr::app::detail::published_file_switch_timing::late : publishedFileSwitchTiming == std::optional<std::string_view>{ "draft" } ? bpr::app::detail::published_file_switch_timing::draft
																																																																		   : bpr::app::detail::published_file_switch_timing::settled,
								.StartReviewCourse               = startReviewCourse == std::optional<std::string_view>{ "pending" } ? std::optional{ bpr::app::detail::start_review_course::pending } : startReviewCourse == std::optional<std::string_view>{ "finish" } ? std::optional{ bpr::app::detail::start_review_course::finish }
									: startReviewCourse == std::optional<std::string_view>{ "refused" }                                                                                                                                                                   ? std::optional{ bpr::app::detail::start_review_course::refused }
									: startReviewCourse == std::optional<std::string_view>{ "uncertain" }                                                                                                                                                                 ? std::optional{ bpr::app::detail::start_review_course::uncertain }
									: startReviewCourse == std::optional<std::string_view>{ "again" }                                                                                                                                                                     ? std::optional{ bpr::app::detail::start_review_course::again }
																																																																		  : std::nullopt,
								.ReplyDraftCourse                = replyDraftCourse,
								.ReviewProgressPushProbe         = reviewProgressPushProbe == std::optional<std::string_view>{ "before" } ? std::optional{ bpr::app::detail::review_progress_push_course::before } : reviewProgressPushProbe == std::optional<std::string_view>{ "after" } ? std::optional{ bpr::app::detail::review_progress_push_course::after }
																																																																						   : std::nullopt,
								.LineDraftCourse                 = lineDraftCourse,
								.CancelFirstBaseNumbering        = cancelFirstBaseNumbering,
								.DefectCompositionProbe          = defectCompositionProbe,
								.OverviewSinceReviewRequested    = overviewSinceReview,
								.ReviewBehaviourRequested        = reviewBehaviour,
								.SavedServerRequested            = savedServerCourse,
								.DiffProbe                       = diffWorkbenchProbe,
								.DiffShutdownProbe               = diffWorkbenchShutdownProbe,
								.StallProbe                      = automatedStallProbe,
								.ThemeCustomizationPath          = appearanceThemeCustomizationPath.has_value() ? std::optional<std::string>{ std::string{ *appearanceThemeCustomizationPath } } : std::nullopt,
								.AppearanceGestureProbe          = appearanceGestureProbe,
								.FixtureName                     = std::move(fixtureName),
								.ExpectedActiveHostText          = std::move(expectedActiveHostText),
								.Expectations                    = std::move(fixtureExpectations),
								.EpochPhase                      = fixtureOptions.EpochPhase,
								.MaintenanceProbe                = selectedMaintenanceProbe,
								.Appearance                      = automated_run::native_appearance_probe_state{
									.Enabled       = nativeAppearanceProbe,
									.EvidencePaths = nativeAppearancePaths.value_or(native_appearance_evidence_paths{ }),
								},
								.DiffEvidenceDirectory  = diffEvidenceDirectory,
								.DiffEvidenceAppearance = *diffEvidenceAppearance,
#endif
								.RenderPath                 = renderPath,
								.RenderCollapsesFilesDrawer = renderCollapsesFilesDrawer,
							}));
				}
#if defined(BPR_BUILD_TESTING)
				if (heldOpenForInspection && (attendedOpenFile.has_value() || attendedOpenPullRequest)) {
					auto look = std::optional<std::string>{ };
					if (attendedWorkshopAgent.has_value()) {
						look = std::string{ "workshop" };
					}
					if (attendedPreflightLook.has_value()) {
						look = std::string{ *attendedPreflightLook };
						// [[BPR-1085]]. Two of the fixture's threads become the
						// fixture account's own blocking feedback, in the
						// fixture's responses only, before the first read.
						bpr::app::detail::github_application_fixture_access::stage_server_root(*application, "PRRT_cc_3", "issue (blocking): 1280 by 720 should be a named constant before this ships.", std::nullopt, std::pair{ std::string{ "U_fixture-author" }, std::string{ "fixture-author" } });
						bpr::app::detail::github_application_fixture_access::stage_server_root(*application, "PRRT_cc_4", "todo (blocking): Rename Handler to OnClick while you are here.", std::nullopt, std::pair{ std::string{ "U_fixture-author" }, std::string{ "fixture-author" } });
					}
					if (attendedTourStop) {
						// [[BPR-1086]]. The pull request's author published a tour
						// stop on that thread, pinned source and heading first, and
						// labelled its narrative; the fixture's responses only,
						// before the first read.
						const std::array navigation{ bpr::core::tour_markdown_link{ "Previous", "https://github.com/bpr-fixtures/nested-folders/pull/21#discussion_r1" },
													 bpr::core::tour_markdown_link{ "Tour overview", "https://github.com/bpr-fixtures/nested-folders/pull/21#pullrequestreview-9" },
													 bpr::core::tour_markdown_link{ "Next", "https://github.com/bpr-fixtures/nested-folders/pull/21#discussion_r3" } };
						const auto       stop = bpr::core::render_tour_stop(
							"Why the width is clamped", 2,
							"issue (blocking): A zero-width view divides by zero in the aspect ratio below.\n\nThe clamp keeps the renderer safe until the caller checks the size itself.",
							navigation, "https://github.com/bpr-fixtures/nested-folders/blob/b20cb443428212543f5911e091a0ffeb407d23cf/src/engine/renderer.cpp#L10-L14");
						if (stop) {
							bpr::app::detail::github_application_fixture_access::stage_server_root(*application, "PRRT_cc_2", *stop, std::nullopt, std::pair{ std::string{ "U_fixture-author" }, std::string{ "fixture-author" } });
						}
						QTextStream{ stdout } << (stop ? "ATTENDED_TOUR_STOP_STAGED\n" : "ATTENDED_TOUR_STOP_FAILED\n");
					}
					if (attendedHomeLook) {
						schedule_attended_home_look(*application, attendedPullRequest, readDelayFixture ? readDelayFixture : std::make_shared<bpr::app::detail::github_read_delay_fixture>(), readDelay.value_or(0U));
					} else {
						schedule_attended_reveal(*application, std::string{ attendedOpenFile.value_or(std::string_view{ }) }, attendedPullRequest, std::move(look),
												 attendedReadState ? std::optional<std::string>{ fixtureName } : std::nullopt, attendedOpenOnGithub);
					}
				}
				if (heldOpenForInspection && attendedOpenSettings) {
					schedule_attended_settings(*application);
				}
#endif

			} catch (...) {
#if defined(BPR_BUILD_TESTING)
				// [[BPR-0882]]. The runtime and its window exist by now, so what
				// threw is this entry point's own show or run setup. The outer
				// handler's cause line never sees it, so it is named here, the
				// way the composition's own arms are.
				bpr::app::detail::note_startup_failure("entry-point-run-setup", std::current_exception());
				bpr::app::detail::terminal_text_stream{ } << "STARTUP_FAILURE_SOURCE:" << QString::fromStdString(bpr::app::startup_failure_source()) << '\n';
#endif
				QTextStream{ stdout } << "STARTUP_FAILED\n";
				desktop->quit(EXIT_FAILURE);
			}
		};
		// [[BPR-0820]]. The gate behind the private-profile seam above. Every
		// composition below that is not a fixture resolves an absent root to
		// the default application data root, which is a person's profile, so
		// an automated run reaching one without a root is refused here, before
		// a lease is taken or a store opened. A future flag or start path that
		// skipped the seam fails its course on every host rather than only on
		// one where a person's application happens to be open.
		if ((selfTest || renderPath.has_value())
			&& !config.ApplicationDataRoot.has_value()
#if defined(BPR_BUILD_TESTING)
			&& !selectedFixture.has_value()
#endif
		) {
			QTextStream{ stdout } << "AUTOMATED_PROFILE_PRIVATE_FAILED\n";
			return EXIT_FAILURE;
		}
		desktop =
#if defined(BPR_BUILD_TESTING)
			selectedFixture                ? bpr::app::detail::github_application_fixture_access::start_desktop(*selectedFixture, fixtureOptions, completed)
			: publicPreviewSelectionForced ? bpr::app::detail::github_application_fixture_access::start_desktop_public_preview_selection_available(std::move(config), completed)
										   :
#endif
										   bpr::app::profile_desktop::start(std::move(config), completed);

		QTimer desktopPoll;
		QObject::connect(&desktopPoll, &QTimer::timeout, &qtApplication, [&desktop] { if (desktop->is_quiescent()) { QCoreApplication::exit(desktop->exit_code()); } });
		desktopPoll.start(10);
		return QApplication::exec();
	} catch (const std::exception& error) {
#if defined(BPR_BUILD_TESTING)
		// The verdict on stdout stays one word. A developer build says on
		// stderr what threw, because the first Windows Qt-graph run spent
		// thirty-five tests on a startup failure nothing could name
		// (BPR-0435). A tests-disabled build says nothing more, as before.
		bpr::app::detail::terminal_text_stream{ } << "STARTUP_FAILED_CAUSE: "
												  << QString::fromStdString(bpr::app::detail::bounded_startup_diagnostic(error.what())) << '\n';
#else
		static_cast<void>(error);
#endif
		QTextStream{ stdout } << "STARTUP_FAILED\n";
		return EXIT_FAILURE;
	} catch (...) {
		QTextStream{ stdout } << "STARTUP_FAILED\n";
		return EXIT_FAILURE;
	}
}
