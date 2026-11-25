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
#include "bpr/core/prompt_macro.hpp"
#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/agent_run_status.hpp"
#include "bpr/qt/agent_suggestion_list.hpp"
#include "bpr/qt/destructive_question.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/tour_agent_panel.hpp"
#include "bpr/qt/tour_side_chat.hpp"

#include "../support/qt_prompt_sources.hpp"

#include <QApplication>
#include <QClipboard>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTabBar>
#include <QTextBlock>
#include <QTextEdit>
#include <QTextLayout>
#include <QTimer>
#include <QToolButton>

#include <algorithm>
#include <array>
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
	void verify_first_live_second()
	{
		// [[BPR-1945]], [[BPR-1946]]. Vary the start phase: even a native
		// precise tick can arrive before the actual whole second. Count
		// two seconds with an activity refresh between them, keeping the
		// composed chat's deadline allowance at each elapsed boundary.
#ifdef NDEBUG
		constexpr auto liveBudget = 1300;
#else
		constexpr auto liveBudget = 3000;
#endif
		struct live_probe
		{
			qt::agent_run_status_line Line{ };
			QElapsedTimer             Started;
			qint64                    Second  = 1;
			bool                      Checked = false;
		};
		auto probes = std::array<live_probe, 8>{ };
		for (std::size_t index = 0; index < probes.size(); ++index) {
			auto& probe = probes[index];
			QTimer::singleShot(static_cast<int>(index) * 137, Qt::PreciseTimer, &probe.Line, [&probe] {
				probe.Started.start();
				auto status  = qt::agent_run_status{ };
				status.Agent = QStringLiteral("Claude Code");
				probe.Line.present(status, probe.Started);
			});
		}
		auto clock = QElapsedTimer{ };
		clock.start();
		const auto complete = [&probes] { return std::ranges::all_of(probes, [](const live_probe& aProbe) { return aProbe.Checked; }); };
		while (!complete() && clock.elapsed() < liveBudget + 3000) {
			QApplication::processEvents(QEventLoop::AllEvents, 50);
			for (auto& probe : probes) {
				if (!probe.Started.isValid() || probe.Checked) {
					continue;
				}
				const auto expected = probe.Second == 1 ? QStringLiteral("● Claude Code is thinking, 0:01.") : QStringLiteral("● Claude Code is writing, 0:02.");
				if (probe.Line.text() == expected) {
					expect(probe.Line.live() && probe.Started.elapsed() >= probe.Second * 1000, "the live line reads a whole elapsed second");
					if (probe.Second == 1) {
						auto status  = qt::agent_run_status{ };
						status.Agent = QStringLiteral("Claude Code");
						status.Stage = qt::agent_run_stage::writing;
						probe.Line.present(status, probe.Started);
						probe.Second = 2;
					} else {
						probe.Line.settle(QStringLiteral("Stopped after 0:02"), false);
						probe.Checked = true;
					}
				} else {
					expect(probe.Started.elapsed() < liveBudget + (probe.Second - 1) * 1000, "the shared live line counts each second at every start phase: " + probe.Line.text().toStdString());
				}
			}
		}
		expect(complete(), "each staggered live-clock check ran");
	}
	[[maybe_unused]] void render(QWidget& aWidget, const std::string& aPrefix, const char* aName)
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
	verify_first_live_second();

	// [[BPR-2275]]. The course ran on the Solution's chat until
	// [[BPR-2274]] made a tour only its stops; it runs on Stop 2's now.
	auto draft               = tests::review_tour_fixture();
	draft.Stops.front().Body = "Clear first:\n\n```cpp\nauto lock = std::scoped_lock{ Mutex };\nreturn *Table;\n```\n";
	{
		auto second  = draft.Stops.front();
		second.Id    = core::tour_stop_id{ 2 };
		second.Title = "Where it presents";
		second.Body  = "Each view now clears between begin and present.";
		draft.Stops.push_back(second);
		draft.LastStopId = 2;
	}
	const auto             partThread = core::tour_thread{ .Kind = core::tour_thread_kind::stop, .Stop = core::tour_stop_id{ 2 } };
	const auto             shared     = std::make_shared<const core::review_tour>(draft);
	qt::review_tour_editor editor;
	auto                   actions = std::vector<qt::tour_editor_action>{ };
	editor.set_dispatch([](core::tour_command) { }, [&actions](const qt::tour_editor_action aAction) { actions.push_back(aAction); });
	auto asked   = std::vector<qt::tour_ask>{ };
	auto decided = std::vector<std::tuple<std::size_t, core::tour_proposal_decision, QString>>{ };
	auto shown   = std::vector<bool>{ };
	auto stops   = std::size_t{ 0U };
	editor.set_tour_ask_dispatch([&asked](const qt::tour_ask& aAsk) { asked.push_back(aAsk); });
	editor.set_tour_session_dispatch([] { }, [&stops] { ++stops; }, [&decided](const std::size_t aIndex, const core::tour_proposal_decision aDecision, const QString& aText) { decided.emplace_back(aIndex, aDecision, aText); });
	editor.set_side_chat_visibility_dispatch([&shown](const bool aShown) { shown.push_back(aShown); });
	editor.resize(380, 1200);
	editor.show();
	editor.present({ .Draft = shared });
	editor.set_agent_readiness(qt::agent_setup_readiness::ready);
	settle();
	auto* const chat = editor.side_chat();
	chat->resize(380, 900);
	// [[BPR-1934]], RFC-BPR-0125 AC05. The window gives the chat the shipped
	// suggested prompts; every list and text below is today's.
	tests::install_prompt_source(*chat, core::merge_prompt_macros(core::shipped_prompt_macros(), { }).macros());
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

	// (1) The tour stays clean: one quiet glyph on the open stop, and
	// nothing of a chat in the tour's column.
	editor.show_stops();
	editor.go_to_stop(core::tour_stop_id{ 2 });
	editor.begin_stop_edit();
	settle();
	auto* const written = editor.stop_body();
	auto* const glyph   = editor.findChild<QToolButton*>(QStringLiteral("tourStopChat"));
	expect(glyph != nullptr && glyph->isVisibleTo(&editor) && glyph->isCheckable() && !glyph->isChecked(), "the open stop's chat toggle shows, unpressed");
	expect(glyph->focusPolicy() == Qt::StrongFocus && glyph->accessibleName().startsWith(QStringLiteral("Open the chat")), "it says it opens the stop's chat and takes the keyboard: " + glyph->accessibleName().toStdString());
	expect(!editor.isAncestorOf(chat) && chat->isWindow() && chat->findChild<QTabBar*>() == nullptr, "the chat is its own column, for the window to place beside the tour, with no tabs");
	expect(!chat->isVisible() && shown.empty(), "no chat is open until asked for");
	auto cursor = written->textCursor();
	cursor.setPosition(static_cast<int>(QStringLiteral("Each view now clears ").size()));
	cursor.setPosition(static_cast<int>(QStringLiteral("Each view now clears between begin and present").size()), QTextCursor::KeepAnchor);
	written->setTextCursor(cursor);
	settle();
	glyph->click();
	chat->show();
	settle();
	expect(!shown.empty() && shown.back() && chat->column_open(), "opening a chat asks for its column");
	expect(chat->chat_count() == 1U && chat->current() == partThread && chat->discussed() == chat->current(), "it is Stop 2's chat");
	expect(chat->subject()->text() == QStringLiteral("Stop 2, Where it presents"), "the top names the stop: " + chat->subject()->text().toStdString());
	// [[BPR-1925]]. The part is marked: its glyph pressed while its chat is
	// beside the tour.
	expect(glyph->isChecked(), "the stop's glyph is pressed while its chat shows");
	expect(chat->passage() && chat->passage()->Text == "between begin and present" && chat->passage()->Field == core::tour_passage_field::stop && chat->passage_line()->isVisibleTo(chat), "the selected passage is quoted above the composer");
	expect(!chat->can_update() && chat->unchanged_line() == nullptr, "no update is offered until there is a chat to update from");
	expect(chat->passage_line()->parentWidget()->parentWidget() == chat->composer_box() && chat->composer()->parentWidget() == chat->composer_box(), "[[BPR-1995]] AC01 the passage is a chip inside the composer's field");
	// [[BPR-1934]] AC1, AC2. The empty stop chat offers its prompts, in the
	// Agent tab's own suggestion component and the agent's accent; with
	// text selected in the stop, Explain the selected block leads.
	const auto partPrompts = offered();
	expect(partPrompts.size() >= 2 && partPrompts.front() == QStringLiteral("Explain the selected block"), "the empty stop chat offers its prompts, Explain the selected block first while text is selected: " + partPrompts.join(QLatin1Char('|')).toStdString());
	expect(chat->suggestions()->buttons().size() == static_cast<std::size_t>(partPrompts.size()) && chat->suggestions()->buttons().front()->objectName() == QStringLiteral("agentSuggestion") && chat->suggestions()->buttons().front()->isEnabled()
			   && chat->suggestions()->lead()->text() == QStringLiteral("Or try:"),
		   "AC2 the Agent tab's chips, one each, usable, under their lead");
	expect(chat->suggestions()->buttons().front()->palette().color(QPalette::ButtonText) == qt::agent_accent(chat->palette()), "in the agent's accent");
	render(*chat, prefix, "side-chat-stop-prompts");
	// [[BPR-1881]]'s gate: none while the agent is not set up.
	editor.set_agent_readiness(qt::agent_setup_readiness::unfinished);
	settle();
	expect(offered().isEmpty(), "no prompts while the agent is not set up");
	editor.set_agent_readiness(qt::agent_setup_readiness::ready);
	settle();
	expect(offered() == partPrompts, "they come back once it is");
	chat->composer()->setPlainText(QStringLiteral("What does this mean?"));
	auto enter = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
	QApplication::sendEvent(chat->composer(), &enter);
	expect(asked.size() == 1U && asked[0].Thread == partThread && asked[0].Passage && asked[0].Question == QStringLiteral("What does this mean?") && !asked[0].Update, "Return asks in the stop's chat, with the passage");
	expect(chat->composer()->toPlainText().isEmpty() && !chat->passage(), "the composer clears");

	// (2) A plain chat: the bubble, the Agent tab's live line while it runs
	// ([[BPR-1941]]), the reply rendered, who answered and how long it took,
	// Copy on it and on the whole chat.
	auto       session = core::tour_agent_session{ };
	const auto first   = core::begin_tour_ask(session, asked[0].Thread, asked[0].Passage, "What does this mean?");
	editor.set_tour_host_idle(false);
	editor.set_tour_running(std::nullopt, false, true);
	editor.present_tour_session(session, std::nullopt);
	settle();
	expect(chat->send_button()->text() == QStringLiteral("Stop") && chat->send_button()->isEnabled(), "a running turn is stopped from Send");
	chat->send_button()->click();
	expect(stops == 1U && asked.size() == 1U, "a non-idle host preserves the existing Stop gesture and sends no question");
	editor.set_tour_host_idle(true);
	expect(chat->send_button()->text() == QStringLiteral("Stop") && chat->send_button()->isEnabled() && !chat->new_session_action()->isEnabled(),
		   "an idle observation cannot turn a newer running request into New session or hide Stop");
	editor.set_tour_host_idle(false);
	auto revisionChoice     = core::agent_continuation_offer{ };
	revisionChoice.Required = true;
	chat->present_revision_continuation(revisionChoice, { }, false);
	expect(chat->send_button()->isEnabled(), "a head change still permits Stop while the authorized turn is running");
	chat->present_revision_continuation(std::nullopt, { }, false);
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
		// Debug qualifies the live update, with room for a coalesced timer;
		// its scheduling latency is not a performance acceptance gate.
