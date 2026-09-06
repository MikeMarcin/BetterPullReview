#include "bpr/core/diff_compute.hpp"

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
#include <vector>

namespace {
	using namespace bpr::core;

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
		return compute_file_diff(input);
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
			exactEndings.LeftDocument->Lines[0].Ending == text_line_ending::crlf && exactEndings.LeftDocument->Lines[1].Ending == text_line_ending::cr,
			"splitter distinguishes CRLF and lone CR endings");
		expect(
			exactEndings.LeftDocument->Lines[1].Content.End - exactEndings.LeftDocument->Lines[1].Content.Begin == 1U,
			"lone CR terminates a logical line without entering display content");

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

		auto overCells                    = exactLimits;
		overCells.MaximumPrimaryIndexWork = 0U;
		--overCells.MaximumMatrixCells;
		const auto cellLimited = run("a\n", "b\n", noPairing, true, true, overCells);
		expect(cellLimited.Reason == diff_reason_code::compute_limit && cellLimited.Result.has_value() && cellLimited.Result->Alignment.Segments.empty(),
			   "over-limit computation publishes no partial records");

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
		const auto pairedLimited = run("prefix alpha\n", "prefix beta\n", { }, true, true, pairedLimits);
		expect(pairedLimited.Reason == diff_reason_code::compute_limit && pairedLimited.Result.has_value() && pairedLimited.Result->Alignment.Segments.empty(),
			   "one cell below secondary work returns no partial records");
	}

	auto test_ordered_unique_line_fallback() -> void
	{
		auto options                        = diff_options{ };
		options.Pairing                     = pairing_policy::disabled;
		options.SimilarityThresholdPermille = 0U;
		auto indexedLimits                  = diff_compute_limits{ };
		indexedLimits.MaximumMatrixCells    = 0U;

		// Exhaustively compare small sequences against the original full-matrix
		// result. Duplicates/crossings may refuse, but must never change a tie.
		std::vector<std::string> sequences{ "" };
		std::size_t              begin = 0U, end = 1U;
		for (int length = 0; length < 3; ++length) {
			for (std::size_t index = begin; index < end; ++index) {
				for (const char value : { 'a', 'b', 'c' }) {
					sequences.push_back(sequences[index] + value + '\n');
				}
			}
			begin = end;
			end   = sequences.size();
		}
		for (const auto& left : sequences) {
			for (const auto& right : sequences) {
				bool        eligible = true;
				std::size_t previous = 0U;
				for (std::size_t position = 0U; position < left.size(); position += 2U) {
					const auto found = right.find(left[position]);
					if (found == std::string::npos) {
						continue;
					}
					if (std::ranges::count(left, left[position]) != 1 || std::ranges::count(right, left[position]) != 1
						|| found + 1U <= previous) {
						eligible = false;
						break;
					}
					previous = found + 1U;
				}
				const auto indexed = run(left, right, options, true, true, indexedLimits);
				expect((indexed.Reason == diff_reason_code::none) == eligible, "ordered unique eligibility agrees with independent occurrence/order check");
				if (!eligible) {
					expect(indexed.Reason == diff_reason_code::compute_limit && indexed.Result && indexed.Result->Alignment.Segments.empty(), "ambiguous fallback publishes no partial alignment");
					continue;
				}
				const auto matrix = text_result(run(left, right, options), "matrix reference");
				const auto actual = text_result(indexed, "indexed reference");
				expect(actual.Alignment == matrix.Alignment && actual.LeftDocument == matrix.LeftDocument && actual.RightDocument == matrix.RightDocument
						   && actual.Intraline == matrix.Intraline && actual.MatrixCellsConsumed == 0U,
					   "indexed result preserves exact matrix semantics and identities");
			}
		}

		auto normalized             = options;
		normalized.Whitespace       = whitespace_policy::ignore_all;
		normalized.LineEndings      = line_ending_policy::normalize;
		const auto normalizedMatrix = text_result(run("a b\r\nc\n", "ab\nc\r\n", normalized), "normalization matrix");
		const auto normalizedIndex  = text_result(run("a b\r\nc\n", "ab\nc\r\n", normalized, true, true, indexedLimits), "normalization index");
		expect(normalizedIndex.Alignment == normalizedMatrix.Alignment && normalizedIndex.LeftDocument == normalizedMatrix.LeftDocument
				   && normalizedIndex.RightDocument == normalizedMatrix.RightDocument,
			   "index uses normalized equality and retains source bytes");
		const auto repeatedNormalized = run("a b\nab\n", "ab\n", normalized, true, true, indexedLimits);
		expect(repeatedNormalized.Reason == diff_reason_code::compute_limit, "normalization cannot hide an ambiguous repeated anchor");

		const auto measured = text_result(run("alpha\nbeta\n", "alpha\nnew\nbeta\n", options, true, true, indexedLimits), "index budget baseline");
		expect(measured.PrimaryIndexWorkConsumed > 0U, "index work is independently reported");
		indexedLimits.MaximumPrimaryIndexWork = measured.PrimaryIndexWorkConsumed;
		expect(run("alpha\nbeta\n", "alpha\nnew\nbeta\n", options, true, true, indexedLimits).Reason == diff_reason_code::none, "exact index budget succeeds");
		--indexedLimits.MaximumPrimaryIndexWork;
		const auto exhausted = run("alpha\nbeta\n", "alpha\nnew\nbeta\n", options, true, true, indexedLimits);
		expect(exhausted.Reason == diff_reason_code::compute_limit && exhausted.Result && exhausted.Result->Alignment.Segments.empty(), "exhausted index budget publishes no partial records");

		std::string largeLeft, largeRight;
		for (int line = 0; line < 20000; ++line) {
			const auto prefix = "int sample_" + std::to_string(line) + " = ";
			largeLeft += prefix + "0;\n";
			largeRight += prefix + (line % 1000 == 0 ? "1;\n" : "0;\n");
		}
		const auto large = text_result(run(largeLeft, largeRight), "default limits compare large ordered source");
		expect(large.LeftDocument->Lines.size() == 20000U && large.RightDocument->Lines.size() == 20000U && large.Alignment.Groups.size() == 20U
				   && large.PrimaryIndexWorkConsumed > 0U && large.PrimaryIndexWorkConsumed <= diff_compute_limits{ }.MaximumPrimaryIndexWork
				   && large.MatrixCellsConsumed <= diff_compute_limits{ }.MaximumMatrixCells,
			   "large comparison preserves source/groups within both independent budgets");
	}

	auto test_computed_result_validator() -> void
	{
		const auto valid = text_result(
			run(std::string_view{ "caf\xC3\xA9 \xF0\x9F\x98\x80 tail\n" },
				std::string_view{ "caf\xC3\xA8 \xF0\x9F\x98\x83 tail\n" }),
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
} // namespace

int main()
{
	test_identical_and_empty();
	test_insertions_and_deletions();
	test_replacements_pairing_and_replay();
	test_whitespace_and_endings();
	test_unicode_binary_and_unavailable();
	test_moves_identity_and_limits();
	test_ordered_unique_line_fallback();
	test_computed_result_validator();
	test_generated_replay_validation();
	std::cout << "BPR-0073 deterministic diff computation tests passed\n";
	return EXIT_SUCCESS;
}
