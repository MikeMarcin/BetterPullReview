#include "bpr/core/epoch_transition.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/sha256.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>

namespace bpr::core {
	namespace {
		enum class completion_match : std::uint8_t {
			current,
			stale,
			identity_mismatch,
		};

		[[nodiscard]] auto maintenance_target_digest(const epoch_maintenance_kind        aKind,
													 const epoch_catalog_identity&       aCatalog,
													 const repository_cache_scope_id     aCache,
													 const std::vector<review_epoch_id>& aEpochs) noexcept
			-> epoch_maintenance_target_digest
		{
			static constexpr std::array<std::uint8_t, 24> domain{
				'B',
				'P',
				'R',
				'-',
				'e',
				'p',
				'o',
				'c',
				'h',
				'-',
				'm',
				'a',
				'i',
				'n',
				't',
				'e',
				'n',
				'a',
				'n',
				'c',
				'e',
				'-',
				'v',
				'1',
			};
			constexpr auto fixedSize = domain.size() + 1U + 32U + 8U + 32U + 8U;
			constexpr auto epochSize = 32U + 32U + 8U;
			auto           bytes     = std::array<
				std::uint8_t,
				fixedSize + maximum_epoch_maintenance_target_count * epochSize>{ };
			if (aEpochs.size() > maximum_epoch_maintenance_target_count) {
				return { };
			}
			auto       size   = std::size_t{ 0U };
			const auto append = [&](const std::span<const std::uint8_t> aValues) {
				for (const auto value : aValues) {
					bytes[size++] = value;
				}
			};
			const auto appendU64 = [&](const std::uint64_t aValue) {
				for (std::size_t shift = 8U; shift != 0U; --shift) {
					bytes[size++] = static_cast<std::uint8_t>(
						aValue >> ((shift - 1U) * 8U));
				}
			};
			append(domain);
			bytes[size++] = static_cast<std::uint8_t>(aKind);
			append(aCatalog.Incarnation.bytes());
			appendU64(static_cast<std::uint64_t>(aCatalog.Revision.value()));
			append(aCache.bytes());
			appendU64(static_cast<std::uint64_t>(aEpochs.size()));
			for (const auto& epoch : aEpochs) {
				append(epoch.Authority.bytes());
				append(epoch.Incarnation.bytes());
				appendU64(static_cast<std::uint64_t>(epoch.Ordinal.value()));
			}
			return epoch_maintenance_target_digest{
				sha256(std::span{ bytes }.first(size))
			};
		}

		[[nodiscard]] constexpr auto valid_nonzero(const std::uint64_t aValue) noexcept
			-> bool
		{
			return aValue != 0U;
		}

		// A mode outside the enumeration is not a third mode to interpret; it
		// is a corrupt discriminant, and an authority carrying one is invalid.
		[[nodiscard]] constexpr auto valid_access_mode(
			const github_access_mode aMode) noexcept -> bool
		{
			switch (aMode) {
			case github_access_mode::authenticated:
			case github_access_mode::public_preview:
				return true;
			}
			return false;
		}

		// `pull_request_authority::Mode` is value-initialized because
		// `epoch_model.hpp` only declares the enumeration. That default is the
		// authenticated mode, which is what keeps every authority written
		// before the discriminant existed meaning exactly what it meant. If
		// the enumeration is ever reordered, this fails here rather than
		// silently re-reading every existing authority as a public one.
		static_assert(github_access_mode{ } == github_access_mode::authenticated,
					  "a value-initialized access mode is the authenticated mode");

		[[nodiscard]] constexpr auto valid_availability(
			const anchor_availability aAvailability) noexcept -> bool
		{
			switch (aAvailability) {
			case anchor_availability::available:
			case anchor_availability::unavailable:
			case anchor_availability::pruned:
			case anchor_availability::cache_deleted:
			case anchor_availability::corrupt:
			case anchor_availability::never_observed:
				return true;
			}
			return false;
		}

		[[nodiscard]] constexpr auto valid_review_state(
			const github_review_state aState) noexcept -> bool
		{
			switch (aState) {
			case github_review_state::pending:
			case github_review_state::approved:
			case github_review_state::changes_requested:
			case github_review_state::commented:
			case github_review_state::dismissed:
				return true;
			}
			return false;
		}

		[[nodiscard]] constexpr auto eligible_review_state(
			const github_review_state aState) noexcept -> bool
		{
			return valid_review_state(aState) && aState != github_review_state::pending;
		}

		[[nodiscard]] constexpr auto valid_lineage(const head_lineage aLineage) noexcept
			-> bool
		{
			return aLineage == head_lineage::not_applicable
				|| aLineage == head_lineage::descendant
				|| aLineage == head_lineage::non_fast_forward
				|| aLineage == head_lineage::unknown;
		}

		[[nodiscard]] constexpr auto valid_rehydration_requirement(
			const retention_rehydration_requirement aRequirement) noexcept -> bool
		{
			return aRequirement == retention_rehydration_requirement::not_required
				|| aRequirement == retention_rehydration_requirement::required;
		}

		[[nodiscard]] constexpr auto valid_alias(const named_comparison aAlias) noexcept
			-> bool
		{
			return aAlias == named_comparison::whole_pr
				|| aAlias == named_comparison::previous_upload_to_latest
				|| aAlias == named_comparison::my_last_reviewed_to_latest;
		}

		[[nodiscard]] constexpr auto valid_side(const comparison_side aSide) noexcept
			-> bool
		{
			return aSide == comparison_side::left || aSide == comparison_side::right;
		}

		[[nodiscard]] auto valid_catalog_review(
			const catalog_review_association& aReview) noexcept -> bool
		{
			return aReview.Association.valid() && valid_nonzero(aReview.Review.value())
				&& valid_nonzero(aReview.Actor.value()) && valid(aReview.Order)
				&& eligible_review_state(aReview.State) && aReview.Commit.valid()
				&& valid_availability(aReview.Availability);
		}

		[[nodiscard]] auto same_provider_review(
			const current_viewer_review_candidate& aProvider,
			const catalog_review_association&      aCatalogReview) noexcept -> bool
		{
			return aProvider.Review == aCatalogReview.Review
				&& aProvider.Actor == aCatalogReview.Actor
				&& aProvider.Order == aCatalogReview.Order
				&& aProvider.State == aCatalogReview.State
				&& aProvider.Commit == aCatalogReview.Commit;
		}

		[[nodiscard]] auto valid_material(
			const epoch_material_identity& aMaterial) noexcept -> bool
		{
			return aMaterial.BaseTree.valid() && aMaterial.HeadTree.valid()
				&& aMaterial.BaseTree.algorithm() == aMaterial.HeadTree.algorithm()
				&& valid_nonzero(aMaterial.PolicyRevision.value());
		}

		[[nodiscard]] constexpr auto valid_cause(const epoch_cause& aCause) noexcept
			-> bool
		{
			const bool origin = aCause.Origin == epoch_origin::initial_observation
				|| aCause.Origin == epoch_origin::live_observation
				|| aCause.Origin == epoch_origin::explicit_historical_import
				|| aCause.Origin == epoch_origin::retention_rehydration;
			const bool base = aCause.Base == epoch_base_transition::not_applicable
				|| aCause.Base == epoch_base_transition::unchanged
				|| aCause.Base == epoch_base_transition::base_oid_changed
				|| aCause.Base == epoch_base_transition::base_branch_changed;
			const bool head = aCause.Head == epoch_head_transition::not_applicable
				|| aCause.Head == epoch_head_transition::unchanged
				|| aCause.Head == epoch_head_transition::descendant
				|| aCause.Head == epoch_head_transition::non_fast_forward
				|| aCause.Head == epoch_head_transition::lineage_unknown;
			const bool material = aCause.Material == epoch_material_relation::same
				|| aCause.Material == epoch_material_relation::different
				|| aCause.Material == epoch_material_relation::unknown;
			const bool primary = aCause.Primary == epoch_primary_cause::initial_observation
				|| aCause.Primary == epoch_primary_cause::explicit_historical_import
				|| aCause.Primary == epoch_primary_cause::base_branch_change
				|| aCause.Primary == epoch_primary_cause::force_push_rebase
				|| aCause.Primary == epoch_primary_cause::author_upload
				|| aCause.Primary == epoch_primary_cause::head_change_lineage_unavailable
				|| aCause.Primary == epoch_primary_cause::base_update
				|| aCause.Primary == epoch_primary_cause::retention_rehydration;
			if (!(origin && base && head && material && primary)) {
				return false;
			}
			if (aCause.Origin == epoch_origin::initial_observation) {
				return aCause.Base == epoch_base_transition::not_applicable
					&& aCause.Head == epoch_head_transition::not_applicable
					&& aCause.Material == epoch_material_relation::unknown
					&& aCause.Primary == epoch_primary_cause::initial_observation;
			}
			if (aCause.Origin == epoch_origin::explicit_historical_import) {
				return aCause.Base == epoch_base_transition::not_applicable
					&& aCause.Head == epoch_head_transition::not_applicable
					&& aCause.Primary == epoch_primary_cause::explicit_historical_import;
			}
			if (aCause.Origin == epoch_origin::retention_rehydration) {
				return aCause.Base == epoch_base_transition::unchanged
					&& aCause.Head == epoch_head_transition::unchanged
					&& aCause.Primary == epoch_primary_cause::retention_rehydration;
			}
			if (aCause.Base == epoch_base_transition::not_applicable
				|| aCause.Head == epoch_head_transition::not_applicable) {
				return false;
			}
			if (aCause.Base == epoch_base_transition::base_branch_changed) {
				return aCause.Primary == epoch_primary_cause::base_branch_change;
			}
			if (aCause.Head == epoch_head_transition::non_fast_forward) {
				return aCause.Primary == epoch_primary_cause::force_push_rebase;
			}
			if (aCause.Head == epoch_head_transition::descendant) {
				return aCause.Primary == epoch_primary_cause::author_upload;
			}
			if (aCause.Head == epoch_head_transition::lineage_unknown) {
				return aCause.Primary
					== epoch_primary_cause::head_change_lineage_unavailable;
			}
			return aCause.Head == epoch_head_transition::unchanged
				&& aCause.Base == epoch_base_transition::base_oid_changed
				&& aCause.Primary == epoch_primary_cause::base_update;
		}

		[[nodiscard]] constexpr auto live(const review_epoch_summary& aEpoch) noexcept
			-> bool
		{
			return aEpoch.Cause.Origin != epoch_origin::explicit_historical_import;
		}

		[[nodiscard]] auto valid_summary(
			const pull_request_authority& aAuthority,
			const epoch_catalog_identity& aCatalog,
			const review_epoch_summary&   aEpoch) noexcept -> bool
		{
			return valid(aEpoch.Id) && aEpoch.Id.Authority == aAuthority.Scope
				&& aEpoch.Id.Incarnation == aCatalog.Incarnation
				&& valid(aEpoch.Source)
				&& aEpoch.BaseBranch.valid() && aEpoch.Base.valid() && aEpoch.Head.valid()
				&& aEpoch.Base.algorithm() == aEpoch.Head.algorithm()
				&& (!aEpoch.Material.has_value()
					|| (valid_material(*aEpoch.Material)
						&& aEpoch.Material->BaseTree.algorithm()
							== aEpoch.Base.algorithm()))
				&& valid(aEpoch.ObservedAt) && valid_cause(aEpoch.Cause)
				&& valid_availability(aEpoch.BaseAvailability)
				&& valid_availability(aEpoch.HeadAvailability);
		}

		[[nodiscard]] auto epoch_anchor(
			const review_epoch_summary&  aEpoch,
			const comparison_anchor_kind aKind) noexcept -> comparison_anchor
		{
			return comparison_anchor{
				.Identity = comparison_anchor_identity{
					.Authority   = aEpoch.Id.Authority,
					.Incarnation = aEpoch.Id.Incarnation,
					.Kind        = aKind,
					.Epoch       = aEpoch.Id.Ordinal,
				},
				.Commit       = aKind == comparison_anchor_kind::epoch_base ? aEpoch.Base : aEpoch.Head,
				.Availability = aKind == comparison_anchor_kind::epoch_base ? aEpoch.BaseAvailability : aEpoch.HeadAvailability,
			};
		}

		[[nodiscard]] auto reject(const epoch_reason_code aReason)
			-> epoch_transition_output
		{
			epoch_transition_output output;
			output.Changed = epoch_changed_domain::diagnostics;
			output.Diagnostics.push_back(epoch_diagnostic{ .Code = aReason });
			return output;
		}

		auto diagnose(epoch_transition_output& aOutput, const epoch_reason_code aReason)
			-> void
		{
			aOutput.Diagnostics.push_back(epoch_diagnostic{ .Code = aReason });
			aOutput.Changed = aOutput.Changed | epoch_changed_domain::diagnostics;
		}

		auto set_status(
			epoch_state&             aState,
			epoch_transition_output& aOutput,
			const epoch_status       aStatus) -> void
		{
			aState.Status    = aStatus;
			aOutput.Status   = epoch_status_delta{ .Status = aStatus };
			aOutput.Commands = epoch_command_delta{ .Commands = epoch_commands(aState) };
			aOutput.Changed  = aOutput.Changed | epoch_changed_domain::status
				| epoch_changed_domain::commands;
		}

		[[nodiscard]] auto advanced(const epoch_generation aGeneration) noexcept
			-> std::optional<epoch_generation>
		{
			if (aGeneration.value() == std::numeric_limits<std::uint64_t>::max()) {
				return std::nullopt;
			}
			return epoch_generation{ aGeneration.value() + 1U };
		}

		[[nodiscard]] auto advanced(const comparison_generation aGeneration) noexcept
			-> std::optional<comparison_generation>
		{
			if (aGeneration.value() == std::numeric_limits<std::uint64_t>::max()) {
				return std::nullopt;
			}
			return comparison_generation{ aGeneration.value() + 1U };
		}

		[[nodiscard]] auto advanced(const comparison_revision aRevision) noexcept
			-> std::optional<comparison_revision>
		{
			if (aRevision.value() >= maximum_comparison_revision) {
				return std::nullopt;
			}
			return comparison_revision{ aRevision.value() + 1 };
		}

		[[nodiscard]] auto advanced(const epoch_catalog_revision aRevision) noexcept
			-> std::optional<epoch_catalog_revision>
		{
			if (aRevision.value() >= maximum_epoch_catalog_revision) {
				return std::nullopt;
			}
			return epoch_catalog_revision{ aRevision.value() + 1 };
		}

