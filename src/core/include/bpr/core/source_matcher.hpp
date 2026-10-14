#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_SOURCE_MATCHER_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_SOURCE_MATCHER_HPP

#include "bpr/core/diff_semantics.hpp"
#include "bpr/core/source_search.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::core {

	/// \brief Hard upper bounds; callers may choose stricter limits only.
	struct source_match_limits
	{
		std::size_t   MaximumBytes       = 16U * 1024U * 1024U;
		std::size_t   MaximumLines       = 200000U;
		std::size_t   MaximumOwnedBytes  = 64U * 1024U * 1024U;
		std::size_t   MaximumResultBytes = 16U * 1024U * 1024U;
		std::uint32_t MaximumMatches     = maximum_source_search_matches;
		/// The regex engine's memory for one compiled query, program and caches.
		std::size_t   MaximumProgramBytes = 8U * 1024U * 1024U;
		std::uint64_t MaximumCompileSteps = 1024U * 1024U;
		std::uint64_t MaximumScanSteps    = 128U * 1024U * 1024U;
	};

	/// \brief One compiled RE2-syntax pattern supplied by the host's regex engine.
	/// \details [[BPR-1431]]. The engine is a mature library (RE2) behind this
	/// interface; core keeps the matching rules. Implementations are immutable
	/// and safe to call from several threads.
	class source_regex
	{
	public:
		source_regex()                                       = default;
		source_regex(const source_regex&)                    = delete;
		auto operator=(const source_regex&) -> source_regex& = delete;
		virtual ~source_regex()                              = default;
		/// \brief The leftmost-first match beginning at or after `aBegin` and
		/// ending at or before `aEnd`, or one beginning exactly at `aBegin` when
		/// anchored. Text outside the range is context for `^`, `$` and `\b` only.
		[[nodiscard]] virtual auto find(std::string_view aText, std::size_t aBegin, std::size_t aEnd, bool aAnchored) const -> std::optional<byte_span> = 0;
	};
	/// \brief How the engine reads a pattern; the pattern's own flags still apply.
	struct source_regex_syntax
	{
		bool        Literal       = false;
		bool        CaseSensitive = true;
		std::size_t MaximumMemory = 8U * 1024U * 1024U;
	};
	/// \brief A compiled pattern or a refusal; offsets index the supplied pattern.
	struct source_regex_compile
	{
		source_search_status                Status      = source_search_status::invalid_pattern;
		std::size_t                         ErrorOffset = 0U;
		std::shared_ptr<const source_regex> Regex;
		std::size_t                         RetainedBytes = 0U;
	};
	/// \brief The host's regex engine, injected so core links no library.
	class source_regex_engine
	{
	public:
		source_regex_engine()                                                                                                    = default;
		source_regex_engine(const source_regex_engine&)                                                                          = delete;
		auto operator=(const source_regex_engine&) -> source_regex_engine&                                                       = delete;
		virtual ~source_regex_engine()                                                                                           = default;
		[[nodiscard]] virtual auto compile(std::string_view aPattern, source_regex_syntax aSyntax) const -> source_regex_compile = 0;
	};

	/// \brief Immutable compiled query, owned by the host and reusable across documents.
	/// \details Literal and regular-expression queries both run on the injected
	/// engine (RE2 syntax, linear time, no backtracking). Lookaround,
	/// backreferences and `\C` are refused explicitly before compilation.
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
	[[nodiscard]] auto compile_source_match(const source_regex_engine& aEngine, std::string_view aQuery, source_search_options aOptions = { }, source_match_limits aLimits = { }, diff_compute_stop* aStop = nullptr) -> source_match_compile;

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
	/// Cancellation is polled before each engine search and within 1024 counted
	/// steps; one engine search is linear in the side's bounded size.
	[[nodiscard]] auto scan_source_match(const source_match_program& aProgram, std::string_view aSource, std::optional<byte_span> aBounds = { }, diff_compute_stop* aStop = nullptr) -> source_match_scan;

	/// \brief Bounded preview, up to 240 Unicode scalars around the occurrence.
	[[nodiscard]] auto source_match_preview(std::string_view aSource, byte_span aSpan) -> std::string;
	/// \brief The occurrence's byte span within `source_match_preview`'s text.
	/// \details Clipped to the preview, so a match running past the preview's
	/// 240 scalars ends at the preview's end. Empty when the preview is empty.
	[[nodiscard]] auto source_match_preview_span(std::string_view aSource, byte_span aSpan) -> byte_span;

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
	[[nodiscard]] auto compile_source_paths(const source_regex_engine& aEngine, std::string_view aInclude, std::string_view aExclude, source_match_limits aLimits = { }, diff_compute_stop* aStop = nullptr) -> source_path_compile;

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
		/// The occurrence's exact bytes within `Preview`, clipped to it.
		byte_span PreviewMatch;
	};
	/// \brief The diff sides whose occurrences a result list shows ([[BPR-1504]]).
	/// \details Two independent sides, both on by default. At least one stays
	/// on: `toggled` refuses to turn off the last one, so no filter hides all.
	struct source_side_filter
	{
		bool Left                                                = true;
		bool Right                                               = true;
		auto operator==(const source_side_filter&) const -> bool = default;
	};
	/// \brief Whether a record has a location on an enabled side; a
	/// both-sides record shows under either single side.
	[[nodiscard]] constexpr auto shown(const source_match_record& aRecord, source_side_filter aFilter) noexcept -> bool
	{
		return (aFilter.Left && aRecord.Locations[0].has_value()) || (aFilter.Right && aRecord.Locations[1].has_value());
	}
	/// \brief The filter with one side flipped, unless that would leave none on.
	[[nodiscard]] constexpr auto toggled(source_side_filter aFilter, diff_side aSide) noexcept -> source_side_filter
	{
		auto next                                           = aFilter;
		(aSide == diff_side::left ? next.Left : next.Right) = !(aSide == diff_side::left ? aFilter.Left : aFilter.Right);
		return next.Left || next.Right ? next : aFilter;
	}
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
