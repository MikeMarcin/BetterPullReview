// RFC-BPR-0130 stage 3 (AC06). The one transcript changes only the turns a
// delta names: while a reply grows, every other turn keeps its widgets.
#include "bpr/core/chat_transcript.hpp"
#include "bpr/qt/agent_run_status.hpp"
#include "bpr/qt/chat_transcript_view.hpp"
#include "bpr/qt/item_side_chat.hpp"
#include "bpr/qt/markdown_body_view.hpp"

#include <QApplication>
#include <QClipboard>
#include <QGuiApplication>
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
		expect(widgets_of(*view, 0U) == first && view->last_delta() == core::chat_delta{ { .Kind = core::chat_change_kind::settled, .Turn = "finding:7:0/round/1", .Index = 1U } }, "its answer settles that turn alone");
	}
}

auto main(int argc, char** argv) -> int
{
	qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsSet("QT_QPA_PLATFORM") ? qgetenv("QT_QPA_PLATFORM") : QByteArray{ "offscreen" });
	QApplication application{ argc, argv };
	test_a_growing_reply_keeps_every_other_turn();
	test_the_item_chat_keeps_its_earlier_rounds();
	std::cout << "chat-transcript-view-tests:pass\n";
	return EXIT_SUCCESS;
}
