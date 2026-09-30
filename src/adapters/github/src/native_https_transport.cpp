#include "native_https_transport.hpp"
#include "github_release_update.hpp"
#include "github_release_update_contract.hpp"

#include "designated_review_public_observer.hpp"
#include "review_mutation_execution_authority.hpp"

#include "github_anonymous_read_admission.hpp"

#include "github_paged_read.hpp"
#include "github_wire_request_validation.hpp"

#include <QAuthenticator>
#include <QByteArray>
#include <QCoreApplication>
#include <QEvent>
#include <QEventLoop>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslError>
#include <QSslKey>
#include <QSslPreSharedKeyAuthenticator>
#include <QSslSocket>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVariant>

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::adapters::github::detail {
	class native_git_smart_lease_state final
	{
	public:
		explicit native_git_smart_lease_state(
			application::git_smart_request_context      aContext,
			std::optional<core::application_session_id> aAuthenticationSession) noexcept
			: Context{ std::move(aContext) }
			, AuthenticationSession{ aAuthenticationSession }
		{
		}

		application::git_smart_request_context            Context;
		const std::optional<core::application_session_id> AuthenticationSession;
		std::atomic_bool                                  Revoked = false;
	};

	namespace {
		constexpr std::size_t      maximum_url_bytes                    = 4U * 1024U;
		constexpr std::size_t      maximum_response_header_count        = 64U;
		constexpr std::size_t      maximum_response_header_field_bytes  = 8U * 1024U;
		constexpr std::size_t      response_read_buffer_bytes           = 64U * 1024U;
		constexpr std::size_t      response_read_chunk_bytes            = 16U * 1024U;
		constexpr std::size_t      maximum_system_proxy_candidates      = 16U;
		constexpr std::uint32_t    maximum_retry_after_seconds          = 86'400U;
		constexpr auto             cancellation_poll_interval           = std::chrono::milliseconds{ 5 };
		constexpr auto             smart_idle_timeout                   = std::chrono::seconds{ 30 };
		constexpr std::string_view smart_git_protocol_header            = "version=2";
		constexpr std::string_view smart_git_request_content_type       = "application/x-git-upload-pack-request";
		constexpr std::string_view smart_git_advertisement_content_type = "application/x-git-upload-pack-advertisement";
		constexpr std::string_view smart_git_result_content_type        = "application/x-git-upload-pack-result";
		constexpr std::string_view smart_git_basic_username             = "x-access-token:";
		constexpr std::string_view device_code_route                    = "/login/device/code";
		constexpr std::string_view device_token_route                   = "/login/oauth/access_token";
		constexpr int              device_code_refusal_status           = 400;
#if defined(BPR_NATIVE_HTTPS_TESTING)
		constexpr auto minimum_test_timeout = std::chrono::milliseconds{ 25 };
#endif

		enum class proxy_policy : std::uint8_t {
			system,
#if defined(BPR_NATIVE_HTTPS_TESTING)
			fixed_test,
#endif
		};

		struct transport_policy
		{
			proxy_policy Proxy = proxy_policy::system;
#if defined(BPR_NATIVE_HTTPS_TESTING)
			std::string   ProxyHost;
			std::uint16_t ProxyPort                 = 0U;
			std::uint16_t FallbackProxyPort         = 0U;
			bool          DirectFirstProxyCandidate = false;
			std::string   TrustedCaCertificatePem;
#endif
			std::chrono::milliseconds TotalTimeout      = native_https_total_timeout;
			std::chrono::milliseconds SmartTotalTimeout = native_git_smart_total_timeout;
			std::size_t               ResponseBodyLimit = native_https_maximum_response_body_bytes;
		};

		struct bootstrap_credential
		{
			core::github_session_request_context    Context;
			application::github_credential_material Material;
			bool                                    Used = false;
		};

		struct account_credential
		{
			application::github_credential_operation_context Context;
			application::github_credential_material          Material;
		};

		/// \brief A deliberate credential-free binding, not an absent credential.
		///
		/// \details It carries no material field at all, so there is no empty or
		/// wiped credential for a later change to mistake for a usable one. The
		/// state ships, because the device-authorization flow runs before any
		/// credential exists; the read-path match for it stays testing-only, so
		/// in a shipped build this binding satisfies exactly the two
		/// device-authorization endpoints and nothing else.
		struct credential_free_request
		{
			core::github_session_request_context Context;
		};

		/// \brief A deliberate anonymous read authority.
		///
		/// \details Distinct from `credential_free_request`, which exists so
		/// the device flow can run before any credential does, and distinct
		/// from a missing credential, which is a failure rather than a state.
		/// Like that one it carries no material field, so there is no empty or
		/// wiped credential for a later change to mistake for a usable one.
		/// Unlike that one its read-path match is the same in a shipped build
		/// as in a test build: RFC-BPR-0017's public preview ships.
		struct anonymous_read_request
		{
			core::github_session_request_context Context;
		};

		/// \brief A deliberate credential-free Git smart authority.
		///
		/// \details RFC-BPR-0017's public preview materializes a public
		/// repository over Git's smart HTTP surface, which authenticates with
		/// HTTP Basic and has no credentialed form here. This binding is its
		/// own alternative rather than a reuse of `anonymous_read_request`
		/// because the two admit disjoint surfaces: the read binding satisfies
		/// eligible REST operations and refuses the smart surface, and this one
		/// satisfies the smart surface and refuses every REST, capability,
		/// mutation, and device operation. Like both other credential-free
		/// alternatives it carries no material field, so there is no empty or
		/// wiped credential a later change could mistake for a usable one.
		struct anonymous_smart_request
		{
			core::github_session_request_context Context;
		};

		using credential_binding = std::variant<bootstrap_credential,
												account_credential,
												credential_free_request,
												anonymous_read_request,
												anonymous_smart_request>;

		enum class native_request_surface : std::uint8_t {
			rest_read,
			graphql_read,
			capability_probe,
			review_mutation,
			device_authorization,
			/// [[BPR-1175]]. The publication catalog: REST reads and writes of
			/// Git objects, a new branch and a pull request.
			publication,
		};

		struct native_request
		{
			native_request_surface                  Surface                 = native_request_surface::rest_read;
			application::github_rate_limit_resource RateLimitResource       = application::github_rate_limit_resource::uninitialized;
			bool                                    RequireRateLimitHeaders = true;
			http_method                             Method                  = http_method::get;
			std::string                             Path;
			std::string                             Query;
			std::vector<http_header>                Headers;
			std::string                             Body;
			std::size_t                             ResponseLimit = native_https_maximum_response_body_bytes;
			// The one non-success status whose body is a contract outcome
			// rather than a failure. Zero admits none, and a hop that reached
			// no status at all is already refused before this is consulted.
			int AdmittedContractStatus = 0;
			// The one status that is success. Only a review mutation's
			// exchange names another ([[BPR-1018]]).
			int SuccessfulStatus = 200;
		};

		struct hop_cancelled
		{
		};

		struct hop_redirect
		{
			std::string Target;
		};

		struct hop_proxy_failure
		{
			application::github_provider_error Error;
		};

		struct hop_success
		{
			int                             Status = 0;
			std::string                     Body;
			std::vector<http_header>        Headers;
			page_completeness               Completeness = page_completeness::unknown;
			application::diagnostic_summary CapabilitySummary{ };
		};

		struct hop_capability_failure
		{
			application::github_provider_error        Error;
			application::github_capability_diagnostic Detail;
		};

		using hop_result = std::variant<hop_success, hop_redirect, hop_proxy_failure,
										application::github_provider_error, hop_cancelled, hop_capability_failure>;

		struct operation_state
		{
			operation_state(native_request aRequest, const std::stop_token aStop,
							const std::chrono::steady_clock::time_point aDeadline,
							github_mutation_transmission_witness        aWitness = { })
				: Request{ std::move(aRequest) }
				, Stop{ aStop }
				, Deadline{ aDeadline }
				, TransmissionWitness{ std::move(aWitness) }
			{
			}

			native_request                        Request;
			std::stop_token                       Stop;
			std::chrono::steady_clock::time_point Deadline;
			github_mutation_transmission_witness  TransmissionWitness;
			bool                                  MutationPostCreated = false;
			std::optional<hop_result>             Result;
			std::atomic_bool                      Done = false;
		};

		enum class smart_request_surface : std::uint8_t {
			discovery,
			ls_refs,
			fetch,
		};

		struct smart_native_request
		{
			smart_request_surface                  Surface = smart_request_surface::discovery;
			application::git_smart_request_context Context;
			std::vector<std::byte>                 Body;
			std::uint64_t                          ResponseLimit = application::maximum_git_smart_control_response_bytes;
			application::git_smart_wire_consumer*  Consumer      = nullptr;
		};

		using smart_operation_result = application::git_smart_transport_result<std::vector<std::byte>>;

		struct smart_operation_state
		{
			smart_operation_state(
				smart_native_request                          aRequest,
				std::shared_ptr<native_git_smart_lease_state> aLease,
				const std::stop_token                         aStop,
				const std::chrono::steady_clock::time_point   aDeadline)
				: Request{ std::move(aRequest) }
				, Lease{ std::move(aLease) }
				, Stop{ aStop }
				, Deadline{ aDeadline }
			{
			}

			smart_native_request                          Request;
			std::shared_ptr<native_git_smart_lease_state> Lease;
			std::stop_token                               Stop;
			std::chrono::steady_clock::time_point         Deadline;
			std::optional<smart_operation_result>         Result;
			std::atomic_bool                              Done = false;
		};

		struct smart_hop_state
		{
			explicit smart_hop_state(
				std::shared_ptr<smart_operation_state> aOperation)
				: Operation{ std::move(aOperation) }
			{
			}

			std::shared_ptr<smart_operation_state> Operation;
			QNetworkReply*                         Reply                        = nullptr;
			bool                                   Cancellation                 = false;
			bool                                   Certificate                  = false;
			bool                                   EncryptedSignalObserved      = false;
			bool                                   Deadline                     = false;
			bool                                   IdleTimeout                  = false;
			bool                                   HeaderLimit                  = false;
			bool                                   ResponseLimit                = false;
			bool                                   Malformed                    = false;
			bool                                   ConsumerRejected             = false;
			bool                                   AuthenticationChallenge      = false;
			bool                                   ProxyAuthenticationChallenge = false;
			bool                                   PreSharedKeyChallenge        = false;
			bool                                   MetadataChecked              = false;
			int                                    Status                       = 0;
			std::optional<std::uint64_t>           ContentLength;
			std::uint64_t                          ReceivedBytes = 0U;
			std::vector<std::byte>                 Body;
		};

		struct hop_state
		{
			hop_state(std::shared_ptr<operation_state> aOperation,
					  const std::size_t                aResponseBodyLimit)
				: Operation{ std::move(aOperation) }
				, ResponseBodyLimit{ aResponseBodyLimit }
			{
			}

			std::shared_ptr<operation_state>          Operation;
			QNetworkReply*                            Reply                        = nullptr;
			bool                                      Cancellation                 = false;
			bool                                      Certificate                  = false;
			bool                                      EncryptedSignalObserved      = false;
			bool                                      Deadline                     = false;
			bool                                      HeaderLimit                  = false;
			bool                                      ResponseLimit                = false;
			bool                                      Malformed                    = false;
			bool                                      AuthenticationChallenge      = false;
			bool                                      ProxyAuthenticationChallenge = false;
			bool                                      PreSharedKeyChallenge        = false;
			bool                                      MetadataChecked              = false;
			int                                       Status                       = 0;
			std::optional<std::string>                ContentType;
			std::optional<std::uint64_t>              ContentLength;
			std::optional<std::string>                Link;
			std::optional<std::string>                RetryAfter;
			std::optional<std::string>                RateLimitLimit;
			std::optional<std::string>                RateLimitRemaining;
			std::optional<std::string>                RateLimitReset;
			std::optional<std::string>                RateLimitResource;
			std::string                               Body;
			std::size_t                               ResponseBodyLimit = native_https_maximum_response_body_bytes;
			application::github_capability_diagnostic CapabilityDetail  = application::github_capability_diagnostic::transport_invalid_metadata;
		};

		[[nodiscard]] auto provider_error(
			const application::github_provider_failure           aFailure,
			const std::optional<std::uint32_t>                   aRetryAfter     = std::nullopt,
			application::github_rate_limit_state                 aRateLimits     = { },
			std::optional<application::github_rate_limit_record> aRateLimitCause = std::nullopt) -> application::github_provider_error
		{
			return {
				.Failure                = aFailure,
				.RetryAfterSeconds      = aRetryAfter,
				.RateLimits             = std::move(aRateLimits),
				.RateLimitCause         = std::move(aRateLimitCause),
				.TransportFailureSource = application::diagnostic_transport_failure_source::native_transport,
			};
		}

		[[nodiscard]] constexpr auto smart_error(
			const application::git_smart_transport_failure aFailure) noexcept
			-> application::git_smart_transport_error
		{
			return { .Failure = aFailure };
		}

		[[nodiscard]] auto smart_failure(
			const application::git_smart_transport_failure aFailure)
			-> smart_operation_result
		{
			return std::unexpected{ smart_error(aFailure) };
		}

		[[nodiscard]] auto rate_limit_state(
			const std::optional<application::github_rate_limit_record>& aRecord) noexcept
			-> application::github_rate_limit_state
		{
			auto state = application::github_rate_limit_state{ };
			if (aRecord.has_value()) {
				static_cast<void>(application::observe(state, *aRecord));
			}
			return state;
		}

		[[nodiscard]] auto
		wire_error(const github_wire_correlation&             aCorrelation,
				   const application::github_provider_failure aFailure,
				   const std::optional<std::uint32_t>         aRetryAfter = std::nullopt)
			-> github_wire_completion
		{
			return github_wire_error{
				.Correlation = aCorrelation,
				.Error       = provider_error(aFailure, aRetryAfter),
			};
		}

		[[nodiscard]] auto unavailable(const github_wire_correlation& aCorrelation)
			-> github_wire_completion
		{
			return wire_error(aCorrelation,
							  application::github_provider_failure::unavailable);
		}

		[[nodiscard]] auto
		capability_error(const github_capability_probe_wire_correlation& aCorrelation,
						 const application::github_provider_error        aError,
						 const application::github_capability_diagnostic aDetail = application::github_capability_diagnostic::unavailable_detail)
			-> github_capability_probe_wire_completion
		{
			return github_capability_probe_wire_error{
				.Correlation      = aCorrelation,
				.Error            = aError,
				.CapabilityDetail = aDetail,
			};
		}

		[[nodiscard]] auto
		capability_error(const github_capability_probe_wire_correlation& aCorrelation,
						 const application::github_provider_failure      aFailure,
						 const std::optional<std::uint32_t>              aRetryAfter = std::nullopt)
			-> github_capability_probe_wire_completion
		{
			return capability_error(aCorrelation, provider_error(aFailure, aRetryAfter));
		}

		[[nodiscard]] auto capability_hop_error(
			const native_request_surface                    aSurface,
			const application::github_provider_failure      aFailure,
			const application::github_capability_diagnostic aDetail) -> hop_result
		{
			const auto error = provider_error(aFailure);
			if (aSurface == native_request_surface::capability_probe) {
				return hop_capability_failure{ .Error = error, .Detail = aDetail };
			}
			return error;
		}

		[[nodiscard]] auto native_request_from(const github_wire_request& aRequest)
			-> native_request
		{
			const auto resource = aRequest.Wire.Operation == read_operation::list_review_threads
				? application::github_rate_limit_resource::graphql
				: (aRequest.Wire.Operation == read_operation::search_pull_requests
						   || aRequest.Wire.Operation == read_operation::search_personal_pull_requests
					   ? application::github_rate_limit_resource::search
					   : application::github_rate_limit_resource::core);
			return {
				.Surface           = aRequest.Wire.Operation == read_operation::list_review_threads
					? native_request_surface::graphql_read
					: native_request_surface::rest_read,
				.RateLimitResource = resource,
				.Method            = aRequest.Wire.Method,
				.Path              = aRequest.Wire.Path,
				.Query             = aRequest.Wire.Query,
				.Headers           = aRequest.Wire.Headers,
				.Body              = aRequest.Wire.Body,
			};
		}

		[[nodiscard]] auto
		native_request_from(const github_capability_probe_wire_request& aRequest)
			-> native_request
		{
			auto headers = std::vector<http_header>{ };
			headers.reserve(aRequest.Wire.Headers.size());
			for (const auto& header : aRequest.Wire.Headers) {
				headers.push_back(http_header{
					.Name  = header.Name,
					.Value = header.Value,
				});
			}
			return {
				.Surface           = native_request_surface::capability_probe,
				.RateLimitResource = application::github_rate_limit_resource::graphql,
				.Method            = http_method::post,
				.Path              = aRequest.Wire.Path,
				.Query             = { },
				.Headers           = std::move(headers),
				.Body              = aRequest.Wire.Body,
			};
		}

		[[nodiscard]] auto
		native_request_from(const github_mutation_wire_request& aRequest,
							const review_mutation_exchange&     aExchange)
			-> native_request
		{
			return {
				.Surface                 = native_request_surface::review_mutation,
				.RateLimitResource       = aExchange.Graphql
					? application::github_rate_limit_resource::graphql
					: application::github_rate_limit_resource::core,
				.RequireRateLimitHeaders = true,
				.Method                  = aRequest.Wire.Method,
				.Path                    = aRequest.Wire.Path,
				.Query                   = { },
				.Headers                 = aRequest.Wire.Headers,
				.Body                    = aRequest.Wire.Body,
				.AdmittedContractStatus  = aRequest.Wire.Operation == review_mutation_operation::create_review_comment ? 422 : 0,
				.SuccessfulStatus        = aExchange.SuccessfulStatus,
			};
		}

		[[nodiscard]] auto
		native_request_from(const github_publication_wire_call& aCall,
							const publication_exchange&         aExchange)
			-> native_request
		{
			return {
				.Surface                 = native_request_surface::publication,
				.RateLimitResource       = application::github_rate_limit_resource::core,
				.RequireRateLimitHeaders = true,
				.Method                  = aCall.Wire.Method,
				.Path                    = aCall.Wire.Path,
				.Query                   = aCall.Wire.Query,
				.Headers                 = aCall.Wire.Headers,
				.Body                    = aCall.Wire.Body,
				.AdmittedContractStatus  = aExchange.AdmittedStatus,
				.SuccessfulStatus        = aExchange.SuccessfulStatus,
			};
		}

		[[nodiscard]] constexpr auto
		ascii_alpha_numeric(const unsigned char aValue) noexcept -> bool
		{
			return (aValue >= 'a' && aValue <= 'z') || (aValue >= 'A' && aValue <= 'Z')
				|| (aValue >= '0' && aValue <= '9');
		}

		[[nodiscard]] constexpr auto ascii_fold(const char aValue) noexcept -> char
		{
			return aValue >= 'A' && aValue <= 'Z' ? static_cast<char>(aValue + ('a' - 'A'))
												  : aValue;
		}

		[[nodiscard]] constexpr auto
		ascii_equal_fold(const std::string_view aLeft,
						 const std::string_view aRight) noexcept -> bool
		{
			if (aLeft.size() != aRight.size()) {
				return false;
			}
			for (std::size_t index = 0U; index < aLeft.size(); ++index) {
				if (ascii_fold(aLeft[index]) != ascii_fold(aRight[index])) {
					return false;
				}
			}
			return true;
		}

		[[nodiscard]] auto valid_bearer_material(
			const application::github_credential_material& aMaterial) noexcept -> bool
		{
			if (!application::valid(aMaterial)) {
				return false;
			}

			bool padding = false;
			for (const auto byte : aMaterial.bytes()) {
				const auto value = std::to_integer<unsigned char>(byte);
				if (value == '=') {
					padding = true;
					continue;
				}
				if (padding
					|| (!ascii_alpha_numeric(value) && value != '-' && value != '.'
						&& value != '_' && value != '~' && value != '+' && value != '/')) {
					return false;
				}
			}
			return true;
		}

		[[nodiscard]] auto
		profile_policy_valid(const validated_github_host_profile& aProfile) noexcept
			-> bool
		{
			return aProfile.authentication_transport()
				== application::github_authentication_transport::native_https
				&& aProfile.proxy_mode() == application::github_proxy_mode::system
				&& aProfile.trust_mode() == application::github_trust_mode::system;
		}

		[[nodiscard]] auto valid_request_shape(const native_request& aRequest) noexcept
			-> bool
		{
			if (!application::valid(aRequest.RateLimitResource)
				|| aRequest.Path.size() > maximum_url_bytes
				|| aRequest.Query.size() > maximum_url_bytes
				|| aRequest.Headers.size() > 8U
				|| aRequest.Body.size() > native_https_maximum_request_body_bytes) {
				return false;
			}

			std::size_t headerBytes = 0U;
			for (const auto& header : aRequest.Headers) {
				if (
					header.Name.empty() || header.Name.size() > 256U
					|| header.Value.size()
						> maximum_response_header_field_bytes
					|| header.Name.size()
						> native_https_maximum_response_header_bytes
							- headerBytes) {
					return false;
				}
				headerBytes += header.Name.size();
				if (
					header.Value.size()
					> native_https_maximum_response_header_bytes
						- headerBytes) {
					return false;
				}
				headerBytes += header.Value.size();
			}
			return true;
		}

		[[nodiscard]] auto authority_matches_profile(
			const application::github_credential_authority_key& aAuthority,
			const validated_github_host_profile&                aProfile) noexcept -> bool
		{
			return aAuthority.Profile == aProfile.profile()
				&& aAuthority.ProfileRevision == aProfile.revision()
				&& aAuthority.Host == aProfile.host()
				&& aAuthority.Hostname.value() == aProfile.hostname()
				&& aAuthority.ServerContract == aProfile.server_contract();
		}

#if defined(BPR_NATIVE_HTTPS_TESTING)
		[[nodiscard]] auto valid_test_configuration(
			const native_https_test_configuration& aConfiguration) noexcept -> bool
		{
			const auto loopback = aConfiguration.ProxyHost == "127.0.0.1"
				|| aConfiguration.ProxyHost == "::1";
			return loopback
				&& aConfiguration.ProxyPort != 0U
				&& (aConfiguration.FallbackProxyPort == 0U
					|| aConfiguration.FallbackProxyPort
						!= aConfiguration.ProxyPort)
				&& aConfiguration.TrustedCaCertificatePem.size()
				<= native_https_maximum_response_header_bytes
				&& aConfiguration.TotalTimeout >= minimum_test_timeout
				&& aConfiguration.TotalTimeout <= native_https_total_timeout
				&& aConfiguration.ResponseBodyLimit != 0U
				&& aConfiguration.ResponseBodyLimit
				<= native_https_maximum_response_body_bytes;
		}
#endif

		[[nodiscard]] auto text_is_bounded_header_value(
			const QByteArray& aValue) noexcept -> bool
		{
			if (
				aValue.size()
				> static_cast<qsizetype>(
					maximum_response_header_field_bytes)) {
				return false;
			}
			// A bare '\n' is not wire bytes: the protocol layer refuses
			// literal control bytes in a field, and Qt joins repeated
			// response headers into one rawHeaderPairs value with '\n' -
			// github.com's paired Set-Cookie headers arrive exactly so. The
			// join byte is Qt's API artifact and passes; NUL and '\r' never
			// have a legitimate source and still refuse.
			return std::ranges::none_of(aValue, [](const char aCharacter) {
				const auto byte = static_cast<unsigned char>(aCharacter);
				return byte == 0U || byte == '\r';
			});
		}

		[[nodiscard]] auto text_is_bounded_header_name(
			const QByteArray& aValue) noexcept -> bool
		{
			if (aValue.isEmpty() || aValue.size() > 256) {
				return false;
			}
			return std::ranges::all_of(aValue, [](const char aCharacter) {
				const auto byte = static_cast<unsigned char>(aCharacter);
				return ascii_alpha_numeric(byte) || byte == '-';
			});
		}

		[[nodiscard]] auto header_name_equal(
			const QByteArray& aActual,
			const char* const aExpected) noexcept -> bool
		{
			return aActual.compare(aExpected, Qt::CaseInsensitive) == 0;
		}

		[[nodiscard]] auto strict_decimal(
			const std::string_view aText) noexcept
			-> std::optional<std::uint64_t>
		{
			if (aText.empty()) {
				return std::nullopt;
			}
			std::uint64_t value  = 0U;
			const auto    parsed = std::from_chars(
				aText.data(),
				aText.data() + aText.size(),
				value);
			if (
				parsed.ec != std::errc{ }
				|| parsed.ptr != aText.data() + aText.size()) {
				return std::nullopt;
			}
			return value;
		}

		[[nodiscard]] constexpr auto rate_limit_resource_name(
			const application::github_rate_limit_resource aResource) noexcept
			-> std::string_view
		{
			switch (aResource) {
			case application::github_rate_limit_resource::core:
				return "core";
			case application::github_rate_limit_resource::search:
				return "search";
			case application::github_rate_limit_resource::graphql:
				return "graphql";
			case application::github_rate_limit_resource::uninitialized:
				return { };
			}
			return { };
		}

		[[nodiscard]] auto trimmed_ascii(const QByteArray& aValue) -> QByteArray
		{
			return aValue.trimmed();
		}

		[[nodiscard]] auto safe_qbytearray_string(
			const QByteArray& aValue) -> std::string
		{
			return {
				aValue.constData(),
				static_cast<std::size_t>(aValue.size()),
			};
		}

		void wipe(QByteArray& aBytes) noexcept
		{
			auto* cursor = reinterpret_cast<volatile unsigned char*>(aBytes.data());
			auto  size   = aBytes.size();
			while (size > 0) {
				*cursor = 0U;
				++cursor;
				--size;
			}
			aBytes.clear();
		}

		class qbytearray_wipe_guard final
		{
		public:
			explicit qbytearray_wipe_guard(QByteArray& aBytes) noexcept
				: Bytes{ aBytes }
			{
			}

			qbytearray_wipe_guard(const qbytearray_wipe_guard&) = delete;
			auto operator=(const qbytearray_wipe_guard&)
				-> qbytearray_wipe_guard& = delete;

			~qbytearray_wipe_guard()
			{
				wipe(Bytes);
			}

		private:
			QByteArray& Bytes;
		};

		[[nodiscard]] auto proxy_text_valid(
			const QString&  aValue,
			const qsizetype aMaximum) noexcept -> bool
		{
			if (aValue.size() > aMaximum) {
				return false;
			}
			return std::ranges::none_of(aValue, [](const QChar aCharacter) {
				return aCharacter.isNull()
					|| aCharacter == u'\r' || aCharacter == u'\n';
			});
		}

		[[nodiscard]] auto supported_proxy(
			const QNetworkProxy& aProxy) noexcept -> bool
		{
			switch (aProxy.type()) {
			case QNetworkProxy::NoProxy:
				return true;
			case QNetworkProxy::HttpProxy:
			case QNetworkProxy::Socks5Proxy:
				return aProxy.port() != 0U
					&& proxy_text_valid(aProxy.hostName(), 253)
					&& !aProxy.hostName().isEmpty()
					&& proxy_text_valid(aProxy.user(), 2'560)
					&& proxy_text_valid(aProxy.password(), 2'560);
			case QNetworkProxy::DefaultProxy:
			case QNetworkProxy::HttpCachingProxy:
			case QNetworkProxy::FtpCachingProxy:
				return false;
			}
			return false;
		}

		[[nodiscard]] auto system_proxy_candidates(const QUrl& aUrl)
			-> std::optional<std::vector<QNetworkProxy>>
		{
			const auto candidates = QNetworkProxyFactory::systemProxyForQuery(
				QNetworkProxyQuery{ aUrl, QNetworkProxyQuery::UrlRequest });
			if (
				candidates.isEmpty()
				|| candidates.size()
					> static_cast<qsizetype>(maximum_system_proxy_candidates)) {
				return std::nullopt;
			}

			std::vector<QNetworkProxy> supported;
			supported.reserve(static_cast<std::size_t>(candidates.size()));
			for (const auto& candidate : candidates) {
				if (supported_proxy(candidate)) {
					supported.push_back(candidate);
				}
			}
			if (supported.empty()) {
				return std::nullopt;
			}
			return supported;
		}

		[[nodiscard]] auto build_url(
			const validated_github_host_profile& aProfile,
			const native_request&                aRequest)
			-> std::optional<QUrl>
		{
			if (aRequest.Path.empty() || aRequest.Path.front() != '/'
				|| aRequest.Path.size() > maximum_url_bytes) {
				return std::nullopt;
			}
			if ((aRequest.Surface == native_request_surface::graphql_read
				 || aRequest.Surface == native_request_surface::capability_probe)
				&& aRequest.Path != aProfile.graphql_route()) {
				return std::nullopt;
			}

			std::string encoded = "https://";
			encoded += aProfile.rest_authority();
			encoded += aRequest.Path;
			if (!aRequest.Query.empty()) {
				encoded += '?';
				encoded += aRequest.Query;
			}
			if (encoded.size() > maximum_url_bytes) {
				return std::nullopt;
			}

			const auto bytes = QByteArray{ encoded.data(), static_cast<qsizetype>(encoded.size()) };
			auto       url   = QUrl::fromEncoded(bytes, QUrl::StrictMode);
			if (!url.isValid() || url.isRelative()
				|| url.scheme() != QStringLiteral("https")
				|| url.host().toUtf8() != aProfile.rest_authority() || url.port(-1) != -1
				|| !url.userInfo().isEmpty() || !url.fragment().isEmpty()
				|| url.toEncoded(QUrl::FullyEncoded) != bytes) {
				return std::nullopt;
			}
			return url;
		}

		[[nodiscard]] auto build_smart_url(
			const validated_github_host_profile& aProfile,
			const smart_native_request&          aRequest) -> std::optional<QUrl>
		{
			if (!core::valid(aRequest.Context.Repository)) {
				return std::nullopt;
			}

			std::string encoded = "https://";
			encoded += aProfile.hostname();
			encoded += '/';
			encoded += aRequest.Context.Repository.Owner;
			encoded += '/';
			encoded += aRequest.Context.Repository.Name;
			encoded += ".git/";
			if (aRequest.Surface == smart_request_surface::discovery) {
				encoded += "info/refs?service=git-upload-pack";
			} else {
				encoded += "git-upload-pack";
			}
			if (encoded.size() > maximum_url_bytes) {
				return std::nullopt;
			}

			const auto bytes = QByteArray{ encoded.data(), static_cast<qsizetype>(encoded.size()) };
			auto       url   = QUrl::fromEncoded(bytes, QUrl::StrictMode);
			if (!url.isValid() || url.isRelative()
				|| url.scheme() != QStringLiteral("https")
				|| url.host().toUtf8() != aProfile.hostname() || url.port(-1) != -1
				|| !url.userInfo().isEmpty() || !url.fragment().isEmpty()
				|| url.toEncoded(QUrl::FullyEncoded) != bytes) {
				return std::nullopt;
			}
			return url;
		}

		[[nodiscard]] constexpr auto device_authorization_route(
			const device_authorization_endpoint aEndpoint) noexcept
			-> std::string_view
		{
			return aEndpoint == device_authorization_endpoint::device_code
				? device_code_route
				: device_token_route;
		}

		[[nodiscard]] auto build_device_authorization_url(
			const validated_github_host_profile& aProfile,
			const native_request&                aRequest) -> std::optional<QUrl>
		{
			if (aProfile.server_contract()
					!= application::github_server_contract::github_com_2022_11_28
				|| !aRequest.Query.empty()
				|| (aRequest.Path != device_code_route
					&& aRequest.Path != device_token_route)) {
				return std::nullopt;
			}

			std::string encoded = "https://";
			encoded += aProfile.hostname();
			encoded += aRequest.Path;
			if (encoded.size() > maximum_url_bytes) {
				return std::nullopt;
			}

			const auto bytes = QByteArray{ encoded.data(), static_cast<qsizetype>(encoded.size()) };
			auto       url   = QUrl::fromEncoded(bytes, QUrl::StrictMode);
			if (!url.isValid() || url.isRelative()
				|| url.scheme() != QStringLiteral("https")
				|| url.host().toUtf8() != aProfile.hostname() || url.port(-1) != -1
				|| !url.userInfo().isEmpty() || !url.query().isEmpty()
				|| !url.fragment().isEmpty()
				|| url.toEncoded(QUrl::FullyEncoded) != bytes) {
				return std::nullopt;
			}
			return url;
		}

		[[nodiscard]] auto device_authorization_native_request(
			const validated_github_host_profile&     aProfile,
			const device_authorization_wire_request& aRequest)
			-> std::optional<native_request>
		{
			if (aRequest.Method != http_method::post || aRequest.Body.empty()) {
				return std::nullopt;
			}
			const auto route  = device_authorization_route(aRequest.Endpoint);
			auto       native = native_request{
				.Surface                 = native_request_surface::device_authorization,
				.RateLimitResource       = application::github_rate_limit_resource::core,
				.RequireRateLimitHeaders = false,
				.Method                  = http_method::post,
				.Path                    = std::string{ route },
				.Query                   = { },
				.Headers                 = aRequest.Headers,
				.Body                    = aRequest.Body,
				.ResponseLimit           = maximum_device_authorization_response_bytes,
				.AdmittedContractStatus  = aRequest.Endpoint
						== device_authorization_endpoint::device_code
					? device_code_refusal_status
					: 0,
			};
			const auto url = build_device_authorization_url(aProfile, native);
			if (!url.has_value()
				|| url->toEncoded(QUrl::FullyEncoded)
					!= QByteArray{ aRequest.Url.data(),
								   static_cast<qsizetype>(aRequest.Url.size()) }) {
				return std::nullopt;
			}
			return native;
		}

		[[nodiscard]] constexpr auto smart_expected_content_type(
			const smart_request_surface aSurface) noexcept -> std::string_view
		{
			return aSurface == smart_request_surface::discovery
				? smart_git_advertisement_content_type
				: smart_git_result_content_type;
		}

		[[nodiscard]] auto
		supported_json_content_type(const std::string_view aValue) noexcept -> bool
		{
			const auto separator = aValue.find(';');
			const auto media     = aValue.substr(0U, separator);
			if (!ascii_equal_fold(media, "application/json")) {
				return false;
			}
			if (separator == std::string_view::npos) {
				return true;
			}
			auto parameter = aValue.substr(separator + 1U);
			while (!parameter.empty() && parameter.front() == ' ') {
				parameter.remove_prefix(1U);
			}
			return ascii_equal_fold(parameter, "charset=utf-8");
		}

		[[nodiscard]] auto
		parse_link_completeness(const std::optional<std::string>& aLink) noexcept
			-> std::optional<page_completeness>
		{
			if (!aLink.has_value()) {
				return page_completeness::complete;
			}
			if (aLink->empty()) {
				return std::nullopt;
			}

			const auto text           = std::string_view{ *aLink };
			const auto tokenCharacter = [](const char aCharacter) noexcept {
				const auto value = static_cast<unsigned char>(aCharacter);
				return ascii_alpha_numeric(value) || aCharacter == '!'
					|| aCharacter == '#' || aCharacter == '$' || aCharacter == '%'
					|| aCharacter == '&' || aCharacter == '\'' || aCharacter == '*'
					|| aCharacter == '+' || aCharacter == '-' || aCharacter == '.'
					|| aCharacter == '^' || aCharacter == '_' || aCharacter == '`'
					|| aCharacter == '|' || aCharacter == '~';
			};
			const auto skipWhitespace = [&text](std::size_t& aPosition) noexcept {
				while (aPosition < text.size()
					   && (text[aPosition] == ' ' || text[aPosition] == '\t')) {
					++aPosition;
				}
			};
			const auto validRelation = [](const std::string_view aValue,
										  bool&                  aHasNext) noexcept {
				if (aValue.empty()) {
					return false;
				}
				std::size_t position = 0U;
				while (position < aValue.size()) {
					const auto separator = aValue.find(' ', position);
					const auto relation  = aValue.substr(position, separator == std::string_view::npos ? aValue.size() - position : separator - position);
					if (relation != "next" && relation != "prev" && relation != "first"
						&& relation != "last") {
						return false;
					}
					aHasNext = aHasNext || relation == "next";
					if (separator == std::string_view::npos) {
						break;
					}
					position = separator + 1U;
					if (position == aValue.size() || aValue[position] == ' ') {
						return false;
					}
				}
				return true;
			};

			bool        hasNext  = false;
			std::size_t position = 0U;
			while (position < text.size()) {
				skipWhitespace(position);
				if (position == text.size() || text[position] != '<') {
					return std::nullopt;
				}
				const auto targetEnd = text.find('>', position + 1U);
				if (targetEnd == std::string_view::npos || targetEnd == position + 1U) {
					return std::nullopt;
				}
				for (auto target = position + 1U; target < targetEnd; ++target) {
					const auto value = static_cast<unsigned char>(text[target]);
					if (value <= 0x20U || value >= 0x7fU || text[target] == '<'
						|| text[target] == '"') {
						return std::nullopt;
					}
				}
				position = targetEnd + 1U;

				bool relationSeen = false;
				while (true) {
					skipWhitespace(position);
					if (position == text.size() || text[position] == ',') {
						break;
					}
					if (text[position] != ';') {
						return std::nullopt;
					}
					++position;
					skipWhitespace(position);
					const auto nameBegin = position;
					while (position < text.size() && tokenCharacter(text[position])) {
						++position;
					}
					if (nameBegin == position) {
						return std::nullopt;
					}
					const auto name = text.substr(nameBegin, position - nameBegin);
					skipWhitespace(position);
					if (position == text.size() || text[position] != '=') {
						return std::nullopt;
					}
					++position;
					skipWhitespace(position);
					const auto quoted = position < text.size() && text[position] == '"';
					if (quoted) {
						++position;
					}
					const auto valueBegin = position;
					if (quoted) {
						while (position < text.size() && text[position] != '"') {
							const auto value = static_cast<unsigned char>(text[position]);
							if (value < 0x20U || value == 0x7fU
								|| text[position] == '\\') {
								return std::nullopt;
							}
							++position;
						}
						if (position == text.size()) {
							return std::nullopt;
						}
					} else {
						while (position < text.size() && tokenCharacter(text[position])) {
							++position;
						}
					}
					if (valueBegin == position) {
						return std::nullopt;
					}
					const auto value = text.substr(valueBegin, position - valueBegin);
					if (quoted) {
						++position;
					}
					if (name == "rel") {
						if (relationSeen || !validRelation(value, hasNext)) {
							return std::nullopt;
						}
						relationSeen = true;
					}
				}
				if (!relationSeen) {
					return std::nullopt;
				}
				if (position == text.size()) {
					break;
				}
				++position;
				if (position == text.size()) {
					return std::nullopt;
				}
			}
			return hasNext ? std::optional{ page_completeness::has_next_page }
						   : std::optional{ page_completeness::complete };
		}

		[[nodiscard]] auto
		parse_retry_after(const std::optional<std::string>& aValue) noexcept
			-> std::optional<std::uint32_t>
		{
			if (!aValue.has_value()) {
				return std::nullopt;
			}
			const auto parsed = strict_decimal(*aValue);
			if (!parsed.has_value() || *parsed > maximum_retry_after_seconds) {
				return std::nullopt;
			}
			return static_cast<std::uint32_t>(*parsed);
		}

		[[nodiscard]] auto allowlisted_success_headers(const hop_state& aState)
			-> std::vector<http_header>
		{
			auto headers = std::vector<http_header>{ };
			headers.reserve(7U);
			const auto add = [&headers](const char* const                 aName,
										const std::optional<std::string>& aValue) {
				if (aValue.has_value()) {
					headers.push_back(http_header{
						.Name  = aName,
						.Value = *aValue,
					});
				}
			};
			add("Content-Type", aState.ContentType);
			add("Link", aState.Link);
			add("X-RateLimit-Limit", aState.RateLimitLimit);
			add("X-RateLimit-Remaining", aState.RateLimitRemaining);
			add("X-RateLimit-Reset", aState.RateLimitReset);
			add("X-RateLimit-Resource", aState.RateLimitResource);
			add("Retry-After", aState.RetryAfter);
			return headers;
		}

		[[nodiscard]] auto classify_http_status(const hop_state& aState)
			-> application::github_provider_error
		{
			const auto remaining        = aState.RateLimitRemaining.has_value()
				? strict_decimal(*aState.RateLimitRemaining)
				: std::nullopt;
			const auto limit            = aState.RateLimitLimit.has_value()
				? strict_decimal(*aState.RateLimitLimit)
				: std::nullopt;
			const auto reset            = aState.RateLimitReset.has_value()
				? strict_decimal(*aState.RateLimitReset)
				: std::nullopt;
			const auto exhausted        = remaining.has_value() && *remaining == 0U;
			const auto retryAfter       = parse_retry_after(aState.RetryAfter);
			const auto secondaryMessage = (aState.Status == 403 || aState.Status == 429)
				&& aState.ContentType.has_value()
				&& supported_json_content_type(*aState.ContentType)
				&& github_error_body_is_secondary_rate_limit(
											  aState.Body, native_https_maximum_error_response_body_bytes);
			const auto structuredGraphqlRateLimit = aState.Status == 403
				&& aState.Operation->Request.Surface != native_request_surface::rest_read
				&& aState.ContentType.has_value()
				&& supported_json_content_type(*aState.ContentType)
				&& github_graphql_error_body_is_rate_limited(
														aState.Body, native_https_maximum_error_response_body_bytes);
			const auto secondary = secondaryMessage || structuredGraphqlRateLimit;
			auto       rateLimit = limit.has_value() && remaining.has_value() && reset.has_value()
					&& aState.RateLimitResource.has_value() && *remaining <= *limit
				? std::optional<
					  application::
						  github_rate_limit_record>{ application::
														 github_rate_limit_record{
															 .Resource         = aState.Operation
																					 ->Request
																					 .RateLimitResource,
															 .Limit            = *limit,
															 .Remaining        = *remaining,
															 .ResetUnixSeconds = *reset,
															 .SecondaryLimited = false,
														 } }
				: std::nullopt;
			auto       failure   = application::github_provider_failure::malformed_response;
			switch (aState.Status) {
			case 401:
				failure = application::github_provider_failure::authentication;
				break;
			case 403:
				failure = exhausted || retryAfter.has_value() || secondary
					? application::github_provider_failure::rate_limit
					: application::github_provider_failure::permission;
				break;
			case 404:
				failure = application::github_provider_failure::permission;
				break;
			case 406:
			case 415:
				failure = application::github_provider_failure::api_version;
				break;
			case 408:
			case 504:
				failure = application::github_provider_failure::timeout;
				break;
			case 429:
				failure = application::github_provider_failure::rate_limit;
				break;
			default:
				if (aState.Status >= 500 && aState.Status <= 599) {
					failure = application::github_provider_failure::unavailable;
				}
				break;
			}
			if (rateLimit.has_value()
				&& failure == application::github_provider_failure::rate_limit
				&& rateLimit->Remaining != 0U) {
				rateLimit->SecondaryLimited = aState.Status == 429 || secondary
					|| (aState.Status == 403 && retryAfter.has_value());
			}
			auto rateLimits = rate_limit_state(rateLimit);
			auto cause      = failure == application::github_provider_failure::rate_limit
				? rateLimit
				: std::optional<application::github_rate_limit_record>{ };
			if (failure == application::github_provider_failure::rate_limit
				&& !cause.has_value()) {
				return provider_error(
					application::github_provider_failure::malformed_response,
					std::nullopt, std::move(rateLimits));
			}
			if (cause.has_value()) {
				static_cast<void>(application::observe(rateLimits, *cause));
			}
			return provider_error(
				failure,
				failure == application::github_provider_failure::rate_limit
					? (retryAfter.has_value()
						   ? retryAfter
						   : (cause.has_value() && cause->SecondaryLimited && !exhausted
								  ? std::optional<std::uint32_t>{ 60U }
								  : std::nullopt))
					: (failure == application::github_provider_failure::unavailable
						   ? retryAfter
						   : std::nullopt),
				std::move(rateLimits), std::move(cause));
		}

		[[nodiscard]] auto recognized_redirect_status(const int aStatus) noexcept
			-> bool
		{
			return aStatus == 301 || aStatus == 302 || aStatus == 303
				|| aStatus == 307 || aStatus == 308;
		}

		[[nodiscard]] constexpr auto smart_redirect_status(const int aStatus) noexcept
			-> bool
		{
			return aStatus >= 300 && aStatus <= 399;
		}

		[[nodiscard]] auto redirect_target(
			QNetworkReply& aReply,
			const QUrl&    aSource) -> std::optional<std::string>
		{
			const auto attribute = aReply.attribute(QNetworkRequest::RedirectionTargetAttribute);
			if (!attribute.isValid() || !attribute.canConvert<QUrl>()) {
				return std::nullopt;
			}
			const auto relative = attribute.toUrl();
			if (!relative.isValid() || !relative.fragment().isEmpty()) {
				return std::nullopt;
			}
			const auto resolved = aSource.resolved(relative);
			const auto encoded  = resolved.toEncoded(QUrl::FullyEncoded);
			if (
				!resolved.isValid() || resolved.isRelative()
				|| encoded.isEmpty() || encoded.size() > 2 * 1024) {
				return std::nullopt;
			}
			return safe_qbytearray_string(encoded);
		}

		void abort_hop(hop_state& aState) noexcept
		{
			if (aState.Reply != nullptr && aState.Reply->isRunning()) {
				aState.Reply->abort();
			}
		}

		class active_hop_guard final
		{
		public:
			active_hop_guard(
				hop_state&  aState,
				hop_state*& aCtive) noexcept
				: State{ aState }
				, Active{ aCtive }
			{
				Active = &State;
			}

			active_hop_guard(const active_hop_guard&) = delete;
			auto operator=(const active_hop_guard&)
				-> active_hop_guard& = delete;

			~active_hop_guard()
			{
				reset();
			}

			void reset() noexcept
			{
				if (Active == &State) {
					Active = nullptr;
				}
				auto* const reply = std::exchange(State.Reply, nullptr);
				if (reply == nullptr) {
					return;
				}
				if (reply->isRunning()) {
					reply->abort();
				}
				reply->deleteLater();
				QCoreApplication::sendPostedEvents(
					reply,
					QEvent::DeferredDelete);
			}

		private:
			hop_state&  State;
			hop_state*& Active;
		};

		class active_smart_hop_guard final
		{
		public:
			active_smart_hop_guard(
				smart_hop_state&  aState,
				smart_hop_state*& aCtive) noexcept
				: State{ aState }
				, Active{ aCtive }
			{
				Active = &State;
			}

			active_smart_hop_guard(const active_smart_hop_guard&) = delete;
			auto operator=(const active_smart_hop_guard&)
				-> active_smart_hop_guard& = delete;

			~active_smart_hop_guard()
			{
				reset();
			}

			void reset() noexcept
			{
				if (Active == &State) {
					Active = nullptr;
				}
				auto* const reply = std::exchange(State.Reply, nullptr);
				if (reply == nullptr) {
					return;
				}
				if (reply->isRunning()) {
					reply->abort();
				}
				reply->deleteLater();
				QCoreApplication::sendPostedEvents(reply, QEvent::DeferredDelete);
			}

		private:
			smart_hop_state&  State;
			smart_hop_state*& Active;
		};

		void abort_smart_hop(smart_hop_state& aState) noexcept
		{
			if (aState.Reply != nullptr && aState.Reply->isRunning()) {
				aState.Reply->abort();
			}
		}

		void inspect_metadata_impl(hop_state& aState)
		{
			if (aState.MetadataChecked || aState.Reply == nullptr) {
				return;
			}
			const auto status = aState.Reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
			if (!status.isValid()) {
				if (
					aState.Reply->isFinished()
					&& aState.Reply->error() == QNetworkReply::NoError) {
					aState.MetadataChecked = true;
					aState.Malformed       = true;
				}
				return;
			}
			aState.MetadataChecked = true;
			aState.Status          = status.toInt();
			if (aState.Status < 100 || aState.Status > 599) {
				aState.Malformed = true;
				abort_hop(aState);
				return;
			}

			const auto& headers = aState.Reply->rawHeaderPairs();
			if (
				static_cast<std::size_t>(headers.size())
				> maximum_response_header_count) {
				aState.HeaderLimit = true;
				abort_hop(aState);
				return;
			}

			std::size_t aggregate            = 0U;
			std::size_t contentTypeCount     = 0U;
			std::size_t contentLengthCount   = 0U;
			std::size_t contentEncodingCount = 0U;
			std::size_t linkCount            = 0U;
			std::size_t locationCount        = 0U;
			std::size_t retryAfterCount      = 0U;
			std::size_t rateLimitCount       = 0U;
			std::size_t rateRemainingCount   = 0U;
			std::size_t rateResetCount       = 0U;
			std::size_t rateResourceCount    = 0U;
			for (const auto& [name, value] : headers) {
				const auto fieldSize = static_cast<std::size_t>(name.size())
					+ static_cast<std::size_t>(value.size());
				if (!text_is_bounded_header_name(name)
					|| !text_is_bounded_header_value(value)
					|| fieldSize
						> native_https_maximum_response_header_bytes - aggregate) {
					aState.HeaderLimit = true;
					abort_hop(aState);
					return;
				}
				aggregate += fieldSize;

				if (header_name_equal(name, "Content-Type")) {
					++contentTypeCount;
					aState.ContentType = safe_qbytearray_string(trimmed_ascii(value));
				} else if (header_name_equal(name, "Content-Length")) {
					++contentLengthCount;
					const auto text   = safe_qbytearray_string(trimmed_ascii(value));
					const auto parsed = strict_decimal(text);
					if (!parsed.has_value()) {
						aState.Malformed = true;
					} else if (
						*parsed
						> (aState.Status == 200
							   ? aState.ResponseBodyLimit
							   : std::min(
									 aState.ResponseBodyLimit,
									 native_https_maximum_error_response_body_bytes))) {
						aState.ResponseLimit = true;
					} else {
						aState.ContentLength = parsed;
					}
				} else if (header_name_equal(name, "Content-Encoding")) {
					++contentEncodingCount;
				} else if (header_name_equal(name, "Link")) {
					++linkCount;
					aState.Link = safe_qbytearray_string(trimmed_ascii(value));
				} else if (header_name_equal(name, "Location")) {
					++locationCount;
				} else if (header_name_equal(name, "Retry-After")) {
					++retryAfterCount;
					aState.RetryAfter = safe_qbytearray_string(trimmed_ascii(value));
				} else if (header_name_equal(name, "X-RateLimit-Limit")) {
					++rateLimitCount;
					aState.RateLimitLimit = safe_qbytearray_string(trimmed_ascii(value));
				} else if (header_name_equal(name, "X-RateLimit-Remaining")) {
					++rateRemainingCount;
					aState.RateLimitRemaining = safe_qbytearray_string(trimmed_ascii(value));
				} else if (header_name_equal(name, "X-RateLimit-Reset")) {
					++rateResetCount;
					aState.RateLimitReset = safe_qbytearray_string(trimmed_ascii(value));
				} else if (header_name_equal(name, "X-RateLimit-Resource")) {
					++rateResourceCount;
					aState.RateLimitResource = safe_qbytearray_string(trimmed_ascii(value));
				}
			}

			if (contentTypeCount > 1U || contentLengthCount > 1U
				|| contentEncodingCount != 0U || linkCount > 1U || locationCount > 1U
				|| (locationCount != 0U && aState.Status == 422
					&& aState.Operation->Request.AdmittedContractStatus == 422)
				|| retryAfterCount > 1U || rateLimitCount > 1U
				|| rateRemainingCount > 1U || rateResetCount > 1U
				|| rateResourceCount > 1U
				|| (linkCount != 0U
					&& aState.Operation->Request.Surface
						!= native_request_surface::rest_read
					&& aState.Operation->Request.Surface
						!= native_request_surface::publication)
				|| (aState.RetryAfter.has_value()
					&& !parse_retry_after(aState.RetryAfter).has_value())
				|| (aState.RateLimitLimit.has_value()
					&& !strict_decimal(*aState.RateLimitLimit).has_value())
				|| (aState.RateLimitRemaining.has_value()
					&& !strict_decimal(*aState.RateLimitRemaining).has_value())
				|| (aState.RateLimitReset.has_value()
					&& !strict_decimal(*aState.RateLimitReset).has_value())) {
				aState.Malformed        = true;
				aState.CapabilityDetail = contentEncodingCount != 0U
					? application::github_capability_diagnostic::transport_content_encoding
					: (contentTypeCount > 1U || contentLengthCount > 1U || linkCount > 1U
					   || locationCount > 1U || retryAfterCount > 1U || rateLimitCount > 1U
					   || rateRemainingCount > 1U || rateResetCount > 1U || rateResourceCount > 1U)
					? application::github_capability_diagnostic::transport_duplicate_metadata
					: application::github_capability_diagnostic::transport_invalid_metadata;
			}
			const auto rateTupleCount = static_cast<std::size_t>(aState.RateLimitLimit.has_value())
				+ static_cast<std::size_t>(aState.RateLimitRemaining.has_value())
				+ static_cast<std::size_t>(aState.RateLimitReset.has_value())
				+ static_cast<std::size_t>(aState.RateLimitResource.has_value());
			const auto redirectStatus = aState.Status == 301 || aState.Status == 302
				|| aState.Status == 303 || aState.Status == 307
				|| aState.Status == 308;
			if ((rateTupleCount != 0U && rateTupleCount != 4U)
				|| (rateTupleCount == 0U
					&& aState.Operation->Request.RequireRateLimitHeaders
					&& !redirectStatus)
				|| (rateTupleCount == 4U
					&& *aState.RateLimitResource
						!= rate_limit_resource_name(
							aState.Operation->Request.RateLimitResource))) {
				if (!aState.Malformed) {
					aState.CapabilityDetail = rateTupleCount == 0U
						? application::github_capability_diagnostic::transport_missing_rate_tuple
						: rateTupleCount != 4U
						? application::github_capability_diagnostic::transport_partial_rate_tuple
						: application::github_capability_diagnostic::transport_wrong_rate_resource;
				}
				aState.Malformed = true;
			}
			const auto rateLimit     = aState.RateLimitLimit.has_value()
				? strict_decimal(*aState.RateLimitLimit)
				: std::optional<std::uint64_t>{ };
			const auto rateRemaining = aState.RateLimitRemaining.has_value()
				? strict_decimal(*aState.RateLimitRemaining)
				: std::optional<std::uint64_t>{ };
			if (rateLimit.has_value() && rateRemaining.has_value()
				&& *rateRemaining > *rateLimit) {
				aState.Malformed = true;
			}
			if (aState.Malformed || aState.ResponseLimit || aState.HeaderLimit) {
				abort_hop(aState);
				return;
			}
		}

		void inspect_metadata(hop_state& aState) noexcept
		{
			try {
				inspect_metadata_impl(aState);
			} catch (...) {
				aState.ResponseLimit = true;
				abort_hop(aState);
			}
		}

		void read_available(hop_state& aState) noexcept
		{
			try {
				if (aState.Reply == nullptr || aState.ResponseLimit || aState.Malformed
					|| aState.HeaderLimit) {
					return;
				}
				inspect_metadata(aState);
				if (aState.Reply == nullptr) {
					return;
				}
				const auto limit = aState.Status == 200
					? aState.ResponseBodyLimit
					: std::min(aState.ResponseBodyLimit,
							   native_https_maximum_error_response_body_bytes);

				std::array<char, response_read_chunk_bytes> buffer{ };
				while (aState.Reply->bytesAvailable() > 0) {
					const auto ceiling = limit + 1U;
					if (aState.Body.size() >= ceiling) {
						aState.ResponseLimit = true;
						abort_hop(aState);
						return;
					}
					const auto remaining = ceiling - aState.Body.size();
					const auto requested = static_cast<qint64>(
						std::min(remaining, buffer.size()));
					const auto count = aState.Reply->read(buffer.data(), requested);
					if (count <= 0) {
						return;
					}
					aState.Body.append(
						buffer.data(),
						static_cast<std::size_t>(count));
					if (aState.Body.size() > limit) {
						aState.ResponseLimit = true;
						abort_hop(aState);
						return;
					}
				}
			} catch (...) {
				aState.ResponseLimit = true;
				abort_hop(aState);
			}
		}

		void inspect_smart_metadata_impl(smart_hop_state& aState)
		{
			if (aState.MetadataChecked || aState.Reply == nullptr) {
				return;
			}
			const auto status = aState.Reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
			if (!status.isValid()) {
				if (aState.Reply->isFinished()
					&& aState.Reply->error() == QNetworkReply::NoError) {
					aState.MetadataChecked = true;
					aState.Malformed       = true;
				}
				return;
			}
			aState.MetadataChecked = true;
			aState.Status          = status.toInt();
			if (aState.Status < 100 || aState.Status > 599) {
				aState.Malformed = true;
				abort_smart_hop(aState);
				return;
			}

			const auto& headers = aState.Reply->rawHeaderPairs();
			if (static_cast<std::size_t>(headers.size())
				> maximum_response_header_count) {
				aState.HeaderLimit = true;
				abort_smart_hop(aState);
				return;
			}

			std::size_t                aggregate            = 0U;
			std::size_t                contentTypeCount     = 0U;
			std::size_t                contentLengthCount   = 0U;
			std::size_t                contentEncodingCount = 0U;
			std::size_t                locationCount        = 0U;
			std::optional<std::string> contentType;
			for (const auto& [name, value] : headers) {
				const auto fieldSize = static_cast<std::size_t>(name.size())
					+ static_cast<std::size_t>(value.size());
				if (!text_is_bounded_header_name(name)
					|| !text_is_bounded_header_value(value)
					|| fieldSize
						> native_https_maximum_response_header_bytes - aggregate) {
					aState.HeaderLimit = true;
					abort_smart_hop(aState);
					return;
				}
				aggregate += fieldSize;

				if (header_name_equal(name, "Content-Type")) {
					++contentTypeCount;
					contentType = safe_qbytearray_string(trimmed_ascii(value));
				} else if (header_name_equal(name, "Content-Length")) {
					++contentLengthCount;
					const auto parsed = strict_decimal(
						safe_qbytearray_string(trimmed_ascii(value)));
					if (!parsed.has_value()) {
						aState.Malformed = true;
					} else if (*parsed > aState.Operation->Request.ResponseLimit) {
						aState.ResponseLimit = true;
					} else {
						aState.ContentLength = parsed;
					}
				} else if (header_name_equal(name, "Content-Encoding")) {
					++contentEncodingCount;
				} else if (header_name_equal(name, "Location")) {
					++locationCount;
				}
			}

			const auto redirect = smart_redirect_status(aState.Status);
			if (contentTypeCount > 1U || contentLengthCount > 1U
				|| contentEncodingCount != 0U || locationCount > 1U) {
				aState.Malformed = true;
			}
			if (aState.Status == 200
				&& (!contentType.has_value()
					|| *contentType
						!= smart_expected_content_type(
							aState.Operation->Request.Surface))) {
				aState.Malformed = true;
			}
			if (aState.HeaderLimit || aState.ResponseLimit || aState.Malformed || redirect
				|| aState.Status != 200) {
				abort_smart_hop(aState);
			}
		}

		void inspect_smart_metadata(smart_hop_state& aState) noexcept
		{
			try {
				inspect_smart_metadata_impl(aState);
			} catch (...) {
				aState.ResponseLimit = true;
				abort_smart_hop(aState);
			}
		}

		void read_smart_available(smart_hop_state& aState, QTimer& aIdle) noexcept
		{
			try {
				if (aState.Reply == nullptr || aState.ResponseLimit || aState.Malformed
					|| aState.HeaderLimit || aState.ConsumerRejected) {
					return;
				}
				inspect_smart_metadata(aState);
				if (aState.Reply == nullptr || aState.Status != 200) {
					return;
				}

				std::array<std::byte, response_read_chunk_bytes> buffer{ };
				while (aState.Reply->bytesAvailable() > 0) {
					const auto remaining = aState.Operation->Request.ResponseLimit
						- aState.ReceivedBytes;
					if (remaining == 0U) {
						aState.ResponseLimit = true;
						abort_smart_hop(aState);
						return;
					}
					const auto requested = static_cast<qint64>(
						std::min<std::uint64_t>(remaining, buffer.size()));
					const auto count = aState.Reply->read(
						reinterpret_cast<char*>(buffer.data()), requested);
					if (count <= 0) {
						return;
					}
					const auto byteCount = static_cast<std::size_t>(count);
					if (aState.ReceivedBytes
						> aState.Operation->Request.ResponseLimit - byteCount) {
						aState.ResponseLimit = true;
						abort_smart_hop(aState);
						return;
					}

					const auto bytes = std::span<const std::byte>{ buffer.data(), byteCount };
					if (aState.Operation->Request.Surface
						== smart_request_surface::fetch) {
						if (aState.Operation->Request.Consumer == nullptr
							|| !aState.Operation->Request.Consumer->consume(bytes)) {
							aState.ConsumerRejected = true;
							abort_smart_hop(aState);
							return;
						}
					} else {
						const auto offset = aState.Body.size();
						aState.Body.resize(offset + byteCount);
						std::memcpy(aState.Body.data() + offset, buffer.data(),
									byteCount);
					}
					aState.ReceivedBytes += byteCount;
					aIdle.start(
						std::chrono::duration_cast<std::chrono::milliseconds>(
							smart_idle_timeout));
				}
			} catch (...) {
				aState.ResponseLimit = true;
				abort_smart_hop(aState);
			}
		}

		[[nodiscard]] auto cancellation_result(
			const std::shared_ptr<operation_state>& aOperation) noexcept -> bool
		{
			return aOperation->Stop.stop_requested();
		}

		[[nodiscard]] auto cancellation_result(
			const std::shared_ptr<smart_operation_state>& aOperation) noexcept -> bool
		{
			return aOperation->Stop.stop_requested()
				|| aOperation->Lease == nullptr
				|| aOperation->Lease->Revoked.load(std::memory_order_acquire);
		}

		[[nodiscard]] auto remaining_before_deadline(
			const std::chrono::steady_clock::time_point aDeadline) noexcept
			-> std::optional<std::chrono::milliseconds>
		{
			const auto now = std::chrono::steady_clock::now();
			if (now >= aDeadline) {
				return std::nullopt;
			}
			return std::chrono::ceil<std::chrono::milliseconds>(aDeadline - now);
		}

		[[nodiscard]] auto tls_terminal_valid(
			QNetworkReply& aReply,
			const QUrl&    aRequested,
			const bool     aEncryptedSignalObserved) -> bool
		{
			if (
				!aEncryptedSignalObserved
				|| !aReply.attribute(QNetworkRequest::ConnectionEncryptedAttribute)
						.toBool()
				|| aReply.attribute(QNetworkRequest::Http2WasUsedAttribute).toBool()
				|| aReply.url() != aRequested) {
				return false;
			}
			const auto configuration = aReply.sslConfiguration();
			return !configuration.peerCertificate().isNull()
				&& configuration.peerVerifyMode() == QSslSocket::VerifyPeer
				&& configuration.protocol() == QSsl::TlsV1_2OrLater;
		}

		[[nodiscard]] auto network_failure(
			const QNetworkReply::NetworkError aError) noexcept
			-> application::github_provider_failure
		{
			switch (aError) {
			case QNetworkReply::TimeoutError:
			case QNetworkReply::ProxyTimeoutError:
				return application::github_provider_failure::timeout;
			case QNetworkReply::AuthenticationRequiredError:
				return application::github_provider_failure::authentication;
			case QNetworkReply::ContentAccessDenied:
			case QNetworkReply::ContentOperationNotPermittedError:
				return application::github_provider_failure::permission;
			case QNetworkReply::SslHandshakeFailedError:
				return application::github_provider_failure::certificate;
			case QNetworkReply::NoError:
			case QNetworkReply::ConnectionRefusedError:
			case QNetworkReply::RemoteHostClosedError:
			case QNetworkReply::HostNotFoundError:
			case QNetworkReply::OperationCanceledError:
			case QNetworkReply::TemporaryNetworkFailureError:
			case QNetworkReply::NetworkSessionFailedError:
			case QNetworkReply::BackgroundRequestNotAllowedError:
			case QNetworkReply::TooManyRedirectsError:
			case QNetworkReply::InsecureRedirectError:
			case QNetworkReply::UnknownNetworkError:
			case QNetworkReply::ProxyConnectionRefusedError:
			case QNetworkReply::ProxyConnectionClosedError:
			case QNetworkReply::ProxyNotFoundError:
			case QNetworkReply::ProxyAuthenticationRequiredError:
			case QNetworkReply::UnknownProxyError:
			case QNetworkReply::ContentNotFoundError:
			case QNetworkReply::ContentReSendError:
			case QNetworkReply::ContentConflictError:
			case QNetworkReply::ContentGoneError:
			case QNetworkReply::UnknownContentError:
			case QNetworkReply::ProtocolUnknownError:
			case QNetworkReply::ProtocolInvalidOperationError:
			case QNetworkReply::ProtocolFailure:
			case QNetworkReply::InternalServerError:
			case QNetworkReply::OperationNotImplementedError:
			case QNetworkReply::ServiceUnavailableError:
			case QNetworkReply::UnknownServerError:
				return application::github_provider_failure::network;
			}
			return application::github_provider_failure::network;
		}

		[[nodiscard]] auto smart_network_failure(
			const QNetworkReply::NetworkError aError) noexcept
			-> application::git_smart_transport_failure
		{
			switch (aError) {
			case QNetworkReply::TimeoutError:
			case QNetworkReply::ProxyTimeoutError:
				return application::git_smart_transport_failure::timeout;
			case QNetworkReply::AuthenticationRequiredError:
				return application::git_smart_transport_failure::authentication;
			case QNetworkReply::ContentAccessDenied:
			case QNetworkReply::ContentOperationNotPermittedError:
				return application::git_smart_transport_failure::permission;
			case QNetworkReply::SslHandshakeFailedError:
				return application::git_smart_transport_failure::certificate;
			case QNetworkReply::ProxyConnectionRefusedError:
			case QNetworkReply::ProxyConnectionClosedError:
			case QNetworkReply::ProxyNotFoundError:
			case QNetworkReply::ProxyAuthenticationRequiredError:
			case QNetworkReply::UnknownProxyError:
				return application::git_smart_transport_failure::proxy;
			default:
				return application::git_smart_transport_failure::network;
			}
		}

		[[nodiscard]] constexpr auto smart_http_failure(const int aStatus) noexcept
			-> application::git_smart_transport_failure
		{
			switch (aStatus) {
			case 401:
				return application::git_smart_transport_failure::authentication;
			case 403:
			case 404:
				return application::git_smart_transport_failure::permission;
			case 408:
			case 504:
				return application::git_smart_transport_failure::timeout;
			case 429:
				return application::git_smart_transport_failure::unavailable;
			default:
				return aStatus >= 500 && aStatus <= 599
					? application::git_smart_transport_failure::unavailable
					: application::git_smart_transport_failure::malformed_response;
			}
		}

		[[nodiscard]] auto proxy_candidate_retryable(
			const QNetworkReply::NetworkError aError) noexcept -> bool
		{
			switch (aError) {
			case QNetworkReply::ConnectionRefusedError:
			case QNetworkReply::RemoteHostClosedError:
			case QNetworkReply::HostNotFoundError:
			case QNetworkReply::TimeoutError:
			case QNetworkReply::OperationCanceledError:
			case QNetworkReply::TemporaryNetworkFailureError:
			case QNetworkReply::NetworkSessionFailedError:
			case QNetworkReply::BackgroundRequestNotAllowedError:
			case QNetworkReply::UnknownNetworkError:
			case QNetworkReply::ProxyConnectionRefusedError:
			case QNetworkReply::ProxyConnectionClosedError:
			case QNetworkReply::ProxyNotFoundError:
			case QNetworkReply::ProxyTimeoutError:
			case QNetworkReply::ProxyAuthenticationRequiredError:
			case QNetworkReply::UnknownProxyError:
				return true;
			case QNetworkReply::NoError:
			case QNetworkReply::SslHandshakeFailedError:
			case QNetworkReply::TooManyRedirectsError:
			case QNetworkReply::InsecureRedirectError:
			case QNetworkReply::AuthenticationRequiredError:
			case QNetworkReply::ContentAccessDenied:
			case QNetworkReply::ContentOperationNotPermittedError:
			case QNetworkReply::ContentNotFoundError:
			case QNetworkReply::ContentReSendError:
			case QNetworkReply::ContentConflictError:
			case QNetworkReply::ContentGoneError:
			case QNetworkReply::UnknownContentError:
			case QNetworkReply::ProtocolUnknownError:
			case QNetworkReply::ProtocolInvalidOperationError:
			case QNetworkReply::ProtocolFailure:
			case QNetworkReply::InternalServerError:
			case QNetworkReply::OperationNotImplementedError:
			case QNetworkReply::ServiceUnavailableError:
			case QNetworkReply::UnknownServerError:
				return false;
			}
			return false;
		}

		[[nodiscard]] auto read_completion(const github_wire_correlation& aCorrelation,
										   hop_result                     aResult) -> github_wire_completion
		{
			if (auto* success = std::get_if<hop_success>(&aResult); success != nullptr) {
				return github_wire_success{
					.Correlation = aCorrelation,
					.Response    = wire_response{
						.Status       = 200,
						.Headers      = std::move(success->Headers),
						.Body         = std::move(success->Body),
						.Completeness = success->Completeness,
					},
				};
			}
			if (auto* error = std::get_if<application::github_provider_error>(&aResult);
				error != nullptr) {
				return github_wire_error{
					.Correlation = aCorrelation,
					.Error       = *error,
				};
			}
			if (std::holds_alternative<hop_cancelled>(aResult)) {
				return github_wire_cancelled{ .Correlation = aCorrelation };
			}
			return wire_error(aCorrelation,
							  application::github_provider_failure::redirect);
		}

		[[nodiscard]] auto capability_completion(
			const github_capability_probe_wire_correlation& aCorrelation,
			hop_result                                      aResult) -> github_capability_probe_wire_completion
		{
			if (auto* success = std::get_if<hop_success>(&aResult); success != nullptr) {
				auto response = capability_probe_response{
					.Status               = 200,
					.Authenticated        = true,
					.ContentType          = { },
					.Body                 = std::move(success->Body),
					.RateLimitHeaders     = { },
					.RateLimitHeaderCount = 0U,
				};
				for (auto& header : success->Headers) {
					if (header.Name == "Content-Type") {
						response.ContentType = std::move(header.Value);
						continue;
					}
					if (response.RateLimitHeaderCount
						>= response.RateLimitHeaders.size()) {
						return capability_error(
							aCorrelation,
							provider_error(application::github_provider_failure::malformed_response), application::github_capability_diagnostic::transport_header_limit);
					}
					response.RateLimitHeaders[response.RateLimitHeaderCount] = capability_probe_header{
						.Name  = std::move(header.Name),
						.Value = std::move(header.Value),
					};
					++response.RateLimitHeaderCount;
				}
				if (response.ContentType.empty()) {
					return capability_error(
						aCorrelation,
						provider_error(application::github_provider_failure::malformed_response), application::github_capability_diagnostic::transport_invalid_content_type);
				}
				return github_capability_probe_wire_success{
					.Correlation = aCorrelation,
					.Response    = std::move(response),
					.Summary     = success->CapabilitySummary,
				};
			}
			if (auto* error = std::get_if<hop_capability_failure>(&aResult)) {
				return capability_error(aCorrelation, error->Error, error->Detail);
			}
			if (auto* error = std::get_if<application::github_provider_error>(&aResult);
				error != nullptr) {
				return capability_error(aCorrelation, *error);
			}
			if (std::holds_alternative<hop_cancelled>(aResult)) {
				return github_capability_probe_wire_cancelled{
					.Correlation = aCorrelation,
				};
			}
			return capability_error(aCorrelation,
									application::github_provider_failure::redirect);
		}

		[[nodiscard]] auto mutation_failure(
			const application::github_provider_failure aFailure) noexcept
			-> application::review_mutation_failure
		{
			switch (aFailure) {
			case application::github_provider_failure::authentication:
			case application::github_provider_failure::permission:
			case application::github_provider_failure::api_version:
			case application::github_provider_failure::unsupported_capability:
				return application::review_mutation_failure::refused_by_server;
			case application::github_provider_failure::malformed_response:
				return application::review_mutation_failure::malformed_response;
			case application::github_provider_failure::network:
			case application::github_provider_failure::rate_limit:
			case application::github_provider_failure::certificate:
			case application::github_provider_failure::redirect:
			case application::github_provider_failure::timeout:
			case application::github_provider_failure::process_startup:
			case application::github_provider_failure::cli_unavailable:
			case application::github_provider_failure::vault:
			case application::github_provider_failure::response_limit:
			case application::github_provider_failure::unavailable:
				return application::review_mutation_failure::unavailable;
			}
			return application::review_mutation_failure::unavailable;
		}

		[[nodiscard]] auto mutation_completion(
			hop_result                         aResult,
			const github_mutation_transmission aTransmission)
			-> github_mutation_wire_completion
		{
			if (auto* success = std::get_if<hop_success>(&aResult)) {
				return github_mutation_wire_success{
					.Response = {
						.Status  = success->Status,
						.Headers = std::move(success->Headers),
						.Body    = std::move(success->Body),
					},
				};
			}
			if (auto* error = std::get_if<application::github_provider_error>(&aResult)) {
				return github_mutation_wire_error{
					.Failure                = mutation_failure(error->Failure),
					.Transmission           = aTransmission,
					.ProviderFailure        = error->Failure,
					.HttpStatus             = error->HttpStatus,
					.TransportFailureSource = error->TransportFailureSource,
				};
			}
			if (std::holds_alternative<hop_cancelled>(aResult)) {
				return github_mutation_wire_cancelled{
					.Transmission = aTransmission,
				};
			}
			return github_mutation_wire_error{
				.Failure      = application::review_mutation_failure::unavailable,
				.Transmission = aTransmission,
			};
		}

		[[nodiscard]] auto device_authorization_error(
			const application::github_provider_failure aFailure)
			-> native_https_device_authorization_completion
		{
			return native_https_device_authorization_error{
				.Error = provider_error(aFailure),
			};
		}

		[[nodiscard]] auto device_authorization_completion(hop_result aResult)
			-> native_https_device_authorization_completion
		{
			if (auto* success = std::get_if<hop_success>(&aResult);
				success != nullptr) {
				return native_https_device_authorization_success{
					.Response = wire_response{
						.Status       = success->Status,
						.Headers      = std::move(success->Headers),
						.Body         = std::move(success->Body),
						.Completeness = success->Completeness,
					},
				};
			}
			if (auto* error = std::get_if<application::github_provider_error>(&aResult);
				error != nullptr) {
				return native_https_device_authorization_error{ .Error = *error };
			}
			if (std::holds_alternative<hop_cancelled>(aResult)) {
				return native_https_device_authorization_cancelled{ };
			}
			return device_authorization_error(
				application::github_provider_failure::redirect);
		}
	} // namespace

	class native_git_smart_transport final
		: public application::git_smart_transport_port
	{
	public:
		[[nodiscard]] static auto acquire(
			std::shared_ptr<native_https_transport>       aTransport,
			const application::git_smart_request_context& aContext,
			std::optional<core::application_session_id>   aAuthenticationSession) noexcept
			-> application::git_smart_transport_result<
				application::git_smart_transport>
		{
			if (aTransport == nullptr || !application::valid(aContext)
				|| !aTransport->can_acquire_smart(aContext, aAuthenticationSession)) {
				return std::unexpected{ smart_error(
					application::git_smart_transport_failure::invalid_request) };
			}
			try {
				auto state = std::make_shared<native_git_smart_lease_state>(aContext, aAuthenticationSession);
				return application::git_smart_transport{
					new native_git_smart_transport{
						std::move(aTransport), std::move(state) }
				};
			} catch (...) {
				return std::unexpected{ smart_error(
					application::git_smart_transport_failure::unavailable) };
			}
		}

		native_git_smart_transport(
			std::shared_ptr<native_https_transport>       aTransport,
			std::shared_ptr<native_git_smart_lease_state> aState) noexcept
			: Transport{ std::move(aTransport) }
			, State{ std::move(aState) }
		{
		}

		~native_git_smart_transport() override
		{
			revoke();
		}

		void revoke() noexcept override
		{
			if (State == nullptr
				|| State->Revoked.exchange(true, std::memory_order_acq_rel)) {
				return;
			}
			if (Transport != nullptr) {
				Transport->cancel_lease(State);
			}
		}

		[[nodiscard]] auto revoked() const noexcept -> bool override
		{
			return State == nullptr
				|| State->Revoked.load(std::memory_order_acquire);
		}

		[[nodiscard]] auto discover(
			const application::git_smart_discovery_request& aRequest,
			const std::stop_token                           aStop)
			-> application::git_smart_transport_result<
				application::git_smart_discovery_response> override
		{
			if (!exact_context(aRequest.Context)) {
				return std::unexpected{ smart_error(
					application::git_smart_transport_failure::invalid_request) };
			}
			if (revoked() || Transport == nullptr) {
				return std::unexpected{ smart_error(
					application::git_smart_transport_failure::cancelled) };
			}
			return Transport->discover_for_lease(aRequest, State, aStop);
		}

		[[nodiscard]] auto ls_refs(
			const application::git_smart_ls_refs_request& aRequest,
			const std::stop_token                         aStop)
			-> application::git_smart_transport_result<
				application::git_smart_ls_refs_response> override
		{
			if (!exact_context(aRequest.Context)) {
				return std::unexpected{ smart_error(
					application::git_smart_transport_failure::invalid_request) };
			}
			if (revoked() || Transport == nullptr) {
				return std::unexpected{ smart_error(
					application::git_smart_transport_failure::cancelled) };
			}
			return Transport->ls_refs_for_lease(aRequest, State, aStop);
		}

		[[nodiscard]] auto fetch(
			const application::git_smart_fetch_request& aRequest,
			application::git_smart_wire_consumer&       aConsumer,
			const std::stop_token                       aStop)
			-> application::git_smart_transport_result<void> override
		{
			if (!exact_context(aRequest.Context)) {
				return std::unexpected{ smart_error(
					application::git_smart_transport_failure::invalid_request) };
			}
			if (revoked() || Transport == nullptr) {
				return std::unexpected{ smart_error(
					application::git_smart_transport_failure::cancelled) };
			}
			return Transport->fetch_for_lease(aRequest, State, aConsumer, aStop);
		}

	private:
		[[nodiscard]] auto exact_context(
			const application::git_smart_request_context& aContext) const noexcept
			-> bool
		{
			return State != nullptr && aContext == State->Context;
		}

		std::shared_ptr<native_https_transport>       Transport;
		std::shared_ptr<native_git_smart_lease_state> State;
	};

	class native_https_transport::implementation final
	{
	public:
		implementation(
			const validated_github_host_profile& aProfile,
			bootstrap_credential                 aCredential,
			transport_policy                     aPolicy)
			: Profile{ aProfile }
			, Credential{ std::move(aCredential) }
			, Policy{ std::move(aPolicy) }
		{
		}

		implementation(
			const validated_github_host_profile& aProfile,
			account_credential                   aCredential,
			transport_policy                     aPolicy)
			: Profile{ aProfile }
			, Credential{ std::move(aCredential) }
			, Policy{ std::move(aPolicy) }
		{
		}

		implementation(
			const validated_github_host_profile& aProfile,
			credential_free_request              aCredential,
			transport_policy                     aPolicy)
			: Profile{ aProfile }
			, Credential{ std::move(aCredential) }
			, Policy{ std::move(aPolicy) }
		{
		}

		implementation(
			const validated_github_host_profile& aProfile,
			anonymous_read_request               aCredential,
			transport_policy                     aPolicy)
			: Profile{ aProfile }
			, Credential{ std::move(aCredential) }
			, Policy{ std::move(aPolicy) }
		{
		}

		implementation(
			const validated_github_host_profile& aProfile,
			anonymous_smart_request              aCredential,
			transport_policy                     aPolicy)
			: Profile{ aProfile }
			, Credential{ std::move(aCredential) }
			, Policy{ std::move(aPolicy) }
		{
		}

		implementation(const implementation&)                    = delete;
		auto operator=(const implementation&) -> implementation& = delete;

		~implementation()
		{
			Closing.store(true, std::memory_order_release);
			if (Dispatcher != nullptr && Thread.isRunning()) {
				static_cast<void>(QMetaObject::invokeMethod(
					Dispatcher,
					[this] {
						if (ActiveHop != nullptr) {
							ActiveHop->Cancellation = true;
							abort_hop(*ActiveHop);
						}
						if (ActiveSmartHop != nullptr) {
							ActiveSmartHop->Cancellation = true;
							abort_smart_hop(*ActiveSmartHop);
						}
						if (Manager != nullptr) {
							Manager->clearAccessCache();
							Manager->clearConnectionCache();
						}
					},
					Qt::BlockingQueuedConnection));
				Thread.quit();
				Thread.wait();
			}
			Dispatcher = nullptr;
			Manager    = nullptr;
		}

		[[nodiscard]] auto start() noexcept -> bool
		{
			try {
				Dispatcher = new (std::nothrow) QObject;
				if (Dispatcher == nullptr) {
					return false;
				}
				Dispatcher->moveToThread(&Thread);
				QObject::connect(
					&Thread,
					&QThread::finished,
					Dispatcher,
					&QObject::deleteLater);
				Thread.start();

				auto       initialized = false;
				const auto invoked     = QMetaObject::invokeMethod(
					Dispatcher,
					[this, &initialized] {
						try {
							Manager = new (std::nothrow)
								QNetworkAccessManager{ Dispatcher };
							if (Manager == nullptr) {
								return;
							}
							Manager->setAutoDeleteReplies(false);
							Manager->setRedirectPolicy(
								QNetworkRequest::ManualRedirectPolicy);
							Manager->setStrictTransportSecurityEnabled(false);
							connect_manager_signals();
							initialized = true;
						} catch (...) {
							delete Manager;
							Manager = nullptr;
						}
					},
					Qt::BlockingQueuedConnection);
				if (!invoked || !initialized) {
					Thread.quit();
					Thread.wait();
					Dispatcher = nullptr;
					Manager    = nullptr;
					return false;
				}
				return true;
			} catch (...) {
				if (Thread.isRunning()) {
					Thread.quit();
					Thread.wait();
				}
				Dispatcher = nullptr;
				Manager    = nullptr;
				return false;
			}
		}

		[[nodiscard]] auto perform(
			const github_wire_request& aRequest,
			const std::stop_token      aStop) -> github_wire_completion
		{
			if (aStop.stop_requested()) {
				return github_wire_cancelled{
					.Correlation = aRequest.Correlation,
				};
			}
			native_request native;
			try {
				if (!validate_github_wire_request(Profile, aRequest).has_value()) {
					return unavailable(aRequest.Correlation);
				}
				native                         = native_request_from(aRequest);
				native.RequireRateLimitHeaders = Profile.server_contract()
					== application::github_server_contract::github_com_2022_11_28;
				if (!valid_request_shape(native)) {
					return unavailable(aRequest.Correlation);
				}
			} catch (...) {
				return wire_error(
					aRequest.Correlation,
					application::github_provider_failure::response_limit);
			}
			return read_completion(
				aRequest.Correlation,
				perform_validated(aRequest, std::move(native), aStop));
		}

		[[nodiscard]] auto perform_capability_probe(
			const github_capability_probe_wire_request& aRequest,
			const std::stop_token                       aStop) -> github_capability_probe_wire_completion
		{
			if (aStop.stop_requested()) {
				return github_capability_probe_wire_cancelled{
					.Correlation = aRequest.Correlation,
				};
			}
			native_request native;
			try {
				if (!validate_github_capability_probe_wire_request(Profile, aRequest)
						 .has_value()) {
					return capability_error(
						aRequest.Correlation,
						application::github_provider_failure::unavailable);
				}
				native                         = native_request_from(aRequest);
				native.RequireRateLimitHeaders = Profile.server_contract()
					== application::github_server_contract::github_com_2022_11_28;
				if (!valid_request_shape(native)) {
					return capability_error(
						aRequest.Correlation,
						application::github_provider_failure::unavailable);
				}
			} catch (...) {
				return capability_error(
					aRequest.Correlation,
					application::github_provider_failure::response_limit);
			}
			return capability_completion(
				aRequest.Correlation,
				perform_validated(aRequest, std::move(native), aStop));
		}

		[[nodiscard]] auto perform_review_mutation(
			const github_mutation_wire_request&  aRequest,
			const std::stop_token                aStop,
			github_mutation_transmission_witness aWitness)
			-> github_mutation_wire_completion
		{
			if (aStop.stop_requested()) {
				return github_mutation_wire_cancelled{ };
			}
			// [[BPR-1018]]. The operation, not its path, says whether this is
			// a REST or GraphQL exchange, which status is success, and whether
			// the POST waits for the caller's witness.
			const auto     exchange = review_mutation_exchange_for(aRequest.Wire.Operation);
			native_request native;
			try {
				if (!application::valid(aRequest.Context) || !exchange.has_value()
					|| exchange->Method != aRequest.Wire.Method
					|| !validate_outbound_review_mutation_request(aRequest.Wire)
							.has_value()) {
					return github_mutation_wire_error{
						.Failure = application::review_mutation_failure::unavailable,
					};
				}
				native                         = native_request_from(aRequest, *exchange);
				native.RequireRateLimitHeaders = Profile.server_contract()
					== application::github_server_contract::github_com_2022_11_28;
				if (!valid_request_shape(native)
					|| exchange->Graphql != (native.Path == Profile.graphql_route())) {
					return github_mutation_wire_error{
						.Failure = application::review_mutation_failure::unavailable,
					};
				}
			} catch (...) {
				return github_mutation_wire_error{
					.Failure = application::review_mutation_failure::unavailable,
				};
			}
			// The hop calls this before handing a mutation to Qt. The durable
			// witness admits that exact operation once. Conservatively retain
			// possible transmission even if a subsequent launch fails.
			auto postCreated = std::make_shared<std::atomic_bool>(false);
			auto admit       = [postCreated, witnessed = exchange->Witnessed,
								transmissionWitness = std::move(aWitness)]() mutable {
				postCreated->store(true, std::memory_order_release);
				return !witnessed || (transmissionWitness && transmissionWitness());
			};
			auto result = perform_validated(
				aRequest, std::move(native), aStop, std::move(admit));
			return mutation_completion(
				std::move(result),
				postCreated->load(std::memory_order_acquire)
					? github_mutation_transmission::indeterminate
					: github_mutation_transmission::definitely_not_sent);
		}

		/// [[BPR-1175]]. One publication exchange. The operation, not its
		/// path, fixes the method and the statuses admitted; the request must
		/// be exactly what the contract compiles. A write that reached Qt is
		/// never sent again through another proxy, and its completion says
		/// whether it may have left.
		[[nodiscard]] auto perform_publication(
			const github_publication_wire_call& aCall,
			const std::stop_token               aStop)
			-> github_publication_wire_completion
		{
			if (aStop.stop_requested()) {
				return github_publication_wire_cancelled{ };
			}
			const auto     exchange = publication_exchange_for(aCall.Wire.Operation);
			native_request native;
			try {
				if (!application::valid(aCall.Context) || !exchange.has_value()
					|| exchange->Method != aCall.Wire.Method
					|| !validate_outbound_publication_request(aCall.Wire)
					|| Profile.server_contract() != aCall.Wire.Target.Server) {
					return github_publication_wire_error{ };
				}
				native                         = native_request_from(aCall, *exchange);
				native.RequireRateLimitHeaders = Profile.server_contract()
					== application::github_server_contract::github_com_2022_11_28;
				if (!valid_request_shape(native)) {
					return github_publication_wire_error{ };
				}
			} catch (...) {
				return github_publication_wire_error{ };
			}
			auto postCreated = std::make_shared<std::atomic_bool>(false);
			auto admit       = [postCreated]() mutable {
				postCreated->store(true, std::memory_order_release);
				return true;
			};
			auto       result       = perform_validated(aCall, std::move(native), aStop, std::move(admit));
			const auto transmission = postCreated->load(std::memory_order_acquire) ? github_mutation_transmission::indeterminate : github_mutation_transmission::definitely_not_sent;
			if (auto* success = std::get_if<hop_success>(&result)) {
				return github_publication_wire_success{
					.Response = { .Status = success->Status, .Headers = std::move(success->Headers), .Body = std::move(success->Body) },
				};
			}
			if (auto* error = std::get_if<application::github_provider_error>(&result)) {
				return github_publication_wire_error{ .Error = *error, .Transmission = transmission };
			}
			if (std::holds_alternative<hop_cancelled>(result)) {
				return github_publication_wire_cancelled{ .Transmission = transmission };
			}
			return github_publication_wire_error{ .Transmission = transmission };
		}

		[[nodiscard]] auto perform_device_authorization(
			const device_authorization_wire_request& aRequest,
			const std::stop_token                    aStop)
			-> native_https_device_authorization_completion
		{
			if (aStop.stop_requested()) {
				return native_https_device_authorization_cancelled{ };
			}
			native_request native;
			try {
				auto compiled = device_authorization_native_request(
					Profile, aRequest);
				if (!compiled.has_value()) {
					return device_authorization_error(
						application::github_provider_failure::unavailable);
				}
				native = std::move(*compiled);
				if (!valid_request_shape(native)) {
					return device_authorization_error(
						application::github_provider_failure::unavailable);
				}
			} catch (...) {
				return device_authorization_error(
					application::github_provider_failure::response_limit);
			}
			return device_authorization_completion(
				perform_validated(aRequest, std::move(native), aStop));
		}

		[[nodiscard]] auto can_acquire_smart(
			const application::git_smart_request_context&     aContext,
			const std::optional<core::application_session_id> aAuthenticationSession) const noexcept
			-> bool
		{
			// Exactly two bindings lease the Git smart surface, and both are
			// named here rather than inferred from whether material happens to
			// exist. The account binding authenticates with HTTP Basic. The
			// anonymous smart binding is RFC-BPR-0017's public preview and
			// sends no authorization at all; `credential_matches` decides
			// whether the leased authority is one it may carry, and
			// `make_smart_request` omits the header under exactly this state.
			// The device flow's credential-free binding and the anonymous REST
			// read binding are both refused: neither names this surface.
			return !Closing.load(std::memory_order_acquire)
				&& admits_smart_binding() && application::valid(aContext)
				&& credential_matches(aContext, aAuthenticationSession);
		}

		void cancel_lease(
			const std::shared_ptr<native_git_smart_lease_state>&
				aLease) noexcept
		{
			if (aLease == nullptr || Dispatcher == nullptr || !Thread.isRunning()) {
				return;
			}
			try {
				static_cast<void>(QMetaObject::invokeMethod(
					Dispatcher,
					[this, aLease] {
						if (ActiveSmartHop != nullptr
							&& ActiveSmartHop->Operation->Lease == aLease) {
							ActiveSmartHop->Cancellation = true;
							abort_smart_hop(*ActiveSmartHop);
						}
					},
					Qt::QueuedConnection));
			} catch (...) {
			}
		}

		[[nodiscard]] auto lease_unavailable(
			const std::shared_ptr<native_git_smart_lease_state>& aLease)
			const noexcept -> bool
		{
			return aLease == nullptr
				|| aLease->Revoked.load(std::memory_order_acquire)
				|| Closing.load(std::memory_order_acquire);
		}

		[[nodiscard]] auto discover(
			const application::git_smart_discovery_request&      aRequest,
			const std::shared_ptr<native_git_smart_lease_state>& aLease,
			const std::stop_token                                aStop)
			-> application::git_smart_transport_result<
				application::git_smart_discovery_response>
		{
			if (aStop.stop_requested()) {
				return std::unexpected{ smart_error(
					application::git_smart_transport_failure::cancelled) };
			}
			if (!application::valid(aRequest)) {
				return std::unexpected{ smart_error(
					application::git_smart_transport_failure::invalid_request) };
			}
			auto native = smart_native_request{
				.Surface       = smart_request_surface::discovery,
				.Context       = aRequest.Context,
				.ResponseLimit = application::maximum_git_smart_control_response_bytes,
			};
			auto result = perform_smart(std::move(native), aLease, aStop);
			if (!result.has_value()) {
				return std::unexpected{ result.error() };
			}
			return application::git_smart_discovery_response{
				.Body = std::move(*result),
			};
		}

		[[nodiscard]] auto ls_refs(
			const application::git_smart_ls_refs_request&        aRequest,
			const std::shared_ptr<native_git_smart_lease_state>& aLease,
			const std::stop_token                                aStop)
			-> application::git_smart_transport_result<
				application::git_smart_ls_refs_response>
		{
			if (aStop.stop_requested()) {
				return std::unexpected{ smart_error(
					application::git_smart_transport_failure::cancelled) };
			}
			if (!application::valid(aRequest)) {
				return std::unexpected{ smart_error(
					application::git_smart_transport_failure::invalid_request) };
			}
			smart_native_request native{
				.Surface       = smart_request_surface::ls_refs,
				.Context       = aRequest.Context,
				.ResponseLimit = application::maximum_git_smart_control_response_bytes,
			};
			try {
				native.Body.assign(aRequest.Body.begin(), aRequest.Body.end());
			} catch (...) {
				return std::unexpected{ smart_error(
					application::git_smart_transport_failure::request_limit) };
			}
			auto result = perform_smart(std::move(native), aLease, aStop);
			if (!result.has_value()) {
				return std::unexpected{ result.error() };
			}
			return application::git_smart_ls_refs_response{
				.Body = std::move(*result),
			};
		}

		[[nodiscard]] auto fetch(
			const application::git_smart_fetch_request&          aRequest,
			const std::shared_ptr<native_git_smart_lease_state>& aLease,
			application::git_smart_wire_consumer&                aConsumer,
			const std::stop_token                                aStop)
			-> application::git_smart_transport_result<void>
		{
			if (aStop.stop_requested()) {
				return std::unexpected{ smart_error(
					application::git_smart_transport_failure::cancelled) };
			}
			if (!application::valid(aRequest)) {
				return std::unexpected{ smart_error(
					application::git_smart_transport_failure::invalid_request) };
			}
			smart_native_request native{
				.Surface       = smart_request_surface::fetch,
				.Context       = aRequest.Context,
				.ResponseLimit = aRequest.MaximumResponseBytes,
				.Consumer      = &aConsumer,
			};
			try {
				native.Body.assign(aRequest.Body.begin(), aRequest.Body.end());
			} catch (...) {
				return std::unexpected{ smart_error(
					application::git_smart_transport_failure::request_limit) };
			}
			auto result = perform_smart(std::move(native), aLease, aStop);
			if (!result.has_value()) {
				return std::unexpected{ result.error() };
			}
			return { };
		}

	private:
		[[nodiscard]] auto perform_smart(
			smart_native_request                          aNative,
			std::shared_ptr<native_git_smart_lease_state> aLease,
			const std::stop_token                         aStop) -> smart_operation_result
		{
			if (aLease == nullptr || Closing.load(std::memory_order_acquire)
				|| Dispatcher == nullptr || Manager == nullptr) {
				return smart_failure(
					application::git_smart_transport_failure::unavailable);
			}
			if (aStop.stop_requested()
				|| aLease->Revoked.load(std::memory_order_acquire)) {
				return smart_failure(
					application::git_smart_transport_failure::cancelled);
			}
			if (Busy.test_and_set(std::memory_order_acq_rel)) {
				return smart_failure(
					aStop.stop_requested()
							|| aLease->Revoked.load(std::memory_order_acquire)
						? application::git_smart_transport_failure::cancelled
						: application::git_smart_transport_failure::unavailable);
			}
			struct busy_guard final
			{
				std::atomic_flag& Busy;
				~busy_guard() { Busy.clear(std::memory_order_release); }
			} guard{ Busy };

			if (aStop.stop_requested() || lease_unavailable(aLease)) {
				return smart_failure(
					application::git_smart_transport_failure::cancelled);
			}
			if (aNative.Context != aLease->Context) {
				return smart_failure(
					application::git_smart_transport_failure::invalid_request);
			}
			if (!credential_matches(aNative.Context, aLease->AuthenticationSession)) {
				return smart_failure(
					application::git_smart_transport_failure::unavailable);
			}

			std::shared_ptr<smart_operation_state> state;
			try {
				state = std::make_shared<smart_operation_state>(
					std::move(aNative), std::move(aLease), aStop,
					std::chrono::steady_clock::now()
						+ Policy.SmartTotalTimeout);
			} catch (...) {
				return smart_failure(
					application::git_smart_transport_failure::response_limit);
			}

			std::stop_callback callback{
				aStop, [this, weak = std::weak_ptr<smart_operation_state>{ state }] {
					const auto operation = weak.lock();
					if (operation == nullptr || Dispatcher == nullptr) {
						return;
					}
					static_cast<void>(QMetaObject::invokeMethod(
						Dispatcher,
						[this, weak] {
							const auto current = weak.lock();
							if (current != nullptr && ActiveSmartHop != nullptr
								&& ActiveSmartHop->Operation == current) {
								ActiveSmartHop->Cancellation = true;
								abort_smart_hop(*ActiveSmartHop);
							}
						},
						Qt::QueuedConnection));
				}
			};

			auto invoked = false;
			try {
				invoked = QMetaObject::invokeMethod(
					Dispatcher,
					[this, state] noexcept {
						try {
							state->Result.emplace(execute_smart(state));
						} catch (...) {
							try {
								state->Result.emplace(smart_failure(
									application::git_smart_transport_failure::
										response_limit));
							} catch (...) {
							}
						}
						state->Done.store(true, std::memory_order_release);
						state->Done.notify_one();
					},
					Qt::QueuedConnection);
			} catch (...) {
				return smart_failure(
					application::git_smart_transport_failure::response_limit);
			}
			if (!invoked) {
				return smart_failure(
					application::git_smart_transport_failure::unavailable);
			}

			state->Done.wait(false, std::memory_order_acquire);
			if (!state->Result.has_value()) {
				return smart_failure(
					application::git_smart_transport_failure::unavailable);
			}
			return std::move(*state->Result);
		}

		template <typename Request>
		[[nodiscard]] auto perform_validated(const Request&                       aRequest,
											 native_request                       aNative,
											 const std::stop_token                aStop,
											 github_mutation_transmission_witness aWitness = { })
			-> hop_result
		{
			if (Closing.load(std::memory_order_acquire) || Dispatcher == nullptr
				|| Manager == nullptr) {
				return provider_error(
					application::github_provider_failure::unavailable);
			}
			if (Busy.test_and_set(std::memory_order_acq_rel)) {
				return provider_error(
					application::github_provider_failure::unavailable);
			}
			struct busy_guard final
			{
				std::atomic_flag& Busy;
				~busy_guard() { Busy.clear(std::memory_order_release); }
			} guard{ Busy };

			if (aStop.stop_requested() || Closing.load(std::memory_order_acquire)) {
				return hop_cancelled{ };
			}
			if (!claim_credential(aRequest)) {
				return provider_error(
					application::github_provider_failure::unavailable);
			}

			std::shared_ptr<operation_state> state;
			try {
				state = std::make_shared<operation_state>(
					std::move(aNative), aStop,
					std::chrono::steady_clock::now() + Policy.TotalTimeout,
					std::move(aWitness));
			} catch (...) {
				discard_bootstrap();
				return provider_error(
					application::github_provider_failure::response_limit);
			}

			std::stop_callback callback{
				aStop, [this, weak = std::weak_ptr<operation_state>{ state }] {
					const auto operation = weak.lock();
					if (operation == nullptr) {
						return;
					}
					if (Dispatcher != nullptr) {
						static_cast<void>(QMetaObject::invokeMethod(
							Dispatcher,
							[this, weak] {
								const auto current = weak.lock();
								if (current != nullptr && ActiveHop != nullptr
									&& ActiveHop->Operation == current) {
									ActiveHop->Cancellation = true;
									abort_hop(*ActiveHop);
								}
							},
							Qt::QueuedConnection));
					}
				}
			};

			auto invoked = false;
			try {
				invoked = QMetaObject::invokeMethod(
					Dispatcher,
					[this, state] noexcept {
						try {
							state->Result.emplace(execute(state));
						} catch (...) {
							try {
								state->Result.emplace(provider_error(
									application::github_provider_failure::
										response_limit));
							} catch (...) {
							}
						}
						discard_bootstrap();
						state->Done.store(true, std::memory_order_release);
						state->Done.notify_one();
					},
					Qt::QueuedConnection);
			} catch (...) {
				discard_bootstrap();
				return provider_error(
					application::github_provider_failure::response_limit);
			}
			if (!invoked) {
				discard_bootstrap();
				return provider_error(
					application::github_provider_failure::unavailable);
			}

			state->Done.wait(false, std::memory_order_acquire);
			if (!state->Result.has_value()) {
				return provider_error(
					application::github_provider_failure::unavailable);
			}
			return std::move(*state->Result);
		}

		void connect_manager_signals()
		{
			QObject::connect(
				Manager,
				&QNetworkAccessManager::authenticationRequired,
				Manager,
				[this](QNetworkReply* const aReply, QAuthenticator* const aUth) {
					static_cast<void>(aUth);
					if (ActiveHop != nullptr && ActiveHop->Reply == aReply) {
						ActiveHop->AuthenticationChallenge = true;
						abort_hop(*ActiveHop);
					}
					if (ActiveSmartHop != nullptr
						&& ActiveSmartHop->Reply == aReply) {
						ActiveSmartHop->AuthenticationChallenge = true;
						abort_smart_hop(*ActiveSmartHop);
					}
				});
			QObject::connect(
				Manager,
				&QNetworkAccessManager::proxyAuthenticationRequired,
				Manager,
				[this](
					const QNetworkProxy&  aProxy,
					QAuthenticator* const aUth) {
					static_cast<void>(aProxy);
					static_cast<void>(aUth);
					if (ActiveHop != nullptr) {
						ActiveHop->ProxyAuthenticationChallenge = true;
						abort_hop(*ActiveHop);
					}
					if (ActiveSmartHop != nullptr) {
						ActiveSmartHop->ProxyAuthenticationChallenge = true;
						abort_smart_hop(*ActiveSmartHop);
					}
				});
			QObject::connect(
				Manager,
				&QNetworkAccessManager::sslErrors,
				Manager,
				[this](
					QNetworkReply* const    aReply,
					const QList<QSslError>& aErrors) {
					static_cast<void>(aErrors);
					if (ActiveHop != nullptr && ActiveHop->Reply == aReply) {
						ActiveHop->Certificate = true;
						abort_hop(*ActiveHop);
					}
					if (ActiveSmartHop != nullptr
						&& ActiveSmartHop->Reply == aReply) {
						ActiveSmartHop->Certificate = true;
						abort_smart_hop(*ActiveSmartHop);
					}
				});
			QObject::connect(
				Manager,
				&QNetworkAccessManager::preSharedKeyAuthenticationRequired,
				Manager,
				[this](
					QNetworkReply* const                 aReply,
					QSslPreSharedKeyAuthenticator* const aUthenticator) {
					static_cast<void>(aUthenticator);
					if (ActiveHop != nullptr && ActiveHop->Reply == aReply) {
						ActiveHop->PreSharedKeyChallenge = true;
						abort_hop(*ActiveHop);
					}
					if (ActiveSmartHop != nullptr
						&& ActiveSmartHop->Reply == aReply) {
						ActiveSmartHop->PreSharedKeyChallenge = true;
						abort_smart_hop(*ActiveSmartHop);
					}
				});
		}

		[[nodiscard]] auto credential_matches(
			const github_wire_request& aRequest) const noexcept -> bool
		{
			if (aRequest.Correlation.AuthenticationSession
				&& (aRequest.Correlation.AuthenticationSession->value() == 0U || !std::holds_alternative<account_credential>(Credential))) {
				return false;
			}
			return std::visit(
				[&aRequest, this](const auto& aCredential) noexcept {
					using credential_type = std::remove_cvref_t<decltype(aCredential)>;
					if constexpr (
						std::same_as<
							credential_type,
							bootstrap_credential>) {
						const auto* parent = std::get_if<
							core::github_session_request_context>(
							&aRequest.Correlation.Parent);
						return !aCredential.Used && parent != nullptr
							&& *parent == aCredential.Context
							&& aRequest.Correlation.Operation
							== read_operation::get_viewer_identity;
					} else if constexpr (
						std::same_as<credential_type, anonymous_read_request>) {
						// One table in every build. The eligible operations are
						// the public REST reads; viewer identity and the
						// GraphQL-routed operation are refused here, before a
						// request is constructed, because `perform_validated`
						// claims the credential before it builds anything.
						if (!admit_anonymous_read(
								 aRequest.Correlation.Operation,
								 Profile.server_contract())
								 .has_value()) {
							return false;
						}
						return std::visit(
							[&aCredential, this](const auto& aParent) noexcept {
								if (
									aParent.ApplicationSession
										!= aCredential.Context.ApplicationSession
									|| aParent.Profile != Profile.profile()
									|| aParent.IdentityGeneration
										!= aCredential.Context.IdentityGeneration) {
									return false;
								}
								using parent_type = std::remove_cvref_t<decltype(aParent)>;
								if constexpr (
									std::same_as<
										parent_type,
										core::repository_request_context>) {
									// [[BPR-0988]]. A discovery read needs a
									// viewer; an anonymous authority has none.
									return false;
								} else if constexpr (
									std::same_as<
										parent_type,
										core::pull_request_request_context>) {
									// An anonymous authority holds no account,
									// so there is no account identity to
									// correlate against — not account zero, not
									// the profile's, none. The host binding is
									// the whole of what remains to check.
									return aParent.Host == Profile.host();
								} else {
									return true;
								}
							},
							aRequest.Correlation.Parent);
					} else if constexpr (
						std::same_as<credential_type, anonymous_smart_request>) {
						// The Git smart surface is the whole of what this
						// binding names. It answers no REST read at all — not
						// the eligible anonymous ones, which are the other
						// binding's, and not the authenticated ones, which
						// need a credential it does not hold.
						return false;
					} else if constexpr (
						std::same_as<credential_type, credential_free_request>) {
#if defined(BPR_NATIVE_HTTPS_TESTING)
						// Exactly the session this binding was handed, nothing
						// wider. With no credential there is no account authority
						// to correlate a pull-request-scoped parent against, so
						// that shape is refused rather than matched loosely.
						const auto* parent = std::get_if<
							core::github_session_request_context>(
							&aRequest.Correlation.Parent);
						return parent != nullptr && *parent == aCredential.Context;
#else
						// A shipped credential-free binding answers reads with
						// nothing: RFC-BPR-0017's anonymous read path is its own
						// gated program, and the device-authorization endpoints
						// are the only shipped surface this binding satisfies.
						return false;
#endif
					} else {
						return std::visit(
							[&aCredential, &aRequest, this](const auto& aParent) noexcept {
								if (
									aRequest.Correlation.AuthenticationSession.value_or(aParent.ApplicationSession)
										!= aCredential.Context.ApplicationSession
									|| aParent.Profile != Profile.profile()
									|| aParent.IdentityGeneration
										!= aCredential.Context.IdentityGeneration) {
									return false;
								}
								using parent_type = std::remove_cvref_t<decltype(aParent)>;
								if constexpr (
									!std::same_as<
										parent_type,
										core::github_session_request_context>) {
									// A pull request read and a repository
									// discovery read ([[BPR-0988]]) both name the
									// exact account this credential belongs to.
									return aParent.Host == Profile.host()
										&& aParent.Account
										== aCredential.Context.Authority.Account;
								} else {
									return true;
								}
							},
							aRequest.Correlation.Parent);
					}
				},
				Credential);
		}

		[[nodiscard]] auto credential_matches(
			const github_capability_probe_wire_request& aRequest) const noexcept
			-> bool
		{
			const auto* credential = std::get_if<account_credential>(&Credential);
			if (credential == nullptr) {
				return false;
			}
			const auto& correlation = aRequest.Correlation;
			return correlation.Session.Request == credential->Context.Request
				&& correlation.Session.ApplicationSession
				== credential->Context.ApplicationSession
				&& correlation.Session.Profile == Profile.profile()
				&& correlation.Session.IdentityGeneration
				== credential->Context.IdentityGeneration
				&& correlation.ProfileRevision == Profile.revision()
				&& correlation.Host == Profile.host()
				&& correlation.ObservedAccount
				== credential->Context.Authority.Account
				&& authority_matches_profile(credential->Context.Authority,
											 Profile);
		}

		[[nodiscard]] auto credential_matches(
			const github_mutation_wire_request& aRequest) const noexcept -> bool
		{
			const auto* credential = std::get_if<account_credential>(&Credential);
			return credential != nullptr
				&& aRequest.Context == credential->Context
				&& authority_matches_profile(aRequest.Context.Authority, Profile);
		}

		/// [[BPR-1175]]. Publication runs only under the account binding it
		/// names, exactly like a review mutation.
		[[nodiscard]] auto credential_matches(
			const github_publication_wire_call& aCall) const noexcept -> bool
		{
			const auto* credential = std::get_if<account_credential>(&Credential);
			return credential != nullptr
				&& aCall.Context == credential->Context
				&& authority_matches_profile(aCall.Context.Authority, Profile);
		}

		[[nodiscard]] auto credential_matches(
			const application::git_smart_request_context&     aContext,
			const std::optional<core::application_session_id> aAuthenticationSession) const noexcept
			-> bool
		{
			if (aAuthenticationSession && (aAuthenticationSession->value() == 0U || !std::holds_alternative<account_credential>(Credential))) {
				return false;
			}
			if (
				const auto* anonymous
				= std::get_if<anonymous_smart_request>(&Credential);
				anonymous != nullptr) {
				// The mode is the admission, not the account's absence. An
				// authenticated authority handed to this binding is a private
				// or signed-in intent wearing a credential-free transport, and
				// it is refused here, before a request is constructed, rather
				// than dialed without the credential it needs. `core::valid`
				// has already refused a public-preview authority carrying a
				// nonzero account or viewer, so admitting the mode is
				// admitting exactly the identity-free authority.
				//
				// A profile whose server contract is not GitHub.com's is
				// refused for the same reason the anonymous REST binding
				// refuses it: GHES publishes no anonymous surface, so a GHES
				// profile sends nothing.
				return aContext.Authority.Mode
					== core::github_access_mode::public_preview
					&& Profile.server_contract()
					== application::github_server_contract::
						github_com_2022_11_28
					&& aContext.ApplicationSession
					== anonymous->Context.ApplicationSession
					&& aContext.Authority.Profile == Profile.profile()
					&& aContext.Authority.Profile == anonymous->Context.Profile
					&& aContext.Authority.ProfileRevision.value()
					== Profile.revision().value()
					&& aContext.Authority.Host == Profile.host()
					&& aContext.Revision.Source.IdentityGeneration
					== anonymous->Context.IdentityGeneration;
			}
			const auto* credential = std::get_if<account_credential>(&Credential);
			return credential != nullptr
				// An authenticated transport carries an account, so it leases
				// only an authenticated authority. A public-preview authority
				// here would be an anonymous read sharing a signed-in
				// namespace, which is the aliasing this card forbids.
				&& aContext.Authority.Mode
				== core::github_access_mode::authenticated
				&& aAuthenticationSession.value_or(aContext.ApplicationSession)
				== credential->Context.ApplicationSession
				&& aContext.Authority.Profile == Profile.profile()
				&& aContext.Authority.ProfileRevision.value()
				== Profile.revision().value()
				&& aContext.Authority.Host == Profile.host()
				&& aContext.Authority.Account
				== credential->Context.Authority.Account
				&& aContext.Revision.Source.IdentityGeneration
				== credential->Context.IdentityGeneration
				&& authority_matches_profile(
					   credential->Context.Authority, Profile);
		}

		[[nodiscard]] auto credential_matches(
			const device_authorization_wire_request&) const noexcept -> bool
		{
			// The device flow runs to obtain the first credential, so the only
			// binding it may run under is the one that deliberately holds none.
			// `credential_free()` names exactly one alternative, so the
			// anonymous read binding is refused here too: an anonymous
			// authority never asks for a credential it would then have nowhere
			// to put.
			// `make_request` omits the authorization header under exactly that
			// state, so refusing every other binding here is what keeps the
			// header's absence a property of this code rather than of a caller.
			return credential_free();
		}

		template <typename Request>
		[[nodiscard]] auto claim_credential(const Request& aRequest) noexcept -> bool
		{
			if (!credential_matches(aRequest)) {
				return false;
			}
			if (auto* bootstrap = std::get_if<bootstrap_credential>(&Credential);
				bootstrap != nullptr) {
				bootstrap->Used = true;
			}
			return true;
		}

		void discard_bootstrap() noexcept
		{
			if (auto* bootstrap = std::get_if<bootstrap_credential>(&Credential);
				bootstrap != nullptr) {
				bootstrap->Material.reset();
				bootstrap->Used = true;
			}
		}

		[[nodiscard]] auto credential_free() const noexcept -> bool
		{
			// Truthful in every build. Until BPR-0318 recorded a distributable
			// identity this was hardwired false outside the test graph, which
			// made a shipped device request refuse before dialing; a shipped
			// build now composes the credential-free binding for the device
			// flow, so the binding must report itself everywhere. Every
			// refusal that consumes this — the device flow demanding it, the
			// smart surface refusing it — keeps its meaning.
			return std::holds_alternative<credential_free_request>(Credential);
		}

		/// \brief Reports whether this binding has a credential to send.
		///
		/// \details The authorization header's absence is structural, not
		/// conditional: it is derived from which binding was named at
		/// construction, and the two bindings that name no material are
		/// enumerated here rather than inferred from an empty field. A future
		/// alternative must be added to this list to send a header, which is
		/// the direction the mistake should be hard in.
		[[nodiscard]] auto carries_authorization() const noexcept -> bool
		{
			return !std::holds_alternative<credential_free_request>(Credential)
				&& !std::holds_alternative<anonymous_read_request>(Credential)
				&& !std::holds_alternative<anonymous_smart_request>(Credential);
		}

		/// \brief Reports whether this binding may lease the Git smart surface.
		///
		/// \details Enumerated rather than derived, for the same reason
		/// `carries_authorization` is: a binding added later reaches this
		/// surface only by being named here, which is the direction the
		/// mistake should be hard in.
		[[nodiscard]] auto admits_smart_binding() const noexcept -> bool
		{
			return std::holds_alternative<account_credential>(Credential)
				|| std::holds_alternative<anonymous_smart_request>(Credential);
		}

		[[nodiscard]] auto credential_bytes() const noexcept
			-> std::span<const std::byte>
		{
			return std::visit(
				[](const auto& aCredential) noexcept
					-> std::span<const std::byte> {
					// Discriminate on the deliberate state, not on whether a
					// field happens to exist. Testing for the field would let a
					// future alternative that renames or omits `Material` compile
					// and silently send an empty credential; naming the type
					// keeps that a build failure.
					using credential_type = std::remove_cvref_t<decltype(aCredential)>;
					if constexpr (
						std::same_as<credential_type, credential_free_request>
						|| std::same_as<credential_type, anonymous_read_request>
						|| std::same_as<credential_type, anonymous_smart_request>) {
						return { };
					} else {
						return aCredential.Material.bytes();
					}
				},
				Credential);
		}

		void configure_proxy(const QNetworkProxy& aProxy)
		{
			Manager->clearAccessCache();
			Manager->clearConnectionCache();
			Manager->setProxy(aProxy);
		}

		[[nodiscard]] auto proxy_candidates(const QUrl& aUrl)
			-> std::optional<std::vector<QNetworkProxy>>
		{
#if defined(BPR_NATIVE_HTTPS_TESTING)
			if (Policy.Proxy == proxy_policy::fixed_test) {
				std::vector<QNetworkProxy> candidates;
				if (Policy.DirectFirstProxyCandidate) {
					candidates.emplace_back(QNetworkProxy::NoProxy);
				}
				candidates.emplace_back(
					QNetworkProxy::HttpProxy,
					QString::fromUtf8(
						Policy.ProxyHost.data(),
						static_cast<qsizetype>(Policy.ProxyHost.size())),
					Policy.ProxyPort);
				if (Policy.FallbackProxyPort != 0U) {
					candidates.emplace_back(
						QNetworkProxy::HttpProxy,
						QString::fromUtf8(
							Policy.ProxyHost.data(),
							static_cast<qsizetype>(
								Policy.ProxyHost.size())),
						Policy.FallbackProxyPort);
				}
				return candidates;
			}
#endif
			return system_proxy_candidates(aUrl);
		}

		[[nodiscard]] auto ssl_configuration() const
			-> std::optional<QSslConfiguration>
		{
			if (!QSslSocket::supportsSsl()) {
				return std::nullopt;
			}
			auto configuration = QSslConfiguration::defaultConfiguration();
			configuration.setCaCertificates(
				QSslConfiguration::systemCaCertificates());
			configuration.setLocalCertificateChain({ });
			configuration.setPrivateKey(QSslKey{ });
			configuration.setBackendConfiguration({ });
			configuration.setSessionTicket({ });
			configuration.setPreSharedKeyIdentityHint({ });
			configuration.setAllowedNextProtocols(
				{ QByteArray{ QSslConfiguration::NextProtocolHttp1_1 } });
			configuration.setPeerVerifyMode(QSslSocket::VerifyPeer);
			configuration.setPeerVerifyDepth(0);
			configuration.setProtocol(QSsl::TlsV1_2OrLater);
			configuration.setSslOption(
				QSsl::SslOptionDisableCompression,
				true);
			configuration.setSslOption(
				QSsl::SslOptionDisableSessionTickets,
				true);
			configuration.setSslOption(
				QSsl::SslOptionDisableSessionSharing,
				true);
			configuration.setSslOption(
				QSsl::SslOptionDisableSessionPersistence,
				true);
			configuration.setSslOption(
				QSsl::SslOptionDisableLegacyRenegotiation,
				true);
#if defined(BPR_NATIVE_HTTPS_TESTING)
			if (!Policy.TrustedCaCertificatePem.empty()) {
				const auto pem = QByteArray{
					Policy.TrustedCaCertificatePem.data(),
					static_cast<qsizetype>(
						Policy.TrustedCaCertificatePem.size())
				};
				const auto certificates = QSslCertificate::fromData(pem, QSsl::Pem);
				if (certificates.size() != 1) {
					return std::nullopt;
				}
				configuration.addCaCertificate(certificates.front());
			}
#endif
			return configuration;
		}

		[[nodiscard]] auto make_request(
			const native_request& aWire, const QUrl& aUrl)
			-> std::optional<QNetworkRequest>
		{
			auto request = QNetworkRequest{ aUrl };
			request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
								 QNetworkRequest::ManualRedirectPolicy);
			request.setAttribute(QNetworkRequest::CookieLoadControlAttribute,
								 QNetworkRequest::Manual);
			request.setAttribute(QNetworkRequest::CookieSaveControlAttribute,
								 QNetworkRequest::Manual);
			request.setAttribute(QNetworkRequest::AuthenticationReuseAttribute,
								 QNetworkRequest::Manual);
			request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
								 QNetworkRequest::AlwaysNetwork);
			request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
			request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
			request.setAttribute(QNetworkRequest::Http2DirectAttribute, false);
			request.setAttribute(QNetworkRequest::Http2CleartextAllowedAttribute,
								 false);
			request.setMaximumRedirectsAllowed(0);
			request.setDecompressedSafetyCheckThreshold(0);

			for (const auto& header : aWire.Headers) {
				request.setRawHeader(
					QByteArray{
						header.Name.data(),
						static_cast<qsizetype>(header.Name.size()) },
					QByteArray{
						header.Value.data(),
						static_cast<qsizetype>(header.Value.size()) });
			}
			request.setRawHeader("Accept-Encoding", "identity");
			request.setRawHeader("Accept-Language", "*");

			// The only ways to reach this without an authorization header are
			// the two bindings that deliberately hold no material — the device
			// flow's, whose whole point is that no credential exists yet, and
			// the anonymous read authority, which never has one. Every other
			// binding sends exactly its held credential.
			if (carries_authorization()) {
				QByteArray                  authorization{ "Bearer " };
				const qbytearray_wipe_guard authorizationGuard{ authorization };
				const auto                  credential = credential_bytes();
				authorization.append(
					reinterpret_cast<const char*>(credential.data()),
					static_cast<qsizetype>(credential.size()));
				request.setRawHeader("Authorization", authorization);
			}

			const auto tls = ssl_configuration();
			if (!tls.has_value()) {
				return std::nullopt;
			}
			request.setSslConfiguration(*tls);
			return request;
		}

		[[nodiscard]] auto make_smart_request(
			const smart_native_request& aWire,
			const QUrl&                 aUrl) -> std::optional<QNetworkRequest>
		{
			auto request = QNetworkRequest{ aUrl };
			request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
								 QNetworkRequest::ManualRedirectPolicy);
			request.setAttribute(QNetworkRequest::CookieLoadControlAttribute,
								 QNetworkRequest::Manual);
			request.setAttribute(QNetworkRequest::CookieSaveControlAttribute,
								 QNetworkRequest::Manual);
			request.setAttribute(QNetworkRequest::AuthenticationReuseAttribute,
								 QNetworkRequest::Manual);
			request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
								 QNetworkRequest::AlwaysNetwork);
			request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
			request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
			request.setAttribute(QNetworkRequest::Http2DirectAttribute, false);
			request.setAttribute(QNetworkRequest::Http2CleartextAllowedAttribute,
								 false);
			request.setMaximumRedirectsAllowed(0);
			request.setDecompressedSafetyCheckThreshold(0);
			request.setRawHeader(
				"Git-Protocol",
				QByteArray{ smart_git_protocol_header.data(),
							static_cast<qsizetype>(
								smart_git_protocol_header.size()) });
			const auto accepted = smart_expected_content_type(aWire.Surface);
			request.setRawHeader(
				"Accept",
				QByteArray{ accepted.data(),
							static_cast<qsizetype>(accepted.size()) });
			if (aWire.Surface != smart_request_surface::discovery) {
				request.setRawHeader(
					"Content-Type",
					QByteArray{
						smart_git_request_content_type.data(),
						static_cast<qsizetype>(
							smart_git_request_content_type.size()) });
			}
			request.setRawHeader("Accept-Encoding", "identity");
			request.setRawHeader("Accept-Language", "*");

			// The Basic header is built only where there is a credential to
			// put in it. The anonymous smart binding holds none, so it sends
			// no authorization at all rather than an empty user/password pair
			// — an empty Basic header is a credential claim, and a public
			// read makes none. The absence is derived from the binding, the
			// same structural rule the REST builder uses.
			if (carries_authorization()) {
				QByteArray userPassword{
					smart_git_basic_username.data(),
					static_cast<qsizetype>(smart_git_basic_username.size())
				};
				const qbytearray_wipe_guard userPasswordGuard{ userPassword };
				const auto                  credential = credential_bytes();
				userPassword.append(
					reinterpret_cast<const char*>(credential.data()),
					static_cast<qsizetype>(credential.size()));
				auto                        encoded = userPassword.toBase64(QByteArray::Base64Encoding);
				const qbytearray_wipe_guard encodedGuard{ encoded };
				QByteArray                  authorization{ "Basic " };
				const qbytearray_wipe_guard authorizationGuard{ authorization };
				authorization.append(encoded);
				request.setRawHeader("Authorization", authorization);
			}

			const auto tls = ssl_configuration();
			if (!tls.has_value()) {
				return std::nullopt;
			}
			request.setSslConfiguration(*tls);
			return request;
		}

		[[nodiscard]] auto execute_hop(
			const std::shared_ptr<operation_state>& aOperation,
			const QUrl&                             aUrl,
			const QNetworkProxy&                    aProxy) -> hop_result
		{
			if (cancellation_result(aOperation)) {
				return hop_cancelled{ };
			}
			if (!remaining_before_deadline(aOperation->Deadline).has_value()) {
				return provider_error(
					application::github_provider_failure::timeout);
			}
			configure_proxy(aProxy);
			if (cancellation_result(aOperation)) {
				return hop_cancelled{ };
			}
			if (!remaining_before_deadline(aOperation->Deadline).has_value()) {
				return provider_error(
					application::github_provider_failure::timeout);
			}
			auto request = make_request(aOperation->Request, aUrl);
			if (!request.has_value()) {
				return provider_error(
					application::github_provider_failure::unavailable);
			}

			const auto responseBodyLimit = std::min(
				Policy.ResponseBodyLimit,
				aOperation->Request.ResponseLimit);
			hop_state state{ aOperation, responseBodyLimit };
			try {
				state.Body.reserve(responseBodyLimit + 1U);
			} catch (...) {
				return provider_error(
					application::github_provider_failure::response_limit);
			}

			QByteArray requestBody;
			if (
				aOperation->Request.Method != http_method::get) {
				requestBody = QByteArray{
					aOperation->Request.Body.data(),
					static_cast<qsizetype>(aOperation->Request.Body.size())
				};
			}
			if (cancellation_result(aOperation)) {
				return hop_cancelled{ };
			}
			const auto sendRemaining = remaining_before_deadline(aOperation->Deadline);
			if (!sendRemaining.has_value()) {
				return provider_error(
					application::github_provider_failure::timeout);
			}
			request->setTransferTimeout(*sendRemaining);
			// Persist the witness before giving any mutation verb to Qt. Marking
			// the attempt conservatively also prevents proxy fallback after an
			// admission failure; no request is created when durability refuses.
			if ((aOperation->Request.Surface == native_request_surface::review_mutation
				 || aOperation->Request.Surface == native_request_surface::publication)
				&& aOperation->Request.Method != http_method::get) {
				aOperation->MutationPostCreated = true;
				auto witnessed                  = false;
				try {
					witnessed = aOperation->TransmissionWitness
						&& aOperation->TransmissionWitness();
				} catch (...) {
				}
				if (!witnessed) {
					abort_hop(state);
					return provider_error(
						application::github_provider_failure::unavailable);
				}
			}
			switch (aOperation->Request.Method) {
			case http_method::get:     state.Reply = Manager->get(*request); break;
			case http_method::post:    state.Reply = Manager->post(*request, requestBody); break;
			case http_method::put:     state.Reply = Manager->put(*request, requestBody); break;
			case http_method::patch:   state.Reply = Manager->sendCustomRequest(*request, "PATCH", requestBody); break;
			case http_method::delete_: state.Reply = Manager->deleteResource(*request); break;
			default:                   return provider_error(application::github_provider_failure::unavailable);
			}
			if (state.Reply == nullptr) {
				return provider_error(
					application::github_provider_failure::network);
			}

			active_hop_guard active{ state, ActiveHop };
			state.Reply->setReadBufferSize(static_cast<qint64>(std::min(
				responseBodyLimit + 1U, response_read_buffer_bytes)));

			QEventLoop loop;
			QTimer     deadline;
			deadline.setSingleShot(true);
			QTimer cancellation;
			cancellation.setInterval(cancellation_poll_interval);

			QObject::connect(state.Reply, &QNetworkReply::encrypted, &loop,
							 [&state] { state.EncryptedSignalObserved = true; });
			QObject::connect(state.Reply, &QNetworkReply::metaDataChanged, &loop,
							 [&state] { inspect_metadata(state); });
			QObject::connect(state.Reply, &QIODevice::readyRead, &loop,
							 [&state] { read_available(state); });
			QObject::connect(state.Reply, &QNetworkReply::finished, &loop,
							 &QEventLoop::quit);
			QObject::connect(&deadline, &QTimer::timeout, &loop, [&state] {
				state.Deadline = true;
				abort_hop(state);
			});
			QObject::connect(&cancellation, &QTimer::timeout, &loop, [&state] {
				if (cancellation_result(state.Operation)) {
					state.Cancellation = true;
					abort_hop(state);
				}
			});

			const auto waitRemaining = remaining_before_deadline(aOperation->Deadline);
			if (!waitRemaining.has_value()) {
				state.Deadline = true;
				abort_hop(state);
			} else {
				deadline.start(*waitRemaining);
				cancellation.start();
				if (cancellation_result(aOperation)) {
					state.Cancellation = true;
					abort_hop(state);
				}
				if (!state.Reply->isFinished()) {
					loop.exec();
				}
			}
			deadline.stop();
			cancellation.stop();
			inspect_metadata(state);
			read_available(state);
			if (state.Cancellation || cancellation_result(aOperation)) {
				active.reset();
				return hop_cancelled{ };
			}
			if (
				state.Deadline
				|| !remaining_before_deadline(aOperation->Deadline).has_value()) {
				active.reset();
				return provider_error(
					application::github_provider_failure::timeout);
			}

			const auto error    = state.Reply->error();
			const auto target   = recognized_redirect_status(state.Status)
				? redirect_target(*state.Reply, aUrl)
				: std::nullopt;
			const auto tlsValid = !state.Certificate
				&& tls_terminal_valid(
					*state.Reply,
					aUrl,
					state.EncryptedSignalObserved);
			const auto terminalExpired = !remaining_before_deadline(aOperation->Deadline).has_value();
			active.reset();

			if (state.Cancellation || cancellation_result(aOperation)) {
				return hop_cancelled{ };
			}
			if (state.Deadline || terminalExpired) {
				return provider_error(
					application::github_provider_failure::timeout);
			}
			if (state.Certificate) {
				return provider_error(
					application::github_provider_failure::certificate);
			}
			if (
				state.AuthenticationChallenge
				|| state.PreSharedKeyChallenge) {
				return provider_error(
					application::github_provider_failure::authentication);
			}
			if (state.ProxyAuthenticationChallenge) {
				return hop_proxy_failure{
					.Error = provider_error(
						application::github_provider_failure::network),
				};
			}
			if (state.HeaderLimit || state.ResponseLimit) {
				return capability_hop_error(aOperation->Request.Surface,
											application::github_provider_failure::response_limit, state.HeaderLimit ? application::github_capability_diagnostic::transport_header_limit : application::github_capability_diagnostic::transport_body_limit);
			}
			if (state.Malformed) {
				return capability_hop_error(aOperation->Request.Surface,
											application::github_provider_failure::malformed_response, state.CapabilityDetail);
			}
			if (state.Status == 0) {
				const auto failure = error == QNetworkReply::NoError
					? application::github_provider_failure::
						  malformed_response
					: network_failure(error);
				auto       result  = provider_error(failure);
				if (error != QNetworkReply::NoError) {
					result.TransportFailureSource = application::diagnostic_transport_failure_source::network_reply;
				}
				if (
					error != QNetworkReply::NoError
					&& proxy_candidate_retryable(error)) {
					return hop_proxy_failure{ .Error = result };
				}
				return result;
			}
			if (!tlsValid) {
				return provider_error(
					application::github_provider_failure::certificate);
			}
			if (recognized_redirect_status(state.Status)) {
				if (!target.has_value()) {
					return provider_error(
						application::github_provider_failure::redirect);
				}
				return hop_redirect{ .Target = *target };
			}
			const auto admittedStatus = state.Status
				== aOperation->Request.AdmittedContractStatus;
			if (state.Status != aOperation->Request.SuccessfulStatus && !admittedStatus) {
				auto result                   = classify_http_status(state);
				result.HttpStatus             = static_cast<std::uint16_t>(state.Status);
				result.TransportFailureSource = application::diagnostic_transport_failure_source::http_response;
				return result;
			}
			if (error != QNetworkReply::NoError
				&& !(admittedStatus
					 && (error == QNetworkReply::ProtocolInvalidOperationError
						 || (state.Status == 422 && error == QNetworkReply::UnknownContentError)
						 // [[BPR-1175]]. A publication read admits 404 as the
						 // observation that the object or branch is absent.
						 || (state.Status == 404 && error == QNetworkReply::ContentNotFoundError)))) {
				auto result                   = provider_error(network_failure(error));
				result.HttpStatus             = static_cast<std::uint16_t>(state.Status);
				result.TransportFailureSource = application::diagnostic_transport_failure_source::network_reply;
				return result;
			}
			if (
				!state.ContentType.has_value()
				|| !supported_json_content_type(*state.ContentType)) {
				return capability_hop_error(aOperation->Request.Surface,
											application::github_provider_failure::malformed_response, application::github_capability_diagnostic::transport_invalid_content_type);
			}
			if (
				state.ContentLength.has_value()
				&& *state.ContentLength != state.Body.size()) {
				return capability_hop_error(aOperation->Request.Surface,
											application::github_provider_failure::malformed_response, application::github_capability_diagnostic::transport_body_length);
			}
			const auto completeness = parse_link_completeness(state.Link);
			if (!completeness.has_value()) {
				return capability_hop_error(aOperation->Request.Surface,
											application::github_provider_failure::malformed_response, application::github_capability_diagnostic::transport_invalid_metadata);
			}
			return hop_success{
				.Status       = state.Status,
				.Body         = std::move(state.Body),
				.Headers      = allowlisted_success_headers(state),
				.Completeness = *completeness,
			};
		}

		[[nodiscard]] auto execute_with_proxy_candidates(
			const std::shared_ptr<operation_state>& aOperation, const QUrl& aUrl,
			const std::vector<QNetworkProxy>& aCandidates) -> hop_result
		{
			for (std::size_t index = 0U; index < aCandidates.size(); ++index) {
				auto attempt = execute_hop(aOperation, aUrl, aCandidates[index]);
				if (auto* proxyFailure = std::get_if<hop_proxy_failure>(&attempt);
					proxyFailure != nullptr) {
					if (aOperation->MutationPostCreated) {
						return proxyFailure->Error;
					}
					if (index + 1U < aCandidates.size()) {
						continue;
					}
					return proxyFailure->Error;
				}
				return attempt;
			}
			return provider_error(application::github_provider_failure::network);
		}

		[[nodiscard]] auto execute_device_authorization(
			const std::shared_ptr<operation_state>& aOperation) -> hop_result
		{
			const auto url = build_device_authorization_url(
				Profile, aOperation->Request);
			if (!url.has_value()) {
				return provider_error(
					application::github_provider_failure::unavailable);
			}
			if (cancellation_result(aOperation)) {
				return hop_cancelled{ };
			}
			if (!remaining_before_deadline(aOperation->Deadline).has_value()) {
				return provider_error(
					application::github_provider_failure::timeout);
			}
			const auto candidates = proxy_candidates(*url);
			if (cancellation_result(aOperation)) {
				return hop_cancelled{ };
			}
			if (!candidates.has_value() || candidates->empty()) {
				return provider_error(
					application::github_provider_failure::network);
			}

			auto result = execute_with_proxy_candidates(
				aOperation, *url, *candidates);
			if (std::holds_alternative<hop_redirect>(result)) {
				return provider_error(
					application::github_provider_failure::redirect);
			}
			return result;
		}

		[[nodiscard]] auto
		execute(const std::shared_ptr<operation_state>& aOperation) -> hop_result
		{
			if (aOperation->Request.Surface
				== native_request_surface::device_authorization) {
				return execute_device_authorization(aOperation);
			}

			const auto initial = build_url(Profile, aOperation->Request);
			if (!initial.has_value()) {
				return provider_error(
					application::github_provider_failure::unavailable);
			}

			auto                     current = *initial;
			std::vector<std::string> visited;
			visited.reserve(native_https_maximum_redirects + 1U);
			visited.emplace_back(
				safe_qbytearray_string(current.toEncoded(QUrl::FullyEncoded)));

			for (std::uint32_t hop = 0U; hop <= native_https_maximum_redirects;
				 ++hop) {
				if (cancellation_result(aOperation)) {
					return hop_cancelled{ };
				}
				if (!remaining_before_deadline(aOperation->Deadline).has_value()) {
					return provider_error(
						application::github_provider_failure::timeout);
				}
				const auto candidates = proxy_candidates(current);
				if (cancellation_result(aOperation)) {
					return hop_cancelled{ };
				}
				if (!remaining_before_deadline(aOperation->Deadline).has_value()) {
					return provider_error(
						application::github_provider_failure::timeout);
				}
				if (!candidates.has_value() || candidates->empty()) {
					return provider_error(
						application::github_provider_failure::network);
				}

				auto result = execute_with_proxy_candidates(aOperation, current, *candidates);
				if (auto* success = std::get_if<hop_success>(&result);
					success != nullptr) {
					if (aOperation->Request.Surface == native_request_surface::capability_probe) {
						success->CapabilitySummary = sanitize_capability_error(success->Body, credential_bytes());
					}
					return std::move(*success);
				}
				if (std::holds_alternative<hop_cancelled>(result)) {
					return hop_cancelled{ };
				}
				if (auto* error = std::get_if<application::github_provider_error>(&result);
					error != nullptr) {
					return *error;
				}

				if (auto* error = std::get_if<hop_capability_failure>(&result)) {
					return *error;
				}

				const auto& redirect = std::get<hop_redirect>(result);
				if (hop == native_https_maximum_redirects
					|| aOperation->Request.Method != http_method::get
					|| aOperation->Request.Surface
						!= native_request_surface::rest_read) {
					return provider_error(
						application::github_provider_failure::redirect);
				}
				const auto approved = validate_github_redirect_target(
					Profile, github_request_surface::rest, redirect.Target);
				if (!approved.has_value()
					|| std::ranges::find(visited, redirect.Target) != visited.end()) {
					return provider_error(
						application::github_provider_failure::redirect);
				}
				visited.push_back(redirect.Target);
				const auto encoded = QByteArray{ redirect.Target.data(),
												 static_cast<qsizetype>(redirect.Target.size()) };
				const auto next    = QUrl::fromEncoded(encoded, QUrl::StrictMode);
				if (!next.isValid() || next.toEncoded(QUrl::FullyEncoded) != encoded) {
					return provider_error(
						application::github_provider_failure::redirect);
				}
				current = next;
			}
			return provider_error(application::github_provider_failure::redirect);
		}

		[[nodiscard]] auto execute_smart_hop(
			const std::shared_ptr<smart_operation_state>& aOperation,
			const QUrl&                                   aUrl,
			const QNetworkProxy&                          aProxy,
			bool&                                         aRetryableCandidateFailure) -> smart_operation_result
		{
			aRetryableCandidateFailure = false;
			if (cancellation_result(aOperation)) {
				return smart_failure(
					application::git_smart_transport_failure::cancelled);
			}
			if (!remaining_before_deadline(aOperation->Deadline).has_value()) {
				return smart_failure(
					application::git_smart_transport_failure::timeout);
			}
			configure_proxy(aProxy);
			if (cancellation_result(aOperation)) {
				return smart_failure(
					application::git_smart_transport_failure::cancelled);
			}

			auto request = make_smart_request(aOperation->Request, aUrl);
			if (!request.has_value()) {
				return smart_failure(
					application::git_smart_transport_failure::unavailable);
			}
			const auto remaining = remaining_before_deadline(aOperation->Deadline);
			if (!remaining.has_value()) {
				return smart_failure(
					application::git_smart_transport_failure::timeout);
			}
			request->setTransferTimeout(std::min(
				*remaining,
				std::chrono::duration_cast<std::chrono::milliseconds>(
					smart_idle_timeout)));

			smart_hop_state state{ aOperation };
			if (aOperation->Request.Surface != smart_request_surface::fetch) {
				try {
					state.Body.reserve(static_cast<std::size_t>(
						aOperation->Request.ResponseLimit));
				} catch (...) {
					return smart_failure(
						application::git_smart_transport_failure::response_limit);
				}
			}

			if (aOperation->Request.Surface == smart_request_surface::discovery) {
				state.Reply = Manager->get(*request);
			} else {
				const auto& body = aOperation->Request.Body;
				state.Reply      = Manager->post(
					*request,
					QByteArray{
						reinterpret_cast<const char*>(body.data()),
						static_cast<qsizetype>(body.size()) });
			}
			if (state.Reply == nullptr) {
				return smart_failure(
					application::git_smart_transport_failure::network);
			}
			active_smart_hop_guard active{ state, ActiveSmartHop };
			state.Reply->setReadBufferSize(
				static_cast<qint64>(response_read_buffer_bytes));

			QEventLoop loop;
			QTimer     deadline;
			deadline.setSingleShot(true);
			QTimer idle;
			idle.setSingleShot(true);
			QTimer cancellation;
			cancellation.setInterval(cancellation_poll_interval);

			QObject::connect(state.Reply, &QNetworkReply::encrypted, &loop,
							 [&state] { state.EncryptedSignalObserved = true; });
			QObject::connect(state.Reply, &QNetworkReply::metaDataChanged, &loop,
							 [&state] { inspect_smart_metadata(state); });
			QObject::connect(
				state.Reply, &QIODevice::readyRead, &loop,
				[&state, &idle] { read_smart_available(state, idle); });
			QObject::connect(
				state.Reply, &QNetworkReply::uploadProgress, &loop,
				[&idle](const qint64 aSent, const qint64) {
					if (aSent > 0) {
						idle.start(std::chrono::duration_cast<
								   std::chrono::milliseconds>(
							smart_idle_timeout));
					}
				});
			QObject::connect(state.Reply, &QNetworkReply::finished, &loop,
							 &QEventLoop::quit);
			QObject::connect(&deadline, &QTimer::timeout, &loop, [&state] {
				state.Deadline = true;
				abort_smart_hop(state);
			});
			QObject::connect(&idle, &QTimer::timeout, &loop, [&state] {
				state.IdleTimeout = true;
				abort_smart_hop(state);
			});
			QObject::connect(&cancellation, &QTimer::timeout, &loop, [&state] {
				if (cancellation_result(state.Operation)) {
					state.Cancellation = true;
					abort_smart_hop(state);
				}
			});

			const auto waitRemaining = remaining_before_deadline(aOperation->Deadline);
			if (!waitRemaining.has_value()) {
				state.Deadline = true;
				abort_smart_hop(state);
			} else {
				deadline.start(*waitRemaining);
				idle.start(std::min(
					*waitRemaining,
					std::chrono::duration_cast<std::chrono::milliseconds>(
						smart_idle_timeout)));
				cancellation.start();
				if (cancellation_result(aOperation)) {
					state.Cancellation = true;
					abort_smart_hop(state);
				}
				if (!state.Reply->isFinished()) {
					loop.exec();
				}
			}
			deadline.stop();
			idle.stop();
			cancellation.stop();
			inspect_smart_metadata(state);
			read_smart_available(state, idle);

			const auto error    = state.Reply->error();
			const auto tlsValid = !state.Certificate
				&& tls_terminal_valid(
					*state.Reply, aUrl, state.EncryptedSignalObserved);
			const auto terminalExpired = !remaining_before_deadline(aOperation->Deadline).has_value();
			active.reset();

			if (state.Cancellation || cancellation_result(aOperation)) {
				return smart_failure(
					application::git_smart_transport_failure::cancelled);
			}
			if (state.Deadline || state.IdleTimeout || terminalExpired) {
				return smart_failure(
					application::git_smart_transport_failure::timeout);
			}
			if (state.Certificate) {
				return smart_failure(
					application::git_smart_transport_failure::certificate);
			}
			if (state.AuthenticationChallenge || state.PreSharedKeyChallenge) {
				return smart_failure(
					application::git_smart_transport_failure::authentication);
			}
			if (state.ProxyAuthenticationChallenge) {
				aRetryableCandidateFailure = state.ReceivedBytes == 0U;
				return smart_failure(
					application::git_smart_transport_failure::proxy);
			}
			if (state.Status != 0 && !tlsValid) {
				return smart_failure(
					application::git_smart_transport_failure::certificate);
			}
			if (smart_redirect_status(state.Status)) {
				return smart_failure(
					application::git_smart_transport_failure::redirect);
			}
			if (state.HeaderLimit || state.ResponseLimit) {
				return smart_failure(
					application::git_smart_transport_failure::response_limit);
			}
			if (state.ConsumerRejected) {
				return smart_failure(
					application::git_smart_transport_failure::consumer_rejected);
			}
			if (state.Status == 0) {
				aRetryableCandidateFailure = error != QNetworkReply::NoError
					&& state.ReceivedBytes == 0U
					&& proxy_candidate_retryable(error);
				return smart_failure(
					error == QNetworkReply::NoError
						? application::git_smart_transport_failure::
							  malformed_response
						: smart_network_failure(error));
			}
			if (!tlsValid) {
				return smart_failure(
					application::git_smart_transport_failure::certificate);
			}
			if (state.Status != 200) {
				return smart_failure(smart_http_failure(state.Status));
			}
			if (state.Malformed) {
				return smart_failure(
					application::git_smart_transport_failure::
						malformed_response);
			}
			if (error != QNetworkReply::NoError) {
				return smart_failure(smart_network_failure(error));
			}
			if (state.ContentLength.has_value()
				&& *state.ContentLength != state.ReceivedBytes) {
				return smart_failure(
					application::git_smart_transport_failure::
						malformed_response);
			}
			return std::move(state.Body);
		}

		[[nodiscard]] auto execute_smart(
			const std::shared_ptr<smart_operation_state>& aOperation)
			-> smart_operation_result
		{
			const auto url = build_smart_url(Profile, aOperation->Request);
			if (!url.has_value()) {
				return smart_failure(
					application::git_smart_transport_failure::invalid_request);
			}
			const auto candidates = proxy_candidates(*url);
			if (!candidates.has_value() || candidates->empty()) {
				return smart_failure(
					application::git_smart_transport_failure::proxy);
			}
			for (std::size_t index = 0U; index < candidates->size(); ++index) {
				auto retryableCandidateFailure = false;
				auto result                    = execute_smart_hop(
					aOperation,
					*url,
					(*candidates)[index],
					retryableCandidateFailure);
				if (result.has_value()
					|| !retryableCandidateFailure
					|| index + 1U == candidates->size()) {
					return result;
				}
			}
			return smart_failure(
				application::git_smart_transport_failure::proxy);
		}

		validated_github_host_profile Profile;
		credential_binding            Credential;
		transport_policy              Policy;
		QThread                       Thread;
		QObject*                      Dispatcher     = nullptr;
		QNetworkAccessManager*        Manager        = nullptr;
		hop_state*                    ActiveHop      = nullptr;
		smart_hop_state*              ActiveSmartHop = nullptr;
		std::atomic_bool              Closing        = false;
		std::atomic_flag              Busy           = ATOMIC_FLAG_INIT;
	};

	namespace {
		[[nodiscard]] auto make_production_policy() -> transport_policy
		{
			return { };
		}

		/// \brief Reports whether a profile may be selected anonymously.
		///
		/// \details Asked before a transport exists, so a GHES profile costs
		/// no thread, no manager, and no request. `admit_anonymous_read` owns
		/// the contract rule; asking it here with an eligible operation keeps
		/// the factory and the binding reading the same table.
		[[nodiscard]] auto anonymous_read_contract_admitted(
			const validated_github_host_profile& aProfile) noexcept -> bool
		{
			return admit_anonymous_read(
					   read_operation::get_repository,
					   aProfile.server_contract())
				.has_value();
		}

#if defined(BPR_NATIVE_HTTPS_TESTING)
		[[nodiscard]] auto make_test_policy(
			const native_https_test_configuration& aTest) -> transport_policy
		{
			return {
				.Proxy                     = proxy_policy::fixed_test,
				.ProxyHost                 = aTest.ProxyHost,
				.ProxyPort                 = aTest.ProxyPort,
				.FallbackProxyPort         = aTest.FallbackProxyPort,
				.DirectFirstProxyCandidate = aTest.DirectFirstProxyCandidate,
				.TrustedCaCertificatePem   = aTest.TrustedCaCertificatePem,
				.TotalTimeout              = aTest.TotalTimeout,
				.SmartTotalTimeout         = aTest.TotalTimeout,
				.ResponseBodyLimit         = aTest.ResponseBodyLimit,
			};
		}
#endif

		template <typename Credential>
		[[nodiscard]] auto make_transport(
			const validated_github_host_profile& aProfile,
			Credential                           aCredential,
			transport_policy                     aPolicy) noexcept
			-> native_https_transport_factory_result
		{
			if (!profile_policy_valid(aProfile)) {
				return std::unexpected{
					aProfile.authentication_transport()
							!= application::github_authentication_transport::
								native_https
						? native_https_configuration_failure::
							  profile_transport_mismatch
						: native_https_configuration_failure::
							  profile_policy_mismatch
				};
			}
			try {
				auto implementation = std::make_unique<native_https_transport::implementation>(
					aProfile,
					std::move(aCredential),
					std::move(aPolicy));
				if (!implementation->start()) {
					return std::unexpected{
						native_https_configuration_failure::allocation_failure
					};
				}
				return std::make_unique<native_https_transport>(
					std::move(implementation));
			} catch (...) {
				return std::unexpected{
					native_https_configuration_failure::allocation_failure
				};
			}
		}
	} // namespace

	native_https_transport::native_https_transport(
		std::unique_ptr<implementation> aImplementation) noexcept
		: Implementation{ std::move(aImplementation) }
	{
	}

	native_https_transport::~native_https_transport() = default;

	auto native_https_transport::can_acquire_smart(
		const application::git_smart_request_context&     aContext,
		const std::optional<core::application_session_id> aAuthenticationSession) const noexcept
		-> bool
	{
		return Implementation != nullptr
			&& Implementation->can_acquire_smart(aContext, aAuthenticationSession);
	}

	auto native_https_transport::discover_for_lease(
		const application::git_smart_discovery_request&      aRequest,
		const std::shared_ptr<native_git_smart_lease_state>& aLease,
		const std::stop_token                                aStop)
		-> application::git_smart_transport_result<
			application::git_smart_discovery_response>
	{
		if (Implementation == nullptr) {
			return std::unexpected{ smart_error(
				application::git_smart_transport_failure::unavailable) };
		}
		return Implementation->discover(aRequest, aLease, aStop);
	}

	auto native_https_transport::ls_refs_for_lease(
		const application::git_smart_ls_refs_request&        aRequest,
		const std::shared_ptr<native_git_smart_lease_state>& aLease,
		const std::stop_token                                aStop)
		-> application::git_smart_transport_result<
			application::git_smart_ls_refs_response>
	{
		if (Implementation == nullptr) {
			return std::unexpected{ smart_error(
				application::git_smart_transport_failure::unavailable) };
		}
		return Implementation->ls_refs(aRequest, aLease, aStop);
	}

	auto native_https_transport::fetch_for_lease(
		const application::git_smart_fetch_request&          aRequest,
		const std::shared_ptr<native_git_smart_lease_state>& aLease,
		application::git_smart_wire_consumer&                aConsumer,
		const std::stop_token                                aStop)
		-> application::git_smart_transport_result<void>
	{
		if (Implementation == nullptr) {
			return std::unexpected{ smart_error(
				application::git_smart_transport_failure::unavailable) };
		}
		return Implementation->fetch(aRequest, aLease, aConsumer, aStop);
	}

	void native_https_transport::cancel_lease(
		const std::shared_ptr<native_git_smart_lease_state>& aLease) noexcept
	{
		if (Implementation != nullptr) {
			Implementation->cancel_lease(aLease);
		}
	}

	auto make_native_git_smart_transport(
		std::shared_ptr<native_https_transport>           aTransport,
		const application::git_smart_request_context&     aContext,
		const std::optional<core::application_session_id> aAuthenticationSession) noexcept
		-> application::git_smart_transport_result<
			application::git_smart_transport>
	{
		return native_git_smart_transport::acquire(
			std::move(aTransport), aContext, aAuthenticationSession);
	}

	auto native_https_transport::perform(const github_wire_request& aRequest,
										 const std::stop_token      aStop)
		-> github_wire_completion
	{
		if (Implementation == nullptr) {
			return unavailable(aRequest.Correlation);
		}
		return Implementation->perform(aRequest, aStop);
	}

	auto native_https_transport::perform_capability_probe(
		const github_capability_probe_wire_request& aRequest,
		const std::stop_token                       aStop) -> github_capability_probe_wire_completion
	{
		if (Implementation == nullptr) {
			return capability_error(aRequest.Correlation,
									application::github_provider_failure::network);
		}
		return Implementation->perform_capability_probe(aRequest, aStop);
	}

	auto native_https_transport::perform_publication(
		const github_publication_wire_call& aCall,
		const std::stop_token               aStop)
		-> github_publication_wire_completion
	{
		if (Implementation == nullptr) {
			return github_publication_wire_error{ };
		}
		return Implementation->perform_publication(aCall, aStop);
	}

	auto native_https_transport::perform_review_mutation(
		const github_mutation_wire_request&  aRequest,
		const std::stop_token                aStop,
		github_mutation_transmission_witness aWitness)
		-> github_mutation_wire_completion
	{
		if (Implementation == nullptr) {
			return github_mutation_wire_error{
				.Failure = application::review_mutation_failure::unavailable,
			};
		}
		return Implementation->perform_review_mutation(
			aRequest, aStop, std::move(aWitness));
	}

	auto native_https_transport::perform_device_authorization(
		const device_authorization_wire_request& aRequest,
		const std::stop_token                    aStop)
		-> native_https_device_authorization_completion
	{
		if (Implementation == nullptr) {
			return device_authorization_error(
				application::github_provider_failure::unavailable);
		}
		return Implementation->perform_device_authorization(aRequest, aStop);
	}

	auto make_native_https_bootstrap_transport(
		const validated_github_host_profile&    aProfile,
		core::github_session_request_context    aContext,
		application::github_credential_material aCredential) noexcept
		-> native_https_transport_factory_result
	{
		if (
			!application::detail::valid_context(aContext)
			|| aContext.Profile != aProfile.profile()) {
			return std::unexpected{
				native_https_configuration_failure::
					invalid_bootstrap_context
			};
		}
		if (!valid_bearer_material(aCredential)) {
			return std::unexpected{
				native_https_configuration_failure::
					invalid_credential_material
			};
		}
		return make_transport(
			aProfile,
			bootstrap_credential{
				.Context  = aContext,
				.Material = std::move(aCredential),
			},
			make_production_policy());
	}

	auto make_native_https_account_transport(
		const validated_github_host_profile&            aProfile,
		application::github_credential_retrieve_success aCredential) noexcept
		-> native_https_transport_factory_result
	{
		if (!application::valid(aCredential)) {
			return std::unexpected{
				native_https_configuration_failure::
					invalid_account_credential
			};
		}
		if (!authority_matches_profile(aCredential.Context.Authority, aProfile)) {
			return std::unexpected{
				native_https_configuration_failure::
					credential_authority_mismatch
			};
		}
		if (!valid_bearer_material(aCredential.Credential)) {
			return std::unexpected{
				native_https_configuration_failure::
					invalid_credential_material
			};
		}
		return make_transport(
			aProfile,
			account_credential{
				.Context  = aCredential.Context,
				.Material = std::move(aCredential.Credential),
			},
			make_production_policy());
	}

	auto make_native_https_credential_free_transport(
		const validated_github_host_profile& aProfile,
		core::github_session_request_context aContext) noexcept
		-> native_https_transport_factory_result
	{
		if (
			!application::detail::valid_context(aContext)
			|| aContext.Profile != aProfile.profile()) {
			return std::unexpected{
				native_https_configuration_failure::
					invalid_bootstrap_context
			};
		}
		return make_transport(
			aProfile,
			credential_free_request{ .Context = aContext },
			make_production_policy());
	}

	auto make_native_https_anonymous_read_transport(
		const validated_github_host_profile& aProfile,
		core::github_session_request_context aContext) noexcept
		-> native_https_transport_factory_result
	{
		if (
			!application::detail::valid_context(aContext)
			|| aContext.Profile != aProfile.profile()) {
			return std::unexpected{
				native_https_configuration_failure::
					invalid_bootstrap_context
			};
		}
		if (!anonymous_read_contract_admitted(aProfile)) {
			return std::unexpected{
				native_https_configuration_failure::
					anonymous_read_contract_unsupported
			};
		}
		return make_transport(
			aProfile,
			anonymous_read_request{ .Context = aContext },
			make_production_policy());
	}

	auto make_native_https_anonymous_smart_transport(
		const validated_github_host_profile& aProfile,
		core::github_session_request_context aContext) noexcept
		-> native_https_transport_factory_result
	{
		if (
			!application::detail::valid_context(aContext)
			|| aContext.Profile != aProfile.profile()) {
			return std::unexpected{
				native_https_configuration_failure::
					invalid_bootstrap_context
			};
		}
		if (!anonymous_read_contract_admitted(aProfile)) {
			return std::unexpected{
				native_https_configuration_failure::
					anonymous_read_contract_unsupported
			};
		}
		return make_transport(
			aProfile,
			anonymous_smart_request{ .Context = aContext },
			make_production_policy());
	}

