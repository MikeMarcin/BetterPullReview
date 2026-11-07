// [[BPR-1914]] v003 and [[BPR-1919]]. Side chats: each part of the tour opens
// its own chat from a quiet glyph or its menu, docked beside the tour while
// the tour stays as it is; a plain chat with the author's bubbles, rendered
// replies, Worked for Ns and Copy; Update the part from this chat comes back
// as the part's proposal, decided in the chat. Fenced code in replies and
// proposals is highlighted as the stop's own explanation is, and code in an
// unknown language stays plain.
#include "../support/review_tour_fixture.hpp"
#include "bpr/adapters/syntax/packaged_syntax_classifier.hpp"
#include "bpr/application/markdown_code_syntax.hpp"
#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/tour_agent_panel.hpp"
#include "bpr/qt/tour_side_chat.hpp"

#include <QApplication>
#include <QClipboard>
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

	// (1) The tour stays clean: one quiet glyph per part, and nothing of a
	// chat in the tour's column.
	editor.open_step(core::tour_step::solution);
	settle();
	auto* const solution = editor.findChild<QPlainTextEdit*>(QStringLiteral("tourSolution"));
	auto* const glyph    = editor.findChild<QToolButton*>(QStringLiteral("tourSolutionChat"));
	expect(glyph != nullptr && glyph->isVisibleTo(&editor) && glyph->toolButtonStyle() == Qt::ToolButtonIconOnly && glyph->toolTip() == QStringLiteral("Open a side chat about the Solution"), "the Solution has one quiet side chat glyph");
	expect(!editor.isAncestorOf(chat) && chat->isWindow() && chat->findChild<QTabBar*>() == nullptr, "the chat is its own column, for the window to place beside the tour, with no tabs");
	expect(!chat->isVisible() && shown.empty(), "no chat is open until asked for");
	auto cursor = solution->textCursor();
	cursor.setPosition(static_cast<int>(QStringLiteral("Each view now clears ").size()));
	cursor.setPosition(static_cast<int>(QStringLiteral("Each view now clears between begin and present").size()), QTextCursor::KeepAnchor);
	solution->setTextCursor(cursor);
	settle();
	expect(glyph->toolTip() == QStringLiteral("Open a side chat about the selected text"), "with text selected it opens a chat on that passage");
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
	chat->composer()->setPlainText(QStringLiteral("What does this mean?"));
	auto enter = QKeyEvent{ QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier };
	QApplication::sendEvent(chat->composer(), &enter);
	expect(asked.size() == 1U && asked[0].Thread.Kind == core::tour_thread_kind::solution && asked[0].Passage && asked[0].Question == QStringLiteral("What does this mean?") && !asked[0].Update, "Return asks in the Solution's chat, with the passage");
	expect(chat->composer()->toPlainText().isEmpty() && !chat->passage(), "the composer clears");

	// (2) A plain chat: the bubble, Worked for Ns with its steps behind it,
	// the reply rendered, Copy on it and on the whole chat.
	auto       session = core::tour_agent_session{ };
	const auto first   = core::begin_tour_ask(session, asked[0].Thread, asked[0].Passage, "What does this mean?");
	editor.set_tour_running(std::nullopt, false, true);
	editor.present_tour_session(session, std::nullopt);
	settle();
	expect(chat->send_button()->text() == QStringLiteral("Stop") && chat->send_button()->isEnabled(), "a running turn is stopped from Send");
	static_cast<void>(core::record_tour_activity(session, *first, { { core::tour_transcript_kind::read, "src/item.cpp" }, { core::tour_transcript_kind::searched, "rg -n lock src" } }));
	static_cast<void>(core::answer_tour_ask(session, *first, "It is the frame window:\n\n```cpp\nauto lock = std::scoped_lock{ Mutex };\nreturn *Table;\n```\n\n```nosuchlanguage\nauto lock = plain_text{ };\n```\n", { }));
	static_cast<void>(core::time_tour_run(session, *first, 7U));
	editor.set_tour_running(std::nullopt, false, false);
	editor.present_tour_session(session, std::nullopt);
	settle();
	// [[BPR-1925]]. Its glyph counts its turns.
	expect(glyph->text() == QStringLiteral("1") && glyph->toolButtonStyle() == Qt::ToolButtonTextBesideIcon && chat->turns_about(core::tour_thread{ .Kind = core::tour_thread_kind::solution }) == 1U, "the Solution's glyph counts its one turn");
	expect(chat->chat_parts() == 1U, "one part has a chat");
	expect(chat->turn_count() == 1U && chat->bubble(0) != nullptr && chat->bubble(0)->findChild<QLabel*>(QStringLiteral("tourSideChatQuestion"))->text() == QStringLiteral("What does this mean?"), "the author's words are a bubble");
	expect(chat->bubble(0)->geometry().right() > chat->reply(0)->geometry().left() + chat->reply(0)->width() / 2, "on the right, the reply on the left");
	expect(chat->worked_button(0)->text() == QStringLiteral("Worked for 7s") && !chat->steps(0)->isVisibleTo(chat), "Worked for 7s, its steps folded away");
	chat->worked_button(0)->click();
	settle();
	expect(chat->steps(0)->isVisibleTo(chat) && chat->steps(0)->findChildren<QLabel*>().size() == 2, "unfolded: what it read and searched");
	auto* const clipboard = QGuiApplication::clipboard();
	chat->copy_reply_button(0)->click();
	expect(clipboard->text().startsWith(QStringLiteral("It is the frame window:")), "Copy takes the reply as written");
	chat->copy_chat_button()->click();
	expect(clipboard->text().startsWith(QStringLiteral("## Side chat: Solution")) && clipboard->text().contains(QStringLiteral("**You:** What does this mean?")) && clipboard->text().contains(QStringLiteral("(worked for 7s)")), "Copy chat takes the whole chat: " + clipboard->text().toStdString());

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
	actions.clear();
	chat->show_button()->click();
	expect(chat->show_button()->text() == QStringLiteral("Show in code") && std::ranges::count(actions, qt::tour_editor_action::navigate_stop) >= 1, "Show in code goes to it again");
	editor.ask_about_code(std::nullopt);
	expect(chat->passage() && chat->passage()->Field == core::tour_passage_field::stop_code && chat->passage()->Path == "src/item.cpp" && chat->passage()->FirstLine == 4U, "its code is quoted for the next question");
	render(*chat, prefix, "side-chat-stop");
	// The tour's own chat, from the panel.
	editor.tour_agent()->chat_button()->click();
	settle();
	expect(chat->chat_count() == 3U && chat->current() == core::tour_thread{ } && chat->subject()->text() == QStringLiteral("The whole tour"), "the tour as a whole has a chat too");
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

	std::cout << "tour-side-chat:pass\n";
	return EXIT_SUCCESS;
}
