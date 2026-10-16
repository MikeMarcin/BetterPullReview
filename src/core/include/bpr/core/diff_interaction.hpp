#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_DIFF_INTERACTION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_DIFF_INTERACTION_HPP

#include "bpr/core/diff_semantics.hpp"

#include <compare>
#include <optional>
#include <variant>

namespace bpr::core {
	/// \brief Describes the compact semantic interaction state of one workbench.
	struct diff_interaction_state
	{
		std::optional<semantic_selection> Selection;

		auto operator<=>(const diff_interaction_state&) const = default;
	};

	struct select_diff_line_intent
	{
		source_line_ref Line;

		auto operator<=>(const select_diff_line_intent&) const = default;
	};

	struct select_diff_group_intent
	{
		change_group_id Group;

		auto operator<=>(const select_diff_group_intent&) const = default;
	};

	/// \brief One change-navigation gesture.
	///
	/// \details [[BPR-1532]]. `Reach` distinguishes Previous and Next change
	/// (`adjacent`, under `Policy`) from First and Last change (`outermost`,
	/// where `Direction` names the end and `Policy` is not consulted).
	struct navigate_diff_change_intent
	{
		change_navigation_direction Direction = change_navigation_direction::next;
		change_navigation_policy    Policy    = change_navigation_policy::stop;
		change_navigation_reach     Reach     = change_navigation_reach::adjacent;

		auto operator<=>(const navigate_diff_change_intent&) const = default;
	};

	using diff_interaction_intent = std::variant<
		select_diff_line_intent,
		select_diff_group_intent,
		navigate_diff_change_intent>;

	/// \brief Reconciles retained semantic state against an accepted replacement.
	[[nodiscard]] auto reconcile_diff_interaction(
		diff_interaction_state&   aState,
		const computed_file_diff& aDiff) -> bool;
	[[nodiscard]] auto reconcile_diff_interaction(
		diff_interaction_state&    aState,
		const diff_semantic_index& aIndex) -> bool;

	/// \brief Applies one semantic workbench gesture in the functional core.
	[[nodiscard]] auto transition(
		diff_interaction_state&        aState,
		const computed_file_diff&      aDiff,
		const diff_interaction_intent& aIntent) -> bool;
	[[nodiscard]] auto transition(
		diff_interaction_state&        aState,
		const diff_semantic_index&     aIndex,
		const diff_interaction_intent& aIntent) -> bool;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_DIFF_INTERACTION_HPP