#if defined(BPR_NATIVE_HTTPS_TESTING)
	auto make_native_https_bootstrap_transport_for_test(
		const validated_github_host_profile&    aProfile,
		core::github_session_request_context    aContext,
		application::github_credential_material aCredential,
		const native_https_test_configuration&  aConfiguration) noexcept
		-> native_https_transport_factory_result
	{
		if (
			!application::detail::valid_context(aContext)
			|| aContext.Profile != aProfile.profile()) {
			return std::unexpected{
				native_https_configuration_failure::
					invalid_bootstrap_context
			};
		}
		if (!valid_bearer_material(aCredential)) {
			return std::unexpected{
				native_https_configuration_failure::
					invalid_credential_material
			};
		}
		if (!valid_test_configuration(aConfiguration)) {
			return std::unexpected{
				native_https_configuration_failure::
					invalid_test_configuration
			};
		}
		return make_transport(
			aProfile,
			bootstrap_credential{
				.Context  = aContext,
				.Material = std::move(aCredential),
			},
			make_test_policy(aConfiguration));
	}

	auto make_native_https_account_transport_for_test(
		const validated_github_host_profile&            aProfile,
		application::github_credential_retrieve_success aCredential,
		const native_https_test_configuration&          aConfiguration) noexcept
		-> native_https_transport_factory_result
	{
		if (!application::valid(aCredential)) {
			return std::unexpected{
				native_https_configuration_failure::
					invalid_account_credential
			};
		}
		if (!authority_matches_profile(aCredential.Context.Authority, aProfile)) {
			return std::unexpected{
				native_https_configuration_failure::
					credential_authority_mismatch
			};
		}
		if (!valid_bearer_material(aCredential.Credential)) {
			return std::unexpected{
				native_https_configuration_failure::
					invalid_credential_material
			};
		}
		if (!valid_test_configuration(aConfiguration)) {
			return std::unexpected{
				native_https_configuration_failure::
					invalid_test_configuration
			};
		}
		return make_transport(
			aProfile,
			account_credential{
				.Context  = aCredential.Context,
				.Material = std::move(aCredential.Credential),
			},
			make_test_policy(aConfiguration));
	}

	auto make_native_https_credential_free_transport_for_test(
		const validated_github_host_profile&   aProfile,
		core::github_session_request_context   aContext,
		const native_https_test_configuration& aConfiguration) noexcept
		-> native_https_transport_factory_result
	{
		if (
			!application::detail::valid_context(aContext)
			|| aContext.Profile != aProfile.profile()) {
			return std::unexpected{
				native_https_configuration_failure::
					invalid_bootstrap_context
			};
		}
		if (!valid_test_configuration(aConfiguration)) {
			return std::unexpected{
				native_https_configuration_failure::
					invalid_test_configuration
			};
		}
		return make_transport(
			aProfile,
			credential_free_request{ .Context = aContext },
			make_test_policy(aConfiguration));
	}

	auto make_native_https_anonymous_read_transport_for_test(
		const validated_github_host_profile&   aProfile,
		core::github_session_request_context   aContext,
		const native_https_test_configuration& aConfiguration) noexcept
		-> native_https_transport_factory_result
	{
		if (
			!application::detail::valid_context(aContext)
			|| aContext.Profile != aProfile.profile()) {
			return std::unexpected{
				native_https_configuration_failure::
					invalid_bootstrap_context
			};
		}
		if (!anonymous_read_contract_admitted(aProfile)) {
			return std::unexpected{
				native_https_configuration_failure::
					anonymous_read_contract_unsupported
			};
		}
		if (!valid_test_configuration(aConfiguration)) {
			return std::unexpected{
				native_https_configuration_failure::
					invalid_test_configuration
			};
		}
		return make_transport(
			aProfile,
			anonymous_read_request{ .Context = aContext },
			make_test_policy(aConfiguration));
	}

	auto make_native_https_anonymous_smart_transport_for_test(
		const validated_github_host_profile&   aProfile,
		core::github_session_request_context   aContext,
		const native_https_test_configuration& aConfiguration) noexcept
		-> native_https_transport_factory_result
	{
		if (
			!application::detail::valid_context(aContext)
			|| aContext.Profile != aProfile.profile()) {
			return std::unexpected{
				native_https_configuration_failure::
					invalid_bootstrap_context
			};
		}
		if (!anonymous_read_contract_admitted(aProfile)) {
			return std::unexpected{
				native_https_configuration_failure::
					anonymous_read_contract_unsupported
			};
		}
		if (!valid_test_configuration(aConfiguration)) {
			return std::unexpected{
				native_https_configuration_failure::
					invalid_test_configuration
			};
		}
		return make_transport(
			aProfile,
			anonymous_smart_request{ .Context = aContext },
			make_test_policy(aConfiguration));
	}
