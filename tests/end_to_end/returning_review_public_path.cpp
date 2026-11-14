/// \file
/// \brief RFC-BPR-0116 ([[BPR-2073]], [[BPR-2074]]). A returning pull request's
/// two questions through the shipped window, on returning-review-v1 served at
/// its next push (the reviewer's review and comment are on the first head),
/// with the scoped writing stub as the installed Claude.
///
/// Courses, by the first argument:
///
/// - `latest STUB`: opening launches no agent; the Pull Request menu's What
///   Changed Since My Review and the Agent tab's suggestion are offered; a
///   press runs one fresh, tool-free call whose request names both revisions
///   and carries the change and whose system prompt is its own; the answer's
///   references are links; Ask more is offered; nothing reaches GitHub.
/// - `addressed STUB`: every thread commits followed offers Was this addressed?
///   (RFC-BPR-0128: whoever wrote it); the line-12 thread's
///   its press asks about that conversation with its lines then and now; the
///   answer's claim is shown; the thread is not resolved and nothing is posted.
/// - `gone STUB`: with my review taken to be on a commit the review's objects
///   do not hold (a rewritten branch), the menu item and the Agent tab say
///   the earlier code can't be read, and nothing is sent.
/// - `latest STUB --attended` / `addressed STUB --attended`: the same, then
///   the window stays open for a person to look; closing it ends the course.

#include "../../src/app/src/github_application_fixture_access.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/ask_menu.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "private_data_root.hpp"
#include "public_path_poll.hpp"
#include "stub_claude_search.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>

