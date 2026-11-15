/// \file
/// \brief [[BPR-2051]], owner decision 2026-11-11 ("A + footer 1"): the pull
/// request window's agent footer, in its status bar.
///
/// \details One picker for the pull request; the run with where it started,
/// what it does, how long and Stop; several runs as a count whose list has a
/// Stop each; a finished run's ending for a moment; the quiet tone after two
/// minutes without a frame; the session's cost summed across places; and
/// setup as a state of the picker with Agent settings…. The picker and the
/// cost keep their place whatever the run region says.
///
/// `--render DIR` saves the footer's states in a window's status bar (light
/// and dark under `BPR_NATIVE_LOOK`), for the owner's look. `--look` opens a
/// window of status bars, one footer in each state, the first one live: its
/// run ticks, its Stop and picker work, and its run ends after a minute.

#include "bpr/core/agent_activity.hpp"
#include "bpr/core/agent_cost.hpp"
#include "bpr/core/agent_grant.hpp"
#include "bpr/qt/agent_entry_dialog.hpp"
#include "bpr/qt/agent_footer.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_run_settings_picker.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QElapsedTimer>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QStyleHints>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

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

	auto Now = qint64{ 1'000'000 };

	[[nodiscard]] auto status(const qt::agent_run_stage aStage, std::optional<core::agent_turn_activity> aActivity = std::nullopt) -> qt::agent_run_status
	{
		auto shown              = qt::agent_run_status{ };
		shown.Agent             = QStringLiteral("Claude Code");
		shown.Stage             = aStage;
		shown.Activity          = std::move(aActivity);
		shown.ActivityStartedMs = Now;
		return shown;
	}

	[[nodiscard]] auto waiting_on_helpers() -> core::agent_turn_activity
	{
		auto activity  = core::agent_turn_activity{ };
		activity.Doing = core::agent_doing::waiting;
		activity.Helpers.push_back({ .Id = "a", .Description = "read the pool" });
		activity.Helpers.push_back({ .Id = "b", .Description = "read the tests" });
		return activity;
	}

	void render(QMainWindow& aWindow, const std::string& aRoot, const char* aName)
	{
		if (aRoot.empty()) {
			return;
		}
		const auto native = qEnvironmentVariable("BPR_NATIVE_LOOK") == QStringLiteral("1");
		for (const auto& [suffix, scheme] : { std::pair{ "light", Qt::ColorScheme::Light }, std::pair{ "dark", Qt::ColorScheme::Dark } }) {
			if (native) {
				QGuiApplication::styleHints()->setColorScheme(scheme);
				settle();
			}
			const auto bar = aWindow.statusBar()->grab();
			expect(bar.save(QString::fromStdString(aRoot + "/footer-" + aName + "-" + suffix + ".png")), "render saved");
		}
	}
}

