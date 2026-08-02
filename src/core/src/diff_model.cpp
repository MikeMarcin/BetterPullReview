#include "bpr/core/diff_model.hpp"

#include "bpr/core/epoch_transition.hpp"
#include "bpr/core/sha256.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string_view>
#include <utility>

namespace bpr::core
{
namespace
{
template<std::size_t Capacity>
class canonical_encoder final
{
public:
    void append_domain(const std::string_view domain) noexcept
    {
        for(const auto character : domain)
        {
            append_u8(static_cast<std::uint8_t>(character));
        }
    }

    void append(const std::span<const std::uint8_t> values) noexcept
    {
        for(const auto value : values)
        {
            append_u8(value);
        }
    }

    void append_u8(const std::uint8_t value) noexcept
    {
        if(Size == Bytes.size())
        {
            Overflow = true;
            return;
        }
        Bytes[Size++] = value;
    }

    void append_u16(const std::uint16_t value) noexcept
    {
        append_u8(static_cast<std::uint8_t>(value >> 8U));
        append_u8(static_cast<std::uint8_t>(value));
    }

    void append_u32(const std::uint32_t value) noexcept
    {
        for(std::size_t shift = 4U; shift != 0U; --shift)
        {
            append_u8(static_cast<std::uint8_t>(
                value >> ((shift - 1U) * 8U)));
        }
    }

    void append_u64(const std::uint64_t value) noexcept
    {
        for(std::size_t shift = 8U; shift != 0U; --shift)
        {
            append_u8(static_cast<std::uint8_t>(
                value >> ((shift - 1U) * 8U)));
        }
    }

    [[nodiscard]] auto bytes() const noexcept
        -> std::span<const std::uint8_t>
    {
        return std::span{Bytes}.first(Size);
    }

