#ifndef BPR_TESTS_SUPPORT_STRICT_CLI_FIXTURE_HPP
#define BPR_TESTS_SUPPORT_STRICT_CLI_FIXTURE_HPP

#include "gh_cli_transport.hpp"
#include "github_cli_process_runner.hpp"
#include "test_expectation.hpp"
#include <QCoreApplication>
#include <QProcessEnvironment>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <random>

namespace bpr::tests {
	using namespace adapters::github;
	using namespace adapters::github::detail;
	inline auto strict_method_name(http_method aMethod) -> std::string_view
	{
		switch (aMethod) {
		case http_method::get:     return "GET";
		case http_method::post:    return "POST";
		case http_method::patch:   return "PATCH";
		case http_method::put:     return "PUT";
		case http_method::delete_: return "DELETE";
		}
		return { };
	}

	inline auto strict_cli_profile(const application::github_credential_operation_context& aContext) -> github_profile_result
	{
		return validate_github_host_profile({ .Profile = aContext.Authority.Profile, .Host = aContext.Authority.Host, .Revision = aContext.Authority.ProfileRevision, .Server = aContext.Authority.ServerContract, .Authentication = application::github_authentication_transport::github_cli, .Proxy = application::github_proxy_mode::system, .Trust = application::github_trust_mode::system, .Provenance = github_profile_provenance::builtin, .Hostname = "github.com", .RestRoot = "https://api.github.com", .GraphqlRoot = "https://api.github.com/graphql", .CliAccountLogin = "fixture-reviewer" });
	}

	/// Test child: local named lookup and a credential-dependent API, with no network.
	inline auto strict_cli_child(const QStringList& aArguments) -> int
	{
		const auto        root = std::filesystem::path{ aArguments[0].toStdString() }.parent_path();
		std::ifstream     source{ root / "fixture-keychain", std::ios::binary };
		const std::string secret{ std::istreambuf_iterator<char>{ source }, { } };
		if (aArguments.size() == 2 && aArguments[1] == "--version") {
			std::cout << "gh version 2.101.0 (strict fixture)\n";
			return 0;
		}
		if (aArguments.size() >= 3 && aArguments.back() == "--help") {
			std::cout << "gh auth token [flags]\ngh repo clone <repository> [-- <gitflags>...]\ngh auth git-credential\n--hostname HOST\n--user LOGIN\n--include\n--method METHOD\n--header VALUE\n--input FILE\n--no-upstream\n";
			return 0;
		}
		if (aArguments.mid(1) == QStringList{ "auth", "token", "--hostname", "github.com", "--user", "fixture-reviewer" }) {
			if (!std::filesystem::exists(root / "available")) {
				return 1;
			}
			std::cout << secret << '\n';
			return 0;
		}
		if (aArguments.size() < 3 || aArguments[1] != "api"
			|| secret.empty() || qEnvironmentVariable("GH_TOKEN").toStdString() != secret
			|| !qEnvironmentVariable("GITHUB_TOKEN").isEmpty()
			|| !qEnvironmentVariable("GH_ENTERPRISE_TOKEN").isEmpty()) {
			return 2;
		}
		std::ifstream response{ root / "response", std::ios::binary };
		if (!response) {
			return 3;
		}
		std::cout << response.rdbuf();
		return 0;
	}

