#pragma once

#include "bpr/application/git_smart_transport_port.hpp"
#include "bpr/application/github_host_authority.hpp"
#include "bpr/core/epoch_model.hpp"

#include <compare>
#include <memory>
#include <optional>

namespace bpr::application
{
struct git_smart_authority_binding
{
    core::application_session_id ApplicationSession;
    core::host_profile_id Profile;
    core::github_profile_revision ProfileRevision;
    core::git_host_id Host;
    core::host_authority_fingerprint HostAuthority;
    github_canonical_hostname Hostname;
    github_server_contract ServerContract =
        github_server_contract::github_com_2022_11_28;
    core::github_account_id Account;
    core::repository_id Repository;
    core::repository_locator Locator;
    core::github_identity_generation IdentityGeneration;
    core::git_hash_algorithm ObjectFormat =
        core::git_hash_algorithm::invalid;

    auto operator<=>(const git_smart_authority_binding&) const = default;
};

[[nodiscard]] inline auto valid(
    const git_smart_authority_binding& binding) noexcept -> bool
{
    return binding.ApplicationSession.value() != 0U
        && binding.Profile.value() != 0U
        && binding.ProfileRevision.value() != 0U
        && binding.Host.value() != 0U && binding.HostAuthority.valid()
        && valid_github_authority_hostname(
            binding.ServerContract, binding.Hostname)
        && binding.Account.value() != 0U
        && binding.Repository.value() != 0U
        && core::valid(binding.Locator)
        && binding.IdentityGeneration.value() != 0U
        && (
            binding.ObjectFormat == core::git_hash_algorithm::sha1
            || binding.ObjectFormat == core::git_hash_algorithm::sha256);
}

class git_smart_authority_lease
{
public:
    git_smart_authority_lease() = default;
    virtual ~git_smart_authority_lease() = default;

    git_smart_authority_lease(const git_smart_authority_lease&) = delete;
    auto operator=(const git_smart_authority_lease&)
        -> git_smart_authority_lease& = delete;
    git_smart_authority_lease(git_smart_authority_lease&&) = delete;
    auto operator=(git_smart_authority_lease&&)
        -> git_smart_authority_lease& = delete;

