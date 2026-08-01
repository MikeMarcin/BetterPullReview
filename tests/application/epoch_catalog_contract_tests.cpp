#include "bpr/application/epoch_catalog_port.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace
{
using namespace bpr;

void expect(const bool condition, const std::string_view message)
{
    if(!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

template<typename Identity>
[[nodiscard]] constexpr auto opaque(const std::uint8_t seed) -> Identity
{
    auto bytes = std::array<std::uint8_t, 32U>{};
    bytes.front() = seed;
    return Identity{bytes};
}

[[nodiscard]] constexpr auto commit(const std::uint8_t seed)
    -> core::git_commit_oid
{
    auto digest = std::array<std::uint8_t, 32U>{};
    digest.front() = seed;
    return core::git_commit_oid{
        core::git_hash_algorithm::sha1,
        digest,
    };
}

[[nodiscard]] constexpr auto pack(const std::uint8_t seed)
    -> application::git_pack_oid
{
    auto digest = std::array<std::uint8_t, 32U>{};
    digest.front() = seed;
    return {
        .Algorithm = core::git_hash_algorithm::sha1,
        .Digest = digest,
    };
}

[[nodiscard]] constexpr auto artifact(
    const application::git_artifact_role role,
    const std::uint8_t seed,
    const std::uint64_t bytes) -> application::git_artifact_fact
{
    auto identity = std::array<std::uint8_t, 32U>{};
    identity[0] = 1U;
    identity[1] = 1U;
    identity[2] = 1U;
    identity[8] = seed;
    return {
        .Role = role,
        .Identity = application::git_artifact_identity{identity},
        .Bytes = bytes,
    };
}

[[nodiscard]] constexpr auto authority_key()
    -> application::epoch_catalog_authority_key
{
    return {
        .Profile = core::host_profile_id{1U},
        .ProfileRevision = core::github_profile_revision{2U},
        .Host = core::git_host_id{3U},
        .HostAuthority =
            opaque<core::host_authority_fingerprint>(4U),
        .Account = core::github_account_id{5U},
        .Viewer = core::github_viewer_actor_id{6U},
        .Repository = core::repository_id{7U},
        .PullRequest = core::pull_request_id{8U},
        .Number = core::pull_request_number{9U},
        .ObjectFormat = core::git_hash_algorithm::sha1,
    };
}

void authority_contract()
{
    const auto key = authority_key();
    expect(application::valid(key), "complete authenticated key is valid");

    auto invalid = key;
    invalid.ProfileRevision = core::github_profile_revision{};
    expect(
        !application::valid(invalid),
        "missing profile revision is rejected");

    const auto request = application::epoch_catalog_open_request{
        .ApplicationSession = core::application_session_id{10U},
        .Authority = key,
    };
    expect(
        application::valid(request),
        "session plus pre-scope authority opens the catalog");

    invalid = key;
    invalid.ObjectFormat = core::git_hash_algorithm::invalid;
    expect(
        !application::valid(invalid),
        "invalid object format is rejected before persistence");
}

void source_observation_identity_contract()
{
    const auto first = core::pull_request_source_revision{
        .Observation =
            opaque<core::epoch_source_observation_id>(0x21U),
        .Snapshot = core::provider_snapshot_id{1U},
        .IdentityGeneration = core::github_identity_generation{1U},
        .PullRequestGeneration = core::request_generation{1U},
    };
    auto restarted_process = first;
    restarted_process.Observation =
        opaque<core::epoch_source_observation_id>(0x22U);
    expect(
        core::valid(first) && core::valid(restarted_process)
            && first != restarted_process
            && first.Snapshot == restarted_process.Snapshot
            && first.IdentityGeneration
                == restarted_process.IdentityGeneration
            && first.PullRequestGeneration
                == restarted_process.PullRequestGeneration,
        "fresh 256-bit source-observation identity distinguishes accepted "
        "observations after process-local counters reset");

    restarted_process.Observation = {};
    expect(
        !core::valid(restarted_process),
        "zero source-observation identity is rejected at the port boundary");
}

void quota_contract()
{
    expect(
        application::maximum_retained_epoch_count_per_pull_request
            == application::maximum_git_cache_private_ref_count / 2U,
        "one retained epoch consumes exactly one base/head private-ref pair");
    expect(
        application::maximum_retained_epoch_count_per_pull_request > 0U,
        "the retained-epoch ceiling is a usable hard limit");
}

void destructive_availability_contract()
{
    auto request = application::mark_anchor_availability_request{
        .Catalog =
            core::epoch_catalog_identity{
                .Incarnation =
                    opaque<core::epoch_catalog_incarnation_id>(31U),
                .Revision = core::epoch_catalog_revision{1},
            },
        .Changes =
            {
                application::anchor_availability_change{
                    .Anchor =
                        core::comparison_anchor_identity{
                            .Authority = opaque<
                                core::pull_request_authority_scope_id>(32U),
                            .Incarnation = opaque<
                                core::epoch_catalog_incarnation_id>(31U),
                            .Kind =
                                core::comparison_anchor_kind::epoch_base,
                            .Epoch = core::epoch_ordinal{1},
                        },
                    .ExpectedCommit = commit(33U),
                    .Expected = core::anchor_availability::available,
                    .Next = core::anchor_availability::corrupt,
                },
            },
    };
    expect(
        application::valid(request),
        "generic availability mutation may record corruption");

    request.Changes.front().Expected = core::anchor_availability::corrupt;
    expect(
        !application::valid(request),
        "an unchanged availability fact is not a catalog mutation");
    request.Changes.front().Expected =
        core::anchor_availability::available;
    request.Changes.front().Next = core::anchor_availability::pruned;
    expect(
        !application::valid(request),
        "pruned state requires the exact durable prune protocol");
    request.Changes.front().Next =
        core::anchor_availability::cache_deleted;
    expect(
        !application::valid(request),
        "cache-deleted state requires the sealed cache-delete protocol");
    request.Changes.front().Expected = core::anchor_availability::pruned;
    request.Changes.front().Next = core::anchor_availability::corrupt;
    expect(
        !application::valid(request),
        "a destructive tombstone cannot be rewritten by generic recovery");
}

void creation_availability_contract()
{
    auto review = core::catalog_review_association{
        .Association = opaque<core::review_association_id>(41U),
        .Review = core::github_review_id{42U},
        .Actor = core::github_viewer_actor_id{43U},
        .Order = core::github_review_order{1U},
        .State = core::github_review_state::approved,
        .Commit = commit(44U),
        .Availability = core::anchor_availability::available,
    };
    expect(
        application::valid_new_review_association(review),
        "a newly retained review association may be available");
    review.Availability = core::anchor_availability::never_observed;
    expect(
        application::valid_new_review_association(review),
        "an honest never-observed review anchor may be associated");
    review.Availability = core::anchor_availability::pruned;
    expect(
        !application::valid_new_review_association(review),
        "a new association cannot import a prune tombstone");
    review.Availability = core::anchor_availability::cache_deleted;
    expect(
        !application::valid_new_review_association(review),
        "a new association cannot import a cache-delete tombstone");
    review.Availability = core::anchor_availability::corrupt;
    expect(
        !application::valid_new_review_association(review),
        "corruption is a catalog verification outcome");
}

void prune_availability_contract()
{
    const auto authority =
        opaque<core::pull_request_authority_scope_id>(51U);
    const auto incarnation =
        opaque<core::epoch_catalog_incarnation_id>(52U);
    auto fact = application::prune_epoch_fact{
        .Epoch =
            core::review_epoch_id{
                .Authority = authority,
                .Incarnation = incarnation,
                .Ordinal = core::epoch_ordinal{1},
            },
        .Retained =
            application::git_retained_ref_pair{
                .Scope = authority,
                .Ordinal = core::epoch_ordinal{1},
                .Base = commit(53U),
                .Head = commit(54U),
            },
        .BaseRef =
            application::epoch_catalog_ref_identity{
                .Authority = authority,
                .Ordinal = core::epoch_ordinal{1},
            },
        .HeadRef =
            application::epoch_catalog_ref_identity{
                .Authority = authority,
                .Ordinal = core::epoch_ordinal{1},
                .Head = true,
            },
    };
    expect(
        application::valid(fact),
        "prune intent may retain exact available anchor states");

    fact.ExpectedBaseAvailability =
        core::anchor_availability::unavailable;
    fact.ExpectedHeadAvailability = core::anchor_availability::corrupt;
    expect(
        application::valid(fact),
        "prune intent accepts exact non-destructive unavailable and corrupt "
        "states");

    fact.ExpectedBaseAvailability =
        core::anchor_availability::never_observed;
    expect(
        !application::valid(fact),
        "epoch creation invariants keep never-observed out of prune intent");
    fact.ExpectedBaseAvailability = core::anchor_availability::pruned;
    expect(
        !application::valid(fact),
        "prune intent cannot import an existing prune tombstone");
    fact.ExpectedBaseAvailability =
        core::anchor_availability::cache_deleted;
    expect(
        !application::valid(fact),
        "prune intent cannot cross a cache-delete tombstone");
}

void semantic_cause_contract()
{
    auto cause = core::epoch_cause{
        .Origin = core::epoch_origin::initial_observation,
        .Base = core::epoch_base_transition::not_applicable,
        .Head = core::epoch_head_transition::not_applicable,
        .Material = core::epoch_material_relation::unknown,
        .Primary = core::epoch_primary_cause::initial_observation,
    };
    expect(
        core::valid(cause),
        "the core exposes its complete semantic cause validator");
    cause.Primary = core::epoch_primary_cause::author_upload;
    expect(
        !core::valid(cause),
        "a range-valid but contradictory cause tuple is rejected");
    cause.Primary = core::epoch_primary_cause::initial_observation;
    cause.Material = core::epoch_material_relation::same;
    expect(
        !core::valid(cause),
        "an initial observation cannot claim comparison with a prior epoch");

    cause = core::epoch_cause{
        .Origin = core::epoch_origin::retention_rehydration,
        .Base = core::epoch_base_transition::unchanged,
        .Head = core::epoch_head_transition::unchanged,
        .Material = core::epoch_material_relation::different,
        .Primary = core::epoch_primary_cause::retention_rehydration,
    };
    expect(
        core::valid(cause),
        "retention rehydration is a first-class semantic cause");
    cause.Head = core::epoch_head_transition::descendant;
    expect(
        !core::valid(cause),
        "rehydration cannot disguise a different live head transition");

    const auto snapshot = application::epoch_catalog_snapshot{
        .RetentionRehydration =
            core::retention_rehydration_requirement::required,
    };
    expect(
        snapshot.RetentionRehydration
            == core::retention_rehydration_requirement::required,
        "the bounded catalog snapshot exposes the typed durable requirement");
}

void phase_contract()
{
    using enum application::epoch_catalog_operation_kind;
    using enum application::epoch_catalog_operation_phase;

    expect(
        application::valid_phase_transition(
            append,
            preparing,
            spool_recorded),
        "append advances one durable phase");
    expect(
        !application::valid_phase_transition(
            append,
            preparing,
            imported),
        "append cannot skip the journal-before-import phase");
    expect(
        application::valid_phase_transition(
            append,
            commit_authorized,
            ref_commit_in_progress),
        "append records the ref point of no cancellation");
    expect(
        !application::valid_phase_transition(
            append,
            ref_commit_in_progress,
            abort_requested),
        "append cannot report cancellation after the ref point of no return");
    expect(
        application::point_of_no_cancellation(
            ref_commit_in_progress),
        "ref commit marker is the append cancellation boundary");
    expect(
        application::valid_repair_origin(
            append,
            repair_required,
            ref_commit_in_progress),
        "repair evidence retains its post-commit-boundary origin");
    expect(
        !application::valid_repair_origin(
            append,
            repair_required,
            std::nullopt),
        "repair state without its prior durable phase is invalid");
    expect(
        !application::valid_repair_origin(
            append,
            preparing,
            preparing),
        "ordinary operation phases cannot carry repair provenance");
    expect(
        !application::valid_repair_origin(
            prune,
            repair_required,
            ref_commit_in_progress),
        "repair provenance must be valid for the operation kind");

    expect(
        application::valid_phase_transition(
            prune,
            prune_intent,
            ref_delete_in_progress),
        "prune records intent before ref deletion");
    expect(
        application::valid_phase_transition(
            cache_delete,
            cache_delete_building_exact_set,
            cache_delete_intent_sealed),
        "cache delete seals its exact affected set first");
    expect(
        !application::valid_phase_transition(
            cache_delete,
            cache_delete_building_exact_set,
            quarantine_rename_in_progress),
        "cache delete cannot rename before sealing the affected set");
    expect(
        !application::valid_phase_transition(
            append,
            cache_delete_intent_sealed,
            repair_required),
        "repair cannot relabel a phase from another operation kind");

    auto transition = application::operation_transition_request{
        .Catalog =
            core::epoch_catalog_identity{
                .Incarnation =
                    opaque<core::epoch_catalog_incarnation_id>(21U),
                .Revision = core::epoch_catalog_revision{2},
            },
        .Operation = opaque<core::epoch_operation_id>(22U),
        .Kind = append,
        .Expected = ref_commit_in_progress,
        .Next = refs_committed,
        .Evidence =
            application::epoch_catalog_transition_evidence::
                catalog_transaction,
        .ExpectedOperationRevision =
            application::epoch_catalog_operation_revision{3},
    };
    expect(
        !application::valid(transition),
        "bare catalog claim cannot authorize final-ref completion");
    transition.Evidence =
        application::epoch_catalog_transition_evidence::exact_final_pair;
    expect(
        application::valid(transition),
        "typed exact-final-ref evidence authorizes the catalog edge");

    transition.Expected = preparing;
    transition.Next = spool_recorded;
    transition.Evidence =
        application::epoch_catalog_transition_evidence::
            catalog_transaction;
    expect(
        !application::valid(transition),
        "generic transition cannot bypass the atomic spool write");

    transition.Expected =
        static_cast<application::epoch_catalog_operation_phase>(255U);
    transition.Next =
        application::epoch_catalog_operation_phase::repair_required;
    transition.Evidence =
        application::epoch_catalog_transition_evidence::repair_ambiguity;
    expect(
        !application::valid(transition),
        "invalid stored phase cannot be relabeled as repair");

    expect(
        application::valid(
            application::epoch_catalog_operation_revision{
                application::
                    maximum_epoch_catalog_operation_revision}),
        "maximum signed operation revision is representable");
    expect(
        !application::can_advance(
            application::epoch_catalog_operation_revision{
                application::
                    maximum_epoch_catalog_operation_revision}),
        "maximum operation revision cannot overflow on mutation");
    const auto final_nonterminal_revision =
        application::epoch_catalog_operation_revision{
            application::maximum_epoch_catalog_operation_revision - 1};
    expect(
        application::can_advance(final_nonterminal_revision)
            && !application::can_advance_nonterminal(
                final_nonterminal_revision)
            && application::can_advance_nonterminal(
                application::epoch_catalog_operation_revision{
                    application::
                        maximum_epoch_catalog_operation_revision
                    - 2}),
        "nonterminal work reserves the final operation revision for a "
        "terminal transition");
    expect(
        !application::valid(
            application::epoch_catalog_operation_revision{}),
        "zero operation revision is rejected");
}

void durable_fact_contract()
{
    const auto operation = opaque<core::epoch_operation_id>(12U);
    auto completed = application::completed_git_pack_receipt{
        .Operation = operation,
        .ExpectedPack = pack(13U),
        .ObservedPack = pack(13U),
        .Spool = artifact(
            application::git_artifact_role::completed_spool,
            14U,
            1024U),
    };
    completed.OpaqueToken.front() = 15U;

    const auto spool = application::durable_fact(completed);
    expect(
        application::valid(spool),
        "durable spool copies exact artifact and pack facts");
    expect(
        spool.Operation == operation,
        "durable spool remains operation-bound");
    auto observed = application::git_preimport_observation{
        .Operation = operation,
        .ExpectedPack = pack(13U),
        .Packs = {
            application::git_pack_inventory_entry{
                .Pack = pack(13U),
                .PackBytes = 1024U,
                .IndexBytes = 256U,
            },
        },
        .RepositoryBytes = 4096U,
        .ExpectedPackPreexisting = true,
    };
    observed.OpaqueToken.front() = 16U;
    const auto preimport = application::durable_fact(observed);
    expect(
        application::valid(preimport),
        "durable preimport inventory is canonical and bounded");

    auto wrong_preexisting = preimport;
    wrong_preexisting.ExpectedPackPreexisting = false;
    expect(
        !application::valid(wrong_preexisting),
        "preexisting fact must agree with the exact inventory");

    auto duplicate = preimport;
    duplicate.Packs.push_back(duplicate.Packs.front());
    expect(
        !application::valid(duplicate),
        "inventory must be strictly ordered and unique");
    auto underreported = preimport;
    underreported.RepositoryBytes =
        underreported.Packs.front().PackBytes
        + underreported.Packs.front().IndexBytes - 1U;
    expect(
        !application::valid(underreported),
        "durable inventory bytes cannot exceed the observed repository");

    auto request = application::record_spool_request{
        .Catalog =
            core::epoch_catalog_identity{
                .Incarnation =
                    opaque<core::epoch_catalog_incarnation_id>(17U),
                .Revision = core::epoch_catalog_revision{1},
            },
        .Operation = operation,
        .ExpectedOperationRevision =
            application::epoch_catalog_operation_revision{1},
        .Spool = spool,
        .Preimport = preimport,
    };
    expect(
        application::valid(request),
        "spool transaction requires one coherent operation");
    request.Preimport.Packs.front().PackBytes += 1U;
    expect(
        !application::valid(request),
        "a preexisting pack must have the exact completed spool size");
    request.Preimport = preimport;
    request.Preimport.Operation =
        opaque<core::epoch_operation_id>(18U);
    expect(
        !application::valid(request),
        "mixed-operation durable facts are rejected");
}

void paging_contract()
{
    const auto binding = application::epoch_catalog_binding{
        .Authority =
            core::pull_request_authority{
                .Profile = core::host_profile_id{1U},
                .ProfileRevision = core::github_profile_revision{2U},
                .Host = core::git_host_id{3U},
                .HostAuthority =
                    opaque<core::host_authority_fingerprint>(4U),
                .Account = core::github_account_id{5U},
                .Viewer = core::github_viewer_actor_id{6U},
                .Repository = core::repository_id{7U},
                .Cache = opaque<core::repository_cache_scope_id>(8U),
                .PullRequest = core::pull_request_id{9U},
                .Number = core::pull_request_number{10U},
                .Scope =
                    opaque<core::pull_request_authority_scope_id>(11U),
            },
        .Catalog =
            core::epoch_catalog_identity{
                .Incarnation =
                    opaque<core::epoch_catalog_incarnation_id>(12U),
                .Revision = core::epoch_catalog_revision{4},
            },
        .ObjectFormat = core::git_hash_algorithm::sha1,
    };
    expect(application::valid(binding), "catalog binding is exact");

    auto page = application::epoch_history_page_request{
        .Catalog = binding.Catalog,
        .Limit = 100U,
    };
    expect(
        application::valid(binding, page),
        "maximum bounded history page is accepted");
    page.Limit = 0U;
    expect(
        !application::valid(binding, page),
        "zero-sized history page is rejected");
    page.Limit = 101U;
    expect(
        !application::valid(binding, page),
        "oversized history page is rejected");

    page.Limit = 10U;
    page.Cursor = application::epoch_history_cursor{
        .Authority = binding.Authority.Scope,
        .Incarnation = binding.Catalog.Incarnation,
        .Revision = core::epoch_catalog_revision{3},
        .Before = core::epoch_ordinal{5},
    };
    expect(
        !application::valid(binding, page),
        "cursor from another catalog revision is rejected");

    auto impact = application::cache_delete_impact{
        .Incarnation = binding.Catalog.Incarnation,
        .Cache = binding.Authority.Cache,
        .Scopes = {
            application::cache_delete_scope_fact{
                .Authority = binding.Authority.Scope,
                .ExpectedRevision = binding.Catalog.Revision,
            },
        },
    };
    expect(
        application::valid(impact),
        "bounded cache-delete impact is exact and revision-bearing");
    impact.RetainedEpochCount =
        application::maximum_retained_epoch_count_per_pull_request + 1U;
    expect(
        !application::valid(impact),
        "cache-delete impact cannot exceed the per-scope retained ceiling");
    impact.RetainedEpochCount = 0U;
    impact.Scopes.push_back(impact.Scopes.front());
    expect(
        !application::valid(impact),
        "cache-delete impact rejects duplicate authority scopes");
}
} // namespace

int main()
{
    authority_contract();
    source_observation_identity_contract();
    quota_contract();
    destructive_availability_contract();
    creation_availability_contract();
    prune_availability_contract();
    semantic_cause_contract();
    phase_contract();
    durable_fact_contract();
    paging_contract();
    std::cout << "epoch catalog application contract: PASS\n";
    return EXIT_SUCCESS;
}
