// [[BPR-1900]]. The Tour tab's own agent session: Draft the tour runs in the
// background and is stopped from its own control while the author keeps
// writing (AC3); the transcript opens only when asked (AC4); the whole-tour
// proposal is decided part by part, with the author's edit (AC8).
#include "../support/review_tour_fixture.hpp"
#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/skill_completer.hpp"
#include "bpr/qt/tour_agent_panel.hpp"
#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocumentFragment>
#include <QTextEdit>
#include <QTextList>
#include <QVBoxLayout>
#include <algorithm>
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
	/// [[BPR-1932]]. A scroll area asks about widths before applying one.
	/// The answer must not depend on whether the header is currently wrapped.
	void header_wrap_measurements()
	{
		auto scroll = QScrollArea{ };
		scroll.setWidgetResizable(true);
		auto* const contents = new QWidget{ };
		auto* const column   = new QVBoxLayout{ contents };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(0);
		auto* const panel = new qt::tour_agent_panel{ contents };
		column->addWidget(panel);
		auto* const rest = new QWidget{ contents };
		rest->setFixedHeight(400);
		column->addWidget(rest);
		scroll.setWidget(contents);
		scroll.resize(900, 500);
		scroll.show();
		QApplication::processEvents();
		const auto        margins     = panel->layout()->contentsMargins();
		const auto* const trail       = panel->chat_button()->parentWidget();
		const auto        boundary    = panel->draft_button()->sizeHint().width() + 8 + trail->sizeHint().width() + margins.left() + margins.right();
		const auto        narrow      = boundary - 1;
		const auto        wide        = boundary + 30;
		const auto        shortHeight = panel->heightForWidth(wide);
		const auto        tallHeight  = panel->heightForWidth(narrow);
		expect(panel->hasHeightForWidth() && tallHeight >= shortHeight + trail->sizeHint().height(), "the header declares its extra row before it is resized: " + std::to_string(shortHeight) + ", " + std::to_string(tallHeight));
		const auto minimum = panel->minimumSizeHint();
		scroll.resize(wide, 500);
		scroll.show();
		for (const auto width : { narrow, wide, narrow, wide }) {
			for (int height = 430; height <= 500; height += 5) {
				scroll.resize(width + 2 * scroll.frameWidth(), height);
				for (int turn = 0; turn < 4; ++turn) {
					QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
					QApplication::processEvents();
				}
				expect(panel->heightForWidth(wide) == shortHeight && panel->heightForWidth(narrow) == tallHeight && panel->minimumSizeHint() == minimum, "width negotiation keeps stable hints across wrapping and scrollbar changes");
				const auto before = contents->size();
				QApplication::processEvents();
				expect(contents->size() == before, "the scroll column settles at the header's wrap boundary");
			}
		}
	}

	void render(QWidget& aWidget, const std::string& aPrefix, const char* aName)
	{
		if (aPrefix.empty()) {
			return;
		}
		QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		QApplication::processEvents();
		expect(aWidget.grab().save(QString::fromStdString(aPrefix + aName + ".png")), "render saved");
	}
}

namespace {
	/// [[BPR-1915]]. A proposed Solution and a stop's new explanation render
	/// their Markdown; the source stays editable, and Accept sends it.
	void formatted(qt::review_tour_editor& aEditor, const std::string& aPrefix)
	{
		auto* const panel   = aEditor.tour_agent();
		auto        decided = std::vector<std::tuple<std::size_t, core::tour_proposal_decision, QString>>{ };
		aEditor.set_tour_session_dispatch([] { }, [] { }, [&decided](const std::size_t aIndex, const core::tour_proposal_decision aDecision, const QString& aText) { decided.emplace_back(aIndex, aDecision, aText); });
		auto       tour     = tests::review_tour_fixture();
		auto       session  = core::tour_agent_session{ };
		const auto run      = core::begin_tour_run(session, core::tour_agent_request::draft_tour, core::tour_agent_tools::read_only, "Draft the tour");
		const auto answer   = core::tour_ask_answer{ .Reply = "Done.", .Changes = { .Solution = std::string{ "Each view clears its own target.\n\n- `clear()` takes the view.\n- `draw()` draws only it." } }, .StopTexts = { { 1U, "Read `begin_frame()` first." } } };
		const auto proposal = core::propose_tour_changes(*run, answer, { }, tour);
		aEditor.present_tour_session(session, proposal);
		// [[BPR-1925]] v003. The proposed Solution waits on the Solution.
		aEditor.open_step(core::tour_step::solution);
		QApplication::processEvents();
		auto* const reader = panel->proposal_reader(0);
		expect(reader != nullptr && reader->isVisibleTo(&aEditor), "the proposed Solution shows rendered");
		auto lists = 0;
		for (auto block = reader->rendered_view()->document()->begin(); block.isValid(); block = block.next()) {
			lists += block.textList() != nullptr ? 1 : 0;
		}
		expect(lists == 2 && !reader->rendered_view()->toPlainText().contains(QLatin1Char('`')), "its list is a list and its code is not backticks");
		expect(panel->proposal_editor(0)->toPlainText().contains(QStringLiteral("- `clear()` takes the view.")), "the Markdown source is kept to edit");
		expect(proposal.Items.size() == 2U && proposal.Items[1].Part == core::tour_proposal_part::stop_text && panel->proposal_reader(1) != nullptr, "a stop's new explanation is proposed too");
		render(aEditor, aPrefix, "formatted-proposal");
		panel->accept_button(0)->click();
		expect(!decided.empty() && std::get<2>(decided.back()).startsWith(QStringLiteral("Each view clears its own target.\n\n- `clear()`")), "Accept sends the Markdown source");
		aEditor.present_tour_session(session, std::nullopt);
	}
}

