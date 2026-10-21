// [[BPR-1602]]. Settings › Storage & privacy › Troubleshooting on the real
// profile desktop: each reset closes the profile's windows, removes exactly
// its stores before any store reopens, and reopens the profile on Home with a
// notice; what it keeps is the same file; sign-in stores and logs are never
// touched; an unacknowledged destructive reset does nothing; a profile whose
// comparison history is damaged fails to start and opens again with a cache
// reset run before its stores.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../sqlite_contract/platform_test_support.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/profile_data_reset_views.hpp"
#include "bpr/qt/settings_window.hpp"
#include "private_data_root.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QElapsedTimer>
#include <QPointer>
#include <QPushButton>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QThread>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>

namespace {
	using namespace bpr;
	using reset  = core::profile_data_reset;
	namespace fs = std::filesystem;
	using bpr::tests::expect;

	void until(const std::function<bool()>& aCondition, const char* aMessage, const int aMilliseconds = 30000)
	{
		QElapsedTimer timer;
		timer.start();
		while (!aCondition() && timer.elapsed() < aMilliseconds) {
			QApplication::processEvents();
			QThread::msleep(2);
		}
		expect(aCondition(), aMessage);
	}

	/// Every diagnostic log file, by its path under the root.
	auto log_files(const fs::path& aRoot) -> std::set<std::string>
	{
		std::set<std::string> found;
		if (fs::exists(aRoot / "logs")) {
			for (const auto& item : fs::directory_iterator{ aRoot / "logs" }) {
				if (item.is_regular_file()) {
					found.insert(fs::relative(item.path(), aRoot).generic_string());
				}
			}
		}
		return found;
	}

	/// Native volume/object identity, or nothing when the file is absent.
	auto identity(const fs::path& aPath) -> std::optional<tests::sqlite_contract::file_identity>
	{
		const auto found = tests::sqlite_contract::identity_of(aPath);
		expect(found.has_value() || !fs::exists(aPath), "native identity is readable for each existing store");
		return found;
	}

	auto identities(const fs::path& aRoot, const std::set<std::string>& aStores) -> std::map<std::string, std::optional<tests::sqlite_contract::file_identity>>
	{
		std::map<std::string, std::optional<tests::sqlite_contract::file_identity>> found;
		for (const auto& store : aStores) {
			found.emplace(store, identity(aRoot / "state/v1" / (store + ".sqlite3")));
		}
		return found;
	}

	auto cache_stores() -> std::set<std::string>
	{
		return { "epoch-catalog", "markdown-images", "review-overviews" };
	}
	auto settings_stores() -> std::set<std::string>
	{
		return { "appearance-settings", "workbench-settings", "diagnostic-preferences", "window-restoration" };
	}
	auto review_stores() -> std::set<std::string>
	{
		return { "review-ledger", "review-tours", "review-cycles", "review-mutation-recovery" };
	}
	auto kept_stores() -> std::set<std::string>
	{
		return { "review-queue", "remembered-connections", "device-bindings", "profile-registry" };
	}

	struct course
	{
		std::unique_ptr<app::profile_desktop> Desktop;
		fs::path                              Root;
		core::bpr_profile_id                  Profile = core::default_bpr_profile_id;

		auto runtime() const -> app::github_application* { return Desktop->runtime(Profile); }
		auto window() const -> qt::workbench_window*
		{
			auto* const owner = runtime();
			return owner && !owner->windows().empty() ? owner->window(owner->windows().front()) : nullptr;
		}

