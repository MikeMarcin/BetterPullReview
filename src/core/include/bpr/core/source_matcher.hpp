#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_SOURCE_MATCHER_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_SOURCE_MATCHER_HPP

#include "bpr/core/diff_semantics.hpp"
#include "bpr/core/source_search.hpp"

#include <memory>
#include <string>

namespace bpr::core {

	/// \brief Hard upper bounds; callers may choose stricter limits only.
	struct source_match_limits
	{
		std::size_t   MaximumBytes        = 16U * 1024U * 1024U;
		std::size_t   MaximumLines        = 200000U;
		std::size_t   MaximumInstructions = 8192U;
		std::size_t   MaximumOwnedBytes   = 64U * 1024U * 1024U;
		std::size_t   MaximumResultBytes  = 16U * 1024U * 1024U;
		std::uint32_t MaximumMatches      = maximum_source_search_matches;
		std::uint64_t MaximumCompileSteps = 1024U * 1024U;
		std::uint64_t MaximumScanSteps    = 128U * 1024U * 1024U;
	};

	/// \brief Immutable compiled query, owned by the host and reusable across documents.
	/// \details Domain-only Thompson VM, never recursive backtracking. Supports
	/// groups, alternation, classes, greedy/lazy repetitions (counts <=1000),
	/// i/m/s/U flags, anchors, ASCII Perl classes and Unicode general categories.
	/// Lookaround, backreferences and unsupported escapes are refused explicitly.
	class source_match_program
	{
	public:
		struct implementation;
		[[nodiscard]] auto retained_bytes() const noexcept -> std::size_t;

	private:
		explicit source_match_program(std::shared_ptr<const implementation> aProgram);
		friend struct source_match_access;
		std::shared_ptr<const implementation> Program;
	};

	/// \brief Pattern diagnostics contain offsets and codes, never query text.
	struct source_match_compile
	{
		source_search_status                Status      = source_search_status::invalid_pattern;
		std::size_t                         ErrorOffset = 0U;
		std::optional<source_match_program> Program;
		std::uint64_t                       Steps = 0U;
	};

	/// \brief Compile once before any source reads; empty input compiles no work.
	[[nodiscard]] auto compile_source_match(std::string_view aQuery, source_search_options aOptions = { }, source_match_limits aLimits = { }, diff_compute_stop* aStop = nullptr) -> source_match_compile;

	/// \brief Exact original UTF-8 byte spans in nonoverlapping source order.
	struct source_match_scan
	{
		source_search_status   Status = source_search_status::complete;
		std::vector<byte_span> Matches;
		bool                   AdditionalMatchObserved = false;
		std::uint64_t          Steps                   = 0U;
		std::size_t            PeakOwnedBytes          = 0U;
	};

	/// \brief Scan one immutable UTF-8 side in a logical LF view.
	/// \details Optional bounds are fixed original byte boundaries. Anchors and
	/// Whole Word inspect the actual document, not invented selection edges.
	/// Cancellation is polled initially and within 1024 counted steps.
	[[nodiscard]] auto scan_source_match(const source_match_program& aProgram, std::string_view aSource, std::optional<byte_span> aBounds = { }, diff_compute_stop* aStop = nullptr) -> source_match_scan;

	/// \brief Bounded preview, up to 240 Unicode scalars around the occurrence.
	[[nodiscard]] auto source_match_preview(std::string_view aSource, byte_span aSpan) -> std::string;

	/// \brief Path decision with an explicit invalid/resource terminal for coverage.
	struct source_path_decision
	{
		source_search_status Status   = source_search_status::complete;
		bool                 Included = false;
	};
	/// \brief Compiled repository-relative, case-sensitive include/exclude policy.
	class source_path_filter
	{
	public:
		struct implementation;
		explicit source_path_filter(std::shared_ptr<const implementation> aFilter);
		/// \brief Evaluate both exact paths, preserving any resource or input refusal.
		[[nodiscard]] auto evaluate(std::optional<std::string_view> aLeft, std::optional<std::string_view> aRight) const -> source_path_decision;
		/// \brief Include either exact path, but any excluded side excludes the entry.
		[[nodiscard]] auto includes(std::optional<std::string_view> aLeft, std::optional<std::string_view> aRight) const -> bool;

	private:
		std::shared_ptr<const implementation> Filter;
	};
	/// \brief Syntax admission and a bounded immutable path filter.
	struct source_path_compile
	{
		source_search_status              Status      = source_search_status::invalid_pattern;
		std::size_t                       ErrorOffset = 0U;
		std::optional<source_path_filter> Filter;
	};
	/// \brief Parse commas, braces, classes and escaping before material acquisition.
	[[nodiscard]] auto compile_source_paths(std::string_view aInclude, std::string_view aExclude, source_match_limits aLimits = { }, diff_compute_stop* aStop = nullptr) -> source_path_compile;

	/// \brief Exact material supplied by its host owner, never a filesystem path.
	struct source_match_side
	{
		git_blob_oid     Blob;
		std::string_view Bytes;
	};
	/// \brief Original bytes and zero-based Unicode scalar line/column on an exact blob.
	struct source_match_location
	{
		diff_side     Side = diff_side::left;
		git_blob_oid  Blob;
		byte_span     Span;
		std::uint32_t Line                                                   = 0U;
		std::uint32_t Column                                                 = 0U;
		auto          operator==(const source_match_location&) const -> bool = default;
	};
	/// \brief One occurrence with at most two proven locations and a bounded preview.
	struct source_match_record
	{
		std::array<std::optional<source_match_location>, 2> Locations;
		std::uint32_t                                       UnifiedRow = 0U;
		std::string                                         Preview;
	};
	/// \brief A single catalogue entry; identity-based merging never crosses entries.
	struct source_match_entry
	{
		file_identity                    File;
		std::optional<source_match_side> Left;
		std::optional<source_match_side> Right;
		source_search_target             Target = source_search_target::files;
		/// Validated selected diff for unified row order; no semantic diff is run.
		const diff_semantic_index*                     Unified = nullptr;
		std::optional<std::pair<diff_side, byte_span>> Selection;
	};
	/// \brief Ordered usable prefix plus honest coverage/resource terminal facts.
	struct source_match_entry_result
	{
		file_identity                    File;
		source_search_status             Status = source_search_status::complete;
		std::vector<source_match_record> Matches;
		bool                             AdditionalMatchObserved = false;
		std::uint64_t                    Steps                   = 0U;
		std::uint32_t                    ScannedSides            = 0U;
		std::size_t                      PeakOwnedBytes          = 0U;
	};
	/// \brief Shared pane/PR composition: actual versions, exact spans and bounded results.
	/// \details Unified scans actual documents, orders by the existing equal/group
	/// row plan and merges only exact unchanged occurrences. PR merging requires
	/// identical blob identity and bytes in this same entry; different blobs stay separate.
	[[nodiscard]] auto match_source_entry(const source_match_program& aProgram, const source_match_entry& aEntry, diff_compute_stop* aStop = nullptr) -> source_match_entry_result;
}
#endif
