#include "../support/test_expectation.hpp"
#include "bpr/adapters/sqlite/epoch_catalog.hpp"
#include "bpr/adapters/sqlite/review_ledger_store.hpp"
#include "bpr/adapters/sqlite/testing.hpp"
#include "bpr/adapters/sqlite/window_review_restoration_store.hpp"
#include "bpr/application/github_host_authority.hpp"
#include "bpr/core/sha256.hpp"
#include "epoch_catalog_legacy_fixture.hpp"
#include "platform_test_support.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <barrier>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>

namespace {
	namespace app           = bpr::application;
	namespace core          = bpr::core;
	namespace sqlite        = bpr::adapters::sqlite;
	namespace test_platform = bpr::tests::sqlite_contract;

	using bpr::tests::expect;

	template <typename Value>
	void expect_failure(
		const app::epoch_catalog_result<Value>& aResult,
		const app::epoch_catalog_failure        aFailure,
		const std::string_view                  aMessage)
	{
		expect(!aResult && aResult.error() == aFailure, aMessage);
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		bytes[0]   = aSeed;
		bytes[11]  = static_cast<std::uint8_t>(aSeed ^ 0x5AU);
		return Identity{ bytes };
	}

	template <typename Identity>
	[[nodiscard]] auto hex_identity(const Identity& aIdentity) -> std::string
	{
		constexpr std::string_view digits = "0123456789ABCDEF";
		auto                       output = std::string{ };
		output.reserve(aIdentity.bytes().size() * 2U);
		for (const auto byte : aIdentity.bytes()) {
			output.push_back(digits[byte >> 4U]);
			output.push_back(digits[byte & 0x0FU]);
		}
		return output;
	}