		[[nodiscard]] auto request_available(const epoch_state& aState) noexcept -> bool
		{
			return !aState.RequestCounterExhausted
				&& valid_nonzero(aState.NextRequest.value());
		}

		[[nodiscard]] auto take_request(epoch_state& aState) noexcept -> epoch_request_id
		{
			const auto result = aState.NextRequest;
			if (result.value() == std::numeric_limits<std::uint64_t>::max()) {
				aState.RequestCounterExhausted = true;
			} else {
				aState.NextRequest = epoch_request_id{ result.value() + 1U };
			}
			return result;
		}

		[[nodiscard]] auto current_load_context(
			const epoch_state& aState, const epoch_request_id aRequest)
			-> epoch_load_context
		{
			return epoch_load_context{
				.Request            = aRequest,
				.ApplicationSession = aState.ApplicationSession,
				.Authority          = *aState.Authority,
				.Revision           = *aState.AcceptedRevision,
				.Generation         = aState.EpochGeneration,
			};
		}

		[[nodiscard]] auto current_preparation_context(
			const epoch_state& aState, const epoch_request_id aRequest)
			-> epoch_preparation_context
		{
			return epoch_preparation_context{
				.Request                 = aRequest,
				.ApplicationSession      = aState.ApplicationSession,
				.Authority               = *aState.Authority,
				.Revision                = *aState.AcceptedRevision,
				.Generation              = aState.EpochGeneration,
				.CatalogIncarnation      = aState.Catalog->Incarnation,
				.ExpectedCatalogRevision = aState.Catalog->Revision,
			};
		}

		[[nodiscard]] auto current_operation_context(
			const epoch_state& aState, const pending_epoch_ingestion& aPending)
			-> epoch_operation_context
		{
			return epoch_operation_context{
				.Request                 = aPending.Request,
				.ApplicationSession      = aState.ApplicationSession,
				.Authority               = *aState.Authority,
				.Revision                = *aState.AcceptedRevision,
				.Generation              = aState.EpochGeneration,
				.CatalogIncarnation      = aState.Catalog->Incarnation,
				.ExpectedCatalogRevision = aState.Catalog->Revision,
				.Operation               = aPending.Operation,
			};
		}

		[[nodiscard]] auto current_comparison_context(
			const epoch_state& aState, const epoch_request_id aRequest)
			-> comparison_context
		{
			return comparison_context{
				.Request            = aRequest,
				.ApplicationSession = aState.ApplicationSession,
				.Authority          = *aState.Authority,
				.Source             = aState.AcceptedRevision->Source,
				.CatalogIncarnation = aState.Catalog->Incarnation,
				.CatalogRevision    = aState.Catalog->Revision,
				.Generation         = aState.ComparisonGeneration,
				.Selection          = aState.RequestedSelection,
			};
		}

		[[nodiscard]] auto current_history_context(
			const epoch_state& aState, const pending_epoch_history_page& aPending)
			-> epoch_history_page_context
		{
			return epoch_history_page_context{
				.Request            = aPending.Request,
				.ApplicationSession = aState.ApplicationSession,
				.Authority          = *aState.Authority,
				.Source             = aState.AcceptedRevision->Source,
				.Generation         = aPending.Generation,
				.Catalog            = aPending.Catalog,
				.Navigation         = aPending.Navigation,
				.Cursor             = aPending.Cursor,
				.Limit              = aPending.Limit,
			};
		}

		[[nodiscard]] auto valid_maintenance_target(
			const pull_request_authority&       aAuthority,
			const epoch_catalog_identity&       aCatalog,
			const epoch_maintenance_kind        aKind,
			const repository_cache_scope_id     aCache,
			const std::vector<review_epoch_id>& aEpochs) noexcept -> bool
		{
			if (aCache != aAuthority.Cache) {
				return false;
			}
			if (aKind == epoch_maintenance_kind::delete_repository_cache) {
				return aEpochs.empty();
			}
			if (aKind != epoch_maintenance_kind::prune_epochs || aEpochs.empty()
				|| aEpochs.size() > maximum_epoch_maintenance_target_count) {
				return false;
			}
			for (std::size_t index = 0U; index < aEpochs.size(); ++index) {
				const auto& epoch = aEpochs[index];
				if (!valid(epoch) || epoch.Authority != aAuthority.Scope
					|| epoch.Incarnation != aCatalog.Incarnation
					|| (index != 0U && !(aEpochs[index - 1U] < epoch))) {
					return false;
				}
			}
			return true;
		}

		[[nodiscard]] auto maintenance_context_match(const epoch_state&               aState,
													 const epoch_maintenance_context& aActual) -> completion_match
		{
			if (!aState.PendingMaintenance.has_value()) {
				return completion_match::stale;
			}
			const auto& pending = *aState.PendingMaintenance;
			if (aActual.Request != pending.Request
				|| aActual.Generation != pending.Generation) {
				return completion_match::stale;
			}
			if (!aState.Authority || !aState.AcceptedRevision || !aState.Catalog
				|| aActual.ApplicationSession != aState.ApplicationSession
				|| aActual.Authority != *aState.Authority
				|| aActual.Source != aState.AcceptedRevision->Source
				|| aActual.Generation != aState.EpochGeneration
				|| aActual.Catalog != *aState.Catalog || aActual.Catalog != pending.Catalog
				|| aActual.Kind != pending.Kind || aActual.Cache != pending.Cache
				|| aActual.Epochs.size() != pending.EpochCount
				|| !valid_maintenance_target(aActual.Authority,
											 aActual.Catalog,
											 aActual.Kind,
											 aActual.Cache,
											 aActual.Epochs)
				|| aActual.TargetDigest != pending.TargetDigest
				|| aActual.TargetDigest
					!= maintenance_target_digest(aActual.Kind,
												 aActual.Catalog,
												 aActual.Cache,
												 aActual.Epochs)) {
				return completion_match::identity_mismatch;
			}
			return completion_match::current;
		}

		template <typename Context>
		[[nodiscard]] auto context_match(
			const std::optional<pending_epoch_ingestion>& aPending,
			const epoch_ingestion_phase                   aPhase,
			const Context&                                aActual,
			const Context&                                aExpected) noexcept -> completion_match
		{
			if (!aPending.has_value() || aPending->Phase != aPhase) {
				return completion_match::stale;
			}
			if (aActual.Request != aPending->Request
				|| aActual.Generation != aPending->Generation) {
				return completion_match::stale;
			}
			return aActual == aExpected ? completion_match::current
										: completion_match::identity_mismatch;
		}

		[[nodiscard]] auto preparation_context_match(
			const epoch_state& aState, const epoch_preparation_context& aActual)
			-> completion_match
		{
			if (
				!aState.PendingIngestion.has_value()
				|| aState.PendingIngestion->Phase != epoch_ingestion_phase::preparing) {
				return completion_match::stale;
			}
			if (
				aActual.Request != aState.PendingIngestion->Request
				|| aActual.Generation != aState.PendingIngestion->Generation) {
				return completion_match::stale;
			}
			if (
				!aState.Authority.has_value() || !aState.AcceptedRevision.has_value()
				|| !aState.Catalog.has_value()) {
				return completion_match::identity_mismatch;
			}
			return aActual
					== current_preparation_context(
						aState, aState.PendingIngestion->Request)
				? completion_match::current
				: completion_match::identity_mismatch;
		}

		[[nodiscard]] auto comparison_context_match(
			const epoch_state& aState, const comparison_context& aActual)
			-> completion_match
		{
			if (!aState.PendingComparison.has_value()) {
				return completion_match::stale;
			}
			if (aActual.Request != aState.PendingComparison->Request
				|| aActual.Generation != aState.PendingComparison->Generation) {
				return completion_match::stale;
			}
			return aActual
					== current_comparison_context(
						aState, aState.PendingComparison->Request)
				? completion_match::current
				: completion_match::identity_mismatch;
		}

		[[nodiscard]] auto history_context_match(
			const epoch_state& aState, const epoch_history_page_context& aActual)
			-> completion_match
		{
			if (!aState.PendingHistory.has_value()) {
				return completion_match::stale;
			}
			if (aActual.Request != aState.PendingHistory->Request
				|| aActual.Generation != aState.PendingHistory->Generation) {
				return completion_match::stale;
			}
			return aActual == current_history_context(aState, *aState.PendingHistory)
				? completion_match::current
				: completion_match::identity_mismatch;
		}

		[[nodiscard]] auto reject_match(const completion_match aMatch)
			-> epoch_transition_output
		{
			return reject(
				aMatch == completion_match::identity_mismatch
					? epoch_reason_code::completion_identity_mismatch
					: epoch_reason_code::stale_completion);
		}

		auto cancel_pending_ingestion(
			const epoch_state& aState, epoch_transition_output& aOutput)
			-> void
		{
			if (aState.PendingIngestion.has_value()) {
				aOutput.Requests.emplace_back(
					cancel_epoch_work_request{
						.Request    = aState.PendingIngestion->Request,
						.Generation = aState.PendingIngestion->Generation,
					});
			}
		}

		auto retire_pending_comparison(
			epoch_state& aState, epoch_transition_output& aOutput) -> void
		{
			if (aState.PendingOccurrence) {
				aOutput.Requests.emplace_back(cancel_epoch_work_request{
					.Request    = aState.PendingOccurrence->Request,
					.Generation = aState.EpochGeneration,
				});
				aState.PendingOccurrence.reset();
			}
			if (!aState.PendingComparison.has_value()) {
				return;
			}
			aOutput.Requests.emplace_back(
				cancel_epoch_work_request{
					.Request    = aState.PendingComparison->Request,
					.Generation = aState.PendingComparison->Generation,
				});
			aState.PendingComparison.reset();
		}

		auto retire_pending_history(
			epoch_state& aState, epoch_transition_output& aOutput) -> void
		{
			if (!aState.PendingHistory.has_value()) {
				return;
			}
			const auto pending = *aState.PendingHistory;
			aOutput.Requests.emplace_back(
				cancel_epoch_work_request{
					.Request    = pending.Request,
					.Generation = pending.Generation,
				});
			aOutput.History = epoch_history_page_delta{
				.Phase           = epoch_history_page_phase::cancelled,
				.Catalog         = pending.Catalog,
				.Navigation      = pending.Navigation,
				.RequestedCursor = pending.Cursor,
				.Limit           = pending.Limit,
				.Reason          = epoch_reason_code::cancelled,
			};
			aOutput.Changed = aOutput.Changed | epoch_changed_domain::history;
			aState.PendingHistory.reset();
		}

		auto invalidate_history_window(
			epoch_state& aState, epoch_transition_output& aOutput) -> void
		{
			retire_pending_history(aState, aOutput);
			if (aState.HistoryCatalog.has_value()) {
				aOutput.History = epoch_history_page_delta{
					.Phase      = epoch_history_page_phase::invalidated,
					.Catalog    = *aState.HistoryCatalog,
					.Navigation = aState.HistoryNavigation.value_or(
						epoch_history_navigation::newest),
					.Reason = epoch_reason_code::catalog_revision_mismatch,
				};
				aOutput.Changed = aOutput.Changed | epoch_changed_domain::history;
			}
			aState.HistoryCatalog.reset();
			aState.HistoryNavigation.reset();
			aState.HistoryContinuation.reset();
		}

		/// BPR-0725. The epoch line draws only rows a loaded page or an
		/// appended epoch delivers; the catalogue identity carries none. A
		/// cache hit prepares no mutation and appends nothing, so without this
		/// the line read "No updates loaded." beside a ready pair, and a first
		/// open showed only the one epoch it appended. Ingestion therefore asks
		/// for the newest page itself once the catalogue revision is settled —
		/// after a no-mutation prepare or a committed mutation, not at catalogue
		/// load, where a commit about to advance the revision would retire the
		/// read. The page must be a host read, so the core emits the same
		/// request an explicit "Return to newest" does and stays the only
		/// place that decides when history is read. A caller emits it before
		/// the comparison request so the actor's FIFO publishes the page
		/// before the ready status. A window already bound to this revision
		/// (possibly an older page the reviewer chose) or an in-flight page is
		/// left alone, and an exhausted request counter leaves the line empty
		/// rather than failing the ingestion that did succeed.
		auto request_newest_history_page(
			epoch_state& aState, epoch_transition_output& aOutput) -> void
		{
			if (!aState.Authority.has_value() || !aState.AcceptedRevision.has_value()
				|| !aState.Catalog.has_value() || aState.PendingHistory.has_value()
				|| aState.HistoryCatalog == aState.Catalog
				|| !request_available(aState)) {
				return;
			}
			aState.PendingHistory = pending_epoch_history_page{
				.Request    = take_request(aState),
				.Generation = aState.EpochGeneration,
				.Catalog    = *aState.Catalog,
				.Navigation = epoch_history_navigation::newest,
				.Limit      = maximum_epoch_history_page_size,
			};
			aOutput.Requests.emplace_back(
				load_epoch_history_page_request{
					.Context = current_history_context(
						aState, *aState.PendingHistory),
				});
			aOutput.History = epoch_history_page_delta{
				.Phase      = epoch_history_page_phase::loading,
				.Catalog    = *aState.Catalog,
				.Navigation = epoch_history_navigation::newest,
				.Limit      = maximum_epoch_history_page_size,
			};
			aOutput.Changed = aOutput.Changed | epoch_changed_domain::history;
		}

		auto retire_pending_maintenance(epoch_state&             aState,
										epoch_transition_output& aOutput) -> void
		{
			if (!aState.PendingMaintenance.has_value()) {
				return;
			}
			aOutput.Requests.emplace_back(cancel_epoch_work_request{
				.Request    = aState.PendingMaintenance->Request,
				.Generation = aState.PendingMaintenance->Generation,
			});
			aState.PendingMaintenance.reset();
			aOutput.Changed = aOutput.Changed | epoch_changed_domain::maintenance;
		}

