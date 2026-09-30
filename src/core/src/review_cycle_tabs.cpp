#include "bpr/core/review_cycle_tabs.hpp"

#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <type_traits>

namespace bpr::core {
	namespace {
		auto position_of(const cycle_tabs_state& aState, const document_tab_id aTab) -> std::optional<std::size_t>
		{
			const auto found = std::ranges::find(aState.Tabs, aTab, &cycle_tab::Id);
			return found == aState.Tabs.end() ? std::nullopt : std::optional{ static_cast<std::size_t>(found - aState.Tabs.begin()) };
		}

		auto add_tab(cycle_tabs_state& aState, cycle_tab_view aView, const std::size_t aAt, const bool aActivate) -> cycle_tabs_output
		{
			if (aState.Tabs.size() >= maximum_cycle_tabs) {
				return { .Reason = document_tab_reason::tab_limit };
			}
			const auto id = document_tab_id{ ++aState.LastTab };
			aState.Tabs.insert(aState.Tabs.begin() + static_cast<std::ptrdiff_t>(std::min(aAt, aState.Tabs.size())), cycle_tab{ id, std::move(aView) });
			if (aActivate || !aState.Active) {
				aState.Active = id;
			}
			return { .Changed = true };
		}

		auto open(cycle_tabs_state& aState, const std::string_view aPath, const bool aNewTab) -> cycle_tabs_output
		{
			if (!aState.Revision) {
				return { .Reason = document_tab_reason::no_window_comparison };
			}
			if (!valid_cycle_path(aPath)) {
				return { .Reason = document_tab_reason::invalid_file };
			}
			const auto active = aState.Active ? position_of(aState, *aState.Active) : std::nullopt;
			if (!active) {
				return add_tab(aState, { .Revision = *aState.Revision, .Path = std::string{ aPath } }, aState.Tabs.size(), true);
			}
			if (aNewTab) {
				// A background tab after the active one, on the revision new tabs take.
				return add_tab(aState, { .Revision = *aState.Revision, .Path = std::string{ aPath } }, *active + 1U, false);
			}
			auto& view = aState.Tabs[*active].View;
			if (view.Path == aPath) {
				return { };
			}
			// The active tab keeps its own revision, or comparison, and shows the other file.
			view.Path = std::string{ aPath };
			view.Line = 0;
			return { .Changed = true };
		}
	}

	auto cycle_file_identity(const local_cycle_subject& aSubject, const std::string_view aPath) -> file_identity
	{
		constexpr auto domain = std::string_view{ "BPR-cycle-file-v1" };
		auto           bytes  = std::vector<std::uint8_t>(domain.begin(), domain.end());
		bytes.push_back(0);
		const auto& incarnation = aSubject.Authority.Incarnation.bytes();
		bytes.insert(bytes.end(), incarnation.begin(), incarnation.end());
		for (const auto value : { aSubject.Authority.Profile.value(), aSubject.Cycle.value(), aSubject.Generation }) {
			for (auto shift = 56; shift >= 0; shift -= 8) {
				bytes.push_back(static_cast<std::uint8_t>(value >> static_cast<unsigned>(shift)));
			}
		}
		bytes.insert(bytes.end(), aPath.begin(), aPath.end());
		return file_identity{ sha256(bytes) };
	}

