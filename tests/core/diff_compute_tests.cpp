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

template <typename Identity>
[[nodiscard]] constexpr auto opaque(const std::uint8_t seed) -> Identity
{
    std::array<std::uint8_t, 32> bytes{};
    bytes[0] = seed;
    bytes[31] = static_cast<std::uint8_t>(seed ^ 0xA5U);
    return Identity{bytes};
}

[[nodiscard]] constexpr auto blob(const std::uint8_t seed) -> git_blob_oid
{
    std::array<std::uint8_t, 32> bytes{};
    for(std::size_t index = 0U; index < 20U; ++index)
    {
        bytes[index] =
            static_cast<std::uint8_t>(seed + static_cast<std::uint8_t>(index));
    }
    return git_blob_oid{git_hash_algorithm::sha1, bytes};
}

[[nodiscard]] auto comparison_key(const diff_options& options,
                                  const bool has_left = true,
                                  const bool has_right = true,
                                  const std::uint8_t file_seed = 31U)
    -> file_comparison_key
{
    const auto digest = options_digest(options);
    expect(digest.has_value(), "test options are normalized");
    const auto scope = opaque<pull_request_authority_scope_id>(11U);
    const auto incarnation = opaque<epoch_catalog_incarnation_id>(12U);
    const auto authority = pull_request_authority{
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
        .Scope = scope,
    };
    return file_comparison_key{
        .Authority = authority,
        .Repository = authority.Repository,
        .PullRequest = authority.PullRequest,
        .LeftAnchor =
            comparison_anchor_identity{
                .Authority = scope,
                .Incarnation = incarnation,
                .Kind = comparison_anchor_kind::epoch_base,
                .Epoch = epoch_ordinal{1},
            },
        .RightAnchor =
            comparison_anchor_identity{
                .Authority = scope,
                .Incarnation = incarnation,
                .Kind = comparison_anchor_kind::epoch_head,
                .Epoch = epoch_ordinal{2},
            },
        .LeftBlob = has_left ? std::optional{blob(21U)} : std::nullopt,
        .RightBlob = has_right ? std::optional{blob(51U)} : std::nullopt,
        .File = opaque<file_identity>(file_seed),
        .Options = *digest,
        .Algorithm = diff_algorithm_revision{1U},
    };
}

[[nodiscard]] auto copy_bytes(const std::string_view text)
    -> std::vector<std::uint8_t>
{
    auto result = std::vector<std::uint8_t>{};
    result.reserve(text.size());
    for(const auto character : text)
    {
        result.push_back(static_cast<std::uint8_t>(character));
    }
    return result;
}

[[nodiscard]] auto is_utf8_boundary(const decoded_text_document& document,
                                    const decoded_text_line& line,
                                    const std::uint64_t offset) -> bool
{
    const auto extent = line.Content.End - line.Content.Begin;
    if(offset > extent)
    {
        return false;
    }
    if(offset == 0U || offset == extent)
    {
        return true;
    }
    const auto byte =
        document.Bytes[static_cast<std::size_t>(line.Content.Begin + offset)];
    return (byte & 0xC0U) != 0x80U;
}

[[nodiscard]] auto run(const std::optional<std::string_view> left,
                       const std::optional<std::string_view> right,
                       const diff_options& options = {},
                       const bool has_left_blob = true,
                       const bool has_right_blob = true,
                       const diff_compute_limits& limits = {},
                       const std::uint8_t file_seed = 31U)
    -> diff_compute_outcome
{
    auto left_storage =
        left.has_value() ? std::optional{copy_bytes(*left)} : std::nullopt;
    auto right_storage =
        right.has_value() ? std::optional{copy_bytes(*right)} : std::nullopt;
    auto input = diff_compute_input{
        .Key =
            comparison_key(options, has_left_blob, has_right_blob, file_seed),
        .Options = options,
        .LeftBytes =
            left_storage.has_value()
                ? std::optional{std::span<const std::uint8_t>{*left_storage}}
                : std::nullopt,
        .RightBytes =
            right_storage.has_value()
                ? std::optional{std::span<const std::uint8_t>{*right_storage}}
                : std::nullopt,
        .Limits = limits,
    };
    return compute_file_diff(input);
}

