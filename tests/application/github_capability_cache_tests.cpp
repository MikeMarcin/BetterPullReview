#include "bpr/application/github_capability_cache.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>
#include <utility>

namespace
{
using namespace bpr;

auto expect(const bool condition, const std::string_view message) -> void
{
    if(!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

auto authority(
    const std::uint64_t profile = 1U,
    const std::uint64_t profile_revision = 1U,
    const std::uint64_t host = 2U,
    const std::uint64_t account = 3U,
    const application::github_server_contract server_contract =
        application::github_server_contract::
            github_com_2022_11_28,
    const std::uint64_t probe_revision = 1U)
    -> application::github_capability_authority_key
{
    return {
        .Profile = core::host_profile_id{profile},
        .ProfileRevision =
            application::github_profile_revision{profile_revision},
        .Host = core::git_host_id{host},
        .Account = core::github_account_id{account},
        .ServerContract = server_contract,
        .ProbeContractRevision =
            application::github_probe_contract_revision{
                probe_revision},
    };
}

auto conclusive_facts()
    -> std::array<
        application::github_capability_fact,
        application::github_capability_fact_count>
{
    using capability = core::github_capability;
    using source = application::github_capability_evidence_source;
    using state = core::github_capability_state;
    return {
        application::github_capability_fact{
            .Capability = capability::review_threads,
            .State = state::supported,
            .Source = source::recorded_contract,
        },
        application::github_capability_fact{
            .Capability = capability::viewed_file_mutations,
            .State = state::unsupported,
            .Source = source::observed_contract,
        },
        application::github_capability_fact{
            .Capability = capability::resolve_thread,
            .State = state::supported,
            .Source = source::observed_contract,
        },
        application::github_capability_fact{
            .Capability = capability::unresolve_thread,
            .State = state::unsupported,
            .Source = source::recorded_contract,
        },
    };
}

auto observation(
    application::github_capability_authority_key key,
    const std::uint64_t epoch)
    -> application::github_capability_probe_observation
{
    return {
        .Authority = key,
        .ObservedAt = application::github_freshness_epoch{epoch},
        .Facts = conclusive_facts(),
    };
}

auto unavailable(
    application::github_capability_authority_key key,
    const std::uint64_t epoch)
    -> application::github_capability_probe_unavailable
{
    return {
        .Authority = key,
        .ObservedAt = application::github_freshness_epoch{epoch},
        .Capabilities = {},
        .Reason =
            application::github_capability_unavailable_reason::network,
    };
}

auto make_cache(
    const application::github_capability_cache_policy policy)
    -> application::github_capability_cache
{
    auto result = application::github_capability_cache::create(policy);
    expect(result.has_value(), "cache policy creates a bounded cache");
    return std::move(*result);
}

auto store(
    application::github_capability_cache& cache,
    application::github_capability_probe_observation value)
    -> application::github_capability_cache_store_result
{
    return cache.store(
        application::github_capability_probe_result{std::move(value)});
}

auto store(
    application::github_capability_cache& cache,
    application::github_capability_probe_unavailable value)
    -> application::github_capability_cache_store_result
{
    return cache.store(
        application::github_capability_probe_result{std::move(value)});
}

auto test_policy_and_evidence_are_bounded() -> void
{
    using application::github_capability_cache;
    using application::github_capability_cache_error;
    using application::github_capability_cache_policy;

    expect(
        !github_capability_cache::create(
             github_capability_cache_policy{.Capacity = 0U})
             .has_value(),
        "zero cache capacity is rejected");
    expect(
        !github_capability_cache::create(
             github_capability_cache_policy{.Capacity = 65U})
             .has_value(),
        "capacity above the security ceiling is rejected");
    expect(
        !github_capability_cache::create(
             github_capability_cache_policy{
                 .FreshnessWindowEpochs = 0U})
             .has_value(),
        "zero freshness window is rejected");
    expect(
        !github_capability_cache::create(
             github_capability_cache_policy{
                 .FreshnessWindowEpochs = 901U})
             .has_value(),
        "freshness window above 900 epochs is rejected");
    expect(
        !github_capability_cache::create(
             github_capability_cache_policy{
                 .FirstRevision = core::capability_revision{}})
             .has_value(),
        "zero first capability revision is rejected");

    auto valid_observation = observation(authority(), 1U);
    expect(
        application::valid(valid_observation),
        "four canonical conclusive facts are valid");
    const auto projected =
        application::projected_capabilities(valid_observation);
    expect(
        projected.ReviewThreads
                == core::github_capability_state::supported
            && projected.ViewedFileMutations
                == core::github_capability_state::unsupported
            && projected.ResolveThread
                == core::github_capability_state::supported
            && projected.UnresolveThread
                == core::github_capability_state::unsupported,
        "canonical facts project without losing tri-state meaning");

    auto reordered = valid_observation;
    std::swap(reordered.Facts[0U], reordered.Facts[1U]);
    expect(
        !application::valid(reordered),
        "facts outside canonical core-enum order are rejected");

    auto inconclusive = valid_observation;
    inconclusive.Facts[2U].State =
        core::github_capability_state::unknown;
    expect(
        !application::valid(inconclusive),
        "a cacheable observation cannot contain Unknown");

    auto assumed = valid_observation;
    assumed.Facts[2U].Source =
        application::github_capability_evidence_source::uninitialized;
    expect(
        !application::valid(assumed),
        "support without recorded or observed evidence is rejected");

    auto invalid_state = valid_observation;
    invalid_state.Facts[2U].State =
        static_cast<core::github_capability_state>(255U);
    expect(
        !application::valid(invalid_state),
        "an unknown capability-state enumerator is rejected");

    auto invalid_source = valid_observation;
    invalid_source.Facts[2U].Source =
        static_cast<
            application::github_capability_evidence_source>(255U);
    expect(
        !application::valid(invalid_source),
        "an unknown evidence-source enumerator is rejected");

    auto whole_unavailable = unavailable(authority(), 2U);
    expect(
        application::valid(whole_unavailable)
            && application::all_unknown(
                whole_unavailable.Capabilities),
        "whole-probe unavailability is explicit all-Unknown");

    auto false_unsupported = whole_unavailable;
    false_unsupported.Capabilities.ReviewThreads =
        core::github_capability_state::unsupported;
    expect(
        !application::valid(false_unsupported),
        "unavailability cannot be mislabeled Unsupported");

    auto missing_reason = whole_unavailable;
    missing_reason.Reason =
        application::github_capability_unavailable_reason::none;
    expect(
        !application::valid(missing_reason),
        "whole-probe unavailability requires a bounded reason");

    auto cache = make_cache({});
    const auto unavailable_store = store(cache, whole_unavailable);
    expect(
        !unavailable_store.stored()
            && unavailable_store.Error
                == github_capability_cache_error::probe_unavailable
            && cache.size() == 0U
            && cache.last_epoch().value() == 0U,
        "a valid unavailable probe is never cached");

    const auto malformed_store = store(cache, false_unsupported);
    expect(
        malformed_store.Error
            == github_capability_cache_error::invalid_probe,
        "a malformed unavailable probe is rejected distinctly");
}

auto test_every_authority_dimension_is_exact() -> void
{
    using lookup_status =
        application::github_capability_cache_lookup_status;

    auto cache = make_cache({
        .Capacity = 1U,
        .FreshnessWindowEpochs = 20U,
    });
    const auto base = authority();
    const auto stored = store(cache, observation(base, 5U));
    expect(stored.stored(), "base authority is cached");

    const auto initial_hit =
        cache.lookup(base, application::github_freshness_epoch{5U});
    expect(
        initial_hit.hit() && initial_hit.Entry.has_value()
            && initial_hit.Entry->Revision
                == core::capability_revision{1U},
        "exact authority hits with its generated revision");

    const std::array changed_keys{
        authority(9U, 1U, 2U, 3U),
        authority(1U, 9U, 2U, 3U),
        authority(1U, 1U, 9U, 3U),
        authority(1U, 1U, 2U, 9U),
        authority(
            1U,
            1U,
            2U,
            3U,
            application::github_server_contract::
                ghes_3_18_2022_11_28),
        authority(1U, 1U, 2U, 3U,
                  application::github_server_contract::
                      github_com_2022_11_28,
                  9U),
    };
    for(const auto& changed : changed_keys)
    {
        const auto result =
            cache.lookup(
                changed,
                application::github_freshness_epoch{5U});
        expect(
            result.Status == lookup_status::miss
                && !result.Entry.has_value(),
            "changing one authority-key dimension produces a miss");
    }

    auto invalid = base;
    invalid.Account = {};
    const auto invalid_lookup =
        cache.lookup(invalid, application::github_freshness_epoch{6U});
    expect(
        invalid_lookup.Status == lookup_status::rejected
            && invalid_lookup.Error
                == application::github_capability_cache_error::
                    invalid_authority
            && cache.last_epoch().value() == 5U,
        "an invalid authority is rejected without advancing cache time");

    auto invalid_contract = base;
    invalid_contract.ServerContract =
        static_cast<application::github_server_contract>(255U);
    expect(
        cache
                .lookup(
                    invalid_contract,
                    application::github_freshness_epoch{6U})
                .Error
            == application::github_capability_cache_error::
                invalid_authority,
        "an unknown server-contract enumerator is rejected");

    const auto repeated_hit =
        cache.lookup(base, application::github_freshness_epoch{5U});
    expect(
        repeated_hit.hit()
            && repeated_hit.Entry->Revision
                == initial_hit.Entry->Revision,
        "cache hits reuse the stored capability revision");
}

auto test_freshness_and_epoch_failures_are_atomic() -> void
{
    using error = application::github_capability_cache_error;
    using status = application::github_capability_cache_lookup_status;

    auto cache = make_cache({
        .Capacity = 2U,
        .FreshnessWindowEpochs = 3U,
    });
    const auto first = authority();
    const auto stored = store(cache, observation(first, 10U));
    expect(
        stored.Entry.has_value()
            && stored.Entry->ObservedAt.value() == 10U
            && stored.Entry->ExpiresBefore.value() == 13U,
        "store records a half-open logical freshness interval");

    expect(
        cache.lookup(
                 first,
                 application::github_freshness_epoch{12U})
            .hit(),
        "entry is fresh immediately before ExpiresBefore");
    expect(
        cache.lookup(
                 first,
                 application::github_freshness_epoch{13U})
                .Status
            == status::expired,
        "entry expires exactly at ExpiresBefore");

    const auto regression =
        cache.lookup(
            first,
            application::github_freshness_epoch{12U});
    expect(
        regression.Status == status::rejected
            && regression.Error == error::epoch_regression
            && cache.last_epoch().value() == 13U,
        "epoch regression is rejected without changing cache time");

    const auto zero_epoch =
        cache.lookup(first, application::github_freshness_epoch{});
    expect(
        zero_epoch.Error == error::invalid_epoch
            && cache.last_epoch().value() == 13U,
        "zero epoch is rejected atomically");

    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    const auto overflow = store(
        cache,
        observation(
            authority(4U),
            maximum - 2U));
    expect(
        overflow.Error == error::epoch_overflow
            && cache.size() == 1U
            && cache.last_epoch().value() == 13U,
        "freshness addition overflow leaves all cache state unchanged");

    const auto second =
        store(cache, observation(authority(4U), 14U));
    expect(
        second.stored()
            && second.Entry->Revision
                == core::capability_revision{2U},
        "an overflow rejection does not consume a revision");
}

auto test_capacity_never_evicts_fresh_authority() -> void
{
    using disposition =
        application::github_capability_cache_store_disposition;
    using error = application::github_capability_cache_error;

    auto cache = make_cache({
        .Capacity = 2U,
        .FreshnessWindowEpochs = 5U,
    });
    const auto first = authority(1U);
    const auto second = authority(2U);
    const auto third = authority(3U);

    const auto first_store = store(cache, observation(first, 1U));
    const auto second_store = store(cache, observation(second, 2U));
    expect(
        first_store.Entry->Revision
                == core::capability_revision{1U}
            && second_store.Entry->Revision
                == core::capability_revision{2U}
            && cache.size() == 2U,
        "the bounded cache accepts exactly N distinct authorities");

    const auto full = store(cache, observation(third, 3U));
    expect(
        full.Error == error::capacity_exhausted
            && cache.size() == 2U
            && cache.last_epoch().value() == 2U,
        "N plus one fresh authority fails without mutation");

    const auto reprobe = store(cache, observation(first, 3U));
    expect(
        reprobe.Disposition == disposition::replaced
            && reprobe.Entry->Revision
                == core::capability_revision{3U}
            && reprobe.Entry->Capabilities
                == first_store.Entry->Capabilities,
        "same-state re-probe replaces in place and advances revision");
    expect(
        cache.lookup(
                 first,
                 application::github_freshness_epoch{3U})
                .Entry->Revision
            == core::capability_revision{3U},
        "a hit reuses the re-probe revision");

    const auto still_full = store(cache, observation(third, 6U));
    expect(
        still_full.Error == error::capacity_exhausted
            && cache.last_epoch().value() == 3U,
        "a failed later store cannot evict either fresh entry");

    const auto reused = store(cache, observation(third, 7U));
    expect(
        reused.Disposition == disposition::reused_expired
            && reused.Entry->Revision
                == core::capability_revision{4U}
            && cache.size() == 2U,
        "a proven-expired slot is reusable at its boundary");
    expect(
        cache.lookup(
                 first,
                 application::github_freshness_epoch{7U})
                .Entry->Revision
            == core::capability_revision{3U},
        "expired-slot reuse preserves the unrelated fresh authority");
    expect(
        cache.lookup(
                 second,
                 application::github_freshness_epoch{7U})
                .Status
            == application::github_capability_cache_lookup_status::miss,
        "only the expired authority was replaced");
}

auto test_invalid_probe_and_revision_overflow_are_atomic() -> void
{
    using error = application::github_capability_cache_error;

    auto cache = make_cache({
        .Capacity = 2U,
        .FreshnessWindowEpochs = 10U,
    });
    const auto first = authority();
    expect(
        store(cache, observation(first, 1U))
                .Entry->Revision
            == core::capability_revision{1U},
        "first conclusive store generates revision one");

    auto malformed = observation(first, 4U);
    malformed.Facts[0U].Capability =
        core::github_capability::unresolve_thread;
    const auto rejected = store(cache, malformed);
    expect(
        rejected.Error == error::invalid_probe
            && cache.last_epoch().value() == 1U
            && cache.size() == 1U,
        "malformed fact order cannot replace a cached entry");

    const auto after_rejection =
        store(cache, observation(authority(2U), 4U));
    expect(
        after_rejection.Entry->Revision
            == core::capability_revision{2U},
        "invalid evidence does not consume a capability revision");

    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    auto exhausted = make_cache({
        .Capacity = 2U,
        .FreshnessWindowEpochs = 10U,
        .FirstRevision = core::capability_revision{maximum},
    });
    const auto maximum_store =
        store(exhausted, observation(first, 1U));
    expect(
        maximum_store.stored()
            && maximum_store.Entry->Revision.value() == maximum,
        "the maximum nonzero capability revision is representable");

    const auto overflow =
        store(exhausted, observation(authority(2U), 2U));
    expect(
        overflow.Error == error::revision_overflow
            && exhausted.size() == 1U
            && exhausted.last_epoch().value() == 1U,
        "revision overflow cannot wrap or partially store");
    expect(
        exhausted
                .lookup(
                    first,
                    application::github_freshness_epoch{1U})
                .Entry->Revision.value()
            == maximum,
        "revision overflow preserves the last valid cached value");
}

auto test_invalidation_is_exact_and_isolated() -> void
{
    auto cache = make_cache({
        .Capacity = 6U,
        .FreshnessWindowEpochs = 100U,
    });
    const auto exact = authority(1U, 1U, 2U, 3U);
    const auto same_identity_other_revision =
        authority(1U, 2U, 2U, 3U);
    const auto other_account = authority(1U, 1U, 2U, 8U);
    const auto other_profile = authority(9U, 1U, 2U, 3U);

    static_cast<void>(store(cache, observation(exact, 1U)));
    static_cast<void>(
        store(cache, observation(same_identity_other_revision, 2U)));
    static_cast<void>(store(cache, observation(other_account, 3U)));
    static_cast<void>(store(cache, observation(other_profile, 4U)));
    expect(cache.size() == 4U, "four isolated authorities are present");

    const auto exact_result = cache.invalidate_exact(exact);
    expect(
        exact_result.accepted() && exact_result.Removed == 1U
            && cache.size() == 3U,
        "exact invalidation removes only the full key");

    auto invalid = exact;
    invalid.Profile = {};
    const auto invalid_result = cache.invalidate_exact(invalid);
    expect(
        !invalid_result.accepted() && invalid_result.Removed == 0U
            && cache.size() == 3U,
        "invalid exact-invalidation input changes nothing");

    const auto identity_result = cache.invalidate_identity(
        core::host_profile_id{1U},
        core::git_host_id{2U},
        core::github_account_id{3U});
    expect(
        identity_result.accepted() && identity_result.Removed == 1U
            && cache.size() == 2U,
        "identity invalidation crosses profile revisions only");
    expect(
        cache.lookup(
                 other_account,
                 application::github_freshness_epoch{4U})
            .hit(),
        "identity invalidation preserves another account");
    expect(
        cache.lookup(
                 other_profile,
                 application::github_freshness_epoch{4U})
            .hit(),
        "identity invalidation preserves another profile");

    const auto profile_result =
        cache.invalidate_profile(core::host_profile_id{1U});
    expect(
        profile_result.accepted() && profile_result.Removed == 1U
            && cache.size() == 1U,
        "profile invalidation removes all remaining profile identities");
    expect(
        cache.lookup(
                 other_profile,
                 application::github_freshness_epoch{4U})
            .hit(),
        "profile invalidation preserves unrelated profiles");

    const auto invalid_identity = cache.invalidate_identity(
        {},
        core::git_host_id{2U},
        core::github_account_id{3U});
    expect(
        !invalid_identity.accepted() && cache.size() == 1U,
        "invalid identity-invalidation input changes nothing");
    expect(
        cache.clear() == 1U && cache.size() == 0U,
        "clear reports the exact number removed");
}

auto test_move_and_result_helpers_fail_closed() -> void
{
    using application::github_capability_cache_error;
    using application::github_capability_cache_lookup_result;
    using application::github_capability_cache_lookup_status;
    using application::github_capability_cache_store_disposition;
    using application::github_capability_cache_store_result;

    auto source = make_cache({
        .Capacity = 2U,
        .FreshnessWindowEpochs = 10U,
    });
    const auto first =
        store(source, observation(authority(), 1U));
    expect(
        first.Entry->Revision == core::capability_revision{1U},
        "source cache allocates its first revision");

    auto destination = std::move(source);
    const auto second =
        store(destination, observation(authority(2U), 2U));
    expect(
        second.stored()
            && second.Entry->Revision
                == core::capability_revision{2U},
        "move transfers the one revision sequence");

    const auto moved_from_store =
        store(source, observation(authority(3U), 2U));
    expect(
        !moved_from_store.stored()
            && moved_from_store.Error
                == github_capability_cache_error::invalid_cache
            && source.size() == 0U && source.capacity() == 0U
            && source.last_epoch().value() == 0U,
        "moved-from cache cannot allocate duplicate revisions");
    expect(
        source
                .lookup(
                    authority(),
                    application::github_freshness_epoch{2U})
                .Error
            == github_capability_cache_error::invalid_cache,
        "moved-from cache lookup fails explicitly");

    github_capability_cache_store_result malformed_store{
        .Disposition =
            static_cast<
                github_capability_cache_store_disposition>(255U),
        .Entry = second.Entry,
    };
    expect(
        !malformed_store.stored(),
        "malformed store disposition cannot report success");
    malformed_store.Disposition =
        github_capability_cache_store_disposition::inserted;
    malformed_store.Error =
        github_capability_cache_error::invalid_probe;
    expect(
        !malformed_store.stored(),
        "store error cannot report success");
    malformed_store.Error = github_capability_cache_error::none;
    malformed_store.Entry.reset();
    expect(
        !malformed_store.stored(),
        "store success requires the stored entry");

    github_capability_cache_lookup_result malformed_lookup{
        .Status = github_capability_cache_lookup_status::hit,
        .Error = github_capability_cache_error::none,
        .Entry = std::nullopt,
    };
    expect(
        !malformed_lookup.hit(),
        "cache hit requires a returned entry");
    malformed_lookup.Entry = second.Entry;
    malformed_lookup.Error =
        github_capability_cache_error::invalid_probe;
    expect(
        !malformed_lookup.hit(),
        "cache hit with an error fails closed");
}
}

int main()
{
    test_policy_and_evidence_are_bounded();
    test_every_authority_dimension_is_exact();
    test_freshness_and_epoch_failures_are_atomic();
    test_capacity_never_evicts_fresh_authority();
    test_invalid_probe_and_revision_overflow_are_atomic();
    test_invalidation_is_exact_and_isolated();
    test_move_and_result_helpers_fail_closed();
    std::cout << "application github capability cache tests passed\n";
    return EXIT_SUCCESS;
}
