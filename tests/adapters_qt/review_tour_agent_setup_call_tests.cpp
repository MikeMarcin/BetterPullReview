// [[BPR-1881]], owner 2026-11-06. Before the agent is set up the Tour tab's
// top says to set it up, and its one click asks the window for the Agent
// tab's setup; nothing asks by itself. Every agent feature wears one accent
// that matches the call to action, in light and dark and before and after
// setup, and waits for the setup with a tooltip that says why. Pass a path
// prefix to save renders.
#include "../support/review_tour_fixture.hpp"
#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QScrollArea>
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
	auto dark_palette(QPalette aPalette) -> QPalette
	{
		for (const auto group : { QPalette::Active, QPalette::Inactive, QPalette::Disabled }) {
			aPalette.setColor(group, QPalette::Window, QColor{ 0x32, 0x32, 0x32 });
			aPalette.setColor(group, QPalette::Base, QColor{ 0x1E, 0x1E, 0x1E });
			aPalette.setColor(group, QPalette::Button, QColor{ 0x3C, 0x3C, 0x3C });
			aPalette.setColor(group, QPalette::WindowText, QColor{ 0xE6, 0xE6, 0xE6 });
			aPalette.setColor(group, QPalette::ButtonText, QColor{ 0xE6, 0xE6, 0xE6 });
			aPalette.setColor(group, QPalette::Text, QColor{ 0xE6, 0xE6, 0xE6 });
		}
		return aPalette;
	}
	auto light_palette(QPalette aPalette) -> QPalette
	{
		for (const auto group : { QPalette::Active, QPalette::Inactive, QPalette::Disabled }) {
			aPalette.setColor(group, QPalette::Window, QColor{ 0xE7, 0xE7, 0xE7 });
			aPalette.setColor(group, QPalette::Base, QColor{ 0xFF, 0xFF, 0xFF });
			aPalette.setColor(group, QPalette::Button, QColor{ 0xF6, 0xF6, 0xF6 });
			aPalette.setColor(group, QPalette::WindowText, QColor{ 0x1F, 0x23, 0x28 });
			aPalette.setColor(group, QPalette::ButtonText, QColor{ 0x1F, 0x23, 0x28 });
			aPalette.setColor(group, QPalette::Text, QColor{ 0x1F, 0x23, 0x28 });
		}
		return aPalette;
	}
	auto contrast(const QColor& aFirst, const QColor& aSecond) -> double { return qt::contrast_ratio(qt::to_appearance_color(aFirst), qt::to_appearance_color(aSecond)); }

	// AC2. One accent, legible on the window and base colours.
	void expect_accent(qt::review_tour_editor& aEditor, const char* aWhen)
	{
		const auto ground = aEditor.palette();
		const auto accent = qt::agent_accent(ground);
		expect(contrast(accent, ground.color(QPalette::Active, QPalette::Window)) >= 4.5 && contrast(accent, ground.color(QPalette::Active, QPalette::Base)) >= 4.5,
			   std::string{ aWhen } + ": the accent reads at 4.5:1 on the window and base");
		// [[BPR-2000]], RFC-BPR-0101: a feature control wears the accent
		// exactly when pressing it starts a run; the others (Insert skeleton
		// stops, the agent picker) are ordinary and keep the spark.
		for (auto* const feature : aEditor.agent_features()) {
			const auto worn = feature->palette().color(QPalette::Active, QPalette::ButtonText) == accent;
			expect(worn == qt::agent_run_control(*feature) && (!qt::agent_run_control(*feature) || feature->palette().color(QPalette::Active, QPalette::WindowText) == accent),
				   std::string{ aWhen } + ": " + feature->objectName().toStdString() + (worn ? " wears the agent accent" : " wears the ordinary roles") + " as its run says");
			if (auto* const button = qobject_cast<QPushButton*>(feature); button != nullptr) {
				expect(!button->icon().isNull(), std::string{ aWhen } + ": " + feature->objectName().toStdString() + " carries the agent's spark, so colour is not the only sign");
			}
		}
		// The ordinary controls keep the palette's own words.
		auto* const map = child<QPushButton>(aEditor, "tourProblemMap");
		expect(map->palette().color(QPalette::Active, QPalette::ButtonText) != accent, std::string{ aWhen } + ": an ordinary control does not wear the accent");
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	const auto   prefix = argc > 1 ? std::string{ argv[1] } : std::string{ };
	QApplication::setPalette(light_palette(QApplication::palette()));

	auto draft     = tests::review_tour_fixture();
	draft.Problem  = { };
	draft.Solution = { };
	qt::review_tour_editor editor;
	auto                   actions = 0;
	editor.set_dispatch([](core::tour_command) { }, [&actions](qt::tour_editor_action) { ++actions; });
	auto asked = 0;
	editor.set_writing_dispatch([&asked](core::writing_hook, qt::prose_capture) { ++asked; }, [] { });
	editor.resize(380, 760);
	editor.show();
	editor.present({ .Draft = std::make_shared<const core::review_tour>(draft) });
	editor.show_problem();
	QApplication::processEvents();

	auto* const magic = child<QPushButton>(editor, "tourProblemMagic");
	expect(editor.agent_readiness() == qt::agent_setup_readiness::ready && magic->isEnabled(), "until told otherwise the agent is ready");
	expect(editor.agent_features().size() >= 3U, "the drafting helpers and Draft the tour are agent features");
	// [[BPR-2051]], owner decision 2026-11-11 ("A + footer 1"): the Tour tab
	// has no picker and no setup band; the window's footer holds both.
	for (const auto* name : { "tourAgentSetupCall", "tourAgentSetupOpen", "tourAgentBar", "tourAgentField", "agentRunSettings" }) {
		expect(editor.findChild<QWidget*>(QString::fromLatin1(name)) == nullptr, std::string{ name } + " is gone from the Tour tab");
	}

	// AC1, AC3. Not set up: no band; the features wait and say where to go.
	editor.set_agent_readiness(qt::agent_setup_readiness::unfinished);
	QApplication::processEvents();
	for (auto* const feature : editor.agent_features()) {
		expect(!feature->isEnabled(), feature->objectName().toStdString() + " waits for the setup");
		expect(feature->toolTip().contains(QStringLiteral("not set up")) && feature->toolTip().contains(QStringLiteral("footer")), feature->objectName().toStdString() + "'s tooltip says why and where to go");
	}
	expect(!editor.problem_editor()->isReadOnly() && child<QPushButton>(editor, "tourProblemMap")->isEnabled(), "writing by hand stays open");
	magic->click();
	expect(asked == 0, "a waiting helper asks nothing");
	expect(magic->palette().color(QPalette::Disabled, QPalette::ButtonText) == qt::agent_accent_disabled(editor.palette()), "a waiting helper's words are the accent, faded");
	expect_accent(editor, "light, before setup");
	render(editor, prefix, "before-setup-light");

	// Dark: the accent follows the palette.
	QApplication::setPalette(dark_palette(QApplication::palette()));
	QApplication::processEvents();
	expect(qt::agent_accent(editor.palette()) != qt::agent_accent(light_palette(QApplication::palette())), "dark has its own accent");
	expect_accent(editor, "dark, before setup");
	render(editor, prefix, "before-setup-dark");

	// Checking: the setup cannot say what is missing yet, so it asks nothing.
	editor.set_agent_readiness(qt::agent_setup_readiness::checking);
	QApplication::processEvents();
	expect(!magic->isEnabled() && magic->toolTip().contains(QStringLiteral("Waiting")), "while the setup checks, the helpers wait saying so");

	// AC1, AC3. Set up: the call goes; the features work and keep the accent.
	editor.set_agent_readiness(qt::agent_setup_readiness::ready);
	QApplication::processEvents();
	for (auto* const feature : editor.agent_features()) {
		expect(feature->isEnabled() && !feature->toolTip().contains(QStringLiteral("not set up")), feature->objectName().toStdString() + " can be used once set up");
	}
	expect(magic->text() == QStringLiteral("Draft from PR context") && magic->toolTip().contains(QStringLiteral("drafting agent")), "the helper says what it does again");
	magic->click();
	// [[BPR-2013]]. Suggest an outline left the application.
	expect(editor.findChild<QPushButton*>(QStringLiteral("tourSuggestOutline")) == nullptr, "no Suggest an outline");
	expect(asked == 1 && actions == 0, "set up, the helper asks");
	expect_accent(editor, "dark, after setup");
	render(editor, prefix, "after-setup-dark");
	QApplication::setPalette(light_palette(QApplication::palette()));
	QApplication::processEvents();
	expect_accent(editor, "light, after setup");
	render(editor, prefix, "after-setup-light");

	std::cout << "review-tour-agent-setup-call:pass\n";
	return EXIT_SUCCESS;
}
