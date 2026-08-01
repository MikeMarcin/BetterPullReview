#include "bpr/application/epoch_application_host.hpp"
#include "bpr/application/epoch_runtime_ports.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

namespace
{
using namespace bpr;

template<typename Type>
concept exposes_source = requires(Type value) { value.Source; };

template<typename Type>
concept exposes_observation = requires(Type value) { value.Observation; };

template<typename Type>
concept exposes_host_requests = requires(Type value) { value.Requests; };

static_assert(
    !exposes_source<application::accepted_pull_request_handoff>);
static_assert(
    !exposes_observation<application::accepted_pull_request_handoff>);
static_assert(
    !std::is_constructible_v<
        application::accepted_pull_request_handoff,
        core::accepted_pull_request_revision>);
static_assert(
    std::is_move_constructible_v<
        application::accepted_pull_request_handoff>);
static_assert(
    !std::is_copy_constructible_v<
        application::accepted_pull_request_handoff>);
static_assert(
    !std::is_copy_constructible_v<
        application::git_smart_authority_lease>);
static_assert(std::variant_size_v<application::epoch_intent> == 6U);
static_assert(
    !std::is_constructible_v<
        application::epoch_intent,
        core::pull_request_revision_accepted_event>);
static_assert(
    !std::is_constructible_v<
        application::epoch_intent,
        core::epoch_catalog_loaded_event>);
static_assert(
    !exposes_source<application::epoch_revision_projection>);
static_assert(
    !exposes_observation<application::epoch_revision_projection>);
static_assert(!exposes_source<application::epoch_row_projection>);
static_assert(!exposes_observation<application::epoch_row_projection>);
static_assert(
    !exposes_host_requests<application::epoch_transition_projection>);

class test_failure final
{
public:
    explicit test_failure(const std::string_view message) noexcept
        : Message{message}
    {
    }

