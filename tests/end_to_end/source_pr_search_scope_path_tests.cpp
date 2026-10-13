#include "../../src/app/src/github_application_fixture_access.hpp"
#include "../support/test_expectation.hpp"
#include "../support/window_system_keys.hpp"
#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/desktop_menu_bar.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/epoch_history_model.hpp"
#include "bpr/qt/files_tree.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/server_card.hpp"
#include "bpr/qt/workbench_drawer.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QThread>
#include <QToolButton>
#include <QTreeView>
#include <algorithm>
#include <iostream>

namespace {
	using namespace bpr;
	using tests::expect;
	using access = app::detail::github_application_fixture_access;
	void wait_for(const std::function<bool()>& aCondition, const char* aMessage)
	{
		auto timer = QElapsedTimer{ };
		timer.start();
		while (!aCondition() && timer.elapsed() < 20000) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
		if (!aCondition()) {
			std::cerr << "WAIT_FAILED " << aMessage << '\n'
					  << std::flush;
		}
		expect(aCondition(), aMessage);
	}
	void settle()
	{
		auto timer = QElapsedTimer{ };
		timer.start();
		while (timer.elapsed() < 180) {
			QApplication::processEvents();
			QThread::msleep(1);
		}
	}
	void render(qt::workbench_window& aWindow, const QString& aName)
	{
		const auto root = qEnvironmentVariable("BPR_FIND_RENDER_ROOT");
		if (root.isEmpty()) {
			return;
		}
		expect(QDir{ }.mkpath(root), "owned render root");
		expect(aWindow.grab().save(root + QLatin1Char('/') + aName + QStringLiteral(".png")), "full shipped composition render saved");
	}
	void query(qt::source_find_bar& aFind, const QString& aQuery)
	{
		aFind.query_editor()->setPlainText(aQuery);
		aFind.flush();
		wait_for([&] { return aFind.projection().Status != core::source_search_status::searching; }, "pane query terminal");
		settle();
	}

}
int main(int aArgc, char** aArgv)
{
	QApplication  applicationQt{ aArgc, aArgv };
	QTemporaryDir temporary{ QDir::tempPath() + QStringLiteral("/bpr-source-find-XXXXXX") };
	auto          options       = app::detail::github_public_path_fixture_options{ };
	options.ApplicationDataRoot = e2e::private_data_root(temporary);
	options.EpochPhase          = app::detail::github_epoch_fixture_phase::seed;
	const auto failureCourse    = QCoreApplication::arguments().contains(QStringLiteral("--read-failure"));
	QByteArray forwardingScript;
	if (failureCourse) {
		const auto git = QStandardPaths::findExecutable(QStringLiteral("git"));
		expect(!git.isEmpty(), "installed Git available for refusing capability course");
		const auto quote = [](QString aText) { return QStringLiteral("'") + aText.replace(QStringLiteral("'"), QStringLiteral("'\"'\"'")) + QStringLiteral("'"); };
		QFile      wrapper{ temporary.path() + QStringLiteral("/git") };
		expect(wrapper.open(QIODevice::WriteOnly), "owned Git gate created");
		const auto bootstrapScript = QStringLiteral("#!/bin/sh\nif [ \"$1\" = --exec-path ]; then printf '%s\\n' %1; exit 0; fi\n").arg(quote(QFileInfo{ temporary.path() }.canonicalFilePath())).toUtf8();
		const auto script          = bootstrapScript + QStringLiteral("for arg in \"$@\"; do\n if [ \"$arg\" = 93319e112da37a058791f8578d41040449945b90 ]; then exit 1; fi\ndone\nexec %1 \"$@\"\n").arg(quote(git)).toUtf8();
		forwardingScript           = bootstrapScript + QStringLiteral("exec %1 \"$@\"\n").arg(quote(git)).toUtf8();
		expect(wrapper.write(script) == script.size(), "exact local read gate written");
		wrapper.close();
		expect(wrapper.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner), "owner private executable gate");
		options.GitExecutable = std::filesystem::path{ wrapper.fileName().toStdString() };
	}
	expect(options.ApplicationDataRoot.has_value(), "private data root");
	std::unique_ptr<app::github_application> application;
	bool                                     ready   = false;
	auto                                     startup = access::start(app::detail::github_public_path_fixture::source_pr_search_v1, options, [&](auto aResult) {ready=true;if(aResult){application=std::move(*aResult);
} });
	wait_for([&] { return ready && startup->is_quiescent(); }, "shipped composition startup");
	expect(application != nullptr, "startup succeeds");
	application->show();
	auto& window = application->window();
	window.resize(1280, 860);

	auto* searchAction = window.findChild<QAction*>(QStringLiteral("sourceFilesAction"));
	auto* nextAction   = window.findChild<QAction*>(QStringLiteral("sourceFilesNextAction"));
	expect(searchAction && nextAction && !searchAction->isEnabled(), "Home cannot Search retained PR");
	window.github_connect_action()->trigger();
	wait_for([&] { return window.github_open_action()->isEnabled(); }, "connect fixture");
	window.home_open_bar()->setText(QStringLiteral("bpr-fixtures/source-pr-search#78"));
	window.home_open_action()->trigger();
	wait_for([&] { return window.showing_overview() && window.overview_page()->projection().has_value(); }, "PR overview");
	wait_for([&] { return searchAction->isEnabled(); }, "accepted PR enables Search command");
	searchAction->trigger();
	settle();
	std::cout << "SEARCH_ENTRY enabled=" << searchAction->isEnabled() << " visible=" << window.pr_search()->isVisible() << " files=" << window.files_drawer()->contents()->rowCount() << " facts=" << access::course_modality_facts(*application).toStdString() << '\n'
			  << std::flush;
	auto* panel   = window.pr_search();
	auto* surface = window.comparison_surface();
	wait_for([&] { return panel->isVisible() && window.files_drawer()->contents()->rowCount() > 0; }, "Review Search entry without source tab");
	if (failureCourse) {
		query(*panel->bar(), QStringLiteral("historical-only token"));
		wait_for([&] { return panel->projection().Status == core::source_search_status::partial && application->status().SourceSearchQuiescent; }, "unavailable read publishes qualified terminal");
		std::cout << "RETRY_FACTS matches=" << panel->projection().Summary.Matches << " skipped=" << panel->projection().Summary.SkippedSides << " retry=" << panel->projection().RetryAvailable << " cached=" << panel->projection().Statistics.CacheHits << "\n"
				  << std::flush;
		const auto failedScope      = panel->projection().Scope;
		const auto failedGeneration = panel->projection().Generation;
		auto*      retry            = panel->findChild<QToolButton*>(QStringLiteral("sourceSearchRetry"));
		expect(panel->projection().Summary.Matches == 0U && panel->projection().Summary.SkippedSides == 4U && !panel->projection().NoResults && retry->isVisible(), "missing material and unsupported bytes visibly qualify empty result with Retry");
		QFile restored{ QString::fromStdString(options.GitExecutable->string()) };
		expect(restored.open(QIODevice::WriteOnly) && restored.write(forwardingScript) == forwardingScript.size(), "restore exact immutable blob capability");
		restored.close();
		retry->click();
		wait_for([&] { return panel->projection().Status == core::source_search_status::partial && panel->projection().Summary.Matches == 1U && application->status().SourceSearchQuiescent; }, "public Retry reads the same exact missing source");
		expect(panel->projection().Scope == failedScope && panel->projection().Generation > failedGeneration && panel->projection().Summary.SkippedSides == 3U, "Retry changes only query generation under identical authority catalogue and anchors");
		application->begin_shutdown();
		wait_for([&] { return application->status().ShutdownComplete; }, "Retry course shutdown quiescent");
		std::cout << "SOURCE_PR_FIND_RETRY_PATH_OK\n";
		return 0;
	}
	query(*panel->bar(), QStringLiteral("historical-only token"));
	wait_for([&] { return panel->projection().Status == core::source_search_status::partial; }, "qualified terminal");
	expect(panel->projection().Summary.Matches == 1U && panel->projection().Summary.SkippedSides == 3U && !panel->projection().NoResults, "binary, undecodable and unsupported mode sides keep truthful coverage");
	expect(std::ranges::any_of(panel->findChildren<QLabel*>(), [](const auto* aLabel) { return aLabel->text().contains(QStringLiteral("Unsupported source mode")); }), "native Search Details explains unsupported exact-tree side mode");
	const auto scope      = panel->projection().Scope;
	const auto generation = panel->projection().Generation;
	expect(scope && !scope->Tab && !surface->current_result(), "opening Search did not create tab or source reads for empty query");
	auto index = panel->tree()->model()->index(0, 0, panel->tree()->model()->index(0, 0));
	panel->tree()->setCurrentIndex(index);
	QKeyEvent enter{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
	QApplication::sendEvent(panel->tree(), &enter);
	wait_for([&] { return surface->current_result() && surface->current_context()->Key.File == core::file_identity_of("historical/only.cpp") && surface->right_pane()->textCursor().selectedText() == QStringLiteral("historical-only token"); }, "first result creates ordinary tab and exact span");
	expect(panel->projection().Generation == generation && panel->projection().Scope == scope, "initial result open creates no second PR scan");
	auto* strip = window.document_tabs();
	expect(strip->tabs().size() == 1U, "ordinary active tab opened");
	const auto historical = strip->tabs()[0];
	strip->new_tab_button()->click();
	settle();
	expect(strip->tabs().size() == 2U, "sibling tab retained");
	query(*panel->bar(), QStringLiteral("catalogue token"));
	wait_for([&] { return panel->projection().Status == core::source_search_status::partial; }, "full ordered both-side query");
	expect(panel->projection().Summary.Matches == 7U, "unopened added removed modified and pure rename sources full both sides, identical rename merged once");

	for (const auto& [path, side] : { std::pair{ std::string_view{ "src/added.cpp" }, core::diff_side::right }, std::pair{ std::string_view{ "src/removed.cpp" }, core::diff_side::left } }) {
		QModelIndex entryGroup;
		for (int row = 0; row < panel->tree()->model()->rowCount(); ++row) {
			const auto candidate = panel->tree()->model()->index(row, 0);
			if (candidate.data().toString().contains(QString::fromUtf8(path.data(), static_cast<qsizetype>(path.size())))) {
				entryGroup = candidate;
				break;
			}
		}
		expect(entryGroup.isValid(), "added/removed exact entry has public result row");
		panel->tree()->setCurrentIndex(panel->tree()->model()->index(0, 0, entryGroup));
		QApplication::sendEvent(panel->tree(), &enter);
		wait_for([&] {
			const auto* reader = side == core::diff_side::left ? surface->left_pane() : surface->right_pane();
			return surface->current_result() && surface->current_context()->Key.File == core::file_identity_of(path) && surface->selection() && surface->selection()->PreferredLine.Side == side && reader->textCursor().selectedText() == QStringLiteral("catalogue token") && reader->hasFocus();
		},
				 "absent opposite side cannot steal exact added/removed source span and focus");
		expect(strip->tabs()[0].Id == historical.Id && strip->tabs()[0].Path == historical.Path, "added/removed activation preserves sibling reading state");
	}
	const auto rename = std::ranges::find(*panel->projection().Matches, core::file_identity_of("src/renamed.cpp"), &application::source_search_match::File);
	expect(rename != panel->projection().Matches->end() && rename->Match.Locations[0] && rename->Match.Locations[1], "same blob span retains both source locations");
	auto group = QModelIndex{ };
	for (int row = 0; row < panel->tree()->model()->rowCount(); ++row) {
		const auto candidate = panel->tree()->model()->index(row, 0);
		if (candidate.data().toString().contains(QStringLiteral("old/name.cpp"))) {
			group = candidate;
			break;
		}
	}
	expect(group.isValid(), "rename header includes exact old and new path");
	panel->tree()->setCurrentIndex(panel->tree()->model()->index(0, 0, group));
	QApplication::sendEvent(panel->tree(), &enter);
	wait_for([&] { return surface->current_result() && surface->current_context()->Key.File == core::file_identity_of("src/renamed.cpp") && surface->left_pane()->textCursor().selectedText() == QStringLiteral("catalogue token"); }, "pure rename opens exact left source through ordinary endpoint");
	expect(strip->tabs()[0].Id == historical.Id && strip->tabs()[0].Path == historical.Path, "result opening preserves sibling view");
	auto* include = window.findChild<QLineEdit*>(QStringLiteral("sourceFilesInclude"));
	auto* exclude = window.findChild<QLineEdit*>(QStringLiteral("sourceFilesExclude"));
	include->setText(QStringLiteral("./old/{name,other}.cpp"));
	panel->bar()->flush();
	wait_for([&] { return panel->projection().Status == core::source_search_status::complete; }, "either rename side path include");
	expect(panel->projection().Summary.Matches == 1U, "old rename path includes exact entry");
	exclude->setText(QStringLiteral("src/renamed.cpp"));
	panel->bar()->flush();
	wait_for([&] { return panel->projection().Status == core::source_search_status::complete; }, "exclude wins terminal");
	expect(panel->projection().NoResults && panel->projection().Summary.EligibleFiles == 0U, "excluded rename yields complete empty corpus");
	include->clear();
	exclude->setText(QStringLiteral("unsupported/**"));
	query(*panel->bar(), QStringLiteral("missing unique text"));
	wait_for([&] { return panel->projection().Status == core::source_search_status::complete; }, "complete empty matching");
	expect(panel->projection().NoResults, "only complete zero matches unqualified No results");
	exclude->clear();
	query(*panel->bar(), QStringLiteral("missing unique text"));
	wait_for([&] { return panel->projection().Status == core::source_search_status::partial; }, "partial empty matching");
	expect(!panel->projection().NoResults, "skipped zero matches stays qualified");

	exclude->setText(QStringLiteral("unsupported/**"));
	query(*panel->bar(), QStringLiteral("historical-only token"));
	wait_for([&] { return panel->projection().Status == core::source_search_status::complete; }, "exact historical query before remote epoch");
	const auto priorPair = panel->projection().Scope->Comparison;
	auto*      controls  = window.epoch_selectors();
	auto       baseRow   = -1;
	auto       headRow   = -1;
	for (int row = 0; row < controls->history_model()->rowCount(); ++row) {
		const auto anchor = controls->history_model()->anchor_at(controls->history_model()->index(row, 0));
		if (anchor && anchor->Identity.Kind == core::comparison_anchor_kind::epoch_base && anchor->Commit == priorPair.Left.Commit) {
			baseRow = row;
		}
		if (anchor && anchor->Identity.Kind == core::comparison_anchor_kind::epoch_head && anchor->Commit == priorPair.Right.Commit) {
			headRow = row;
		}
	}
	expect(baseRow >= 0 && headRow >= 0, "exact retained anchors selectable");
	controls->right_selector()->setCurrentIndex(baseRow);
	expect(!panel->projection().Matches, "comparison gesture clears obsolete hits before asynchronous restoration");
	settle();
	std::cout << "BASE_SCOPE status=" << static_cast<int>(panel->projection().Status) << " query=" << panel->projection().Query << " has=" << bool(panel->projection().Scope) << " pair=" << (panel->projection().Scope && panel->projection().Scope->Comparison.Right.Commit == priorPair.Left.Commit) << " epoch=" << static_cast<int>(application->status().EpochStatus) << " compare=" << controls->comparison_text().toStdString() << '\n'
			  << std::flush;
	wait_for([&] { return panel->projection().Scope && panel->projection().Scope->Comparison.Right.Commit == priorPair.Left.Commit && panel->projection().Status == core::source_search_status::complete; }, "explicit base selection settles");
	controls->right_selector()->setCurrentIndex(headRow);
	wait_for([&] { return panel->projection().Scope && panel->projection().Scope->Comparison.Right.Commit == priorPair.Right.Commit && panel->projection().Status == core::source_search_status::complete; }, "active tab explicitly pins retained epoch");
	const auto pinned      = panel->projection().Scope->Comparison;
	const auto priorEpochs = window.epoch_selectors()->history_model()->epoch_count();
	expect(window.github_refresh_action()->isEnabled(), "public Refresh available");
	window.github_refresh_action()->trigger();
	wait_for([&] { return window.epoch_selectors()->history_model()->epoch_count() > priorEpochs && application->status().EpochStatus == core::epoch_status::ready && !access::epoch_refresh_identity(*application).ComparisonPending && access::epoch_refresh_identity(*application).ActiveTab.has_value(); }, "epoch and active tab settle");
	settle();
	std::cout << "PINNED_TEST same=" << (access::epoch_refresh_identity(*application).ActiveTab->View.Comparison == pinned) << " status=" << static_cast<int>(panel->projection().Status) << '\n'
			  << std::flush;
	expect(access::epoch_refresh_identity(*application).ActiveTab->View.Comparison == pinned, "new remote head cannot silently move active retained comparison");
	wait_for([&] { return panel->projection().Status == core::source_search_status::complete && panel->projection().Summary.Matches == 1U; }, "historical-only source remains in visible exact search");
	expect(panel->projection().Scope->Comparison == pinned, "Search follows active retained pair after new epoch");
	window.epoch_selectors()->whole_pr_action()->trigger();
	wait_for([&] { return panel->projection().Scope && panel->projection().Scope->Comparison != pinned && panel->projection().Status == core::source_search_status::complete; }, "explicit latest pair retargets Search");
	expect(panel->projection().NoResults, "today's exact tree catalogue excludes historical-only path");
	query(*panel->bar(), QStringLiteral("today-only token"));
	expect(panel->projection().Summary.Matches == 1U, "new exact catalogue includes today's-only path");
	panel->bar()->query_editor()->setPlainText(QStringLiteral("catalogue"));
	panel->bar()->flush();
	panel->bar()->query_editor()->setPlainText(QStringLiteral("historical-only"));
	panel->bar()->flush();
	panel->bar()->query_editor()->setPlainText(QStringLiteral("today-only token"));
	panel->bar()->flush();
	wait_for([&] { return panel->projection().Status == core::source_search_status::complete && panel->projection().Query == "today-only token"; }, "rapid reversed query demand accepts only latest");
	expect(panel->projection().Summary.Matches == 1U, "obsolete query matches cannot return");
	const auto targetTab = strip->tabs().back().Id;
	panel->tree()->setCurrentIndex(panel->tree()->model()->index(0, 0, panel->tree()->model()->index(0, 0)));
	QApplication::sendEvent(panel->tree(), &enter);
	const auto openedTarget = access::epoch_refresh_identity(*application).ActiveTab->View.File;
	strip->tab_bar()->setCurrentIndex(0);
	strip->tab_bar()->setCurrentIndex(1);
	wait_for([&] {const auto active=access::epoch_refresh_identity(*application).ActiveTab;return active && active->Id==targetTab && !access::epoch_refresh_identity(*application).TabPending && panel->projection().Status==core::source_search_status::complete && panel->projection().Summary.Matches==1U; }, "rapid tab reversals settle only active exact corpus");
	expect(access::epoch_refresh_identity(*application).ActiveTab->View.File == openedTarget && surface->right_pane()->textCursor().selectedText() != QStringLiteral("today-only token"), "later tab gestures defeat pending exact span even after returning to same tab");
	render(window, QStringLiteral("pr-search-complete-course"));
	panel->bar()->query_editor()->setPlainText(QStringLiteral("catalogue token"));
	panel->bar()->flush();
	auto* signOut = static_cast<QPushButton*>(nullptr);
	for (auto* card : window.server_cards()) {
		if (card->signed_in()) {
			signOut = card->sign_out_button();
			break;
		}
	}
	expect(signOut && signOut->isEnabled(), "fixture's normal account sign-out available");
	signOut->click();
	wait_for([&] { return panel->bar()->query_editor()->toPlainText().isEmpty() && !panel->projection().Matches && !panel->projection().Scope && application->status().SourceSearchQuiescent; }, "authority retirement clears sensitive runtime query results and admitted work");
	settle();
	expect(!searchAction->isEnabled() && panel->bar()->query_editor()->toPlainText().isEmpty() && !panel->projection().Matches, "late old-authority completion cannot restore search");

	application->begin_shutdown();
	wait_for([&] { return application->status().ShutdownComplete; }, "Search shutdown quiescent");
	std::cout << "SOURCE_PR_FIND_SCOPE_PATH_OK\n";
}
