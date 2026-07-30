#pragma once

#include "github_host_profile.hpp"
#include "github_wire_transport.hpp"

#include "bpr/application/github_credential_vault.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>

namespace bpr::adapters::github::detail
{
inline constexpr std::size_t native_https_maximum_request_body_bytes =
    256U * 1024U;
inline constexpr std::size_t native_https_maximum_response_body_bytes =
    4U * 1024U * 1024U;
inline constexpr std::size_t native_https_maximum_response_header_bytes =
    64U * 1024U;
inline constexpr std::uint32_t native_https_maximum_redirects = 3U;
inline constexpr auto native_https_total_timeout =
    std::chrono::seconds{30};

enum class native_https_configuration_failure : std::uint8_t
{
    profile_transport_mismatch,
    profile_policy_mismatch,
    invalid_bootstrap_context,
    invalid_account_credential,
    credential_authority_mismatch,
    invalid_credential_material,
    invalid_test_configuration,
    allocation_failure,
};

enum class native_https_tls_backend : std::uint8_t
{
    unavailable,
    secure_transport,
    openssl,
    schannel,
    other,
};

enum class native_https_system_proxy_kind : std::uint8_t
{
    unavailable,
    direct,
    http,
    socks5,
    unsupported,
};

struct native_https_runtime_facts
{
    native_https_tls_backend TlsBackend =
        native_https_tls_backend::unavailable;
    native_https_system_proxy_kind Proxy =
        native_https_system_proxy_kind::unavailable;
    std::uint32_t ProxyCandidateCount = 0U;
    bool TlsSupported = false;

    auto operator==(const native_https_runtime_facts&) const -> bool =
        default;
};

#if defined(BPR_NATIVE_HTTPS_TESTING)
struct native_https_test_configuration
{
    std::string ProxyHost;
    std::uint16_t ProxyPort = 0U;
    std::uint16_t FallbackProxyPort = 0U;
    std::string TrustedCaCertificatePem;
    std::chrono::milliseconds TotalTimeout =
        native_https_total_timeout;
    std::size_t ResponseBodyLimit =
        native_https_maximum_response_body_bytes;

    auto operator==(const native_https_test_configuration&) const -> bool =
        default;
};
#endif

class native_https_transport final : public github_wire_transport_port
{
public:
    class implementation;

    explicit native_https_transport(
        std::unique_ptr<implementation> implementation) noexcept;
    ~native_https_transport() override;

    native_https_transport(const native_https_transport&) = delete;
    auto operator=(const native_https_transport&)
        -> native_https_transport& = delete;
    native_https_transport(native_https_transport&&) = delete;
    auto operator=(native_https_transport&&)
        -> native_https_transport& = delete;

    [[nodiscard]] auto perform(
        const github_wire_request& request,
        std::stop_token stop) -> github_wire_completion override;

private:
    std::unique_ptr<implementation> Implementation;
};

using native_https_transport_factory_result = std::expected<
    std::unique_ptr<native_https_transport>,
    native_https_configuration_failure>;

[[nodiscard]] auto make_native_https_bootstrap_transport(
    const validated_github_host_profile& profile,
    core::github_session_request_context context,
    application::github_credential_material credential) noexcept
    -> native_https_transport_factory_result;

[[nodiscard]] auto make_native_https_account_transport(
    const validated_github_host_profile& profile,
    application::github_credential_retrieve_success credential) noexcept
    -> native_https_transport_factory_result;

// These factories exist only in test-enabled builds. They preserve the exact
// production request path while replacing system proxy discovery with one
// bounded loopback proxy and, when supplied, augmenting trust with one
// disposable fixture CA. Product composition must use the factories above.
#if defined(BPR_NATIVE_HTTPS_TESTING)
[[nodiscard]] auto make_native_https_bootstrap_transport_for_test(
    const validated_github_host_profile& profile,
    core::github_session_request_context context,
    application::github_credential_material credential,
    const native_https_test_configuration& configuration) noexcept
    -> native_https_transport_factory_result;

[[nodiscard]] auto make_native_https_account_transport_for_test(
    const validated_github_host_profile& profile,
    application::github_credential_retrieve_success credential,
    const native_https_test_configuration& configuration) noexcept
    -> native_https_transport_factory_result;
#endif

[[nodiscard]] auto observe_native_https_runtime(
    const validated_github_host_profile& profile) noexcept
    -> native_https_runtime_facts;
}
