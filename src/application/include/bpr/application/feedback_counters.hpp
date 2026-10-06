#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_FEEDBACK_COUNTERS_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_FEEDBACK_COUNTERS_HPP

#include "bpr/application/github_read_model.hpp"
#include "bpr/core/feedback_partition.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

/// \file
/// \brief [[BPR-1087]]. Accepted label buckets for one pull request, kept up to
/// date one accepted snapshot at a time (RFC-BPR-0049, R25-R27, R29).
/// \details The model adapts each accepted snapshot through the shared
/// feedback adapter, keeps the parsed annotations so an unchanged body is never
/// read again, and keeps one core tally per scope: the pull request, every file
/// a conversation is anchored to, and every folder above those files. A new
/// snapshot re-tallies only the scopes whose conversations changed, and says
/// which those were. Columns and the resolution mode are applied to a kept
/// tally, so choosing them reads no conversation and asks no provider.
namespace bpr::application {
	/// \brief What one observation changed.
	struct feedback_counter_update
	{
		/// False when the snapshot was refused: an annotation or population the
		/// core will not admit (a duplicate post, a contradictory duplicate).
		/// The kept counts then stay exactly as they were.
		bool                              Admitted          = false;
		core::feedback_refresh_refusal    AnnotationRefusal = core::feedback_refresh_refusal::none;
		core::feedback_population_refusal PopulationRefusal = core::feedback_population_refusal::none;
		/// Counts every admitted observation that changed any scope.
		std::uint64_t             Generation = 0U;
		core::feedback_collection Collection = core::feedback_collection::partial;
		/// Scopes whose counts or coverage changed, and scopes no conversation
		/// is in any more.
		std::vector<core::feedback_counter_scope> Changed;
		std::vector<core::feedback_counter_scope> Removed;
		/// Bodies the core read this time, and bodies it reused unread.
		std::size_t Reparsed = 0U;
		std::size_t Reused   = 0U;
		/// Scopes tallied again this time.
		std::size_t Retallied = 0U;
		/// Generated tour index comments set aside as references.
		std::size_t TourIndexReferences = 0U;
	};

	class feedback_counter_model final
	{
	public:
		/// \brief Adopts one accepted snapshot of the pull request \p aScope names.
		/// \details \p aStale marks a snapshot kept after its refresh failed, so
		/// every count says its coverage is stale. A different scope replaces
		/// everything kept.
		auto observe(const core::cached_review_key& aScope, const github_pull_request_snapshot& aSnapshot, bool aStale = false) -> feedback_counter_update;

		[[nodiscard]] auto generation() const noexcept -> std::uint64_t { return Generation; }
		[[nodiscard]] auto population() const noexcept -> const core::feedback_population& { return Population; }
		/// Every scope with a kept tally: the pull request first, then folders
		/// and files in path order.
		[[nodiscard]] auto scopes() const -> std::vector<core::feedback_counter_scope>;
		[[nodiscard]] auto tally(const core::feedback_counter_scope& aScope) const -> const core::feedback_counter_tally*;
		/// \brief The counts one scope shows for these columns and this mode,
		/// from its kept tally only.
		[[nodiscard]] auto project(const core::feedback_counter_scope& aScope, std::span<const core::feedback_label> aColumns,
								   core::feedback_resolution_mode aMode) const -> std::optional<core::feedback_counter_projection>;

	private:
		using scope_key = std::pair<core::feedback_counter_scope_kind, std::string>;

		[[nodiscard]] auto tally_scope(const scope_key& aKey) const -> core::feedback_counter_tally;
		void               index_units();

		core::feedback_annotation_set                     Annotations;
		core::feedback_population                         Population;
		bool                                              Observed = false;
		std::map<std::string, std::vector<std::size_t>>   UnitsByPath;
		std::vector<std::size_t>                          UnknownAnchors;
		std::map<scope_key, core::feedback_counter_tally> Tallies;
		std::uint64_t                                     Generation = 0U;
	};
}

#endif
