#include "bpr/core/document_tabs.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <limits>
#include <utility>

namespace bpr::core {
	namespace {
		using enum document_tab_reason;

		[[nodiscard]] auto refused(const document_tab_reason aReason) -> document_tabs_output
		{
			return { .Changed = false, .Load = std::nullopt, .Reason = aReason };
		}

		[[nodiscard]] auto index_of(const document_tabs_state& aState, const document_tab_id aTab)
			-> std::optional<std::size_t>
		{
			const auto found = std::ranges::find(aState.Tabs, aTab, &document_tab::Id);
			if (found == aState.Tabs.end()) {
				return std::nullopt;
			}
			return static_cast<std::size_t>(std::distance(aState.Tabs.begin(), found));
		}

		[[nodiscard]] auto active_index(const document_tabs_state& aState) -> std::optional<std::size_t>
		{
			return aState.Active ? index_of(aState, *aState.Active) : std::nullopt;
		}

		/// \brief A pair this collection can hold: exact, and in the pull request
		/// the window's comparison names.
		[[nodiscard]] auto admissible(const document_tabs_state& aState, const comparison_pair& aPair) -> bool
		{
			return valid_exact_pair(aPair)
				&& (!aState.WindowComparison
					|| aState.WindowComparison->Left.Identity.Authority == aPair.Left.Identity.Authority);
		}

		[[nodiscard]] auto mint_tab(document_tabs_state& aState) -> std::optional<document_tab_id>
		{
			if (aState.LastTab == std::numeric_limits<std::uint64_t>::max()) {
				return std::nullopt;
			}
			return document_tab_id{ ++aState.LastTab };
		}

		/// \brief Retires the shown or loading view of a tab that stops being active.
		void set_aside(document_tabs_state& aState, const std::size_t aIndex)
		{
			auto& tab = aState.Tabs[aIndex];
			if (tab.State == document_tab_view_state::loading || tab.State == document_tab_view_state::ready) {
				tab.State = document_tab_view_state::idle;
			}
		}

		/// \brief Starts the active tab's load: a fresh request retires every
		/// earlier one, whichever tab it named.
		///
		/// \details [[BPR-1478]]. A file the reviewer chose since the tab took
		/// its view is the later choice, so the load presents it, at the file's
		/// initial position.
		[[nodiscard]] auto load_active(document_tabs_state& aState) -> std::optional<document_tab_view_request>
		{
			const auto index  = active_index(aState);
			const auto chosen = std::exchange(aState.Chosen, std::nullopt);
			aState.Pending.reset();
			aState.Presented.reset();
			if (!index || aState.LastRequest == std::numeric_limits<std::uint64_t>::max()) {
				return std::nullopt;
			}
			auto& tab = aState.Tabs[*index];
			if (chosen && tab.View.File != chosen) {
				tab.View.File = *chosen;
				tab.View.Selection.reset();
			}
			tab.State = document_tab_view_state::loading;
			aState.Pending.emplace(document_tab_view_request{
				.Tab     = tab.Id,
				.Request = request_id{ ++aState.LastRequest },
				.View    = tab.View,
			});
			return aState.Pending;
		}

		/// \brief Makes the tab at `aIndex` active and loads it. A file chosen
		/// for the tab that was active is retired: the switch is later.
		[[nodiscard]] auto activate(document_tabs_state& aState, const std::size_t aIndex) -> document_tabs_output
		{
			if (const auto current = active_index(aState); current && *current != aIndex) {
				set_aside(aState, *current);
			}
			aState.Active = aState.Tabs[aIndex].Id;
			aState.Chosen.reset();
			return { .Changed = true, .Load = load_active(aState), .Reason = none };
		}

		[[nodiscard]] auto new_tab(
			document_tabs_state&   aState,
			window_review_position aView) -> std::optional<document_tab>
		{
			if (aState.Tabs.size() >= maximum_document_tabs) {
				return std::nullopt;
			}
			const auto identity = mint_tab(aState);
			if (!identity) {
				return std::nullopt;
			}
			return document_tab{ .Id = *identity, .View = std::move(aView), .State = document_tab_view_state::idle };
		}

