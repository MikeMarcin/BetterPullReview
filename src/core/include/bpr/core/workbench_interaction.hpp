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
		/// [[BPR-0651]]. The repositories tree, left of the files.
		repositories,
	};

	enum class workbench_drawer_state : std::uint8_t {
		collapsed,
		expanded,
	};
	/// \brief Names whether the source panes number their own lines.
	///
	/// \details This is semantic state rather than a widget flag, for the same
	/// reason a collapsed drawer is: it is a decision the reader made, it
	/// survives a restart, and the action that carries it has to read its
	/// checked state from one projection. It belongs to the workbench surface
	/// exactly as a drawer does, so an inactive workbench refuses the gesture
	/// and disables the command rather than making this the one exception.
	enum class workbench_line_numbers : std::uint8_t {
		hidden,
		shown,
	};

	/// \brief Selects one unified document independently of modality.
	enum class workbench_one_pane : std::uint8_t { disabled,
												   enabled };

	/// \brief [[BPR-1130]]. Whether the Updates line's timeline shows.
	///
	/// \details RFC-BPR-0052: the review bar's Updates disclosure folds the
	/// timeline away and opens it again, and nothing else does. A new window
	/// starts folded and each window keeps its own choice. It is window state
	/// the core retains, as a drawer is, and like a drawer it is about the
	/// room a row takes rather than what is being compared, so the gesture is
	/// accepted whether or not a workbench is active.
	enum class workbench_updates_line : std::uint8_t {
		folded,
		open,
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
		/// [[BPR-0727]]. Overview keeps every drawer collapsed; a drawer opened
		/// by hand is refused while it is the modality.
		overview_keeps_drawers_collapsed,
		/// [[BPR-1295]]. In Discuss the conversation is the page, not a
		/// drawer, so hiding and showing the drawers is not offered there.
		discuss_shows_the_conversation,
		/// [[BPR-1293]]. Conversations is a page that finds a conversation;
		/// its drawers are hidden, so hiding and showing them is not offered.
		conversations_finds_a_conversation,
	};

	/// \brief The review modality the window has chosen.
	///
	/// \details [[BPR-0727]]. The modality is a preset for which drawers are
	/// open; the window chooses it and the core retains it because two
	/// modalities carry rules about drawer gestures. Overview is a page about
	/// the whole pull request, not a column between three drawers, so while it
	/// is chosen a drawer stays collapsed and a gesture to open one is refused
	/// with `overview_keeps_drawers_collapsed`. [[BPR-1295]]: Review keeps the
	/// arrangement it last had in the window, so entering it is the core's
	/// decision too. Discuss's preset is exact set-drawer intents the window
	/// sends, as before. Read merged into Review on 2026-10-06. [[BPR-1293]]:
	/// Conversations is a page that finds a conversation across the pull
	/// request. It has no preset: entering and leaving it changes no drawer,
	/// and the window hides the drawers while it shows, as Discuss does.
	enum class workbench_modality : std::uint8_t {
		overview,
		review,
		discuss,
		conversations,
	};

	/// \brief [[BPR-1295]]. How a modality is being entered.
	///
	/// \details `enter` is every path that lands in a modality: a link, Resume,
	/// Continue review, Open in code, a tour stop, a restored window.
	/// `choose` is the reviewer pressing the modality's key or strip button.
	/// The two differ in one place: choosing Review while the window is
	/// already in Review applies Review's preset, which is the way back to it;
	/// entering Review there changes nothing.
	enum class workbench_modality_entry : std::uint8_t {
		enter,
		choose,
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

	/// \brief Describes the set workbench modality intent record.
	///
	/// \details [[BPR-0727]]. Choosing Overview collapses the three drawers
	/// as part of the same decision, so the rule that Overview keeps them
	/// collapsed holds from the moment it is chosen. [[BPR-1295]]: entering
	/// Review restores the arrangement Review last had in this window, or
	/// applies its preset (Files and Comments open, Repositories as they
	/// were) on the window's first entry and when Review is chosen again
	/// while it shows.
	struct set_workbench_modality_intent
	{
		workbench_modality       Modality = workbench_modality::review;
		workbench_modality_entry Entry    = workbench_modality_entry::enter;

		auto operator<=>(const set_workbench_modality_intent&) const = default;
	};

	/// \brief [[BPR-1295]]. Hides every open drawer, or shows them again.
	///
	/// \details With any drawer open, every drawer collapses and the core
	/// remembers which were open. With none open, the remembered drawers
	/// open again, or Review's preset when nothing is remembered. Overview
	/// refuses it with `overview_keeps_drawers_collapsed`, Discuss with
	/// `discuss_shows_the_conversation` and Conversations with
	/// `conversations_finds_a_conversation` ([[BPR-1293]]).
	struct toggle_workbench_drawers_intent
	{
		auto operator<=>(const toggle_workbench_drawers_intent&) const = default;
	};

	/// \brief Describes the set workbench line numbers intent record.
	///
	/// \details The exact form exists for the same reason the drawer's does:
	/// restoring a persisted setting must not depend on what the state happens
	/// to be when the restore arrives.
	struct set_workbench_line_numbers_intent
	{
		workbench_line_numbers State = workbench_line_numbers::shown;

		auto operator<=>(const set_workbench_line_numbers_intent&) const = default;
	};

	/// \brief Describes the toggle workbench line numbers intent record.
	struct toggle_workbench_line_numbers_intent
	{
		auto operator<=>(const toggle_workbench_line_numbers_intent&) const = default;
	};
	/// \brief Describes the set workbench one pane intent record.
	///
	/// \details The exact form exists for the same reason the drawer's does:
	/// restoring a persisted setting must not depend on what the state happens
	/// to be when the restore arrives.
	struct set_workbench_one_pane_intent
	{
		workbench_one_pane State = workbench_one_pane::disabled;

		auto operator<=>(const set_workbench_one_pane_intent&) const = default;
	};

	/// \brief Describes the toggle workbench one pane intent record.
	struct toggle_workbench_one_pane_intent
	{
		auto operator<=>(const toggle_workbench_one_pane_intent&) const = default;
	};

	/// \brief [[BPR-1130]]. Folds or opens the Updates line exactly.
	///
	/// \details The exact form restores a window's persisted choice; the
	/// reviewer's gesture is the toggle.
	struct set_workbench_updates_line_intent
	{
		workbench_updates_line State = workbench_updates_line::folded;

		auto operator<=>(const set_workbench_updates_line_intent&) const = default;
	};

	/// \brief [[BPR-1130]]. The review bar's Updates disclosure.
	struct toggle_workbench_updates_line_intent
	{
		auto operator<=>(const toggle_workbench_updates_line_intent&) const = default;
	};

	using workbench_interaction_intent = std::variant<
		select_workbench_file_intent,
		navigate_workbench_file_intent,
		set_workbench_drawer_intent,
		toggle_workbench_drawer_intent,
		set_workbench_modality_intent,
		set_workbench_line_numbers_intent,
		toggle_workbench_line_numbers_intent,
		set_workbench_one_pane_intent,
		toggle_workbench_one_pane_intent,
		set_workbench_updates_line_intent,
		toggle_workbench_updates_line_intent,
		toggle_workbench_drawers_intent>;

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
		toggle_line_numbers,
		toggle_repositories_drawer,
		toggle_one_pane,
		/// [[BPR-1130]]. The Updates disclosure; always offered, as a drawer
		/// toggle is.
		toggle_updates_line,
		/// [[BPR-1295]]. Hide or show every drawer; offered in Review only.
		toggle_drawers,
	};

	inline constexpr std::size_t workbench_command_count = 11U;

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
		workbench_drawer_state Repositories  = workbench_drawer_state::expanded;

		auto operator<=>(const workbench_drawer_delta&) const = default;
	};

	/// \brief [[BPR-1295]]. What showing the drawers would open.
	///
	/// \details The drawers the hide-and-show command remembered when it hid
	/// them, or Review's preset when it remembers none. The host publishes it
	/// when it changes, so the command can name what returns without
	/// deciding it.
	struct workbench_drawer_restore_delta
	{
		workbench_drawer_delta Drawers;

		auto operator<=>(const workbench_drawer_restore_delta&) const = default;
	};

	/// \brief [[BPR-1295]]. The drawer arrangement Review keeps.
	///
	/// \details Retained while another modality shows, and restored when
	/// Review is entered again: the three drawer states and, when the drawers
	/// were hidden, the set the command will show.
	struct workbench_review_arrangement
	{
		workbench_drawer_delta                Drawers;
		std::optional<workbench_drawer_delta> Hidden;

		auto operator<=>(const workbench_review_arrangement&) const = default;
	};

	/// \brief Describes the workbench line numbers delta record.
	struct workbench_line_numbers_delta
	{
		workbench_line_numbers State = workbench_line_numbers::shown;

		auto operator<=>(const workbench_line_numbers_delta&) const = default;
	};
	/// \brief Describes the workbench one pane delta record.
	struct workbench_one_pane_delta
	{
		workbench_one_pane State = workbench_one_pane::disabled;

		auto operator<=>(const workbench_one_pane_delta&) const = default;
	};

	/// \brief [[BPR-1130]]. Describes the Updates line's fold delta record.
	struct workbench_updates_line_delta
	{
		workbench_updates_line State = workbench_updates_line::folded;

		auto operator<=>(const workbench_updates_line_delta&) const = default;
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
		std::optional<workbench_line_numbers_delta>  LineNumbers;
		std::optional<workbench_one_pane_delta>      OnePane;
		std::optional<workbench_updates_line_delta>  UpdatesLine;
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
		workbench_drawer_state                     RepositoriesDrawer  = workbench_drawer_state::expanded;
		workbench_line_numbers                     LineNumbers         = workbench_line_numbers::shown;
		workbench_one_pane                         OnePane             = workbench_one_pane::disabled;
		change_navigation_policy                   NavigationPolicy    = change_navigation_policy::stop;
		workbench_modality                         Modality            = workbench_modality::review;
		/// [[BPR-1130]]. A new window starts folded.
		workbench_updates_line UpdatesLine = workbench_updates_line::folded;
		/// [[BPR-1295]]. The drawers the hide-and-show command hid, present
		/// only while every drawer is collapsed by it. Opening a drawer by
		/// hand, or choosing Review in Review, drops it.
		std::optional<workbench_drawer_delta> HiddenDrawers;
		/// [[BPR-1295]]. Review's arrangement, held while another modality
		/// shows. While Review shows, the live drawers are its arrangement.
		std::optional<workbench_review_arrangement> ReviewArrangement;
		/// [[BPR-1295]]. Whether Review has been entered in this window; until
		/// it has, entering it applies its preset.
		bool ReviewEntered = false;

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
	/// bounded change cursor still offers that step, so a widget can never offer
	/// a navigation gesture that would be rejected. A drawer command is always
	/// enabled: the room a drawer takes is the reviewer's to give back, and the
	/// one refusal, Overview keeping its drawers collapsed ([[BPR-0727]]),
	/// is explained by the gesture's reason rather than by a dead control,
	/// because the strip may not carry a disabled toggle. [[BPR-1295]]: the
	/// command that hides and shows every drawer is the exception the owner
	/// accepted; it acts only in Review and is disabled with its reason in
	/// Overview, Discuss and Conversations.
	[[nodiscard]] auto project_workbench_commands(
		const workbench_interaction_state& aState) noexcept
		-> workbench_command_delta;

	/// \brief [[BPR-1295]]. What showing the drawers would open now.
	///
	/// \details The remembered set while the command has the drawers hidden,
	/// otherwise Review's preset from the current drawers.
	[[nodiscard]] auto project_workbench_drawer_restore(
		const workbench_interaction_state& aState) noexcept
		-> workbench_drawer_restore_delta;

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
