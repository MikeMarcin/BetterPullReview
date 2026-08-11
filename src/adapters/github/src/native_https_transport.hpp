#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_NATIVE_HTTPS_TRANSPORT_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_NATIVE_HTTPS_TRANSPORT_HPP

#include "github_host_profile.hpp"
#include "github_mutation_contract.hpp"
#include "github_wire_transport.hpp"

#include "bpr/application/git_smart_transport_port.hpp"
#include "bpr/application/github_credential_vault.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>

namespace bpr::adapters::github::detail {
	class native_git_smart_lease_state;

	inline constexpr std::size_t   native_https_maximum_request_body_bytes        = 256U * 1024U;
	inline constexpr std::size_t   native_https_maximum_response_body_bytes       = 4U * 1024U * 1024U;
	inline constexpr std::size_t   native_https_maximum_error_response_body_bytes = 64U * 1024U;
	inline constexpr std::size_t   native_https_maximum_response_header_bytes     = 64U * 1024U;
	inline constexpr std::uint32_t native_https_maximum_redirects                 = 3U;
	inline constexpr auto          native_https_total_timeout                     = std::chrono::seconds{ 30 };
	inline constexpr auto          native_git_smart_total_timeout                 = std::chrono::minutes{ 15 };

	enum class native_https_configuration_failure : std::uint8_t {
		profile_transport_mismatch,
		profile_policy_mismatch,
		invalid_bootstrap_context,
		invalid_account_credential,
		credential_authority_mismatch,
		invalid_credential_material,
		invalid_test_configuration,
		allocation_failure,
	};

	enum class native_https_tls_backend : std::uint8_t {
		unavailable,
		secure_transport,
		openssl,
		schannel,
		other,
	};

	enum class native_https_system_proxy_kind : std::uint8_t {
		unavailable,
		direct,
		http,
		socks5,
		unsupported,
	};

	struct native_https_runtime_facts
	{
		native_https_tls_backend       TlsBackend          = native_https_tls_backend::unavailable;
		native_https_system_proxy_kind Proxy               = native_https_system_proxy_kind::unavailable;
		std::uint32_t                  ProxyCandidateCount = 0U;
		bool                           TlsSupported        = false;

		auto operator==(const native_https_runtime_facts&) const -> bool = default;
	};

#if defined(BPR_NATIVE_HTTPS_TESTING)
	struct native_https_test_configuration
	{
		std::string               ProxyHost;
		std::uint16_t             ProxyPort                 = 0U;
		std::uint16_t             FallbackProxyPort         = 0U;
		bool                      DirectFirstProxyCandidate = false;
		std::string               TrustedCaCertificatePem;
		std::chrono::milliseconds TotalTimeout      = native_https_total_timeout;
		std::size_t               ResponseBodyLimit = native_https_maximum_response_body_bytes;

		auto operator==(const native_https_test_configuration&) const -> bool = default;
	};
#endif

	class native_git_smart_transport;

	class native_https_transport final
		: public github_wire_transport_port,
		  public github_mutation_wire_transport
	{
	public:
		class implementation;

		explicit native_https_transport(
			std::unique_ptr<implementation> aImplementation) noexcept;
		~native_https_transport() override;

		native_https_transport(const native_https_transport&) = delete;
		auto operator=(const native_https_transport&)
			-> native_https_transport&                   = delete;
		native_https_transport(native_https_transport&&) = delete;
		auto operator=(native_https_transport&&)
			-> native_https_transport& = delete;

		[[nodiscard]] auto perform(
			const github_wire_request& aRequest,
			std::stop_token            aStop) -> github_wire_completion override;

		[[nodiscard]] auto perform_capability_probe(
			const github_capability_probe_wire_request& aRequest,
			std::stop_token                             aStop)
			-> github_capability_probe_wire_completion override;

		[[nodiscard]] auto perform_review_mutation(
			const github_mutation_wire_request& aRequest,
			std::stop_token                     aStop)
			-> github_mutation_wire_completion override;

	private:
		friend class native_git_smart_transport;

		[[nodiscard]] auto can_acquire_smart(
			const application::git_smart_request_context& aContext) const noexcept
			-> bool;

		[[nodiscard]] auto discover_for_lease(
			const application::git_smart_discovery_request&      aRequest,
			const std::shared_ptr<native_git_smart_lease_state>& aLease,
			std::stop_token                                      aStop)
			-> application::git_smart_transport_result<
				application::git_smart_discovery_response>;

		[[nodiscard]] auto ls_refs_for_lease(
			const application::git_smart_ls_refs_request&        aRequest,
			const std::shared_ptr<native_git_smart_lease_state>& aLease,
			std::stop_token                                      aStop)
			-> application::git_smart_transport_result<
				application::git_smart_ls_refs_response>;

		[[nodiscard]] auto fetch_for_lease(
			const application::git_smart_fetch_request&          aRequest,
			const std::shared_ptr<native_git_smart_lease_state>& aLease,
			application::git_smart_wire_consumer&                aConsumer,
			std::stop_token                                      aStop)
			-> application::git_smart_transport_result<void>;

		void cancel_lease(
			const std::shared_ptr<native_git_smart_lease_state>& aLease) noexcept;

		std::unique_ptr<implementation> Implementation;
	};

	using native_https_transport_factory_result = std::expected<std::unique_ptr<native_https_transport>,
																native_https_configuration_failure>;

	[[nodiscard]] auto make_native_git_smart_transport(
		std::shared_ptr<native_https_transport>       aTransport,
		const application::git_smart_request_context& aContext) noexcept
		-> application::git_smart_transport_result<
			application::git_smart_transport>;

	[[nodiscard]] auto make_native_https_bootstrap_transport(
		const validated_github_host_profile&    aProfile,
		core::github_session_request_context    aContext,
		application::github_credential_material aCredential) noexcept
		-> native_https_transport_factory_result;

	[[nodiscard]] auto make_native_https_account_transport(
		const validated_github_host_profile&            aProfile,
		application::github_credential_retrieve_success aCredential) noexcept
		-> native_https_transport_factory_result;

// These factories exist only in test-enabled builds. They preserve the exact
// production request path while replacing system proxy discovery with one
// bounded loopback proxy and, when supplied, augmenting trust with one
// disposable fixture CA. Product composition must use the factories above.
#if defined(BPR_NATIVE_HTTPS_TESTING)
	[[nodiscard]] auto make_native_https_bootstrap_transport_for_test(
		const validated_github_host_profile&    aProfile,
		core::github_session_request_context    aContext,
		application::github_credential_material aCredential,
		const native_https_test_configuration&  aConfiguration) noexcept
		-> native_https_transport_factory_result;

	[[nodiscard]] auto make_native_https_account_transport_for_test(
		const validated_github_host_profile&            aProfile,
		application::github_credential_retrieve_success aCredential,
		const native_https_test_configuration&          aConfiguration) noexcept
		-> native_https_transport_factory_result;
#endif

	[[nodiscard]] auto observe_native_https_runtime(
		const validated_github_host_profile& aProfile) noexcept
		-> native_https_runtime_facts;
} // namespace bpr::adapters::github::detail

#endif // BPR_SRC_ADAPTERS_GITHUB_SRC_NATIVE_HTTPS_TRANSPORT_HPP