		/// \brief The first tab of an empty collection, which is always active.
		[[nodiscard]] auto open_first(document_tabs_state& aState, const file_identity& aFile) -> document_tabs_output
		{
			auto tab = new_tab(aState, window_review_position{ .Comparison = *aState.WindowComparison, .File = aFile, .Selection = std::nullopt });
			if (!tab) {
				return refused(tab_limit);
			}
			aState.Tabs.push_back(std::move(*tab));
			return activate(aState, aState.Tabs.size() - 1U);
		}

		[[nodiscard]] auto apply(document_tabs_state& aState, const open_in_active_tab_intent& aIntent) -> document_tabs_output
		{
			if (!aState.WindowComparison) {
				return refused(no_window_comparison);
			}
			if (!aIntent.File.valid()) {
				return refused(invalid_file);
			}
			// [[BPR-1478]]. The active tab now has the window's file.
			aState.Chosen.reset();
			const auto index = active_index(aState);
			if (!index) {
				return open_first(aState, aIntent.File);
			}
			auto& tab = aState.Tabs[*index];
			if (tab.View.File == aIntent.File) {
				// The same file again is a retry only where nothing is shown.
				if (tab.State == document_tab_view_state::loading || tab.State == document_tab_view_state::ready) {
					return { };
				}
				return { .Changed = true, .Load = load_active(aState), .Reason = none };
			}
			tab.View.File = aIntent.File;
			tab.View.Selection.reset();
			return { .Changed = true, .Load = load_active(aState), .Reason = none };
		}

		[[nodiscard]] auto apply(document_tabs_state& aState, const open_in_new_tab_intent& aIntent) -> document_tabs_output
		{
			if (!aState.WindowComparison) {
				return refused(no_window_comparison);
			}
			if (!aIntent.File.valid()) {
				return refused(invalid_file);
			}
			const auto index = active_index(aState);
			if (!index) {
				return open_first(aState, aIntent.File);
			}
			auto tab = new_tab(aState, window_review_position{ .Comparison = *aState.WindowComparison, .File = aIntent.File, .Selection = std::nullopt });
			if (!tab) {
				return refused(tab_limit);
			}
			// A background tab: the active tab keeps its view and nothing loads.
			aState.Tabs.insert(aState.Tabs.begin() + static_cast<std::ptrdiff_t>(*index + 1U), std::move(*tab));
			return { .Changed = true, .Load = std::nullopt, .Reason = none };
		}

		[[nodiscard]] auto apply(document_tabs_state& aState, const duplicate_tab_intent& aIntent) -> document_tabs_output
		{
			const auto index = index_of(aState, aIntent.Tab);
			if (!index) {
				return refused(unknown_tab);
			}
			auto tab = new_tab(aState, aState.Tabs[*index].View);
			if (!tab) {
				return refused(tab_limit);
			}
			aState.Tabs.insert(aState.Tabs.begin() + static_cast<std::ptrdiff_t>(*index + 1U), std::move(*tab));
			return activate(aState, *index + 1U);
		}

		[[nodiscard]] auto apply(document_tabs_state& aState, const close_tab_intent& aIntent) -> document_tabs_output
		{
			const auto index = index_of(aState, aIntent.Tab);
			if (!index) {
				return refused(unknown_tab);
			}
			const auto wasActive = aState.Active == aIntent.Tab;
			aState.Tabs.erase(aState.Tabs.begin() + static_cast<std::ptrdiff_t>(*index));
			if (!wasActive) {
				return { .Changed = true, .Load = std::nullopt, .Reason = none };
			}
			aState.Active.reset();
			aState.Pending.reset();
			aState.Presented.reset();
			aState.Chosen.reset();
			if (aState.Tabs.empty()) {
				return { .Changed = true, .Load = std::nullopt, .Reason = none };
			}
			// The next neighbour now holds the closed tab's index; the last tab's
			// neighbour is the previous one.
			return activate(aState, std::min(*index, aState.Tabs.size() - 1U));
		}

		[[nodiscard]] auto apply(document_tabs_state& aState, const close_other_tabs_intent& aIntent) -> document_tabs_output
		{
			const auto index = index_of(aState, aIntent.Tab);
			if (!index) {
				return refused(unknown_tab);
			}
			const auto wasActive = aState.Active == aIntent.Tab;
			if (aState.Tabs.size() == 1U) {
				return { };
			}
			auto kept = std::move(aState.Tabs[*index]);
			aState.Tabs.clear();
			aState.Tabs.push_back(std::move(kept));
			if (wasActive) {
				return { .Changed = true, .Load = std::nullopt, .Reason = none };
			}
			aState.Active.reset();
			return activate(aState, 0U);
		}

