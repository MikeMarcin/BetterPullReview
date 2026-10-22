#include "bpr/core/document_tabs.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(const bool aCondition, const char* const aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(1);
		}
	}

	template <typename T>
	auto identity(const std::uint8_t aByte, const std::uint8_t aSecond = 0U) -> T
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes[0]   = aByte;
		bytes[1]   = aSecond;
		return T{ bytes };
	}

	auto file(const std::uint8_t aByte) -> file_identity
	{
		return identity<file_identity>(aByte, 0xF1U);
	}

	auto oid(const char aDigit) -> git_commit_oid
	{
		return *parse_git_commit_oid(std::string(40U, aDigit));
	}

	/// \brief An exact pair of one pull request: its base at `aBase` and its
	/// head at `aHead`, in epoch ordinals.
	auto pair(const std::int64_t aBase, const std::int64_t aHead, const std::uint8_t aAuthority = 1U) -> comparison_pair
	{
		const auto scope       = identity<pull_request_authority_scope_id>(aAuthority);
		const auto incarnation = identity<epoch_catalog_incarnation_id>(2U);
		return {
			.Left  = { { scope, incarnation, comparison_anchor_kind::epoch_base, epoch_ordinal{ aBase }, { } }, oid(static_cast<char>('0' + aBase)), anchor_availability::available },
			.Right = { { scope, incarnation, comparison_anchor_kind::epoch_head, epoch_ordinal{ aHead }, { } }, oid(static_cast<char>('a' + aHead)), anchor_availability::available },
		};
	}

	auto line(const std::uint8_t aComparison, const std::uint32_t aLine) -> semantic_selection
	{
		const auto comparison = identity<file_comparison_digest>(aComparison, 0xC0U);
		return { .Comparison    = comparison,
				 .Kind          = semantic_selection_kind::line,
				 .PreferredLine = { .Comparison = comparison, .Side = diff_side::right, .Line = aLine },
				 .Segment       = std::nullopt,
				 .Group         = std::nullopt };
	}

	auto open(document_tabs_state& aState, const document_tab_intent& aIntent) -> document_tabs_output
	{
		return transition(aState, aIntent);
	}

	/// \brief Completes the load in force as ready, as the host would.
	void present(document_tabs_state& aState, const document_tabs_output& aOutput)
	{
		expect(aOutput.Load.has_value(), "fixture: the gesture loaded a view");
		expect(complete_view(aState, aOutput.Load->Request, document_tab_view_outcome::ready).Reason == document_tab_reason::none,
			   "fixture: the load in force completes");
	}

	auto active(const document_tabs_state& aState) -> const document_tab&
	{
		const auto* const tab = find_tab(aState, *aState.Active);
		expect(tab != nullptr, "fixture: the active tab exists");
		return *tab;
	}

	auto ids(const document_tabs_state& aState) -> std::vector<std::uint64_t>
	{
		auto result = std::vector<std::uint64_t>{ };
		for (const auto& tab : aState.Tabs) {
			result.push_back(tab.Id.value());
		}
		return result;
	}

	void test_nothing_opens_before_the_window_has_a_comparison()
	{
		auto state = document_tabs_state{ };
		expect(open(state, open_in_active_tab_intent{ file(1) }).Reason == document_tab_reason::no_window_comparison
				   && state.Tabs.empty(),
			   "a file click before the pull request's first comparison opens no tab");
		expect(open(state, select_tab_comparison_intent{ pair(1, 2) }).Reason == document_tab_reason::no_window_comparison,
			   "no tab comparison exists before the window's");
		auto invalid               = pair(1, 2);
		invalid.Right.Availability = anchor_availability::unavailable;
		expect(open(state, set_window_comparison_intent{ invalid }).Reason == document_tab_reason::invalid_comparison
				   && !state.WindowComparison,
			   "an unavailable anchor is never the window's comparison");
		const auto accepted = open(state, set_window_comparison_intent{ pair(1, 2) });
		expect(accepted.Changed && !accepted.Load && state.WindowComparison == pair(1, 2),
			   "the first accepted comparison becomes the window's, and loads nothing");
		expect(open(state, set_window_comparison_intent{ pair(1, 2) }) == document_tabs_output{ },
			   "the same comparison again changes nothing");
	}

	void test_clicking_a_file_reuses_the_active_tab()
	{
		auto state = document_tabs_state{ };
		static_cast<void>(open(state, set_window_comparison_intent{ pair(1, 2) }));
		const auto first = open(state, open_in_active_tab_intent{ file(1) });
		expect(first.Changed && first.Load && state.Tabs.size() == 1U && state.Active == state.Tabs[0].Id
				   && first.Load->Tab == state.Tabs[0].Id && first.Load->View.File == file(1)
				   && first.Load->View.Comparison == pair(1, 2) && !first.Load->View.Selection,
			   "opening the first file creates the initial tab on the window's comparison and loads it");
		expect(open(state, open_in_active_tab_intent{ file_identity{ } }).Reason == document_tab_reason::invalid_file,
			   "an invalid file identity is refused");
		present(state, first);
		expect(observe_reading_position(state, first.Load->Request, line(1, 40)).Changed, "the presented view reports its reading");
		const auto second = open(state, open_in_active_tab_intent{ file(2) });
		expect(state.Tabs.size() == 1U && active(state).View.File == file(2) && !active(state).View.Selection
				   && active(state).View.Comparison == pair(1, 2) && second.Load && second.Load->View.File == file(2),
			   "clicking another file reuses the active tab, keeps its comparison and starts at the file's initial position");
		present(state, second);
		expect(open(state, open_in_active_tab_intent{ file(2) }) == document_tabs_output{ },
			   "clicking the shown file again changes nothing");
	}

	void test_open_in_new_tab_is_a_background_tab_on_the_window_comparison()
	{
		auto state = document_tabs_state{ };
		static_cast<void>(open(state, set_window_comparison_intent{ pair(1, 2) }));
		const auto first = open(state, open_in_active_tab_intent{ file(1) });
		present(state, first);
		static_cast<void>(open(state, select_tab_comparison_intent{ pair(1, 3) }));
		const auto shown = state.Active;
		static_cast<void>(open(state, open_in_new_tab_intent{ file(2) }));
		const auto background = open(state, open_in_new_tab_intent{ file(3) });
		expect(background.Changed && !background.Load && state.Active == shown && state.Tabs.size() == 3U,
			   "Open in New Tab adds a tab without changing or reloading the active one");
		expect(state.Tabs[1].View.File == file(3) && state.Tabs[2].View.File == file(2),
			   "each new background tab sits just after the active tab");
		expect(state.Tabs[1].View.Comparison == *state.WindowComparison && state.Tabs[1].State == document_tab_view_state::idle,
			   "a new tab takes the window's comparison and stays idle until it is shown");
		expect(ids(state) == std::vector<std::uint64_t>{ 1U, 3U, 2U }, "tab identities are never reused and never reorder");
	}

	void test_the_active_tab_carries_the_window_comparison_only_while_it_shares_it()
	{
		auto state = document_tabs_state{ };
		static_cast<void>(open(state, set_window_comparison_intent{ pair(1, 2) }));
		present(state, open(state, open_in_active_tab_intent{ file(1) }));
		static_cast<void>(open(state, open_in_new_tab_intent{ file(1) }));
		const auto sibling = state.Tabs[1].Id;
		const auto moved   = open(state, select_tab_comparison_intent{ pair(1, 3) });
		expect(moved.Load && state.WindowComparison == pair(1, 3) && active(state).View.Comparison == pair(1, 3)
				   && !has_own_comparison(state, active(state)),
			   "a tab on the window's comparison carries the window with it");
		expect(find_tab(state, sibling)->View.Comparison == pair(1, 2) && has_own_comparison(state, *find_tab(state, sibling)),
			   "a sibling keeps its exact pair, and now says it has its own");
		present(state, open(state, activate_tab_intent{ sibling }));
		static_cast<void>(open(state, select_tab_comparison_intent{ pair(2, 3) }));
		expect(state.WindowComparison == pair(1, 3) && active(state).View.Comparison == pair(2, 3),
			   "a tab with its own pair changes alone");
		expect(open(state, select_tab_comparison_intent{ pair(1, 2, 7U) }).Reason == document_tab_reason::invalid_comparison,
			   "a pair from another pull request is refused");
	}

	void test_two_tabs_on_one_file_stay_distinct_and_exact()
	{
		auto state = document_tabs_state{ };
		static_cast<void>(open(state, set_window_comparison_intent{ pair(1, 3) }));
		present(state, open(state, open_in_active_tab_intent{ file(4) }));
		const auto first = *state.Active;
		present(state, open(state, duplicate_tab_intent{ first }));
		const auto second = *state.Active;
		present(state, open(state, select_tab_comparison_intent{ pair(1, 2) }));
		expect(first != second && find_tab(state, first)->View.File == find_tab(state, second)->View.File,
			   "two tabs name one file");
		expect(find_tab(state, first)->View.Comparison == pair(1, 3) && find_tab(state, second)->View.Comparison == pair(1, 2),
			   "each keeps its own exact pair");
		const auto beforeRefresh = state.Tabs;
		const auto shown         = state.Pending ? state.Pending->Request : state.Presented->Request;
		const auto refreshed     = reload_active(state);
		expect(refreshed.Load && refreshed.Load->Tab == second && refreshed.Load->View == beforeRefresh[1].View
				   && state.Tabs[0].View == beforeRefresh[0].View && state.Tabs[1].View == beforeRefresh[1].View,
			   "a refresh reloads the active tab's exact view and changes no tab's pair or file");
		expect(complete_view(state, shown, document_tab_view_outcome::ready).Reason == document_tab_reason::stale_view,
			   "the view shown before the refresh can no longer complete");
		present(state, refreshed);
		const auto back = open(state, activate_tab_intent{ first });
		expect(back.Load && back.Load->Tab == first && back.Load->View.Comparison == pair(1, 3),
			   "switching loads exactly the tab's own pair");
		const auto saved     = persisted(state);
		auto       restarted = document_tabs_state{ };
		const auto restored  = restore(restarted, *saved);
		expect(restored.Load && restarted.Tabs.size() == 2U && restarted.Tabs[0].View.Comparison == pair(1, 3)
				   && restarted.Tabs[1].View.Comparison == pair(1, 2) && restarted.Tabs[0].View.File == file(4)
				   && restarted.Tabs[1].View.File == file(4) && restarted.Active == restarted.Tabs[0].Id,
			   "the two views of one file survive a restart distinct and exact");
	}

	/// [[BPR-1478]]. The reviewer chose another file in Review, and the
	/// refresh landed before the active tab took it.
	void test_a_refresh_presents_a_file_chosen_after_the_tab_took_its_view()
	{
		auto state = document_tabs_state{ };
		static_cast<void>(open(state, set_window_comparison_intent{ pair(1, 2) }));
		const auto first = open(state, open_in_active_tab_intent{ file(1) });
		present(state, first);
		static_cast<void>(observe_reading_position(state, first.Load->Request, line(1, 12)));
		static_cast<void>(open(state, open_in_new_tab_intent{ file(3) }));
		const auto before = state.Tabs;
		const auto saved  = persisted(state);
		expect(observe_file_choice(state, file(2)) == document_tabs_output{ } && state.Tabs == before && !state.Pending
				   && persisted(state) == saved,
			   "a chosen file changes no tab, loads nothing and is not persisted until the tab takes it");
		const auto refreshed = reload_active(state);
		expect(refreshed.Load && refreshed.Load->Tab == before[0].Id && refreshed.Load->View.File == file(2)
				   && !refreshed.Load->View.Selection && refreshed.Load->View.Comparison == pair(1, 2)
				   && active(state).View == refreshed.Load->View && state.Tabs[1].View == before[1].View,
			   "a refresh presents the later choice at its initial position on the tab's pair, and no other tab changes");
		present(state, refreshed);
		const auto again = reload_active(state);
		expect(again.Load && again.Load->View == active(state).View && again.Load->View.File == file(2),
			   "the choice is taken once: the next refresh reloads the tab's own view");
		present(state, again);
		expect(observe_file_choice(state, file_identity{ }).Reason == document_tab_reason::invalid_file && !state.Chosen,
			   "an invalid file is no choice");
		static_cast<void>(observe_file_choice(state, file(1)));
		static_cast<void>(observe_file_choice(state, file(2)));
		const auto shown = reload_active(state);
		expect(!state.Chosen && shown.Load && shown.Load->View.File == file(2),
			   "choosing the shown file last leaves the tab's own view the latest");
		present(state, shown);
	}

	/// [[BPR-1478]]. A choice is retired by the tab taking it or by a later
	/// tab gesture; only the reviewer's latest gesture is presented.
	void test_a_file_choice_gives_way_to_the_tab_taking_it_and_to_later_tab_gestures()
	{
		auto state = document_tabs_state{ };
		static_cast<void>(open(state, set_window_comparison_intent{ pair(1, 2) }));
		present(state, open(state, open_in_active_tab_intent{ file(1) }));
		static_cast<void>(observe_file_choice(state, file(2)));
		present(state, open(state, open_in_active_tab_intent{ file(2) }));
		expect(!state.Chosen && active(state).View.File == file(2), "the tab taking the chosen file retires the choice");
		static_cast<void>(open(state, open_in_new_tab_intent{ file(3) }));
		static_cast<void>(observe_file_choice(state, file(4)));
		present(state, open(state, activate_tab_intent{ state.Tabs[1].Id }));
		const auto switched = reload_active(state);
		expect(!state.Chosen && switched.Load && switched.Load->View.File == file(3) && state.Tabs[0].View.File == file(2),
			   "a tab switch after the choice is later: the refresh presents the switched-to tab, and no tab takes the choice");
		present(state, switched);
		static_cast<void>(observe_file_choice(state, file(4)));
		static_cast<void>(open(state, close_tab_intent{ *state.Active }));
		expect(!state.Chosen && active(state).View.File == file(2), "closing the active tab retires its choice");
		static_cast<void>(observe_file_choice(state, file(4)));
		static_cast<void>(open(state, close_tab_intent{ *state.Active }));
		expect(!state.Chosen && state.Tabs.empty() && reload_active(state).Reason == document_tab_reason::no_active_tab,
			   "with no tab left there is no choice to present");
	}

	/// [[BPR-1478]]. A refresh that moves the head reloads the active tab for
	/// its new comparison; that load presents the choice too, so the choice
	/// does not outlive it.
	void test_a_comparison_accepted_for_the_active_tab_presents_the_choice()
	{
		auto state = document_tabs_state{ };
		static_cast<void>(open(state, set_window_comparison_intent{ pair(1, 2) }));
		const auto first = open(state, open_in_active_tab_intent{ file(1) });
		present(state, first);
		static_cast<void>(observe_reading_position(state, first.Load->Request, line(1, 12)));
		static_cast<void>(observe_file_choice(state, file(2)));
		const auto moved = open(state, select_tab_comparison_intent{ pair(1, 3) });
		expect(moved.Load && moved.Load->View.File == file(2) && moved.Load->View.Comparison == pair(1, 3) && !moved.Load->View.Selection
				   && !state.Chosen,
			   "the comparison's load presents the chosen file on the new pair");
		present(state, moved);
		const auto refreshed = reload_active(state);
		expect(refreshed.Load && refreshed.Load->View == active(state).View && refreshed.Load->View.File == file(2),
			   "a later refresh reloads the tab's own view: the choice was taken");
	}

	void test_duplicate_copies_the_exact_view_and_activates_it()
	{
		auto state = document_tabs_state{ };
		static_cast<void>(open(state, set_window_comparison_intent{ pair(1, 2) }));
		const auto first = open(state, open_in_active_tab_intent{ file(5) });
		present(state, first);
		static_cast<void>(observe_reading_position(state, first.Load->Request, line(5, 120)));
		static_cast<void>(open(state, open_in_new_tab_intent{ file(6) }));
		const auto source    = *state.Active;
		const auto duplicate = open(state, duplicate_tab_intent{ source });
		expect(duplicate.Load && state.Tabs.size() == 3U && state.Tabs[1].Id == *state.Active && state.Active != source
				   && state.Tabs[1].View == find_tab(state, source)->View && duplicate.Load->View.Selection == line(5, 120),
			   "Duplicate Tab opens and activates the exact file, pair and reading position beside its source");
		expect(open(state, duplicate_tab_intent{ document_tab_id{ 99U } }).Reason == document_tab_reason::unknown_tab,
			   "an unknown tab cannot be duplicated");
	}

	void test_closing_follows_the_neighbour_rule()
	{
		auto state = document_tabs_state{ };
		static_cast<void>(open(state, set_window_comparison_intent{ pair(1, 2) }));
		present(state, open(state, open_in_active_tab_intent{ file(1) }));
		static_cast<void>(open(state, open_in_new_tab_intent{ file(4) }));
		static_cast<void>(open(state, open_in_new_tab_intent{ file(3) }));
		static_cast<void>(open(state, open_in_new_tab_intent{ file(2) }));
		// Order: 1, 2, 3, 4 by file.
		present(state, open(state, activate_tab_intent{ state.Tabs[1].Id }));
		const auto closed = open(state, close_tab_intent{ *state.Active });
		expect(closed.Load && state.Tabs.size() == 3U && active(state).View.File == file(3),
			   "closing the active tab selects its next neighbour");
		present(state, closed);
		present(state, open(state, activate_tab_intent{ state.Tabs[2].Id }));
		const auto last = open(state, close_tab_intent{ *state.Active });
		expect(last.Load && active(state).View.File == file(3), "closing the last tab selects its previous neighbour");
		present(state, last);
		const auto inactive = open(state, close_tab_intent{ state.Tabs[0].Id });
		expect(inactive.Changed && !inactive.Load && active(state).View.File == file(3),
			   "closing an inactive tab leaves the active one as it is");
		const auto final = open(state, close_tab_intent{ *state.Active });
		expect(reload_active(state).Reason == document_tab_reason::no_active_tab, "a refresh with no tab loads nothing");
		expect(final.Changed && !final.Load && state.Tabs.empty() && !state.Active && state.WindowComparison == pair(1, 2),
			   "closing the final tab leaves the pull request open, with its comparison and no tab");
		expect(persisted(state)->Tabs.empty() && !persisted(state)->Active, "an empty collection persists with no active tab");
		expect(open(state, close_tab_intent{ document_tab_id{ 1U } }).Reason == document_tab_reason::unknown_tab,
			   "a closed tab cannot be closed again");
	}

	void test_close_others_activate_and_adjacent()
	{
		auto state = document_tabs_state{ };
		static_cast<void>(open(state, set_window_comparison_intent{ pair(1, 2) }));
		present(state, open(state, open_in_active_tab_intent{ file(1) }));
		static_cast<void>(open(state, open_in_new_tab_intent{ file(3) }));
		static_cast<void>(open(state, open_in_new_tab_intent{ file(2) }));
		expect(open(state, activate_tab_intent{ *state.Active }) == document_tabs_output{ }, "activating the active tab changes nothing");
		const auto next = open(state, activate_adjacent_tab_intent{ change_navigation_direction::next });
		expect(next.Load && active(state).View.File == file(2), "Ctrl+Tab activates the next tab");
		static_cast<void>(open(state, activate_adjacent_tab_intent{ change_navigation_direction::next }));
		const auto wrapped = open(state, activate_adjacent_tab_intent{ change_navigation_direction::next });
		expect(wrapped.Load && active(state).View.File == file(1), "Ctrl+Tab wraps from the last tab to the first");
		static_cast<void>(open(state, activate_adjacent_tab_intent{ change_navigation_direction::previous }));
		expect(active(state).View.File == file(3), "Ctrl+Shift+Tab wraps from the first tab to the last");
		const auto kept   = state.Tabs[1].Id;
		const auto others = open(state, close_other_tabs_intent{ kept });
		expect(others.Load && state.Tabs.size() == 1U && state.Active == kept, "Close Others keeps and activates the named tab");
		expect(open(state, activate_adjacent_tab_intent{ change_navigation_direction::next }) == document_tabs_output{ },
			   "one tab has no neighbour to move to");
		expect(open(state, close_other_tabs_intent{ kept }) == document_tabs_output{ }, "Close Others with one tab changes nothing");
	}

	void test_the_tab_menu_moves_pairs_between_a_tab_and_the_window()
	{
		auto state = document_tabs_state{ };
		static_cast<void>(open(state, set_window_comparison_intent{ pair(1, 2) }));
		present(state, open(state, open_in_active_tab_intent{ file(1) }));
		static_cast<void>(open(state, open_in_new_tab_intent{ file(2) }));
		const auto other = state.Tabs[1].Id;
		present(state, open(state, activate_tab_intent{ other }));
		present(state, open(state, select_tab_comparison_intent{ pair(2, 3) }));
		expect(state.WindowComparison == pair(2, 3), "fixture: the active tab carried the window to 2..3");
		const auto first = state.Tabs[0].Id;
		expect(has_own_comparison(state, *find_tab(state, first)), "fixture: the first tab kept 1..2 and has its own pair");
		const auto made = open(state, make_tab_comparison_window_intent{ first });
		expect(made.Changed && !made.Load && state.WindowComparison == pair(1, 2)
				   && find_tab(state, other)->View.Comparison == pair(2, 3),
			   "making a tab's pair the window's changes only what new tabs take");
		static_cast<void>(open(state, open_in_new_tab_intent{ file(3) }));
		expect(state.Tabs[2].View.Comparison == pair(1, 2), "the next new tab takes the pair made the window's");
		const auto returned = open(state, return_tab_to_window_comparison_intent{ other });
		expect(returned.Load && active(state).View.Comparison == pair(1, 2) && !has_own_comparison(state, active(state)),
			   "returning the active tab to the window's comparison reloads it on that pair");
		expect(open(state, return_tab_to_window_comparison_intent{ other }) == document_tabs_output{ },
			   "a tab already on the window's comparison is unchanged");
	}

	void test_late_work_cannot_reach_another_tab()
	{
		auto state = document_tabs_state{ };
		static_cast<void>(open(state, set_window_comparison_intent{ pair(1, 2) }));
		const auto a = open(state, open_in_active_tab_intent{ file(1) });
		static_cast<void>(open(state, open_in_new_tab_intent{ file(2) }));
		const auto b = open(state, activate_tab_intent{ state.Tabs[1].Id });
		expect(complete_view(state, a.Load->Request, document_tab_view_outcome::ready).Reason == document_tab_reason::stale_view
				   && state.Tabs[0].State == document_tab_view_state::idle && state.Tabs[1].State == document_tab_view_state::loading,
			   "tab A's late completion is refused while B loads, and neither tab changes");
		expect(observe_reading_position(state, a.Load->Request, line(1, 9)).Reason == document_tab_reason::stale_view
				   && !state.Tabs[0].View.Selection && !state.Tabs[1].View.Selection,
			   "A's late reading reaches no tab");
		expect(observe_reading_position(state, b.Load->Request, line(2, 9)).Reason == document_tab_reason::stale_view,
			   "a reading before the load completes is refused");
		const auto again = open(state, activate_tab_intent{ state.Tabs[0].Id });
		expect(complete_view(state, b.Load->Request, document_tab_view_outcome::ready).Reason == document_tab_reason::stale_view,
			   "B's completion after switching back to A is refused");
		expect(complete_view(state, again.Load->Request, document_tab_view_outcome::ready).Reason == document_tab_reason::none
				   && state.Tabs[0].State == document_tab_view_state::ready,
			   "A's own newer load completes");
		expect(complete_view(state, again.Load->Request, document_tab_view_outcome::ready).Reason == document_tab_reason::stale_view,
			   "a duplicate completion is refused");
		const auto closing = open(state, activate_tab_intent{ state.Tabs[1].Id });
		static_cast<void>(open(state, close_tab_intent{ state.Tabs[1].Id }));
		expect(complete_view(state, closing.Load->Request, document_tab_view_outcome::ready).Reason == document_tab_reason::stale_view,
			   "a closed loading tab's completion reaches nothing");
	}

	void test_an_unavailable_view_says_so_and_is_never_retargeted()
	{
		auto state = document_tabs_state{ };
		static_cast<void>(open(state, set_window_comparison_intent{ pair(1, 2) }));
		const auto load   = open(state, open_in_active_tab_intent{ file(1) });
		const auto before = active(state).View;
		const auto result = complete_view(state, load.Load->Request, document_tab_view_outcome::unavailable);
		expect(result.Changed && active(state).State == document_tab_view_state::unavailable && active(state).View == before
				   && state.WindowComparison == pair(1, 2),
			   "an unavailable exact view stays on its tab with its pair and file");
		expect(observe_reading_position(state, load.Load->Request, line(1, 3)).Reason == document_tab_reason::stale_view,
			   "an unavailable view reports no reading");
		const auto retry = open(state, open_in_active_tab_intent{ file(1) });
		expect(retry.Load && retry.Load->View == before, "clicking the file again retries the same exact view");
	}

	void test_the_reading_position_is_the_presented_views()
	{
		auto state = document_tabs_state{ };
		static_cast<void>(open(state, set_window_comparison_intent{ pair(1, 2) }));
		const auto load = open(state, open_in_active_tab_intent{ file(1) });
		present(state, load);
		expect(observe_reading_position(state, load.Load->Request, line(1, 12)).Changed && active(state).View.Selection == line(1, 12),
			   "the presented tab adopts its reading position");
		expect(observe_reading_position(state, load.Load->Request, line(1, 12)) == document_tabs_output{ },
			   "the same reading changes nothing");
		auto broken                     = line(1, 13);
		broken.PreferredLine.Comparison = identity<file_comparison_digest>(9U, 9U);
		expect(observe_reading_position(state, load.Load->Request, broken).Reason == document_tab_reason::invalid_position
				   && active(state).View.Selection == line(1, 12),
			   "an incoherent reading is refused");
		expect(observe_reading_position(state, load.Load->Request, std::nullopt).Changed && !active(state).View.Selection,
			   "a cleared selection is a reading too");
	}

	void test_the_collection_is_bounded()
	{
		auto state = document_tabs_state{ };
		static_cast<void>(open(state, set_window_comparison_intent{ pair(1, 2) }));
		present(state, open(state, open_in_active_tab_intent{ file(1) }));
		for (std::uint8_t index = 2U; state.Tabs.size() < maximum_document_tabs; ++index) {
			expect(open(state, open_in_new_tab_intent{ file(index) }).Changed, "fixture: tabs open up to the bound");
		}
		expect(open(state, open_in_new_tab_intent{ file(200) }).Reason == document_tab_reason::tab_limit
				   && state.Tabs.size() == maximum_document_tabs,
			   "a tab past the bound is refused");
		expect(open(state, duplicate_tab_intent{ *state.Active }).Reason == document_tab_reason::tab_limit,
			   "a duplicate past the bound is refused");
		expect(open(state, open_in_active_tab_intent{ file(201) }).Load.has_value(), "the active tab still takes another file");
	}

	void test_restoration_keeps_order_active_pairs_positions_and_the_window()
	{
		auto state = document_tabs_state{ };
		static_cast<void>(open(state, set_window_comparison_intent{ pair(1, 2) }));
		const auto first = open(state, open_in_active_tab_intent{ file(1) });
		present(state, first);
		static_cast<void>(observe_reading_position(state, first.Load->Request, line(1, 30)));
		static_cast<void>(open(state, open_in_new_tab_intent{ file(3) }));
		static_cast<void>(open(state, open_in_new_tab_intent{ file(2) }));
		present(state, open(state, activate_tab_intent{ state.Tabs[2].Id }));
		present(state, open(state, select_tab_comparison_intent{ pair(2, 3) }));
		static_cast<void>(open(state, make_tab_comparison_window_intent{ state.Tabs[0].Id }));
		const auto saved = persisted(state);
		expect(saved && valid(*saved) && saved->Tabs.size() == 3U && saved->Active == 2U && saved->WindowComparison == pair(1, 2)
				   && saved->Tabs[0].Selection == line(1, 30) && saved->Tabs[2].Comparison == pair(2, 3),
			   "the persisted collection holds order, active tab, pairs, readings and the window's comparison");
		auto later          = document_tabs_state{ };
		later.LastTab       = 40U;
		later.LastRequest   = 70U;
		const auto restored = restore(later, *saved);
		expect(restored.Load && restored.Load->Tab == later.Tabs[2].Id && restored.Load->View == saved->Tabs[2]
				   && later.Tabs[0].View == saved->Tabs[0] && later.Tabs[1].View == saved->Tabs[1],
			   "restoration keeps the order and loads the active tab's exact view");
		expect(later.Tabs[0].Id.value() == 41U && restored.Load->Request.value() == 71U,
			   "restored tabs and their loads take fresh identities");
		expect(persisted(later) == saved, "a restored collection persists as it was saved");
		auto broken   = *saved;
		broken.Active = 3U;
		expect(restore(later, broken).Reason == document_tab_reason::invalid_restoration, "an active index past the tabs is refused");
		broken        = *saved;
		broken.Active = std::nullopt;
		expect(restore(later, broken).Reason == document_tab_reason::invalid_restoration, "tabs without an active one are refused");
		broken                    = *saved;
		broken.Tabs[1].Comparison = pair(1, 2, 8U);
		expect(restore(later, broken).Reason == document_tab_reason::invalid_restoration,
			   "a tab from another pull request is refused");
		broken              = *saved;
		broken.Tabs[1].File = std::nullopt;
		expect(restore(later, broken).Reason == document_tab_reason::invalid_restoration, "a tab without a file is refused");
		expect(later.Tabs.size() == 3U && later.Tabs[0].Id.value() == 41U, "a refused restoration leaves the state as it was");
		expect(!persisted(document_tabs_state{ }), "nothing persists before the window has a comparison");
	}
	void test_completion_and_reading_are_one_transition()
	{
		auto state = document_tabs_state{ };
		static_cast<void>(transition(state, set_window_comparison_intent{ pair(1, 2) }));
		const auto opened  = transition(state, open_in_active_tab_intent{ file(1) });
		const auto request = opened.Load->Request;
		const auto before  = state;
		expect(complete_view_with_reading_position(state, request_id{ request.value() + 1U }, line(1, 1)).Reason == document_tab_reason::stale_view && state == before, "stale combined completion changes nothing");
		auto invalid                     = line(1, 1);
		invalid.PreferredLine.Comparison = { };
		expect(complete_view_with_reading_position(state, request, invalid).Reason == document_tab_reason::invalid_position && state == before, "invalid reading cannot partly complete the load");
		const auto accepted = complete_view_with_reading_position(state, request, line(1, 1));
		expect(accepted.Changed && accepted.Reason == document_tab_reason::none && !state.Pending && state.Presented && active(state).View.Selection == line(1, 1), "ready completion includes the chosen reading");
		const auto refreshed = reload_active(state);
		expect(refreshed.Load && refreshed.Load->View.Selection == line(1, 1), "a following refresh inherits the chosen reading");
	}

} // namespace

