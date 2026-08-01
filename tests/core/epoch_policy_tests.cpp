#include "bpr/core/epoch_transition.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace
{
using namespace bpr::core;

auto expect(const bool condition, const std::string_view message) -> void
{
    if(!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

template<typename Identity>
[[nodiscard]] constexpr auto opaque(const std::uint8_t seed) -> Identity
{
    std::array<std::uint8_t, 32> bytes{};
    bytes[0] = seed;
    bytes[31] = static_cast<std::uint8_t>(seed ^ 0xA5U);
    return Identity{bytes};
}

template<typename Oid>
[[nodiscard]] constexpr auto oid(
    const std::uint8_t seed,
    const git_hash_algorithm algorithm = git_hash_algorithm::sha1) -> Oid
{
    std::array<std::uint8_t, 32> bytes{};
    const std::size_t count = algorithm == git_hash_algorithm::sha1 ? 20U : 32U;
    for(std::size_t index = 0; index < count; ++index)
    {
        bytes[index] =
            static_cast<std::uint8_t>(seed + static_cast<std::uint8_t>(index));
    }
    return Oid{algorithm, bytes};
}

[[nodiscard]] auto authority(const std::uint8_t seed = 1U)
    -> pull_request_authority
{
    return pull_request_authority{
        .Profile = host_profile_id{static_cast<std::uint64_t>(seed) + 10U},
        .ProfileRevision =
            github_profile_revision{static_cast<std::uint64_t>(seed) + 20U},
        .Host = git_host_id{static_cast<std::uint64_t>(seed) + 30U},
        .HostAuthority = opaque<host_authority_fingerprint>(
            static_cast<std::uint8_t>(seed + 1U)),
        .Account = github_account_id{static_cast<std::uint64_t>(seed) + 40U},
        .Viewer =
            github_viewer_actor_id{static_cast<std::uint64_t>(seed) + 50U},
        .Repository = repository_id{static_cast<std::uint64_t>(seed) + 60U},
        .Cache = opaque<repository_cache_scope_id>(
            static_cast<std::uint8_t>(seed + 2U)),
        .PullRequest = pull_request_id{static_cast<std::uint64_t>(seed) + 70U},
        .Number = pull_request_number{static_cast<std::uint32_t>(seed) + 80U},
        .Scope = opaque<pull_request_authority_scope_id>(
            static_cast<std::uint8_t>(seed + 3U)),
    };
}

[[nodiscard]] auto provider_review(
    const std::uint8_t seed,
    const git_commit_oid commit,
    const github_viewer_actor_id actor = github_viewer_actor_id{51U})
    -> current_viewer_review_candidate
{
    return current_viewer_review_candidate{
        .Review = github_review_id{static_cast<std::uint64_t>(seed) + 20U},
        .Actor = actor,
        .Order = github_review_order{static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(seed) + 1U)},
        .State = github_review_state::commented,
        .Commit = commit,
    };
}

[[nodiscard]] auto associated_review(
    const std::uint8_t seed,
    const git_commit_oid commit,
    const anchor_availability availability = anchor_availability::available,
    const github_viewer_actor_id actor = github_viewer_actor_id{51U})
    -> catalog_review_association
{
    const auto provider = provider_review(seed, commit, actor);
    return catalog_review_association{
        .Association = opaque<review_association_id>(
            static_cast<std::uint8_t>(seed + 10U)),
        .Review = provider.Review,
        .Actor = provider.Actor,
        .Order = provider.Order,
        .State = provider.State,
        .Commit = provider.Commit,
        .Availability = availability,
    };
}

[[nodiscard]] auto revision(
    const std::uint64_t snapshot = 1U,
    const std::uint8_t base = 10U,
    const std::uint8_t head = 20U,
    const std::uint8_t branch = 30U,
    const pull_request_revision_origin origin =
        pull_request_revision_origin::live,
    std::optional<current_viewer_review_candidate> current_review =
        std::nullopt) -> accepted_pull_request_revision
{
    return accepted_pull_request_revision{
        .Source =
            pull_request_source_revision{
                .Observation = opaque<epoch_source_observation_id>(
                    static_cast<std::uint8_t>(snapshot)),
                .Snapshot = provider_snapshot_id{snapshot},
                .IdentityGeneration = github_identity_generation{3U},
                .PullRequestGeneration = request_generation{4U},
            },
        .Origin = origin,
        .BaseBranch = opaque<base_branch_id>(branch),
        .Base = oid<git_commit_oid>(base),
        .Head = oid<git_commit_oid>(head),
        .CurrentViewerReview = std::move(current_review),
    };
}

[[nodiscard]] auto catalog(const std::int64_t value = 0)
    -> epoch_catalog_identity
{
    return epoch_catalog_identity{
        .Incarnation = opaque<epoch_catalog_incarnation_id>(41U),
        .Revision = epoch_catalog_revision{value},
    };
}

[[nodiscard]] auto material(
    const std::uint8_t base = 60U,
    const std::uint8_t head = 70U,
    const std::uint64_t policy = 1U) -> epoch_material_identity
{
    return epoch_material_identity{
        .BaseTree = oid<git_tree_oid>(base),
        .HeadTree = oid<git_tree_oid>(head),
        .PolicyRevision = comparison_policy_revision{policy},
    };
}

[[nodiscard]] auto summary(
    const pull_request_authority& owner,
    const epoch_catalog_identity& owner_catalog,
    const std::int64_t ordinal,
    const std::uint8_t base,
    const std::uint8_t head,
    const std::uint8_t branch = 30U,
    const epoch_origin origin = epoch_origin::live_observation,
    const anchor_availability base_availability =
        anchor_availability::available,
    const anchor_availability head_availability =
        anchor_availability::available,
    std::optional<epoch_material_identity> compared_material = material())
    -> review_epoch_summary
{
    return review_epoch_summary{
        .Id =
            review_epoch_id{
                .Authority = owner.Scope,
                .Incarnation = owner_catalog.Incarnation,
                .Ordinal = epoch_ordinal{ordinal},
            },
        .Source =
            pull_request_source_revision{
                .Observation = opaque<epoch_source_observation_id>(
                    static_cast<std::uint8_t>(ordinal + 90)),
                .Snapshot =
                    provider_snapshot_id{
                        static_cast<std::uint64_t>(ordinal) + 100U},
                .IdentityGeneration = github_identity_generation{3U},
                .PullRequestGeneration = request_generation{4U},
            },
        .BaseBranch = opaque<base_branch_id>(branch),
        .Base = oid<git_commit_oid>(base),
        .Head = oid<git_commit_oid>(head),
        .Material = std::move(compared_material),
        .Cause =
            epoch_cause{
                .Origin = origin,
                .Base = origin == epoch_origin::explicit_historical_import
                    ? epoch_base_transition::not_applicable
                    : epoch_base_transition::unchanged,
                .Head = origin == epoch_origin::explicit_historical_import
                    ? epoch_head_transition::not_applicable
                    : epoch_head_transition::descendant,
                .Material = epoch_material_relation::different,
                .Primary = origin == epoch_origin::explicit_historical_import
                    ? epoch_primary_cause::explicit_historical_import
                    : epoch_primary_cause::author_upload,
            },
        .ObservedAt = observed_utc_milliseconds{ordinal * 100},
        .BaseAvailability = base_availability,
        .HeadAvailability = head_availability,
    };
}

[[nodiscard]] auto anchor(
    const review_epoch_summary& epoch, const comparison_anchor_kind kind)
    -> comparison_anchor
{
    return comparison_anchor{
        .Identity =
            comparison_anchor_identity{
                .Authority = epoch.Id.Authority,
                .Incarnation = epoch.Id.Incarnation,
                .Kind = kind,
                .Epoch = epoch.Id.Ordinal,
            },
        .Commit = kind == comparison_anchor_kind::epoch_base ? epoch.Base
                                                             : epoch.Head,
        .Availability = kind == comparison_anchor_kind::epoch_base
            ? epoch.BaseAvailability
            : epoch.HeadAvailability,
    };
}

[[nodiscard]] auto review_anchor(
    const pull_request_authority& owner,
    const epoch_catalog_identity& owner_catalog,
    const catalog_review_association& current) -> comparison_anchor
{
    return comparison_anchor{
        .Identity =
            comparison_anchor_identity{
                .Authority = owner.Scope,
                .Incarnation = owner_catalog.Incarnation,
                .Kind = comparison_anchor_kind::review_commit,
                .Review = current.Association,
            },
        .Commit = current.Commit,
        .Availability = current.Availability,
    };
}

template<typename Request>
[[nodiscard]] auto request_from(const epoch_transition_output& output)
    -> const Request*
{
    for(const auto& request : output.Requests)
    {
        if(const auto* typed = std::get_if<Request>(&request))
        {
            return typed;
        }
    }
    return nullptr;
}

[[nodiscard]] auto initial_state() -> epoch_state
{
    return epoch_state{
        .ApplicationSession = application_session_id{91U},
    };
}

struct ready_fixture
{
    epoch_state State;
    pull_request_authority Authority;
    accepted_pull_request_revision Revision;
    epoch_catalog_identity Catalog;
    review_epoch_summary Epoch;

    auto operator<=>(const ready_fixture&) const = default;
};

[[nodiscard]] auto make_ready_fixture(epoch_state state = initial_state())
    -> ready_fixture
{
    const auto owner = authority();
    const auto accepted = revision();
    const auto initial_catalog = catalog();

    const auto accepted_output = transition(
        state,
        pull_request_revision_accepted_event{
            .ApplicationSession = state.ApplicationSession,
            .Authority = owner,
            .Revision = accepted,
        });
    const auto* load =
        request_from<load_epoch_catalog_request>(accepted_output);
    expect(load != nullptr, "new authority requests a catalog load");

    const auto loaded_output = transition(
        state,
        epoch_catalog_loaded_event{
            .Context = load->Context,
            .Catalog = initial_catalog,
        });
    const auto* prepare =
        request_from<prepare_epoch_revision_request>(loaded_output);
    expect(prepare != nullptr, "catalog load requests preparation");

    const auto prepared_output = transition(
        state,
        epoch_revision_prepared_event{
            .Context = prepare->Context,
            .Operation = opaque<epoch_operation_id>(51U),
            .Observation =
                epoch_preparation_observation{
                    .ProposedOrdinal = epoch_ordinal{1},
                    .ObservedAt = observed_utc_milliseconds{100},
                    .Material = material(),
                },
        });
    const auto* commit =
        request_from<commit_epoch_catalog_request>(prepared_output);
    expect(commit != nullptr, "preparation requests an exact catalog commit");
    const auto* append = std::get_if<append_epoch_mutation>(&commit->Mutation);
    expect(append != nullptr, "initial commit carries an append mutation");

    const auto committed_output = transition(
        state,
        epoch_catalog_committed_event{
            .Context = commit->Context,
            .Mutation = commit->Mutation,
            .NewRevision = epoch_catalog_revision{1},
        });
    const auto* resolve =
        request_from<resolve_comparison_request>(committed_output);
    expect(resolve != nullptr, "commit requests semantic comparison");
    const auto committed_epoch = append->Epoch;
    const auto updated_catalog = catalog(1);

    const auto resolved_output = transition(
        state,
        comparison_resolved_event{
            .Context = resolve->Context,
            .Candidates =
                comparison_candidates{
                    .Catalog = updated_catalog,
                    .LatestLive = committed_epoch,
                },
        });
    expect(
        resolved_output.Comparison.has_value()
            && resolved_output.Comparison->AvailableForDiff,
        "fixture resolves an available Whole PR pair");
    return ready_fixture{
        .State = state,
        .Authority = owner,
        .Revision = accepted,
        .Catalog = updated_catalog,
        .Epoch = committed_epoch,
    };
}

[[nodiscard]] auto diagnostic_is(
    const epoch_transition_output& output, const epoch_reason_code reason)
    -> bool
{
    return output.Diagnostics == std::vector{epoch_diagnostic{.Code = reason}};
}

[[nodiscard]] auto history_cursor(
    const ready_fixture& fixture, const std::int64_t before)
    -> epoch_history_cursor
{
    return {
        .Authority = fixture.Authority.Scope,
        .Incarnation = fixture.Catalog.Incarnation,
        .Revision = fixture.Catalog.Revision,
        .Before = epoch_ordinal{before},
    };
}

[[nodiscard]] auto history_page(
    const ready_fixture& fixture,
    std::vector<review_epoch_summary> epochs,
    std::optional<epoch_history_cursor> continuation = std::nullopt)
    -> epoch_history_page_observation
{
    return {
        .Authority = fixture.Authority.Scope,
        .Catalog = fixture.Catalog,
        .Epochs = std::move(epochs),
        .Continuation = continuation,
    };
}

[[nodiscard]] auto append_from(const planned_epoch_mutation& plan)
    -> const append_epoch_mutation*
{
    if(!plan.Mutation.has_value())
    {
        return nullptr;
    }
    return std::get_if<append_epoch_mutation>(&*plan.Mutation);
}

auto test_preparation_effect_policy() -> void
{
    const auto owner = authority();
    const auto owner_catalog = catalog(4);
    const auto prior = summary(owner, owner_catalog, 4, 10U, 20U);

    expect(
        plan_epoch_preparation(
            owner,
            revision(),
            owner_catalog,
            {})
                == planned_epoch_preparation{
                    .Effect = epoch_preparation_effect::retain_revision,
                },
        "initial observation requires retained-revision preparation");

    auto unchanged = revision(2U, 10U, 20U);
    expect(
        plan_epoch_preparation(
            owner,
            unchanged,
            owner_catalog,
            epoch_preparation_catalog_observation{.LatestLive = prior})
                == planned_epoch_preparation{},
        "unchanged observation requires no Git preparation");

    unchanged.CurrentViewerReview = provider_review(7U, unchanged.Head);
    expect(
        plan_epoch_preparation(
            owner,
            unchanged,
            owner_catalog,
            epoch_preparation_catalog_observation{.LatestLive = prior})
                == planned_epoch_preparation{},
        "association-only observation requires no Git preparation");

    auto changed = revision(2U, 10U, 21U);
    expect(
        plan_epoch_preparation(
            owner,
            changed,
            owner_catalog,
            epoch_preparation_catalog_observation{.LatestLive = prior})
                == planned_epoch_preparation{
                    .Effect = epoch_preparation_effect::retain_revision,
                },
        "changed head requires retained-revision preparation");

    changed.Source.Observation = prior.Source.Observation;
    expect(
        plan_epoch_preparation(
            owner,
            changed,
            owner_catalog,
            epoch_preparation_catalog_observation{.LatestLive = prior})
                .Rejection == epoch_reason_code::invalid_observation,
        "changed revision rejects reuse of the latest source observation");

    auto deleted = prior;
    deleted.BaseAvailability = anchor_availability::cache_deleted;
    deleted.HeadAvailability = anchor_availability::cache_deleted;
    auto rehydrated = revision(3U, 10U, 20U);
    expect(
        plan_epoch_preparation(
            owner,
            rehydrated,
            owner_catalog,
            epoch_preparation_catalog_observation{
                .LatestLive = deleted,
                .RetentionRehydration =
                    retention_rehydration_requirement::required,
            })
                == planned_epoch_preparation{
                    .Effect = epoch_preparation_effect::retain_revision,
                },
        "durable cache-delete requirement requests exact rehydration work");
}

auto test_binary_oid_types_and_compact_state() -> void
{
    static_assert(!std::is_same_v<git_commit_oid, git_tree_oid>);
    static_assert(!std::is_same_v<epoch_generation, comparison_generation>);
    static_assert(!std::is_same_v<
                  pull_request_authority_scope_id,
                  repository_cache_scope_id>);
    static_assert(std::is_trivially_copyable_v<epoch_state>);
    static_assert(std::is_trivially_destructible_v<epoch_state>);
    static_assert(sizeof(epoch_state) <= 2'048U);
    static_assert(std::variant_size_v<epoch_event> == 29U);
    static_assert(std::variant_size_v<epoch_host_request> == 9U);

    const auto sha1 =
        parse_git_commit_oid("0123456789ABCDEF0123456789ABCDEF01234567");
    expect(sha1.has_value(), "40 hexadecimal digits parse as SHA-1");
    expect(
        sha1->algorithm() == git_hash_algorithm::sha1
            && sha1->byte_count() == 20U,
        "SHA-1 remains algorithm tagged and binary");
    expect(
        canonical_hex(*sha1).view()
            == "0123456789abcdef0123456789abcdef01234567",
        "SHA-1 renders once as complete canonical lowercase text");

    const auto sha256 = parse_git_tree_oid(
        "0123456789abcdef0123456789abcdef"
        "0123456789abcdef0123456789abcdef");
    expect(sha256.has_value(), "64 hexadecimal digits parse as SHA-256");
    expect(
        sha256->algorithm() == git_hash_algorithm::sha256
            && canonical_hex(*sha256).view().size() == 64U,
        "SHA-256 remains algorithm tagged and complete");
    expect(
        !parse_git_commit_oid(std::string(40U, '0')).has_value(),
        "all-zero OIDs are invalid");
    expect(
        !parse_git_commit_oid("abcd").has_value()
            && !parse_git_commit_oid(std::string(40U, 'z')).has_value(),
        "wrong length and non-hexadecimal OIDs fail closed");

    auto malformed_bytes = oid<git_commit_oid>(7U).bytes();
    malformed_bytes[31] = 1U;
    expect(
        !git_commit_oid{git_hash_algorithm::sha1, malformed_bytes}.valid(),
        "SHA-1 rejects nonzero bytes outside its exact 20-byte digest");
    expect(valid(authority()), "complete authority fixture is valid");
    auto incomplete = authority();
    incomplete.Cache = {};
    expect(!valid(incomplete), "missing cache scope invalidates authority");
}

auto test_cause_precedence_and_material_policy() -> void
{
    const auto owner = authority();
    const auto owner_catalog = catalog();
    const auto prior = summary(owner, owner_catalog, 4, 10U, 20U, 30U);

    struct cause_case
    {
        const char* Name;
        accepted_pull_request_revision Revision;
        head_lineage Lineage;
        epoch_base_transition Base;
        epoch_head_transition Head;
        epoch_primary_cause Primary;
    };

    const std::array cases{
        cause_case{
            .Name = "base update",
            .Revision = revision(2U, 11U, 20U, 30U),
            .Lineage = head_lineage::not_applicable,
            .Base = epoch_base_transition::base_oid_changed,
            .Head = epoch_head_transition::unchanged,
            .Primary = epoch_primary_cause::base_update,
        },
        cause_case{
            .Name = "author upload",
            .Revision = revision(2U, 10U, 21U, 30U),
            .Lineage = head_lineage::descendant,
            .Base = epoch_base_transition::unchanged,
            .Head = epoch_head_transition::descendant,
            .Primary = epoch_primary_cause::author_upload,
        },
        cause_case{
            .Name = "force push",
            .Revision = revision(2U, 10U, 22U, 30U),
            .Lineage = head_lineage::non_fast_forward,
            .Base = epoch_base_transition::unchanged,
            .Head = epoch_head_transition::non_fast_forward,
            .Primary = epoch_primary_cause::force_push_rebase,
        },
        cause_case{
            .Name = "unknown lineage",
            .Revision = revision(2U, 10U, 23U, 30U),
            .Lineage = head_lineage::unknown,
            .Base = epoch_base_transition::unchanged,
            .Head = epoch_head_transition::lineage_unknown,
            .Primary = epoch_primary_cause::head_change_lineage_unavailable,
        },
        cause_case{
            .Name = "simultaneous base and head",
            .Revision = revision(2U, 12U, 24U, 30U),
            .Lineage = head_lineage::descendant,
            .Base = epoch_base_transition::base_oid_changed,
            .Head = epoch_head_transition::descendant,
            .Primary = epoch_primary_cause::author_upload,
        },
        cause_case{
            .Name = "branch outranks head",
            .Revision = revision(2U, 12U, 25U, 31U),
            .Lineage = head_lineage::non_fast_forward,
            .Base = epoch_base_transition::base_branch_changed,
            .Head = epoch_head_transition::non_fast_forward,
            .Primary = epoch_primary_cause::base_branch_change,
        },
    };

    for(const auto& item : cases)
    {
        const auto plan = plan_epoch_mutation(
            owner,
            item.Revision,
            owner_catalog,
            epoch_preparation_observation{
                .LatestLive = prior,
                .ProposedOrdinal = epoch_ordinal{5},
                .ObservedAt = observed_utc_milliseconds{500},
                .HeadLineage = item.Lineage,
                .Material = material(61U, 71U),
            });
        const auto* append = append_from(plan);
        expect(
            plan.Rejection == epoch_reason_code::none && append != nullptr,
            item.Name);
        const auto& cause = append->Epoch.Cause;
        expect(
            cause.Origin == epoch_origin::live_observation
                && cause.Base == item.Base && cause.Head == item.Head
                && cause.Primary == item.Primary,
            item.Name);
    }

    const auto initial = plan_epoch_mutation(
        owner,
        revision(),
        owner_catalog,
        epoch_preparation_observation{
            .ProposedOrdinal = epoch_ordinal{1},
            .ObservedAt = observed_utc_milliseconds{0},
            .Material = material(),
        });
    const auto* initial_append = append_from(initial);
    expect(initial_append != nullptr &&
             initial_append->Epoch.Cause ==
                 epoch_cause{
                     .Origin = epoch_origin::initial_observation,
                     .Base = epoch_base_transition::not_applicable,
                     .Head = epoch_head_transition::not_applicable,
                     .Material = epoch_material_relation::unknown,
                     .Primary = epoch_primary_cause::initial_observation,
                 },
         "first live observation has exact initial cause axes");

    const auto historical = plan_epoch_mutation(
        owner,
        revision(
            2U,
            7U,
            8U,
            9U,
            pull_request_revision_origin::explicit_historical_import),
        owner_catalog,
        epoch_preparation_observation{
            .LatestLive = prior,
            .ProposedOrdinal = epoch_ordinal{5},
            .ObservedAt = observed_utc_milliseconds{500},
            .Material = material(),
        });
    const auto* historical_append = append_from(historical);
    expect(
        historical_append != nullptr
            && historical_append->Epoch.Cause.Origin
                == epoch_origin::explicit_historical_import
            && historical_append->Epoch.Cause.Base
                == epoch_base_transition::not_applicable
            && historical_append->Epoch.Cause.Head
                == epoch_head_transition::not_applicable,
        "explicit historical import never invents live transitions");

    const auto materially_same = plan_epoch_mutation(
        owner,
        revision(2U, 10U, 21U, 30U),
        owner_catalog,
        epoch_preparation_observation{
            .LatestLive = prior,
            .ProposedOrdinal = epoch_ordinal{5},
            .ObservedAt = observed_utc_milliseconds{500},
            .HeadLineage = head_lineage::descendant,
            .Material = prior.Material,
        });
    const auto* materially_same_append = append_from(materially_same);
    expect(
        materially_same_append != nullptr
            && materially_same_append->Epoch.Cause.Material
                == epoch_material_relation::same,
        "material equality annotates but never suppresses a changed epoch");

    const auto bad_lineage = plan_epoch_mutation(
        owner,
        revision(2U, 10U, 21U, 30U),
        owner_catalog,
        epoch_preparation_observation{
            .LatestLive = prior,
            .ProposedOrdinal = epoch_ordinal{5},
            .ObservedAt = observed_utc_milliseconds{500},
            .HeadLineage = head_lineage::not_applicable,
        });
    expect(
        bad_lineage.Rejection == epoch_reason_code::invalid_observation,
        "changed head requires an explicit bounded lineage fact");

    const auto max_catalog = catalog(maximum_epoch_catalog_revision);
    const auto max_prior = summary(owner, max_catalog, 4, 10U, 20U, 30U);
    const auto exhausted = plan_epoch_mutation(
        owner,
        revision(2U, 10U, 21U, 30U),
        max_catalog,
        epoch_preparation_observation{
            .LatestLive = max_prior,
            .ProposedOrdinal = epoch_ordinal{5},
            .ObservedAt = observed_utc_milliseconds{500},
            .HeadLineage = head_lineage::descendant,
        });
    expect(
        exhausted.Rejection == epoch_reason_code::catalog_revision_exhausted,
        "catalog revision exhaustion fails before a mutation");

    const auto idempotent_at_max = plan_epoch_mutation(
        owner,
        revision(2U, 10U, 20U, 30U),
        max_catalog,
        epoch_preparation_observation{
            .LatestLive = max_prior,
            .ObservedAt = observed_utc_milliseconds{600},
        });
    expect(
        !idempotent_at_max.Mutation.has_value()
            && idempotent_at_max.Rejection == epoch_reason_code::none,
        "catalog revision exhaustion still permits a true idempotent no-op");
}

auto test_retention_rehydration_policy() -> void
{
    const auto owner = authority();
    const auto owner_catalog = catalog(8);
    auto prior = summary(
        owner,
        owner_catalog,
        4,
        10U,
        20U,
        30U,
        epoch_origin::live_observation,
        anchor_availability::cache_deleted,
        anchor_availability::cache_deleted,
        material());
    auto unchanged = revision(prior.Source.Snapshot.value());
    unchanged.Source = prior.Source;
    unchanged.Source.Observation =
        opaque<epoch_source_observation_id>(91U);

    const auto rehydration = plan_epoch_mutation(
        owner,
        unchanged,
        owner_catalog,
        epoch_preparation_observation{
            .LatestLive = prior,
            .ProposedOrdinal = epoch_ordinal{5},
            .ObservedAt = observed_utc_milliseconds{500},
            .Material = material(60U, 70U, 2U),
            .RetentionRehydration =
                retention_rehydration_requirement::required,
        });
    const auto* append = append_from(rehydration);
    expect(
        rehydration.Rejection == epoch_reason_code::none && append
            && append->Epoch.Source.Observation
                != prior.Source.Observation
            && append->Epoch.Source.Snapshot == prior.Source.Snapshot
            && append->Epoch.Source.IdentityGeneration
                == prior.Source.IdentityGeneration
            && append->Epoch.Source.PullRequestGeneration
                == prior.Source.PullRequestGeneration
            && append->Epoch.Id.Ordinal == epoch_ordinal{5}
            && append->Epoch.Cause
                == epoch_cause{
                    .Origin = epoch_origin::retention_rehydration,
                    .Base = epoch_base_transition::unchanged,
                    .Head = epoch_head_transition::unchanged,
                    .Material = epoch_material_relation::different,
                    .Primary =
                        epoch_primary_cause::retention_rehydration,
                },
        "durable retention loss deliberately creates one fresh-observation "
        "replacement epoch with an explicit cause even after counters reset");
    expect(
        prior.BaseAvailability == anchor_availability::cache_deleted
            && prior.HeadAvailability == anchor_availability::cache_deleted,
        "planning a replacement leaves the prior cache-delete tombstone "
        "unchanged");

    auto not_required = epoch_preparation_observation{
        .LatestLive = prior,
        .ObservedAt = observed_utc_milliseconds{500},
        .Material = material(),
    };
    const auto ordinary_noop = plan_epoch_mutation(
        owner, unchanged, owner_catalog, not_required);
    expect(
        ordinary_noop.Rejection == epoch_reason_code::none
            && !ordinary_noop.Mutation,
        "an unchanged source remains a no-op without the durable requirement");

    auto malformed = epoch_preparation_observation{
        .LatestLive = prior,
        .ProposedOrdinal = epoch_ordinal{5},
        .ObservedAt = observed_utc_milliseconds{500},
        .Material = material(),
        .RetentionRehydration =
            retention_rehydration_requirement::required,
    };
    malformed.LatestLive->HeadAvailability = anchor_availability::available;
    expect(
        plan_epoch_mutation(owner, unchanged, owner_catalog, malformed)
                .Rejection
            == epoch_reason_code::invalid_observation,
        "required rehydration must name an exact cache-deleted comparison "
        "point");

    malformed.LatestLive = prior;
    auto reused_source = unchanged;
    reused_source.Source.Observation = prior.Source.Observation;
    reused_source.Source.Snapshot = provider_snapshot_id{999U};
    expect(
        plan_epoch_mutation(
            owner, reused_source, owner_catalog, malformed)
                .Rejection
            == epoch_reason_code::invalid_observation,
        "retention rehydration rejects reuse of the prior durable source-"
        "observation identity regardless of process-local counters");

    auto newer = unchanged;
    newer.Base = oid<git_commit_oid>(11U);
    const auto changed = plan_epoch_mutation(
        owner,
        newer,
        owner_catalog,
        epoch_preparation_observation{
            .LatestLive = prior,
            .ProposedOrdinal = epoch_ordinal{5},
            .ObservedAt = observed_utc_milliseconds{500},
            .Material = material(61U, 70U),
            .RetentionRehydration =
                retention_rehydration_requirement::required,
        });
    const auto* changed_append = append_from(changed);
    expect(
        changed.Rejection == epoch_reason_code::none && changed_append
            && changed_append->Epoch.Cause.Origin
                == epoch_origin::live_observation
            && changed_append->Epoch.Cause.Primary
                == epoch_primary_cause::base_update,
        "a genuine newer live revision replaces missing retention using its "
        "ordinary semantic cause");

    auto duplicate_observation = newer;
    duplicate_observation.Source.Observation = prior.Source.Observation;
    expect(
        plan_epoch_mutation(
            owner,
            duplicate_observation,
            owner_catalog,
            epoch_preparation_observation{
                .LatestLive = prior,
                .ProposedOrdinal = epoch_ordinal{5},
                .ObservedAt = observed_utc_milliseconds{500},
                .HeadLineage = head_lineage::not_applicable,
                .Material = material(61U, 70U),
                .RetentionRehydration =
                    retention_rehydration_requirement::required,
            })
                .Rejection
            == epoch_reason_code::invalid_observation,
        "one source-observation identity cannot publish a changed semantic "
        "revision");

    malformed.LatestLive.reset();
    expect(
        plan_epoch_mutation(owner, unchanged, owner_catalog, malformed)
                .Rejection
            == epoch_reason_code::invalid_observation,
        "a never-retained pull request cannot claim rehydration");
}

auto test_association_only_policy() -> void
{
    const auto owner = authority();
    const auto owner_catalog = catalog();
    const auto prior = summary(owner, owner_catalog, 4, 10U, 20U, 30U);
    const auto old_review = associated_review(1U, prior.Head);
    const auto new_review = associated_review(2U, prior.Head);
    auto accepted = revision(2U, 10U, 20U, 30U);
    accepted.CurrentViewerReview = provider_review(2U, prior.Head);

    const auto changed = plan_epoch_mutation(
        owner,
        accepted,
        owner_catalog,
        epoch_preparation_observation{
            .LatestLive = prior,
            .CatalogCurrentViewerReview = old_review,
            .ProposedCurrentViewerReview = new_review,
            .ObservedAt = observed_utc_milliseconds{500},
        });
    const auto* association = changed.Mutation.has_value()
        ? std::get_if<associate_review_mutation>(&*changed.Mutation)
        : nullptr;
    expect(
        association != nullptr && association->Review == new_review,
        "changed review on unchanged revision is association-only");

    const auto identical = plan_epoch_mutation(
        owner,
        accepted,
        owner_catalog,
        epoch_preparation_observation{
            .LatestLive = prior,
            .CatalogCurrentViewerReview = new_review,
            .ProposedCurrentViewerReview = new_review,
            .ObservedAt = observed_utc_milliseconds{500},
        });
    expect(
        !identical.Mutation.has_value()
            && identical.Rejection == epoch_reason_code::none,
        "identical review association is idempotent");

    const auto absent_provider_fact = plan_epoch_mutation(
        owner,
        revision(3U, 10U, 20U, 30U),
        owner_catalog,
        epoch_preparation_observation{
            .LatestLive = prior,
            .CatalogCurrentViewerReview = new_review,
            .ObservedAt = observed_utc_milliseconds{600},
        });
    expect(
        !absent_provider_fact.Mutation.has_value()
            && absent_provider_fact.Rejection == epoch_reason_code::none,
        "absent provider candidate does not erase a retained association");

    auto mismatched = new_review;
    mismatched.Review = github_review_id{999U};
    const auto mismatched_proposal = plan_epoch_mutation(
        owner,
        accepted,
        owner_catalog,
        epoch_preparation_observation{
            .LatestLive = prior,
            .CatalogCurrentViewerReview = old_review,
            .ProposedCurrentViewerReview = mismatched,
            .ObservedAt = observed_utc_milliseconds{700},
        });
    expect(
        mismatched_proposal.Rejection == epoch_reason_code::invalid_observation,
        "catalog association proposal must match exact provider review facts");
}

auto test_alias_resolution_and_exact_pairs() -> void
{
    const auto owner = authority();
    const auto owner_catalog = catalog(8);
    const auto previous = summary(owner, owner_catalog, 7, 10U, 20U);
    const auto latest = summary(owner, owner_catalog, 8, 11U, 30U);
    const auto newest_review = associated_review(8U, latest.Head);
    const auto review_commit =
        review_anchor(owner, owner_catalog, newest_review);

    const auto candidates = comparison_candidates{
        .Catalog = owner_catalog,
        .LatestLive = latest,
        .PreviousDistinctHead = previous,
        .CurrentViewerReview = review_commit,
    };

    const auto whole = resolve_comparison(
        owner, named_selection(named_comparison::whole_pr), candidates);
    expect(
        whole.Pair.has_value()
            && whole.Pair->Left.Identity.Kind
                == comparison_anchor_kind::epoch_base
            && whole.Pair->Left.Commit == latest.Base
            && whole.Pair->Right.Commit == latest.Head,
        "Whole PR resolves latest live base to latest live head");

    const auto previous_upload = resolve_comparison(
        owner,
        named_selection(named_comparison::previous_upload_to_latest),
        candidates);
    expect(
        previous_upload.Pair.has_value()
            && previous_upload.Pair->Left.Commit == previous.Head
            && previous_upload.Pair->Right.Commit == latest.Head,
        "Previous upload uses the supplied preceding distinct live head");

    const auto my_review = resolve_comparison(
        owner,
        named_selection(named_comparison::my_last_reviewed_to_latest),
        candidates);
    expect(
        my_review.Pair.has_value()
            && my_review.Pair->Left.Identity.Kind
                == comparison_anchor_kind::review_commit
            && my_review.Pair->Right.Identity.Kind
                == comparison_anchor_kind::epoch_head
            && my_review.Pair->Left.Commit == my_review.Pair->Right.Commit
            && my_review.Pair->Left.Identity != my_review.Pair->Right.Identity,
        "review identity stays distinct from an equal epoch-head OID");

    auto wrong_review_kind = candidates;
    wrong_review_kind.CurrentViewerReview =
        anchor(latest, comparison_anchor_kind::epoch_head);
    expect(
        resolve_comparison(
            owner,
            named_selection(named_comparison::my_last_reviewed_to_latest),
            wrong_review_kind)
                .Reason
            == epoch_reason_code::invalid_observation,
        "My last reviewed accepts only an exact review-commit identity");

    auto unavailable_candidates = candidates;
    unavailable_candidates.PreviousDistinctHead->HeadAvailability =
        anchor_availability::pruned;
    const auto unavailable = resolve_comparison(
        owner,
        named_selection(named_comparison::previous_upload_to_latest),
        unavailable_candidates);
    expect(
        unavailable.Pair.has_value()
            && unavailable.Pair->Left.Identity.Epoch == epoch_ordinal{7}
            && unavailable.Reason == epoch_reason_code::unavailable,
        "alias chooses semantic candidate before availability with no "
        "fallback");

    auto historical_latest = candidates;
    historical_latest.LatestLive->Cause.Origin =
        epoch_origin::explicit_historical_import;
    expect(
        !resolve_comparison(
             owner,
             named_selection(named_comparison::whole_pr),
             historical_latest)
             .Pair.has_value(),
        "historical import never becomes Latest");
    auto historical_previous = candidates;
    historical_previous.PreviousDistinctHead->Cause.Origin =
        epoch_origin::explicit_historical_import;
    expect(
        !resolve_comparison(
             owner,
             named_selection(named_comparison::previous_upload_to_latest),
             historical_previous)
             .Pair.has_value(),
        "historical import never becomes Previous upload");

    const auto previous_head =
        anchor(previous, comparison_anchor_kind::epoch_head);
    const auto latest_head = anchor(latest, comparison_anchor_kind::epoch_head);
    const auto reverse = resolve_comparison(
        owner,
        explicit_selection(latest_head.Identity, previous_head.Identity),
        comparison_candidates{
            .Catalog = owner_catalog,
            .ExplicitLeft = latest_head,
            .ExplicitRight = previous_head,
        });
    expect(
        reverse.Pair.has_value() && reverse.Pair->Left.Commit == latest.Head
            && reverse.Pair->Right.Commit == previous.Head,
        "explicit reverse pair remains exact and legal");

    const auto identical = resolve_comparison(
        owner,
        explicit_selection(latest_head.Identity, latest_head.Identity),
        comparison_candidates{
            .Catalog = owner_catalog,
            .ExplicitLeft = latest_head,
            .ExplicitRight = latest_head,
        });
    expect(
        identical.Pair.has_value()
            && identical.Pair->Left == identical.Pair->Right
            && available_for_diff(*identical.Pair),
        "explicit identical pair remains representable as an empty comparison");

    auto foreign = latest_head;
    foreign.Identity.Authority = authority(9U).Scope;
    expect(
        resolve_comparison(
            owner,
            explicit_selection(foreign.Identity, latest_head.Identity),
            comparison_candidates{
                .Catalog = owner_catalog,
                .ExplicitLeft = foreign,
                .ExplicitRight = latest_head,
            })
                .Reason
            == epoch_reason_code::invalid_selection,
        "cross-authority explicit pair fails closed");
}

auto test_prepare_commit_visibility_and_linearization() -> void
{
    auto state = initial_state();
    const auto owner = authority();
    const auto accepted = revision();
    const auto accepted_output = transition(
        state,
        pull_request_revision_accepted_event{
            .ApplicationSession = state.ApplicationSession,
            .Authority = owner,
            .Revision = accepted,
        });
    expect(
        state.RequestedSelection == named_selection(named_comparison::whole_pr)
            && request_from<invalidate_epoch_authority_request>(accepted_output)
                != nullptr
            && accepted_output.Catalog.has_value()
            && !accepted_output.Catalog->Catalog.has_value()
            && accepted_output.Comparison.has_value()
            && !accepted_output.Comparison->Pair.has_value(),
        "new authority resets to semantic Whole PR and invalidates prior "
        "scope");
    const auto* load =
        request_from<load_epoch_catalog_request>(accepted_output);
    expect(load != nullptr, "new authority emits load request");

    auto wrong_load = load->Context;
    wrong_load.Authority.Account = github_account_id{999U};
    const auto before_mismatch = state;
    const auto mismatch = transition(
        state,
        epoch_catalog_loaded_event{
            .Context = wrong_load,
            .Catalog = catalog(),
        });
    expect(
        diagnostic_is(mismatch, epoch_reason_code::completion_identity_mismatch)
            && state == before_mismatch,
        "same request/generation with wrong authority is mismatch and inert");

    auto stale_load = load->Context;
    stale_load.Request = epoch_request_id{999U};
    const auto stale = transition(
        state,
        epoch_catalog_loaded_event{
            .Context = stale_load,
            .Catalog = catalog(),
        });
    expect(
        diagnostic_is(stale, epoch_reason_code::stale_completion)
            && state == before_mismatch,
        "different request is stale and cannot clear current work");

    const auto loaded = transition(
        state,
        epoch_catalog_loaded_event{
            .Context = load->Context,
            .Catalog = catalog(),
        });
    const auto* prepare = request_from<prepare_epoch_revision_request>(loaded);
    expect(prepare != nullptr, "matching load enters preparation");
    expect(
        diagnostic_is(
            transition(
                state,
                epoch_catalog_loaded_event{
                    .Context = load->Context,
                    .Catalog = catalog(),
                }),
            epoch_reason_code::stale_completion),
        "duplicate load terminal is stale");

    const auto prepared = transition(
        state,
        epoch_revision_prepared_event{
            .Context = prepare->Context,
            .Operation = opaque<epoch_operation_id>(71U),
            .Observation =
                epoch_preparation_observation{
                    .ProposedOrdinal = epoch_ordinal{1},
                    .ObservedAt = observed_utc_milliseconds{100},
                    .Material = material(),
                },
        });
    const auto* commit = request_from<commit_epoch_catalog_request>(prepared);
    expect(
        commit != nullptr && !prepared.AcceptedEpoch.has_value()
            && state.Catalog->Revision == epoch_catalog_revision{0}
            && !state.AcceptedComparison.has_value(),
        "prepared candidate is invisible until exact durable commit terminal");

    const auto cancellation = transition(state, cancel_epoch_refresh_intent{});
    expect(
        request_from<cancel_epoch_work_request>(cancellation) != nullptr
            && state.PendingIngestion->CancellationRequested
            && cancellation.Commands.has_value()
            && !cancellation.Commands->Commands[2].Enabled
            && has_changed(
                cancellation.Changed, epoch_changed_domain::commands),
        "advisory cancellation disables its command while commit is pending");

    const auto committed = transition(
        state,
        epoch_catalog_committed_event{
            .Context = commit->Context,
            .Mutation = commit->Mutation,
            .NewRevision = epoch_catalog_revision{1},
        });
    expect(
        committed.AcceptedEpoch.has_value()
            && state.Catalog->Revision == epoch_catalog_revision{1}
            && request_from<resolve_comparison_request>(committed) != nullptr,
        "matching success wins a cancellation race at catalog linearization");

    const auto after_commit = state;
    const auto duplicate = transition(
        state,
        epoch_catalog_committed_event{
            .Context = commit->Context,
            .Mutation = commit->Mutation,
            .NewRevision = epoch_catalog_revision{1},
        });
    expect(
        diagnostic_is(duplicate, epoch_reason_code::stale_completion)
            && state == after_commit,
        "duplicate committed terminal is stale and inert");
}

auto test_commit_payload_binding_and_cancellation_point() -> void
{
    auto begin_commit = []
    {
        auto state = initial_state();
        const auto owner = authority();
        const auto accepted = revision();
        const auto accepted_output = transition(
            state,
            pull_request_revision_accepted_event{
                .ApplicationSession = state.ApplicationSession,
                .Authority = owner,
                .Revision = accepted,
            });
        const auto load =
            *request_from<load_epoch_catalog_request>(accepted_output);
        const auto loaded = transition(
            state,
            epoch_catalog_loaded_event{
                .Context = load.Context,
                .Catalog = catalog(),
            });
        const auto prepare =
            *request_from<prepare_epoch_revision_request>(loaded);
        const auto prepared = transition(
            state,
            epoch_revision_prepared_event{
                .Context = prepare.Context,
                .Operation = opaque<epoch_operation_id>(81U),
                .Observation =
                    epoch_preparation_observation{
                        .ProposedOrdinal = epoch_ordinal{1},
                        .ObservedAt = observed_utc_milliseconds{100},
                    },
            });
        return std::pair{
            state,
            *request_from<commit_epoch_catalog_request>(prepared),
        };
    };

    {
        auto [state, commit] = begin_commit();
        auto wrong_context = commit.Context;
        wrong_context.Operation = opaque<epoch_operation_id>(82U);
        const auto before = state;
        const auto result = transition(
            state,
            epoch_catalog_committed_event{
                .Context = wrong_context,
                .Mutation = commit.Mutation,
                .NewRevision = epoch_catalog_revision{1},
            });
        expect(
            diagnostic_is(
                result, epoch_reason_code::completion_identity_mismatch)
                && state == before,
            "wrong durable operation ID cannot commit another preparation");
    }
    {
        auto [state, commit] = begin_commit();
        const epoch_catalog_mutation wrong_mutation = associate_review_mutation{
            .Review = associated_review(9U, oid<git_commit_oid>(20U)),
        };
        const auto before = state;
        const auto result = transition(
            state,
            epoch_catalog_committed_event{
                .Context = commit.Context,
                .Mutation = wrong_mutation,
                .NewRevision = epoch_catalog_revision{1},
            });
        expect(
            diagnostic_is(
                result, epoch_reason_code::completion_identity_mismatch)
                && state == before,
            "same request and operation with wrong mutation is mismatch");
    }
    {
        auto [state, commit] = begin_commit();
        const auto before = state;
        const auto result = transition(
            state,
            epoch_catalog_commit_cancelled_event{
                .Context = commit.Context,
                .Mutation = commit.Mutation,
                .Progress = epoch_commit_progress::ref_commit_in_progress,
            });
        expect(
            diagnostic_is(
                result,
                epoch_reason_code::cancellation_after_point_of_no_return)
                && state == before,
            "cancellation after ref-commit phase is rejected pending recovery");
    }
    {
        auto [state, commit] = begin_commit();
        const auto before = state;
        const auto result = transition(
            state,
            epoch_catalog_commit_cancelled_event{
                .Context = commit.Context,
                .Mutation = commit.Mutation,
                .Progress = static_cast<epoch_commit_progress>(255U),
            });
        expect(
            diagnostic_is(result, epoch_reason_code::invalid_observation)
                && state == before,
            "malformed cancellation progress retains pending recovery work");
    }
    {
        auto [state, commit] = begin_commit();
        const auto result = transition(
            state,
            epoch_catalog_commit_cancelled_event{
                .Context = commit.Context,
                .Mutation = commit.Mutation,
                .Progress = epoch_commit_progress::before_ref_commit,
            });
        expect(
            diagnostic_is(result, epoch_reason_code::cancelled)
                && !state.PendingIngestion.has_value()
                && state.Catalog->Revision == epoch_catalog_revision{0}
                && !result.AcceptedEpoch.has_value(),
            "pre-ref-commit cancellation publishes no epoch");
    }
    {
        auto [state, commit] = begin_commit();
        const auto result = transition(
            state,
            epoch_catalog_committed_event{
                .Context = commit.Context,
                .Mutation = commit.Mutation,
                .NewRevision = epoch_catalog_revision{2},
            });
        expect(
            diagnostic_is(
                result, epoch_reason_code::completion_identity_mismatch)
                && !state.PendingIngestion.has_value()
                && state.Catalog->Revision == epoch_catalog_revision{0}
                && state.Status == epoch_status::failed,
            "current success with a wrong next revision terminalizes as "
            "failure");
    }
    {
        auto [state, commit] = begin_commit();
        const auto old_generation = state.EpochGeneration;
        const auto result = transition(
            state,
            epoch_catalog_commit_failed_event{
                .Context = commit.Context,
                .Mutation = commit.Mutation,
                .Failure = epoch_failure::catalog_conflict,
            });
        const auto* reload = request_from<load_epoch_catalog_request>(result);
        expect(
            diagnostic_is(result, epoch_reason_code::catalog_conflict)
                && reload != nullptr
                && reload->Context.Generation != old_generation
                && !state.Catalog.has_value()
                && !state.AcceptedComparison.has_value()
                && state.AcceptedComparisonRevision
                    == comparison_revision{}
                && state.RequestedSelection
                    == named_selection(named_comparison::whole_pr)
                && result.Catalog.has_value()
                && !result.Catalog->Catalog.has_value()
                && result.Selection.has_value()
                && result.Comparison.has_value()
                && !result.Comparison->Pair.has_value()
                && has_changed(
                    result.Changed, epoch_changed_domain::catalog)
                && has_changed(
                    result.Changed, epoch_changed_domain::selection)
                && has_changed(
                    result.Changed, epoch_changed_domain::comparison)
                && state.PendingIngestion.has_value()
                && state.PendingIngestion->Phase
                    == epoch_ingestion_phase::loading_catalog
                && state.Status == epoch_status::loading_catalog,
            "catalog conflict clears every catalog-bound projection and "
            "loads a fresh bounded catalog");

        auto new_catalog = catalog(7);
        new_catalog.Incarnation =
            opaque<epoch_catalog_incarnation_id>(42U);
        const auto loaded = transition(
            state,
            epoch_catalog_loaded_event{
                .Context = reload->Context,
                .Catalog = new_catalog,
            });
        expect(
            request_from<prepare_epoch_revision_request>(loaded) != nullptr
                && state.Catalog == new_catalog
                && !state.AcceptedComparison.has_value(),
            "a replacement catalog incarnation cannot inherit an old pair");

        const auto replacement_epoch =
            summary(authority(), new_catalog, 1, 10U, 20U);
        const auto replacement_head =
            anchor(replacement_epoch, comparison_anchor_kind::epoch_head);
        const auto before_selection = state;
        const auto one_sided = transition(
            state,
            select_comparison_anchor_intent{
                .Side = comparison_side::right,
                .Anchor = replacement_head.Identity,
                .ExpectedCatalogRevision = new_catalog.Revision,
            });
        expect(
            diagnostic_is(
                one_sided, epoch_reason_code::no_resolved_comparison)
                && state == before_selection,
            "one-sided selection cannot combine a new catalog anchor with "
            "an old-incarnation pair");
    }
}

auto test_catalog_conflict_reloads_catalog() -> void
{
    {
        auto fixture = make_ready_fixture();
        const auto old_generation = fixture.State.EpochGeneration;
        const auto refresh =
            transition(fixture.State, refresh_epochs_intent{});
        const auto prepare =
            *request_from<prepare_epoch_revision_request>(refresh);
        static_cast<void>(
            transition(fixture.State, cancel_epoch_refresh_intent{}));

        auto wrong_context = prepare.Context;
        wrong_context.CatalogIncarnation =
            opaque<epoch_catalog_incarnation_id>(99U);
        const auto before_mismatch = fixture.State;
        const auto mismatch = transition(
            fixture.State,
            epoch_revision_prepare_failed_event{
                .Context = wrong_context,
                .Failure = epoch_failure::catalog_conflict,
            });
        expect(
            diagnostic_is(
                mismatch, epoch_reason_code::completion_identity_mismatch)
                && fixture.State == before_mismatch,
            "mismatched preparation conflict cannot clear catalog state");

        const auto conflict = transition(
            fixture.State,
            epoch_revision_prepare_failed_event{
                .Context = prepare.Context,
                .Failure = epoch_failure::catalog_conflict,
            });
        const auto* reload =
            request_from<load_epoch_catalog_request>(conflict);
        expect(
            diagnostic_is(conflict, epoch_reason_code::catalog_conflict)
                && reload != nullptr
                && reload->Context.Generation != old_generation
                && reload->Context.Authority == fixture.Authority
                && reload->Context.Revision == fixture.Revision
                && !fixture.State.Catalog.has_value()
                && !fixture.State.AcceptedComparison.has_value()
                && fixture.State.AcceptedComparisonRevision
                    == comparison_revision{}
                && fixture.State.RequestedSelection
                    == named_selection(named_comparison::whole_pr)
                && fixture.State.PendingIngestion.has_value()
                && fixture.State.PendingIngestion->Phase
                    == epoch_ingestion_phase::loading_catalog
                && fixture.State.Status == epoch_status::loading_catalog
                && conflict.Catalog.has_value()
                && !conflict.Catalog->Catalog.has_value()
                && conflict.Selection.has_value()
                && conflict.Comparison.has_value()
                && !conflict.Comparison->Pair.has_value(),
            "preparation catalog conflict supersedes advisory cancellation "
            "and reloads from fail-closed catalog state");

        const auto before_stale = fixture.State;
        const auto stale = transition(
            fixture.State,
            epoch_revision_prepare_failed_event{
                .Context = prepare.Context,
                .Failure = epoch_failure::unavailable,
            });
        expect(
            diagnostic_is(stale, epoch_reason_code::stale_completion)
                && fixture.State == before_stale,
            "pre-conflict preparation terminals cannot disturb the fresh "
            "catalog load");
    }

    {
        auto fixture = make_ready_fixture();
        const auto old_generation = fixture.State.EpochGeneration;
        const auto selected = transition(
            fixture.State,
            select_named_comparison_intent{
                .Alias = named_comparison::previous_upload_to_latest,
                .ExpectedCatalogRevision = fixture.Catalog.Revision,
            });
        const auto resolve =
            *request_from<resolve_comparison_request>(selected);

        const auto conflict = transition(
            fixture.State,
            comparison_resolve_failed_event{
                .Context = resolve.Context,
                .Failure = epoch_failure::catalog_conflict,
            });
        const auto* reload =
            request_from<load_epoch_catalog_request>(conflict);
        expect(
            diagnostic_is(conflict, epoch_reason_code::catalog_conflict)
                && conflict.Requests.size() == 1U && reload != nullptr
                && reload->Context.Generation != old_generation
                && !fixture.State.Catalog.has_value()
                && !fixture.State.AcceptedComparison.has_value()
                && fixture.State.AcceptedComparisonRevision
                    == comparison_revision{}
                && fixture.State.RequestedSelection
                    == named_selection(named_comparison::whole_pr)
                && !fixture.State.PendingComparison.has_value()
                && fixture.State.PendingIngestion.has_value()
                && fixture.State.PendingIngestion->Phase
                    == epoch_ingestion_phase::loading_catalog
                && fixture.State.Status == epoch_status::loading_catalog,
            "comparison catalog conflict retires its terminal lane and "
            "starts the same bounded catalog reload path");

        const auto before_stale = fixture.State;
        const auto stale = transition(
            fixture.State,
            comparison_resolved_event{
                .Context = resolve.Context,
                .Candidates =
                    comparison_candidates{
                        .Catalog = fixture.Catalog,
                        .LatestLive = fixture.Epoch,
                    },
            });
        expect(
            diagnostic_is(stale, epoch_reason_code::stale_completion)
                && fixture.State == before_stale,
            "pre-conflict comparison results cannot overwrite the fresh "
            "catalog load");
    }

    {
        auto fixture = make_ready_fixture();
        fixture.State.NextRequest = epoch_request_id{
            std::numeric_limits<std::uint64_t>::max()};
        const auto selected = transition(
            fixture.State,
            select_named_comparison_intent{
                .Alias = named_comparison::previous_upload_to_latest,
                .ExpectedCatalogRevision = fixture.Catalog.Revision,
            });
        const auto resolve =
            *request_from<resolve_comparison_request>(selected);
        expect(
            fixture.State.RequestCounterExhausted,
            "comparison can consume the final bounded request identity");

        const auto conflict = transition(
            fixture.State,
            comparison_resolve_failed_event{
                .Context = resolve.Context,
                .Failure = epoch_failure::catalog_conflict,
            });
        expect(
            conflict.Diagnostics
                    == std::vector{
                        epoch_diagnostic{
                            .Code = epoch_reason_code::catalog_conflict,
                        },
                        epoch_diagnostic{
                            .Code =
                                epoch_reason_code::request_counter_exhausted,
                        },
                    }
                && conflict.Requests.empty()
                && !fixture.State.Catalog.has_value()
                && !fixture.State.PendingComparison.has_value()
                && !fixture.State.PendingIngestion.has_value()
                && fixture.State.Status == epoch_status::failed,
            "catalog conflict fails closed when the bounded reload request "
            "space is exhausted");
    }
}

auto test_current_malformed_terminals_close_work() -> void
{
    {
        auto state = initial_state();
        const auto accepted = transition(
            state,
            pull_request_revision_accepted_event{
                .ApplicationSession = state.ApplicationSession,
                .Authority = authority(),
                .Revision = revision(),
            });
        const auto load = *request_from<load_epoch_catalog_request>(accepted);
        const auto malformed = transition(
            state,
            epoch_catalog_loaded_event{
                .Context = load.Context,
                .Catalog = {},
            });
        expect(
            diagnostic_is(malformed, epoch_reason_code::invalid_catalog)
                && !state.PendingIngestion.has_value()
                && state.Status == epoch_status::failed,
            "current malformed catalog load terminal closes its pending work");
    }

    {
        auto state = initial_state();
        const auto accepted = transition(
            state,
            pull_request_revision_accepted_event{
                .ApplicationSession = state.ApplicationSession,
                .Authority = authority(),
                .Revision = revision(),
            });
        const auto load = *request_from<load_epoch_catalog_request>(accepted);
        const auto loaded = transition(
            state,
            epoch_catalog_loaded_event{
                .Context = load.Context,
                .Catalog = catalog(),
            });
        const auto prepare =
            *request_from<prepare_epoch_revision_request>(loaded);
        const auto malformed = transition(
            state,
            epoch_revision_prepared_event{
                .Context = prepare.Context,
                .Operation = {},
                .Observation =
                    epoch_preparation_observation{
                        .ProposedOrdinal = epoch_ordinal{1},
                        .ObservedAt = observed_utc_milliseconds{100},
                    },
            });
        expect(
            diagnostic_is(malformed, epoch_reason_code::invalid_identity)
                && !state.PendingIngestion.has_value()
                && state.Status == epoch_status::failed,
            "current prepared terminal with no operation identity closes its "
            "pending work");
    }

    {
        auto state = initial_state();
        const auto accepted = transition(
            state,
            pull_request_revision_accepted_event{
                .ApplicationSession = state.ApplicationSession,
                .Authority = authority(),
                .Revision = revision(),
            });
        const auto load = *request_from<load_epoch_catalog_request>(accepted);
        const auto loaded = transition(
            state,
            epoch_catalog_loaded_event{
                .Context = load.Context,
                .Catalog = catalog(),
            });
        const auto prepare =
            *request_from<prepare_epoch_revision_request>(loaded);
        const auto malformed = transition(
            state,
            epoch_revision_prepared_event{
                .Context = prepare.Context,
                .Operation = opaque<epoch_operation_id>(83U),
                .Observation =
                    epoch_preparation_observation{
                        .ObservedAt = observed_utc_milliseconds{100},
                    },
            });
        expect(
            diagnostic_is(malformed, epoch_reason_code::invalid_observation)
                && request_from<discard_epoch_preparation_request>(malformed)
                    != nullptr
                && !state.PendingIngestion.has_value(),
            "current rejected preparation is exact-discarded before failure");
    }

    {
        auto fixture = make_ready_fixture();
        const auto selected = transition(
            fixture.State,
            select_named_comparison_intent{
                .Alias = named_comparison::previous_upload_to_latest,
                .ExpectedCatalogRevision = fixture.Catalog.Revision,
            });
        const auto resolve =
            *request_from<resolve_comparison_request>(selected);
        auto mismatched_catalog = fixture.Catalog;
        mismatched_catalog.Incarnation =
            opaque<epoch_catalog_incarnation_id>(42U);
        const auto malformed = transition(
            fixture.State,
            comparison_resolved_event{
                .Context = resolve.Context,
                .Candidates =
                    comparison_candidates{
                        .Catalog = mismatched_catalog,
                    },
            });
        expect(
            diagnostic_is(
                malformed, epoch_reason_code::completion_identity_mismatch)
                && !fixture.State.PendingComparison.has_value()
                && fixture.State.Status == epoch_status::failed,
            "current comparison terminal with a mismatched catalog closes its "
            "pending work");
    }
}

auto test_noop_discard_and_exact_invalidation() -> void
{
    {
        auto switched = make_ready_fixture();
        const auto replacement = authority(9U);
        const auto result = transition(
            switched.State,
            pull_request_revision_accepted_event{
                .ApplicationSession = switched.State.ApplicationSession,
                .Authority = replacement,
                .Revision = revision(2U, 11U, 21U, 31U),
            });
        expect(
            switched.State.Authority == replacement
                && !switched.State.Catalog.has_value()
                && !switched.State.AcceptedComparison.has_value()
                && result.Catalog.has_value()
                && !result.Catalog->Catalog.has_value()
                && result.Selection.has_value()
                && result.Selection->Selection
                    == named_selection(named_comparison::whole_pr)
                && result.Comparison.has_value()
                && !result.Comparison->Pair.has_value()
                && has_changed(
                    result.Changed, epoch_changed_domain::catalog)
                && has_changed(
                    result.Changed, epoch_changed_domain::selection)
                && has_changed(
                    result.Changed, epoch_changed_domain::comparison),
            "authority switch publishes every catalog-bound projection "
            "clear");
    }

    auto fixture = make_ready_fixture();
    const auto old_revision = fixture.State.AcceptedRevision;
    const auto old_generation = fixture.State.EpochGeneration;
    const auto refreshed_revision = revision(2U, 10U, 20U, 30U);
    const auto refresh = transition(
        fixture.State,
        pull_request_revision_accepted_event{
            .ApplicationSession = fixture.State.ApplicationSession,
            .Authority = fixture.Authority,
            .Revision = refreshed_revision,
        });
    const auto* prepare = request_from<prepare_epoch_revision_request>(refresh);
    expect(prepare != nullptr, "same-authority refresh prepares directly");

    const auto before_delayed = fixture.State;
    const auto delayed = transition(
        fixture.State,
        pull_request_revision_invalidated_event{
            .ApplicationSession = fixture.State.ApplicationSession,
            .Authority = fixture.Authority,
            .Revision = *old_revision,
            .Generation = old_generation,
        });
    expect(
        diagnostic_is(delayed, epoch_reason_code::stale_completion)
            && fixture.State == before_delayed,
        "delayed same-authority invalidation cannot clear newer revision");

    const auto no_change = transition(
        fixture.State,
        epoch_revision_prepared_event{
            .Context = prepare->Context,
            .Operation = opaque<epoch_operation_id>(91U),
            .Observation =
                epoch_preparation_observation{
                    .LatestLive = fixture.Epoch,
                    .ObservedAt = observed_utc_milliseconds{200},
                    .Material = fixture.Epoch.Material,
                },
        });
    expect(
        request_from<discard_epoch_preparation_request>(no_change) != nullptr
            && request_from<commit_epoch_catalog_request>(no_change) == nullptr
            && request_from<resolve_comparison_request>(no_change) != nullptr
            && fixture.State.Catalog->Revision == fixture.Catalog.Revision,
        "unchanged prepared operation is exact-discarded without commit");

    const auto exact_revision = *fixture.State.AcceptedRevision;
    const auto exact_generation = fixture.State.EpochGeneration;
    const auto invalidated = transition(
        fixture.State,
        pull_request_revision_invalidated_event{
            .ApplicationSession = fixture.State.ApplicationSession,
            .Authority = fixture.Authority,
            .Revision = exact_revision,
            .Generation = exact_generation,
        });
    expect(
        invalidated.Authority.has_value()
            && !invalidated.Authority->Authority.has_value()
            && invalidated.Catalog.has_value()
            && !invalidated.Catalog->Catalog.has_value()
            && invalidated.Selection.has_value()
            && invalidated.Selection->Selection
                == named_selection(named_comparison::whole_pr)
            && invalidated.Comparison.has_value()
            && !invalidated.Comparison->Pair.has_value()
            && has_changed(
                invalidated.Changed, epoch_changed_domain::catalog)
            && has_changed(
                invalidated.Changed, epoch_changed_domain::selection)
            && has_changed(
                invalidated.Changed, epoch_changed_domain::comparison)
            && !fixture.State.Authority.has_value()
            && !fixture.State.AcceptedComparison.has_value(),
        "exact invalidation publishes every authority-sensitive clear");
}

auto test_association_only_commit_transition() -> void
{
    auto fixture = make_ready_fixture();
    const auto provider = provider_review(3U, fixture.Epoch.Head);
    const auto association = associated_review(3U, fixture.Epoch.Head);
    auto refreshed = revision(2U, 10U, 20U, 30U);
    refreshed.CurrentViewerReview = provider;

    const auto refresh = transition(
        fixture.State,
        pull_request_revision_accepted_event{
            .ApplicationSession = fixture.State.ApplicationSession,
            .Authority = fixture.Authority,
            .Revision = refreshed,
        });
    const auto* prepare = request_from<prepare_epoch_revision_request>(refresh);
    const auto prepared = transition(
        fixture.State,
        epoch_revision_prepared_event{
            .Context = prepare->Context,
            .Operation = opaque<epoch_operation_id>(92U),
            .Observation =
                epoch_preparation_observation{
                    .LatestLive = fixture.Epoch,
                    .ProposedCurrentViewerReview = association,
                    .ObservedAt = observed_utc_milliseconds{200},
                    .Material = fixture.Epoch.Material,
                },
        });
    const auto* commit = request_from<commit_epoch_catalog_request>(prepared);
    expect(
        commit != nullptr
            && std::holds_alternative<associate_review_mutation>(
                commit->Mutation)
            && !prepared.AcceptedEpoch.has_value(),
        "unchanged revision emits only association mutation");

    const auto committed = transition(
        fixture.State,
        epoch_catalog_committed_event{
            .Context = commit->Context,
            .Mutation = commit->Mutation,
            .NewRevision = epoch_catalog_revision{2},
        });
    expect(
        !committed.AcceptedEpoch.has_value() && committed.Catalog.has_value()
            && committed.Catalog->ChangedReviewAssociation
                == association.Association
            && !committed.Catalog->InsertedEpoch.has_value()
            && fixture.State.Catalog->Revision == epoch_catalog_revision{2},
        "association terminal advances catalog once and creates no epoch");
}

auto test_fail_closed_comparison_exhaustion() -> void
{
    {
        auto fixture = make_ready_fixture();
        const auto refreshed = revision(2U, 10U, 21U, 30U);
        const auto refresh = transition(
            fixture.State,
            pull_request_revision_accepted_event{
                .ApplicationSession = fixture.State.ApplicationSession,
                .Authority = fixture.Authority,
                .Revision = refreshed,
            });
        const auto* prepare =
            request_from<prepare_epoch_revision_request>(refresh);
        const auto prepared = transition(
            fixture.State,
            epoch_revision_prepared_event{
                .Context = prepare->Context,
                .Operation = opaque<epoch_operation_id>(93U),
                .Observation =
                    epoch_preparation_observation{
                        .LatestLive = fixture.Epoch,
                        .ProposedOrdinal = epoch_ordinal{2},
                        .ObservedAt = observed_utc_milliseconds{200},
                        .HeadLineage = head_lineage::descendant,
                        .Material = material(62U, 72U),
                    },
            });
        const auto* commit =
            request_from<commit_epoch_catalog_request>(prepared);
        const auto selected = transition(
            fixture.State,
            select_named_comparison_intent{
                .Alias = named_comparison::previous_upload_to_latest,
                .ExpectedCatalogRevision = fixture.Catalog.Revision,
            });
        const auto pending_comparison =
            *request_from<resolve_comparison_request>(selected);
        fixture.State.ComparisonGeneration =
            comparison_generation{std::numeric_limits<std::uint64_t>::max()};
        const auto committed = transition(
            fixture.State,
            epoch_catalog_committed_event{
                .Context = commit->Context,
                .Mutation = commit->Mutation,
                .NewRevision = epoch_catalog_revision{2},
            });
        const auto* cancelled_comparison =
            request_from<cancel_epoch_work_request>(committed);
        expect(
            committed.AcceptedEpoch.has_value()
                && diagnostic_is(
                    committed,
                    epoch_reason_code::comparison_generation_exhausted)
                && fixture.State.Catalog->Revision == epoch_catalog_revision{2}
                && !fixture.State.AcceptedComparison.has_value()
                && !fixture.State.PendingComparison.has_value()
                && cancelled_comparison != nullptr
                && cancelled_comparison->Request
                    == pending_comparison.Context.Request,
            "generation exhaustion retires superseded comparison work after a "
            "durable commit");
    }

    {
        auto fixture = make_ready_fixture();
        fixture.State.AcceptedComparisonRevision =
            comparison_revision{maximum_comparison_revision};
        const auto alternate =
            summary(fixture.Authority, fixture.Catalog, 2, 12U, 32U);
        const auto alternate_head =
            anchor(alternate, comparison_anchor_kind::epoch_head);
        const auto selected = transition(
            fixture.State,
            select_comparison_anchors_intent{
                .Left = alternate_head.Identity,
                .Right = alternate_head.Identity,
                .ExpectedCatalogRevision = fixture.Catalog.Revision,
            });
        const auto* resolve =
            request_from<resolve_comparison_request>(selected);
        const auto exhausted = transition(
            fixture.State,
            comparison_resolved_event{
                .Context = resolve->Context,
                .Candidates =
                    comparison_candidates{
                        .Catalog = fixture.Catalog,
                        .ExplicitLeft = alternate_head,
                        .ExplicitRight = alternate_head,
                    },
            });
        expect(
            diagnostic_is(
                exhausted, epoch_reason_code::comparison_revision_exhausted)
                && !fixture.State.AcceptedComparison.has_value()
                && !fixture.State.PendingComparison.has_value()
                && fixture.State.Status == epoch_status::unavailable,
            "comparison revision exhaustion clears pending and old "
            "authorization");
    }
}

auto test_invalid_cross_facts_and_command_policy() -> void
{
    const auto owner = authority();
    const auto owner_catalog = catalog();
    const auto prior = summary(owner, owner_catalog, 1, 10U, 20U);
    const auto initial_bad_lineage = plan_epoch_mutation(
        owner,
        revision(),
        owner_catalog,
        epoch_preparation_observation{
            .ProposedOrdinal = epoch_ordinal{1},
            .ObservedAt = observed_utc_milliseconds{100},
            .HeadLineage = head_lineage::descendant,
        });
    expect(
        initial_bad_lineage.Rejection == epoch_reason_code::invalid_observation,
        "initial observation rejects an invented lineage transition");

    const auto unchanged_bad_lineage = plan_epoch_mutation(
        owner,
        revision(2U, 10U, 20U),
        owner_catalog,
        epoch_preparation_observation{
            .LatestLive = prior,
            .ObservedAt = observed_utc_milliseconds{200},
            .HeadLineage = head_lineage::descendant,
        });
    expect(
        unchanged_bad_lineage.Rejection
            == epoch_reason_code::invalid_observation,
        "unchanged head rejects a non-applicable ancestry claim");

    auto malformed = prior;
    malformed.Cause.Primary = static_cast<epoch_primary_cause>(255U);
    expect(
        resolve_comparison(
            owner,
            named_selection(named_comparison::whole_pr),
            comparison_candidates{
                .Catalog = owner_catalog,
                .LatestLive = malformed,
            })
                .Reason
            == epoch_reason_code::invalid_observation,
        "malformed persisted cause enum fails closed");
    expect(
        resolve_comparison(
            owner,
            named_selection(static_cast<named_comparison>(255U)),
            comparison_candidates{
                .Catalog = owner_catalog,
                .LatestLive = prior,
            })
                .Reason
            == epoch_reason_code::invalid_selection,
        "malformed named alias fails closed");

    auto invalid_review_revision = revision();
    invalid_review_revision.CurrentViewerReview = provider_review(
        1U, invalid_review_revision.Head, github_viewer_actor_id{999U});
    auto state = initial_state();
    const auto before = state;
    expect(
        diagnostic_is(
            transition(
                state,
                pull_request_revision_accepted_event{
                    .ApplicationSession = state.ApplicationSession,
                    .Authority = owner,
                    .Revision = invalid_review_revision,
                }),
            epoch_reason_code::invalid_revision)
            && state == before,
        "review actor must equal exact accepted viewer authority");

    {
        epoch_state invalid_session;
        const auto before = invalid_session;
        const auto result = transition(
            invalid_session,
            pull_request_revision_accepted_event{
                .ApplicationSession = application_session_id{},
                .Authority = owner,
                .Revision = revision(),
            });
        expect(
            diagnostic_is(result, epoch_reason_code::invalid_revision)
                && invalid_session == before && result.Requests.empty(),
            "zero application session cannot enter a host request context");
    }

    auto fixture = make_ready_fixture();
    const auto ready_commands = epoch_commands(fixture.State);
    expect(
        ready_commands[0].Enabled && ready_commands[1].Enabled
            && !ready_commands[2].Enabled,
        "ready core enables refresh and selection but not cancellation");

    {
        auto last_request_state = initial_state();
        last_request_state.NextRequest =
            epoch_request_id{std::numeric_limits<std::uint64_t>::max()};
        const auto exhausted = make_ready_fixture(last_request_state);
        const auto commands = epoch_commands(exhausted.State);
        expect(
            exhausted.State.RequestCounterExhausted && !commands[0].Enabled
                && commands[0].DisabledReason
                    == epoch_reason_code::request_counter_exhausted
                && !commands[1].Enabled
                && commands[1].DisabledReason
                    == epoch_reason_code::request_counter_exhausted,
            "last request completes normally and then disables new "
            "request-backed commands");
    }

    {
        auto exhausted = make_ready_fixture();
        exhausted.State.EpochGeneration =
            epoch_generation{std::numeric_limits<std::uint64_t>::max()};
        const auto commands = epoch_commands(exhausted.State);
        expect(
            !commands[0].Enabled
                && commands[0].DisabledReason
                    == epoch_reason_code::epoch_generation_exhausted
                && commands[1].Enabled,
            "epoch generation exhaustion disables only refresh");
    }

    {
        auto exhausted = make_ready_fixture();
        exhausted.State.ComparisonGeneration =
            comparison_generation{std::numeric_limits<std::uint64_t>::max()};
        const auto commands = epoch_commands(exhausted.State);
        expect(
            commands[0].Enabled && !commands[1].Enabled
                && commands[1].DisabledReason
                    == epoch_reason_code::comparison_generation_exhausted,
            "comparison generation exhaustion disables only selection");
    }

    {
        auto retry_fixture = make_ready_fixture();
        const auto accepted_pair = retry_fixture.State.AcceptedComparison;
        const auto first_refresh =
            transition(retry_fixture.State, refresh_epochs_intent{});
        const auto* first_prepare =
            request_from<prepare_epoch_revision_request>(first_refresh);
        expect(
            first_prepare != nullptr
                && retry_fixture.State.AcceptedComparison == accepted_pair
                && retry_fixture.State.Status == epoch_status::preparing,
            "explicit refresh retries the accepted revision while retaining "
            "its "
            "pair");

        const auto failed = transition(
            retry_fixture.State,
            epoch_revision_prepare_failed_event{
                .Context = first_prepare->Context,
                .Failure = epoch_failure::unavailable,
            });
        expect(
            diagnostic_is(failed, epoch_reason_code::unavailable)
                && !retry_fixture.State.PendingIngestion.has_value(),
            "failed refresh terminal clears only the pending ingestion");

        const auto retried =
            transition(retry_fixture.State, refresh_epochs_intent{});
        const auto* retry_prepare =
            request_from<prepare_epoch_revision_request>(retried);
        const auto retry_commands = epoch_commands(retry_fixture.State);
        expect(
            retry_prepare != nullptr
                && retry_prepare->Context.Request
                    != first_prepare->Context.Request
                && retry_prepare->Context.Generation
                    != first_prepare->Context.Generation
                && retry_fixture.State.AcceptedComparison == accepted_pair
                && !retry_commands[0].Enabled && retry_commands[2].Enabled,
            "refresh intent provides a new exact retry after failure");
    }

    {
        auto interleaved = make_ready_fixture();
        const auto selection = transition(
            interleaved.State,
            select_named_comparison_intent{
                .Alias = named_comparison::previous_upload_to_latest,
                .ExpectedCatalogRevision = interleaved.Catalog.Revision,
            });
        const auto resolve =
            *request_from<resolve_comparison_request>(selection);
        const auto refresh =
            transition(interleaved.State, refresh_epochs_intent{});
        const auto* cancelled =
            request_from<cancel_epoch_work_request>(refresh);
        expect(
            cancelled != nullptr
                && cancelled->Request == resolve.Context.Request
                && !interleaved.State.PendingComparison.has_value()
                && interleaved.State.PendingIngestion.has_value(),
            "refresh cancels and retires an older comparison lane");

        const auto delayed = transition(
            interleaved.State,
            comparison_resolved_event{
                .Context = resolve.Context,
                .Candidates =
                    comparison_candidates{
                        .Catalog = interleaved.Catalog,
                        .LatestLive = interleaved.Epoch,
                    },
            });
        expect(
            diagnostic_is(delayed, epoch_reason_code::stale_completion)
                && interleaved.State.Status == epoch_status::preparing,
            "retired comparison cannot overwrite a newer refresh status");

        const auto prior_request = interleaved.State.PendingIngestion->Request;
        const auto restarted =
            transition(interleaved.State, refresh_epochs_intent{});
        const auto* restart_cancel =
            request_from<cancel_epoch_work_request>(restarted);
        const auto* restart_prepare =
            request_from<prepare_epoch_revision_request>(restarted);
        expect(
            restart_cancel != nullptr
                && restart_cancel->Request == prior_request
                && restart_prepare != nullptr
                && restart_prepare->Context.Request != prior_request,
            "refresh intent explicitly restarts an in-flight ingestion");
    }

    const auto before_side = fixture.State;
    const auto invalid_side = transition(
        fixture.State,
        select_comparison_anchor_intent{
            .Side = static_cast<comparison_side>(255U),
            .Anchor = fixture.State.AcceptedComparison->Left.Identity,
            .ExpectedCatalogRevision = fixture.Catalog.Revision,
        });
    expect(
        diagnostic_is(invalid_side, epoch_reason_code::invalid_selection)
            && fixture.State == before_side,
        "malformed comparison side cannot become an explicit selection");
}

auto test_selection_transition_policy() -> void
{
    auto fixture = make_ready_fixture();
    const auto whole_pair = *fixture.State.AcceptedComparison;
    const auto alternate =
        summary(fixture.Authority, fixture.Catalog, 2, 12U, 32U);
    const auto alternate_head =
        anchor(alternate, comparison_anchor_kind::epoch_head);

    const auto changed = transition(
        fixture.State,
        select_comparison_anchor_intent{
            .Side = comparison_side::left,
            .Anchor = alternate_head.Identity,
            .ExpectedCatalogRevision = fixture.Catalog.Revision,
        });
    const auto* explicit_pair = changed.Selection.has_value()
        ? std::get_if<explicit_comparison_selection>(
              &changed.Selection->Selection)
        : nullptr;
    expect(
        explicit_pair != nullptr
            && explicit_pair->Left == alternate_head.Identity
            && explicit_pair->Right == whole_pair.Right.Identity
            && !fixture.State.AcceptedComparison.has_value(),
        "editing one side converts named selection to an exact explicit pair");

    const auto* resolve = request_from<resolve_comparison_request>(changed);
    expect(resolve != nullptr, "one-side edit requests exact resolution");
    const auto resolved = transition(
        fixture.State,
        comparison_resolved_event{
            .Context = resolve->Context,
            .Candidates =
                comparison_candidates{
                    .Catalog = fixture.Catalog,
                    .ExplicitLeft = alternate_head,
                    .ExplicitRight = whole_pair.Right,
                },
        });
    expect(
        resolved.Comparison.has_value()
            && resolved.Comparison->Pair->Left == alternate_head
            && resolved.Comparison->Pair->Right == whole_pair.Right,
        "explicit one-side resolution preserves both exact identities");

    const auto before_mismatch = fixture.State;
    const auto bad_revision = transition(
        fixture.State,
        select_named_comparison_intent{
            .Alias = named_comparison::whole_pr,
            .ExpectedCatalogRevision = epoch_catalog_revision{999},
        });
    expect(
        diagnostic_is(
            bad_revision, epoch_reason_code::catalog_revision_mismatch)
            && fixture.State == before_mismatch,
        "stale selector catalog revision cannot mutate selection");

    const auto unavailable_head = comparison_anchor{
        .Identity = whole_pair.Right.Identity,
        .Commit = whole_pair.Right.Commit,
        .Availability = anchor_availability::pruned,
    };
    const auto select_unavailable = transition(
        fixture.State,
        select_comparison_anchors_intent{
            .Left = unavailable_head.Identity,
            .Right = unavailable_head.Identity,
            .ExpectedCatalogRevision = fixture.Catalog.Revision,
        });
    const auto* unavailable_request =
        request_from<resolve_comparison_request>(select_unavailable);
    const auto unavailable = transition(
        fixture.State,
        comparison_resolved_event{
            .Context = unavailable_request->Context,
            .Candidates =
                comparison_candidates{
                    .Catalog = fixture.Catalog,
                    .ExplicitLeft = unavailable_head,
                    .ExplicitRight = unavailable_head,
                },
        });
    expect(
        unavailable.Comparison.has_value()
            && unavailable.Comparison->Pair.has_value()
            && !unavailable.Comparison->AvailableForDiff
            && fixture.State.Status == epoch_status::unavailable,
        "exact explicit pair remains selected when its object is unavailable");
}

auto test_overflow_is_checked_and_invalidation_fails_closed() -> void
{
    {
        auto state = initial_state();
        state.NextRequest =
            epoch_request_id{std::numeric_limits<std::uint64_t>::max()};
        const auto first = transition(
            state,
            pull_request_revision_accepted_event{
                .ApplicationSession = state.ApplicationSession,
                .Authority = authority(),
                .Revision = revision(),
            });
        const auto* load = request_from<load_epoch_catalog_request>(first);
        expect(
            load != nullptr
                && load->Context.Request
                    == epoch_request_id{std::numeric_limits<
                        std::uint64_t>::max()}
                && state.RequestCounterExhausted,
            "maximum request ID is issued once and then marked exhausted");

        const auto second = transition(
            state,
            pull_request_revision_accepted_event{
                .ApplicationSession = state.ApplicationSession,
                .Authority = authority(),
                .Revision = revision(2U, 10U, 21U),
            });
        expect(
            diagnostic_is(second, epoch_reason_code::request_counter_exhausted)
                && !state.Authority.has_value()
                && !state.AcceptedComparison.has_value()
                && second.Catalog.has_value()
                && !second.Catalog->Catalog.has_value()
                && second.Selection.has_value()
                && second.Comparison.has_value()
                && !second.Comparison->Pair.has_value()
                && has_changed(
                    second.Changed, epoch_changed_domain::catalog)
                && has_changed(
                    second.Changed, epoch_changed_domain::selection)
                && has_changed(
                    second.Changed, epoch_changed_domain::comparison),
            "exhausted refresh request publishes fail-closed projection "
            "clears");
    }
    {
        auto state = initial_state();
        state.EpochGeneration =
            epoch_generation{std::numeric_limits<std::uint64_t>::max()};
        const auto before = state;
        const auto result = transition(
            state,
            pull_request_revision_accepted_event{
                .ApplicationSession = state.ApplicationSession,
                .Authority = authority(),
                .Revision = revision(),
            });
        expect(
            diagnostic_is(result, epoch_reason_code::epoch_generation_exhausted)
                && state == before,
            "generation overflow rejects activation without partial state");
    }
    {
        auto fixture = make_ready_fixture();
        fixture.State.ComparisonGeneration =
            comparison_generation{std::numeric_limits<std::uint64_t>::max()};
        const auto before = fixture.State;
        const auto result = transition(
            fixture.State,
            select_named_comparison_intent{
                .Alias = named_comparison::previous_upload_to_latest,
                .ExpectedCatalogRevision = fixture.Catalog.Revision,
            });
        expect(
            diagnostic_is(
                result, epoch_reason_code::comparison_generation_exhausted)
                && fixture.State == before,
            "comparison generation overflow leaves selection unchanged");
    }
    {
        auto fixture = make_ready_fixture();
        fixture.State.EpochGeneration =
            epoch_generation{std::numeric_limits<std::uint64_t>::max()};
        fixture.State.ComparisonGeneration =
            comparison_generation{std::numeric_limits<std::uint64_t>::max()};
        const auto result = transition(
            fixture.State,
            pull_request_revision_invalidated_event{
                .ApplicationSession = fixture.State.ApplicationSession,
                .Authority = fixture.Authority,
                .Revision = fixture.Revision,
                .Generation = fixture.State.EpochGeneration,
            });
        expect(
            !fixture.State.Authority.has_value()
                && !fixture.State.AcceptedRevision.has_value()
                && !fixture.State.Catalog.has_value()
                && result.Diagnostics.size() == 2U,
            "externally revoked authority clears fail-closed even at max "
            "generations");
    }
}

auto test_bounded_epoch_history_paging() -> void
{
    {
        auto state = initial_state();
        const auto result = transition(
            state,
            request_epoch_history_page_intent{
                .Catalog = catalog(),
                .Limit = 10U,
            });
        expect(
            diagnostic_is(result, epoch_reason_code::no_active_authority)
                && !state.PendingHistory,
            "history paging requires an active pull-request authority");
    }

    {
        auto fixture = make_ready_fixture();
        const auto before = fixture.State;
        const std::array invalid_requests{
            request_epoch_history_page_intent{
                .Catalog = {},
                .Limit = 10U,
            },
            request_epoch_history_page_intent{
                .Catalog = fixture.Catalog,
                .Limit = 0U,
            },
            request_epoch_history_page_intent{
                .Catalog = fixture.Catalog,
                .Limit = static_cast<std::uint16_t>(
                    maximum_epoch_history_page_size + 1U),
            },
        };
        const std::array expected{
            epoch_reason_code::invalid_catalog,
            epoch_reason_code::invalid_observation,
            epoch_reason_code::invalid_observation,
        };
        for(std::size_t index = 0U; index < invalid_requests.size(); ++index)
        {
            const auto result =
                transition(fixture.State, invalid_requests[index]);
            expect(
                diagnostic_is(result, expected[index])
                    && fixture.State == before,
                "invalid history catalog and page bounds are inert");
        }

        auto wrong_catalog = fixture.Catalog;
        wrong_catalog.Revision = epoch_catalog_revision{99};
        const auto wrong_revision = transition(
            fixture.State,
            request_epoch_history_page_intent{
                .Catalog = wrong_catalog,
                .Limit = 10U,
            });
        expect(
            diagnostic_is(
                wrong_revision,
                epoch_reason_code::catalog_revision_mismatch)
                && fixture.State == before,
            "history paging rejects a stale catalog revision");

        const auto wrong_cursor = transition(
            fixture.State,
            request_epoch_history_page_intent{
                .Catalog = fixture.Catalog,
                .Cursor = history_cursor(fixture, 4),
                .Limit = 2U,
            });
        expect(
            diagnostic_is(
                wrong_cursor, epoch_reason_code::invalid_history_cursor)
                && fixture.State == before,
            "a continuation cursor is valid only after the exact prior page");
    }

    {
        auto fixture = make_ready_fixture();
        const auto status = fixture.State.Status;
        const auto loading = transition(
            fixture.State,
            request_epoch_history_page_intent{
                .Catalog = fixture.Catalog,
                .Limit = 2U,
            });
        const auto* request =
            request_from<load_epoch_history_page_request>(loading);
        expect(
            request != nullptr && loading.Requests.size() == 1U
                && loading.History
                && loading.History->Phase == epoch_history_page_phase::loading
                && loading.History->Epochs.empty()
                && has_changed(
                    loading.Changed, epoch_changed_domain::history)
                && fixture.State.PendingHistory
                && fixture.State.Status == status,
            "one initial intent emits one bounded request without disturbing "
            "comparison status");

        const auto pending = fixture.State;
        const auto duplicate = transition(
            fixture.State,
            request_epoch_history_page_intent{
                .Catalog = fixture.Catalog,
                .Limit = 2U,
            });
        expect(
            diagnostic_is(
                duplicate, epoch_reason_code::duplicate_history_page)
                && fixture.State == pending,
            "an in-flight history page cannot be scheduled twice");

        const auto continuation = history_cursor(fixture, 2);
        const auto loaded = transition(
            fixture.State,
            epoch_history_page_loaded_event{
                .Context = request->Context,
                .Page = history_page(
                    fixture,
                    {
                        summary(
                            fixture.Authority,
                            fixture.Catalog,
                            3,
                            10U,
                            23U),
                        summary(
                            fixture.Authority,
                            fixture.Catalog,
                            2,
                            10U,
                            22U),
                    },
                    continuation),
            });
        expect(
            loaded.History
                && loaded.History->Phase == epoch_history_page_phase::loaded
                && loaded.History->Epochs.size() == 2U
                && loaded.History->Continuation == continuation
                && fixture.State.HistoryCatalog
                    == std::optional{fixture.Catalog}
                && fixture.State.HistoryContinuation == continuation
                && !fixture.State.PendingHistory
                && fixture.State.Status == status,
            "a full initial page publishes rows transiently and retains only "
            "its exact continuation");

        const auto wrong_continuation = transition(
            fixture.State,
            request_epoch_history_page_intent{
                .Catalog = fixture.Catalog,
                .Cursor = history_cursor(fixture, 1),
                .Limit = 2U,
            });
        expect(
            diagnostic_is(
                wrong_continuation,
                epoch_reason_code::invalid_history_cursor)
                && !fixture.State.PendingHistory,
            "a caller cannot skip or rewrite the retained continuation");

        const auto continuing = transition(
            fixture.State,
            request_epoch_history_page_intent{
                .Catalog = fixture.Catalog,
                .Cursor = continuation,
                .Limit = 2U,
            });
        const auto* continuation_request =
            request_from<load_epoch_history_page_request>(continuing);
        expect(
            continuation_request
                && continuation_request->Context.Cursor == continuation,
            "the next page request carries the exact revision-bound cursor");

        auto mismatched_authority = continuation_request->Context;
        mismatched_authority.Authority = authority(2U);
        const auto before_authority_mismatch = fixture.State;
        const auto authority_mismatch = transition(
            fixture.State,
            epoch_history_page_loaded_event{
                .Context = mismatched_authority,
                .Page = history_page(fixture, {}),
            });
        expect(
            diagnostic_is(
                authority_mismatch,
                epoch_reason_code::completion_identity_mismatch)
                && fixture.State == before_authority_mismatch,
            "a history terminal with mismatched authority is inert");

        auto mismatched_context = continuation_request->Context;
        mismatched_context.Source.Snapshot = provider_snapshot_id{999U};
        const auto before_mismatch = fixture.State;
        const auto mismatch = transition(
            fixture.State,
            epoch_history_page_loaded_event{
                .Context = mismatched_context,
                .Page = history_page(fixture, {}),
            });
        expect(
            diagnostic_is(
                mismatch,
                epoch_reason_code::completion_identity_mismatch)
                && fixture.State == before_mismatch,
            "a history terminal with mismatched source identity is inert");

        const auto final_page = transition(
            fixture.State,
            epoch_history_page_loaded_event{
                .Context = continuation_request->Context,
                .Page = history_page(
                    fixture,
                    {
                        summary(
                            fixture.Authority,
                            fixture.Catalog,
                            1,
                            10U,
                            21U),
                    }),
            });
        expect(
            final_page.History && final_page.History->Epochs.size() == 1U
                && !final_page.History->Continuation
                && !fixture.State.HistoryContinuation
                && !fixture.State.PendingHistory,
            "a short continuation page terminates the bounded sequence");

        const auto repeat = transition(
            fixture.State,
            request_epoch_history_page_intent{
                .Catalog = fixture.Catalog,
                .Limit = 2U,
            });
        expect(
            diagnostic_is(
                repeat, epoch_reason_code::duplicate_history_page),
            "an already-completed initial page cannot be replayed");
    }

    {
        const auto reject_malformed_page = [](
            const auto& mutate, const std::string_view claim)
        {
            auto fixture = make_ready_fixture();
            const auto loading = transition(
                fixture.State,
                request_epoch_history_page_intent{
                    .Catalog = fixture.Catalog,
                    .Limit = 2U,
                });
            const auto request =
                *request_from<load_epoch_history_page_request>(loading);
            auto page = history_page(fixture, {});
            mutate(page, fixture);
            const auto rejected = transition(
                fixture.State,
                epoch_history_page_loaded_event{
                    .Context = request.Context,
                    .Page = std::move(page),
                });
            expect(
                diagnostic_is(
                    rejected, epoch_reason_code::invalid_observation)
                    && rejected.History
                    && rejected.History->Phase
                        == epoch_history_page_phase::failed
                    && !fixture.State.PendingHistory
                    && !fixture.State.HistoryCatalog,
                claim);
        };

        reject_malformed_page(
            [](auto& page, const auto&)
            {
                page.Authority = authority(2U).Scope;
            },
            "a page cannot cross pull-request authority");
        reject_malformed_page(
            [](auto& page, const auto&)
            {
                page.Catalog.Revision = epoch_catalog_revision{99};
            },
            "a page cannot cross catalog revision");
        reject_malformed_page(
            [](auto& page, const auto& fixture)
            {
                page.Epochs = {
                    summary(
                        fixture.Authority,
                        fixture.Catalog,
                        3,
                        10U,
                        23U),
                    summary(
                        fixture.Authority,
                        fixture.Catalog,
                        2,
                        10U,
                        22U),
                    summary(
                        fixture.Authority,
                        fixture.Catalog,
                        1,
                        10U,
                        21U),
                };
            },
            "a page cannot exceed its requested bound");
        reject_malformed_page(
            [](auto& page, const auto& fixture)
            {
                page.Epochs = {
                    summary(
                        fixture.Authority,
                        fixture.Catalog,
                        2,
                        10U,
                        22U),
                    summary(
                        fixture.Authority,
                        fixture.Catalog,
                        1,
                        10U,
                        21U),
                };
                page.Continuation = history_cursor(fixture, 2);
            },
            "a continuation must equal the last returned stable ordinal");
    }

    {
        auto fixture = make_ready_fixture();
        const auto loading = transition(
            fixture.State,
            request_epoch_history_page_intent{
                .Catalog = fixture.Catalog,
                .Limit = 2U,
            });
        const auto request =
            *request_from<load_epoch_history_page_request>(loading);
        const auto exact_bound = transition(
            fixture.State,
            epoch_history_page_loaded_event{
                .Context = request.Context,
                .Page = history_page(
                    fixture,
                    {
                        summary(
                            fixture.Authority,
                            fixture.Catalog,
                            2,
                            10U,
                            22U),
                        summary(
                            fixture.Authority,
                            fixture.Catalog,
                            1,
                            10U,
                            21U),
                    }),
            });
        expect(
            exact_bound.History && exact_bound.History->Epochs.size() == 2U
                && !exact_bound.History->Continuation
                && !fixture.State.HistoryContinuation,
            "an exact-bound terminal page needs no invented continuation");
    }

    {
        auto fixture = make_ready_fixture();
        const auto loading = transition(
            fixture.State,
            request_epoch_history_page_intent{
                .Catalog = fixture.Catalog,
                .Limit = 2U,
            });
        const auto request =
            *request_from<load_epoch_history_page_request>(loading);
        const auto empty = transition(
            fixture.State,
            epoch_history_page_loaded_event{
                .Context = request.Context,
                .Page = history_page(fixture, {}),
            });
        expect(
            empty.History && empty.History->Epochs.empty()
                && !empty.History->Continuation
                && fixture.State.HistoryCatalog
                    == std::optional{fixture.Catalog},
            "an empty first page is a valid completed history sequence");
    }

    {
        auto fixture = make_ready_fixture();
        const auto loading = transition(
            fixture.State,
            request_epoch_history_page_intent{
                .Catalog = fixture.Catalog,
                .Limit = 2U,
            });
        const auto request =
            *request_from<load_epoch_history_page_request>(loading);
        const auto malformed = transition(
            fixture.State,
            epoch_history_page_loaded_event{
                .Context = request.Context,
                .Page = history_page(
                    fixture,
                    {
                        summary(
                            fixture.Authority,
                            fixture.Catalog,
                            1,
                            10U,
                            21U),
                        summary(
                            fixture.Authority,
                            fixture.Catalog,
                            2,
                            10U,
                            22U),
                    }),
            });
        expect(
            diagnostic_is(
                malformed, epoch_reason_code::invalid_observation)
                && malformed.History
                && malformed.History->Phase
                    == epoch_history_page_phase::failed
                && !fixture.State.PendingHistory
                && !fixture.State.HistoryCatalog,
            "duplicate or non-descending stable identities fail closed");
    }

    {
        auto fixture = make_ready_fixture();
        const auto loading = transition(
            fixture.State,
            request_epoch_history_page_intent{
                .Catalog = fixture.Catalog,
                .Limit = 2U,
            });
        const auto request =
            *request_from<load_epoch_history_page_request>(loading);
        const auto failed = transition(
            fixture.State,
            epoch_history_page_load_failed_event{
                .Context = request.Context,
                .Failure = epoch_failure::unavailable,
            });
        expect(
            diagnostic_is(failed, epoch_reason_code::unavailable)
                && failed.History
                && failed.History->Phase == epoch_history_page_phase::failed
                && !fixture.State.PendingHistory,
            "a failed history page produces one terminal and remains retryable");

        const auto retry = transition(
            fixture.State,
            request_epoch_history_page_intent{
                .Catalog = fixture.Catalog,
                .Limit = 2U,
            });
        const auto retry_request =
            *request_from<load_epoch_history_page_request>(retry);
        const auto cancelled = transition(
            fixture.State,
            epoch_history_page_load_cancelled_event{
                .Context = retry_request.Context,
            });
        expect(
            diagnostic_is(cancelled, epoch_reason_code::cancelled)
                && cancelled.History
                && cancelled.History->Phase
                    == epoch_history_page_phase::cancelled
                && !fixture.State.PendingHistory,
            "a cancelled history page produces one terminal and remains retryable");
    }

    {
        auto fixture = make_ready_fixture();
        const auto loading = transition(
            fixture.State,
            request_epoch_history_page_intent{
                .Catalog = fixture.Catalog,
                .Limit = 2U,
            });
        const auto request =
            *request_from<load_epoch_history_page_request>(loading);
        const auto refresh = transition(fixture.State, refresh_epochs_intent{});
        expect(
            refresh.History
                && refresh.History->Phase
                    == epoch_history_page_phase::cancelled
                && has_changed(
                    refresh.Changed, epoch_changed_domain::history)
                && has_changed(
                    refresh.Changed, epoch_changed_domain::authority)
                && !fixture.State.PendingHistory,
            "a new epoch generation explicitly cancels the pending page");
        const auto before_stale = fixture.State;
        const auto stale = transition(
            fixture.State,
            epoch_history_page_loaded_event{
                .Context = request.Context,
                .Page = history_page(fixture, {}),
            });
        expect(
            diagnostic_is(stale, epoch_reason_code::stale_completion)
                && fixture.State == before_stale,
            "a page from the prior generation cannot publish after refresh");
    }

    {
        auto fixture = make_ready_fixture();
        const auto loading = transition(
            fixture.State,
            request_epoch_history_page_intent{
                .Catalog = fixture.Catalog,
                .Limit = 2U,
            });
        const auto request =
            *request_from<load_epoch_history_page_request>(loading);
        const auto invalidated = transition(
            fixture.State,
            pull_request_revision_invalidated_event{
                .ApplicationSession = fixture.State.ApplicationSession,
                .Authority = fixture.Authority,
                .Revision = fixture.Revision,
                .Generation = fixture.State.EpochGeneration,
            });
        expect(
            invalidated.History
                && invalidated.History->Phase
                    == epoch_history_page_phase::cancelled
                && !fixture.State.Authority && !fixture.State.Catalog
                && !fixture.State.HistoryCatalog
                && !fixture.State.HistoryContinuation,
            "an authority switch clears every history-page binding");
        const auto stale = transition(
            fixture.State,
            epoch_history_page_loaded_event{
                .Context = request.Context,
                .Page = history_page(fixture, {}),
            });
        expect(
            diagnostic_is(stale, epoch_reason_code::stale_completion),
            "an invalidated authority rejects a late page terminal");
    }

    {
        auto fixture = make_ready_fixture();
        const auto loading = transition(
            fixture.State,
            request_epoch_history_page_intent{
                .Catalog = fixture.Catalog,
                .Limit = 2U,
            });
        const auto request =
            *request_from<load_epoch_history_page_request>(loading);
        const auto conflict = transition(
            fixture.State,
            epoch_history_page_load_failed_event{
                .Context = request.Context,
                .Failure = epoch_failure::catalog_conflict,
            });
        expect(
            diagnostic_is(conflict, epoch_reason_code::catalog_conflict)
                && conflict.History
                && conflict.History->Phase
                    == epoch_history_page_phase::cancelled
                && request_from<load_epoch_catalog_request>(conflict)
                    != nullptr
                && !fixture.State.Catalog
                && fixture.State.PendingIngestion
                && fixture.State.PendingIngestion->Phase
                    == epoch_ingestion_phase::loading_catalog,
            "a changed catalog revision retires paging and reloads exact "
            "authority state");
    }
}

auto test_core_authorized_epoch_maintenance() -> void
{
    {
        auto state = initial_state();
        const auto result = transition(state,
            prune_review_epochs_intent{
                .Catalog = catalog(),
                .Epochs = {},
            });
        expect(diagnostic_is(result, epoch_reason_code::no_active_authority),
            "maintenance requires an active exact authority");
    }

    {
        auto fixture = make_ready_fixture();
        const auto before = fixture.State;
        auto wrong_catalog = fixture.Catalog;
        wrong_catalog.Revision = epoch_catalog_revision{99};
        const auto wrong_revision = transition(fixture.State,
            prune_review_epochs_intent{
                .Catalog = wrong_catalog,
                .Epochs = {fixture.Epoch.Id},
            });
        expect(diagnostic_is(wrong_revision,
                   epoch_reason_code::catalog_revision_mismatch)
                && fixture.State == before,
            "maintenance rejects a stale catalog revision");

        const auto empty = transition(fixture.State,
            prune_review_epochs_intent{
                .Catalog = fixture.Catalog,
            });
        expect(
            diagnostic_is(empty, epoch_reason_code::invalid_maintenance_target)
                && fixture.State == before,
            "prune requires at least one exact epoch");

        auto foreign = fixture.Epoch.Id;
        foreign.Authority = authority(2U).Scope;
        const auto crossed = transition(fixture.State,
            prune_review_epochs_intent{
                .Catalog = fixture.Catalog,
                .Epochs = {foreign},
            });
        expect(diagnostic_is(crossed,
                   epoch_reason_code::invalid_maintenance_target)
                && fixture.State == before,
            "prune cannot cross pull-request authority");

        const auto duplicate = transition(fixture.State,
            prune_review_epochs_intent{
                .Catalog = fixture.Catalog,
                .Epochs = {fixture.Epoch.Id, fixture.Epoch.Id},
            });
        expect(diagnostic_is(duplicate,
                   epoch_reason_code::invalid_maintenance_target)
                && fixture.State == before,
            "prune targets are strictly ordered and unique");

        std::vector<review_epoch_id> oversized;
        oversized.reserve(maximum_epoch_maintenance_target_count + 1U);
        for(std::uint16_t index = 0U;
            index <= maximum_epoch_maintenance_target_count;
            ++index)
        {
            auto id = fixture.Epoch.Id;
            id.Ordinal = epoch_ordinal{static_cast<std::int64_t>(index) + 1};
            oversized.push_back(id);
        }
        const auto too_many = transition(fixture.State,
            prune_review_epochs_intent{
                .Catalog = fixture.Catalog,
                .Epochs = std::move(oversized),
            });
        expect(diagnostic_is(too_many,
                   epoch_reason_code::invalid_maintenance_target)
                && fixture.State == before,
            "prune batches are bounded at one hundred exact epochs");
    }

    {
        auto fixture = make_ready_fixture();
        auto second = fixture.Epoch.Id;
        second.Ordinal = epoch_ordinal{2};
        const auto started = transition(fixture.State,
            prune_review_epochs_intent{
                .Catalog = fixture.Catalog,
                .Epochs = {fixture.Epoch.Id, second},
            });
        const auto* request =
            request_from<perform_epoch_maintenance_request>(started);
        static constexpr std::array<std::uint8_t, 32> expected_digest{
            0x95U,
            0xcdU,
            0xd8U,
            0x19U,
            0xd2U,
            0x58U,
            0x99U,
            0xfcU,
            0x28U,
            0x39U,
            0xa3U,
            0xfcU,
            0xf2U,
            0xacU,
            0x24U,
            0x1aU,
            0xdaU,
            0xcbU,
            0x4eU,
            0xbeU,
            0xd4U,
            0x45U,
            0x3eU,
            0xd3U,
            0x9eU,
            0x55U,
            0x22U,
            0xdaU,
            0xc4U,
            0xa0U,
            0x28U,
            0xa5U,
        };
        expect(request
                && request->Context.Kind == epoch_maintenance_kind::prune_epochs
                && request->Context.Cache == fixture.Authority.Cache
                && request->Context.Epochs
                    == std::vector{fixture.Epoch.Id, second}
                && request->Context.TargetDigest.valid()
                && request->Context.TargetDigest.bytes() == expected_digest
                && fixture.State.PendingMaintenance
                && fixture.State.PendingMaintenance->EpochCount == 2U
                && fixture.State.Status == epoch_status::maintaining
                && !fixture.State.PendingComparison
                && has_changed(started.Changed,
                    epoch_changed_domain::maintenance),
            "one prune intent binds a bounded exact batch and retires stale "
            "comparison work");

        const auto pending = fixture.State;
        const auto duplicate = transition(fixture.State,
            prune_review_epochs_intent{
                .Catalog = fixture.Catalog,
                .Epochs = {fixture.Epoch.Id},
            });
        expect(
            diagnostic_is(duplicate, epoch_reason_code::duplicate_maintenance)
                && fixture.State == pending,
            "only one maintenance action may be pending");

        auto tampered = request->Context;
        tampered.Epochs.back().Ordinal = epoch_ordinal{3};
        const auto mismatch = transition(fixture.State,
            epoch_maintenance_completed_event{
                .Context = tampered,
                .NewCatalog = catalog(2),
            });
        expect(diagnostic_is(mismatch,
                   epoch_reason_code::completion_identity_mismatch)
                && fixture.State == pending,
            "a terminal cannot alter a digest-bound prune target");

        const auto cancelling =
            transition(fixture.State, cancel_epoch_maintenance_intent{});
        expect(request_from<cancel_epoch_work_request>(cancelling) != nullptr
                && fixture.State.PendingMaintenance->CancellationRequested,
            "maintenance cancellation is an explicit request");
        const auto after_point = transition(fixture.State,
            epoch_maintenance_cancelled_event{
                .Context = request->Context,
                .Progress =
                    epoch_maintenance_progress::after_point_of_no_return,
            });
        expect(diagnostic_is(after_point,
                   epoch_reason_code::cancellation_after_point_of_no_return)
                && fixture.State.PendingMaintenance,
            "post-point-of-no-return cancellation cannot terminate "
            "maintenance");

        const auto completed = transition(fixture.State,
            epoch_maintenance_completed_event{
                .Context = request->Context,
                .NewCatalog = catalog(2),
            });
        expect(!fixture.State.PendingMaintenance
                && fixture.State.Catalog == std::optional{catalog(2)}
                && completed.Catalog
                && completed.Catalog->AvailabilityChangedEpochs
                    == std::vector{fixture.Epoch.Id, second}
                && !completed.Catalog->AllEpochAvailability
                && request_from<resolve_comparison_request>(completed)
                    != nullptr,
            "durable prune success wins after the point of no return and "
            "incrementally marks exact stable IDs");

        const auto stale = transition(fixture.State,
            epoch_maintenance_completed_event{
                .Context = request->Context,
                .NewCatalog = catalog(2),
            });
        expect(diagnostic_is(stale, epoch_reason_code::stale_completion),
            "a duplicate maintenance terminal is inert");
    }

    {
        auto fixture = make_ready_fixture();
        const auto started = transition(fixture.State,
            delete_repository_cache_intent{
                .Catalog = fixture.Catalog,
                .Cache = fixture.Authority.Cache,
            });
        const auto request =
            *request_from<perform_epoch_maintenance_request>(started);
        expect(request.Context.Kind
                    == epoch_maintenance_kind::delete_repository_cache
                && request.Context.Epochs.empty()
                && request.Context.TargetDigest.valid(),
            "cache deletion binds the exact active repository cache");

        const auto completed = transition(fixture.State,
            epoch_maintenance_completed_event{
                .Context = request.Context,
                .NewCatalog = catalog(2),
            });
        expect(completed.Catalog
                && completed.Catalog->AllEpochAvailability
                    == anchor_availability::cache_deleted
                && completed.Catalog->AvailabilityChangedEpochs.empty(),
            "whole-cache success publishes one bulk cache-deleted availability "
            "delta");
    }

    {
        auto fixture = make_ready_fixture();
        const auto started = transition(fixture.State,
            delete_repository_cache_intent{
                .Catalog = fixture.Catalog,
                .Cache = fixture.Authority.Cache,
            });
        const auto request =
            *request_from<perform_epoch_maintenance_request>(started);
        static_cast<void>(
            transition(fixture.State, cancel_epoch_maintenance_intent{}));
        const auto cancelled = transition(fixture.State,
            epoch_maintenance_cancelled_event{
                .Context = request.Context,
            });
        expect(diagnostic_is(cancelled, epoch_reason_code::cancelled)
                && !fixture.State.PendingMaintenance
                && fixture.State.Catalog == std::optional{fixture.Catalog}
                && fixture.State.Status == epoch_status::cancelled,
            "pre-point-of-no-return cancellation preserves the catalog");
    }

    {
        auto fixture = make_ready_fixture();
        const auto started = transition(fixture.State,
            prune_review_epochs_intent{
                .Catalog = fixture.Catalog,
                .Epochs = {fixture.Epoch.Id},
            });
        const auto request =
            *request_from<perform_epoch_maintenance_request>(started);
        const auto refreshed =
            transition(fixture.State, refresh_epochs_intent{});
        expect(request_from<cancel_epoch_work_request>(refreshed) != nullptr
                && !fixture.State.PendingMaintenance,
            "refresh retires maintenance from the superseded generation");
        const auto stale = transition(fixture.State,
            epoch_maintenance_completed_event{
                .Context = request.Context,
                .NewCatalog = catalog(2),
            });
        expect(diagnostic_is(stale, epoch_reason_code::stale_completion),
            "maintenance completion from a retired generation cannot publish");
    }
}

auto test_deterministic_replay() -> void
{
    const auto first = make_ready_fixture();
    const auto second = make_ready_fixture();
    expect(
        first == second,
        "same bounded observations replay to byte-equivalent semantic state");
}
} // namespace

auto main() -> int
{
    test_binary_oid_types_and_compact_state();
    test_preparation_effect_policy();
    test_cause_precedence_and_material_policy();
    test_retention_rehydration_policy();
    test_association_only_policy();
    test_alias_resolution_and_exact_pairs();
    test_prepare_commit_visibility_and_linearization();
    test_commit_payload_binding_and_cancellation_point();
    test_catalog_conflict_reloads_catalog();
    test_current_malformed_terminals_close_work();
    test_noop_discard_and_exact_invalidation();
    test_association_only_commit_transition();
    test_fail_closed_comparison_exhaustion();
    test_invalid_cross_facts_and_command_policy();
    test_selection_transition_policy();
    test_overflow_is_checked_and_invalidation_fails_closed();
    test_bounded_epoch_history_paging();
    test_core_authorized_epoch_maintenance();
    test_deterministic_replay();
    return EXIT_SUCCESS;
}
