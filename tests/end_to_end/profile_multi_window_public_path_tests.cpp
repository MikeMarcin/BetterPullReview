#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/profile_management_window.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_drawer.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QThread>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>

namespace {
	using namespace bpr;
	void expect(const bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	void wait_for(const std::function<bool()>& aCondition, const char* aMessage, const std::function<void()>& aDiagnostic = { }, const int aTimeoutMs = 15000)
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
	auto action(qt::workbench_window& aWindow, const char* aName) -> QAction*
	{
		auto* result = aWindow.findChild<QAction*>(QString::fromUtf8(aName));
		expect(result != nullptr, "public profile action exists");
		wait_for([&] { return result->isEnabled(); }, "public profile action becomes enabled");
		return result;
	}
	auto visible_settings() -> std::size_t
	{
		auto count = std::size_t{ 0U };
		for (auto* widget : QApplication::topLevelWidgets()) {
			if (dynamic_cast<qt::settings_window*>(widget) != nullptr && widget->isVisible()) {
				++count;
			}
		}
		return count;
	}
	void capture(qt::workbench_window& aWindow, const QString& aName)
	{
		const auto root = qEnvironmentVariable("BPR_MULTI_WINDOW_ARTIFACT_ROOT");
		if (!root.isEmpty()) {
			expect(QDir{ }.mkpath(root), "multiwindow artifact directory");
			expect(aWindow.grab().save(root + QLatin1Char{ '/' } + aName), "multiwindow rendered artifact");
		}
	}
	void select_file(app::github_application& aOwner, qt::workbench_window& aWindow)
	{
		wait_for([&] {
			for (const auto context : aOwner.windows()) {
				if (aOwner.window(context) == &aWindow) {
					const auto status = aOwner.status(context);
					return status && status->EpochStatus == core::epoch_status::ready;
				}
			}
			return false; },
				 "exact window comparison epoch becomes ready", [&] {
            for (const auto context : aOwner.windows()) {
                if (aOwner.window(context) != &aWindow){ continue;
}
                const auto status = aOwner.status(context);
                const auto work = app::detail::github_application_fixture_access::diff_work_evidence(aOwner, context);
                std::cerr << "epoch=" << (status ? static_cast<int>(status->EpochStatus) : -1)
                          << " outstanding=" << work.EpochOutstandingJobs
                          << " navigation=" << work.NavigationReady
                          << " workbench=" << work.WorkbenchOutstandingRequests
                          << " status=" << aWindow.status_text().toStdString() << '\n';
            } }, 60000);
		aWindow.modality()->action(qt::review_modality::read)->trigger();
		auto* files = aWindow.files_drawer()->contents_view();
		auto* model = aWindow.files_drawer()->contents();
		wait_for([&] { return model->index_for_file(core::file_identity_of("review.cpp")).isValid(); }, "accepted review lists the exact requested file");
		files->selectionModel()->clearCurrentIndex();
		files->selectionModel()->setCurrentIndex(model->index_for_file(core::file_identity_of("review.cpp")), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
		wait_for([&] {
			auto* surface = aWindow.comparison_surface();
			return surface->current_result() && surface->current_result()->Alignment.Key.File == core::file_identity_of("review.cpp") && surface->current_semantic_index() && !surface->geometry_pending() && surface->isVisible(); },
				 "selected file produces an actual comparison", [&] {
                auto* surface = aWindow.comparison_surface();
                std::cerr << "comparison status=" << aWindow.status_text().toStdString()
                          << " result=" << bool(surface->current_result())
                          << " semantic=" << bool(surface->current_semantic_index())
                          << " geometry-pending=" << surface->geometry_pending()
                          << " visible=" << surface->isVisible() << '\n';
                capture(aWindow, QStringLiteral("comparison-failure.png")); }, 60000);
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
{
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	QTemporaryDir temporary;
	expect(temporary.isValid(), "private multiwindow test root");
	const auto root                   = std::filesystem::path{ QFileInfo{ temporary.path() }.canonicalFilePath().toStdString() };
	auto       options                = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot       = root;
	options.RegisteredDeviceCourse    = true;
	options.IndependentWindowReads    = true;
	options.RepeatReadSessions        = true;
	app::github_application* owner    = nullptr;
	auto                     terminal = false;
	auto                     desktop  = app::detail::github_application_fixture_access::start_desktop(
		app::detail::github_public_path_fixture::profile_window_restoration_v1, options, [&](auto aResult) {
			terminal = true;
			if (aResult) {
				owner = *aResult;
			} else {
				std::cerr << "startup failure: " << static_cast<int>(aResult.error()) << '\n';
			}
		});
	wait_for([&] { return terminal; }, "profile owner startup completes");
	expect(owner != nullptr, "profile owner admitted");
	owner->show();
	wait_for([&] { return owner->windows().size() == 1U; }, "first authoritative window admitted");
	const auto first   = owner->windows().front();
	auto*      windowA = owner->window(first);
	expect(windowA != nullptr && windowA->isVisible(), "first public window visible");
	submit(*windowA, 17);
	auto* signIn = windowA->findChild<QPushButton*>(QStringLiteral("homeSetupContinue"));
	expect(signIn != nullptr && signIn->isVisible(), "first review offers explicit sign in");
	signIn->click();
	wait_for([&] { return windowA->session_panel()->status() == core::github_status::pull_request_ready; }, "first review opens through public setup", [&] {
		const auto evidence = app::detail::github_application_fixture_access::evidence(*owner);
		std::cerr << "status=" << windowA->session_panel()->status_text()
			<< " diagnostic=" << windowA->session_panel()->diagnostic_text()
			<< " notice=" << windowA->home_open_notice()->text().toStdString()
			<< " vault=" << evidence.VaultStores << " bootstrap=" << evidence.BootstrapTransportSelected
			<< " account=" << evidence.AccountTransportSelected << " reads=" << evidence.AccountReadCount
			<< " last-read=" << evidence.LastReadOperation << " wire-error=" << evidence.RecordingFailure << '\n'; });

	action(*windowA, "newBprWindowAction")->trigger();
	wait_for([&] { return owner->windows().size() == 2U; }, "New Window admits a second session");
	const auto contexts = owner->windows();
	const auto second   = contexts.front() == first ? contexts.back() : contexts.front();
	expect(second != first && second.Profile == first.Profile, "distinct windows share exact profile identity");
	auto* windowB = owner->window(second);
	expect(windowB != nullptr && windowB != windowA && windowB->isVisible(), "second real Qt window visible");
	expect(windowB->showing_home(), "new window starts on Home without copying sibling review");
	expect(desktop->runtime(first.Profile) == owner && desktop->window(second) == windowB, "one profile owner routes both exact contexts");
	windowA->resize(1100, 700);
	windowB->resize(1400, 900);
	QApplication::processEvents();
	const auto sizeA = windowA->size();
	const auto sizeB = windowB->size();
	expect(sizeA != sizeB, "window geometry is independent");

	windowA->open_settings(qt::settings_page::appearance);
	expect(visible_settings() == 1U, "first Settings gesture opens one shell");
	windowB->open_settings(qt::settings_page::appearance);
	expect(visible_settings() == 1U, "second Settings gesture reuses profile shell");
	auto* mode = windowB->appearance_settings()->mode_selector();
	mode->setCurrentIndex(mode->findData(static_cast<int>(core::appearance_mode::high_contrast_dark)));
	wait_for([&] {
		const auto statusA = owner->status(first);
		const auto statusB = owner->status(second);
		return statusA && statusB && statusA->ProfileMode == core::appearance_mode::high_contrast_dark && statusB->ProfileMode == statusA->ProfileMode;
	},
			 "accepted profile preference reaches both windows");
	expect(windowA->size() == sizeA && windowB->size() == sizeB, "shared preferences preserve independent geometry");
	submit(*windowB, 18);
	wait_for([&] { return windowB->session_panel()->status() == core::github_status::pull_request_ready; }, "second window opens distinct recorded PR using shared authentication");
	expect(windowA->session_panel()->status() == core::github_status::pull_request_ready, "second read preserves first review");

	expect(app::detail::github_application_fixture_access::evidence(*owner).VaultStores == 1U, "second window reuses accepted authentication without enrolling again");
	const auto snapshotA = app::detail::github_application_fixture_access::large_review_snapshot(*owner, first);
	const auto snapshotB = app::detail::github_application_fixture_access::large_review_snapshot(*owner, second);
	expect(snapshotA && snapshotB && snapshotA->PullRequest.Number.value() == 17U && snapshotB->PullRequest.Number.value() == 18U,
		   "accepted read snapshots retain distinct PR identities");
	expect(snapshotA->Account == snapshotB->Account && snapshotA->Profile == snapshotB->Profile, "both reviews use shared account authority");
	select_file(*owner, *windowA);
	select_file(*owner, *windowB);
	const auto comparisonA = windowA->comparison_surface()->current_result();
	const auto comparisonB = windowB->comparison_surface()->current_result();
	expect(comparisonA->Alignment.Key.PullRequest == snapshotA->PullRequest.PullRequest && comparisonB->Alignment.Key.PullRequest == snapshotB->PullRequest.PullRequest, "each displayed comparison names its accepted PR");
	expect(comparisonA->Alignment.Key != comparisonB->Alignment.Key, "different PR windows retain distinct exact comparison keys");
	const auto selectionB = windowB->comparison_surface()->selection();
	expect(windowA->comparison_surface()->select_conversation_line(core::diff_side::right, 2U), "first window supports exact semantic line selection");
	expect(windowA->comparison_surface()->selection().has_value(), "first window receives semantic selection");
	expect(windowB->comparison_surface()->selection() == selectionB && windowB->comparison_surface()->current_result() == comparisonB,
		   "navigation in first window preserves second comparison and selection");
	expect(windowA->comparison_surface()->current_result() == comparisonA, "sibling file selection preserved first comparison");
	capture(*windowA, QStringLiteral("window-a.png"));
	capture(*windowB, QStringLiteral("window-b.png"));
	const auto unrelatedReceiptB = windowB->review_editor()->shown_receipt();
	action(*windowA, "toggleSelectedFileReadAction")->trigger();
	wait_for([&] { return windowA->review_editor()->shown_receipt() && windowA->review_editor()->shown_receipt()->Read == core::file_read_state::read; }, "first review accepts the public receipt action");
	QApplication::processEvents();
	expect(windowB->review_editor()->shown_receipt() == unrelatedReceiptB
			   && windowB->comparison_surface()->current_result() == comparisonB
			   && windowB->comparison_surface()->selection() == selectionB,
		   "receipt writes in PR17 do not change PR18's receipt or comparison");
	// The existing private-note seed writes through A's composed ledger host.
	// Observe the resulting public overview in another session of the same PR.
	submit(*windowB, 17);
	wait_for([&] { return windowB->session_panel()->status() == core::github_status::pull_request_ready; }, "second window temporarily opens first review for note fanout");
	select_file(*owner, *windowB);
	const auto noteComparisonB = windowB->comparison_surface()->current_result();
	const auto noteSelectionB  = windowB->comparison_surface()->selection();
	const auto noteSelectionA  = windowA->comparison_surface()->selection();
	const auto noteScrollB     = windowB->comparison_surface()->shared_scrollbar()->value();
	expect(app::detail::github_application_fixture_access::seed_agent_private_context_canary(*owner, "First shared private fixture note", "Second shared private fixture note"), "private notes persist through the composed ledger");
	wait_for([&] {
		return windowA->overview_page()->projection() && windowB->overview_page()->projection()
			&& windowA->overview_page()->projection()->Progress.PrivateNotes == 2U
			&& windowB->overview_page()->projection()->Progress.PrivateNotes == 2U;
	},
			 "accepted private notes refresh both exact-review overviews");
	expect(windowA->overview_page()->value_label(qt::overview_value::private_notes)->text() == QStringLiteral("2")
			   && windowB->overview_page()->value_label(qt::overview_value::private_notes)->text() == QStringLiteral("2"),
		   "both overview labels render the accepted private-note count");
	expect(windowA->comparison_surface()->current_result() == comparisonA
			   && windowA->comparison_surface()->selection() == noteSelectionA
			   && windowB->comparison_surface()->current_result() == noteComparisonB
			   && windowB->comparison_surface()->selection() == noteSelectionB
			   && windowB->comparison_surface()->shared_scrollbar()->value() == noteScrollB,
		   "private-note fanout preserves independent comparisons and reading position");
	submit(*windowB, 18);
	wait_for([&] { return windowB->session_panel()->status() == core::github_status::pull_request_ready; }, "second window returns to its independent review");
	select_file(*owner, *windowB);
	expect(windowB->overview_page()->projection() && windowB->overview_page()->projection()->Progress.PrivateNotes == 0U, "replacement review does not inherit another PR's private notes");
	app::detail::github_application_fixture_access::hold_window_read_terminal(*owner, second, true);
	windowB->github_refresh_action()->trigger();
	wait_for([&] { return app::detail::github_application_fixture_access::held_window_read_terminals(*owner, second) > 0U; }, "surviving window has a real read completion pending");
	windowA->close();
	wait_for([&] { return owner->window(first) == nullptr && owner->windows().size() == 1U; }, "closing first window retires only its session");
	expect(desktop->runtime(first.Profile) == owner && owner->window(second) == windowB, "profile services survive first window close");
	expect(!windowA->isVisible(), "accepted close hides initial retained shell while sibling survives");
	app::detail::github_application_fixture_access::hold_window_read_terminal(*owner, second, false);
	wait_for([&] { return windowB->session_panel()->status() == core::github_status::pull_request_ready; }, "surviving read completes after first window closes");
	const auto resumed = app::detail::github_application_fixture_access::large_review_snapshot(*owner, second);
	expect(resumed && resumed->PullRequest.Number.value() == 18U, "surviving completion retains exact PR identity");
	select_file(*owner, *windowB);
	windowB->open_settings(qt::settings_page::appearance);
	expect(visible_settings() == 1U, "profile Settings remains usable after first window retirement");
	// Closing hides the shell before durable retirement and its queued
	// session removal complete. Observe that public completion boundary.
	wait_for([&] { return !owner->status(first); }, "retired exact context completes durable shutdown");
	expect(!owner->status(first).has_value(), "retired exact context has no status");
	auto stale       = second;
	stale.Generation = core::bpr_window_generation{ second.Generation.value() + 1U };
	expect(owner->window(stale) == nullptr && !owner->status(stale), "foreign generation cannot address surviving window");
	action(*windowB, "manageProfilesAction")->trigger();
	wait_for([&] { return desktop->management_window() != nullptr; }, "management remains reachable from survivor");
	auto& management = *desktop->management_window();
	wait_for([&] { return management.open_button()->isEnabled(); }, "Open Profile available for current profile");
	management.open_button()->click();
	QApplication::processEvents();
	expect(owner->windows().size() == 1U && owner->window(second) == windowB, "Open Profile activates survivor without duplicate owner or session");
	action(*windowB, "newBprWindowAction")->trigger();
	wait_for([&] { return owner->windows().size() == 2U; }, "New Window remains usable after initial retirement");
	qt::workbench_window* windowC = nullptr;
	for (const auto context : owner->windows()) {
		expect(context != first, "retired context is never reused");
		if (context != second) {
			expect(owner->window(context) != nullptr && owner->window(context)->showing_home(), "replacement is a fresh Home session");
			windowC = owner->window(context);
		}
	}
	expect(windowC != nullptr, "replacement window is reachable by its context");
	submit(*windowC, 18);
	wait_for([&] { return windowC->session_panel()->status() == core::github_status::pull_request_ready; }, "replacement opens sibling PR without another sign-in");
	select_file(*owner, *windowC);
	wait_for([&] { return windowB->review_editor()->shown_receipt() && windowC->review_editor()->shown_receipt(); }, "same-review windows receive selected-file receipts");
	expect(windowB->comparison_surface()->select_conversation_line(core::diff_side::right, 1U)
			   && windowC->comparison_surface()->select_conversation_line(core::diff_side::right, 3U),
		   "same-review windows choose distinct recorded source lines before receipt fanout");
	expect(windowC->comparison_surface()->selection().has_value()
			   && windowC->comparison_surface()->selection() != windowB->comparison_surface()->selection(),
		   "same-review receipt course starts with distinct semantic reading positions");
	const auto receiptComparisonB = windowB->comparison_surface()->current_result();
	const auto receiptComparisonC = windowC->comparison_surface()->current_result();
	const auto receiptSelectionB  = windowB->comparison_surface()->selection();
	const auto receiptSelectionC  = windowC->comparison_surface()->selection();
	const auto receiptScrollB     = windowB->comparison_surface()->shared_scrollbar()->value();
	const auto receiptScrollC     = windowC->comparison_surface()->shared_scrollbar()->value();
	expect(windowB->review_editor()->shown_receipt()->Read != core::file_read_state::read
			   && windowB->review_editor()->shown_receipt()->File == windowC->review_editor()->shown_receipt()->File
			   && windowB->review_editor()->shown_receipt()->Material == windowC->review_editor()->shown_receipt()->Material,
		   "receipt course starts with the same exact unread file material");
	action(*windowB, "toggleSelectedFileReadAction")->trigger();
	wait_for([&] {
		return windowB->review_editor()->shown_receipt() && windowC->review_editor()->shown_receipt()
			&& windowB->review_editor()->shown_receipt()->Read == core::file_read_state::read
			&& windowC->review_editor()->shown_receipt() == windowB->review_editor()->shown_receipt();
	},
			 "public read action refreshes the sibling's accepted receipt");
	expect(windowB->comparison_surface()->current_result() == receiptComparisonB
			   && windowC->comparison_surface()->current_result() == receiptComparisonC
			   && windowB->comparison_surface()->selection() == receiptSelectionB
			   && windowC->comparison_surface()->selection() == receiptSelectionC
			   && windowB->comparison_surface()->shared_scrollbar()->value() == receiptScrollB
			   && windowC->comparison_surface()->shared_scrollbar()->value() == receiptScrollC,
		   "receipt fanout preserves each exact comparison, semantic selection, and scroll");
	expect(receiptComparisonB->Alignment.Key.PullRequest == snapshotB->PullRequest.PullRequest
			   && receiptComparisonC->Alignment.Key.PullRequest == snapshotB->PullRequest.PullRequest,
		   "receipt delivery retains both windows' selected PR identity");
	expect(windowB->comparison_surface()->select_conversation_line(core::diff_side::right, 2U)
			   && windowC->comparison_surface()->select_conversation_line(core::diff_side::right, 2U),
		   "both windows select the same review anchor");
	wait_for([&] { return windowB->review_editor()->shown_draft() && windowC->review_editor()->shown_draft(); }, "both actual editors receive a local draft");
	expect(windowB->review_editor()->shown_draft()->Anchor == windowC->review_editor()->shown_draft()->Anchor, "shared draft course uses identical comparison and line");
	windowB->review_editor()->body_editor()->setPlainText(QStringLiteral("Shared profile draft from surviving window"));
	wait_for([&] { return windowC->review_editor()->body_editor()->toPlainText() == QStringLiteral("Shared profile draft from surviving window"); }, "accepted draft edit reaches sibling editor");
	windowC->review_editor()->body_editor()->setPlainText(QStringLiteral("Shared profile draft edited in replacement"));
	wait_for([&] { return windowB->review_editor()->body_editor()->toPlainText() == QStringLiteral("Shared profile draft edited in replacement"); }, "replacement edit returns through shared owner");
	// Queue the sibling write before starting a different-file request. Observe
	// action transitions throughout delivery, including before Git completes.
	auto*      pendingReceiptAction            = action(*windowC, "toggleSelectedFileReadAction");
	auto       receiptEnabledWithoutComparison = false;
	auto       observingPendingReceipt         = false;
	const auto pendingReceiptConnection        = QObject::connect(pendingReceiptAction, &QAction::changed, windowC, [&] {
		if (observingPendingReceipt && pendingReceiptAction->isEnabled() && !windowC->comparison_surface()->current_result()) {
			receiptEnabledWithoutComparison = true;
		}
	});
	const auto pendingComparisonB              = windowB->comparison_surface()->current_result();
	const auto pendingSelectionB               = windowB->comparison_surface()->selection();
	auto*      pendingFiles                    = windowC->files_drawer()->contents_view();
	expect(pendingFiles->model()->rowCount() == 2, "pending selection course has two recorded files");
	action(*windowB, "toggleSelectedFileReadAction")->trigger();
	pendingFiles->selectionModel()->setCurrentIndex(windowC->files_drawer()->contents()->index_for_file(core::file_identity_of("secondary.cpp")), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
	expect(!windowC->comparison_surface()->current_result() && !windowC->review_editor()->shown_receipt()
			   && !pendingReceiptAction->isEnabled() && !windowC->review_editor()->mark_read_action()->isEnabled()
			   && !windowC->review_editor()->accept_action()->isEnabled() && !windowC->review_editor()->exclude_action()->isEnabled(),
		   "beginning the second-file comparison immediately clears the old receipt and actions");
	// Arm after synchronous presentation has finished changing labels and
	// disabling the actions; no queued ledger or Git terminal has run yet.
	observingPendingReceipt = true;
	wait_for([&] {
		const auto result = windowC->comparison_surface()->current_result();
		return result && result->Alignment.Key.File == core::file_identity_of("secondary.cpp")
			&& windowC->review_editor()->shown_receipt() && !windowC->comparison_surface()->geometry_pending();
	},
			 "second recorded file completes after sibling receipt delivery");
	QObject::disconnect(pendingReceiptConnection);
	expect(!receiptEnabledWithoutComparison, "sibling fanout never enables a receipt against an unresolved comparison");
	expect(windowC->review_editor()->shown_receipt()->File == windowC->comparison_surface()->current_result()->Alignment.Key.File
			   && windowC->review_editor()->shown_receipt()->Read != core::file_read_state::read,
		   "second-file receipt belongs to its own material rather than the sibling's file");
	expect(windowB->comparison_surface()->current_result() == pendingComparisonB
			   && windowB->comparison_surface()->selection() == pendingSelectionB,
		   "sibling file replacement preserves the writer's comparison and reading position");
	select_file(*owner, *windowC);
	wait_for([&] { return windowC->review_editor()->shown_receipt() && windowC->review_editor()->shown_receipt()->Read == core::file_read_state::unread; }, "returning to the first file loads its accepted shared receipt");
	action(*windowB, "toggleSelectedFileReadAction")->trigger();
	wait_for([&] { return windowB->review_editor()->shown_receipt() && windowC->review_editor()->shown_receipt()
					   && windowB->review_editor()->shown_receipt()->Read == core::file_read_state::read
					   && windowC->review_editor()->shown_receipt() == windowB->review_editor()->shown_receipt(); }, "same-file receipt state is restored before PR replacement");
	// Replace C's review before queued receipt notifications can be delivered.
	action(*windowB, "toggleSelectedFileReadAction")->trigger();
	submit(*windowC, 17);
	wait_for([&] { return windowC->session_panel()->status() == core::github_status::pull_request_ready; }, "replacement review opens with a sibling receipt notification pending");
	select_file(*owner, *windowC);
	wait_for([&] { return windowB->review_editor()->shown_receipt() && windowB->review_editor()->shown_receipt()->Read == core::file_read_state::unread; }, "surviving PR18 accepts its receipt toggle");
	expect(windowC->comparison_surface()->current_result()->Alignment.Key.PullRequest == snapshotA->PullRequest.PullRequest
			   && windowC->review_editor()->shown_receipt() && windowC->review_editor()->shown_receipt()->Read == core::file_read_state::read
			   && windowC->overview_page()->projection() && windowC->overview_page()->projection()->Progress.PrivateNotes == 2U,
		   "late PR18 receipt delivery cannot replace PR17's accepted receipt or private notes");
	action(*windowB, "quitProfilesAction")->trigger();
	wait_for([&] { return desktop->is_quiescent(); }, "Quit drains all sessions and shared profile services");
	desktop.reset();
	std::ifstream     stream{ root / "logs/diagnostics.log" };
	const std::string log{ std::istreambuf_iterator<char>{ stream }, { } };
	const auto        started = log.find("op=application outcome=started");
	const auto        stopped = log.find("op=application outcome=shutdown");
	expect(started != std::string::npos && log.find("op=application outcome=started", started + 1U) == std::string::npos, "one profile services startup");
	expect(stopped != std::string::npos && log.find("op=application outcome=shutdown", stopped + 1U) == std::string::npos, "one profile services shutdown");
	std::cout << "profile-multi-window-public-path:pass\n";
	return EXIT_SUCCESS;
}