auto main() -> int
{
	test_completion_and_reading_are_one_transition();
	test_nothing_opens_before_the_window_has_a_comparison();
	test_clicking_a_file_reuses_the_active_tab();
	test_open_in_new_tab_is_a_background_tab_on_the_window_comparison();
	test_the_active_tab_carries_the_window_comparison_only_while_it_shares_it();
	test_two_tabs_on_one_file_stay_distinct_and_exact();
	test_a_refresh_presents_a_file_chosen_after_the_tab_took_its_view();
	test_a_file_choice_gives_way_to_the_tab_taking_it_and_to_later_tab_gestures();
	test_a_comparison_accepted_for_the_active_tab_presents_the_choice();
	test_duplicate_copies_the_exact_view_and_activates_it();
	test_closing_follows_the_neighbour_rule();
	test_close_others_activate_and_adjacent();
	test_the_tab_menu_moves_pairs_between_a_tab_and_the_window();
	test_late_work_cannot_reach_another_tab();
	test_an_unavailable_view_says_so_and_is_never_retargeted();
	test_the_reading_position_is_the_presented_views();
	test_the_collection_is_bounded();
	test_restoration_keeps_order_active_pairs_positions_and_the_window();
	std::cout << "All document tab core tests passed\n";
	return 0;
}
