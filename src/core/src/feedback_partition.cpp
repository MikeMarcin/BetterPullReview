#include "bpr/core/feedback_partition.hpp"

#include <algorithm>
#include <functional>
#include <numeric>
#include <utility>

namespace bpr::core {
	namespace {
		/// A clean repository-relative path: no empty, leading or trailing
		/// component. An empty path is allowed only where the caller says so.
		auto clean_path(std::string_view aPath) noexcept -> bool
		{
			return !aPath.starts_with('/') && !aPath.ends_with('/') && aPath.find("//") == std::string_view::npos;
		}
		auto valid_anchor(const feedback_anchor& aAnchor) noexcept -> bool
		{
			if (aAnchor.Status != feedback_anchor_status::file) {
				return !aAnchor.Path;
			}
			return aAnchor.Path && !aAnchor.Path->empty() && clean_path(*aAnchor.Path);
		}
		auto valid_scope(const feedback_counter_scope& aScope) noexcept -> bool
		{
			switch (aScope.Kind) {
			case feedback_counter_scope_kind::pull_request:
				return aScope.Path.empty();
			case feedback_counter_scope_kind::file:
				return !aScope.Path.empty() && clean_path(aScope.Path);
			case feedback_counter_scope_kind::folder:
			case feedback_counter_scope_kind::directory:
				return clean_path(aScope.Path);
			case feedback_counter_scope_kind::unattributed:
				return aScope.Path.empty();
			}
			return false;
		}
		/// Scopes that hold units by their exact file, so a unit whose file is
		/// unknown may be missing from them.
		auto file_bound(feedback_counter_scope_kind aKind) noexcept -> bool
		{
			return aKind != feedback_counter_scope_kind::pull_request && aKind != feedback_counter_scope_kind::unattributed;
		}

		auto index_of(feedback_resolution aResolution) noexcept -> std::size_t
		{
			return static_cast<std::size_t>(aResolution);
		}
		auto includes(feedback_resolution_mode aMode, std::size_t aState) noexcept -> bool
		{
			return aMode == feedback_resolution_mode::all || aState == index_of(feedback_resolution::unresolved);
		}
		/// The units a mode includes from one row.
		auto included(const feedback_state_counts& aRow, feedback_resolution_mode aMode) noexcept -> std::size_t
		{
			std::size_t total = 0U;
			for (std::size_t state = 0U; state < aRow.size(); ++state) {
				total += includes(aMode, state) ? aRow[state] : 0U;
			}
			return total;
		}
		auto sum(const feedback_state_counts& aRow) noexcept -> std::size_t
		{
			return std::accumulate(aRow.begin(), aRow.end(), std::size_t{ 0U });
		}

		/// Every row of a tally, labelled ones first.
		template <typename TVisit>
		void for_each_row(const feedback_counter_tally& aTally, TVisit aVisit)
		{
			std::ranges::for_each(aTally.Labelled, aVisit);
			aVisit(aTally.Plain);
			aVisit(aTally.Custom);
			aVisit(aTally.Unclassified);
		}
		/// The scope's units per state, whatever the mode.
		auto state_totals(const feedback_counter_tally& aTally) -> feedback_state_counts
		{
			feedback_state_counts totals{ };
			for_each_row(aTally, [&](const feedback_state_counts& aRow) { std::ranges::transform(totals, aRow, totals.begin(), std::plus{ }); });
			return totals;
		}
		/// The units a mode includes, counted over every row independently of
		/// how the rows are later split into columns and catch-all.
		auto included_total(const feedback_counter_tally& aTally, feedback_resolution_mode aMode) -> std::size_t
		{
			std::size_t total = 0U;
			for_each_row(aTally, [&](const feedback_state_counts& aRow) { total += included(aRow, aMode); });
			return total;
		}

