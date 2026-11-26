// RFC-BPR-0130 stages 3 to 5 (AC06). The one transcript changes only the
// turns a delta names: while a reply grows, every other turn keeps its
// widgets, in the side chats' look and in the Agent tab's.
#include "bpr/core/chat_transcript.hpp"
#include "bpr/qt/agent_run_status.hpp"
#include "bpr/qt/chat_transcript_view.hpp"
#include "bpr/qt/item_side_chat.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/tour_side_chat.hpp"

#include <QApplication>
#include <QClipboard>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string& aMessage)
	{
		if (!aCondition) {
			std::cerr << "chat-transcript-view-tests: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void settle() { QCoreApplication::processEvents(); }

	[[nodiscard]] auto names() -> qt::chat_transcript_view::names
	{
		return {
			.Scroll    = QStringLiteral("testScroll"),
			.Bubble    = QStringLiteral("testBubble"),
			.Question  = QStringLiteral("testQuestion"),
			.Steered   = QStringLiteral("testSteered"),
			.Reply     = QStringLiteral("testReply"),
			.Failure   = QStringLiteral("testFailure"),
			.Ending    = QStringLiteral("testEnding"),
			.CopyReply = QStringLiteral("testCopyReply"),
			.Resend    = QStringLiteral("testResend"),
			.Delete    = QStringLiteral("testDelete"),
		};
	}

	[[nodiscard]] auto turn(const std::string& aId, const std::string& aQuestion, const std::string& aReply, const bool aRunning) -> core::chat_turn
	{
		auto made     = core::chat_turn{ };
		made.Id       = aId;
		made.Question = aQuestion;
		made.Reply    = aReply;
		made.Running  = aRunning;
		if (!aRunning) {
			auto ending       = core::chat_ending{ };
			ending.Agent      = "Codex";
			ending.Seconds    = 4;
			made.Ending       = std::move(ending);
			made.Actions.Copy = true;
		}
		return made;
	}

	/// The widgets of one turn, to hold against the same turn later.
	struct turn_widgets
	{
		QWidget*                Turn   = nullptr;
		QWidget*                Bubble = nullptr;
		qt::markdown_body_view* Reply  = nullptr;
		QLabel*                 Ending = nullptr;

		auto operator==(const turn_widgets&) const -> bool = default;
	};

	[[nodiscard]] auto widgets_of(const qt::chat_transcript_view& aView, const std::size_t aTurn) -> turn_widgets
	{
		return { .Turn = aView.turn_widget(aTurn), .Bubble = aView.bubble(aTurn), .Reply = aView.reply(aTurn), .Ending = aView.ending(aTurn) };
	}

	void test_a_growing_reply_keeps_every_other_turn()
	{
		auto  window = QWidget{ };
		auto* view   = new qt::chat_transcript_view{ names(), &window };
		auto  made   = std::vector<std::string>{ };
		auto  acted  = std::vector<std::pair<std::string, qt::chat_turn_gesture>>{ };
		view->set_parts({
			.DressReader    = { },
			.MakeAttachment = [&made](const core::chat_turn& aTurn, const core::chat_attachment& aAttachment, QWidget* aHolder) -> QWidget* {
				made.push_back(aTurn.Id + "/" + aAttachment.Key);
				auto* const card = new QLabel{ QString::fromStdString(std::get<core::chat_follow_up_proposal>(aAttachment.Payload).Proposal.Text), aHolder };
				card->setObjectName(QStringLiteral("testCard"));
				return card;
			},
			.Act = [&acted](const std::string& aTurn, const qt::chat_turn_gesture aGesture) { acted.emplace_back(aTurn, aGesture); },
		});
		window.resize(360, 600);
		view->resize(360, 600);
		window.show();

		auto transcript = core::chat_transcript{ };
		transcript.Turns.push_back(turn("a", "Why is this blocking?", "It asserts on **minimise**.", false));
		transcript.Turns.push_back(turn("b", "And the width?", "", true));
		view->present_live(qt::agent_run_status{ .Agent = QStringLiteral("Codex") }, QElapsedTimer{ });
		view->present(transcript);
		settle();
		expect(view->turn_count() == 2U && view->last_delta().size() == 2U, "both turns appended");
		expect(view->question(0U) != nullptr && view->question(0U)->text() == QStringLiteral("Why is this blocking?"), "the question is its bubble's words");
		expect(view->reply(0U) != nullptr && view->reply(0U)->original_body() == "It asserts on **minimise**.", "the reply is Markdown");
		expect(view->ending(0U) != nullptr && view->ending(0U)->text() == QStringLiteral("Codex answered in 0:04."), "the one ending line: " + (view->ending(0U) != nullptr ? view->ending(0U)->text().toStdString() : std::string{ }));
		expect(view->ending(1U) != nullptr && view->ending(1U)->live() && view->ending(1U)->text().contains(QStringLiteral("Codex")), "the running turn's line is live and names the agent");
		expect(view->reply(1U) == nullptr && view->copy_reply(1U) == nullptr, "nothing to copy while nothing has come");
		const auto first = widgets_of(*view, 0U);

		// The reply grows: one change, naming its turn, and the same widgets.
		transcript.Turns[1].Reply = "Clamp";
		view->present(transcript);
		const auto growing = widgets_of(*view, 1U);
		expect(growing.Reply != nullptr && growing.Reply->original_body() == "Clamp", "the running turn's reply shows as it comes");
		transcript.Turns[1].Reply = "Clamp both sides.";
		view->present(transcript);
		settle();
		expect(view->last_delta() == core::chat_delta{ { .Kind = core::chat_change_kind::reply_grew, .Turn = "b", .Index = 1U } }, "one change, naming the turn");
		expect(widgets_of(*view, 0U) == first, "the other turn's widgets are the same objects");
		expect(widgets_of(*view, 1U) == growing && view->reply(1U)->original_body() == "Clamp both sides.", "and the growing turn keeps its own reader");

		// It settles: its line ends, the other turn untouched.
		transcript.Turns[1] = turn("b", "And the width?", "Clamp both sides.", false);
		view->present(transcript);
		settle();
		expect(widgets_of(*view, 0U) == first && view->turn_widget(1U) == growing.Turn && view->reply(1U) == growing.Reply, "settling keeps every turn's widgets");
		expect(!view->ending(1U)->live() && view->ending(1U)->text() == QStringLiteral("Codex answered in 0:04."), "its ending line says who answered");

		// A third turn with an attachment; the first two keep theirs.
		auto proposal     = core::chat_follow_up_proposal{ };
		proposal.Proposal = core::follow_up_proposal{ .Id = 9U, .Apply = core::follow_up_apply::update_suggestion, .Text = "issue: clamp.", .Base = "issue." };
		auto third        = core::chat_turn{ };
		third.Id          = "c";
		third.Attachments.push_back({ .Key = "proposal", .Payload = proposal });
		transcript.Turns.push_back(third);
		view->present(transcript);
		settle();
		expect(widgets_of(*view, 0U) == first && view->turn_widget(1U) == growing.Turn, "a turn appended keeps the others");
		expect(made == std::vector<std::string>{ "c/proposal" } && view->attachment(2U, "proposal") != nullptr && view->bubble(2U) == nullptr && view->ending(2U) == nullptr, "an attachment made once; a turn without a question has no bubble");
		auto* const card = view->attachment(2U, "proposal");
		view->present(transcript);
		expect(made.size() == 1U && view->attachment(2U, "proposal") == card, "an equal attachment is kept");
		std::get<core::chat_follow_up_proposal>(transcript.Turns[2].Attachments[0].Payload).Usable = false;
		view->present(transcript);
		expect(made.size() == 2U && widgets_of(*view, 0U) == first, "a changed attachment is made again, in its turn only");

		// Copy copies the reply; Resend and Delete name their turn.
		view->copy_reply(0U)->click();
		expect(QGuiApplication::clipboard()->text() == QStringLiteral("It asserts on **minimise**."), "Copy copies the reply as written");
		auto failed            = turn("b", "And the width?", "", false);
		failed.Ending->Outcome = core::agent_turn_outcome::exited_with_error;
		failed.Failure         = "The agent exited (status 1).";
		failed.Actions         = { .Error = true, .Copy = true, .Recovery = core::agent_turn_recovery::resend, .Delete = true };
		transcript.Turns[1]    = failed;
		view->present(transcript);
		settle();
		expect(view->reply(1U) == nullptr && view->ending(1U)->text() == QStringLiteral("Stopped after 0:04.") && view->resend(1U) != nullptr && view->remove(1U) != nullptr, "a failed turn: why, Resend and Delete");
		view->copy_reply(1U)->click();
		expect(QGuiApplication::clipboard()->text() == QStringLiteral("The agent exited (status 1)."), "Copy copies what went wrong");
		view->resend(1U)->click();
		view->remove(1U)->click();
		expect(acted.size() == 2U && acted[0] == std::pair{ std::string{ "b" }, qt::chat_turn_gesture::resend } && acted[1] == std::pair{ std::string{ "b" }, qt::chat_turn_gesture::remove }, "Resend and Delete name the turn");
		expect(widgets_of(*view, 0U) == first, "and the first turn is still the same objects");

		// A turn removed: the next stands in its place, with its own widgets.
		const auto second = view->turn_widget(1U);
		transcript.Turns.erase(transcript.Turns.begin());
		view->present(transcript);
		expect(view->turn_count() == 2U && view->turn_widget(0U) == second, "a removed turn goes; the rest keep their widgets");
	}

	void test_the_item_chat_keeps_its_earlier_rounds()
	{
		auto subject  = core::follow_up_subject{ };
		subject.Kind  = core::follow_up_subject_kind::suggested_comment;
		subject.Key   = "finding:7:0";
		subject.Path  = "src/engine/renderer.cpp";
		subject.First = 12U;
		subject.Last  = 13U;
		subject.Title = "A zero height still reaches the swap chain.";
		subject.Text  = "issue (blocking): clamp both.";
		auto state    = core::open_follow_up(subject);
		auto page     = QWidget{ };
		auto chat     = new qt::item_side_chat{ &page };
		page.resize(420, 640);
		page.show();
		chat->set_column_open(true);
		static_cast<void>(core::ask_follow_up(state, "Why?"));
		chat->present(state, QStringLiteral("Codex"), { });
		static_cast<void>(core::settle_follow_up(state, state.Pending, std::string{ "Because." }));
		chat->present(state, QStringLiteral("Codex"), { });
		settle();
		auto* const view  = chat->transcript();
		const auto  first = widgets_of(*view, 0U);
		expect(first.Reply != nullptr && first.Ending != nullptr && first.Ending->text().startsWith(QStringLiteral("Codex answered in ")), "the item chat's round ends with the one ending line: " + (first.Ending != nullptr ? first.Ending->text().toStdString() : std::string{ }));
		static_cast<void>(core::ask_follow_up(state, "And then?"));
		chat->present(state, QStringLiteral("Codex"), { });
		expect(widgets_of(*view, 0U) == first && view->last_delta().size() == 1U, "a new question appends one turn and keeps the first");
		static_cast<void>(core::settle_follow_up(state, state.Pending, std::string{ "Then it clamps." }));
		chat->present(state, QStringLiteral("Codex"), { });
		auto expected          = core::chat_delta(1);
		expected.front().Kind  = core::chat_change_kind::settled;
		expected.front().Turn  = "finding:7:0/round/1";
		expected.front().Index = 1U;
		expect(widgets_of(*view, 0U) == first && view->last_delta() == expected, "its answer settles that turn alone");
	}
}

namespace {
	[[nodiscard]] auto ask_run(const std::uint64_t aId, const core::tour_thread& aThread, const core::tour_agent_run_state aState, const std::string& aQuestion, const std::string& aReply) -> core::tour_agent_run
	{
		auto run     = core::tour_agent_run{ };
		run.Id       = core::tour_agent_run_id{ aId };
		run.Request  = core::tour_agent_request::ask;
		run.Thread   = aThread;
		run.State    = aState;
		run.Question = aQuestion;
		run.Reply    = aReply;
		return run;
	}

	/// RFC-BPR-0130 stage 4 (AC06 for the tour side chat): a turn that
	/// settles, and a proposal decided, change their own turn alone.
	void test_the_tour_chat_keeps_its_other_turns()
	{
		const auto stop    = core::tour_thread{ .Kind = core::tour_thread_kind::stop, .Stop = core::tour_stop_id{ 4U } };
		auto       session = core::tour_agent_session{ };
		session.Runs.push_back(ask_run(1U, stop, core::tour_agent_run_state::answered, "What does this mean?", "It clamps."));
		session.Runs.push_back(ask_run(2U, stop, core::tour_agent_run_state::running, "And the width?", ""));
		auto  page = QWidget{ };
		auto* chat = new qt::tour_side_chat{ &page };
		page.resize(420, 640);
		chat->resize(420, 640);
		page.show();
		chat->open(stop);
		chat->present(session, std::nullopt, nullptr);
		settle();
		auto* const view  = chat->transcript();
		const auto  first = widgets_of(*view, 0U);
		expect(chat->turn_count() == 2U && first.Reply != nullptr && view->ending(1U) != nullptr && view->ending(1U)->live(), "the tour chat draws its runs in the one transcript");

		session.Runs[1].State   = core::tour_agent_run_state::answered;
		session.Runs[1].Reply   = "Clamp both sides.";
		session.Runs[1].Seconds = 3U;
		auto proposal           = core::tour_proposal{ };
		proposal.Run            = core::tour_agent_run_id{ 2U };
		auto item               = core::tour_proposal_item{ };
		item.Part               = core::tour_proposal_part::stop_text;
		item.Text               = "Clamps the height and the width.";
		item.Run                = core::tour_agent_run_id{ 2U };
		proposal.Items.push_back(item);
		session.Runs[1].Proposed.StopTexts = 1U;
		chat->present(session, proposal, nullptr);
		settle();
		expect(widgets_of(*view, 0U) == first, "a turn that settles with a proposal keeps the other turn's widgets");
		expect(view->last_delta() == core::chat_delta{ { .Kind = core::chat_change_kind::settled, .Turn = "tour-run:2", .Index = 1U } }, "one change, naming the settled turn");
		expect(chat->accept_button(0U) != nullptr && view->ending(1U)->text() == QStringLiteral("The tour agent answered in 0:03."), "its proposal waits with Accept, under the one ending line");
		auto* const settledReply = view->reply(1U);

		proposal.Items[0].Decision = core::tour_proposal_decision::accepted;
		chat->present(session, proposal, nullptr);
		settle();
		expect(widgets_of(*view, 0U) == first && view->reply(1U) == settledReply && chat->accept_button(0U) == nullptr, "deciding the proposal changes its card alone");
	}

	/// RFC-BPR-0130 stage 5. The Agent tab's card look: the same rule, its
	/// own parts made once with each turn, and what an ended turn offers drawn
	/// from what the projector decided.
	void test_the_card_look_keeps_every_other_turn()
	{
		auto window   = QWidget{ };
		auto named    = names();
		named.Turns   = QStringLiteral("testTurns");
		named.Card    = QStringLiteral("testCard");
		auto* view    = new qt::chat_transcript_view{ named, &window, qt::chat_transcript_look::card };
		auto  dressed = std::vector<std::string>{ };
		auto  footed  = std::vector<std::string>{ };
		auto  acted   = std::vector<std::pair<std::string, qt::chat_turn_gesture>>{ };
		view->set_parts({
			.DressReader    = { },
			.MakeAttachment = nullptr,
			.Act            = [&acted](const std::string& aTurn, const qt::chat_turn_gesture aGesture) { acted.emplace_back(aTurn, aGesture); },
			.DressBubble    = { },
			.ReplyBody      = { },
			.Stop           = { },
			.ReplyIdentity  = { },
			.DressCard =
				[&dressed](const core::chat_turn& aTurn, const qt::chat_transcript_view::card_slots& aSlots) {
					dressed.push_back(aTurn.Id);
					aSlots.AfterReply->addWidget(new QLabel{ QStringLiteral("Under the reply"), aSlots.Card });
				},
			.DressFoot   = [&footed](const core::chat_turn& aTurn, QWidget& /*aCard*/, QHBoxLayout& /*aActions*/) { footed.push_back(aTurn.Id); },
			.DressEnding = { },
		});
		window.resize(360, 600);
		view->resize(360, 600);
		window.show();
		expect(view->turns_column() != nullptr && view->turns_column()->isHidden(), "the turns' column waits for a turn");

		auto entries        = std::vector<core::agent_thread_entry>(2U);
		entries[0].Serial   = 1U;
		entries[0].Question = "Review this pull request";
		entries[0].Answer   = "Two findings.";
		entries[0].Done     = true;
		entries[0].Line     = "Claude answered in 0:42 · 2 findings";
		entries[1].Serial   = 2U;
		entries[1].Question = "Why?";
		view->present(core::project_agent_thread(entries, { .Working = true }));
		view->present_live(qt::agent_run_status{ .Agent = QStringLiteral("Claude Code") }, QElapsedTimer{ });
		settle();
		expect(!view->turns_column()->isHidden() && view->turn_count() == 2U, "both turns in the turns' column");
		expect(view->ending(0U) != nullptr && view->ending(0U)->text() == QStringLiteral("Claude answered in 0:42 · 2 findings"), "the line as it was kept");
		expect(view->copy_reply(0U) != nullptr && view->copy_reply(0U)->isVisibleTo(&window), "an answer offers Copy");
		expect(view->ending(1U) != nullptr && view->ending(1U)->live(), "the running turn's line is live");
		const auto first = widgets_of(*view, 0U);

		// The answer grows: one change, and every other turn keeps its widgets.
		entries[1].Answer = "Bec";
		view->present(core::project_agent_thread(entries, { .Working = true }));
		const auto growing = widgets_of(*view, 1U);
		expect(growing.Reply != nullptr && growing.Reply->isVisibleTo(&window) && growing.Reply->original_body() == "Bec", "the reader shows the answer as it comes");
		entries[1].Answer = "Because it clamps.";
		view->present(core::project_agent_thread(entries, { .Working = true }));
		settle();
		expect(view->last_delta() == core::chat_delta{ { .Kind = core::chat_change_kind::reply_grew, .Turn = "agent-turn-2", .Index = 1U } }, "one change, naming the turn");
		expect(widgets_of(*view, 0U) == first && widgets_of(*view, 1U) == growing, "the same objects, the growing turn's reader its own");
		expect(dressed == std::vector<std::string>{ "agent-turn-1", "agent-turn-2" } && footed == dressed, "the chat's own parts are made once a turn");

		// It ends in an error: the card says so, with Resend and Delete.
		entries[1].Done    = true;
		entries[1].Outcome = core::agent_turn_outcome::exited_with_error;
		entries[1].Line    = "Stopped after 0:03.";
		view->present(core::project_agent_thread(entries, { }));
		settle();
		expect(widgets_of(*view, 0U) == first && view->reply(1U) == growing.Reply, "settling changes its own turn alone");
		expect(view->card(1U) != nullptr && view->card(1U)->accessibleDescription() == QStringLiteral("Error"), "an error's card says it is one");
		expect(view->recovery(1U) != nullptr && view->recovery(1U)->isVisibleTo(&window) && view->recovery(1U)->text() == QStringLiteral("Resend"), "the latest error offers Resend");
		expect(view->remove(1U) != nullptr && view->remove(1U)->isVisibleTo(&window), "and Delete");
		view->recovery(1U)->click();
		view->remove(1U)->click();
		expect(acted == std::vector<std::pair<std::string, qt::chat_turn_gesture>>{ { "agent-turn-2", qt::chat_turn_gesture::resend }, { "agent-turn-2", qt::chat_turn_gesture::remove } }, "each press names its turn");

		// Deleted: one removal; the kept turn keeps its widgets.
		entries.pop_back();
		view->present(core::project_agent_thread(entries, { }));
		settle();
		expect(view->turn_count() == 1U && widgets_of(*view, 0U) == first, "the deleted turn leaves alone");
	}

	/// The tab stops after \p aFrom, as Qt's focus order has them, up to \p aCount.
	[[nodiscard]] auto tab_stops_after(QWidget& aFrom, const std::size_t aCount) -> std::vector<QWidget*>
	{
		auto stops = std::vector<QWidget*>{ };
		for (auto* next = aFrom.nextInFocusChain(); next != nullptr && next != &aFrom && stops.size() < aCount; next = next->nextInFocusChain()) {
			if ((next->focusPolicy() & Qt::TabFocus) != 0 && next->isVisible() && next->isEnabled()) {
				stops.push_back(next);
			}
		}
		return stops;
	}

	[[nodiscard]] auto top_in(const QWidget& aCard, const QWidget& aWidget) -> int { return aWidget.mapTo(&aCard, QPoint{ }).y(); }

	/// RFC-BPR-0130 stage 5. The Agent tab's verdict, report, Draft all and
	/// cards are attachments: the verdict over the reply after the chat's own
	/// parts there, the rest under it before the ending line; an attachment
	/// made after its turn is reached by Tab where it stands; an equal one is
	/// kept and one that changed is made again alone.
	void test_the_card_look_hangs_attachments()
	{
		auto window = QWidget{ };
		auto named  = names();
		named.Turns = QStringLiteral("testTurns");
		named.Card  = QStringLiteral("testCard");
		auto* view  = new qt::chat_transcript_view{ named, &window, qt::chat_transcript_look::card };
		auto  made  = std::vector<std::string>{ };
		view->set_parts({
			.DressReader = { },
			.MakeAttachment =
				[&made](const core::chat_turn& aTurn, const core::chat_attachment& aAttachment, QWidget* aHolder) -> QWidget* {
				made.push_back(aTurn.Id + ":" + aAttachment.Key);
				if (const auto* const verdict = std::get_if<core::chat_agent_verdict>(&aAttachment.Payload); verdict != nullptr) {
					auto* const label = new QLabel{ QString::fromStdString(verdict->Word), aHolder };
					label->setObjectName(QStringLiteral("testVerdict"));
					return label;
				}
				if (const auto* const report = std::get_if<core::chat_agent_report>(&aAttachment.Payload); report != nullptr) {
					auto* const row = new QPushButton{ QString::fromStdString(report->Title), aHolder };
					row->setObjectName(QStringLiteral("testReport"));
					row->setFocusPolicy(Qt::StrongFocus);
					return row;
				}
				if (const auto* const findings = std::get_if<core::chat_agent_findings>(&aAttachment.Payload); findings != nullptr) {
					auto* const holder = new QWidget{ aHolder };
					holder->setObjectName(QStringLiteral("testFindings"));
					auto* const column = new QVBoxLayout{ holder };
					column->setContentsMargins(0, 0, 0, 0);
					for (const auto card : findings->Cards) {
						auto* const button = new QPushButton{ QStringLiteral("Card %1").arg(card), holder };
						button->setFocusPolicy(Qt::StrongFocus);
						column->addWidget(button);
					}
					return holder;
				}
				return nullptr;
			},
			.Act           = { },
			.DressBubble   = { },
			.ReplyBody     = { },
			.Stop          = { },
			.ReplyIdentity = { },
			.DressCard =
				[](const core::chat_turn& /*aTurn*/, const qt::chat_transcript_view::card_slots& aSlots) {
					auto* const over = new QLabel{ QStringLiteral("Over the reply"), aSlots.Card };
					over->setObjectName(QStringLiteral("testOver"));
					aSlots.BeforeReply->addWidget(over);
				},
			.DressFoot   = { },
			.DressEnding = { },
		});
		window.resize(360, 900);
		view->resize(360, 900);
		window.show();

		// A turn running holds nothing; it ends a report with two cards.
		auto entries        = std::vector<core::agent_thread_entry>(1U);
		entries[0].Serial   = 1U;
		entries[0].Question = "Review this pull request";
		entries[0].Answer   = "Two findings.";
		view->present(core::project_agent_thread(entries, { .Working = true }));
		settle();
		expect(made.empty() && view->attachment(0U, "report") == nullptr, "a running turn holds nothing");
		entries[0].Done    = true;
		entries[0].Line    = "Claude answered in 0:42";
		entries[0].Report  = core::chat_agent_report{ .Title = "Review", .Sections = 2U };
		entries[0].Verdict = core::chat_agent_verdict{ .Word = "Needs changes", .Tone = core::report_tone::open };
		entries[0].Cards   = { 11U, 12U };
		view->present(core::project_agent_thread(entries, { }));
		settle();
		expect(made == std::vector<std::string>{ "agent-turn-1:verdict", "agent-turn-1:report", "agent-turn-1:draft-all", "agent-turn-1:findings" }, "the factory makes each attachment once: " + std::to_string(made.size()));
		auto* const card     = view->card(0U);
		auto* const verdict  = view->attachment(0U, "verdict");
		auto* const report   = view->attachment(0U, "report");
		auto* const findings = view->attachment(0U, "findings");
		auto* const over     = card->findChild<QLabel*>(QStringLiteral("testOver"));
		expect(verdict != nullptr && report != nullptr && findings != nullptr && over != nullptr && verdict->isVisibleTo(&window) && report->isVisibleTo(&window), "the verdict, the report and the cards are drawn");
		const auto reader = top_in(*card, *view->reply(0U));
		expect(top_in(*card, *over) < top_in(*card, *verdict) && top_in(*card, *verdict) < reader, "the verdict stands over the reply, after the chat's own part there");
		expect(reader < top_in(*card, *report) && top_in(*card, *report) < top_in(*card, *findings) && top_in(*card, *findings) < top_in(*card, *view->ending(0U)), "the report and the cards stand under the reply, before the ending line");
		// Made after the turn, they are still reached where they stand: the
		// report, its cards, then Copy.
		const auto stops = tab_stops_after(*report, 3U);
		const auto cards = findings->findChildren<QPushButton*>();
		auto       said  = std::string{ };
		for (auto* const stop : stops) {
			said += " " + (stop->objectName().isEmpty() ? std::string{ stop->metaObject()->className() } : stop->objectName().toStdString());
		}
		expect(cards.size() == 2 && stops.size() == 3U && stops[0] == cards[0] && stops[1] == cards[1] && stops[2] == view->copy_reply(0U), "Tab goes from the report through its cards to Copy:" + said);

		// A card more: the cards are made again, the rest kept.
		made.clear();
		entries[0].Cards.push_back(13U);
		view->present(core::project_agent_thread(entries, { }));
		settle();
		expect(made == std::vector<std::string>{ "agent-turn-1:findings" } && view->attachment(0U, "report") == report && view->attachment(0U, "verdict") == verdict, "a changed attachment is made again alone");
		expect(view->attachment(0U, "findings") != findings && view->attachment(0U, "findings")->findChildren<QPushButton*>().size() == 3, "the cards as they now stand");

		// The verdict is gone: its widget goes, the others stay.
		entries[0].Verdict.reset();
		view->present(core::project_agent_thread(entries, { }));
		settle();
		expect(view->attachment(0U, "verdict") == nullptr && view->attachment(0U, "report") == report, "an attachment the turn no longer holds goes alone");

		// A turn that holds its attachments when it is first drawn makes them
		// with its own parts, in reading order.
		auto second   = entries[0];
		second.Serial = 2U;
		second.Cards  = { };
		entries.push_back(second);
		view->present(core::project_agent_thread(entries, { }));
		settle();
		auto* const early = view->attachment(1U, "report");
		expect(early != nullptr && early->isVisibleTo(&window) && tab_stops_after(*early, 1U) == std::vector<QWidget*>{ view->copy_reply(1U) }, "a report made with its turn is reached before Copy");
	}
}

auto main(int argc, char** argv) -> int
{
	qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsSet("QT_QPA_PLATFORM") ? qgetenv("QT_QPA_PLATFORM") : QByteArray{ "offscreen" });
	QApplication application{ argc, argv };
	test_a_growing_reply_keeps_every_other_turn();
	test_the_item_chat_keeps_its_earlier_rounds();
	test_the_tour_chat_keeps_its_other_turns();
	test_the_card_look_keeps_every_other_turn();
	test_the_card_look_hangs_attachments();
	std::cout << "chat-transcript-view-tests:pass\n";
	return EXIT_SUCCESS;
}
