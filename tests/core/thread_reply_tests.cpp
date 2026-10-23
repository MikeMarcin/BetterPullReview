// [[BPR-1745]]. The owner, 2026-10-22: "Allow replying to an outdated
// thread from the drawer, like GitHub does". The decisions only: where a reply
// to a thread is written (on its line, or bound to the thread when it is
// outdated), what GitHub's thread data refuses, which saved thread-bound reply
// a thread's Reply opens again, and what the draft reducer and the reply
// standing refuse a reply that has no line. Nothing here writes the server.
#include "bpr/core/reply_draft_standing.hpp"
#include "bpr/core/review_round_trip.hpp"
#include "bpr/core/revisioned_review_draft.hpp"
#include "bpr/core/thread_reply.hpp"

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

	int  Failures = 0;
	auto Scenario = std::string_view{ };

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: [" << Scenario << "] " << aMessage << '\n';
			++Failures;
		}
	}

	[[nodiscard]] auto comparison(const std::uint8_t aSeed) -> file_comparison_digest
	{
		std::array<std::uint8_t, 32> bytes{ };
		bytes[0]  = aSeed;
		bytes[31] = 0x42U;
		return file_comparison_digest{ bytes };
	}

	constexpr auto outdated_thread = std::string_view{ "PRRT_outdated_1" };

	[[nodiscard]] auto thread_reply(const std::uint64_t aId, const std::uint8_t aComparison = 1U, const review_draft_state aState = review_draft_state::drafting) -> review_comment_draft
	{
		return { .Id = review_draft_id{ aId }, .Anchor = thread_reply_anchor(comparison(aComparison)), .Body = "Still true after the change?", .State = aState, .ReplyToThread = std::string{ outdated_thread } };
	}

	void routes()
	{
		Scenario            = "routes";
		const auto outdated = decide_thread_reply({ .ReviewThread = true, .Outdated = true });
		expect(outdated.Route == thread_reply_route::on_thread && outdated.Refusal == thread_reply_refusal::none && outdated.OutdatedNote,
			   "an outdated thread is answered by the thread, with the outdated note (it was refused before)");
		const auto current = decide_thread_reply({ .ReviewThread = true, .Outdated = false });
		expect(current.Route == thread_reply_route::on_line && !current.OutdatedNote, "a current thread is answered on its line, as before");
		const auto allowed = decide_thread_reply({ .ReviewThread = true, .Outdated = true, .ViewerCanReply = true, .Locked = false });
		expect(allowed.Route == thread_reply_route::on_thread, "GitHub saying the viewer can reply, and the pull request open, keeps the route");
		const auto unread = decide_thread_reply({ .ReviewThread = true, .Outdated = true, .ViewerCanReply = std::nullopt, .Locked = std::nullopt });
		expect(unread.Route == thread_reply_route::on_thread, "facts the read does not carry refuse nothing: GitHub decides at Publish");
	}

	void refusals()
	{
		Scenario          = "refusals";
		const auto denied = decide_thread_reply({ .ReviewThread = true, .Outdated = true, .ViewerCanReply = false });
		expect(denied.Route == thread_reply_route::refused && denied.Refusal == thread_reply_refusal::viewer_cannot_reply && !denied.OutdatedNote,
			   "GitHub saying the viewer cannot reply refuses the reply");
		// [[BPR-PENDING-AX]]. GitHub's `viewerCanReply` already weighs the
		// lock: false for a reader on a locked pull request, true for a
		// collaborator, as BPR-Test#135 answered live on 2026-10-23.
		const auto locked = decide_thread_reply({ .ReviewThread = true, .Outdated = true, .ViewerCanReply = false, .Locked = true });
		expect(locked.Route == thread_reply_route::refused && locked.Refusal == thread_reply_refusal::locked && !locked.OutdatedNote,
			   "a reader on a locked pull request is refused, and the reason is the lock");
		const auto currentLocked = decide_thread_reply({ .ReviewThread = true, .Outdated = false, .ViewerCanReply = false, .Locked = true });
		expect(currentLocked.Route == thread_reply_route::refused && currentLocked.Refusal == thread_reply_refusal::locked, "the lock refuses a current thread's reply too");
		const auto collaborator = decide_thread_reply({ .ReviewThread = true, .Outdated = false, .ViewerCanReply = true, .Locked = true });
		expect(collaborator.Route == thread_reply_route::on_line && collaborator.Refusal == thread_reply_refusal::none,
			   "a collaborator GitHub lets reply on a locked pull request is not refused");
		const auto collaboratorOutdated = decide_thread_reply({ .ReviewThread = true, .Outdated = true, .ViewerCanReply = true, .Locked = true });
		expect(collaboratorOutdated.Route == thread_reply_route::on_thread && collaboratorOutdated.OutdatedNote, "nor on an outdated thread");
		const auto lockUnweighed = decide_thread_reply({ .ReviewThread = true, .Outdated = false, .ViewerCanReply = std::nullopt, .Locked = true });
		expect(lockUnweighed.Route == thread_reply_route::on_line, "a lock with GitHub's permission unread refuses nothing: GitHub decides at Publish");
		const auto currentDenied = decide_thread_reply({ .ReviewThread = true, .Outdated = false, .ViewerCanReply = false, .Locked = false });
		expect(currentDenied.Route == thread_reply_route::refused && currentDenied.Refusal == thread_reply_refusal::viewer_cannot_reply,
			   "GitHub refusing the viewer on an open pull request refuses a current thread's reply");
		const auto anonymousDenied = decide_thread_reply({ .ReviewThread = true, .AnonymousRead = true, .ViewerCanReply = false, .Locked = true });
		expect(anonymousDenied.Refusal == thread_reply_refusal::anonymous_read, "an anonymous read is refused as anonymous before GitHub's permission");
		const auto anonymous = decide_thread_reply({ .ReviewThread = true, .Outdated = true, .AnonymousRead = true });
		expect(anonymous.Refusal == thread_reply_refusal::anonymous_read, "an anonymous read has no account to reply as");
		const auto comment = decide_thread_reply({ .ReviewThread = false, .Outdated = true });
		expect(comment.Refusal == thread_reply_refusal::not_a_review_thread, "a pull request comment is not answered here");
		expect(name(thread_reply_route::on_thread) == "on_thread" && name(thread_reply_refusal::locked) == "locked", "the decisions have stable names");
	}

	void anchors()
	{
		Scenario         = "anchors";
		const auto draft = thread_reply(3U);
		expect(thread_bound_reply(draft) && thread_bound_anchor(draft.Anchor), "a thread-bound reply carries the thread-bound anchor");
		expect(draft.Anchor.Line == thread_reply_line && draft.Anchor.Side == diff_side::right, "it is written on the right, at the line no document has");
		auto ranged      = draft;
		ranged.StartLine = 2U;
		expect(!thread_bound_reply(ranged), "a range is not a thread-bound reply");
		auto comment          = draft;
		comment.ReplyToThread = std::nullopt;
		expect(!thread_bound_reply(comment), "without a thread it is not a reply");
		auto lined        = draft;
		lined.Anchor.Line = 4U;
		expect(!thread_bound_reply(lined), "a reply on a line is not thread-bound");
		expect(!reopen_draft_at(std::vector{ draft }, source_line_ref{ comparison(1U), diff_side::right, 4U }, std::string{ outdated_thread }).has_value(),
			   "choosing a line never opens a thread-bound reply");
	}

	void reopen()
	{
		Scenario          = "reopen";
		const auto drafts = std::vector{ thread_reply(3U, 1U), thread_reply(5U, 2U), thread_reply(6U, 2U, review_draft_state::published) };
		expect(reopen_thread_reply(drafts, outdated_thread) == std::optional{ review_draft_id{ 5U } },
			   "Reply opens the latest unsent reply to the thread, whichever comparison it was written in");
		expect(!reopen_thread_reply(drafts, "PRRT_other").has_value(), "another thread's reply is not opened");
		expect(!reopen_thread_reply(std::vector{ thread_reply(6U, 2U, review_draft_state::discarded) }, outdated_thread).has_value(), "a discarded reply is not opened");
		expect(reopen_thread_reply(std::vector{ thread_reply(8U, 2U, review_draft_state::publishing) }, outdated_thread) == std::optional{ review_draft_id{ 8U } },
			   "one being published is opened, so its outcome can be settled");
		expect(!reopen_thread_reply(drafts, "").has_value(), "no thread opens nothing");
	}

	void reducer()
	{
		Scenario         = "reducer";
		const auto saved = revisioned_review_draft{ thread_reply(3U), 2U, review_draft_scope_id{ 1U } };
		auto       edit  = revisioned_review_draft_command{ .Command = { .Id = saved.Draft.Id, .Kind = review_draft_command_kind::edit, .Body = "Edited." }, .ExpectedRevision = 2U, .Scope = review_draft_scope_id{ 1U } };
		const auto typed = apply(saved, edit);
		expect(typed.Refusal == revisioned_draft_refusal::none && typed.Accepted.Draft.Body == "Edited." && thread_bound_reply(typed.Accepted.Draft),
			   "its words can be edited, and it stays bound to its thread");
		auto retarget              = edit;
		retarget.ChangeReplyTarget = true;
		retarget.ReplyToThread     = std::nullopt;
		expect(apply(saved, retarget).Refusal == revisioned_draft_refusal::invalid_command, "it cannot become a new comment: it has no line");
		retarget.ReplyToThread = "PRRT_other";
		expect(apply(saved, retarget).Refusal == revisioned_draft_refusal::invalid_command, "it cannot answer another thread");
		auto range        = edit;
		range.ChangeRange = true;
		range.StartLine   = 1U;
		expect(apply(saved, range).Refusal == revisioned_draft_refusal::invalid_command, "it cannot take a range");
		auto publish         = edit;
		publish.Command.Kind = review_draft_command_kind::begin_publish;
		publish.Command.Body = saved.Draft.Body;
		expect(apply(saved, publish).Accepted.Draft.State == review_draft_state::publishing, "it can be published");
	}

	void standing()
	{
		Scenario         = "standing";
		const auto draft = thread_reply(3U);
		const auto here  = decide_reply_draft(draft, { .Complete = true, .Threads = { { .Thread = std::string{ outdated_thread } } } });
		expect(here.Standing == reply_draft_standing::answerable && here.Sendable, "a thread-bound reply to a thread GitHub holds is sent there");
		const auto gone = decide_reply_draft(draft, { .Complete = true, .Threads = { } });
		expect(gone.Standing == reply_draft_standing::thread_deleted && !gone.Sendable && !gone.KeepableAsComment,
			   "when the thread is gone it is kept, never sent, and not offered as a comment: it has no line");
	}
}

auto main() -> int
{
	routes();
	refusals();
	anchors();
	reopen();
	reducer();
	standing();
	if (Failures != 0) {
		std::cerr << Failures << " expectation(s) failed\n";
		return EXIT_FAILURE;
	}
	std::cout << "THREAD_REPLY_OK\n";
	return EXIT_SUCCESS;
}
