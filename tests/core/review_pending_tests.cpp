// [[BPR-1345]]. Start a review: the pending mark a draft keeps on this
// computer, the commands that give and take it, the lock a submitted review
// holds, and the review that carries pending drafts as its line comments,
// whose Comment needs no summary.
#include "bpr/core/review_preflight.hpp"
#include "bpr/core/review_round_trip.hpp"
#include "bpr/core/review_submission.hpp"
#include "bpr/core/revisioned_review_draft.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;
	using kind      = review_draft_command_kind;
	using rejection = review_submission_rejection;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto head(const std::uint8_t aSeed) -> git_commit_oid
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		for (std::size_t index = 0U; index < 20U; ++index) {
			bytes[index] = aSeed;
		}
		return git_commit_oid{ git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto mark(std::string aSubmission = { }) -> review_pending_mark
	{
		return { .Path = "src/renderer.cpp", .Head = head(7U), .Submission = std::move(aSubmission) };
	}

	constexpr auto scope = review_draft_scope_id{ 1U };

	[[nodiscard]] auto line_draft(const diff_side aSide = diff_side::right) -> revisioned_review_draft
	{
		auto draft              = revisioned_review_draft{ };
		draft.Draft.Id          = review_draft_id{ 9U };
		draft.Draft.Anchor.Side = aSide;
		draft.Draft.Anchor.Line = 430U;
		draft.Draft.Body        = "question: Where does 0.01 come from?";
		draft.Revision          = 3U;
		draft.Scope             = scope;
		return draft;
	}

	[[nodiscard]] auto command(const revisioned_review_draft& aDraft, const kind aKind, std::optional<std::optional<review_pending_mark>> aPending = std::nullopt)
		-> revisioned_review_draft_command
	{
		auto value             = revisioned_review_draft_command{ };
		value.Command.Id       = aDraft.Draft.Id;
		value.Command.Kind     = aKind;
		value.Command.Body     = aKind == kind::edit ? aDraft.Draft.Body : std::string{ };
		value.ExpectedRevision = aDraft.Revision;
		value.Scope            = aDraft.Scope;
		if (aPending) {
			value.ChangePending = true;
			value.Pending       = *aPending;
		}
		return value;
	}

	[[nodiscard]] auto accepted(const revisioned_review_draft& aDraft, const revisioned_review_draft_command& aCommand, const std::string_view aWhat)
		-> revisioned_review_draft
	{
		const auto outcome = apply(aDraft, aCommand);
		expect(outcome.Refusal == revisioned_draft_refusal::none && outcome.Changed, aWhat);
		return outcome.Accepted;
	}

	void refused(const revisioned_review_draft& aDraft, const revisioned_review_draft_command& aCommand, const review_draft_rejection aPolicy, const std::string_view aWhat)
	{
		const auto outcome = apply(aDraft, aCommand);
		expect(outcome.Refusal == revisioned_draft_refusal::policy && outcome.Policy == aPolicy && !outcome.Changed && outcome.Accepted == aDraft, aWhat);
	}

	void test_start_a_review_marks_a_draft()
	{
		const auto draft   = line_draft();
		const auto pending = accepted(draft, command(draft, kind::edit, std::optional{ mark() }), "Start a review marks the draft pending in the same revision as its text");
		expect(pending.Revision == draft.Revision + 1U && pending.Draft.InReview == mark() && pending_in_review(pending.Draft) && !held_by_review(pending.Draft),
			   "the pending draft names its file and head, and no submission holds it");
		expect(pending.Draft.Body == draft.Draft.Body && pending.Draft.Anchor == draft.Draft.Anchor && pending.Draft.State == review_draft_state::drafting,
			   "the mark changes nothing else");

		auto edit         = command(pending, kind::edit);
		edit.Command.Body = "question: Where does 0.01 come from? It looks arbitrary.";
		const auto edited = accepted(pending, edit, "an edit of a pending draft is accepted");
		expect(edited.Draft.InReview == mark(), "a pending draft stays pending while it is edited");

		auto left = line_draft(diff_side::left);
		expect(pending_in_review(accepted(left, command(left, kind::edit, std::optional{ mark() }), "a base line can be pending").Draft), "a base-side draft can be pending");

		auto reply                = line_draft();
		reply.Draft.ReplyToThread = "PRRT_thread";
		refused(reply, command(reply, kind::edit, std::optional{ mark() }), review_draft_rejection::invalid_review_mark, "a reply is never pending in a review");

		auto blank       = line_draft();
		blank.Draft.Body = " \n\t";
		refused(blank, command(blank, kind::edit, std::optional{ mark() }), review_draft_rejection::empty_body, "a review takes no comment without words");

		refused(draft, command(draft, kind::edit, std::optional{ mark("key-1") }), review_draft_rejection::invalid_review_mark, "Start a review cannot set a submission key");
		auto noPath = mark();
		noPath.Path.clear();
		refused(draft, command(draft, kind::edit, std::optional{ noPath }), review_draft_rejection::invalid_review_mark, "a mark names a file");
		auto noHead = mark();
		noHead.Head = { };
		refused(draft, command(draft, kind::edit, std::optional{ noHead }), review_draft_rejection::invalid_review_mark, "a mark names a head");

		auto stray    = command(draft, kind::edit);
		stray.Pending = mark();
		expect(apply(draft, stray).Refusal == revisioned_draft_refusal::invalid_command, "a mark is carried only by a command that changes it");
		auto withRange        = command(draft, kind::edit, std::optional{ mark() });
		withRange.ChangeRange = true;
		expect(apply(draft, withRange).Refusal == revisioned_draft_refusal::invalid_command, "a mark and a range change are separate edits");
		expect(apply(draft, command(draft, kind::discard, std::optional{ mark() })).Refusal == revisioned_draft_refusal::invalid_command,
			   "a discard carries no mark");

		auto toThread              = command(pending, kind::edit);
		toThread.ChangeReplyTarget = true;
		toThread.ReplyToThread     = "PRRT_thread";
		const auto answered        = accepted(pending, toThread, "answering a thread is accepted");
		expect(!answered.Draft.InReview, "a draft that becomes a reply leaves the review");
	}

	void test_discard_review_and_add_single_comment()
	{
		const auto draft   = line_draft();
		const auto pending = accepted(draft, command(draft, kind::edit, std::optional{ mark() }), "pending");
		const auto kept    = accepted(pending, command(pending, kind::edit, std::optional<review_pending_mark>{ }), "Discard review takes the draft out of the review");
		expect(!kept.Draft.InReview && kept.Draft.Body == pending.Draft.Body && kept.Draft.State == review_draft_state::drafting,
			   "Discard review keeps the draft and its text");

		refused(pending, command(pending, kind::begin_publish), review_draft_rejection::invalid_review_mark,
				"a pending draft is never published on its own by a Publish that leaves it in the review");
		const auto single = accepted(pending, command(pending, kind::begin_publish, std::optional<review_pending_mark>{ }), "Add single comment on a pending draft");
		expect(single.Draft.State == review_draft_state::publishing && !single.Draft.InReview && !held_by_review(single.Draft),
			   "Add single comment publishes it on its own, out of the review");
		const auto failed = accepted(single, command(single, kind::fail_publish), "its single publication fails");
		expect(failed.Draft.State == review_draft_state::drafting && !failed.Draft.InReview, "it is an ordinary draft again");

		const auto other = accepted(draft, command(draft, kind::begin_publish), "a draft never pending publishes as before");
		expect(other.Draft.State == review_draft_state::publishing && !other.Draft.InReview, "Publish without a review is unchanged");
	}

	void test_a_submitted_review_holds_its_drafts()
	{
		const auto draft   = line_draft();
		const auto pending = accepted(draft, command(draft, kind::edit, std::optional{ mark() }), "pending");
		refused(pending, command(pending, kind::begin_publish, std::optional{ mark() }), review_draft_rejection::invalid_review_mark,
				"a review locks a draft only with its submission key");
		auto elsewhere = mark("key-1");
		elsewhere.Path = "src/other.cpp";
		refused(pending, command(pending, kind::begin_publish, std::optional{ elsewhere }), review_draft_rejection::invalid_review_mark,
				"a review locks a draft only on the file and head it is pending on");
		const auto held = accepted(pending, command(pending, kind::begin_publish, std::optional{ mark("key-1") }), "Submit locks the pending draft");
		expect(held_by_review(held.Draft) && !pending_in_review(held.Draft) && held.Draft.State == review_draft_state::publishing,
			   "the submitted review holds the draft");

		auto edit         = command(held, kind::edit);
		edit.Command.Body = "changed while submitted";
		expect(apply(held, edit).Refusal == revisioned_draft_refusal::policy && apply(held, edit).Policy == review_draft_rejection::already_publishing,
			   "a held draft cannot be edited");
		expect(apply(held, command(held, kind::discard)).Policy == review_draft_rejection::already_publishing, "a held draft cannot be discarded");
		refused(held, command(held, kind::edit, std::optional<review_pending_mark>{ }), review_draft_rejection::already_publishing,
				"Discard review cannot release a held draft");
		refused(held, command(held, kind::fail_publish), review_draft_rejection::invalid_review_mark,
				"a held draft is released only with its review's answer");
		refused(held, command(held, kind::fail_publish, std::optional{ mark("key-2") }), review_draft_rejection::invalid_review_mark,
				"releasing keeps no key");

		const auto released = accepted(held, command(held, kind::fail_publish, std::optional{ mark() }), "a refused review releases its drafts");
		expect(pending_in_review(released.Draft) && released.Draft.InReview == mark() && released.Draft.Body == draft.Draft.Body,
			   "a refused review leaves each draft pending, with its text");

		const auto published = accepted(held, command(held, kind::complete_publish), "an accepted review completes its drafts");
		expect(published.Draft.State == review_draft_state::published && !pending_in_review(published.Draft) && !held_by_review(published.Draft),
			   "a published draft is neither pending nor held");
	}

	[[nodiscard]] auto comment(const std::uint64_t aDraft, std::string aBody = "nit: scale could be constexpr.") -> review_submission_comment
	{
		return { .Draft = review_draft_id{ aDraft }, .DraftRevision = 4U, .Path = "src/renderer.cpp", .Side = diff_side::right, .Line = 434U, .Body = std::move(aBody) };
	}

	[[nodiscard]] auto review() -> review_submission_draft
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes.fill(1U);
		return { .Id            = review_submission_draft_id{ 1U },
				 .Target        = { .Host = git_host_id{ 2U }, .Repository = repository_id{ 3U }, .PullRequest = pull_request_id{ 4U }, .ExpectedHead = git_commit_oid{ git_hash_algorithm::sha256, bytes } },
				 .ActingAccount = github_account_id{ 5U } };
	}

	[[nodiscard]] auto set(const review_submission_draft& aDraft, std::vector<review_submission_comment> aComments) -> review_submission_outcome
	{
		return apply(aDraft, { .Id = aDraft.Id, .Kind = review_submission_command_kind::set_comments, .Comments = std::move(aComments) });
	}

	/// [[BPR-1345]] v002. The three verdict buttons submit directly, each its
	/// own verdict, so the core says which of them can submit now: Approve
	/// always, Comment when it has a summary or carries a line comment, and
	/// Request changes only with a summary. None while the review is not
	/// being written.
	void test_each_verdict_says_whether_it_can_submit()
	{
		using disposition = review_disposition;
		const auto all    = [](const review_submission_controls& aControls, const bool aComment, const bool aApprove, const bool aChanges) {
			return submits_as(aControls, disposition::comment) == aComment && submits_as(aControls, disposition::approve) == aApprove
				&& submits_as(aControls, disposition::request_changes) == aChanges;
		};
		const auto empty = review();
		expect(all(controls(empty), false, true, false), "with no summary and no line comment only Approve submits");
		const auto carried = set(empty, { comment(1U) });
		expect(carried.Draft && all(controls(*carried.Draft), true, true, false), "with a line comment Comment submits too; Request changes still needs a summary");
		auto summarised = *carried.Draft;
		summarised.Body = "Please fix the scale.";
		expect(all(controls(summarised), true, true, true), "with a summary every verdict submits");
		auto blank = summarised;
		blank.Body = " \n\t";
		expect(all(controls(blank), true, true, false), "a blank summary is no summary");
		auto approving        = summarised;
		approving.Disposition = disposition::approve;
		expect(all(controls(approving), true, true, true) && controls(approving).Submit, "the chosen verdict does not change what the others can do");
		const auto submitting = apply(summarised, { .Id = summarised.Id, .Kind = review_submission_command_kind::begin_submit });
		expect(submitting.Draft && all(controls(*submitting.Draft), false, false, false), "a review being submitted submits no verdict");
		const auto discarded = apply(summarised, { .Id = summarised.Id, .Kind = review_submission_command_kind::discard });
		expect(discarded.Draft && all(controls(*discarded.Draft), false, false, false), "nor does a discarded one");
	}

	void test_a_review_carries_its_comments()
	{
		expect(summary_required(review_disposition::comment, { }) && !summary_required(review_disposition::approve, { }), "the summary rule without comments is unchanged");
		const auto one = std::vector{ comment(1U) };
		expect(!summary_required(review_disposition::comment, one) && summary_required(review_disposition::request_changes, one),
			   "Comment needs no summary with a line comment; Request changes still does");

		const auto empty = review();
		expect(!controls(empty).Submit, "an empty Comment with no line comment cannot be submitted");
		expect(apply(empty, { .Id = empty.Id, .Kind = review_submission_command_kind::begin_submit }).Rejection == rejection::blank_body,
			   "GitHub refuses that review, so the core does");
		const auto carried = set(empty, one);
		expect(carried.Draft && carried.Draft->Comments == one && carried.Changed, "the review carries the pending drafts");
		expect(controls(*carried.Draft).Submit, "Comment with no summary is allowed when it holds a line comment");
		auto changes        = *carried.Draft;
		changes.Disposition = review_disposition::request_changes;
		expect(!controls(changes).Submit, "Request changes with no summary is not, comments or not");
		changes.Body = "Please fix the scale.";
		expect(controls(changes).Submit, "Request changes with a summary is");

		const auto submitting = apply(*carried.Draft, { .Id = empty.Id, .Kind = review_submission_command_kind::begin_submit });
		expect(submitting.Draft && submitting.Draft->State == review_submission_state::submitting && valid(*submitting.Draft), "it submits");
		const auto proposal = propose(*submitting.Draft);
		expect(proposal && proposal->Comments == one && proposal->Body.empty() && valid(*proposal), "the proposal carries exactly the comments");
		auto bare     = *proposal;
		bare.Comments = { };
		expect(!valid(bare), "the same proposal without its comments needs a summary");
		expect(set(*submitting.Draft, { }).Rejection == rejection::already_submitting, "Submit freezes the comments it carries");
		expect(apply(*submitting.Draft, { .Id = empty.Id, .Kind = review_submission_command_kind::fail_submit, .Attempt = submitting.Draft->Attempt }).Draft->Comments == one,
			   "a failed submission keeps them");

		const auto blank = set(empty, { comment(1U, "  \n") });
		expect(blank.Draft.has_value(), "a pending draft with no words is still listed");
		expect(!controls(*blank.Draft).Submit
				   && apply(*blank.Draft, { .Id = empty.Id, .Kind = review_submission_command_kind::begin_submit }).Rejection == rejection::blank_comment,
			   "but the review is not submitted with it");

		expect(set(empty, { comment(1U), comment(1U) }).Rejection == rejection::invalid_comment, "one draft rides once");
		auto range      = comment(2U);
		range.StartLine = 434U;
		expect(set(empty, { range }).Rejection == rejection::invalid_comment, "a range ends below its start");
		range.StartLine = 431U;
		range.Side      = diff_side::left;
		expect(set(empty, { range }).Draft.has_value(), "a base-side range is a comment");
		auto pathless = comment(3U);
		pathless.Path.clear();
		expect(set(empty, { pathless }).Rejection == rejection::invalid_comment, "a comment names its file");
		auto unrevised          = comment(3U);
		unrevised.DraftRevision = 0U;
		expect(set(empty, { unrevised }).Rejection == rejection::invalid_comment, "a comment names its draft's revision");
		auto many = std::vector<review_submission_comment>{ };
		for (auto index = std::uint64_t{ 1U }; index <= maximum_review_submission_comments + 1U; ++index) {
			many.push_back(comment(index));
		}
		expect(set(empty, many).Rejection == rejection::too_many_comments, "a review carries a bounded number of comments");
		auto large = std::vector<review_submission_comment>{ };
		for (auto index = std::uint64_t{ 1U }; index <= 4U; ++index) {
			large.push_back(comment(index, std::string(16U * 1024U, 'x')));
		}
		expect(set(empty, large).Rejection == rejection::review_too_large, "a review fits one request");
	}

	void test_submit_binds_the_comments_it_showed()
	{
		const auto shown = set(review(), { comment(1U) });
		const auto first = prepare_review_decision(*shown.Draft, { });
		expect(first.has_value() && review_decision_changes(*first, *prepare_review_decision(*shown.Draft, { })).empty(), "the same comments change nothing");
		auto edited             = *shown.Draft;
		edited.Comments[0].Body = "nit: make scale constexpr.";
		expect(review_decision_changes(*first, *prepare_review_decision(edited, { })) == std::vector{ review_preflight_change::comments },
			   "an edited pending comment refuses the Submit that showed the old one");
		auto added = *shown.Draft;
		added.Comments.push_back(comment(2U));
		expect(review_decision_changes(*first, *prepare_review_decision(added, { })) == std::vector{ review_preflight_change::comments },
			   "a comment added since refuses it too");
		auto moved                  = *shown.Draft;
		moved.Comments[0].StartLine = 433U;
		expect(review_decision_changes(*first, *prepare_review_decision(moved, { })) == std::vector{ review_preflight_change::comments },
			   "so does a changed range");
		expect(stable_name(review_preflight_change::comments) == "comments" && name(review_draft_rejection::invalid_review_mark) == "invalid_review_mark",
			   "the new values name themselves");
	}
}

int main()
{
	test_start_a_review_marks_a_draft();
	test_discard_review_and_add_single_comment();
	test_a_submitted_review_holds_its_drafts();
	test_a_review_carries_its_comments();
	test_each_verdict_says_whether_it_can_submit();
	test_submit_binds_the_comments_it_showed();
	std::cout << "core_review_pending: ok\n";
	return EXIT_SUCCESS;
}
