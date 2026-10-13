#include "pr_search_controller.hpp"
#include <QPlainTextEdit>
#include <algorithm>
namespace bpr::app {
	pr_search_controller::pr_search_controller(qt::source_search_panel& aPanel, qt::diff_workbench& aSurface, application::source_corpus_port& aCorpus, application::decision_executor& aDecisions, application::effect_executor& aEffects, scope_supplier aScope, open_file aOpen, reveal_validator aValidate, tab_supplier aTab)
		: Panel{ aPanel }
		, Surface{ aSurface }
		, Scope{ std::move(aScope) }
		, OpenFile{ std::move(aOpen) }
		, Validate{ std::move(aValidate) }
		, Tab{ std::move(aTab) }
		, Execution{ aCorpus }
		, Host{ aDecisions, aEffects, Execution, *this }
	{
		Panel.Activate = [this](auto aIndex) { activate(aIndex); };
		Panel.Select   = [this](auto aIndex) { static_cast<void>(Host.select_match(aIndex)); };
		RevealTimer.setInterval(10);
		QObject::connect(&RevealTimer, &QTimer::timeout, [this] { try_reveal(); });
	}
	pr_search_controller::~pr_search_controller()
	{
		Panel.Activate = { };
		Panel.Select   = { };
	}
	void pr_search_controller::open()
	{
		const auto wasOpen = Host.state().Open;
		static_cast<void>(Host.set_scope(Scope()));
		if (!wasOpen) {
			auto seed = Surface.source_search_seed();
			if (seed.isEmpty()) {
				seed = QString::fromStdString(Surface.source_find()->query());
			}
			Panel.bar()->seed(seed, true);
			dispatch(qt::source_find_gesture::edit);
		} else {
			Panel.bar()->seed({ }, false);
		}
	}
	void pr_search_controller::dispatch(qt::source_find_gesture aGesture)
	{
		switch (aGesture) {
		case qt::source_find_gesture::escape: Surface.focus_source(); return;
		case qt::source_find_gesture::close:
			Panel.cancel_pending();
			cancel_reveal();
			static_cast<void>(Host.close());
			return;
		case qt::source_find_gesture::query_dirty:
			cancel_reveal();
			static_cast<void>(Host.clear());
			return;
		case qt::source_find_gesture::selection:
			cancel_reveal();
			static_cast<void>(Host.retry());
			return;
		default: break;
		}
		cancel_reveal();
		static_cast<void>(Host.set_scope(Scope()));
		const auto projection = Host.projection();
		const auto include    = Panel.include();
		const auto exclude    = Panel.exclude();
		if (projection.Include != include || projection.Exclude != exclude) {
			static_cast<void>(Host.set_paths(include, exclude));
		}
		if (Host.projection().Query != Panel.bar()->query() || Host.state().Options != Panel.bar()->options() || !Host.state().Open) {
			static_cast<void>(Host.set_query(Panel.bar()->query(), Panel.bar()->options()));
		}
	}
	void pr_search_controller::navigate(core::change_navigation_direction aDirection)
	{
		static_cast<void>(Host.navigate(aDirection));
		if (Host.state().CurrentMatch) {
			activate(*Host.state().CurrentMatch);
		}
	}
	void pr_search_controller::activate(std::uint32_t aIndex)
	{
		cancel_reveal();
		const auto projection = Host.projection();
		const auto current    = Scope();
		if (!current || projection.Scope != current || !projection.Matches || !projection.Corpus || aIndex >= projection.Matches->size()) {
			return;
		}
		const auto& hit   = (*projection.Matches)[aIndex];
		const auto  entry = std::ranges::find(projection.Corpus->Entries, hit.File, &application::source_corpus_entry::File);
		if (entry == projection.Corpus->Entries.end()) {
			return;
		}
		const auto& location = hit.Match.Locations[0] ? hit.Match.Locations[0] : hit.Match.Locations[1];
		if (!location) {
			return;
		}
		const auto& side = location->Side == core::diff_side::left ? entry->Left : entry->Right;
		if (!side || side->Object != location->Blob) {
			return;
		}
		static_cast<void>(Host.select_match(aIndex));
		const auto tab = OpenFile(*current, *entry);
		if (!tab) {
			return;
		}
		Pending = reveal{ *current, tab, projection.Generation, hit.File, side->Path, *location };
		RevealTimer.start();
		try_reveal();
	}
	void pr_search_controller::try_reveal()
	{
		if (!Pending) {
			RevealTimer.stop();
			return;
		}
		const auto scope      = Scope();
		const auto projection = Host.projection();
		if (scope != std::optional{ Pending->Scope } || Tab() != Pending->Tab || projection.Generation != Pending->Generation || projection.Scope != std::optional{ Pending->Scope }) {
			cancel_reveal();
			return;
		}
		const auto& context = Surface.current_context();
		if (!context || context->Key.Authority != Pending->Scope.Authority || context->Key.File != Pending->File || context->Key.LeftAnchor != Pending->Scope.Comparison.Left.Identity || context->Key.RightAnchor != Pending->Scope.Comparison.Right.Identity) {
			return;
		}
		if (Validate(Pending->Scope, Pending->File, Pending->Path, Pending->Location) && Surface.reveal_source_match(Pending->Location, true)) {
			cancel_reveal();
			Surface.focus_source();
		}
	}
	void pr_search_controller::observe_scope()
	{
		const auto scope = Scope();
		if (Host.state().Scope != scope) {
			cancel_reveal();
			static_cast<void>(Host.set_scope(scope));
		}
	}
	void pr_search_controller::cancel_reveal()
	{
		Pending.reset();
		RevealTimer.stop();
	}
	void pr_search_controller::reset()
	{
		cancel_reveal();
		Panel.reset();
		static_cast<void>(Host.reset());
	}
	void pr_search_controller::shutdown()
	{
		Panel.cancel_pending();
		cancel_reveal();
		Host.shutdown();
	}
	auto pr_search_controller::is_quiescent() const -> bool { return Host.is_quiescent() && !RevealTimer.isActive(); }
	auto pr_search_controller::counters() const -> application::source_search_host_counters { return Host.counters(); }
	void pr_search_controller::publish(const application::source_search_projection& aProjection)
	{
		auto projection       = aProjection;
		projection.Statistics = Execution.statistics();
		Panel.set_snippet_font(Surface.right_pane()->font());
		if (projection.Scope) {
			const auto left  = core::canonical_hex(projection.Scope->Comparison.Left.Commit);
			const auto right = core::canonical_hex(projection.Scope->Comparison.Right.Commit);
			Panel.set_scope_labels(QStringLiteral("PR #%1  %2 → %3").arg(projection.Scope->Authority.Number.value()).arg(QString::fromUtf8(left.view().data(), 8), QString::fromUtf8(right.view().data(), 8)), QStringLiteral("Left %1\nRight %2").arg(QString::fromUtf8(left.view().data(), static_cast<qsizetype>(left.view().size())), QString::fromUtf8(right.view().data(), static_cast<qsizetype>(right.view().size()))));
		}
		Panel.present(projection);
	}
}
