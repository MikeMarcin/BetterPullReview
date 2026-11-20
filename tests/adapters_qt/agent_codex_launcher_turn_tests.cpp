/// \file
/// \brief [[BPR-2305]]. A Codex turn through a sandbox launcher, in the
/// Agent tab: its configuration warnings fold with the turn's activity, and a
/// turn whose file changes all failed ends with words naming them instead of
/// a normal answer. One that wrote a path ends as its answer.

#include "bpr/application/agent_launch.hpp"
#include "bpr/application/agent_workspace.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_run_status.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QApplication>
#include <QComboBox>
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
		aWindow.tray_tabs()->setCurrentIndex(2);
		auto profile        = core::agent_profile{ };
		profile.Id          = core::codex_jsonl_profile_id;
		profile.DisplayName = "Codex";
		profile.Executable  = "/usr/bin/true";
		profile.Protocol    = core::agent_protocol::codex_jsonl_exec;
		aWindow.agent_panel()->offer_profiles(std::vector{ profile });
		settle();
		auto* const selector = aWindow.agent_panel()->profile_selector();
		selector->setCurrentIndex(selector->findData(QVariant::fromValue<qulonglong>(profile.Id.value())));
		expect(selector->currentData().toULongLong() == profile.Id.value(), "Codex is the agent asked: " + selector->currentText().toStdString());
		settle();
		aWindow.agent_tab()->refresh();
		settle();
	}

	/// One turn that streams its answer and ends with `aResult`.
	void run_turn(qt::workbench_window& aWindow, const std::uint64_t aGeneration, application::agent_run_result aResult)
	{
		auto identity                 = application::agent_invocation_projection_identity{ };
		identity.ApplicationSession   = core::application_session_id{ 1U };
		identity.Session.Id           = core::agent_session_id{ 20U + aGeneration };
		identity.InvocationGeneration = aGeneration;
		aWindow.agent_panel()->admit(identity);
		aWindow.agent_panel()->start(identity, application::prepared_workspace{ .Path = "/tmp/workspace" });
		aWindow.agent_panel()->stream(identity, application::agent_stream_event{ .Session = identity.Session.Id, .Sequence = 1U, .Text = "I wrote the notes." });
		aResult.Session = identity.Session.Id;
		aResult.Outcome = application::agent_run_outcome::success;
		aWindow.agent_panel()->finish(identity, aResult);
		settle();
	}

	void test_a_launcher_turn_says_what_failed_and_what_it_warned()
	{
		qt::workbench_window window;
		open_review(window);
		auto* const tab = window.agent_tab();

		auto failed                = application::agent_run_result{ };
		failed.HarnessWarnings     = { "Codex is ignoring 1 unrecognized configuration setting: `retired_option`.", "A second warning." };
		failed.HarnessWarningCount = 2U;
		failed.FailedWrites        = { "/work/scratch/notes.txt", "/work/scratch/plan.md" };
		failed.FailedWriteCount    = 2U;
		run_turn(window, 1U, failed);
		expect(tab->turn_count() == 1U, "the turn settles");
		const auto answer = tab->turn_answer(0U);
		expect(answer.contains(QStringLiteral("I wrote the notes.")) && answer.contains(QStringLiteral("every change failed, so nothing was written"))
				   && answer.contains(QStringLiteral("/work/scratch/notes.txt")) && answer.contains(QStringLiteral("/work/scratch/plan.md")),
			   "a turn whose file changes all failed names them after what it said: " + answer.toStdString());
		auto* const activity = tab->turn_activity(0U);
		expect(activity != nullptr && activity->summary_text().contains(QStringLiteral("2 warnings")), "its warnings fold with the turn's activity");

		auto wrote                = application::agent_run_result{ };
		wrote.FailedWrites        = { "/work/scratch/a.txt" };
		wrote.FailedWriteCount    = 1U;
		wrote.CompletedWriteCount = 1U;
		run_turn(window, 2U, wrote);
		expect(tab->turn_count() == 2U && !tab->turn_answer(1U).contains(QStringLiteral("nothing was written")), "a turn that wrote a path ends as its answer");
		window.hide();
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	auto application = QApplication{ argc, argv };
	test_a_launcher_turn_says_what_failed_and_what_it_warned();
	std::cout << "agent-codex-launcher-turn:pass\n";
	return EXIT_SUCCESS;
}
