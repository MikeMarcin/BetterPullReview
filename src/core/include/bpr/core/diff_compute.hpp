#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_DIFF_COMPUTE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_DIFF_COMPUTE_HPP

#include "bpr/core/diff_model.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
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

	/// \brief The engine's phases, in order. [[BPR-1366]]
	enum class diff_compute_phase : std::uint8_t {
		/// Binary and UTF-8 scans, line split and normalization of each side;
		/// resumes at the chunk or line.
		prepare,
		/// The primary match work list; resumes at the region. One region's
		/// table or ordered index is a restart unit.
		primary,
		/// Secondary pairing of each replacement segment; resumes at the
		/// segment. One segment's tables are a restart unit.
		refine,
		/// Move detection; restarts, bounded by `MaximumMoveDetectionWork`.
		moves,
		/// Groups, segments, pairs with their intraline edits, and move
		/// relations; resumes at the record. One pair's table is a restart unit.
		assemble,
		/// Validation of the alignment and the result; resumes at the stage
		/// (and inside the equal-content stage at the line).
		validate,
		finished,
	};

	/// \brief Steps between two polls of a caller's stop predicate. [[BPR-1366]]
	inline constexpr std::uint64_t default_diff_compute_check_interval = 64U * 1024U;

	/// \brief A caller's request to stop a computation, polled by the engine.
	///
	/// \details [[BPR-1366]]. The engine never reads a clock or a thread
	/// primitive: it counts steps (matrix cells, primary-index probe values,
	/// move-detection work, bytes scanned or normalized, weighted validation
	/// records) and asks this predicate every `CheckInterval` of them. A stop
	/// is honoured at the poll that sees it.
	class diff_compute_stop
	{
	public:
		diff_compute_stop()                                            = default;
		diff_compute_stop(const diff_compute_stop&)                    = default;
		diff_compute_stop(diff_compute_stop&&)                         = default;
		auto operator=(const diff_compute_stop&) -> diff_compute_stop& = default;
		auto operator=(diff_compute_stop&&) -> diff_compute_stop&      = default;
		virtual ~diff_compute_stop()                                   = default;

		/// \param aSteps The computation's steps so far, counted as an
		/// uninterrupted run counts them.
		[[nodiscard]] virtual auto stop_requested(std::uint64_t aSteps) -> bool = 0;
	};

	/// \brief How a stoppable computation may be stopped. [[BPR-1366]]
	struct diff_compute_control
	{
		/// Not owned; absent for a computation nothing may stop.
		diff_compute_stop* Stop          = nullptr;
		std::uint64_t      CheckInterval = default_diff_compute_check_interval;
	};

	/// \brief The step accounting of one computation, across its slices.
	struct diff_compute_statistics
	{
		/// Steps of the computation so far, as an uninterrupted run counts
		/// them: work a pause discarded is counted once, when it is redone.
		std::uint64_t Steps = 0U;
		/// Steps executed by every slice, including work a pause discarded.
		std::uint64_t WorkSteps = 0U;
		/// The most steps between two resume points so far: the most a pause
		/// can discard, and enough steps for a slice to pass a resume point.
		std::uint64_t LargestRestartUnit = 0U;
		/// The most steps a poll of the stop predicate has come after it was
		/// due, because one increment of work went past it: a stop is honoured
		/// within `CheckInterval` plus this many steps of its request.
		std::uint64_t      LargestPollDelay = 0U;
		std::uint32_t      Slices           = 0U;
		diff_compute_phase Phase            = diff_compute_phase::prepare;
		/// The step at which each phase began, by `diff_compute_phase`; zero
		/// for a phase not begun. The same in every run of one computation.
		std::array<std::uint64_t, static_cast<std::size_t>(diff_compute_phase::finished)> PhaseStarts{ };
		/// This slice continued the paused state it was given, rather than
		/// refusing it and starting again.
		bool Resumed = false;

		auto operator<=>(const diff_compute_statistics&) const = default;
	};

	struct diff_compute_progress;

	/// \brief A stopped computation's progress: opaque, movable, and resumable
	/// only with the comparison it was taken from. [[BPR-1366]]
	///
	/// \details Keyed by the comparison key (its blob identities, options
	/// digest and algorithm revision), the options, the compute limits and the
	/// bytes of both sides. It carries the matrix and index budget consumption
	/// and the approximate flag, so a resumed computation degrades exactly as
	/// an uninterrupted one would.
	class paused_diff_compute
	{
	public:
		paused_diff_compute() noexcept;
		~paused_diff_compute();
		paused_diff_compute(const paused_diff_compute&)                    = delete;
		auto operator=(const paused_diff_compute&) -> paused_diff_compute& = delete;
		paused_diff_compute(paused_diff_compute&& aOther) noexcept;
		auto operator=(paused_diff_compute&& aOther) noexcept -> paused_diff_compute&;

		[[nodiscard]] auto empty() const noexcept -> bool;
		/// \brief Bytes this state holds, including its copies of both sides.
		[[nodiscard]] auto retained_bytes() const noexcept -> std::uint64_t;
		[[nodiscard]] auto statistics() const noexcept -> diff_compute_statistics;

	private:
		friend struct diff_compute_progress_access;
		std::unique_ptr<diff_compute_progress> Progress;
	};

	/// \brief One slice of a stoppable file comparison. [[BPR-1366]]
	struct file_diff_compute_slice
	{
		/// The finished outcome; absent while the computation is paused.
		std::optional<diff_compute_outcome> Outcome;
		/// The progress to resume from; empty once finished.
		paused_diff_compute     Paused;
		diff_compute_statistics Statistics;
	};

	/// \brief One slice of a stoppable material comparison. [[BPR-1366]]
	struct material_diff_compute_slice
	{
		std::optional<material_diff_outcome> Outcome;
		paused_diff_compute                  Paused;
		diff_compute_statistics              Statistics;
	};

	/// \brief Computes the compute file diff result.
	[[nodiscard]] auto compute_file_diff(const diff_compute_input& aInput)
		-> diff_compute_outcome;
	/// \brief Computes one slice of a file comparison that `aControl` may
	/// stop. [[BPR-1366]]
	///
	/// \details A stop returns a paused slice whose state continues the
	/// computation when it is passed back with the same input. A state taken
	/// from any other comparison, options, limits or bytes is refused and the
	/// computation starts again; either way the finished outcome equals
	/// `compute_file_diff(aInput)`, work counters included.
	[[nodiscard]] auto compute_file_diff(const diff_compute_input& aInput, const diff_compute_control& aControl,
										 paused_diff_compute aResume = { }) -> file_diff_compute_slice;
	/// \brief Computes the validate computed file diff result.
	[[nodiscard]] auto validate_computed_file_diff(const computed_file_diff& aDiff)
		-> diff_compute_validation_result;
	/// \brief Compares two material sides with the shared engine: the same
	/// decoding, alignment, pairing, move and limit rules as
	/// `compute_file_diff`, and no pull-request identity.
	[[nodiscard]] auto compute_material_diff(const material_diff_input& aInput)
		-> material_diff_outcome;
	/// \brief One slice of a material comparison that `aControl` may stop,
	/// with the rules of the file overload. [[BPR-1366]]
	[[nodiscard]] auto compute_material_diff(const material_diff_input& aInput, const diff_compute_control& aControl,
											 paused_diff_compute aResume = { }) -> material_diff_compute_slice;
	/// \brief Validates a material comparison by the rules applied to
	/// `computed_file_diff`.
	[[nodiscard]] auto validate_computed_material_diff(const computed_material_diff& aDiff)
		-> diff_compute_validation_result;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_DIFF_COMPUTE_HPP
