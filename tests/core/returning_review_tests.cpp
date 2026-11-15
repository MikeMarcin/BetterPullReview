/// \file
/// \brief RFC-BPR-0116 ([[BPR-2073]], [[BPR-2074]]). When What changed since my
/// review and Was this addressed? are offered, the two revisions the first
/// names, its change text and bound, the thread's lines then and now, the
/// four-word lead of an answer, and the supplement's bounds. No Qt.

#include "bpr/core/agent_chat_scope.hpp"
#include "bpr/core/agent_question.hpp"
#include "bpr/core/returning_review.hpp"
#include "product_diff.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto commit(const char aDigit) -> std::string { return std::string(40U, aDigit); }
	auto bytes(const std::string_view aText) -> std::vector<std::uint8_t> { return { aText.begin(), aText.end() }; }

	auto facts(std::string aReviewed, std::string aSeen, std::string aCurrent, core::returning_material aMaterial) -> core::latest_upload_facts
	{
		return { .ReviewedHead = std::move(aReviewed), .SeenHead = std::move(aSeen), .CurrentHead = std::move(aCurrent), .Commits = { commit('1'), commit('2'), commit('3') }, .Material = aMaterial };
	}

	/// AC01. Offered only with commits newer than the review or the Seen,
	/// and it names the earlier of the two revisions it compares.
	void latest_upload_is_offered_only_for_newer_commits()
	{
		using state = core::latest_upload_state;
		expect(core::decide_latest_upload(facts({ }, { }, commit('3'), core::returning_material::readable)).State == state::hidden, "no review and no Seen: not offered");
		expect(core::decide_latest_upload(facts(commit('3'), { }, commit('3'), core::returning_material::readable)).State == state::hidden, "reviewed at the head: not offered");
		const auto offered = core::decide_latest_upload(facts(commit('1'), { }, commit('3'), core::returning_material::readable));
		expect(offered.State == state::offered && offered.Earlier && offered.Earlier->Head == commit('1') && offered.Earlier->Basis == core::returning_basis::review,
			   "reviewed earlier: offered on the reviewed head");
		const auto seen = core::decide_latest_upload(facts(commit('1'), commit('2'), commit('3'), core::returning_material::readable));
		expect(seen.State == state::offered && seen.Earlier && seen.Earlier->Head == commit('2') && seen.Earlier->Basis == core::returning_basis::seen, "a later Seen is the earlier revision");
		const auto reviewedAfterSeen = core::decide_latest_upload(facts(commit('2'), commit('1'), commit('3'), core::returning_material::readable));
		expect(reviewedAfterSeen.Earlier && reviewedAfterSeen.Earlier->Head == commit('2'), "a review after the Seen is the earlier revision");
		expect(core::decide_latest_upload(facts(commit('1'), commit('3'), commit('3'), core::returning_material::readable)).State == state::hidden, "Seen at the head: not offered");
		const auto gone = core::decide_latest_upload(facts(commit('9'), { }, commit('3'), core::returning_material::readable));
		expect(gone.State == state::offered && gone.Earlier->Head == commit('9'), "a review on a commit no longer listed still names it");
		auto authored     = facts(commit('1'), { }, commit('3'), core::returning_material::readable);
		authored.Authored = true;
		expect(core::decide_latest_upload(authored).State == state::hidden, "never on the reviewer's own pull request");
		expect(core::decide_latest_upload(facts(commit('1'), { }, commit('3'), core::returning_material::reading)).State == state::reading, "reading while the material is read");
	}

	/// AC02. The earlier revision cannot be read: unavailable, and it still
	/// says which revision that is.
	void unreadable_earlier_revision_is_unavailable()
	{
		const auto offer = core::decide_latest_upload(facts(commit('1'), { }, commit('3'), core::returning_material::unreadable));
		expect(offer.State == core::latest_upload_state::unavailable && offer.Earlier && offer.Earlier->Head == commit('1'), "unavailable, naming the unreadable revision");
		expect(core::stable_name(offer.State) == "unavailable", "named");
	}

	/// AC03. Was this addressed? is offered only on a thread the reviewer
	/// commented in, after commits newer than that comment, and asks about
	/// the conversation with no selection.
	void proposed_fix_is_offered_on_my_threads_after_new_commits()
	{
		const auto head = commit('3');
		expect(core::proposed_fix_offered({ .ReviewerCommented = true, .LastCommentCommit = commit('1'), .CurrentHead = head }), "my comment, newer commits: offered");
		expect(!core::proposed_fix_offered({ .ReviewerCommented = true, .LastCommentCommit = head, .CurrentHead = head }), "my comment on the head: not offered");
		expect(!core::proposed_fix_offered({ .ReviewerCommented = false, .LastCommentCommit = commit('1'), .CurrentHead = head }), "someone else's thread: not offered");
		expect(!core::proposed_fix_offered({ .ReviewerCommented = true, .LastCommentCommit = { }, .CurrentHead = head }), "a comment whose commit the read does not name: not offered");
		expect(!core::proposed_fix_offered({ .Note = true, .ReviewerCommented = true, .LastCommentCommit = commit('1'), .CurrentHead = head }), "never on a private note");
		expect(!core::acts_on_selection(core::agent_entry_point::evaluate_proposed_fix), "evaluate_proposed_fix acts on a conversation, not a selection");
		expect(!core::acts_on_selection(core::agent_entry_point::analyze_latest_upload), "analyze_latest_upload needs no selection");
		const auto addressed = core::decide_agent_question({ .ConversationAttached = true, .Returning = core::returning_operation::proposed_fix });
		expect(addressed && addressed->Subject == core::agent_question_subject::conversation && addressed->EntryPoint == core::agent_entry_point::evaluate_proposed_fix,
			   "Was this addressed? is a question about the conversation through evaluate_proposed_fix");
		expect(!core::decide_agent_question({ .Returning = core::returning_operation::proposed_fix }), "and is not sent without a conversation");
		const auto changed = core::decide_agent_question({ .LinesAttached = true, .LinesSelected = true, .Returning = core::returning_operation::latest_upload });
		expect(changed && changed->Subject == core::agent_question_subject::pull_request && changed->EntryPoint == core::agent_entry_point::analyze_latest_upload,
			   "What changed since my review is about the pull request through analyze_latest_upload, whatever is attached");
	}

	/// AC04. The four words, read from the start of an answer only.
	void the_answer_leads_with_one_of_four_words()
	{
		using claim = core::addressed_claim;
		expect(core::read_addressed_claim("Addressed. The clamp is in.") == claim::addressed, "addressed");
		expect(core::read_addressed_claim("**Partly** addressed: the clamp is in, the test is not.") == claim::partly, "partly, in bold");
		expect(core::read_addressed_claim("  Not addressed — nothing changed there.") == claim::not_addressed, "not addressed is not read as addressed");
		expect(core::read_addressed_claim("# Cannot tell\nThe file moved.") == claim::cannot_tell, "cannot tell, as a heading");
		expect(core::read_addressed_claim("Can't tell from the diff.") == claim::cannot_tell, "can't tell");
		expect(core::read_addressed_claim("NOT ADDRESSED") == claim::not_addressed, "any case");
		expect(!core::read_addressed_claim("Addressedness is unclear"), "a longer word is not the claim");
		expect(!core::read_addressed_claim("The fix looks addressed."), "a claim later in the answer is no lead");
		expect(!core::read_addressed_claim(""), "an empty answer leads with nothing");
		expect(core::claim_words(claim::not_addressed) == "Not addressed" && core::stable_name(claim::cannot_tell) == "cannot_tell", "words and names");
	}

	auto change(const std::string_view aPath, const std::string_view aEarlier, const std::string_view aCurrent) -> std::string
	{
		const auto left    = bytes(aEarlier);
		const auto right   = bytes(aCurrent);
		const auto options = core::diff_options{ };
		const auto key     = core::returning_comparison(aPath, std::span<const std::uint8_t>{ left }, std::span<const std::uint8_t>{ right }, options);
		expect(key.has_value(), "a comparison key");
		const auto outcome = tests::product_material_diff({ .Key = *key, .Options = options, .LeftBytes = std::span<const std::uint8_t>{ left }, .RightBytes = std::span<const std::uint8_t>{ right }, .Limits = { } });
		expect(outcome.Result.has_value(), "the comparison computes");
		return core::render_returning_change(aPath, *outcome.Result);
	}

	/// AC01. The change between the two revisions, as the agent reads it.
	void the_change_is_unified_text_within_its_bound()
	{
		const auto text = change("src/a.cpp", "1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n", "1\n2\n3\n4\n5\nsix\n7\n8\n9\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n");
		const auto want = std::string{ "--- a/src/a.cpp\n+++ b/src/a.cpp\n@@ -3,7 +3,7 @@\n 3\n 4\n 5\n-6\n+six\n 7\n 8\n 9\n@@ -18,3 +18,4 @@\n 18\n 19\n 20\n+21\n" };
		expect(text == want, "two hunks with three lines of context:\n" + text);
		expect(change("src/b.cpp", "same\n", "same\n").empty(), "an unchanged file renders nothing");
		const auto near = change("src/c.cpp", "a\nb\nc\nd\ne\nf\ng\n", "A\nb\nc\nd\ne\nf\nG\n");
		expect(near.find("@@ -1,7 +1,7 @@") != std::string::npos && near.find("@@ -", near.find("@@ -") + 1U) == std::string::npos, "changes whose context touches are one hunk:\n" + near);

		auto       files  = std::vector<core::returning_change_file>{ { .Path = "z.cpp", .Text = "Z\n" }, { .Path = "a.cpp", .Text = "A\n" }, { .Path = "m.cpp", .Text = { } } };
		const auto joined = core::bound_returning_changes(files);
		expect(joined.Text == "A\nZ\n" && joined.FilesShown == 2U && joined.FilesOmitted == 0U, "in path order; an unchanged file is not counted");
		auto       big     = std::vector<core::returning_change_file>{ { .Path = "a", .Text = std::string(core::maximum_returning_change_bytes - 1U, 'x') }, { .Path = "b", .Text = "yy" } };
		const auto bounded = core::bound_returning_changes(big);
		expect(bounded.FilesShown == 1U && bounded.FilesOmitted == 1U && bounded.Text.size() <= core::maximum_returning_change_bytes, "past the bound a file is counted, not cut");
	}

	/// AC03. The thread's lines then and now, with context.
	void excerpts_keep_the_lines_and_their_context()
	{
		auto text = std::string{ };
		for (auto line = 1; line <= 30; ++line) {
			text += "line " + std::to_string(line) + '\n';
		}
		const auto excerpt = core::returning_excerpt(commit('1'), "a.cpp", text, 12U, 13U);
		expect(excerpt && excerpt->FirstLine == 6U && excerpt->Lines.size() == 14U && excerpt->Lines.front() == "line 6" && excerpt->Lines.back() == "line 19", "six lines around");
		expect(!core::returning_excerpt(commit('1'), "a.cpp", text, 31U, 31U), "a line the file does not have is no excerpt");
		const auto top = core::returning_excerpt(commit('1'), "a.cpp", "a\r\nb\r\n", 1U, 1U);
		expect(top && top->FirstLine == 1U && top->Lines == std::vector<std::string>{ "a", "b" }, "carriage returns are not kept");
	}

	void the_supplement_keeps_its_bounds()
	{
		auto latest = core::supplement_returning{ .Operation = core::returning_operation::latest_upload, .EarlierHead = commit('1'), .CurrentHead = commit('3'), .Changes = "--- a/x\n", .FilesShown = 1U };
		expect(core::valid(latest), "a latest-upload supplement");
		auto same        = latest;
		same.EarlierHead = same.CurrentHead;
		expect(!core::valid(same), "the two revisions differ");
		auto withLines = latest;
		withLines.Then = core::returning_lines{ .Commit = commit('1'), .Path = "x", .FirstLine = 1U, .Lines = { "a" } };
		expect(!core::valid(withLines), "lines ride only on a proposed-fix run");
		auto fix = core::supplement_returning{ .Operation = core::returning_operation::proposed_fix, .EarlierHead = commit('1'), .CurrentHead = commit('3') };
		fix.Then = core::returning_lines{ .Commit = commit('1'), .Path = "x", .FirstLine = 1U, .Lines = { "a" } };
		expect(core::valid(fix), "a proposed-fix supplement");
		fix.Changes = "--- a/x\n";
		expect(!core::valid(fix), "no change text on a proposed-fix run");
		latest.Changes = std::string(core::maximum_returning_change_bytes + 1U, 'x');
		expect(!core::valid(latest), "change text within its bound");
	}
}

