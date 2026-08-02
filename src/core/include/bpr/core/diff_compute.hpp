#pragma once

#include "bpr/core/diff_model.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace bpr::core
{
enum class text_line_ending : std::uint8_t
{
    none,
    lf,
    cr,
    crlf,
};

struct byte_span
{
    std::uint64_t Begin = 0U;
    std::uint64_t End = 0U;

    [[nodiscard]] constexpr auto empty() const noexcept -> bool
    {
        return Begin == End;
    }

    auto operator<=>(const byte_span&) const = default;
};

struct decoded_text_line
{
    byte_span Content;
    byte_span Terminator;
    text_line_ending Ending = text_line_ending::none;

    auto operator<=>(const decoded_text_line&) const = default;
};

struct decoded_text_document
{
    std::vector<std::uint8_t> Bytes;
    std::vector<decoded_text_line> Lines;
    bool HasFinalNewline = false;

    auto operator<=>(const decoded_text_document&) const = default;
};

struct intraline_edit
{
    line_pair_id Pair;
    // Offsets are relative to each paired line's immutable Content span.
    byte_span Left;
    byte_span Right;

    auto operator<=>(const intraline_edit&) const = default;
};

struct diff_compute_limits
{
    std::uint64_t MaximumBytesPerSide = 16U * 1024U * 1024U;
    std::uint32_t MaximumLinesPerSide = 200'000U;
    std::uint64_t MaximumMatrixCells = 16U * 1024U * 1024U;
    std::uint32_t MaximumSegments = 1'000'000U;

    auto operator<=>(const diff_compute_limits&) const = default;
};

struct diff_compute_input
{
    file_comparison_key Key;
    diff_options Options;
    std::optional<std::span<const std::uint8_t>> LeftBytes;
    std::optional<std::span<const std::uint8_t>> RightBytes;
    diff_compute_limits Limits;
};

struct computed_file_diff
{
    file_alignment Alignment;
    std::optional<decoded_text_document> LeftDocument;
    std::optional<decoded_text_document> RightDocument;
    std::vector<intraline_edit> Intraline;
    std::uint64_t MatrixCellsConsumed = 0U;

    auto operator<=>(const computed_file_diff&) const = default;
};

struct diff_compute_outcome
{
    diff_reason_code Reason = diff_reason_code::none;
    std::optional<computed_file_diff> Result;

    [[nodiscard]] auto published() const noexcept -> bool
    {
        return Result.has_value();
    }
};

enum class diff_compute_validation_error : std::uint8_t
{
    none,
    invalid_alignment,
    records_for_non_text,
    document_presence_mismatch,
    document_line_count_mismatch,
    invalid_document,
    unknown_intraline_pair,
    intraline_for_exact_pair,
    invalid_intraline_span,
    nonmonotonic_intraline,
};

struct diff_compute_validation_result
{
    static constexpr auto no_record = std::numeric_limits<std::size_t>::max();

    diff_compute_validation_error Error = diff_compute_validation_error::none;
    std::size_t Record = no_record;

    [[nodiscard]] constexpr auto valid() const noexcept -> bool
    {
        return Error == diff_compute_validation_error::none;
    }
};

[[nodiscard]] auto compute_file_diff(const diff_compute_input& input)
    -> diff_compute_outcome;
[[nodiscard]] auto validate_computed_file_diff(const computed_file_diff& diff)
    -> diff_compute_validation_result;
} // namespace bpr::core