	auto transition(cycle_tabs_state& aState, const local_cycle_subject& aSubject, const document_tab_intent& aIntent, const std::vector<std::string>& aPaths)
		-> cycle_tabs_output
	{
		const auto pathOf = [&](const file_identity& aFile) -> std::optional<std::string_view> {
			const auto found = std::ranges::find_if(aPaths, [&](const std::string& aPath) { return cycle_file_identity(aSubject, aPath) == aFile; });
			return found == aPaths.end() ? std::nullopt : std::optional<std::string_view>{ *found };
		};
		return std::visit(
			[&](const auto& aValue) -> cycle_tabs_output {
				using value_type = std::decay_t<decltype(aValue)>;
				if constexpr (std::is_same_v<value_type, open_in_active_tab_intent> || std::is_same_v<value_type, open_in_new_tab_intent>) {
					const auto path = pathOf(aValue.File);
					if (!path) {
						return { .Reason = document_tab_reason::invalid_file };
					}
					return open(aState, *path, std::is_same_v<value_type, open_in_new_tab_intent>);
				} else if constexpr (std::is_same_v<value_type, duplicate_tab_intent>) {
					const auto at = position_of(aState, aValue.Tab);
					if (!at) {
						return { .Reason = document_tab_reason::unknown_tab };
					}
					return add_tab(aState, aState.Tabs[*at].View, *at + 1U, true);
				} else if constexpr (std::is_same_v<value_type, close_tab_intent>) {
					const auto at = position_of(aState, aValue.Tab);
					if (!at) {
						return { .Reason = document_tab_reason::unknown_tab };
					}
					aState.Tabs.erase(aState.Tabs.begin() + static_cast<std::ptrdiff_t>(*at));
					if (aState.Active == aValue.Tab) {
						// The neighbour to the right takes over, else the one to the left.
						aState.Active = aState.Tabs.empty() ? std::nullopt : std::optional{ aState.Tabs[std::min(*at, aState.Tabs.size() - 1U)].Id };
					}
					return { .Changed = true };
				} else if constexpr (std::is_same_v<value_type, close_other_tabs_intent>) {
					const auto at = position_of(aState, aValue.Tab);
					if (!at) {
						return { .Reason = document_tab_reason::unknown_tab };
					}
					const auto kept = aState.Tabs[*at];
					const auto was  = aState.Tabs.size() != 1U || aState.Active != aValue.Tab;
					aState.Tabs     = { kept };
					aState.Active   = kept.Id;
					return { .Changed = was };
				} else if constexpr (std::is_same_v<value_type, activate_tab_intent>) {
					if (!position_of(aState, aValue.Tab)) {
						return { .Reason = document_tab_reason::unknown_tab };
					}
					const auto changed = aState.Active != aValue.Tab;
					aState.Active      = aValue.Tab;
					return { .Changed = changed };
				} else if constexpr (std::is_same_v<value_type, activate_adjacent_tab_intent>) {
					const auto at = aState.Active ? position_of(aState, *aState.Active) : std::nullopt;
					if (!at) {
						return { .Reason = document_tab_reason::no_active_tab };
					}
					const auto count = aState.Tabs.size();
					const auto next  = aValue.Direction == change_navigation_direction::next ? (*at + 1U) % count : (*at + count - 1U) % count;
					const auto moved = next != *at;
					aState.Active    = aState.Tabs[next].Id;
					return { .Changed = moved };
				} else {
					// Comparisons of cycle material are their own views; a tab here is full source.
					return { .Reason = document_tab_reason::invalid_comparison };
				}
			},
			aIntent);
	}

	auto observe_line(cycle_tabs_state& aState, const document_tab_id aTab, const std::uint64_t aLine) -> cycle_tabs_output
	{
		const auto at = position_of(aState, aTab);
		if (!at) {
			return { .Reason = document_tab_reason::unknown_tab };
		}
		auto& line = aState.Tabs[*at].View.Line;
		if (line == aLine) {
			return { };
		}
		line = aLine;
		return { .Changed = true };
	}

	auto open_path(cycle_tabs_state& aState, const std::string_view aPath, const bool aNewTab) -> cycle_tabs_output
	{
		return open(aState, aPath, aNewTab);
	}

	auto active_tab(const cycle_tabs_state& aState) noexcept -> const cycle_tab*
	{
		if (!aState.Active) {
			return nullptr;
		}
		const auto found = std::ranges::find(aState.Tabs, *aState.Active, &cycle_tab::Id);
		return found == aState.Tabs.end() ? nullptr : &*found;
	}

	auto shown_pair(const local_cycle_subject& aSubject, const cycle_tab_view& aView) -> cycle_comparison_pair
	{
		auto pair = cycle_comparison_pair{ .Right = { .Cycle = aSubject.Cycle, .Id = aView.Revision } };
		if (aView.Comparison) {
			pair.Left = cycle_source_revision_ref{ .Cycle = aSubject.Cycle, .Id = aView.Comparison->Left };
		}
		return pair;
	}

	auto show_comparison(cycle_tabs_state& aState, const cycle_view_request& aRequest, const cycle_comparison_pair& aPair) -> cycle_tabs_output
	{
		const auto at = aState.Active ? position_of(aState, *aState.Active) : std::nullopt;
		if (!at) {
			return { .Reason = document_tab_reason::no_active_tab };
		}
		if (!aPair.Left) {
			return { .Reason = document_tab_reason::invalid_comparison };
		}
		auto& view = aState.Tabs[*at].View;
		auto  next = cycle_tab_comparison{ .Request = aRequest, .Left = aPair.Left->Id };
		if (view.Comparison == next && view.Revision == aPair.Right.Id) {
			return { };
		}
		view.Comparison = std::move(next);
		view.Revision   = aPair.Right.Id;
		return { .Changed = true };
	}