	template <typename Oid>
	[[nodiscard]] constexpr auto oid(const std::uint8_t aSeed) -> Oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		bytes[0]   = aSeed;
		bytes[7]   = static_cast<std::uint8_t>(aSeed ^ 0xC3U);
		return Oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] constexpr auto pack(const std::uint8_t aSeed)
		-> app::git_pack_oid
	{
		auto digest = std::array<std::uint8_t, 32U>{ };
		digest[0]   = aSeed;
		digest[9]   = static_cast<std::uint8_t>(aSeed ^ 0x96U);
		return {
			.Algorithm = core::git_hash_algorithm::sha1,
			.Digest    = digest,
		};
	}

	[[nodiscard]] constexpr auto artifact(
		const app::git_artifact_role aRole,
		const std::uint8_t           aSeed,
		const std::uint64_t          aBytes) -> app::git_artifact_fact
	{
		auto identity = std::array<std::uint8_t, 32U>{ };
		identity[0]   = 1U;
		identity[1]   = 1U;
		identity[2]   = static_cast<std::uint8_t>(aRole);
		identity[8]   = aSeed;
		identity[24]  = static_cast<std::uint8_t>(aSeed ^ 0x69U);
		return {
			.Role     = aRole,
			.Identity = app::git_artifact_identity{ identity },
			.Bytes    = aBytes,
		};
	}

	[[nodiscard]] auto token_canary() -> std::array<std::uint8_t, 32U>
	{
		constexpr std::string_view canary = "BPR_TOKEN_CANARY_NEVER_DURABLE";
		auto                       output = std::array<std::uint8_t, 32U>{ };
		for (std::size_t index = 0U; index < canary.size(); ++index) {
			output[index] = static_cast<std::uint8_t>(canary[index]);
		}
		return output;
	}

	class temporary_root final
	{
	public:
		explicit temporary_root(const std::string_view aLabel)
		{
			const auto base = std::filesystem::temp_directory_path();
			for (std::uint32_t attempt = 0U; attempt < 1'000U; ++attempt) {
				auto candidate = base
					/ (std::string{ "bpr-sqlite-behavior-" }
					   + std::string{ aLabel } + "-"
					   + std::to_string(
						   test_platform::process_id())
					   + "-" + std::to_string(attempt));
				std::error_code error;
				if (std::filesystem::create_directory(candidate, error)) {
					// [[BPR-1121]]. Removed again if the rest of construction fails.
					auto made = bpr::tests::scratch_guard{ candidate };
					expect(
						test_platform::make_owner_private(candidate, true),
						"behavior-test root becomes owner-private");
					Path = std::filesystem::canonical(candidate);
					made.release();
					return;
				}
				expect(
					!error || error == std::errc::file_exists,
					"temporary root creation fails only for a collision");
			}
			expect(false, "a unique behavior-test root is available");
		}

		temporary_root(const temporary_root&)                    = delete;
		auto operator=(const temporary_root&) -> temporary_root& = delete;

		~temporary_root()
		{
			std::error_code ignored;
			(void)test_platform::make_owner_private(Path, true);
			std::filesystem::remove_all(Path, ignored);
		}

		[[nodiscard]] auto path() const -> const std::filesystem::path&
		{
			return Path;
		}

	private:
		std::filesystem::path Path;
	};

	[[nodiscard]] auto database_path(const std::filesystem::path& aRoot)
		-> std::filesystem::path
	{
		return aRoot / "state" / "v1" / "epoch-catalog.sqlite3";
	}

	[[nodiscard]] auto direct_count(
		const std::filesystem::path& aPath,
		std::string_view             aSql) -> std::int64_t;

	[[nodiscard]] auto authority_key(
		const std::uint64_t aRepository  = 0xF200000000000006ULL,
		const std::uint64_t aPullRequest = 0xF200000000000007ULL,
		const std::uint32_t aNumber      = 4242U)
		-> app::epoch_catalog_authority_key
	{
		return {
			.Profile         = core::host_profile_id{ 0xF200000000000001ULL },
			.ProfileRevision = core::github_profile_revision{ 0xF200000000000002ULL },
			.Host            = core::git_host_id{ 0xF200000000000003ULL },
			.HostAuthority   = opaque<core::host_authority_fingerprint>(0x41U),
			.Account         = core::github_account_id{ 0xF200000000000004ULL },
			.Viewer          = core::github_viewer_actor_id{ 0xF200000000000005ULL },
			.Repository      = core::repository_id{ aRepository },
			.PullRequest     = core::pull_request_id{ aPullRequest },
			.Number          = core::pull_request_number{ aNumber },
			.ObjectFormat    = core::git_hash_algorithm::sha1,
		};
	}

	[[nodiscard]] auto open_request(
		const std::uint64_t                     aSession,
		const app::epoch_catalog_authority_key& aAuthority = authority_key())
		-> app::epoch_catalog_open_request
	{
		return {
			.ApplicationSession = core::application_session_id{ aSession },
			.Authority          = aAuthority,
		};
	}

	[[nodiscard]] auto revision(
		const std::uint8_t                                          aSeed,
		const std::optional<core::current_viewer_review_candidate>& aReview = std::nullopt) -> core::accepted_pull_request_revision
	{
		return {
			.Source = core::pull_request_source_revision{
				.Observation = opaque<core::epoch_source_observation_id>(
					aSeed),
				.Snapshot = core::provider_snapshot_id{
					0xF300000000000000ULL + aSeed },
				.IdentityGeneration    = core::github_identity_generation{ 0xF400000000000000ULL + aSeed },
				.PullRequestGeneration = core::request_generation{ 0xF500000000000000ULL + aSeed },
			},
			.Origin              = core::pull_request_revision_origin::live,
			.BaseBranch          = opaque<core::base_branch_id>(static_cast<std::uint8_t>(0x50U + aSeed)),
			.Base                = oid<core::git_commit_oid>(static_cast<std::uint8_t>(0x60U + aSeed)),
			.Head                = oid<core::git_commit_oid>(static_cast<std::uint8_t>(0x70U + aSeed)),
			.CurrentViewerReview = aReview,
		};
	}

	[[nodiscard]] auto operation(const std::uint8_t aSeed)
		-> core::epoch_operation_id
	{
		return opaque<core::epoch_operation_id>(aSeed);
	}

	[[nodiscard]] auto context(
		const app::epoch_catalog_binding&           aBinding,
		const core::epoch_operation_id&             aId,
		const core::accepted_pull_request_revision& aAccepted,
		const std::uint8_t                          aSeed,
		const core::application_session_id          aApplicationSession)
		-> app::append_operation_context
	{
		return {
			.ApplicationSession = aApplicationSession,
			.Request            = core::epoch_request_id{ 0xF700000000000000ULL + aSeed },
			.Generation         = core::epoch_generation{
				0xF800000000000000ULL + aSeed },
			.Operation = aId,
			.Revision  = aAccepted,
			.Catalog   = aBinding.Catalog,
		};
	}

	[[nodiscard]] auto spool_request(
		const app::append_operation_record& aRecord,
		const std::uint8_t                  aSeed,
		const bool                          aOverflowInventory = false) -> app::record_spool_request
	{
		const auto expectedPack = pack(static_cast<std::uint8_t>(0x20U + aSeed));
		auto       receipt      = app::completed_git_pack_receipt{
			.Operation    = aRecord.Context.Operation,
			.ExpectedPack = expectedPack,
			.ObservedPack = expectedPack,
			.Spool        = artifact(
				app::git_artifact_role::completed_spool,
				aSeed,
				4'096U),
			.OpaqueToken = token_canary(),
		};
		auto inventory = app::git_preimport_observation{
			.Operation    = aRecord.Context.Operation,
			.ExpectedPack = expectedPack,
			.Packs        = {
				app::git_pack_inventory_entry{
					.Pack       = expectedPack,
					.PackBytes  = aOverflowInventory
						? std::numeric_limits<std::uint64_t>::max()
						: 4'096U,
					.IndexBytes = 1'024U,
				},
			},
			.RepositoryBytes         = 8'192U,
			.ExpectedPackPreexisting = true,
			.OpaqueToken             = token_canary(),
		};
		expect(app::valid(receipt), "ephemeral spool receipt is valid");
		expect(
			app::valid(inventory) != aOverflowInventory,
			aOverflowInventory
				? "overflowing pre-import inventory is invalid"
				: "ephemeral pre-import inventory is valid");
		return {
			.Catalog                   = aRecord.Context.Catalog,
			.Operation                 = aRecord.Context.Operation,
			.ExpectedOperationRevision = aRecord.OperationRevision,
			.Spool                     = app::durable_fact(receipt),
			.Preimport                 = app::durable_fact(inventory),
		};
	}

	[[nodiscard]] auto install_request(
		const app::append_operation_record& aRecord,
		const std::uint8_t                  aSeed,
		const core::head_lineage            aLineage = core::head_lineage::not_applicable) -> app::record_install_request
	{
		expect(
			aRecord.Spool.has_value(),
			"install fixture follows a durable spool record");
		return {
			.Catalog                   = aRecord.Context.Catalog,
			.Operation                 = aRecord.Context.Operation,
			.ExpectedOperationRevision = aRecord.OperationRevision,
			.Install                   = app::durable_git_install_fact{
				.Operation     = aRecord.Context.Operation,
				.Pack          = aRecord.Spool->ExpectedPack,
				.InstalledPack = artifact(
					app::git_artifact_role::installed_pack,
					static_cast<std::uint8_t>(aSeed + 1U),
					4'096U),
				.InstalledIndex = artifact(
					app::git_artifact_role::installed_index,
					static_cast<std::uint8_t>(aSeed + 2U),
					1'024U),
				.Base     = aRecord.Context.Revision.Base,
				.BaseTree = oid<core::git_tree_oid>(
					static_cast<std::uint8_t>(0x80U + aSeed)),
				.Head     = aRecord.Context.Revision.Head,
				.HeadTree = oid<core::git_tree_oid>(
					static_cast<std::uint8_t>(0x90U + aSeed)),
				.HeadLineage = aLineage,
			},
		};
	}

	[[nodiscard]] auto append_record(
		app::epoch_catalog_result<app::epoch_recovery_operation> aResult,
		const std::string_view                                   aMessage) -> app::append_operation_record
	{
		expect(aResult.has_value(), aMessage);
		const auto* record = std::get_if<app::append_operation_record>(&*aResult);
		expect(record != nullptr, "operation transition returns append record");
		return *record;
	}

	[[nodiscard]] auto transition_append(
		app::epoch_catalog_session&                  aSession,
		const core::epoch_catalog_identity&          aCatalog,
		const app::append_operation_record&          aRecord,
		const app::epoch_catalog_operation_phase     aNext,
		const app::epoch_catalog_transition_evidence aEvidence)
		-> app::append_operation_record
	{
		return append_record(
			aSession.transition_operation(
				app::operation_transition_request{
					.Catalog                   = aCatalog,
					.Operation                 = aRecord.Context.Operation,
					.Kind                      = app::epoch_catalog_operation_kind::append,
					.Expected                  = aRecord.Phase,
					.Next                      = aNext,
					.Evidence                  = aEvidence,
					.ExpectedOperationRevision = aRecord.OperationRevision,
				},
				{ }),
			"append phase transition succeeds");
	}

	[[nodiscard]] auto candidate(
		const app::append_operation_record&             aRecord,
		const std::int64_t                              aObservedAt,
		const bool                                      aInitial,
		std::optional<core::catalog_review_association> aReview = std::nullopt)
		-> core::append_epoch_mutation
	{
		expect(aRecord.Install.has_value(), "candidate follows installed facts");
		return {
			.Epoch = core::review_epoch_summary{
				.Id = core::review_epoch_id{
					.Authority   = aRecord.StagingBase.Authority,
					.Incarnation = aRecord.Context.Catalog.Incarnation,
					.Ordinal     = aRecord.ReservedOrdinal,
				},
				.Source     = aRecord.Context.Revision.Source,
				.BaseBranch = aRecord.Context.Revision.BaseBranch,
				.Base       = aRecord.Context.Revision.Base,
				.Head       = aRecord.Context.Revision.Head,
				.Material   = core::epoch_material_identity{
					.BaseTree       = aRecord.Install->BaseTree,
					.HeadTree       = aRecord.Install->HeadTree,
					.PolicyRevision = core::comparison_policy_revision{ 1U },
				},
				.Cause            = aInitial ? core::epoch_cause{
												   .Origin   = core::epoch_origin::initial_observation,
												   .Base     = core::epoch_base_transition::not_applicable,
												   .Head     = core::epoch_head_transition::not_applicable,
												   .Material = core::epoch_material_relation::unknown,
												   .Primary  = core::epoch_primary_cause::initial_observation,
											   }
											 : core::epoch_cause{
												   .Origin   = core::epoch_origin::live_observation,
												   .Base     = core::epoch_base_transition::unchanged,
												   .Head     = core::epoch_head_transition::descendant,
												   .Material = core::epoch_material_relation::different,
												   .Primary  = core::epoch_primary_cause::author_upload,
											   },
				.ObservedAt       = core::observed_utc_milliseconds{ aObservedAt },
				.BaseAvailability = core::anchor_availability::available,
				.HeadAvailability = core::anchor_availability::available,
			},
			.Review = std::move(aReview),
		};
	}

	struct append_outcome
	{
		core::append_epoch_mutation  Mutation;
		app::catalog_commit_result   Commit;
		app::append_operation_record Terminal;
	};

	struct append_fixture_options
	{
		bool                                                PersistReceiving = false;
		std::optional<core::accepted_pull_request_revision> Accepted;
		std::optional<core::epoch_cause>                    Cause;
		std::optional<core::epoch_occurrence_id>            HistoricalOccurrence;
		std::optional<core::git_commit_oid>                 DerivedComparisonBase;
		bool                                                StopBeforeFinalize = false;
		app::git_revision_residency                         Residency          = app::git_revision_residency::complete_closure;
	};

	[[nodiscard]] auto complete_append(
		app::epoch_catalog_session&        aSession,
		const core::epoch_ordinal          aOrdinal,
		const std::uint8_t                 aSeed,
		const core::application_session_id aApplicationSession,
		const bool                         aInitial,
		const bool                         aProveAtomicSpool = false,
		const std::filesystem::path*       aInspectionRoot   = nullptr,
		const append_fixture_options&      aOptions          = { }) -> append_outcome
	{
		const auto accepted = aOptions.Accepted.value_or(revision(aSeed));
		auto       reserved = aSession.reserve_append(
			app::append_reservation_request{
				.Context = context(
					aSession.binding(),
					operation(aSeed),
					accepted,
					aSeed,
					aApplicationSession),
				.ProposedOrdinal = aOrdinal,
			},
			{ });
		expect(reserved.has_value(), "append ordinal reserves");
		auto record = *reserved;
		expect(
			record.ReservedOrdinal == aOrdinal
				&& record.Phase
					== app::epoch_catalog_operation_phase::preparing,
			"reservation returns exact ordinal in preparing phase");

		auto forbiddenTransition = app::operation_transition_request{
			.Catalog                   = aSession.binding().Catalog,
			.Operation                 = record.Context.Operation,
			.Kind                      = app::epoch_catalog_operation_kind::append,
			.Expected                  = app::epoch_catalog_operation_phase::preparing,
			.Next                      = app::epoch_catalog_operation_phase::spool_recorded,
			.Evidence                  = app::epoch_catalog_transition_evidence::catalog_transaction,
			.ExpectedOperationRevision = record.OperationRevision,
		};
		expect(
			!app::valid(forbiddenTransition),
			"fact-bearing spool edge has no generic transition request");
		expect_failure(
			aSession.transition_operation(forbiddenTransition, { }),
			app::epoch_catalog_failure::invalid_request,
			"adapter rejects free fact-bearing spool transition");

		if (aProveAtomicSpool) {
			expect(
				aInspectionRoot != nullptr,
				"atomic spool proof has a direct database path");
			const auto invalidStorage = spool_request(record, aSeed, true);
			const auto failedStorage  = aSession.record_spool(invalidStorage, { });
			expect_failure(
				failedStorage,
				app::epoch_catalog_failure::invalid_request,
				"non-durable inventory fact is rejected before SQL");
			auto unchanged = aSession.recovery_page(std::nullopt, 1U, { });
			expect(
				unchanged && unchanged->Operations.size() == 1U,
				"rejected spool leaves one recoverable reservation");
			const auto& unchangedAppend = std::get<app::append_operation_record>(
				unchanged->Operations.front());
			expect(
				unchangedAppend.Phase
						== app::epoch_catalog_operation_phase::preparing
					&& !unchangedAppend.Spool && !unchangedAppend.Preimport,
				"rejected spool leaves phase and facts unchanged");
			expect(
				direct_count(
					database_path(*aInspectionRoot),
					"SELECT count(*) FROM append_receipt")
						== 0
					&& direct_count(
						   database_path(*aInspectionRoot),
						   "SELECT count(*) FROM append_inventory")
						== 0,
				"rejected spool leaves no partial on-disk receipt or inventory");
		}

		if (aOptions.PersistReceiving) {
			const auto receiving = aSession.record_receiving({
																 .Catalog                   = aSession.binding().Catalog,
																 .Operation                 = record.Context.Operation,
																 .ExpectedOperationRevision = record.OperationRevision,
																 .Receiving                 = { .Operation    = record.Context.Operation,
																								.Identity     = spool_request(record, aSeed).Spool.Spool.Identity,
																								.MaximumBytes = 4096U },
															 },
															 { });
			expect(receiving.has_value(), "complete append persists receiving ownership before payload receipt");
			record = *receiving;
		}
		auto spool            = spool_request(record, aSeed);
		spool.Spool.Residency = aOptions.Residency;
		auto spooled          = aSession.record_spool(spool, { });
		expect(spooled.has_value(), "spool and inventory record atomically");
		record = *spooled;
		expect(
			record.Phase == app::epoch_catalog_operation_phase::spool_recorded
				&& record.Spool == spool.Spool
				&& record.Preimport == spool.Preimport,
			"spool transition returns exact durable facts");
		auto idempotentSpool = aSession.record_spool(spool, { });
		expect(
			idempotentSpool && *idempotentSpool == record,
			"exact spool retry is idempotent");

		auto install              = install_request(record, aSeed);
		install.Install.Residency = aOptions.Residency;
		if (aOptions.Residency == app::git_revision_residency::sparse_commit_trees) {
			install.Install.HeadLineage = core::head_lineage::unknown;
		}
		if (aOptions.DerivedComparisonBase) {
			install.DerivedComparisonBase = aOptions.DerivedComparisonBase;
			install.Install.Base          = *aOptions.DerivedComparisonBase;
		}
		auto installed = aSession.record_install(install, { });
		expect(installed.has_value(), "installed artifacts record atomically");
		record = *installed;
		expect(
			record.Phase == app::epoch_catalog_operation_phase::imported
				&& record.Install == install.Install,
			"install facts and imported phase agree");
		auto idempotentInstall = aSession.record_install(install, { });
		expect(
			idempotentInstall && *idempotentInstall == record,
			"exact install retry is idempotent");

		record = transition_append(
			aSession,
			aSession.binding().Catalog,
			record,
			app::epoch_catalog_operation_phase::staging_ready,
			app::epoch_catalog_transition_evidence::exact_staging_pair);
		const auto staged            = record;
		auto       idempotentStaging = aSession.transition_operation(
			app::operation_transition_request{
				.Catalog                   = aSession.binding().Catalog,
				.Operation                 = staged.Context.Operation,
				.Kind                      = app::epoch_catalog_operation_kind::append,
				.Expected                  = app::epoch_catalog_operation_phase::imported,
				.Next                      = app::epoch_catalog_operation_phase::staging_ready,
				.Evidence                  = app::epoch_catalog_transition_evidence::exact_staging_pair,
				.ExpectedOperationRevision = app::epoch_catalog_operation_revision{
					staged.OperationRevision.value() - 1 },
			},
			{ });
		expect(
			idempotentStaging.has_value(),
			"exact phase CAS retry is idempotent");

		auto mutation = candidate(
			record, static_cast<std::int64_t>(1'000 + aOrdinal.value()), aInitial);
		if (aOptions.Cause) {
			mutation.Epoch.Cause = *aOptions.Cause;
		}
		auto candidateRequest = app::record_candidate_request{
			.Catalog                   = aSession.binding().Catalog,
			.Operation                 = record.Context.Operation,
			.ExpectedOperationRevision = record.OperationRevision,
			.Candidate                 = mutation,
			.HistoricalOccurrence      = aOptions.HistoricalOccurrence,
		};
		auto recordedCandidate = aSession.record_candidate(candidateRequest, { });
		expect(recordedCandidate.has_value(), "candidate records atomically");
		record = *recordedCandidate;
		expect(
			record.Phase
					== app::epoch_catalog_operation_phase::commit_authorized
				&& record.Candidate == mutation,
			"candidate and commit authorization agree");
		auto idempotentCandidate = aSession.record_candidate(candidateRequest, { });
		expect(
			idempotentCandidate && *idempotentCandidate == record,
			"exact candidate retry is idempotent");

		record = transition_append(
			aSession,
			aSession.binding().Catalog,
			record,
			app::epoch_catalog_operation_phase::ref_commit_in_progress,
			app::epoch_catalog_transition_evidence::catalog_transaction);
		record = transition_append(
			aSession,
			aSession.binding().Catalog,
			record,
			app::epoch_catalog_operation_phase::refs_committed,
			app::epoch_catalog_transition_evidence::exact_final_pair);

		if (aOptions.StopBeforeFinalize) {
			return { .Mutation = mutation, .Terminal = record };
		}
		const auto refs = app::git_retained_ref_pair{
			.Scope   = aSession.binding().Authority.Scope,
			.Ordinal = aOrdinal,
			.Base    = mutation.Epoch.Base,
			.Head    = mutation.Epoch.Head,
		};
		const auto finalizeRequest = app::finalize_append_request{
			.Catalog                   = aSession.binding().Catalog,
			.Operation                 = record.Context.Operation,
			.ExpectedOperationRevision = record.OperationRevision,
			.Mutation                  = mutation,
			.ObservedRefs              = refs,
			.HistoricalOccurrence      = aOptions.HistoricalOccurrence,
		};
		auto finalized = aSession.finalize_append(finalizeRequest, { });
		expect(finalized.has_value(), "visible epoch finalizes atomically");
		expect(
			finalized->Changed && finalized->InsertedEpoch == mutation.Epoch.Id
				&& finalized->Catalog == aSession.binding().Catalog,
			"catalog commit reports exact inserted epoch and new revision");
		auto finalizedRetry = aSession.finalize_append(finalizeRequest, { });
		expect(
			finalizedRetry && *finalizedRetry == *finalized,
			"exact append finalizer replay returns the original commit");

		auto recovery = aSession.recovery_page(std::nullopt, 100U, { });
		expect(recovery.has_value(), "post-finalize recovery page loads");
		std::optional<app::append_operation_record> cleanupRecord;
		for (const auto& value : recovery->Operations) {
			const auto* append = std::get_if<app::append_operation_record>(&value);
			if (append != nullptr
				&& append->Context.Operation == record.Context.Operation) {
				cleanupRecord = *append;
			}
		}
		expect(
			cleanupRecord.has_value()
				&& cleanupRecord->Phase
					== app::epoch_catalog_operation_phase::
						visible_cleanup_pending,
			"finalize leaves one typed cleanup recovery record");
		const auto cleanupTransition = app::operation_transition_request{
			.Catalog                   = aSession.binding().Catalog,
			.Operation                 = cleanupRecord->Context.Operation,
			.Kind                      = app::epoch_catalog_operation_kind::append,
			.Expected                  = app::epoch_catalog_operation_phase::visible_cleanup_pending,
			.Next                      = app::epoch_catalog_operation_phase::complete,
			.Evidence                  = app::epoch_catalog_transition_evidence::exact_cleanup_complete,
			.ExpectedOperationRevision = cleanupRecord->OperationRevision,
		};
		if (aInspectionRoot != nullptr) {
			expect(
				direct_count(
					database_path(*aInspectionRoot),
					"SELECT count(*) FROM append_receipt")
						== 1
					&& direct_count(
						   database_path(*aInspectionRoot),
						   "SELECT count(*) FROM append_inventory")
						== 1,
				"visible cleanup retains one receipt and inventory row");
		}
		record = append_record(
			aSession.transition_operation(cleanupTransition, { }),
			"append cleanup reaches terminal");
		expect(
			record.Phase == app::epoch_catalog_operation_phase::complete
				&& record.RecoveryInputCompacted && !record.Spool
				&& !record.Preimport && record.Install && record.Candidate,
			"append cleanup compacts only terminal recovery input");
		auto cleanupRetry = aSession.transition_operation(cleanupTransition, { });
		expect(
			cleanupRetry
				&& std::get<app::append_operation_record>(*cleanupRetry) == record
				&& aSession.binding().Catalog == finalized->Catalog,
			"exact append cleanup replay does not advance either revision again");
		if (aInspectionRoot != nullptr) {
			expect(
				direct_count(
					database_path(*aInspectionRoot),
					"SELECT count(*) FROM append_receipt")
						== 0
					&& direct_count(
						   database_path(*aInspectionRoot),
						   "SELECT count(*) FROM append_inventory")
						== 0
					&& direct_count(
						   database_path(*aInspectionRoot),
						   "SELECT recovery_input_compacted FROM operation "
						   "WHERE phase = 9")
						== 1,
				"complete append deletes receipt/inventory rows and marks compaction");
		}

		return {
			.Mutation = std::move(mutation),
			.Commit   = *finalized,
			.Terminal = std::move(record),
		};
	}

	[[nodiscard]] auto complete_cache_delete(
		app::epoch_catalog_session&     aSession,
		const core::epoch_operation_id& aOperationId)
		-> app::cache_delete_impact
	{
		auto impact = aSession.cache_delete_impact(
			aSession.binding().Authority.Cache, { });
		expect(impact.has_value(), "cache-delete fixture impact loads");
		auto begun = aSession.begin_cache_delete(
			app::begin_cache_delete_request{
				.Operation = aOperationId,
				.Impact    = *impact,
			},
			{ });
		expect(begun.has_value(), "cache-delete fixture seals exact intent");
		auto renaming = aSession.transition_operation(
			app::operation_transition_request{
				.Catalog   = aSession.binding().Catalog,
				.Operation = aOperationId,
				.Kind      = app::epoch_catalog_operation_kind::cache_delete,
				.Expected  = app::epoch_catalog_operation_phase::
					cache_delete_intent_sealed,
				.Next = app::epoch_catalog_operation_phase::
					quarantine_rename_in_progress,
				.Evidence                  = app::epoch_catalog_transition_evidence::catalog_transaction,
				.ExpectedOperationRevision = begun->OperationRevision,
			},
			{ });
		expect(renaming.has_value(), "cache-delete fixture starts quarantine");
		const auto& renamingRecord = std::get<app::maintenance_operation_record>(*renaming);
		auto        quarantined    = aSession.transition_operation(
			app::operation_transition_request{
				.Catalog   = aSession.binding().Catalog,
				.Operation = aOperationId,
				.Kind      = app::epoch_catalog_operation_kind::cache_delete,
				.Expected  = app::epoch_catalog_operation_phase::
					quarantine_rename_in_progress,
				.Next                      = app::epoch_catalog_operation_phase::quarantined,
				.Evidence                  = app::epoch_catalog_transition_evidence::exact_quarantine,
				.ExpectedOperationRevision = renamingRecord.OperationRevision,
			},
			{ });
		expect(quarantined.has_value(), "cache-delete fixture quarantines");
		const auto& quarantinedRecord = std::get<app::maintenance_operation_record>(*quarantined);
		auto        finalized         = aSession.finalize_cache_delete(
			app::finalize_cache_delete_request{
				.Transition = app::operation_transition_request{
					.Catalog   = aSession.binding().Catalog,
					.Operation = aOperationId,
					.Kind      = app::epoch_catalog_operation_kind::cache_delete,
					.Expected  = app::epoch_catalog_operation_phase::quarantined,
					.Next      = app::epoch_catalog_operation_phase::
						catalog_tombstoned_cleanup_pending,
					.Evidence = app::epoch_catalog_transition_evidence::
						catalog_transaction,
					.ExpectedOperationRevision = quarantinedRecord.OperationRevision,
				},
			},
			{ });
		expect(finalized.has_value(), "cache-delete fixture tombstones catalog");
		auto page = aSession.recovery_page(std::nullopt, 100U, { });
		expect(page.has_value(), "cache-delete cleanup page loads");
		std::optional<app::maintenance_operation_record> cleanup;
		for (const auto& value : page->Operations) {
			const auto* maintenance = std::get_if<app::maintenance_operation_record>(&value);
			if (maintenance != nullptr && maintenance->Operation == aOperationId) {
				cleanup = *maintenance;
			}
		}
		expect(
			cleanup
				&& cleanup->Phase
					== app::epoch_catalog_operation_phase::
						catalog_tombstoned_cleanup_pending,
			"cache-delete fixture retains exact cleanup journal");
		auto completed = aSession.transition_operation(
			app::operation_transition_request{
				.Catalog   = aSession.binding().Catalog,
				.Operation = aOperationId,
				.Kind      = app::epoch_catalog_operation_kind::cache_delete,
				.Expected  = app::epoch_catalog_operation_phase::
					catalog_tombstoned_cleanup_pending,
				.Next                      = app::epoch_catalog_operation_phase::complete,
				.Evidence                  = app::epoch_catalog_transition_evidence::exact_cleanup_complete,
				.ExpectedOperationRevision = cleanup->OperationRevision,
			},
			{ });
		expect(completed.has_value(), "cache-delete fixture cleanup completes");
		return *impact;
	}

	[[nodiscard]] auto anchor_identity(
		const core::review_epoch_id&       aEpoch,
		const core::comparison_anchor_kind aKind)
		-> core::comparison_anchor_identity
	{
		return {
			.Authority   = aEpoch.Authority,
			.Incarnation = aEpoch.Incarnation,
			.Kind        = aKind,
			.Epoch       = aEpoch.Ordinal,
		};
	}

	[[nodiscard]] auto review_association(
		const std::uint8_t          aSeed,
		const core::git_commit_oid& aCommit)
		-> core::catalog_review_association
	{
		return {
			.Association = opaque<core::review_association_id>(aSeed),
			.Review      = core::github_review_id{ 0xFA00000000000000ULL + aSeed },
			.Actor       = authority_key().Viewer,
			.Order       = core::github_review_order{
				static_cast<std::uint16_t>(aSeed) },
			.State        = core::github_review_state::approved,
			.Commit       = aCommit,
			.Availability = core::anchor_availability::available,
		};
	}

	class direct_database final
	{
	public:
		explicit direct_database(
			const std::filesystem::path& aPath,
			const bool                   aWritable = false)
		{
			const auto native = test_platform::sqlite_path(aPath);
			const auto access = aWritable ? SQLITE_OPEN_READWRITE
										  : SQLITE_OPEN_READONLY;
			expect(
				::sqlite3_open_v2(
					native.c_str(),
					&Handle,
					static_cast<int>(static_cast<unsigned>(access) | static_cast<unsigned>(SQLITE_OPEN_FULLMUTEX) | static_cast<unsigned>(SQLITE_OPEN_NOFOLLOW)),
					test_platform::sqlite_vfs())
						== SQLITE_OK
					&& Handle != nullptr,
				"behavior database opens for direct contract inspection");
		}

		~direct_database()
		{
			// [[BPR-1121]]. A destructor must not throw, so a failure here exits.
			const bpr::tests::exit_on_failure inDestructor;
			expect(
				::sqlite3_close_v2(Handle) == SQLITE_OK,
				"behavior inspection database closes");
		}

		[[nodiscard]] auto integer(const std::string_view aSql) const
			-> std::int64_t
		{
			const auto      query     = std::string{ aSql };
			::sqlite3_stmt* statement = nullptr;
			const auto      prepared  = ::sqlite3_prepare_v3(
				Handle,
				query.c_str(),
				static_cast<int>(query.size()),
				SQLITE_PREPARE_NO_VTAB,
				&statement,
				nullptr);
			if (prepared != SQLITE_OK) {
				std::cerr << "inspection SQL did not prepare: " << query << " ("
						  << ::sqlite3_errmsg(Handle) << ")\n";
			}
			expect(prepared == SQLITE_OK, "behavior inspection query prepares");
			expect(
				::sqlite3_step(statement) == SQLITE_ROW
					&& ::sqlite3_column_type(statement, 0) == SQLITE_INTEGER,
				"behavior inspection returns integer");
			const auto value = ::sqlite3_column_int64(statement, 0);
			expect(
				::sqlite3_step(statement) == SQLITE_DONE,
				"behavior inspection returns one row");
			expect(
				::sqlite3_finalize(statement) == SQLITE_OK,
				"behavior inspection query finalizes");
			return value;
		}

		[[nodiscard]] auto logical_fingerprint(const bool aLegacyRowsOnly = false) const -> std::string
		{
			const auto quoteIdentifier = [](const std::string_view aIdentifier) {
				auto quoted = std::string{ "\"" };
				for (const auto character : aIdentifier) {
					quoted.push_back(character);
					if (character == '"') {
						quoted.push_back(character);
					}
				}
				quoted.push_back('"');
				return quoted;
			};
			const auto quoteText = [](const std::string_view aValue) {
				auto quoted = std::string{ "'" };
				for (const auto character : aValue) {
					quoted.push_back(character);
					if (character == '\'') {
						quoted.push_back(character);
					}
				}
				quoted.push_back('\'');
				return quoted;
			};
			const auto appendField = [](std::string&           aOutput,
										const std::string_view aValue) {
				aOutput.append(std::to_string(aValue.size()));
				aOutput.push_back(':');
				aOutput.append(aValue);
			};
			const auto collectFirstTextColumn = [this](
													const std::string& aSql) {
				::sqlite3_stmt* statement = nullptr;
				expect(
					::sqlite3_prepare_v3(
						Handle,
						aSql.c_str(),
						static_cast<int>(aSql.size()),
						0,
						&statement,
						nullptr)
						== SQLITE_OK,
					"logical fingerprint query prepares");
				auto values = std::vector<std::string>{ };
				auto code   = ::sqlite3_step(statement);
				while (code == SQLITE_ROW) {
					const auto* raw  = ::sqlite3_column_text(statement, 0);
					const auto  size = ::sqlite3_column_bytes(statement, 0);
					expect(
						raw != nullptr && size >= 0,
						"logical fingerprint value is text");
					values.emplace_back(
						reinterpret_cast<const char*>(raw),
						static_cast<std::size_t>(size));
					code = ::sqlite3_step(statement);
				}
				expect(
					code == SQLITE_DONE
						&& ::sqlite3_finalize(statement) == SQLITE_OK,
					"logical fingerprint query completes");
				return values;
			};

			const auto tables = collectFirstTextColumn(
				"SELECT name FROM sqlite_schema WHERE type='table' "
				"AND name NOT LIKE 'sqlite_%' ORDER BY name");
			auto output = std::string{ };
			for (const auto& table : tables) {
				if (aLegacyRowsOnly && (table == "catalog_meta" || table == "append_residency" || table == "append_occurrence" || table == "epoch_occurrence" || table == "epoch_occurrence_history" || table == "epoch_occurrence_supersession")) {
					continue;
				}
				appendField(output, table);
				const auto quotedTable = quoteIdentifier(table);
				const auto columns     = collectFirstTextColumn(
					"SELECT name FROM pragma_table_info(" + quoteText(table)
					+ ") ORDER BY cid");
				expect(!columns.empty(), "logical table has declared columns");

				auto query = std::string{ "SELECT " };
				for (std::size_t index = 0U; index < columns.size(); ++index) {
					if (index != 0U) {
						query.append(", ");
					}
					const auto quotedColumn = quoteIdentifier(columns[index]);
					query.append("typeof(");
					query.append(quotedColumn);
					query.append(") || ':' || CASE typeof(");
					query.append(quotedColumn);
					query.append(") WHEN 'text' THEN hex(CAST(");
					query.append(quotedColumn);
					query.append(" AS BLOB)) WHEN 'blob' THEN hex(");
					query.append(quotedColumn);
					query.append(") ELSE quote(");
					query.append(quotedColumn);
					query.append(") END");
					appendField(output, columns[index]);
				}
				query.append(" FROM ");
				query.append(quotedTable);
				query.append(" ORDER BY ");
				for (std::size_t index = 0U; index < columns.size(); ++index) {
					if (index != 0U) {
						query.append(", ");
					}
					query.append(quoteIdentifier(columns[index]));
				}

				::sqlite3_stmt* statement = nullptr;
				expect(
					::sqlite3_prepare_v3(
						Handle,
						query.c_str(),
						static_cast<int>(query.size()),
						SQLITE_PREPARE_NO_VTAB,
						&statement,
						nullptr)
						== SQLITE_OK,
					"logical table snapshot prepares");
				auto code = ::sqlite3_step(statement);
				while (code == SQLITE_ROW) {
					output.push_back('R');
					for (std::size_t index = 0U; index < columns.size(); ++index) {
						const auto  column = static_cast<int>(index);
						const auto* raw    = ::sqlite3_column_text(statement, column);
						const auto  size   = ::sqlite3_column_bytes(statement, column);
						expect(
							raw != nullptr && size >= 0,
							"quoted logical field is text");
						appendField(
							output,
							std::string_view{
								reinterpret_cast<const char*>(raw),
								static_cast<std::size_t>(size) });
					}
					code = ::sqlite3_step(statement);
				}
				expect(
					code == SQLITE_DONE
						&& ::sqlite3_finalize(statement) == SQLITE_OK,
					"logical table snapshot completes");
				output.push_back('E');
			}
			return output;
		}

		void execute(const std::string_view aSql) const
		{
			const auto statement = std::string{ aSql };
			char*      detail    = nullptr;
			const auto code      = ::sqlite3_exec(
				Handle, statement.c_str(), nullptr, nullptr, &detail);
			if (code != SQLITE_OK && detail != nullptr) {
				std::cerr << "direct SQL failure: " << detail << '\n';
			}
			::sqlite3_free(detail);
			expect(code == SQLITE_OK, "direct contract mutation succeeds");
		}

		[[nodiscard]] auto execute_code(const std::string_view aSql) const -> int
		{
			const auto statement = std::string{ aSql };
			char*      detail    = nullptr;
			const auto code      = ::sqlite3_exec(
				Handle, statement.c_str(), nullptr, nullptr, &detail);
			::sqlite3_free(detail);
			return code;
		}

		[[nodiscard]] auto foreign_key_violation_count() const -> std::size_t
		{
			::sqlite3_stmt*            statement = nullptr;
			constexpr std::string_view query     = "PRAGMA foreign_key_check";
			expect(
				::sqlite3_prepare_v3(
					Handle,
					query.data(),
					static_cast<int>(query.size()),
					SQLITE_PREPARE_NO_VTAB,
					&statement,
					nullptr)
					== SQLITE_OK,
				"foreign-key inspection prepares");
			auto count = std::size_t{ 0 };
			auto code  = ::sqlite3_step(statement);
			while (code == SQLITE_ROW) {
				++count;
				code = ::sqlite3_step(statement);
			}
			expect(
				code == SQLITE_DONE && ::sqlite3_finalize(statement) == SQLITE_OK,
				"foreign-key inspection completes");
			return count;
		}

		void insert_pull_request_scope_fillers(const std::size_t aCount) const
		{
			::sqlite3_stmt*            source    = nullptr;
			constexpr std::string_view sourceSql = "SELECT incarnation, cache_scope, viewer_id "
												   "FROM pull_request_scope LIMIT 1";
			expect(
				::sqlite3_prepare_v3(
					Handle,
					sourceSql.data(),
					static_cast<int>(sourceSql.size()),
					SQLITE_PREPARE_NO_VTAB,
					&source,
					nullptr)
						== SQLITE_OK
					&& ::sqlite3_step(source) == SQLITE_ROW,
				"quota fixture source scope loads");
			auto       incarnation = std::array<std::uint8_t, 32U>{ };
			auto       cache       = std::array<std::uint8_t, 32U>{ };
			auto       viewer      = std::array<std::uint8_t, 8U>{ };
			const auto copyColumn  = [source](
										 const int aColumn,
										 auto&     aOutput) {
				const auto* raw = static_cast<const std::uint8_t*>(
					::sqlite3_column_blob(source, aColumn));
				expect(
					raw != nullptr
						&& ::sqlite3_column_bytes(source, aColumn)
							== static_cast<int>(aOutput.size()),
					"quota fixture source identity is exact BLOB storage");
				std::copy_n(raw, aOutput.size(), aOutput.begin());
			};
			copyColumn(0, incarnation);
			copyColumn(1, cache);
			copyColumn(2, viewer);
			expect(
				::sqlite3_step(source) == SQLITE_DONE
					&& ::sqlite3_finalize(source) == SQLITE_OK,
				"quota fixture source scope finalizes");

			execute("BEGIN IMMEDIATE");
			::sqlite3_stmt*            insert    = nullptr;
			constexpr std::string_view insertSql = "INSERT INTO pull_request_scope(incarnation, pr_scope, "
												   "cache_scope, viewer_id, access_mode, pull_request_id, pull_request_number, "
												   "next_ordinal, catalog_revision, retained_epoch_count, "
												   "latest_live_ordinal, previous_distinct_head_ordinal, lifecycle, "
												   "retention_rehydration_required) "
												   "VALUES(?1, ?2, ?3, ?4, 1, ?5, ?6, 1, 0, 0, NULL, NULL, 1, 0)";
			expect(
				::sqlite3_prepare_v3(
					Handle,
					insertSql.data(),
					static_cast<int>(insertSql.size()),
					SQLITE_PREPARE_NO_VTAB,
					&insert,
					nullptr)
					== SQLITE_OK,
				"PR-scope quota fixture insert prepares");
			const auto encode = [](const std::uint64_t aValue) {
				auto bytes = std::array<std::uint8_t, 8U>{ };
				for (std::size_t index = 0U; index < bytes.size(); ++index) {
					bytes[bytes.size() - 1U - index] = static_cast<std::uint8_t>(aValue >> (index * 8U));
				}
				return bytes;
			};
			for (std::size_t index = 1U; index <= aCount; ++index) {
				auto scope        = std::array<std::uint8_t, 32U>{ };
				scope[0]          = 0xD1U;
				const auto suffix = encode(static_cast<std::uint64_t>(index));
				std::copy(
					suffix.begin(), suffix.end(), scope.end() - suffix.size());
				const auto pullRequest = encode(0xD200000000000000ULL
												+ static_cast<std::uint64_t>(index));
				expect(
					::sqlite3_reset(insert) == SQLITE_OK
						&& ::sqlite3_clear_bindings(insert) == SQLITE_OK
						&& ::sqlite3_bind_blob(
							   insert,
							   1,
							   incarnation.data(),
							   static_cast<int>(incarnation.size()),
							   SQLITE_TRANSIENT)
							== SQLITE_OK
						&& ::sqlite3_bind_blob(
							   insert,
							   2,
							   scope.data(),
							   static_cast<int>(scope.size()),
							   SQLITE_TRANSIENT)
							== SQLITE_OK
						&& ::sqlite3_bind_blob(
							   insert,
							   3,
							   cache.data(),
							   static_cast<int>(cache.size()),
							   SQLITE_TRANSIENT)
							== SQLITE_OK
						&& ::sqlite3_bind_blob(
							   insert,
							   4,
							   viewer.data(),
							   static_cast<int>(viewer.size()),
							   SQLITE_TRANSIENT)
							== SQLITE_OK
						&& ::sqlite3_bind_blob(
							   insert,
							   5,
							   pullRequest.data(),
							   static_cast<int>(pullRequest.size()),
							   SQLITE_TRANSIENT)
							== SQLITE_OK
						&& ::sqlite3_bind_int64(
							   insert,
							   6,
							   100'000 + static_cast<std::int64_t>(index))
							== SQLITE_OK
						&& ::sqlite3_step(insert) == SQLITE_DONE,
					"PR-scope quota fixture row inserts");
			}
			expect(
				::sqlite3_finalize(insert) == SQLITE_OK,
				"PR-scope quota fixture insert finalizes");
			execute("COMMIT");
		}

		void fill_retained_epoch_quota(const std::size_t aTarget) const
		{
			expect(aTarget >= 1U, "retained quota fixture has a source epoch");
			const auto maximum = std::to_string(aTarget);
			execute("PRAGMA synchronous = OFF");
			execute("BEGIN IMMEDIATE");
			execute(
				std::string{
					"WITH RECURSIVE sequence(ordinal) AS ("
					"SELECT 2 UNION ALL SELECT ordinal + 1 FROM sequence "
					"WHERE ordinal < " }
				+ maximum
				+ "), template AS (SELECT * FROM epoch WHERE ordinal = 1) "
				  "INSERT INTO epoch(incarnation, pr_scope, ordinal, "
				  "source_observation, source_snapshot, identity_generation, "
				  "pull_request_generation, base_branch, observed_at, "
				  "origin, base_transition, "
				  "head_transition, material_relation, primary_cause, "
				  "material_base_algorithm, material_base_digest, "
				  "material_head_algorithm, material_head_digest, "
				  "material_policy_revision) "
				  "SELECT template.incarnation, template.pr_scope, sequence.ordinal, "
				  "unhex(printf('%064x', 0x4000000000000000 + sequence.ordinal)), "
				  "unhex(printf('%016x', 0x4100000000000000 + sequence.ordinal)), "
				  "unhex(printf('%016x', 0x4200000000000000 + sequence.ordinal)), "
				  "unhex(printf('%016x', 0x4300000000000000 + sequence.ordinal)), "
				  "template.base_branch, template.observed_at, template.origin, "
				  "template.base_transition, template.head_transition, "
				  "template.material_relation, template.primary_cause, "
				  "template.material_base_algorithm, template.material_base_digest, "
				  "template.material_head_algorithm, template.material_head_digest, "
				  "template.material_policy_revision FROM sequence CROSS JOIN template");
			execute(
				std::string{
					"WITH RECURSIVE sequence(ordinal) AS ("
					"SELECT 2 UNION ALL SELECT ordinal + 1 FROM sequence "
					"WHERE ordinal < " }
				+ maximum
				+ "), template AS (SELECT * FROM epoch_anchor WHERE ordinal = 1) "
				  "INSERT INTO epoch_anchor(incarnation, pr_scope, ordinal, "
				  "endpoint, object_algorithm, object_digest, ref_format, "
				  "availability) SELECT template.incarnation, template.pr_scope, "
				  "sequence.ordinal, template.endpoint, template.object_algorithm, "
				  "template.object_digest, template.ref_format, "
				  "template.availability FROM sequence CROSS JOIN template");
			execute(
				"UPDATE pull_request_scope SET "
				"retained_epoch_count = (SELECT count(*) FROM epoch_anchor a "
				"WHERE a.incarnation = pull_request_scope.incarnation "
				"AND a.pr_scope = pull_request_scope.pr_scope "
				"AND a.endpoint = 1 AND a.availability NOT IN (3, 4)), "
				"next_ordinal = (SELECT max(ordinal) + 1 FROM epoch e "
				"WHERE e.incarnation = pull_request_scope.incarnation "
				"AND e.pr_scope = pull_request_scope.pr_scope)");
			execute("COMMIT");
			execute("PRAGMA synchronous = FULL");
		}

	private:
		::sqlite3* Handle = nullptr;
	};

	[[nodiscard]] auto direct_count(
		const std::filesystem::path& aPath,
		const std::string_view       aSql) -> std::int64_t
	{
		const direct_database database{ aPath };
		return database.integer(aSql);
	}

	[[nodiscard]] auto catalog_logical_fingerprint(
		const std::filesystem::path& aPath) -> std::string
	{
		const direct_database database{ aPath };
		return database.logical_fingerprint();
	}

	[[nodiscard]] auto read_binary(const std::filesystem::path& aPath)
		-> std::string
	{
		auto input = std::ifstream{ aPath, std::ios::binary };
		expect(input.good(), "behavior database bytes are readable");
		return {
			std::istreambuf_iterator<char>{ input },
			std::istreambuf_iterator<char>{ }
		};
	}

	void expect_recovery_failure(
		const std::filesystem::path&     aRoot,
		const std::uint64_t              aApplicationSession,
		const app::epoch_catalog_failure aFailure,
		const std::string_view           aMessage)
	{
		auto made = sqlite::make_epoch_catalog({ .StorageRoot = aRoot });
		expect(made.has_value(), "corrupt-row fixture passes structural bootstrap");
		auto port   = std::move(*made);
		auto opened = port->open(open_request(aApplicationSession), { });
		expect(opened.has_value(), "corrupt-row authority reopens");
		auto session = std::move(*opened);
		expect_failure(
			session->recovery_page(std::nullopt, 100U, { }), aFailure, aMessage);
	}

	void historical_occurrence_atomic_recovery_contract()
	{
		temporary_root root{ "historical-atomic" };
		auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "historical catalogue opens");
		auto port   = std::move(*made);
		auto opened = port->open(open_request(951U), { });
		expect(opened.has_value(), "historical scope opens");
		auto session = std::move(*opened);
		(void)complete_append(*session, core::epoch_ordinal{ 1 }, 1U, core::application_session_id{ 951U }, true);
		(void)complete_append(*session, core::epoch_ordinal{ 2 }, 2U, core::application_session_id{ 951U }, false);
		const auto live       = session->snapshot({ });
		auto       historical = revision(3U);
		historical.Origin     = core::pull_request_revision_origin::explicit_historical_import;
		core::epoch_occurrence occurrence{
			.Evidence            = { .Kind = core::epoch_occurrence_kind::force_push_event, .EventId = 951U, .Head = historical.Head },
			.DateSource          = core::epoch_occurrence_date_source::github_event,
			.DateUtcMilliseconds = 100,
			.FirstSeen           = core::observed_utc_milliseconds{ 900 },
		};
		occurrence.Id  = *core::make_epoch_occurrence_id("ghe.example.com", authority_key().PullRequest, occurrence.Evidence);
		auto duplicate = occurrence;
		duplicate.Evidence.EventId++;
		duplicate.Id = *core::make_epoch_occurrence_id("ghe.example.com", authority_key().PullRequest, duplicate.Evidence);
		expect(session->record_occurrences({ .Occurrences = { occurrence, duplicate } }, { }).has_value(), "same-head historical events persist independently");
		const auto derived = oid<core::git_commit_oid>(91U);
		const auto pending = complete_append(*session, core::epoch_ordinal{ 3 }, 3U, core::application_session_id{ 951U }, false, false, nullptr,
											 { .Accepted = historical, .Cause = core::epoch_cause{ .Origin = core::epoch_origin::explicit_historical_import, .Primary = core::epoch_primary_cause::explicit_historical_import }, .HistoricalOccurrence = occurrence.Id, .DerivedComparisonBase = derived, .StopBeforeFinalize = true });
		expect(pending.Terminal.Context.Revision.Base == derived, "derived base becomes exact recovery ref base");
		expect(direct_count(database_path(root.path()), "SELECT count(*) FROM epoch") == 2
				   && direct_count(database_path(root.path()), "SELECT count(*) FROM epoch_occurrence WHERE local_ordinal IS NOT NULL") == 0,
			   "pre-finalize interruption exposes neither epoch nor occurrence binding");
		session.reset();
		port.reset();
		made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "pending historical catalogue reopens");
		port   = std::move(*made);
		opened = port->open(open_request(952U), { });
		expect(opened.has_value(), "historical scope recovers");
		session             = std::move(*opened);
		const auto recovery = session->recovery_page(std::nullopt, 100U, { });
		expect(recovery && recovery->Operations.size() == 1U, "historical journal survives restart");
		const auto& record = std::get<app::append_operation_record>(recovery->Operations.front());
		expect(record.Candidate == pending.Mutation && record.HistoricalOccurrence == occurrence.Id,
			   "recovery carries exact event instead of inferring same-head equivalence");
		const app::finalize_append_request request{
			.Catalog                   = record.Context.Catalog,
			.Operation                 = record.Context.Operation,
			.ExpectedOperationRevision = record.OperationRevision,
			.Mutation                  = *record.Candidate,
			.ObservedRefs              = { .Scope = record.Candidate->Epoch.Id.Authority, .Ordinal = core::epoch_ordinal{ 3 }, .Base = derived, .Head = historical.Head },
			.HistoricalOccurrence      = record.HistoricalOccurrence,
		};
		auto committed = session->finalize_append(request, { });
		expect(committed.has_value() && session->finalize_append(request, { }) == committed, "historical finalization replays idempotently");
		const auto stored = session->occurrences({ });
		expect(stored.has_value(), "bound history reads");
		const auto bound = std::ranges::find(stored->Occurrences, occurrence.Id, &core::epoch_occurrence::Id);
		const auto other = std::ranges::find(stored->Occurrences, duplicate.Id, &core::epoch_occurrence::Id);
		expect(bound != stored->Occurrences.end() && bound->LocalEpoch == pending.Mutation.Epoch.Id
				   && !bound->HistoricalBase && bound->DerivedComparisonBase == derived
				   && other != stored->Occurrences.end() && !other->LocalEpoch,
			   "only exact occurrence binds atomically and unknown original base stays unknown");
		const auto after = session->snapshot({ });
		expect(after && live && after->LatestLive == live->LatestLive && after->PreviousDistinctHead == live->PreviousDistinctHead,
			   "historical finalization leaves both live ordinal pointers unchanged");
	}

	void base_rewrite_association_recovery_contract(const bool aMigrate)
	{
		temporary_root root{ "base-rewrite-recovery" };
		auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "base rewrite catalogue opens");
		auto port   = std::move(*made);
		auto opened = port->open(open_request(961U), { });
		expect(opened.has_value(), "base rewrite scope opens");
		auto       session    = std::move(*opened);
		const auto live       = complete_append(*session, core::epoch_ordinal{ 1 }, 1U, core::application_session_id{ 961U }, true);
		auto       historical = revision(2U);
		historical.Origin     = core::pull_request_revision_origin::explicit_historical_import;
		core::epoch_occurrence event{
			.Evidence            = { .Kind = core::epoch_occurrence_kind::force_push_event, .EventId = 961U, .Head = historical.Head },
			.DateSource          = core::epoch_occurrence_date_source::github_event,
			.DateUtcMilliseconds = 100,
			.FirstSeen           = core::observed_utc_milliseconds{ 900 },
		};
		event.Id = *core::make_epoch_occurrence_id("ghe.example.com", authority_key().PullRequest, event.Evidence);
		expect(session->record_occurrences({ .Occurrences = { event } }, { }).has_value(), "unbound event persists");
		const auto imported = complete_append(*session, core::epoch_ordinal{ 2 }, 2U, core::application_session_id{ 961U }, false, false, nullptr,
											  { .Accepted = historical, .Cause = core::epoch_cause{ .Origin = core::epoch_origin::explicit_historical_import, .Primary = core::epoch_primary_cause::explicit_historical_import }, .HistoricalOccurrence = event.Id, .DerivedComparisonBase = oid<core::git_commit_oid>(91U) });
		const auto bound    = session->occurrences({ });
		expect(bound && bound->Occurrences.front().LocalEpoch == imported.Mutation.Epoch.Id, "journaled import establishes the comparison");
		auto changed       = revision(3U);
		changed.Head       = live.Mutation.Epoch.Head;
		changed.Base       = oid<core::git_commit_oid>(99U);
		const auto pending = complete_append(*session, core::epoch_ordinal{ 3 }, 3U, core::application_session_id{ 961U }, false, false, nullptr,
											 { .Accepted = changed, .StopBeforeFinalize = true });
		const auto binding = session->binding();
		const auto before  = session->snapshot({ });
		expect(session->occurrences({ }) == bound, "pre-finalize interruption has not modified the old association");
		session.reset();
		port.reset();
		std::string oldRows;
		if (aMigrate) {
			direct_database database{ database_path(root.path()), true };
			database.execute("BEGIN IMMEDIATE; PRAGMA defer_foreign_keys=ON; DROP TABLE append_residency; CREATE TABLE old_meta AS SELECT * FROM catalog_meta; DROP TABLE catalog_meta; CREATE TABLE catalog_meta(singleton INTEGER NOT NULL PRIMARY KEY CHECK(singleton = 1), schema_format INTEGER NOT NULL CHECK(schema_format = 5), incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32))) STRICT, WITHOUT ROWID; CREATE UNIQUE INDEX catalog_meta_incarnation ON catalog_meta(incarnation); INSERT INTO catalog_meta SELECT singleton, 5, incarnation FROM old_meta; DROP TABLE old_meta; PRAGMA user_version=5; COMMIT");
			oldRows = database.logical_fingerprint(true);
		}
		if (aMigrate) {
			sqlite::detail::testing::interrupt_next_migration_before_commit();
			expect(!sqlite::make_epoch_catalog({ .StorageRoot = root.path() }), "interrupted v5 migration refuses publication");
			const direct_database database{ database_path(root.path()) };
			expect(database.integer("PRAGMA user_version") == 5 && database.logical_fingerprint(true) == oldRows
					   && database.integer("SELECT count(*) FROM epoch_occurrence WHERE local_ordinal IS NOT NULL") == 1,
				   "migration rollback preserves old bindings and every epoch and journal row");
		}
		made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "base rewrite catalogue reopens or migrates");
		port   = std::move(*made);
		opened = port->open(open_request(962U), { });
		expect(opened.has_value(), "base rewrite authority reopens");
		session = std::move(*opened);
		expect(session->binding() == binding && session->snapshot({ }) == before,
			   "migration and restart retain incarnation, catalog revision and live selectors");
		if (aMigrate) {
			const direct_database database{ database_path(root.path()) };
			expect(database.integer("PRAGMA user_version") == 7 && database.logical_fingerprint(true) == oldRows,
				   "v6 migration changes no old epoch, anchor, or pending journal row");
			const auto unlinked = session->occurrences({ });
			expect(unlinked && unlinked->Occurrences == std::vector{ event }, "unversioned v5 comparison association is unavailable after migration");
		}
		const auto recovery = session->recovery_page(std::nullopt, 100U, { });
		expect(recovery && recovery->Operations.size() == 1U, "one pending live append survives restart");
		const auto& record = std::get<app::append_operation_record>(recovery->Operations.front());
		expect(record == pending.Terminal, "pending base replacement retains its exact recovery identity");
		const app::finalize_append_request request{
			.Catalog                   = record.Context.Catalog,
			.Operation                 = record.Context.Operation,
			.ExpectedOperationRevision = record.OperationRevision,
			.Mutation                  = *record.Candidate,
			.ObservedRefs              = { .Scope = record.Candidate->Epoch.Id.Authority, .Ordinal = record.Candidate->Epoch.Id.Ordinal, .Base = changed.Base, .Head = changed.Head },
		};
		if (!aMigrate) {
			auto mismatched              = request;
			mismatched.ObservedRefs.Base = live.Mutation.Epoch.Base;
			expect(!session->finalize_append(mismatched, { }), "recovery refuses a mismatched retained base pair");
			expect(session->occurrences({ }) == bound && session->snapshot({ }) == before,
				   "refused recovery leaves comparison association and live pointer unchanged");
		}
		const auto committed = session->finalize_append(request, { });
		expect(committed && session->finalize_append(request, { }) == committed, "recovered live replacement finalizes and replays atomically");
		const auto unlinked = session->occurrences({ });
		expect(unlinked && unlinked->Occurrences == std::vector{ event }, "base replacement unlinks only derived comparison material");
		expect(!session->record_occurrences(*bound, { }) && session->occurrences({ }) == unlinked,
			   "stale timeline reconciliation cannot resurrect the old imported comparison");
		const auto page = session->history({ .Catalog = session->binding().Catalog, .Limit = 3U }, { });
		expect(page && page->Epochs == std::vector{ pending.Mutation.Epoch, imported.Mutation.Epoch, live.Mutation.Epoch },
			   "all old complete epoch rows and anchors remain independently resolvable");
		session.reset();
		port.reset();
		made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "completed base replacement restarts");
		port   = std::move(*made);
		opened = port->open(open_request(963U), { });
		expect(opened && (*opened)->occurrences({ }) == unlinked, "restart cannot restore invalidated comparison associations");
	}

	void portable_occurrence_storage_contract()
	{
		temporary_root root{ "occurrences" };
		auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "occurrence catalogue opens");
		auto port   = std::move(*made);
		auto opened = port->open(open_request(901U), { });
		expect(opened.has_value(), "occurrence authority opens");
		auto       session    = std::move(*opened);
		const auto local      = complete_append(*session, core::epoch_ordinal{ 1 }, 1U, core::application_session_id{ 901U }, true);
		const auto snapshot   = session->snapshot({ });
		const auto unresolved = session->occurrences({ });
		expect(unresolved && unresolved->UnresolvedLegacy && unresolved->Incomplete && unresolved->Occurrences.empty(),
			   "a head snapshot without transition evidence remains unresolved and receives no fabricated portable key");
		core::epoch_occurrence first{
			.Evidence            = { .Kind = core::epoch_occurrence_kind::force_push_event, .EventId = 101U, .Head = local.Mutation.Epoch.Head },
			.LocalEpoch          = local.Mutation.Epoch.Id,
			.DateSource          = core::epoch_occurrence_date_source::github_event,
			.DateUtcMilliseconds = 100,
			.FirstSeen           = core::observed_utc_milliseconds{ 900 },
		};
		first.Id                = *core::make_epoch_occurrence_id("ghe.example.com", authority_key().PullRequest, first.Evidence);
		auto second             = first;
		second.Evidence.EventId = 102U;
		second.Id               = *core::make_epoch_occurrence_id("ghe.example.com", authority_key().PullRequest, second.Evidence);
		second.LocalEpoch.reset();
		second.DerivedComparisonBase = local.Mutation.Epoch.Base;
		expect(!session->record_occurrences({ .Occurrences = { second } }, { }), "timeline cannot persist an unversioned derived base without a journaled comparison");
		second.DerivedComparisonBase.reset();
		core::epoch_occurrence_history history{ .Occurrences = { second, first }, .Incomplete = false };
		expect(session->record_occurrences(history, { }).has_value(), "same-head event occurrences store independently");
		auto stored = session->occurrences({ });
		expect(stored && stored->Occurrences.size() == 2U && !stored->UnresolvedLegacy
				   && !stored->Occurrences.front().HistoricalBase && session->snapshot({ }) == snapshot,
			   "sidecar preserves local catalogue identity, revision, ordinals and unknown historical bases");
		expect(std::ranges::is_sorted(stored->Occurrences, core::epoch_occurrence_less), "stored occurrences return deterministic date/key order");
		const auto original = *stored;
		auto       bad      = first;
		bad.Evidence.Head   = oid<core::git_commit_oid>(222U);
		expect_failure(session->record_occurrences({ .Occurrences = { bad } }, { }), app::epoch_catalog_failure::invalid_request,
					   "an existing event cannot silently acquire another after-head");
		auto wrongBinding       = second;
		wrongBinding.LocalEpoch = first.LocalEpoch;
		expect(!session->record_occurrences({ .Occurrences = { wrongBinding } }, { }), "two occurrences cannot acquire one local epoch");
		expect(session->occurrences({ }) == stored, "a refused batch leaves every occurrence and status unchanged");
		std::stop_source cancelled;
		cancelled.request_stop();
		expect_failure(session->record_occurrences(history, cancelled.get_token()), app::epoch_catalog_failure::cancelled,
					   "cancelled occurrence write commits nothing");
		first.FirstSeen = core::observed_utc_milliseconds{ 999 };
		expect(session->record_occurrences({ .Occurrences = { first }, .Incomplete = true, .Ambiguous = true }, { }).has_value(),
			   "partial refresh merges without deleting earlier evidence");
		stored = session->occurrences({ });
		expect(stored && stored->Occurrences == original.Occurrences && stored->Incomplete && stored->Ambiguous,
			   "partial read preserves old rows and original local first-seen while recording gaps");
		session.reset();
		port.reset();
		made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "occurrence catalogue restarts");
		port   = std::move(*made);
		opened = port->open(open_request(902U), { });
		expect(opened && (*opened)->occurrences({ }) == stored, "portable evidence, dates, bases and partial status survive restart");
	}

	void occurrence_supersession_contract()
	{
		for (const bool targetExists : { false, true }) {
			temporary_root root{ targetExists ? "supersede-existing" : "supersede-new" };
			auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(made.has_value(), "supersession catalogue opens");
			auto port   = std::move(*made);
			auto opened = port->open(open_request(931U), { });
			expect(opened.has_value(), "supersession scope opens");
			auto                   session  = std::move(*opened);
			const auto             local    = complete_append(*session, core::epoch_ordinal{ 1 }, 1U, core::application_session_id{ 931U }, true);
			const auto             snapshot = session->snapshot({ });
			core::epoch_occurrence observed{
				.Evidence            = { .Kind = core::epoch_occurrence_kind::observed_head, .Head = local.Mutation.Epoch.Head },
				.LocalEpoch          = local.Mutation.Epoch.Id,
				.HistoricalBase      = local.Mutation.Epoch.Base,
				.DateSource          = core::epoch_occurrence_date_source::commit_date,
				.DateUtcMilliseconds = 100,
				.FirstSeen           = core::observed_utc_milliseconds{ 900 }
			};
			observed.Id               = *core::make_epoch_occurrence_id("github.com", authority_key().PullRequest, observed.Evidence);
			auto event                = observed;
			event.Evidence.Kind       = core::epoch_occurrence_kind::force_push_event;
			event.Evidence.EventId    = 771U;
			event.Id                  = *core::make_epoch_occurrence_id("github.com", authority_key().PullRequest, event.Evidence);
			event.DateSource          = core::epoch_occurrence_date_source::github_event;
			event.DateUtcMilliseconds = 200;
			expect(session->record_occurrences({ .Occurrences = { observed } }, { }).has_value(), "observed occurrence persists first");
			if (targetExists) {
				auto unbound = event;
				unbound.LocalEpoch.reset();
				expect(session->record_occurrences({ .Occurrences = { unbound } }, { }).has_value(), "unbound event may already be persisted");
			}
			const auto before = catalog_logical_fingerprint(database_path(root.path()));
			auto       stale  = observed;
			stale.FirstSeen   = core::observed_utc_milliseconds{ 901 };
			expect_failure(session->supersede_occurrence(stale, event, { }), app::epoch_catalog_failure::revision_conflict,
						   "supersession compares every expected old fact");
			auto conflict          = event;
			conflict.Evidence.Head = oid<core::git_commit_oid>(222U);
			expect(!session->supersede_occurrence(observed, conflict, { }), "supersession refuses a different head");
			if (targetExists) {
				conflict                     = event;
				conflict.DateUtcMilliseconds = 201;
				expect(!session->supersede_occurrence(observed, conflict, { }), "existing event target facts cannot be overwritten by supersession");
			}
			std::stop_source cancelled;
			cancelled.request_stop();
			expect_failure(session->supersede_occurrence(observed, event, cancelled.get_token()), app::epoch_catalog_failure::cancelled,
						   "cancelled supersession preserves both identities");
			expect(catalog_logical_fingerprint(database_path(root.path())) == before, "refused supersessions change no stored fact");
			expect(session->supersede_occurrence(observed, event, { }).has_value(), "event atomically supersedes observed identity");
			const auto after = catalog_logical_fingerprint(database_path(root.path()));
			expect(session->supersede_occurrence(observed, event, { }).has_value()
					   && catalog_logical_fingerprint(database_path(root.path())) == after,
				   "identical supersession retry is idempotent");
			const auto history = session->occurrences({ });
			expect(history && history->Occurrences == std::vector<core::epoch_occurrence>{ event }
					   && session->snapshot({ }) == snapshot,
				   "active history contains only the event while local epochs and ordinals remain unchanged");
			conflict                  = event;
			conflict.Evidence.EventId = 772U;
			conflict.Id               = *core::make_epoch_occurrence_id("github.com", authority_key().PullRequest, conflict.Evidence);
			expect(!session->supersede_occurrence(observed, conflict, { }), "archived identity cannot be redirected to another event");
			conflict.LocalEpoch.reset();
			expect(!session->record_occurrences({ .Occurrences = { conflict, observed } }, { }), "stale observed upsert refuses the entire batch after a valid insertion");
			expect(catalog_logical_fingerprint(database_path(root.path())) == after, "stale resurrection failure rolls back earlier batch writes");
			{
				direct_database database{ database_path(root.path()) };
				expect(database.integer("SELECT count(*) FROM epoch_occurrence_supersession") == 1
						   && database.integer("SELECT first_seen FROM epoch_occurrence_supersession") == observed.FirstSeen.value()
						   && database.integer("SELECT date_utc FROM epoch_occurrence_supersession") == observed.DateUtcMilliseconds
						   && database.integer("SELECT local_ordinal FROM epoch_occurrence_supersession") == local.Mutation.Epoch.Id.Ordinal.value()
						   && database.foreign_key_violation_count() == 0U,
					   "archive retains original dates and local binding with intact references");
			}
			session.reset();
			port.reset();
			made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(made.has_value(), "supersession catalogue restarts");
			port   = std::move(*made);
			opened = port->open(open_request(932U), { });
			expect(opened.has_value(), "supersession scope reopens");
			session = std::move(*opened);
			expect(session->occurrences({ }) == history && session->supersede_occurrence(observed, event, { }).has_value()
					   && !session->record_occurrences({ .Occurrences = { observed } }, { }),
				   "restart preserves active event, retry identity and stale resurrection refusal");
		}
	}

	[[nodiscard]] auto migration_position(const core::review_epoch_summary& aEpoch) -> core::window_review_position
	{
		auto position            = core::window_review_position{ };
		position.Comparison.Left = {
			.Identity     = { .Authority = aEpoch.Id.Authority, .Incarnation = aEpoch.Id.Incarnation, .Kind = core::comparison_anchor_kind::epoch_base, .Epoch = aEpoch.Id.Ordinal },
			.Commit       = aEpoch.Base,
			.Availability = core::anchor_availability::available
		};
		position.Comparison.Right               = position.Comparison.Left;
		position.Comparison.Right.Identity.Kind = core::comparison_anchor_kind::epoch_head;
		position.Comparison.Right.Commit        = aEpoch.Head;
		return position;
	}

	[[nodiscard]] auto migration_ledger(const std::filesystem::path& aRoot)
	{
		return sqlite::make_review_ledger_store({ .StorageRoot = aRoot, .Host = "ghe.example.test", .Account = "fixture", .Repository = "fixture/repository", .PullRequest = 4242 });
	}

	void verify_migrated_consumers(const std::filesystem::path& aRoot,
								   app::epoch_catalog_session& aSession, const app::window_review_snapshot& aExpected,
								   const core::agent_session& aAgent)
	{
		auto       restoration = sqlite::make_window_review_restoration_store(aRoot);
		const auto loaded      = restoration->load({ });
		expect(loaded && *loaded == aExpected, "copied restoration retains every original exact epoch position");
		for (const auto& record : loaded->Records) {
			const auto& pair    = record.Position->Comparison;
			const auto  anchors = aSession.exact_anchors({ .Catalog = aSession.binding().Catalog,
														   .Anchors = { pair.Left.Identity, pair.Right.Identity } },
														 { });
			expect(anchors && anchors->Anchors == std::vector{ pair.Left, pair.Right },
				   "persisted restoration identities resolve to original available commits after migration");
		}
		auto ledger = migration_ledger(aRoot);
		expect(ledger.has_value(), "copied review ledger opens after catalogue migration");
		const auto retained = (*ledger)->load_agent_session_catalog();
		expect(retained && retained->Latest == aAgent, "ledger retains original agent binding and epoch ordinals");
		const auto& binding = retained->Latest->Binding;
		auto        left    = aExpected.Records.front().Position->Comparison.Left.Identity;
		auto        right   = aExpected.Records.back().Position->Comparison.Right.Identity;
		left.Epoch          = binding.LeftEpoch;
		right.Epoch         = binding.RightEpoch;
		const auto anchors  = aSession.exact_anchors({ .Catalog = aSession.binding().Catalog, .Anchors = { left, right } }, { });
		expect(anchors && anchors->Anchors.size() == 2U
				   && anchors->Anchors[0].Commit == binding.BaseOid && anchors->Anchors[1].Commit == binding.HeadOid,
			   "ledger agent binding resolves its original compared material without portable identity rebinding");
	}

	[[nodiscard]] auto legacy_ghes_authority() -> app::epoch_catalog_authority_key
	{
		const app::github_profile_authority profile{
			.Profile = core::host_profile_id{ 11U }, .ProfileRevision = app::github_profile_revision{ 21U }, .Host = core::git_host_id{ 3U }, .Hostname = app::github_canonical_hostname{ "ghe.example.test" }, .ServerContract = app::github_server_contract::ghes_3_18_2022_11_28, .Authentication = app::github_authentication_transport::native_https, .ProbeContractRevision = app::github_probe_contract_revision{ 1U }
		};
		expect(app::valid_github_authority_hostname(profile.ServerContract, profile.Hostname),
			   "legacy fixture originates from an explicit valid synthetic GHES profile");
		// Frozen v1 host-authority format from github_window_read_host.cpp.
		// This constructs synthetic old-host input, not a captured installation.
		std::vector<std::uint8_t> bytes;
		const auto                appendInteger = [&](const std::uint64_t aValue) {
			for (std::size_t shift = 8U; shift != 0U; --shift) {
				bytes.push_back(static_cast<std::uint8_t>(aValue >> ((shift - 1U) * 8U)));
			}
		};
		const auto appendText = [&](const std::string_view aText) {
			appendInteger(aText.size());
			bytes.insert(bytes.end(), aText.begin(), aText.end());
		};
		appendText("BetterPullReview/github-host-authority/v1");
		appendInteger(profile.Profile.value());
		appendInteger(profile.ProfileRevision.value());
		appendInteger(profile.Host.value());
		appendText(profile.Hostname.value());
		appendInteger(static_cast<std::uint8_t>(profile.ServerContract));
		appendInteger(static_cast<std::uint8_t>(profile.Authentication));
		appendInteger(profile.ProbeContractRevision.value());
		auto authority            = authority_key(6U, 7U);
		authority.Profile         = profile.Profile;
		authority.ProfileRevision = core::github_profile_revision{ profile.ProfileRevision.value() };
		authority.Host            = profile.Host;
		authority.HostAuthority   = core::host_authority_fingerprint{ core::sha256(bytes) };
		expect(hex_identity(authority.HostAuthority) == "796B30AF1230709324616CA2230E53C97DBB7EF5886C9588AC514778EF109FEB",
			   "legacy opaque fingerprint is pinned to the synthetic GHES hostname and server contract");
		return authority;
	}

	void lossless_occurrence_migration_contract(const bool aVersionFour)
	{
		temporary_root originalRoot{ "occurrence-v3-original" };
		temporary_root copiedRoot{ "occurrence-v3-copy" };
		const auto     authority = legacy_ghes_authority();
		const auto     request   = open_request(911U, authority);
		auto           made      = sqlite::make_epoch_catalog({ .StorageRoot = originalRoot.path() });
		expect(made.has_value(), "migration fixture opens");
		auto port   = std::move(*made);
		auto opened = port->open(request, { });
		expect(opened.has_value(), "migration fixture authority opens");
		auto       session                         = std::move(*opened);
		const auto first                           = complete_append(*session, core::epoch_ordinal{ 1 }, 1U, request.ApplicationSession, true);
		const auto second                          = complete_append(*session, core::epoch_ordinal{ 2 }, 2U, request.ApplicationSession, false);
		auto       returning                       = revision(3U);
		returning.Head                             = first.Mutation.Epoch.Head;
		const auto                        third    = complete_append(*session, core::epoch_ordinal{ 3 }, 3U, request.ApplicationSession, false,
																	 false, nullptr, { .Accepted = returning });
		const auto                        binding  = session->binding();
		const auto                        snapshot = session->snapshot({ });
		const app::window_review_snapshot positions{
			.Revision = 2U,
			.Records  = {
				{ core::bpr_window_id{ 1U }, { core::github_access_mode::authenticated, "ghe.example.test", { "fixture", "repository" }, request.Authority.Number }, migration_position(first.Mutation.Epoch) },
				{ core::bpr_window_id{ 2U }, { core::github_access_mode::authenticated, "ghe.example.test", { "fixture", "repository" }, request.Authority.Number }, migration_position(second.Mutation.Epoch) },
				{ core::bpr_window_id{ 3U }, { core::github_access_mode::authenticated, "ghe.example.test", { "fixture", "repository" }, request.Authority.Number }, migration_position(third.Mutation.Epoch) } }
		};
		core::agent_session savedAgent;
		{
			auto restoration = sqlite::make_window_review_restoration_store(originalRoot.path());
			expect(restoration->load({ }).has_value() && restoration->save(positions, 1U, { }) == positions,
				   "legacy consumer positions persist before copying");
			auto ledger = migration_ledger(originalRoot.path());
			expect(ledger.has_value(), "legacy ledger opens before copying");
			const auto admitted = (*ledger)->admit_agent_session({ .Profile = core::agent_profile_id{ 1U },
																   .Binding = { .Host = request.Authority.Host, .Repository = request.Authority.Repository, .PullRequest = request.Authority.PullRequest, .BaseOid = first.Mutation.Epoch.Base, .HeadOid = third.Mutation.Epoch.Head, .LeftEpoch = first.Mutation.Epoch.Id.Ordinal, .RightEpoch = third.Mutation.Epoch.Id.Ordinal } });
			expect(admitted.has_value(), "legacy agent binding persists before copying");
			savedAgent = *admitted;
		}
		session.reset();
		port.reset();
		std::string oldRows;
		{
			direct_database database{ database_path(originalRoot.path()), true };
			expect(database.integer("SELECT count(*) FROM epoch_occurrence") == 0
					   && database.integer("SELECT count(*) FROM append_occurrence") == 0,
				   "old-host fixture never wrote occurrence evidence; conversion discards none");
			expect(database.integer("SELECT count(*) FROM sqlite_schema WHERE name='repository_scope' AND (instr(sql, 'hostname') > 0 OR instr(sql, 'server_contract') > 0)") == 0,
				   "legacy catalogue has no recoverable hostname or server-contract column");
			expect(aVersionFour ? test_platform::make_version_four_catalog(database) : test_platform::make_version_three_catalog(database), "fixture restores exact previous production schema");
			oldRows = database.logical_fingerprint(true);
		}
		std::filesystem::copy(originalRoot.path(), copiedRoot.path(), std::filesystem::copy_options::recursive);
#if defined(_WIN32)
		// [[BPR-1049]]. On Windows every entry a copy creates inherits its
		// parent's entries, and the stores refuse an inherited descriptor, so
		// the copy would be refused before any migration ran. Each copied entry
		// is narrowed as the root was. POSIX copies keep each entry's own mode.
		for (const auto& entry : std::filesystem::recursive_directory_iterator{ copiedRoot.path() }) {
			expect(test_platform::make_owner_private(entry.path(), entry.is_directory()),
				   "each copied entry becomes owner-private");
		}
#endif
		made = sqlite::make_epoch_catalog({ .StorageRoot = copiedRoot.path() });
		expect(made.has_value(), "a copy of the previous file migrates to v6");
		port   = std::move(*made);
		opened = port->open(request, { });
		expect(opened.has_value(), "migrated scope opens");
		session = std::move(*opened);
		expect(session->binding() == binding && session->snapshot({ }) == snapshot,
			   "migration preserves incarnation, authority, next ordinal and both live selectors");
		verify_migrated_consumers(copiedRoot.path(), *session, positions, savedAgent);
		const auto history = session->occurrences({ });
		expect(history && history->Occurrences.empty() && history->UnresolvedLegacy && history->Incomplete,
			   "legacy duplicate heads and absent hostname provenance stay unresolved");
		const auto page = session->history({ .Catalog = binding.Catalog, .Limit = 3U }, { });
		expect(page && page->Epochs == std::vector{ third.Mutation.Epoch, second.Mutation.Epoch, first.Mutation.Epoch }
				   && !page->Continuation,
			   "migration preserves each complete old epoch row, including duplicate-head identities and observed bases");
		for (const auto& prior : { first, second, third }) {
			const core::comparison_anchor_identity head{ .Authority   = prior.Mutation.Epoch.Id.Authority,
														 .Incarnation = prior.Mutation.Epoch.Id.Incarnation,
														 .Kind        = core::comparison_anchor_kind::epoch_head,
														 .Epoch       = prior.Mutation.Epoch.Id.Ordinal };
			auto                                   base = head;
			base.Kind                                   = core::comparison_anchor_kind::epoch_base;
			const auto anchors                          = session->exact_anchors({ .Catalog = binding.Catalog, .Anchors = { base, head } }, { });
			expect(anchors && anchors->Anchors.size() == 2U
					   && anchors->Anchors[0].Commit == prior.Mutation.Epoch.Base
					   && anchors->Anchors[1].Commit == prior.Mutation.Epoch.Head,
				   "both old local anchor identities still resolve their exact original commits");
		}
		session.reset();
		port.reset();
		made = sqlite::make_epoch_catalog({ .StorageRoot = copiedRoot.path() });
		expect(made.has_value(), "migrated catalogue restarts without a second migration");
		port   = std::move(*made);
		opened = port->open(request, { });
		expect(opened.has_value(), "migrated authority reopens after restart");
		session = std::move(*opened);
		expect(session->binding() == binding && session->snapshot({ }) == snapshot
				   && session->occurrences({ }) == history,
			   "restart preserves old binding, live selectors and explicitly unresolved provenance");
		verify_migrated_consumers(copiedRoot.path(), *session, positions, savedAgent);
		{
			direct_database database{ database_path(copiedRoot.path()) };
			expect(database.logical_fingerprint(true) == oldRows && database.foreign_key_violation_count() == 0U,
				   "every original non-metadata table row survives byte-for-byte, including operations and retained reference components");
		}
		{
			direct_database database{ database_path(originalRoot.path()) };
			expect(database.integer("PRAGMA user_version") == (aVersionFour ? 4 : 3) && database.logical_fingerprint(true) == oldRows,
				   "migration of the copied file does not touch its original");
		}
	}

	void append_snapshot_history_anchor_contract()
	{
		temporary_root root{ "append" };
		auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "append fixture catalog bootstraps");
		auto           port               = std::move(*made);
		constexpr auto applicationSession = 0xF900000000000001ULL;
		auto           opened             = port->open(open_request(applicationSession), { });
		expect(opened.has_value(), "append fixture authority opens");
		auto session = std::move(*opened);

		const auto first = complete_append(
			*session,
			core::epoch_ordinal{ 1 },
			1U,
			core::application_session_id{ applicationSession },
			true,
			true,
			&root.path());
		{
			direct_database database{ database_path(root.path()) };
			expect(
				database.integer("SELECT count(*) FROM append_receipt") == 0
					&& database.integer(
						   "SELECT count(*) FROM append_inventory")
						== 0
					&& database.integer(
						   "SELECT recovery_input_compacted FROM operation "
						   "WHERE phase = 9")
						== 1,
				"terminal append compacts receipt and inventory recovery input");
			expect(
				database.integer("SELECT count(*) FROM epoch") == 1
					&& database.integer(
						   "SELECT count(*) FROM epoch_anchor")
						== 2,
				"epoch and its two anchors become visible together");
		}

		const auto second = complete_append(
			*session,
			core::epoch_ordinal{ 2 },
			2U,
			core::application_session_id{ applicationSession },
			false);
		const auto third = complete_append(
			*session,
			core::epoch_ordinal{ 3 },
			3U,
			core::application_session_id{ applicationSession },
			false);

		auto snapshot = session->snapshot({ });
		expect(snapshot.has_value(), "catalog snapshot loads");
		expect(
			snapshot->Binding == session->binding()
				&& snapshot->LatestLive == third.Mutation.Epoch
				&& snapshot->PreviousDistinctHead == second.Mutation.Epoch,
			"snapshot binds latest and previous distinct head to one revision");

		const auto historyRevision = session->binding().Catalog;
		auto       page            = session->history(
			app::epoch_history_page_request{
				.Catalog = historyRevision,
				.Limit   = 2U,
			},
			{ });
		expect(page.has_value(), "first bounded history page loads");
		expect(
			page->Catalog == historyRevision && page->Epochs.size() == 2U
				&& page->Epochs[0] == third.Mutation.Epoch
				&& page->Epochs[1] == second.Mutation.Epoch
				&& page->Continuation.has_value(),
			"history is newest-first and revision-bound");
		const auto savedCursor = page->Continuation;
		auto       nextPage    = session->history(
			app::epoch_history_page_request{
				.Catalog = historyRevision,
				.Cursor  = page->Continuation,
				.Limit   = 2U,
			},
			{ });
		expect(nextPage.has_value(), "history continuation loads");
		expect(
			nextPage->Epochs.size() == 1U
				&& nextPage->Epochs.front() == first.Mutation.Epoch
				&& !nextPage->Continuation,
			"history continuation has no duplicates or omissions");

		const auto firstBase = anchor_identity(
			first.Mutation.Epoch.Id, core::comparison_anchor_kind::epoch_base);
		const auto firstHead = anchor_identity(
			first.Mutation.Epoch.Id, core::comparison_anchor_kind::epoch_head);
		auto anchors = session->exact_anchors(
			app::exact_anchor_lookup_request{
				.Catalog = session->binding().Catalog,
				.Anchors = { firstBase, firstHead },
			},
			{ });
		expect(anchors.has_value(), "exact anchor pair loads");
		expect(
			anchors->Anchors.size() == 2U
				&& anchors->Anchors[0].Identity == firstBase
				&& anchors->Anchors[0].Commit == first.Mutation.Epoch.Base
				&& anchors->Anchors[1].Identity == firstHead
				&& anchors->Anchors[1].Commit == first.Mutation.Epoch.Head,
			"exact anchors preserve stable semantic identity and OIDs");

		const auto review     = review_association(20U, second.Mutation.Epoch.Head);
		auto       associated = session->associate_review(
			app::associate_review_request{
				.Catalog          = session->binding().Catalog,
				.Review           = review,
				.AcceptedSnapshot = core::provider_snapshot_id{
					0xFC00000000000001ULL },
			},
			{ });
		expect(associated.has_value(), "new review association commits");
		expect(
			associated->Changed
				&& associated->Catalog.Revision.value()
					== historyRevision.Revision.value() + 1,
			"changed review association advances catalog revision");
		auto associatedRetry = session->associate_review(
			app::associate_review_request{
				.Catalog          = session->binding().Catalog,
				.Review           = review,
				.AcceptedSnapshot = core::provider_snapshot_id{
					0xFC00000000000001ULL },
			},
			{ });
		expect(
			associatedRetry.has_value() && !associatedRetry->Changed
				&& associatedRetry->Catalog == associated->Catalog,
			"exact review association retry is idempotent");

		expect_failure(
			session->history(
				app::epoch_history_page_request{
					.Catalog = session->binding().Catalog,
					.Cursor  = savedCursor,
					.Limit   = 2U,
				},
				{ }),
			app::epoch_catalog_failure::invalid_request,
			"history cursor from an older catalog revision is rejected");

		const auto reviewAnchor = core::comparison_anchor_identity{
			.Authority   = session->binding().Authority.Scope,
			.Incarnation = session->binding().Catalog.Incarnation,
			.Kind        = core::comparison_anchor_kind::review_commit,
			.Review      = review.Association,
		};
		auto exactReview = session->exact_anchors(
			app::exact_anchor_lookup_request{
				.Catalog = session->binding().Catalog,
				.Anchors = { reviewAnchor },
			},
			{ });
		expect(
			exactReview && exactReview->Anchors.size() == 1U
				&& exactReview->Anchors.front().Commit == review.Commit,
			"review anchor resolves by exact association identity");

		const auto lookupRevision = session->binding().Catalog;
		auto       missingEpoch   = firstHead;
		missingEpoch.Epoch        = core::epoch_ordinal{ 9999 };
		auto missingReview        = reviewAnchor;
		missingReview.Review      = opaque<core::review_association_id>(231U);
		for (const auto& missing : { missingEpoch, missingReview }) {
			const auto partial = session->exact_anchors({ .Catalog = lookupRevision, .Anchors = { firstBase, missing } }, { });
			expect(partial && partial->Catalog == lookupRevision && partial->Anchors.size() == 1U
					   && partial->Anchors.front() == anchors->Anchors.front()
					   && session->binding().Catalog == lookupRevision,
				   "missing epoch/review is omitted without changing present candidate or revision");
			auto wrongAuthority      = missing;
			wrongAuthority.Authority = opaque<core::pull_request_authority_scope_id>(232U);
			expect_failure(session->exact_anchors({ .Catalog = lookupRevision, .Anchors = { wrongAuthority } }, { }),
						   app::epoch_catalog_failure::authority_mismatch, "missing anchor cannot bypass authority validation");
		}

		auto corrupt = session->mark_anchor_availability(
			app::mark_anchor_availability_request{
				.Catalog = session->binding().Catalog,
				.Changes = {
					app::anchor_availability_change{
						.Anchor         = firstBase,
						.ExpectedCommit = first.Mutation.Epoch.Base,
						.Expected       = core::anchor_availability::available,
						.Next           = core::anchor_availability::corrupt,
					},
				},
			},
			{ });
		expect(
			corrupt && corrupt->Changed,
			"corrupt availability tombstone commits");
		auto corruptedAnchor = session->exact_anchors(
			app::exact_anchor_lookup_request{
				.Catalog = session->binding().Catalog,
				.Anchors = { firstBase },
			},
			{ });
		expect(
			corruptedAnchor
				&& corruptedAnchor->Anchors.front().Availability
					== core::anchor_availability::corrupt,
			"corrupt tombstone survives exact lookup");

		const auto retained = app::git_retained_ref_pair{
			.Scope   = session->binding().Authority.Scope,
			.Ordinal = second.Mutation.Epoch.Id.Ordinal,
			.Base    = second.Mutation.Epoch.Base,
			.Head    = second.Mutation.Epoch.Head,
		};
		const auto pruneFact = app::prune_epoch_fact{
			.Epoch    = second.Mutation.Epoch.Id,
			.Retained = retained,
			.BaseRef  = app::epoch_catalog_ref_identity{
				.Authority = session->binding().Authority.Scope,
				.Ordinal   = second.Mutation.Epoch.Id.Ordinal,
			},
			.HeadRef = app::epoch_catalog_ref_identity{
				.Authority = session->binding().Authority.Scope,
				.Ordinal   = second.Mutation.Epoch.Id.Ordinal,
				.Head      = true,
			},
		};
		auto prune = session->begin_prune(
			app::begin_prune_request{
				.Catalog   = session->binding().Catalog,
				.Operation = operation(40U),
				.Epochs    = { pruneFact },
			},
			{ });
		expect(
			prune && prune->Kind == app::epoch_catalog_operation_kind::prune
				&& prune->Phase
					== app::epoch_catalog_operation_phase::prune_intent
				&& prune->Epochs == std::vector{ pruneFact },
			"prune begins from an exact persisted epoch/ref intent");
		auto pruneRetry = session->begin_prune(
			app::begin_prune_request{
				.Catalog   = session->binding().Catalog,
				.Operation = operation(40U),
				.Epochs    = { pruneFact },
			},
			{ });
		expect(
			pruneRetry && *pruneRetry == *prune,
			"exact prune-intent retry is idempotent");
		auto pruneStartedResult = session->transition_operation(
			app::operation_transition_request{
				.Catalog                   = session->binding().Catalog,
				.Operation                 = prune->Operation,
				.Kind                      = app::epoch_catalog_operation_kind::prune,
				.Expected                  = app::epoch_catalog_operation_phase::prune_intent,
				.Next                      = app::epoch_catalog_operation_phase::ref_delete_in_progress,
				.Evidence                  = app::epoch_catalog_transition_evidence::catalog_transaction,
				.ExpectedOperationRevision = prune->OperationRevision,
			},
			{ });
		expect(pruneStartedResult.has_value(), "prune records ref-delete start");
		const auto* pruneStarted = std::get_if<app::maintenance_operation_record>(&*pruneStartedResult);
		expect(
			pruneStarted != nullptr
				&& pruneStarted->Phase
					== app::epoch_catalog_operation_phase::
						ref_delete_in_progress,
			"prune transition returns exact maintenance record");
		const auto finalizePruneRequest = app::finalize_prune_request{
			.Transition = app::operation_transition_request{
				.Catalog   = session->binding().Catalog,
				.Operation = pruneStarted->Operation,
				.Kind      = app::epoch_catalog_operation_kind::prune,
				.Expected  = app::epoch_catalog_operation_phase::
					ref_delete_in_progress,
				.Next = app::epoch_catalog_operation_phase::
					visible_cleanup_pending,
				.Evidence = app::epoch_catalog_transition_evidence::
					exact_refs_absent,
				.ExpectedOperationRevision = pruneStarted->OperationRevision,
			},
			.ObservedAbsent = { retained },
		};
		auto pruned = session->finalize_prune(finalizePruneRequest, { });
		expect(
			pruned && pruned->Changed
				&& pruned->Catalog == session->binding().Catalog,
			"exact absent refs atomically tombstone pruned anchors");
		auto prunedRetry = session->finalize_prune(finalizePruneRequest, { });
		expect(
			prunedRetry && *prunedRetry == *pruned,
			"exact prune finalizer replay returns the original commit");
		auto prunedAnchors = session->exact_anchors(
			app::exact_anchor_lookup_request{
				.Catalog = session->binding().Catalog,
				.Anchors = {
					anchor_identity(
						second.Mutation.Epoch.Id,
						core::comparison_anchor_kind::epoch_base),
					anchor_identity(
						second.Mutation.Epoch.Id,
						core::comparison_anchor_kind::epoch_head),
				},
			},
			{ });
		expect(
			prunedAnchors && prunedAnchors->Anchors.size() == 2U
				&& prunedAnchors->Anchors[0].Availability
					== core::anchor_availability::pruned
				&& prunedAnchors->Anchors[1].Availability
					== core::anchor_availability::pruned,
			"both exact anchors retain identity with pruned availability");
		auto prunedReview = session->exact_anchors(
			app::exact_anchor_lookup_request{
				.Catalog = session->binding().Catalog,
				.Anchors = { reviewAnchor },
			},
			{ });
		auto prunedSnapshot = session->snapshot({ });
		expect(
			prunedReview
				&& prunedReview->Anchors.front().Availability
					== core::anchor_availability::pruned
				&& prunedSnapshot && prunedSnapshot->CurrentViewerReview
				&& prunedSnapshot->CurrentViewerReview->Association
					== review.Association
				&& prunedSnapshot->CurrentViewerReview->Availability
					== core::anchor_availability::pruned,
			"pruning a linked epoch tombstones its review anchor and current pointer");
		auto pruneCleanupPage = session->recovery_page(std::nullopt, 100U, { });
		expect(pruneCleanupPage.has_value(), "prune cleanup is recoverable");
		std::optional<app::maintenance_operation_record> pruneCleanup;
		for (const auto& value : pruneCleanupPage->Operations) {
			const auto* maintenance = std::get_if<app::maintenance_operation_record>(&value);
			if (maintenance != nullptr && maintenance->Operation == operation(40U)) {
				pruneCleanup = *maintenance;
			}
		}
		expect(
			pruneCleanup
				&& pruneCleanup->Phase
					== app::epoch_catalog_operation_phase::
						visible_cleanup_pending,
			"prune finalization retains bounded cleanup record");
		auto pruneComplete = session->transition_operation(
			app::operation_transition_request{
				.Catalog   = session->binding().Catalog,
				.Operation = pruneCleanup->Operation,
				.Kind      = app::epoch_catalog_operation_kind::prune,
				.Expected  = app::epoch_catalog_operation_phase::
					visible_cleanup_pending,
				.Next                      = app::epoch_catalog_operation_phase::complete,
				.Evidence                  = app::epoch_catalog_transition_evidence::exact_cleanup_complete,
				.ExpectedOperationRevision = pruneCleanup->OperationRevision,
			},
			{ });
		expect(pruneComplete.has_value(), "prune cleanup reaches terminal");

		std::stop_source cancelled;
		cancelled.request_stop();
		expect_failure(
			session->history(
				app::epoch_history_page_request{
					.Catalog = session->binding().Catalog,
					.Limit   = 1U,
				},
				cancelled.get_token()),
			app::epoch_catalog_failure::cancelled,
			"pre-cancelled bounded history returns cancellation");

		const auto currentBinding = session->binding();
		session.reset();
		port.reset();
		auto raw = read_binary(database_path(root.path()));
		expect(
			raw.find("BPR_TOKEN_CANARY") == std::string::npos,
			"opaque receipt and inventory tokens never enter the main database");
		const auto wal = test_platform::with_ascii_suffix(
			database_path(root.path()), "-wal");
		if (std::filesystem::exists(wal)) {
			raw = read_binary(wal);
			expect(
				raw.find("BPR_TOKEN_CANARY") == std::string::npos,
				"opaque receipt and inventory tokens never enter the WAL");
		}

		auto reopenedPortResult = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(reopenedPortResult.has_value(), "populated WAL catalog reopens");
		auto reopenedPort   = std::move(*reopenedPortResult);
		auto reopenedResult = reopenedPort->open(open_request(0xF900000000000002ULL), { });
		expect(reopenedResult.has_value(), "populated authority reopens");
		auto reopened = std::move(*reopenedResult);
		expect(
			reopened->binding() == currentBinding,
			"populated catalog revision and scopes survive WAL reopen");
		auto reopenedSnapshot = reopened->snapshot({ });
		expect(
			reopenedSnapshot
				&& reopenedSnapshot->LatestLive == third.Mutation.Epoch
				&& reopenedSnapshot->CurrentViewerReview
				&& reopenedSnapshot->CurrentViewerReview->Association
					== review.Association
				&& reopenedSnapshot->CurrentViewerReview->Availability
					== core::anchor_availability::pruned,
			"epoch and pruned review tombstone survive WAL reopen");
	}

	void reservation_conflict_and_never_reuse_contract()
	{
		temporary_root root{ "writers" };
		auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "two-writer fixture bootstraps");
		auto port       = std::move(*made);
		auto firstOpen  = port->open(open_request(101U), { });
		auto secondOpen = port->open(open_request(102U), { });
		expect(
			firstOpen.has_value() && secondOpen.has_value(),
			"two independent sessions open one authority");
		auto first         = std::move(*firstOpen);
		auto second        = std::move(*secondOpen);
		auto freshSnapshot = first->snapshot({ });
		expect(
			freshSnapshot
				&& freshSnapshot->NextOrdinal == core::epoch_ordinal{ 1 },
			"fresh catalog exposes ordinal one as the exact next reservation");
		const auto accepted     = revision(30U);
		auto       unusedSource = first->source_observation_exists(
			accepted.Source.Observation, { });
		expect(
			unusedSource && !*unusedSource,
			"indexed source probe reports a fresh observation as unused");
		expect_failure(
			first->source_observation_exists({ }, { }),
			app::epoch_catalog_failure::invalid_request,
			"source probe rejects the reserved zero identity");
		const auto firstRequest = app::append_reservation_request{
			.Context = context(
				first->binding(),
				operation(30U),
				accepted,
				30U,
				core::application_session_id{ 101U }),
			.ProposedOrdinal = core::epoch_ordinal{ 1 },
		};
		const auto secondRequest = app::append_reservation_request{
			.Context = context(
				second->binding(),
				operation(31U),
				revision(31U),
				31U,
				core::application_session_id{ 102U }),
			.ProposedOrdinal = core::epoch_ordinal{ 1 },
		};

		std::barrier ready{ 3 };
		std::optional<app::epoch_catalog_result<app::append_operation_record>>
			firstResult;
		std::optional<app::epoch_catalog_result<app::append_operation_record>>
			 secondResult;
		auto firstThread = std::thread{ [&] {
			ready.arrive_and_wait();
			firstResult.emplace(first->reserve_append(firstRequest, { }));
		} };
		auto secondThread = std::thread{ [&] {
			ready.arrive_and_wait();
			secondResult.emplace(second->reserve_append(secondRequest, { }));
		} };
		ready.arrive_and_wait();
		firstThread.join();
		secondThread.join();

		expect(
			firstResult.has_value() && secondResult.has_value(),
			"both serialized writer attempts return");
		const auto successes = static_cast<int>(firstResult->has_value())
			+ static_cast<int>(secondResult->has_value());
		expect(successes == 1, "exactly one writer reserves ordinal one");
		const auto& failed = firstResult->has_value() ? *secondResult
													  : *firstResult;
		expect_failure(
			failed,
			app::epoch_catalog_failure::revision_conflict,
			"competing writer receives typed revision conflict");

		auto*       winner          = firstResult->has_value() ? first.get() : second.get();
		const auto  winnerSession   = firstResult->has_value()
			? core::application_session_id{ 101U }
			: core::application_session_id{ 102U };
		auto        winnerRecord    = firstResult->has_value() ? **firstResult
															   : **secondResult;
		const auto  winningRevision = winnerRecord.Context.Revision;
		const auto& winnerRequest   = firstResult->has_value() ? firstRequest
															   : secondRequest;
		auto        exactRetry      = winner->reserve_append(winnerRequest, { });
		expect(
			exactRetry && *exactRetry == winnerRecord,
			"exact operation reservation retry remains idempotent");
		auto durableSource = winner->source_observation_exists(
			winningRevision.Source.Observation, { });
		expect(
			durableSource && *durableSource,
			"indexed source probe includes an unpublished durable append");
		std::stop_source cancelledProbe;
		cancelledProbe.request_stop();
		expect_failure(
			winner->source_observation_exists(
				winningRevision.Source.Observation,
				cancelledProbe.get_token()),
			app::epoch_catalog_failure::cancelled,
			"pre-cancelled source probe returns cancellation");

		const auto siblingAuthority = authority_key(
			authority_key().Repository.value(),
			0xF2000000000000A7ULL,
			4342U);
		auto siblingOpen = port->open(open_request(103U, siblingAuthority), { });
		expect(
			siblingOpen.has_value(),
			"collision fixture opens a sibling PR in the catalog incarnation");
		auto sibling          = std::move(*siblingOpen);
		auto siblingCollision = sibling->reserve_append(
			app::append_reservation_request{
				.Context = context(
					sibling->binding(),
					operation(34U),
					winningRevision,
					34U,
					core::application_session_id{ 103U }),
				.ProposedOrdinal = core::epoch_ordinal{ 1 },
			},
			{ });
		expect_failure(
			siblingCollision,
			app::epoch_catalog_failure::source_observation_conflict,
			"source collision crosses PR scopes within one catalog incarnation");
		auto operationCollision = sibling->reserve_append(
			app::append_reservation_request{
				.Context = context(
					sibling->binding(),
					winnerRecord.Context.Operation,
					revision(35U),
					35U,
					core::application_session_id{ 103U }),
				.ProposedOrdinal = core::epoch_ordinal{ 1 },
			},
			{ });
		expect_failure(
			operationCollision,
			app::epoch_catalog_failure::operation_identity_conflict,
			"operation identity collision is distinct across PR scopes");
		auto reservedSnapshot = winner->snapshot({ });
		expect(
			reservedSnapshot
				&& reservedSnapshot->NextOrdinal == core::epoch_ordinal{ 2 },
			"reservation advances the durable next ordinal before publication");
		auto spooled = winner->record_spool(
			spool_request(winnerRecord, 30U), { });
		expect(spooled.has_value(), "aborted-operation spool facts record");
		winnerRecord        = *spooled;
		auto abortRequested = transition_append(
			*winner,
			winner->binding().Catalog,
			winnerRecord,
			app::epoch_catalog_operation_phase::abort_requested,
			app::epoch_catalog_transition_evidence::catalog_transaction);
		const auto abortTransition = app::operation_transition_request{
			.Catalog                   = winner->binding().Catalog,
			.Operation                 = abortRequested.Context.Operation,
			.Kind                      = app::epoch_catalog_operation_kind::append,
			.Expected                  = app::epoch_catalog_operation_phase::abort_requested,
			.Next                      = app::epoch_catalog_operation_phase::aborted,
			.Evidence                  = app::epoch_catalog_transition_evidence::exact_cleanup_complete,
			.ExpectedOperationRevision = abortRequested.OperationRevision,
		};
		auto aborted = append_record(
			winner->transition_operation(abortTransition, { }),
			"append abort cleanup succeeds");
		expect(
			aborted.Phase == app::epoch_catalog_operation_phase::aborted
				&& aborted.RecoveryInputCompacted && !aborted.Spool
				&& !aborted.Preimport,
			"aborted append compacts its recovery-only receipt and inventory");
		auto abortedSourceCollision = sibling->reserve_append(
			app::append_reservation_request{
				.Context = context(
					sibling->binding(),
					operation(36U),
					winningRevision,
					36U,
					core::application_session_id{ 103U }),
				.ProposedOrdinal = core::epoch_ordinal{ 1 },
			},
			{ });
		expect_failure(
			abortedSourceCollision,
			app::epoch_catalog_failure::source_observation_conflict,
			"terminal aborted append retains its source identity collision");
		auto abortedRetry = winner->transition_operation(abortTransition, { });
		expect(
			abortedRetry
				&& std::get<app::append_operation_record>(*abortedRetry)
					== aborted
				&& direct_count(
					   database_path(root.path()),
					   "SELECT count(*) FROM append_receipt")
					== 0
				&& direct_count(
					   database_path(root.path()),
					   "SELECT count(*) FROM append_inventory")
					== 0,
			"aborted compaction first return and exact retry agree on deleted input");

		auto next = winner->reserve_append(
			app::append_reservation_request{
				.Context = context(
					winner->binding(),
					operation(32U),
					revision(32U),
					32U,
					winnerSession),
				.ProposedOrdinal = core::epoch_ordinal{ 2 },
			},
			{ });
		expect(
			next && next->ReservedOrdinal == core::epoch_ordinal{ 2 },
			"aborted ordinal is never reused");
		auto nextSnapshot = winner->snapshot({ });
		expect(
			nextSnapshot
				&& nextSnapshot->NextOrdinal == core::epoch_ordinal{ 3 },
			"aborted gap remains visible in the exact never-reused sequence");
		auto reused = winner->reserve_append(
			app::append_reservation_request{
				.Context = context(
					winner->binding(),
					operation(33U),
					revision(33U),
					33U,
					winnerSession),
				.ProposedOrdinal = core::epoch_ordinal{ 1 },
			},
			{ });
		expect_failure(
			reused,
			app::epoch_catalog_failure::revision_conflict,
			"previously reserved ordinal cannot be reused");
	}

	void canonical_review_read_contract()
	{
		temporary_root root{ "canonical-review-read" };
		auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "canonical-review fixture bootstraps");
		auto port   = std::move(*made);
		auto opened = port->open(open_request(111U), { });
		expect(opened.has_value(), "canonical-review authority opens");
		auto session = std::move(*opened);

		const auto accepted = revision(90U);
		const auto proposed = review_association(90U, accepted.Head);
		auto       provider = core::current_viewer_review_candidate{
			.Review = proposed.Review,
			.Actor  = proposed.Actor,
			.Order  = proposed.Order,
			.State  = proposed.State,
			.Commit = proposed.Commit,
		};
		auto request = app::canonical_review_request{
			.Catalog             = session->binding().Catalog,
			.Candidate           = provider,
			.ProposedAssociation = proposed.Association,
		};
		const auto before        = catalog_logical_fingerprint(database_path(root.path()));
		auto       neverObserved = session->canonical_review(request, { });
		expect(
			neverObserved
				&& neverObserved->Association == proposed.Association
				&& neverObserved->Availability
					== core::anchor_availability::never_observed,
			"unretained provider review receives proposed identity and an honest "
			"never-observed anchor state");

		request.ProjectedHead = app::projected_append_head{
			.Ordinal = core::epoch_ordinal{ 1 },
			.Head    = accepted.Head,
		};
		auto projected = session->canonical_review(request, { });
		expect(
			projected
				&& projected->Availability
					== core::anchor_availability::available,
			"matching projected append head canonicalizes review availability");
		expect(
			catalog_logical_fingerprint(database_path(root.path())) == before,
			"canonical review lookup and projected availability perform no write");

		const auto epoch = complete_append(
			*session,
			core::epoch_ordinal{ 1 },
			90U,
			core::application_session_id{ 111U },
			true,
			false,
			nullptr,
			append_fixture_options{ .Accepted = accepted });
		request.Catalog = session->binding().Catalog;
		request.ProjectedHead.reset();
		auto retained = session->canonical_review(request, { });
		expect(
			retained
				&& retained->Availability
					== core::anchor_availability::available
				&& retained->Commit == epoch.Mutation.Epoch.Head,
			"indexed retained head canonicalizes the review without history paging");

		auto associated = session->associate_review(
			app::associate_review_request{
				.Catalog          = session->binding().Catalog,
				.Review           = *retained,
				.AcceptedSnapshot = accepted.Source.Snapshot,
			},
			{ });
		expect(associated && associated->Changed, "review association persists");
		request.Catalog             = session->binding().Catalog;
		request.ProposedAssociation = opaque<core::review_association_id>(91U);
		request.Candidate.State     = core::github_review_state::dismissed;
		const auto beforeReuse      = catalog_logical_fingerprint(database_path(root.path()));
		auto       reused           = session->canonical_review(request, { });
		expect(
			reused && reused->Association == proposed.Association
				&& reused->State == core::github_review_state::dismissed
				&& reused->Availability
					== core::anchor_availability::available,
			"same provider review reuses durable local identity while projecting "
			"the newly observed provider facts");
		expect(
			catalog_logical_fingerprint(database_path(root.path()))
				== beforeReuse,
			"stable review identity reuse remains read-only");

		auto identityCollision                = request;
		identityCollision.Candidate.Review    = core::github_review_id{ 91U };
		identityCollision.Candidate.Order     = core::github_review_order{ 91U };
		identityCollision.ProposedAssociation = proposed.Association;
		expect_failure(
			session->canonical_review(identityCollision, { }),
			app::epoch_catalog_failure::operation_conflict,
			"new provider review cannot claim another review association identity");

		auto orderCollision                = identityCollision;
		orderCollision.ProposedAssociation = opaque<core::review_association_id>(92U);
		orderCollision.Candidate.Order     = proposed.Order;
		expect_failure(
			session->canonical_review(orderCollision, { }),
			app::epoch_catalog_failure::operation_conflict,
			"new provider review cannot claim another review order");

		std::stop_source cancelled;
		cancelled.request_stop();
		expect_failure(
			session->canonical_review(request, cancelled.get_token()),
			app::epoch_catalog_failure::cancelled,
			"pre-cancelled canonical review read returns cancellation");
	}

	void ordinal_exhaustion_snapshot_contract()
	{
		temporary_root root{ "ordinal-exhaustion-snapshot" };
		auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "ordinal-exhaustion fixture bootstraps");
		auto port   = std::move(*made);
		auto opened = port->open(open_request(121U), { });
		expect(opened.has_value(), "ordinal-exhaustion authority opens");
		auto session = std::move(*opened);
		session.reset();
		port.reset();
		{
			const direct_database database{ database_path(root.path()), true };
			database.execute(
				"UPDATE pull_request_scope SET next_ordinal = "
				"9223372036854775807");
		}
		made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "maximum-ordinal catalog reopens");
		port   = std::move(*made);
		opened = port->open(open_request(122U), { });
		expect(opened.has_value(), "maximum-ordinal authority reopens");
		session       = std::move(*opened);
		auto boundary = session->snapshot({ });
		expect(
			boundary
				&& boundary->NextOrdinal
					== core::epoch_ordinal{
						std::numeric_limits<std::int64_t>::max() },
			"snapshot exposes the exact final reservable ordinal");

		auto reserved = session->reserve_append(
			app::append_reservation_request{
				.Context = context(
					session->binding(),
					operation(37U),
					revision(37U),
					37U,
					core::application_session_id{ 122U }),
				.ProposedOrdinal = core::epoch_ordinal{
					std::numeric_limits<std::int64_t>::max() },
			},
			{ });
		expect(reserved.has_value(), "final ordinal reserves exactly once");
		auto exhausted = session->snapshot({ });
		expect(
			exhausted && !exhausted->NextOrdinal,
			"empty next ordinal is the explicit durable exhaustion state");
		expect_failure(
			session->reserve_append(
				app::append_reservation_request{
					.Context = context(
						session->binding(),
						operation(38U),
						revision(38U),
						38U,
						core::application_session_id{ 122U }),
					.ProposedOrdinal = core::epoch_ordinal{
						std::numeric_limits<std::int64_t>::max() },
				},
				{ }),
			app::epoch_catalog_failure::ordinal_exhausted, "exhausted catalog rejects every later ordinal reservation");
	}

	void stale_session_and_association_noop_contract()
	{
		temporary_root root{ "stale-session" };
		auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "stale-session fixture bootstraps");
		auto port      = std::move(*made);
		auto firstOpen = port->open(open_request(151U), { });
		auto staleOpen = port->open(open_request(152U), { });
		expect(
			firstOpen && staleOpen,
			"two sessions bind the same initial PR revision");
		auto       first         = std::move(*firstOpen);
		auto       stale         = std::move(*staleOpen);
		const auto staleRevision = stale->binding().Catalog;

		const auto epoch = complete_append(
			*first,
			core::epoch_ordinal{ 1 },
			45U,
			core::application_session_id{ 151U },
			true);
		expect(
			stale->binding().Catalog == staleRevision
				&& first->binding().Catalog.Revision.value()
					== staleRevision.Revision.value() + 1,
			"one session advancing the catalog leaves the other binding stale");
		expect_failure(
			stale->history(
				app::epoch_history_page_request{
					.Catalog = staleRevision,
					.Limit   = 1U,
				},
				{ }),
			app::epoch_catalog_failure::revision_conflict,
			"revision-bound history rejects a stale session");
		auto refreshed = stale->snapshot({ });
		expect(
			refreshed && refreshed->Binding == first->binding()
				&& stale->binding() == first->binding()
				&& refreshed->LatestLive == epoch.Mutation.Epoch,
			"snapshot refreshes a stale binding from one consistent read");

		const auto review           = review_association(46U, epoch.Mutation.Epoch.Head);
		const auto associateRequest = app::associate_review_request{
			.Catalog          = first->binding().Catalog,
			.Review           = review,
			.AcceptedSnapshot = core::provider_snapshot_id{
				0xFC00000000000046ULL },
		};
		auto associated = first->associate_review(associateRequest, { });
		expect(
			associated && associated->Changed,
			"first session advances the review association");
		auto staleNoop = stale->associate_review(associateRequest, { });
		expect_failure(
			staleNoop,
			app::epoch_catalog_failure::revision_conflict,
			"stale exact-association retry cannot masquerade as a no-op");
		auto reviewRefreshed = stale->snapshot({ });
		expect(
			reviewRefreshed
				&& reviewRefreshed->Binding == first->binding()
				&& reviewRefreshed->CurrentViewerReview == review,
			"stale session refresh observes the committed review and revision");
		const auto exactRetry = app::associate_review_request{
			.Catalog          = stale->binding().Catalog,
			.Review           = review,
			.AcceptedSnapshot = associateRequest.AcceptedSnapshot,
		};
		auto noChange = stale->associate_review(exactRetry, { });
		expect(
			noChange && !noChange->Changed
				&& noChange->Catalog == associated->Catalog
				&& direct_count(
					   database_path(root.path()),
					   "SELECT catalog_revision FROM pull_request_scope")
					== associated->Catalog.Revision.value(),
			"fresh exact association retry is a no-op with no second revision");
	}

	void creation_validation_atomicity_contract()
	{
		temporary_root root{ "creation-validation" };
		auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "creation-validation fixture bootstraps");
		auto port   = std::move(*made);
		auto opened = port->open(open_request(161U), { });
		expect(opened.has_value(), "creation-validation authority opens");
		auto       session = std::move(*opened);
		const auto epoch   = complete_append(
			*session,
			core::epoch_ordinal{ 1 },
			47U,
			core::application_session_id{ 161U },
			true);
		auto invalidReview         = review_association(48U, epoch.Mutation.Epoch.Head);
		invalidReview.Availability = core::anchor_availability::corrupt;
		const auto beforeRevision  = session->binding().Catalog;
		expect_failure(
			session->associate_review(
				app::associate_review_request{
					.Catalog          = beforeRevision,
					.Review           = invalidReview,
					.AcceptedSnapshot = core::provider_snapshot_id{
						0xFC00000000000048ULL },
				},
				{ }),
			app::epoch_catalog_failure::invalid_request, "new review association cannot begin as a destructive tombstone");
		expect(
			session->binding().Catalog == beforeRevision
				&& direct_count(
					   database_path(root.path()),
					   "SELECT count(*) FROM review_association")
					== 0,
			"invalid review creation leaves no row or catalog revision");

		auto reserved = session->reserve_append(
			app::append_reservation_request{
				.Context = context(
					session->binding(),
					operation(49U),
					revision(49U),
					49U,
					core::application_session_id{ 161U }),
				.ProposedOrdinal = core::epoch_ordinal{ 2 },
			},
			{ });
		expect(reserved.has_value(), "invalid-candidate append reserves");
		auto record  = *reserved;
		auto spooled = session->record_spool(spool_request(record, 49U), { });
		expect(spooled.has_value(), "invalid-candidate spool records");
		record         = *spooled;
		auto installed = session->record_install(install_request(record, 49U), { });
		expect(installed.has_value(), "invalid-candidate install records");
		record = transition_append(
			*session,
			session->binding().Catalog,
			*installed,
			app::epoch_catalog_operation_phase::staging_ready,
			app::epoch_catalog_transition_evidence::exact_staging_pair);
		auto invalidEndpoints                   = candidate(record, 2'000, false);
		invalidEndpoints.Epoch.BaseAvailability = core::anchor_availability::unavailable;
		expect_failure(
			session->record_candidate(
				app::record_candidate_request{
					.Catalog                   = session->binding().Catalog,
					.Operation                 = record.Context.Operation,
					.ExpectedOperationRevision = record.OperationRevision,
					.Candidate                 = invalidEndpoints,
				},
				{ }),
			app::epoch_catalog_failure::invalid_request,
			"append candidate requires two available endpoints");
		auto invalidCandidateReview   = candidate(record, 2'000, false);
		invalidCandidateReview.Review = invalidReview;
		expect_failure(
			session->record_candidate(
				app::record_candidate_request{
					.Catalog                   = session->binding().Catalog,
					.Operation                 = record.Context.Operation,
					.ExpectedOperationRevision = record.OperationRevision,
					.Candidate                 = invalidCandidateReview,
				},
				{ }),
			app::epoch_catalog_failure::invalid_request,
			"append candidate cannot create a tombstoned review");
		auto recovery = session->recovery_page(std::nullopt, 100U, { });
		expect(
			recovery && recovery->Operations.size() == 1U
				&& std::get<app::append_operation_record>(
					   recovery->Operations.front())
						.Phase
					== app::epoch_catalog_operation_phase::staging_ready
				&& !std::get<app::append_operation_record>(
						recovery->Operations.front())
						.Candidate
				&& direct_count(
					   database_path(root.path()),
					   "SELECT count(*) FROM append_hidden_candidate c "
					   "JOIN operation o ON o.incarnation = c.incarnation "
					   "AND o.operation = c.operation WHERE o.phase = 4")
					== 0,
			"invalid candidate variants leave phase, facts, and SQL unchanged");
	}

	void sparse_spool_restart_contract()
	{
		temporary_root root{ "sparse-spool" };
		auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "sparse spool catalog opens");
		auto port   = std::move(*made);
		auto opened = port->open(open_request(991U), { });
		expect(opened.has_value(), "sparse spool authority opens");
		auto       session  = std::move(*opened);
		const auto reserved = session->reserve_append({
														  .Context         = context(session->binding(), operation(51U), revision(51U), 51U, core::application_session_id{ 991U }),
														  .ProposedOrdinal = core::epoch_ordinal{ 1 },
													  },
													  { });
		expect(reserved.has_value(), "sparse spool reserves");
		auto request            = spool_request(*reserved, 51U);
		request.Spool.Residency = app::git_revision_residency::sparse_commit_trees;
		const auto recorded     = session->record_spool(request, { });
		expect(recorded && recorded->Spool->Residency == request.Spool.Residency && !recorded->Install,
			   "sparse mode is durable before index or installation witness");
		auto conflictingSpool            = request;
		conflictingSpool.Spool.Residency = app::git_revision_residency::complete_closure;
		expect(!session->record_spool(conflictingSpool, { }), "spool retry cannot change import mode");
		session.reset();
		port.reset();
		made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "pre-index sparse catalog restarts");
		port   = std::move(*made);
		opened = port->open(open_request(992U), { });
		expect(opened.has_value(), "pre-index sparse authority reopens");
		session             = std::move(*opened);
		const auto recovery = session->recovery_page(std::nullopt, 100U, { });
		expect(recovery && recovery->Operations.size() == 1U, "pre-index sparse journal remains recoverable");
		const auto record = std::get<app::append_operation_record>(recovery->Operations.front());
		expect(record.Spool == recorded->Spool && !record.Install, "restart retains exact sparse spool mode without invented installation");
		auto install = install_request(record, 51U, core::head_lineage::unknown);
		expect(!session->record_install(install, { }), "installation cannot reinterpret sparse spool as complete");
		install.Install.Residency = app::git_revision_residency::sparse_commit_trees;
		expect(session->record_install(install, { }).has_value(), "matching sparse installation is admitted after restart");
	}

	void sparse_install_restart_and_migration_contract(const bool aLegacy)
	{
		using residency = app::git_revision_residency;
		temporary_root root{ "sparse-residency" };
		auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "sparse residency catalog opens");
		auto port   = std::move(*made);
		auto opened = port->open(open_request(981U), { });
		expect(opened.has_value(), "sparse residency authority opens");
		auto       session   = std::move(*opened);
		const auto expected  = aLegacy ? residency::complete_closure : residency::sparse_commit_trees;
		const auto completed = complete_append(*session, core::epoch_ordinal{ 1 }, 1U,
											   core::application_session_id{ 981U }, true, false, nullptr, { .Residency = expected });
		expect(completed.Terminal.RecoveryInputCompacted && completed.Terminal.Install
				   && completed.Terminal.Install->Residency == expected,
			   "terminal compaction preserves exact residency witness");
		const auto pending = complete_append(*session, core::epoch_ordinal{ 2 }, 2U,
											 core::application_session_id{ 981U }, false, false, nullptr,
											 { .StopBeforeFinalize = true, .Residency = expected });
		const auto before  = session->snapshot({ });
		session.reset();
		port.reset();
		if (aLegacy) {
			direct_database database{ database_path(root.path()), true };
			database.execute("BEGIN IMMEDIATE; PRAGMA defer_foreign_keys=ON; DROP TABLE append_residency; CREATE TABLE old_meta AS SELECT * FROM catalog_meta; DROP TABLE catalog_meta; CREATE TABLE catalog_meta(singleton INTEGER NOT NULL PRIMARY KEY CHECK(singleton = 1), schema_format INTEGER NOT NULL CHECK(schema_format = 6), incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32))) STRICT, WITHOUT ROWID; CREATE UNIQUE INDEX catalog_meta_incarnation ON catalog_meta(incarnation); INSERT INTO catalog_meta SELECT singleton, 6, incarnation FROM old_meta; DROP TABLE old_meta; PRAGMA user_version=6; COMMIT");
		}
		if (aLegacy) {
			sqlite::detail::testing::interrupt_next_migration_before_commit();
			expect(!sqlite::make_epoch_catalog({ .StorageRoot = root.path() }), "interrupted sparse schema migration refuses");
			const direct_database database{ database_path(root.path()) };
			expect(database.integer("PRAGMA user_version") == 6
					   && database.integer("SELECT count(*) FROM sqlite_schema WHERE name='append_residency'") == 0,
				   "interrupted migration rolls back version and residency table together");
		}
		made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "residency catalog restarts or migrates");
		port   = std::move(*made);
		opened = port->open(open_request(982U), { });
		expect(opened.has_value(), "residency authority reopens");
		session         = std::move(*opened);
		const auto page = session->recovery_page(std::nullopt, 100U, { });
		expect(page && page->Operations.size() == 1U && session->snapshot({ }) == before,
			   "restart preserves available epochs and pending operation count");
		const auto& record = std::get<app::append_operation_record>(page->Operations.front());
		expect(record.Install == pending.Terminal.Install && record.Install->Residency == expected,
			   "restart preserves sparse witness and old v6 installs retain complete closure semantics");

		expect(session->retained_residency(core::epoch_ordinal{ 1 }, { }) == expected,
			   "retained mode survives restart and legacy migration exactly");
		direct_database database{ database_path(root.path()), true };
		expect(database.integer("PRAGMA user_version") == 7
				   && database.integer("SELECT count(*) FROM append_residency WHERE residency=1") == (aLegacy ? 0 : 2),
			   "sparse marker survives terminal compaction and no legacy row is upgraded");
		if (!aLegacy) {
			database.execute("UPDATE operation SET reserved_ordinal=77 WHERE kind=1 AND reserved_ordinal=1");
			expect(!session->retained_residency(core::epoch_ordinal{ 1 }, { }), "forged operation ordinal cannot label an epoch sparse");
			database.execute("UPDATE operation SET reserved_ordinal=1 WHERE kind=1 AND reserved_ordinal=77");
			database.execute("PRAGMA ignore_check_constraints=ON; UPDATE append_residency SET residency=9");
			expect_failure(session->retained_residency(core::epoch_ordinal{ 1 }, { }), app::epoch_catalog_failure::schema_mismatch,
						   "forged residency encoding is refused");
			database.execute("UPDATE append_residency SET residency=1; PRAGMA ignore_check_constraints=OFF");
			database.execute("DELETE FROM append_residency");
			expect(session->retained_residency(core::epoch_ordinal{ 1 }, { }) == residency::complete_closure,
				   "missing durable marker never infers sparse mode from physical refs or trees");
		}
	}

	void install_lineage_durability_contract()
	{
		temporary_root root{ "install-lineage" };
		auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "install-lineage fixture bootstraps");
		auto port   = std::move(*made);
		auto opened = port->open(open_request(171U), { });
		expect(opened.has_value(), "install-lineage authority opens");
		auto session = std::move(*opened);

		auto reserved = session->reserve_append(
			app::append_reservation_request{
				.Context = context(
					session->binding(),
					operation(50U),
					revision(50U),
					50U,
					core::application_session_id{ 171U }),
				.ProposedOrdinal = core::epoch_ordinal{ 1 },
			},
			{ });
		expect(reserved.has_value(), "install-lineage append reserves");
		auto spooled = session->record_spool(spool_request(*reserved, 50U), { });
		expect(spooled.has_value(), "install-lineage spool records");

		const auto request = install_request(
			*spooled, 50U, core::head_lineage::descendant);
		auto invalid                = request;
		invalid.Install.HeadLineage = static_cast<core::head_lineage>(0xFFU);
		expect_failure(
			session->record_install(invalid, { }),
			app::epoch_catalog_failure::invalid_request,
			"install rejects an unknown durable head-lineage encoding");

		auto installed = session->record_install(request, { });
		expect(
			installed && installed->Install
				&& installed->Install->HeadLineage
					== core::head_lineage::descendant,
			"install result preserves the exact durable head lineage");
		auto retry = session->record_install(request, { });
		expect(
			retry && *retry == *installed,
			"exact install-lineage replay is idempotent");

		auto conflicting                = request;
		conflicting.Install.HeadLineage = core::head_lineage::non_fast_forward;
		expect_failure(
			session->record_install(conflicting, { }),
			app::epoch_catalog_failure::phase_conflict,
			"a changed lineage cannot replay an already durable install");

		const auto binding = session->binding();
		session.reset();
		port.reset();
		made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "install-lineage catalog restarts");
		port   = std::move(*made);
		opened = port->open(open_request(172U), { });
		expect(opened.has_value(), "install-lineage authority reopens");
		session       = std::move(*opened);
		auto recovery = session->recovery_page(std::nullopt, 100U, { });
		expect(
			session->binding() == binding && recovery
				&& recovery->Operations.size() == 1U,
			"install-lineage restart exposes one recoverable append");
		const auto* recovered = std::get_if<app::append_operation_record>(
			&recovery->Operations.front());
		expect(
			recovered != nullptr
				&& recovered->Phase
					== app::epoch_catalog_operation_phase::imported
				&& recovered->Install
				&& *recovered->Install == request.Install
				&& recovered->Install->HeadLineage
					== core::head_lineage::descendant,
			"recovery round-trips the exact durable head lineage");
	}

	void semantic_corruption_fail_closed_contract()
	{
		const auto toStaging = [](
								   app::epoch_catalog_session&                 aSession,
								   const core::accepted_pull_request_revision& aAccepted,
								   const std::uint8_t                          aSeed,
								   const std::uint64_t                         aApplicationSession) {
			auto reserved = aSession.reserve_append(
				app::append_reservation_request{
					.Context = context(
						aSession.binding(),
						operation(aSeed),
						aAccepted,
						aSeed,
						core::application_session_id{ aApplicationSession }),
					.ProposedOrdinal = core::epoch_ordinal{ 1 },
				},
				{ });
			expect(reserved.has_value(), "corruption append reserves");
			auto spooled = aSession.record_spool(spool_request(*reserved, aSeed), { });
			expect(spooled.has_value(), "corruption append records spool");
			auto installed = aSession.record_install(install_request(*spooled, aSeed), { });
			expect(installed.has_value(), "corruption append records install");
			return transition_append(
				aSession,
				aSession.binding().Catalog,
				*installed,
				app::epoch_catalog_operation_phase::staging_ready,
				app::epoch_catalog_transition_evidence::exact_staging_pair);
		};

		{
			temporary_root root{ "corrupt-source-presence" };
			auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(made.has_value(), "source-presence fixture bootstraps");
			auto port   = std::move(*made);
			auto opened = port->open(open_request(181U), { });
			expect(opened.has_value(), "source-presence authority opens");
			auto session  = std::move(*opened);
			auto reserved = session->reserve_append(
				app::append_reservation_request{
					.Context = context(
						session->binding(),
						operation(55U),
						revision(55U),
						55U,
						core::application_session_id{ 181U }),
					.ProposedOrdinal = core::epoch_ordinal{ 1 },
				},
				{ });
			expect(reserved.has_value(), "source-presence append reserves");
			session.reset();
			port.reset();
			{
				const direct_database database{ database_path(root.path()), true };
				database.execute(
					"UPDATE append_operation SET source_review_present = 1");
			}
			expect_recovery_failure(
				root.path(),
				182U,
				app::epoch_catalog_failure::schema_mismatch,
				"source-review presence mismatch fails closed");
		}

		{
			temporary_root root{ "corrupt-inventory-count" };
			auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(made.has_value(), "inventory-count fixture bootstraps");
			auto port   = std::move(*made);
			auto opened = port->open(open_request(183U), { });
			expect(opened.has_value(), "inventory-count authority opens");
			auto session  = std::move(*opened);
			auto reserved = session->reserve_append(
				app::append_reservation_request{
					.Context = context(
						session->binding(),
						operation(56U),
						revision(56U),
						56U,
						core::application_session_id{ 183U }),
					.ProposedOrdinal = core::epoch_ordinal{ 1 },
				},
				{ });
			expect(reserved.has_value(), "inventory-count append reserves");
			auto spooled = session->record_spool(spool_request(*reserved, 56U), { });
			expect(spooled.has_value(), "inventory-count spool records");
			session.reset();
			port.reset();
			{
				const direct_database database{ database_path(root.path()), true };
				database.execute("UPDATE append_receipt SET inventory_count = 0");
			}
			expect_recovery_failure(
				root.path(),
				184U,
				app::epoch_catalog_failure::schema_mismatch,
				"inventory count mismatch fails closed");
		}

		{
			temporary_root root{ "corrupt-install-source" };
			auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(made.has_value(), "install-source fixture bootstraps");
			auto port   = std::move(*made);
			auto opened = port->open(open_request(185U), { });
			expect(opened.has_value(), "install-source authority opens");
			auto session = std::move(*opened);
			(void)toStaging(*session, revision(57U), 57U, 185U);
			session.reset();
			port.reset();
			{
				const direct_database database{ database_path(root.path()), true };
				database.execute(
					"UPDATE append_install SET head_digest = base_digest");
			}
			expect_recovery_failure(
				root.path(),
				186U,
				app::epoch_catalog_failure::schema_mismatch,
				"installed endpoints that disagree with source fail closed");
		}

		{
			temporary_root root{ "corrupt-install-lineage" };
			auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(made.has_value(), "install-lineage fixture bootstraps");
			auto port   = std::move(*made);
			auto opened = port->open(open_request(1861U), { });
			expect(opened.has_value(), "install-lineage authority opens");
			auto session = std::move(*opened);
			(void)toStaging(*session, revision(67U), 67U, 1861U);
			session.reset();
			port.reset();
			{
				const direct_database database{ database_path(root.path()), true };
				database.execute(
					"PRAGMA ignore_check_constraints = ON;"
					"UPDATE append_install SET head_lineage = 9");
			}
			auto rejected = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect_failure(
				rejected,
				app::epoch_catalog_failure::integrity_failure,
				"out-of-range durable head lineage fails bootstrap integrity");
		}

		{
			temporary_root root{ "corrupt-artifact-presence" };
			auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(made.has_value(), "artifact-presence fixture bootstraps");
			auto port   = std::move(*made);
			auto opened = port->open(open_request(187U), { });
			expect(opened.has_value(), "artifact-presence authority opens");
			auto session = std::move(*opened);
			(void)toStaging(*session, revision(58U), 58U, 187U);
			session.reset();
			port.reset();
			{
				const direct_database database{ database_path(root.path()), true };
				database.execute(
					"PRAGMA foreign_keys = ON;"
					"DELETE FROM append_installed_artifact "
					"WHERE artifact_role = 3");
			}
			expect_recovery_failure(
				root.path(),
				188U,
				app::epoch_catalog_failure::schema_mismatch,
				"missing installed artifact fails closed");
		}

		{
			temporary_root root{ "corrupt-installed-pack-bytes" };
			auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(made.has_value(), "installed-pack-byte fixture bootstraps");
			auto port   = std::move(*made);
			auto opened = port->open(open_request(1881U), { });
			expect(opened.has_value(), "installed-pack-byte authority opens");
			auto session = std::move(*opened);
			(void)toStaging(*session, revision(60U), 60U, 1881U);
			session.reset();
			port.reset();
			{
				const direct_database database{ database_path(root.path()), true };
				database.execute(
					"UPDATE append_installed_artifact SET "
					"artifact_bytes = artifact_bytes + 1 "
					"WHERE artifact_role = 2");
			}
			expect_recovery_failure(
				root.path(),
				1882U,
				app::epoch_catalog_failure::schema_mismatch,
				"installed pack bytes that disagree with inventory fail closed");
		}

		{
			temporary_root root{ "corrupt-preexisting-index-bytes" };
			auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(made.has_value(), "preexisting-index-byte fixture bootstraps");
			auto port   = std::move(*made);
			auto opened = port->open(open_request(1883U), { });
			expect(opened.has_value(), "preexisting-index-byte authority opens");
			auto session = std::move(*opened);
			(void)toStaging(*session, revision(62U), 62U, 1883U);
			session.reset();
			port.reset();
			{
				const direct_database database{ database_path(root.path()), true };
				database.execute(
					"UPDATE append_inventory SET index_bytes = index_bytes + 1");
			}
			expect_recovery_failure(
				root.path(),
				1884U,
				app::epoch_catalog_failure::schema_mismatch,
				"preexisting index bytes that disagree with install fail closed");
		}

		{
			temporary_root root{ "corrupt-material" };
			auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(made.has_value(), "material-coherence fixture bootstraps");
			auto port   = std::move(*made);
			auto opened = port->open(open_request(189U), { });
			expect(opened.has_value(), "material-coherence authority opens");
			auto session  = std::move(*opened);
			auto record   = toStaging(*session, revision(59U), 59U, 189U);
			auto mutation = candidate(record, 2'100, true);
			auto recorded = session->record_candidate(
				app::record_candidate_request{
					.Catalog                   = session->binding().Catalog,
					.Operation                 = record.Context.Operation,
					.ExpectedOperationRevision = record.OperationRevision,
					.Candidate                 = mutation,
				},
				{ });
			expect(recorded.has_value(), "material candidate records");
			session.reset();
			port.reset();
			{
				const direct_database database{ database_path(root.path()), true };
				database.execute(
					"UPDATE append_hidden_candidate SET "
					"material_head_digest = material_base_digest");
			}
			expect_recovery_failure(
				root.path(),
				190U,
				app::epoch_catalog_failure::schema_mismatch,
				"candidate material that disagrees with install fails closed");
		}

		{
			temporary_root root{ "corrupt-review" };
			auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(made.has_value(), "review-coherence fixture bootstraps");
			auto port   = std::move(*made);
			auto opened = port->open(open_request(191U), { });
			expect(opened.has_value(), "review-coherence authority opens");
			auto       session           = std::move(*opened);
			auto       accepted          = revision(61U);
			const auto catalogReview     = review_association(61U, accepted.Head);
			accepted.CurrentViewerReview = core::current_viewer_review_candidate{
				.Review = catalogReview.Review,
				.Actor  = catalogReview.Actor,
				.Order  = catalogReview.Order,
				.State  = catalogReview.State,
				.Commit = catalogReview.Commit,
			};
			auto record   = toStaging(*session, accepted, 61U, 191U);
			auto mutation = candidate(record, 2'200, true, catalogReview);
			auto recorded = session->record_candidate(
				app::record_candidate_request{
					.Catalog                   = session->binding().Catalog,
					.Operation                 = record.Context.Operation,
					.ExpectedOperationRevision = record.OperationRevision,
					.Candidate                 = mutation,
				},
				{ });
			expect(recorded.has_value(), "review candidate records");
			session.reset();
			port.reset();
			{
				const direct_database database{ database_path(root.path()), true };
				database.execute(
					"UPDATE append_candidate_review SET "
					"review_order = review_order + 1");
			}
			expect_recovery_failure(
				root.path(),
				192U,
				app::epoch_catalog_failure::schema_mismatch,
				"candidate review that disagrees with source fails closed");
		}

		{
			temporary_root root{ "corrupt-detail-count" };
			auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(made.has_value(), "detail-count fixture bootstraps");
			auto port   = std::move(*made);
			auto opened = port->open(open_request(193U), { });
			expect(opened.has_value(), "detail-count authority opens");
			auto       session = std::move(*opened);
			const auto epoch   = complete_append(
				*session,
				core::epoch_ordinal{ 1 },
				62U,
				core::application_session_id{ 193U },
				true);
			const auto retained = app::git_retained_ref_pair{
				.Scope   = session->binding().Authority.Scope,
				.Ordinal = epoch.Mutation.Epoch.Id.Ordinal,
				.Base    = epoch.Mutation.Epoch.Base,
				.Head    = epoch.Mutation.Epoch.Head,
			};
			auto begun = session->begin_prune(
				app::begin_prune_request{
					.Catalog   = session->binding().Catalog,
					.Operation = operation(63U),
					.Epochs    = {
						app::prune_epoch_fact{
							.Epoch    = epoch.Mutation.Epoch.Id,
							.Retained = retained,
							.BaseRef  = app::epoch_catalog_ref_identity{
								.Authority = session->binding().Authority.Scope,
								.Ordinal   = epoch.Mutation.Epoch.Id.Ordinal,
							},
							.HeadRef = app::epoch_catalog_ref_identity{
								.Authority = session->binding().Authority.Scope,
								.Ordinal   = epoch.Mutation.Epoch.Id.Ordinal,
								.Head      = true,
							},
						},
					},
				},
				{ });
			expect(begun.has_value(), "detail-count prune begins");
			session.reset();
			port.reset();
			{
				const direct_database database{ database_path(root.path()), true };
				database.execute(
					"UPDATE operation SET detail_count = 2 WHERE kind = 2");
			}
			expect_recovery_failure(
				root.path(),
				194U,
				app::epoch_catalog_failure::schema_mismatch,
				"maintenance detail-count mismatch fails closed");
		}

		{
			temporary_root root{ "corrupt-terminal" };
			auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(made.has_value(), "terminal-coherence fixture bootstraps");
			auto port   = std::move(*made);
			auto opened = port->open(open_request(195U), { });
			expect(opened.has_value(), "terminal-coherence authority opens");
			auto session  = std::move(*opened);
			auto reserved = session->reserve_append(
				app::append_reservation_request{
					.Context = context(
						session->binding(),
						operation(64U),
						revision(64U),
						64U,
						core::application_session_id{ 195U }),
					.ProposedOrdinal = core::epoch_ordinal{ 1 },
				},
				{ });
			expect(reserved.has_value(), "terminal-coherence append reserves");
			session.reset();
			port.reset();
			{
				const direct_database database{ database_path(root.path()), true };
				database.execute(
					"PRAGMA ignore_check_constraints = ON;"
					"UPDATE operation SET terminal = 1");
			}
			auto rejected = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect_failure(
				rejected,
				app::epoch_catalog_failure::integrity_failure,
				"terminal/phase CHECK corruption fails bootstrap integrity");
		}

		{
			temporary_root root{ "corrupt-cause" };
			auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(made.has_value(), "cause-coherence fixture bootstraps");
			auto port   = std::move(*made);
			auto opened = port->open(open_request(196U), { });
			expect(opened.has_value(), "cause-coherence authority opens");
			auto session = std::move(*opened);
			(void)complete_append(
				*session,
				core::epoch_ordinal{ 1 },
				69U,
				core::application_session_id{ 196U },
				true);
			session.reset();
			port.reset();
			{
				const direct_database database{ database_path(root.path()), true };
				database.execute(
					"UPDATE epoch SET primary_cause = 5 "
					"WHERE origin = 1 AND base_transition = 1 "
					"AND head_transition = 1");
			}
			auto remade = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(remade.has_value(), "range-valid cause corruption reopens");
			port          = std::move(*remade);
			auto reopened = port->open(open_request(197U), { });
			expect(reopened.has_value(), "cause-corrupt authority reopens");
			session = std::move(*reopened);
			expect_failure(
				session->snapshot({ }),
				app::epoch_catalog_failure::schema_mismatch,
				"range-valid but contradictory cause tuple fails closed");
		}
	}

	void composite_scope_integrity_contract()
	{
		temporary_root root{ "composite-scope-integrity" };
		auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "composite-scope fixture bootstraps");
		auto       port            = std::move(*made);
		const auto firstAuthority  = authority_key();
		const auto secondAuthority = authority_key(
			0xF200000000000016ULL,
			0xF200000000000017ULL,
			4252U);
		auto firstOpen  = port->open(open_request(501U, firstAuthority), { });
		auto secondOpen = port->open(open_request(502U, secondAuthority), { });
		expect(firstOpen && secondOpen, "two-cache integrity authorities open");
		auto first  = std::move(*firstOpen);
		auto second = std::move(*secondOpen);
		expect(
			first->binding().Authority.Cache
					!= second->binding().Authority.Cache
				&& first->binding().Authority.Scope
					!= second->binding().Authority.Scope,
			"integrity fixture owns distinct PR and cache scopes");

		const auto firstEpoch = complete_append(
			*first,
			core::epoch_ordinal{ 1 },
			100U,
			core::application_session_id{ 501U },
			true);
		const auto secondEpoch = complete_append(
			*second,
			core::epoch_ordinal{ 1 },
			101U,
			core::application_session_id{ 502U },
			true);
		const auto firstAppend  = operation(100U);
		const auto secondAppend = operation(101U);
		const auto firstPr      = hex_identity(first->binding().Authority.Scope);
		const auto secondPr     = hex_identity(second->binding().Authority.Scope);
		const auto firstCache   = hex_identity(first->binding().Authority.Cache);
		const auto secondCache  = hex_identity(second->binding().Authority.Cache);

		{
			const direct_database database{ database_path(root.path()), true };
			const auto            code = database.execute_code(
				std::string{ "UPDATE operation SET source_observation=(SELECT "
							 "source_observation FROM operation WHERE operation=x'" }
				+ hex_identity(firstAppend) + "') WHERE operation=x'"
				+ hex_identity(secondAppend) + "'");
			expect(
				(static_cast<unsigned>(code) & 0xFFU) == SQLITE_CONSTRAINT
					&& database.integer(
						   "SELECT count(DISTINCT source_observation) "
						   "FROM operation WHERE kind=1")
						== 2,
				"canonical unique index rejects a durable append source collision "
				"at the database writer boundary");
		}

		{
			const direct_database database{ database_path(root.path()), true };
			database.execute("PRAGMA foreign_keys = ON");
			const auto code = database.execute_code(
				std::string{ "UPDATE operation SET pr_scope=x'" }
				+ secondPr + "' WHERE operation=x'"
				+ hex_identity(firstAppend) + "'");
			expect(
				(static_cast<unsigned>(code) & 0xFFU) == SQLITE_CONSTRAINT,
				"operation cannot bind a PR from another cache");
			expect(
				database.foreign_key_violation_count() == 0
					&& database.integer(
						   std::string{
							   "SELECT count(*) FROM operation WHERE operation=x'" }
						   + hex_identity(firstAppend) + "' AND pr_scope=x'"
						   + firstPr + "'")
						== 1,
				"rejected operation-to-PR substitution changes no row");
		}

		const auto makePrune = [](
								   app::epoch_catalog_session&    aSession,
								   const append_outcome&          aEpoch,
								   const core::epoch_operation_id aId) {
			const auto retained = app::git_retained_ref_pair{
				.Scope   = aSession.binding().Authority.Scope,
				.Ordinal = aEpoch.Mutation.Epoch.Id.Ordinal,
				.Base    = aEpoch.Mutation.Epoch.Base,
				.Head    = aEpoch.Mutation.Epoch.Head,
			};
			const auto fact = app::prune_epoch_fact{
				.Epoch    = aEpoch.Mutation.Epoch.Id,
				.Retained = retained,
				.BaseRef  = app::epoch_catalog_ref_identity{
					.Authority = aSession.binding().Authority.Scope,
					.Ordinal   = aEpoch.Mutation.Epoch.Id.Ordinal,
				},
				.HeadRef = app::epoch_catalog_ref_identity{
					.Authority = aSession.binding().Authority.Scope,
					.Ordinal   = aEpoch.Mutation.Epoch.Id.Ordinal,
					.Head      = true,
				},
			};
			auto begun = aSession.begin_prune(
				app::begin_prune_request{
					.Catalog   = aSession.binding().Catalog,
					.Operation = aId,
					.Epochs    = { fact },
				},
				{ });
			expect(begun.has_value(), "cross-scope prune intent begins");
			return *begun;
		};
		auto firstPrune  = makePrune(*first, firstEpoch, operation(102U));
		auto secondPrune = makePrune(*second, secondEpoch, operation(103U));

		{
			const direct_database database{ database_path(root.path()), true };
			database.execute("PRAGMA foreign_keys = ON");
			const auto code = database.execute_code(
				std::string{ "UPDATE prune_operation SET operation=x'" }
				+ hex_identity(secondPrune.Operation)
				+ "' WHERE operation=x'" + hex_identity(firstPrune.Operation)
				+ "'");
			expect(
				(static_cast<unsigned>(code) & 0xFFU) == SQLITE_CONSTRAINT,
				"prune detail cannot bind an operation owned by another PR");
			expect(
				database.foreign_key_violation_count() == 0
					&& database.integer(
						   std::string{
							   "SELECT count(*) FROM prune_operation WHERE "
							   "operation=x'" }
						   + hex_identity(firstPrune.Operation)
						   + "' AND pr_scope=x'" + firstPr + "'")
						== 1,
				"rejected prune-detail substitution changes no row");
		}

		const auto abortPrune = [](app::epoch_catalog_session&       aSession,
								   app::maintenance_operation_record aRecord) {
			auto requested = aSession.transition_operation(
				app::operation_transition_request{
					.Catalog   = aSession.binding().Catalog,
					.Operation = aRecord.Operation,
					.Kind      = app::epoch_catalog_operation_kind::prune,
					.Expected  = app::epoch_catalog_operation_phase::prune_intent,
					.Next      = app::epoch_catalog_operation_phase::abort_requested,
					.Evidence  = app::epoch_catalog_transition_evidence::
						catalog_transaction,
					.ExpectedOperationRevision = aRecord.OperationRevision,
				},
				{ });
			expect(requested.has_value(), "cross-scope prune abort requests");
			const auto& requestedRecord = std::get<app::maintenance_operation_record>(*requested);
			auto        aborted         = aSession.transition_operation(
				app::operation_transition_request{
					.Catalog   = aSession.binding().Catalog,
					.Operation = requestedRecord.Operation,
					.Kind      = app::epoch_catalog_operation_kind::prune,
					.Expected  = app::epoch_catalog_operation_phase::abort_requested,
					.Next      = app::epoch_catalog_operation_phase::aborted,
					.Evidence  = app::epoch_catalog_transition_evidence::
						exact_cleanup_complete,
					.ExpectedOperationRevision = requestedRecord.OperationRevision,
				},
				{ });
			expect(aborted.has_value(), "cross-scope prune abort completes");
		};
		abortPrune(*first, firstPrune);
		abortPrune(*second, secondPrune);

		auto firstImpact = first->cache_delete_impact(
			first->binding().Authority.Cache, { });
		auto secondImpact = second->cache_delete_impact(
			second->binding().Authority.Cache, { });
		expect(firstImpact && secondImpact, "two cache impacts load exactly");
		auto firstDelete = first->begin_cache_delete(
			app::begin_cache_delete_request{
				.Operation = operation(104U),
				.Impact    = *firstImpact,
			},
			{ });
		auto secondDelete = second->begin_cache_delete(
			app::begin_cache_delete_request{
				.Operation = operation(105U),
				.Impact    = *secondImpact,
			},
			{ });
		expect(firstDelete && secondDelete, "two cache-delete seals persist");

		{
			const direct_database database{ database_path(root.path()), true };
			database.execute("PRAGMA foreign_keys = ON");
			const auto code = database.execute_code(
				std::string{ "UPDATE cache_delete_scope SET cache_scope=x'" }
				+ secondCache + "' WHERE operation=x'"
				+ hex_identity(firstDelete->Operation) + "'");
			expect(
				(static_cast<unsigned>(code) & 0xFFU) == SQLITE_CONSTRAINT,
				"cache-delete detail cannot substitute another cache");
			expect(
				database.foreign_key_violation_count() == 0
					&& database.integer(
						   std::string{
							   "SELECT count(*) FROM cache_delete_scope WHERE "
							   "operation=x'" }
						   + hex_identity(firstDelete->Operation)
						   + "' AND cache_scope=x'" + firstCache + "'")
						== 1,
				"rejected cache-delete detail substitution changes no row");
		}

		first.reset();
		second.reset();
		port.reset();
		std::array<std::int64_t, 4U> before{ };
		{
			const direct_database database{ database_path(root.path()), true };
			database.execute("PRAGMA foreign_keys = OFF");
			database.execute(
				std::string{ "UPDATE cache_delete_scope SET cache_scope=x'" }
				+ secondCache + "' WHERE operation=x'"
				+ hex_identity(firstDelete->Operation) + "'");
			expect(
				database.foreign_key_violation_count() > 0,
				"raw cross-cache corruption is present before reopen");
			before = {
				database.integer(
					"SELECT sum(phase * 100 + operation_revision) FROM operation"),
				database.integer(
					"SELECT sum(catalog_revision * 10 + lifecycle) "
					"FROM pull_request_scope"),
				database.integer("SELECT sum(lifecycle) FROM repository_scope"),
				database.integer(
					"SELECT count(*) FROM epoch_anchor WHERE availability = 1"),
			};
		}
		auto rejectedReopen = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect_failure(
			rejectedReopen,
			app::epoch_catalog_failure::integrity_failure,
			"raw cross-cache detail corruption fails catalog reopen closed");
		{
			const direct_database database{ database_path(root.path()) };
			const auto            after = std::array{
				database.integer(
					"SELECT sum(phase * 100 + operation_revision) FROM operation"),
				database.integer(
					"SELECT sum(catalog_revision * 10 + lifecycle) "
					"FROM pull_request_scope"),
				database.integer("SELECT sum(lifecycle) FROM repository_scope"),
				database.integer(
					"SELECT count(*) FROM epoch_anchor WHERE availability = 1"),
			};
			expect(
				after == before,
				"failed reopen performs no cross-scope recovery mutation");
		}
	}

	void revision_exhaustion_zero_mutation_contract()
	{
		temporary_root root{ "revision-exhaustion" };
		auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "revision-exhaustion fixture bootstraps");
		auto port   = std::move(*made);
		auto opened = port->open(open_request(171U), { });
		expect(opened.has_value(), "revision-exhaustion authority opens");
		auto       session = std::move(*opened);
		const auto epoch   = complete_append(
			*session,
			core::epoch_ordinal{ 1 },
			52U,
			core::application_session_id{ 171U },
			true);
		session.reset();
		port.reset();
		{
			const direct_database database{ database_path(root.path()), true };
			database.execute(
				"UPDATE pull_request_scope SET "
				"catalog_revision = 9223372036854775807");
		}
		auto remade = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(remade.has_value(), "maximum-revision catalog reopens");
		port          = std::move(*remade);
		auto reopened = port->open(open_request(172U), { });
		expect(reopened.has_value(), "maximum-revision authority reopens");
		session = std::move(*reopened);
		expect(
			session->binding().Catalog.Revision.value()
				== std::numeric_limits<std::int64_t>::max(),
			"direct fixture reaches the exact catalog revision boundary");
		const auto operationCount = direct_count(
			database_path(root.path()), "SELECT count(*) FROM operation");
		const auto reviewCount = direct_count(
			database_path(root.path()), "SELECT count(*) FROM review_association");
		const auto nextOrdinal = direct_count(
			database_path(root.path()),
			"SELECT next_ordinal FROM pull_request_scope");
		const auto baseAvailability = direct_count(
			database_path(root.path()),
			"SELECT availability FROM epoch_anchor WHERE endpoint = 1");
		auto reserve = session->reserve_append(
			app::append_reservation_request{
				.Context = context(
					session->binding(),
					operation(53U),
					revision(53U),
					53U,
					core::application_session_id{ 172U }),
				.ProposedOrdinal = core::epoch_ordinal{ 2 },
			},
			{ });
		expect_failure(
			reserve,
			app::epoch_catalog_failure::revision_conflict,
			"maximum catalog revision rejects a new append reservation");
		const auto review = review_association(54U, epoch.Mutation.Epoch.Head);
		expect_failure(
			session->associate_review(
				app::associate_review_request{
					.Catalog          = session->binding().Catalog,
					.Review           = review,
					.AcceptedSnapshot = core::provider_snapshot_id{
						0xFC00000000000054ULL },
				},
				{ }),
			app::epoch_catalog_failure::revision_conflict, "maximum catalog revision rejects review association");
		const auto base = anchor_identity(
			epoch.Mutation.Epoch.Id,
			core::comparison_anchor_kind::epoch_base);
		expect_failure(
			session->mark_anchor_availability(
				app::mark_anchor_availability_request{
					.Catalog = session->binding().Catalog,
					.Changes = {
						app::anchor_availability_change{
							.Anchor         = base,
							.ExpectedCommit = epoch.Mutation.Epoch.Base,
							.Expected       = core::anchor_availability::available,
							.Next           = core::anchor_availability::unavailable,
						},
					},
				},
				{ }),
			app::epoch_catalog_failure::revision_conflict, "maximum catalog revision rejects availability mutation");
		expect(
			direct_count(
				database_path(root.path()), "SELECT count(*) FROM operation")
					== operationCount
				&& direct_count(
					   database_path(root.path()),
					   "SELECT count(*) FROM review_association")
					== reviewCount
				&& direct_count(
					   database_path(root.path()),
					   "SELECT next_ordinal FROM pull_request_scope")
					== nextOrdinal
				&& direct_count(
					   database_path(root.path()),
					   "SELECT availability FROM epoch_anchor WHERE endpoint = 1")
					== baseAvailability,
			"revision exhaustion rolls back every attempted row and pointer change");
	}

	void nonterminal_revision_reserve_contract()
	{
		constexpr auto maximumRevision               = app::maximum_epoch_catalog_operation_revision;
		const auto     finalNonterminalRevision      = app::epoch_catalog_operation_revision{ maximumRevision - 1 };
		const auto     forceFinalNonterminalRevision = [=](
														   const auto& aRoot,
														   const auto& aId) {
			const direct_database database{ database_path(aRoot), true };
			database.execute(
				std::string{ "UPDATE operation SET operation_revision=" }
				+ std::to_string(finalNonterminalRevision.value())
				+ " WHERE operation=x'" + hex_identity(aId) + "'");
			expect(
				database.integer(
					std::string{
						"SELECT operation_revision FROM operation WHERE "
						"operation=x'" }
					+ hex_identity(aId) + "'")
					== finalNonterminalRevision.value(),
				"fixture reaches the final nonterminal operation revision");
		};

		{
			temporary_root root{ "append-revision-reserve" };
			auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(made.has_value(), "append reserve fixture bootstraps");
			auto port   = std::move(*made);
			auto opened = port->open(open_request(631U), { });
			expect(opened.has_value(), "append reserve authority opens");
			auto       session     = std::move(*opened);
			const auto operationId = operation(133U);
			auto       reserved    = session->reserve_append(
				app::append_reservation_request{
					.Context = context(
						session->binding(),
						operationId,
						revision(133U),
						133U,
						core::application_session_id{ 631U }),
					.ProposedOrdinal = core::epoch_ordinal{ 1 },
				},
				{ });
			expect(reserved.has_value(), "append reserve operation begins");
			forceFinalNonterminalRevision(root.path(), operationId);
			const auto before               = catalog_logical_fingerprint(database_path(root.path()));
			auto       spool                = spool_request(*reserved, 133U);
			spool.ExpectedOperationRevision = finalNonterminalRevision;
			expect_failure(
				session->record_spool(spool, { }),
				app::epoch_catalog_failure::invalid_request,
				"append cannot consume its terminal revision reserve for spool");
			expect(
				catalog_logical_fingerprint(database_path(root.path()))
					== before,
				"rejected final-reserve append changes no catalog row");

			const auto repairRequest = app::operation_transition_request{
				.Catalog                   = session->binding().Catalog,
				.Operation                 = operationId,
				.Kind                      = app::epoch_catalog_operation_kind::append,
				.Expected                  = app::epoch_catalog_operation_phase::preparing,
				.Next                      = app::epoch_catalog_operation_phase::repair_required,
				.Evidence                  = app::epoch_catalog_transition_evidence::repair_ambiguity,
				.ExpectedOperationRevision = finalNonterminalRevision,
			};
			auto        repaired     = session->transition_operation(repairRequest, { });
			const auto* repairRecord = repaired
				? std::get_if<app::append_operation_record>(&*repaired)
				: nullptr;
			auto        repairRetry  = session->transition_operation(repairRequest, { });
			expect(
				repairRecord != nullptr
					&& repairRecord->Phase
						== app::epoch_catalog_operation_phase::repair_required
					&& repairRecord->RepairFromPhase
						== app::epoch_catalog_operation_phase::preparing
					&& repairRecord->OperationRevision.value()
						== maximumRevision
					&& repairRetry
					&& std::get<app::append_operation_record>(*repairRetry)
						== *repairRecord,
				"append repair alone may consume the final revision and replay");
		}

		{
			temporary_root root{ "prune-revision-reserve" };
			auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(made.has_value(), "prune reserve fixture bootstraps");
			auto port   = std::move(*made);
			auto opened = port->open(open_request(641U), { });
			expect(opened.has_value(), "prune reserve authority opens");
			auto       session = std::move(*opened);
			const auto epoch   = complete_append(
				*session,
				core::epoch_ordinal{ 1 },
				134U,
				core::application_session_id{ 641U },
				true);
			const auto retained = app::git_retained_ref_pair{
				.Scope   = session->binding().Authority.Scope,
				.Ordinal = epoch.Mutation.Epoch.Id.Ordinal,
				.Base    = epoch.Mutation.Epoch.Base,
				.Head    = epoch.Mutation.Epoch.Head,
			};
			const auto pruneFact = app::prune_epoch_fact{
				.Epoch    = epoch.Mutation.Epoch.Id,
				.Retained = retained,
				.BaseRef  = app::epoch_catalog_ref_identity{
					.Authority = session->binding().Authority.Scope,
					.Ordinal   = epoch.Mutation.Epoch.Id.Ordinal,
				},
				.HeadRef = app::epoch_catalog_ref_identity{
					.Authority = session->binding().Authority.Scope,
					.Ordinal   = epoch.Mutation.Epoch.Id.Ordinal,
					.Head      = true,
				},
			};
			const auto operationId = operation(135U);
			auto       begun       = session->begin_prune(
				app::begin_prune_request{
					.Catalog   = session->binding().Catalog,
					.Operation = operationId,
					.Epochs    = { pruneFact },
				},
				{ });
			expect(begun.has_value(), "prune reserve operation begins");
			forceFinalNonterminalRevision(root.path(), operationId);
			const auto before = catalog_logical_fingerprint(database_path(root.path()));
			expect_failure(
				session->transition_operation(
					app::operation_transition_request{
						.Catalog   = session->binding().Catalog,
						.Operation = operationId,
						.Kind      = app::epoch_catalog_operation_kind::prune,
						.Expected  = app::epoch_catalog_operation_phase::prune_intent,
						.Next      = app::epoch_catalog_operation_phase::
							ref_delete_in_progress,
						.Evidence = app::epoch_catalog_transition_evidence::
							catalog_transaction,
						.ExpectedOperationRevision = finalNonterminalRevision,
					},
					{ }),
				app::epoch_catalog_failure::invalid_request,
				"prune cannot consume its terminal revision reserve for deletion");
			expect(
				catalog_logical_fingerprint(database_path(root.path()))
					== before,
				"rejected final-reserve prune changes no catalog row");

			const auto repairRequest = app::operation_transition_request{
				.Catalog                   = session->binding().Catalog,
				.Operation                 = operationId,
				.Kind                      = app::epoch_catalog_operation_kind::prune,
				.Expected                  = app::epoch_catalog_operation_phase::prune_intent,
				.Next                      = app::epoch_catalog_operation_phase::repair_required,
				.Evidence                  = app::epoch_catalog_transition_evidence::repair_ambiguity,
				.ExpectedOperationRevision = finalNonterminalRevision,
			};
			auto        repaired     = session->transition_operation(repairRequest, { });
			const auto* repairRecord = repaired
				? std::get_if<app::maintenance_operation_record>(&*repaired)
				: nullptr;
			auto        repairRetry  = session->transition_operation(repairRequest, { });
			expect(
				repairRecord != nullptr
					&& repairRecord->Phase
						== app::epoch_catalog_operation_phase::repair_required
					&& repairRecord->RepairFromPhase
						== app::epoch_catalog_operation_phase::prune_intent
					&& repairRecord->OperationRevision.value()
						== maximumRevision
					&& repairRetry
					&& std::get<app::maintenance_operation_record>(*repairRetry)
						== *repairRecord,
				"prune repair alone may consume the final revision and replay");
		}

		{
			temporary_root root{ "cache-delete-revision-reserve" };
			auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(made.has_value(), "cache-delete reserve fixture bootstraps");
			auto port   = std::move(*made);
			auto opened = port->open(open_request(651U), { });
			expect(opened.has_value(), "cache-delete reserve authority opens");
			auto session = std::move(*opened);
			auto impact  = session->cache_delete_impact(
				session->binding().Authority.Cache, { });
			expect(impact.has_value(), "cache-delete reserve impact loads");
			const auto operationId = operation(136U);
			auto       begun       = session->begin_cache_delete(
				app::begin_cache_delete_request{
					.Operation = operationId,
					.Impact    = *impact,
				},
				{ });
			expect(begun.has_value(), "cache-delete reserve operation seals");
			forceFinalNonterminalRevision(root.path(), operationId);
			const auto before = catalog_logical_fingerprint(database_path(root.path()));
			expect_failure(
				session->transition_operation(
					app::operation_transition_request{
						.Catalog   = session->binding().Catalog,
						.Operation = operationId,
						.Kind      = app::epoch_catalog_operation_kind::cache_delete,
						.Expected  = app::epoch_catalog_operation_phase::
							cache_delete_intent_sealed,
						.Next = app::epoch_catalog_operation_phase::
							quarantine_rename_in_progress,
						.Evidence = app::epoch_catalog_transition_evidence::
							catalog_transaction,
						.ExpectedOperationRevision = finalNonterminalRevision,
					},
					{ }),
				app::epoch_catalog_failure::invalid_request,
				"cache delete cannot consume its terminal revision reserve for "
				"rename");
			expect(
				catalog_logical_fingerprint(database_path(root.path()))
					== before,
				"rejected final-reserve cache delete changes no catalog row");

			const auto repairRequest = app::operation_transition_request{
				.Catalog   = session->binding().Catalog,
				.Operation = operationId,
				.Kind      = app::epoch_catalog_operation_kind::cache_delete,
				.Expected  = app::epoch_catalog_operation_phase::
					cache_delete_intent_sealed,
				.Next                      = app::epoch_catalog_operation_phase::repair_required,
				.Evidence                  = app::epoch_catalog_transition_evidence::repair_ambiguity,
				.ExpectedOperationRevision = finalNonterminalRevision,
			};
			auto        repaired     = session->transition_operation(repairRequest, { });
			const auto* repairRecord = repaired
				? std::get_if<app::maintenance_operation_record>(&*repaired)
				: nullptr;
			auto        repairRetry  = session->transition_operation(repairRequest, { });
			expect(
				repairRecord != nullptr
					&& repairRecord->Phase
						== app::epoch_catalog_operation_phase::repair_required
					&& repairRecord->RepairFromPhase
						== app::epoch_catalog_operation_phase::
							cache_delete_intent_sealed
					&& repairRecord->OperationRevision.value()
						== maximumRevision
					&& repairRetry
					&& std::get<app::maintenance_operation_record>(*repairRetry)
						== *repairRecord,
				"cache-delete repair alone may consume the final revision and "
				"replay");
		}
	}

	void quota_boundaries_contract()
	{
		{
			temporary_root root{ "pr-scope-quota" };
			auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(made.has_value(), "PR-scope quota fixture bootstraps");
			auto port   = std::move(*made);
			auto opened = port->open(open_request(211U), { });
			expect(opened.has_value(), "PR-scope quota seed authority opens");
			opened->reset();
			port.reset();
			{
				const direct_database database{ database_path(root.path()), true };
				database.insert_pull_request_scope_fillers(
					app::maximum_cache_delete_scope_count - 1U);
				expect(
					database.integer(
						"SELECT count(*) FROM pull_request_scope")
						== static_cast<std::int64_t>(
							app::maximum_cache_delete_scope_count),
					"direct fixture reaches the exact 10k PR-scope boundary");
			}
			auto remade = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(remade.has_value(), "full PR-scope catalog reopens");
			port                         = std::move(*remade);
			const auto overflowAuthority = authority_key(
				authority_key().Repository.value(),
				0xF20000000000FF01ULL,
				9'001U);
			expect_failure(
				port->open(open_request(212U, overflowAuthority), { }),
				app::epoch_catalog_failure::quota_exceeded,
				"10,001st PR scope is rejected at the public boundary");
			expect(
				direct_count(
					database_path(root.path()),
					"SELECT count(*) FROM pull_request_scope")
					== static_cast<std::int64_t>(
						app::maximum_cache_delete_scope_count),
				"rejected PR scope leaves the exact quota unchanged");
		}

		{
			temporary_root root{ "retained-epoch-quota" };
			auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(made.has_value(), "retained-epoch quota fixture bootstraps");
			auto port   = std::move(*made);
			auto opened = port->open(open_request(221U), { });
			expect(opened.has_value(), "retained-epoch quota authority opens");
			auto       session = std::move(*opened);
			const auto first   = complete_append(
				*session,
				core::epoch_ordinal{ 1 },
				65U,
				core::application_session_id{ 221U },
				true);
			session.reset();
			port.reset();
			{
				const direct_database database{ database_path(root.path()), true };
				database.fill_retained_epoch_quota(
					app::maximum_retained_epoch_count_per_pull_request);
				expect(
					database.integer("SELECT count(*) FROM epoch")
							== static_cast<std::int64_t>(
								app::maximum_retained_epoch_count_per_pull_request)
						&& database.integer(
							   "SELECT retained_epoch_count "
							   "FROM pull_request_scope")
							== static_cast<std::int64_t>(
								app::maximum_retained_epoch_count_per_pull_request),
					"direct fixture reaches the exact retained-epoch boundary");
			}
			auto remade = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(remade.has_value(), "full retained-epoch catalog reopens");
			port          = std::move(*remade);
			auto reopened = port->open(open_request(222U), { });
			expect(reopened.has_value(), "full retained-epoch authority reopens");
			session = std::move(*reopened);

			std::optional<core::epoch_history_cursor> cursor;
			std::size_t                               pageCount       = 0U;
			std::size_t                               epochCount      = 0U;
			auto                                      expectedOrdinal = static_cast<std::int64_t>(
				app::maximum_retained_epoch_count_per_pull_request);
			do {
				auto page = session->history(
					app::epoch_history_page_request{
						.Catalog = session->binding().Catalog,
						.Cursor  = cursor,
						.Limit   = 99U,
					},
					{ });
				expect(page.has_value() && !page->Epochs.empty()
						   && page->Epochs.size() <= 99U,
					   "one real SQLite history page remains bounded");
				for (const auto& epoch : page->Epochs) {
					expect(epoch.Id.Ordinal.value() == expectedOrdinal,
						   "real SQLite history has no gap, duplicate, or reorder");
					--expectedOrdinal;
				}
				epochCount += page->Epochs.size();
				++pageCount;
				cursor = page->Continuation;
			} while (cursor);
			expect(pageCount > 1'000U
					   && epochCount
						   == app::maximum_retained_epoch_count_per_pull_request
					   && expectedOrdinal == 0,
				   "more than one thousand real SQLite pages reach ordinal one "
				   "without a whole-history query");

			auto blocked = session->reserve_append(
				app::append_reservation_request{
					.Context = context(
						session->binding(),
						operation(66U),
						revision(66U),
						66U,
						core::application_session_id{ 222U }),
					.ProposedOrdinal = core::epoch_ordinal{ 100'001 },
				},
				{ });
			expect_failure(
				blocked,
				app::epoch_catalog_failure::quota_exceeded,
				"retained-epoch boundary blocks another append reservation");

			const auto retained = app::git_retained_ref_pair{
				.Scope   = session->binding().Authority.Scope,
				.Ordinal = first.Mutation.Epoch.Id.Ordinal,
				.Base    = first.Mutation.Epoch.Base,
				.Head    = first.Mutation.Epoch.Head,
			};
			auto prune = session->begin_prune(
				app::begin_prune_request{
					.Catalog   = session->binding().Catalog,
					.Operation = operation(67U),
					.Epochs    = {
						app::prune_epoch_fact{
							.Epoch    = first.Mutation.Epoch.Id,
							.Retained = retained,
							.BaseRef  = app::epoch_catalog_ref_identity{
								.Authority = session->binding().Authority.Scope,
								.Ordinal   = first.Mutation.Epoch.Id.Ordinal,
							},
							.HeadRef = app::epoch_catalog_ref_identity{
								.Authority = session->binding().Authority.Scope,
								.Ordinal   = first.Mutation.Epoch.Id.Ordinal,
								.Head      = true,
							},
						},
					},
				},
				{ });
			expect(prune.has_value(), "prune remains available at retained quota");
			auto deleting = session->transition_operation(
				app::operation_transition_request{
					.Catalog   = session->binding().Catalog,
					.Operation = prune->Operation,
					.Kind      = app::epoch_catalog_operation_kind::prune,
					.Expected  = app::epoch_catalog_operation_phase::prune_intent,
					.Next      = app::epoch_catalog_operation_phase::
						ref_delete_in_progress,
					.Evidence = app::epoch_catalog_transition_evidence::
						catalog_transaction,
					.ExpectedOperationRevision = prune->OperationRevision,
				},
				{ });
			expect(deleting.has_value(), "quota prune starts ref deletion");
			const auto& deletingRecord = std::get<app::maintenance_operation_record>(*deleting);
			auto        pruned         = session->finalize_prune(
				app::finalize_prune_request{
					.Transition = app::operation_transition_request{
						.Catalog   = session->binding().Catalog,
						.Operation = deletingRecord.Operation,
						.Kind      = app::epoch_catalog_operation_kind::prune,
						.Expected  = app::epoch_catalog_operation_phase::
							ref_delete_in_progress,
						.Next = app::epoch_catalog_operation_phase::
							visible_cleanup_pending,
						.Evidence = app::epoch_catalog_transition_evidence::
							exact_refs_absent,
						.ExpectedOperationRevision = deletingRecord.OperationRevision,
					},
					.ObservedAbsent = { retained },
				},
				{ });
			expect(pruned && pruned->Changed, "quota prune commits one tombstone");
			auto page = session->recovery_page(std::nullopt, 100U, { });
			expect(page && page->Operations.size() == 1U, "quota cleanup recovers");
			const auto& cleanup = std::get<app::maintenance_operation_record>(
				page->Operations.front());
			auto completed = session->transition_operation(
				app::operation_transition_request{
					.Catalog   = session->binding().Catalog,
					.Operation = cleanup.Operation,
					.Kind      = app::epoch_catalog_operation_kind::prune,
					.Expected  = app::epoch_catalog_operation_phase::
						visible_cleanup_pending,
					.Next     = app::epoch_catalog_operation_phase::complete,
					.Evidence = app::epoch_catalog_transition_evidence::
						exact_cleanup_complete,
					.ExpectedOperationRevision = cleanup.OperationRevision,
				},
				{ });
			expect(completed.has_value(), "quota prune cleanup completes");
			auto admitted = session->reserve_append(
				app::append_reservation_request{
					.Context = context(
						session->binding(),
						operation(68U),
						revision(68U),
						68U,
						core::application_session_id{ 222U }),
					.ProposedOrdinal = core::epoch_ordinal{ 100'001 },
				},
				{ });
			expect(
				admitted
					&& admitted->ReservedOrdinal == core::epoch_ordinal{ 100'001 }
					&& direct_count(
						   database_path(root.path()),
						   "SELECT retained_epoch_count "
						   "FROM pull_request_scope")
						== static_cast<std::int64_t>(
							app::maximum_retained_epoch_count_per_pull_request
							- 1U),
				"one prune frees exactly one retained-epoch quota slot");
		}
	}

	void bounded_wal_contract()
	{
		temporary_root root{ "bounded-wal" };
		auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "bounded-WAL fixture bootstraps");
		auto port   = std::move(*made);
		auto opened = port->open(open_request(231U), { });
		expect(opened.has_value(), "bounded-WAL authority opens");
		auto       session = std::move(*opened);
		const auto epoch   = complete_append(
			*session,
			core::epoch_ordinal{ 1 },
			72U,
			core::application_session_id{ 231U },
			true);
		const std::array reviews{
			review_association(73U, epoch.Mutation.Epoch.Head),
			review_association(74U, epoch.Mutation.Epoch.Head),
		};
		constexpr std::size_t writeCount = 768U;
		for (std::size_t index = 0U; index < writeCount; ++index) {
			const auto selected  = index % reviews.size();
			auto       committed = session->associate_review(
				app::associate_review_request{
					.Catalog          = session->binding().Catalog,
					.Review           = reviews[selected],
					.AcceptedSnapshot = core::provider_snapshot_id{
						0xFC00000000001000ULL
						+ static_cast<std::uint64_t>(selected) },
				},
				{ });
			expect(
				committed && committed->Changed,
				"alternating review update commits one bounded transaction");
		}
		const auto wal = test_platform::with_ascii_suffix(
			database_path(root.path()), "-wal");
		constexpr std::uintmax_t maximumExpectedWalBytes = 8U * 1024U * 1024U;
		expect(
			!std::filesystem::exists(wal)
				|| std::filesystem::file_size(wal) <= maximumExpectedWalBytes,
			"automatic checkpoints bound WAL growth under sustained writes");
		const auto binding = session->binding();
		session.reset();
		port.reset();
		auto remade = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(remade.has_value(), "bounded-WAL catalog restarts");
		port          = std::move(*remade);
		auto reopened = port->open(open_request(232U), { });
		expect(reopened.has_value(), "bounded-WAL authority reopens");
		session       = std::move(*reopened);
		auto snapshot = session->snapshot({ });
		expect(
			snapshot && snapshot->Binding == binding
				&& snapshot->CurrentViewerReview
				&& snapshot->CurrentViewerReview->Association
					== reviews[(writeCount - 1U) % reviews.size()].Association,
			"checkpointed catalog retains the last committed review on restart");
	}

	void recovery_paging_and_phase_cas_contract()
	{
		temporary_root root{ "recovery" };
		auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "recovery fixture bootstraps");
		auto       port             = std::move(*made);
		const auto firstAuthority   = authority_key();
		const auto siblingAuthority = authority_key(
			firstAuthority.Repository.value(),
			0xF200000000000008ULL,
			4243U);
		const auto otherCacheAuthority = authority_key(
			0xF200000000000009ULL,
			0xF20000000000000AULL,
			4244U);
		auto firstOpen      = port->open(open_request(201U, firstAuthority), { });
		auto siblingOpen    = port->open(open_request(202U, siblingAuthority), { });
		auto otherCacheOpen = port->open(open_request(203U, otherCacheAuthority), { });
		expect(
			firstOpen && siblingOpen && otherCacheOpen,
			"recovery authorities across one and two caches open");
		auto first      = std::move(*firstOpen);
		auto sibling    = std::move(*siblingOpen);
		auto otherCache = std::move(*otherCacheOpen);
		expect(
			first->binding().Authority.Cache
					== sibling->binding().Authority.Cache
				&& first->binding().Authority.Cache
					!= otherCache->binding().Authority.Cache
				&& first->binding().Authority.Scope
					!= sibling->binding().Authority.Scope,
			"fixture allocates sibling PR scopes in one cache and another cache");

		const auto reserve = [](app::epoch_catalog_session& aSession,
								const std::uint8_t          aSeed,
								const std::uint64_t         aApplicationSession) {
			return aSession.reserve_append(
				app::append_reservation_request{
					.Context = context(
						aSession.binding(),
						operation(aSeed),
						revision(aSeed),
						aSeed,
						core::application_session_id{ aApplicationSession }),
					.ProposedOrdinal = core::epoch_ordinal{ 1 },
				},
				{ });
		};
		auto firstPending   = reserve(*first, 50U, 201U);
		auto siblingPending = reserve(*sibling, 60U, 202U);
		auto otherPending   = reserve(*otherCache, 70U, 203U);
		expect(
			firstPending && siblingPending && otherPending,
			"one pending append reserves independently per PR authority");

		auto blockedSecond = first->reserve_append(
			app::append_reservation_request{
				.Context = context(
					first->binding(),
					operation(51U),
					revision(51U),
					51U,
					core::application_session_id{ 201U }),
				.ProposedOrdinal = core::epoch_ordinal{ 2 },
			},
			{ });
		expect_failure(
			blockedSecond,
			app::epoch_catalog_failure::operation_conflict,
			"one PR authority permits only one active operation");
		auto activeImpact = first->cache_delete_impact(
			first->binding().Authority.Cache, { });
		expect(activeImpact.has_value(), "active cache impact remains inspectable");
		auto blockedDelete = first->begin_cache_delete(
			app::begin_cache_delete_request{
				.Operation = operation(75U),
				.Impact    = *activeImpact,
			},
			{ });
		expect_failure(
			blockedDelete,
			app::epoch_catalog_failure::operation_conflict,
			"cache delete cannot seal while a child PR operation is active");

		const auto exactRecovery = [](app::epoch_catalog_session&     aSession,
									  const core::epoch_operation_id& aExpected) {
			auto page = aSession.recovery_page(std::nullopt, 100U, { });
			expect(
				page && page->Operations.size() == 1U && !page->Continuation,
				"recovery is bounded to exactly one owned operation");
			expect(
				std::get<app::append_operation_record>(page->Operations.front())
						.Context.Operation
					== aExpected,
				"recovery returns only the exact PR-owned operation");
		};
		exactRecovery(*first, operation(50U));
		exactRecovery(*sibling, operation(60U));
		exactRecovery(*otherCache, operation(70U));

		expect_failure(
			first->recovery_page(std::nullopt, 0U, { }),
			app::epoch_catalog_failure::invalid_request,
			"zero-sized recovery page is rejected");
		expect_failure(
			first->recovery_page(
				std::nullopt,
				static_cast<std::uint16_t>(
					app::maximum_epoch_recovery_page_size + 1U),
				{ }),
			app::epoch_catalog_failure::invalid_request,
			"oversized recovery page is rejected");

		auto wrongPhase = first->transition_operation(
			app::operation_transition_request{
				.Catalog                   = first->binding().Catalog,
				.Operation                 = firstPending->Context.Operation,
				.Kind                      = app::epoch_catalog_operation_kind::append,
				.Expected                  = app::epoch_catalog_operation_phase::imported,
				.Next                      = app::epoch_catalog_operation_phase::staging_ready,
				.Evidence                  = app::epoch_catalog_transition_evidence::exact_staging_pair,
				.ExpectedOperationRevision = firstPending->OperationRevision,
			},
			{ });
		expect_failure(
			wrongPhase,
			app::epoch_catalog_failure::phase_conflict,
			"phase CAS rejects a valid edge from the wrong stored phase");

		auto siblingTakeover = first->transition_operation(
			app::operation_transition_request{
				.Catalog                   = first->binding().Catalog,
				.Operation                 = siblingPending->Context.Operation,
				.Kind                      = app::epoch_catalog_operation_kind::append,
				.Expected                  = app::epoch_catalog_operation_phase::preparing,
				.Next                      = app::epoch_catalog_operation_phase::abort_requested,
				.Evidence                  = app::epoch_catalog_transition_evidence::catalog_transaction,
				.ExpectedOperationRevision = siblingPending->OperationRevision,
			},
			{ });
		expect_failure(
			siblingTakeover,
			app::epoch_catalog_failure::operation_conflict,
			"same-cache sibling PR cannot transition another authority's operation");
		auto otherCacheTakeover = first->transition_operation(
			app::operation_transition_request{
				.Catalog                   = first->binding().Catalog,
				.Operation                 = otherPending->Context.Operation,
				.Kind                      = app::epoch_catalog_operation_kind::append,
				.Expected                  = app::epoch_catalog_operation_phase::preparing,
				.Next                      = app::epoch_catalog_operation_phase::abort_requested,
				.Evidence                  = app::epoch_catalog_transition_evidence::catalog_transaction,
				.ExpectedOperationRevision = otherPending->OperationRevision,
			},
			{ });
		expect_failure(
			otherCacheTakeover,
			app::epoch_catalog_failure::operation_conflict,
			"different-cache authority cannot transition another operation");
		exactRecovery(*sibling, operation(60U));
		exactRecovery(*otherCache, operation(70U));

		std::stop_source cancelled;
		cancelled.request_stop();
		expect_failure(
			first->recovery_page(std::nullopt, 1U, cancelled.get_token()),
			app::epoch_catalog_failure::cancelled,
			"pre-cancelled recovery page returns cancellation");

		const auto firstBinding   = first->binding();
		const auto siblingBinding = sibling->binding();
		const auto otherBinding   = otherCache->binding();
		first.reset();
		sibling.reset();
		otherCache.reset();
		port.reset();
		auto reopenedPortResult = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(
			reopenedPortResult.has_value(),
			"catalog with pending recovery records reopens");
		auto reopenedPort          = std::move(*reopenedPortResult);
		auto reopenedResult        = reopenedPort->open(open_request(204U, firstAuthority), { });
		auto reopenedSiblingResult = reopenedPort->open(open_request(205U, siblingAuthority), { });
		auto reopenedOtherResult   = reopenedPort->open(open_request(206U, otherCacheAuthority), { });
		expect(
			reopenedResult && reopenedSiblingResult && reopenedOtherResult,
			"all recovery authorities reopen");
		auto reopened        = std::move(*reopenedResult);
		auto reopenedSibling = std::move(*reopenedSiblingResult);
		auto reopenedOther   = std::move(*reopenedOtherResult);
		expect(
			reopened->binding() == firstBinding
				&& reopenedSibling->binding() == siblingBinding
				&& reopenedOther->binding() == otherBinding,
			"pending operations preserve each authority binding across restart");
		auto recovered        = reopened->recovery_page(std::nullopt, 100U, { });
		auto recoveredSibling = reopenedSibling->recovery_page(std::nullopt, 100U, { });
		auto recoveredOther   = reopenedOther->recovery_page(std::nullopt, 100U, { });
		expect(
			recovered && recovered->Operations.size() == 1U
				&& recoveredSibling
				&& recoveredSibling->Operations.size() == 1U
				&& recoveredOther
				&& recoveredOther->Operations.size() == 1U,
			"restart preserves exact PR- and cache-scoped recovery isolation");
	}

	void exact_cache_delete_intent_contract()
	{
		temporary_root root{ "cache-delete" };
		auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "cache-delete fixture bootstraps");
		auto       port      = std::move(*made);
		const auto authority = authority_key();
		auto       opened    = port->open(open_request(301U, authority), { });
		expect(opened.has_value(), "cache-delete fixture authority opens");
		auto       session = std::move(*opened);
		const auto epoch   = complete_append(
			*session,
			core::epoch_ordinal{ 1 },
			80U,
			core::application_session_id{ 301U },
			true);
		const auto review     = review_association(81U, epoch.Mutation.Epoch.Head);
		auto       associated = session->associate_review(
			app::associate_review_request{
				.Catalog          = session->binding().Catalog,
				.Review           = review,
				.AcceptedSnapshot = core::provider_snapshot_id{
					0xFC00000000000081ULL },
			},
			{ });
		expect(associated && associated->Changed, "delete fixture links a review");
		const auto liveBinding  = session->binding();
		const auto reviewAnchor = core::comparison_anchor_identity{
			.Authority   = liveBinding.Authority.Scope,
			.Incarnation = liveBinding.Catalog.Incarnation,
			.Kind        = core::comparison_anchor_kind::review_commit,
			.Review      = review.Association,
		};

		const auto restart = [&](const std::uint64_t aApplicationSession) {
			session.reset();
			port.reset();
			auto remade = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(remade.has_value(), "cache-delete catalog restarts");
			port          = std::move(*remade);
			auto reopened = port->open(
				open_request(aApplicationSession, authority), { });
			expect(reopened.has_value(), "cache-delete authority reopens");
			session = std::move(*reopened);
		};
		const auto recoverDelete = [&]() {
			auto page = session->recovery_page(std::nullopt, 100U, { });
			expect(page.has_value(), "cache-delete recovery page loads");
			std::optional<app::maintenance_operation_record> found;
			for (const auto& value : page->Operations) {
				const auto* maintenance = std::get_if<app::maintenance_operation_record>(&value);
				if (maintenance != nullptr && maintenance->Operation == operation(82U)) {
					found = *maintenance;
				}
			}
			expect(found.has_value(), "cache-delete recovery remains exact");
			return *found;
		};

		auto impact = session->cache_delete_impact(
			session->binding().Authority.Cache, { });
		expect(impact.has_value(), "exact cache-delete impact loads");
		expect(
			impact->Incarnation == session->binding().Catalog.Incarnation
				&& impact->Cache == session->binding().Authority.Cache
				&& impact->Scopes.size() == 1U
				&& impact->Scopes.front().Authority
					== session->binding().Authority.Scope
				&& impact->Scopes.front().ExpectedRevision
					== session->binding().Catalog.Revision
				&& impact->RetainedEpochCount == 1U,
			"cache-delete impact seals exact scope revisions and epoch count");

		auto wrongImpact               = *impact;
		wrongImpact.RetainedEpochCount = 2U;
		auto rejected                  = session->begin_cache_delete(
			app::begin_cache_delete_request{
				.Operation = operation(81U),
				.Impact    = wrongImpact,
			},
			{ });
		expect_failure(
			rejected,
			app::epoch_catalog_failure::revision_conflict,
			"cache delete rejects an impact that no longer exactly matches");

		auto begun = session->begin_cache_delete(
			app::begin_cache_delete_request{
				.Operation = operation(82U),
				.Impact    = *impact,
			},
			{ });
		expect(
			begun
				&& begun->Kind
					== app::epoch_catalog_operation_kind::cache_delete
				&& begun->Phase
					== app::epoch_catalog_operation_phase::
						cache_delete_intent_sealed
				&& begun->Scopes == impact->Scopes,
			"cache-delete operation persists the exact sealed scope set");
		auto begunRetry = session->begin_cache_delete(
			app::begin_cache_delete_request{
				.Operation = operation(82U),
				.Impact    = *impact,
			},
			{ });
		expect(
			begunRetry && *begunRetry == *begun,
			"exact cache-delete begin retry is idempotent");

		expect_failure(
			session->snapshot({ }),
			app::epoch_catalog_failure::operation_conflict,
			"sealed parent cache blocks ordinary child reads");
		auto blockedAppend = session->reserve_append(
			app::append_reservation_request{
				.Context = context(
					session->binding(),
					operation(83U),
					revision(83U),
					83U,
					core::application_session_id{ 301U }),
				.ProposedOrdinal = core::epoch_ordinal{ 2 },
			},
			{ });
		expect_failure(
			blockedAppend,
			app::epoch_catalog_failure::authority_mismatch,
			"sealed parent cache blocks new child operations");

		restart(302U);
		expect(
			session->binding() == liveBinding,
			"sealed cache restart retains the original authority binding");
		auto sealed = recoverDelete();
		expect(
			sealed.Phase
				== app::epoch_catalog_operation_phase::
					cache_delete_intent_sealed,
			"restart resumes from the durable delete seal");

		auto renameResult = session->transition_operation(
			app::operation_transition_request{
				.Catalog   = session->binding().Catalog,
				.Operation = sealed.Operation,
				.Kind      = app::epoch_catalog_operation_kind::cache_delete,
				.Expected  = app::epoch_catalog_operation_phase::
					cache_delete_intent_sealed,
				.Next = app::epoch_catalog_operation_phase::
					quarantine_rename_in_progress,
				.Evidence                  = app::epoch_catalog_transition_evidence::catalog_transaction,
				.ExpectedOperationRevision = sealed.OperationRevision,
			},
			{ });
		expect(renameResult.has_value(), "cache quarantine rename starts");
		restart(303U);
		auto renaming = recoverDelete();
		expect(
			renaming.Phase
				== app::epoch_catalog_operation_phase::
					quarantine_rename_in_progress,
			"restart retains the point-of-no-cancellation rename phase");
		std::stop_source cancelledAfterPonr;
		cancelledAfterPonr.request_stop();
		auto quarantinedResult = session->transition_operation(
			app::operation_transition_request{
				.Catalog   = session->binding().Catalog,
				.Operation = renaming.Operation,
				.Kind      = app::epoch_catalog_operation_kind::cache_delete,
				.Expected  = app::epoch_catalog_operation_phase::
					quarantine_rename_in_progress,
				.Next                      = app::epoch_catalog_operation_phase::quarantined,
				.Evidence                  = app::epoch_catalog_transition_evidence::exact_quarantine,
				.ExpectedOperationRevision = renaming.OperationRevision,
			},
			cancelledAfterPonr.get_token());
		expect(
			quarantinedResult.has_value(),
			"post-PONR cancellation cannot interrupt exact quarantine recovery");
		const auto* quarantined = std::get_if<app::maintenance_operation_record>(&*quarantinedResult);
		expect(quarantined != nullptr, "quarantine returns maintenance record");
		const auto finalizeRequest = app::finalize_cache_delete_request{
			.Transition = app::operation_transition_request{
				.Catalog   = session->binding().Catalog,
				.Operation = quarantined->Operation,
				.Kind      = app::epoch_catalog_operation_kind::cache_delete,
				.Expected  = app::epoch_catalog_operation_phase::quarantined,
				.Next      = app::epoch_catalog_operation_phase::
					catalog_tombstoned_cleanup_pending,
				.Evidence = app::epoch_catalog_transition_evidence::
					catalog_transaction,
				.ExpectedOperationRevision = quarantined->OperationRevision,
			},
		};
		auto finalized = session->finalize_cache_delete(
			finalizeRequest, cancelledAfterPonr.get_token());
		expect(
			finalized.has_value(),
			"post-PONR cancellation cannot split cache tombstoning");
		auto finalizedRetry = session->finalize_cache_delete(finalizeRequest, { });
		expect(
			finalizedRetry.has_value()
				&& session->binding().Catalog.Revision.value()
					== liveBinding.Catalog.Revision.value() + 1,
			"exact cache-delete finalizer replay adds no second revision");

		restart(304U);
		auto cleanup = recoverDelete();
		expect(
			session->binding().Authority.Cache == liveBinding.Authority.Cache
				&& session->binding().Authority.Scope
					== liveBinding.Authority.Scope
				&& cleanup.Phase
					== app::epoch_catalog_operation_phase::
						catalog_tombstoned_cleanup_pending,
			"tombstoned restart preserves ownership until cleanup completes");
		const auto cleanupTransition = app::operation_transition_request{
			.Catalog   = session->binding().Catalog,
			.Operation = cleanup.Operation,
			.Kind      = app::epoch_catalog_operation_kind::cache_delete,
			.Expected  = app::epoch_catalog_operation_phase::
				catalog_tombstoned_cleanup_pending,
			.Next                      = app::epoch_catalog_operation_phase::complete,
			.Evidence                  = app::epoch_catalog_transition_evidence::exact_cleanup_complete,
			.ExpectedOperationRevision = cleanup.OperationRevision,
		};
		auto complete = session->transition_operation(
			cleanupTransition, cancelledAfterPonr.get_token());
		expect(complete.has_value(), "cache-delete cleanup reaches terminal");
		auto completeRetry = session->transition_operation(cleanupTransition, { });
		expect(
			completeRetry.has_value(),
			"exact cache-delete cleanup replay remains terminal");

		expect_failure(
			session->cache_delete_impact(session->binding().Authority.Cache, { }),
			app::epoch_catalog_failure::authority_mismatch,
			"deleted cache no longer produces a live impact set");
		restart(305U);
		expect(
			session->binding().Catalog.Incarnation
					== epoch.Commit.Catalog.Incarnation
				&& session->binding().Authority.Cache
					!= impact->Cache
				&& session->binding().Authority.Scope
					== epoch.Mutation.Epoch.Id.Authority
				&& session->binding().Catalog.Revision.value()
					== liveBinding.Catalog.Revision.value() + 1,
			"new cache lifetime reparents the stable PR scope and revision");
		const auto oldCache = hex_identity(impact->Cache);
		expect(
			direct_count(
				database_path(root.path()),
				std::string{ "SELECT count(*) FROM operation WHERE cache_scope=x'" }
					+ oldCache + "'")
					== 0
				&& direct_count(
					   database_path(root.path()),
					   "SELECT (SELECT count(*) FROM append_operation) + "
					   "(SELECT count(*) FROM append_source_review) + "
					   "(SELECT count(*) FROM append_receipt) + "
					   "(SELECT count(*) FROM append_inventory) + "
					   "(SELECT count(*) FROM append_install) + "
					   "(SELECT count(*) FROM append_installed_artifact) + "
					   "(SELECT count(*) FROM append_hidden_candidate) + "
					   "(SELECT count(*) FROM append_candidate_review) + "
					   "(SELECT count(*) FROM prune_operation) + "
					   "(SELECT count(*) FROM cache_delete_scope)")
					== 0,
			"cache retirement removes old terminal operations and every operation "
			"detail before stable-scope reparenting");
		auto deletedAnchors = session->exact_anchors(
			app::exact_anchor_lookup_request{
				.Catalog = session->binding().Catalog,
				.Anchors = {
					anchor_identity(
						epoch.Mutation.Epoch.Id,
						core::comparison_anchor_kind::epoch_base),
					reviewAnchor,
				},
			},
			{ });
		auto retainedHistory = session->history(
			app::epoch_history_page_request{
				.Catalog = session->binding().Catalog,
				.Limit   = 10U,
			},
			{ });
		auto deletedSnapshot = session->snapshot({ });
		expect(
			deletedAnchors && deletedAnchors->Anchors.size() == 2U
				&& deletedAnchors->Anchors[0].Availability
					== core::anchor_availability::cache_deleted
				&& deletedAnchors->Anchors[1].Availability
					== core::anchor_availability::cache_deleted
				&& retainedHistory && retainedHistory->Epochs.size() == 1U
				&& retainedHistory->Epochs.front().Id
					== epoch.Mutation.Epoch.Id
				&& deletedSnapshot && deletedSnapshot->CurrentViewerReview
				&& deletedSnapshot->CurrentViewerReview->Availability
					== core::anchor_availability::cache_deleted,
			"reparented history retains epoch and review cache-delete tombstones");

		auto next = session->reserve_append(
			app::append_reservation_request{
				.Context = context(
					session->binding(),
					operation(84U),
					revision(84U),
					84U,
					core::application_session_id{ 305U }),
				.ProposedOrdinal = core::epoch_ordinal{ 2 },
			},
			{ });
		expect(
			next && next->ReservedOrdinal == core::epoch_ordinal{ 2 },
			"reparented PR continues its never-reused ordinal sequence");
	}

	void receiving_ownership_migration_and_restart_contract()
	{
		temporary_root root{ "receiving-ownership" };
		auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "receiving fixture bootstraps");
		auto port   = std::move(*made);
		auto opened = port->open(open_request(1U), { });
		expect(opened.has_value(), "receiving fixture opens");
		auto       session = std::move(*opened);
		const auto visible = complete_append(*session, core::epoch_ordinal{ 1 }, 151U,
											 core::application_session_id{ 1U }, true, false, nullptr, { .PersistReceiving = true });
		expect(!visible.Terminal.Receiving && direct_count(database_path(root.path()), "SELECT count(*) FROM append_receiving") == 0,
			   "successful completion compacts the exact receiving fact");
		auto reserved = session->reserve_append({
													.Context         = context(session->binding(), operation(152U), revision(152U), 152U, core::application_session_id{ 1U }),
													.ProposedOrdinal = core::epoch_ordinal{ 2 },
												},
												{ });
		expect(reserved.has_value(), "reserve a legacy unfinished operation");
		const auto& legacy           = *reserved;
		const auto  originalSnapshot = session->snapshot({ });
		expect(originalSnapshot.has_value(), "snapshot exists before migration");
		session.reset();
		port.reset();
		{
			direct_database database{ database_path(root.path()), true };
			expect(test_platform::make_version_two_catalog(database), "restore v2 with visible epoch and unfinished operation");
		}
		auto remade = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(remade.has_value(), "migrate populated v2 catalog");
		port          = std::move(*remade);
		auto reopened = port->open(open_request(1U), { });
		expect(reopened.has_value(), "reopen migrated authority");
		session                     = std::move(*reopened);
		const auto migratedSnapshot = session->snapshot({ });
		const auto legacyPage       = session->recovery_page({ }, 10U, { });
		expect(migratedSnapshot && *migratedSnapshot == *originalSnapshot && legacyPage && legacyPage->Operations.size() == 1U
				   && std::get<app::append_operation_record>(legacyPage->Operations.front()) == legacy,
			   "migration preserves visible epoch, next ordinal, authority and full legacy operation without invented ownership");
		const auto receivingRequest = app::record_receiving_request{
			.Catalog                   = session->binding().Catalog,
			.Operation                 = legacy.Context.Operation,
			.ExpectedOperationRevision = legacy.OperationRevision,
			.Receiving                 = { .Operation    = legacy.Context.Operation,
										   .Identity     = spool_request(legacy, 152U).Spool.Spool.Identity,
										   .MaximumBytes = 4096U },
		};
		std::stop_source stopped;
		stopped.request_stop();
		expect_failure(session->record_receiving(receivingRequest, stopped.get_token()), app::epoch_catalog_failure::cancelled,
					   "cancelled receiving persistence writes nothing");
		auto invalid                   = receivingRequest;
		invalid.Receiving.MaximumBytes = 0;
		expect_failure(session->record_receiving(invalid, { }), app::epoch_catalog_failure::invalid_request, "zero receiving bound is refused");
		auto received = session->record_receiving(receivingRequest, { });
		expect(received && received->Receiving == receivingRequest.Receiving
				   && received->Phase == app::epoch_catalog_operation_phase::preparing
				   && received->OperationRevision.value() == legacy.OperationRevision.value() + 1,
			   "receiving persistence advances only its operation revision and exact ownership");
		const auto replay = session->record_receiving(receivingRequest, { });
		expect(replay && *replay == *received, "exact receiving retry is idempotent");
		auto conflict = receivingRequest;
		conflict.Receiving.Identity.Bytes.back() ^= 1U;
		expect_failure(session->record_receiving(conflict, { }), app::epoch_catalog_failure::phase_conflict, "conflicting identity cannot overwrite persisted ownership");
		session.reset();
		port.reset();
		remade = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(remade.has_value(), "receiving ownership catalog restarts");
		port     = std::move(*remade);
		reopened = port->open(open_request(1U), { });
		expect(reopened.has_value(), "receiving ownership authority reopens");
		session         = std::move(*reopened);
		const auto page = session->recovery_page({ }, 10U, { });
		expect(page && page->Operations.size() == 1U && std::get<app::append_operation_record>(page->Operations.front()) == *received,
			   "reopen returns exact durable receiving ownership and revision");
		auto spool      = spool_request(*received, 152U);
		auto wrongSpool = spool;
		wrongSpool.Spool.Spool.Identity.Bytes.back() ^= 1U;
		expect_failure(session->record_spool(wrongSpool, { }), app::epoch_catalog_failure::invalid_request, "completed receipt cannot substitute another file identity");
		wrongSpool                   = spool;
		wrongSpool.Spool.Spool.Bytes = 8192U;
		expect_failure(session->record_spool(wrongSpool, { }), app::epoch_catalog_failure::invalid_request, "completed receipt cannot exceed the persisted payload limit");
		const auto spooled = session->record_spool(spool, { });
		expect(spooled && spooled->Receiving == received->Receiving, "completed receipt preserves receiving ownership");
		const auto aborting = transition_append(*session, session->binding().Catalog, *spooled,
												app::epoch_catalog_operation_phase::abort_requested, app::epoch_catalog_transition_evidence::catalog_transaction);
		const auto aborted  = transition_append(*session, session->binding().Catalog, aborting,
												app::epoch_catalog_operation_phase::aborted, app::epoch_catalog_transition_evidence::exact_cleanup_complete);
		expect(!aborted.Receiving && !aborted.Spool && direct_count(database_path(root.path()), "SELECT count(*) FROM append_receiving") == 0,
			   "abort compacts ownership and completed spool facts without altering the visible epoch");
	}

	void source_observation_restart_contract()
	{
		temporary_root root{ "source-observation-restart" };
		auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "source-observation fixture bootstraps");
		auto       port      = std::move(*made);
		const auto authority = authority_key(
			0xF200000000000076ULL,
			0xF200000000000077ULL,
			4312U);
		auto opened = port->open(open_request(1U, authority), { });
		expect(opened.has_value(), "source-observation authority opens");
		auto       session = std::move(*opened);
		const auto first   = complete_append(
			*session,
			core::epoch_ordinal{ 1 },
			160U,
			core::application_session_id{ 1U },
			true);

		auto restartedRevision = core::accepted_pull_request_revision{
			.Source     = first.Mutation.Epoch.Source,
			.Origin     = core::pull_request_revision_origin::live,
			.BaseBranch = first.Mutation.Epoch.BaseBranch,
			.Base       = first.Mutation.Epoch.Base,
			.Head       = oid<core::git_commit_oid>(0x39U),
		};
		restartedRevision.Source.Observation = opaque<core::epoch_source_observation_id>(161U);

		session.reset();
		port.reset();
		auto remade = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(remade.has_value(), "source-observation catalog restarts");
		port          = std::move(*remade);
		auto reopened = port->open(open_request(1U, authority), { });
		expect(reopened.has_value(), "source-observation authority reopens");
		session = std::move(*reopened);

		const auto second = complete_append(
			*session,
			core::epoch_ordinal{ 2 },
			161U,
			core::application_session_id{ 1U },
			false,
			false,
			nullptr,
			append_fixture_options{ .Accepted = restartedRevision });
		session.reset();
		port.reset();
		remade = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(remade.has_value(), "changed source observation persists");
		port     = std::move(*remade);
		reopened = port->open(open_request(1U, authority), { });
		expect(reopened.has_value(), "persisted source observations reopen");
		session            = std::move(*reopened);
		const auto history = session->history(
			app::epoch_history_page_request{
				.Catalog = session->binding().Catalog,
				.Limit   = 10U,
			},
			{ });
		expect(
			history && history->Epochs.size() == 2U
				&& history->Epochs[0].Source == second.Mutation.Epoch.Source
				&& history->Epochs[1].Source == first.Mutation.Epoch.Source
				&& history->Epochs[0].Source.Observation
					!= history->Epochs[1].Source.Observation
				&& history->Epochs[0].Source.Snapshot
					== history->Epochs[1].Source.Snapshot
				&& history->Epochs[0].Source.IdentityGeneration
					== history->Epochs[1].Source.IdentityGeneration
				&& history->Epochs[0].Source.PullRequestGeneration
					== history->Epochs[1].Source.PullRequestGeneration
				&& history->Epochs[0].Head == restartedRevision.Head
				&& history->Epochs[0].Head != history->Epochs[1].Head
				&& direct_count(
					   database_path(root.path()),
					   "SELECT count(*) FROM operation WHERE kind=1 AND "
					   "application_session=x'0000000000000001'")
					== 2,
			"two changed revisions survive real restart boundaries when fresh "
			"durable observation IDs distinguish reset process-local counters");
	}

	void retention_rehydration_restart_contract()
	{
		temporary_root root{ "retention-rehydration" };
		auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "retention-rehydration fixture bootstraps");
		auto       port      = std::move(*made);
		const auto authority = authority_key(
			0xF200000000000056ULL,
			0xF200000000000057ULL,
			4292U);
		auto opened = port->open(open_request(661U, authority), { });
		expect(opened.has_value(), "retention-rehydration authority opens");
		auto       session = std::move(*opened);
		const auto first   = complete_append(
			*session,
			core::epoch_ordinal{ 1 },
			140U,
			core::application_session_id{ 661U },
			true);
		auto accepted = core::accepted_pull_request_revision{
			.Source     = first.Mutation.Epoch.Source,
			.Origin     = core::pull_request_revision_origin::live,
			.BaseBranch = first.Mutation.Epoch.BaseBranch,
			.Base       = first.Mutation.Epoch.Base,
			.Head       = first.Mutation.Epoch.Head,
		};
		accepted.Source.Observation     = opaque<core::epoch_source_observation_id>(142U);
		constexpr auto rehydrationCause = core::epoch_cause{
			.Origin   = core::epoch_origin::retention_rehydration,
			.Base     = core::epoch_base_transition::unchanged,
			.Head     = core::epoch_head_transition::unchanged,
			.Material = core::epoch_material_relation::different,
			.Primary  = core::epoch_primary_cause::retention_rehydration,
		};
		const auto originalBinding = session->binding();
		const auto restart         = [&](const std::uint64_t aApplicationSession) {
			session.reset();
			port.reset();
			auto remade = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(remade.has_value(), "retention-rehydration catalog restarts");
			port          = std::move(*remade);
			auto reopened = port->open(
				open_request(aApplicationSession, authority), { });
			expect(
				reopened.has_value(),
				"retention-rehydration authority reopens");
			session = std::move(*reopened);
		};

		const auto firstDeleted = complete_cache_delete(*session, operation(141U));
		restart(662U);
		auto required = session->snapshot({ });
		expect(
			session->binding().Authority.Scope
					== originalBinding.Authority.Scope
				&& session->binding().Authority.Cache != firstDeleted.Cache
				&& required
				&& required->RetentionRehydration
					== core::retention_rehydration_requirement::required
				&& required->LatestLive
				&& required->LatestLive->Id == first.Mutation.Epoch.Id
				&& required->LatestLive->BaseAvailability
					== core::anchor_availability::cache_deleted
				&& required->LatestLive->HeadAvailability
					== core::anchor_availability::cache_deleted,
			"reparent preserves stable PR identity and exposes the deleted latest "
			"as requiring retention rehydration");

		const auto reparentedBinding = session->binding();
		restart(663U);
		auto requiredAfterRestart = session->snapshot({ });
		expect(
			session->binding() == reparentedBinding
				&& requiredAfterRestart
				&& requiredAfterRestart->RetentionRehydration
					== core::retention_rehydration_requirement::required
				&& requiredAfterRestart->LatestLive
				&& requiredAfterRestart->LatestLive->Id
					== first.Mutation.Epoch.Id,
			"rehydration requirement and reparented binding survive a database "
			"restart before append");

		const auto second = complete_append(
			*session,
			core::epoch_ordinal{ 2 },
			142U,
			core::application_session_id{ 663U },
			false,
			false,
			nullptr,
			append_fixture_options{
				.Accepted = accepted,
				.Cause    = rehydrationCause,
			});
		auto restored   = session->snapshot({ });
		auto oldAnchors = session->exact_anchors(
			app::exact_anchor_lookup_request{
				.Catalog = session->binding().Catalog,
				.Anchors = {
					anchor_identity(
						first.Mutation.Epoch.Id,
						core::comparison_anchor_kind::epoch_base),
					anchor_identity(
						first.Mutation.Epoch.Id,
						core::comparison_anchor_kind::epoch_head),
				},
			},
			{ });
		expect(
			second.Mutation.Epoch.Source.Observation
					!= first.Mutation.Epoch.Source.Observation
				&& second.Mutation.Epoch.Source.Snapshot
					== first.Mutation.Epoch.Source.Snapshot
				&& second.Mutation.Epoch.Source.IdentityGeneration
					== first.Mutation.Epoch.Source.IdentityGeneration
				&& second.Mutation.Epoch.Source.PullRequestGeneration
					== first.Mutation.Epoch.Source.PullRequestGeneration
				&& second.Mutation.Epoch.BaseBranch
					== first.Mutation.Epoch.BaseBranch
				&& second.Mutation.Epoch.Base == first.Mutation.Epoch.Base
				&& second.Mutation.Epoch.Head == first.Mutation.Epoch.Head
				&& second.Mutation.Epoch.Material
					!= first.Mutation.Epoch.Material
				&& second.Mutation.Epoch.Cause == rehydrationCause
				&& restored
				&& restored->RetentionRehydration
					== core::retention_rehydration_requirement::not_required
				&& restored->LatestLive
				&& restored->LatestLive->Id == second.Mutation.Epoch.Id
				&& restored->LatestLive->BaseAvailability
					== core::anchor_availability::available
				&& restored->LatestLive->HeadAvailability
					== core::anchor_availability::available
				&& oldAnchors && oldAnchors->Anchors.size() == 2U
				&& oldAnchors->Anchors[0].Availability
					== core::anchor_availability::cache_deleted
				&& oldAnchors->Anchors[1].Availability
					== core::anchor_availability::cache_deleted
				&& direct_count(
					   database_path(root.path()),
					   "SELECT count(*) FROM pull_request_scope WHERE "
					   "latest_live_ordinal=2 AND "
					   "retention_rehydration_required=0")
					== 1,
			"one fresh-observation replacement atomically clears the durable "
			"flag after a counter reset, becomes latest, and preserves the old "
			"cache-delete tombstone");

		const auto secondDeleted = complete_cache_delete(*session, operation(143U));
		restart(664U);
		auto requiredAgain = session->snapshot({ });
		expect(
			session->binding().Authority.Cache != secondDeleted.Cache
				&& session->binding().Authority.Scope
					== originalBinding.Authority.Scope
				&& requiredAgain
				&& requiredAgain->RetentionRehydration
					== core::retention_rehydration_requirement::required
				&& requiredAgain->LatestLive
				&& requiredAgain->LatestLive->Id == second.Mutation.Epoch.Id
				&& requiredAgain->LatestLive->BaseAvailability
					== core::anchor_availability::cache_deleted
				&& requiredAgain->LatestLive->HeadAvailability
					== core::anchor_availability::cache_deleted,
			"a later cache lifetime independently requires one new retention "
			"epoch");

		auto acceptedAgain               = accepted;
		acceptedAgain.Source.Observation = opaque<core::epoch_source_observation_id>(144U);
		const auto third                 = complete_append(
			*session,
			core::epoch_ordinal{ 3 },
			144U,
			core::application_session_id{ 664U },
			false,
			false,
			nullptr,
			append_fixture_options{
				.Accepted = acceptedAgain,
				.Cause    = rehydrationCause,
			});
		auto thirdSnapshot = session->snapshot({ });
		expect(
			third.Mutation.Epoch.Cause == rehydrationCause && thirdSnapshot
				&& thirdSnapshot->RetentionRehydration
					== core::retention_rehydration_requirement::not_required
				&& thirdSnapshot->LatestLive
				&& thirdSnapshot->LatestLive->Id == third.Mutation.Epoch.Id,
			"the second cache loss also admits exactly one replacement epoch");

		restart(665U);
		auto clearedAfterRestart = session->snapshot({ });
		expect(
			clearedAfterRestart
				&& clearedAfterRestart->RetentionRehydration
					== core::retention_rehydration_requirement::not_required
				&& clearedAfterRestart->LatestLive
				&& clearedAfterRestart->LatestLive->Id
					== third.Mutation.Epoch.Id,
			"successful retention rehydration remains consumed after restart");

		const auto beforeDuplicate = catalog_logical_fingerprint(database_path(root.path()));
		expect_failure(
			session->reserve_append(
				app::append_reservation_request{
					.Context = context(
						session->binding(),
						operation(145U),
						accepted,
						145U,
						core::application_session_id{ 665U }),
					.ProposedOrdinal = core::epoch_ordinal{ 4 },
				},
				{ }),
			app::epoch_catalog_failure::source_observation_conflict,
			"a source-observation identity already published before restart is "
			"rejected before a second operation reserves");
		expect(
			catalog_logical_fingerprint(database_path(root.path()))
				== beforeDuplicate,
			"rejected duplicate reservation changes no operation, epoch, anchor, "
			"pointer, revision, lifecycle, or retention row");
	}

	void no_latest_reparent_contract()
	{
		temporary_root root{ "no-latest-reparent" };
		auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "no-latest reparent fixture bootstraps");
		auto       port      = std::move(*made);
		const auto authority = authority_key(
			0xF200000000000066ULL,
			0xF200000000000067ULL,
			4302U);
		auto opened = port->open(open_request(671U, authority), { });
		expect(opened.has_value(), "no-latest authority opens");
		auto       session         = std::move(*opened);
		const auto originalBinding = session->binding();
		const auto deleted         = complete_cache_delete(*session, operation(150U));
		session.reset();
		port.reset();
		auto remade = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(remade.has_value(), "no-latest catalog restarts");
		port          = std::move(*remade);
		auto reopened = port->open(open_request(672U, authority), { });
		expect(reopened.has_value(), "no-latest authority reparents");
		session       = std::move(*reopened);
		auto snapshot = session->snapshot({ });
		expect(
			session->binding().Authority.Scope
					== originalBinding.Authority.Scope
				&& session->binding().Authority.Cache != deleted.Cache
				&& snapshot && !snapshot->LatestLive
				&& snapshot->RetentionRehydration
					== core::retention_rehydration_requirement::not_required
				&& direct_count(
					   database_path(root.path()),
					   "SELECT count(*) FROM pull_request_scope WHERE "
					   "latest_live_ordinal IS NULL AND "
					   "retention_rehydration_required=0")
					== 1,
			"a stable PR with no latest-live pointer never invents a rehydration "
			"requirement");
	}

	void prune_replay_revision_identity_contract()
	{
		temporary_root root{ "prune-replay-revision" };
		auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "prune-replay fixture bootstraps");
		auto port   = std::move(*made);
		auto opened = port->open(open_request(681U), { });
		expect(opened.has_value(), "prune-replay authority opens");
		auto       session = std::move(*opened);
		const auto epoch   = complete_append(
			*session,
			core::epoch_ordinal{ 1 },
			151U,
			core::application_session_id{ 681U },
			true);
		const auto retained = app::git_retained_ref_pair{
			.Scope   = session->binding().Authority.Scope,
			.Ordinal = epoch.Mutation.Epoch.Id.Ordinal,
			.Base    = epoch.Mutation.Epoch.Base,
			.Head    = epoch.Mutation.Epoch.Head,
		};
		const auto pruneFact = app::prune_epoch_fact{
			.Epoch    = epoch.Mutation.Epoch.Id,
			.Retained = retained,
			.BaseRef  = app::epoch_catalog_ref_identity{
				.Authority = session->binding().Authority.Scope,
				.Ordinal   = epoch.Mutation.Epoch.Id.Ordinal,
			},
			.HeadRef = app::epoch_catalog_ref_identity{
				.Authority = session->binding().Authority.Scope,
				.Ordinal   = epoch.Mutation.Epoch.Id.Ordinal,
				.Head      = true,
			},
		};
		const auto pruneId = operation(152U);
		const auto request = app::begin_prune_request{
			.Catalog   = session->binding().Catalog,
			.Operation = pruneId,
			.Epochs    = { pruneFact },
		};
		auto begun = session->begin_prune(request, { });
		expect(begun.has_value(), "prune-replay exact intent begins");
		{
			const direct_database database{ database_path(root.path()), true };
			database.execute(
				std::string{ "UPDATE operation SET expected_revision=" }
				+ std::to_string(request.Catalog.Revision.value() + 1)
				+ " WHERE operation=x'" + hex_identity(pruneId) + "'");
		}
		const auto before = catalog_logical_fingerprint(database_path(root.path()));
		expect_failure(
			session->begin_prune(request, { }),
			app::epoch_catalog_failure::operation_conflict,
			"prune replay rejects a journal from another expected revision");
		expect(
			catalog_logical_fingerprint(database_path(root.path()))
				== before,
			"rejected prune replay changes no operation, detail, catalog, epoch, "
			"anchor, pointer, lifecycle, or retention row");
	}

	void prune_expected_availability_contract()
	{
		temporary_root root{ "prune-expected-availability" };
		auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "expected-availability fixture bootstraps");
		auto       port      = std::move(*made);
		const auto authority = authority_key(
			0xF200000000000086ULL,
			0xF200000000000087ULL,
			4322U);
		auto opened = port->open(open_request(711U, authority), { });
		expect(opened.has_value(), "expected-availability authority opens");
		auto       session = std::move(*opened);
		const auto epoch   = complete_append(
			*session,
			core::epoch_ordinal{ 1 },
			170U,
			core::application_session_id{ 711U },
			true);

		auto reviews = std::array{
			review_association(171U, epoch.Mutation.Epoch.Head),
			review_association(172U, epoch.Mutation.Epoch.Head),
			review_association(173U, epoch.Mutation.Epoch.Head),
			review_association(174U, epoch.Mutation.Epoch.Head),
		};
		reviews[1].Availability = core::anchor_availability::unavailable;
		reviews[2].Availability = core::anchor_availability::never_observed;
		for (std::size_t index = 0U; index < reviews.size(); ++index) {
			auto associated = session->associate_review(
				app::associate_review_request{
					.Catalog          = session->binding().Catalog,
					.Review           = reviews[index],
					.AcceptedSnapshot = core::provider_snapshot_id{
						0xFC00000000002000ULL
						+ static_cast<std::uint64_t>(index) },
				},
				{ });
			expect(
				associated && associated->Changed,
				"each linked review prestate associates durably");
		}

		const auto reviewAnchor = [](const auto& aBinding, const auto& aReview) {
			return core::comparison_anchor_identity{
				.Authority   = aBinding.Authority.Scope,
				.Incarnation = aBinding.Catalog.Incarnation,
				.Kind        = core::comparison_anchor_kind::review_commit,
				.Review      = aReview.Association,
			};
		};
		auto corruptReview = session->mark_anchor_availability(
			app::mark_anchor_availability_request{
				.Catalog = session->binding().Catalog,
				.Changes = {
					app::anchor_availability_change{
						.Anchor = reviewAnchor(
							session->binding(), reviews[3]),
						.ExpectedCommit = reviews[3].Commit,
						.Expected       = core::anchor_availability::available,
						.Next           = core::anchor_availability::corrupt,
					},
				},
			},
			{ });
		expect(
			corruptReview && corruptReview->Changed,
			"linked review reaches a durable corrupt prestate");

		const auto baseAnchor = anchor_identity(
			epoch.Mutation.Epoch.Id,
			core::comparison_anchor_kind::epoch_base);
		const auto headAnchor = anchor_identity(
			epoch.Mutation.Epoch.Id,
			core::comparison_anchor_kind::epoch_head);
		auto degraded = session->mark_anchor_availability(
			app::mark_anchor_availability_request{
				.Catalog = session->binding().Catalog,
				.Changes = {
					app::anchor_availability_change{
						.Anchor         = baseAnchor,
						.ExpectedCommit = epoch.Mutation.Epoch.Base,
						.Expected       = core::anchor_availability::available,
						.Next           = core::anchor_availability::corrupt,
					},
					app::anchor_availability_change{
						.Anchor         = headAnchor,
						.ExpectedCommit = epoch.Mutation.Epoch.Head,
						.Expected       = core::anchor_availability::available,
						.Next           = core::anchor_availability::unavailable,
					},
				},
			},
			{ });
		expect(
			degraded && degraded->Changed
				&& direct_count(
					   database_path(root.path()),
					   "SELECT retained_epoch_count FROM pull_request_scope")
					== 1,
			"corrupt and unavailable retained anchors still consume one quota "
			"slot");

		const auto retained = app::git_retained_ref_pair{
			.Scope   = session->binding().Authority.Scope,
			.Ordinal = epoch.Mutation.Epoch.Id.Ordinal,
			.Base    = epoch.Mutation.Epoch.Base,
			.Head    = epoch.Mutation.Epoch.Head,
		};
		const auto pruneFact = app::prune_epoch_fact{
			.Epoch    = epoch.Mutation.Epoch.Id,
			.Retained = retained,
			.BaseRef  = app::epoch_catalog_ref_identity{
				.Authority = session->binding().Authority.Scope,
				.Ordinal   = epoch.Mutation.Epoch.Id.Ordinal,
			},
			.HeadRef = app::epoch_catalog_ref_identity{
				.Authority = session->binding().Authority.Scope,
				.Ordinal   = epoch.Mutation.Epoch.Id.Ordinal,
				.Head      = true,
			},
			.ExpectedBaseAvailability = core::anchor_availability::corrupt,
			.ExpectedHeadAvailability = core::anchor_availability::unavailable,
		};

		auto wrongNewFact                     = pruneFact;
		wrongNewFact.ExpectedBaseAvailability = core::anchor_availability::available;
		const auto beforeWrongNew             = catalog_logical_fingerprint(database_path(root.path()));
		expect_failure(
			session->begin_prune(
				app::begin_prune_request{
					.Catalog   = session->binding().Catalog,
					.Operation = operation(175U),
					.Epochs    = { wrongNewFact },
				},
				{ }),
			app::epoch_catalog_failure::authority_mismatch,
			"new prune rejects a wrong expected anchor availability");
		expect(
			catalog_logical_fingerprint(database_path(root.path()))
				== beforeWrongNew,
			"wrong new-prune availability changes no logical catalog row");

		const auto request = app::begin_prune_request{
			.Catalog   = session->binding().Catalog,
			.Operation = operation(176U),
			.Epochs    = { pruneFact },
		};
		auto begun = session->begin_prune(request, { });
		expect(
			begun && begun->Epochs == request.Epochs
				&& direct_count(
					   database_path(root.path()),
					   "SELECT base_availability * 10 + head_availability "
					   "FROM prune_operation")
					== 52,
			"prune intent persists exact corrupt and unavailable expectations");

		auto wrongReplay                                    = request;
		wrongReplay.Epochs.front().ExpectedHeadAvailability = core::anchor_availability::corrupt;
		const auto beforeWrongReplay                        = catalog_logical_fingerprint(database_path(root.path()));
		expect_failure(
			session->begin_prune(wrongReplay, { }),
			app::epoch_catalog_failure::operation_conflict,
			"prune replay rejects different expected availability");
		expect(
			catalog_logical_fingerprint(database_path(root.path()))
				== beforeWrongReplay,
			"wrong prune replay changes no logical catalog row");

		{
			const direct_database database{ database_path(root.path()), true };
			database.execute(
				"UPDATE epoch_anchor SET availability=2 WHERE endpoint=1");
		}
		const auto beforeDriftedReplay = catalog_logical_fingerprint(database_path(root.path()));
		expect_failure(
			session->begin_prune(request, { }),
			app::epoch_catalog_failure::operation_conflict,
			"prune replay verifies persisted expectation against the live anchor");
		expect(
			catalog_logical_fingerprint(database_path(root.path()))
				== beforeDriftedReplay,
			"drifted-anchor replay adds no compensating catalog mutation");
		{
			const direct_database database{ database_path(root.path()), true };
			database.execute(
				"UPDATE epoch_anchor SET availability=5 WHERE endpoint=1");
		}

		const auto restart = [&](const std::uint64_t aApplicationSession) {
			session.reset();
			port.reset();
			auto remade = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(remade.has_value(), "expected-availability catalog restarts");
			port          = std::move(*remade);
			auto reopened = port->open(
				open_request(aApplicationSession, authority), { });
			expect(reopened.has_value(), "expected-availability authority reopens");
			session = std::move(*reopened);
		};

		restart(712U);
		auto recovery = session->recovery_page(std::nullopt, 100U, { });
		expect(
			recovery && recovery->Operations.size() == 1U
				&& std::get<app::maintenance_operation_record>(
					   recovery->Operations.front())
						.Epochs
					== request.Epochs,
			"exact prune availability intent survives restart");
		auto replayed = session->begin_prune(request, { });
		expect(
			replayed && *replayed == *begun,
			"pre-finalization replay revalidates the persisted anchor states");

		auto deleting = session->transition_operation(
			app::operation_transition_request{
				.Catalog   = session->binding().Catalog,
				.Operation = replayed->Operation,
				.Kind      = app::epoch_catalog_operation_kind::prune,
				.Expected  = app::epoch_catalog_operation_phase::prune_intent,
				.Next      = app::epoch_catalog_operation_phase::
					ref_delete_in_progress,
				.Evidence                  = app::epoch_catalog_transition_evidence::catalog_transaction,
				.ExpectedOperationRevision = replayed->OperationRevision,
			},
			{ });
		expect(deleting.has_value(), "degraded-anchor prune starts ref deletion");
		const auto deletingRecord  = std::get<app::maintenance_operation_record>(*deleting);
		const auto finalizeRequest = app::finalize_prune_request{
			.Transition = app::operation_transition_request{
				.Catalog   = session->binding().Catalog,
				.Operation = deletingRecord.Operation,
				.Kind      = app::epoch_catalog_operation_kind::prune,
				.Expected  = app::epoch_catalog_operation_phase::
					ref_delete_in_progress,
				.Next = app::epoch_catalog_operation_phase::
					visible_cleanup_pending,
				.Evidence = app::epoch_catalog_transition_evidence::
					exact_refs_absent,
				.ExpectedOperationRevision = deletingRecord.OperationRevision,
			},
			.ObservedAbsent = { retained },
		};

		{
			const direct_database database{ database_path(root.path()), true };
			database.execute(
				"UPDATE epoch_anchor SET availability=5 WHERE endpoint=2");
		}
		const auto beforeWrongFinalize = catalog_logical_fingerprint(database_path(root.path()));
		expect_failure(
			session->finalize_prune(finalizeRequest, { }),
			app::epoch_catalog_failure::operation_conflict,
			"finalize prune rejects a changed expected availability");
		expect(
			catalog_logical_fingerprint(database_path(root.path()))
					== beforeWrongFinalize
				&& direct_count(
					   database_path(root.path()),
					   "SELECT retained_epoch_count FROM pull_request_scope")
					== 1,
			"failed finalize rolls back anchor, review, revision, operation, and "
			"quota mutations");
		{
			const direct_database database{ database_path(root.path()), true };
			database.execute(
				"UPDATE epoch_anchor SET availability=2 WHERE endpoint=2");
		}

		auto pruned = session->finalize_prune(finalizeRequest, { });
		expect(
			pruned && pruned->Changed
				&& direct_count(
					   database_path(root.path()),
					   "SELECT retained_epoch_count FROM pull_request_scope")
					== 0,
			"exact degraded-anchor prune releases one retained quota slot");
		const auto afterFinalize  = catalog_logical_fingerprint(database_path(root.path()));
		auto       finalizeReplay = session->finalize_prune(finalizeRequest, { });
		expect(
			finalizeReplay && *finalizeReplay == *pruned
				&& catalog_logical_fingerprint(database_path(root.path()))
					== afterFinalize
				&& direct_count(
					   database_path(root.path()),
					   "SELECT retained_epoch_count FROM pull_request_scope")
					== 0,
			"finalize replay neither decrements quota nor mutates any row twice");

		auto finalizedBeginReplay = session->begin_prune(request, { });
		expect(
			finalizedBeginReplay
				&& finalizedBeginReplay->Phase
					== app::epoch_catalog_operation_phase::
						visible_cleanup_pending
				&& direct_count(
					   database_path(root.path()),
					   "SELECT count(*) FROM epoch_anchor WHERE availability=3")
					== 2
				&& direct_count(
					   database_path(root.path()),
					   "SELECT count(*) FROM review_association")
					== static_cast<std::int64_t>(reviews.size())
				&& direct_count(
					   database_path(root.path()),
					   "SELECT count(*) FROM review_association "
					   "WHERE availability=3")
					== static_cast<std::int64_t>(reviews.size()),
			"finalized begin replay verifies both anchors and every linked review "
			"are coherently pruned");

		auto snapshot = session->snapshot({ });
		expect(
			snapshot && snapshot->LatestLive
				&& snapshot->LatestLive->BaseAvailability
					== core::anchor_availability::pruned
				&& snapshot->LatestLive->HeadAvailability
					== core::anchor_availability::pruned
				&& snapshot->CurrentViewerReview
				&& snapshot->CurrentViewerReview->Availability
					== core::anchor_availability::pruned,
			"pruned anchor and current-review projections agree");

		auto complete = session->transition_operation(
			app::operation_transition_request{
				.Catalog   = session->binding().Catalog,
				.Operation = finalizedBeginReplay->Operation,
				.Kind      = app::epoch_catalog_operation_kind::prune,
				.Expected  = app::epoch_catalog_operation_phase::
					visible_cleanup_pending,
				.Next     = app::epoch_catalog_operation_phase::complete,
				.Evidence = app::epoch_catalog_transition_evidence::
					exact_cleanup_complete,
				.ExpectedOperationRevision = finalizedBeginReplay->OperationRevision,
			},
			{ });
		expect(complete.has_value(), "degraded-anchor prune cleanup completes");

		restart(713U);
		auto completeReplay = session->begin_prune(request, { });
		auto history        = session->history(
			app::epoch_history_page_request{
				.Catalog = session->binding().Catalog,
				.Limit   = 10U,
			},
			{ });
		expect(
			completeReplay
				&& completeReplay->Phase
					== app::epoch_catalog_operation_phase::complete
				&& history && history->Epochs.size() == 1U
				&& history->Epochs.front().BaseAvailability
					== core::anchor_availability::pruned
				&& history->Epochs.front().HeadAvailability
					== core::anchor_availability::pruned
				&& direct_count(
					   database_path(root.path()),
					   "SELECT retained_epoch_count FROM pull_request_scope")
					== 0,
			"complete prune replay and exact zero quota survive restart");

		auto admitted = session->reserve_append(
			app::append_reservation_request{
				.Context = context(
					session->binding(),
					operation(177U),
					revision(177U),
					177U,
					core::application_session_id{ 713U }),
				.ProposedOrdinal = core::epoch_ordinal{ 2 },
			},
			{ });
		expect(
			admitted && admitted->ReservedOrdinal == core::epoch_ordinal{ 2 },
			"released retained quota admits the next append after restart");
	}

	void cache_delete_abort_restores_active_contract()
	{
		temporary_root root{ "cache-delete-abort" };
		auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "cache-delete-abort fixture bootstraps");
		auto       port      = std::move(*made);
		const auto authority = authority_key(
			0xF200000000000046ULL,
			0xF200000000000047ULL,
			4282U);
		auto opened = port->open(open_request(621U, authority), { });
		expect(opened.has_value(), "cache-delete-abort authority opens");
		auto       session = std::move(*opened);
		const auto epoch   = complete_append(
			*session,
			core::epoch_ordinal{ 1 },
			130U,
			core::application_session_id{ 621U },
			true);
		const auto activeBinding = session->binding();
		auto       impact        = session->cache_delete_impact(
			activeBinding.Authority.Cache, { });
		expect(impact.has_value(), "abort fixture cache impact loads");
		const auto deleteId = operation(131U);
		auto       begun    = session->begin_cache_delete(
			app::begin_cache_delete_request{
				.Operation = deleteId,
				.Impact    = *impact,
			},
			{ });
		expect(begun.has_value(), "abort fixture cache delete seals");
		expect_failure(
			session->snapshot({ }),
			app::epoch_catalog_failure::operation_conflict,
			"sealed cache blocks ordinary reads before abort");

		auto abortRequested = session->transition_operation(
			app::operation_transition_request{
				.Catalog   = activeBinding.Catalog,
				.Operation = deleteId,
				.Kind      = app::epoch_catalog_operation_kind::cache_delete,
				.Expected  = app::epoch_catalog_operation_phase::
					cache_delete_intent_sealed,
				.Next                      = app::epoch_catalog_operation_phase::abort_requested,
				.Evidence                  = app::epoch_catalog_transition_evidence::catalog_transaction,
				.ExpectedOperationRevision = begun->OperationRevision,
			},
			{ });
		expect(abortRequested.has_value(), "pre-PONR cache abort requests");
		const auto& requestedRecord = std::get<app::maintenance_operation_record>(*abortRequested);
		expect(
			requestedRecord.Phase
				== app::epoch_catalog_operation_phase::abort_requested,
			"pre-PONR cache abort retains its typed journal phase");
		const auto finalNonterminalRevision = app::epoch_catalog_operation_revision{
			app::maximum_epoch_catalog_operation_revision - 1
		};
		{
			const direct_database database{ database_path(root.path()), true };
			database.execute(
				std::string{ "UPDATE operation SET operation_revision=" }
				+ std::to_string(finalNonterminalRevision.value())
				+ " WHERE operation=x'" + hex_identity(deleteId) + "'");
		}
		const auto abortTransition = app::operation_transition_request{
			.Catalog                   = activeBinding.Catalog,
			.Operation                 = deleteId,
			.Kind                      = app::epoch_catalog_operation_kind::cache_delete,
			.Expected                  = app::epoch_catalog_operation_phase::abort_requested,
			.Next                      = app::epoch_catalog_operation_phase::aborted,
			.Evidence                  = app::epoch_catalog_transition_evidence::exact_cleanup_complete,
			.ExpectedOperationRevision = finalNonterminalRevision,
		};
		auto aborted = session->transition_operation(abortTransition, { });
		expect(aborted.has_value(), "pre-PONR cache abort reaches terminal");
		const auto abortedRecord = std::get<app::maintenance_operation_record>(*aborted);
		auto       abortedRetry  = session->transition_operation(abortTransition, { });
		expect(
			abortedRecord.Phase == app::epoch_catalog_operation_phase::aborted
				&& abortedRecord.OperationRevision.value()
					== app::maximum_epoch_catalog_operation_revision
				&& abortedRetry
				&& std::get<app::maintenance_operation_record>(*abortedRetry)
					== abortedRecord,
			"exact cache-abort retry remains stable");
		expect(
			direct_count(
				database_path(root.path()),
				std::string{ "SELECT count(*) FROM repository_scope WHERE "
							 "cache_scope=x'" }
					+ hex_identity(activeBinding.Authority.Cache)
					+ "' AND lifecycle=1")
					== 1
				&& direct_count(
					   database_path(root.path()),
					   std::string{ "SELECT count(*) FROM operation WHERE "
									"operation=x'" }
						   + hex_identity(deleteId)
						   + "' AND phase=11 AND terminal=2 "
							 "AND operation_revision=9223372036854775807")
					== 1,
			"cache abort atomically restores active lifecycle with terminal journal");

		auto restoredSnapshot = session->snapshot({ });
		auto restoredImpact   = session->cache_delete_impact(
			activeBinding.Authority.Cache, { });
		expect(
			restoredSnapshot
				&& restoredSnapshot->Binding.Catalog == activeBinding.Catalog
				&& restoredSnapshot->LatestLive
				&& restoredSnapshot->LatestLive->Id == epoch.Mutation.Epoch.Id
				&& restoredImpact && *restoredImpact == *impact,
			"cache abort restores ordinary snapshot and impact access");

		auto append = session->reserve_append(
			app::append_reservation_request{
				.Context = context(
					session->binding(),
					operation(132U),
					revision(132U),
					132U,
					core::application_session_id{ 621U }),
				.ProposedOrdinal = core::epoch_ordinal{ 2 },
			},
			{ });
		expect(append.has_value(), "cache abort restores append access");
		auto appendAbortRequested = session->transition_operation(
			app::operation_transition_request{
				.Catalog                   = session->binding().Catalog,
				.Operation                 = append->Context.Operation,
				.Kind                      = app::epoch_catalog_operation_kind::append,
				.Expected                  = app::epoch_catalog_operation_phase::preparing,
				.Next                      = app::epoch_catalog_operation_phase::abort_requested,
				.Evidence                  = app::epoch_catalog_transition_evidence::catalog_transaction,
				.ExpectedOperationRevision = append->OperationRevision,
			},
			{ });
		expect(appendAbortRequested.has_value(), "probe append abort requests");
		const auto& appendRequested = std::get<app::append_operation_record>(*appendAbortRequested);
		auto        appendAborted   = session->transition_operation(
			app::operation_transition_request{
				.Catalog                   = session->binding().Catalog,
				.Operation                 = appendRequested.Context.Operation,
				.Kind                      = app::epoch_catalog_operation_kind::append,
				.Expected                  = app::epoch_catalog_operation_phase::abort_requested,
				.Next                      = app::epoch_catalog_operation_phase::aborted,
				.Evidence                  = app::epoch_catalog_transition_evidence::exact_cleanup_complete,
				.ExpectedOperationRevision = appendRequested.OperationRevision,
			},
			{ });
		expect(appendAborted.has_value(), "probe append abort completes");

		session.reset();
		port.reset();
		auto remade = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(remade.has_value(), "aborted cache catalog restarts");
		port          = std::move(*remade);
		auto reopened = port->open(open_request(622U, authority), { });
		expect(reopened.has_value(), "aborted cache authority reopens active");
		session              = std::move(*reopened);
		auto restartSnapshot = session->snapshot({ });
		auto restartImpact   = session->cache_delete_impact(
			session->binding().Authority.Cache, { });
		auto recovery                       = session->recovery_page(std::nullopt, 100U, { });
		auto expectedRestartSnapshot        = *restoredSnapshot;
		expectedRestartSnapshot.NextOrdinal = core::epoch_ordinal{ 3 };
		expect(
			session->binding() == activeBinding && restartSnapshot
				&& restoredSnapshot->NextOrdinal == core::epoch_ordinal{ 2 }
				&& *restartSnapshot == expectedRestartSnapshot
				&& restartImpact && *restartImpact == *impact && recovery
				&& recovery->Operations.empty(),
			"aborted cache restart preserves the restored snapshot and impact, "
			"advances the never-reused ordinal, and has no recovery handoff");
	}

	void tombstoned_cache_repair_contract()
	{
		temporary_root root{ "tombstoned-cache-repair" };
		auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "tombstoned-repair fixture bootstraps");
		auto       port      = std::move(*made);
		const auto authority = authority_key(
			0xF200000000000026ULL,
			0xF200000000000027ULL,
			4262U);
		auto opened = port->open(open_request(601U, authority), { });
		expect(opened.has_value(), "tombstoned-repair authority opens");
		auto       session = std::move(*opened);
		const auto epoch   = complete_append(
			*session,
			core::epoch_ordinal{ 1 },
			110U,
			core::application_session_id{ 601U },
			true);
		const auto review     = review_association(111U, epoch.Mutation.Epoch.Head);
		auto       associated = session->associate_review(
			app::associate_review_request{
				.Catalog          = session->binding().Catalog,
				.Review           = review,
				.AcceptedSnapshot = core::provider_snapshot_id{
					0xFC00000000000111ULL },
			},
			{ });
		expect(associated && associated->Changed, "repair fixture links a review");
		const auto liveBinding = session->binding();

		auto impact = session->cache_delete_impact(
			session->binding().Authority.Cache, { });
		expect(impact.has_value(), "tombstoned-repair impact loads");
		auto begun = session->begin_cache_delete(
			app::begin_cache_delete_request{
				.Operation = operation(112U),
				.Impact    = *impact,
			},
			{ });
		expect(begun.has_value(), "tombstoned-repair delete seals");
		auto renaming = session->transition_operation(
			app::operation_transition_request{
				.Catalog   = session->binding().Catalog,
				.Operation = begun->Operation,
				.Kind      = app::epoch_catalog_operation_kind::cache_delete,
				.Expected  = app::epoch_catalog_operation_phase::
					cache_delete_intent_sealed,
				.Next = app::epoch_catalog_operation_phase::
					quarantine_rename_in_progress,
				.Evidence                  = app::epoch_catalog_transition_evidence::catalog_transaction,
				.ExpectedOperationRevision = begun->OperationRevision,
			},
			{ });
		expect(renaming.has_value(), "tombstoned-repair rename starts");
		const auto& renamingRecord = std::get<app::maintenance_operation_record>(*renaming);
		auto        quarantined    = session->transition_operation(
			app::operation_transition_request{
				.Catalog   = session->binding().Catalog,
				.Operation = renamingRecord.Operation,
				.Kind      = app::epoch_catalog_operation_kind::cache_delete,
				.Expected  = app::epoch_catalog_operation_phase::
					quarantine_rename_in_progress,
				.Next                      = app::epoch_catalog_operation_phase::quarantined,
				.Evidence                  = app::epoch_catalog_transition_evidence::exact_quarantine,
				.ExpectedOperationRevision = renamingRecord.OperationRevision,
			},
			{ });
		expect(quarantined.has_value(), "tombstoned-repair quarantine completes");
		const auto& quarantinedRecord = std::get<app::maintenance_operation_record>(*quarantined);
		auto        finalized         = session->finalize_cache_delete(
			app::finalize_cache_delete_request{
				.Transition = app::operation_transition_request{
					.Catalog   = session->binding().Catalog,
					.Operation = quarantinedRecord.Operation,
					.Kind      = app::epoch_catalog_operation_kind::cache_delete,
					.Expected  = app::epoch_catalog_operation_phase::quarantined,
					.Next      = app::epoch_catalog_operation_phase::
						catalog_tombstoned_cleanup_pending,
					.Evidence = app::epoch_catalog_transition_evidence::
						catalog_transaction,
					.ExpectedOperationRevision = quarantinedRecord.OperationRevision,
				},
			},
			{ });
		expect(finalized.has_value(), "repair fixture atomically tombstones cache");
		auto page = session->recovery_page(std::nullopt, 100U, { });
		expect(page && page->Operations.size() == 1U, "phase-19 record recovers");
		const auto cleanup = std::get<app::maintenance_operation_record>(page->Operations.front());
		expect(
			cleanup.Phase
				== app::epoch_catalog_operation_phase::
					catalog_tombstoned_cleanup_pending,
			"cache delete reaches catalog-tombstoned cleanup pending");
		auto repaired = session->transition_operation(
			app::operation_transition_request{
				.Catalog   = session->binding().Catalog,
				.Operation = cleanup.Operation,
				.Kind      = app::epoch_catalog_operation_kind::cache_delete,
				.Expected  = app::epoch_catalog_operation_phase::
					catalog_tombstoned_cleanup_pending,
				.Next                      = app::epoch_catalog_operation_phase::repair_required,
				.Evidence                  = app::epoch_catalog_transition_evidence::repair_ambiguity,
				.ExpectedOperationRevision = cleanup.OperationRevision,
			},
			{ });
		expect(repaired.has_value(), "phase-19 cleanup can enter durable repair");
		const auto repairRecord = std::get<app::maintenance_operation_record>(*repaired);
		expect(
			repairRecord.Phase
					== app::epoch_catalog_operation_phase::repair_required
				&& repairRecord.RepairFromPhase
					== app::epoch_catalog_operation_phase::
						catalog_tombstoned_cleanup_pending
				&& repairRecord.Scopes == cleanup.Scopes,
			"repair records exact phase-19 origin and sealed scope facts");

		const auto reviewAnchor = core::comparison_anchor_identity{
			.Authority   = epoch.Mutation.Epoch.Id.Authority,
			.Incarnation = epoch.Mutation.Epoch.Id.Incarnation,
			.Kind        = core::comparison_anchor_kind::review_commit,
			.Review      = review.Association,
		};
		const auto assertTombstones = [&]() {
			auto snapshot = session->snapshot({ });
			auto history  = session->history(
				app::epoch_history_page_request{
					.Catalog = session->binding().Catalog,
					.Limit   = 10U,
				},
				{ });
			auto anchors = session->exact_anchors(
				app::exact_anchor_lookup_request{
					.Catalog = session->binding().Catalog,
					.Anchors = {
						anchor_identity(
							epoch.Mutation.Epoch.Id,
							core::comparison_anchor_kind::epoch_base),
						anchor_identity(
							epoch.Mutation.Epoch.Id,
							core::comparison_anchor_kind::epoch_head),
						reviewAnchor,
					},
				},
				{ });
			expect(
				snapshot && snapshot->CurrentViewerReview
					&& snapshot->CurrentViewerReview->Availability
						== core::anchor_availability::cache_deleted
					&& history && history->Epochs.size() == 1U
					&& history->Epochs.front().Id == epoch.Mutation.Epoch.Id
					&& anchors && anchors->Anchors.size() == 3U
					&& std::ranges::all_of(
						anchors->Anchors,
						[](const core::comparison_anchor& aAnchor) {
							return aAnchor.Availability
								== core::anchor_availability::cache_deleted;
						}),
				"phase-19 repair keeps all tombstoned read surfaces coherent");
		};
		assertTombstones();
		const auto repairBinding = session->binding();
		expect(
			repairBinding.Catalog.Revision.value()
				== liveBinding.Catalog.Revision.value() + 1,
			"phase-19 repair retains the single tombstone revision advance");

		session.reset();
		port.reset();
		auto remade = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(remade.has_value(), "phase-19 repair catalog restarts");
		port          = std::move(*remade);
		auto reopened = port->open(open_request(602U, authority), { });
		expect(reopened.has_value(), "phase-19 repair authority reopens");
		session        = std::move(*reopened);
		auto recovered = session->recovery_page(std::nullopt, 100U, { });
		expect(
			session->binding() == repairBinding && recovered
				&& recovered->Operations.size() == 1U
				&& std::get<app::maintenance_operation_record>(
					   recovered->Operations.front())
						.RepairFromPhase
					== app::epoch_catalog_operation_phase::
						catalog_tombstoned_cleanup_pending,
			"phase-19 repair origin and tombstoned binding survive restart");
		assertTombstones();
	}

	void cache_delete_supersession_exhaustion_contract()
	{
		temporary_root root{ "cache-supersession-exhaustion" };
		auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
		expect(made.has_value(), "supersession-exhaustion fixture bootstraps");
		auto       port      = std::move(*made);
		const auto authority = authority_key(
			0xF200000000000036ULL,
			0xF200000000000037ULL,
			4272U);
		auto opened = port->open(open_request(611U, authority), { });
		expect(opened.has_value(), "supersession-exhaustion authority opens");
		auto       session = std::move(*opened);
		const auto epoch   = complete_append(
			*session,
			core::epoch_ordinal{ 1 },
			120U,
			core::application_session_id{ 611U },
			true);
		const auto review     = review_association(121U, epoch.Mutation.Epoch.Head);
		auto       associated = session->associate_review(
			app::associate_review_request{
				.Catalog          = session->binding().Catalog,
				.Review           = review,
				.AcceptedSnapshot = core::provider_snapshot_id{
					0xFC00000000000121ULL },
			},
			{ });
		expect(
			associated && associated->Changed,
			"supersession-exhaustion fixture links a review");

		const auto repairId = operation(122U);
		auto       reserved = session->reserve_append(
			app::append_reservation_request{
				.Context = context(
					session->binding(),
					repairId,
					revision(122U),
					122U,
					core::application_session_id{ 611U }),
				.ProposedOrdinal = core::epoch_ordinal{ 2 },
			},
			{ });
		expect(reserved.has_value(), "superseded append reserves");
		auto spooled = session->record_spool(spool_request(*reserved, 122U), { });
		expect(spooled.has_value(), "superseded append retains recovery input");
		auto repaired = session->transition_operation(
			app::operation_transition_request{
				.Catalog                   = session->binding().Catalog,
				.Operation                 = repairId,
				.Kind                      = app::epoch_catalog_operation_kind::append,
				.Expected                  = app::epoch_catalog_operation_phase::spool_recorded,
				.Next                      = app::epoch_catalog_operation_phase::repair_required,
				.Evidence                  = app::epoch_catalog_transition_evidence::repair_ambiguity,
				.ExpectedOperationRevision = spooled->OperationRevision,
			},
			{ });
		expect(repaired.has_value(), "superseded append enters repair");
		{
			const direct_database database{ database_path(root.path()), true };
			database.execute(
				std::string{ "UPDATE operation SET operation_revision=" }
				+ std::to_string(std::numeric_limits<std::int64_t>::max())
				+ " WHERE operation=x'" + hex_identity(repairId) + "'");
		}

		auto impact = session->cache_delete_impact(
			session->binding().Authority.Cache, { });
		expect(impact.has_value(), "repair-bearing cache impact loads");
		const auto deleteId = operation(123U);
		auto       begun    = session->begin_cache_delete(
			app::begin_cache_delete_request{
				.Operation = deleteId,
				.Impact    = *impact,
			},
			{ });
		expect(begun.has_value(), "repair-bearing cache delete seals");
		auto renaming = session->transition_operation(
			app::operation_transition_request{
				.Catalog   = session->binding().Catalog,
				.Operation = deleteId,
				.Kind      = app::epoch_catalog_operation_kind::cache_delete,
				.Expected  = app::epoch_catalog_operation_phase::
					cache_delete_intent_sealed,
				.Next = app::epoch_catalog_operation_phase::
					quarantine_rename_in_progress,
				.Evidence                  = app::epoch_catalog_transition_evidence::catalog_transaction,
				.ExpectedOperationRevision = begun->OperationRevision,
			},
			{ });
		expect(renaming.has_value(), "repair-bearing cache rename starts");
		const auto& renamingRecord = std::get<app::maintenance_operation_record>(*renaming);
		auto        quarantined    = session->transition_operation(
			app::operation_transition_request{
				.Catalog   = session->binding().Catalog,
				.Operation = deleteId,
				.Kind      = app::epoch_catalog_operation_kind::cache_delete,
				.Expected  = app::epoch_catalog_operation_phase::
					quarantine_rename_in_progress,
				.Next                      = app::epoch_catalog_operation_phase::quarantined,
				.Evidence                  = app::epoch_catalog_transition_evidence::exact_quarantine,
				.ExpectedOperationRevision = renamingRecord.OperationRevision,
			},
			{ });
		expect(quarantined.has_value(), "repair-bearing cache quarantines");
		const auto& quarantinedRecord = std::get<app::maintenance_operation_record>(*quarantined);

		const auto prScope     = hex_identity(session->binding().Authority.Scope);
		const auto cacheScope  = hex_identity(session->binding().Authority.Cache);
		const auto fingerprint = [&](const direct_database& aDatabase) {
			return std::array<std::int64_t, 11U>{
				aDatabase.integer(
					std::string{ "SELECT operation_revision FROM operation WHERE "
								 "operation=x'" }
					+ hex_identity(repairId) + "'"),
				aDatabase.integer(
					std::string{ "SELECT phase * 10 + terminal FROM operation "
								 "WHERE operation=x'" }
					+ hex_identity(repairId) + "'"),
				aDatabase.integer(
					std::string{ "SELECT count(*) FROM append_receipt WHERE "
								 "operation=x'" }
					+ hex_identity(repairId) + "'"),
				aDatabase.integer(
					std::string{ "SELECT count(*) FROM append_inventory WHERE "
								 "operation=x'" }
					+ hex_identity(repairId) + "'"),
				aDatabase.integer(
					std::string{ "SELECT catalog_revision FROM pull_request_scope "
								 "WHERE pr_scope=x'" }
					+ prScope + "'"),
				aDatabase.integer(
					std::string{ "SELECT lifecycle FROM pull_request_scope WHERE "
								 "pr_scope=x'" }
					+ prScope + "'"),
				aDatabase.integer(
					std::string{ "SELECT sum(availability) FROM epoch_anchor WHERE "
								 "pr_scope=x'" }
					+ prScope + "'"),
				aDatabase.integer(
					std::string{ "SELECT sum(availability) FROM review_association "
								 "WHERE pr_scope=x'" }
					+ prScope + "'"),
				aDatabase.integer(
					std::string{ "SELECT lifecycle FROM repository_scope WHERE "
								 "cache_scope=x'" }
					+ cacheScope + "'"),
				aDatabase.integer(
					std::string{ "SELECT operation_revision FROM operation WHERE "
								 "operation=x'" }
					+ hex_identity(deleteId) + "'"),
				aDatabase.integer(
					std::string{ "SELECT phase * 10 + terminal FROM operation "
								 "WHERE operation=x'" }
					+ hex_identity(deleteId) + "'"),
			};
		};
		std::array<std::int64_t, 11U> before{ };
		{
			const direct_database database{ database_path(root.path()) };
			before = fingerprint(database);
		}
		auto rejected = session->finalize_cache_delete(
			app::finalize_cache_delete_request{
				.Transition = app::operation_transition_request{
					.Catalog   = session->binding().Catalog,
					.Operation = deleteId,
					.Kind      = app::epoch_catalog_operation_kind::cache_delete,
					.Expected  = app::epoch_catalog_operation_phase::quarantined,
					.Next      = app::epoch_catalog_operation_phase::
						catalog_tombstoned_cleanup_pending,
					.Evidence = app::epoch_catalog_transition_evidence::
						catalog_transaction,
					.ExpectedOperationRevision = quarantinedRecord.OperationRevision,
				},
			},
			{ });
		expect_failure(
			rejected,
			app::epoch_catalog_failure::operation_conflict,
			"saturated repair supersession fails with a typed conflict");
		{
			const direct_database database{ database_path(root.path()) };
			expect(
				fingerprint(database) == before,
				"failed supersession changes neither repair input, PR revision and "
				"lifecycle, anchors, reviews, repository, nor delete operation");
		}
	}

	void repair_fail_closed_recovery_contract()
	{
		{
			temporary_root root{ "append-repair" };
			auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(made.has_value(), "append-repair fixture bootstraps");
			auto port   = std::move(*made);
			auto opened = port->open(open_request(401U), { });
			expect(opened.has_value(), "append-repair authority opens");
			auto session  = std::move(*opened);
			auto reserved = session->reserve_append(
				app::append_reservation_request{
					.Context = context(
						session->binding(),
						operation(90U),
						revision(90U),
						90U,
						core::application_session_id{ 401U }),
					.ProposedOrdinal = core::epoch_ordinal{ 1 },
				},
				{ });
			expect(reserved.has_value(), "append repair operation reserves");
			auto spooled = session->record_spool(spool_request(*reserved, 90U), { });
			expect(spooled.has_value(), "append repair input records durably");
			const auto repairTransition = app::operation_transition_request{
				.Catalog                   = session->binding().Catalog,
				.Operation                 = spooled->Context.Operation,
				.Kind                      = app::epoch_catalog_operation_kind::append,
				.Expected                  = app::epoch_catalog_operation_phase::spool_recorded,
				.Next                      = app::epoch_catalog_operation_phase::repair_required,
				.Evidence                  = app::epoch_catalog_transition_evidence::repair_ambiguity,
				.ExpectedOperationRevision = spooled->OperationRevision,
			};
			auto repaired = session->transition_operation(repairTransition, { });
			expect(repaired.has_value(), "append enters durable repair state");
			const auto& repairRecord = std::get<app::append_operation_record>(*repaired);
			expect(
				repairRecord.Phase
						== app::epoch_catalog_operation_phase::repair_required
					&& repairRecord.RepairFromPhase
						== app::epoch_catalog_operation_phase::spool_recorded
					&& !repairRecord.RecoveryInputCompacted
					&& repairRecord.Spool == spooled->Spool
					&& repairRecord.Preimport == spooled->Preimport
					&& direct_count(
						   database_path(root.path()),
						   "SELECT count(*) FROM append_receipt")
						== 1
					&& direct_count(
						   database_path(root.path()),
						   "SELECT count(*) FROM append_inventory")
						== 1,
				"append repair records its origin and retains exact recovery input");
			expect_failure(
				session->snapshot({ }),
				app::epoch_catalog_failure::repair_required,
				"append repair fail-closes ordinary PR reads");
			auto page = session->recovery_page(std::nullopt, 100U, { });
			expect(
				page && page->Operations.size() == 1U
					&& std::get<app::append_operation_record>(
						   page->Operations.front())
							.RepairFromPhase
						== app::epoch_catalog_operation_phase::spool_recorded
					&& std::get<app::append_operation_record>(
						   page->Operations.front())
							.Spool
						== repairRecord.Spool,
				"append repair preserves exact scoped recovery evidence");
			const auto binding = session->binding();
			session.reset();
			port.reset();
			auto remade = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(remade.has_value(), "append-repair catalog restarts");
			port          = std::move(*remade);
			auto reopened = port->open(open_request(402U), { });
			expect(reopened.has_value(), "append-repair authority reopens");
			session        = std::move(*reopened);
			auto recovered = session->recovery_page(std::nullopt, 100U, { });
			expect(
				session->binding() == binding && recovered
					&& recovered->Operations.size() == 1U
					&& std::get<app::append_operation_record>(
						   recovered->Operations.front())
							.RepairFromPhase
						== app::epoch_catalog_operation_phase::spool_recorded
					&& std::get<app::append_operation_record>(
						   recovered->Operations.front())
							.Preimport
						== repairRecord.Preimport
					&& direct_count(
						   database_path(root.path()),
						   "SELECT count(*) FROM append_inventory")
						== 1,
				"append repair origin, authority, and exact input survive restart");
		}

		{
			temporary_root root{ "prune-repair" };
			auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(made.has_value(), "prune-repair fixture bootstraps");
			auto port   = std::move(*made);
			auto opened = port->open(open_request(411U), { });
			expect(opened.has_value(), "prune-repair authority opens");
			auto       session = std::move(*opened);
			const auto epoch   = complete_append(
				*session,
				core::epoch_ordinal{ 1 },
				91U,
				core::application_session_id{ 411U },
				true);
			const auto retained = app::git_retained_ref_pair{
				.Scope   = session->binding().Authority.Scope,
				.Ordinal = epoch.Mutation.Epoch.Id.Ordinal,
				.Base    = epoch.Mutation.Epoch.Base,
				.Head    = epoch.Mutation.Epoch.Head,
			};
			const auto pruneFact = app::prune_epoch_fact{
				.Epoch    = epoch.Mutation.Epoch.Id,
				.Retained = retained,
				.BaseRef  = app::epoch_catalog_ref_identity{
					.Authority = session->binding().Authority.Scope,
					.Ordinal   = epoch.Mutation.Epoch.Id.Ordinal,
				},
				.HeadRef = app::epoch_catalog_ref_identity{
					.Authority = session->binding().Authority.Scope,
					.Ordinal   = epoch.Mutation.Epoch.Id.Ordinal,
					.Head      = true,
				},
			};
			auto begun = session->begin_prune(
				app::begin_prune_request{
					.Catalog   = session->binding().Catalog,
					.Operation = operation(92U),
					.Epochs    = { pruneFact },
				},
				{ });
			expect(begun.has_value(), "prune repair intent begins");
			auto deleting = session->transition_operation(
				app::operation_transition_request{
					.Catalog   = session->binding().Catalog,
					.Operation = begun->Operation,
					.Kind      = app::epoch_catalog_operation_kind::prune,
					.Expected  = app::epoch_catalog_operation_phase::prune_intent,
					.Next      = app::epoch_catalog_operation_phase::
						ref_delete_in_progress,
					.Evidence = app::epoch_catalog_transition_evidence::
						catalog_transaction,
					.ExpectedOperationRevision = begun->OperationRevision,
				},
				{ });
			expect(deleting.has_value(), "prune crosses its cancellation point");
			const auto* deletingRecord = std::get_if<app::maintenance_operation_record>(&*deleting);
			expect(deletingRecord != nullptr, "prune recovery remains typed");
			std::stop_source cancelled;
			cancelled.request_stop();
			auto repaired = session->transition_operation(
				app::operation_transition_request{
					.Catalog   = session->binding().Catalog,
					.Operation = deletingRecord->Operation,
					.Kind      = app::epoch_catalog_operation_kind::prune,
					.Expected  = app::epoch_catalog_operation_phase::
						ref_delete_in_progress,
					.Next                      = app::epoch_catalog_operation_phase::repair_required,
					.Evidence                  = app::epoch_catalog_transition_evidence::repair_ambiguity,
					.ExpectedOperationRevision = deletingRecord->OperationRevision,
				},
				cancelled.get_token());
			expect(repaired.has_value(), "post-PONR prune repair ignores cancellation");
			const auto& repairRecord = std::get<app::maintenance_operation_record>(*repaired);
			expect(
				repairRecord.RepairFromPhase
					== app::epoch_catalog_operation_phase::
						ref_delete_in_progress,
				"prune repair records its post-PONR origin");
			expect_failure(
				session->snapshot({ }),
				app::epoch_catalog_failure::repair_required,
				"prune repair fail-closes ordinary PR reads");
			auto page = session->recovery_page(std::nullopt, 100U, { });
			expect(
				page && page->Operations.size() == 1U
					&& std::get<app::maintenance_operation_record>(
						   page->Operations.front())
							.Epochs
						== std::vector{ pruneFact },
				"prune repair retains exact refs and epoch recovery evidence");
		}

		{
			temporary_root root{ "cache-repair" };
			auto           made = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(made.has_value(), "cache-repair fixture bootstraps");
			auto port   = std::move(*made);
			auto opened = port->open(open_request(421U), { });
			expect(opened.has_value(), "cache-repair authority opens");
			auto session = std::move(*opened);
			(void)complete_append(
				*session,
				core::epoch_ordinal{ 1 },
				93U,
				core::application_session_id{ 421U },
				true);
			auto impact = session->cache_delete_impact(
				session->binding().Authority.Cache, { });
			expect(impact.has_value(), "cache-repair impact loads");
			auto begun = session->begin_cache_delete(
				app::begin_cache_delete_request{
					.Operation = operation(94U),
					.Impact    = *impact,
				},
				{ });
			expect(begun.has_value(), "cache-repair delete seal begins");
			auto renaming = session->transition_operation(
				app::operation_transition_request{
					.Catalog   = session->binding().Catalog,
					.Operation = begun->Operation,
					.Kind      = app::epoch_catalog_operation_kind::cache_delete,
					.Expected  = app::epoch_catalog_operation_phase::
						cache_delete_intent_sealed,
					.Next = app::epoch_catalog_operation_phase::
						quarantine_rename_in_progress,
					.Evidence = app::epoch_catalog_transition_evidence::
						catalog_transaction,
					.ExpectedOperationRevision = begun->OperationRevision,
				},
				{ });
			expect(renaming.has_value(), "cache repair reaches rename PONR");
			const auto* renamingRecord = std::get_if<app::maintenance_operation_record>(&*renaming);
			expect(renamingRecord != nullptr, "cache rename recovery is typed");
			auto repaired = session->transition_operation(
				app::operation_transition_request{
					.Catalog   = session->binding().Catalog,
					.Operation = renamingRecord->Operation,
					.Kind      = app::epoch_catalog_operation_kind::cache_delete,
					.Expected  = app::epoch_catalog_operation_phase::
						quarantine_rename_in_progress,
					.Next                      = app::epoch_catalog_operation_phase::repair_required,
					.Evidence                  = app::epoch_catalog_transition_evidence::repair_ambiguity,
					.ExpectedOperationRevision = renamingRecord->OperationRevision,
				},
				{ });
			expect(repaired.has_value(), "cache delete enters repair state");
			const auto& repairRecord = std::get<app::maintenance_operation_record>(*repaired);
			expect(
				repairRecord.RepairFromPhase
					== app::epoch_catalog_operation_phase::
						quarantine_rename_in_progress,
				"cache repair records its post-PONR origin");
			expect_failure(
				session->snapshot({ }),
				app::epoch_catalog_failure::operation_conflict,
				"cache repair fail-closes child reads");
			const auto binding = session->binding();
			session.reset();
			port.reset();
			auto remade = sqlite::make_epoch_catalog({ .StorageRoot = root.path() });
			expect(remade.has_value(), "cache-repair catalog restarts");
			port          = std::move(*remade);
			auto reopened = port->open(open_request(422U), { });
			expect(reopened.has_value(), "cache-repair authority reopens");
			session   = std::move(*reopened);
			auto page = session->recovery_page(std::nullopt, 100U, { });
			expect(
				session->binding() == binding && page
					&& page->Operations.size() == 1U
					&& std::get<app::maintenance_operation_record>(
						   page->Operations.front())
							.RepairFromPhase
						== app::epoch_catalog_operation_phase::
							quarantine_rename_in_progress,
				"cache repair restart preserves the cache lifetime and evidence");
		}
	}
} // namespace

