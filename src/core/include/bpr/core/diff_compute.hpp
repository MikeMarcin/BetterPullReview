#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_DIFF_COMPUTE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_DIFF_COMPUTE_HPP

#include "bpr/core/diff_model.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace bpr::core {
	inline constexpr auto supported_diff_algorithm_revision = diff_algorithm_revision{ 1U };

	enum class text_line_ending : std::uint8_t {
		none,
		lf,
		cr,
		crlf,
	};

	/// \brief Describes the byte span record.
	struct byte_span
	{
		std::uint64_t Begin = 0U;
		std::uint64_t End   = 0U;

		[[nodiscard]] constexpr auto empty() const noexcept -> bool
		{
			return Begin == End;
		}

		auto operator<=>(const byte_span&) const = default;
	};

	/// \brief Describes the decoded text line record.
	struct decoded_text_line
	{
		byte_span        Content;
		byte_span        Terminator;
		text_line_ending Ending = text_line_ending::none;

		auto operator<=>(const decoded_text_line&) const = default;
	};

	/// \brief Describes the decoded text document record.
	struct decoded_text_document
	{
		std::vector<std::uint8_t>      Bytes;
		std::vector<decoded_text_line> Lines;
		bool                           HasFinalNewline = false;

		auto operator<=>(const decoded_text_document&) const = default;
	};

	/// \brief Describes the intraline edit record.
	struct intraline_edit
	{
		line_pair_id Pair;
		// Offsets are relative to each paired line's immutable Content span.
		byte_span Left;
		byte_span Right;

		auto operator<=>(const intraline_edit&) const = default;
	};

	/// \brief Describes the diff compute limits record.
	struct diff_compute_limits
	{
		std::uint64_t MaximumBytesPerSide      = 16U * 1024U * 1024U;
		std::uint32_t MaximumLinesPerSide      = 200'000U;
		std::uint64_t MaximumMatrixCells       = 16U * 1024U * 1024U;
		std::uint64_t MaximumMoveDetectionWork = 1'000'000U;
		std::uint32_t MaximumSegments          = 1'000'000U;
		/// Separate from matrix cells: bounds ordered line-index probes and values.
		std::uint64_t MaximumPrimaryIndexWork = 64U * 1024U * 1024U;

		auto operator<=>(const diff_compute_limits&) const = default;
	};

	/// \brief Describes the diff compute input record.
	struct diff_compute_input
	{
		file_comparison_key                          Key;
		diff_options                                 Options;
		std::optional<std::span<const std::uint8_t>> LeftBytes;
		std::optional<std::span<const std::uint8_t>> RightBytes;
		diff_compute_limits                          Limits;
	};

	/// \brief Describes the computed file diff record.
	struct computed_file_diff
	{
		file_alignment                       Alignment;
		std::optional<decoded_text_document> LeftDocument;
		std::optional<decoded_text_document> RightDocument;
		std::vector<intraline_edit>          Intraline;
		std::uint64_t                        MatrixCellsConsumed              = 0U;
		std::uint64_t                        MoveDetectionWorkConsumed        = 0U;
		bool                                 MoveDetectionWorkLimitReached    = false;
		bool                                 MoveDetectionSegmentLimitReached = false;
		std::uint64_t                        PrimaryIndexWorkConsumed         = 0U;
		/// [[BPR-0719]]: true when a work limit degraded the alignment (a
		/// patience or plain-replacement region, an unpaired replacement or an
		/// omitted intraline pass) instead of refusing the file. The alignment
		/// is still valid and covers both documents; it may not be the shortest.
		bool AlignmentApproximate = false;

		auto operator<=>(const computed_file_diff&) const = default;
	};

	/// \brief Describes the diff compute outcome record.
	struct diff_compute_outcome
	{
		diff_reason_code                  Reason = diff_reason_code::none;
		std::optional<computed_file_diff> Result;

		[[nodiscard]] auto published() const noexcept -> bool
		{
			return Result.has_value();
		}
	};

	enum class diff_compute_validation_error : std::uint8_t {
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
		invalid_exact_content,
		invalid_move_content,
		invalid_move_statistics,
	};

	/// \brief Describes the diff compute validation result record.
	struct diff_compute_validation_result
	{
		static constexpr auto no_record = std::numeric_limits<std::size_t>::max();

		diff_compute_validation_error Error  = diff_compute_validation_error::none;
		std::size_t                   Record = no_record;

		[[nodiscard]] constexpr auto valid() const noexcept -> bool
		{
			return Error == diff_compute_validation_error::none;
		}
	};

	/// \brief Inputs of a material comparison: bytes the owner resolved from
	/// its own immutable material, compared by the same engine and limits.
	struct material_diff_input
	{
		material_comparison_key                      Key;
		diff_options                                 Options;
		std::optional<std::span<const std::uint8_t>> LeftBytes;
		std::optional<std::span<const std::uint8_t>> RightBytes;
		diff_compute_limits                          Limits;
	};

	/// \brief A material comparison's result: the same records as
	/// `computed_file_diff`, keyed by the material comparison.
	struct computed_material_diff
	{
		material_alignment                   Alignment;
		std::optional<decoded_text_document> LeftDocument;
		std::optional<decoded_text_document> RightDocument;
		std::vector<intraline_edit>          Intraline;
		std::uint64_t                        MatrixCellsConsumed              = 0U;
		std::uint64_t                        MoveDetectionWorkConsumed        = 0U;
		bool                                 MoveDetectionWorkLimitReached    = false;
		bool                                 MoveDetectionSegmentLimitReached = false;
		std::uint64_t                        PrimaryIndexWorkConsumed         = 0U;
		bool                                 AlignmentApproximate             = false;

		auto operator<=>(const computed_material_diff&) const = default;
	};

	/// \brief Describes the material diff outcome record.
	struct material_diff_outcome
	{
		diff_reason_code                      Reason = diff_reason_code::none;
		std::optional<computed_material_diff> Result;

		[[nodiscard]] auto published() const noexcept -> bool
		{
			return Result.has_value();
		}
	};

	/// \brief Computes the compute file diff result.
	[[nodiscard]] auto compute_file_diff(const diff_compute_input& aInput)
		-> diff_compute_outcome;
	/// \brief Computes the validate computed file diff result.
	[[nodiscard]] auto validate_computed_file_diff(const computed_file_diff& aDiff)
		-> diff_compute_validation_result;
	/// \brief Compares two material sides with the shared engine: the same
	/// decoding, alignment, pairing, move and limit rules as
	/// `compute_file_diff`, and no pull-request identity.
	[[nodiscard]] auto compute_material_diff(const material_diff_input& aInput)
		-> material_diff_outcome;
	/// \brief Validates a material comparison by the rules applied to
	/// `computed_file_diff`.
	[[nodiscard]] auto validate_computed_material_diff(const computed_material_diff& aDiff)
		-> diff_compute_validation_result;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_DIFF_COMPUTE_HPP