		/// Whether a root's label is known: labelled, plain or a custom label.
		/// Every other root counts in the `Unclassified` row.
		auto classified(feedback_root_status aRoot) noexcept -> bool
		{
			switch (aRoot) {
			case feedback_root_status::labelled:
			case feedback_root_status::unknown_label:
			case feedback_root_status::plain:
				return true;
			case feedback_root_status::unclassified:
			case feedback_root_status::root_missing:
			case feedback_root_status::root_deleted:
			case feedback_root_status::root_ambiguous:
				break;
			}
			return false;
		}
		/// The units the Open count leaves out or cannot classify, from one
		/// tally: every unknown and not-applicable state, and the unclassified
		/// units of the other two states, so no unit is counted twice.
		auto uncertain_total(const feedback_counter_tally& aTally, const feedback_state_counts& aStates) noexcept -> std::size_t
		{
			return aStates[index_of(feedback_resolution::unknown)] + aStates[index_of(feedback_resolution::not_applicable)]
				+ aTally.Unclassified[index_of(feedback_resolution::unresolved)] + aTally.Unclassified[index_of(feedback_resolution::resolved)];
		}
		/// Whether a unit is one of `uncertain_total`'s.
		auto uncertain_unit(const feedback_counter_unit& aUnit) noexcept -> bool
		{
			return aUnit.Resolution == feedback_resolution::unknown || aUnit.Resolution == feedback_resolution::not_applicable || !classified(aUnit.Root);
		}

		/// The tally row a unit's root classification counts in.
		auto row_of(feedback_counter_tally& aTally, const feedback_counter_unit& aUnit) -> feedback_state_counts&
		{
			switch (aUnit.Root) {
			case feedback_root_status::labelled:
				return aTally.Labelled[static_cast<std::size_t>(*aUnit.Label)];
			case feedback_root_status::unknown_label:
				return aTally.Custom;
			case feedback_root_status::plain:
				return aTally.Plain;
			case feedback_root_status::unclassified:
			case feedback_root_status::root_missing:
			case feedback_root_status::root_deleted:
			case feedback_root_status::root_ambiguous:
				break;
			}
			return aTally.Unclassified;
		}

		/// The root classification of one conversation; one whose posts
		/// were never observed has no root.
		auto classification_of(const feedback_annotation_set& aAnnotations, std::string_view aId) -> std::pair<feedback_root_status, std::optional<feedback_label>>
		{
			const auto found = std::ranges::lower_bound(aAnnotations.Conversations, aId, std::less{ }, [](const feedback_conversation& aItem) -> std::string_view { return aItem.Id; });
			if (found == aAnnotations.Conversations.end() || found->Id != aId) {
				return { feedback_root_status::root_missing, std::nullopt };
			}
			return { found->Status, found->Status == feedback_root_status::labelled ? found->Label : std::nullopt };
		}

		/// Count a unit left out of the shared population, or report that it
		/// is shared.
		auto excluded(feedback_population_exclusions& aExcluded, const feedback_conversation_facts& aFacts) -> bool
		{
			switch (aFacts.Origin) {
			case feedback_unit_origin::published:
				break;
			case feedback_unit_origin::private_note:
				++aExcluded.Private;
				return true;
			case feedback_unit_origin::draft:
			case feedback_unit_origin::pending_publication:
				++aExcluded.Unpublished;
				return true;
			}
			if (aFacts.Deleted) {
				++aExcluded.Deleted;
				return true;
			}
			return false;
		}

		void add_limits(feedback_counter_projection& aProjection, const feedback_counter_tally& aTally)
		{
			switch (aTally.Collection) {
			case feedback_collection::complete:
				break;
			case feedback_collection::partial:
				aProjection.Limits.push_back(feedback_count_limit::collection_partial);
				break;
			case feedback_collection::stale:
				aProjection.Limits.push_back(feedback_count_limit::collection_stale);
				break;
			case feedback_collection::unavailable:
				aProjection.Limits.push_back(feedback_count_limit::collection_unavailable);
				break;
			}
			if (aProjection.ScopeUnclassified != 0U) {
				aProjection.Limits.push_back(feedback_count_limit::classification_partial);
			}
			if (aProjection.Mode == feedback_resolution_mode::unresolved && aProjection.States[index_of(feedback_resolution::unknown)] != 0U) {
				aProjection.Limits.push_back(feedback_count_limit::resolution_unknown);
			}
			if (aTally.AttributionUnknown) {
				aProjection.Limits.push_back(feedback_count_limit::attribution_unknown);
			}
			aProjection.Exact = aProjection.Limits.empty();
		}
	}

