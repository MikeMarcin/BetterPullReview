#include "bpr/core/diff_semantics.hpp"

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
                                  const bool has_left,
                                  const bool has_right,
                                  const std::uint8_t file_seed)
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
    return {
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

[[nodiscard]] auto bytes(const std::string_view text)
    -> std::vector<std::uint8_t>
{
    return {text.begin(), text.end()};
}

[[nodiscard]] auto run(const std::optional<std::string_view> left,
                       const std::optional<std::string_view> right,
                       const diff_options& options = {},
                       const bool has_left = true,
                       const bool has_right = true,
                       const std::uint8_t file_seed = 31U) -> computed_file_diff
{
    auto left_bytes =
        left.has_value() ? std::optional{bytes(*left)} : std::nullopt;
    auto right_bytes =
        right.has_value() ? std::optional{bytes(*right)} : std::nullopt;
    const auto outcome = compute_file_diff({
        .Key = comparison_key(options, has_left, has_right, file_seed),
        .Options = options,
        .LeftBytes =
            left_bytes.has_value()
                ? std::optional{std::span<const std::uint8_t>{*left_bytes}}
                : std::nullopt,
        .RightBytes =
            right_bytes.has_value()
                ? std::optional{std::span<const std::uint8_t>{*right_bytes}}
                : std::nullopt,
    });
    expect(outcome.Result.has_value(), "fixture publishes a result");
    return *outcome.Result;
}

[[nodiscard]] constexpr auto line(const computed_file_diff& diff,
                                  const diff_side side,
                                  const std::uint32_t number) -> source_line_ref
{
    return {.Comparison = diff.Alignment.Digest, .Side = side, .Line = number};
}

[[nodiscard]] auto segment(const computed_file_diff& diff,
                           const alignment_segment_id id)
    -> const alignment_segment&
{
    const auto found =
        std::ranges::find(diff.Alignment.Segments, id, &alignment_segment::Id);
    expect(found != diff.Alignment.Segments.end(), "segment identity exists");
    return *found;
}

auto test_exact_and_one_sided_mapping() -> void
{
    const auto exact = run("alpha\nbeta\n", "alpha\nbeta\n");
    const auto left = map_line(exact, line(exact, diff_side::left, 1U));
    expect(left.Kind == line_mapping_kind::exact_line &&
               left.Counterpart == line(exact, diff_side::right, 1U) &&
               !left.Group.has_value() && !left.Pair.has_value(),
           "equal line maps to its exact counterpart");
    const auto reverse = map_line(exact, *left.Counterpart);
    expect(reverse.Kind == line_mapping_kind::exact_line &&
               reverse.Counterpart == left.Source &&
               reverse.Segment == left.Segment,
           "equal mapping is exactly reversible");

    const auto insertion = run("a\nc\n", "a\nb\nc\n");
    const auto inserted =
        map_line(insertion, line(insertion, diff_side::right, 1U));
    expect(inserted.Kind == line_mapping_kind::opposing_boundary &&
               inserted.OpposingBoundary ==
                   source_boundary_ref{
                       insertion.Alignment.Digest, diff_side::left, 1U},
           "middle insertion maps to the exact empty left boundary");
    const auto insertion_detail = detail_for_mapping(insertion, inserted);
    expect(insertion_detail.Kind == line_detail_kind::opposing_boundary &&
               insertion_detail.Selected == inserted.Source &&
               !insertion_detail.LeftLine.has_value() &&
               insertion_detail.RightLine == inserted.Source &&
               insertion_detail.OpposingBoundary == inserted.OpposingBoundary &&
               insertion_detail.Segment == inserted.Segment &&
               insertion_detail.Group == inserted.Group,
           "one-sided detail preserves selected line boundary and identities");

    const auto deletion = run("a\nb\nc\n", "a\nc\n");
    const auto deleted =
        map_line(deletion, line(deletion, diff_side::left, 1U));
    expect(deleted.Kind == line_mapping_kind::opposing_boundary &&
               deleted.OpposingBoundary ==
                   source_boundary_ref{
                       deletion.Alignment.Digest, diff_side::right, 1U},
           "middle deletion maps to the exact empty right boundary");
    const auto deletion_detail = detail_for_mapping(deletion, deleted);
    expect(deletion_detail.LeftLine == deleted.Source &&
               !deletion_detail.RightLine.has_value(),
           "deletion detail populates only its real left source line");

    const auto added = run(std::nullopt, "new\n", {}, false, true);
    const auto added_line = map_line(added, line(added, diff_side::right, 0U));
    expect(added_line.Kind == line_mapping_kind::opposing_boundary &&
               added_line.OpposingBoundary->Boundary == 0U &&
               added_line.OpposingBoundary->Side == diff_side::left,
           "added-file content maps to boundary zero on the absent side");
}

auto test_pair_span_and_unequal_tail_mapping() -> void
{
    const auto paired = run("prefix alpha suffix\n", "prefix beta suffix\n");
    const auto left = map_line(paired, line(paired, diff_side::left, 0U));
    const auto right = map_line(paired, line(paired, diff_side::right, 0U));
    expect(left.Kind == line_mapping_kind::similar_line &&
               right.Kind == line_mapping_kind::similar_line &&
               left.Counterpart == right.Source &&
               right.Counterpart == left.Source && left.Pair == right.Pair &&
               left.Segment == right.Segment && left.Group == right.Group,
           "accepted similar pair is reversible with one stable identity set");
    const auto detail = detail_for_line(paired, left.Source);
    expect(detail.Kind == line_detail_kind::paired_lines &&
               detail.LeftLine == left.Source &&
               detail.RightLine == left.Counterpart &&
               detail.Pair == left.Pair && detail.Segment == left.Segment &&
               detail.Group == left.Group && !detail.Intraline.empty() &&
               std::ranges::all_of(detail.Intraline,
                                   [&detail](const intraline_edit& edit)
                                   { return edit.Pair == *detail.Pair; }),
           "paired detail contains only that pair's intraline edits");

    auto unpaired_options = diff_options{};
    unpaired_options.Pairing = pairing_policy::disabled;
    unpaired_options.SimilarityThresholdPermille = 0U;
    const auto unpaired = run("left one\nleft two\n",
                              "right one\nright two\nright three\n",
                              unpaired_options);
    const auto from_left =
        map_line(unpaired, line(unpaired, diff_side::left, 1U));
    const auto from_right =
        map_line(unpaired, line(unpaired, diff_side::right, 2U));
    expect(from_left.Kind == line_mapping_kind::opposing_span &&
               from_left.OpposingSpan->Lines == line_span{0U, 3U} &&
               from_right.Kind == line_mapping_kind::opposing_span &&
               from_right.OpposingSpan->Lines == line_span{0U, 2U},
           "weak unpaired lines expose the complete opposing replacement span");
    const auto unpaired_detail = detail_for_mapping(unpaired, from_right);
    expect(unpaired_detail.Kind == line_detail_kind::opposing_span &&
               unpaired_detail.Selected == from_right.Source &&
               !unpaired_detail.LeftLine.has_value() &&
               unpaired_detail.RightLine == from_right.Source &&
               unpaired_detail.OpposingSpan == from_right.OpposingSpan &&
               !unpaired_detail.Pair.has_value() &&
               unpaired_detail.Intraline.empty(),
           "unpaired detail does not invent a line or intraline edits");

    const auto four =
        std::string_view{"item one\nitem two\nitem three\nitem four\n"};
    const auto seven = std::string_view{
        "item 1\nitem 2\nitem 3\nitem 4\nitem 5\nitem 6\nitem 7\n"};
    const auto unequal = run(four, seven);
    for(auto index = std::uint32_t{0U}; index < 4U; ++index)
    {
        expect(map_line(unequal, line(unequal, diff_side::left, index))
                       .Counterpart == line(unequal, diff_side::right, index),
               "4:7 accepted anchors map exactly by their pair");
    }
    for(auto index = std::uint32_t{4U}; index < 7U; ++index)
    {
        const auto tail =
            map_line(unequal, line(unequal, diff_side::right, index));
        expect(tail.Kind == line_mapping_kind::opposing_boundary &&
                   tail.OpposingBoundary->Boundary == 4U &&
                   tail.OpposingBoundary->Side == diff_side::left,
               "4:7 unpaired tail maps to the exact left endpoint boundary");
    }
}

auto test_projection_navigation_and_identity() -> void
{
    expect(navigate_change(computed_file_diff{},
                           std::nullopt,
                           change_navigation_direction::next,
                           change_navigation_policy::stop)
                   .Outcome == change_navigation_outcome::invalid_selection,
           "default projection fails navigation closed");
    const auto diff = run("same\nold one\nmiddle\nold two\nend\n",
                          "same\nnew one\nmiddle\nnew two\nend\n");
    const auto projection = project_changes(diff);
    expect(projection.available() && projection.Changes.size() == 2U &&
               projection.Changes[0].Group == diff.Alignment.Groups[0] &&
               projection.Changes[1].Group == diff.Alignment.Groups[1],
           "change projection retains canonical group vector order exactly");
    for(std::size_t group_index = 0U; group_index < projection.Changes.size();
        ++group_index)
    {
        const auto& change = projection.Changes[group_index];
        const auto& group = diff.Alignment.Groups[group_index];
        expect(change.Segments.size() ==
                   static_cast<std::size_t>(group.Members.End -
                                            group.Members.Begin),
               "change projection uses the canonical member index span");
        for(std::size_t member = 0U; member < change.Segments.size(); ++member)
        {
            expect(change.Segments[member].Id ==
                       diff.Alignment.Segments[group.Members.Begin + member].Id,
                   "projected segment identity comes from the alignment");
        }
    }

    const auto first = projection.Changes.front().Group.Id;
    const auto last = projection.Changes.back().Group.Id;
    const auto none_next = navigate_change(diff,
                                           std::nullopt,
                                           change_navigation_direction::next,
                                           change_navigation_policy::stop);
    const auto none_previous =
        navigate_change(diff,
                        std::nullopt,
                        change_navigation_direction::previous,
                        change_navigation_policy::wrap);
    expect(none_next.Outcome == change_navigation_outcome::selected &&
               none_next.Group == first && none_next.Index == 0U &&
               none_previous.Outcome == change_navigation_outcome::selected &&
               none_previous.Group == last && none_previous.Index == 1U,
           "no-selection navigation chooses first next and last previous");
    expect(navigate_change(diff,
                           first,
                           change_navigation_direction::previous,
                           change_navigation_policy::stop)
                       .Outcome == change_navigation_outcome::at_start &&
               navigate_change(diff,
                               last,
                               change_navigation_direction::next,
                               change_navigation_policy::stop)
                       .Outcome == change_navigation_outcome::at_end,
           "stop navigation reports both endpoints without changing identity");
    const auto previous_wrap =
        navigate_change(diff,
                        first,
                        change_navigation_direction::previous,
                        change_navigation_policy::wrap);
    const auto next_wrap = navigate_change(diff,
                                           last,
                                           change_navigation_direction::next,
                                           change_navigation_policy::wrap);
    expect(previous_wrap.Outcome == change_navigation_outcome::wrapped &&
               previous_wrap.Group == last &&
               next_wrap.Outcome == change_navigation_outcome::wrapped &&
               next_wrap.Group == first,
           "wrap navigation reports wrapping and the canonical target");
    expect(navigate_change(diff,
                           first,
                           change_navigation_direction::next,
                           change_navigation_policy::stop)
                       .Group == last &&
               navigate_change(diff,
                               last,
                               change_navigation_direction::previous,
                               change_navigation_policy::stop)
                       .Group == first,
           "interior previous and next follow vector order");
    expect(navigate_change(diff,
                           opaque<change_group_id>(99U),
                           change_navigation_direction::next,
                           change_navigation_policy::stop)
                   .Outcome == change_navigation_outcome::invalid_selection,
           "unknown current group is invalid rather than ordinally guessed");
    expect(navigate_change(diff,
                           first,
                           static_cast<change_navigation_direction>(99U),
                           change_navigation_policy::stop)
                       .Outcome ==
                   change_navigation_outcome::invalid_selection &&
               navigate_change(diff,
                               first,
                               change_navigation_direction::next,
                               static_cast<change_navigation_policy>(99U))
                       .Outcome == change_navigation_outcome::invalid_selection,
           "invalid navigation policy values fail closed");

    const auto unchanged = run("same\n", "same\n");
    const auto no_changes = project_changes(unchanged);
    expect(navigate_change(unchanged,
                           std::nullopt,
                           change_navigation_direction::next,
                           change_navigation_policy::wrap)
                   .Outcome == change_navigation_outcome::no_changes,
           "valid empty group vector has an explicit no-changes outcome");
    expect(navigate_change(unchanged,
                           opaque<change_group_id>(90U),
                           change_navigation_direction::next,
                           change_navigation_policy::stop)
                   .Outcome == change_navigation_outcome::invalid_selection,
           "current identity in an empty group vector is invalid selection");

    const auto selected_line = line(diff, diff_side::left, 1U);
    const auto mapping = map_line(diff, selected_line);
    const auto detail = detail_for_mapping(diff, mapping);
    const auto line_selection = select_line(diff, selected_line);
    const auto selection = select_group(diff, *mapping.Group);
    expect(mapping.Segment.has_value() && mapping.Group == first &&
               detail.Segment == mapping.Segment &&
               detail.Group == mapping.Group && detail.Pair == mapping.Pair &&
               projection.Changes.front().Group.Id == *mapping.Group &&
               std::ranges::find(projection.Changes.front().Segments,
                                 *mapping.Segment,
                                 &change_segment_projection::Id) !=
                   projection.Changes.front().Segments.end() &&
               none_next.Group == mapping.Group && line_selection.has_value() &&
               line_selection->Segment == mapping.Segment &&
               line_selection->Group == mapping.Group &&
               detail_for_selection(diff, *line_selection) == detail &&
               selection.has_value() && selection->Segment == mapping.Segment &&
               selection->Group == mapping.Group &&
               detail_for_selection(diff, *selection).Group == mapping.Group,
           "alignment mapping detail navigation selection and change "
           "projection agree");
}

auto test_selection_reconciliation() -> void
{
    const auto diff = run("old\nkeep\n", "new\nkeep\n");
    const auto replay = run("old\nkeep\n", "new\nkeep\n");
    const auto line_selection =
        select_line(diff, line(diff, diff_side::right, 0U));
    const auto group_selection =
        select_group(diff, diff.Alignment.Groups.front().Id);
    expect(
        line_selection.has_value() && group_selection.has_value() &&
            reconcile_selection(replay, line_selection).Outcome ==
                selection_reconciliation_outcome::preserved &&
            reconcile_selection(replay, group_selection).Outcome ==
                selection_reconciliation_outcome::preserved,
        "same-digest replay preserves valid stable line and group selection");

    const auto insertion = run("same\n", "added\nsame\n");
    const auto inserted_group = insertion.Alignment.Groups.front().Id;
    const auto preferred = preferred_group_line(insertion, inserted_group);
    const auto selected_group = select_group(insertion, inserted_group);
    expect(preferred == line(insertion, diff_side::right, 0U) &&
               selected_group->PreferredLine == *preferred,
           "group preference uses first member right begin when left is empty");
    expect(preferred_group_line(diff, diff.Alignment.Groups.front().Id) ==
               line(diff, diff_side::left, 0U),
           "group preference uses first member left begin when populated");

    const auto changed_digest =
        run("old\nkeep\n", "new\nkeep\n", {}, true, true, 32U);
    expect(reconcile_selection(changed_digest, line_selection).Outcome ==
                   selection_reconciliation_outcome::cleared &&
               reconcile_selection(changed_digest, group_selection).Outcome ==
                   selection_reconciliation_outcome::cleared,
           "changed comparison digest clears without path or ordinal fallback");

    auto bad_line = *line_selection;
    bad_line.PreferredLine.Line = 99U;
    auto bad_group = *group_selection;
    bad_group.Group = opaque<change_group_id>(88U);
    expect(reconcile_selection(replay, bad_line).Outcome ==
                   selection_reconciliation_outcome::cleared &&
               reconcile_selection(replay, bad_group).Outcome ==
                   selection_reconciliation_outcome::cleared,
           "missing line or stable group identity clears selection");
    expect(detail_for_selection(replay, bad_group).Reason ==
               semantic_lookup_reason::invalid_result,
           "detail rejects a fabricated semantic selection");
}

auto test_unavailable_and_fail_closed() -> void
{
    const auto text = run("a\n", "b\n");
    auto stale = line(text, diff_side::left, 0U);
    stale.Comparison = opaque<file_comparison_digest>(77U);
    expect(map_line(text, stale).Reason ==
               semantic_lookup_reason::comparison_mismatch,
           "mismatched comparison digest is typed as stale identity");
    expect(detail_for_line(text, stale).Reason ==
               semantic_lookup_reason::comparison_mismatch,
           "stale detail preserves the primary mapping reason");
    expect(map_line(text, line(text, diff_side::left, 1U)).Reason ==
               semantic_lookup_reason::line_out_of_range,
           "out-of-range line fails explicitly");
    auto invalid_side = line(text, diff_side::left, 0U);
    invalid_side.Side = static_cast<diff_side>(99U);
    expect(map_line(text, invalid_side).Reason ==
               semantic_lookup_reason::invalid_side,
           "invalid side fails explicitly");

    const auto binary = run(std::string_view{"a\0b", 3U}, "text");
    const auto binary_mapping =
        map_line(binary, line(binary, diff_side::left, 0U));
    expect(binary_mapping.Reason == semantic_lookup_reason::non_text &&
               binary_mapping.Diagnostic == diff_reason_code::binary_file &&
               project_changes(binary).Reason ==
                   semantic_lookup_reason::non_text,
           "binary result exposes its typed nontext diagnostic");
    const auto missing = run(std::nullopt, "text");
    const auto missing_mapping =
        map_line(missing, line(missing, diff_side::right, 0U));
    expect(missing_mapping.Reason == semantic_lookup_reason::non_text &&
               missing_mapping.Diagnostic ==
                   diff_reason_code::missing_left_blob,
           "unavailable result exposes its typed diagnostic");
    expect(!select_line(binary, line(binary, diff_side::left, 0U)).has_value(),
           "nontext content cannot create semantic selection");

    auto mutated = text;
    ++mutated.Alignment.Segments.front().Left.End;
    const auto invalid_line = line(mutated, diff_side::left, 0U);
    const auto invalid_mapping = map_line(mutated, invalid_line);
    const auto invalid_projection = project_changes(mutated);
    expect(invalid_mapping.Reason == semantic_lookup_reason::invalid_result &&
               invalid_projection.Reason ==
                   semantic_lookup_reason::invalid_result &&
               detail_for_mapping(mutated, invalid_mapping).Kind ==
                   line_detail_kind::unavailable &&
               !move_for_line(mutated, invalid_line).has_value() &&
               !select_line(mutated, invalid_line).has_value() &&
               navigate_change(mutated,
                               std::nullopt,
                               change_navigation_direction::next,
                               change_navigation_policy::stop)
                       .Outcome == change_navigation_outcome::invalid_selection,
           "mutated invalid result fails every semantic projection closed");
    auto fabricated = map_line(text, line(text, diff_side::left, 0U));
    fabricated.Segment = opaque<alignment_segment_id>(98U);
    fabricated.Diagnostic = diff_reason_code::binary_file;
    const auto fabricated_detail = detail_for_mapping(text, fabricated);
    expect(fabricated_detail.Reason == semantic_lookup_reason::invalid_result &&
               !fabricated_detail.Segment.has_value() &&
               !fabricated_detail.Group.has_value() &&
               !fabricated_detail.Pair.has_value() &&
               fabricated_detail.Diagnostic == text.Alignment.Diagnostic.Code,
           "detail rejects fabricated mapping identities and diagnostics");
    const auto valid_selection =
        select_line(text, line(text, diff_side::left, 0U));
    expect(reconcile_selection(mutated, valid_selection).Outcome ==
               selection_reconciliation_outcome::cleared,
           "invalid replacement clears prior selection");
}

auto test_move_relation_stays_secondary() -> void
{
    auto options = diff_options{};
    options.Moves = move_detection_policy::deterministic_exact;
    const auto diff =
        run("move one\nmove two\nstay one\nstay two\nstay three\n",
            "stay one\nstay two\nstay three\nmove one\nmove two\n",
            options);
    expect(diff.Alignment.Moves.size() == 1U, "move fixture has one relation");
    for(auto offset = std::uint32_t{0U}; offset < 2U; ++offset)
    {
        const auto deletion_line = line(diff, diff_side::left, offset);
        const auto insertion_line = line(diff, diff_side::right, offset + 3U);
        const auto deletion_primary = map_line(diff, deletion_line);
        const auto insertion_primary = map_line(diff, insertion_line);
        const auto deletion_move = move_for_line(diff, deletion_line);
        const auto insertion_move = move_for_line(diff, insertion_line);
        expect(
            deletion_primary.Kind == line_mapping_kind::opposing_boundary &&
                insertion_primary.Kind ==
                    line_mapping_kind::opposing_boundary &&
                deletion_move.has_value() && insertion_move.has_value() &&
                deletion_move->Counterpart == insertion_line &&
                insertion_move->Counterpart == deletion_line &&
                deletion_move->Relation == insertion_move->Relation &&
                detail_for_line(diff, deletion_line).Move ==
                    deletion_move->Relation,
            "move is reversible metadata and never replaces primary boundary");
    }
    const auto projection = project_changes(diff);
    expect(projection.Changes.size() == 2U &&
               projection.Changes[0].Moves.size() == 1U &&
               projection.Changes[1].Moves.size() == 1U &&
               projection.Changes[0].Moves.front() ==
                   projection.Changes[1].Moves.front(),
           "both canonical change projections retain the same move marker");
}

auto verify_generated_mapping(const computed_file_diff& diff) -> void
{
    const auto projection = project_changes(diff);
    expect(projection.available(), "generated result projects");
    const auto check_side =
        [&diff](const diff_side side, const std::uint32_t count)
    {
        for(auto number = std::uint32_t{0U}; number < count; ++number)
        {
            const auto source = line(diff, side, number);
            const auto mapping = map_line(diff, source);
            expect(mapping.Kind != line_mapping_kind::unavailable &&
                       mapping.Segment.has_value(),
                   "generated source line always has one primary mapping");
            const auto& owner = segment(diff, *mapping.Segment);
            if(mapping.Kind == line_mapping_kind::exact_line ||
               mapping.Kind == line_mapping_kind::similar_line)
            {
                expect(mapping.Counterpart.has_value() &&
                           valid(*mapping.Counterpart,
                                 diff.Alignment.LeftLineCount,
                                 diff.Alignment.RightLineCount),
                       "generated paired counterpart stays in bounds");
                const auto reverse = map_line(diff, *mapping.Counterpart);
                expect(reverse.Counterpart == source &&
                           reverse.Segment == mapping.Segment &&
                           reverse.Pair == mapping.Pair,
                       "generated exact or similar pair is reversible");
            }
            else if(mapping.Kind == line_mapping_kind::opposing_boundary)
            {
                expect(mapping.OpposingBoundary.has_value() &&
                           valid(*mapping.OpposingBoundary,
                                 diff.Alignment.LeftLineCount,
                                 diff.Alignment.RightLineCount) &&
                           (mapping.OpposingBoundary->Side == diff_side::left
                                ? owner.Left.empty()
                                : owner.Right.empty()),
                       "generated one-sided line maps to its exact empty span");
            }
            else
            {
                const auto expected =
                    side == diff_side::left ? owner.Right : owner.Left;
                expect(mapping.OpposingSpan.has_value() &&
                           mapping.OpposingSpan->Lines == expected &&
                           valid(expected,
                                 side == diff_side::left
                                     ? diff.Alignment.RightLineCount
                                     : diff.Alignment.LeftLineCount),
                       "generated unpaired replacement exposes full span");
            }
            if(move_for_line(diff, source).has_value())
            {
                expect(mapping.Kind == line_mapping_kind::opposing_boundary,
                       "generated move never changes the primary mapping");
            }
        }
    };
    check_side(diff_side::left, diff.Alignment.LeftLineCount);
    check_side(diff_side::right, diff.Alignment.RightLineCount);

    auto visited = std::vector<change_group_id>{};
    auto navigation = navigate_change(diff,
                                      std::nullopt,
                                      change_navigation_direction::next,
                                      change_navigation_policy::stop);
    if(projection.Changes.empty())
    {
        expect(navigation.Outcome == change_navigation_outcome::no_changes,
               "generated empty traversal reports no changes");
        return;
    }
    while(navigation.Outcome == change_navigation_outcome::selected)
    {
        visited.push_back(*navigation.Group);
        navigation = navigate_change(diff,
                                     navigation.Group,
                                     change_navigation_direction::next,
                                     change_navigation_policy::stop);
    }
    expect(navigation.Outcome == change_navigation_outcome::at_end &&
               visited.size() == projection.Changes.size(),
           "generated navigation visits every group exactly once");
    for(std::size_t index = 0U; index < visited.size(); ++index)
    {
        expect(visited[index] == projection.Changes[index].Group.Id,
               "generated traversal preserves exact group vector order");
    }
}

auto test_generated_properties() -> void
{
    const auto texts = std::array<std::string_view, 7U>{
        "", "a\n", "b\n", "a\nb\n", "b\na\n", "a one\nb\n", "a two\nc\n"};
    for(const auto left : texts)
    {
        for(const auto right : texts)
        {
            verify_generated_mapping(run(left, right));
        }
    }
}
} // namespace

int main()
{
    test_exact_and_one_sided_mapping();
    test_pair_span_and_unequal_tail_mapping();
    test_projection_navigation_and_identity();
    test_selection_reconciliation();
    test_unavailable_and_fail_closed();
    test_move_relation_stays_secondary();
    test_generated_properties();
    std::cout << "BPR-0074 diff semantic projection tests passed\n";
    return EXIT_SUCCESS;
}
