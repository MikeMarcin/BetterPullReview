#ifndef BPR_TESTS_END_TO_END_RIGHT_SIDE_CHANGE_HPP
#define BPR_TESTS_END_TO_END_RIGHT_SIDE_CHANGE_HPP

/// \file
/// \brief Moves a course's selection to a change it can comment on.
///
/// \details [[BPR-0587]]. A new draft is offered only on a right-side,
/// current-head line; a left-side line takes replies, not new roots. Change
/// navigation lands on a change's first left line when it has one, so a course
/// that navigates to a change and then writes a new comment picks the change's
/// right side first, as a reviewer would. [[BPR-1245]] and [[BPR-1246]] found
/// four courses still waiting for a draft on the left.

#include "bpr/qt/diff_workbench.hpp"

namespace bpr::e2e {
	/// \brief From the selected change on, finds the first change with a
	/// right side and selects that side's first line.
	///
	/// \details A change with no right side (a pure deletion) is skipped with
	/// the ordinary next-change gesture. Returns false when nothing is
	/// selected, the comparison is not ready, or no later change has a right
	/// side.
	[[nodiscard]] inline auto select_right_side_of_change(qt::diff_workbench& aSurface) -> bool
	{
		constexpr auto maximumChanges = 256;
		for (auto change = 0; change < maximumChanges; ++change) {
			const auto& selection = aSurface.selection();
			const auto& result    = aSurface.current_result();
			if (!selection.has_value() || !selection->Group.has_value() || result == nullptr) {
				return false;
			}
			for (const auto& segment : result->Alignment.Segments) {
				if (segment.Group == selection->Group && !segment.Right.empty()) {
					return aSurface.select_conversation_line(core::diff_side::right, segment.Right.Begin);
				}
			}
			if (!aSurface.navigate_change(core::change_navigation_direction::next, core::change_navigation_policy::stop)) {
				return false;
			}
		}
		return false;
	}
}

#endif // BPR_TESTS_END_TO_END_RIGHT_SIDE_CHANGE_HPP
