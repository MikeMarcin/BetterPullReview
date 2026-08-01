#include "bpr/adapters/sqlite/epoch_catalog.hpp"
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

namespace
{
namespace app = bpr::application;
namespace core = bpr::core;
namespace sqlite = bpr::adapters::sqlite;
namespace test_platform = bpr::tests::sqlite_contract;

void expect(const bool condition, const std::string_view message)
{
    if(!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

template<typename Value>
void expect_failure(
    const app::epoch_catalog_result<Value>& result,
    const app::epoch_catalog_failure failure,
    const std::string_view message)
{
    expect(!result && result.error() == failure, message);
}

template<typename Identity>
[[nodiscard]] constexpr auto opaque(const std::uint8_t seed) -> Identity
{
    auto bytes = std::array<std::uint8_t, 32U>{};
    bytes[0] = seed;
    bytes[11] = static_cast<std::uint8_t>(seed ^ 0x5AU);
    return Identity{bytes};
}

template<typename Identity>
[[nodiscard]] auto hex_identity(const Identity& identity) -> std::string
{
    constexpr std::string_view Digits = "0123456789ABCDEF";
    auto output = std::string{};
    output.reserve(identity.bytes().size() * 2U);
    for(const auto byte : identity.bytes())
    {
        output.push_back(Digits[byte >> 4U]);
        output.push_back(Digits[byte & 0x0FU]);
    }
    return output;
}

template<typename Oid>
[[nodiscard]] constexpr auto oid(const std::uint8_t seed) -> Oid
{
    auto bytes = std::array<std::uint8_t, 32U>{};
    bytes[0] = seed;
    bytes[7] = static_cast<std::uint8_t>(seed ^ 0xC3U);
    return Oid{core::git_hash_algorithm::sha1, bytes};
}

[[nodiscard]] constexpr auto pack(const std::uint8_t seed)
    -> app::git_pack_oid
{
    auto digest = std::array<std::uint8_t, 32U>{};
    digest[0] = seed;
    digest[9] = static_cast<std::uint8_t>(seed ^ 0x96U);
    return {
        .Algorithm = core::git_hash_algorithm::sha1,
        .Digest = digest,
    };
}

[[nodiscard]] constexpr auto artifact(
    const app::git_artifact_role role,
    const std::uint8_t seed,
    const std::uint64_t bytes) -> app::git_artifact_fact
{
    auto identity = std::array<std::uint8_t, 32U>{};
    identity[0] = 1U;
    identity[1] = 1U;
    identity[2] = static_cast<std::uint8_t>(role);
    identity[8] = seed;
    identity[24] = static_cast<std::uint8_t>(seed ^ 0x69U);
    return {
        .Role = role,
        .Identity = app::git_artifact_identity{identity},
        .Bytes = bytes,
    };
}

[[nodiscard]] auto token_canary() -> std::array<std::uint8_t, 32U>
{
    constexpr std::string_view Canary = "BPR_TOKEN_CANARY_NEVER_DURABLE";
    auto output = std::array<std::uint8_t, 32U>{};
    for(std::size_t index = 0U; index < Canary.size(); ++index)
    {
        output[index] = static_cast<std::uint8_t>(Canary[index]);
    }
    return output;
}

class temporary_root final
{
public:
    explicit temporary_root(const std::string_view label)
    {
        const auto base = std::filesystem::temp_directory_path();
        for(std::uint32_t attempt = 0U; attempt < 1'000U; ++attempt)
        {
            auto candidate = base
                / (std::string{"bpr-sqlite-behavior-"}
                   + std::string{label} + "-"
                   + std::to_string(
                       test_platform::process_id())
                   + "-" + std::to_string(attempt));
            std::error_code error;
            if(std::filesystem::create_directory(candidate, error))
            {
                expect(
                    test_platform::make_owner_private(candidate, true),
                    "behavior-test root becomes owner-private");
                Path = std::filesystem::canonical(candidate);
                return;
            }
            expect(
                !error || error == std::errc::file_exists,
                "temporary root creation fails only for a collision");
        }
        expect(false, "a unique behavior-test root is available");
    }

    temporary_root(const temporary_root&) = delete;
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

[[nodiscard]] auto database_path(const std::filesystem::path& root)
    -> std::filesystem::path
{
    return root / "state" / "v1" / "epoch-catalog.sqlite3";
}

[[nodiscard]] auto direct_count(
    const std::filesystem::path& path,
    std::string_view sql) -> std::int64_t;

[[nodiscard]] auto authority_key(
    const std::uint64_t repository = 0xF200000000000006ULL,
    const std::uint64_t pull_request = 0xF200000000000007ULL,
    const std::uint32_t number = 4242U)
    -> app::epoch_catalog_authority_key
{
    return {
        .Profile = core::host_profile_id{0xF200000000000001ULL},
        .ProfileRevision =
            core::github_profile_revision{0xF200000000000002ULL},
        .Host = core::git_host_id{0xF200000000000003ULL},
        .HostAuthority = opaque<core::host_authority_fingerprint>(0x41U),
        .Account = core::github_account_id{0xF200000000000004ULL},
        .Viewer = core::github_viewer_actor_id{0xF200000000000005ULL},
        .Repository = core::repository_id{repository},
        .PullRequest = core::pull_request_id{pull_request},
        .Number = core::pull_request_number{number},
        .ObjectFormat = core::git_hash_algorithm::sha1,
    };
}

[[nodiscard]] auto open_request(
    const std::uint64_t session,
    const app::epoch_catalog_authority_key& authority = authority_key())
    -> app::epoch_catalog_open_request
{
    return {
        .ApplicationSession = core::application_session_id{session},
        .Authority = authority,
    };
}

[[nodiscard]] auto revision(
    const std::uint8_t seed,
    const std::optional<core::current_viewer_review_candidate>& review =
        std::nullopt) -> core::accepted_pull_request_revision
{
    return {
        .Source =
            core::pull_request_source_revision{
                .Observation = opaque<core::epoch_source_observation_id>(
                    seed),
                .Snapshot = core::provider_snapshot_id{
                    0xF300000000000000ULL + seed},
                .IdentityGeneration = core::github_identity_generation{
                    0xF400000000000000ULL + seed},
                .PullRequestGeneration = core::request_generation{
                    0xF500000000000000ULL + seed},
            },
        .Origin = core::pull_request_revision_origin::live,
        .BaseBranch = opaque<core::base_branch_id>(
            static_cast<std::uint8_t>(0x50U + seed)),
        .Base = oid<core::git_commit_oid>(
            static_cast<std::uint8_t>(0x60U + seed)),
        .Head = oid<core::git_commit_oid>(
            static_cast<std::uint8_t>(0x70U + seed)),
        .CurrentViewerReview = review,
    };
}

[[nodiscard]] auto operation(const std::uint8_t seed)
    -> core::epoch_operation_id
{
    return opaque<core::epoch_operation_id>(seed);
}

[[nodiscard]] auto context(
    const app::epoch_catalog_binding& binding,
    const core::epoch_operation_id& id,
    const core::accepted_pull_request_revision& accepted,
    const std::uint8_t seed,
    const core::application_session_id application_session)
    -> app::append_operation_context
{
    return {
        .ApplicationSession = application_session,
        .Request = core::epoch_request_id{0xF700000000000000ULL + seed},
        .Generation = core::epoch_generation{
            0xF800000000000000ULL + seed},
        .Operation = id,
        .Revision = accepted,
        .Catalog = binding.Catalog,
    };
}

[[nodiscard]] auto spool_request(
    const app::append_operation_record& record,
    const std::uint8_t seed,
    const bool overflow_inventory = false) -> app::record_spool_request
{
    const auto expected_pack = pack(static_cast<std::uint8_t>(0x20U + seed));
    auto receipt = app::completed_git_pack_receipt{
        .Operation = record.Context.Operation,
        .ExpectedPack = expected_pack,
        .ObservedPack = expected_pack,
        .Spool = artifact(
            app::git_artifact_role::completed_spool,
            seed,
            4'096U),
        .OpaqueToken = token_canary(),
    };
    auto inventory = app::git_preimport_observation{
        .Operation = record.Context.Operation,
        .ExpectedPack = expected_pack,
        .Packs =
            {
                app::git_pack_inventory_entry{
                    .Pack = expected_pack,
                    .PackBytes = overflow_inventory
                        ? std::numeric_limits<std::uint64_t>::max()
                        : 4'096U,
                    .IndexBytes = 1'024U,
                },
            },
        .RepositoryBytes = 8'192U,
        .ExpectedPackPreexisting = true,
        .OpaqueToken = token_canary(),
    };
    expect(app::valid(receipt), "ephemeral spool receipt is valid");
    expect(
        app::valid(inventory) != overflow_inventory,
        overflow_inventory
            ? "overflowing pre-import inventory is invalid"
            : "ephemeral pre-import inventory is valid");
    return {
        .Catalog = record.Context.Catalog,
        .Operation = record.Context.Operation,
        .ExpectedOperationRevision = record.OperationRevision,
        .Spool = app::durable_fact(receipt),
        .Preimport = app::durable_fact(inventory),
    };
}

[[nodiscard]] auto install_request(
    const app::append_operation_record& record,
    const std::uint8_t seed,
    const core::head_lineage lineage =
        core::head_lineage::not_applicable) -> app::record_install_request
{
    expect(
        record.Spool.has_value(),
        "install fixture follows a durable spool record");
    return {
        .Catalog = record.Context.Catalog,
        .Operation = record.Context.Operation,
        .ExpectedOperationRevision = record.OperationRevision,
        .Install =
            app::durable_git_install_fact{
                .Operation = record.Context.Operation,
                .Pack = record.Spool->ExpectedPack,
                .InstalledPack = artifact(
                    app::git_artifact_role::installed_pack,
                    static_cast<std::uint8_t>(seed + 1U),
                    4'096U),
                .InstalledIndex = artifact(
                    app::git_artifact_role::installed_index,
                    static_cast<std::uint8_t>(seed + 2U),
                    1'024U),
                .Base = record.Context.Revision.Base,
                .BaseTree = oid<core::git_tree_oid>(
                    static_cast<std::uint8_t>(0x80U + seed)),
                .Head = record.Context.Revision.Head,
                .HeadTree = oid<core::git_tree_oid>(
                    static_cast<std::uint8_t>(0x90U + seed)),
                .HeadLineage = lineage,
            },
    };
}

[[nodiscard]] auto append_record(
    app::epoch_catalog_result<app::epoch_recovery_operation> result,
    const std::string_view message) -> app::append_operation_record
{
    expect(result.has_value(), message);
    const auto* record = std::get_if<app::append_operation_record>(&*result);
    expect(record != nullptr, "operation transition returns append record");
    return *record;
}

[[nodiscard]] auto transition_append(
    app::epoch_catalog_session& session,
    const core::epoch_catalog_identity& catalog,
    const app::append_operation_record& record,
    const app::epoch_catalog_operation_phase next,
    const app::epoch_catalog_transition_evidence evidence)
    -> app::append_operation_record
{
    return append_record(
        session.transition_operation(
            app::operation_transition_request{
                .Catalog = catalog,
                .Operation = record.Context.Operation,
                .Kind = app::epoch_catalog_operation_kind::append,
                .Expected = record.Phase,
                .Next = next,
                .Evidence = evidence,
                .ExpectedOperationRevision = record.OperationRevision,
            },
            {}),
        "append phase transition succeeds");
}

[[nodiscard]] auto candidate(
    const app::append_operation_record& record,
    const std::int64_t observed_at,
    const bool initial,
    std::optional<core::catalog_review_association> review = std::nullopt)
    -> core::append_epoch_mutation
{
    expect(record.Install.has_value(), "candidate follows installed facts");
    return {
        .Epoch =
            core::review_epoch_summary{
                .Id =
                    core::review_epoch_id{
                        .Authority = record.StagingBase.Authority,
                        .Incarnation = record.Context.Catalog.Incarnation,
                        .Ordinal = record.ReservedOrdinal,
                    },
                .Source = record.Context.Revision.Source,
                .BaseBranch = record.Context.Revision.BaseBranch,
                .Base = record.Context.Revision.Base,
                .Head = record.Context.Revision.Head,
                .Material =
                    core::epoch_material_identity{
                        .BaseTree = record.Install->BaseTree,
                        .HeadTree = record.Install->HeadTree,
                        .PolicyRevision = core::comparison_policy_revision{1U},
                    },
                .Cause = initial
                    ? core::epoch_cause{
                          .Origin = core::epoch_origin::initial_observation,
                          .Base = core::epoch_base_transition::not_applicable,
                          .Head = core::epoch_head_transition::not_applicable,
                          .Material = core::epoch_material_relation::unknown,
                          .Primary =
                              core::epoch_primary_cause::initial_observation,
                      }
                    : core::epoch_cause{
                          .Origin = core::epoch_origin::live_observation,
                          .Base = core::epoch_base_transition::unchanged,
                          .Head = core::epoch_head_transition::descendant,
                          .Material = core::epoch_material_relation::different,
                          .Primary = core::epoch_primary_cause::author_upload,
                      },
                .ObservedAt = core::observed_utc_milliseconds{observed_at},
                .BaseAvailability = core::anchor_availability::available,
                .HeadAvailability = core::anchor_availability::available,
            },
        .Review = std::move(review),
    };
}

struct append_outcome
{
    core::append_epoch_mutation Mutation;
    app::catalog_commit_result Commit;
    app::append_operation_record Terminal;
};

struct append_fixture_options
{
    std::optional<core::accepted_pull_request_revision> Accepted;
    std::optional<core::epoch_cause> Cause;
};

[[nodiscard]] auto complete_append(
    app::epoch_catalog_session& session,
    const core::epoch_ordinal ordinal,
    const std::uint8_t seed,
    const core::application_session_id application_session,
    const bool initial,
    const bool prove_atomic_spool = false,
    const std::filesystem::path* inspection_root = nullptr,
    const append_fixture_options& options = {}) -> append_outcome
{
    const auto accepted = options.Accepted.value_or(revision(seed));
    auto reserved = session.reserve_append(
        app::append_reservation_request{
            .Context = context(
                session.binding(),
                operation(seed),
                accepted,
                seed,
                application_session),
            .ProposedOrdinal = ordinal,
        },
        {});
    expect(reserved.has_value(), "append ordinal reserves");
    auto record = *reserved;
    expect(
        record.ReservedOrdinal == ordinal
            && record.Phase
                == app::epoch_catalog_operation_phase::preparing,
        "reservation returns exact ordinal in preparing phase");

    auto forbidden_transition = app::operation_transition_request{
        .Catalog = session.binding().Catalog,
        .Operation = record.Context.Operation,
        .Kind = app::epoch_catalog_operation_kind::append,
        .Expected = app::epoch_catalog_operation_phase::preparing,
        .Next = app::epoch_catalog_operation_phase::spool_recorded,
        .Evidence =
            app::epoch_catalog_transition_evidence::catalog_transaction,
        .ExpectedOperationRevision = record.OperationRevision,
    };
    expect(
        !app::valid(forbidden_transition),
        "fact-bearing spool edge has no generic transition request");
    expect_failure(
        session.transition_operation(forbidden_transition, {}),
        app::epoch_catalog_failure::invalid_request,
        "adapter rejects free fact-bearing spool transition");

    if(prove_atomic_spool)
    {
        expect(
            inspection_root != nullptr,
            "atomic spool proof has a direct database path");
        const auto invalid_storage = spool_request(record, seed, true);
        const auto failed_storage =
            session.record_spool(invalid_storage, {});
        expect_failure(
            failed_storage,
            app::epoch_catalog_failure::invalid_request,
            "non-durable inventory fact is rejected before SQL");
        auto unchanged = session.recovery_page(std::nullopt, 1U, {});
        expect(
            unchanged && unchanged->Operations.size() == 1U,
            "rejected spool leaves one recoverable reservation");
        const auto& unchanged_append =
            std::get<app::append_operation_record>(
                unchanged->Operations.front());
        expect(
            unchanged_append.Phase
                    == app::epoch_catalog_operation_phase::preparing
                && !unchanged_append.Spool && !unchanged_append.Preimport,
            "rejected spool leaves phase and facts unchanged");
        expect(
            direct_count(
                database_path(*inspection_root),
                "SELECT count(*) FROM append_receipt")
                    == 0
                && direct_count(
                       database_path(*inspection_root),
                       "SELECT count(*) FROM append_inventory")
                    == 0,
            "rejected spool leaves no partial on-disk receipt or inventory");
    }

    const auto spool = spool_request(record, seed);
    auto spooled = session.record_spool(spool, {});
    expect(spooled.has_value(), "spool and inventory record atomically");
    record = *spooled;
    expect(
        record.Phase == app::epoch_catalog_operation_phase::spool_recorded
            && record.Spool == spool.Spool
            && record.Preimport == spool.Preimport,
        "spool transition returns exact durable facts");
    auto idempotent_spool = session.record_spool(spool, {});
    expect(
        idempotent_spool && *idempotent_spool == record,
        "exact spool retry is idempotent");

    const auto install = install_request(record, seed);
    auto installed = session.record_install(install, {});
    expect(installed.has_value(), "installed artifacts record atomically");
    record = *installed;
    expect(
        record.Phase == app::epoch_catalog_operation_phase::imported
            && record.Install == install.Install,
        "install facts and imported phase agree");
    auto idempotent_install = session.record_install(install, {});
    expect(
        idempotent_install && *idempotent_install == record,
        "exact install retry is idempotent");

    record = transition_append(
        session,
        session.binding().Catalog,
        record,
        app::epoch_catalog_operation_phase::staging_ready,
        app::epoch_catalog_transition_evidence::exact_staging_pair);
    const auto staged = record;
    auto idempotent_staging = session.transition_operation(
        app::operation_transition_request{
            .Catalog = session.binding().Catalog,
            .Operation = staged.Context.Operation,
            .Kind = app::epoch_catalog_operation_kind::append,
            .Expected = app::epoch_catalog_operation_phase::imported,
            .Next = app::epoch_catalog_operation_phase::staging_ready,
            .Evidence =
                app::epoch_catalog_transition_evidence::exact_staging_pair,
            .ExpectedOperationRevision =
                app::epoch_catalog_operation_revision{
                    staged.OperationRevision.value() - 1},
        },
        {});
    expect(
        idempotent_staging.has_value(),
        "exact phase CAS retry is idempotent");

    auto mutation = candidate(
        record, static_cast<std::int64_t>(1'000 + ordinal.value()), initial);
    if(options.Cause)
    {
        mutation.Epoch.Cause = *options.Cause;
    }
    auto candidate_request = app::record_candidate_request{
        .Catalog = session.binding().Catalog,
        .Operation = record.Context.Operation,
        .ExpectedOperationRevision = record.OperationRevision,
        .Candidate = mutation,
    };
    auto recorded_candidate = session.record_candidate(candidate_request, {});
    expect(recorded_candidate.has_value(), "candidate records atomically");
    record = *recorded_candidate;
    expect(
        record.Phase
                == app::epoch_catalog_operation_phase::commit_authorized
            && record.Candidate == mutation,
        "candidate and commit authorization agree");
    auto idempotent_candidate =
        session.record_candidate(candidate_request, {});
    expect(
        idempotent_candidate && *idempotent_candidate == record,
        "exact candidate retry is idempotent");

    record = transition_append(
        session,
        session.binding().Catalog,
        record,
        app::epoch_catalog_operation_phase::ref_commit_in_progress,
        app::epoch_catalog_transition_evidence::catalog_transaction);
    record = transition_append(
        session,
        session.binding().Catalog,
        record,
        app::epoch_catalog_operation_phase::refs_committed,
        app::epoch_catalog_transition_evidence::exact_final_pair);

    const auto refs = app::git_retained_ref_pair{
        .Scope = session.binding().Authority.Scope,
        .Ordinal = ordinal,
        .Base = mutation.Epoch.Base,
        .Head = mutation.Epoch.Head,
    };
    const auto finalize_request = app::finalize_append_request{
        .Catalog = session.binding().Catalog,
        .Operation = record.Context.Operation,
        .ExpectedOperationRevision = record.OperationRevision,
        .Mutation = mutation,
        .ObservedRefs = refs,
    };
    auto finalized = session.finalize_append(finalize_request, {});
    expect(finalized.has_value(), "visible epoch finalizes atomically");
    expect(
        finalized->Changed && finalized->InsertedEpoch == mutation.Epoch.Id
            && finalized->Catalog == session.binding().Catalog,
        "catalog commit reports exact inserted epoch and new revision");
    auto finalized_retry = session.finalize_append(finalize_request, {});
    expect(
        finalized_retry && *finalized_retry == *finalized,
        "exact append finalizer replay returns the original commit");

    auto recovery = session.recovery_page(std::nullopt, 100U, {});
    expect(recovery.has_value(), "post-finalize recovery page loads");
    std::optional<app::append_operation_record> cleanup_record;
    for(const auto& value : recovery->Operations)
    {
        const auto* append = std::get_if<app::append_operation_record>(&value);
        if(append != nullptr
           && append->Context.Operation == record.Context.Operation)
        {
            cleanup_record = *append;
        }
    }
    expect(
        cleanup_record.has_value()
            && cleanup_record->Phase
                == app::epoch_catalog_operation_phase::
                    visible_cleanup_pending,
        "finalize leaves one typed cleanup recovery record");
    const auto cleanup_transition = app::operation_transition_request{
        .Catalog = session.binding().Catalog,
        .Operation = cleanup_record->Context.Operation,
        .Kind = app::epoch_catalog_operation_kind::append,
        .Expected = app::epoch_catalog_operation_phase::visible_cleanup_pending,
        .Next = app::epoch_catalog_operation_phase::complete,
        .Evidence =
            app::epoch_catalog_transition_evidence::exact_cleanup_complete,
        .ExpectedOperationRevision = cleanup_record->OperationRevision,
    };
    if(inspection_root != nullptr)
    {
        expect(
            direct_count(
                database_path(*inspection_root),
                "SELECT count(*) FROM append_receipt")
                    == 1
                && direct_count(
                       database_path(*inspection_root),
                       "SELECT count(*) FROM append_inventory")
                    == 1,
            "visible cleanup retains one receipt and inventory row");
    }
    record = append_record(
        session.transition_operation(cleanup_transition, {}),
        "append cleanup reaches terminal");
    expect(
        record.Phase == app::epoch_catalog_operation_phase::complete
            && record.RecoveryInputCompacted && !record.Spool
            && !record.Preimport && record.Install && record.Candidate,
        "append cleanup compacts only terminal recovery input");
    auto cleanup_retry = session.transition_operation(cleanup_transition, {});
    expect(
        cleanup_retry
            && std::get<app::append_operation_record>(*cleanup_retry) == record
            && session.binding().Catalog == finalized->Catalog,
        "exact append cleanup replay does not advance either revision again");
    if(inspection_root != nullptr)
    {
        expect(
            direct_count(
                database_path(*inspection_root),
                "SELECT count(*) FROM append_receipt")
                    == 0
                && direct_count(
                       database_path(*inspection_root),
                       "SELECT count(*) FROM append_inventory")
                    == 0
                && direct_count(
                       database_path(*inspection_root),
                       "SELECT recovery_input_compacted FROM operation "
                       "WHERE phase = 9")
                    == 1,
            "complete append deletes receipt/inventory rows and marks compaction");
    }

    return {
        .Mutation = std::move(mutation),
        .Commit = *finalized,
        .Terminal = std::move(record),
    };
}

[[nodiscard]] auto complete_cache_delete(
    app::epoch_catalog_session& session,
    const core::epoch_operation_id& operation_id)
    -> app::cache_delete_impact
{
    auto impact = session.cache_delete_impact(
        session.binding().Authority.Cache, {});
    expect(impact.has_value(), "cache-delete fixture impact loads");
    auto begun = session.begin_cache_delete(
        app::begin_cache_delete_request{
            .Operation = operation_id,
            .Impact = *impact,
        },
        {});
    expect(begun.has_value(), "cache-delete fixture seals exact intent");
    auto renaming = session.transition_operation(
        app::operation_transition_request{
            .Catalog = session.binding().Catalog,
            .Operation = operation_id,
            .Kind = app::epoch_catalog_operation_kind::cache_delete,
            .Expected = app::epoch_catalog_operation_phase::
                cache_delete_intent_sealed,
            .Next = app::epoch_catalog_operation_phase::
                quarantine_rename_in_progress,
            .Evidence =
                app::epoch_catalog_transition_evidence::catalog_transaction,
            .ExpectedOperationRevision = begun->OperationRevision,
        },
        {});
    expect(renaming.has_value(), "cache-delete fixture starts quarantine");
    const auto& renaming_record =
        std::get<app::maintenance_operation_record>(*renaming);
    auto quarantined = session.transition_operation(
        app::operation_transition_request{
            .Catalog = session.binding().Catalog,
            .Operation = operation_id,
            .Kind = app::epoch_catalog_operation_kind::cache_delete,
            .Expected = app::epoch_catalog_operation_phase::
                quarantine_rename_in_progress,
            .Next = app::epoch_catalog_operation_phase::quarantined,
            .Evidence =
                app::epoch_catalog_transition_evidence::exact_quarantine,
            .ExpectedOperationRevision =
                renaming_record.OperationRevision,
        },
        {});
    expect(quarantined.has_value(), "cache-delete fixture quarantines");
    const auto& quarantined_record =
        std::get<app::maintenance_operation_record>(*quarantined);
    auto finalized = session.finalize_cache_delete(
        app::finalize_cache_delete_request{
            .Transition =
                app::operation_transition_request{
                    .Catalog = session.binding().Catalog,
                    .Operation = operation_id,
                    .Kind = app::epoch_catalog_operation_kind::cache_delete,
                    .Expected =
                        app::epoch_catalog_operation_phase::quarantined,
                    .Next = app::epoch_catalog_operation_phase::
                        catalog_tombstoned_cleanup_pending,
                    .Evidence = app::epoch_catalog_transition_evidence::
                        catalog_transaction,
                    .ExpectedOperationRevision =
                        quarantined_record.OperationRevision,
                },
        },
        {});
    expect(finalized.has_value(), "cache-delete fixture tombstones catalog");
    auto page = session.recovery_page(std::nullopt, 100U, {});
    expect(page.has_value(), "cache-delete cleanup page loads");
    std::optional<app::maintenance_operation_record> cleanup;
    for(const auto& value : page->Operations)
    {
        const auto* maintenance =
            std::get_if<app::maintenance_operation_record>(&value);
        if(maintenance != nullptr && maintenance->Operation == operation_id)
        {
            cleanup = *maintenance;
        }
    }
    expect(
        cleanup
            && cleanup->Phase
                == app::epoch_catalog_operation_phase::
                    catalog_tombstoned_cleanup_pending,
        "cache-delete fixture retains exact cleanup journal");
    auto completed = session.transition_operation(
        app::operation_transition_request{
            .Catalog = session.binding().Catalog,
            .Operation = operation_id,
            .Kind = app::epoch_catalog_operation_kind::cache_delete,
            .Expected = app::epoch_catalog_operation_phase::
                catalog_tombstoned_cleanup_pending,
            .Next = app::epoch_catalog_operation_phase::complete,
            .Evidence =
                app::epoch_catalog_transition_evidence::exact_cleanup_complete,
            .ExpectedOperationRevision = cleanup->OperationRevision,
        },
        {});
    expect(completed.has_value(), "cache-delete fixture cleanup completes");
    return *impact;
}

[[nodiscard]] auto anchor_identity(
    const core::review_epoch_id& epoch,
    const core::comparison_anchor_kind kind)
    -> core::comparison_anchor_identity
{
    return {
        .Authority = epoch.Authority,
        .Incarnation = epoch.Incarnation,
        .Kind = kind,
        .Epoch = epoch.Ordinal,
    };
}

[[nodiscard]] auto review_association(
    const std::uint8_t seed,
    const core::git_commit_oid& commit)
    -> core::catalog_review_association
{
    return {
        .Association = opaque<core::review_association_id>(seed),
        .Review = core::github_review_id{0xFA00000000000000ULL + seed},
        .Actor = authority_key().Viewer,
        .Order = core::github_review_order{
            static_cast<std::uint16_t>(seed)},
        .State = core::github_review_state::approved,
        .Commit = commit,
        .Availability = core::anchor_availability::available,
    };
}

class direct_database final
{
public:
    explicit direct_database(
        const std::filesystem::path& path,
        const bool writable = false)
    {
        const auto native = test_platform::sqlite_path(path);
        const auto access = writable ? SQLITE_OPEN_READWRITE
                                     : SQLITE_OPEN_READONLY;
        expect(
            ::sqlite3_open_v2(
                native.c_str(),
                &Handle,
                access | SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_NOFOLLOW,
                test_platform::sqlite_vfs())
                == SQLITE_OK
                && Handle != nullptr,
            "behavior database opens for direct contract inspection");
    }

    ~direct_database()
    {
        expect(
            ::sqlite3_close_v2(Handle) == SQLITE_OK,
            "behavior inspection database closes");
    }

    [[nodiscard]] auto integer(const std::string_view sql) const
        -> std::int64_t
    {
        const auto query = std::string{sql};
        ::sqlite3_stmt* statement = nullptr;
        const auto prepared = ::sqlite3_prepare_v3(
            Handle,
            query.c_str(),
            static_cast<int>(query.size()),
            SQLITE_PREPARE_NO_VTAB,
            &statement,
            nullptr);
        if(prepared != SQLITE_OK)
        {
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

    [[nodiscard]] auto logical_fingerprint() const -> std::string
    {
        const auto quote_identifier = [](const std::string_view identifier) {
            auto quoted = std::string{"\""};
            for(const auto character : identifier)
            {
                quoted.push_back(character);
                if(character == '"')
                {
                    quoted.push_back(character);
                }
            }
            quoted.push_back('"');
            return quoted;
        };
        const auto quote_text = [](const std::string_view value) {
            auto quoted = std::string{"'"};
            for(const auto character : value)
            {
                quoted.push_back(character);
                if(character == '\'')
                {
                    quoted.push_back(character);
                }
            }
            quoted.push_back('\'');
            return quoted;
        };
        const auto append_field = [](std::string& output,
                                     const std::string_view value) {
            output.append(std::to_string(value.size()));
            output.push_back(':');
            output.append(value);
        };
        const auto collect_first_text_column = [this](
                                                       const std::string& sql) {
            ::sqlite3_stmt* statement = nullptr;
            expect(
                ::sqlite3_prepare_v3(
                    Handle,
                    sql.c_str(),
                    static_cast<int>(sql.size()),
                    0,
                    &statement,
                    nullptr)
                    == SQLITE_OK,
                "logical fingerprint query prepares");
            auto values = std::vector<std::string>{};
            auto code = ::sqlite3_step(statement);
            while(code == SQLITE_ROW)
            {
                const auto* raw = ::sqlite3_column_text(statement, 0);
                const auto size = ::sqlite3_column_bytes(statement, 0);
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

        const auto tables = collect_first_text_column(
            "SELECT name FROM sqlite_schema WHERE type='table' "
            "AND name NOT LIKE 'sqlite_%' ORDER BY name");
        auto output = std::string{};
        for(const auto& table : tables)
        {
            append_field(output, table);
            const auto quoted_table = quote_identifier(table);
            const auto columns = collect_first_text_column(
                "SELECT name FROM pragma_table_info(" + quote_text(table)
                + ") ORDER BY cid");
            expect(!columns.empty(), "logical table has declared columns");

            auto query = std::string{"SELECT "};
            for(std::size_t index = 0U; index < columns.size(); ++index)
            {
                if(index != 0U)
                {
                    query.append(", ");
                }
                const auto quoted_column =
                    quote_identifier(columns[index]);
                query.append("typeof(");
                query.append(quoted_column);
                query.append(") || ':' || CASE typeof(");
                query.append(quoted_column);
                query.append(") WHEN 'text' THEN hex(CAST(");
                query.append(quoted_column);
                query.append(" AS BLOB)) WHEN 'blob' THEN hex(");
                query.append(quoted_column);
                query.append(") ELSE quote(");
                query.append(quoted_column);
                query.append(") END");
                append_field(output, columns[index]);
            }
            query.append(" FROM ");
            query.append(quoted_table);
            query.append(" ORDER BY ");
            for(std::size_t index = 0U; index < columns.size(); ++index)
            {
                if(index != 0U)
                {
                    query.append(", ");
                }
                query.append(quote_identifier(columns[index]));
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
            while(code == SQLITE_ROW)
            {
                output.push_back('R');
                for(std::size_t index = 0U; index < columns.size(); ++index)
                {
                    const auto column = static_cast<int>(index);
                    const auto* raw = ::sqlite3_column_text(statement, column);
                    const auto size = ::sqlite3_column_bytes(statement, column);
                    expect(
                        raw != nullptr && size >= 0,
                        "quoted logical field is text");
                    append_field(
                        output,
                        std::string_view{
                            reinterpret_cast<const char*>(raw),
                            static_cast<std::size_t>(size)});
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

    void execute(const std::string_view sql) const
    {
        const auto statement = std::string{sql};
        char* detail = nullptr;
        const auto code = ::sqlite3_exec(
            Handle, statement.c_str(), nullptr, nullptr, &detail);
        if(code != SQLITE_OK && detail != nullptr)
        {
            std::cerr << "direct SQL failure: " << detail << '\n';
        }
        ::sqlite3_free(detail);
        expect(code == SQLITE_OK, "direct contract mutation succeeds");
    }

    [[nodiscard]] auto execute_code(const std::string_view sql) const -> int
    {
        const auto statement = std::string{sql};
        char* detail = nullptr;
        const auto code = ::sqlite3_exec(
            Handle, statement.c_str(), nullptr, nullptr, &detail);
        ::sqlite3_free(detail);
        return code;
    }

    [[nodiscard]] auto foreign_key_violation_count() const -> std::size_t
    {
        ::sqlite3_stmt* statement = nullptr;
        constexpr std::string_view Query = "PRAGMA foreign_key_check";
        expect(
            ::sqlite3_prepare_v3(
                Handle,
                Query.data(),
                static_cast<int>(Query.size()),
                SQLITE_PREPARE_NO_VTAB,
                &statement,
                nullptr)
                == SQLITE_OK,
            "foreign-key inspection prepares");
        auto count = std::size_t{0};
        auto code = ::sqlite3_step(statement);
        while(code == SQLITE_ROW)
        {
            ++count;
            code = ::sqlite3_step(statement);
        }
        expect(
            code == SQLITE_DONE && ::sqlite3_finalize(statement) == SQLITE_OK,
            "foreign-key inspection completes");
        return count;
    }

    void insert_pull_request_scope_fillers(const std::size_t count) const
    {
        ::sqlite3_stmt* source = nullptr;
        constexpr std::string_view SourceSql =
            "SELECT incarnation, cache_scope, viewer_id "
            "FROM pull_request_scope LIMIT 1";
        expect(
            ::sqlite3_prepare_v3(
                Handle,
                SourceSql.data(),
                static_cast<int>(SourceSql.size()),
                SQLITE_PREPARE_NO_VTAB,
                &source,
                nullptr)
                    == SQLITE_OK
                && ::sqlite3_step(source) == SQLITE_ROW,
            "quota fixture source scope loads");
        auto incarnation = std::array<std::uint8_t, 32U>{};
        auto cache = std::array<std::uint8_t, 32U>{};
        auto viewer = std::array<std::uint8_t, 8U>{};
        const auto copy_column = [source](
                                     const int column,
                                     auto& output) {
            const auto* raw = static_cast<const std::uint8_t*>(
                ::sqlite3_column_blob(source, column));
            expect(
                raw != nullptr
                    && ::sqlite3_column_bytes(source, column)
                        == static_cast<int>(output.size()),
                "quota fixture source identity is exact BLOB storage");
            std::copy_n(raw, output.size(), output.begin());
        };
        copy_column(0, incarnation);
        copy_column(1, cache);
        copy_column(2, viewer);
        expect(
            ::sqlite3_step(source) == SQLITE_DONE
                && ::sqlite3_finalize(source) == SQLITE_OK,
            "quota fixture source scope finalizes");

        execute("BEGIN IMMEDIATE");
        ::sqlite3_stmt* insert = nullptr;
        constexpr std::string_view InsertSql =
            "INSERT INTO pull_request_scope(incarnation, pr_scope, "
            "cache_scope, viewer_id, pull_request_id, pull_request_number, "
            "next_ordinal, catalog_revision, retained_epoch_count, "
            "latest_live_ordinal, previous_distinct_head_ordinal, lifecycle, "
            "retention_rehydration_required) "
            "VALUES(?1, ?2, ?3, ?4, ?5, ?6, 1, 0, 0, NULL, NULL, 1, 0)";
        expect(
            ::sqlite3_prepare_v3(
                Handle,
                InsertSql.data(),
                static_cast<int>(InsertSql.size()),
                SQLITE_PREPARE_NO_VTAB,
                &insert,
                nullptr)
                == SQLITE_OK,
            "PR-scope quota fixture insert prepares");
        const auto encode = [](const std::uint64_t value) {
            auto bytes = std::array<std::uint8_t, 8U>{};
            for(std::size_t index = 0U; index < bytes.size(); ++index)
            {
                bytes[bytes.size() - 1U - index] =
                    static_cast<std::uint8_t>(value >> (index * 8U));
            }
            return bytes;
        };
        for(std::size_t index = 1U; index <= count; ++index)
        {
            auto scope = std::array<std::uint8_t, 32U>{};
            scope[0] = 0xD1U;
            const auto suffix = encode(static_cast<std::uint64_t>(index));
            std::copy(
                suffix.begin(), suffix.end(), scope.end() - suffix.size());
            const auto pull_request =
                encode(0xD200000000000000ULL
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
                           pull_request.data(),
                           static_cast<int>(pull_request.size()),
                           SQLITE_TRANSIENT)
                        == SQLITE_OK
                    && ::sqlite3_bind_int64(
                           insert,
                           6,
                           static_cast<std::int64_t>(100'000U + index))
                        == SQLITE_OK
                    && ::sqlite3_step(insert) == SQLITE_DONE,
                "PR-scope quota fixture row inserts");
        }
        expect(
            ::sqlite3_finalize(insert) == SQLITE_OK,
            "PR-scope quota fixture insert finalizes");
        execute("COMMIT");
    }

    void fill_retained_epoch_quota(const std::size_t target) const
    {
        expect(target >= 1U, "retained quota fixture has a source epoch");
        const auto maximum = std::to_string(target);
        execute("PRAGMA synchronous = OFF");
        execute("BEGIN IMMEDIATE");
        execute(
            std::string{
            "WITH RECURSIVE sequence(ordinal) AS ("
            "SELECT 2 UNION ALL SELECT ordinal + 1 FROM sequence "
            "WHERE ordinal < "}
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
            "WHERE ordinal < "}
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
    const std::filesystem::path& path,
    const std::string_view sql) -> std::int64_t
{
    const direct_database database{path};
    return database.integer(sql);
}

[[nodiscard]] auto catalog_logical_fingerprint(
    const std::filesystem::path& path) -> std::string
{
    const direct_database database{path};
    return database.logical_fingerprint();
}

[[nodiscard]] auto read_binary(const std::filesystem::path& path)
    -> std::string
{
    auto input = std::ifstream{path, std::ios::binary};
    expect(input.good(), "behavior database bytes are readable");
    return {
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
}

void expect_recovery_failure(
    const std::filesystem::path& root,
    const std::uint64_t application_session,
    const app::epoch_catalog_failure failure,
    const std::string_view message)
{
    auto made = sqlite::make_epoch_catalog({.StorageRoot = root});
    expect(made.has_value(), "corrupt-row fixture passes structural bootstrap");
    auto port = std::move(*made);
    auto opened = port->open(open_request(application_session), {});
    expect(opened.has_value(), "corrupt-row authority reopens");
    auto session = std::move(*opened);
    expect_failure(
        session->recovery_page(std::nullopt, 100U, {}), failure, message);
}

void append_snapshot_history_anchor_contract()
{
    temporary_root root{"append"};
    auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
    expect(made.has_value(), "append fixture catalog bootstraps");
    auto port = std::move(*made);
    constexpr auto ApplicationSession = 0xF900000000000001ULL;
    auto opened = port->open(open_request(ApplicationSession), {});
    expect(opened.has_value(), "append fixture authority opens");
    auto session = std::move(*opened);

    const auto first = complete_append(
        *session,
        core::epoch_ordinal{1},
        1U,
        core::application_session_id{ApplicationSession},
        true,
        true,
        &root.path());
    {
        direct_database database{database_path(root.path())};
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
        core::epoch_ordinal{2},
        2U,
        core::application_session_id{ApplicationSession},
        false);
    const auto third = complete_append(
        *session,
        core::epoch_ordinal{3},
        3U,
        core::application_session_id{ApplicationSession},
        false);

    auto snapshot = session->snapshot({});
    expect(snapshot.has_value(), "catalog snapshot loads");
    expect(
        snapshot->Binding == session->binding()
            && snapshot->LatestLive == third.Mutation.Epoch
            && snapshot->PreviousDistinctHead == second.Mutation.Epoch,
        "snapshot binds latest and previous distinct head to one revision");

    const auto history_revision = session->binding().Catalog;
    auto page = session->history(
        app::epoch_history_page_request{
            .Catalog = history_revision,
            .Limit = 2U,
        },
        {});
    expect(page.has_value(), "first bounded history page loads");
    expect(
        page->Catalog == history_revision && page->Epochs.size() == 2U
            && page->Epochs[0] == third.Mutation.Epoch
            && page->Epochs[1] == second.Mutation.Epoch
            && page->Continuation.has_value(),
        "history is newest-first and revision-bound");
    const auto saved_cursor = page->Continuation;
    auto next_page = session->history(
        app::epoch_history_page_request{
            .Catalog = history_revision,
            .Cursor = page->Continuation,
            .Limit = 2U,
        },
        {});
    expect(next_page.has_value(), "history continuation loads");
    expect(
        next_page->Epochs.size() == 1U
            && next_page->Epochs.front() == first.Mutation.Epoch
            && !next_page->Continuation,
        "history continuation has no duplicates or omissions");

    const auto first_base = anchor_identity(
        first.Mutation.Epoch.Id, core::comparison_anchor_kind::epoch_base);
    const auto first_head = anchor_identity(
        first.Mutation.Epoch.Id, core::comparison_anchor_kind::epoch_head);
    auto anchors = session->exact_anchors(
        app::exact_anchor_lookup_request{
            .Catalog = session->binding().Catalog,
            .Anchors = {first_base, first_head},
        },
        {});
    expect(anchors.has_value(), "exact anchor pair loads");
    expect(
        anchors->Anchors.size() == 2U
            && anchors->Anchors[0].Identity == first_base
            && anchors->Anchors[0].Commit == first.Mutation.Epoch.Base
            && anchors->Anchors[1].Identity == first_head
            && anchors->Anchors[1].Commit == first.Mutation.Epoch.Head,
        "exact anchors preserve stable semantic identity and OIDs");

    const auto review = review_association(20U, second.Mutation.Epoch.Head);
    auto associated = session->associate_review(
        app::associate_review_request{
            .Catalog = session->binding().Catalog,
            .Review = review,
            .AcceptedSnapshot = core::provider_snapshot_id{
                0xFC00000000000001ULL},
        },
        {});
    expect(associated.has_value(), "new review association commits");
    expect(
        associated->Changed
            && associated->Catalog.Revision.value()
                == history_revision.Revision.value() + 1,
        "changed review association advances catalog revision");
    auto associated_retry = session->associate_review(
        app::associate_review_request{
            .Catalog = session->binding().Catalog,
            .Review = review,
            .AcceptedSnapshot = core::provider_snapshot_id{
                0xFC00000000000001ULL},
        },
        {});
    expect(
        associated_retry.has_value() && !associated_retry->Changed
            && associated_retry->Catalog == associated->Catalog,
        "exact review association retry is idempotent");

    expect_failure(
        session->history(
            app::epoch_history_page_request{
                .Catalog = session->binding().Catalog,
                .Cursor = saved_cursor,
                .Limit = 2U,
            },
            {}),
        app::epoch_catalog_failure::invalid_request,
        "history cursor from an older catalog revision is rejected");

    const auto review_anchor = core::comparison_anchor_identity{
        .Authority = session->binding().Authority.Scope,
        .Incarnation = session->binding().Catalog.Incarnation,
        .Kind = core::comparison_anchor_kind::review_commit,
        .Review = review.Association,
    };
    auto exact_review = session->exact_anchors(
        app::exact_anchor_lookup_request{
            .Catalog = session->binding().Catalog,
            .Anchors = {review_anchor},
        },
        {});
    expect(
        exact_review && exact_review->Anchors.size() == 1U
            && exact_review->Anchors.front().Commit == review.Commit,
        "review anchor resolves by exact association identity");

    auto corrupt = session->mark_anchor_availability(
        app::mark_anchor_availability_request{
            .Catalog = session->binding().Catalog,
            .Changes =
                {
                    app::anchor_availability_change{
                        .Anchor = first_base,
                        .ExpectedCommit = first.Mutation.Epoch.Base,
                        .Expected = core::anchor_availability::available,
                        .Next = core::anchor_availability::corrupt,
                    },
                },
        },
        {});
    expect(
        corrupt && corrupt->Changed,
        "corrupt availability tombstone commits");
    auto corrupted_anchor = session->exact_anchors(
        app::exact_anchor_lookup_request{
            .Catalog = session->binding().Catalog,
            .Anchors = {first_base},
        },
        {});
    expect(
        corrupted_anchor
            && corrupted_anchor->Anchors.front().Availability
                == core::anchor_availability::corrupt,
        "corrupt tombstone survives exact lookup");

    const auto retained = app::git_retained_ref_pair{
        .Scope = session->binding().Authority.Scope,
        .Ordinal = second.Mutation.Epoch.Id.Ordinal,
        .Base = second.Mutation.Epoch.Base,
        .Head = second.Mutation.Epoch.Head,
    };
    const auto prune_fact = app::prune_epoch_fact{
        .Epoch = second.Mutation.Epoch.Id,
        .Retained = retained,
        .BaseRef =
            app::epoch_catalog_ref_identity{
                .Authority = session->binding().Authority.Scope,
                .Ordinal = second.Mutation.Epoch.Id.Ordinal,
            },
        .HeadRef =
            app::epoch_catalog_ref_identity{
                .Authority = session->binding().Authority.Scope,
                .Ordinal = second.Mutation.Epoch.Id.Ordinal,
                .Head = true,
            },
    };
    auto prune = session->begin_prune(
        app::begin_prune_request{
            .Catalog = session->binding().Catalog,
            .Operation = operation(40U),
            .Epochs = {prune_fact},
        },
        {});
    expect(
        prune && prune->Kind == app::epoch_catalog_operation_kind::prune
            && prune->Phase
                == app::epoch_catalog_operation_phase::prune_intent
            && prune->Epochs == std::vector{prune_fact},
        "prune begins from an exact persisted epoch/ref intent");
    auto prune_retry = session->begin_prune(
        app::begin_prune_request{
            .Catalog = session->binding().Catalog,
            .Operation = operation(40U),
            .Epochs = {prune_fact},
        },
        {});
    expect(
        prune_retry && *prune_retry == *prune,
        "exact prune-intent retry is idempotent");
    auto prune_started_result = session->transition_operation(
        app::operation_transition_request{
            .Catalog = session->binding().Catalog,
            .Operation = prune->Operation,
            .Kind = app::epoch_catalog_operation_kind::prune,
            .Expected = app::epoch_catalog_operation_phase::prune_intent,
            .Next =
                app::epoch_catalog_operation_phase::ref_delete_in_progress,
            .Evidence =
                app::epoch_catalog_transition_evidence::catalog_transaction,
            .ExpectedOperationRevision = prune->OperationRevision,
        },
        {});
    expect(prune_started_result.has_value(), "prune records ref-delete start");
    const auto* prune_started =
        std::get_if<app::maintenance_operation_record>(&*prune_started_result);
    expect(
        prune_started != nullptr
            && prune_started->Phase
                == app::epoch_catalog_operation_phase::
                    ref_delete_in_progress,
        "prune transition returns exact maintenance record");
    const auto finalize_prune_request = app::finalize_prune_request{
        .Transition =
            app::operation_transition_request{
                .Catalog = session->binding().Catalog,
                .Operation = prune_started->Operation,
                .Kind = app::epoch_catalog_operation_kind::prune,
                .Expected = app::epoch_catalog_operation_phase::
                    ref_delete_in_progress,
                .Next = app::epoch_catalog_operation_phase::
                    visible_cleanup_pending,
                .Evidence = app::epoch_catalog_transition_evidence::
                    exact_refs_absent,
                .ExpectedOperationRevision =
                    prune_started->OperationRevision,
            },
        .ObservedAbsent = {retained},
    };
    auto pruned = session->finalize_prune(finalize_prune_request, {});
    expect(
        pruned && pruned->Changed
            && pruned->Catalog == session->binding().Catalog,
        "exact absent refs atomically tombstone pruned anchors");
    auto pruned_retry =
        session->finalize_prune(finalize_prune_request, {});
    expect(
        pruned_retry && *pruned_retry == *pruned,
        "exact prune finalizer replay returns the original commit");
    auto pruned_anchors = session->exact_anchors(
        app::exact_anchor_lookup_request{
            .Catalog = session->binding().Catalog,
            .Anchors =
                {
                    anchor_identity(
                        second.Mutation.Epoch.Id,
                        core::comparison_anchor_kind::epoch_base),
                    anchor_identity(
                        second.Mutation.Epoch.Id,
                        core::comparison_anchor_kind::epoch_head),
                },
        },
        {});
    expect(
        pruned_anchors && pruned_anchors->Anchors.size() == 2U
            && pruned_anchors->Anchors[0].Availability
                == core::anchor_availability::pruned
            && pruned_anchors->Anchors[1].Availability
                == core::anchor_availability::pruned,
        "both exact anchors retain identity with pruned availability");
    auto pruned_review = session->exact_anchors(
        app::exact_anchor_lookup_request{
            .Catalog = session->binding().Catalog,
            .Anchors = {review_anchor},
        },
        {});
    auto pruned_snapshot = session->snapshot({});
    expect(
        pruned_review
            && pruned_review->Anchors.front().Availability
                == core::anchor_availability::pruned
            && pruned_snapshot && pruned_snapshot->CurrentViewerReview
            && pruned_snapshot->CurrentViewerReview->Association
                == review.Association
            && pruned_snapshot->CurrentViewerReview->Availability
                == core::anchor_availability::pruned,
        "pruning a linked epoch tombstones its review anchor and current pointer");
    auto prune_cleanup_page = session->recovery_page(std::nullopt, 100U, {});
    expect(prune_cleanup_page.has_value(), "prune cleanup is recoverable");
    std::optional<app::maintenance_operation_record> prune_cleanup;
    for(const auto& value : prune_cleanup_page->Operations)
    {
        const auto* maintenance =
            std::get_if<app::maintenance_operation_record>(&value);
        if(maintenance != nullptr && maintenance->Operation == operation(40U))
        {
            prune_cleanup = *maintenance;
        }
    }
    expect(
        prune_cleanup
            && prune_cleanup->Phase
                == app::epoch_catalog_operation_phase::
                    visible_cleanup_pending,
        "prune finalization retains bounded cleanup record");
    auto prune_complete = session->transition_operation(
        app::operation_transition_request{
            .Catalog = session->binding().Catalog,
            .Operation = prune_cleanup->Operation,
            .Kind = app::epoch_catalog_operation_kind::prune,
            .Expected = app::epoch_catalog_operation_phase::
                visible_cleanup_pending,
            .Next = app::epoch_catalog_operation_phase::complete,
            .Evidence =
                app::epoch_catalog_transition_evidence::exact_cleanup_complete,
            .ExpectedOperationRevision = prune_cleanup->OperationRevision,
        },
        {});
    expect(prune_complete.has_value(), "prune cleanup reaches terminal");

    std::stop_source cancelled;
    cancelled.request_stop();
    expect_failure(
        session->history(
            app::epoch_history_page_request{
                .Catalog = session->binding().Catalog,
                .Limit = 1U,
            },
            cancelled.get_token()),
        app::epoch_catalog_failure::cancelled,
        "pre-cancelled bounded history returns cancellation");

    const auto current_binding = session->binding();
    session.reset();
    port.reset();
    auto raw = read_binary(database_path(root.path()));
    expect(
        raw.find("BPR_TOKEN_CANARY") == std::string::npos,
        "opaque receipt and inventory tokens never enter the main database");
    const auto wal = test_platform::with_ascii_suffix(
        database_path(root.path()), "-wal");
    if(std::filesystem::exists(wal))
    {
        raw = read_binary(wal);
        expect(
            raw.find("BPR_TOKEN_CANARY") == std::string::npos,
            "opaque receipt and inventory tokens never enter the WAL");
    }

    auto reopened_port_result =
        sqlite::make_epoch_catalog({.StorageRoot = root.path()});
    expect(reopened_port_result.has_value(), "populated WAL catalog reopens");
    auto reopened_port = std::move(*reopened_port_result);
    auto reopened_result =
        reopened_port->open(open_request(0xF900000000000002ULL), {});
    expect(reopened_result.has_value(), "populated authority reopens");
    auto reopened = std::move(*reopened_result);
    expect(
        reopened->binding() == current_binding,
        "populated catalog revision and scopes survive WAL reopen");
    auto reopened_snapshot = reopened->snapshot({});
    expect(
        reopened_snapshot
            && reopened_snapshot->LatestLive == third.Mutation.Epoch
            && reopened_snapshot->CurrentViewerReview
            && reopened_snapshot->CurrentViewerReview->Association
                == review.Association
            && reopened_snapshot->CurrentViewerReview->Availability
                == core::anchor_availability::pruned,
        "epoch and pruned review tombstone survive WAL reopen");
}

void reservation_conflict_and_never_reuse_contract()
{
    temporary_root root{"writers"};
    auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
    expect(made.has_value(), "two-writer fixture bootstraps");
    auto port = std::move(*made);
    auto first_open = port->open(open_request(101U), {});
    auto second_open = port->open(open_request(102U), {});
    expect(
        first_open.has_value() && second_open.has_value(),
        "two independent sessions open one authority");
    auto first = std::move(*first_open);
    auto second = std::move(*second_open);
    auto fresh_snapshot = first->snapshot({});
    expect(
        fresh_snapshot
            && fresh_snapshot->NextOrdinal == core::epoch_ordinal{1},
        "fresh catalog exposes ordinal one as the exact next reservation");
    const auto accepted = revision(30U);
    auto unused_source = first->source_observation_exists(
        accepted.Source.Observation, {});
    expect(
        unused_source && !*unused_source,
        "indexed source probe reports a fresh observation as unused");
    expect_failure(
        first->source_observation_exists({}, {}),
        app::epoch_catalog_failure::invalid_request,
        "source probe rejects the reserved zero identity");
    const auto first_request = app::append_reservation_request{
        .Context = context(
            first->binding(),
            operation(30U),
            accepted,
            30U,
            core::application_session_id{101U}),
        .ProposedOrdinal = core::epoch_ordinal{1},
    };
    const auto second_request = app::append_reservation_request{
        .Context = context(
            second->binding(),
            operation(31U),
            revision(31U),
            31U,
            core::application_session_id{102U}),
        .ProposedOrdinal = core::epoch_ordinal{1},
    };

    std::barrier ready{3};
    std::optional<app::epoch_catalog_result<app::append_operation_record>>
        first_result;
    std::optional<app::epoch_catalog_result<app::append_operation_record>>
        second_result;
    auto first_thread = std::thread{[&] {
        ready.arrive_and_wait();
        first_result.emplace(first->reserve_append(first_request, {}));
    }};
    auto second_thread = std::thread{[&] {
        ready.arrive_and_wait();
        second_result.emplace(second->reserve_append(second_request, {}));
    }};
    ready.arrive_and_wait();
    first_thread.join();
    second_thread.join();

    expect(
        first_result.has_value() && second_result.has_value(),
        "both serialized writer attempts return");
    const auto successes = static_cast<int>(first_result->has_value())
        + static_cast<int>(second_result->has_value());
    expect(successes == 1, "exactly one writer reserves ordinal one");
    const auto& failed = first_result->has_value() ? *second_result
                                                   : *first_result;
    expect_failure(
        failed,
        app::epoch_catalog_failure::revision_conflict,
        "competing writer receives typed revision conflict");

    auto* winner = first_result->has_value() ? first.get() : second.get();
    const auto winner_session = first_result->has_value()
        ? core::application_session_id{101U}
        : core::application_session_id{102U};
    auto winner_record = first_result->has_value() ? **first_result
                                                    : **second_result;
    const auto winning_revision = winner_record.Context.Revision;
    const auto& winner_request = first_result->has_value() ? first_request
                                                            : second_request;
    auto exact_retry = winner->reserve_append(winner_request, {});
    expect(
        exact_retry && *exact_retry == winner_record,
        "exact operation reservation retry remains idempotent");
    auto durable_source = winner->source_observation_exists(
        winning_revision.Source.Observation, {});
    expect(
        durable_source && *durable_source,
        "indexed source probe includes an unpublished durable append");
    std::stop_source cancelled_probe;
    cancelled_probe.request_stop();
    expect_failure(
        winner->source_observation_exists(
            winning_revision.Source.Observation,
            cancelled_probe.get_token()),
        app::epoch_catalog_failure::cancelled,
        "pre-cancelled source probe returns cancellation");

    const auto sibling_authority = authority_key(
        authority_key().Repository.value(),
        0xF2000000000000A7ULL,
        4342U);
    auto sibling_open = port->open(open_request(103U, sibling_authority), {});
    expect(
        sibling_open.has_value(),
        "collision fixture opens a sibling PR in the catalog incarnation");
    auto sibling = std::move(*sibling_open);
    auto sibling_collision = sibling->reserve_append(
        app::append_reservation_request{
            .Context = context(
                sibling->binding(),
                operation(34U),
                winning_revision,
                34U,
                core::application_session_id{103U}),
            .ProposedOrdinal = core::epoch_ordinal{1},
        },
        {});
    expect_failure(
        sibling_collision,
        app::epoch_catalog_failure::source_observation_conflict,
        "source collision crosses PR scopes within one catalog incarnation");
    auto operation_collision = sibling->reserve_append(
        app::append_reservation_request{
            .Context = context(
                sibling->binding(),
                winner_record.Context.Operation,
                revision(35U),
                35U,
                core::application_session_id{103U}),
            .ProposedOrdinal = core::epoch_ordinal{1},
        },
        {});
    expect_failure(
        operation_collision,
        app::epoch_catalog_failure::operation_identity_conflict,
        "operation identity collision is distinct across PR scopes");
    auto reserved_snapshot = winner->snapshot({});
    expect(
        reserved_snapshot
            && reserved_snapshot->NextOrdinal == core::epoch_ordinal{2},
        "reservation advances the durable next ordinal before publication");
    auto spooled = winner->record_spool(
        spool_request(winner_record, 30U), {});
    expect(spooled.has_value(), "aborted-operation spool facts record");
    winner_record = *spooled;
    auto abort_requested = transition_append(
        *winner,
        winner->binding().Catalog,
        winner_record,
        app::epoch_catalog_operation_phase::abort_requested,
        app::epoch_catalog_transition_evidence::catalog_transaction);
    const auto abort_transition = app::operation_transition_request{
        .Catalog = winner->binding().Catalog,
        .Operation = abort_requested.Context.Operation,
        .Kind = app::epoch_catalog_operation_kind::append,
        .Expected = app::epoch_catalog_operation_phase::abort_requested,
        .Next = app::epoch_catalog_operation_phase::aborted,
        .Evidence =
            app::epoch_catalog_transition_evidence::exact_cleanup_complete,
        .ExpectedOperationRevision = abort_requested.OperationRevision,
    };
    auto aborted = append_record(
        winner->transition_operation(abort_transition, {}),
        "append abort cleanup succeeds");
    expect(
        aborted.Phase == app::epoch_catalog_operation_phase::aborted
            && aborted.RecoveryInputCompacted && !aborted.Spool
            && !aborted.Preimport,
        "aborted append compacts its recovery-only receipt and inventory");
    auto aborted_source_collision = sibling->reserve_append(
        app::append_reservation_request{
            .Context = context(
                sibling->binding(),
                operation(36U),
                winning_revision,
                36U,
                core::application_session_id{103U}),
            .ProposedOrdinal = core::epoch_ordinal{1},
        },
        {});
    expect_failure(
        aborted_source_collision,
        app::epoch_catalog_failure::source_observation_conflict,
        "terminal aborted append retains its source identity collision");
    auto aborted_retry = winner->transition_operation(abort_transition, {});
    expect(
        aborted_retry
            && std::get<app::append_operation_record>(*aborted_retry)
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
                winner_session),
            .ProposedOrdinal = core::epoch_ordinal{2},
        },
        {});
    expect(
        next && next->ReservedOrdinal == core::epoch_ordinal{2},
        "aborted ordinal is never reused");
    auto next_snapshot = winner->snapshot({});
    expect(
        next_snapshot
            && next_snapshot->NextOrdinal == core::epoch_ordinal{3},
        "aborted gap remains visible in the exact never-reused sequence");
    auto reused = winner->reserve_append(
        app::append_reservation_request{
            .Context = context(
                winner->binding(),
                operation(33U),
                revision(33U),
                33U,
                winner_session),
            .ProposedOrdinal = core::epoch_ordinal{1},
        },
        {});
    expect_failure(
        reused,
        app::epoch_catalog_failure::revision_conflict,
        "previously reserved ordinal cannot be reused");
}

void canonical_review_read_contract()
{
    temporary_root root{"canonical-review-read"};
    auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
    expect(made.has_value(), "canonical-review fixture bootstraps");
    auto port = std::move(*made);
    auto opened = port->open(open_request(111U), {});
    expect(opened.has_value(), "canonical-review authority opens");
    auto session = std::move(*opened);

    const auto accepted = revision(90U);
    const auto proposed = review_association(90U, accepted.Head);
    auto provider = core::current_viewer_review_candidate{
        .Review = proposed.Review,
        .Actor = proposed.Actor,
        .Order = proposed.Order,
        .State = proposed.State,
        .Commit = proposed.Commit,
    };
    auto request = app::canonical_review_request{
        .Catalog = session->binding().Catalog,
        .Candidate = provider,
        .ProposedAssociation = proposed.Association,
    };
    const auto before =
        catalog_logical_fingerprint(database_path(root.path()));
    auto never_observed = session->canonical_review(request, {});
    expect(
        never_observed
            && never_observed->Association == proposed.Association
            && never_observed->Availability
                == core::anchor_availability::never_observed,
        "unretained provider review receives proposed identity and an honest "
        "never-observed anchor state");

    request.ProjectedHead = app::projected_append_head{
        .Ordinal = core::epoch_ordinal{1},
        .Head = accepted.Head,
    };
    auto projected = session->canonical_review(request, {});
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
        core::epoch_ordinal{1},
        90U,
        core::application_session_id{111U},
        true,
        false,
        nullptr,
        append_fixture_options{.Accepted = accepted});
    request.Catalog = session->binding().Catalog;
    request.ProjectedHead.reset();
    auto retained = session->canonical_review(request, {});
    expect(
        retained
            && retained->Availability
                == core::anchor_availability::available
            && retained->Commit == epoch.Mutation.Epoch.Head,
        "indexed retained head canonicalizes the review without history paging");

    auto associated = session->associate_review(
        app::associate_review_request{
            .Catalog = session->binding().Catalog,
            .Review = *retained,
            .AcceptedSnapshot = accepted.Source.Snapshot,
        },
        {});
    expect(associated && associated->Changed, "review association persists");
    request.Catalog = session->binding().Catalog;
    request.ProposedAssociation =
        opaque<core::review_association_id>(91U);
    request.Candidate.State = core::github_review_state::dismissed;
    const auto before_reuse =
        catalog_logical_fingerprint(database_path(root.path()));
    auto reused = session->canonical_review(request, {});
    expect(
        reused && reused->Association == proposed.Association
            && reused->State == core::github_review_state::dismissed
            && reused->Availability
                == core::anchor_availability::available,
        "same provider review reuses durable local identity while projecting "
        "the newly observed provider facts");
    expect(
        catalog_logical_fingerprint(database_path(root.path()))
            == before_reuse,
        "stable review identity reuse remains read-only");

    auto identity_collision = request;
    identity_collision.Candidate.Review = core::github_review_id{91U};
    identity_collision.Candidate.Order = core::github_review_order{91U};
    identity_collision.ProposedAssociation = proposed.Association;
    expect_failure(
        session->canonical_review(identity_collision, {}),
        app::epoch_catalog_failure::operation_conflict,
        "new provider review cannot claim another review association identity");

    auto order_collision = identity_collision;
    order_collision.ProposedAssociation =
        opaque<core::review_association_id>(92U);
    order_collision.Candidate.Order = proposed.Order;
    expect_failure(
        session->canonical_review(order_collision, {}),
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
    temporary_root root{"ordinal-exhaustion-snapshot"};
    auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
    expect(made.has_value(), "ordinal-exhaustion fixture bootstraps");
    auto port = std::move(*made);
    auto opened = port->open(open_request(121U), {});
    expect(opened.has_value(), "ordinal-exhaustion authority opens");
    auto session = std::move(*opened);
    session.reset();
    port.reset();
    {
        const direct_database database{database_path(root.path()), true};
        database.execute(
            "UPDATE pull_request_scope SET next_ordinal = "
            "9223372036854775807");
    }
    made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
    expect(made.has_value(), "maximum-ordinal catalog reopens");
    port = std::move(*made);
    opened = port->open(open_request(122U), {});
    expect(opened.has_value(), "maximum-ordinal authority reopens");
    session = std::move(*opened);
    auto boundary = session->snapshot({});
    expect(
        boundary
            && boundary->NextOrdinal
                == core::epoch_ordinal{
                    std::numeric_limits<std::int64_t>::max()},
        "snapshot exposes the exact final reservable ordinal");

    auto reserved = session->reserve_append(
        app::append_reservation_request{
            .Context = context(
                session->binding(),
                operation(37U),
                revision(37U),
                37U,
                core::application_session_id{122U}),
            .ProposedOrdinal = core::epoch_ordinal{
                std::numeric_limits<std::int64_t>::max()},
        },
        {});
    expect(reserved.has_value(), "final ordinal reserves exactly once");
    auto exhausted = session->snapshot({});
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
                    core::application_session_id{122U}),
                .ProposedOrdinal = core::epoch_ordinal{
                    std::numeric_limits<std::int64_t>::max()},
            },
            {}),
        app::epoch_catalog_failure::ordinal_exhausted,
        "exhausted catalog rejects every later ordinal reservation");
}

void stale_session_and_association_noop_contract()
{
    temporary_root root{"stale-session"};
    auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
    expect(made.has_value(), "stale-session fixture bootstraps");
    auto port = std::move(*made);
    auto first_open = port->open(open_request(151U), {});
    auto stale_open = port->open(open_request(152U), {});
    expect(
        first_open && stale_open,
        "two sessions bind the same initial PR revision");
    auto first = std::move(*first_open);
    auto stale = std::move(*stale_open);
    const auto stale_revision = stale->binding().Catalog;

    const auto epoch = complete_append(
        *first,
        core::epoch_ordinal{1},
        45U,
        core::application_session_id{151U},
        true);
    expect(
        stale->binding().Catalog == stale_revision
            && first->binding().Catalog.Revision.value()
                == stale_revision.Revision.value() + 1,
        "one session advancing the catalog leaves the other binding stale");
    expect_failure(
        stale->history(
            app::epoch_history_page_request{
                .Catalog = stale_revision,
                .Limit = 1U,
            },
            {}),
        app::epoch_catalog_failure::revision_conflict,
        "revision-bound history rejects a stale session");
    auto refreshed = stale->snapshot({});
    expect(
        refreshed && refreshed->Binding == first->binding()
            && stale->binding() == first->binding()
            && refreshed->LatestLive == epoch.Mutation.Epoch,
        "snapshot refreshes a stale binding from one consistent read");

    const auto review = review_association(46U, epoch.Mutation.Epoch.Head);
    const auto associate_request = app::associate_review_request{
        .Catalog = first->binding().Catalog,
        .Review = review,
        .AcceptedSnapshot = core::provider_snapshot_id{
            0xFC00000000000046ULL},
    };
    auto associated = first->associate_review(associate_request, {});
    expect(
        associated && associated->Changed,
        "first session advances the review association");
    auto stale_noop = stale->associate_review(associate_request, {});
    expect_failure(
        stale_noop,
        app::epoch_catalog_failure::revision_conflict,
        "stale exact-association retry cannot masquerade as a no-op");
    auto review_refreshed = stale->snapshot({});
    expect(
        review_refreshed
            && review_refreshed->Binding == first->binding()
            && review_refreshed->CurrentViewerReview == review,
        "stale session refresh observes the committed review and revision");
    const auto exact_retry = app::associate_review_request{
        .Catalog = stale->binding().Catalog,
        .Review = review,
        .AcceptedSnapshot = associate_request.AcceptedSnapshot,
    };
    auto no_change = stale->associate_review(exact_retry, {});
    expect(
        no_change && !no_change->Changed
            && no_change->Catalog == associated->Catalog
            && direct_count(
                   database_path(root.path()),
                   "SELECT catalog_revision FROM pull_request_scope")
                == associated->Catalog.Revision.value(),
        "fresh exact association retry is a no-op with no second revision");
}

void creation_validation_atomicity_contract()
{
    temporary_root root{"creation-validation"};
    auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
    expect(made.has_value(), "creation-validation fixture bootstraps");
    auto port = std::move(*made);
    auto opened = port->open(open_request(161U), {});
    expect(opened.has_value(), "creation-validation authority opens");
    auto session = std::move(*opened);
    const auto epoch = complete_append(
        *session,
        core::epoch_ordinal{1},
        47U,
        core::application_session_id{161U},
        true);
    auto invalid_review =
        review_association(48U, epoch.Mutation.Epoch.Head);
    invalid_review.Availability = core::anchor_availability::corrupt;
    const auto before_revision = session->binding().Catalog;
    expect_failure(
        session->associate_review(
            app::associate_review_request{
                .Catalog = before_revision,
                .Review = invalid_review,
                .AcceptedSnapshot = core::provider_snapshot_id{
                    0xFC00000000000048ULL},
            },
            {}),
        app::epoch_catalog_failure::invalid_request,
        "new review association cannot begin as a destructive tombstone");
    expect(
        session->binding().Catalog == before_revision
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
                core::application_session_id{161U}),
            .ProposedOrdinal = core::epoch_ordinal{2},
        },
        {});
    expect(reserved.has_value(), "invalid-candidate append reserves");
    auto record = *reserved;
    auto spooled = session->record_spool(spool_request(record, 49U), {});
    expect(spooled.has_value(), "invalid-candidate spool records");
    record = *spooled;
    auto installed = session->record_install(install_request(record, 49U), {});
    expect(installed.has_value(), "invalid-candidate install records");
    record = transition_append(
        *session,
        session->binding().Catalog,
        *installed,
        app::epoch_catalog_operation_phase::staging_ready,
        app::epoch_catalog_transition_evidence::exact_staging_pair);
    auto invalid_endpoints = candidate(record, 2'000, false);
    invalid_endpoints.Epoch.BaseAvailability =
        core::anchor_availability::unavailable;
    expect_failure(
        session->record_candidate(
            app::record_candidate_request{
                .Catalog = session->binding().Catalog,
                .Operation = record.Context.Operation,
                .ExpectedOperationRevision = record.OperationRevision,
                .Candidate = invalid_endpoints,
            },
            {}),
        app::epoch_catalog_failure::invalid_request,
        "append candidate requires two available endpoints");
    auto invalid_candidate_review = candidate(record, 2'000, false);
    invalid_candidate_review.Review = invalid_review;
    expect_failure(
        session->record_candidate(
            app::record_candidate_request{
                .Catalog = session->binding().Catalog,
                .Operation = record.Context.Operation,
                .ExpectedOperationRevision = record.OperationRevision,
                .Candidate = invalid_candidate_review,
            },
            {}),
        app::epoch_catalog_failure::invalid_request,
        "append candidate cannot create a tombstoned review");
    auto recovery = session->recovery_page(std::nullopt, 100U, {});
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

void install_lineage_durability_contract()
{
    temporary_root root{"install-lineage"};
    auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
    expect(made.has_value(), "install-lineage fixture bootstraps");
    auto port = std::move(*made);
    auto opened = port->open(open_request(171U), {});
    expect(opened.has_value(), "install-lineage authority opens");
    auto session = std::move(*opened);

    auto reserved = session->reserve_append(
        app::append_reservation_request{
            .Context = context(
                session->binding(),
                operation(50U),
                revision(50U),
                50U,
                core::application_session_id{171U}),
            .ProposedOrdinal = core::epoch_ordinal{1},
        },
        {});
    expect(reserved.has_value(), "install-lineage append reserves");
    auto spooled = session->record_spool(spool_request(*reserved, 50U), {});
    expect(spooled.has_value(), "install-lineage spool records");

    const auto request = install_request(
        *spooled, 50U, core::head_lineage::descendant);
    auto invalid = request;
    invalid.Install.HeadLineage = static_cast<core::head_lineage>(0xFFU);
    expect_failure(
        session->record_install(invalid, {}),
        app::epoch_catalog_failure::invalid_request,
        "install rejects an unknown durable head-lineage encoding");

    auto installed = session->record_install(request, {});
    expect(
        installed && installed->Install
            && installed->Install->HeadLineage
                == core::head_lineage::descendant,
        "install result preserves the exact durable head lineage");
    auto retry = session->record_install(request, {});
    expect(
        retry && *retry == *installed,
        "exact install-lineage replay is idempotent");

    auto conflicting = request;
    conflicting.Install.HeadLineage =
        core::head_lineage::non_fast_forward;
    expect_failure(
        session->record_install(conflicting, {}),
        app::epoch_catalog_failure::phase_conflict,
        "a changed lineage cannot replay an already durable install");

    const auto binding = session->binding();
    session.reset();
    port.reset();
    made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
    expect(made.has_value(), "install-lineage catalog restarts");
    port = std::move(*made);
    opened = port->open(open_request(172U), {});
    expect(opened.has_value(), "install-lineage authority reopens");
    session = std::move(*opened);
    auto recovery = session->recovery_page(std::nullopt, 100U, {});
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
    const auto to_staging = [](
                                app::epoch_catalog_session& session,
                                const core::accepted_pull_request_revision& accepted,
                                const std::uint8_t seed,
                                const std::uint64_t application_session) {
        auto reserved = session.reserve_append(
            app::append_reservation_request{
                .Context = context(
                    session.binding(),
                    operation(seed),
                    accepted,
                    seed,
                    core::application_session_id{application_session}),
                .ProposedOrdinal = core::epoch_ordinal{1},
            },
            {});
        expect(reserved.has_value(), "corruption append reserves");
        auto spooled = session.record_spool(spool_request(*reserved, seed), {});
        expect(spooled.has_value(), "corruption append records spool");
        auto installed =
            session.record_install(install_request(*spooled, seed), {});
        expect(installed.has_value(), "corruption append records install");
        return transition_append(
            session,
            session.binding().Catalog,
            *installed,
            app::epoch_catalog_operation_phase::staging_ready,
            app::epoch_catalog_transition_evidence::exact_staging_pair);
    };

    {
        temporary_root root{"corrupt-source-presence"};
        auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
        expect(made.has_value(), "source-presence fixture bootstraps");
        auto port = std::move(*made);
        auto opened = port->open(open_request(181U), {});
        expect(opened.has_value(), "source-presence authority opens");
        auto session = std::move(*opened);
        auto reserved = session->reserve_append(
            app::append_reservation_request{
                .Context = context(
                    session->binding(),
                    operation(55U),
                    revision(55U),
                    55U,
                    core::application_session_id{181U}),
                .ProposedOrdinal = core::epoch_ordinal{1},
            },
            {});
        expect(reserved.has_value(), "source-presence append reserves");
        session.reset();
        port.reset();
        {
            const direct_database database{database_path(root.path()), true};
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
        temporary_root root{"corrupt-inventory-count"};
        auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
        expect(made.has_value(), "inventory-count fixture bootstraps");
        auto port = std::move(*made);
        auto opened = port->open(open_request(183U), {});
        expect(opened.has_value(), "inventory-count authority opens");
        auto session = std::move(*opened);
        auto reserved = session->reserve_append(
            app::append_reservation_request{
                .Context = context(
                    session->binding(),
                    operation(56U),
                    revision(56U),
                    56U,
                    core::application_session_id{183U}),
                .ProposedOrdinal = core::epoch_ordinal{1},
            },
            {});
        expect(reserved.has_value(), "inventory-count append reserves");
        auto spooled =
            session->record_spool(spool_request(*reserved, 56U), {});
        expect(spooled.has_value(), "inventory-count spool records");
        session.reset();
        port.reset();
        {
            const direct_database database{database_path(root.path()), true};
            database.execute("UPDATE append_receipt SET inventory_count = 0");
        }
        expect_recovery_failure(
            root.path(),
            184U,
            app::epoch_catalog_failure::schema_mismatch,
            "inventory count mismatch fails closed");
    }

    {
        temporary_root root{"corrupt-install-source"};
        auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
        expect(made.has_value(), "install-source fixture bootstraps");
        auto port = std::move(*made);
        auto opened = port->open(open_request(185U), {});
        expect(opened.has_value(), "install-source authority opens");
        auto session = std::move(*opened);
        (void)to_staging(*session, revision(57U), 57U, 185U);
        session.reset();
        port.reset();
        {
            const direct_database database{database_path(root.path()), true};
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
        temporary_root root{"corrupt-install-lineage"};
        auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
        expect(made.has_value(), "install-lineage fixture bootstraps");
        auto port = std::move(*made);
        auto opened = port->open(open_request(1861U), {});
        expect(opened.has_value(), "install-lineage authority opens");
        auto session = std::move(*opened);
        (void)to_staging(*session, revision(67U), 67U, 1861U);
        session.reset();
        port.reset();
        {
            const direct_database database{database_path(root.path()), true};
            database.execute(
                "PRAGMA ignore_check_constraints = ON;"
                "UPDATE append_install SET head_lineage = 9");
        }
        auto rejected =
            sqlite::make_epoch_catalog({.StorageRoot = root.path()});
        expect_failure(
            rejected,
            app::epoch_catalog_failure::integrity_failure,
            "out-of-range durable head lineage fails bootstrap integrity");
    }

    {
        temporary_root root{"corrupt-artifact-presence"};
        auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
        expect(made.has_value(), "artifact-presence fixture bootstraps");
        auto port = std::move(*made);
        auto opened = port->open(open_request(187U), {});
        expect(opened.has_value(), "artifact-presence authority opens");
        auto session = std::move(*opened);
        (void)to_staging(*session, revision(58U), 58U, 187U);
        session.reset();
        port.reset();
        {
            const direct_database database{database_path(root.path()), true};
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
        temporary_root root{"corrupt-installed-pack-bytes"};
        auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
        expect(made.has_value(), "installed-pack-byte fixture bootstraps");
        auto port = std::move(*made);
        auto opened = port->open(open_request(1881U), {});
        expect(opened.has_value(), "installed-pack-byte authority opens");
        auto session = std::move(*opened);
        (void)to_staging(*session, revision(60U), 60U, 1881U);
        session.reset();
        port.reset();
        {
            const direct_database database{database_path(root.path()), true};
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
        temporary_root root{"corrupt-preexisting-index-bytes"};
        auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
        expect(made.has_value(), "preexisting-index-byte fixture bootstraps");
        auto port = std::move(*made);
        auto opened = port->open(open_request(1883U), {});
        expect(opened.has_value(), "preexisting-index-byte authority opens");
        auto session = std::move(*opened);
        (void)to_staging(*session, revision(62U), 62U, 1883U);
        session.reset();
        port.reset();
        {
            const direct_database database{database_path(root.path()), true};
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
        temporary_root root{"corrupt-material"};
        auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
        expect(made.has_value(), "material-coherence fixture bootstraps");
        auto port = std::move(*made);
        auto opened = port->open(open_request(189U), {});
        expect(opened.has_value(), "material-coherence authority opens");
        auto session = std::move(*opened);
        auto record = to_staging(*session, revision(59U), 59U, 189U);
        auto mutation = candidate(record, 2'100, true);
        auto recorded = session->record_candidate(
            app::record_candidate_request{
                .Catalog = session->binding().Catalog,
                .Operation = record.Context.Operation,
                .ExpectedOperationRevision = record.OperationRevision,
                .Candidate = mutation,
            },
            {});
        expect(recorded.has_value(), "material candidate records");
        session.reset();
        port.reset();
        {
            const direct_database database{database_path(root.path()), true};
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
        temporary_root root{"corrupt-review"};
        auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
        expect(made.has_value(), "review-coherence fixture bootstraps");
        auto port = std::move(*made);
        auto opened = port->open(open_request(191U), {});
        expect(opened.has_value(), "review-coherence authority opens");
        auto session = std::move(*opened);
        auto accepted = revision(61U);
        const auto catalog_review =
            review_association(61U, accepted.Head);
        accepted.CurrentViewerReview = core::current_viewer_review_candidate{
            .Review = catalog_review.Review,
            .Actor = catalog_review.Actor,
            .Order = catalog_review.Order,
            .State = catalog_review.State,
            .Commit = catalog_review.Commit,
        };
        auto record = to_staging(*session, accepted, 61U, 191U);
        auto mutation = candidate(record, 2'200, true, catalog_review);
        auto recorded = session->record_candidate(
            app::record_candidate_request{
                .Catalog = session->binding().Catalog,
                .Operation = record.Context.Operation,
                .ExpectedOperationRevision = record.OperationRevision,
                .Candidate = mutation,
            },
            {});
        expect(recorded.has_value(), "review candidate records");
        session.reset();
        port.reset();
        {
            const direct_database database{database_path(root.path()), true};
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
        temporary_root root{"corrupt-detail-count"};
        auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
        expect(made.has_value(), "detail-count fixture bootstraps");
        auto port = std::move(*made);
        auto opened = port->open(open_request(193U), {});
        expect(opened.has_value(), "detail-count authority opens");
        auto session = std::move(*opened);
        const auto epoch = complete_append(
            *session,
            core::epoch_ordinal{1},
            62U,
            core::application_session_id{193U},
            true);
        const auto retained = app::git_retained_ref_pair{
            .Scope = session->binding().Authority.Scope,
            .Ordinal = epoch.Mutation.Epoch.Id.Ordinal,
            .Base = epoch.Mutation.Epoch.Base,
            .Head = epoch.Mutation.Epoch.Head,
        };
        auto begun = session->begin_prune(
            app::begin_prune_request{
                .Catalog = session->binding().Catalog,
                .Operation = operation(63U),
                .Epochs =
                    {
                        app::prune_epoch_fact{
                            .Epoch = epoch.Mutation.Epoch.Id,
                            .Retained = retained,
                            .BaseRef =
                                app::epoch_catalog_ref_identity{
                                    .Authority =
                                        session->binding().Authority.Scope,
                                    .Ordinal = epoch.Mutation.Epoch.Id.Ordinal,
                                },
                            .HeadRef =
                                app::epoch_catalog_ref_identity{
                                    .Authority =
                                        session->binding().Authority.Scope,
                                    .Ordinal = epoch.Mutation.Epoch.Id.Ordinal,
                                    .Head = true,
                                },
                        },
                    },
            },
            {});
        expect(begun.has_value(), "detail-count prune begins");
        session.reset();
        port.reset();
        {
            const direct_database database{database_path(root.path()), true};
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
        temporary_root root{"corrupt-terminal"};
        auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
        expect(made.has_value(), "terminal-coherence fixture bootstraps");
        auto port = std::move(*made);
        auto opened = port->open(open_request(195U), {});
        expect(opened.has_value(), "terminal-coherence authority opens");
        auto session = std::move(*opened);
        auto reserved = session->reserve_append(
            app::append_reservation_request{
                .Context = context(
                    session->binding(),
                    operation(64U),
                    revision(64U),
                    64U,
                    core::application_session_id{195U}),
                .ProposedOrdinal = core::epoch_ordinal{1},
            },
            {});
        expect(reserved.has_value(), "terminal-coherence append reserves");
        session.reset();
        port.reset();
        {
            const direct_database database{database_path(root.path()), true};
            database.execute(
                "PRAGMA ignore_check_constraints = ON;"
                "UPDATE operation SET terminal = 1");
        }
        auto rejected =
            sqlite::make_epoch_catalog({.StorageRoot = root.path()});
        expect_failure(
            rejected,
            app::epoch_catalog_failure::integrity_failure,
            "terminal/phase CHECK corruption fails bootstrap integrity");
    }

    {
        temporary_root root{"corrupt-cause"};
        auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
        expect(made.has_value(), "cause-coherence fixture bootstraps");
        auto port = std::move(*made);
        auto opened = port->open(open_request(196U), {});
        expect(opened.has_value(), "cause-coherence authority opens");
        auto session = std::move(*opened);
        (void)complete_append(
            *session,
            core::epoch_ordinal{1},
            69U,
            core::application_session_id{196U},
            true);
        session.reset();
        port.reset();
        {
            const direct_database database{database_path(root.path()), true};
            database.execute(
                "UPDATE epoch SET primary_cause = 5 "
                "WHERE origin = 1 AND base_transition = 1 "
                "AND head_transition = 1");
        }
        auto remade =
            sqlite::make_epoch_catalog({.StorageRoot = root.path()});
        expect(remade.has_value(), "range-valid cause corruption reopens");
        port = std::move(*remade);
        auto reopened = port->open(open_request(197U), {});
        expect(reopened.has_value(), "cause-corrupt authority reopens");
        session = std::move(*reopened);
        expect_failure(
            session->snapshot({}),
            app::epoch_catalog_failure::schema_mismatch,
            "range-valid but contradictory cause tuple fails closed");
    }
}

void composite_scope_integrity_contract()
{
    temporary_root root{"composite-scope-integrity"};
    auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
    expect(made.has_value(), "composite-scope fixture bootstraps");
    auto port = std::move(*made);
    const auto first_authority = authority_key();
    const auto second_authority = authority_key(
        0xF200000000000016ULL,
        0xF200000000000017ULL,
        4252U);
    auto first_open = port->open(open_request(501U, first_authority), {});
    auto second_open = port->open(open_request(502U, second_authority), {});
    expect(first_open && second_open, "two-cache integrity authorities open");
    auto first = std::move(*first_open);
    auto second = std::move(*second_open);
    expect(
        first->binding().Authority.Cache
                != second->binding().Authority.Cache
            && first->binding().Authority.Scope
                != second->binding().Authority.Scope,
        "integrity fixture owns distinct PR and cache scopes");

    const auto first_epoch = complete_append(
        *first,
        core::epoch_ordinal{1},
        100U,
        core::application_session_id{501U},
        true);
    const auto second_epoch = complete_append(
        *second,
        core::epoch_ordinal{1},
        101U,
        core::application_session_id{502U},
        true);
    const auto first_append = operation(100U);
    const auto second_append = operation(101U);
    const auto first_pr = hex_identity(first->binding().Authority.Scope);
    const auto second_pr = hex_identity(second->binding().Authority.Scope);
    const auto first_cache = hex_identity(first->binding().Authority.Cache);
    const auto second_cache = hex_identity(second->binding().Authority.Cache);

    {
        const direct_database database{database_path(root.path()), true};
        const auto code = database.execute_code(
            std::string{"UPDATE operation SET source_observation=(SELECT "
                        "source_observation FROM operation WHERE operation=x'"}
            + hex_identity(first_append) + "') WHERE operation=x'"
            + hex_identity(second_append) + "'");
        expect(
            (code & 0xFF) == SQLITE_CONSTRAINT
                && database.integer(
                       "SELECT count(DISTINCT source_observation) "
                       "FROM operation WHERE kind=1")
                    == 2,
            "canonical unique index rejects a durable append source collision "
            "at the database writer boundary");
    }

    {
        const direct_database database{database_path(root.path()), true};
        database.execute("PRAGMA foreign_keys = ON");
        const auto code = database.execute_code(
            std::string{"UPDATE operation SET pr_scope=x'"}
            + second_pr + "' WHERE operation=x'"
            + hex_identity(first_append) + "'");
        expect(
            (code & 0xFF) == SQLITE_CONSTRAINT,
            "operation cannot bind a PR from another cache");
        expect(
            database.foreign_key_violation_count() == 0
                && database.integer(
                       std::string{
                           "SELECT count(*) FROM operation WHERE operation=x'"}
                       + hex_identity(first_append) + "' AND pr_scope=x'"
                       + first_pr + "'")
                    == 1,
            "rejected operation-to-PR substitution changes no row");
    }

    const auto make_prune = [](
                                app::epoch_catalog_session& session,
                                const append_outcome& epoch,
                                const core::epoch_operation_id id) {
        const auto retained = app::git_retained_ref_pair{
            .Scope = session.binding().Authority.Scope,
            .Ordinal = epoch.Mutation.Epoch.Id.Ordinal,
            .Base = epoch.Mutation.Epoch.Base,
            .Head = epoch.Mutation.Epoch.Head,
        };
        const auto fact = app::prune_epoch_fact{
            .Epoch = epoch.Mutation.Epoch.Id,
            .Retained = retained,
            .BaseRef =
                app::epoch_catalog_ref_identity{
                    .Authority = session.binding().Authority.Scope,
                    .Ordinal = epoch.Mutation.Epoch.Id.Ordinal,
                },
            .HeadRef =
                app::epoch_catalog_ref_identity{
                    .Authority = session.binding().Authority.Scope,
                    .Ordinal = epoch.Mutation.Epoch.Id.Ordinal,
                    .Head = true,
                },
        };
        auto begun = session.begin_prune(
            app::begin_prune_request{
                .Catalog = session.binding().Catalog,
                .Operation = id,
                .Epochs = {fact},
            },
            {});
        expect(begun.has_value(), "cross-scope prune intent begins");
        return *begun;
    };
    auto first_prune = make_prune(*first, first_epoch, operation(102U));
    auto second_prune = make_prune(*second, second_epoch, operation(103U));

    {
        const direct_database database{database_path(root.path()), true};
        database.execute("PRAGMA foreign_keys = ON");
        const auto code = database.execute_code(
            std::string{"UPDATE prune_operation SET operation=x'"}
            + hex_identity(second_prune.Operation)
            + "' WHERE operation=x'" + hex_identity(first_prune.Operation)
            + "'");
        expect(
            (code & 0xFF) == SQLITE_CONSTRAINT,
            "prune detail cannot bind an operation owned by another PR");
        expect(
            database.foreign_key_violation_count() == 0
                && database.integer(
                       std::string{
                           "SELECT count(*) FROM prune_operation WHERE "
                           "operation=x'"}
                       + hex_identity(first_prune.Operation)
                       + "' AND pr_scope=x'" + first_pr + "'")
                    == 1,
            "rejected prune-detail substitution changes no row");
    }

    const auto abort_prune = [](app::epoch_catalog_session& session,
                                app::maintenance_operation_record record) {
        auto requested = session.transition_operation(
            app::operation_transition_request{
                .Catalog = session.binding().Catalog,
                .Operation = record.Operation,
                .Kind = app::epoch_catalog_operation_kind::prune,
                .Expected = app::epoch_catalog_operation_phase::prune_intent,
                .Next = app::epoch_catalog_operation_phase::abort_requested,
                .Evidence = app::epoch_catalog_transition_evidence::
                    catalog_transaction,
                .ExpectedOperationRevision = record.OperationRevision,
            },
            {});
        expect(requested.has_value(), "cross-scope prune abort requests");
        const auto& requested_record =
            std::get<app::maintenance_operation_record>(*requested);
        auto aborted = session.transition_operation(
            app::operation_transition_request{
                .Catalog = session.binding().Catalog,
                .Operation = requested_record.Operation,
                .Kind = app::epoch_catalog_operation_kind::prune,
                .Expected =
                    app::epoch_catalog_operation_phase::abort_requested,
                .Next = app::epoch_catalog_operation_phase::aborted,
                .Evidence = app::epoch_catalog_transition_evidence::
                    exact_cleanup_complete,
                .ExpectedOperationRevision =
                    requested_record.OperationRevision,
            },
            {});
        expect(aborted.has_value(), "cross-scope prune abort completes");
    };
    abort_prune(*first, first_prune);
    abort_prune(*second, second_prune);

    auto first_impact = first->cache_delete_impact(
        first->binding().Authority.Cache, {});
    auto second_impact = second->cache_delete_impact(
        second->binding().Authority.Cache, {});
    expect(first_impact && second_impact, "two cache impacts load exactly");
    auto first_delete = first->begin_cache_delete(
        app::begin_cache_delete_request{
            .Operation = operation(104U),
            .Impact = *first_impact,
        },
        {});
    auto second_delete = second->begin_cache_delete(
        app::begin_cache_delete_request{
            .Operation = operation(105U),
            .Impact = *second_impact,
        },
        {});
    expect(first_delete && second_delete, "two cache-delete seals persist");

    {
        const direct_database database{database_path(root.path()), true};
        database.execute("PRAGMA foreign_keys = ON");
        const auto code = database.execute_code(
            std::string{"UPDATE cache_delete_scope SET cache_scope=x'"}
            + second_cache + "' WHERE operation=x'"
            + hex_identity(first_delete->Operation) + "'");
        expect(
            (code & 0xFF) == SQLITE_CONSTRAINT,
            "cache-delete detail cannot substitute another cache");
        expect(
            database.foreign_key_violation_count() == 0
                && database.integer(
                       std::string{
                           "SELECT count(*) FROM cache_delete_scope WHERE "
                           "operation=x'"}
                       + hex_identity(first_delete->Operation)
                       + "' AND cache_scope=x'" + first_cache + "'")
                    == 1,
            "rejected cache-delete detail substitution changes no row");
    }

    first.reset();
    second.reset();
    port.reset();
    std::array<std::int64_t, 4U> before{};
    {
        const direct_database database{database_path(root.path()), true};
        database.execute("PRAGMA foreign_keys = OFF");
        database.execute(
            std::string{"UPDATE cache_delete_scope SET cache_scope=x'"}
            + second_cache + "' WHERE operation=x'"
            + hex_identity(first_delete->Operation) + "'");
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
    auto rejected_reopen =
        sqlite::make_epoch_catalog({.StorageRoot = root.path()});
    expect_failure(
        rejected_reopen,
        app::epoch_catalog_failure::integrity_failure,
        "raw cross-cache detail corruption fails catalog reopen closed");
    {
        const direct_database database{database_path(root.path())};
        const auto after = std::array{
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
    temporary_root root{"revision-exhaustion"};
    auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
    expect(made.has_value(), "revision-exhaustion fixture bootstraps");
    auto port = std::move(*made);
    auto opened = port->open(open_request(171U), {});
    expect(opened.has_value(), "revision-exhaustion authority opens");
    auto session = std::move(*opened);
    const auto epoch = complete_append(
        *session,
        core::epoch_ordinal{1},
        52U,
        core::application_session_id{171U},
        true);
    session.reset();
    port.reset();
    {
        const direct_database database{database_path(root.path()), true};
        database.execute(
            "UPDATE pull_request_scope SET "
            "catalog_revision = 9223372036854775807");
    }
    auto remade = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
    expect(remade.has_value(), "maximum-revision catalog reopens");
    port = std::move(*remade);
    auto reopened = port->open(open_request(172U), {});
    expect(reopened.has_value(), "maximum-revision authority reopens");
    session = std::move(*reopened);
    expect(
        session->binding().Catalog.Revision.value()
            == std::numeric_limits<std::int64_t>::max(),
        "direct fixture reaches the exact catalog revision boundary");
    const auto operation_count = direct_count(
        database_path(root.path()), "SELECT count(*) FROM operation");
    const auto review_count = direct_count(
        database_path(root.path()), "SELECT count(*) FROM review_association");
    const auto next_ordinal = direct_count(
        database_path(root.path()),
        "SELECT next_ordinal FROM pull_request_scope");
    const auto base_availability = direct_count(
        database_path(root.path()),
        "SELECT availability FROM epoch_anchor WHERE endpoint = 1");
    auto reserve = session->reserve_append(
        app::append_reservation_request{
            .Context = context(
                session->binding(),
                operation(53U),
                revision(53U),
                53U,
                core::application_session_id{172U}),
            .ProposedOrdinal = core::epoch_ordinal{2},
        },
        {});
    expect_failure(
        reserve,
        app::epoch_catalog_failure::revision_conflict,
        "maximum catalog revision rejects a new append reservation");
    const auto review = review_association(54U, epoch.Mutation.Epoch.Head);
    expect_failure(
        session->associate_review(
            app::associate_review_request{
                .Catalog = session->binding().Catalog,
                .Review = review,
                .AcceptedSnapshot = core::provider_snapshot_id{
                    0xFC00000000000054ULL},
            },
            {}),
        app::epoch_catalog_failure::revision_conflict,
        "maximum catalog revision rejects review association");
    const auto base = anchor_identity(
        epoch.Mutation.Epoch.Id,
        core::comparison_anchor_kind::epoch_base);
    expect_failure(
        session->mark_anchor_availability(
            app::mark_anchor_availability_request{
                .Catalog = session->binding().Catalog,
                .Changes =
                    {
                        app::anchor_availability_change{
                            .Anchor = base,
                            .ExpectedCommit = epoch.Mutation.Epoch.Base,
                            .Expected = core::anchor_availability::available,
                            .Next =
                                core::anchor_availability::unavailable,
                        },
                    },
            },
            {}),
        app::epoch_catalog_failure::revision_conflict,
        "maximum catalog revision rejects availability mutation");
    expect(
        direct_count(
            database_path(root.path()), "SELECT count(*) FROM operation")
                == operation_count
            && direct_count(
                   database_path(root.path()),
                   "SELECT count(*) FROM review_association")
                == review_count
            && direct_count(
                   database_path(root.path()),
                   "SELECT next_ordinal FROM pull_request_scope")
                == next_ordinal
            && direct_count(
                   database_path(root.path()),
                   "SELECT availability FROM epoch_anchor WHERE endpoint = 1")
                == base_availability,
        "revision exhaustion rolls back every attempted row and pointer change");
}

void nonterminal_revision_reserve_contract()
{
    constexpr auto MaximumRevision =
        app::maximum_epoch_catalog_operation_revision;
    const auto final_nonterminal_revision =
        app::epoch_catalog_operation_revision{MaximumRevision - 1};
    const auto force_final_nonterminal_revision = [=](
                                                        const auto& root,
                                                        const auto& id) {
        const direct_database database{database_path(root), true};
        database.execute(
            std::string{"UPDATE operation SET operation_revision="}
            + std::to_string(final_nonterminal_revision.value())
            + " WHERE operation=x'" + hex_identity(id) + "'");
        expect(
            database.integer(
                std::string{
                    "SELECT operation_revision FROM operation WHERE "
                    "operation=x'"}
                + hex_identity(id) + "'")
                == final_nonterminal_revision.value(),
            "fixture reaches the final nonterminal operation revision");
    };

    {
        temporary_root root{"append-revision-reserve"};
        auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
        expect(made.has_value(), "append reserve fixture bootstraps");
        auto port = std::move(*made);
        auto opened = port->open(open_request(631U), {});
        expect(opened.has_value(), "append reserve authority opens");
        auto session = std::move(*opened);
        const auto operation_id = operation(133U);
        auto reserved = session->reserve_append(
            app::append_reservation_request{
                .Context = context(
                    session->binding(),
                    operation_id,
                    revision(133U),
                    133U,
                    core::application_session_id{631U}),
                .ProposedOrdinal = core::epoch_ordinal{1},
            },
            {});
        expect(reserved.has_value(), "append reserve operation begins");
        force_final_nonterminal_revision(root.path(), operation_id);
        const auto before =
            catalog_logical_fingerprint(database_path(root.path()));
        auto spool = spool_request(*reserved, 133U);
        spool.ExpectedOperationRevision = final_nonterminal_revision;
        expect_failure(
            session->record_spool(spool, {}),
            app::epoch_catalog_failure::invalid_request,
            "append cannot consume its terminal revision reserve for spool");
        expect(
            catalog_logical_fingerprint(database_path(root.path()))
                == before,
            "rejected final-reserve append changes no catalog row");

        const auto repair_request = app::operation_transition_request{
            .Catalog = session->binding().Catalog,
            .Operation = operation_id,
            .Kind = app::epoch_catalog_operation_kind::append,
            .Expected = app::epoch_catalog_operation_phase::preparing,
            .Next = app::epoch_catalog_operation_phase::repair_required,
            .Evidence =
                app::epoch_catalog_transition_evidence::repair_ambiguity,
            .ExpectedOperationRevision = final_nonterminal_revision,
        };
        auto repaired = session->transition_operation(repair_request, {});
        const auto* repair_record = repaired
            ? std::get_if<app::append_operation_record>(&*repaired)
            : nullptr;
        auto repair_retry = session->transition_operation(repair_request, {});
        expect(
            repair_record != nullptr
                && repair_record->Phase
                    == app::epoch_catalog_operation_phase::repair_required
                && repair_record->RepairFromPhase
                    == app::epoch_catalog_operation_phase::preparing
                && repair_record->OperationRevision.value()
                    == MaximumRevision
                && repair_retry
                && std::get<app::append_operation_record>(*repair_retry)
                    == *repair_record,
            "append repair alone may consume the final revision and replay");
    }

    {
        temporary_root root{"prune-revision-reserve"};
        auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
        expect(made.has_value(), "prune reserve fixture bootstraps");
        auto port = std::move(*made);
        auto opened = port->open(open_request(641U), {});
        expect(opened.has_value(), "prune reserve authority opens");
        auto session = std::move(*opened);
        const auto epoch = complete_append(
            *session,
            core::epoch_ordinal{1},
            134U,
            core::application_session_id{641U},
            true);
        const auto retained = app::git_retained_ref_pair{
            .Scope = session->binding().Authority.Scope,
            .Ordinal = epoch.Mutation.Epoch.Id.Ordinal,
            .Base = epoch.Mutation.Epoch.Base,
            .Head = epoch.Mutation.Epoch.Head,
        };
        const auto prune_fact = app::prune_epoch_fact{
            .Epoch = epoch.Mutation.Epoch.Id,
            .Retained = retained,
            .BaseRef =
                app::epoch_catalog_ref_identity{
                    .Authority = session->binding().Authority.Scope,
                    .Ordinal = epoch.Mutation.Epoch.Id.Ordinal,
                },
            .HeadRef =
                app::epoch_catalog_ref_identity{
                    .Authority = session->binding().Authority.Scope,
                    .Ordinal = epoch.Mutation.Epoch.Id.Ordinal,
                    .Head = true,
                },
        };
        const auto operation_id = operation(135U);
        auto begun = session->begin_prune(
            app::begin_prune_request{
                .Catalog = session->binding().Catalog,
                .Operation = operation_id,
                .Epochs = {prune_fact},
            },
            {});
        expect(begun.has_value(), "prune reserve operation begins");
        force_final_nonterminal_revision(root.path(), operation_id);
        const auto before =
            catalog_logical_fingerprint(database_path(root.path()));
        expect_failure(
            session->transition_operation(
                app::operation_transition_request{
                    .Catalog = session->binding().Catalog,
                    .Operation = operation_id,
                    .Kind = app::epoch_catalog_operation_kind::prune,
                    .Expected =
                        app::epoch_catalog_operation_phase::prune_intent,
                    .Next = app::epoch_catalog_operation_phase::
                        ref_delete_in_progress,
                    .Evidence = app::epoch_catalog_transition_evidence::
                        catalog_transaction,
                    .ExpectedOperationRevision =
                        final_nonterminal_revision,
                },
                {}),
            app::epoch_catalog_failure::invalid_request,
            "prune cannot consume its terminal revision reserve for deletion");
        expect(
            catalog_logical_fingerprint(database_path(root.path()))
                == before,
            "rejected final-reserve prune changes no catalog row");

        const auto repair_request = app::operation_transition_request{
            .Catalog = session->binding().Catalog,
            .Operation = operation_id,
            .Kind = app::epoch_catalog_operation_kind::prune,
            .Expected = app::epoch_catalog_operation_phase::prune_intent,
            .Next = app::epoch_catalog_operation_phase::repair_required,
            .Evidence =
                app::epoch_catalog_transition_evidence::repair_ambiguity,
            .ExpectedOperationRevision = final_nonterminal_revision,
        };
        auto repaired = session->transition_operation(repair_request, {});
        const auto* repair_record = repaired
            ? std::get_if<app::maintenance_operation_record>(&*repaired)
            : nullptr;
        auto repair_retry = session->transition_operation(repair_request, {});
        expect(
            repair_record != nullptr
                && repair_record->Phase
                    == app::epoch_catalog_operation_phase::repair_required
                && repair_record->RepairFromPhase
                    == app::epoch_catalog_operation_phase::prune_intent
                && repair_record->OperationRevision.value()
                    == MaximumRevision
                && repair_retry
                && std::get<app::maintenance_operation_record>(*repair_retry)
                    == *repair_record,
            "prune repair alone may consume the final revision and replay");
    }

    {
        temporary_root root{"cache-delete-revision-reserve"};
        auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
        expect(made.has_value(), "cache-delete reserve fixture bootstraps");
        auto port = std::move(*made);
        auto opened = port->open(open_request(651U), {});
        expect(opened.has_value(), "cache-delete reserve authority opens");
        auto session = std::move(*opened);
        auto impact = session->cache_delete_impact(
            session->binding().Authority.Cache, {});
        expect(impact.has_value(), "cache-delete reserve impact loads");
        const auto operation_id = operation(136U);
        auto begun = session->begin_cache_delete(
            app::begin_cache_delete_request{
                .Operation = operation_id,
                .Impact = *impact,
            },
            {});
        expect(begun.has_value(), "cache-delete reserve operation seals");
        force_final_nonterminal_revision(root.path(), operation_id);
        const auto before =
            catalog_logical_fingerprint(database_path(root.path()));
        expect_failure(
            session->transition_operation(
                app::operation_transition_request{
                    .Catalog = session->binding().Catalog,
                    .Operation = operation_id,
                    .Kind = app::epoch_catalog_operation_kind::cache_delete,
                    .Expected = app::epoch_catalog_operation_phase::
                        cache_delete_intent_sealed,
                    .Next = app::epoch_catalog_operation_phase::
                        quarantine_rename_in_progress,
                    .Evidence = app::epoch_catalog_transition_evidence::
                        catalog_transaction,
                    .ExpectedOperationRevision =
                        final_nonterminal_revision,
                },
                {}),
            app::epoch_catalog_failure::invalid_request,
            "cache delete cannot consume its terminal revision reserve for "
            "rename");
        expect(
            catalog_logical_fingerprint(database_path(root.path()))
                == before,
            "rejected final-reserve cache delete changes no catalog row");

        const auto repair_request = app::operation_transition_request{
            .Catalog = session->binding().Catalog,
            .Operation = operation_id,
            .Kind = app::epoch_catalog_operation_kind::cache_delete,
            .Expected = app::epoch_catalog_operation_phase::
                cache_delete_intent_sealed,
            .Next = app::epoch_catalog_operation_phase::repair_required,
            .Evidence =
                app::epoch_catalog_transition_evidence::repair_ambiguity,
            .ExpectedOperationRevision = final_nonterminal_revision,
        };
        auto repaired = session->transition_operation(repair_request, {});
        const auto* repair_record = repaired
            ? std::get_if<app::maintenance_operation_record>(&*repaired)
            : nullptr;
        auto repair_retry = session->transition_operation(repair_request, {});
        expect(
            repair_record != nullptr
                && repair_record->Phase
                    == app::epoch_catalog_operation_phase::repair_required
                && repair_record->RepairFromPhase
                    == app::epoch_catalog_operation_phase::
                        cache_delete_intent_sealed
                && repair_record->OperationRevision.value()
                    == MaximumRevision
                && repair_retry
                && std::get<app::maintenance_operation_record>(*repair_retry)
                    == *repair_record,
            "cache-delete repair alone may consume the final revision and "
            "replay");
    }
}

void quota_boundaries_contract()
{
    {
        temporary_root root{"pr-scope-quota"};
        auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
        expect(made.has_value(), "PR-scope quota fixture bootstraps");
        auto port = std::move(*made);
        auto opened = port->open(open_request(211U), {});
        expect(opened.has_value(), "PR-scope quota seed authority opens");
        opened->reset();
        port.reset();
        {
            const direct_database database{database_path(root.path()), true};
            database.insert_pull_request_scope_fillers(
                app::maximum_cache_delete_scope_count - 1U);
            expect(
                database.integer(
                    "SELECT count(*) FROM pull_request_scope")
                    == static_cast<std::int64_t>(
                        app::maximum_cache_delete_scope_count),
                "direct fixture reaches the exact 10k PR-scope boundary");
        }
        auto remade =
            sqlite::make_epoch_catalog({.StorageRoot = root.path()});
        expect(remade.has_value(), "full PR-scope catalog reopens");
        port = std::move(*remade);
        const auto overflow_authority = authority_key(
            authority_key().Repository.value(),
            0xF20000000000FF01ULL,
            9'001U);
        expect_failure(
            port->open(open_request(212U, overflow_authority), {}),
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
        temporary_root root{"retained-epoch-quota"};
        auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
        expect(made.has_value(), "retained-epoch quota fixture bootstraps");
        auto port = std::move(*made);
        auto opened = port->open(open_request(221U), {});
        expect(opened.has_value(), "retained-epoch quota authority opens");
        auto session = std::move(*opened);
        const auto first = complete_append(
            *session,
            core::epoch_ordinal{1},
            65U,
            core::application_session_id{221U},
            true);
        session.reset();
        port.reset();
        {
            const direct_database database{database_path(root.path()), true};
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
        auto remade =
            sqlite::make_epoch_catalog({.StorageRoot = root.path()});
        expect(remade.has_value(), "full retained-epoch catalog reopens");
        port = std::move(*remade);
        auto reopened = port->open(open_request(222U), {});
        expect(reopened.has_value(), "full retained-epoch authority reopens");
        session = std::move(*reopened);
        auto blocked = session->reserve_append(
            app::append_reservation_request{
                .Context = context(
                    session->binding(),
                    operation(66U),
                    revision(66U),
                    66U,
                    core::application_session_id{222U}),
                .ProposedOrdinal = core::epoch_ordinal{100'001},
            },
            {});
        expect_failure(
            blocked,
            app::epoch_catalog_failure::quota_exceeded,
            "retained-epoch boundary blocks another append reservation");

        const auto retained = app::git_retained_ref_pair{
            .Scope = session->binding().Authority.Scope,
            .Ordinal = first.Mutation.Epoch.Id.Ordinal,
            .Base = first.Mutation.Epoch.Base,
            .Head = first.Mutation.Epoch.Head,
        };
        auto prune = session->begin_prune(
            app::begin_prune_request{
                .Catalog = session->binding().Catalog,
                .Operation = operation(67U),
                .Epochs =
                    {
                        app::prune_epoch_fact{
                            .Epoch = first.Mutation.Epoch.Id,
                            .Retained = retained,
                            .BaseRef =
                                app::epoch_catalog_ref_identity{
                                    .Authority =
                                        session->binding().Authority.Scope,
                                    .Ordinal = first.Mutation.Epoch.Id.Ordinal,
                                },
                            .HeadRef =
                                app::epoch_catalog_ref_identity{
                                    .Authority =
                                        session->binding().Authority.Scope,
                                    .Ordinal = first.Mutation.Epoch.Id.Ordinal,
                                    .Head = true,
                                },
                        },
                    },
            },
            {});
        expect(prune.has_value(), "prune remains available at retained quota");
        auto deleting = session->transition_operation(
            app::operation_transition_request{
                .Catalog = session->binding().Catalog,
                .Operation = prune->Operation,
                .Kind = app::epoch_catalog_operation_kind::prune,
                .Expected = app::epoch_catalog_operation_phase::prune_intent,
                .Next = app::epoch_catalog_operation_phase::
                    ref_delete_in_progress,
                .Evidence = app::epoch_catalog_transition_evidence::
                    catalog_transaction,
                .ExpectedOperationRevision = prune->OperationRevision,
            },
            {});
        expect(deleting.has_value(), "quota prune starts ref deletion");
        const auto& deleting_record =
            std::get<app::maintenance_operation_record>(*deleting);
        auto pruned = session->finalize_prune(
            app::finalize_prune_request{
                .Transition =
                    app::operation_transition_request{
                        .Catalog = session->binding().Catalog,
                        .Operation = deleting_record.Operation,
                        .Kind = app::epoch_catalog_operation_kind::prune,
                        .Expected = app::epoch_catalog_operation_phase::
                            ref_delete_in_progress,
                        .Next = app::epoch_catalog_operation_phase::
                            visible_cleanup_pending,
                        .Evidence = app::epoch_catalog_transition_evidence::
                            exact_refs_absent,
                        .ExpectedOperationRevision =
                            deleting_record.OperationRevision,
                    },
                .ObservedAbsent = {retained},
            },
            {});
        expect(pruned && pruned->Changed, "quota prune commits one tombstone");
        auto page = session->recovery_page(std::nullopt, 100U, {});
        expect(page && page->Operations.size() == 1U, "quota cleanup recovers");
        const auto& cleanup =
            std::get<app::maintenance_operation_record>(
                page->Operations.front());
        auto completed = session->transition_operation(
            app::operation_transition_request{
                .Catalog = session->binding().Catalog,
                .Operation = cleanup.Operation,
                .Kind = app::epoch_catalog_operation_kind::prune,
                .Expected = app::epoch_catalog_operation_phase::
                    visible_cleanup_pending,
                .Next = app::epoch_catalog_operation_phase::complete,
                .Evidence = app::epoch_catalog_transition_evidence::
                    exact_cleanup_complete,
                .ExpectedOperationRevision = cleanup.OperationRevision,
            },
            {});
        expect(completed.has_value(), "quota prune cleanup completes");
        auto admitted = session->reserve_append(
            app::append_reservation_request{
                .Context = context(
                    session->binding(),
                    operation(68U),
                    revision(68U),
                    68U,
                    core::application_session_id{222U}),
                .ProposedOrdinal = core::epoch_ordinal{100'001},
            },
            {});
        expect(
            admitted
                && admitted->ReservedOrdinal == core::epoch_ordinal{100'001}
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
    temporary_root root{"bounded-wal"};
    auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
    expect(made.has_value(), "bounded-WAL fixture bootstraps");
    auto port = std::move(*made);
    auto opened = port->open(open_request(231U), {});
    expect(opened.has_value(), "bounded-WAL authority opens");
    auto session = std::move(*opened);
    const auto epoch = complete_append(
        *session,
        core::epoch_ordinal{1},
        72U,
        core::application_session_id{231U},
        true);
    const std::array reviews{
        review_association(73U, epoch.Mutation.Epoch.Head),
        review_association(74U, epoch.Mutation.Epoch.Head),
    };
    constexpr std::size_t WriteCount = 768U;
    for(std::size_t index = 0U; index < WriteCount; ++index)
    {
        const auto selected = index % reviews.size();
        auto committed = session->associate_review(
            app::associate_review_request{
                .Catalog = session->binding().Catalog,
                .Review = reviews[selected],
                .AcceptedSnapshot = core::provider_snapshot_id{
                    0xFC00000000001000ULL
                    + static_cast<std::uint64_t>(selected)},
            },
            {});
        expect(
            committed && committed->Changed,
            "alternating review update commits one bounded transaction");
    }
    const auto wal = test_platform::with_ascii_suffix(
        database_path(root.path()), "-wal");
    constexpr std::uintmax_t MaximumExpectedWalBytes = 8U * 1024U * 1024U;
    expect(
        !std::filesystem::exists(wal)
            || std::filesystem::file_size(wal) <= MaximumExpectedWalBytes,
        "automatic checkpoints bound WAL growth under sustained writes");
    const auto binding = session->binding();
    session.reset();
    port.reset();
    auto remade = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
    expect(remade.has_value(), "bounded-WAL catalog restarts");
    port = std::move(*remade);
    auto reopened = port->open(open_request(232U), {});
    expect(reopened.has_value(), "bounded-WAL authority reopens");
    session = std::move(*reopened);
    auto snapshot = session->snapshot({});
    expect(
        snapshot && snapshot->Binding == binding
            && snapshot->CurrentViewerReview
            && snapshot->CurrentViewerReview->Association
                == reviews[(WriteCount - 1U) % reviews.size()].Association,
        "checkpointed catalog retains the last committed review on restart");
}

void recovery_paging_and_phase_cas_contract()
{
    temporary_root root{"recovery"};
    auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
    expect(made.has_value(), "recovery fixture bootstraps");
    auto port = std::move(*made);
    const auto first_authority = authority_key();
    const auto sibling_authority = authority_key(
        first_authority.Repository.value(),
        0xF200000000000008ULL,
        4243U);
    const auto other_cache_authority = authority_key(
        0xF200000000000009ULL,
        0xF20000000000000AULL,
        4244U);
    auto first_open = port->open(open_request(201U, first_authority), {});
    auto sibling_open =
        port->open(open_request(202U, sibling_authority), {});
    auto other_cache_open =
        port->open(open_request(203U, other_cache_authority), {});
    expect(
        first_open && sibling_open && other_cache_open,
        "recovery authorities across one and two caches open");
    auto first = std::move(*first_open);
    auto sibling = std::move(*sibling_open);
    auto other_cache = std::move(*other_cache_open);
    expect(
        first->binding().Authority.Cache
                == sibling->binding().Authority.Cache
            && first->binding().Authority.Cache
                != other_cache->binding().Authority.Cache
            && first->binding().Authority.Scope
                != sibling->binding().Authority.Scope,
        "fixture allocates sibling PR scopes in one cache and another cache");

    const auto reserve = [](app::epoch_catalog_session& session,
                            const std::uint8_t seed,
                            const std::uint64_t application_session) {
        return session.reserve_append(
            app::append_reservation_request{
                .Context = context(
                    session.binding(),
                    operation(seed),
                    revision(seed),
                    seed,
                    core::application_session_id{application_session}),
                .ProposedOrdinal = core::epoch_ordinal{1},
            },
            {});
    };
    auto first_pending = reserve(*first, 50U, 201U);
    auto sibling_pending = reserve(*sibling, 60U, 202U);
    auto other_pending = reserve(*other_cache, 70U, 203U);
    expect(
        first_pending && sibling_pending && other_pending,
        "one pending append reserves independently per PR authority");

    auto blocked_second = first->reserve_append(
        app::append_reservation_request{
            .Context = context(
                first->binding(),
                operation(51U),
                revision(51U),
                51U,
                core::application_session_id{201U}),
            .ProposedOrdinal = core::epoch_ordinal{2},
        },
        {});
    expect_failure(
        blocked_second,
        app::epoch_catalog_failure::operation_conflict,
        "one PR authority permits only one active operation");
    auto active_impact = first->cache_delete_impact(
        first->binding().Authority.Cache, {});
    expect(active_impact.has_value(), "active cache impact remains inspectable");
    auto blocked_delete = first->begin_cache_delete(
        app::begin_cache_delete_request{
            .Operation = operation(75U),
            .Impact = *active_impact,
        },
        {});
    expect_failure(
        blocked_delete,
        app::epoch_catalog_failure::operation_conflict,
        "cache delete cannot seal while a child PR operation is active");

    const auto exact_recovery = [](app::epoch_catalog_session& session,
                                   const core::epoch_operation_id& expected) {
        auto page = session.recovery_page(std::nullopt, 100U, {});
        expect(
            page && page->Operations.size() == 1U && !page->Continuation,
            "recovery is bounded to exactly one owned operation");
        expect(
            std::get<app::append_operation_record>(page->Operations.front())
                    .Context.Operation
                == expected,
            "recovery returns only the exact PR-owned operation");
    };
    exact_recovery(*first, operation(50U));
    exact_recovery(*sibling, operation(60U));
    exact_recovery(*other_cache, operation(70U));

    expect_failure(
        first->recovery_page(std::nullopt, 0U, {}),
        app::epoch_catalog_failure::invalid_request,
        "zero-sized recovery page is rejected");
    expect_failure(
        first->recovery_page(
            std::nullopt,
            static_cast<std::uint16_t>(
                app::maximum_epoch_recovery_page_size + 1U),
            {}),
        app::epoch_catalog_failure::invalid_request,
        "oversized recovery page is rejected");

    auto wrong_phase = first->transition_operation(
        app::operation_transition_request{
            .Catalog = first->binding().Catalog,
            .Operation = first_pending->Context.Operation,
            .Kind = app::epoch_catalog_operation_kind::append,
            .Expected = app::epoch_catalog_operation_phase::imported,
            .Next = app::epoch_catalog_operation_phase::staging_ready,
            .Evidence =
                app::epoch_catalog_transition_evidence::exact_staging_pair,
            .ExpectedOperationRevision = first_pending->OperationRevision,
        },
        {});
    expect_failure(
        wrong_phase,
        app::epoch_catalog_failure::phase_conflict,
        "phase CAS rejects a valid edge from the wrong stored phase");

    auto sibling_takeover = first->transition_operation(
        app::operation_transition_request{
            .Catalog = first->binding().Catalog,
            .Operation = sibling_pending->Context.Operation,
            .Kind = app::epoch_catalog_operation_kind::append,
            .Expected = app::epoch_catalog_operation_phase::preparing,
            .Next = app::epoch_catalog_operation_phase::abort_requested,
            .Evidence =
                app::epoch_catalog_transition_evidence::catalog_transaction,
            .ExpectedOperationRevision =
                sibling_pending->OperationRevision,
        },
        {});
    expect_failure(
        sibling_takeover,
        app::epoch_catalog_failure::operation_conflict,
        "same-cache sibling PR cannot transition another authority's operation");
    auto other_cache_takeover = first->transition_operation(
        app::operation_transition_request{
            .Catalog = first->binding().Catalog,
            .Operation = other_pending->Context.Operation,
            .Kind = app::epoch_catalog_operation_kind::append,
            .Expected = app::epoch_catalog_operation_phase::preparing,
            .Next = app::epoch_catalog_operation_phase::abort_requested,
            .Evidence =
                app::epoch_catalog_transition_evidence::catalog_transaction,
            .ExpectedOperationRevision = other_pending->OperationRevision,
        },
        {});
    expect_failure(
        other_cache_takeover,
        app::epoch_catalog_failure::operation_conflict,
        "different-cache authority cannot transition another operation");
    exact_recovery(*sibling, operation(60U));
    exact_recovery(*other_cache, operation(70U));

    std::stop_source cancelled;
    cancelled.request_stop();
    expect_failure(
        first->recovery_page(std::nullopt, 1U, cancelled.get_token()),
        app::epoch_catalog_failure::cancelled,
        "pre-cancelled recovery page returns cancellation");

    const auto first_binding = first->binding();
    const auto sibling_binding = sibling->binding();
    const auto other_binding = other_cache->binding();
    first.reset();
    sibling.reset();
    other_cache.reset();
    port.reset();
    auto reopened_port_result =
        sqlite::make_epoch_catalog({.StorageRoot = root.path()});
    expect(
        reopened_port_result.has_value(),
        "catalog with pending recovery records reopens");
    auto reopened_port = std::move(*reopened_port_result);
    auto reopened_result =
        reopened_port->open(open_request(204U, first_authority), {});
    auto reopened_sibling_result =
        reopened_port->open(open_request(205U, sibling_authority), {});
    auto reopened_other_result =
        reopened_port->open(open_request(206U, other_cache_authority), {});
    expect(
        reopened_result && reopened_sibling_result && reopened_other_result,
        "all recovery authorities reopen");
    auto reopened = std::move(*reopened_result);
    auto reopened_sibling = std::move(*reopened_sibling_result);
    auto reopened_other = std::move(*reopened_other_result);
    expect(
        reopened->binding() == first_binding
            && reopened_sibling->binding() == sibling_binding
            && reopened_other->binding() == other_binding,
        "pending operations preserve each authority binding across restart");
    auto recovered = reopened->recovery_page(std::nullopt, 100U, {});
    auto recovered_sibling =
        reopened_sibling->recovery_page(std::nullopt, 100U, {});
    auto recovered_other =
        reopened_other->recovery_page(std::nullopt, 100U, {});
    expect(
        recovered && recovered->Operations.size() == 1U
            && recovered_sibling
            && recovered_sibling->Operations.size() == 1U
            && recovered_other
            && recovered_other->Operations.size() == 1U,
        "restart preserves exact PR- and cache-scoped recovery isolation");
}

void exact_cache_delete_intent_contract()
{
    temporary_root root{"cache-delete"};
    auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
    expect(made.has_value(), "cache-delete fixture bootstraps");
    auto port = std::move(*made);
    const auto authority = authority_key();
    auto opened = port->open(open_request(301U, authority), {});
    expect(opened.has_value(), "cache-delete fixture authority opens");
    auto session = std::move(*opened);
    const auto epoch = complete_append(
        *session,
        core::epoch_ordinal{1},
        80U,
        core::application_session_id{301U},
        true);
    const auto review = review_association(81U, epoch.Mutation.Epoch.Head);
    auto associated = session->associate_review(
        app::associate_review_request{
            .Catalog = session->binding().Catalog,
            .Review = review,
            .AcceptedSnapshot = core::provider_snapshot_id{
                0xFC00000000000081ULL},
        },
        {});
    expect(associated && associated->Changed, "delete fixture links a review");
    const auto live_binding = session->binding();
    const auto review_anchor = core::comparison_anchor_identity{
        .Authority = live_binding.Authority.Scope,
        .Incarnation = live_binding.Catalog.Incarnation,
        .Kind = core::comparison_anchor_kind::review_commit,
        .Review = review.Association,
    };

    const auto restart = [&](const std::uint64_t application_session) {
        session.reset();
        port.reset();
        auto remade =
            sqlite::make_epoch_catalog({.StorageRoot = root.path()});
        expect(remade.has_value(), "cache-delete catalog restarts");
        port = std::move(*remade);
        auto reopened = port->open(
            open_request(application_session, authority), {});
        expect(reopened.has_value(), "cache-delete authority reopens");
        session = std::move(*reopened);
    };
    const auto recover_delete = [&]() {
        auto page = session->recovery_page(std::nullopt, 100U, {});
        expect(page.has_value(), "cache-delete recovery page loads");
        std::optional<app::maintenance_operation_record> found;
        for(const auto& value : page->Operations)
        {
            const auto* maintenance =
                std::get_if<app::maintenance_operation_record>(&value);
            if(maintenance != nullptr && maintenance->Operation == operation(82U))
            {
                found = *maintenance;
            }
        }
        expect(found.has_value(), "cache-delete recovery remains exact");
        return *found;
    };

    auto impact = session->cache_delete_impact(
        session->binding().Authority.Cache, {});
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

    auto wrong_impact = *impact;
    wrong_impact.RetainedEpochCount = 2U;
    auto rejected = session->begin_cache_delete(
        app::begin_cache_delete_request{
            .Operation = operation(81U),
            .Impact = wrong_impact,
        },
        {});
    expect_failure(
        rejected,
        app::epoch_catalog_failure::revision_conflict,
        "cache delete rejects an impact that no longer exactly matches");

    auto begun = session->begin_cache_delete(
        app::begin_cache_delete_request{
            .Operation = operation(82U),
            .Impact = *impact,
        },
        {});
    expect(
        begun
            && begun->Kind
                == app::epoch_catalog_operation_kind::cache_delete
            && begun->Phase
                == app::epoch_catalog_operation_phase::
                    cache_delete_intent_sealed
            && begun->Scopes == impact->Scopes,
        "cache-delete operation persists the exact sealed scope set");
    auto begun_retry = session->begin_cache_delete(
        app::begin_cache_delete_request{
            .Operation = operation(82U),
            .Impact = *impact,
        },
        {});
    expect(
        begun_retry && *begun_retry == *begun,
        "exact cache-delete begin retry is idempotent");

    expect_failure(
        session->snapshot({}),
        app::epoch_catalog_failure::operation_conflict,
        "sealed parent cache blocks ordinary child reads");
    auto blocked_append = session->reserve_append(
        app::append_reservation_request{
            .Context = context(
                session->binding(),
                operation(83U),
                revision(83U),
                83U,
                core::application_session_id{301U}),
            .ProposedOrdinal = core::epoch_ordinal{2},
        },
        {});
    expect_failure(
        blocked_append,
        app::epoch_catalog_failure::authority_mismatch,
        "sealed parent cache blocks new child operations");

    restart(302U);
    expect(
        session->binding() == live_binding,
        "sealed cache restart retains the original authority binding");
    auto sealed = recover_delete();
    expect(
        sealed.Phase
            == app::epoch_catalog_operation_phase::
                cache_delete_intent_sealed,
        "restart resumes from the durable delete seal");

    auto rename_result = session->transition_operation(
        app::operation_transition_request{
            .Catalog = session->binding().Catalog,
            .Operation = sealed.Operation,
            .Kind = app::epoch_catalog_operation_kind::cache_delete,
            .Expected = app::epoch_catalog_operation_phase::
                cache_delete_intent_sealed,
            .Next = app::epoch_catalog_operation_phase::
                quarantine_rename_in_progress,
            .Evidence =
                app::epoch_catalog_transition_evidence::catalog_transaction,
            .ExpectedOperationRevision = sealed.OperationRevision,
        },
        {});
    expect(rename_result.has_value(), "cache quarantine rename starts");
    restart(303U);
    auto renaming = recover_delete();
    expect(
        renaming.Phase
            == app::epoch_catalog_operation_phase::
                quarantine_rename_in_progress,
        "restart retains the point-of-no-cancellation rename phase");
    std::stop_source cancelled_after_ponr;
    cancelled_after_ponr.request_stop();
    auto quarantined_result = session->transition_operation(
        app::operation_transition_request{
            .Catalog = session->binding().Catalog,
            .Operation = renaming.Operation,
            .Kind = app::epoch_catalog_operation_kind::cache_delete,
            .Expected = app::epoch_catalog_operation_phase::
                quarantine_rename_in_progress,
            .Next = app::epoch_catalog_operation_phase::quarantined,
            .Evidence =
                app::epoch_catalog_transition_evidence::exact_quarantine,
            .ExpectedOperationRevision = renaming.OperationRevision,
        },
        cancelled_after_ponr.get_token());
    expect(
        quarantined_result.has_value(),
        "post-PONR cancellation cannot interrupt exact quarantine recovery");
    const auto* quarantined =
        std::get_if<app::maintenance_operation_record>(&*quarantined_result);
    expect(quarantined != nullptr, "quarantine returns maintenance record");
    const auto finalize_request = app::finalize_cache_delete_request{
        .Transition =
            app::operation_transition_request{
                .Catalog = session->binding().Catalog,
                .Operation = quarantined->Operation,
                .Kind = app::epoch_catalog_operation_kind::cache_delete,
                .Expected = app::epoch_catalog_operation_phase::quarantined,
                .Next = app::epoch_catalog_operation_phase::
                    catalog_tombstoned_cleanup_pending,
                .Evidence = app::epoch_catalog_transition_evidence::
                    catalog_transaction,
                .ExpectedOperationRevision = quarantined->OperationRevision,
            },
    };
    auto finalized = session->finalize_cache_delete(
        finalize_request, cancelled_after_ponr.get_token());
    expect(
        finalized.has_value(),
        "post-PONR cancellation cannot split cache tombstoning");
    auto finalized_retry =
        session->finalize_cache_delete(finalize_request, {});
    expect(
        finalized_retry.has_value()
            && session->binding().Catalog.Revision.value()
                == live_binding.Catalog.Revision.value() + 1,
        "exact cache-delete finalizer replay adds no second revision");

    restart(304U);
    auto cleanup = recover_delete();
    expect(
        session->binding().Authority.Cache == live_binding.Authority.Cache
            && session->binding().Authority.Scope
                == live_binding.Authority.Scope
            && cleanup.Phase
                == app::epoch_catalog_operation_phase::
                    catalog_tombstoned_cleanup_pending,
        "tombstoned restart preserves ownership until cleanup completes");
    const auto cleanup_transition = app::operation_transition_request{
        .Catalog = session->binding().Catalog,
        .Operation = cleanup.Operation,
        .Kind = app::epoch_catalog_operation_kind::cache_delete,
        .Expected = app::epoch_catalog_operation_phase::
            catalog_tombstoned_cleanup_pending,
        .Next = app::epoch_catalog_operation_phase::complete,
        .Evidence =
            app::epoch_catalog_transition_evidence::exact_cleanup_complete,
        .ExpectedOperationRevision = cleanup.OperationRevision,
    };
    auto complete = session->transition_operation(
        cleanup_transition, cancelled_after_ponr.get_token());
    expect(complete.has_value(), "cache-delete cleanup reaches terminal");
    auto complete_retry = session->transition_operation(cleanup_transition, {});
    expect(
        complete_retry.has_value(),
        "exact cache-delete cleanup replay remains terminal");

    expect_failure(
        session->cache_delete_impact(session->binding().Authority.Cache, {}),
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
                == live_binding.Catalog.Revision.value() + 1,
        "new cache lifetime reparents the stable PR scope and revision");
    const auto old_cache = hex_identity(impact->Cache);
    expect(
        direct_count(
            database_path(root.path()),
            std::string{"SELECT count(*) FROM operation WHERE cache_scope=x'"}
                + old_cache + "'")
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
    auto deleted_anchors = session->exact_anchors(
        app::exact_anchor_lookup_request{
            .Catalog = session->binding().Catalog,
            .Anchors =
                {
                    anchor_identity(
                        epoch.Mutation.Epoch.Id,
                        core::comparison_anchor_kind::epoch_base),
                    review_anchor,
                },
        },
        {});
    auto retained_history = session->history(
        app::epoch_history_page_request{
            .Catalog = session->binding().Catalog,
            .Limit = 10U,
        },
        {});
    auto deleted_snapshot = session->snapshot({});
    expect(
        deleted_anchors && deleted_anchors->Anchors.size() == 2U
            && deleted_anchors->Anchors[0].Availability
                == core::anchor_availability::cache_deleted
            && deleted_anchors->Anchors[1].Availability
                == core::anchor_availability::cache_deleted
            && retained_history && retained_history->Epochs.size() == 1U
            && retained_history->Epochs.front().Id
                == epoch.Mutation.Epoch.Id
            && deleted_snapshot && deleted_snapshot->CurrentViewerReview
            && deleted_snapshot->CurrentViewerReview->Availability
                == core::anchor_availability::cache_deleted,
        "reparented history retains epoch and review cache-delete tombstones");

    auto next = session->reserve_append(
        app::append_reservation_request{
            .Context = context(
                session->binding(),
                operation(84U),
                revision(84U),
                84U,
                core::application_session_id{305U}),
            .ProposedOrdinal = core::epoch_ordinal{2},
        },
        {});
    expect(
        next && next->ReservedOrdinal == core::epoch_ordinal{2},
        "reparented PR continues its never-reused ordinal sequence");
}

void source_observation_restart_contract()
{
    temporary_root root{"source-observation-restart"};
    auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
    expect(made.has_value(), "source-observation fixture bootstraps");
    auto port = std::move(*made);
    const auto authority = authority_key(
        0xF200000000000076ULL,
        0xF200000000000077ULL,
        4312U);
    auto opened = port->open(open_request(1U, authority), {});
    expect(opened.has_value(), "source-observation authority opens");
    auto session = std::move(*opened);
    const auto first = complete_append(
        *session,
        core::epoch_ordinal{1},
        160U,
        core::application_session_id{1U},
        true);

    auto restarted_revision = core::accepted_pull_request_revision{
        .Source = first.Mutation.Epoch.Source,
        .Origin = core::pull_request_revision_origin::live,
        .BaseBranch = first.Mutation.Epoch.BaseBranch,
        .Base = first.Mutation.Epoch.Base,
        .Head = oid<core::git_commit_oid>(0x39U),
    };
    restarted_revision.Source.Observation =
        opaque<core::epoch_source_observation_id>(161U);

    session.reset();
    port.reset();
    auto remade = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
    expect(remade.has_value(), "source-observation catalog restarts");
    port = std::move(*remade);
    auto reopened = port->open(open_request(1U, authority), {});
    expect(reopened.has_value(), "source-observation authority reopens");
    session = std::move(*reopened);

    const auto second = complete_append(
        *session,
        core::epoch_ordinal{2},
        161U,
        core::application_session_id{1U},
        false,
        false,
        nullptr,
        append_fixture_options{.Accepted = restarted_revision});
    session.reset();
    port.reset();
    remade = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
    expect(remade.has_value(), "changed source observation persists");
    port = std::move(*remade);
    reopened = port->open(open_request(1U, authority), {});
    expect(reopened.has_value(), "persisted source observations reopen");
    session = std::move(*reopened);
    const auto history = session->history(
        app::epoch_history_page_request{
            .Catalog = session->binding().Catalog,
            .Limit = 10U,
        },
        {});
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
            && history->Epochs[0].Head == restarted_revision.Head
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
    temporary_root root{"retention-rehydration"};
    auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
    expect(made.has_value(), "retention-rehydration fixture bootstraps");
    auto port = std::move(*made);
    const auto authority = authority_key(
        0xF200000000000056ULL,
        0xF200000000000057ULL,
        4292U);
    auto opened = port->open(open_request(661U, authority), {});
    expect(opened.has_value(), "retention-rehydration authority opens");
    auto session = std::move(*opened);
    const auto first = complete_append(
        *session,
        core::epoch_ordinal{1},
        140U,
        core::application_session_id{661U},
        true);
    auto accepted = core::accepted_pull_request_revision{
        .Source = first.Mutation.Epoch.Source,
        .Origin = core::pull_request_revision_origin::live,
        .BaseBranch = first.Mutation.Epoch.BaseBranch,
        .Base = first.Mutation.Epoch.Base,
        .Head = first.Mutation.Epoch.Head,
    };
    accepted.Source.Observation =
        opaque<core::epoch_source_observation_id>(142U);
    constexpr auto RehydrationCause = core::epoch_cause{
        .Origin = core::epoch_origin::retention_rehydration,
        .Base = core::epoch_base_transition::unchanged,
        .Head = core::epoch_head_transition::unchanged,
        .Material = core::epoch_material_relation::different,
        .Primary = core::epoch_primary_cause::retention_rehydration,
    };
    const auto original_binding = session->binding();
    const auto restart = [&](const std::uint64_t application_session) {
        session.reset();
        port.reset();
        auto remade =
            sqlite::make_epoch_catalog({.StorageRoot = root.path()});
        expect(remade.has_value(), "retention-rehydration catalog restarts");
        port = std::move(*remade);
        auto reopened = port->open(
            open_request(application_session, authority), {});
        expect(
            reopened.has_value(),
            "retention-rehydration authority reopens");
        session = std::move(*reopened);
    };

    const auto first_deleted =
        complete_cache_delete(*session, operation(141U));
    restart(662U);
    auto required = session->snapshot({});
    expect(
        session->binding().Authority.Scope
                == original_binding.Authority.Scope
            && session->binding().Authority.Cache != first_deleted.Cache
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

    const auto reparented_binding = session->binding();
    restart(663U);
    auto required_after_restart = session->snapshot({});
    expect(
        session->binding() == reparented_binding
            && required_after_restart
            && required_after_restart->RetentionRehydration
                == core::retention_rehydration_requirement::required
            && required_after_restart->LatestLive
            && required_after_restart->LatestLive->Id
                == first.Mutation.Epoch.Id,
        "rehydration requirement and reparented binding survive a database "
        "restart before append");

    const auto second = complete_append(
        *session,
        core::epoch_ordinal{2},
        142U,
        core::application_session_id{663U},
        false,
        false,
        nullptr,
        append_fixture_options{
            .Accepted = accepted,
            .Cause = RehydrationCause,
        });
    auto restored = session->snapshot({});
    auto old_anchors = session->exact_anchors(
        app::exact_anchor_lookup_request{
            .Catalog = session->binding().Catalog,
            .Anchors =
                {
                    anchor_identity(
                        first.Mutation.Epoch.Id,
                        core::comparison_anchor_kind::epoch_base),
                    anchor_identity(
                        first.Mutation.Epoch.Id,
                        core::comparison_anchor_kind::epoch_head),
                },
        },
        {});
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
            && second.Mutation.Epoch.Cause == RehydrationCause
            && restored
            && restored->RetentionRehydration
                == core::retention_rehydration_requirement::not_required
            && restored->LatestLive
            && restored->LatestLive->Id == second.Mutation.Epoch.Id
            && restored->LatestLive->BaseAvailability
                == core::anchor_availability::available
            && restored->LatestLive->HeadAvailability
                == core::anchor_availability::available
            && old_anchors && old_anchors->Anchors.size() == 2U
            && old_anchors->Anchors[0].Availability
                == core::anchor_availability::cache_deleted
            && old_anchors->Anchors[1].Availability
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

    const auto second_deleted =
        complete_cache_delete(*session, operation(143U));
    restart(664U);
    auto required_again = session->snapshot({});
    expect(
        session->binding().Authority.Cache != second_deleted.Cache
            && session->binding().Authority.Scope
                == original_binding.Authority.Scope
            && required_again
            && required_again->RetentionRehydration
                == core::retention_rehydration_requirement::required
            && required_again->LatestLive
            && required_again->LatestLive->Id == second.Mutation.Epoch.Id
            && required_again->LatestLive->BaseAvailability
                == core::anchor_availability::cache_deleted
            && required_again->LatestLive->HeadAvailability
                == core::anchor_availability::cache_deleted,
        "a later cache lifetime independently requires one new retention "
        "epoch");

    auto accepted_again = accepted;
    accepted_again.Source.Observation =
        opaque<core::epoch_source_observation_id>(144U);
    const auto third = complete_append(
        *session,
        core::epoch_ordinal{3},
        144U,
        core::application_session_id{664U},
        false,
        false,
        nullptr,
        append_fixture_options{
            .Accepted = accepted_again,
            .Cause = RehydrationCause,
        });
    auto third_snapshot = session->snapshot({});
    expect(
        third.Mutation.Epoch.Cause == RehydrationCause && third_snapshot
            && third_snapshot->RetentionRehydration
                == core::retention_rehydration_requirement::not_required
            && third_snapshot->LatestLive
            && third_snapshot->LatestLive->Id == third.Mutation.Epoch.Id,
        "the second cache loss also admits exactly one replacement epoch");

    restart(665U);
    auto cleared_after_restart = session->snapshot({});
    expect(
        cleared_after_restart
            && cleared_after_restart->RetentionRehydration
                == core::retention_rehydration_requirement::not_required
            && cleared_after_restart->LatestLive
            && cleared_after_restart->LatestLive->Id
                == third.Mutation.Epoch.Id,
        "successful retention rehydration remains consumed after restart");

    const auto before_duplicate =
        catalog_logical_fingerprint(database_path(root.path()));
    expect_failure(
        session->reserve_append(
            app::append_reservation_request{
                .Context = context(
                    session->binding(),
                    operation(145U),
                    accepted,
                    145U,
                    core::application_session_id{665U}),
                .ProposedOrdinal = core::epoch_ordinal{4},
            },
            {}),
        app::epoch_catalog_failure::source_observation_conflict,
        "a source-observation identity already published before restart is "
        "rejected before a second operation reserves");
    expect(
        catalog_logical_fingerprint(database_path(root.path()))
                == before_duplicate,
        "rejected duplicate reservation changes no operation, epoch, anchor, "
        "pointer, revision, lifecycle, or retention row");
}

void no_latest_reparent_contract()
{
    temporary_root root{"no-latest-reparent"};
    auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
    expect(made.has_value(), "no-latest reparent fixture bootstraps");
    auto port = std::move(*made);
    const auto authority = authority_key(
        0xF200000000000066ULL,
        0xF200000000000067ULL,
        4302U);
    auto opened = port->open(open_request(671U, authority), {});
    expect(opened.has_value(), "no-latest authority opens");
    auto session = std::move(*opened);
    const auto original_binding = session->binding();
    const auto deleted = complete_cache_delete(*session, operation(150U));
    session.reset();
    port.reset();
    auto remade = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
    expect(remade.has_value(), "no-latest catalog restarts");
    port = std::move(*remade);
    auto reopened = port->open(open_request(672U, authority), {});
    expect(reopened.has_value(), "no-latest authority reparents");
    session = std::move(*reopened);
    auto snapshot = session->snapshot({});
    expect(
        session->binding().Authority.Scope
                == original_binding.Authority.Scope
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
    temporary_root root{"prune-replay-revision"};
    auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
    expect(made.has_value(), "prune-replay fixture bootstraps");
    auto port = std::move(*made);
    auto opened = port->open(open_request(681U), {});
    expect(opened.has_value(), "prune-replay authority opens");
    auto session = std::move(*opened);
    const auto epoch = complete_append(
        *session,
        core::epoch_ordinal{1},
        151U,
        core::application_session_id{681U},
        true);
    const auto retained = app::git_retained_ref_pair{
        .Scope = session->binding().Authority.Scope,
        .Ordinal = epoch.Mutation.Epoch.Id.Ordinal,
        .Base = epoch.Mutation.Epoch.Base,
        .Head = epoch.Mutation.Epoch.Head,
    };
    const auto prune_fact = app::prune_epoch_fact{
        .Epoch = epoch.Mutation.Epoch.Id,
        .Retained = retained,
        .BaseRef =
            app::epoch_catalog_ref_identity{
                .Authority = session->binding().Authority.Scope,
                .Ordinal = epoch.Mutation.Epoch.Id.Ordinal,
            },
        .HeadRef =
            app::epoch_catalog_ref_identity{
                .Authority = session->binding().Authority.Scope,
                .Ordinal = epoch.Mutation.Epoch.Id.Ordinal,
                .Head = true,
            },
    };
    const auto prune_id = operation(152U);
    const auto request = app::begin_prune_request{
        .Catalog = session->binding().Catalog,
        .Operation = prune_id,
        .Epochs = {prune_fact},
    };
    auto begun = session->begin_prune(request, {});
    expect(begun.has_value(), "prune-replay exact intent begins");
    {
        const direct_database database{database_path(root.path()), true};
        database.execute(
            std::string{"UPDATE operation SET expected_revision="}
            + std::to_string(request.Catalog.Revision.value() + 1)
            + " WHERE operation=x'" + hex_identity(prune_id) + "'");
    }
    const auto before =
        catalog_logical_fingerprint(database_path(root.path()));
    expect_failure(
        session->begin_prune(request, {}),
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
    temporary_root root{"prune-expected-availability"};
    auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
    expect(made.has_value(), "expected-availability fixture bootstraps");
    auto port = std::move(*made);
    const auto authority = authority_key(
        0xF200000000000086ULL,
        0xF200000000000087ULL,
        4322U);
    auto opened = port->open(open_request(711U, authority), {});
    expect(opened.has_value(), "expected-availability authority opens");
    auto session = std::move(*opened);
    const auto epoch = complete_append(
        *session,
        core::epoch_ordinal{1},
        170U,
        core::application_session_id{711U},
        true);

    auto reviews = std::array{
        review_association(171U, epoch.Mutation.Epoch.Head),
        review_association(172U, epoch.Mutation.Epoch.Head),
        review_association(173U, epoch.Mutation.Epoch.Head),
        review_association(174U, epoch.Mutation.Epoch.Head),
    };
    reviews[1].Availability = core::anchor_availability::unavailable;
    reviews[2].Availability = core::anchor_availability::never_observed;
    for(std::size_t index = 0U; index < reviews.size(); ++index)
    {
        auto associated = session->associate_review(
            app::associate_review_request{
                .Catalog = session->binding().Catalog,
                .Review = reviews[index],
                .AcceptedSnapshot = core::provider_snapshot_id{
                    0xFC00000000002000ULL
                    + static_cast<std::uint64_t>(index)},
            },
            {});
        expect(
            associated && associated->Changed,
            "each linked review prestate associates durably");
    }

    const auto review_anchor = [](const auto& binding, const auto& review) {
        return core::comparison_anchor_identity{
            .Authority = binding.Authority.Scope,
            .Incarnation = binding.Catalog.Incarnation,
            .Kind = core::comparison_anchor_kind::review_commit,
            .Review = review.Association,
        };
    };
    auto corrupt_review = session->mark_anchor_availability(
        app::mark_anchor_availability_request{
            .Catalog = session->binding().Catalog,
            .Changes =
                {
                    app::anchor_availability_change{
                        .Anchor = review_anchor(
                            session->binding(), reviews[3]),
                        .ExpectedCommit = reviews[3].Commit,
                        .Expected = core::anchor_availability::available,
                        .Next = core::anchor_availability::corrupt,
                    },
                },
        },
        {});
    expect(
        corrupt_review && corrupt_review->Changed,
        "linked review reaches a durable corrupt prestate");

    const auto base_anchor = anchor_identity(
        epoch.Mutation.Epoch.Id,
        core::comparison_anchor_kind::epoch_base);
    const auto head_anchor = anchor_identity(
        epoch.Mutation.Epoch.Id,
        core::comparison_anchor_kind::epoch_head);
    auto degraded = session->mark_anchor_availability(
        app::mark_anchor_availability_request{
            .Catalog = session->binding().Catalog,
            .Changes =
                {
                    app::anchor_availability_change{
                        .Anchor = base_anchor,
                        .ExpectedCommit = epoch.Mutation.Epoch.Base,
                        .Expected = core::anchor_availability::available,
                        .Next = core::anchor_availability::corrupt,
                    },
                    app::anchor_availability_change{
                        .Anchor = head_anchor,
                        .ExpectedCommit = epoch.Mutation.Epoch.Head,
                        .Expected = core::anchor_availability::available,
                        .Next = core::anchor_availability::unavailable,
                    },
                },
        },
        {});
    expect(
        degraded && degraded->Changed
            && direct_count(
                   database_path(root.path()),
                   "SELECT retained_epoch_count FROM pull_request_scope")
                == 1,
        "corrupt and unavailable retained anchors still consume one quota "
        "slot");

    const auto retained = app::git_retained_ref_pair{
        .Scope = session->binding().Authority.Scope,
        .Ordinal = epoch.Mutation.Epoch.Id.Ordinal,
        .Base = epoch.Mutation.Epoch.Base,
        .Head = epoch.Mutation.Epoch.Head,
    };
    const auto prune_fact = app::prune_epoch_fact{
        .Epoch = epoch.Mutation.Epoch.Id,
        .Retained = retained,
        .BaseRef =
            app::epoch_catalog_ref_identity{
                .Authority = session->binding().Authority.Scope,
                .Ordinal = epoch.Mutation.Epoch.Id.Ordinal,
            },
        .HeadRef =
            app::epoch_catalog_ref_identity{
                .Authority = session->binding().Authority.Scope,
                .Ordinal = epoch.Mutation.Epoch.Id.Ordinal,
                .Head = true,
            },
        .ExpectedBaseAvailability = core::anchor_availability::corrupt,
        .ExpectedHeadAvailability = core::anchor_availability::unavailable,
    };

    auto wrong_new_fact = prune_fact;
    wrong_new_fact.ExpectedBaseAvailability =
        core::anchor_availability::available;
    const auto before_wrong_new =
        catalog_logical_fingerprint(database_path(root.path()));
    expect_failure(
        session->begin_prune(
            app::begin_prune_request{
                .Catalog = session->binding().Catalog,
                .Operation = operation(175U),
                .Epochs = {wrong_new_fact},
            },
            {}),
        app::epoch_catalog_failure::authority_mismatch,
        "new prune rejects a wrong expected anchor availability");
    expect(
        catalog_logical_fingerprint(database_path(root.path()))
            == before_wrong_new,
        "wrong new-prune availability changes no logical catalog row");

    const auto request = app::begin_prune_request{
        .Catalog = session->binding().Catalog,
        .Operation = operation(176U),
        .Epochs = {prune_fact},
    };
    auto begun = session->begin_prune(request, {});
    expect(
        begun && begun->Epochs == request.Epochs
            && direct_count(
                   database_path(root.path()),
                   "SELECT base_availability * 10 + head_availability "
                   "FROM prune_operation")
                == 52,
        "prune intent persists exact corrupt and unavailable expectations");

    auto wrong_replay = request;
    wrong_replay.Epochs.front().ExpectedHeadAvailability =
        core::anchor_availability::corrupt;
    const auto before_wrong_replay =
        catalog_logical_fingerprint(database_path(root.path()));
    expect_failure(
        session->begin_prune(wrong_replay, {}),
        app::epoch_catalog_failure::operation_conflict,
        "prune replay rejects different expected availability");
    expect(
        catalog_logical_fingerprint(database_path(root.path()))
            == before_wrong_replay,
        "wrong prune replay changes no logical catalog row");

    {
        const direct_database database{database_path(root.path()), true};
        database.execute(
            "UPDATE epoch_anchor SET availability=2 WHERE endpoint=1");
    }
    const auto before_drifted_replay =
        catalog_logical_fingerprint(database_path(root.path()));
    expect_failure(
        session->begin_prune(request, {}),
        app::epoch_catalog_failure::operation_conflict,
        "prune replay verifies persisted expectation against the live anchor");
    expect(
        catalog_logical_fingerprint(database_path(root.path()))
            == before_drifted_replay,
        "drifted-anchor replay adds no compensating catalog mutation");
    {
        const direct_database database{database_path(root.path()), true};
        database.execute(
            "UPDATE epoch_anchor SET availability=5 WHERE endpoint=1");
    }

    const auto restart = [&](const std::uint64_t application_session) {
        session.reset();
        port.reset();
        auto remade =
            sqlite::make_epoch_catalog({.StorageRoot = root.path()});
        expect(remade.has_value(), "expected-availability catalog restarts");
        port = std::move(*remade);
        auto reopened = port->open(
            open_request(application_session, authority), {});
        expect(reopened.has_value(), "expected-availability authority reopens");
        session = std::move(*reopened);
    };

    restart(712U);
    auto recovery = session->recovery_page(std::nullopt, 100U, {});
    expect(
        recovery && recovery->Operations.size() == 1U
            && std::get<app::maintenance_operation_record>(
                   recovery->Operations.front())
                   .Epochs
                == request.Epochs,
        "exact prune availability intent survives restart");
    auto replayed = session->begin_prune(request, {});
    expect(
        replayed && *replayed == *begun,
        "pre-finalization replay revalidates the persisted anchor states");

    auto deleting = session->transition_operation(
        app::operation_transition_request{
            .Catalog = session->binding().Catalog,
            .Operation = replayed->Operation,
            .Kind = app::epoch_catalog_operation_kind::prune,
            .Expected = app::epoch_catalog_operation_phase::prune_intent,
            .Next = app::epoch_catalog_operation_phase::
                ref_delete_in_progress,
            .Evidence =
                app::epoch_catalog_transition_evidence::catalog_transaction,
            .ExpectedOperationRevision = replayed->OperationRevision,
        },
        {});
    expect(deleting.has_value(), "degraded-anchor prune starts ref deletion");
    const auto deleting_record =
        std::get<app::maintenance_operation_record>(*deleting);
    const auto finalize_request = app::finalize_prune_request{
        .Transition =
            app::operation_transition_request{
                .Catalog = session->binding().Catalog,
                .Operation = deleting_record.Operation,
                .Kind = app::epoch_catalog_operation_kind::prune,
                .Expected = app::epoch_catalog_operation_phase::
                    ref_delete_in_progress,
                .Next = app::epoch_catalog_operation_phase::
                    visible_cleanup_pending,
                .Evidence = app::epoch_catalog_transition_evidence::
                    exact_refs_absent,
                .ExpectedOperationRevision =
                    deleting_record.OperationRevision,
            },
        .ObservedAbsent = {retained},
    };

    {
        const direct_database database{database_path(root.path()), true};
        database.execute(
            "UPDATE epoch_anchor SET availability=5 WHERE endpoint=2");
    }
    const auto before_wrong_finalize =
        catalog_logical_fingerprint(database_path(root.path()));
    expect_failure(
        session->finalize_prune(finalize_request, {}),
        app::epoch_catalog_failure::operation_conflict,
        "finalize prune rejects a changed expected availability");
    expect(
        catalog_logical_fingerprint(database_path(root.path()))
            == before_wrong_finalize
            && direct_count(
                   database_path(root.path()),
                   "SELECT retained_epoch_count FROM pull_request_scope")
                == 1,
        "failed finalize rolls back anchor, review, revision, operation, and "
        "quota mutations");
    {
        const direct_database database{database_path(root.path()), true};
        database.execute(
            "UPDATE epoch_anchor SET availability=2 WHERE endpoint=2");
    }

    auto pruned = session->finalize_prune(finalize_request, {});
    expect(
        pruned && pruned->Changed
            && direct_count(
                   database_path(root.path()),
                   "SELECT retained_epoch_count FROM pull_request_scope")
                == 0,
        "exact degraded-anchor prune releases one retained quota slot");
    const auto after_finalize =
        catalog_logical_fingerprint(database_path(root.path()));
    auto finalize_replay = session->finalize_prune(finalize_request, {});
    expect(
        finalize_replay && *finalize_replay == *pruned
            && catalog_logical_fingerprint(database_path(root.path()))
                == after_finalize
            && direct_count(
                   database_path(root.path()),
                   "SELECT retained_epoch_count FROM pull_request_scope")
                == 0,
        "finalize replay neither decrements quota nor mutates any row twice");

    auto finalized_begin_replay = session->begin_prune(request, {});
    expect(
        finalized_begin_replay
            && finalized_begin_replay->Phase
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

    auto snapshot = session->snapshot({});
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
            .Catalog = session->binding().Catalog,
            .Operation = finalized_begin_replay->Operation,
            .Kind = app::epoch_catalog_operation_kind::prune,
            .Expected = app::epoch_catalog_operation_phase::
                visible_cleanup_pending,
            .Next = app::epoch_catalog_operation_phase::complete,
            .Evidence = app::epoch_catalog_transition_evidence::
                exact_cleanup_complete,
            .ExpectedOperationRevision =
                finalized_begin_replay->OperationRevision,
        },
        {});
    expect(complete.has_value(), "degraded-anchor prune cleanup completes");

    restart(713U);
    auto complete_replay = session->begin_prune(request, {});
    auto history = session->history(
        app::epoch_history_page_request{
            .Catalog = session->binding().Catalog,
            .Limit = 10U,
        },
        {});
    expect(
        complete_replay
            && complete_replay->Phase
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
                core::application_session_id{713U}),
            .ProposedOrdinal = core::epoch_ordinal{2},
        },
        {});
    expect(
        admitted && admitted->ReservedOrdinal == core::epoch_ordinal{2},
        "released retained quota admits the next append after restart");
}

void cache_delete_abort_restores_active_contract()
{
    temporary_root root{"cache-delete-abort"};
    auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
    expect(made.has_value(), "cache-delete-abort fixture bootstraps");
    auto port = std::move(*made);
    const auto authority = authority_key(
        0xF200000000000046ULL,
        0xF200000000000047ULL,
        4282U);
    auto opened = port->open(open_request(621U, authority), {});
    expect(opened.has_value(), "cache-delete-abort authority opens");
    auto session = std::move(*opened);
    const auto epoch = complete_append(
        *session,
        core::epoch_ordinal{1},
        130U,
        core::application_session_id{621U},
        true);
    const auto active_binding = session->binding();
    auto impact = session->cache_delete_impact(
        active_binding.Authority.Cache, {});
    expect(impact.has_value(), "abort fixture cache impact loads");
    const auto delete_id = operation(131U);
    auto begun = session->begin_cache_delete(
        app::begin_cache_delete_request{
            .Operation = delete_id,
            .Impact = *impact,
        },
        {});
    expect(begun.has_value(), "abort fixture cache delete seals");
    expect_failure(
        session->snapshot({}),
        app::epoch_catalog_failure::operation_conflict,
        "sealed cache blocks ordinary reads before abort");

    auto abort_requested = session->transition_operation(
        app::operation_transition_request{
            .Catalog = active_binding.Catalog,
            .Operation = delete_id,
            .Kind = app::epoch_catalog_operation_kind::cache_delete,
            .Expected = app::epoch_catalog_operation_phase::
                cache_delete_intent_sealed,
            .Next = app::epoch_catalog_operation_phase::abort_requested,
            .Evidence =
                app::epoch_catalog_transition_evidence::catalog_transaction,
            .ExpectedOperationRevision = begun->OperationRevision,
        },
        {});
    expect(abort_requested.has_value(), "pre-PONR cache abort requests");
    const auto& requested_record =
        std::get<app::maintenance_operation_record>(*abort_requested);
    expect(
        requested_record.Phase
            == app::epoch_catalog_operation_phase::abort_requested,
        "pre-PONR cache abort retains its typed journal phase");
    const auto final_nonterminal_revision =
        app::epoch_catalog_operation_revision{
            app::maximum_epoch_catalog_operation_revision - 1};
    {
        const direct_database database{database_path(root.path()), true};
        database.execute(
            std::string{"UPDATE operation SET operation_revision="}
            + std::to_string(final_nonterminal_revision.value())
            + " WHERE operation=x'" + hex_identity(delete_id) + "'");
    }
    const auto abort_transition = app::operation_transition_request{
        .Catalog = active_binding.Catalog,
        .Operation = delete_id,
        .Kind = app::epoch_catalog_operation_kind::cache_delete,
        .Expected = app::epoch_catalog_operation_phase::abort_requested,
        .Next = app::epoch_catalog_operation_phase::aborted,
        .Evidence =
            app::epoch_catalog_transition_evidence::exact_cleanup_complete,
        .ExpectedOperationRevision = final_nonterminal_revision,
    };
    auto aborted = session->transition_operation(abort_transition, {});
    expect(aborted.has_value(), "pre-PONR cache abort reaches terminal");
    const auto aborted_record =
        std::get<app::maintenance_operation_record>(*aborted);
    auto aborted_retry = session->transition_operation(abort_transition, {});
    expect(
        aborted_record.Phase == app::epoch_catalog_operation_phase::aborted
            && aborted_record.OperationRevision.value()
                == app::maximum_epoch_catalog_operation_revision
            && aborted_retry
            && std::get<app::maintenance_operation_record>(*aborted_retry)
                == aborted_record,
        "exact cache-abort retry remains stable");
    expect(
        direct_count(
            database_path(root.path()),
            std::string{"SELECT count(*) FROM repository_scope WHERE "
                        "cache_scope=x'"}
                + hex_identity(active_binding.Authority.Cache)
                + "' AND lifecycle=1")
                == 1
            && direct_count(
                   database_path(root.path()),
                   std::string{"SELECT count(*) FROM operation WHERE "
                               "operation=x'"}
                       + hex_identity(delete_id)
                       + "' AND phase=11 AND terminal=2 "
                         "AND operation_revision=9223372036854775807")
                == 1,
        "cache abort atomically restores active lifecycle with terminal journal");

    auto restored_snapshot = session->snapshot({});
    auto restored_impact = session->cache_delete_impact(
        active_binding.Authority.Cache, {});
    expect(
        restored_snapshot
            && restored_snapshot->Binding.Catalog == active_binding.Catalog
            && restored_snapshot->LatestLive
            && restored_snapshot->LatestLive->Id == epoch.Mutation.Epoch.Id
            && restored_impact && *restored_impact == *impact,
        "cache abort restores ordinary snapshot and impact access");

    auto append = session->reserve_append(
        app::append_reservation_request{
            .Context = context(
                session->binding(),
                operation(132U),
                revision(132U),
                132U,
                core::application_session_id{621U}),
            .ProposedOrdinal = core::epoch_ordinal{2},
        },
        {});
    expect(append.has_value(), "cache abort restores append access");
    auto append_abort_requested = session->transition_operation(
        app::operation_transition_request{
            .Catalog = session->binding().Catalog,
            .Operation = append->Context.Operation,
            .Kind = app::epoch_catalog_operation_kind::append,
            .Expected = app::epoch_catalog_operation_phase::preparing,
            .Next = app::epoch_catalog_operation_phase::abort_requested,
            .Evidence =
                app::epoch_catalog_transition_evidence::catalog_transaction,
            .ExpectedOperationRevision = append->OperationRevision,
        },
        {});
    expect(append_abort_requested.has_value(), "probe append abort requests");
    const auto& append_requested =
        std::get<app::append_operation_record>(*append_abort_requested);
    auto append_aborted = session->transition_operation(
        app::operation_transition_request{
            .Catalog = session->binding().Catalog,
            .Operation = append_requested.Context.Operation,
            .Kind = app::epoch_catalog_operation_kind::append,
            .Expected = app::epoch_catalog_operation_phase::abort_requested,
            .Next = app::epoch_catalog_operation_phase::aborted,
            .Evidence =
                app::epoch_catalog_transition_evidence::exact_cleanup_complete,
            .ExpectedOperationRevision = append_requested.OperationRevision,
        },
        {});
    expect(append_aborted.has_value(), "probe append abort completes");

    session.reset();
    port.reset();
    auto remade = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
    expect(remade.has_value(), "aborted cache catalog restarts");
    port = std::move(*remade);
    auto reopened = port->open(open_request(622U, authority), {});
    expect(reopened.has_value(), "aborted cache authority reopens active");
    session = std::move(*reopened);
    auto restart_snapshot = session->snapshot({});
    auto restart_impact = session->cache_delete_impact(
        session->binding().Authority.Cache, {});
    auto recovery = session->recovery_page(std::nullopt, 100U, {});
    auto expected_restart_snapshot = *restored_snapshot;
    expected_restart_snapshot.NextOrdinal = core::epoch_ordinal{3};
    expect(
        session->binding() == active_binding && restart_snapshot
            && restored_snapshot->NextOrdinal == core::epoch_ordinal{2}
            && *restart_snapshot == expected_restart_snapshot
            && restart_impact && *restart_impact == *impact && recovery
            && recovery->Operations.empty(),
        "aborted cache restart preserves the restored snapshot and impact, "
        "advances the never-reused ordinal, and has no recovery handoff");
}

void tombstoned_cache_repair_contract()
{
    temporary_root root{"tombstoned-cache-repair"};
    auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
    expect(made.has_value(), "tombstoned-repair fixture bootstraps");
    auto port = std::move(*made);
    const auto authority = authority_key(
        0xF200000000000026ULL,
        0xF200000000000027ULL,
        4262U);
    auto opened = port->open(open_request(601U, authority), {});
    expect(opened.has_value(), "tombstoned-repair authority opens");
    auto session = std::move(*opened);
    const auto epoch = complete_append(
        *session,
        core::epoch_ordinal{1},
        110U,
        core::application_session_id{601U},
        true);
    const auto review = review_association(111U, epoch.Mutation.Epoch.Head);
    auto associated = session->associate_review(
        app::associate_review_request{
            .Catalog = session->binding().Catalog,
            .Review = review,
            .AcceptedSnapshot = core::provider_snapshot_id{
                0xFC00000000000111ULL},
        },
        {});
    expect(associated && associated->Changed, "repair fixture links a review");
    const auto live_binding = session->binding();

    auto impact = session->cache_delete_impact(
        session->binding().Authority.Cache, {});
    expect(impact.has_value(), "tombstoned-repair impact loads");
    auto begun = session->begin_cache_delete(
        app::begin_cache_delete_request{
            .Operation = operation(112U),
            .Impact = *impact,
        },
        {});
    expect(begun.has_value(), "tombstoned-repair delete seals");
    auto renaming = session->transition_operation(
        app::operation_transition_request{
            .Catalog = session->binding().Catalog,
            .Operation = begun->Operation,
            .Kind = app::epoch_catalog_operation_kind::cache_delete,
            .Expected = app::epoch_catalog_operation_phase::
                cache_delete_intent_sealed,
            .Next = app::epoch_catalog_operation_phase::
                quarantine_rename_in_progress,
            .Evidence =
                app::epoch_catalog_transition_evidence::catalog_transaction,
            .ExpectedOperationRevision = begun->OperationRevision,
        },
        {});
    expect(renaming.has_value(), "tombstoned-repair rename starts");
    const auto& renaming_record =
        std::get<app::maintenance_operation_record>(*renaming);
    auto quarantined = session->transition_operation(
        app::operation_transition_request{
            .Catalog = session->binding().Catalog,
            .Operation = renaming_record.Operation,
            .Kind = app::epoch_catalog_operation_kind::cache_delete,
            .Expected = app::epoch_catalog_operation_phase::
                quarantine_rename_in_progress,
            .Next = app::epoch_catalog_operation_phase::quarantined,
            .Evidence =
                app::epoch_catalog_transition_evidence::exact_quarantine,
            .ExpectedOperationRevision = renaming_record.OperationRevision,
        },
        {});
    expect(quarantined.has_value(), "tombstoned-repair quarantine completes");
    const auto& quarantined_record =
        std::get<app::maintenance_operation_record>(*quarantined);
    auto finalized = session->finalize_cache_delete(
        app::finalize_cache_delete_request{
            .Transition =
                app::operation_transition_request{
                    .Catalog = session->binding().Catalog,
                    .Operation = quarantined_record.Operation,
                    .Kind = app::epoch_catalog_operation_kind::cache_delete,
                    .Expected =
                        app::epoch_catalog_operation_phase::quarantined,
                    .Next = app::epoch_catalog_operation_phase::
                        catalog_tombstoned_cleanup_pending,
                    .Evidence = app::epoch_catalog_transition_evidence::
                        catalog_transaction,
                    .ExpectedOperationRevision =
                        quarantined_record.OperationRevision,
                },
        },
        {});
    expect(finalized.has_value(), "repair fixture atomically tombstones cache");
    auto page = session->recovery_page(std::nullopt, 100U, {});
    expect(page && page->Operations.size() == 1U, "phase-19 record recovers");
    const auto cleanup =
        std::get<app::maintenance_operation_record>(page->Operations.front());
    expect(
        cleanup.Phase
            == app::epoch_catalog_operation_phase::
                catalog_tombstoned_cleanup_pending,
        "cache delete reaches catalog-tombstoned cleanup pending");
    auto repaired = session->transition_operation(
        app::operation_transition_request{
            .Catalog = session->binding().Catalog,
            .Operation = cleanup.Operation,
            .Kind = app::epoch_catalog_operation_kind::cache_delete,
            .Expected = app::epoch_catalog_operation_phase::
                catalog_tombstoned_cleanup_pending,
            .Next = app::epoch_catalog_operation_phase::repair_required,
            .Evidence =
                app::epoch_catalog_transition_evidence::repair_ambiguity,
            .ExpectedOperationRevision = cleanup.OperationRevision,
        },
        {});
    expect(repaired.has_value(), "phase-19 cleanup can enter durable repair");
    const auto repair_record =
        std::get<app::maintenance_operation_record>(*repaired);
    expect(
        repair_record.Phase
                == app::epoch_catalog_operation_phase::repair_required
            && repair_record.RepairFromPhase
                == app::epoch_catalog_operation_phase::
                    catalog_tombstoned_cleanup_pending
            && repair_record.Scopes == cleanup.Scopes,
        "repair records exact phase-19 origin and sealed scope facts");

    const auto review_anchor = core::comparison_anchor_identity{
        .Authority = epoch.Mutation.Epoch.Id.Authority,
        .Incarnation = epoch.Mutation.Epoch.Id.Incarnation,
        .Kind = core::comparison_anchor_kind::review_commit,
        .Review = review.Association,
    };
    const auto assert_tombstones = [&]() {
        auto snapshot = session->snapshot({});
        auto history = session->history(
            app::epoch_history_page_request{
                .Catalog = session->binding().Catalog,
                .Limit = 10U,
            },
            {});
        auto anchors = session->exact_anchors(
            app::exact_anchor_lookup_request{
                .Catalog = session->binding().Catalog,
                .Anchors =
                    {
                        anchor_identity(
                            epoch.Mutation.Epoch.Id,
                            core::comparison_anchor_kind::epoch_base),
                        anchor_identity(
                            epoch.Mutation.Epoch.Id,
                            core::comparison_anchor_kind::epoch_head),
                        review_anchor,
                    },
            },
            {});
        expect(
            snapshot && snapshot->CurrentViewerReview
                && snapshot->CurrentViewerReview->Availability
                    == core::anchor_availability::cache_deleted
                && history && history->Epochs.size() == 1U
                && history->Epochs.front().Id == epoch.Mutation.Epoch.Id
                && anchors && anchors->Anchors.size() == 3U
                && std::ranges::all_of(
                    anchors->Anchors,
                    [](const core::comparison_anchor& anchor) {
                        return anchor.Availability
                            == core::anchor_availability::cache_deleted;
                    }),
            "phase-19 repair keeps all tombstoned read surfaces coherent");
    };
    assert_tombstones();
    const auto repair_binding = session->binding();
    expect(
        repair_binding.Catalog.Revision.value()
            == live_binding.Catalog.Revision.value() + 1,
        "phase-19 repair retains the single tombstone revision advance");

    session.reset();
    port.reset();
    auto remade = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
    expect(remade.has_value(), "phase-19 repair catalog restarts");
    port = std::move(*remade);
    auto reopened = port->open(open_request(602U, authority), {});
    expect(reopened.has_value(), "phase-19 repair authority reopens");
    session = std::move(*reopened);
    auto recovered = session->recovery_page(std::nullopt, 100U, {});
    expect(
        session->binding() == repair_binding && recovered
            && recovered->Operations.size() == 1U
            && std::get<app::maintenance_operation_record>(
                   recovered->Operations.front())
                   .RepairFromPhase
                == app::epoch_catalog_operation_phase::
                    catalog_tombstoned_cleanup_pending,
        "phase-19 repair origin and tombstoned binding survive restart");
    assert_tombstones();
}

void cache_delete_supersession_exhaustion_contract()
{
    temporary_root root{"cache-supersession-exhaustion"};
    auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
    expect(made.has_value(), "supersession-exhaustion fixture bootstraps");
    auto port = std::move(*made);
    const auto authority = authority_key(
        0xF200000000000036ULL,
        0xF200000000000037ULL,
        4272U);
    auto opened = port->open(open_request(611U, authority), {});
    expect(opened.has_value(), "supersession-exhaustion authority opens");
    auto session = std::move(*opened);
    const auto epoch = complete_append(
        *session,
        core::epoch_ordinal{1},
        120U,
        core::application_session_id{611U},
        true);
    const auto review = review_association(121U, epoch.Mutation.Epoch.Head);
    auto associated = session->associate_review(
        app::associate_review_request{
            .Catalog = session->binding().Catalog,
            .Review = review,
            .AcceptedSnapshot = core::provider_snapshot_id{
                0xFC00000000000121ULL},
        },
        {});
    expect(
        associated && associated->Changed,
        "supersession-exhaustion fixture links a review");

    const auto repair_id = operation(122U);
    auto reserved = session->reserve_append(
        app::append_reservation_request{
            .Context = context(
                session->binding(),
                repair_id,
                revision(122U),
                122U,
                core::application_session_id{611U}),
            .ProposedOrdinal = core::epoch_ordinal{2},
        },
        {});
    expect(reserved.has_value(), "superseded append reserves");
    auto spooled = session->record_spool(spool_request(*reserved, 122U), {});
    expect(spooled.has_value(), "superseded append retains recovery input");
    auto repaired = session->transition_operation(
        app::operation_transition_request{
            .Catalog = session->binding().Catalog,
            .Operation = repair_id,
            .Kind = app::epoch_catalog_operation_kind::append,
            .Expected = app::epoch_catalog_operation_phase::spool_recorded,
            .Next = app::epoch_catalog_operation_phase::repair_required,
            .Evidence =
                app::epoch_catalog_transition_evidence::repair_ambiguity,
            .ExpectedOperationRevision = spooled->OperationRevision,
        },
        {});
    expect(repaired.has_value(), "superseded append enters repair");
    {
        const direct_database database{database_path(root.path()), true};
        database.execute(
            std::string{"UPDATE operation SET operation_revision="}
            + std::to_string(std::numeric_limits<std::int64_t>::max())
            + " WHERE operation=x'" + hex_identity(repair_id) + "'");
    }

    auto impact = session->cache_delete_impact(
        session->binding().Authority.Cache, {});
    expect(impact.has_value(), "repair-bearing cache impact loads");
    const auto delete_id = operation(123U);
    auto begun = session->begin_cache_delete(
        app::begin_cache_delete_request{
            .Operation = delete_id,
            .Impact = *impact,
        },
        {});
    expect(begun.has_value(), "repair-bearing cache delete seals");
    auto renaming = session->transition_operation(
        app::operation_transition_request{
            .Catalog = session->binding().Catalog,
            .Operation = delete_id,
            .Kind = app::epoch_catalog_operation_kind::cache_delete,
            .Expected = app::epoch_catalog_operation_phase::
                cache_delete_intent_sealed,
            .Next = app::epoch_catalog_operation_phase::
                quarantine_rename_in_progress,
            .Evidence =
                app::epoch_catalog_transition_evidence::catalog_transaction,
            .ExpectedOperationRevision = begun->OperationRevision,
        },
        {});
    expect(renaming.has_value(), "repair-bearing cache rename starts");
    const auto& renaming_record =
        std::get<app::maintenance_operation_record>(*renaming);
    auto quarantined = session->transition_operation(
        app::operation_transition_request{
            .Catalog = session->binding().Catalog,
            .Operation = delete_id,
            .Kind = app::epoch_catalog_operation_kind::cache_delete,
            .Expected = app::epoch_catalog_operation_phase::
                quarantine_rename_in_progress,
            .Next = app::epoch_catalog_operation_phase::quarantined,
            .Evidence =
                app::epoch_catalog_transition_evidence::exact_quarantine,
            .ExpectedOperationRevision = renaming_record.OperationRevision,
        },
        {});
    expect(quarantined.has_value(), "repair-bearing cache quarantines");
    const auto& quarantined_record =
        std::get<app::maintenance_operation_record>(*quarantined);

    const auto pr_scope = hex_identity(session->binding().Authority.Scope);
    const auto cache_scope = hex_identity(session->binding().Authority.Cache);
    const auto fingerprint = [&](const direct_database& database) {
        return std::array<std::int64_t, 11U>{
            database.integer(
                std::string{"SELECT operation_revision FROM operation WHERE "
                            "operation=x'"}
                + hex_identity(repair_id) + "'"),
            database.integer(
                std::string{"SELECT phase * 10 + terminal FROM operation "
                            "WHERE operation=x'"}
                + hex_identity(repair_id) + "'"),
            database.integer(
                std::string{"SELECT count(*) FROM append_receipt WHERE "
                            "operation=x'"}
                + hex_identity(repair_id) + "'"),
            database.integer(
                std::string{"SELECT count(*) FROM append_inventory WHERE "
                            "operation=x'"}
                + hex_identity(repair_id) + "'"),
            database.integer(
                std::string{"SELECT catalog_revision FROM pull_request_scope "
                            "WHERE pr_scope=x'"}
                + pr_scope + "'"),
            database.integer(
                std::string{"SELECT lifecycle FROM pull_request_scope WHERE "
                            "pr_scope=x'"}
                + pr_scope + "'"),
            database.integer(
                std::string{"SELECT sum(availability) FROM epoch_anchor WHERE "
                            "pr_scope=x'"}
                + pr_scope + "'"),
            database.integer(
                std::string{"SELECT sum(availability) FROM review_association "
                            "WHERE pr_scope=x'"}
                + pr_scope + "'"),
            database.integer(
                std::string{"SELECT lifecycle FROM repository_scope WHERE "
                            "cache_scope=x'"}
                + cache_scope + "'"),
            database.integer(
                std::string{"SELECT operation_revision FROM operation WHERE "
                            "operation=x'"}
                + hex_identity(delete_id) + "'"),
            database.integer(
                std::string{"SELECT phase * 10 + terminal FROM operation "
                            "WHERE operation=x'"}
                + hex_identity(delete_id) + "'"),
        };
    };
    std::array<std::int64_t, 11U> before{};
    {
        const direct_database database{database_path(root.path())};
        before = fingerprint(database);
    }
    auto rejected = session->finalize_cache_delete(
        app::finalize_cache_delete_request{
            .Transition =
                app::operation_transition_request{
                    .Catalog = session->binding().Catalog,
                    .Operation = delete_id,
                    .Kind = app::epoch_catalog_operation_kind::cache_delete,
                    .Expected =
                        app::epoch_catalog_operation_phase::quarantined,
                    .Next = app::epoch_catalog_operation_phase::
                        catalog_tombstoned_cleanup_pending,
                    .Evidence = app::epoch_catalog_transition_evidence::
                        catalog_transaction,
                    .ExpectedOperationRevision =
                        quarantined_record.OperationRevision,
                },
        },
        {});
    expect_failure(
        rejected,
        app::epoch_catalog_failure::operation_conflict,
        "saturated repair supersession fails with a typed conflict");
    {
        const direct_database database{database_path(root.path())};
        expect(
            fingerprint(database) == before,
            "failed supersession changes neither repair input, PR revision and "
            "lifecycle, anchors, reviews, repository, nor delete operation");
    }
}

void repair_fail_closed_recovery_contract()
{
    {
        temporary_root root{"append-repair"};
        auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
        expect(made.has_value(), "append-repair fixture bootstraps");
        auto port = std::move(*made);
        auto opened = port->open(open_request(401U), {});
        expect(opened.has_value(), "append-repair authority opens");
        auto session = std::move(*opened);
        auto reserved = session->reserve_append(
            app::append_reservation_request{
                .Context = context(
                    session->binding(),
                    operation(90U),
                    revision(90U),
                    90U,
                    core::application_session_id{401U}),
                .ProposedOrdinal = core::epoch_ordinal{1},
            },
            {});
        expect(reserved.has_value(), "append repair operation reserves");
        auto spooled =
            session->record_spool(spool_request(*reserved, 90U), {});
        expect(spooled.has_value(), "append repair input records durably");
        const auto repair_transition = app::operation_transition_request{
            .Catalog = session->binding().Catalog,
            .Operation = spooled->Context.Operation,
            .Kind = app::epoch_catalog_operation_kind::append,
            .Expected = app::epoch_catalog_operation_phase::spool_recorded,
            .Next = app::epoch_catalog_operation_phase::repair_required,
            .Evidence =
                app::epoch_catalog_transition_evidence::repair_ambiguity,
            .ExpectedOperationRevision = spooled->OperationRevision,
        };
        auto repaired = session->transition_operation(repair_transition, {});
        expect(repaired.has_value(), "append enters durable repair state");
        const auto& repair_record =
            std::get<app::append_operation_record>(*repaired);
        expect(
            repair_record.Phase
                    == app::epoch_catalog_operation_phase::repair_required
                && repair_record.RepairFromPhase
                    == app::epoch_catalog_operation_phase::spool_recorded
                && !repair_record.RecoveryInputCompacted
                && repair_record.Spool == spooled->Spool
                && repair_record.Preimport == spooled->Preimport
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
            session->snapshot({}),
            app::epoch_catalog_failure::repair_required,
            "append repair fail-closes ordinary PR reads");
        auto page = session->recovery_page(std::nullopt, 100U, {});
        expect(
            page && page->Operations.size() == 1U
                && std::get<app::append_operation_record>(
                       page->Operations.front())
                       .RepairFromPhase
                    == app::epoch_catalog_operation_phase::spool_recorded
                && std::get<app::append_operation_record>(
                       page->Operations.front())
                       .Spool
                    == repair_record.Spool,
            "append repair preserves exact scoped recovery evidence");
        const auto binding = session->binding();
        session.reset();
        port.reset();
        auto remade =
            sqlite::make_epoch_catalog({.StorageRoot = root.path()});
        expect(remade.has_value(), "append-repair catalog restarts");
        port = std::move(*remade);
        auto reopened = port->open(open_request(402U), {});
        expect(reopened.has_value(), "append-repair authority reopens");
        session = std::move(*reopened);
        auto recovered = session->recovery_page(std::nullopt, 100U, {});
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
                    == repair_record.Preimport
                && direct_count(
                       database_path(root.path()),
                       "SELECT count(*) FROM append_inventory")
                    == 1,
            "append repair origin, authority, and exact input survive restart");
    }

    {
        temporary_root root{"prune-repair"};
        auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
        expect(made.has_value(), "prune-repair fixture bootstraps");
        auto port = std::move(*made);
        auto opened = port->open(open_request(411U), {});
        expect(opened.has_value(), "prune-repair authority opens");
        auto session = std::move(*opened);
        const auto epoch = complete_append(
            *session,
            core::epoch_ordinal{1},
            91U,
            core::application_session_id{411U},
            true);
        const auto retained = app::git_retained_ref_pair{
            .Scope = session->binding().Authority.Scope,
            .Ordinal = epoch.Mutation.Epoch.Id.Ordinal,
            .Base = epoch.Mutation.Epoch.Base,
            .Head = epoch.Mutation.Epoch.Head,
        };
        const auto prune_fact = app::prune_epoch_fact{
            .Epoch = epoch.Mutation.Epoch.Id,
            .Retained = retained,
            .BaseRef =
                app::epoch_catalog_ref_identity{
                    .Authority = session->binding().Authority.Scope,
                    .Ordinal = epoch.Mutation.Epoch.Id.Ordinal,
                },
            .HeadRef =
                app::epoch_catalog_ref_identity{
                    .Authority = session->binding().Authority.Scope,
                    .Ordinal = epoch.Mutation.Epoch.Id.Ordinal,
                    .Head = true,
                },
        };
        auto begun = session->begin_prune(
            app::begin_prune_request{
                .Catalog = session->binding().Catalog,
                .Operation = operation(92U),
                .Epochs = {prune_fact},
            },
            {});
        expect(begun.has_value(), "prune repair intent begins");
        auto deleting = session->transition_operation(
            app::operation_transition_request{
                .Catalog = session->binding().Catalog,
                .Operation = begun->Operation,
                .Kind = app::epoch_catalog_operation_kind::prune,
                .Expected = app::epoch_catalog_operation_phase::prune_intent,
                .Next = app::epoch_catalog_operation_phase::
                    ref_delete_in_progress,
                .Evidence = app::epoch_catalog_transition_evidence::
                    catalog_transaction,
                .ExpectedOperationRevision = begun->OperationRevision,
            },
            {});
        expect(deleting.has_value(), "prune crosses its cancellation point");
        const auto* deleting_record =
            std::get_if<app::maintenance_operation_record>(&*deleting);
        expect(deleting_record != nullptr, "prune recovery remains typed");
        std::stop_source cancelled;
        cancelled.request_stop();
        auto repaired = session->transition_operation(
            app::operation_transition_request{
                .Catalog = session->binding().Catalog,
                .Operation = deleting_record->Operation,
                .Kind = app::epoch_catalog_operation_kind::prune,
                .Expected = app::epoch_catalog_operation_phase::
                    ref_delete_in_progress,
                .Next = app::epoch_catalog_operation_phase::repair_required,
                .Evidence =
                    app::epoch_catalog_transition_evidence::repair_ambiguity,
                .ExpectedOperationRevision =
                    deleting_record->OperationRevision,
            },
            cancelled.get_token());
        expect(repaired.has_value(), "post-PONR prune repair ignores cancellation");
        const auto& repair_record =
            std::get<app::maintenance_operation_record>(*repaired);
        expect(
            repair_record.RepairFromPhase
                == app::epoch_catalog_operation_phase::
                    ref_delete_in_progress,
            "prune repair records its post-PONR origin");
        expect_failure(
            session->snapshot({}),
            app::epoch_catalog_failure::repair_required,
            "prune repair fail-closes ordinary PR reads");
        auto page = session->recovery_page(std::nullopt, 100U, {});
        expect(
            page && page->Operations.size() == 1U
                && std::get<app::maintenance_operation_record>(
                       page->Operations.front())
                       .Epochs
                    == std::vector{prune_fact},
            "prune repair retains exact refs and epoch recovery evidence");
    }

    {
        temporary_root root{"cache-repair"};
        auto made = sqlite::make_epoch_catalog({.StorageRoot = root.path()});
        expect(made.has_value(), "cache-repair fixture bootstraps");
        auto port = std::move(*made);
        auto opened = port->open(open_request(421U), {});
        expect(opened.has_value(), "cache-repair authority opens");
        auto session = std::move(*opened);
        (void)complete_append(
            *session,
            core::epoch_ordinal{1},
            93U,
            core::application_session_id{421U},
            true);
        auto impact = session->cache_delete_impact(
            session->binding().Authority.Cache, {});
        expect(impact.has_value(), "cache-repair impact loads");
        auto begun = session->begin_cache_delete(
            app::begin_cache_delete_request{
                .Operation = operation(94U),
                .Impact = *impact,
            },
            {});
        expect(begun.has_value(), "cache-repair delete seal begins");
        auto renaming = session->transition_operation(
            app::operation_transition_request{
                .Catalog = session->binding().Catalog,
                .Operation = begun->Operation,
                .Kind = app::epoch_catalog_operation_kind::cache_delete,
                .Expected = app::epoch_catalog_operation_phase::
                    cache_delete_intent_sealed,
                .Next = app::epoch_catalog_operation_phase::
                    quarantine_rename_in_progress,
                .Evidence = app::epoch_catalog_transition_evidence::
                    catalog_transaction,
                .ExpectedOperationRevision = begun->OperationRevision,
            },
            {});
        expect(renaming.has_value(), "cache repair reaches rename PONR");
        const auto* renaming_record =
            std::get_if<app::maintenance_operation_record>(&*renaming);
        expect(renaming_record != nullptr, "cache rename recovery is typed");
        auto repaired = session->transition_operation(
            app::operation_transition_request{
                .Catalog = session->binding().Catalog,
                .Operation = renaming_record->Operation,
                .Kind = app::epoch_catalog_operation_kind::cache_delete,
                .Expected = app::epoch_catalog_operation_phase::
                    quarantine_rename_in_progress,
                .Next = app::epoch_catalog_operation_phase::repair_required,
                .Evidence =
                    app::epoch_catalog_transition_evidence::repair_ambiguity,
                .ExpectedOperationRevision =
                    renaming_record->OperationRevision,
            },
            {});
        expect(repaired.has_value(), "cache delete enters repair state");
        const auto& repair_record =
            std::get<app::maintenance_operation_record>(*repaired);
        expect(
            repair_record.RepairFromPhase
                == app::epoch_catalog_operation_phase::
                    quarantine_rename_in_progress,
            "cache repair records its post-PONR origin");
        expect_failure(
            session->snapshot({}),
            app::epoch_catalog_failure::operation_conflict,
            "cache repair fail-closes child reads");
        const auto binding = session->binding();
        session.reset();
        port.reset();
        auto remade =
            sqlite::make_epoch_catalog({.StorageRoot = root.path()});
        expect(remade.has_value(), "cache-repair catalog restarts");
        port = std::move(*remade);
        auto reopened = port->open(open_request(422U), {});
        expect(reopened.has_value(), "cache-repair authority reopens");
        session = std::move(*reopened);
        auto page = session->recovery_page(std::nullopt, 100U, {});
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
{
    append_snapshot_history_anchor_contract();
    reservation_conflict_and_never_reuse_contract();
    canonical_review_read_contract();
    ordinal_exhaustion_snapshot_contract();
    stale_session_and_association_noop_contract();
    creation_validation_atomicity_contract();
    install_lineage_durability_contract();
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
}
