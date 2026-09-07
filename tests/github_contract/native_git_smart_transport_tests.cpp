#include "bpr/application/git_protocol_v2.hpp"

#include "github_host_profile.hpp"
#include "native_https_transport.hpp"

#include "bpr/core/epoch_transition.hpp"

#include <QCoreApplication>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <iostream>
#include <span>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;
	using namespace bpr::adapters::github;
	using namespace bpr::adapters::github::detail;
	using namespace std::chrono_literals;

	constexpr std::string_view discovery_body   = "001e# service=git-upload-pack\n0000000eversion 2\n000cls-refs\n000afetch\n0017object-format=sha1\n0000";
	constexpr std::string_view ls_refs_body     = "0008NAK\n0000";
	constexpr std::size_t      fetch_body_bytes = 200'000U;

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
		std::array<std::uint8_t, 32> bytes{ };
		bytes[0]  = aSeed;
		bytes[31] = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return Identity{ bytes };
	}

	template <typename Oid>
	[[nodiscard]] constexpr auto oid(const std::uint8_t aSeed) -> Oid
	{
		std::array<std::uint8_t, 32> bytes{ };
		for (std::size_t index = 0U; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + static_cast<std::uint8_t>(index));
		}
		return Oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto profile_candidate(
		const bool aGithubCom = false) -> github_host_profile_candidate
	{
		return {
			.Profile        = core::host_profile_id{ 11U },
			.Host           = core::git_host_id{ 21U },
			.Revision       = application::github_profile_revision{ 31U },
			.Server         = aGithubCom
				? application::github_server_contract::
					  github_com_2022_11_28
				: application::github_server_contract::
					  ghes_3_18_2022_11_28,
			.Authentication = application::github_authentication_transport::native_https,
			.Proxy          = application::github_proxy_mode::system,
			.Trust          = application::github_trust_mode::system,
			.Provenance     = github_profile_provenance::managed,
			.Hostname       = aGithubCom ? "github.com" : "ghe.bpr.invalid",
			.RestRoot       = aGithubCom
				? "https://api.github.com"
				: "https://ghe.bpr.invalid/api/v3",
			.GraphqlRoot    = aGithubCom
				? "https://api.github.com/graphql"
				: "https://ghe.bpr.invalid/api/graphql",
		};
	}

	[[nodiscard]] auto credential_material(const std::string_view aValue)
		-> application::github_credential_material
	{
		auto bytes = std::vector<std::byte>{ };
		bytes.reserve(aValue.size());
		for (const auto character : aValue) {
			bytes.push_back(
				static_cast<std::byte>(static_cast<unsigned char>(character)));
		}
		auto material = application::github_credential_material::create(bytes);
		expect(material.has_value(), "credential-material");
		return std::move(*material);
	}

	[[nodiscard]] auto smart_context()
		-> application::git_smart_request_context
	{
		const auto baseRef = application::parse_git_base_branch_ref("refs/heads/main");
		expect(baseRef.has_value(), "base-ref");
		return {
			.ApplicationSession = core::application_session_id{ 202U },
			.Authority          = core::pull_request_authority{
				.Profile         = core::host_profile_id{ 11U },
				.ProfileRevision = core::github_profile_revision{ 31U },
				.Host            = core::git_host_id{ 21U },
				.HostAuthority   = opaque<core::host_authority_fingerprint>(1U),
				.Account         = core::github_account_id{ 203U },
				.Viewer          = core::github_viewer_actor_id{ 204U },
				.Repository      = core::repository_id{ 205U },
				.Cache           = opaque<core::repository_cache_scope_id>(2U),
				.PullRequest     = core::pull_request_id{ 206U },
				.Number          = core::pull_request_number{ 17U },
				.Scope           = opaque<core::pull_request_authority_scope_id>(3U),
			},
			.Revision = core::accepted_pull_request_revision{
				.Source = core::pull_request_source_revision{
					.Observation           = opaque<core::epoch_source_observation_id>(9U),
					.Snapshot              = core::provider_snapshot_id{ 301U },
					.IdentityGeneration    = core::github_identity_generation{ 207U },
					.PullRequestGeneration = core::request_generation{ 208U },
				},
				.Origin     = core::pull_request_revision_origin::live,
				.BaseBranch = opaque<core::base_branch_id>(4U),
				.Base       = oid<core::git_commit_oid>(10U),
				.Head       = oid<core::git_commit_oid>(30U),
			},
			.Repository = core::repository_locator{
				.Owner = "octo-org",
				.Name  = "review-repository",
			},
			.BaseRef = *baseRef,
		};
	}

	// The signed-out twin of `smart_context()`. Only the reader changes: the
	// authority names no account and no viewer and carries the public-preview
	// discriminant, which is what admits the credential-free transport.
	[[nodiscard]] auto anonymous_smart_context()
		-> application::git_smart_request_context
	{
		auto context              = smart_context();
		context.Authority.Mode    = core::github_access_mode::public_preview;
		context.Authority.Account = core::github_account_id{ };
		context.Authority.Viewer  = core::github_viewer_actor_id{ };
		return context;
	}

	[[nodiscard]] auto anonymous_session_context(
		const validated_github_host_profile& aProfile)
		-> core::github_session_request_context
	{
		return {
			.Request            = core::request_id{ 101U },
			.ApplicationSession = core::application_session_id{ 202U },
			.Profile            = aProfile.profile(),
			.IdentityGeneration = core::github_identity_generation{ 207U },
		};
	}

	[[nodiscard]] auto credential_context(
		const validated_github_host_profile& aProfile)
		-> application::github_credential_operation_context
	{
		return {
			.Request            = core::request_id{ 201U },
			.ApplicationSession = core::application_session_id{ 202U },
			.Authority          = application::github_credential_authority_key{
				.Profile         = aProfile.profile(),
				.ProfileRevision = aProfile.revision(),
				.Host            = aProfile.host(),
				.Hostname        = application::github_canonical_hostname{
					aProfile.hostname() },
				.Account        = core::github_account_id{ 203U },
				.ServerContract = aProfile.server_contract(),
			},
			.IdentityGeneration = core::github_identity_generation{ 207U },
		};
	}

	[[nodiscard]] auto bytes(const std::string_view aValue)
		-> std::vector<std::byte>
	{
		auto result = std::vector<std::byte>{ };
		result.reserve(aValue.size());
		for (const auto character : aValue) {
			result.push_back(
				static_cast<std::byte>(static_cast<unsigned char>(character)));
		}
		return result;
	}

	[[nodiscard]] auto read_frame_length() -> std::uint32_t
	{
		std::array<unsigned char, 4> value{ };
		std::cin.read(
			reinterpret_cast<char*>(value.data()),
			static_cast<std::streamsize>(value.size()));
		expect(std::cin.gcount() == static_cast<std::streamsize>(value.size()),
			   "frame-length");
		return (static_cast<std::uint32_t>(value[0]) << 24U)
			| (static_cast<std::uint32_t>(value[1]) << 16U)
			| (static_cast<std::uint32_t>(value[2]) << 8U)
			| static_cast<std::uint32_t>(value[3]);
	}

	[[nodiscard]] auto read_frame(const std::size_t aMaximum) -> std::string
	{
		const auto length = read_frame_length();
		expect(length <= aMaximum, "frame-limit");
		auto result = std::string(length, '\0');
		std::cin.read(result.data(), static_cast<std::streamsize>(result.size()));
		expect(std::cin.gcount() == static_cast<std::streamsize>(result.size()),
			   "frame-body");
		return result;
	}

	class recording_consumer final
		: public application::git_smart_wire_consumer
	{
	public:
		explicit recording_consumer(const bool aReject = false) noexcept
			: Reject{ aReject }
		{
		}

		[[nodiscard]] auto consume(
			const std::span<const std::byte> aValue) noexcept -> bool override
		{
			++Calls;
			MaximumChunk = std::max(MaximumChunk, aValue.size());
			if (Reject) {
				return false;
			}
			for (const auto byte : aValue) {
				if (byte != std::byte{ 'P' }) {
					Valid = false;
				}
			}
			Total += aValue.size();
			return true;
		}

		bool        Reject       = false;
		bool        Valid        = true;
		std::size_t Calls        = 0U;
		std::size_t MaximumChunk = 0U;
		std::size_t Total        = 0U;
	};

	[[nodiscard]] auto make_transport(
		const validated_github_host_profile&   aProfile,
		const std::string_view                 aCredential,
		const native_https_test_configuration& aConfiguration)
		-> std::shared_ptr<native_https_transport>
	{
		auto created = make_native_https_account_transport_for_test(
			aProfile,
			application::github_credential_retrieve_success{
				.Context    = credential_context(aProfile),
				.Credential = credential_material(aCredential),
			},
			aConfiguration);
		expect(created.has_value(), "native-transport");
		return std::shared_ptr<native_https_transport>{ std::move(*created) };
	}

	void expect_failure(
		const application::git_smart_transport_error&  aError,
		const application::git_smart_transport_failure aExpected,
		const std::string_view                         aMessage)
	{
		expect(aError.Failure == aExpected, aMessage);
	}

	void run_contract()
	{
		static_assert(!std::is_copy_constructible_v<
					  application::git_smart_transport_port>);
		static_assert(
			application::maximum_git_smart_control_response_bytes == 64U * 1024U);
		static_assert(native_https_total_timeout == 30s);
		static_assert(native_git_smart_total_timeout == 15min);

		const auto profile = validate_github_host_profile(profile_candidate());
		expect(profile.has_value(), "profile");
		const auto githubComProfile = validate_github_host_profile(profile_candidate(true));
		expect(
			githubComProfile.has_value()
				&& githubComProfile->hostname() == "github.com"
				&& githubComProfile->rest_authority() == "api.github.com",
			"github-com-profile-authorities");
		const auto configuration = native_https_test_configuration{
			.ProxyHost    = "127.0.0.1",
			.ProxyPort    = 1U,
			.TotalTimeout = 250ms,
		};
		// The enterprise refusal happens before any transport exists, so it is
		// proved here rather than on the wire: there is no request to observe
		// because there was never a connection to make one on.
		const auto ghesAnonymous = make_native_https_anonymous_smart_transport(
			*profile, anonymous_session_context(*profile));
		expect(!ghesAnonymous.has_value()
				   && ghesAnonymous.error()
					   == native_https_configuration_failure::
						   anonymous_read_contract_unsupported,
			   "ghes-anonymous-smart-production");
		const auto ghesAnonymousForTest
			= make_native_https_anonymous_smart_transport_for_test(
				*profile, anonymous_session_context(*profile), configuration);
		expect(!ghesAnonymousForTest.has_value()
				   && ghesAnonymousForTest.error()
					   == native_https_configuration_failure::
						   anonymous_read_contract_unsupported,
			   "ghes-anonymous-smart-loopback");

		// [[BPR-0663]]. The anonymous binding admits a lease only for the
		// identity generation the accepted revision names: the generation the
		// wire was built with must be the revision's, not a constant. The
		// shipped factory once passed a constant and every public-preview
		// acquisition was refused before a request was made.
		{
			auto anonymousWire = make_native_https_anonymous_smart_transport_for_test(
				*githubComProfile, anonymous_session_context(*githubComProfile), configuration);
			expect(anonymousWire.has_value(), "github-com-anonymous-smart-loopback");
			auto exact                      = anonymous_smart_context();
			exact.Authority.Profile         = githubComProfile->profile();
			exact.Authority.ProfileRevision = core::github_profile_revision{ githubComProfile->revision().value() };
			exact.Authority.Host            = githubComProfile->host();
			auto       wire                 = std::shared_ptr<native_https_transport>{ std::move(*anonymousWire) };
			const auto admitted             = make_native_git_smart_transport(wire, exact);
			expect(admitted.has_value(), "anonymous-exact-generation-acquisition");
			auto drifted                               = exact;
			drifted.Revision.Source.IdentityGeneration = core::github_identity_generation{ 918U };
			const auto refused                         = make_native_git_smart_transport(wire, drifted);
			expect(!refused.has_value(), "anonymous-generation-drift-acquisition-refused");
			expect_failure(
				refused.error(),
				application::git_smart_transport_failure::invalid_request,
				"anonymous-generation-drift-failure");
		}

		auto transport = make_transport(*profile, "BPR0043ContractToken", configuration);
		auto context   = smart_context();

		auto wrongAuthority               = context;
		wrongAuthority.ApplicationSession = core::application_session_id{ 999U };
		const auto mismatched             = make_native_git_smart_transport(transport, wrongAuthority);
		expect(!mismatched.has_value(), "mismatched-acquisition");

		auto lease = make_native_git_smart_transport(transport, context);
		expect(lease.has_value(), "exact-acquisition");
		auto drift            = context;
		drift.Repository.Name = "other-repository";
		auto       request    = application::git_smart_discovery_request{ .Context = drift };
		const auto drifted    = (*lease)->discover(request, { });
		expect(!drifted.has_value(), "drifted-request");
		expect_failure(
			drifted.error(),
			application::git_smart_transport_failure::invalid_request,
			"drifted-request-failure");

		(*lease)->revoke();
		expect((*lease)->revoked(), "lease-revoked");
		request.Context    = std::move(context);
		const auto revoked = (*lease)->discover(request, { });
		expect(!revoked.has_value(), "revoked-request");
		expect_failure(
			revoked.error(),
			application::git_smart_transport_failure::cancelled,
			"revoked-request-failure");
	}

	void run_fixture(
		const std::string_view aScenario,
		const std::uint16_t    aProxyPort,
		const std::string_view aProfileName)
	{
		expect(
			aProfileName == "ghes" || aProfileName == "github-com",
			"fixture-profile-name");
		auto       credential = read_frame(application::maximum_github_credential_bytes);
		auto       ca         = read_frame(native_https_maximum_response_header_bytes);
		const auto profile    = validate_github_host_profile(
			profile_candidate(aProfileName == "github-com"));
		expect(profile.has_value(), "fixture-profile");
		const auto configuration = native_https_test_configuration{
			.ProxyHost                 = "127.0.0.1",
			.ProxyPort                 = aProxyPort,
			.DirectFirstProxyCandidate = aScenario == "direct-fallback",
			.TrustedCaCertificatePem   = std::move(ca),
			// Leave enough time for target-native proxy discovery and TLS setup;
			// the fixture origin remains silent for two seconds, so this still
			// proves the configured absolute deadline rather than connect delay.
			.TotalTimeout = aScenario == "timeout" ? 1500ms : 5s,
		};
		const auto anonymous = aScenario.starts_with("anonymous-");
		// The credential frame is read and destroyed on both courses. On the
		// anonymous course nothing is ever handed it, so a fixture that
		// refuses an Authorization header proves the absence rather than a
		// caller that merely forgot to pass one.
		auto transport = anonymous
			? [&] {
				  auto created = make_native_https_anonymous_smart_transport_for_test(
					  *profile, anonymous_session_context(*profile), configuration);
				  expect(created.has_value(), "anonymous-native-transport");
				  return std::shared_ptr<native_https_transport>{ std::move(*created) };
			  }()
			: make_transport(*profile, credential, configuration);
		std::fill(credential.begin(), credential.end(), '\0');
		credential.clear();
		const auto context = anonymous ? anonymous_smart_context() : smart_context();
		auto       lease   = make_native_git_smart_transport(transport, context);
		expect(lease.has_value(), "fixture-lease");

		if (anonymous) {
			if (aScenario == "anonymous-discovery") {
				const auto result = (*lease)->discover(
					application::git_smart_discovery_request{ .Context = context }, { });
				expect(result.has_value(), "anonymous-discovery-result");
				expect(result->Body == bytes(discovery_body),
					   "anonymous-discovery-body");
				return;
			}
			const auto anonymousBody = bytes("0014command=ls-refs\n0000");
			if (aScenario == "anonymous-ls-refs") {
				const auto result = (*lease)->ls_refs(
					application::git_smart_ls_refs_request{
						.Context = context,
						.Body    = anonymousBody,
					},
					{ });
				expect(result.has_value(), "anonymous-ls-refs-result");
				expect(result->Body == bytes(ls_refs_body), "anonymous-ls-refs-body");
				return;
			}
			expect(aScenario == "anonymous-fetch", "anonymous-scenario");
			recording_consumer consumer{ false };
			const auto         result = (*lease)->fetch(
				application::git_smart_fetch_request{
					.Context              = context,
					.Body                 = anonymousBody,
					.MaximumResponseBytes = fetch_body_bytes,
				},
				consumer, { });
			expect(result.has_value(), "anonymous-fetch-result");
			expect(consumer.Valid && consumer.Total == fetch_body_bytes,
				   "anonymous-fetch-body");
			return;
		}

		if (aScenario == "revoke-isolation") {
			auto other = make_native_git_smart_transport(transport, context);
			expect(other.has_value(), "isolation-other-lease");
			const auto request = application::git_smart_discovery_request{ .Context = context };
			application::git_smart_transport_result<
				application::git_smart_discovery_response>
						 result = std::unexpected{ application::git_smart_transport_error{ } };
			std::jthread worker{ [&] {
				result = (*lease)->discover(request, { });
			} };
			std::this_thread::sleep_for(1500ms);
			(*other)->revoke();
			worker.join();
			expect(
				result.has_value() && result->Body == bytes(discovery_body),
				"isolation-active-result");
			expect(
				!(*lease)->revoked() && (*other)->revoked(),
				"isolation-revoke-state");
			return;
		}

		if (aScenario == "active-shutdown") {
			const auto request = application::git_smart_discovery_request{ .Context = context };
			application::git_smart_transport_result<
				application::git_smart_discovery_response>
								 result    = std::unexpected{ application::git_smart_transport_error{ } };
			std::atomic_uint32_t terminals = 0U;
			std::jthread         worker{ [&] {
				result = (*lease)->discover(request, { });
				terminals.fetch_add(1U, std::memory_order_release);
			} };
			std::this_thread::sleep_for(1500ms);
			(*lease)->revoke();
			worker.join();
			expect(
				!result.has_value(),
				"active-shutdown-result");
			expect_failure(
				result.error(),
				application::git_smart_transport_failure::cancelled,
				"active-shutdown-terminal");
			expect(
				terminals.load(std::memory_order_acquire) == 1U,
				"active-shutdown-terminal-count");
			expect((*lease)->revoked(), "active-shutdown-revoked");
			lease->reset();
			transport.reset();
			return;
		}

		if (aScenario == "discovery" || aScenario.starts_with("redirect")
			|| aScenario.starts_with("http-")
			|| aScenario == "wrong-media" || aScenario == "header-limit"
			|| aScenario == "timeout" || aScenario == "direct-fallback"
			|| aScenario == "cancel" || aScenario == "revoke") {
			const auto request = application::git_smart_discovery_request{ .Context = context };
			if (aScenario == "cancel" || aScenario == "revoke") {
				std::stop_source stop;
				application::git_smart_transport_result<
					application::git_smart_discovery_response>
							 result = std::unexpected{ application::git_smart_transport_error{ } };
				std::jthread worker{ [&] {
					result = (*lease)->discover(request, stop.get_token());
				} };
				std::this_thread::sleep_for(1500ms);
				if (aScenario == "cancel") {
					stop.request_stop();
				} else {
					(*lease)->revoke();
				}
				worker.join();
				expect(!result.has_value(), "cancel-result");
				expect_failure(
					result.error(),
					application::git_smart_transport_failure::cancelled,
					"cancel-failure");
				expect(aScenario != "revoke" || (*lease)->revoked(),
					   "revoke-state");
				return;
			}
			const auto result = (*lease)->discover(request, { });
			if (aScenario == "discovery" || aScenario == "direct-fallback") {
				expect(result.has_value(), "discovery-result");
				expect(result->Body == bytes(discovery_body), "discovery-body");
				expect(application::detail::parse_git_protocol_v2_advertisement(
						   result->Body, core::git_hash_algorithm::sha1)
						   .has_value(),
					   "native-discovery-parses-as-v2");
				return;
			}
			expect(!result.has_value(), "discovery-failure");
			const auto expected = aScenario.starts_with("redirect")
				? application::git_smart_transport_failure::redirect
				: aScenario == "http-401"
				? application::git_smart_transport_failure::authentication
				: aScenario == "http-403"
				? application::git_smart_transport_failure::permission
				: aScenario == "http-503"
				? application::git_smart_transport_failure::unavailable
				: aScenario == "header-limit"
				? application::git_smart_transport_failure::response_limit
				: aScenario == "wrong-media"
				? application::git_smart_transport_failure::
					  malformed_response
				: application::git_smart_transport_failure::timeout;
			expect_failure(result.error(), expected, "discovery-failure-kind");
			return;
		}

		const auto requestBody = bytes("0014command=ls-refs\n0000");
		if (aScenario == "ls-refs") {
			const auto result = (*lease)->ls_refs(
				application::git_smart_ls_refs_request{
					.Context = context,
					.Body    = requestBody,
				},
				{ });
			expect(result.has_value(), "ls-refs-result");
			expect(result->Body == bytes(ls_refs_body), "ls-refs-body");
			return;
		}

		recording_consumer consumer{ aScenario == "consumer-reject" };
		const auto         maximum = aScenario == "response-limit" ? 1'024U : fetch_body_bytes;
		const auto         result  = (*lease)->fetch(
			application::git_smart_fetch_request{
				.Context              = context,
				.Body                 = requestBody,
				.MaximumResponseBytes = maximum,
			},
			consumer, { });
		if (aScenario == "fetch") {
			expect(result.has_value(), "fetch-result");
			expect(consumer.Valid && consumer.Total == fetch_body_bytes,
				   "fetch-body");
			expect(consumer.Calls > 1U
					   && consumer.MaximumChunk <= 16U * 1024U,
				   "fetch-streaming");
			return;
		}
		expect(!result.has_value(), "fetch-failure");
		expect_failure(
			result.error(),
			aScenario == "consumer-reject"
				? application::git_smart_transport_failure::consumer_rejected
				: application::git_smart_transport_failure::response_limit,
			"fetch-failure-kind");
		if (aScenario == "response-limit") {
			expect(consumer.Total == 0U, "response-limit-before-consumer");
		}
	}
}

int main(int argc, char** argv)
{
	QCoreApplication application{ argc, argv };
	try {
		if (argc == 2 && std::string_view{ argv[1] } == "--contract") {
			run_contract();
		} else if (argc == 5 && std::string_view{ argv[1] } == "--fixture") {
			const auto portValue = std::stoul(argv[3]);
			expect(portValue > 0U && portValue <= 65'535U, "proxy-port");
			run_fixture(
				argv[2],
				static_cast<std::uint16_t>(portValue),
				argv[4]);
		} else {
			throw test_failure{ "arguments" };
		}
		std::cout << "OK\n";
		return EXIT_SUCCESS;
	} catch (const test_failure& failure) {
		std::cerr << "FAILED: " << failure.what() << '\n';
	} catch (const std::exception& failure) {
		std::cerr << "FAILED: exception: " << failure.what() << '\n';
	} catch (...) {
		std::cerr << "FAILED: unknown\n";
	}
	return EXIT_FAILURE;
}