		auto clear_catalog_bound_state(
			epoch_state& aState, epoch_transition_output& aOutput) -> void
		{
			invalidate_history_window(aState, aOutput);
			retire_pending_maintenance(aState, aOutput);
			aState.Catalog.reset();
			aState.RequestedSelection = named_selection(named_comparison::whole_pr);
			aState.AcceptedComparison.reset();
			aState.AcceptedComparisonRevision = comparison_revision{ };

			aOutput.Catalog   = epoch_catalog_delta{ };
			aOutput.Selection = comparison_selection_delta{
				.Selection = aState.RequestedSelection,
			};
			aOutput.Comparison = comparison_resolution_delta{
				.Selection = aState.RequestedSelection,
				.Revision  = aState.AcceptedComparisonRevision,
			};
			aOutput.Changed = aOutput.Changed | epoch_changed_domain::catalog
				| epoch_changed_domain::selection
				| epoch_changed_domain::comparison;
		}

		auto start_ingestion(
			epoch_state&             aState,
			epoch_transition_output& aOutput,
			const epoch_request_id   aRequest) -> void
		{
			const bool catalogReady = aState.Catalog.has_value();
			aState.PendingIngestion = pending_epoch_ingestion{
				.Request    = aRequest,
				.Generation = aState.EpochGeneration,
				.Phase      = catalogReady ? epoch_ingestion_phase::preparing
										   : epoch_ingestion_phase::loading_catalog,
			};
			if (catalogReady) {
				aOutput.Requests.emplace_back(
					prepare_epoch_revision_request{
						.Context = current_preparation_context(aState, aRequest),
					});
				set_status(aState, aOutput, epoch_status::preparing);
				return;
			}
			aOutput.Requests.emplace_back(
				load_epoch_catalog_request{
					.Context = current_load_context(aState, aRequest),
				});
			set_status(aState, aOutput, epoch_status::loading_catalog);
		}

		[[nodiscard]] auto restart_after_catalog_conflict(epoch_state& aState)
			-> epoch_transition_output
		{
			auto output = epoch_transition_output{ };
			aState.PendingIngestion.reset();
			retire_pending_comparison(aState, output);
			clear_catalog_bound_state(aState, output);
			diagnose(output, epoch_reason_code::catalog_conflict);

			const auto nextGeneration = advanced(aState.EpochGeneration);
			if (!nextGeneration.has_value()) {
				diagnose(output, epoch_reason_code::epoch_generation_exhausted);
				set_status(aState, output, epoch_status::failed);
				return output;
			}
			if (!request_available(aState)) {
				diagnose(output, epoch_reason_code::request_counter_exhausted);
				set_status(aState, output, epoch_status::failed);
				return output;
			}

			aState.EpochGeneration = *nextGeneration;
			const auto request     = take_request(aState);
			output.Authority       = epoch_authority_delta{
				.Authority  = aState.Authority,
				.Revision   = aState.AcceptedRevision,
				.Generation = aState.EpochGeneration,
			};
			output.Changed = output.Changed | epoch_changed_domain::authority;
			start_ingestion(aState, output, request);
			return output;
		}

		[[nodiscard]] auto clear_authority_fail_closed(
			epoch_state& aState, const epoch_reason_code aReason)
			-> epoch_transition_output
		{
			epoch_transition_output output;
			cancel_pending_ingestion(aState, output);
			retire_pending_comparison(aState, output);
			const auto previous = aState.Authority;
			aState.Authority.reset();
			aState.AcceptedRevision.reset();
			aState.PendingIngestion.reset();
			clear_catalog_bound_state(aState, output);
			output.Requests.insert(
				output.Requests.begin(),
				invalidate_epoch_authority_request{
					.ApplicationSession = aState.ApplicationSession,
					.PreviousAuthority  = previous,
					.NewGeneration      = aState.EpochGeneration,
				});
			output.Authority = epoch_authority_delta{ .Generation = aState.EpochGeneration };
			output.Changed   = output.Changed | epoch_changed_domain::authority;
			diagnose(output, aReason);
			set_status(aState, output, epoch_status::unavailable);
			return output;
		}

		[[nodiscard]] auto start_comparison(
			epoch_state&             aState,
			epoch_transition_output& aOutput,
			const epoch_request_id   aRequest,
			const bool               aPreserveAcceptedPair) -> bool
		{
			const auto generation = advanced(aState.ComparisonGeneration);
			if (!generation.has_value()) {
				retire_pending_comparison(aState, aOutput);
				aState.AcceptedComparison.reset();
				aOutput.Comparison = comparison_resolution_delta{
					.Selection = aState.RequestedSelection,
					.Revision  = aState.AcceptedComparisonRevision,
				};
				aOutput.Changed = aOutput.Changed | epoch_changed_domain::comparison;
				diagnose(aOutput, epoch_reason_code::comparison_generation_exhausted);
				set_status(aState, aOutput, epoch_status::unavailable);
				return false;
			}

			retire_pending_comparison(aState, aOutput);

			aState.ComparisonGeneration = *generation;
			aState.PendingComparison    = pending_comparison_resolution{
				.Request    = aRequest,
				.Generation = *generation,
			};
			if (!aPreserveAcceptedPair) {
				aState.AcceptedComparison.reset();
			}
			aOutput.Requests.emplace_back(
				resolve_comparison_request{
					.Context = current_comparison_context(aState, aRequest),
				});
			set_status(aState, aOutput, epoch_status::resolving);
			return true;
		}

		[[nodiscard]] auto next_catalog_revision(
			const epoch_catalog_revision aRevision,
			const epoch_catalog_revision aActual) noexcept -> bool
		{
			const auto expected = advanced(aRevision);
			return expected.has_value() && aActual == *expected;
		}

		auto clear_ingestion(
			epoch_state&                           aState,
			epoch_transition_output&               aOutput,
			const epoch_status                     aStatus,
			const std::optional<epoch_reason_code> aDiagnostic = std::nullopt) -> void
		{
			aState.PendingIngestion.reset();
			if (aDiagnostic.has_value()) {
				diagnose(aOutput, *aDiagnostic);
			}
			set_status(aState, aOutput, aStatus);
		}

		[[nodiscard]] auto operation_payload_matches(
			const epoch_state&             aState,
			const epoch_operation_context& aContext,
			const epoch_catalog_mutation&  aMutation) -> completion_match
		{
			if (!aState.PendingIngestion.has_value()
				|| aState.PendingIngestion->Phase != epoch_ingestion_phase::committing) {
				return completion_match::stale;
			}
			const auto& pending = *aState.PendingIngestion;
			if (aContext.Request != pending.Request
				|| aContext.Generation != pending.Generation) {
				return completion_match::stale;
			}
			if (aContext != current_operation_context(aState, pending)
				|| !pending.Mutation.has_value() || aMutation != *pending.Mutation) {
				return completion_match::identity_mismatch;
			}
			return completion_match::current;
		}

		/// \brief [[BPR-1557]]. What Whole PR compares from, whatever is
		/// selected: today the latest live epoch's base. `select_pair`'s Whole
		/// PR takes its left side from here and the Updates line starts with
		/// the same anchor, so a change to the base the comparison uses moves
		/// the line's base with it.
		[[nodiscard]] auto pull_request_base(
			const pull_request_authority& aAuthority,
			const comparison_candidates&  aCandidates) noexcept -> std::optional<comparison_anchor>
		{
			if (!aCandidates.LatestLive || !live(*aCandidates.LatestLive)
				|| !valid_summary(aAuthority, aCandidates.Catalog, *aCandidates.LatestLive)) {
				return std::nullopt;
			}
			return epoch_anchor(*aCandidates.LatestLive, comparison_anchor_kind::epoch_base);
		}

		[[nodiscard]] auto select_pair(
			const pull_request_authority& aAuthority,
			const comparison_selection&   aSelection,
			const comparison_candidates&  aCandidates) noexcept -> resolved_comparison
		{
			if (aCandidates.LatestLive.has_value()
				&& !valid_summary(aAuthority, aCandidates.Catalog, *aCandidates.LatestLive)) {
				return { .Reason = epoch_reason_code::invalid_observation };
			}
			const auto latestValid = aCandidates.LatestLive.has_value() && live(*aCandidates.LatestLive);

			if (const auto* explicitPair = std::get_if<explicit_comparison_selection>(&aSelection)) {
				if (!aCandidates.ExplicitLeft.has_value()
					|| !aCandidates.ExplicitRight.has_value()) {
					return { .Reason = epoch_reason_code::unavailable };
				}
				if (aCandidates.ExplicitLeft->Identity != explicitPair->Left
					|| aCandidates.ExplicitRight->Identity != explicitPair->Right
					|| !valid(*aCandidates.ExplicitLeft)
					|| !valid(*aCandidates.ExplicitRight)) {
					return { .Reason = epoch_reason_code::invalid_selection };
				}
				return {
					.Pair = comparison_pair{
						.Left  = *aCandidates.ExplicitLeft,
						.Right = *aCandidates.ExplicitRight,
					},
				};
			}

			const auto& named = std::get<named_comparison_selection>(aSelection);
			if (!valid_alias(named.Alias)) {
				return { .Reason = epoch_reason_code::invalid_selection };
			}
			if (!latestValid) {
				return { .Reason = epoch_reason_code::unavailable };
			}
			const auto latestHead = epoch_anchor(
				*aCandidates.LatestLive, comparison_anchor_kind::epoch_head);

			switch (named.Alias) {
			case named_comparison::whole_pr: {
				const auto base = pull_request_base(aAuthority, aCandidates);
				if (!base) {
					return { .Reason = epoch_reason_code::unavailable };
				}
				return {
					.Pair = comparison_pair{
						.Left  = *base,
						.Right = latestHead,
					},
				};
			}
			case named_comparison::previous_upload_to_latest:
				if (aCandidates.PreviousDistinctHead.has_value()
					&& !valid_summary(
						aAuthority, aCandidates.Catalog, *aCandidates.PreviousDistinctHead)) {
					return { .Reason = epoch_reason_code::invalid_observation };
				}
				if (!aCandidates.PreviousDistinctHead.has_value()
					|| !live(*aCandidates.PreviousDistinctHead)
					|| aCandidates.PreviousDistinctHead->Id.Ordinal.value()
						>= aCandidates.LatestLive->Id.Ordinal.value()
					|| aCandidates.PreviousDistinctHead->Head
						== aCandidates.LatestLive->Head) {
					return { .Reason = epoch_reason_code::unavailable };
				}
				return {
					.Pair = comparison_pair{
						.Left = epoch_anchor(
							*aCandidates.PreviousDistinctHead,
							comparison_anchor_kind::epoch_head),
						.Right = latestHead,
					},
				};
			case named_comparison::my_last_reviewed_to_latest:
				if (aCandidates.CurrentViewerReview.has_value()
					&& (!valid(*aCandidates.CurrentViewerReview)
						|| aCandidates.CurrentViewerReview->Identity.Kind
							!= comparison_anchor_kind::review_commit)) {
					return { .Reason = epoch_reason_code::invalid_observation };
				}
				if (!aCandidates.CurrentViewerReview.has_value()) {
					return { .Reason = epoch_reason_code::unavailable };
				}
				return {
					.Pair = comparison_pair{
						.Left  = *aCandidates.CurrentViewerReview,
						.Right = latestHead,
					},
				};
			}
			return { .Reason = epoch_reason_code::invalid_selection };
		}

		[[nodiscard]] auto handle(
			epoch_state& aState, const pull_request_revision_accepted_event& aEvent)
			-> epoch_transition_output
		{
			if (aEvent.ApplicationSession != aState.ApplicationSession
				|| !valid_nonzero(aState.ApplicationSession.value())
				|| !valid(aEvent.Authority) || !valid(aEvent.Revision)
				|| (aEvent.Revision.CurrentViewerReview.has_value()
					&& aEvent.Revision.CurrentViewerReview->Actor
						!= aEvent.Authority.Viewer)) {
				return reject(epoch_reason_code::invalid_revision);
			}
			if (aState.Authority == aEvent.Authority
				&& aState.AcceptedRevision == aEvent.Revision) {
				return { };
			}

			const bool authorityChanged    = !aState.Authority.has_value() || *aState.Authority != aEvent.Authority;
			const auto nextEpochGeneration = advanced(aState.EpochGeneration);
			if (!nextEpochGeneration.has_value()) {
				return aState.Authority.has_value()
					? clear_authority_fail_closed(
						  aState, epoch_reason_code::epoch_generation_exhausted)
					: reject(epoch_reason_code::epoch_generation_exhausted);
			}
			std::optional<comparison_generation> nextComparisonGeneration;
			if (authorityChanged) {
				nextComparisonGeneration = advanced(aState.ComparisonGeneration);
				if (!nextComparisonGeneration.has_value()) {
					return aState.Authority.has_value()
						? clear_authority_fail_closed(
							  aState, epoch_reason_code::comparison_generation_exhausted)
						: reject(epoch_reason_code::comparison_generation_exhausted);
				}
			}
			if (!request_available(aState)) {
				return aState.Authority.has_value()
					? clear_authority_fail_closed(
						  aState, epoch_reason_code::request_counter_exhausted)
					: reject(epoch_reason_code::request_counter_exhausted);
			}

			auto output = epoch_transition_output{ };
			cancel_pending_ingestion(aState, output);
			retire_pending_comparison(aState, output);
			retire_pending_history(aState, output);
			retire_pending_maintenance(aState, output);

			const auto previousAuthority = aState.Authority;
			aState.EpochGeneration       = *nextEpochGeneration;
			if (nextComparisonGeneration.has_value()) {
				aState.ComparisonGeneration = *nextComparisonGeneration;
			}
			aState.Authority        = aEvent.Authority;
			aState.AcceptedRevision = aEvent.Revision;
			aState.PendingIngestion.reset();

			if (authorityChanged) {
				clear_catalog_bound_state(aState, output);
				output.Requests.insert(
					output.Requests.begin(),
					invalidate_epoch_authority_request{
						.ApplicationSession = aState.ApplicationSession,
						.PreviousAuthority  = previousAuthority,
						.NewGeneration      = aState.EpochGeneration,
					});
			}

			const auto request = take_request(aState);

			output.Authority = epoch_authority_delta{
				.Authority  = aState.Authority,
				.Revision   = aState.AcceptedRevision,
				.Generation = aState.EpochGeneration,
			};
			output.Changed = output.Changed | epoch_changed_domain::authority;
			start_ingestion(aState, output, request);
			return output;
		}

