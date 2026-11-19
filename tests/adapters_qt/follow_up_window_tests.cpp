/// \file
/// \brief [[BPR-2286]]. The follow-up window and the controls that open it:
/// each press leaves as an intent; a proposal is shown beside what it replaces
/// and nothing is applied until Use; the run-starting controls wear the Agent
/// accent and Stop the ordinary roles; a report's finding offers Ask follow-up
/// and shows its updated suggested comment; a thread's Ask menu offers Ask
/// about this. Nothing here runs an agent or writes anywhere.
#include "bpr/core/agent_report.hpp"
#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/ask_menu.hpp"
#include "bpr/qt/follow_up_window.hpp"
#include "bpr/qt/report_page.hpp"

#include <QAction>
#include <QApplication>
#include <QFrame>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string& aMessage)
	{
		if (!aCondition) {
			std::cerr << "follow-up-window-tests: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto suggestion() -> core::follow_up_subject
	{
		auto subject  = core::follow_up_subject{ };
		subject.Kind  = core::follow_up_subject_kind::suggested_comment;
		subject.Key   = "finding:7:0";
		subject.Path  = "src/engine/renderer.cpp";
		subject.First = 12U;
		subject.Last  = 13U;
		subject.Title = "A zero height still reaches the swap chain.";
		subject.Text  = "issue (blocking): clamp both.";
		return subject;
	}

	void test_the_window_asks_shows_and_applies_only_on_use()
	{
		auto asked     = std::vector<QString>{ };
		auto proposed  = std::vector<core::follow_up_apply>{ };
		auto used      = std::vector<std::uint64_t>{ };
		auto discarded = std::vector<std::uint64_t>{ };
		auto stops     = 0;
		auto closed    = 0;
		auto window    = qt::follow_up_window{ };
		window.set_intents({
			.Ask     = [&](const QString& aQuestion) { asked.push_back(aQuestion); },
			.Propose = [&](const core::follow_up_apply aApply) { proposed.push_back(aApply); },
			.Use     = [&](const std::uint64_t aId) { used.push_back(aId); },
			.Discard = [&](const std::uint64_t aId) { discarded.push_back(aId); },
			.Stop    = [&] { ++stops; },
			.Closed  = [&] { ++closed; },
		});
		auto state = core::open_follow_up(suggestion());
		window.present(state, QStringLiteral("Fixture agent"), { });
		window.resize(380, 520);
		window.show();
		QApplication::processEvents();
		expect(window.heading_text() == QStringLiteral("Suggested comment at renderer.cpp:12–13"), "the heading names the item and its place: " + window.heading_text().toStdString());
		expect(window.item_text() == QStringLiteral("issue (blocking): clamp both."), "the item is quoted as it is");
		expect(window.apply_button()->isVisible() && window.apply_button()->text() == QStringLiteral("Update suggested comment"), "a suggestion offers Update suggested comment");
		expect(qt::agent_run_control(*window.ask_button()) && qt::agent_run_control(*window.apply_button()), "Ask and the apply action start runs: the Agent accent");
		expect(!window.proposal_panel()->isVisible(), "no proposal yet");

		// Return asks; Shift+Return is a new line; a blank question is not sent.
		window.question_field()->setPlainText(QStringLiteral("Why is this blocking?"));
		auto enter = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
		QApplication::sendEvent(window.question_field(), &enter);
		expect(asked == std::vector{ QStringLiteral("Why is this blocking?") } && window.question_field()->toPlainText().isEmpty(), "Return asks and clears the field");
		auto shiftEnter = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::ShiftModifier };
		QApplication::sendEvent(window.question_field(), &shiftEnter);
		expect(asked.size() == 1U, "Shift+Return asks nothing");
		window.ask_button()->click();
		expect(asked.size() == 1U && window.status_text() == qt::follow_up_refusal_words(core::follow_up_refusal::empty_question), "a blank question is not sent, and it says so");

		// A run going: Stop, in the ordinary roles; the apply action waits.
		static_cast<void>(core::ask_follow_up(state, "Why is this blocking?"));
		window.present(state, QStringLiteral("Fixture agent"), { });
		expect(window.running() && window.ask_button()->text() == QStringLiteral("Stop") && !qt::agent_run_control(*window.ask_button()) && !window.apply_button()->isEnabled(),
			   "while it answers, Ask is Stop in the ordinary roles and Update waits");
		expect(window.findChild<QLabel*>(QStringLiteral("followUpWorking"))->text() == QStringLiteral("Fixture agent is answering…"), "it says who is answering");
		window.ask_button()->click();
		expect(stops == 1 && asked.size() == 1U, "Stop stops; it asks nothing");
		static_cast<void>(core::settle_follow_up(state, state.Pending, std::string{ "It asserts on minimise." }));
		window.present(state, QStringLiteral("Fixture agent"), { });
		expect(!window.running() && window.answers() == QStringList{ QStringLiteral("It asserts on minimise.") }, "the answer is shown as a round");

		// Update: the proposal beside what it replaces; Use and Discard name it.
		window.apply_button()->click();
		expect(proposed == std::vector{ core::follow_up_apply::update_suggestion }, "the apply action asks for the proposal");
		const auto step = core::propose_follow_up(state, core::follow_up_apply::update_suggestion, state.Subject.Text);
		static_cast<void>(core::settle_follow_up(state, step.Request->Id, std::string{ "issue (blocking): clamp the height too." }));
		window.present(state, QStringLiteral("Fixture agent"), { });
		QApplication::processEvents();
		expect(window.proposal_panel()->isVisible() && window.proposal_text() == QStringLiteral("issue (blocking): clamp the height too."), "the proposal is shown");
		expect(window.findChild<QLabel*>(QStringLiteral("followUpProposalBefore"))->text().contains(QStringLiteral("issue (blocking): clamp both.")), "beside what it replaces");
		expect(window.item_text() == QStringLiteral("issue (blocking): clamp both."), "the item is unchanged before Use");
		expect(window.use_button()->hasFocus(), "the keyboard is on Use");
		window.discard_button()->click();
		window.use_button()->click();
		expect(discarded == std::vector{ state.Proposal->Id } && used == std::vector{ state.Proposal->Id }, "Discard and Use name the proposal shown");

		// A refusal is said in words.
		static_cast<void>(core::apply_follow_up(state, state.Proposal->Id, "changed meanwhile"));
		window.present(state, QStringLiteral("Fixture agent"), { });
		expect(window.status_text() == qt::follow_up_refusal_words(core::follow_up_refusal::target_changed) && window.status_text().contains(QStringLiteral("Nothing was replaced")), "a changed text is refused in words");
		window.present(state, QStringLiteral("Fixture agent"), QStringLiteral("The agent did not answer (timeout)."));
		expect(window.status_text() == QStringLiteral("The agent did not answer (timeout)."), "a run's failure is said as the host said it");

		// A thread offers Draft reply; one that takes no reply offers nothing.
		auto thread    = core::follow_up_subject{ };
		thread.Kind    = core::follow_up_subject_kind::thread;
		thread.Key     = "review-thread:1";
		thread.Path    = "src/engine/renderer.cpp";
		thread.First   = 12U;
		thread.Last    = 12U;
		thread.Replies = true;
		thread.Thread  = { { .Author = "octo-reviewer", .Body = "Should the height clamp too?" } };
		window.question_field()->setPlainText(QStringLiteral("left over"));
		window.present(core::open_follow_up(thread), { }, { });
		expect(window.heading_text() == QStringLiteral("Conversation at renderer.cpp:12") && window.item_text() == QStringLiteral("octo-reviewer: Should the height clamp too?"), "a thread shows its posts");
		expect(window.apply_button()->text() == QStringLiteral("Draft reply") && window.answers().isEmpty() && window.question_field()->toPlainText().isEmpty(), "another item: Draft reply, its own rounds, a clear field");
		thread.Replies = false;
		window.present(core::open_follow_up(thread), { }, { });
		expect(!window.apply_button()->isVisible(), "a thread that takes no reply offers no apply action");
		auto draft = suggestion();
		draft.Kind = core::follow_up_subject_kind::draft;
		draft.Key  = "draft";
		window.present(core::open_follow_up(draft), { }, { });
		expect(window.apply_button()->text() == QStringLiteral("Update draft") && window.heading_text().startsWith(QStringLiteral("Your draft at")), "a draft offers Update draft");
		window.close();
		expect(closed == 1, "closing says so");
	}

	void test_a_report_finding_offers_ask_follow_up()
	{
		const auto report  = core::dress_report("# Review\n\n**Verdict:** Needs changes\n\nOne thing.\n\n## Must fix\n\n### issue (blocking): A zero height\n\n`src/engine/renderer.cpp:12-13`\n\nThe height is not clamped.\n");
		auto       actions = qt::report_page_actions{ .Findings = core::report_findings(report) };
		expect(!actions.Findings.empty(), "the report has a finding");
		const auto outline = actions.Findings.front().Outline;
		actions.Link       = [](const std::uint32_t aOutline, const qt::report_link aLink) { return QStringLiteral("action-%1-%2").arg(aOutline).arg(static_cast<int>(aLink)); };
		const auto askLink = QStringLiteral("action-%1-%2").arg(outline).arg(static_cast<int>(qt::report_link::ask));
		expect(!qt::report_page_html(report, { }, actions).contains(askLink), "without a follow-up, no Ask follow-up");
		actions.FollowUp = true;
		const auto page  = qt::report_page_html(report, { }, actions);
		expect(page.contains(askLink) && page.contains(QStringLiteral("Ask follow-up")), "a finding offers Ask follow-up beside its keep action");
		expect(!page.contains(QStringLiteral("Suggested comment, updated")), "nothing updated yet");
		actions.Revised[outline] = QStringLiteral("issue (blocking): clamp <both>.");
		const auto revised       = qt::report_page_html(report, { }, actions);
		expect(revised.contains(QStringLiteral("Suggested comment, updated:")) && revised.contains(QStringLiteral("clamp &lt;both&gt;.")), "the updated comment shows, escaped");
	}

	void test_a_threads_ask_menu_offers_ask_about_this()
	{
		auto       followed = std::vector<QString>{ };
		auto       dispatch = qt::ask_menu_dispatch{ };
		const auto request  = qt::ask_menu_request{ .Anchor = core::quick_ask_anchor::thread, .File = { }, .Thread = QStringLiteral("review-thread:1"), .ThreadTitle = QStringLiteral("Thread on line 12") };
		{
			QMenu menu;
			qt::fill_ask_menu(menu, { }, request, dispatch);
			expect(menu.findChild<QAction*>(QStringLiteral("quickAskFollowUp")) == nullptr, "without a dispatch, no Ask about this");
		}
		dispatch.FollowUp = [&followed](const qt::ask_menu_request& aRequest) { followed.push_back(aRequest.Thread); };
		QMenu menu;
		qt::fill_ask_menu(menu, { }, request, dispatch);
		auto* const row = menu.findChild<QAction*>(QStringLiteral("quickAskFollowUp"));
		expect(row != nullptr && row->text() == QStringLiteral("Ask about this…"), "a thread's menu offers Ask about this");
		row->trigger();
		expect(followed == std::vector{ QStringLiteral("review-thread:1") }, "it names the thread");
		QMenu fileMenu;
		qt::fill_ask_menu(fileMenu, { }, qt::ask_menu_request{ .Anchor = core::quick_ask_anchor::file, .File = QStringLiteral("a.cpp"), .Thread = { }, .ThreadTitle = { } }, dispatch);
		expect(fileMenu.findChild<QAction*>(QStringLiteral("quickAskFollowUp")) == nullptr, "a file's menu has none");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsSet("QT_QPA_PLATFORM") ? qgetenv("QT_QPA_PLATFORM") : QByteArray{ "offscreen" });
	QApplication application{ argc, argv };
	test_the_window_asks_shows_and_applies_only_on_use();
	test_a_report_finding_offers_ask_follow_up();
	test_a_threads_ask_menu_offers_ask_about_this();
	std::cout << "follow-up-window-tests:pass\n";
	return EXIT_SUCCESS;
}
