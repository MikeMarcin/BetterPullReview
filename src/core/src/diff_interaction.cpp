#include "bpr/core/diff_interaction.hpp"

#include <type_traits>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto temporary_semantic_index(
			const computed_file_diff& aDiff)
			-> std::optional<diff_semantic_index>
		{
			return build_diff_semantic_index(
				std::shared_ptr<const computed_file_diff>{
					std::shared_ptr<const computed_file_diff>{ },
					&aDiff });
		}
	} // namespace

	auto reconcile_diff_interaction(
		diff_interaction_state&    aState,
		const diff_semantic_index& aIndex) -> bool
	{
		const auto reconciled = reconcile_selection(aIndex, aState.Selection);
		const auto changed    = reconciled.Selection != aState.Selection;
		aState.Selection      = reconciled.Selection;
		return changed;
	}

	auto transition(
		diff_interaction_state&        aState,
		const diff_semantic_index&     aIndex,
		const diff_interaction_intent& aIntent) -> bool
	{
		const auto original    = aState.Selection;
		auto       replacement = reconcile_selection(aIndex, original).Selection;
		std::visit(
			[&](const auto& aValue) {
				using intent_type = std::remove_cvref_t<decltype(aValue)>;
				if constexpr (std::is_same_v<intent_type, select_diff_line_intent>) {
					if (const auto selected = select_line(aIndex, aValue.Line)) {
						replacement = selected;
					}
				} else if constexpr (
					std::is_same_v<intent_type, select_diff_group_intent>) {
					if (const auto selected = select_group(aIndex, aValue.Group)) {
						replacement = selected;
					}
				} else {
					const auto navigation = replacement && !replacement->Group
						? navigate_change(aIndex, replacement->PreferredLine, aValue.Direction, aValue.Policy)
						: navigate_change(aIndex, replacement ? replacement->Group : std::optional<change_group_id>{ }, aValue.Direction, aValue.Policy);
					if (navigation.Group && (navigation.Outcome == change_navigation_outcome::selected || navigation.Outcome == change_navigation_outcome::wrapped)) {
						replacement = select_group(aIndex, *navigation.Group);
					}
				}
			},
			aIntent);
		aState.Selection = replacement;
		return replacement != original;
	}

	auto reconcile_diff_interaction(
		diff_interaction_state&   aState,
		const computed_file_diff& aDiff) -> bool
	{
		const auto index = temporary_semantic_index(aDiff);
		if (!index.has_value()) {
			const auto changed = aState.Selection.has_value();
			aState.Selection.reset();
			return changed;
		}
		return reconcile_diff_interaction(aState, *index);
	}

	auto transition(
		diff_interaction_state&        aState,
		const computed_file_diff&      aDiff,
		const diff_interaction_intent& aIntent) -> bool
	{
		const auto index = temporary_semantic_index(aDiff);
		if (!index.has_value()) {
			const auto changed = aState.Selection.has_value();
			aState.Selection.reset();
			return changed;
		}
		return transition(aState, *index, aIntent);
	}
} // namespace bpr::core
