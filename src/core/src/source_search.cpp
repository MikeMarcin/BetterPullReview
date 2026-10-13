#include "bpr/core/source_search.hpp"

#include "bpr/core/epoch_transition.hpp"
#include "bpr/core/review_preparation.hpp"

#include <limits>
#include <type_traits>

namespace bpr::core {

	static_assert(std::is_trivially_copyable_v<source_search_state>);
	static_assert(sizeof(source_search_state) <= 2048U);

	namespace {
		auto retire(source_search_state& aState) -> source_search_output
		{
			auto output    = source_search_output{ };
			output.Changed = true;
			output.Cancel  = aState.Pending;
			aState.Pending.reset();
			aState.Summary = { };
			aState.CurrentMatch.reset();
			aState.Status = source_search_status::idle;
			if (aState.Generation == std::numeric_limits<std::uint64_t>::max()) {
				aState.Status = source_search_status::identity_exhausted;
				output.Reason = source_search_reason::identity_exhausted;
			} else {
				++aState.Generation;
			}
			return output;
		}

		void request_work(source_search_state& aState, source_search_output& aOutput)
		{
			if (aOutput.Reason == source_search_reason::identity_exhausted || !aState.Open) {
				return;
			}
			if (aState.Input != source_search_input::ready) {
				aState.Status = aState.Input == source_search_input::empty ? source_search_status::idle : source_search_status::invalid_input;
				return;
			}
			if (!aState.Scope) {
				aState.Status  = source_search_status::unavailable;
				aOutput.Reason = source_search_reason::unavailable_scope;
				return;
			}
			aState.Pending = source_search_request{ *aState.Scope, aState.Generation, aState.Options };
			aState.Status  = source_search_status::searching;
			aOutput.Work   = aState.Pending;
		}

		auto valid_terminal(const source_search_terminal& aTerminal) noexcept -> bool
		{
			const auto& summary = aTerminal.Summary;
			if (summary.Matches > maximum_source_search_matches || summary.SearchedFiles > summary.EligibleFiles
				|| (summary.Matches != 0U && summary.SearchedFiles == 0U)
				|| static_cast<std::uint64_t>(summary.SkippedSides) > static_cast<std::uint64_t>(summary.EligibleFiles) * 2U
				|| (summary.InitialMatch && *summary.InitialMatch >= summary.Matches)
				|| (summary.AdditionalMatchObserved && (summary.Matches != maximum_source_search_matches || aTerminal.Status != source_search_status::result_limit))) {
				return false;
			}
			switch (aTerminal.Status) {
			case source_search_status::complete:
			case source_search_status::partial:
			case source_search_status::cancelled:
			case source_search_status::resource_limit:
			case source_search_status::unavailable:
			case source_search_status::failure:
				return true;
			case source_search_status::result_limit:
				return summary.Matches == maximum_source_search_matches;
			case source_search_status::invalid_pattern:
			case source_search_status::unsupported_pattern:
				return summary.Matches == 0U;
			default:
				return false;
			}
		}
	}

	auto inspect_source_search_query(const std::string_view aQuery) noexcept -> source_search_input
	{
		if (aQuery.size() > maximum_source_search_query_bytes) {
			return source_search_input::input_limit;
		}
		if (!valid_writing_utf8(aQuery)) {
			return source_search_input::invalid_utf8;
		}
		return aQuery.empty() ? source_search_input::empty : source_search_input::ready;
	}

	auto valid(const source_search_scope& aScope) noexcept -> bool
	{
		if (aScope.Session.value() == 0U || !valid(aScope.Authority) || !valid_exact_pair(aScope.Comparison)
			|| aScope.Comparison.Left.Identity.Authority != aScope.Authority.Scope || aScope.CatalogueGeneration == 0U
			|| (aScope.Tab && aScope.Tab->value() == 0U)) {
			return false;
		}
		if (aScope.Target == source_search_target::files) {
			return !aScope.Document && !aScope.Selection;
		}
		if (!aScope.Document || !aScope.Document->File.valid() || !aScope.Document->Comparison.valid()
			|| (aScope.Document->LeftBlob && !aScope.Document->LeftBlob->valid())
			|| (aScope.Document->RightBlob && !aScope.Document->RightBlob->valid())
			|| (aScope.Document->LeftBlob && aScope.Document->LeftBlob->algorithm() != aScope.Comparison.Left.Commit.algorithm())
			|| (aScope.Document->RightBlob && aScope.Document->RightBlob->algorithm() != aScope.Comparison.Right.Commit.algorithm())) {
			return false;
		}
		if (aScope.Selection && (aScope.Selection->Span.Begin >= aScope.Selection->Span.End || (aScope.Selection->Side != diff_side::left && aScope.Selection->Side != diff_side::right) || (aScope.Selection->Side == diff_side::left && (!aScope.Document->LeftBlob || aScope.Target == source_search_target::right)) || (aScope.Selection->Side == diff_side::right && (!aScope.Document->RightBlob || aScope.Target == source_search_target::left)))) {
			return false;
		}
		switch (aScope.Target) {
		case source_search_target::left:    return aScope.Document->LeftBlob.has_value();
		case source_search_target::right:   return aScope.Document->RightBlob.has_value();
		case source_search_target::unified: return aScope.Document->LeftBlob || aScope.Document->RightBlob;
		default:                            return false;
		}
	}

