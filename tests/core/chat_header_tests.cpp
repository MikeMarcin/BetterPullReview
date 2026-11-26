// The one header row of a chat with an agent: what each kind of chat offers,
// what its row says, and the words a chat says of its session.
#include "bpr/core/chat_header.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;
	using action = chat_header_action;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto header_of(const chat_kind aKind, const bool aCanShow = true) -> chat_header
	{
		return plan_chat_header({ .Kind = aKind, .Subject = "What it is about", .Place = "src/queue.cpp, lines 4 to 9", .CanShow = aCanShow });
	}

	// RFC-BPR-0130's header table, row by row.
	void test_offered()
	{
		const auto tab = header_of(chat_kind::agent_tab);
		expect(tab.Offered == std::vector{ action::history, action::new_session, action::delete_all_sessions, action::copy_conversation, action::show_every_run },
			   "the Agent tab: History, and in it New session, Delete all sessions, Copy conversation and Show every run");
		expect(tab.Tool == chat_header_tool::history && !tab.Close && tab.Show == chat_show::none, "its row is the title and History: the drawer closes it, and a pull request has no one place to show");

		const auto part = header_of(chat_kind::tour_part);
		expect(part.Offered == std::vector{ action::show, action::history, action::new_session, action::delete_all_sessions, action::other_tour_chats, action::copy_conversation, action::close },
			   "a tour part's chat: Show in code, History with the other parts' chats and Copy conversation, Close");
		expect(part.Tool == chat_header_tool::history && part.Close && part.Show == chat_show::in_code, "its row: Show in code, History, Close");

		const auto whole = header_of(chat_kind::whole_tour);
		expect(!whole.offers(action::show) && whole.Show == chat_show::none && whole.Tool == chat_header_tool::history && whole.Close, "the whole tour has no one place to show");
		expect(whole.offers(action::other_tour_chats) && whole.offers(action::new_session), "and the same History");

		// RFC-BPR-0130 stage 7 (owner, 2026-11-25: "sure seems like they
		// should right"): an item's chat keeps sessions.
		const auto item = header_of(chat_kind::review_item);
		expect(item.Offered == std::vector{ action::show, action::history, action::new_session, action::delete_all_sessions, action::copy_conversation, action::close },
			   "an item's chat: Show, History with New session, Delete all sessions and Copy conversation, Close");
		expect(item.Tool == chat_header_tool::history && item.Show == chat_show::place && item.Close, "its row: Show, History, Close");
		expect(!item.offers(action::other_tour_chats) && !item.offers(action::show_every_run), "it reads no other chats and no run list");

		const auto local = header_of(chat_kind::local_draft_discussion);
		expect(local.Tool == chat_header_tool::copy_conversation && local.Show == chat_show::in_code && local.Close && !local.offers(action::history),
			   "a local draft's discussion keeps no sessions either: Show in code, Copy conversation, Close");

		for (const auto kind : { chat_kind::agent_tab, chat_kind::tour_part, chat_kind::whole_tour, chat_kind::local_draft_discussion, chat_kind::review_item }) {
			const auto header = header_of(kind);
			expect(std::ranges::find(header.Offered, action::new_session) == header.Offered.end() || header.Tool == chat_header_tool::history, "New session is never a tool of the row: it is offered only where History is");
			expect(header.offers(action::copy_conversation), "every chat can copy its conversation");
			expect(chat_keeps_sessions(kind) == header.offers(action::history), "History exactly where the chat keeps sessions");
			expect(!header_of(kind, false).offers(action::show) && header_of(kind, false).Show == chat_show::none, "nothing to show where the subject has no place");
		}
	}

	void test_row()
	{
		auto facts = chat_header_facts{ .Kind = chat_kind::agent_tab, .Subject = "#2368 Queued prompt rows leave the list", .RevisionRead = "3f9c2ab11111aaaa" };
		auto row   = plan_chat_header(facts);
		expect(row.Title == facts.Subject && row.TitleNote.empty() && row.Place.empty(), "the Agent tab's row says the pull request's title");
		expect(row.RevisionRead == "3f9c2ab" && chat_revision_words("3f9c2ab11111") == "3f9c2ab" && chat_revision_words("3f9") == "3f9", "a revision is seven characters in every chat");

		facts.SessionName = "The queue's races";
		row               = plan_chat_header(facts);
		expect(row.Title == "The queue's races" && row.TitleNote == "The queue's races, in #2368 Queued prompt rows leave the list" && row.SessionName == "The queue's races",
			   "a session the reviewer named is the row's title, and its tooltip says which pull request it is in");

		auto part = chat_header_facts{ .Kind = chat_kind::tour_part, .Subject = "Stop 2, The claim is stored first", .Place = "src/queue.cpp, lines 4 to 9", .CanShow = true, .SessionName = "Named" };
		row       = plan_chat_header(part);
		expect(row.Title == part.Subject && row.Place == part.Place && row.TitleNote.empty() && row.SessionName == "Named", "a side chat's row keeps saying which part it stands beside, whatever its session is named");

		auto item = chat_header_facts{ .Kind = chat_kind::review_item, .Subject = "Suggested comment", .Place = "queue.cpp, lines 212 to 218", .CanShow = true, .SessionName = "Named", .RevisionRead = "3f9c2ab1" };
		row       = plan_chat_header(item);
		expect(row.Title == item.Subject && row.Place == item.Place && row.SessionName == "Named" && row.RevisionRead == "3f9c2ab" && row.ToolNote.empty(),
			   "an item's chat keeps saying which item it is about, whatever its session is named, and History says the revision read");
		auto local = chat_header_facts{ .Kind = chat_kind::local_draft_discussion, .Subject = "Discussion", .SessionName = "ignored", .RevisionRead = "3f9c2ab1" };
		row        = plan_chat_header(local);
		expect(row.SessionName.empty() && row.RevisionRead.empty() && row.ToolNote.empty(), "a chat that keeps no sessions names none and says no revision");
		local.Cost = agent_cost{ 20'000'000U };
		expect(plan_chat_header(local).ToolNote == "This conversation has cost $0.02 so far.", "what it has cost is said by its Copy tool, where a session's row would say it");
		item.Cost = agent_cost{ 20'000'000U };
		expect(plan_chat_header(item).ToolNote.empty(), "an item's chat says a session's cost in its row of History, not on its face");
		facts.Cost = agent_cost{ 20'000'000U };
		expect(plan_chat_header(facts).ToolNote.empty(), "a chat with sessions says a session's cost in its row of History, not on its face");
	}

	// "Yes assuming Codex there is {agent}" (owner, 2026-11-24).
	void test_words()
	{
		expect(chat_session_notice(chat_continuation::native, "Claude Code").empty(), "an ordinary session says nothing of how it goes on");
		expect(chat_continuation_of(false, true) == chat_continuation::native && chat_continuation_of(true, false) == chat_continuation::carried_over
				   && chat_continuation_of(true, true) == chat_continuation::carried_over && chat_continuation_of(false, false) == chat_continuation::cannot_continue,
			   "carried over wins; an agent that cannot go on natively cannot continue");
		for (const auto* const agent : { "Claude Code", "Codex", "My reviewer" }) {
			const auto name = std::string{ agent };
			expect(chat_session_notice(chat_continuation::carried_over, name) == "Carried over from a saved transcript. " + name + " has the questions and answers, not what the earlier agent read.",
				   "the carried-over notice names the chosen agent: " + name);
			expect(chat_session_notice(chat_continuation::cannot_continue, name) == name + " can't pick up this conversation where it stopped. Choose New session, or carry the transcript over.",
				   "the cannot-continue notice names the chosen agent: " + name);
			expect(chat_carry_over_words(name) == "Carry this conversation over to " + name, "the carry-over item names the chosen agent: " + name);
		}
		const auto claude = chat_session_notice(chat_continuation::carried_over, "Claude Code") + chat_session_notice(chat_continuation::cannot_continue, "Claude Code") + chat_carry_over_words("Claude Code");
		expect(claude.find("Codex") == std::string::npos, "no sentence names an agent of its own");
		expect(chat_session_notice(chat_continuation::carried_over, "").starts_with("Carried over from a saved transcript. The agent has")
				   && chat_session_notice(chat_continuation::cannot_continue, "").starts_with("The agent can't") && chat_carry_over_words("") == "Carry this conversation over to the chosen agent",
			   "while no agent is known the sentences say so plainly");

		expect(chat_carry_over_tip(0U, false) == "Long conversations are shortened: the oldest turns are left out.", "what carrying over leaves out");
		expect(chat_carry_over_tip(1U, false).ends_with(" This one left out 1 older turn.") && chat_carry_over_tip(4U, true).ends_with(" This one left out 4 older turns. Some of its longest text was cut."),
			   "and what it left out of this conversation");
		expect(chat_revision_read_words("3f9c2ab11111") == "Reads the pull request at 3f9c2ab" && chat_revision_read_words("").empty(), "History's last line, seven characters");

		const auto all = claude + chat_carry_over_tip(4U, true) + chat_revision_read_words("3f9c2ab");
		for (const auto* const gone : { "Native continuation", "Transcript replay", "Native:", "KiB" }) {
			expect(all.find(gone) == std::string::npos, std::string{ "no sentence says " } + gone);
		}
	}
}

int main()
{
	test_offered();
	test_row();
	test_words();
	std::cout << "chat-header-core:pass\n";
	return EXIT_SUCCESS;
}
