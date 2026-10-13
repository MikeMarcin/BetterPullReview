#include "../support/test_expectation.hpp"
#include "gh_cli_transport.hpp"
#include "github_cli_process_runner.hpp"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>

namespace {

	using namespace bpr;
	using namespace bpr::adapters::github;
	using namespace bpr::adapters::github::detail;
	using bpr::tests::expect;

	/// \brief Exercises strict native credential ownership outside routine CTest.
	class audited_process final : public github_cli_process_port
	{
	public:
		std::unique_ptr<github_cli_process_port> Runner       = make_github_cli_process_runner();
		std::size_t                              Local        = 0U;
		std::size_t                              Acquisition  = 0U;
		std::size_t                              Credentialed = 0U;
		bool                                     WrongFamily  = false;
		std::filesystem::path                    FixtureCertificate;
		std::filesystem::path                    FixtureSocket;

		auto run(const github_cli_process_request& aRequest, std::stop_token aStop) -> github_cli_process_result override
		{
			++Local;
			return Runner->run(aRequest, aStop);
		}
		auto acquire_sensitive(const github_cli_process_request& aRequest, std::stop_token aStop, std::shared_ptr<const void> aOwner) -> cli_sensitive_result override
		{
			++Acquisition;
			return Runner->acquire_sensitive(aRequest, aStop, std::move(aOwner));
		}
		auto run_credentialed(const github_cli_process_request& aRequest, cli_transient_credential aCredential,
							  cli_credential_environment aEnvironment, std::stop_token aStop) -> github_cli_process_result override
		{
			++Credentialed;
			const auto secret = aCredential->view();
			expect(aRequest.StandardInput.find(secret) == std::string::npos, "native credential absent from generic input");
			for (const auto& argument : aRequest.Arguments) {
				expect(argument.find(secret) == std::string::npos, "native credential absent from arguments");
			}
			for (const auto& entry : aRequest.Environment) {
				expect(entry.Name != "GH_TOKEN" && entry.Name != "GH_ENTERPRISE_TOKEN"
						   && entry.Name != "GITHUB_TOKEN" && entry.Name != "GITHUB_ENTERPRISE_TOKEN"
						   && entry.Value.find(secret) == std::string::npos,
					   "generic environment is token free");
			}
			if (WrongFamily) {
				aEnvironment = aEnvironment == cli_credential_environment::github ? cli_credential_environment::enterprise : cli_credential_environment::github;
			}
			auto request = aRequest;
			if (!FixtureSocket.empty()) {
				// Explicit test-owned routing only. macOS production continues to
				// use system trust and never admits a supplied certificate file.
				for (const auto& entry : request.Environment) {
					if (entry.Name == "GH_CONFIG_DIR") {
						const auto config = std::filesystem::path{ entry.Value } / "config.yml";
						std::filesystem::permissions(config, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
						std::ofstream output{ config, std::ios::app };
						output << "http_unix_socket: " << FixtureSocket.string() << '\n';
						output.close();
						std::filesystem::permissions(config, std::filesystem::perms::owner_read);
					}
				}
				request.Arguments.insert(request.Arguments.end(), { "--config", "http.sslCAInfo=" + FixtureCertificate.string() });
			}
			auto result = Runner->run_credentialed(request, aCredential, aEnvironment, aStop);
			expect(!result || result->StandardOutput.find(secret) == std::string::npos, "retained output contains no credential");
			return result;
		}
	};

	template <typename TIdentity>
	auto opaque(std::uint8_t aSeed) -> TIdentity
	{
		std::array<std::uint8_t, 32U> bytes{ };
		bytes.front() = aSeed;
		bytes.back()  = aSeed;
		return TIdentity{ bytes };
	}