		[[nodiscard]] auto handle(epoch_state& aState, const refresh_epochs_intent&)
			-> epoch_transition_output
		{
			if (!aState.Authority.has_value() || !aState.AcceptedRevision.has_value()) {
				return reject(epoch_reason_code::no_active_authority);
			}

			const auto nextGeneration = advanced(aState.EpochGeneration);
			if (!nextGeneration.has_value()) {
				return reject(epoch_reason_code::epoch_generation_exhausted);
			}
			if (!request_available(aState)) {
				return reject(epoch_reason_code::request_counter_exhausted);
			}

			auto output = epoch_transition_output{ };
			cancel_pending_ingestion(aState, output);
			retire_pending_comparison(aState, output);
			retire_pending_history(aState, output);
			retire_pending_maintenance(aState, output);
			aState.EpochGeneration = *nextGeneration;
			const auto request     = take_request(aState);
			output.Authority       = epoch_authority_delta{
				.Authority  = aState.Authority,
				.Revision   = aState.AcceptedRevision,
				.Generation = aState.EpochGeneration,
			};
			output.Changed = output.Changed | epoch_changed_domain::authority;
			start_ingestion(aState, output, request);
			return output;
		}

		[[nodiscard]] auto handle(
			epoch_state& aState, const pull_request_revision_invalidated_event& aEvent)
			-> epoch_transition_output
		{
			if (aEvent.ApplicationSession != aState.ApplicationSession
				|| !aState.Authority.has_value() || !aState.AcceptedRevision.has_value()
				|| *aState.Authority != aEvent.Authority
				|| *aState.AcceptedRevision != aEvent.Revision
				|| aState.EpochGeneration != aEvent.Generation) {
				return reject(epoch_reason_code::stale_completion);
			}

			auto output = epoch_transition_output{ };
			cancel_pending_ingestion(aState, output);
			retire_pending_comparison(aState, output);
			retire_pending_history(aState, output);
			retire_pending_maintenance(aState, output);

			const auto previous       = aState.Authority;
			const auto nextEpoch      = advanced(aState.EpochGeneration);
			const auto nextComparison = advanced(aState.ComparisonGeneration);
			if (nextEpoch.has_value()) {
				aState.EpochGeneration = *nextEpoch;
			} else {
				diagnose(output, epoch_reason_code::epoch_generation_exhausted);
			}
			if (nextComparison.has_value()) {
				aState.ComparisonGeneration = *nextComparison;
			} else {
				diagnose(output, epoch_reason_code::comparison_generation_exhausted);
			}

			aState.Authority.reset();
			aState.AcceptedRevision.reset();
			aState.PendingIngestion.reset();
			clear_catalog_bound_state(aState, output);

			output.Requests.insert(
				output.Requests.begin(),
				invalidate_epoch_authority_request{
					.ApplicationSession = aState.ApplicationSession,
					.PreviousAuthority  = previous,
					.NewGeneration      = aState.EpochGeneration,
				});
			output.Authority = epoch_authority_delta{
				.Generation = aState.EpochGeneration,
			};
			output.Changed = output.Changed | epoch_changed_domain::authority;
			set_status(aState, output, epoch_status::inactive);
			return output;
		}

		[[nodiscard]] auto handle(
			epoch_state& aState, const epoch_catalog_loaded_event& aEvent)
			-> epoch_transition_output
		{
			if (!aState.PendingIngestion.has_value()) {
				return reject(epoch_reason_code::stale_completion);
			}
			const auto expected = current_load_context(aState, aState.PendingIngestion->Request);
			const auto match    = context_match(
				aState.PendingIngestion,
				epoch_ingestion_phase::loading_catalog,
				aEvent.Context,
				expected);
			if (match != completion_match::current) {
				return reject_match(match);
			}
			if (!valid(aEvent.Catalog)) {
				auto output = epoch_transition_output{ };
				clear_ingestion(
					aState,
					output,
					epoch_status::failed,
					epoch_reason_code::invalid_catalog);
				return output;
			}
			if (aState.PendingIngestion->CancellationRequested) {
				auto output = epoch_transition_output{ };
				clear_ingestion(
					aState,
					output,
					epoch_status::cancelled,
					epoch_reason_code::cancelled);
				return output;
			}

			epoch_transition_output output;
			if (aState.Catalog != std::optional{ aEvent.Catalog }) {
				invalidate_history_window(aState, output);
			}
			aState.Catalog                 = aEvent.Catalog;
			aState.PendingIngestion->Phase = epoch_ingestion_phase::preparing;
			output.Catalog                 = epoch_catalog_delta{ .Catalog = aEvent.Catalog };
			output.Changed                 = output.Changed | epoch_changed_domain::catalog;
			output.Requests.emplace_back(
				prepare_epoch_revision_request{
					.Context = current_preparation_context(
						aState, aState.PendingIngestion->Request),
				});
			set_status(aState, output, epoch_status::preparing);
			return output;
		}

		template <typename Event>
		[[nodiscard]] auto finish_load(
			epoch_state&            aState,
			const Event&            aEvent,
			const epoch_status      aStatus,
			const epoch_reason_code aReason) -> epoch_transition_output
		{
			if (!aState.PendingIngestion.has_value()) {
				return reject(epoch_reason_code::stale_completion);
			}
			const auto match = context_match(
				aState.PendingIngestion,
				epoch_ingestion_phase::loading_catalog,
				aEvent.Context,
				current_load_context(aState, aState.PendingIngestion->Request));
			if (match != completion_match::current) {
				return reject_match(match);
			}
			auto       output           = epoch_transition_output{ };
			const auto cancellationWins = aState.PendingIngestion->CancellationRequested;
			clear_ingestion(
				aState,
				output,
				cancellationWins ? epoch_status::cancelled : aStatus,
				cancellationWins ? epoch_reason_code::cancelled : aReason);
			return output;
		}

		[[nodiscard]] auto handle(
			epoch_state& aState, const epoch_catalog_load_failed_event& aEvent)
			-> epoch_transition_output
		{
			return finish_load(
				aState, aEvent, epoch_status::failed, reason_for(aEvent.Failure));
		}

		[[nodiscard]] auto handle(
			epoch_state& aState, const epoch_catalog_load_cancelled_event& aEvent)
			-> epoch_transition_output
		{
			return finish_load(
				aState, aEvent, epoch_status::cancelled, epoch_reason_code::cancelled);
		}

		[[nodiscard]] auto handle(
			epoch_state& aState, const epoch_revision_prepared_event& aEvent)
			-> epoch_transition_output
		{
			const auto match = preparation_context_match(aState, aEvent.Context);
			if (match != completion_match::current) {
				return reject_match(match);
			}
			if (!aEvent.Operation.valid()) {
				auto output = epoch_transition_output{ };
				clear_ingestion(
					aState,
					output,
					epoch_status::failed,
					epoch_reason_code::invalid_identity);
				return output;
			}

			const auto plan = plan_epoch_mutation(
				*aState.Authority,
				*aState.AcceptedRevision,
				*aState.Catalog,
				aEvent.Observation);
			aState.PendingIngestion->Operation = aEvent.Operation;
			const auto operationContext        = current_operation_context(aState, *aState.PendingIngestion);
			if (aState.PendingIngestion->CancellationRequested) {
				auto output = epoch_transition_output{ };
				output.Requests.emplace_back(
					discard_epoch_preparation_request{
						.Context = operationContext,
					});
				clear_ingestion(
					aState,
					output,
					epoch_status::cancelled,
					epoch_reason_code::cancelled);
				return output;
			}
			if (plan.Rejection != epoch_reason_code::none) {
				auto output = epoch_transition_output{ };
				output.Requests.emplace_back(
					discard_epoch_preparation_request{
						.Context = operationContext,
					});
				clear_ingestion(aState, output, epoch_status::failed, plan.Rejection);
				return output;
			}

			epoch_transition_output output;
			const auto              request = aState.PendingIngestion->Request;
			if (!plan.Mutation.has_value()) {
				output.Requests.emplace_back(
					discard_epoch_preparation_request{
						.Context = operationContext,
					});
				aState.PendingIngestion.reset();
				request_newest_history_page(aState, output);
				static_cast<void>(start_comparison(aState, output, request, true));
				return output;
			}

			aState.PendingIngestion->Mutation = plan.Mutation;
			aState.PendingIngestion->Phase    = epoch_ingestion_phase::committing;
			output.Requests.emplace_back(
				commit_epoch_catalog_request{
					.Context  = current_operation_context(aState, *aState.PendingIngestion),
					.Mutation = *plan.Mutation,
				});
			set_status(aState, output, epoch_status::committing);
			return output;
		}

		template <typename Event>
		[[nodiscard]] auto finish_preparation(
			epoch_state&            aState,
			const Event&            aEvent,
			const epoch_status      aStatus,
			const epoch_reason_code aReason) -> epoch_transition_output
		{
			const auto match = preparation_context_match(aState, aEvent.Context);
			if (match != completion_match::current) {
				return reject_match(match);
			}
			auto       output           = epoch_transition_output{ };
			const auto cancellationWins = aState.PendingIngestion->CancellationRequested;
			clear_ingestion(
				aState,
				output,
				cancellationWins ? epoch_status::cancelled : aStatus,
				cancellationWins ? epoch_reason_code::cancelled : aReason);
			return output;
		}

		[[nodiscard]] auto handle(
			epoch_state& aState, const epoch_revision_prepare_failed_event& aEvent)
			-> epoch_transition_output
		{
			if (aEvent.Failure == epoch_failure::catalog_conflict) {
				const auto match = preparation_context_match(aState, aEvent.Context);
				if (match != completion_match::current) {
					return reject_match(match);
				}
				return restart_after_catalog_conflict(aState);
			}
			return finish_preparation(
				aState, aEvent, epoch_status::failed, reason_for(aEvent.Failure));
		}

		[[nodiscard]] auto handle(
			epoch_state& aState, const epoch_revision_prepare_cancelled_event& aEvent)
			-> epoch_transition_output
		{
			return finish_preparation(
				aState, aEvent, epoch_status::cancelled, epoch_reason_code::cancelled);
		}

		[[nodiscard]] auto handle(
			epoch_state& aState, const epoch_catalog_committed_event& aEvent)
			-> epoch_transition_output
		{
			const auto match = operation_payload_matches(aState, aEvent.Context, aEvent.Mutation);
			if (match != completion_match::current) {
				return reject_match(match);
			}
			if (!next_catalog_revision(aState.Catalog->Revision, aEvent.NewRevision)) {
				auto output = epoch_transition_output{ };
				clear_ingestion(
					aState,
					output,
					epoch_status::failed,
					epoch_reason_code::completion_identity_mismatch);
				return output;
			}

			const auto              request  = aState.PendingIngestion->Request;
			const auto              mutation = *aState.PendingIngestion->Mutation;
			epoch_transition_output output;
			invalidate_history_window(aState, output);
			aState.Catalog->Revision = aEvent.NewRevision;
			aState.PendingIngestion.reset();
			output.Catalog = epoch_catalog_delta{ .Catalog = aState.Catalog };
			output.Changed = output.Changed | epoch_changed_domain::catalog;
			if (const auto* append = std::get_if<append_epoch_mutation>(&mutation)) {
				output.AcceptedEpoch = accepted_epoch_delta{
					.Epoch           = append->Epoch,
					.CatalogRevision = aEvent.NewRevision,
				};
				output.Catalog->InsertedEpoch = append->Epoch.Id;
				if (append->Review.has_value()) {
					output.Catalog->ChangedReviewAssociation = append->Review->Association;
				}
				output.Changed = output.Changed | epoch_changed_domain::epoch;
			} else {
				output.Catalog->ChangedReviewAssociation = std::get<associate_review_mutation>(mutation).Review.Association;
			}
			request_newest_history_page(aState, output);
			static_cast<void>(start_comparison(aState, output, request, true));
			return output;
		}

		[[nodiscard]] auto handle(
			epoch_state& aState, const epoch_historical_catalog_advanced_event& aEvent)
			-> epoch_transition_output
		{
			if (aState.ApplicationSession != aEvent.ApplicationSession
				|| aState.Authority != aEvent.Authority
				|| aState.AcceptedRevision != aEvent.Revision
				|| aState.Catalog != aEvent.ExpectedCatalog) {
				return reject(epoch_reason_code::stale_completion);
			}
			if (!valid(aEvent.NewCatalog)
				|| aEvent.NewCatalog.Incarnation != aEvent.ExpectedCatalog.Incarnation
				|| !next_catalog_revision(aEvent.ExpectedCatalog.Revision, aEvent.NewCatalog.Revision)
				|| aState.PendingIngestion || aState.PendingMaintenance) {
				return reject(epoch_reason_code::completion_identity_mismatch);
			}
			epoch_transition_output output;
			invalidate_history_window(aState, output);
			aState.Catalog = aEvent.NewCatalog;
			output.Catalog = epoch_catalog_delta{ .Catalog = aEvent.NewCatalog };
			output.Changed = output.Changed | epoch_changed_domain::catalog;
			if (aState.PendingOccurrence) {
				const auto pending    = *aState.PendingOccurrence;
				const auto generation = advanced(aState.ComparisonGeneration);
				retire_pending_comparison(aState, output);
				if (!generation || !request_available(aState)) {
					diagnose(output, generation ? epoch_reason_code::request_counter_exhausted : epoch_reason_code::comparison_generation_exhausted);
					set_status(aState, output, epoch_status::failed);
					return output;
				}
				// A prior import may finish durably after a newer gesture. Rebase
				// that gesture, not the older imported head, onto the new catalog.
				const auto request          = take_request(aState);
				aState.ComparisonGeneration = *generation;
				aState.PendingOccurrence    = pending_epoch_occurrence_import{ request, pending.Occurrence, pending.Side };
				output.Requests.emplace_back(import_epoch_occurrence_request{
					.Context = { aState.ApplicationSession, request, *aState.Authority, *aState.AcceptedRevision,
								 aState.EpochGeneration, *generation, *aState.Catalog, pending.Occurrence, pending.Side } });
				request_newest_history_page(aState, output);
				set_status(aState, output, epoch_status::resolving);
				return output;
			}
			request_newest_history_page(aState, output);
			if (request_available(aState)) {
				static_cast<void>(start_comparison(aState, output, take_request(aState), true));
			}
			return output;
		}