namespace {
	/// The owner's look: each state in its own status bar, the first live.
	auto look(const std::shared_ptr<qt::agent_run_settings_source>& aSource) -> int
	{
		qt::set_agent_activity_clock({ });
		QWidget window;
		window.setWindowTitle(QStringLiteral("Agent footer: live run, quiet, several, answered, needs setup, idle"));
		auto* const column = new QVBoxLayout{ &window };
		column->setSpacing(14);
		const auto add = [&](const QString& aCaption) {
			auto* const caption = new QLabel{ aCaption, &window };
			auto        bold    = caption->font();
			bold.setWeight(QFont::DemiBold);
			caption->setFont(bold);
			column->addWidget(caption);
			auto* const bar    = new QStatusBar{ &window };
			auto* const review = new QLabel{ QStringLiteral("Actionable file 2 of 4. Private progress: seen, not accepted. Review history is ready."), bar };
			review->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
			bar->addWidget(review, 1);
			auto* const footer = new qt::agent_footer{ aSource, bar };
			bar->addPermanentWidget(footer);
			footer->present_cost(qt::agent_footer_place::agent_tab, core::parse_agent_cost("0.12"));
			footer->present_cost(qt::agent_footer_place::draft_tour, core::parse_agent_cost("0.41"));
			column->addWidget(bar);
			return footer;
		};
		auto started = QElapsedTimer{ };
		started.start();
		auto* const live       = add(QStringLiteral("One run, live: it ticks; Stop ends it; the picker's menu chooses the agent"));
		auto        busy       = status(qt::agent_run_stage::thinking, waiting_on_helpers());
		busy.ActivityStartedMs = qt::agent_activity_clock_ms();
		const auto stop        = [live, &started] { live->present_finished(qt::agent_footer_place::agent_tab, QStringLiteral("stopped after %1").arg(qt::agent_elapsed_words(started.elapsed() / 1000))); };
		live->present_run(qt::agent_footer_place::agent_tab, qt::agent_footer_run{ .Status = busy, .Started = started, .Stop = stop });
		QTimer::singleShot(60'000, live, [live, &started] { live->present_finished(qt::agent_footer_place::agent_tab, QStringLiteral("answered in %1 · $1.84 · 2 findings").arg(qt::agent_elapsed_words(started.elapsed() / 1000))); });
		auto quiet              = busy;
		quiet.ActivityStartedMs = qt::agent_activity_clock_ms() - 130'000;
		add(QStringLiteral("Quiet for two minutes: the tone changes; nothing else moves"))->present_run(qt::agent_footer_place::agent_tab, qt::agent_footer_run{ .Status = quiet, .Started = started, .Stop = [] { } });
		auto* const several = add(QStringLiteral("Several runs: a count that opens the list, each with its own Stop"));
		several->present_run(qt::agent_footer_place::agent_tab, qt::agent_footer_run{ .Status = busy, .Started = started, .Stop = [] { } });
		several->present_run(qt::agent_footer_place::enhance, qt::agent_footer_run{ .Status = status(qt::agent_run_stage::writing), .Started = started, .Stop = [] { } });
		auto* const answered = add(QStringLiteral("Just answered (here held; in the app for a few seconds)"));
		answered->set_finished_hold(24 * 60 * 60 * 1000);
		answered->present_finished(qt::agent_footer_place::tour_chat, QStringLiteral("answered in 0:21 · $0.04"));
		add(QStringLiteral("The chosen agent needs setup: it says what is missing; Agent settings… leads to Settings"))->present_setup(QStringLiteral("not signed in"));
		add(QStringLiteral("Idle: the pull request's agent and what this session has cost. Its menu holds the grant: Claude writes only in its scratch folder; Codex may build here"));
		// [[BPR-2008]]. The question a pull request asks once, before the first build.
		auto* const ask = new QPushButton{ QStringLiteral("Show the build question…"), &window };
		ask->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		QObject::connect(ask, &QPushButton::clicked, &window, [&window] { static_cast<void>(qt::ask_build_question(&window)); });
		column->addWidget(ask);
		column->addStretch(1);
		window.resize(1100, 560);
		window.show();
		return QApplication::exec();
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	const auto   root = argc > 2 && std::string_view{ argv[1] } == "--render" ? std::string{ argv[2] } : std::string{ };
	qt::set_agent_activity_clock([] { return Now; });

	qt::agent_launch_panel panel;
	const auto             profiles = std::array{ core::agent_profile{ .Id = core::agent_profile_id{ 1U }, .DisplayName = "Claude Code" }, core::agent_profile{ .Id = core::agent_profile_id{ 2U }, .DisplayName = "Codex" } };
	panel.offer_profiles(profiles);
	auto source    = std::make_shared<qt::agent_run_settings_source>(panel);
	auto chosen    = 0U;
	source->Choose = [&chosen, &panel](const std::uint64_t aProfile) {
		chosen = static_cast<unsigned>(aProfile);
		panel.profile_selector()->setCurrentIndex(panel.profile_selector()->findData(QVariant::fromValue<qulonglong>(aProfile)));
		return true;
	};
	auto claude            = qt::agent_run_settings{ };
	claude.Profile         = 1U;
	claude.Agent           = QStringLiteral("Claude Code");
	claude.ModelChoosable  = true;
	claude.EffortChoosable = true;
	claude.Model           = "claude-opus-5-5";
	claude.Effort          = "high";
	// [[BPR-2007]], [[BPR-2008]], RFC-BPR-0104 as amended. Claude with its
	// scratch folder on and builds off; Codex allowed to build here.
	const auto present_grant = [](qt::agent_run_settings& aSettings, const core::agent_grant_facts& aFacts) {
		const auto grant          = core::resolve_agent_grant(aFacts);
		aSettings.GrantApplies    = grant.Applies;
		aSettings.ScratchText     = QString::fromStdString(core::agent_scratch_toggle_text(grant));
		aSettings.ScratchOn       = core::agent_scratch_toggle_checked(grant);
		aSettings.ScratchEnabled  = core::agent_scratch_toggle_enabled(grant);
		aSettings.ScratchTip      = QString::fromStdString(core::agent_scratch_tooltip(grant, aFacts.Program, 256U));
		aSettings.BuildsOn        = core::agent_builds_toggle_checked(grant);
		aSettings.BuildsEnabled   = core::agent_builds_toggle_enabled(grant);
		aSettings.BuildsTip       = QString::fromStdString(core::agent_builds_tooltip(grant, aFacts.Program));
		aSettings.ScratchOpenable = core::scratch_granted(grant);
		aSettings.ModelHint       = QStringLiteral("Or type /model NAME in the Agent tab's question field.");
	};
	present_grant(claude, { .Program = core::agent_program::claude });
	auto codex    = claude;
	codex.Profile = 2U;
	codex.Agent   = QStringLiteral("Codex");
	codex.Model.clear();
	codex.Effort.clear();
	present_grant(codex, { .Program = core::agent_program::codex, .Builds = true, .BuildAnswer = true });
	source->present({ claude, codex });
	panel.profile_selector()->setCurrentIndex(panel.profile_selector()->findData(QVariant::fromValue<qulonglong>(1U)));
	if (argc > 1 && std::string_view{ argv[1] } == "--look") {
		return look(source);
	}

	QMainWindow window;
	auto* const review = new QLabel{ QStringLiteral("Actionable file 2 of 4. Private progress: seen, not accepted. Review history is ready."), &window };
	review->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
	window.statusBar()->addWidget(review, 1);
	auto* const footer = new qt::agent_footer{ source, &window };
	window.statusBar()->addPermanentWidget(footer);
	auto settingsAsked = 0;
	footer->set_settings_dispatch([&settingsAsked] { ++settingsAsked; });
	window.resize(1100, 200);
	window.show();
	settle();

	auto* const picker   = footer->picker();
	const auto  pickerAt = [&] { return picker->mapTo(&window, QPoint{ 0, 0 }); };

	// Idle: the picker, nothing running, no cost yet.
	expect(picker->isVisible() && picker->full_text().startsWith(QStringLiteral("Claude Code · Opus 5.5")) && !footer->stop_button()->isVisible() && !footer->runs_button()->isVisible() && footer->cost_label()->text().isEmpty(),
		   "idle: the chosen agent in the picker, nothing running, no cost");
	expect(!footer->settings_button()->isVisible(), "a set-up agent needs no Agent settings… beside it");
	render(window, root, "idle-no-cost");

	// The session's cost: every place's conversation summed.
	footer->present_cost(qt::agent_footer_place::agent_tab, core::parse_agent_cost("0.12"));
	footer->present_cost(qt::agent_footer_place::tour_chat, core::parse_agent_cost("0.04"));
	footer->present_cost(qt::agent_footer_place::draft_tour, core::parse_agent_cost("0.37"));
	settle();
	expect(footer->cost_label()->text() == QStringLiteral("$0.53") && footer->cost_label()->toolTip().contains(QStringLiteral("Agent tab $0.12")) && footer->cost_label()->toolTip().contains(QStringLiteral("Draft the tour $0.37")),
		   "the cost is this pull request's total, each place in its tooltip");
	render(window, root, "idle");
	const auto idleAt = pickerAt();

	// One run: where it started, what it does, how long, and Stop.
	auto stopped = std::string{ };
	auto started = QElapsedTimer{ };
	started.start();
	footer->present_run(qt::agent_footer_place::agent_tab, qt::agent_footer_run{ .Status = status(qt::agent_run_stage::thinking, waiting_on_helpers()), .Started = started, .Stop = [&stopped] { stopped += "agent;"; } });
	settle();
	expect(footer->run_where()->text() == QStringLiteral("Agent tab") && footer->run_doing()->text() == QStringLiteral("waiting on 2 helpers") && footer->run_elapsed()->text() == QStringLiteral("0:00") && footer->stop_button()->isVisible(),
		   "one run: Agent tab, waiting on 2 helpers, its time, Stop");
	expect(pickerAt() == idleAt, "the picker keeps its place while a run shows");
	render(window, root, "one-run");
	footer->stop_button()->click();
	expect(stopped == "agent;", "Stop stops that run");

	// Quiet for two minutes: the tone and the words change, nothing moves.
	auto quiet = status(qt::agent_run_stage::thinking, waiting_on_helpers());
	Now += 125'000;
	footer->present_run(qt::agent_footer_place::agent_tab, qt::agent_footer_run{ .Status = quiet, .Started = started, .Stop = [&stopped] { stopped += "agent;"; } });
	settle();
	expect(footer->run_doing()->text() == QStringLiteral("no activity for 2 min") && pickerAt() == idleAt, "quiet: no activity for 2 min, in place");
	render(window, root, "quiet");
	Now -= 125'000;

	// Several: a count that opens the list, each run with its own Stop.
	footer->present_run(qt::agent_footer_place::enhance, qt::agent_footer_run{ .Status = status(qt::agent_run_stage::writing), .Started = started, .Stop = [&stopped] { stopped += "enhance;"; } });
	settle();
	expect(footer->runs_button()->isVisible() && !footer->stop_button()->isVisible() && footer->runs_button()->text().contains(QStringLiteral("2 running")) && footer->runs_button()->text().contains(QStringLiteral("Agent tab, Enhance")),
		   "two runs: a count naming where each started");
	expect(pickerAt() == idleAt, "the picker keeps its place with two runs");
	emit footer->runs_menu()->aboutToShow();
	auto stops = 0;
	for (auto* const item : footer->runs_menu()->actions()) {
		if (item->objectName() == QStringLiteral("agentFooterStopRun")) {
			++stops;
			if (item->text().startsWith(QStringLiteral("Stop Enhance"))) {
				item->trigger();
			}
		}
	}
	expect(stops == 2 && stopped == "agent;enhance;", "the list has a Stop for each run, and each stops its own");
	render(window, root, "several");

	// Finished: its ending for a moment, then idle again.
	footer->set_finished_hold(30);
	footer->present_run(qt::agent_footer_place::enhance, std::nullopt);
	footer->present_finished(qt::agent_footer_place::agent_tab, QStringLiteral("answered in 7:03 · $1.84 · 2 findings"));
	settle();
	expect(footer->running() == 0U && footer->run_where()->isVisible() && footer->run_where()->text() == QStringLiteral("Agent tab") && footer->run_doing()->text().startsWith(QStringLiteral("answered in 7:03")) && !footer->stop_button()->isVisible(),
		   "just answered: where, and how it ended");
	render(window, root, "answered");
	QElapsedTimer wait;
	wait.start();
	while (footer->run_where()->isVisible() && wait.elapsed() < 2000) {
		QApplication::processEvents();
	}
	expect(!footer->run_where()->isVisible() && pickerAt() == idleAt, "then idle, the picker where it was");

	// Setup: the picker says what is missing, and Agent settings… leads on.
	footer->present_setup(QStringLiteral("not signed in"));
	settle();
	expect(picker->full_text() == QStringLiteral("Claude Code · not signed in") && picker->attention() == QStringLiteral("not signed in") && footer->settings_button()->isVisible() && footer->settings_button()->text() == QStringLiteral("Agent settings…"),
		   "setup: the picker names what is missing, and Agent settings… shows");
	expect(picker->width() >= picker->sizeHint().width(), "the picker keeps its whole words: " + std::to_string(picker->width()) + " of " + std::to_string(picker->sizeHint().width()));
	footer->settings_button()->click();
	expect(settingsAsked == 1, "Agent settings… asks for Settings");
	// Owner note on the footer look ("Why is it red?"): a failed check is
	// drawn in the failed check's colour; an agent not checked yet is quiet.
	expect(!picker->attention_failed(), "a need given without a failed check is drawn quiet");
	render(window, root, "needs-setup");
	footer->present_setup(QStringLiteral("check failed"), true);
	settle();
	expect(picker->attention_failed(), "a failed check keeps the failed check's colour");
	render(window, root, "check-failed");
	footer->present_setup({ });
	settle();
	expect(!footer->settings_button()->isVisible() && picker->full_text().startsWith(QStringLiteral("Claude Code · Opus 5.5")), "set up again: the picker as before");

	// [[BPR-2007]], RFC-BPR-0104 AC05, AC09 as amended 2026-11-11 (owner look
	// e97c6a3d). Under the model and effort, two toggles for this pull
	// request; every explanation is a tooltip, and no line explains.
	auto toggled          = std::string{ };
	auto opened           = 0;
	source->ToggleScratch = [&toggled](const std::uint64_t aProfile, const bool aOn) { toggled += std::to_string(aProfile) + (aOn ? ":on;" : ":off;"); };
	source->ToggleBuilds  = [&toggled](const std::uint64_t aProfile, const bool aOn) { toggled += std::to_string(aProfile) + (aOn ? ":build;" : ":nobuild;"); };
	source->OpenScratch   = [&opened](const std::uint64_t) { ++opened; };
	const auto find       = [&](const char* aName) -> QAction* {
		for (auto* const item : picker->menu()->actions()) {
			if (item->objectName() == QString::fromLatin1(aName)) {
				return item;
			}
		}
		return nullptr;
	};
	{
		emit        picker->menu()->aboutToShow();
		auto* const scratch = find("agentRunSettingsScratch");
		auto* const builds  = find("agentRunSettingsBuilds");
		auto* const open    = find("agentRunSettingsOpenScratch");
		expect(scratch != nullptr && scratch->text() == QStringLiteral("Use scratch folder") && scratch->isCheckable() && scratch->isChecked() && scratch->isEnabled(), "Use scratch folder is checked while on");
		expect(scratch->toolTip().contains(QStringLiteral("permission rule")) && !scratch->toolTip().contains(QStringLiteral("sandbox")), "its tooltip: Claude's grant is never called a sandbox");
		expect(builds != nullptr && builds->text() == QStringLiteral("Allow build and run tests") && builds->isCheckable() && !builds->isChecked() && builds->isEnabled(), "Allow build and run tests, off");
		expect(picker->menu()->toolTipsVisible(), "the menu shows its tooltips");
		for (auto* const item : picker->menu()->actions()) {
			expect(item->isEnabled() || item->isSeparator() || item == builds || item == scratch, "no disabled explainer line in the menu: " + item->text().toStdString());
			expect(!item->text().contains(QStringLiteral("/model")) && !item->text().contains(QStringLiteral(" runs ")), "no /model line and no runs line");
		}
		expect(picker->menu()->findChild<QMenu*>(QStringLiteral("agentRunSettingsModels"))->menuAction()->toolTip().contains(QStringLiteral("/model")), "the /model hint is the Model menu's tooltip");
		expect(picker->menu()->findChild<QMenu*>(QStringLiteral("agentRunSettingsEfforts"))->menuAction()->toolTip().isEmpty() || !picker->menu()->findChild<QMenu*>(QStringLiteral("agentRunSettingsEfforts"))->menuAction()->toolTip().contains(QStringLiteral("/effort")),
			   "no /effort hint where it was not said to be true");
		auto actions = picker->menu()->actions();
		expect(actions.indexOf(scratch) > actions.indexOf(picker->menu()->findChild<QMenu*>(QStringLiteral("agentRunSettingsEfforts"))->menuAction()), "the toggles sit under the model and effort");
		expect(open != nullptr, "Open scratch folder once the session has one");
		// [[BPR-2007]]. For the owner's look: the menu with the toggles, the
		// agent's sheet with its two settings, and the question.
		if (!root.empty()) {
			const auto native = qEnvironmentVariable("BPR_NATIVE_LOOK") == QStringLiteral("1");
			const auto scheme = native ? qEnvironmentVariable("BPR_NATIVE_LOOK_SCHEME", QStringLiteral("light")) : QStringLiteral("offscreen");
			if (native) {
				QGuiApplication::styleHints()->setColorScheme(scheme == QStringLiteral("dark") ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
				settle();
			}
			picker->menu()->adjustSize();
			expect(picker->menu()->grab().save(QString::fromStdString(root) + QStringLiteral("/grant-menu-") + scheme + QStringLiteral(".png")), "menu render saved");
			auto codexEntry           = core::preset_agent_entry(core::setup_agent::codex, core::codex_protocol_choice::app_server, "/opt/homebrew/bin/codex");
			codexEntry.Id             = 2U;
			codexEntry.BuildsAndTests = true;
			auto sheet                = qt::agent_entry_dialog{ codexEntry, &window };
			sheet.adjustSize();
			expect(sheet.grab().save(QString::fromStdString(root) + QStringLiteral("/agent-sheet-") + scheme + QStringLiteral(".png")), "sheet render saved");
			const auto question = qt::make_build_question(&window);
			question->adjustSize();
			expect(question->grab().save(QString::fromStdString(root) + QStringLiteral("/build-question-") + scheme + QStringLiteral(".png")), "question render saved");
		}
		open->trigger();
		scratch->trigger();
		expect(toggled == "1:off;" && opened == 1, "unchecking turns it off for this pull request and agent; Open opens it");
	}
	{
		emit picker->menu()->aboutToShow();
		find("agentRunSettingsBuilds")->trigger();
		expect(toggled == "1:off;1:build;", "checking Allow build and run tests turns builds on for this pull request and agent");
	}
	present_grant(claude, { .Program = core::agent_program::claude, .ScratchChoice = false });
	source->present({ claude, codex });
	settle();
	{
		emit picker->menu()->aboutToShow();
		expect(!find("agentRunSettingsScratch")->isChecked() && find("agentRunSettingsScratch")->toolTip().contains(QStringLiteral("its own settings allow")), "off: unchecked, and its tooltip says what still applies");
	}
	// Review finding 4: a full folder is named full; a grant the launch will
	// not carry reads unavailable, never on.
	present_grant(claude, { .Program = core::agent_program::claude, .ScratchFull = true });
	source->present({ claude, codex });
	settle();
	{
		emit picker->menu()->aboutToShow();
		expect(find("agentRunSettingsScratch")->text() == QStringLiteral("Use scratch folder (full)") && find("agentRunSettingsScratch")->toolTip().contains(QStringLiteral("256 MiB")), "full: named full, with its limit");
	}
	present_grant(claude, { .Program = core::agent_program::claude, .ScratchPlace = false });
	source->present({ claude, codex });
	settle();
	{
		emit picker->menu()->aboutToShow();
		expect(!find("agentRunSettingsScratch")->isChecked() && !find("agentRunSettingsScratch")->isEnabled(), "no place for a folder: unchecked and unavailable");
	}
	present_grant(claude, { .Program = core::agent_program::claude });
	source->present({ claude, codex });
	settle();

	// One choice: the picker's menu chooses the agent every feature uses.
	emit picker->menu()->aboutToShow();
	for (auto* const item : picker->menu()->actions()) {
		if (item->objectName() == QStringLiteral("agentRunSettingsAgent") && item->text() == QStringLiteral("Codex")) {
			// The choice rebuilds the menu, so nothing of it is read after.
			item->trigger();
			break;
		}
	}
	settle();
	expect(chosen == 2U && picker->full_text().startsWith(QStringLiteral("Codex")), "choosing in the footer chooses the agent");
	emit picker->menu()->aboutToShow();
	expect(find("agentRunSettingsBuilds")->isChecked() && find("agentRunSettingsBuilds")->toolTip().contains(QStringLiteral("Codex's sandbox holds")), "Codex allowed to build: checked, and its tooltip says Codex's sandbox holds it");

	// RFC-BPR-0126 AC09 ([[BPR-2151]], owner Q10/AC09): one Stop all beside
	// the picker, shown while any turn runs or any message is queued; it
	// stops every run and asks the window to pause every queue.
	{
		auto stoppedAll = std::string{ };
		auto pausedAll  = 0;
		footer->set_stop_all_dispatch([&pausedAll] { ++pausedAll; });
		footer->present_run(qt::agent_footer_place::agent_tab, std::nullopt);
		footer->present_run(qt::agent_footer_place::tour_chat, std::nullopt);
		footer->present_run(qt::agent_footer_place::draft_tour, std::nullopt);
		footer->present_queued(0U);
		settle();
		expect(footer->stop_all_button() != nullptr && !footer->stop_all_button()->isVisible(), "Stop all hides while nothing runs and nothing waits");
		footer->present_queued(2U);
		settle();
		expect(footer->stop_all_button()->isVisible() && footer->stop_all_button()->accessibleName() == QStringLiteral("Stop all"), "a queued message shows Stop all, named");
		auto started2 = QElapsedTimer{ };
		started2.start();
		footer->present_run(qt::agent_footer_place::agent_tab, qt::agent_footer_run{ .Status = status(qt::agent_run_stage::thinking, waiting_on_helpers()), .Started = started2, .Stop = [&stoppedAll] { stoppedAll += "agent;"; } });
		footer->present_run(qt::agent_footer_place::tour_chat, qt::agent_footer_run{ .Status = status(qt::agent_run_stage::thinking, waiting_on_helpers()), .Started = started2, .Stop = [&stoppedAll] { stoppedAll += "tour;"; } });
		settle();
		expect(footer->stop_all_button()->isVisible() && footer->stop_all_button()->focusPolicy() != Qt::NoFocus, "and so does a running turn; it takes keyboard focus");
		// Look prompts-v001: beside it, how long the turn has been thinking.
		expect(footer->thinking_for()->isVisible() && footer->thinking_for()->text().startsWith(QStringLiteral("Thinking for ")) && footer->thinking_for()->text().endsWith(QStringLiteral("s")),
			   "a running turn says how long it has been thinking: " + footer->thinking_for()->text().toStdString());
		footer->stop_all_button()->click();
		expect(stoppedAll == "agent;tour;" && pausedAll == 1, "it stops every run and pauses every queue: " + stoppedAll);
		footer->present_run(qt::agent_footer_place::agent_tab, std::nullopt);
		footer->present_run(qt::agent_footer_place::tour_chat, std::nullopt);
		footer->present_queued(0U);
		settle();
	}
	// Look ask-v005 (owner, 2026-11-14: "I don't know why it says setup
	// agent on launch if the agent is already set up"): the window shows its
	// footer before the agents are known, so the picker is hidden and the
	// footer offers Set up an agent…; once the agent is known the picker shows,
	// and that offer must go.
	{
		qt::agent_launch_panel laterPanel;
		laterPanel.offer_profiles(profiles);
		laterPanel.profile_selector()->setCurrentIndex(laterPanel.profile_selector()->findData(QVariant::fromValue<qulonglong>(1U)));
		auto        later = std::make_shared<qt::agent_run_settings_source>(laterPanel);
		QMainWindow launch;
		auto* const early = new qt::agent_footer{ later, &launch };
		launch.statusBar()->addPermanentWidget(early);
		launch.resize(1100, 200);
		launch.show();
		settle();
		expect(early->settings_button()->isVisible() && early->settings_button()->text() == QStringLiteral("Set up an agent…"), "before the agents are known: Set up an agent…");
		later->present({ claude });
		settle();
		expect(early->picker()->isVisible() && !early->settings_button()->isVisible(), "once the agent is known the picker shows and Set up an agent… goes");
	}
	std::cout << "agent-footer:pass\n";
	return EXIT_SUCCESS;
}
