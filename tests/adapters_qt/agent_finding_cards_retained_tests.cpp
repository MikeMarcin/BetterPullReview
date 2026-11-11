/// \file
/// \brief Owner report 2026-11-10: "I had an agent response which included 4
/// suggestion draft comments. I added one and after a while I went back and
/// all the other suggestions were gone."
///
/// \details A review turn's finding cards live under its answer. The host
/// shows the thread again whenever it reads the agent's native history back
/// (the first epoch publication after a turn ends does, and so does choosing
/// another pull request or agent and coming back), and `present_thread`
/// used to drop every card under a turn and never make them again. These
/// checks hold four cards under one answer, add one as a draft, and show the
/// same thread again the ways the host does: the drafted card still says
/// Drafted, once, and the three others are still there to add.

#include "bpr/application/agent_finding_delivery.hpp"
#include "bpr/application/agent_invocation_host.hpp"
#include "bpr/application/agent_launch.hpp"
#include "bpr/application/agent_workspace.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/finding_queue_view.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string& aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void settle()
	{
		for (int pass = 0; pass < 4; ++pass) {
			QApplication::processEvents();
		}
	}

	void open_review(qt::workbench_window& aWindow)
	{
		aWindow.resize(1'440, 900);
		aWindow.show();
		auto opened   = application::workbench_projection{ };
		auto commands = core::workbench_command_delta{ };
		for (std::size_t index = 0U; index < core::workbench_command_count; ++index) {
			commands.Commands[index] = { .Id = static_cast<core::workbench_command_id>(index), .Enabled = true, .DisabledReason = core::workbench_reason_code::none };
		}
		opened.Commands = commands;
		opened.Drawers  = core::workbench_drawer_delta{ .Files = core::workbench_drawer_state::expanded, .Conversations = core::workbench_drawer_state::expanded };
		opened.Layout   = application::workbench_layout_delta{ .FilesWidth = 260U, .ConversationsWidth = 520U };
		aWindow.publish(opened);
		settle();
		aWindow.choose_modality(qt::review_modality::review);
		settle();
		aWindow.tray_tabs()->setCurrentIndex(3);
		auto profile        = core::agent_profile{ };
		profile.Id          = core::agent_profile_id{ 1U };
		profile.DisplayName = "Claude Code";
		profile.Executable  = "/usr/bin/true";
		aWindow.agent_panel()->offer_profiles(std::vector{ profile });
		settle();
		aWindow.agent_tab()->refresh();
		settle();
	}

	[[nodiscard]] auto finding_at(const std::uint32_t aLine) -> core::agent_finding
	{
		auto finding             = core::agent_finding{ };
		finding.Provider         = core::agent_profile_id{ 1U };
		finding.Severity         = core::defect_severity::minor;
		finding.Confidence       = core::agent_finding_confidence::high;
		finding.Category         = core::defect_category::correctness;
		finding.Path             = "src/engine/renderer.cpp";
		finding.Lines            = { .Begin = aLine, .End = aLine };
		finding.Claim            = "Finding at line " + std::to_string(aLine + 1U) + ".";
		finding.SuggestedComment = "suggestion: look at line " + std::to_string(aLine + 1U);
		return finding;
	}

	[[nodiscard]] auto state_of(const QWidget* aCard) -> qt::agent_finding_card_state
	{
		return static_cast<qt::agent_finding_card_state>(aCard->property("agentFindingStateName").toInt());
	}

	/// \brief The cards under \p aTurn: how many are open, how many drafted,
	/// and whether each open one still offers Add as draft comment.
	struct card_count
	{
		std::size_t Open    = 0U;
		std::size_t Drafted = 0U;
		std::size_t Other   = 0U;
		bool        Offered = true;
	};

	[[nodiscard]] auto count_cards(const qt::agent_tab_view& aTab, const std::size_t aTurn) -> card_count
	{
		auto counted = card_count{ };
		for (auto* const card : aTab.turn_cards(aTurn)) {
			switch (state_of(card)) {
			case qt::agent_finding_card_state::open: {
				++counted.Open;
				const auto* const draft = card->findChild<QPushButton*>(QStringLiteral("agentFindingDraftComment"));
				counted.Offered         = counted.Offered && draft != nullptr && !draft->isHidden();
				break;
			}
			case qt::agent_finding_card_state::drafted: ++counted.Drafted; break;
			default:                                    ++counted.Other; break;
			}
		}
		return counted;
	}

	void expect_three_open_one_drafted(const qt::agent_tab_view& aTab, const std::size_t aTurn, const std::string& aWhen)
	{
		const auto counted = count_cards(aTab, aTurn);
		expect(aTab.turn_cards(aTurn).size() == 4U, aWhen + ": the turn still has its four cards, not " + std::to_string(aTab.turn_cards(aTurn).size()));
		expect(counted.Drafted == 1U && counted.Open == 3U && counted.Other == 0U && counted.Offered,
			   aWhen + ": one card says Drafted and the other three still offer Add as draft comment (open " + std::to_string(counted.Open) + ", drafted "
				   + std::to_string(counted.Drafted) + ")");
	}

	void test_unadded_suggestions_survive_the_thread_shown_again()
	{
		qt::workbench_window window;
		open_review(window);
		auto* const agent = window.agent_tab();

		// The review turn: its answer ends with the findings block, which the
		// native history keeps as written, and its four findings arrive.
		const auto prose              = std::string{ "Four places want a second look." };
		const auto block              = std::string{ "```bpr-findings\n{\"findings\":[{\"path\":\"src/engine/renderer.cpp\",\"startLine\":11,\"claim\":\"Finding at line 11.\"}]}\n```" };
		const auto answer             = prose + "\n\n" + block;
		auto       identity           = application::agent_invocation_projection_identity{ };
		identity.ApplicationSession   = core::application_session_id{ 1U };
		identity.Session.Id           = core::agent_session_id{ 9U };
		identity.InvocationGeneration = 1U;
		window.agent_panel()->admit(identity);
		window.agent_panel()->start(identity, application::prepared_workspace{ .Path = "/tmp/workspace" });
		window.agent_panel()->stream(identity, application::agent_stream_event{ .Session = identity.Session.Id, .Sequence = 1U, .Text = answer });
		auto delivery    = application::agent_finding_delivery{ };
		delivery.Session = identity.Session.Id;
		for (const auto line : { 10U, 20U, 30U, 40U }) {
			delivery.Findings.push_back(application::agent_delivered_finding{ .Finding = finding_at(line), .Verdict = { } });
		}
		window.finding_queue()->present(delivery);
		window.agent_panel()->finish(identity, application::agent_run_result{ .Session = identity.Session.Id, .Outcome = application::agent_run_outcome::success });
		settle();
		expect(agent->turn_count() == 1U && agent->turn_cards(0U).size() == 4U, "the review's four findings are four cards under its answer");
		expect(agent->turn_answer(0U) == QString::fromStdString(prose), "the answer shows without its findings block");

		// Add the second one as a draft; the host says it was drafted.
		auto asked = std::vector<qt::agent_finding_card_request>{ };
		agent->set_finding_card_dispatch([&asked](const qt::agent_finding_card_request& aRequest) { asked.push_back(aRequest); });
		agent->turn_cards(0U)[1]->findChild<QPushButton*>(QStringLiteral("agentFindingDraftComment"))->click();
		settle();
		expect(asked.size() == 1U && asked.back().Ordinal == 1U, "Add as draft comment asks the host for that finding");
		agent->present_finding_card_state(asked.back().Session, asked.back().Ordinal, qt::agent_finding_card_state::drafted);
		settle();
		expect_three_open_one_drafted(*agent, 0U, "once one is added");

		// After a while: the host reads the native history back and shows the
		// same thread again, the answer as the agent wrote it, block and all.
		const auto native = std::vector<qt::agent_thread_turn>{
			{ .Question = QStringLiteral("Review this pull request using the review skill."), .Answer = QString::fromStdString(answer), .Footer = QStringLiteral("success") },
		};
		agent->present_thread(native);
		settle();
		expect(agent->turn_count() == 1U, "the thread shown again has its one turn");
		expect_three_open_one_drafted(*agent, 0U, "after the native history is shown again");

		// The cards made again still act, and still report to the same place.
		agent->turn_cards(0U)[2]->findChild<QPushButton*>(QStringLiteral("agentFindingDraftComment"))->click();
		settle();
		expect(asked.size() == 2U && asked.back().Ordinal == 2U && asked.back().Session == identity.Session.Id.value() && asked.back().Finding == finding_at(30U),
			   "a card made again asks for its own finding");
		agent->present_finding_card_state(asked.back().Session, asked.back().Ordinal, qt::agent_finding_card_state::open);
		settle();

		// The kept thread the observer saved (its answer without the block)
		// shows them too.
		agent->present_thread(agent->thread_turns());
		settle();
		expect_three_open_one_drafted(*agent, 0U, "after the kept thread is shown again");

		// Another pull request's thread, then this one again.
		agent->present_thread({ { .Question = QStringLiteral("Something else?"), .Answer = QStringLiteral("Another answer."), .Footer = QStringLiteral("success") } });
		settle();
		expect(agent->turn_cards(0U).empty() && agent->finding_cards().empty(), "another thread shows none of these cards");
		agent->present_thread({ });
		settle();
		agent->present_thread(native);
		settle();
		expect_three_open_one_drafted(*agent, 0U, "after another thread and back");

		// A later turn under the same thread leaves them where they are.
		agent->present_thread({ native.front(), { .Question = QStringLiteral("And now?"), .Answer = QStringLiteral("Nothing more."), .Footer = QStringLiteral("success") } });
		settle();
		expect(agent->turn_count() == 2U && agent->turn_cards(1U).empty(), "a later turn has no cards of its own");
		expect_three_open_one_drafted(*agent, 0U, "with a later turn after it");
		agent->set_finding_card_dispatch({ });
		window.hide();
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_unadded_suggestions_survive_the_thread_shown_again();
	std::cout << "agent-finding-cards-retained:pass\n";
	return EXIT_SUCCESS;
}
