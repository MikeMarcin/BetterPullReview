#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_DIFF_MODEL_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_DIFF_MODEL_HPP

#include "bpr/core/epoch_model.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace bpr::core {
	struct git_blob_oid_kind;
	using git_blob_oid = git_object_oid<git_blob_oid_kind>;

	struct file_identity_tag;
	struct diff_options_digest_tag;
	struct file_comparison_digest_tag;
	struct alignment_segment_id_tag;
	struct line_pair_id_tag;
	struct change_group_id_tag;
	struct move_relation_id_tag;
	struct diff_algorithm_revision_tag;

	using file_identity           = fixed_identity_256<file_identity_tag>;
	using diff_options_digest     = fixed_identity_256<diff_options_digest_tag>;
	using file_comparison_digest  = fixed_identity_256<file_comparison_digest_tag>;
	using alignment_segment_id    = fixed_identity_256<alignment_segment_id_tag>;
	using line_pair_id            = fixed_identity_256<line_pair_id_tag>;
	using change_group_id         = fixed_identity_256<change_group_id_tag>;
	using move_relation_id        = fixed_identity_256<move_relation_id_tag>;
	using diff_algorithm_revision = strong_id<diff_algorithm_revision_tag>;

	enum class whitespace_policy : std::uint8_t {
		exact,
		ignore_trailing,
		ignore_all,
	};

	enum class line_ending_policy : std::uint8_t {
		exact,
		normalize,
	};

	enum class pairing_policy : std::uint8_t {
		disabled,
		deterministic_similarity,
	};

	enum class move_detection_policy : std::uint8_t {
		disabled,
		deterministic_exact,
	};

	inline constexpr std::uint32_t maximum_diff_context_lines            = 10'000U;
	inline constexpr std::uint16_t maximum_similarity_threshold_permille = 1'000U;

	/// \brief Describes the diff options record.
	struct diff_options
	{
		whitespace_policy     Whitespace                  = whitespace_policy::exact;
		line_ending_policy    LineEndings                 = line_ending_policy::exact;
		std::uint32_t         ContextLines                = 3U;
		pairing_policy        Pairing                     = pairing_policy::deterministic_similarity;
		std::uint16_t         SimilarityThresholdPermille = 600U;
		move_detection_policy Moves                       = move_detection_policy::disabled;

		auto operator<=>(const diff_options&) const = default;
	};

	/// \brief Computes the normalize diff options result.
	[[nodiscard]] auto normalize_diff_options(diff_options aOptions) noexcept
		-> std::optional<diff_options>;
	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(const diff_options& aOptions) noexcept -> bool;
	/// \brief Computes the options digest result.
	[[nodiscard]] auto options_digest(const diff_options& aOptions) noexcept
		-> std::optional<diff_options_digest>;

	/// \brief Describes the file comparison key record.
	struct file_comparison_key
	{
		pull_request_authority      Authority;
		repository_id               Repository;
		pull_request_id             PullRequest;
		comparison_anchor_identity  LeftAnchor;
		comparison_anchor_identity  RightAnchor;
		std::optional<git_blob_oid> LeftBlob;
		std::optional<git_blob_oid> RightBlob;
		file_identity               File;
		diff_options_digest         Options;
		diff_algorithm_revision     Algorithm;

		auto operator<=>(const file_comparison_key&) const = default;
	};

	enum class diff_side : std::uint8_t {
		left,
		right,
	};

	/// \brief Describes the line span record.
	struct line_span
	{
		std::uint32_t Begin = 0;
		std::uint32_t End   = 0;

		[[nodiscard]] constexpr auto empty() const noexcept -> bool
		{
			return Begin == End;
		}

		[[nodiscard]] constexpr auto extent() const noexcept -> std::uint64_t
		{
			return Begin <= End
				? static_cast<std::uint64_t>(End)
					- static_cast<std::uint64_t>(Begin)
				: 0U;
		}

		auto operator<=>(const line_span&) const = default;
	};

	/// \brief Describes the source line ref record.
	struct source_line_ref
	{
		file_comparison_digest Comparison;
		diff_side              Side = diff_side::left;
		std::uint32_t          Line = 0;

		auto operator<=>(const source_line_ref&) const = default;
	};

	/// \brief Describes the source boundary ref record.
	struct source_boundary_ref
	{
		file_comparison_digest Comparison;
		diff_side              Side     = diff_side::left;
		std::uint32_t          Boundary = 0;

		auto operator<=>(const source_boundary_ref&) const = default;
	};

	enum class alignment_kind : std::uint8_t {
		equal,
		replacement,
		left_only,
		right_only,
	};

	/// \brief Describes the alignment segment record.
	struct alignment_segment
	{
		alignment_segment_id           Id;
		alignment_kind                 Kind = alignment_kind::equal;
		line_span                      Left;
		line_span                      Right;
		std::optional<change_group_id> Group;

		auto operator<=>(const alignment_segment&) const = default;
	};

	enum class line_pair_kind : std::uint8_t {
		exact,
		similar,
	};

	/// \brief Describes the line pair record.
	struct line_pair
	{
		line_pair_id         Id;
		alignment_segment_id Segment;
		std::uint32_t        LeftLine  = 0;
		std::uint32_t        RightLine = 0;
		line_pair_kind       Kind      = line_pair_kind::similar;

		auto operator<=>(const line_pair&) const = default;
	};

	/// \brief Describes the segment index span record.
	struct segment_index_span
	{
		std::uint32_t Begin = 0;
		std::uint32_t End   = 0;

		[[nodiscard]] constexpr auto empty() const noexcept -> bool
		{
			return Begin == End;
		}

		auto operator<=>(const segment_index_span&) const = default;
	};

	/// \brief Describes the alignment boundary record.
	struct alignment_boundary
	{
		std::uint32_t Left  = 0;
		std::uint32_t Right = 0;

		auto operator<=>(const alignment_boundary&) const = default;
	};

	enum class change_summary_kind : std::uint8_t {
		addition,
		deletion,
		replacement,
		mixed,
	};

	/// \brief Describes the change group record.
	struct change_group
	{
		change_group_id     Id;
		segment_index_span  Members;
		line_span           Left;
		line_span           Right;
		alignment_boundary  First;
		alignment_boundary  Last;
		change_summary_kind Kind               = change_summary_kind::mixed;
		std::uint64_t       AddedLines         = 0;
		std::uint64_t       DeletedLines       = 0;
		std::uint64_t       ReplacedLeftLines  = 0;
		std::uint64_t       ReplacedRightLines = 0;

		auto operator<=>(const change_group&) const = default;
	};

	/// \brief Describes the move relation record.
	struct move_relation
	{
		move_relation_id     Id;
		alignment_segment_id SourceDeletion;
		alignment_segment_id DestinationInsertion;

		auto operator<=>(const move_relation&) const = default;
	};

	enum class diff_content_state : std::uint8_t {
		text,
		binary,
		unavailable,
	};

	enum class diff_reason_code : std::uint8_t {
		none,
		binary_file,
		missing_left_blob,
		missing_right_blob,
		decode_failure,
		materialization_limit,
		compute_limit,
		generated_file_unsupported,
		cancelled,
		stale,
		invalid_alignment,
		host_failure,
	};

	/// \brief Describes the diff diagnostic record.
	struct diff_diagnostic
	{
		diff_reason_code Code = diff_reason_code::none;

		auto operator<=>(const diff_diagnostic&) const = default;
	};

	/// \brief Describes the file alignment record.
	struct file_alignment
	{
		file_comparison_key            Key;
		diff_options                   Options;
		file_comparison_digest         Digest;
		std::uint32_t                  LeftLineCount  = 0;
		std::uint32_t                  RightLineCount = 0;
		diff_content_state             Content        = diff_content_state::text;
		std::vector<alignment_segment> Segments;
		std::vector<line_pair>         Pairs;
		std::vector<change_group>      Groups;
		std::vector<move_relation>     Moves;
		diff_diagnostic                Diagnostic;

		auto operator<=>(const file_alignment&) const = default;
	};

	enum class alignment_validation_error : std::uint8_t {
		none,
		count_overflow,
		invalid_comparison_key,
		options_digest_mismatch,
		comparison_digest_mismatch,
		invalid_content_diagnostic,
		records_for_non_text,
		records_without_segments,
		records_disabled_by_options,
		missing_text_segments,
		invalid_segment_id,
		duplicate_segment_id,
		invalid_segment_span,
		left_coverage_discontinuity,
		right_coverage_discontinuity,
		empty_segment,
		segment_kind_mismatch,
		segment_group_mismatch,
		noncanonical_adjacent_segment,
		incomplete_left_coverage,
		incomplete_right_coverage,
		invalid_group_id,
		duplicate_group_id,
		invalid_group_span,
		empty_group,
		unordered_group,
		noncanonical_adjacent_group,
		noncontiguous_group,
		group_span_mismatch,
		group_boundary_mismatch,
		group_summary_mismatch,
		ungrouped_change,
		grouped_equal_segment,
		invalid_pair_id,
		duplicate_pair_id,
		invalid_pair_kind,
		unknown_pair_segment,
		pair_out_of_bounds,
		nonmonotonic_pair,
		invalid_move_id,
		duplicate_move_id,
		unknown_move_segment,
		invalid_move_topology,
		duplicate_move_relation,
		noncanonical_move_order,
	};

	/// \brief Describes the alignment validation result record.
	struct alignment_validation_result
	{
		static constexpr auto no_record = std::numeric_limits<std::size_t>::max();

		alignment_validation_error Error   = alignment_validation_error::none;
		std::size_t                Record  = no_record;
		std::size_t                Related = no_record;

		[[nodiscard]] constexpr auto valid() const noexcept -> bool
		{
			return Error == alignment_validation_error::none;
		}

		auto operator<=>(const alignment_validation_result&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(const file_comparison_key& aKey) noexcept -> bool;
	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const line_span aSpan, const std::uint32_t aLineCount) noexcept -> bool
	{
		return aSpan.Begin <= aSpan.End && aSpan.End <= aLineCount;
	}
	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(const source_line_ref& aLine,
							 std::uint32_t          aLeftLineCount,
							 std::uint32_t          aRightLineCount) noexcept -> bool;
	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(const source_boundary_ref& aBoundary,
							 std::uint32_t              aLeftLineCount,
							 std::uint32_t              aRightLineCount) noexcept -> bool;

	/// \brief Computes the comparison digest result.
	[[nodiscard]] auto comparison_digest(
		const file_comparison_key& aKey) noexcept -> file_comparison_digest;
	/// \brief Computes the segment identity result.
	[[nodiscard]] auto segment_identity(file_comparison_digest aComparison,
										alignment_kind         aKind,
										line_span              aLeft,
										line_span              aRight,
										std::uint32_t          aOrdinal) noexcept -> alignment_segment_id;
	/// \brief Computes the pair identity result.
	[[nodiscard]] auto pair_identity(file_comparison_digest aComparison,
									 alignment_segment_id   aSegment,
									 std::uint32_t          aLeftLine,
									 std::uint32_t          aRightLine,
									 line_pair_kind         aKind,
									 std::uint32_t          aOrdinal) noexcept -> line_pair_id;
	/// \brief Computes the group identity result.
	[[nodiscard]] auto group_identity(file_comparison_digest aComparison,
									  line_span              aLeft,
									  line_span              aRight,
									  std::uint32_t          aOrdinal) noexcept -> change_group_id;
	/// \brief Computes the move identity result.
	[[nodiscard]] auto move_identity(file_comparison_digest aComparison,
									 alignment_segment_id   aSourceDeletion,
									 alignment_segment_id   aDestinationInsertion,
									 std::uint32_t          aOrdinal) noexcept -> move_relation_id;

	/// \brief Computes the validate alignment result.
	[[nodiscard]] auto validate_alignment(const file_alignment& aAlignment)
		-> alignment_validation_result;
}

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_DIFF_MODEL_HPP
