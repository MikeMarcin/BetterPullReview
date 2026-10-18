// BPR-0969 inventories current behavior; success does not certify offline mode.
#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/github_session_panel.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/workbench_landing.hpp"

#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSysInfo>
#include <QThread>
#include <QTreeView>

#include <array>
#include <cstdlib>
#include <functional>
#include <iostream>

namespace {
	using namespace bpr;
	using tests::expect;
	using access                                       = app::detail::github_application_fixture_access;
	app::github_application* diagnosticOwner           = nullptr;
	bool                     staleRecentWhileSignedOut = false;

	void wait_for(const std::function<bool()>& aCondition, const char* aMessage)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (!aCondition() && elapsed.elapsed() < 30000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		if (!aCondition() && diagnosticOwner) {
			const auto evidence = access::evidence(*diagnosticOwner);
			std::cerr << "pending=" << aMessage << " status=" << static_cast<int>(diagnosticOwner->window().session_panel()->status())
					  << " diagnostic=" << diagnosticOwner->window().session_panel()->diagnostic_text() << " recording=" << evidence.RecordingFailure
					  << " operation=" << evidence.LastReadOperation << '\n';
		}
		expect(aCondition(), aMessage);
	}

	constexpr auto freshnessNotice = QStringView{ u"Refresh failed. Showing previously loaded review data; freshness is unknown." };

	auto visible_freshness_notice(qt::workbench_window& aWindow) -> bool
	{
		for (auto* label : aWindow.findChildren<QLabel*>()) {
			if (label->isVisible() && label->text().startsWith(freshnessNotice)) {
				return true;
			}
		}
		return false;
	}

	void report(app::github_application& aOwner, qt::workbench_window& aWindow, const char* aStage)
	{
		QApplication::processEvents();
		const auto snapshot = access::large_review_snapshot(aOwner);
		std::cerr << "stage=" << aStage << " snapshot=" << bool(snapshot)
				  << " home=" << aWindow.showing_home()
				  << " source=" << bool(aWindow.comparison_surface()->current_result())
				  << " comparison=" << aWindow.epoch_selectors()->comparison_available()
				  << " threads_model_rows=" << aWindow.conversation_model()->rowCount()
				  << " availability=" << aWindow.conversation_panel()->availability_label()->text().toStdString()
				  << " diagnostic=" << aWindow.session_panel()->diagnostic_text() << '\n';
		if (snapshot) {
			std::cerr << "authority=" << snapshot->Profile.value() << ':' << snapshot->Host.value() << ':'
					  << snapshot->Account.value() << ':' << snapshot->Repository.Repository.value() << ':'
					  << snapshot->PullRequest.PullRequest.value() << " snapshot_id=" << snapshot->Snapshot.value()
					  << " base=" << snapshot->PullRequest.BaseOid.value() << " head=" << snapshot->PullRequest.HeadOid.value()
					  << " files=" << snapshot->Files.size() << " threads=" << snapshot->ReviewThreads.size()
					  << " threads_availability=" << static_cast<int>(snapshot->ReviewThreadsAvailability) << '\n';
		}
		for (auto* label : aWindow.findChildren<QLabel*>()) {
			const auto text = label->text();
			if (label->isVisible() && (text.contains(QStringLiteral("cache"), Qt::CaseInsensitive) || text.contains(QStringLiteral("stale"), Qt::CaseInsensitive) || text.contains(QStringLiteral("network"), Qt::CaseInsensitive) || text.contains(QStringLiteral("unavailable"), Qt::CaseInsensitive))) {
				std::cerr << "visible_label=" << text.toStdString() << '\n';
			}
		}
		const auto image = aWindow.grab();
		expect(!image.isNull(), "full window capture exists");
		const auto render = qEnvironmentVariable("BPR_OFFLINE_RENDER_DIR");
		if (!render.isEmpty()) {
			expect(image.save(render + QLatin1Char{ '/' } + QString::fromUtf8(aStage) + QStringLiteral(".png")), "offline inventory raster saved");
		}
	}

	void open(qt::workbench_window& aWindow, const QString& aLocator)
	{
		if (!aWindow.showing_home()) {
			aWindow.home_action()->trigger();
		}
		aWindow.home_open_bar()->setText(aLocator);
		aWindow.home_open_action()->trigger();
	}

