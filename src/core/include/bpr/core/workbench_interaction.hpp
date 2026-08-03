#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_WORKBENCH_INTERACTION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_WORKBENCH_INTERACTION_HPP

#include "bpr/core/diff_model.hpp"
#include "bpr/core/diff_semantics.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <variant>

namespace bpr::core {
	/// \brief Names one workbench file by its stable diff-pipeline identity.
	///
	/// \details This is deliberately the exact identity a `file_comparison_key`
	/// carries. A separate workbench identity space would oblige the host to
	/// retain a second mapping between two notions of the same file, and the
	/// selected workbench file must be usable as a diff request key without one.
	using workbench_file_id = file_identity;

	enum class workbench_drawer : std::uint8_t {
		files,
		conversations,
	};

	enum class workbench_drawer_state : std::uint8_t {
		collapsed,
		expanded,
	};

	// `diff_reason_code` reports why one file comparison is unavailable. None of
	// its members can express workbench navigation legality, so this narrow enum
	// carries the workbench reasons instead of widening the diff vocabulary.
	enum class workbench_reason_code : std::uint8_t {
		none,
		no_active_workbench,
		no_actionable_files,
		at_first_file,
		at_last_file,
		invalid_file_identity,
		invalid_navigation_cursor,
		no_open_comparison,
		no_changes,
		at_first_change,
		at_last_change,
		invalid_change_selection,
		invalid_change_cursor,
	};

	/// \brief Describes the bounded workbench navigation cursor record.
	///
	/// \details The core never retains the workbench file list. The host supplies
	/// these bounded facts about the current filtered actionable order and the
	/// core decides legality from them alone. `Ordinal` is 1-based within that
	/// order and is zero when the selection is absent from it, which is the only
	/// case where `PreviousAvailable` and `NextAvailable` carry a fact the other
	/// members do not already imply.
	struct workbench_navigation_cursor
	{
		std::optional<workbench_file_id> File;
		std::uint32_t                    Ordinal           = 0U;
		std::uint32_t                    ActionableCount   = 0U;
		bool                             PreviousAvailable = false;
		bool                             NextAvailable     = false;

