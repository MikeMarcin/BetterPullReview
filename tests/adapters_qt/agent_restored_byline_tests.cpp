/// \file
/// \brief [[BPR-2241]]. A restored report's byline says how its turn ended,
/// as the live turn's did, not the outcome the host stored ("success").
///
/// \details A native session's turn is stored by the host with its outcome's
/// name for an ending and how long the run took (`agent_invocation_host`,
/// `review_agent_runtime`). These checks settle a successful, a stopped and a
/// failed turn live and open each one's report; then, in a fresh window, the
/// same three turns are restored as the host stores them and the window maps
/// them, and each report's byline begins with the live turn's ending. A turn
/// stored with the line it settled with keeps that line.

#include "bpr/application/agent_launch.hpp"
#include "bpr/application/agent_workspace.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_report_view.hpp"
#include "bpr/qt/agent_run_status.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QApplication>
#include <QComboBox>
#include <QTabWidget>

#include <array>
#include <cstdlib>
#include <iostream>
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
		aWindow.tray_tabs()->setCurrentIndex(2);
		auto profile        = core::agent_profile{ };
		profile.Id          = core::claude_stream_json_profile_id;
		profile.DisplayName = "Claude Code";
		profile.Executable  = "/usr/bin/true";
		profile.Protocol    = core::agent_protocol::claude_stream_json;
		aWindow.agent_panel()->offer_profiles(std::vector{ profile });
		settle();
		auto* const selector = aWindow.agent_panel()->profile_selector();
		selector->setCurrentIndex(selector->findData(QVariant::fromValue<qulonglong>(profile.Id.value())));
		auto offered = std::string{ };
		for (int index = 0; index < selector->count(); ++index) {
			offered += selector->itemText(index).toStdString() + "; ";
		}
		expect(selector->currentText() == QStringLiteral("Claude Code"), "Claude Code is the agent asked, of: " + offered);
		settle();
		aWindow.agent_tab()->refresh();
		settle();
	}

	/// The byline's ending of turn \p aTurn's report, as its card's Open hands it over.
	[[nodiscard]] auto report_ending(qt::agent_tab_view& aTab, const std::size_t aTurn) -> QString
	{
		auto ending = std::optional<QString>{ };
		aTab.set_report_dispatch([&ending](const qt::agent_report_document& aReport) { ending = aReport.Byline.Ending; });
		aTab.open_report(aTurn);
		aTab.set_report_dispatch({ });
		expect(ending.has_value(), "turn " + std::to_string(aTurn) + "'s report opens");
		return *ending;
	}

	struct ended_turn
	{
		application::agent_run_outcome Outcome;
		core::agent_turn_outcome       Kept;
		const char*                    Said;
	};

	constexpr auto endings = std::array{
		ended_turn{ application::agent_run_outcome::success, core::agent_turn_outcome::answered, "a successful turn" },
		ended_turn{ application::agent_run_outcome::cancelled, core::agent_turn_outcome::ended_without_answer, "a stopped turn" },
		ended_turn{ application::agent_run_outcome::nonzero_exit, core::agent_turn_outcome::ended_without_answer, "a failed turn" },
	};

	void test_a_restored_turns_byline_says_how_it_ended()
	{
		// Live: each turn settles, and its report's byline is read.
		auto live = std::vector<QString>{ };
		{
			qt::workbench_window window;
			open_review(window);
			auto* const tab        = window.agent_tab();
			auto        generation = std::uint64_t{ 0U };
			for (const auto& ended : endings) {
				auto identity                 = application::agent_invocation_projection_identity{ };
				identity.ApplicationSession   = core::application_session_id{ 1U };
				identity.Session.Id           = core::agent_session_id{ 9U + generation };
				identity.InvocationGeneration = ++generation;
				window.agent_panel()->admit(identity);
				window.agent_panel()->start(identity, application::prepared_workspace{ .Path = "/tmp/workspace" });
				window.agent_panel()->stream(identity, application::agent_stream_event{ .Session = identity.Session.Id, .Sequence = 1U, .Text = "The loop ends." });
				window.agent_panel()->finish(identity, application::agent_run_result{ .Session = identity.Session.Id, .Outcome = ended.Outcome });
				settle();
				expect(tab->turn_count() == generation, std::string{ ended.Said } + " settles as a turn");
				live.push_back(report_ending(*tab, tab->turn_count() - 1U));
			}
			window.hide();
		}
		expect(live[0] == qt::agent_run_answered_words(QStringLiteral("Claude"), 0) && live[1] == qt::agent_run_stopped_words(0) && live[2] == qt::agent_run_stopped_words(0),
			   "the live endings: " + live[0].toStdString() + " | " + live[1].toStdString() + " | " + live[2].toStdString());

		// Restored: as the host stores a native turn (its outcome's name and
		// how long it took) and the window maps it.
		{
			qt::workbench_window window;
			open_review(window);
			auto* const tab    = window.agent_tab();
			auto        thread = std::vector<qt::agent_thread_turn>{ };
			for (const auto& ended : endings) {
				thread.push_back({ .Question = QStringLiteral("Does this work?"),
								   .Answer   = QStringLiteral("The loop ends."),
								   .Footer   = QString::fromUtf8(application::name(ended.Outcome)),
								   .Outcome  = ended.Kept,
								   .Seconds  = 0 });
			}
			tab->present_thread(thread);
			settle();
			expect(tab->turn_count() == endings.size(), "the three turns are restored");
			for (std::size_t index = 0U; index < endings.size(); ++index) {
				const auto restored = report_ending(*tab, index);
				expect(restored == live[index], std::string{ "the restored report's byline begins as the live one did for " } + endings[index].Said + ": " + restored.toStdString());
				expect(restored != QString::fromUtf8(application::name(endings[index].Outcome)), "not with the outcome's name");
			}

			// A longer run says its duration; one stored without it says
			// only how it ended; one stored with its line keeps the line.
			thread = { { .Question = QStringLiteral("Why?"), .Answer = QStringLiteral("Because."), .Footer = QStringLiteral("success"), .Outcome = core::agent_turn_outcome::answered, .Seconds = 72 },
					   { .Question = QStringLiteral("And?"), .Answer = QStringLiteral("So."), .Footer = QStringLiteral("success"), .Outcome = core::agent_turn_outcome::answered },
					   { .Question = QStringLiteral("Then?"), .Answer = QStringLiteral("Done."), .Footer = QStringLiteral("Claude answered in 0:12 · $0.41"), .Outcome = core::agent_turn_outcome::answered } };
			tab->present_thread(thread);
			settle();
			expect(report_ending(*tab, 0U) == QStringLiteral("Claude answered in 1:12"), "a stored duration is said: " + report_ending(*tab, 0U).toStdString());
			expect(report_ending(*tab, 1U) == QStringLiteral("Claude answered"), "without one, how it ended: " + report_ending(*tab, 1U).toStdString());
			expect(report_ending(*tab, 2U) == QStringLiteral("Claude answered in 0:12"), "a stored line is kept: " + report_ending(*tab, 2U).toStdString());
			window.hide();
		}
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_a_restored_turns_byline_says_how_it_ended();
	std::cout << "agent-restored-byline:pass\n";
	return EXIT_SUCCESS;
}