[[nodiscard]] auto text_result(const diff_compute_outcome& outcome,
                               const std::string_view message)
    -> computed_file_diff
{
    expect(outcome.Reason == diff_reason_code::none, message);
    expect(outcome.Result.has_value(), message);
    expect(outcome.Result->Alignment.Content == diff_content_state::text,
           message);
    expect(validate_computed_file_diff(*outcome.Result).valid(), message);
    return *outcome.Result;
}

auto expect_compute_error(const computed_file_diff& diff,
                          const diff_compute_validation_error expected,
                          const std::string_view message) -> void
{
    const auto result = validate_computed_file_diff(diff);
    expect(result.Error == expected, message);
}

auto test_identical_and_empty() -> void
{
    const auto& empty = text_result(run("", ""), "empty text computes");
    expect(empty.Alignment.Segments.empty(), "empty text has no fake segment");
    expect(empty.Alignment.LeftLineCount == 0U &&
               empty.Alignment.RightLineCount == 0U,
           "empty text has zero logical lines");

    const auto first = run("alpha\nbeta\n", "alpha\nbeta\n");
    const auto replay = run("alpha\nbeta\n", "alpha\nbeta\n");
    const auto& identical = text_result(first, "identical text computes");
    expect(identical.Alignment.Segments.size() == 1U &&
               identical.Alignment.Segments.front().Kind ==
                   alignment_kind::equal,
           "identical text is one equal segment");
    expect(first.Result == replay.Result, "identical replay is byte stable");
}

auto test_insertions_and_deletions() -> void
{
    struct insertion_case
    {
        std::string_view Left;
        std::string_view Right;
        std::uint32_t Boundary = 0U;
    };
    const auto insertion_cases = std::array{
        insertion_case{"b\nc\n", "a\nb\nc\n", 0U},
        insertion_case{"a\nc\n", "a\nb\nc\n", 1U},
        insertion_case{"a\nb\n", "a\nb\nc\n", 2U},
    };
    for(const auto& test_case : insertion_cases)
    {
        const auto& result = text_result(run(test_case.Left, test_case.Right),
                                         "insertion computes");
        expect(result.Alignment.Groups.size() == 1U,
               "insertion has one canonical group");
        expect(result.Alignment.Groups.front().AddedLines == 1U,
               "insertion group counts one added line");
        const auto segment = std::ranges::find_if(
            result.Alignment.Segments,
            [](const alignment_segment& candidate)
            { return candidate.Kind == alignment_kind::right_only; });
        expect(segment != result.Alignment.Segments.end() &&
                   segment->Left ==
                       line_span{test_case.Boundary, test_case.Boundary} &&
                   segment->Right ==
                       line_span{test_case.Boundary, test_case.Boundary + 1U},
               "insertion preserves its empty boundary and source span");
    }
    for(const auto& test_case : insertion_cases)
    {
        const auto& result = text_result(run(test_case.Right, test_case.Left),
                                         "deletion computes");
        expect(result.Alignment.Groups.size() == 1U,
               "deletion has one canonical group");
        expect(result.Alignment.Groups.front().DeletedLines == 1U,
               "deletion group counts one deleted line");
        const auto segment = std::ranges::find_if(
            result.Alignment.Segments,
            [](const alignment_segment& candidate)
            { return candidate.Kind == alignment_kind::left_only; });
        expect(segment != result.Alignment.Segments.end() &&
                   segment->Left ==
                       line_span{test_case.Boundary, test_case.Boundary + 1U} &&
                   segment->Right ==
                       line_span{test_case.Boundary, test_case.Boundary},
               "deletion preserves its source span and empty boundary");
    }
}

