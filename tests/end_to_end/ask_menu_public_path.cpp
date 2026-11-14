/// \file
/// \brief RFC-BPR-0128 ([[BPR-2163]], [[BPR-2164]]). The Ask menus through
/// the shipped window, on returning-review-v1 served at its next push, with
/// the scoped writing stub as the installed Claude.
///
/// Courses, by the first argument:
///
/// - `menu STUB`: the review bar, the open file's header and the open
///   thread's reply row each have one Ask; a thread's and a file tab's
///   right-click menus have one Ask item and none of the agent items they
///   replace (AC01); a menu lists only quick asks, each naming what a press
///   reads, a scope's subject absent is not listed and one that cannot run is
///   greyed with its reason as tooltip, status tip and accessible text, and
///   building every menu launches nothing (AC02, AC03); Custom question…
///   attaches the thread and sends nothing, Edit Quick Asks… opens Settings
///   at the suggested prompts (AC02); in Settings, Quick ask off on a
///   built-in and on with File and Pull request on an own prompt are saved
///   with the prompt in the profile's store and change the menus, and a
///   scope narrower than a prompt's subject cannot be set (AC04); What
///   changed pressed from the review bar sends the action's own prompt as
///   before (AC04); greyed rows are reachable by keyboard and every control
///   is named (AC08); nothing reaches GitHub.
/// - `addressed STUB`: Was this addressed? on a thread I did not comment in,
///   on each thread of a file (one run a thread) and listed for every thread
///   of the pull request; each claim is a chip in its thread's header that
///   changes no card's height, its press shows the thread's lines when it
///   started and now; a thread commits have not followed is greyed with its
///   reason; nothing is resolved, posted or published (AC05).
/// - `report STUB`: What changed from the review bar's Ask answers as a report
///   card in the Agent tab, its material listing the commits (AC07's listing
///   in the request); Open report opens it as a tab in the files' strip with
///   its outline, the comparison's file controls off and the Agent tab still
///   usable; a code reference opens the file's tab at the line, and the
///   report's tab returns to the report (AC06).
/// - `menu|addressed|report STUB --attended`: the same, then the window is left on
///   Review with a thread open for a person to look; closing it ends the
///   course.

#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/adapters/sqlite/agent_setup_store.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/agent_report_view.hpp"
#include "bpr/qt/ask_menu.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/document_tab_strip.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/prompt_macros_panel.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"
#include "public_path_poll.hpp"
#include "stub_claude_search.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStyle>
#include <QTemporaryDir>
#include <QTimer>
#include <QTabBar>
#include <QToolBar>
#include <QToolButton>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;
	using access = app::detail::github_application_fixture_access;

	template <typename Predicate>
	void wait(Predicate aPredicate, const char* aMessage)
	{
		if (!e2e::wait_until_plain(aPredicate, e2e::extended_agent_budget)) {
			throw std::runtime_error{ aMessage };
		}
	}

	void expect(const bool aCondition, const std::string& aMessage)
	{
		if (!aCondition) {
			throw std::runtime_error{ aMessage };
		}
	}

	void shutdown(app::github_application& aApplication)
	{
		aApplication.begin_shutdown();
		wait([&] { return aApplication.status().ShutdownComplete; }, "shutdown");
	}

	void agent_ready(qt::workbench_window& aWindow)
	{
		aWindow.open_agent_question();
		auto*      tab   = aWindow.agent_tab();
		const auto ready = [tab] { return tab->setup_readiness() == qt::agent_setup_readiness::ready; };
		wait([tab] { return tab->setup_readiness() != qt::agent_setup_readiness::checking; }, "the agent setup looks for agents");
		if (!ready()) {
			aWindow.open_agent_setup();
			const auto check = [&aWindow] {
				auto* const button = aWindow.agent_setup()->findChild<QPushButton*>(QStringLiteral("agentSetupCheck"));
				return button != nullptr && button->isVisible() && button->isEnabled() ? button : nullptr;
			};
			wait([&] { return check() != nullptr; }, "the setup offers Check it works on the stub claude");
			check()->click();
			wait(ready, "Check it works passes on the stub claude");
			aWindow.open_agent_question();
		}
		auto* const selector = aWindow.agent_panel()->profile_selector();
		for (const auto claude : { core::claude_stream_json_profile_id, core::entry_profile_id(core::agent_protocol::claude_stream_json, 1U) }) {
			if (const auto index = selector->findData(QVariant::fromValue<qulonglong>(claude.value())); index >= 0) {
				selector->setCurrentIndex(index);
				break;
			}
		}
	}

	[[nodiscard]] auto row(const QMenu& aMenu, const QString& aId, const QString& aScope) -> QAction*
	{
		return aMenu.findChild<QAction*>(QStringLiteral("quickAsk_%1_%2").arg(aId, aScope));
	}

	[[nodiscard]] auto rows(const QMenu& aMenu) -> QStringList
	{
		auto names = QStringList{ };
		for (auto* const action : aMenu.actions()) {
			if (action->objectName().startsWith(QStringLiteral("quickAsk_"))) {
				names << action->objectName();
			}
		}
		return names;
	}

	[[nodiscard]] auto sections(const QMenu& aMenu) -> QStringList
	{
		auto titles = QStringList{ };
		for (auto* const action : aMenu.actions()) {
			if (action->isSeparator() && !action->text().isEmpty()) {
				titles << action->text();
			}
		}
		return titles;
	}

	[[nodiscard]] auto answer_of(const qt::agent_tab_view& aTab) -> std::string
	{
		return aTab.turn_count() == 0U ? std::string{ } : aTab.turn_answer(aTab.turn_count() - 1U).toStdString();
	}
}

