#pragma once

#include "bpr/application/github_capability_cache.hpp"
#include "bpr/application/github_credential_vault.hpp"
#include "bpr/application/github_epoch_handoff.hpp"
#include "bpr/application/github_projection.hpp"
#include "bpr/application/github_provider_port.hpp"
#include "bpr/application/port.hpp"
#include "bpr/core/github_transition.hpp"

#include <array>
#include <cstddef>
#include <memory>
#include <optional>

namespace bpr::application
{
inline constexpr std::size_t maximum_github_credential_bindings = 64U;

enum class github_credential_shutdown_policy
{
    retain,
    delete_retained_on_shutdown,
};

class github_credential_binding_catalog final
{
public:
    github_credential_binding_catalog() = default;
    ~github_credential_binding_catalog();

    github_credential_binding_catalog(
        const github_credential_binding_catalog&) = delete;
    auto operator=(const github_credential_binding_catalog&)
        -> github_credential_binding_catalog& = delete;
    github_credential_binding_catalog(github_credential_binding_catalog&&) =
        delete;
    auto operator=(github_credential_binding_catalog&&)
        -> github_credential_binding_catalog& = delete;

    [[nodiscard]] auto size() const noexcept -> std::size_t;

private:
    [[nodiscard]] auto find(core::host_profile_id profile) const noexcept
        -> const github_credential_authority_key*;
    [[nodiscard]] auto
    remember(github_credential_authority_key authority) noexcept -> bool;
    void forget(const github_credential_authority_key& authority) noexcept;

    std::array<std::optional<github_credential_authority_key>,
               maximum_github_credential_bindings>
        Entries{};

    friend class github_application_host;
};

class github_application_host final
{
public:
    // shutdown() closes ingress but does not wait for remote work. When
    // is_quiescent() is false, composition keeps this host and its referenced
    // ports, decision lane, and projections alive while draining callbacks.
    // delete_retained_on_shutdown makes an exact vault delete terminal part of
    // quiescence for the credential authority retained by this host.
    // Destruction alone cannot guarantee cleanup of an extracted vault store.
    github_application_host(
        core::github_state initial_state,
        github_profile_port& profiles,
        github_viewer_port& viewers,
        github_capability_port& capabilities,
        github_pull_request_port& pull_requests,
        github_credential_vault_port& credentials,
        github_credential_binding_catalog& credential_bindings,
        decision_executor& decision_lane,
        github_projection_port& projections,
        github_credential_shutdown_policy credential_shutdown_policy =
            github_credential_shutdown_policy::retain,
        github_epoch_handoff_port* epochs = nullptr);
    ~github_application_host();

    github_application_host(const github_application_host&) = delete;
    auto operator=(const github_application_host&)
        -> github_application_host& = delete;
    github_application_host(github_application_host&&) = delete;
    auto operator=(github_application_host&&)
        -> github_application_host& = delete;

    void dispatch(const core::github_event& event);
    void enroll_profile(core::host_profile_id profile,
                        github_credential_material credential);
    // Resume asks only for a profile. The host-owned trusted binding catalog,
    // populated from prior accepted /user observations, selects the exact
    // account authority. Public account and login values never select vault
    // entries.
    void resume_profile(core::host_profile_id profile);
    void shutdown();

    [[nodiscard]] auto state() const noexcept -> const core::github_state&;
    [[nodiscard]] auto accepted_session() const noexcept
        -> const std::optional<github_session_read_model>&;
    [[nodiscard]] auto accepted_snapshot() const noexcept
        -> const std::optional<github_pull_request_snapshot>&;
    [[nodiscard]] auto rate_limits() const noexcept
        -> const github_rate_limit_state&;
    [[nodiscard]] auto capability_cache_size() const noexcept -> std::size_t;
    [[nodiscard]] auto is_closing() const noexcept -> bool;
    [[nodiscard]] auto is_quiescent() const noexcept -> bool;
    [[nodiscard]] auto shutdown_cleanup_failed() const noexcept -> bool;
    [[nodiscard]] auto projection_delivery_failed() const noexcept -> bool;
    [[nodiscard]] auto epoch_handoff_delivery_failed() const noexcept -> bool;

private:
    struct implementation;
    std::unique_ptr<implementation> Implementation;
};
}
