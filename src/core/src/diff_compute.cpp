#include "bpr/core/diff_compute.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace bpr::core
{
namespace
{
struct unicode_scalar
{
    std::uint32_t Value = 0U;
    std::uint64_t Begin = 0U;
    std::uint64_t End = 0U;
};

struct normalized_line
{
    std::vector<std::uint32_t> Values;

    auto operator<=>(const normalized_line&) const = default;
};

struct prepared_document
{
    decoded_text_document Document;
    std::vector<normalized_line> Normalized;
};

struct pending_segment
{
    alignment_kind Kind = alignment_kind::equal;
    line_span Left;
    line_span Right;
    std::optional<line_pair_kind> PairKind;
};

struct accepted_pair
{
    std::uint32_t Left = 0U;
    std::uint32_t Right = 0U;
    line_pair_kind Kind = line_pair_kind::similar;
};

struct matrix_budget
{
    std::uint64_t Limit = 0U;
    std::uint64_t Consumed = 0U;

    [[nodiscard]] auto consume(const std::size_t left,
                               const std::size_t right) noexcept -> bool
    {
        if(left == 0U || right == 0U)
        {
            return true;
        }
        const auto left_value = static_cast<std::uint64_t>(left);
        const auto right_value = static_cast<std::uint64_t>(right);
        if(left_value > std::numeric_limits<std::uint64_t>::max() / right_value)
        {
            return false;
        }
        const auto cells = left_value * right_value;
        if(cells > Limit || Consumed > Limit - cells)
        {
            return false;
        }
        Consumed += cells;
        return true;
    }
};

[[nodiscard]] auto valid_utf8(const std::span<const std::uint8_t> bytes,
                              std::vector<unicode_scalar>* output = nullptr)
    -> bool
{
    auto index = std::size_t{0U};
    while(index < bytes.size())
    {
        const auto first = bytes[index];
        auto length = std::size_t{0U};
        auto value = std::uint32_t{0U};
        auto minimum = std::uint32_t{0U};
        if(first <= 0x7FU)
        {
            length = 1U;
            value = first;
        }
        else if(first >= 0xC2U && first <= 0xDFU)
        {
            length = 2U;
            value = static_cast<std::uint32_t>(first & 0x1FU);
            minimum = 0x80U;
        }
        else if(first >= 0xE0U && first <= 0xEFU)
        {
            length = 3U;
            value = static_cast<std::uint32_t>(first & 0x0FU);
            minimum = 0x800U;
        }
        else if(first >= 0xF0U && first <= 0xF4U)
        {
            length = 4U;
            value = static_cast<std::uint32_t>(first & 0x07U);
            minimum = 0x10000U;
        }
        else
        {
            return false;
        }
        if(length > bytes.size() - index)
        {
            return false;
        }
        for(std::size_t offset = 1U; offset < length; ++offset)
        {
            const auto continuation = bytes[index + offset];
            if((continuation & 0xC0U) != 0x80U)
            {
                return false;
            }
            value = static_cast<std::uint32_t>((value << 6U) |
                                               (continuation & 0x3FU));
        }
        if(value < minimum || value > 0x10FFFFU ||
           (value >= 0xD800U && value <= 0xDFFFU))
        {
            return false;
        }
        if(output != nullptr)
        {
            output->push_back(unicode_scalar{
                .Value = value,
                .Begin = static_cast<std::uint64_t>(index),
                .End = static_cast<std::uint64_t>(index + length),
            });
        }
        index += length;
    }
    return true;
}

[[nodiscard]] auto line_bytes(const decoded_text_document& document,
                              const std::uint32_t line)
    -> std::span<const std::uint8_t>
{
    const auto& record = document.Lines[static_cast<std::size_t>(line)];
    const auto begin = static_cast<std::size_t>(record.Content.Begin);
    const auto end = static_cast<std::size_t>(record.Content.End);
    return std::span<const std::uint8_t>{document.Bytes}.subspan(begin,
                                                                 end - begin);
}

[[nodiscard]] auto byte_boundary(const decoded_text_document& document,
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

[[nodiscard]] auto valid_document(const decoded_text_document& document) -> bool
{
    if(!valid_utf8(document.Bytes) ||
       std::ranges::find(document.Bytes, std::uint8_t{0U}) !=
           document.Bytes.end())
    {
        return false;
    }
    if(document.Lines.empty())
    {
        return document.Bytes.empty() && !document.HasFinalNewline;
    }

    auto cursor = std::uint64_t{0U};
    for(std::size_t index = 0U; index < document.Lines.size(); ++index)
    {
        const auto& line = document.Lines[index];
        if(line.Content.Begin != cursor ||
           line.Content.Begin > line.Content.End ||
           line.Content.End != line.Terminator.Begin ||
           line.Terminator.Begin > line.Terminator.End ||
           line.Terminator.End > document.Bytes.size())
        {
            return false;
        }
        const auto begin = static_cast<std::size_t>(line.Terminator.Begin);
        const auto extent = line.Terminator.End - line.Terminator.Begin;
        switch(line.Ending)
        {
        case text_line_ending::none:
            if(extent != 0U || index + 1U != document.Lines.size())
            {
                return false;
            }
            break;
        case text_line_ending::lf:
            if(extent != 1U || document.Bytes[begin] != '\n')
            {
                return false;
            }
            break;
        case text_line_ending::cr:
            if(extent != 1U || document.Bytes[begin] != '\r')
            {
                return false;
            }
            break;
        case text_line_ending::crlf:
            if(extent != 2U || document.Bytes[begin] != '\r' ||
               document.Bytes[begin + 1U] != '\n')
            {
                return false;
            }
            break;
        }
        cursor = line.Terminator.End;
    }
    return cursor == document.Bytes.size() &&
           document.HasFinalNewline ==
               (document.Lines.back().Ending != text_line_ending::none);
}

[[nodiscard]] constexpr auto compute_failure(
    const diff_compute_validation_error error,
    const std::size_t record = diff_compute_validation_result::no_record)
    -> diff_compute_validation_result
{
    return {.Error = error, .Record = record};
}

[[nodiscard]] auto normalize_line(const decoded_text_document& document,
                                  const decoded_text_line& line,
                                  const diff_options& options)
    -> normalized_line
{
    const auto begin = static_cast<std::size_t>(line.Content.Begin);
    const auto end = static_cast<std::size_t>(line.Content.End);
    auto scalars = std::vector<unicode_scalar>{};
    static_cast<void>(
        valid_utf8(std::span<const std::uint8_t>{document.Bytes}.subspan(
                       begin, end - begin),
                   &scalars));
    auto result = normalized_line{};
    result.Values.reserve(scalars.size() + 1U);
    for(const auto& scalar : scalars)
    {
        if(options.Whitespace == whitespace_policy::ignore_all &&
           (scalar.Value == 0x20U || scalar.Value == 0x09U))
        {
            continue;
        }
        result.Values.push_back(scalar.Value);
    }
    if(options.Whitespace == whitespace_policy::ignore_trailing)
    {
        while(!result.Values.empty() &&
              (result.Values.back() == 0x20U || result.Values.back() == 0x09U))
        {
            result.Values.pop_back();
        }
    }
    if(line.Ending != text_line_ending::none)
    {
        const auto marker = options.LineEndings == line_ending_policy::normalize
                                ? 0x110000U
                            : line.Ending == text_line_ending::lf ? 0x110000U
                            : line.Ending == text_line_ending::cr ? 0x110001U
                                                                  : 0x110002U;
        result.Values.push_back(marker);
    }
    return result;
}

[[nodiscard]] auto normalized_lines_equal(const decoded_text_document& left,
                                          const std::uint32_t left_line,
                                          const decoded_text_document& right,
                                          const std::uint32_t right_line,
                                          const diff_options& options) -> bool
{
    return normalize_line(left,
                          left.Lines[static_cast<std::size_t>(left_line)],
                          options) ==
           normalize_line(right,
                          right.Lines[static_cast<std::size_t>(right_line)],
                          options);
}

[[nodiscard]] auto prepare_document(const std::span<const std::uint8_t> bytes,
                                    const diff_options& options,
                                    const std::uint32_t maximum_lines)
    -> std::optional<prepared_document>
{
    if(!valid_utf8(bytes))
    {
        return std::nullopt;
    }
    auto result = prepared_document{};
    result.Document.Bytes.assign(bytes.begin(), bytes.end());
    auto line_start = std::size_t{0U};
    for(std::size_t index = 0U; index < bytes.size(); ++index)
    {
        const auto current = bytes[index];
        if(current != static_cast<std::uint8_t>('\n') &&
           current != static_cast<std::uint8_t>('\r'))
        {
            continue;
        }
        const auto crlf = current == static_cast<std::uint8_t>('\r') &&
                          index + 1U < bytes.size() &&
                          bytes[index + 1U] == static_cast<std::uint8_t>('\n');
        const auto ending = crlf ? text_line_ending::crlf
                            : current == static_cast<std::uint8_t>('\r')
                                ? text_line_ending::cr
                                : text_line_ending::lf;
        const auto end = crlf ? index + 2U : index + 1U;
        result.Document.Lines.push_back(decoded_text_line{
            .Content = {static_cast<std::uint64_t>(line_start),
                        static_cast<std::uint64_t>(index)},
            .Terminator = {static_cast<std::uint64_t>(index),
                           static_cast<std::uint64_t>(end)},
            .Ending = ending,
        });
        if(result.Document.Lines.size() >
           static_cast<std::size_t>(maximum_lines))
        {
            return std::nullopt;
        }
        line_start = end;
        if(crlf)
        {
            ++index;
        }
    }
    if(line_start < bytes.size())
    {
        result.Document.Lines.push_back(decoded_text_line{
            .Content = {static_cast<std::uint64_t>(line_start),
                        static_cast<std::uint64_t>(bytes.size())},
            .Terminator = {static_cast<std::uint64_t>(bytes.size()),
                           static_cast<std::uint64_t>(bytes.size())},
            .Ending = text_line_ending::none,
        });
    }
    if(result.Document.Lines.size() > static_cast<std::size_t>(maximum_lines))
    {
        return std::nullopt;
    }
    result.Document.HasFinalNewline =
        !bytes.empty() && (bytes.back() == static_cast<std::uint8_t>('\n') ||
                           bytes.back() == static_cast<std::uint8_t>('\r'));
    result.Normalized.reserve(result.Document.Lines.size());
    for(const auto& line : result.Document.Lines)
    {
        result.Normalized.push_back(
            normalize_line(result.Document, line, options));
    }
    return result;
}

template <typename Left, typename Right, typename Equal>
[[nodiscard]] auto lcs_matches(const Left& left,
                               const Right& right,
                               Equal equal,
                               matrix_budget& budget)
    -> std::optional<std::vector<std::pair<std::size_t, std::size_t>>>
{
    if(left.empty() || right.empty())
    {
        return std::vector<std::pair<std::size_t, std::size_t>>{};
    }
    if(left.size() == std::numeric_limits<std::size_t>::max() ||
       right.size() == std::numeric_limits<std::size_t>::max())
    {
        return std::nullopt;
    }
    const auto rows = left.size() + 1U;
    const auto columns = right.size() + 1U;
    if(rows > std::numeric_limits<std::size_t>::max() / columns)
    {
        return std::nullopt;
    }
    if(!budget.consume(rows, columns))
    {
        return std::nullopt;
    }
    auto table = std::vector<std::uint32_t>(rows * columns, 0U);
    const auto at = [&table,
                     columns](const std::size_t row,
                              const std::size_t column) -> std::uint32_t&
    { return table[row * columns + column]; };
    for(auto row = left.size(); row != 0U; --row)
    {
        for(auto column = right.size(); column != 0U; --column)
        {
            if(equal(left[row - 1U], right[column - 1U]))
            {
                at(row - 1U, column - 1U) =
                    static_cast<std::uint32_t>(1U + at(row, column));
            }
            else
            {
                at(row - 1U, column - 1U) =
                    std::max(at(row, column - 1U), at(row - 1U, column));
            }
        }
    }
    auto result = std::vector<std::pair<std::size_t, std::size_t>>{};
    auto row = std::size_t{0U};
    auto column = std::size_t{0U};
    while(row < left.size() && column < right.size())
    {
        if(equal(left[row], right[column]))
        {
            result.emplace_back(row, column);
            ++row;
            ++column;
        }
        else if(at(row + 1U, column) >= at(row, column + 1U))
        {
            ++row;
        }
        else
        {
            ++column;
        }
    }
    return result;
}

[[nodiscard]] auto similarity(const normalized_line& left,
                              const normalized_line& right,
                              matrix_budget& budget)
    -> std::optional<std::uint16_t>
{
    if(left == right)
    {
        return maximum_similarity_threshold_permille;
    }
    if(left.Values.empty() || right.Values.empty())
    {
        return std::uint16_t{0U};
    }
    const auto matches = lcs_matches(
        left.Values,
        right.Values,
        [](const std::uint32_t lhs, const std::uint32_t rhs)
        { return lhs == rhs; },
        budget);
    if(!matches.has_value())
    {
        return std::nullopt;
    }
    const auto numerator = static_cast<std::uint64_t>(matches->size()) * 2U *
                           maximum_similarity_threshold_permille;
    const auto denominator = static_cast<std::uint64_t>(left.Values.size()) +
                             static_cast<std::uint64_t>(right.Values.size());
    return static_cast<std::uint16_t>(numerator / denominator);
}

[[nodiscard]] auto secondary_pairs(const prepared_document& left,
                                   const prepared_document& right,
                                   const line_span left_span,
                                   const line_span right_span,
                                   const diff_options& options,
                                   matrix_budget& budget)
    -> std::optional<std::vector<accepted_pair>>
{
    if(options.Pairing == pairing_policy::disabled)
    {
        return std::vector<accepted_pair>{};
    }
    const auto left_count = static_cast<std::size_t>(left_span.extent());
    const auto right_count = static_cast<std::size_t>(right_span.extent());
    if((left_count != 0U &&
        right_count > std::numeric_limits<std::size_t>::max() / left_count) ||
       left_count == std::numeric_limits<std::size_t>::max() ||
       right_count == std::numeric_limits<std::size_t>::max())
    {
        return std::nullopt;
    }
    const auto cells = left_count * right_count;
    const auto rows = left_count + 1U;
    const auto columns = right_count + 1U;
    if(rows > std::numeric_limits<std::size_t>::max() / columns)
    {
        return std::nullopt;
    }
    if(!budget.consume(left_count, right_count) ||
       !budget.consume(rows, columns))
    {
        return std::nullopt;
    }
    auto scores = std::vector<std::uint16_t>(cells, 0U);
    auto table = std::vector<std::uint64_t>(rows * columns, 0U);
    for(std::size_t left_index = 0U; left_index < left_count; ++left_index)
    {
        for(std::size_t right_index = 0U; right_index < right_count;
            ++right_index)
        {
            const auto score = similarity(
                left.Normalized[static_cast<std::size_t>(left_span.Begin) +
                                left_index],
                right.Normalized[static_cast<std::size_t>(right_span.Begin) +
                                 right_index],
                budget);
            if(!score.has_value())
            {
                return std::nullopt;
            }
            scores[left_index * right_count + right_index] = *score;
        }
    }
    const auto at = [&table,
                     columns](const std::size_t row,
                              const std::size_t column) -> std::uint64_t&
    { return table[row * columns + column]; };
    for(auto row = left_count; row != 0U; --row)
    {
        for(auto column = right_count; column != 0U; --column)
        {
            const auto score = scores[(row - 1U) * right_count + column - 1U];
            auto best = std::max(at(row, column - 1U), at(row - 1U, column));
            if(score >= options.SimilarityThresholdPermille)
            {
                best = std::max(
                    best, static_cast<std::uint64_t>(score) + at(row, column));
            }
            at(row - 1U, column - 1U) = best;
        }
    }
    auto result = std::vector<accepted_pair>{};
    auto row = std::size_t{0U};
    auto column = std::size_t{0U};
    while(row < left_count && column < right_count)
    {
        const auto score = scores[row * right_count + column];
        const auto paired =
            score >= options.SimilarityThresholdPermille &&
            at(row, column) ==
                static_cast<std::uint64_t>(score) + at(row + 1U, column + 1U);
        if(paired)
        {
            result.push_back(accepted_pair{
                .Left = static_cast<std::uint32_t>(
                    static_cast<std::uint64_t>(left_span.Begin) + row),
                .Right = static_cast<std::uint32_t>(
                    static_cast<std::uint64_t>(right_span.Begin) + column),
                .Kind = score == maximum_similarity_threshold_permille
                            ? line_pair_kind::exact
                            : line_pair_kind::similar,
            });
            ++row;
            ++column;
        }
        else if(at(row + 1U, column) >= at(row, column + 1U))
        {
            ++row;
        }
        else
        {
            ++column;
        }
    }
    return result;
}

auto append_shape(std::vector<pending_segment>& segments,
                  const line_span left,
                  const line_span right,
                  const std::optional<line_pair_kind> pair = std::nullopt)
    -> void
{
    if(left.empty() && right.empty())
    {
        return;
    }
    const auto kind = left.empty()    ? alignment_kind::right_only
                      : right.empty() ? alignment_kind::left_only
                                      : alignment_kind::replacement;
    segments.push_back(pending_segment{
        .Kind = kind,
        .Left = left,
        .Right = right,
        .PairKind = pair,
    });
}

[[nodiscard]] auto primary_segments(const prepared_document& left,
                                    const prepared_document& right,
                                    const diff_options& options,
                                    matrix_budget& budget)
    -> std::optional<std::vector<pending_segment>>
{
    const auto matches = lcs_matches(
        left.Normalized,
        right.Normalized,
        [](const normalized_line& lhs, const normalized_line& rhs)
        { return lhs == rhs; },
        budget);
    if(!matches.has_value())
    {
        return std::nullopt;
    }
    auto primary = std::vector<pending_segment>{};
    auto left_cursor = std::uint32_t{0U};
    auto right_cursor = std::uint32_t{0U};
    auto match_index = std::size_t{0U};
    while(match_index < matches->size())
    {
        const auto match_left =
            static_cast<std::uint32_t>((*matches)[match_index].first);
        const auto match_right =
            static_cast<std::uint32_t>((*matches)[match_index].second);
        append_shape(
            primary, {left_cursor, match_left}, {right_cursor, match_right});
        auto equal_left_end = match_left + 1U;
        auto equal_right_end = match_right + 1U;
        ++match_index;
        while(match_index < matches->size() &&
              (*matches)[match_index].first == equal_left_end &&
              (*matches)[match_index].second == equal_right_end)
        {
            ++equal_left_end;
            ++equal_right_end;
            ++match_index;
        }
        primary.push_back(pending_segment{
            .Kind = alignment_kind::equal,
            .Left = {match_left, equal_left_end},
            .Right = {match_right, equal_right_end},
        });
        left_cursor = equal_left_end;
        right_cursor = equal_right_end;
    }
    append_shape(
        primary,
        {left_cursor, static_cast<std::uint32_t>(left.Normalized.size())},
        {right_cursor, static_cast<std::uint32_t>(right.Normalized.size())});

    auto refined = std::vector<pending_segment>{};
    for(const auto& segment : primary)
    {
        if(segment.Kind != alignment_kind::replacement)
        {
            refined.push_back(segment);
            continue;
        }
        const auto pairs = secondary_pairs(
            left, right, segment.Left, segment.Right, options, budget);
        if(!pairs.has_value())
        {
            return std::nullopt;
        }
        auto left_position = segment.Left.Begin;
        auto right_position = segment.Right.Begin;
        for(const auto& pair : *pairs)
        {
            append_shape(refined,
                         {left_position, pair.Left},
                         {right_position, pair.Right});
            append_shape(
                refined,
                {pair.Left, static_cast<std::uint32_t>(pair.Left + 1U)},
                {pair.Right, static_cast<std::uint32_t>(pair.Right + 1U)},
                pair.Kind);
            left_position = pair.Left + 1U;
            right_position = pair.Right + 1U;
        }
        append_shape(refined,
                     {left_position, segment.Left.End},
                     {right_position, segment.Right.End});
    }
    return refined;
}

[[nodiscard]] constexpr auto
summary_kind(const std::uint64_t added,
             const std::uint64_t deleted,
             const std::uint64_t replaced_left,
             const std::uint64_t replaced_right) noexcept -> change_summary_kind
{
    const auto addition = added != 0U;
    const auto deletion = deleted != 0U;
    const auto replacement = replaced_left != 0U || replaced_right != 0U;
    if(addition && !deletion && !replacement)
    {
        return change_summary_kind::addition;
    }
    if(!addition && deletion && !replacement)
    {
        return change_summary_kind::deletion;
    }
    if(!addition && !deletion && replacement)
    {
        return change_summary_kind::replacement;
    }
    return change_summary_kind::mixed;
}

[[nodiscard]] auto nontext_result(const diff_compute_input& input,
                                  const diff_content_state content,
                                  const diff_reason_code reason)
    -> diff_compute_outcome
{
    auto alignment = file_alignment{
        .Key = input.Key,
        .Options = input.Options,
        .Digest = comparison_digest(input.Key),
        .Content = content,
        .Diagnostic = {.Code = reason},
    };
    if(!validate_alignment(alignment).valid())
    {
        return {.Reason = diff_reason_code::invalid_alignment};
    }
    auto result = computed_file_diff{.Alignment = std::move(alignment)};
    if(!validate_computed_file_diff(result).valid())
    {
        return {.Reason = diff_reason_code::invalid_alignment};
    }
    return {.Reason = reason, .Result = std::move(result)};
}

[[nodiscard]] auto same_segment_text(const pending_segment& left_segment,
                                     const pending_segment& right_segment,
                                     const prepared_document& left,
                                     const prepared_document& right) -> bool
{
    if(left_segment.Left.extent() != right_segment.Right.extent())
    {
        return false;
    }
    for(std::uint64_t offset = 0U; offset < left_segment.Left.extent();
        ++offset)
    {
        if(left.Normalized[static_cast<std::size_t>(
               static_cast<std::uint64_t>(left_segment.Left.Begin) + offset)] !=
           right.Normalized[static_cast<std::size_t>(
               static_cast<std::uint64_t>(right_segment.Right.Begin) + offset)])
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] auto intraline_ranges(const decoded_text_document& left,
                                    const decoded_text_document& right,
                                    const std::uint32_t left_line,
                                    const std::uint32_t right_line,
                                    matrix_budget& budget)
    -> std::optional<std::vector<std::pair<byte_span, byte_span>>>
{
    auto left_scalars = std::vector<unicode_scalar>{};
    auto right_scalars = std::vector<unicode_scalar>{};
    static_cast<void>(valid_utf8(line_bytes(left, left_line), &left_scalars));
    static_cast<void>(
        valid_utf8(line_bytes(right, right_line), &right_scalars));
    const auto matches = lcs_matches(
        left_scalars,
        right_scalars,
        [](const unicode_scalar& lhs, const unicode_scalar& rhs)
        { return lhs.Value == rhs.Value; },
        budget);
    if(!matches.has_value())
    {
        return std::nullopt;
    }
    const auto left_size =
        static_cast<std::uint64_t>(line_bytes(left, left_line).size());
    const auto right_size =
        static_cast<std::uint64_t>(line_bytes(right, right_line).size());
    auto result = std::vector<std::pair<byte_span, byte_span>>{};
    auto prior_left = std::uint64_t{0U};
    auto prior_right = std::uint64_t{0U};
    for(const auto& [left_index, right_index] : *matches)
    {
        const auto left_begin = left_scalars[left_index].Begin;
        const auto right_begin = right_scalars[right_index].Begin;
        if(prior_left != left_begin || prior_right != right_begin)
        {
            result.emplace_back(byte_span{prior_left, left_begin},
                                byte_span{prior_right, right_begin});
        }
        prior_left = left_scalars[left_index].End;
        prior_right = right_scalars[right_index].End;
    }
    if(prior_left != left_size || prior_right != right_size)
    {
        result.emplace_back(byte_span{prior_left, left_size},
                            byte_span{prior_right, right_size});
    }
    return result;
}
} // namespace

auto validate_computed_file_diff(const computed_file_diff& diff)
    -> diff_compute_validation_result
{
    if(!validate_alignment(diff.Alignment).valid())
    {
        return compute_failure(
            diff_compute_validation_error::invalid_alignment);
    }
    if(diff.Alignment.Content != diff_content_state::text)
    {
        return !diff.LeftDocument.has_value() &&
                       !diff.RightDocument.has_value() &&
                       diff.Intraline.empty() && diff.MatrixCellsConsumed == 0U
                   ? diff_compute_validation_result{}
                   : compute_failure(
                         diff_compute_validation_error::records_for_non_text);
    }

    if(diff.LeftDocument.has_value() !=
           diff.Alignment.Key.LeftBlob.has_value() ||
       diff.RightDocument.has_value() !=
           diff.Alignment.Key.RightBlob.has_value())
    {
        return compute_failure(
            diff_compute_validation_error::document_presence_mismatch);
    }
    if((diff.LeftDocument.has_value() &&
        diff.LeftDocument->Lines.size() != diff.Alignment.LeftLineCount) ||
       (diff.RightDocument.has_value() &&
        diff.RightDocument->Lines.size() != diff.Alignment.RightLineCount))
    {
        return compute_failure(
            diff_compute_validation_error::document_line_count_mismatch);
    }
    if((diff.LeftDocument.has_value() && !valid_document(*diff.LeftDocument)) ||
       (diff.RightDocument.has_value() && !valid_document(*diff.RightDocument)))
    {
        return compute_failure(diff_compute_validation_error::invalid_document);
    }

    for(std::size_t index = 0U; index < diff.Alignment.Moves.size(); ++index)
    {
        const auto& move = diff.Alignment.Moves[index];
        const auto source = std::ranges::find(diff.Alignment.Segments,
                                              move.SourceDeletion,
                                              &alignment_segment::Id);
        const auto destination = std::ranges::find(diff.Alignment.Segments,
                                                   move.DestinationInsertion,
                                                   &alignment_segment::Id);
        if(!diff.LeftDocument.has_value() || !diff.RightDocument.has_value() ||
           source == diff.Alignment.Segments.end() ||
           destination == diff.Alignment.Segments.end() ||
           source->Left.extent() != destination->Right.extent())
        {
            return compute_failure(
                diff_compute_validation_error::invalid_move_content, index);
        }
        for(std::uint64_t offset = 0U; offset < source->Left.extent(); ++offset)
        {
            const auto left_line = static_cast<std::uint32_t>(
                static_cast<std::uint64_t>(source->Left.Begin) + offset);
            const auto right_line = static_cast<std::uint32_t>(
                static_cast<std::uint64_t>(destination->Right.Begin) + offset);
            if(!normalized_lines_equal(*diff.LeftDocument,
                                       left_line,
                                       *diff.RightDocument,
                                       right_line,
                                       diff.Alignment.Options))
            {
                return compute_failure(
                    diff_compute_validation_error::invalid_move_content, index);
            }
        }
    }

    auto pair_index = std::size_t{0U};
    auto prior_pair_index = diff_compute_validation_result::no_record;
    auto prior_left_end = std::uint64_t{0U};
    auto prior_right_end = std::uint64_t{0U};
    for(std::size_t index = 0U; index < diff.Intraline.size(); ++index)
    {
        const auto& edit = diff.Intraline[index];
        while(pair_index < diff.Alignment.Pairs.size() &&
              diff.Alignment.Pairs[pair_index].Id != edit.Pair)
        {
            ++pair_index;
        }
        if(pair_index == diff.Alignment.Pairs.size())
        {
            return compute_failure(
                diff_compute_validation_error::unknown_intraline_pair, index);
        }
        const auto& pair = diff.Alignment.Pairs[pair_index];
        if(pair.Kind != line_pair_kind::similar)
        {
            return compute_failure(
                diff_compute_validation_error::intraline_for_exact_pair, index);
        }
        const auto& left_line =
            diff.LeftDocument->Lines[static_cast<std::size_t>(pair.LeftLine)];
        const auto& right_line =
            diff.RightDocument->Lines[static_cast<std::size_t>(pair.RightLine)];
        if(edit.Left.Begin > edit.Left.End ||
           edit.Right.Begin > edit.Right.End ||
           (edit.Left.empty() && edit.Right.empty()) ||
           !byte_boundary(*diff.LeftDocument, left_line, edit.Left.Begin) ||
           !byte_boundary(*diff.LeftDocument, left_line, edit.Left.End) ||
           !byte_boundary(*diff.RightDocument, right_line, edit.Right.Begin) ||
           !byte_boundary(*diff.RightDocument, right_line, edit.Right.End))
        {
            return compute_failure(
                diff_compute_validation_error::invalid_intraline_span, index);
        }
        if(prior_pair_index != pair_index)
        {
            prior_pair_index = pair_index;
            prior_left_end = 0U;
            prior_right_end = 0U;
        }
        if(edit.Left.Begin < prior_left_end ||
           edit.Right.Begin < prior_right_end)
        {
            return compute_failure(
                diff_compute_validation_error::nonmonotonic_intraline, index);
        }
        prior_left_end = edit.Left.End;
        prior_right_end = edit.Right.End;
    }

    for(std::size_t index = 0U; index < diff.Alignment.Segments.size(); ++index)
    {
        const auto& segment = diff.Alignment.Segments[index];
        if(segment.Kind != alignment_kind::equal)
        {
            continue;
        }
        for(std::uint64_t offset = 0U; offset < segment.Left.extent(); ++offset)
        {
            const auto left_line = static_cast<std::uint32_t>(
                static_cast<std::uint64_t>(segment.Left.Begin) + offset);
            const auto right_line = static_cast<std::uint32_t>(
                static_cast<std::uint64_t>(segment.Right.Begin) + offset);
            if(!normalized_lines_equal(*diff.LeftDocument,
                                       left_line,
                                       *diff.RightDocument,
                                       right_line,
                                       diff.Alignment.Options))
            {
                return compute_failure(
                    diff_compute_validation_error::invalid_exact_content,
                    index);
            }
        }
    }
    for(std::size_t index = 0U; index < diff.Alignment.Pairs.size(); ++index)
    {
        const auto& pair = diff.Alignment.Pairs[index];
        if(pair.Kind == line_pair_kind::exact &&
           !normalized_lines_equal(*diff.LeftDocument,
                                   pair.LeftLine,
                                   *diff.RightDocument,
                                   pair.RightLine,
                                   diff.Alignment.Options))
        {
            return compute_failure(
                diff_compute_validation_error::invalid_exact_content, index);
        }
    }
    return {};
}

auto compute_file_diff(const diff_compute_input& input) -> diff_compute_outcome
{
    const auto digest = options_digest(input.Options);
    if(!valid(input.Key) || !digest.has_value() ||
       input.Key.Options != *digest ||
       (!input.Key.LeftBlob.has_value() && input.LeftBytes.has_value()) ||
       (!input.Key.RightBlob.has_value() && input.RightBytes.has_value()))
    {
        return {.Reason = diff_reason_code::invalid_alignment};
    }
    if(input.Key.LeftBlob.has_value() && !input.LeftBytes.has_value())
    {
        return nontext_result(input,
                              diff_content_state::unavailable,
                              diff_reason_code::missing_left_blob);
    }
    if(input.Key.RightBlob.has_value() && !input.RightBytes.has_value())
    {
        return nontext_result(input,
                              diff_content_state::unavailable,
                              diff_reason_code::missing_right_blob);
    }
    const auto left_bytes =
        input.LeftBytes.value_or(std::span<const std::uint8_t>{});
    const auto right_bytes =
        input.RightBytes.value_or(std::span<const std::uint8_t>{});
    if(static_cast<std::uint64_t>(left_bytes.size()) >
           input.Limits.MaximumBytesPerSide ||
       static_cast<std::uint64_t>(right_bytes.size()) >
           input.Limits.MaximumBytesPerSide)
    {
        return nontext_result(input,
                              diff_content_state::unavailable,
                              diff_reason_code::materialization_limit);
    }
    if(std::ranges::find(left_bytes, std::uint8_t{0U}) != left_bytes.end() ||
       std::ranges::find(right_bytes, std::uint8_t{0U}) != right_bytes.end())
    {
        return nontext_result(
            input, diff_content_state::binary, diff_reason_code::binary_file);
    }
    auto left = prepare_document(
        left_bytes, input.Options, input.Limits.MaximumLinesPerSide);
    auto right = prepare_document(
        right_bytes, input.Options, input.Limits.MaximumLinesPerSide);
    if(!left.has_value() || !right.has_value())
    {
        const auto decode_failed =
            !valid_utf8(left_bytes) || !valid_utf8(right_bytes);
        return nontext_result(input,
                              diff_content_state::unavailable,
                              decode_failed
                                  ? diff_reason_code::decode_failure
                                  : diff_reason_code::materialization_limit);
    }

    auto budget = matrix_budget{.Limit = input.Limits.MaximumMatrixCells};
    auto pending = primary_segments(*left, *right, input.Options, budget);
    if(!pending.has_value() ||
       pending->size() > static_cast<std::size_t>(
                             std::numeric_limits<std::uint32_t>::max()) ||
       pending->size() > static_cast<std::size_t>(input.Limits.MaximumSegments))
    {
        return nontext_result(input,
                              diff_content_state::unavailable,
                              diff_reason_code::compute_limit);
    }

    auto alignment = file_alignment{
        .Key = input.Key,
        .Options = input.Options,
        .Digest = comparison_digest(input.Key),
        .LeftLineCount =
            static_cast<std::uint32_t>(left->Document.Lines.size()),
        .RightLineCount =
            static_cast<std::uint32_t>(right->Document.Lines.size()),
    };
    auto group_ranges = std::vector<std::pair<std::uint32_t, std::uint32_t>>{};
    auto segment_groups =
        std::vector<std::optional<change_group_id>>(pending->size());
    for(std::size_t index = 0U; index < pending->size();)
    {
        if((*pending)[index].Kind == alignment_kind::equal)
        {
            ++index;
            continue;
        }
        const auto begin = index;
        while(index < pending->size() &&
              (*pending)[index].Kind != alignment_kind::equal)
        {
            ++index;
        }
        group_ranges.emplace_back(static_cast<std::uint32_t>(begin),
                                  static_cast<std::uint32_t>(index));
    }
    for(std::size_t index = 0U; index < group_ranges.size(); ++index)
    {
        const auto [begin, end] = group_ranges[index];
        const auto& first = (*pending)[begin];
        const auto& last = (*pending)[end - 1U];
        const auto left_span = line_span{first.Left.Begin, last.Left.End};
        const auto right_span = line_span{first.Right.Begin, last.Right.End};
        const auto id = group_identity(alignment.Digest,
                                       left_span,
                                       right_span,
                                       static_cast<std::uint32_t>(index));
        auto added = std::uint64_t{0U};
        auto deleted = std::uint64_t{0U};
        auto replaced_left = std::uint64_t{0U};
        auto replaced_right = std::uint64_t{0U};
        for(auto member = begin; member < end; ++member)
        {
            const auto& segment = (*pending)[member];
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
            .Members = {begin, end},
            .Left = left_span,
            .Right = right_span,
            .First = {.Left = first.Left.Begin, .Right = first.Right.Begin},
            .Last = {.Left = last.Left.End, .Right = last.Right.End},
            .Kind = summary_kind(added, deleted, replaced_left, replaced_right),
            .AddedLines = added,
            .DeletedLines = deleted,
            .ReplacedLeftLines = replaced_left,
            .ReplacedRightLines = replaced_right,
        });
        for(auto member = begin; member < end; ++member)
        {
            segment_groups[static_cast<std::size_t>(member)] = id;
        }
    }

    alignment.Segments.reserve(pending->size());
    for(std::size_t index = 0U; index < pending->size(); ++index)
    {
        const auto& source = (*pending)[index];
        alignment.Segments.push_back(alignment_segment{
            .Id = segment_identity(alignment.Digest,
                                   source.Kind,
                                   source.Left,
                                   source.Right,
                                   static_cast<std::uint32_t>(index)),
            .Kind = source.Kind,
            .Left = source.Left,
            .Right = source.Right,
            .Group = segment_groups[index],
        });
    }

    auto pending_intraline =
        std::vector<std::vector<std::pair<byte_span, byte_span>>>{};
    for(std::size_t index = 0U; index < pending->size(); ++index)
    {
        const auto& source = (*pending)[index];
        if(!source.PairKind.has_value())
        {
            continue;
        }
        const auto ordinal = static_cast<std::uint32_t>(alignment.Pairs.size());
        const auto pair = line_pair{
            .Id = pair_identity(alignment.Digest,
                                alignment.Segments[index].Id,
                                source.Left.Begin,
                                source.Right.Begin,
                                *source.PairKind,
                                ordinal),
            .Segment = alignment.Segments[index].Id,
            .LeftLine = source.Left.Begin,
            .RightLine = source.Right.Begin,
            .Kind = *source.PairKind,
        };
        alignment.Pairs.push_back(pair);
        if(pair.Kind == line_pair_kind::similar)
        {
            auto ranges = intraline_ranges(left->Document,
                                           right->Document,
                                           pair.LeftLine,
                                           pair.RightLine,
                                           budget);
            if(!ranges.has_value())
            {
                return nontext_result(input,
                                      diff_content_state::unavailable,
                                      diff_reason_code::compute_limit);
            }
            pending_intraline.push_back(std::move(*ranges));
        }
        else
        {
            pending_intraline.emplace_back();
        }
    }

    if(input.Options.Moves == move_detection_policy::deterministic_exact)
    {
        for(std::size_t source = 0U; source < pending->size(); ++source)
        {
            if((*pending)[source].Kind != alignment_kind::left_only)
            {
                continue;
            }
            auto destinations = std::vector<std::size_t>{};
            for(std::size_t destination = 0U; destination < pending->size();
                ++destination)
            {
                if((*pending)[destination].Kind == alignment_kind::right_only &&
                   same_segment_text((*pending)[source],
                                     (*pending)[destination],
                                     *left,
                                     *right))
                {
                    destinations.push_back(destination);
                }
            }
            if(destinations.size() != 1U)
            {
                continue;
            }
            const auto destination = destinations.front();
            auto matching_sources = std::size_t{0U};
            for(std::size_t candidate = 0U; candidate < pending->size();
                ++candidate)
            {
                if((*pending)[candidate].Kind == alignment_kind::left_only &&
                   same_segment_text((*pending)[candidate],
                                     (*pending)[destination],
                                     *left,
                                     *right))
                {
                    ++matching_sources;
                }
            }
            if(matching_sources != 1U)
            {
                continue;
            }
            const auto ordinal =
                static_cast<std::uint32_t>(alignment.Moves.size());
            alignment.Moves.push_back(move_relation{
                .Id = move_identity(alignment.Digest,
                                    alignment.Segments[source].Id,
                                    alignment.Segments[destination].Id,
                                    ordinal),
                .SourceDeletion = alignment.Segments[source].Id,
                .DestinationInsertion = alignment.Segments[destination].Id,
            });
        }
    }
    if(!validate_alignment(alignment).valid())
    {
        return {.Reason = diff_reason_code::invalid_alignment};
    }
    auto result = computed_file_diff{
        .Alignment = std::move(alignment),
        .LeftDocument = input.Key.LeftBlob.has_value()
                            ? std::optional<decoded_text_document>{std::move(
                                  left->Document)}
                            : std::nullopt,
        .RightDocument = input.Key.RightBlob.has_value()
                             ? std::optional<decoded_text_document>{std::move(
                                   right->Document)}
                             : std::nullopt,
        .MatrixCellsConsumed = budget.Consumed,
    };
    auto pair_index = std::size_t{0U};
    for(const auto& ranges : pending_intraline)
    {
        const auto pair_id = result.Alignment.Pairs[pair_index].Id;
        for(const auto& [left_range, right_range] : ranges)
        {
            result.Intraline.push_back(intraline_edit{
                .Pair = pair_id,
                .Left = left_range,
                .Right = right_range,
            });
        }
        ++pair_index;
    }
    if(!validate_computed_file_diff(result).valid())
    {
        return {.Reason = diff_reason_code::invalid_alignment};
    }
    return {.Reason = diff_reason_code::none, .Result = std::move(result)};
}
} // namespace bpr::core
