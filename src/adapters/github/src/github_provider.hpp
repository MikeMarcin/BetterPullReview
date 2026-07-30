#pragma once

#include "github_host_profile.hpp"
#include "github_wire_transport.hpp"

#include "bpr/application/github_provider_port.hpp"
#include "bpr/application/port.hpp"

#include <cstdint>
#include <expected>
#include <memory>
#include <span>

namespace bpr::adapters::github::detail
{
using github_wire_transport_factory_result =
    std::expected<std::shared_ptr<github_wire_transport_port>,
                  application::github_provider_error>;

class github_wire_transport_factory_port
{
public:
    github_wire_transport_factory_port() = default;
    virtual ~github_wire_transport_factory_port() = default;

    github_wire_transport_factory_port(
        const github_wire_transport_factory_port&) = delete;
    auto operator=(const github_wire_transport_factory_port&)
        -> github_wire_transport_factory_port& = delete;
    github_wire_transport_factory_port(github_wire_transport_factory_port&&) =
        delete;
    auto operator=(github_wire_transport_factory_port&&)
        -> github_wire_transport_factory_port& = delete;

    [[nodiscard]] virtual auto
    make_bootstrap(const validated_github_host_profile& profile,
                   core::github_session_request_context context,
                   application::github_credential_material credential) noexcept
        -> github_wire_transport_factory_result = 0;

    [[nodiscard]] virtual auto make_account(
        const validated_github_host_profile& profile,
        application::github_credential_retrieve_success credential) noexcept
        -> github_wire_transport_factory_result = 0;
};

class native_github_wire_transport_factory final
    : public github_wire_transport_factory_port
{
public:
    [[nodiscard]] auto
    make_bootstrap(const validated_github_host_profile& profile,
                   core::github_session_request_context context,
                   application::github_credential_material credential) noexcept
        -> github_wire_transport_factory_result override;

    [[nodiscard]] auto make_account(
        const validated_github_host_profile& profile,
        application::github_credential_retrieve_success credential) noexcept
        -> github_wire_transport_factory_result override;
};

enum class github_provider_configuration_failure : std::uint8_t
{
    no_profiles,
    invalid_profile,
    duplicate_profile,
    allocation_failure,
};

class github_provider final : public application::github_provider_port
{
public:
    class implementation;

    explicit github_provider(
        std::unique_ptr<implementation> implementation) noexcept;
    ~github_provider() override;

    github_provider(const github_provider&) = delete;
    auto operator=(const github_provider&) -> github_provider& = delete;
    github_provider(github_provider&&) = delete;
    auto operator=(github_provider&&) -> github_provider& = delete;

    void resolve_profile(
        application::github_profile_request request,
        application::github_profile_completion_handler completion) override;
    void cancel_profile(application::github_profile_request request) override;

    void observe_viewer(
        application::github_viewer_submission submission,
        application::github_viewer_completion_handler completion) override;
    void cancel_viewer(application::github_viewer_request request) override;

    void probe_capabilities(
        application::github_capability_request request,
        application::github_capability_completion_handler completion) override;
    void cancel_capabilities(
        application::github_capability_request request) override;

    void fetch_pull_request(
        application::github_pull_request_provider_request request,
        application::github_pull_request_completion_handler completion)
        override;
    void cancel_pull_request(
        application::github_pull_request_provider_request request) override;

    void shutdown() noexcept;

private:
    std::unique_ptr<implementation> Implementation;
};

using github_provider_factory_result =
    std::expected<std::unique_ptr<github_provider>,
                  github_provider_configuration_failure>;

[[nodiscard]] auto
make_github_provider(std::span<const validated_github_host_profile> profiles,
                     application::effect_executor& effects,
                     std::shared_ptr<github_wire_transport_factory_port>
                         transport_factory) noexcept
    -> github_provider_factory_result;

[[nodiscard]] auto make_github_provider(
    std::span<const validated_github_host_profile* const> profiles,
    application::effect_executor& effects,
    std::shared_ptr<github_wire_transport_factory_port>
        transport_factory) noexcept -> github_provider_factory_result;
}
