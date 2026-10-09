// [[BPR-1212]]. A new pull-request comment on one line or several: the range
// GitHub takes, the refusals before any draft carries it, the draft and
// proposal that carry it. The suggestion a selection starts is
// core_suggestion_draft's.
#include "bpr/core/diff_compute.hpp"
#include "bpr/core/review_comment_range.hpp"
#include "bpr/core/review_round_trip.hpp"
#include "bpr/core/revisioned_review_draft.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		std::array<std::uint8_t, 32> bytes{ };
		bytes[0]  = aSeed;
		bytes[31] = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return Identity{ bytes };
	}

	[[nodiscard]] constexpr auto blob(const std::uint8_t aSeed) -> git_blob_oid
	{
		std::array<std::uint8_t, 32> bytes{ };
		for (std::size_t index = 0U; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + static_cast<std::uint8_t>(index));
		}
		return git_blob_oid{ git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto comparison_key(const diff_options& aOptions) -> file_comparison_key
	{
		const auto digest = options_digest(aOptions);
		expect(digest.has_value(), "test options are normalized");
		const auto scope       = opaque<pull_request_authority_scope_id>(11U);
		const auto incarnation = opaque<epoch_catalog_incarnation_id>(12U);
		const auto authority   = pull_request_authority{
			.Profile         = host_profile_id{ 10U },
			.ProfileRevision = github_profile_revision{ 20U },
			.Host            = git_host_id{ 30U },
			.HostAuthority   = opaque<host_authority_fingerprint>(13U),
			.Account         = github_account_id{ 40U },
			.Viewer          = github_viewer_actor_id{ 50U },
			.Repository      = repository_id{ 101U },
			.Cache           = opaque<repository_cache_scope_id>(14U),
			.PullRequest     = pull_request_id{ 202U },
			.Number          = pull_request_number{ 17U },
			.Scope           = scope,
		};
		return file_comparison_key{
			.Authority   = authority,
			.Repository  = authority.Repository,
			.PullRequest = authority.PullRequest,
			.LeftAnchor  = comparison_anchor_identity{ .Authority = scope, .Incarnation = incarnation, .Kind = comparison_anchor_kind::epoch_base, .Epoch = epoch_ordinal{ 1 } },
			.RightAnchor = comparison_anchor_identity{ .Authority = scope, .Incarnation = incarnation, .Kind = comparison_anchor_kind::epoch_head, .Epoch = epoch_ordinal{ 2 } },
			.LeftBlob    = blob(21U),
			.RightBlob   = blob(51U),
			.File        = opaque<file_identity>(31U),
			.Options     = *digest,
			.Algorithm   = diff_algorithm_revision{ 1U },
		};
	}

	[[nodiscard]] auto bytes_of(const std::string_view aText) -> std::vector<std::uint8_t>
	{
		return { aText.begin(), aText.end() };
	}

	/// The real engine over two texts, so the hunks are read from the change
	/// groups production publishes.
	[[nodiscard]] auto diff(const std::string_view aLeft, const std::string_view aRight) -> computed_file_diff
	{
		const auto left    = bytes_of(aLeft);
		const auto right   = bytes_of(aRight);
		const auto options = diff_options{ };
		auto       input   = diff_compute_input{ };
		input.Key          = comparison_key(options);
		input.Options      = options;
		input.LeftBytes    = std::span<const std::uint8_t>{ left };
		input.RightBytes   = std::span<const std::uint8_t>{ right };
		auto outcome       = compute_file_diff(input);
		expect(outcome.Result.has_value(), "the fixture texts compute a diff");
		return std::move(*outcome.Result);
	}

	[[nodiscard]] auto numbered(const std::uint32_t aCount, const std::uint32_t aChanged = 0xFFFF'FFFFU) -> std::string
	{
		auto text = std::string{ };
		for (std::uint32_t line = 0U; line < aCount; ++line) {
			text += line == aChanged ? "changed " : "line ";
			text += std::to_string(line);
			text += '\n';
		}
		return text;
	}

	[[nodiscard]] auto at(const computed_file_diff& aDiff, const diff_side aSide, const std::uint32_t aLine) -> source_line_ref
	{
		return { .Comparison = aDiff.Alignment.Digest, .Side = aSide, .Line = aLine };
	}

	void test_hunks_follow_githubs_three_lines_of_context()
	{
		// Forty lines; line 10 and line 30 changed: two hunks, 7..14 and 27..34.
		auto right = numbered(40U, 10U);
		right.replace(right.find("line 30\n"), 8U, "changed 30\n");
		const auto result = diff(numbered(40U), right);
		const auto hunks  = github_review_hunks(result.Alignment, diff_side::right);
		expect(hunks.size() == 2U, "two separated changes are two hunks");
		expect(hunks[0].Begin == 7U && hunks[0].End == 14U, "a change at line 10 takes lines 7 to 13");
		expect(hunks[1].Begin == 27U && hunks[1].End == 34U, "a change at line 30 takes lines 27 to 33");

		// Six unchanged lines between two changes: the contexts touch and merge.
		auto close = numbered(40U, 10U);
		close.replace(close.find("line 17\n"), 8U, "changed 17\n");
		const auto merged = github_review_hunks(diff(numbered(40U), close).Alignment, diff_side::right);
		expect(merged.size() == 1U && merged[0].Begin == 7U && merged[0].End == 21U, "six lines apart is one hunk");
		// Seven unchanged lines: two.
		auto apart = numbered(40U, 10U);
		apart.replace(apart.find("line 18\n"), 8U, "changed 18\n");
		expect(github_review_hunks(diff(numbered(40U), apart).Alignment, diff_side::right).size() == 2U, "seven lines apart is two hunks");

		// A change at the top and the bottom clamps to the document.
		auto edges = numbered(8U, 0U);
		edges.replace(edges.find("line 7\n"), 7U, "changed 7\n");
		const auto clamped = github_review_hunks(diff(numbered(8U), edges).Alignment, diff_side::right);
		expect(clamped.size() == 1U && clamped[0].Begin == 0U && clamped[0].End == 8U, "context clamps at both ends");

		// A deletion has no right lines of its own; its context still is a hunk.
		auto deleted = numbered(40U);
		deleted.erase(deleted.find("line 20\n"), 8U);
		const auto deletion = github_review_hunks(diff(numbered(40U), deleted).Alignment, diff_side::right);
		expect(deletion.size() == 1U && deletion[0].Begin == 17U && deletion[0].End == 23U, "a deletion's right context is three lines either side");

		auto binary           = result.Alignment;
		binary.Content        = diff_content_state::binary;
		binary.RightLineCount = 0U;
		binary.LeftLineCount  = 0U;
		expect(github_review_hunks(binary, diff_side::right).empty(), "a binary side has no hunks");
	}

	void test_a_selection_becomes_an_anchor_or_a_typed_refusal()
	{
		auto right = numbered(40U, 10U);
		right.replace(right.find("line 30\n"), 8U, "changed 30\n");
		const auto  result    = diff(numbered(40U), right);
		const auto& alignment = result.Alignment;

		const auto single = decide_comment_range(alignment, at(result, diff_side::right, 22U), at(result, diff_side::right, 22U));
		expect(single.Refusal == comment_range_refusal::none && single.Anchor == at(result, diff_side::right, 22U) && !single.StartLine,
			   "one line anywhere on the right is its own anchor, with no first line (off-hunk is Publish's)");

		const auto range = decide_comment_range(alignment, at(result, diff_side::right, 8U), at(result, diff_side::right, 12U));
		expect(range.Refusal == comment_range_refusal::none && range.Anchor.Line == 12U && range.StartLine == 8U,
			   "lines 8 to 12 inside one hunk anchor to 12 and start at 8");
		const auto reversed = decide_comment_range(alignment, at(result, diff_side::right, 12U), at(result, diff_side::right, 8U));
		expect(reversed == range, "the two ends may come in either order");
		const auto edge = decide_comment_range(alignment, at(result, diff_side::right, 7U), at(result, diff_side::right, 13U));
		expect(edge.Refusal == comment_range_refusal::none && edge.StartLine == 7U && edge.Anchor.Line == 13U, "the hunk's first and last context lines are inside it");

		expect(decide_comment_range(alignment, at(result, diff_side::right, 12U), at(result, diff_side::right, 28U)).Refusal == comment_range_refusal::crosses_hunk,
			   "two hunks' lines are refused as crossing a hunk");
		expect(decide_comment_range(alignment, at(result, diff_side::right, 12U), at(result, diff_side::right, 20U)).Refusal == comment_range_refusal::crosses_hunk,
			   "one end outside every hunk is refused as crossing a hunk");
		expect(decide_comment_range(alignment, at(result, diff_side::right, 18U), at(result, diff_side::right, 22U)).Refusal == comment_range_refusal::outside_hunk,
			   "a range in unchanged lines between hunks is refused as outside a hunk");
		expect(decide_comment_range(alignment, at(result, diff_side::left, 9U), at(result, diff_side::left, 9U)).Refusal == comment_range_refusal::base_side,
			   "one left line is refused without evidence that the base is numbered as GitHub's");
		expect(decide_comment_range(alignment, at(result, diff_side::left, 8U), at(result, diff_side::left, 11U)).Refusal == comment_range_refusal::base_side,
			   "a left range is refused for the base side without that evidence");
		// [[BPR-1212]]. With the host's evidence, a base line is GitHub's `side: LEFT`.
		const auto leftLine = decide_comment_range(alignment, at(result, diff_side::left, 9U), at(result, diff_side::left, 9U), true);
		expect(leftLine.Refusal == comment_range_refusal::none && leftLine.Anchor == at(result, diff_side::left, 9U) && !leftLine.StartLine,
			   "with the evidence one left line in a hunk is its own anchor on the left");
		const auto leftRange = decide_comment_range(alignment, at(result, diff_side::left, 11U), at(result, diff_side::left, 8U), true);
		expect(leftRange.Refusal == comment_range_refusal::none && leftRange.Anchor == at(result, diff_side::left, 11U) && leftRange.StartLine == 8U,
			   "with the evidence lines 8 to 11 on the left anchor to 11 and start at 8");
		expect(decide_comment_range(alignment, at(result, diff_side::left, 22U), at(result, diff_side::left, 22U), true).Refusal == comment_range_refusal::outside_hunk,
			   "one left line outside every hunk is refused: a base line has no file-comment fallback");
		expect(decide_comment_range(alignment, at(result, diff_side::left, 12U), at(result, diff_side::left, 28U), true).Refusal == comment_range_refusal::crosses_hunk,
			   "left lines in two hunks are refused as crossing a hunk");
		expect(decide_comment_range(alignment, at(result, diff_side::left, 40U), at(result, diff_side::left, 40U), true).Refusal == comment_range_refusal::outside_document,
			   "a left line past the base document is refused");
		expect(decide_comment_range(alignment, at(result, diff_side::right, 22U), at(result, diff_side::right, 22U), true) == single,
			   "the evidence changes nothing on the right");
		expect(decide_comment_range(alignment, at(result, diff_side::left, 9U), at(result, diff_side::right, 10U)).Refusal == comment_range_refusal::mixed_side,
			   "a range across sides is refused");
		auto foreign       = at(result, diff_side::right, 10U);
		foreign.Comparison = opaque<file_comparison_digest>(99U);
		expect(decide_comment_range(alignment, foreign, at(result, diff_side::right, 11U)).Refusal == comment_range_refusal::mixed_comparison
				   && decide_comment_range(alignment, foreign, foreign).Refusal == comment_range_refusal::mixed_comparison,
			   "an end from another comparison is refused");
		expect(decide_comment_range(alignment, at(result, diff_side::right, 38U), at(result, diff_side::right, 40U)).Refusal == comment_range_refusal::outside_document
				   && decide_comment_range(alignment, at(result, diff_side::right, 40U), at(result, diff_side::right, 40U)).Refusal == comment_range_refusal::outside_document,
			   "a line past the document is refused");

		for (const auto refusal : { comment_range_refusal::none, comment_range_refusal::mixed_comparison, comment_range_refusal::mixed_side, comment_range_refusal::base_side,
									comment_range_refusal::outside_document, comment_range_refusal::outside_hunk, comment_range_refusal::crosses_hunk }) {
			expect(!name(refusal).empty(), "every refusal names itself");
		}
	}

	[[nodiscard]] auto range_draft() -> revisioned_review_draft
	{
		auto draft         = revisioned_review_draft{ };
		draft.Draft.Id     = review_draft_id{ 7U };
		draft.Draft.Anchor = { .Comparison = opaque<file_comparison_digest>(5U), .Side = diff_side::right, .Line = 12U };
		draft.Draft.Body   = "Name these.";
		draft.Revision     = 3U;
		draft.Scope        = review_draft_scope_id{ 1U };
		return draft;
	}

	[[nodiscard]] auto range_command(const revisioned_review_draft& aDraft, const std::optional<std::uint32_t> aStart, std::string aBody) -> revisioned_review_draft_command
	{
		auto command             = revisioned_review_draft_command{ };
		command.Command.Id       = aDraft.Draft.Id;
		command.Command.Kind     = review_draft_command_kind::edit;
		command.Command.Body     = std::move(aBody);
		command.ExpectedRevision = aDraft.Revision;
		command.Scope            = aDraft.Scope;
		command.ChangeRange      = true;
		command.StartLine        = aStart;
		return command;
	}

	void test_a_draft_carries_its_range_to_the_proposal()
	{
		const auto draft = range_draft();
		expect(valid_comment_range(draft.Draft), "one line is a valid range");

		const auto ranged = apply(draft, range_command(draft, 8U, "```suggestion\na\n```"));
		expect(ranged.Refusal == revisioned_draft_refusal::none && ranged.Changed && ranged.Accepted.Revision == 4U
				   && ranged.Accepted.Draft.StartLine == 8U && ranged.Accepted.Draft.Anchor == draft.Draft.Anchor
				   && ranged.Accepted.Draft.Body == "```suggestion\na\n```",
			   "one edit sets the first line and the body in one revision, and the anchor stays the last line");
		const auto same = apply(ranged.Accepted, range_command(ranged.Accepted, 8U, ranged.Accepted.Draft.Body));
		expect(same.Refusal == revisioned_draft_refusal::none && !same.Changed, "setting the same range is no change");
		const auto narrowed = apply(ranged.Accepted, range_command(ranged.Accepted, std::nullopt, ranged.Accepted.Draft.Body));
		expect(narrowed.Changed && !narrowed.Accepted.Draft.StartLine, "no first line makes it one line again");

		for (const auto start : { 12U, 13U }) {
			const auto refused = apply(draft, range_command(draft, start, draft.Draft.Body));
			expect(refused.Refusal == revisioned_draft_refusal::policy && refused.Policy == review_draft_rejection::invalid_range && refused.Accepted == draft,
				   "a first line that does not precede the anchor is refused and changes nothing");
		}
		auto left                = draft;
		left.Draft.Anchor.Side   = diff_side::left;
		left.Draft.ReplyToThread = "PRRT_left";
		expect(apply(left, range_command(left, 8U, left.Draft.Body)).Policy == review_draft_rejection::invalid_range, "a reply, here on the left, takes no range");
		// [[BPR-1212]]. A new comment on the base side takes a range on that side.
		auto base              = draft;
		base.Draft.Anchor.Side = diff_side::left;
		const auto baseRanged  = apply(base, range_command(base, 8U, base.Draft.Body));
		expect(baseRanged.Refusal == revisioned_draft_refusal::none && baseRanged.Accepted.Draft.StartLine == 8U && baseRanged.Accepted.Draft.Anchor.Side == diff_side::left,
			   "a new comment on the left takes a first line on the left");
		auto reply                = draft;
		reply.Draft.ReplyToThread = "PRRT_right";
		expect(apply(reply, range_command(reply, 8U, reply.Draft.Body)).Policy == review_draft_rejection::invalid_range, "a reply on the right takes no range");

		auto both              = range_command(draft, 8U, draft.Draft.Body);
		both.ChangeReplyTarget = true;
		both.ReplyToThread     = "PRRT_x";
		expect(apply(draft, both).Refusal == revisioned_draft_refusal::invalid_command, "a range and a reply target are not changed together");
		auto stray        = range_command(draft, 8U, draft.Draft.Body);
		stray.ChangeRange = false;
		expect(apply(draft, stray).Refusal == revisioned_draft_refusal::invalid_command, "a first line without ChangeRange is refused");
		auto publish         = range_command(draft, 8U, draft.Draft.Body);
		publish.Command.Kind = review_draft_command_kind::begin_publish;
		expect(apply(draft, publish).Refusal == revisioned_draft_refusal::invalid_command, "a range changes only with an edit");

		auto answer              = revisioned_review_draft_command{ };
		answer.Command.Id        = ranged.Accepted.Draft.Id;
		answer.Command.Kind      = review_draft_command_kind::edit;
		answer.ExpectedRevision  = ranged.Accepted.Revision;
		answer.Scope             = ranged.Accepted.Scope;
		answer.ChangeReplyTarget = true;
		answer.ReplyToThread     = "PRRT_thread";
		const auto answered      = apply(ranged.Accepted, answer);
		expect(answered.Changed && answered.Accepted.Draft.ReplyToThread == "PRRT_thread" && !answered.Accepted.Draft.StartLine
				   && answered.Accepted.Draft.Body == ranged.Accepted.Draft.Body && answered.Accepted.Draft.Anchor == draft.Draft.Anchor,
			   "answering a thread keeps the text and the anchor and drops the first line");

		auto publishing = apply(ranged.Accepted.Draft, review_draft_command{ .Id = draft.Draft.Id, .Kind = review_draft_command_kind::begin_publish, .Body = { } });
		expect(publishing.Rejection == review_draft_rejection::none, "a valid range publishes");
		const auto proposal = propose_publication(publishing.Draft, review_disposition::comment);
		expect(proposal && proposal->StartLine == 8U && proposal->Anchor == draft.Draft.Anchor, "the proposal carries the range's first and last line");
		auto       single = apply(draft.Draft, review_draft_command{ .Id = draft.Draft.Id, .Kind = review_draft_command_kind::begin_publish, .Body = { } });
		const auto one    = propose_publication(single.Draft, review_disposition::comment);
		expect(one && !one->StartLine, "a single line's proposal has no first line");

		auto corrupt      = ranged.Accepted.Draft;
		corrupt.StartLine = 12U;
		expect(!valid_comment_range(corrupt), "a stored first line at the anchor is invalid");
		expect(apply(corrupt, review_draft_command{ .Id = corrupt.Id, .Kind = review_draft_command_kind::begin_publish, .Body = { } }).Rejection == review_draft_rejection::invalid_range,
			   "an invalid range never begins publishing");
		expect(name(review_draft_rejection::invalid_range) == "invalid_range", "the rejection names itself");
	}
} // namespace

int main()
{
	test_hunks_follow_githubs_three_lines_of_context();
	test_a_selection_becomes_an_anchor_or_a_typed_refusal();
	test_a_draft_carries_its_range_to_the_proposal();
	std::cout << "core_review_comment_range: ok\n";
	return EXIT_SUCCESS;
}
