/// \file
/// \brief Proves a finding anchors to frozen identities, or refuses without guessing.

#include "bpr/core/agent_finding.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
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

	[[nodiscard]] auto relation() -> core::move_relation_id
	{
		return identity<core::move_relation_id>(8U);
	}

	constexpr std::string_view analyzedPath = "src/retry.cpp";

	[[nodiscard]] auto binding() -> core::agent_revision_binding
	{
		return {
			.Host        = core::git_host_id{ 10U },
			.Repository  = core::repository_id{ 20U },
			.PullRequest = core::pull_request_id{ 30U },
			.BaseOid     = commit(1U),
			.HeadOid     = commit(2U),
			.LeftEpoch   = core::epoch_ordinal{ 1U },
			.RightEpoch  = core::epoch_ordinal{ 2U },
		};
	}

	/// \brief One file frozen with a short left side and a longer right side.
	[[nodiscard]] auto snapshot() -> core::agent_finding_snapshot
	{
		return {
			.Binding = binding(),
			.Files   = {
				{
					.Path           = std::string{ analyzedPath },
					.Comparison     = comparison(),
					.LeftLineCount  = 40U,
					.RightLineCount = 42U,
					.Segments       = { segment() },
					.Groups         = { group() },
					.Moves          = { relation() },
				},
			},
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
				.Line = core::source_line_ref{
					.Comparison = comparison(),
					.Side       = core::diff_side::right,
					.Line       = 17U,
				},
				.Segment = segment(),
				.Group   = group(),
			},
			.Claim            = "the retry loop never terminates while the server keeps answering 503",
			.Evidence         = "the delay is recomputed but the attempt counter is not incremented",
			.Consequence      = "a single unhealthy endpoint holds the review thread open indefinitely",
			.SuggestedComment = std::string{ "Should this increment `attempts` before sleeping?" },
			.RequiredContext  = "the caller of retry_until, which this session did not read",
			.AnalyzedBaseOid  = commit(1U),
			.AnalyzedHeadOid  = commit(2U),
			.AnalyzedCommits  = { commit(1U), commit(2U) },
		};
	}

	/// \brief One mutation of the fixture, and the refusal it must produce.
	struct anchor_case
	{
		core::agent_finding_refusal Expected = core::agent_finding_refusal::none;
		std::string_view            Message;
		void (*Mutate)(core::agent_finding&, core::agent_finding_snapshot&) = nullptr;
	};

	/// \brief Applies each mutation to a fresh fixture and holds it to its reason.
	void expect_refusals(const std::span<const anchor_case> aCases)
	{
		for (const auto& item : aCases) {
			auto subject = finding();
			auto frozen  = snapshot();
			item.Mutate(subject, frozen);
			const auto verdict = core::validate_agent_finding(subject, frozen);
			expect(
				verdict.State == core::agent_finding_anchor_state::unanchored
					&& verdict.Refusal == item.Expected,
				item.Message);
			// Every refusal is still revision-bound, and a refusal always names
			// a reason: a state and reason that could disagree would let a
			// caller read "unanchored, no reason given" as permission to guess.
			expect(
				verdict.AnalyzedBaseOid == subject.AnalyzedBaseOid
					&& verdict.AnalyzedHeadOid == subject.AnalyzedHeadOid
					&& verdict.Refusal != core::agent_finding_refusal::none
					&& !verdict.anchored(),
				item.Message);
		}
	}

	/// \brief A finding whose parts are all frozen anchors, and only then.
	void test_a_frozen_anchor_validates_as_anchored()
	{
		const auto verdict = core::validate_agent_finding(finding(), snapshot());
		expect(
			verdict.anchored()
				&& verdict.Refusal == core::agent_finding_refusal::none,
			"a finding whose path, range, line, segment, and group are all in "
			"the frozen snapshot anchors");
		expect(
			verdict.AnalyzedBaseOid == commit(1U)
				&& verdict.AnalyzedHeadOid == commit(2U),
			"and an anchored verdict still carries the revisions the finding was "
			"analyzed against, so anchored never means revision-free");

		// Non-vacuity: a validator that anchored everything would pass the
		// assertion above, so one changed field must be enough to lose it.
		auto elsewhere = finding();
		elsewhere.Path = "src/absent.cpp";
		expect(
			!core::validate_agent_finding(elsewhere, snapshot()).anchored(),
			"the same fixture naming a file the snapshot does not hold does not "
			"anchor, so anchoring is a decision rather than the only answer");

		auto bare   = finding();
		bare.Anchor = {
			.Line = core::source_line_ref{
				.Comparison = comparison(),
				.Side       = core::diff_side::right,
				.Line       = 17U,
			},
		};
		expect(
			core::validate_agent_finding(bare, snapshot()).anchored(),
			"a line anchor carrying no segment or group anchors, because the "
			"refinements are optional rather than required identity");

		auto seam   = finding();
		seam.Lines  = { 19U, 19U };
		seam.Anchor = {
			.Boundary = core::source_boundary_ref{
				.Comparison = comparison(),
				.Side       = core::diff_side::right,
				.Boundary   = 19U,
			},
			.Move = relation(),
		};
		expect(
			core::validate_agent_finding(seam, snapshot()).anchored(),
			"a boundary anchor with an empty range anchors, because a seam "
			"between two lines is a place a finding may legitimately name");
	}

	/// \brief A moved head makes the same anchor stale, and keeps its OIDs.
	void test_a_moved_head_makes_the_same_anchor_stale()
	{
		const auto subject = finding();
		expect(
			core::validate_agent_finding(subject, snapshot()).anchored(),
			"this exact finding anchors against the binding it was analyzed "
			"against, so the staleness below is the head movement alone");

		auto moved            = snapshot();
		moved.Binding.HeadOid = commit(9U);
		const auto verdict    = core::validate_agent_finding(subject, moved);
		expect(
			verdict.State == core::agent_finding_anchor_state::stale
				&& verdict.Refusal == core::agent_finding_refusal::none,
			"a finding analyzed against one head is stale once the head moved, "
			"and stale is its own state rather than a kind of refusal");
		expect(
			verdict.AnalyzedBaseOid == subject.AnalyzedBaseOid
				&& verdict.AnalyzedHeadOid == subject.AnalyzedHeadOid
				&& verdict.AnalyzedHeadOid != moved.Binding.HeadOid,
			"and it keeps the OIDs the finding was analyzed against, which are "
			"not the ones now frozen, so the two are distinguishable");

		auto rebased            = snapshot();
		rebased.Binding.BaseOid = commit(9U);
		expect(
			core::validate_agent_finding(subject, rebased).State
				== core::agent_finding_anchor_state::stale,
			"a moved base is the same mistake as a moved head: the comparison "
			"the finding was computed from is no longer the frozen one");

		// Staleness is decided before any snapshot content is consulted. The
		// pair below differs only in whether the head moved, so a validator
		// that measured a stale finding against the new snapshot would report
		// the path refusal instead and fail here.
		auto renamed = subject;
		renamed.Path = "src/absent.cpp";
		expect(
			core::validate_agent_finding(renamed, snapshot()).Refusal
				== core::agent_finding_refusal::unknown_path,
			"against the frozen binding, this mutation is an unknown path");
		expect(
			core::validate_agent_finding(renamed, moved).State
				== core::agent_finding_anchor_state::stale,
			"but once the head has moved the same finding is stale, because a "
			"finding from another revision is never measured against this one");

		auto oversize  = subject;
		oversize.Claim = std::string(core::maximum_finding_claim_bytes + 1U, 'c');
		expect(
			core::validate_agent_finding(oversize, moved).Refusal
				== core::agent_finding_refusal::malformed,
			"a finding that is malformed at all is malformed whichever revision "
			"it is compared against, because that verdict reads no snapshot");
	}

	/// \brief An unknown path refuses exactly, and offers nothing in its place.
	void test_an_unknown_path_is_refused_without_a_substitute()
	{
		auto subject       = finding();
		subject.Path       = "src/absent.cpp";
		const auto verdict = core::validate_agent_finding(subject, snapshot());
		expect(
			verdict
				== core::agent_finding_verdict{
					.State           = core::agent_finding_anchor_state::unanchored,
					.Refusal         = core::agent_finding_refusal::unknown_path,
					.AnalyzedBaseOid = commit(1U),
					.AnalyzedHeadOid = commit(2U),
				},
			"a path absent from the frozen snapshot is unanchored, and the whole "
			"verdict is that refusal plus the finding's own revisions - there is "
			"no nearest file and no substitute line anywhere in it");

		auto prefixed = finding();
		prefixed.Path = std::string{ analyzedPath } + "x";
		expect(
			core::validate_agent_finding(prefixed, snapshot()).Refusal
				== core::agent_finding_refusal::unknown_path,
			"a path that merely shares a prefix with a frozen one is unknown, "
			"because paths are matched exactly rather than approximately");

		expect(
			core::validate_agent_finding(finding(), snapshot()).anchored(),
			"the same fixture with its original path anchors, so the refusals "
			"above are about the path and not about the snapshot being empty");
	}

	/// \brief A range past the end refuses, on the side that is actually short.
	void test_a_span_past_the_end_is_refused_without_a_substitute()
	{
		auto subject              = finding();
		subject.Lines             = { 41U, 44U };
		subject.Anchor.Line->Line = 43U;
		const auto verdict        = core::validate_agent_finding(subject, snapshot());
		expect(
			verdict
				== core::agent_finding_verdict{
					.State           = core::agent_finding_anchor_state::unanchored,
					.Refusal         = core::agent_finding_refusal::span_past_end,
					.AnalyzedBaseOid = commit(1U),
					.AnalyzedHeadOid = commit(2U),
				},
			"a range running past the end of the frozen file is unanchored, and "
			"the whole verdict holds no line the validator picked instead");

		// The frozen left side is 40 lines and the right is 42. The pair below
		// differs only in which side the anchor names, so a validator that
		// measured against either count, or the larger one, would fail here.
		auto onRight              = finding();
		onRight.Lines             = { 40U, 42U };
		onRight.Anchor.Line->Line = 41U;
		expect(
			core::validate_agent_finding(onRight, snapshot()).anchored(),
			"a range ending at the last right-side line anchors, so the bound is "
			"the file's length and not a margin below it");
		auto onLeft              = onRight;
		onLeft.Anchor.Line->Side = core::diff_side::left;
		expect(
			core::validate_agent_finding(onLeft, snapshot()).Refusal
				== core::agent_finding_refusal::span_past_end,
			"the identical range on the shorter left side runs past the end, so "
			"the side the anchor names selects the count it is measured by");

		auto lastSeam   = finding();
		lastSeam.Lines  = { 42U, 42U };
		lastSeam.Anchor = {
			.Boundary = core::source_boundary_ref{
				.Comparison = comparison(),
				.Side       = core::diff_side::right,
				.Boundary   = 42U,
			},
		};
		expect(
			core::validate_agent_finding(lastSeam, snapshot()).anchored(),
			"the seam after the last right-side line anchors, because a file's "
			"end is a place an insertion can be proposed");
		auto pastSeam                      = lastSeam;
		pastSeam.Lines                     = { 43U, 43U };
		pastSeam.Anchor.Boundary->Boundary = 43U;
		expect(
			core::validate_agent_finding(pastSeam, snapshot()).Refusal
				== core::agent_finding_refusal::span_past_end,
			"the seam one past that is refused, so a boundary is bounded by the "
			"frozen line count exactly as a line is");
	}

	/// \brief Every refusal reason is reachable, by every check that produces it.
	void test_every_refusal_reason_is_swept()
	{
		static_assert(
			core::agent_finding_refusal_count == 4U,
			"this sweep must cover every declared refusal reason");

		expect(
			core::validate_agent_finding(finding(), snapshot()).anchored(),
			"the unmutated fixture anchors, so every refusal below is produced "
			"by its own mutation rather than by the fixture");

		static constexpr auto cases = std::array{
			// Attribution. Confidence is deliberately not in this list.
			anchor_case{
				core::agent_finding_refusal::malformed,
				"a finding attributed to no provider is malformed",
				[](core::agent_finding& aFinding, core::agent_finding_snapshot&) {
					aFinding.Provider = core::agent_profile_id{ };
				} },
			anchor_case{
				core::agent_finding_refusal::malformed,
				"a severity outside the declared ladder is malformed",
				[](core::agent_finding& aFinding, core::agent_finding_snapshot&) {
					aFinding.Severity = static_cast<core::defect_severity>(200U);
				} },
			anchor_case{
				core::agent_finding_refusal::malformed,
				"a category outside the declared set is malformed",
				[](core::agent_finding& aFinding, core::agent_finding_snapshot&) {
					aFinding.Category = static_cast<core::defect_category>(200U);
				} },
			// Required prose.
			anchor_case{
				core::agent_finding_refusal::malformed,
				"a finding naming no path is malformed rather than unknown-path, "
				"because it never named a file to be unknown",
				[](core::agent_finding& aFinding, core::agent_finding_snapshot&) {
					aFinding.Path.clear();
				} },
			anchor_case{
				core::agent_finding_refusal::malformed,
				"a finding claiming nothing is malformed: there is no claim for "
				"an anchor to be the anchor of",
				[](core::agent_finding& aFinding, core::agent_finding_snapshot&) {
					aFinding.Claim.clear();
				} },
			// Analyzed revisions.
			anchor_case{
				core::agent_finding_refusal::malformed,
				"an unusable analyzed base is malformed rather than stale, "
				"because it names no revision to have moved from",
				[](core::agent_finding& aFinding, core::agent_finding_snapshot&) {
					aFinding.AnalyzedBaseOid = core::git_commit_oid{ };
				} },
			anchor_case{
				core::agent_finding_refusal::malformed,
				"an unusable analyzed head is malformed for the same reason",
				[](core::agent_finding& aFinding, core::agent_finding_snapshot&) {
					aFinding.AnalyzedHeadOid = core::git_commit_oid{ };
				} },
			anchor_case{
				core::agent_finding_refusal::malformed,
				"one unusable commit in the analyzed list is malformed, so the "
				"list is not accepted merely because most of it parses",
				[](core::agent_finding& aFinding, core::agent_finding_snapshot&) {
					aFinding.AnalyzedCommits.emplace_back();
				} },
			// Position.
			anchor_case{
				core::agent_finding_refusal::malformed,
				"an anchor naming no position at all is malformed",
				[](core::agent_finding& aFinding, core::agent_finding_snapshot&) {
					aFinding.Anchor.Line.reset();
				} },
			anchor_case{
				core::agent_finding_refusal::malformed,
				"an anchor naming both a line and a boundary is malformed, "
				"because a finding may not be in two places",
				[](core::agent_finding& aFinding, core::agent_finding_snapshot&) {
					aFinding.Anchor.Boundary = core::source_boundary_ref{
						.Comparison = comparison(),
						.Side       = core::diff_side::right,
						.Boundary   = 17U,
					};
				} },
			anchor_case{
				core::agent_finding_refusal::malformed,
				"an inverted range is malformed rather than past-the-end, "
				"because it is not a range of anything",
				[](core::agent_finding& aFinding, core::agent_finding_snapshot&) {
					aFinding.Lines = { 19U, 16U };
				} },
			anchor_case{
				core::agent_finding_refusal::malformed,
				"a line anchor with an empty range is malformed: an empty range "
				"contains no line, least of all the anchored one",
				[](core::agent_finding& aFinding, core::agent_finding_snapshot&) {
					aFinding.Lines = { 17U, 17U };
				} },
			anchor_case{
				core::agent_finding_refusal::malformed,
				"an anchored line before the claimed range is malformed, so a "
				"finding cannot report one place and point at another",
				[](core::agent_finding& aFinding, core::agent_finding_snapshot&) {
					aFinding.Lines = { 18U, 19U };
				} },
			anchor_case{
				core::agent_finding_refusal::malformed,
				"an anchored line at the exclusive end of the claimed range is "
				"malformed, because that line is outside it",
				[](core::agent_finding& aFinding, core::agent_finding_snapshot&) {
					aFinding.Lines = { 16U, 17U };
				} },
			anchor_case{
				core::agent_finding_refusal::malformed,
				"a line anchor carrying no comparison identity is malformed",
				[](core::agent_finding& aFinding, core::agent_finding_snapshot&) {
					aFinding.Anchor.Line->Comparison
						= core::file_comparison_digest{ };
				} },
			anchor_case{
				core::agent_finding_refusal::malformed,
				"a line anchor naming a side that does not exist is malformed",
				[](core::agent_finding& aFinding, core::agent_finding_snapshot&) {
					aFinding.Anchor.Line->Side
						= static_cast<core::diff_side>(200U);
				} },
			anchor_case{
				core::agent_finding_refusal::malformed,
				"a boundary before the claimed range is malformed",
				[](core::agent_finding& aFinding, core::agent_finding_snapshot&) {
					aFinding.Anchor.Line.reset();
					aFinding.Anchor.Boundary = core::source_boundary_ref{
						.Comparison = comparison(),
						.Side       = core::diff_side::right,
						.Boundary   = 15U,
					};
				} },
			anchor_case{
				core::agent_finding_refusal::malformed,
				"a boundary past the claimed range is malformed",
				[](core::agent_finding& aFinding, core::agent_finding_snapshot&) {
					aFinding.Anchor.Line.reset();
					aFinding.Anchor.Boundary = core::source_boundary_ref{
						.Comparison = comparison(),
						.Side       = core::diff_side::right,
						.Boundary   = 20U,
					};
				} },
			anchor_case{
				core::agent_finding_refusal::malformed,
				"a boundary anchor carrying no comparison identity is malformed",
				[](core::agent_finding& aFinding, core::agent_finding_snapshot&) {
					aFinding.Anchor.Line.reset();
					aFinding.Anchor.Boundary = core::source_boundary_ref{
						.Side     = core::diff_side::right,
						.Boundary = 17U,
					};
				} },
			anchor_case{
				core::agent_finding_refusal::malformed,
				"a boundary anchor naming a side that does not exist is malformed",
				[](core::agent_finding& aFinding, core::agent_finding_snapshot&) {
					aFinding.Anchor.Line.reset();
					aFinding.Anchor.Boundary = core::source_boundary_ref{
						.Comparison = comparison(),
						.Side       = static_cast<core::diff_side>(200U),
						.Boundary   = 17U,
					};
				} },
			// Semantic refinements, structurally then against the snapshot.
			anchor_case{
				core::agent_finding_refusal::malformed,
				"an empty segment identity is malformed",
				[](core::agent_finding& aFinding, core::agent_finding_snapshot&) {
					aFinding.Anchor.Segment = core::alignment_segment_id{ };
				} },
			anchor_case{
				core::agent_finding_refusal::malformed,
				"an empty group identity is malformed",
				[](core::agent_finding& aFinding, core::agent_finding_snapshot&) {
					aFinding.Anchor.Group = core::change_group_id{ };
				} },
			anchor_case{
				core::agent_finding_refusal::malformed,
				"an empty move-relation identity is malformed",
				[](core::agent_finding& aFinding, core::agent_finding_snapshot&) {
					aFinding.Anchor.Move = core::move_relation_id{ };
				} },
			anchor_case{
				core::agent_finding_refusal::malformed,
				"an anchor whose comparison is not the one frozen for that path "
				"is malformed, so a finding cannot carry a line identity from a "
				"different comparison of the same file",
				[](core::agent_finding& aFinding, core::agent_finding_snapshot&) {
					aFinding.Anchor.Line->Comparison
						= identity<core::file_comparison_digest>(9U);
				} },
			anchor_case{
				core::agent_finding_refusal::malformed,
				"a segment the frozen alignment does not contain is malformed",
				[](core::agent_finding& aFinding, core::agent_finding_snapshot&) {
					aFinding.Anchor.Segment
						= identity<core::alignment_segment_id>(9U);
				} },
			anchor_case{
				core::agent_finding_refusal::malformed,
				"a group the frozen alignment does not contain is malformed",
				[](core::agent_finding& aFinding, core::agent_finding_snapshot&) {
					aFinding.Anchor.Group = identity<core::change_group_id>(9U);
				} },
			anchor_case{
				core::agent_finding_refusal::malformed,
				"a move relation the frozen alignment does not contain is "
				"malformed, so a claimed move is checked rather than believed",
				[](core::agent_finding& aFinding, core::agent_finding_snapshot&) {
					aFinding.Anchor.Move = identity<core::move_relation_id>(9U);
				} },
			// The other two reasons, so the sweep covers the whole enumeration.
			anchor_case{
				core::agent_finding_refusal::unknown_path,
				"a path absent from the frozen snapshot is unanchored",
				[](core::agent_finding& aFinding, core::agent_finding_snapshot&) {
					aFinding.Path = "src/absent.cpp";
				} },
			anchor_case{
				core::agent_finding_refusal::span_past_end,
				"a range past the frozen line count is unanchored",
				[](core::agent_finding& aFinding, core::agent_finding_snapshot&) {
					aFinding.Lines             = { 41U, 44U };
					aFinding.Anchor.Line->Line = 43U;
				} },
		};
		expect_refusals(cases);
	}

	/// \brief One declared bound, admitted at its limit and refused past it.
	struct bound_case
	{
		std::size_t      Limit = 0U;
		std::string_view Message;
		void (*Fill)(
			core::agent_finding&, core::agent_finding_snapshot&, std::size_t)
			= nullptr;
	};

	/// \brief Every declared bound is exact: the limit passes, one byte past fails.
	void test_every_declared_bound_admits_its_limit_and_refuses_one_past()
	{
		static constexpr auto cases = std::array{
			bound_case{
				core::maximum_identified_path_bytes,
				"a path is admitted at the repository path bound and refused one "
				"byte past it, where no file identity could name it anyway",
				[](core::agent_finding&          aFinding,
				   core::agent_finding_snapshot& aSnapshot,
				   const std::size_t             aSize) {
					aFinding.Path.assign(aSize, 'a');
					aSnapshot.Files.front().Path = aFinding.Path;
				} },
			bound_case{
				core::maximum_finding_claim_bytes,
				"a claim is admitted at its declared bound and refused one byte "
				"past it, rather than truncated into a different claim",
				[](core::agent_finding& aFinding,
				   core::agent_finding_snapshot&,
				   const std::size_t aSize) { aFinding.Claim.assign(aSize, 'c'); } },
			bound_case{
				core::maximum_finding_evidence_bytes,
				"evidence is admitted at its declared bound and refused one byte "
				"past it",
				[](core::agent_finding& aFinding,
				   core::agent_finding_snapshot&,
				   const std::size_t aSize) {
					aFinding.Evidence.assign(aSize, 'e');
				} },
			bound_case{
				core::maximum_finding_consequence_bytes,
				"a consequence is admitted at its declared bound and refused one "
				"byte past it",
				[](core::agent_finding& aFinding,
				   core::agent_finding_snapshot&,
				   const std::size_t aSize) {
					aFinding.Consequence.assign(aSize, 'q');
				} },
			bound_case{
				core::maximum_finding_comment_bytes,
				"a suggested comment is admitted at its declared bound and "
				"refused one byte past it",
				[](core::agent_finding& aFinding,
				   core::agent_finding_snapshot&,
				   const std::size_t aSize) {
					aFinding.SuggestedComment = std::string(aSize, 's');
				} },
			bound_case{
				core::maximum_finding_required_context_bytes,
				"a statement of required context is admitted at its declared "
				"bound and refused one byte past it",
				[](core::agent_finding& aFinding,
				   core::agent_finding_snapshot&,
				   const std::size_t aSize) {
					aFinding.RequiredContext.assign(aSize, 'r');
				} },
			bound_case{
				core::maximum_finding_analyzed_oids,
				"the analyzed-OID list is admitted at its declared bound and "
				"refused one entry past it, rather than silently shortened",
				[](core::agent_finding& aFinding,
				   core::agent_finding_snapshot&,
				   const std::size_t aSize) {
					aFinding.AnalyzedCommits.assign(aSize, commit(1U));
				} },
		};

		for (const auto& item : cases) {
			auto atLimit       = finding();
			auto frozenAtLimit = snapshot();
			item.Fill(atLimit, frozenAtLimit, item.Limit);
			auto pastLimit       = finding();
			auto frozenPastLimit = snapshot();
			item.Fill(pastLimit, frozenPastLimit, item.Limit + 1U);
			expect(
				core::validate_agent_finding(atLimit, frozenAtLimit).anchored()
					&& core::validate_agent_finding(pastLimit, frozenPastLimit)
							.Refusal
						== core::agent_finding_refusal::malformed,
				item.Message);
		}
	}

	/// \brief Confidence is data: no value of it moves any verdict.
	void test_confidence_never_decides_a_verdict()
	{
		auto absent           = finding();
		absent.Path           = "src/absent.cpp";
		auto moved            = snapshot();
		moved.Binding.HeadOid = commit(9U);

		const auto anchoredBaseline = core::validate_agent_finding(finding(), snapshot());
		const auto refusedBaseline  = core::validate_agent_finding(absent, snapshot());
		const auto staleBaseline    = core::validate_agent_finding(finding(), moved);
		expect(
			anchoredBaseline.anchored()
				&& refusedBaseline.Refusal
					== core::agent_finding_refusal::unknown_path
				&& staleBaseline.State == core::agent_finding_anchor_state::stale,
			"the three baselines are genuinely different verdicts, so the "
			"equalities below are not three copies of one answer");

		static constexpr auto stated = std::array{
			core::agent_finding_confidence::unstated,
			core::agent_finding_confidence::low,
			core::agent_finding_confidence::medium,
			core::agent_finding_confidence::high,
			static_cast<core::agent_finding_confidence>(200U),
		};
		for (const auto confidence : stated) {
			auto anchoredSubject       = finding();
			anchoredSubject.Confidence = confidence;
			auto refusedSubject        = absent;
			refusedSubject.Confidence  = confidence;
			auto staleSubject          = finding();
			staleSubject.Confidence    = confidence;
			expect(
				core::validate_agent_finding(anchoredSubject, snapshot())
						== anchoredBaseline
					&& core::validate_agent_finding(refusedSubject, snapshot())
						== refusedBaseline
					&& core::validate_agent_finding(staleSubject, moved)
						== staleBaseline,
				"no stated confidence changes any verdict, including one outside "
				"the declared set, which no branch may reject either");
		}
	}

	/// \brief No field of these types can hold a coordinate or a ribbon path.
	void test_no_field_can_hold_a_coordinate_or_ribbon_path()
	{
		const auto whole              = core::agent_finding{ };
		const auto& [provider,
					 severity,
					 confidence,
					 category,
					 path,
					 lines,
					 anchor,
					 claim,
					 evidence,
					 consequence,
					 comment,
					 requiredContext,
					 analyzedBase,
					 analyzedHead,
					 analyzedCommits] = whole;
		static_assert(
			std::is_same_v<decltype(provider), const core::agent_profile_id>);
		static_assert(
			std::is_same_v<decltype(confidence), const core::agent_finding_confidence>);
		static_assert(std::is_same_v<decltype(path), const std::string>);
		static_assert(std::is_same_v<decltype(lines), const core::line_span>);
		static_assert(
			std::is_same_v<decltype(anchor), const core::agent_finding_anchor>);
		static_assert(
			std::is_same_v<decltype(comment), const std::optional<std::string>>);
		static_assert(
			std::is_same_v<decltype(analyzedCommits), const std::vector<core::git_commit_oid>>);
		expect(
			provider.value() == 0U && severity == core::defect_severity::minor
				&& confidence == core::agent_finding_confidence::unstated
				&& category == core::defect_category::correctness && path.empty()
				&& lines.empty() && !anchor.Line && claim.empty()
				&& evidence.empty() && consequence.empty() && !comment
				&& requiredContext.empty() && !analyzedBase.valid()
				&& !analyzedHead.valid() && analyzedCommits.empty(),
			"a complete finding is a classification, bounded prose, a repository "
			"path, an RFC-BPR-0004 anchor, and the analyzed revisions - there is "
			"no y-position, viewport offset, widget row, or ribbon path in it, "
			"and adding one would not compile here");

		const auto position                                  = core::agent_finding_anchor{ };
		const auto& [line, boundary, seg, grp, moveRelation] = position;
		static_assert(
			std::is_same_v<decltype(line), const std::optional<core::source_line_ref>>);
		static_assert(
			std::is_same_v<decltype(boundary), const std::optional<core::source_boundary_ref>>);
		static_assert(
			std::is_same_v<decltype(seg), const std::optional<core::alignment_segment_id>>);
		static_assert(
			std::is_same_v<decltype(grp), const std::optional<core::change_group_id>>);
		static_assert(
			std::is_same_v<decltype(moveRelation), const std::optional<core::move_relation_id>>);
		expect(
			!line && !boundary && !seg && !grp && !moveRelation,
			"an anchor is exactly one semantic position plus the frozen "
			"identities that explain it, and every part of it is absent until "
			"something states it");

		const auto refusal                                    = core::agent_finding_verdict{ };
		const auto& [state, reason, verdictBase, verdictHead] = refusal;
		static_assert(
			std::is_same_v<decltype(state), const core::agent_finding_anchor_state>);
		static_assert(
			std::is_same_v<decltype(reason), const core::agent_finding_refusal>);
		static_assert(std::is_same_v<decltype(verdictBase), const core::git_commit_oid>);
		static_assert(std::is_same_v<decltype(verdictHead), const core::git_commit_oid>);
		expect(
			state == core::agent_finding_anchor_state::unanchored
				&& reason == core::agent_finding_refusal::malformed
				&& !verdictBase.valid() && !verdictHead.valid(),
			"a verdict is a state, a reason, and the analyzed revisions - there "
			"is no field a nearest or inferred line could occupy, and an "
			"unfilled verdict already refuses");

		const auto frozen   = core::agent_finding_snapshot_file{ };
		const auto& [frozenPath,
					 frozenComparison,
					 leftCount,
					 rightCount,
					 segments,
					 groups,
					 moves] = frozen;
		static_assert(std::is_same_v<decltype(leftCount), const std::uint32_t>);
		static_assert(std::is_same_v<decltype(rightCount), const std::uint32_t>);
		static_assert(
			std::is_same_v<decltype(segments), const std::vector<core::alignment_segment_id>>);
		expect(
			frozenPath.empty() && !frozenComparison.valid() && leftCount == 0U
				&& rightCount == 0U && segments.empty() && groups.empty()
				&& moves.empty(),
			"a frozen file is a path, a comparison, two line counts, and the "
			"identities the alignment produced - the validator is given facts "
			"about the diff and nothing about a view of it");

		const auto facts            = core::agent_finding_snapshot{ };
		const auto& [pinned, files] = facts;
		static_assert(
			std::is_same_v<decltype(pinned), const core::agent_revision_binding>);
		static_assert(
			std::is_same_v<decltype(files), const std::vector<core::agent_finding_snapshot_file>>);
		expect(
			pinned == core::agent_revision_binding{ } && files.empty(),
			"and the snapshot is the session's own binding plus those files, so "
			"there is nothing in it through which a current head could arrive");
	}
} // namespace

auto main() -> int
{
	test_a_frozen_anchor_validates_as_anchored();
	test_a_moved_head_makes_the_same_anchor_stale();
	test_an_unknown_path_is_refused_without_a_substitute();
	test_a_span_past_the_end_is_refused_without_a_substitute();
	test_every_refusal_reason_is_swept();
	test_every_declared_bound_admits_its_limit_and_refuses_one_past();
	test_confidence_never_decides_a_verdict();
	test_no_field_can_hold_a_coordinate_or_ribbon_path();
	std::cout << "agent-finding-tests:pass\n";
	return EXIT_SUCCESS;
}
