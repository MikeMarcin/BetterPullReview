// [[BPR-1878]], amended by [[BPR-2051]] (owner decision 2026-11-11, "A +
// footer 1"): the Tour tab has no agent picker of its own; the pull request
// window's footer holds the one picker every agent feature uses. Every
// helper's label says what a click does, a written field's Enhance is its
// nib alone (owner look 2026-11-11), and no label names something the tab
// does not show. Pass a path prefix to save renders.
#include "../support/review_tour_fixture.hpp"
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
#include <vector>
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
		for (const char* const name : { "tourStopMagic" }) {
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

	// [[BPR-2274]]. A tour is its stops: the help is a stop's own.
	auto draft               = tests::review_tour_fixture();
	draft.Stops.front().Body = { };
	qt::review_tour_editor editor;
	editor.set_dispatch([](core::tour_command) { }, [](qt::tour_editor_action) { });
	std::optional<core::writing_hook> asked;
	editor.set_writing_dispatch([&](const core::writing_hook aHook, qt::prose_capture) { asked = aHook; }, [] { });
	editor.resize(380, 760);
	editor.show();
	editor.activateWindow();
	editor.present({ .Draft = std::make_shared<const core::review_tour>(draft) });
	editor.show_stops();
	editor.go_to_stop(draft.Stops.front().Id);
	editor.begin_stop_edit();
	QApplication::processEvents();

	auto* const magic = child<QPushButton>(editor, "tourStopMagic");

	// [[BPR-2051]]. No picker, no agent bar: the window's footer has them.
	for (const auto* name : { "agentRunSettings", "tourAgentBar", "tourAgentField", "tourAgentSetupCall" }) {
		expect(editor.findChild<QWidget*>(QString::fromLatin1(name)) == nullptr, std::string{ name } + " is not in the Tour tab");
	}
	render(editor, prefix, "agent-offered");

	// Starting help: the empty stop's button drafts its comment, and says so.
	expect(magic->text() == QStringLiteral("Generate comment") && magic->toolTip().contains(QStringLiteral("drafting agent")), "an empty stop's helper says it drafts from the code");
	magic->click();
	expect(asked == core::writing_hook::comment, "Generate comment asks for help with the stop");
	expect_plain_labels(editor, "agents offered");
	editor.stop_body()->setPlainText(QStringLiteral("Reading can crash."));
	QApplication::processEvents();
	// Owner look 2026-11-11: "We also don't need the \"Enhance\" label if
	// we're using the new enhance icon." The nib alone, named and explained.
	expect(magic->text().isEmpty() && !magic->icon().isNull() && magic->accessibleName() == QStringLiteral("Enhance comment") && magic->toolTip().contains(QStringLiteral("Enhance this stop. The suggestion waits for you to accept it.")),
		   "a written field's helper is Enhance: its icon, its name and its tooltip");
	// [[BPR-1995]], RFC-BPR-0100 AC05, as amended by [[BPR-2138]] (RFC-BPR-0101
	// AC11): Enhance is a fresh update whose suggestion waits on the part; it
	// neither opens nor sends a turn to the side chat, and is not the
	// one-shot writing hook, which stays for the CLI.
	asked.reset();
	auto chatAsked = std::vector<qt::tour_ask>{ };
	editor.set_tour_ask_dispatch([&chatAsked](const qt::tour_ask& aAsk) { chatAsked.push_back(aAsk); });
	const auto shownBefore = editor.side_chat()->current();
	magic->click();
	expect(!asked && chatAsked.size() == 1U && chatAsked[0].Update && chatAsked[0].Fresh && chatAsked[0].Thread.Kind == core::tour_thread_kind::stop && editor.side_chat()->current() == shownBefore,
		   "Enhance asks for a fresh update of the stop and leaves the side chat as it was");
	editor.request_field_writing(core::writing_hook::comment);
	expect(asked == core::writing_hook::comment && editor.request_name(core::writing_hook::comment) == QStringLiteral("Enhance comment"), "the one-shot request keeps its whole name");
	expect(!editor.stop_body()->isReadOnly(), "writing by hand stays open");
	expect_plain_labels(editor, "written");
	render(editor, prefix, "written");

	std::cout << "review-tour-agent-help:pass\n";
	return EXIT_SUCCESS;
}