    [[nodiscard]] auto message() const noexcept -> std::string_view
    {
        return Message;
    }

private:
    std::string_view Message;
};

void expect(const bool condition, const std::string_view message)
{
    if(!condition)
    {
        throw test_failure{message};
    }
}

template<typename Identity>
[[nodiscard]] constexpr auto identity(const std::uint8_t seed) -> Identity
{
    auto bytes = std::array<std::uint8_t, 32U>{};
    bytes.front() = seed;
    bytes.back() = static_cast<std::uint8_t>(seed ^ 0xA5U);
    return Identity{bytes};
}

template<typename Oid>
[[nodiscard]] constexpr auto oid(const std::uint8_t seed) -> Oid
{
    auto bytes = std::array<std::uint8_t, 32U>{};
    for(std::size_t index = 0U; index < 20U; ++index)
    {
        bytes[index] = static_cast<std::uint8_t>(seed + index);
    }
    return Oid{core::git_hash_algorithm::sha1, bytes};
}

class fake_smart_authority final
    : public application::git_smart_authority_lease
{
public:
    explicit fake_smart_authority(
        application::git_smart_authority_binding binding)
        : Binding{std::move(binding)}
    {
    }

    [[nodiscard]] auto binding() const noexcept
        -> const application::git_smart_authority_binding& override
    {
        return Binding;
    }

    [[nodiscard]] auto acquire(
        const application::git_smart_request_context&)
        -> application::git_smart_transport_result<
            application::git_smart_transport> override
    {
        return std::unexpected{application::git_smart_transport_error{
            .Failure =
                application::git_smart_transport_failure::unavailable}};
    }

    void revoke() noexcept override
    {
        Revoked = true;
    }

    [[nodiscard]] auto revoked() const noexcept -> bool override
    {
        return Revoked;
    }

private:
    application::git_smart_authority_binding Binding;
    bool Revoked = false;
};

[[nodiscard]] auto handoff()
    -> application::accepted_pull_request_handoff
{
    const auto base_ref =
        application::parse_git_base_branch_ref("refs/heads/main");
    expect(base_ref.has_value(), "fixture base ref is valid");

    return {
        .ApplicationSession = core::application_session_id{1U},
        .Profile = core::host_profile_id{2U},
        .ProfileRevision = core::github_profile_revision{3U},
        .Host = core::git_host_id{4U},
        .HostAuthority =
            identity<core::host_authority_fingerprint>(5U),
        .Hostname = application::github_canonical_hostname{"github.com"},
        .ServerContract = application::github_server_contract::
            github_com_2022_11_28,
        .Account = core::github_account_id{6U},
        .Viewer = core::github_viewer_actor_id{7U},
        .Repository = core::repository_id{8U},
        .Locator = {.Owner = "octo-org", .Name = "review-repository"},
        .PullRequest = core::pull_request_id{9U},
        .Number = core::pull_request_number{10U},
        .Snapshot = core::provider_snapshot_id{11U},
        .IdentityGeneration = core::github_identity_generation{12U},
        .PullRequestGeneration = core::request_generation{13U},
        .Origin = core::pull_request_revision_origin::live,
        .ObjectFormat = core::git_hash_algorithm::sha1,
        .BaseBranch = identity<core::base_branch_id>(14U),
        .BaseRef = *base_ref,
        .Base = oid<core::git_commit_oid>(20U),
        .Head = oid<core::git_commit_oid>(50U),
        .CurrentViewerReview =
            core::current_viewer_review_candidate{
                .Review = core::github_review_id{15U},
                .Actor = core::github_viewer_actor_id{7U},
                .Order = core::github_review_order{1U},
                .State = core::github_review_state::approved,
                .Commit = oid<core::git_commit_oid>(50U),
            },
    };
}

void source_free_handoff_is_exact()
{
    auto accepted = handoff();
    expect(application::valid(accepted), "source-free handoff is valid");

    auto binding = application::authority_binding(accepted);
    accepted.SmartGit =
        std::make_unique<fake_smart_authority>(binding);
    expect(
        application::valid(accepted),
        "matching live smart authority is valid");

    accepted.SmartGit->revoke();
    expect(
        !application::valid(accepted),
        "revoked smart authority invalidates the capability handoff");
}

void mismatched_facts_fail_closed()
{
    auto accepted = handoff();
    auto binding = application::authority_binding(accepted);
    binding.Repository = core::repository_id{99U};
    accepted.SmartGit =
        std::make_unique<fake_smart_authority>(std::move(binding));
    expect(
        !application::valid(accepted),
        "cross-repository smart authority is rejected");

    accepted = handoff();
    accepted.CurrentViewerReview->Actor =
        core::github_viewer_actor_id{99U};
    expect(
        !application::valid(accepted),
        "non-viewer review candidate is rejected");

    accepted = handoff();
    accepted.ObjectFormat = core::git_hash_algorithm::sha256;
    expect(
        !application::valid(accepted),
        "object-format and OID disagreement is rejected");

    accepted = handoff();
    accepted.Origin =
        static_cast<core::pull_request_revision_origin>(0xFFU);
    expect(
        !application::valid(accepted),
        "unknown revision origin is rejected");
}

void runtime_ports_are_typed_and_bounded()
{
    auto zero = application::epoch_entropy_bytes{};
    expect(!application::valid(zero), "zero entropy output is invalid");
    zero.front() = 1U;
    expect(application::valid(zero), "nonzero entropy output is valid");

    const auto invalidation =
        application::pull_request_authority_invalidation{
            .ApplicationSession = core::application_session_id{1U},
            .Profile = core::host_profile_id{2U},
            .ProfileRevision = core::github_profile_revision{3U},
            .Host = core::git_host_id{4U},
            .HostAuthority =
                identity<core::host_authority_fingerprint>(5U),
            .Account = core::github_account_id{6U},
            .Viewer = core::github_viewer_actor_id{7U},
            .Repository = core::repository_id{8U},
            .PullRequest = core::pull_request_id{9U},
            .IdentityGeneration = core::github_identity_generation{12U},
        };
    expect(
        application::valid(invalidation),
        "exact invalidation identity is valid");
}

void system_runtime_ports_observe_native_state()
{
    auto entropy = application::make_system_epoch_entropy();
    expect(entropy != nullptr, "system entropy port is constructed");

    const auto first = entropy->generate();
    const auto second = entropy->generate();
    expect(first.has_value(), "system CSPRNG produces a first sample");
    expect(second.has_value(), "system CSPRNG produces a second sample");
    expect(application::valid(*first), "first CSPRNG sample is nonzero");
    expect(application::valid(*second), "second CSPRNG sample is nonzero");
    expect(*first != *second, "successive CSPRNG samples are fresh");

    auto clock = application::make_system_epoch_clock();
    expect(clock != nullptr, "system clock port is constructed");
    const auto native_now =
        std::chrono::system_clock::now().time_since_epoch();
    const auto expected =
        std::chrono::duration_cast<std::chrono::milliseconds>(native_now)
            .count();
    const auto observed = clock->observe_utc();
    expect(observed.has_value(), "system UTC clock produces an observation");
    expect(core::valid(*observed), "system UTC observation is bounded");

    constexpr std::int64_t MaximumObservationSkew = 60'000;
    const auto difference = observed->value()
        - static_cast<std::int64_t>(expected);
    expect(
        difference >= -MaximumObservationSkew
            && difference <= MaximumObservationSkew,
        "system UTC observation agrees with the native system clock");
}
} // namespace

int main()
{
    try
    {
        source_free_handoff_is_exact();
        mismatched_facts_fail_closed();
        runtime_ports_are_typed_and_bounded();
        system_runtime_ports_observe_native_state();
    }
    catch(const test_failure& failure)
    {
        std::cerr << "FAIL: " << failure.message() << '\n';
        return EXIT_FAILURE;
    }
    catch(...)
    {
        std::cerr << "FAIL: unexpected exception\n";
        return EXIT_FAILURE;
    }

    std::cout << "epoch application contract tests passed\n";
    return EXIT_SUCCESS;
}