		[[nodiscard]] auto apply(document_tabs_state& aState, const activate_tab_intent& aIntent) -> document_tabs_output
		{
			const auto index = index_of(aState, aIntent.Tab);
			if (!index) {
				return refused(unknown_tab);
			}
			if (aState.Active == aIntent.Tab) {
				return { };
			}
			return activate(aState, *index);
		}

		[[nodiscard]] auto apply(document_tabs_state& aState, const activate_adjacent_tab_intent& aIntent) -> document_tabs_output
		{
			const auto index = active_index(aState);
			if (!index) {
				return refused(no_active_tab);
			}
			const auto count = aState.Tabs.size();
			if (count == 1U) {
				return { };
			}
			const auto next = aIntent.Direction == change_navigation_direction::next
				? (*index + 1U) % count
				: (*index + count - 1U) % count;
			return activate(aState, next);
		}

		[[nodiscard]] auto apply(document_tabs_state& aState, const return_tab_to_window_comparison_intent& aIntent) -> document_tabs_output
		{
			const auto index = index_of(aState, aIntent.Tab);
			if (!index) {
				return refused(unknown_tab);
			}
			auto& tab = aState.Tabs[*index];
			if (tab.View.Comparison == *aState.WindowComparison) {
				return { };
			}
			tab.View.Comparison = *aState.WindowComparison;
			tab.View.Selection.reset();
			if (aState.Active == aIntent.Tab) {
				return { .Changed = true, .Load = load_active(aState), .Reason = none };
			}
			tab.State = document_tab_view_state::idle;
			return { .Changed = true, .Load = std::nullopt, .Reason = none };
		}

		[[nodiscard]] auto apply(document_tabs_state& aState, const make_tab_comparison_window_intent& aIntent) -> document_tabs_output
		{
			const auto index = index_of(aState, aIntent.Tab);
			if (!index) {
				return refused(unknown_tab);
			}
			const auto& pair = aState.Tabs[*index].View.Comparison;
			if (aState.WindowComparison == pair) {
				return { };
			}
			aState.WindowComparison = pair;
			return { .Changed = true, .Load = std::nullopt, .Reason = none };
		}

		[[nodiscard]] auto apply(document_tabs_state& aState, const select_tab_comparison_intent& aIntent) -> document_tabs_output
		{
			const auto index = active_index(aState);
			if (!index) {
				return refused(no_active_tab);
			}
			if (!admissible(aState, aIntent.Comparison)) {
				return refused(invalid_comparison);
			}
			auto& tab = aState.Tabs[*index];
			if (tab.View.Comparison == aIntent.Comparison) {
				return { };
			}
			if (tab.View.Comparison == *aState.WindowComparison) {
				aState.WindowComparison = aIntent.Comparison;
			}
			tab.View.Comparison = aIntent.Comparison;
			tab.View.Selection.reset();
			return { .Changed = true, .Load = load_active(aState), .Reason = none };
		}

		[[nodiscard]] auto apply(document_tabs_state& aState, const set_window_comparison_intent& aIntent) -> document_tabs_output
		{
			// A different pull request is a different collection, not a new
			// window comparison for the tabs this one holds.
			if (!valid_exact_pair(aIntent.Comparison)
				|| (!aState.Tabs.empty() && !admissible(aState, aIntent.Comparison))) {
				return refused(invalid_comparison);
			}
			if (aState.WindowComparison == aIntent.Comparison) {
				return { };
			}
			aState.WindowComparison = aIntent.Comparison;
			return { .Changed = true, .Load = std::nullopt, .Reason = none };
		}

		[[nodiscard]] auto needs_window(const document_tab_intent& aIntent) -> bool
		{
			return std::holds_alternative<return_tab_to_window_comparison_intent>(aIntent)
				|| std::holds_alternative<select_tab_comparison_intent>(aIntent);
		}
	} // namespace

	auto transition(document_tabs_state& aState, const document_tab_intent& aIntent) -> document_tabs_output
	{
		// Every tab exists only under a window comparison, so these two can
		// only lack one when the state holds no tab either.
		if (needs_window(aIntent) && !aState.WindowComparison) {
			return refused(no_window_comparison);
		}
		return std::visit([&aState](const auto& aValue) { return apply(aState, aValue); }, aIntent);
	}