	/// Runs the real strict adapter/runner; only the server's replies are synthetic.
	class strict_cli_fixture final : public github_cli_process_port
	{
	public:
		explicit strict_cli_fixture(const std::filesystem::path& aRoot)
			: Root{ aRoot }
			, Runner{ make_github_cli_process_runner() }
		{
			std::filesystem::create_directory(Root / "neutral");
			std::filesystem::create_directory(Root / "home");
			std::filesystem::copy_file(QCoreApplication::applicationFilePath().toStdString(), Root / "gh");
			std::filesystem::permissions(Root / "gh", std::filesystem::perms::owner_all);
			Canary = "strict-fixture-" + std::to_string(std::random_device{ }()) + std::to_string(std::random_device{ }());
			std::ofstream source{ Root / "fixture-keychain", std::ios::binary };
			source << Canary;
			available(true);
		}
		auto available(bool aValue) -> void
		{
			if (aValue) {
				std::ofstream marker{ Root / "available" };
				marker << "available";
			} else {
				std::filesystem::remove(Root / "available");
			}
		}
		auto configuration() const -> gh_cli_configuration_result
		{
			return validate_gh_cli_configuration({ .Executable = Root / "gh", .NeutralWorkingDirectory = Root / "neutral", .Platform = gh_cli_macos_environment_facts{ .HomeDirectory = Root / "home" }, .Https = { } });
		}
		auto run(const github_cli_process_request& aRequest, std::stop_token aStop) -> github_cli_process_result override
		{
			expect_generic_request(aRequest);
			++Probes;
			return Runner->run(aRequest, aStop);
		}
		auto acquire_sensitive(const github_cli_process_request& aRequest, std::stop_token aStop, std::shared_ptr<const void> aLifetime) -> cli_sensitive_result override
		{
			expect_generic_request(aRequest);
			++Acquisitions;
			if (AcquisitionFailure) {
				return std::unexpected{ *AcquisitionFailure };
			}
			auto acquired = Runner->acquire_sensitive(aRequest, aStop, std::move(aLifetime));
			if (acquired) {
				Credential = *acquired;
			}
			return acquired;
		}
		auto run_credentialed(const github_cli_process_request& aRequest, cli_transient_credential aCredential,
							  cli_credential_environment aEnvironment, std::stop_token aStop) -> github_cli_process_result override
		{
			const exit_on_failure insideTheSession;
			expect_generic_request(aRequest);
			expect(Wire != nullptr && aCredential && aCredential.get() == Credential.lock().get(), "complete strict operation uses exactly one acquired owner");
			auto arguments = aRequest.Arguments;
#if defined(__APPLE__)
			arguments.erase(arguments.begin(), arguments.begin() + 3);
#endif
			expect(arguments.size() > 6 && arguments[0] == "api" && arguments[1] == Wire->Wire.Path.substr(1)
					   && arguments[3] == "github.com" && arguments[6] == strict_method_name(Wire->Wire.Method),
				   "actual strict child receives exact API resource, host and method");
			expect(aRequest.StandardInput == Wire->Wire.Body, "strict runner receives exact canonical request body");
			const auto    response = Respond(*Wire);
			std::ofstream output{ Root / "response", std::ios::binary | std::ios::trunc };
			if (const auto* success = std::get_if<github_mutation_wire_success>(&response)) {
				output << "HTTP/2.0 " << success->Response.Status << " status\r\nContent-Type: application/json\r\n\r\n"
					   << success->Response.Body;
			} else {
				output << "HTTP/2.0 500 status\r\nContent-Type: application/json\r\n\r\n{}";
			}
			output.close();
			++ApiCalls;
			if (Wire->Wire.Method != http_method::get) {
				++Writes;
			}
			auto result = Runner->run_credentialed(aRequest, std::move(aCredential), aEnvironment, aStop);
			expect(!result || result->StandardOutput.find(Canary) == std::string::npos, "strict credential never enters generic retained process output");
			if (Completed) {
				Completed(*Wire);
			}
			return result;
		}
		auto verify_persistence() const -> void
		{
			for (const auto& entry : std::filesystem::recursive_directory_iterator{ Root }) {
				if (!entry.is_regular_file() || entry.path() == Root / "fixture-keychain" || entry.path() == Root / "gh") {
					continue;
				}
				std::ifstream     file{ entry.path(), std::ios::binary };
				const std::string bytes{ std::istreambuf_iterator<char>{ file }, { } };
				expect(bytes.find(Canary) == std::string::npos, "strict canary is absent from SQLite recovery, response files and every BPR-created private configuration/store");
			}
		}
		std::function<github_mutation_wire_completion(const github_mutation_wire_request&)> Respond;
		std::function<void(const github_mutation_wire_request&)>                            Completed;
		const github_mutation_wire_request*                                                 Wire = nullptr;
		std::optional<github_cli_process_failure>                                           AcquisitionFailure;
		std::weak_ptr<const cli_sensitive_bytes>                                            Credential;
		std::size_t                                                                         Probes = 0, Acquisitions = 0, ApiCalls = 0, Writes = 0;

	private:
		auto expect_generic_request(const github_cli_process_request& aRequest) const -> void
		{
			expect(aRequest.StandardInput.find(Canary) == std::string::npos, "generic stdin is credential-free");
			for (const auto& argument : aRequest.Arguments) {
				expect(argument.find(Canary) == std::string::npos, "generic arguments are credential-free");
			}
			for (const auto& entry : aRequest.Environment) {
				expect(entry.Value.find(Canary) == std::string::npos && entry.Name != "GH_TOKEN" && entry.Name != "GITHUB_TOKEN"
						   && entry.Name != "GH_ENTERPRISE_TOKEN" && entry.Name != "GITHUB_ENTERPRISE_TOKEN",
					   "generic environments contain neither credential bytes nor token variables");
			}
		}
		std::filesystem::path                    Root;
		std::unique_ptr<github_cli_process_port> Runner;
		std::string                              Canary;
	};

	/// Records the semantic fixture request while the selected strict transport runs.
	class strict_cli_relay final : public github_mutation_wire_transport
	{
	public:
		strict_cli_relay(github_mutation_wire_transport& aInner, strict_cli_fixture& aFixture,
						 std::shared_ptr<github_mutation_wire_transport> aOwner = { })
			: Inner{ aInner }
			, Fixture{ aFixture }
			, Owner{ std::move(aOwner) }
		{ }
		auto begin_mutation_operation(std::stop_token aStop) -> github_mutation_operation override
		{
			auto operation = Inner.begin_mutation_operation(aStop);
			if (!operation) {
				return std::unexpected{ operation.error() };
			}
			if (!*operation) {
				return std::shared_ptr<github_mutation_wire_transport>{ };
			}
			return std::make_shared<strict_cli_relay>(**operation, Fixture, *operation);
		}
		auto perform_review_mutation(const github_mutation_wire_request& aRequest, std::stop_token aStop,
									 github_mutation_transmission_witness aWitness) -> github_mutation_wire_completion override
		{
			Fixture.Wire = &aRequest;
			auto result  = Inner.perform_review_mutation(aRequest, aStop, std::move(aWitness));
			Fixture.Wire = nullptr;
			return result;
		}

	private:
		github_mutation_wire_transport&                 Inner;
		strict_cli_fixture&                             Fixture;
		std::shared_ptr<github_mutation_wire_transport> Owner;
	};
}
#endif