auto main(int argc, char** argv) -> int
{
	qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsSet("QT_QPA_PLATFORM") ? qgetenv("QT_QPA_PLATFORM") : QByteArray{ "offscreen" });
	QApplication qtApplication{ argc, argv };
	const auto   course = argc > 1 ? std::string_view{ argv[1] } : std::string_view{ };
	if (argc < 3 || (course != "menu" && course != "addressed" && course != "report")) {
		std::cerr << "usage: ask_menu_public_path menu|addressed|report STUB [--attended]\n";
		return EXIT_FAILURE;
	}
	const auto    stub     = std::filesystem::path{ argv[2] };
	const auto    attended = argc > 3 && std::string_view{ argv[3] } == "--attended";
	QTemporaryDir data;
	auto          dataRoot = e2e::private_data_root(data);
	if (!dataRoot) {
		std::cerr << "private data root refused\n";
		return EXIT_FAILURE;
	}
	auto search = e2e::stub_claude_search(std::filesystem::path{ data.path().toStdString() } / "stub-claude-search", stub);
	if (!search) {
		std::cerr << "stub claude search refused\n";
		return EXIT_FAILURE;
	}
	std::unique_ptr<app::github_application> application;
	try {
		auto options                  = app::detail::github_public_path_fixture_options{ .ApplicationDataRoot = *dataRoot };
		options.EpochPhase            = app::detail::github_epoch_fixture_phase::reuse;
		options.ClaudeAgentExecutable = stub;
		options.ClaudeSearch          = search;
		auto terminal                 = false;
		auto startup                  = access::start(app::detail::github_public_path_fixture::returning_review_v1,
													  std::move(options), [&](auto aResult) { terminal = true; if (aResult) { application = std::move(*aResult); } });
		wait([&] { return terminal && startup && startup->is_quiescent(); }, "startup terminal");
		expect(application != nullptr, "startup succeeds");
		application->show();
		auto& window = application->window();
		window.resize(1440, 960);
		window.github_connect_action()->trigger();
		wait([&] { return window.github_open_action()->isEnabled(); }, "connected");
		window.home_open_bar()->setText(QStringLiteral("bpr-fixtures/nested-folders#21"));
		window.home_open_action()->trigger();
		const auto  launchesAt = [&] { return access::agent_invocation_evidence(*application).AgentProcessLauncherInvocations; };
		auto* const tab        = window.agent_tab();
		auto* const cards      = window.conversation_panel()->cards();
		const auto  path       = QStringLiteral("src/engine/renderer.cpp");
		wait([&] { return window.what_changed_action()->isEnabled(); }, "the returning pull request's commits are read");
		agent_ready(window);
		wait([&] { return window.go_to_file_line(path, 12U, false) && window.active_document_path() == path && !cards->cards().empty(); }, "the file opens beside its threads");
		const auto setupLaunches = launchesAt();
		const auto mutation      = access::review_mutation_evidence(*application);

		// AC01. One Ask at each anchor; their menus are decided when they open.
		auto* const reviewAsk = window.review_ask_button();
		auto* const fileAsk   = window.file_ask_button();
		expect(reviewAsk != nullptr && reviewAsk->isVisible() && qobject_cast<QToolBar*>(reviewAsk->parentWidget()) != nullptr, "the review bar has Ask");
		expect(fileAsk != nullptr && fileAsk->isVisible() && fileAsk->isEnabled() && fileAsk->parentWidget() == window.document_tabs(), "the file's header has Ask, in the tab strip");
		expect(window.findChild<QToolButton*>(QStringLiteral("explainFileButton")) == nullptr, "Explain this file's control is gone from the header");
		const auto line12 = std::ranges::find_if(cards->cards(), [&path](const qt::thread_card& aCard) { return aCard.Path == path && aCard.Line == 12U; });
		expect(line12 != cards->cards().end(), "the line-12 thread is listed");
		const auto identity  = line12->Id;
		auto*      threadAsk = static_cast<qt::ask_menu_button*>(nullptr);
		wait([&] {
			cards->open_thread(identity);
			QApplication::processEvents();
			threadAsk = static_cast<qt::ask_menu_button*>(cards->findChild<QToolButton*>(QStringLiteral("threadAsk")));
			return threadAsk != nullptr;
		},
			 "the open thread has Ask beside Reply");
		expect(cards->findChild<QToolButton*>(QStringLiteral("threadAskAgent")) == nullptr && cards->findChild<QToolButton*>(QStringLiteral("threadWasAddressed")) == nullptr,
			   "the thread's header agent controls are gone");

		if (course == "report") {
			reviewAsk->refresh_menu();
			row(*reviewAsk->menu(), QStringLiteral("agent-tab.what-changed"), QStringLiteral("pull-request"))->trigger();
			wait([&] { return answer_of(*tab).find("RETURNING_ECHO") != std::string::npos && access::agent_invocation_evidence(*application).Quiescent && tab->state() != qt::agent_tab_state::working; },
				 "What changed is answered");
			const auto last   = tab->turn_count() - 1U;
			const auto answer = answer_of(*tab);
			expect(answer.find("commits=1") != std::string::npos, "the request listed the commits since the review:\n" + answer);
			auto* const card = tab->turn_report_card(last);
			expect(card != nullptr && card->isVisibleTo(tab) && !tab->turn_answer_view(last)->isVisibleTo(tab), "a long answer is a report card where it was asked");
			auto* const title = card->findChild<QLabel*>(QStringLiteral("agentTurnReportTitle"));
			expect(title != nullptr && title->text() == QStringLiteral("What changed since your review"), "the card names the report");
			auto* const strip = window.document_tabs();
			const auto  files = strip->tabs().size();
			card->findChild<QPushButton*>(QStringLiteral("agentTurnReportOpen"))->click();
			QApplication::processEvents();
			const auto active = [strip] { return std::ranges::find_if(strip->tabs(), [](const qt::document_tab_presentation& aTab) { return aTab.Active; }); };
			expect(strip->tabs().size() == files + 1U && active() != strip->tabs().end() && active()->Report && active()->Name == QStringLiteral("What changed since your review"),
				   "Open report opens it as a tab in the files' strip");
			auto* const report = window.report_view();
			expect(window.report_shown() && report->isVisible() && !window.comparison_surface()->isVisible() && report->outline()->count() == 3, "the report shows with its outline in the comparison's place");
			expect(!window.file_ask_button()->isEnabled() && !window.findChild<QToolButton*>(QStringLiteral("workbenchProgressSeenStep"))->isEnabled(), "the file controls are drawn in place and off (Q9)");
			expect(tab->isVisible() && tab->ask_button() != nullptr, "the Agent tab keeps its place beside the report");
			report->go_to_heading(2);
			// A code reference read in the report: its file's tab, at the line.
			const auto& body = report->body()->original_body();
			const auto  open = body.find("](");
			expect(open != std::string::npos, "the report's references are links");
			const auto link = body.substr(open + 2U, body.find(')', open) - open - 2U);
			tab->open_reference(QString::fromStdString(link));
			wait([&] { return !window.report_shown() && window.active_document_path() == path && window.comparison_surface()->isVisible(); }, "a reference opens its file's tab");
			expect(strip->tabs().size() == files + 1U && window.file_ask_button()->isEnabled(), "the report's tab stays beside it, and the file controls are back");
			// The report's tab returns to the report.
			const auto reportAt = std::ranges::find_if(strip->tabs(), [](const qt::document_tab_presentation& aTab) { return aTab.Report; });
			auto* const bar      = strip->findChild<QTabBar*>(QStringLiteral("documentTabBar"));
			bar->setCurrentIndex(static_cast<int>(std::distance(strip->tabs().begin(), reportAt)));
			wait([&] { return window.report_shown() && window.report_view()->isVisible(); }, "the report's tab returns to the report");
			std::cout << "ask-menu-report: link=" << link << '\n';
			if (!attended) {
				shutdown(*application);
				std::cout << "ask-menu-report:pass\n";
				return EXIT_SUCCESS;
			}
		}
		if (course == "addressed") {
			// The pull request's Was this addressed? reads every thread
			// commits followed: five, whoever started them.
			reviewAsk->refresh_menu();
			auto* const everything = row(*reviewAsk->menu(), QStringLiteral("conversation.was-addressed"), QStringLiteral("pull-request"));
			expect(everything != nullptr && everything->isEnabled() && everything->text().endsWith(QStringLiteral("5 threads")), "the pull request's row reads five threads: " + (everything != nullptr ? everything->text().toStdString() : std::string{ }));
			// README.md's thread started on the head: greyed, saying why.
			auto readme   = window.ask_request(core::quick_ask_anchor::thread);
			readme.Thread = QStringLiteral("review-thread:PRRT_cc_5");
			QMenu quiet;
			window.fill_quick_ask(quiet, readme);
			auto* const greyed = row(quiet, QStringLiteral("conversation.was-addressed"), QStringLiteral("thread"));
			expect(greyed != nullptr && !greyed->isEnabled() && greyed->toolTip() == QStringLiteral("No commits since this thread started."), "a thread nothing followed is greyed with its reason");
			// The file's row: one run a thread, tbutler's line-6 thread first.
			fileAsk->refresh_menu();
			auto* const file = row(*fileAsk->menu(), QStringLiteral("conversation.was-addressed"), QStringLiteral("file"));
			expect(file != nullptr && file->isEnabled() && file->text().endsWith(QStringLiteral("2 threads")), "the file's row reads its two threads");
			const auto heights = [cards] {
				auto each = std::vector<int>{ };
				for (const auto& card : cards->cards()) {
					if (card.Id != cards->open_identity()) {
						each.push_back(card.Rect.height());
					}
				}
				return each;
			};
			const auto closedBefore = heights();
			const auto headerBefore = cards->open_card()->findChild<QToolButton*>(QStringLiteral("threadMarkRead"))->parentWidget()->sizeHint().height();
			file->trigger();
			wait([&] { return cards->thread_claims().size() == 2U && access::agent_invocation_evidence(*application).Quiescent && tab->state() != qt::agent_tab_state::working; }, "both threads are answered, one run each");
			expect(launchesAt() == setupLaunches + 2U, "one run a thread");
			expect(cards->thread_claims().contains("review-thread:PRRT_cc_1") && cards->thread_claims().contains(identity), "a claim on each thread, the one I did not comment in too");
			QApplication::processEvents();
			expect(heights() == closedBefore, "the claims change no closed card's height");
			cards->open_thread("review-thread:PRRT_cc_1");
			QApplication::processEvents();
			auto* const claimChip = cards->open_card()->findChild<QToolButton*>(QStringLiteral("threadClaimChip"));
			const auto  words     = QStringList{ QStringLiteral("Addressed"), QStringLiteral("Partly"), QStringLiteral("Not addressed"), QStringLiteral("Unclear") };
			expect(claimChip != nullptr && words.contains(claimChip->text()) && claimChip->accessibleName() == QStringLiteral("Agent's claim: %1").arg(claimChip->text()),
				   "tbutler's thread shows the claim as one chip in its header");
			expect(claimChip->parentWidget()->sizeHint().height() == headerBefore, "the chip takes no height in the header");
			claimChip->click();
			QApplication::processEvents();
			auto* const evidence = window.findChild<QFrame*>(QStringLiteral("threadClaimEvidence"));
			expect(evidence != nullptr && evidence->isVisible(), "a press shows the evidence over the code");
			auto* const then = evidence->findChild<QPlainTextEdit*>(QStringLiteral("threadClaimThen"));
			auto* const now  = evidence->findChild<QPlainTextEdit*>(QStringLiteral("threadClaimNow"));
			expect(then != nullptr && now != nullptr && !then->toPlainText().isEmpty() && !now->toPlainText().isEmpty() && then->toPlainText().contains(QStringLiteral("   6  ")),
				   "the thread's lines as it started and as they are:\n" + (then != nullptr ? then->toPlainText().toStdString() : std::string{ }));
			evidence->close();
			const auto after = access::review_mutation_evidence(*application);
			const auto line6 = std::ranges::find_if(cards->cards(), [](const qt::thread_card& aCard) { return aCard.Id == "review-thread:PRRT_cc_1"; });
			expect(after.Submissions == mutation.Submissions && after.TargetObservations == mutation.TargetObservations && after.StoredComments == mutation.StoredComments
					   && line6 != cards->cards().end() && !line6->Resolved,
				   "nothing was resolved, posted or published");
			std::cout << "ask-menu-addressed: claims=" << cards->thread_claims().size() << " chip=" << claimChip->text().toStdString() << '\n';
			if (attended) {
				window.setWindowTitle(QStringLiteral("Ask: Was this addressed? \u2014 synthetic data \u2014 close to finish"));
				QApplication::setQuitOnLastWindowClosed(true);
				window.set_profile_management_dispatch({ }, { }, [] { QApplication::quit(); });
				QTimer::singleShot(30 * 60 * 1000, &qtApplication, &QApplication::quit);
				std::cout << "ask-menu-attended:ready\n"
						  << std::flush;
				const auto result = QApplication::exec();
				shutdown(*application);
				return result;
			}
			shutdown(*application);
			std::cout << "ask-menu-addressed:pass\n";
			return EXIT_SUCCESS;
		}

		// AC02, AC03. The review bar's menu: the pull request's, the file's,
		// then the thread's quick asks.
		reviewAsk->refresh_menu();
		auto* const barMenu = reviewAsk->menu();
		expect(sections(*barMenu) == QStringList({ QStringLiteral("Pull request"), QStringLiteral("renderer.cpp"), QStringLiteral("Thread on line 12") }),
			   "three sections in order: " + sections(*barMenu).join(QStringLiteral(", ")).toStdString());
		const auto barRows = rows(*barMenu);
		std::cout << "ask-menu: review-bar rows=" << barRows.join(QLatin1Char(' ')).toStdString() << '\n';
		auto* const changed = row(*barMenu, QStringLiteral("agent-tab.what-changed"), QStringLiteral("pull-request"));
		expect(changed != nullptr && changed->isEnabled() && changed->text().startsWith(QStringLiteral("What changed since my review")),
			   "What changed names the commits a press reads: " + (changed != nullptr ? changed->text().toStdString() : std::string{ }));
		expect(row(*barMenu, QStringLiteral("agent-tab.explain-line"), QStringLiteral("file")) == nullptr && !barRows.join(' ').contains(QStringLiteral("explain-line")),
			   "a prompt without Quick ask is not listed");
		expect(row(*barMenu, QStringLiteral("agent-tab.explain-file"), QStringLiteral("file")) != nullptr && row(*barMenu, QStringLiteral("conversation.summarize"), QStringLiteral("thread")) != nullptr,
			   "the file's and the thread's quick asks are reached from the review bar");
		expect(barMenu->findChild<QAction*>(QStringLiteral("quickAskCustomQuestion")) != nullptr && barMenu->findChild<QAction*>(QStringLiteral("quickAskEdit")) != nullptr,
			   "Custom question… and Edit Quick Asks… close the menu");
		// A file with no review threads: its Was this addressed? is greyed,
		// with the reason; its own section only in the file's menu.
		auto request = window.ask_request(core::quick_ask_anchor::file);
		request.File = QStringLiteral("docs/guide.md");
		QMenu quiet;
		window.fill_quick_ask(quiet, request);
		auto* const greyed = row(quiet, QStringLiteral("conversation.was-addressed"), QStringLiteral("file"));
		expect(greyed != nullptr && !greyed->isEnabled() && greyed->toolTip() == QStringLiteral("No review threads on this file.") && greyed->statusTip() == greyed->toolTip()
				   && greyed->whatsThis() == greyed->toolTip(),
			   "a quick ask that cannot run is greyed, its reason its tooltip and accessible text");
		expect(sections(quiet).isEmpty(), "a one-scope menu has no section titles");
		// AC08: the greyed row is reachable by keyboard.
		expect(quiet.style()->styleHint(QStyle::SH_Menu_AllowActiveAndDisabled, nullptr, &quiet) == 1, "greyed rows are reached by the arrow keys");
		// No thread open: the thread section is not listed.
		auto noThread   = window.ask_request(core::quick_ask_anchor::review_bar);
		noThread.Thread = QString{ };
		QMenu bare;
		window.fill_quick_ask(bare, noThread);
		expect(!sections(bare).contains(QStringLiteral("Thread on line 12")) && sections(bare).size() == 2, "a scope whose subject is absent is not listed");
		// The thread's own menus.
		threadAsk->refresh_menu();
		expect(rows(*threadAsk->menu()).size() >= 3 && row(*threadAsk->menu(), QStringLiteral("conversation.was-addressed"), QStringLiteral("thread")) != nullptr,
			   "the thread's Ask lists its quick asks, Was this addressed? among them");
		auto threadMenu = std::unique_ptr<QMenu>{ cards->thread_menu(identity) };
		expect(threadMenu != nullptr, "the thread has a right-click menu");
		auto* const askItem = threadMenu->findChild<QMenu*>(QStringLiteral("threadAskMenu"));
		expect(askItem != nullptr && rows(*askItem) == rows(*threadAsk->menu()), "the thread's menu has one Ask item with the same quick asks");
		for (auto* const action : threadMenu->actions()) {
			expect(action->objectName() != QStringLiteral("threadAskAgentAction") && action->objectName() != QStringLiteral("threadWasAddressedAction") && !action->text().startsWith(QStringLiteral("Ask agent")),
				   "the agent items it replaces are gone");
		}
		auto*       strip   = window.document_tabs();
		const auto  active  = std::ranges::find_if(strip->tabs(), [](const qt::document_tab_presentation& aTab) { return aTab.Active; });
		auto        tabMenu = std::unique_ptr<QMenu>{ strip->make_tab_menu(active->Id, &window) };
		auto* const tabAsk  = tabMenu != nullptr ? tabMenu->findChild<QMenu*>(QStringLiteral("documentTabAskMenu")) : nullptr;
		expect(tabAsk != nullptr && row(*tabAsk, QStringLiteral("agent-tab.explain-file"), QStringLiteral("file")) != nullptr, "the file tab's menu has Ask with the file's quick asks");
		expect(tabMenu->findChild<QAction*>(QStringLiteral("documentTabExplainFile")) == nullptr, "Explain This File is gone from the tab's menu");
		expect(launchesAt() == setupLaunches, "building and filtering every menu launched nothing");

		// AC02. Custom question… attaches the thread and sends nothing.
		const auto turns = tab->turn_count();
		threadAsk->menu()->findChild<QAction*>(QStringLiteral("quickAskCustomQuestion"))->trigger();
		QApplication::processEvents();
		expect(tab->isVisible() && tab->conversation_chip_text().contains(QStringLiteral("renderer.cpp")) && tab->turn_count() == turns && launchesAt() == setupLaunches,
			   "Custom question… opens the Agent tab on the thread and sends nothing");
		// Edit Quick Asks… opens Settings at the suggested prompts.
		barMenu->findChild<QAction*>(QStringLiteral("quickAskEdit"))->trigger();
		QApplication::processEvents();
		auto* const macros = window.prompt_macros();
		expect(macros != nullptr && window.settings() != nullptr && window.settings()->current_page() == qt::settings_page::agents_and_context, "Edit Quick Asks… opens Settings at Agents & context");
		expect(macros->list(core::prompt_place::agent_tab)->count() > 0, "the suggested prompts are listed");
		// AC04. The built-in rows, and the sheet's Quick ask row.
		const auto has = [macros](const std::string& aId) { return std::ranges::any_of(macros->macros(), [&aId](const core::prompt_macro& aMacro) { return aMacro.Id == aId; }); };
		expect(has("agent-tab.what-changed") && has("conversation.was-addressed") && has("agent-tab.explain-file"), "What changed, Was this addressed? and Explain this file have built-in rows");
		macros->edit("agent-tab.what-could-break", [](qt::prompt_macro_sheet& aSheet) {
			expect(aSheet.quick_ask_box()->isChecked() && aSheet.scope_button(core::prompt_scope::pull_request)->isChecked(), "a shipped quick ask's sheet says so");
			expect(!aSheet.scope_button(core::prompt_scope::file)->isEnabled() && !aSheet.scope_button(core::prompt_scope::thread)->isEnabled(), "a scope narrower than the pull request cannot be set");
			aSheet.quick_ask_box()->setChecked(false);
			aSheet.finish(qt::prompt_macro_sheet::outcome::saved);
		});
		macros->add_prompt(core::prompt_place::agent_tab, [](qt::prompt_macro_sheet& aSheet) {
			aSheet.name_field()->setText(QStringLiteral("Security pass on {file.name}"));
			aSheet.template_field()->setPlainText(QStringLiteral("Check {file} for injection."));
			auto* const about = aSheet.findChild<QComboBox*>(QStringLiteral("promptMacroSubject"));
			about->setCurrentIndex(about->findText(QStringLiteral("The open file")));
			aSheet.quick_ask_box()->setChecked(true);
			expect(aSheet.scope_button(core::prompt_scope::file)->isChecked() && !aSheet.scope_button(core::prompt_scope::thread)->isEnabled(), "a file's prompt: File on, Thread unavailable");
			aSheet.scope_button(core::prompt_scope::pull_request)->setChecked(true);
			expect(aSheet.save_button()->isEnabled(), "the sheet can be saved: " + aSheet.problems_text().toStdString());
			aSheet.finish(qt::prompt_macro_sheet::outcome::saved);
		});
		wait([&] {
			fileAsk->refresh_menu();
			return std::ranges::any_of(rows(*fileAsk->menu()), [](const QString& aName) { return aName.startsWith(QStringLiteral("quickAsk_own.")); });
		},
			 "the own quick ask is in the file's Ask");
		reviewAsk->refresh_menu();
		expect(row(*reviewAsk->menu(), QStringLiteral("agent-tab.what-could-break"), QStringLiteral("pull-request")) == nullptr, "Quick ask off: no longer in the review bar's Ask");
		auto store  = adapters::sqlite::make_prompt_macro_store(*dataRoot);
		auto stored = store->load();
		expect(stored && stored->Revision == 2U, "both edits are in the profile's store");
		const auto change = std::ranges::find(stored->Differences.Changes, std::string{ "agent-tab.what-could-break" }, &core::prompt_builtin_change::Id);
		expect(change != stored->Differences.Changes.end() && change->Body && !change->Body->QuickAsk, "Quick ask off is saved with the built-in");
		expect(stored->Differences.Own.size() == 1U && stored->Differences.Own.front().Body.QuickAsk && stored->Differences.Own.front().Body.Scopes == core::prompt_scopes{ .File = true, .PullRequest = true },
			   "the own prompt's Quick ask and scopes are saved");
		const auto sheetLaunches = launchesAt();
		expect(sheetLaunches == setupLaunches, "editing prompts launched nothing");

		// AC08. Every control is named and takes focus.
		for (auto* const button : { static_cast<QToolButton*>(reviewAsk), static_cast<QToolButton*>(fileAsk), static_cast<QToolButton*>(threadAsk) }) {
			expect(!button->accessibleName().isEmpty() && button->focusPolicy() == Qt::StrongFocus && button->popupMode() == QToolButton::InstantPopup, "an Ask control is named and reached by keyboard");
		}

		// AC04. What changed from the review bar sends the action's own prompt.
		reviewAsk->refresh_menu();
		row(*reviewAsk->menu(), QStringLiteral("agent-tab.what-changed"), QStringLiteral("pull-request"))->trigger();
		wait([&] { return answer_of(*tab).find("RETURNING_ECHO") != std::string::npos && access::agent_invocation_evidence(*application).Quiescent; }, "What changed is answered");
		const auto answer = answer_of(*tab);
		expect(launchesAt() == setupLaunches + 1U && answer.find("system=1") != std::string::npos && answer.find("changes=1") != std::string::npos,
			   "one press, one call, with its own prompt and the change:\n" + answer);
		expect(tab->turn_returning(tab->turn_count() - 1U) == core::returning_operation::latest_upload, "the turn is What changed");

		const auto after = access::review_mutation_evidence(*application);
		expect(after.Submissions == mutation.Submissions && after.TargetObservations == mutation.TargetObservations && after.StoredComments == mutation.StoredComments, "nothing reached GitHub");
		if (attended) {
			window.setWindowTitle(QStringLiteral("Ask — synthetic data — close to finish"));
			QApplication::setQuitOnLastWindowClosed(true);
			window.set_profile_management_dispatch({ }, { }, [] { QApplication::quit(); });
			QTimer::singleShot(30 * 60 * 1000, &qtApplication, &QApplication::quit);
			std::cout << "ask-menu-attended:ready\n"
					  << std::flush;
			const auto result = QApplication::exec();
			shutdown(*application);
			return result;
		}
		shutdown(*application);
		std::cout << "ask-menu:pass\n";
		return EXIT_SUCCESS;
	} catch (const std::exception& aError) {
		std::cerr << "ask-menu: " << aError.what() << '\n';
		if (application) {
			const auto& tab = *application->window().agent_tab();
			std::cerr << "github-status: " << application->window().github_status_text() << "\nagent-panel: " << application->window().agent_panel()->status_text().toStdString()
					  << "\nlast-answer: " << answer_of(tab) << '\n';
			shutdown(*application);
		}
		return EXIT_FAILURE;
	}
}