		[[nodiscard]] auto handle(
			epoch_state& aState, const epoch_catalog_commit_failed_event& aEvent)
			-> epoch_transition_output
		{
			const auto match = operation_payload_matches(aState, aEvent.Context, aEvent.Mutation);
			if (match != completion_match::current) {
				return reject_match(match);
			}
			if (aEvent.Failure == epoch_failure::catalog_conflict) {
				return restart_after_catalog_conflict(aState);
			}
			auto output = epoch_transition_output{ };
			clear_ingestion(
				aState, output, epoch_status::failed, reason_for(aEvent.Failure));
			return output;
		}

		[[nodiscard]] auto handle(
			epoch_state& aState, const epoch_catalog_commit_cancelled_event& aEvent)
			-> epoch_transition_output
		{
			const auto match = operation_payload_matches(aState, aEvent.Context, aEvent.Mutation);
			if (match != completion_match::current) {
				return reject_match(match);
			}
			switch (aEvent.Progress) {
			case epoch_commit_progress::ref_commit_in_progress:
				return reject(epoch_reason_code::cancellation_after_point_of_no_return);
			case epoch_commit_progress::before_ref_commit:
				break;
			default:
				return reject(epoch_reason_code::invalid_observation);
			}

			auto output = epoch_transition_output{ };
			clear_ingestion(
				aState, output, epoch_status::cancelled, epoch_reason_code::cancelled);
			return output;
		}

		[[nodiscard]] auto handle(
			epoch_state& aState, const comparison_resolved_event& aEvent)
			-> epoch_transition_output
		{
			const auto match = comparison_context_match(aState, aEvent.Context);
			if (match != completion_match::current) {
				return reject_match(match);
			}
			if (!valid(aEvent.Candidates.Catalog)
				|| aEvent.Candidates.Catalog != *aState.Catalog) {
				aState.PendingComparison.reset();
				auto output = reject(epoch_reason_code::completion_identity_mismatch);
				set_status(aState, output, epoch_status::failed);
				return output;
			}

			const auto resolution = resolve_comparison(
				*aState.Authority, aState.RequestedSelection, aEvent.Candidates);
			if (resolution.Reason != epoch_reason_code::none
				&& resolution.Reason != epoch_reason_code::unavailable) {
				auto output = epoch_transition_output{ };
				aState.PendingComparison.reset();
				diagnose(output, resolution.Reason);
				set_status(aState, output, epoch_status::failed);
				return output;
			}

			const bool                         changed = aState.AcceptedComparison != resolution.Pair;
			std::optional<comparison_revision> nextRevision;
			if (changed) {
				nextRevision = advanced(aState.AcceptedComparisonRevision);
				if (!nextRevision.has_value()) {
					aState.PendingComparison.reset();
					aState.AcceptedComparison.reset();
					auto output       = reject(epoch_reason_code::comparison_revision_exhausted);
					output.Comparison = comparison_resolution_delta{
						.Selection = aState.RequestedSelection,
						.Revision  = aState.AcceptedComparisonRevision,
					};
					output.Changed = output.Changed | epoch_changed_domain::comparison;
					set_status(aState, output, epoch_status::unavailable);
					return output;
				}
			}

			aState.PendingComparison.reset();
			if (changed) {
				aState.AcceptedComparison         = resolution.Pair;
				aState.AcceptedComparisonRevision = *nextRevision;
			}

			epoch_transition_output output;
			output.Comparison = comparison_resolution_delta{
				.Selection        = aState.RequestedSelection,
				.Pair             = resolution.Pair,
				.Revision         = aState.AcceptedComparisonRevision,
				.AvailableForDiff = resolution.Pair.has_value() && available_for_diff(*resolution.Pair),
				.PullRequestBase  = pull_request_base(*aState.Authority, aEvent.Candidates),
			};
			output.Changed = epoch_changed_domain::comparison;
			set_status(
				aState,
				output,
				output.Comparison->AvailableForDiff ? epoch_status::ready
													: epoch_status::unavailable);
			return output;
		}

		template <typename Event>
		[[nodiscard]] auto finish_comparison(
			epoch_state&            aState,
			const Event&            aEvent,
			const epoch_status      aStatus,
			const epoch_reason_code aReason) -> epoch_transition_output
		{
			const auto match = comparison_context_match(aState, aEvent.Context);
			if (match != completion_match::current) {
				return reject_match(match);
			}
			aState.PendingComparison.reset();
			auto output = reject(aReason);
			set_status(aState, output, aStatus);
			return output;
		}

		[[nodiscard]] auto handle(
			epoch_state& aState, const comparison_resolve_failed_event& aEvent)
			-> epoch_transition_output
		{
			if (aEvent.Failure == epoch_failure::catalog_conflict) {
				const auto match = comparison_context_match(aState, aEvent.Context);
				if (match != completion_match::current) {
					return reject_match(match);
				}
				aState.PendingComparison.reset();
				return restart_after_catalog_conflict(aState);
			}
			return finish_comparison(
				aState, aEvent, epoch_status::failed, reason_for(aEvent.Failure));
		}

		[[nodiscard]] auto handle(
			epoch_state& aState, const comparison_resolve_cancelled_event& aEvent)
			-> epoch_transition_output
		{
			return finish_comparison(
				aState, aEvent, epoch_status::cancelled, epoch_reason_code::cancelled);
		}

		[[nodiscard]] auto valid_history_page(
			const epoch_state&                    aState,
			const epoch_history_page_context&     aContext,
			const epoch_history_page_observation& aPage) noexcept -> bool
		{
			if (
				!aState.Authority.has_value() || !aState.Catalog.has_value()
				|| aPage.Authority != aState.Authority->Scope
				|| aPage.Catalog != aContext.Catalog
				|| aPage.Catalog != *aState.Catalog
				|| aPage.Epochs.size() > aContext.Limit
				|| (aPage.Continuation.has_value()
					&& (aPage.Epochs.empty()
						|| aPage.Epochs.size() != aContext.Limit
						|| !valid(*aPage.Continuation)
						|| aPage.Continuation->Authority != aPage.Authority
						|| aPage.Continuation->Incarnation
							!= aPage.Catalog.Incarnation
						|| aPage.Continuation->Revision != aPage.Catalog.Revision
						|| aPage.Continuation->Before
							!= aPage.Epochs.back().Id.Ordinal))) {
				return false;
			}

			std::optional<epoch_ordinal> prior;
			for (const auto& epoch : aPage.Epochs) {
				if (!valid_summary(*aState.Authority, aPage.Catalog, epoch)
					|| (aContext.Cursor
						&& epoch.Id.Ordinal.value()
							>= aContext.Cursor->Before.value())
					|| (prior && epoch.Id.Ordinal.value() >= prior->value())) {
					return false;
				}
				prior = epoch.Id.Ordinal;
			}
			return true;
		}

		[[nodiscard]] auto handle(
			epoch_state& aState, const epoch_history_page_loaded_event& aEvent)
			-> epoch_transition_output
		{
			const auto match = history_context_match(aState, aEvent.Context);
			if (match != completion_match::current) {
				return reject_match(match);
			}
			const auto pending = *aState.PendingHistory;
			if (!valid_history_page(aState, aEvent.Context, aEvent.Page)) {
				aState.PendingHistory.reset();
				epoch_transition_output output;
				output.History = epoch_history_page_delta{
					.Phase           = epoch_history_page_phase::failed,
					.Catalog         = pending.Catalog,
					.Navigation      = pending.Navigation,
					.RequestedCursor = pending.Cursor,
					.Limit           = pending.Limit,
					.Reason          = epoch_reason_code::invalid_observation,
				};
				output.Changed = epoch_changed_domain::history;
				diagnose(output, epoch_reason_code::invalid_observation);
				return output;
			}

			aState.PendingHistory.reset();
			aState.HistoryCatalog      = aEvent.Page.Catalog;
			aState.HistoryNavigation   = pending.Navigation;
			aState.HistoryContinuation = aEvent.Page.Continuation;
			epoch_transition_output output;
			output.History = epoch_history_page_delta{
				.Phase           = epoch_history_page_phase::loaded,
				.Catalog         = aEvent.Page.Catalog,
				.Navigation      = pending.Navigation,
				.RequestedCursor = pending.Cursor,
				.Limit           = pending.Limit,
				.Epochs          = aEvent.Page.Epochs,
				.Continuation    = aEvent.Page.Continuation,
			};
			output.Changed = epoch_changed_domain::history;
			return output;
		}

		template <typename Event>
		[[nodiscard]] auto finish_history_page(
			epoch_state&                   aState,
			const Event&                   aEvent,
			const epoch_history_page_phase aPhase,
			const epoch_reason_code        aReason) -> epoch_transition_output
		{
			const auto match = history_context_match(aState, aEvent.Context);
			if (match != completion_match::current) {
				return reject_match(match);
			}
			const auto pending = *aState.PendingHistory;
			aState.PendingHistory.reset();
			epoch_transition_output output;
			output.History = epoch_history_page_delta{
				.Phase           = aPhase,
				.Catalog         = pending.Catalog,
				.Navigation      = pending.Navigation,
				.RequestedCursor = pending.Cursor,
				.Limit           = pending.Limit,
				.Reason          = aReason,
			};
			output.Changed = epoch_changed_domain::history;
			diagnose(output, aReason);
			return output;
		}

		[[nodiscard]] auto handle(
			epoch_state& aState, const epoch_history_page_load_failed_event& aEvent)
			-> epoch_transition_output
		{
			if (aEvent.Failure == epoch_failure::catalog_conflict) {
				const auto match = history_context_match(aState, aEvent.Context);
				if (match != completion_match::current) {
					return reject_match(match);
				}
				return restart_after_catalog_conflict(aState);
			}
			return finish_history_page(
				aState,
				aEvent,
				epoch_history_page_phase::failed,
				reason_for(aEvent.Failure));
		}

		[[nodiscard]] auto handle(
			epoch_state& aState, const epoch_history_page_load_cancelled_event& aEvent)
			-> epoch_transition_output
		{
			return finish_history_page(
				aState,
				aEvent,
				epoch_history_page_phase::cancelled,
				epoch_reason_code::cancelled);
		}

		[[nodiscard]] auto handle(
			epoch_state& aState, const request_epoch_history_page_intent& aIntent)
			-> epoch_transition_output
		{
			if (!aState.Authority.has_value() || !aState.AcceptedRevision.has_value()) {
				return reject(epoch_reason_code::no_active_authority);
			}
			if (!aState.Catalog.has_value()) {
				return reject(epoch_reason_code::no_catalog);
			}
			if (!valid(aIntent.Catalog)) {
				return reject(epoch_reason_code::invalid_catalog);
			}
			if (aIntent.Catalog != *aState.Catalog) {
				return reject(epoch_reason_code::catalog_revision_mismatch);
			}
			if (aIntent.Limit == 0U
				|| aIntent.Limit > maximum_epoch_history_page_size) {
				return reject(epoch_reason_code::invalid_observation);
			}
			if (aState.PendingHistory.has_value()) {
				return reject(epoch_reason_code::duplicate_history_page);
			}
			if (aIntent.Navigation == epoch_history_navigation::older) {
				if (
					!aIntent.Cursor.has_value()
					|| !valid(*aIntent.Cursor)
					|| aIntent.Cursor->Authority != aState.Authority->Scope
					|| aIntent.Cursor->Incarnation != aIntent.Catalog.Incarnation
					|| aIntent.Cursor->Revision != aIntent.Catalog.Revision
					|| aState.HistoryCatalog != std::optional{ aIntent.Catalog }
					|| aState.HistoryContinuation != aIntent.Cursor) {
					return reject(epoch_reason_code::invalid_history_cursor);
				}
			} else if (aIntent.Cursor.has_value()) {
				return reject(epoch_reason_code::invalid_history_cursor);
			} else if (aState.HistoryCatalog == std::optional{ aIntent.Catalog }
					   && aState.HistoryNavigation
						   == epoch_history_navigation::newest) {
				return reject(epoch_reason_code::duplicate_history_page);
			}
			if (!request_available(aState)) {
				return reject(epoch_reason_code::request_counter_exhausted);
			}

			const auto request    = take_request(aState);
			aState.PendingHistory = pending_epoch_history_page{
				.Request    = request,
				.Generation = aState.EpochGeneration,
				.Catalog    = aIntent.Catalog,
				.Navigation = aIntent.Navigation,
				.Cursor     = aIntent.Cursor,
				.Limit      = aIntent.Limit,
			};
			epoch_transition_output output;
			output.Requests.emplace_back(
				load_epoch_history_page_request{
					.Context = current_history_context(
						aState, *aState.PendingHistory),
				});
			output.History = epoch_history_page_delta{
				.Phase           = epoch_history_page_phase::loading,
				.Catalog         = aIntent.Catalog,
				.Navigation      = aIntent.Navigation,
				.RequestedCursor = aIntent.Cursor,
				.Limit           = aIntent.Limit,
			};
			output.Changed = epoch_changed_domain::history;
			return output;
		}

		[[nodiscard]] auto start_maintenance(epoch_state&                    aState,
											 const epoch_maintenance_kind    aKind,
											 const epoch_catalog_identity    aCatalog,
											 const repository_cache_scope_id aCache,
											 std::vector<review_epoch_id>    aEpochs) -> epoch_transition_output
		{
			if (!aState.Authority.has_value() || !aState.AcceptedRevision.has_value()) {
				return reject(epoch_reason_code::no_active_authority);
			}
			if (!aState.Catalog.has_value()) {
				return reject(epoch_reason_code::no_catalog);
			}
			if (!valid(aCatalog)) {
				return reject(epoch_reason_code::invalid_catalog);
			}
			if (aCatalog != *aState.Catalog) {
				return reject(epoch_reason_code::catalog_revision_mismatch);
			}
			if (!valid_maintenance_target(*aState.Authority,
										  aCatalog,
										  aKind,
										  aCache,
										  aEpochs)) {
				return reject(epoch_reason_code::invalid_maintenance_target);
			}
			if (aState.PendingMaintenance.has_value()) {
				return reject(epoch_reason_code::duplicate_maintenance);
			}
			if (aState.PendingIngestion.has_value()) {
				return reject(epoch_reason_code::unavailable);
			}
			if (!request_available(aState)) {
				return reject(epoch_reason_code::request_counter_exhausted);
			}

			const auto request        = take_request(aState);
			const auto digest         = maintenance_target_digest(aKind, aCatalog, aCache, aEpochs);
			aState.PendingMaintenance = pending_epoch_maintenance{
				.Request      = request,
				.Generation   = aState.EpochGeneration,
				.Catalog      = aCatalog,
				.Kind         = aKind,
				.Cache        = aCache,
				.EpochCount   = static_cast<std::uint16_t>(aEpochs.size()),
				.TargetDigest = digest,
			};
			epoch_transition_output output;
			retire_pending_comparison(aState, output);
			retire_pending_history(aState, output);
			output.Requests.emplace_back(perform_epoch_maintenance_request{
				.Context = epoch_maintenance_context{
					.Request            = request,
					.ApplicationSession = aState.ApplicationSession,
					.Authority          = *aState.Authority,
					.Source             = aState.AcceptedRevision->Source,
					.Generation         = aState.EpochGeneration,
					.Catalog            = aCatalog,
					.Kind               = aKind,
					.Cache              = aCache,
					.Epochs             = std::move(aEpochs),
					.TargetDigest       = digest,
				},
			});
			output.Changed = output.Changed | epoch_changed_domain::maintenance;
			set_status(aState, output, epoch_status::maintaining);
			return output;
		}