		/// Confirms `aReset` in the real Settings and waits for the profile to
		/// reopen on a new runtime with the reset's notice.
		void reset_from_settings(const reset aReset, const QString& aNotice)
		{
			const auto  beforeAddress = reinterpret_cast<std::uintptr_t>(runtime());
			const auto  beforeContext = runtime()->windows().front();
			auto* const shown         = window();
			expect(shown != nullptr, "the profile has a window");
			const auto retiredWindow = QPointer<qt::workbench_window>{ shown };
			shown->open_settings(qt::settings_page::storage_and_privacy);
			auto* const panel = shown->profile_reset_panel_widget();
			expect(panel != nullptr && panel->isVisibleTo(shown->settings()), "Troubleshooting is on Storage & privacy in the real Settings");
			panel->button(aReset)->click();
			QApplication::processEvents();
			qt::profile_reset_confirmation* question = nullptr;
			for (auto* dialog : QApplication::topLevelWidgets()) {
				for (auto* candidate : dialog->findChildren<QDialog*>(QStringLiteral("profileResetConfirmation"))) {
					if (auto* typed = dynamic_cast<qt::profile_reset_confirmation*>(candidate); typed && typed->isVisible() && typed->reset() == aReset) {
						question = typed;
					}
				}
				if (auto* typed = dynamic_cast<qt::profile_reset_confirmation*>(dialog); typed && typed->isVisible() && typed->reset() == aReset) {
					question = typed;
				}
			}
			expect(question != nullptr, "the row asks its confirmation");
			if (question->acknowledge_box() != nullptr) {
				question->copy_box()->setChecked(false);
				question->acknowledge_box()->setChecked(true);
			}
			question->confirm_button()->click();
			until([&] {
				auto* const now      = runtime();
				auto* const reopened = window();
				// An allocator may reuse the destroyed runtime's address. Its
				// retired QObject and public window authority identify lifetimes.
				return retiredWindow.isNull() && now != nullptr && !now->windows().empty() && now->windows().front() != beforeContext
					&& reopened != nullptr && reopened->statusBar()->currentMessage().startsWith(aNotice);
			},
				  "the profile reopens on a new runtime and says what was reset", 60000);
			std::cout << "PROFILE_RESET_LIFETIME:retired-window;fresh-context;runtime-address-reused="
					  << (reinterpret_cast<std::uintptr_t>(runtime()) == beforeAddress) << '\n';
		}
	};
}

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	QTemporaryDir temporary{ qEnvironmentVariable("QUALIFICATION_TARGET_ROOT", QDir::tempPath()) + QStringLiteral("/profile-reset-XXXXXX") };
	expect(temporary.isValid(), "disposable app root exists");
	const auto root = e2e::private_data_root(temporary).value_or(fs::path{ });
	expect(!root.empty(), "private application root");

	app::detail::github_public_path_fixture_options options;
	options.ApplicationDataRoot    = root;
	options.RegisteredDeviceCourse = true;
	options.IndependentWindowReads = true;
	options.RepeatReadSessions     = true;
	course run{ .Root = root };
	run.Desktop = app::detail::github_application_fixture_access::start_desktop(app::detail::github_public_path_fixture::profile_window_restoration_v1, options, [](auto) { });
	until([&] { return run.window() != nullptr; }, "real profile desktop starts");
	run.window()->show();
	until([&] { return fs::exists(root / "state/v1/review-queue.sqlite3"); }, "the profile made its stores");
	until([&] { return fs::exists(root / "state/v1/review-cycles.sqlite3") && fs::exists(root / "state/v1/appearance-settings.sqlite3") && fs::exists(root / "state/v1/epoch-catalog.sqlite3"); },
		  "the profile made a store of each kind");

	// An unacknowledged destructive reset never reaches the desktop.
	{
		auto* const before = run.runtime();
		auto* const shown  = run.window();
		shown->open_settings(qt::settings_page::storage_and_privacy);
		auto* const question = shown->profile_reset_panel_widget()->ask(reset::everything);
		question->confirm_button()->click();
		for (auto round = 0; round < 50; ++round) {
			QApplication::processEvents();
			QThread::msleep(2);
		}
		expect(run.runtime() == before && question->isVisible(), "an unacknowledged Reset everything does nothing");
		question->reject();
	}

	const auto logs = log_files(root);
	expect(!logs.empty(), "the profile has diagnostic logs");
	const auto checkKeptLogs = [&] {
		for (const auto& path : logs) {
			expect(fs::exists(root / path), "no reset removes a diagnostic log");
		}
	};

	// Reset settings: its stores go; review data, caches, saved sources and sign-in stay the same files.
	{
		const auto settings = identities(root, settings_stores());
		const auto other    = identities(root, [] { auto all = review_stores(); all.merge(cache_stores()); all.merge(kept_stores()); return all; }());
		run.reset_from_settings(reset::settings, QStringLiteral("Settings reset"));
		for (const auto& [store, before] : settings) {
			expect(!before || identity(root / "state/v1" / (store + ".sqlite3")) != before, "Reset settings removed each settings store");
		}
		for (const auto& [store, before] : other) {
			expect(!before || identity(root / "state/v1" / (store + ".sqlite3")) == before, "Reset settings keeps every other store as the same file");
		}
		checkKeptLogs();
	}
	// Clear caches: the comparison history goes; review data and settings stay.
	{
		const auto caches = identities(root, cache_stores());
		const auto other  = identities(root, [] { auto all = review_stores(); all.merge(kept_stores()); return all; }());
		run.reset_from_settings(reset::caches, QStringLiteral("Caches cleared"));
		for (const auto& [store, before] : caches) {
			expect(!before || identity(root / "state/v1" / (store + ".sqlite3")) != before, "Clear caches removed each cache store");
		}
		for (const auto& [store, before] : other) {
			expect(!before || identity(root / "state/v1" / (store + ".sqlite3")) == before, "Clear caches keeps review data and saved sources");
		}
		checkKeptLogs();
	}
	// Reset review data: the review stores go; saved sources and sign-in stay.
	{
		const auto review = identities(root, review_stores());
		const auto kept   = identities(root, kept_stores());
		expect(review.at("review-cycles").has_value(), "a review store exists before its reset");
		run.reset_from_settings(reset::review_data, QStringLiteral("Review data reset"));
		for (const auto& [store, before] : review) {
			expect(!before || identity(root / "state/v1" / (store + ".sqlite3")) != before, "Reset review data removed each review store");
		}
		for (const auto& [store, before] : kept) {
			expect(!before || identity(root / "state/v1" / (store + ".sqlite3")) == before, "Reset review data keeps saved sources and sign-in");
		}
		checkKeptLogs();
	}

	run.Desktop->quit();
	until([&] { return run.Desktop->is_quiescent(); }, "desktop drains");
	run.Desktop.reset();

	// A damaged comparison history stops the profile at start; the same
	// profile opens once a cache reset runs before its stores.
	{
		const auto catalog = root / "state/v1/epoch-catalog.sqlite3";
		expect(fs::exists(catalog), "the catalog was made again after the cache reset");
		for (const auto* sidecar : { "-wal", "-shm" }) {
			std::error_code ignored;
			fs::remove(root / "state/v1" / (std::string{ "epoch-catalog.sqlite3" } + sidecar), ignored);
		}
		{
			std::fstream      damaged{ catalog, std::ios::in | std::ios::out | std::ios::binary };
			const std::string garbage(4096, 'Z');
			damaged.write(garbage.data(), static_cast<std::streamsize>(garbage.size()));
		}
		const auto ledger = identity(root / "state/v1/review-cycles.sqlite3");
		auto       failed = false;
		run.Desktop       = app::detail::github_application_fixture_access::start_desktop(app::detail::github_public_path_fixture::profile_window_restoration_v1, options, [&](auto aResult) { failed = !aResult; });
		until([&] { return failed && run.Desktop->is_quiescent(); }, "a damaged comparison history stops the profile at start");
		run.Desktop.reset();
		auto recovered      = options;
		recovered.DataReset = core::profile_data_reset_request{ reset::caches };
		run.Desktop         = app::detail::github_application_fixture_access::start_desktop(app::detail::github_public_path_fixture::profile_window_restoration_v1, recovered, [](auto) { });
		until([&] { return run.window() != nullptr && run.window()->statusBar()->currentMessage().startsWith(QStringLiteral("Caches cleared")); }, "the profile opens after a cache reset run before its stores", 60000);
		expect(ledger.has_value() && identity(root / "state/v1/review-cycles.sqlite3") == ledger, "recovery keeps the review data");
		run.Desktop->quit();
		until([&] { return run.Desktop->is_quiescent(); }, "desktop drains after recovery");
		run.Desktop.reset();
	}
	expect(e2e::remove_course_root(temporary), "course root removed");
	std::cout << "profile-data-reset-public-path:pass settings caches review-data unacknowledged-refused logs-kept sign-in-stores-kept startup-recovery\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
