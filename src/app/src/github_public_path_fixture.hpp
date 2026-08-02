#pragma once

#if !defined(BPR_BUILD_TESTS)
#error "The GitHub public-path fixture is test-build only"
#endif

#include "github_application_fixture_access.hpp"

#include "github_host_profile.hpp"
#include "github_provider.hpp"

#include "bpr/application/github_credential_vault.hpp"
#include "bpr/application/port.hpp"

#include <memory>
#include <filesystem>

namespace bpr::app::detail
{
class github_public_path_fixture_components final
{
public:
    github_public_path_fixture_components(
        github_public_path_fixture fixture,
        const adapters::github::validated_github_host_profile& profile,
        application::effect_executor& vault_effects,
        github_public_path_fixture_options options);
    ~github_public_path_fixture_components();

    github_public_path_fixture_components(
        const github_public_path_fixture_components&) = delete;
    auto operator=(const github_public_path_fixture_components&)
        -> github_public_path_fixture_components& = delete;
    github_public_path_fixture_components(
        github_public_path_fixture_components&&) = delete;
    auto operator=(github_public_path_fixture_components&&)
        -> github_public_path_fixture_components& = delete;

    [[nodiscard]] auto transport_factory() const noexcept
        -> std::shared_ptr<
            adapters::github::detail::github_wire_transport_factory_port>;
    [[nodiscard]] auto credential_vault() const noexcept
        -> application::github_credential_vault_port&;
    [[nodiscard]] auto take_enrollment_credential()
        -> application::github_credential_material;
    [[nodiscard]] auto evidence() const noexcept
        -> github_public_path_fixture_evidence;
    [[nodiscard]] auto last_smart_authority() const noexcept
        -> std::optional<core::pull_request_authority>;
    [[nodiscard]] auto application_data_root() const
        -> const std::filesystem::path&;
    [[nodiscard]] auto git_executable() const
        -> const std::filesystem::path&;

private:
    class implementation;
    std::unique_ptr<implementation> Implementation;
};
}
