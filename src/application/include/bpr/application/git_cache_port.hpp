#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GIT_CACHE_PORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GIT_CACHE_PORT_HPP

#include "bpr/application/agent_workspace.hpp"
#include "bpr/application/git_pack_stream.hpp"
#include "bpr/application/git_process_port.hpp"
#include "bpr/core/epoch_model.hpp"
#include "bpr/core/epoch_transition.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace bpr::application {
	inline constexpr std::uint64_t maximum_git_cache_operation_pack_bytes       = 4ULL * 1024ULL * 1024ULL * 1024ULL;
	inline constexpr std::uint64_t maximum_git_cache_blob_materialization_bytes = 16ULL * 1024ULL * 1024ULL;
	inline constexpr std::uint64_t maximum_git_repository_cache_bytes           = 64ULL * 1024ULL * 1024ULL * 1024ULL;
	inline constexpr std::uint32_t maximum_git_cache_pack_count                 = 4096U;
	inline constexpr std::uint32_t maximum_git_cache_private_ref_count          = 200'000U;

	/// \brief Computes the valid cache token result.
	[[nodiscard]] constexpr auto valid_cache_token(
		const std::array<std::uint8_t, 32U>& aToken) noexcept -> bool
	{
		for (const auto byte : aToken) {
			if (byte != 0U) {
				return true;
			}
		}
		return false;
	}

	/// \brief Describes the git cache limits record.
	struct git_cache_limits
	{
		std::uint64_t OperationPackBytes = maximum_git_cache_operation_pack_bytes;
		std::uint64_t RepositoryBytes    = maximum_git_repository_cache_bytes;
		std::uint32_t PackCount          = maximum_git_cache_pack_count;
		std::uint32_t PrivateRefCount    = maximum_git_cache_private_ref_count;

		auto operator<=>(const git_cache_limits&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const git_cache_limits& aLimits) noexcept -> bool
	{
		return aLimits.OperationPackBytes != 0U
			&& aLimits.OperationPackBytes
			<= maximum_git_cache_operation_pack_bytes
			&& aLimits.RepositoryBytes >= aLimits.OperationPackBytes
			&& aLimits.RepositoryBytes
			<= maximum_git_repository_cache_bytes
			&& aLimits.PackCount != 0U
			&& aLimits.PackCount <= maximum_git_cache_pack_count
			&& aLimits.PrivateRefCount >= 2U
			&& aLimits.PrivateRefCount
			<= maximum_git_cache_private_ref_count;
	}

	/// \brief Describes the git cache open request record.
	struct git_cache_open_request
	{
		core::application_session_id ApplicationSession;
		core::pull_request_authority Authority;
		core::git_hash_algorithm     ObjectFormat = core::git_hash_algorithm::invalid;
		git_cache_limits             Limits;

		auto operator<=>(const git_cache_open_request&) const = default;
	};

	[[nodiscard]] inline auto valid(
		const git_cache_open_request& aRequest) noexcept -> bool
	{
		return aRequest.ApplicationSession.value() != 0U
			&& core::valid(aRequest.Authority)
			&& (aRequest.ObjectFormat == core::git_hash_algorithm::sha1
				|| aRequest.ObjectFormat == core::git_hash_algorithm::sha256)
			&& valid(aRequest.Limits);
	}

	/// \brief Describes one exact blob read under an owned cache session.
	struct git_cache_blob_request
	{
		core::git_blob_oid Blob;
		std::uint64_t      MaximumBytes = 0U;

		auto operator<=>(const git_cache_blob_request&) const = default;
	};

	/// \brief Describes one exact path lookup under an owned cache session.
	struct git_cache_tree_entry_request
	{
		core::git_commit_oid  Commit;
		std::filesystem::path Path;

		auto operator<=>(const git_cache_tree_entry_request&) const = default;
	};

	/// \brief Changed entries under the already retained lifetime-exclusive cache.
	struct git_cache_changed_side
	{
		std::string        Path;
		core::git_blob_oid Object;
		std::uint32_t      Mode = 0U;
	};
	struct git_cache_changed_entry
	{
		std::optional<git_cache_changed_side> Left;
		std::optional<git_cache_changed_side> Right;
		char                                  Change = 'M';
	};
	struct git_cache_changed_entries_request
	{
		core::git_commit_oid Left;
		core::git_commit_oid Right;
	};

	/// \brief Describes one selected-commit rename query under an owned cache.
	struct git_cache_rename_pair_request
	{
		core::git_commit_oid                 Left;
		core::git_commit_oid                 Right;
		std::filesystem::path                CurrentPath;
		std::optional<std::filesystem::path> PreviousPath;

		auto operator<=>(const git_cache_rename_pair_request&) const = default;
	};

	/// \brief Queries exact cached commits without resolving a moving base ref.
	struct git_cache_comparison_base_request
	{
		core::git_commit_oid Head;
		core::git_commit_oid SelectedBase;
	};

	/// \brief Reports a derived comparison candidate, never an observed historical base.
	struct git_cache_comparison_base_observation
	{
		git_comparison_base_status          Status = git_comparison_base_status::objects_unavailable;
		std::optional<core::git_commit_oid> Base;
	};

	/// \brief Describes the git pack inventory entry record.
	struct git_pack_inventory_entry
	{
		git_pack_oid  Pack;
		std::uint64_t PackBytes  = 0U;
		std::uint64_t IndexBytes = 0U;

		auto operator<=>(const git_pack_inventory_entry&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const git_pack_inventory_entry& aEntry) noexcept -> bool
	{
		return valid(aEntry.Pack) && aEntry.PackBytes != 0U
			&& aEntry.IndexBytes != 0U;
	}

	/// \brief Describes the git preimport observation record.
	struct git_preimport_observation
	{
		core::epoch_operation_id              Operation;
		git_pack_oid                          ExpectedPack;
		std::vector<git_pack_inventory_entry> Packs;
		std::uint64_t                         RepositoryBytes         = 0U;
		bool                                  ExpectedPackPreexisting = false;
		std::array<std::uint8_t, 32U>         OpaqueToken{ };

		auto operator<=>(const git_preimport_observation&) const = default;
	};

	[[nodiscard]] inline auto valid(
		const git_preimport_observation& aObservation) noexcept -> bool
	{
		if (
			!aObservation.Operation.valid()
			|| !valid(aObservation.ExpectedPack)
			|| aObservation.Packs.size() > maximum_git_cache_pack_count
			|| aObservation.RepositoryBytes
				> maximum_git_repository_cache_bytes
			|| !valid_cache_token(aObservation.OpaqueToken)) {
			return false;
		}
		auto          expectedPackPresent = false;
		std::uint64_t inventoryBytes      = 0U;
		for (std::size_t index = 0U; index < aObservation.Packs.size(); ++index) {
			const auto& entry = aObservation.Packs[index];
			if (
				!valid(entry)
				|| entry.PackBytes
					> aObservation.RepositoryBytes - inventoryBytes
				|| entry.IndexBytes
					> aObservation.RepositoryBytes
						- inventoryBytes - entry.PackBytes
				|| entry.Pack.Algorithm
					!= aObservation.ExpectedPack.Algorithm
				|| (index != 0U
					&& !(aObservation.Packs[index - 1U].Pack
						 < entry.Pack))) {
				return false;
			}
			inventoryBytes += entry.PackBytes + entry.IndexBytes;
			expectedPackPresent = expectedPackPresent
				|| entry.Pack == aObservation.ExpectedPack;
		}
		return expectedPackPresent
			== aObservation.ExpectedPackPreexisting;
	}

	/// \brief Describes the git import authorization record.
	struct git_import_authorization
	{
		core::epoch_catalog_incarnation_id CatalogIncarnation;
		core::epoch_catalog_revision       ExpectedCatalogRevision;
		core::epoch_operation_id           Operation;
		git_pack_oid                       ExpectedPack;
		std::array<std::uint8_t, 32U>      ReceiptToken{ };
		std::array<std::uint8_t, 32U>      InventoryToken{ };
		std::array<std::uint8_t, 32U>      DurableRecordToken{ };

		auto operator<=>(const git_import_authorization&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const git_import_authorization& aAuthorization) noexcept -> bool
	{
		return aAuthorization.CatalogIncarnation.valid()
			&& core::valid(aAuthorization.ExpectedCatalogRevision)
			&& aAuthorization.Operation.valid()
			&& valid(aAuthorization.ExpectedPack)
			&& valid_cache_token(aAuthorization.ReceiptToken)
			&& valid_cache_token(aAuthorization.InventoryToken)
			&& valid_cache_token(aAuthorization.DurableRecordToken);
	}

	/// \brief Describes the git prepare revision request record.
	struct git_prepare_revision_request
	{
		completed_git_pack_receipt          Receipt;
		git_preimport_observation           Inventory;
		git_import_authorization            Authorization;
		core::git_commit_oid                Base;
		core::git_commit_oid                Head;
		std::optional<core::git_commit_oid> PriorHead;
		/// Derive Base after installation; witness must journal the resulting anchor.
		bool DeriveComparisonBase = false;

		auto operator<=>(const git_prepare_revision_request&) const = default;
	};

	[[nodiscard]] inline auto valid(
		const git_prepare_revision_request& aRequest) noexcept -> bool
	{
		return agrees(aRequest.Receipt) && valid(aRequest.Inventory)
			&& valid(aRequest.Authorization) && aRequest.Base.valid()
			&& aRequest.Head.valid()
			&& aRequest.Base.algorithm() == aRequest.Head.algorithm()
			&& (!aRequest.PriorHead.has_value()
				|| (aRequest.PriorHead->valid()
					&& aRequest.PriorHead->algorithm()
						== aRequest.Head.algorithm()))
			&& aRequest.Base.algorithm()
			== aRequest.Receipt.ExpectedPack.Algorithm
			&& aRequest.Receipt.Operation == aRequest.Inventory.Operation
			&& aRequest.Receipt.Operation
			== aRequest.Authorization.Operation
			&& aRequest.Receipt.ExpectedPack
			== aRequest.Inventory.ExpectedPack
			&& aRequest.Receipt.ExpectedPack
			== aRequest.Authorization.ExpectedPack
			&& aRequest.Receipt.OpaqueToken
			== aRequest.Authorization.ReceiptToken
			&& aRequest.Inventory.OpaqueToken
			== aRequest.Authorization.InventoryToken;
	}

	/// \brief Retains an evidenced historical head without asserting an original base.
	struct git_prepare_historical_revision_request
	{
		completed_git_pack_receipt Receipt;
		git_preimport_observation  Inventory;
		git_import_authorization   Authorization;
		core::git_commit_oid       SelectedBase;
		core::git_commit_oid       Head;
	};

	/// \brief States precisely which object residency an import established.
	enum class git_revision_residency : std::uint8_t {
		complete_closure,
		sparse_commit_trees,
	};

	[[nodiscard]] constexpr auto valid(const git_revision_residency aResidency) noexcept -> bool
	{
		return aResidency == git_revision_residency::complete_closure
			|| aResidency == git_revision_residency::sparse_commit_trees;
	}

	/// \brief Describes the git prepared revision record.
	struct git_prepared_revision
	{
		core::epoch_operation_id Operation;
		git_pack_oid             Pack;
		git_artifact_fact        InstalledPack{
			.Role = git_artifact_role::installed_pack
		};
		git_artifact_fact InstalledIndex{
			.Role = git_artifact_role::installed_index
		};
		core::git_commit_oid Base;
		core::git_tree_oid   BaseTree;
		core::git_commit_oid Head;
		core::git_tree_oid   HeadTree;
		core::head_lineage   HeadLineage   = core::head_lineage::not_applicable;
		std::uint64_t        ImportedBytes = 0U;

		git_revision_residency Residency = git_revision_residency::complete_closure;

		auto operator<=>(const git_prepared_revision&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const git_prepared_revision& aRevision) noexcept -> bool
	{
		return valid(aRevision.Residency)
			&& (aRevision.Residency != git_revision_residency::sparse_commit_trees || (aRevision.Pack.Algorithm == core::git_hash_algorithm::sha1 && (aRevision.HeadLineage == core::head_lineage::unknown || aRevision.HeadLineage == core::head_lineage::not_applicable)))
			&& aRevision.Operation.valid() && valid(aRevision.Pack)
			&& valid(
				   aRevision.InstalledPack,
				   git_artifact_role::installed_pack)
			&& valid(
				   aRevision.InstalledIndex,
				   git_artifact_role::installed_index)
			&& aRevision.Base.valid() && aRevision.BaseTree.valid()
			&& aRevision.Head.valid() && aRevision.HeadTree.valid()
			&& aRevision.Base.algorithm() == aRevision.Pack.Algorithm
			&& aRevision.Base.algorithm() == aRevision.BaseTree.algorithm()
			&& aRevision.Base.algorithm() == aRevision.Head.algorithm()
			&& aRevision.Base.algorithm() == aRevision.HeadTree.algorithm()
			&& (aRevision.HeadLineage
					== core::head_lineage::not_applicable
				|| aRevision.HeadLineage
					== core::head_lineage::descendant
				|| aRevision.HeadLineage
					== core::head_lineage::non_fast_forward
				|| aRevision.HeadLineage
					== core::head_lineage::unknown)
			&& aRevision.ImportedBytes != 0U
			&& aRevision.ImportedBytes
			== aRevision.InstalledPack.Bytes;
	}

	// Durable catalog facts are projected into this Git-owned recovery contract.
	// The cache never interprets a physical final ref as catalog visibility or
	// durable success; it only reports exact physical evidence to the host.
	/// Exact receiving-file ownership, captured before payload consumption.
	struct git_spool_ownership
	{
		core::epoch_operation_id Operation;
		git_artifact_identity    Identity;
		std::uint64_t            MaximumBytes                                  = 0U;
		auto                     operator<=>(const git_spool_ownership&) const = default;
	};

	[[nodiscard]] constexpr auto valid(const git_spool_ownership& aOwnership) noexcept -> bool
	{
		return aOwnership.Operation.valid() && valid(aOwnership.Identity)
			&& aOwnership.MaximumBytes > 0U
			&& aOwnership.MaximumBytes <= maximum_git_cache_operation_pack_bytes;
	}

	struct git_recovery_spool_expectation
	{
		git_pack_oid      ExpectedPack;
		git_artifact_fact CompletedSpool{
			.Role = git_artifact_role::completed_spool
		};
		std::vector<git_pack_inventory_entry> PreimportPacks;
		std::uint64_t                         PreimportRepositoryBytes = 0U;
		bool                                  ExpectedPackPreexisting  = false;

		git_revision_residency Residency = git_revision_residency::complete_closure;

		auto operator<=>(const git_recovery_spool_expectation&) const = default;
	};

	[[nodiscard]] inline auto valid(
		const git_recovery_spool_expectation& aExpectation) noexcept -> bool
	{
		if (
			!valid(aExpectation.Residency)
			|| (aExpectation.Residency == git_revision_residency::sparse_commit_trees && aExpectation.ExpectedPack.Algorithm != core::git_hash_algorithm::sha1)
			|| !valid(aExpectation.ExpectedPack)
			|| !valid(
				aExpectation.CompletedSpool,
				git_artifact_role::completed_spool)
			|| aExpectation.CompletedSpool.Bytes
				> maximum_git_cache_operation_pack_bytes
			|| aExpectation.PreimportPacks.size()
				> maximum_git_cache_pack_count
			|| aExpectation.PreimportRepositoryBytes
				> maximum_git_repository_cache_bytes) {
			return false;
		}
		auto          expectedPresent = false;
		std::uint64_t inventoryBytes  = 0U;
		for (
			std::size_t index = 0U;
			index < aExpectation.PreimportPacks.size();
			++index) {
			const auto& entry = aExpectation.PreimportPacks[index];
			if (
				!valid(entry)
				|| entry.PackBytes
					> aExpectation.PreimportRepositoryBytes - inventoryBytes
				|| entry.IndexBytes
					> aExpectation.PreimportRepositoryBytes
						- inventoryBytes - entry.PackBytes
				|| entry.Pack.Algorithm
					!= aExpectation.ExpectedPack.Algorithm
				|| (index != 0U
					&& !(aExpectation.PreimportPacks[index - 1U].Pack
						 < entry.Pack))) {
				return false;
			}
			inventoryBytes += entry.PackBytes + entry.IndexBytes;
			expectedPresent = expectedPresent || entry.Pack == aExpectation.ExpectedPack;
		}
		return expectedPresent == aExpectation.ExpectedPackPreexisting;
	}

	/// \brief Describes the git recovery install expectation record.
	struct git_recovery_install_expectation
	{
		git_pack_oid      Pack;
		git_artifact_fact InstalledPack{
			.Role = git_artifact_role::installed_pack
		};
		git_artifact_fact InstalledIndex{
			.Role = git_artifact_role::installed_index
		};
		core::git_tree_oid BaseTree;
		core::git_tree_oid HeadTree;

		git_revision_residency Residency = git_revision_residency::complete_closure;

		auto operator<=>(const git_recovery_install_expectation&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const git_recovery_install_expectation& aExpectation) noexcept -> bool
	{
		return valid(aExpectation.Residency)
			&& (aExpectation.Residency != git_revision_residency::sparse_commit_trees || aExpectation.Pack.Algorithm == core::git_hash_algorithm::sha1)
			&& valid(aExpectation.Pack)
			&& valid(
				   aExpectation.InstalledPack,
				   git_artifact_role::installed_pack)
			&& valid(
				   aExpectation.InstalledIndex,
				   git_artifact_role::installed_index)
			&& aExpectation.InstalledPack.Bytes
			<= maximum_git_cache_operation_pack_bytes
			&& aExpectation.InstalledIndex.Bytes
			<= maximum_git_repository_cache_bytes
			&& aExpectation.BaseTree.valid() && aExpectation.HeadTree.valid()
			&& aExpectation.BaseTree.algorithm() == aExpectation.Pack.Algorithm
			&& aExpectation.HeadTree.algorithm() == aExpectation.Pack.Algorithm;
	}

	/// \brief Describes the git operation recovery request record.
	struct git_operation_recovery_request
	{
		core::epoch_operation_id                        Operation;
		core::pull_request_authority_scope_id           Scope;
		core::epoch_ordinal                             Ordinal;
		core::git_commit_oid                            Base;
		core::git_commit_oid                            Head;
		std::optional<git_recovery_spool_expectation>   Spool;
		std::optional<git_recovery_install_expectation> Install;
		std::optional<git_spool_ownership>              Receiving;

		auto operator<=>(const git_operation_recovery_request&) const = default;
	};

	[[nodiscard]] inline auto valid(
		const git_operation_recovery_request& aRequest) noexcept -> bool
	{
		return (!aRequest.Receiving || (valid(*aRequest.Receiving) && aRequest.Receiving->Operation == aRequest.Operation && (!aRequest.Spool || (aRequest.Spool->CompletedSpool.Identity == aRequest.Receiving->Identity && aRequest.Spool->CompletedSpool.Bytes <= aRequest.Receiving->MaximumBytes))))
			&& aRequest.Operation.valid() && aRequest.Scope.valid()
			&& core::valid(aRequest.Ordinal) && aRequest.Base.valid()
			&& aRequest.Head.valid()
			&& aRequest.Base.algorithm() == aRequest.Head.algorithm()
			&& (!aRequest.Spool
				|| (valid(*aRequest.Spool)
					&& aRequest.Spool->ExpectedPack.Algorithm
						== aRequest.Base.algorithm()))
			&& (!aRequest.Install
				|| (valid(*aRequest.Install)
					&& aRequest.Install->Pack.Algorithm
						== aRequest.Base.algorithm()))
			&& (!aRequest.Spool || !aRequest.Install
				|| (aRequest.Spool->Residency == aRequest.Install->Residency
					&& aRequest.Spool->ExpectedPack == aRequest.Install->Pack
					&& aRequest.Spool->CompletedSpool.Bytes
						== aRequest.Install->InstalledPack.Bytes));
	}

	enum class git_recovery_match : std::uint8_t {
		absent,
		exact,
		mismatch,
		ambiguous,
		// Installed pack/index only: the durable receipt and complete inventory
		// agree, but no installed file identities committed. This permits retaining
		// the pack during an abort with no refs; it is never deletion or publication
		// authority and still requires independent object-closure validation.
		inventory_only,
		// Spool only: an empty private receiving file with no committed ownership
		// or payload receipt. Abort may preserve it when no operation refs exist.
		// It must never be opened for payload, reused or deleted as owned residue.
		unowned_empty,
	};

	enum class git_recovery_object_match : std::uint8_t {
		not_checked,
		exact,
		unavailable,
		mismatch,
	};

	enum class git_recovery_ref_topology : std::uint8_t {
		absent,
		staging,
		final,
		ambiguous,
	};

	/// \brief Describes the git operation recovery observation record.
	struct git_operation_recovery_observation
	{
		core::epoch_operation_id  Operation;
		git_recovery_match        Spool     = git_recovery_match::absent;
		git_recovery_match        Installed = git_recovery_match::absent;
		git_recovery_match        Staging   = git_recovery_match::absent;
		git_recovery_match        Final     = git_recovery_match::absent;
		git_recovery_object_match Objects   = git_recovery_object_match::not_checked;
		git_recovery_ref_topology Refs      = git_recovery_ref_topology::absent;

		auto operator<=>(const git_operation_recovery_observation&) const = default;
	};

	/// \brief Describes the git retained ref pair record.
	struct git_retained_ref_pair
	{
		core::pull_request_authority_scope_id Scope;
		core::epoch_ordinal                   Ordinal;
		core::git_commit_oid                  Base;
		core::git_commit_oid                  Head;

		git_revision_residency Residency = git_revision_residency::complete_closure;

		auto operator<=>(const git_retained_ref_pair&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const git_retained_ref_pair& aPair) noexcept -> bool
	{
		return valid(aPair.Residency) && aPair.Scope.valid() && core::valid(aPair.Ordinal)
			&& aPair.Base.valid() && aPair.Head.valid()
			&& aPair.Base.algorithm() == aPair.Head.algorithm();
	}

	/// \brief Describes the git cache commit inspection record.
	struct git_cache_commit_inspection
	{
		core::git_commit_oid Commit;
		core::git_tree_oid   Tree;

		git_revision_residency Residency = git_revision_residency::complete_closure;

		auto operator<=>(const git_cache_commit_inspection&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const git_cache_commit_inspection& aInspection) noexcept -> bool
	{
		return valid(aInspection.Residency) && aInspection.Commit.valid() && aInspection.Tree.valid()
			&& aInspection.Commit.algorithm() == aInspection.Tree.algorithm();
	}

	/// \brief Describes the git ref promotion authorization record.
	struct git_ref_promotion_authorization
	{
		core::epoch_catalog_incarnation_id    CatalogIncarnation;
		core::epoch_catalog_revision          ExpectedCatalogRevision;
		core::epoch_operation_id              Operation;
		core::pull_request_authority_scope_id Scope;
		core::epoch_ordinal                   Ordinal;
		core::git_commit_oid                  Base;
		core::git_commit_oid                  Head;
		std::array<std::uint8_t, 32U>         DurableCommitToken{ };

		auto operator<=>(const git_ref_promotion_authorization&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const git_ref_promotion_authorization& aAuthorization) noexcept -> bool
	{
		return aAuthorization.CatalogIncarnation.valid()
			&& core::valid(aAuthorization.ExpectedCatalogRevision)
			&& aAuthorization.Operation.valid() && aAuthorization.Scope.valid()
			&& core::valid(aAuthorization.Ordinal)
			&& aAuthorization.Base.valid() && aAuthorization.Head.valid()
			&& aAuthorization.Base.algorithm()
			== aAuthorization.Head.algorithm()
			&& valid_cache_token(aAuthorization.DurableCommitToken);
	}

	/// \brief Describes the git ref prune authorization record.
	struct git_ref_prune_authorization
	{
		core::epoch_catalog_incarnation_id CatalogIncarnation;
		core::epoch_catalog_revision       ExpectedCatalogRevision;
		git_retained_ref_pair              Expected;
		std::array<std::uint8_t, 32U>      DurableIntentToken{ };

		auto operator<=>(const git_ref_prune_authorization&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const git_ref_prune_authorization& aAuthorization) noexcept -> bool
	{
		return aAuthorization.CatalogIncarnation.valid()
			&& core::valid(aAuthorization.ExpectedCatalogRevision)
			&& valid(aAuthorization.Expected)
			&& valid_cache_token(aAuthorization.DurableIntentToken);
	}

	/// \brief Describes the git cache delete authorization record.
	struct git_cache_delete_authorization
	{
		core::application_session_id    ApplicationSession;
		core::repository_cache_scope_id Cache;
		std::array<std::uint8_t, 32U>   DurableIntentToken{ };

		auto operator<=>(const git_cache_delete_authorization&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const git_cache_delete_authorization& aAuthorization) noexcept -> bool
	{
		return aAuthorization.ApplicationSession.value() != 0U
			&& aAuthorization.Cache.valid()
			&& valid_cache_token(aAuthorization.DurableIntentToken);
	}

	enum class git_cache_failure : std::uint8_t {
		invalid_request,
		invalid_authority,
		invalid_path,
		invalid_manifest,
		invalid_repository,
		lock_unavailable,
		quota_exceeded,
		spool_failure,
		inventory_mismatch,
		authorization_mismatch,
		process_failure,
		object_mismatch,
		ref_conflict,
		repair_required,
		cancelled,
		/// \brief The fetched range carries an object Git refuses to accept.
		///
		/// \details `Detail` names the fsck message class, so the refusal can
		/// say what is wrong with the repository instead of reporting that some
		/// Git command failed. Nothing about it is repairable locally and no
		/// retry can help (BPR-0880).
		unsupported_object,
	};

	/// \brief Carries a cache failure and any safe installed-Git explanation.
	struct git_cache_error
	{
		git_cache_failure Code = git_cache_failure::invalid_request;
		std::string       Detail;

		git_cache_error() = default;

		// Implicit on purpose: a bare failure code is still a complete error.
		git_cache_error(const git_cache_failure aCode)
			: Code{ aCode }
		{
		}

		git_cache_error(
			const git_cache_failure aCode,
			decltype(Detail)        aDetail)
			: Code{ aCode }
			, Detail{ std::move(aDetail) }
		{
		}

		auto operator<=>(const git_cache_error&) const = default;
	};

	/// \brief Compares a Git cache error with its primary failure code.
	[[nodiscard]] inline auto operator==(
		const git_cache_error&  aError,
		const git_cache_failure aCode) noexcept -> bool
	{
		return aError.Code == aCode;
	}

	template <typename Value>
	using git_cache_result = std::expected<Value, git_cache_error>;

	/// \brief Provides the git cache session contract.
	class git_cache_session
	{
	public:
		git_cache_session()                                            = default;
		git_cache_session(const git_cache_session&)                    = delete;
		auto operator=(const git_cache_session&) -> git_cache_session& = delete;
		git_cache_session(git_cache_session&&)                         = delete;
		auto operator=(git_cache_session&&) -> git_cache_session&      = delete;
		virtual ~git_cache_session()                                   = default;

		[[nodiscard]] virtual auto request() const noexcept
			-> const git_cache_open_request& = 0;

		// The optional witness runs synchronously after creating the empty file,
		// before any payload can be written. Persist ownership before returning true.
		// Do not reenter this session from the witness; its operation lock is held.
		// Refusal cleans only the exact empty file and returns no usable sink.
		[[nodiscard]] virtual auto create_pack_sink(
			core::epoch_operation_id                               aOperation,
			std::uint64_t                                          aMaximumBytes,
			std::stop_token                                        aStop,
			const std::function<bool(const git_spool_ownership&)>& aWitness = { })
			-> git_cache_result<std::unique_ptr<git_pack_stream_sink>> = 0;

		[[nodiscard]] virtual auto observe_preimport(
			const completed_git_pack_receipt& aReceipt,
			std::stop_token                   aStop)
			-> git_cache_result<git_preimport_observation> = 0;

		// When supplied, the witness runs synchronously on the effect worker after
		// object validation and before staging refs/spool deletion. It must durably
		// record the exact installation before returning true. A refusal preserves
		// potentially journaled artifacts for recovery. The witness must not reenter
		// this cache session while preparation holds its operation lock.
		[[nodiscard]] virtual auto prepare_revision(
			const git_prepare_revision_request&                      aRequest,
			std::stop_token                                          aStop,
			const std::function<bool(const git_prepared_revision&)>& aWitness = { })
			-> git_cache_result<git_prepared_revision> = 0;

		/// \brief Imports exact SHA-1 commit/root trees with explicitly incomplete closure.
		/// \details Requires a durable installation witness. Parents, subtrees and blobs
		/// may be absent. Never derives a comparison base or proves ancestry. The host
		/// must preserve Residency in the durable fact and recovery expectation.
		[[nodiscard]] virtual auto prepare_sparse_revision(
			const git_prepare_revision_request& aRequest, std::stop_token aStop,
			const std::function<bool(const git_prepared_revision&)>& aWitness)
			-> git_cache_result<git_prepared_revision>
		{
			static_cast<void>(aRequest);
			static_cast<void>(aStop);
			static_cast<void>(aWitness);
			return std::unexpected{ git_cache_failure::invalid_request };
		}

		/// \brief Checks exact retained refs and commit/root trees without closure claims.
		/// \details Residency reports the extent checked, not durable import mode.
		/// Obtain durable mode from the catalog before selecting this inspection.
		[[nodiscard]] virtual auto inspect_sparse_retained(core::epoch_ordinal aOrdinal, std::stop_token aStop)
			-> git_cache_result<git_retained_ref_pair>
		{
			static_cast<void>(aOrdinal);
			static_cast<void>(aStop);
			return std::unexpected{ git_cache_failure::invalid_request };
		}

		/// \brief Checks exact SHA-1 commit/root-tree residency, never a negotiation have.
		[[nodiscard]] virtual auto inspect_sparse_commit_object(core::git_commit_oid aCommit, std::stop_token aStop)
			-> git_cache_result<git_cache_commit_inspection>
		{
			static_cast<void>(aCommit);
			static_cast<void>(aStop);
			return std::unexpected{ git_cache_failure::invalid_request };
		}

		/// \brief Prepares exact historical head closure through the journaled import path.
		/// \details The returned Base and BaseTree derive from SelectedBase and Head and
		/// are comparison anchors only, never evidence of the original historical base.
		/// The witness must journal these exact physical facts before staging; keep
		/// historical provenance and any derived comparison base separately. Derive
		/// any later comparison after this call returns; never reenter from the witness.
		[[nodiscard]] auto prepare_historical_revision(
			const git_prepare_historical_revision_request&           aRequest,
			std::stop_token                                          aStop,
			const std::function<bool(const git_prepared_revision&)>& aWitness = { })
			-> git_cache_result<git_prepared_revision>
		{
			return prepare_revision({
										.Receipt              = aRequest.Receipt,
										.Inventory            = aRequest.Inventory,
										.Authorization        = aRequest.Authorization,
										.Base                 = aRequest.SelectedBase,
										.Head                 = aRequest.Head,
										.DeriveComparisonBase = true,
									},
									aStop, aWitness);
		}

		[[nodiscard]] virtual auto promote_retained(
			const git_prepared_revision&           aPrepared,
			const git_ref_promotion_authorization& aAuthorization,
			std::stop_token                        aStop)
			-> git_cache_result<git_retained_ref_pair> = 0;

		[[nodiscard]] virtual auto inspect_retained(
			core::epoch_ordinal aOrdinal,
			std::stop_token     aStop)
			-> git_cache_result<git_retained_ref_pair> = 0;

		// Verifies an exact commit independently of catalog availability or a
		// retained epoch ref. This is the restart-safe gate for protocol-v2 haves
		// and review-commit anchors: commit type, tree, and reachable closure are
		// checked under the exact cache writer lock.
		[[nodiscard]] virtual auto inspect_commit_object(
			core::git_commit_oid aCommit,
			std::stop_token      aStop)
			-> git_cache_result<git_cache_commit_inspection> = 0;

		/// \brief Queries a derived base under this active session's operation lock.
		/// \details No network fetch or new session is performed. Both commits must
		/// have complete reachable closure; missing or ambiguous ancestry yields no
		/// candidate. SelectedBase is an exact selected commit, never historical
		/// provenance. The caller keeps that distinction when storing the result.
		[[nodiscard]] virtual auto derive_comparison_base(
			const git_cache_comparison_base_request& aRequest,
			std::stop_token                          aStop) -> git_cache_result<git_cache_comparison_base_observation>
		{
			static_cast<void>(aRequest);
			static_cast<void>(aStop);
			return std::unexpected{ git_cache_failure::invalid_request };
		}

		/// \brief Reads exact head committer time as UTC Unix seconds, not first-seen.
		/// \details Runs under this active session. Missing commits return nullopt;
		/// this does not establish a push time or chronological update order.
		[[nodiscard]] virtual auto read_committer_time_seconds(
			core::git_commit_oid aCommit,
			std::stop_token      aStop) -> git_cache_result<std::optional<std::int64_t>>
		{
			static_cast<void>(aCommit);
			static_cast<void>(aStop);
			return std::unexpected{ git_cache_failure::invalid_request };
		}

		/// \brief Copies one exact accepted head into an isolated owned workspace.
		/// \details [[BPR-1738]]. A sparse review supplies `aSnapshot`,
		/// the head's complete snapshot, because its cache holds no blobs or
		/// parents to copy; the workspace is then shallow at the head.
		[[nodiscard]] virtual auto materialize_agent_workspace(
			const agent_workspace_materialization_request&    aRequest,
			const std::optional<git_workspace_snapshot_pack>& aSnapshot,
			std::stop_token                                   aStop)
			-> git_cache_result<owned_agent_workspace>
		{
			static_cast<void>(aRequest);
			static_cast<void>(aSnapshot);
			static_cast<void>(aStop);
			return std::unexpected{ git_cache_failure::invalid_request };
		}

		/// \brief Removes only the exact workspace this session materialized.
		[[nodiscard]] virtual auto cleanup_agent_workspace(
			const owned_agent_workspace& aWorkspace,
			std::stop_token              aStop) -> git_cache_result<void>
		{
			static_cast<void>(aWorkspace);
			static_cast<void>(aStop);
			return std::unexpected{ git_cache_failure::invalid_request };
		}

		// Materializes through this lifetime-exclusive session so callers never
		// open a competing cache writer. The process adapter rejects oversize
		// objects before retaining their content bytes.
		[[nodiscard]] virtual auto materialize_blob(
			const git_cache_blob_request& aRequest,
			std::stop_token               aStop)
			-> git_cache_result<std::vector<std::uint8_t>> = 0;

		/// \brief Resolves one exact commit-relative path without opening a session.
		[[nodiscard]] virtual auto find_blob_at_path(
			const git_cache_tree_entry_request& aRequest,
			std::stop_token                     aStop)
			-> git_cache_result<std::optional<core::git_blob_oid>> = 0;

		/// \brief Complete exact retained-tree membership; missing trees never mean empty.
		[[nodiscard]] virtual auto changed_entries(const git_cache_changed_entries_request& aRequest, std::stop_token aStop)
			-> git_cache_result<std::vector<git_cache_changed_entry>>
		{
			static_cast<void>(aRequest);
			static_cast<void>(aStop);
			return std::unexpected{ git_cache_failure::invalid_request };
		}

		/// \brief Finds bounded rename records without opening another session.
		[[nodiscard]] virtual auto find_rename_pairs(
			const git_cache_rename_pair_request& aRequest,
			std::stop_token                      aStop)
			-> git_cache_result<git_rename_pair_observation> = 0;

		[[nodiscard]] virtual auto prune_retained(
			const git_ref_prune_authorization& aAuthorization,
			std::stop_token                    aStop) -> git_cache_result<void> = 0;

		[[nodiscard]] virtual auto abort_operation(
			core::epoch_operation_id aOperation,
			std::stop_token          aStop) -> git_cache_result<void> = 0;

		// This is a physical recovery inspection only. The catalog phase remains
		// the sole durable visibility/success authority even when final refs are
		// exact. Every filesystem scan and Git subprocess remains bounded and
		// cancellable under the exact cache writer lock.
		[[nodiscard]] virtual auto inspect_operation_recovery(
			const git_operation_recovery_request& aRequest,
			std::stop_token                       aStop)
			-> git_cache_result<git_operation_recovery_observation> = 0;

		// Re-inspects under the session's operation lock and removes only an
		// exact completed spool and exact operation staging pair. Final refs,
		// installed packs/indexes, mismatches, and ambiguous residue are never
		// deleted by this operation-scoped abort.
		[[nodiscard]] virtual auto abort_recovered_operation(
			const git_operation_recovery_request& aRequest,
			std::stop_token                       aStop) -> git_cache_result<void> = 0;

		// Successful catalog cleanup is intentionally distinct from abort. It
		// requires the exact final pair and witnessed object residency, removes only an exact
		// completed spool/staging residue, and never marks the operation aborted
		// or consumes bounded abort-history capacity.
		[[nodiscard]] virtual auto complete_recovered_operation(
			const git_operation_recovery_request& aRequest,
			std::stop_token                       aStop) -> git_cache_result<void> = 0;
	};

	/// \brief Provides the git cache port contract.
	class git_cache_port
	{
	public:
		git_cache_port()                                         = default;
		git_cache_port(const git_cache_port&)                    = delete;
		auto operator=(const git_cache_port&) -> git_cache_port& = delete;
		git_cache_port(git_cache_port&&)                         = delete;
		auto operator=(git_cache_port&&) -> git_cache_port&      = delete;
		virtual ~git_cache_port()                                = default;

		[[nodiscard]] virtual auto open(
			const git_cache_open_request& aRequest,
			std::stop_token               aStop)
			-> git_cache_result<std::unique_ptr<git_cache_session>> = 0;

		[[nodiscard]] virtual auto delete_cache(
			const git_cache_open_request&         aExpected,
			const git_cache_delete_authorization& aAuthorization,
			std::stop_token                       aStop) -> git_cache_result<void> = 0;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GIT_CACHE_PORT_HPP