	auto context_for(core::git_commit_oid aCommit) -> application::git_smart_request_context
	{
		return {
			.ApplicationSession = core::application_session_id{ 202U },
			.Authority          = {
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
			.Revision = {
				.Source = {
					.Observation           = opaque<core::epoch_source_observation_id>(9U),
					.Snapshot              = core::provider_snapshot_id{ 301U },
					.IdentityGeneration    = core::github_identity_generation{ 207U },
					.PullRequestGeneration = core::request_generation{ 208U },
				},
				.Origin     = core::pull_request_revision_origin::live,
				.BaseBranch = opaque<core::base_branch_id>(4U),
				.Base       = aCommit,
				.Head       = aCommit,
			},
			.Repository = { .Owner = "MikeMarcin", .Name = "BPR-Test" },
			.BaseRef    = *application::parse_git_base_branch_ref("refs/heads/main"),
		};
	}

	auto scan(const std::filesystem::path& aRoot, std::string_view aSecret) -> void
	{
		for (const auto& entry : std::filesystem::recursive_directory_iterator{ aRoot }) {
			expect(!entry.is_symlink(), "native course creates no persistent symlink");
			if (!entry.is_regular_file()) {
				continue;
			}
			std::ifstream     input{ entry.path(), std::ios::binary };
			const std::string contents{ std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{ } };
			expect(contents.find(aSecret) == std::string::npos, "BPR-created configuration and clone store contain no credential");
		}
	}

	auto probe(const QStringList& aArguments) -> int
	{
		expect(aArguments.size() == 10, "native probe needs mode, canonical gh, home, host, login, proxy, CA, commit and expected login");
		const auto    mode = aArguments[1].toStdString();
		QTemporaryDir temporary{ QStringLiteral("/private/tmp/bpr-cli-git-XXXXXX") };
		expect(temporary.isValid(), "native probe owns its root");
		const std::filesystem::path root{ temporary.path().toStdString() };
		std::filesystem::create_directory(root / "neutral");
		std::filesystem::create_directory(root / "empty");
		const auto                     host  = aArguments[4].toStdString();
		const auto                     login = aArguments[5].toStdString();
		gh_cli_https_environment_facts https;
		if (aArguments[6] != "-") {
			https.Proxy = aArguments[6].toStdString();
		}
		const auto configuration = validate_gh_cli_configuration({
			.Executable              = aArguments[2].toStdString(),
			.NeutralWorkingDirectory = root / "neutral",
			.Platform                = gh_cli_macos_environment_facts{ .HomeDirectory = aArguments[3].toStdString() },
			.Https                   = https,
		});
		if (!configuration) {
			std::cerr << "native-configuration-category=" << static_cast<unsigned>(configuration.error()) << '\n';
		}
		expect(configuration.has_value(), "native configuration admitted");
		const auto pinned = pin_gh_cli_configuration(*configuration, host, login);
		expect(pinned.has_value(), "native account pin admitted");
		audited_process process;
		auto            acquired = acquire_gh_cli_credential(process, *pinned, { });
		if (mode == "missing") {
			expect(!acquired && acquired.error().CredentialUnavailable && process.Local == 5U
					   && process.Acquisition == 1U && process.Credentialed == 0U,
				   "named native refusal launches zero API or network Git operations");
			std::cout << "native-missing:pass local=5 acquisition=1 credentialed=0\n";
			return 0;
		}
		expect(acquired.has_value(), "stock gh named credential acquisition succeeds");
		const std::weak_ptr<const cli_sensitive_bytes> lifetime = *acquired;
		if (mode == "api") {
			auto request = compile_gh_cli_version_request(*pinned);
			request.Arguments.resize(3U);
			request.Arguments.insert(request.Arguments.end(), { "api", "user", "--hostname", host, "--include", "--method", "GET" });
			request.StandardOutputLimit = 64U * 1024U;
			const auto result           = run_gh_cli(process, *pinned, request, { }, *acquired);
			expect(result.has_value() && result->ExitCode == 0, "selected native API succeeds");
			const auto boundary = result->StandardOutput.find("\r\n\r\n");
			expect(boundary != std::string::npos, "native API carries response headers");
			const auto json = QJsonDocument::fromJson(QByteArray::fromStdString(result->StandardOutput.substr(boundary + 4U))).object();
			expect(json["login"].toString() == aArguments[9] && json["id"].toDouble() > 0, "exact selected-account native API identity");
			const auto headers  = result->StandardOutput.substr(0, boundary);
			const auto lowered  = QString::fromStdString(headers).toLower().toStdString();
			const auto position = lowered.find("x-ratelimit-remaining:");
			expect(position != std::string::npos, "native evidence includes actual rate remaining header");
			const auto remaining = headers.substr(position + 22U, headers.find('\n', position) - position - 22U);
			expect(std::stoi(remaining) > 100, "native course preserves shared rate margin");
			std::cout << "native-api:pass login=" << login << " account=" << json["id"].toInteger() << " rate_remaining=" << remaining << '\n';
		} else {
			expect(mode == "clone" || mode == "clone-negative", "native mode is closed");
			const auto commit = core::parse_git_commit_oid(aArguments[8].toStdString());
			expect(commit.has_value(), "native clone exact commit");
			github_host_profile_candidate candidate{
				.Profile         = core::host_profile_id{ 11U },
				.Host            = core::git_host_id{ 21U },
				.Revision        = application::github_profile_revision{ 31U },
				.Server          = application::github_server_contract::ghes_3_18_2022_11_28,
				.Authentication  = application::github_authentication_transport::github_cli,
				.Proxy           = application::github_proxy_mode::system,
				.Trust           = application::github_trust_mode::system,
				.Provenance      = github_profile_provenance::explicit_user,
				.Hostname        = host,
				.RestRoot        = "https://" + host + "/api/v3",
				.GraphqlRoot     = "https://" + host + "/api/graphql",
				.CliAccountLogin = login,
			};
			const auto profile = validate_github_host_profile(candidate);
			expect(profile.has_value(), "native synthetic clone profile");
			const auto request = compile_gh_cli_git_clone_request(*pinned, *profile, context_for(*commit), *commit, "/usr/bin/git", root, 0U);
			expect(request.has_value(), "production exact clone compiler admits native course");
			process.WrongFamily        = mode == "clone-negative";
			process.FixtureCertificate = aArguments[7].toStdString();
			process.FixtureSocket      = aArguments[9].toStdString();
			std::cout << "native-acquired\n"
					  << std::flush;
			expect(std::cin.get() == '\n', "test-owned named item removed before helper use");
			const auto result = run_gh_cli(process, *pinned, *request, { }, *acquired);
			expect(result.has_value() && (result->ExitCode == 0) == !process.WrongFamily, "official gh clone and helper positive/negative are authentication dependent");
			std::cout << "native-" << mode << ":pass\n";
		}
		scan(root, (*acquired)->view());
		acquired->reset();
		process.Runner.reset();
		expect(lifetime.expired(), "native operation releases sensitive owner after runner drains");
		std::cout << "native-transience:pass\n";
		return 0;
	}
}

auto main(int aArgc, char** aArgv) -> int
try {
	QCoreApplication application{ aArgc, aArgv };
	return probe(QCoreApplication::arguments());
} catch (...) {
	return bpr::tests::failing_exit();
}
