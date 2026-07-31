#include "bpr/application/github_capability_cache.hpp"

#include <algorithm>
#include <initializer_list>
#include <limits>
#include <new>
#include <optional>
#include <utility>

namespace bpr::application
{
namespace
{
[[nodiscard]] constexpr auto conclusive(
    const core::github_capability_state state) noexcept -> bool
{
    return state == core::github_capability_state::supported
        || state == core::github_capability_state::unsupported;
}

[[nodiscard]] auto valid_facts(
    const std::array<
        github_capability_fact,
        github_capability_fact_count>& facts) noexcept -> bool
{
    for(std::size_t index = 0U; index < facts.size(); ++index)
    {
        if(
            facts[index].Capability != canonical_github_capabilities[index]
            || !conclusive(facts[index].State)
            || !valid(facts[index].Source))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] auto valid_schema_facts(
    const std::array<
        github_schema_capability_fact,
        github_schema_capability_fact_count>& facts) noexcept -> bool
{
    for(std::size_t index = 0U; index < facts.size(); ++index)
    {
        if(
            facts[index].Capability
                != canonical_github_schema_capabilities[index]
            || !conclusive(facts[index].State)
            || !valid(facts[index].Source))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] constexpr auto combined_state(
    const std::initializer_list<core::github_capability_state> states)
    noexcept -> core::github_capability_state
{
    for(const auto state : states)
    {
        if(state != core::github_capability_state::supported)
        {
            return core::github_capability_state::unsupported;
        }
    }
    return core::github_capability_state::supported;
}

[[nodiscard]] auto schema_capabilities(
    const std::array<
        github_schema_capability_fact,
        github_schema_capability_fact_count>& facts) noexcept
    -> core::github_capabilities
{
    return {
        .ReviewThreads = facts[0U].State,
        .ViewedFileMutations = combined_state(
            {facts[1U].State, facts[2U].State, facts[3U].State}),
        .ResolveThread = combined_state(
            {facts[0U].State, facts[4U].State, facts[5U].State}),
        .UnresolveThread = combined_state(
            {facts[0U].State, facts[6U].State, facts[7U].State}),
    };
}

[[nodiscard]] auto lookup_rejected(
    const github_capability_cache_error error) noexcept
    -> github_capability_cache_lookup_result
{
    return {
        .Status = github_capability_cache_lookup_status::rejected,
        .Error = error,
        .Entry = std::nullopt,
    };
}

[[nodiscard]] auto store_rejected(
    const github_capability_cache_error error) noexcept
    -> github_capability_cache_store_result
{
    return {
        .Disposition =
            github_capability_cache_store_disposition::rejected,
        .Error = error,
        .Entry = std::nullopt,
    };
}

[[nodiscard]] auto stage_rejected(
    const github_capability_cache_error error) noexcept
    -> github_capability_cache_stage_result
{
    return {
        .Error = error,
        .Stage = std::nullopt,
    };
}

void advance_mutation_generation(std::uint64_t& generation) noexcept
{
    if(generation != std::numeric_limits<std::uint64_t>::max())
    {
        ++generation;
    }
}

struct cache_storage_slot
{
    std::vector<github_capability_cache_entry>::iterator Reusable;
    github_capability_cache_store_disposition Disposition =
        github_capability_cache_store_disposition::inserted;
};

[[nodiscard]] auto select_storage_slot(
    std::vector<github_capability_cache_entry>& entries,
    const github_capability_authority_key& authority,
    const github_freshness_epoch observed_at,
    const std::size_t capacity) noexcept
    -> std::expected<
        cache_storage_slot,
        github_capability_cache_error>
{
    const auto same_authority = std::ranges::find(
        entries,
        authority,
        &github_capability_cache_entry::Authority);
    if(same_authority != entries.end())
    {
        return cache_storage_slot{
            .Reusable = same_authority,
            .Disposition =
                github_capability_cache_store_disposition::replaced,
        };
    }
    if(entries.size() != capacity)
    {
        return cache_storage_slot{
            .Reusable = entries.end(),
            .Disposition =
                github_capability_cache_store_disposition::inserted,
        };
    }

    const auto expired = std::ranges::find_if(
        entries,
        [observed_at](const auto& entry) {
            return observed_at >= entry.ExpiresBefore;
        });
    if(expired == entries.end())
    {
        return std::unexpected{
            github_capability_cache_error::capacity_exhausted};
    }
    return cache_storage_slot{
        .Reusable = expired,
        .Disposition =
            github_capability_cache_store_disposition::reused_expired,
    };
}
}

auto all_unknown(const core::github_capabilities& capabilities) noexcept
    -> bool
{
    return capabilities.ReviewThreads
            == core::github_capability_state::unknown
        && capabilities.ViewedFileMutations
            == core::github_capability_state::unknown
        && capabilities.ResolveThread
            == core::github_capability_state::unknown
        && capabilities.UnresolveThread
            == core::github_capability_state::unknown;
}

auto valid(
    const github_capability_probe_observation& observation) noexcept -> bool
{
    return valid(observation.Authority)
        && observation.ObservedAt.value() != 0U
        && valid_facts(observation.Facts)
        && valid_schema_facts(observation.SchemaFacts)
        && projected_capabilities(observation)
            == core::github_capabilities{
                .ReviewThreads = observation.Facts[0U].State,
                .ViewedFileMutations = observation.Facts[1U].State,
                .ResolveThread = observation.Facts[2U].State,
                .UnresolveThread = observation.Facts[3U].State,
            };
}

auto valid(
    const github_capability_probe_unavailable& unavailable) noexcept -> bool
{
    return valid(unavailable.Authority)
        && unavailable.ObservedAt.value() != 0U
        && all_unknown(unavailable.Capabilities)
        && valid(unavailable.Reason);
}

auto projected_capabilities(
    const github_capability_probe_observation& observation) noexcept
    -> core::github_capabilities
{
    if(!valid_schema_facts(observation.SchemaFacts))
    {
        return {};
    }

    return schema_capabilities(observation.SchemaFacts);
}

auto projected_read_shape(
    const github_capability_probe_observation& observation) noexcept
    -> github_read_shape
{
    if(!valid_schema_facts(observation.SchemaFacts))
    {
        return {};
    }

    return {
        .ReviewThreads = observation.SchemaFacts[0U].State
            == core::github_capability_state::supported,
        .ViewedFileState = observation.SchemaFacts[1U].State
            == core::github_capability_state::supported,
        .ResolvePermission = observation.SchemaFacts[4U].State
            == core::github_capability_state::supported,
        .UnresolvePermission = observation.SchemaFacts[6U].State
            == core::github_capability_state::supported,
    };
}

github_capability_cache::github_capability_cache(
    github_capability_cache_policy policy) noexcept
    : Policy{policy}
    , NextRevision{policy.FirstRevision.value()}
{
}

github_capability_cache::github_capability_cache(
    github_capability_cache&& other) noexcept
    : Policy{other.Policy}
    , Entries{std::move(other.Entries)}
    , LastEpoch{other.LastEpoch}
    , NextRevision{other.NextRevision}
    , MutationGeneration{other.MutationGeneration}
    , RevisionExhausted{other.RevisionExhausted}
    , Usable{other.Usable}
{
    other.LastEpoch = {};
    other.NextRevision = 0U;
    other.MutationGeneration = 0U;
    other.RevisionExhausted = true;
    other.Usable = false;
}

auto github_capability_cache::create(
    github_capability_cache_policy policy) noexcept
    -> std::expected<
        github_capability_cache,
        github_capability_cache_error>
{
    if(!valid(policy))
    {
        return std::unexpected{
            github_capability_cache_error::invalid_policy};
    }

    github_capability_cache cache{policy};
    try
    {
        cache.Entries.reserve(policy.Capacity);
    }
    catch(const std::bad_alloc&)
    {
        return std::unexpected{
            github_capability_cache_error::allocation_failure};
    }
    return cache;
}

auto github_capability_cache::lookup(
    const github_capability_authority_key& authority,
    const github_freshness_epoch now) noexcept
    -> github_capability_cache_lookup_result
{
    if(!Usable)
    {
        return lookup_rejected(
            github_capability_cache_error::invalid_cache);
    }
    if(!valid(authority))
    {
        return lookup_rejected(
            github_capability_cache_error::invalid_authority);
    }
    if(now.value() == 0U)
    {
        return lookup_rejected(
            github_capability_cache_error::invalid_epoch);
    }
    if(now < LastEpoch)
    {
        return lookup_rejected(
            github_capability_cache_error::epoch_regression);
    }

    const auto found = std::ranges::find(
        Entries,
        authority,
        &github_capability_cache_entry::Authority);
    LastEpoch = now;
    if(found == Entries.end())
    {
        return {
            .Status = github_capability_cache_lookup_status::miss,
            .Error = github_capability_cache_error::none,
            .Entry = std::nullopt,
        };
    }
    if(now >= found->ExpiresBefore)
    {
        return {
            .Status = github_capability_cache_lookup_status::expired,
            .Error = github_capability_cache_error::none,
            .Entry = std::nullopt,
        };
    }
    return {
        .Status = github_capability_cache_lookup_status::hit,
        .Entry = *found,
    };
}

auto github_capability_cache::store(
    const github_capability_probe_result& result) noexcept
    -> github_capability_cache_store_result
{
    const auto staged = stage(result);
    if(!staged.staged())
    {
        return store_rejected(staged.Error);
    }
    return commit(*staged.Stage);
}

auto github_capability_cache::stage(
    const github_capability_probe_result& result) noexcept
    -> github_capability_cache_stage_result
{
    if(!Usable)
    {
        return stage_rejected(
            github_capability_cache_error::invalid_cache);
    }
    if(result.valueless_by_exception())
    {
        return stage_rejected(
            github_capability_cache_error::invalid_probe);
    }
    if(const auto* unavailable =
           std::get_if<github_capability_probe_unavailable>(&result);
       unavailable != nullptr)
    {
        if(!valid(unavailable->Authority))
        {
            return stage_rejected(
                github_capability_cache_error::invalid_authority);
        }
        if(unavailable->ObservedAt.value() == 0U)
        {
            return stage_rejected(
                github_capability_cache_error::invalid_epoch);
        }
        if(unavailable->ObservedAt < LastEpoch)
        {
            return stage_rejected(
                github_capability_cache_error::epoch_regression);
        }
        if(!valid(*unavailable))
        {
            return stage_rejected(
                github_capability_cache_error::invalid_probe);
        }
        return stage_rejected(
            github_capability_cache_error::probe_unavailable);
    }

    const auto& observation =
        std::get<github_capability_probe_observation>(result);
    if(!valid(observation.Authority))
    {
        return stage_rejected(
            github_capability_cache_error::invalid_authority);
    }
    if(observation.ObservedAt.value() == 0U)
    {
        return stage_rejected(
            github_capability_cache_error::invalid_epoch);
    }
    if(observation.ObservedAt < LastEpoch)
    {
        return stage_rejected(
            github_capability_cache_error::epoch_regression);
    }
    if(!valid(observation))
    {
        return stage_rejected(
            github_capability_cache_error::invalid_probe);
    }
    if(
        observation.ObservedAt.value()
        > std::numeric_limits<std::uint64_t>::max()
            - Policy.FreshnessWindowEpochs)
    {
        return stage_rejected(
            github_capability_cache_error::epoch_overflow);
    }
    if(RevisionExhausted)
    {
        return stage_rejected(
            github_capability_cache_error::revision_overflow);
    }

    const auto slot = select_storage_slot(
        Entries,
        observation.Authority,
        observation.ObservedAt,
        Policy.Capacity);
    if(!slot.has_value())
    {
        return stage_rejected(slot.error());
    }

    const auto expires_before = github_freshness_epoch{
        observation.ObservedAt.value()
        + Policy.FreshnessWindowEpochs};
    const auto revision = core::capability_revision{NextRevision};
    const github_capability_cache_entry entry{
        .Authority = observation.Authority,
        .Facts = observation.Facts,
        .SchemaFacts = observation.SchemaFacts,
        .Capabilities = projected_capabilities(observation),
        .ReadShape = projected_read_shape(observation),
        .Revision = revision,
        .ObservedAt = observation.ObservedAt,
        .ExpiresBefore = expires_before,
    };

    return {
        .Error = github_capability_cache_error::none,
        .Stage = github_capability_cache_stage{
            entry,
            slot->Disposition,
            MutationGeneration},
    };
}

auto github_capability_cache::commit(
    const github_capability_cache_stage& stage) noexcept
    -> github_capability_cache_store_result
{
    if(!Usable)
    {
        return store_rejected(
            github_capability_cache_error::invalid_cache);
    }
    if(
        stage.MutationGeneration != MutationGeneration
        || stage.Entry.Revision.value() != NextRevision)
    {
        return store_rejected(
            github_capability_cache_error::stale_stage);
    }
    if(stage.Entry.ObservedAt < LastEpoch)
    {
        return store_rejected(
            github_capability_cache_error::epoch_regression);
    }
    if(
        MutationGeneration
            == std::numeric_limits<std::uint64_t>::max()
        || RevisionExhausted)
    {
        return store_rejected(
            github_capability_cache_error::revision_overflow);
    }

    const github_capability_probe_observation observation{
        .Authority = stage.Entry.Authority,
        .ObservedAt = stage.Entry.ObservedAt,
        .Facts = stage.Entry.Facts,
        .SchemaFacts = stage.Entry.SchemaFacts,
    };
    if(
        !valid(observation)
        || stage.Entry.Capabilities
            != projected_capabilities(observation)
        || stage.Entry.ReadShape != projected_read_shape(observation)
        || stage.Entry.ObservedAt.value()
            > std::numeric_limits<std::uint64_t>::max()
                - Policy.FreshnessWindowEpochs
        || stage.Entry.ExpiresBefore
            != github_freshness_epoch{
                stage.Entry.ObservedAt.value()
                + Policy.FreshnessWindowEpochs})
    {
        return store_rejected(
            github_capability_cache_error::invalid_probe);
    }

    const auto slot = select_storage_slot(
        Entries,
        stage.Entry.Authority,
        stage.Entry.ObservedAt,
        Policy.Capacity);
    if(!slot.has_value())
    {
        return store_rejected(slot.error());
    }
    if(slot->Disposition != stage.Disposition)
    {
        return store_rejected(
            github_capability_cache_error::stale_stage);
    }

    if(slot->Reusable == Entries.end())
    {
        try
        {
            Entries.push_back(stage.Entry);
        }
        catch(const std::bad_alloc&)
        {
            return store_rejected(
                github_capability_cache_error::allocation_failure);
        }
    }
    else
    {
        *slot->Reusable = stage.Entry;
    }

    if(NextRevision == std::numeric_limits<std::uint64_t>::max())
    {
        RevisionExhausted = true;
    }
    else
    {
        ++NextRevision;
    }
    LastEpoch = stage.Entry.ObservedAt;
    advance_mutation_generation(MutationGeneration);

    return {
        .Disposition = slot->Disposition,
        .Entry = stage.Entry,
    };
}

auto github_capability_cache::invalidate_exact(
    const github_capability_authority_key& authority) noexcept
    -> github_capability_cache_invalidation_result
{
    if(!Usable)
    {
        return {
            .Error = github_capability_cache_error::invalid_cache,
        };
    }
    if(!valid(authority))
    {
        return {
            .Error =
                github_capability_cache_error::invalid_authority,
        };
    }

    const auto old_size = Entries.size();
    std::erase_if(
        Entries,
        [&authority](const auto& entry) {
            return entry.Authority == authority;
        });
    const auto removed = old_size - Entries.size();
    advance_mutation_generation(MutationGeneration);
    return {.Removed = removed};
}

auto github_capability_cache::invalidate_profile(
    const core::host_profile_id profile) noexcept
    -> github_capability_cache_invalidation_result
{
    if(!Usable)
    {
        return {
            .Error = github_capability_cache_error::invalid_cache,
        };
    }
    if(profile.value() == 0U)
    {
        return {
            .Error =
                github_capability_cache_error::invalid_authority,
        };
    }

    const auto old_size = Entries.size();
    std::erase_if(
        Entries,
        [profile](const auto& entry) {
            return entry.Authority.Profile == profile;
        });
    const auto removed = old_size - Entries.size();
    advance_mutation_generation(MutationGeneration);
    return {.Removed = removed};
}

auto github_capability_cache::invalidate_identity(
    const core::host_profile_id profile,
    const core::git_host_id host,
    const core::github_account_id account) noexcept
    -> github_capability_cache_invalidation_result
{
    if(!Usable)
    {
        return {
            .Error = github_capability_cache_error::invalid_cache,
        };
    }
    if(
        profile.value() == 0U || host.value() == 0U
        || account.value() == 0U)
    {
        return {
            .Error =
                github_capability_cache_error::invalid_authority,
        };
    }

    const auto old_size = Entries.size();
    std::erase_if(
        Entries,
        [profile, host, account](const auto& entry) {
            return entry.Authority.Profile == profile
                && entry.Authority.Host == host
                && entry.Authority.Account == account;
        });
    const auto removed = old_size - Entries.size();
    advance_mutation_generation(MutationGeneration);
    return {.Removed = removed};
}

auto github_capability_cache::clear() noexcept -> std::size_t
{
    if(!Usable)
    {
        return 0U;
    }
    const auto old_size = Entries.size();
    Entries.clear();
    advance_mutation_generation(MutationGeneration);
    return old_size;
}

auto github_capability_cache::size() const noexcept -> std::size_t
{
    return Usable ? Entries.size() : 0U;
}

auto github_capability_cache::capacity() const noexcept -> std::size_t
{
    return Usable ? Policy.Capacity : 0U;
}

auto github_capability_cache::last_epoch() const noexcept
    -> github_freshness_epoch
{
    return Usable ? LastEpoch : github_freshness_epoch{};
}
}
