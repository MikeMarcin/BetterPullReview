#include "bpr/application/feedback_counters.hpp"

#include "bpr/application/feedback_sources.hpp"

#include <algorithm>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace bpr::application {
	namespace {
		using core::feedback_counter_scope_kind;

		[[nodiscard]] auto same_unit(const core::feedback_counter_unit& aLeft, const core::feedback_counter_unit& aRight) -> bool
		{
			return aLeft.Id == aRight.Id && aLeft.Root == aRight.Root && aLeft.Label == aRight.Label && aLeft.Resolution == aRight.Resolution
				&& aLeft.Anchor == aRight.Anchor;
		}

		/// The counts a reader sees, whose pull request they are and what limits
		/// them; not the epoch, which every observation advances.
		[[nodiscard]] auto same_counts(const core::feedback_counter_tally& aLeft, const core::feedback_counter_tally& aRight) -> bool
		{
			return aLeft.Valid == aRight.Valid && aLeft.Scope == aRight.Scope && aLeft.Collection == aRight.Collection && aLeft.Labelled == aRight.Labelled
				&& aLeft.Plain == aRight.Plain
				&& aLeft.Custom == aRight.Custom && aLeft.Unclassified == aRight.Unclassified && aLeft.Unattributed == aRight.Unattributed
				&& aLeft.AttributionUnknown == aRight.AttributionUnknown;
		}

		[[nodiscard]] auto file_path(const core::feedback_counter_unit& aUnit) -> const std::string*
		{
			return aUnit.Anchor.Status == core::feedback_anchor_status::file && aUnit.Anchor.Path ? &*aUnit.Anchor.Path : nullptr;
		}

		/// Every folder above \p aPath, nearest last: `a/b/c.cpp` gives `a` and `a/b`.
		template <typename TVisit>
		void for_each_folder(const std::string& aPath, TVisit&& aVisit)
		{
			for (auto slash = aPath.find('/'); slash != std::string::npos; slash = aPath.find('/', slash + 1U)) {
				if (slash != 0U) {
					aVisit(aPath.substr(0U, slash));
				}
			}
		}
	} // namespace

	auto feedback_counter_model::observe(const core::cached_review_key& aScope, const github_pull_request_snapshot& aSnapshot, const bool aStale)
		-> feedback_counter_update
	{
		auto update  = feedback_counter_update{ .Generation = Generation, .Collection = Population.Collection };
		auto sources = observe_feedback_sources(aSnapshot);
		++Work.Observations;

		// A generated tour index is a reference to its stops, which are counted
		// as themselves; it is no conversation of its own.
		auto references = std::unordered_set<std::string>{ };
		for (const auto& conversation : sources.Conversations) {
			if (conversation.TourIndex) {
				references.insert(conversation.Facts.Id);
			}
		}
		update.TourIndexReferences = references.size();
		std::erase_if(sources.Posts, [&](const feedback_source_post& aPost) { return references.contains(aPost.Conversation); });
		auto facts = std::vector<core::feedback_conversation_facts>{ };
		facts.reserve(sources.Conversations.size());
		for (const auto& conversation : sources.Conversations) {
			if (!conversation.TourIndex) {
				facts.push_back(conversation.Facts);
			}
		}

		// The kept annotations make every unchanged body a reuse, not a parse.
		// [[BPR-1349]]. Read in place, not copied: the kept bodies are shared.
		const auto  replaced     = !Observed || Annotations.Scope != aScope;
		const auto  none         = core::feedback_annotation_set{ };
		const auto& current      = replaced ? none : Annotations;
		auto        refresh      = core::refresh_feedback_annotations(current, aScope, feedback_post_observations(sources));
		update.AnnotationRefusal = refresh.Refusal;
		if (refresh.Refusal != core::feedback_refresh_refusal::none) {
			return update;
		}
		update.Reparsed = refresh.Recognized.size();
		update.Reused   = refresh.Reused;
		Work.BodiesParsed += update.Reparsed;
		Work.BodiesReused += update.Reused;
		Work.BodiesHashed += refresh.Hashed;
		Work.RefreshesCarried += refresh.Carried ? 1U : 0U;
		auto adopted = core::adopt_feedback_refresh(current, std::move(refresh));
		if (!adopted) {
			update.AnnotationRefusal = core::feedback_refresh_refusal::invalid_scope;
			return update;
		}
		const auto collection    = aStale ? core::feedback_collection::stale : sources.Collection;
		auto       built         = core::build_feedback_population(*adopted, facts, collection);
		update.PopulationRefusal = built.Refusal;
		if (built.Refusal != core::feedback_population_refusal::none) {
			return update;
		}

		// Which paths changed, and whether a change reaches every scope: a new
		// pull request, a new coverage, or a conversation whose file is unknown
		// (it limits every file and folder count).
		auto full = replaced || collection != Population.Collection;
		// [[BPR-1349]]. Whether a unit arrived, left or moved to another
		// anchor: only then do the units' places, files and scopes change.
		auto       relaid       = full;
		auto       affected     = std::set<std::string>{ };
		auto       unattributed = false;
		const auto touch        = [&](const core::feedback_counter_unit& aUnit) {
			if (aUnit.Anchor.Status == core::feedback_anchor_status::unknown) {
				full = true;
			}
			if (const auto* path = file_path(aUnit)) {
				affected.insert(*path);
			} else {
				unattributed = true;
			}
		};
		if (!full) {
			auto before = std::unordered_map<std::string_view, const core::feedback_counter_unit*>{ };
			for (const auto& unit : Population.Units) {
				before.emplace(unit.Id, &unit);
			}
			Work.UnitsCompared += built.Population.Units.size();
			for (const auto& unit : built.Population.Units) {
				const auto found = before.find(unit.Id);
				if (found == before.end()) {
					touch(unit);
					relaid = true;
					continue;
				}
				if (!same_unit(*found->second, unit)) {
					touch(*found->second);
					touch(unit);
					relaid = relaid || found->second->Anchor != unit.Anchor;
				}
				before.erase(found);
			}
			for (const auto& [id, unit] : before) {
				touch(*unit);
				relaid = true;
			}
		}
		// A change that reaches every scope reads them all afresh.
		relaid = relaid || full;

		Annotations = std::move(*adopted);
		Population  = std::move(built.Population);
		Observed    = true;
		// The same units at the same anchors keep their places in the
		// population, which is in identity order, so the index still holds.
		if (relaid) {
			index_units();
		}
		update.Admitted   = true;
		update.Collection = Population.Collection;

		// Every scope a conversation is in now; unchanged, and every kept
		// tally still present, unless the units were laid out again.
		auto present = std::set<scope_key>{ };
		if (relaid) {
			present.insert({ feedback_counter_scope_kind::pull_request, std::string{ } });
			if (!Unattributed.empty()) {
				present.insert({ feedback_counter_scope_kind::unattributed, std::string{ } });
			}
			for (const auto& [path, units] : UnitsByPath) {
				present.insert({ feedback_counter_scope_kind::file, path });
				for_each_folder(path, [&](std::string aFolder) { present.insert({ feedback_counter_scope_kind::folder, std::move(aFolder) }); });
			}
		}
		auto retally = std::set<scope_key>{ };
		if (full) {
			retally = present;
		} else {
			retally.insert({ feedback_counter_scope_kind::pull_request, std::string{ } });
			if (unattributed) {
				retally.insert({ feedback_counter_scope_kind::unattributed, std::string{ } });
			}
			for (const auto& path : affected) {
				retally.insert({ feedback_counter_scope_kind::file, path });
				for_each_folder(path, [&](std::string aFolder) { retally.insert({ feedback_counter_scope_kind::folder, std::move(aFolder) }); });
			}
		}

		for (auto kept = Tallies.begin(); relaid && kept != Tallies.end();) {
			if (!present.contains(kept->first)) {
				update.Removed.push_back(kept->second.Where);
				kept = Tallies.erase(kept);
			} else {
				++kept;
			}
		}
		for (const auto& key : retally) {
			if (relaid && !present.contains(key)) {
				continue;
			}
			auto       tally = tally_scope(key);
			const auto kept  = Tallies.find(key);
			if (kept == Tallies.end() || !same_counts(kept->second, tally)) {
				update.Changed.push_back(tally.Where);
			}
			Tallies.insert_or_assign(key, std::move(tally));
			++update.Retallied;
			++Work.ScopesRetallied;
		}
		// Every kept tally names the same accepted observation, so counts read
		// together are one snapshot.
		for (auto& [key, tally] : Tallies) {
			tally.Epoch      = Population.Epoch;
			tally.Collection = Population.Collection;
		}
		if (!update.Changed.empty() || !update.Removed.empty()) {
			++Generation;
		}
		update.Generation = Generation;
		return update;
	}

	auto feedback_counter_model::scopes() const -> std::vector<core::feedback_counter_scope>
	{
		auto result = std::vector<core::feedback_counter_scope>{ };
		result.reserve(Tallies.size());
		for (const auto& [key, tally] : Tallies) {
			result.push_back(tally.Where);
		}
		return result;
	}

	auto feedback_counter_model::tally(const core::feedback_counter_scope& aScope) const -> const core::feedback_counter_tally*
	{
		const auto found = Tallies.find({ aScope.Kind, aScope.Path });
		return found == Tallies.end() ? nullptr : &found->second;
	}

	auto feedback_counter_model::project(const core::feedback_counter_scope& aScope, const std::span<const core::feedback_label> aColumns,
										 const core::feedback_resolution_mode aMode) const -> std::optional<core::feedback_counter_projection>
	{
		const auto* kept = tally(aScope);
		if (kept == nullptr) {
			return std::nullopt;
		}
		auto projection = core::project_feedback_counters(*kept, aColumns, aMode);
		if (projection.Refusal != core::feedback_projection_refusal::none) {
			return std::nullopt;
		}
		return projection;
	}

	void feedback_counter_model::index_units()
	{
		++Work.UnitIndexes;
		UnitsByPath.clear();
		UnknownAnchors.clear();
		Unattributed.clear();
		for (std::size_t index = 0U; index < Population.Units.size(); ++index) {
			const auto& unit = Population.Units[index];
			if (const auto* path = file_path(unit)) {
				UnitsByPath[*path].push_back(index);
				continue;
			}
			Unattributed.push_back(index);
			if (unit.Anchor.Status == core::feedback_anchor_status::unknown) {
				UnknownAnchors.push_back(index);
			}
		}
	}

	auto feedback_counter_model::tally_of(const core::feedback_counter_scope& aScope) const -> core::feedback_counter_tally
	{
		if (const auto* kept = tally(aScope)) {
			return *kept;
		}
		++Work.ScopesTalliedOnRequest;
		return tally_scope({ aScope.Kind, aScope.Path });
	}

	/// One scope's tally from only the conversations it can hold, plus the
	/// conversations whose file is unknown, which limit every file, folder and
	/// directory count; the core counts them exactly as it counts the whole
	/// population.
	auto feedback_counter_model::tally_scope(const scope_key& aKey) const -> core::feedback_counter_tally
	{
		const auto where = core::feedback_counter_scope{ .Kind = aKey.first, .Path = aKey.second };
		if (aKey.first == feedback_counter_scope_kind::pull_request) {
			Work.UnitsTallied += Population.Units.size();
			return core::tally_feedback_counters(Population, where);
		}
		auto       part = core::feedback_population{ .Scope = Population.Scope, .Epoch = Population.Epoch, .Collection = Population.Collection, .Excluded = Population.Excluded };
		const auto take = [&](const std::vector<std::size_t>& aIndices) {
			for (const auto index : aIndices) {
				part.Units.push_back(Population.Units[index]);
			}
		};
		if (aKey.first == feedback_counter_scope_kind::unattributed) {
			// Every unit it holds, and nothing a file holds.
			take(Unattributed);
			Work.UnitsTallied += part.Units.size();
			return core::tally_feedback_counters(part, where);
		}
		if (aKey.first == feedback_counter_scope_kind::file) {
			if (const auto found = UnitsByPath.find(aKey.second); found != UnitsByPath.end()) {
				take(found->second);
			}
		} else if (aKey.first == feedback_counter_scope_kind::directory) {
			// The files directly in it: the core reads only those.
			const auto prefix = aKey.second.empty() ? std::string{ } : aKey.second + '/';
			for (auto found = UnitsByPath.lower_bound(prefix); found != UnitsByPath.end() && found->first.starts_with(prefix); ++found) {
				if (found->first.find('/', prefix.size()) == std::string::npos) {
					take(found->second);
				}
			}
		} else {
			const auto prefix = aKey.second + '/';
			for (auto found = UnitsByPath.lower_bound(prefix); found != UnitsByPath.end() && found->first.starts_with(prefix); ++found) {
				take(found->second);
			}
		}
		take(UnknownAnchors);
		Work.UnitsTallied += part.Units.size();
		return core::tally_feedback_counters(part, where);
	}
}
