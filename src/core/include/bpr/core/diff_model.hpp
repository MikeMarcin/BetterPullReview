#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_DIFF_MODEL_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_DIFF_MODEL_HPP

#include "bpr/core/epoch_model.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

namespace bpr::core {
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

	/// \brief The longest repository path this build will name.
	///
	/// \details Matches the read model's own path bound. A path beyond it is
	/// refused rather than truncated, because two paths sharing a prefix would
	/// otherwise collapse to one identity and a reviewer's receipt for one file
	/// would silently answer for another.
	inline constexpr std::size_t maximum_identified_path_bytes = 4U * 1024U;

	/// \brief Derives one file's stable identity from its repository path.
	///
	/// \details `file_identity` names the *file*; `file_material_id` names its
	/// *content*. A receipt pairs them, so changing a file's bytes makes a
	/// receipt stale without making it meaningless. This is the single
	/// derivation of the former, and it must stay single: RFC-BPR-0007 keys
	/// receipts by this identity, so a second derivation anywhere would make a
	/// reviewer's receipts quietly stop matching their files.
	///
	/// It is a pure function of one path, and that is the decision rather than
	/// an accident of implementation. **A rename therefore produces a new
	/// identity**, and a reviewer who had marked the file read sees it unread
	/// again even though no byte changed.
	///
	/// The alternative was to carry identity across a rename by consuming the
	/// host's reported previous path. That was refused: identity would then
	/// depend on the review snapshot rather than on the file, two callers
	/// holding different snapshots could derive different identities for the
	/// same file, and the receipt key would stop being well defined. Re-reading
	/// a moved file is a small cost paid visibly; receipts that silently stop
	/// matching are not.
	///
	/// \returns An invalid identity for an empty path or one beyond
	/// `maximum_identified_path_bytes`. Callers check `valid()`.
	[[nodiscard]] auto file_identity_of(std::string_view aPath) noexcept
		-> file_identity;

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

	/// \brief RFC-BPR-0089. The two commits a commit-filtered diff is between.
	///
	/// \details Both are commits of the pull request's first-parent chain (or
	/// the commit the chain grew from): the diff is the path at `Left` against
	/// the path at `Right`, a tree some commit contains on each side. The
	/// comparison's anchors stay the window's comparison; these name what the
	/// file's sides actually are, so nothing reads a filtered side as the
	/// comparison's own.
	struct filtered_diff_endpoints
	{
		git_commit_oid Left;
		git_commit_oid Right;

		auto operator<=>(const filtered_diff_endpoints&) const = default;
	};

	/// \brief Checks both commits are valid, distinct and of one format.
	[[nodiscard]] constexpr auto valid(const filtered_diff_endpoints& aEndpoints) noexcept -> bool
	{
		return aEndpoints.Left.valid() && aEndpoints.Right.valid() && aEndpoints.Left != aEndpoints.Right
			&& aEndpoints.Left.algorithm() == aEndpoints.Right.algorithm();
	}

	/// \brief RFC-BPR-0089. The ledger file a commit-filtered diff's own
	/// receipt is kept under.
	///
	/// \details A file whose filtered diff differs from the comparison's has
	/// its own receipt for that diff; the file's receipt for the comparison is
	/// never carried to it. The key is the file and the two commits, which name
	/// the diff's blobs exactly, so it is known before either blob is read and
	/// every filter giving the same two commits shares it. It is a receipt key,
	/// not a second identity for the file: the workbench, tabs and threads keep
	/// naming the file by `file_identity_of`. Invalid for an invalid file or
	/// endpoints.
	[[nodiscard]] auto filtered_receipt_file(const file_identity& aFile, const filtered_diff_endpoints& aEndpoints) noexcept
		-> file_identity;

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
		/// RFC-BPR-0089. Present for a commit-filtered diff: its blobs are the
		/// path at these two commits, not at the anchors. It enters the
		/// comparison digest, and only when present, so every unfiltered key
		/// keeps its digest; it does not enter the material, which is the blobs.
		std::optional<filtered_diff_endpoints> Filter;

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
	/// \details The digest is persisted beside stored line coordinates, so it
	/// names what is compared and not how: the key's algorithm revision does
	/// not enter it ([[BPR-1497]]). Zero for an invalid key.
	[[nodiscard]] auto comparison_digest(
		const file_comparison_key& aKey) noexcept -> file_comparison_digest;
	/// \brief [[BPR-1331]]. The material a file receipt attaches to.
	///
	/// \details RFC-BPR-0007: a new epoch keeps a receipt exactly when the
	/// compared material is unchanged. The comparison digest cannot answer
	/// that, because its two anchors name epochs and every push advances
	/// them: a receipt keyed by it reopened every file on every push,
	/// changed or not. The material is the comparison digest without the
	/// anchors: the pull request, the file, the two blobs and the options
	/// that compared them; like the comparison digest it leaves out the
	/// algorithm revision ([[BPR-1497]]). A push keeps it for a file whose
	/// blobs did not move and moves it for one whose did. Zero for an
	/// invalid key, like `comparison_digest`.
	[[nodiscard]] auto file_material(
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

	/// \brief Identity of a comparison that is not between pull-request
	/// epochs, such as two local cycle revisions.
	/// \details The owner derives `Comparison` under its own domain separation
	/// and never from a pull-request identity. The engine needs only that
	/// identity, which sides exist, and the options and algorithm, and applies
	/// exactly the same alignment rules as to a pull-request comparison.
	struct material_comparison_key
	{
		file_comparison_digest  Comparison;
		bool                    LeftPresent  = false;
		bool                    RightPresent = false;
		diff_options_digest     Options;
		diff_algorithm_revision Algorithm;

		auto operator<=>(const material_comparison_key&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(const material_comparison_key& aKey) noexcept -> bool;
	/// \brief The comparison identity of a valid material key, or zero.
	[[nodiscard]] auto comparison_digest(const material_comparison_key& aKey) noexcept
		-> file_comparison_digest;

	/// \brief The shared alignment records for a material comparison.
	struct material_alignment
	{
		material_comparison_key        Key;
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

		auto operator<=>(const material_alignment&) const = default;
	};

	/// \brief Validates a material alignment by the same rules as a
	/// pull-request alignment.
	[[nodiscard]] auto validate_alignment(const material_alignment& aAlignment)
		-> alignment_validation_result;
}

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_DIFF_MODEL_HPP
