// [[BPR-1784]]. The Tour tab reads as three numbered steps, as the agent setup
// does: 1 Problem, 2 Solution, 3 Stops. Each step's state follows the draft,
// done steps fold to what was written with Change, moving on is Next (or the
// key that marks a stop Ready), and the keyboard lands in the open step's
// first field. Pass a path prefix to save light and dark renders.
#include "../support/review_tour_fixture.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/keyboard_shortcuts.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include <QAbstractButton>
#include <QApplication>
#include <QLineEdit>
#include <QListWidget>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
namespace {
	using namespace bpr;
	using core::tour_step;
	using state = core::tour_step_state;

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
	[[nodiscard]] auto states(const qt::review_tour_editor& aEditor, const state aProblem, const state aSolution, const state aStops) -> bool
	{
		const auto& steps = aEditor.steps();
		return steps.state(tour_step::problem) == aProblem && steps.state(tour_step::solution) == aSolution && steps.state(tour_step::stops) == aStops;
	}
	[[nodiscard]] auto dark_palette() -> QPalette
	{
		auto palette = QPalette{ };
		palette.setColor(QPalette::Window, QColor{ 0x1F, 0x23, 0x28 });
		palette.setColor(QPalette::WindowText, QColor{ 0xE6, 0xED, 0xF3 });
		palette.setColor(QPalette::Base, QColor{ 0x16, 0x1B, 0x22 });
		palette.setColor(QPalette::Text, QColor{ 0xE6, 0xED, 0xF3 });
		palette.setColor(QPalette::Button, QColor{ 0x2D, 0x33, 0x3B });
		palette.setColor(QPalette::ButtonText, QColor{ 0xE6, 0xED, 0xF3 });
		palette.setColor(QPalette::Highlight, QColor{ 0x2F, 0x81, 0xF7 });
		palette.setColor(QPalette::HighlightedText, QColor{ 0xFF, 0xFF, 0xFF });
		palette.setColor(QPalette::PlaceholderText, QColor{ 0x8B, 0x94, 0x9E });
		palette.setColor(QPalette::Link, QColor{ 0x58, 0xA6, 0xFF });
		return palette;
	}
	void render(qt::review_tour_editor& aEditor, const std::string& aPrefix, const char* aName)
	{
		if (aPrefix.empty()) {
			return;
		}
		const auto light = aEditor.palette();
		for (const auto& [suffix, palette] : { std::pair{ "light", light }, std::pair{ "dark", dark_palette() } }) {
			aEditor.setPalette(palette);
			QApplication::processEvents();
			expect(aEditor.grab().save(QString::fromStdString(aPrefix + aName + "-" + suffix + ".png")), "render saved");
		}
		aEditor.setPalette(light);
		QApplication::processEvents();
	}
	[[nodiscard]] auto stop(const std::uint64_t aId, std::string aTitle, const core::tour_preparation aPreparation) -> core::tour_stop
	{
		auto value        = tests::review_tour_fixture().Stops.front();
		value.Id          = core::tour_stop_id{ aId };
		value.Title       = std::move(aTitle);
		value.Body        = value.Title.empty() ? std::string{ } : "The explanation and source stay together.";
		value.Preparation = aPreparation;
		return value;
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	const auto   prefix = argc > 1 ? std::string{ argv[1] } : std::string{ };

	// The draft, as the host keeps it: each edit applied in core and presented.
	auto draft       = tests::review_tour_fixture();
	draft.Problem    = { };
	draft.Solution   = { };
	draft.Stops      = { };
	draft.LastStopId = 0;
	auto selection   = std::uint64_t{ 1 };
	auto selected    = core::tour_stop_id{ };

	qt::review_tour_editor editor;
	const auto             show = [&] { editor.present({ .Draft = std::make_shared<const core::review_tour>(draft), .Selected = selected, .SelectionRevision = selection, .SavedRevision = draft.Revision }); };
	editor.set_dispatch(
		[&](const core::tour_command aCommand) {
			auto outcome = core::apply(draft, aCommand);
			if (outcome.Refusal == core::tour_refusal::none) {
				draft = std::move(outcome.Accepted);
			}
			show();
		},
		[](qt::tour_editor_action) { });
	editor.resize(380, 760);
	editor.show();
	editor.activateWindow();
	show();
	QApplication::processEvents();

	auto* const problemRow   = child<QAbstractButton>(editor, "tourProblemRow");
	auto* const solutionRow  = child<QAbstractButton>(editor, "tourSolutionRow");
	auto* const stopsRow     = child<QAbstractButton>(editor, "tourStopsRow");
	auto* const problemNext  = child<QPushButton>(editor, "tourProblemNext");
	auto* const solutionNext = child<QPushButton>(editor, "tourSolutionNext");
	auto* const problemEdit  = child<QPushButton>(editor, "tourProblemChange");
	auto* const solutionEdit = child<QPushButton>(editor, "tourSolutionChange");
	auto* const addStop      = child<QPushButton>(editor, "tourAddStop");
	auto* const publish      = child<QPushButton>(editor, "tourPreviewAndPublish");
	auto* const problem      = editor.problem_editor();
	auto* const solution     = editor.solution_editor();
	auto* const outline      = editor.outline();

	// An empty tour: three numbered steps, Problem open.
	expect(problemRow->accessibleName().startsWith(QStringLiteral("Step 1, Problem")) && solutionRow->accessibleName().startsWith(QStringLiteral("Step 2, Solution"))
			   && stopsRow->accessibleName().startsWith(QStringLiteral("Step 3, Stops")),
		   "the tour is three numbered steps: Problem, Solution, Stops");
	expect(editor.steps().Current == tour_step::problem && states(editor, state::current, state::to_do, state::to_do), "an empty tour opens on Problem; the others are to do");
	expect(problem->isVisible() && !solution->isVisible() && !outline->isVisible(), "only the open step is open");
	expect(problemNext->shortcut() == qt::shortcut_key(qt::shortcut_command::tour_ready_next) && problemNext->isVisible(), "Next carries the key that marks a stop Ready and goes on");
	expect(qt::drawer_principle_violations(editor).isEmpty(), "the steps keep RFC-BPR-0085's drawer principles: " + qt::drawer_principle_violations(editor).join(QStringLiteral("; ")).toStdString());
	render(editor, prefix, "empty");
	{
		// The narrowest drawer: every control in the open step keeps its words.
		editor.resize(280, 760);
		QApplication::processEvents();
		expect(editor.width() == 280, "the open Problem step fits a 280 pixel drawer (" + std::to_string(editor.width()) + ")");
		for (auto* const button : editor.findChildren<QPushButton*>()) {
			if (button->isVisible() && button->sizeHint().width() > button->width()) {
				expect(false, "at 280 pixels " + button->objectName().toStdString() + " is " + std::to_string(button->width()) + " wide, needs " + std::to_string(button->sizeHint().width()));
			}
		}
		render(editor, prefix, "empty-280");
		editor.resize(380, 760);
		QApplication::processEvents();
	}

	// Writing the Problem leaves it open: moving on is explicit.
	problem->insertPlainText(QStringLiteral("Readers need the change's context before reviewing its details."));
	QApplication::processEvents();
	expect(draft.Problem == "Readers need the change's context before reviewing its details.", "the Problem is saved in the draft");
	expect(editor.steps().Current == tour_step::problem && problem->isVisible() && !problemEdit->isVisible(), "a written Problem stays open until Next");

	// Next: Problem folds to what was written with Change; Solution opens, its text taking the keyboard.
	problemNext->click();
	QApplication::processEvents();
	expect(states(editor, state::done, state::current, state::to_do), "after Next, Problem is done and Solution current");
	expect(!problem->isVisible() && solution->isVisible() && problemEdit->isVisible() && editor.focusWidget() == solution, "Problem folds with Change; the keyboard is in Solution's text");
	expect(problemRow->accessibleDescription().startsWith(QStringLiteral("Readers need")), "the folded step says what was written");
	render(editor, prefix, "problem-done");

	// Change reopens a done step, and never blocks it.
	problemEdit->click();
	QApplication::processEvents();
	expect(problem->isVisible() && !solution->isVisible() && editor.focusWidget() == problem && editor.steps().Current == tour_step::problem, "Change reopens Problem with the keyboard in its text");
	expect(states(editor, state::current, state::to_do, state::to_do), "an unwritten Solution after the open step is still to do");
	solutionRow->click();
	QApplication::processEvents();
	expect(solution->isVisible() && editor.focusWidget() == solution, "a step's header opens it too");

	// Solution, then Next to the stops: no stop yet, so Add stop takes the keyboard.
	solution->insertPlainText(QStringLiteral("Explain the boundary first, then follow the exact source range."));
	QApplication::processEvents();
	solutionNext->click();
	QApplication::processEvents();
	expect(states(editor, state::done, state::done, state::current) && outline->isVisible() && addStop->isVisible() && editor.focusWidget() == addStop,
		   "Next from Solution opens the stops; with none yet, Add stop has the keyboard");
	expect(solutionEdit->isVisible() && problemEdit->isVisible() && !problem->isVisible() && !solution->isVisible(), "both written steps fold, each with Change");

	// A new stop, still untitled, takes the keyboard in its title (BPR-1783).
	draft.Stops      = { stop(1, "Why this boundary matters", core::tour_preparation::ready), stop(2, "", core::tour_preparation::editing) };
	draft.LastStopId = 2;
	++draft.Revision;
	selected = core::tour_stop_id{ 2 };
	++selection;
	show();
	editor.show_stops();
	QApplication::processEvents();
	expect(editor.focusWidget() == editor.stop_title() && editor.stop_title()->isVisible(), "a new stop's title is the stops step's first field");
	expect(states(editor, state::done, state::done, state::current) && editor.steps().Stops == 2U && editor.steps().ReadyStops == 1U, "stops in progress: the step stays open and counts what is ready");
	render(editor, prefix, "stops-in-progress");

	// Folded, the stops say how far along they are; choosing a stop opens them.
	problemEdit->click();
	QApplication::processEvents();
	expect(stopsRow->accessibleDescription() == QStringLiteral("1 of 2 stops ready") && editor.steps().state(tour_step::stops) == state::to_do, "folded, the stops say how many are ready");
	stopsRow->click();
	QApplication::processEvents();
	expect(outline->isVisible() && editor.steps().Current == tour_step::stops, "the stops header opens the route again");

	// Ready to publish: every step done, the stops open, Preview and publish at the foot.
	draft.Stops[1] = stop(2, "Zero width is a real state", core::tour_preparation::ready);
	++draft.Revision;
	show();
	QApplication::processEvents();
	problemEdit->click();
	QApplication::processEvents();
	expect(states(editor, state::current, state::done, state::done) && stopsRow->accessibleDescription() == QStringLiteral("2 of 2 stops ready"), "every stop Ready: the stops step is done");
	stopsRow->click();
	QApplication::processEvents();
	expect(publish->isVisible() && publish->isEnabled(), "Preview and publish stays at the foot");
	render(editor, prefix, "ready-to-publish");

	// A step passed unwritten needs attention, and still opens.
	draft.Problem = { };
	++draft.Revision;
	show();
	QApplication::processEvents();
	expect(states(editor, state::needs_attention, state::done, state::current) && problemRow->accessibleName().endsWith(QStringLiteral("needs attention")), "a Problem passed unwritten needs attention");
	render(editor, prefix, "needs-attention");
	problemRow->click();
	QApplication::processEvents();
	expect(problem->isVisible() && editor.focusWidget() == problem, "and opening it is never blocked");

	// A stop needing the author marks the stops step.
	draft.Problem              = "Readers need the change's context before reviewing its details.";
	draft.Stops[0].Preparation = core::tour_preparation::needs_attention;
	++draft.Revision;
	show();
	QApplication::processEvents();
	expect(editor.steps().state(tour_step::stops) == state::needs_attention, "a stop that needs attention marks step 3");

	// The tour page: the same steps, a header choosing the section.
	editor.resize(1180, 760);
	editor.set_mode(qt::tour_editor_mode::page);
	QApplication::processEvents();
	solutionRow->click();
	QApplication::processEvents();
	expect(problemRow->isVisible() && stopsRow->isVisible() && editor.roadmap()->currentIndex() == 1 && solution->isVisible() && editor.focusWidget() == solution,
		   "on the tour page the steps choose the section, with the keyboard in its field");
	render(editor, prefix, "page");
	editor.set_mode(qt::tour_editor_mode::route);
	QApplication::processEvents();
	expect(solution->isVisible() && editor.steps().Current == tour_step::solution, "back in the drawer, the open step stays open");

	// Another draft opens on its first unwritten step.
	auto other     = tests::review_tour_fixture();
	other.Id       = core::tour_draft_id{ 2 };
	other.Solution = { };
	editor.present({ .Draft = std::make_shared<const core::review_tour>(other) });
	QApplication::processEvents();
	expect(editor.steps().Current == tour_step::solution && solution->isVisible(), "another draft opens on its first step not yet written");

	std::cout << "review-tour-steps:pass\n";
	return EXIT_SUCCESS;
}
