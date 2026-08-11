#include "github_wire_request_validation.hpp"
#include "native_https_transport.hpp"

#include <QCoreApplication>
#include <QMessageLogContext>
#include <QString>
#include <QtLogging>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <expected>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#if defined(_WIN32)
	#include <fcntl.h>
	#include <io.h>
#endif

namespace {
	using namespace bpr;
	using namespace bpr::adapters::github;
	using namespace bpr::adapters::github::detail;

	using namespace std::chrono_literals;

	static_assert(
		std::is_base_of_v<github_wire_transport_port, native_https_transport>);
	static_assert(!std::is_copy_constructible_v<native_https_transport>);
	static_assert(!std::is_move_constructible_v<native_https_transport>);
	static_assert(native_https_maximum_request_body_bytes == 256U * 1024U);
	static_assert(native_https_maximum_response_body_bytes == 4U * 1024U * 1024U);
	static_assert(native_https_maximum_error_response_body_bytes == 64U * 1024U);
	static_assert(native_https_maximum_response_header_bytes == 64U * 1024U);
	static_assert(native_https_maximum_redirects == 3U);

	class test_failure final
	{
	public:
		explicit test_failure(const std::string_view aMessage) noexcept
			: Message{ aMessage }
		{
		}

		[[nodiscard]] auto message() const noexcept -> std::string_view
		{
			return Message;
		}

