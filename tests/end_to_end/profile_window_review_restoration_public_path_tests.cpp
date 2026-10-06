#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/adapters/sqlite/window_review_restoration_store.hpp"
#include "bpr/app/profile_desktop.hpp"
#include "bpr/qt/diff_link_map.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/document_tab_strip.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/files_tree.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_landing.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMenu>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabBar>
#include <QTextDocument>
#include <QThread>
#include <QToolButton>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {
	using namespace bpr;
	using app::detail::github_application_fixture_access;
	void expect(bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	void wait_for(const std::function<bool()>& aCondition, const char* aMessage, const std::function<void()>& aDiagnostic = { }, int aTimeoutMs = 15000)
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
	void trigger(qt::workbench_window& aWindow, const char* aName)
	{
		auto* action = aWindow.findChild<QAction*>(QString::fromUtf8(aName));
		expect(action != nullptr, "public action exists");
		wait_for([&] { return action->isEnabled(); }, "public action enabled");
		action->trigger();
	}
	void submit(qt::workbench_window& aWindow, int aNumber)
	{
		if (!aWindow.showing_home()) {
			aWindow.home_action()->trigger();
		}
		aWindow.home_open_bar()->setText(QStringLiteral("https://github.com/octo-org/review-repository/pull/%1").arg(aNumber));
		aWindow.home_open_action()->trigger();
	}
	void settle(int aMilliseconds)
	{
		QElapsedTimer settling;
		settling.start();
		while (settling.elapsed() < aMilliseconds) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
	}
	// [[BPR-0733]]. One recently viewed row, read the way a reviewer reads it:
	// "owner/name#number", then the title where a snapshot was seen, then the
	// saved reading where one can be resumed.
	auto recent_numbers(const QListWidget& aList) -> std::vector<int>
	{
		auto numbers = std::vector<int>{ };
		for (int row = 0; row < aList.count(); ++row) {
			const auto locator = aList.item(row)->text().section(QLatin1Char{ ' ' }, 0, 0);
			expect(locator.startsWith(QStringLiteral("octo-org/review-repository#")), "recently viewed row names the exact repository");
			numbers.push_back(locator.section(QLatin1Char{ '#' }, 1, 1).toInt());
		}
		return numbers;
	}
	auto recent_row(const QListWidget& aList, int aNumber) -> int
	{
		const auto numbers = recent_numbers(aList);
		for (std::size_t row = 0; row < numbers.size(); ++row) {
			if (numbers[row] == aNumber) {
				return static_cast<int>(row);
			}
		}
		return -1;
	}
	void report_recent(app::github_application& aOwner, core::bpr_window_context aContext, const char* aStage)
	{
		const auto* window   = aOwner.window(aContext);
		const auto* list     = window->home_recent_list();
		const auto  evidence = github_application_fixture_access::evidence(aOwner);
		std::cerr << "recent-" << aStage << " window=" << aContext.Window.value() << " home=" << window->showing_home()
				  << " visible=" << list->isVisible() << " rows=" << list->count() << " current=" << list->currentRow()
				  << " resume=" << window->home_recent_resume_action()->isEnabled()
				  << " status=" << static_cast<int>(window->session_panel()->status())
				  << " reads=" << evidence.AccountReadCount << " operation=" << evidence.LastReadOperation
				  << " preview-reads=" << aOwner.status().PublicPreviewRequests
				  << " notice=" << window->home_open_notice()->text().toStdString();
		for (int row = 0; row < list->count(); ++row) {
			std::cerr << " [" << list->item(row)->text().toStdString() << ']';
		}
		std::cerr << '\n';
	}
	// [[BPR-0733]]. What this card asks a restart to be. The session is selected
	// and restored exactly as before, and every window stays on Home with what
	// the profile read listed on it, most recent first. Nothing opens by itself,
	// so no pull-request snapshot may have been read before a gesture: the
	// account read count and the signed-out request count are the fixture's own
	// evidence of that, and both must stand still while the window settles.
	void home_lists_recent(app::github_application& aOwner, core::bpr_window_context aContext, const std::vector<int>& aNumbers)
	{
		auto& window = *aOwner.window(aContext);
		auto* list   = window.home_recent_list();
		expect(list != nullptr, "Home composes a recently viewed list");
		wait_for([&] { return list->isVisible() && recent_numbers(*list) == aNumbers; }, "restart lists the retained reviews on Home, most recent first", [&] { report_recent(aOwner, aContext, "listed"); }, 30000);
		const auto reads   = github_application_fixture_access::evidence(aOwner).AccountReadCount;
		const auto preview = aOwner.status().PublicPreviewRequests;
		settle(400);
		expect(window.showing_home() && window.session_panel()->status() != core::github_status::loading_pull_request, "restart leaves the window on Home without loading a pull request");
		expect(!github_application_fixture_access::large_review_snapshot(aOwner, aContext), "restart accepts no pull request snapshot without a gesture");
		expect(github_application_fixture_access::evidence(aOwner).AccountReadCount == reads && aOwner.status().PublicPreviewRequests == preview,
			   "restart issues no pull request read before a gesture");
		expect(window.home_open_notice()->text().isEmpty(), "an accepted restart leaves no restoration notice");
		expect(recent_numbers(*list) == aNumbers, "the listed order survives the settled window");
	}
	// The list is republished whenever the profile's reading changes, and a
	// publication may reorder it, so the row is found again on every turn and
	// the gesture leaves in the same turn that found it.
	auto select_recent(app::github_application& aOwner, core::bpr_window_context aContext, int aNumber, bool aResume) -> QListWidget*
	{
		auto& window = *aOwner.window(aContext);
		auto* list   = window.home_recent_list();
		auto* resume = window.home_recent_resume_action();
		expect(list != nullptr && resume != nullptr, "Home composes a recently viewed list and its resume action");
		wait_for([&] { const auto row = list->isVisible() ? recent_row(*list, aNumber) : -1;
			if (row < 0) { return false; }
			list->setCurrentRow(row);
			return list->currentRow() == row && (!aResume || resume->isEnabled()); }, aResume ? "recently viewed row offers its saved reading" : "recently viewed row is selectable", [&] { report_recent(aOwner, aContext, "select"); });
		return list;
	}
	// Resumes the exact saved reading, which is the course a restart used to run
	// by itself.
	void resume_recent(app::github_application& aOwner, core::bpr_window_context aContext, int aNumber)
	{
		static_cast<void>(select_recent(aOwner, aContext, aNumber, true));
		aOwner.window(aContext)->home_recent_resume_action()->trigger();
	}
	// Opens the row on Overview, which is what an entry without a saved reading
	// offers. Return is read by the window's own filter on every platform.
	void open_recent(app::github_application& aOwner, core::bpr_window_context aContext, int aNumber)
	{
		auto*     list = select_recent(aOwner, aContext, aNumber, false);
		QKeyEvent activate{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
		QApplication::sendEvent(list, &activate);
	}
	void accepted(app::github_application& aOwner, core::bpr_window_context aContext, unsigned aNumber)
	{
		wait_for([&] { const auto snapshot = github_application_fixture_access::large_review_snapshot(aOwner, aContext);
            return snapshot && snapshot->PullRequest.Number.value() == aNumber; }, "exact window accepts expected PR", [&] {
			const auto* window = aOwner.window(aContext);
			const auto evidence = github_application_fixture_access::evidence(aOwner);
			std::cerr << "accepted-read window=" << aContext.Window.value() << " expected-pr=" << aNumber
				<< " status=" << static_cast<int>(window->session_panel()->status())
				<< " recording=" << evidence.RecordingFailure << " operation=" << evidence.LastReadOperation
				<< " diagnostic=" << window->session_panel()->diagnostic_text()
				<< " text=" << window->status_text().toStdString() << " notice=" << window->home_open_notice()->text().toStdString() << '\n'; });
		expect(!aOwner.window(aContext)->showing_home(), "accepted review is visible");
	}
	auto restoration_store(const std::filesystem::path& aRoot)
	{
		for (const auto& entry : std::filesystem::recursive_directory_iterator(aRoot)) {
			if (entry.path().filename() == "window-restoration.sqlite3") {
				return adapters::sqlite::make_window_review_restoration_store(entry.path().parent_path().parent_path().parent_path());
			}
		}
		expect(false, "profile restoration database exists");
		return adapters::sqlite::make_window_review_restoration_store(aRoot);
	}
	auto reading(qt::workbench_window& aWindow) -> core::window_review_position
	{
		auto&       controls = *aWindow.epoch_selectors();
		const auto  left     = core::parse_git_commit_oid(controls.left_oid_editor()->text().toStdString());
		const auto  right    = core::parse_git_commit_oid(controls.right_oid_editor()->text().toStdString());
		const auto* surface  = aWindow.comparison_surface();
		expect(left && right && surface->current_context() && surface->current_result() && surface->selection(), "public reading has exact anchors, commit OIDs, file and semantic selection");
		// Accepted identities remain available even when their history rows
		// are not resident in the independently paged selector model.
		const auto& key = surface->current_context()->Key;
		return { { { key.LeftAnchor, *left, core::anchor_availability::available },
				   { key.RightAnchor, *right, core::anchor_availability::available } },
				 key.File,
				 surface->selection() };
	}
	void select_reading(app::github_application& aOwner, core::bpr_window_context aContext, int aRow, bool aCrossUpload = false)
	{
		auto&                                           window = *aOwner.window(aContext);
		std::optional<core::comparison_anchor_identity> expectedLeft;
		std::optional<core::comparison_anchor_identity> expectedRight;
		// Read and Review own the active tab's pair; Overview and Discuss own
		// the window's. Enter the reading surface before changing its pair.
		window.modality()->action(qt::review_modality::read)->trigger();
		QApplication::processEvents();
		wait_for([&] { const auto status = aOwner.status(aContext); return status && status->EpochStatus == core::epoch_status::ready; }, "reading epoch ready");
		if (aRow == 1 || aCrossUpload) {
			auto&      controls         = *window.epoch_selectors();
			auto*      model            = controls.history_model();
			int        left             = -1;
			int        right            = -1;
			const auto anchorsAvailable = [&] {
				left  = -1;
				right = -1;
				for (int row = 0; row < model->rowCount(); ++row) {
					const auto anchor    = model->anchor_at(model->index(row, 0));
					const auto leftKind  = aCrossUpload && aRow == 0 ? core::comparison_anchor_kind::epoch_base : core::comparison_anchor_kind::epoch_head;
					const auto rightKind = aCrossUpload ? core::comparison_anchor_kind::epoch_head : core::comparison_anchor_kind::epoch_base;
					if (anchor && anchor->Identity.Epoch.value() == 1 && anchor->Identity.Kind == leftKind) {
						left = row;
					}
					if (anchor && anchor->Identity.Epoch.value() == (aCrossUpload ? 2 : 1) && anchor->Identity.Kind == rightKind) {
						right = row;
					}
				}
				return left >= 0 && right >= 0;
			};
			for (int page = 0; page < 2 && !anchorsAvailable(); ++page) {
				wait_for([&] { return anchorsAvailable() || controls.return_to_newest_action()->isEnabled() || controls.load_history_action()->isEnabled(); }, "public retained history available or loadable");
				if (!anchorsAvailable()) {
					auto* load = controls.return_to_newest_action()->isEnabled() ? controls.return_to_newest_action() : controls.load_history_action();
					load->trigger();
					wait_for([&] { return anchorsAvailable() || load->isEnabled(); }, "requested history page settles");
				}
			}
			expect(anchorsAvailable(), "requested retained upload anchors available");
			expectedLeft  = model->anchor_at(model->index(left, 0))->Identity;
			expectedRight = model->anchor_at(model->index(right, 0))->Identity;
			controls.left_selector()->setCurrentIndex(left);
			controls.right_selector()->setCurrentIndex(right);
			wait_for([&] { return controls.left_selector()->currentIndex() == left && controls.right_selector()->currentIndex() == right; }, "exact retained pair accepted");
		}
		auto*       files = window.files_drawer()->contents_view();
		const auto  path  = aRow == 0 ? QStringLiteral("review.cpp") : QStringLiteral("secondary.cpp");
		QModelIndex selected;
		wait_for([&] { for (int row = 0; row < files->model()->rowCount(); ++row) {
			const auto index = files->model()->index(row, 0);
			if (index.data(Qt::DisplayRole).toString() == path) { selected = index; return true; }
		} return false; }, "exact independent file path available");
		files->selectionModel()->setCurrentIndex(selected, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
		auto* surface = window.comparison_surface();
		wait_for([&] { return surface->current_result() && surface->current_semantic_index() && surface->current_context()
						   && surface->current_context()->Key.File == core::file_identity_of(path.toStdString())
						   && (!expectedLeft || surface->current_context()->Key.LeftAnchor == expectedLeft)
						   && (!expectedRight || surface->current_context()->Key.RightAnchor == expectedRight)
						   && !surface->geometry_pending(); }, "installed Git produces exact semantic file comparison", [&] {
			std::cerr << "reading row=" << aRow << " cross=" << aCrossUpload << " path=" << path.toStdString()
				<< " result=" << bool(surface->current_result()) << " context=" << bool(surface->current_context())
				<< " geometry=" << surface->geometry_pending() << " epoch=" << static_cast<int>(aOwner.status(aContext)->EpochStatus)
				<< " status=" << window.status_text().toStdString() << '\n';
			if (surface->current_context()) {
				const auto& key = surface->current_context()->Key;
				std::cerr << "file-match=" << (key.File == core::file_identity_of(path.toStdString()))
					<< " left-match=" << (!expectedLeft || key.LeftAnchor == expectedLeft)
					<< " right-match=" << (!expectedRight || key.RightAnchor == expectedRight) << '\n';
			}
			for (const auto& tab : window.document_tabs()->tabs()) {
				std::cerr << "tab=" << tab.Id.value() << " active=" << tab.Active << " state=" << static_cast<int>(tab.State)
					<< " own=" << tab.OwnComparison << " path=" << tab.Path.toStdString() << '\n';
			} }, 60000);
		expect(surface->select_conversation_line(core::diff_side::right, aRow == 0 ? 3U : 1U), "public line gesture selects exact semantic location");
	}
	void restored_reading(qt::workbench_window& aWindow, const core::window_review_position& aExpected, const std::function<void()>& aDiagnostic = { })
	{
		auto* surface = aWindow.comparison_surface();
		wait_for([&] { return surface->current_context() && surface->current_result() && surface->selection() && !surface->geometry_pending()
						   && core::parse_git_commit_oid(aWindow.epoch_selectors()->left_oid_editor()->text().toStdString())
						   && core::parse_git_commit_oid(aWindow.epoch_selectors()->right_oid_editor()->text().toStdString())
						   && reading(aWindow) == aExpected; }, "restart restores semantic reading without a file gesture", [&] { std::cerr << "restored-reading context=" << bool(surface->current_context()) << " result=" << bool(surface->current_result())
																																																 << " semantic=" << bool(surface->current_semantic_index()) << " selection=" << bool(surface->selection())
																																																 << " geometry=" << surface->geometry_pending() << " status=" << aWindow.status_text().toStdString()
				<< " notice=" << aWindow.home_open_notice()->text().toStdString() << '\n';
			std::cerr << " expected-line=" << (aExpected.Selection ? aExpected.Selection->PreferredLine.Line : 99999U)
                << " actual-line=" << (surface->selection() ? surface->selection()->PreferredLine.Line : 99999U)
                << " left-commit=" << (core::parse_git_commit_oid(aWindow.epoch_selectors()->left_oid_editor()->text().toStdString()) == aExpected.Comparison.Left.Commit)
                << " right-commit=" << (core::parse_git_commit_oid(aWindow.epoch_selectors()->right_oid_editor()->text().toStdString()) == aExpected.Comparison.Right.Commit)
                << " file=" << (surface->current_context() && aExpected.File == surface->current_context()->Key.File)
                << " selection=" << (surface->selection() == aExpected.Selection)
                << " left-text=" << aWindow.epoch_selectors()->left_oid_editor()->text().toStdString()
                << " right-text=" << aWindow.epoch_selectors()->right_oid_editor()->text().toStdString() << '\n';
            if (aDiagnostic) { aDiagnostic(); } }, 60000);
		expect(reading(aWindow) == aExpected, "restart preserves exact anchors, commit OIDs, file and complete semantic identity");
	}
	auto read_vault() -> std::string
	{
		std::array<char, 2049U> bytes{ };
		std::cin.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
		const auto size = std::cin.gcount();
		expect(size <= 2048, "vault input bounded");
		return { bytes.data(), static_cast<std::size_t>(size) };
	}
	void check_vault_protocol(std::string_view aBytes)
	{
		expect(app::detail::export_github_fixture_vault(nullptr).empty(), "absent fixture vault exports no material");
		expect(!app::detail::import_github_fixture_vault(std::string(2049U, '0')), "oversized vault refused");
		if (aBytes.empty()) {
			return;
		}
		expect(!app::detail::import_github_fixture_vault(aBytes.substr(0U, aBytes.size() - 2U)), "truncated slot refused");
		auto hostile = std::string{ aBytes };
		// Seven uint64 fields precede the first bounded slot length.
		hostile.replace(112U, 8U, "7fffffff");
		expect(!app::detail::import_github_fixture_vault(hostile), "embedded oversized slot length refused before allocation");
	}
	void capture(qt::workbench_window& aWindow, const std::string& aName)
	{
		const auto root = qEnvironmentVariable("BPR_REVIEW_RESTORATION_ARTIFACT_ROOT");
		if (!root.isEmpty()) {
			expect(QDir{ }.mkpath(root), "artifact directory available");
			expect(aWindow.grab().save(root + QLatin1Char{ '/' } + QString::fromStdString(aName) + QStringLiteral(".png")), "full-window restoration raster saved");
		}
	}
	void expect_no_vault_attempts(const app::detail::github_public_path_fixture_evidence& aEvidence)
	{
		expect(aEvidence.VaultStoreAttempts == 0U && aEvidence.VaultRetrieveAttempts == 0U && aEvidence.VaultDeleteAttempts == 0U
				   && aEvidence.RefreshStoreAttempts == 0U && aEvidence.RefreshRetrieveAttempts == 0U && aEvidence.RefreshDeleteAttempts == 0U,
			   "no vault request reaches any admission boundary, including failed retrieval");
	}
	void expect_no_account_attempts(const app::detail::github_public_path_fixture_evidence& aEvidence)
	{
		expect(aEvidence.AccountFactoryAttempts == 0U && aEvidence.BootstrapFactoryAttempts == 0U
				   && aEvidence.AccountFactoryCalls == 0U && aEvidence.BootstrapFactoryCalls == 0U && aEvidence.AccountReadCount == 0,
			   "zero account/bootstrap factory attempts and accepted calls");
	}
	void expect_anonymous(app::github_application& aOwner, bool aRead)
	{
		const auto status   = aOwner.status();
		const auto evidence = github_application_fixture_access::evidence(aOwner);
		expect_no_vault_attempts(evidence);
		expect_no_account_attempts(evidence);
		expect((!aRead || status.PublicPreviewRequests > 0U) && status.PublicPreviewVaultEffects == 0U && status.PublicPreviewHeaderEffects == 0U, "anonymous path has zero vault/header effects");
		expect(!evidence.AccountTransportSelected && !evidence.BootstrapTransportSelected && evidence.VaultStores == 0U && evidence.DeviceTransportSubmissions == 0U, "anonymous path selects no account/bootstrap/enrollment transport");
	}
	auto seed_auth(app::github_application& aOwner, core::bpr_window_context aFirst) -> std::vector<core::window_review_position>
	{
		auto* first = aOwner.window(aFirst);
		// [[BPR-0732]]. A pasted public link no longer stops at sign-in where
		// preview is composed (it opens signed out); the account course signs
		// in from the card first, then pastes.
		expect(first->home_landing()->state() == qt::landing_state::sign_in && !first->home_list()->queue_shown(), "fresh Home is signed out with no queue");
		first->home_landing()->primary_button()->click();
		wait_for([&] { return first->session_panel()->status() == core::github_status::session_ready && !aOwner.status().PublicPreviewSignedOut; }, "Home sign-in reaches an account session");
		expect(first->home_list()->queue_shown(), "an account shows the queue");
		submit(*first, 17);
		expect(first->findChild<QPushButton*>(QStringLiteral("homeSetupContinue"))->isHidden(), "an account opens the destination without a stop");
		accepted(aOwner, aFirst, 17);
		wait_for([&] { return github_application_fixture_access::device_binding_durable(aOwner); }, "remembered grant settles before Quit");
		select_reading(aOwner, aFirst, 0);
		const auto firstReading = reading(*first);
		trigger(*first, "newBprWindowAction");
		wait_for([&] { return aOwner.windows().size() == 2U; }, "second stable window created");
		const auto windows = aOwner.windows();
		const auto second  = windows.front() == aFirst ? windows.back() : windows.front();
		expect(aOwner.window(second)->showing_home() && aOwner.window(second)->home_open_notice()->text().isEmpty(), "fresh sibling has no fabricated destination");
		submit(*aOwner.window(second), 18);
		accepted(aOwner, second, 18);
		select_reading(aOwner, second, 1);
		const auto secondReading = reading(*aOwner.window(second));
		expect(firstReading.Comparison != secondReading.Comparison && firstReading.File != secondReading.File && firstReading.Selection != secondReading.Selection, "windows retain different exact pairs, files and semantic locations");
		aOwner.window(second)->home_action()->trigger();
		expect(aOwner.window(second)->showing_home(), "Home usable over accepted destination");
		expect(github_application_fixture_access::evidence(aOwner).VaultStores == 1U, "two windows share one enrollment");
		return { firstReading, secondReading };
	}
	void check_contexts(app::github_application& aOwner, std::uint64_t aGeneration)
	{
		for (const auto context : aOwner.windows()) {
			expect(context.Generation.value() == aGeneration, "restart advances exact stable generation");
			if (aGeneration > 1U) {
				auto stale       = context;
				stale.Generation = core::bpr_window_generation{ aGeneration - 1U };
				expect(aOwner.window(stale) == nullptr, "previous process generation refused");
			}
		}
	}
}
auto main(int aArgc, char** aArgv) -> int
{
	expect(aArgc == 4, "phase, root and expected generation required");
	const auto phase      = std::string{ aArgv[1] };
	const auto root       = std::filesystem::canonical(aArgv[2]);
	const auto anonymous  = phase.starts_with("anonymous") || phase == "capability";
	const auto seed       = phase.ends_with("seed");
	const auto failure    = phase == "missing-grant" || phase == "missing-server" || phase == "corrupt" || phase == "future" || phase == "capability" || phase == "failed-read";
	const auto vaultBytes = read_vault();
	const auto generation = std::stoull(aArgv[3]);
	check_vault_protocol(vaultBytes);
	const auto                                         readingFailure = phase.starts_with("unavailable-");
	std::optional<application::window_review_snapshot> unavailableReadings;
	if (readingFailure) {
		auto store    = restoration_store(root);
		auto retained = store->load({ });
		expect(retained && retained->Records.front().Position, "unavailable course begins with accepted reading");
		// [[BPR-0733]]. A gesture on the recently viewed list is what opens a
		// retained reading now, so the controlled unavailable identity has to be
		// the one the list carries as well as the one the window retained.
		auto        readings    = std::vector<core::window_review_position*>{ &*retained->Records.front().Position };
		const auto& destination = retained->Records.front().Destination;
		for (auto& entry : retained->Recent) {
			if (entry.Destination == destination && entry.Position) {
				readings.push_back(&*entry.Position);
			}
		}
		expect(readings.size() == 2U, "the retained destination is listed with its exact reading");
		for (auto* target : readings) {
			auto& position = *target;
			if (phase == "unavailable-content") {
				// A cloned cache has a different directory identity; retain its record.
			} else if (phase == "unavailable-anchor") {
				position.Comparison.Left.Identity.Epoch = core::epoch_ordinal{ 999 };
			} else if (phase == "unavailable-file") {
				position.File = core::file_identity_of("removed-after-recording.cpp");
				position.Selection.reset();
			} else {
				expect(position.Selection.has_value(), "semantic unavailable course has exact selection");
				auto material = position.Selection->Comparison.bytes();
				material.front() ^= 1U;
				position.Selection->Comparison               = core::file_comparison_digest{ material };
				position.Selection->PreferredLine.Comparison = position.Selection->Comparison;
			}
		}
		const auto revision = retained->Revision;
		++retained->Revision;
		expect(store->save(*retained, revision, { }).has_value(), "controlled unavailable identity saved transactionally");
		unavailableReadings = *retained;
	}
	std::vector<core::window_review_position>          seededReadings;
	std::optional<application::window_review_snapshot> expectedReadings;
	std::optional<core::window_review_position>        replacementReading;
	const auto                                         readingRace = phase == "auth-reading-gesture" || phase == "auth-reading-unchanged" || phase == "auth-reading-home";
	if (phase == "auth-restore" || phase == "auth-advance" || phase.starts_with("tabs-") || readingRace) {
		const auto retained = restoration_store(root)->load({ });
		expect(retained.has_value() && retained->Records.size() == 2U, "restart reads two durable positions");
		expectedReadings = *retained;
	}
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	app::detail::github_public_path_fixture_options options;
	options.ApplicationDataRoot        = root;
	options.PublicPreviewEnabledCourse = anonymous && phase != "capability";
	options.IndependentWindowReads     = true;
	options.RepeatReadSessions         = true;
	options.RegisteredDeviceCourse     = !anonymous;
	options.PersistDeviceBindings      = true;
	options.EpochPhase                 = !anonymous && !seed ? app::detail::github_epoch_fixture_phase::reuse : app::detail::github_epoch_fixture_phase::single;
	options.SharedDeviceVault          = vaultBytes.empty() ? app::detail::make_shared_github_fixture_vault() : app::detail::import_github_fixture_vault(vaultBytes);
	expect(options.SharedDeviceVault != nullptr, "retained synthetic vault decodes");
	app::github_application* owner    = nullptr;
	bool                     terminal = false;
	auto                     desktop  = github_application_fixture_access::start_desktop(phase == "capability" ? app::detail::github_public_path_fixture::ghes_3_18 : anonymous ? app::detail::github_public_path_fixture::github_com
																																												: app::detail::github_public_path_fixture::profile_window_restoration_v1,
																						 options,
																						 [&](auto aResult) { terminal = true; if(aResult) { owner = *aResult; } });
	wait_for([&] { return terminal; }, "desktop startup finishes");
	expect(owner != nullptr, "desktop admitted");
	if (phase == "auth-stale" || phase == "auth-home") {
		github_application_fixture_access::hold_window_read_terminal(*owner, { core::default_bpr_profile_id, core::bpr_window_id{ 1U }, core::bpr_window_generation{ generation } }, true);
	}
	const auto expectedCount = anonymous || phase == "auth-verify" || seed ? 1U : 2U;
	wait_for([&] { return owner->windows().size() == expectedCount; }, "exact retained window count");
	check_contexts(*owner, generation);
	const auto firstContext = core::bpr_window_context{ core::default_bpr_profile_id, core::bpr_window_id{ phase == "auth-verify" ? 2U : 1U }, core::bpr_window_generation{ generation } };
	auto*      first        = owner->window(firstContext);
	expect(first != nullptr, "stable first identity restored");
	bool                                        readingGestureQueued    = false;
	bool                                        readingGestureDelivered = false;
	std::optional<core::window_review_position> gestureReading;
	if (readingRace) {
		auto* surface = first->comparison_surface();
		expect(!surface->current_result(), "reading race installed before restored diff publication");
		QObject::connect(surface->right_pane()->document(), &QTextDocument::contentsChanged, &application, [&] {
			auto* current = first->comparison_surface();
			if (readingGestureQueued || !current->current_result() || !current->current_semantic_index() || !current->current_context()
				|| current->current_context()->Key.File != expectedReadings->Records[0].Position->File) {
				return;
			}
			readingGestureQueued = true;
			// The native document publishes inside the diff projection. This
			// callback is queued before its deferred restoration continuation.
			QMetaObject::invokeMethod(&application, [&] {
				expect(first->home_open_notice()->text().startsWith(QStringLiteral("Resuming")), "gesture precedes deferred reading restoration completion");
				if (phase == "auth-reading-home") {
					first->home_action()->trigger();
				} else if (phase == "auth-reading-unchanged") {
					auto* gestureSurface = first->comparison_surface();
					for (unsigned attempt = 0; attempt < 4U && gestureSurface->geometry_pending(); ++attempt) {
						QApplication::sendPostedEvents(gestureSurface, QEvent::MetaCall);
					}
					expect(!gestureSurface->geometry_pending(), "unchanged gesture geometry settles without delivering restoration continuation");
					const auto before = reading(*first);
					auto* map = gestureSurface->link_map();
					expect(before.Selection && before.Selection->Kind == core::semantic_selection_kind::group && map->selected() && map->selected()->Group == before.Selection->Group, "native link map targets current semantic group");
					expect(gestureSurface->current_result()->Alignment.Groups.size() == 1U, "unchanged gesture has sole first group");
					expect(before.Selection != expectedReadings->Records[0].Position->Selection, "current group differs from retained line");
					expect(first->home_open_notice()->text().startsWith(QStringLiteral("Resuming")), "unchanged gesture retains pending restoration stage");
					const auto callbacks = map->statistics().SelectionCallbacks;
					QKeyEvent key{QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier};
					QApplication::sendEvent(map, &key);
					expect(map->statistics().SelectionCallbacks == callbacks + 1U, "native unchanged group gesture delivered exactly once");
					gestureReading = reading(*first);
					expect(*gestureReading == before, "unchanged native gesture preserves full semantic identity");
				} else {
					expect(first->comparison_surface()->select_conversation_line(core::diff_side::right, 2U), "newer public semantic gesture accepted during restoration");
					gestureReading = reading(*first);
					expect(gestureReading->Selection != expectedReadings->Records[0].Position->Selection, "newer gesture differs from retained line");
				}
				QMetaObject::invokeMethod(&application, [&] { readingGestureDelivered = true; }, Qt::QueuedConnection); }, Qt::QueuedConnection);
		});
	}
	if (seed) {
		expect(first->showing_home() && first->home_open_notice()->text().isEmpty(), "fresh window is Home without resume target");
		if (anonymous) {
			owner->activate_public_preview();
			wait_for([&] { return owner->status().PublicPreviewSignedOut; }, "anonymous authority accepted");
			wait_for([&] { return first->github_open_action()->isEnabled(); }, "anonymous open enabled");
			// [[BPR-0771]]. A pull request is named in the Home bar; the locator is retired.
			first->home_open_bar()->setText(QStringLiteral("octo-org/review-repository#17"));
			first->home_open_action()->trigger();
			accepted(*owner, firstContext, 17);
		} else {
			seededReadings = seed_auth(*owner, firstContext);
		}
	} else if (phase == "auth-advance") {
		const auto second = core::bpr_window_context{ firstContext.Profile, core::bpr_window_id{ 2U }, firstContext.Generation };
		// Window 2 read #18 after window 1 read #17, so both windows list #18
		// first. Neither has opened anything: the list is the only way back.
		home_lists_recent(*owner, firstContext, { 18, 17 });
		home_lists_recent(*owner, second, { 18, 17 });
		resume_recent(*owner, firstContext, 17);
		accepted(*owner, firstContext, 17);
		restored_reading(*first, *expectedReadings->Records[0].Position, [&] {
			const auto status = owner->status(firstContext);
			const auto work   = github_application_fixture_access::diff_work_evidence(*owner, firstContext);
			std::cerr << "advance-reading epoch=" << (status ? static_cast<int>(status->EpochStatus) : -1)
					  << " navigation=" << work.NavigationReady << " outstanding=" << work.OutstandingJobs
					  << " epoch-outstanding=" << work.EpochOutstandingJobs << " workbench-outstanding=" << work.WorkbenchOutstandingRequests << '\n';
		});
		resume_recent(*owner, second, 18);
		accepted(*owner, second, 18);
		restored_reading(*owner->window(second), *expectedReadings->Records[1].Position);
		select_reading(*owner, firstContext, 0, true);
		select_reading(*owner, second, 1, true);
		const auto firstReading  = reading(*first);
		const auto secondReading = reading(*owner->window(second));
		expect(firstReading.Comparison.Left.Identity.Epoch.value() == 1 && firstReading.Comparison.Right.Identity.Epoch.value() == 2
				   && secondReading.Comparison.Left.Identity.Epoch.value() == 1 && secondReading.Comparison.Right.Identity.Epoch.value() == 2,
			   "both windows compare exact retained upload epochs");
		expect(firstReading.Comparison.Left.Commit == expectedReadings->Records[0].Position->Comparison.Left.Commit
				   && secondReading.Comparison.Left.Commit == expectedReadings->Records[1].Position->Comparison.Left.Commit
				   && firstReading.Comparison.Right.Commit != expectedReadings->Records[0].Position->Comparison.Right.Commit
				   && secondReading.Comparison.Right.Commit != secondReading.Comparison.Left.Commit,
			   "new upload differs while original retained commit identities survive process replacement");
		expect(firstReading.Comparison != secondReading.Comparison && firstReading.File != secondReading.File
				   && firstReading.Selection->PreferredLine.Line != secondReading.Selection->PreferredLine.Line,
			   "independent windows retain distinct cross-upload pairs, files and semantic lines");
		seededReadings = { firstReading, secondReading };
	} else if (phase == "tabs-edit" || phase == "tabs-restore") {
		if (phase == "tabs-restore") {
			const auto& record = expectedReadings->Records[0];
			const auto& tabs   = *record.Tabs;
			std::cerr << "saved-tabs count=" << tabs.Tabs.size() << " active=" << *tabs.Active
					  << " position-equals-active=" << (record.Position == std::optional{ tabs.Tabs[*tabs.Active] })
					  << " position-line=" << (record.Position->Selection ? record.Position->Selection->PreferredLine.Line : 99999U)
					  << " active-line=" << (tabs.Tabs[*tabs.Active].Selection ? tabs.Tabs[*tabs.Active].Selection->PreferredLine.Line : 99999U) << '\n';
		}
		home_lists_recent(*owner, firstContext, std::vector{ 18, 17 });
		resume_recent(*owner, firstContext, 17);
		accepted(*owner, firstContext, 17);
		restored_reading(*first, *expectedReadings->Records[0].Position);
		auto* strip = first->document_tabs();
		wait_for([&] { return !strip->tabs().empty(); }, "Resume publishes document tabs");
		const auto activate = [&](int aIndex, const core::window_review_position& aView) {
			std::cerr << "tabs-course:activate " << aIndex << '\n';
			strip->tab_bar()->setCurrentIndex(aIndex);
			wait_for([&] { return first->comparison_surface()->current_result() && first->comparison_surface()->selection()
							   && !first->comparison_surface()->geometry_pending()
							   && core::parse_git_commit_oid(first->epoch_selectors()->left_oid_editor()->text().toStdString())
							   && core::parse_git_commit_oid(first->epoch_selectors()->right_oid_editor()->text().toStdString())
							   && reading(*first) == aView; }, "tab switch presents exact pair, file and semantic reading", [&] {
				const auto* surface = first->comparison_surface();
				std::cerr << "tab-switch result=" << bool(surface->current_result()) << " selection=" << bool(surface->selection())
					<< " left=" << first->epoch_selectors()->left_oid_editor()->text().toStdString()
					<< " right=" << first->epoch_selectors()->right_oid_editor()->text().toStdString() << '\n';
				for (const auto& tab : strip->tabs()) { std::cerr << "tab " << tab.Id.value() << " active=" << tab.Active << " state=" << static_cast<int>(tab.State) << '\n'; } }, 60000);
		};
		if (phase == "tabs-edit") {
			std::cerr << "tabs-course:open-background\n";
			const auto original = reading(*first);
			auto*      files    = first->files_tree();
			const auto index    = files->tree_model()->index_for_file(*original.File);
			auto       menu     = std::unique_ptr<QMenu>{ files->make_file_menu(index, first) };
			expect(menu && menu->actions().size() == 1, "file menu offers Open in New Tab");
			menu->actions().front()->trigger();
			wait_for([&] { return strip->tabs().size() == 2U; }, "Open in New Tab adds one background view");
			expect(strip->tabs()[0].Active && reading(*first) == original, "background open preserves active reading");
			first->next_tab_action()->trigger();
			wait_for([&] { return strip->tabs()[1].Active; }, "next tab activates new file view");
			std::cerr << "tabs-course:select-distinct-pair\n";
			select_reading(*owner, firstContext, 1);
			select_reading(*owner, firstContext, 0);
			const auto changed = reading(*first);
			expect(changed.File == original.File && changed.Comparison != original.Comparison, "two tabs compare distinct exact pairs of one file");
			wait_for([&] { return strip->tabs()[0].OwnComparison; }, "older tab label identifies its own pair");
			std::cerr << "tabs-course:modality-round-trip\n";
			activate(0, original);
			first->modality()->action(qt::review_modality::discuss)->trigger();
			wait_for([&] { return core::parse_git_commit_oid(first->epoch_selectors()->right_oid_editor()->text().toStdString()) == changed.Comparison.Right.Commit; }, "Discuss presents window comparison");
			first->modality()->action(qt::review_modality::read)->trigger();
			activate(0, original);
			activate(1, changed);
			std::cerr << "tabs-course:duplicate-reading\n";
			strip->new_tab_button()->click();
			wait_for([&] { return strip->tabs().size() == 3U && strip->tabs()[2].Active; }, "plus duplicates exact active view");
			expect(reading(*first) == changed, "duplicate preserves exact semantic reading");
			expect(first->comparison_surface()->select_conversation_line(core::diff_side::right, 1U), "duplicate takes a distinct reading position");
			const auto duplicateReading = reading(*first);
			expect(duplicateReading.Selection != changed.Selection, "same-pair tabs have distinct semantic positions");
			QApplication::processEvents();
			activate(1, changed);
			activate(2, duplicateReading);
			first->close_tab_action()->trigger();
			wait_for([&] { return strip->tabs().size() == 2U && strip->tabs()[1].Active; }, "close chooses left neighbour at end");
			activate(0, original);
		} else {
			const auto& saved = *expectedReadings->Records[0].Tabs;
			expect(saved.Tabs.size() == 2U && strip->tabs().size() == 2U, "restart retains both document tabs");
			expect(saved.Tabs[0].File == saved.Tabs[1].File && saved.Tabs[0].Comparison != saved.Tabs[1].Comparison, "durable tabs retain distinct pairs of one file");
			expect(strip->tabs()[*saved.Active].Active, "restart retains active tab in order");
			activate(1, saved.Tabs[1]);
			activate(0, saved.Tabs[0]);
			first->close_tab_action()->trigger();
			wait_for([&] { return strip->tabs().size() == 1U; }, "closing first tab leaves its right neighbour");
			first->close_tab_action()->trigger();
			wait_for([&] { return strip->tabs().empty() && !first->comparison_surface()->current_result(); }, "closing final tab clears its comparison");
			settle(100);
			expect(strip->tabs().empty(), "late projection does not recreate a closed final tab");
			auto* files = first->files_tree();
			if (!files->isVisible()) {
				first->files_drawer_toggle_action()->trigger();
			}
			wait_for([&] { return files->isVisible(); }, "Files drawer is visible for the reopen click");
			const auto index = files->tree_model()->index_for_file(*saved.Tabs[0].File);
			expect(index.isValid() && files->currentIndex() == index, "file cursor remains after last tab closes");
			files->scrollTo(index);
			wait_for([&] { return !files->visualRect(index).isEmpty() && files->viewport()->rect().contains(files->visualRect(index).center()) && files->indexAt(files->visualRect(index).center()) == index; }, "retained file has a visible click target");
			const auto point = files->visualRect(index).center();
			auto       press = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ point }, QPointF{ files->viewport()->mapToGlobal(point) }, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
			QApplication::sendEvent(files->viewport(), &press);
			auto release = QMouseEvent{ QEvent::MouseButtonRelease, QPointF{ point }, QPointF{ files->viewport()->mapToGlobal(point) }, Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
			QApplication::sendEvent(files->viewport(), &release);
			settle(100);
			std::cerr << "tabs-reopen count=" << strip->tabs().size() << " result=" << bool(first->comparison_surface()->current_result()) << " click=" << point.x() << "," << point.y() << " target=" << (files->indexAt(point) == index) << '\n';
			wait_for([&] { return strip->tabs().size() == 1U && first->comparison_surface()->current_result(); }, "clicking the retained file opens a new first tab");
		}
	} else if (readingRace) {
		home_lists_recent(*owner, firstContext, { 18, 17 });
		resume_recent(*owner, firstContext, 17);
		wait_for([&] { return readingGestureDelivered; }, "queued gesture and restoration continuation cross decision barrier", [&] {
			const auto* surface = first->comparison_surface();
			const auto status = owner->status(firstContext);
			const auto work = github_application_fixture_access::diff_work_evidence(*owner, firstContext);
			std::cerr << "reading-race queued=" << readingGestureQueued << " result=" << bool(surface->current_result())
				<< " semantic=" << bool(surface->current_semantic_index()) << " selection=" << bool(surface->selection())
				<< " epoch=" << (status ? static_cast<int>(status->EpochStatus) : -1)
				<< " navigation=" << work.NavigationReady << " outstanding=" << work.OutstandingJobs
				<< " epoch-outstanding=" << work.EpochOutstandingJobs << " workbench-outstanding=" << work.WorkbenchOutstandingRequests
				<< " notice=" << first->home_open_notice()->text().toStdString() << '\n'; }, 60000);
		if (phase == "auth-reading-home") {
			expect(first->showing_home(), "Home defeats late reading restoration");
		} else {
			expect(!first->showing_home() && reading(*first) == *gestureReading, "newer semantic gesture defeats late restored line");
		}
	} else if (readingFailure) {
		const auto second = core::bpr_window_context{ firstContext.Profile, core::bpr_window_id{ 2U }, firstContext.Generation };
		home_lists_recent(*owner, firstContext, { 18, 17 });
		home_lists_recent(*owner, second, { 18, 17 });
		resume_recent(*owner, firstContext, 17);
		wait_for([&] { const auto text = first->home_open_notice()->text(); return !text.isEmpty() && !text.startsWith(QStringLiteral("Resuming")) && text != QString::fromStdString(first->session_panel()->status_text()); }, "unavailable reading reports explicit failure", [&] {
			const auto work = github_application_fixture_access::diff_work_evidence(*owner, firstContext);
			std::cerr << "unavailable-reading navigation=" << work.NavigationReady << " outstanding=" << work.OutstandingJobs
				<< " workbench-outstanding=" << work.WorkbenchOutstandingRequests << " result=" << bool(first->comparison_surface()->current_result())
				<< " notice=" << first->home_open_notice()->text().toStdString() << '\n'; }, 60000);
		expect(first->showing_home() && first->home_open_bar()->isEnabled(), "unavailable reading leaves usable Home");
		expect(!first->comparison_surface()->selection(), "unavailable reading substitutes no semantic location");
		if (phase != "unavailable-content") {
			resume_recent(*owner, second, 18);
			accepted(*owner, second, 18);
			expect(unavailableReadings->Records[1].Position.has_value(), "unaffected sibling has recorded reading");
			restored_reading(*owner->window(second), *unavailableReadings->Records[1].Position);
			const auto status = owner->status(firstContext);
			if (phase == "unavailable-anchor") {
				expect(status && status->EpochStatus == core::epoch_status::unavailable, "missing retained anchor reports unavailable epoch");
			} else {
				expect(status && status->EpochStatus == core::epoch_status::ready, "file and semantic negatives have an available accepted epoch");
			}
		}
	} else if (phase == "failed-read") {
		// [[BPR-0733]]. Startup selects the session and stops, so the read that
		// cannot be served is the one this gesture asks for: the newest listed
		// entry names a pull request the server does not have.
		home_lists_recent(*owner, firstContext, { 99, 17 });
		resume_recent(*owner, firstContext, 99);
		wait_for([&] { const auto text = first->home_open_notice()->text(); return !text.isEmpty() && !text.startsWith(QStringLiteral("Resuming")) && text != QString::fromStdString(first->session_panel()->status_text()); }, "resumed read failure explicitly visible");
		std::cerr << "resumed-read-error=" << first->home_open_notice()->text().toStdString() << '\n';
		expect(first->showing_home() && first->home_open_bar()->isEnabled(), "failed resume leaves usable Home");
		expect(!github_application_fixture_access::large_review_snapshot(*owner, firstContext), "failed resume fabricates no content");
	} else if (failure) {
		// [[BPR-0733]]. Startup selects a session and stops, so no review is
		// being restored and a refusal is no longer a failed restore. What the
		// retained destination itself refuses is still said at once - an
		// unconfigured server, a store that cannot be read - because that is
		// about what was saved. An authority that cannot be brought back says
		// nothing: the destination was fine and nothing was being opened, so
		// neither a missing grant nor an unavailable signed-out capability is
		// a failure of anything the reviewer asked for.
		const auto refused = phase == "missing-server" || phase == "corrupt" || phase == "future";
		if (refused) {
			wait_for([&] { const auto text = first->home_open_notice()->text(); return !text.isEmpty() && !text.startsWith(QStringLiteral("Resuming")) && text != QString::fromStdString(first->session_panel()->status_text()); }, "unreadable saved reviews are explicitly visible");
			std::cerr << "restoration-error=" << first->home_open_notice()->text().toStdString() << '\n';
		}
		settle(600);
		auto* listed = first->home_recent_list();
		wait_for([&] { return listed->count() == (phase == "capability" ? 1 : 0); }, "a refused restart lists only what its session admits", [&] { report_recent(*owner, firstContext, "refused"); });
		std::cerr << "restoration-refusal=" << phase << " notice=" << first->home_open_notice()->text().toStdString() << '\n';
		expect(refused || first->home_open_notice()->text().isEmpty(), "an unrevalidated grant reports no failed restore");
		expect(first->showing_home() && first->home_open_bar()->isEnabled(), "refused restart leaves usable Home");
		expect(!github_application_fixture_access::large_review_snapshot(*owner, firstContext), "refused restart fabricates no content");
		expect(!first->home_recent_resume_action()->isEnabled(), "a refused restart offers no saved reading to resume");
	} else if (phase == "auth-stale" || phase == "auth-home") {
		home_lists_recent(*owner, firstContext, { 18, 17 });
		resume_recent(*owner, firstContext, 17);
		wait_for([&] { return github_application_fixture_access::held_window_read_terminals(*owner, firstContext) > 0U; }, "resumed read terminal is held before acceptance");
		// [[BPR-0795]]. The owner rejected an opening read that stayed on Home
		// for the whole long read: a read enters Overview's loading state with
		// its first answers. [[BPR-1200]]: until GitHub has answered for the
		// repository and the pull request, which this held lane withholds, the
		// read is said on Home, so a refused read never flashes an empty
		// Overview. The Home gesture stays enabled for the steps below.
		std::cerr << "pending-restoration home=" << first->showing_home()
				  << " modality=" << static_cast<int>(first->current_modality())
				  << " home-enabled=" << first->home_action()->isEnabled()
				  << " notice=" << first->home_open_notice()->text().toStdString() << '\n';
		expect(first->showing_home() && first->current_modality() == qt::review_modality::overview
				   && first->home_open_notice()->isVisible()
				   && first->home_open_notice()->text() == QStringLiteral("Resuming the saved reading…")
				   && first->home_action()->isEnabled(),
			   "pending restoration is said under the Home bar until it is answered, with Overview prepared and Home available");
		if (phase == "auth-home") {
			first->home_action()->trigger();
			std::cerr << "home-after-gesture=" << first->showing_home() << '\n';
			github_application_fixture_access::hold_window_read_terminal(*owner, firstContext, false);
			// The read lane posts released terminals as queued MetaCalls on this
			// thread. A later queued barrier observes their actual delivery.
			bool delivered = false;
			QMetaObject::invokeMethod(&application, [&] { delivered = true; }, Qt::QueuedConnection);
			wait_for([&] { return delivered; }, "released read terminal reaches decision lane after Home gesture");
			const auto snapshot = github_application_fixture_access::large_review_snapshot(*owner, firstContext);
			std::cerr << "home-after-terminal=" << first->showing_home() << " accepted-snapshot=" << bool(snapshot) << '\n';
			expect(first->showing_home() && !snapshot, "Home cancels pending restoration and refuses its late content");
		} else {
			submit(*first, 18);
			github_application_fixture_access::hold_window_read_terminal(*owner, firstContext, false);
			accepted(*owner, firstContext, 18);
			expect(first->home_open_notice()->text().isEmpty(), "late restoration completion does not replace user navigation or notice");
		}
	} else if (anonymous) {
		// A signed-out read is listed and opened by the same gesture as any
		// other, and the row says in words whether it has a reading to resume:
		// nothing here depends on colour.
		home_lists_recent(*owner, firstContext, { 17 });
		expect(first->home_recent_list()->item(0)->text().endsWith(QStringLiteral(" · saved reading")) == first->home_recent_resume_action()->isEnabled(),
			   "the listed row states exactly what its resume action offers");
		open_recent(*owner, firstContext, 17);
		accepted(*owner, firstContext, 17);
		// [[BPR-0987]]. The row of the pull request the window already shows
		// lands on Overview, as a row does, and reads nothing again.
		wait_for([&] { return first->session_panel()->status() == core::github_status::pull_request_ready && !first->showing_home(); }, "the opened row settles on its review");
		const auto shownReads   = github_application_fixture_access::evidence(*owner).AccountReadCount;
		const auto shownPreview = owner->status().PublicPreviewRequests;
		// Read first, so landing on Overview is the row's doing and not a view
		// that was already there.
		first->choose_modality(qt::review_modality::read);
		expect(first->current_modality() == qt::review_modality::read, "the shown review is read before Home");
		first->home_action()->trigger();
		wait_for([&] { return first->showing_home(); }, "Home shows over the open review");
		open_recent(*owner, firstContext, 17);
		wait_for([&] { return !first->showing_home() && first->current_modality() == qt::review_modality::overview; }, "a row of the shown pull request returns to it on Overview");
		expect(github_application_fixture_access::evidence(*owner).AccountReadCount == shownReads && owner->status().PublicPreviewRequests == shownPreview,
			   "a row of the shown pull request reads nothing again");
	} else {
		const auto second = core::bpr_window_context{ firstContext.Profile, core::bpr_window_id{ 2U }, firstContext.Generation };
		home_lists_recent(*owner, firstContext, { 18, 17 });
		if (phase != "auth-verify") {
			home_lists_recent(*owner, second, { 18, 17 });
		}
		if (phase == "auth-restore") {
			resume_recent(*owner, firstContext, 17);
		} else {
			open_recent(*owner, firstContext, 18);
		}
		accepted(*owner, firstContext, phase == "auth-restore" ? 17 : 18);
		if (phase == "auth-restore") {
			expect(expectedReadings->Records[0].Position.has_value(), "first window has durable reading");
			restored_reading(*first, *expectedReadings->Records[0].Position);
			capture(*first, "restored-auth-pr17");
		}
		if (phase != "auth-verify") {
			if (phase == "auth-restore") {
				resume_recent(*owner, second, 18);
			} else {
				open_recent(*owner, second, 18);
			}
			accepted(*owner, second, 18);
			if (phase == "auth-restore") {
				expect(expectedReadings->Records[1].Position.has_value(), "second window has durable reading");
				restored_reading(*owner->window(second), *expectedReadings->Records[1].Position);
				capture(*owner->window(second), "restored-auth-pr18");
				submit(*first, 18);
				accepted(*owner, firstContext, 18);
				select_reading(*owner, firstContext, 0);
				replacementReading = reading(*first);
				submit(*first, 99);
				wait_for([&] { return first->session_panel()->status() == core::github_status::pull_request_failed; }, "failed attempted navigation terminates");
			} else {
				first->close();
				wait_for([&] { return owner->windows().size() == 1U; }, "Close retires exact first window");
				expect(owner->window(firstContext) == nullptr, "closed context retired");
				first = owner->window(second);
			}
		} else {
			trigger(*first, "newBprWindowAction");
			wait_for([&] { return owner->windows().size() == 2U; }, "fresh window after restart");
			const auto fresh = core::bpr_window_context{ firstContext.Profile, core::bpr_window_id{ 3U }, core::bpr_window_generation{ 1U } };
			expect(owner->window(fresh) && owner->window(fresh)->showing_home() && owner->window(fresh)->home_open_notice()->text().isEmpty(), "monotonic new identity stays Home without destination");
			// [[BPR-0733]]. The list is the profile's, so a fresh window reads
			// the same entries without acquiring a destination of its own.
			auto* freshList = owner->window(fresh)->home_recent_list();
			wait_for([&] { return freshList->isVisible() && recent_numbers(*freshList) == std::vector<int>{ 18, 17 }; }, "fresh window lists the profile's recent reviews", [&] { report_recent(*owner, fresh, "fresh"); });
		}
	}
	if (anonymous) {
		expect_anonymous(*owner, !failure);
	}
	if (!seed) {
		const auto evidence = github_application_fixture_access::evidence(*owner);
		expect(evidence.VaultStores == 0U && evidence.DeviceTransportSubmissions == 0U, "restart never enrolls or exchanges tokens");
		if (failure && phase != "failed-read") {
			expect_no_account_attempts(evidence);
			if (phase != "missing-grant") {
				expect_no_vault_attempts(evidence);
			} else {
				expect(evidence.VaultRetrieveAttempts > 0U && !evidence.CredentialRetrieved,
					   "negative control: empty-vault failed lookup increments raw admission attempts");
			}
		}
		if (!anonymous && !failure) {
			expect(evidence.CredentialRetrieved && evidence.AccountTransportSelected, "remembered binding revalidated through retained vault/account");
		}
	}
	capture(*first, phase);
	trigger(*first, "quitProfilesAction");
	wait_for([&] { return desktop->is_quiescent(); }, "Quit flushes latest destinations and drains services");
	desktop.reset();
	if (replacementReading || readingRace) {
		const auto retained = restoration_store(root)->load({ });
		expect(retained && retained->Records.size() == 2U, "post-navigation reading records retained");
		if (replacementReading) {
			expect(retained->Records[0].Destination.Number.value() == 18U && retained->Records[0].Position == replacementReading, "PR replacement immediately persists its reading despite later failed navigation");
		} else if (gestureReading) {
			expect(retained->Records[0].Position == gestureReading, "Quit persists newer semantic gesture exactly");
		} else {
			expect(retained->Records[0] == expectedReadings->Records[0], "Home during reading restoration preserves original accepted position");
		}
	}
	if (unavailableReadings) {
		const auto retained = restoration_store(root)->load({ });
		expect(retained && retained->Records == unavailableReadings->Records, "failed reading restoration preserves exact recorded identities and sibling");
	}
	if (!seededReadings.empty()) {
		const auto retained = restoration_store(root)->load({ });
		expect(retained && retained->Records.size() == seededReadings.size(), "Quit drains both reading writes");
		for (std::size_t index = 0; index < seededReadings.size(); ++index) {
			expect(retained->Records[index].Position == seededReadings[index], "durable record equals actual accepted public reading");
			// [[BPR-1236]]. The active tab carries the same reading, even when
			// Home followed the selection at once.
			const auto& tabs = retained->Records[index].Tabs;
			expect(!tabs || (tabs->Active && tabs->Tabs[*tabs->Active] == seededReadings[index]), "durable active tab equals the accepted reading");
		}
	}
	// stdout is a dedicated binary pipe to parent memory, never a diagnostic log.
	const auto exported = app::detail::export_github_fixture_vault(options.SharedDeviceVault);
	expect(exported.size() <= 2048U, "vault output bounded");
	if (!anonymous && !failure) {
		expect(!exported.empty(), "orderly Quit retained synthetic grant");
	}
	std::cout.write(exported.data(), static_cast<std::streamsize>(exported.size()));
	std::cerr << "profile-window-review-restoration:" << phase << ":pass\n";
	return EXIT_SUCCESS;
}
