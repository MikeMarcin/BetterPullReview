#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_DOCUMENT_TABS_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_DOCUMENT_TABS_HPP

#include "bpr/core/diff_semantics.hpp"
#include "bpr/core/model.hpp"
#include "bpr/core/window_review_restoration.hpp"

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

namespace bpr::core {
	struct document_tab_id_tag;
	/// \brief [[BPR-1131]]. One document tab, unique within its window's
	/// collection for the life of that collection.
	using document_tab_id = strong_id<document_tab_id_tag>;

	/// \brief What the window knows about a tab's exact view.
	///
	/// \details `idle` is a tab that is not shown: a background tab, or one
	/// the reviewer moved away from. Only the active tab loads. `unavailable`
	/// is an exact view the host could not present, a removed file or a
	/// pruned anchor; it stays on the tab and is never retargeted.
	enum class document_tab_view_state : std::uint8_t {
		idle,
		loading,
		ready,
		unavailable,
	};

	/// \brief One document tab: an identity and an exact view.
	///
	/// \details The view is the same record restoration keeps for a window's
	/// single reading: an exact comparison pair, a file and a reading position.
	/// A tab always names a file.
	struct document_tab
	{
		document_tab_id         Id;
		window_review_position  View;
		document_tab_view_state State = document_tab_view_state::idle;

		auto operator==(const document_tab&) const -> bool = default;
	};

	/// \brief The load the host performs to present the active tab.
	///
	/// \details `Request` is fresh for every load. The host repeats it with
	/// the load's outcome and with any reading position it observes; the core
	/// accepts either only while it names the load in force, so a late answer
	/// for another tab, or an earlier load of this one, is refused.
	struct document_tab_view_request
	{
		document_tab_id        Tab;
		request_id             Request;
		window_review_position View;

		auto operator==(const document_tab_view_request&) const -> bool = default;
	};

	/// \brief One window's document tabs for the pull request it shows.
	///
	/// \details The window's comparison is the exact pair new tabs take. It
	/// is absent until the host reports the pull request's first accepted
	/// comparison, and no tab opens before it. `Active` names a tab in `Tabs`
	/// or is absent exactly when there is no tab. `Pending` is the one load in
	/// force; `Presented` is the load whose view the window shows, which is
	/// the one whose reading position the core adopts. `Chosen` is a file the
	/// reviewer chose in Review that the active tab has not taken yet
	/// ([[BPR-1478]]); it is never persisted.
	struct document_tabs_state
	{
		std::optional<comparison_pair>           WindowComparison;
		std::vector<document_tab>                Tabs;
		std::optional<document_tab_id>           Active;
		std::optional<document_tab_view_request> Pending;
		std::optional<document_tab_view_request> Presented;
		std::optional<file_identity>             Chosen;
		std::uint64_t                            LastTab     = 0U;
		std::uint64_t                            LastRequest = 0U;

		auto operator==(const document_tabs_state&) const -> bool = default;
	};

	/// \brief Clicking a file: the active tab shows it, or the first tab opens.
	struct open_in_active_tab_intent
	{
		file_identity File;

		auto operator==(const open_in_active_tab_intent&) const -> bool = default;
	};

	/// \brief Cmd-click (Ctrl-click) or Open in New Tab: a background tab
	/// after the active one, on the window's comparison.
	struct open_in_new_tab_intent
	{
		file_identity File;

		auto operator==(const open_in_new_tab_intent&) const -> bool = default;
	};

	/// \brief Duplicate Tab: the exact view, in a new active tab beside it.
	struct duplicate_tab_intent
	{
		document_tab_id Tab;

		auto operator==(const duplicate_tab_intent&) const -> bool = default;
	};

	/// \brief Close Tab, with the neighbour rule.
	struct close_tab_intent
	{
		document_tab_id Tab;

		auto operator==(const close_tab_intent&) const -> bool = default;
	};

	/// \brief Close Others: the named tab stays, active.
	struct close_other_tabs_intent
	{
		document_tab_id Tab;

		auto operator==(const close_other_tabs_intent&) const -> bool = default;
	};

	/// \brief Makes a tab the active tab.
	struct activate_tab_intent
	{
		document_tab_id Tab;

		auto operator==(const activate_tab_intent&) const -> bool = default;
	};

	/// \brief Ctrl+Tab and Ctrl+Shift+Tab: the next or previous tab, wrapping.
	struct activate_adjacent_tab_intent
	{
		change_navigation_direction Direction = change_navigation_direction::next;

		auto operator==(const activate_adjacent_tab_intent&) const -> bool = default;
	};

	/// \brief The tab's menu: the tab takes the window's comparison.
	struct return_tab_to_window_comparison_intent
	{
		document_tab_id Tab;

		auto operator==(const return_tab_to_window_comparison_intent&) const -> bool = default;
	};

	/// \brief The tab's menu: the tab's pair becomes the one new tabs take.
	struct make_tab_comparison_window_intent
	{
		document_tab_id Tab;

		auto operator==(const make_tab_comparison_window_intent&) const -> bool = default;
	};

	/// \brief A comparison accepted for the active tab, from the Updates line
	/// or the comparison's menu.
	///
	/// \details The active tab takes the pair. A tab that was on the window's
	/// comparison carries the window with it, so a reviewer with one tab never
	/// sees their own tab labelled as different from the window; a tab with its
	/// own pair changes alone. Sibling tabs keep their exact pairs either way.
	struct select_tab_comparison_intent
	{
		comparison_pair Comparison;

