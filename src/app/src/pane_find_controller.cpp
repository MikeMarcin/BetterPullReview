#include "pane_find_controller.hpp"
#include <algorithm>

namespace bpr::app {
	pane_find_controller::pane_find_controller(qt::diff_workbench& aSurface, application::decision_executor& aDecisions, application::effect_executor& aEffects, scope_supplier aScope, std::function<void()> aYield)
		: Surface{ aSurface }
		, Scope{ std::move(aScope) }
		, Yield{ std::move(aYield) }
		, Host{ aDecisions, aEffects, *this, *this }
	{
		Surface.set_source_find_dispatch([this](const auto& aIntent) { dispatch(aIntent); });
	}
	pane_find_controller::~pane_find_controller() { Surface.set_source_find_dispatch({ }); }
	void pane_find_controller::dispatch(const qt::source_find_intent& aIntent)
	{
		switch (aIntent.Gesture) {
		case qt::source_find_gesture::next:        static_cast<void>(Host.navigate(core::change_navigation_direction::next)); return;
		case qt::source_find_gesture::previous:    static_cast<void>(Host.navigate(core::change_navigation_direction::previous)); return;
		case qt::source_find_gesture::close:       static_cast<void>(Host.close()); return;
		case qt::source_find_gesture::query_dirty: static_cast<void>(Host.clear()); return;
		case qt::source_find_gesture::document_changed:
			static_cast<void>(Host.set_scope(std::nullopt));
			{
				const auto lock = std::lock_guard{ MaterialMutex };
				Material.reset();
				Cache.reset();
			}
			return;
		default: break;
		}
		auto        scope   = Scope();
		const auto& diff    = Surface.current_result();
		const auto& context = Surface.current_context();
		if (!scope || !diff || !context || !Surface.current_semantic_index()) {
			static_cast<void>(Host.set_scope(std::nullopt));
			return;
		}
		scope->Target    = aIntent.Target;
		scope->Selection = aIntent.Selection;
		scope->Document  = core::source_search_document{ context->Key.File, diff->Alignment.Digest, context->Key.LeftBlob, context->Key.RightBlob };
		auto accepted    = std::make_shared<material>();
		accepted->Scope  = *scope;
		accepted->Diff   = diff;
		accepted->Index  = Surface.current_semantic_index();
		accepted->Origin = aIntent;
		{
			const auto lock = std::lock_guard{ MaterialMutex };
			Material        = std::move(accepted);
		}
		if (Yield) {
			Yield();
		}
		static_cast<void>(Host.set_scope(scope));
		if (Host.projection().Query != aIntent.Query || Host.state().Options != aIntent.Options || !Host.state().Open) {
			static_cast<void>(Host.set_query(aIntent.Query, aIntent.Options));
		}
	}
	auto pane_find_controller::execute(const application::source_search_work& aWork, std::stop_token aStop) -> application::source_search_execution
	{
		auto accepted = std::shared_ptr<const material>{ };
		auto reused   = std::optional<application::source_search_execution>{ };
		{
			const auto lock = std::lock_guard{ MaterialMutex };
			accepted        = Material;
			if (!Stopping && Cache && Cache->Scope == aWork.Request.Scope && Cache->Options == aWork.Request.Options && Cache->Query == aWork.Query) {
				reused = Cache->Result;
			}
		}
		if (!accepted || accepted->Scope != aWork.Request.Scope) {
			return application::source_search_execution{ { aWork.Request, core::source_search_status::cancelled, { } } };
		}
		auto entry   = core::source_match_entry{ };
		entry.File   = aWork.Request.Scope.Document->File;
		entry.Target = aWork.Request.Scope.Target;
		if (aWork.Request.Scope.Selection) {
			entry.Selection = std::pair{ aWork.Request.Scope.Selection->Side, aWork.Request.Scope.Selection->Span };
		}
		entry.Unified   = accepted->Index.get();
		const auto bind = [&](const auto& aDocument, const auto& aBlob) -> std::optional<core::source_match_side> {
			if (!aDocument || !aBlob) {
				return std::nullopt;
			}
			return core::source_match_side{ *aBlob, std::string_view{ reinterpret_cast<const char*>(aDocument->Bytes.data()), aDocument->Bytes.size() } };
		};
		entry.Left  = bind(accepted->Diff->LeftDocument, aWork.Request.Scope.Document->LeftBlob);
		entry.Right = bind(accepted->Diff->RightDocument, aWork.Request.Scope.Document->RightBlob);
		auto result = application::source_search_execution{ { aWork.Request, core::source_search_status::cancelled, { } } };
		if (aStop.stop_requested()) {
			return result;
		}
		if (reused) {
			result                  = std::move(*reused);
			result.Terminal.Request = aWork.Request;
			CachedRuns.fetch_add(1U, std::memory_order_relaxed);
		} else {
			MatchingRuns.fetch_add(1U, std::memory_order_relaxed);
			result          = application::execute_source_pane_match(aWork, entry, aStop);
			const auto lock = std::lock_guard{ MaterialMutex };
			if (!Stopping && Material && Material->Scope == aWork.Request.Scope && result.Terminal.Status == core::source_search_status::complete) {
				Cache.emplace(aWork, result);
			}
		}
		if (result.Matches && !result.Matches->empty()) {
			const auto& origin                   = accepted->Origin;
			const auto  at                       = std::ranges::find_if(*result.Matches, [&](const application::source_search_match& aMatch) {
				if (entry.Target == core::source_search_target::unified) {
					return aMatch.Match.UnifiedRow >= origin.OriginRow;
				}
				const auto& location = aMatch.Match.Locations[origin.OriginSide == core::diff_side::left ? 0U : 1U];
				return location && location->Span.Begin >= origin.OriginByte;
			});
			result.Terminal.Summary.InitialMatch = at == result.Matches->end() ? 0U : static_cast<std::uint32_t>(at - result.Matches->begin());
		}
		return result;
	}
	void pane_find_controller::publish(const application::source_search_projection& aProjection)
	{
		Surface.observe_source_find_execution(MatchingRuns.load(std::memory_order_relaxed), CachedRuns.load(std::memory_order_relaxed));
		Surface.present_source_find(aProjection);
	}
	void pane_find_controller::observe_scope()
	{
		const auto& bound = Host.state().Scope;
		if (!bound) {
			return;
		}
		const auto current = Scope();
		if (!current || current->Session != bound->Session || current->Authority != bound->Authority
			|| current->Comparison != bound->Comparison || current->Tab != bound->Tab
			|| current->CatalogueGeneration != bound->CatalogueGeneration) {
			Surface.retarget_source_find();
		}
	}
	void pane_find_controller::shutdown()
	{
		{
			const auto lock = std::lock_guard{ MaterialMutex };
			Stopping        = true;
			Cache.reset();
			Material.reset();
		}
		Host.shutdown();
	}
	void pane_find_controller::reset()
	{
		Surface.source_find()->reset_query();
		{
			const auto lock = std::lock_guard{ MaterialMutex };
			Material.reset();
			Cache.reset();
		}
		static_cast<void>(Host.reset());
	}
	auto pane_find_controller::is_quiescent() const -> bool { return Host.is_quiescent(); }
	auto pane_find_controller::counters() const -> application::source_search_host_counters { return Host.counters(); }
}