#ifdef NDEBUG
		constexpr auto liveBudget = 1300;
#else
		constexpr auto liveBudget = 3000;
#endif
		while (clock.elapsed() < liveBudget && line->text() == QStringLiteral("● Claude Code is thinking, 0:00.")) {
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
		// [[BPR-2005]] AC08. The Agent tab's running block: what it does,
		// its helpers, the quiet notice, on a clock the test holds.
		auto now = qint64{ 5'000'000 };
		qt::set_agent_activity_clock([&now] { return now; });
		auto activity          = core::agent_turn_activity{ };
		auto reading           = core::agent_activity_update{ };
		reading.Kind           = core::agent_activity_kind::doing;
		reading.Doing          = core::agent_doing::reading;
		reading.Subject        = "src/item.cpp";
		activity               = core::apply_agent_activity(activity, reading, 1'000U);
		auto helper            = core::agent_activity_update{ };
		helper.Kind            = core::agent_activity_kind::helper_started;
		helper.Helper          = "h1";
		helper.Subject         = "Callers of present";
		activity               = core::apply_agent_activity(activity, helper, 2'000U);
		live.Activity          = activity;
		live.ActivityStartedMs = now - 3'000;
		editor.present_run_status(live);
		settle();
		auto* const block = chat->activity_view(0);
		expect(block != nullptr && block->live() && block->helper_rows() == 1 && block->helper_row_text(0).startsWith(QStringLiteral("Callers of present\trunning")), "AC08 the side chat shows the helper's row");
		expect(chat->status_line(0)->text().contains(QStringLiteral("is reading item.cpp")) && chat->status_line(0)->text().contains(QStringLiteral("last activity 1 s ago")), "AC08 and what it is doing: " + chat->status_line(0)->text().toStdString());
		now += 121'000;
		block->tick();
		expect(block->quiet_shown(), "AC08 and the quiet notice");
		render(*chat, prefix, "side-chat-quiet");
		block->keep_waiting_button()->click();
		expect(!block->quiet_shown(), "Keep waiting puts it away");
		static_cast<void>(session);
		chat->record_run_activity(*first, core::end_agent_activity(activity, 130'000U), 130);
		qt::set_agent_activity_clock({ });
		live.Activity.reset();
		editor.present_run_status(live);
	}
	static_cast<void>(core::record_tour_activity(session, *first, { { core::tour_transcript_kind::read, "src/item.cpp" }, { core::tour_transcript_kind::searched, "rg -n lock src" } }));
	static_cast<void>(core::answer_tour_ask(session, *first, "It is the frame window:\n\n```cpp\nauto lock = std::scoped_lock{ Mutex };\nreturn *Table;\n```\n\n```nosuchlanguage\nauto lock = plain_text{ };\n```\n", { }));
	static_cast<void>(core::time_tour_run(session, *first, 7U));
	editor.present_run_status(std::nullopt);
	editor.set_tour_running(std::nullopt, false, false);
	editor.present_tour_session(session, std::nullopt);
	settle();
	// [[BPR-2040]]. A final reply is not a promise that cleanup retired.
	const auto retainedReply = chat->reply(0)->original_body();
	chat->composer()->setPlainText(QStringLiteral("Follow up after retirement"));
	expect(!chat->send_button()->isEnabled() && !chat->new_session_action()->isEnabled() && !chat->can_update() && chat->unchanged_line() != nullptr && !chat->unchanged_line()->text().contains(QStringLiteral("<a "))
			   && !chat->status_line(0)->live(),
		   "a final reply does not offer another request before host retirement");
	QApplication::sendEvent(chat->composer(), &enter);
	expect(asked.size() == 1U && chat->composer()->toPlainText() == QStringLiteral("Follow up after retirement") && chat->reply(0)->original_body() == retainedReply,
		   "retirement withholds Return without replacing final text or the next question");
	chat->present_revision_continuation(revisionChoice, { }, false);
	auto* const continueOld = chat->findChild<QToolButton*>(QStringLiteral("tourContinueOldRevision"));
	expect(continueOld && !continueOld->isEnabled(), "retirement withholds continuation of the saved revision");
	editor.set_tour_host_idle(true);
	expect(continueOld->isEnabled(), "the idle host permits the existing revision choice");
	chat->present_revision_continuation(std::nullopt, { }, false);
	editor.set_tour_host_idle(true);
	expect(chat->send_button()->isEnabled() && chat->new_session_action()->isEnabled() && chat->can_update() && chat->unchanged_line()->text().contains(QStringLiteral("<a ")),
		   "the observed idle host enables the existing request capabilities");
	chat->composer()->clear();
	// [[BPR-1925]]. Its glyph counts its turns.
	expect(glyph->text() == QStringLiteral("1") && chat->turns_about(partThread) == 1U, "the stop's toggle counts its one turn");
	expect(glyph->isChecked() && glyph->accessibleName().startsWith(QStringLiteral("Close the chat")), "pressed, it says it closes the chat: " + glyph->accessibleName().toStdString());
	expect(chat->chat_parts() == 1U, "one part has a chat");
	expect(chat->turn_count() == 1U && chat->bubble(0) != nullptr && chat->bubble(0)->findChild<QLabel*>(QStringLiteral("tourSideChatQuestion"))->text() == QStringLiteral("What does this mean?"), "the author's words are a bubble");
	expect(offered() == QStringList{ QStringLiteral("Explain the selected block") } && !chat->suggestions()->lead()->isVisibleTo(chat),
		   "AC1 the stop's prompts go once the chat has a turn; Explain the selected block stays while text is selected");
	expect(chat->bubble(0)->geometry().right() > chat->reply(0)->geometry().left() + chat->reply(0)->width() / 2, "on the right, the reply on the left");
	expect(!chat->status_line(0)->live() && chat->status_line(0)->text() == QStringLiteral("Claude Code answered in 0:07.") && !chat->status_line(0)->font().italic(), "AC2 its ending says who answered and how long it took, as the Agent tab's does: " + chat->status_line(0)->text().toStdString());
	expect(chat->status_line(0)->geometry().top() >= chat->reply(0)->geometry().bottom(), "under the reply");
	expect(chat->activity_view(0) != nullptr && chat->activity_view(0)->summary_text() == QStringLiteral("Worked for 2 min: 1 helper, 1 file read"), "AC08 the block folds under the ended turn: " + (chat->activity_view(0) != nullptr ? chat->activity_view(0)->summary_text().toStdString() : std::string{ }));
	expect(chat->resend_button(0) == nullptr && chat->delete_button(0) == nullptr, "an answer offers no Resend or Delete");
	auto* const clipboard = QGuiApplication::clipboard();
	chat->copy_reply_button(0)->click();
	expect(clipboard->text().startsWith(QStringLiteral("It is the frame window:")), "Copy takes the reply as written");
	chat->copy_chat_button()->click();
	expect(clipboard->text().startsWith(QStringLiteral("## Side chat: Stop 2")) && clipboard->text().contains(QStringLiteral("**You:** What does this mean?")) && clipboard->text().contains(QStringLiteral("**Claude Code** (answered in 0:07):")), "Copy chat takes the whole chat: " + clipboard->text().toStdString());

	// (3) Update this stop from our chat: an Update turn, and its proposal
	// for the stop decided in the chat, not in the draft's box.
	// [[BPR-1995]], RFC-BPR-0100 AC02: the only way the update is offered is
	// the link under the answer that changed nothing, naming the part.
	expect(chat->unchanged_line() != nullptr && chat->unchanged_line()->text() == QStringLiteral("Nothing in this stop changed. <a href=\"#update\">Update this stop from this chat</a>"), "the link names the part: " + chat->unchanged_line()->text().toStdString());
	expect(chat->findChild<QPushButton*>(QStringLiteral("tourSideChatUpdate")) == nullptr, "no other control offers the update");
	emit chat->unchanged_line()->linkActivated(QStringLiteral("#update"));
	expect(asked.size() == 2U && asked[1].Update && asked[1].Thread == partThread && asked[1].Question == QStringLiteral("Update this stop's explanation from our chat."), "Update asks for the part rewritten from the chat");
	const auto update  = core::begin_tour_ask(session, asked[1].Thread, std::nullopt, asked[1].Question.toStdString(), true);
	const auto answer  = core::tour_ask_answer{ .Reply = "Rewritten.", .StopTexts = { { 2U, "Both views share one frame:\n\n```cpp\nauto lock = std::scoped_lock{ Mutex };\nreturn *Table;\n```\n" } } };
	const auto changes = core::keep_thread_part(core::propose_tour_changes(*update, answer, { }, draft), asked[1].Thread);
	static_cast<void>(core::answer_tour_ask(session, *update, "I rewrote this stop from our chat, around `auto x = 1;`.", core::tally_of(changes)));
	editor.present_tour_session(session, changes);
	settle();
	expect(chat->turn_count() == 2U && chat->accept_button(0) != nullptr && chat->proposal_reader(0) != nullptr, "the proposed explanation is in the chat, with Accept");
	expect(editor.tour_agent()->accept_button(0) == nullptr, "and not in the draft's box");
	render(*chat, prefix, "side-chat-stop-update");
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
	// [[BPR-2275]]. Going to Stop 1 brought its chat; the replies below are
	// Stop 2's, so the course goes back to it.
	editor.go_to_stop(core::tour_stop_id{ 2 });
	syntax.deliver(editor);
	expect(chat->current() == partThread, "going back to Stop 2 brings its chat back");
	expect(colours(*chat->reply(0), QStringLiteral("scoped_lock")) == stopOwn, "AC1 the reply's C++ in the same colours");
	expect(colours(*chat->proposal_reader(0), QStringLiteral("scoped_lock")) == stopOwn, "AC1 the proposal's too");
	expect(colours(*chat->reply(0), QStringLiteral("plain_text")).size() <= 1U, "AC2 an unknown language stays plain");
	// [[BPR-1944]]. The reply's blocks are boxes as wide as the chat, each
	// with its language, Wrap and Copy, coloured as before; a proposal's code
	// reads as its stop will once published.
	{
		auto boxes = chat->reply(0)->findChildren<QWidget*>(QStringLiteral("markdownCodeBlock"));
		std::ranges::sort(boxes, [](const QWidget* aFirst, const QWidget* aSecond) { return aFirst->y() < aSecond->y(); });
		expect(boxes.size() == 2 && boxes[0]->accessibleName() == QStringLiteral("C++ code") && boxes[1]->accessibleName() == QStringLiteral("nosuchlanguage code"), "AC1 the tour chat's reply shows each block as a box named by its language");
		for (auto* const box : boxes) {
			expect(box->isVisibleTo(chat->reply(0)) && box->findChild<QToolButton*>(QStringLiteral("markdownCodeBlockWrap")) != nullptr && box->findChild<QToolButton*>(QStringLiteral("markdownCodeBlockCopy")) != nullptr, "AC1 with Wrap and Copy");
			expect(box->width() == chat->reply(0)->rendered_view()->viewport()->width(), "AC7 as wide as the reply");
		}
		expect(chat->reply(0)->rendered_view()->horizontalScrollBar()->maximum() == 0, "AC7 the reply has no horizontal scroll range");
		if (boxes.size() == 2) {
			auto  boxed = std::set<QRgb>{ };
			auto* text  = boxes[0]->findChild<QTextEdit*>(QStringLiteral("markdownCodeBlockText"));
			for (auto block = text->document()->begin(); block.isValid(); block = block.next()) {
				if (block.text().contains(QStringLiteral("scoped_lock")) && block.layout() != nullptr) {
					for (const auto& range : block.layout()->formats()) {
						boxed.insert(range.format.foreground().color().rgb());
					}
				}
			}
			expect(boxed == stopOwn, "AC4 the box's C++ in the stop reader's colours");
			boxes[0]->findChild<QToolButton*>(QStringLiteral("markdownCodeBlockCopy"))->click();
			expect(QGuiApplication::clipboard()->text() == QStringLiteral("auto lock = std::scoped_lock{ Mutex };\nreturn *Table;"), "AC2 Copy in the tour chat takes exactly the code");
		}
		expect(chat->proposal_reader(0)->findChildren<QWidget*>(QStringLiteral("markdownCodeBlock")).isEmpty(), "a proposal's code reads as its stop will");
	}
	// The composed chat stays bounded with genuinely wide code too.
	{
		auto       wideSession = session;
		const auto wideRun     = core::begin_tour_ask(wideSession, asked[0].Thread, std::nullopt, "Show a long line");
		expect(wideRun.has_value(), "start the wide-code turn");
		static_cast<void>(core::answer_tour_ask(wideSession, *wideRun, "```\n" + std::string(400, 'w') + "\n```", { }));
		editor.present_tour_session(wideSession, changes);
		settle();
		auto* const view = chat->reply(chat->turn_count() - 1U);
		auto* const box  = view->findChild<QWidget*>(QStringLiteral("markdownCodeBlock"));
		auto* const text = box == nullptr ? nullptr : box->findChild<QTextEdit*>(QStringLiteral("markdownCodeBlockText"));
		auto* const pane = chat->findChild<QScrollArea*>(QStringLiteral("tourSideChatScroll"));
		expect(text != nullptr && text->horizontalScrollBar()->maximum() > 0, "AC7 a 400-character line scrolls inside the tour reply's code block");
		expect(pane != nullptr && pane->horizontalScrollBar()->maximum() == 0 && view->rendered_view()->horizontalScrollBar()->maximum() == 0, "AC7 neither tour pane nor message has a horizontal scroll range");
		expect(box != nullptr && box->width() == view->rendered_view()->viewport()->width() && view->width() <= pane->viewport()->width(), "AC7 the tour reply stays inside the column");
		editor.present_tour_session(session, changes);
		settle();
		syntax.deliver(editor);
	}
	// [[BPR-1998]], RFC-BPR-0101 AC01. The reply's references are links by
	// the Agent tab's recogniser: prose, emphasis and a whole code span, not
	// fenced code; a file the pull request does not change is plain; Copy
	// keeps the reply as written.
	{
		auto went = std::vector<std::pair<QString, std::uint32_t>>{ };
		chat->set_reference_links([] { return core::agent_reference_files{ .Changed = { "src/frame.cpp" } }; },
								  [&went](const QString& aPath, const std::uint32_t aFirst, std::uint32_t, bool) { went.emplace_back(aPath, aFirst); });
		auto       linked = session;
		const auto run    = core::begin_tour_ask(linked, asked[0].Thread, std::nullopt, "Where?");
		expect(run.has_value(), "start the references turn");
		static_cast<void>(core::answer_tour_ask(linked, *run, "See src/frame.cpp:3-5, **frame.cpp line 7**, `frame.cpp:9` and other.cpp:2.\n\n```\nframe.cpp:11\n```\n", { }));
		editor.present_tour_session(linked, changes);
		settle();
		auto* const view = chat->reply(chat->turn_count() - 1U);
		const auto  body = view == nullptr ? QString{ } : QString::fromStdString(view->original_body());
		expect(body.contains(QStringLiteral("[src/frame.cpp:3-5](src/frame.cpp#L3-L5)")) && body.contains(QStringLiteral("[frame.cpp line 7](src/frame.cpp#L7)"))
				   && body.contains(QStringLiteral("[`frame.cpp:9`](src/frame.cpp#L9)")) && !body.contains(QStringLiteral("other.cpp#")) && body.contains(QStringLiteral("```\nframe.cpp:11\n```")),
			   "AC01 the tour side chat's reply links each form, and not in fenced code: " + body.toStdString());
		chat->copy_reply_button(chat->turn_count() - 1U)->click();
		expect(QGuiApplication::clipboard()->text().startsWith(QStringLiteral("See src/frame.cpp:3-5, **frame.cpp line 7**")), "Copy keeps the reply as written");
		chat->set_reference_links({ }, { });
		editor.present_tour_session(session, changes);
		settle();
		syntax.deliver(editor);
	}
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
	// Stop 2's selection ends; none is offered for it any more.
	editor.begin_stop_edit();
	settle();
	{
		auto none = written->textCursor();
		none.setPosition(0);
		none.setPosition(4, QTextCursor::KeepAnchor);
		written->setTextCursor(none);
		none.clearSelection();
		written->setTextCursor(none);
	}
	editor.finish_stop_edit();
	settle();
	actions.clear();
	editor.go_to_stop(core::tour_stop_id{ 1 });
	settle();
	if (chat->current() != core::tour_thread{ .Kind = core::tour_thread_kind::stop, .Stop = core::tour_stop_id{ 1 } }) {
		stopGlyph->click();
		settle();
	}
	expect(chat->chat_count() == 2U && chat->current() == core::tour_thread{ .Kind = core::tour_thread_kind::stop, .Stop = core::tour_stop_id{ 1 } }, "switching parts switches the chat: the stop's");
	expect(stopGlyph->isChecked() && chat->turns_about(partThread) == 2U, "Stop 1's glyph is pressed now; Stop 2's chat keeps its turns");
	auto* const route = editor.findChild<QListWidget*>(QStringLiteral("tourOutline"));
	expect(route != nullptr && route->item(0)->data(Qt::UserRole + 10).toBool(), "the stop is marked on the route");
	expect(std::ranges::count(actions, qt::tour_editor_action::navigate_stop) >= 1, "the stop's lines are shown in the code");
	expect(chat->subject()->text() == QStringLiteral("Stop 1, Why") && chat->findChild<QLabel*>(QStringLiteral("tourSideChatQuote"))->text() == QStringLiteral("src/item.cpp, lines 4 to 6"), "the top names the stop and its lines");
	settle();
	expect(offered()
			   == QStringList{ QStringLiteral("What does this code do?"), QStringLiteral("Why does this change matter?"), QStringLiteral("Rewrite this explanation more simply"), QStringLiteral("Is this the right place for this stop?") },
		   "AC1 the empty stop chat offers the stop's prompts: " + offered().join(QLatin1Char('|')).toStdString());
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
	expect(asked.back().SkillText == std::optional<std::string>{ "" }, "RFC-BPR-0125 AC09 a suggested prompt sent as offered reads skills only from its own text");
	expect(asked.back().Name.isEmpty(), "BPR-2235 a prompt whose text is its name is sent with no other name");
	// [[BPR-2235]]. A turn sent under a prompt's name (kept by the host for a
	// restart) shows the agent glyph and the name; what was sent is behind
	// Show prompt, as in the Agent tab.
	chat->set_turn_name_source([](const std::string& aAsked) { return aAsked == "Too many stops?" ? QStringLiteral("Shape check") : QString{ }; });
	const auto tourTurn = core::begin_tour_ask(session, asked.back().Thread, std::nullopt, "Too many stops?");
	static_cast<void>(core::answer_tour_ask(session, *tourTurn, "Four stops is about right.", { }));
	editor.present_tour_session(session, std::nullopt);
	settle();
	{
		auto* const bubble   = chat->bubble(0);
		auto* const name     = bubble != nullptr ? bubble->findChild<QLabel*>(QStringLiteral("tourSideChatPromptName")) : nullptr;
		auto* const mark     = bubble != nullptr ? bubble->findChild<QLabel*>(QStringLiteral("tourSideChatPromptGlyph")) : nullptr;
		auto* const fold     = bubble != nullptr ? bubble->findChild<QPushButton*>(QStringLiteral("tourSideChatShowPrompt")) : nullptr;
		auto* const question = bubble != nullptr ? bubble->findChild<QLabel*>(QStringLiteral("tourSideChatQuestion")) : nullptr;
		expect(name != nullptr && name->text() == QStringLiteral("Shape check") && mark != nullptr && !mark->pixmap().isNull() && fold != nullptr && fold->text() == QStringLiteral("Show prompt") && question != nullptr
				   && !question->isVisibleTo(chat),
			   "BPR-2235 AC-a a prompt's turn shows its name and glyph, what was sent hidden");
		render(*bubble, prefix, "bpr-2235-tour-prompt-turn");
		fold->click();
		settle();
		expect(question->isVisibleTo(chat) && question->text() == QStringLiteral("Too many stops?") && fold->text() == QStringLiteral("Hide prompt"), "BPR-2235 AC-b Show prompt shows what was sent");
		fold->click();
		settle();
		expect(!question->isVisibleTo(chat), "and Hide prompt hides it again");
	}
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
		auto* const typed = chat->bubble(chat->turn_count() - 1U);
		expect(typed != nullptr && typed->findChild<QLabel*>(QStringLiteral("tourSideChatPromptName")) == nullptr && typed->findChild<QLabel*>(QStringLiteral("tourSideChatQuestion"))->isVisibleTo(chat)
				   && typed->findChild<QLabel*>(QStringLiteral("tourSideChatQuestion"))->text() == QStringLiteral("Lower, please."),
			   "BPR-2235 AC-c a typed message shows as typed");
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
	editor.show_stops();
	editor.go_to_stop(core::tour_stop_id{ 2 });
	settle();
	realClick(glyph);
	expect(chat->current() == partThread && chat->turn_count() == 2U, "one click on Stop 2's glyph brings its chat back, turns and all");
	chat->copy_chat_button()->click();
	expect(clipboard->text().startsWith(QStringLiteral("## Side chat: Stop 2")) && clipboard->text().contains(QStringLiteral("Update this stop's explanation from our chat.")), "and it copies whole");
	// Close: the column goes and every chat is kept.
	realClick(chat->close_button());
	expect(!chat->column_open() && !shown.back() && !chat->discussed() && !glyph->isChecked() && glyph->text() == QStringLiteral("2"), "close takes the column away; the glyph keeps its count");
	chat->open(partThread);
	settle();
	expect(chat->turn_count() == 2U && chat->column_open(), "reopened, a chat still has its turns");

	// (6) [[BPR-1941]] AC2. A turn that ended without an answer, as the Agent
	// tab's error turns: why, "Stopped after", Resend and Delete.
	{
		auto deleted = std::vector<core::tour_agent_run_id>{ };
		chat->set_delete_dispatch([&deleted](const core::tour_agent_run_id aRun) { deleted.push_back(aRun); });
		const auto firstStop = core::tour_thread{ .Kind = core::tour_thread_kind::stop, .Stop = core::tour_stop_id{ 1 } };
		chat->open(firstStop);
		const auto broken = core::begin_tour_ask(session, firstStop, std::nullopt, "Is this stop right?");
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
		editor.set_tour_host_idle(false);
		expect(!chat->resend_button(0)->isEnabled() && chat->delete_button(0)->isEnabled(), "retirement withholds Resend and keeps Delete");
		chat->resend_button(0)->click();
		expect(asked.size() == before, "a retiring host receives no recovery request");
		editor.set_tour_host_idle(true);
		auto pendingHistory    = application::native_agent_history_projection{ };
		pendingHistory.Loading = true;
		chat->present_native_history(pendingHistory);
		expect(!chat->resend_button(0)->isEnabled() && chat->delete_button(0)->isEnabled(), "native history loading withholds Resend and keeps Delete");
		chat->resend_button(0)->click();
		expect(asked.size() == before, "pending history admits no recovery request even after retirement");
		pendingHistory.Loading = false;
		chat->present_native_history(pendingHistory);
		expect(chat->resend_button(0)->isEnabled(), "settled history restores the existing recovery request");
		chat->resend_button(0)->click();
		expect(asked.size() == before + 1U && asked.back().Thread == firstStop && asked.back().Question == QStringLiteral("Is this stop right?") && !asked.back().Update, "AC2 Resend asks the same again in the same chat");
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
		editor.set_tour_host_idle(false);
		editor.set_tour_running(std::nullopt, true);
		auto live  = qt::agent_run_status{ };
		live.Agent = QStringLiteral("Claude Code");
		live.Stage = qt::agent_run_stage::thinking;
		editor.present_run_status(live);
		settle();
		expect(chat->draft_status_line() != nullptr && chat->draft_status_line()->isVisibleTo(chat) && chat->draft_status_line()->text() == QStringLiteral("● Claude Code is thinking, 0:00."), "the tour's chat says the draft runs");
		expect(editor.draft_status_line()->isVisibleTo(&editor) && editor.draft_status_line()->text() == chat->draft_status_line()->text(), "the tab's notice says it the same way");
		chat->composer()->setPlainText(QStringLiteral("Keep the draft focused"));
		const auto beforeFeedback = asked.size();
		expect(chat->send_button()->isEnabled(), "the existing queued-draft feedback exception does not require an idle host");
		chat->send_button()->click();
		expect(asked.size() == beforeFeedback + 1U && asked.back().Thread.Kind == core::tour_thread_kind::tour && !asked.back().Update,
			   "draft feedback keeps its one existing dispatch while the host runs");
		render(*chat, prefix, "side-chat-drafting");
		editor.present_run_status(std::nullopt);
		editor.set_tour_running(std::nullopt, false);
		editor.set_tour_host_idle(true);
		settle();
		expect(chat->draft_status_line() == nullptr && !editor.draft_status_line()->isVisibleTo(&editor), "both go when it ends");
	}

	// (8) [[BPR-1995]], RFC-BPR-0100 AC01, AC02, AC05. Under the transcript,
	// with a native session and an unmoved head: the composer and the session
	// line only; a replay or a moved head adds exactly its one notice. The
	// update link in a stop's thread; Enhance as an update turn in the part's
	// session, a selection with its passage.
	{
		auto       footSession = core::tour_agent_session{ };
		auto       stopThread  = core::tour_thread{ .Kind = core::tour_thread_kind::stop, .Stop = draft.Stops.front().Id };
		const auto said        = core::begin_tour_ask(footSession, stopThread, std::nullopt, "Why clear first?");
		static_cast<void>(core::answer_tour_ask(footSession, *said, "Because the frame is kept.", { }));
		static_cast<void>(core::cost_tour_run(footSession, *said, core::agent_cost{ 40000000U }, core::agent_cost{ 40000000U }));
		editor.present_tour_session(footSession, std::nullopt);
		chat->open(stopThread);
		auto view     = application::native_agent_history_projection{ };
		view.Entries  = { application::native_agent_history_entry{ .Id = "newest" }, application::native_agent_history_entry{ .Id = "older" } };
		view.Selected = "older";
		chat->present_native_history(view);
		chat->present_revision_continuation(std::nullopt, { }, false);
		settle();
		const auto under = [chat] {
			auto visible = QStringList{ };
			for (auto* const child : chat->findChildren<QWidget*>(Qt::FindDirectChildrenOnly)) {
				if (child->isVisibleTo(chat) && child->objectName() != QStringLiteral("tourSideChatScroll") && child->y() > chat->findChild<QWidget*>(QStringLiteral("tourSideChatScroll"))->geometry().bottom()) {
					visible << child->objectName();
				}
			}
			visible.sort();
			return visible;
		};
		// [[BPR-2337]]. Send is inside the composer's field, as the Agent tab's.
		expect(chat->send_button()->parentWidget() == chat->composer_box(), "Send is in the composer's field");
		expect(under() == QStringList{ QStringLiteral("tourChatSession"), QStringLiteral("tourSideChatField"), QStringLiteral("tourSideChatSessionCost") },
			   "AC01 under the transcript only the composer and the session line: " + under().join(QLatin1Char(',')).toStdString());
		expect(chat->session_button()->text() == QStringLiteral("Session 1") && chat->session_cost_line()->text() == QStringLiteral("$0.04 this session"), "the session line: its ordinal on the left, its cost on the right: " + chat->session_button()->text().toStdString());
		expect(chat->session_button()->menu()->actions().contains(chat->new_session_action()) && chat->new_session_action()->text() == QStringLiteral("New session"), "New session is an item of the Session menu");
		expect(chat->session_button()->geometry().top() > chat->composer_box()->geometry().bottom(), "the session line is under the composer");
		render(*chat, prefix, "side-chat-foot");
		// [[BPR-2377]]. The Session menu holds the same session rows as the
		// Agent tab's, and their edits reach the chat's dispatch for the thread.
		{
			auto edits = std::vector<std::pair<std::string, std::optional<std::string>>>{ };
			chat->set_session_edit_dispatch([&edits](const core::tour_thread&, const std::string& aId, std::optional<std::string> aName) { edits.emplace_back(aId, std::move(aName)); });
			const auto sessionActions = chat->session_button()->menu()->actions();
			expect(sessionActions.contains(&chat->session_rows()) && sessionActions.contains(chat->session_rows().delete_all_action()), "the Session menu holds the session rows and Delete all sessions…");
			expect(chat->session_rows().ids() == std::vector<std::string>{ "newest", "older" } && chat->session_rows().selected() == "older", "the rows are the thread's sessions");
			chat->session_rows().rename("older", QStringLiteral("Stop one notes"));
			expect(edits.size() == 1U && edits.back().first == "older" && edits.back().second == std::string{ "Stop one notes" }, "a rename reaches the chat's dispatch");
			chat->session_rows().delete_all_action()->trigger();
			settle();
			auto* const question = chat->session_rows().question();
			expect(question != nullptr && question->isVisible(), "Delete all asks first");
			question->findChild<QPushButton*>(QStringLiteral("deleteAllAgentSessionsConfirm"))->click();
			settle();
			expect(edits.size() == 2U && edits.back().first.empty() && !edits.back().second, "Delete all reaches the chat's dispatch as every session");
			chat->present_session_edit(QStringLiteral("Renamed."));
			expect(chat->session_rows().status() == QStringLiteral("Renamed."), "the outcome is said under the rows");
		}
		view.Selected = { };
		chat->present_native_history(view);
		expect(chat->session_button()->text() == QStringLiteral("Session 3"), "a chat with no session selected starts the next one");
		view.Replay = true;
		chat->present_native_history(view);
		settle();
		expect(chat->notice()->isVisibleTo(chat) && chat->findChild<QLabel*>(QStringLiteral("tourContinuationMode"))->isVisibleTo(chat) && !chat->findChild<QLabel*>(QStringLiteral("tourRevisionStatus"))->isVisibleTo(chat),
			   "AC01 a replay adds its one notice");
		expect(chat->notice()->geometry().bottom() <= chat->composer_box()->geometry().top(), "above the composer");
		render(*chat, prefix, "side-chat-foot-replay");
		view.Replay = false;
		chat->present_native_history(view);
		auto moved     = core::agent_continuation_offer{ };
		moved.Required = true;
		chat->present_revision_continuation(moved, { }, false);
		settle();
		expect(chat->notice()->isVisibleTo(chat) && !chat->findChild<QLabel*>(QStringLiteral("tourContinuationMode"))->isVisibleTo(chat) && chat->findChild<QLabel*>(QStringLiteral("tourRevisionStatus"))->isVisibleTo(chat)
				   && chat->findChild<QToolButton*>(QStringLiteral("tourContinueOldRevision"))->isVisibleTo(chat),
			   "AC01 a moved head adds its one notice, with Continue old revision");
		chat->present_revision_continuation(std::nullopt, { }, false);
		settle();
		expect(!chat->notice()->isVisibleTo(chat), "and it goes with the cause");
		// AC02 in a stop's thread.
		expect(chat->unchanged_line() != nullptr && chat->unchanged_line()->text() == QStringLiteral("Nothing in this stop changed. <a href=\"#update\">Update this stop from this chat</a>"), "AC02 the stop's thread names the stop: " + (chat->unchanged_line() ? chat->unchanged_line()->text().toStdString() : std::string{ }));
		const auto before = asked.size();
		emit       chat->unchanged_line()->linkActivated(QStringLiteral("#update"));
		expect(asked.size() == before + 1U && asked.back().Update && asked.back().Thread == stopThread && asked.back().Question == QStringLiteral("Update this stop's explanation from our chat.") && !asked.back().Passage, "following it sends the stop's update turn");
		// AC05. Enhance from the stop's own button, and on a selection.
		// [[BPR-2138]], RFC-BPR-0101 AC11 (owner, 2026-11-12): a fresh run
		// whose result is a proposal on the part; the side chat is neither
		// opened nor sent a turn. [[BPR-2275]]: on Stop 2, which has words.
		editor.show_stops();
		editor.go_to_stop(core::tour_stop_id{ 2 });
		editor.begin_stop_edit();
		settle();
		auto* const stopEnhance = editor.findChild<QPushButton*>(QStringLiteral("tourStopMagic"));
		expect(stopEnhance != nullptr && stopEnhance->property("enhanceOnly").toBool(), "the stop has words, so its button is Enhance");
		const auto shownBefore = chat->current();
		stopEnhance->click();
		expect(asked.size() == before + 2U && asked.back().Update && asked.back().Fresh && asked.back().Thread == partThread && !asked.back().Passage && chat->current() == shownBefore,
			   "AC11 Enhance asks for a fresh rewrite of the stop and leaves its side chat as it was: " + asked.back().Question.toStdString());
		const auto passage = core::tour_passage{ .Field = core::tour_passage_field::stop, .Text = "clears" };
		expect(chat->enhance(partThread, passage) && asked.back().Passage == std::optional{ passage } && asked.back().Update && asked.back().Fresh, "AC05 Enhance on a selection sends the selection as the update's passage: " + asked.back().Question.toStdString());
		editor.set_tour_host_idle(false);
		expect(!chat->enhance(partThread, std::nullopt), "Enhance waits while the agent is busy");
		editor.set_tour_host_idle(true);
		// [[BPR-2138]], RFC-BPR-0101 AC12. The fresh Enhance's suggestion
		// offers Ask more, flat beside Discard; it names the fresh run.
		{
			auto       freshSession = core::tour_agent_session{ };
			const auto enhanced     = core::begin_tour_ask(freshSession, partThread, std::nullopt, "Enhance the writing of this stop.", true);
			expect(enhanced.has_value(), "a fresh Enhance run begins");
			for (auto& made : freshSession.Runs) {
				if (made.Id == *enhanced) {
					made.Fresh = true;
				}
			}
			auto suggested        = core::tour_ask_answer{ };
			suggested.Reply       = "Tightened the stop.";
			suggested.StopTexts   = { { 2U, "Each view clears between begin and present, so two never share a frame." } };
			const auto suggestion = core::propose_tour_changes(*enhanced, suggested, { }, draft);
			static_cast<void>(core::answer_tour_ask(freshSession, *enhanced, suggested.Reply, core::tally_of(suggestion)));
			auto askedMore = std::vector<core::tour_agent_run_id>{ };
			editor.set_tour_ask_more_dispatch([&askedMore](const core::tour_agent_run_id aRun) { askedMore.push_back(aRun); });
			editor.present_tour_session(freshSession, suggestion);
			chat->open(partThread);
			settle();
			auto* const more = chat->findChild<QPushButton*>(QStringLiteral("tourSideChatAskMore"));
			expect(more != nullptr && more->text() == QStringLiteral("Ask more") && !more->isFlat() && more->property("bprTourAction").toBool(), "AC12 the fresh suggestion offers Ask more, in the one Tour action style (owner, 2026-11-13)");
			more->click();
			expect(askedMore.size() == 1U && askedMore[0] == *enhanced, "AC12 Ask more names the fresh run whose answer the chat continues from");
			render(*chat, prefix, "side-chat-ask-more");
		}
	}
	// (9) [[BPR-1995]], RFC-BPR-0100 AC03, AC04. A change request in the
	// whole tour's chat, answered in its turn with a revision: the card.
	{
		auto       wholeSession  = core::tour_agent_session{ };
		const auto turn          = core::begin_tour_ask(wholeSession, core::tour_thread{ }, std::nullopt, "Drop the first stop.");
		auto       revised       = core::tour_ask_answer{ };
		revised.Reply            = "Dropped stop 1.";
		revised.Revision         = core::tour_revision{ };
		revised.Revision->Remove = { 1U };
		const auto revision      = core::propose_tour_changes(*turn, revised, { }, draft);
		static_cast<void>(core::answer_tour_ask(wholeSession, *turn, revised.Reply, core::tally_of(revision)));
		editor.present_tour_session(wholeSession, revision);
		chat->open(core::tour_thread{ });
		settle();
		expect(chat->revision_card() != nullptr && chat->revision_operations()->text() == QStringLiteral("Remove stop 1") && chat->unchanged_line() == nullptr,
			   "a request answered with its revision shows the card, and no update link: " + (chat->revision_operations() ? chat->revision_operations()->text().toStdString() : std::string{ }));
		render(*chat, prefix, "side-chat-revision-card");
	}

	// (10) [[BPR-1996]], RFC-BPR-0100 AC06. A stop's references: a link that
	// opens its lines in Review; alone on its line, the permalink of at most
	// 12 of its lines (a snippet) and Show in Review; one that names no file
	// of the pull request stays as written.
	{
		auto opened = std::vector<std::tuple<QString, std::uint32_t, std::uint32_t>>{ };
		editor.set_reference_links([] { return core::agent_reference_files{ .Changed = { "src/item.cpp" } }; },
								   [&opened](const QString& aPath, const std::uint32_t aFirst, const std::uint32_t aLast, bool) { opened.emplace_back(aPath, aFirst, aLast); },
								   [](const std::string& aPath, const std::uint32_t aFirst, const std::uint32_t aLast, const bool aBefore) -> std::optional<std::string> {
									   return "https://github.com/o/r/blob/" + std::string(aBefore ? "base" : "head") + "/" + aPath + "#L" + std::to_string(aFirst) + "-L" + std::to_string(aLast);
								   },
								   [](const std::string&) { });
		auto cited               = draft;
		cited.Stops.front().Body = "Clears first (`item.cpp:4-6`), unlike missing.cpp:3.\n\nsrc/item.cpp:4-30\n";
		editor.present({ .Draft = std::make_shared<const core::review_tour>(cited) });
		editor.show_stops();
		editor.go_to_stop(cited.Stops.front().Id);
		settle();
		auto* const reader = static_cast<qt::markdown_body_view*>(editor.findChild<QWidget*>(QStringLiteral("tourStopReader")));
		const auto  body   = reader->original_body();
		expect(body == "Clears first ([`item.cpp:4-6`](src/item.cpp#L4-L6)), unlike missing.cpp:3.\n\nhttps://github.com/o/r/blob/head/src/item.cpp#L4-L15\n\n[Show in Review](src/item.cpp#L4-L30)\n",
			   "AC06 an inline reference is a link, one alone a snippet of 12 lines with Show in Review, one naming no file stays as written: " + body);
		expect(qt::follow_agent_reference(reader, QStringLiteral("src/item.cpp#L4-L30"), [&opened](const QString& aPath, const std::uint32_t aFirst, const std::uint32_t aLast, bool) { opened.emplace_back(aPath, aFirst, aLast); }) && opened.size() == 1U
				   && opened[0] == std::tuple{ QStringLiteral("src/item.cpp"), 4U, 30U },
			   "Show in Review opens those lines");
		editor.present({ .Draft = shared });
		settle();
	}

	std::cout << "tour-side-chat:pass\n";
	return EXIT_SUCCESS;
}
