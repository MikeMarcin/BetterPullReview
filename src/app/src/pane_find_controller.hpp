#ifndef BPR_SRC_APP_SRC_PANE_FIND_CONTROLLER_HPP
#define BPR_SRC_APP_SRC_PANE_FIND_CONTROLLER_HPP

#include "bpr/application/source_search_host.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include <atomic>
#include <mutex>

namespace bpr::app {
	/// \brief Window-owned pane search composition; immutable accepted material only.
	class pane_find_controller final : public application::source_search_execution_port, public application::source_search_projection_port
	{
	public:
		using scope_supplier = std::function<std::optional<core::source_search_scope>()>;
		pane_find_controller(qt::diff_workbench& aSurface, const core::source_regex_engine& aEngine, application::decision_executor& aDecisions, application::effect_executor& aEffects, scope_supplier aScope, std::function<void()> aYield);
		~pane_find_controller() override;
		void               dispatch(const qt::source_find_intent& aIntent);
		void               shutdown();
		void               reset();
		void               observe_scope();
		[[nodiscard]] auto is_quiescent() const -> bool;
		[[nodiscard]] auto counters() const -> application::source_search_host_counters;
		auto               execute(const application::source_search_work& aWork, std::stop_token aStop) -> application::source_search_execution override;
		void               publish(const application::source_search_projection& aProjection) override;

	private:
		struct material
		{
			core::source_search_scope                        Scope;
			std::shared_ptr<const core::computed_file_diff>  Diff;
			std::shared_ptr<const core::diff_semantic_index> Index;
			qt::source_find_intent                           Origin;
		};
		qt::diff_workbench&              Surface;
		const core::source_regex_engine& Engine;
		scope_supplier                   Scope;
		std::function<void()>            Yield;
		std::mutex                       MaterialMutex;
		std::shared_ptr<const material>  Material;
		struct cached_match
		{
			core::source_search_scope            Scope;
			core::source_search_options          Options;
			std::string                          Query;
			application::source_search_execution Result;
			cached_match(const application::source_search_work& aWork, application::source_search_execution aResult)
				: Scope{ aWork.Request.Scope }
				, Options{ aWork.Request.Options }
				, Query{ aWork.Query }
				, Result{ std::move(aResult) }
			{ }
		};
		std::optional<cached_match>     Cache;
		bool                            Stopping = false;
		std::atomic<std::uint64_t>      MatchingRuns{ 0U };
		std::atomic<std::uint64_t>      CachedRuns{ 0U };
		application::source_search_host Host;
	};
}
#endif
