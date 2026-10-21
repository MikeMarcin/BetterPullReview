#include "bpr/application/document_tab_host.hpp"
#include "bpr/application/manual_executor.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
	using namespace bpr;
	using namespace bpr::application;

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

	auto file(const std::uint8_t aByte) -> core::file_identity
	{
		return identity<core::file_identity>(aByte, 0xF1U);
	}

	auto pair(const std::int64_t aBase, const std::int64_t aHead) -> core::comparison_pair
	{
		const auto scope       = identity<core::pull_request_authority_scope_id>(1U);
		const auto incarnation = identity<core::epoch_catalog_incarnation_id>(2U);
		return {
			.Left  = { { scope, incarnation, core::comparison_anchor_kind::epoch_base, core::epoch_ordinal{ aBase }, { } }, *core::parse_git_commit_oid(std::string(40U, static_cast<char>('0' + aBase))), core::anchor_availability::available },
			.Right = { { scope, incarnation, core::comparison_anchor_kind::epoch_head, core::epoch_ordinal{ aHead }, { } }, *core::parse_git_commit_oid(std::string(40U, static_cast<char>('a' + aHead))), core::anchor_availability::available },
		};
	}

	auto line(const std::uint8_t aComparison, const std::uint32_t aLine) -> core::semantic_selection
	{
		const auto comparison = identity<core::file_comparison_digest>(aComparison, 0xC0U);
		return { .Comparison    = comparison,
				 .Kind          = core::semantic_selection_kind::line,
				 .PreferredLine = { .Comparison = comparison, .Side = core::diff_side::right, .Line = aLine },
				 .Segment       = std::nullopt,
				 .Group         = std::nullopt };
	}

	/// \brief Records every load the host asks for; answers only when told.
	struct views final : document_tab_view_port
	{
		std::vector<core::document_tab_view_request> Loads;
		void                                         load(const core::document_tab_view_request& aRequest) override { Loads.push_back(aRequest); }
	};

	struct projections final : document_tabs_projection_port
	{
		std::vector<document_tabs_projection> Published;
		void                                  publish(const document_tabs_projection& aProjection) override { Published.push_back(aProjection); }
	};

	struct fixture
	{
		manual_executor                       Lane;
		views                                 Views;
		projections                           Output;
		std::vector<core::window_review_tabs> Persisted;
		document_tab_host                     Host{ Lane, Views, Output, [this](const core::window_review_tabs& aTabs) { Persisted.push_back(aTabs); } };

		void open_window(const core::comparison_pair& aPair)
		{
			Host.apply(core::set_window_comparison_intent{ aPair });
		}
		void answer(const core::document_tab_view_request& aLoad, const core::document_tab_view_outcome aOutcome = core::document_tab_view_outcome::ready)
		{
			Host.complete(aLoad.Request, aOutcome);
			Lane.drain();
		}
		[[nodiscard]] auto entry(const core::document_tab_id aTab) const -> document_tab_entry
		{
			for (const auto& tab : Output.Published.back().Tabs) {
				if (tab.Id == aTab) {
					return tab;
				}
			}
			expect(false, "fixture: the tab is published");
			return { };
		}
	};

	// Late work cannot reach another tab: A/B/A completions delivered out of
	// order land only on the load that asked for them.
	void test_completions_route_to_the_tab_that_asked()
	{
		fixture f;
		f.open_window(pair(1, 2));
		f.Host.apply(core::open_in_active_tab_intent{ file(1) });
		f.Host.apply(core::open_in_new_tab_intent{ file(2) });
		const auto a = f.Host.state().Tabs[0].Id;
		const auto b = f.Host.state().Tabs[1].Id;
		f.Host.apply(core::activate_tab_intent{ b });
		f.Host.apply(core::activate_tab_intent{ a });
		expect(f.Views.Loads.size() == 3U && f.Views.Loads[0].Tab == a && f.Views.Loads[1].Tab == b && f.Views.Loads[2].Tab == a,
			   "each activation asks the view port for that tab's exact view");
		f.Host.complete(f.Views.Loads[1].Request, core::document_tab_view_outcome::ready);
		expect(f.Host.state().Tabs[1].State == core::document_tab_view_state::idle, "a completion never runs inline on its caller");
		f.Lane.drain();
		expect(f.Host.stale_terminals() == 1U && f.entry(b).State == core::document_tab_view_state::idle
				   && f.entry(a).State == core::document_tab_view_state::loading,
			   "B's late completion is refused and neither tab changes");
		f.answer(f.Views.Loads[0]);
		expect(f.Host.stale_terminals() == 2U && f.entry(a).State == core::document_tab_view_state::loading,
			   "A's first, superseded load cannot complete A's current one");
		f.answer(f.Views.Loads[2]);
		expect(f.Host.stale_terminals() == 2U && f.entry(a).State == core::document_tab_view_state::ready && f.entry(a).Active,
			   "A's own load completes A");
		f.answer(f.Views.Loads[2]);
		expect(f.Host.stale_terminals() == 3U, "a duplicate completion is refused");
		f.Host.observe_reading_position(f.Views.Loads[1].Request, line(2, 5));
		f.Lane.drain();
		expect(f.Host.stale_terminals() == 4U && !f.Host.state().Tabs[1].View.Selection && !f.Host.state().Tabs[0].View.Selection,
			   "B's reading reaches no tab while A is shown");
		expect(f.Host.is_quiescent(), "every posted terminal ran");
	}

	void test_a_closed_loading_tab_publishes_nothing()
	{
		fixture f;
		f.open_window(pair(1, 2));
		f.Host.apply(core::open_in_active_tab_intent{ file(1) });
		f.answer(f.Views.Loads[0]);
		f.Host.apply(core::open_in_new_tab_intent{ file(2) });
		const auto loading = f.Host.state().Tabs[1].Id;
		f.Host.apply(core::activate_tab_intent{ loading });
		const auto orphan = f.Views.Loads.back();
		f.Host.apply(core::close_tab_intent{ loading });
		const auto neighbour = f.Views.Loads.back();
		expect(neighbour.Tab == f.Host.state().Tabs[0].Id && neighbour.View.File == file(1), "closing the loading tab loads its neighbour");
		f.answer(orphan);
		expect(f.Host.stale_terminals() == 1U && f.Host.state().Tabs.size() == 1U
				   && f.Host.state().Tabs[0].State == core::document_tab_view_state::loading,
			   "the closed tab's completion reaches nothing, not even the neighbour that replaced it");
		f.answer(neighbour, core::document_tab_view_outcome::unavailable);
		expect(f.entry(neighbour.Tab).State == core::document_tab_view_state::unavailable && f.entry(neighbour.Tab).View == neighbour.View,
			   "an unavailable view is published on its own tab, exact, and not retargeted");
	}

	// Two tabs on one file with different pairs stay distinct and exact through
	// refresh and restart.
	void test_two_views_of_one_file_through_refresh_and_restart()
	{
		fixture f;
		f.open_window(pair(1, 3));
		f.Host.apply(core::open_in_active_tab_intent{ file(7) });
		f.answer(f.Views.Loads.back());
		f.Host.apply(core::duplicate_tab_intent{ *f.Host.state().Active });
		f.answer(f.Views.Loads.back());
		f.Host.apply(core::select_tab_comparison_intent{ pair(1, 2) });
		f.answer(f.Views.Loads.back());
		f.Host.observe_reading_position(f.Views.Loads.back().Request, line(7, 44));
		f.Lane.drain();
		const auto first  = f.Host.state().Tabs[0].Id;
		const auto second = f.Host.state().Tabs[1].Id;
		expect(f.entry(first).View.Comparison == pair(1, 3) && f.entry(second).View.Comparison == pair(1, 2)
				   && f.entry(first).View.File == f.entry(second).View.File && f.entry(first).OwnComparison
				   && !f.entry(second).OwnComparison && f.entry(second).View.Selection == line(7, 44),
			   "one file in two tabs, each with its own exact pair; the one the window moved away from says so");
		const auto shown = f.Views.Loads.back();
		f.Host.refresh();
		const auto again = f.Views.Loads.back();
		expect(again.Tab == second && again.View.Comparison == shown.View.Comparison && again.View.File == shown.View.File
				   && again.View.Selection == line(7, 44) && again.Request != shown.Request,
			   "a refresh presents the active tab's exact view and reading again, under a fresh request");
		f.answer(shown);
		expect(f.Host.stale_terminals() == 1U, "the pre-refresh load cannot complete the refreshed one");
		f.answer(again);
		expect(f.entry(first).View.Comparison == pair(1, 3) && f.entry(second).View.Comparison == pair(1, 2),
			   "the refresh left both pairs exact");
		expect(!f.Persisted.empty(), "every change reached the restoration owner");
		const auto saved = f.Persisted.back();
		expect(saved.Tabs.size() == 2U && saved.Active == 1U && saved.WindowComparison == pair(1, 2)
				   && saved.Tabs[1].Selection == line(7, 44),
			   "the last persisted collection is the window's current tabs, reading included");

		fixture restarted;
		restarted.Host.restore(saved);
		expect(restarted.Views.Loads.size() == 1U && restarted.Views.Loads[0].View == saved.Tabs[1]
				   && restarted.Views.Loads[0].View.Comparison == pair(1, 2),
			   "after a restart the active tab loads its own exact pair and reading");
		restarted.answer(restarted.Views.Loads[0]);
		const auto& tabs = restarted.Output.Published.back().Tabs;
		expect(tabs.size() == 2U && tabs[0].View == saved.Tabs[0] && tabs[1].View == saved.Tabs[1] && tabs[1].Active
				   && tabs[0].OwnComparison && !tabs[1].OwnComparison && tabs[0].View.File == tabs[1].View.File,
			   "the two views of one file come back distinct, exact and in order");
	}

	void test_refusals_are_published_and_change_nothing()
	{
		fixture f;
		f.Host.apply(core::open_in_active_tab_intent{ file(1) });
		expect(f.Host.last_reason() == core::document_tab_reason::no_window_comparison && f.Views.Loads.empty()
				   && f.Persisted.empty() && f.Output.Published.back().Reason == core::document_tab_reason::no_window_comparison,
			   "a refused gesture is published with its reason and loads and persists nothing");
		f.open_window(pair(1, 2));
		f.Host.apply(core::close_tab_intent{ core::document_tab_id{ 9U } });
		expect(f.Host.last_reason() == core::document_tab_reason::unknown_tab && f.Views.Loads.empty(), "an unknown tab is refused");
	}

	void test_lane_reading_is_persisted_before_immediate_shutdown()
	{
		fixture f;
		f.open_window(pair(1, 2));
		f.Host.apply(core::open_in_active_tab_intent{ file(1) });
		const auto request = f.Views.Loads.back().Request;
		f.Host.complete_on_lane(request, core::document_tab_view_outcome::ready);
		f.Host.observe_reading_position_on_lane(request, line(1, 9));
		f.Host.shutdown();
		expect(f.Host.is_quiescent() && f.Persisted.back().Tabs[0].Selection == line(1, 9),
			   "a lane's accepted line is durable before Quit without another event turn");
		f.Host.observe_reading_position_on_lane(request, line(1, 10));
		f.Lane.drain();
		expect(f.Persisted.back().Tabs[0].Selection == line(1, 9), "shutdown refuses later lane readings");
	}

	void test_leaving_the_pull_request_retires_its_work()
	{
		fixture f;
		f.open_window(pair(1, 2));
		f.Host.apply(core::open_in_active_tab_intent{ file(1) });
		const auto left = f.Views.Loads.back();
		f.Host.reset();
		expect(f.Host.state().Tabs.empty() && !f.Host.state().WindowComparison && f.Output.Published.back().Tabs.empty(),
			   "the next pull request starts with no tab");
		f.open_window(pair(1, 2));
		f.Host.apply(core::open_in_active_tab_intent{ file(1) });
		const auto next = f.Views.Loads.back();
		expect(next.Tab != left.Tab && next.Request != left.Request, "identities continue across pull requests");
		f.answer(left);
		expect(f.Host.stale_terminals() == 1U && f.Host.state().Tabs[0].State == core::document_tab_view_state::loading,
			   "the left pull request's late completion reaches nothing");
		f.Host.complete(next.Request, core::document_tab_view_outcome::ready);
		f.Host.shutdown();
		f.Lane.drain();
		expect(f.Host.state().Tabs[0].State == core::document_tab_view_state::loading && f.Host.is_quiescent(),
			   "after shutdown a queued terminal runs as a no-op");
	}
} // namespace

auto main() -> int
{
	test_completions_route_to_the_tab_that_asked();
	test_a_closed_loading_tab_publishes_nothing();
	test_two_views_of_one_file_through_refresh_and_restart();
	test_refusals_are_published_and_change_nothing();
	test_lane_reading_is_persisted_before_immediate_shutdown();
	test_leaving_the_pull_request_retires_its_work();
	std::cout << "All document tab host tests passed\n";
	return 0;
}
