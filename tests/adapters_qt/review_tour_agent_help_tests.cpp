// [[BPR-1878]], owner look v001 ("we should just have the bottom agent picker
// the same one from the agent tab"). The Tour tab has the Agent tab's own
// agent picker (agent_run_settings_picker over the same source) at its
// bottom: an agent chosen in either shows in both, and with no agent it is
// hidden, as in the Agent tab. Every helper's label says what a click does; no
// label names something the tab does not show. Pass a path prefix to save
// renders.
#include "../support/review_tour_fixture.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
namespace {
	using namespace bpr;

	void expect(bool aPass, const std::string& aMessage)
	{
		if (!aPass) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	template <typename Widget>
	auto child(qt::review_tour_editor& aEditor, const char* aName) -> Widget*
	{
		auto* const value = aEditor.findChild<Widget*>(QString::fromUtf8(aName));
		expect(value != nullptr, aName);
		return value;
	}
	void render(qt::review_tour_editor& aEditor, const std::string& aPrefix, const char* aName)
	{
		if (aPrefix.empty()) {
			return;
		}
		QApplication::processEvents();
		expect(aEditor.grab().save(QString::fromStdString(aPrefix + aName + ".png")), "render saved");
	}
	// AC2. Labels the owner could not read, or that name what the tab lacks.
	void expect_plain_labels(qt::review_tour_editor& aEditor, const char* aWhen)
	{
		for (auto* const button : aEditor.findChildren<QPushButton*>()) {
			if (!button->isVisibleTo(&aEditor)) {
				continue;
			}
			const auto text = button->text();
			expect(text != QStringLiteral("Next") && text != QStringLiteral("Recover explanation") && text != QStringLiteral("Use selected description text") && text != QStringLiteral("Enhance text"),
				   std::string{ aWhen } + ": no visible button reads " + text.toStdString());
		}
		for (const char* const name : { "tourProblemMagic", "tourProblemMap", "tourProblemNext" }) {
			auto* const button = child<QPushButton>(aEditor, name);
			expect(!button->toolTip().isEmpty(), std::string{ aWhen } + ": " + name + " says what it does");
		}
		const auto notice = child<QLabel>(aEditor, "tourWritingStatus")->text();
		expect(!notice.contains(QStringLiteral("Choose an available agent")), std::string{ aWhen } + ": no notice asks for a menu the tab lacks");
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	const auto   prefix = argc > 1 ? std::string{ argv[1] } : std::string{ };

	qt::agent_launch_panel panel;
	const auto             profiles = std::array{
		core::agent_profile{ .Id = core::agent_profile_id{ 1U }, .DisplayName = "Claude Code" },
		core::agent_profile{ .Id = core::agent_profile_id{ 2U }, .DisplayName = "Codex" }
	};
	panel.offer_profiles(profiles);
	panel.prefer_profile(std::nullopt);
	// The source the Agent tab keeps; a choice goes to the launch panel.
	auto source    = std::make_shared<qt::agent_run_settings_source>(panel);
	source->Choose = [&panel](const std::uint64_t aProfile) {
		const auto row = panel.profile_selector()->findData(QVariant::fromValue<qulonglong>(aProfile));
		panel.profile_selector()->setCurrentIndex(row);
		return row >= 0;
	};
	auto claude            = qt::agent_run_settings{ };
	claude.Profile         = 1U;
	claude.Agent           = QStringLiteral("Claude Code");
	claude.ModelChoosable  = true;
	claude.EffortChoosable = true;
	claude.Efforts         = { "low", "high" };
	auto codex             = qt::agent_run_settings{ };
	codex.Profile          = 2U;
	codex.Agent            = QStringLiteral("Codex");
	codex.ModelChoosable   = true;
	source->present({ claude, codex });
	// The Agent tab's own pill, standing in for its question field's.
	qt::agent_run_settings_picker agentTab{ source };

	auto draft     = tests::review_tour_fixture();
	draft.Problem  = { };
	draft.Solution = { };
	qt::review_tour_editor editor;
	editor.set_dispatch([](core::tour_command) { }, [](qt::tour_editor_action) { });
	std::optional<core::writing_hook> asked;
	editor.set_writing_dispatch([&](const core::writing_hook aHook, qt::prose_capture) { asked = aHook; }, [] { });
	editor.set_agent_picker(new qt::agent_run_settings_picker{ source, &editor });
	editor.resize(380, 760);
	editor.show();
	editor.activateWindow();
	editor.present({ .Draft = std::make_shared<const core::review_tour>(draft) });
	editor.show_problem();
	QApplication::processEvents();

	auto* const picker = editor.agent_picker();
	auto* const magic  = child<QPushButton>(editor, "tourProblemMagic");
	auto* const map    = child<QPushButton>(editor, "tourProblemMap");
	auto* const next   = child<QPushButton>(editor, "tourProblemNext");
	auto* const foot   = editor.foot();

	// AC1. The Agent tab's picker, at the bottom of the tab, showing its choice.
	expect(picker != nullptr && picker->objectName() == QStringLiteral("agentRunSettings") && picker->source() == agentTab.source(), "the Tour tab's picker is the Agent tab's component over its source");
	expect(picker->isVisible() && picker->full_text() == agentTab.full_text() && picker->full_text().startsWith(QStringLiteral("Claude Code")), "the picker shows the chosen agent, as the Agent tab's does");
	const auto pickerTop = picker->mapTo(&editor, QPoint{ 0, 0 }).y();
	expect(pickerTop > editor.problem_editor()->mapTo(&editor, QPoint{ 0, 0 }).y() && pickerTop + picker->height() <= foot->mapTo(&editor, QPoint{ 0, 0 }).y() + 1, "the picker sits at the bottom, just above the draft's state");
	render(editor, prefix, "agent-offered");

	// Choosing in the Tour tab's menu chooses in the Agent tab, and back.
	const auto choose = [](qt::agent_run_settings_picker& aPicker, const QString& aAgent) {
		Q_EMIT aPicker.menu()->aboutToShow();
		for (auto* const action : aPicker.menu()->actions()) {
			if (action->objectName() == QStringLiteral("agentRunSettingsAgent") && action->text() == aAgent) {
				action->trigger();
				return true;
			}
		}
		return false;
	};
	expect(choose(*picker, QStringLiteral("Codex")), "the Tour tab's menu offers every agent");
	QApplication::processEvents();
	expect(panel.selected_profile() == profiles[1].Id && agentTab.full_text().startsWith(QStringLiteral("Codex")) && picker->full_text().startsWith(QStringLiteral("Codex")), "an agent chosen in the Tour tab is the Agent tab's choice");
	expect(choose(agentTab, QStringLiteral("Claude Code")), "the Agent tab's menu offers every agent");
	QApplication::processEvents();
	expect(picker->full_text().startsWith(QStringLiteral("Claude Code")), "a choice in the Agent tab shows in the Tour tab");

	// Owner note on v002 ("i need the ability to pick effort level"): the
	// Tour tab's picker carries the Agent tab's Model and Effort menus, and an
	// effort chosen there goes where the Agent tab's does and shows in both.
	std::optional<std::pair<std::uint64_t, std::string>> effortChosen;
	source->Dispatch = [&](const std::uint64_t aProfile, std::optional<std::string>, std::optional<std::string> aEffort) {
		if (aEffort) {
			effortChosen = std::pair{ aProfile, *aEffort };
		}
		return true;
	};
	Q_EMIT picker->menu()->aboutToShow();
	QMenu* efforts = nullptr;
	QMenu* models  = nullptr;
	for (auto* const submenu : picker->menu()->findChildren<QMenu*>(Qt::FindDirectChildrenOnly)) {
		if (submenu->objectName() == QStringLiteral("agentRunSettingsEfforts")) {
			efforts = submenu;
		} else if (submenu->objectName() == QStringLiteral("agentRunSettingsModels")) {
			models = submenu;
		}
	}
	expect(efforts != nullptr && models != nullptr, "the Tour tab's picker offers Model and Effort, as the Agent tab's does");
	auto tookHigh = false;
	for (auto* const action : efforts->actions()) {
		if (action->text() == QStringLiteral("high")) {
			action->trigger();
			tookHigh = true;
		}
	}
	expect(tookHigh && effortChosen == std::pair{ std::uint64_t{ 1U }, std::string{ "high" } }, "an effort chosen in the Tour tab goes where the Agent tab's goes");
	claude.Effort = "high";
	source->present({ claude, codex });
	QApplication::processEvents();
	expect(picker->full_text().contains(QStringLiteral("high effort")) && agentTab.full_text() == picker->full_text(), "the chosen effort shows in both pickers");
	render(editor, prefix, "effort");

	// Starting help: the empty Problem's button drafts it, and says so.
	expect(magic->text() == QStringLiteral("Draft from PR context") && magic->toolTip().contains(QStringLiteral("drafting agent")), "an empty field's helper says it drafts from the pull request");
	magic->click();
	expect(asked == core::writing_hook::problem, "Draft from PR context asks for help with the Problem");
	expect(map->text() == QStringLiteral("Copy selected description text") && next->text() == QStringLiteral("Next: Solution"), "the other controls say their effect and where Next goes");
	expect_plain_labels(editor, "agents offered");
	editor.problem_editor()->setPlainText(QStringLiteral("Reading can crash."));
	QApplication::processEvents();
	expect(magic->text() == QStringLiteral("Enhance problem statement"), "a written field's helper says it rewrites the problem statement");

	// AC1. With no agent the picker is hidden, as the Agent tab's is; writing
	// by hand stays open (the host's notice on a click says why).
	panel.offer_profiles({ });
	panel.forget_choice();
	source->present({ });
	QApplication::processEvents();
	expect(!picker->isVisible() && agentTab.isHidden() && !child<QWidget>(editor, "tourAgentRow")->isVisible(), "with no agent the picker is hidden in both tabs");
	expect(!editor.problem_editor()->isReadOnly(), "writing by hand stays open without an agent");
	expect_plain_labels(editor, "no agent");
	render(editor, prefix, "no-agent");

	std::cout << "review-tour-agent-help:pass\n";
	return EXIT_SUCCESS;
}
