/// \file
/// \brief [[BPR-2005]], RFC-BPR-0103. The running block: what the agent is
/// doing and when it last did anything, a row per helper, the quiet notice and
/// its two choices, and the folded line after the turn.

#include "bpr/qt/agent_footer.hpp"
#include "bpr/qt/agent_run_status.hpp"
#include "bpr/qt/quiet_tool_button.hpp"

#include <QApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>

#include <algorithm>
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
	// In the narrowest drawer's column the two choices take a row each: side
	// by side they are wider than it, and no drawer pans sideways.
	{
		const auto wide = view.size();
		view.resize(220, view.heightForWidth(220) > 0 ? view.heightForWidth(220) : wide.height() + 60);
		QApplication::processEvents();
		const auto inside = [&view](const QPushButton* aButton) { return aButton->mapTo(&view, QPoint{ 0, 0 }).x() >= 0 && aButton->mapTo(&view, QPoint{ aButton->width(), 0 }).x() <= view.width(); };
		expect(inside(view.stop_button()) && inside(view.keep_waiting_button()) && view.stop_button()->width() >= view.stop_button()->minimumSizeHint().width()
				   && view.keep_waiting_button()->width() >= view.keep_waiting_button()->minimumSizeHint().width(),
			   "both choices lie inside a 220 px column, whole");
		expect(view.keep_waiting_button()->mapTo(&view, QPoint{ 0, 0 }).y() > view.stop_button()->mapTo(&view, QPoint{ 0, 0 }).y(), "Keep waiting wraps under Stop and keep what arrived");
		view.resize(wide);
		QApplication::processEvents();
	}
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
	// The line is the profile's quiet chip with a chevron. A plain tool button
	// with an arrow is the platform style's to draw: a grey bezel on macOS.
	expect(dynamic_cast<qt::quiet_tool_button*>(view.summary_button()) != nullptr && view.summary_button()->autoRaise() && view.summary_button()->arrowType() == Qt::RightArrow,
		   "the folded line is a quiet chip, bare until opened, with a chevron");
	expect(view.summary_button()->width() <= view.width(), "the folded line is no wider than the turn");
	expect(!view.rows_open(), "closed");
	view.summary_button()->click();
	expect(view.rows_open() && view.helper_rows() == 3, "it opens to the rows");
	expect(view.command_rows() == 1 && view.command_row_text(0) == QStringLiteral("git diff --stat") && view.command_disclosure(0) == nullptr, "the one command it ran, whole");

	// [[BPR-2285]], owner 2026-11-19: "The tool calls that show in the agents
	// tab get shortened which makes sense but there should be a disclosure to
	// see the full command." A short command shows whole, with no disclosure;
	// a long or many-line one shows its first line shortened, and a closed
	// disclosure opens to the whole of it.
	{
		const auto heredoc = std::string{ "python3 - <<'EOF'\nimport sys\nprint(sys.argv)\nEOF" };
		const auto wide    = std::string{ "rg -n --hidden --glob '!build/**' 'agent_footer_doing_words|agent_footer_state_words|present_cost' src tests docs scripts" };
		auto       ran     = core::agent_turn_activity{ };
		for (const auto& command : { std::string{ "git diff --stat" }, heredoc, wide }) {
			ran = core::apply_agent_activity(ran, update(kind::doing, { }, command, core::agent_doing::running), 1'000U);
		}
		auto commands = qt::agent_activity_view{ };
		commands.resize(420, 300);
		commands.fold(ran, 30);
		commands.show();
		commands.summary_button()->click();
		QApplication::processEvents();
		expect(commands.command_rows() == 3, "a row for each command");
		expect(commands.command_row_text(0) == QStringLiteral("git diff --stat") && commands.command_disclosure(0) == nullptr, "a short command is whole, with no disclosure");
		expect(commands.command_row_text(1) == QStringLiteral("python3 - <<'EOF' …"), "a many-line command shows its first line: " + commands.command_row_text(1).toStdString());
		expect(commands.command_row_text(2).size() == 100 && commands.command_row_text(2).endsWith(QStringLiteral("…")), "a long one is shortened: " + commands.command_row_text(2).toStdString());
		auto* const toggle = commands.command_disclosure(1);
		expect(toggle != nullptr && !toggle->isChecked() && commands.command_full(1)->isHidden() && toggle->accessibleName() == QStringLiteral("Show the full command"), "closed by default, and says what it opens");
		expect(toggle->focusPolicy() == Qt::StrongFocus, "the disclosure takes keyboard focus");
		toggle->setFocus();
		QApplication::processEvents();
		auto press   = QKeyEvent{ QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" ") };
		auto release = QKeyEvent{ QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier, QStringLiteral(" ") };
		QApplication::sendEvent(toggle, &press);
		QApplication::sendEvent(toggle, &release);
		QApplication::processEvents();
		expect(toggle->isChecked() && !commands.command_full(1)->isHidden() && commands.command_full(1)->text() == QString::fromStdString(heredoc) && toggle->accessibleName() == QStringLiteral("Hide the full command"),
			   "Space opens it to the whole command");
		commands.fold(ran, 31);
		expect(commands.command_disclosure(1)->isChecked() && !commands.command_full(1)->isHidden(), "an opened command stays open when the rows are drawn again");
		expect(!commands.command_disclosure(2)->isChecked(), "the others stay closed");
		commands.command_disclosure(1)->click();
		expect(commands.command_full(1)->isHidden() && !commands.command_disclosure(1)->isChecked(), "and it closes again");
		if (const auto folder = qEnvironmentVariable("BPR_ACTIVITY_RENDER"); !folder.isEmpty()) {
			commands.summary_button()->click();
			commands.command_disclosure(2)->click();
			commands.resize(420, 190);
			QApplication::processEvents();
			static_cast<void>(commands.grab().save(folder + QStringLiteral("/agent-activity-commands.png")));
		}
		// The live line says a command's first line, shortened.
		auto running              = qt::agent_run_status{ };
		running.Agent             = QStringLiteral("Claude");
		running.Activity          = core::apply_agent_activity({ }, update(kind::doing, { }, heredoc, core::agent_doing::running), 0U);
		running.ActivityStartedMs = now;
		expect(qt::agent_run_status_words(running, 1).contains(QStringLiteral("is running python3 - <<'EOF' …,")), "the live line: " + qt::agent_run_status_words(running, 1).toStdString());
	}

	// [[BPR-2305]]. The runtime's warnings (Codex's configuration warnings)
	// fold with the turn: counted on the line, each said in full when opened.
	{
		auto warned     = core::agent_turn_activity{ };
		warned.Warnings = { "Codex is ignoring 1 unrecognized configuration setting: `retired_option`.", "A second warning." };
		auto folded     = qt::agent_activity_view{ };
		folded.fold(warned, 4);
		expect(!folded.isHidden() && folded.summary_text() == QStringLiteral("Worked for 4 s: 2 warnings"), "a turn that only warned still folds to a line: " + folded.summary_text().toStdString());
		folded.summary_button()->click();
		const auto rows = folded.findChildren<QLabel*>(QStringLiteral("agentActivityWarning"));
		expect(rows.size() == 4 && std::ranges::any_of(rows, [](const QLabel* aRow) { return aRow->text().contains(QStringLiteral("retired_option")); }), "and opens to each warning as it was written");
	}

	auto quiet = qt::agent_activity_view{ };
	quiet.fold(core::agent_turn_activity{ }, 3);
	expect(quiet.isHidden(), "a turn that reported no work folds to nothing");
	qt::set_agent_activity_clock({ });
	std::cout << "agent-activity-view:pass\n";
	return EXIT_SUCCESS;
}