		auto operator==(const select_tab_comparison_intent&) const -> bool = default;
	};

	/// \brief A comparison accepted while no tab is active: the pull request's
	/// first comparison, or one chosen in Overview or Discuss.
	struct set_window_comparison_intent
	{
		comparison_pair Comparison;

		auto operator==(const set_window_comparison_intent&) const -> bool = default;
	};

	using document_tab_intent = std::variant<
		open_in_active_tab_intent,
		open_in_new_tab_intent,
		duplicate_tab_intent,
		close_tab_intent,
		close_other_tabs_intent,
		activate_tab_intent,
		activate_adjacent_tab_intent,
		return_tab_to_window_comparison_intent,
		make_tab_comparison_window_intent,
		select_tab_comparison_intent,
		set_window_comparison_intent>;

	/// \brief Why a tab gesture or observation changed nothing.
	enum class document_tab_reason : std::uint8_t {
		none,
		no_window_comparison,
		invalid_file,
		invalid_comparison,
		unknown_tab,
		tab_limit,
		no_active_tab,
		stale_view,
		invalid_position,
		invalid_restoration,
	};

	/// \brief How the host's load of one tab's exact view ended.
	enum class document_tab_view_outcome : std::uint8_t {
		ready,
		unavailable,
	};

	/// \brief What one transition asks of the host.
	///
	/// \details `Changed` is set when the collection, the active tab, the
	/// window's comparison or a tab's view or state changed, so the host
	/// republishes and persists. `Load` is the active tab's exact view to
	/// present; the host answers it with `complete_view`.
	struct document_tabs_output
	{
		bool                                     Changed = false;
		std::optional<document_tab_view_request> Load;
		document_tab_reason                      Reason = document_tab_reason::none;

		auto operator==(const document_tabs_output&) const -> bool = default;
	};

	/// \brief Applies one tab gesture in the functional core.
	[[nodiscard]] auto transition(
		document_tabs_state&       aState,
		const document_tab_intent& aIntent) -> document_tabs_output;

	/// \brief Adopts the host's outcome for one load.
	///
	/// \details Accepted only for the load in force, in constant time; any
	/// other request is `stale_view` and changes nothing. An unavailable view
	/// stays on its tab with its exact pair and file.
	[[nodiscard]] auto complete_view(
		document_tabs_state&      aState,
		request_id                aRequest,
		document_tab_view_outcome aOutcome) -> document_tabs_output;

	/// \brief Adopts the reading position the presented view reports.
	[[nodiscard]] auto observe_reading_position(
		document_tabs_state&                     aState,
		request_id                               aRequest,
		const std::optional<semantic_selection>& aSelection) -> document_tabs_output;

	/// \brief Completes the current load with its already-chosen reading atomically.
	/// \details Neither transition is adopted if the request or reading is refused.
	[[nodiscard]] auto complete_view_with_reading_position(
		document_tabs_state& aState, request_id aRequest,
		const std::optional<semantic_selection>& aSelection) -> document_tabs_output;

	/// \brief [[BPR-1478]]. Adopts a file the reviewer chose in Review, before
	/// the active tab takes it.
	///
	/// \details The host reflects a chosen file into the active tab with
	/// `open_in_active_tab_intent` once the comparison it opens under is
	/// accepted. A refresh can land in between and retire the window's
	/// selection, so the choice is kept here until the tab takes it: the
	/// active tab's next load, for a refresh or for a comparison accepted for
	/// it, presents the chosen file at its initial position. Nothing loads and
	/// no tab changes now. A choice of the file the active tab already shows
	/// clears it, and so does switching, closing or restoring tabs.
	[[nodiscard]] auto observe_file_choice(
		document_tabs_state& aState,
		const file_identity& aFile) -> document_tabs_output;

	/// \brief Presents the active tab's exact view again, after the pull
	/// request refreshed.
	///
	/// \details A refresh never changes a tab's pair: the active tab loads
	/// under a fresh request, which retires the load before it, and every
	/// other tab keeps its view untouched. The active tab loads the same exact
	/// view, unless the reviewer chose another file it has not taken yet
	/// ([[BPR-1478]]). That choice is the later one, so the tab takes it at the
	/// file's initial position, as clicking it does.
	[[nodiscard]] auto reload_active(document_tabs_state& aState) -> document_tabs_output;

	/// \brief Replaces the state with a persisted collection, minting fresh
	/// identities in the persisted order and loading its active tab.
	[[nodiscard]] auto restore(
		document_tabs_state&      aState,
		const window_review_tabs& aTabs) -> document_tabs_output;

	/// \brief The collection as the restoration store keeps it; absent before
	/// the window has a comparison.
	[[nodiscard]] auto persisted(const document_tabs_state& aState)
		-> std::optional<window_review_tabs>;

	/// \brief Whether a tab's pair differs from the window's, which its label
	/// says.
	[[nodiscard]] auto has_own_comparison(
		const document_tabs_state& aState,
		const document_tab&        aTab) noexcept -> bool;

	/// \brief The tab with this identity, if the collection holds it.
	[[nodiscard]] auto find_tab(
		const document_tabs_state& aState,
		document_tab_id            aTab) noexcept -> const document_tab*;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_DOCUMENT_TABS_HPP
