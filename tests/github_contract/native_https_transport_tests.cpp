#include "github_mutation_contract.hpp"
#include "github_wire_request_validation.hpp"
#include "native_https_transport.hpp"

#include <QCoreApplication>
#include <QMessageLogContext>
#include <QString>
#include <QtLogging>

#include <algorithm>
#include <array>
#include <atomic>
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
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
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

	class test_failure final : public std::runtime_error
	{
	public:
		explicit test_failure(const std::string_view aMessage)
			: std::runtime_error{ std::string{ aMessage } }
		{
		}
	};

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			throw test_failure{ aMessage };
		}
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes.fill(aSeed);
		return Identity{ bytes };
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

	[[nodiscard]] auto github_com_profile_candidate()
		-> github_host_profile_candidate
	{
		auto candidate = profile_candidate(
			application::github_authentication_transport::native_https);
		candidate.Server = application::github_server_contract::
			github_com_2022_11_28;
		candidate.Hostname    = "github.com";
		candidate.RestRoot    = "https://api.github.com";
		candidate.GraphqlRoot = "https://api.github.com/graphql";
		return candidate;
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
						   const core::github_account_id aObservedAccount, const std::uint8_t aPart = 0U)
		-> github_capability_probe_wire_request
	{
		const auto wire = compile_capability_probe_request(aProfile, { }, aPart);
		expect(wire.has_value(), "canonical-capability-request");
		return {
			.Correlation = github_capability_probe_wire_correlation{
				.Part            = aPart,
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

	/// \brief Derives the session an anonymous read correlates against.
	///
	/// \details An anonymous binding compares the parent's application session
	/// and identity generation, never its account, so the session it is handed
	/// has to be the one the pull-request parent carries rather than the
	/// unrelated one the bootstrap courses use.
	[[nodiscard]] auto anonymous_session_context(
		const validated_github_host_profile& aProfile)
		-> core::github_session_request_context
	{
		const auto parent = pull_request_context(aProfile);
		return {
			.Request            = parent.Request,
			.ApplicationSession = parent.ApplicationSession,
			.Profile            = parent.Profile,
			.IdentityGeneration = parent.IdentityGeneration,
		};
	}

	/// \brief Builds a GitHub.com read the anonymous authority may admit.
	///
	/// \details The anonymous courses need a different authority from the rest
	/// of this file: the anonymous factory admits only `github.com`, so the
	/// semantic, the correlation, and the fixture host all move together.
	[[nodiscard]] auto anonymous_wire_request_for(
		const validated_github_host_profile& aProfile,
		const read_operation                 aOperation) -> github_wire_request
	{
		auto semantic   = semantic_request(aOperation);
		semantic.Server = server_contract::github_com;
		const auto wire = compile_read_request(semantic);
		expect(wire.has_value(), "anonymous-canonical-wire-request");
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

	// The pull request every mutation course writes to. The loopback fixture
	// answers with these identities, so the two files change together.
	[[nodiscard]] auto mutation_target_for(
		const validated_github_host_profile& aProfile)
		-> application::review_mutation_target_binding
	{
		const auto context = pull_request_context(aProfile);
		return {
			.Semantic = {
				.Host         = context.Host,
				.Repository   = *context.ExpectedRepository,
				.PullRequest  = *context.ExpectedPullRequest,
				.ExpectedHead = *core::parse_git_commit_oid(
					"1111111111111111111111111111111111111111"),
			},
			.Hostname                 = application::github_canonical_hostname{ aProfile.hostname() },
			.ServerContract           = aProfile.server_contract(),
			.Repository               = context.Repository,
			.Number                   = context.Number,
			.RepositoryDatabaseId     = 1'328'277'631U,
			.RepositoryNodeId         = "R_kgDOTyvofw",
			.PullRequestDatabaseId    = 4'246'446'933U,
			.PullRequestNodeId        = "PR_kwDOTyvof879G6NV",
			.BaseRepositoryDatabaseId = 1'328'277'631U,
			.BaseOid                  = *core::parse_git_commit_oid("2222222222222222222222222222222222222222"),
			.BaseRef                  = "main",
			.HeadRef                  = "feature/review",
			.Open                     = true,
		};
	}

	[[nodiscard]] auto mutation_identity_for(
		const validated_github_host_profile& aProfile)
		-> application::review_mutation_acting_identity
	{
		return {
			.Account    = pull_request_context(aProfile).Account,
			.DatabaseId = 22U,
			.NodeId     = "MDQ6VXNlcjIy",
			.Login      = "fixture-reviewer",
		};
	}

	/// \brief A new root comment, or with `aThread` a reply under that thread.
	[[nodiscard]] auto mutation_semantic_for(
		const validated_github_host_profile& aProfile,
		std::optional<std::string>           aThread = std::nullopt)
		-> application::review_mutation_request
	{
		const auto target   = mutation_target_for(aProfile);
		const auto identity = mutation_identity_for(aProfile);
		const auto proposal = core::review_publication_proposal{
			.Draft  = core::review_draft_id{ 31U },
			.Anchor = {
				.Comparison = opaque<core::file_comparison_digest>(0x22U),
				.Side       = core::diff_side::right,
				.Line       = 12U,
			},
			.Body          = "Please rename this.",
			.Disposition   = core::review_disposition::comment,
			.ReplyToThread = std::move(aThread),
		};
		return {
			.Proposal       = proposal,
			.Target         = target.Semantic,
			.ActingIdentity = identity.Account,
			.IdempotencyKey = application::idempotency_key_for(
				proposal, target.Semantic),
			.LiveBinding = application::review_mutation_live_binding{
				.Target         = target,
				.ActingIdentity = identity,
				.Anchor         = {
					.Semantic = proposal.Anchor,
					.Path     = "fixture/review.txt",
					.Commit   = target.Semantic.ExpectedHead,
				},
			},
		};
	}

	[[nodiscard]] auto mutation_request_for(
		const validated_github_host_profile& aProfile)
		-> github_mutation_wire_request
	{
		const auto wire = compile_review_comment_request(
			mutation_semantic_for(aProfile), aProfile.server_contract());
		expect(wire.has_value(), "fixture-mutation-wire");
		return {
			.Context = account_operation_context(aProfile, pull_request_context(aProfile)),
			.Wire    = *wire,
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

	/// \brief Proves the credential-free state can only be asked for by name.
	///
	/// \details RFC-BPR-0017's gate is not "can a request go out without a
	/// header" — it is "can that only happen deliberately". So this checks both
	/// directions. The credential-free factory refuses the same malformed inputs
	/// every other factory refuses, and the credentialed factories still reject
	/// empty, malformed, and wiped material with their own typed failures rather
	/// than degrading into the credential-free state.
	void test_credential_free_is_deliberate(
		const validated_github_host_profile&   aNative,
		const validated_github_host_profile&   aCli,
		const native_https_test_configuration& aConfiguration)
	{
		const auto credentialFree = make_native_https_credential_free_transport_for_test(
			aNative, session_context(aNative), aConfiguration);
		expect(credentialFree.has_value(), "credential-free-factory");

		auto foreignContext    = session_context(aNative);
		foreignContext.Profile = core::host_profile_id{ 99 };
		expect(
			make_native_https_credential_free_transport_for_test(
				aNative, foreignContext, aConfiguration)
					.error()
				== native_https_configuration_failure::invalid_bootstrap_context,
			"credential-free-profile-drift");

		expect(
			make_native_https_credential_free_transport_for_test(
				aCli, session_context(aCli), aConfiguration)
					.error()
				== native_https_configuration_failure::profile_transport_mismatch,
			"credential-free-transport-mismatch");

		expect(
			make_native_https_credential_free_transport_for_test(
				aNative, session_context(aNative), native_https_test_configuration{ })
					.error()
				== native_https_configuration_failure::invalid_test_configuration,
			"credential-free-test-configuration");

		// The cut wire. Every way an authenticated caller can arrive without
		// usable material must produce its own typed refusal. None of them may
		// hand back a working transport, which is what a silent fallback into
		// the credential-free state would look like from here.
		auto emptyMaterial = credential_material("fixture-token");
		emptyMaterial.reset();
		expect(
			make_native_https_bootstrap_transport_for_test(
				aNative, session_context(aNative), std::move(emptyMaterial),
				aConfiguration)
					.error()
				== native_https_configuration_failure::invalid_credential_material,
			"credential-free-no-empty-bootstrap-fallback");
		expect(
			make_native_https_bootstrap_transport_for_test(
				aNative, session_context(aNative), credential_material("not a token"),
				aConfiguration)
					.error()
				== native_https_configuration_failure::invalid_credential_material,
			"credential-free-no-malformed-bootstrap-fallback");

		// The account factory is the other way in, and it must refuse the same
		// way rather than leaving the caller with a transport that happens to
		// send nothing.
		auto wipedAccountMaterial = credential_material("fixture-token");
		wipedAccountMaterial.reset();
		expect(
			make_native_https_account_transport_for_test(
				aNative,
				application::github_credential_retrieve_success{
					.Context = account_operation_context(
						aNative, pull_request_context(aNative)),
					.Credential = std::move(wipedAccountMaterial),
				},
				aConfiguration)
					.error()
				== native_https_configuration_failure::invalid_account_credential,
			"credential-free-no-wiped-account-fallback");
		expect(
			make_native_https_account_transport_for_test(
				aNative,
				application::github_credential_retrieve_success{
					.Context = account_operation_context(
						aNative, pull_request_context(aNative)),
					.Credential = credential_material("not a token"),
				},
				aConfiguration)
					.error()
				== native_https_configuration_failure::invalid_credential_material,
			"credential-free-no-malformed-account-fallback");
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
		for (const auto provenance : { core::application_session_id{ }, core::application_session_id{ 999U } }) {
			auto forged                              = repository;
			forged.Correlation.AuthenticationSession = provenance;
			expect_unavailable((*account)->perform(forged, { }), forged.Correlation, "invalid-authentication-provenance-preflight");
		}

		const auto mutationTarget = application::review_mutation_target_binding{
			.Semantic = {
				.Host         = pullRequest.Host,
				.Repository   = core::repository_id{ 204U },
				.PullRequest  = core::pull_request_id{ 205U },
				.ExpectedHead = *core::parse_git_commit_oid(
					"1111111111111111111111111111111111111111"),
			},
			.Hostname                 = application::github_canonical_hostname{ native->hostname() },
			.ServerContract           = native->server_contract(),
			.Repository               = pullRequest.Repository,
			.Number                   = pullRequest.Number,
			.RepositoryDatabaseId     = 204U,
			.RepositoryNodeId         = "R_fixture_204",
			.PullRequestDatabaseId    = 205U,
			.PullRequestNodeId        = "PR_fixture_205",
			.BaseRepositoryDatabaseId = 204U,
			.BaseOid                  = *core::parse_git_commit_oid("2222222222222222222222222222222222222222"),
			.BaseRef                  = "main",
			.HeadRef                  = "feature/review",
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
			mutationRequest, mutationStopped.get_token(), [] { return true; });
		const auto* mutationCancellation = std::get_if<github_mutation_wire_cancelled>(&mutationCancelled);
		expect(mutationCancellation != nullptr
				   && mutationCancellation->Transmission
					   == github_mutation_transmission::definitely_not_sent,
			   "mutation-pre-cancelled-without-network");

		auto invalidMutation = mutationRequest;
		invalidMutation.Wire.Path += "/tampered";
		const auto invalidMutationCompletion = (*account)->perform_review_mutation(
			invalidMutation, std::stop_token{ }, [] { return true; });
		const auto* invalidMutationError = std::get_if<github_mutation_wire_error>(
			&invalidMutationCompletion);
		expect(invalidMutationError != nullptr
				   && invalidMutationError->Failure
					   == application::review_mutation_failure::unavailable
				   && invalidMutationError->Transmission
					   == github_mutation_transmission::definitely_not_sent,
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

		// [[BPR-0988]]. A repository-scoped discovery search binds the same
		// exact account: another account or host is refused before any
		// connection, exactly as a pull request read's drift is.
		{
			const auto semantic = semantic_read_request{
				.Server            = repository.Wire.Server,
				.Operation         = read_operation::search_personal_pull_requests,
				.Owner             = accountParent.Repository.Owner,
				.Repository        = accountParent.Repository.Name,
				.PullRequestNumber = 0U,
				.HeadOid           = { },
				.SearchTerms       = "author:octo-reviewer",
				.Page              = 1U,
				.GraphqlPage       = std::nullopt,
			};
			const auto searchWire = compile_read_request(semantic);
			expect(searchWire.has_value(), "discovery-wire-contract");
			const auto discoveryParent = core::repository_request_context{
				.Request            = accountParent.Request,
				.ApplicationSession = accountParent.ApplicationSession,
				.Profile            = accountParent.Profile,
				.Host               = accountParent.Host,
				.Account            = accountParent.Account,
				.Repository         = accountParent.Repository,
				.Capabilities       = accountParent.Capabilities,
				.IdentityGeneration = accountParent.IdentityGeneration,
			};
			auto search = github_wire_request{
				.Correlation = repository.Correlation,
				.Wire        = *searchWire,
			};
			search.Correlation.Operation = read_operation::search_personal_pull_requests;
			search.Correlation.Parent    = discoveryParent;
			expect(validate_github_wire_request(*native, search).has_value(), "discovery-wire-validation");
			for (const auto drift : { 0U, 1U }) {
				auto forged       = search;
				auto forgedParent = discoveryParent;
				if (drift == 0U) {
					forgedParent.Account = core::github_account_id{ 999 };
				} else {
					forgedParent.Host = core::git_host_id{ 999 };
				}
				forged.Correlation.Parent = forgedParent;
				expect_unavailable((*account)->perform(forged, std::stop_token{ }), forged.Correlation, "discovery-authority-drift-preflight");
			}
		}

		const auto observedAccount = core::github_account_id{ 203 };
		const auto session         = session_context(*native);
		const auto capability      = capability_request_for(*native, session, observedAccount);
		expect(validate_github_capability_probe_wire_request(*native, capability)
				   .has_value(),
			   "capability-wire-validation");

		for (const auto drift : { 0U, 1U, 2U }) {
			auto wrongPart = capability;
			if (drift == 0U) {
				wrongPart.Correlation.Part = 1U;
			}
			if (drift == 1U) {
				wrongPart.Wire.Part = 1U;
			}
			if (drift == 2U) {
				wrongPart.Correlation.Part = 9U;
				wrongPart.Wire.Part        = 9U;
			}
			expect(!validate_github_capability_probe_wire_request(*native, wrongPart), "noncanonical indexed part refused by pure wire validator");
			expect_capability_error((*account)->perform_capability_probe(wrongPart, std::stop_token{ }), wrongPart.Correlation, application::github_provider_failure::unavailable, std::nullopt, "capability-part-drift-preflight");
		}

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

		test_credential_free_is_deliberate(*native, *cli, configuration);
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
		mutation_witness,
		mutation_witness_failure,
		mutation_proxy_no_fallback,
		mutation_graphql_course,
		mutation_review_submission,
		mutation_graphql_witness_failure,
		mutation_status_refusal,
		mutation_graphql_core_resource,
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
		http_404,
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
		capability_metadata,
		capability_redirect,
		capability_403_rate,
		capability_429,
		capability_refusal_encoding,
		capability_refusal_duplicate,
		capability_refusal_invalid,
		capability_refusal_missing_rate,
		capability_refusal_partial_rate,
		capability_refusal_wrong_resource,
		capability_refusal_content_type,
		capability_refusal_header_bound,
		capability_refusal_body_bound,

		credential_free,
		anonymous_read,
		anonymous_read_canary,
	};

	[[nodiscard]] auto is_capability_refusal(const fixture_scenario aScenario) -> bool
	{
		return aScenario >= fixture_scenario::capability_refusal_encoding
			&& aScenario <= fixture_scenario::capability_refusal_body_bound;
	}

	[[nodiscard]] auto parse_scenario(const std::string_view aValue)
		-> std::optional<fixture_scenario>
	{
		if (aValue == "success") {
			return fixture_scenario::success;
		}
		if (aValue == "credential-free") {
			return fixture_scenario::credential_free;
		}
		if (aValue == "anonymous-read") {
			return fixture_scenario::anonymous_read;
		}
		if (aValue == "anonymous-read-canary") {
			return fixture_scenario::anonymous_read_canary;
		}
		if (aValue == "mutation-witness") {
			return fixture_scenario::mutation_witness;
		}
		if (aValue == "mutation-witness-failure") {
			return fixture_scenario::mutation_witness_failure;
		}
		if (aValue == "mutation-proxy-no-fallback") {
			return fixture_scenario::mutation_proxy_no_fallback;
		}
		if (aValue == "mutation-graphql-course") {
			return fixture_scenario::mutation_graphql_course;
		}
		if (aValue == "mutation-review-submission") {
			return fixture_scenario::mutation_review_submission;
		}
		if (aValue == "mutation-graphql-witness-failure") {
			return fixture_scenario::mutation_graphql_witness_failure;
		}
		if (aValue == "mutation-status-refusal") {
			return fixture_scenario::mutation_status_refusal;
		}
		if (aValue == "mutation-graphql-core-resource") {
			return fixture_scenario::mutation_graphql_core_resource;
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
		if (aValue == "http-404") {
			return fixture_scenario::http_404;
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
		if (aValue == "capability-metadata") {
			return fixture_scenario::capability_metadata;
		}
		if (aValue == "capability-refusal-encoding") {
			return fixture_scenario::capability_refusal_encoding;
		}
		if (aValue == "capability-refusal-duplicate") {
			return fixture_scenario::capability_refusal_duplicate;
		}
		if (aValue == "capability-refusal-invalid") {
			return fixture_scenario::capability_refusal_invalid;
		}
		if (aValue == "capability-refusal-missing-rate") {
			return fixture_scenario::capability_refusal_missing_rate;
		}
		if (aValue == "capability-refusal-partial-rate") {
			return fixture_scenario::capability_refusal_partial_rate;
		}
		if (aValue == "capability-refusal-wrong-resource") {
			return fixture_scenario::capability_refusal_wrong_resource;
		}
		if (aValue == "capability-refusal-content-type") {
			return fixture_scenario::capability_refusal_content_type;
		}
		if (aValue == "capability-refusal-header-bound") {
			return fixture_scenario::capability_refusal_header_bound;
		}
		if (aValue == "capability-refusal-body-bound") {
			return fixture_scenario::capability_refusal_body_bound;
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
		case fixture_scenario::credential_free:
			return "credential-free";
		case fixture_scenario::anonymous_read:
			return "anonymous-read";
		case fixture_scenario::anonymous_read_canary:
			return "anonymous-read-canary";
		case fixture_scenario::mutation_witness:
			return "mutation-witness";
		case fixture_scenario::mutation_witness_failure:
			return "mutation-witness-failure";
		case fixture_scenario::mutation_proxy_no_fallback:
			return "mutation-proxy-no-fallback";
		case fixture_scenario::mutation_graphql_course:
			return "mutation-graphql-course";
		case fixture_scenario::mutation_review_submission:
			return "mutation-review-submission";
		case fixture_scenario::mutation_graphql_witness_failure:
			return "mutation-graphql-witness-failure";
		case fixture_scenario::mutation_status_refusal:
			return "mutation-status-refusal";
		case fixture_scenario::mutation_graphql_core_resource:
			return "mutation-graphql-core-resource";
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
		case fixture_scenario::http_404:
			return "http-404";
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
		case fixture_scenario::capability_metadata:
			return "capability-metadata";
		case fixture_scenario::capability_refusal_encoding:
			return "capability-refusal-encoding";
		case fixture_scenario::capability_refusal_duplicate:
			return "capability-refusal-duplicate";
		case fixture_scenario::capability_refusal_invalid:
			return "capability-refusal-invalid";
		case fixture_scenario::capability_refusal_missing_rate:
			return "capability-refusal-missing-rate";
		case fixture_scenario::capability_refusal_partial_rate:
			return "capability-refusal-partial-rate";
		case fixture_scenario::capability_refusal_wrong_resource:
			return "capability-refusal-wrong-resource";
		case fixture_scenario::capability_refusal_content_type:
			return "capability-refusal-content-type";
		case fixture_scenario::capability_refusal_header_bound:
			return "capability-refusal-header-bound";
		case fixture_scenario::capability_refusal_body_bound:
			return "capability-refusal-body-bound";
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
		case fixture_scenario::credential_free:
		// The anonymous courses assert their own completions before reaching
		// here, so listing them keeps the switch exhaustive without claiming a
		// shared expectation they do not use.
		case fixture_scenario::anonymous_read:
		case fixture_scenario::anonymous_read_canary:
		case fixture_scenario::mutation_witness:
		case fixture_scenario::mutation_witness_failure:
		case fixture_scenario::mutation_proxy_no_fallback:
		case fixture_scenario::mutation_graphql_course:
		case fixture_scenario::mutation_review_submission:
		case fixture_scenario::mutation_graphql_witness_failure:
		case fixture_scenario::mutation_status_refusal:
		case fixture_scenario::mutation_graphql_core_resource:
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
			expect(std::get<github_wire_error>(aCompletion).Error.HttpStatus == 0U
					   && std::get<github_wire_error>(aCompletion).Error.TransportFailureSource == application::diagnostic_transport_failure_source::native_transport,
				   "local-tls-refusal-does-not-fabricate-http-status");
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
		case fixture_scenario::http_404: {
			expect_error(aCompletion, aCorrelation,
						 application::github_provider_failure::permission);
			const auto& error = std::get<github_wire_error>(aCompletion).Error;
			expect(error.HttpStatus == (aScenario == fixture_scenario::http_403 ? 403U : 404U)
					   && error.TransportFailureSource == application::diagnostic_transport_failure_source::http_response,
				   "permission-retains-exact-http-status-and-source");
			return;
		}
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
		case fixture_scenario::capability_refusal_encoding:
		case fixture_scenario::capability_refusal_duplicate:
		case fixture_scenario::capability_refusal_invalid:
		case fixture_scenario::capability_refusal_missing_rate:
		case fixture_scenario::capability_refusal_partial_rate:
		case fixture_scenario::capability_refusal_wrong_resource:
		case fixture_scenario::capability_refusal_content_type:
		case fixture_scenario::capability_refusal_header_bound:
		case fixture_scenario::capability_refusal_body_bound:
		case fixture_scenario::capability_success:
		case fixture_scenario::capability_metadata:
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
		if (is_capability_refusal(aScenario)) {
			auto expected = application::github_capability_diagnostic::unavailable_detail;
			if (aScenario == fixture_scenario::capability_refusal_encoding) {
				expected = application::github_capability_diagnostic::transport_content_encoding;
			}
			if (aScenario == fixture_scenario::capability_refusal_duplicate) {
				// Qt combines repeated numeric fields before exposing rawHeaderPairs.
				// Diagnose the invalid singleton actually observed, not its provenance.
				expected = application::github_capability_diagnostic::transport_invalid_metadata;
			}
			if (aScenario == fixture_scenario::capability_refusal_invalid) {
				expected = application::github_capability_diagnostic::transport_invalid_metadata;
			}
			if (aScenario == fixture_scenario::capability_refusal_missing_rate) {
				expected = application::github_capability_diagnostic::transport_missing_rate_tuple;
			}
			if (aScenario == fixture_scenario::capability_refusal_partial_rate) {
				expected = application::github_capability_diagnostic::transport_partial_rate_tuple;
			}
			if (aScenario == fixture_scenario::capability_refusal_wrong_resource) {
				expected = application::github_capability_diagnostic::transport_wrong_rate_resource;
			}
			if (aScenario == fixture_scenario::capability_refusal_content_type) {
				expected = application::github_capability_diagnostic::transport_invalid_content_type;
			}
			if (aScenario == fixture_scenario::capability_refusal_header_bound) {
				expected = application::github_capability_diagnostic::transport_header_limit;
			}
			if (aScenario == fixture_scenario::capability_refusal_body_bound) {
				expected = application::github_capability_diagnostic::transport_body_limit;
			}
			const auto* error   = std::get_if<github_capability_probe_wire_error>(&aCompletion);
			const auto  failure = aScenario == fixture_scenario::capability_refusal_header_bound
					|| aScenario == fixture_scenario::capability_refusal_body_bound
				? application::github_provider_failure::response_limit
				: application::github_provider_failure::malformed_response;
			expect(error != nullptr && error->Correlation == aCorrelation
					   && error->Error.Failure == failure && error->CapabilityDetail == expected,
				   "capability-native-refusal-has-exact-safe-detail");
			return;
		}

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
		auto        expectedRateLimitHeaders = std::array<capability_probe_header, 5U>{
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
			capability_probe_header{ },
		};
		if (aScenario == fixture_scenario::capability_metadata) {
			expectedRateLimitHeaders.back() = capability_probe_header{
				.Name = "Retry-After", .Value = "7"
			};
			expect(success != nullptr && success->Correlation == aCorrelation
					   && success->Response.Status == 200 && success->Response.Authenticated
					   && success->Response.ContentType == "Application/JSON; Charset=UTF-8"
					   && !success->Response.Body.empty()
					   && success->Response.RateLimitHeaderCount == 5U
					   && success->Response.RateLimitHeaders == expectedRateLimitHeaders,
				   "capability-fixture-five-header-metadata");
			expect(success->Summary.State == application::diagnostic_summary_state::sanitized
					   && success->Summary.text().find("Unknown schema field [omitted:unclassified-symbol]") != std::string_view::npos
					   && success->Summary.text().find("[omitted:unclassified-code]") != std::string_view::npos
					   && success->Summary.text().find("[omitted:unclassified-content]") != std::string_view::npos
					   && success->Summary.text().find("ghp_synthetic_secret_marker") == std::string_view::npos
					   && success->Summary.text().find('\n') == std::string_view::npos
					   && success->Summary.text().find('\x1b') == std::string_view::npos,
				   "native-worker-sanitizes-reflected-credential-before-completion");
			return;
		}
		expect(
			success != nullptr && success->Correlation == aCorrelation && success->Response.Status == 200 && success->Response.Authenticated && success->Response.ContentType == "Application/JSON; Charset=UTF-8" && !success->Response.Body.empty() && success->Response.RateLimitHeaderCount == 4U && success->Response.RateLimitHeaders == expectedRateLimitHeaders,
			"capability-fixture-success");
	}

	void expect_complete_capability_batch(const capability_probe_batch& aBatch)
	{
		const auto observation = aBatch.finish();
		expect(observation.Complete
				   && observation.Unavailability == capability_probe_unavailability::none
				   && valid(observation)
				   && observation.Capabilities == core::github_capabilities{
						  .ReviewThreads       = core::github_capability_state::supported,
						  .ViewedFileMutations = core::github_capability_state::supported,
						  .ResolveThread       = core::github_capability_state::supported,
						  .UnresolveThread     = core::github_capability_state::supported,
					  },
			   "capability-native-response-normalizes-supported-schema");
		expect(std::ranges::all_of(observation.Facts, [](const auto aFact) {
				   return aFact == capability_schema_fact_state::matched;
			   }),
			   "capability-native-response-matches-all-schema-facts");
	}

	// [[BPR-1018]]. The review-mutation exchanges at the GitHub.com authority,
	// where every answer must carry the rate-limit tuple and name its
	// resource. The loopback fixture answers each request by its method, path
	// and GraphQL operation rather than by position, so each answer pins the
	// request it answers and a request nobody scripted fails at the fixture.
	[[nodiscard]] constexpr auto is_review_exchange(const fixture_scenario aScenario) noexcept
		-> bool
	{
		return aScenario == fixture_scenario::mutation_graphql_course
			|| aScenario == fixture_scenario::mutation_review_submission
			|| aScenario == fixture_scenario::mutation_graphql_witness_failure
			|| aScenario == fixture_scenario::mutation_status_refusal
			|| aScenario == fixture_scenario::mutation_graphql_core_resource;
	}

	class fixture_review_mutation_lease final
		: public application::review_mutation_transport_lease
	{
	};

	/// \brief The recovery store a reply course needs, kept in memory.
	///
	/// \details Only as strict as the course needs: the session is under test
	/// here, and the SQLite store's own rules are proven by its own suite.
	class fixture_recovery final : public application::review_mutation_recovery_port
	{
	public:
		using failure = application::review_mutation_recovery_failure;
		using record  = application::review_mutation_recovery_record;

		[[nodiscard]] auto load(const std::string& aKey)
			-> std::expected<std::optional<record>, failure> override
		{
			if (const auto found = Records.find(aKey); found != Records.end()) {
				return found->second;
			}
			return std::optional<record>{ };
		}

		[[nodiscard]] auto admit(const record& aRecord)
			-> std::expected<application::review_mutation_recovery_admission, failure> override
		{
			const auto [found, inserted] = Records.try_emplace(aRecord.IdempotencyKey, aRecord);
			return application::review_mutation_recovery_admission{
				.Record   = found->second,
				.Inserted = inserted,
			};
		}

		[[nodiscard]] auto mark_possibly_transmitted(
			const std::string&                  aKey,
			const std::array<std::uint8_t, 32>& aFingerprint)
			-> std::expected<record, failure> override
		{
			const auto found = Records.find(aKey);
			if (found == Records.end() || found->second.RequestFingerprint != aFingerprint) {
				return std::unexpected(failure::conflict);
			}
			found->second.Transmission
				= application::review_mutation_recovery_transmission::possibly_transmitted;
			return found->second;
		}

		[[nodiscard]] auto resolve(
			const std::string&                          aKey,
			const std::array<std::uint8_t, 32>&         aFingerprint,
			const application::review_mutation_receipt& aReceipt)
			-> std::expected<record, failure> override
		{
			const auto found = Records.find(aKey);
			if (found == Records.end() || found->second.RequestFingerprint != aFingerprint
				|| found->second.Transmission
					!= application::review_mutation_recovery_transmission::possibly_transmitted) {
				return std::unexpected(failure::conflict);
			}
			found->second.State   = application::review_mutation_recovery_state::succeeded;
			found->second.Receipt = aReceipt;
			return found->second;
		}

		std::map<std::string, record> Records;
	};

	/// \brief Forwards to the native transport and records, per exchange, the
	/// operation, whether the session supplied a witness, how often the
	/// transport consulted it, and whether the exchange succeeded.
	class recording_mutation_transport final : public github_mutation_wire_transport
	{
	public:
		struct exchange
		{
			review_mutation_operation Operation        = review_mutation_operation::observe_pull_request;
			bool                      WitnessSupplied  = false;
			std::uint32_t             WitnessConsulted = 0U;
			bool                      Succeeded        = false;
		};

		explicit recording_mutation_transport(github_mutation_wire_transport& aInner) noexcept
			: Inner{ &aInner }
		{
		}

		[[nodiscard]] auto perform_review_mutation(
			const github_mutation_wire_request&  aRequest,
			const std::stop_token                aStop,
			github_mutation_transmission_witness aWitness)
			-> github_mutation_wire_completion override
		{
			auto observed = exchange{
				.Operation       = aRequest.Wire.Operation,
				.WitnessSupplied = static_cast<bool>(aWitness),
			};
			// The transport consults a witness on its own dispatch thread.
			auto consulted            = std::make_shared<std::atomic_uint32_t>(0U);
			auto witness              = aWitness
				? github_mutation_transmission_witness{ [consulted, inner = std::move(aWitness)] {
					  consulted->fetch_add(1U, std::memory_order_relaxed);
					  return inner();
				  } }
				: github_mutation_transmission_witness{ };
			auto completion           = Inner->perform_review_mutation(aRequest, aStop, std::move(witness));
			observed.WitnessConsulted = consulted->load(std::memory_order_relaxed);
			observed.Succeeded        = std::holds_alternative<github_mutation_wire_success>(completion);
			Exchanges.push_back(observed);
			return completion;
		}

		std::vector<exchange> Exchanges;

	private:
		github_mutation_wire_transport* Inner = nullptr;
	};

	[[nodiscard]] auto resolution_request_for(
		const validated_github_host_profile& aProfile,
		const bool                           aResolve)
		-> application::review_thread_resolution_request
	{
		const auto target   = mutation_target_for(aProfile);
		const auto identity = mutation_identity_for(aProfile);
		return {
			.Target         = target.Semantic,
			.ActingIdentity = identity.Account,
			.Thread         = "PRRT_fixture_1",
			.Resolve        = aResolve,
			.LiveBinding    = application::review_mutation_live_binding{
				.Target         = target,
				.ActingIdentity = identity,
			},
		};
	}

	/// \brief The production session's reply, resolution and unresolution,
	/// each through the native transport to the loopback fixture.
	///
	/// \details The reply is [[BPR-0896]]'s course as `reply_course_wires`
	/// pins it in live_review_mutation_provider_tests.cpp; the fixture pins
	/// each request's method, path and GraphQL document in the same order.
	void run_review_mutation_course(
		native_https_transport&              aTransport,
		const validated_github_host_profile& aProfile)
	{
		auto        recorder = recording_mutation_transport{ aTransport };
		auto        recovery = std::make_unique<fixture_recovery>();
		const auto& records  = recovery->Records;
		auto        session  = review_mutation_session{
			recorder,
			live_review_mutation_configuration{
				.CredentialContext = account_operation_context(aProfile, pull_request_context(aProfile)),
				.Target            = mutation_target_for(aProfile),
				.ActingIdentity    = mutation_identity_for(aProfile),
			},
			std::make_shared<fixture_review_mutation_lease>(),
			std::move(recovery),
		};

		const auto reply      = mutation_semantic_for(aProfile, "PRRT_fixture_1");
		const auto submission = session.submit_and_reconcile(reply, { });
		expect(submission.Result.has_value() && !submission.Result->Duplicate
				   && submission.Result->ServerCommentId == "PRRC_reply_45"
				   && submission.MutationAttempted && submission.Reconciliation.has_value()
				   && submission.Reconciliation->size() == 2U,
			   "mutation-course-reply-confirmed-against-its-thread");
		const auto record = records.find(reply.IdempotencyKey);
		expect(record != records.end()
				   && record->second.State == application::review_mutation_recovery_state::succeeded
				   && record->second.Receipt.has_value()
				   && record->second.Receipt->ServerCommentId == "PRRC_reply_45",
			   "mutation-course-reply-recorded-as-its-receipt");

		for (const auto resolve : { true, false }) {
			const auto receipt = session.resolve_thread(resolution_request_for(aProfile, resolve), { });
			expect(receipt.has_value() && receipt->Thread == "PRRT_fixture_1"
					   && receipt->Resolved == resolve,
				   resolve ? "mutation-course-resolved" : "mutation-course-unresolved");
		}

		// The session witnesses the reply and nothing else: the GraphQL reads
		// and both resolutions carry no witness and must still complete.
		using operation = review_mutation_operation;
		struct expected_exchange
		{
			operation Operation = operation::observe_pull_request;
			bool      Witnessed = false;
		};
		constexpr auto expected = std::array{
			expected_exchange{ .Operation = operation::observe_review_thread },
			expected_exchange{ .Operation = operation::observe_review_thread_comments },
			expected_exchange{ .Operation = operation::observe_viewer },
			expected_exchange{ .Operation = operation::observe_pull_request },
			expected_exchange{ .Operation = operation::reply_review_thread, .Witnessed = true },
			expected_exchange{ .Operation = operation::observe_review_thread_comments },
			expected_exchange{ .Operation = operation::observe_review_thread },
			expected_exchange{ .Operation = operation::resolve_review_thread },
			expected_exchange{ .Operation = operation::observe_review_thread },
			expected_exchange{ .Operation = operation::unresolve_review_thread },
		};
		expect(recorder.Exchanges.size() == expected.size(), "mutation-course-exchange-count");
		for (std::size_t index = 0U; index < expected.size(); ++index) {
			const auto& observed = recorder.Exchanges[index];
			expect(observed.Operation == expected[index].Operation && observed.Succeeded
					   && observed.WitnessSupplied == expected[index].Witnessed
					   && observed.WitnessConsulted == (expected[index].Witnessed ? 1U : 0U),
				   "mutation-course-exchange-" + std::to_string(index + 1U));
		}
	}

	void run_review_exchange_fixture(
		const fixture_scenario                 aScenario,
		const validated_github_host_profile&   aProfile,
		std::string&                           aCredential,
		const native_https_test_configuration& aConfiguration)
	{
		const auto context   = account_operation_context(aProfile, pull_request_context(aProfile));
		auto       transport = make_native_https_account_transport_for_test(
			aProfile,
			application::github_credential_retrieve_success{
				.Context    = context,
				.Credential = credential_material(aCredential),
			},
			aConfiguration);
		wipe(aCredential);
		expect(transport.has_value(), "review-exchange-transport");
		if (aScenario == fixture_scenario::mutation_graphql_course) {
			run_review_mutation_course(**transport, aProfile);
			return;
		}

		const auto server = aProfile.server_contract();
		const auto wire   = [&context](const review_mutation_request_result& aCompiled) {
			expect(aCompiled.has_value(), "review-exchange-wire");
			return github_mutation_wire_request{ .Context = context, .Wire = *aCompiled };
		};
		const auto comment = wire(compile_review_comment_request(mutation_semantic_for(aProfile), server));
		const auto review  = wire(compile_review_submission_request(
			review_submission_request{
				.Target         = mutation_target_for(aProfile),
				.ActingIdentity = mutation_identity_for(aProfile),
				.Disposition    = core::review_disposition::comment,
				.Body           = "Looks good.",
			},
			server));
		const auto reply   = wire(compile_review_comment_request(
			mutation_semantic_for(aProfile, "PRRT_fixture_1"), server));
		const auto resolve = wire(compile_thread_resolution_request(
			resolution_request_for(aProfile, true), server));
		const auto observe = wire(compile_thread_target_observation_request(
			resolution_request_for(aProfile, true), server));

		// One exchange. A witness that admits or refuses, or none at all; the
		// count is how often the transport consulted it.
		struct outcome
		{
			github_mutation_wire_completion Completion;
			std::uint32_t                   Consulted = 0U;
		};
		const auto perform = [&transport](const github_mutation_wire_request& aRequest,
										  const std::optional<bool>           aAdmit) {
			auto consulted  = std::make_shared<std::atomic_uint32_t>(0U);
			auto witness    = aAdmit.has_value()
				? github_mutation_transmission_witness{ [consulted, admit = *aAdmit] {
					  consulted->fetch_add(1U, std::memory_order_relaxed);
					  return admit;
				  } }
				: github_mutation_transmission_witness{ };
			auto completion = (*transport)->perform_review_mutation(aRequest, { }, std::move(witness));
			return outcome{
				.Completion = std::move(completion),
				.Consulted  = consulted->load(std::memory_order_relaxed),
			};
		};
		// Refused after the POST was handed to the network, so possibly sent.
		const auto refusedAfterPost = [](const outcome& aOutcome, const application::review_mutation_failure aFailure = application::review_mutation_failure::malformed_response) {
			const auto* error = std::get_if<github_mutation_wire_error>(&aOutcome.Completion);
			return error != nullptr
				&& error->Failure == aFailure
				&& error->Transmission == github_mutation_transmission::indeterminate;
		};

		if (aScenario == fixture_scenario::mutation_review_submission) {
			// The other witnessed REST write, which GitHub answers 200 OK.
			const auto  submitted = perform(review, true);
			const auto* success   = std::get_if<github_mutation_wire_success>(&submitted.Completion);
			expect(success != nullptr && success->Response.Status == 200 && submitted.Consulted == 1U,
				   "review-submission-completes-with-200-after-one-witness");
			using op = application::staged_review_operation;
			for (auto operation : { op::update_summary, op::update_comment, op::submit_comment, op::discard_pending }) {
				application::staged_review_request staged{ .Binding = { mutation_target_for(aProfile), mutation_identity_for(aProfile) }, .Operation = operation };
				if (operation == op::update_comment) {
					staged.Comment = 44;
				} else {
					staged.Review = application::review_database_id{ 80 };
				}
				if (operation != op::discard_pending) {
					staged.Body = "Literal `$(no shell)` tour";
				}
				const auto  sent     = perform(wire(compile_staged_review_request(staged)), true);
				const auto* answered = std::get_if<github_mutation_wire_success>(&sent.Completion);
				expect(answered && answered->Response.Status == 200 && sent.Consulted == 1, "staged-exact-verb-one-witness");
			}
			return;
		}
		if (aScenario == fixture_scenario::mutation_graphql_witness_failure) {
			const auto refused = perform(reply, false);
			expect(refusedAfterPost(refused, application::review_mutation_failure::unavailable) && refused.Consulted == 1U,
				   "graphql-reply-refused-by-its-witness-is-aborted");
			return;
		}
		if (aScenario == fixture_scenario::mutation_graphql_core_resource) {
			const auto answered = perform(observe, std::nullopt);
			expect(refusedAfterPost(answered) && answered.Consulted == 0U,
				   "graphql-review-read-accounted-to-core-is-refused");
			return;
		}
		expect(aScenario == fixture_scenario::mutation_status_refusal, "review-exchange-scenario");
		// Every answer is a 2xx carrying the right rate-limit resource; only
		// the status is not the operation's own.
		struct refusal
		{
			const github_mutation_wire_request* Request = nullptr;
			std::optional<bool>                 Admit;
			std::string_view                    Name;
		};
		for (const auto& row : {
				 refusal{ .Request = &comment, .Admit = true, .Name = "rest-comment-answered-200-is-refused" },
				 refusal{ .Request = &review, .Admit = true, .Name = "review-submission-answered-201-is-refused" },
				 refusal{ .Request = &reply, .Admit = true, .Name = "graphql-reply-answered-201-is-refused" },
				 refusal{ .Request = &resolve, .Name = "graphql-resolution-answered-201-is-refused" },
				 refusal{ .Request = &observe, .Name = "graphql-thread-read-answered-201-is-refused" },
			 }) {
			const auto answered = perform(*row.Request, row.Admit);
			expect(refusedAfterPost(answered) && answered.Consulted == (row.Admit.has_value() ? 1U : 0U),
				   row.Name);
			const auto& error = std::get<github_mutation_wire_error>(answered.Completion);
			expect(error.ProviderFailure.has_value() && error.HttpStatus == (row.Request == &comment ? 200U : 201U) && error.TransportFailureSource == application::diagnostic_transport_failure_source::http_response, "mutation HTTP refusal retains typed status without response body");
		}
		const auto  rejected  = perform(comment, true);
		const auto* rejection = std::get_if<github_mutation_wire_success>(&rejected.Completion);
		expect(rejection && rejection->Response.Status == 422 && rejection->Response.Body == R"({"message":"Validation Failed"})" && rejected.Consulted == 1U,
			   "complete bounded REST comment 422 reaches the exact classifier as data");
		expect(refusedAfterPost(perform(review, true)), "whole-review 422 remains a refusal");
		const auto truncated = perform(comment, true);
		expect(std::holds_alternative<github_mutation_wire_error>(truncated.Completion) && truncated.Consulted == 1U, "truncated comment 422 cannot reach classifier as a complete response");
		expect(refusedAfterPost(perform(comment, true)), "comment 422 with Location cannot discard conflicting routing metadata");
	}

	void run_fixture(const fixture_scenario aScenario,
					 const std::uint16_t    aProxyPort,
					 const std::uint16_t    aFallbackProxyPort)
	{
		auto framedCredential     = read_frame(application::maximum_github_credential_bytes,
											   "fixture-credential-limit");
		auto authorityCertificate = read_frame(
			native_https_maximum_response_header_bytes, "fixture-authority-limit");
		// The credential-free course is the one case where the harness sends an
		// empty credential frame. That is the point: this process never holds
		// credential material, so there is nothing to discover, look up, or fall
		// back to. Every other course still requires one.
		if (aScenario == fixture_scenario::credential_free
			|| aScenario == fixture_scenario::anonymous_read) {
			expect(framedCredential.empty(), "credential-free-material-present");
		} else {
			expect(!framedCredential.empty(), "fixture-credential-empty");
		}
		expect(!authorityCertificate.empty(), "fixture-authority-empty");

		const auto profile = validate_github_host_profile(
			(aScenario == fixture_scenario::capability_success || is_capability_refusal(aScenario)
			 || is_review_exchange(aScenario))
				? github_com_profile_candidate()
				: profile_candidate(application::github_authentication_transport::native_https));
		expect(profile.has_value(), "fixture-profile");

		auto configuration = native_https_test_configuration{
			.ProxyHost               = "127.0.0.1",
			.ProxyPort               = aProxyPort,
			.FallbackProxyPort       = aFallbackProxyPort,
			.TrustedCaCertificatePem = aScenario == fixture_scenario::untrusted_certificate
				? std::string{ }
				: std::move(authorityCertificate),
			.TotalTimeout            = aScenario == fixture_scenario::timeout ? 1s : 5s,
			.ResponseBodyLimit       = (aScenario == fixture_scenario::response_limit || aScenario == fixture_scenario::capability_refusal_body_bound)
				? 1'024U
				: native_https_maximum_response_body_bytes,
		};

		if (is_review_exchange(aScenario)) {
			run_review_exchange_fixture(aScenario, *profile, framedCredential, configuration);
			return;
		}
		if (aScenario == fixture_scenario::anonymous_read
			|| aScenario == fixture_scenario::anonymous_read_canary) {
			const auto anonymousProfile = validate_github_host_profile(
				github_com_profile_candidate());
			expect(anonymousProfile.has_value(), "anonymous-profile");
			auto request = anonymous_wire_request_for(
				*anonymousProfile, read_operation::get_repository);
			if (aScenario == fixture_scenario::anonymous_read) {
				// This process is handed no credential at all, so there is
				// nothing here to look up, discover, or fall back to. The
				// fixture treats any authorization header as a violation, and
				// the canary course below proves it would have seen one.
				expect(framedCredential.empty(),
					   "anonymous-read-material-present");
				auto transport = make_native_https_anonymous_read_transport_for_test(
					*anonymousProfile,
					anonymous_session_context(*anonymousProfile),
					configuration);
				expect(transport.has_value(), "anonymous-read-transport");
				for (const auto provenance : { core::application_session_id{ }, core::application_session_id{ 1U } }) {
					auto forged                              = request;
					forged.Correlation.AuthenticationSession = provenance;
					expect_unavailable((*transport)->perform(forged, { }), forged.Correlation, "anonymous-read-refuses-authentication-provenance");
				}
				const auto  completion = (*transport)->perform(request, std::stop_token{ });
				const auto* success    = std::get_if<github_wire_success>(&completion);
				expect(success != nullptr
						   && success->Correlation == request.Correlation
						   && success->Response.Status == 200,
					   "anonymous-read-success");
				// The two refused operations. Each returns before a request is
				// built, and the fixture's exact request count is what proves
				// neither one reached the wire.
				for (const auto refusedOperation :
					 { read_operation::list_review_threads,
					   read_operation::get_viewer_identity }) {
					const auto refused = anonymous_wire_request_for(
						*anonymousProfile, refusedOperation);
					expect_unavailable(
						(*transport)->perform(refused, std::stop_token{ }),
						refused.Correlation,
						"anonymous-read-refusal");
				}
				return;
			}
			expect(!framedCredential.empty(), "anonymous-canary-credential-empty");
			const auto parent = std::get<core::pull_request_request_context>(
				request.Correlation.Parent);
			auto transport = make_native_https_account_transport_for_test(
				*anonymousProfile,
				application::github_credential_retrieve_success{
					.Context    = account_operation_context(*anonymousProfile, parent),
					.Credential = credential_material(framedCredential),
				},
				configuration);
			wipe(framedCredential);
			expect(transport.has_value(), "anonymous-canary-transport");
			request.Correlation.AuthenticationSession = parent.ApplicationSession;
			auto windowParent                         = parent;
			windowParent.ApplicationSession           = core::application_session_id{ 900U };
			request.Correlation.Parent                = windowParent;
			for (const auto variant : { 0, 1, 2 }) {
				auto forged  = request;
				auto changed = windowParent;
				if (variant == 0) {
					forged.Correlation.AuthenticationSession = core::application_session_id{ 999U };
				}
				if (variant == 1) {
					changed.IdentityGeneration = core::github_identity_generation{ 999U };
				}
				if (variant == 2) {
					changed.Account = core::github_account_id{ 999U };
				}
				forged.Correlation.Parent = changed;
				expect_unavailable((*transport)->perform(forged, { }), forged.Correlation, "window-read-forged-provenance-refuses-before-fixture-request");
			}
			const auto  completion = (*transport)->perform(request, std::stop_token{ });
			const auto* success    = std::get_if<github_wire_success>(&completion);
			expect(success != nullptr
					   && success->Correlation == request.Correlation
					   && success->Response.Status == 200,
				   "anonymous-canary-success");
			return;
		}
		if (aScenario == fixture_scenario::credential_free) {
			const auto request = wire_request_for(
				*profile, read_operation::get_viewer_identity);
			auto transport = make_native_https_credential_free_transport_for_test(
				*profile, session_context(*profile), configuration);
			expect(transport.has_value(), "credential-free-transport");
			const auto completion = (*transport)->perform(request, std::stop_token{ });
			expect_success(completion, request.Correlation, aScenario);
			// A credential-free binding is not one-shot the way a bootstrap
			// credential is, so a second request must still succeed. If this
			// ever starts failing, the state has quietly become a spent
			// credential rather than a deliberate absence.
			const auto replay = (*transport)->perform(request, std::stop_token{ });
			expect_success(replay, request.Correlation, aScenario);
			// A binding with no credential has no account authority, so a
			// pull-request-scoped parent must be refused rather than served
			// under some other account's correlation. The fixture's exact
			// two-request count is what proves the refusal never reached the
			// wire.
			const auto foreign = wire_request_for(
				*profile, read_operation::list_review_threads);
			expect_unavailable(
				(*transport)->perform(foreign, std::stop_token{ }),
				foreign.Correlation,
				"credential-free-refuses-pull-request-parent");
			return;
		}
		if (is_capability_refusal(aScenario) || aScenario == fixture_scenario::capability_success
			|| aScenario == fixture_scenario::capability_metadata
			|| aScenario == fixture_scenario::capability_redirect
			|| aScenario == fixture_scenario::capability_403_rate
			|| aScenario == fixture_scenario::capability_429) {
			const auto request = capability_request_for(
				*profile, session_context(*profile), core::github_account_id{ 203 });
			auto transport = fixture_transport(*profile, request, framedCredential,
											   configuration);
			wipe(framedCredential);
			expect(transport.has_value(), "capability-fixture-transport");
			capability_probe_batch batch;
			const auto             parts = aScenario == fixture_scenario::capability_success ? capability_probe_part_count : std::uint8_t{ 1U };
			for (std::uint8_t part = 0U; part < parts; ++part) {
				const auto partRequest = capability_request_for(*profile, request.Correlation.Session, request.Correlation.ObservedAccount, part);
				const auto completion  = (*transport)->perform_capability_probe(partRequest, std::stop_token{ });
				expect_capability_fixture_completion(completion, partRequest.Correlation, aScenario);
				if (aScenario == fixture_scenario::capability_success) {
					const auto& response = std::get<github_capability_probe_wire_success>(completion).Response;
					expect(batch.append(response, part).has_value(), "native canonical part assembles");
				}
			}
			if (aScenario == fixture_scenario::capability_success) {
				expect_complete_capability_batch(batch);
			}

			return;
		}
		if (aScenario == fixture_scenario::mutation_witness
			|| aScenario == fixture_scenario::mutation_witness_failure
			|| aScenario == fixture_scenario::mutation_proxy_no_fallback) {
			const auto request   = mutation_request_for(*profile);
			auto       transport = make_native_https_account_transport_for_test(
				*profile,
				application::github_credential_retrieve_success{
					.Context    = request.Context,
					.Credential = credential_material(framedCredential),
				},
				configuration);
			wipe(framedCredential);
			expect(transport.has_value(), "mutation-fixture-transport");
			std::atomic_uint32_t witnessCount{ 0U };
			const auto           completion = (*transport)->perform_review_mutation(request, std::stop_token{ }, [&witnessCount, aScenario] {
				witnessCount.fetch_add(1U, std::memory_order_relaxed);
				return aScenario != fixture_scenario::mutation_witness_failure;
			});
			expect(
				witnessCount.load(std::memory_order_relaxed) == 1U,
				"mutation-fixture-one-witness");
			if (aScenario == fixture_scenario::mutation_witness) {
				const auto* success = std::get_if<github_mutation_wire_success>(
					&completion);
				expect(
					success != nullptr && success->Response.Status == 201,
					"mutation-fixture-success");
			} else {
				const auto* failure = std::get_if<github_mutation_wire_error>(
					&completion);
				expect(
					failure != nullptr
						&& failure->Failure
							== application::review_mutation_failure::unavailable
						&& failure->Transmission
							== github_mutation_transmission::indeterminate,
					"mutation-fixture-failed-witness-aborts");
			}
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
				(*scenario == fixture_scenario::proxy_fallback
				 || *scenario == fixture_scenario::mutation_proxy_no_fallback)
				&& argument_count == 7
				&& std::string_view{ arguments[5] }
					== "--fallback-proxy-port"
				&& fallbackProxyPort.has_value())
				|| (*scenario != fixture_scenario::proxy_fallback
					&& *scenario != fixture_scenario::mutation_proxy_no_fallback
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
			<< failure.what()
			<< '\n';
		return EXIT_FAILURE;
	} catch (...) {
		std::cerr << "native-https-transport:fail:internal\n";
		return EXIT_FAILURE;
	}
}
