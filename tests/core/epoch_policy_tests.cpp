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

[[nodiscard]] auto append_from(const planned_epoch_mutation& plan)
    -> const append_epoch_mutation*
{
    if(!plan.Mutation.has_value())
    {
        return nullptr;
    }
    return std::get_if<append_epoch_mutation>(&*plan.Mutation);
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
    static_assert(std::variant_size_v<epoch_event> == 19U);
    static_assert(std::variant_size_v<epoch_host_request> == 7U);

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
    test_cause_precedence_and_material_policy();
    test_association_only_policy();
    test_alias_resolution_and_exact_pairs();
    test_prepare_commit_visibility_and_linearization();
    test_commit_payload_binding_and_cancellation_point();
    test_current_malformed_terminals_close_work();
    test_noop_discard_and_exact_invalidation();
    test_association_only_commit_transition();
    test_fail_closed_comparison_exhaustion();
    test_invalid_cross_facts_and_command_policy();
    test_selection_transition_policy();
    test_overflow_is_checked_and_invalidation_fails_closed();
    test_deterministic_replay();
    return EXIT_SUCCESS;
}