	void select_source(app::github_application& aOwner, qt::workbench_window& aWindow)
	{
		wait_for([&] { return aOwner.status().EpochStatus == core::epoch_status::ready; }, "retained epoch ready");
		app::detail::course_modality_step modality{ .Modality = qt::review_modality::review,
													.Needs    = { .Comparison = true, .FilesDrawer = true } };
		wait_for([&] { return access::enter_course_modality(aOwner, modality) != app::detail::course_modality_result::pending; }, "Review modality settles");
		expect(modality.Result == app::detail::course_modality_result::entered, "Review exposes real source and file drawer");
		auto*       files = aWindow.files_drawer()->contents_view();
		QModelIndex selected;
		wait_for([&] {
			for (int row = 0; row < files->model()->rowCount(); ++row) {
				const auto index = files->model()->index(row, 0);
				if (index.data().toString() == QStringLiteral("review.cpp")) {
					selected = index;
					return true;
				}
			}
			return false;
		},
				 "exact fixture file listed");
		files->selectionModel()->setCurrentIndex(selected, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
		auto* surface = aWindow.comparison_surface();
		wait_for([&] { return surface->isVisible() && surface->current_result() && surface->current_context() && !surface->geometry_pending(); }, "installed Git source rendered");
		const auto snapshot = access::large_review_snapshot(aOwner);
		expect(snapshot && snapshot->Files.size() == 2U && !snapshot->ReviewThreads.empty(), "exact two-file review has observed threads");
		expect(aWindow.epoch_selectors()->left_oid_editor()->text().toStdString() == snapshot->PullRequest.BaseOid.value()
				   && aWindow.epoch_selectors()->right_oid_editor()->text().toStdString() == snapshot->PullRequest.HeadOid.value(),
			   "selected source exact accepted base and head");
		expect(!surface->left_pane()->toPlainText().isEmpty() && !surface->right_pane()->toPlainText().isEmpty(), "both exact source documents populated");
		expect(surface->select_conversation_line(core::diff_side::right, 3U), "save exact reading position");
	}

	void switch_account(qt::workbench_window& aWindow, const std::shared_ptr<std::atomic<std::uint64_t>>& aAccount, std::uint64_t aNext)
	{
		auto* card = aWindow.server_card_for(*aWindow.session_panel()->active_profile());
		expect(card && card->sign_out_button()->isEnabled(), "real sign-out available");
		card->sign_out_button()->click();
		wait_for([&] { return !access::evidence(*diagnosticOwner).SecretRetained && !access::evidence(*diagnosticOwner).RefreshRetained && !card->signed_in() && aWindow.session_panel()->status() != core::github_status::pull_request_ready && aWindow.session_panel()->status() != core::github_status::session_ready; }, "sign-out retires accepted identity");
		staleRecentWhileSignedOut = staleRecentWhileSignedOut || aWindow.home_recent_list()->count() != 0;
		std::cerr << "settled_signedout_recent_rows=" << aWindow.home_recent_list()->count() << '\n';
		aAccount->store(aNext);
		aWindow.home_landing()->primary_button()->click();
		wait_for([&] { return aWindow.session_panel()->status() == core::github_status::session_ready; }, "replacement account accepted");
		expect(aWindow.account_action()->text().contains(QString::number(aNext)), "account strip binds generated viewer");
	}

	void expect_recent(qt::workbench_window& aWindow, std::uint64_t aAccount, int aNumber)
	{
		expect(aWindow.home_recent_list()->count() == 1, "only the current account has a Recent row");
		const auto text = aWindow.home_recent_list()->item(0)->text();
		expect(text.contains(QStringLiteral("Private title for account %1").arg(aAccount))
				   && text.contains(QStringLiteral("review-repository#%1").arg(aNumber)),
			   "Recent title and destination belong to accepted account");
	}

	auto cached_visible(qt::workbench_window& aWindow) -> bool
	{
		auto* summary = aWindow.findChild<QLabel*>(QStringLiteral("overviewSummary"));
		return summary && summary->isVisible() && summary->text().contains(QStringLiteral("Cached overview"));
	}
	void expect_cached(qt::workbench_window& aWindow, std::uint64_t aAccount = 0, bool aCatalogueAvailable = true, bool aThreadsAvailable = true, bool aSourceAvailable = true, bool aMismatchedPair = false)
	{
		if (auto* back = aWindow.findChild<QPushButton*>(QStringLiteral("overviewCachedSourceBack")); back && back->isVisible()) {
			back->click();
		}
		expect(cached_visible(aWindow) && !aWindow.showing_home(), "durable cached overview is actually visible after offline reopen");
		auto* summary = aWindow.findChild<QLabel*>(QStringLiteral("overviewSummary"));
		expect(summary->text().contains(QStringLiteral("freshness unknown")), "cached metadata never implies fresh server truth");
		expect(summary->toolTip().contains(aMismatchedPair ? QString(40, QLatin1Char{ '1' }) : QStringLiteral("a61a212aa0484929bc5f20a26e9a7c1c6541c726"))
				   && summary->toolTip().contains(aMismatchedPair ? QString(40, QLatin1Char{ '2' }) : QStringLiteral("3b60c8324eee117ce72e6446fe640d85a60b37a2")),
			   "cached overview retains the exact accepted base and head");
		auto* title = aWindow.findChild<QLabel*>(QStringLiteral("overviewTitle"));
		expect(title && !title->text().isEmpty(), "cached title is populated");
		if (aAccount) {
			expect(title->text() == QStringLiteral("Private title for account %1").arg(aAccount), "cached title belongs to the exact current account");
		}
		auto* files = aWindow.findChild<QTreeView*>(QStringLiteral("overviewFiles"));
		auto* model = files ? dynamic_cast<qt::overview_file_model*>(files->model()) : nullptr;
		if (!aCatalogueAvailable) {
			const auto* heading = aWindow.findChild<QLabel*>(QStringLiteral("overviewFilesHeading"));
			expect(model && model->file_count() == 0 && heading && heading->text().contains(QStringLiteral("catalogue unavailable")), "legacy overview remains catalogue unavailable, never observed empty");
			const auto* threadsHeading = aWindow.findChild<QLabel*>(QStringLiteral("overviewCachedThreadsHeading"));
			expect(threadsHeading && threadsHeading->text().contains(QStringLiteral("unavailable")), "legacy threads remain unavailable, not empty");
			return;
		}
		auto* threads = aWindow.findChild<QPlainTextEdit*>(QStringLiteral("overviewCachedThreads"));
		if (aThreadsAvailable) {
			const auto* discussion = aWindow.findChild<QPlainTextEdit*>(QStringLiteral("overviewCachedDiscussion"));
			expect(discussion && discussion->isVisible() && discussion->isReadOnly()
					   && discussion->toPlainText().contains(QStringLiteral("General comments"))
					   && discussion->toPlainText().contains(QStringLiteral("Review summaries"))
					   && discussion->toPlainText().contains(QStringLiteral("Fixture follow-up: validation is complete."))
					   && discussion->toPlainText().contains(QStringLiteral("Please preserve snapshot identity across refreshes.")),
				   "durable general comments and review summaries survive actual restart as historical text");
			expect(threads && threads->isVisible() && threads->isReadOnly()
					   && threads->toPlainText().contains(QStringLiteral("Check the exact retained reading position.")),
				   "durable cached thread bodies survive process restart as historical read-only content");
		} else {
			const auto* heading = aWindow.findChild<QLabel*>(QStringLiteral("overviewCachedThreadsHeading"));
			expect(threads && threads->isHidden() && heading && heading->text().contains(QStringLiteral("unavailable")), "previous catalogue payload preserves thread unavailability");
		}
		expect(model && model->file_count() == 2U, "durable cached file catalogue survives process restart");
		for (const auto* path : { "review.cpp", "secondary.cpp" }) {
			const auto  index = model->index_for_path(QString::fromUtf8(path));
			const auto* row   = model->row_at(index);
			expect(row && row->Path == path && !row->PreviousPath && row->Additions == 6 && row->Deletions == 6
					   && row->Operation == application::overview_file_operation::modified,
				   "cached rows match the exact accepted provider catalogue");
		}
		for (int column = qt::overview_file_model::changed_column; column < qt::overview_file_model::column_count; ++column) {
			expect(files->isColumnHidden(column), "unavailable review progress and conversation counts never render as zero");
		}
		const auto index = model->index_for_path(QStringLiteral("review.cpp"));
		QMetaObject::invokeMethod(files, "activated", Qt::DirectConnection, Q_ARG(QModelIndex, index));
		QApplication::processEvents();
		auto* historicalSource = aWindow.findChild<QPlainTextEdit*>(QStringLiteral("overviewCachedSource"));
		wait_for([&] { return historicalSource && historicalSource->isVisible() && historicalSource->toPlainText().contains(aSourceAvailable ? QStringLiteral("HEAD — observed source") : QStringLiteral("Source unavailable")); }, "exact retained source is readable after restart without live comparison authority");
		expect(historicalSource->isReadOnly() && (aSourceAvailable ? historicalSource->toPlainText().contains(QStringLiteral("BASE — observed source")) : !historicalSource->toPlainText().contains(QStringLiteral("observed source"))) && historicalSource->toPlainText().contains(QStringLiteral("freshness unknown")), "historical source shows both bounded exact endpoints and unknown freshness");
		if (aSourceAvailable) {
			QString expectedBase, expectedHead;
			for (int line = 1; line <= 6; ++line) {
				expectedBase += QStringLiteral("int review_line_%1 = %2;\n").arg(line).arg(10 + line);
				expectedHead += QStringLiteral("int review_line_%1 = %2;\n").arg(line).arg(20 + line);
			}
			expect(historicalSource->toPlainText().contains(expectedBase) && historicalSource->toPlainText().contains(expectedHead), "rendered source preserves all six exact fixture lines on both endpoints");
		}
		expect(cached_visible(aWindow) && !aWindow.comparison_surface()->current_result(), "historical source grants no live comparison authority");
		std::cerr << "cached_overview=visible exact_revisions=1 freshness=unknown catalogue=2 source=historical-or-explicit-unavailable threads="
				  << (aThreadsAvailable ? "historical" : "unavailable") << '\n';
	}
	void expect_unavailable(app::github_application& aOwner, qt::workbench_window& aWindow)
	{
		expect(!access::large_review_snapshot(aOwner), "failed open has no accepted observation, not an empty review");
		expect(!visible_freshness_notice(aWindow), "unavailable review never claims to display retained data");
		expect(aWindow.conversation_model()->rowCount() == 0
				   && aWindow.conversation_panel()->availability_label()->text().contains(QStringLiteral("unavailable")),
			   "zero thread rows explicitly mean unavailable");
		expect(!aWindow.comparison_surface()->current_result() && !aWindow.epoch_selectors()->comparison_available(), "retained disk objects alone do not reopen a comparison");
	}
}

auto main(int aArgc, char** aArgv) -> int
try {
	expect(aArgc == 3, "phase and absolute private data root required");
	const auto argument       = std::string_view{ aArgv[1] };
	const bool identityCourse = argument.starts_with("identity-");
	const auto phase          = identityCourse ? argument.substr(9) : argument;
	expect(phase == "seed" || phase == "restart-read" || phase == "restart-offline" || phase == "restart-corrupt" || phase == "restart-legacy" || phase == "restart-catalogue" || phase == "restart-missing-source" || phase == "restart-mismatched-source" || (identityCourse && phase == "legacy"), "known offline phase");
	std::array<char, 2049U> bytes{ };
	std::cin.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
	expect(std::cin.gcount() <= 2048, "synthetic vault pipe bounded");
	const auto   vaultBytes = std::string{ bytes.data(), static_cast<std::size_t>(std::cin.gcount()) };
	QApplication application{ aArgc, aArgv };
	QApplication::setQuitOnLastWindowClosed(false);
	std::cerr << "phase=" << phase << " platform=" << QSysInfo::productType().toStdString() << " arch=" << QSysInfo::currentCpuArchitecture().toStdString() << " qt=" << qVersion() << " backend=" << QGuiApplication::platformName().toStdString() << '\n';
	auto offline     = std::make_shared<app::detail::github_offline_fixture>();
	offline->Offline = phase == "restart-offline";
	app::detail::github_public_path_fixture_options options;
	options.ApplicationDataRoot = std::filesystem::path{ aArgv[2] };
	expect(options.ApplicationDataRoot->is_absolute(), "absolute private root");
	options.RegisteredDeviceCourse = true;
	options.PersistDeviceBindings  = true;
	options.RepeatReadSessions     = true;
	options.IndependentWindowReads = true;
	options.Offline                = offline;
	options.RecentAccount          = identityCourse ? std::make_shared<std::atomic<std::uint64_t>>(910001U) : nullptr;
	options.SharedDeviceVault      = vaultBytes.empty() ? app::detail::make_shared_github_fixture_vault() : app::detail::import_github_fixture_vault(vaultBytes);
	expect(options.SharedDeviceVault != nullptr, "synthetic vault restored");
	app::github_application* owner    = nullptr;
	bool                     terminal = false;
	auto                     desktop  = access::start_desktop(app::detail::github_public_path_fixture::profile_window_restoration_v1, options,
															  [&](auto aResult) { terminal = true; if (aResult) { owner = *aResult; } });
	wait_for([&] { return terminal; }, "desktop startup terminal");
	expect(owner != nullptr, "desktop admitted");
	diagnosticOwner = owner;
	owner->show();
	wait_for([&] { return owner->windows().size() == 1U; }, "one restored window");
	auto& window = *owner->window(owner->windows().front());
	window.resize(1500, 950);
	const auto locator = QStringLiteral("https://github.com/octo-org/review-repository/pull/17");
	if (phase == "restart-offline") {
		wait_for([&] { return offline->WireRefusals.load() > 0U && window.session_panel()->status() == core::github_status::session_failed; }, "offline restart cannot revalidate account");
		expect_unavailable(*owner, window);
		expect(window.showing_home() && window.home_recent_list()->count() == 0, "unvalidated account cannot expose authenticated Recent entries");
		report(*owner, window, "restart-offline");
	} else {
		if (phase == "seed" || phase == "legacy") {
			window.home_landing()->primary_button()->click();
		}
		wait_for([&] { return window.session_panel()->status() == core::github_status::session_ready; }, "exact fixture account accepted");
		if (phase == "legacy") {
			expect(window.home_recent_list()->count() == 0, "legacy unknown accounts cannot inherit the authenticated viewer");
			expect_unavailable(*owner, window);
			report(*owner, window, "identity-legacy");
		}
		if (phase == "restart-read" || phase == "restart-corrupt" || phase == "restart-legacy" || phase == "restart-catalogue" || phase == "restart-missing-source" || phase == "restart-mismatched-source") {
			wait_for([&] { return window.home_recent_list()->count() == 1; }, "Recent metadata survives process restart");
			expect(!access::large_review_snapshot(*owner), "Recent title is not a cached PR snapshot");
			if (identityCourse) {
				expect_recent(window, 910001U, 17);
			}
			expect(window.home_recent_list()->item(0)->text().contains(QStringLiteral("octo-org/review-repository#17")), "Recent names exact target");
			offline->Offline = true;
			window.home_recent_list()->setCurrentRow(0);
			wait_for([&] { return window.home_recent_resume_action()->isEnabled(); }, "saved reading offers Resume");
			window.home_recent_resume_action()->trigger();
			wait_for([&] { return window.session_panel()->status() == core::github_status::pull_request_failed; }, "offline Resume reaches wire failure");
			expect(offline->WireRefusals.load() > 0U, "Resume actually attempts provider read");
			expect_unavailable(*owner, window);
			if (phase == "restart-corrupt") {
				expect(!cached_visible(window), "corrupt overview refuses visible metadata through real composition");
				report(*owner, window, "restart-corrupt-offline");
			} else {
				expect_cached(window, identityCourse ? 910001U : 0U, phase != "restart-legacy", phase != "restart-catalogue", phase != "restart-missing-source" && phase != "restart-mismatched-source", phase == "restart-mismatched-source");
				report(*owner, window, phase == "restart-missing-source" ? "restart-missing-source" : phase == "restart-mismatched-source" ? "restart-mismatched-source"
						   : phase == "restart-legacy"                                                                                     ? "restart-legacy-offline"
						   : phase == "restart-catalogue"                                                                                  ? "restart-catalogue-offline"
																																		   : "restart-read-offline");
				window.home_action()->trigger();
				expect(window.showing_home(), "cached overview can return to Home");
				window.home_landing()->primary_button()->click();
				expect_cached(window, identityCourse ? 910001U : 0U, phase != "restart-legacy", phase != "restart-catalogue", phase != "restart-missing-source" && phase != "restart-mismatched-source", phase == "restart-mismatched-source");
				if (!identityCourse) {
					open(window, QStringLiteral("https://github.com/octo-org/another-repository/pull/17"));
					wait_for([&] { return window.session_panel()->status() == core::github_status::pull_request_failed; }, "cached view replaced by foreign offline failure");
					expect_unavailable(*owner, window);
					expect(!cached_visible(window) && window.showing_home(), "new failed destination clears previous cached metadata and its marker");
				}
			}
			offline->Offline = false;
		}
		if (phase != "restart-missing-source") {
			open(window, locator);
			wait_for([&] { return window.session_panel()->status() == core::github_status::pull_request_ready; }, "online original review accepted");
			select_source(*owner, window);
			const auto snapshot = access::large_review_snapshot(*owner);
			expect(!visible_freshness_notice(window) && !cached_visible(window), "accepted online data clears cached and failed-refresh notices");
			const auto* historical = window.findChild<QPlainTextEdit*>(QStringLiteral("overviewCachedThreads"));
			expect(historical && historical->isHidden() && historical->toPlainText().isEmpty(), "reconnect removes historical thread content");
			if (!identityCourse && phase == "restart-read") {
				expect(snapshot && snapshot->ReviewThreads.front().Comments.front().Body == "Check the exact retained reading position.",
					   "reconnect replaces the prior process edited root with the newly observed server body");
			}
			const auto key   = window.comparison_surface()->current_context()->Key;
			const auto left  = window.comparison_surface()->left_pane()->toPlainText();
			const auto right = window.comparison_surface()->right_pane()->toPlainText();
			report(*owner, window, phase == "seed" ? "online-seed" : "restart-reconnected");
			if (identityCourse && phase == "seed") {
				window.home_action()->trigger();
				expect_recent(window, 910001U, 17);
				const auto context = owner->windows().front();
				access::hold_window_read_terminal(*owner, context, true);
				window.github_refresh_action()->trigger();
				wait_for([&] { return access::held_window_read_terminals(*owner, context) > 0U; }, "first-account refresh terminal is held");
				switch_account(window, options.RecentAccount, 910002U);
				access::hold_window_read_terminal(*owner, context, false);
				QApplication::processEvents();
				report(*owner, window, "identity-other-account");
				expect(window.home_recent_list()->count() == 0, "another account cannot see the first account's title or destination");
				offline->Offline = true;
				open(window, locator);
				wait_for([&] { return window.session_panel()->status() == core::github_status::pull_request_failed; }, "foreign account offline open completes");
				expect(!cached_visible(window), "other account cannot borrow the cached overview by known locator");
				offline->Offline = false;
				expect_unavailable(*owner, window);
				open(window, QStringLiteral("https://github.com/octo-org/review-repository/pull/18"));
				wait_for([&] { return window.session_panel()->status() == core::github_status::pull_request_ready; }, "second account review accepted");
				select_source(*owner, window);
				expect(access::large_review_snapshot(*owner)->Account == core::github_account_id{ 910002U }, "second snapshot is exact account");
				window.home_action()->trigger();
				expect_recent(window, 910002U, 18);
				switch_account(window, options.RecentAccount, 910001U);
				expect_recent(window, 910001U, 17);
				window.home_recent_list()->setCurrentRow(0);
				window.home_recent_resume_action()->trigger();
				wait_for([&] { return window.comparison_surface()->current_context() && window.comparison_surface()->current_context()->Key == key; }, "returning account resumes its exact retained comparison");
			} else if (identityCourse && phase == "restart-read") {
				window.home_action()->trigger();
				switch_account(window, options.RecentAccount, 910002U);
				expect_recent(window, 910002U, 18);
				offline->Offline = true;
				window.home_recent_list()->setCurrentRow(0);
				wait_for([&] { return window.home_recent_resume_action()->isEnabled(); }, "second account retained reading offers Resume");
				window.home_recent_resume_action()->trigger();
				wait_for([&] { return window.session_panel()->status() == core::github_status::pull_request_failed; }, "second account offline Resume fails visibly");
				expect_unavailable(*owner, window);
				expect_cached(window, 910002U);
				report(*owner, window, "identity-second-account-offline");
				offline->Offline = false;
				switch_account(window, options.RecentAccount, 910001U);
				expect_recent(window, 910001U, 17);
			} else if (phase == "seed") {
				offline->Offline = true;
				window.github_refresh_action()->trigger();
				wait_for([&] { return window.session_panel()->status() == core::github_status::pull_request_failed; }, "network loss refresh terminal");
				expect(offline->WireRefusals.load() > 0U && access::large_review_snapshot(*owner) == snapshot, "failed refresh preserves exact in-memory accepted snapshot");
				report(*owner, window, "failed-refresh");
				expect(visible_freshness_notice(window), "failed refresh visibly labels retained data and unknown freshness");
				expect(window.comparison_surface()->current_context() && window.comparison_surface()->current_context()->Key == key
						   && window.comparison_surface()->left_pane()->toPlainText() == left && window.comparison_surface()->right_pane()->toPlainText() == right,
					   "failed refresh retains exact source identity and documents");
				std::cerr << "retained_source_key=" << (window.comparison_surface()->current_context() && window.comparison_surface()->current_context()->Key == key)
						  << " retained_source_text=" << (window.comparison_surface()->left_pane()->toPlainText() == left && window.comparison_surface()->right_pane()->toPlainText() == right) << '\n';
				offline->Offline = false;
				access::stage_server_conversation(*owner, std::string{ snapshot->ReviewThreads.front().Thread.value() }, true);
				window.github_refresh_action()->trigger();
				wait_for([&] { return window.session_panel()->status() == core::github_status::pull_request_ready; }, "reconnection refresh accepts server truth");
				const auto recovered = access::large_review_snapshot(*owner);
				expect(recovered && recovered->Snapshot != snapshot->Snapshot && recovered->Profile == snapshot->Profile
						   && recovered->Host == snapshot->Host && recovered->Account == snapshot->Account && recovered->Repository == snapshot->Repository
						   && recovered->PullRequest == snapshot->PullRequest && recovered->ReviewThreads.front().Comments.front().Body == "Edited server root",
					   "fresh exact-authority snapshot adopts changed server conversation");
				expect(!visible_freshness_notice(window), "successful refresh removes failed-refresh notice");
				expect(window.comparison_surface()->current_context() && window.comparison_surface()->current_context()->Key == key, "metadata refresh preserves exact retained comparison");
				report(*owner, window, "refresh-reconnected");
				offline->Offline = true;
				open(window, QStringLiteral("https://github.com/octo-org/another-repository/pull/17"));
				wait_for([&] { return window.session_panel()->status() == core::github_status::pull_request_failed; }, "other repository offline terminal");
				expect_unavailable(*owner, window);
				expect(!cached_visible(window), "foreign repository cannot borrow a cached overview");
				report(*owner, window, "other-repository-offline");
				offline->Offline = false;
				open(window, locator);
				wait_for([&] { return window.session_panel()->status() == core::github_status::pull_request_ready; }, "original repository recovery");
				select_source(*owner, window);
				expect(window.comparison_surface()->current_context()->Key == key, "exact retained revision identity restored after failure");
			}
		}
	}
	expect(!staleRecentWhileSignedOut, "settled sign-out hides authenticated Recent");
	expect(access::review_mutation_evidence(*owner).Submissions == 0U, "inventory performs no mutation");
	std::cerr << "wire_refusals=" << offline->WireRefusals.load() << " git_refusals=" << offline->GitRefusals.load() << '\n';
	auto* quit = window.findChild<QAction*>(QStringLiteral("quitProfilesAction"));
	expect(quit != nullptr && quit->isEnabled(), "public Quit available");
	quit->trigger();
	wait_for([&] { return desktop->is_quiescent(); }, "Quit drains persisted reading and all services");
	diagnosticOwner = nullptr;
	desktop.reset();
	const auto exported = app::detail::export_github_fixture_vault(options.SharedDeviceVault);
	expect(exported.size() <= 2048U, "synthetic vault output bounded");
	std::cout.write(exported.data(), static_cast<std::streamsize>(exported.size()));
	std::cerr << "offline-inventory:" << argument << ":pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