	auto stable_name(feedback_resolution aResolution) noexcept -> std::string_view
	{
		switch (aResolution) {
		case feedback_resolution::unresolved:
			return "unresolved";
		case feedback_resolution::resolved:
			return "resolved";
		case feedback_resolution::unknown:
			return "unknown";
		case feedback_resolution::not_applicable:
			return "not_applicable";
		}
		return "unknown";
	}
	auto stable_name(feedback_resolution_mode aMode) noexcept -> std::string_view
	{
		switch (aMode) {
		case feedback_resolution_mode::unresolved:
			return "unresolved";
		case feedback_resolution_mode::all:
			return "all";
		}
		return "unknown";
	}
	auto stable_name(feedback_unit_origin aOrigin) noexcept -> std::string_view
	{
		switch (aOrigin) {
		case feedback_unit_origin::published:
			return "published";
		case feedback_unit_origin::private_note:
			return "private_note";
		case feedback_unit_origin::draft:
			return "draft";
		case feedback_unit_origin::pending_publication:
			return "pending_publication";
		}
		return "unknown";
	}
	auto stable_name(feedback_anchor_status aStatus) noexcept -> std::string_view
	{
		switch (aStatus) {
		case feedback_anchor_status::file:
			return "file";
		case feedback_anchor_status::unanchored:
			return "unanchored";
		case feedback_anchor_status::unknown:
			return "unknown";
		}
		return "unknown";
	}
	auto stable_name(feedback_collection aCollection) noexcept -> std::string_view
	{
		switch (aCollection) {
		case feedback_collection::complete:
			return "complete";
		case feedback_collection::partial:
			return "partial";
		case feedback_collection::stale:
			return "stale";
		case feedback_collection::unavailable:
			return "unavailable";
		}
		return "unknown";
	}
	auto stable_name(feedback_population_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case feedback_population_refusal::none:
			return "none";
		case feedback_population_refusal::invalid_scope:
			return "invalid_scope";
		case feedback_population_refusal::invalid_identity:
			return "invalid_identity";
		case feedback_population_refusal::invalid_anchor:
			return "invalid_anchor";
		case feedback_population_refusal::contradictory_duplicate:
			return "contradictory_duplicate";
		}
		return "unknown";
	}
	auto stable_name(feedback_count_limit aLimit) noexcept -> std::string_view
	{
		switch (aLimit) {
		case feedback_count_limit::collection_partial:
			return "collection_partial";
		case feedback_count_limit::collection_stale:
			return "collection_stale";
		case feedback_count_limit::collection_unavailable:
			return "collection_unavailable";
		case feedback_count_limit::classification_partial:
			return "classification_partial";
		case feedback_count_limit::resolution_unknown:
			return "resolution_unknown";
		case feedback_count_limit::attribution_unknown:
			return "attribution_unknown";
		}
		return "unknown";
	}
	auto stable_name(feedback_count_kind aKind) noexcept -> std::string_view
	{
		switch (aKind) {
		case feedback_count_kind::included:
			return "included";
		case feedback_count_kind::dedicated:
			return "dedicated";
		case feedback_count_kind::catchall:
			return "catchall";
		case feedback_count_kind::uncertain:
			return "uncertain";
		}
		return "unknown";
	}
	auto stable_name(feedback_selection_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case feedback_selection_refusal::none:
			return "none";
		case feedback_selection_refusal::invalid_scope:
			return "invalid_scope";
		case feedback_selection_refusal::duplicate_column:
			return "duplicate_column";
		case feedback_selection_refusal::unselected_label:
			return "unselected_label";
		}
		return "unknown";
	}
	auto stable_name(feedback_projection_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case feedback_projection_refusal::none:
			return "none";
		case feedback_projection_refusal::invalid_tally:
			return "invalid_tally";
		case feedback_projection_refusal::duplicate_column:
			return "duplicate_column";
		}
		return "unknown";
	}

	auto build_feedback_population(const feedback_annotation_set& aAnnotations, std::span<const feedback_conversation_facts> aFacts,
								   feedback_collection aCollection) -> feedback_population_result
	{
		feedback_population_result result;
		if (!valid(aAnnotations.Scope)) {
			result.Refusal = feedback_population_refusal::invalid_scope;
			return result;
		}
		if (std::ranges::any_of(aFacts, [](const feedback_conversation_facts& aItem) { return aItem.Id.empty(); })) {
			result.Refusal = feedback_population_refusal::invalid_identity;
			return result;
		}
		if (!std::ranges::all_of(aFacts, [](const feedback_conversation_facts& aItem) { return valid_anchor(aItem.Anchor); })) {
			result.Refusal = feedback_population_refusal::invalid_anchor;
			return result;
		}
		// Visit each identity once, in identity order; equal neighbours are
		// pages that repeated it.
		std::vector<std::size_t> order(aFacts.size());
		std::iota(order.begin(), order.end(), std::size_t{ 0U });
		std::ranges::stable_sort(order, std::less{ }, [&](std::size_t aAt) -> std::string_view { return aFacts[aAt].Id; });
		auto& population      = result.Population;
		population.Scope      = aAnnotations.Scope;
		population.Epoch      = aAnnotations.Epoch;
		population.Collection = aCollection;
		for (std::size_t at = 0U; at < order.size(); ++at) {
			const auto& facts = aFacts[order[at]];
			if (at != 0U && aFacts[order[at - 1U]].Id == facts.Id) {
				if (aFacts[order[at - 1U]] != facts) {
					return { .Refusal = feedback_population_refusal::contradictory_duplicate, .Population = { } };
				}
				++population.Excluded.Collapsed;
				continue;
			}
			if (excluded(population.Excluded, facts)) {
				continue;
			}
			const auto [root, label] = classification_of(aAnnotations, facts.Id);
			population.Units.push_back({ .Id = facts.Id, .Root = root, .Label = label, .Resolution = facts.Resolution, .Anchor = facts.Anchor });
		}
		return result;
	}

	auto in_feedback_counter_scope(const feedback_counter_unit& aUnit, const feedback_counter_scope& aScope) -> bool
	{
		if (aScope.Kind == feedback_counter_scope_kind::pull_request) {
			return true;
		}
		const auto exact = aUnit.Anchor.Status == feedback_anchor_status::file && aUnit.Anchor.Path;
		if (aScope.Kind == feedback_counter_scope_kind::unattributed) {
			return !exact;
		}
		if (!exact) {
			return false;
		}
		const std::string_view path = *aUnit.Anchor.Path;
		if (aScope.Kind == feedback_counter_scope_kind::file) {
			return path == aScope.Path;
		}
		if (aScope.Kind == feedback_counter_scope_kind::directory) {
			const auto slash = path.rfind('/');
			return slash == std::string_view::npos ? aScope.Path.empty() : path.substr(0U, slash) == aScope.Path;
		}
		return aScope.Path.empty() || (path.size() > aScope.Path.size() && path.starts_with(aScope.Path) && path[aScope.Path.size()] == '/');
	}

	auto tally_feedback_counters(const feedback_population& aPopulation, const feedback_counter_scope& aScope) -> feedback_counter_tally
	{
		feedback_counter_tally tally{ .Scope = aPopulation.Scope, .Epoch = aPopulation.Epoch, .Collection = aPopulation.Collection, .Where = aScope };
		if (!valid_scope(aScope)) {
			return tally;
		}
		tally.Valid = true;
		for (const auto& unit : aPopulation.Units) {
			if (file_bound(aScope.Kind) && unit.Anchor.Status == feedback_anchor_status::unknown) {
				tally.AttributionUnknown = true;
			}
			if (!in_feedback_counter_scope(unit, aScope)) {
				continue;
			}
			const auto state = index_of(unit.Resolution);
			++row_of(tally, unit)[state];
			if (unit.Anchor.Status != feedback_anchor_status::file) {
				++tally.Unattributed[state];
			}
		}
		return tally;
	}

	auto project_feedback_counters(const feedback_counter_tally& aTally, std::span<const feedback_label> aColumns,
								   feedback_resolution_mode aMode) -> feedback_counter_projection
	{
		feedback_counter_projection projection{ .Scope = aTally.Scope, .Epoch = aTally.Epoch, .Where = aTally.Where, .Mode = aMode };
		if (!aTally.Valid) {
			projection.Refusal = feedback_projection_refusal::invalid_tally;
			return projection;
		}
		std::array<bool, feedback_label_count> selected{ };
		for (const auto label : aColumns) {
			auto& slot = selected[static_cast<std::size_t>(label)];
			if (slot) {
				projection.Refusal = feedback_projection_refusal::duplicate_column;
				return projection;
			}
			slot = true;
		}
		for (const auto label : aColumns) {
			const auto& row = aTally.Labelled[static_cast<std::size_t>(label)];
			projection.Dedicated.push_back({ .Label = label, .Count = included(row, aMode), .States = row });
		}
		auto&      parts = projection.CatchallParts;
		const auto add   = [&](const feedback_state_counts& aRow) { std::ranges::transform(projection.CatchallStates, aRow, projection.CatchallStates.begin(), std::plus{ }); };
		for (std::size_t label = 0U; label < feedback_label_count; ++label) {
			parts.Unselected += selected[label] ? 0U : included(aTally.Labelled[label], aMode);
			if (!selected[label]) {
				add(aTally.Labelled[label]);
			}
		}
		add(aTally.Plain);
		add(aTally.Custom);
		add(aTally.Unclassified);
		parts.Plain                  = included(aTally.Plain, aMode);
		parts.Custom                 = included(aTally.Custom, aMode);
		parts.Unclassified           = included(aTally.Unclassified, aMode);
		projection.Catchall          = parts.Unselected + parts.Plain + parts.Custom + parts.Unclassified;
		projection.Included          = included_total(aTally, aMode);
		projection.Unattributed      = included(aTally.Unattributed, aMode);
		projection.States            = state_totals(aTally);
		projection.ScopeTotal        = sum(projection.States);
		projection.ScopeUnclassified = sum(aTally.Unclassified);
		projection.Uncertain         = uncertain_total(aTally, projection.States);
		add_limits(projection, aTally);
		return projection;
	}

	auto merge_feedback_counter_tallies(std::span<const feedback_counter_tally> aParts, const feedback_counter_scope& aWhere) -> feedback_counter_tally
	{
		auto merged = feedback_counter_tally{ .Where = aWhere };
		if (aParts.empty()) {
			return merged;
		}
		const auto& first = aParts.front();
		merged.Scope      = first.Scope;
		merged.Epoch      = first.Epoch;
		merged.Collection = first.Collection;
		const auto agrees = [&](const feedback_counter_tally& aPart) {
			return aPart.Valid && aPart.Scope == first.Scope && aPart.Epoch == first.Epoch && aPart.Collection == first.Collection;
		};
		if (!valid_scope(aWhere) || !std::ranges::all_of(aParts, agrees)) {
			return merged;
		}
		const auto add = [](feedback_state_counts& aInto, const feedback_state_counts& aRow) { std::ranges::transform(aInto, aRow, aInto.begin(), std::plus{ }); };
		for (const auto& part : aParts) {
			for (std::size_t label = 0U; label < feedback_label_count; ++label) {
				add(merged.Labelled[label], part.Labelled[label]);
			}
			add(merged.Plain, part.Plain);
			add(merged.Custom, part.Custom);
			add(merged.Unclassified, part.Unclassified);
			add(merged.Unattributed, part.Unattributed);
			merged.AttributionUnknown = merged.AttributionUnknown || part.AttributionUnknown;
		}
		merged.Valid = true;
		return merged;
	}

	auto feedback_count_of(const feedback_counter_projection& aProjection, const feedback_count_target& aTarget) noexcept -> std::size_t
	{
		switch (aTarget.Kind) {
		case feedback_count_kind::included:
			return aProjection.Included;
		case feedback_count_kind::dedicated:
			for (const auto& column : aProjection.Dedicated) {
				if (aTarget.Label == column.Label) {
					return column.Count;
				}
			}
			return 0U;
		case feedback_count_kind::catchall:
			return aProjection.Catchall;
		case feedback_count_kind::uncertain:
			return aProjection.Uncertain;
		}
		return 0U;
	}

	auto select_feedback_units(const feedback_population& aPopulation, const feedback_counter_scope& aWhere, std::span<const feedback_label> aColumns,
							   feedback_resolution_mode aMode, const feedback_count_target& aTarget) -> feedback_unit_selection
	{
		auto selection = feedback_unit_selection{ .Scope = aPopulation.Scope, .Epoch = aPopulation.Epoch };
		if (!valid_scope(aWhere)) {
			selection.Refusal = feedback_selection_refusal::invalid_scope;
			return selection;
		}
		std::array<bool, feedback_label_count> selected{ };
		for (const auto label : aColumns) {
			auto& slot = selected[static_cast<std::size_t>(label)];
			if (slot) {
				selection.Refusal = feedback_selection_refusal::duplicate_column;
				return selection;
			}
			slot = true;
		}
		if (aTarget.Kind == feedback_count_kind::dedicated && (!aTarget.Label || !selected[static_cast<std::size_t>(*aTarget.Label)])) {
			selection.Refusal = feedback_selection_refusal::unselected_label;
			return selection;
		}
		// The same rules as the tally and the projection: the scope test, the
		// row a root counts in, and the states a mode includes.
		const auto dedicatedTo = [&](const feedback_counter_unit& aUnit) -> std::optional<feedback_label> {
			if (aUnit.Root == feedback_root_status::labelled && aUnit.Label && selected[static_cast<std::size_t>(*aUnit.Label)]) {
				return aUnit.Label;
			}
			return std::nullopt;
		};
		for (const auto& unit : aPopulation.Units) {
			if (!in_feedback_counter_scope(unit, aWhere)) {
				continue;
			}
			const auto inMode = includes(aMode, index_of(unit.Resolution));
			auto       counts = false;
			switch (aTarget.Kind) {
			case feedback_count_kind::included:
				counts = inMode;
				break;
			case feedback_count_kind::dedicated:
				counts = inMode && dedicatedTo(unit) == aTarget.Label;
				break;
			case feedback_count_kind::catchall:
				counts = inMode && !dedicatedTo(unit);
				break;
			case feedback_count_kind::uncertain:
				counts = uncertain_unit(unit);
				break;
			}
			if (counts) {
				selection.Ids.push_back(unit.Id);
			}
		}
		return selection;
	}

	auto feedback_sort_key_of(const feedback_counter_projection& aProjection, const feedback_count_target& aTarget) noexcept -> feedback_sort_key
	{
		return { .Count = feedback_count_of(aProjection, aTarget), .Qualified = !aProjection.Exact };
	}

	auto feedback_sorts_before(const feedback_sort_key& aLeft, const feedback_sort_key& aRight) noexcept -> bool
	{
		if (aLeft.Count != aRight.Count) {
			return aLeft.Count > aRight.Count;
		}
		return aLeft.Qualified && !aRight.Qualified;
	}
}