auto test_replacements_pairing_and_replay() -> void
{
    const auto four =
        std::string_view{"item one\nitem two\nitem three\nitem four\n"};
    const auto seven = std::string_view{
        "item 1\nitem 2\nitem 3\nitem 4\nitem 5\nitem 6\nitem 7\n"};
    const auto& forward = text_result(run(four, seven), "4:7 computes");
    const auto& reverse = text_result(run(seven, four), "7:4 computes");
    expect(forward.Alignment.Pairs.size() == 4U &&
               reverse.Alignment.Pairs.size() == 4U &&
               forward.Alignment.Segments.size() == 5U &&
               reverse.Alignment.Segments.size() == 5U,
           "unequal replacements retain the complete refined path");
    for(std::uint32_t index = 0U; index < 4U; ++index)
    {
        const auto& forward_pair = forward.Alignment.Pairs[index];
        const auto& reverse_pair = reverse.Alignment.Pairs[index];
        expect(forward_pair.LeftLine == index &&
                   forward_pair.RightLine == index &&
                   forward_pair.Kind == line_pair_kind::similar &&
                   reverse_pair.LeftLine == index &&
                   reverse_pair.RightLine == index &&
                   reverse_pair.Kind == line_pair_kind::similar,
               "unequal replacement pair coordinates and kinds are exact");
        expect(forward.Alignment.Segments[index].Left ==
                       line_span{index, index + 1U} &&
                   forward.Alignment.Segments[index].Right ==
                       line_span{index, index + 1U} &&
                   reverse.Alignment.Segments[index].Left ==
                       line_span{index, index + 1U} &&
                   reverse.Alignment.Segments[index].Right ==
                       line_span{index, index + 1U},
               "unequal replacement anchors retain exact refined spans");
    }
    expect(forward.Alignment.Segments.back().Kind ==
                   alignment_kind::right_only &&
               forward.Alignment.Segments.back().Left == line_span{4U, 4U} &&
               forward.Alignment.Segments.back().Right == line_span{4U, 7U} &&
               reverse.Alignment.Segments.back().Kind ==
                   alignment_kind::left_only &&
               reverse.Alignment.Segments.back().Left == line_span{4U, 7U} &&
               reverse.Alignment.Segments.back().Right == line_span{4U, 4U},
           "unequal replacement tails remain explicit one-sided spans");

    const auto anchored =
        text_result(run("old alpha\nfixed anchor\nold omega\n",
                        "new alpha\nfixed anchor\nnew omega\n"),
                    "internal exact anchor computes");
    expect(anchored.Alignment.Segments.size() >= 3U,
           "primary exact internal anchor refines the path");
    expect(anchored.Alignment.Segments[1].Kind == alignment_kind::equal,
           "internal exact anchor is represented exactly");

    const auto& similar_anchors =
        text_result(run("alpha old\nleft gap one\nleft gap two\nomega old\n",
                        "alpha new\nright gap\nomega new\n"),
                    "internal similar anchors compute");
    expect(similar_anchors.Alignment.Pairs.size() == 2U &&
               similar_anchors.Alignment.Pairs[0].LeftLine == 0U &&
               similar_anchors.Alignment.Pairs[0].RightLine == 0U &&
               similar_anchors.Alignment.Pairs[1].LeftLine == 3U &&
               similar_anchors.Alignment.Pairs[1].RightLine == 2U,
           "internal similar anchors refine a replacement monotonically");

    const auto similar = run("prefix alpha suffix\n", "prefix beta suffix\n");
    const auto similar_replay =
        run("prefix alpha suffix\n", "prefix beta suffix\n");
    const auto& similar_result = text_result(similar, "similar pair computes");
    expect(similar_result.Alignment.Pairs.size() == 1U &&
               similar_result.Alignment.Pairs.front().Kind ==
                   line_pair_kind::similar,
           "strong similar lines pair");
    expect(!similar_result.Intraline.empty(),
           "similar pair carries original-text intraline ranges");
    expect(similar.Result == similar_replay.Result,
           "similar pair ties and IDs replay deterministically");

    auto strict = diff_options{};
    strict.SimilarityThresholdPermille = 900U;
    const auto& weak = text_result(run("aaaaaaaa\n", "zzzzzzzz\n", strict),
                                   "weak replacement computes");
    expect(weak.Alignment.Pairs.empty() && weak.Intraline.empty(),
           "weak pairing remains explicitly unpaired");

    const auto repeated =
        run("same prefix A\nsame prefix B\n", "same prefix B\nsame prefix A\n");
    const auto repeated_replay =
        run("same prefix A\nsame prefix B\n", "same prefix B\nsame prefix A\n");
    expect(repeated.Result == repeated_replay.Result,
           "repeated-line tie resolution replays exactly");
    expect(
        repeated.Result->Alignment.Segments.size() == 3U &&
            repeated.Result->Alignment.Segments[0].Kind ==
                alignment_kind::left_only &&
            repeated.Result->Alignment.Segments[1].Kind ==
                alignment_kind::equal &&
            repeated.Result->Alignment.Segments[1].Left == line_span{1U, 2U} &&
            repeated.Result->Alignment.Segments[1].Right == line_span{0U, 1U} &&
            repeated.Result->Alignment.Segments[2].Kind ==
                alignment_kind::right_only,
        "repeated-line tie selects the documented earliest right anchor");
}

