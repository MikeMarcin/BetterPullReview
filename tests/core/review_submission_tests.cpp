#include "bpr/core/review_submission.hpp"
#include "bpr/core/review_submission_unknown.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>

namespace {
	using namespace bpr::core;
	using kind      = review_submission_command_kind;
	using state     = review_submission_state;
	using rejection = review_submission_rejection;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto draft() -> review_submission_draft
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes.fill(1U);
		return { .Id            = review_submission_draft_id{ 1U },
				 .Target        = { .Host = git_host_id{ 2U }, .Repository = repository_id{ 3U }, .PullRequest = pull_request_id{ 4U }, .ExpectedHead = git_commit_oid{ git_hash_algorithm::sha256, bytes } },
				 .ActingAccount = github_account_id{ 5U },
				 .Body          = "Review summary" };
	}

	[[nodiscard]] auto command(const kind aKind, const std::uint64_t aAttempt = 0U) -> review_submission_command
	{
		return { .Id = draft().Id, .Kind = aKind, .Attempt = review_submission_attempt_id{ aAttempt } };
	}

	void refused(const review_submission_draft&   aDraft,
				 const review_submission_command& aCommand, const rejection aReason)
	{
		const auto result = apply(aDraft, aCommand);
		expect(result.Rejection == aReason && !result.Changed && !result.Draft.has_value(),
			   "refusal returns no replacement draft and names its reason");
	}

	void test_decided_controls()
	{
		auto current = draft();
		current.Body.clear();
		expect(controls(current).Edit && controls(current).Discard && !controls(current).Submit, "blank comment stays editable without submit");
		current.Disposition = review_disposition::approve;
		expect(controls(current).Submit, "empty approval enables submit");
		current = *apply(current, command(kind::begin_submit)).Draft;
		expect(controls(current) == review_submission_controls{ .Reconcile = true }, "pending only reconciles");
		current = *apply(current, command(kind::complete_submit, 1U)).Draft;
		expect(controls(current) == review_submission_controls{ .NewReview = true }, "success requires explicit new draft");
		current = *apply(draft(), command(kind::discard)).Draft;
		expect(controls(current) == review_submission_controls{ .NewReview = true }, "discard is terminal");
		current.Id = { };
		expect(controls(current) == review_submission_controls{ }, "invalid identity enables nothing");
	}

	// [[BPR-1129]]. Compose review's mark: a summary or a verdict while
	// drafting, or a submission not yet settled, is kept work.
	void test_kept_drafts()
	{
		auto current = draft();
		expect(kept(current), "a drafted summary is kept");
		current.Body.clear();
		expect(!kept(current), "an empty comment draft keeps nothing");
		current.Disposition = review_disposition::approve;
		expect(kept(current), "a chosen verdict is kept even with no summary");
		current = *apply(draft(), command(kind::begin_submit)).Draft;
		expect(kept(current), "a submission in flight is kept");
		expect(!kept(*apply(current, command(kind::complete_submit, 1U)).Draft), "a submitted review keeps nothing");
		expect(!kept(*apply(draft(), command(kind::discard)).Draft), "a discarded draft keeps nothing");
	}

	void test_transitions()
	{
		const auto initial = draft();
		expect(valid(initial) && !propose(initial), "drafting has no proposal");
		auto edit         = command(kind::edit);
		edit.Body         = "Edited summary";
		const auto edited = apply(initial, edit);
		expect(edited.Changed && edited.Draft->Body == edit.Body, "edit replaces body");
		const auto sameEdit = apply(*edited.Draft, edit);
		expect(sameEdit.Rejection == rejection::none && sameEdit.Draft && !sameEdit.Changed
				   && *sameEdit.Draft == *edited.Draft,
			   "identical edit returns accepted unchanged state");
		auto choose        = command(kind::choose_disposition);
		choose.Disposition = review_disposition::request_changes;
		const auto chosen  = apply(*edited.Draft, choose);
		expect(chosen.Changed && chosen.Draft->Disposition == choose.Disposition,
			   "choice replaces disposition");
		const auto sameChoice = apply(*chosen.Draft, choose);
		expect(sameChoice.Rejection == rejection::none && sameChoice.Draft && !sameChoice.Changed
				   && *sameChoice.Draft == *chosen.Draft,
			   "identical choice returns accepted unchanged state");
		const auto begun = apply(*chosen.Draft, command(kind::begin_submit));
		expect(begun.Changed && begun.Draft->State == state::submitting
				   && begun.Draft->Attempt.value() == 1U,
			   "begin issues attempt one");
		const auto proposal = propose(*begun.Draft);
		expect(proposal && valid(*proposal) && proposal->Draft == initial.Id
				   && proposal->Attempt == begun.Draft->Attempt && proposal->Target == initial.Target
				   && proposal->ActingAccount == initial.ActingAccount && proposal->Body == edit.Body
				   && proposal->Disposition == choose.Disposition,
			   "proposal carries frozen exact semantic intent");
		const auto failed = apply(*begun.Draft, command(kind::fail_submit, 1U));
		expect(failed.Changed && failed.Draft->State == state::drafting
				   && failed.Draft->Body == edit.Body && failed.Draft->Disposition == choose.Disposition
				   && failed.Draft->Attempt.value() == 1U,
			   "definitive failure retains exact intent and counter");
		refused(*failed.Draft, command(kind::fail_submit, 1U), rejection::not_submitting);
		const auto retry = apply(*failed.Draft, command(kind::begin_submit));
		expect(retry.Changed && retry.Draft->Attempt.value() == 2U, "retry advances attempt");
		for (const auto terminal : { kind::complete_submit, kind::fail_submit }) {
			refused(*retry.Draft, command(terminal, 1U), rejection::stale_attempt);
			refused(*retry.Draft, command(terminal, 3U), rejection::stale_attempt);
			refused(*retry.Draft, command(terminal), rejection::invalid_identity);
		}
		const auto done = apply(*retry.Draft, command(kind::complete_submit, 2U));
		expect(done.Changed && done.Draft->State == state::submitted && !propose(*done.Draft),
			   "exact completion reaches terminal submitted");
		expect(done.Draft->Target == initial.Target && done.Draft->ActingAccount == initial.ActingAccount,
			   "all commands preserve creation authority");
		const auto discarded = apply(initial, command(kind::discard));
		expect(discarded.Changed && discarded.Draft->State == state::discarded && !propose(*discarded.Draft),
			   "discard is terminal and cannot propose");
		for (const auto action : { kind::edit, kind::choose_disposition, kind::discard,
								   kind::begin_submit, kind::complete_submit, kind::fail_submit }) {
			const auto terminal = action == kind::complete_submit || action == kind::fail_submit;
			const auto cmd      = command(action, terminal ? 2U : 0U);
			refused(*done.Draft, cmd, rejection::submitted_draft);
			refused(*discarded.Draft, cmd, rejection::discarded_draft);
			if (!terminal) {
				refused(*retry.Draft, cmd, rejection::already_submitting);
			} else {
				refused(initial, cmd, rejection::not_submitting);
			}
		}
	}

	void test_late_terminals_do_not_inspect_or_return_body()
	{
		auto current = draft();
		current.Body.assign(maximum_review_submission_body_bytes - 1U, 'x');
		current.Body += static_cast<char>(0xFF);
		current.Attempt = review_submission_attempt_id{ 2U };
		for (const auto terminal : { kind::complete_submit, kind::fail_submit }) {
			current.State = state::submitting;
			refused(current, command(terminal, 1U), rejection::stale_attempt);
			current.State = state::drafting;
			refused(current, command(terminal, 2U), rejection::not_submitting);
			current.State = state::submitted;
			refused(current, command(terminal, 2U), rejection::submitted_draft);
			current.State = state::discarded;
			refused(current, command(terminal, 2U), rejection::discarded_draft);
		}
	}

	void test_body_policy()
	{
		constexpr auto whitespace = std::array<std::string_view, 25>{
			"\x09", "\x0A", "\x0B", "\x0C", "\x0D", " ", "\xC2\x85", "\xC2\xA0",
			"\xE1\x9A\x80", "\xE2\x80\x80", "\xE2\x80\x81", "\xE2\x80\x82",
			"\xE2\x80\x83", "\xE2\x80\x84", "\xE2\x80\x85", "\xE2\x80\x86",
			"\xE2\x80\x87", "\xE2\x80\x88", "\xE2\x80\x89", "\xE2\x80\x8A",
			"\xE2\x80\xA8", "\xE2\x80\xA9", "\xE2\x80\xAF", "\xE2\x81\x9F", "\xE3\x80\x80"
		};
		for (const auto disposition : { review_disposition::comment, review_disposition::approve,
										review_disposition::request_changes }) {
			auto current        = draft();
			current.Disposition = disposition;
			current.Body.clear();
			for (const auto space : whitespace) {
				expect(valid(current), "blank drafts remain editable");
				const auto result = apply(current, command(kind::begin_submit));
				expect(result.Rejection == (disposition == review_disposition::approve ? rejection::none : rejection::blank_body), "empty and mixed Unicode whitespace follow disposition");
				current.Body += space;
			}
			const auto final = apply(current, command(kind::begin_submit));
			expect(final.Rejection == (disposition == review_disposition::approve ? rejection::none : rejection::blank_body), "entire White_Space set is blank");
			for (const auto text : { "\xE2\x80\x8B", "\xEF\xBB\xBF", "\xE1\xA0\x8E", "\xF4\x8F\xBF\xBF", "\xEF\xBF\xBD" }) {
				current.Body = text;
				expect(apply(current, command(kind::begin_submit)).Changed, "non-whitespace Unicode scalar is valid text");
			}
		}
		for (const auto bad : { "\x80", "\xC0\x80", "\xC1\xBF", "\xC2", "\xC2\x20",
								"\xE0\x80\x80", "\xED\xA0\x80", "\xED\xBF\xBF", "\xE2\x80",
								"\xF0\x80\x80\x80", "\xF4\x90\x80\x80", "\xF5\x80\x80\x80", "\xFF" }) {
			auto edit = command(kind::edit);
			edit.Body = std::string{ "valid prefix" } + bad;
			refused(draft(), edit, rejection::invalid_utf8);
			auto malformed = draft();
			malformed.Body = edit.Body;
			expect(!valid(malformed), "malformed initial text is refused");
			refused(malformed, command(kind::begin_submit), rejection::invalid_utf8);
		}
		auto edit = command(kind::edit);
		edit.Body.clear();
		expect(apply(draft(), edit).Changed, "edit may clear summary");
		edit.Body.assign(maximum_review_submission_body_bytes, 'x');
		const auto boundary = apply(draft(), edit);
		expect(boundary.Changed && apply(*boundary.Draft, command(kind::begin_submit)).Changed,
			   "exact 64 KiB semantic body accepted");
		edit.Body += 'x';
		refused(*boundary.Draft, edit, rejection::body_too_large);
		edit.Body.assign(maximum_review_submission_body_bytes - 2U, 'x');
		edit.Body += "\xC2\xA0";
		expect(apply(draft(), edit).Changed, "byte ceiling respects complete UTF-8 boundary");
		edit.Body.pop_back();
		refused(draft(), edit, rejection::invalid_utf8);
	}

	void test_invalid_identity_and_enums()
	{
		for (const auto mutation : std::array<void (*)(review_submission_draft&), 6>{
				 [](auto& aDraft) { aDraft.Id = { }; },
				 [](auto& aDraft) { aDraft.Target.Host = { }; },
				 [](auto& aDraft) { aDraft.Target.Repository = { }; },
				 [](auto& aDraft) { aDraft.Target.PullRequest = { }; },
				 [](auto& aDraft) { aDraft.Target.ExpectedHead = { }; },
				 [](auto& aDraft) { aDraft.ActingAccount = { }; } }) {
			auto current = draft();
			mutation(current);
			expect(!valid(current), "invalid creation identity refused");
			refused(current, command(kind::begin_submit), rejection::invalid_identity);
			current.State   = state::submitting;
			current.Attempt = review_submission_attempt_id{ 1U };
			expect(!propose(current), "invalid identity has no proposal");
		}
		auto current  = draft();
		current.State = static_cast<state>(255U);
		refused(current, command(kind::discard), rejection::invalid_state);
		expect(!valid(current) && !propose(current), "invalid state fails closed");
		current             = draft();
		current.Disposition = static_cast<review_disposition>(255U);
		refused(current, command(kind::begin_submit), rejection::invalid_disposition);
		auto invalid = command(static_cast<kind>(255U));
		refused(draft(), invalid, rejection::invalid_command);
		invalid             = command(kind::choose_disposition);
		invalid.Disposition = static_cast<review_disposition>(255U);
		refused(draft(), invalid, rejection::invalid_disposition);
		invalid    = command(kind::edit);
		invalid.Id = { };
		refused(draft(), invalid, rejection::invalid_identity);
		invalid.Id = review_submission_draft_id{ 99U };
		refused(draft(), invalid, rejection::unknown_draft);
		refused(draft(), command(kind::begin_submit, 1U), rejection::invalid_identity);
		current       = draft();
		current.State = state::submitting;
		refused(current, command(kind::complete_submit, 1U), rejection::invalid_identity);
		expect(!propose(current), "zero active attempt cannot propose");
		current         = draft();
		current.Attempt = review_submission_attempt_id{ std::numeric_limits<std::uint64_t>::max() - 1U };
		const auto last = apply(current, command(kind::begin_submit));
		expect(last.Changed && last.Draft->Attempt.value() == std::numeric_limits<std::uint64_t>::max(),
			   "last representable attempt is usable");
		const auto failed = apply(*last.Draft, command(kind::fail_submit, last.Draft->Attempt.value()));
		refused(*failed.Draft, command(kind::begin_submit), rejection::attempt_exhausted);
	}
	/// [[BPR-1540]]. Every reason has one stable name and survives the
	/// log line; the line carries enums and counts only, and nothing else
	/// is read back.
	void test_unknown_reasons()
	{
		for (std::size_t index = 0U; index < review_submission_unknown_reason_count; ++index) {
			const auto value = static_cast<review_submission_unknown_reason>(index);
			const auto named = name(value);
			expect(!named.empty() && review_submission_unknown_reason_named(named) == value, "every reason has one stable name");
			auto diagnosis             = review_submission_unknown_diagnosis{ };
			diagnosis.Reason           = value;
			diagnosis.PostAnswer       = review_submission_post_answer::error_status;
			diagnosis.PostStatusClass  = 5U;
			diagnosis.HttpStatusClass  = 4U;
			diagnosis.CommentsExpected = 3U;
			diagnosis.CommentsFound    = 2U;
			diagnosis.CommentsMatched  = 1U;
			diagnosis.CandidateReviews = 1U;
			diagnosis.PagesRead        = 2U;
			const auto line            = describe(diagnosis);
			expect(line == "review_submission=" + std::string{ named } + " post=error_status post_class=5 http_class=4 expected=3 found=2 matched=1 candidates=1 pages=2", "the log line is fixed keys, enums and counts");
			expect(parse_review_submission_unknown(line) == diagnosis, "the log line reads back exactly");
		}
		expect(name(static_cast<review_submission_unknown_reason>(review_submission_unknown_reason_count)).empty() && !review_submission_unknown_reason_named("unknown"), "nothing outside the set has a name");
		auto diagnosis = review_submission_unknown_diagnosis{ };
		expect(valid(diagnosis) && describe(diagnosis) == "review_submission=review_not_listed post=not_sent post_class=0 http_class=0 expected=0 found=0 matched=0 candidates=0 pages=0", "the default is a valid reason");
		for (const auto& invalid : std::array{
				 review_submission_unknown_diagnosis{ .Reason = static_cast<review_submission_unknown_reason>(review_submission_unknown_reason_count) },
				 review_submission_unknown_diagnosis{ .PostAnswer = static_cast<review_submission_post_answer>(4U) },
				 review_submission_unknown_diagnosis{ .HttpStatusClass = 1U },
				 review_submission_unknown_diagnosis{ .HttpStatusClass = 6U },
				 review_submission_unknown_diagnosis{ .PostStatusClass = 5U },
				 review_submission_unknown_diagnosis{ .CommentsExpected = 51U },
				 review_submission_unknown_diagnosis{ .CommentsExpected = 2U, .CommentsFound = 1U, .CommentsMatched = 2U },
			 }) {
			expect(!valid(invalid) && describe(invalid).empty(), "an out-of-range diagnosis is not valid and is never written");
		}
		const auto line = describe(diagnosis);
		for (const auto& text : std::array<std::string, 9>{ std::string{ }, line + " ", "x" + line, line + " host=example.invalid", "review_submission=review_not_listed", "review_submission=other post=not_sent post_class=0 http_class=0 expected=0 found=0 matched=0 candidates=0 pages=0",
															"review_submission=review_not_listed post=not_sent post_class=0 http_class=0 expected=00 found=0 matched=0 candidates=0 pages=0",
															"review_submission=review_not_listed post=not_sent post_class=0 http_class=0 expected=0 found=0 matched=0 candidates=0 pages=70000",
															"review_submission=review_not_listed post=not_sent http_class=0 post_class=0 expected=0 found=0 matched=0 candidates=0 pages=0" }) {
			expect(!parse_review_submission_unknown(text), "anything but the exact line is not read");
		}
	}
	/// [[BPR-1541]]. The core chooses each reason's line and the only numbers
	/// it may say: comment counts for a mismatch that carried comments, the
	/// status class 4 or 5 for an HTTP status; nothing else reaches the page.
	void test_unknown_notices()
	{
		for (std::size_t index = 0U; index < review_submission_unknown_reason_count; ++index) {
			const auto reason              = static_cast<review_submission_unknown_reason>(index);
			auto       diagnosis           = review_submission_unknown_diagnosis{ .Reason = reason };
			diagnosis.PostAnswer           = review_submission_post_answer::error_status;
			diagnosis.PostStatusClass      = 5U;
			diagnosis.HttpStatusClass      = 4U;
			diagnosis.CommentsExpected     = 3U;
			diagnosis.CommentsFound        = 3U;
			diagnosis.CommentsMatched      = 2U;
			diagnosis.CandidateReviews     = 1U;
			diagnosis.PagesRead            = 2U;
			const auto line                = notice(diagnosis);
			const auto counted             = reason == review_submission_unknown_reason::comments_mismatch;
			const auto classed             = reason == review_submission_unknown_reason::http_status;
			expect(line.Reason == reason && (line.Count != review_submission_unknown_count::none) == counted && (line.StatusClass != 0U) == classed,
				   "each reason keeps its line and only its own numbers");
		}
		auto mismatch             = review_submission_unknown_diagnosis{ .Reason = review_submission_unknown_reason::comments_mismatch, .CommentsExpected = 3U, .CommentsFound = 3U, .CommentsMatched = 2U };
		auto line                 = notice(mismatch);
		expect(line.Count == review_submission_unknown_count::matched_of_carried && line.Value == 2U && line.Of == 3U, "as many held as carried: how many matched");
		mismatch.CommentsFound    = 2U;
		mismatch.CommentsMatched  = 0U;
		line                      = notice(mismatch);
		expect(line.Count == review_submission_unknown_count::held_of_carried && line.Value == 2U && line.Of == 3U, "another number held: how many it holds");
		mismatch.CommentsExpected = 0U;
		mismatch.CommentsFound    = 0U;
		expect(notice(mismatch).Count == review_submission_unknown_count::none, "a review that carried no comment says no count");
		for (const auto status : std::array<std::uint8_t, 3>{ 0U, 2U, 3U }) {
			expect(notice(review_submission_unknown_diagnosis{ .Reason = review_submission_unknown_reason::http_status, .HttpStatusClass = status }).StatusClass == 0U, "only a refusal or a server error is told apart");
		}
	}
} // namespace

int main()
{
	test_unknown_reasons();
	test_unknown_notices();
	test_decided_controls();
	test_kept_drafts();
	test_transitions();
	test_body_policy();
	test_late_terminals_do_not_inspect_or_return_body();
	test_invalid_identity_and_enums();
	return EXIT_SUCCESS;
}
