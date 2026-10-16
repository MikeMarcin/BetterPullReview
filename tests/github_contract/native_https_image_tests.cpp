// RFC-BPR-0084. Native HTTPS body-image fetches: the media binding, the GHES
// credential origins, core-admitted redirects and the fetch bounds.
//
// `--contract` checks what is decided before any socket exists. The fixture
// mode is driven by tests/scripts/verify_native_https_images.py, which owns a
// loopback CONNECT proxy, a disposable CA and the origins, and judges every
// request the transport made, including where an authorization header went.
#include "github_host_profile.hpp"
#include "github_read_contract.hpp"
#include "native_https_transport.hpp"

#include <QCoreApplication>
#include <QMessageLogContext>
#include <QString>
#include <QtLogging>

#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>

#if defined(_WIN32)
	#include <fcntl.h>
	#include <io.h>
#endif

namespace {
	using namespace bpr;
	using namespace bpr::adapters::github;
	using namespace bpr::adapters::github::detail;
	using namespace std::chrono_literals;

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

	void discard_qt_message(QtMsgType, const QMessageLogContext&, const QString&) { }

	constexpr auto attachment_uuid = std::string_view{ "25addea2-357a-42c0-a1c8-fba0163382cc" };

	[[nodiscard]] auto ghes_candidate(const application::github_authentication_transport aAuthentication) -> github_host_profile_candidate
	{
		return {
			.Profile        = core::host_profile_id{ 11 },
			.Host           = core::git_host_id{ 21 },
			.Revision       = application::github_profile_revision{ 31 },
			.Server         = application::github_server_contract::ghes_3_18_2022_11_28,
			.Authentication = aAuthentication,
			.Proxy          = application::github_proxy_mode::system,
			.Trust          = application::github_trust_mode::system,
			.Provenance     = github_profile_provenance::managed,
			.Hostname       = "ghe.bpr.invalid",
			.RestRoot       = "https://ghe.bpr.invalid/api/v3",
			.GraphqlRoot    = "https://ghe.bpr.invalid/api/graphql",
		};
	}

	[[nodiscard]] auto github_com_candidate(const application::github_authentication_transport aAuthentication) -> github_host_profile_candidate
	{
		auto candidate        = ghes_candidate(aAuthentication);
		candidate.Server      = application::github_server_contract::github_com_2022_11_28;
		candidate.Hostname    = "github.com";
		candidate.RestRoot    = "https://api.github.com";
		candidate.GraphqlRoot = "https://api.github.com/graphql";
		return candidate;
	}

	[[nodiscard]] auto credential_material(const std::string_view aValue) -> application::github_credential_material
	{
		const auto characters = std::span<const char>{ aValue.data(), aValue.size() };
		auto       material   = application::github_credential_material::create(std::as_bytes(characters));
		expect(material.has_value(), "credential-material");
		return std::move(*material);
	}

	[[nodiscard]] auto account_credential(const validated_github_host_profile& aProfile, const std::string_view aToken)
		-> application::github_credential_retrieve_success
	{
		return {
			.Context = {
				.Request            = core::request_id{ 301 },
				.ApplicationSession = core::application_session_id{ 202 },
				.Authority          = {
					.Profile         = aProfile.profile(),
					.ProfileRevision = aProfile.revision(),
					.Host            = aProfile.host(),
					.Hostname        = application::github_canonical_hostname{ aProfile.hostname() },
					.Account         = core::github_account_id{ 203 },
					.ServerContract  = aProfile.server_contract(),
				},
				.IdentityGeneration = core::github_identity_generation{ 207 },
			},
			.Credential = credential_material(aToken),
		};
	}

	[[nodiscard]] auto policy_for(const validated_github_host_profile& aProfile) -> core::markdown_image_fetch_policy
	{
		return {
			.Contract            = aProfile.server_contract() == application::github_server_contract::github_com_2022_11_28 ? core::markdown_image_host_contract::github_com : core::markdown_image_host_contract::ghes,
			.Hostname            = std::string{ aProfile.hostname() },
			.CredentialAvailable = true,
		};
	}

	[[nodiscard]] auto request_for(const validated_github_host_profile& aProfile, const std::string_view aAddress) -> application::markdown_image_fetch_request
	{
		auto request     = application::markdown_image_fetch_request{ };
		request.Policy   = policy_for(aProfile);
		const auto first = core::admit_markdown_image_fetch(aAddress, request.Policy);
		expect(first.has_value(), "first-hop-admitted");
		request.First = *first;
		return request;
	}

	[[nodiscard]] auto state_name(const application::markdown_image_fetch_result& aResult) -> std::string_view
	{
		if (aResult) {
			return "fetched";
		}
		switch (aResult.error()) {
		case core::markdown_image_state::failed:      return "failed";
		case core::markdown_image_state::too_large:   return "too_large";
		case core::markdown_image_state::blocked:     return "blocked";
		case core::markdown_image_state::unsupported: return "unsupported";
		case core::markdown_image_state::loading:
		case core::markdown_image_state::loaded:
		case core::markdown_image_state::over_limit:  return "unexpected";
		}
		return "unexpected";
	}

