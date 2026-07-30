#pragma once

#include "github_cli_process.hpp"
#include "github_host_profile.hpp"
#include "github_wire_transport.hpp"

#include <array>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace bpr::adapters::github::detail
{
struct gh_cli_macos_environment_facts
{
    std::filesystem::path HomeDirectory;

    auto operator==(const gh_cli_macos_environment_facts&) const -> bool =
        default;
};

struct gh_cli_linux_environment_facts
{
    std::filesystem::path HomeDirectory;
    std::filesystem::path RuntimeDirectory;
    std::string SessionBusAddress;

    auto operator==(const gh_cli_linux_environment_facts&) const -> bool =
        default;
};

struct gh_cli_windows_environment_facts
{
    std::filesystem::path UserProfileDirectory;
    std::filesystem::path RoamingApplicationDataDirectory;
    std::filesystem::path LocalApplicationDataDirectory;
    std::filesystem::path SystemRootDirectory;

    auto operator==(const gh_cli_windows_environment_facts&) const -> bool =
        default;
};

using gh_cli_platform_environment_facts = std::variant<
    gh_cli_macos_environment_facts,
    gh_cli_linux_environment_facts,
    gh_cli_windows_environment_facts>;

struct gh_cli_https_environment_facts
{
    std::optional<std::string> Proxy;
    std::optional<std::string> NoProxy;
    std::optional<std::filesystem::path> CertificateFile;
    std::optional<std::filesystem::path> CertificateDirectory;

    auto operator==(const gh_cli_https_environment_facts&) const -> bool =
        default;
};

struct gh_cli_configuration_candidate
{
    std::filesystem::path Executable;
    std::filesystem::path NeutralWorkingDirectory;
    gh_cli_platform_environment_facts Platform;
    gh_cli_https_environment_facts Https;
};

enum class gh_cli_invocation_failure : std::uint8_t
{
    profile_transport_mismatch,
    profile_correlation_mismatch,
    server_contract_mismatch,
    invalid_wire_request,
    invalid_route,
    request_limit,
};

enum class gh_cli_configuration_failure : std::uint8_t
{
    executable_not_absolute,
    executable_not_found,
    executable_not_regular,
    executable_not_executable,
    executable_name_mismatch,
    working_directory_not_absolute,
    working_directory_not_found,
    working_directory_not_directory,
    working_directory_is_repository,
    irrelevant_platform_facts,
    invalid_platform_environment,
    environment_limit,
    invalid_https_proxy,
    credential_bearing_https_proxy,
    invalid_no_proxy,
    invalid_certificate_file,
    invalid_certificate_directory,
};

class validated_gh_cli_configuration final
{
public:
    validated_gh_cli_configuration(
        const validated_gh_cli_configuration&) = default;
    validated_gh_cli_configuration(
        validated_gh_cli_configuration&&) = delete;
    auto operator=(const validated_gh_cli_configuration&)
        -> validated_gh_cli_configuration& = default;
    auto operator=(validated_gh_cli_configuration&&)
        -> validated_gh_cli_configuration& = delete;

    [[nodiscard]] auto executable() const noexcept
        -> const std::filesystem::path&;
    [[nodiscard]] auto neutral_working_directory() const noexcept
        -> const std::filesystem::path&;

private:
    validated_gh_cli_configuration(
        std::filesystem::path executable,
        std::filesystem::path neutral_working_directory,
        std::vector<github_cli_process_environment_entry> environment);

    std::filesystem::path Executable;
    std::filesystem::path NeutralWorkingDirectory;
    std::vector<github_cli_process_environment_entry> Environment;

    friend auto validate_gh_cli_configuration(
        const gh_cli_configuration_candidate& candidate)
        -> std::expected<
            validated_gh_cli_configuration,
            gh_cli_configuration_failure>;
    friend auto compile_gh_cli_process_request(
        const validated_gh_cli_configuration& configuration,
        const validated_github_host_profile& profile,
        const github_wire_request& request)
        -> std::expected<
            github_cli_process_request,
            gh_cli_invocation_failure>;
};

using gh_cli_configuration_result = std::expected<
    validated_gh_cli_configuration,
    gh_cli_configuration_failure>;

[[nodiscard]] auto validate_gh_cli_configuration(
    const gh_cli_configuration_candidate& candidate)
    -> gh_cli_configuration_result;

using gh_cli_invocation_result = std::expected<
    github_cli_process_request,
    gh_cli_invocation_failure>;

[[nodiscard]] auto compile_gh_cli_process_request(
    const validated_gh_cli_configuration& configuration,
    const validated_github_host_profile& profile,
    const github_wire_request& request) -> gh_cli_invocation_result;

enum class gh_cli_availability_reason : std::uint8_t
{
    executable_identity_unverifiable,
    cross_authority_redirect_unverifiable,
    ambient_configuration_unverifiable,
    credential_identity_binding_unverifiable,
};

class gh_cli_transport final : public github_wire_transport_port
{
public:
    gh_cli_transport(
        const validated_github_host_profile& profile,
        const validated_gh_cli_configuration& configuration,
        github_cli_process_port& process);

    [[nodiscard]] auto availability_reasons() const noexcept
        -> std::array<gh_cli_availability_reason, 4>;

    [[nodiscard]] auto perform(
        const github_wire_request& request,
        std::stop_token stop) -> github_wire_completion override;

private:
    validated_github_host_profile Profile;
    validated_gh_cli_configuration Configuration;
    github_cli_process_port& Process;
};
}