#endif

	auto observe_native_https_runtime(
		const validated_github_host_profile& aProfile) noexcept
		-> native_https_runtime_facts
	{
		native_https_runtime_facts facts;
		if (!profile_policy_valid(aProfile)) {
			return facts;
		}
		try {
			facts.TlsSupported = QSslSocket::supportsSsl();
			const auto backend = QSslSocket::activeBackend().toLower();
			if (backend == QStringLiteral("securetransport")) {
				facts.TlsBackend = native_https_tls_backend::secure_transport;
			} else if (backend == QStringLiteral("openssl")) {
				facts.TlsBackend = native_https_tls_backend::openssl;
			} else if (backend == QStringLiteral("schannel")) {
				facts.TlsBackend = native_https_tls_backend::schannel;
			} else if (!backend.isEmpty()) {
				facts.TlsBackend = native_https_tls_backend::other;
			}

			const auto url = QUrl::fromEncoded(
				QByteArray{
					aProfile.rest_root().data(),
					static_cast<qsizetype>(aProfile.rest_root().size()) },
				QUrl::StrictMode);
			const auto candidates = QNetworkProxyFactory::systemProxyForQuery(
				QNetworkProxyQuery{
					url,
					QNetworkProxyQuery::UrlRequest });
			const auto candidateCount = static_cast<std::uint64_t>(candidates.size());
			facts.ProxyCandidateCount = static_cast<std::uint32_t>(
				std::min(
					candidateCount,
					static_cast<std::uint64_t>(
						std::numeric_limits<std::uint32_t>::max())));
			if (candidates.isEmpty()) {
				facts.Proxy = native_https_system_proxy_kind::unavailable;
			} else {
				switch (candidates.front().type()) {
				case QNetworkProxy::NoProxy:
					facts.Proxy = native_https_system_proxy_kind::direct;
					break;
				case QNetworkProxy::HttpProxy:
					facts.Proxy = native_https_system_proxy_kind::http;
					break;
				case QNetworkProxy::Socks5Proxy:
					facts.Proxy = native_https_system_proxy_kind::socks5;
					break;
				case QNetworkProxy::DefaultProxy:
				case QNetworkProxy::HttpCachingProxy:
				case QNetworkProxy::FtpCachingProxy:
					facts.Proxy = native_https_system_proxy_kind::unsupported;
					break;
				}
			}
			return facts;
		} catch (...) {
			return facts;
		}
	}

	namespace {
		constexpr auto designated_public_observation_timeout
			= std::chrono::seconds{ 15 };
		constexpr auto designated_public_observation_limit
			= std::size_t{ 256U * 1024U };

		[[nodiscard]] auto public_observation_failure(
			const application::detail::designated_review_public_observation_failure aFailure)
			-> std::expected<
				void,
				application::detail::designated_review_public_observation_failure>
		{
			return std::unexpected(aFailure);
		}

		class native_designated_review_public_observer final
			: public application::detail::designated_review_public_observation_port
		{
		public:
			auto observe(const application::review_mutation_target_binding& aExpected)
				-> std::expected<
					void,
					application::detail::designated_review_public_observation_failure> override
			{
				using failure_type
					= application::detail::designated_review_public_observation_failure;
				if (QCoreApplication::instance() == nullptr
					|| QThread::currentThread() != QCoreApplication::instance()->thread()
					|| aExpected.Hostname.value() != "github.com"
					|| aExpected.ServerContract
						!= application::github_server_contract::github_com_2022_11_28) {
					return public_observation_failure(failure_type::unavailable);
				}
				const auto compiled = compile_target_observation_request(aExpected);
				if (!compiled.has_value() || compiled->Method != http_method::get
					|| !compiled->Body.empty()) {
					return public_observation_failure(failure_type::identity_mismatch);
				}
				const auto encoded = QByteArray{ "https://api.github.com" }
					+ QByteArray{ compiled->Path.data(),
								  static_cast<qsizetype>(compiled->Path.size()) };
				const auto url = QUrl::fromEncoded(encoded, QUrl::StrictMode);
				if (!url.isValid() || url.scheme() != QStringLiteral("https")
					|| url.host() != QStringLiteral("api.github.com")
					|| !url.userInfo().isEmpty() || !url.query().isEmpty()
					|| !url.fragment().isEmpty()) {
					return public_observation_failure(failure_type::identity_mismatch);
				}

				auto manager = QNetworkAccessManager{ };
				manager.setProxy(QNetworkProxy{ QNetworkProxy::NoProxy });
				auto request = QNetworkRequest{ url };
				request.setAttribute(
					QNetworkRequest::RedirectPolicyAttribute,
					QNetworkRequest::ManualRedirectPolicy);
				request.setAttribute(
					QNetworkRequest::CookieLoadControlAttribute,
					QNetworkRequest::Manual);
				request.setAttribute(
					QNetworkRequest::CookieSaveControlAttribute,
					QNetworkRequest::Manual);
				request.setAttribute(
					QNetworkRequest::AuthenticationReuseAttribute,
					QNetworkRequest::Manual);
				request.setAttribute(
					QNetworkRequest::CacheLoadControlAttribute,
					QNetworkRequest::AlwaysNetwork);
				request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
				request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
				request.setAttribute(QNetworkRequest::Http2DirectAttribute, false);
				request.setAttribute(QNetworkRequest::Http2CleartextAllowedAttribute, false);
				request.setMaximumRedirectsAllowed(0);
				request.setDecompressedSafetyCheckThreshold(0);
				request.setTransferTimeout(static_cast<int>(
					std::chrono::duration_cast<std::chrono::milliseconds>(
						designated_public_observation_timeout)
						.count()));
				for (const auto& header : compiled->Headers) {
					const auto name = QByteArray{ header.Name.data(),
												  static_cast<qsizetype>(header.Name.size()) };
					if (name.compare("Authorization", Qt::CaseInsensitive) == 0
						|| name.compare("Cookie", Qt::CaseInsensitive) == 0
						|| name.compare("Proxy-Authorization", Qt::CaseInsensitive) == 0) {
						return public_observation_failure(failure_type::identity_mismatch);
					}
					request.setRawHeader(
						name,
						QByteArray{ header.Value.data(),
									static_cast<qsizetype>(header.Value.size()) });
				}
				request.setRawHeader("Accept-Encoding", "identity");
				request.setRawHeader("Accept-Language", "*");
				auto tls = QSslConfiguration::defaultConfiguration();
				tls.setCaCertificates(QSslConfiguration::systemCaCertificates());
				tls.setLocalCertificateChain({ });
				tls.setPrivateKey(QSslKey{ });
				tls.setBackendConfiguration({ });
				tls.setSessionTicket({ });
				tls.setPreSharedKeyIdentityHint({ });
				tls.setAllowedNextProtocols(
					{ QByteArray{ QSslConfiguration::NextProtocolHttp1_1 } });
				tls.setPeerVerifyMode(QSslSocket::VerifyPeer);
				tls.setPeerVerifyDepth(0);
				tls.setProtocol(QSsl::TlsV1_2OrLater);
				tls.setSslOption(QSsl::SslOptionDisableCompression, true);
				tls.setSslOption(QSsl::SslOptionDisableSessionTickets, true);
				tls.setSslOption(QSsl::SslOptionDisableSessionSharing, true);
				tls.setSslOption(QSsl::SslOptionDisableSessionPersistence, true);
				tls.setSslOption(QSsl::SslOptionDisableLegacyRenegotiation, true);
				request.setSslConfiguration(tls);

				auto* const reply = manager.get(request);
				if (reply == nullptr) {
					return public_observation_failure(failure_type::unavailable);
				}
				auto body                = QByteArray{ };
				auto timedOut            = false;
				auto responseLimit       = false;
				auto transportRefused    = false;
				auto authentication      = false;
				auto proxyAuthentication = false;
				auto preSharedKey        = false;
				auto loop                = QEventLoop{ };
				auto timer               = QTimer{ };
				timer.setSingleShot(true);
				timer.start(static_cast<int>(
					std::chrono::duration_cast<std::chrono::milliseconds>(
						designated_public_observation_timeout)
						.count()));
				const auto drain = [&] {
					const auto chunk = reply->readAll();
					if (chunk.size()
						> static_cast<qsizetype>(designated_public_observation_limit)
							- body.size()) {
						responseLimit = true;
						reply->abort();
						return;
					}
					body.append(chunk);
				};
				QObject::connect(reply, &QNetworkReply::readyRead, &loop, drain);
				QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
				QObject::connect(&timer, &QTimer::timeout, &loop, [&] {
					timedOut = true;
					reply->abort();
					loop.quit();
				});
				QObject::connect(
					reply,
					&QNetworkReply::sslErrors,
					&loop,
					[&](const QList<QSslError>&) {
						transportRefused = true;
						reply->abort();
					});
				QObject::connect(
					&manager,
					&QNetworkAccessManager::authenticationRequired,
					&loop,
					[&](QNetworkReply*, QAuthenticator*) {
						authentication = true;
						reply->abort();
					});
				QObject::connect(
					&manager,
					&QNetworkAccessManager::proxyAuthenticationRequired,
					&loop,
					[&](const QNetworkProxy&, QAuthenticator*) {
						proxyAuthentication = true;
						reply->abort();
					});
				QObject::connect(
					reply,
					&QNetworkReply::preSharedKeyAuthenticationRequired,
					&loop,
					[&](QSslPreSharedKeyAuthenticator*) {
						preSharedKey = true;
						reply->abort();
					});
				loop.exec(QEventLoop::ExcludeUserInputEvents);
				timer.stop();
				drain();

				if (timedOut) {
					return public_observation_failure(failure_type::timeout);
				}
				if (responseLimit) {
					return public_observation_failure(failure_type::response_limit);
				}
				if (transportRefused || authentication || proxyAuthentication
					|| preSharedKey || reply->error() != QNetworkReply::NoError) {
					return public_observation_failure(failure_type::network);
				}
				const auto status = reply->attribute(
					QNetworkRequest::HttpStatusCodeAttribute);
				const auto redirect = reply->attribute(
					QNetworkRequest::RedirectionTargetAttribute);
				if (redirect.isValid()
					|| (status.isValid() && status.toInt() >= 300 && status.toInt() < 400)) {
					return public_observation_failure(failure_type::redirect);
				}
				if (!status.isValid() || status.toInt() != 200) {
					return public_observation_failure(failure_type::http);
				}
				const auto encoding = reply->rawHeader("Content-Encoding");
				if (!encoding.isEmpty()
					&& encoding.compare("identity", Qt::CaseInsensitive) != 0) {
					return public_observation_failure(failure_type::malformed);
				}
				auto       headers = std::vector<http_header>{ };
				const auto pairs   = reply->rawHeaderPairs();
				if (pairs.size() > 64) {
					return public_observation_failure(failure_type::response_limit);
				}
				for (const auto& [name, value] : pairs) {
					if (name.size() > 256 || value.size() > 8U * 1024U) {
						return public_observation_failure(failure_type::response_limit);
					}
					headers.push_back({
						.Name  = name.toStdString(),
						.Value = value.toStdString(),
					});
				}
				const auto parsed = parse_target_observation(
					review_mutation_wire_response{
						.Status  = status.toInt(),
						.Headers = std::move(headers),
						.Body    = body.toStdString(),
					},
					aExpected);
				if (!parsed.has_value()) {
					return public_observation_failure(
						parsed.error() == review_mutation_contract_failure::identity_mismatch
							? failure_type::identity_mismatch
							: failure_type::malformed);
				}
				if (parsed->Target.Semantic.ExpectedHead
					!= aExpected.Semantic.ExpectedHead) {
					return public_observation_failure(failure_type::identity_mismatch);
				}
				return { };
			}
		};
	} // namespace

	auto make_designated_review_public_observer()
		-> std::unique_ptr<application::detail::designated_review_public_observation_port>
	{
		return std::make_unique<native_designated_review_public_observer>();
	}

	namespace {
		using update_reason = core::release_update_reason;
		struct release_hop_result
		{
			release_update_wire_result Result;
			QUrl                       Redirect{ };
			bool                       ProxyFailure = false;
		};
		auto release_redirect_valid(const QUrl& aUrl) -> bool
		{
			return aUrl.isValid() && aUrl.scheme() == QStringLiteral("https")
				&& aUrl.host() == QStringLiteral("release-assets.githubusercontent.com")
				&& (aUrl.port() == -1 || aUrl.port() == 443) && aUrl.userInfo().isEmpty()
				&& !aUrl.hasFragment() && aUrl.path().startsWith(QLatin1Char('/'))
				&& aUrl.toEncoded().size() <= static_cast<qsizetype>(maximum_url_bytes);
		}
		auto release_tls(const transport_policy& aPolicy) -> QSslConfiguration
		{
			auto tls = QSslConfiguration::defaultConfiguration();
			tls.setCaCertificates(QSslConfiguration::systemCaCertificates());
#if defined(BPR_NATIVE_HTTPS_TESTING)
			if (!aPolicy.TrustedCaCertificatePem.empty()) {
				tls.addCaCertificates(QSslCertificate::fromData(QByteArray::fromStdString(aPolicy.TrustedCaCertificatePem)));
			}
#else
			static_cast<void>(aPolicy);
#endif
			tls.setLocalCertificateChain({ });
			tls.setPrivateKey(QSslKey{ });
			tls.setBackendConfiguration({ });
			tls.setSessionTicket({ });
			tls.setPreSharedKeyIdentityHint({ });
			tls.setAllowedNextProtocols({ QByteArray{ QSslConfiguration::NextProtocolHttp1_1 } });
			tls.setPeerVerifyMode(QSslSocket::VerifyPeer);
			tls.setProtocol(QSsl::TlsV1_2OrLater);
			tls.setSslOption(QSsl::SslOptionDisableCompression, true);
			tls.setSslOption(QSsl::SslOptionDisableSessionTickets, true);
			tls.setSslOption(QSsl::SslOptionDisableSessionSharing, true);
			tls.setSslOption(QSsl::SslOptionDisableSessionPersistence, true);
			return tls;
		}
		auto release_hop(const QUrl& aUrl, const QNetworkProxy& aProxy, bool aMetadata,
						 std::span<const std::byte> aCredential, std::uint64_t aLimit, std::stop_token aStop,
						 const release_update_consumer& aConsume, const transport_policy& aPolicy,
						 std::chrono::steady_clock::time_point aDeadline) -> release_hop_result
		{
			QNetworkAccessManager manager;
			manager.setProxy(aProxy);
			manager.setRedirectPolicy(QNetworkRequest::ManualRedirectPolicy);
			manager.setStrictTransportSecurityEnabled(false);
			QNetworkRequest request{ aUrl };
			request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
			request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
			request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
			request.setAttribute(QNetworkRequest::AuthenticationReuseAttribute, QNetworkRequest::Manual);
			request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
			request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
			request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
			request.setAttribute(QNetworkRequest::Http2DirectAttribute, false);
			request.setAttribute(QNetworkRequest::Http2CleartextAllowedAttribute, false);
			request.setMaximumRedirectsAllowed(0);
			request.setDecompressedSafetyCheckThreshold(0);
			request.setTransferTimeout(std::chrono::seconds{ 30 });
			request.setSslConfiguration(release_tls(aPolicy));
			request.setRawHeader("User-Agent", "BetterPullReview-release-updater");
			request.setRawHeader("Accept", aMetadata ? "application/vnd.github+json" : "application/octet-stream");
			request.setRawHeader("Accept-Encoding", "identity");
			request.setRawHeader("X-GitHub-Api-Version", QByteArray::fromStdString(std::string{ release_update_api_version }));
			if (!aCredential.empty()) {
				QByteArray authorization{ "Bearer " };
				authorization.append(reinterpret_cast<const char*>(aCredential.data()), static_cast<qsizetype>(aCredential.size()));
				request.setRawHeader("Authorization", authorization);
				wipe(authorization);
			}
			auto reply = std::unique_ptr<QNetworkReply>{ manager.get(request) };
			if (!reply) {
				return { { update_reason::unavailable } };
			}
			reply->setReadBufferSize(static_cast<qint64>(response_read_buffer_bytes));
			QEventLoop loop;
			QTimer     poll;
			poll.setInterval(5);
			release_hop_result result;
			bool               encrypted    = false;
			bool               inspected    = false;
			int                status       = 0;
			std::uint64_t      ignoredBytes = 0;
			auto               refuse       = [&](update_reason aReason) { result.Result.Failure = aReason; reply->abort(); };
			auto               inspect      = [&] {
				if (inspected || !reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid()) {
					return;
				}
				inspected               = true;
				status                  = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
				std::size_t headerBytes = 0;
				const auto  headers     = reply->rawHeaderPairs();
				for (const auto& [name, value] : headers) {
					headerBytes += static_cast<std::size_t>(name.size() + value.size());
					if (name.size() > 8192 || value.size() > 8192) {
						refuse(update_reason::size_limit);
						return;
					}
				}
				if (headers.size() > 64 || headerBytes > native_https_maximum_response_header_bytes) {
					refuse(update_reason::size_limit);
					return;
				}
				for (const auto* critical : { "content-length", "content-encoding", "content-type", "location", "retry-after", "x-ratelimit-remaining" }) {
					if (std::ranges::count_if(headers, [critical](const auto& aHeader) { return aHeader.first.compare(critical, Qt::CaseInsensitive) == 0; }) > 1) {
						refuse(update_reason::malformed_release);
						return;
					}
				}
				const auto encoding = reply->rawHeader("Content-Encoding");
				if (!encoding.isEmpty() && encoding != "identity") {
					refuse(update_reason::malformed_release);
					return;
				}
				if (status == 200) {
					const auto type = reply->rawHeader("Content-Type").split(';').front().trimmed().toLower();
					if (aMetadata ? (type != "application/json" && type != "application/vnd.github+json")
								  : (type != "application/octet-stream" && type != "application/zip")) {
						refuse(update_reason::malformed_release);
						return;
					}
				}
				bool       validLength  = false;
				const auto lengthHeader = reply->rawHeader("Content-Length");
				const auto length       = lengthHeader.toULongLong(&validLength);
				if (!lengthHeader.isEmpty() && (!validLength || length > (status == 200 ? aLimit : 65536U))) {
					refuse(update_reason::size_limit);
					return;
				}
				if (status == 429 || (status == 403 && (reply->hasRawHeader("Retry-After") || reply->rawHeader("X-RateLimit-Remaining") == "0"))) {
					bool       validRetry           = false;
					const auto retry                = reply->rawHeader("Retry-After").toULongLong(&validRetry);
					result.Result.RetryAfterSeconds = validRetry ? std::clamp<std::uint64_t>(retry, 1U, core::release_update_day_seconds) : core::release_update_day_seconds;
					refuse(update_reason::rate_limited);
				} else if (status == 401) {
					refuse(update_reason::unauthenticated);
				} else if (status == 403) {
					refuse(update_reason::forbidden);
				} else if (status == 404) {
					refuse(update_reason::inaccessible);
				} else if (recognized_redirect_status(status)) {
					const auto raw    = reply->rawHeader("Location");
					const auto target = QUrl::fromEncoded(raw, QUrl::StrictMode);
					if (aMetadata || raw.isEmpty() || !release_redirect_valid(target)) {
						refuse(update_reason::redirect);
					} else {
						result.Redirect = target;
					}
				} else if (status != 200) {
					refuse(update_reason::unavailable);
				}
			};
			auto read = [&] {
				inspect();
				while (result.Result.Failure == update_reason::none && reply->bytesAvailable() > 0) {
					const auto bytes = reply->read(static_cast<qint64>(response_read_chunk_bytes));
					if (bytes.isEmpty()) {
						break;
					}
					const auto count = static_cast<std::uint64_t>(bytes.size());
					if (status != 200) {
						ignoredBytes += count;
						if (ignoredBytes > 65536U) {
							refuse(update_reason::size_limit);
						}
					} else if (!encrypted || !inspected) {
						refuse(update_reason::certificate);
					} else if (count > aLimit - result.Result.Bytes) {
						refuse(update_reason::size_limit);
					} else {
						result.Result.Bytes += count;
						bool consumed = false;
						try {
							consumed = aConsume(std::as_bytes(std::span{ bytes.constData(), static_cast<std::size_t>(bytes.size()) }));
						} catch (...) { }
						if (!consumed) {
							refuse(update_reason::unavailable);
						}
					}
				}
			};
			QObject::connect(reply.get(), &QNetworkReply::encrypted, &loop, [&] { encrypted = true; });
			QObject::connect(reply.get(), &QNetworkReply::sslErrors, &loop, [&](const QList<QSslError>&) { refuse(update_reason::certificate); });
			QObject::connect(&manager, &QNetworkAccessManager::authenticationRequired, &loop, [&](QNetworkReply*, QAuthenticator*) { refuse(update_reason::unauthenticated); });
			QObject::connect(&manager, &QNetworkAccessManager::proxyAuthenticationRequired, &loop, [&](const QNetworkProxy&, QAuthenticator*) { refuse(update_reason::unavailable); });
			QObject::connect(reply.get(), &QNetworkReply::metaDataChanged, &loop, inspect);
			QObject::connect(reply.get(), &QNetworkReply::readyRead, &loop, read);
			QObject::connect(reply.get(), &QNetworkReply::finished, &loop, &QEventLoop::quit);
			QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
				if (aStop.stop_requested()) {
					refuse(update_reason::cancelled);
				} else if (std::chrono::steady_clock::now() >= aDeadline) {
					refuse(update_reason::timed_out);
				}
			});
			poll.start();
			if (!reply->isFinished()) {
				loop.exec();
			}
			read();
			poll.stop();
			if (aStop.stop_requested()) {
				result.Result.Failure = update_reason::cancelled;
			}
			if (result.Result.Failure == update_reason::none && reply->error() != QNetworkReply::NoError) {
				result.ProxyFailure = status == 0 && result.Result.Bytes == 0
					&& (reply->error() == QNetworkReply::ProxyConnectionRefusedError || reply->error() == QNetworkReply::ProxyNotFoundError
						|| reply->error() == QNetworkReply::ProxyTimeoutError || reply->error() == QNetworkReply::ProxyConnectionClosedError);
				result.Result.Failure = reply->error() == QNetworkReply::SslHandshakeFailedError ? update_reason::certificate : update_reason::offline;
			}
			if (result.Result.Failure == update_reason::none && !tls_terminal_valid(*reply, aUrl, encrypted)) {
				result.Result.Failure = update_reason::certificate;
			}
			return result;
		}
		auto release_https(std::span<const std::byte> aCredential, std::uint64_t aAsset, std::uint64_t aLimit,
						   std::stop_token aStop, const release_update_consumer& aConsume, const transport_policy& aPolicy) -> release_update_wire_result
		{
			if (aCredential.empty() || aCredential.size() > application::maximum_github_credential_bytes || !aConsume
				|| aLimit == 0U || aLimit > (aAsset == 0U ? application::release_update_maximum_metadata_bytes : core::release_update_maximum_archive_bytes)
				|| !std::ranges::all_of(aCredential, [](std::byte aByte) { const auto byte = std::to_integer<unsigned char>(aByte); return ascii_alpha_numeric(byte) || byte == '-' || byte == '_' || byte == '.' || byte == '~' || byte == '+' || byte == '/' || byte == '='; })) {
				return { update_reason::unavailable };
			}
			const auto path     = aAsset == 0U ? std::string{ release_update_latest_path } : std::string{ release_update_asset_path_prefix } + std::to_string(aAsset);
			auto       url      = QUrl::fromEncoded(QByteArray::fromStdString(std::string{ release_update_api_origin } + path), QUrl::StrictMode);
			const auto deadline = std::chrono::steady_clock::now() + (aAsset == 0U ? aPolicy.TotalTimeout : aPolicy.SmartTotalTimeout);
			for (std::uint32_t hop = 0U; hop <= release_update_maximum_redirects; ++hop) {
				if (aStop.stop_requested()) {
					return { update_reason::cancelled };
				}
				if (std::chrono::steady_clock::now() >= deadline) {
					return { update_reason::timed_out };
				}
				auto proxies = system_proxy_candidates(url);
#if defined(BPR_NATIVE_HTTPS_TESTING)
				if (aPolicy.Proxy == proxy_policy::fixed_test) {
					proxies = std::vector{ QNetworkProxy{ QNetworkProxy::HttpProxy, QString::fromStdString(aPolicy.ProxyHost), aPolicy.ProxyPort } };
				}
#endif
				if (!proxies) {
					return { update_reason::unavailable };
				}
				release_hop_result result;
				for (const auto& proxy : *proxies) {
					result = release_hop(url, proxy, aAsset == 0U, hop == 0U ? aCredential : std::span<const std::byte>{ },
										 aLimit, aStop, aConsume, aPolicy, deadline);
					if (!result.ProxyFailure) {
						break;
					}
				}
				if (result.Result.Failure != update_reason::none || result.Redirect.isEmpty()) {
					return result.Result;
				}
				if (hop == release_update_maximum_redirects || !release_redirect_valid(result.Redirect)) {
					return { update_reason::redirect };
				}
				url = std::move(result.Redirect);
			}
			return { update_reason::redirect };
		}
	}
	auto perform_release_update_https(std::span<const std::byte> aCredential, std::uint64_t aAsset, std::uint64_t aLimit,
									  std::stop_token aStop, const release_update_consumer& aConsume) -> release_update_wire_result
	{
		transport_policy policy;
		policy.TotalTimeout      = std::chrono::seconds{ application::release_update_metadata_timeout_seconds };
		policy.SmartTotalTimeout = std::chrono::seconds{ application::release_update_download_timeout_seconds };
		return release_https(aCredential, aAsset, aLimit, aStop, aConsume, policy);
	}
#if defined(BPR_NATIVE_HTTPS_TESTING)
	auto perform_release_update_https_for_test(std::span<const std::byte> aCredential, std::uint64_t aAsset, std::uint64_t aLimit,
											   std::stop_token aStop, const release_update_consumer& aConsume, const native_https_test_configuration& aConfiguration) -> release_update_wire_result
	{
		if (!valid_test_configuration(aConfiguration)) {
			return { update_reason::unavailable };
		}
		transport_policy policy;
		policy.Proxy                   = proxy_policy::fixed_test;
		policy.ProxyHost               = aConfiguration.ProxyHost;
		policy.ProxyPort               = aConfiguration.ProxyPort;
		policy.TrustedCaCertificatePem = aConfiguration.TrustedCaCertificatePem;
		policy.TotalTimeout            = aConfiguration.TotalTimeout;
		policy.SmartTotalTimeout       = aConfiguration.TotalTimeout;
		return release_https(aCredential, aAsset, aLimit, aStop, aConsume, policy);
	}
#endif
} // namespace bpr::adapters::github::detail