	[[nodiscard]] auto unreachable_configuration() -> native_https_test_configuration
	{
		// A proxy port nothing listens on: a request that reached the wire
		// would fail as a network error, never as a policy refusal.
		return { .ProxyHost = "127.0.0.1", .ProxyPort = 9U, .FallbackProxyPort = 0U, .DirectFirstProxyCandidate = false, .TrustedCaCertificatePem = { }, .TotalTimeout = 2s, .ResponseBodyLimit = native_https_maximum_response_body_bytes };
	}

	void test_contract()
	{
		const auto dotcom = validate_github_host_profile(github_com_candidate(application::github_authentication_transport::native_https));
		const auto cli    = validate_github_host_profile(github_com_candidate(application::github_authentication_transport::github_cli));
		const auto ghes   = validate_github_host_profile(ghes_candidate(application::github_authentication_transport::native_https));
		expect(dotcom.has_value() && cli.has_value() && ghes.has_value(), "profiles");

		auto media = make_native_https_media_transport_for_test(*dotcom, unreachable_configuration());
		expect(media.has_value(), "media-transport");
		expect(make_native_https_media_transport(*cli).has_value(), "a CLI profile has a media transport");

		// The media binding answers no GitHub read.
		auto semantic              = semantic_read_request{ };
		semantic.Server            = server_contract::github_com;
		semantic.Operation         = read_operation::get_pull_request;
		semantic.Owner             = "octo-org";
		semantic.Repository        = "review-repository";
		semantic.PullRequestNumber = 17U;
		const auto wire            = compile_read_request(semantic);
		expect(wire.has_value(), "wire");
		const auto  read    = (*media)->perform({ .Correlation = { .Parent = core::github_session_request_context{ .Request = core::request_id{ 1 }, .ApplicationSession = core::application_session_id{ 2 }, .Profile = dotcom->profile(), .IdentityGeneration = core::github_identity_generation{ 3 } }, .ProfileRevision = dotcom->revision(), .Host = dotcom->host(), .Operation = read_operation::get_pull_request, .Page = 1U, .AuthenticationSession = std::nullopt }, .Wire = *wire },
												std::stop_token{ });
		const auto* refused = std::get_if<github_wire_error>(&read);
		expect(refused != nullptr && refused->Error.Failure == application::github_provider_failure::unavailable, "the media binding refuses a GitHub read before the wire");

		// A first hop the core would refuse never reaches the wire.
		auto forged          = request_for(*dotcom, "https://camo.githubusercontent.com/a");
		forged.First.Address = "https://evil.bpr.invalid/a.png";
		expect(state_name((*media)->perform_markdown_image(forged, { })) == "blocked", "a forged first hop is refused before the wire");
		forged               = request_for(*dotcom, "https://camo.githubusercontent.com/a");
		forged.First.Address = "http://camo.githubusercontent.com/a";
		expect(state_name((*media)->perform_markdown_image(forged, { })) == "blocked", "plain HTTP is refused before the wire");
		forged                  = request_for(*dotcom, "https://camo.githubusercontent.com/a");
		forged.First.Credential = true;
		expect(state_name((*media)->perform_markdown_image(forged, { })) == "blocked", "a credential mark the core did not give is refused");
		forged              = request_for(*dotcom, "https://camo.githubusercontent.com/a");
		forged.MaximumBytes = core::markdown_image_encoded_bytes + 1U;
		expect(state_name((*media)->perform_markdown_image(forged, { })) == "blocked", "a byte bound above the core's is refused");

		// An account binding fetches images only for its own GHES host.
		auto dotcomAccount = make_native_https_account_transport_for_test(*dotcom, account_credential(*dotcom, "fixture-token"), unreachable_configuration());
		expect(dotcomAccount.has_value(), "dotcom-account");
		expect(state_name((*dotcomAccount)->perform_markdown_image(request_for(*dotcom, "https://camo.githubusercontent.com/a"), { })) == "failed", "a GitHub.com account binding runs no image fetch");
		auto ghesAccount = make_native_https_account_transport_for_test(*ghes, account_credential(*ghes, "fixture-token"), unreachable_configuration());
		expect(ghesAccount.has_value(), "ghes-account");
		auto elsewhere            = request_for(*ghes, "https://ghe.bpr.invalid/a.png");
		elsewhere.Policy.Hostname = "other.bpr.invalid";
		expect(state_name((*ghesAccount)->perform_markdown_image(elsewhere, { })) == "blocked", "another host's policy is refused");

		std::stop_source stopped;
		stopped.request_stop();
		expect(state_name((*media)->perform_markdown_image(request_for(*dotcom, "https://camo.githubusercontent.com/a"), stopped.get_token())) == "failed", "a stopped fetch sends nothing");
	}