int main()
try {
	historical_occurrence_atomic_recovery_contract();
	base_rewrite_association_recovery_contract(false);
	base_rewrite_association_recovery_contract(true);
	portable_occurrence_storage_contract();
	occurrence_supersession_contract();
	lossless_occurrence_migration_contract(false);
	lossless_occurrence_migration_contract(true);
	append_snapshot_history_anchor_contract();
	receiving_ownership_migration_and_restart_contract();
	reservation_conflict_and_never_reuse_contract();
	canonical_review_read_contract();
	ordinal_exhaustion_snapshot_contract();
	stale_session_and_association_noop_contract();
	creation_validation_atomicity_contract();
	install_lineage_durability_contract();
	sparse_spool_restart_contract();
	sparse_install_restart_and_migration_contract(false);
	sparse_install_restart_and_migration_contract(true);
	semantic_corruption_fail_closed_contract();
	composite_scope_integrity_contract();
	revision_exhaustion_zero_mutation_contract();
	nonterminal_revision_reserve_contract();
	quota_boundaries_contract();
	bounded_wal_contract();
	recovery_paging_and_phase_cas_contract();
	tombstoned_cache_repair_contract();
	cache_delete_supersession_exhaustion_contract();
	cache_delete_abort_restores_active_contract();
	exact_cache_delete_intent_contract();
	source_observation_restart_contract();
	retention_rehydration_restart_contract();
	no_latest_reparent_contract();
	prune_replay_revision_identity_contract();
	prune_expected_availability_contract();
	repair_fail_closed_recovery_contract();
	std::cout << "sqlite-epoch-catalog-behavior-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
