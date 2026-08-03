#include "bpr/application/epoch_application_host.hpp"
#include "bpr/application/git_protocol_v2.hpp"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <limits>
#include <mutex>
#include <ranges>
#include <stop_token>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::application {
	namespace {
		inline constexpr std::size_t maximum_identity_attempts    = 32U;
		inline constexpr std::size_t maximum_pending_preparations = 100U;
		inline constexpr std::size_t maximum_recovery_operations  = 1'000U;
		inline constexpr std::size_t maximum_cache_actors         = 1'000U;

		struct cache_actor_key
		{
			core::application_session_id     ApplicationSession;
			core::host_profile_id            Profile;
			core::github_profile_revision    ProfileRevision;
			core::git_host_id                Host;
			core::host_authority_fingerprint HostAuthority;
			core::github_account_id          Account;
			core::repository_id              Repository;
			core::git_hash_algorithm         ObjectFormat = core::git_hash_algorithm::invalid;

			auto operator<=>(const cache_actor_key&) const = default;
		};

		[[nodiscard]] auto actor_key(
			const accepted_pull_request_handoff& aHandoff) noexcept -> cache_actor_key
		{
			return {
				.ApplicationSession = aHandoff.ApplicationSession,
				.Profile            = aHandoff.Profile,
				.ProfileRevision    = aHandoff.ProfileRevision,
				.Host               = aHandoff.Host,
				.HostAuthority      = aHandoff.HostAuthority,
				.Account            = aHandoff.Account,
				.Repository         = aHandoff.Repository,
				.ObjectFormat       = aHandoff.ObjectFormat,
			};
		}

		[[nodiscard]] auto actor_key(
			const diff_blob_request& aRequest) noexcept
			-> std::optional<cache_actor_key>
		{
			const auto algorithm = aRequest.Blob.algorithm();
			if (
				!core::valid(aRequest.Context) || !aRequest.Blob.valid()
				|| aRequest.MaximumBytes == 0U
				|| (algorithm != core::git_hash_algorithm::sha1
					&& algorithm != core::git_hash_algorithm::sha256)) {
				return std::nullopt;
			}
			const auto& authority = aRequest.Context.Key.Authority;
			return cache_actor_key{
				.ApplicationSession = aRequest.Context.ApplicationSession,
				.Profile            = authority.Profile,
				.ProfileRevision    = authority.ProfileRevision,
				.Host               = authority.Host,
				.HostAuthority      = authority.HostAuthority,
				.Account            = authority.Account,
				.Repository         = authority.Repository,
				.ObjectFormat       = algorithm,
			};
		}

		template <typename Identity>
		[[nodiscard]] auto mint_identity(epoch_entropy_port& aEntropy)
			-> std::optional<Identity>
		{
			const auto bytes = aEntropy.generate();
			if (!bytes || !application::valid(*bytes)) {
				return std::nullopt;
			}
			return Identity{ *bytes };
		}

		[[nodiscard]] auto catalog_failure(
			const epoch_catalog_failure aFailure) noexcept -> core::epoch_failure
		{
			switch (aFailure) {
			case epoch_catalog_failure::cancelled:
				return core::epoch_failure::unavailable;
			case epoch_catalog_failure::quota_exceeded:
				return core::epoch_failure::quota_exceeded;
			case epoch_catalog_failure::ordinal_exhausted:
				return core::epoch_failure::ordinal_exhausted;
			case epoch_catalog_failure::revision_conflict:
			case epoch_catalog_failure::catalog_mismatch:
			case epoch_catalog_failure::authority_mismatch:
			case epoch_catalog_failure::operation_identity_conflict:
			case epoch_catalog_failure::source_observation_conflict:
			case epoch_catalog_failure::operation_conflict:
			case epoch_catalog_failure::phase_conflict:
			case epoch_catalog_failure::busy:
				return core::epoch_failure::catalog_conflict;
			case epoch_catalog_failure::schema_mismatch:
			case epoch_catalog_failure::integrity_failure:
			case epoch_catalog_failure::repair_required:
				return core::epoch_failure::repair_required;
			case epoch_catalog_failure::invalid_request:
				return core::epoch_failure::malformed_observation;
			case epoch_catalog_failure::invalid_path:
			case epoch_catalog_failure::invalid_permissions:
			case epoch_catalog_failure::unavailable:
			case epoch_catalog_failure::platform_unsupported:
				return core::epoch_failure::unavailable;
			}
			return core::epoch_failure::unavailable;
		}

		[[nodiscard]] auto cache_failure(const git_cache_failure aFailure) noexcept
			-> core::epoch_failure
		{
			switch (aFailure) {
			case git_cache_failure::cancelled:
				return core::epoch_failure::unavailable;
			case git_cache_failure::quota_exceeded:
				return core::epoch_failure::quota_exceeded;
			case git_cache_failure::repair_required:
			case git_cache_failure::invalid_manifest:
			case git_cache_failure::invalid_repository:
			case git_cache_failure::inventory_mismatch:
			case git_cache_failure::authorization_mismatch:
			case git_cache_failure::object_mismatch:
			case git_cache_failure::ref_conflict:
				return core::epoch_failure::repair_required;
			case git_cache_failure::invalid_request:
			case git_cache_failure::invalid_authority:
				return core::epoch_failure::malformed_observation;
			case git_cache_failure::invalid_path:
			case git_cache_failure::lock_unavailable:
			case git_cache_failure::spool_failure:
			case git_cache_failure::process_failure:
				return core::epoch_failure::unavailable;
			}
			return core::epoch_failure::unavailable;
		}

		[[nodiscard]] auto catalog_key(
			const accepted_pull_request_handoff& aHandoff)
			-> epoch_catalog_authority_key
		{
			return {
				.Profile         = aHandoff.Profile,
				.ProfileRevision = aHandoff.ProfileRevision,
				.Host            = aHandoff.Host,
				.HostAuthority   = aHandoff.HostAuthority,
				.Account         = aHandoff.Account,
				.Viewer          = aHandoff.Viewer,
				.Repository      = aHandoff.Repository,
				.PullRequest     = aHandoff.PullRequest,
				.Number          = aHandoff.Number,
				.ObjectFormat    = aHandoff.ObjectFormat,
			};
		}

		[[nodiscard]] auto agrees(
			const core::pull_request_authority&  aAuthority,
			const accepted_pull_request_handoff& aHandoff) noexcept -> bool
		{
			return aAuthority.Profile == aHandoff.Profile
				&& aAuthority.ProfileRevision == aHandoff.ProfileRevision
				&& aAuthority.Host == aHandoff.Host
				&& aAuthority.HostAuthority == aHandoff.HostAuthority
				&& aAuthority.Account == aHandoff.Account
				&& aAuthority.Viewer == aHandoff.Viewer
				&& aAuthority.Repository == aHandoff.Repository
				&& aAuthority.PullRequest == aHandoff.PullRequest
				&& aAuthority.Number == aHandoff.Number;
		}

		[[nodiscard]] auto accepted_revision(
			const accepted_pull_request_handoff&    aHandoff,
			const core::epoch_source_observation_id aObservation)
			-> core::accepted_pull_request_revision
		{
			return {
				.Source = {
					.Observation           = aObservation,
					.Snapshot              = aHandoff.Snapshot,
					.IdentityGeneration    = aHandoff.IdentityGeneration,
					.PullRequestGeneration = aHandoff.PullRequestGeneration,
				},
				.Origin              = aHandoff.Origin,
				.BaseBranch          = aHandoff.BaseBranch,
				.Base                = aHandoff.Base,
				.Head                = aHandoff.Head,
				.CurrentViewerReview = aHandoff.CurrentViewerReview,
			};
		}

		[[nodiscard]] auto invalidation_identity(
			const accepted_pull_request_handoff& aHandoff)
			-> pull_request_authority_invalidation
		{
			return {
				.ApplicationSession = aHandoff.ApplicationSession,
				.Profile            = aHandoff.Profile,
				.ProfileRevision    = aHandoff.ProfileRevision,
				.Host               = aHandoff.Host,
				.HostAuthority      = aHandoff.HostAuthority,
				.Account            = aHandoff.Account,
				.Viewer             = aHandoff.Viewer,
				.Repository         = aHandoff.Repository,
				.PullRequest        = aHandoff.PullRequest,
				.IdentityGeneration = aHandoff.IdentityGeneration,
			};
		}

		[[nodiscard]] auto projected_revision(
			const core::accepted_pull_request_revision& aRevision)
			-> epoch_revision_projection
		{
			return {
				.Snapshot              = aRevision.Source.Snapshot,
				.IdentityGeneration    = aRevision.Source.IdentityGeneration,
				.PullRequestGeneration = aRevision.Source.PullRequestGeneration,
				.Origin                = aRevision.Origin,
				.BaseBranch            = aRevision.BaseBranch,
				.Base                  = aRevision.Base,
				.Head                  = aRevision.Head,
				.CurrentViewerReview   = aRevision.CurrentViewerReview,
			};
		}

		[[nodiscard]] auto projected_row(const core::review_epoch_summary& aEpoch)
			-> epoch_row_projection
		{
			return {
				.Id                    = aEpoch.Id,
				.Snapshot              = aEpoch.Source.Snapshot,
				.IdentityGeneration    = aEpoch.Source.IdentityGeneration,
				.PullRequestGeneration = aEpoch.Source.PullRequestGeneration,
				.BaseBranch            = aEpoch.BaseBranch,
				.Base                  = aEpoch.Base,
				.Head                  = aEpoch.Head,
				.Material              = aEpoch.Material,
				.Cause                 = aEpoch.Cause,
				.ObservedAt            = aEpoch.ObservedAt,
				.BaseAvailability      = aEpoch.BaseAvailability,
				.HeadAvailability      = aEpoch.HeadAvailability,
			};
		}

		[[nodiscard]] auto review_anchor(
			const core::pull_request_authority&     aAuthority,
			const core::epoch_catalog_identity      aCatalog,
			const core::catalog_review_association& aReview)
			-> core::comparison_anchor
		{
			return {
				.Identity = {
					.Authority   = aAuthority.Scope,
					.Incarnation = aCatalog.Incarnation,
					.Kind        = core::comparison_anchor_kind::review_commit,
					.Review      = aReview.Association,
				},
				.Commit       = aReview.Commit,
				.Availability = aReview.Availability,
			};
		}

		[[nodiscard]] auto recovery_request(const append_operation_record& aRecord)
			-> std::optional<git_operation_recovery_request>
		{
			if (!application::valid(aRecord)) {
				return std::nullopt;
			}
			auto request = git_operation_recovery_request{
				.Operation = aRecord.Context.Operation,
				.Scope     = aRecord.StagingBase.Authority,
				.Ordinal   = aRecord.ReservedOrdinal,
				.Base      = aRecord.Context.Revision.Base,
				.Head      = aRecord.Context.Revision.Head,
			};
			if (aRecord.Spool && aRecord.Preimport) {
				request.Spool = git_recovery_spool_expectation{
					.ExpectedPack             = aRecord.Spool->ExpectedPack,
					.CompletedSpool           = aRecord.Spool->Spool,
					.PreimportPacks           = aRecord.Preimport->Packs,
					.PreimportRepositoryBytes = aRecord.Preimport->RepositoryBytes,
					.ExpectedPackPreexisting  = aRecord.Preimport->ExpectedPackPreexisting,
				};
			}
			if (aRecord.Install) {
				request.Install = git_recovery_install_expectation{
					.Pack           = aRecord.Install->Pack,
					.InstalledPack  = aRecord.Install->InstalledPack,
					.InstalledIndex = aRecord.Install->InstalledIndex,
					.BaseTree       = aRecord.Install->BaseTree,
					.HeadTree       = aRecord.Install->HeadTree,
				};
			}
			return application::valid(request) ? std::optional{ std::move(request) }
											   : std::nullopt;
		}

		[[nodiscard]] auto recovered_prepared_revision(
			const append_operation_record& aRecord)
			-> std::optional<git_prepared_revision>
		{
			if (!aRecord.Install) {
				return std::nullopt;
			}
			auto prepared = git_prepared_revision{
				.Operation      = aRecord.Context.Operation,
				.Pack           = aRecord.Install->Pack,
				.InstalledPack  = aRecord.Install->InstalledPack,
				.InstalledIndex = aRecord.Install->InstalledIndex,
				.Base           = aRecord.Install->Base,
				.BaseTree       = aRecord.Install->BaseTree,
				.Head           = aRecord.Install->Head,
				.HeadTree       = aRecord.Install->HeadTree,
				.HeadLineage    = aRecord.Install->HeadLineage,
				.ImportedBytes  = aRecord.Install->InstalledPack.Bytes,
			};
			return application::valid(prepared) ? std::optional{ std::move(prepared) }
												: std::nullopt;
		}

		[[nodiscard]] auto expected_retained_pair(
			const append_operation_record& aRecord) -> git_retained_ref_pair
		{
			return {
				.Scope   = aRecord.FinalBase.Authority,
				.Ordinal = aRecord.ReservedOrdinal,
				.Base    = aRecord.Context.Revision.Base,
				.Head    = aRecord.Context.Revision.Head,
			};
		}

		[[nodiscard]] auto same_append_identity(
			const append_operation_record& aPrior,
			const append_operation_record& aNext) noexcept -> bool
		{
			return application::valid(aNext) && aNext.Context == aPrior.Context
				&& aNext.ReservedOrdinal == aPrior.ReservedOrdinal
				&& aNext.StagingBase == aPrior.StagingBase
				&& aNext.StagingHead == aPrior.StagingHead
				&& aNext.FinalBase == aPrior.FinalBase
				&& aNext.FinalHead == aPrior.FinalHead;
		}

		[[nodiscard]] auto same_maintenance_identity(
			const maintenance_operation_record& aPrior,
			const maintenance_operation_record& aNext) noexcept -> bool
		{
			return application::valid(aNext) && aNext.Incarnation == aPrior.Incarnation
				&& aNext.Operation == aPrior.Operation && aNext.Kind == aPrior.Kind
				&& aNext.Cache == aPrior.Cache && aNext.Epochs == aPrior.Epochs
				&& aNext.Scopes == aPrior.Scopes;
		}

		[[nodiscard]] auto exact_maintenance_transition(
			const maintenance_operation_record& aPrior,
			const maintenance_operation_record& aNext,
			const epoch_catalog_operation_phase aPhase) noexcept -> bool
		{
			return same_maintenance_identity(aPrior, aNext)
				&& application::can_advance(aPrior.OperationRevision)
				&& aNext.OperationRevision.value() == aPrior.OperationRevision.value() + 1
				&& aNext.Phase == aPhase
				&& (aPhase == epoch_catalog_operation_phase::repair_required
						? aNext.RepairFromPhase == std::optional{ aPrior.Phase }
						: !aNext.RepairFromPhase);
		}

		[[nodiscard]] auto exact_prune_intent(
			const maintenance_operation_record&   aRecord,
			const begin_prune_request&            aRequest,
			const core::repository_cache_scope_id aCache) noexcept -> bool
		{
			return application::valid(aRecord)
				&& aRecord.Incarnation == aRequest.Catalog.Incarnation
				&& aRecord.Operation == aRequest.Operation
				&& aRecord.Kind == epoch_catalog_operation_kind::prune
				&& aRecord.Phase == epoch_catalog_operation_phase::prune_intent
				&& aRecord.OperationRevision == epoch_catalog_operation_revision{ 1 }
			&& aRecord.Cache == aCache && aRecord.Epochs == aRequest.Epochs
				&& aRecord.Scopes.empty() && !aRecord.RepairFromPhase;
		}

		[[nodiscard]] auto exact_cache_delete_intent(
			const maintenance_operation_record& aRecord,
			const begin_cache_delete_request&   aRequest) noexcept -> bool
		{
			return application::valid(aRecord)
				&& aRecord.Incarnation == aRequest.Impact.Incarnation
				&& aRecord.Operation == aRequest.Operation
				&& aRecord.Kind == epoch_catalog_operation_kind::cache_delete
				&& aRecord.Phase
				== epoch_catalog_operation_phase::cache_delete_intent_sealed
				&& aRecord.OperationRevision == epoch_catalog_operation_revision{ 1 }
			&& aRecord.Cache == aRequest.Impact.Cache && aRecord.Epochs.empty()
				&& aRecord.Scopes == aRequest.Impact.Scopes && !aRecord.RepairFromPhase;
		}

		[[nodiscard]] auto recovery_operation_identity(
			const epoch_recovery_operation& aOperation) noexcept
			-> core::epoch_operation_id
		{
			return std::visit(
				[](const auto& aValue) {
					using value_type = std::remove_cvref_t<decltype(aValue)>;
					if constexpr (
						std::is_same_v<value_type, append_operation_record>) {
						return aValue.Context.Operation;
					} else {
						return aValue.Operation;
					}
				},
				aOperation);
		}

		[[nodiscard]] auto exact_append_reservation(
			const append_operation_record&              aRecord,
			const append_reservation_request&           aRequest,
			const core::pull_request_authority_scope_id aAuthority) noexcept -> bool
		{
			return application::valid(aRecord)
				&& aRecord.Context == aRequest.Context
				&& aRecord.ReservedOrdinal == aRequest.ProposedOrdinal
				&& aRecord.OperationRevision
				== epoch_catalog_operation_revision{ 1 }
			&& aRecord.Phase == epoch_catalog_operation_phase::preparing
				&& !aRecord.RepairFromPhase && !aRecord.RecoveryInputCompacted
				&& !aRecord.Spool && !aRecord.Preimport && !aRecord.Install
				&& !aRecord.Candidate
				&& aRecord.StagingBase
				== epoch_catalog_ref_identity{
					   .Authority = aAuthority,
					   .Ordinal   = aRequest.ProposedOrdinal,
					   .Operation = aRequest.Context.Operation,
					   .Staging   = true,
				   }
			&& aRecord.StagingHead == epoch_catalog_ref_identity{
				   .Authority = aAuthority,
				   .Ordinal   = aRequest.ProposedOrdinal,
				   .Operation = aRequest.Context.Operation,
				   .Staging   = true,
				   .Head      = true,
			   }
			&& aRecord.FinalBase == epoch_catalog_ref_identity{
				   .Authority = aAuthority,
				   .Ordinal   = aRequest.ProposedOrdinal,
			   }
			&& aRecord.FinalHead == epoch_catalog_ref_identity{
				   .Authority = aAuthority,
				   .Ordinal   = aRequest.ProposedOrdinal,
				   .Head      = true,
			   };
		}

		[[nodiscard]] auto exact_append_transition(
			const append_operation_record&      aPrior,
			const append_operation_record&      aNext,
			const epoch_catalog_operation_phase aPhase) noexcept -> bool
		{
			if (
				!same_append_identity(aPrior, aNext)
				|| !application::can_advance(aPrior.OperationRevision)
				|| aNext.OperationRevision.value()
					!= aPrior.OperationRevision.value() + 1
				|| aNext.Phase != aPhase) {
				return false;
			}
			const auto terminal = aPhase == epoch_catalog_operation_phase::complete
				|| aPhase == epoch_catalog_operation_phase::aborted;
			const auto exactRepairOrigin = aPhase == epoch_catalog_operation_phase::repair_required
				? aNext.RepairFromPhase == std::optional{ aPrior.Phase }
				: !aNext.RepairFromPhase;
			return exactRepairOrigin && aNext.Install == aPrior.Install
				&& aNext.Candidate == aPrior.Candidate
				&& (terminal
						? aNext.RecoveryInputCompacted && !aNext.Spool
							&& !aNext.Preimport
						: !aNext.RecoveryInputCompacted
							&& aNext.Spool == aPrior.Spool
							&& aNext.Preimport == aPrior.Preimport);
		}

		[[nodiscard]] auto exact_spool_record(
			const append_operation_record& aPrior,
			const append_operation_record& aNext,
			const record_spool_request&    aRequest) noexcept -> bool
		{
			return same_append_identity(aPrior, aNext)
				&& aNext.Phase == epoch_catalog_operation_phase::spool_recorded
				&& application::can_advance_nonterminal(aPrior.OperationRevision)
				&& aNext.OperationRevision.value()
				== aPrior.OperationRevision.value() + 1
				&& !aNext.RepairFromPhase && !aNext.RecoveryInputCompacted
				&& aNext.Spool == std::optional{ aRequest.Spool }
			&& aNext.Preimport == std::optional{ aRequest.Preimport }
			&& aNext.Install == aPrior.Install
				&& aNext.Candidate == aPrior.Candidate;
		}

		[[nodiscard]] auto exact_install_record(
			const append_operation_record& aPrior,
			const append_operation_record& aNext,
			const record_install_request&  aRequest) noexcept -> bool
		{
			return same_append_identity(aPrior, aNext)
				&& aNext.Phase == epoch_catalog_operation_phase::imported
				&& application::can_advance_nonterminal(aPrior.OperationRevision)
				&& aNext.OperationRevision.value()
				== aPrior.OperationRevision.value() + 1
				&& !aNext.RepairFromPhase && !aNext.RecoveryInputCompacted
				&& aNext.Spool == aPrior.Spool && aNext.Preimport == aPrior.Preimport
				&& aNext.Install == std::optional{ aRequest.Install }
			&& aNext.Candidate == aPrior.Candidate;
		}

		[[nodiscard]] auto exact_prepared_revision(
			const git_prepared_revision&        aPrepared,
			const git_prepare_revision_request& aRequest) noexcept -> bool
		{
			return application::valid(aPrepared)
				&& aPrepared.Operation == aRequest.Authorization.Operation
				&& aPrepared.Pack == aRequest.Authorization.ExpectedPack
				&& aPrepared.Base == aRequest.Base && aPrepared.Head == aRequest.Head
				&& aPrepared.ImportedBytes == aRequest.Receipt.Spool.Bytes
				&& aPrepared.InstalledPack.Bytes == aRequest.Receipt.Spool.Bytes;
		}

		[[nodiscard]] auto exact_candidate_record(
			const append_operation_record&  aPrior,
			const append_operation_record&  aNext,
			const record_candidate_request& aRequest) noexcept -> bool
		{
			return same_append_identity(aPrior, aNext)
				&& aNext.Phase
				== epoch_catalog_operation_phase::commit_authorized
				&& application::can_advance_nonterminal(aPrior.OperationRevision)
				&& aNext.OperationRevision.value()
				== aPrior.OperationRevision.value() + 1
				&& !aNext.RepairFromPhase && !aNext.RecoveryInputCompacted
				&& aNext.Spool == aPrior.Spool && aNext.Preimport == aPrior.Preimport
				&& aNext.Install == aPrior.Install
				&& aNext.Candidate == std::optional{ aRequest.Candidate };
		}

		[[nodiscard]] auto exact_catalog_commit(
			const catalog_commit_result&               aResult,
			const core::epoch_catalog_identity         aPrior,
			const std::optional<core::review_epoch_id> aInserted) noexcept -> bool
		{
			return core::valid(aResult.Catalog) && aResult.Changed
				&& aPrior.Revision.value() < core::maximum_epoch_catalog_revision
				&& aResult.Catalog.Incarnation == aPrior.Incarnation
				&& aResult.Catalog.Revision.value() == aPrior.Revision.value() + 1
				&& aResult.InsertedEpoch == aInserted;
		}
	} // namespace

	struct epoch_application_host::implementation
	{
		enum class prepared_kind : std::uint8_t {
			observation_only,
			append,
		};

		struct prepared_operation final
		{
			prepared_kind                          Kind = prepared_kind::observation_only;
			core::epoch_operation_id               Operation;
			std::optional<append_operation_record> Record;
			std::optional<git_prepared_revision>   Git;
		};

		// One cache-scoped storage actor is retained across revision and pull
		// request context replacement. Its FIFO serializes catalog/cache effects
		// that share the lifetime-exclusive cache session, while each context
		// remains independently discardable.
		struct authority_effect_actor final
		{
			std::mutex                         QueueMutex;
			std::deque<std::function<void()>>  Queue;
			bool                               Scheduled = false;
			std::unique_ptr<git_cache_session> Cache;
		};

		struct blob_waiter final
		{
			std::mutex                           Mutex;
			std::condition_variable              Completed;
			std::optional<diff_blob_observation> Result;
		};

		struct authority_context final
		{
			authority_context(
				accepted_pull_request_handoff           aAccepted,
				core::pull_request_authority            aAuthority,
				core::accepted_pull_request_revision    aRevision,
				std::unique_ptr<epoch_catalog_session>  aCatalog,
				std::shared_ptr<authority_effect_actor> aActor)
				: Handoff{ std::move(aAccepted) }
				, Authority{ std::move(aAuthority) }
				, Revision{ std::move(aRevision) }
				, Catalog{ std::move(aCatalog) }
				, Actor{ std::move(aActor) }
			{
				Preparations.reserve(maximum_pending_preparations);
			}

			[[nodiscard]] auto store(prepared_operation aOperation) -> bool
			{
				if (
					Preparations.size() >= maximum_pending_preparations
					|| std::ranges::any_of(
						Preparations,
						[&](const auto& aExisting) {
							return aExisting.Operation == aOperation.Operation;
						})) {
					return false;
				}
				Preparations.push_back(std::move(aOperation));
				return true;
			}

			[[nodiscard]] auto take(const core::epoch_operation_id aOperation)
				-> std::optional<prepared_operation>
			{
				const auto found = std::ranges::find(
					Preparations,
					aOperation,
					&prepared_operation::Operation);
				if (found == Preparations.end()) {
					return std::nullopt;
				}
				auto result = std::move(*found);
				Preparations.erase(found);
				return result;
			}

			accepted_pull_request_handoff           Handoff;
			core::pull_request_authority            Authority;
			core::accepted_pull_request_revision    Revision;
			std::unique_ptr<epoch_catalog_session>  Catalog;
			std::shared_ptr<authority_effect_actor> Actor;
			std::vector<prepared_operation>         Preparations;
		};

		struct job_control final
		{
			std::stop_source                           Stop;
			std::optional<core::epoch_request_id>      Request;
			std::optional<core::epoch_generation>      EpochGeneration;
			std::optional<core::comparison_generation> ComparisonGeneration;
			std::optional<std::uint64_t>               OfferToken;
		};

		struct completion_gate final
		{
			std::recursive_mutex         Barrier;
			std::atomic<implementation*> Host           = nullptr;
			std::atomic_bool             DeliveryFailed = false;
		};

		struct deferred_accept final
		{
			std::shared_ptr<accepted_pull_request_handoff> Handoff;
		};
		struct deferred_invalidate final
		{
			pull_request_authority_invalidation Invalidation;
		};
		struct deferred_dispatch final
		{
			epoch_intent Intent;
		};
		struct deferred_shutdown final
		{
		};
		using deferred_ingress = std::variant<
			deferred_accept,
			deferred_invalidate,
			deferred_dispatch,
			deferred_shutdown>;
		using worker_completion = std::function<void(implementation&)>;
		using worker_task       = std::function<worker_completion(std::stop_token)>;

		implementation(
			core::epoch_state      aInitialState,
			git_cache_port&        aCaches,
			epoch_catalog_port&    aCatalogs,
			epoch_entropy_port&    aEntropy,
			epoch_clock_port&      aClock,
			effect_executor&       aEffects,
			decision_executor&     aDecisionLane,
			epoch_projection_port& aProjections)
			: State{ std::move(aInitialState) }
			, Caches{ aCaches }
			, Catalogs{ aCatalogs }
			, Entropy{ aEntropy }
			, Clock{ aClock }
			, Effects{ aEffects }
			, DecisionLane{ aDecisionLane }
			, Projections{ aProjections }
			, CompletionGate{ std::make_shared<completion_gate>() }
		{
			CompletionGate->Host.store(this, std::memory_order_release);
		}

		~implementation()
		{
			const std::lock_guard barrier{ CompletionGate->Barrier };
			CompletionGate->Host.store(nullptr, std::memory_order_release);
		}

		template <typename Action>
		void run_turn(Action&& aAction)
		{
			TurnActive = true;
			try {
				std::forward<Action>(aAction)();
				const auto count = DeferredIngress.size();
				for (std::size_t index = 0U; index < count; ++index) {
					auto ingress = std::move(DeferredIngress.front());
					DeferredIngress.pop_front();
					std::visit(
						[this](auto&& aValue) {
							using value_type = std::decay_t<decltype(aValue)>;
							if constexpr (
								std::is_same_v<value_type, deferred_accept>) {
								accept_now(std::move(*aValue.Handoff));
							} else if constexpr (
								std::is_same_v<value_type, deferred_invalidate>) {
								invalidate_now(aValue.Invalidation);
							} else if constexpr (
								std::is_same_v<value_type, deferred_dispatch>) {
								dispatch_now(aValue.Intent);
							} else {
								shutdown_now();
							}
						},
						std::move(ingress));
				}
				TurnActive = false;
				schedule_deferred_ingress();
			} catch (...) {
				DeferredIngress.clear();
				TurnActive = false;
				throw;
			}
		}

		void schedule_deferred_ingress() noexcept
		{
			if (DeferredIngress.empty() || DeferredIngressScheduled) {
				return;
			}
			DeferredIngressScheduled = true;
			const auto gate          = std::weak_ptr<completion_gate>{ CompletionGate };
			try {
				DecisionLane.post(
					[gate] {
						const auto locked = gate.lock();
						if (!locked) {
							return;
						}
						const std::lock_guard barrier{ locked->Barrier };
						auto* const           host = locked->Host.load(std::memory_order_acquire);
						if (host == nullptr) {
							return;
						}
						host->DeferredIngressScheduled = false;
						host->run_turn([] { });
					});
			} catch (...) {
				DeferredIngressScheduled = false;
				DeferredIngress.clear();
				ProjectionDeliveryFailed = true;
			}
		}

		static void enqueue_completion(
			const std::weak_ptr<completion_gate>& aGate,
			decision_executor* const              aDecisionLane,
			std::shared_ptr<job_control>          aControl,
			worker_completion                     aCompletion) noexcept
		{
			try {
				aDecisionLane->post(
					[aGate,
					 control    = std::move(aControl),
					 completion = std::move(aCompletion)]() mutable {
						const auto locked = aGate.lock();
						if (!locked) {
							return;
						}
						const std::lock_guard barrier{ locked->Barrier };
						auto* const           host = locked->Host.load(std::memory_order_acquire);
						if (host == nullptr) {
							return;
						}
						host->run_turn(
							[host, &control, &completion] {
								host->finish_job(control);
								completion(*host);
							});
					});
			} catch (...) {
				const auto locked = aGate.lock();
				if (!locked) {
					return;
				}
				// A conforming decision lane never rejects. Preserve the live
				// job (and therefore non-quiescence) instead of retiring it or
				// mutating core-owned state from this worker thread.
				locked->DeliveryFailed.store(true, std::memory_order_release);
			}
		}

		static void drain_actor(
			const std::shared_ptr<authority_effect_actor>& aActor) noexcept
		{
			while (true) {
				std::function<void()> work;
				{
					std::scoped_lock lock{ aActor->QueueMutex };
					if (aActor->Queue.empty()) {
						aActor->Scheduled = false;
						return;
					}
					work = std::move(aActor->Queue.front());
					aActor->Queue.pop_front();
				}
				try {
					work();
				} catch (...) {
					// submit_effect workers contain their own typed exception
					// boundary. Keep the actor live if an allocator or foreign
					// callable violates that boundary unexpectedly.
				}
			}
		}

		void enqueue_actor_work(
			const std::shared_ptr<authority_effect_actor>& aActor,
			std::function<void()>                          aWork)
		{
			bool schedule = false;
			{
				std::scoped_lock lock{ aActor->QueueMutex };
				aActor->Queue.push_back(std::move(aWork));
				if (!aActor->Scheduled) {
					aActor->Scheduled = true;
					schedule          = true;
				}
			}
			if (!schedule) {
				return;
			}
			try {
				Effects.post([aActor] {
					drain_actor(aActor);
				});
			} catch (...) {
				std::scoped_lock lock{ aActor->QueueMutex };
				aActor->Scheduled = false;
				aActor->Queue.clear();
				throw;
			}
		}

		auto submit_effect(
			worker_task                                 aTask,
			worker_completion                           aExceptionCompletion,
			const std::optional<core::epoch_request_id> aRequest         = std::nullopt,
			const std::optional<core::epoch_generation> aEpochGeneration = std::nullopt,
			const std::optional<core::comparison_generation>
													aComparisonGeneration = std::nullopt,
			const std::optional<std::uint64_t>      aOfferToken           = std::nullopt,
			std::shared_ptr<authority_effect_actor> aActor                = { }) -> bool
		{
			auto control                  = std::make_shared<job_control>();
			control->Request              = aRequest;
			control->EpochGeneration      = aEpochGeneration;
			control->ComparisonGeneration = aComparisonGeneration;
			control->OfferToken           = aOfferToken;
			Jobs.push_back(control);
			const auto  gate         = std::weak_ptr<completion_gate>{ CompletionGate };
			auto* const decisionLane = &DecisionLane;
			try {
				auto work = [gate,
							 decisionLane,
							 control,
							 aExceptionCompletion,
							 task = std::move(aTask)]() mutable {
					auto completion = worker_completion{ };
					try {
						completion = task(control->Stop.get_token());
					} catch (...) {
						completion = std::move(aExceptionCompletion);
						if (!completion) {
							completion = [](implementation& aHost) {
								aHost.publish_diagnostic(
									core::epoch_reason_code::unavailable);
							};
						}
					}
					enqueue_completion(
						gate,
						decisionLane,
						std::move(control),
						std::move(completion));
				};
				if (aActor) {
					enqueue_actor_work(std::move(aActor), std::move(work));
				} else {
					Effects.post(std::move(work));
				}
				return true;
			} catch (...) {
				finish_job(control);
				if (aExceptionCompletion) {
					aExceptionCompletion(*this);
				} else {
					publish_diagnostic(core::epoch_reason_code::unavailable);
				}
				return false;
			}
		}

		void finish_job(const std::shared_ptr<job_control>& aControl) noexcept
		{
			const auto match = std::ranges::find(Jobs, aControl);
			if (match != Jobs.end()) {
				Jobs.erase(match);
			}
		}

		void cancel_all_jobs() noexcept
		{
			for (const auto& job : Jobs) {
				static_cast<void>(job->Stop.request_stop());
			}
		}

		[[nodiscard]] auto actors_idle() const noexcept -> bool
		{
			auto actors = std::vector<std::shared_ptr<authority_effect_actor>>{ };
			{
				std::scoped_lock lock{ CacheActorsMutex };
				actors.reserve(CacheActors.size());
				for (const auto& [key, actor] : CacheActors) {
					static_cast<void>(key);
					actors.push_back(actor);
				}
			}
			for (const auto& actor : actors) {
				std::scoped_lock lock{ actor->QueueMutex };
				if (actor->Scheduled || !actor->Queue.empty()) {
					return false;
				}
			}
			return true;
		}

		void cancel_authority_jobs() noexcept
		{
			for (const auto& job : Jobs) {
				if (!job->OfferToken) {
					static_cast<void>(job->Stop.request_stop());
				}
			}
		}

		void cancel_offer_job(const std::uint64_t aToken) noexcept
		{
			for (const auto& job : Jobs) {
				if (job->OfferToken == aToken) {
					static_cast<void>(job->Stop.request_stop());
				}
			}
		}

		void cancel_work(const core::cancel_epoch_work_request& aRequest) noexcept
		{
			for (const auto& job : Jobs) {
				if (job->Request != aRequest.Request) {
					continue;
				}
				const bool generationMatches = std::visit(
					[&job](const auto aGeneration) {
						using generation_type = std::remove_cvref_t<decltype(aGeneration)>;
						if constexpr (std::is_same_v<
										  generation_type,
										  core::epoch_generation>) {
							return job->EpochGeneration == aGeneration;
						} else {
							return job->ComparisonGeneration == aGeneration;
						}
					},
					aRequest.Generation);
				if (generationMatches) {
					static_cast<void>(job->Stop.request_stop());
				}
			}
		}

		[[nodiscard]] static auto advance_append(
			epoch_catalog_session&              aCatalogSession,
			const append_operation_record&      aRecord,
			const core::epoch_catalog_identity  aCatalog,
			const epoch_catalog_operation_phase aNext,
			const std::stop_token               aStop)
			-> epoch_catalog_result<append_operation_record>
		{
			const auto evidence = required_transition_evidence(
				epoch_catalog_operation_kind::append,
				aRecord.Phase,
				aNext);
			if (!evidence) {
				return std::unexpected{ epoch_catalog_failure::phase_conflict };
			}
			auto advanced = aCatalogSession.transition_operation(
				operation_transition_request{
					.Catalog                   = aCatalog,
					.Operation                 = aRecord.Context.Operation,
					.Kind                      = epoch_catalog_operation_kind::append,
					.Expected                  = aRecord.Phase,
					.Next                      = aNext,
					.Evidence                  = *evidence,
					.ExpectedOperationRevision = aRecord.OperationRevision,
				},
				aStop);
			if (!advanced) {
				return std::unexpected{ advanced.error() };
			}
			const auto* append = std::get_if<append_operation_record>(&*advanced);
			if (
				append == nullptr
				|| !exact_append_transition(aRecord, *append, aNext)) {
				return std::unexpected{ epoch_catalog_failure::operation_conflict };
			}
			return *append;
		}

		[[nodiscard]] static auto advance_append(
			authority_context&                  aContext,
			const append_operation_record&      aRecord,
			const core::epoch_catalog_identity  aCatalog,
			const epoch_catalog_operation_phase aNext,
			const std::stop_token               aStop)
			-> epoch_catalog_result<append_operation_record>
		{
			return advance_append(
				*aContext.Catalog, aRecord, aCatalog, aNext, aStop);
		}

		[[nodiscard]] static auto advance_maintenance(
			epoch_catalog_session&              aCatalog,
			const maintenance_operation_record& aRecord,
			const core::epoch_catalog_identity  aExpectedCatalog,
			const epoch_catalog_operation_phase aNext,
			const std::stop_token               aStop)
			-> epoch_catalog_result<maintenance_operation_record>
		{
			const auto evidence = required_transition_evidence(aRecord.Kind, aRecord.Phase, aNext);
			if (!evidence) {
				return std::unexpected{ epoch_catalog_failure::phase_conflict };
			}
			auto advanced = aCatalog.transition_operation(
				operation_transition_request{
					.Catalog                   = aExpectedCatalog,
					.Operation                 = aRecord.Operation,
					.Kind                      = aRecord.Kind,
					.Expected                  = aRecord.Phase,
					.Next                      = aNext,
					.Evidence                  = *evidence,
					.ExpectedOperationRevision = aRecord.OperationRevision,
				},
				aStop);
			if (!advanced) {
				return std::unexpected{ advanced.error() };
			}
			const auto* maintenance = std::get_if<maintenance_operation_record>(&*advanced);
			if (maintenance == nullptr
				|| !exact_maintenance_transition(aRecord, *maintenance, aNext)) {
				return std::unexpected{ epoch_catalog_failure::operation_conflict };
			}
			return *maintenance;
		}

		[[nodiscard]] static auto open_actor_cache(authority_effect_actor&       aActor,
												   git_cache_port&               aCaches,
												   const git_cache_open_request& aExpected,
												   const std::stop_token         aStop)
			-> git_cache_result<git_cache_session*>
		{
			if (aActor.Cache && aActor.Cache->request() != aExpected) {
				aActor.Cache.reset();
			}
			if (!aActor.Cache) {
				auto opened = aCaches.open(aExpected, aStop);
				if (!opened) {
					return std::unexpected{ opened.error() };
				}
				if (
					!*opened
					|| !application::valid((*opened)->request())
					|| (*opened)->request() != aExpected) {
					return std::unexpected{
						git_cache_failure::repair_required
					};
				}
				aActor.Cache = std::move(*opened);
			}
			return aActor.Cache.get();
		}

		static void mark_append_repair(
			epoch_catalog_session&         aCatalog,
			const append_operation_record& aRecord)
		{
			if (aRecord.Phase == epoch_catalog_operation_phase::repair_required) {
				return;
			}
			static_cast<void>(advance_append(
				aCatalog,
				aRecord,
				aCatalog.binding().Catalog,
				epoch_catalog_operation_phase::repair_required,
				{ }));
		}

		static void mark_maintenance_repair(epoch_catalog_session&              aCatalog,
											const maintenance_operation_record& aRecord)
		{
			if (aRecord.Phase == epoch_catalog_operation_phase::repair_required) {
				return;
			}
			static_cast<void>(advance_maintenance(aCatalog,
												  aRecord,
												  aCatalog.binding().Catalog,
												  epoch_catalog_operation_phase::repair_required,
												  { }));
		}

		[[nodiscard]] static auto recover_append(
			epoch_catalog_session&  aCatalog,
			git_cache_session&      aCache,
			epoch_entropy_port&     aEntropy,
			append_operation_record aRecord,
			const std::stop_token   aStop)
			-> std::expected<void, core::epoch_failure>
		{
			const auto failedCache = [&](const git_cache_failure aFailure)
				-> std::expected<void, core::epoch_failure> {
				const auto mapped = cache_failure(aFailure);
				if (mapped == core::epoch_failure::repair_required) {
					mark_append_repair(aCatalog, aRecord);
				}
				return std::unexpected{ mapped };
			};
			const auto request = recovery_request(aRecord);
			if (!request) {
				mark_append_repair(aCatalog, aRecord);
				return std::unexpected{ core::epoch_failure::repair_required };
			}
			if (aRecord.Phase == epoch_catalog_operation_phase::repair_required) {
				return std::unexpected{ core::epoch_failure::repair_required };
			}

			const auto beforePointOfNoReturn = aRecord.Phase == epoch_catalog_operation_phase::preparing
				|| aRecord.Phase
					== epoch_catalog_operation_phase::spool_recorded
				|| aRecord.Phase == epoch_catalog_operation_phase::imported
				|| aRecord.Phase
					== epoch_catalog_operation_phase::staging_ready
				|| aRecord.Phase
					== epoch_catalog_operation_phase::abort_requested;
			if (beforePointOfNoReturn) {
				if (
					aRecord.Phase
					!= epoch_catalog_operation_phase::abort_requested) {
					const auto requested = advance_append(
						aCatalog,
						aRecord,
						aCatalog.binding().Catalog,
						epoch_catalog_operation_phase::abort_requested,
						aStop);
					if (!requested) {
						return std::unexpected{
							catalog_failure(requested.error())
						};
					}
					aRecord = *requested;
				}
				auto cleanup = aCache.abort_recovered_operation(*request, aStop);
				if (!cleanup) {
					cleanup = aCache.abort_recovered_operation(*request, aStop);
				}
				if (!cleanup) {
					if (cleanup.error() == git_cache_failure::repair_required) {
						mark_append_repair(aCatalog, aRecord);
					}
					return failedCache(cleanup.error());
				}
				const auto completed = advance_append(
					aCatalog,
					aRecord,
					aCatalog.binding().Catalog,
					epoch_catalog_operation_phase::aborted,
					aStop);
				if (!completed) {
					return std::unexpected{ catalog_failure(completed.error()) };
				}
				return { };
			}

			if (
				aRecord.Phase
					!= epoch_catalog_operation_phase::commit_authorized
				&& aRecord.Phase
					!= epoch_catalog_operation_phase::ref_commit_in_progress
				&& aRecord.Phase
					!= epoch_catalog_operation_phase::refs_committed
				&& aRecord.Phase
					!= epoch_catalog_operation_phase::visible_cleanup_pending) {
				mark_append_repair(aCatalog, aRecord);
				return std::unexpected{ core::epoch_failure::repair_required };
			}
			if (!aRecord.Install || !aRecord.Candidate) {
				mark_append_repair(aCatalog, aRecord);
				return std::unexpected{ core::epoch_failure::repair_required };
			}

			const auto physicalStop = aRecord.Phase
					== epoch_catalog_operation_phase::commit_authorized
				? aStop
				: std::stop_token{ };
			auto       physical     = aCache.inspect_operation_recovery(*request, physicalStop);
			if (!physical) {
				return failedCache(physical.error());
			}
			if (physical->Operation != request->Operation) {
				mark_append_repair(aCatalog, aRecord);
				return std::unexpected{ core::epoch_failure::repair_required };
			}
			const auto durablePhysicalMatch = physical->Installed == git_recovery_match::exact
				&& physical->Objects == git_recovery_object_match::exact
				&& (physical->Spool == git_recovery_match::absent
					|| physical->Spool == git_recovery_match::exact);
			if (!durablePhysicalMatch) {
				mark_append_repair(aCatalog, aRecord);
				return std::unexpected{ core::epoch_failure::repair_required };
			}

			if (
				aRecord.Phase
				== epoch_catalog_operation_phase::commit_authorized) {
				if (physical->Refs != git_recovery_ref_topology::staging) {
					mark_append_repair(aCatalog, aRecord);
					return std::unexpected{ core::epoch_failure::repair_required };
				}
				const auto started = advance_append(
					aCatalog,
					aRecord,
					aCatalog.binding().Catalog,
					epoch_catalog_operation_phase::ref_commit_in_progress,
					aStop);
				if (!started) {
					return std::unexpected{ catalog_failure(started.error()) };
				}
				aRecord = *started;
			}

			const auto postCommitStop = std::stop_token{ };

			if (
				aRecord.Phase
				== epoch_catalog_operation_phase::ref_commit_in_progress) {
				if (physical->Refs == git_recovery_ref_topology::staging) {
					const auto prepared = recovered_prepared_revision(aRecord);
					const auto token    = aEntropy.generate();
					if (!prepared || !token || !application::valid(*token)) {
						return std::unexpected{ core::epoch_failure::unavailable };
					}
					const auto promoted = aCache.promote_retained(
						*prepared,
						git_ref_promotion_authorization{
							.CatalogIncarnation      = aRecord.Context.Catalog.Incarnation,
							.ExpectedCatalogRevision = aRecord.Context.Catalog.Revision,
							.Operation               = aRecord.Context.Operation,
							.Scope                   = aRecord.FinalBase.Authority,
							.Ordinal                 = aRecord.ReservedOrdinal,
							.Base                    = aRecord.Context.Revision.Base,
							.Head                    = aRecord.Context.Revision.Head,
							.DurableCommitToken      = *token,
						},
						postCommitStop);
					if (!promoted) {
						auto afterPromotion = aCache.inspect_operation_recovery(
							*request, postCommitStop);
						if (!afterPromotion) {
							return failedCache(afterPromotion.error());
						}
						if (afterPromotion->Operation != request->Operation) {
							mark_append_repair(aCatalog, aRecord);
							return std::unexpected{
								core::epoch_failure::repair_required
							};
						}
						const auto exactAfter = afterPromotion->Installed
								== git_recovery_match::exact
							&& afterPromotion->Objects
								== git_recovery_object_match::exact
							&& (afterPromotion->Spool
									== git_recovery_match::absent
								|| afterPromotion->Spool
									== git_recovery_match::exact);
						if (
							exactAfter
							&& afterPromotion->Refs
								== git_recovery_ref_topology::final) {
							physical = std::move(afterPromotion);
						} else if (
							exactAfter
							&& afterPromotion->Refs
								== git_recovery_ref_topology::staging) {
							return std::unexpected{
								core::epoch_failure::unavailable
							};
						} else {
							mark_append_repair(aCatalog, aRecord);
							return std::unexpected{
								core::epoch_failure::repair_required
							};
						}
					} else if (*promoted != expected_retained_pair(aRecord)) {
						mark_append_repair(aCatalog, aRecord);
						return std::unexpected{
							core::epoch_failure::repair_required
						};
					}
				} else if (physical->Refs != git_recovery_ref_topology::final) {
					mark_append_repair(aCatalog, aRecord);
					return std::unexpected{ core::epoch_failure::repair_required };
				}
				const auto committed = advance_append(
					aCatalog,
					aRecord,
					aCatalog.binding().Catalog,
					epoch_catalog_operation_phase::refs_committed,
					postCommitStop);
				if (!committed) {
					return std::unexpected{ catalog_failure(committed.error()) };
				}
				aRecord = *committed;
			} else if (
				aRecord.Phase
						== epoch_catalog_operation_phase::visible_cleanup_pending
					? (
						  physical->Final != git_recovery_match::exact
						  || (physical->Staging != git_recovery_match::absent
							  && physical->Staging
								  != git_recovery_match::exact))
					: physical->Refs != git_recovery_ref_topology::final) {
				mark_append_repair(aCatalog, aRecord);
				return std::unexpected{ core::epoch_failure::repair_required };
			}

			const auto retained = aCache.inspect_retained(
				aRecord.ReservedOrdinal, postCommitStop);
			if (!retained) {
				return failedCache(retained.error());
			}
			if (*retained != expected_retained_pair(aRecord)) {
				mark_append_repair(aCatalog, aRecord);
				return std::unexpected{ core::epoch_failure::repair_required };
			}

			auto cleanupCatalog = aCatalog.binding().Catalog;
			if (
				aRecord.Phase == epoch_catalog_operation_phase::refs_committed) {
				if (aCatalog.binding().Catalog != aRecord.Context.Catalog) {
					mark_append_repair(aCatalog, aRecord);
					return std::unexpected{ core::epoch_failure::repair_required };
				}
				const auto finalized = aCatalog.finalize_append(
					finalize_append_request{
						.Catalog                   = aRecord.Context.Catalog,
						.Operation                 = aRecord.Context.Operation,
						.ExpectedOperationRevision = aRecord.OperationRevision,
						.Mutation                  = *aRecord.Candidate,
						.ObservedRefs              = *retained,
					},
					postCommitStop);
				if (!finalized) {
					return std::unexpected{ catalog_failure(finalized.error()) };
				}
				if (
					!exact_catalog_commit(
						*finalized,
						aRecord.Context.Catalog,
						aRecord.Candidate->Epoch.Id)
					|| aCatalog.binding().Catalog != finalized->Catalog) {
					mark_append_repair(aCatalog, aRecord);
					return std::unexpected{
						core::epoch_failure::repair_required
					};
				}
				cleanupCatalog            = finalized->Catalog;
				aRecord.Phase             = epoch_catalog_operation_phase::visible_cleanup_pending;
				aRecord.OperationRevision = epoch_catalog_operation_revision{
					aRecord.OperationRevision.value() + 1
				};
			}

			auto cleaned = aCache.complete_recovered_operation(
				*request, postCommitStop);
			if (!cleaned) {
				const auto afterCleanup = aCache.inspect_operation_recovery(
					*request, postCommitStop);
				if (!afterCleanup) {
					return failedCache(afterCleanup.error());
				}
				const auto exactCleanupState = afterCleanup->Operation == request->Operation
					&& afterCleanup->Installed == git_recovery_match::exact
					&& afterCleanup->Objects
						== git_recovery_object_match::exact
					&& afterCleanup->Final == git_recovery_match::exact
					&& (afterCleanup->Staging == git_recovery_match::absent
						|| afterCleanup->Staging == git_recovery_match::exact)
					&& (afterCleanup->Spool == git_recovery_match::absent
						|| afterCleanup->Spool == git_recovery_match::exact);
				if (!exactCleanupState) {
					mark_append_repair(aCatalog, aRecord);
					return std::unexpected{
						core::epoch_failure::repair_required
					};
				}
				cleaned = aCache.complete_recovered_operation(
					*request, postCommitStop);
				if (!cleaned) {
					return std::unexpected{ core::epoch_failure::unavailable };
				}
			}
			const auto completedResult = advance_append(
				aCatalog,
				aRecord,
				cleanupCatalog,
				epoch_catalog_operation_phase::complete,
				postCommitStop);
			if (!completedResult) {
				return std::unexpected{
					catalog_failure(completedResult.error())
				};
			}
			return { };
		}

		[[nodiscard]] static auto recover_maintenance(
			epoch_catalog_session&               aCatalog,
			git_cache_port&                      aCaches,
			const accepted_pull_request_handoff& aHandoff,
			std::unique_ptr<git_cache_session>&  aCache,
			maintenance_operation_record         aRecord,
			const std::stop_token                aStop)
			-> std::expected<core::epoch_catalog_identity, core::epoch_failure>
		{
			const auto failCatalog = [&](const epoch_catalog_failure aFailure)
				-> std::expected<core::epoch_catalog_identity, core::epoch_failure> {
				const auto mapped = catalog_failure(aFailure);
				if (mapped == core::epoch_failure::repair_required) {
					mark_maintenance_repair(aCatalog, aRecord);
				}
				return std::unexpected{ mapped };
			};
			const auto failCache = [&](const git_cache_failure aFailure)
				-> std::expected<core::epoch_catalog_identity, core::epoch_failure> {
				const auto mapped = cache_failure(aFailure);
				if (mapped == core::epoch_failure::repair_required) {
					mark_maintenance_repair(aCatalog, aRecord);
				}
				return std::unexpected{ mapped };
			};
			if (!application::valid(aRecord)
				|| aRecord.Incarnation != aCatalog.binding().Catalog.Incarnation
				|| aRecord.Cache != aCatalog.binding().Authority.Cache) {
				mark_maintenance_repair(aCatalog, aRecord);
				return std::unexpected{ core::epoch_failure::repair_required };
			}
			if (aRecord.Phase == epoch_catalog_operation_phase::repair_required) {
				return std::unexpected{ core::epoch_failure::repair_required };
			}
			if (aRecord.Phase == epoch_catalog_operation_phase::complete
				|| aRecord.Phase == epoch_catalog_operation_phase::aborted) {
				return aCatalog.binding().Catalog;
			}

			const auto beforePointOfNoReturn = !point_of_no_cancellation(aRecord.Phase);
			if (beforePointOfNoReturn) {
				if (aRecord.Phase != epoch_catalog_operation_phase::abort_requested) {
					const auto requested = advance_maintenance(aCatalog,
															   aRecord,
															   aCatalog.binding().Catalog,
															   epoch_catalog_operation_phase::abort_requested,
															   aStop);
					if (!requested) {
						return failCatalog(requested.error());
					}
					aRecord = *requested;
				}
				const auto aborted = advance_maintenance(aCatalog,
														 aRecord,
														 aCatalog.binding().Catalog,
														 epoch_catalog_operation_phase::aborted,
														 aStop);
				if (!aborted) {
					return failCatalog(aborted.error());
				}
				return aCatalog.binding().Catalog;
			}

			const auto expectedCache = git_cache_open_request{
				.ApplicationSession = aHandoff.ApplicationSession,
				.Authority          = aCatalog.binding().Authority,
				.ObjectFormat       = aHandoff.ObjectFormat,
			};
			const auto durableToken  = aRecord.Operation.bytes();
			const auto postPointStop = std::stop_token{ };

			if (aRecord.Kind == epoch_catalog_operation_kind::prune) {
				if (aRecord.Phase
					== epoch_catalog_operation_phase::ref_delete_in_progress) {
					if (!aCache) {
						auto opened = aCaches.open(expectedCache, postPointStop);
						if (!opened) {
							return failCache(opened.error());
						}
						if (!*opened || !application::valid((*opened)->request())
							|| (*opened)->request() != expectedCache) {
							mark_maintenance_repair(aCatalog, aRecord);
							return std::unexpected{
								core::epoch_failure::repair_required
							};
						}
						aCache = std::move(*opened);
					}
					std::vector<git_retained_ref_pair> absent;
					absent.reserve(aRecord.Epochs.size());
					for (const auto& fact : aRecord.Epochs) {
						const auto pruned = aCache->prune_retained(
							git_ref_prune_authorization{
								.CatalogIncarnation      = aRecord.Incarnation,
								.ExpectedCatalogRevision = aCatalog.binding().Catalog.Revision,
								.Expected                = fact.Retained,
								.DurableIntentToken      = durableToken,
							},
							postPointStop);
						if (!pruned) {
							return failCache(pruned.error());
						}
						absent.push_back(fact.Retained);
					}
					const auto before    = aCatalog.binding().Catalog;
					const auto finalized = aCatalog.finalize_prune(
						finalize_prune_request{
							.Transition = operation_transition_request{
								.Catalog   = before,
								.Operation = aRecord.Operation,
								.Kind      = aRecord.Kind,
								.Expected  = aRecord.Phase,
								.Next      = epoch_catalog_operation_phase::
									visible_cleanup_pending,
								.Evidence = epoch_catalog_transition_evidence::
									exact_refs_absent,
								.ExpectedOperationRevision = aRecord.OperationRevision,
							},
							.ObservedAbsent = std::move(absent),
						},
						postPointStop);
					if (!finalized) {
						return failCatalog(finalized.error());
					}
					if (!exact_catalog_commit(*finalized, before, std::nullopt)) {
						mark_maintenance_repair(aCatalog, aRecord);
						return std::unexpected{
							core::epoch_failure::repair_required
						};
					}
					aRecord.Phase             = epoch_catalog_operation_phase::visible_cleanup_pending;
					aRecord.OperationRevision = epoch_catalog_operation_revision{
						aRecord.OperationRevision.value() + 1
					};
				}
				if (aRecord.Phase
					!= epoch_catalog_operation_phase::visible_cleanup_pending) {
					mark_maintenance_repair(aCatalog, aRecord);
					return std::unexpected{ core::epoch_failure::repair_required };
				}
				const auto completed = advance_maintenance(aCatalog,
														   aRecord,
														   aCatalog.binding().Catalog,
														   epoch_catalog_operation_phase::complete,
														   postPointStop);
				if (!completed) {
					return failCatalog(completed.error());
				}
				return aCatalog.binding().Catalog;
			}

			if (aRecord.Kind != epoch_catalog_operation_kind::cache_delete) {
				mark_maintenance_repair(aCatalog, aRecord);
				return std::unexpected{ core::epoch_failure::repair_required };
			}
			const auto deletePhysicalCache =
				[&]() -> std::expected<void, core::epoch_failure> {
				aCache.reset();
				const auto deleted = aCaches.delete_cache(expectedCache,
														  git_cache_delete_authorization{
															  .ApplicationSession = aHandoff.ApplicationSession,
															  .Cache              = aRecord.Cache,
															  .DurableIntentToken = durableToken,
														  },
														  postPointStop);
				if (!deleted) {
					const auto mapped = cache_failure(deleted.error());
					if (mapped == core::epoch_failure::repair_required) {
						mark_maintenance_repair(aCatalog, aRecord);
					}
					return std::unexpected{ mapped };
				}
				return { };
			};
			if (aRecord.Phase
				== epoch_catalog_operation_phase::quarantine_rename_in_progress) {
				if (const auto deleted = deletePhysicalCache(); !deleted) {
					return std::unexpected{ deleted.error() };
				}
				const auto quarantined = advance_maintenance(aCatalog,
															 aRecord,
															 aCatalog.binding().Catalog,
															 epoch_catalog_operation_phase::quarantined,
															 postPointStop);
				if (!quarantined) {
					return failCatalog(quarantined.error());
				}
				aRecord = *quarantined;
			}
			if (aRecord.Phase == epoch_catalog_operation_phase::quarantined) {
				const auto before    = aCatalog.binding();
				const auto finalized = aCatalog.finalize_cache_delete(
					finalize_cache_delete_request{
						.Transition = operation_transition_request{
							.Catalog   = before.Catalog,
							.Operation = aRecord.Operation,
							.Kind      = aRecord.Kind,
							.Expected  = aRecord.Phase,
							.Next      = epoch_catalog_operation_phase::
								catalog_tombstoned_cleanup_pending,
							.Evidence = epoch_catalog_transition_evidence::
								catalog_transaction,
							.ExpectedOperationRevision = aRecord.OperationRevision,
						},
					},
					postPointStop);
				if (!finalized) {
					return failCatalog(finalized.error());
				}
				const auto& after = aCatalog.binding();
				if (!application::valid(after) || after.Authority != before.Authority
					|| after.Catalog.Incarnation != before.Catalog.Incarnation
					|| before.Catalog.Revision.value()
						>= core::maximum_epoch_catalog_revision
					|| after.Catalog.Revision.value()
						!= before.Catalog.Revision.value() + 1) {
					mark_maintenance_repair(aCatalog, aRecord);
					return std::unexpected{ core::epoch_failure::repair_required };
				}
				aRecord.Phase = epoch_catalog_operation_phase::
					catalog_tombstoned_cleanup_pending;
				aRecord.OperationRevision = epoch_catalog_operation_revision{
					aRecord.OperationRevision.value() + 1
				};
			}
			if (aRecord.Phase
				!= epoch_catalog_operation_phase::
					catalog_tombstoned_cleanup_pending) {
				mark_maintenance_repair(aCatalog, aRecord);
				return std::unexpected{ core::epoch_failure::repair_required };
			}
			if (const auto deleted = deletePhysicalCache(); !deleted) {
				return std::unexpected{ deleted.error() };
			}
			const auto completed = advance_maintenance(aCatalog,
													   aRecord,
													   aCatalog.binding().Catalog,
													   epoch_catalog_operation_phase::complete,
													   postPointStop);
			if (!completed) {
				return failCatalog(completed.error());
			}
			return aCatalog.binding().Catalog;
		}

		[[nodiscard]] static auto recover_catalog(epoch_catalog_session&               aCatalog,
												  git_cache_port&                      aCaches,
												  epoch_entropy_port&                  aEntropy,
												  const accepted_pull_request_handoff& aHandoff,
												  std::unique_ptr<git_cache_session>&  aCache,
												  const std::stop_token                aStop)
			-> std::expected<void, core::epoch_failure>
		{
			std::optional<core::epoch_operation_id> after;
			std::size_t                             recoveredCount = 0U;
			while (true) {
				const auto page = aCatalog.recovery_page(
					after, maximum_epoch_recovery_page_size, aStop);
				if (!page) {
					return std::unexpected{ catalog_failure(page.error()) };
				}
				if (
					!application::valid(*page)
					|| page->Incarnation
						!= aCatalog.binding().Catalog.Incarnation
					|| (page->Continuation
						&& page->Operations.empty())
					|| (after && page->Operations.empty())
					|| (after && page->Continuation
						&& !(*after < *page->Continuation))
					|| page->Operations.size()
						> maximum_recovery_operations - recoveredCount) {
					return std::unexpected{ core::epoch_failure::repair_required };
				}
				if (after && !page->Operations.empty()) {
					const auto first = recovery_operation_identity(
						page->Operations.front());
					// SQLite's continuation is the first omitted identity, so
					// the next page includes an identity equal to `after`.
					if (first != *after) {
						return std::unexpected{
							core::epoch_failure::repair_required
						};
					}
				}
				recoveredCount += page->Operations.size();
				for (const auto& operation : page->Operations) {
					const auto* append = std::get_if<append_operation_record>(&operation);
					if (append == nullptr) {
						const auto* maintenance = std::get_if<maintenance_operation_record>(&operation);
						if (maintenance == nullptr) {
							return std::unexpected{
								core::epoch_failure::repair_required
							};
						}
						const auto recovered = recover_maintenance(aCatalog,
																   aCaches,
																   aHandoff,
																   aCache,
																   *maintenance,
																   aStop);
						if (!recovered) {
							return std::unexpected{ recovered.error() };
						}
						continue;
					}
					if (!aCache) {
						const auto expected = git_cache_open_request{
							.ApplicationSession = aHandoff.ApplicationSession,
							.Authority          = aCatalog.binding().Authority,
							.ObjectFormat       = aHandoff.ObjectFormat,
						};
						auto opened = aCaches.open(expected, aStop);
						if (!opened) {
							return std::unexpected{ cache_failure(opened.error()) };
						}
						if (!*opened) {
							return std::unexpected{
								core::epoch_failure::repair_required
							};
						}
						if (
							!application::valid((*opened)->request())
							|| (*opened)->request() != expected) {
							return std::unexpected{
								core::epoch_failure::repair_required
							};
						}
						aCache = std::move(*opened);
					}
					const auto recovered = recover_append(
						aCatalog, *aCache, aEntropy, *append, aStop);
					if (!recovered) {
						return recovered;
					}
				}
				if (!page->Continuation) {
					return { };
				}
				after = page->Continuation;
			}
		}

		[[nodiscard]] static auto pending_append(
			epoch_catalog_session&         aCatalog,
			const core::epoch_operation_id aOperation)
			-> std::expected<
				std::optional<append_operation_record>,
				core::epoch_failure>
		{
			auto page = aCatalog.recovery_page(aOperation, 1U, { });
			if (!page) {
				return std::unexpected{ core::epoch_failure::repair_required };
			}
			if (
				!application::valid(*page)
				|| page->Incarnation
					!= aCatalog.binding().Catalog.Incarnation
				|| page->Operations.size() > 1U
				|| (page->Continuation && page->Operations.empty())) {
				return std::unexpected{ core::epoch_failure::repair_required };
			}
			if (page->Operations.empty()) {
				return std::optional<append_operation_record>{ };
			}
			const auto* append = std::get_if<append_operation_record>(&page->Operations.front());
			const auto  found  = recovery_operation_identity(
				page->Operations.front());
			if (found < aOperation) {
				return std::unexpected{ core::epoch_failure::repair_required };
			}
			if (found != aOperation) {
				return std::optional<append_operation_record>{ };
			}
			if (append == nullptr) {
				return std::unexpected{ core::epoch_failure::repair_required };
			}
			return std::optional{ *append };
		}

		[[nodiscard]] static auto abort_preparation(
			authority_context&  aContext,
			prepared_operation& aOperation) -> bool
		{
			if (aOperation.Kind == prepared_kind::observation_only) {
				return true;
			}
			if (!aOperation.Record) {
				return false;
			}
			auto record = *aOperation.Record;
			if (record.Phase == epoch_catalog_operation_phase::aborted
				|| record.Phase == epoch_catalog_operation_phase::complete) {
				return true;
			}
			if (record.Phase == epoch_catalog_operation_phase::repair_required
				|| point_of_no_cancellation(record.Phase)) {
				return false;
			}
			const auto markRepair = [&aContext, &record, &aOperation]() {
				const auto repaired = advance_append(
					aContext,
					record,
					record.Context.Catalog,
					epoch_catalog_operation_phase::repair_required,
					{ });
				if (repaired) {
					record            = *repaired;
					aOperation.Record = record;
				}
			};
			if (record.Phase != epoch_catalog_operation_phase::abort_requested) {
				const auto requested = advance_append(
					aContext,
					record,
					record.Context.Catalog,
					epoch_catalog_operation_phase::abort_requested,
					{ });
				if (!requested) {
					markRepair();
					return false;
				}
				record = *requested;
			}
			if (aContext.Actor->Cache) {
				const auto request = recovery_request(record);
				if (!request) {
					markRepair();
					return false;
				}
				auto aborted = aContext.Actor->Cache->abort_recovered_operation(
					*request, { });
				if (!aborted) {
					aborted = aContext.Actor->Cache->abort_recovered_operation(
						*request, { });
				}
				if (!aborted) {
					markRepair();
					return false;
				}
			}
			const auto completed = advance_append(
				aContext,
				record,
				record.Context.Catalog,
				epoch_catalog_operation_phase::aborted,
				{ });
			if (!completed) {
				markRepair();
				return false;
			}
			aOperation.Record = *completed;
			return true;
		}

		void discard_context_preparation(
			std::shared_ptr<authority_context>            aContext,
			const std::optional<core::epoch_operation_id> aExpected = std::nullopt)
		{
			auto        actor  = aContext->Actor;
			auto* const caches = &Caches;
			submit_effect(
				[context = std::move(aContext), aExpected, caches](
					const std::stop_token aStop)
					-> worker_completion {
					const auto requiresCache = std::ranges::any_of(
						context->Preparations,
						[&](const auto& aOperation) {
							return aOperation.Kind == prepared_kind::append
								&& (!aExpected
									|| aOperation.Operation == *aExpected);
						});
					if (requiresCache) {
						const auto cache = open_actor_cache(
							*context->Actor,
							*caches,
							git_cache_open_request{
								.ApplicationSession = context->Handoff.ApplicationSession,
								.Authority          = context->Authority,
								.ObjectFormat       = context->Handoff.ObjectFormat,
							},
							aStop);
						if (!cache) {
							return [](implementation& aHost) {
								aHost.publish_diagnostic(
									core::epoch_reason_code::unavailable);
							};
						}
					}
					if (aExpected) {
						auto operation = context->take(*aExpected);
						if (operation) {
							static_cast<void>(
								abort_preparation(*context, *operation));
						}
					} else {
						while (!context->Preparations.empty()) {
							auto operation = context->take(
								context->Preparations.back().Operation);
							if (operation) {
								static_cast<void>(
									abort_preparation(*context, *operation));
							}
						}
					}
					return [](implementation&) { };
				},
				[](implementation& aHost) {
					aHost.publish_diagnostic(
						core::epoch_reason_code::unavailable);
				},
				std::nullopt,
				std::nullopt,
				std::nullopt,
				std::nullopt,
				std::move(actor));
		}

		void publish_diagnostic(const core::epoch_reason_code aReason) noexcept
		{
			auto update       = epoch_presentation_update{ };
			update.Transition = epoch_transition_projection{
				.Changed     = core::epoch_changed_domain::diagnostics,
				.Diagnostics = { core::epoch_diagnostic{ .Code = aReason } },
			};
			publish(update);
		}

		void publish(const epoch_presentation_update& aUpdate) noexcept
		{
			try {
				Projections.publish(aUpdate);
			} catch (...) {
				ProjectionDeliveryFailed = true;
			}
		}

		void publish_transition(const core::epoch_transition_output& aOutput)
		{
			if (aOutput.Changed == core::epoch_changed_domain::none
				&& aOutput.Diagnostics.empty()) {
				return;
			}
			epoch_presentation_update   update;
			epoch_transition_projection transition{
				.Changed     = aOutput.Changed,
				.Selection   = aOutput.Selection,
				.Status      = aOutput.Status,
				.Commands    = aOutput.Commands,
				.Diagnostics = aOutput.Diagnostics,
			};
			if (aOutput.Authority) {
				transition.Authority = epoch_authority_projection{
					.Authority  = aOutput.Authority->Authority,
					.Generation = aOutput.Authority->Generation,
				};
				if (aOutput.Authority->Revision) {
					transition.Authority->Revision = projected_revision(*aOutput.Authority->Revision);
				}
			}
			if (aOutput.Comparison) {
				auto reason = core::epoch_reason_code::none;
				if (!aOutput.Comparison->Pair && !aOutput.Diagnostics.empty()) {
					reason = aOutput.Diagnostics.back().Code;
				}
				transition.Comparison = epoch_comparison_projection{
					.Selection        = aOutput.Comparison->Selection,
					.Pair             = aOutput.Comparison->Pair,
					.Generation       = State.ComparisonGeneration,
					.Revision         = aOutput.Comparison->Revision,
					.AvailableForDiff = aOutput.Comparison->AvailableForDiff,
					.Reason           = reason,
				};
			}
			update.Transition = std::move(transition);

			if (aOutput.Catalog || aOutput.AcceptedEpoch || aOutput.History) {
				update.Catalog = epoch_catalog_projection{ };
				if (aOutput.Catalog) {
					update.Catalog->Catalog                   = aOutput.Catalog->Catalog;
					update.Catalog->Replace                   = !aOutput.Catalog->Catalog.has_value();
					update.Catalog->AvailabilityChangedEpochs = aOutput.Catalog->AvailabilityChangedEpochs;
					update.Catalog->AvailabilityChangedTo     = aOutput.Catalog->AvailabilityChangedTo;
					update.Catalog->AllEpochAvailability      = aOutput.Catalog->AllEpochAvailability;
				}
				if (aOutput.AcceptedEpoch) {
					update.Catalog->Catalog = core::epoch_catalog_identity{
						.Incarnation = aOutput.AcceptedEpoch->Epoch.Id.Incarnation,
						.Revision    = aOutput.AcceptedEpoch->CatalogRevision,
					};
					const bool historyWindowInvalidated = aOutput.History
						&& aOutput.History->Phase
							== core::epoch_history_page_phase::invalidated;
					if (!historyWindowInvalidated) {
						update.Catalog->Changes.push_back(
							epoch_row_change_projection{
								.Kind = epoch_row_change_kind::insert,
								.Id   = aOutput.AcceptedEpoch->Epoch.Id,
								.Row  = projected_row(aOutput.AcceptedEpoch->Epoch),
							});
					}
				}
				if (aOutput.History) {
					if (!aOutput.Catalog) {
						update.Catalog->Catalog = aOutput.History->Catalog;
					}
					auto page = epoch_history_page_projection{
						.Phase           = aOutput.History->Phase,
						.Catalog         = aOutput.History->Catalog,
						.Navigation      = aOutput.History->Navigation,
						.RequestedCursor = aOutput.History->RequestedCursor,
						.Limit           = aOutput.History->Limit,
						.Continuation    = aOutput.History->Continuation,
						.Reason          = aOutput.History->Reason,
					};
					page.Rows.reserve(aOutput.History->Epochs.size());
					for (const auto& epoch : aOutput.History->Epochs) {
						page.Rows.push_back(projected_row(epoch));
					}
					update.Catalog->HistoryPage = std::move(page);
				}
			}
			publish(update);
		}

		auto dispatch_core(
			const core::epoch_event& aEvent,
			const std::function<void(const core::epoch_transition_output&)>&
				aBeforeRequests = { })
			-> core::epoch_transition_output
		{
			auto output = core::transition(State, aEvent);
			publish_transition(output);
			if (aBeforeRequests) {
				aBeforeRequests(output);
			}
			for (const auto& request : output.Requests) {
				handle_request(request);
			}
			return output;
		}

		void handle_request(const core::epoch_host_request& aRequest)
		{
			std::visit(
				[this](const auto& aValue) {
					using request_type = std::decay_t<decltype(aValue)>;
					if constexpr (std::is_same_v<
									  request_type,
									  core::invalidate_epoch_authority_request>) {
						cancel_authority_jobs();
					} else if constexpr (std::is_same_v<
											 request_type,
											 core::cancel_epoch_work_request>) {
						cancel_work(aValue);
					} else if constexpr (std::is_same_v<
											 request_type,
											 core::load_epoch_catalog_request>) {
						start_catalog_load(aValue);
					} else if constexpr (std::is_same_v<
											 request_type,
											 core::prepare_epoch_revision_request>) {
						start_revision_preparation(aValue);
					} else if constexpr (std::is_same_v<
											 request_type,
											 core::commit_epoch_catalog_request>) {
						start_catalog_commit(aValue);
					} else if constexpr (std::is_same_v<
											 request_type,
											 core::discard_epoch_preparation_request>) {
						discard_preparation(aValue);
					} else if constexpr (std::is_same_v<
											 request_type,
											 core::load_epoch_history_page_request>) {
						start_history_page(aValue);
					} else if constexpr (std::is_same_v<request_type,
														core::perform_epoch_maintenance_request>) {
						start_epoch_maintenance(aValue);
					} else {
						start_comparison_resolution(aValue);
					}
				},
				aRequest);
		}

		[[nodiscard]] auto exact_context(
			const core::pull_request_authority&         aAuthority,
			const core::accepted_pull_request_revision& aRevision) const
			-> std::shared_ptr<authority_context>
		{
			if (Active && Active->Authority == aAuthority
				&& Active->Revision == aRevision) {
				return Active;
			}
			return { };
		}

		[[nodiscard]] auto actor_for(
			const accepted_pull_request_handoff& aHandoff)
			-> std::shared_ptr<authority_effect_actor>
		{
			std::scoped_lock lock{ CacheActorsMutex };
			const auto       key   = actor_key(aHandoff);
			const auto       found = std::ranges::find(
				CacheActors, key, &decltype(CacheActors)::value_type::first);
			if (found != CacheActors.end()) {
				return found->second;
			}
			if (CacheActors.size() >= maximum_cache_actors) {
				return { };
			}
			auto actor = std::make_shared<authority_effect_actor>();
			CacheActors.emplace_back(key, actor);
			return actor;
		}

		[[nodiscard]] auto materialize(
			const diff_blob_request& aRequest,
			const std::stop_token    aStop) -> diff_blob_observation
		{
			auto result = diff_blob_observation{
				.Context = aRequest.Context,
				.Side    = aRequest.Side,
				.Blob    = aRequest.Blob,
				.Status  = diff_blob_status::failure,
			};
			const auto key = actor_key(aRequest);
			if (
				!key.has_value()
				|| aRequest.MaximumBytes
					> maximum_git_cache_blob_materialization_bytes
				|| BlobIngressClosing.load(std::memory_order_acquire)) {
				return result;
			}

			auto actor = std::shared_ptr<authority_effect_actor>{ };
			{
				std::scoped_lock lock{ CacheActorsMutex };
				const auto       found = std::ranges::find(
					CacheActors, *key, &decltype(CacheActors)::value_type::first);
				if (found != CacheActors.end()) {
					actor = found->second;
				}
			}
			if (!actor) {
				return result;
			}

			auto       waiter   = std::make_shared<blob_waiter>();
			const auto expected = git_cache_open_request{
				.ApplicationSession = aRequest.Context.ApplicationSession,
				.Authority          = aRequest.Context.Key.Authority,
				.ObjectFormat       = aRequest.Blob.algorithm(),
			};
			try {
				enqueue_actor_work(
					actor,
					[this, actor, expected, aRequest, aStop, waiter] {
						auto observation = diff_blob_observation{
							.Context = aRequest.Context,
							.Side    = aRequest.Side,
							.Blob    = aRequest.Blob,
							.Status  = diff_blob_status::failure,
						};
						if (
							aStop.stop_requested()
							|| BlobIngressClosing.load(std::memory_order_acquire)) {
							observation.Status = diff_blob_status::cancelled;
						} else if (
							actor->Cache != nullptr
							&& actor->Cache->request() == expected) {
							auto bytes = actor->Cache->materialize_blob(
								git_cache_blob_request{
									.Blob         = aRequest.Blob,
									.MaximumBytes = aRequest.MaximumBytes,
								},
								aStop);
							if (bytes.has_value()) {
								observation.Status = diff_blob_status::available;
								observation.Bytes  = std::move(*bytes);
							} else if (bytes.error() == git_cache_failure::cancelled) {
								observation.Status = diff_blob_status::cancelled;
							} else if (bytes.error() == git_cache_failure::quota_exceeded) {
								observation.Status = diff_blob_status::materialization_limit;
							} else if (bytes.error() == git_cache_failure::object_mismatch) {
								observation.Status = diff_blob_status::missing;
							}
						}
						{
							std::scoped_lock lock{ waiter->Mutex };
							waiter->Result = std::move(observation);
						}
						waiter->Completed.notify_all();
					});
			} catch (...) {
				return result;
			}

			std::unique_lock lock{ waiter->Mutex };
			waiter->Completed.wait(lock, [&waiter] {
				return waiter->Result.has_value();
			});
			return std::move(*waiter->Result);
		}

		void accept(accepted_pull_request_handoff aHandoff)
		{
			if (Closing || !application::valid(aHandoff)
				|| aHandoff.ApplicationSession != State.ApplicationSession) {
				return;
			}
			auto accepted = std::make_shared<accepted_pull_request_handoff>(
				std::move(aHandoff));
			if (TurnActive || DeferredIngressScheduled || !DeferredIngress.empty()) {
				DeferredIngress.emplace_back(deferred_accept{ std::move(accepted) });
				schedule_deferred_ingress();
				return;
			}
			run_turn([this, &accepted] {
				accept_now(std::move(*accepted));
			});
		}

		void accept_now(accepted_pull_request_handoff aHandoff)
		{
			if (Closing) {
				return;
			}
			++OfferToken;
			if (PendingOfferToken) {
				cancel_offer_job(*PendingOfferToken);
			}
			auto actor = actor_for(aHandoff);
			if (!actor) {
				PendingAuthority.reset();
				PendingOfferToken.reset();
				if (aHandoff.SmartGit) {
					aHandoff.SmartGit->revoke();
				}
				publish_diagnostic(core::epoch_reason_code::quota_exceeded);
				return;
			}
			const bool preservesActiveAuthority = Active && agrees(Active->Authority, aHandoff);
			if (Active) {
				cancel_authority_jobs();
			}
			if (Active && !preservesActiveAuthority) {
				auto retired = std::move(Active);
				if (retired->Handoff.SmartGit) {
					retired->Handoff.SmartGit->revoke();
				}
				dispatch_core(core::pull_request_revision_invalidated_event{
					.ApplicationSession = State.ApplicationSession,
					.Authority          = retired->Authority,
					.Revision           = retired->Revision,
					.Generation         = State.EpochGeneration,
				});
				discard_context_preparation(std::move(retired));
			}

			const auto token   = OfferToken;
			auto       offered = std::make_shared<accepted_pull_request_handoff>(
				std::move(aHandoff));
			PendingAuthority     = invalidation_identity(*offered);
			PendingOfferToken    = token;
			auto* const catalogs = &Catalogs;
			auto* const caches   = &Caches;
			auto* const entropy  = &Entropy;
			// Keep worker and scheduling ownership distinct before entering the
			// call. Capturing and moving one local in separate arguments would
			// make the worker's actor depend on argument evaluation order.
			auto       workerActor = actor;
			const auto submitted   = submit_effect(
				[offered,
				 actor = std::move(workerActor),
				 catalogs,
				 caches,
				 entropy,
				 token](
					const std::stop_token aStop) mutable -> worker_completion {
					struct lease_guard final
					{
						std::shared_ptr<accepted_pull_request_handoff> Offered;

						~lease_guard()
						{
							if (Offered && Offered->SmartGit) {
								Offered->SmartGit->revoke();
							}
						}
					} lease{ offered };
					const auto diagnostic = [token](
												const core::epoch_reason_code aReason)
						-> worker_completion {
						return [token, aReason](implementation& aHost) {
							if (token != aHost.OfferToken) {
								return;
							}
							aHost.PendingAuthority.reset();
							aHost.PendingOfferToken.reset();
							aHost.publish_diagnostic(aReason);
						};
					};
					if (aStop.stop_requested()) {
						return [](implementation&) { };
					}
					auto opened = catalogs->open(
						epoch_catalog_open_request{
							.ApplicationSession = offered->ApplicationSession,
							.Authority          = catalog_key(*offered),
						},
						aStop);
					if (!opened) {
						const auto reason = core::reason_for(
							catalog_failure(opened.error()));
						return diagnostic(reason);
					}
					if (!*opened) {
						return diagnostic(
							core::epoch_reason_code::repair_required);
					}

					const auto initialBinding = (*opened)->binding();
					if (!application::valid(initialBinding)
						|| initialBinding.ObjectFormat != offered->ObjectFormat
						|| !agrees(initialBinding.Authority, *offered)) {
						return diagnostic(core::epoch_reason_code::invalid_identity);
					}

					const auto expectedCache = git_cache_open_request{
						.ApplicationSession = offered->ApplicationSession,
						.Authority          = initialBinding.Authority,
						.ObjectFormat       = offered->ObjectFormat,
					};
					if (actor->Cache && actor->Cache->request() != expectedCache) {
						actor->Cache.reset();
					}
					const auto recovered = recover_catalog(
						**opened,
						*caches,
						*entropy,
						*offered,
						actor->Cache,
						aStop);
					if (!recovered) {
						return diagnostic(core::reason_for(recovered.error()));
					}

					std::optional<core::epoch_source_observation_id> source;
					for (std::size_t attempt = 0U;
						 attempt < maximum_identity_attempts;
						 ++attempt) {
						source = mint_identity<core::epoch_source_observation_id>(
							*entropy);
						if (!source) {
							break;
						}
						const auto exists = (*opened)->source_observation_exists(*source, aStop);
						if (!exists) {
							const auto reason = core::reason_for(
								catalog_failure(exists.error()));
							return diagnostic(reason);
						}
						if (!*exists) {
							break;
						}
						source.reset();
					}
					if (!source || aStop.stop_requested()) {
						return diagnostic(core::epoch_reason_code::unavailable);
					}

					const auto binding = (*opened)->binding();
					if (!application::valid(binding)
						|| binding.ObjectFormat != offered->ObjectFormat
						|| !agrees(binding.Authority, *offered)) {
						return diagnostic(core::epoch_reason_code::invalid_identity);
					}
					auto revision = accepted_revision(*offered, *source);
					auto context  = std::make_shared<authority_context>(
						std::move(*offered),
						binding.Authority,
						revision,
						std::move(*opened),
						std::move(actor));
					return [context = std::move(context), token](implementation& aHost) {
						if (aHost.Closing || token != aHost.OfferToken) {
							if (context->Handoff.SmartGit) {
								context->Handoff.SmartGit->revoke();
							}
							return;
						}
						aHost.PendingAuthority.reset();
						aHost.PendingOfferToken.reset();
						auto previous = aHost.Active;
						aHost.Active  = context;
						bool accepted = false;
						static_cast<void>(aHost.dispatch_core(
							core::pull_request_revision_accepted_event{
								.ApplicationSession = context->Handoff.ApplicationSession,
								.Authority          = context->Authority,
								.Revision           = context->Revision,
							},
							[&](const core::epoch_transition_output&) {
								accepted = aHost.State.Authority
									&& aHost.State.AcceptedRevision
									&& *aHost.State.Authority
										== context->Authority
									&& *aHost.State.AcceptedRevision
										== context->Revision;
								if (accepted && previous) {
									if (previous->Handoff.SmartGit) {
										previous->Handoff.SmartGit->revoke();
									}
									aHost.discard_context_preparation(
										std::move(previous));
								}
							}));
						if (!accepted) {
							const auto previousStillOwned = previous
								&& aHost.State.Authority
								&& aHost.State.AcceptedRevision
								&& *aHost.State.Authority == previous->Authority
								&& *aHost.State.AcceptedRevision
									== previous->Revision;
							if (previousStillOwned) {
								aHost.Active = std::move(previous);
								if (context->Handoff.SmartGit) {
									context->Handoff.SmartGit->revoke();
								}
								return;
							}
							aHost.Active.reset();
							if (context->Handoff.SmartGit) {
								context->Handoff.SmartGit->revoke();
							}
							if (previous) {
								if (previous->Handoff.SmartGit) {
									previous->Handoff.SmartGit->revoke();
								}
								aHost.discard_context_preparation(
									std::move(previous));
							}
							return;
						}
					};
				},
				[token](implementation& aHost) {
					if (token == aHost.OfferToken) {
						aHost.PendingAuthority.reset();
						aHost.PendingOfferToken.reset();
						aHost.publish_diagnostic(
							core::epoch_reason_code::unavailable);
					}
				},
				std::nullopt,
				std::nullopt,
				std::nullopt,
				token,
				std::move(actor));
			if (!submitted && token == OfferToken) {
				PendingAuthority.reset();
				PendingOfferToken.reset();
				if (offered->SmartGit) {
					offered->SmartGit->revoke();
				}
			}
		}

		void invalidate(const pull_request_authority_invalidation aInvalidation)
		{
			if (Closing || !application::valid(aInvalidation)) {
				return;
			}
			if (TurnActive || DeferredIngressScheduled || !DeferredIngress.empty()) {
				DeferredIngress.emplace_back(deferred_invalidate{ aInvalidation });
				schedule_deferred_ingress();
				return;
			}
			run_turn([this, &aInvalidation] {
				invalidate_now(aInvalidation);
			});
		}

		void invalidate_now(
			const pull_request_authority_invalidation& aInvalidation)
		{
			if (PendingAuthority == std::optional{ aInvalidation }) {
				++OfferToken;
				if (PendingOfferToken) {
					cancel_offer_job(*PendingOfferToken);
				}
				PendingAuthority.reset();
				PendingOfferToken.reset();
			}
			if (!Active
				|| aInvalidation.ApplicationSession
					!= Active->Handoff.ApplicationSession
				|| aInvalidation.Profile != Active->Authority.Profile
				|| aInvalidation.ProfileRevision
					!= Active->Authority.ProfileRevision
				|| aInvalidation.Host != Active->Authority.Host
				|| aInvalidation.HostAuthority
					!= Active->Authority.HostAuthority
				|| aInvalidation.Account != Active->Authority.Account
				|| aInvalidation.Viewer != Active->Authority.Viewer
				|| aInvalidation.Repository != Active->Authority.Repository
				|| aInvalidation.PullRequest != Active->Authority.PullRequest
				|| aInvalidation.IdentityGeneration
					!= Active->Revision.Source.IdentityGeneration) {
				return;
			}
			cancel_authority_jobs();
			if (Active->Handoff.SmartGit) {
				Active->Handoff.SmartGit->revoke();
			}
			dispatch_core(core::pull_request_revision_invalidated_event{
				.ApplicationSession = aInvalidation.ApplicationSession,
				.Authority          = Active->Authority,
				.Revision           = Active->Revision,
				.Generation         = State.EpochGeneration,
			});
			Active.reset();
		}

		void dispatch(const epoch_intent aIntent)
		{
			if (Closing) {
				return;
			}
			if (TurnActive || DeferredIngressScheduled || !DeferredIngress.empty()) {
				DeferredIngress.emplace_back(deferred_dispatch{ aIntent });
				schedule_deferred_ingress();
				return;
			}
			run_turn([this, &aIntent] {
				dispatch_now(aIntent);
			});
		}

		void dispatch_now(const epoch_intent& aIntent)
		{
			if (Closing) {
				return;
			}
			std::visit(
				[this](const auto& aValue) {
					dispatch_core(core::epoch_event{ aValue });
				},
				aIntent);
		}

		void shutdown()
		{
			if (TurnActive) {
				DeferredIngress.emplace_back(deferred_shutdown{ });
				return;
			}
			run_turn([this] {
				shutdown_now();
			});
		}

		void shutdown_now()
		{
			if (Closing) {
				return;
			}
			Closing = true;
			BlobIngressClosing.store(true, std::memory_order_release);
			++OfferToken;
			cancel_all_jobs();
			PendingAuthority.reset();
			PendingOfferToken.reset();
			if (Active) {
				if (Active->Handoff.SmartGit) {
					Active->Handoff.SmartGit->revoke();
				}
				dispatch_core(core::pull_request_revision_invalidated_event{
					.ApplicationSession = State.ApplicationSession,
					.Authority          = Active->Authority,
					.Revision           = Active->Revision,
					.Generation         = State.EpochGeneration,
				});
				Active.reset();
			}
		}

		void start_catalog_load(const core::load_epoch_catalog_request& aRequest)
		{
			auto context = exact_context(
				aRequest.Context.Authority, aRequest.Context.Revision);
			if (!context) {
				return;
			}
			auto actor = context->Actor;
			submit_effect(
				[context, aRequest](const std::stop_token aStop)
					-> worker_completion {
					const auto snapshot = context->Catalog->snapshot(aStop);
					if (!snapshot) {
						if (snapshot.error() == epoch_catalog_failure::cancelled) {
							return [aRequest](implementation& aHost) {
								aHost.dispatch_core(
									core::epoch_catalog_load_cancelled_event{
										.Context = aRequest.Context,
									});
							};
						}
						const auto failure = catalog_failure(snapshot.error());
						return [aRequest, failure](implementation& aHost) {
							aHost.dispatch_core(core::epoch_catalog_load_failed_event{
								.Context = aRequest.Context,
								.Failure = failure,
							});
						};
					}
					if (
						!application::valid(snapshot->Binding)
						|| snapshot->Binding != context->Catalog->binding()) {
						return [aRequest](implementation& aHost) {
							aHost.dispatch_core(core::epoch_catalog_load_failed_event{
								.Context = aRequest.Context,
								.Failure = core::epoch_failure::repair_required,
							});
						};
					}
					const auto catalog = snapshot->Binding.Catalog;
					return [aRequest, catalog](implementation& aHost) {
						aHost.dispatch_core(core::epoch_catalog_loaded_event{
							.Context = aRequest.Context,
							.Catalog = catalog,
						});
					};
				},
				[aRequest](implementation& aHost) {
					aHost.dispatch_core(core::epoch_catalog_load_failed_event{
						.Context = aRequest.Context,
						.Failure = core::epoch_failure::unavailable,
					});
				},
				aRequest.Context.Request,
				aRequest.Context.Generation,
				std::nullopt,
				std::nullopt,
				std::move(actor));
		}

		void start_revision_preparation(
			const core::prepare_epoch_revision_request& aRequest)
		{
			auto context = exact_context(
				aRequest.Context.Authority, aRequest.Context.Revision);
			if (!context) {
				return;
			}
			auto* const caches  = &Caches;
			auto* const entropy = &Entropy;
			auto* const clock   = &Clock;
			auto        actor   = context->Actor;
			submit_effect(
				[context, aRequest, caches, entropy, clock](
					const std::stop_token aStop) -> worker_completion {
					const auto cancelled = [aRequest]() -> worker_completion {
						return [aRequest](implementation& aHost) {
							aHost.dispatch_core(
								core::epoch_revision_prepare_cancelled_event{
									.Context = aRequest.Context,
								});
						};
					};
					const auto failed = [aRequest](const core::epoch_failure aFailure)
						-> worker_completion {
						return [aRequest, aFailure](implementation& aHost) {
							aHost.dispatch_core(
								core::epoch_revision_prepare_failed_event{
									.Context = aRequest.Context,
									.Failure = aFailure,
								});
						};
					};
					if (aStop.stop_requested()) {
						return cancelled();
					}
					const auto snapshot = context->Catalog->snapshot(aStop);
					if (!snapshot) {
						return snapshot.error() == epoch_catalog_failure::cancelled
							? cancelled()
							: failed(catalog_failure(snapshot.error()));
					}
					if (
						!application::valid(snapshot->Binding)
						|| snapshot->Binding != context->Catalog->binding()) {
						return failed(core::epoch_failure::repair_required);
					}
					const auto expectedCatalog = core::epoch_catalog_identity{
						.Incarnation = aRequest.Context.CatalogIncarnation,
						.Revision    = aRequest.Context.ExpectedCatalogRevision,
					};
					if (snapshot->Binding.Catalog != expectedCatalog) {
						return failed(core::epoch_failure::catalog_conflict);
					}

					const auto effect = core::plan_epoch_preparation(
						context->Authority,
						aRequest.Context.Revision,
						expectedCatalog,
						core::epoch_preparation_catalog_observation{
							.LatestLive           = snapshot->LatestLive,
							.RetentionRehydration = snapshot->RetentionRehydration,
						});
					if (effect.Rejection != core::epoch_reason_code::none) {
						return failed(core::epoch_failure::malformed_observation);
					}

					const auto canonicalReview =
						[&](const std::optional<projected_append_head> aProjected)
						-> std::expected<
							std::optional<core::catalog_review_association>,
							core::epoch_failure> {
						if (!aRequest.Context.Revision.CurrentViewerReview) {
							return std::nullopt;
						}
						for (std::size_t attempt = 0U;
							 attempt < maximum_identity_attempts;
							 ++attempt) {
							const auto association = mint_identity<core::review_association_id>(
								*entropy);
							if (!association) {
								return std::unexpected{
									core::epoch_failure::unavailable
								};
							}
							const auto review = context->Catalog->canonical_review(
								canonical_review_request{
									.Catalog             = expectedCatalog,
									.Candidate           = *aRequest.Context.Revision.CurrentViewerReview,
									.ProposedAssociation = *association,
									.ProjectedHead       = aProjected,
								},
								aStop);
							if (review) {
								return std::optional{ *review };
							}
							if (review.error()
								!= epoch_catalog_failure::
									operation_identity_conflict) {
								return std::unexpected{
									catalog_failure(review.error())
								};
							}
						}
						return std::unexpected{
							core::epoch_failure::catalog_conflict
						};
					};

					const auto observedTime = [&]()
						-> std::optional<core::observed_utc_milliseconds> {
						const auto observed = clock->observe_utc();
						return observed && core::valid(*observed)
							? observed
							: std::nullopt;
					};

					if (effect.Effect == core::epoch_preparation_effect::none) {
						const auto operation = mint_identity<core::epoch_operation_id>(*entropy);
						const auto review    = canonicalReview(std::nullopt);
						const auto observed  = observedTime();
						if (!operation || !review || !observed) {
							return failed(
								review ? core::epoch_failure::unavailable
									   : review.error());
						}
						const auto prepared = prepared_operation{
							.Kind      = prepared_kind::observation_only,
							.Operation = *operation,
						};
						if (!context->store(prepared)) {
							return failed(core::epoch_failure::unavailable);
						}
						const auto event = core::epoch_revision_prepared_event{
							.Context     = aRequest.Context,
							.Operation   = *operation,
							.Observation = {
								.LatestLive                  = snapshot->LatestLive,
								.CatalogCurrentViewerReview  = snapshot->CurrentViewerReview,
								.ProposedCurrentViewerReview = *review,
								.ObservedAt                  = *observed,
								.HeadLineage                 = core::head_lineage::not_applicable,
								.RetentionRehydration        = snapshot->RetentionRehydration,
							},
						};
						return [context, event](implementation& aHost) {
							const auto output   = aHost.dispatch_core(event);
							const auto accepted = std::ranges::any_of(
								output.Requests,
								[](const auto& aNext) {
									return std::holds_alternative<
											   core::commit_epoch_catalog_request>(
											   aNext)
										|| std::holds_alternative<
											   core::discard_epoch_preparation_request>(
											   aNext);
								});
							if (!accepted) {
								aHost.discard_context_preparation(
									context, event.Operation);
							}
						};
					}

					if (!snapshot->NextOrdinal) {
						return failed(core::epoch_failure::ordinal_exhausted);
					}

					auto prepared = prepared_operation{
						.Kind = prepared_kind::append,
					};
					for (std::size_t attempt = 0U;
						 attempt < maximum_identity_attempts;
						 ++attempt) {
						const auto operation = mint_identity<core::epoch_operation_id>(*entropy);
						if (!operation) {
							return failed(core::epoch_failure::unavailable);
						}
						const auto reservation = append_reservation_request{
							.Context = {
								.ApplicationSession = aRequest.Context.ApplicationSession,
								.Request            = aRequest.Context.Request,
								.Generation         = aRequest.Context.Generation,
								.Operation          = *operation,
								.Revision           = aRequest.Context.Revision,
								.Catalog            = expectedCatalog,
							},
							.ProposedOrdinal = *snapshot->NextOrdinal,
						};
						const auto reserved = context->Catalog->reserve_append(
							reservation, aStop);
						if (reserved) {
							if (!exact_append_reservation(
									*reserved,
									reservation,
									context->Authority.Scope)) {
								return failed(
									core::epoch_failure::repair_required);
							}
							prepared.Operation = *operation;
							prepared.Record    = *reserved;
							break;
						}
						if (reserved.error()
							== epoch_catalog_failure::operation_identity_conflict) {
							continue;
						}
						if (reserved.error()
							== epoch_catalog_failure::source_observation_conflict) {
							std::optional<core::epoch_source_observation_id>
								replacement;
							for (std::size_t sourceAttempt = 0U;
								 sourceAttempt < maximum_identity_attempts;
								 ++sourceAttempt) {
								replacement = mint_identity<
									core::epoch_source_observation_id>(*entropy);
								if (!replacement) {
									break;
								}
								const auto exists = context->Catalog->source_observation_exists(*replacement, aStop);
								if (!exists) {
									return failed(catalog_failure(exists.error()));
								}
								if (!*exists) {
									break;
								}
								replacement.reset();
							}
							if (!replacement) {
								return failed(core::epoch_failure::unavailable);
							}
							auto nextRevision               = context->Revision;
							nextRevision.Source.Observation = *replacement;
							return [context,
									prior = aRequest.Context.Revision,
									nextRevision](implementation& aHost) {
								if (aHost.Active != context
									|| aHost.State.AcceptedRevision
										!= std::optional{ prior }) {
									return;
								}
								context->Revision = nextRevision;
								aHost.dispatch_core(
									core::pull_request_revision_accepted_event{
										.ApplicationSession = context->Handoff.ApplicationSession,
										.Authority          = context->Authority,
										.Revision           = nextRevision,
									});
								if (aHost.State.AcceptedRevision
									!= std::optional{ nextRevision }) {
									context->Revision = prior;
								}
							};
						}
						return reserved.error()
								== epoch_catalog_failure::cancelled
							? cancelled()
							: failed(catalog_failure(reserved.error()));
					}
					if (!prepared.Record) {
						return failed(core::epoch_failure::catalog_conflict);
					}

					const auto failPrepared =
						[&](const core::epoch_failure aFailure)
						-> worker_completion {
						const auto safe = abort_preparation(*context, prepared);
						return failed(
							safe ? aFailure : core::epoch_failure::repair_required);
					};
					const auto cancelPrepared = [&]() -> worker_completion {
						const auto safe = abort_preparation(*context, prepared);
						return safe
							? cancelled()
							: failed(core::epoch_failure::repair_required);
					};

					const auto expectedCache = git_cache_open_request{
						.ApplicationSession = aRequest.Context.ApplicationSession,
						.Authority          = context->Authority,
						.ObjectFormat       = context->Handoff.ObjectFormat,
					};
					const auto cache = open_actor_cache(
						*context->Actor, *caches, expectedCache, aStop);
					if (!cache) {
						return cache.error() == git_cache_failure::cancelled
							? cancelPrepared()
							: failPrepared(cache_failure(cache.error()));
					}
					if (!context->Handoff.SmartGit
						|| context->Handoff.SmartGit->revoked()) {
						return failPrepared(core::epoch_failure::unavailable);
					}
					auto sink = context->Actor->Cache->create_pack_sink(
						prepared.Operation,
						context->Actor->Cache->request().Limits.OperationPackBytes,
						aStop);
					if (!sink) {
						return sink.error() == git_cache_failure::cancelled
							? cancelPrepared()
							: failPrepared(cache_failure(sink.error()));
					}
					if (!*sink) {
						return failPrepared(
							core::epoch_failure::repair_required);
					}
					const auto smartContext = git_smart_request_context{
						.ApplicationSession = aRequest.Context.ApplicationSession,
						.Authority          = context->Authority,
						.Revision           = aRequest.Context.Revision,
						.Repository         = context->Handoff.Locator,
						.BaseRef            = context->Handoff.BaseRef,
					};
					auto transport = context->Handoff.SmartGit->acquire(smartContext);
					if (!transport) {
						(*sink)->abort();
						return aStop.stop_requested()
								|| transport.error().Failure
									== git_smart_transport_failure::cancelled
							? cancelPrepared()
							: failPrepared(core::epoch_failure::unavailable);
					}
					if (!*transport) {
						(*sink)->abort();
						return failPrepared(
							core::epoch_failure::repair_required);
					}
					std::vector<git_protocol_v2_have> haves;
					if (snapshot->LatestLive) {
						const auto appendHave = [&](
													const core::git_commit_oid aOid,
													const core::anchor_availability
														aAvailability) {
							if (aAvailability != core::anchor_availability::available
								|| std::ranges::any_of(
									haves,
									[&](const auto& aHave) {
										return aHave.Commit == aOid;
									})) {
								return;
							}
							const auto inspected = context->Actor->Cache->inspect_commit_object(
								aOid, aStop);
							if (
								!inspected || !application::valid(*inspected)
								|| inspected->Commit != aOid) {
								return;
							}
							haves.push_back(git_protocol_v2_have{
								.Cache          = context->Authority.Cache,
								.Commit         = aOid,
								.CatalogVisible = true,
								.ObjectVerified = true,
							});
						};
						appendHave(
							snapshot->LatestLive->Base,
							snapshot->LatestLive->BaseAvailability);
						appendHave(
							snapshot->LatestLive->Head,
							snapshot->LatestLive->HeadAvailability);
					}
					const auto acquisition = acquire_git_protocol_v2_pack(
						**transport,
						smartContext,
						haves,
						**sink,
						git_protocol_v2_fetch_limits{
							.MaximumWireBytes     = maximum_git_smart_fetch_response_bytes,
							.MaximumPackBytes     = context->Actor->Cache->request().Limits.OperationPackBytes,
							.MaximumProgressBytes = maximum_git_protocol_v2_progress_bytes,
							.MaximumPackets       = maximum_git_protocol_v2_fetch_packets,
						},
						aStop);
					if (!acquisition) {
						(*transport)->revoke();
						const auto wasCancelled = aStop.stop_requested()
							|| (std::holds_alternative<git_smart_transport_error>(
									acquisition.error())
								&& std::get<git_smart_transport_error>(
									   acquisition.error())
										.Failure
									== git_smart_transport_failure::cancelled);
						return wasCancelled
							? cancelPrepared()
							: failPrepared(core::epoch_failure::unavailable);
					}
					const auto inventory = context->Actor->Cache->observe_preimport(
						acquisition->Fetch.Pack, aStop);
					if (!inventory) {
						return inventory.error() == git_cache_failure::cancelled
							? cancelPrepared()
							: failPrepared(cache_failure(inventory.error()));
					}
					const auto spoolRequest = record_spool_request{
						.Catalog                   = expectedCatalog,
						.Operation                 = prepared.Operation,
						.ExpectedOperationRevision = prepared.Record->OperationRevision,
						.Spool                     = durable_fact(acquisition->Fetch.Pack),
						.Preimport                 = durable_fact(*inventory),
					};
					auto record = context->Catalog->record_spool(
						spoolRequest, aStop);
					if (!record) {
						return record.error() == epoch_catalog_failure::cancelled
							? cancelPrepared()
							: failPrepared(catalog_failure(record.error()));
					}
					if (!exact_spool_record(
							*prepared.Record, *record, spoolRequest)) {
						return failPrepared(
							core::epoch_failure::repair_required);
					}
					prepared.Record = *record;

					const auto durableRecordToken = entropy->generate();
					if (!durableRecordToken
						|| !application::valid(*durableRecordToken)) {
						return failPrepared(core::epoch_failure::unavailable);
					}
					std::optional<core::git_commit_oid> priorHead;
					if (snapshot->LatestLive) {
						priorHead = snapshot->LatestLive->Head;
					}
					const auto gitRequest = git_prepare_revision_request{
						.Receipt       = acquisition->Fetch.Pack,
						.Inventory     = *inventory,
						.Authorization = {
							.CatalogIncarnation      = expectedCatalog.Incarnation,
							.ExpectedCatalogRevision = expectedCatalog.Revision,
							.Operation               = prepared.Operation,
							.ExpectedPack            = acquisition->Fetch.Pack.ExpectedPack,
							.ReceiptToken            = acquisition->Fetch.Pack.OpaqueToken,
							.InventoryToken          = inventory->OpaqueToken,
							.DurableRecordToken      = *durableRecordToken,
						},
						.Base      = aRequest.Context.Revision.Base,
						.Head      = aRequest.Context.Revision.Head,
						.PriorHead = priorHead,
					};
					auto git = context->Actor->Cache->prepare_revision(
						gitRequest, aStop);
					if (!git) {
						return git.error() == git_cache_failure::cancelled
							? cancelPrepared()
							: failPrepared(cache_failure(git.error()));
					}
					if (!exact_prepared_revision(*git, gitRequest)) {
						return failPrepared(
							core::epoch_failure::repair_required);
					}
					if (
						snapshot->LatestLive
						&& snapshot->LatestLive->Head
							!= aRequest.Context.Revision.Head
						&& snapshot->LatestLive->HeadAvailability
							!= core::anchor_availability::available) {
						git->HeadLineage = core::head_lineage::unknown;
					}
					prepared.Git              = *git;
					const auto installRequest = record_install_request{
						.Catalog                   = expectedCatalog,
						.Operation                 = prepared.Operation,
						.ExpectedOperationRevision = prepared.Record->OperationRevision,
						.Install                   = durable_fact(*git),
					};
					record = context->Catalog->record_install(
						installRequest, aStop);
					if (!record) {
						return record.error() == epoch_catalog_failure::cancelled
							? cancelPrepared()
							: failPrepared(catalog_failure(record.error()));
					}
					if (!exact_install_record(
							*prepared.Record, *record, installRequest)) {
						return failPrepared(
							core::epoch_failure::repair_required);
					}
					prepared.Record    = *record;
					const auto staging = advance_append(
						*context,
						*prepared.Record,
						expectedCatalog,
						epoch_catalog_operation_phase::staging_ready,
						aStop);
					if (!staging) {
						return staging.error()
								== epoch_catalog_failure::cancelled
							? cancelPrepared()
							: failPrepared(catalog_failure(staging.error()));
					}
					prepared.Record = *staging;

					const auto review   = canonicalReview(projected_append_head{
						.Ordinal = prepared.Record->ReservedOrdinal,
						.Head    = aRequest.Context.Revision.Head,
					});
					const auto observed = observedTime();
					if (!review || !observed) {
						return failPrepared(
							review ? core::epoch_failure::unavailable
								   : review.error());
					}
					if (!context->store(prepared)) {
						const auto safe = abort_preparation(*context, prepared);
						return failed(
							safe ? core::epoch_failure::unavailable
								 : core::epoch_failure::repair_required);
					}
					const auto event = core::epoch_revision_prepared_event{
						.Context     = aRequest.Context,
						.Operation   = prepared.Operation,
						.Observation = {
							.LatestLive                  = snapshot->LatestLive,
							.CatalogCurrentViewerReview  = snapshot->CurrentViewerReview,
							.ProposedCurrentViewerReview = *review,
							.ProposedOrdinal             = prepared.Record->ReservedOrdinal,
							.ObservedAt                  = *observed,
							.HeadLineage                 = git->HeadLineage,
							.Material                    = core::epoch_material_identity{
								.BaseTree       = git->BaseTree,
								.HeadTree       = git->HeadTree,
								.PolicyRevision = core::comparison_policy_revision{ 1U },
							},
							.RetentionRehydration = snapshot->RetentionRehydration,
						},
					};
					return [context, event](implementation& aHost) {
						const auto output   = aHost.dispatch_core(event);
						const auto accepted = std::ranges::any_of(
							output.Requests,
							[](const auto& aNext) {
								return std::holds_alternative<
										   core::commit_epoch_catalog_request>(aNext)
									|| std::holds_alternative<
										   core::discard_epoch_preparation_request>(
										   aNext);
							});
						if (!accepted) {
							aHost.discard_context_preparation(
								context, event.Operation);
						}
					};
				},
				[aRequest](implementation& aHost) {
					aHost.dispatch_core(
						core::epoch_revision_prepare_failed_event{
							.Context = aRequest.Context,
							.Failure = core::epoch_failure::unavailable,
						});
				},
				aRequest.Context.Request,
				aRequest.Context.Generation,
				std::nullopt,
				std::nullopt,
				std::move(actor));
		}

		void start_catalog_commit(const core::commit_epoch_catalog_request& aRequest)
		{
			auto context = exact_context(
				aRequest.Context.Authority, aRequest.Context.Revision);
			if (!context) {
				return;
			}
			auto* const entropy = &Entropy;
			auto* const caches  = &Caches;
			auto        actor   = context->Actor;
			submit_effect(
				[context, aRequest, entropy, caches](const std::stop_token aStop)
					-> worker_completion {
					const auto cancelled = [aRequest](const core::epoch_commit_progress
														  aProgress)
						-> worker_completion {
						return [aRequest, aProgress](implementation& aHost) {
							aHost.dispatch_core(
								core::epoch_catalog_commit_cancelled_event{
									.Context  = aRequest.Context,
									.Mutation = aRequest.Mutation,
									.Progress = aProgress,
								});
						};
					};
					const auto failed = [aRequest](const core::epoch_failure aFailure)
						-> worker_completion {
						return [aRequest, aFailure](implementation& aHost) {
							aHost.dispatch_core(
								core::epoch_catalog_commit_failed_event{
									.Context  = aRequest.Context,
									.Mutation = aRequest.Mutation,
									.Failure  = aFailure,
								});
						};
					};
					const auto succeeded = [aRequest](
											   const core::epoch_catalog_revision
												   aRevision)
						-> worker_completion {
						return [aRequest, aRevision](implementation& aHost) {
							aHost.dispatch_core(core::epoch_catalog_committed_event{
								.Context     = aRequest.Context,
								.Mutation    = aRequest.Mutation,
								.NewRevision = aRevision,
							});
						};
					};

					auto claimed = context->take(aRequest.Context.Operation);
					if (!claimed) {
						return failed(core::epoch_failure::catalog_conflict);
					}
					auto       operation       = std::move(*claimed);
					const auto expectedCatalog = core::epoch_catalog_identity{
						.Incarnation = aRequest.Context.CatalogIncarnation,
						.Revision    = aRequest.Context.ExpectedCatalogRevision,
					};
					const auto abortAndFail =
						[&](const core::epoch_failure aFailure)
						-> worker_completion {
						const auto safe = abort_preparation(*context, operation);
						return failed(
							safe ? aFailure : core::epoch_failure::repair_required);
					};
					const auto abortAndCancel = [&]() -> worker_completion {
						const auto safe = abort_preparation(*context, operation);
						return safe
							? cancelled(
								  core::epoch_commit_progress::before_ref_commit)
							: failed(core::epoch_failure::repair_required);
					};

					if (const auto* association = std::get_if<core::associate_review_mutation>(
							&aRequest.Mutation)) {
						if (operation.Kind != prepared_kind::observation_only) {
							return abortAndFail(
								core::epoch_failure::malformed_observation);
						}
						if (aStop.stop_requested()) {
							return cancelled(
								core::epoch_commit_progress::before_ref_commit);
						}
						const auto visibleAssociation = [&]()
							-> std::optional<core::epoch_catalog_revision> {
							try {
								const auto snapshot = context->Catalog->snapshot({ });
								if (
									!snapshot
									|| !application::valid(snapshot->Binding)
									|| snapshot->Binding
										!= context->Catalog->binding()
									|| snapshot->Binding.Catalog.Incarnation
										!= expectedCatalog.Incarnation
									|| expectedCatalog.Revision.value()
										>= core::maximum_epoch_catalog_revision
									|| snapshot->Binding.Catalog.Revision.value()
										!= expectedCatalog.Revision.value() + 1
									|| snapshot->CurrentViewerReview
										!= std::optional{ association->Review }) {
									return std::nullopt;
								}
								return snapshot->Binding.Catalog.Revision;
							} catch (...) {
								return std::nullopt;
							}
						};
						epoch_catalog_result<catalog_commit_result> committed = std::unexpected{ epoch_catalog_failure::unavailable };
						try {
							committed = context->Catalog->associate_review(
								associate_review_request{
									.Catalog          = expectedCatalog,
									.Review           = association->Review,
									.AcceptedSnapshot = aRequest.Context.Revision.Source.Snapshot,
								},
								aStop);
						} catch (...) {
							const auto visible = visibleAssociation();
							return visible ? succeeded(*visible)
										   : failed(
												 core::epoch_failure::
													 repair_required);
						}
						if (!committed) {
							if (const auto visible = visibleAssociation()) {
								return succeeded(*visible);
							}
							return committed.error()
									== epoch_catalog_failure::cancelled
								? cancelled(core::epoch_commit_progress::
												before_ref_commit)
								: failed(catalog_failure(committed.error()));
						}
						if (
							!exact_catalog_commit(
								*committed, expectedCatalog, std::nullopt)
							|| context->Catalog->binding().Catalog
								!= committed->Catalog) {
							const auto visible = visibleAssociation();
							return visible ? succeeded(*visible)
										   : failed(
												 core::epoch_failure::
													 catalog_conflict);
						}
						return succeeded(committed->Catalog.Revision);
					}

					const auto* append = std::get_if<core::append_epoch_mutation>(&aRequest.Mutation);
					if (append == nullptr || operation.Kind != prepared_kind::append
						|| !operation.Record || !operation.Git
						|| operation.Record->Context.Catalog != expectedCatalog
						|| operation.Record->ReservedOrdinal
							!= append->Epoch.Id.Ordinal
						|| operation.Git->Base != append->Epoch.Base
						|| operation.Git->Head != append->Epoch.Head) {
						return abortAndFail(
							core::epoch_failure::malformed_observation);
					}
					const auto expectedCache = git_cache_open_request{
						.ApplicationSession = aRequest.Context.ApplicationSession,
						.Authority          = context->Authority,
						.ObjectFormat       = context->Handoff.ObjectFormat,
					};
					const auto cache = open_actor_cache(
						*context->Actor, *caches, expectedCache, aStop);
					if (!cache) {
						return abortAndFail(cache_failure(cache.error()));
					}
					if (aStop.stop_requested()) {
						return abortAndCancel();
					}
					const auto durableCommitToken = entropy->generate();
					if (!durableCommitToken
						|| !application::valid(*durableCommitToken)) {
						return abortAndFail(core::epoch_failure::unavailable);
					}

					const auto visibleAppend = [&]()
						-> std::optional<core::epoch_catalog_revision> {
						try {
							const auto snapshot = context->Catalog->snapshot({ });
							if (
								!snapshot
								|| !application::valid(snapshot->Binding)
								|| snapshot->Binding
									!= context->Catalog->binding()
								|| snapshot->Binding.Catalog.Incarnation
									!= expectedCatalog.Incarnation
								|| expectedCatalog.Revision.value()
									>= core::maximum_epoch_catalog_revision
								|| snapshot->Binding.Catalog.Revision.value()
									!= expectedCatalog.Revision.value() + 1
								|| snapshot->LatestLive
									!= std::optional{ append->Epoch }) {
								return std::nullopt;
							}
							return snapshot->Binding.Catalog.Revision;
						} catch (...) {
							return std::nullopt;
						}
					};
					const auto resolveAmbiguity = [&, context](
													  const core::epoch_failure aFallback,
													  const bool                aCancelledBeforePonr) -> worker_completion {
						if (const auto visible = visibleAppend()) {
							return succeeded(*visible);
						}
						try {
							const auto pending = pending_append(
								*context->Catalog, operation.Operation);
							if (!pending) {
								return failed(
									core::epoch_failure::repair_required);
							}
							if (!*pending) {
								if (const auto visible = visibleAppend()) {
									return succeeded(*visible);
								}
								return aCancelledBeforePonr
									? cancelled(core::epoch_commit_progress::
													before_ref_commit)
									: failed(aFallback);
							}
							const auto& durable = **pending;
							if (
								!same_append_identity(
									*operation.Record, durable)
								|| durable.Spool != operation.Record->Spool
								|| durable.Preimport
									!= operation.Record->Preimport
								|| durable.Install != operation.Record->Install
								|| (durable.Candidate
									&& durable.Candidate
										!= std::optional{ *append })) {
								return failed(
									core::epoch_failure::repair_required);
							}
							operation.Record = durable;
							if (!point_of_no_cancellation(durable.Phase)) {
								const auto safe = abort_preparation(*context, operation);
								if (!safe) {
									return failed(
										core::epoch_failure::repair_required);
								}
								return aCancelledBeforePonr
									? cancelled(core::epoch_commit_progress::
													before_ref_commit)
									: failed(aFallback);
							}

							const auto recovered = recover_append(
								*context->Catalog,
								**cache,
								*entropy,
								durable,
								{ });
							if (const auto visible = visibleAppend()) {
								return succeeded(*visible);
							}
							return recovered
								? failed(core::epoch_failure::repair_required)
								: failed(recovered.error());
						} catch (...) {
							if (const auto visible = visibleAppend()) {
								return succeeded(*visible);
							}
							return failed(
								core::epoch_failure::repair_required);
						}
					};

					const auto candidateRequest = record_candidate_request{
						.Catalog                   = expectedCatalog,
						.Operation                 = operation.Operation,
						.ExpectedOperationRevision = operation.Record->OperationRevision,
						.Candidate                 = *append,
					};
					epoch_catalog_result<append_operation_record> record = std::unexpected{ epoch_catalog_failure::unavailable };
					try {
						record = context->Catalog->record_candidate(
							candidateRequest, aStop);
					} catch (...) {
						return resolveAmbiguity(
							core::epoch_failure::repair_required, false);
					}
					if (!record) {
						const auto wasCancelled = record.error() == epoch_catalog_failure::cancelled;
						return resolveAmbiguity(
							wasCancelled
								? core::epoch_failure::unavailable
								: catalog_failure(record.error()),
							wasCancelled);
					}
					if (!exact_candidate_record(
							*operation.Record, *record, candidateRequest)) {
						return resolveAmbiguity(
							core::epoch_failure::repair_required, false);
					}
					operation.Record = *record;
					if (aStop.stop_requested()) {
						return abortAndCancel();
					}
					epoch_catalog_result<append_operation_record>
						pointOfNoReturn = std::unexpected{
							epoch_catalog_failure::unavailable
						};
					try {
						pointOfNoReturn = advance_append(
							*context,
							*operation.Record,
							expectedCatalog,
							epoch_catalog_operation_phase::
								ref_commit_in_progress,
							aStop);
					} catch (...) {
						return resolveAmbiguity(
							core::epoch_failure::repair_required, false);
					}
					if (!pointOfNoReturn) {
						const auto wasCancelled = pointOfNoReturn.error()
							== epoch_catalog_failure::cancelled;
						return resolveAmbiguity(
							wasCancelled
								? core::epoch_failure::unavailable
								: catalog_failure(
									  pointOfNoReturn.error()),
							wasCancelled);
					}
					operation.Record = *pointOfNoReturn;

					const auto resolveRecovery = [&]() -> worker_completion {
						return resolveAmbiguity(
							core::epoch_failure::repair_required, false);
					};

					try {
						auto retained = context->Actor->Cache->promote_retained(
							*operation.Git,
							git_ref_promotion_authorization{
								.CatalogIncarnation      = expectedCatalog.Incarnation,
								.ExpectedCatalogRevision = expectedCatalog.Revision,
								.Operation               = operation.Operation,
								.Scope                   = context->Authority.Scope,
								.Ordinal                 = append->Epoch.Id.Ordinal,
								.Base                    = append->Epoch.Base,
								.Head                    = append->Epoch.Head,
								.DurableCommitToken      = *durableCommitToken,
							},
							{ });
						if (!retained) {
							return resolveRecovery();
						}
						if (*retained != expected_retained_pair(*operation.Record)) {
							return resolveRecovery();
						}
						record = advance_append(
							*context,
							*operation.Record,
							expectedCatalog,
							epoch_catalog_operation_phase::refs_committed,
							{ });
						if (!record) {
							return resolveRecovery();
						}
						operation.Record     = *record;
						const auto inspected = context->Actor->Cache->inspect_retained(
							append->Epoch.Id.Ordinal, { });
						if (!inspected) {
							return resolveRecovery();
						}
						if (
							*inspected
							!= expected_retained_pair(*operation.Record)) {
							return resolveRecovery();
						}
						const auto finalized = context->Catalog->finalize_append(
							finalize_append_request{
								.Catalog                   = expectedCatalog,
								.Operation                 = operation.Operation,
								.ExpectedOperationRevision = operation.Record->OperationRevision,
								.Mutation                  = *append,
								.ObservedRefs              = *inspected,
							},
							{ });
						if (!finalized) {
							return resolveRecovery();
						}
						if (
							!exact_catalog_commit(
								*finalized,
								expectedCatalog,
								append->Epoch.Id)
							|| context->Catalog->binding().Catalog
								!= finalized->Catalog) {
							return resolveRecovery();
						}

						// Catalog visibility is the durable-success linearization
						// point. Cleanup failure remains recoverable and must not
						// replace this committed terminal with failure/cancellation.
						auto cleanupRecord              = *operation.Record;
						cleanupRecord.Phase             = epoch_catalog_operation_phase::visible_cleanup_pending;
						cleanupRecord.OperationRevision = epoch_catalog_operation_revision{
							cleanupRecord.OperationRevision.value() + 1
						};
						const auto cleanupRequest = recovery_request(cleanupRecord);
						if (!cleanupRequest) {
							mark_append_repair(*context->Catalog, cleanupRecord);
						} else {
							const auto cleaned = context->Actor->Cache->complete_recovered_operation(
								*cleanupRequest, { });
							if (cleaned) {
								static_cast<void>(advance_append(
									*context,
									cleanupRecord,
									finalized->Catalog,
									epoch_catalog_operation_phase::complete,
									{ }));
							} else if (
								cache_failure(cleaned.error())
								== core::epoch_failure::repair_required) {
								mark_append_repair(
									*context->Catalog, cleanupRecord);
							}
						}
						return succeeded(finalized->Catalog.Revision);
					} catch (...) {
						return resolveRecovery();
					}
				},
				[aRequest](implementation& aHost) {
					aHost.dispatch_core(core::epoch_catalog_commit_failed_event{
						.Context  = aRequest.Context,
						.Mutation = aRequest.Mutation,
						.Failure  = core::epoch_failure::repair_required,
					});
				},
				aRequest.Context.Request,
				aRequest.Context.Generation,
				std::nullopt,
				std::nullopt,
				std::move(actor));
		}

		void discard_preparation(
			const core::discard_epoch_preparation_request& aRequest)
		{
			auto context = exact_context(
				aRequest.Context.Authority, aRequest.Context.Revision);
			if (!context) {
				return;
			}
			discard_context_preparation(
				std::move(context), aRequest.Context.Operation);
		}

		void start_epoch_maintenance(
			const core::perform_epoch_maintenance_request& aRequest)
		{
			auto context = Active;
			if (!context || context->Authority != aRequest.Context.Authority
				|| context->Revision.Source != aRequest.Context.Source) {
				return;
			}
			auto* const caches  = &Caches;
			auto* const entropy = &Entropy;
			auto        actor   = context->Actor;
			submit_effect(
				[context, aRequest, caches, entropy](
					const std::stop_token aStop) -> worker_completion {
					const auto completed =
						[aRequest](const core::epoch_catalog_identity aCatalog)
						-> worker_completion {
						return [aRequest, aCatalog](implementation& aHost) {
							aHost.dispatch_core(
								core::epoch_maintenance_completed_event{
									.Context    = aRequest.Context,
									.NewCatalog = aCatalog,
								});
						};
					};
					const auto failed =
						[aRequest](
							const core::epoch_failure aFailure) -> worker_completion {
						return [aRequest, aFailure](implementation& aHost) {
							aHost.dispatch_core(core::epoch_maintenance_failed_event{
								.Context = aRequest.Context,
								.Failure = aFailure,
							});
						};
					};
					const auto cancelled = [aRequest]() -> worker_completion {
						return [aRequest](implementation& aHost) {
							aHost.dispatch_core(
								core::epoch_maintenance_cancelled_event{
									.Context = aRequest.Context,
								});
						};
					};
					const auto& binding = context->Catalog->binding();
					if (!application::valid(binding)
						|| binding.Authority != aRequest.Context.Authority
						|| binding.Catalog != aRequest.Context.Catalog
						|| aRequest.Context.Cache != binding.Authority.Cache
						|| aRequest.Context.ApplicationSession
							!= context->Handoff.ApplicationSession
						|| aRequest.Context.Generation.value() == 0U
						|| aRequest.Context.Request.value() == 0U
						|| !aRequest.Context.TargetDigest.valid()) {
						return failed(core::epoch_failure::catalog_conflict);
					}

					const auto snapshot = context->Catalog->snapshot(aStop);
					if (!snapshot) {
						return snapshot.error() == epoch_catalog_failure::cancelled
							? cancelled()
							: failed(catalog_failure(snapshot.error()));
					}
					if (!application::valid(snapshot->Binding)
						|| snapshot->Binding != binding) {
						return failed(core::epoch_failure::repair_required);
					}

					std::optional<maintenance_operation_record> operation;
					for (std::size_t attempt = 0U;
						 attempt < maximum_identity_attempts;
						 ++attempt) {
						const auto identity = mint_identity<core::epoch_operation_id>(*entropy);
						if (!identity) {
							return failed(core::epoch_failure::unavailable);
						}

						if (aRequest.Context.Kind
							== core::epoch_maintenance_kind::prune_epochs) {
							if (aRequest.Context.Epochs.empty()
								|| aRequest.Context.Epochs.size()
									> maximum_prune_epoch_count) {
								return failed(
									core::epoch_failure::malformed_observation);
							}
							std::vector<core::comparison_anchor_identity>
								identities;
							identities.reserve(aRequest.Context.Epochs.size() * 2U);
							for (std::size_t index = 0U;
								 index < aRequest.Context.Epochs.size();
								 ++index) {
								const auto& epoch = aRequest.Context.Epochs[index];
								if (!core::valid(epoch)
									|| epoch.Authority != binding.Authority.Scope
									|| epoch.Incarnation
										!= binding.Catalog.Incarnation
									|| (index != 0U
										&& !(aRequest.Context.Epochs[index - 1U]
											 < epoch))) {
									return failed(
										core::epoch_failure::malformed_observation);
								}
								identities.push_back(
									core::comparison_anchor_identity{
										.Authority   = epoch.Authority,
										.Incarnation = epoch.Incarnation,
										.Kind        = core::comparison_anchor_kind::
											epoch_base,
										.Epoch = epoch.Ordinal,
									});
								identities.push_back(
									core::comparison_anchor_identity{
										.Authority   = epoch.Authority,
										.Incarnation = epoch.Incarnation,
										.Kind        = core::comparison_anchor_kind::
											epoch_head,
										.Epoch = epoch.Ordinal,
									});
							}
							std::ranges::sort(identities);
							const auto anchors = context->Catalog->exact_anchors(
								exact_anchor_lookup_request{
									.Catalog = aRequest.Context.Catalog,
									.Anchors = identities,
								},
								aStop);
							if (!anchors) {
								return anchors.error()
										== epoch_catalog_failure::cancelled
									? cancelled()
									: failed(catalog_failure(anchors.error()));
							}
							if (anchors->Catalog != aRequest.Context.Catalog
								|| anchors->Anchors.size() != identities.size()) {
								return failed(core::epoch_failure::repair_required);
							}
							std::vector<prune_epoch_fact> facts;
							facts.reserve(aRequest.Context.Epochs.size());
							for (const auto& epoch : aRequest.Context.Epochs) {
								const auto find = [&](const auto aKind)
									-> const core::comparison_anchor* {
									const auto identity = core::comparison_anchor_identity{
										.Authority   = epoch.Authority,
										.Incarnation = epoch.Incarnation,
										.Kind        = aKind,
										.Epoch       = epoch.Ordinal,
									};
									const auto found = std::ranges::find(anchors->Anchors,
																		 identity,
																		 &core::comparison_anchor::Identity);
									return found == anchors->Anchors.end()
										? nullptr
										: &*found;
								};
								const auto* base = find(core::comparison_anchor_kind::epoch_base);
								const auto* head = find(core::comparison_anchor_kind::epoch_head);
								if (base == nullptr || head == nullptr
									|| !core::valid(*base) || !core::valid(*head)
									|| !valid_prune_anchor_availability(
										base->Availability)
									|| !valid_prune_anchor_availability(
										head->Availability)
									|| base->Commit.algorithm()
										!= context->Handoff.ObjectFormat
									|| head->Commit.algorithm()
										!= context->Handoff.ObjectFormat) {
									return failed(
										core::epoch_failure::repair_required);
								}
								facts.push_back(prune_epoch_fact{
									.Epoch    = epoch,
									.Retained = git_retained_ref_pair{
										.Scope   = epoch.Authority,
										.Ordinal = epoch.Ordinal,
										.Base    = base->Commit,
										.Head    = head->Commit,
									},
									.BaseRef = epoch_catalog_ref_identity{
										.Authority = epoch.Authority,
										.Ordinal   = epoch.Ordinal,
									},
									.HeadRef = epoch_catalog_ref_identity{
										.Authority = epoch.Authority,
										.Ordinal   = epoch.Ordinal,
										.Head      = true,
									},
									.ExpectedBaseAvailability = base->Availability,
									.ExpectedHeadAvailability = head->Availability,
								});
							}
							const auto begin = begin_prune_request{
								.Catalog   = aRequest.Context.Catalog,
								.Operation = *identity,
								.Epochs    = std::move(facts),
							};
							auto begun = context->Catalog->begin_prune(begin, aStop);
							if (begun) {
								if (!exact_prune_intent(*begun,
														begin,
														binding.Authority.Cache)) {
									return failed(
										core::epoch_failure::repair_required);
								}
								operation = std::move(*begun);
								break;
							}
							if (begun.error() == epoch_catalog_failure::cancelled) {
								return cancelled();
							}
							if (begun.error()
									!= epoch_catalog_failure::operation_conflict
								&& begun.error()
									!= epoch_catalog_failure::
										operation_identity_conflict) {
								return failed(catalog_failure(begun.error()));
							}
						} else if (aRequest.Context.Kind
								   == core::epoch_maintenance_kind::
									   delete_repository_cache) {
							if (!aRequest.Context.Epochs.empty()) {
								return failed(
									core::epoch_failure::malformed_observation);
							}
							const auto impact = context->Catalog->cache_delete_impact(
								aRequest.Context.Cache,
								aStop);
							if (!impact) {
								return impact.error()
										== epoch_catalog_failure::cancelled
									? cancelled()
									: failed(catalog_failure(impact.error()));
							}
							const auto currentScope = std::ranges::find(impact->Scopes,
																		binding.Authority.Scope,
																		&cache_delete_scope_fact::Authority);
							if (!application::valid(*impact)
								|| impact->Incarnation
									!= binding.Catalog.Incarnation
								|| impact->Cache != binding.Authority.Cache
								|| currentScope == impact->Scopes.end()
								|| currentScope->ExpectedRevision
									!= binding.Catalog.Revision) {
								return failed(core::epoch_failure::repair_required);
							}
							const auto begin = begin_cache_delete_request{
								.Operation = *identity,
								.Impact    = *impact,
							};
							auto begun = context->Catalog->begin_cache_delete(begin, aStop);
							if (begun) {
								if (!exact_cache_delete_intent(*begun, begin)) {
									return failed(
										core::epoch_failure::repair_required);
								}
								operation = std::move(*begun);
								break;
							}
							if (begun.error() == epoch_catalog_failure::cancelled) {
								return cancelled();
							}
							if (begun.error()
									!= epoch_catalog_failure::operation_conflict
								&& begun.error()
									!= epoch_catalog_failure::
										operation_identity_conflict) {
								return failed(catalog_failure(begun.error()));
							}
						} else {
							return failed(
								core::epoch_failure::malformed_observation);
						}
					}
					if (!operation) {
						return failed(core::epoch_failure::catalog_conflict);
					}

					if (aStop.stop_requested()) {
						const auto aborted = recover_maintenance(*context->Catalog,
																 *caches,
																 context->Handoff,
																 context->Actor->Cache,
																 *operation,
																 { });
						return aborted ? cancelled() : failed(aborted.error());
					}
					const auto next       = operation->Kind == epoch_catalog_operation_kind::prune
						? epoch_catalog_operation_phase::ref_delete_in_progress
						: epoch_catalog_operation_phase::
							  quarantine_rename_in_progress;
					auto       authorized = advance_maintenance(*context->Catalog,
																*operation,
																aRequest.Context.Catalog,
																next,
																aStop);
					if (!authorized) {
						if (authorized.error() == epoch_catalog_failure::cancelled) {
							const auto aborted = recover_maintenance(*context->Catalog,
																	 *caches,
																	 context->Handoff,
																	 context->Actor->Cache,
																	 *operation,
																	 { });
							return aborted ? cancelled() : failed(aborted.error());
						}
						return failed(catalog_failure(authorized.error()));
					}
					const auto recovered = recover_maintenance(*context->Catalog,
															   *caches,
															   context->Handoff,
															   context->Actor->Cache,
															   *authorized,
															   { });
					return recovered ? completed(*recovered)
									 : failed(recovered.error());
				},
				[aRequest](implementation& aHost) {
					aHost.dispatch_core(core::epoch_maintenance_failed_event{
						.Context = aRequest.Context,
						.Failure = core::epoch_failure::unavailable,
					});
				},
				aRequest.Context.Request,
				aRequest.Context.Generation,
				std::nullopt,
				std::nullopt,
				std::move(actor));
		}

		void start_history_page(
			const core::load_epoch_history_page_request& aRequest)
		{
			auto context = Active;
			if (!context || context->Authority != aRequest.Context.Authority
				|| context->Revision.Source != aRequest.Context.Source) {
				return;
			}
			auto actor = context->Actor;
			submit_effect(
				[context, aRequest](const std::stop_token aStop)
					-> worker_completion {
					if (
						!application::valid(context->Catalog->binding())
						|| context->Catalog->binding().Authority
							!= aRequest.Context.Authority
						|| context->Catalog->binding().Catalog
							!= aRequest.Context.Catalog) {
						return [aRequest](implementation& aHost) {
							aHost.dispatch_core(
								core::epoch_history_page_load_failed_event{
									.Context = aRequest.Context,
									.Failure = core::epoch_failure::catalog_conflict,
								});
						};
					}
					auto page = context->Catalog->history(
						epoch_history_page_request{
							.Catalog = aRequest.Context.Catalog,
							.Cursor  = aRequest.Context.Cursor,
							.Limit   = aRequest.Context.Limit,
						},
						aStop);
					if (!page) {
						if (page.error() == epoch_catalog_failure::cancelled) {
							return [aRequest](implementation& aHost) {
								aHost.dispatch_core(
									core::epoch_history_page_load_cancelled_event{
										.Context = aRequest.Context,
									});
							};
						}
						const auto failure = catalog_failure(page.error());
						return [aRequest, failure](implementation& aHost) {
							aHost.dispatch_core(
								core::epoch_history_page_load_failed_event{
									.Context = aRequest.Context,
									.Failure = failure,
								});
						};
					}
					auto observation = core::epoch_history_page_observation{
						.Authority    = page->Authority,
						.Catalog      = page->Catalog,
						.Epochs       = std::move(page->Epochs),
						.Continuation = page->Continuation,
					};
					return [aRequest, observation = std::move(observation)](
							   implementation& aHost) {
						aHost.dispatch_core(core::epoch_history_page_loaded_event{
							.Context = aRequest.Context,
							.Page    = observation,
						});
					};
				},
				[aRequest](implementation& aHost) {
					aHost.dispatch_core(
						core::epoch_history_page_load_failed_event{
							.Context = aRequest.Context,
							.Failure = core::epoch_failure::unavailable,
						});
				},
				aRequest.Context.Request,
				aRequest.Context.Generation,
				std::nullopt,
				std::nullopt,
				std::move(actor));
		}

		void start_comparison_resolution(
			const core::resolve_comparison_request& aRequest)
		{
			auto context = Active;
			if (!context || context->Authority != aRequest.Context.Authority
				|| context->Revision.Source != aRequest.Context.Source) {
				return;
			}
			auto* const caches = &Caches;
			auto        actor  = context->Actor;
			submit_effect(
				[context, aRequest, caches](const std::stop_token aStop)
					-> worker_completion {
					const auto snapshot = context->Catalog->snapshot(aStop);
					if (!snapshot) {
						if (snapshot.error() == epoch_catalog_failure::cancelled) {
							return [aRequest](implementation& aHost) {
								aHost.dispatch_core(
									core::comparison_resolve_cancelled_event{
										.Context = aRequest.Context,
									});
							};
						}
						const auto failure = catalog_failure(snapshot.error());
						return [aRequest, failure](implementation& aHost) {
							aHost.dispatch_core(core::comparison_resolve_failed_event{
								.Context = aRequest.Context,
								.Failure = failure,
							});
						};
					}
					if (
						!application::valid(snapshot->Binding)
						|| snapshot->Binding != context->Catalog->binding()) {
						return [aRequest](implementation& aHost) {
							aHost.dispatch_core(
								core::comparison_resolve_failed_event{
									.Context = aRequest.Context,
									.Failure = core::epoch_failure::repair_required,
								});
						};
					}
					const auto expectedCatalog = core::epoch_catalog_identity{
						.Incarnation = aRequest.Context.CatalogIncarnation,
						.Revision    = aRequest.Context.CatalogRevision,
					};
					if (snapshot->Binding.Catalog != expectedCatalog) {
						return [aRequest](implementation& aHost) {
							aHost.dispatch_core(core::comparison_resolve_failed_event{
								.Context = aRequest.Context,
								.Failure = core::epoch_failure::catalog_conflict,
							});
						};
					}

					core::comparison_candidates candidates{
						.Catalog              = snapshot->Binding.Catalog,
						.LatestLive           = snapshot->LatestLive,
						.PreviousDistinctHead = snapshot->PreviousDistinctHead,
					};
					if (snapshot->CurrentViewerReview) {
						candidates.CurrentViewerReview = review_anchor(
							context->Authority,
							snapshot->Binding.Catalog,
							*snapshot->CurrentViewerReview);
					}
					if (const auto* explicitPair = std::get_if<core::explicit_comparison_selection>(
							&aRequest.Context.Selection)) {
						std::vector<core::comparison_anchor_identity> identities{
							explicitPair->Left,
							explicitPair->Right,
						};
						std::ranges::sort(identities);
						identities.erase(
							std::unique(identities.begin(), identities.end()),
							identities.end());
						const auto exact = context->Catalog->exact_anchors(
							exact_anchor_lookup_request{
								.Catalog = snapshot->Binding.Catalog,
								.Anchors = identities,
							},
							aStop);
						if (!exact) {
							if (exact.error() == epoch_catalog_failure::cancelled) {
								return [aRequest](implementation& aHost) {
									aHost.dispatch_core(
										core::comparison_resolve_cancelled_event{
											.Context = aRequest.Context,
										});
								};
							}
							const auto failure = catalog_failure(exact.error());
							return [aRequest, failure](implementation& aHost) {
								aHost.dispatch_core(
									core::comparison_resolve_failed_event{
										.Context = aRequest.Context,
										.Failure = failure,
									});
							};
						}
						std::vector<core::comparison_anchor_identity>
							returnedIdentities;
						returnedIdentities.reserve(exact->Anchors.size());
						for (const auto& anchor : exact->Anchors) {
							if (
								!core::valid(anchor)
								|| !std::ranges::binary_search(
									identities, anchor.Identity)) {
								return [aRequest](implementation& aHost) {
									aHost.dispatch_core(
										core::comparison_resolve_failed_event{
											.Context = aRequest.Context,
											.Failure = core::epoch_failure::
												repair_required,
										});
								};
							}
							returnedIdentities.push_back(anchor.Identity);
						}
						std::ranges::sort(returnedIdentities);
						if (
							exact->Catalog != snapshot->Binding.Catalog
							|| exact->Anchors.size() > identities.size()
							|| std::ranges::adjacent_find(returnedIdentities)
								!= returnedIdentities.end()) {
							return [aRequest](implementation& aHost) {
								aHost.dispatch_core(
									core::comparison_resolve_failed_event{
										.Context = aRequest.Context,
										.Failure = core::epoch_failure::
											repair_required,
									});
							};
						}
						const auto findAnchor = [&exact](const auto& aIdentity)
							-> std::optional<core::comparison_anchor> {
							const auto found = std::ranges::find(
								exact->Anchors,
								aIdentity,
								&core::comparison_anchor::Identity);
							return found == exact->Anchors.end()
								? std::nullopt
								: std::optional{ *found };
						};
						candidates.ExplicitLeft  = findAnchor(explicitPair->Left);
						candidates.ExplicitRight = findAnchor(explicitPair->Right);
					}

					const auto expectedCache = git_cache_open_request{
						.ApplicationSession = aRequest.Context.ApplicationSession,
						.Authority          = context->Authority,
						.ObjectFormat       = context->Handoff.ObjectFormat,
					};
					const auto openedCache = open_actor_cache(
						*context->Actor, *caches, expectedCache, aStop);
					if (!openedCache) {
						const auto failure = cache_failure(openedCache.error());
						return [aRequest, failure](implementation& aHost) {
							aHost.dispatch_core(
								core::comparison_resolve_failed_event{
									.Context = aRequest.Context,
									.Failure = failure,
								});
						};
					}
					std::vector<std::pair<core::git_commit_oid, bool>> verified;
					const auto                                         isVerified = [&](const core::git_commit_oid aCommit) {
						const auto prior = std::ranges::find_if(
							verified,
							[&](const auto& aEntry) {
								return aEntry.first == aCommit;
							});
						if (prior != verified.end()) {
							return prior->second;
						}
						const auto inspected = context->Actor->Cache->inspect_commit_object(
							aCommit, aStop);
						const auto exact = inspected
							&& application::valid(*inspected)
							&& inspected->Commit == aCommit;
						verified.emplace_back(aCommit, exact);
						return exact;
					};
					const auto verifyEpoch = [&](auto& aEpoch) {
						if (
							aEpoch.BaseAvailability
								== core::anchor_availability::available
							&& !isVerified(aEpoch.Base)) {
							aEpoch.BaseAvailability = core::anchor_availability::unavailable;
						}
						if (
							aEpoch.HeadAvailability
								== core::anchor_availability::available
							&& !isVerified(aEpoch.Head)) {
							aEpoch.HeadAvailability = core::anchor_availability::unavailable;
						}
					};
					const auto verifyAnchor = [&](auto& aAnchor) {
						if (
							aAnchor
							&& aAnchor->Availability
								== core::anchor_availability::available
							&& !isVerified(aAnchor->Commit)) {
							aAnchor->Availability = core::anchor_availability::unavailable;
						}
					};
					if (candidates.LatestLive) {
						verifyEpoch(*candidates.LatestLive);
					}
					if (candidates.PreviousDistinctHead) {
						verifyEpoch(*candidates.PreviousDistinctHead);
					}
					verifyAnchor(candidates.CurrentViewerReview);
					verifyAnchor(candidates.ExplicitLeft);
					verifyAnchor(candidates.ExplicitRight);
					return [aRequest, candidates = std::move(candidates)](
							   implementation& aHost) {
						aHost.dispatch_core(core::comparison_resolved_event{
							.Context    = aRequest.Context,
							.Candidates = candidates,
						});
					};
				},
				[aRequest](implementation& aHost) {
					aHost.dispatch_core(core::comparison_resolve_failed_event{
						.Context = aRequest.Context,
						.Failure = core::epoch_failure::unavailable,
					});
				},
				aRequest.Context.Request,
				std::nullopt,
				aRequest.Context.Generation,
				std::nullopt,
				std::move(actor));
		}

		core::epoch_state                State;
		git_cache_port&                  Caches;
		epoch_catalog_port&              Catalogs;
		epoch_entropy_port&              Entropy;
		epoch_clock_port&                Clock;
		effect_executor&                 Effects;
		decision_executor&               DecisionLane;
		epoch_projection_port&           Projections;
		std::shared_ptr<completion_gate> CompletionGate;
		std::vector<
			std::pair<cache_actor_key, std::shared_ptr<authority_effect_actor>>>
														   CacheActors;
		mutable std::mutex                                 CacheActorsMutex;
		std::shared_ptr<authority_context>                 Active;
		std::optional<pull_request_authority_invalidation> PendingAuthority;
		std::optional<std::uint64_t>                       PendingOfferToken;
		std::vector<std::shared_ptr<job_control>>          Jobs;
		std::deque<deferred_ingress>                       DeferredIngress;
		std::uint64_t                                      OfferToken               = 0U;
		bool                                               TurnActive               = false;
		bool                                               DeferredIngressScheduled = false;
		bool                                               Closing                  = false;
		bool                                               ProjectionDeliveryFailed = false;
		std::atomic_bool                                   BlobIngressClosing       = false;
	};

	epoch_application_host::epoch_application_host(
		core::epoch_state      aInitialState,
		git_cache_port&        aCaches,
		epoch_catalog_port&    aCatalogs,
		epoch_entropy_port&    aEntropy,
		epoch_clock_port&      aClock,
		effect_executor&       aEffects,
		decision_executor&     aDecisionLane,
		epoch_projection_port& aProjections)
		: Implementation{ std::make_unique<implementation>(
			  std::move(aInitialState),
			  aCaches,
			  aCatalogs,
			  aEntropy,
			  aClock,
			  aEffects,
			  aDecisionLane,
			  aProjections) }
	{
	}

	epoch_application_host::~epoch_application_host()
	{
		shutdown();
	}

	void epoch_application_host::accept(accepted_pull_request_handoff aHandoff)
	{
		Implementation->accept(std::move(aHandoff));
	}

	void epoch_application_host::invalidate(
		pull_request_authority_invalidation aInvalidation)
	{
		Implementation->invalidate(std::move(aInvalidation));
	}

	void epoch_application_host::dispatch(epoch_intent aIntent)
	{
		Implementation->dispatch(std::move(aIntent));
	}

	auto epoch_application_host::materialize(
		const diff_blob_request& aRequest,
		const std::stop_token    aStop) -> diff_blob_observation
	{
		return Implementation->materialize(aRequest, aStop);
	}

	void epoch_application_host::shutdown()
	{
		Implementation->shutdown();
	}

	auto epoch_application_host::state() const noexcept -> const core::epoch_state&
	{
		return Implementation->State;
	}

	auto epoch_application_host::is_closing() const noexcept -> bool
	{
		return Implementation->Closing;
	}

	auto epoch_application_host::is_quiescent() const noexcept -> bool
	{
		return Implementation->Closing && Implementation->Jobs.empty()
			&& !Implementation->TurnActive
			&& Implementation->DeferredIngress.empty()
			&& !Implementation->DeferredIngressScheduled
			&& !Implementation->PendingOfferToken
			&& Implementation->actors_idle();
	}

	auto epoch_application_host::projection_delivery_failed() const noexcept
		-> bool
	{
		return Implementation->ProjectionDeliveryFailed
			|| Implementation->CompletionGate->DeliveryFailed.load(
				std::memory_order_acquire);
	}
} // namespace bpr::application