namespace {
	// Look ask-v005 (owner, 2026-11-14): "Why would we do it this one instead
	// of having one large prompt with all 6 threads? We could still have it
	// build structured output if we want to display each thread result
	// separately." One ask carries every thread; its answer's bpr-claims
	// block names each thread's claim.
	void one_ask_reads_every_threads_claim()
	{
		using claim       = core::addressed_claim;
		const auto answer = std::string{ "Two of three are done.\n\n```bpr-claims\nreview-thread:PRRT_cc_1: Partly, the test is missing\nreview-thread:PRRT_cc_2: **Addressed**\nreview-thread:PRRT_cc_3: not addressed\nnonsense line\nreview-thread:PRRT_cc_4: maybe\n```\nMore prose." };
		const auto claims = core::read_addressed_claims(answer);
		expect(claims.size() == 3U, "three readable claims, the unreadable two left out");
		expect(claims[0].first == "review-thread:PRRT_cc_1" && claims[0].second == claim::partly, "a thread's claim");
		expect(claims[1].second == claim::addressed && claims[2].second == claim::not_addressed, "each in order");
		expect(core::read_addressed_claims("Addressed.").empty(), "no block, no claims");
		expect(core::read_addressed_claims("```bpr-claims\nreview-thread:PRRT_cc_1: partly").size() == 1U, "an unclosed block is read to its end");
		expect(core::stable_name(core::returning_operation::addressed_threads) == "addressed_threads", "its operation's name");

		auto many = core::supplement_returning{ .Operation = core::returning_operation::addressed_threads, .EarlierHead = "a", .CurrentHead = "b" };
		expect(!core::valid(many), "no thread, nothing to ask");
		many.Threads.push_back({ .Identity = "review-thread:PRRT_cc_1", .Path = "src/a.cpp", .Line = 6U, .Baseline = "a", .Conversation = "tbutler: clamp it" });
		many.Threads.push_back({ .Identity = "review-thread:PRRT_cc_2", .Path = "src/a.cpp", .Line = 12U, .Baseline = "c", .Conversation = "jiayin: and the height" });
		expect(core::valid(many), "threads with their own baselines");
		many.Threads.front().Identity.clear();
		expect(!core::valid(many), "a thread is named");
		many.Threads.front().Identity = "review-thread:PRRT_cc_1";
		many.Threads.resize(core::maximum_addressed_threads + 1U, many.Threads.back());
		expect(!core::valid(many), "within the bound");
	}
}