	private:
		std::string_view Message;
	};

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			throw test_failure{ aMessage };
		}
	}

	void discard_qt_message(
		QtMsgType,
		const QMessageLogContext&,
		const QString&)
	{
	}

	void select_binary_standard_streams()
	{
#if defined(_WIN32)
		expect(
			_setmode(_fileno(stdin), _O_BINARY) != -1,
			"binary-stdin");
		expect(
			_setmode(_fileno(stdout), _O_BINARY) != -1,
			"binary-stdout");
		expect(
			_setmode(_fileno(stderr), _O_BINARY) != -1,
			"binary-stderr");
#endif
	}

	[[nodiscard]] auto profile_candidate(
		const application::github_authentication_transport aAuthentication)
		-> github_host_profile_candidate
	{
		return {
			.Profile  = core::host_profile_id{ 11 },
			.Host     = core::git_host_id{ 21 },
			.Revision = application::github_profile_revision{ 31 },
			.Server   = application::github_server_contract::
				ghes_3_18_2022_11_28,
			.Authentication = aAuthentication,
			.Proxy          = application::github_proxy_mode::system,
			.Trust          = application::github_trust_mode::system,
			.Provenance     = github_profile_provenance::managed,
			.Hostname       = "ghe.bpr.invalid",
			.RestRoot       = "https://ghe.bpr.invalid/api/v3",
			.GraphqlRoot    = "https://ghe.bpr.invalid/api/graphql",
		};
	}

	[[nodiscard]] auto session_context(
		const validated_github_host_profile& aProfile)
		-> core::github_session_request_context
	{
		return {
			.Request            = core::request_id{ 101 },
			.ApplicationSession = core::application_session_id{ 102 },
			.Profile            = aProfile.profile(),
			.IdentityGeneration = core::github_identity_generation{ 103 },
		};
	}

	[[nodiscard]] auto pull_request_context(
		const validated_github_host_profile& aProfile)
		-> core::pull_request_request_context
	{
		return {
			.Request            = core::request_id{ 201 },
			.ApplicationSession = core::application_session_id{ 202 },
			.Profile            = aProfile.profile(),
			.Host               = aProfile.host(),
			.Account            = core::github_account_id{ 203 },
			.Repository         = core::repository_locator{
				.Owner = "octo-org",
				.Name  = "review-repository",
			},
			.Number              = core::pull_request_number{ 17 },
			.ExpectedRepository  = core::repository_id{ 204 },
			.ExpectedPullRequest = core::pull_request_id{ 205 },
			.Capabilities        = core::capability_revision{ 206 },
			.IdentityGeneration  = core::github_identity_generation{ 207 },
			.Generation          = core::request_generation{ 208 },
		};
	}

	[[nodiscard]] auto semantic_request(
		const read_operation aOperation) -> semantic_read_request
	{
		semantic_read_request semantic{
			.Server            = server_contract::ghes_3_18,
			.Operation         = aOperation,
			.Owner             = { },
			.Repository        = { },
			.PullRequestNumber = 0U,
			.HeadOid           = { },
			.SearchTerms       = { },
			.Page              = 1U,
			.GraphqlPage       = std::nullopt,
		};
		if (aOperation != read_operation::get_viewer_identity) {
			semantic.Owner      = "octo-org";
			semantic.Repository = "review-repository";
		}
		if (aOperation == read_operation::list_review_threads) {
			semantic.PullRequestNumber = 17U;
		}
		return semantic;
	}

	[[nodiscard]] auto wire_request_for(
		const validated_github_host_profile& aProfile,
		const read_operation                 aOperation) -> github_wire_request
	{
		const auto wire = compile_read_request(semantic_request(aOperation));
		expect(wire.has_value(), "canonical-wire-request");
		return {
			.Correlation = github_wire_correlation{
				.Parent          = aOperation == read_operation::get_viewer_identity
					? github_wire_parent_context{ session_context(aProfile) }
					: github_wire_parent_context{
						  pull_request_context(aProfile) },
				.ProfileRevision = aProfile.revision(),
				.Host            = aProfile.host(),
				.Operation       = aOperation,
				.Page            = 1U,
			},
			.Wire = *wire,
		};
	}

	[[nodiscard]] auto
	capability_request_for(const validated_github_host_profile&        aProfile,
						   const core::github_session_request_context& aSession,
						   const core::github_account_id               aObservedAccount)
		-> github_capability_probe_wire_request
	{
		const auto wire = compile_capability_probe_request(aProfile);
		expect(wire.has_value(), "canonical-capability-request");
		return {
			.Correlation = github_capability_probe_wire_correlation{
				.Session         = aSession,
				.ProfileRevision = aProfile.revision(),
				.Host            = aProfile.host(),
				.ObservedAccount = aObservedAccount,
			},
			.Wire = *wire,
		};
	}

	[[nodiscard]] auto credential_material(const std::string_view aValue)
		-> application::github_credential_material
	{
		const auto characters = std::span<const char>{ aValue.data(), aValue.size() };
		auto       material   = application::github_credential_material::create(
			std::as_bytes(characters));
		expect(material.has_value(), "credential-material");
		return std::move(*material);
	}

	[[nodiscard]] auto
	account_operation_context(const validated_github_host_profile&      aProfile,
							  const core::pull_request_request_context& aRequest)
		-> application::github_credential_operation_context
	{
		return {
			.Request            = core::request_id{ 301 },
			.ApplicationSession = aRequest.ApplicationSession,
			.Authority          = application::github_credential_authority_key{
				.Profile         = aProfile.profile(),
				.ProfileRevision = aProfile.revision(),
				.Host            = aProfile.host(),
				.Hostname        = application::github_canonical_hostname{
					aProfile.hostname() },
				.Account        = aRequest.Account,
				.ServerContract = aProfile.server_contract(),
			},
			.IdentityGeneration = aRequest.IdentityGeneration,
		};
	}

	[[nodiscard]] auto
	account_operation_context(const validated_github_host_profile&        aProfile,
							  const core::github_session_request_context& aRequest,
							  const core::github_account_id               aAccount)
		-> application::github_credential_operation_context
	{
		return {
			.Request            = aRequest.Request,
			.ApplicationSession = aRequest.ApplicationSession,
			.Authority          = application::github_credential_authority_key{
				.Profile         = aProfile.profile(),
				.ProfileRevision = aProfile.revision(),
				.Host            = aProfile.host(),
				.Hostname        = application::github_canonical_hostname{
					aProfile.hostname() },
				.Account        = aAccount,
				.ServerContract = aProfile.server_contract(),
			},
			.IdentityGeneration = aRequest.IdentityGeneration,
		};
	}

	[[nodiscard]] auto test_configuration() -> native_https_test_configuration
	{
		return {
			.ProxyHost               = "127.0.0.1",
			.ProxyPort               = 9U,
			.FallbackProxyPort       = 0U,
			.TrustedCaCertificatePem = { },
			.TotalTimeout            = 1s,
			.ResponseBodyLimit       = native_https_maximum_response_body_bytes,
		};
	}

	void expect_unavailable(const github_wire_completion&  aCompletion,
							const github_wire_correlation& aCorrelation,
							const std::string_view         aMessage)
	{
		const auto* error = std::get_if<github_wire_error>(&aCompletion);
		expect(error != nullptr && error->Correlation == aCorrelation
				   && error->Error.Failure
					   == application::github_provider_failure::unavailable
				   && !error->Error.RetryAfterSeconds.has_value(),
			   aMessage);
	}

	void expect_capability_error(
		const github_capability_probe_wire_completion&  aCompletion,
		const github_capability_probe_wire_correlation& aCorrelation,
		const application::github_provider_failure      aError,
		const std::optional<std::uint32_t>              aRetryAfter,
		const std::string_view                          aMessage)
	{
		const auto* failure = std::get_if<github_capability_probe_wire_error>(&aCompletion);
		expect(failure != nullptr && failure->Correlation == aCorrelation
				   && failure->Error.Failure == aError
				   && failure->Error.RetryAfterSeconds == aRetryAfter,
			   aMessage);
	}

	void test_factory_contracts_and_preflight()
	{
		const auto native = validate_github_host_profile(
			profile_candidate(
				application::github_authentication_transport::native_https));
		const auto cli = validate_github_host_profile(
			profile_candidate(
				application::github_authentication_transport::github_cli));
		expect(native.has_value() && cli.has_value(), "profile-validation");

		const auto invalidRuntime = observe_native_https_runtime(*cli);
		expect(
			invalidRuntime == native_https_runtime_facts{ },
			"runtime-profile-policy");
		const auto runtime = observe_native_https_runtime(*native);
		if (runtime.TlsSupported) {
			expect(
				runtime.TlsBackend
					!= native_https_tls_backend::unavailable,
				"runtime-tls-backend");
		}
		if (runtime.ProxyCandidateCount == 0U) {
			expect(
				runtime.Proxy
					== native_https_system_proxy_kind::unavailable,
				"runtime-empty-proxy");
		}

		const auto configuration = test_configuration();
		const auto cliFactory    = make_native_https_bootstrap_transport_for_test(
			*cli,
			session_context(*cli),
			credential_material("BPR0026FixtureValue"),
			configuration);
		expect(
			!cliFactory.has_value()
				&& cliFactory.error()
					== native_https_configuration_failure::
						profile_transport_mismatch,
			"profile-transport-mismatch");

		auto invalidConfiguration       = configuration;
		invalidConfiguration.ProxyHost  = "proxy.bpr.invalid";
		const auto configurationFactory = make_native_https_bootstrap_transport_for_test(
			*native,
			session_context(*native),
			credential_material("BPR0026FixtureValue"),
			invalidConfiguration);
		expect(
			!configurationFactory.has_value()
				&& configurationFactory.error()
					== native_https_configuration_failure::
						invalid_test_configuration,
			"invalid-test-configuration");

		auto duplicateProxyConfiguration              = configuration;
		duplicateProxyConfiguration.FallbackProxyPort = duplicateProxyConfiguration.ProxyPort;
		const auto duplicateProxyFactory              = make_native_https_bootstrap_transport_for_test(
			*native,
			session_context(*native),
			credential_material("BPR0026FixtureValue"),
			duplicateProxyConfiguration);
		expect(
			!duplicateProxyFactory.has_value()
				&& duplicateProxyFactory.error()
					== native_https_configuration_failure::
						invalid_test_configuration,
			"duplicate-test-proxy");

		auto invalidSession       = session_context(*native);
		invalidSession.Profile    = core::host_profile_id{ 999 };
		const auto contextFactory = make_native_https_bootstrap_transport_for_test(
			*native,
			invalidSession,
			credential_material("BPR0026FixtureValue"),
			configuration);
		expect(
			!contextFactory.has_value()
				&& contextFactory.error()
					== native_https_configuration_failure::
						invalid_bootstrap_context,
			"invalid-bootstrap-context");

		const auto materialFactory = make_native_https_bootstrap_transport_for_test(
			*native,
			session_context(*native),
			credential_material("invalid material"),
			configuration);
		expect(
			!materialFactory.has_value()
				&& materialFactory.error()
					== native_https_configuration_failure::
						invalid_credential_material,
			"invalid-credential-material");

		const auto pullRequest             = pull_request_context(*native);
		auto       mismatchedOperation     = account_operation_context(*native, pullRequest);
		mismatchedOperation.Authority.Host = core::git_host_id{ 999 };
		const auto authorityFactory        = make_native_https_account_transport_for_test(
			*native,
			application::github_credential_retrieve_success{
				.Context    = mismatchedOperation,
				.Credential = credential_material("BPR0026FixtureValue"),
			},
			configuration);
		expect(
			!authorityFactory.has_value()
				&& authorityFactory.error()
					== native_https_configuration_failure::
						credential_authority_mismatch,
			"credential-authority-mismatch");

		auto mismatchedHostname               = account_operation_context(*native, pullRequest);
		mismatchedHostname.Authority.Hostname = application::github_canonical_hostname{ "other.bpr.invalid" };
		const auto hostnameAuthorityFactory   = make_native_https_account_transport_for_test(
			*native,
			application::github_credential_retrieve_success{
				.Context    = mismatchedHostname,
				.Credential = credential_material("BPR0026FixtureValue"),
			},
			configuration);
		expect(
			!hostnameAuthorityFactory.has_value()
				&& hostnameAuthorityFactory.error()
					== native_https_configuration_failure::
						credential_authority_mismatch,
			"credential-hostname-authority-mismatch");

		auto bootstrap = make_native_https_bootstrap_transport_for_test(
			*native,
			session_context(*native),
			credential_material("BPR0026FixtureValue"),
			configuration);
		expect(bootstrap.has_value(), "bootstrap-transport");

		const auto       viewer = wire_request_for(*native, read_operation::get_viewer_identity);
		std::stop_source stopped;
		expect(stopped.request_stop(), "pre-cancel-request");
		const auto  cancelled    = (*bootstrap)->perform(viewer, stopped.get_token());
		const auto* cancellation = std::get_if<github_wire_cancelled>(&cancelled);
		expect(
			cancellation != nullptr
				&& cancellation->Correlation == viewer.Correlation,
			"pre-cancel-correlation");

		auto invalidWire                        = viewer;
		invalidWire.Correlation.ProfileRevision = application::github_profile_revision{ 999 };
		const auto invalidCompletion            = (*bootstrap)->perform(invalidWire, std::stop_token{ });
		expect_unavailable(
			invalidCompletion,
			invalidWire.Correlation,
			"invalid-wire-preflight");

		const auto repository    = wire_request_for(*native, read_operation::get_repository);
		const auto accountParent = std::get<core::pull_request_request_context>(
			repository.Correlation.Parent);
		auto account = make_native_https_account_transport_for_test(
			*native,
			application::github_credential_retrieve_success{
				.Context    = account_operation_context(*native, accountParent),
				.Credential = credential_material("BPR0026FixtureValue"),
			},
			configuration);
		expect(account.has_value(), "account-transport");

		const auto mutationTarget = application::review_mutation_target_binding{
			.Semantic = {
				.Host         = pullRequest.Host,
				.Repository   = core::repository_id{ 204U },
				.PullRequest  = core::pull_request_id{ 205U },
				.ExpectedHead = *core::parse_git_commit_oid(
					"1111111111111111111111111111111111111111"),
			},
			.Repository               = pullRequest.Repository,
			.Number                   = pullRequest.Number,
			.RepositoryDatabaseId     = 204U,
			.RepositoryNodeId         = "R_fixture_204",
			.PullRequestDatabaseId    = 205U,
			.PullRequestNodeId        = "PR_fixture_205",
			.BaseRepositoryDatabaseId = 204U,
			.Open                     = true,
		};
		const auto mutationWire = compile_target_observation_request(mutationTarget);
		expect(mutationWire.has_value(), "mutation-wire-contract");
		const auto mutationRequest = github_mutation_wire_request{
			.Context = account_operation_context(*native, pullRequest),
			.Wire    = *mutationWire,
		};
		std::stop_source mutationStopped;
		expect(mutationStopped.request_stop(), "mutation-pre-cancel-request");
		const auto mutationCancelled = (*account)->perform_review_mutation(
			mutationRequest, mutationStopped.get_token());
		expect(std::holds_alternative<github_mutation_wire_cancelled>(
				   mutationCancelled),
			   "mutation-pre-cancelled-without-network");

		auto invalidMutation = mutationRequest;
		invalidMutation.Wire.Path += "/tampered";
		const auto invalidMutationCompletion = (*account)->perform_review_mutation(
			invalidMutation, std::stop_token{ });
		const auto* invalidMutationError = std::get_if<github_mutation_wire_error>(
			&invalidMutationCompletion);
		expect(invalidMutationError != nullptr
				   && invalidMutationError->Failure
					   == application::review_mutation_failure::unavailable,
			   "invalid-mutation-refused-before-network");

		auto accountDrift = repository;
		auto driftParent  = std::get<core::pull_request_request_context>(
			accountDrift.Correlation.Parent);
		driftParent.Account             = core::github_account_id{ 999 };
		accountDrift.Correlation.Parent = driftParent;
		const auto driftCompletion      = (*account)->perform(accountDrift, std::stop_token{ });
		expect_unavailable(
			driftCompletion,
			accountDrift.Correlation,
			"account-drift-preflight");

		const auto observedAccount = core::github_account_id{ 203 };
		const auto session         = session_context(*native);
		const auto capability      = capability_request_for(*native, session, observedAccount);
		expect(validate_github_capability_probe_wire_request(*native, capability)
				   .has_value(),
			   "capability-wire-validation");

		auto mutation                 = capability;
		mutation.Wire.Body            = R"({"query":"mutation { __typename }"})";
		const auto mutationCompletion = (*account)->perform_capability_probe(mutation, std::stop_token{ });
		expect_capability_error(mutationCompletion, mutation.Correlation,
								application::github_provider_failure::unavailable,
								std::nullopt, "capability-mutation-preflight");

		const auto bootstrapCapability = (*bootstrap)->perform_capability_probe(capability, std::stop_token{ });
		expect_capability_error(bootstrapCapability, capability.Correlation,
								application::github_provider_failure::unavailable,
								std::nullopt, "bootstrap-capability-denied");

		auto capabilityAccount = make_native_https_account_transport_for_test(
			*native,
			application::github_credential_retrieve_success{
				.Context    = account_operation_context(*native, session, observedAccount),
				.Credential = credential_material("BPR0027FixtureValue"),
			},
			configuration);
		expect(capabilityAccount.has_value(), "capability-account-transport");

		auto accountDriftCapability                        = capability;
		accountDriftCapability.Correlation.ObservedAccount = core::github_account_id{ 999 };
		const auto accountDriftProbe                       = (*capabilityAccount)
																 ->perform_capability_probe(accountDriftCapability,
																							std::stop_token{ });
		expect_capability_error(accountDriftProbe,
								accountDriftCapability.Correlation,
								application::github_provider_failure::unavailable,
								std::nullopt, "capability-account-drift");

		const auto expectSessionDrift =
			[&capabilityAccount](const github_capability_probe_wire_request& aDrift,
								 const std::string_view                      aMessage) {
				const auto completion = (*capabilityAccount)
											->perform_capability_probe(aDrift, std::stop_token{ });
				expect_capability_error(
					completion, aDrift.Correlation,
					application::github_provider_failure::unavailable, std::nullopt,
					aMessage);
			};
		auto requestDrift                        = capability;
		requestDrift.Correlation.Session.Request = core::request_id{ 999 };
		expectSessionDrift(requestDrift, "capability-session-request-drift");
		auto applicationSessionDrift                                   = capability;
		applicationSessionDrift.Correlation.Session.ApplicationSession = core::application_session_id{ 999 };
		expectSessionDrift(applicationSessionDrift,
						   "capability-application-session-drift");
		auto identityGenerationDrift                                   = capability;
		identityGenerationDrift.Correlation.Session.IdentityGeneration = core::github_identity_generation{ 999 };
		expectSessionDrift(identityGenerationDrift,
						   "capability-identity-generation-drift");

		std::stop_source probeStop;
		expect(probeStop.request_stop(), "capability-pre-cancel-request");
		const auto  capabilityCancelled = (*capabilityAccount)
											  ->perform_capability_probe(capability, probeStop.get_token());
		const auto* probeCancellation   = std::get_if<github_capability_probe_wire_cancelled>(
			&capabilityCancelled);
		expect(probeCancellation != nullptr
				   && probeCancellation->Correlation == capability.Correlation,
			   "capability-pre-cancel-correlation");
	}

	[[nodiscard]] constexpr auto tls_backend_name(
		const native_https_tls_backend aBackend) noexcept -> std::string_view
	{
		switch (aBackend) {
		case native_https_tls_backend::unavailable:
			return "unavailable";
		case native_https_tls_backend::secure_transport:
			return "secure-transport";
		case native_https_tls_backend::openssl:
			return "openssl";
		case native_https_tls_backend::schannel:
			return "schannel";
		case native_https_tls_backend::other:
			return "other";
		}
		return "invalid";
	}

	[[nodiscard]] constexpr auto proxy_kind_name(
		const native_https_system_proxy_kind aProxy) noexcept -> std::string_view
	{
		switch (aProxy) {
		case native_https_system_proxy_kind::unavailable:
			return "unavailable";
		case native_https_system_proxy_kind::direct:
			return "direct";
		case native_https_system_proxy_kind::http:
			return "http";
		case native_https_system_proxy_kind::socks5:
			return "socks5";
		case native_https_system_proxy_kind::unsupported:
			return "unsupported";
		}
		return "invalid";
	}

	void print_runtime_facts()
	{
		const auto profile = validate_github_host_profile(
			profile_candidate(
				application::github_authentication_transport::native_https));
		expect(profile.has_value(), "runtime-profile");
		const auto runtime = observe_native_https_runtime(*profile);
		std::cout
			<< "native-https-runtime:tls="
			<< tls_backend_name(runtime.TlsBackend)
			<< ":proxy=" << proxy_kind_name(runtime.Proxy)
			<< ":candidates=" << runtime.ProxyCandidateCount
			<< ":supported=" << (runtime.TlsSupported ? "true" : "false")
			<< '\n';
	}

	enum class fixture_scenario : std::uint8_t {
		success,
		proxy_fallback,
		same_redirect,
		same_query_redirect,
		cross_redirect,
		graphql_redirect,
		untrusted_certificate,
		response_limit,
		header_limit,
		cancel,
		timeout,
		pre_cancel,
		http_401,
		http_403,
		http_403_secondary,
		http_403_secondary_retry,
		http_408,
		http_429,
		http_503_retry,
		http_504,
		graphql_403_rate,
		content_encoding,
		response_metadata,
		duplicate_rate_limit,
		invalid_rate_limit,
		wrong_rate_resource,
		invalid_link,
		graphql_link,
		capability_success,
		capability_redirect,
		capability_403_rate,
		capability_429,
	};

	[[nodiscard]] auto parse_scenario(const std::string_view aValue)
		-> std::optional<fixture_scenario>
	{
		if (aValue == "success") {
			return fixture_scenario::success;
		}
		if (aValue == "proxy-fallback") {
			return fixture_scenario::proxy_fallback;
		}
		if (aValue == "same-redirect") {
			return fixture_scenario::same_redirect;
		}
		if (aValue == "same-query-redirect") {
			return fixture_scenario::same_query_redirect;
		}
		if (aValue == "cross-redirect") {
			return fixture_scenario::cross_redirect;
		}
		if (aValue == "graphql-redirect") {
			return fixture_scenario::graphql_redirect;
		}
		if (aValue == "untrusted-certificate") {
			return fixture_scenario::untrusted_certificate;
		}
		if (aValue == "response-limit") {
			return fixture_scenario::response_limit;
		}
		if (aValue == "header-limit") {
			return fixture_scenario::header_limit;
		}
		if (aValue == "cancel") {
			return fixture_scenario::cancel;
		}
		if (aValue == "timeout") {
			return fixture_scenario::timeout;
		}
		if (aValue == "pre-cancel") {
			return fixture_scenario::pre_cancel;
		}
		if (aValue == "http-401") {
			return fixture_scenario::http_401;
		}
		if (aValue == "http-403") {
			return fixture_scenario::http_403;
		}
		if (aValue == "http-403-secondary") {
			return fixture_scenario::http_403_secondary;
		}
		if (aValue == "http-403-secondary-retry") {
			return fixture_scenario::http_403_secondary_retry;
		}
		if (aValue == "http-408") {
			return fixture_scenario::http_408;
		}
		if (aValue == "http-429") {
			return fixture_scenario::http_429;
		}
		if (aValue == "http-503-retry") {
			return fixture_scenario::http_503_retry;
		}
		if (aValue == "http-504") {
			return fixture_scenario::http_504;
		}
		if (aValue == "graphql-403-rate") {
			return fixture_scenario::graphql_403_rate;
		}
		if (aValue == "content-encoding") {
			return fixture_scenario::content_encoding;
		}
		if (aValue == "response-metadata") {
			return fixture_scenario::response_metadata;
		}
		if (aValue == "duplicate-rate-limit") {
			return fixture_scenario::duplicate_rate_limit;
		}
		if (aValue == "invalid-rate-limit") {
			return fixture_scenario::invalid_rate_limit;
		}
		if (aValue == "wrong-rate-resource") {
			return fixture_scenario::wrong_rate_resource;
		}
		if (aValue == "invalid-link") {
			return fixture_scenario::invalid_link;
		}
		if (aValue == "graphql-link") {
			return fixture_scenario::graphql_link;
		}
		if (aValue == "capability-success") {
			return fixture_scenario::capability_success;
		}
		if (aValue == "capability-redirect") {
			return fixture_scenario::capability_redirect;
		}
		if (aValue == "capability-403-rate") {
			return fixture_scenario::capability_403_rate;
		}
		if (aValue == "capability-429") {
			return fixture_scenario::capability_429;
		}
		return std::nullopt;
	}

	[[nodiscard]] constexpr auto
	scenario_name(const fixture_scenario aScenario) noexcept -> std::string_view
	{
		switch (aScenario) {
		case fixture_scenario::success:
			return "success";
		case fixture_scenario::proxy_fallback:
			return "proxy-fallback";
		case fixture_scenario::same_redirect:
			return "same-redirect";
		case fixture_scenario::same_query_redirect:
			return "same-query-redirect";
		case fixture_scenario::cross_redirect:
			return "cross-redirect";
		case fixture_scenario::graphql_redirect:
			return "graphql-redirect";
		case fixture_scenario::untrusted_certificate:
			return "untrusted-certificate";
		case fixture_scenario::response_limit:
			return "response-limit";
		case fixture_scenario::header_limit:
			return "header-limit";
		case fixture_scenario::cancel:
			return "cancel";
		case fixture_scenario::timeout:
			return "timeout";
		case fixture_scenario::pre_cancel:
			return "pre-cancel";
		case fixture_scenario::http_401:
			return "http-401";
		case fixture_scenario::http_403:
			return "http-403";
		case fixture_scenario::http_403_secondary:
			return "http-403-secondary";
		case fixture_scenario::http_403_secondary_retry:
			return "http-403-secondary-retry";
		case fixture_scenario::http_408:
			return "http-408";
		case fixture_scenario::http_429:
			return "http-429";
		case fixture_scenario::http_503_retry:
			return "http-503-retry";
		case fixture_scenario::http_504:
			return "http-504";
		case fixture_scenario::graphql_403_rate:
			return "graphql-403-rate";
		case fixture_scenario::content_encoding:
			return "content-encoding";
		case fixture_scenario::response_metadata:
			return "response-metadata";
		case fixture_scenario::duplicate_rate_limit:
			return "duplicate-rate-limit";
		case fixture_scenario::invalid_rate_limit:
			return "invalid-rate-limit";
		case fixture_scenario::wrong_rate_resource:
			return "wrong-rate-resource";
		case fixture_scenario::invalid_link:
			return "invalid-link";
		case fixture_scenario::graphql_link:
			return "graphql-link";
		case fixture_scenario::capability_success:
			return "capability-success";
		case fixture_scenario::capability_redirect:
			return "capability-redirect";
		case fixture_scenario::capability_403_rate:
			return "capability-403-rate";
		case fixture_scenario::capability_429:
			return "capability-429";
		}
		return { };
	}

	[[nodiscard]] auto parse_port(
		const std::string_view aValue) noexcept -> std::optional<std::uint16_t>
	{
		std::uint32_t parsed = 0U;
		const auto    result = std::from_chars(
			aValue.data(),
			aValue.data() + aValue.size(),
			parsed);
		if (
			result.ec != std::errc{ }
			|| result.ptr != aValue.data() + aValue.size()
			|| parsed == 0U
			|| parsed > std::numeric_limits<std::uint16_t>::max()) {
			return std::nullopt;
		}
		return static_cast<std::uint16_t>(parsed);
	}

	[[nodiscard]] auto read_frame_length() -> std::uint32_t
	{
		std::array<unsigned char, 4U> bytes{ };
		std::cin.read(
			reinterpret_cast<char*>(bytes.data()),
			static_cast<std::streamsize>(bytes.size()));
		expect(
			std::cin.gcount()
				== static_cast<std::streamsize>(bytes.size()),
			"fixture-frame-length");
		return (static_cast<std::uint32_t>(bytes[0]) << 24U)
			| (static_cast<std::uint32_t>(bytes[1]) << 16U)
			| (static_cast<std::uint32_t>(bytes[2]) << 8U)
			| static_cast<std::uint32_t>(bytes[3]);
	}

	[[nodiscard]] auto read_frame(
		const std::size_t      aMaximum,
		const std::string_view aLimitMessage) -> std::string
	{
		const auto length = read_frame_length();
		expect(
			static_cast<std::size_t>(length) <= aMaximum,
			aLimitMessage);
		std::string result(static_cast<std::size_t>(length), '\0');
		if (!result.empty()) {
			std::cin.read(
				result.data(),
				static_cast<std::streamsize>(result.size()));
			expect(
				std::cin.gcount()
					== static_cast<std::streamsize>(result.size()),
				"fixture-frame-body");
		}
		return result;
	}

	void wipe(std::string& aValue) noexcept
	{
		volatile char* cursor = aValue.data();
		for (std::size_t index = 0U; index < aValue.size(); ++index) {
			cursor[index] = '\0';
		}
		aValue.clear();
	}

	[[nodiscard]] auto fixture_transport(
		const validated_github_host_profile&   aProfile,
		const github_wire_request&             aRequest,
		const std::string_view                 aCredential,
		const native_https_test_configuration& aConfiguration)
		-> native_https_transport_factory_result
	{
		if (
			aRequest.Wire.Operation
			== read_operation::list_review_threads) {
			const auto parent = std::get<core::pull_request_request_context>(
				aRequest.Correlation.Parent);
			return make_native_https_account_transport_for_test(
				aProfile,
				application::github_credential_retrieve_success{
					.Context    = account_operation_context(aProfile, parent),
					.Credential = credential_material(aCredential),
				},
				aConfiguration);
		}

		const auto parent = std::get<core::github_session_request_context>(
			aRequest.Correlation.Parent);
		return make_native_https_bootstrap_transport_for_test(
			aProfile,
			parent,
			credential_material(aCredential),
			aConfiguration);
	}

	[[nodiscard]] auto
	fixture_transport(const validated_github_host_profile&        aProfile,
					  const github_capability_probe_wire_request& aRequest,
					  const std::string_view                      aCredential,
					  const native_https_test_configuration&      aConfiguration)
		-> native_https_transport_factory_result
	{
		return make_native_https_account_transport_for_test(
			aProfile,
			application::github_credential_retrieve_success{
				.Context    = account_operation_context(aProfile, aRequest.Correlation.Session,
														aRequest.Correlation.ObservedAccount),
				.Credential = credential_material(aCredential),
			},
			aConfiguration);
	}

	[[nodiscard]] auto perform_fixture(native_https_transport&    aTransport,
									   const github_wire_request& aRequest,
									   const fixture_scenario     aScenario)
		-> github_wire_completion
	{
		if (aScenario == fixture_scenario::pre_cancel) {
			std::stop_source stop;
			expect(stop.request_stop(), "fixture-pre-cancel-request");
			return aTransport.perform(aRequest, stop.get_token());
		}
		if (aScenario != fixture_scenario::cancel) {
			return aTransport.perform(aRequest, std::stop_token{ });
		}

		std::stop_source                      stop;
		std::optional<github_wire_completion> completion;
		std::exception_ptr                    workerFailure;
		std::jthread                          worker{ [&aTransport, &aRequest, &stop, &completion,
													   &workerFailure] {
			try {
				completion.emplace(aTransport.perform(aRequest, stop.get_token()));
			} catch (...) {
				workerFailure = std::current_exception();
			}
		} };
		std::this_thread::sleep_for(750ms);
		stop.request_stop();
		worker.join();
		expect(workerFailure == nullptr, "fixture-cancel-worker");
		expect(completion.has_value(), "fixture-cancel-completion");
		return std::move(*completion);
	}

	void expect_success(const github_wire_completion&  aCompletion,
						const github_wire_correlation& aCorrelation,
						const fixture_scenario         aScenario)
	{
		const auto* success = std::get_if<github_wire_success>(&aCompletion);
		if (success == nullptr) {
			if (const auto* error = std::get_if<github_wire_error>(&aCompletion);
				error != nullptr) {
				switch (error->Error.Failure) {
				case application::github_provider_failure::authentication:
					throw test_failure{ "fixture-success-authentication" };
				case application::github_provider_failure::permission:
					throw test_failure{ "fixture-success-permission" };
				case application::github_provider_failure::network:
					throw test_failure{ "fixture-success-network" };
				case application::github_provider_failure::api_version:
					throw test_failure{ "fixture-success-api-version" };
				case application::github_provider_failure::unsupported_capability:
					throw test_failure{ "fixture-success-unsupported" };
				case application::github_provider_failure::rate_limit:
					throw test_failure{ "fixture-success-rate-limit" };
				case application::github_provider_failure::certificate:
					throw test_failure{ "fixture-success-certificate" };
				case application::github_provider_failure::redirect:
					throw test_failure{ "fixture-success-redirect" };
				case application::github_provider_failure::timeout:
					throw test_failure{ "fixture-success-timeout" };
				case application::github_provider_failure::process_startup:
					throw test_failure{ "fixture-success-process-startup" };
				case application::github_provider_failure::cli_unavailable:
					throw test_failure{ "fixture-success-cli-unavailable" };
				case application::github_provider_failure::vault:
					throw test_failure{ "fixture-success-vault" };
				case application::github_provider_failure::malformed_response:
					throw test_failure{ "fixture-success-malformed" };
				case application::github_provider_failure::response_limit:
					throw test_failure{ "fixture-success-response-limit" };
				case application::github_provider_failure::unavailable:
					throw test_failure{ "fixture-success-unavailable" };
				}
			}
			throw test_failure{ "fixture-success-cancelled" };
		}
		expect(
			success->Correlation == aCorrelation,
			"fixture-success-correlation");
		expect(
			success->Response.Status == 200,
			"fixture-success-status");
		const auto metadataHeaders = std::array<http_header, 7U>{
			http_header{
				.Name  = "Content-Type",
				.Value = "Application/JSON; Charset=UTF-8",
			},
			http_header{
				.Name  = "Link",
				.Value = "<https://ghe.bpr.invalid/api/v3/user?page=2>; rel=\"next\"",
			},
			http_header{
				.Name  = "X-RateLimit-Limit",
				.Value = "60",
			},
			http_header{
				.Name  = "X-RateLimit-Remaining",
				.Value = "42",
			},
			http_header{
				.Name  = "X-RateLimit-Reset",
				.Value = "1999999999",
			},
			http_header{
				.Name  = "X-RateLimit-Resource",
				.Value = "core",
			},
			http_header{
				.Name  = "Retry-After",
				.Value = "7",
			},
		};
		const auto standardHeaders = std::array<http_header, 1U>{
			http_header{
				.Name  = "Content-Type",
				.Value = "application/json",
			},
		};
		const auto headersMatch = aScenario == fixture_scenario::response_metadata
			? std::ranges::equal(success->Response.Headers, metadataHeaders)
			: std::ranges::equal(success->Response.Headers, standardHeaders);
		expect(headersMatch, "fixture-success-headers");
		expect(success->Response.Body == R"({"id":7,"login":"fixture-user"})",
			   "fixture-success-body");
		expect(success->Response.Completeness
				   == (aScenario == fixture_scenario::response_metadata
						   ? page_completeness::has_next_page
						   : page_completeness::complete),
			   "fixture-success-completeness");
	}

	void expect_error(const github_wire_completion&              aCompletion,
					  const github_wire_correlation&             aCorrelation,
					  const application::github_provider_failure aFailure,
					  const std::optional<std::uint32_t>         aRetryAfter = std::nullopt,
					  const std::optional<application::github_rate_limit_resource>
								 aObservedResource = std::nullopt,
					  const bool aExpectRateCause  = false,
					  const bool aSecondary        = false)
	{
		const auto* error = std::get_if<github_wire_error>(&aCompletion);
		expect(error != nullptr && error->Correlation == aCorrelation
				   && error->Error.Failure == aFailure
				   && error->Error.RetryAfterSeconds == aRetryAfter
				   && application::valid(error->Error),
			   "fixture-error-completion");
		if (!aObservedResource.has_value()) {
			return;
		}
		const auto* observation = application::find(error->Error.RateLimits, *aObservedResource);
		expect(
			observation != nullptr
				&& error->Error.RateLimitCause.has_value() == aExpectRateCause
				&& (!aExpectRateCause
					|| (error->Error.RateLimitCause->Resource == *aObservedResource
						&& error->Error.RateLimitCause->SecondaryLimited
							== aSecondary
						&& *observation == *error->Error.RateLimitCause)),
			"fixture-error-rate-resource");
	}

	void expect_fixture_completion(const github_wire_completion&  aCompletion,
								   const github_wire_correlation& aCorrelation,
								   const fixture_scenario         aScenario)
	{
		switch (aScenario) {
		case fixture_scenario::success:
		case fixture_scenario::proxy_fallback:
		case fixture_scenario::same_redirect:
		case fixture_scenario::response_metadata:
			expect_success(aCompletion, aCorrelation, aScenario);
			return;
		case fixture_scenario::cross_redirect:
		case fixture_scenario::graphql_redirect:
		case fixture_scenario::same_query_redirect:
			expect_error(aCompletion, aCorrelation,
						 application::github_provider_failure::redirect);
			return;
		case fixture_scenario::untrusted_certificate:
			expect_error(aCompletion, aCorrelation,
						 application::github_provider_failure::certificate);
			return;
		case fixture_scenario::response_limit:
		case fixture_scenario::header_limit:
			expect_error(aCompletion, aCorrelation,
						 application::github_provider_failure::response_limit);
			return;
		case fixture_scenario::cancel:
		case fixture_scenario::pre_cancel: {
			const auto* cancelled = std::get_if<github_wire_cancelled>(&aCompletion);
			expect(cancelled != nullptr && cancelled->Correlation == aCorrelation,
				   "fixture-cancelled-completion");
			return;
		}
		case fixture_scenario::timeout:
			expect_error(aCompletion, aCorrelation,
						 application::github_provider_failure::timeout);
			return;
		case fixture_scenario::http_401:
			expect_error(aCompletion, aCorrelation,
						 application::github_provider_failure::authentication);
			return;
		case fixture_scenario::http_403:
			expect_error(aCompletion, aCorrelation,
						 application::github_provider_failure::permission);
			return;
		case fixture_scenario::http_403_secondary:
			expect_error(aCompletion, aCorrelation,
						 application::github_provider_failure::rate_limit, 60U,
						 application::github_rate_limit_resource::core, true, true);
			return;
		case fixture_scenario::http_403_secondary_retry:
			expect_error(aCompletion, aCorrelation,
						 application::github_provider_failure::rate_limit, 17U,
						 application::github_rate_limit_resource::core, true, true);
			return;
		case fixture_scenario::http_408:
			expect_error(aCompletion, aCorrelation,
						 application::github_provider_failure::timeout);
			return;
		case fixture_scenario::http_429:
			expect_error(aCompletion, aCorrelation,
						 application::github_provider_failure::rate_limit, 7U,
						 application::github_rate_limit_resource::core, true, true);
			return;
		case fixture_scenario::http_503_retry:
			expect_error(aCompletion, aCorrelation,
						 application::github_provider_failure::unavailable, 9U,
						 application::github_rate_limit_resource::core);
			return;
		case fixture_scenario::http_504:
			expect_error(aCompletion, aCorrelation,
						 application::github_provider_failure::timeout);
			return;
		case fixture_scenario::graphql_403_rate:
			expect_error(aCompletion, aCorrelation,
						 application::github_provider_failure::rate_limit, 60U,
						 application::github_rate_limit_resource::graphql, true,
						 true);
			return;
		case fixture_scenario::content_encoding:
		case fixture_scenario::duplicate_rate_limit:
		case fixture_scenario::invalid_rate_limit:
		case fixture_scenario::wrong_rate_resource:
		case fixture_scenario::invalid_link:
		case fixture_scenario::graphql_link:
			expect_error(aCompletion, aCorrelation,
						 application::github_provider_failure::malformed_response);
			return;
		case fixture_scenario::capability_success:
		case fixture_scenario::capability_redirect:
		case fixture_scenario::capability_403_rate:
		case fixture_scenario::capability_429:
			throw test_failure{ "capability-scenario-read-completion" };
		}
		throw test_failure{ "fixture-scenario" };
	}

	void expect_capability_fixture_completion(
		const github_capability_probe_wire_completion&  aCompletion,
		const github_capability_probe_wire_correlation& aCorrelation,
		const fixture_scenario                          aScenario)
	{
		if (aScenario == fixture_scenario::capability_redirect) {
			expect_capability_error(aCompletion, aCorrelation,
									application::github_provider_failure::redirect,
									std::nullopt, "capability-fixture-redirect");
			return;
		}
		if (aScenario == fixture_scenario::capability_403_rate
			|| aScenario == fixture_scenario::capability_429) {
			const auto* failure = std::get_if<github_capability_probe_wire_error>(&aCompletion);
			expect(failure != nullptr && failure->Correlation == aCorrelation
					   && failure->Error.Failure
						   == application::github_provider_failure::rate_limit
					   && failure->Error.RetryAfterSeconds
						   == std::optional<std::uint32_t>{ 60U }
					   && failure->Error.RateLimitCause.has_value()
					   && failure->Error.RateLimitCause->Resource
						   == application::github_rate_limit_resource::graphql
					   && failure->Error.RateLimitCause->SecondaryLimited
					   && failure->Error.RateLimits.Graphql
						   == failure->Error.RateLimitCause
					   && application::valid(failure->Error),
				   "capability-fixture-rate-limit");
			return;
		}

		const auto* success                  = std::get_if<github_capability_probe_wire_success>(&aCompletion);
		const auto  expectedRateLimitHeaders = std::array<capability_probe_header, 5U>{
			capability_probe_header{
				.Name  = "X-RateLimit-Limit",
				.Value = "60",
			},
			capability_probe_header{
				.Name  = "X-RateLimit-Remaining",
				.Value = "42",
			},
			capability_probe_header{
				.Name  = "X-RateLimit-Reset",
				.Value = "1999999999",
			},
			capability_probe_header{
				.Name  = "X-RateLimit-Resource",
				.Value = "graphql",
			},
			capability_probe_header{
				.Name  = "Retry-After",
				.Value = "7",
			},
		};
		expect(
			success != nullptr && success->Correlation == aCorrelation && success->Response.Status == 200 && success->Response.Authenticated && success->Response.ContentType == "Application/JSON; Charset=UTF-8" && success->Response.Body == "{}" && success->Response.RateLimitHeaderCount == 5U && success->Response.RateLimitHeaders == expectedRateLimitHeaders,
			"capability-fixture-success");
	}

	void run_fixture(const fixture_scenario aScenario,
					 const std::uint16_t    aProxyPort,
					 const std::uint16_t    aFallbackProxyPort)
	{
		auto framedCredential     = read_frame(application::maximum_github_credential_bytes,
											   "fixture-credential-limit");
		auto authorityCertificate = read_frame(
			native_https_maximum_response_header_bytes, "fixture-authority-limit");
		expect(!framedCredential.empty(), "fixture-credential-empty");
		expect(!authorityCertificate.empty(), "fixture-authority-empty");

		const auto profile = validate_github_host_profile(profile_candidate(
			application::github_authentication_transport::native_https));
		expect(profile.has_value(), "fixture-profile");

		auto configuration = native_https_test_configuration{
			.ProxyHost               = "127.0.0.1",
			.ProxyPort               = aProxyPort,
			.FallbackProxyPort       = aFallbackProxyPort,
			.TrustedCaCertificatePem = aScenario == fixture_scenario::untrusted_certificate
				? std::string{ }
				: std::move(authorityCertificate),
			.TotalTimeout            = aScenario == fixture_scenario::timeout ? 1s : 5s,
			.ResponseBodyLimit       = aScenario == fixture_scenario::response_limit
				? 1'024U
				: native_https_maximum_response_body_bytes,
		};

		if (aScenario == fixture_scenario::capability_success
			|| aScenario == fixture_scenario::capability_redirect
			|| aScenario == fixture_scenario::capability_403_rate
			|| aScenario == fixture_scenario::capability_429) {
			const auto request = capability_request_for(
				*profile, session_context(*profile), core::github_account_id{ 203 });
			auto transport = fixture_transport(*profile, request, framedCredential,
											   configuration);
			wipe(framedCredential);
			expect(transport.has_value(), "capability-fixture-transport");
			const auto completion = (*transport)->perform_capability_probe(request, std::stop_token{ });
			expect_capability_fixture_completion(completion, request.Correlation,
												 aScenario);
			return;
		}

		const auto operation = aScenario == fixture_scenario::graphql_redirect
				|| aScenario == fixture_scenario::graphql_link
				|| aScenario == fixture_scenario::graphql_403_rate
			? read_operation::list_review_threads
			: read_operation::get_viewer_identity;
		const auto request   = wire_request_for(*profile, operation);
		auto       transport = fixture_transport(*profile, request, framedCredential, configuration);
		wipe(framedCredential);
		expect(transport.has_value(), "fixture-transport");

		const auto started    = std::chrono::steady_clock::now();
		const auto completion = perform_fixture(**transport, request, aScenario);
		const auto elapsed    = std::chrono::steady_clock::now() - started;
		if (aScenario == fixture_scenario::timeout) {
			expect(elapsed >= 500ms && elapsed < 4s, "fixture-timeout-window");
		}
		expect_fixture_completion(completion, request.Correlation, aScenario);
		if (aScenario != fixture_scenario::graphql_redirect
			&& aScenario != fixture_scenario::graphql_link
			&& aScenario != fixture_scenario::graphql_403_rate
			&& aScenario != fixture_scenario::pre_cancel) {
			const auto replay = (*transport)->perform(request, std::stop_token{ });
			expect_unavailable(replay, request.Correlation,
							   "fixture-bootstrap-one-shot");
		}
	}
} // namespace