	auto set_source_search_scope(source_search_state& aState, const std::optional<source_search_scope>& aScope) -> source_search_output
	{
		if (aState.Scope == aScope && (!aScope || valid(*aScope))) {
			return { };
		}
		auto output = retire(aState);
		if (aState.QueryOwner && aScope && (aState.QueryOwner->Authority != aScope->Authority || aState.QueryOwner->Session != aScope->Session)) {
			aState.Input      = source_search_input::empty;
			aState.Options    = { };
			output.ClearQuery = true;
		}
		aState.Scope = aScope && valid(*aScope) ? aScope : std::nullopt;
		if (aState.Scope) {
			aState.QueryOwner = source_search_query_owner{ aState.Scope->Session, aState.Scope->Authority };
		}
		request_work(aState, output);
		if (aScope && !aState.Scope && output.Reason != source_search_reason::identity_exhausted) {
			output.Reason = source_search_reason::invalid_scope;
		}
		return output;
	}

	auto edit_source_search_query(source_search_state& aState, const source_search_input aInput, const source_search_options aOptions) -> source_search_output
	{
		auto output    = retire(aState);
		aState.Input   = aInput;
		aState.Options = aOptions;
		aState.Open    = true;
		request_work(aState, output);
		return output;
	}

	auto open_source_search(source_search_state& aState) -> source_search_output
	{
		if (aState.Open) {
			return { };
		}
		auto output = retire(aState);
		aState.Open = true;
		request_work(aState, output);
		return output;
	}

	auto close_source_search(source_search_state& aState) -> source_search_output
	{
		auto output = retire(aState);
		aState.Open = false;
		return output;
	}

	auto reset_source_search(source_search_state& aState) -> source_search_output
	{
		auto output = close_source_search(aState);
		aState.Scope.reset();
		aState.QueryOwner.reset();
		aState.Input      = source_search_input::empty;
		aState.Options    = { };
		output.ClearQuery = true;
		return output;
	}

	auto complete_source_search(source_search_state& aState, const source_search_terminal& aTerminal) -> source_search_output
	{
		if (!aState.Pending || *aState.Pending != aTerminal.Request) {
			auto output   = source_search_output{ };
			output.Reason = source_search_reason::stale_completion;
			return output;
		}
		aState.Pending.reset();
		auto output    = source_search_output{ };
		output.Changed = true;
		if (!valid_terminal(aTerminal)) {
			aState.Status  = source_search_status::failure;
			aState.Summary = { };
			aState.CurrentMatch.reset();
			output.Reason = source_search_reason::invalid_completion;
			return output;
		}
		output.Accepted = true;
		aState.Summary  = aTerminal.Summary;
		aState.Status   = aTerminal.Status;
		if (aState.Status == source_search_status::complete && (aState.Summary.SearchedFiles != aState.Summary.EligibleFiles || aState.Summary.SkippedSides != 0U)) {
			aState.Status = source_search_status::partial;
		}
		aState.CurrentMatch = aState.Summary.Matches == 0U ? std::nullopt : std::optional{ aState.CurrentMatch && *aState.CurrentMatch < aState.Summary.Matches ? *aState.CurrentMatch : aState.Summary.InitialMatch.value_or(0U) };
		return output;
	}

	auto progress_source_search(source_search_state& aState, const source_search_request& aRequest, const source_search_summary aSummary) -> source_search_output
	{
		auto output = source_search_output{ };
		if (!aState.Pending || *aState.Pending != aRequest) {
			output.Reason = source_search_reason::stale_completion;
			return output;
		}
		if (!valid_terminal({ aRequest, source_search_status::partial, aSummary }) || aSummary.Matches < aState.Summary.Matches || aSummary.SearchedFiles < aState.Summary.SearchedFiles || aSummary.SkippedSides < aState.Summary.SkippedSides) {
			output.Reason = source_search_reason::invalid_completion;
			return output;
		}
		aState.Summary = aSummary;
		if (aSummary.Matches != 0U && !aState.CurrentMatch) {
			aState.CurrentMatch = 0U;
		}
		output.Accepted = true;
		output.Changed  = true;
		return output;
	}

	auto navigate_source_search(source_search_state& aState, const change_navigation_direction aDirection) -> source_search_output
	{
		auto output = source_search_output{ };
		if (!aState.CurrentMatch || aState.Summary.Matches == 0U) {
			output.Reason = source_search_reason::no_matches;
			return output;
		}
		output.Changed = true;
		if (aDirection == change_navigation_direction::next) {
			output.Wrapped      = *aState.CurrentMatch + 1U == aState.Summary.Matches;
			aState.CurrentMatch = output.Wrapped ? 0U : *aState.CurrentMatch + 1U;
		} else {
			output.Wrapped      = *aState.CurrentMatch == 0U;
			aState.CurrentMatch = output.Wrapped ? aState.Summary.Matches - 1U : *aState.CurrentMatch - 1U;
		}
		return output;
	}

	auto select_source_search_match(source_search_state& aState, const std::uint32_t aIndex) -> source_search_output
	{
		if (!aState.CurrentMatch || aIndex >= aState.Summary.Matches) {
			return { .Reason = source_search_reason::no_matches };
		}
		aState.CurrentMatch = aIndex;
		return { .Changed = true };
	}
	auto source_search_no_results(const source_search_state& aState) noexcept -> bool
	{
		return aState.Status == source_search_status::complete && aState.Summary.Matches == 0U;
	}
}
