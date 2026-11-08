// [[BPR-1914]] v003 and [[BPR-1919]]. Side chats: each part of the tour opens
// its own chat from a quiet glyph or its menu, docked beside the tour while
// the tour stays as it is; a plain chat with the author's bubbles, rendered
// replies, the Agent tab's run line ([[BPR-1941]]) and Copy; Update the part
// from this chat comes back
// as the part's proposal, decided in the chat. Fenced code in replies and
// proposals is highlighted as the stop's own explanation is, and code in an
// unknown language stays plain.
#include "../support/review_tour_fixture.hpp"
#include "bpr/adapters/syntax/packaged_syntax_classifier.hpp"
#include "bpr/application/markdown_code_syntax.hpp"
#include "bpr/application/tour_chat_prompts.hpp"
#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/agent_suggestion_list.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/tour_agent_panel.hpp"
#include "bpr/qt/tour_side_chat.hpp"

#include <QApplication>
#include <QClipboard>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabBar>
#include <QTextBlock>
#include <QTextEdit>
#include <QTextLayout>
#include <QToolButton>

#include <cstdlib>
#include <deque>
#include <iostream>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aPass, const std::string& aMessage)
	{
		if (!aPass) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	void settle()
	{
		for (int round = 0; round < 6; ++round) {
			QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
			QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
			QApplication::processEvents();
		}
	}
	void render(QWidget& aWidget, const std::string& aPrefix, const char* aName)
	{
		if (aPrefix.empty()) {
			return;
		}
		settle();
		expect(aWidget.grab().save(QString::fromStdString(aPrefix + aName + ".png")), "render saved");
	}

	/// The composed code colouring, as the window relays it.
	class held_texts final : public adapters::syntax::blob_source
	{
	public:
		explicit held_texts(application::markdown_code_texts& aTexts)
			: Texts{ aTexts }
		{
		}
		auto blob_bytes(const core::git_blob_oid& aBlob) -> std::optional<std::string> override { return Texts.bytes(aBlob); }

	private:
		application::markdown_code_texts& Texts;
	};
	class manual_executor final : public application::decision_executor, public application::effect_executor
	{
	public:
		void                              post(std::function<void()> aTask) override { Tasks.push_back(std::move(aTask)); }
		std::deque<std::function<void()>> Tasks;
	};
	struct composed_syntax
	{
		application::markdown_code_texts                           Texts;
		held_texts                                                 Source{ Texts };
		adapters::syntax::packaged_definition_catalog              Catalog;
		adapters::syntax::packaged_syntax_classifier               Classifier{ Catalog, Source };
		manual_executor                                            Effects;
		manual_executor                                            Decisions;
		std::vector<application::markdown_code_syntax_publication> Published;
		application::markdown_code_syntax_host                     Host{ Classifier, Texts, Effects, Decisions,
																		 [this](const std::string_view aSelector) -> std::optional<application::syntax_definition_version> {
														 auto definition = Catalog.definition_for_path(aSelector);
														 if (definition.Name == adapters::syntax::plain_text_definition_name) {
															 return std::nullopt;
														 }
														 return definition;
													 },
													 [this](const application::markdown_code_syntax_publication& aPublication) { Published.push_back(aPublication); } };
		void deliver(qt::review_tour_editor& aEditor)
		{
			for (int round = 0; round < 4; ++round) {
				settle();
				while (!Effects.Tasks.empty() || !Decisions.Tasks.empty()) {
					for (auto* lane : { &Effects, &Decisions }) {
						while (!lane->Tasks.empty()) {
							auto task = std::move(lane->Tasks.front());
							lane->Tasks.pop_front();
							task();
						}
					}
				}
				for (const auto& publication : std::exchange(Published, { })) {
					aEditor.apply_code_syntax(publication);
				}
			}
			settle();
		}
	};
	auto colours(qt::markdown_body_view& aReader, const QString& aText) -> std::set<QRgb>
	{
		auto seen = std::set<QRgb>{ };
		for (auto block = aReader.rendered_view()->document()->begin(); block.isValid(); block = block.next()) {
			if (!block.text().contains(aText) || block.layout() == nullptr) {
				continue;
			}
			for (const auto& range : block.layout()->formats()) {
				seen.insert(range.format.foreground().color().rgb());
			}
		}
		return seen;
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	const auto   prefix = argc > 1 ? std::string{ argv[1] } : std::string{ };

	auto draft                    = tests::review_tour_fixture();
	draft.Problem                 = "Reads crash.";
	draft.Solution                = "Each view now clears between begin and present.";
	draft.Stops.front().Body      = "Clear first:\n\n```cpp\nauto lock = std::scoped_lock{ Mutex };\nreturn *Table;\n```\n";
	const auto             shared = std::make_shared<const core::review_tour>(draft);
	qt::review_tour_editor editor;
	auto                   actions = std::vector<qt::tour_editor_action>{ };
	editor.set_dispatch([](core::tour_command) { }, [&actions](const qt::tour_editor_action aAction) { actions.push_back(aAction); });
	auto asked   = std::vector<qt::tour_ask>{ };
	auto decided = std::vector<std::tuple<std::size_t, core::tour_proposal_decision, QString>>{ };
	auto shown   = std::vector<bool>{ };
	editor.set_tour_ask_dispatch([&asked](const qt::tour_ask& aAsk) { asked.push_back(aAsk); });
	editor.set_tour_session_dispatch([] { }, [] { }, [&decided](const std::size_t aIndex, const core::tour_proposal_decision aDecision, const QString& aText) { decided.emplace_back(aIndex, aDecision, aText); });
	editor.set_side_chat_visibility_dispatch([&shown](const bool aShown) { shown.push_back(aShown); });
	editor.resize(380, 1200);
	editor.show();
	editor.present({ .Draft = shared });
	editor.set_agent_readiness(qt::agent_setup_readiness::ready);
	settle();
	auto* const chat = editor.side_chat();
	chat->resize(380, 900);
	// [[BPR-1934]]. The window gives the chat the application's prompt list.
	chat->set_prompt_source(&application::tour_chat_prompts);
	const auto offered = [chat] {
		auto* const list = chat->suggestions();
		return list == nullptr || !list->isVisibleTo(chat) ? QStringList{ } : list->prompts();
	};
	const auto choose = [chat](const QString& aPrompt) {
		for (auto* const button : chat->suggestions()->buttons()) {
			if (button->property("agentPrompt").toString() == aPrompt) {
				button->click();
				return true;
			}
		}
		return false;
	};

	// (1) The tour stays clean: one quiet glyph per part, and nothing of a
	// chat in the tour's column.
	editor.open_step(core::tour_step::solution);
	settle();
	auto* const solution = editor.findChild<QPlainTextEdit*>(QStringLiteral("tourSolution"));
	auto* const glyph    = editor.findChild<QToolButton*>(QStringLiteral("tourSolutionChat"));
	// [[BPR-1925]] v002, owner on v001: "The mockup had the chat button on
	// the right side of each of the sections". The toggle is at the right of
	// the Solution's header row, and says what it does.
	auto* const solutionRow = editor.findChild<QWidget*>(QStringLiteral("tourSolutionRow"));
	expect(glyph != nullptr && glyph->isVisibleTo(&editor) && glyph->parentWidget() == solutionRow && glyph->isCheckable() && !glyph->isChecked(), "the Solution's chat toggle is in its header");
	expect(glyph->geometry().right() >= solutionRow->width() - 2 - (solutionRow->findChild<QPushButton*>(QStringLiteral("tourSolutionChange")) != nullptr && solutionRow->findChild<QPushButton*>(QStringLiteral("tourSolutionChange"))->isVisible() ? solutionRow->findChild<QPushButton*>(QStringLiteral("tourSolutionChange"))->width() + 8 : 0), "at the header's right");
	expect(glyph->accessibleName() == QStringLiteral("Open the chat about the Solution") && glyph->focusPolicy() == Qt::StrongFocus, "it says it opens the Solution's chat and takes the keyboard");
	expect(!editor.isAncestorOf(chat) && chat->isWindow() && chat->findChild<QTabBar*>() == nullptr, "the chat is its own column, for the window to place beside the tour, with no tabs");
	expect(!chat->isVisible() && shown.empty(), "no chat is open until asked for");
	auto cursor = solution->textCursor();
	cursor.setPosition(static_cast<int>(QStringLiteral("Each view now clears ").size()));
	cursor.setPosition(static_cast<int>(QStringLiteral("Each view now clears between begin and present").size()), QTextCursor::KeepAnchor);
	solution->setTextCursor(cursor);
	settle();
	glyph->click();
	chat->show();
	settle();
	expect(!shown.empty() && shown.back() && chat->column_open(), "opening a chat asks for its column");
	expect(chat->chat_count() == 1U && chat->current() == core::tour_thread{ .Kind = core::tour_thread_kind::solution } && chat->discussed() == chat->current(), "it is the Solution's chat");
	expect(chat->subject()->text() == QStringLiteral("Solution") && chat->findChild<QLabel*>(QStringLiteral("tourSideChatQuote"))->text().contains(QStringLiteral("Each view now clears")), "the top names the part and quotes it");
	// [[BPR-1925]]. The part is marked: its glyph pressed while its chat is
	// beside the tour.
	expect(glyph->isChecked(), "the Solution's glyph is pressed while its chat shows");
	expect(chat->passage() && chat->passage()->Text == "between begin and present" && chat->passage_line()->isVisibleTo(chat), "the selected passage is quoted above the composer");
	expect(!chat->update_button()->isEnabled(), "Update waits until there is a chat to update from");
	// [[BPR-1934]] AC1, AC2. The empty Solution chat offers its prompts, in
	// the Agent tab's own suggestion component and the agent's accent.
	// [[BPR-1934]] v002: with text selected in the Solution, Explain the
	// selected block leads.
	expect(offered() == QStringList{ QStringLiteral("Explain the selected block"), QStringLiteral("Make this clearer"), QStringLiteral("What's missing?"), QStringLiteral("Make it shorter") },
		   "the empty Solution chat offers the part's prompts, Explain the selected block first while text is selected");
	expect(chat->suggestions()->buttons().size() == 4U && chat->suggestions()->buttons().front()->objectName() == QStringLiteral("agentSuggestion") && chat->suggestions()->buttons().front()->isEnabled()
			   && chat->suggestions()->lead()->text() == QStringLiteral("Or try:"),
		   "AC2 the Agent tab's chips, one each, usable, under their lead");
	expect(chat->suggestions()->buttons().front()->palette().color(QPalette::ButtonText) == qt::agent_accent(chat->palette()), "in the agent's accent");
	render(*chat, prefix, "side-chat-solution-prompts");
	// [[BPR-1881]]'s gate: none while the agent is not set up.
	editor.set_agent_readiness(qt::agent_setup_readiness::unfinished);
	settle();
	expect(offered().isEmpty(), "no prompts while the agent is not set up");
	editor.set_agent_readiness(qt::agent_setup_readiness::ready);
	settle();
	expect(offered().size() == 4, "they come back once it is");
	chat->composer()->setPlainText(QStringLiteral("What does this mean?"));
	auto enter = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
	QApplication::sendEvent(chat->composer(), &enter);
	expect(asked.size() == 1U && asked[0].Thread.Kind == core::tour_thread_kind::solution && asked[0].Passage && asked[0].Question == QStringLiteral("What does this mean?") && !asked[0].Update, "Return asks in the Solution's chat, with the passage");
	expect(chat->composer()->toPlainText().isEmpty() && !chat->passage(), "the composer clears");

	// (2) A plain chat: the bubble, the Agent tab's live line while it runs
	// ([[BPR-1941]]), the reply rendered, who answered and how long it took,
	// Copy on it and on the whole chat.
	auto       session = core::tour_agent_session{ };
	const auto first   = core::begin_tour_ask(session, asked[0].Thread, asked[0].Passage, "What does this mean?");
	editor.set_tour_running(std::nullopt, false, true);
	editor.present_tour_session(session, std::nullopt);
	settle();
	expect(chat->send_button()->text() == QStringLiteral("Stop") && chat->send_button()->isEnabled(), "a running turn is stopped from Send");
	// [[BPR-1941]] AC1. The run's events, as the host relays them: the
	// workspace, then thinking, then writing; the time counts by itself.
	{
		auto live  = qt::agent_run_status{ };
		live.Agent = QStringLiteral("Claude Code");
		live.Stage = qt::agent_run_stage::preparing;
		editor.present_run_status(live);
		settle();
		auto* const line = chat->status_line(0);
		expect(line != nullptr && line->live() && line->isVisibleTo(chat) && line->text() == QStringLiteral("● Claude Code is making its workspace at the head, 0:00."), "AC1 the running turn says its agent and what it does: " + (line != nullptr ? line->text().toStdString() : std::string{ }));
		expect(chat->findChild<QToolButton*>(QStringLiteral("tourSideChatWorked")) == nullptr, "the Working disclosure is gone");
		live.Stage = qt::agent_run_stage::thinking;
		editor.present_run_status(live);
		expect(line->text() == qt::agent_run_status_words(live, 0), "then it thinks, in the Agent tab's words");
		expect(line->text() == QStringLiteral("● Claude Code is thinking, 0:00."), "the same words the Agent tab's turn says");
		auto clock = QElapsedTimer{ };
		clock.start();
		while (clock.elapsed() < 1300) {
			QApplication::processEvents(QEventLoop::AllEvents, 50);
		}
		expect(line->text() == QStringLiteral("● Claude Code is thinking, 0:01."), "AC1 the time counts live: " + line->text().toStdString());
		live.Stage = qt::agent_run_stage::writing;
		editor.present_run_status(live);
		expect(line->text() == QStringLiteral("● Claude Code is writing, 0:01."), "and it writes");
		// A redraw of the chat keeps the run's time.
		editor.present_tour_session(session, std::nullopt);
		settle();
		expect(chat->status_line(0)->text() == QStringLiteral("● Claude Code is writing, 0:01."), "the chat drawn again keeps its time");
		render(*chat, prefix, "side-chat-running");
	}
	static_cast<void>(core::record_tour_activity(session, *first, { { core::tour_transcript_kind::read, "src/item.cpp" }, { core::tour_transcript_kind::searched, "rg -n lock src" } }));
	static_cast<void>(core::answer_tour_ask(session, *first, "It is the frame window:\n\n```cpp\nauto lock = std::scoped_lock{ Mutex };\nreturn *Table;\n```\n\n```nosuchlanguage\nauto lock = plain_text{ };\n```\n", { }));
	static_cast<void>(core::time_tour_run(session, *first, 7U));
	editor.present_run_status(std::nullopt);
	editor.set_tour_running(std::nullopt, false, false);
	editor.present_tour_session(session, std::nullopt);
	settle();
	// [[BPR-1925]]. Its glyph counts its turns.
	expect(glyph->text() == QStringLiteral("1") && chat->turns_about(core::tour_thread{ .Kind = core::tour_thread_kind::solution }) == 1U, "the Solution's toggle counts its one turn");
	expect(glyph->isChecked() && glyph->accessibleName() == QStringLiteral("Close the chat about the Solution"), "pressed, it says it closes the chat");
	expect(chat->chat_parts() == 1U, "one part has a chat");
	expect(chat->turn_count() == 1U && chat->bubble(0) != nullptr && chat->bubble(0)->findChild<QLabel*>(QStringLiteral("tourSideChatQuestion"))->text() == QStringLiteral("What does this mean?"), "the author's words are a bubble");
	expect(offered() == QStringList{ QStringLiteral("Explain the selected block") } && !chat->suggestions()->lead()->isVisibleTo(chat),
		   "AC1 the part's prompts go once the chat has a turn; Explain the selected block stays while text is selected");
	expect(chat->bubble(0)->geometry().right() > chat->reply(0)->geometry().left() + chat->reply(0)->width() / 2, "on the right, the reply on the left");
	expect(!chat->status_line(0)->live() && chat->status_line(0)->text() == QStringLiteral("Claude Code answered in 0:07.") && !chat->status_line(0)->font().italic(), "AC2 its ending says who answered and how long it took, as the Agent tab's does: " + chat->status_line(0)->text().toStdString());
	expect(chat->status_line(0)->geometry().top() >= chat->reply(0)->geometry().bottom(), "under the reply");
	expect(chat->resend_button(0) == nullptr && chat->delete_button(0) == nullptr, "an answer offers no Resend or Delete");
	auto* const clipboard = QGuiApplication::clipboard();
	chat->copy_reply_button(0)->click();
	expect(clipboard->text().startsWith(QStringLiteral("It is the frame window:")), "Copy takes the reply as written");
	chat->copy_chat_button()->click();
	expect(clipboard->text().startsWith(QStringLiteral("## Side chat: Solution")) && clipboard->text().contains(QStringLiteral("**You:** What does this mean?")) && clipboard->text().contains(QStringLiteral("**Claude Code** (answered in 0:07):")), "Copy chat takes the whole chat: " + clipboard->text().toStdString());

	// (3) Update this section from our chat: an Update turn, and its proposal
	// for the Solution decided in the chat, not in the draft's box.
	expect(chat->update_button()->isEnabled() && chat->update_button()->text() == QStringLiteral("Update the Solution from this chat"), "Update names the part");
	chat->update_button()->click();
	expect(asked.size() == 2U && asked[1].Update && asked[1].Thread.Kind == core::tour_thread_kind::solution && asked[1].Question == QStringLiteral("Update the Solution from our chat."), "Update asks for the part rewritten from the chat");
	const auto update  = core::begin_tour_ask(session, asked[1].Thread, std::nullopt, asked[1].Question.toStdString(), true);
	const auto answer  = core::tour_ask_answer{ .Reply = "Rewritten.", .Changes = { .Solution = std::string{ "Both views share one frame:\n\n```cpp\nauto lock = std::scoped_lock{ Mutex };\nreturn *Table;\n```\n" } } };
	const auto changes = core::keep_thread_part(core::propose_tour_changes(*update, answer, { }, draft), asked[1].Thread);
	static_cast<void>(core::answer_tour_ask(session, *update, "I rewrote the Solution from our chat, around `auto x = 1;`.", core::tally_of(changes)));
	editor.present_tour_session(session, changes);
	settle();
	expect(chat->turn_count() == 2U && chat->accept_button(0) != nullptr && chat->proposal_reader(0) != nullptr, "the proposed Solution is in the chat, with Accept");
	expect(editor.tour_agent()->accept_button(0) == nullptr, "and not in the draft's box");
	render(*chat, prefix, "side-chat-solution");
	chat->accept_button(0)->click();
	expect(decided.size() == 1U && std::get<0>(decided[0]) == 0U && std::get<1>(decided[0]) == core::tour_proposal_decision::accepted && std::get<2>(decided[0]).startsWith(QStringLiteral("Both views share one frame")), "Accept applies it to the part");

	// (4) [[BPR-1919]]. The reply's and the proposal's C++ in the stop
	// reader's colours; an unknown language plain.
	auto syntax = composed_syntax{ };
	editor.set_code_syntax_dispatch([&syntax](std::vector<application::markdown_code_syntax_request> aRequests) { syntax.Host.request(aRequests); });
	editor.show_stops();
	editor.go_to_stop(core::tour_stop_id{ 1 });
	syntax.deliver(editor);
	auto* const stopReader = static_cast<qt::markdown_body_view*>(editor.findChild<QWidget*>(QStringLiteral("tourStopReader")));
	const auto  stopOwn    = colours(*stopReader, QStringLiteral("scoped_lock"));
	expect(stopOwn.size() >= 2U, "the stop's own C++ is highlighted");
	expect(colours(*chat->reply(0), QStringLiteral("scoped_lock")) == stopOwn, "AC1 the reply's C++ in the same colours");
	expect(colours(*chat->proposal_reader(0), QStringLiteral("scoped_lock")) == stopOwn, "AC1 the proposal's too");
	expect(colours(*chat->reply(0), QStringLiteral("plain_text")).size() <= 1U, "AC2 an unknown language stays plain");
	// [[BPR-1914]] v004, owner look v003: "it seems like inline code isnt
	// being highlighted only blocks". Inline code reads in the language of
	// the chat's part (here the tour's stops, all C++).
	const auto inlineAt = [](qt::markdown_body_view& aReader, const QString& aText) {
		auto seen = std::set<QRgb>{ };
		for (auto block = aReader.rendered_view()->document()->begin(); block.isValid(); block = block.next()) {
			if (!block.text().contains(aText) || block.layout() == nullptr) {
				continue;
			}
			const auto start = block.text().indexOf(aText);
			for (const auto& range : block.layout()->formats()) {
				if (range.start < start + aText.size() && range.start + range.length > start) {
					seen.insert(range.format.foreground().color().rgb());
				}
			}
		}
		return seen;
	};
	expect(inlineAt(*chat->reply(1), QStringLiteral("auto x = 1;")).size() >= 2U, "inline code in a reply is coloured as the part's language");

	// (5) A stop's chat: its glyph shows the stop's lines in the code, the top
	// names its file and lines, Show in code goes there again, and Ask about
	// its code quotes the lines.
	settle();
	actions.clear();
	auto* const stopGlyph = editor.findChild<QToolButton*>(QStringLiteral("tourStopChat"));
	expect(stopGlyph != nullptr && stopGlyph->isVisibleTo(&editor), "an open stop has its glyph");
	stopGlyph->click();
	settle();
	expect(chat->chat_count() == 2U && chat->current() == core::tour_thread{ .Kind = core::tour_thread_kind::stop, .Stop = core::tour_stop_id{ 1 } }, "switching parts switches the chat: the stop's");
	expect(stopGlyph->isChecked() && !glyph->isChecked(), "the stop's glyph is pressed now, the Solution's not");
	auto* const route = editor.findChild<QListWidget*>(QStringLiteral("tourOutline"));
	expect(route != nullptr && route->item(0)->data(Qt::UserRole + 10).toBool(), "the stop is marked on the route");
	expect(std::ranges::count(actions, qt::tour_editor_action::navigate_stop) >= 1, "the stop's lines are shown in the code");
	expect(chat->subject()->text() == QStringLiteral("Stop 1, Why") && chat->findChild<QLabel*>(QStringLiteral("tourSideChatQuote"))->text() == QStringLiteral("src/item.cpp, lines 4 to 6"), "the top names the stop and its lines");
	// The Solution's selection ends; none is offered for it any more.
	{
		auto none = solution->textCursor();
		none.clearSelection();
		solution->setTextCursor(none);
	}
	settle();
	expect(offered()
			   == QStringList{ QStringLiteral("What does this code do?"), QStringLiteral("Why does this change matter?"), QStringLiteral("Rewrite this explanation more simply"), QStringLiteral("Is this the right place for this stop?") },
		   "AC1 the empty stop chat offers the stop's prompts");
	actions.clear();
	chat->show_button()->click();
	expect(chat->show_button()->text() == QStringLiteral("Show in code") && std::ranges::count(actions, qt::tour_editor_action::navigate_stop) >= 1, "Show in code goes to it again");
	// [[BPR-1934]] v002. Text selected in the stop's explanation: Explain the
	// selected block asks about exactly it, quoted, in the stop's chat.
	{
		auto* const reading = static_cast<qt::markdown_body_view*>(editor.findChild<QWidget*>(QStringLiteral("tourStopReader")))->rendered_view();
		auto        block   = reading->textCursor();
		block.setPosition(0);
		block.setPosition(static_cast<int>(QStringLiteral("Clear first").size()), QTextCursor::KeepAnchor);
		reading->setTextCursor(block);
		settle();
	}
	expect(!offered().isEmpty() && offered().front() == QStringLiteral("Explain the selected block"), "selected text in the stop offers Explain the selected block");
	expect(choose(QStringLiteral("Explain the selected block")), "Explain the selected block is a chip");
	expect(asked.back().Thread == core::tour_thread{ .Kind = core::tour_thread_kind::stop, .Stop = core::tour_stop_id{ 1 } } && asked.back().Question == QStringLiteral("Explain the selected block") && asked.back().Passage
			   && asked.back().Passage->Text == "Clear first" && asked.back().Passage->Field == core::tour_passage_field::stop && !asked.back().Update,
		   "v002 the turn asks about exactly the selected block, quoted, in the stop's chat");
	editor.ask_about_code(std::nullopt);
	expect(chat->passage() && chat->passage()->Field == core::tour_passage_field::stop_code && chat->passage()->Path == "src/item.cpp" && chat->passage()->FirstLine == 4U, "its code is quoted for the next question");
	render(*chat, prefix, "side-chat-stop");
	// The tour's own chat, from the panel.
	editor.tour_agent()->chat_button()->click();
	settle();
	expect(chat->chat_count() == 3U && chat->current() == core::tour_thread{ } && chat->subject()->text() == QStringLiteral("The whole tour"), "the tour as a whole has a chat too");
	// [[BPR-1934]] AC1. The whole tour's prompts. One click sends one; the
	// one the reviewer finishes waits in the composer instead.
	expect(offered() == QStringList{ QStringLiteral("Explain the selected block in Stop 1"), QStringLiteral("Too many stops?"), QStringLiteral("Suggest a better order"), QStringLiteral("Focus on …"), QStringLiteral("What's not covered?") }, "AC1 the empty tour chat offers the tour's prompts: " + offered().join(QLatin1Char('|')).toStdString());
	render(*chat, prefix, "side-chat-tour-prompts");
	const auto askedBefore = asked.size();
	expect(choose(QStringLiteral("Focus on …")) && asked.size() == askedBefore && chat->composer()->toPlainText() == QStringLiteral("Focus on "),
		   "Focus on … is put in the composer to finish");
	chat->composer()->clear();
	expect(choose(QStringLiteral("Too many stops?")) && asked.size() == askedBefore + 1U && asked.back().Thread == core::tour_thread{ } && asked.back().Question == QStringLiteral("Too many stops?") && !asked.back().Update,
		   "AC1 one click sends the prompt in the tour's chat");
	const auto tourTurn = core::begin_tour_ask(session, asked.back().Thread, std::nullopt, "Too many stops?");
	static_cast<void>(core::answer_tour_ask(session, *tourTurn, "Four stops is about right.", { }));
	editor.present_tour_session(session, std::nullopt);
	settle();
	expect(chat->turn_count() == 1U && offered() == QStringList{ QStringLiteral("Explain the selected block in Stop 1") }, "AC1 and they hide after the first turn");
	// [[BPR-1937]]. An answer that changed nothing offers Update the tour
	// from this chat in place, so a plan given in words can be asked for.
	expect(chat->unchanged_line() != nullptr && chat->unchanged_line()->isVisibleTo(chat) && chat->unchanged_line()->text().contains(QStringLiteral("Nothing in the tour changed")), "BPR-1937 an answer that changed nothing says so");
	const auto beforeUpdate = asked.size();
	emit       chat->unchanged_line()->linkActivated(QStringLiteral("#update"));
	settle();
	expect(asked.size() == beforeUpdate + 1U && asked.back().Update && asked.back().Thread == core::tour_thread{ } && asked.back().Question == QStringLiteral("Update the tour from our chat."), "BPR-1937 its link asks for the tour updated from the chat");
	// [[BPR-1937]] AC2. A revision whose merge was refused says which and why.
	{
		const auto refusedTurn = core::begin_tour_ask(session, core::tour_thread{ }, std::nullopt, "Lower, please.");
		auto       tally       = core::tour_proposal_tally{ };
		tally.Refused          = { { .Kind = core::tour_revision_kind::merge, .Stops = { 4U, 9U }, .Reason = core::tour_revision_refusal::different_files } };
		static_cast<void>(core::answer_tour_ask(session, *refusedTurn, "Merge 4 and 9 too.", tally));
		editor.present_tour_session(session, std::nullopt);
		settle();
		expect(chat->refused_line() != nullptr && chat->refused_line()->text() == QStringLiteral("Merging stops 4 and 9 wasn't proposed: their lines are in different files and not confirmed yet, so one stop can't keep them all."),
			   "BPR-1937 AC2 the refused merge is named with why: " + (chat->refused_line() != nullptr ? chat->refused_line()->text().toStdString() : std::string{ }));
		expect(chat->unchanged_line() == nullptr, "BPR-1937 a turn with a refusal is not said to have changed nothing");
	}
	// v002. From another part's chat, Explain the selected block opens the
	// selected part's chat and asks there.
	expect(choose(QStringLiteral("Explain the selected block in Stop 1")) && chat->current() == core::tour_thread{ .Kind = core::tour_thread_kind::stop, .Stop = core::tour_stop_id{ 1 } }
			   && asked.back().Thread == chat->current() && asked.back().Passage && asked.back().Passage->Text == "Clear first",
		   "v002 it opens the stop's chat and asks about the selection there");
	// [[BPR-1925]]. Every part's chat stays: one real click on a part's glyph
	// brings it back, with its turns, and it copies whole.
	const auto realClick = [](QWidget* aWidget) {
		const auto at      = aWidget->rect().center();
		auto       press   = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ at }, aWidget->mapToGlobal(QPointF{ at }), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		auto       release = QMouseEvent{ QEvent::MouseButtonRelease, QPointF{ at }, aWidget->mapToGlobal(QPointF{ at }), Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
		QApplication::sendEvent(aWidget, &press);
		QApplication::sendEvent(aWidget, &release);
		settle();
	};
	editor.open_step(core::tour_step::solution);
	settle();
	realClick(glyph);
	expect(chat->current() == core::tour_thread{ .Kind = core::tour_thread_kind::solution } && chat->turn_count() == 2U, "one click on the Solution's glyph brings its chat back, turns and all");
	chat->copy_chat_button()->click();
	expect(clipboard->text().startsWith(QStringLiteral("## Side chat: Solution")) && clipboard->text().contains(QStringLiteral("Update the Solution from our chat.")), "and it copies whole");
	// Close: the column goes and every chat is kept.
	realClick(chat->close_button());
	expect(!chat->column_open() && !shown.back() && !chat->discussed() && !glyph->isChecked() && glyph->text() == QStringLiteral("2"), "close takes the column away; the glyph keeps its count");
	chat->open(core::tour_thread{ .Kind = core::tour_thread_kind::solution });
	settle();
	expect(chat->turn_count() == 2U && chat->column_open(), "reopened, a chat still has its turns");

	// (6) [[BPR-1941]] AC2. A turn that ended without an answer, as the Agent
	// tab's error turns: why, "Stopped after", Resend and Delete.
	{
		auto deleted = std::vector<core::tour_agent_run_id>{ };
		chat->set_delete_dispatch([&deleted](const core::tour_agent_run_id aRun) { deleted.push_back(aRun); });
		const auto problem = core::tour_thread{ .Kind = core::tour_thread_kind::problem };
		chat->open(problem);
		const auto broken = core::begin_tour_ask(session, problem, std::nullopt, "Is the Problem right?");
		static_cast<void>(core::finish_tour_run(session, *broken, core::tour_agent_run_state::failed, "Claude Code exited with an error.\n\nexit 1"));
		static_cast<void>(core::time_tour_run(session, *broken, 3U));
		editor.present_tour_session(session, std::nullopt);
		settle();
		auto* const failure = chat->findChild<QLabel*>(QStringLiteral("tourSideChatFailure"));
		expect(chat->turn_count() == 1U && chat->reply(0) == nullptr && failure != nullptr && failure->text() == QStringLiteral("Didn't finish: Claude Code exited with an error."), "AC2 the failed turn says why");
		expect(chat->status_line(0)->text() == QStringLiteral("Stopped after 0:03.") && chat->status_line(0)->font().italic(), "AC2 and ends as the Agent tab's error turns do, in italics");
		expect(chat->resend_button(0) != nullptr && chat->resend_button(0)->isVisibleTo(chat) && chat->delete_button(0) != nullptr && chat->delete_button(0)->isVisibleTo(chat), "AC2 Resend and Delete");
		render(*chat, prefix, "side-chat-failed");
		const auto before = asked.size();
		chat->resend_button(0)->click();
		expect(asked.size() == before + 1U && asked.back().Thread == problem && asked.back().Question == QStringLiteral("Is the Problem right?") && !asked.back().Update, "AC2 Resend asks the same again in the same chat");
		chat->delete_button(0)->click();
		expect(deleted.size() == 1U && deleted.front() == *broken, "AC2 Delete asks for the turn to go");
		static_cast<void>(core::remove_tour_run(session, *broken));
		editor.present_tour_session(session, std::nullopt);
		settle();
		expect(chat->turn_count() == 0U, "and it goes");
	}
	// (7) [[BPR-1941]]. Draft Tour running: the whole tour's chat and the
	// tab's notice say it with the same live line.
	{
		chat->open(core::tour_thread{ });
		editor.set_tour_running(std::nullopt, true);
		auto live  = qt::agent_run_status{ };
		live.Agent = QStringLiteral("Claude Code");
		live.Stage = qt::agent_run_stage::thinking;
		editor.present_run_status(live);
		settle();
		expect(chat->draft_status_line() != nullptr && chat->draft_status_line()->isVisibleTo(chat) && chat->draft_status_line()->text() == QStringLiteral("● Claude Code is thinking, 0:00."), "the tour's chat says the draft runs");
		expect(editor.draft_status_line()->isVisibleTo(&editor) && editor.draft_status_line()->text() == chat->draft_status_line()->text(), "the tab's notice says it the same way");
		render(*chat, prefix, "side-chat-drafting");
		editor.present_run_status(std::nullopt);
		editor.set_tour_running(std::nullopt, false);
		settle();
		expect(chat->draft_status_line() == nullptr && !editor.draft_status_line()->isVisibleTo(&editor), "both go when it ends");
	}

	std::cout << "tour-side-chat:pass\n";
	return EXIT_SUCCESS;
}