namespace {
	/// [[BPR-1916]]. `/name` completion in a side chat's composer
	/// ([[BPR-1914]] v003): the rows say where each skill came from, Return
	/// chooses a row while the list is open and only then sends, with the
	/// name in the question.
	void skills_in_the_ask_box(qt::review_tour_editor& aEditor)
	{
		auto* const chat  = aEditor.side_chat();
		auto        asked = std::vector<QString>{ };
		aEditor.set_tour_ask_dispatch([&asked](const qt::tour_ask& aAsk) { asked.push_back(aAsk.Question); });
		chat->present_skills({ { .Name = "fixture-technical-writer", .Description = "Short paragraphs.", .Source = core::skill_source::personal, .Origin = "~/.claude/skills" },
							   { .Name = "team-voice", .Description = "The team's.", .Source = core::skill_source::repository, .Origin = "this pull request's repository, .claude/skills at base 1a2b3c4" } });
		chat->open(core::tour_thread{ .Kind = core::tour_thread_kind::solution });
		chat->show();
		chat->activateWindow();
		chat->composer()->setFocus();
		QApplication::processEvents();
		expect(chat->skill_completion() != nullptr, "the composer completes skills");
		chat->composer()->clear();
		chat->composer()->insertPlainText(QStringLiteral("Rewrite the Solution as /fix"));
		QApplication::processEvents();
		const auto rows = chat->skill_completion()->rows();
		expect(rows == QStringList{ QStringLiteral("/fixture-technical-writer (Personal, ~/.claude/skills)") }, "completion lists the matching skill with its source");
		auto enter = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
		QApplication::sendEvent(chat->composer(), &enter);
		expect(asked.empty() && chat->composer()->toPlainText() == QStringLiteral("Rewrite the Solution as /fixture-technical-writer "), "Return chooses the row and does not send yet");
		chat->composer()->insertPlainText(QStringLiteral("says."));
		QApplication::processEvents();
		QApplication::sendEvent(chat->composer(), &enter);
		expect(asked.size() == 1U && asked[0] == QStringLiteral("Rewrite the Solution as /fixture-technical-writer says."), "the question carries the skill's name");
		chat->hide();
	}
}

namespace {
	auto accept_named(QWidget& aRoot, const QString& aWords) -> QPushButton*
	{
		for (auto* const button : aRoot.findChildren<QPushButton*>(QStringLiteral("tourProposalAccept"))) {
			if (button->text() == aWords) {
				return button;
			}
		}
		return nullptr;
	}