auto main(int argument_count, char* arguments[]) -> int
{
	try {
		select_binary_standard_streams();
		qInstallMessageHandler(discard_qt_message);
		QCoreApplication application{ argument_count, arguments };
		static_cast<void>(application);

		if (
			argument_count == 2
			&& std::string_view{ arguments[1] } == "--contract") {
			test_factory_contracts_and_preflight();
			std::cout << "native-https-transport:contract:pass\n";
			return EXIT_SUCCESS;
		}
		if (
			argument_count == 2
			&& std::string_view{ arguments[1] } == "--runtime") {
			print_runtime_facts();
			return EXIT_SUCCESS;
		}

		expect(
			argument_count == 5 || argument_count == 7,
			"arguments");
		expect(
			std::string_view{ arguments[1] } == "--fixture"
				&& std::string_view{ arguments[3] } == "--proxy-port",
			"fixture-arguments");
		const auto scenario          = parse_scenario(std::string_view{ arguments[2] });
		const auto proxyPort         = parse_port(std::string_view{ arguments[4] });
		const auto fallbackProxyPort = argument_count == 7
			? parse_port(std::string_view{ arguments[6] })
			: std::optional<std::uint16_t>{ };
		expect(
			scenario.has_value() && proxyPort.has_value(),
			"fixture-selection");
		expect(
			(
				*scenario == fixture_scenario::proxy_fallback
				&& argument_count == 7
				&& std::string_view{ arguments[5] }
					== "--fallback-proxy-port"
				&& fallbackProxyPort.has_value())
				|| (*scenario != fixture_scenario::proxy_fallback
					&& argument_count == 5),
			"fixture-proxy-selection");
		run_fixture(
			*scenario,
			*proxyPort,
			fallbackProxyPort.value_or(0U));
		std::cout
			<< "native-https-transport:"
			<< scenario_name(*scenario)
			<< ":pass\n";
		return EXIT_SUCCESS;
	} catch (const test_failure& failure) {
		std::cerr
			<< "native-https-transport:fail:"
			<< failure.message()
			<< '\n';
		return EXIT_FAILURE;
	} catch (...) {
		std::cerr << "native-https-transport:fail:internal\n";
		return EXIT_FAILURE;
	}
}