		[[nodiscard]] auto handle(epoch_state&                      aState,
								  const prune_review_epochs_intent& aIntent) -> epoch_transition_output
		{
			const auto cache = aState.Authority ? aState.Authority->Cache : repository_cache_scope_id{ };
			return start_maintenance(aState,
									 epoch_maintenance_kind::prune_epochs,
									 aIntent.Catalog,
									 cache,
									 aIntent.Epochs);
		}

		[[nodiscard]] auto handle(epoch_state&                          aState,
								  const delete_repository_cache_intent& aIntent) -> epoch_transition_output
		{
			return start_maintenance(aState,
									 epoch_maintenance_kind::delete_repository_cache,
									 aIntent.Catalog,
									 aIntent.Cache,
									 { });
		}

		[[nodiscard]] auto handle(epoch_state& aState,
								  const cancel_epoch_maintenance_intent&) -> epoch_transition_output
		{
			if (!aState.PendingMaintenance.has_value()) {
				return reject(epoch_reason_code::stale_completion);
			}
			if (aState.PendingMaintenance->CancellationRequested) {
				return { };
			}
			aState.PendingMaintenance->CancellationRequested = true;
			epoch_transition_output output;
			output.Requests.emplace_back(cancel_epoch_work_request{
				.Request    = aState.PendingMaintenance->Request,
				.Generation = aState.PendingMaintenance->Generation,
			});
			output.Commands = epoch_command_delta{ .Commands = epoch_commands(aState) };
			output.Changed  = epoch_changed_domain::maintenance
				| epoch_changed_domain::commands;
			return output;
		}

		[[nodiscard]] auto handle(epoch_state&                             aState,
								  const epoch_maintenance_completed_event& aEvent) -> epoch_transition_output
		{
			const auto match = maintenance_context_match(aState, aEvent.Context);
			if (match != completion_match::current) {
				return reject_match(match);
			}
			if (!valid(aEvent.NewCatalog)
				|| aEvent.NewCatalog.Incarnation != aState.Catalog->Incarnation
				|| !next_catalog_revision(aState.Catalog->Revision,
										  aEvent.NewCatalog.Revision)) {
				aState.PendingMaintenance.reset();
				auto output    = reject(epoch_reason_code::completion_identity_mismatch);
				output.Changed = output.Changed | epoch_changed_domain::maintenance;
				set_status(aState, output, epoch_status::failed);
				return output;
			}

			const auto              request = aState.PendingMaintenance->Request;
			const auto              kind    = aState.PendingMaintenance->Kind;
			epoch_transition_output output;
			retire_pending_comparison(aState, output);
			invalidate_history_window(aState, output);
			aState.PendingMaintenance.reset();
			aState.Catalog = aEvent.NewCatalog;
			output.Catalog = epoch_catalog_delta{
				.Catalog                   = aEvent.NewCatalog,
				.AvailabilityChangedEpochs = kind == epoch_maintenance_kind::prune_epochs
					? aEvent.Context.Epochs
					: std::vector<review_epoch_id>{ },
				.AvailabilityChangedTo     = kind == epoch_maintenance_kind::prune_epochs
					? std::optional{ anchor_availability::pruned }
					: std::nullopt,
				.AllEpochAvailability      = kind == epoch_maintenance_kind::delete_repository_cache
					? std::optional{ anchor_availability::cache_deleted }
					: std::nullopt,
			};
			output.Changed = output.Changed | epoch_changed_domain::catalog
				| epoch_changed_domain::maintenance;
			static_cast<void>(start_comparison(aState, output, request, true));
			return output;
		}

		[[nodiscard]] auto handle(epoch_state&                          aState,
								  const epoch_maintenance_failed_event& aEvent) -> epoch_transition_output
		{
			const auto match = maintenance_context_match(aState, aEvent.Context);
			if (match != completion_match::current) {
				return reject_match(match);
			}
			if (aEvent.Failure == epoch_failure::catalog_conflict) {
				aState.PendingMaintenance.reset();
				return restart_after_catalog_conflict(aState);
			}
			aState.PendingMaintenance.reset();
			auto output    = reject(reason_for(aEvent.Failure));
			output.Changed = output.Changed | epoch_changed_domain::maintenance;
			set_status(aState, output, epoch_status::failed);
			return output;
		}

		[[nodiscard]] auto handle(epoch_state&                             aState,
								  const epoch_maintenance_cancelled_event& aEvent) -> epoch_transition_output
		{
			const auto match = maintenance_context_match(aState, aEvent.Context);
			if (match != completion_match::current) {
				return reject_match(match);
			}
			if (aEvent.Progress == epoch_maintenance_progress::after_point_of_no_return) {
				return reject(epoch_reason_code::cancellation_after_point_of_no_return);
			}
			if (aEvent.Progress != epoch_maintenance_progress::before_point_of_no_return) {
				return reject(epoch_reason_code::invalid_observation);
			}
			aState.PendingMaintenance.reset();
			auto output    = reject(epoch_reason_code::cancelled);
			output.Changed = output.Changed | epoch_changed_domain::maintenance;
			set_status(aState, output, epoch_status::cancelled);
			return output;
		}

		[[nodiscard]] auto start_selection(
			epoch_state& aState, const comparison_selection& aSelection)
			-> epoch_transition_output
		{
			if (!aState.Authority.has_value() || !aState.AcceptedRevision.has_value()) {
				return reject(epoch_reason_code::no_active_authority);
			}
			if (!aState.Catalog.has_value()) {
				return reject(epoch_reason_code::no_catalog);
			}
			if (aSelection == aState.RequestedSelection
				&& aState.AcceptedComparison.has_value()
				&& !aState.PendingComparison.has_value() && !aState.PendingOccurrence) {
				return { };
			}
			const auto generation = advanced(aState.ComparisonGeneration);
			if (!generation.has_value()) {
				return reject(epoch_reason_code::comparison_generation_exhausted);
			}
			if (!request_available(aState)) {
				return reject(epoch_reason_code::request_counter_exhausted);
			}

			epoch_transition_output output;
			retire_pending_comparison(aState, output);
			const bool changed          = aSelection != aState.RequestedSelection;
			aState.RequestedSelection   = aSelection;
			aState.ComparisonGeneration = *generation;
			const auto request          = take_request(aState);
			aState.PendingComparison    = pending_comparison_resolution{
				.Request    = request,
				.Generation = *generation,
			};
			if (changed) {
				aState.AcceptedComparison.reset();
				output.Selection = comparison_selection_delta{ .Selection = aSelection };
				output.Changed   = epoch_changed_domain::selection;
			}
			output.Requests.emplace_back(
				resolve_comparison_request{
					.Context = current_comparison_context(aState, request),
				});
			set_status(aState, output, epoch_status::resolving);
			return output;
		}

		[[nodiscard]] auto handle(
			epoch_state& aState, const select_named_comparison_intent& aIntent)
			-> epoch_transition_output
		{
			if (!valid_alias(aIntent.Alias)) {
				return reject(epoch_reason_code::invalid_selection);
			}
			if (!aState.Catalog.has_value()
				|| aIntent.ExpectedCatalogRevision != aState.Catalog->Revision) {
				return reject(epoch_reason_code::catalog_revision_mismatch);
			}
			return start_selection(aState, named_selection(aIntent.Alias));
		}

		[[nodiscard]] auto anchor_belongs(
			const epoch_state& aState, const comparison_anchor_identity& aAnchor) noexcept
			-> bool
		{
			return aState.Authority.has_value() && aState.Catalog.has_value()
				&& valid(aAnchor) && aAnchor.Authority == aState.Authority->Scope
				&& aAnchor.Incarnation == aState.Catalog->Incarnation;
		}

		[[nodiscard]] auto handle(
			epoch_state& aState, const select_comparison_anchors_intent& aIntent)
			-> epoch_transition_output
		{
			if (!aState.Catalog.has_value()
				|| aIntent.ExpectedCatalogRevision != aState.Catalog->Revision) {
				return reject(epoch_reason_code::catalog_revision_mismatch);
			}
			if (!anchor_belongs(aState, aIntent.Left)
				|| !anchor_belongs(aState, aIntent.Right)) {
				return reject(epoch_reason_code::invalid_selection);
			}
			return start_selection(
				aState, explicit_selection(aIntent.Left, aIntent.Right));
		}

		[[nodiscard]] auto handle(
			epoch_state& aState, const select_comparison_anchor_intent& aIntent)
			-> epoch_transition_output
		{
			if (!valid_side(aIntent.Side)) {
				return reject(epoch_reason_code::invalid_selection);
			}
			if (!aState.Catalog.has_value()
				|| aIntent.ExpectedCatalogRevision != aState.Catalog->Revision) {
				return reject(epoch_reason_code::catalog_revision_mismatch);
			}
			if (!anchor_belongs(aState, aIntent.Anchor)) {
				return reject(epoch_reason_code::invalid_selection);
			}
			explicit_comparison_selection pair;
			if (aState.AcceptedComparison.has_value()) {
				pair = { aState.AcceptedComparison->Left.Identity, aState.AcceptedComparison->Right.Identity };
			} else if (const auto* pending = std::get_if<explicit_comparison_selection>(&aState.RequestedSelection);
					   (aState.PendingComparison.has_value() || aState.PendingOccurrence.has_value()) && pending != nullptr
					   && anchor_belongs(aState, pending->Left) && anchor_belongs(aState, pending->Right)) {
				// Selection stays enabled during resolution. Preserve the latest
				// requested opposite side rather than an absent or older result.
				pair = *pending;
			} else {
				return reject(epoch_reason_code::no_resolved_comparison);
			}
			if (aIntent.Side == comparison_side::left) {
				pair.Left = aIntent.Anchor;
			} else {
				pair.Right = aIntent.Anchor;
			}
			return start_selection(aState, pair);
		}

		[[nodiscard]] auto occurrence_context_matches(const epoch_state&                     aState,
													  const epoch_occurrence_import_context& aContext) -> bool
		{
			return aState.PendingOccurrence && aState.PendingOccurrence->Request == aContext.Request
				&& aState.PendingOccurrence->Occurrence == aContext.Occurrence
				&& aState.PendingOccurrence->Side == aContext.Side
				&& aState.ApplicationSession == aContext.ApplicationSession
				&& aState.Authority == aContext.Authority && aState.AcceptedRevision == aContext.Revision
				&& aState.Catalog == aContext.ExpectedCatalog && aState.EpochGeneration == aContext.Generation
				&& aState.ComparisonGeneration == aContext.ComparisonGeneration;
		}

		[[nodiscard]] auto handle(epoch_state& aState, const select_epoch_occurrence_intent& aIntent)
			-> epoch_transition_output
		{
			if (!aState.Authority || !aState.AcceptedRevision) {
				return reject(epoch_reason_code::no_active_authority);
			}
			if (!aState.Catalog || aState.Catalog != aIntent.ExpectedCatalog) {
				return reject(epoch_reason_code::catalog_revision_mismatch);
			}
			if (!valid_side(aIntent.Side) || !aIntent.Occurrence.valid()
				|| aState.PendingIngestion || aState.PendingMaintenance) {
				return reject(epoch_reason_code::invalid_selection);
			}
			explicit_comparison_selection pair;
			if (aState.AcceptedComparison) {
				pair = { aState.AcceptedComparison->Left.Identity, aState.AcceptedComparison->Right.Identity };
			} else if (const auto* pending = std::get_if<explicit_comparison_selection>(&aState.RequestedSelection);
					   pending && (aState.PendingComparison || aState.PendingOccurrence)) {
				pair = *pending;
			} else {
				return reject(epoch_reason_code::no_resolved_comparison);
			}
			if (!anchor_belongs(aState, pair.Left) || !anchor_belongs(aState, pair.Right)) {
				return reject(epoch_reason_code::invalid_selection);
			}
			const auto generation = advanced(aState.ComparisonGeneration);
			if (!generation) {
				return reject(epoch_reason_code::comparison_generation_exhausted);
			}
			if (!request_available(aState)) {
				return reject(epoch_reason_code::request_counter_exhausted);
			}
			epoch_transition_output output;
			retire_pending_comparison(aState, output);
			aState.ComparisonGeneration = *generation;
			aState.RequestedSelection   = pair;
			const auto request          = take_request(aState);
			aState.PendingOccurrence    = pending_epoch_occurrence_import{ request, aIntent.Occurrence, aIntent.Side };
			output.Requests.emplace_back(import_epoch_occurrence_request{
				.Context = { aState.ApplicationSession, request, *aState.Authority, *aState.AcceptedRevision,
							 aState.EpochGeneration, *generation, *aState.Catalog, aIntent.Occurrence, aIntent.Side } });
			set_status(aState, output, epoch_status::resolving);
			return output;
		}

