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
    identity[2] = static_cast<std::uint8_t>(role);
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
    impact.Scopes.push_back(impact.Scopes.front());
    expect(
        !application::valid(impact),
        "cache-delete impact rejects duplicate authority scopes");
}
} // namespace

int main()
{
    authority_contract();
    phase_contract();
    durable_fact_contract();
    paging_contract();
    std::cout << "epoch catalog application contract: PASS\n";
    return EXIT_SUCCESS;
}
