#include "bpr/core/diff_interaction.hpp"

#include <type_traits>

namespace bpr::core {
	auto reconcile_diff_interaction(
		diff_interaction_state&   aState,
		const computed_file_diff& aDiff) -> bool
	{
		const auto reconciled = reconcile_selection(aDiff, aState.Selection);
		const auto changed    = reconciled.Selection != aState.Selection;
		aState.Selection      = reconciled.Selection;
		return changed;
	}

	auto transition(
		diff_interaction_state&        aState,
		const computed_file_diff&      aDiff,
		const diff_interaction_intent& aIntent) -> bool
	{
		const auto original    = aState.Selection;
		auto       replacement = reconcile_selection(aDiff, original).Selection;
		std::visit(
			[&](const auto& aValue) {
				using intent_type = std::remove_cvref_t<decltype(aValue)>;
				if constexpr (std::is_same_v<intent_type, select_diff_line_intent>) {
					if (const auto selected = select_line(aDiff, aValue.Line)) {
						replacement = selected;
					}
				} else if constexpr (
					std::is_same_v<intent_type, select_diff_group_intent>) {
					if (const auto selected = select_group(aDiff, aValue.Group)) {
						replacement = selected;
					}
				} else {
					const auto current    = replacement
						? replacement->Group
						: std::optional<change_group_id>{ };
					const auto navigation = navigate_change(
						aDiff, current, aValue.Direction, aValue.Policy);
					if (navigation.Group.has_value()) {
						replacement = select_group(aDiff, *navigation.Group);
					}
				}
			},
			aIntent);
		aState.Selection = replacement;
		return replacement != original;
	}
} // namespace bpr::core