		[[nodiscard]] auto handle(epoch_state& aState, const epoch_occurrence_imported_event& aEvent)
			-> epoch_transition_output
		{
			if (!occurrence_context_matches(aState, aEvent.Context)) {
				return reject(epoch_reason_code::stale_completion);
			}
			if (!valid(aEvent.NewCatalog) || aEvent.NewCatalog.Incarnation != aState.Catalog->Incarnation
				|| !next_catalog_revision(aState.Catalog->Revision, aEvent.NewCatalog.Revision)
				|| !valid(aEvent.Head) || !anchor_belongs(aState, aEvent.Head.Identity)
				|| aEvent.Head.Identity.Kind != comparison_anchor_kind::epoch_head
				|| aEvent.Head.Availability != anchor_availability::available
				|| aState.PendingIngestion || aState.PendingMaintenance) {
				aState.PendingOccurrence.reset();
				auto output = reject(epoch_reason_code::completion_identity_mismatch);
				set_status(aState, output, epoch_status::failed);
				return output;
			}
			auto pair = std::get<explicit_comparison_selection>(aState.RequestedSelection);
			if (aEvent.Context.Side == comparison_side::left) {
				pair.Left = aEvent.Head.Identity;
			} else {
				pair.Right = aEvent.Head.Identity;
			}
			aState.PendingOccurrence.reset();
			epoch_transition_output output;
			invalidate_history_window(aState, output);
			aState.Catalog = aEvent.NewCatalog;
			output.Catalog = epoch_catalog_delta{ .Catalog = aEvent.NewCatalog };
			output.Changed = output.Changed | epoch_changed_domain::catalog;
			// Reuse the import's request identity; resolution advances its own generation.
			aState.RequestedSelection = pair;
			aState.AcceptedComparison.reset();
			output.Selection = comparison_selection_delta{ pair };
			output.Changed   = output.Changed | epoch_changed_domain::selection;
			request_newest_history_page(aState, output);
			static_cast<void>(start_comparison(aState, output, aEvent.Context.Request, true));
			return output;
		}

		[[nodiscard]] auto handle(epoch_state& aState, const epoch_occurrence_import_failed_event& aEvent)
			-> epoch_transition_output
		{
			if (!occurrence_context_matches(aState, aEvent.Context)) {
				return reject(epoch_reason_code::stale_completion);
			}
			aState.PendingOccurrence.reset();
			auto output = reject(reason_for(aEvent.Failure));
			set_status(aState, output, epoch_status::failed);
			return output;
		}

		[[nodiscard]] auto handle(epoch_state& aState, const epoch_occurrence_import_cancelled_event& aEvent)
			-> epoch_transition_output
		{
			if (!occurrence_context_matches(aState, aEvent.Context)) {
				return reject(epoch_reason_code::stale_completion);
			}
			aState.PendingOccurrence.reset();
			auto output = reject(epoch_reason_code::cancelled);
			set_status(aState, output, epoch_status::cancelled);
			return output;
		}

		[[nodiscard]] auto handle(
			epoch_state& aState, const cancel_epoch_refresh_intent&)
			-> epoch_transition_output
		{
			if (!aState.PendingIngestion.has_value()) {
				return reject(epoch_reason_code::stale_completion);
			}
			if (aState.PendingIngestion->CancellationRequested) {
				return { };
			}
			aState.PendingIngestion->CancellationRequested = true;
			epoch_transition_output output;
			output.Requests.emplace_back(
				cancel_epoch_work_request{
					.Request    = aState.PendingIngestion->Request,
					.Generation = aState.PendingIngestion->Generation,
				});
			output.Commands = epoch_command_delta{ .Commands = epoch_commands(aState) };
			output.Changed  = epoch_changed_domain::commands;
			return output;
		}
		[[nodiscard]] auto handle(epoch_state& aState, const cancel_comparison_resolution_intent& aIntent) -> epoch_transition_output
		{
			if (!aState.PendingComparison || aState.PendingComparison->Request != aIntent.Request || aState.PendingComparison->Generation != aIntent.Generation) {
				return reject(epoch_reason_code::stale_completion);
			}
			epoch_transition_output output;
			retire_pending_comparison(aState, output);
			if (aState.AcceptedComparison) {
				aState.RequestedSelection = explicit_selection(aState.AcceptedComparison->Left.Identity, aState.AcceptedComparison->Right.Identity);
			} else {
				aState.RequestedSelection = named_selection(named_comparison::whole_pr);
			}
			output.Selection = comparison_selection_delta{ aState.RequestedSelection };
			output.Changed   = output.Changed | epoch_changed_domain::selection;
			set_status(aState, output, aState.AcceptedComparison && available_for_diff(*aState.AcceptedComparison) ? epoch_status::ready : epoch_status::cancelled);
			return output;
		}

	} // namespace

	auto valid(const pull_request_authority& aAuthority) noexcept -> bool
	{
		if (!valid_access_mode(aAuthority.Mode)) {
			return false;
		}
		// The account and the viewer are the identity the authority carries,
		// and the mode decides whether there is one. Authenticated keeps
		// today's rule unchanged: both must be present. Public preview has no
		// reader to name, so both must be absent, and a nonzero value there is
		// an anonymous authority wearing an account's identity. That is
		// refused rather than normalized away, because accepting it would let
		// a signed-out read share a key with a signed-in one.
		const bool identityValid = aAuthority.Mode == github_access_mode::authenticated
			? valid_nonzero(aAuthority.Account.value())
				&& valid_nonzero(aAuthority.Viewer.value())
			: aAuthority.Account.value() == 0U && aAuthority.Viewer.value() == 0U;
		return identityValid && valid_nonzero(aAuthority.Profile.value())
			&& valid_nonzero(aAuthority.ProfileRevision.value())
			&& valid_nonzero(aAuthority.Host.value())
			&& aAuthority.HostAuthority.valid()
			&& valid_nonzero(aAuthority.Repository.value())
			&& aAuthority.Cache.valid()
			&& valid_nonzero(aAuthority.PullRequest.value())
			&& aAuthority.Number.valid() && aAuthority.Scope.valid();
	}

	auto valid(const pull_request_source_revision& aRevision) noexcept -> bool
	{
		return aRevision.Observation.valid()
			&& valid_nonzero(aRevision.Snapshot.value())
			&& valid_nonzero(aRevision.IdentityGeneration.value())
			&& valid_nonzero(aRevision.PullRequestGeneration.value());
	}

	auto valid(const accepted_pull_request_revision& aRevision) noexcept -> bool
	{
		const bool validOrigin = aRevision.Origin == pull_request_revision_origin::live
			|| aRevision.Origin
				== pull_request_revision_origin::explicit_historical_import;
		const bool validReview = !aRevision.CurrentViewerReview.has_value()
			|| (valid_nonzero(aRevision.CurrentViewerReview->Review.value())
				&& valid_nonzero(aRevision.CurrentViewerReview->Actor.value())
				&& valid(aRevision.CurrentViewerReview->Order)
				&& eligible_review_state(aRevision.CurrentViewerReview->State)
				&& aRevision.CurrentViewerReview->Commit.valid()
				&& aRevision.CurrentViewerReview->Commit.algorithm()
					== aRevision.Base.algorithm());

		return valid(aRevision.Source) && validOrigin
			&& aRevision.BaseBranch.valid() && aRevision.Base.valid()
			&& aRevision.Head.valid()
			&& aRevision.Base.algorithm() == aRevision.Head.algorithm()
			&& validReview;
	}

	auto valid(const epoch_cause& aCause) noexcept -> bool
	{
		return valid_cause(aCause);
	}

	auto valid(const epoch_catalog_identity& aCatalog) noexcept -> bool
	{
		return aCatalog.Incarnation.valid() && valid(aCatalog.Revision);
	}

	auto valid(const review_epoch_id& aEpoch) noexcept -> bool
	{
		return aEpoch.Authority.valid() && aEpoch.Incarnation.valid()
			&& valid(aEpoch.Ordinal);
	}

	auto valid(const comparison_anchor_identity& aAnchor) noexcept -> bool
	{
		if (!aAnchor.Authority.valid() || !aAnchor.Incarnation.valid()) {
			return false;
		}
		switch (aAnchor.Kind) {
		case comparison_anchor_kind::epoch_base:
		case comparison_anchor_kind::epoch_head:
			return valid(aAnchor.Epoch) && !aAnchor.Review.valid();
		case comparison_anchor_kind::review_commit:
			return aAnchor.Epoch.value() == 0 && aAnchor.Review.valid();
		}
		return false;
	}

	auto valid(const comparison_anchor& aAnchor) noexcept -> bool
	{
		return valid(aAnchor.Identity) && aAnchor.Commit.valid()
			&& valid_availability(aAnchor.Availability);
	}

	auto available_for_diff(const comparison_pair& aPair) noexcept -> bool
	{
		return valid(aPair.Left) && valid(aPair.Right)
			&& aPair.Left.Identity.Authority == aPair.Right.Identity.Authority
			&& aPair.Left.Identity.Incarnation == aPair.Right.Identity.Incarnation
			&& aPair.Left.Commit.algorithm() == aPair.Right.Commit.algorithm()
			&& aPair.Left.Availability == anchor_availability::available
			&& aPair.Right.Availability == anchor_availability::available;
	}

	auto epoch_commands(const epoch_state& aState) noexcept
		-> std::array<epoch_command_projection, 6>
	{
		const bool        active        = aState.Authority.has_value() && aState.AcceptedRevision.has_value();
		const bool        catalogReady  = active && aState.Catalog.has_value();
		epoch_reason_code refreshReason = epoch_reason_code::no_active_authority;
		if (active) {
			if (aState.PendingIngestion.has_value()) {
				refreshReason = epoch_reason_code::unavailable;
			} else if (!advanced(aState.EpochGeneration).has_value()) {
				refreshReason = epoch_reason_code::epoch_generation_exhausted;
			} else if (!request_available(aState)) {
				refreshReason = epoch_reason_code::request_counter_exhausted;
			} else {
				refreshReason = epoch_reason_code::none;
			}
		}

		epoch_reason_code selectionReason = epoch_reason_code::no_active_authority;
		if (active) {
			if (!catalogReady) {
				selectionReason = epoch_reason_code::no_catalog;
			} else if (!advanced(aState.ComparisonGeneration).has_value()) {
				selectionReason = epoch_reason_code::comparison_generation_exhausted;
			} else if (!request_available(aState)) {
				selectionReason = epoch_reason_code::request_counter_exhausted;
			} else {
				selectionReason = epoch_reason_code::none;
			}
		}

		const bool canRefresh       = refreshReason == epoch_reason_code::none;
		const bool canSelect        = selectionReason == epoch_reason_code::none;
		const bool canCancelRefresh = aState.PendingIngestion.has_value()
			&& !aState.PendingIngestion->CancellationRequested;

		auto maintenanceReason = epoch_reason_code::no_active_authority;
		if (active) {
			if (!catalogReady) {
				maintenanceReason = epoch_reason_code::no_catalog;
			} else if (aState.PendingMaintenance.has_value()) {
				maintenanceReason = epoch_reason_code::duplicate_maintenance;
			} else if (aState.PendingIngestion.has_value()) {
				maintenanceReason = epoch_reason_code::unavailable;
			} else if (!request_available(aState)) {
				maintenanceReason = epoch_reason_code::request_counter_exhausted;
			} else {
				maintenanceReason = epoch_reason_code::none;
			}
		}

		auto pruneReason = maintenanceReason;
		if (pruneReason == epoch_reason_code::none) {
			if (!aState.AcceptedComparison.has_value()) {
				pruneReason = epoch_reason_code::no_resolved_comparison;
			} else {
				const auto& pair            = *aState.AcceptedComparison;
				const auto  isPrunableEpoch = [](const comparison_anchor& aValue) {
					return (aValue.Identity.Kind
								== comparison_anchor_kind::epoch_base
							|| aValue.Identity.Kind
								== comparison_anchor_kind::epoch_head)
						&& prunable(aValue.Availability);
				};
				if (!isPrunableEpoch(pair.Left) && !isPrunableEpoch(pair.Right)) {
					pruneReason = epoch_reason_code::invalid_maintenance_target;
				}
			}
		}

		const bool canPrune             = pruneReason == epoch_reason_code::none;
		const bool canDeleteCache       = maintenanceReason == epoch_reason_code::none;
		const bool canCancelMaintenance = aState.PendingMaintenance.has_value()
			&& !aState.PendingMaintenance->CancellationRequested;

		return {
			epoch_command_projection{
				.Id             = epoch_command_id::refresh_epochs,
				.Enabled        = canRefresh,
				.DisabledReason = refreshReason,
			},
			epoch_command_projection{
				.Id             = epoch_command_id::select_comparison,
				.Enabled        = canSelect,
				.DisabledReason = selectionReason,
			},
			epoch_command_projection{
				.Id             = epoch_command_id::prune_selected_epochs,
				.Enabled        = canPrune,
				.DisabledReason = pruneReason,
			},
			epoch_command_projection{
				.Id             = epoch_command_id::delete_repository_cache,
				.Enabled        = canDeleteCache,
				.DisabledReason = maintenanceReason,
			},
			epoch_command_projection{
				.Id             = epoch_command_id::cancel_epoch_refresh,
				.Enabled        = canCancelRefresh,
				.DisabledReason = canCancelRefresh
					? epoch_reason_code::none
					: epoch_reason_code::unavailable,
			},
			epoch_command_projection{
				.Id             = epoch_command_id::cancel_epoch_maintenance,
				.Enabled        = canCancelMaintenance,
				.DisabledReason = canCancelMaintenance
					? epoch_reason_code::none
					: epoch_reason_code::unavailable,
			},
		};
	}

	auto material_relation(
		const pull_request_authority&                 aCurrentAuthority,
		const std::optional<epoch_material_identity>& aCurrent,
		const pull_request_authority&                 aPriorAuthority,
		const std::optional<epoch_material_identity>& aPrior) noexcept
		-> epoch_material_relation
	{
		if (aCurrentAuthority != aPriorAuthority || !aCurrent.has_value()
			|| !aPrior.has_value() || !valid_material(*aCurrent)
			|| !valid_material(*aPrior)) {
			return epoch_material_relation::unknown;
		}
		return *aCurrent == *aPrior ? epoch_material_relation::same
									: epoch_material_relation::different;
	}

