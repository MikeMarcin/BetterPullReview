#include "bpr/application/github_capability_cache.hpp"

#include <algorithm>
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
        && valid_facts(observation.Facts);
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
    if(!valid(observation))
    {
        return {};
    }

    return {
        .ReviewThreads = observation.Facts[0U].State,
        .ViewedFileMutations = observation.Facts[1U].State,
        .ResolveThread = observation.Facts[2U].State,
        .UnresolveThread = observation.Facts[3U].State,
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
    , RevisionExhausted{other.RevisionExhausted}
    , Usable{other.Usable}
{
    other.LastEpoch = {};
    other.NextRevision = 0U;
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
    if(!Usable)
    {
        return store_rejected(
            github_capability_cache_error::invalid_cache);
    }
    if(const auto* unavailable =
           std::get_if<github_capability_probe_unavailable>(&result);
       unavailable != nullptr)
    {
        if(!valid(unavailable->Authority))
        {
            return store_rejected(
                github_capability_cache_error::invalid_authority);
        }
        if(unavailable->ObservedAt.value() == 0U)
        {
            return store_rejected(
                github_capability_cache_error::invalid_epoch);
        }
        if(unavailable->ObservedAt < LastEpoch)
        {
            return store_rejected(
                github_capability_cache_error::epoch_regression);
        }
        if(!valid(*unavailable))
        {
            return store_rejected(
                github_capability_cache_error::invalid_probe);
        }
        return store_rejected(
            github_capability_cache_error::probe_unavailable);
    }

    const auto& observation =
        std::get<github_capability_probe_observation>(result);
    if(!valid(observation.Authority))
    {
        return store_rejected(
            github_capability_cache_error::invalid_authority);
    }
    if(observation.ObservedAt.value() == 0U)
    {
        return store_rejected(
            github_capability_cache_error::invalid_epoch);
    }
    if(observation.ObservedAt < LastEpoch)
    {
        return store_rejected(
            github_capability_cache_error::epoch_regression);
    }
    if(!valid_facts(observation.Facts))
    {
        return store_rejected(
            github_capability_cache_error::invalid_probe);
    }
    if(
        observation.ObservedAt.value()
        > std::numeric_limits<std::uint64_t>::max()
            - Policy.FreshnessWindowEpochs)
    {
        return store_rejected(
            github_capability_cache_error::epoch_overflow);
    }
    if(RevisionExhausted)
    {
        return store_rejected(
            github_capability_cache_error::revision_overflow);
    }

    const auto same_authority = std::ranges::find(
        Entries,
        observation.Authority,
        &github_capability_cache_entry::Authority);
    auto reusable = Entries.end();
    auto disposition =
        github_capability_cache_store_disposition::inserted;

    if(same_authority != Entries.end())
    {
        reusable = same_authority;
        disposition =
            github_capability_cache_store_disposition::replaced;
    }
    else if(Entries.size() == Policy.Capacity)
    {
        reusable = std::ranges::find_if(
            Entries,
            [&observation](const auto& entry) {
                return observation.ObservedAt >= entry.ExpiresBefore;
            });
        if(reusable == Entries.end())
        {
            return store_rejected(
                github_capability_cache_error::capacity_exhausted);
        }
        disposition =
            github_capability_cache_store_disposition::reused_expired;
    }

    const auto expires_before = github_freshness_epoch{
        observation.ObservedAt.value()
        + Policy.FreshnessWindowEpochs};
    const auto revision = core::capability_revision{NextRevision};
    const github_capability_cache_entry entry{
        .Authority = observation.Authority,
        .Facts = observation.Facts,
        .Capabilities = projected_capabilities(observation),
        .Revision = revision,
        .ObservedAt = observation.ObservedAt,
        .ExpiresBefore = expires_before,
    };

    if(reusable == Entries.end())
    {
        try
        {
            Entries.push_back(entry);
        }
        catch(const std::bad_alloc&)
        {
            return store_rejected(
                github_capability_cache_error::allocation_failure);
        }
    }
    else
    {
        *reusable = entry;
    }

    if(NextRevision == std::numeric_limits<std::uint64_t>::max())
    {
        RevisionExhausted = true;
    }
    else
    {
        ++NextRevision;
    }
    LastEpoch = observation.ObservedAt;

    return {
        .Disposition = disposition,
        .Entry = entry,
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
    return {.Removed = old_size - Entries.size()};
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
    return {.Removed = old_size - Entries.size()};
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
    return {.Removed = old_size - Entries.size()};
}

auto github_capability_cache::clear() noexcept -> std::size_t
{
    if(!Usable)
    {
        return 0U;
    }
    const auto old_size = Entries.size();
    Entries.clear();
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
