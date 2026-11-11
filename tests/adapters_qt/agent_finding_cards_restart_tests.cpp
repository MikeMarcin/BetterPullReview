/// \file
/// \brief Owner report 2026-11-10, after BPR-2052: the finding cards of a
/// review answer came back while the application ran, but after a restart
/// they were gone.
///
/// \details The stored answer keeps its `bpr-findings` block, and the card
/// host keeps what was done with each card in the profile's agent setup
/// file. These checks run one review turn with four findings, add one as a
/// draft, then "restart": a fresh window, a fresh host and a fresh store over
/// the same file. The native history's answer and the kept thread's each
/// make the four cards again: one says Drafted, once, and the three others
/// still offer Add as draft comment; one of those still drafts its own
/// finding, and that is kept past the next restart too. A refusal comes back
/// with its reason.

#include "bpr/adapters/sqlite/agent_setup_store.hpp"
#include "bpr/application/agent_finding_card_host.hpp"
#include "bpr/application/agent_finding_delivery.hpp"
#include "bpr/application/agent_invocation_host.hpp"
#include "bpr/application/agent_launch.hpp"
#include "bpr/application/agent_workspace.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/agent_finding_card_memory.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/finding_queue_view.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QTemporaryDir>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
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

	/// The review answer as the agent wrote it, and as the native history
	/// keeps it: prose, then a block with four findings.
	[[nodiscard]] auto review_answer() -> std::string
	{
		auto block = std::string{ "```bpr-findings\n{\"findings\":[" };
		for (const auto line : { 11, 21, 31, 41 }) {
			if (line != 11) {
				block += ',';
			}
			block += "{\"path\":\"src/engine/renderer.cpp\",\"startLine\":" + std::to_string(line) + ",\"endLine\":" + std::to_string(line) + ",\"claim\":\"Finding at line "
				+ std::to_string(line) + ".\",\"suggestedComment\":\"suggestion: look at line " + std::to_string(line) + "\"}";
		}
		block += "]}\n```";
		return "Four places want a second look.\n\n" + block;
	}

	/// \brief What the composition does: a profile's card host over the
	/// agent setup file in \p aRoot, on one manual lane.
	struct card_keeping
	{
		explicit card_keeping(const std::filesystem::path& aRoot)
			: Store{ adapters::sqlite::make_agent_finding_card_store(aRoot) }
		{
			auto dependencies     = application::agent_finding_card_dependencies{ };
			dependencies.Store    = Store.get();
			dependencies.Effects  = &Lane;
			dependencies.Decision = &Lane;
			Host                  = std::make_unique<application::agent_finding_card_host>(std::move(dependencies));
			Host->load();
			Lane.drain();
		}
		card_keeping(const card_keeping&)                    = delete;
		auto operator=(const card_keeping&) -> card_keeping& = delete;
		~card_keeping()
		{
			Lane.drain();
			Host->shutdown();
		}

		/// The cards the stored \p aAnswer makes, as the composition gives them.
		[[nodiscard]] auto cards(const std::string& aAnswer) const -> std::vector<qt::agent_restored_finding_card>
		{
			auto       result   = std::vector<qt::agent_restored_finding_card>{ };
			const auto restored = application::restore_agent_findings(aAnswer, Host->memory());
			if (!restored) {
				return result;
			}
			for (const auto& card : restored->Cards) {
				auto state = qt::agent_finding_card_state::open;
				switch (card.Kept.State) {
				case core::agent_finding_card_disposition::open:      state = qt::agent_finding_card_state::open; break;
				case core::agent_finding_card_disposition::drafted:   state = qt::agent_finding_card_state::drafted; break;
				case core::agent_finding_card_disposition::noted:     state = qt::agent_finding_card_state::noted; break;
				case core::agent_finding_card_disposition::dismissed: state = qt::agent_finding_card_state::dismissed; break;
				case core::agent_finding_card_disposition::refused:   state = qt::agent_finding_card_state::refused; break;
				}
				result.push_back({ .Session = restored->Session, .Ordinal = card.Ordinal, .Finding = card.Finding, .State = state, .Why = QString::fromStdString(card.Kept.Why), .Note = card.Kept.Note });
			}
			return result;
		}

		/// Where the tab says each card's state, as the composition keeps it.
		void observe(qt::agent_tab_view& aTab)
		{
			aTab.set_finding_card_observer([this](const qt::agent_finding_card_change& aChange) {
				auto state = core::agent_finding_card_disposition::open;
				switch (aChange.State) {
				case qt::agent_finding_card_state::open:      state = core::agent_finding_card_disposition::open; break;
				case qt::agent_finding_card_state::working:   return;
				case qt::agent_finding_card_state::drafted:   state = core::agent_finding_card_disposition::drafted; break;
				case qt::agent_finding_card_state::noted:     state = core::agent_finding_card_disposition::noted; break;
				case qt::agent_finding_card_state::dismissed: state = core::agent_finding_card_disposition::dismissed; break;
				case qt::agent_finding_card_state::refused:   state = core::agent_finding_card_disposition::refused; break;
				}
				Host->remember({ .Findings = core::digest_agent_findings(aChange.FindingsBlock),
								 .Ordinal  = static_cast<std::uint32_t>(aChange.Ordinal),
								 .State    = state,
								 .Why      = aChange.Why.toStdString(),
								 .Note     = aChange.Note });
				Lane.drain();
			});
		}

		std::unique_ptr<application::agent_finding_card_store_port> Store;
		application::manual_executor                                Lane;
		std::unique_ptr<application::agent_finding_card_host>       Host;
	};

	[[nodiscard]] auto state_of(const QWidget* aCard) -> qt::agent_finding_card_state
	{
		return static_cast<qt::agent_finding_card_state>(aCard->property("agentFindingStateName").toInt());
	}

	struct card_count
	{
		std::size_t Open    = 0U;
		std::size_t Drafted = 0U;
		std::size_t Refused = 0U;
		std::size_t Other   = 0U;
		/// Every open card still offers Add as draft comment.
		bool Offered = true;
		/// No drafted card offers it.
		bool DraftedClosed = true;
	};

	[[nodiscard]] auto count_cards(const qt::agent_tab_view& aTab, const std::size_t aTurn) -> card_count
	{
		auto counted = card_count{ };
		for (auto* const card : aTab.turn_cards(aTurn)) {
			const auto* const draft   = card->findChild<QPushButton*>(QStringLiteral("agentFindingDraftComment"));
			const auto        offered = draft != nullptr && !draft->isHidden();
			switch (state_of(card)) {
			case qt::agent_finding_card_state::open:
				++counted.Open;
				counted.Offered = counted.Offered && offered;
				break;
			case qt::agent_finding_card_state::drafted:
				++counted.Drafted;
				counted.DraftedClosed = counted.DraftedClosed && !offered;
				break;
			case qt::agent_finding_card_state::refused: ++counted.Refused; break;
			default:                                    ++counted.Other; break;
			}
		}
		return counted;
	}

	void expect_cards(const qt::agent_tab_view& aTab, const std::size_t aOpen, const std::size_t aDrafted, const std::size_t aRefused, const std::string& aWhen)
	{
		const auto counted = count_cards(aTab, 0U);
		expect(aTab.turn_cards(0U).size() == 4U, aWhen + ": the turn has its four cards, not " + std::to_string(aTab.turn_cards(0U).size()));
		expect(counted.Open == aOpen && counted.Drafted == aDrafted && counted.Refused == aRefused && counted.Other == 0U,
			   aWhen + ": open " + std::to_string(counted.Open) + ", drafted " + std::to_string(counted.Drafted) + ", refused " + std::to_string(counted.Refused) + ", other "
				   + std::to_string(counted.Other));
		expect(counted.Offered && counted.DraftedClosed, aWhen + ": every open card offers Add as draft comment and no drafted one does");
	}

	[[nodiscard]] auto native_thread(const std::string& aAnswer, std::vector<qt::agent_restored_finding_card> aCards) -> std::vector<qt::agent_thread_turn>
	{
		return { { .Question = QStringLiteral("Review this pull request using the review skill."),
				   .Answer   = QString::fromStdString(aAnswer),
				   .Footer   = QStringLiteral("success"),
				   .Cards    = std::move(aCards) } };
	}

	void test_cards_and_their_states_survive_a_restart()
	{
		QTemporaryDir root;
		expect(root.isValid(), "a private state root");
		const auto path   = std::filesystem::path{ root.path().toStdString() };
		const auto answer = review_answer();
		auto       kept   = std::vector<qt::agent_thread_turn>{ };
		auto       asked  = std::vector<qt::agent_finding_card_request>{ };
		const auto ask    = [&asked](const qt::agent_finding_card_request& aRequest) { asked.push_back(aRequest); };

		// The first run: the review turn, its four findings delivered live,
		// and the second added as a draft.
		{
			auto                 keeping = card_keeping{ path };
			qt::workbench_window window;
			open_review(window);
			auto* const agent = window.agent_tab();
			keeping.observe(*agent);
			agent->set_finding_card_dispatch(ask);
			const auto restored = application::restore_agent_findings(answer, core::agent_finding_card_memory{ });
			expect(restored && restored->Cards.size() == 4U, "the stored answer's block reads as four findings");

			auto identity                 = application::agent_invocation_projection_identity{ };
			identity.ApplicationSession   = core::application_session_id{ 1U };
			identity.Session.Id           = core::agent_session_id{ 9U };
			identity.InvocationGeneration = 1U;
			window.agent_panel()->admit(identity);
			window.agent_panel()->start(identity, application::prepared_workspace{ .Path = "/tmp/workspace" });
			window.agent_panel()->stream(identity, application::agent_stream_event{ .Session = identity.Session.Id, .Sequence = 1U, .Text = answer });
			auto delivery    = application::agent_finding_delivery{ };
			delivery.Session = identity.Session.Id;
			for (const auto& card : restored->Cards) {
				delivery.Findings.push_back(application::agent_delivered_finding{ .Finding = card.Finding, .Verdict = { } });
			}
			window.finding_queue()->present(delivery);
			window.agent_panel()->finish(identity, application::agent_run_result{ .Session = identity.Session.Id, .Outcome = application::agent_run_outcome::success });
			settle();
			expect(agent->turn_count() == 1U && agent->turn_cards(0U).size() == 4U, "the review's four findings are four cards under its answer");

			agent->turn_cards(0U)[1]->findChild<QPushButton*>(QStringLiteral("agentFindingDraftComment"))->click();
			settle();
			expect(asked.size() == 1U && asked.back().Ordinal == 1U, "Add as draft comment asks the host for the second finding");
			agent->present_finding_card_state(asked.back().Session, asked.back().Ordinal, qt::agent_finding_card_state::drafted);
			settle();
			expect_cards(*agent, 3U, 1U, 0U, "once one is added");
			expect(keeping.Host->memory().size() == 1U, "the drafted card is kept, and nothing else");
			kept = agent->thread_turns();
			agent->set_finding_card_dispatch({ });
			agent->set_finding_card_observer({ });
			window.hide();
		}

		// A restart: the native history's answer, block and all, makes the
		// cards again as they were left.
		{
			auto                 keeping = card_keeping{ path };
			qt::workbench_window window;
			open_review(window);
			auto* const agent = window.agent_tab();
			keeping.observe(*agent);
			agent->set_finding_card_dispatch(ask);
			expect(keeping.Host->is_loaded() && keeping.Host->memory().size() == 1U, "the fresh host reads the one kept card back");
			agent->present_thread(native_thread(answer, keeping.cards(answer)));
			settle();
			expect(agent->turn_count() == 1U && agent->turn_answer(0U) == QStringLiteral("Four places want a second look."), "the restored turn shows its answer without the block");
			expect_cards(*agent, 3U, 1U, 0U, "after a restart");
			expect(state_of(agent->turn_cards(0U)[1]) == qt::agent_finding_card_state::drafted, "the drafted one is the one that was added");

			// A restored open card drafts its own finding.
			asked.clear();
			agent->turn_cards(0U)[2]->findChild<QPushButton*>(QStringLiteral("agentFindingDraftComment"))->click();
			settle();
			expect(asked.size() == 1U && asked.back().Ordinal == 2U && core::is_restored_finding_session(asked.back().Session), "a restored card asks for its own place, under its restored delivery");
			const auto& finding = asked.back().Finding;
			expect(finding.Path == "src/engine/renderer.cpp" && finding.Lines.Begin == 30U && finding.Claim == "Finding at line 31." && finding.SuggestedComment == std::optional<std::string>{ "suggestion: look at line 31" }
					   && finding.Anchor.Line && finding.Anchor.Line->Side == core::diff_side::right,
				   "a restored card asks for its own finding, on the side its block named");
			expect(state_of(agent->turn_cards(0U)[2]) == qt::agent_finding_card_state::working, "the card works while the host does");
			agent->present_finding_card_state(asked.back().Session, asked.back().Ordinal, qt::agent_finding_card_state::drafted);
			// The last one is refused, with its reason.
			agent->present_finding_card_state(asked.back().Session, 3U, qt::agent_finding_card_state::refused, QStringLiteral("Not drafted: resolve the draft open at that line first."));
			settle();
			expect_cards(*agent, 1U, 2U, 1U, "after a restored card is added and another refused");
			agent->set_finding_card_dispatch({ });
			agent->set_finding_card_observer({ });
			window.hide();
		}

		// Another restart, through the kept thread this time (its answer keeps
		// the block for this), and through the native history.
		{
			auto                 keeping = card_keeping{ path };
			qt::workbench_window window;
			open_review(window);
			auto* const agent = window.agent_tab();
			expect(kept.size() == 1U && kept.front().Answer.contains(QStringLiteral("```bpr-findings")), "the kept thread keeps the findings block");
			auto thread          = kept;
			thread.front().Cards = keeping.cards(kept.front().Answer.toStdString());
			agent->present_thread(thread);
			settle();
			expect(agent->turn_answer(0U) == QStringLiteral("Four places want a second look."), "the kept thread's answer shows without the block");
			expect_cards(*agent, 1U, 2U, 1U, "the kept thread after another restart");
			const auto* const why = agent->turn_cards(0U)[3]->findChild<QLabel*>(QStringLiteral("agentFindingState"));
			expect(why != nullptr && why->text() == QStringLiteral("Not drafted: resolve the draft open at that line first."), "the refused card says why, as it did");
			agent->present_thread({ });
			settle();
			agent->present_thread(native_thread(answer, keeping.cards(answer)));
			settle();
			expect_cards(*agent, 1U, 2U, 1U, "the native history after another restart");
			window.hide();
		}
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_cards_and_their_states_survive_a_restart();
	std::cout << "agent-finding-cards-restart:pass\n";
	return EXIT_SUCCESS;
}