		auto operator<=>(const workbench_navigation_cursor&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	///
	/// \details A resident selection must agree with its own order: it exists,
	/// its ordinal is inside the actionable count, and its neighbour facts follow
	/// from that ordinal. An absent selection has no ordinal and can enter the
	/// order from either end whenever it is not empty. The core rejects any other
	/// shape rather than deciding navigation from an incoherent observation.
	[[nodiscard]] auto valid(const workbench_navigation_cursor& aCursor) noexcept
		-> bool;

	/// \brief Describes the bounded workbench change cursor record.
	///
	/// \details The core holds no diff, exactly as it holds no file list. The
	/// host resolves the RFC-BPR-0004 semantic index and reports these bounded
	/// facts about the open comparison's canonical change order; the core decides
	/// change-command legality from them alone. `Ordinal` is 1-based within that
	/// order and is zero when the selected group is absent from it, which is the
	/// selection `navigate_change` refuses rather than steps from.
	struct workbench_change_cursor
	{
		std::optional<change_group_id> Group;
		std::uint32_t                  Ordinal           = 0U;
		std::uint32_t                  ChangeCount       = 0U;
		bool                           PreviousAvailable = false;
		bool                           NextAvailable     = false;

		auto operator<=>(const workbench_change_cursor&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	///
	/// \details This mirrors the file cursor contract with one deliberate
	/// difference: the change order is resolved rather than walked, so a
	/// selection that order cannot place reaches nothing on either side, where a
	/// file selection that left its order still has neighbours only the host can
	/// name.
	[[nodiscard]] auto valid(const workbench_change_cursor& aCursor) noexcept
		-> bool;

	/// \brief Describes the select workbench file intent record.
	struct select_workbench_file_intent
	{
		workbench_file_id File;

		auto operator<=>(const select_workbench_file_intent&) const = default;
	};

	/// \brief Describes the navigate workbench file intent record.
	///
	/// \details The stop-versus-wrap policy is retained state rather than an
	/// intent field, so command enablement and the transition that follows it
	/// cannot disagree about the same gesture.
	struct navigate_workbench_file_intent
	{
		change_navigation_direction Direction = change_navigation_direction::next;

		auto operator<=>(const navigate_workbench_file_intent&) const = default;
	};

	/// \brief Describes the set workbench drawer intent record.
	struct set_workbench_drawer_intent
	{
		workbench_drawer       Drawer = workbench_drawer::files;
		workbench_drawer_state State  = workbench_drawer_state::expanded;

		auto operator<=>(const set_workbench_drawer_intent&) const = default;
	};

	/// \brief Describes the toggle workbench drawer intent record.
	struct toggle_workbench_drawer_intent
	{
		workbench_drawer Drawer = workbench_drawer::files;

		auto operator<=>(const toggle_workbench_drawer_intent&) const = default;
	};

	using workbench_interaction_intent = std::variant<
		select_workbench_file_intent,
		navigate_workbench_file_intent,
		set_workbench_drawer_intent,
		toggle_workbench_drawer_intent>;

	enum class workbench_navigation_target : std::uint8_t {
		exact_file,
		previous_actionable,
		next_actionable,
		first_actionable,
		last_actionable,
	};

	/// \brief Describes the workbench navigation decision record.
	///
	/// \details The core decides which entry of the filtered actionable order the
	/// host must resolve; the host performs the lookup and answers with the next
	/// bounded cursor. `File` names an exact target, `Ordinal` names a 1-based
	/// position in that order, and a relative target against a selection that
	/// left the order carries neither, because only the host can name the
	/// neighbour of a nonresident selection.
	struct workbench_navigation_decision
	{
		workbench_navigation_target      Target = workbench_navigation_target::exact_file;
		std::optional<workbench_file_id> File;
		std::optional<std::uint32_t>     Ordinal;

		auto operator<=>(const workbench_navigation_decision&) const = default;
	};

	enum class workbench_command_id : std::uint8_t {
		previous_file,
		next_file,
		previous_change,
		next_change,
		toggle_files_drawer,
		toggle_conversations_drawer,
	};

	inline constexpr std::size_t workbench_command_count = 6U;

	/// \brief Describes the workbench command projection record.
	struct workbench_command_projection
	{
		workbench_command_id  Id             = workbench_command_id::previous_file;
		bool                  Enabled        = false;
		workbench_reason_code DisabledReason = workbench_reason_code::no_active_workbench;

		auto operator<=>(const workbench_command_projection&) const = default;
	};

	/// \brief Describes the workbench command delta record.
	///
	/// \details Entry `i` always projects `workbench_command_id{ i }`. `Policy`
	/// is the retained stop-versus-wrap policy every navigation entry was decided
	/// under. A gesture repeats the policy it was offered, so the gesture and the
	/// enablement that offered it can never disagree about stop versus wrap.
	struct workbench_command_delta
	{
		std::array<workbench_command_projection, workbench_command_count> Commands{ };
		change_navigation_policy                                          Policy = change_navigation_policy::stop;

		auto operator<=>(const workbench_command_delta&) const = default;
	};

	/// \brief Describes the workbench selection delta record.
	///
	/// \details An absent cursor publishes an inactive workbench with no
	/// selection; the delta is emitted only when the retained cursor changed.
	struct workbench_selection_delta
	{
		std::optional<workbench_navigation_cursor> Cursor;

		auto operator<=>(const workbench_selection_delta&) const = default;
	};

	/// \brief Describes the workbench drawer delta record.
	struct workbench_drawer_delta
	{
		workbench_drawer_state Files         = workbench_drawer_state::expanded;
		workbench_drawer_state Conversations = workbench_drawer_state::expanded;

		auto operator<=>(const workbench_drawer_delta&) const = default;
	};

	/// \brief Describes the workbench interaction output record.
	///
	/// \details Every delta is present only when the corresponding published
	/// meaning changed. `Reason` is `none` for an accepted gesture, including an
	/// accepted gesture that asked for the state it already had.
	struct workbench_interaction_output
	{
		std::optional<workbench_navigation_decision> Decision;
		std::optional<workbench_selection_delta>     Selection;
		std::optional<workbench_drawer_delta>        Drawers;
		std::optional<workbench_command_delta>       Commands;
		workbench_reason_code                        Reason = workbench_reason_code::none;

		auto operator<=>(const workbench_interaction_output&) const = default;
	};

	/// \brief Describes the compact semantic interaction state of one workbench.
	///
	/// \details The core retains no file list, diff, path, file content,
	/// conversation, or other unbounded record: only the selected file identity
	/// with its bounded cursor, the equally bounded change cursor of the open
	/// comparison, the semantic state of the two drawers, and the navigation
	/// policy. `Navigation` is absent until the host supplies its first bounded
	/// observation. That absence is the inactive workbench, where every workbench
	/// command is disabled and every workbench gesture is rejected. `Change` is
	/// absent whenever no file comparison is open inside that workbench.
	struct workbench_interaction_state
	{
		std::optional<workbench_navigation_cursor> Navigation;
		std::optional<workbench_change_cursor>     Change;
		workbench_drawer_state                     FilesDrawer         = workbench_drawer_state::expanded;
		workbench_drawer_state                     ConversationsDrawer = workbench_drawer_state::expanded;
		change_navigation_policy                   NavigationPolicy    = change_navigation_policy::stop;

		auto operator<=>(const workbench_interaction_state&) const = default;
	};

	static_assert(std::is_trivially_copyable_v<workbench_interaction_state>);
	static_assert(std::is_trivially_destructible_v<workbench_interaction_state>);
	static_assert(sizeof(workbench_interaction_state) <= 128U);

	/// \brief Computes the workbench commands result.
	///
	/// \details This is the single projection of workbench command legality, for
	/// change navigation exactly as for file navigation. A file navigation
	/// command is enabled exactly when the matching navigate intent would produce
	/// a decision, a change navigation command is enabled exactly when the
	/// bounded change cursor still offers that step, and a drawer command is
	/// enabled exactly when the matching drawer intent would be accepted, so a
	/// widget can never offer a gesture that would be rejected.
	[[nodiscard]] auto project_workbench_commands(
		const workbench_interaction_state& aState) noexcept
		-> workbench_command_delta;

	/// \brief Applies one bounded host navigation observation to the state.
	///
	/// \details An absent cursor deactivates the workbench. A malformed cursor is
	/// rejected without changing any retained fact.
	[[nodiscard]] auto observe_workbench_navigation(
		workbench_interaction_state&                      aState,
		const std::optional<workbench_navigation_cursor>& aCursor)
		-> workbench_interaction_output;

	/// \brief Applies one bounded host change-cursor observation to the state.
	///
	/// \details An absent cursor closes the comparison. A malformed cursor is
	/// rejected without changing any retained fact. Which change is selected
	/// stays RFC-BPR-0004's decision and the core republishes none of it, so this
	/// observation publishes command legality alone.
	[[nodiscard]] auto observe_workbench_change_navigation(
		workbench_interaction_state&                  aState,
		const std::optional<workbench_change_cursor>& aCursor)
		-> workbench_interaction_output;

	/// \brief Applies one semantic workbench gesture in the functional core.
	///
	/// \details Selection and navigation gestures decide a target and leave the
	/// retained cursor to the host observation that resolves it. Drawer gestures
	/// are decided entirely in the core.
	[[nodiscard]] auto transition(
		workbench_interaction_state&        aState,
		const workbench_interaction_intent& aIntent)
		-> workbench_interaction_output;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_WORKBENCH_INTERACTION_HPP