auto test_whitespace_and_endings() -> void
{
    const auto& exact =
        text_result(run("value \t\n", "value\n"), "exact whitespace computes");
    expect(!exact.Alignment.Groups.empty(), "exact whitespace detects change");

    auto trailing = diff_options{};
    trailing.Whitespace = whitespace_policy::ignore_trailing;
    const auto& ignored_trailing =
        text_result(run("value \t\n", "value\n", trailing),
                    "trailing whitespace mode computes");
    expect(ignored_trailing.Alignment.Groups.empty(),
           "trailing whitespace mode matches without changing display bytes");
    expect(ignored_trailing.LeftDocument->Bytes == copy_bytes("value \t\n"),
           "ignored trailing whitespace remains byte-faithful for display");

    auto all = diff_options{};
    all.Whitespace = whitespace_policy::ignore_all;
    const auto& ignored_all = text_result(run("a \tb\n", "ab\n", all),
                                          "all-whitespace mode computes");
    expect(ignored_all.Alignment.Groups.empty(),
           "all-whitespace mode removes spaces and tabs for matching only");

    const auto& exact_endings =
        text_result(run("a\r\nb\rc\n", "a\nb\rc\n"), "exact endings compute");
    expect(!exact_endings.Alignment.Groups.empty(),
           "exact line-ending policy distinguishes LF and CRLF");
    expect(
        exact_endings.LeftDocument->Lines[0].Ending == text_line_ending::crlf &&
            exact_endings.LeftDocument->Lines[1].Ending == text_line_ending::cr,
        "splitter distinguishes CRLF and lone CR endings");
    expect(
        exact_endings.LeftDocument->Lines[1].Content.End -
                exact_endings.LeftDocument->Lines[1].Content.Begin ==
            1U,
        "lone CR terminates a logical line without entering display content");

    auto normalized = diff_options{};
    normalized.LineEndings = line_ending_policy::normalize;
    const auto& normalized_endings = text_result(
        run("a\r\n", "a\n", normalized), "normalized endings compute");
    expect(normalized_endings.Alignment.Groups.empty(),
           "normalized line-ending policy matches LF and CRLF");
    expect(normalized_endings.LeftDocument->Bytes == copy_bytes("a\r\n"),
           "line-ending normalization does not alter retained bytes");

    const auto& final_newline =
        text_result(run("last\n", "last"), "missing final newline computes");
    expect(final_newline.LeftDocument->HasFinalNewline &&
               !final_newline.RightDocument->HasFinalNewline,
           "final-newline fact is preserved");
    expect(final_newline.Alignment.LeftLineCount == 1U &&
               final_newline.Alignment.RightLineCount == 1U,
           "final newline creates no phantom line");
}