    [[nodiscard]] virtual auto binding() const noexcept
        -> const git_smart_authority_binding& = 0;
    [[nodiscard]] virtual auto acquire(
        const git_smart_request_context& context)
        -> git_smart_transport_result<git_smart_transport> = 0;
    virtual void revoke() noexcept = 0;
    [[nodiscard]] virtual auto revoked() const noexcept -> bool = 0;
};

using git_smart_authority = std::unique_ptr<git_smart_authority_lease>;

// This is the sole accepted GitHub-to-epoch revision handoff. It intentionally
// has no pull_request_source_revision, accepted_pull_request_revision, Source,
// or Observation member. The epoch host mints the source observation only
// after accepting this bounded, provider-free record.
struct accepted_pull_request_handoff
{
    core::application_session_id ApplicationSession;
    core::host_profile_id Profile;
    core::github_profile_revision ProfileRevision;
    core::git_host_id Host;
    core::host_authority_fingerprint HostAuthority;
    github_canonical_hostname Hostname;
    github_server_contract ServerContract =
        github_server_contract::github_com_2022_11_28;
    core::github_account_id Account;
    core::github_viewer_actor_id Viewer;
    core::repository_id Repository;
    core::repository_locator Locator;
    core::pull_request_id PullRequest;
    core::pull_request_number Number;
    core::provider_snapshot_id Snapshot;
    core::github_identity_generation IdentityGeneration;
    core::request_generation PullRequestGeneration;
    core::pull_request_revision_origin Origin =
        core::pull_request_revision_origin::live;
    core::git_hash_algorithm ObjectFormat =
        core::git_hash_algorithm::invalid;
    core::base_branch_id BaseBranch;
    git_base_branch_ref BaseRef;
    core::git_commit_oid Base;
    core::git_commit_oid Head;
    std::optional<core::current_viewer_review_candidate> CurrentViewerReview;
    // A missing lease is a valid authenticated observation with smart Git
    // unavailable. It never grants an alternate transport.
    git_smart_authority SmartGit;
};

[[nodiscard]] constexpr auto valid(
    const core::pull_request_revision_origin origin) noexcept -> bool
{
    switch(origin)
    {
    case core::pull_request_revision_origin::live:
    case core::pull_request_revision_origin::explicit_historical_import:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr auto valid(
    const core::github_review_state state) noexcept -> bool
{
    switch(state)
    {
    case core::github_review_state::pending:
    case core::github_review_state::approved:
    case core::github_review_state::changes_requested:
    case core::github_review_state::commented:
    case core::github_review_state::dismissed:
        return true;
    }
    return false;
}

[[nodiscard]] inline auto valid(
    const core::current_viewer_review_candidate& review,
    const core::github_viewer_actor_id viewer,
    const core::git_hash_algorithm object_format) noexcept -> bool
{
    return review.Review.value() != 0U && review.Actor == viewer
        && core::valid(review.Order) && valid(review.State)
        && review.State != core::github_review_state::pending
        && review.Commit.valid()
        && review.Commit.algorithm() == object_format;
}

[[nodiscard]] inline auto authority_binding(
    const accepted_pull_request_handoff& handoff)
    -> git_smart_authority_binding
{
    return {
        .ApplicationSession = handoff.ApplicationSession,
        .Profile = handoff.Profile,
        .ProfileRevision = handoff.ProfileRevision,
        .Host = handoff.Host,
        .HostAuthority = handoff.HostAuthority,
        .Hostname = handoff.Hostname,
        .ServerContract = handoff.ServerContract,
        .Account = handoff.Account,
        .Repository = handoff.Repository,
        .Locator = handoff.Locator,
        .IdentityGeneration = handoff.IdentityGeneration,
        .ObjectFormat = handoff.ObjectFormat,
    };
}

[[nodiscard]] inline auto agrees(
    const git_smart_authority_binding& binding,
    const accepted_pull_request_handoff& handoff) noexcept -> bool
{
    return binding.ApplicationSession == handoff.ApplicationSession
        && binding.Profile == handoff.Profile
        && binding.ProfileRevision == handoff.ProfileRevision
        && binding.Host == handoff.Host
        && binding.HostAuthority == handoff.HostAuthority
        && binding.Hostname == handoff.Hostname
        && binding.ServerContract == handoff.ServerContract
        && binding.Account == handoff.Account
        && binding.Repository == handoff.Repository
        && binding.Locator == handoff.Locator
        && binding.IdentityGeneration == handoff.IdentityGeneration
        && binding.ObjectFormat == handoff.ObjectFormat;
}

[[nodiscard]] inline auto valid(
    const accepted_pull_request_handoff& handoff) noexcept -> bool
{
    const auto valid_facts = handoff.ApplicationSession.value() != 0U
        && handoff.Profile.value() != 0U
        && handoff.ProfileRevision.value() != 0U
        && handoff.Host.value() != 0U && handoff.HostAuthority.valid()
        && valid_github_authority_hostname(
            handoff.ServerContract, handoff.Hostname)
        && handoff.Account.value() != 0U
        && handoff.Viewer.value() != 0U
        && handoff.Repository.value() != 0U
        && core::valid(handoff.Locator)
        && handoff.PullRequest.value() != 0U && handoff.Number.valid()
        && handoff.Snapshot.value() != 0U
        && handoff.IdentityGeneration.value() != 0U
        && handoff.PullRequestGeneration.value() != 0U
        && application::valid(handoff.Origin)
        && (
            handoff.ObjectFormat == core::git_hash_algorithm::sha1
            || handoff.ObjectFormat == core::git_hash_algorithm::sha256)
        && handoff.BaseBranch.valid() && application::valid(handoff.BaseRef)
        && handoff.Base.valid() && handoff.Head.valid()
        && handoff.Base.algorithm() == handoff.ObjectFormat
        && handoff.Head.algorithm() == handoff.ObjectFormat
        && (
            !handoff.CurrentViewerReview
            || application::valid(
                *handoff.CurrentViewerReview,
                handoff.Viewer,
                handoff.ObjectFormat));
    if(!valid_facts || handoff.SmartGit == nullptr)
    {
        return valid_facts;
    }

    const auto& binding = handoff.SmartGit->binding();
    return !handoff.SmartGit->revoked()
        && application::valid(binding) && agrees(binding, handoff);
}

struct pull_request_authority_invalidation
{
    core::application_session_id ApplicationSession;
    core::host_profile_id Profile;
    core::github_profile_revision ProfileRevision;
    core::git_host_id Host;
    core::host_authority_fingerprint HostAuthority;
    core::github_account_id Account;
    core::github_viewer_actor_id Viewer;
    core::repository_id Repository;
    core::pull_request_id PullRequest;
    core::github_identity_generation IdentityGeneration;

    auto operator<=>(const pull_request_authority_invalidation&) const =
        default;
};

[[nodiscard]] constexpr auto valid(
    const pull_request_authority_invalidation& invalidation) noexcept -> bool
{
    return invalidation.ApplicationSession.value() != 0U
        && invalidation.Profile.value() != 0U
        && invalidation.ProfileRevision.value() != 0U
        && invalidation.Host.value() != 0U
        && invalidation.HostAuthority.valid()
        && invalidation.Account.value() != 0U
        && invalidation.Viewer.value() != 0U
        && invalidation.Repository.value() != 0U
        && invalidation.PullRequest.value() != 0U
        && invalidation.IdentityGeneration.value() != 0U;
}

class github_epoch_handoff_port
{
public:
    virtual ~github_epoch_handoff_port() = default;

    virtual void accept(accepted_pull_request_handoff handoff) = 0;
    virtual void invalidate(
        pull_request_authority_invalidation invalidation) = 0;
};
}
