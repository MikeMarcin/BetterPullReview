#include "github_device_authorization_contract.hpp"
#include "github_device_authorization_transport.hpp"
#include "github_host_profile.hpp"
#include "native_https_transport.hpp"

#include "bpr/application/device_authorization_application_host.hpp"
#include "bpr/application/manual_executor.hpp"

#include <QCoreApplication>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <expected>
#include <functional>
#include <iostream>
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

	constexpr std::string_view fixture_client_id      = "Iv23bprfixtureclient";
	constexpr std::string_view contract_device_code   = "bpr0278-contract-device-code";
	constexpr std::string_view device_code_url        = "https://github.com/login/device/code";
	constexpr std::string_view device_token_url       = "https://github.com/login/oauth/access_token";
	constexpr std::string_view verification_uri       = "https://github.com/login/device";
	constexpr std::uint32_t    device_code_expires_in = 899U;
	constexpr std::uint32_t    device_code_interval   = 5U;
	constexpr std::uint32_t    token_expires_in       = 28'800U;
	constexpr std::uint32_t    refresh_expires_in     = 15'897'600U;
	constexpr std::uint32_t    slow_down_interval     = 10U;

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

	enum class device_course : std::uint8_t {
		success,
		disabled,
		slow_down,
		untrusted_certificate,
		cross_redirect,
		query_redirect,
		response_limit,
		cancel,
		// BPR-0450. The documented renewal refusal, and the one course that
		// drives the real application host through the real production adapter
		// rather than the transport alone.
		bad_refresh_token,
		lifecycle,
	};

	struct fixture_selection
	{
		device_authorization_endpoint Endpoint
			= device_authorization_endpoint::device_code;
		device_course Course = device_course::success;
		// A renewal posts to the token route, so the endpoint alone cannot say
		// which exchange this is.
		bool Refresh = false;
	};

	struct fixture_material
	{
		std::string DeviceCode;
		std::string UserCode;
		std::string AccessToken;
		std::string RefreshToken;
	};

	void select_binary_standard_streams()
	{
#if defined(_WIN32)
		expect(_setmode(_fileno(stdin), _O_BINARY) != -1, "binary-stdin");
		expect(_setmode(_fileno(stdout), _O_BINARY) != -1, "binary-stdout");
		expect(_setmode(_fileno(stderr), _O_BINARY) != -1, "binary-stderr");
#endif
	}

	[[nodiscard]] auto parse_course(const std::string_view aValue)
		-> std::optional<device_course>
	{
		if (aValue == "success") {
			return device_course::success;
		}
		if (aValue == "disabled") {
			return device_course::disabled;
		}
		if (aValue == "slow-down") {
			return device_course::slow_down;
		}
		if (aValue == "untrusted-certificate") {
			return device_course::untrusted_certificate;
		}
		if (aValue == "cross-redirect") {
			return device_course::cross_redirect;
		}
		if (aValue == "query-redirect") {
			return device_course::query_redirect;
		}
		if (aValue == "response-limit") {
			return device_course::response_limit;
		}
		if (aValue == "cancel") {
			return device_course::cancel;
		}
		if (aValue == "bad-refresh-token") {
			return device_course::bad_refresh_token;
		}
		if (aValue == "lifecycle") {
			return device_course::lifecycle;
		}
		return std::nullopt;
	}

	[[nodiscard]] auto parse_scenario(const std::string_view aValue)
		-> std::optional<fixture_selection>
	{
		if (aValue.starts_with("device-code-")) {
			const auto course = parse_course(
				aValue.substr(std::string_view{ "device-code-" }.size()));
			if (!course.has_value()) {
				return std::nullopt;
			}
			return fixture_selection{
				.Endpoint = device_authorization_endpoint::device_code,
				.Course   = *course,
			};
		}
		if (aValue.starts_with("refresh-")) {
			const auto course = parse_course(
				aValue.substr(std::string_view{ "refresh-" }.size()));
			if (!course.has_value()) {
				return std::nullopt;
			}
			return fixture_selection{
				.Endpoint = device_authorization_endpoint::token,
				.Course   = *course,
				.Refresh  = true,
			};
		}
		if (aValue.starts_with("token-")) {
			const auto course = parse_course(
				aValue.substr(std::string_view{ "token-" }.size()));
			if (!course.has_value()) {
				return std::nullopt;
			}
			return fixture_selection{
				.Endpoint = device_authorization_endpoint::token,
				.Course   = *course,
			};
		}
		return std::nullopt;
	}

	[[nodiscard]] auto profile_candidate(const bool aGithubCom)
		-> github_host_profile_candidate
	{
		return {
			.Profile        = core::host_profile_id{ 11U },
			.Host           = core::git_host_id{ 21U },
			.Revision       = application::github_profile_revision{ 31U },
			.Server         = aGithubCom
				? application::github_server_contract::github_com_2022_11_28
				: application::github_server_contract::ghes_3_18_2022_11_28,
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

	[[nodiscard]] auto session_context(
		const validated_github_host_profile& aProfile)
		-> core::github_session_request_context
	{
		return {
			.Request            = core::request_id{ 101U },
			.ApplicationSession = core::application_session_id{ 102U },
			.Profile            = aProfile.profile(),
			.IdentityGeneration = core::github_identity_generation{ 103U },
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
			.IdentityGeneration = core::github_identity_generation{ 103U },
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

	[[nodiscard]] auto ascii_equal_fold(
		const std::string_view aLeft,
		const std::string_view aRight) noexcept -> bool
	{
		return std::ranges::equal(
			aLeft,
			aRight,
			[](const char aFirst, const char aSecond) noexcept {
				const auto fold = [](const char aValue) noexcept {
					return aValue >= 'A' && aValue <= 'Z'
						? static_cast<char>(aValue + ('a' - 'A'))
						: aValue;
				};
				return fold(aFirst) == fold(aSecond);
			});
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

	void wipe(std::string& aValue) noexcept
	{
		std::ranges::fill(aValue, '\0');
		aValue.clear();
	}

	[[nodiscard]] auto compiled_request(
		const fixture_selection& aSelection,
		const fixture_material&  aMaterial)
		-> device_authorization_wire_request
	{
		auto compiled = aSelection.Refresh
			? compile_device_refresh_request(
				  server_contract::github_com,
				  fixture_client_id,
				  aMaterial.RefreshToken)
			: (aSelection.Endpoint
					   == device_authorization_endpoint::device_code
				   ? compile_device_code_request(
						 server_contract::github_com, fixture_client_id)
				   : compile_device_token_request(
						 server_contract::github_com,
						 fixture_client_id,
						 aMaterial.DeviceCode));
		expect(compiled.has_value(), "compiled-request");
		return std::move(*compiled);
	}

	void expect_failure(
		const native_https_device_authorization_completion& aCompletion,
		const application::github_provider_failure          aFailure,
		const std::string_view                              aMessage)
	{
		const auto* error = std::get_if<native_https_device_authorization_error>(
			&aCompletion);
		expect(error != nullptr && error->Error.Failure == aFailure, aMessage);
	}

	[[nodiscard]] auto refused(
		native_https_transport&                  aTransport,
		const device_authorization_wire_request& aRequest) -> bool
	{
		const auto completion = aTransport.perform_device_authorization(
			aRequest, std::stop_token{ });
		const auto* error = std::get_if<native_https_device_authorization_error>(
			&completion);
		return error != nullptr
			&& error->Error.Failure
			== application::github_provider_failure::unavailable;
	}

	void test_compiled_requests_carry_no_credential_header()
	{
		const auto code = compile_device_code_request(
			server_contract::github_com, fixture_client_id);
		const auto token = compile_device_token_request(
			server_contract::github_com, fixture_client_id, contract_device_code);
		expect(code.has_value() && token.has_value(), "contract-compiled");
		expect(code->Url == device_code_url && token->Url == device_token_url,
			   "contract-endpoint-urls");
		for (const auto& request : { *code, *token }) {
			expect(request.Method == http_method::post, "contract-method");
			for (const auto& header : request.Headers) {
				expect(!ascii_equal_fold(header.Name, "Authorization")
						   && !ascii_equal_fold(header.Name, "Proxy-Authorization")
						   && !ascii_equal_fold(header.Name, "Cookie"),
					   "contract-credential-header");
			}
		}
	}

	void test_profile_and_path_refusals(
		const validated_github_host_profile&   aGithubCom,
		const validated_github_host_profile&   aGhes,
		const native_https_test_configuration& aConfiguration)
	{
		const auto code = compile_device_code_request(
			server_contract::github_com, fixture_client_id);
		const auto token = compile_device_token_request(
			server_contract::github_com, fixture_client_id, contract_device_code);
		expect(code.has_value() && token.has_value(), "refusal-compiled");

		auto ghes = make_native_https_credential_free_transport_for_test(
			aGhes, session_context(aGhes), aConfiguration);
		expect(ghes.has_value(), "ghes-credential-free-transport");
		expect(refused(**ghes, *code) && refused(**ghes, *token),
			   "ghes-profile-refused");

		auto transport = make_native_https_credential_free_transport_for_test(
			aGithubCom, session_context(aGithubCom), aConfiguration);
		expect(transport.has_value(), "credential-free-transport");

		const auto offPath = std::array<std::string_view, 7U>{
			"https://github.com/login/device/code/extra",
			"https://github.com/login/device",
			"https://github.com/login/device/code?client_id=x",
			"https://github.com/login/device/code#fragment",
			"http://github.com/login/device/code",
			"https://github.com.bpr.invalid/login/device/code",
			"https://user@github.com/login/device/code",
		};
		for (const auto& url : offPath) {
			auto drifted = *code;
			drifted.Url  = std::string{ url };
			expect(refused(**transport, drifted), "off-path-url-refused");
		}

		auto swappedEndpoint     = *code;
		swappedEndpoint.Endpoint = device_authorization_endpoint::token;
		expect(refused(**transport, swappedEndpoint), "endpoint-url-pairing");

		auto swappedUrl = *token;
		swappedUrl.Url  = std::string{ device_code_url };
		expect(refused(**transport, swappedUrl), "url-endpoint-pairing");

		auto wrongMethod   = *code;
		wrongMethod.Method = http_method::get;
		expect(refused(**transport, wrongMethod), "method-refused");

		auto emptyBody = *code;
		emptyBody.Body.clear();
		expect(refused(**transport, emptyBody), "empty-body-refused");

		auto unboundedHeaders = *code;
		unboundedHeaders.Headers.assign(
			9U, http_header{ .Name = "X-Bpr-Fixture", .Value = "1" });
		expect(refused(**transport, unboundedHeaders), "header-count-refused");
	}

	void test_only_a_credential_free_binding_reaches_the_wire(
		const validated_github_host_profile&   aProfile,
		const native_https_test_configuration& aConfiguration)
	{
		const auto code = compile_device_code_request(
			server_contract::github_com, fixture_client_id);
		expect(code.has_value(), "binding-compiled");

		auto account = make_native_https_account_transport_for_test(
			aProfile,
			application::github_credential_retrieve_success{
				.Context    = credential_context(aProfile),
				.Credential = credential_material("BPR0278ContractToken"),
			},
			aConfiguration);
		expect(account.has_value(), "account-transport");
		expect(refused(**account, *code), "account-binding-refused");

		auto bootstrap = make_native_https_bootstrap_transport_for_test(
			aProfile,
			session_context(aProfile),
			credential_material("BPR0278BootstrapToken"),
			aConfiguration);
		expect(bootstrap.has_value(), "bootstrap-transport");
		expect(refused(**bootstrap, *code), "bootstrap-binding-refused");
	}

	void test_pre_cancellation(
		const validated_github_host_profile&   aProfile,
		const native_https_test_configuration& aConfiguration)
	{
		const auto code = compile_device_code_request(
			server_contract::github_com, fixture_client_id);
		expect(code.has_value(), "pre-cancel-compiled");

		auto transport = make_native_https_credential_free_transport_for_test(
			aProfile, session_context(aProfile), aConfiguration);
		expect(transport.has_value(), "pre-cancel-transport");

		std::stop_source stop;
		stop.request_stop();
		const auto completion = (*transport)->perform_device_authorization(*code, stop.get_token());
		expect(
			std::holds_alternative<native_https_device_authorization_cancelled>(
				completion),
			"pre-cancel-completion");
	}

	void run_contract()
	{
		static_assert(
			maximum_device_authorization_response_bytes == 64U * 1024U);
		static_assert(native_https_total_timeout == 30s);
		static_assert(!std::is_copy_constructible_v<native_https_transport>);

		const auto githubCom = validate_github_host_profile(profile_candidate(true));
		expect(githubCom.has_value() && githubCom->hostname() == "github.com",
			   "github-com-profile");
		const auto ghes = validate_github_host_profile(profile_candidate(false));
		expect(ghes.has_value(), "ghes-profile");

		const auto configuration = native_https_test_configuration{
			.ProxyHost    = "127.0.0.1",
			.ProxyPort    = 1U,
			.TotalTimeout = 250ms,
		};

		test_compiled_requests_carry_no_credential_header();
		test_profile_and_path_refusals(*githubCom, *ghes, configuration);
		test_only_a_credential_free_binding_reaches_the_wire(
			*githubCom, configuration);
		test_pre_cancellation(*githubCom, configuration);
	}

	[[nodiscard]] auto perform_cancelled_mid_response(
		native_https_transport&                  aTransport,
		const device_authorization_wire_request& aRequest)
		-> native_https_device_authorization_completion
	{
		std::stop_source                             stop;
		native_https_device_authorization_completion completion
			= native_https_device_authorization_error{ };
		{
			std::jthread worker{ [&] {
				completion = aTransport.perform_device_authorization(
					aRequest, stop.get_token());
			} };
			std::this_thread::sleep_for(1s);
			stop.request_stop();
		}
		return completion;
	}

	void expect_device_code_grant(
		const wire_response&    aResponse,
		const fixture_material& aMaterial)
	{
		const auto outcome = parse_device_code_response(aResponse);
		expect(outcome.has_value(), "device-code-parse");
		const auto* grant = std::get_if<device_code_grant>(&*outcome);
		expect(grant != nullptr && grant->DeviceCode == aMaterial.DeviceCode
				   && grant->UserCode == aMaterial.UserCode
				   && grant->VerificationUri == verification_uri
				   && grant->ExpiresInSeconds == device_code_expires_in
				   && grant->IntervalSeconds == device_code_interval,
			   "device-code-grant");
	}

	void expect_token_grant(
		const wire_response&    aResponse,
		const fixture_material& aMaterial)
	{
		const auto outcome = parse_device_token_response(aResponse);
		expect(outcome.has_value(), "token-parse");
		const auto* grant = std::get_if<device_token_grant>(&*outcome);
		expect(grant != nullptr && grant->AccessToken == aMaterial.AccessToken
				   && grant->RefreshToken == aMaterial.RefreshToken
				   && grant->ExpiresInSeconds == token_expires_in
				   && grant->RefreshTokenExpiresInSeconds == refresh_expires_in,
			   "token-grant");
	}

	// The renewal answer is the token answer, and this proves it on the wire
	// rather than trusting the shared parser to have stayed shared.
	void expect_refresh_grant(
		const wire_response&    aResponse,
		const fixture_material& aMaterial)
	{
		const auto outcome = parse_device_refresh_response(aResponse);
		expect(outcome.has_value(), "refresh-parse");
		const auto* grant = std::get_if<device_token_grant>(&*outcome);
		expect(grant != nullptr && grant->AccessToken == aMaterial.AccessToken
				   && grant->RefreshToken == aMaterial.RefreshToken
				   && grant->ExpiresInSeconds == token_expires_in
				   && grant->RefreshTokenExpiresInSeconds == refresh_expires_in,
			   "refresh-grant");
	}

	void expect_course(
		const native_https_device_authorization_completion& aCompletion,
		const fixture_selection&                            aSelection,
		const fixture_material&                             aMaterial)
	{
		switch (aSelection.Course) {
		case device_course::untrusted_certificate:
			expect_failure(aCompletion,
						   application::github_provider_failure::certificate,
						   "fixture-certificate-refusal");
			return;
		case device_course::cross_redirect:
		case device_course::query_redirect:
			expect_failure(aCompletion,
						   application::github_provider_failure::redirect,
						   "fixture-redirect-refusal");
			return;
		case device_course::response_limit:
			expect_failure(aCompletion,
						   application::github_provider_failure::response_limit,
						   "fixture-response-limit-refusal");
			return;
		case device_course::cancel:
			expect(
				std::holds_alternative<
					native_https_device_authorization_cancelled>(aCompletion),
				"fixture-cancellation");
			return;
		case device_course::success:
		case device_course::disabled:
		case device_course::slow_down:
		case device_course::bad_refresh_token:
			break;
		case device_course::lifecycle:
			expect(false, "lifecycle-course-is-not-a-wire-course");
			return;
		}

		const auto* success = std::get_if<native_https_device_authorization_success>(
			&aCompletion);
		expect(success != nullptr, "fixture-success-completion");
		const auto expectedStatus = aSelection.Course == device_course::disabled
			? 400
			: 200;
		expect(success->Response.Status == expectedStatus, "fixture-status");

		if (aSelection.Course == device_course::disabled) {
			const auto outcome = parse_device_code_response(success->Response);
			expect(outcome.has_value(), "device-flow-disabled-parse");
			const auto* refusal = std::get_if<device_code_refusal>(&*outcome);
			expect(refusal != nullptr
					   && refusal->Kind
						   == device_code_refusal_kind::device_flow_disabled,
				   "device-flow-disabled-outcome");
			return;
		}
		if (aSelection.Course == device_course::slow_down) {
			const auto outcome = parse_device_token_response(success->Response);
			expect(outcome.has_value(), "slow-down-parse");
			const auto* delay = std::get_if<device_token_delay>(&*outcome);
			expect(delay != nullptr
					   && delay->Kind == device_token_delay_kind::slow_down
					   && delay->SlowDownIntervalSeconds == slow_down_interval,
				   "slow-down-outcome");
			return;
		}
		if (aSelection.Course == device_course::bad_refresh_token) {
			const auto outcome = parse_device_refresh_response(success->Response);
			expect(outcome.has_value(), "bad-refresh-token-parse");
			const auto* refusal = std::get_if<device_refresh_refusal>(&*outcome);
			expect(refusal != nullptr
					   && refusal->Kind
						   == device_refresh_refusal_kind::bad_refresh_token,
				   "bad-refresh-token-outcome");
			return;
		}
		if (aSelection.Refresh) {
			expect_refresh_grant(success->Response, aMaterial);
			return;
		}
		if (aSelection.Endpoint == device_authorization_endpoint::device_code) {
			expect_device_code_grant(success->Response, aMaterial);
			return;
		}
		expect_token_grant(success->Response, aMaterial);
	}

	// -- BPR-0450 renewal lifecycle ---------------------------------------

	// One slot, held in memory. The point of this vault is that the host reads
	// renewal material out of it and writes rotated material back into it over
	// a real socket, not that any platform store behaves a particular way.
	class lifecycle_vault final : public application::github_credential_vault_port
	{
	public:
		explicit lifecycle_vault(std::string aRefresh)
			: Refresh{ std::move(aRefresh) }
		{
		}

		void store(
			application::github_credential_enrollment               aEnrollment,
			application::github_credential_store_completion_handler aCompletion)
			override
		{
			Access = text_of(aEnrollment.Credential);
			aCompletion(application::github_credential_store_success{
				.Context = aEnrollment.Context,
			});
		}

		void retrieve(
			const application::github_credential_operation_context     aContext,
			application::github_credential_retrieve_completion_handler aCompletion)
			override
		{
			answer(aContext, Access, std::move(aCompletion));
		}

		void delete_credential(
			const application::github_credential_operation_context   aContext,
			application::github_credential_delete_completion_handler aCompletion)
			override
		{
			Access.clear();
			aCompletion(application::github_credential_delete_success{
				.Context = aContext,
			});
		}

		void store_refresh(
			application::github_credential_enrollment               aEnrollment,
			application::github_credential_store_completion_handler aCompletion)
			override
		{
			Refresh = text_of(aEnrollment.Credential);
			++Rotations;
			aCompletion(application::github_credential_store_success{
				.Context = aEnrollment.Context,
			});
		}

		void retrieve_refresh(
			const application::github_credential_operation_context     aContext,
			application::github_credential_retrieve_completion_handler aCompletion)
			override
		{
			answer(aContext, Refresh, std::move(aCompletion));
		}

		void delete_refresh(
			const application::github_credential_operation_context   aContext,
			application::github_credential_delete_completion_handler aCompletion)
			override
		{
			Refresh.clear();
			aCompletion(application::github_credential_delete_success{
				.Context = aContext,
			});
		}

		void cancel(application::github_credential_operation_context) override
		{
		}

		std::string Access;
		std::string Refresh;
		std::size_t Rotations = 0U;

	private:
		[[nodiscard]] static auto text_of(
			const application::github_credential_material& aMaterial)
			-> std::string
		{
			const auto bytes = aMaterial.bytes();
			auto       text  = std::string{ };
			text.resize(bytes.size());
			std::ranges::transform(
				bytes,
				text.begin(),
				[](const std::byte aByte) {
					return static_cast<char>(aByte);
				});
			return text;
		}

		static void answer(
			const application::github_credential_operation_context     aContext,
			const std::string&                                         aValue,
			application::github_credential_retrieve_completion_handler aCompletion)
		{
			if (aValue.empty()) {
				aCompletion(application::github_credential_retrieve_absent{
					.Context = aContext,
				});
				return;
			}
			auto bytes = std::vector<std::byte>{ };
			bytes.resize(aValue.size());
			std::ranges::transform(
				aValue,
				bytes.begin(),
				[](const char aCharacter) {
					return static_cast<std::byte>(aCharacter);
				});
			auto material
				= application::github_credential_material::create(bytes);
			expect(material.has_value(), "lifecycle-vault-material");
			aCompletion(application::github_credential_retrieve_success{
				.Context    = aContext,
				.Credential = std::move(*material),
			});
		}
	};

	class lifecycle_clock final
		: public application::device_authorization_clock_port
	{
	public:
		[[nodiscard]] auto observe_monotonic() noexcept
			-> std::chrono::milliseconds override
		{
			return Now;
		}

		std::chrono::milliseconds Now{ 0 };
	};

	class lifecycle_timer final
		: public application::device_authorization_timer_port
	{
	public:
		[[nodiscard]] auto schedule_after(std::uint32_t, std::function<void()>)
			-> bool override
		{
			return true;
		}

		void cancel() noexcept override
		{
		}
	};

	class lifecycle_enrollment final
		: public application::device_authorization_enrollment_port
	{
	public:
		void enroll(
			core::host_profile_id,
			application::github_credential_material aCredential) override
		{
			++Enrollments;
			const auto bytes = aCredential.bytes();
			Credential.resize(bytes.size());
			std::ranges::transform(
				bytes,
				Credential.begin(),
				[](const std::byte aByte) {
					return static_cast<char>(aByte);
				});
		}

		std::size_t Enrollments = 0U;
		std::string Credential;
	};

	class lifecycle_projections final
		: public application::device_authorization_projection_port
	{
	public:
		void publish(
			const application::device_authorization_presentation&) override
		{
		}
	};

	[[nodiscard]] auto lifecycle_authority()
		-> application::github_credential_authority_key
	{
		return {
			.Profile         = core::host_profile_id{ 11U },
			.ProfileRevision = application::github_profile_revision{ 31U },
			.Host            = core::git_host_id{ 21U },
			.Hostname        = application::github_canonical_hostname{ "github.com" },
			.Account         = core::github_account_id{ 41U },
			.ServerContract  = application::github_server_contract::
				github_com_2022_11_28,
		};
	}

	// The card's own requirement: the host's refresh lifecycle running against
	// the production adapter, over the real transport, on a real socket.
	// Exactly one exchange leaves the process, which is what lets the loopback
	// fixture assert a single connection.
	void run_refresh_lifecycle(
		const std::string&      aAuthority,
		const fixture_material& aMaterial,
		const std::uint16_t     aProxyPort)
	{
		const auto profile = validate_github_host_profile(profile_candidate(true));
		expect(profile.has_value(), "lifecycle-profile");

		const auto configuration = native_https_test_configuration{
			.ProxyHost               = "127.0.0.1",
			.ProxyPort               = aProxyPort,
			.TrustedCaCertificatePem = aAuthority,
			.TotalTimeout            = 5s,
			.ResponseBodyLimit       = native_https_maximum_response_body_bytes,
		};
		auto wire = make_native_https_credential_free_transport_for_test(
			*profile, session_context(*profile), configuration);
		expect(wire.has_value(), "lifecycle-transport");

		application::manual_executor lane;
		application::manual_executor effects;
		auto                         adapter
			= make_github_device_authorization_transport(
				std::shared_ptr<native_https_transport>{ std::move(*wire) },
				std::string{ fixture_client_id },
				effects);
		expect(adapter.has_value(), "lifecycle-adapter");

		lifecycle_vault       vault{ aMaterial.RefreshToken };
		lifecycle_clock       clock;
		lifecycle_timer       timer;
		lifecycle_enrollment  enrollment;
		lifecycle_projections projections;
		auto                  host
			= application::device_authorization_application_host{
				  core::device_authorization_state{
					  .ApplicationSession = core::application_session_id{ 102U },
				  },
				  **adapter,
				  vault,
				  enrollment,
				  clock,
				  timer,
				  lane,
				  projections,
			  };

		host.resume_refresh(lifecycle_authority());
		// The vault answers inline, the adapter's exchange runs when the effect
		// lane is drained, and the terminal comes back through the decision
		// lane. Draining both until neither has work is the host reaching
		// quiescence.
		for (auto turn = 0; turn < 32 && (lane.pending() > 0U || effects.pending() > 0U); ++turn) {
			lane.drain();
			effects.drain();
		}

		expect(
			vault.Rotations == 1U,
			"the renewal rotated the refresh slot exactly once");
		expect(
			vault.Refresh == aMaterial.RefreshToken + "-rotated",
			"storing the rotated material the server returned, not the one it "
			"was sent");
		expect(
			enrollment.Enrollments == 1U
				&& enrollment.Credential == aMaterial.AccessToken,
			"and the renewed access token crossed the enrollment boundary once");
		expect(
			host.refresh_observation().Rotations == 1U
				&& host.refresh_observation().Failure
					== application::device_authorization_refresh_failure::none,
			"the host observed one clean rotation");
		expect(
			!host.holds_pending_refresh_material(),
			"and retained nothing once the vault had it");

		host.shutdown();
		lane.drain();
	}

	void run_fixture(
		const fixture_selection& aSelection,
		const std::uint16_t      aProxyPort)
	{
		auto authority = read_frame(native_https_maximum_response_header_bytes);
		auto material  = fixture_material{
			.DeviceCode   = read_frame(maximum_device_transient_code_bytes),
			.UserCode     = read_frame(maximum_device_transient_code_bytes),
			.AccessToken  = read_frame(maximum_device_transient_code_bytes),
			.RefreshToken = read_frame(maximum_device_transient_code_bytes),
		};
		expect(!authority.empty(), "fixture-authority");
		expect(!material.DeviceCode.empty() && !material.UserCode.empty()
				   && !material.AccessToken.empty()
				   && !material.RefreshToken.empty(),
			   "fixture-material");

		if (aSelection.Course == device_course::lifecycle) {
			run_refresh_lifecycle(authority, material, aProxyPort);
			wipe(material.DeviceCode);
			wipe(material.UserCode);
			wipe(material.AccessToken);
			wipe(material.RefreshToken);
			return;
		}

		const auto profile = validate_github_host_profile(profile_candidate(true));
		expect(profile.has_value(), "fixture-profile");

		const auto configuration = native_https_test_configuration{
			.ProxyHost               = "127.0.0.1",
			.ProxyPort               = aProxyPort,
			.TrustedCaCertificatePem = aSelection.Course
					== device_course::untrusted_certificate
				? std::string{ }
				: std::move(authority),
			.TotalTimeout            = 5s,
			.ResponseBodyLimit       = native_https_maximum_response_body_bytes,
		};

		auto transport = make_native_https_credential_free_transport_for_test(
			*profile, session_context(*profile), configuration);
		expect(transport.has_value(), "fixture-transport");

		const auto request    = compiled_request(aSelection, material);
		const auto completion = aSelection.Course == device_course::cancel
			? perform_cancelled_mid_response(**transport, request)
			: (*transport)->perform_device_authorization(request, std::stop_token{ });

		expect_course(completion, aSelection, material);

		wipe(material.DeviceCode);
		wipe(material.UserCode);
		wipe(material.AccessToken);
		wipe(material.RefreshToken);
	}
} // namespace

auto main(int argument_count, char* arguments[]) -> int
{
	std::string_view scenario;
	try {
		select_binary_standard_streams();
		QCoreApplication application{ argument_count, arguments };
		static_cast<void>(application);

		if (argument_count == 2
			&& std::string_view{ arguments[1] } == "--contract") {
			run_contract();
			std::cout << "native-https-device-authorization:contract:pass\n";
			return EXIT_SUCCESS;
		}

		expect(argument_count == 4
				   && std::string_view{ arguments[1] } == "--fixture",
			   "arguments");
		scenario             = std::string_view{ arguments[2] };
		const auto selection = parse_scenario(scenario);
		expect(selection.has_value(), "fixture-scenario");
		const auto port = std::stoul(arguments[3]);
		expect(port > 0UL && port <= 65'535UL, "fixture-proxy-port");
		run_fixture(*selection, static_cast<std::uint16_t>(port));
		std::cout << "native-https-device-authorization:" << scenario
				  << ":pass\n";
		return EXIT_SUCCESS;
	} catch (const test_failure& failure) {
		std::cerr << "native-https-device-authorization:fail:"
				  << failure.what() << '\n';
	} catch (...) {
		std::cerr << "native-https-device-authorization:fail:internal\n";
	}
	return EXIT_FAILURE;
}
