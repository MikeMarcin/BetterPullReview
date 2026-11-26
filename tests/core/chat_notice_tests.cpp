// The one notice of every chat with an agent (RFC-BPR-0130 stage 6, AC08):
// a send refused, a stopped run, a moved head and a carried-over
// conversation, each planned once, with its words and its choices.
#include "bpr/core/chat_notice.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {
	using namespace bpr::core;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void test_send_refused()
	{
		const auto plain = plan_send_refused_notice("Type a question, then send.");
		expect(plain.Kind == chat_notice_kind::send_refused && plain.Form == chat_notice_form::strip && plain.Tone == chat_notice_tone::danger && plain.Dismissable, "a refusal is a strip with a close, in the danger tone");
		expect(plain.Words == "Not sent. Type a question, then send." && plain.Title.empty() && plain.Choices.empty(), "it says Not sent and why, and offers nothing");
		expect(plan_send_refused_notice("Fixture agent didn't start.", "no exact accepted review context").Words == "Not sent. Fixture agent didn't start. (no exact accepted review context)",
			   "the host's own detail follows in brackets");
		expect(plan_send_refused_notice("x") == plan_send_refused_notice("x"), "equal facts, equal notices");
	}

	void test_moved_head_before_a_question()
	{
		const auto card = plan_moved_head_notice({ .Read = "b20cb4434282aa", .KeepsFindings = true });
		expect(card.Kind == chat_notice_kind::head_moved && card.Form == chat_notice_form::card && card.Tone == chat_notice_tone::callout && !card.Dismissable, "the pinned-revision card asks and cannot be closed");
		expect(card.Title == "This session read b20cb44", "it names the revision read by seven characters");
		expect(card.Words == "The pull request has moved on. Its findings stay at that head.", "the Agent tab's findings stay at that head");
		expect(card.Choices.size() == 2U && card.Choices[0].Action == chat_notice_action::continue_at && card.Choices[1].Action == chat_notice_action::new_session, "Continue at the revision read, then New session");
		expect(card.Choices[0].Words == "Continue at b20cb44" && card.Choices[0].ShortWords == "Continue" && card.Choices[0].Tip == "Keep this conversation and ask about b20cb44, the revision it read",
			   "Continue names the revision, and says Continue where the card is narrow");
		expect(card.Choices[1].Words == "New session" && card.Choices[1].Enabled
				   && card.Choices[1].Tip == "Keep this conversation in History and start an empty one at the current head. Nothing is sent until you ask.",
			   "New session is History's, and sends nothing");
		expect(!card.Choices[0].Starts && !card.Choices[1].Starts, "before a question neither choice starts a run");

		// The tour's chat keeps no findings: the same card, without them.
		const auto tour = plan_moved_head_notice({ .Read = "b20cb4434282aa" });
		expect(tour.Title == card.Title && tour.Choices == card.Choices && tour.Words == "The pull request has moved on.", "every chat says the same card");

		const auto refused = plan_moved_head_notice({ .Read = "b20cb44", .NewSessionRefused = "Wait for the sessions to load." });
		expect(!refused.Choices[1].Enabled && refused.Choices[1].Tip == "Wait for the sessions to load.", "New session greyed says why");
		const auto unknown = plan_moved_head_notice({ });
		expect(unknown.Title == "This session read an earlier head" && unknown.Choices[0].Words == "Continue at its head", "a revision not known yet is an earlier head");
	}

	void test_moved_head_after_a_refused_question()
	{
		const auto card = plan_moved_head_notice({ .Read = "b20cb44", .Asked = true });
		expect(card.Title == "This session read b20cb44" && card.Words == "Your message is kept and is sent when you choose.", "the same card, keeping the message");
		expect(card.Choices.size() == 2U && card.Choices[0].Starts && card.Choices[1].Starts, "each choice sends the kept message, so each starts a run");
		expect(card.Tone == chat_notice_tone::danger && card.Dismissable, "it reports a turn that did not run, and closes like a stopped run's card");
		expect(card.Choices[1].Tip == "Keep this conversation in History, start an empty one at the current head and send your message there", "New session says the message goes there");
		const auto said = plan_moved_head_notice({ .Read = "b20cb44", .Asked = true, .Reason = "The session reads an earlier head." });
		expect(said.Words == "The session reads an earlier head. Your message is kept and is sent when you choose.", "the reason is said when no turn says it");
		const auto only = plan_moved_head_notice({ .Read = "b20cb44", .Asked = true, .ContinueOffered = false });
		expect(only.Choices.size() == 1U && only.Choices[0].Action == chat_notice_action::new_session, "a conversation about another review offers New session alone");
	}

	void test_wait_words()
	{
		expect(chat_moved_head_wait_words("b20cb4434282") == "Choose Continue at b20cb44 or New session first.", "a prompt waits on the card's two choices, by their words");
		expect(chat_moved_head_wait_words("") == "Choose Continue or New session first.", "or on Continue where the revision is not known");
		expect(chat_moved_head_wait_words("b20cb44", false) == "Choose New session first.", "or on New session alone");
	}

	void test_stopped()
	{
		const auto card = plan_stopped_notice({ .Agent = "Claude Code", .Reason = "The agent's copy of this pull request could not be made." });
		expect(card.Kind == chat_notice_kind::stopped && card.Form == chat_notice_form::card && card.Tone == chat_notice_tone::danger && card.Dismissable, "a stopped run is a danger card with a close");
		expect(card.Title == "Claude Code stopped before answering" && card.Words == "The agent's copy of this pull request could not be made. Your message is kept.", "who stopped, why, and that the message is kept");
		expect(card.Choices.size() == 2U && card.Choices[0].Action == chat_notice_action::resend && card.Choices[0].Starts && card.Choices[1].Action == chat_notice_action::toggle_log && card.Choices[1].Aside,
			   "Resend, and Show the log beside it");
		expect(card.Choices[1].Words == "Show the log" && plan_stopped_notice({ .Agent = "Codex", .LogShown = true }).Choices[1].Words == "Hide the log", "the log's action says which way it goes");
		const auto forGood = plan_stopped_notice({ .Agent = "Codex", .ResendOffered = false });
		expect(forGood.Choices.size() == 1U && forGood.Choices[0].Action == chat_notice_action::toggle_log && forGood.Words == "Your message is kept.", "a stop for good offers no Resend; its turn says why");
		expect(plan_stopped_notice({ }).Title == "The agent stopped before answering", "an agent not known is the agent");
	}

	void test_session()
	{
		const auto carried = plan_session_notice(chat_continuation::carried_over, "Codex", "Two turns were left out.");
		expect(carried.Kind == chat_notice_kind::carried_over && carried.Form == chat_notice_form::line && carried.Tone == chat_notice_tone::quiet, "a carried-over conversation is a quiet line");
		expect(carried.Words == chat_session_notice(chat_continuation::carried_over, "Codex") && carried.Tip == "Two turns were left out.", "in RFC-BPR-0130's accepted words, naming the chosen agent");
		const auto stuck = plan_session_notice(chat_continuation::cannot_continue, "Claude Code");
		expect(stuck.Kind == chat_notice_kind::cannot_continue && stuck.Words.find("Claude Code") == 0U, "the agent that cannot pick it up is named");
		const auto native = plan_session_notice(chat_continuation::native, "Codex", "unused");
		expect(!native.says() && native.Tip.empty(), "a session that goes on in the ordinary way says nothing");
		for (const auto agent : { std::string_view{ "Codex" }, std::string_view{ "Claude Code" } }) {
			for (const auto way : { chat_continuation::carried_over, chat_continuation::cannot_continue }) {
				const auto words = plan_session_notice(way, agent).Words;
				expect(words.find("Native") == std::string::npos && words.find("Transcript replay") == std::string::npos, "no internal word");
				expect(words.find(agent) != std::string::npos, "the chosen agent is the one named");
			}
		}
	}
} // namespace

auto main() -> int
{
	test_send_refused();
	test_moved_head_before_a_question();
	test_moved_head_after_a_refused_question();
	test_wait_words();
	test_stopped();
	test_session();
	std::cout << "chat-notice:pass\n";
	return EXIT_SUCCESS;
}
