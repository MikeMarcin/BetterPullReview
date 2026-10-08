// [[BPR-0990]]. RFC-BPR-0026 AC-04, AC-05 and AC-10 through the composed
// application: one profile, two windows, and a fixture server whose saved
// repository holds thousands of unrelated open pull requests. Discovery issues
// only the bounded personal searches and requested-reviewer reads, Home lists
// what they found under its reasons, a second window adds no discovery request,
// and a pull request opened by link is neither enrolled nor saved until the
// reviewer follows it or saves its repository. Nothing mutates GitHub.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/profile_management_window.hpp"
#include "bpr/qt/review_landing_model.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>
#include <QToolButton>

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace {
	using namespace bpr;

	using bpr::tests::expect;

	void wait_for(const std::function<bool()>& aCondition, const char* aMessage, const std::function<void()>& aDiagnostic = { }, const int aTimeoutMs = 20000)
	{
		QElapsedTimer elapsed;
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

	/// The viewer the recorded session signs in as.
	constexpr auto viewer = "octo-reviewer";

	/// The saved repository as the fixture server holds it: 2,400 open pull
	/// requests nobody asked this viewer about, two the viewer authored (one
	/// awaiting a user and a team), one that names the viewer as a reviewer
	/// and one that asks only a team.
	[[nodiscard]] auto repository_with_unrelated_pull_requests() -> app::detail::github_personal_discovery_fixture
	{
		auto fixture = app::detail::github_personal_discovery_fixture{ };
		for (std::uint32_t number = 1'000U; number < 3'400U; ++number) {
			fixture.OpenPullRequests.push_back({ .Number = number, .Author = "contributor-" + std::to_string(number % 37U), .RequestedUsers = { "someone-else" } });
		}
		fixture.OpenPullRequests.push_back({ .Number = 4'001U, .Author = viewer, .RequestedUsers = { "alice" }, .RequestedTeams = 1U });
		fixture.OpenPullRequests.push_back({ .Number = 4'002U, .Author = "Octo-Reviewer" });
		fixture.OpenPullRequests.push_back({ .Number = 4'003U, .Author = "contributor-a", .RequestedUsers = { viewer } });
		fixture.OpenPullRequests.push_back({ .Number = 4'004U, .Author = "contributor-b", .RequestedTeams = 1U });
		return fixture;
	}

	[[nodiscard]] auto queue_numbers(qt::workbench_window& aWindow) -> std::set<std::int64_t>
	{
		auto        numbers = std::set<std::int64_t>{ };
		auto* const model   = aWindow.queue_filter_model();
		for (auto row = 0; row < model->rowCount(); ++row) {
			if (const auto item = model->item_at(model->index(row, 0)); item.has_value()) {
				numbers.insert(item->Ref.Number);
			}
		}
		return numbers;
	}

	[[nodiscard]] auto queue_item(qt::workbench_window& aWindow, const std::int64_t aNumber) -> std::optional<core::queue_item>
	{
		auto* const model = aWindow.queue_filter_model();
		for (auto row = 0; row < model->rowCount(); ++row) {
			if (auto item = model->item_at(model->index(row, 0)); item.has_value() && item->Ref.Number == aNumber) {
				return item;
			}
		}
		return std::nullopt;
	}

	/// The identities a Home state panel lists, in its order.
	[[nodiscard]] auto state_rows(qt::workbench_window& aWindow, const qt::home_group aGroup) -> std::vector<QString>
	{
		auto        rows  = std::vector<QString>{ };
		auto* const model = aWindow.home_list()->state_model(aGroup);
		for (auto row = 0; model != nullptr && row < model->rowCount(); ++row) {
			rows.push_back(model->data(model->index(row, 0), qt::home_row_role::identity).toString());
		}
		return rows;
	}

	[[nodiscard]] auto contains(const std::vector<QString>& aRows, const char* aIdentity) -> bool
	{
		return std::ranges::find(aRows, QString::fromUtf8(aIdentity)) != aRows.end();
	}

	/// The account reads that arrived after `aFrom`, by operation.
	[[nodiscard]] auto reads_since(const app::detail::github_public_path_fixture_evidence& aEvidence, const std::size_t aFrom) -> std::vector<std::string>
	{
		return { aEvidence.AccountReadOperations.begin() + static_cast<std::ptrdiff_t>(aFrom), aEvidence.AccountReadOperations.end() };
	}

	[[nodiscard]] auto only_discovery(const std::vector<std::string>& aOperations) -> bool
	{
		return std::ranges::all_of(aOperations, [](const std::string& aOperation) {
			return aOperation == "SearchPersonalPullRequests" || aOperation == "ListRequestedReviewers";
		});
	}

	void submit(qt::workbench_window& aWindow, const int aNumber)
	{
		if (!aWindow.showing_home()) {
			aWindow.home_action()->trigger();
		}
		aWindow.home_open_bar()->setText(QStringLiteral("https://github.com/octo-org/review-repository/pull/%1").arg(aNumber));
		aWindow.home_open_action()->trigger();
	}
}

auto main(int aArgc, char** aArgv) -> int
try {
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	const auto    attended        = QApplication::arguments().contains(QStringLiteral("--attended"));
	const auto    renderArgument  = QApplication::arguments().indexOf(QStringLiteral("--render-directory"));
	const auto    renderDirectory = renderArgument >= 0 ? QApplication::arguments().value(renderArgument + 1) : QString{ };
	QTemporaryDir temporary;
	expect(temporary.isValid(), "private discovery test root");
	const auto root = e2e::private_data_root(temporary).value_or(std::filesystem::path{ });
	expect(!root.empty(), "private desktop test root under the canonical storage policy");
	auto options                   = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot    = root;
	options.RegisteredDeviceCourse = true;
	options.IndependentWindowReads = true;
	options.RepeatReadSessions     = true;
	// [[BPR-1042]]. The [[BPR-1017]] sibling steps enter public preview. Only
	// macOS ships it, so the course forces it against the fixture, as the
	// other composed public-preview courses do; otherwise Linux and Windows
	// refuse the activation and never reach those steps.
	options.PublicPreviewEnabledCourse = true;
	options.PersonalDiscovery          = repository_with_unrelated_pull_requests();
	options.SharedDeviceVault          = app::detail::make_shared_github_fixture_vault();
	options.Offline                    = std::make_shared<app::detail::github_offline_fixture>();
	options.RateLimit                  = std::make_shared<app::detail::github_rate_limit_fixture>();
	const auto unrelated               = options.PersonalDiscovery->OpenPullRequests.size() - 4U;
	expect(unrelated >= 2'000U, "the saved repository holds at least 2,000 unrelated open pull requests");

	app::github_application* owner    = nullptr;
	auto                     terminal = false;
	auto                     desktop  = app::detail::github_application_fixture_access::start_desktop(
		app::detail::github_public_path_fixture::profile_window_restoration_v1, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				owner = *aResult;
			}
		});
	wait_for([&] { return terminal; }, "profile owner startup completes");
	expect(owner != nullptr, "profile owner admitted");
	owner->show();
	wait_for([&] { return owner->windows().size() == 1U; }, "first window admitted");
	const auto first   = owner->windows().front();
	auto*      windowA = owner->window(first);
	expect(windowA != nullptr && windowA->isVisible(), "first window visible");
	const auto evidence  = [&] { return app::detail::github_application_fixture_access::evidence(*owner); };
	const auto discovery = [&] { return app::detail::github_application_fixture_access::personal_discovery_evidence(*owner); };
	const auto diagnose  = [&] {
		const auto served = evidence();
		const auto own    = discovery();
		std::cerr << "searches=" << served.DiscoverySearches << " details=" << served.DiscoveryDetailReads << " reads=" << served.AccountReadOperations.size()
				  << " wire-error=" << served.RecordingFailure << " state=" << static_cast<int>(own.Status.State) << " problem=" << static_cast<int>(own.Status.Problem)
				  << " submitted=" << own.Statistics.Submitted << " answered=" << own.Statistics.Answered << " failed=" << own.Statistics.Failed
				  << " status-text=" << windowA->home_discovery_status()->text().toStdString() << '\n';
	};

	// [[BPR-1017]]. Without an accepted account session a new window presents as
	// it always has, because there is no account session to give it: signed out,
	// and then in public preview. It reads nothing, and closing it leaves the
	// first window as it was.
	const auto siblingWithoutAccount = [&](const char* aVariant) {
		const auto before = evidence().AccountReadOperations.size();
		windowA->findChild<QAction*>(QStringLiteral("newBprWindowAction"))->trigger();
		wait_for([&] { return owner->windows().size() == 2U; }, "New Window admits a window without an account");
		const auto contexts = owner->windows();
		auto*      sibling  = owner->window(contexts.front() == first ? contexts.back() : contexts.front());
		QApplication::processEvents();
		const auto observed = std::tuple{ sibling->session_panel()->status(), sibling->home_landing()->state(), sibling->home_list()->queue_shown(), sibling->account_action()->text() };
		std::cerr << aVariant << " sibling: status=" << static_cast<int>(std::get<0>(observed)) << " landing=" << static_cast<int>(std::get<1>(observed))
				  << " queue shown=" << std::get<2>(observed) << " account=" << std::get<3>(observed).toStdString() << '\n';
		expect(sibling->showing_home() && evidence().AccountReadOperations.size() == before, "a window without an account reads nothing");
		sibling->close();
		wait_for([&] { return owner->windows().size() == 1U && owner->windows().front() == first; }, "the sibling closes and the first window stays");
		return observed;
	};
	expect(!owner->status().PublicPreviewSignedOut && windowA->home_landing()->state() == qt::landing_state::sign_in, "the profile opens signed out");
	expect(siblingWithoutAccount("signed-out") == std::tuple{ core::github_status::no_profile, qt::landing_state::sign_in, false, QStringLiteral("Sign in") },
		   "signed out, a new window lands on the sign-in card with no queue");
	owner->activate_public_preview();
	wait_for([&] { return owner->status().PublicPreviewSignedOut; }, "the profile enters public preview");
	expect(siblingWithoutAccount("public-preview") == std::tuple{ core::github_status::no_profile, qt::landing_state::sign_in, false, QStringLiteral("Sign in") },
		   "in public preview, a new window lands on the sign-in card with no queue");

	// Signed in from Home, with nothing saved: nothing is searched, and Home
	// says what would change that.
	windowA->home_landing()->primary_button()->click();
	wait_for([&] { return windowA->session_panel()->status() == core::github_status::session_ready && !owner->status().PublicPreviewSignedOut; }, "Home sign-in reaches an account session");
	wait_for([&] { return windowA->home_discovery_status()->isVisible() && windowA->home_discovery_status()->text() == QStringLiteral("Add a repository to find my pull requests and review requests."); },
			 "with nothing saved Home says to save a repository", diagnose);
	expect(discovery().Composed && discovery().Status.Account == viewer && evidence().DiscoverySearches == 0U && discovery().Statistics.Submitted == 0U,
		   "the account session is adopted and nothing is searched");
	expect(windowA->queue_refresh_action()->isVisible() && windowA->queue_refresh_action()->isEnabled(), "Home's refresh glyph can run for the account");

	// [[BPR-1310]]. This exact composed fixture is also the attended candidate.
	// It uses its own temporary private profile and never reaches a live provider.
	if (attended) {
		QApplication::setQuitOnLastWindowClosed(true);
		std::cout << "REPOSITORY_HOME_READY: fixture account octo-reviewer on github.com; no live GitHub reads\n"
				  << std::flush;
		QApplication::exec();
		owner->begin_shutdown();
		wait_for([&] { return owner->status().ShutdownComplete; }, "attended fixture drains");
		desktop.reset();
		expect(e2e::remove_course_root(temporary), "attended fixture removes its private root");
		return EXIT_SUCCESS;
	}
	if (!renderDirectory.isEmpty()) {
		expect(QDir{ }.mkpath(renderDirectory), "render output directory exists");
		const auto modes = std::array{ core::appearance_mode::system, core::appearance_mode::light, core::appearance_mode::dark,
									   core::appearance_mode::high_contrast_system, core::appearance_mode::high_contrast_light, core::appearance_mode::high_contrast_dark };
		for (const auto mode : modes) {
			windowA->appearance_settings()->choose_mode(mode);
			wait_for([&] { return owner->status().ProfileMode == mode; }, "rendered composed appearance is accepted");
			for (const auto width : { 1100, 520 }) {
				windowA->resize(width, 760);
				QApplication::processEvents();
				const auto prefix = renderDirectory + QStringLiteral("/%1-%2").arg(QString::fromUtf8(core::stable_name(mode))).arg(width);
				expect(windowA->grab().save(prefix + QStringLiteral("-home.png")), "real Home render saved");
				windowA->open_repository_addition();
				windowA->saved_repository_name()->setText(QStringLiteral("organization-with-a-long-name/repository-with-a-long-name"));
				windowA->repository_addition_dialog()->resize(width == 520 ? 360 : 480, 270);
				QApplication::processEvents();
				expect(windowA->repository_addition_dialog()->palette().color(QPalette::Window) == windowA->palette().color(QPalette::Window), "dialog inherits the accepted profile appearance");
				expect(windowA->repository_addition_dialog()->grab().save(prefix + QStringLiteral("-dialog.png")), "real dialog render saved");
				windowA->repository_addition_dialog()->reject();
			}
		}
		windowA->resize(1100, 760);
		owner->begin_shutdown();
		wait_for([&] { return owner->status().ShutdownComplete; }, "rendered fixture drains");
		desktop.reset();
		expect(e2e::remove_course_root(temporary), "rendered fixture removes its private root");
		std::cout << "repository-home-render:pass\n";
		return EXIT_SUCCESS;
	}
	const auto openAddition = [&] {
		windowA->findChild<QToolButton*>(QStringLiteral("homeAddRepositoryButton"))->click();
		wait_for([&] { return windowA->repository_addition_dialog() && windowA->repository_addition_dialog()->isVisible(); }, "Home opens shared addition");
	};
	const auto addDraft = [&](const QString& aText) {
		windowA->saved_repository_name()->setText(aText);
		windowA->repository_addition_dialog()->findChild<QPushButton*>(QStringLiteral("repositoryAdditionAdd"))->click();
	};
	const auto notice     = [&] { return windowA->repository_addition_dialog()->findChild<QLabel*>(QStringLiteral("repositoryAdditionStatus"))->text(); };
	const auto savedCount = [&] { return windowA->findChild<QComboBox*>(QStringLiteral("savedRepositorySelector"))->count(); };
	openAddition();
	addDraft(QStringLiteral("https://github.com/octo-org/empty-repository/pull/17"));
	wait_for([&] { return notice().contains(QStringLiteral("without a pull-request path")); }, "PR-shaped repository input is refused");
	expect(savedCount() == 0 && evidence().DiscoverySearches == 0U, "invalid input saves and searches nothing");
	addDraft(QStringLiteral("https://unknown.example/octo-org/empty-repository"));
	wait_for([&] { return windowA->repository_addition_dialog()->findChild<QPushButton*>(QStringLiteral("repositoryAdditionPrepare"))->isVisible(); }, "foreign host requires explicit preparation");
	windowA->repository_addition_dialog()->findChild<QPushButton*>(QStringLiteral("repositoryAdditionPrepare"))->click();
	wait_for([&] { return windowA->sign_in_dialog() && windowA->sign_in_dialog()->isVisible() && windowA->enterprise_server_draft()->isVisible(); }, "explicit preparation opens server setup");
	windowA->sign_in_dialog()->reject();
	wait_for([&] { return windowA->repository_addition_dialog()->isVisible(); }, "cancelled setup returns to draft");
	expect(savedCount() == 0 && windowA->saved_repository_name()->text().contains(QStringLiteral("unknown.example")) && evidence().DiscoverySearches == 0U,
		   "cancelled setup keeps draft without saving or discovering under another host");
	windowA->repository_addition_dialog()->reject();
	windowA->settings()->hide();

	// Discovery needs no pull request: a repository saved from Home before
	// anything is opened is searched at once. This one holds nothing for the
	// viewer, so it costs the two searches and no detail read.
	expect(windowA->findChild<QToolButton*>(QStringLiteral("homeAddRepositoryButton"))->isVisible(), "Home offers Add repository after sign-in with no PR");
	windowA->findChild<QToolButton*>(QStringLiteral("homeAddRepositoryButton"))->click();
	wait_for([&] { return windowA->repository_addition_dialog() && windowA->repository_addition_dialog()->isVisible(); }, "Home opens the shared addition dialog");
	windowA->saved_repository_name()->setText(QStringLiteral("octo-org/empty-repository"));
	wait_for([&] { return windowA->add_saved_repository_action()->isEnabled(); }, "an account session can save a repository before any pull request is opened");
	const auto signedIn = evidence().AccountReadOperations.size();
	windowA->repository_addition_dialog()->findChild<QPushButton*>(QStringLiteral("repositoryAdditionAdd"))->click();
	wait_for([&] { return !windowA->repository_addition_dialog()->isVisible(); }, "durable save closes the dialog");
	wait_for([&] { return discovery().Status.State == application::queue_discovery_state::current && discovery().Quiescent; }, "a saved repository is searched with no pull request opened", diagnose);
	expect(evidence().DiscoverySearches == 2U && evidence().DiscoveryDetailReads == 0U
			   && reads_since(evidence(), signedIn) == std::vector<std::string>{ "SearchPersonalPullRequests", "SearchPersonalPullRequests" },
		   "with no pull request opened, saving a repository reads exactly its two personal searches");
	expect(windowA->home_discovery_status()->text().endsWith(QStringLiteral(" in 1 saved repository.")), "Home says it searched the one saved repository");

	openAddition();
	addDraft(QStringLiteral("https://github.com/octo-org/empty-repository/"));
	wait_for([&] { return notice() == QStringLiteral("This repository is already added."); }, "canonical duplicate is shown through real store state");
	expect(savedCount() == 1 && evidence().DiscoverySearches == 2U, "duplicate saves and searches nothing again");
	windowA->repository_addition_dialog()->reject();

	// A pull request opened by link is recently viewed, and nothing else.
	submit(*windowA, 17);
	wait_for([&] { return windowA->session_panel()->status() == core::github_status::pull_request_ready; }, "the pull request opens by link", diagnose);
	wait_for([&] { return windowA->home_recent_list()->count() >= 1; }, "the opened pull request is under Recently viewed");
	auto* const follow = windowA->follow_pull_request_action();
	auto* const save   = windowA->save_pull_request_repository_action();
	expect(!queue_numbers(*windowA).contains(17) && save->isEnabled() && evidence().DiscoverySearches == 2U,
		   "opening a pull request by link enrolls nothing, saves nothing (Save Repository is still offered) and searches nothing");
	expect(follow->isEnabled() && !follow->isChecked() && windowA->pull_request_menu()->actions().contains(follow) && windowA->pull_request_menu()->actions().contains(save),
		   "the Pull Request menu offers Follow and Save Repository for it");

	// Save Repository: the repository is searched at once, with exactly the
	// personal reads, and Home lists what they found under its reasons.
	const auto before = evidence().AccountReadOperations.size();
	save->trigger();
	wait_for([&] { return discovery().Statistics.Refreshes == 2U && discovery().Status.State == application::queue_discovery_state::current && discovery().Quiescent; },
			 "saving the repository runs one discovery refresh", diagnose);
	const auto served = evidence();
	const auto reads  = reads_since(served, before);
	if (served.DiscoverySearches != 6U || reads.size() != 8U) {
		std::cerr << "searches=" << served.DiscoverySearches << " details=" << served.DiscoveryDetailReads << " reads:";
		for (const auto& read : reads) {
			std::cerr << ' ' << read;
		}
		std::cerr << '\n';
	}
	expect(served.DiscoverySearches == 6U && served.DiscoveryDetailReads == 4U && reads.size() == 8U && only_discovery(reads),
		   "one refresh is two personal searches per saved repository and one requested-reviewers read per row they returned; no snapshot, file, diff or list read");
	auto terms = served.DiscoverySearchTerms;
	std::ranges::sort(terms);
	terms.erase(std::ranges::unique(terms).begin(), terms.end());
	expect(terms == std::vector<std::string>{ "author:octo-reviewer", "review-requested:octo-reviewer" }, "the searches name the viewer and nothing broader");
	expect(discovery().Statistics.MaximumInFlight <= application::maximum_personal_discovery_in_flight && discovery().Statistics.Refreshes == 2U,
		   "never more than two reads at once, one refresh per saved-repository change");
	wait_for([&] { return contains(state_rows(*windowA, qt::home_group::needs_you), "octo-org/review-repository#4003"); }, "the pull request that asks the viewer is in Needs me");
	const auto needs   = state_rows(*windowA, qt::home_group::needs_you);
	const auto waiting = state_rows(*windowA, qt::home_group::waiting_on_others);
	expect(contains(waiting, "octo-org/review-repository#4001") && !contains(needs, "octo-org/review-repository#4001"),
		   "an authored pull request awaiting reviewers waits on others and does not need me");
	const auto requested = queue_item(*windowA, 4003);
	const auto authored  = queue_item(*windowA, 4001);
	expect(requested && requested->Membership.Requested && requested->Bucket == core::attention_bucket::initial_review_requested
			   && qt::home_reason_word(requested->Bucket) == QStringLiteral("Requested"),
		   "the requested row carries its reason");
	expect(authored && authored->Membership.Authored && authored->Bucket == core::attention_bucket::authored_awaiting_reviewer_action,
		   "the authored row carries its reason");
	const auto numbers = queue_numbers(*windowA);
	expect(!numbers.contains(4004) && !numbers.contains(1'000) && !numbers.contains(3'399) && !numbers.contains(17),
		   "a team-only request, the unrelated pull requests and the opened one are not in the queue");
	expect(windowA->home_discovery_status()->text().startsWith(QStringLiteral("Found at ")) && windowA->home_discovery_status()->text().endsWith(QStringLiteral(" in 2 saved repositories.")),
		   "Home says when the list was found and from how many repositories");
	expect(!save->isEnabled() && !follow->isChecked(), "the repository is saved; the opened pull request is still not followed");

	// Follow keeps the opened pull request, locally.
	follow->trigger();
	wait_for([&] { return queue_numbers(*windowA).contains(17) && follow->isChecked(); }, "Follow keeps the opened pull request in the queue");
	expect(queue_item(*windowA, 17)->Membership.Followed && !queue_item(*windowA, 17)->Membership.discovered(), "it is there because it is followed");

	// A second window of the profile sees the same list and asks for nothing.
	// [[BPR-1017]]. It presents the profile's session from its first frame:
	// signed-in Home with the queue, the account on the strip and the strip's
	// refresh, before it reads anything, and without an account read or a
	// vault operation to get there.
	const auto beforeWindow = evidence();
	windowA->findChild<QAction*>(QStringLiteral("newBprWindowAction"))->trigger();
	wait_for([&] { return owner->windows().size() == 2U; }, "New Window admits a second window");
	const auto contexts = owner->windows();
	const auto second   = contexts.front() == first ? contexts.back() : contexts.front();
	auto*      windowB  = owner->window(second);
	expect(windowB != nullptr && windowB->showing_home(), "the second window starts on Home");
	const auto diagnoseB = [&] {
		std::cerr << "B status=" << static_cast<int>(windowB->session_panel()->status()) << " B landing=" << static_cast<int>(windowB->home_landing()->state())
				  << " B queue shown=" << windowB->home_list()->queue_shown() << " B account=" << windowB->account_action()->text().toStdString()
				  << " A rows=" << queue_numbers(*windowA).size() << " B rows=" << queue_numbers(*windowB).size() << " A text=" << windowA->home_discovery_status()->text().toStdString()
				  << " B text=" << windowB->home_discovery_status()->text().toStdString() << '\n';
	};
	wait_for([&] { return windowB->showing_home() && queue_numbers(*windowB) == queue_numbers(*windowA) && windowB->home_discovery_status()->text() == windowA->home_discovery_status()->text()
					   && contains(state_rows(*windowB, qt::home_group::needs_you), "octo-org/review-repository#4003"); },
			 "the second window's Home shows the same queue and the same discovery sentence", diagnoseB);
	QApplication::processEvents();
	if (windowB->session_panel()->status() != core::github_status::session_ready || windowB->home_landing()->state() != qt::landing_state::choose_pull_request) {
		diagnoseB();
	}
	expect(windowB->session_panel()->status() == core::github_status::session_ready, "the second window presents the account session before it reads");
	expect(windowB->home_landing()->state() == qt::landing_state::choose_pull_request && windowB->home_list()->queue_shown(),
		   "the second window's Home is the signed-in Home with its queue, not the sign-in card");
	if (windowB->account_action()->text() != windowA->account_action()->text()) {
		std::cerr << "A account=" << windowA->account_action()->text().toStdString() << " B account=" << windowB->account_action()->text().toStdString() << '\n';
	}
	expect(windowB->account_action()->text().contains(QStringLiteral("@%1").arg(QString::fromUtf8(viewer))) && windowB->account_action()->text() == windowA->account_action()->text(),
		   "the second window's strip names the account");
	expect(windowB->queue_refresh_action()->isVisible() && windowB->queue_refresh_action()->isEnabled(), "the second window's strip can refresh the queue");
	const auto afterWindow = evidence();
	expect(afterWindow.AccountReadOperations.size() == beforeWindow.AccountReadOperations.size() && afterWindow.VaultStoreAttempts == beforeWindow.VaultStoreAttempts
			   && afterWindow.VaultRetrieveAttempts == beforeWindow.VaultRetrieveAttempts && afterWindow.VaultDeleteAttempts == beforeWindow.VaultDeleteAttempts,
		   "presenting the session in the second window reads no account and touches no vault");
	expect(evidence().DiscoverySearches == 6U && discovery().Statistics.Refreshes == 2U, "a second window adds no discovery request");

	// The explicit Refresh, from the second window: one more refresh, which
	// both windows see.
	expect(windowB->queue_refresh_action()->isVisible() && windowB->queue_refresh_action()->isEnabled(), "the second window's glyph can refresh");
	const auto beforeRefresh = evidence().AccountReadOperations.size();
	windowB->queue_refresh_action()->trigger();
	wait_for([&] { return discovery().Statistics.Refreshes == 3U && discovery().Status.State == application::queue_discovery_state::current && discovery().Quiescent; },
			 "Refresh runs one more refresh", diagnose);
	expect(evidence().DiscoverySearches == 10U && evidence().DiscoveryDetailReads == 8U && reads_since(evidence(), beforeRefresh).size() == 8U
			   && only_discovery(reads_since(evidence(), beforeRefresh)),
		   "the refresh is the same bounded personal reads, and nothing else");
	expect(queue_numbers(*windowA).contains(4003) && queue_numbers(*windowB).contains(4003) && queue_numbers(*windowB).contains(17), "both windows keep the list");

	// A second SQLite connection holds the real fixture store's write lock.
	// A failed durable add leaves the input and collection intact; it cannot say saved.
	sqlite3* raw           = nullptr;
	auto     queueDatabase = std::filesystem::path{ };
	for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
		if (entry.path().filename() == "review-queue.sqlite3") {
			queueDatabase = entry.path();
			break;
		}
	}
	expect(!queueDatabase.empty() && sqlite3_open_v2(queueDatabase.string().c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK, "fixture opens its own queue store");
	auto blocker = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>{ raw, sqlite3_close };
	expect(sqlite3_exec(raw, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK, "fixture blocks durable queue writes");
	openAddition();
	addDraft(QStringLiteral("octo-org/offline-repository"));
	wait_for([&] { return notice().contains(QStringLiteral("could not be saved locally")); }, "real storage refusal is visible", diagnose);
	expect(savedCount() == 2 && windowA->saved_repository_name()->text() == QStringLiteral("octo-org/offline-repository") && evidence().DiscoverySearches == 10U,
		   "storage refusal retains the draft, no record or discovery or review is created");
	expect(sqlite3_exec(raw, "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK, "fixture releases durable queue lock");
	blocker.reset();

	// Offline and secondary limits affect discovery, never the accepted local save.
	windowA->home_action()->trigger();
	options.Offline->Offline = true;
	addDraft(QStringLiteral("octo-org/offline-repository"));
	wait_for([&] { return !windowA->repository_addition_dialog()->isVisible() && discovery().Quiescent && discovery().Status.Problem == application::queue_discovery_problem::offline; }, "offline save remains durable with accurate discovery", diagnose);
	expect(savedCount() == 3 && windowA->home_discovery_status()->text().contains(QStringLiteral("could not be reached")), "offline discovery keeps saved records and prior rows");
	options.Offline->Offline = false;
	options.RateLimit->Mode  = 2U;
	openAddition();
	addDraft(QStringLiteral("octo-org/limited-repository"));
	wait_for([&] { return !windowA->repository_addition_dialog()->isVisible() && discovery().Quiescent && discovery().Status.Problem == application::queue_discovery_problem::secondary_rate_limited; }, "limited save remains durable with accurate discovery", diagnose);
	expect(savedCount() == 4 && options.RateLimit->Responses > 0U && windowA->home_discovery_status()->text().contains(QStringLiteral("slow down")), "secondary limit keeps four saved records and communicates delay");
	options.RateLimit->Mode = 0U;

	expect(app::detail::github_application_fixture_access::review_mutation_evidence(*owner).Submissions == 0U, "nothing here asked GitHub to change anything");

	// [[BPR-1103]]. The profile's work drains, the desktop goes, and the
	// course's root goes with it before the pass line: the row passes on that
	// line alone, so a root that stays must fail it. On Windows the stores close
	// only once the desktop's runtimes are deleted from the event loop.
	owner->begin_shutdown();
	wait_for([&] { return owner->status().ShutdownComplete; }, "the profile's work drains before teardown");
	auto retiredDesktop = QPointer<qt::profile_management_window>{ desktop->management_window() };
	expect(retiredDesktop, "desktop retirement has a lifetime witness");
	desktop.reset();
	wait_for([&] { return retiredDesktop.isNull(); }, "old desktop releases its runtime and profile lease before restart");
	owner    = nullptr;
	terminal = false;
	desktop  = app::detail::github_application_fixture_access::start_desktop(
		app::detail::github_public_path_fixture::profile_window_restoration_v1, options, [&](auto aResult) { terminal = true; if (aResult) { owner = *aResult; } else {std::cerr << "restart refusal=" << static_cast<int>(aResult.error()) << '\n';} });
	wait_for([&] { return terminal && owner != nullptr; }, "same private profile restarts");
	owner->show();
	wait_for([&] { return !owner->windows().empty(); }, "restored window is admitted");
	windowA = owner->window(owner->windows().front());
	wait_for([&] { return savedCount() == 4; }, "all successful records survive profile restart");
	owner->begin_shutdown();
	wait_for([&] { return owner->status().ShutdownComplete; }, "restarted profile drains");
	desktop.reset();
	expect(e2e::remove_course_root(temporary), "the course's temporary root is removed");
	std::cout << "personal-discovery-public-path:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