	[[nodiscard]] auto read_frame() -> std::string
	{
		std::array<unsigned char, 4U> bytes{ };
		std::cin.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
		expect(std::cin.gcount() == static_cast<std::streamsize>(bytes.size()), "frame-length");
		const auto length = (static_cast<std::uint32_t>(bytes[0]) << 24U) | (static_cast<std::uint32_t>(bytes[1]) << 16U) | (static_cast<std::uint32_t>(bytes[2]) << 8U) | static_cast<std::uint32_t>(bytes[3]);
		expect(length <= 64U * 1024U, "frame-limit");
		std::string result(length, '\0');
		if (!result.empty()) {
			std::cin.read(result.data(), static_cast<std::streamsize>(result.size()));
			expect(std::cin.gcount() == static_cast<std::streamsize>(result.size()), "frame-body");
		}
		return result;
	}

	[[nodiscard]] auto parse_port(const std::string_view aValue) -> std::uint16_t
	{
		unsigned   value  = 0U;
		const auto parsed = std::from_chars(aValue.data(), aValue.data() + aValue.size(), value);
		expect(parsed.ec == std::errc{ } && parsed.ptr == aValue.data() + aValue.size() && value != 0U && value <= std::numeric_limits<std::uint16_t>::max(), "port");
		return static_cast<std::uint16_t>(value);
	}

	/// One scenario; prints the terminal state and the fetched bytes' length.
	void run_fixture(const std::string_view aScenario, const std::uint16_t aProxyPort)
	{
		auto       token         = read_frame();
		const auto authority     = read_frame();
		const auto configuration = native_https_test_configuration{ .ProxyHost = "127.0.0.1", .ProxyPort = aProxyPort, .FallbackProxyPort = 0U, .DirectFirstProxyCandidate = false, .TrustedCaCertificatePem = authority, .TotalTimeout = 10s, .ResponseBodyLimit = native_https_maximum_response_body_bytes };
		const auto ghesScenario  = aScenario.starts_with("ghes-");
		const auto profile       = validate_github_host_profile(ghesScenario ? ghes_candidate(application::github_authentication_transport::native_https)
																			 : github_com_candidate(application::github_authentication_transport::native_https));
		expect(profile.has_value(), "fixture-profile");
		const auto account   = aScenario == "ghes-credential" || aScenario == "ghes-credential-other";
		auto       transport = account ? make_native_https_account_transport_for_test(*profile, account_credential(*profile, token), configuration)
									   : make_native_https_media_transport_for_test(*profile, configuration);
		expect(transport.has_value(), "fixture-transport");
		const auto first   = aScenario == "dotcom-attachment" ? "https://github.com/user-attachments/assets/" + std::string{ attachment_uuid }
			: aScenario == "ghes-credential"                  ? "https://ghe.bpr.invalid/user-attachments/assets/" + std::string{ attachment_uuid }
			: aScenario == "ghes-credential-other"            ? std::string{ "https://storage.bpr.invalid/first.png" }
			: aScenario == "ghes-media-binding"               ? "https://ghe.bpr.invalid/user-attachments/assets/" + std::string{ attachment_uuid }
															  : std::string{ "https://camo.githubusercontent.com/fixture/image" };
		auto       request = request_for(*profile, first);
		if (aScenario == "dotcom-too-large") {
			request.MaximumBytes = 1'024U;
		}
		if (aScenario == "dotcom-timeout") {
			request.DeadlineMs = 1'000U;
		}
		const auto result = (*transport)->perform_markdown_image(request, { });
		std::cout << "native-https-image:" << aScenario << ':' << state_name(result) << ':' << (result ? result->Bytes.size() : 0U) << ':'
				  << (result ? result->ContentType : std::string{ }) << '\n';
	}
} // namespace

auto main(int argument_count, char** arguments) -> int
{
	try {
#if defined(_WIN32)
		expect(_setmode(_fileno(stdin), _O_BINARY) != -1, "binary-stdin");
#endif
		qInstallMessageHandler(discard_qt_message);
		QCoreApplication application{ argument_count, arguments };
		static_cast<void>(application);
		if (argument_count == 2 && std::string_view{ arguments[1] } == "--contract") {
			test_contract();
			std::cout << "native-https-image:contract:pass\n";
			return EXIT_SUCCESS;
		}
		expect(argument_count == 5 && std::string_view{ arguments[1] } == "--fixture" && std::string_view{ arguments[3] } == "--proxy-port", "arguments");
		run_fixture(std::string_view{ arguments[2] }, parse_port(std::string_view{ arguments[4] }));
		return EXIT_SUCCESS;
	} catch (const test_failure& aFailure) {
		std::cerr << "native-https-image:fail:" << aFailure.what() << '\n';
		return EXIT_FAILURE;
	} catch (...) {
		std::cerr << "native-https-image:fail:internal\n";
		return EXIT_FAILURE;
	}
}
