#include "../support/qualification_build.hpp"
#include "bpr/core/diff_compute.hpp"
#include "bpr/core/diff_compute_components.hpp"
#include "bpr/core/diff_semantics.hpp"
#include "product_diff.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr::core;
	using bpr::tests::product_file_diff;
	using bpr::tests::product_material_diff;

	/// [[BPR-1485]] The product's line matcher as the engine's primary
	/// component, for the stoppable entry points.
	[[nodiscard]] auto product_matching() -> bpr::application::diff_line_matching&
	{
		static auto matching = bpr::application::diff_line_matching{ *bpr::tests::product_line_matcher() };
		return matching;
	}

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
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

	[[nodiscard]] auto comparison_key(const diff_options& aOptions,
									  const bool          aHasLeft  = true,
									  const bool          aHasRight = true,
									  const std::uint8_t  aFileSeed = 31U)
		-> file_comparison_key
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
			.LeftAnchor  = comparison_anchor_identity{
				.Authority   = scope,
				.Incarnation = incarnation,
				.Kind        = comparison_anchor_kind::epoch_base,
				.Epoch       = epoch_ordinal{ 1 },
			},
			.RightAnchor = comparison_anchor_identity{
				.Authority   = scope,
				.Incarnation = incarnation,
				.Kind        = comparison_anchor_kind::epoch_head,
				.Epoch       = epoch_ordinal{ 2 },
			},
			.LeftBlob  = aHasLeft ? std::optional{ blob(21U) } : std::nullopt,
			.RightBlob = aHasRight ? std::optional{ blob(51U) } : std::nullopt,
			.File      = opaque<file_identity>(aFileSeed),
			.Options   = *digest,
			.Algorithm = diff_algorithm_revision{ 1U },
		};
	}

	[[nodiscard]] auto copy_bytes(const std::string_view aText)
		-> std::vector<std::uint8_t>
	{
		auto result = std::vector<std::uint8_t>{ };
		result.reserve(aText.size());
		for (const auto character : aText) {
			result.push_back(static_cast<std::uint8_t>(character));
		}
		return result;
	}

	[[nodiscard]] auto is_utf8_boundary(const decoded_text_document& aDocument,
										const decoded_text_line&     aLine,
										const std::uint64_t          aOffset) -> bool
	{
		const auto extent = aLine.Content.End - aLine.Content.Begin;
		if (aOffset > extent) {
			return false;
		}
		if (aOffset == 0U || aOffset == extent) {
			return true;
		}
		const auto byte = aDocument.Bytes[static_cast<std::size_t>(aLine.Content.Begin + aOffset)];
		return (byte & 0xC0U) != 0x80U;
	}

	[[nodiscard]] auto run(const std::optional<std::string_view> aLeft,
						   const std::optional<std::string_view> aRight,
						   const diff_options&                   aOptions      = { },
						   const bool                            aHasLeftBlob  = true,
						   const bool                            aHasRightBlob = true,
						   const diff_compute_limits&            aLimits       = { },
						   const std::uint8_t                    aFileSeed     = 31U)
		-> diff_compute_outcome
	{
		auto leftStorage  = aLeft.has_value() ? std::optional{ copy_bytes(*aLeft) } : std::nullopt;
		auto rightStorage = aRight.has_value() ? std::optional{ copy_bytes(*aRight) } : std::nullopt;
		auto input        = diff_compute_input{
			.Key        = comparison_key(aOptions, aHasLeftBlob, aHasRightBlob, aFileSeed),
			.Options    = aOptions,
			.LeftBytes  = leftStorage.has_value()
				? std::optional{ std::span<const std::uint8_t>{ *leftStorage } }
				: std::nullopt,
			.RightBytes = rightStorage.has_value()
				? std::optional{ std::span<const std::uint8_t>{ *rightStorage } }
				: std::nullopt,
			.Limits     = aLimits,
		};
		return product_file_diff(input);
	}

	[[nodiscard]] auto text_result(const diff_compute_outcome& aOutcome,
								   const std::string_view      aMessage)
		-> computed_file_diff
	{
		expect(aOutcome.Reason == diff_reason_code::none, aMessage);
		expect(aOutcome.Result.has_value(), aMessage);
		expect(aOutcome.Result->Alignment.Content == diff_content_state::text,
			   aMessage);
		expect(validate_computed_file_diff(*aOutcome.Result).valid(), aMessage);
		return *aOutcome.Result;
	}

	auto expect_compute_error(const computed_file_diff&           aDiff,
							  const diff_compute_validation_error aExpected,
							  const std::string_view              aMessage) -> void
	{
		const auto result = validate_computed_file_diff(aDiff);
		expect(result.Error == aExpected, aMessage);
	}

	auto test_identical_and_empty() -> void
	{
		const auto& empty = text_result(run("", ""), "empty text computes");
		expect(empty.Alignment.Segments.empty(), "empty text has no fake segment");
		expect(empty.Alignment.LeftLineCount == 0U && empty.Alignment.RightLineCount == 0U,
			   "empty text has zero logical lines");

		const auto  first     = run("alpha\nbeta\n", "alpha\nbeta\n");
		const auto  replay    = run("alpha\nbeta\n", "alpha\nbeta\n");
		const auto& identical = text_result(first, "identical text computes");
		expect(identical.Alignment.Segments.size() == 1U && identical.Alignment.Segments.front().Kind == alignment_kind::equal,
			   "identical text is one equal segment");
		expect(first.Result == replay.Result, "identical replay is byte stable");
	}

	auto test_insertions_and_deletions() -> void
	{
		struct insertion_case
		{
			std::string_view Left;
			std::string_view Right;
			std::uint32_t    Boundary = 0U;
		};
		const auto insertionCases = std::array{
			insertion_case{ "b\nc\n", "a\nb\nc\n", 0U },
			insertion_case{ "a\nc\n", "a\nb\nc\n", 1U },
			insertion_case{ "a\nb\n", "a\nb\nc\n", 2U },
		};
		for (const auto& testCase : insertionCases) {
			const auto& result = text_result(run(testCase.Left, testCase.Right),
											 "insertion computes");
			expect(result.Alignment.Groups.size() == 1U,
				   "insertion has one canonical group");
			expect(result.Alignment.Groups.front().AddedLines == 1U,
				   "insertion group counts one added line");
			const auto segment = std::ranges::find_if(
				result.Alignment.Segments,
				[](const alignment_segment& aCandidate) { return aCandidate.Kind == alignment_kind::right_only; });
			expect(segment != result.Alignment.Segments.end() && segment->Left == line_span{ testCase.Boundary, testCase.Boundary } && segment->Right == line_span{ testCase.Boundary, testCase.Boundary + 1U },
				   "insertion preserves its empty boundary and source span");
		}
		for (const auto& testCase : insertionCases) {
			const auto& result = text_result(run(testCase.Right, testCase.Left),
											 "deletion computes");
			expect(result.Alignment.Groups.size() == 1U,
				   "deletion has one canonical group");
			expect(result.Alignment.Groups.front().DeletedLines == 1U,
				   "deletion group counts one deleted line");
			const auto segment = std::ranges::find_if(
				result.Alignment.Segments,
				[](const alignment_segment& aCandidate) { return aCandidate.Kind == alignment_kind::left_only; });
			expect(segment != result.Alignment.Segments.end() && segment->Left == line_span{ testCase.Boundary, testCase.Boundary + 1U } && segment->Right == line_span{ testCase.Boundary, testCase.Boundary },
				   "deletion preserves its source span and empty boundary");
		}
	}

	auto test_replacements_pairing_and_replay() -> void
	{
		const auto four  = std::string_view{ "item one\nitem two\nitem three\nitem four\n" };
		const auto seven = std::string_view{
			"item 1\nitem 2\nitem 3\nitem 4\nitem 5\nitem 6\nitem 7\n"
		};
		const auto& forward = text_result(run(four, seven), "4:7 computes");
		const auto& reverse = text_result(run(seven, four), "7:4 computes");
		expect(forward.Alignment.Pairs.size() == 4U && reverse.Alignment.Pairs.size() == 4U && forward.Alignment.Segments.size() == 5U && reverse.Alignment.Segments.size() == 5U,
			   "unequal replacements retain the complete refined path");
		for (std::uint32_t index = 0U; index < 4U; ++index) {
			const auto& forwardPair = forward.Alignment.Pairs[index];
			const auto& reversePair = reverse.Alignment.Pairs[index];
			expect(forwardPair.LeftLine == index && forwardPair.RightLine == index && forwardPair.Kind == line_pair_kind::similar && reversePair.LeftLine == index && reversePair.RightLine == index && reversePair.Kind == line_pair_kind::similar,
				   "unequal replacement pair coordinates and kinds are exact");
			expect(forward.Alignment.Segments[index].Left == line_span{ index, index + 1U } && forward.Alignment.Segments[index].Right == line_span{ index, index + 1U } && reverse.Alignment.Segments[index].Left == line_span{ index, index + 1U } && reverse.Alignment.Segments[index].Right == line_span{ index, index + 1U },
				   "unequal replacement anchors retain exact refined spans");
		}
		expect(forward.Alignment.Segments.back().Kind == alignment_kind::right_only && forward.Alignment.Segments.back().Left == line_span{ 4U, 4U } && forward.Alignment.Segments.back().Right == line_span{ 4U, 7U } && reverse.Alignment.Segments.back().Kind == alignment_kind::left_only && reverse.Alignment.Segments.back().Left == line_span{ 4U, 7U } && reverse.Alignment.Segments.back().Right == line_span{ 4U, 4U },
			   "unequal replacement tails remain explicit one-sided spans");

		const auto anchored = text_result(run("old alpha\nfixed anchor\nold omega\n",
											  "new alpha\nfixed anchor\nnew omega\n"),
										  "internal exact anchor computes");
		expect(anchored.Alignment.Segments.size() >= 3U,
			   "primary exact internal anchor refines the path");
		expect(anchored.Alignment.Segments[1].Kind == alignment_kind::equal,
			   "internal exact anchor is represented exactly");

		const auto& similarAnchors = text_result(run("alpha old\nleft gap one\nleft gap two\nomega old\n",
													 "alpha new\nright gap\nomega new\n"),
												 "internal similar anchors compute");
		expect(similarAnchors.Alignment.Pairs.size() == 2U && similarAnchors.Alignment.Pairs[0].LeftLine == 0U && similarAnchors.Alignment.Pairs[0].RightLine == 0U && similarAnchors.Alignment.Pairs[1].LeftLine == 3U && similarAnchors.Alignment.Pairs[1].RightLine == 2U,
			   "internal similar anchors refine a replacement monotonically");

		const auto  similar       = run("prefix alpha suffix\n", "prefix beta suffix\n");
		const auto  similarReplay = run("prefix alpha suffix\n", "prefix beta suffix\n");
		const auto& similarResult = text_result(similar, "similar pair computes");
		expect(similarResult.Alignment.Pairs.size() == 1U && similarResult.Alignment.Pairs.front().Kind == line_pair_kind::similar,
			   "strong similar lines pair");
		expect(!similarResult.Intraline.empty(),
			   "similar pair carries original-text intraline ranges");
		expect(similar.Result == similarReplay.Result,
			   "similar pair ties and IDs replay deterministically");

		auto strict                        = diff_options{ };
		strict.SimilarityThresholdPermille = 900U;
		const auto& weak                   = text_result(run("aaaaaaaa\n", "zzzzzzzz\n", strict),
														 "weak replacement computes");
		expect(weak.Alignment.Pairs.empty() && weak.Intraline.empty(),
			   "weak pairing remains explicitly unpaired");

		const auto repeated       = run("same prefix A\nsame prefix B\n", "same prefix B\nsame prefix A\n");
		const auto repeatedReplay = run("same prefix A\nsame prefix B\n", "same prefix B\nsame prefix A\n");
		expect(repeated.Result == repeatedReplay.Result,
			   "repeated-line tie resolution replays exactly");
		expect(
			repeated.Result->Alignment.Segments.size() == 3U && repeated.Result->Alignment.Segments[0].Kind == alignment_kind::left_only && repeated.Result->Alignment.Segments[1].Kind == alignment_kind::equal && repeated.Result->Alignment.Segments[1].Left == line_span{ 1U, 2U } && repeated.Result->Alignment.Segments[1].Right == line_span{ 0U, 1U } && repeated.Result->Alignment.Segments[2].Kind == alignment_kind::right_only,
			"repeated-line tie selects the documented earliest right anchor");
	}

	auto test_whitespace_and_endings() -> void
	{
		const auto& exact = text_result(run("value \t\n", "value\n"), "exact whitespace computes");
		expect(!exact.Alignment.Groups.empty(), "exact whitespace detects change");

		auto trailing               = diff_options{ };
		trailing.Whitespace         = whitespace_policy::ignore_trailing;
		const auto& ignoredTrailing = text_result(run("value \t\n", "value\n", trailing),
												  "trailing whitespace mode computes");
		expect(ignoredTrailing.Alignment.Groups.empty(),
			   "trailing whitespace mode matches without changing display bytes");
		expect(ignoredTrailing.LeftDocument->Bytes == copy_bytes("value \t\n"),
			   "ignored trailing whitespace remains byte-faithful for display");

		auto all               = diff_options{ };
		all.Whitespace         = whitespace_policy::ignore_all;
		const auto& ignoredAll = text_result(run("a \tb\n", "ab\n", all),
											 "all-whitespace mode computes");
		expect(ignoredAll.Alignment.Groups.empty(),
			   "all-whitespace mode removes spaces and tabs for matching only");

		const auto& exactEndings = text_result(run("a\r\nb\rc\n", "a\nb\rc\n"), "exact endings compute");
		expect(!exactEndings.Alignment.Groups.empty(),
			   "exact line-ending policy distinguishes LF and CRLF");
		expect(
			exactEndings.LeftDocument->Lines.size() == 2U && exactEndings.LeftDocument->Lines[0].Ending == text_line_ending::crlf && exactEndings.LeftDocument->Lines[1].Ending == text_line_ending::lf,
			"[[BPR-1432]] splitter ends lines only at LF and CRLF, as Git counts them");
		expect(
			exactEndings.LeftDocument->Lines[1].Content.End - exactEndings.LeftDocument->Lines[1].Content.Begin == 3U,
			"[[BPR-1432]] a lone CR stays inside its line's content");

		auto normalized               = diff_options{ };
		normalized.LineEndings        = line_ending_policy::normalize;
		const auto& normalizedEndings = text_result(
			run("a\r\n", "a\n", normalized), "normalized endings compute");
		expect(normalizedEndings.Alignment.Groups.empty(),
			   "normalized line-ending policy matches LF and CRLF");
		expect(normalizedEndings.LeftDocument->Bytes == copy_bytes("a\r\n"),
			   "line-ending normalization does not alter retained bytes");

		const auto& finalNewline = text_result(run("last\n", "last"), "missing final newline computes");
		expect(finalNewline.LeftDocument->HasFinalNewline && !finalNewline.RightDocument->HasFinalNewline,
			   "final-newline fact is preserved");
		expect(finalNewline.Alignment.LeftLineCount == 1U && finalNewline.Alignment.RightLineCount == 1U,
			   "final newline creates no phantom line");
	}

	auto test_unicode_binary_and_unavailable() -> void
	{
		const auto  left    = std::string_view{ "caf\xC3\xA9 e\xCC\x81 \xF0\x9F\x98\x80\n" };
		const auto  right   = std::string_view{ "caf\xC3\xA8 e\xCC\x81 \xF0\x9F\x98\x83\n" };
		const auto& unicode = text_result(run(left, right), "Unicode computes");
		expect(!unicode.Intraline.empty(), "Unicode similar lines have ranges");
		for (const auto& range : unicode.Intraline) {
			const auto& pair      = unicode.Alignment.Pairs.front();
			const auto  leftSize  = unicode.LeftDocument->Lines[pair.LeftLine].Content.End - unicode.LeftDocument->Lines[pair.LeftLine].Content.Begin;
			const auto  rightSize = unicode.RightDocument->Lines[pair.RightLine].Content.End - unicode.RightDocument->Lines[pair.RightLine].Content.Begin;
			expect(range.Left.Begin <= range.Left.End && range.Left.End <= leftSize && range.Right.Begin <= range.Right.End && range.Right.End <= rightSize,
				   "Unicode intraline ranges stay within original text");
			expect(
				is_utf8_boundary(*unicode.LeftDocument,
								 unicode.LeftDocument->Lines[pair.LeftLine],
								 range.Left.Begin)
					&& is_utf8_boundary(*unicode.LeftDocument,
										unicode.LeftDocument->Lines[pair.LeftLine],
										range.Left.End)
					&& is_utf8_boundary(*unicode.RightDocument,
										unicode.RightDocument->Lines[pair.RightLine],
										range.Right.Begin)
					&& is_utf8_boundary(*unicode.RightDocument,
										unicode.RightDocument->Lines[pair.RightLine],
										range.Right.End),
				"Unicode intraline endpoints are UTF-8 boundaries");
		}

		const auto malformedCases = std::array{
			std::string_view{ "\x80", 1U },
			std::string_view{ "\xE2\x82", 2U },
			std::string_view{ "\xC0\xAF", 2U },
			std::string_view{ "\xED\xA0\x80", 3U },
			std::string_view{ "\xF4\x90\x80\x80", 4U },
		};
		for (const auto malformedText : malformedCases) {
			const auto malformed = run(malformedText, "ok");
			expect(malformed.Reason == diff_reason_code::decode_failure && malformed.Result.has_value() && malformed.Result->Alignment.Segments.empty(),
				   "malformed UTF-8 publishes only decode status");
		}

		const auto binary = run(std::string_view{ "a\0b", 3U }, "text");
		expect(binary.Reason == diff_reason_code::binary_file && binary.Result.has_value() && binary.Result->Alignment.Content == diff_content_state::binary && binary.Result->Alignment.Segments.empty(),
			   "NUL classifies binary without semantic records");

		const auto missing = run(std::nullopt, "text");
		expect(missing.Reason == diff_reason_code::missing_left_blob && missing.Result.has_value() && missing.Result->Alignment.Segments.empty(),
			   "expected but unavailable material is explicit");

		const auto added = text_result(run(std::nullopt, "new\n", { }, false, true),
									   "added file computes");
		expect(!added.LeftDocument.has_value() && added.Alignment.LeftLineCount == 0U && added.Alignment.Groups.front().AddedLines == 1U,
			   "absent added-file side differs from an empty materialized blob");
		const auto deleted = text_result(
			run("old\n", std::nullopt, { }, true, false), "deleted file computes");
		expect(!deleted.RightDocument.has_value() && deleted.Alignment.RightLineCount == 0U && deleted.Alignment.Groups.front().DeletedLines == 1U,
			   "absent deleted-file side is retained exactly");

		const auto contradiction = run("invented", "real", { }, false, true);
		expect(contradiction.Reason == diff_reason_code::invalid_alignment && !contradiction.Result.has_value(),
			   "material for an absent key side is rejected without a result");
	}

	auto test_moves_identity_and_limits() -> void
	{
		auto moves        = diff_options{ };
		moves.Moves       = move_detection_policy::deterministic_exact;
		const auto& moved = text_result(run("move me\nstay\n", "stay\nmove me\n", moves),
										"move detection computes");
		expect(moved.Alignment.Moves.size() == 1U,
			   "unambiguous exact whole segment has separate move metadata");
		expect(moved.Alignment.Segments.front().Kind == alignment_kind::left_only && moved.Alignment.Segments.back().Kind == alignment_kind::right_only,
			   "move remains deletion plus insertion in monotonic primary path");
		expect(
			moved.MoveDetectionWorkConsumed != 0U
				&& !moved.MoveDetectionWorkLimitReached,
			"move discovery reports bounded indexed work");

		const auto& movedSubblock = text_result(
			run(
				"stay-a\nmove-1\nmove-2\ndeleted-neighbor\nstay-b1\nstay-b2\nstay-b3\n",
				"stay-a\nstay-b1\nstay-b2\nstay-b3\nmove-1\nmove-2\ninserted-neighbor\n",
				moves),
			"moved sub-block computes");
		expect(
			movedSubblock.Alignment.Moves.size() == 1U,
			"unique exact moved sub-block is detected beside unrelated one-sided text");
		const auto movedSource = std::ranges::find(
			movedSubblock.Alignment.Segments,
			movedSubblock.Alignment.Moves.front().SourceDeletion,
			&alignment_segment::Id);
		const auto movedDestination = std::ranges::find(
			movedSubblock.Alignment.Segments,
			movedSubblock.Alignment.Moves.front().DestinationInsertion,
			&alignment_segment::Id);
		expect(
			movedSource != movedSubblock.Alignment.Segments.end()
				&& movedDestination != movedSubblock.Alignment.Segments.end()
				&& movedSource->Kind == alignment_kind::left_only
				&& movedSource->Left == line_span{ 1U, 3U }
				&& movedSource->Right.empty()
				&& movedDestination->Kind == alignment_kind::right_only
				&& movedDestination->Right == line_span{ 4U, 6U }
				&& movedDestination->Left.empty()
				&& movedSubblock.Alignment.Segments.size() >= 5U,
			"move refinement splits only the exact sub-block and retains adjacent unrelated segments");

		auto       wrongMoveContent = moved;
		const auto destinationLine  = static_cast<std::size_t>(
			wrongMoveContent.Alignment.Segments.back().Right.Begin);
		const auto destinationByte = static_cast<std::size_t>(
			wrongMoveContent.RightDocument->Lines[destinationLine]
				.Content.Begin);
		wrongMoveContent.RightDocument->Bytes[destinationByte] = 'x';
		expect_compute_error(
			wrongMoveContent,
			diff_compute_validation_error::invalid_move_content,
			"validator rejects move metadata whose retained text is not equal");

		auto disabled      = moves;
		disabled.Moves     = move_detection_policy::disabled;
		const auto& noMove = text_result(run("move me\nstay\n", "stay\nmove me\n", disabled),
										 "disabled moves compute");
		expect(noMove.Alignment.Moves.empty(), "disabled move detection is empty");

		const auto& ambiguous = text_result(run("x\na\nb\n", "a\nx\nb\nx\n", moves),
											"ambiguous move computes");
		expect(ambiguous.Alignment.Moves.empty(),
			   "ambiguous exact moved text creates no relation");

		auto exactMoveWork = diff_compute_limits{ };
		exactMoveWork.MaximumMoveDetectionWork
			= movedSubblock.MoveDetectionWorkConsumed;
		const auto& exactMoveBoundary = text_result(
			run(
				"stay-a\nmove-1\nmove-2\ndeleted-neighbor\nstay-b1\nstay-b2\nstay-b3\n",
				"stay-a\nstay-b1\nstay-b2\nstay-b3\nmove-1\nmove-2\ninserted-neighbor\n",
				moves,
				true,
				true,
				exactMoveWork),
			"exact move-work boundary computes");
		expect(
			exactMoveBoundary.Alignment.Moves
					== movedSubblock.Alignment.Moves
				&& !exactMoveBoundary.MoveDetectionWorkLimitReached,
			"exact move-work boundary retains stable moved sub-block metadata");
		--exactMoveWork.MaximumMoveDetectionWork;
		const auto& limitedMove = text_result(
			run(
				"stay-a\nmove-1\nmove-2\ndeleted-neighbor\nstay-b1\nstay-b2\nstay-b3\n",
				"stay-a\nstay-b1\nstay-b2\nstay-b3\nmove-1\nmove-2\ninserted-neighbor\n",
				moves,
				true,
				true,
				exactMoveWork),
			"limited move-work boundary computes");
		expect(
			limitedMove.Alignment.Moves.empty()
				&& limitedMove.MoveDetectionWorkLimitReached
				&& limitedMove.MoveDetectionWorkConsumed
					<= exactMoveWork.MaximumMoveDetectionWork,
			"move-work exhaustion leaves optional metadata unrelated without failing the primary diff");

		const auto primarySubblock = text_result(
			run(
				"stay-a\nmove-1\nmove-2\ndeleted-neighbor\nstay-b1\nstay-b2\nstay-b3\n",
				"stay-a\nstay-b1\nstay-b2\nstay-b3\nmove-1\nmove-2\ninserted-neighbor\n",
				disabled),
			"unrefined move fixture computes");
		expect(
			primarySubblock.Alignment.Segments.size()
				< movedSubblock.Alignment.Segments.size(),
			"moved sub-block metadata requires bounded one-sided refinement");
		auto segmentLimited            = diff_compute_limits{ };
		segmentLimited.MaximumSegments = static_cast<std::uint32_t>(
			primarySubblock.Alignment.Segments.size());
		const auto& limitedMoveSegments = text_result(
			run(
				"stay-a\nmove-1\nmove-2\ndeleted-neighbor\nstay-b1\nstay-b2\nstay-b3\n",
				"stay-a\nstay-b1\nstay-b2\nstay-b3\nmove-1\nmove-2\ninserted-neighbor\n",
				moves,
				true,
				true,
				segmentLimited),
			"move-refinement segment boundary computes");
		expect(
			limitedMoveSegments.Alignment.Moves.empty()
				&& !limitedMoveSegments.MoveDetectionWorkLimitReached
				&& limitedMoveSegments.MoveDetectionSegmentLimitReached,
			"optional moved sub-block refinement reports its segment ceiling separately");

		auto           adversarialLeft   = std::string{ };
		auto           adversarialRight  = std::string{ };
		constexpr auto adversarialBlocks = 96U;
		for (auto block = 0U; block < adversarialBlocks; ++block) {
			adversarialLeft += "move-" + std::to_string(block) + '\n';
			adversarialLeft += "anchor-" + std::to_string(block) + '\n';
			adversarialRight += "anchor-" + std::to_string(block) + '\n';
		}
		for (auto block = adversarialBlocks; block > 0U; --block) {
			adversarialRight += "move-" + std::to_string(block - 1U) + '\n';
		}
		auto adversarialLimits                     = diff_compute_limits{ };
		adversarialLimits.MaximumMoveDetectionWork = 64U;
		const auto& boundedAdversarial             = text_result(
			run(
				adversarialLeft,
				adversarialRight,
				moves,
				true,
				true,
				adversarialLimits),
			"adversarial many-segment move fixture computes");
		expect(
			boundedAdversarial.Alignment.Segments.size() > 32U
				&& boundedAdversarial.Alignment.Moves.empty()
				&& boundedAdversarial.MoveDetectionWorkLimitReached
				&& boundedAdversarial.MoveDetectionWorkConsumed
					<= adversarialLimits.MaximumMoveDetectionWork,
			"many one-sided segments exhaust bounded move work without partial metadata");

		auto contradictoryMoveStatistics                          = movedSubblock;
		contradictoryMoveStatistics.MoveDetectionWorkLimitReached = true;
		expect_compute_error(
			contradictoryMoveStatistics,
			diff_compute_validation_error::invalid_move_statistics,
			"validator rejects move relations paired with an exhausted-work claim");

		const auto stableFile   = run("a\n", "b\n", { }, true, true, { }, 77U);
		const auto stableReplay = run("a\n", "b\n", { }, true, true, { }, 77U);
		const auto otherFile    = run("a\n", "b\n", { }, true, true, { }, 78U);
		expect(stableFile.Result == stableReplay.Result,
			   "rename-stable file identity produces stable IDs under replay");
		expect(stableFile.Result->Alignment.Digest != otherFile.Result->Alignment.Digest,
			   "stable file identity is consumed by comparison identity");

		auto noPairing                        = diff_options{ };
		noPairing.Pairing                     = pairing_policy::disabled;
		noPairing.SimilarityThresholdPermille = 0U;
		const auto baseline                   = text_result(run("a\n", "b\n", noPairing), "limit baseline computes");
		auto       exactLimits                = diff_compute_limits{ };
		exactLimits.MaximumBytesPerSide       = 2U;
		exactLimits.MaximumLinesPerSide       = 1U;
		exactLimits.MaximumMatrixCells        = baseline.MatrixCellsConsumed;
		exactLimits.MaximumSegments           = 1U;
		const auto exact                      = text_result(run("a\n", "b\n", noPairing, true, true, exactLimits),
															"exact limits compute");
		expect(exact.MatrixCellsConsumed == baseline.MatrixCellsConsumed,
			   "exact matrix limit is accepted without overflow");

		// [[BPR-1485]] The primary matches come from the line matcher, which
		// charges no matrix cells: without pairing a replacement costs none.
		expect(baseline.MatrixCellsConsumed == 0U && !baseline.AlignmentApproximate && baseline.Alignment.Segments.size() == 1U
				   && baseline.Alignment.Segments.front().Kind == alignment_kind::replacement,
			   "the primary matches charge no matrix cells and need no budget");

		auto overBytes                = exactLimits;
		overBytes.MaximumBytesPerSide = 1U;
		const auto byteLimited        = run("a\n", "b\n", noPairing, true, true, overBytes);
		expect(byteLimited.Reason == diff_reason_code::materialization_limit && byteLimited.Result.has_value() && byteLimited.Result->Alignment.Segments.empty(),
			   "over-limit materialization publishes no partial records");

		const auto pairedBaseline       = text_result(run("prefix alpha\n", "prefix beta\n"),
													  "secondary budget baseline computes");
		auto       pairedLimits         = diff_compute_limits{ };
		pairedLimits.MaximumMatrixCells = pairedBaseline.MatrixCellsConsumed;
		const auto pairedExact          = text_result(
			run("prefix alpha\n", "prefix beta\n", { }, true, true, pairedLimits),
			"secondary exact limit computes");
		expect(pairedExact.MatrixCellsConsumed == pairedBaseline.MatrixCellsConsumed,
			   "secondary score, decision, similarity, and intraline grids are "
			   "charged");
		--pairedLimits.MaximumMatrixCells;
		const auto pairedLimited = text_result(run("prefix alpha\n", "prefix beta\n", { }, true, true, pairedLimits), "one cell below secondary work still aligns");
		expect(pairedLimited.AlignmentApproximate && pairedLimited.MatrixCellsConsumed < pairedBaseline.MatrixCellsConsumed,
			   "one cell below secondary work degrades to an approximate alignment");
	}

	/// [[BPR-1485]] The engine has no primary matcher of its own: the matches
	/// come from the component, which must supply them, and the engine checks
	/// them against its own line classes.
	class declining_primary final : public bpr::core::diff_components
	{
	public:
		explicit declining_primary(const bool aOffered) noexcept
			: Offered{ aOffered }
		{
		}

		[[nodiscard]] auto parts() const noexcept -> diff_component_parts override
		{
			return { .Primary = Offered };
		}

		[[nodiscard]] auto primary_matches(const diff_primary_request& /*aRequest*/, diff_component_meter& /*aMeter*/)
			-> std::optional<std::vector<diff_match>> override
		{
			++Calls;
			return std::nullopt;
		}

		bool        Offered = false;
		std::size_t Calls   = 0U;
	};

	/// [[BPR-1485]] A final line without an ending that normalizes to nothing
	/// is still a line: the matcher must match the engine's lines one for one,
	/// or the comparison is refused.
	auto test_bpr1485_whitespace_only_final_lines() -> void
	{
		const auto equalOnly = [](const computed_file_diff& aDiff) {
			return std::ranges::all_of(aDiff.Alignment.Segments, [](const alignment_segment& aSegment) { return aSegment.Kind == alignment_kind::equal; });
		};
		for (const auto policy : { whitespace_policy::ignore_trailing, whitespace_policy::ignore_all }) {
			auto options       = diff_options{ };
			options.Whitespace = policy;
			const auto check   = [&options](const std::optional<std::string_view> aLeft, const std::optional<std::string_view> aRight, const std::string_view aMessage) {
				const auto outcome = run(aLeft, aRight, options, aLeft.has_value(), aRight.has_value());
				expect(outcome.Reason == diff_reason_code::none && outcome.Result.has_value() && validate_computed_file_diff(*outcome.Result).valid(), aMessage);
				return *outcome.Result;
			};
			static_cast<void>(check("a\n  ", "a\n", "[[BPR-1485]] a whitespace-only final line on the left aligns"));
			static_cast<void>(check("a\n", "a\n  ", "[[BPR-1485]] a whitespace-only final line on the right aligns"));
			static_cast<void>(check("a\n  ", "a\nb", "[[BPR-1485]] a whitespace-only final line against a non-empty one aligns"));
			static_cast<void>(check("a\nb", "a\n\t", "[[BPR-1485]] a non-empty final line against a whitespace-only one aligns"));
			static_cast<void>(check(std::nullopt, "  ", "[[BPR-1485]] an added whitespace-only file aligns"));
			static_cast<void>(check("\t ", std::nullopt, "[[BPR-1485]] a deleted whitespace-only file aligns"));
			expect(equalOnly(check("a\n  ", "a\n\t", "[[BPR-1485]] whitespace-only final lines on both sides align")),
				   "[[BPR-1485]] two final lines that normalize to nothing are equal");
			expect(!equalOnly(check("a\n  ", "a\n\n", "[[BPR-1485]] a whitespace-only final line against an empty ended line aligns")),
				   "[[BPR-1485]] a final line without an ending differs from an empty line that has one");
		}
	}

	auto test_bpr1485_primary_matches_come_from_the_line_matcher() -> void
	{
		const auto left    = copy_bytes("alpha\nbeta\n");
		const auto right   = copy_bytes("alpha\ngamma\n");
		const auto request = diff_compute_input{
			.Key        = comparison_key({ }, true, true, 31U),
			.Options    = { },
			.LeftBytes  = std::span<const std::uint8_t>{ left },
			.RightBytes = std::span<const std::uint8_t>{ right },
		};
		for (const auto offered : { false, true }) {
			auto       component = declining_primary{ offered };
			const auto refused   = compute_file_diff(request, component);
			expect(refused.Reason == diff_reason_code::host_failure && !refused.Result.has_value() && component.Calls == (offered ? 1U : 0U),
				   "[[BPR-1485]] a comparison without the line matcher's matches is refused as a host failure, never computed another way");
		}
		const auto binary = copy_bytes(std::string_view{ "a\0b", 3U });
		auto       none   = declining_primary{ false };
		expect(compute_file_diff(diff_compute_input{ .Key = request.Key, .Options = { }, .LeftBytes = std::span<const std::uint8_t>{ binary }, .RightBytes = request.RightBytes }, none).Reason
				   == diff_reason_code::binary_file,
			   "[[BPR-1485]] a binary file is decided before the line matcher is asked");

		// Normalized equality: the matcher compares lines as the engine
		// normalizes them, and the source bytes are kept.
		auto normalized        = diff_options{ };
		normalized.Whitespace  = whitespace_policy::ignore_all;
		normalized.LineEndings = line_ending_policy::normalize;
		const auto ignoring    = text_result(run("a b\r\nc\n", "ab\nc\r\n", normalized), "normalized comparison");
		expect(ignoring.Alignment.Segments.size() == 1U && ignoring.Alignment.Segments.front().Kind == alignment_kind::equal
				   && ignoring.LeftDocument->Bytes == copy_bytes("a b\r\nc\n"),
			   "[[BPR-1485]] lines equal under the options match, and the source bytes are retained");

		std::string largeLeft, largeRight;
		for (int line = 0; line < 20000; ++line) {
			const auto prefix = "int sample_" + std::to_string(line) + " = ";
			largeLeft += prefix + "0;\n";
			largeRight += prefix + (line % 1000 == 0 ? "1;\n" : "0;\n");
		}
		const auto large = text_result(run(largeLeft, largeRight), "default limits compare large ordered source");
		expect(large.LeftDocument->Lines.size() == 20000U && large.RightDocument->Lines.size() == 20000U && large.Alignment.Groups.size() == 20U
				   && !large.AlignmentApproximate && large.MatrixCellsConsumed <= diff_compute_limits{ }.MaximumMatrixCells,
			   "large comparison preserves source and groups within the matrix budget");
	}

	/// [[BPR-0719]]: a limit degrades the alignment; it never refuses a file
	/// for its size alone.
	auto test_large_files_degrade_instead_of_refusing() -> void
	{
		// Every line repeats, so the ordered-unique fallback cannot help; only
		// trimming the common prefix and suffix makes the pure insertion exact.
		const auto repeatedLine = [](const int aLine) -> std::string {
			switch (aLine % 5) {
			case 0:  return "}\n";
			case 1:  return "\n";
			case 2:  return "    VERIFY_IS_TRUE(result);\n";
			case 3:  return "    auto& buffer = si.GetTextBuffer();\n";
			default: return "    {\n";
			}
		};
		std::string largeLeft;
		for (int line = 0; line < 8000; ++line) {
			largeLeft += repeatedLine(line);
		}
		std::string inserted;
		for (int line = 0; line < 291; ++line) {
			inserted += repeatedLine(line);
		}
		std::string largeRight;
		for (int line = 0; line < 4000; ++line) {
			largeRight += repeatedLine(line);
		}
		largeRight += inserted;
		for (int line = 4000; line < 8000; ++line) {
			largeRight += repeatedLine(line);
		}
		const auto insertion = text_result(run(largeLeft, largeRight), "a repeated 8,000-line file with a pure insertion computes under default limits");
		expect(!insertion.AlignmentApproximate, "trimming the common prefix and suffix keeps the pure insertion exact");
		expect(insertion.Alignment.LeftLineCount == 8000U && insertion.Alignment.RightLineCount == 8291U, "both documents are retained whole");
		auto insertedLines = std::uint64_t{ 0U };
		auto removedLines  = std::uint64_t{ 0U };
		for (const auto& segment : insertion.Alignment.Segments) {
			if (segment.Kind == alignment_kind::right_only) {
				insertedLines += segment.Right.extent();
			} else if (segment.Kind != alignment_kind::equal) {
				removedLines += segment.Left.extent();
			}
		}
		expect(insertedLines == 291U && removedLines == 0U && insertion.Alignment.Groups.size() == 1U, "every inserted line is reported and nothing else changes");
		expect(insertion.MatrixCellsConsumed <= diff_compute_limits{ }.MaximumMatrixCells, "the exact alignment stays within the matrix budget");

		// The changed region alone exceeds the bound, every line repeats and
		// nothing anchors: the result is an approximate alignment, not a refusal.
		std::string ambiguousLeft  = "left only opening line\n";
		std::string ambiguousRight = "right only opening line\n";
		for (int line = 0; line < 5000; ++line) {
			ambiguousLeft += repeatedLine(line);
			ambiguousRight += repeatedLine(line + 2);
		}
		ambiguousLeft += "left only closing line\n";
		ambiguousRight += "right only closing line\n";
		// [[BPR-1485]] The line matcher has no budget of its own, so the
		// region aligns; only pairing and intraline budgets may approximate.
		const auto ambiguous = text_result(run(ambiguousLeft, ambiguousRight), "a large repetitive changed region still computes");
		expect(ambiguous.Alignment.LeftLineCount == 5002U && ambiguous.Alignment.RightLineCount == 5002U && !ambiguous.Alignment.Segments.empty(), "the alignment covers both documents");
		expect(ambiguous.MatrixCellsConsumed <= diff_compute_limits{ }.MaximumMatrixCells, "the alignment stays within the matrix budget");

		// An exhausted pairing budget leaves the replacement unpaired.
		auto oneCell               = diff_compute_limits{ };
		oneCell.MaximumMatrixCells = 1U;
		const auto tiny            = text_result(run("alpha\nbeta\n", "alpha\ngamma\n", { }, true, true, oneCell), "an exhausted budget on a tiny file still computes");
		expect(tiny.AlignmentApproximate && tiny.Alignment.Segments.size() == 2U, "an exhausted pairing budget degrades to an approximate replacement");
	}

	auto test_computed_result_validator() -> void
	{
		// [[BPR-1537]] Two changed words, each starting with a multibyte
		// scalar, kept apart by an unchanged " and ".
		const auto valid = text_result(
			run(std::string_view{ "\xC3\xA9lan and \xF0\x9F\x98\x80 tail\n" },
				std::string_view{ "\xC3\xA8lan and \xF0\x9F\x98\x83 tail\n" }),
			"validator fixture computes");
		expect(valid.Intraline.size() >= 2U,
			   "validator fixture contains separate multibyte edits");

		auto missingDocument = valid;
		missingDocument.LeftDocument.reset();
		expect_compute_error(
			missingDocument,
			diff_compute_validation_error::document_presence_mismatch,
			"validator rejects document/key presence disagreement");

		auto wrongCount = valid;
		wrongCount.LeftDocument->Lines.clear();
		expect_compute_error(
			wrongCount,
			diff_compute_validation_error::document_line_count_mismatch,
			"validator rejects retained document line-count disagreement");

		auto malformedDocument = valid;
		++malformedDocument.LeftDocument->Lines.front().Content.End;
		expect_compute_error(malformedDocument,
							 diff_compute_validation_error::invalid_document,
							 "validator rejects malformed line topology");

		auto orphan                   = valid;
		orphan.Intraline.front().Pair = opaque<line_pair_id>(99U);
		expect_compute_error(orphan,
							 diff_compute_validation_error::unknown_intraline_pair,
							 "validator rejects orphan intraline identity");

		auto midScalar = valid;
		++midScalar.Intraline.front().Left.Begin;
		expect_compute_error(midScalar,
							 diff_compute_validation_error::invalid_intraline_span,
							 "validator rejects a mid-scalar intraline endpoint");

		auto overlap                     = valid;
		overlap.Intraline[1].Left.Begin  = overlap.Intraline[0].Left.Begin;
		overlap.Intraline[1].Right.Begin = overlap.Intraline[0].Right.Begin;
		expect_compute_error(overlap,
							 diff_compute_validation_error::nonmonotonic_intraline,
							 "validator rejects overlapping intraline edits");

		auto  exactPair = valid;
		auto& pair      = exactPair.Alignment.Pairs.front();
		pair.Kind       = line_pair_kind::exact;
		pair.Id         = pair_identity(exactPair.Alignment.Digest,
										pair.Segment,
										pair.LeftLine,
										pair.RightLine,
										pair.Kind,
										0U);
		for (auto& edit : exactPair.Intraline) {
			edit.Pair = pair.Id;
		}
		expect_compute_error(
			exactPair,
			diff_compute_validation_error::intraline_for_exact_pair,
			"validator rejects intraline edits on an exact pair");

		auto unequalEqualSegment = text_result(
			run("same\n", "same\n"), "equal validator fixture computes");
		unequalEqualSegment.RightDocument->Bytes.front() = 'x';
		expect_compute_error(
			unequalEqualSegment,
			diff_compute_validation_error::invalid_exact_content,
			"validator rejects unequal retained text in an equal segment");

		auto exactPairContent = text_result(run("prefix one\n", "prefix two\n"),
											"exact pair content validator fixture computes");
		expect(exactPairContent.Alignment.Pairs.size() == 1U,
			   "exact pair content fixture starts with one similar pair");
		auto& contentPair = exactPairContent.Alignment.Pairs.front();
		contentPair.Kind  = line_pair_kind::exact;
		contentPair.Id    = pair_identity(exactPairContent.Alignment.Digest,
										  contentPair.Segment,
										  contentPair.LeftLine,
										  contentPair.RightLine,
										  contentPair.Kind,
										  0U);
		exactPairContent.Intraline.clear();
		auto&      rightBytes = exactPairContent.RightDocument->Bytes;
		const auto rightText  = std::string_view{ "prefix one\n" };
		std::ranges::copy(rightText, rightBytes.begin());
		expect(validate_computed_file_diff(exactPairContent).valid(),
			   "validator accepts normalized-equal retained exact-pair text");
		rightBytes[7U] = 't';
		expect_compute_error(
			exactPairContent,
			diff_compute_validation_error::invalid_exact_content,
			"validator rejects unequal retained text in an exact pair");
	}

	auto test_generated_replay_validation() -> void
	{
		const auto atoms = std::array<std::string_view, 4U>{ "a\n", "b\n", "c\n", "" };
		for (const auto first : atoms) {
			for (const auto second : atoms) {
				const auto left = std::string{ first } + std::string{ second };
				for (const auto third : atoms) {
					const auto right  = std::string{ second } + std::string{ third };
					const auto result = run(left, right);
					const auto replay = run(left, right);
					expect(result.Result.has_value() && validate_alignment(result.Result->Alignment).valid(),
						   "generated comparison validates");
					expect(result.Result == replay.Result,
						   "generated comparison replays exactly");
				}
			}
		}
	}

	// [[BPR-1366]] ---------------------------------------------------------
	// A computation that a caller's predicate may stop, pausing with a state
	// that resumes it.

	constexpr auto phase_count = static_cast<std::size_t>(diff_compute_phase::finished);

	/// An unoptimized build runs the engine several times slower; the large
	/// pairs are large in an optimized build and smaller in a debug one.
