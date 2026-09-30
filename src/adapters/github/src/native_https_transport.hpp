#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_NATIVE_HTTPS_TRANSPORT_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_NATIVE_HTTPS_TRANSPORT_HPP

#include "github_device_authorization_contract.hpp"
#include "github_host_profile.hpp"
#include "github_mutation_contract.hpp"
#include "github_publication_contract.hpp"
#include "github_wire_transport.hpp"

#include "bpr/application/git_smart_transport_port.hpp"
#include "bpr/application/github_credential_vault.hpp"
#include "bpr/application/review_mutation_application_host.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <variant>

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
		anonymous_read_contract_unsupported,
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

	struct native_https_device_authorization_success
	{
		wire_response Response;

		auto operator==(const native_https_device_authorization_success&) const
			-> bool = default;
	};

	struct native_https_device_authorization_error
	{
		application::github_provider_error Error{
			.Failure           = application::github_provider_failure::unavailable,
			.RetryAfterSeconds = std::nullopt,
			.RateLimits        = { },
			.RateLimitCause    = std::nullopt,
		};

		auto operator==(const native_https_device_authorization_error&) const
			-> bool = default;
	};

	struct native_https_device_authorization_cancelled
	{
		auto operator==(const native_https_device_authorization_cancelled&) const
			-> bool = default;
	};

	using native_https_device_authorization_completion
		= std::variant<native_https_device_authorization_success,
					   native_https_device_authorization_error,
					   native_https_device_authorization_cancelled>;

	class native_git_smart_transport;

	class native_https_transport final
		: public github_wire_transport_port,
		  public github_mutation_wire_transport,
		  public github_publication_wire_transport
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
			const github_mutation_wire_request&  aRequest,
			std::stop_token                      aStop,
			github_mutation_transmission_witness aWitness)
			-> github_mutation_wire_completion override;

		/// \brief [[BPR-1175]]. One exchange of the publication catalog.
		[[nodiscard]] auto perform_publication(
			const github_publication_wire_call& aCall,
			std::stop_token                     aStop)
			-> github_publication_wire_completion override;

		/// \brief Computes the device authorization exchange result.
		///
		/// \details The device flow runs before any credential exists, so this
		/// admits only a deliberately credential-free binding and refuses every
		/// credentialed one. That refusal is what makes the missing
		/// authorization header structural rather than conditional: the exact
		/// state this entry requires is the exact state under which the request
		/// builder omits the header. Everything else — manual redirects with
		/// none allowed, certificate refusal, proxy candidates, cancellation,
		/// and the response bound — is the shared machinery unchanged.
		[[nodiscard]] auto perform_device_authorization(
			const device_authorization_wire_request& aRequest,
			std::stop_token                          aStop)
			-> native_https_device_authorization_completion;

	private:
		friend class native_git_smart_transport;

		[[nodiscard]] auto can_acquire_smart(
			const application::git_smart_request_context& aContext,
			std::optional<core::application_session_id>   aAuthenticationSession) const noexcept
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
		const application::git_smart_request_context& aContext,
		std::optional<core::application_session_id>   aAuthenticationSession = std::nullopt) noexcept
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

	/// \brief Builds a shipped transport whose credential-free state is deliberate.
	///
	/// \details The device flow runs before any credential exists, so
	/// [[BPR-0279]] needs a production transport that can issue a request with
	/// no authorization header while every other RFC-BPR-0002 rule stays intact.
	/// This builds exactly that and nothing else. It takes no credential
	/// parameter, so the caller cannot supply one and a missing, malformed, or
	/// revoked credential cannot decay into this state; reaching it requires
	/// naming it. It is the production twin of the `_for_test` factory below and
	/// differs from it only in using the production policy — the same system
	/// trust, proxy discovery, redirect refusal, and response bounds as every
	/// other shipped transport.
	///
	/// The transport this returns admits only the two device-authorization
	/// endpoints, because `perform_device_authorization` is the only entry whose
	/// binding it satisfies. It creates no anonymous profile and no partial
	/// public mode.
	[[nodiscard]] auto make_native_https_credential_free_transport(
		const validated_github_host_profile& aProfile,
		core::github_session_request_context aContext) noexcept
		-> native_https_transport_factory_result;

	/// \brief Builds a shipped transport bound to an anonymous read authority.
	///
	/// \details RFC-BPR-0017's public preview reads GitHub.com's public REST
	/// surface with no credential at all. This is the only production entry
	/// that produces such a transport, and it takes no credential parameter,
	/// so an absent, malformed, or revoked credential cannot decay into it:
	/// reaching this state requires naming it. Nothing reachable from here
	/// touches a vault.
	///
	/// The anonymous binding is distinct from the device flow's
	/// credential-free binding above. The device overload refuses it, the Git
	/// smart surface refuses it, review mutations refuse it, and on the read
	/// path it satisfies only the eligible REST operations named by
	/// `admit_anonymous_read`; viewer identity and the GraphQL-routed
	/// operation are refused before a request is built.
	///
	/// A profile whose server contract is not `github_com_2022_11_28` is
	/// refused here with `anonymous_read_contract_unsupported` before any
	/// transport exists, so a GHES profile sends no request. Every other
	/// RFC-BPR-0002 rule — system trust, ordered proxy candidates, manual
	/// redirect refusal, timeouts, and response bounds — is the shared
	/// production policy, unchanged.
	[[nodiscard]] auto make_native_https_anonymous_read_transport(
		const validated_github_host_profile& aProfile,
		core::github_session_request_context aContext) noexcept
		-> native_https_transport_factory_result;

	/// \brief Builds a shipped transport bound to an anonymous Git smart
	/// authority.
	///
	/// \details RFC-BPR-0017's public preview materializes a public
	/// repository over Git's smart HTTP surface, which authenticates with
	/// HTTP Basic and has no credentialed form for a signed-out reader. This
	/// is the only production entry that produces such a transport, and it
	/// takes no credential parameter, so an absent, malformed, or revoked
	/// credential cannot decay into it: reaching this state requires naming
	/// it. Nothing reachable from here touches a vault.
	///
	/// The binding is distinct from `anonymous_read_request`'s: that one
	/// satisfies the eligible public REST reads and refuses this surface, and
	/// this one satisfies this surface and refuses every REST read,
	/// capability probe, review mutation, and device-authorization request.
	///
	/// A lease is admitted only for a `public_preview` authority. An
	/// authenticated authority — a signed-in or private intent — is refused
	/// before a request is constructed rather than dialed without the
	/// credential it would need, and a profile whose server contract is not
	/// `github_com_2022_11_28` is refused here with
	/// `anonymous_read_contract_unsupported` before any transport exists, so
	/// a GHES profile sends nothing. The surface reaches only
	/// `git-upload-pack`: `smart_request_surface` enumerates discovery,
	/// ls-refs, and fetch and nothing else, so there is no push route to
	/// refuse at run time. Every other RFC-BPR-0002 rule — system trust,
	/// ordered proxy candidates, manual redirect refusal, timeouts, and
	/// response bounds — is the shared production policy, unchanged.
	[[nodiscard]] auto make_native_https_anonymous_smart_transport(
		const validated_github_host_profile& aProfile,
		core::github_session_request_context aContext) noexcept
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

	/// \brief Builds a transport whose credential-free state is deliberate.
	///
	/// \details RFC-BPR-0017 needs to know whether this transport can issue a
	/// request with no authorization header while every other RFC-BPR-0002 rule
	/// stays intact. This answers that question and nothing else. It takes no
	/// credential parameter, so the caller cannot supply one and a missing,
	/// malformed, or revoked credential cannot decay into this state; reaching
	/// it requires naming it. The symbol exists only under
	/// `BPR_NATIVE_HTTPS_TESTING` and is absent from a shipped build, so it
	/// creates no anonymous profile and no partial public mode.
	[[nodiscard]] auto make_native_https_credential_free_transport_for_test(
		const validated_github_host_profile&   aProfile,
		core::github_session_request_context   aContext,
		const native_https_test_configuration& aConfiguration) noexcept
		-> native_https_transport_factory_result;

	/// \brief Builds an anonymous-read transport against the loopback seam.
	///
	/// \details The production twin of this factory is the shipped entry; the
	/// two differ only in policy, so a course driven through this one exercises
	/// the same binding, the same header construction, and the same refusals
	/// that ship.
	[[nodiscard]] auto make_native_https_anonymous_read_transport_for_test(
		const validated_github_host_profile&   aProfile,
		core::github_session_request_context   aContext,
		const native_https_test_configuration& aConfiguration) noexcept
		-> native_https_transport_factory_result;

	/// \brief Builds an anonymous smart transport against the loopback seam.
	///
	/// \details The production twin of this factory is the shipped entry; the
	/// two differ only in policy, so a course driven through this one
	/// exercises the same binding, the same absent authorization header, and
	/// the same refusals that ship.
	[[nodiscard]] auto make_native_https_anonymous_smart_transport_for_test(
		const validated_github_host_profile&   aProfile,
		core::github_session_request_context   aContext,
		const native_https_test_configuration& aConfiguration) noexcept
		-> native_https_transport_factory_result;
#endif

	[[nodiscard]] auto observe_native_https_runtime(
		const validated_github_host_profile& aProfile) noexcept
		-> native_https_runtime_facts;

} // namespace bpr::adapters::github::detail

#endif // BPR_SRC_ADAPTERS_GITHUB_SRC_NATIVE_HTTPS_TRANSPORT_HPP
