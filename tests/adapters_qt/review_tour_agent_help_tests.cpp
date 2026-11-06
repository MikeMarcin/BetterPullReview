// [[BPR-1878]]. The Tour tab offers the agent it drafts with: a Drafting agent
// menu sharing the Agent tab's, so a choice made in either shows in both, and
// one line saying what that agent will do. With no agent offered the menu
// gives way to the reason and Set up an agent. Every helper's label says what
// a click does; no label names something the tab does not show. Pass a path
// prefix to save renders.
#include "../support/review_tour_fixture.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include <QApplication>
#include <QComboBox>
#include <QLabel>
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

	auto draft     = tests::review_tour_fixture();
	draft.Problem  = { };
	draft.Solution = { };
	qt::review_tour_editor                editor;
	std::optional<qt::tour_editor_action> acted;
	editor.set_dispatch([](core::tour_command) { }, [&](const qt::tour_editor_action aAction) { acted = aAction; });
	std::optional<core::writing_hook> asked;
	editor.set_writing_dispatch([&](const core::writing_hook aHook, qt::prose_capture) { asked = aHook; }, [] { });
	editor.share_agent_choices(panel.profile_selector());
	editor.resize(380, 760);
	editor.show();
	editor.activateWindow();
	editor.present({ .Draft = std::make_shared<const core::review_tour>(draft) });
	editor.show_problem();
	QApplication::processEvents();

	auto* const picker = editor.agent_picker();
	auto* const setUp  = child<QPushButton>(editor, "tourAgentSetUp");
	auto* const magic  = child<QPushButton>(editor, "tourProblemMagic");
	auto* const map    = child<QPushButton>(editor, "tourProblemMap");
	auto* const next   = child<QPushButton>(editor, "tourProblemNext");

	// AC1. With agents offered, the tab shows them, the Agent tab's choice current.
	expect(picker->isVisible() && !setUp->isVisible() && picker->model() == panel.profile_selector()->model() && picker->count() == 2,
		   "the Tour tab offers the Agent tab's agents in its own menu");
	expect(picker->currentIndex() == panel.profile_selector()->currentIndex() && picker->currentIndex() >= 0, "the menu shows the agent already chosen");
	expect(editor.agent_help_text().contains(QStringLiteral("Agent tab")), "the line says the choice is the Agent tab's too");
	render(editor, prefix, "agent-offered");

	// Choosing here chooses in the Agent tab; choosing there shows here.
	picker->setCurrentIndex(1);
	emit picker->activated(1);
	QApplication::processEvents();
	expect(panel.selected_profile() == profiles[1].Id, "an agent chosen in the Tour tab is the Agent tab's choice");
	panel.profile_selector()->setCurrentIndex(0);
	QApplication::processEvents();
	expect(picker->currentIndex() == 0 && panel.selected_profile() == profiles[0].Id, "a choice in the Agent tab shows in the Tour tab");

	// The help the line offers is the host's, when it has more to say.
	editor.set_agent_help_source([] { return QStringLiteral("Codex can't draft tour text, so drafts and rewrites use Claude Code."); });
	expect(editor.agent_help_text().startsWith(QStringLiteral("Codex can't")), "the host's reason replaces the default line");
	editor.set_agent_help_source({ });

	// Starting help: the empty Problem's button drafts it, and says so.
	expect(magic->text() == QStringLiteral("Draft from PR context") && magic->toolTip().contains(QStringLiteral("drafting agent")), "an empty field's helper says it drafts from the pull request");
	magic->click();
	expect(asked == core::writing_hook::problem, "Draft from PR context asks for help with the Problem");
	expect(map->text() == QStringLiteral("Copy selected description text") && next->text() == QStringLiteral("Next: Solution"), "the other controls say their effect and where Next goes");
	expect_plain_labels(editor, "agents offered");
	editor.problem_editor()->setPlainText(QStringLiteral("Reading can crash."));
	QApplication::processEvents();
	expect(magic->text() == QStringLiteral("Enhance problem statement"), "a written field's helper says it rewrites the problem statement");

	// AC1. With none offered, the menu gives way to why, and setup.
	panel.offer_profiles({ });
	panel.forget_choice();
	QApplication::processEvents();
	expect(!picker->isVisible() && setUp->isVisible() && editor.agent_help_text().contains(QStringLiteral("No agent is set up")), "with no agent the tab says why and offers setup");
	setUp->click();
	expect(acted == qt::tour_editor_action::set_up_agent, "Set up an agent asks the window for agent settings");
	expect(!editor.problem_editor()->isReadOnly(), "writing by hand stays open without an agent");
	expect_plain_labels(editor, "no agent");
	render(editor, prefix, "no-agent");

	// A draft that cannot be written has nothing to draft: no agent help.
	auto readOnly     = draft;
	readOnly.ReadOnly = true;
	editor.present({ .Draft = std::make_shared<const core::review_tour>(readOnly) });
	QApplication::processEvents();
	expect(!child<QWidget>(editor, "tourAgentHelp")->isVisible(), "a read-only tour shows no agent help");

	std::cout << "review-tour-agent-help:pass\n";
	return EXIT_SUCCESS;
}
