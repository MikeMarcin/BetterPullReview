#include "bpr/core/diff_model.hpp"

#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/diff_compute.hpp"
#include "bpr/core/file_progress_mark.hpp"
#include "bpr/core/review_ledger.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
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

	auto expect_error(const file_alignment&            aAlignment,
					  const alignment_validation_error aExpected,
					  const std::string_view           aMessage) -> void
	{
		const auto result = validate_alignment(aAlignment);
		if (result.Error != aExpected) {
			std::cerr << "FAILED: " << aMessage << " (expected "
					  << static_cast<unsigned>(aExpected) << ", got "
					  << static_cast<unsigned>(result.Error) << ")\n";
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

	[[nodiscard]] constexpr auto digest_bytes(const std::string_view aHex)
		-> std::array<std::uint8_t, 32>
	{
		const auto nibble = [](const char aValue) -> std::uint8_t {
			return aValue >= '0' && aValue <= '9'
				? static_cast<std::uint8_t>(aValue - '0')
				: static_cast<std::uint8_t>(aValue - 'a' + 10);
		};
		auto result = std::array<std::uint8_t, 32>{ };
		for (std::size_t index = 0U; index < result.size(); ++index) {
			result[index] = static_cast<std::uint8_t>(
				static_cast<std::uint8_t>(nibble(aHex[index * 2U]) << 4U)
				| nibble(aHex[index * 2U + 1U]));
		}
		return result;
	}

	[[nodiscard]] constexpr auto blob(const std::uint8_t       aSeed,
									  const git_hash_algorithm aAlgorithm = git_hash_algorithm::sha1)
		-> git_blob_oid
	{
		std::array<std::uint8_t, 32> bytes{ };
		const auto                   count = aAlgorithm == git_hash_algorithm::sha1 ? 20U : 32U;
		for (std::size_t index = 0U; index < count; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + static_cast<std::uint8_t>(index));
		}
		return git_blob_oid{ aAlgorithm, bytes };
	}

	[[nodiscard]] auto move_options() -> diff_options
	{
		auto result  = diff_options{ };
		result.Moves = move_detection_policy::deterministic_exact;
		return result;
	}

	[[nodiscard]] auto comparison_key(diff_options aOptions = { })
		-> file_comparison_key
	{
		const auto digest = options_digest(aOptions);
		expect(digest.has_value(), "default diff options have a digest");
		const auto authority   = opaque<pull_request_authority_scope_id>(11U);
		const auto incarnation = opaque<epoch_catalog_incarnation_id>(12U);
		const auto owner       = pull_request_authority{
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
			.Scope           = authority,
		};
		return file_comparison_key{
			.Authority   = owner,
			.Repository  = owner.Repository,
			.PullRequest = owner.PullRequest,
			.LeftAnchor  = comparison_anchor_identity{
				.Authority   = authority,
				.Incarnation = incarnation,
				.Kind        = comparison_anchor_kind::epoch_base,
				.Epoch       = epoch_ordinal{ 1 },
			},
			.RightAnchor = comparison_anchor_identity{
				.Authority   = authority,
				.Incarnation = incarnation,
				.Kind        = comparison_anchor_kind::epoch_head,
				.Epoch       = epoch_ordinal{ 2 },
			},
			.LeftBlob  = blob(21U),
			.RightBlob = blob(51U),
			.File      = opaque<file_identity>(31U),
			.Options   = *digest,
			.Algorithm = diff_algorithm_revision{ 1U },
		};
	}

	[[nodiscard]] auto alignment_with_counts(
		const std::uint32_t aLeft,
		const std::uint32_t aRight,
		diff_options        aOptions = { }) -> file_alignment
	{
		auto result = file_alignment{
			.Key            = comparison_key(aOptions),
			.Options        = aOptions,
			.LeftLineCount  = aLeft,
			.RightLineCount = aRight,
		};
		result.Digest = comparison_digest(result.Key);
		return result;
	}

	auto append_segment(file_alignment&      aAlignment,
						const alignment_kind aKind,
						const line_span      aLeft,
						const line_span      aRight) -> void
	{
		const auto ordinal = static_cast<std::uint32_t>(aAlignment.Segments.size());
		aAlignment.Segments.push_back(alignment_segment{
			.Id    = segment_identity(aAlignment.Digest, aKind, aLeft, aRight, ordinal),
			.Kind  = aKind,
			.Left  = aLeft,
			.Right = aRight,
		});
	}

	[[nodiscard]] constexpr auto group_summary(const std::uint64_t aAdded,
											   const std::uint64_t aDeleted,
											   const std::uint64_t aReplacedLeft,
											   const std::uint64_t aReplacedRight) -> change_summary_kind
	{
		const auto hasAdded    = aAdded != 0U;
		const auto hasDeleted  = aDeleted != 0U;
		const auto hasReplaced = aReplacedLeft != 0U || aReplacedRight != 0U;
		if (hasAdded && !hasDeleted && !hasReplaced) {
			return change_summary_kind::addition;
		}
		if (!hasAdded && hasDeleted && !hasReplaced) {
			return change_summary_kind::deletion;
		}
		if (!hasAdded && !hasDeleted && hasReplaced) {
			return change_summary_kind::replacement;
		}
		return change_summary_kind::mixed;
	}

	auto append_group(file_alignment&     aAlignment,
					  const std::uint32_t aBegin,
					  const std::uint32_t aEnd) -> void
	{
		expect(aBegin < aEnd, "test group has members");
		expect(
			static_cast<std::size_t>(aEnd) <= aAlignment.Segments.size(),
			"test group is in bounds");
		const auto& first         = aAlignment.Segments[static_cast<std::size_t>(aBegin)];
		const auto& last          = aAlignment.Segments[static_cast<std::size_t>(aEnd - 1U)];
		const auto  left          = line_span{ first.Left.Begin, last.Left.End };
		const auto  right         = line_span{ first.Right.Begin, last.Right.End };
		const auto  ordinal       = static_cast<std::uint32_t>(aAlignment.Groups.size());
		const auto  id            = group_identity(aAlignment.Digest, left, right, ordinal);
		auto        added         = std::uint64_t{ 0U };
		auto        deleted       = std::uint64_t{ 0U };
		auto        replacedLeft  = std::uint64_t{ 0U };
		auto        replacedRight = std::uint64_t{ 0U };
		for (auto member = aBegin; member < aEnd; ++member) {
			auto& segment = aAlignment.Segments[static_cast<std::size_t>(member)];
			segment.Group = id;
			switch (segment.Kind) {
			case alignment_kind::right_only:
				added += segment.Right.extent();
				break;
			case alignment_kind::left_only:
				deleted += segment.Left.extent();
				break;
			case alignment_kind::replacement:
				replacedLeft += segment.Left.extent();
				replacedRight += segment.Right.extent();
				break;
			case alignment_kind::equal:
				break;
			}
		}
		aAlignment.Groups.push_back(change_group{
			.Id      = id,
			.Members = { .Begin = aBegin, .End = aEnd },
			.Left    = left,
			.Right   = right,
			.First   = { .Left = left.Begin, .Right = right.Begin },
			.Last    = { .Left = left.End, .Right = right.End },
			.Kind    = group_summary(
				added, deleted, replacedLeft, replacedRight),
			.AddedLines         = added,
			.DeletedLines       = deleted,
			.ReplacedLeftLines  = replacedLeft,
			.ReplacedRightLines = replacedRight,
		});
	}

	auto append_pair(file_alignment&      aAlignment,
					 const std::uint32_t  aSegmentIndex,
					 const std::uint32_t  aLeft,
					 const std::uint32_t  aRight,
					 const line_pair_kind aKind) -> void
	{
		const auto segment = aAlignment.Segments[static_cast<std::size_t>(aSegmentIndex)].Id;
		const auto ordinal = static_cast<std::uint32_t>(aAlignment.Pairs.size());
		aAlignment.Pairs.push_back(line_pair{
			.Id = pair_identity(
				aAlignment.Digest, segment, aLeft, aRight, aKind, ordinal),
			.Segment   = segment,
			.LeftLine  = aLeft,
			.RightLine = aRight,
			.Kind      = aKind,
		});
	}

	auto append_move(file_alignment&     aAlignment,
					 const std::uint32_t aSourceIndex,
					 const std::uint32_t aDestinationIndex) -> void
	{
		const auto source      = aAlignment.Segments[static_cast<std::size_t>(aSourceIndex)].Id;
		const auto destination = aAlignment.Segments[static_cast<std::size_t>(aDestinationIndex)].Id;
		const auto ordinal     = static_cast<std::uint32_t>(aAlignment.Moves.size());
		aAlignment.Moves.push_back(move_relation{
			.Id = move_identity(
				aAlignment.Digest, source, destination, ordinal),
			.SourceDeletion       = source,
			.DestinationInsertion = destination,
		});
	}

	auto refresh_pair_ids(file_alignment& aAlignment) -> void
	{
		for (std::size_t index = 0U; index < aAlignment.Pairs.size(); ++index) {
			auto& pair = aAlignment.Pairs[index];
			pair.Id    = pair_identity(aAlignment.Digest,
									   pair.Segment,
									   pair.LeftLine,
									   pair.RightLine,
									   pair.Kind,
									   static_cast<std::uint32_t>(index));
		}
	}

	auto refresh_move_ids(file_alignment& aAlignment) -> void
	{
		for (std::size_t index = 0U; index < aAlignment.Moves.size(); ++index) {
			auto& move = aAlignment.Moves[index];
			move.Id    = move_identity(aAlignment.Digest,
									   move.SourceDeletion,
									   move.DestinationInsertion,
									   static_cast<std::uint32_t>(index));
		}
	}

	[[nodiscard]] auto equal_alignment(const std::uint32_t aCount = 3U)
		-> file_alignment
	{
		auto result = alignment_with_counts(aCount, aCount);
		if (aCount != 0U) {
			append_segment(result,
						   alignment_kind::equal,
						   { 0U, aCount },
						   { 0U, aCount });
		}
		return result;
	}

	[[nodiscard]] auto insertion_alignment() -> file_alignment
	{
		auto result = alignment_with_counts(2U, 4U);
		append_segment(result, alignment_kind::equal, { 0U, 1U }, { 0U, 1U });
		append_segment(result, alignment_kind::right_only, { 1U, 1U }, { 1U, 3U });
		append_segment(result, alignment_kind::equal, { 1U, 2U }, { 3U, 4U });
		append_group(result, 1U, 2U);
		return result;
	}

	[[nodiscard]] auto deletion_alignment() -> file_alignment
	{
		auto result = alignment_with_counts(4U, 2U);
		append_segment(result, alignment_kind::equal, { 0U, 1U }, { 0U, 1U });
		append_segment(result, alignment_kind::left_only, { 1U, 3U }, { 1U, 1U });
		append_segment(result, alignment_kind::equal, { 3U, 4U }, { 1U, 2U });
		append_group(result, 1U, 2U);
		return result;
	}

	[[nodiscard]] auto replacement_alignment() -> file_alignment
	{
		auto result = alignment_with_counts(4U, 4U);
		append_segment(result, alignment_kind::equal, { 0U, 1U }, { 0U, 1U });
		append_segment(result, alignment_kind::replacement, { 1U, 3U }, { 1U, 3U });
		append_segment(result, alignment_kind::equal, { 3U, 4U }, { 3U, 4U });
		append_group(result, 1U, 2U);
		append_pair(result, 1U, 1U, 1U, line_pair_kind::exact);
		append_pair(result, 1U, 2U, 2U, line_pair_kind::similar);
		return result;
	}

	[[nodiscard]] auto move_alignment(diff_options aOptions = move_options())
		-> file_alignment
	{
		auto result = alignment_with_counts(3U, 3U, aOptions);
		append_segment(result, alignment_kind::left_only, { 0U, 1U }, { 0U, 0U });
		append_segment(result, alignment_kind::equal, { 1U, 2U }, { 0U, 1U });
		append_segment(result, alignment_kind::right_only, { 2U, 2U }, { 1U, 2U });
		append_segment(result, alignment_kind::equal, { 2U, 3U }, { 2U, 3U });
		append_group(result, 0U, 1U);
		append_group(result, 2U, 3U);
		append_move(result, 0U, 2U);
		return result;
	}

	[[nodiscard]] auto two_move_alignment(diff_options aOptions = move_options())
		-> file_alignment
	{
		auto result = alignment_with_counts(5U, 5U, aOptions);
		append_segment(result, alignment_kind::left_only, { 0U, 1U }, { 0U, 0U });
		append_segment(result, alignment_kind::equal, { 1U, 2U }, { 0U, 1U });
		append_segment(result, alignment_kind::left_only, { 2U, 3U }, { 1U, 1U });
		append_segment(result, alignment_kind::equal, { 3U, 4U }, { 1U, 2U });
		append_segment(result, alignment_kind::right_only, { 4U, 4U }, { 2U, 3U });
		append_segment(result, alignment_kind::equal, { 4U, 5U }, { 3U, 4U });
		append_segment(result, alignment_kind::right_only, { 5U, 5U }, { 4U, 5U });
		append_group(result, 0U, 1U);
		append_group(result, 2U, 3U);
		append_group(result, 4U, 5U);
		append_group(result, 6U, 7U);
		append_move(result, 0U, 4U);
		append_move(result, 2U, 6U);
		return result;
	}

	[[nodiscard]] auto mixed_alignment(diff_options aOptions = { }) -> file_alignment
	{
		auto result = alignment_with_counts(2U, 3U, aOptions);
		append_segment(result, alignment_kind::left_only, { 0U, 1U }, { 0U, 0U });
		append_segment(result, alignment_kind::right_only, { 1U, 1U }, { 0U, 2U });
		append_segment(result, alignment_kind::equal, { 1U, 2U }, { 2U, 3U });
		append_group(result, 0U, 2U);
		return result;
	}

	auto test_options_and_comparison_identity() -> void
	{
		const auto defaults = diff_options{ };
		expect(valid(defaults), "default options are normalized and valid");
		const auto first  = options_digest(defaults);
		const auto replay = options_digest(defaults);
		expect(first.has_value() && first == replay,
			   "option digest repeats for exact normalized options");
		expect(first->bytes()
				   == digest_bytes(
					   "c8330c88ed5ccaf5d992d132487d89aa"
					   "e266130e3763357b6036a17e6c6eafc6"),
			   "option digest matches the endian-defined known-answer vector");

		auto disabled                        = defaults;
		disabled.Pairing                     = pairing_policy::disabled;
		disabled.SimilarityThresholdPermille = 875U;
		const auto normalized                = normalize_diff_options(disabled);
		expect(normalized.has_value()
				   && normalized->SimilarityThresholdPermille == 0U,
			   "disabled pairing has one canonical threshold");
		expect(!valid(disabled) && valid(*normalized),
			   "only the canonical disabled representation is valid");

		auto malformed       = defaults;
		malformed.Whitespace = static_cast<whitespace_policy>(255U);
		expect(!normalize_diff_options(malformed).has_value(),
			   "unknown whitespace policy is rejected");
		malformed              = defaults;
		malformed.ContextLines = maximum_diff_context_lines + 1U;
		expect(!normalize_diff_options(malformed).has_value(),
			   "unbounded context is rejected");
		malformed                             = defaults;
		malformed.SimilarityThresholdPermille = 0U;
		expect(!normalize_diff_options(malformed).has_value(),
			   "enabled pairing requires a positive threshold");
		malformed.SimilarityThresholdPermille = static_cast<std::uint16_t>(maximum_similarity_threshold_permille + 1U);
		expect(!normalize_diff_options(malformed).has_value(),
			   "similarity threshold is bounded");

		const auto key = comparison_key();
		expect(valid(key), "exact comparison key is valid");
		const auto keyDigest = comparison_digest(key);
		expect(keyDigest.valid() && keyDigest == comparison_digest(key),
			   "comparison digest replays exactly");
		// Unchanged across BPR-0531, and that is the point of running it here.
		// This digest is persisted: the review ledger and the recovery store
		// hold anchors keyed by it. Adding the access mode to the authority
		// must leave the authenticated encoding byte-identical, so this vector
		// is the compatibility proof, not just a replay check.
		expect(keyDigest.bytes()
				   == digest_bytes(
					   "fbdfe1e651fe7e0227040953c236b296"
					   "032ddcf3f30503c7107d6d8037f588ae"),
			   "comparison digest matches the exact-key known-answer vector");

		// The same files compared under the two access modes. The
		// authenticated encoding is frozen, so discrimination rests entirely
		// on the public-preview encoding carrying something the authenticated
		// one never can.
		auto previewKey              = key;
		previewKey.Authority.Mode    = github_access_mode::public_preview;
		previewKey.Authority.Account = github_account_id{ };
		previewKey.Authority.Viewer  = github_viewer_actor_id{ };
		expect(valid(previewKey) && comparison_digest(previewKey).valid()
				   && comparison_digest(previewKey) != keyDigest,
			   "a public-preview comparison never shares a cache key with the "
			   "authenticated comparison of the same files");

		auto variants = std::vector<file_comparison_key>{ };
		variants.push_back(key);
		variants.back().Authority.Repository = repository_id{ 102U };
		variants.back().Repository           = repository_id{ 102U };
		variants.push_back(key);
		variants.back().Authority.PullRequest = pull_request_id{ 203U };
		variants.back().PullRequest           = pull_request_id{ 203U };
		variants.push_back(key);
		variants.back().LeftAnchor.Epoch = epoch_ordinal{ 3 };
		variants.push_back(key);
		variants.back().RightBlob = blob(52U);
		variants.push_back(key);
		variants.back().File = opaque<file_identity>(32U);
		variants.push_back(key);
		variants.back().Options = opaque<diff_options_digest>(33U);
		variants.push_back(key);
		variants.back().Authority.Account = github_account_id{ 41U };
		for (const auto& variant : variants) {
			expect(valid(variant) && comparison_digest(variant) != keyDigest,
				   "every exact cache-key dimension but the algorithm changes comparison identity");
		}

		// RFC-BPR-0089. A commit-filtered key names its two commits in the
		// comparison digest, never in the material, and only when present.
		const auto commit = [](const std::uint8_t aSeed) {
			auto bytes = std::array<std::uint8_t, 32U>{ };
			bytes[0]   = aSeed;
			return git_commit_oid{ git_hash_algorithm::sha1, bytes };
		};
		auto filtered   = key;
		filtered.Filter = filtered_diff_endpoints{ .Left = commit(1U), .Right = commit(2U) };
		expect(valid(filtered) && comparison_digest(filtered).valid() && comparison_digest(filtered) != keyDigest,
			   "a filtered diff is a different comparison from the unfiltered one");
		expect(file_material(filtered) == file_material(key), "a filtered diff with the same blobs has the same material");
		auto otherEnds   = filtered;
		otherEnds.Filter = filtered_diff_endpoints{ .Left = commit(1U), .Right = commit(3U) };
		expect(comparison_digest(otherEnds) != comparison_digest(filtered), "different endpoint commits are different comparisons");
		auto sameEnds   = filtered;
		sameEnds.Filter = filtered_diff_endpoints{ .Left = commit(4U), .Right = commit(4U) };
		expect(!valid(sameEnds) && !comparison_digest(sameEnds).valid(), "a filtered diff between one commit and itself is refused");
		auto missingEnd   = filtered;
		missingEnd.Filter = filtered_diff_endpoints{ .Left = commit(1U), .Right = git_commit_oid{ } };
		expect(!valid(missingEnd), "both filtered endpoints must be commits");
		const auto receipt = filtered_receipt_file(key.File, *filtered.Filter);
		expect(receipt.valid() && receipt != key.File && receipt == filtered_receipt_file(key.File, *filtered.Filter)
				   && receipt != filtered_receipt_file(key.File, *otherEnds.Filter)
				   && receipt != filtered_receipt_file(opaque<file_identity>(32U), *filtered.Filter),
			   "a filtered diff keeps its own receipt, keyed by the file and its two commits");
		expect(!filtered_receipt_file(key.File, *sameEnds.Filter).valid(), "no receipt key for invalid endpoints");

		auto contradictory       = key;
		contradictory.Repository = repository_id{ 102U };
		expect(!valid(contradictory),
			   "repository field must agree with full pull-request authority");
		contradictory                      = key;
		contradictory.LeftAnchor.Authority = opaque<pull_request_authority_scope_id>(90U);
		expect(!valid(contradictory),
			   "both anchors must belong to the exact pull-request authority");

		auto addedFile = key;
		addedFile.LeftBlob.reset();
		expect(valid(addedFile) && comparison_digest(addedFile).valid(),
			   "one absent blob has exact added-file identity");
		auto noBlobs = addedFile;
		noBlobs.RightBlob.reset();
		expect(!valid(noBlobs) && !comparison_digest(noBlobs).valid(),
			   "comparison without either blob is rejected");
		auto mixedHashes      = key;
		mixedHashes.RightBlob = blob(52U, git_hash_algorithm::sha256);
		expect(!valid(mixedHashes),
			   "two present blobs must use the same object algorithm");
	}

	// [[BPR-1331]]. A push advances both anchors' epochs. The comparison
	// digest names them, so it moves for every file; the material a receipt
	// attaches to does not, unless a blob moved. Through the real carry-over,
	// the private progress mark a file shows after a push follows its bytes.
	auto test_file_material_outlives_a_push() -> void
	{
		const auto key           = comparison_key();
		auto       pushed        = key;
		pushed.LeftAnchor.Epoch  = epoch_ordinal{ 3 };
		pushed.RightAnchor.Epoch = epoch_ordinal{ 4 };
		expect(valid(pushed) && comparison_digest(pushed) != comparison_digest(key),
			   "a push moves every file's comparison digest");
		expect(file_material(key).valid() && file_material(pushed) == file_material(key),
			   "a push keeps the material of a file whose blobs did not move");
		expect(file_material(key) != comparison_digest(key),
			   "material and comparison are separate identities");
		auto changed      = pushed;
		changed.RightBlob = blob(52U);
		expect(file_material(changed) != file_material(key),
			   "a push that changes the file's bytes moves its material");
		auto rebased     = pushed;
		rebased.LeftBlob = blob(22U);
		expect(file_material(rebased) != file_material(key),
			   "a new base blob is new compared material");

		auto variants = std::vector<file_comparison_key>{ };
		variants.push_back(key);
		variants.back().Authority.Repository = repository_id{ 102U };
		variants.back().Repository           = repository_id{ 102U };
		variants.push_back(key);
		variants.back().Authority.PullRequest = pull_request_id{ 203U };
		variants.back().PullRequest           = pull_request_id{ 203U };
		variants.push_back(key);
		variants.back().File = opaque<file_identity>(32U);
		variants.push_back(key);
		variants.back().Options = opaque<diff_options_digest>(33U);
		variants.push_back(key);
		variants.back().Authority.Account = github_account_id{ 41U };
		for (const auto& variant : variants) {
			expect(valid(variant) && file_material(variant) != file_material(key),
				   "every dimension but the anchors and the algorithm changes the material");
		}
		auto noBlobs = key;
		noBlobs.LeftBlob.reset();
		noBlobs.RightBlob.reset();
		expect(!file_material(noBlobs).valid(), "an invalid key has no material");

		// The receipts a reviewer left on the first head, carried to the push.
		const auto file     = key.File;
		auto       accepted = file_receipt{ .File = file, .Material = file_material(key), .Read = file_read_state::read, .Accepted = true };
		const auto kept     = carry_over(accepted, { .File = file, .Material = file_material(pushed) });
		expect(kept.Disposition == receipt_carry_over::retained && progress_mark(kept.Receipt) == file_progress_mark::accepted,
			   "an accepted file the push did not change stays accepted");
		const auto reopened = carry_over(accepted, { .File = file, .Material = file_material(changed) });
		expect(reopened.Disposition == receipt_carry_over::reopened_material_changed
				   && reopened.Receipt.Read == file_read_state::unread && !reopened.Receipt.Accepted
				   && progress_mark(reopened.Receipt) == file_progress_mark::unread,
			   "an accepted file the push changed reopens unread, not seen, and loses its acceptance");
		auto       excluded      = file_receipt{ .File = file, .Material = file_material(key), .Read = file_read_state::seen, .Excluded = true };
		const auto stillExcluded = carry_over(excluded, { .File = file, .Material = file_material(changed) });
		expect(stillExcluded.Disposition == receipt_carry_over::reopened_material_changed
				   && progress_mark(stillExcluded.Receipt) == file_progress_mark::excluded,
			   "an excluded file stays excluded through a push that changes it");
		const auto byComparison = carry_over(file_receipt{ .File = file, .Material = comparison_digest(key), .Read = file_read_state::read, .Accepted = true },
											 { .File = file, .Material = comparison_digest(pushed) });
		expect(byComparison.Disposition == receipt_carry_over::reopened_material_changed,
			   "keyed by the comparison digest, an unchanged file would reopen on every push");
	}

	// [[BPR-1497]]. Moving to linked xdiff bumped the algorithm revision to 2.
	// The comparison digest and the material are persisted beside stored line
	// coordinates (receipts, defects, drafts, private notes and threads,
	// tours, window restoration), so both must be byte-identical to what
	// revision 1 wrote. The vectors are the ones revision 1 produced for this
	// exact key, captured from the engine before the bump.
	auto test_stored_identities_survive_the_algorithm_revision() -> void
	{
		expect(supported_diff_algorithm_revision == diff_algorithm_revision{ 2U },
			   "linked xdiff computes algorithm revision 2");
		const auto stored = comparison_key();
		expect(stored.Algorithm == diff_algorithm_revision{ 1U },
			   "the fixture key is the one revision 1 stored");
		auto current      = stored;
		current.Algorithm = supported_diff_algorithm_revision;
		expect(valid(current) && current != stored,
			   "the current key still names its algorithm, so a revision-1 result never satisfies it");
		expect(comparison_digest(current) == comparison_digest(stored)
				   && comparison_digest(current).bytes()
					   == digest_bytes(
						   "fbdfe1e651fe7e0227040953c236b296"
						   "032ddcf3f30503c7107d6d8037f588ae"),
			   "a comparison digest stored by revision 1 still names the same comparison");
		expect(file_material(current) == file_material(stored)
				   && file_material(current).bytes()
					   == digest_bytes(
						   "606e433a03b07aa3c81bbe15b2d85486"
						   "bbaffd152fd34570a39648cd0118badf"),
			   "a material stored by revision 1 still names the same material");

		// A receipt written under revision 1 meets the current material.
		auto       accepted = file_receipt{ .File = stored.File, .Material = file_material(stored), .Read = file_read_state::read, .Accepted = true };
		const auto kept     = carry_over(accepted, { .File = current.File, .Material = file_material(current) });
		expect(kept.Disposition == receipt_carry_over::retained && progress_mark(kept.Receipt) == file_progress_mark::accepted,
			   "an accepted file stays accepted across the algorithm revision");

		// A line anchor written under revision 1 still names a line of the
		// current comparison.
		const auto anchor = source_line_ref{ .Comparison = comparison_digest(stored), .Side = diff_side::right, .Line = 4U };
		expect(anchor.Comparison == comparison_digest(current) && valid(anchor, 8U, 8U),
			   "a stored line anchor still binds the current comparison");
	}

	auto test_canonical_record_tables() -> void
	{
		const auto empty       = equal_alignment(0U);
		const auto equal       = equal_alignment();
		const auto insertion   = insertion_alignment();
		const auto deletion    = deletion_alignment();
		const auto replacement = replacement_alignment();
		const auto moved       = move_alignment();
		const auto twoMoves    = two_move_alignment();
		const auto mixed       = mixed_alignment();
		for (const auto* alignment :
			 { &empty,
			   &equal,
			   &insertion,
			   &deletion,
			   &replacement,
			   &moved,
			   &twoMoves,
			   &mixed }) {
			expect(validate_alignment(*alignment).valid(),
				   "canonical table alignment is accepted");
		}

		expect(insertion.Segments[1].Left.empty()
				   && insertion.Segments[1].Right.extent() == 2U,
			   "right-only insertion retains an empty left span");
		expect(deletion.Segments[1].Right.empty()
				   && deletion.Segments[1].Left.extent() == 2U,
			   "left-only deletion retains an empty right span");
		expect(moved.Moves.size() == 1U
				   && moved.Segments[0].Kind == alignment_kind::left_only
				   && moved.Segments[2].Kind == alignment_kind::right_only,
			   "move metadata leaves delete and insert in the primary path");
		expect(mixed.Groups[0].Kind == change_summary_kind::mixed
				   && mixed.Groups[0].AddedLines == 2U
				   && mixed.Groups[0].DeletedLines == 1U,
			   "group summary is derived from one ordered member interval");

		auto addedFile = alignment_with_counts(0U, 2U);
		addedFile.Key.LeftBlob.reset();
		addedFile.Digest = comparison_digest(addedFile.Key);
		append_segment(
			addedFile, alignment_kind::right_only, { 0U, 0U }, { 0U, 2U });
		append_group(addedFile, 0U, 1U);
		expect(validate_alignment(addedFile).valid(),
			   "an entire added file uses a boundary, not fake left lines");

		auto binary            = alignment_with_counts(0U, 0U);
		binary.Content         = diff_content_state::binary;
		binary.Diagnostic.Code = diff_reason_code::binary_file;
		expect(validate_alignment(binary).valid(),
			   "binary diagnostic publishes no fake semantic records");
		auto unavailable            = alignment_with_counts(0U, 0U);
		unavailable.Content         = diff_content_state::unavailable;
		unavailable.Diagnostic.Code = diff_reason_code::decode_failure;
		expect(validate_alignment(unavailable).valid(),
			   "unavailable diagnostic publishes no fake semantic records");

		const auto maximum  = std::numeric_limits<std::uint32_t>::max();
		auto       boundary = alignment_with_counts(maximum, maximum);
		append_segment(
			boundary, alignment_kind::equal, { 0U, maximum }, { 0U, maximum });
		expect(boundary.Segments[0].Left.extent()
					   == static_cast<std::uint64_t>(maximum)
				   && validate_alignment(boundary).valid(),
			   "maximum uint32 line boundary validates without count overflow");
	}

	auto test_source_references() -> void
	{
		const auto digest = comparison_digest(comparison_key());
		expect(valid(source_line_ref{ digest, diff_side::left, 2U }, 3U, 5U),
			   "left source line is bounded by left document");
		expect(!valid(source_line_ref{ digest, diff_side::left, 3U }, 3U, 5U),
			   "line coordinate is exclusive at line count");
		expect(valid(source_boundary_ref{ digest, diff_side::right, 5U }, 3U, 5U),
			   "boundary coordinate includes document end");
		expect(!valid(source_boundary_ref{ digest, diff_side::right, 6U }, 3U, 5U),
			   "boundary beyond document end is rejected");
		expect(!valid(source_line_ref{ { }, diff_side::right, 0U }, 1U, 1U),
			   "source reference requires exact comparison identity");
		expect(!valid(source_boundary_ref{
						  digest, static_cast<diff_side>(255U), 0U },
					  1U, 1U),
			   "unknown side is rejected");
	}

	auto test_stable_record_identities() -> void
	{
		const auto digest  = comparison_digest(comparison_key());
		const auto segment = segment_identity(digest,
											  alignment_kind::replacement,
											  { 2U, 5U },
											  { 2U, 7U },
											  4U);
		expect(segment.valid()
				   && segment
					   == segment_identity(digest,
										   alignment_kind::replacement,
										   { 2U, 5U },
										   { 2U, 7U },
										   4U),
			   "segment identity replays from canonical comparison and structure");
		// This vector and the three below it derive from the comparison digest
		// of an authenticated key, so they hold across BPR-0531 for the same
		// reason that digest does. A drift here would mean the authenticated
		// encoding moved.
		expect(segment.bytes()
				   == digest_bytes(
					   "612fe655afb24746886fcb5c4b8f13db"
					   "b91f2fd6eec227a00a547c11f278ff7c"),
			   "segment identity matches its known-answer vector");
		expect(segment
					   != segment_identity(digest,
										   alignment_kind::replacement,
										   { 2U, 5U },
										   { 2U, 7U },
										   5U)
				   && segment
					   != segment_identity(digest,
										   alignment_kind::replacement,
										   { 2U, 5U },
										   { 2U, 8U },
										   4U),
			   "segment ordinal and spans are identity dimensions");
		expect(!segment_identity(digest,
								 alignment_kind::right_only,
								 { 0U, 0U },
								 { 0U, 0U },
								 0U)
					.valid(),
			   "identity factory rejects a both-empty segment");

		const auto pair = pair_identity(digest, segment, 2U, 3U, line_pair_kind::similar, 0U);
		expect(pair.valid()
				   && pair
					   == pair_identity(
						   digest, segment, 2U, 3U, line_pair_kind::similar, 0U)
				   && pair
					   != pair_identity(
						   digest, segment, 2U, 3U, line_pair_kind::exact, 0U),
			   "pair identity includes segment, coordinates, confidence, and ordinal");
		expect(pair.bytes()
				   == digest_bytes(
					   "a26bd4f10537260fdd68aae38d8c6eb5"
					   "8c077f84a577fa2296612fa86d1a482d"),
			   "pair identity matches its known-answer vector");

		const auto group = group_identity(digest, { 2U, 5U }, { 2U, 7U }, 0U);
		expect(group.valid()
				   && group == group_identity(digest, { 2U, 5U }, { 2U, 7U }, 0U)
				   && group != group_identity(digest, { 2U, 5U }, { 2U, 8U }, 0U),
			   "group identity replays from canonical covering spans");
		expect(group.bytes()
				   == digest_bytes(
					   "5f0c01c652486a98000ad5c03c9e9976"
					   "b6729cfd200609a5928484ca233af438"),
			   "group identity matches its known-answer vector");

		const auto destination = segment_identity(digest,
												  alignment_kind::right_only,
												  { 7U, 7U },
												  { 9U, 11U },
												  7U);
		const auto move        = move_identity(digest, segment, destination, 0U);
		expect(move.valid()
				   && move == move_identity(digest, segment, destination, 0U)
				   && move != move_identity(digest, destination, segment, 0U),
			   "move identity includes directed endpoints");
		expect(move.bytes()
				   == digest_bytes(
					   "3f5d03bd834367aae2d5152f3fecde86"
					   "50f2876081b3f2c7d45c3f532828585f"),
			   "move identity matches its known-answer vector");
	}

	auto test_validator_failures() -> void
	{
		{
			auto broken           = equal_alignment();
			broken.Key.Repository = { };
			expect_error(broken,
						 alignment_validation_error::invalid_comparison_key,
						 "invalid key is rejected");
		}
		{
			auto broken   = equal_alignment();
			broken.Digest = opaque<file_comparison_digest>(91U);
			expect_error(broken,
						 alignment_validation_error::comparison_digest_mismatch,
						 "comparison digest mismatch is rejected");
		}
		{
			auto broken                 = equal_alignment();
			broken.Options.ContextLines = 4U;
			expect_error(broken,
						 alignment_validation_error::options_digest_mismatch,
						 "published normalized options must match the cache-key digest");
		}
		{
			auto broken            = equal_alignment();
			broken.Diagnostic.Code = diff_reason_code::binary_file;
			expect_error(broken,
						 alignment_validation_error::invalid_content_diagnostic,
						 "text result cannot carry binary diagnostic");
		}
		{
			auto broken            = equal_alignment();
			broken.Content         = diff_content_state::binary;
			broken.Diagnostic.Code = diff_reason_code::binary_file;
			expect_error(broken,
						 alignment_validation_error::records_for_non_text,
						 "binary result cannot retain text records");
		}
		{
			auto broken            = alignment_with_counts(1U, 0U);
			broken.Content         = diff_content_state::unavailable;
			broken.Diagnostic.Code = diff_reason_code::decode_failure;
			expect_error(broken,
						 alignment_validation_error::records_for_non_text,
						 "non-text diagnostic cannot retain logical line counts");
		}
		{
			auto broken = alignment_with_counts(0U, 1U);
			broken.Key.RightBlob.reset();
			broken.Digest = comparison_digest(broken.Key);
			expect_error(broken,
						 alignment_validation_error::invalid_content_diagnostic,
						 "absent blob cannot claim source lines");
		}
		{
			auto broken = alignment_with_counts(1U, 1U);
			expect_error(broken,
						 alignment_validation_error::missing_text_segments,
						 "nonempty text requires covering segments");
		}
		{
			auto broken = equal_alignment(0U);
			broken.Groups.push_back(change_group{ });
			expect_error(broken,
						 alignment_validation_error::records_without_segments,
						 "empty text cannot bypass orphan semantic-record rejection");
		}
		{
			auto disabled                        = diff_options{ };
			disabled.Pairing                     = pairing_policy::disabled;
			disabled.SimilarityThresholdPermille = 0U;
			auto broken                          = alignment_with_counts(2U, 2U, disabled);
			append_segment(
				broken, alignment_kind::replacement, { 0U, 2U }, { 0U, 2U });
			append_group(broken, 0U, 1U);
			append_pair(broken, 0U, 0U, 0U, line_pair_kind::exact);
			expect_error(broken,
						 alignment_validation_error::records_disabled_by_options,
						 "disabled pairing policy rejects pair records");
		}
		{
			auto broken = move_alignment(diff_options{ });
			expect_error(broken,
						 alignment_validation_error::records_disabled_by_options,
						 "disabled move policy rejects move records");
		}
		{
			auto broken = alignment_with_counts(0U, 0U);
			broken.Key.LeftBlob.reset();
			broken.Digest          = comparison_digest(broken.Key);
			broken.Content         = diff_content_state::unavailable;
			broken.Diagnostic.Code = diff_reason_code::missing_left_blob;
			expect_error(broken,
						 alignment_validation_error::invalid_content_diagnostic,
						 "absent-file identity is not an acquisition-missing diagnostic");
		}
		{
			auto broken           = equal_alignment();
			broken.Segments[0].Id = opaque<alignment_segment_id>(92U);
			expect_error(broken,
						 alignment_validation_error::invalid_segment_id,
						 "segment identity mismatch is rejected");
		}
		{
			auto broken             = equal_alignment();
			broken.Segments[0].Left = { 2U, 1U };
			expect_error(broken,
						 alignment_validation_error::invalid_segment_span,
						 "reversed segment span is rejected");
		}
		{
			auto broken              = equal_alignment();
			broken.Segments[0].Left  = { 1U, 3U };
			broken.Segments[0].Right = { 1U, 3U };
			broken.Segments[0].Id    = segment_identity(broken.Digest,
														broken.Segments[0].Kind,
														broken.Segments[0].Left,
														broken.Segments[0].Right,
														0U);
			expect_error(broken,
						 alignment_validation_error::left_coverage_discontinuity,
						 "left coverage gap is rejected");
		}
		{
			auto broken                    = equal_alignment();
			broken.Segments[0].Right.Begin = 1U;
			expect_error(broken,
						 alignment_validation_error::right_coverage_discontinuity,
						 "right coverage gap is rejected");
		}
		{
			auto broken              = equal_alignment();
			broken.Segments[0].Left  = { 0U, 0U };
			broken.Segments[0].Right = { 0U, 0U };
			expect_error(broken,
						 alignment_validation_error::empty_segment,
						 "both-empty segment is rejected");
		}
		{
			auto broken             = equal_alignment();
			broken.Segments[0].Kind = alignment_kind::left_only;
			expect_error(broken,
						 alignment_validation_error::segment_kind_mismatch,
						 "kind and span shape must agree");
		}
		{
			auto broken = insertion_alignment();
			broken.Segments[1].Group.reset();
			expect_error(broken,
						 alignment_validation_error::ungrouped_change,
						 "every changed segment belongs to a group");
		}
		{
			auto broken              = equal_alignment();
			broken.Segments[0].Group = opaque<change_group_id>(93U);
			expect_error(broken,
						 alignment_validation_error::grouped_equal_segment,
						 "equal segment cannot belong to change group");
		}
		{
			auto broken = alignment_with_counts(4U, 4U);
			append_segment(
				broken, alignment_kind::equal, { 0U, 2U }, { 0U, 2U });
			append_segment(
				broken, alignment_kind::equal, { 2U, 4U }, { 2U, 4U });
			expect_error(broken,
						 alignment_validation_error::noncanonical_adjacent_segment,
						 "adjacent equal operations must be coalesced");
		}
		{
			auto broken = alignment_with_counts(4U, 4U);
			append_segment(
				broken, alignment_kind::replacement, { 0U, 2U }, { 0U, 2U });
			append_segment(
				broken, alignment_kind::replacement, { 2U, 4U }, { 2U, 4U });
			append_group(broken, 0U, 2U);
			expect_error(broken,
						 alignment_validation_error::noncanonical_adjacent_segment,
						 "unrefined adjacent replacements must be coalesced");
		}
		{
			auto refined = alignment_with_counts(4U, 4U);
			append_segment(
				refined, alignment_kind::replacement, { 0U, 2U }, { 0U, 2U });
			append_segment(
				refined, alignment_kind::replacement, { 2U, 4U }, { 2U, 4U });
			append_group(refined, 0U, 2U);
			append_pair(refined, 1U, 2U, 2U, line_pair_kind::exact);
			expect(validate_alignment(refined).valid(),
				   "accepted pair metadata retains a replacement refinement");
		}
		{
			auto broken = alignment_with_counts(2U, 0U);
			append_segment(
				broken, alignment_kind::left_only, { 0U, 1U }, { 0U, 0U });
			append_segment(
				broken, alignment_kind::left_only, { 1U, 2U }, { 0U, 0U });
			append_group(broken, 0U, 2U);
			expect_error(broken,
						 alignment_validation_error::noncanonical_adjacent_segment,
						 "mergeable adjacent deletions must be coalesced");
		}
		{
			auto broken = alignment_with_counts(0U, 2U);
			append_segment(
				broken, alignment_kind::right_only, { 0U, 0U }, { 0U, 1U });
			append_segment(
				broken, alignment_kind::right_only, { 0U, 0U }, { 1U, 2U });
			append_group(broken, 0U, 2U);
			expect_error(broken,
						 alignment_validation_error::noncanonical_adjacent_segment,
						 "mergeable adjacent insertions must be coalesced");
		}
		{
			auto refined = alignment_with_counts(5U, 4U, move_options());
			append_segment(
				refined, alignment_kind::left_only, { 0U, 1U }, { 0U, 0U });
			append_segment(
				refined, alignment_kind::left_only, { 1U, 2U }, { 0U, 0U });
			append_segment(
				refined, alignment_kind::equal, { 2U, 3U }, { 0U, 1U });
			append_segment(
				refined, alignment_kind::right_only, { 3U, 3U }, { 1U, 2U });
			append_segment(
				refined, alignment_kind::equal, { 3U, 5U }, { 2U, 4U });
			append_group(refined, 0U, 2U);
			append_group(refined, 3U, 4U);
			append_move(refined, 0U, 3U);
			expect(validate_alignment(refined).valid(),
				   "move metadata retains a one-sided refinement boundary");
		}
		{
			auto broken = alignment_with_counts(3U, 3U);
			append_segment(
				broken, alignment_kind::equal, { 0U, 2U }, { 0U, 2U });
			expect_error(broken,
						 alignment_validation_error::incomplete_left_coverage,
						 "incomplete final coverage is rejected");
		}
		{
			auto broken                  = insertion_alignment();
			broken.Groups[0].Members.End = 4U;
			expect_error(broken,
						 alignment_validation_error::invalid_group_span,
						 "group member interval is bounded");
		}
		{
			auto broken                  = insertion_alignment();
			broken.Groups[0].Members.End = broken.Groups[0].Members.Begin;
			expect_error(broken,
						 alignment_validation_error::empty_group,
						 "empty group is rejected");
		}
		{
			auto broken              = move_alignment();
			broken.Groups[1].Members = broken.Groups[0].Members;
			expect_error(broken,
						 alignment_validation_error::unordered_group,
						 "group intervals must be ordered");
		}
		{
			auto broken = alignment_with_counts(1U, 2U);
			append_segment(
				broken, alignment_kind::left_only, { 0U, 1U }, { 0U, 0U });
			append_segment(
				broken, alignment_kind::right_only, { 1U, 1U }, { 0U, 2U });
			append_group(broken, 0U, 1U);
			append_group(broken, 1U, 2U);
			expect_error(broken,
						 alignment_validation_error::noncanonical_adjacent_group,
						 "one touching changed run cannot publish competing groups");
		}
		{
			auto broken                 = insertion_alignment();
			broken.Groups[0].Left.Begin = 0U;
			expect_error(broken,
						 alignment_validation_error::group_span_mismatch,
						 "group covering span is derived from its members");
		}
		{
			auto broken                 = insertion_alignment();
			broken.Groups[0].First.Left = 0U;
			expect_error(broken,
						 alignment_validation_error::group_boundary_mismatch,
						 "group comparison boundary is derived from its members");
		}
		{
			auto broken = insertion_alignment();
			++broken.Groups[0].AddedLines;
			expect_error(broken,
						 alignment_validation_error::group_summary_mismatch,
						 "group summary counts are derived from members");
		}
		{
			auto broken              = insertion_alignment();
			broken.Groups[0].Id      = opaque<change_group_id>(99U);
			broken.Segments[1].Group = broken.Groups[0].Id;
			expect_error(broken,
						 alignment_validation_error::invalid_group_id,
						 "group identity mismatch is rejected");
		}
		{
			auto broken              = mixed_alignment();
			broken.Segments[1].Group = opaque<change_group_id>(94U);
			expect_error(broken,
						 alignment_validation_error::noncontiguous_group,
						 "group member IDs must agree with the contiguous range");
		}
		{
			auto broken = move_alignment();
			broken.Groups.pop_back();
			expect_error(broken,
						 alignment_validation_error::segment_group_mismatch,
						 "changed segment cannot name an unpublished group");
		}
		{
			auto broken          = replacement_alignment();
			broken.Pairs[0].Kind = static_cast<line_pair_kind>(255U);
			expect_error(broken,
						 alignment_validation_error::invalid_pair_kind,
						 "unknown pair kind is rejected");
		}
		{
			auto broken             = replacement_alignment();
			broken.Pairs[0].Segment = opaque<alignment_segment_id>(95U);
			refresh_pair_ids(broken);
			expect_error(broken,
						 alignment_validation_error::unknown_pair_segment,
						 "pair must reference a published segment");
		}
		{
			auto broken              = replacement_alignment();
			broken.Pairs[0].LeftLine = 3U;
			refresh_pair_ids(broken);
			expect_error(broken,
						 alignment_validation_error::pair_out_of_bounds,
						 "pair coordinates remain inside replacement segment");
		}
		{
			auto broken               = replacement_alignment();
			broken.Pairs[0].Segment   = broken.Segments[0].Id;
			broken.Pairs[0].LeftLine  = 0U;
			broken.Pairs[0].RightLine = 0U;
			refresh_pair_ids(broken);
			expect_error(broken,
						 alignment_validation_error::pair_out_of_bounds,
						 "secondary pair cannot attach to an equal segment");
		}
		{
			auto broken = replacement_alignment();
			std::swap(broken.Pairs[0], broken.Pairs[1]);
			refresh_pair_ids(broken);
			expect_error(broken,
						 alignment_validation_error::nonmonotonic_pair,
						 "secondary pairs are strictly monotonic");
		}
		{
			auto broken        = replacement_alignment();
			broken.Pairs[0].Id = opaque<line_pair_id>(96U);
			expect_error(broken,
						 alignment_validation_error::invalid_pair_id,
						 "pair identity mismatch is rejected");
		}
		{
			auto broken                    = move_alignment();
			broken.Moves[0].SourceDeletion = opaque<alignment_segment_id>(97U);
			refresh_move_ids(broken);
			expect_error(broken,
						 alignment_validation_error::unknown_move_segment,
						 "move endpoint must reference a published segment");
		}
		{
			auto broken                    = move_alignment();
			broken.Moves[0].SourceDeletion = broken.Segments[2].Id;
			refresh_move_ids(broken);
			expect_error(broken,
						 alignment_validation_error::invalid_move_topology,
						 "move source must be deletion and destination insertion");
		}
		{
			auto broken = mixed_alignment(move_options());
			append_move(broken, 0U, 1U);
			expect_error(broken,
						 alignment_validation_error::invalid_move_topology,
						 "exact move endpoints require equal line extents");
		}
		{
			auto broken = move_alignment();
			append_move(broken, 0U, 2U);
			expect_error(broken,
						 alignment_validation_error::duplicate_move_relation,
						 "duplicate move relation is rejected");
		}
		{
			auto broken = two_move_alignment();
			std::swap(broken.Moves[0], broken.Moves[1]);
			refresh_move_ids(broken);
			expect_error(broken,
						 alignment_validation_error::noncanonical_move_order,
						 "move relations use canonical source/destination order");
		}
		{
			auto broken                    = two_move_alignment();
			broken.Moves[1].SourceDeletion = broken.Moves[0].SourceDeletion;
			refresh_move_ids(broken);
			expect_error(broken,
						 alignment_validation_error::invalid_move_topology,
						 "exact move endpoints participate in at most one relation");
		}
		{
			auto broken        = move_alignment();
			broken.Moves[0].Id = opaque<move_relation_id>(98U);
			expect_error(broken,
						 alignment_validation_error::invalid_move_id,
						 "move identity mismatch is rejected");
		}
	}

	class deterministic_sequence final
	{
	public:
		explicit deterministic_sequence(const std::uint32_t aSeed)
			: State{ aSeed }
		{
		}

		[[nodiscard]] auto next() noexcept -> std::uint32_t
		{
			State = State * 1'664'525U + 1'013'904'223U;
			return State;
		}

	private:
		std::uint32_t State;
	};

	[[nodiscard]] auto generated_alignment(const std::uint32_t aSeed)
		-> file_alignment
	{
		auto       sequence  = deterministic_sequence{ aSeed };
		auto       result    = alignment_with_counts(0U, 0U);
		auto       left      = std::uint32_t{ 0U };
		auto       right     = std::uint32_t{ 0U };
		auto       priorKind = std::optional<alignment_kind>{ };
		const auto count     = 1U + sequence.next() % 32U;
		for (auto index = std::uint32_t{ 0U }; index < count; ++index) {
			auto kindValue = sequence.next() % 4U;
			if (priorKind.has_value()
				&& kindValue == static_cast<std::uint32_t>(*priorKind)) {
				kindValue = (kindValue + 1U) % 4U;
			}
			const auto kind         = static_cast<alignment_kind>(kindValue);
			const auto firstExtent  = 1U + sequence.next() % 4U;
			const auto secondExtent = 1U + sequence.next() % 4U;
			auto       leftExtent   = std::uint32_t{ 0U };
			auto       rightExtent  = std::uint32_t{ 0U };
			switch (kind) {
			case alignment_kind::equal:
				leftExtent  = firstExtent;
				rightExtent = firstExtent;
				break;
			case alignment_kind::replacement:
				leftExtent  = firstExtent;
				rightExtent = secondExtent;
				break;
			case alignment_kind::left_only:
				leftExtent = firstExtent;
				break;
			case alignment_kind::right_only:
				rightExtent = secondExtent;
				break;
			}
			append_segment(result,
						   kind,
						   { left, static_cast<std::uint32_t>(left + leftExtent) },
						   { right, static_cast<std::uint32_t>(right + rightExtent) });
			left      = static_cast<std::uint32_t>(left + leftExtent);
			right     = static_cast<std::uint32_t>(right + rightExtent);
			priorKind = kind;
		}
		result.LeftLineCount  = left;
		result.RightLineCount = right;

		auto groupBegin = std::optional<std::uint32_t>{ };
		for (auto index = std::uint32_t{ 0U }; index < count; ++index) {
			const auto changed = result.Segments[static_cast<std::size_t>(index)].Kind
				!= alignment_kind::equal;
			if (changed && !groupBegin.has_value()) {
				groupBegin = index;
			}
			if (!changed && groupBegin.has_value()) {
				append_group(result, *groupBegin, index);
				groupBegin.reset();
			}
		}
		if (groupBegin.has_value()) {
			append_group(result, *groupBegin, count);
		}
		for (auto index = std::uint32_t{ 0U }; index < count; ++index) {
			const auto& segment = result.Segments[static_cast<std::size_t>(index)];
			if (segment.Kind == alignment_kind::replacement) {
				append_pair(result,
							index,
							segment.Left.Begin,
							segment.Right.Begin,
							(sequence.next() & 1U) == 0U ? line_pair_kind::exact
														 : line_pair_kind::similar);
			}
		}
		return result;
	}

	auto test_generated_replay_and_rejection() -> void
	{
		for (auto seed = std::uint32_t{ 1U }; seed <= 512U; ++seed) {
			const auto first  = generated_alignment(seed);
			const auto replay = generated_alignment(seed);
			expect(first == replay,
				   "fixed-seed generated alignment replays byte-for-byte");
			expect(validate_alignment(first).valid(),
				   "fixed-seed generated canonical alignment validates");

			auto malformed = first;
			if (!malformed.Segments[0].Left.empty()) {
				++malformed.Segments[0].Left.Begin;
			} else {
				++malformed.Segments[0].Right.Begin;
			}
			expect(!validate_alignment(malformed).valid(),
				   "fixed-seed malformed alignment is rejected");
		}
	}
	auto test_file_identity_is_derived_from_the_path() -> void
	{
		const auto alpha = file_identity_of("src/app/main.cpp");
		const auto beta  = file_identity_of("src/app/other.cpp");
		expect(alpha.valid() && beta.valid(),
			   "an ordinary repository path derives a usable identity");
		expect(alpha != beta,
			   "two different paths derive two different identities");
		expect(alpha == file_identity_of("src/app/main.cpp"),
			   "the same path derives the same identity");

		// Pinned to a recorded value rather than recomputed beside the
		// assertion. Recomputing would agree with any derivation, including a
		// changed one, and this identity is the key of RFC-BPR-0007's durable
		// receipts: a reviewer's stored progress stops matching their files on
		// the day this encoding silently moves. The value is
		// sha256("BPR-file-identity-v1" || u32be(16) || "src/app/main.cpp").
		constexpr auto recorded = digest_bytes(
			"5977615b7e623569e66c5aaecf6f4569b2b90b0b9da630dda22abe838116bfbf");
		expect(alpha.bytes() == recorded,
			   "the derived identity is byte-for-byte the recorded encoding, "
			   "so a change to the domain, the length prefix, or the ordering "
			   "is caught rather than silently re-keying every receipt");

		// Two paths with the same bytes and the same length, differing only in
		// where the separator falls.
		//
		// This does NOT test the length prefix, and an earlier comment here
		// claimed it did. With a fixed domain and the path as the final field,
		// no two distinct paths can encode identically whatever the prefix
		// says, so the prefix is defensive against the day a further field is
		// appended rather than load-bearing today. The recorded-digest
		// assertion above is what actually pins the encoding.
		expect(file_identity_of("a/bc") != file_identity_of("ab/c"),
			   "paths differing only in separator placement differ");

		expect(!file_identity_of("").valid(),
			   "an empty path names no file and derives no identity");
		expect(!file_identity_of(
					std::string(maximum_identified_path_bytes + 1U, 'p'))
					.valid(),
			   "a path beyond the published bound is refused, never truncated");
		expect(file_identity_of(
				   std::string(maximum_identified_path_bytes, 'p'))
				   .valid(),
			   "the published bound itself is usable, so it is inclusive");
	}

	auto test_a_rename_produces_a_new_identity() -> void
	{
		// This is the consequence of deriving from the path, and it is asserted
		// so that changing it is a deliberate act rather than a quiet drift.
		//
		// A reviewer who marked `old/name.cpp` read sees the file unread again
		// once it is renamed, even though no byte of it changed. That cost was
		// accepted: the alternative carried identity across a rename by
		// consuming the host's reported previous path, which makes identity
		// depend on the review snapshot rather than on the file. Two callers
		// holding different snapshots would then derive different identities
		// for the same file and the receipt key would stop being well defined.
		expect(file_identity_of("old/name.cpp")
				   != file_identity_of("new/name.cpp"),
			   "a rename produces a new identity, so a receipt does not follow "
			   "a file to its new path");
	}
} // namespace

int main()
{
	test_options_and_comparison_identity();
	test_file_material_outlives_a_push();
	test_stored_identities_survive_the_algorithm_revision();
	test_canonical_record_tables();
	test_source_references();
	test_stable_record_identities();
	test_validator_failures();
	test_generated_replay_and_rejection();
	test_file_identity_is_derived_from_the_path();
	test_a_rename_produces_a_new_identity();
	std::cout << "DIFF_MODEL_TESTS_OK\n";
	return EXIT_SUCCESS;
}
