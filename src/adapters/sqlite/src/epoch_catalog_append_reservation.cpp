#include "bpr/adapters/sqlite/epoch_catalog.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/epoch_transition.hpp"
#include "catalog_database.hpp"
#include "epoch_catalog_support.hpp"

#include <array>
#include <cstddef>
#include <cstring>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <sqlite3.h>
#include <stop_token>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::adapters::sqlite::epoch_catalog_detail {
	namespace {
		[[nodiscard]] auto read_pack(
			::sqlite3_stmt* aQuery, const int aAlgorithmColumn, const int aDigestColumn)
			-> std::optional<app::git_pack_oid>
		{
			if (::sqlite3_column_type(aQuery, aAlgorithmColumn) != SQLITE_INTEGER) {
				return std::nullopt;
			}
			const auto algorithm = static_cast<core::git_hash_algorithm>(
				::sqlite3_column_int(aQuery, aAlgorithmColumn));
			const auto size = algorithm == core::git_hash_algorithm::sha1 ? 20U
				: algorithm == core::git_hash_algorithm::sha256           ? 32U
																		  : 0U;
			if (size == 0U || ::sqlite3_column_type(aQuery, aDigestColumn) != SQLITE_BLOB
				|| ::sqlite3_column_bytes(aQuery, aDigestColumn)
					!= static_cast<int>(size)) {
				return std::nullopt;
			}
			app::git_pack_oid output{ .Algorithm = algorithm };
			const auto*       source = static_cast<const std::uint8_t*>(
				::sqlite3_column_blob(aQuery, aDigestColumn));
			if (source == nullptr) {
				return std::nullopt;
			}
			std::memcpy(output.Digest.data(), source, size);
			return app::valid(output) ? std::optional{ output } : std::nullopt;
		}

		[[nodiscard]] auto valid_append_context(
			const app::append_operation_context& aContext,
			const app::epoch_catalog_binding&    aBinding,
			const core::application_session_id   aSession) noexcept -> bool
		{
			return aContext.ApplicationSession == aSession
				&& aContext.Request.value() != 0U && aContext.Generation.value() != 0U
				&& aContext.Operation.valid() && core::valid(aContext.Revision)
				&& aContext.Revision.Base.algorithm() == aBinding.ObjectFormat
				&& (!aContext.Revision.CurrentViewerReview
					|| aContext.Revision.CurrentViewerReview->Actor
						== aBinding.Authority.Viewer)
				&& aContext.Catalog == aBinding.Catalog;
		}
	}

	auto sqlite_epoch_catalog_session::load_append(
		const core::epoch_operation_id aOperation)
		-> result<app::append_operation_record>
	{
		auto* database = State->Database.handle();
		auto  query    = prepare(
			database,
			"SELECT o.phase, o.operation_revision, o.repair_from_phase, "
			"o.recovery_input_compacted, o.application_session, "
			"o.request_id, o.epoch_generation, o.source_observation, "
			"o.source_snapshot, o.identity_generation, "
			"o.pull_request_generation, "
			"o.expected_revision, o.reserved_ordinal, "
			"a.base_branch, a.revision_origin, a.base_algorithm, a.base_digest, "
			"a.head_algorithm, a.head_digest, a.source_review_present "
			"FROM operation o JOIN append_operation a "
			"ON a.incarnation = o.incarnation AND a.operation = o.operation "
			"WHERE o.incarnation = ?1 AND o.operation = ?2 AND o.kind = 1 "
			"AND o.cache_scope = ?3 AND o.pr_scope = ?4");
		if (!query || !bind_fixed(query->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_operation(query->Value, 2, aOperation)
			|| !bind_fixed(query->Value, 3, Binding.Authority.Cache)
			|| !bind_fixed(query->Value, 4, Binding.Authority.Scope)
			|| ::sqlite3_step(query->Value) != SQLITE_ROW) {
			return std::unexpected(
				query ? failure::operation_conflict : query.error());
		}
		const auto applicationSession = read_u64(query->Value, 4);
		const auto request            = read_u64(query->Value, 5);
		const auto generation         = read_u64(query->Value, 6);
		const auto observation        = read_fixed<core::epoch_source_observation_id>(query->Value, 7);
		const auto source             = read_u64(query->Value, 8);
		const auto identityGeneration = read_u64(query->Value, 9);
		const auto prGeneration       = read_u64(query->Value, 10);
		const auto branch             = read_fixed<core::base_branch_id>(query->Value, 13);
		const auto base               = read_oid<core::git_commit_oid>(query->Value, 15, 16);
		const auto head               = read_oid<core::git_commit_oid>(query->Value, 17, 18);
		if (!applicationSession || !request || !generation || !observation
			|| !source || !identityGeneration || !prGeneration || !branch
			|| !base || !head) {
			return std::unexpected(failure::schema_mismatch);
		}
		core::accepted_pull_request_revision accepted{
			.Source = core::pull_request_source_revision{
				.Observation           = *observation,
				.Snapshot              = core::provider_snapshot_id{ *source },
				.IdentityGeneration    = core::github_identity_generation{ *identityGeneration },
				.PullRequestGeneration = core::request_generation{ *prGeneration },
			},
			.Origin     = restored_enum<core::pull_request_revision_origin>(::sqlite3_column_int(query->Value, 14)),
			.BaseBranch = *branch,
			.Base       = *base,
			.Head       = *head,
		};
		const auto phase = static_cast<app::epoch_catalog_operation_phase>(
			::sqlite3_column_int(query->Value, 0));
		const auto operationRevision = app::epoch_catalog_operation_revision{
			::sqlite3_column_int64(query->Value, 1)
		};
		std::optional<app::epoch_catalog_operation_phase> repairFromPhase;
		if (::sqlite3_column_type(query->Value, 2) == SQLITE_INTEGER) {
			repairFromPhase = static_cast<app::epoch_catalog_operation_phase>(
				::sqlite3_column_int(query->Value, 2));
		} else if (::sqlite3_column_type(query->Value, 2) != SQLITE_NULL) {
			return std::unexpected(failure::schema_mismatch);
		}
		const auto recoveryInputCompacted = ::sqlite3_column_int(query->Value, 3);
		const auto catalogRevision        = ::sqlite3_column_int64(query->Value, 11);
		const auto ordinal                = core::epoch_ordinal{
			::sqlite3_column_int64(query->Value, 12)
		};
		const auto sourceReviewPresent = ::sqlite3_column_int(query->Value, 19);
		if (!app::valid(operationRevision) || catalogRevision < 0
			|| !core::valid(ordinal)
			|| (recoveryInputCompacted != 0 && recoveryInputCompacted != 1)
			|| (sourceReviewPresent != 0 && sourceReviewPresent != 1)
			|| ::sqlite3_step(query->Value) != SQLITE_DONE) {
			return std::unexpected(failure::schema_mismatch);
		}
		auto sourceReview = prepare(
			database,
			"SELECT review_id, actor_id, review_order, review_state, "
			"commit_algorithm, commit_digest FROM append_source_review "
			"WHERE incarnation = ?1 AND operation = ?2");
		if (!sourceReview
			|| !bind_fixed(
				sourceReview->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_operation(sourceReview->Value, 2, aOperation)) {
			return std::unexpected(
				sourceReview ? failure::unavailable : sourceReview.error());
		}
		auto sourceReviewCode = ::sqlite3_step(sourceReview->Value);
		if ((sourceReviewCode == SQLITE_ROW) != (sourceReviewPresent != 0)) {
			return std::unexpected(failure::schema_mismatch);
		}
		if (sourceReviewCode == SQLITE_ROW) {
			const auto review = read_u64(sourceReview->Value, 0);
			const auto actor  = read_u64(sourceReview->Value, 1);
			const auto commit = read_oid<core::git_commit_oid>(
				sourceReview->Value, 4, 5);
			const auto order = ::sqlite3_column_int(sourceReview->Value, 2);
			const auto state = ::sqlite3_column_int(sourceReview->Value, 3);
			if (!review || !actor
				|| *actor != Binding.Authority.Viewer.value() || !commit
				|| ::sqlite3_step(sourceReview->Value) != SQLITE_DONE) {
				return std::unexpected(failure::schema_mismatch);
			}
			accepted.CurrentViewerReview = core::current_viewer_review_candidate{
				.Review = core::github_review_id{ *review },
				.Actor  = core::github_viewer_actor_id{ *actor },
				.Order  = core::github_review_order{
					static_cast<std::uint16_t>(order) },
				.State  = restored_enum<core::github_review_state>(state),
				.Commit = *commit,
			};
		} else if (sourceReviewCode != SQLITE_DONE) {
			return std::unexpected(sql_failure(database, { }));
		}
		app::append_operation_record output{
			.Context = app::append_operation_context{
				.ApplicationSession = core::application_session_id{ *applicationSession },
				.Request            = core::epoch_request_id{ *request },
				.Generation         = core::epoch_generation{ *generation },
				.Operation          = aOperation,
				.Revision           = accepted,
				.Catalog            = core::epoch_catalog_identity{
					.Incarnation = Binding.Catalog.Incarnation,
					.Revision    = core::epoch_catalog_revision{ catalogRevision },
				},
			},
			.ReservedOrdinal        = ordinal,
			.OperationRevision      = operationRevision,
			.Phase                  = phase,
			.RepairFromPhase        = repairFromPhase,
			.RecoveryInputCompacted = recoveryInputCompacted != 0,
			.StagingBase            = app::epoch_catalog_ref_identity{
				.Authority = Binding.Authority.Scope,
				.Ordinal   = ordinal,
				.Operation = aOperation,
				.Staging   = true,
				.Head      = false,
			},
			.StagingHead = app::epoch_catalog_ref_identity{
				.Authority = Binding.Authority.Scope,
				.Ordinal   = ordinal,
				.Operation = aOperation,
				.Staging   = true,
				.Head      = true,
			},
			.FinalBase = app::epoch_catalog_ref_identity{
				.Authority = Binding.Authority.Scope,
				.Ordinal   = ordinal,
			},
			.FinalHead = app::epoch_catalog_ref_identity{
				.Authority = Binding.Authority.Scope,
				.Ordinal   = ordinal,
				.Head      = true,
			},
		};
		auto receiving = prepare(database,
								 "SELECT spool_identity, maximum_bytes FROM append_receiving WHERE incarnation = ?1 AND operation = ?2");
		if (!receiving || !bind_fixed(receiving->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_operation(receiving->Value, 2, aOperation)) {
			return std::unexpected(receiving ? failure::unavailable : receiving.error());
		}
		const auto receivingCode = ::sqlite3_step(receiving->Value);
		if (receivingCode == SQLITE_ROW) {
			app::git_spool_ownership ownership{ .Operation = aOperation };
			if (!read_blob(receiving->Value, 0, ownership.Identity.Bytes)) {
				return std::unexpected(failure::schema_mismatch);
			}
			ownership.MaximumBytes = static_cast<std::uint64_t>(::sqlite3_column_int64(receiving->Value, 1));
			if (!app::valid(ownership) || ::sqlite3_step(receiving->Value) != SQLITE_DONE) {
				return std::unexpected(failure::schema_mismatch);
			}
			output.Receiving = ownership;
		} else if (receivingCode != SQLITE_DONE) {
			return std::unexpected(sql_failure(database, { }));
		}
		auto residencyQuery = prepare(database, "SELECT residency FROM append_residency WHERE incarnation = ?1 AND operation = ?2");
		if (!residencyQuery || !bind_fixed(residencyQuery->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_operation(residencyQuery->Value, 2, aOperation)) {
			return std::unexpected(failure::unavailable);
		}
		auto       residency     = app::git_revision_residency::complete_closure;
		const auto residencyCode = ::sqlite3_step(residencyQuery->Value);
		if (residencyCode == SQLITE_ROW) {
			const auto value = ::sqlite3_column_int(residencyQuery->Value, 0);
			if (::sqlite3_column_type(residencyQuery->Value, 0) != SQLITE_INTEGER || value < 0 || value > 1
				|| ::sqlite3_step(residencyQuery->Value) != SQLITE_DONE) {
				return std::unexpected(failure::schema_mismatch);
			}
			residency = static_cast<app::git_revision_residency>(value);
		} else if (residencyCode != SQLITE_DONE) {
			return std::unexpected(sql_failure(database, { }));
		}

		auto receipt = prepare(
			database,
			"SELECT spool_identity, spool_bytes, expected_pack_algorithm, "
			"expected_pack_digest, observed_pack_algorithm, "
			"observed_pack_digest, repository_bytes, "
			"expected_pack_preexisting, inventory_count FROM append_receipt "
			"WHERE incarnation = ?1 AND operation = ?2");
		if (!receipt || !bind_fixed(receipt->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_operation(receipt->Value, 2, aOperation)) {
			return std::unexpected(receipt ? failure::unavailable : receipt.error());
		}
		auto receiptCode = ::sqlite3_step(receipt->Value);
		if (receiptCode == SQLITE_ROW) {
			std::array<std::uint8_t, 32> spoolIdentity{ };
			const auto                   expected = read_pack(receipt->Value, 2, 3);
			const auto                   observed = read_pack(receipt->Value, 4, 5);
			if (!read_blob(receipt->Value, 0, spoolIdentity) || !expected
				|| !observed) {
				return std::unexpected(failure::schema_mismatch);
			}
			output.Spool = app::durable_git_spool_fact{
				.Operation    = aOperation,
				.ExpectedPack = *expected,
				.ObservedPack = *observed,
				.Spool        = app::git_artifact_fact{
					.Role     = app::git_artifact_role::completed_spool,
					.Identity = app::git_artifact_identity{ spoolIdentity },
					.Bytes    = static_cast<std::uint64_t>(
						::sqlite3_column_int64(receipt->Value, 1)),
				},
				.Residency = residency,
			};
			app::durable_git_preimport_fact preimport{
				.Operation       = aOperation,
				.ExpectedPack    = *expected,
				.RepositoryBytes = static_cast<std::uint64_t>(
					::sqlite3_column_int64(receipt->Value, 6)),
				.ExpectedPackPreexisting = ::sqlite3_column_int(receipt->Value, 7) != 0,
			};
			const auto expectedInventoryCount = ::sqlite3_column_int64(receipt->Value, 8);
			if (expectedInventoryCount < 0
				|| expectedInventoryCount
					> static_cast<std::int64_t>(
						app::maximum_git_cache_pack_count)) {
				return std::unexpected(failure::schema_mismatch);
			}
			if (::sqlite3_step(receipt->Value) != SQLITE_DONE) {
				return std::unexpected(failure::schema_mismatch);
			}
			auto inventory = prepare(
				database,
				"SELECT position, pack_algorithm, pack_digest, pack_bytes, "
				"index_bytes "
				"FROM append_inventory WHERE incarnation = ?1 "
				"AND operation = ?2 ORDER BY position LIMIT 4097");
			if (!inventory
				|| !bind_fixed(
					inventory->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(inventory->Value, 2, aOperation)) {
				return std::unexpected(
					inventory ? failure::unavailable : inventory.error());
			}
			while (true) {
				const auto inventoryCode = ::sqlite3_step(inventory->Value);
				if (inventoryCode == SQLITE_DONE) {
					break;
				}
				if (inventoryCode != SQLITE_ROW) {
					return std::unexpected(sql_failure(database, { }));
				}
				const auto position = ::sqlite3_column_int64(inventory->Value, 0);
				const auto pack     = read_pack(inventory->Value, 1, 2);
				if (!pack
					|| position
						!= static_cast<std::int64_t>(preimport.Packs.size())) {
					return std::unexpected(failure::schema_mismatch);
				}
				if (preimport.Packs.size() >= app::maximum_git_cache_pack_count) {
					return std::unexpected(failure::schema_mismatch);
				}
				preimport.Packs.push_back(app::git_pack_inventory_entry{
					.Pack      = *pack,
					.PackBytes = static_cast<std::uint64_t>(
						::sqlite3_column_int64(inventory->Value, 3)),
					.IndexBytes = static_cast<std::uint64_t>(
						::sqlite3_column_int64(inventory->Value, 4)),
				});
			}
			if (preimport.Packs.size()
				!= static_cast<std::size_t>(expectedInventoryCount)) {
				return std::unexpected(failure::schema_mismatch);
			}
			output.Preimport = std::move(preimport);
		} else if (receiptCode != SQLITE_DONE) {
			return std::unexpected(sql_failure(database, { }));
		}
		auto install = prepare(
			database,
			"SELECT i.pack_algorithm, i.pack_digest, i.base_algorithm, "
			"i.base_digest, i.base_tree_algorithm, i.base_tree_digest, "
			"i.head_algorithm, i.head_digest, i.head_tree_algorithm, "
			"i.head_tree_digest, i.head_lineage, p.artifact_identity, "
			"p.artifact_bytes, x.artifact_identity, x.artifact_bytes "
			"FROM append_install i "
			"JOIN append_installed_artifact p ON p.incarnation = i.incarnation "
			"AND p.operation = i.operation AND p.artifact_role = 2 "
			"JOIN append_installed_artifact x ON x.incarnation = i.incarnation "
			"AND x.operation = i.operation AND x.artifact_role = 3 "
			"WHERE i.incarnation = ?1 AND i.operation = ?2");
		if (!install || !bind_fixed(install->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_operation(install->Value, 2, aOperation)) {
			return std::unexpected(install ? failure::unavailable : install.error());
		}
		const auto installCode = ::sqlite3_step(install->Value);
		if (installCode == SQLITE_ROW) {
			const auto                   pack         = read_pack(install->Value, 0, 1);
			const auto                   installBase  = read_oid<core::git_commit_oid>(install->Value, 2, 3);
			const auto                   baseTree     = read_oid<core::git_tree_oid>(install->Value, 4, 5);
			const auto                   installHead  = read_oid<core::git_commit_oid>(install->Value, 6, 7);
			const auto                   headTree     = read_oid<core::git_tree_oid>(install->Value, 8, 9);
			const auto                   lineageValue = ::sqlite3_column_int(install->Value, 10);
			std::array<std::uint8_t, 32> packIdentity{ };
			std::array<std::uint8_t, 32> indexIdentity{ };
			const auto                   packBytes  = ::sqlite3_column_int64(install->Value, 12);
			const auto                   indexBytes = ::sqlite3_column_int64(install->Value, 14);
			if (!pack || !installBase || !baseTree || !installHead || !headTree
				|| ::sqlite3_column_type(install->Value, 10) != SQLITE_INTEGER
				|| lineageValue
					< stored_enum(core::head_lineage::not_applicable)
				|| lineageValue > stored_enum(core::head_lineage::unknown)
				|| !read_blob(install->Value, 11, packIdentity)
				|| !read_blob(install->Value, 13, indexIdentity)
				|| ::sqlite3_step(install->Value) != SQLITE_DONE) {
				return std::unexpected(failure::schema_mismatch);
			}
			const auto lineage = restored_enum<core::head_lineage>(lineageValue);
			output.Install     = app::durable_git_install_fact{
				.Operation     = aOperation,
				.Pack          = *pack,
				.InstalledPack = app::git_artifact_fact{
					.Role     = app::git_artifact_role::installed_pack,
					.Identity = app::git_artifact_identity{ packIdentity },
					.Bytes    = static_cast<std::uint64_t>(packBytes),
				},
				.InstalledIndex = app::git_artifact_fact{
					.Role     = app::git_artifact_role::installed_index,
					.Identity = app::git_artifact_identity{ indexIdentity },
					.Bytes    = static_cast<std::uint64_t>(indexBytes),
				},
				.Base        = *installBase,
				.BaseTree    = *baseTree,
				.Head        = *installHead,
				.HeadTree    = *headTree,
				.HeadLineage = lineage,
				.Residency   = residency,
			};
		} else if (installCode != SQLITE_DONE) {
			return std::unexpected(sql_failure(database, { }));
		}
		auto candidate = prepare(
			database,
			"SELECT material_base_algorithm, material_base_digest, "
			"material_head_algorithm, material_head_digest, "
			"material_policy_revision, observed_at, origin, base_transition, "
			"head_transition, material_relation, primary_cause, "
			"base_availability, head_availability, candidate_review_present "
			"FROM append_hidden_candidate WHERE incarnation = ?1 "
			"AND operation = ?2");
		if (!candidate
			|| !bind_fixed(candidate->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_operation(candidate->Value, 2, aOperation)) {
			return std::unexpected(candidate ? failure::unavailable : candidate.error());
		}
		const auto candidateCode = ::sqlite3_step(candidate->Value);
		if (candidateCode == SQLITE_ROW) {
			const auto candidateReviewPresent = ::sqlite3_column_int(candidate->Value, 13);
			if (candidateReviewPresent != 0 && candidateReviewPresent != 1) {
				return std::unexpected(failure::schema_mismatch);
			}
			std::optional<core::epoch_material_identity> material;
			if (::sqlite3_column_type(candidate->Value, 0) != SQLITE_NULL) {
				const auto baseTree = read_oid<core::git_tree_oid>(candidate->Value, 0, 1);
				const auto headTree = read_oid<core::git_tree_oid>(candidate->Value, 2, 3);
				const auto policy   = read_u64(candidate->Value, 4);
				if (!baseTree || !headTree || !policy) {
					return std::unexpected(failure::schema_mismatch);
				}
				material = core::epoch_material_identity{
					.BaseTree       = *baseTree,
					.HeadTree       = *headTree,
					.PolicyRevision = core::comparison_policy_revision{ *policy },
				};
			}
			core::append_epoch_mutation mutation{
				.Epoch = core::review_epoch_summary{
					.Id = core::review_epoch_id{
						.Authority   = Binding.Authority.Scope,
						.Incarnation = Binding.Catalog.Incarnation,
						.Ordinal     = ordinal,
					},
					.Source     = accepted.Source,
					.BaseBranch = accepted.BaseBranch,
					.Base       = accepted.Base,
					.Head       = accepted.Head,
					.Material   = material,
					.Cause      = core::epoch_cause{
						.Origin   = restored_enum<core::epoch_origin>(::sqlite3_column_int(candidate->Value, 6)),
						.Base     = restored_enum<core::epoch_base_transition>(::sqlite3_column_int(candidate->Value, 7)),
						.Head     = restored_enum<core::epoch_head_transition>(::sqlite3_column_int(candidate->Value, 8)),
						.Material = restored_enum<core::epoch_material_relation>(::sqlite3_column_int(candidate->Value, 9)),
						.Primary  = restored_enum<core::epoch_primary_cause>(::sqlite3_column_int(candidate->Value, 10)),
					},
					.ObservedAt       = core::observed_utc_milliseconds{ ::sqlite3_column_int64(candidate->Value, 5) },
					.BaseAvailability = restored_enum<core::anchor_availability>(::sqlite3_column_int(candidate->Value, 11)),
					.HeadAvailability = restored_enum<core::anchor_availability>(::sqlite3_column_int(candidate->Value, 12)),
				},
			};
			auto occurrence = prepare(database, "SELECT occurrence_id FROM append_occurrence WHERE incarnation = ?1 AND operation = ?2");
			if (!occurrence || !bind_fixed(occurrence->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(occurrence->Value, 2, aOperation)) {
				return std::unexpected(failure::unavailable);
			}
			const auto occurrenceCode = ::sqlite3_step(occurrence->Value);
			if (occurrenceCode == SQLITE_ROW) {
				output.HistoricalOccurrence = read_fixed<core::epoch_occurrence_id>(occurrence->Value, 0);
				if (!output.HistoricalOccurrence || mutation.Epoch.Cause.Origin != core::epoch_origin::explicit_historical_import
					|| ::sqlite3_step(occurrence->Value) != SQLITE_DONE) {
					return std::unexpected(failure::integrity_failure);
				}
			} else if (occurrenceCode != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, { }));
			}
			if (::sqlite3_step(candidate->Value) != SQLITE_DONE) {
				return std::unexpected(failure::schema_mismatch);
			}
			auto review = prepare(
				database,
				"SELECT association, review_id, actor_id, review_order, "
				"review_state, commit_algorithm, commit_digest, availability "
				"FROM append_candidate_review WHERE incarnation = ?1 "
				"AND operation = ?2");
			if (!review
				|| !bind_fixed(review->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(review->Value, 2, aOperation)) {
				return std::unexpected(review ? failure::unavailable : review.error());
			}
			const auto reviewCode = ::sqlite3_step(review->Value);
			if ((reviewCode == SQLITE_ROW) != (candidateReviewPresent != 0)) {
				return std::unexpected(failure::schema_mismatch);
			}
			if (reviewCode == SQLITE_ROW) {
				const auto association  = read_fixed<core::review_association_id>(review->Value, 0);
				const auto reviewId     = read_u64(review->Value, 1);
				const auto actor        = read_u64(review->Value, 2);
				const auto commit       = read_oid<core::git_commit_oid>(review->Value, 5, 6);
				const auto order        = ::sqlite3_column_int(review->Value, 3);
				const auto state        = ::sqlite3_column_int(review->Value, 4);
				const auto availability = ::sqlite3_column_int(review->Value, 7);
				if (!association || !reviewId || !actor || !commit
					|| ::sqlite3_step(review->Value) != SQLITE_DONE) {
					return std::unexpected(failure::schema_mismatch);
				}
				mutation.Review = core::catalog_review_association{
					.Association = *association,
					.Review      = core::github_review_id{ *reviewId },
					.Actor       = core::github_viewer_actor_id{ *actor },
					.Order       = core::github_review_order{
						static_cast<std::uint16_t>(order) },
					.State        = restored_enum<core::github_review_state>(state),
					.Commit       = *commit,
					.Availability = restored_enum<core::anchor_availability>(availability),
				};
			} else if (reviewCode != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, { }));
			}
			output.Candidate = std::move(mutation);
		} else if (candidateCode != SQLITE_DONE) {
			return std::unexpected(sql_failure(database, { }));
		}
		const auto exactSpoolPair    = output.Spool && output.Preimport;
		const auto noSpoolPair       = !output.Spool && !output.Preimport;
		const auto coherentSpoolPair = exactSpoolPair || noSpoolPair;
		const auto exactInstall      = output.Install.has_value();
		const auto exactCandidate    = output.Candidate.has_value();
		using enum app::epoch_catalog_operation_phase;
		const auto factPhase          = output.RepairFromPhase.value_or(output.Phase);
		const auto coherentFactPrefix = coherentSpoolPair
			&& ((noSpoolPair && !exactInstall && !exactCandidate)
				|| (exactSpoolPair && !exactInstall && !exactCandidate)
				|| (exactSpoolPair && exactInstall && !exactCandidate)
				|| (exactSpoolPair && exactInstall && exactCandidate));
		const auto completeFactsValid = output.Phase == complete
			&& output.RecoveryInputCompacted && noSpoolPair && exactInstall
			&& exactCandidate;
		const auto abortedFactsValid = output.Phase == aborted
			&& output.RecoveryInputCompacted && noSpoolPair
			&& (!exactCandidate || exactInstall);
		const auto recoverableFactsValid = !output.RecoveryInputCompacted
			&& output.Phase != complete && output.Phase != aborted
			&& ((factPhase == preparing
				 && noSpoolPair && !exactInstall && !exactCandidate)
				|| (factPhase == spool_recorded && exactSpoolPair
					&& !exactInstall && !exactCandidate)
				|| ((factPhase == imported || factPhase == staging_ready)
					&& exactSpoolPair && exactInstall && !exactCandidate)
				|| ((factPhase == commit_authorized
					 || factPhase == ref_commit_in_progress
					 || factPhase == refs_committed
					 || factPhase == visible_cleanup_pending)
					&& exactSpoolPair && exactInstall && exactCandidate)
				|| (factPhase == abort_requested && coherentFactPrefix));
		const auto phaseFactsValid = completeFactsValid
			|| abortedFactsValid || recoverableFactsValid;
		if (!phaseFactsValid) {
			return std::unexpected(failure::schema_mismatch);
		}
		if (
			output.Install
			&& (output.Install->Pack.Algorithm != Binding.ObjectFormat
				|| output.Install->Base != output.Context.Revision.Base
				|| output.Install->Head != output.Context.Revision.Head
				|| (!output.RecoveryInputCompacted
					&& (!output.Spool || !output.Preimport
						|| !coherent_install_facts(
							*output.Spool,
							*output.Preimport,
							*output.Install))))) {
			return std::unexpected(failure::schema_mismatch);
		}
		if (output.Candidate) {
			const auto& epoch = output.Candidate->Epoch;
			if (
				epoch.Source != output.Context.Revision.Source
				|| epoch.BaseBranch != output.Context.Revision.BaseBranch
				|| epoch.Base != output.Context.Revision.Base
				|| epoch.Head != output.Context.Revision.Head
				|| epoch.BaseAvailability
					!= core::anchor_availability::available
				|| epoch.HeadAvailability
					!= core::anchor_availability::available
				|| !coherent_candidate_review(
					output.Candidate->Review,
					output.Context.Revision.CurrentViewerReview,
					Binding.Authority.Viewer)
				|| !epoch.Material || !output.Install
				|| epoch.Material->BaseTree != output.Install->BaseTree
				|| epoch.Material->HeadTree != output.Install->HeadTree
				|| ((output.Context.Revision.Origin
					 == core::pull_request_revision_origin::
						 explicit_historical_import)
					!= (epoch.Cause.Origin
						== core::epoch_origin::explicit_historical_import))) {
				return std::unexpected(failure::schema_mismatch);
			}
		}
		if (!app::valid(output)) {
			return std::unexpected(failure::schema_mismatch);
		}
		return output;
	}

	auto sqlite_epoch_catalog_session::reserve_append(
		const app::append_reservation_request& aRequest,
		std::stop_token                        aStop) -> result<app::append_operation_record>
	{
		if (!valid_append_context(aRequest.Context, Binding, ApplicationSession)
			|| !core::valid(aRequest.ProposedOrdinal)) {
			return std::unexpected(failure::invalid_request);
		}
		if (aStop.stop_requested()) {
			return std::unexpected(failure::cancelled);
		}
		std::scoped_lock lock{ State->Mutex };
		auto*            database = State->Database.handle();
		progress_guard   progress{ database, aStop };
		transaction      write{ database, aStop };
		if (auto begun = write.begin(); !begun) {
			return std::unexpected(begun.error());
		}
		if (auto active = require_active_pull_request_scope(
				database,
				Binding,
				aRequest.Context.Catalog.Revision,
				true,
				true);
			!active) {
			return std::unexpected(active.error());
		}
		auto existingIdentity = prepare(
			database,
			"SELECT kind FROM operation WHERE incarnation = ?1 "
			"AND operation = ?2");
		if (!existingIdentity
			|| !bind_fixed(
				existingIdentity->Value,
				1,
				Binding.Catalog.Incarnation)
			|| !bind_operation(
				existingIdentity->Value, 2, aRequest.Context.Operation)) {
			return std::unexpected(
				existingIdentity ? failure::unavailable
								 : existingIdentity.error());
		}
		const auto existingIdentityCode = ::sqlite3_step(existingIdentity->Value);
		if (existingIdentityCode == SQLITE_ROW) {
			const auto kind = ::sqlite3_column_int(existingIdentity->Value, 0);
			if (
				::sqlite3_column_type(existingIdentity->Value, 0)
					!= SQLITE_INTEGER
				|| kind < 1 || kind > 3
				|| ::sqlite3_step(existingIdentity->Value) != SQLITE_DONE) {
				return std::unexpected(failure::schema_mismatch);
			}
			if (kind == 1) {
				auto existing = load_append(aRequest.Context.Operation);
				if (
					existing && existing->Context == aRequest.Context
					&& existing->ReservedOrdinal == aRequest.ProposedOrdinal) {
					return existing;
				}
				if (!existing
					&& existing.error() != failure::operation_conflict) {
					return std::unexpected(existing.error());
				}
			}
			return std::unexpected(failure::operation_identity_conflict);
		}
		if (existingIdentityCode != SQLITE_DONE) {
			return std::unexpected(sql_failure(database, aStop));
		}

		auto usedSource = catalog_contains_source_observation(
			database,
			Binding.Catalog.Incarnation,
			aRequest.Context.Revision.Source.Observation,
			aStop);
		if (!usedSource) {
			return std::unexpected(usedSource.error());
		}
		if (*usedSource) {
			return std::unexpected(failure::source_observation_conflict);
		}
		auto scope = prepare(
			database,
			"SELECT next_ordinal, catalog_revision FROM pull_request_scope "
			"WHERE incarnation = ?1 AND pr_scope = ?2 AND lifecycle = 1");
		if (!scope || !bind_fixed(scope->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_fixed(scope->Value, 2, Binding.Authority.Scope)
			|| ::sqlite3_step(scope->Value) != SQLITE_ROW) {
			return std::unexpected(failure::authority_mismatch);
		}
		if (::sqlite3_column_type(scope->Value, 0) == SQLITE_NULL) {
			return std::unexpected(failure::ordinal_exhausted);
		}
		const auto next     = ::sqlite3_column_int64(scope->Value, 0);
		const auto revision = ::sqlite3_column_int64(scope->Value, 1);
		if (next != aRequest.ProposedOrdinal.value()
			|| revision != aRequest.Context.Catalog.Revision.value()
			|| ::sqlite3_step(scope->Value) != SQLITE_DONE) {
			return std::unexpected(failure::revision_conflict);
		}
		if (auto idle = require_no_blocking_operation(database, Binding, false);
			!idle) {
			return std::unexpected(idle.error());
		}
		auto operation = prepare(
			database,
			"INSERT INTO operation(incarnation, operation, kind, phase, "
			"last_evidence, operation_revision, cache_scope, pr_scope, "
			"expected_revision, application_session, request_id, "
			"epoch_generation, source_observation, source_snapshot, "
			"identity_generation, pull_request_generation, reserved_ordinal, "
			"detail_count, "
			"retained_epoch_count, terminal) "
			"VALUES(?1, ?2, 1, 1, 1, 1, ?3, ?4, ?5, ?6, ?7, ?8, ?9, "
			"?10, ?11, ?12, ?13, 0, 0, 0)");
		if (!operation
			|| !bind_fixed(operation->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_operation(operation->Value, 2, aRequest.Context.Operation)
			|| !bind_fixed(operation->Value, 3, Binding.Authority.Cache)
			|| !bind_fixed(operation->Value, 4, Binding.Authority.Scope)
			|| ::sqlite3_bind_int64(
				   operation->Value, 5, aRequest.Context.Catalog.Revision.value())
				!= SQLITE_OK
			|| !bind_u64(
				operation->Value, 6, aRequest.Context.ApplicationSession.value())
			|| !bind_u64(operation->Value, 7, aRequest.Context.Request.value())
			|| !bind_u64(operation->Value, 8, aRequest.Context.Generation.value())
			|| !bind_fixed(
				operation->Value,
				9,
				aRequest.Context.Revision.Source.Observation)
			|| !bind_u64(
				operation->Value,
				10,
				aRequest.Context.Revision.Source.Snapshot.value())
			|| !bind_u64(
				operation->Value,
				11,
				aRequest.Context.Revision.Source.IdentityGeneration.value())
			|| !bind_u64(
				operation->Value,
				12,
				aRequest.Context.Revision.Source.PullRequestGeneration.value())
			|| ::sqlite3_bind_int64(
				   operation->Value, 13, aRequest.ProposedOrdinal.value())
				!= SQLITE_OK
			|| ::sqlite3_step(operation->Value) != SQLITE_DONE) {
			return std::unexpected(sql_failure(
				database, { }, failure::operation_identity_conflict));
		}
		auto append = prepare(
			database,
			"INSERT INTO append_operation(incarnation, operation, base_branch, "
			"revision_origin, base_algorithm, base_digest, head_algorithm, "
			"head_digest, source_review_present, staging_ref_format, "
			"final_ref_format) "
			"VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, 1, 1)");
		if (!append || !bind_fixed(append->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_operation(append->Value, 2, aRequest.Context.Operation)
			|| !bind_fixed(append->Value, 3, aRequest.Context.Revision.BaseBranch)
			|| ::sqlite3_bind_int(
				   append->Value,
				   4,
				   stored_enum(aRequest.Context.Revision.Origin))
				!= SQLITE_OK
			|| !bind_oid(append->Value, 5, 6, aRequest.Context.Revision.Base)
			|| !bind_oid(append->Value, 7, 8, aRequest.Context.Revision.Head)
			|| ::sqlite3_bind_int(
				   append->Value,
				   9,
				   aRequest.Context.Revision.CurrentViewerReview ? 1 : 0)
				!= SQLITE_OK
			|| ::sqlite3_step(append->Value) != SQLITE_DONE) {
			return std::unexpected(sql_failure(database, aStop));
		}
		if (aRequest.Context.Revision.CurrentViewerReview) {
			const auto& review = *aRequest.Context.Revision.CurrentViewerReview;
			auto        insert = prepare(
				database,
				"INSERT INTO append_source_review(incarnation, operation, "
				"review_id, actor_id, review_order, review_state, "
				"commit_algorithm, commit_digest) "
				"VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)");
			if (!insert
				|| !bind_fixed(insert->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(insert->Value, 2, aRequest.Context.Operation)
				|| !bind_u64(insert->Value, 3, review.Review.value())
				|| !bind_u64(insert->Value, 4, review.Actor.value())
				|| ::sqlite3_bind_int(insert->Value, 5, review.Order.value())
					!= SQLITE_OK
				|| ::sqlite3_bind_int(
					   insert->Value, 6, stored_enum(review.State))
					!= SQLITE_OK
				|| !bind_oid(insert->Value, 7, 8, review.Commit)
				|| ::sqlite3_step(insert->Value) != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, { }));
			}
		}
		auto advance = prepare(
			database,
			"UPDATE pull_request_scope SET next_ordinal = "
			"CASE WHEN next_ordinal = 9223372036854775807 THEN NULL "
			"ELSE next_ordinal + 1 END "
			"WHERE incarnation = ?1 AND pr_scope = ?2 "
			"AND next_ordinal = ?3 AND catalog_revision = ?4");
		if (!advance || !bind_fixed(advance->Value, 1, Binding.Catalog.Incarnation)
			|| !bind_fixed(advance->Value, 2, Binding.Authority.Scope)
			|| ::sqlite3_bind_int64(
				   advance->Value, 3, aRequest.ProposedOrdinal.value())
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   advance->Value, 4, aRequest.Context.Catalog.Revision.value())
				!= SQLITE_OK
			|| ::sqlite3_step(advance->Value) != SQLITE_DONE
			|| ::sqlite3_changes(database) != 1) {
			return std::unexpected(failure::revision_conflict);
		}
		auto output = load_append(aRequest.Context.Operation);
		if (!output) {
			return std::unexpected(output.error());
		}
		if (auto committed = write.commit(); !committed) {
			return std::unexpected(committed.error());
		}
		return output;
	}
}
