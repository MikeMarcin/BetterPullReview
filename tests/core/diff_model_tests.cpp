#include "bpr/core/diff_model.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using namespace bpr::core;

auto expect(const bool condition, const std::string_view message) -> void
{
    if(!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

auto expect_error(const file_alignment& alignment,
    const alignment_validation_error expected,
    const std::string_view message) -> void
{
    const auto result = validate_alignment(alignment);
    if(result.Error != expected)
    {
        std::cerr << "FAILED: " << message << " (expected "
                  << static_cast<unsigned>(expected) << ", got "
                  << static_cast<unsigned>(result.Error) << ")\n";
        std::exit(EXIT_FAILURE);
    }
}

template<typename Identity>
[[nodiscard]] constexpr auto opaque(const std::uint8_t seed) -> Identity
{
    std::array<std::uint8_t, 32> bytes{};
    bytes[0] = seed;
    bytes[31] = static_cast<std::uint8_t>(seed ^ 0xA5U);
    return Identity{bytes};
}

[[nodiscard]] constexpr auto digest_bytes(const std::string_view hex)
    -> std::array<std::uint8_t, 32>
{
    const auto nibble = [](const char value) -> std::uint8_t
    {
        return value >= '0' && value <= '9'
            ? static_cast<std::uint8_t>(value - '0')
            : static_cast<std::uint8_t>(value - 'a' + 10);
    };
    auto result = std::array<std::uint8_t, 32>{};
    for(std::size_t index = 0U; index < result.size(); ++index)
    {
        result[index] = static_cast<std::uint8_t>(
            static_cast<std::uint8_t>(nibble(hex[index * 2U]) << 4U)
            | nibble(hex[index * 2U + 1U]));
    }
    return result;
}

[[nodiscard]] constexpr auto blob(const std::uint8_t seed,
    const git_hash_algorithm algorithm = git_hash_algorithm::sha1)
    -> git_blob_oid
{
    std::array<std::uint8_t, 32> bytes{};
    const auto count = algorithm == git_hash_algorithm::sha1 ? 20U : 32U;
    for(std::size_t index = 0U; index < count; ++index)
    {
        bytes[index] =
            static_cast<std::uint8_t>(seed + static_cast<std::uint8_t>(index));
    }
    return git_blob_oid{algorithm, bytes};
}

[[nodiscard]] auto move_options() -> diff_options
{
    auto result = diff_options{};
    result.Moves = move_detection_policy::deterministic_exact;
    return result;
}

[[nodiscard]] auto comparison_key(diff_options options = {})
    -> file_comparison_key
{
    const auto digest = options_digest(options);
    expect(digest.has_value(), "default diff options have a digest");
    const auto authority = opaque<pull_request_authority_scope_id>(11U);
    const auto incarnation = opaque<epoch_catalog_incarnation_id>(12U);
    const auto owner = pull_request_authority{
        .Profile = host_profile_id{10U},
        .ProfileRevision = github_profile_revision{20U},
        .Host = git_host_id{30U},
        .HostAuthority = opaque<host_authority_fingerprint>(13U),
        .Account = github_account_id{40U},
        .Viewer = github_viewer_actor_id{50U},
        .Repository = repository_id{101U},
        .Cache = opaque<repository_cache_scope_id>(14U),
        .PullRequest = pull_request_id{202U},
        .Number = pull_request_number{17U},
        .Scope = authority,
    };
    return file_comparison_key{
        .Authority = owner,
        .Repository = owner.Repository,
        .PullRequest = owner.PullRequest,
        .LeftAnchor =
            comparison_anchor_identity{
                .Authority = authority,
                .Incarnation = incarnation,
                .Kind = comparison_anchor_kind::epoch_base,
                .Epoch = epoch_ordinal{1},
            },
        .RightAnchor =
            comparison_anchor_identity{
                .Authority = authority,
                .Incarnation = incarnation,
                .Kind = comparison_anchor_kind::epoch_head,
                .Epoch = epoch_ordinal{2},
            },
        .LeftBlob = blob(21U),
        .RightBlob = blob(51U),
        .File = opaque<file_identity>(31U),
        .Options = *digest,
        .Algorithm = diff_algorithm_revision{1U},
    };
}

[[nodiscard]] auto alignment_with_counts(
    const std::uint32_t left,
    const std::uint32_t right,
    diff_options options = {}) -> file_alignment
{
    auto result = file_alignment{
        .Key = comparison_key(options),
        .Options = options,
        .LeftLineCount = left,
        .RightLineCount = right,
    };
    result.Digest = comparison_digest(result.Key);
    return result;
}

auto append_segment(file_alignment& alignment,
    const alignment_kind kind,
    const line_span left,
    const line_span right) -> void
{
    const auto ordinal =
        static_cast<std::uint32_t>(alignment.Segments.size());
    alignment.Segments.push_back(alignment_segment{
        .Id = segment_identity(alignment.Digest, kind, left, right, ordinal),
        .Kind = kind,
        .Left = left,
        .Right = right,
    });
}

[[nodiscard]] constexpr auto group_summary(const std::uint64_t added,
    const std::uint64_t deleted,
    const std::uint64_t replaced_left,
    const std::uint64_t replaced_right) -> change_summary_kind
{
    const auto has_added = added != 0U;
    const auto has_deleted = deleted != 0U;
    const auto has_replaced =
        replaced_left != 0U || replaced_right != 0U;
    if(has_added && !has_deleted && !has_replaced)
    {
        return change_summary_kind::addition;
    }
    if(!has_added && has_deleted && !has_replaced)
    {
        return change_summary_kind::deletion;
    }
    if(!has_added && !has_deleted && has_replaced)
    {
        return change_summary_kind::replacement;
    }
    return change_summary_kind::mixed;
}

auto append_group(file_alignment& alignment,
    const std::uint32_t begin,
    const std::uint32_t end) -> void
{
    expect(begin < end, "test group has members");
    expect(
        static_cast<std::size_t>(end) <= alignment.Segments.size(),
        "test group is in bounds");
    const auto& first = alignment.Segments[static_cast<std::size_t>(begin)];
    const auto& last = alignment.Segments[static_cast<std::size_t>(end - 1U)];
    const auto left = line_span{first.Left.Begin, last.Left.End};
    const auto right = line_span{first.Right.Begin, last.Right.End};
    const auto ordinal = static_cast<std::uint32_t>(alignment.Groups.size());
    const auto id = group_identity(alignment.Digest, left, right, ordinal);
    auto added = std::uint64_t{0U};
    auto deleted = std::uint64_t{0U};
    auto replaced_left = std::uint64_t{0U};
    auto replaced_right = std::uint64_t{0U};
    for(auto member = begin; member < end; ++member)
    {
        auto& segment = alignment.Segments[static_cast<std::size_t>(member)];
        segment.Group = id;
        switch(segment.Kind)
        {
        case alignment_kind::right_only:
            added += segment.Right.extent();
            break;
        case alignment_kind::left_only:
            deleted += segment.Left.extent();
            break;
        case alignment_kind::replacement:
            replaced_left += segment.Left.extent();
            replaced_right += segment.Right.extent();
            break;
        case alignment_kind::equal:
            break;
        }
    }
    alignment.Groups.push_back(change_group{
        .Id = id,
        .Members = {.Begin = begin, .End = end},
        .Left = left,
        .Right = right,
        .First = {.Left = left.Begin, .Right = right.Begin},
        .Last = {.Left = left.End, .Right = right.End},
        .Kind = group_summary(
            added, deleted, replaced_left, replaced_right),
        .AddedLines = added,
        .DeletedLines = deleted,
        .ReplacedLeftLines = replaced_left,
        .ReplacedRightLines = replaced_right,
    });
}

auto append_pair(file_alignment& alignment,
    const std::uint32_t segment_index,
    const std::uint32_t left,
    const std::uint32_t right,
    const line_pair_kind kind) -> void
{
    const auto segment =
        alignment.Segments[static_cast<std::size_t>(segment_index)].Id;
    const auto ordinal = static_cast<std::uint32_t>(alignment.Pairs.size());
    alignment.Pairs.push_back(line_pair{
        .Id = pair_identity(
            alignment.Digest, segment, left, right, kind, ordinal),
        .Segment = segment,
        .LeftLine = left,
        .RightLine = right,
        .Kind = kind,
    });
}

auto append_move(file_alignment& alignment,
    const std::uint32_t source_index,
    const std::uint32_t destination_index) -> void
{
    const auto source =
        alignment.Segments[static_cast<std::size_t>(source_index)].Id;
    const auto destination =
        alignment.Segments[static_cast<std::size_t>(destination_index)].Id;
    const auto ordinal = static_cast<std::uint32_t>(alignment.Moves.size());
    alignment.Moves.push_back(move_relation{
        .Id = move_identity(
            alignment.Digest, source, destination, ordinal),
        .SourceDeletion = source,
        .DestinationInsertion = destination,
    });
}

auto refresh_pair_ids(file_alignment& alignment) -> void
{
    for(std::size_t index = 0U; index < alignment.Pairs.size(); ++index)
    {
        auto& pair = alignment.Pairs[index];
        pair.Id = pair_identity(alignment.Digest,
            pair.Segment,
            pair.LeftLine,
            pair.RightLine,
            pair.Kind,
            static_cast<std::uint32_t>(index));
    }
}

auto refresh_move_ids(file_alignment& alignment) -> void
{
    for(std::size_t index = 0U; index < alignment.Moves.size(); ++index)
    {
        auto& move = alignment.Moves[index];
        move.Id = move_identity(alignment.Digest,
            move.SourceDeletion,
            move.DestinationInsertion,
            static_cast<std::uint32_t>(index));
    }
}

[[nodiscard]] auto equal_alignment(const std::uint32_t count = 3U)
    -> file_alignment
{
    auto result = alignment_with_counts(count, count);
    if(count != 0U)
    {
        append_segment(result,
            alignment_kind::equal,
            {0U, count},
            {0U, count});
    }
    return result;
}

[[nodiscard]] auto insertion_alignment() -> file_alignment
{
    auto result = alignment_with_counts(2U, 4U);
    append_segment(result, alignment_kind::equal, {0U, 1U}, {0U, 1U});
    append_segment(result, alignment_kind::right_only, {1U, 1U}, {1U, 3U});
    append_segment(result, alignment_kind::equal, {1U, 2U}, {3U, 4U});
    append_group(result, 1U, 2U);
    return result;
}

[[nodiscard]] auto deletion_alignment() -> file_alignment
{
    auto result = alignment_with_counts(4U, 2U);
    append_segment(result, alignment_kind::equal, {0U, 1U}, {0U, 1U});
    append_segment(result, alignment_kind::left_only, {1U, 3U}, {1U, 1U});
    append_segment(result, alignment_kind::equal, {3U, 4U}, {1U, 2U});
    append_group(result, 1U, 2U);
    return result;
}

[[nodiscard]] auto replacement_alignment() -> file_alignment
{
    auto result = alignment_with_counts(4U, 4U);
    append_segment(result, alignment_kind::equal, {0U, 1U}, {0U, 1U});
    append_segment(result, alignment_kind::replacement, {1U, 3U}, {1U, 3U});
    append_segment(result, alignment_kind::equal, {3U, 4U}, {3U, 4U});
    append_group(result, 1U, 2U);
    append_pair(result, 1U, 1U, 1U, line_pair_kind::exact);
    append_pair(result, 1U, 2U, 2U, line_pair_kind::similar);
    return result;
}

[[nodiscard]] auto move_alignment(diff_options options = move_options())
    -> file_alignment
{
    auto result = alignment_with_counts(3U, 3U, options);
    append_segment(result, alignment_kind::left_only, {0U, 1U}, {0U, 0U});
    append_segment(result, alignment_kind::equal, {1U, 2U}, {0U, 1U});
    append_segment(result, alignment_kind::right_only, {2U, 2U}, {1U, 2U});
    append_segment(result, alignment_kind::equal, {2U, 3U}, {2U, 3U});
    append_group(result, 0U, 1U);
    append_group(result, 2U, 3U);
    append_move(result, 0U, 2U);
    return result;
}

[[nodiscard]] auto two_move_alignment(diff_options options = move_options())
    -> file_alignment
{
    auto result = alignment_with_counts(5U, 5U, options);
    append_segment(result, alignment_kind::left_only, {0U, 1U}, {0U, 0U});
    append_segment(result, alignment_kind::equal, {1U, 2U}, {0U, 1U});
    append_segment(result, alignment_kind::left_only, {2U, 3U}, {1U, 1U});
    append_segment(result, alignment_kind::equal, {3U, 4U}, {1U, 2U});
    append_segment(result, alignment_kind::right_only, {4U, 4U}, {2U, 3U});
    append_segment(result, alignment_kind::equal, {4U, 5U}, {3U, 4U});
    append_segment(result, alignment_kind::right_only, {5U, 5U}, {4U, 5U});
    append_group(result, 0U, 1U);
    append_group(result, 2U, 3U);
    append_group(result, 4U, 5U);
    append_group(result, 6U, 7U);
    append_move(result, 0U, 4U);
    append_move(result, 2U, 6U);
    return result;
}

[[nodiscard]] auto mixed_alignment(diff_options options = {}) -> file_alignment
{
    auto result = alignment_with_counts(2U, 3U, options);
    append_segment(result, alignment_kind::left_only, {0U, 1U}, {0U, 0U});
    append_segment(result, alignment_kind::right_only, {1U, 1U}, {0U, 2U});
    append_segment(result, alignment_kind::equal, {1U, 2U}, {2U, 3U});
    append_group(result, 0U, 2U);
    return result;
}

auto test_options_and_comparison_identity() -> void
{
    const auto defaults = diff_options{};
    expect(valid(defaults), "default options are normalized and valid");
    const auto first = options_digest(defaults);
    const auto replay = options_digest(defaults);
    expect(first.has_value() && first == replay,
        "option digest repeats for exact normalized options");
    expect(first->bytes()
            == digest_bytes(
                "c8330c88ed5ccaf5d992d132487d89aa"
                "e266130e3763357b6036a17e6c6eafc6"),
        "option digest matches the endian-defined known-answer vector");

    auto disabled = defaults;
    disabled.Pairing = pairing_policy::disabled;
    disabled.SimilarityThresholdPermille = 875U;
    const auto normalized = normalize_diff_options(disabled);
    expect(normalized.has_value()
            && normalized->SimilarityThresholdPermille == 0U,
        "disabled pairing has one canonical threshold");
    expect(!valid(disabled) && valid(*normalized),
        "only the canonical disabled representation is valid");

    auto malformed = defaults;
    malformed.Whitespace = static_cast<whitespace_policy>(255U);
    expect(!normalize_diff_options(malformed).has_value(),
        "unknown whitespace policy is rejected");
    malformed = defaults;
    malformed.ContextLines = maximum_diff_context_lines + 1U;
    expect(!normalize_diff_options(malformed).has_value(),
        "unbounded context is rejected");
    malformed = defaults;
    malformed.SimilarityThresholdPermille = 0U;
    expect(!normalize_diff_options(malformed).has_value(),
        "enabled pairing requires a positive threshold");
    malformed.SimilarityThresholdPermille =
        static_cast<std::uint16_t>(maximum_similarity_threshold_permille + 1U);
    expect(!normalize_diff_options(malformed).has_value(),
        "similarity threshold is bounded");

    const auto key = comparison_key();
    expect(valid(key), "exact comparison key is valid");
    const auto key_digest = comparison_digest(key);
    expect(key_digest.valid() && key_digest == comparison_digest(key),
        "comparison digest replays exactly");
    expect(key_digest.bytes()
            == digest_bytes(
                "fbdfe1e651fe7e0227040953c236b296"
                "032ddcf3f30503c7107d6d8037f588ae"),
        "comparison digest matches the exact-key known-answer vector");

    auto variants = std::vector<file_comparison_key>{};
    variants.push_back(key);
    variants.back().Authority.Repository = repository_id{102U};
    variants.back().Repository = repository_id{102U};
    variants.push_back(key);
    variants.back().Authority.PullRequest = pull_request_id{203U};
    variants.back().PullRequest = pull_request_id{203U};
    variants.push_back(key);
    variants.back().LeftAnchor.Epoch = epoch_ordinal{3};
    variants.push_back(key);
    variants.back().RightBlob = blob(52U);
    variants.push_back(key);
    variants.back().File = opaque<file_identity>(32U);
    variants.push_back(key);
    variants.back().Options = opaque<diff_options_digest>(33U);
    variants.push_back(key);
    variants.back().Algorithm = diff_algorithm_revision{2U};
    variants.push_back(key);
    variants.back().Authority.Account = github_account_id{41U};
    for(const auto& variant : variants)
    {
        expect(valid(variant) && comparison_digest(variant) != key_digest,
            "every exact cache-key dimension changes comparison identity");
    }

    auto contradictory = key;
    contradictory.Repository = repository_id{102U};
    expect(!valid(contradictory),
        "repository field must agree with full pull-request authority");
    contradictory = key;
    contradictory.LeftAnchor.Authority =
        opaque<pull_request_authority_scope_id>(90U);
    expect(!valid(contradictory),
        "both anchors must belong to the exact pull-request authority");

    auto added_file = key;
    added_file.LeftBlob.reset();
    expect(valid(added_file) && comparison_digest(added_file).valid(),
        "one absent blob has exact added-file identity");
    auto no_blobs = added_file;
    no_blobs.RightBlob.reset();
    expect(!valid(no_blobs) && !comparison_digest(no_blobs).valid(),
        "comparison without either blob is rejected");
    auto mixed_hashes = key;
    mixed_hashes.RightBlob = blob(52U, git_hash_algorithm::sha256);
    expect(!valid(mixed_hashes),
        "two present blobs must use the same object algorithm");
}

auto test_canonical_record_tables() -> void
{
    const auto empty = equal_alignment(0U);
    const auto equal = equal_alignment();
    const auto insertion = insertion_alignment();
    const auto deletion = deletion_alignment();
    const auto replacement = replacement_alignment();
    const auto moved = move_alignment();
    const auto two_moves = two_move_alignment();
    const auto mixed = mixed_alignment();
    for(const auto* alignment :
        {&empty,
            &equal,
            &insertion,
            &deletion,
            &replacement,
            &moved,
            &two_moves,
            &mixed})
    {
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

    auto added_file = alignment_with_counts(0U, 2U);
    added_file.Key.LeftBlob.reset();
    added_file.Digest = comparison_digest(added_file.Key);
    append_segment(
        added_file, alignment_kind::right_only, {0U, 0U}, {0U, 2U});
    append_group(added_file, 0U, 1U);
    expect(validate_alignment(added_file).valid(),
        "an entire added file uses a boundary, not fake left lines");

    auto binary = alignment_with_counts(0U, 0U);
    binary.Content = diff_content_state::binary;
    binary.Diagnostic.Code = diff_reason_code::binary_file;
    expect(validate_alignment(binary).valid(),
        "binary diagnostic publishes no fake semantic records");
    auto unavailable = alignment_with_counts(0U, 0U);
    unavailable.Content = diff_content_state::unavailable;
    unavailable.Diagnostic.Code = diff_reason_code::decode_failure;
    expect(validate_alignment(unavailable).valid(),
        "unavailable diagnostic publishes no fake semantic records");

    const auto maximum = std::numeric_limits<std::uint32_t>::max();
    auto boundary = alignment_with_counts(maximum, maximum);
    append_segment(
        boundary, alignment_kind::equal, {0U, maximum}, {0U, maximum});
    expect(boundary.Segments[0].Left.extent()
            == static_cast<std::uint64_t>(maximum)
            && validate_alignment(boundary).valid(),
        "maximum uint32 line boundary validates without count overflow");
}

auto test_source_references() -> void
{
    const auto digest = comparison_digest(comparison_key());
    expect(valid(source_line_ref{digest, diff_side::left, 2U}, 3U, 5U),
        "left source line is bounded by left document");
    expect(!valid(source_line_ref{digest, diff_side::left, 3U}, 3U, 5U),
        "line coordinate is exclusive at line count");
    expect(valid(source_boundary_ref{digest, diff_side::right, 5U}, 3U, 5U),
        "boundary coordinate includes document end");
    expect(!valid(source_boundary_ref{digest, diff_side::right, 6U}, 3U, 5U),
        "boundary beyond document end is rejected");
    expect(!valid(source_line_ref{{}, diff_side::right, 0U}, 1U, 1U),
        "source reference requires exact comparison identity");
    expect(!valid(source_boundary_ref{
                      digest, static_cast<diff_side>(255U), 0U},
               1U,
               1U),
        "unknown side is rejected");
}

auto test_stable_record_identities() -> void
{
    const auto digest = comparison_digest(comparison_key());
    const auto segment = segment_identity(digest,
        alignment_kind::replacement,
        {2U, 5U},
        {2U, 7U},
        4U);
    expect(segment.valid()
            && segment
                == segment_identity(digest,
                    alignment_kind::replacement,
                    {2U, 5U},
                    {2U, 7U},
                    4U),
        "segment identity replays from canonical comparison and structure");
    expect(segment.bytes()
            == digest_bytes(
                "612fe655afb24746886fcb5c4b8f13db"
                "b91f2fd6eec227a00a547c11f278ff7c"),
        "segment identity matches its known-answer vector");
    expect(segment
                != segment_identity(digest,
                    alignment_kind::replacement,
                    {2U, 5U},
                    {2U, 7U},
                    5U)
            && segment
                != segment_identity(digest,
                    alignment_kind::replacement,
                    {2U, 5U},
                    {2U, 8U},
                    4U),
        "segment ordinal and spans are identity dimensions");
    expect(!segment_identity(digest,
                alignment_kind::right_only,
                {0U, 0U},
                {0U, 0U},
                0U)
                .valid(),
        "identity factory rejects a both-empty segment");

    const auto pair =
        pair_identity(digest, segment, 2U, 3U, line_pair_kind::similar, 0U);
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

    const auto group = group_identity(digest, {2U, 5U}, {2U, 7U}, 0U);
    expect(group.valid()
            && group == group_identity(digest, {2U, 5U}, {2U, 7U}, 0U)
            && group != group_identity(digest, {2U, 5U}, {2U, 8U}, 0U),
        "group identity replays from canonical covering spans");
    expect(group.bytes()
            == digest_bytes(
                "5f0c01c652486a98000ad5c03c9e9976"
                "b6729cfd200609a5928484ca233af438"),
        "group identity matches its known-answer vector");

    const auto destination = segment_identity(digest,
        alignment_kind::right_only,
        {7U, 7U},
        {9U, 11U},
        7U);
    const auto move = move_identity(digest, segment, destination, 0U);
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
        auto broken = equal_alignment();
        broken.Key.Repository = {};
        expect_error(broken,
            alignment_validation_error::invalid_comparison_key,
            "invalid key is rejected");
    }
    {
        auto broken = equal_alignment();
        broken.Digest = opaque<file_comparison_digest>(91U);
        expect_error(broken,
            alignment_validation_error::comparison_digest_mismatch,
            "comparison digest mismatch is rejected");
    }
    {
        auto broken = equal_alignment();
        broken.Options.ContextLines = 4U;
        expect_error(broken,
            alignment_validation_error::options_digest_mismatch,
            "published normalized options must match the cache-key digest");
    }
    {
        auto broken = equal_alignment();
        broken.Diagnostic.Code = diff_reason_code::binary_file;
        expect_error(broken,
            alignment_validation_error::invalid_content_diagnostic,
            "text result cannot carry binary diagnostic");
    }
    {
        auto broken = equal_alignment();
        broken.Content = diff_content_state::binary;
        broken.Diagnostic.Code = diff_reason_code::binary_file;
        expect_error(broken,
            alignment_validation_error::records_for_non_text,
            "binary result cannot retain text records");
    }
    {
        auto broken = alignment_with_counts(1U, 0U);
        broken.Content = diff_content_state::unavailable;
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
        broken.Groups.push_back(change_group{});
        expect_error(broken,
            alignment_validation_error::records_without_segments,
            "empty text cannot bypass orphan semantic-record rejection");
    }
    {
        auto disabled = diff_options{};
        disabled.Pairing = pairing_policy::disabled;
        disabled.SimilarityThresholdPermille = 0U;
        auto broken = alignment_with_counts(2U, 2U, disabled);
        append_segment(
            broken, alignment_kind::replacement, {0U, 2U}, {0U, 2U});
        append_group(broken, 0U, 1U);
        append_pair(broken, 0U, 0U, 0U, line_pair_kind::exact);
        expect_error(broken,
            alignment_validation_error::records_disabled_by_options,
            "disabled pairing policy rejects pair records");
    }
    {
        auto broken = move_alignment(diff_options{});
        expect_error(broken,
            alignment_validation_error::records_disabled_by_options,
            "disabled move policy rejects move records");
    }
    {
        auto broken = alignment_with_counts(0U, 0U);
        broken.Key.LeftBlob.reset();
        broken.Digest = comparison_digest(broken.Key);
        broken.Content = diff_content_state::unavailable;
        broken.Diagnostic.Code = diff_reason_code::missing_left_blob;
        expect_error(broken,
            alignment_validation_error::invalid_content_diagnostic,
            "absent-file identity is not an acquisition-missing diagnostic");
    }
    {
        auto broken = equal_alignment();
        broken.Segments[0].Id = opaque<alignment_segment_id>(92U);
        expect_error(broken,
            alignment_validation_error::invalid_segment_id,
            "segment identity mismatch is rejected");
    }
    {
        auto broken = equal_alignment();
        broken.Segments[0].Left = {2U, 1U};
        expect_error(broken,
            alignment_validation_error::invalid_segment_span,
            "reversed segment span is rejected");
    }
    {
        auto broken = equal_alignment();
        broken.Segments[0].Left = {1U, 3U};
        broken.Segments[0].Right = {1U, 3U};
        broken.Segments[0].Id = segment_identity(broken.Digest,
            broken.Segments[0].Kind,
            broken.Segments[0].Left,
            broken.Segments[0].Right,
            0U);
        expect_error(broken,
            alignment_validation_error::left_coverage_discontinuity,
            "left coverage gap is rejected");
    }
    {
        auto broken = equal_alignment();
        broken.Segments[0].Right.Begin = 1U;
        expect_error(broken,
            alignment_validation_error::right_coverage_discontinuity,
            "right coverage gap is rejected");
    }
    {
        auto broken = equal_alignment();
        broken.Segments[0].Left = {0U, 0U};
        broken.Segments[0].Right = {0U, 0U};
        expect_error(broken,
            alignment_validation_error::empty_segment,
            "both-empty segment is rejected");
    }
    {
        auto broken = equal_alignment();
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
        auto broken = equal_alignment();
        broken.Segments[0].Group = opaque<change_group_id>(93U);
        expect_error(broken,
            alignment_validation_error::grouped_equal_segment,
            "equal segment cannot belong to change group");
    }
    {
        auto broken = alignment_with_counts(4U, 4U);
        append_segment(
            broken, alignment_kind::equal, {0U, 2U}, {0U, 2U});
        append_segment(
            broken, alignment_kind::equal, {2U, 4U}, {2U, 4U});
        expect_error(broken,
            alignment_validation_error::noncanonical_adjacent_segment,
            "adjacent equal operations must be coalesced");
    }
    {
        auto broken = alignment_with_counts(4U, 4U);
        append_segment(
            broken, alignment_kind::replacement, {0U, 2U}, {0U, 2U});
        append_segment(
            broken, alignment_kind::replacement, {2U, 4U}, {2U, 4U});
        append_group(broken, 0U, 2U);
        expect_error(broken,
            alignment_validation_error::noncanonical_adjacent_segment,
            "unrefined adjacent replacements must be coalesced");
    }
    {
        auto refined = alignment_with_counts(4U, 4U);
        append_segment(
            refined, alignment_kind::replacement, {0U, 2U}, {0U, 2U});
        append_segment(
            refined, alignment_kind::replacement, {2U, 4U}, {2U, 4U});
        append_group(refined, 0U, 2U);
        append_pair(refined, 1U, 2U, 2U, line_pair_kind::exact);
        expect(validate_alignment(refined).valid(),
            "accepted pair metadata retains a replacement refinement");
    }
    {
        auto broken = alignment_with_counts(2U, 0U);
        append_segment(
            broken, alignment_kind::left_only, {0U, 1U}, {0U, 0U});
        append_segment(
            broken, alignment_kind::left_only, {1U, 2U}, {0U, 0U});
        append_group(broken, 0U, 2U);
        expect_error(broken,
            alignment_validation_error::noncanonical_adjacent_segment,
            "mergeable adjacent deletions must be coalesced");
    }
    {
        auto broken = alignment_with_counts(0U, 2U);
        append_segment(
            broken, alignment_kind::right_only, {0U, 0U}, {0U, 1U});
        append_segment(
            broken, alignment_kind::right_only, {0U, 0U}, {1U, 2U});
        append_group(broken, 0U, 2U);
        expect_error(broken,
            alignment_validation_error::noncanonical_adjacent_segment,
            "mergeable adjacent insertions must be coalesced");
    }
    {
        auto refined = alignment_with_counts(5U, 4U, move_options());
        append_segment(
            refined, alignment_kind::left_only, {0U, 1U}, {0U, 0U});
        append_segment(
            refined, alignment_kind::left_only, {1U, 2U}, {0U, 0U});
        append_segment(
            refined, alignment_kind::equal, {2U, 3U}, {0U, 1U});
        append_segment(
            refined, alignment_kind::right_only, {3U, 3U}, {1U, 2U});
        append_segment(
            refined, alignment_kind::equal, {3U, 5U}, {2U, 4U});
        append_group(refined, 0U, 2U);
        append_group(refined, 3U, 4U);
        append_move(refined, 0U, 3U);
        expect(validate_alignment(refined).valid(),
            "move metadata retains a one-sided refinement boundary");
    }
    {
        auto broken = alignment_with_counts(3U, 3U);
        append_segment(
            broken, alignment_kind::equal, {0U, 2U}, {0U, 2U});
        expect_error(broken,
            alignment_validation_error::incomplete_left_coverage,
            "incomplete final coverage is rejected");
    }
    {
        auto broken = insertion_alignment();
        broken.Groups[0].Members.End = 4U;
        expect_error(broken,
            alignment_validation_error::invalid_group_span,
            "group member interval is bounded");
    }
    {
        auto broken = insertion_alignment();
        broken.Groups[0].Members.End = broken.Groups[0].Members.Begin;
        expect_error(broken,
            alignment_validation_error::empty_group,
            "empty group is rejected");
    }
    {
        auto broken = move_alignment();
        broken.Groups[1].Members = broken.Groups[0].Members;
        expect_error(broken,
            alignment_validation_error::unordered_group,
            "group intervals must be ordered");
    }
    {
        auto broken = alignment_with_counts(1U, 2U);
        append_segment(
            broken, alignment_kind::left_only, {0U, 1U}, {0U, 0U});
        append_segment(
            broken, alignment_kind::right_only, {1U, 1U}, {0U, 2U});
        append_group(broken, 0U, 1U);
        append_group(broken, 1U, 2U);
        expect_error(broken,
            alignment_validation_error::noncanonical_adjacent_group,
            "one touching changed run cannot publish competing groups");
    }
    {
        auto broken = insertion_alignment();
        broken.Groups[0].Left.Begin = 0U;
        expect_error(broken,
            alignment_validation_error::group_span_mismatch,
            "group covering span is derived from its members");
    }
    {
        auto broken = insertion_alignment();
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
        auto broken = insertion_alignment();
        broken.Groups[0].Id = opaque<change_group_id>(99U);
        broken.Segments[1].Group = broken.Groups[0].Id;
        expect_error(broken,
            alignment_validation_error::invalid_group_id,
            "group identity mismatch is rejected");
    }
    {
        auto broken = mixed_alignment();
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
        auto broken = replacement_alignment();
        broken.Pairs[0].Kind = static_cast<line_pair_kind>(255U);
        expect_error(broken,
            alignment_validation_error::invalid_pair_kind,
            "unknown pair kind is rejected");
    }
    {
        auto broken = replacement_alignment();
        broken.Pairs[0].Segment = opaque<alignment_segment_id>(95U);
        refresh_pair_ids(broken);
        expect_error(broken,
            alignment_validation_error::unknown_pair_segment,
            "pair must reference a published segment");
    }
    {
        auto broken = replacement_alignment();
        broken.Pairs[0].LeftLine = 3U;
        refresh_pair_ids(broken);
        expect_error(broken,
            alignment_validation_error::pair_out_of_bounds,
            "pair coordinates remain inside replacement segment");
    }
    {
        auto broken = replacement_alignment();
        broken.Pairs[0].Segment = broken.Segments[0].Id;
        broken.Pairs[0].LeftLine = 0U;
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
        auto broken = replacement_alignment();
        broken.Pairs[0].Id = opaque<line_pair_id>(96U);
        expect_error(broken,
            alignment_validation_error::invalid_pair_id,
            "pair identity mismatch is rejected");
    }
    {
        auto broken = move_alignment();
        broken.Moves[0].SourceDeletion = opaque<alignment_segment_id>(97U);
        refresh_move_ids(broken);
        expect_error(broken,
            alignment_validation_error::unknown_move_segment,
            "move endpoint must reference a published segment");
    }
    {
        auto broken = move_alignment();
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
        auto broken = two_move_alignment();
        broken.Moves[1].SourceDeletion = broken.Moves[0].SourceDeletion;
        refresh_move_ids(broken);
        expect_error(broken,
            alignment_validation_error::invalid_move_topology,
            "exact move endpoints participate in at most one relation");
    }
    {
        auto broken = move_alignment();
        broken.Moves[0].Id = opaque<move_relation_id>(98U);
        expect_error(broken,
            alignment_validation_error::invalid_move_id,
            "move identity mismatch is rejected");
    }
}

class deterministic_sequence final
{
public:
    explicit deterministic_sequence(const std::uint32_t seed) : State{seed}
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

[[nodiscard]] auto generated_alignment(const std::uint32_t seed)
    -> file_alignment
{
    auto sequence = deterministic_sequence{seed};
    auto result = alignment_with_counts(0U, 0U);
    auto left = std::uint32_t{0U};
    auto right = std::uint32_t{0U};
    auto prior_kind = std::optional<alignment_kind>{};
    const auto count = 1U + sequence.next() % 32U;
    for(auto index = std::uint32_t{0U}; index < count; ++index)
    {
        auto kind_value = sequence.next() % 4U;
        if(prior_kind.has_value()
           && kind_value == static_cast<std::uint32_t>(*prior_kind))
        {
            kind_value = (kind_value + 1U) % 4U;
        }
        const auto kind = static_cast<alignment_kind>(kind_value);
        const auto first_extent = 1U + sequence.next() % 4U;
        const auto second_extent = 1U + sequence.next() % 4U;
        auto left_extent = std::uint32_t{0U};
        auto right_extent = std::uint32_t{0U};
        switch(kind)
        {
        case alignment_kind::equal:
            left_extent = first_extent;
            right_extent = first_extent;
            break;
        case alignment_kind::replacement:
            left_extent = first_extent;
            right_extent = second_extent;
            break;
        case alignment_kind::left_only:
            left_extent = first_extent;
            break;
        case alignment_kind::right_only:
            right_extent = second_extent;
            break;
        }
        append_segment(result,
            kind,
            {left, static_cast<std::uint32_t>(left + left_extent)},
            {right, static_cast<std::uint32_t>(right + right_extent)});
        left = static_cast<std::uint32_t>(left + left_extent);
        right = static_cast<std::uint32_t>(right + right_extent);
        prior_kind = kind;
    }
    result.LeftLineCount = left;
    result.RightLineCount = right;

    auto group_begin = std::optional<std::uint32_t>{};
    for(auto index = std::uint32_t{0U}; index < count; ++index)
    {
        const auto changed =
            result.Segments[static_cast<std::size_t>(index)].Kind
            != alignment_kind::equal;
        if(changed && !group_begin.has_value())
        {
            group_begin = index;
        }
        if(!changed && group_begin.has_value())
        {
            append_group(result, *group_begin, index);
            group_begin.reset();
        }
    }
    if(group_begin.has_value())
    {
        append_group(result, *group_begin, count);
    }
    for(auto index = std::uint32_t{0U}; index < count; ++index)
    {
        const auto& segment =
            result.Segments[static_cast<std::size_t>(index)];
        if(segment.Kind == alignment_kind::replacement)
        {
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
    for(auto seed = std::uint32_t{1U}; seed <= 512U; ++seed)
    {
        const auto first = generated_alignment(seed);
        const auto replay = generated_alignment(seed);
        expect(first == replay,
            "fixed-seed generated alignment replays byte-for-byte");
        expect(validate_alignment(first).valid(),
            "fixed-seed generated canonical alignment validates");

        auto malformed = first;
        if(!malformed.Segments[0].Left.empty())
        {
            ++malformed.Segments[0].Left.Begin;
        }
        else
        {
            ++malformed.Segments[0].Right.Begin;
        }
        expect(!validate_alignment(malformed).valid(),
            "fixed-seed malformed alignment is rejected");
    }
}
} // namespace

int main()
{
    test_options_and_comparison_identity();
    test_canonical_record_tables();
    test_source_references();
    test_stable_record_identities();
    test_validator_failures();
    test_generated_replay_and_rejection();
    std::cout << "DIFF_MODEL_TESTS_OK\n";
    return EXIT_SUCCESS;
}
