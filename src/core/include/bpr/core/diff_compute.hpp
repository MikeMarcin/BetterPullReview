#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_DIFF_COMPUTE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_DIFF_COMPUTE_HPP

#include "bpr/core/diff_model.hpp"

#include <array>
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
		/// [[BPR-0719]]: true when a work limit degraded the alignment (an
		/// unpaired replacement or an omitted intraline pass) instead of
		/// refusing the file. The alignment is still valid and covers both
		/// documents. [[BPR-1485]] The primary matches come from the host's
		/// line matcher, which no budget limits.
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

	/// \brief The engine's phases, in order. [[BPR-1366]]
	enum class diff_compute_phase : std::uint8_t {
		/// Binary and UTF-8 scans, line split and normalization of each side.
		prepare,
		/// The primary matches, supplied by the line matcher ([[BPR-1485]]).
		primary,
		/// Secondary pairing of each replacement segment.
		refine,
		/// Move detection, bounded by `MaximumMoveDetectionWork`.
		moves,
		/// Groups, segments, pairs with their intraline edits, and move
		/// relations.
		assemble,
		/// Validation of the alignment and the result.
		validate,
		finished,
	};

	/// \brief Steps between two polls of a caller's stop predicate. [[BPR-1366]]
	inline constexpr std::uint64_t default_diff_compute_check_interval = 64U * 1024U;

	/// \brief A caller's request to stop a computation, polled by the engine.
	///
	/// \details [[BPR-1366]]. The engine never reads a clock or a thread
	/// primitive: it counts steps (matrix cells, move-detection work, bytes
	/// scanned or normalized, weighted validation records) and asks this
	/// predicate every `CheckInterval` of them; the line matcher asks it while
	/// it waits. A stop is honoured at the poll that sees it, and abandons the
	/// computation: [[BPR-1485]] nothing is kept, and a comparison wanted again
	/// is computed from the start.
	class diff_compute_stop
	{
	public:
		diff_compute_stop()                                            = default;
		diff_compute_stop(const diff_compute_stop&)                    = default;
		diff_compute_stop(diff_compute_stop&&)                         = default;
		auto operator=(const diff_compute_stop&) -> diff_compute_stop& = default;
		auto operator=(diff_compute_stop&&) -> diff_compute_stop&      = default;
		virtual ~diff_compute_stop()                                   = default;

		/// \param aSteps The computation's steps so far.
		[[nodiscard]] virtual auto stop_requested(std::uint64_t aSteps) -> bool = 0;
	};

	/// \brief How a stoppable computation may be stopped. [[BPR-1366]]
	struct diff_compute_control
	{
		/// Not owned; absent for a computation nothing may stop.
		diff_compute_stop* Stop          = nullptr;
		std::uint64_t      CheckInterval = default_diff_compute_check_interval;
	};

	/// \brief The step accounting of one computation.
	struct diff_compute_statistics
	{
		/// Steps counted, up to the end or the stop.
		std::uint64_t Steps = 0U;
		/// The most steps a poll of the stop predicate has come after it was
		/// due, because one increment of work went past it: a stop is honoured
		/// within `CheckInterval` plus this many steps of its request.
		std::uint64_t LargestPollDelay = 0U;
		/// The phase the computation finished or stopped in.
		diff_compute_phase Phase = diff_compute_phase::prepare;
		/// The step at which each phase began, by `diff_compute_phase`; zero
		/// for a phase not begun. The same in every run of one computation.
		std::array<std::uint64_t, static_cast<std::size_t>(diff_compute_phase::finished)> PhaseStarts{ };

		auto operator<=>(const diff_compute_statistics&) const = default;
	};

	/// \brief One stoppable file comparison. [[BPR-1366]] [[BPR-1485]]
	struct file_diff_compute_run
	{
		/// The finished outcome; absent when the computation was stopped.
		std::optional<diff_compute_outcome> Outcome;
		diff_compute_statistics             Statistics;
	};

	/// \brief One stoppable material comparison.
	struct material_diff_compute_run
	{
		std::optional<material_diff_outcome> Outcome;
		diff_compute_statistics              Statistics;
	};

	class diff_components;

	/// \brief Computes one file comparison.
	///
	/// \details [[BPR-1485]] The primary line matches come from `aComponents`,
	/// which must supply them (the application host's line matcher, or a test
	/// double); the engine has no matcher of its own. A matcher that declines
	/// refuses the comparison with `host_failure`, and one that breaks the
	/// primary rules refuses it with `invalid_alignment`.
	[[nodiscard]] auto compute_file_diff(const diff_compute_input& aInput, diff_components& aComponents)
		-> diff_compute_outcome;
	/// \brief Computes one file comparison that `aControl` may stop. A stopped
	/// run has no outcome; a finished one equals `compute_file_diff`.
	[[nodiscard]] auto compute_file_diff(const diff_compute_input& aInput, const diff_compute_control& aControl,
										 diff_components& aComponents) -> file_diff_compute_run;
	/// \brief Computes the validate computed file diff result.
	[[nodiscard]] auto validate_computed_file_diff(const computed_file_diff& aDiff)
		-> diff_compute_validation_result;
	/// \brief Compares two material sides with the shared engine: the same
	/// decoding, alignment, pairing, move and limit rules as
	/// `compute_file_diff`, and no pull-request identity.
	[[nodiscard]] auto compute_material_diff(const material_diff_input& aInput, diff_components& aComponents)
		-> material_diff_outcome;
	/// \brief One material comparison that `aControl` may stop, with the
	/// rules of the file overload.
	[[nodiscard]] auto compute_material_diff(const material_diff_input& aInput, const diff_compute_control& aControl,
											 diff_components& aComponents) -> material_diff_compute_run;
	/// \brief Validates a material comparison by the rules applied to
	/// `computed_file_diff`.
	[[nodiscard]] auto validate_computed_material_diff(const computed_material_diff& aDiff)
		-> diff_compute_validation_result;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_DIFF_COMPUTE_HPP
