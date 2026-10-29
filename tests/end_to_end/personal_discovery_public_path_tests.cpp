// [[BPR-0990]]. RFC-BPR-0026 AC-04, AC-05 and AC-10 through the composed
// application: one profile, two windows, and a fixture server whose saved
// repository holds thousands of unrelated open pull requests. Discovery issues
// only the bounded personal searches and requested-reviewer reads, Home lists
// what they found under its reasons, a second window adds no discovery request,
// and a pull request opened by link is neither enrolled nor saved until the
// reviewer follows it or saves its repository. Nothing mutates GitHub.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../../src/app/src/review_submission_fixture.hpp"
#include "../support/diagnostic_files.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/core/discovery_diagnosis.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/feedback_label_controls.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/home_panel.hpp"
#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/profile_management_window.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/review_landing_model.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"
#include <QAbstractButton>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QStatusBar>
#include <QStyleOptionViewItem>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QToolButton>

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <string_view>
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
	/// [[BPR-1680]]. How long the explicit Refresh's searches are held.
	constexpr std::uint32_t search_hold_ms = 400U;

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

	/// The account reads after `aFrom`; CLI lease checks can be counted separately.
	[[nodiscard]] auto reads_since(const app::detail::github_public_path_fixture_evidence& aEvidence, const std::size_t aFrom, const bool aCliLease = false) -> std::vector<std::string>
	{
		auto reads = std::vector<std::string>{ aEvidence.AccountReadOperations.begin() + static_cast<std::ptrdiff_t>(aFrom), aEvidence.AccountReadOperations.end() };
		if (aCliLease) {
			std::erase(reads, "GetViewerIdentity");
		}
		return reads;
	}

	[[nodiscard]] auto only_discovery(const std::vector<std::string>& aOperations) -> bool
	{
		return std::ranges::all_of(aOperations, [](const std::string& aOperation) {
			return aOperation == "SearchPersonalPullRequests" || aOperation == "ListRequestedReviewers" || aOperation == "GetPullRequest" || aOperation == "ListReviews"
				// [[BPR-1680]]. Home's batched read of retained pull requests.
				|| aOperation == "ListHomePullRequests" || aOperation == "ListHomeReviewMembership";
		});
	}

	/// [[BPR-1680]]. The refreshes that ended, as the profile's diagnostics
	/// file records them, each printed with its reads, requests and duration.
	[[nodiscard]] auto discovery_refreshes(const std::filesystem::path& aRoot, const std::string_view aLabel) -> std::vector<core::discovery_diagnosis>
	{
		auto           result = std::vector<core::discovery_diagnosis>{ };
		const auto     log    = bpr::tests::read_diagnostic_logs(aRoot);
		constexpr auto key    = std::string_view{ "\"detail\":\"" };
		for (auto at = log.find("\"op\":\"github.discovery\""); at != std::string::npos; at = log.find("\"op\":\"github.discovery\"", at + 1U)) {
			const auto start = log.find(key, at);
			const auto end   = start == std::string::npos ? std::string::npos : log.find('"', start + key.size());
			if (end == std::string::npos) {
				break;
			}
			const auto detail = std::string_view{ log }.substr(start + key.size(), end - start - key.size());
			const auto line   = core::parse_discovery_diagnosis(detail);
			// [[BPR-1744]]. This build writes every line with its
			// timing split and answer counts, and a read's wire time is
			// part of its run.
			expect(line.has_value() && detail.contains(" credential_ms=") && detail.contains(" backoff_s=")
					   && (line->Scope == core::discovery_diagnosis_scope::refresh || line->WireMilliseconds <= line->RunMilliseconds),
				   "the composed profile writes each discovery line in full");
			if (line && line->Scope == core::discovery_diagnosis_scope::refresh && line->State != core::discovery_diagnosis_state::none) {
				std::cerr << aLabel << " refresh=" << line->Refresh << " state=" << core::name(line->State) << " repositories=" << line->Repositories
						  << " retained=" << line->PullRequests << " reads=" << line->Reads << " failed=" << line->Failed << " requests=" << line->Requests
						  << " wait_ms=" << line->WaitMilliseconds << " run_ms=" << line->RunMilliseconds << " credential_ms=" << line->CredentialMilliseconds
						  << " wire_ms=" << line->WireMilliseconds << " decision_ms=" << line->DecisionMilliseconds << " http_403=" << line->Forbidden
						  << " http_429=" << line->Throttled << " http_5xx=" << line->ServerErrors << " timeouts=" << line->Timeouts << " backoff_s=" << line->BackoffSeconds << '\n';
				result.push_back(*line);
			}
		}
		return result;
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
	const auto    cliCourse       = QApplication::arguments().contains(QStringLiteral("--gh-cli"));
	const auto    homeWaiting     = QApplication::arguments().contains(QStringLiteral("--home-waiting"));
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
	options.PublicPreviewEnabledCourse               = true;
	options.PersonalDiscovery                        = repository_with_unrelated_pull_requests();
	options.ReviewSubmissionServer                   = std::make_shared<app::detail::review_submission_fixture_state>();
	options.ReviewSubmissionServer->DraftPullRequest = true;
	const auto approved                              = std::make_shared<std::atomic<bool>>(false);
	const auto approvalRow                           = std::ranges::find(options.PersonalDiscovery->OpenPullRequests, 4003U, &app::detail::github_personal_discovery_fixture::pull_request::Number);
	approvalRow->ReviewedByViewer                    = approved;
	const auto awaiting                              = std::make_shared<std::atomic<bool>>(true);
	const auto authoredRow                           = std::ranges::find(options.PersonalDiscovery->OpenPullRequests, 4001U, &app::detail::github_personal_discovery_fixture::pull_request::Number);
	authoredRow->ReviewerRequestsPresent             = awaiting;
	// [[BPR-1780]]. #4001 starts ready, so it waits on its reviewers; the
	// explicit Refresh below turns it into a draft, which waits on nobody.
	const auto draftStatus = std::make_shared<std::atomic<bool>>(false);
	authoredRow->Draft     = draftStatus;

	// [[BPR-1681]]. Set before the restart below.
	const auto otherRepositoriesRefused                 = std::make_shared<std::atomic<bool>>(false);
	options.PersonalDiscovery->OtherRepositoriesRefused = otherRepositoriesRefused;

	if (cliCourse) {
		options.GithubCliDiscoveryAccounts = std::vector<app::detail::github_public_path_fixture_options::github_cli_discovery_account>{ { .Hostname = "github.com", .Login = "octo-reviewer" } };
	}
	options.SharedDeviceVault = app::detail::make_shared_github_fixture_vault();
	if (homeWaiting) {
		authoredRow->WaitingDetails        = true;
		authoredRow->RequestedTeams        = 3;
		authoredRow->RequiredReviewPresent = std::make_shared<std::atomic<bool>>(true);
		authoredRow->CheckFailed           = std::make_shared<std::atomic<bool>>(true);
	}
	options.Offline = std::make_shared<app::detail::github_offline_fixture>();
	// [[BPR-1680]]. Holds the explicit Refresh's searches, to show whether
	// they overlap; nothing is held otherwise.
	options.ReadDelay    = std::make_shared<app::detail::github_read_delay_fixture>();
	options.RateLimit    = std::make_shared<app::detail::github_rate_limit_fixture>();
	const auto unrelated = options.PersonalDiscovery->OpenPullRequests.size() - 4U;
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
		std::cerr << "operations:";
		for (const auto& operation : served.AccountReadOperations) {
			std::cerr << ' ' << operation;
		}
		std::cerr << '\n';
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
	const auto connect = [&] {
		if (!cliCourse) {
			windowA->home_landing()->primary_button()->click();
			return;
		}
		QTimer consent;
		QObject::connect(&consent, &QTimer::timeout, [] {
			if (auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
				prompt->button(QMessageBox::Yes)->click();
			}
		});
		consent.start(1);
		windowA->home_landing()->secondary_button()->click();
		wait_for([&] { return windowA->session_panel()->status() == core::github_status::session_ready && !owner->status().PublicPreviewSignedOut; }, "Home accepts the selected CLI account");
	};
	connect();
	wait_for([&] { return windowA->session_panel()->status() == core::github_status::session_ready && !owner->status().PublicPreviewSignedOut; }, "Home sign-in reaches an account session");
	// RFC-BPR-0062, owner verdicts of 2026-10-07 and 2026-10-08: one button,
	// not the ordinary row, and no Needs me lane that could hold nothing.
	wait_for([&] { return windowA->home_repository_onboarding()->isVisible() && windowA->home_repository_onboarding_button()->isVisible()
					   && windowA->home_repository_onboarding_button()->text() == QStringLiteral("Add a repository") && !windowA->home_discovery_status()->isVisible()
					   && !windowA->home_list()->state_panel(qt::home_group::needs_you)->isVisible(); },
			 "with nothing saved Home shows only the first-repository button and no empty lane", diagnose);
	expect(discovery().Composed && discovery().Status.Account == viewer && evidence().DiscoverySearches == 0U && discovery().Statistics.Submitted == 0U,
		   "the account session is adopted and nothing is searched");
	expect(windowA->queue_refresh_action()->isVisible() && windowA->queue_refresh_action()->isEnabled(), "Home's refresh glyph can run for the account");

	// [[BPR-1310]]. This exact composed fixture is also the attended candidate.
	// It uses its own temporary private profile and never reaches a live provider.
	if (attended && !homeWaiting) {
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
	auto* const draft      = windowA->repository_addition_draft();
	const auto  notice     = [&] { return draft->findChild<QLabel*>(QStringLiteral("repositoryAdditionStatus"))->text(); };
	const auto  draftReady = [&] {
		return windowA->settings()->isVisible() && windowA->settings()->current_page() == qt::settings_page::repositories && draft->isVisible()
			&& windowA->saved_repository_name()->isEnabled();
	};
	const auto addDraft = [&](const QString& aText) {
		windowA->saved_repository_name()->setText(aText);
		draft->findChild<QPushButton*>(QStringLiteral("repositoryAdditionAdd"))->click();
	};
	const auto cancelDraft = [&] {
		draft->findChild<QPushButton*>(QStringLiteral("repositoryAdditionCancel"))->click();
		windowA->settings()->hide();
	};
	if (homeWaiting) {
		windowA->home_repository_onboarding_button()->click();
		wait_for(draftReady, "waiting course opens repository setup");
		addDraft(QStringLiteral("octo-org/review-repository"));
		wait_for([&] { const auto item=queue_item(*windowA,4001); return !draft->isVisible() && discovery().Quiescent && item && item->Facts.Waiting.CodeOwners.size()==3; }, "unopened discovery publishes all three named code-owner teams", diagnose);
		windowA->settings()->hide();
		windowA->home_action()->trigger();
		auto       item     = queue_item(*windowA, 4001);
		const auto identity = item->Ref;
		std::cerr << "waiting bucket=" << static_cast<int>(item->Bucket) << " requests=" << item->Facts.Waiting.Requests.value_or(999) << " failures=" << item->Facts.Waiting.FailedChecks.size() << " checks-known=" << item->Facts.Waiting.ChecksKnown << " head=" << item->Facts.Waiting.Head << "\n";
		expect(item->Bucket == core::attention_bucket::authored_awaiting_reviewer_action && item->Facts.Waiting.FailedChecks == std::vector<std::string>{ "Branch Freshness" }, "unopened mixed review/check example stays in Waiting on others");
		expect(qt::home_why_sentence(*item) == QStringLiteral("Waiting on 3 reviewers; Branch Freshness failed."), "[[BPR-1407]] native row counts its reviewers and keeps the independent failure");
		const auto renderWaiting = [&](const QString& aName) {
			if (renderDirectory.isEmpty()) {
				return;
			}
			expect(QDir{ }.mkpath(renderDirectory), "Home waiting render directory exists");
			for (const auto mode : { core::appearance_mode::light, core::appearance_mode::dark, core::appearance_mode::high_contrast_light, core::appearance_mode::high_contrast_dark }) {
				windowA->appearance_settings()->choose_mode(mode);
				wait_for([&] { return owner->status().ProfileMode == mode; }, "Home appearance accepted");
				for (const auto width : { 1100, 520 }) {
					windowA->resize(width, 760);
					QApplication::processEvents();
					expect(windowA->grab().save(renderDirectory + QStringLiteral("/%1-%2-%3.png").arg(aName, QString::fromUtf8(core::stable_name(mode))).arg(width)), "composed Home waiting render saved");
				}
			}
		};
		renderWaiting(QStringLiteral("waiting-discovered"));
		// [[BPR-1405]]. The owner's look at the waiting rows: the same
		// composed fixture, left open after discovery.
		if (attended) {
			windowA->resize(1100, 760);
			QApplication::setQuitOnLastWindowClosed(true);
			std::cout << "HOME_WAITING_ATTENDED_READY: fixture account; no live GitHub reads\n"
					  << std::flush;
			QApplication::exec();
			owner->begin_shutdown();
			wait_for([&] { return owner->status().ShutdownComplete; }, "attended waiting fixture drains");
			desktop.reset();
			expect(e2e::remove_course_root(temporary), "attended waiting fixture removes its private root");
			return EXIT_SUCCESS;
		}
		expect(windowA->home_list()->select(identity, true), "exact waiting identity selected");
		auto open = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
		QApplication::sendEvent(windowA->home_list()->current_view(), &open);
		wait_for([&] { return windowA->showing_overview() && windowA->session_panel()->status() == core::github_status::pull_request_ready; }, "Home opens the exact accepted Overview", diagnose);
		const auto rows  = windowA->overview_page()->request_rows();
		const auto teams = std::ranges::count_if(rows, [](const auto* aRow) { return aRow->request().Team && aRow->request().Name.starts_with("octo-org/team-"); });
		expect(teams == 3, "Overview preserves the same three accepted code-owner teams");
		windowA->home_action()->trigger();
		wait_for([&] {const auto row=queue_item(*windowA,4001);return windowA->showing_home() && row && row->Facts.Waiting.CodeOwners.size()==3; }, "returning Home preserves requests", diagnose);
		item = queue_item(*windowA, 4001);
		expect(core::home_section_for(item->Membership, item->Bucket) == (item->Facts.UnseenComments ? core::home_queue_section::my_pull_requests : core::home_queue_section::waiting_on_others), "accepted unread activity retains existing authored precedence");
		awaiting->store(false);
		windowA->queue_refresh_action()->trigger();
		wait_for([&] {const auto row=queue_item(*windowA,4001);return discovery().Quiescent && row && row->Facts.Waiting.Requests==0; }, "complete newer request removal updates Home", diagnose);
		expect(queue_item(*windowA, 4001)->Facts.Waiting.RequiredReview == true && queue_item(*windowA, 4001)->Facts.Waiting.FailedChecks == std::vector<std::string>{ "Branch Freshness" }, "removal preserves aggregate review requirement and check failure");
		expect(options.ReviewSubmissionServer->Submissions == 0 && options.ReviewSubmissionServer->StagedWrites == 0, "Home waiting course is read-only");
		owner->begin_shutdown();
		wait_for([&] { return owner->status().ShutdownComplete; }, "Home waiting course drains every host");
		desktop.reset();
		expect(e2e::remove_course_root(temporary), "Home waiting course removes its private root");
		std::cout << "home-waiting-public-path:pass\n";
		return EXIT_SUCCESS;
	}
	if (!renderDirectory.isEmpty()) {
		// [[BPR-1310]] v003. Actual composition, offscreen: Home first run; the
		// Repositories page empty, adding and refusing; two durable saves through
		// the page; Home and the page populated; then every repository removed
		// and Home with none again.
		expect(QDir{ }.mkpath(renderDirectory), "render output directory exists");
		const auto modes = std::array{ core::appearance_mode::light, core::appearance_mode::dark, core::appearance_mode::high_contrast_light, core::appearance_mode::high_contrast_dark };
		const auto each  = [&](const std::function<void(const QString&, int)>& aRender) {
			for (const auto mode : modes) {
				windowA->appearance_settings()->choose_mode(mode);
				wait_for([&] { return owner->status().ProfileMode == mode; }, "rendered composed appearance is accepted");
				for (const auto width : { 1100, 520 }) {
					aRender(renderDirectory + QStringLiteral("/%1-%2").arg(QString::fromUtf8(core::stable_name(mode))).arg(width), width);
				}
			}
		};
		const auto home = [&](const QString& aPath, const int aWidth) {
			windowA->settings()->hide();
			windowA->home_action()->trigger();
			windowA->resize(aWidth, 760);
			QApplication::processEvents();
			wait_for([&] {
				for (auto* const view : windowA->home_list()->findChildren<QListView*>()) {
					if (!view->isVisible()) {
						continue;
					}
					for (auto row = 0; row < view->model()->rowCount(); ++row) {
						const auto index  = view->model()->index(row, 0);
						auto       option = QStyleOptionViewItem{ };
						option.initFrom(view);
						option.widget = view;
						option.rect   = view->visualRect(index);
						if (option.rect.height() < windowA->home_list()->row_delegate()->sizeHint(option, index).height()
							|| option.rect.bottom() >= view->viewport()->height()) {
							return false;
						}
					}
				}
				return true;
			},
					 "resized Home fits every wrapped row before rendering");
			expect(windowA->grab().save(aPath), "real Home render saved");
		};
		const auto page = [&](const QString& aPath, const int aWidth) {
			windowA->settings()->present(qt::settings_page::repositories);
			windowA->settings()->resize(aWidth == 520 ? 560 : 860, 560);
			QApplication::processEvents();
			expect(windowA->settings()->grab().save(aPath), "real Repositories page render saved");
		};
		each([&](const QString& aPrefix, const int aWidth) {
			home(aPrefix + QStringLiteral("-home-first-run.png"), aWidth);
			expect(windowA->home_repository_onboarding()->isVisible(), "first run shows the button");
			page(aPrefix + QStringLiteral("-repositories-empty.png"), aWidth);
			expect(windowA->saved_repositories_empty()->isVisible(), "the page's card says it holds none");
			windowA->home_repository_onboarding_button()->click();
			wait_for(draftReady, "the button opens the page with the new-repository card");
			windowA->saved_repository_name()->setText(QStringLiteral("organization-with-a-long-name/repository-with-a-long-name"));
			page(aPrefix + QStringLiteral("-repositories-adding.png"), aWidth);
			addDraft(QStringLiteral("https://github.com/organization-with-a-long-name/repository-with-a-long-name/pull/17"));
			wait_for([&] { return notice().contains(QStringLiteral("without a pull-request path")); }, "render refusal is shown beside the field");
			page(aPrefix + QStringLiteral("-repositories-refusal.png"), aWidth);
			cancelDraft();
		});
		windowA->home_action()->trigger();
		for (const auto* const name : { "octo-org/review-repository", "organization-with-a-long-name/repository-with-a-long-name" }) {
			windowA->open_repository_addition();
			wait_for(draftReady, "render fixture opens the card");
			addDraft(QString::fromUtf8(name));
			wait_for([&] { return !draft->isVisible() && discovery().Quiescent; }, "render fixture saves through the page", diagnose);
		}
		wait_for([&] { return windowA->saved_repository_list()->count() == 2 && contains(state_rows(*windowA, qt::home_group::needs_you), "octo-org/review-repository#4003"); },
				 "render fixture shows populated Home", diagnose);
		each([&](const QString& aPrefix, const int aWidth) {
			home(aPrefix + QStringLiteral("-home-populated.png"), aWidth);
			expect(!windowA->home_repository_onboarding()->isVisible(), "populated Home shows the ordinary row");
			page(aPrefix + QStringLiteral("-repositories-populated.png"), aWidth);
		});
		windowA->settings()->present(qt::settings_page::repositories);
		while (windowA->saved_repository_list()->count() > 0) {
			const auto remaining = windowA->saved_repository_list()->count();
			windowA->saved_repository_list()->setCurrentRow(0);
			windowA->remove_saved_repository_action()->trigger();
			wait_for([&] { return windowA->saved_repository_list()->count() == remaining - 1 && discovery().Quiescent; }, "render fixture removes a repository", diagnose);
		}
		each([&](const QString& aPrefix, const int aWidth) {
			home(aPrefix + QStringLiteral("-home-no-repositories.png"), aWidth);
			expect(windowA->home_repository_onboarding()->isVisible() && !windowA->home_list()->state_panel(qt::home_group::needs_you)->isVisible(),
				   "with every repository removed Home shows the button and no empty lane");
		});
		windowA->settings()->hide();
		windowA->resize(1100, 760);
		owner->begin_shutdown();
		wait_for([&] { return owner->status().ShutdownComplete; }, "rendered fixture drains");
		desktop.reset();
		expect(e2e::remove_course_root(temporary), "rendered fixture removes its private root");
		std::cout << "repository-home-render:pass\n";
		return EXIT_SUCCESS;
	}
	const auto openAddition = [&] {
		// The first-repository call to action while nothing is saved; the row after.
		if (windowA->home_repository_onboarding()->isVisible()) {
			windowA->home_repository_onboarding_button()->click();
		} else {
			windowA->findChild<QToolButton*>(QStringLiteral("homeAddRepositoryButton"))->click();
		}
		wait_for([&] { return draftReady() && windowA->settings()->focusWidget() == windowA->saved_repository_name(); }, "Home opens Settings › Repositories with the field focused", [&] { std::cerr << "settings visible=" << windowA->settings()->isVisible() << " page=" << static_cast<int>(windowA->settings()->current_page()) << " draft=" << draft->isVisible()
																																																	  << " enabled=" << windowA->saved_repository_name()->isEnabled() << " focus=" << (windowA->settings()->focusWidget() ? windowA->settings()->focusWidget()->objectName().toStdString() : std::string{ "none" }) << '\n'; });
	};
	const auto savedCount = [&] { return windowA->saved_repository_list()->count(); };
	openAddition();
	addDraft(QStringLiteral("https://github.com/octo-org/empty-repository/pull/17"));
	wait_for([&] { return notice().contains(QStringLiteral("without a pull-request path")); }, "PR-shaped repository input is refused");
	expect(savedCount() == 0 && evidence().DiscoverySearches == 0U, "invalid input saves and searches nothing");
	addDraft(QStringLiteral("https://unknown.example/octo-org/empty-repository"));
	wait_for([&] { return draft->findChild<QPushButton*>(QStringLiteral("repositoryAdditionPrepare"))->isVisible(); }, "foreign host requires explicit preparation");
	draft->findChild<QPushButton*>(QStringLiteral("repositoryAdditionPrepare"))->click();
	wait_for([&] { return windowA->sign_in_dialog() && windowA->sign_in_dialog()->isVisible() && windowA->enterprise_server_draft()->isVisible(); }, "explicit preparation opens server setup");
	windowA->sign_in_dialog()->reject();
	wait_for([&] { return draftReady() && notice().contains(QStringLiteral("not been saved")); }, "cancelled setup returns to the draft on Repositories");
	expect(savedCount() == 0 && windowA->saved_repository_name()->text().contains(QStringLiteral("unknown.example")) && evidence().DiscoverySearches == 0U,
		   "cancelled setup keeps draft without saving or discovering under another host");
	cancelDraft();

	// Discovery needs no pull request: a repository saved from Home before
	// anything is opened is searched at once. This one holds nothing for the
	// viewer, so it costs the two searches and no detail read.
	expect(windowA->home_repository_onboarding_button()->isVisible() && windowA->home_repository_onboarding_button()->isEnabled(), "Home offers its first-repository Add after sign-in with no PR");
	windowA->home_repository_onboarding_button()->click();
	wait_for(draftReady, "Home's button opens the page's new-repository card");
	windowA->saved_repository_name()->setText(QStringLiteral("octo-org/empty-repository"));
	wait_for([&] { return windowA->add_saved_repository_action()->isEnabled(); }, "an account session can save a repository before any pull request is opened");
	const auto signedIn = evidence().AccountReadOperations.size();
	auto       enter    = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
	QApplication::sendEvent(windowA->saved_repository_name(), &enter);
	wait_for([&] { return !draft->isVisible() && windowA->settings()->findChild<QLabel*>(QStringLiteral("repositoriesConfirmation"))->text() == QStringLiteral("Added octo-org/empty-repository."); },
			 "Return saves; the durable save closes the card into the list and confirms on the page");
	windowA->settings()->hide();
	wait_for([&] { return discovery().Status.State == application::queue_discovery_state::current && discovery().Quiescent; }, "a saved repository is searched with no pull request opened", diagnose);
	if (reads_since(evidence(), signedIn, cliCourse) != std::vector<std::string>{ "SearchPersonalPullRequests", "SearchPersonalPullRequests" }) {
		diagnose();
		std::cerr << "first repository reads:";
		for (const auto& read : reads_since(evidence(), signedIn)) {
			std::cerr << ' ' << read;
		}
		std::cerr << '\n';
	}
	expect(evidence().DiscoverySearches == 2U && evidence().DiscoveryDetailReads == 0U
			   && reads_since(evidence(), signedIn, cliCourse) == std::vector<std::string>{ "SearchPersonalPullRequests", "SearchPersonalPullRequests" },
		   "with no pull request opened, saving a repository reads exactly its two personal searches");
	if (cliCourse) {
		expect(std::ranges::count(reads_since(evidence(), signedIn), "GetViewerIdentity") == 4,
			   "the CLI lease checks the accepted viewer before and after each search");
	}
	expect(windowA->home_discovery_status()->text().endsWith(QStringLiteral(" in 1 saved repository.")), "Home says it searched the one saved repository");
	expect(!windowA->home_repository_onboarding()->isVisible() && windowA->findChild<QToolButton*>(QStringLiteral("homeAddRepositoryButton"))->isVisible(),
		   "the first save retires the call to action; the ordinary Add repository row remains");

	openAddition();
	addDraft(QStringLiteral("https://github.com/octo-org/empty-repository/"));
	wait_for([&] { return notice() == QStringLiteral("This repository is already added."); }, "canonical duplicate is shown through real store state");
	expect(savedCount() == 1 && evidence().DiscoverySearches == 2U, "duplicate saves and searches nothing again");
	cancelDraft();

	// A pull request opened by link is recently viewed, and nothing else.
	submit(*windowA, 17);
	wait_for([&] { return windowA->session_panel()->status() == core::github_status::pull_request_ready; }, "the pull request opens by link", diagnose);
	wait_for([&] { return windowA->home_recent_list()->count() >= 1; }, "the opened pull request is under Recently viewed");
	expect(windowA->home_recent_list()->item(0)->data(qt::home_row_role::draft).toBool() && windowA->home_recent_list()->item(0)->data(Qt::AccessibleTextRole).toString().startsWith(QStringLiteral("Draft pull request;")), "recent-only accepted draft is annotated before it is enrolled");
	auto* const follow = windowA->follow_pull_request_action();
	auto* const save   = windowA->save_pull_request_repository_action();
	expect(!queue_numbers(*windowA).contains(17) && save->isEnabled() && evidence().DiscoverySearches == 2U,
		   "opening a pull request by link enrolls nothing, saves nothing (Save Repository is still offered) and searches nothing");
	expect(follow->isEnabled() && !follow->isChecked() && windowA->pull_request_menu()->actions().contains(follow) && windowA->pull_request_menu()->actions().contains(save),
		   "the Pull Request menu offers Follow and Save Repository for it");

	// Save Repository: the repository is searched at once, with exactly the
	// personal reads, and Home lists what they found under its reasons.
	// [[BPR-1680]]. One search per reason names both saved repositories.
	const auto before = evidence().AccountReadOperations.size();
	save->trigger();
	wait_for([&] { return discovery().Statistics.Refreshes == 2U && discovery().Status.State == application::queue_discovery_state::current && discovery().Quiescent; },
			 "saving the repository runs one discovery refresh", diagnose);
	const auto served = evidence();
	const auto reads  = reads_since(served, before, cliCourse);
	if (served.DiscoverySearches != 4U || reads.size() != 8U) {
		std::cerr << "searches=" << served.DiscoverySearches << " details=" << served.DiscoveryDetailReads << " reads:";
		for (const auto& read : reads) {
			std::cerr << ' ' << read;
		}
		std::cerr << '\n';
	}
	expect(served.DiscoverySearches == 4U && served.DiscoveryDetailReads == 4U && reads.size() == 8U && only_discovery(reads),
		   "one refresh uses personal searches and bounded reviewer/metadata reads; no files, diffs or conversations");
	auto terms = served.DiscoverySearchTerms;
	std::ranges::sort(terms);
	terms.erase(std::ranges::unique(terms).begin(), terms.end());
	expect(terms == std::vector<std::string>{ "author:octo-reviewer", "user-review-requested:@me" }, "the searches name the viewer and nothing broader");
	expect(discovery().Statistics.MaximumInFlight <= application::maximum_personal_discovery_in_flight && discovery().Statistics.Refreshes == 2U,
		   "never more than two reads at once, one refresh per saved-repository change");
	wait_for([&] { return contains(state_rows(*windowA, qt::home_group::needs_you), "octo-org/review-repository#4003"); }, "the pull request that asks the viewer is in Needs me");
	const auto needs   = state_rows(*windowA, qt::home_group::needs_you);
	const auto waiting = state_rows(*windowA, qt::home_group::waiting_on_others);
	expect(contains(waiting, "octo-org/review-repository#4001") && !contains(needs, "octo-org/review-repository#4001"),
		   "an authored pull request awaiting reviewers waits on others and does not need me");
	const auto myRows = state_rows(*windowA, qt::home_group::my_pull_requests);
	expect(contains(myRows, "octo-org/review-repository#4002") && !contains(needs, "octo-org/review-repository#4002"), "accepted authored discovery with unknown detail has its own Home section");
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

	// [[BPR-1338]]. Open on GitHub through the real composition and the
	// recording browser: the title row's button on the open pull request, a
	// Home queue row and a Recently viewed row. Each address is the accepted
	// host's canonical pull request page.
	{
		const auto pageOpens = [&] { return app::detail::github_application_fixture_access::browser_opens(*owner); };
		const auto lastPage  = [&] { return app::detail::github_application_fixture_access::browser_last_address(*owner); };
		const auto wasHome   = windowA->showing_home();
		const auto wasSize   = windowA->size();
		windowA->resize(1'300, 820);
		QApplication::processEvents();
		// The way back from Home a reviewer takes: a modality.
		windowA->choose_modality(qt::review_modality::overview);
		QApplication::processEvents();
		const auto pageBefore = pageOpens();
		wait_for([&] { return !windowA->showing_home() && windowA->open_on_github_button()->isVisible(); }, "the open pull request offers Open on GitHub beside its name",
				 [&] { std::cerr << "width=" << windowA->width() << " identity=" << windowA->identity_label()->isVisible() << " enabled=" << windowA->open_on_github_action()->isEnabled() << '\n'; });
		windowA->open_on_github_button()->click();
		expect(pageOpens() == pageBefore + 1U && lastPage() == "https://github.com/octo-org/review-repository/pull/17",
			   "the title row's button opens the open pull request's page on the accepted host");
		windowA->choose_home(true);
		QApplication::processEvents();
		auto* const needsView = windowA->home_list()->state_view(qt::home_group::needs_you);
		auto        pageRow   = QModelIndex{ };
		for (auto index = 0; needsView != nullptr && index < needsView->model()->rowCount(); ++index) {
			if (needsView->model()->index(index, 0).data(qt::home_row_role::identity).toString() == QStringLiteral("octo-org/review-repository#4003")) {
				pageRow = needsView->model()->index(index, 0);
			}
		}
		expect(windowA->showing_home() && pageRow.isValid(), "Home lists the requested row");
		// The keyboard is in a list only in the active window, as after a click.
		windowA->activateWindow();
		QApplication::processEvents();
		needsView->setFocus(Qt::OtherFocusReason);
		needsView->setCurrentIndex(pageRow);
		QApplication::processEvents();
		windowA->open_on_github_action()->trigger();
		expect(pageOpens() == pageBefore + 2U && lastPage() == "https://github.com/octo-org/review-repository/pull/4003",
			   "Open on GitHub on a Home queue row opens that row's page");
		auto* const recentRows = windowA->home_recent_list();
		expect(recentRows->count() >= 1, "the opened pull request is under Recently viewed");
		recentRows->setFocus(Qt::OtherFocusReason);
		recentRows->setCurrentRow(0);
		QApplication::processEvents();
		windowA->open_on_github_action()->trigger();
		if (lastPage() != "https://github.com/octo-org/review-repository/pull/17") {
			std::cerr << "recent opens=" << pageOpens() - pageBefore << " last=" << lastPage() << " status=" << windowA->statusBar()->currentMessage().toStdString() << " count=" << recentRows->count() << " row0=" << recentRows->item(0)->text().toStdString() << '\n';
		}
		expect(pageOpens() == pageBefore + 3U && lastPage() == "https://github.com/octo-org/review-repository/pull/17",
			   "Open on GitHub on a Recently viewed row opens that row's page");
		// [[BPR-1566]]. About Conventional Comments through the same composition
		// and browser: Settings' link, then the composer's label menu, which the
		// window binds to the same dispatch. Each opens exactly the site's root.
		auto* const about = windowA->conventional_comments_link();
		expect(about != nullptr && !about->isHidden(), "the composed window offers About Conventional Comments in Settings");
		about->click();
		expect(pageOpens() == pageBefore + 4U && lastPage() == "https://conventionalcomments.org/" && windowA->conventional_comments_status()->isHidden(),
			   "Settings' link opens https://conventionalcomments.org/ in the window's browser");
		auto* const composerLabels = windowA->review_editor()->label_controls();
		expect(composerLabels->reference_offered(), "the composed window binds the composer's label menu");
		composerLabels->open_reference(core::reference_page::conventional_comments);
		expect(pageOpens() == pageBefore + 5U && lastPage() == "https://conventionalcomments.org/",
			   "the label menu's About opens the same page through the same browser");
		windowA->choose_home(wasHome);
		windowA->resize(wasSize);
		QApplication::processEvents();
	}

	// [[BPR-1342]]. Open Review in New Window through the real profile
	// desktop: the profile's own New Window, under its one runtime owner,
	// opens Home and then the row's pull request, on Overview. The first
	// window already shows that pull request, so the new one is a second view
	// of it (RFC-BPR-0024), and the first keeps its view. Once from a queue
	// row's menu and once by ⌘Return on a Recently viewed row.
	{
		windowA->choose_home(true);
		QApplication::processEvents();
		expect(owner->windows().size() == 1U, "one window before");
		const auto shown  = windowA->identity_label()->text();
		const auto status = windowA->session_panel()->status();
		const auto opened = [&](const char* aStep) {
			wait_for([&] { return owner->windows().size() == 2U; }, aStep);
			const auto contexts = owner->windows();
			const auto other    = contexts.front() == first ? contexts.back() : contexts.front();
			auto*      sibling  = owner->window(other);
			expect(sibling != nullptr && other.Profile == first.Profile && other.Window != first.Window, "the new window is the same profile's, under the same owner");
			wait_for([&] { return !sibling->showing_home() && sibling->session_panel()->status() == core::github_status::pull_request_ready
							   && sibling->identity_label()->toolTip().startsWith(QStringLiteral("octo-org/review-repository  #17 ")); },
					 "the new window opens the row's pull request on its own",
					 [&] { std::cerr << "sibling home=" << sibling->showing_home() << " status=" << static_cast<int>(sibling->session_panel()->status())
									 << " identity=" << sibling->identity_label()->text().toStdString() << " first=" << shown.toStdString()
									 << " notice=" << sibling->home_open_notice()->text().toStdString() << '\n'; });
			expect(sibling->current_modality() == qt::review_modality::overview, "it arrives on Overview, as a row opened here does");
			expect(windowA->showing_home() && windowA->identity_label()->text() == shown && windowA->session_panel()->status() == status,
				   "the first window keeps its own view of the same pull request");
			// [[BPR-1387]]. The visible snapshot can precede its Git/epoch
			// reads. Finish those reads before closing this course's window,
			// so their CLI identity checks cannot spill into the next course's
			// assertion that opening an empty Home window reads no account.
			// A preparation can be scheduled, or resolving its inputs, while
			// the active job counters are zero. Those reads must finish too.
			wait_for([&] {
				const auto state = owner->status(other);
				const auto work  = app::detail::github_application_fixture_access::diff_work_evidence(*owner, other);
				return state && state->EpochStatus == core::epoch_status::ready
					&& work.EpochOutstandingJobs == 0U && work.WorkbenchOutstandingRequests == 0U
					&& work.OutstandingJobs == 0U && !work.Pending && !work.PreparationPending
					&& work.Preparing == 0U && work.PreparationsInFlight == 0U;
			},
					 "the opened pull request's reads settle before the Home-only window course");
			sibling->close();
			wait_for([&] { return owner->windows().size() == 1U && owner->windows().front() == first; }, "the second view closes and the first window stays");
		};
		// The label's text elides at the window's width (on Windows it drops
		// the title here); its tooltip names the whole pull request.
		expect(windowA->identity_label()->toolTip().startsWith(QStringLiteral("octo-org/review-repository  #17 ")) && status == core::github_status::pull_request_ready,
			   "the first window has the pull request open behind Home");
		auto* rows = static_cast<QListView*>(nullptr);
		auto  row  = QModelIndex{ };
		for (const auto group : { qt::home_group::needs_you, qt::home_group::my_pull_requests, qt::home_group::waiting_on_others, qt::home_group::following }) {
			auto* const view = windowA->home_list()->state_view(group);
			for (auto index = 0; view != nullptr && index < view->model()->rowCount(); ++index) {
				if (view->model()->index(index, 0).data(qt::home_row_role::identity).toString() == QStringLiteral("octo-org/review-repository#17")) {
					rows = view;
					row  = view->model()->index(index, 0);
				}
			}
		}
		expect(rows != nullptr && row.isValid(), "Home lists the followed pull request");
		windowA->activateWindow();
		rows->setFocus(Qt::OtherFocusReason);
		rows->setCurrentIndex(row);
		QApplication::processEvents();
		const auto at     = rows->visualRect(row).center();
		auto       menuAt = QContextMenuEvent{ QContextMenuEvent::Mouse, at, rows->viewport()->mapToGlobal(at) };
		QApplication::sendEvent(rows->viewport(), &menuAt);
		QApplication::processEvents();
		auto* const fresh = windowA->home_open_review_window_action();
		expect(windowA->home_row_menu()->isVisible() && fresh->isEnabled(), "the row's menu offers Open Review in New Window");
		const auto readsBefore = evidence().AccountReadOperations.size();
		fresh->trigger();
		windowA->home_row_menu()->hide();
		opened("Open Review in New Window from a queue row's menu admits one more window");
		expect(evidence().AccountReadOperations.size() > readsBefore, "the new window read the pull request for itself");
		auto* const recentRows = windowA->home_recent_list();
		expect(recentRows->count() >= 1, "the pull request is under Recently viewed");
		recentRows->setFocus(Qt::OtherFocusReason);
		recentRows->setCurrentRow(0);
		QApplication::processEvents();
		auto key = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::ControlModifier };
		QApplication::sendEvent(recentRows, &key);
		opened("⌘Return on a Recently viewed row admits one more window");
		expect(app::detail::github_application_fixture_access::review_mutation_evidence(*owner).Submissions == 0U, "opening in a new window asks GitHub to change nothing");
	}

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
	expect(!windowB->home_repository_onboarding()->isVisible() && windowB->saved_repository_list()->count() == 2, "the sibling lists the same saved records and shows no first-run call to action");
	const auto afterWindow = evidence();
	if (afterWindow.AccountReadOperations.size() != beforeWindow.AccountReadOperations.size()
		|| afterWindow.VaultStoreAttempts != beforeWindow.VaultStoreAttempts
		|| afterWindow.VaultRetrieveAttempts != beforeWindow.VaultRetrieveAttempts
		|| afterWindow.VaultDeleteAttempts != beforeWindow.VaultDeleteAttempts) {
		std::cerr << "new-window account reads=" << beforeWindow.AccountReadOperations.size() << ':' << afterWindow.AccountReadOperations.size()
				  << " vault store=" << beforeWindow.VaultStoreAttempts << ':' << afterWindow.VaultStoreAttempts
				  << " retrieve=" << beforeWindow.VaultRetrieveAttempts << ':' << afterWindow.VaultRetrieveAttempts
				  << " delete=" << beforeWindow.VaultDeleteAttempts << ':' << afterWindow.VaultDeleteAttempts << '\n';
		for (const auto& operation : reads_since(afterWindow, beforeWindow.AccountReadOperations.size())) {
			std::cerr << "new-window read=" << operation << '\n';
		}
	}
	expect(afterWindow.AccountReadOperations.size() == beforeWindow.AccountReadOperations.size() && afterWindow.VaultStoreAttempts == beforeWindow.VaultStoreAttempts
			   && afterWindow.VaultRetrieveAttempts == beforeWindow.VaultRetrieveAttempts && afterWindow.VaultDeleteAttempts == beforeWindow.VaultDeleteAttempts,
		   "presenting the session in the second window reads no account and touches no vault");
	expect(evidence().DiscoverySearches == 4U && discovery().Statistics.Refreshes == 2U, "a second window adds no discovery request");
	// [[BPR-1414]]. The real strip actions must survive the host round trip.
	{
		const auto allRows = queue_numbers(*windowA);
		const auto role    = [&](const char* aName) {
			auto* const action = windowA->findChild<QAction*>(QString::fromUtf8(aName));
			expect(action != nullptr, "the Home role action exists");
			action->trigger();
			QApplication::processEvents();
		};
		role("homeFilterRequestedAction");
		expect(queue_numbers(*windowA) == std::set<std::int64_t>{ 4003 }, "Requested includes discovered requests through the real menu");
		role("homeFilterAuthorAction");
		expect(queue_numbers(*windowA) == std::set<std::int64_t>({ 4001, 4002 }), "Author includes discovered authorship through the real menu");
		role("homeFilterAnyAction");
		expect(queue_numbers(*windowA) == allRows, "Any restores every row");
		const auto selected = queue_item(*windowA, 4003)->Ref;
		expect(windowA->home_list()->select(selected, true), "requested row selected before grouping");
		for (const auto grouping : { application::home_grouping::repository, application::home_grouping::organization, application::home_grouping::account }) {
			windowA->home_group_action(grouping)->trigger();
			QApplication::processEvents();
			expect(windowA->home_view().Grouping == grouping && windowA->home_list()->grouping() == grouping,
				   "grouping survives composed host publication");
			expect(!windowA->home_list()->group_panels().empty() && windowA->selected_queue_item() && windowA->selected_queue_item()->Ref == selected,
				   "group panels contain the selected PR");
			expect(windowB->home_view().Grouping == application::home_grouping::none, "grouping belongs to one window");
			for (const auto headers : { false, true }) {
				windowA->home_state_headers_action()->trigger();
				QApplication::processEvents();
				expect(windowA->home_list()->state_headers() == headers && queue_numbers(*windowA) == allRows,
					   "state headers do not lose grouped rows");
			}
		}
		windowA->home_group_action(application::home_grouping::none)->trigger();
		QApplication::processEvents();
	}

	// The explicit Refresh, from the second window: one more refresh, which
	// both windows see.
	expect(windowB->queue_refresh_action()->isVisible() && windowB->queue_refresh_action()->isEnabled(), "the second window's glyph can refresh");
	const auto beforeRefresh = evidence().AccountReadOperations.size();
	const auto authoredRef   = queue_item(*windowA, 4001)->Ref;
	expect(windowA->home_list()->select(authoredRef, true), "authored waiting row selected before accepted refresh");
	expect(queue_item(*windowA, 4001)->Facts.Draft == false && queue_item(*windowA, 4002)->Facts.Draft == false, "unopened discovery shows no draft chip on a ready pull request");
	approved->store(true);
	draftStatus->store(true);
	awaiting->store(false);
	const auto heldBefore                    = evidence().MaximumConcurrentAccountReads;
	options.ReadDelay->DiscoveryMilliseconds = search_hold_ms;
	auto refreshTimer                        = QElapsedTimer{ };
	refreshTimer.start();
	windowB->queue_refresh_action()->trigger();
	wait_for([&] { return discovery().Statistics.Refreshes == 3U && discovery().Status.State == application::queue_discovery_state::current && discovery().Quiescent; },
			 "Refresh runs one more refresh", diagnose);
	options.ReadDelay->DiscoveryMilliseconds = 0U;
	// [[BPR-1680]]. A CLI account's two searches run at once, on its
	// background lanes; a native account's one after the other.
	std::cerr << "explicit refresh with searches held " << search_hold_ms << " ms: " << refreshTimer.elapsed() << " ms, at most "
			  << evidence().MaximumConcurrentAccountReads << " account reads at once (before " << heldBefore << ")\n";
	if (cliCourse) {
		expect(evidence().MaximumConcurrentAccountReads >= 2U, "a CLI account's held searches overlap on its background lanes");
	}
	std::cerr << "refresh searches=" << evidence().DiscoverySearches << " details=" << evidence().DiscoveryDetailReads << " reads=" << reads_since(evidence(), beforeRefresh, cliCourse).size() << " reused=" << discovery().Statistics.Reused << "\n";
	expect(evidence().DiscoverySearches == 6U && evidence().DiscoveryDetailReads <= 12U && reads_since(evidence(), beforeRefresh, cliCourse).size() <= 32U
			   && only_discovery(reads_since(evidence(), beforeRefresh, cliCourse)),
		   "Refresh reads retained PR metadata and review summaries before the bounded personal searches");
	// [[BPR-1680]]. #4001, #4002 and #4003 were read as retained pull
	// requests, so the searches that find them read none of them again.
	expect(discovery().Statistics.Reused == 3U && evidence().DiscoveryDetailReads == 6U, "the searches reuse the refresh's three retained reads");
	// [[BPR-1680]]. The four retained pull requests of the one repository
	// are one GraphQL read; #17, the recorded review, has more reviews than a
	// page there, so it alone is read the REST way.
	{
		const auto refreshReads = reads_since(evidence(), beforeRefresh, cliCourse);
		expect(std::ranges::count(refreshReads, "ListHomePullRequests") == 1 && std::ranges::count(refreshReads, "ListReviews") == 1,
			   "the retained pull requests are one batched read, with one paged out to REST");
	}
	wait_for([&] { const auto item = queue_item(*windowA, 4001); return item && item->Bucket == core::attention_bucket::quiet
		&& contains(state_rows(*windowA, qt::home_group::my_pull_requests), "octo-org/review-repository#4001"); }, "accepted authored refresh moves waiting-only into My pull requests", diagnose);
	expect(windowA->selected_queue_item() && windowA->selected_queue_item()->Ref == authoredRef
			   && windowA->home_list()->current_view() == windowA->home_list()->state_view(qt::home_group::my_pull_requests),
		   "exact authored selection survives its section change without retargeting");
	expect(queue_item(*windowA, 4001)->Facts.Draft == true && queue_item(*windowB, 4001)->Facts.Draft == true, "accepted draft status updates both Homes with the same provider budget");
	expect(!contains(state_rows(*windowA, qt::home_group::waiting_on_others), "octo-org/review-repository#4001")
			   && !contains(state_rows(*windowA, qt::home_group::needs_you), "octo-org/review-repository#4001"),
		   "accepted refresh leaves one active authored row and no Needs me contribution");

	expect(queue_numbers(*windowA).contains(4003) && queue_numbers(*windowB).contains(4003) && queue_numbers(*windowB).contains(17), "both windows keep the list");
	expect(queue_item(*windowA, 4003)->Bucket == core::attention_bucket::quiet && queue_item(*windowB, 4003)->Bucket == core::attention_bucket::quiet
			   && contains(state_rows(*windowA, qt::home_group::following), "octo-org/review-repository#4003"),
		   "Home Refresh moves an approved unopened PR to Following in both windows");
	// [[BPR-1718]]. And says that I approved it: the chip, its tooltip and
	// the row's accessible name, from the same batched read.
	const auto approvedChip = [&](qt::workbench_window& aWindow) {
		auto* const view = aWindow.home_list()->state_view(qt::home_group::following);
		for (auto row = 0; view != nullptr && row < view->model()->rowCount(); ++row) {
			const auto index = view->model()->index(row, 0);
			if (index.data(qt::home_row_role::identity).toString() == QStringLiteral("octo-org/review-repository#4003")) {
				auto option = QStyleOptionViewItem{ };
				option.initFrom(view);
				option.rect   = view->visualRect(index);
				option.font   = view->font();
				option.widget = view;
				return std::tuple{ index.data(qt::home_row_role::verdict).toString(), index.data(qt::home_row_role::verdict_tip).toString(),
								   index.data(Qt::AccessibleTextRole).toString(), !aWindow.home_list()->row_delegate()->verdict_rect(option, index).isEmpty() };
			}
		}
		return std::tuple{ QString{ }, QString{ }, QString{ }, false };
	};
	expect(queue_item(*windowA, 4003)->Facts.MyReview == core::queue_my_review{ .Verdict = core::my_review_verdict::approved, .SubmittedAt = 1'791'994'800 }
			   && queue_item(*windowB, 4003)->Facts.MyReview == queue_item(*windowA, 4003)->Facts.MyReview,
		   "the approval and when it was submitted reach both windows' rows");
	for (auto* const window : { windowA, windowB }) {
		const auto [words, tip, name, drawn] = approvedChip(*window);
		std::cerr << "approved chip: " << words.toStdString() << " | " << tip.toStdString() << '\n';
		expect(words == QStringLiteral("Approved") && tip.startsWith(QStringLiteral("You approved this on ")) && tip.contains(QStringLiteral("2026")) && name.endsWith(tip) && drawn,
			   "the Following row carries an Approved chip whose tooltip and accessible name say when I approved it");
	}
	windowA->findChild<QAction*>(QStringLiteral("homeFilterReviewerAction"))->trigger();
	QApplication::processEvents();
	expect(queue_numbers(*windowA).contains(4003), "Reviewer includes the accepted discovered review");
	windowA->findChild<QAction*>(QStringLiteral("homeFilterAnyAction"))->trigger();
	QApplication::processEvents();
	expect(windowA->home_list()->select(authoredRef, true), "restore the authored row after checking Reviewer");

	if (!cliCourse) {
		// The normal Return gesture on that same authored identity opens it here.
		const auto siblingIdentity = windowB->identity_label()->toolTip();
		auto       openAuthored    = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
		QApplication::sendEvent(windowA->home_list()->current_view(), &openAuthored);
		wait_for([&] { return !windowA->showing_home() && windowA->session_panel()->status() == core::github_status::pull_request_ready
						   && windowA->identity_label()->toolTip().startsWith(QStringLiteral("octo-org/review-repository  #4001 ")); }, "ordinary authored row action opens the exact PR in its intended window", diagnose);
		expect(windowB->showing_home() && windowB->identity_label()->toolTip() == siblingIdentity, "opening the authored row preserves the sibling's destination");
		windowA->home_action()->trigger();
		wait_for([&] { return windowA->showing_home(); }, "return Home after the ordinary authored open");
	}

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
	expect(savedCount() == 2 && windowA->saved_repository_name()->text() == QStringLiteral("octo-org/offline-repository") && evidence().DiscoverySearches == 6U,
		   "storage refusal retains the draft, no record or discovery or review is created");
	expect(sqlite3_exec(raw, "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK, "fixture releases durable queue lock");
	blocker.reset();

	// Offline and secondary limits affect discovery, never the accepted local save.
	windowA->home_action()->trigger();
	options.Offline->Offline = true;
	addDraft(QStringLiteral("octo-org/offline-repository"));
	wait_for([&] { return !draft->isVisible() && discovery().Quiescent && discovery().Status.Problem == application::queue_discovery_problem::offline; }, "offline save remains durable with accurate discovery", diagnose);
	expect(savedCount() == 3 && windowA->home_discovery_status()->text().contains(QStringLiteral("could not be reached")), "offline discovery keeps saved records and prior rows");
	options.Offline->Offline = false;
	options.RateLimit->Mode  = 2U;
	openAddition();
	addDraft(QStringLiteral("octo-org/limited-repository"));
	wait_for([&] { return !draft->isVisible() && discovery().Quiescent && discovery().Status.Problem == application::queue_discovery_problem::secondary_rate_limited; }, "limited save remains durable with accurate discovery", diagnose);
	expect(savedCount() == 4 && options.RateLimit->Responses > 0U && windowA->home_discovery_status()->text().contains(QStringLiteral("slow down")), "secondary limit keeps four saved records and communicates delay");
	options.RateLimit->Mode = 0U;

	expect(app::detail::github_application_fixture_access::review_mutation_evidence(*owner).Submissions == 0U, "nothing here asked GitHub to change anything");

	// [[BPR-1103]]. The profile's work drains, the desktop goes, and the
	// course's root goes with it before the pass line: the row passes on that
	// line alone, so a root that stays must fail it. On Windows the stores close
	// only once the desktop's runtimes are deleted from the event loop.
	const auto followedTitle = queue_item(*windowA, 17)->Facts.Title;
	expect(!followedTitle.empty(), "followed PR has an accepted title before restarting");
	expect(queue_item(*windowA, 17)->Facts.Draft == true, "queue agrees with accepted Recent draft before restart");
	windowA->home_group_action(application::home_grouping::repository)->trigger();
	QApplication::processEvents();
	// [[BPR-1681]]. The restarted session's first refresh finds the
	// three saved repositories besides the fixture's own unreadable.
	otherRepositoriesRefused->store(true);
	// A restart observes the last arrangement only after public Quit drains
	// its write. Forced shutdown deliberately cancels unfinished stores.
	auto* const quit = windowA->findChild<QAction*>(QStringLiteral("quitProfilesAction"));
	expect(quit && quit->isEnabled(), "public Quit is available before the persistence restart");
	quit->trigger();
	wait_for([&] { return desktop->is_quiescent(); }, "public Quit drains the profile before teardown");
	auto retiredDesktop = QPointer<qt::profile_management_window>{ desktop->management_window() };
	expect(retiredDesktop, "desktop retirement has a lifetime witness");
	desktop.reset();
	wait_for([&] { return retiredDesktop.isNull(); }, "old desktop releases its runtime and profile lease before restart");
	// [[BPR-1680]]. The profile's diagnostics file holds each refresh's
	// start and end and each read's end, with counts and timings only.
	{
		const auto refreshes = discovery_refreshes(root, cliCourse ? "first-session-cli" : "first-session");
		expect(refreshes.size() >= 3U && std::ranges::all_of(refreshes, [](const core::discovery_diagnosis& aLine) { return aLine.Requests != 0U && aLine.Reads != 0U; }),
			   "each refresh that ran is in the diagnostics file with its reads and requests");
		expect(std::ranges::any_of(refreshes, [](const core::discovery_diagnosis& aLine) { return aLine.Work.DirectMatches == 1U && aLine.Work.SearchCandidates == 3U; })
				   && std::ranges::any_of(refreshes, [](const core::discovery_diagnosis& aLine) { return aLine.Work.RetainedReuse == 3U && aLine.Work.Rejected == 0U; }),
			   "actual provider-to-host-to-file diagnostics report direct search candidates and this refresh's retained reuse with no team-only search candidate");

		const auto log = bpr::tests::read_diagnostic_logs(root);
		expect(log.find("\"event\":\"github.discovery.succeeded\"") != std::string::npos && log.find("\"detail\":\"discovery=read refresh=") != std::string::npos,
			   "and each read's end, with its requests and timings");
		expect(log.find("octo-reviewer") == std::string::npos && log.find("review-repository") == std::string::npos,
			   "and no discovery line names the viewer or a repository");
	}
	owner    = nullptr;
	terminal = false;
	desktop  = app::detail::github_application_fixture_access::start_desktop(
		app::detail::github_public_path_fixture::profile_window_restoration_v1, options, [&](auto aResult) { terminal = true; if (aResult) { owner = *aResult; } else {std::cerr << "restart refusal=" << static_cast<int>(aResult.error()) << '\n';} });
	wait_for([&] { return terminal && owner != nullptr; }, "same private profile restarts");
	owner->show();
	wait_for([&] { return !owner->windows().empty(); }, "restored window is admitted");
	windowA = owner->window(owner->windows().front());
	wait_for([&] { return savedCount() == 4; }, "all successful records survive profile restart", [&] { std::cerr << "restart: status=" << static_cast<int>(windowA->session_panel()->status()) << " list=" << savedCount()
																												  << " destination=" << windowA->settings()->findChild<QLabel*>(QStringLiteral("repositoriesDestination"))->text().toStdString() << '\n'; });
	// Native fixture sign-in is explicit. CLI consent is remembered, so its
	// restart revalidates the account without another Home sign-in action.
	const auto beforeReconnect = evidence().AccountReadOperations.size();
	if (!cliCourse) {
		expect(windowA->saved_repository_list()->item(0)->text().contains(QStringLiteral("(@octo-reviewer on github.com)")) && !windowA->remove_saved_repository_action()->isEnabled(),
			   "signed out, kept records name their account and host and cannot be removed");
		connect();
	}
	wait_for([&] { return windowA->session_panel()->status() == core::github_status::session_ready && savedCount() == 4
					   && !windowA->saved_repository_list()->item(0)->text().contains(QLatin1Char('@')); },
			 "signing in again lists the account's own records", diagnose);
	if (cliCourse) {
		const auto transport = evidence();
		expect(transport.AccountTransportSelected && transport.VaultStoreAttempts == 0U && transport.VaultRetrieveAttempts == 0U,
			   "restored Home metadata uses the CLI account lease without a native credential");
	}

	wait_for([&] { const auto item = queue_item(*windowA, 4003); const auto followed = queue_item(*windowA, 17); return discovery().Quiescent && item && followed && followed->Facts.Title == followedTitle && item->Facts.Title == "Discovered pull request 4003" && item->Bucket == core::attention_bucket::quiet; },
			 "restarted Home fills retained titles and approval state before opening a review", diagnose);
	expect(windowA->home_view().Grouping == application::home_grouping::repository && windowA->home_list()->grouping() == application::home_grouping::repository
			   && windowA->home_list()->group_panel(QStringLiteral("octo-org/review-repository")) != nullptr,
		   "repository grouping survives restart and rebuilt queue rows");
	// [[BPR-1681]]. The diagnostic export's dialog takes the window's
	// activation and closing it gives it back. A refresh that has just ended,
	// though with unreadable repositories, is not run again for that.
	{
		const auto ended = [&] {
			return discovery().Quiescent && discovery().Status.State == application::queue_discovery_state::stale
				&& discovery().Status.Problem == application::queue_discovery_problem::repository_unavailable;
		};
		wait_for(ended, "the restarted refresh ends with the refused repositories unreadable", diagnose);
		windowA->choose_home(true);
		windowA->activateWindow();
		wait_for([&] { return windowA->isActiveWindow() && windowA->showing_home(); }, "the window shows Home and is active");
		wait_for(ended, "Home is settled before the export");
		const auto  refreshes    = discovery().Statistics.Refreshes;
		const auto  submitted    = discovery().Statistics.Submitted;
		auto* const exportAction = windowA->findChild<QAction*>(QStringLiteral("exportAnonymizedDiagnosticsAction"));
		expect(exportAction != nullptr, "the window offers the anonymized diagnostic export");
		exportAction->trigger();
		auto exportDialog = QPointer<QDialog>{ };
		wait_for([&] {
			for (auto* const widget : QApplication::topLevelWidgets()) {
				if (widget->objectName() == QStringLiteral("diagnosticExportDialog") && widget->isVisible()) {
					exportDialog = qobject_cast<QDialog*>(widget);
				}
			}
			return exportDialog && !windowA->isActiveWindow();
		},
				 "the export dialog opens and takes the window's activation");
		exportDialog->reject();
		windowA->activateWindow();
		wait_for([&] { return windowA->isActiveWindow(); }, "closing the export dialog gives the window its activation back");
		QApplication::processEvents();
		if (discovery().Statistics.Refreshes != refreshes) {
			std::cerr << "export refreshes=" << refreshes << ':' << discovery().Statistics.Refreshes << " submitted=" << submitted << ':' << discovery().Statistics.Submitted << '\n';
		}
		expect(discovery().Statistics.Refreshes == refreshes && discovery().Statistics.Submitted == submitted,
			   "returning from the diagnostic export runs no second refresh");
		otherRepositoriesRefused->store(false);
		wait_for([&] { return discovery().Quiescent; }, "discovery is quiet after the export", diagnose);
	}
	auto retainedRecentDraft = false;
	for (auto index = 0; index < windowA->home_recent_list()->count(); ++index) {
		const auto* recent  = windowA->home_recent_list()->item(index);
		retainedRecentDraft = retainedRecentDraft || (recent->data(qt::home_row_role::identity).toString() == QStringLiteral("octo-org/review-repository#17") && recent->data(qt::home_row_role::draft).toBool());
	}
	expect(queue_item(*windowA, 17)->Facts.Draft == true && retainedRecentDraft, "restart preserves the exact PR draft in queue and Recent independently of recency order");
	expect(contains(state_rows(*windowA, qt::home_group::following), "octo-org/review-repository#4003"), "restored approved PR is Following before it is opened");
	{
		auto* const model    = windowA->home_list()->state_model(qt::home_group::following);
		auto        restored = QString{ };
		for (auto row = 0; model != nullptr && row < model->rowCount(); ++row) {
			if (model->index(row, 0).data(qt::home_row_role::identity).toString() == QStringLiteral("octo-org/review-repository#4003")) {
				restored = model->index(row, 0).data(qt::home_row_role::verdict).toString();
			}
		}
		expect(restored == QStringLiteral("Approved"), "[[BPR-1718]] and still says I approved it");
	}
	expect(std::ranges::none_of(reads_since(evidence(), beforeReconnect), [](const std::string& aOperation) {
			   return aOperation == "GetRepository" || aOperation == "ListPullRequestFiles" || aOperation == "ListPullRequestCommits"
				   || aOperation == "ListReviewThreads" || aOperation == "ListIssueComments";
		   }),
		   "restored Home metadata never opens the snapshot, diff, history or conversation path");
	// Recently viewed: Remove takes a row off the restored list through the
	// profile's restoration host, and Delete takes the rest; GitHub is asked
	// for nothing.
	{
		auto* const recentRows         = windowA->home_recent_list();
		const auto  listed             = recentRows->count();
		const auto  readsBeforeRemoval = evidence().AccountReadOperations.size();
		expect(listed >= 1, "the restored profile lists what it viewed");
		const auto removed = recentRows->item(0)->data(qt::home_row_role::identity).toString();
		recentRows->setFocus(Qt::OtherFocusReason);
		recentRows->setCurrentRow(0);
		QApplication::processEvents();
		windowA->home_recent_remove_action()->trigger();
		wait_for([&] {
			for (auto index = 0; index < recentRows->count(); ++index) {
				if (recentRows->item(index)->data(qt::home_row_role::identity).toString() == removed) {
					return false;
				}
			}
			return recentRows->count() == listed - 1;
		},
				 "Remove from Recently Viewed takes the row off the list", diagnose);
		for (const auto context : owner->windows()) {
			expect(owner->window(context)->home_recent_list()->count() == listed - 1, "every profile window observes recent removal");
		}
		while (recentRows->count() > 0) {
			const auto remaining = recentRows->count();
			recentRows->setFocus(Qt::OtherFocusReason);
			recentRows->setCurrentRow(0);
			auto deleteKey = QKeyEvent{ QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier };
			QApplication::sendEvent(recentRows, &deleteKey);
			wait_for([&] { return recentRows->count() == remaining - 1; }, "Delete takes the current Recently viewed row off", diagnose);
		}
		expect(!windowA->home_recent_panel()->isVisible(), "an emptied Recently viewed is not shown");
		expect(evidence().AccountReadOperations.size() == readsBeforeRemoval, "recent removal makes no GitHub read");
	}
	// Settings › Repositories removes through the existing saved-repository
	// intent and store; the last removal brings back the first-run call to action.
	windowA->settings_action()->trigger();
	windowA->settings()->present(qt::settings_page::repositories);
	QApplication::processEvents();
	auto* const savedList = windowA->saved_repository_list();
	expect(windowA->settings()->page_widget(qt::settings_page::repositories)->isAncestorOf(savedList) && savedList->isVisible(), "the restored profile lists its records on Repositories");
	savedList->setCurrentRow(0);
	windowA->settings()->findChild<QPushButton*>(QStringLiteral("removeSavedRepositoryButton"))->click();
	wait_for([&] { return savedCount() == 3; }, "Remove deletes one durable record", diagnose);
	while (savedCount() > 0) {
		const auto remaining = savedCount();
		savedList->setFocus(Qt::OtherFocusReason);
		savedList->setCurrentRow(0);
		auto deleteKey = QKeyEvent{ QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier };
		QApplication::sendEvent(savedList, &deleteKey);
		wait_for([&] { return savedCount() == remaining - 1; }, "Delete removes the selected durable record", diagnose);
	}
	wait_for([&] { return windowA->saved_repositories_empty()->isVisible() && discovery().Quiescent; }, "an emptied page shows its empty state", diagnose);
	windowA->settings()->hide();
	windowA->home_action()->trigger();
	wait_for([&] { return windowA->home_repository_onboarding()->isVisible(); }, "with nothing saved again, Home shows the call to action again", diagnose);
	expect(app::detail::github_application_fixture_access::review_mutation_evidence(*owner).Submissions == 0U, "removal asks GitHub to change nothing");
	owner->begin_shutdown();
	wait_for([&] { return owner->status().ShutdownComplete; }, "restarted profile drains");
	retiredDesktop = desktop->management_window();
	desktop.reset();
	wait_for([&] { return retiredDesktop.isNull(); }, "removed recents are durable before another startup");
	owner    = nullptr;
	terminal = false;
	desktop  = app::detail::github_application_fixture_access::start_desktop(
		app::detail::github_public_path_fixture::profile_window_restoration_v1, options, [&](auto aResult) { terminal = true; if (aResult) { owner = *aResult; } });
	wait_for([&] { return terminal && owner != nullptr && !owner->windows().empty(); }, "profile restarts after recent removal");
	owner->show();
	windowA = owner->window(owner->windows().front());
	if (!cliCourse) {
		connect();
	}
	wait_for([&] { return windowA->session_panel()->status() == core::github_status::session_ready && savedCount() == 0; }, "same account returns after recent removal");
	expect(windowA->home_recent_list()->count() == 0, "SQLite restart does not resurrect removed recent entries");
	owner->begin_shutdown();
	wait_for([&] { return owner->status().ShutdownComplete; }, "final removal verification drains");
	desktop.reset();
	static_cast<void>(discovery_refreshes(root, cliCourse ? "course-cli" : "course"));
	expect(e2e::remove_course_root(temporary), "the course's temporary root is removed");
	std::cout << (cliCourse ? "personal-discovery-cli-public-path:pass\n" : "personal-discovery-public-path:pass\n");
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