	/// [[BPR-1930]] AC1-AC3, [[BPR-1931]] AC1. The tour's own chat proposes
	/// the tour's shape: the stops it would remove, merge or move are marked
	/// grey on the route, each part's box waits on the stop it changes with
	/// Remove, Merge or Reorder at its right, the focus waits under the
	/// panel's line and then shows as a chip; feedback is taken while the
	/// draft runs; Reset tour asks first and offers Undo. Nothing is decided
	/// here: every gesture is dispatched.
	void whole_tour(const std::string& aPrefix)
	{
		auto        tour  = tests::review_tour_fixture();
		const auto& first = *tour.Stops.front().Original;
		for (std::uint64_t id = 2U; id <= 4U; ++id) {
			auto source       = first;
			source.First.Line = static_cast<std::uint32_t>(id * 10U);
			source.Last.Line  = static_cast<std::uint32_t>(id * 10U + 3U);
			auto stop         = core::tour_stop{ };
			stop.Id           = core::tour_stop_id{ id };
			stop.Title        = "Stop " + std::to_string(id);
			stop.Body         = "Body " + std::to_string(id);
			stop.Original     = source;
			stop.Mapping      = core::tour_mapping::exact_original;
			tour.Stops.push_back(std::move(stop));
		}
		tour.LastStopId = 4U;
		qt::review_tour_editor editor;
		editor.set_dispatch([](core::tour_command) { }, [](qt::tour_editor_action) { });
		auto decided = std::vector<std::tuple<std::size_t, core::tour_proposal_decision, QString>>{ };
		editor.set_tour_session_dispatch([] { }, [] { }, [&decided](const std::size_t aIndex, const core::tour_proposal_decision aDecision, const QString& aText) { decided.emplace_back(aIndex, aDecision, aText); });
		auto asked = std::vector<qt::tour_ask>{ };
		editor.set_tour_ask_dispatch([&asked](const qt::tour_ask& aAsk) { asked.push_back(aAsk); });
		auto cleared = 0, resets = 0, undone = 0;
		editor.set_tour_shape_dispatch([&cleared] { ++cleared; }, [&resets] { ++resets; }, [&undone] { ++undone; });
		editor.resize(420, 1100);
		editor.show();
		const auto shared = std::make_shared<const core::review_tour>(tour);
		editor.present({ .Draft = shared });
		editor.show_stops();
		QApplication::processEvents();
		auto* const panel = editor.tour_agent();

		// AC3. While Draft the tour runs, the tour's chat still takes feedback.
		editor.open_side_chat(core::tour_thread{ });
		editor.set_tour_running(std::nullopt, true);
		auto* const chat = editor.side_chat();
		chat->composer()->setPlainText(QStringLiteral("Fewer stops"));
		expect(chat->send_button()->isEnabled(), "AC3 the tour's chat takes feedback while the draft runs");
		chat->send_button()->click();
		expect(asked.size() == 1U && asked.back().Thread == core::tour_thread{ } && asked.back().Question == QStringLiteral("Fewer stops"), "the feedback is dispatched for the tour as a whole");
		auto       session  = core::tour_agent_session{ };
		const auto drafting = core::begin_tour_run(session, core::tour_agent_request::draft_tour, core::tour_agent_tools::read_only, "Draft the tour");
		expect(core::queue_tour_feedback(session, "Fewer stops"), "the host keeps it");
		editor.present_tour_session(session, std::nullopt);
		QApplication::processEvents();
		expect(chat->waiting_feedback() != nullptr && !chat->waiting_feedback()->isHidden() && chat->waiting_feedback()->findChild<QLabel*>()->text() == QStringLiteral("Fewer stops"), "the waiting feedback shows in the tour's chat");
		render(editor, aPrefix, "whole-tour-feedback-waiting");
		static_cast<void>(core::finish_tour_run(session, *drafting, core::tour_agent_run_state::answered, "Proposed."));
		static_cast<void>(core::take_tour_feedback(session));
		editor.set_tour_running(std::nullopt, false);
		chat->composer()->setPlainText(QStringLiteral("Anything"));
		editor.open_side_chat(core::tour_thread{ .Kind = core::tour_thread_kind::solution });
		editor.set_tour_running(std::nullopt, true);
		expect(!chat->send_button()->isEnabled(), "a part's chat still waits for the draft");
		editor.set_tour_running(std::nullopt, false);
		editor.open_side_chat(core::tour_thread{ });

		// AC1. "Too many stops, merge 2 and 3, focus on the renderer".
		const auto turn     = core::begin_tour_ask(session, core::tour_thread{ }, std::nullopt, "Too many stops. Merge 2 and 3, and focus on the renderer.");
		auto       answer   = core::tour_ask_answer{ };
		answer.Reply        = "Fewer stops.";
		auto revision       = core::tour_revision{ };
		revision.Remove     = { 4U };
		revision.Merges     = { { .Stops = { 2U, 3U }, .Title = "Both", .Explanation = "One stop for `both`." } };
		revision.Focus      = "the renderer";
		answer.Revision     = std::move(revision);
		const auto proposal = core::propose_tour_changes(*turn, answer, { }, tour);
		static_cast<void>(core::answer_tour_ask(session, *turn, answer.Reply, core::tally_of(proposal)));
		expect(proposal.Items.size() == 2U && proposal.Focus == "the renderer", "a merge and a removal are proposed; the focus is no part to accept");
		// [[BPR-1930]] follow-up. The host applies the focus at once.
		expect(core::apply_tour_focus(session, *turn, proposal.Focus), "the host applies the focus");
		editor.present_tour_session(session, proposal);
		QApplication::processEvents();
		const auto mark = [&editor](const int aRow) { return editor.route_mark(aRow); };
		expect(mark(0).first == 0 && mark(1) == std::pair{ 2, QStringLiteral("Merge with 3") } && mark(2) == std::pair{ 2, QStringLiteral("Merges into 2") } && mark(3) == std::pair{ 1, QStringLiteral("Remove") }, "the stops it changes are marked on the route, in place");
		expect(chat->in_tour_line() != nullptr && chat->in_tour_line()->text().startsWith(QStringLiteral("2 changes wait in the tour")), "the tour's chat says where its proposal waits");
		expect(chat->findChildren<QPushButton*>(QStringLiteral("tourSideChatAccept")).isEmpty(), "nothing of it is decided in the chat");
		expect(panel->proposal_title()->text().contains(QStringLiteral("2 parts wait in the tour")), "the panel's one line counts them");
		expect(accept_named(editor, QStringLiteral("Set focus")) == nullptr && accept_named(*panel, QStringLiteral("Accept")) == nullptr, "no Set focus to press: the focus is not a proposal");
		expect(panel->focus_chip()->isVisibleTo(&editor) && panel->focus_text()->text() == QStringLiteral("Focus: the renderer") && panel->focus_chip()->toolTip().startsWith(QStringLiteral("Later drafts and chat answers favour this part")), "the chip shows at once and says what it does");
		auto* const focusLine = chat->focus_line();
		expect(focusLine != nullptr && focusLine->text() == QStringLiteral("Focus set to \u201Cthe renderer\u201D \u00B7 <a href=\"#undo\">Undo</a>"), "the chat answer says the focus was set, with Undo: " + (focusLine == nullptr ? std::string{ } : focusLine->text().toStdString()));
		auto undoneRun = std::optional<core::tour_agent_run_id>{ };
		chat->set_focus_undo_dispatch([&undoneRun](const core::tour_agent_run_id aRun) { undoneRun = aRun; });
		emit focusLine->linkActivated(QStringLiteral("#undo"));
		expect(undoneRun == turn, "Undo asks the host to undo that turn's focus");
		expect(core::undo_tour_focus(session, *turn), "the host undoes it");
		editor.present_tour_session(session, proposal);
		QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		QApplication::processEvents();
		expect(!panel->focus_chip()->isVisibleTo(&editor) && chat->focus_line() != nullptr && chat->focus_line()->text().endsWith(QStringLiteral(", then undone.")), "undone, the chip goes and the line says so");
		editor.go_to_stop(core::tour_stop_id{ 2 });
		QApplication::processEvents();
		auto* const merge = accept_named(editor, QStringLiteral("Merge"));
		expect(merge != nullptr && merge->isVisibleTo(&editor), "the merge waits on stop 2's card, Merge at its right");
		expect(merge->parentWidget()->findChildren<QLabel*>().size() > 2U && merge->parentWidget()->findChild<QLabel*>()->text() == QStringLiteral("Merge stops 2 and 3 into Both"), "it names the stops and the merged stop");
		auto* const complete = editor.findChild<QPushButton*>(QStringLiteral("tourComplete"));
		expect(complete == nullptr || !complete->isVisibleTo(&editor), "Ready, next gives way to it");
		render(editor, aPrefix, "whole-tour-merge");
		editor.go_to_stop(core::tour_stop_id{ 4 });
		QApplication::processEvents();
		auto* const remove = accept_named(editor, QStringLiteral("Remove"));
		expect(remove != nullptr && remove->isVisibleTo(&editor) && (merge == nullptr || !accept_named(editor, QStringLiteral("Merge"))->isVisibleTo(&editor)), "the removal waits on stop 4's card, the merge with its own");
		remove->click();
		expect(!decided.empty() && std::get<1>(decided.back()) == core::tour_proposal_decision::accepted && proposal.Items[std::get<0>(decided.back())].Part == core::tour_proposal_part::remove_stop, "Remove dispatches the removal's acceptance");

		// AC2. The focus the host remembered: a chip, and its × asks to clear it.
		static_cast<void>(core::set_tour_focus(session, "the renderer"));
		editor.present_tour_session(session, std::nullopt);
		QApplication::processEvents();
		expect(panel->focus_chip()->isVisibleTo(&editor) && panel->focus_text()->text() == QStringLiteral("Focus: the renderer"), "AC2 the focus shows as a chip");
		expect(mark(1).first == 0 && mark(3).first == 0, "with nothing waiting, no stop is marked");
		render(editor, aPrefix, "whole-tour-focus");
		panel->focus_clear_button()->click();
		expect(cleared == 1, "its × asks the host to clear it");

		// A reorder: the new order above the route, the moved stops marked.
		auto reorder       = core::tour_ask_answer{ };
		reorder.Reply      = "Start at 3.";
		auto order         = core::tour_revision{ };
		order.Order        = { 1U, 3U, 2U, 4U };
		reorder.Revision   = std::move(order);
		const auto ordered = core::propose_tour_changes(*turn, reorder, { }, tour);
		editor.present_tour_session(session, ordered);
		QApplication::processEvents();
		expect(editor.proposed_order()->isVisibleTo(&editor) && accept_named(*editor.proposed_order(), QStringLiteral("Reorder")) != nullptr, "the new order waits above the route, Reorder at its right");
		expect(mark(1) == std::pair{ 4, QStringLiteral("Moves to 3") } && mark(2) == std::pair{ 4, QStringLiteral("Moves to 2") } && mark(0).first == 0, "the moved stops are marked");
		expect(editor.proposed_order()->findChildren<QLabel*>(QStringLiteral("tourProposedOrderMoved")).size() == 2U, "the order lists the moved stops");
		render(editor, aPrefix, "whole-tour-reorder");
		editor.present_tour_session(session, std::nullopt);
		QApplication::processEvents();
		expect(!editor.proposed_order()->isVisibleTo(&editor), "decided, the order goes");

		// [[BPR-1931]] AC1. Reset tour asks in a plain sentence; Keep the tour
		// changes nothing; Reset tour dispatches; Undo reset is offered.
		auto tally      = core::reset_tally_of(tour, session, std::nullopt);
		tally.Published = true;
		editor.present_tour_reset(tally, true, false);
		auto* const reset = panel->reset_button();
		expect(reset->isEnabled() && !panel->reset_confirm()->isVisibleTo(&editor), "Reset tour is offered, and asks nothing yet");
		// [[BPR-1931]] v002, owner on v001: "the reset tour button can get
		// smaller like it is on agents so that top row doesn't have to wrap so
		// early". The narrowest Tour tab at which Draft the tour, Reset tour,
		// Conversation and the chat toggle still share one row.
		{
			const auto oneRow = [panel] {
				QApplication::processEvents();
				return panel->chat_button()->mapTo(panel, QPoint{ }).y() < panel->draft_button()->mapTo(panel, QPoint{ }).y() + panel->draft_button()->height();
			};
			auto widest = 900;
			auto narrow = 200;
			while (widest - narrow > 1) {
				const auto middle = (widest + narrow) / 2;
				editor.resize(middle, 1100);
				(oneRow() ? widest : narrow) = middle;
			}
			std::cout << "TOUR_HEAD_ONE_ROW_FROM:" << widest << '\n';
			// v001's worded button needed 409 pixels here.
			expect(widest <= 380, "v002 the header keeps one row down to a narrower tab: " + std::to_string(widest));
			expect(reset->toolButtonStyle() == Qt::ToolButtonIconOnly && reset->text().isEmpty() && !reset->icon().isNull() && reset->autoRaise(), "Reset tour is an icon button, as the Agent tab's header actions are");
			expect(reset->accessibleName() == QStringLiteral("Reset tour") && reset->toolTip().startsWith(QStringLiteral("Reset tour\n")), "it is named for the keyboard, screen readers and its tooltip");
			editor.resize(420, 1100);
			QApplication::processEvents();
		}
		reset->click();
		expect(panel->reset_confirm()->isVisibleTo(&editor), "it asks first");
		const auto sentence = panel->reset_sentence()->text();
		expect(sentence.startsWith(QStringLiteral("Reset clears the Problem, the Solution, all 4 stops, 1 chat and the focus.")) && sentence.contains(QStringLiteral("only your local draft")), "the question names what is lost, and that GitHub's tour stays: " + sentence.toStdString());
		render(editor, aPrefix, "reset-question");
		panel->reset_cancel_button()->click();
		expect(resets == 0 && !panel->reset_confirm()->isVisibleTo(&editor), "Keep the tour changes nothing");
		reset->click();
		panel->reset_confirm_button()->click();
		expect(resets == 1, "Reset tour asks the host to reset");
		editor.present_tour_reset(core::tour_reset_tally{ }, true, true);
		expect(panel->reset_done()->isVisibleTo(&editor) && !reset->isEnabled(), "Undo reset is offered, and an empty tour has nothing to reset");
		render(editor, aPrefix, "reset-done");
		panel->undo_reset_button()->click();
		expect(undone == 1, "Undo reset asks the host to restore it");
		editor.present_tour_reset(tally, true, false);
		expect(!panel->reset_done()->isVisibleTo(&editor), "once the tour changes, Undo reset goes");
		editor.set_tour_running(std::nullopt, true);
		expect(!reset->isEnabled(), "nothing is reset while the agent runs");
		editor.set_tour_running(std::nullopt, false);
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
	auto drafted   = 0;
	auto stopped   = 0;
	auto decided   = std::vector<std::tuple<std::size_t, core::tour_proposal_decision, QString>>{ };
	auto relocated = std::vector<std::size_t>{ };
	editor.set_tour_session_dispatch([&drafted] { ++drafted; }, [&stopped] { ++stopped; }, [&decided](const std::size_t aIndex, const core::tour_proposal_decision aDecision, const QString& aText) { decided.emplace_back(aIndex, aDecision, aText); }, [&relocated](const std::size_t aIndex) { relocated.push_back(aIndex); });
	editor.resize(380, 900);
	editor.show();
	editor.present({ .Draft = std::make_shared<const core::review_tour>(draft) });
	editor.show_problem();
	QApplication::processEvents();

	auto* const panel     = editor.tour_agent();
	auto* const draftTour = panel->draft_button();
	expect(draftTour->text() == QStringLiteral("Draft Tour") && draftTour->isEnabled(), "Draft Tour is offered");
	expect(draftTour->palette().color(QPalette::Active, QPalette::ButtonText) == qt::agent_accent(editor.palette()) && !draftTour->icon().isNull(), "it wears the agent accent and spark");
	expect(!panel->transcript()->isVisible() && !panel->proposal()->isVisibleTo(&editor), "nothing of the session is pushed in front of the author");
	// [[BPR-1913]] AC4, owner 2026-11-06: "the button at the top looks like
	// a tab, except they're not tabs". Draft the tour is one bordered action
	// at its own width; Transcript is bare, never checkable, apart from it.
	expect(!draftTour->isFlat() && !draftTour->isCheckable() && draftTour->sizePolicy().horizontalPolicy() == QSizePolicy::Fixed, "Draft the tour is a button at its own width");
	expect(panel->transcript_button()->isFlat() && !panel->transcript_button()->isCheckable() && !panel->transcript_button()->icon().isNull(), "Transcript is a bare action with a glyph, not a toggle");
	for (auto* const button : panel->findChildren<QPushButton*>()) {
		expect(!button->isCheckable(), "no control of the session is a checkable segment");
	}

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
	const auto files  = std::vector<core::tour_anchor_file>{ { "src/renderer.cpp", true, 40U, std::nullopt } };
	const auto answer = core::tour_draft_answer{
		.Problem = std::string{ "Two views drew over each other." },
		.Stops   = { { "src/renderer.cpp", 2U, 5U, "Where drawing starts", "draw() is the one entry point." }, { "src/elsewhere.cpp", 1U, 2U, "Elsewhere", "Not here." }, { "src/renderer.cpp", 10U, 12U, "Resize keeps its size", "resize() stores what it was given." } },
	};
	auto proposal = core::propose_tour(*run, answer, files, draft);
	static_cast<void>(core::note_finished_tour_run(session, *run, core::left_out_entries(proposal)));
	editor.present_tour_session(session, proposal);
	expect(panel->transcript_button()->isEnabled() && panel->transcript_button()->text() == QStringLiteral("Conversation (1)"), "the transcript can be opened, as the conversation");
	expect(!panel->transcript()->isVisible(), "it is not opened for the author");
	panel->transcript_button()->click();
	// [[BPR-1913]] AC1. Its own window: nothing of it in the Tour tab's column.
	expect(panel->transcript()->isVisible() && panel->transcript()->isWindow(), "it opens from the Tour tab, in its own window");
	expect(editor.findChildren<QTextBrowser*>().size() == 1U && panel->transcript_view()->window() != editor.window(), "the editing column holds no transcript");
	const auto said = panel->transcript_text();
	expect(said.contains(QStringLiteral("Searched rg -n draw_scene src")) && said.contains(QStringLiteral("Read src/renderer.cpp")) && said.contains(QStringLiteral("Answered Proposed a Problem and 2 stops.")), "it says what it searched, read and answered: " + said.toStdString());
	expect(said.contains(QStringLiteral("Left out \"Elsewhere\", src/elsewhere.cpp, lines 1 to 2: not a file of this pull request")), "it keeps the stop that was left out: " + said.toStdString());
	panel->transcript()->hide();

	expect(panel->proposal()->isVisibleTo(&editor), "the proposal shows where it was asked for");
	expect(panel->accept_button(0) != nullptr && panel->accept_button(0)->isEnabled(), "the Problem can be accepted");
	expect(panel->accept_button(1) != nullptr && panel->accept_button(1)->isEnabled(), "an anchored stop can be accepted");
	// Owner look v002 ("What's up with this?"): the stop outside the pull
	// request is not shown; one quiet line says so; nothing is disabled.
	expect(panel->accept_button(2) != nullptr && panel->accept_button(2)->isEnabled() && panel->proposal_editor(2)->isEnabled() && panel->accept_button(3) == nullptr, "only parts that can be accepted are shown");
	// [[BPR-1925]] v003. The parts wait in the tour itself, not in a list.
	expect(panel->proposal_items()->findChildren<QWidget*>(QStringLiteral("tourProposalItem")).isEmpty(), "no list of parts in the panel");
	expect(editor.findChild<QWidget*>(QStringLiteral("tourProposalFlag")) == nullptr, "no red flag");
	for (auto* const button : editor.findChildren<QPushButton*>(QStringLiteral("tourProposalAccept"))) {
		expect(button->isEnabled(), "no disabled button in the proposal");
	}
	expect(panel->left_out()->isVisibleTo(&editor) && panel->left_out()->text() == QStringLiteral("1 suggested stop pointed outside this pull request and was left out."), "one quiet line says one stop was left out");
	expect(panel->left_out()->foregroundRole() == QPalette::PlaceholderText, "in the quiet colour");
	panel->proposal_editor(0)->setPlainText(QStringLiteral("Edited by the author."));
	// A presentation of the same proposal keeps what the author typed.
	editor.present_tour_session(session, proposal);
	expect(panel->proposal_editor(0)->toPlainText() == QStringLiteral("Edited by the author."), "an edit in a pending part survives");
	render(editor, prefix, "proposal");
	panel->accept_button(0)->click();
	expect(decided.size() == 1U && std::get<0>(decided[0]) == 0U && std::get<1>(decided[0]) == core::tour_proposal_decision::accepted && std::get<2>(decided[0]) == QStringLiteral("Edited by the author."), "Accept sends the author's edit");
	auto* const discard = panel->accept_button(2)->parentWidget()->findChild<QPushButton*>(QStringLiteral("tourProposalDiscard"));
	discard->click();
	expect(decided.size() == 2U && std::get<0>(decided[1]) == 2U && std::get<1>(decided[1]) == core::tour_proposal_decision::discarded, "Discard sends the part's index");
	// Owner look v001, rejected: "the first click on accept highlights a box in
	// the pane and doesnt click accept and that box makes the text feel
	// cramped". One real click, while a proposed field holds the keyboard,
	// accepts; it moves no focus, and focusing a field shrinks nothing.
	// [[BPR-1925]] v003. The proposed stop waits on the route: the Stops step.
	editor.show_stops();
	auto* const field = panel->proposal_editor(1);
	editor.activateWindow();
	for (int settle = 0; settle < 5; ++settle) {
		QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
		QApplication::processEvents();
	}
	// [[BPR-1915]]. The source shows on Edit text; rendered until then.
	expect(panel->proposal_reader(1) != nullptr && panel->proposal_reader(1)->isVisibleTo(&editor) && !field->isVisibleTo(&editor), "a proposed text shows rendered first");
	panel->edit_button(1)->click();
	expect(!panel->proposal_reader(1)->isVisibleTo(&editor) && field->isVisibleTo(&editor) && panel->edit_button(1)->text() == QStringLiteral("Show formatted"), "Edit text shows its Markdown source");
	for (int settle = 0; settle < 5; ++settle) {
		QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
		QApplication::processEvents();
	}
	const auto rest = field->contentsRect();
	field->setFocus(Qt::MouseFocusReason);
	QApplication::processEvents();
	expect(field->hasFocus() && field->contentsRect() == rest, "focusing a proposed field keeps its room for the text");
	expect(!field->testAttribute(Qt::WA_MacShowFocusRect), "no native focus ring is drawn inside the field");
	expect(panel->accept_button(1)->parentWidget()->focusPolicy() == Qt::NoFocus, "the proposed part never takes the keyboard itself");
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
	// The plural, and nothing at all when none were left out.
	auto twice = answer;
	twice.Stops.push_back({ "src/other.cpp", 1U, 1U, "Other", "Also not here." });
	editor.present_tour_session(session, core::propose_tour(core::tour_agent_run_id{ 9 }, twice, files, draft));
	expect(panel->left_out()->text() == QStringLiteral("2 suggested stops pointed outside this pull request and were left out."), "the plural for more");
	auto none = answer;
	none.Stops.erase(none.Stops.begin() + 1);
	editor.present_tour_session(session, core::propose_tour(core::tour_agent_run_id{ 10 }, none, files, draft));
	expect(!panel->left_out()->isVisibleTo(&editor), "nothing when none were left out");
	// [[BPR-1900]], owner 2026-11-06: "strongly prefer on hunk". A stop
	// near the change wears a small quiet label; one off it says plainly it
	// won't publish and offers Move to nearest change; none is disabled.
	auto hunked       = files;
	hunked[0].Hunks   = std::vector<core::tour_line_span>{ { 2U, 8U } };
	const auto placed = core::tour_draft_answer{ .Stops = { { "src/renderer.cpp", 20U, 24U, "Off", "Off the change." }, { "src/renderer.cpp", 12U, 12U, "Near", "Near it." }, { "src/renderer.cpp", 3U, 5U, "On", "On it." } } };
	auto       shown  = core::propose_tour(core::tour_agent_run_id{ 11 }, placed, hunked, draft);
	editor.present_tour_session(session, shown);
	expect(panel->placement_text(0).isEmpty() && panel->relocate_button(0) == nullptr, "(a) a stop on the change says nothing more");
	expect(panel->placement_text(1) == QStringLiteral("Outside the change") && panel->relocate_button(1) == nullptr, "(b) a stop near the change wears one quiet label");
	expect(panel->placement_text(2).startsWith(QStringLiteral("Outside the change. It won't appear when the tour is published")) && panel->relocate_button(2) != nullptr, "(c) a stop off the change says it won't publish and offers the move");
	expect(panel->accept_button(2)->isEnabled() && panel->relocate_button(2)->isEnabled(), "nothing is disabled");
	panel->relocate_button(2)->click();
	expect(relocated == std::vector<std::size_t>{ 2U }, "Move to nearest change asks for that stop");
	static_cast<void>(core::relocate_tour_stop(shown, 2U, hunked));
	editor.present_tour_session(session, shown);
	expect(panel->placement_text(2).isEmpty() && panel->relocate_button(2) == nullptr, "once moved it is on the change");
	auto kept = core::propose_tour(core::tour_agent_run_id{ 12 }, placed, hunked, draft);
	static_cast<void>(core::decide_tour_proposal(kept, 2U, core::tour_proposal_decision::accepted, "Off the change."));
	editor.present_tour_session(session, kept);
	// [[BPR-1925]] v003. Accepted, it is a stop of the tour: nothing of it
	// waits as a proposal any more; its card says its lines are to confirm.
	expect(panel->accept_button(2) == nullptr && panel->proposal_reader(2) == nullptr, "accepted, it no longer waits as a proposal");
	// [[BPR-1912]] AC4, [[BPR-1913]] AC2/AC3. A failed run in which Claude ran
	// a command outside the read-only list.
	{
		auto       failed = core::tour_agent_session{ };
		const auto first  = core::begin_tour_run(failed, core::tour_agent_request::draft_tour, core::tour_agent_tools::read_only, "Draft the tour");
		static_cast<void>(core::record_tour_activity(failed, *first, { { core::tour_transcript_kind::searched, "rg -n draw src" }, { core::tour_transcript_kind::read, "awk 'NR<3' notes.txt", core::tour_transcript_note::ran_outside_list } }));
		static_cast<void>(core::finish_tour_run(failed, *first, core::tour_agent_run_state::answered, "Proposed a Problem and 1 stop."));
		const auto again = core::begin_tour_run(failed, core::tour_agent_request::draft_tour, core::tour_agent_tools::read_only, "Draft the tour");
		static_cast<void>(core::record_tour_activity(failed, *again, { { core::tour_transcript_kind::read, "src/renderer.cpp" }, { core::tour_transcript_kind::ran, "python3 -c 'print(42)'", core::tour_transcript_note::not_run } }));
		static_cast<void>(core::finish_tour_run(failed, *again, core::tour_agent_run_state::failed, "It tried a tool a drafting run may not use, so the run was stopped."));
		auto withOutside = core::propose_tour(*first, answer, files, draft);
		editor.present_tour_session(failed, withOutside);
		expect(panel->outside_list()->isVisibleTo(&editor) && panel->outside_list()->text().startsWith(QStringLiteral("Claude ran 1 command outside the read-only list. <a href=")), "[[BPR-1912]] AC4 the proposal says one command ran outside the list: " + panel->outside_list()->text().toStdString());
		expect(panel->outside_list()->foregroundRole() == QPalette::PlaceholderText, "in the quiet colour, not as an error");
		render(editor, prefix, "outside-list");
		emit panel->outside_list()->linkActivated(QStringLiteral("#outside-list"));
		expect(panel->transcript()->isVisible() && panel->shown_run() == first, "its link opens the transcript at that run");
		expect(panel->transcript_view()->toPlainText().contains(QStringLiteral("awk 'NR<3' notes.txt")) && panel->transcript_view()->toPlainText().contains(QStringLiteral("ran, outside the read-only list")), "where the command is named as run outside the list");
		editor.present_tour_session(failed, core::propose_tour(*again, answer, files, draft));
		expect(!panel->outside_list()->isVisibleTo(&editor), "a run with none shows nothing extra");
		// BPR-1892's Show the run: the failed run, in the window.
		panel->open_transcript(again);
		expect(panel->shown_run() == again && panel->transcript_runs()->count() == 2 && panel->transcript_runs()->currentRow() == 0, "the failed run is shown, newest first");
		render(*panel->transcript(), prefix, "transcript-window");
		auto* const clipboard = QGuiApplication::clipboard();
		clipboard->clear();
		panel->copy_run_button()->click();
		const auto copied = clipboard->text();
		expect(copied.startsWith(QStringLiteral("## Run 2: Draft the tour\n\nDid not finish, read-only, in the review's workspace.\n\n- Asked: Draft the tour\n- Read: src/renderer.cpp\n- Ran: python3 -c 'print(42)' (not run, outside the read-only list)\n- Ended: It tried a tool a drafting run may not use, so the run was stopped.")),
			   "AC2 Copy run puts the whole failed run, with its reason, on the clipboard: " + copied.toStdString());
		panel->copy_all_button()->click();
		const auto all = clipboard->text();
		expect(all.indexOf(QStringLiteral("## Run 1")) == 0 && all.contains(QStringLiteral("## Run 2")) && all.contains(QStringLiteral("- Read: awk 'NR<3' notes.txt (ran, outside the read-only list)")), "Copy all runs copies the session, oldest first: " + all.toStdString());
		// AC3. One selection runs across the entries.
		panel->transcript_view()->selectAll();
		const auto selected = panel->transcript_view()->textCursor().selection().toPlainText();
		expect(selected.contains(QStringLiteral("src/renderer.cpp")) && selected.contains(QStringLiteral("so the run was stopped")), "a selection spans entries: " + selected.toStdString());
		panel->transcript_view()->copy();
		expect(clipboard->text().contains(QStringLiteral("python3 -c 'print(42)'")) && clipboard->text().contains(QStringLiteral("Ended")), "and the platform's Copy takes it");
		// Its own window wears the main window's appearance.
		auto dark = editor.palette();
		dark.setColor(QPalette::Window, QColor{ 0x1e, 0x1f, 0x22 });
		dark.setColor(QPalette::Base, QColor{ 0x16, 0x17, 0x19 });
		panel->restyle(dark);
		expect(panel->transcript()->palette().color(QPalette::Window) == QColor{ 0x1e, 0x1f, 0x22 } && panel->transcript_view()->palette().color(QPalette::Base) == QColor{ 0x16, 0x17, 0x19 }, "the transcript window follows the appearance");
		panel->restyle(editor.palette());
		panel->transcript()->hide();
	}
	formatted(editor, prefix);
	skills_in_the_ask_box(editor);
	header_wrap_measurements();
	whole_tour(prefix);
	std::cout << "tour-agent-session-view:pass\n";
	return EXIT_SUCCESS;
}