	auto plan_epoch_preparation(
		const pull_request_authority&                aAuthority,
		const accepted_pull_request_revision&        aRevision,
		const epoch_catalog_identity&                aCatalog,
		const epoch_preparation_catalog_observation& aObservation) noexcept
		-> planned_epoch_preparation
	{
		if (!valid(aAuthority) || !valid(aRevision) || !valid(aCatalog)
			|| (aRevision.CurrentViewerReview.has_value()
				&& aRevision.CurrentViewerReview->Actor != aAuthority.Viewer)
			|| !valid_rehydration_requirement(
				aObservation.RetentionRehydration)
			|| (aObservation.LatestLive.has_value()
				&& (!valid_summary(
						aAuthority, aCatalog, *aObservation.LatestLive)
					|| !live(*aObservation.LatestLive)))) {
			return { .Rejection = epoch_reason_code::invalid_observation };
		}

		const bool historical = aRevision.Origin
			== pull_request_revision_origin::explicit_historical_import;
		const bool initial       = !historical && !aObservation.LatestLive.has_value();
		const bool branchChanged = aObservation.LatestLive.has_value()
			&& aRevision.BaseBranch != aObservation.LatestLive->BaseBranch;
		const bool baseChanged = aObservation.LatestLive.has_value()
			&& aRevision.Base != aObservation.LatestLive->Base;
		const bool headChanged = aObservation.LatestLive.has_value()
			&& aRevision.Head != aObservation.LatestLive->Head;
		const bool retentionRequired = aObservation.RetentionRehydration
			== retention_rehydration_requirement::required;

		if (retentionRequired
			&& (historical || !aObservation.LatestLive.has_value()
				|| aObservation.LatestLive->BaseAvailability
					!= anchor_availability::cache_deleted
				|| aObservation.LatestLive->HeadAvailability
					!= anchor_availability::cache_deleted)) {
			return { .Rejection = epoch_reason_code::invalid_observation };
		}

		const bool sourceObservationReused = aObservation.LatestLive
			&& aRevision.Source.Observation
				== aObservation.LatestLive->Source.Observation;
		const bool rehydrating = retentionRequired && !branchChanged
			&& !baseChanged && !headChanged && !sourceObservationReused;
		if (retentionRequired && !branchChanged && !baseChanged && !headChanged
			&& !rehydrating) {
			return { .Rejection = epoch_reason_code::invalid_observation };
		}

		const bool retainRevision = historical || initial || retentionRequired
			|| branchChanged || baseChanged || headChanged;
		if (retainRevision && !initial && sourceObservationReused) {
			return { .Rejection = epoch_reason_code::invalid_observation };
		}
		return {
			.Effect = retainRevision ? epoch_preparation_effect::retain_revision
									 : epoch_preparation_effect::none,
		};
	}

	auto historical_comparison_selection_changed(
		const accepted_pull_request_revision&      aRevision,
		const std::optional<review_epoch_summary>& aLatestLive) noexcept -> bool
	{
		return aRevision.Origin != pull_request_revision_origin::explicit_historical_import
			&& (!aLatestLive || aRevision.Base != aLatestLive->Base || aRevision.BaseBranch != aLatestLive->BaseBranch);
	}

	void invalidate_derived_comparison_bindings(epoch_occurrence_history& aHistory) noexcept
	{
		for (auto& occurrence : aHistory.Occurrences) {
			if (occurrence.Evidence.Kind == epoch_occurrence_kind::force_push_event
				&& (occurrence.DerivedComparisonBase || (occurrence.LocalEpoch && !occurrence.HistoricalBase))) {
				occurrence.LocalEpoch.reset();
				occurrence.DerivedComparisonBase.reset();
			}
		}
	}

	auto plan_historical_epoch_mutation(
		const pull_request_authority&         aAuthority,
		const accepted_pull_request_revision& aLiveRevision,
		const pull_request_source_revision&   aHistoricalSource,
		const epoch_catalog_identity&         aCatalog,
		const epoch_occurrence&               aOccurrence,
		const git_commit_oid&                 aDerivedComparisonBase,
		const epoch_preparation_observation&  aObservation) noexcept -> planned_epoch_mutation
	{
		if (!valid(aLiveRevision) || !valid(aHistoricalSource) || !valid(aOccurrence)
			|| aOccurrence.LocalEpoch || !aDerivedComparisonBase.valid()
			|| aDerivedComparisonBase.algorithm() != aOccurrence.Evidence.Head.algorithm()
			|| aLiveRevision.Head.algorithm() != aDerivedComparisonBase.algorithm()
			|| aObservation.ProposedCurrentViewerReview || aObservation.CatalogCurrentViewerReview
			|| !aObservation.Material) {
			return { .Rejection = epoch_reason_code::invalid_observation };
		}
		auto historical   = aLiveRevision;
		historical.Source = aHistoricalSource;
		historical.Base   = aDerivedComparisonBase;
		historical.Head   = aOccurrence.Evidence.Head;
		historical.Origin = pull_request_revision_origin::explicit_historical_import;
		historical.CurrentViewerReview.reset();
		auto planned = plan_epoch_mutation(aAuthority, historical, aCatalog, aObservation);

		return planned;
	}

	auto plan_epoch_mutation(
		const pull_request_authority&         aAuthority,
		const accepted_pull_request_revision& aRevision,
		const epoch_catalog_identity&         aCatalog,
		const epoch_preparation_observation&  aObservation) noexcept
		-> planned_epoch_mutation
	{
		const auto preparation = plan_epoch_preparation(
			aAuthority,
			aRevision,
			aCatalog,
			epoch_preparation_catalog_observation{
				.LatestLive           = aObservation.LatestLive,
				.RetentionRehydration = aObservation.RetentionRehydration,
			});
		if (preparation.Rejection != epoch_reason_code::none) {
			return { .Rejection = preparation.Rejection };
		}
		if (!valid(aAuthority) || !valid(aRevision) || !valid(aCatalog)
			|| (aRevision.CurrentViewerReview.has_value()
				&& aRevision.CurrentViewerReview->Actor != aAuthority.Viewer)
			|| !valid_lineage(aObservation.HeadLineage)
			|| (aObservation.CatalogCurrentViewerReview.has_value()
				&& (!valid_catalog_review(*aObservation.CatalogCurrentViewerReview)
					|| aObservation.CatalogCurrentViewerReview->Actor
						!= aAuthority.Viewer))
			|| (aObservation.ProposedCurrentViewerReview.has_value()
				&& (!valid_catalog_review(*aObservation.ProposedCurrentViewerReview)
					|| aObservation.ProposedCurrentViewerReview->Actor
						!= aAuthority.Viewer))) {
			return { .Rejection = epoch_reason_code::invalid_observation };
		}
		if (!valid(aObservation.ObservedAt)
			|| (aObservation.Material.has_value()
				&& (!valid_material(*aObservation.Material)
					|| aObservation.Material->BaseTree.algorithm()
						!= aRevision.Base.algorithm()))) {
			return { .Rejection = epoch_reason_code::invalid_observation };
		}
		const bool historical = aRevision.Origin
			== pull_request_revision_origin::explicit_historical_import;
		const bool              initial       = !historical && !aObservation.LatestLive.has_value();
		bool                    branchChanged = false;
		bool                    baseChanged   = false;
		bool                    headChanged   = false;
		epoch_material_relation relation      = epoch_material_relation::unknown;
		if (aObservation.LatestLive.has_value()) {
			branchChanged = aRevision.BaseBranch != aObservation.LatestLive->BaseBranch;
			baseChanged   = aRevision.Base != aObservation.LatestLive->Base;
			headChanged   = aRevision.Head != aObservation.LatestLive->Head;
			relation      = material_relation(
				aAuthority,
				aObservation.Material,
				aAuthority,
				aObservation.LatestLive->Material);
		}

		const bool retentionRequired = aObservation.RetentionRehydration
			== retention_rehydration_requirement::required;
		const bool rehydrating = retentionRequired && !branchChanged
			&& !baseChanged && !headChanged;
		const bool revisionChanged = preparation.Effect
			== epoch_preparation_effect::retain_revision;
		if (revisionChanged) {
			if (!aObservation.ProposedOrdinal.has_value()
				|| !valid(*aObservation.ProposedOrdinal)) {
				return { .Rejection = epoch_reason_code::invalid_observation };
			}
			if (aObservation.LatestLive.has_value()
				&& aObservation.LatestLive->Id.Ordinal.value()
					>= maximum_epoch_ordinal) {
				return {
					.Rejection = epoch_reason_code::epoch_ordinal_exhausted,
				};
			}
			if (aObservation.LatestLive.has_value()
				&& aObservation.ProposedOrdinal->value()
					<= aObservation.LatestLive->Id.Ordinal.value()) {
				return { .Rejection = epoch_reason_code::invalid_observation };
			}
		}

		if ((historical || initial)
			&& aObservation.HeadLineage != head_lineage::not_applicable) {
			return { .Rejection = epoch_reason_code::invalid_observation };
		}
		if (!historical && !initial
			&& ((headChanged
				 && aObservation.HeadLineage == head_lineage::not_applicable)
				|| (!headChanged
					&& aObservation.HeadLineage != head_lineage::not_applicable))) {
			return { .Rejection = epoch_reason_code::invalid_observation };
		}

		if (aRevision.CurrentViewerReview.has_value()) {
			if (!aObservation.ProposedCurrentViewerReview.has_value()
				|| !same_provider_review(
					*aRevision.CurrentViewerReview,
					*aObservation.ProposedCurrentViewerReview)) {
				return { .Rejection = epoch_reason_code::invalid_observation };
			}
		} else if (aObservation.ProposedCurrentViewerReview.has_value()) {
			return { .Rejection = epoch_reason_code::invalid_observation };
		}

		if (!historical && !initial && !retentionRequired && !branchChanged
			&& !baseChanged && !headChanged) {
			if (!aRevision.CurrentViewerReview.has_value()) {
				return { };
			}
			if (aObservation.ProposedCurrentViewerReview
				== aObservation.CatalogCurrentViewerReview) {
				return { };
			}
			if (aCatalog.Revision.value() >= maximum_epoch_catalog_revision) {
				return {
					.Rejection = epoch_reason_code::catalog_revision_exhausted,
				};
			}
			return {
				.Mutation = associate_review_mutation{
					.Review = *aObservation.ProposedCurrentViewerReview,
				},
			};
		}

		if (aCatalog.Revision.value() >= maximum_epoch_catalog_revision) {
			return {
				.Rejection = epoch_reason_code::catalog_revision_exhausted,
			};
		}

		epoch_cause cause;
		cause.Material = relation;
		if (historical) {
			cause.Origin  = epoch_origin::explicit_historical_import;
			cause.Base    = epoch_base_transition::not_applicable;
			cause.Head    = epoch_head_transition::not_applicable;
			cause.Primary = epoch_primary_cause::explicit_historical_import;
		} else if (initial) {
			cause.Origin  = epoch_origin::initial_observation;
			cause.Base    = epoch_base_transition::not_applicable;
			cause.Head    = epoch_head_transition::not_applicable;
			cause.Primary = epoch_primary_cause::initial_observation;
		} else if (rehydrating) {
			cause.Origin  = epoch_origin::retention_rehydration;
			cause.Base    = epoch_base_transition::unchanged;
			cause.Head    = epoch_head_transition::unchanged;
			cause.Primary = epoch_primary_cause::retention_rehydration;
		} else {
			cause.Origin = epoch_origin::live_observation;
			cause.Base   = branchChanged
				? epoch_base_transition::base_branch_changed
				: (baseChanged ? epoch_base_transition::base_oid_changed
							   : epoch_base_transition::unchanged);
			if (!headChanged) {
				cause.Head = epoch_head_transition::unchanged;
			} else {
				switch (aObservation.HeadLineage) {
				case head_lineage::descendant:
					cause.Head = epoch_head_transition::descendant;
					break;
				case head_lineage::non_fast_forward:
					cause.Head = epoch_head_transition::non_fast_forward;
					break;
				case head_lineage::unknown:
					cause.Head = epoch_head_transition::lineage_unknown;
					break;
				case head_lineage::not_applicable:
					return {
						.Rejection = epoch_reason_code::invalid_observation,
					};
				}
			}

			if (branchChanged) {
				cause.Primary = epoch_primary_cause::base_branch_change;
			} else if (cause.Head == epoch_head_transition::non_fast_forward) {
				cause.Primary = epoch_primary_cause::force_push_rebase;
			} else if (cause.Head == epoch_head_transition::descendant) {
				cause.Primary = epoch_primary_cause::author_upload;
			} else if (cause.Head == epoch_head_transition::lineage_unknown) {
				cause.Primary = epoch_primary_cause::head_change_lineage_unavailable;
			} else {
				cause.Primary = epoch_primary_cause::base_update;
			}
		}

		const review_epoch_summary epoch{
			.Id = review_epoch_id{
				.Authority   = aAuthority.Scope,
				.Incarnation = aCatalog.Incarnation,
				.Ordinal     = *aObservation.ProposedOrdinal,
			},
			.Source           = aRevision.Source,
			.BaseBranch       = aRevision.BaseBranch,
			.Base             = aRevision.Base,
			.Head             = aRevision.Head,
			.Material         = aObservation.Material,
			.Cause            = cause,
			.ObservedAt       = aObservation.ObservedAt,
			.BaseAvailability = anchor_availability::available,
			.HeadAvailability = anchor_availability::available,
		};
		return {
			.Mutation = append_epoch_mutation{
				.Epoch  = epoch,
				.Review = aObservation.ProposedCurrentViewerReview,
			},
		};
	}

	auto resolve_comparison(
		const pull_request_authority& aAuthority,
		const comparison_selection&   aSelection,
		const comparison_candidates&  aCandidates) noexcept -> resolved_comparison
	{
		if (!valid(aAuthority) || !valid(aCandidates.Catalog)) {
			return { .Reason = epoch_reason_code::invalid_observation };
		}
		const auto resolution = select_pair(aAuthority, aSelection, aCandidates);
		if (!resolution.Pair.has_value()) {
			return resolution;
		}
		if (resolution.Pair->Left.Identity.Authority != aAuthority.Scope
			|| resolution.Pair->Right.Identity.Authority != aAuthority.Scope
			|| resolution.Pair->Left.Identity.Incarnation
				!= aCandidates.Catalog.Incarnation
			|| resolution.Pair->Right.Identity.Incarnation
				!= aCandidates.Catalog.Incarnation
			|| resolution.Pair->Left.Commit.algorithm()
				!= resolution.Pair->Right.Commit.algorithm()) {
			return { .Reason = epoch_reason_code::invalid_selection };
		}
		if (!available_for_diff(*resolution.Pair)) {
			return {
				.Pair   = resolution.Pair,
				.Reason = epoch_reason_code::unavailable,
			};
		}
		return resolution;
	}

	auto transition(epoch_state& aState, const epoch_event& aEvent)
		-> epoch_transition_output
	{
		return std::visit(
			[&aState](const auto& aConcrete) { return handle(aState, aConcrete); },
			aEvent);
	}
} // namespace bpr::core
