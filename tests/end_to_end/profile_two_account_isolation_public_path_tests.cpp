// [[BPR-0978]]. RFC-BPR-0024 AC-02/08/09 through the shipped composition:
// two BPR profiles run at once on one registry root, each signed in to the
// same fixture server as a different account. The course discriminates the
// account each profile acts as, its vault slot, its private storage root and
// review ledger, its appearance preference, and a sign-out with a held late
// read completion in one profile that must leave the other profile untouched.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/appearance_settings_store.hpp"
#include "bpr/adapters/sqlite/profile_registry_store.hpp"
#include "bpr/adapters/sqlite/profile_storage_root.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/server_card.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "private_data_root.hpp"

#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>

#include <sqlite3.h>

#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

namespace {
	using namespace bpr;
	using app::detail::github_application_fixture_access;
	constexpr core::bpr_profile_id work_profile{ 2U };
	// The recorded github.com viewer, and the login the fixture substitutes for
	// every profile other than Default when the course asks for distinct accounts.
	constexpr auto default_login = "octo-reviewer";
	constexpr auto work_login    = "other-profile-reviewer";

	using bpr::tests::expect;
	void wait_for(const std::function<bool()>& aCondition, const char* aMessage, const std::function<void()>& aDiagnostic = { }, const int aTimeoutMs = 15000)
	{
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < aTimeoutMs) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		if (!aCondition() && aDiagnostic) {
			aDiagnostic();
		}
		expect(aCondition(), aMessage);
	}
	void run_interval(const int aMilliseconds)
	{
		auto elapsed = QElapsedTimer{ };
		elapsed.start();
		while (elapsed.elapsed() < aMilliseconds) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
	}
	void seed_appearance(const std::filesystem::path& aRoot, const core::appearance_mode aMode)
	{
		auto store = adapters::sqlite::make_appearance_settings_store({ .StorageRoot = aRoot });
		expect(store.has_value(), "seed appearance store");
		auto settings                        = application::appearance_settings{ };
		settings.Mode                        = aMode;
		settings.Typography.Ui.Font.Family   = "Sans Serif";
		settings.Typography.Code.Font.Family = "Monospace";
		expect((*store)->store(settings, { }).has_value(), "seed valid distinct appearance");
	}
	struct running_profile
	{
		std::unique_ptr<app::github_application_startup> Startup;
		std::unique_ptr<app::github_application>         Runtime;
		[[nodiscard]] auto                               window() const -> qt::workbench_window& { return Runtime->window(); }
		[[nodiscard]] auto                               context() const -> core::bpr_window_context { return Runtime->windows().front(); }
		[[nodiscard]] auto                               session() const -> core::github_status { return window().session_panel()->status(); }
		[[nodiscard]] auto                               evidence() const { return github_application_fixture_access::evidence(*Runtime); }
		[[nodiscard]] auto                               account_text() const -> std::string { return window().session_panel()->active_account_text(); }
	};
	void diagnose(const char* aName, const running_profile& aProfile)
	{
		const auto evidence = aProfile.evidence();
		std::cerr << aName << ": status=" << aProfile.window().session_panel()->status_text()
				  << " diagnostic=" << aProfile.window().session_panel()->diagnostic_text()
				  << " account=" << aProfile.account_text()
				  << " notice=" << aProfile.window().home_open_notice()->text().toStdString()
				  << " vault-stores=" << evidence.VaultStores << " credential-deleted=" << evidence.CredentialDeleted
				  << " bootstrap=" << evidence.BootstrapTransportSelected << " account-transport=" << evidence.AccountTransportSelected
				  << " reads=" << evidence.AccountReadCount << " wire-error=" << evidence.RecordingFailure << '\n';
	}
	// Only the test harness waits; both production and fixture factories return immediately.
	auto await_profile(const std::filesystem::path& aRoot, const core::bpr_profile_id aProfile) -> running_profile
	{
		auto result                     = running_profile{ };
		auto terminal                   = false;
		auto failed                     = false;
		auto options                    = app::detail::github_public_path_fixture_options{ };
		options.ApplicationDataRoot     = aRoot;
		options.Profile                 = aProfile;
		options.DistinctProfileAccounts = true;
		options.RegisteredDeviceCourse  = true;
		options.IndependentWindowReads  = true;
		options.RepeatReadSessions      = true;
		result.Startup                  = github_application_fixture_access::start(app::detail::github_public_path_fixture::profile_window_restoration_v1,
																				   options, [&](auto aResult) {
																		  terminal = true;
																		  failed   = !aResult.has_value();
																		  if (aResult) {
																			  result.Runtime = std::move(*aResult);
																		  } else {
																			  std::cerr << "startup failure: " << static_cast<int>(aResult.error()) << '\n';
																		  } });
		expect(!terminal && !result.Runtime, "startup never returns an inline runtime");
		wait_for([&] { return terminal && result.Startup->is_quiescent(); }, "profile admission terminal");
		expect(!failed && result.Runtime != nullptr, "exact profile is admitted");
		result.Runtime->show();
		wait_for([&] { return result.Runtime->windows().size() == 1U; }, "one authoritative window admitted");
		return result;
	}
	void stop(running_profile& aProfile)
	{
		aProfile.Runtime->begin_shutdown();
		wait_for([&] { return aProfile.Runtime->status().ShutdownComplete; }, "profile shutdown");
		aProfile.Runtime.reset();
		aProfile.Startup.reset();
	}
	void sign_in(const char* aName, running_profile& aProfile, const char* aLogin)
	{
		auto& window = aProfile.window();
		expect(window.home_landing()->state() == qt::landing_state::sign_in && !window.home_list()->queue_shown(), "profile lands signed out with no queue");
		window.home_landing()->primary_button()->click();
		wait_for([&] { return aProfile.session() == core::github_status::session_ready && !aProfile.Runtime->status().PublicPreviewSignedOut; },
				 "Home sign-in reaches an account session", [&] { diagnose(aName, aProfile); });
		wait_for([&] { return window.home_landing()->state() != qt::landing_state::sign_in; }, "an account leaves the sign-in landing", [&] { diagnose(aName, aProfile); });
		expect(aProfile.account_text().find(aLogin) != std::string::npos, "the strip names the exact account this profile signed in as");
	}
	void open_review(const char* aName, running_profile& aProfile)
	{
		auto& window = aProfile.window();
		if (!window.showing_home()) {
			window.home_action()->trigger();
		}
		window.home_open_bar()->setText(QStringLiteral("https://github.com/octo-org/review-repository/pull/17"));
		window.home_open_action()->trigger();
		wait_for([&] { return aProfile.session() == core::github_status::pull_request_ready; },
				 "the review opens under the profile's own account", [&] { diagnose(aName, aProfile); }, 60000);
		// [[BPR-1211]]. The review's ledger opens off the decision lane.
		wait_for([&] { return github_application_fixture_access::review_ledger_settled(*aProfile.Runtime); },
				 "the review's ledger has opened", [&] { diagnose(aName, aProfile); }, 60000);
	}
	auto signed_in_sign_out(running_profile& aProfile) -> QPushButton*
	{
		for (auto* const card : aProfile.window().server_cards()) {
			if (card->signed_in()) {
				return card->sign_out_button();
			}
		}
		return nullptr;
	}
	struct reviewed_head_rows
	{
		std::size_t Rows = 0U;
		std::string Account;
	};
	// The durable partition itself: the reviewed-head rows this root's own
	// ledger holds, with the exact account the scope was written under. A root
	// with no ledger yet holds none.
	auto reviewed_heads(const std::filesystem::path& aRoot) -> reviewed_head_rows
	{
		auto       result = reviewed_head_rows{ };
		const auto path   = aRoot / "state/v1/review-ledger.sqlite3";
		if (!std::filesystem::exists(path)) {
			return result;
		}
		sqlite3* raw = nullptr;
		expect(sqlite3_open_v2(path.string().c_str(), &raw, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK, "fixture opens the root's own ledger read-only");
		const auto    connection = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>{ raw, sqlite3_close };
		sqlite3_stmt* statement  = nullptr;
		expect(sqlite3_prepare_v2(raw, "SELECT s.account FROM last_reviewed_head h JOIN review_scope s ON s.scope_id = h.scope_id", -1, &statement, nullptr) == SQLITE_OK, "ledger schema exposes reviewed heads");
		while (sqlite3_step(statement) == SQLITE_ROW) {
			++result.Rows;
			result.Account = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
		}
		sqlite3_finalize(statement);
		return result;
	}
}

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	QTemporaryDir temporary;
	expect(temporary.isValid(), "private two-account test root");
	const auto root = e2e::private_data_root(temporary).value_or(std::filesystem::path{ });
	expect(!root.empty(), "private desktop test root under the canonical storage policy");

	auto registry = adapters::sqlite::make_profile_registry_store(root);
	auto snapshot = registry->load({ });
	expect(snapshot.has_value(), "Default registry adoption");
	const auto revision = snapshot->Revision;
	++snapshot->Revision;
	snapshot->LastProfileId = work_profile.value();
	snapshot->Profiles.push_back({ { work_profile, "Work" }, application::profile_root_mode::isolated, "profile-2" });
	expect(registry->save(*snapshot, revision, { }).has_value(), "seed durable Work profile");
	const auto authority = application::profile_storage_authority::select(*snapshot, work_profile);
	expect(authority.has_value(), "exact Work root authority");
	const auto workRoot = adapters::sqlite::resolve_profile_storage_root(root, *authority);
	expect(workRoot.has_value() && *workRoot != root, "Work resolves its own private root");
	seed_appearance(root, core::appearance_mode::dark);
	seed_appearance(*workRoot, core::appearance_mode::light);

	// Both profiles run concurrently in this process: one runtime owner each,
	// one registry root, two accounts on the same server.
	auto original = await_profile(root, core::default_bpr_profile_id);
	auto work     = await_profile(root, work_profile);
	wait_for([&] { return original.Runtime->status().ProfileMode == core::appearance_mode::dark && work.Runtime->status().ProfileMode == core::appearance_mode::light; },
			 "each profile restores its own appearance beside the other");
	expect(original.window().windowTitle() != work.window().windowTitle() && work.window().windowTitle().contains(QStringLiteral("Work")), "window chrome identifies each profile");

	sign_in("default", original, default_login);
	sign_in("work", work, work_login);
	expect(original.account_text().find(work_login) == std::string::npos && work.account_text().find(default_login) == std::string::npos,
		   "neither profile names the other profile's account");
	{
		const auto first  = original.evidence();
		const auto second = work.evidence();
		expect(first.VaultStores == 1U && second.VaultStores == 1U && first.CredentialStored && second.CredentialStored,
			   "each profile enrolled exactly one credential into its own vault");
	}

	open_review("default", original);
	open_review("work", work);
	{
		const auto first  = original.evidence();
		const auto second = work.evidence();
		expect(first.AccountTransportSelected && second.AccountTransportSelected && first.AccountReadCount > 0 && second.AccountReadCount > 0,
			   "both profiles read the same review through their own account transport");
	}
	// Private review data: the same pull request identity in both profiles,
	// a reviewed-head receipt written in Work only, under Work's own account.
	expect(reviewed_heads(root).Rows == 0U && reviewed_heads(*workRoot).Rows == 0U, "neither root holds a reviewed head before the gesture");
	expect(github_application_fixture_access::mark_head_reviewed(*work.Runtime, work.evidence().HeadOneOid), "Work records its reviewed head");
	{
		const auto workRows = reviewed_heads(*workRoot);
		expect(workRows.Rows == 1U && workRows.Account == std::to_string(1000U + work_profile.value()), "Work's reviewed head lives in Work's ledger under Work's exact account");
		expect(reviewed_heads(root).Rows == 0U, "Default's ledger holds nothing from Work's gesture on the same review");
	}
	expect(github_application_fixture_access::mark_head_reviewed(*original.Runtime, original.evidence().HeadOneOid), "Default records its own reviewed head");
	{
		const auto defaultRows = reviewed_heads(root);
		expect(defaultRows.Rows == 1U && defaultRows.Account == "1001", "Default's reviewed head lives in Default's ledger under the recorded account");
		expect(reviewed_heads(*workRoot).Rows == 1U, "Default's gesture adds nothing to Work's ledger");
	}

	// Preferences: an accepted Work appearance reaches Work's durable store and
	// not Default's window.
	const auto workWrites = work.Runtime->status().AppearanceDurableWrites;
	work.window().open_settings(qt::settings_page::appearance);
	expect(work.window().settings()->windowTitle().contains(QStringLiteral("Work")), "Settings identifies the profile it edits");
	work.window().appearance_settings()->choose_mode(core::appearance_mode::high_contrast_light);
	wait_for([&] { return work.Runtime->status().AppearanceDurableWrites > workWrites && work.Runtime->status().ProfileMode == core::appearance_mode::high_contrast_light; },
			 "Work setting persists through its owning host");
	expect(original.Runtime->status().ProfileMode == core::appearance_mode::dark, "Work edit leaves Default appearance unchanged");

	// Sign-out with a held late completion: Work's refresh is held on its read
	// lane, Work signs out, and the release must not resurrect Work's session
	// nor reach Default at all.
	const auto workContext = work.context();
	github_application_fixture_access::hold_window_read_terminal(*work.Runtime, workContext, true);
	work.window().github_refresh_action()->trigger();
	wait_for([&] { return github_application_fixture_access::held_window_read_terminals(*work.Runtime, workContext) > 0U; }, "Work refresh terminal is held");
	auto* const signOut = signed_in_sign_out(work);
	expect(signOut != nullptr, "Work's signed-in server card exposes the ordinary sign-out control");
	const auto defaultReads = original.evidence().AccountReadCount;
	signOut->click();
	wait_for([&] { return work.evidence().CredentialDeleted && work.session() != core::github_status::pull_request_ready && work.session() != core::github_status::session_ready; },
			 "Work sign-out retires its session and erases only its own credential", [&] { diagnose("work", work); });
	github_application_fixture_access::hold_window_read_terminal(*work.Runtime, workContext, false);
	run_interval(750);
	expect(work.session() != core::github_status::pull_request_ready && work.session() != core::github_status::session_ready, "the released late completion cannot revive Work's retired session");
	expect(work.window().home_landing()->state() == qt::landing_state::sign_in || !work.window().home_list()->queue_shown(), "Work returns to a signed-out landing");
	{
		const auto first = original.evidence();
		expect(!first.CredentialDeleted && first.SecretRetained && first.CredentialStored, "Default's vault slot is untouched by Work's sign-out");
		expect(first.AccountReadCount == defaultReads, "no Work completion reached Default's read lane");
		expect(original.session() == core::github_status::pull_request_ready && original.account_text().find(default_login) != std::string::npos,
			   "Default's review and account remain accepted");
	}
	original.window().github_refresh_action()->trigger();
	wait_for([&] { return original.evidence().AccountReadCount > defaultReads && original.session() == core::github_status::pull_request_ready; },
			 "Default still refreshes under its own authority after Work signed out", [&] { diagnose("default", original); }, 60000);
	expect(original.Runtime->status().ProfileMode == core::appearance_mode::dark, "Default preferences survive Work's sign-out");

	stop(work);
	expect(!original.Runtime->status().Closing && original.session() == core::github_status::pull_request_ready, "closing Work leaves Default usable");
	stop(original);
	std::cout << "profile-two-account-isolation-public-path:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
