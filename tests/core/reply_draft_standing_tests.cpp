// [[BPR-1374]]. An unsent reply survives a Refresh and a restart, and a reply
// whose thread is resolved or deleted elsewhere is kept and flagged rather
// than dropped. The decisions only: where the reply stands against the
// accepted server threads, which saved draft a line opens again, which reply
// the editor holds again after a Refresh, and the commands that keep a reply
// whose thread is gone as a new comment, or discard it. Nothing here writes
// the server; a reply whose thread is gone is never sendable.
#include "bpr/core/reply_draft_standing.hpp"
#include "bpr/core/review_round_trip.hpp"
#include "bpr/core/revisioned_review_draft.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	// Every scenario runs, and each failed expectation is reported, so a
	// rule changed back fails each scenario it breaks by name.
	int  Failures = 0;
	auto Scenario = std::string_view{ };

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: [" << Scenario << "] " << aMessage << '\n';
			++Failures;
		}
	}

	[[nodiscard]] auto comparison() -> file_comparison_digest
	{
		std::array<std::uint8_t, 32> bytes{ };
		bytes[0]  = 0x74U;
		bytes[31] = 0x13U;
		return file_comparison_digest{ bytes };
	}

	[[nodiscard]] auto line(const std::uint32_t aLine) -> source_line_ref
	{
		return { comparison(), diff_side::right, aLine };
	}

	constexpr auto thread_id = std::string_view{ "PRRT_reply_1" };
	constexpr auto body      = std::string_view{ "Local unsent reply draft held while another client edits." };

	/// The reviewer's reply on line 1, answering `PRRT_reply_1`, as saved.
	[[nodiscard]] auto reply(const std::uint64_t aId = 7U) -> review_comment_draft
	{
		return { .Id = review_draft_id{ aId }, .Anchor = line(0U), .Body = std::string{ body }, .State = review_draft_state::drafting, .ReplyToThread = std::string{ thread_id } };
	}

	/// The new comment the same line opens when no reply is asked for.
	[[nodiscard]] auto comment(const std::uint64_t aId) -> review_comment_draft
	{
		return { .Id = review_draft_id{ aId }, .Anchor = line(0U), .Body = { }, .State = review_draft_state::drafting };
	}

	[[nodiscard]] auto server(const bool aThreadThere, const bool aResolved = false, const bool aComplete = true) -> reply_thread_observation
	{
		auto threads     = reply_thread_observation{ };
		threads.Complete = aComplete;
		threads.Threads.push_back({ .Thread = "PRRT_other", .Resolved = false });
		if (aThreadThere) {
			threads.Threads.push_back({ .Thread = std::string{ thread_id }, .Resolved = aResolved });
		}
		return threads;
	}

	[[nodiscard]] auto saved(review_comment_draft aDraft) -> revisioned_review_draft
	{
		return { .Draft = std::move(aDraft), .Revision = 3U, .Scope = review_draft_scope_id{ 9U } };
	}

	void refresh_with_no_change_holds_the_same_reply_again()
	{
		// The editor held the reply; the Refresh retired it with the
		// composition; nothing changed on the server.
		const auto drafts = std::vector{ comment(5U), reply() };
		const auto again  = resume_reply_after_refresh(drafts, review_draft_id{ 7U }, server(true));
		expect(again == std::optional{ review_draft_id{ 7U } }, "refresh, no change: the editor holds the same reply again");
		const auto decision = decide_reply_draft(reply(), server(true));
		expect(decision.Standing == reply_draft_standing::answerable && decision.Sendable && !decision.KeepableAsComment && decision.Thread == thread_id,
			   "refresh, no change: the reply stands on its thread, sendable");
		expect(reopen_draft_at(drafts, line(0U), std::string{ thread_id }) == std::optional{ review_draft_id{ 7U } },
			   "refresh, no change: choosing the line for its thread opens the same reply, not a new one");
	}

	void refresh_with_an_external_edit_keeps_the_reply_and_takes_the_server_text()
	{
		// Another client edited a post in the thread. The thread's identity
		// is the same; the reply's words are the reviewer's and are not read
		// against the server's.
		auto edited = server(true);
		expect(decide_reply_draft(reply(), edited).Standing == reply_draft_standing::answerable, "external edit: the reply still answers its thread");
		expect(resume_reply_after_refresh(std::vector{ reply() }, review_draft_id{ 7U }, edited) == std::optional{ review_draft_id{ 7U } },
			   "external edit: the editor holds the same reply again");
		const auto decisions = decide_reply_drafts(std::vector{ reply() }, edited);
		expect(decisions.size() == 1U && decisions.front().Draft == review_draft_id{ 7U }, "external edit: one unsent reply, kept");
	}

	void restart_presents_the_saved_reply_on_its_thread()
	{
		// After a restart nothing held the reply; the store restores it with
		// the line's empty comment allocated before it.
		const auto restored  = std::vector{ comment(5U), reply(), comment(8U) };
		const auto decisions = decide_reply_drafts(restored, server(true));
		expect(decisions.size() == 1U && decisions.front().Standing == reply_draft_standing::answerable && decisions.front().Thread == thread_id,
			   "restart: the saved reply is presented on its thread");
		expect(reopen_draft_at(restored, line(0U), std::string{ thread_id }) == std::optional{ review_draft_id{ 7U } },
			   "restart: Reply on the thread opens the saved reply, not an empty one");
		expect(reopen_draft_at(restored, line(0U), std::nullopt) == std::optional{ review_draft_id{ 8U } },
			   "restart: choosing the line opens its latest new comment, never the reply");
		expect(!reopen_draft_at(restored, line(1U), std::string{ thread_id }).has_value(), "restart: another line opens nothing saved");
		expect(!reopen_draft_at(restored, line(0U), std::string{ "PRRT_other" }).has_value(), "restart: another thread's Reply opens nothing saved");
	}

	void a_deleted_thread_keeps_the_reply_flagged_and_unsendable()
	{
		const auto gone     = server(false);
		const auto decision = decide_reply_draft(reply(), gone);
		expect(decision.Standing == reply_draft_standing::thread_deleted, "deleted: the reply is kept, flagged as its thread deleted");
		expect(!decision.Sendable, "deleted: the reply is never sent to a thread that is gone");
		expect(decision.KeepableAsComment, "deleted: the reply can be kept as a comment on its line");
		expect(!resume_reply_after_refresh(std::vector{ reply() }, review_draft_id{ 7U }, gone).has_value(),
			   "deleted: the reply is not put back into a thread that is gone");
		expect(decide_reply_drafts(std::vector{ comment(5U), reply() }, gone).size() == 1U, "deleted: the reply is still listed, not dropped");

		const auto kept = keep_reply_as_comment(saved(reply()), gone);
		expect(kept.has_value() && kept->ChangeReplyTarget && !kept->ReplyToThread && kept->ExpectedRevision == 3U, "deleted: Keep as comment is one edit that drops the thread");
		if (!kept) {
			return;
		}
		const auto applied = apply(saved(reply()), *kept);
		expect(applied.Refusal == revisioned_draft_refusal::none && applied.Changed, "deleted: Keep as comment applies");
		expect(!applied.Accepted.Draft.ReplyToThread && applied.Accepted.Draft.Body == body && applied.Accepted.Draft.Anchor == line(0U)
				   && applied.Accepted.Draft.State == review_draft_state::drafting && applied.Accepted.Revision == 4U,
			   "deleted: the kept comment has the reply's words on its line, still a draft");
		expect(decide_reply_draft(applied.Accepted.Draft, gone).Standing == reply_draft_standing::none, "deleted: once kept as a comment it is no longer a reply");

		const auto discard = discard_unsent_reply(saved(reply()));
		expect(discard.has_value() && apply(saved(reply()), discard.value_or(revisioned_review_draft_command{ })).Accepted.Draft.State == review_draft_state::discarded, "deleted: Discard discards it");
	}

	void an_unread_thread_list_never_calls_a_thread_deleted()
	{
		const auto unread   = server(false, false, false);
		const auto decision = decide_reply_draft(reply(), unread);
		expect(decision.Standing == reply_draft_standing::thread_unknown && !decision.Sendable && !decision.KeepableAsComment,
			   "unknown: a thread missing from an incomplete read is not called deleted");
		expect(!keep_reply_as_comment(saved(reply()), unread).has_value(), "unknown: nothing is offered from an absence the read cannot vouch for");
	}

	void a_resolved_thread_keeps_the_reply_flagged_and_sendable()
	{
		const auto resolved = server(true, true);
		const auto decision = decide_reply_draft(reply(), resolved);
		expect(decision.Standing == reply_draft_standing::thread_resolved, "resolved: the reply is kept, flagged as its thread resolved");
		expect(decision.Sendable && !decision.KeepableAsComment, "resolved: the server takes a reply on a resolved thread, so it can still be sent there");
		expect(resume_reply_after_refresh(std::vector{ reply() }, review_draft_id{ 7U }, resolved) == std::optional{ review_draft_id{ 7U } },
			   "resolved: the editor holds the same reply again");
		expect(!keep_reply_as_comment(saved(reply()), resolved).has_value(), "resolved: the thread is there, so the reply stays a reply");
	}

	void only_an_unsent_reply_with_words_has_a_standing()
	{
		const auto there = server(true);
		expect(decide_reply_draft(comment(5U), there).Standing == reply_draft_standing::none, "a new comment is not a reply");
		auto empty = reply();
		empty.Body = " \n\t";
		expect(decide_reply_draft(empty, server(false)).Standing == reply_draft_standing::none, "an empty reply has nothing to keep");
		auto sending  = reply();
		sending.State = review_draft_state::publishing;
		expect(decide_reply_draft(sending, there).Standing == reply_draft_standing::none, "a reply being published is the publication's");
		expect(reopen_draft_at(std::vector{ sending }, line(0U), std::string{ thread_id }) == std::optional{ review_draft_id{ 7U } }, "a reply being published still opens on its line");
		auto sent  = reply();
		sent.State = review_draft_state::published;
		expect(decide_reply_draft(sent, server(false)).Standing == reply_draft_standing::none, "a published reply is GitHub's");
		expect(!reopen_draft_at(std::vector{ sent }, line(0U), std::string{ thread_id }).has_value(), "a published reply is not opened again");
		auto gone  = reply();
		gone.State = review_draft_state::discarded;
		expect(!resume_reply_after_refresh(std::vector{ gone }, review_draft_id{ 7U }, there).has_value(), "a discarded reply is not held again");
		expect(!discard_unsent_reply(saved(comment(5U))).has_value(), "Discard here is only for an unsent reply");
		expect(name(reply_draft_standing::thread_deleted) == "thread_deleted", "standings have stable names");
	}

	/// [[BPR-1475]]. A reread after publishing holds the note being written
	/// again; one that was sent, discarded or is a reply is not.
	void a_reread_holds_the_comment_being_written_again()
	{
		auto written = comment(5U);
		written.Body = "A note still being written.";
		expect(resume_comment_after_refresh(std::vector{ written, reply() }, review_draft_id{ 5U }) == std::optional{ review_draft_id{ 5U } }, "the comment being written is held again");
		auto sent  = written;
		sent.State = review_draft_state::published;
		expect(!resume_comment_after_refresh(std::vector{ sent }, review_draft_id{ 5U }).has_value(), "a published comment is not held again");
		auto gone  = written;
		gone.State = review_draft_state::discarded;
		expect(!resume_comment_after_refresh(std::vector{ gone }, review_draft_id{ 5U }).has_value(), "a discarded comment is not held again");
		expect(!resume_comment_after_refresh(std::vector{ reply() }, review_draft_id{ 7U }).has_value(), "a reply is held again by its own rule");
		expect(!resume_comment_after_refresh(std::vector{ written }, review_draft_id{ 6U }).has_value(), "a comment no longer saved is not held again");
	}
}

int main()
{
	const auto run = [](const std::string_view aName, void (*aScenario)()) {
		Scenario = aName;
		aScenario();
	};
	run("refresh with no change", refresh_with_no_change_holds_the_same_reply_again);
	run("refresh with an external edit", refresh_with_an_external_edit_keeps_the_reply_and_takes_the_server_text);
	run("restart", restart_presents_the_saved_reply_on_its_thread);
	run("thread deleted", a_deleted_thread_keeps_the_reply_flagged_and_unsendable);
	run("thread list unread", an_unread_thread_list_never_calls_a_thread_deleted);
	run("thread resolved", a_resolved_thread_keeps_the_reply_flagged_and_sendable);
	run("no standing", only_an_unsent_reply_with_words_has_a_standing);
	run("reread holds the comment", a_reread_holds_the_comment_being_written_again);
	if (Failures != 0) {
		std::cerr << Failures << " expectations failed\n";
		return EXIT_FAILURE;
	}
	std::cout << "REPLY_DRAFT_STANDING_OK\n";
	return EXIT_SUCCESS;
}
