#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_SOURCE_SEARCH_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_SOURCE_SEARCH_HPP

#include "bpr/core/diff_compute.hpp"
#include "bpr/core/document_tabs.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace bpr::core {

	inline constexpr std::size_t   maximum_source_search_query_bytes = 4096U;
	inline constexpr std::uint32_t maximum_source_search_matches     = 10000U;

	enum class source_search_target : std::uint8_t { left,
													 right,
													 unified,
													 files };

	/// \brief Exact immutable source identity for a pane; no text or paths are retained.
	struct source_search_document
	{
		file_identity               File;
		file_comparison_digest      Comparison;
		std::optional<git_blob_oid> LeftBlob;
		std::optional<git_blob_oid> RightBlob;
		auto                        operator==(const source_search_document&) const -> bool = default;
	};

	/// \brief Fixed original source bounds; navigation never moves this range.
	struct source_search_range
	{
		diff_side Side = diff_side::right;
		byte_span Span;
		auto      operator==(const source_search_range&) const -> bool = default;
	};
	/// \brief Binds one search to the active tab's exact pair and corpus generation.
	/// \details CatalogueGeneration identifies the host's catalogue for this pair,
	/// not the latest remote epoch. A refresh which leaves this scope valid is a no-op.
	struct source_search_scope
	{
		application_session_id                Session;
		pull_request_authority                Authority;
		comparison_pair                       Comparison;
		std::uint64_t                         CatalogueGeneration = 0U;
		std::optional<document_tab_id>        Tab;
		std::optional<source_search_range>    Selection;
		source_search_target                  Target = source_search_target::right;
		std::optional<source_search_document> Document;
		auto                                  operator==(const source_search_scope&) const -> bool = default;
	};

	struct source_search_options
	{
		bool MatchCase                                              = false;
		bool WholeWord                                              = false;
		bool RegularExpression                                      = false;
		auto operator==(const source_search_options&) const -> bool = default;
	};

	enum class source_search_input : std::uint8_t { empty,
													ready,
													input_limit,
													invalid_utf8 };
	enum class source_search_status : std::uint8_t {
		idle,
		searching,
		complete,
		partial,
		cancelled,
		result_limit,
		resource_limit,
		unavailable,
		failure,
		invalid_input,
		invalid_pattern,
		unsupported_pattern,
		identity_exhausted
	};
	enum class source_search_reason : std::uint8_t {
		none,
		unavailable_scope,
		invalid_scope,
		stale_completion,
		invalid_completion,
		no_matches,
		identity_exhausted
	};

	/// \brief Complete constant-size identity of one admitted query generation.
	struct source_search_request
	{
		source_search_scope   Scope;
		std::uint64_t         Generation = 0U;
		source_search_options Options;
		auto                  operator==(const source_search_request&) const -> bool = default;
	};

	/// \brief Bounded coverage facts; result collections remain with the host.
	struct source_search_summary
	{
		std::uint32_t                Matches                 = 0U;
		std::uint32_t                EligibleFiles           = 0U;
		std::uint32_t                SearchedFiles           = 0U;
		std::uint32_t                SkippedSides            = 0U;
		bool                         AdditionalMatchObserved = false;
		std::optional<std::uint32_t> InitialMatch;
		auto                         operator==(const source_search_summary&) const -> bool = default;
	};

	/// \brief An effect's one terminal, including cancellation and malformed input.
	struct source_search_terminal
	{
		source_search_request Request;
		source_search_status  Status = source_search_status::failure;
		source_search_summary Summary;
	};

	/// \brief Authority to which retained query text belongs, even during unavailability.
	struct source_search_query_owner
	{
		application_session_id Session;
		pull_request_authority Authority;
	};

	/// \brief Compact deterministic interaction state; no dynamic storage or effects.
	struct source_search_state
	{
		std::optional<source_search_scope>       Scope;
		std::optional<source_search_query_owner> QueryOwner;
		std::optional<source_search_request>     Pending;
		std::uint64_t                            Generation = 0U;
		source_search_options                    Options;
		source_search_input                      Input  = source_search_input::empty;
		source_search_status                     Status = source_search_status::idle;
		source_search_summary                    Summary;
		std::optional<std::uint32_t>             CurrentMatch;
		bool                                     Open = false;
	};

	/// \brief Requests work/cancellation and reports accepted terminals or cursor wrap.
	struct source_search_output
	{
		bool                                 Changed    = false;
		bool                                 Accepted   = false;
		bool                                 ClearQuery = false;
		bool                                 Wrapped    = false;
		std::optional<source_search_request> Work;
		std::optional<source_search_request> Cancel;
		source_search_reason                 Reason = source_search_reason::none;
	};

	/// \brief Admits the query byte/UTF-8 boundary without retaining its text.
	[[nodiscard]] auto inspect_source_search_query(std::string_view aQuery) noexcept -> source_search_input;
	/// \brief Validates exact scope and pane-side availability before work.
	[[nodiscard]] auto valid(const source_search_scope& aScope) noexcept -> bool;
	/// \brief Changes scope, invalidates obsolete hits and preserves same-authority input.
	[[nodiscard]] auto set_source_search_scope(source_search_state& aState, const std::optional<source_search_scope>& aScope) -> source_search_output;
	/// \brief Opens and edits a query, invalidating old hits even for invalid input.
	[[nodiscard]] auto edit_source_search_query(source_search_state& aState, source_search_input aInput, source_search_options aOptions) -> source_search_output;
	/// \brief Reopens the retained query and options; an already open session is unchanged.
	[[nodiscard]] auto open_source_search(source_search_state& aState) -> source_search_output;
	/// \brief Cancels unfinished work and clears hits while retaining query/options.
	[[nodiscard]] auto close_source_search(source_search_state& aState) -> source_search_output;
	/// \brief Revokes authority, query and results without reusing a generation.
	[[nodiscard]] auto reset_source_search(source_search_state& aState) -> source_search_output;
	/// \brief Accepts only the exact current request, once; incomplete empty stays qualified.
	[[nodiscard]] auto complete_source_search(source_search_state& aState, const source_search_terminal& aTerminal) -> source_search_output;
	/// \brief Accepts monotonic prefix coverage only for the pending exact request.
	[[nodiscard]] auto progress_source_search(source_search_state& aState, const source_search_request& aRequest, source_search_summary aSummary) -> source_search_output;
	/// \brief Traverses accepted counts in constant work, without matching or source access.
	[[nodiscard]] auto navigate_source_search(source_search_state& aState, change_navigation_direction aDirection) -> source_search_output;
	/// \brief True only for an unqualified, completely searched, empty corpus.
	[[nodiscard]] auto source_search_no_results(const source_search_state& aState) noexcept -> bool;
}

#endif
