#include "bpr/application/document_tab_host.hpp"

#include <atomic>
#include <utility>

namespace bpr::application {
	struct document_tab_host::implementation
	{
		implementation(decision_executor& aLane, document_tab_view_port& aViews, document_tabs_projection_port& aProjections, persistence aPersist)
			: Lane{ aLane }
			, Views{ aViews }
			, Projections{ aProjections }
			, Persist{ std::move(aPersist) }
		{
		}

		decision_executor&             Lane;
		document_tab_view_port&        Views;
		document_tabs_projection_port& Projections;
		persistence                    Persist;
		core::document_tabs_state      State;
		core::document_tab_reason      LastReason = core::document_tab_reason::none;
		std::size_t                    Loads      = 0U;
		std::size_t                    Stale      = 0U;
		/// \brief Terminals posted onto the lane and not yet run; they may be
		/// posted from any thread.
		std::atomic<std::size_t> Posted{ 0U };
		bool                     Closed = false;

		[[nodiscard]] auto projection() const -> document_tabs_projection
		{
			auto result             = document_tabs_projection{ };
			result.WindowComparison = State.WindowComparison;
			result.Reason           = LastReason;
			result.Tabs.reserve(State.Tabs.size());
			for (const auto& tab : State.Tabs) {
				result.Tabs.push_back(document_tab_entry{
					.Id            = tab.Id,
					.View          = tab.View,
					.State         = tab.State,
					.Active        = State.Active == tab.Id,
					.OwnComparison = core::has_own_comparison(State, tab),
				});
			}
			return result;
		}

		/// \brief Performs what one core decision asks: publish and persist a
		/// change, then start the load it names.
		void perform(const core::document_tabs_output& aOutput)
		{
			LastReason = aOutput.Reason;
			if (aOutput.Reason == core::document_tab_reason::stale_view) {
				++Stale;
			}
			if (aOutput.Changed || aOutput.Reason != core::document_tab_reason::none) {
				Projections.publish(projection());
			}
			if (aOutput.Changed && Persist) {
				if (const auto persisted = core::persisted(State); persisted) {
					Persist(*persisted);
				}
			}
			// Projection or persistence feedback can supersede this decision.
			// Only the load still pending in the core may reach the view port.
			if (!Closed && aOutput.Load && State.Pending == aOutput.Load) {
				++Loads;
				Views.load(*aOutput.Load);
			}
		}
	};

	document_tab_host::document_tab_host(
		decision_executor&             aDecisionLane,
		document_tab_view_port&        aViews,
		document_tabs_projection_port& aProjections,
		persistence                    aPersist)
		: Implementation{ std::make_shared<implementation>(aDecisionLane, aViews, aProjections, std::move(aPersist)) }
	{
	}

	document_tab_host::~document_tab_host()
	{
		Implementation->Closed = true;
	}

	void document_tab_host::apply(const core::document_tab_intent& aIntent)
	{
		auto& host = *Implementation;
		if (!host.Closed) {
			host.perform(core::transition(host.State, aIntent));
		}
	}

	void document_tab_host::restore(const core::window_review_tabs& aTabs)
	{
		auto& host = *Implementation;
		if (!host.Closed) {
			host.perform(core::restore(host.State, aTabs));
		}
	}

	void document_tab_host::observe_file_choice(const core::file_identity& aFile)
	{
		auto& host = *Implementation;
		if (!host.Closed) {
			host.perform(core::observe_file_choice(host.State, aFile));
		}
	}

	void document_tab_host::refresh()
	{
		auto& host = *Implementation;
		if (!host.Closed && host.State.Active) {
			host.perform(core::reload_active(host.State));
		}
	}

	void document_tab_host::reset()
	{
		auto& host = *Implementation;
		if (host.Closed) {
			return;
		}
		// Identities and requests continue, so nothing issued for the pull
		// request left behind can name a tab of the next one.
		auto next        = core::document_tabs_state{ };
		next.LastTab     = host.State.LastTab;
		next.LastRequest = host.State.LastRequest;
		host.State       = std::move(next);
		host.LastReason  = core::document_tab_reason::none;
		host.Projections.publish(host.projection());
	}

	void document_tab_host::complete(const core::request_id aRequest, const core::document_tab_view_outcome aOutcome)
	{
		auto host = Implementation;
		++host->Posted;
		host->Lane.post([host, aRequest, aOutcome] {
			--host->Posted;
			if (!host->Closed) {
				host->perform(core::complete_view(host->State, aRequest, aOutcome));
			}
		});
	}

	void document_tab_host::observe_reading_position(const core::request_id aRequest, std::optional<core::semantic_selection> aSelection)
	{
		auto host = Implementation;
		++host->Posted;
		host->Lane.post([host, aRequest, selection = std::move(aSelection)] {
			--host->Posted;
			if (!host->Closed) {
				host->perform(core::observe_reading_position(host->State, aRequest, selection));
			}
		});
	}

	void document_tab_host::complete_on_lane(const core::request_id aRequest, const core::document_tab_view_outcome aOutcome)
	{
		auto& host = *Implementation;
		if (!host.Closed) {
			host.perform(core::complete_view(host.State, aRequest, aOutcome));
		}
	}

	void document_tab_host::observe_reading_position_on_lane(const core::request_id aRequest, const std::optional<core::semantic_selection>& aSelection)
	{
		auto& host = *Implementation;
		if (!host.Closed) {
			host.perform(core::observe_reading_position(host.State, aRequest, aSelection));
		}
	}

	void document_tab_host::complete_with_reading_position_on_lane(const core::request_id aRequest, const std::optional<core::semantic_selection>& aSelection)
	{
		auto& host = *Implementation;
		if (!host.Closed) {
			host.perform(core::complete_view_with_reading_position(host.State, aRequest, aSelection));
		}
	}

	void document_tab_host::shutdown()
	{
		Implementation->Closed = true;
	}

	auto document_tab_host::state() const noexcept -> const core::document_tabs_state&
	{
		return Implementation->State;
	}

	auto document_tab_host::projection() const -> document_tabs_projection
	{
		return Implementation->projection();
	}

	auto document_tab_host::loads() const noexcept -> std::size_t
	{
		return Implementation->Loads;
	}

	auto document_tab_host::stale_terminals() const noexcept -> std::size_t
	{
		return Implementation->Stale;
	}

	auto document_tab_host::last_reason() const noexcept -> core::document_tab_reason
	{
		return Implementation->LastReason;
	}

	auto document_tab_host::is_quiescent() const noexcept -> bool
	{
		return Implementation->Posted == 0U;
	}
} // namespace bpr::application