auto test_unicode_binary_and_unavailable() -> void
{
    const auto left =
        std::string_view{"caf\xC3\xA9 e\xCC\x81 \xF0\x9F\x98\x80\n"};
    const auto right =
        std::string_view{"caf\xC3\xA8 e\xCC\x81 \xF0\x9F\x98\x83\n"};
    const auto& unicode = text_result(run(left, right), "Unicode computes");
    expect(!unicode.Intraline.empty(), "Unicode similar lines have ranges");
    for(const auto& range : unicode.Intraline)
    {
        const auto& pair = unicode.Alignment.Pairs.front();
        const auto left_size =
            unicode.LeftDocument->Lines[pair.LeftLine].Content.End -
            unicode.LeftDocument->Lines[pair.LeftLine].Content.Begin;
        const auto right_size =
            unicode.RightDocument->Lines[pair.RightLine].Content.End -
            unicode.RightDocument->Lines[pair.RightLine].Content.Begin;
        expect(range.Left.Begin <= range.Left.End &&
                   range.Left.End <= left_size &&
                   range.Right.Begin <= range.Right.End &&
                   range.Right.End <= right_size,
               "Unicode intraline ranges stay within original text");
        expect(
            is_utf8_boundary(*unicode.LeftDocument,
                             unicode.LeftDocument->Lines[pair.LeftLine],
                             range.Left.Begin) &&
                is_utf8_boundary(*unicode.LeftDocument,
                                 unicode.LeftDocument->Lines[pair.LeftLine],
                                 range.Left.End) &&
                is_utf8_boundary(*unicode.RightDocument,
                                 unicode.RightDocument->Lines[pair.RightLine],
                                 range.Right.Begin) &&
                is_utf8_boundary(*unicode.RightDocument,
                                 unicode.RightDocument->Lines[pair.RightLine],
                                 range.Right.End),
            "Unicode intraline endpoints are UTF-8 boundaries");
    }

    const auto malformed_cases = std::array{
        std::string_view{"\x80", 1U},
        std::string_view{"\xE2\x82", 2U},
        std::string_view{"\xC0\xAF", 2U},
        std::string_view{"\xED\xA0\x80", 3U},
        std::string_view{"\xF4\x90\x80\x80", 4U},
    };
    for(const auto malformed_text : malformed_cases)
    {
        const auto malformed = run(malformed_text, "ok");
        expect(malformed.Reason == diff_reason_code::decode_failure &&
                   malformed.Result.has_value() &&
                   malformed.Result->Alignment.Segments.empty(),
               "malformed UTF-8 publishes only decode status");
    }

    const auto binary = run(std::string_view{"a\0b", 3U}, "text");
    expect(binary.Reason == diff_reason_code::binary_file &&
               binary.Result.has_value() &&
               binary.Result->Alignment.Content == diff_content_state::binary &&
               binary.Result->Alignment.Segments.empty(),
           "NUL classifies binary without semantic records");

    const auto missing = run(std::nullopt, "text");
    expect(missing.Reason == diff_reason_code::missing_left_blob &&
               missing.Result.has_value() &&
               missing.Result->Alignment.Segments.empty(),
           "expected but unavailable material is explicit");

    const auto added = text_result(run(std::nullopt, "new\n", {}, false, true),
                                   "added file computes");
    expect(!added.LeftDocument.has_value() &&
               added.Alignment.LeftLineCount == 0U &&
               added.Alignment.Groups.front().AddedLines == 1U,
           "absent added-file side differs from an empty materialized blob");
    const auto deleted = text_result(
        run("old\n", std::nullopt, {}, true, false), "deleted file computes");
    expect(!deleted.RightDocument.has_value() &&
               deleted.Alignment.RightLineCount == 0U &&
               deleted.Alignment.Groups.front().DeletedLines == 1U,
           "absent deleted-file side is retained exactly");

    const auto contradiction = run("invented", "real", {}, false, true);
    expect(contradiction.Reason == diff_reason_code::invalid_alignment &&
               !contradiction.Result.has_value(),
           "material for an absent key side is rejected without a result");
}

