// [[BPR-1900]]. The Tour tab's own agent session: Draft the tour runs in the
// background and is stopped from its own control while the author keeps
// writing (AC3); the transcript opens only when asked (AC4); the whole-tour
// proposal is decided part by part, with the author's edit (AC8).
#include "../support/review_tour_fixture.hpp"
#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/tour_agent_panel.hpp"
#include <QApplication>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <cstdlib>
#include <iostream>
#include <memory>
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
	void render(QWidget& aWidget, const std::string& aPrefix, const char* aName)
	{
		if (aPrefix.empty()) {
			return;
		}
		QApplication::processEvents();
		expect(aWidget.grab().save(QString::fromStdString(aPrefix + aName + ".png")), "render saved");
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	const auto   prefix = argc > 1 ? std::string{ argv[1] } : std::string{ };

	auto draft     = tests::review_tour_fixture();
	draft.Problem  = "The author's own problem.";
	draft.Solution = { };
	qt::review_tour_editor editor;
	editor.set_dispatch([](core::tour_command) { }, [](qt::tour_editor_action) { });
	auto cancelled = 0;
	editor.set_writing_dispatch([](core::writing_hook, qt::prose_capture) { }, [&cancelled] { ++cancelled; });
	auto drafted = 0;
	auto stopped = 0;
	auto decided = std::vector<std::tuple<std::size_t, core::tour_proposal_decision, QString>>{ };
	editor.set_tour_session_dispatch([&drafted] { ++drafted; }, [&stopped] { ++stopped; }, [&decided](const std::size_t aIndex, const core::tour_proposal_decision aDecision, const QString& aText) { decided.emplace_back(aIndex, aDecision, aText); });
	editor.resize(380, 900);
	editor.show();
	editor.present({ .Draft = std::make_shared<const core::review_tour>(draft) });
	editor.show_problem();
	QApplication::processEvents();

	auto* const panel     = editor.tour_agent();
	auto* const draftTour = panel->draft_button();
	expect(draftTour->text() == QStringLiteral("Draft the tour") && draftTour->isEnabled(), "Draft the tour is offered");
	expect(draftTour->palette().color(QPalette::Active, QPalette::ButtonText) == qt::agent_accent(editor.palette()) && !draftTour->icon().isNull(), "it wears the agent accent and spark");
	expect(!panel->transcript()->isVisibleTo(&editor) && !panel->proposal()->isVisibleTo(&editor), "nothing of the session is pushed in front of the author");

	// AC3. In the background: the control says it is running and stops it;
	// the author keeps writing, and leaving the window does not stop it.
	draftTour->click();
	expect(drafted == 1, "Draft the tour asks once");
	auto       session = core::tour_agent_session{ };
	const auto run     = core::begin_tour_run(session, core::tour_agent_request::draft_tour, core::tour_agent_tools::read_only, "Draft the tour");
	editor.set_tour_running(std::nullopt, true);
	editor.present_tour_session(session, std::nullopt);
	expect(draftTour->text() == QStringLiteral("Stop drafting") && draftTour->isEnabled(), "its control says it is running");
	expect(!editor.problem_editor()->isReadOnly(), "the author keeps writing");
	editor.problem_editor()->setPlainText(QStringLiteral("Written while it drafts."));
	expect(editor.problem_editor()->toPlainText() == QStringLiteral("Written while it drafts."), "an edit lands while it drafts");
	render(editor, prefix, "running");
	draftTour->click();
	expect(stopped == 1 && drafted == 1, "pressing it again stops it, from its own control");
	// A Draft from PR context run: its own control says Stop drafting.
	editor.set_tour_running(core::writing_hook::solution, false);
	editor.open_step(core::tour_step::solution);
	QApplication::processEvents();
	auto* const solutionMagic = editor.findChild<QPushButton*>(QStringLiteral("tourSolutionMagic"));
	expect(solutionMagic != nullptr && solutionMagic->text() == QStringLiteral("Stop drafting"), "a field's drafting run is shown on its own control");
	expect(!draftTour->isEnabled(), "the session asks one thing at a time");
	const auto before = cancelled;
	solutionMagic->click();
	expect(cancelled == before + 1, "and stopped from it");
	editor.set_tour_running(std::nullopt, false);

	// AC4. The transcript opens from the Tour tab, and says what it read.
	static_cast<void>(core::record_tour_activity(session, *run, { { core::tour_transcript_kind::searched, "rg -n draw_scene src" }, { core::tour_transcript_kind::read, "src/renderer.cpp" } }));
	static_cast<void>(core::finish_tour_run(session, *run, core::tour_agent_run_state::answered, "Proposed a Problem and 2 stops."));

	// AC8. The proposal: Problem (replacing the author's), and two stops,
	// one of which does not anchor.
	const auto files  = std::vector<core::tour_anchor_file>{ { "src/renderer.cpp", true, 40U } };
	const auto answer = core::tour_draft_answer{
		.Problem = std::string{ "Two views drew over each other." },
		.Stops   = { { "src/renderer.cpp", 2U, 5U, "Where drawing starts", "draw() is the one entry point." }, { "src/elsewhere.cpp", 1U, 2U, "Elsewhere", "Not here." } },
	};
	auto proposal = core::propose_tour(*run, answer, files, draft);
	editor.present_tour_session(session, proposal);
	expect(panel->transcript_button()->isEnabled() && panel->transcript_button()->text() == QStringLiteral("Transcript (1)"), "the transcript can be opened");
	expect(!panel->transcript()->isVisibleTo(&editor), "it is not opened for the author");
	panel->transcript_button()->click();
	expect(panel->transcript()->isVisibleTo(&editor), "it opens from the Tour tab");
	const auto said = panel->transcript_text();
	expect(said.contains(QStringLiteral("Searched rg -n draw_scene src")) && said.contains(QStringLiteral("Read src/renderer.cpp")) && said.contains(QStringLiteral("Answered Proposed a Problem and 2 stops.")), "it says what it searched, read and answered: " + said.toStdString());
	render(editor, prefix, "transcript");
	panel->transcript_button()->click();

	expect(panel->proposal()->isVisibleTo(&editor), "the proposal shows where it was asked for");
	expect(panel->accept_button(0) != nullptr && panel->accept_button(0)->isEnabled(), "the Problem can be accepted");
	expect(panel->accept_button(1) != nullptr && panel->accept_button(1)->isEnabled(), "an anchored stop can be accepted");
	expect(panel->accept_button(2) != nullptr && !panel->accept_button(2)->isEnabled() && !panel->proposal_editor(2)->isEnabled(), "a stop that does not anchor cannot be accepted");
	panel->proposal_editor(0)->setPlainText(QStringLiteral("Edited by the author."));
	// A presentation of the same proposal keeps what the author typed.
	editor.present_tour_session(session, proposal);
	expect(panel->proposal_editor(0)->toPlainText() == QStringLiteral("Edited by the author."), "an edit in a pending part survives");
	render(editor, prefix, "proposal");
	panel->accept_button(0)->click();
	expect(decided.size() == 1U && std::get<0>(decided[0]) == 0U && std::get<1>(decided[0]) == core::tour_proposal_decision::accepted && std::get<2>(decided[0]) == QStringLiteral("Edited by the author."), "Accept sends the author's edit");
	auto* const discard = panel->proposal_items()->findChildren<QPushButton*>(QStringLiteral("tourProposalDiscard")).at(2);
	discard->click();
	expect(decided.size() == 2U && std::get<0>(decided[1]) == 2U && std::get<1>(decided[1]) == core::tour_proposal_decision::discarded, "Discard sends the part's index");
	// Owner look v001, rejected: "the first click on accept highlights a box in
	// the pane and doesnt click accept and that box makes the text feel
	// cramped". One real click, while a proposed field holds the keyboard,
	// accepts; it moves no focus, and focusing a field shrinks nothing.
	auto* const field = panel->proposal_editor(1);
	editor.activateWindow();
	for (int settle = 0; settle < 5; ++settle) {
		QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
		QApplication::processEvents();
	}
	const auto rest = field->contentsRect();
	field->setFocus(Qt::MouseFocusReason);
	QApplication::processEvents();
	expect(field->hasFocus() && field->contentsRect() == rest, "focusing a proposed field keeps its room for the text");
	expect(!field->testAttribute(Qt::WA_MacShowFocusRect), "no native focus ring is drawn inside the field");
	expect(panel->proposal_items()->parentWidget()->parentWidget()->focusPolicy() == Qt::NoFocus, "the proposal pane never takes the keyboard");
	auto* const accept = panel->accept_button(1);
	expect(accept->focusPolicy() == Qt::TabFocus, "Accept is reached with Tab, not focused by a press");
	const auto centre    = accept->rect().center();
	auto       press     = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ centre }, accept->mapToGlobal(QPointF{ centre }), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
	auto       release   = QMouseEvent{ QEvent::MouseButtonRelease, QPointF{ centre }, accept->mapToGlobal(QPointF{ centre }), Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
	const auto decisions = decided.size();
	QApplication::sendEvent(accept, &press);
	expect(field->hasFocus(), "the press leaves the keyboard where it was");
	QApplication::sendEvent(accept, &release);
	expect(decided.size() == decisions + 1U && std::get<0>(decided.back()) == 1U && std::get<1>(decided.back()) == core::tour_proposal_decision::accepted, "one click on Accept accepts");
	std::cout << "tour-agent-session-view:pass\n";
	return EXIT_SUCCESS;
}
