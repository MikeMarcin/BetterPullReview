#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_EPOCH_TRANSITION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_EPOCH_TRANSITION_HPP

#include "bpr/core/epoch_model.hpp"

namespace bpr::core {
	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(const pull_request_authority& aAuthority) noexcept
		-> bool;
	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(const pull_request_source_revision& aRevision) noexcept
		-> bool;
	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(
		const accepted_pull_request_revision& aRevision) noexcept -> bool;
	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(const epoch_cause& aCause) noexcept -> bool;
	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(const epoch_catalog_identity& aCatalog) noexcept
		-> bool;
	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(const review_epoch_id& aEpoch) noexcept -> bool;
	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(const comparison_anchor_identity& aAnchor) noexcept
		-> bool;
	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(const comparison_anchor& aAnchor) noexcept -> bool;
	/// \brief Reports whether the available for diff condition holds.
	[[nodiscard]] auto available_for_diff(const comparison_pair& aPair) noexcept
		-> bool;
	/// \brief Computes the epoch commands result.
	[[nodiscard]] auto epoch_commands(const epoch_state& aState) noexcept
		-> std::array<epoch_command_projection, 6>;

	/// \brief Computes the material relation result.
	[[nodiscard]] auto material_relation(
		const pull_request_authority&                 aCurrentAuthority,
		const std::optional<epoch_material_identity>& aCurrent,
		const pull_request_authority&                 aPriorAuthority,
		const std::optional<epoch_material_identity>& aPrior) noexcept
		-> epoch_material_relation;

	enum class epoch_preparation_effect : std::uint8_t {
		none,
		retain_revision,
	};

	/// \brief Describes the epoch preparation catalog observation record.
	struct epoch_preparation_catalog_observation
	{
		std::optional<review_epoch_summary> LatestLive;
		retention_rehydration_requirement   RetentionRehydration = retention_rehydration_requirement::not_required;

		auto operator<=>(const epoch_preparation_catalog_observation&) const = default;
	};

	/// \brief Describes the planned epoch preparation record.
	struct planned_epoch_preparation
	{
		epoch_preparation_effect Effect    = epoch_preparation_effect::none;
		epoch_reason_code        Rejection = epoch_reason_code::none;

		auto operator<=>(const planned_epoch_preparation&) const = default;
	};

	// This is the single core-owned decision that bounds host preparation work.
	// In particular, an unchanged revision or association-only update must not
	// reserve an ordinal, fetch a pack, import objects, or create staging refs.
	/// \brief Computes the plan epoch preparation result.
	[[nodiscard]] auto plan_epoch_preparation(
		const pull_request_authority&                aAuthority,
		const accepted_pull_request_revision&        aRevision,
		const epoch_catalog_identity&                aCatalog,
		const epoch_preparation_catalog_observation& aObservation) noexcept
		-> planned_epoch_preparation;

	/// \brief Describes the planned epoch mutation record.
	struct planned_epoch_mutation
	{
		std::optional<epoch_catalog_mutation> Mutation;
		epoch_reason_code                     Rejection = epoch_reason_code::none;

		auto operator<=>(const planned_epoch_mutation&) const = default;
	};

	/// \brief Reports when an accepted live selection invalidates derived occurrence comparisons.
	[[nodiscard]] auto historical_comparison_selection_changed(
		const accepted_pull_request_revision&      aRevision,
		const std::optional<review_epoch_summary>& aLatestLive) noexcept -> bool;

	/// \brief Masks derived and unknown-base event comparison links, retaining observed-head links and evidence.
	void invalidate_derived_comparison_bindings(epoch_occurrence_history& aHistory) noexcept;

	/// \brief Plans a retained historical comparison without changing the accepted live revision.
	/// \details BaseBranch identifies the selected comparison branch only. The occurrence's
	/// optional historical base remains independent of the derived retained base.
	[[nodiscard]] auto plan_historical_epoch_mutation(
		const pull_request_authority&         aAuthority,
		const accepted_pull_request_revision& aLiveRevision,
		const pull_request_source_revision&   aHistoricalSource,
		const epoch_catalog_identity&         aCatalog,
		const epoch_occurrence&               aOccurrence,
		const git_commit_oid&                 aDerivedComparisonBase,
		const epoch_preparation_observation&  aObservation) noexcept -> planned_epoch_mutation;

	/// \brief Computes the plan epoch mutation result.
	[[nodiscard]] auto plan_epoch_mutation(
		const pull_request_authority&         aAuthority,
		const accepted_pull_request_revision& aRevision,
		const epoch_catalog_identity&         aCatalog,
		const epoch_preparation_observation&  aObservation) noexcept
		-> planned_epoch_mutation;

	/// \brief Describes the resolved comparison record.
	struct resolved_comparison
	{
		std::optional<comparison_pair> Pair;
		epoch_reason_code              Reason = epoch_reason_code::none;

		auto operator<=>(const resolved_comparison&) const = default;
	};

	/// \brief Computes the resolve comparison result.
	/// \details [[BPR-1560]]. Under `comparison_base_basis::recorded_base`
	/// Whole PR resolves to its pair with the base anchor unavailable and the
	/// reason `merge_base_unavailable`: the newest base is not the commit
	/// GitHub's Files changed compares from. Comparisons between updates are
	/// unaffected.
	[[nodiscard]] auto resolve_comparison(
		const pull_request_authority& aAuthority,
		const comparison_selection&   aSelection,
		const comparison_candidates&  aCandidates,
		comparison_base_basis         aBasis = comparison_base_basis::merge_base) noexcept -> resolved_comparison;

	/// \brief Applies an accepted event to the supplied state.
	[[nodiscard]] auto transition(epoch_state& aState, const epoch_event& aEvent)
		-> epoch_transition_output;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_EPOCH_TRANSITION_HPP