	auto show_full_source(cycle_tabs_state& aState) -> cycle_tabs_output
	{
		const auto at = aState.Active ? position_of(aState, *aState.Active) : std::nullopt;
		if (!at) {
			return { .Reason = document_tab_reason::no_active_tab };
		}
		auto& view = aState.Tabs[*at].View;
		if (!view.Comparison) {
			return { };
		}
		view.Comparison.reset();
		return { .Changed = true };
	}

	auto refresh_active(cycle_tabs_state& aState, const std::optional<cycle_comparison_pair>& aPair, const cycle_source_revision_id aLatest) -> cycle_tabs_output
	{
		const auto at = aState.Active ? position_of(aState, *aState.Active) : std::nullopt;
		if (!at) {
			return { .Reason = document_tab_reason::no_active_tab };
		}
		auto& view = aState.Tabs[*at].View;
		if (!view.Comparison) {
			if (view.Revision == aLatest) {
				return { };
			}
			view.Revision = aLatest;
			return { .Changed = true };
		}
		if (pinned(view.Comparison->Request)) {
			return { .Reason = document_tab_reason::invalid_comparison };
		}
		if (!aPair || !aPair->Left) {
			return { .Reason = document_tab_reason::invalid_comparison };
		}
		if (view.Comparison->Left == aPair->Left->Id && view.Revision == aPair->Right.Id) {
			return { };
		}
		view.Comparison->Left = aPair->Left->Id;
		view.Revision         = aPair->Right.Id;
		return { .Changed = true };
	}

	auto persisted(const cycle_tabs_state& aState) -> cycle_tabs_record
	{
		auto record = cycle_tabs_record{ .Revision = aState.Revision };
		for (std::size_t index = 0; index < aState.Tabs.size(); ++index) {
			record.Views.push_back(aState.Tabs[index].View);
			if (aState.Active == aState.Tabs[index].Id) {
				record.Active = index;
			}
		}
		return record;
	}

	auto restore(cycle_tabs_state& aState, const review_cycle_state& aCycle, const cycle_tabs_record& aRecord) -> cycle_tabs_output
	{
		const auto allocated = [&](const cycle_source_revision_id aRevision) {
			return valid_cycle_id(aRevision) && aRevision.value() < aCycle.Next.SourceRevision;
		};
		const auto comparison = [&](const cycle_tab_comparison& aComparison) {
			if (const auto* round = std::get_if<this_round_request>(&aComparison.Request)) {
				return valid_cycle_id(round->Round) && round->Round.value() < aCycle.Next.Round && (!round->Result || allocated(*round->Result));
			}
			if (const auto* chosen = std::get_if<chosen_pair_request>(&aComparison.Request)) {
				return allocated(chosen->Left) && allocated(chosen->Right);
			}
			return true;
		};
		const auto validView = [&](const cycle_tab_view& aView) {
			return allocated(aView.Revision) && valid_cycle_path(aView.Path) && (!aView.Comparison || (allocated(aView.Comparison->Left) && comparison(*aView.Comparison)));
		};
		if (aRecord.Views.size() > maximum_cycle_tabs || (aRecord.Revision && !allocated(*aRecord.Revision))
			|| (aRecord.Active && *aRecord.Active >= aRecord.Views.size()) || !std::ranges::all_of(aRecord.Views, validView)) {
			return { .Reason = document_tab_reason::invalid_restoration };
		}
		auto state = cycle_tabs_state{ .Revision = aRecord.Revision, .LastTab = aState.LastTab };
		for (std::size_t index = 0; index < aRecord.Views.size(); ++index) {
			const auto id = document_tab_id{ ++state.LastTab };
			state.Tabs.push_back({ id, aRecord.Views[index] });
			if (aRecord.Active == index) {
				state.Active = id;
			}
		}
		const auto changed = state.Tabs != aState.Tabs || state.Revision != aState.Revision;
		aState             = std::move(state);
		return { .Changed = changed };
	}
}