auto test_moves_identity_and_limits() -> void
{
    auto moves = diff_options{};
    moves.Moves = move_detection_policy::deterministic_exact;
    const auto& moved =
        text_result(run("move me\nstay\n", "stay\nmove me\n", moves),
                    "move detection computes");
    expect(moved.Alignment.Moves.size() == 1U,
           "unambiguous exact whole segment has separate move metadata");
    expect(moved.Alignment.Segments.front().Kind == alignment_kind::left_only &&
               moved.Alignment.Segments.back().Kind ==
                   alignment_kind::right_only,
           "move remains deletion plus insertion in monotonic primary path");

    auto disabled = moves;
    disabled.Moves = move_detection_policy::disabled;
    const auto& no_move =
        text_result(run("move me\nstay\n", "stay\nmove me\n", disabled),
                    "disabled moves compute");
    expect(no_move.Alignment.Moves.empty(), "disabled move detection is empty");

    const auto& ambiguous = text_result(run("x\na\nb\n", "a\nx\nb\nx\n", moves),
                                        "ambiguous move computes");
    expect(ambiguous.Alignment.Moves.empty(),
           "ambiguous exact moved text creates no relation");

    const auto stable_file = run("a\n", "b\n", {}, true, true, {}, 77U);
    const auto stable_replay = run("a\n", "b\n", {}, true, true, {}, 77U);
    const auto other_file = run("a\n", "b\n", {}, true, true, {}, 78U);
    expect(stable_file.Result == stable_replay.Result,
           "rename-stable file identity produces stable IDs under replay");
    expect(stable_file.Result->Alignment.Digest !=
               other_file.Result->Alignment.Digest,
           "stable file identity is consumed by comparison identity");

    auto no_pairing = diff_options{};
    no_pairing.Pairing = pairing_policy::disabled;
    no_pairing.SimilarityThresholdPermille = 0U;
    const auto baseline =
        text_result(run("a\n", "b\n", no_pairing), "limit baseline computes");
    auto exact_limits = diff_compute_limits{};
    exact_limits.MaximumBytesPerSide = 2U;
    exact_limits.MaximumLinesPerSide = 1U;
    exact_limits.MaximumMatrixCells = baseline.MatrixCellsConsumed;
    exact_limits.MaximumSegments = 1U;
    const auto exact =
        text_result(run("a\n", "b\n", no_pairing, true, true, exact_limits),
                    "exact limits compute");
    expect(exact.MatrixCellsConsumed == baseline.MatrixCellsConsumed,
           "exact matrix limit is accepted without overflow");

    auto over_cells = exact_limits;
    --over_cells.MaximumMatrixCells;
    const auto cell_limited =
        run("a\n", "b\n", no_pairing, true, true, over_cells);
    expect(cell_limited.Reason == diff_reason_code::compute_limit &&
               cell_limited.Result.has_value() &&
               cell_limited.Result->Alignment.Segments.empty(),
           "over-limit computation publishes no partial records");

    auto over_bytes = exact_limits;
    over_bytes.MaximumBytesPerSide = 1U;
    const auto byte_limited =
        run("a\n", "b\n", no_pairing, true, true, over_bytes);
    expect(byte_limited.Reason == diff_reason_code::materialization_limit &&
               byte_limited.Result.has_value() &&
               byte_limited.Result->Alignment.Segments.empty(),
           "over-limit materialization publishes no partial records");

    const auto paired_baseline =
        text_result(run("prefix alpha\n", "prefix beta\n"),
                    "secondary budget baseline computes");
    auto paired_limits = diff_compute_limits{};
    paired_limits.MaximumMatrixCells = paired_baseline.MatrixCellsConsumed;
    const auto paired_exact = text_result(
        run("prefix alpha\n", "prefix beta\n", {}, true, true, paired_limits),
        "secondary exact limit computes");
    expect(paired_exact.MatrixCellsConsumed ==
               paired_baseline.MatrixCellsConsumed,
           "secondary score, decision, similarity, and intraline grids are "
           "charged");
    --paired_limits.MaximumMatrixCells;
    const auto paired_limited =
        run("prefix alpha\n", "prefix beta\n", {}, true, true, paired_limits);
    expect(paired_limited.Reason == diff_reason_code::compute_limit &&
               paired_limited.Result.has_value() &&
               paired_limited.Result->Alignment.Segments.empty(),
           "one cell below secondary work returns no partial records");
}