#include <algorithm>
#include <cstdlib>
#include <iostream>
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

	/// The Agent tab's setup finds the stub claude and its Check passes, as
	/// a person's does; nothing is asked about the pull request.
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
		// The installed Claude is the agent asked: the stub is the only
		// program found.
		auto* const selector = aWindow.agent_panel()->profile_selector();
		for (const auto claude : { core::claude_stream_json_profile_id, core::entry_profile_id(core::agent_protocol::claude_stream_json, 1U) }) {
			if (const auto index = selector->findData(QVariant::fromValue<qulonglong>(claude.value())); index >= 0) {
				selector->setCurrentIndex(index);
				break;
			}
		}
		std::cout << "agent: " << selector->currentText().toStdString() << '\n';
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
	if (argc < 3) {
		std::cerr << "usage: returning_review_public_path latest|addressed|gone STUB [--attended]\n";
		return EXIT_FAILURE;
	}
	const auto    course   = std::string_view{ argv[1] };
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
		// AC02. The reviewed head is a commit the review's objects do not
		// hold: a rewritten branch whose earlier commits are gone.
		if (course == "gone") {
			access::force_returning_review_head(*application, "0badc0de0badc0de0badc0de0badc0de0badc0d");
		}
		application->show();
		auto& window = application->window();
		window.resize(1440, 960);
		window.github_connect_action()->trigger();
		wait([&] { return window.github_open_action()->isEnabled(); }, "connected");
		window.home_open_bar()->setText(QStringLiteral("bpr-fixtures/nested-folders#21"));
		window.home_open_action()->trigger();
		auto* const whatChanged = window.what_changed_action();
		auto* const tab         = window.agent_tab();
		expect(whatChanged != nullptr, "the Pull Request menu has What Changed Since My Review");
		const auto launchesAt = [&] { return access::agent_invocation_evidence(*application).AgentProcessLauncherInvocations; };

		if (course == "gone") {
			wait([&] { return whatChanged->text().contains(QStringLiteral("can't be read here")); }, "the menu says the earlier code can't be read");
			agent_ready(window);
			const auto before = launchesAt();
			expect(!whatChanged->isEnabled() && whatChanged->text().contains(QStringLiteral("0badc0d")), "unavailable, naming the reviewed commit");
			expect(tab->latest_upload_offer().State == core::latest_upload_state::unavailable && tab->latest_upload_note() != nullptr && tab->latest_upload_note()->isVisible()
					   && tab->latest_upload_note()->text().contains(QStringLiteral("can't be read here")),
				   "the Agent tab says why, where the suggestion would be");
			expect(!tab->suggestions().contains(QStringLiteral("What changed since my review")), "no suggestion is offered in its place");
			tab->ask_returning(core::returning_operation::latest_upload);
			QApplication::processEvents();
			expect(launchesAt() == before, "a press when unavailable launches nothing");
			shutdown(*application);
			std::cout << "returning-review-gone:pass\n";
			return EXIT_SUCCESS;
		}

		// AC01, AC06. Offered once the earlier revision is read; opening and
		// reading launched no agent.
		wait([&] { return whatChanged->isEnabled(); }, "What Changed Since My Review is offered");
		expect(launchesAt() == 0U, "opening a returning pull request launches no agent");
		agent_ready(window);
		expect(tab->suggestions().contains(QStringLiteral("What changed since my review")), "the Agent tab suggests it");
		const auto setupLaunches = launchesAt();
		const auto mutation      = access::review_mutation_evidence(*application);

		if (course == "latest") {
			whatChanged->trigger();
			wait([&] { return answer_of(*tab).find("RETURNING_ECHO") != std::string::npos && access::agent_invocation_evidence(*application).Quiescent; }, "the answer arrives");
			const auto answer = answer_of(*tab);
			const auto last   = tab->turn_count() - 1U;
			expect(launchesAt() == setupLaunches + 1U, "one press, one call");
			expect(answer.find("system=1") != std::string::npos, "its own prompt was the system prompt:\n" + answer);
			expect(answer.find("changes=1") != std::string::npos, "the change between the revisions was in the request:\n" + answer);
			expect(tab->turn_returning(last) == core::returning_operation::latest_upload, "the turn is the returning question");
			auto* const view = tab->turn_answer_view(last);
			expect(view != nullptr && view->original_body().find("](") != std::string::npos && view->original_body().find("renderer.cpp") != std::string::npos,
				   "the answer's code references are links");
			auto* const askMore = tab->turn_ask_more_button(last);
			expect(askMore != nullptr && askMore->isVisible(), "Ask more is offered on the fresh answer");
			const auto after = access::review_mutation_evidence(*application);
			expect(after.Submissions == mutation.Submissions && after.TargetObservations == mutation.TargetObservations && after.StoredComments == mutation.StoredComments,
				   "nothing reached GitHub");
			std::cout << "returning-review-latest: answer=" << answer.substr(0U, answer.find('\n')) << '\n';
			if (!attended) {
				askMore->click();
				QApplication::processEvents();
				expect(tab->ask_button() != nullptr, "the question field is ready for more");
			}
		} else {
			auto* const cards   = window.conversation_panel()->cards();
			// RFC-BPR-0128: any review thread whose start commits have
			// followed, whoever wrote it; README.md's thread started on the
			// head, so it is not offered.
			const auto offered = cards->addressed_threads();
			expect(offered.size() == 5U && !offered.contains("review-thread:PRRT_cc_5"), "every thread commits followed offers Was this addressed?, whoever wrote it");
			const auto identity = std::string{ "review-thread:PRRT_cc_2" };
			expect(offered.contains(identity), "the line-12 thread is among them");
			// Its file open in Review, as a person reaches the thread beside its code.
			wait([&] { return window.go_to_file_line(QStringLiteral("src/engine/renderer.cpp"), 12U, false) && std::ranges::any_of(cards->cards(), [&](const auto& aCard) { return aCard.Id == identity; }); },
				 "the thread's file opens beside its threads");
			// RFC-BPR-0128: Was this addressed? is the open thread's quick ask.
			auto* button = static_cast<QAction*>(nullptr);
			wait([&] {
				cards->open_thread(identity);
				QApplication::processEvents();
				auto* const ask = static_cast<qt::ask_menu_button*>(window.findChild<QToolButton*>(QStringLiteral("threadAsk")));
				if (ask != nullptr) {
					ask->refresh_menu();
					button = ask->menu()->findChild<QAction*>(QStringLiteral("quickAsk_conversation.was-addressed_thread"));
				}
				return button != nullptr && button->isEnabled();
			},
				 "the open thread's Ask offers Was this addressed?");
			button->trigger();
			wait([&] { return answer_of(*tab).find("RETURNING_ECHO") != std::string::npos && access::agent_invocation_evidence(*application).Quiescent; }, "the answer arrives");
			const auto answer = answer_of(*tab);
			const auto last   = tab->turn_count() - 1U;
			expect(launchesAt() == setupLaunches + 1U, "one press, one call");
			expect(answer.find("system=2") != std::string::npos && answer.find("conversation=1") != std::string::npos, "its own prompt and the conversation were sent:\n" + answer);
			expect(answer.find("then=1") != std::string::npos && answer.find("now=1") != std::string::npos, "the lines as they were and as they are were sent:\n" + answer);
			expect(tab->turn_returning(last) == core::returning_operation::proposed_fix && tab->conversation_chip_text().contains(QStringLiteral("renderer.cpp")), "asked about the conversation");
			auto* const claim = tab->turn_claim(last);
			expect(claim != nullptr && claim->isVisible() && claim->text() == QStringLiteral("Agent's claim: Partly"), "the answer's lead is shown as the agent's claim");
			const auto after = access::review_mutation_evidence(*application);
			expect(after.Submissions == mutation.Submissions && after.TargetObservations == mutation.TargetObservations && after.StoredComments == mutation.StoredComments,
				   "nothing was resolved or posted");
			std::cout << "returning-review-addressed: thread=" << identity << " claim=" << claim->text().toStdString() << '\n';
		}
		if (attended) {
			window.setWindowTitle(QStringLiteral("Returning pull request — synthetic data — close to finish"));
			QApplication::setQuitOnLastWindowClosed(true);
			window.set_profile_management_dispatch({ }, { }, [] { QApplication::quit(); });
			QTimer::singleShot(30 * 60 * 1000, &qtApplication, &QApplication::quit);
			std::cout << "returning-review-attended:ready\n"
					  << std::flush;
			const auto result = QApplication::exec();
			shutdown(*application);
			return result;
		}
		shutdown(*application);
		std::cout << "returning-review-" << course << ":pass\n";
		return EXIT_SUCCESS;
	} catch (const std::exception& aError) {
		std::cerr << "returning-review-" << course << ": " << aError.what() << '\n';
		if (application) {
			const auto& tab = *application->window().agent_tab();
			std::cerr << "github-status: " << application->window().github_status_text() << "\nwhat-changed: " << application->window().what_changed_action()->text().toStdString() << '\n';
			std::cerr << "agent-panel: " << application->window().agent_panel()->status_text().toStdString() << "\nlast-answer: " << answer_of(tab) << '\n';
			shutdown(*application);
		}
		return EXIT_FAILURE;
	}
}
