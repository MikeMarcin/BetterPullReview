#include "github_release_update.hpp"
#include "native_https_transport.hpp"

#include <QCoreApplication>
#include <QFile>

#include <chrono>
#include <cstdlib>
#include <iostream>

namespace {
	using namespace bpr;
	using namespace adapters::github::detail;
	using reason = core::release_update_reason;
	void expect(bool aValue, const char* aMessage)
	{
		if (!aValue) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	void contract()
	{
		const auto body   = R"({"id":10,"tag_name":"v2.3.4","draft":false,"prerelease":false,"body":"private <b>notes</b>","html_url":"https://evil.invalid","assets":[{"id":20,"name":"BetterPullReview-2.3.4-macos-universal.zip","size":100,"state":"uploaded","digest":null,"browser_download_url":"https://evil.invalid/token"}]})";
		const auto parsed = parse_release_update_metadata(body);
		expect(parsed.Failure == reason::none && parsed.Release.Assets.size() == 1U && parsed.Notes == "private <b>notes</b>", "bounded normalized metadata");
		expect(release_update_browser_address("v2.3.4").starts_with("https://github.com/MikeMarcin/BetterPullReview/releases/tag/"), "fixed browser authority");
		expect(release_update_browser_address("../token") == "https://github.com/MikeMarcin/BetterPullReview/releases", "untrusted tag is never a browser path");
		expect(parse_release_update_metadata("{\"id\":10}").Failure == reason::malformed_release, "missing required metadata refused");
		expect(parse_release_update_metadata(std::string(application::release_update_maximum_metadata_bytes + 1U, 'x')).Failure == reason::size_limit, "metadata bound");
		const std::string                                canary   = "fixture-release-secret-canary";
		auto                                             material = [&] { return application::github_credential_material::create(std::as_bytes(std::span{ canary.data(), canary.size() })); };
		application::github_credential_operation_context context{ core::request_id{ 1U }, core::application_session_id{ 1U }, { core::host_profile_id{ 1U }, application::github_profile_revision{ 1U }, core::git_host_id{ 1U }, application::github_canonical_hostname{ "github.com" }, core::github_account_id{ 1U } }, core::github_identity_generation{ 1U } };
		auto                                             lease = make_native_release_update_access({ context, std::move(*material()) }, core::release_update_access_id{ 7U }, { });
		expect(lease && lease->binding().value() == 7U, "accepted native authority creates release-only lease");
		lease->revoke();
		expect(lease->binding().value() == 0U && lease->latest({ }).Failure == reason::cancelled, "revocation refuses before network");
		context.Authority.ServerContract = application::github_server_contract::ghes_3_18_2022_11_28;
		context.Authority.Hostname       = application::github_canonical_hostname{ "ghe.example.com" };
		expect(!make_native_release_update_access({ context, std::move(*material()) }, core::release_update_access_id{ 8U }, { }), "enterprise authority cannot access release service");
		auto anonymous = make_public_release_update_access(core::release_update_access_id{ 9U }, { });
		expect(anonymous && anonymous->binding().value() == 9U && anonymous->kind() == core::release_update_access_kind::native_github_com, "public releases need no sign-in");
		anonymous->revoke();
		expect(anonymous->latest({ }).Failure == reason::cancelled, "public lease revocation refuses before network");
		expect(!make_public_release_update_access(core::release_update_access_id{ }, { }), "public lease needs an identity");
		std::cout << "release-update-transport-contract:pass\n";
	}
}
auto main(int aArgc, char** aArgv) -> int
{
	QCoreApplication application{ aArgc, aArgv };
	if (aArgc == 1) {
		contract();
		return 0;
	}
	expect(aArgc == 4, "wire fixture arguments");
	const std::string scenario{ aArgv[1] };
	const bool        timeout = scenario == "timeout" || scenario == "timeout-connect" || scenario == "timeout-handshake";
	QFile             ca{ QString::fromUtf8(aArgv[3]) };
	expect(ca.open(QIODevice::ReadOnly), "fixture authority readable");
	native_https_test_configuration configuration;
	configuration.ProxyHost               = "127.0.0.1";
	configuration.ProxyPort               = static_cast<std::uint16_t>(std::stoul(aArgv[2]));
	configuration.TrustedCaCertificatePem = scenario == "untrusted" ? std::string{ } : ca.readAll().toStdString();
	// Healthy TLS courses test wire classification, not a half-second scheduling
	// race on a loaded build host. Keep the timeout courses' short deadline;
	// the redirect-cap course gets an allowance for each of its four connections.
	configuration.TotalTimeout = timeout ? std::chrono::milliseconds{ 500 }
		: scenario == "redirect-loop"    ? std::chrono::milliseconds{ 8000 }
										 : std::chrono::milliseconds{ 2000 };
	std::string canary;
	std::getline(std::cin, canary);
	expect(canary.size() == 40U, "bounded fixture canary");
	std::stop_source stop;
	if (scenario == "pre-cancel") {
		stop.request_stop();
	}
	std::uint64_t received = 0;
	const bool    metadata = scenario == "metadata" || scenario == "metadata-redirect";
	const auto    started  = std::chrono::steady_clock::now();
	const auto    result   = perform_release_update_https_for_test(std::as_bytes(std::span{ canary.data(), canary.size() }), metadata ? 0U : 20U, 100U, stop.get_token(), [&](std::span<const std::byte> aBytes) {
		received += aBytes.size();
		if (scenario == "cancel") {
			stop.request_stop();
		}
		return scenario != "sink-refuses"; }, configuration);
	reason        expected = reason::none;
	if (scenario == "401") {
		expected = reason::unauthenticated;
	} else if (scenario == "403") {
		expected = reason::forbidden;
	} else if (scenario == "404") {
		expected = reason::inaccessible;
	} else if (scenario == "429") {
		expected = reason::rate_limited;
	} else if (scenario == "untrusted") {
		expected = reason::certificate;
	} else if (scenario == "cancel" || scenario == "pre-cancel") {
		expected = reason::cancelled;
	} else if (timeout) {
		expected = reason::timed_out;
	} else if (scenario == "size" || scenario == "stream-size" || scenario == "headers") {
		expected = reason::size_limit;
	} else if (scenario == "encoding" || scenario == "duplicate" || scenario == "type") {
		expected = reason::malformed_release;
	} else if (scenario == "sink-refuses") {
		expected = reason::unavailable;
	} else if (scenario.ends_with("redirect") || scenario == "redirect-loop") {
		expected = reason::redirect;
	}
	// The approved cross-origin redirect is the one successful redirect case.
	if (scenario == "asset-redirect") {
		expected = reason::none;
	}
	if (result.Failure != expected) {
		std::cerr << "reason=" << static_cast<int>(result.Failure) << " expected=" << static_cast<int>(expected) << '\n';
	}
	expect(result.Failure == expected, "wire terminal classification");
	expect(received <= 100U && result.Bytes <= 100U, "streaming bound held");
	if (timeout) {
		expect(std::chrono::steady_clock::now() - started >= configuration.TotalTimeout, "the total deadline elapses before a timeout terminal");
		expect(received == 0U && result.Bytes == 0U, "a stalled timeout delivers no response bytes");
	}
	if (scenario == "cancel") {
		expect(received == 1U && result.Bytes == 1U, "cancellation occurs after the first response byte");
	}
	if (expected == reason::none) {
		expect(received == (metadata ? 2U : 100U), "complete bounded payload");
	}
	if (scenario == "429") {
		expect(result.RetryAfterSeconds == 61U, "retry interval preserved");
	}
	std::cout << "release-update-wire:pass\n";
}