auto test_computed_result_validator() -> void
{
    const auto valid = text_result(
        run(std::string_view{"caf\xC3\xA9 \xF0\x9F\x98\x80 tail\n"},
            std::string_view{"caf\xC3\xA8 \xF0\x9F\x98\x83 tail\n"}),
        "validator fixture computes");
    expect(valid.Intraline.size() >= 2U,
           "validator fixture contains separate multibyte edits");

    auto missing_document = valid;
    missing_document.LeftDocument.reset();
    expect_compute_error(
        missing_document,
        diff_compute_validation_error::document_presence_mismatch,
        "validator rejects document/key presence disagreement");

    auto wrong_count = valid;
    wrong_count.LeftDocument->Lines.clear();
    expect_compute_error(
        wrong_count,
        diff_compute_validation_error::document_line_count_mismatch,
        "validator rejects retained document line-count disagreement");

    auto malformed_document = valid;
    ++malformed_document.LeftDocument->Lines.front().Content.End;
    expect_compute_error(malformed_document,
                         diff_compute_validation_error::invalid_document,
                         "validator rejects malformed line topology");

    auto orphan = valid;
    orphan.Intraline.front().Pair = opaque<line_pair_id>(99U);
    expect_compute_error(orphan,
                         diff_compute_validation_error::unknown_intraline_pair,
                         "validator rejects orphan intraline identity");

    auto mid_scalar = valid;
    ++mid_scalar.Intraline.front().Left.Begin;
    expect_compute_error(mid_scalar,
                         diff_compute_validation_error::invalid_intraline_span,
                         "validator rejects a mid-scalar intraline endpoint");

    auto overlap = valid;
    overlap.Intraline[1].Left.Begin = overlap.Intraline[0].Left.Begin;
    overlap.Intraline[1].Right.Begin = overlap.Intraline[0].Right.Begin;
    expect_compute_error(overlap,
                         diff_compute_validation_error::nonmonotonic_intraline,
                         "validator rejects overlapping intraline edits");

    auto exact_pair = valid;
    auto& pair = exact_pair.Alignment.Pairs.front();
    pair.Kind = line_pair_kind::exact;
    pair.Id = pair_identity(exact_pair.Alignment.Digest,
                            pair.Segment,
                            pair.LeftLine,
                            pair.RightLine,
                            pair.Kind,
                            0U);
    for(auto& edit : exact_pair.Intraline)
    {
        edit.Pair = pair.Id;
    }
    expect_compute_error(
        exact_pair,
        diff_compute_validation_error::intraline_for_exact_pair,
        "validator rejects intraline edits on an exact pair");
}

auto test_generated_replay_validation() -> void
{
    const auto atoms =
        std::array<std::string_view, 4U>{"a\n", "b\n", "c\n", ""};
    for(const auto first : atoms)
    {
        for(const auto second : atoms)
        {
            const auto left = std::string{first} + std::string{second};
            for(const auto third : atoms)
            {
                const auto right = std::string{second} + std::string{third};
                const auto result = run(left, right);
                const auto replay = run(left, right);
                expect(result.Result.has_value() &&
                           validate_alignment(result.Result->Alignment).valid(),
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
    test_computed_result_validator();
    test_generated_replay_validation();
    std::cout << "BPR-0073 deterministic diff computation tests passed\n";
    return EXIT_SUCCESS;
}