	auto complete_view(
		document_tabs_state&            aState,
		const request_id                aRequest,
		const document_tab_view_outcome aOutcome) -> document_tabs_output
	{
		if (!aState.Pending || aState.Pending->Request != aRequest) {
			return refused(stale_view);
		}
		const auto index = index_of(aState, aState.Pending->Tab);
		if (!index) {
			aState.Pending.reset();
			return refused(stale_view);
		}
		auto& tab = aState.Tabs[*index];
		if (aOutcome == document_tab_view_outcome::ready) {
			tab.State        = document_tab_view_state::ready;
			aState.Presented = std::move(aState.Pending);
		} else {
			tab.State = document_tab_view_state::unavailable;
		}
		aState.Pending.reset();
		return { .Changed = true, .Load = std::nullopt, .Reason = none };
	}

	auto observe_reading_position(
		document_tabs_state&                     aState,
		const request_id                         aRequest,
		const std::optional<semantic_selection>& aSelection) -> document_tabs_output
	{
		if (!aState.Presented || aState.Presented->Request != aRequest) {
			return refused(stale_view);
		}
		const auto index = index_of(aState, aState.Presented->Tab);
		if (!index) {
			return refused(stale_view);
		}
		auto& tab = aState.Tabs[*index];
		if (tab.View.Selection == aSelection) {
			return { };
		}
		auto view      = tab.View;
		view.Selection = aSelection;
		if (!valid(view)) {
			return refused(invalid_position);
		}
		tab.View = std::move(view);
		return { .Changed = true, .Load = std::nullopt, .Reason = none };
	}

	auto observe_file_choice(document_tabs_state& aState, const file_identity& aFile) -> document_tabs_output
	{
		if (!aFile.valid()) {
			return refused(invalid_file);
		}
		const auto index = active_index(aState);
		if (index && aState.Tabs[*index].View.File == aFile) {
			aState.Chosen.reset();
		} else {
			aState.Chosen = aFile;
		}
		return { };
	}

	auto reload_active(document_tabs_state& aState) -> document_tabs_output
	{
		if (!aState.Active) {
			return refused(no_active_tab);
		}
		return { .Changed = true, .Load = load_active(aState), .Reason = none };
	}

	auto restore(document_tabs_state& aState, const window_review_tabs& aTabs) -> document_tabs_output
	{
		if (!valid(aTabs)) {
			return refused(invalid_restoration);
		}
		auto restored             = document_tabs_state{ };
		restored.WindowComparison = aTabs.WindowComparison;
		// Identities and requests continue from this state's, so nothing an
		// earlier collection issued can name a restored tab.
		restored.LastTab     = aState.LastTab;
		restored.LastRequest = aState.LastRequest;
		for (const auto& view : aTabs.Tabs) {
			auto tab = new_tab(restored, view);
			if (!tab) {
				return refused(invalid_restoration);
			}
			restored.Tabs.push_back(std::move(*tab));
		}
		aState = std::move(restored);
		if (!aTabs.Active) {
			return { .Changed = true, .Load = std::nullopt, .Reason = none };
		}
		return activate(aState, *aTabs.Active);
	}

	auto persisted(const document_tabs_state& aState) -> std::optional<window_review_tabs>
	{
		if (!aState.WindowComparison) {
			return std::nullopt;
		}
		auto result             = window_review_tabs{ };
		result.WindowComparison = *aState.WindowComparison;
		result.Tabs.reserve(aState.Tabs.size());
		for (const auto& tab : aState.Tabs) {
			result.Tabs.push_back(tab.View);
		}
		if (const auto index = active_index(aState); index) {
			result.Active = static_cast<std::uint32_t>(*index);
		}
		return result;
	}

	auto has_own_comparison(const document_tabs_state& aState, const document_tab& aTab) noexcept -> bool
	{
		return aState.WindowComparison && aTab.View.Comparison != *aState.WindowComparison;
	}

	auto find_tab(const document_tabs_state& aState, const document_tab_id aTab) noexcept -> const document_tab*
	{
		const auto found = std::ranges::find(aState.Tabs, aTab, &document_tab::Id);
		return found == aState.Tabs.end() ? nullptr : &*found;
	}
} // namespace bpr::core
