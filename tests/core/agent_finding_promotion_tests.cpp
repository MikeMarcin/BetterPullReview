/// \file
/// \brief Proves a finding becomes one local artifact, or none, and never a publication.

#include "bpr/core/agent_finding_promotion.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
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

	template <typename Identity>
	[[nodiscard]] constexpr auto identity(const std::uint8_t aSeed) -> Identity
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		bytes.fill(aSeed);
		return Identity{ bytes };
	}

	[[nodiscard]] auto commit(const std::uint8_t aSeed) -> core::git_commit_oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		for (auto index = std::size_t{ 0U }; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + index);
		}
		return core::git_commit_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto comparison() -> core::file_comparison_digest
	{
		return identity<core::file_comparison_digest>(5U);
	}

	[[nodiscard]] auto segment() -> core::alignment_segment_id
	{
		return identity<core::alignment_segment_id>(6U);
	}

	[[nodiscard]] auto group() -> core::change_group_id
	{
		return identity<core::change_group_id>(7U);
	}

	constexpr std::string_view analyzedPath = "src/retry.cpp";

	/// \brief The suggestion the agent made, with bytes a rewriter would tidy.
	///
	/// \details Leading spaces, a trailing blank line, and an embedded NUL. A
	/// promotion that trimmed, re-wrapped, or copied through a C string would
	/// produce something that still reads correctly and is not what the agent
	/// wrote.
	[[nodiscard]] auto suggestion() -> std::string
	{
		using namespace std::string_view_literals;
		return std::string{ "  Should this increment `attempts`\0 before sleeping?\n\n"sv };
	}

	[[nodiscard]] auto snapshot() -> core::agent_finding_snapshot
	{
		return {
			.Binding = {
				.Host        = core::git_host_id{ 10U },
				.Repository  = core::repository_id{ 20U },
				.PullRequest = core::pull_request_id{ 30U },
				.BaseOid     = commit(1U),
				.HeadOid     = commit(2U),
				.LeftEpoch   = core::epoch_ordinal{ 1U },
				.RightEpoch  = core::epoch_ordinal{ 2U },
			},
			.Files = {
				{
					.Path           = std::string{ analyzedPath },
					.Comparison     = comparison(),
					.LeftLineCount  = 40U,
					.RightLineCount = 42U,
					.Segments       = { segment() },
					.Groups         = { group() },
				},
			},
		};
	}

	/// \brief The one line the whole fixture is about.
	[[nodiscard]] auto anchored_line() -> core::source_line_ref
	{
		return {
			.Comparison = comparison(),
			.Side       = core::diff_side::right,
			.Line       = 17U,
		};
	}

	/// \brief A finding whose every part is present in the frozen snapshot.
	[[nodiscard]] auto finding() -> core::agent_finding
	{
		return {
			.Provider   = core::claude_stream_json_profile_id,
			.Severity   = core::defect_severity::major,
			.Confidence = core::agent_finding_confidence::medium,
			.Category   = core::defect_category::correctness,
			.Path       = std::string{ analyzedPath },
			.Lines      = { 16U, 19U },
			.Anchor     = {
				.Line    = anchored_line(),
				.Segment = segment(),
				.Group   = group(),
			},
			.Claim            = "the retry loop never terminates while the server keeps answering 503",
			.Evidence         = "the delay is recomputed but the attempt counter is not incremented",
			.Consequence      = "a single unhealthy endpoint holds the review thread open indefinitely",
			.SuggestedComment = suggestion(),
			.RequiredContext  = "the caller of retry_until, which this session did not read",
			.AnalyzedBaseOid  = commit(1U),
			.AnalyzedHeadOid  = commit(2U),
			.AnalyzedCommits  = { commit(1U), commit(2U) },
		};
	}

	/// \brief The verdict BPR-0497 actually reaches about that finding.
	///
	/// \details Computed rather than written out. A hand-built verdict would let
	/// these cases agree with a validator that no longer exists.
	[[nodiscard]] auto verdict_for(const core::agent_finding& aFinding)
		-> core::agent_finding_verdict
	{
		return core::validate_agent_finding(aFinding, snapshot());
	}

	/// \brief The reviewer's own draft, already anchored where the finding is.
	[[nodiscard]] auto draft() -> core::review_comment_draft
	{
		return {
			.Id     = core::review_draft_id{ 77U },
			.Anchor = anchored_line(),
			.Body   = "the sentence the reviewer had already written",
			.State  = core::review_draft_state::drafting,
		};
	}

	/// \brief The reviewer's own defect, classified differently on purpose.
	///
	/// \details Its severity and category are not the finding's, so a promotion
	/// that carried neither across would leave this record unchanged and fail.
	[[nodiscard]] auto defect() -> core::defect_record
	{
		return {
			.Id       = core::defect_id{ 88U },
			.Anchor   = anchored_line(),
			.Severity = core::defect_severity::trivial,
			.Category = core::defect_category::style,
			.State    = core::defect_state::open,
		};
	}

	/// \brief A complete request naming every target, so the choice is the only variable.
	[[nodiscard]] auto request(const core::agent_finding_promotion_choice aChoice)
		-> core::agent_finding_promotion_request
	{
		auto subject = finding();
		return {
			.Finding  = subject,
			.Verdict  = verdict_for(subject),
			.Choice   = aChoice,
			.Standing = core::agent_finding_standing::delivered,
			.Draft    = draft(),
			.Defect   = defect(),
		};
	}

	/// \brief Re-derives the verdict after a mutation changed the finding.
	void revalidate(core::agent_finding_promotion_request& aRequest)
	{
		aRequest.Verdict = verdict_for(aRequest.Finding);
	}

	/// \brief Asserts a result carries one named artifact and nothing else.
	void expect_single(
		const core::agent_finding_promotion& aResult,
		const std::string_view               aMessage)
	{
		expect(
			aResult.command_count() == 1U && aResult.promoted()
				&& aResult.Refusal == core::agent_finding_promotion_refusal::none,
			aMessage);
	}

	/// \brief Promoting into a draft emits one edit carrying the agent's words.
	void test_a_draft_promotion_yields_exactly_one_edit_command()
	{
		const auto result = core::promote_agent_finding(
			request(core::agent_finding_promotion_choice::draft_comment));
		expect_single(
			result,
			"promoting one finding into a draft comment produces exactly one "
			"command, counted rather than assumed");
		expect(
			result.Draft && !result.Defect && !result.Note,
			"and the one command is the draft command - promoting into a draft "
			"does not also open a defect or write a note");
		expect(
			result.Draft->Id == draft().Id
				&& result.Draft->Kind == core::review_draft_command_kind::edit,
			"it edits the draft the reviewer named, and edit is the only kind a "
			"promotion emits");

		// The command has to be usable by the draft machine that owns the
		// policy, and it must leave the draft where a reviewer can still read
		// it before anyone publishes anything.
		const auto outcome = core::apply(draft(), *result.Draft);
		expect(
			outcome.Rejection == core::review_draft_rejection::none
				&& outcome.Changed
				&& outcome.Draft.State == core::review_draft_state::drafting
				&& !outcome.Draft.shared(),
			"applying it changes the draft, leaves it drafting, and leaves it "
			"private - promotion moves nothing towards publication");
	}

	/// \brief The suggested comment reaches the body unchanged, byte for byte.
	void test_the_suggested_comment_is_carried_byte_for_byte()
	{
		const auto proposed = suggestion();
		const auto result   = core::promote_agent_finding(
			request(core::agent_finding_promotion_choice::draft_comment));
		expect(result.Draft.has_value(), "the fixture promotes into a draft");
		const auto& body = result.Draft->Body;
		expect(
			body.size() == proposed.size()
				&& std::ranges::equal(body, proposed),
			"the draft body is the suggested comment byte for byte, including "
			"its leading spaces, its embedded NUL, and its trailing blank line - "
			"nothing trims, re-wraps, or annotates what the agent proposed");
		expect(
			core::apply(draft(), *result.Draft).Draft.Body == proposed,
			"and the body survives the draft machine, so the reviewer's own edit "
			"is the first thing that can change it");

		// Non-vacuity: a promoter that emitted a constant body would satisfy the
		// comparison above against this one fixture.
		auto other = request(
			core::agent_finding_promotion_choice::draft_comment);
		other.Finding.SuggestedComment = std::string{ "a different sentence " };
		const auto second              = core::promote_agent_finding(other);
		expect(
			second.Draft
				&& second.Draft->Body == *other.Finding.SuggestedComment
				&& second.Draft->Body != body,
			"a different suggestion produces a different body, so the body is "
			"copied from the finding rather than written here");
	}

	/// \brief Promoting into a defect emits one reclassify, and moves no state.
	void test_a_defect_promotion_yields_exactly_one_reclassify_command()
	{
		const auto result = core::promote_agent_finding(
			request(core::agent_finding_promotion_choice::defect));
		expect_single(
			result,
			"promoting one finding into a defect produces exactly one command");
		expect(
			result.Defect && !result.Draft && !result.Note,
			"and the one command is the defect command");
		expect(
			result.Defect->Id == defect().Id
				&& result.Defect->Kind == core::defect_command_kind::reclassify
				&& result.Defect->Severity == core::defect_severity::major
				&& result.Defect->Category == core::defect_category::correctness,
			"it carries the finding's own severity and category across unmapped, "
			"which is why the finding states them in the defect vocabulary");

		const auto outcome = core::apply(defect(), *result.Defect);
		expect(
			outcome.Rejection == core::defect_rejection::none && outcome.Changed
				&& outcome.Defect.Severity == core::defect_severity::major
				&& outcome.Defect.Category == core::defect_category::correctness,
			"applying it reclassifies the reviewer's defect to what the agent "
			"said, and the fixture's own trivial/style classification proves the "
			"values travelled rather than already agreeing");
		expect(
			outcome.Defect.State == core::defect_state::open
				&& core::outstanding(outcome.Defect.State),
			"and the defect stays open: a promotion never claims a fix, verifies "
			"anything, or settles a defect on the reviewer's behalf");

		// A defect needs no suggested comment. Only the draft does, so the two
		// choices are refused for different reasons rather than in lockstep.
		auto silent = request(core::agent_finding_promotion_choice::defect);
		silent.Finding.SuggestedComment.reset();
		expect_single(
			core::promote_agent_finding(silent),
			"a finding that proposed no comment still promotes into a defect");
	}

	/// \brief Promoting into a note yields one payload, attached exactly.
	void test_a_note_promotion_yields_exactly_one_payload()
	{
		const auto result = core::promote_agent_finding(
			request(core::agent_finding_promotion_choice::private_note));
		expect_single(
			result, "promoting one finding into a private note produces exactly "
					"one payload");
		expect(
			result.Note && !result.Draft && !result.Defect,
			"and the one payload is the note - there is no store call here, and "
			"nothing else came with it");
		expect(
			result.Note->File == core::file_identity_of(analyzedPath)
				&& result.Note->Comparison == comparison(),
			"it is attached by the file identity derived from the finding's own "
			"path and the comparison its anchor belongs to, which is exactly "
			"what a durable note attachment needs");
		expect(
			result.Note->Text.size() == finding().Claim.size()
				&& std::ranges::equal(result.Note->Text, finding().Claim),
			"and its text is the claim byte for byte - the note is copied, not "
			"composed, so no evidence or consequence is appended to it");

		// A seam is a place a note may be about, and it is the one promotion a
		// boundary-anchored finding can still make.
		auto seam = request(
			core::agent_finding_promotion_choice::private_note);
		seam.Finding.Lines  = { 19U, 19U };
		seam.Finding.Anchor = {
			.Boundary = core::source_boundary_ref{
				.Comparison = comparison(),
				.Side       = core::diff_side::right,
				.Boundary   = 19U,
			},
		};
		revalidate(seam);
		const auto atSeam = core::promote_agent_finding(seam);
		expect_single(
			atSeam,
			"a boundary-anchored finding still promotes into a note, because a "
			"note is attached by comparison rather than by line");
		expect(
			atSeam.Note && atSeam.Note->Comparison == comparison(),
			"and it carries the seam's own comparison");
	}

	/// \brief Each choice produces its own artifact, and only that one.
	void test_each_choice_produces_exactly_one_artifact()
	{
		static constexpr auto choices = std::array{
			core::agent_finding_promotion_choice::draft_comment,
			core::agent_finding_promotion_choice::defect,
			core::agent_finding_promotion_choice::private_note,
		};
		static_assert(
			choices.size() == core::agent_finding_promotion_choice_count,
			"this sweep must cover every declared promotion choice");

		for (const auto choice : choices) {
			// The request names a draft and a defect every time, so nothing but
			// the choice decides which artifact appears.
			const auto result = core::promote_agent_finding(request(choice));
			expect_single(
				result,
				"every declared choice promotes the same finding into exactly "
				"one artifact, counted");
			expect(
				result.Draft.has_value()
						== (choice
							== core::agent_finding_promotion_choice::draft_comment)
					&& result.Defect.has_value()
						== (choice
							== core::agent_finding_promotion_choice::defect)
					&& result.Note.has_value()
						== (choice
							== core::agent_finding_promotion_choice::private_note),
				"and it is the artifact the reviewer chose, never a second one "
				"alongside it");
		}
	}

	/// \brief One mutation of a complete request, and the refusal it must produce.
	struct refusal_case
	{
		core::agent_finding_promotion_refusal Expected
			= core::agent_finding_promotion_refusal::none;
		core::agent_finding_promotion_choice Choice
			= core::agent_finding_promotion_choice::private_note;
		std::string_view Message;
		void (*Mutate)(core::agent_finding_promotion_request&) = nullptr;
	};

	/// \brief Applies each mutation to a fresh request and holds it to its reason.
	void expect_refusals(const std::span<const refusal_case> aCases)
	{
		auto covered
			= std::array<bool, core::agent_finding_promotion_refusal_count>{ };
		for (const auto& item : aCases) {
			auto subject = request(item.Choice);
			item.Mutate(subject);
			const auto result = core::promote_agent_finding(subject);
			expect(result.Refusal == item.Expected, item.Message);
			// A refusal is an absence of commands as well as a reason. A result
			// that named a reason and still carried a command would be read as
			// permission by anyone who checked only one of the two.
			expect(
				result.command_count() == 0U && !result.promoted()
					&& !result.Draft && !result.Defect && !result.Note,
				item.Message);
			covered[static_cast<std::size_t>(item.Expected)] = true;
		}

		for (auto reason = std::size_t{ 0U };
			 reason < core::agent_finding_promotion_refusal_count; ++reason) {
			const auto isNone = reason
				== static_cast<std::size_t>(
									core::agent_finding_promotion_refusal::none);
			expect(
				covered[reason] != isNone,
				"every declared refusal reason is produced by some mutation "
				"above, and none of them is the not-a-refusal value");
		}
	}

	/// \brief Every refusal reason is reachable, by the rule that produces it.
	void test_every_refusal_reason_is_swept()
	{
		static_assert(
			core::agent_finding_promotion_refusal_count == 11U,
			"this sweep must cover every declared refusal reason");

		static constexpr auto cases = std::array{
			// A request that is not a request.
			refusal_case{
				core::agent_finding_promotion_refusal::unknown_choice,
				core::agent_finding_promotion_choice::draft_comment,
				"a choice nobody declared names no artifact, and is refused "
				"rather than resolved to the nearest declared one",
				[](core::agent_finding_promotion_request& aRequest) {
					aRequest.Choice
						= static_cast<core::agent_finding_promotion_choice>(200U);
				} },
			// The reviewer's own decision, before the anchor is consulted.
			refusal_case{
				core::agent_finding_promotion_refusal::dismissed,
				core::agent_finding_promotion_choice::draft_comment,
				"a dismissed finding does not become a draft comment",
				[](core::agent_finding_promotion_request& aRequest) {
					aRequest.Standing = core::agent_finding_standing::dismissed;
				} },
			refusal_case{
				core::agent_finding_promotion_refusal::dismissed,
				core::agent_finding_promotion_choice::defect,
				"a dismissed finding does not become a defect either",
				[](core::agent_finding_promotion_request& aRequest) {
					aRequest.Standing = core::agent_finding_standing::dismissed;
				} },
			refusal_case{
				core::agent_finding_promotion_refusal::dismissed,
				core::agent_finding_promotion_choice::private_note,
				"nor a private note: dismissal is about the finding, not about "
				"one of the three things it could have become",
				[](core::agent_finding_promotion_request& aRequest) {
					aRequest.Standing = core::agent_finding_standing::dismissed;
				} },
			// An anchor that did not hold, whatever the reviewer chose.
			refusal_case{
				core::agent_finding_promotion_refusal::unanchored,
				core::agent_finding_promotion_choice::draft_comment,
				"an unanchored finding does not become a draft comment",
				[](core::agent_finding_promotion_request& aRequest) {
					aRequest.Finding.Path = "src/absent.cpp";
					revalidate(aRequest);
				} },
			refusal_case{
				core::agent_finding_promotion_refusal::unanchored,
				core::agent_finding_promotion_choice::defect,
				"an unanchored finding does not become a defect",
				[](core::agent_finding_promotion_request& aRequest) {
					aRequest.Finding.Path = "src/absent.cpp";
					revalidate(aRequest);
				} },
			refusal_case{
				core::agent_finding_promotion_refusal::unanchored,
				core::agent_finding_promotion_choice::private_note,
				"an unanchored finding does not become a note, so a refused "
				"anchor is not written down as though it had landed",
				[](core::agent_finding_promotion_request& aRequest) {
					aRequest.Finding.Path = "src/absent.cpp";
					revalidate(aRequest);
				} },
			// A finding from another revision, whatever the reviewer chose.
			refusal_case{
				core::agent_finding_promotion_refusal::stale,
				core::agent_finding_promotion_choice::draft_comment,
				"a stale finding does not become a draft comment",
				[](core::agent_finding_promotion_request& aRequest) {
					aRequest.Finding.AnalyzedHeadOid = commit(9U);
					revalidate(aRequest);
				} },
			refusal_case{
				core::agent_finding_promotion_refusal::stale,
				core::agent_finding_promotion_choice::defect,
				"a stale finding does not become a defect",
				[](core::agent_finding_promotion_request& aRequest) {
					aRequest.Finding.AnalyzedHeadOid = commit(9U);
					revalidate(aRequest);
				} },
			refusal_case{
				core::agent_finding_promotion_refusal::stale,
				core::agent_finding_promotion_choice::private_note,
				"a stale finding does not become a note: staleness is a fact "
				"about the finding, not about where it was headed",
				[](core::agent_finding_promotion_request& aRequest) {
					aRequest.Finding.AnalyzedHeadOid = commit(9U);
					revalidate(aRequest);
				} },
			// The seam between validation and promotion. Each of these makes
			// the same change as its stale twin above and simply does not
			// recompute the verdict, so the pair differs only in whether the
			// verdict is this finding's.
			refusal_case{
				core::agent_finding_promotion_refusal::foreign_verdict,
				core::agent_finding_promotion_choice::draft_comment,
				"a verdict computed about another finding does not license a "
				"draft, even though it says anchored",
				[](core::agent_finding_promotion_request& aRequest) {
					aRequest.Finding.AnalyzedHeadOid = commit(9U);
				} },
			refusal_case{
				core::agent_finding_promotion_refusal::foreign_verdict,
				core::agent_finding_promotion_choice::defect,
				"nor a defect",
				[](core::agent_finding_promotion_request& aRequest) {
					aRequest.Finding.AnalyzedBaseOid = commit(9U);
				} },
			refusal_case{
				core::agent_finding_promotion_refusal::foreign_verdict,
				core::agent_finding_promotion_choice::private_note,
				"nor a note",
				[](core::agent_finding_promotion_request& aRequest) {
					aRequest.Verdict.AnalyzedHeadOid = commit(9U);
				} },
			// What a promotion copies. Each of these forges an anchored verdict
			// the validator would never have returned, which is the only way to
			// reach a promotion with nothing to copy.
			refusal_case{
				core::agent_finding_promotion_refusal::malformed_finding,
				core::agent_finding_promotion_choice::private_note,
				"a finding claiming nothing writes no note, because the note "
				"would say nothing",
				[](core::agent_finding_promotion_request& aRequest) {
					aRequest.Finding.Claim.clear();
					aRequest.Verdict.State
						= core::agent_finding_anchor_state::anchored;
					aRequest.Verdict.Refusal
						= core::agent_finding_refusal::none;
				} },
			refusal_case{
				core::agent_finding_promotion_refusal::malformed_finding,
				core::agent_finding_promotion_choice::private_note,
				"a finding naming no path names no file to attach a note to",
				[](core::agent_finding_promotion_request& aRequest) {
					aRequest.Finding.Path.clear();
					aRequest.Verdict.State
						= core::agent_finding_anchor_state::anchored;
					aRequest.Verdict.Refusal
						= core::agent_finding_refusal::none;
				} },
			refusal_case{
				core::agent_finding_promotion_refusal::malformed_finding,
				core::agent_finding_promotion_choice::private_note,
				"a path beyond the length any file identity can name is refused "
				"rather than attached to a truncated one",
				[](core::agent_finding_promotion_request& aRequest) {
					aRequest.Finding.Path.assign(
						core::maximum_identified_path_bytes + 1U, 'a');
					aRequest.Verdict.State
						= core::agent_finding_anchor_state::anchored;
					aRequest.Verdict.Refusal
						= core::agent_finding_refusal::none;
				} },
			refusal_case{
				core::agent_finding_promotion_refusal::malformed_finding,
				core::agent_finding_promotion_choice::draft_comment,
				"a finding naming no position at all promotes nowhere",
				[](core::agent_finding_promotion_request& aRequest) {
					aRequest.Finding.Anchor.Line.reset();
					aRequest.Verdict.State
						= core::agent_finding_anchor_state::anchored;
					aRequest.Verdict.Refusal
						= core::agent_finding_refusal::none;
				} },
			refusal_case{
				core::agent_finding_promotion_refusal::malformed_finding,
				core::agent_finding_promotion_choice::draft_comment,
				"a finding naming a line and a seam at once promotes nowhere "
				"either, rather than promoting to whichever is read first",
				[](core::agent_finding_promotion_request& aRequest) {
					aRequest.Finding.Anchor.Boundary
						= core::source_boundary_ref{
							  .Comparison = comparison(),
							  .Side       = core::diff_side::right,
							  .Boundary   = 17U,
						  };
					aRequest.Verdict.State
						= core::agent_finding_anchor_state::anchored;
					aRequest.Verdict.Refusal
						= core::agent_finding_refusal::none;
				} },
			refusal_case{
				core::agent_finding_promotion_refusal::malformed_finding,
				core::agent_finding_promotion_choice::private_note,
				"a position carrying no comparison identity attaches to no "
				"compared material",
				[](core::agent_finding_promotion_request& aRequest) {
					aRequest.Finding.Anchor.Line->Comparison
						= core::file_comparison_digest{ };
					aRequest.Verdict.State
						= core::agent_finding_anchor_state::anchored;
					aRequest.Verdict.Refusal
						= core::agent_finding_refusal::none;
				} },
			// The target the chosen artifact needs.
			refusal_case{
				core::agent_finding_promotion_refusal::unnamed_target,
				core::agent_finding_promotion_choice::draft_comment,
				"a draft promotion that names no draft is refused rather than "
				"issued against a default identity",
				[](core::agent_finding_promotion_request& aRequest) {
					aRequest.Draft.reset();
				} },
			refusal_case{
				core::agent_finding_promotion_refusal::unnamed_target,
				core::agent_finding_promotion_choice::defect,
				"a defect promotion that names no defect is refused the same "
				"way",
				[](core::agent_finding_promotion_request& aRequest) {
					aRequest.Defect.reset();
				} },
			refusal_case{
				core::agent_finding_promotion_refusal::anchor_elsewhere,
				core::agent_finding_promotion_choice::draft_comment,
				"a draft anchored one line away is not the draft this finding "
				"is about, and the finding is not moved onto it",
				[](core::agent_finding_promotion_request& aRequest) {
					aRequest.Draft->Anchor.Line += 1U;
				} },
			refusal_case{
				core::agent_finding_promotion_refusal::anchor_elsewhere,
				core::agent_finding_promotion_choice::defect,
				"a defect anchored to the same line number on the other side is "
				"anchored elsewhere, because a side is part of the identity",
				[](core::agent_finding_promotion_request& aRequest) {
					aRequest.Defect->Anchor.Side = core::diff_side::left;
				} },
			refusal_case{
				core::agent_finding_promotion_refusal::boundary_anchor,
				core::agent_finding_promotion_choice::draft_comment,
				"a seam cannot anchor a draft comment, because the line above "
				"or below it is a line the agent did not name",
				[](core::agent_finding_promotion_request& aRequest) {
					aRequest.Finding.Lines  = { 19U, 19U };
					aRequest.Finding.Anchor = {
						.Boundary = core::source_boundary_ref{
							.Comparison = comparison(),
							.Side       = core::diff_side::right,
							.Boundary   = 19U,
						},
					};
					revalidate(aRequest);
				} },
			refusal_case{
				core::agent_finding_promotion_refusal::boundary_anchor,
				core::agent_finding_promotion_choice::defect,
				"and it cannot anchor a defect either, which is anchored by the "
				"same identity",
				[](core::agent_finding_promotion_request& aRequest) {
					aRequest.Finding.Lines  = { 19U, 19U };
					aRequest.Finding.Anchor = {
						.Boundary = core::source_boundary_ref{
							.Comparison = comparison(),
							.Side       = core::diff_side::right,
							.Boundary   = 19U,
						},
					};
					revalidate(aRequest);
				} },
			// What the draft needs and nothing else does.
			refusal_case{
				core::agent_finding_promotion_refusal::no_suggested_comment,
				core::agent_finding_promotion_choice::draft_comment,
				"a finding that proposed no comment produces no draft, because "
				"the only body available would be one this tool wrote",
				[](core::agent_finding_promotion_request& aRequest) {
					aRequest.Finding.SuggestedComment.reset();
				} },
		};

		// Non-vacuity: every mutation above starts from a request that promotes,
		// so each refusal is produced by its own mutation rather than by the
		// fixture.
		expect_single(
			core::promote_agent_finding(
				request(core::agent_finding_promotion_choice::draft_comment)),
			"the unmutated draft request promotes");
		expect_single(
			core::promote_agent_finding(
				request(core::agent_finding_promotion_choice::defect)),
			"the unmutated defect request promotes");
		expect_single(
			core::promote_agent_finding(
				request(core::agent_finding_promotion_choice::private_note)),
			"the unmutated note request promotes");

		expect_refusals(cases);
	}

	/// \brief Whether a field of the result is one of the artifacts it may carry.
	template <typename TField>
	inline constexpr bool promotion_field_permitted
		= std::is_same_v<TField, std::optional<core::review_draft_command>>
		|| std::is_same_v<TField, std::optional<core::defect_command>>
		|| std::is_same_v<TField, std::optional<core::supplement_note>>
		|| std::is_same_v<TField, core::agent_finding_promotion_refusal>;

	/// \brief No field of the result can hold a publication proposal.
	void test_no_promotion_result_can_carry_a_publication_proposal()
	{
		const auto whole                                        = core::agent_finding_promotion{ };
		const auto& [draftCommand, defectCommand, note, reason] = whole;
		static_assert(
			promotion_field_permitted<std::remove_cvref_t<decltype(draftCommand)>>);
		static_assert(
			promotion_field_permitted<std::remove_cvref_t<decltype(defectCommand)>>);
		static_assert(promotion_field_permitted<std::remove_cvref_t<decltype(note)>>);
		static_assert(promotion_field_permitted<std::remove_cvref_t<decltype(reason)>>);
		static_assert(
			!std::is_same_v<
				std::remove_cvref_t<decltype(note)>,
				std::optional<core::review_publication_proposal>>);
		expect(
			!draftCommand && !defectCommand && !note
				&& reason == core::agent_finding_promotion_refusal::unanchored,
			"a promotion result is three optional local artifacts and a reason - "
			"there is no field a review_publication_proposal could occupy, and "
			"adding one would not compile here");

		// The type-level guard says a proposal cannot be carried. This says no
		// branch can ask for one either: propose_publication accepts only a
		// draft that has begun publishing, and the only draft command a
		// promotion emits leaves the draft where it was.
		static constexpr auto choices = std::array{
			core::agent_finding_promotion_choice::draft_comment,
			core::agent_finding_promotion_choice::defect,
			core::agent_finding_promotion_choice::private_note,
		};
		for (const auto choice : choices) {
			const auto result = core::promote_agent_finding(request(choice));
			expect(
				!result.Draft
					|| result.Draft->Kind
						== core::review_draft_command_kind::edit,
				"no declared choice emits a publishing draft command");
			const auto reached = result.Draft
				? core::apply(draft(), *result.Draft).Draft
				: draft();
			expect(
				!core::propose_publication(
					 reached, core::review_disposition::comment)
					 .has_value(),
				"and the draft a promotion leaves behind cannot be proposed for "
				"publication, so publishing still needs the reviewer's own "
				"separate gesture");
		}
	}

	/// \brief A result nobody filled in already refuses, and says why.
	void test_an_unfilled_request_already_refuses()
	{
		const auto empty = core::promote_agent_finding({ });
		expect(
			empty == core::agent_finding_promotion{ },
			"promoting a request nobody filled in returns exactly the default "
			"result, so an unfilled result cannot be mistaken for one that was "
			"never computed");
		expect(
			empty.command_count() == 0U && !empty.promoted()
				&& empty.Refusal
					== core::agent_finding_promotion_refusal::unanchored,
			"and that result is a refusal naming an unanchored finding, rather "
			"than an empty or default command");
	}
} // namespace

auto main() -> int
{
	test_a_draft_promotion_yields_exactly_one_edit_command();
	test_the_suggested_comment_is_carried_byte_for_byte();
	test_a_defect_promotion_yields_exactly_one_reclassify_command();
	test_a_note_promotion_yields_exactly_one_payload();
	test_each_choice_produces_exactly_one_artifact();
	test_every_refusal_reason_is_swept();
	test_no_promotion_result_can_carry_a_publication_proposal();
	test_an_unfilled_request_already_refuses();
	std::cout << "agent-finding-promotion-tests:pass\n";
	return EXIT_SUCCESS;
}