namespace {
	/// [[BPR-2184]] (review N4). Q6 in core: the baseline is the first
	/// comment not deleted, at the commit it was written on, else where
	/// GitHub places it; progress counts the commits after it.
	void thread_baseline_is_decided_in_core()
	{
		expect(core::thread_baseline({ { .Deleted = true, .OriginalCommit = "aaa", .Commit = "aaa" }, { .OriginalCommit = "bbb", .Commit = "ccc" } }) == "bbb", "the first comment not deleted, at the commit it was written on");
		expect(core::thread_baseline({ { .OriginalCommit = "", .Commit = "ccc" } }) == "ccc", "else the commit GitHub places it on");
		expect(core::thread_baseline({ }).empty() && core::thread_baseline({ { .Deleted = true, .OriginalCommit = "a", .Commit = "a" } }).empty(), "none when nothing is left");
		const auto commits = std::vector<std::string>{ "c1", "c2", "c3", "c4" };
		const auto since   = core::thread_progress_since("c2", "c4", commits);
		expect(since.Changed == true && since.CommitsSince == 2U, "two commits after c2, the head past it");
		expect(core::thread_progress_since("c4", "c4", commits).Changed == false, "at the head: unchanged");
		expect(!core::thread_progress_since("gone", "c4", commits).CommitsSince && core::thread_progress_since("gone", "c4", commits).Changed == true, "a rebased-away baseline: changed, uncounted");
		expect(!core::thread_progress_since("", "c4", commits).Changed, "no baseline: unknown");
	}
}

auto main() -> int
{
	thread_baseline_is_decided_in_core();
	one_ask_reads_every_threads_claim();
	latest_upload_is_offered_only_for_newer_commits();
	unreadable_earlier_revision_is_unavailable();
	proposed_fix_is_offered_on_my_threads_after_new_commits();
	the_answer_leads_with_one_of_four_words();
	the_change_is_unified_text_within_its_bound();
	excerpts_keep_the_lines_and_their_context();
	the_supplement_keeps_its_bounds();
	std::cout << "returning review core tests passed\n";
	return EXIT_SUCCESS;
}