#ifdef NDEBUG
	constexpr auto optimized_build = true;
#else
	constexpr auto optimized_build = false;
#endif

	[[nodiscard]] constexpr auto phase_name(const diff_compute_phase aPhase) -> std::string_view
	{
		switch (aPhase) {
		case diff_compute_phase::prepare:
			return "prepare";
		case diff_compute_phase::primary:
			return "primary";
		case diff_compute_phase::refine:
			return "refine";
		case diff_compute_phase::moves:
			return "moves";
		case diff_compute_phase::assemble:
			return "assemble";
		case diff_compute_phase::validate:
			return "validate";
		case diff_compute_phase::finished:
			return "finished";
		}
		return "unknown";
	}

	/// A deterministic xorshift sequence for seeded inputs and stop points.
	struct seeded_sequence
	{
		std::uint64_t State = 1U;

		[[nodiscard]] auto next() -> std::uint64_t
		{
			State ^= State << 13U;
			State ^= State >> 7U;
			State ^= State << 17U;
			return State;
		}

		[[nodiscard]] auto below(const std::uint64_t aBound) -> std::uint64_t
		{
			return aBound == 0U ? 0U : next() % aBound;
		}
	};

	/// \brief The shape of one synthetic file pair.
	struct synthetic_shape
	{
		std::uint64_t Lines = 0U;
		std::uint64_t Seed  = 1U;
		/// Per mille of lines edited in place, deleted, inserted, or starting a
		/// moved block of twenty lines.
		std::uint32_t Edited   = 30U;
		std::uint32_t Deleted  = 10U;
		std::uint32_t Inserted = 10U;
		std::uint32_t Moved    = 2U;
		bool          Crlf     = false;
		bool          Unicode  = false;
	};

	[[nodiscard]] auto synthetic_line(seeded_sequence& aSequence, const std::uint64_t aOrdinal, const bool aUnicode)
		-> std::string
	{
		switch (aSequence.below(12U)) {
		case 0U:
			return "";
		case 1U:
			return "    }";
		default:
			break;
		}
		auto line = std::string{ "    auto value_" } + std::to_string(aOrdinal) + " = compute(" + std::to_string(aSequence.below(100'000U)) + ", item" + std::to_string(aSequence.below(1'000U)) + ") + token" + std::to_string(aSequence.below(50'000U)) + ";";
		if (aUnicode && aSequence.below(4U) == 0U) {
			line += " // \xC3\xA9t\xC3\xA9 \xE2\x82\xAC";
		}
		return line;
	}

	/// \brief A seeded pair of related files: the right side edits, deletes,
	/// inserts and moves lines of the left.
	[[nodiscard]] auto synthetic_pair(const synthetic_shape& aShape)
		-> std::pair<std::vector<std::uint8_t>, std::vector<std::uint8_t>>
	{
		auto sequence = seeded_sequence{ .State = aShape.Seed * 0x9E37'79B9'7F4A'7C15ULL + 1U };
		auto left     = std::vector<std::string>{ };
		for (std::uint64_t line = 0U; line < aShape.Lines; ++line) {
			left.push_back(synthetic_line(sequence, line, aShape.Unicode));
		}
		auto right = std::vector<std::string>{ };
		auto moved = std::vector<std::string>{ };
		for (std::uint64_t line = 0U; line < aShape.Lines; ++line) {
			const auto roll = sequence.below(1'000U);
			if (roll < aShape.Moved && line + 30U < aShape.Lines) {
				moved.insert(moved.end(), left.begin() + static_cast<std::ptrdiff_t>(line), left.begin() + static_cast<std::ptrdiff_t>(line + 20U));
				line += 19U;
				continue;
			}
			if (roll < aShape.Moved + aShape.Deleted) {
				continue;
			}
			if (roll < aShape.Moved + aShape.Deleted + aShape.Inserted) {
				right.push_back(synthetic_line(sequence, aShape.Lines + line, aShape.Unicode));
			}
			if (roll >= 1'000U - aShape.Edited) {
				auto edited = left[line];
				if (edited.empty()) {
					edited = "  // note";
				} else {
					edited.insert(edited.size() / 2U, "x_" + std::to_string(sequence.below(9U)));
				}
				right.push_back(edited);
			} else {
				right.push_back(left[line]);
			}
			if (!moved.empty() && sequence.below(1'000U) < 5U) {
				right.insert(right.end(), moved.begin(), moved.end());
				moved.clear();
			}
		}
		right.insert(right.end(), moved.begin(), moved.end());
		const auto join = [&aShape](const std::vector<std::string>& aLines) {
			auto text = std::string{ };
			for (const auto& line : aLines) {
				text += line;
				text += aShape.Crlf ? "\r\n" : "\n";
			}
			return copy_bytes(text);
		};
		return { join(left), join(right) };
	}

	/// \brief One comparison of a synthetic pair, owning its bytes.
	struct synthetic_case
	{
		std::vector<std::uint8_t> Left;
		std::vector<std::uint8_t> Right;
		diff_options              Options;
		diff_compute_limits       Limits;
		std::uint8_t              FileSeed = 31U;

		[[nodiscard]] auto input() const -> diff_compute_input
		{
			return diff_compute_input{
				.Key        = comparison_key(Options, true, true, FileSeed),
				.Options    = Options,
				.LeftBytes  = std::span<const std::uint8_t>{ Left },
				.RightBytes = std::span<const std::uint8_t>{ Right },
				.Limits     = Limits,
			};
		}

		[[nodiscard]] auto material_input() const -> material_diff_input
		{
			return material_diff_input{
				.Key = material_comparison_key{
					.Comparison   = opaque<file_comparison_digest>(FileSeed),
					.LeftPresent  = true,
					.RightPresent = true,
					.Options      = *options_digest(Options),
					.Algorithm    = supported_diff_algorithm_revision,
				},
				.Options    = Options,
				.LeftBytes  = std::span<const std::uint8_t>{ Left },
				.RightBytes = std::span<const std::uint8_t>{ Right },
				.Limits     = Limits,
			};
		}
	};

	[[nodiscard]] auto make_case(const synthetic_shape& aShape, const diff_options& aOptions = { },
								 const diff_compute_limits& aLimits = { }) -> synthetic_case
	{
		auto [left, right] = synthetic_pair(aShape);
		auto result        = synthetic_case{ };
		result.Left        = std::move(left);
		result.Right       = std::move(right);
		result.Options     = aOptions;
		result.Limits      = aLimits;
		return result;
	}

	[[nodiscard]] auto moves_options() -> diff_options
	{
		auto options  = diff_options{ };
		options.Moves = move_detection_policy::deterministic_exact;
		return options;
	}

	/// \brief Stops once the computation reaches a step, and records the step
	/// of the poll that saw it.
	class stop_at_step final : public diff_compute_stop
	{
	public:
		explicit stop_at_step(const std::uint64_t aAt) noexcept
			: At{ aAt }
		{
		}

		[[nodiscard]] auto stop_requested(const std::uint64_t aSteps) -> bool override
		{
			if (aSteps < At) {
				return false;
			}
			StoppedAt = aSteps;
			return true;
		}

		std::uint64_t                At = 0U;
		std::optional<std::uint64_t> StoppedAt;
	};

	/// \brief Never stops; records the wall time between polls.
	class poll_timer final : public diff_compute_stop
	{
	public:
		[[nodiscard]] auto stop_requested(const std::uint64_t /*aSteps*/) -> bool override
		{
			const auto now = std::chrono::steady_clock::now();
			Longest        = std::max(Longest, std::chrono::duration<double, std::milli>(now - Last).count());
			Last           = now;
			++Polls;
			return false;
		}

		std::chrono::steady_clock::time_point Last    = std::chrono::steady_clock::now();
		double                                Longest = 0.0;
		std::uint64_t                         Polls   = 0U;
	};

	[[nodiscard]] auto uninterrupted(const synthetic_case& aCase) -> file_diff_compute_run
	{
		auto run = compute_file_diff(aCase.input(), diff_compute_control{ }, product_matching());
		expect(run.Outcome.has_value() && run.Statistics.Phase == diff_compute_phase::finished,
			   "[[BPR-1366]] nothing stops a computation without a predicate");
		expect(run.Outcome->Result == product_file_diff(aCase.input()).Result && run.Outcome->Reason == product_file_diff(aCase.input()).Reason,
			   "[[BPR-1366]] the stoppable entry point computes what the one call computes");
		return run;
	}

	/// The steps where aPhase begins and ends in a run of aStatistics.
	[[nodiscard]] auto phase_window(const diff_compute_statistics& aStatistics, const diff_compute_phase aPhase)
		-> std::pair<std::uint64_t, std::uint64_t>
	{
		const auto index = static_cast<std::size_t>(aPhase);
		const auto begin = aStatistics.PhaseStarts[index];
		const auto end   = index + 1U < phase_count ? aStatistics.PhaseStarts[index + 1U] : aStatistics.Steps;
		return { begin, end };
	}

	/// The comparisons the property tests run: seeded shapes, options and
	/// limits, including limits that make the alignment approximate.
	[[nodiscard]] auto property_cases() -> std::vector<synthetic_case>
	{
		auto cases = std::vector<synthetic_case>{ };
		cases.push_back(make_case({ .Lines = 500U, .Seed = 3U, .Edited = 200U, .Moved = 4U }, moves_options()));
		cases.push_back(make_case({ .Lines = 400U, .Seed = 5U, .Edited = 300U, .Deleted = 20U, .Inserted = 20U, .Moved = 6U, .Crlf = true, .Unicode = true }, moves_options()));
		auto ignoring        = moves_options();
		ignoring.Whitespace  = whitespace_policy::ignore_all;
		ignoring.LineEndings = line_ending_policy::normalize;
		cases.push_back(make_case({ .Lines = 400U, .Seed = 7U, .Edited = 120U, .Moved = 6U, .Unicode = true }, ignoring));
		// [[BPR-0719]] Budgets small enough that pairs and intraline passes
		// degrade.
		auto tight                     = diff_compute_limits{ };
		tight.MaximumMatrixCells       = 400'000U;
		tight.MaximumMoveDetectionWork = 40'000U;
		cases.push_back(make_case({ .Lines = 700U, .Seed = 11U, .Edited = 250U, .Moved = 5U }, moves_options(), tight));
		auto unpaired                        = diff_options{ };
		unpaired.Pairing                     = pairing_policy::disabled;
		unpaired.SimilarityThresholdPermille = 0U;
		cases.push_back(make_case({ .Lines = 400U, .Seed = 13U, .Edited = 60U }, unpaired));
		return cases;
	}

	auto test_bpr1366_an_uninterrupted_run_is_the_one_call() -> void
	{
		for (const auto& testCase : property_cases()) {
			const auto slice  = uninterrupted(testCase);
			auto       never  = stop_at_step{ std::numeric_limits<std::uint64_t>::max() };
			const auto polled = compute_file_diff(testCase.input(), diff_compute_control{ .Stop = &never, .CheckInterval = 1U }, product_matching());
			expect(polled.Outcome.has_value() && polled.Outcome->Result == slice.Outcome->Result && polled.Statistics.Steps == slice.Statistics.Steps
					   && polled.Statistics.PhaseStarts == slice.Statistics.PhaseStarts,
				   "[[BPR-1366]] a predicate that never fires changes nothing, however often it is polled");
		}
		// The checks that need no text finish at once.
		auto binary        = make_case({ .Lines = 50U, .Seed = 2U });
		binary.Right[10U]  = 0U;
		const auto refused = compute_file_diff(binary.input(), diff_compute_control{ }, product_matching());
		expect(refused.Outcome.has_value() && refused.Outcome->Reason == diff_reason_code::binary_file,
			   "[[BPR-1366]] a binary file finishes in one run");
	}

	auto test_bpr1366_a_stop_is_honoured_within_the_bound() -> void
	{
		// A large pair: hundreds of milliseconds uninterrupted in an optimized
		// build. Each stop below must come at the first poll at or after its
		// request: within the check interval plus the poll delay, which one
		// indivisible increment of work bounds. [[BPR-1485]] A stop abandons
		// the computation.
		std::cout << "QUALIFICATION_WORKLOAD:stop-lines=" << (optimized_build ? 40'000U : 6'000U) << '\n';
		const auto large = make_case({ .Lines = optimized_build ? 40'000U : 6'000U, .Seed = 17U, .Edited = 300U, .Deleted = 15U, .Inserted = 15U, .Moved = 3U }, moves_options());
		auto       timer = poll_timer{ };
		const auto start = std::chrono::steady_clock::now();
		const auto timed = compute_file_diff(large.input(), diff_compute_control{ .Stop = &timer }, product_matching());
		const auto wall  = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		const auto full  = uninterrupted(large);
		const auto total = full.Statistics.Steps;
		const auto bound = default_diff_compute_check_interval + timed.Statistics.LargestPollDelay;
		expect(timed.Outcome.has_value() && timed.Outcome->Result == full.Outcome->Result && timed.Statistics.Steps == total,
			   "[[BPR-1366]] polling the predicate changes nothing");
		expect(timed.Statistics.LargestPollDelay <= 2U * default_diff_compute_check_interval,
			   "[[BPR-1366]] no increment of work on this pair is larger than two check intervals");
		std::cout << "BPR-1366 prompt stop: " << total << " steps, " << wall << " ms uninterrupted (" << timer.Polls
				  << " polls, longest " << timer.Longest << " ms between polls); bound " << default_diff_compute_check_interval
				  << " + " << timed.Statistics.LargestPollDelay << " steps\n";
		auto sequence = seeded_sequence{ .State = 1366U };
		auto landed   = std::array<bool, phase_count>{ };
		for (std::size_t phase = 0U; phase < phase_count; ++phase) {
			const auto [begin, end] = phase_window(full.Statistics, static_cast<diff_compute_phase>(phase));
			expect(end > begin, "[[BPR-1366]] every phase of the large pair does work");
			for (std::size_t sample = 0U; sample < 2U; ++sample) {
				const auto request = begin + sequence.below(end - begin);
				auto       stop    = stop_at_step{ request };
				const auto slice   = compute_file_diff(large.input(), diff_compute_control{ .Stop = &stop }, product_matching());
				expect(!slice.Outcome.has_value() && stop.StoppedAt.has_value(), "[[BPR-1366]] a stop requested mid-computation abandons it");
				expect(*stop.StoppedAt >= request && *stop.StoppedAt - request <= bound,
					   "[[BPR-1366]] a stop is honoured within the check interval plus the poll delay");
				expect(*stop.StoppedAt < total, "[[BPR-1366]] the stop comes before the computation would have finished");
				landed[static_cast<std::size_t>(slice.Statistics.Phase)] = true;
				std::cout << "BPR-1366 stop in " << phase_name(static_cast<diff_compute_phase>(phase)) << ": requested at "
						  << request << ", stopped at " << *stop.StoppedAt << " (+" << (*stop.StoppedAt - request) << " steps) in "
						  << phase_name(slice.Statistics.Phase) << '\n';
			}
		}
		expect(std::ranges::all_of(landed, [](const bool aLanded) { return aLanded; }), "[[BPR-1366]] the stops land in every phase");
	}

	/// [[BPR-1485]] Restart-only cancellation: a stopped computation keeps
	/// nothing, and computing the comparison again from the start, stoppably
	/// or not, gives exactly the uninterrupted result.
	auto test_bpr1366_interrupted_once_then_recomputed() -> void
	{
		auto sequence = seeded_sequence{ .State = 2024U };
		auto cases    = property_cases();
		for (std::size_t index = 0U; index < cases.size(); ++index) {
			const auto& testCase = cases[index];
			const auto  full     = uninterrupted(testCase);
			for (std::size_t phase = 0U; phase < phase_count; ++phase) {
				const auto [begin, end] = phase_window(full.Statistics, static_cast<diff_compute_phase>(phase));
				if (end <= begin) {
					continue;
				}
				auto stop  = stop_at_step{ begin + sequence.below(end - begin) };
				auto slice = compute_file_diff(testCase.input(), diff_compute_control{ .Stop = &stop, .CheckInterval = 1U }, product_matching());
				expect(!slice.Outcome.has_value(), "[[BPR-1366]] the seeded stop abandons the computation");
				// Recomputed from scratch, the stop leaves no trace.
				const auto again = product_file_diff(testCase.input());
				expect(again.Reason == full.Outcome->Reason && again.Result == full.Outcome->Result,
					   "[[BPR-1366]] a computation recomputed after a stop equals the uninterrupted one, counters and approximation included");
				const auto restarted = compute_file_diff(testCase.input(), diff_compute_control{ }, product_matching());
				expect(restarted.Outcome.has_value() && restarted.Outcome->Result == full.Outcome->Result && restarted.Statistics == full.Statistics,
					   "[[BPR-1485]] a restarted stoppable computation equals the uninterrupted one, steps and phases included");
			}
		}
	}

	auto test_bpr1366_the_semantic_index_build_stops() -> void
	{
		const auto testCase = property_cases().front();
		const auto result   = std::make_shared<const computed_file_diff>(*product_file_diff(testCase.input()).Result);
		auto       stop     = stop_at_step{ 1U };
		const auto stopped  = build_diff_semantic_index(result, diff_compute_control{ .Stop = &stop, .CheckInterval = 1U });
		expect(stopped.Stopped && !stopped.Index.has_value() && stop.StoppedAt.has_value(), "[[BPR-1366]] the index build polls the stop through its validation");
		auto       never = stop_at_step{ std::numeric_limits<std::uint64_t>::max() };
		const auto built = build_diff_semantic_index(result, diff_compute_control{ .Stop = &never, .CheckInterval = 1U });
		const auto plain = build_diff_semantic_index(result);
		expect(!built.Stopped && built.Index.has_value() && plain.has_value() && built.Index->matches_result(*result) && built.Index->statistics() == plain->statistics(),
			   "[[BPR-1366]] an index build nothing stops equals the plain build");
		auto invalid = *result;
		invalid.Intraline.push_back(intraline_edit{ });
		const auto refused = build_diff_semantic_index(std::make_shared<const computed_file_diff>(std::move(invalid)), diff_compute_control{ });
		expect(!refused.Stopped && !refused.Index.has_value(), "[[BPR-1366]] an invalid result still builds no index");
	}
} // namespace

int main()
{
	bpr::tests::report_qualification_build();
	test_identical_and_empty();
	test_insertions_and_deletions();
	test_replacements_pairing_and_replay();
	test_whitespace_and_endings();
	test_unicode_binary_and_unavailable();
	test_moves_identity_and_limits();
	test_bpr1485_primary_matches_come_from_the_line_matcher();
	test_bpr1485_whitespace_only_final_lines();
	test_large_files_degrade_instead_of_refusing();
	test_computed_result_validator();
	test_generated_replay_validation();
	test_bpr1366_an_uninterrupted_run_is_the_one_call();
	test_bpr1366_a_stop_is_honoured_within_the_bound();
	test_bpr1366_interrupted_once_then_recomputed();
	test_bpr1366_the_semantic_index_build_stops();
	std::cout << "BPR-0073 deterministic diff computation tests passed\n";
	return EXIT_SUCCESS;
}
