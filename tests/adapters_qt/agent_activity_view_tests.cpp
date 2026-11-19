/// \file
/// \brief [[BPR-2005]], RFC-BPR-0103. The running block: what the agent is
/// doing and when it last did anything, a row per helper, the quiet notice and
/// its two choices, and the folded line after the turn.

#include "bpr/qt/agent_footer.hpp"
#include "bpr/qt/agent_run_status.hpp"

#include <QApplication>
#include <QPushButton>
#include <QToolButton>

#include <cstdlib>
#include <iostream>
#include <string>

namespace {
	using namespace bpr;

	void expect(const bool aPass, const std::string& aMessage)
	{
		if (!aPass) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto update(const core::agent_activity_kind aKind, std::string aHelper, std::string aSubject, const core::agent_doing aDoing = core::agent_doing::thinking) -> core::agent_activity_update
	{
		auto value    = core::agent_activity_update{ };
		value.Kind    = aKind;
		value.Helper  = std::move(aHelper);
		value.Subject = std::move(aSubject);
		value.Doing   = aDoing;
		return value;
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	auto         now = qint64{ 1'000'000 };
	qt::set_agent_activity_clock([&now] { return now; });
	const auto started = now;
	using kind         = core::agent_activity_kind;

	// AC06. The line names the file being read, and "last activity" restarts
	// on every frame.
	auto turn              = core::agent_turn_activity{ };
	turn                   = core::apply_agent_activity(turn, update(kind::doing, { }, "/work/src/frame_pool.cpp", core::agent_doing::reading), 1'000U);
	auto live              = qt::agent_run_status{ };
	live.Agent             = QStringLiteral("Claude");
	live.Activity          = turn;
	live.ActivityStartedMs = started;
	now                    = started + 5'000;
	expect(qt::agent_run_status_words(live, 5) == QStringLiteral("● Claude is reading frame_pool.cpp, 0:05 · last activity 4 s ago."), "the file being read, and the last sign of life: " + qt::agent_run_status_words(live, 5).toStdString());
	turn          = core::note_agent_frame(turn, 5'000U);
	live.Activity = turn;
	expect(qt::agent_run_status_words(live, 5).endsWith(QStringLiteral("last activity 0 s ago.")), "a frame restarts it");
	turn          = core::apply_agent_activity(turn, update(kind::doing, { }, "git diff --stat", core::agent_doing::running), 5'000U);
	live.Activity = turn;
	expect(qt::agent_run_status_words(live, 5).contains(QStringLiteral("is running git diff --stat")), "the command being run");
	// [[BPR-2252]]. The turn's cost so far follows its time once reported;
	// none is said before, nor while the workspace is made.
	live.Cost = core::agent_cost{ 4'000'000U };
	expect(qt::agent_run_status_words(live, 5) == QStringLiteral("● Claude is running git diff --stat, 0:05 · < $0.01 · last activity 0 s ago."), "a sub-cent cost after the time: " + qt::agent_run_status_words(live, 5).toStdString());
	{
		auto bare     = qt::agent_run_status{ };
		bare.Agent    = QStringLiteral("Claude");
		bare.Cost     = core::agent_cost{ 1'840'000'000U };
		const auto at = qt::agent_run_status_words(bare, 72);
		expect(at == QStringLiteral("● Claude is thinking, 1:12 · $1.84."), "the thinking line with its cost: " + at.toStdString());
		bare.Stage = qt::agent_run_stage::preparing;
		expect(!qt::agent_run_status_words(bare, 3).contains(QLatin1Char{ '$' }), "no cost while the workspace is made");
		// [[BPR-2261]], AC5. Making is said only while a workspace is made;
		// the retained one is checked.
		expect(qt::agent_run_status_words(bare, 3) == QStringLiteral("● Claude is making its workspace at the head, 0:03."), "a workspace being made");
		bare.Stage = qt::agent_run_stage::checking;
		expect(qt::agent_run_status_words(bare, 3) == QStringLiteral("● Claude is checking its workspace at the head, 0:03."), "the retained workspace being checked: " + qt::agent_run_status_words(bare, 3).toStdString());
		expect(qt::agent_footer_doing_words(bare) == QStringLiteral("checking its workspace"), "and the footer says the same");
		// [[BPR-2261]]. A turn the app's closing stopped says so on reload.
		expect(qt::agent_turn_ending_words(QStringLiteral("Claude Code"), core::agent_turn_outcome::closed_with_app, 1006) == QStringLiteral("Stopped when the app closed, after 16:46"), "a turn stopped by the app closing");
		expect(qt::agent_turn_ending_words(QStringLiteral("Claude Code"), core::agent_turn_outcome::answered, 1006) == QStringLiteral("Claude answered in 16:46"), "an answered turn, reloaded");
		bare.Cost.reset();
		bare.Stage = qt::agent_run_stage::thinking;
		expect(qt::agent_run_status_words(bare, 72) == QStringLiteral("● Claude is thinking, 1:12."), "no cost reported, none said");
	}
	live.Cost.reset();

	// AC02's rows: three helpers, one running, one finished, one failed.
	turn         = core::apply_agent_activity(turn, update(kind::helper_started, "a", "Rendering path"), 6'000U);
	turn         = core::apply_agent_activity(turn, update(kind::helper_started, "b", "Tests"), 6'000U);
	turn         = core::apply_agent_activity(turn, update(kind::helper_started, "c", "Public interface"), 6'000U);
	turn         = core::apply_agent_activity(turn, update(kind::helper_progress, "a", "Reading frame_pool.cpp", core::agent_doing::reading), 7'000U);
	turn         = core::apply_agent_activity(turn, update(kind::helper_ended, "b", "2 findings"), 9'000U);
	auto failed  = update(kind::helper_ended, "c", { });
	failed.Ended = core::agent_helper_state::failed;
	turn         = core::apply_agent_activity(turn, failed, 9'500U);
	turn         = core::apply_agent_activity(turn, update(kind::doing, { }, { }, core::agent_doing::waiting), 10'000U);
	now          = started + 12'000;
	auto view    = qt::agent_activity_view{ };
	auto stopped = 0;
	view.set_stop([&stopped] { ++stopped; });
	view.resize(420, 200);
	view.present(QStringLiteral("Claude"), turn, started);
	view.show();
	expect(view.live() && view.helper_rows() == 3, "a row for each helper");
	expect(view.helper_row_text(0) == QStringLiteral("Rendering path\tReading frame_pool.cpp\t0:06"), "its description, what it last did, its time: " + view.helper_row_text(0).toStdString());
	expect(view.helper_row_text(1) == QStringLiteral("Tests\tfinished, 2 findings\t0:03"), "a finished helper: " + view.helper_row_text(1).toStdString());
	expect(view.helper_row_text(2).startsWith(QStringLiteral("Public interface\tfailed")), "a failed one says so");
	live.Activity = turn;
	expect(qt::agent_run_status_words(live, 12).contains(QStringLiteral("is waiting on 1 helper")), "waiting on its helper");
	expect(!view.quiet_shown(), "no notice while frames come");

	// AC05. Two minutes without a frame: the notice, with both choices.
	now = started + 10'000 + 119'000;
	view.tick();
	expect(!view.quiet_shown(), "not before two minutes");
	now = started + 10'000 + 120'000;
	view.tick();
	expect(view.quiet_shown() && view.quiet_text().startsWith(QStringLiteral("No activity for 2 min")), "the notice after two minutes: " + view.quiet_text().toStdString());
	expect(view.quiet_text().contains(QStringLiteral("since it started “Rendering path”")), "it says what the agent was last waiting on");
	if (const auto folder = qEnvironmentVariable("BPR_ACTIVITY_RENDER"); !folder.isEmpty()) {
		QApplication::processEvents();
		static_cast<void>(view.grab().save(folder + QStringLiteral("/agent-activity-quiet.png")));
	}
	expect(view.stop_button()->text() == QStringLiteral("Stop and keep what arrived") && view.keep_waiting_button()->text() == QStringLiteral("Keep waiting"), "both choices");
	view.keep_waiting_button()->click();
	expect(!view.quiet_shown() && stopped == 0 && view.live(), "Keep waiting leaves the run untouched and puts the notice away");
	now += 119'000;
	view.tick();
	expect(!view.quiet_shown(), "for two minutes");
	now += 1'000;
	view.tick();
	expect(view.quiet_shown(), "then it returns");
	turn = core::note_agent_frame(turn, static_cast<std::uint64_t>(now - started));
	view.present(QStringLiteral("Claude"), turn, started);
	expect(!view.quiet_shown(), "a frame clears it");
	now += 120'000;
	view.tick();
	view.stop_button()->click();
	expect(stopped == 1, "Stop and keep what arrived stops the run");

	// After the turn: one folded line that opens to the same rows.
	turn = core::apply_agent_activity(turn, update(kind::helper_ended, "a", { }), static_cast<std::uint64_t>(now - started));
	view.fold(turn, 423);
	expect(!view.live() && !view.quiet_shown(), "no notice once it ended");
	expect(view.summary_text() == QStringLiteral("Worked for 7 min: 3 helpers, 2 files read, 1 command"), "the folded line: " + view.summary_text().toStdString());
	expect(!view.rows_open(), "closed");
	view.summary_button()->click();
	expect(view.rows_open() && view.helper_rows() == 3, "it opens to the rows");
	auto quiet = qt::agent_activity_view{ };
	quiet.fold(core::agent_turn_activity{ }, 3);
	expect(quiet.isHidden(), "a turn that reported no work folds to nothing");
	qt::set_agent_activity_clock({ });
	std::cout << "agent-activity-view:pass\n";
	return EXIT_SUCCESS;
}