    [[nodiscard]] auto overflowed() const noexcept -> bool
    {
        return Overflow;
    }

private:
    std::array<std::uint8_t, Capacity> Bytes{};
    std::size_t Size = 0U;
    bool Overflow = false;
};

template<typename Identity, std::size_t Capacity>
[[nodiscard]] auto identity_from(
    const canonical_encoder<Capacity>& encoder) noexcept -> Identity
{
    if(encoder.overflowed())
    {
        return {};
    }
    return Identity{sha256(encoder.bytes())};
}

[[nodiscard]] constexpr auto valid_whitespace_policy(
    const whitespace_policy policy) noexcept -> bool
{
    switch(policy)
    {
    case whitespace_policy::exact:
    case whitespace_policy::ignore_trailing:
    case whitespace_policy::ignore_all:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr auto valid_line_ending_policy(
    const line_ending_policy policy) noexcept -> bool
{
    switch(policy)
    {
    case line_ending_policy::exact:
    case line_ending_policy::normalize:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr auto valid_pairing_policy(
    const pairing_policy policy) noexcept -> bool
{
    switch(policy)
    {
    case pairing_policy::disabled:
    case pairing_policy::deterministic_similarity:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr auto valid_move_detection_policy(
    const move_detection_policy policy) noexcept -> bool
{
    switch(policy)
    {
    case move_detection_policy::disabled:
    case move_detection_policy::deterministic_exact:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr auto valid_side(const diff_side side) noexcept -> bool
{
    switch(side)
    {
    case diff_side::left:
    case diff_side::right:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr auto valid_alignment_kind(
    const alignment_kind kind) noexcept -> bool
{
    switch(kind)
    {
    case alignment_kind::equal:
    case alignment_kind::replacement:
    case alignment_kind::left_only:
    case alignment_kind::right_only:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr auto valid_line_pair_kind(
    const line_pair_kind kind) noexcept -> bool
{
    switch(kind)
    {
    case line_pair_kind::exact:
    case line_pair_kind::similar:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr auto valid_content_state(
    const diff_content_state state) noexcept -> bool
{
    switch(state)
    {
    case diff_content_state::text:
    case diff_content_state::binary:
    case diff_content_state::unavailable:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr auto valid_reason_code(
    const diff_reason_code reason) noexcept -> bool
{
    switch(reason)
    {
    case diff_reason_code::none:
    case diff_reason_code::binary_file:
    case diff_reason_code::missing_left_blob:
    case diff_reason_code::missing_right_blob:
    case diff_reason_code::decode_failure:
    case diff_reason_code::materialization_limit:
    case diff_reason_code::compute_limit:
    case diff_reason_code::generated_file_unsupported:
    case diff_reason_code::cancelled:
    case diff_reason_code::stale:
    case diff_reason_code::invalid_alignment:
    case diff_reason_code::host_failure:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr auto valid_diagnostic(
    const diff_content_state content,
    const diff_reason_code reason) noexcept -> bool
{
    if(!valid_content_state(content) || !valid_reason_code(reason))
    {
        return false;
    }
    switch(content)
    {
    case diff_content_state::text:
        return reason == diff_reason_code::none;
    case diff_content_state::binary:
        return reason == diff_reason_code::binary_file;
    case diff_content_state::unavailable:
        return reason != diff_reason_code::none
            && reason != diff_reason_code::binary_file;
    }
    return false;
}

template<std::size_t Capacity>
void append_authority(canonical_encoder<Capacity>& encoder,
    const pull_request_authority& authority) noexcept
{
    encoder.append_u64(authority.Profile.value());
    encoder.append_u64(authority.ProfileRevision.value());
    encoder.append_u64(authority.Host.value());
    encoder.append(authority.HostAuthority.bytes());
    encoder.append_u64(authority.Account.value());
    encoder.append_u64(authority.Viewer.value());
    encoder.append_u64(authority.Repository.value());
    encoder.append(authority.Cache.bytes());
    encoder.append_u64(authority.PullRequest.value());
    encoder.append_u32(authority.Number.value());
    encoder.append(authority.Scope.bytes());
}

template<std::size_t Capacity>
void append_anchor(canonical_encoder<Capacity>& encoder,
    const comparison_anchor_identity& anchor) noexcept
{
    encoder.append(anchor.Authority.bytes());
    encoder.append(anchor.Incarnation.bytes());
    encoder.append_u8(static_cast<std::uint8_t>(anchor.Kind));
    encoder.append_u64(static_cast<std::uint64_t>(anchor.Epoch.value()));
    encoder.append(anchor.Review.bytes());
}

template<std::size_t Capacity>
void append_blob(canonical_encoder<Capacity>& encoder,
    const std::optional<git_blob_oid>& blob) noexcept
{
    encoder.append_u8(blob.has_value() ? 1U : 0U);
    if(blob.has_value())
    {
        encoder.append_u8(static_cast<std::uint8_t>(blob->algorithm()));
        encoder.append(blob->bytes());
    }
}

template<std::size_t Capacity>
void append_span(canonical_encoder<Capacity>& encoder,
    const line_span span) noexcept
{
    encoder.append_u32(span.Begin);
    encoder.append_u32(span.End);
}

[[nodiscard]] constexpr auto segment_shape_valid(
    const alignment_kind kind,
    const line_span left,
    const line_span right) noexcept -> bool
{
    if(left.Begin > left.End || right.Begin > right.End
       || (left.empty() && right.empty()))
    {
        return false;
    }
    switch(kind)
    {
    case alignment_kind::equal:
        return !left.empty() && !right.empty()
            && left.extent() == right.extent();
    case alignment_kind::replacement:
        return !left.empty() && !right.empty();
    case alignment_kind::left_only:
        return !left.empty() && right.empty();
    case alignment_kind::right_only:
        return left.empty() && !right.empty();
    }
    return false;
}

[[nodiscard]] constexpr auto summary_kind(const std::uint64_t added,
    const std::uint64_t deleted,
    const std::uint64_t replaced_left,
    const std::uint64_t replaced_right) noexcept -> change_summary_kind
{
    const auto has_addition = added != 0U;
    const auto has_deletion = deleted != 0U;
    const auto has_replacement =
        replaced_left != 0U || replaced_right != 0U;
    if(has_addition && !has_deletion && !has_replacement)
    {
        return change_summary_kind::addition;
    }
    if(!has_addition && has_deletion && !has_replacement)
    {
        return change_summary_kind::deletion;
    }
    if(!has_addition && !has_deletion && has_replacement)
    {
        return change_summary_kind::replacement;
    }
    return change_summary_kind::mixed;
}

[[nodiscard]] constexpr auto failure(const alignment_validation_error error,
    const std::size_t record = alignment_validation_result::no_record,
    const std::size_t related = alignment_validation_result::no_record)
    noexcept -> alignment_validation_result
{
    return {.Error = error, .Record = record, .Related = related};
}
} // namespace

auto normalize_diff_options(diff_options options) noexcept
    -> std::optional<diff_options>
{
    if(!valid_whitespace_policy(options.Whitespace)
       || !valid_line_ending_policy(options.LineEndings)
       || !valid_pairing_policy(options.Pairing)
       || !valid_move_detection_policy(options.Moves)
       || options.ContextLines > maximum_diff_context_lines)
    {
        return std::nullopt;
    }
    if(options.Pairing == pairing_policy::disabled)
    {
        options.SimilarityThresholdPermille = 0U;
    }
    else if(options.SimilarityThresholdPermille == 0U
            || options.SimilarityThresholdPermille
                > maximum_similarity_threshold_permille)
    {
        return std::nullopt;
    }
    return options;
}

auto valid(const diff_options& options) noexcept -> bool
{
    const auto normalized = normalize_diff_options(options);
    return normalized.has_value() && *normalized == options;
}

auto options_digest(const diff_options& options) noexcept
    -> std::optional<diff_options_digest>
{
    if(!valid(options))
    {
        return std::nullopt;
    }
    auto encoder = canonical_encoder<64U>{};
    encoder.append_domain("BPR-diff-options-v1");
    encoder.append_u8(static_cast<std::uint8_t>(options.Whitespace));
    encoder.append_u8(static_cast<std::uint8_t>(options.LineEndings));
    encoder.append_u32(options.ContextLines);
    encoder.append_u8(static_cast<std::uint8_t>(options.Pairing));
    encoder.append_u16(options.SimilarityThresholdPermille);
    encoder.append_u8(static_cast<std::uint8_t>(options.Moves));
    return identity_from<diff_options_digest>(encoder);
}

auto valid(const file_comparison_key& key) noexcept -> bool
{
    const auto blobs_valid =
        (key.LeftBlob.has_value() || key.RightBlob.has_value())
        && (!key.LeftBlob.has_value() || key.LeftBlob->valid())
        && (!key.RightBlob.has_value() || key.RightBlob->valid())
        && (!key.LeftBlob.has_value() || !key.RightBlob.has_value()
            || key.LeftBlob->algorithm() == key.RightBlob->algorithm());
    return bpr::core::valid(key.Authority)
        && key.Repository == key.Authority.Repository
        && key.PullRequest == key.Authority.PullRequest
        && bpr::core::valid(key.LeftAnchor)
        && bpr::core::valid(key.RightAnchor)
        && key.LeftAnchor.Authority == key.Authority.Scope
        && key.RightAnchor.Authority == key.Authority.Scope
        && key.LeftAnchor.Authority == key.RightAnchor.Authority
        && key.LeftAnchor.Incarnation == key.RightAnchor.Incarnation
        && blobs_valid && key.File.valid() && key.Options.valid()
        && key.Algorithm.value() != 0U;
}

auto valid(const source_line_ref& line,
    const std::uint32_t left_line_count,
    const std::uint32_t right_line_count) noexcept -> bool
{
    if(!line.Comparison.valid() || !valid_side(line.Side))
    {
        return false;
    }
    return line.Side == diff_side::left ? line.Line < left_line_count
                                        : line.Line < right_line_count;
}

auto valid(const source_boundary_ref& boundary,
    const std::uint32_t left_line_count,
    const std::uint32_t right_line_count) noexcept -> bool
{
    if(!boundary.Comparison.valid() || !valid_side(boundary.Side))
    {
        return false;
    }
    return boundary.Side == diff_side::left
        ? boundary.Boundary <= left_line_count
        : boundary.Boundary <= right_line_count;
}

auto comparison_digest(const file_comparison_key& key) noexcept
    -> file_comparison_digest
{
    if(!valid(key))
    {
        return {};
    }
    auto encoder = canonical_encoder<640U>{};
    encoder.append_domain("BPR-file-comparison-v1");
    append_authority(encoder, key.Authority);
    encoder.append_u64(key.Repository.value());
    encoder.append_u64(key.PullRequest.value());
    append_anchor(encoder, key.LeftAnchor);
    append_anchor(encoder, key.RightAnchor);
    append_blob(encoder, key.LeftBlob);
    append_blob(encoder, key.RightBlob);
    encoder.append(key.File.bytes());
    encoder.append(key.Options.bytes());
    encoder.append_u64(key.Algorithm.value());
    return identity_from<file_comparison_digest>(encoder);
}

auto segment_identity(const file_comparison_digest comparison,
    const alignment_kind kind,
    const line_span left,
    const line_span right,
    const std::uint32_t ordinal) noexcept -> alignment_segment_id
{
    if(!comparison.valid() || !valid_alignment_kind(kind)
       || !segment_shape_valid(kind, left, right))
    {
        return {};
    }
    auto encoder = canonical_encoder<96U>{};
    encoder.append_domain("BPR-alignment-segment-v1");
    encoder.append(comparison.bytes());
    encoder.append_u8(static_cast<std::uint8_t>(kind));
    append_span(encoder, left);
    append_span(encoder, right);
    encoder.append_u32(ordinal);
    return identity_from<alignment_segment_id>(encoder);
}

auto pair_identity(const file_comparison_digest comparison,
    const alignment_segment_id segment,
    const std::uint32_t left_line,
    const std::uint32_t right_line,
    const line_pair_kind kind,
    const std::uint32_t ordinal) noexcept -> line_pair_id
{
    if(!comparison.valid() || !segment.valid() || !valid_line_pair_kind(kind))
    {
        return {};
    }
    auto encoder = canonical_encoder<112U>{};
    encoder.append_domain("BPR-line-pair-v1");
    encoder.append(comparison.bytes());
    encoder.append(segment.bytes());
    encoder.append_u32(left_line);
    encoder.append_u32(right_line);
    encoder.append_u8(static_cast<std::uint8_t>(kind));
    encoder.append_u32(ordinal);
    return identity_from<line_pair_id>(encoder);
}

auto group_identity(const file_comparison_digest comparison,
    const line_span left,
    const line_span right,
    const std::uint32_t ordinal) noexcept -> change_group_id
{
    if(!comparison.valid() || left.Begin > left.End || right.Begin > right.End
       || (left.empty() && right.empty()))
    {
        return {};
    }
    auto encoder = canonical_encoder<80U>{};
    encoder.append_domain("BPR-change-group-v1");
    encoder.append(comparison.bytes());
    append_span(encoder, left);
    append_span(encoder, right);
    encoder.append_u32(ordinal);
    return identity_from<change_group_id>(encoder);
}

auto move_identity(const file_comparison_digest comparison,
    const alignment_segment_id source_deletion,
    const alignment_segment_id destination_insertion,
    const std::uint32_t ordinal) noexcept -> move_relation_id
{
    if(!comparison.valid() || !source_deletion.valid()
       || !destination_insertion.valid()
       || source_deletion == destination_insertion)
    {
        return {};
    }
    auto encoder = canonical_encoder<128U>{};
    encoder.append_domain("BPR-move-relation-v1");
    encoder.append(comparison.bytes());
    encoder.append(source_deletion.bytes());
    encoder.append(destination_insertion.bytes());
    encoder.append_u32(ordinal);
    return identity_from<move_relation_id>(encoder);
}

auto validate_alignment(const file_alignment& alignment)
    -> alignment_validation_result
{
    constexpr auto maximum_records =
        static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max());
    if(alignment.Segments.size() > maximum_records
       || alignment.Pairs.size() > maximum_records
       || alignment.Groups.size() > maximum_records
       || alignment.Moves.size() > maximum_records)
    {
        return failure(alignment_validation_error::count_overflow);
    }
    if(!valid(alignment.Key))
    {
        return failure(alignment_validation_error::invalid_comparison_key);
    }
    const auto normalized_options_digest = options_digest(alignment.Options);
    if(!normalized_options_digest.has_value()
       || *normalized_options_digest != alignment.Key.Options)
    {
        return failure(alignment_validation_error::options_digest_mismatch);
    }
    if(alignment.Digest != comparison_digest(alignment.Key))
    {
        return failure(
            alignment_validation_error::comparison_digest_mismatch);
    }
    if(!valid_diagnostic(alignment.Content, alignment.Diagnostic.Code))
    {
        return failure(
            alignment_validation_error::invalid_content_diagnostic);
    }
    if(alignment.Content == diff_content_state::unavailable
       && ((alignment.Diagnostic.Code == diff_reason_code::missing_left_blob
               && !alignment.Key.LeftBlob.has_value())
           || (alignment.Diagnostic.Code
                   == diff_reason_code::missing_right_blob
               && !alignment.Key.RightBlob.has_value())))
    {
        return failure(
            alignment_validation_error::invalid_content_diagnostic);
    }
    if(alignment.Content != diff_content_state::text)
    {
        if(alignment.LeftLineCount != 0U || alignment.RightLineCount != 0U
           || !alignment.Segments.empty() || !alignment.Pairs.empty()
           || !alignment.Groups.empty() || !alignment.Moves.empty())
        {
            return failure(alignment_validation_error::records_for_non_text);
        }
        return {};
    }
    if((alignment.Options.Pairing == pairing_policy::disabled
           && !alignment.Pairs.empty())
       || (alignment.Options.Moves == move_detection_policy::disabled
           && !alignment.Moves.empty()))
    {
        return failure(
            alignment_validation_error::records_disabled_by_options);
    }
    if((!alignment.Key.LeftBlob.has_value()
        && alignment.LeftLineCount != 0U)
       || (!alignment.Key.RightBlob.has_value()
           && alignment.RightLineCount != 0U))
    {
        return failure(
            alignment_validation_error::invalid_content_diagnostic);
    }
    if(alignment.Segments.empty())
    {
        if(alignment.LeftLineCount != 0U || alignment.RightLineCount != 0U)
        {
            return failure(alignment_validation_error::missing_text_segments);
        }
        return alignment.Pairs.empty() && alignment.Groups.empty()
                && alignment.Moves.empty()
            ? alignment_validation_result{}
            : failure(alignment_validation_error::records_without_segments);
    }

    auto segment_indexes = std::map<alignment_segment_id, std::size_t>{};
    auto expected_left = std::uint32_t{0U};
    auto expected_right = std::uint32_t{0U};
    for(std::size_t index = 0U; index < alignment.Segments.size(); ++index)
    {
        const auto& segment = alignment.Segments[index];
        const auto ordinal = static_cast<std::uint32_t>(index);
        if(!valid(segment.Left, alignment.LeftLineCount)
           || !valid(segment.Right, alignment.RightLineCount))
        {
            return failure(
                alignment_validation_error::invalid_segment_span, index);
        }
        if(segment.Left.Begin != expected_left)
        {
            return failure(
                alignment_validation_error::left_coverage_discontinuity,
                index);
        }
        if(segment.Right.Begin != expected_right)
        {
            return failure(
                alignment_validation_error::right_coverage_discontinuity,
                index);
        }
        if(segment.Left.empty() && segment.Right.empty())
        {
            return failure(alignment_validation_error::empty_segment, index);
        }
        if(!segment_shape_valid(segment.Kind, segment.Left, segment.Right))
        {
            return failure(
                alignment_validation_error::segment_kind_mismatch, index);
        }
        if(segment.Kind == alignment_kind::equal)
        {
            if(segment.Group.has_value())
            {
                return failure(
                    alignment_validation_error::grouped_equal_segment,
                    index);
            }
        }
        else if(!segment.Group.has_value())
        {
            return failure(
                alignment_validation_error::ungrouped_change, index);
        }
        if(!segment.Id.valid()
           || segment.Id
               != segment_identity(alignment.Digest,
                   segment.Kind,
                   segment.Left,
                   segment.Right,
                   ordinal))
        {
            return failure(
                alignment_validation_error::invalid_segment_id, index);
        }
        const auto [iterator, inserted] =
            segment_indexes.emplace(segment.Id, index);
        if(!inserted)
        {
            return failure(alignment_validation_error::duplicate_segment_id,
                index,
                iterator->second);
        }
        expected_left = segment.Left.End;
        expected_right = segment.Right.End;
    }
    if(expected_left != alignment.LeftLineCount)
    {
        return failure(
            alignment_validation_error::incomplete_left_coverage);
    }
    if(expected_right != alignment.RightLineCount)
    {
        return failure(
            alignment_validation_error::incomplete_right_coverage);
    }

    auto group_indexes = std::map<change_group_id, std::size_t>{};
    auto prior_group_end = std::uint32_t{0U};
    for(std::size_t index = 0U; index < alignment.Groups.size(); ++index)
    {
        const auto& group = alignment.Groups[index];
        const auto ordinal = static_cast<std::uint32_t>(index);
        const auto segment_count =
            static_cast<std::uint32_t>(alignment.Segments.size());
        if(group.Members.Begin > group.Members.End
           || group.Members.End > segment_count)
        {
            return failure(
                alignment_validation_error::invalid_group_span, index);
        }
        if(group.Members.empty())
        {
            return failure(alignment_validation_error::empty_group, index);
        }
        if(index != 0U && group.Members.Begin < prior_group_end)
        {
            return failure(
                alignment_validation_error::unordered_group, index);
        }
        if(index != 0U && group.Members.Begin == prior_group_end)
        {
            return failure(
                alignment_validation_error::noncanonical_adjacent_group,
                index,
                index - 1U);
        }

        const auto first_index = static_cast<std::size_t>(group.Members.Begin);
        const auto last_index =
            static_cast<std::size_t>(group.Members.End - 1U);
        const auto& first = alignment.Segments[first_index];
        const auto& last = alignment.Segments[last_index];
        if(group.Left
               != line_span{.Begin = first.Left.Begin, .End = last.Left.End}
           || group.Right
               != line_span{.Begin = first.Right.Begin, .End = last.Right.End})
        {
            return failure(
                alignment_validation_error::group_span_mismatch, index);
        }
        if(group.First
               != alignment_boundary{
                   .Left = first.Left.Begin,
                   .Right = first.Right.Begin,
               }
           || group.Last
               != alignment_boundary{
                   .Left = last.Left.End,
                   .Right = last.Right.End,
               })
        {
            return failure(
                alignment_validation_error::group_boundary_mismatch, index);
        }

        auto added = std::uint64_t{0U};
        auto deleted = std::uint64_t{0U};
        auto replaced_left = std::uint64_t{0U};
        auto replaced_right = std::uint64_t{0U};
        for(auto member = group.Members.Begin; member < group.Members.End;
            ++member)
        {
            const auto& segment =
                alignment.Segments[static_cast<std::size_t>(member)];
            if(segment.Kind == alignment_kind::equal
               || !segment.Group.has_value() || *segment.Group != group.Id)
            {
                return failure(
                    alignment_validation_error::noncontiguous_group,
                    index,
                    static_cast<std::size_t>(member));
            }
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
        if(group.AddedLines != added || group.DeletedLines != deleted
           || group.ReplacedLeftLines != replaced_left
           || group.ReplacedRightLines != replaced_right
           || group.Kind
               != summary_kind(
                   added, deleted, replaced_left, replaced_right))
        {
            return failure(
                alignment_validation_error::group_summary_mismatch, index);
        }
        if(!group.Id.valid()
           || group.Id
               != group_identity(
                   alignment.Digest, group.Left, group.Right, ordinal))
        {
            return failure(
                alignment_validation_error::invalid_group_id, index);
        }
        const auto [iterator, inserted] =
            group_indexes.emplace(group.Id, index);
        if(!inserted)
        {
            return failure(alignment_validation_error::duplicate_group_id,
                index,
                iterator->second);
        }
        prior_group_end = group.Members.End;
    }

    for(std::size_t index = 0U; index < alignment.Segments.size(); ++index)
    {
        const auto& segment = alignment.Segments[index];
        if(segment.Kind == alignment_kind::equal)
        {
            continue;
        }
        const auto group = group_indexes.find(*segment.Group);
        if(group == group_indexes.end())
        {
            return failure(
                alignment_validation_error::segment_group_mismatch, index);
        }
        const auto& members = alignment.Groups[group->second].Members;
        if(index < static_cast<std::size_t>(members.Begin)
           || index >= static_cast<std::size_t>(members.End))
        {
            return failure(alignment_validation_error::segment_group_mismatch,
                index,
                group->second);
        }
    }

    auto pair_indexes = std::map<line_pair_id, std::size_t>{};
    auto paired_segments = std::set<std::size_t>{};
    auto prior_pair_segment = std::optional<std::size_t>{};
    auto prior_left = std::uint32_t{0U};
    auto prior_right = std::uint32_t{0U};
    for(std::size_t index = 0U; index < alignment.Pairs.size(); ++index)
    {
        const auto& pair = alignment.Pairs[index];
        const auto ordinal = static_cast<std::uint32_t>(index);
        if(!valid_line_pair_kind(pair.Kind))
        {
            return failure(
                alignment_validation_error::invalid_pair_kind, index);
        }
        const auto found_segment = segment_indexes.find(pair.Segment);
        if(found_segment == segment_indexes.end())
        {
            return failure(
                alignment_validation_error::unknown_pair_segment, index);
        }
        const auto& segment = alignment.Segments[found_segment->second];
        if(segment.Kind != alignment_kind::replacement
           || pair.LeftLine < segment.Left.Begin
           || pair.LeftLine >= segment.Left.End
           || pair.RightLine < segment.Right.Begin
           || pair.RightLine >= segment.Right.End)
        {
            return failure(
                alignment_validation_error::pair_out_of_bounds, index);
        }
        if(prior_pair_segment.has_value()
           && (found_segment->second < *prior_pair_segment
               || (found_segment->second == *prior_pair_segment
                   && (pair.LeftLine <= prior_left
                       || pair.RightLine <= prior_right))))
        {
            return failure(
                alignment_validation_error::nonmonotonic_pair, index);
        }
        if(!pair.Id.valid()
           || pair.Id
               != pair_identity(alignment.Digest,
                   pair.Segment,
                   pair.LeftLine,
                   pair.RightLine,
                   pair.Kind,
                   ordinal))
        {
            return failure(
                alignment_validation_error::invalid_pair_id, index);
        }
        const auto [iterator, inserted] = pair_indexes.emplace(pair.Id, index);
        if(!inserted)
        {
            return failure(alignment_validation_error::duplicate_pair_id,
                index,
                iterator->second);
        }
        paired_segments.insert(found_segment->second);
        prior_pair_segment = found_segment->second;
        prior_left = pair.LeftLine;
        prior_right = pair.RightLine;
    }

    auto move_indexes = std::map<move_relation_id, std::size_t>{};
    auto move_pairs =
        std::map<std::pair<std::size_t, std::size_t>, std::size_t>{};
    auto moved_sources = std::set<std::size_t>{};
    auto moved_destinations = std::set<std::size_t>{};
    auto prior_move_pair =
        std::optional<std::pair<std::size_t, std::size_t>>{};
    for(std::size_t index = 0U; index < alignment.Moves.size(); ++index)
    {
        const auto& move = alignment.Moves[index];
        const auto ordinal = static_cast<std::uint32_t>(index);
        const auto source = segment_indexes.find(move.SourceDeletion);
        const auto destination =
            segment_indexes.find(move.DestinationInsertion);
        if(source == segment_indexes.end()
           || destination == segment_indexes.end())
        {
            return failure(
                alignment_validation_error::unknown_move_segment, index);
        }
        if(alignment.Segments[source->second].Kind
               != alignment_kind::left_only
           || alignment.Segments[destination->second].Kind
               != alignment_kind::right_only
           || alignment.Segments[source->second].Left.extent()
               != alignment.Segments[destination->second].Right.extent())
        {
            return failure(
                alignment_validation_error::invalid_move_topology, index);
        }
        const auto current_move_pair =
            std::pair{source->second, destination->second};
        const auto existing_relation = move_pairs.find(current_move_pair);
        if(existing_relation != move_pairs.end())
        {
            return failure(
                alignment_validation_error::duplicate_move_relation,
                index,
                existing_relation->second);
        }
        if(moved_sources.contains(source->second)
           || moved_destinations.contains(destination->second))
        {
            return failure(
                alignment_validation_error::invalid_move_topology, index);
        }
        if(prior_move_pair.has_value()
           && current_move_pair < *prior_move_pair)
        {
            return failure(
                alignment_validation_error::noncanonical_move_order,
                index,
                index - 1U);
        }
        if(!move.Id.valid()
           || move.Id
               != move_identity(alignment.Digest,
                   move.SourceDeletion,
                   move.DestinationInsertion,
                   ordinal))
        {
            return failure(
                alignment_validation_error::invalid_move_id, index);
        }
        const auto [iterator, inserted] = move_indexes.emplace(move.Id, index);
        if(!inserted)
        {
            return failure(alignment_validation_error::duplicate_move_id,
                index,
                iterator->second);
        }
        move_pairs.emplace(current_move_pair, index);
        moved_sources.insert(source->second);
        moved_destinations.insert(destination->second);
        prior_move_pair = current_move_pair;
    }

    // Same-kind boundaries are canonical only when secondary semantic
    // metadata needs the two stable segment identities to remain distinct.
    for(std::size_t index = 1U; index < alignment.Segments.size(); ++index)
    {
        const auto& prior = alignment.Segments[index - 1U];
        const auto& current = alignment.Segments[index];
        if(prior.Kind != current.Kind)
        {
            continue;
        }
        const auto pair_refinement =
            current.Kind == alignment_kind::replacement
            && (paired_segments.contains(index - 1U)
                || paired_segments.contains(index));
        const auto moved_deletion_boundary =
            current.Kind == alignment_kind::left_only
            && (moved_sources.contains(index - 1U)
                || moved_sources.contains(index));
        const auto moved_insertion_boundary =
            current.Kind == alignment_kind::right_only
            && (moved_destinations.contains(index - 1U)
                || moved_destinations.contains(index));
        if(!pair_refinement && !moved_deletion_boundary
           && !moved_insertion_boundary)
        {
            return failure(
                alignment_validation_error::noncanonical_adjacent_segment,
                index,
                index - 1U);
        }
    }
    return {};
}
} // namespace bpr::core
