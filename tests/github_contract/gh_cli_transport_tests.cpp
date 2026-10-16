#include "../support/test_expectation.hpp"
#include "bpr/application/review_mutation_coordinator.hpp"
#include "gh_cli_transport.hpp"
#include "github_paged_read.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#if !defined(_WIN32)
	#include <sys/stat.h>
#endif

namespace {
	using namespace bpr;
	using namespace bpr::adapters::github;
	using namespace bpr::adapters::github::detail;

	static_assert(
		!std::is_default_constructible_v<validated_gh_cli_configuration>);
	static_assert(
		!std::is_move_constructible_v<validated_gh_cli_configuration>);
	static_assert(
		!std::is_move_assignable_v<validated_gh_cli_configuration>);
	static_assert(std::is_copy_constructible_v<github_wire_correlation>);
	static_assert(std::is_copy_constructible_v<github_wire_request>);
	static_assert(
		std::is_base_of_v<github_wire_transport_port, gh_cli_transport>);

	using bpr::tests::expect;

#if defined(_WIN32)
	constexpr auto private_cli_configuration_available = false;
#else
	constexpr auto private_cli_configuration_available = true;
#endif

	[[nodiscard]] auto gh_arguments(const github_cli_process_request& aRequest)
		-> std::vector<std::string>
	{
#if defined(__APPLE__)
		expect(aRequest.Executable == "/usr/bin/sandbox-exec"
				   && aRequest.Arguments.size() >= 4U
				   && aRequest.Arguments[0] == "-p"
				   && aRequest.Arguments[1] == "(version 1)(allow default)(deny file-write*)(allow file-write* (literal \"/dev/null\"))",
			   "every macOS gh request denies file writes before launch");
		return { aRequest.Arguments.begin() + 3, aRequest.Arguments.end() };
#else
		return aRequest.Arguments;
#endif
	}

	[[nodiscard]] auto path_bytes(const std::filesystem::path& aPath)
		-> std::string
	{
		const auto bytes = aPath.generic_u8string();
		return {
			reinterpret_cast<const char*>(bytes.data()),
			bytes.size(),
		};
	}

	auto create_regular_file(
		const std::filesystem::path& aPath,
		const bool                   aExecutable) -> void
	{
		std::ofstream stream{ aPath, std::ios::binary | std::ios::trunc };
		stream << "fixture\n";
		stream.close();
		expect(stream.good(), "temporary regular file is created");
#if !defined(_WIN32)
		std::error_code error;
		auto            permissions = std::filesystem::perms::owner_read
			| std::filesystem::perms::owner_write;
		if (aExecutable) {
			permissions |= std::filesystem::perms::owner_exec;
		}
		std::filesystem::permissions(
			aPath,
			permissions,
			std::filesystem::perm_options::replace,
			error);
		expect(!error, "temporary file permissions are set");
#else
		static_cast<void>(aExecutable);
#endif
	}

	class temporary_layout final
	{
	public:
		temporary_layout()
		{
			const auto base = std::filesystem::temp_directory_path();
			for (std::uint64_t attempt = 1U; attempt <= 100U; ++attempt) {
				const auto stamp = std::chrono::steady_clock::now()
									   .time_since_epoch()
									   .count();
				Root             = base
					/ ("bpr-gh-cli-"
					   + std::to_string(stamp)
					   + "-"
					   + std::to_string(attempt));
				std::error_code error;
				if (std::filesystem::create_directory(Root, error)) {
					break;
				}
				Root.clear();
			}
			expect(!Root.empty(), "isolated temporary root is allocated");
			// [[BPR-1121]]. Removed again if the rest of construction fails.
			auto made = bpr::tests::scratch_guard{ Root };

			ExecutableDirectory  = Root / "real";
			AliasDirectory       = Root / "alias";
			NeutralDirectory     = Root / "neutral";
			HomeDirectory        = Root / "home";
			RuntimeDirectory     = Root / "runtime";
			RoamingDirectory     = Root / "roaming";
			LocalDirectory       = Root / "local";
			SystemRootDirectory  = Root / "system-root";
			CertificateDirectory = Root / "certificates";
			for (const auto& directory : {
					 ExecutableDirectory,
					 AliasDirectory,
					 NeutralDirectory,
					 HomeDirectory,
					 RuntimeDirectory,
					 RoamingDirectory,
					 LocalDirectory,
					 SystemRootDirectory,
					 CertificateDirectory,
				 }) {
				std::error_code error;
				expect(
					std::filesystem::create_directory(directory, error)
						&& !error,
					"temporary layout directory is created");
			}

#if defined(_WIN32)
			Executable = ExecutableDirectory / "gh.exe";
			Alias      = AliasDirectory / "gh.exe";
#else
			Executable = ExecutableDirectory / "gh";
			Alias      = AliasDirectory / "gh";
#endif
			create_regular_file(Executable, true);
			CertificateFile = CertificateDirectory / "roots.pem";
			create_regular_file(CertificateFile, false);

			std::error_code symlinkError;
			std::filesystem::create_symlink(
				Executable,
				Alias,
				symlinkError);
			HasExecutableSymlink = !symlinkError;
			made.release();
		}

		temporary_layout(const temporary_layout&)                    = delete;
		auto operator=(const temporary_layout&) -> temporary_layout& = delete;

		~temporary_layout()
		{
			std::error_code error;
			std::filesystem::remove_all(Root, error);
		}

		std::filesystem::path Root;
		std::filesystem::path ExecutableDirectory;
		std::filesystem::path AliasDirectory;
		std::filesystem::path NeutralDirectory;
		std::filesystem::path HomeDirectory;
		std::filesystem::path RuntimeDirectory;
		std::filesystem::path RoamingDirectory;
		std::filesystem::path LocalDirectory;
		std::filesystem::path SystemRootDirectory;
		std::filesystem::path CertificateDirectory;
		std::filesystem::path CertificateFile;
		std::filesystem::path Executable;
		std::filesystem::path Alias;
		bool                  HasExecutableSymlink = false;
	};

	[[nodiscard]] auto platform_facts(const temporary_layout& aLayout)
		-> gh_cli_platform_environment_facts
	{
#if defined(__APPLE__)
		return gh_cli_macos_environment_facts{
			.HomeDirectory = aLayout.HomeDirectory,
		};
#elif defined(_WIN32)
		return gh_cli_windows_environment_facts{
			.UserProfileDirectory            = aLayout.HomeDirectory,
			.RoamingApplicationDataDirectory = aLayout.RoamingDirectory,
			.LocalApplicationDataDirectory   = aLayout.LocalDirectory,
			.SystemRootDirectory             = aLayout.SystemRootDirectory,
		};
#elif defined(__linux__)
		return gh_cli_linux_environment_facts{
			.HomeDirectory     = aLayout.HomeDirectory,
			.RuntimeDirectory  = aLayout.RuntimeDirectory,
			.SessionBusAddress = "unix:path=" + path_bytes(aLayout.RuntimeDirectory / "bus"),
		};
#else
	#error Unsupported contract-test platform
#endif
	}

	[[nodiscard]] auto configuration_candidate(
		const temporary_layout& aLayout) -> gh_cli_configuration_candidate
	{
		return {
			.Executable              = aLayout.HasExecutableSymlink ? aLayout.Alias : aLayout.Executable,
			.NeutralWorkingDirectory = aLayout.NeutralDirectory,
			.Platform                = platform_facts(aLayout),
			.Https                   = gh_cli_https_environment_facts{
				.Proxy                = "https://proxy.example.invalid:8443",
				.NoProxy              = "localhost,127.0.0.1",
				.CertificateFile      = std::nullopt,
				.CertificateDirectory = std::nullopt,
			},
		};
	}

	[[nodiscard]] auto expected_environment(
		const temporary_layout& aLayout)
		-> std::vector<github_cli_process_environment_entry>
	{
		std::vector<github_cli_process_environment_entry> result{
			{ "GH_PROMPT_DISABLED", "1" },
			{ "GH_NO_UPDATE_NOTIFIER", "1" },
			{ "GH_NO_EXTENSION_UPDATE_NOTIFIER", "1" },
			{ "GH_SPINNER_DISABLED", "1" },
			{ "GH_TELEMETRY", "0" },
			{ "NO_COLOR", "1" },
			{ "CLICOLOR", "0" },
		};
#if defined(__APPLE__)
		result.push_back(
			{ "HOME", path_bytes(std::filesystem::canonical(aLayout.HomeDirectory)) });
#elif defined(_WIN32)
		result.push_back(
			{ "USERPROFILE", path_bytes(std::filesystem::canonical(aLayout.HomeDirectory)) });
		result.push_back(
			{ "APPDATA", path_bytes(std::filesystem::canonical(aLayout.RoamingDirectory)) });
		result.push_back(
			{ "LOCALAPPDATA", path_bytes(std::filesystem::canonical(aLayout.LocalDirectory)) });
		result.push_back(
			{ "SystemRoot", path_bytes(std::filesystem::canonical(aLayout.SystemRootDirectory)) });
		result.push_back({ "PATH", "" });
#elif defined(__linux__)
		result.push_back(
			{ "HOME", path_bytes(std::filesystem::canonical(aLayout.HomeDirectory)) });
		result.push_back(
			{ "XDG_RUNTIME_DIR", path_bytes(std::filesystem::canonical(aLayout.RuntimeDirectory)) });
		result.push_back(
			{
				"DBUS_SESSION_BUS_ADDRESS",
				"unix:path=" + path_bytes(aLayout.RuntimeDirectory / "bus"),
			});
#endif
		result.push_back(
			{ "HTTPS_PROXY", "https://proxy.example.invalid:8443" });
		result.push_back({ "NO_PROXY", "localhost,127.0.0.1" });
		return result;
	}

	[[nodiscard]] auto cloud_candidate() -> github_host_profile_candidate
	{
		return {
			.Profile        = core::host_profile_id{ 11 },
			.Host           = core::git_host_id{ 21 },
			.Revision       = application::github_profile_revision{ 31 },
			.Server         = application::github_server_contract::github_com_2022_11_28,
			.Authentication = application::github_authentication_transport::github_cli,
			.Proxy          = application::github_proxy_mode::system,
			.Trust          = application::github_trust_mode::system,
			.Provenance     = github_profile_provenance::builtin,
			.Hostname       = "github.com",
			.RestRoot       = "https://api.github.com",
			.GraphqlRoot    = "https://api.github.com/graphql",
		};
	}

	[[nodiscard]] auto enterprise_candidate()
		-> github_host_profile_candidate
	{
		return {
			.Profile        = core::host_profile_id{ 12 },
			.Host           = core::git_host_id{ 22 },
			.Revision       = application::github_profile_revision{ 32 },
			.Server         = application::github_server_contract::ghes_3_18_2022_11_28,
			.Authentication = application::github_authentication_transport::github_cli,
			.Proxy          = application::github_proxy_mode::system,
			.Trust          = application::github_trust_mode::system,
			.Provenance     = github_profile_provenance::managed,
			.Hostname       = "ghe.example.invalid",
			.RestRoot       = "https://ghe.example.invalid/api/v3",
			.GraphqlRoot    = "https://ghe.example.invalid/api/graphql",
		};
	}

	[[nodiscard]] auto semantic(
		const read_operation  aOperation,
		const server_contract aServer) -> semantic_read_request
	{
		semantic_read_request request{
			.Server            = aServer,
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
			request.Owner      = "octo-org";
			request.Repository = "review-repository";
		}
		switch (aOperation) {
		case read_operation::get_pull_request:
		case read_operation::list_pull_request_files:
		case read_operation::list_pull_request_commits:
		case read_operation::list_reviews:
		case read_operation::list_review_threads:
		case read_operation::list_issue_comments:
		case read_operation::list_pull_request_timeline:
		case read_operation::list_review_comments:
		case read_operation::get_merge_assessment:
		case read_operation::list_review_requests:
		case read_operation::list_requested_reviewers:
			request.PullRequestNumber = 17U;
			break;
		case read_operation::get_viewer_identity:
		case read_operation::get_repository:
		case read_operation::list_checks:
		case read_operation::list_commit_statuses:
		case read_operation::search_pull_requests:
			break;
		case read_operation::list_branch_rules:
		case read_operation::get_branch_protection:
			request.BranchRef = "main";
			break;
		case read_operation::search_personal_pull_requests:
			request.SearchTerms = "author:octo-reviewer";
			break;
		}
		if (aOperation == read_operation::list_checks) {
			request.HeadOid = "0123456789abcdef0123456789abcdef01234567";
		}
		if (aOperation == read_operation::search_pull_requests) {
			request.SearchTerms = "is:open";
		}
		return request;
	}

	[[nodiscard]] auto pull_request_parent(
		const validated_github_host_profile& aProfile)
		-> core::pull_request_request_context
	{
		return {
			.Request            = core::request_id{ 101 },
			.ApplicationSession = core::application_session_id{ 102 },
			.Profile            = aProfile.profile(),
			.Host               = aProfile.host(),
			.Account            = core::github_account_id{ 103 },
			.Repository         = core::repository_locator{
				.Owner = "octo-org",
				.Name  = "review-repository",
			},
			.Number              = core::pull_request_number{ 17 },
			.ExpectedRepository  = core::repository_id{ 104 },
			.ExpectedPullRequest = core::pull_request_id{ 105 },
			.Capabilities        = core::capability_revision{ 106 },
			.IdentityGeneration  = core::github_identity_generation{ 107 },
			.Generation          = core::request_generation{ 108 },
		};
	}

	/// [[BPR-0988]]. The repository scope of a discovery search.
	[[nodiscard]] auto repository_parent(
		const validated_github_host_profile& aProfile)
		-> core::repository_request_context
	{
		const auto parent = pull_request_parent(aProfile);
		return {
			.Request            = parent.Request,
			.ApplicationSession = parent.ApplicationSession,
			.Profile            = parent.Profile,
			.Host               = parent.Host,
			.Account            = parent.Account,
			.Repository         = parent.Repository,
			.Capabilities       = parent.Capabilities,
			.IdentityGeneration = parent.IdentityGeneration,
		};
	}

	[[nodiscard]] auto session_parent(
		const validated_github_host_profile& aProfile)
		-> core::github_session_request_context
	{
		return {
			.Request            = core::request_id{ 201 },
			.ApplicationSession = core::application_session_id{ 202 },
			.Profile            = aProfile.profile(),
			.IdentityGeneration = core::github_identity_generation{ 203 },
		};
	}

	[[nodiscard]] auto wire_request_for(
		const validated_github_host_profile& aProfile,
		const read_operation                 aOperation) -> github_wire_request
	{
		const auto server = aProfile.server_contract()
				== application::github_server_contract::
					github_com_2022_11_28
			? server_contract::github_com
			: server_contract::ghes_3_18;
		const auto wire   = compile_read_request(semantic(aOperation, server));
		expect(wire.has_value(), "canonical wire request compiles");
		return {
			.Correlation = github_wire_correlation{
				.Parent          = aOperation == read_operation::get_viewer_identity
					? github_wire_parent_context{ session_parent(aProfile) }
					: aOperation == read_operation::search_personal_pull_requests
					? github_wire_parent_context{ repository_parent(aProfile) }
					: github_wire_parent_context{
						  pull_request_parent(aProfile) },
				.ProfileRevision = aProfile.revision(),
				.Host            = aProfile.host(),
				.Operation       = aOperation,
				.Page            = 1U,
			},
			.Wire = *wire,
		};
	}

	[[nodiscard]] auto expected_endpoint(
		const github_wire_request& aRequest,
		const server_contract      aServer) -> std::string
	{
		if (graphql_read(aRequest.Wire.Operation)) {
			return "graphql";
		}
		const auto  path = std::string_view{ aRequest.Wire.Path };
		std::string result;
		if (aServer == server_contract::github_com) {
			result = path.substr(1U);
		} else {
			constexpr auto prefix = std::string_view{ "/api/v3/" };
			result                = path.substr(prefix.size());
		}
		if (!aRequest.Wire.Query.empty()) {
			result += '?';
			result += aRequest.Wire.Query;
		}
		return result;
	}

	[[nodiscard]] auto expected_arguments(
		const validated_github_host_profile& aProfile,
		const github_wire_request&           aRequest) -> std::vector<std::string>
	{
		const auto               server = aRequest.Wire.Server;
		std::vector<std::string> result{
			"api",
			expected_endpoint(aRequest, server),
			"--hostname",
			std::string{ aProfile.hostname() },
			"--include",
			"--method",
			aRequest.Wire.Method == http_method::get ? "GET" : "POST",
		};
		for (const auto& header : aRequest.Wire.Headers) {
			result.emplace_back("--header");
			result.emplace_back(header.Name + ": " + header.Value);
		}
		if (aRequest.Wire.Method == http_method::post) {
			result.emplace_back("--input");
			result.emplace_back("-");
		}
		return result;
	}

	[[nodiscard]] auto synthetic_operation_credential() -> cli_transient_credential
	{
		auto value = std::make_shared<cli_sensitive_bytes>();
		expect(value->append("synthetic-operation-credential\n") && value->finish_credential(), "synthetic scoped credential is valid");
		return value;
	}

	[[nodiscard]] auto scoped_configuration(const validated_gh_cli_configuration& aConfiguration, const validated_github_host_profile& aProfile) -> validated_gh_cli_configuration
	{
		if (aConfiguration.pin()) {
			return aConfiguration;
		}
		const auto value = pin_gh_cli_configuration(aConfiguration, aProfile.hostname(), "fixture");
		expect(value.has_value(), "compiler course selects one explicit account");
		return *value;
	}

	class successful_counting_process final : public github_cli_process_port
	{
	public:
		[[nodiscard]] auto run(
			const github_cli_process_request& aRequest,
			const std::stop_token             aStop) -> github_cli_process_result override
		{
			const auto arguments = gh_arguments(aRequest);
			if (EnableAcquisition && ProbeFailure && (arguments == std::vector<std::string>{ "--version" } || (!arguments.empty() && arguments.back() == "--help"))) {
				return std::unexpected{ *ProbeFailure };
			}
			if (EnableAcquisition && arguments == std::vector<std::string>{ "--version" }) {
				return github_cli_process_success{ .StandardOutput = "gh version 2.101.0 (fixture)\n" };
			}
			if (EnableAcquisition && !arguments.empty() && arguments.back() == "--help") {
				return github_cli_process_success{ .StandardOutput = "gh auth token [flags]\ngh repo clone <repo> [-- <gitflags>...]\ngh auth git-credential\n --hostname HOST\n --user LOGIN\n --include\n --method METHOD\n --header VALUE\n --input FILE\n --no-upstream\n" };
			}
			++Calls;
			LastRequest       = aRequest;
			LastStopRequested = aStop.stop_requested();
			if (StopOnRun != nullptr) {
				StopOnRun->request_stop();
			}
			if (OnRun) {
				OnRun();
			}
			return Result;
		}

		[[nodiscard]] auto acquire_sensitive(const github_cli_process_request& aRequest, std::stop_token aStop,
											 std::shared_ptr<const void> = { }) -> cli_sensitive_result override
		{
			++Acquisitions;
			expect(gh_arguments(aRequest) == std::vector<std::string>{ "auth", "token", "--hostname", "github.com", "--user", "fixture-reviewer" }, "recovery operation acquires only its exact named credential");
			if (MissingCredential || aStop.stop_requested()) {
				return std::unexpected{ aStop.stop_requested() ? github_cli_process_failure::cancelled : github_cli_process_failure::nonzero_exit };
			}
			if (AcquisitionFailure) {
				return std::unexpected{ *AcquisitionFailure };
			}
			auto credential    = synthetic_operation_credential();
			AcquiredCredential = credential;
			return credential;
		}

		[[nodiscard]] auto run_credentialed(const github_cli_process_request& aRequest, cli_transient_credential aCredential,
											cli_credential_environment, std::stop_token                          aStop) -> github_cli_process_result override
		{
			expect(aCredential != nullptr, "compiler unit seam receives a scoped credential separately");
			if (Acquisitions != 0U) {
				expect(aCredential.get() == AcquiredCredential.lock().get(), "all requests in the operation use exactly the acquired owner");
			}
			return run(aRequest, aStop);
		}

		bool                                      EnableAcquisition = false;
		std::size_t                               Acquisitions      = 0U;
		bool                                      MissingCredential = false;
		std::optional<github_cli_process_failure> AcquisitionFailure;
		std::optional<github_cli_process_failure> ProbeFailure;
		std::weak_ptr<const cli_sensitive_bytes>  AcquiredCredential;
		github_cli_process_result                 Result = github_cli_process_success{ .StandardOutput = "HTTP/2.0 200 OK\nContent-Type: application/json\r\n\r\n{}" };
		std::function<void()>                     OnRun;
		std::stop_source*                         StopOnRun = nullptr;
		std::size_t                               Calls     = 0U;
		std::optional<github_cli_process_request> LastRequest;
		bool                                      LastStopRequested = false;
	};

	auto set_unknown_canary(
		const std::string& aName,
		const std::string& aValue,
		const bool         aPresent) -> void
	{
#if defined(_WIN32)
		const auto result = _putenv_s(
			aName.c_str(),
			aPresent ? aValue.c_str() : "");
		expect(result == 0, "unknown parent environment canary is updated");
#else
		const auto result = aPresent
			? setenv(
				  aName.c_str(),
				  aValue.c_str(),
				  1)
			: unsetenv(aName.c_str());
		expect(result == 0, "unknown parent environment canary is updated");
#endif
	}

	auto expect_configuration_failure(
		const gh_cli_configuration_candidate& aCandidate,
		const gh_cli_configuration_failure    aFailure,
		const std::string_view                aMessage) -> void
	{
		const auto result = validate_gh_cli_configuration(aCandidate);
		expect(
			!result.has_value() && result.error() == aFailure,
			aMessage);
	}

	auto expect_invocation_failure(
		const validated_gh_cli_configuration& aConfiguration,
		const validated_github_host_profile&  aProfile,
		const github_wire_request&            aRequest,
		const gh_cli_invocation_failure       aFailure,
		const std::string_view                aMessage) -> void
	{
		const auto result = compile_gh_cli_process_request(aConfiguration, aProfile, aRequest);
		expect(
			!result.has_value() && result.error() == aFailure,
			aMessage);
	}

	auto test_configuration_is_canonical_typed_and_closed() -> void
	{
		temporary_layout layout;
		const auto       suffix = std::to_string(
			std::chrono::steady_clock::now()
				.time_since_epoch()
				.count());
		const auto unknownName  = "BPR_UNKNOWN_GH_ENV_CANARY_" + suffix;
		const auto unknownValue = "runtime-only-" + suffix;
		set_unknown_canary(unknownName, unknownValue, true);
		const auto result = validate_gh_cli_configuration(configuration_candidate(layout));
		set_unknown_canary(unknownName, unknownValue, false);
		expect(result.has_value(), "canonical CLI configuration validates");
		expect(
			result->executable()
					== std::filesystem::canonical(layout.Executable)
				&& result->neutral_working_directory()
					== std::filesystem::canonical(layout.NeutralDirectory),
			"validated configuration stores canonical real paths");
		if (layout.HasExecutableSymlink) {
			expect(
				result->executable() != layout.Alias,
				"absolute executable symlink is resolved before storage");
		}

		const auto cloud = validate_github_host_profile(cloud_candidate());
		expect(cloud.has_value(), "cloud CLI profile validates");
		const auto invocation = compile_gh_cli_process_request(
			*result,
			*cloud,
			wire_request_for(
				*cloud,
				read_operation::get_viewer_identity));
		expect(invocation.has_value(), "closed invocation compiles");
		expect(
			invocation->Environment == expected_environment(layout),
			"child environment is the exact typed allowlist");
		expect(
			std::ranges::none_of(
				invocation->Environment,
				[&unknownName](
					const github_cli_process_environment_entry& aEntry) {
					return aEntry.Name == unknownName;
				}),
			"unknown parent environment canary is not inherited");
	}

	auto test_all_operations_compile_to_exact_closed_invocations() -> void
	{
		temporary_layout layout;
		const auto       configuration = validate_gh_cli_configuration(configuration_candidate(layout));
		const auto       cloud         = validate_github_host_profile(cloud_candidate());
		const auto       enterprise    = validate_github_host_profile(enterprise_candidate());
		expect(
			configuration.has_value() && cloud.has_value()
				&& enterprise.has_value(),
			"all exact-invocation prerequisites validate");
		expect(gh_arguments(compile_gh_cli_version_request(*configuration))
				   == std::vector<std::string>{ "--version" },
			   "version preflight has the same write containment as reads");
		for (const auto& [topic, expected] : std::array<std::pair<gh_cli_help_topic, std::vector<std::string>>, 3U>{ {
				 { gh_cli_help_topic::api, { "api", "--help" } },
				 { gh_cli_help_topic::repo_clone, { "repo", "clone", "--help" } },
				 { gh_cli_help_topic::auth_git_credential, { "auth", "git-credential", "--help" } },
			 } }) {
			const auto help = compile_gh_cli_help_request(*configuration, topic);
			expect(gh_arguments(help) == expected && help.StandardInput.empty() && help.StandardInputLimit == 1U
					   && help.StandardOutputLimit == 32U * 1024U && help.StandardErrorLimit == 2048U
					   && help.Timeout == std::chrono::seconds{ 10 } && !help.PreserveCompletedOutput
					   && help.Environment == compile_gh_cli_version_request(*configuration).Environment,
				   "each gh capability probe is closed local help with the version probe's write denial, environment and bounds");
		}

		// [[BPR-0988]]. The two discovery reads join the recorded catalog here:
		// a configured CLI session discovers through the same closed shape.
		constexpr auto discovery  = std::array{ read_operation::search_personal_pull_requests, read_operation::list_requested_reviewers };
		auto           operations = std::vector<read_operation>{ read_operation_catalog.begin(), read_operation_catalog.end() };
		operations.insert(operations.end(), discovery.begin(), discovery.end());
		operations.insert(operations.end(), { read_operation::get_merge_assessment, read_operation::list_review_requests, read_operation::list_branch_rules, read_operation::get_branch_protection });
		for (const auto* profile : { &*cloud, &*enterprise }) {
			for (const auto operation : operations) {
				const auto request    = wire_request_for(*profile, operation);
				const auto invocation = compile_gh_cli_process_request(
					*configuration,
					*profile,
					request);
				expect(
					invocation.has_value(),
					"every read operation compiles for both server contracts");
				expect(
#if defined(__APPLE__)
					invocation->Arguments[2]
#else
					invocation->Executable
#endif
							== path_bytes(std::filesystem::canonical(
								layout.Executable))
						&& invocation->WorkingDirectory
							== path_bytes(std::filesystem::canonical(
								layout.NeutralDirectory)),
					"invocation retains canonical executable and neutral cwd");
				expect(
					gh_arguments(*invocation)
						== expected_arguments(*profile, request),
					"invocation uses the exact closed argument shape");
				expect(
					invocation->Environment == expected_environment(layout),
					"every operation uses the exact fresh child environment");
				expect(
					invocation->StandardInput == request.Wire.Body
						&& (request.Wire.Method == http_method::post
							|| invocation->StandardInput.empty()),
					"only the canonical GraphQL body enters standard input");
				expect(
					invocation->StandardInputLimit == 256U * 1024U
						&& invocation->StandardOutputLimit
							== 4U * 1024U * 1024U
						&& invocation->StandardErrorLimit == 64U * 1024U
						&& invocation->Timeout == std::chrono::seconds{ 30 }
						&& invocation->ReapTimeout
							== std::chrono::seconds{ 2 },
					"all process dimensions use fixed bounds");
				expect(
					!gh_arguments(*invocation)[1U].starts_with('-')
						&& !gh_arguments(*invocation)[1U].starts_with('@')
						&& gh_arguments(*invocation)[1U].find("://")
							== std::string::npos,
					"compiled endpoint remains relative and non-option-like");
			}
		}
	}

	auto test_configuration_rejects_ambient_and_unsafe_candidates() -> void
	{
		temporary_layout layout;
		const auto       canonical = configuration_candidate(layout);

		auto candidate       = canonical;
		candidate.Executable = std::filesystem::path{ "gh" };
		expect_configuration_failure(
			candidate,
			gh_cli_configuration_failure::executable_not_absolute,
			"relative executable is rejected");

		candidate = canonical;
#if defined(_WIN32)
		candidate.Executable = layout.Root / "missing" / "gh.exe";
#else
		candidate.Executable = layout.Root / "missing" / "gh";
#endif
		expect_configuration_failure(
			candidate,
			gh_cli_configuration_failure::executable_not_found,
			"missing executable is rejected");

		candidate            = canonical;
		candidate.Executable = layout.ExecutableDirectory;
		expect_configuration_failure(
			candidate,
			gh_cli_configuration_failure::executable_not_regular,
			"directory executable is rejected");

		const auto wrongName = layout.Root / "other";
		create_regular_file(wrongName, true);
		candidate            = canonical;
		candidate.Executable = wrongName;
		expect_configuration_failure(
			candidate,
			gh_cli_configuration_failure::executable_name_mismatch,
			"executable with the wrong canonical name is rejected");

#if !defined(_WIN32)
		const auto      nonExecutableDirectory = layout.Root / "not-executable";
		std::error_code error;
		expect(
			std::filesystem::create_directory(nonExecutableDirectory, error)
				&& !error,
			"non-executable fixture directory is created");
		const auto nonExecutable = nonExecutableDirectory / "gh";
		create_regular_file(nonExecutable, false);
		candidate            = canonical;
		candidate.Executable = nonExecutable;
		expect_configuration_failure(
			candidate,
			gh_cli_configuration_failure::executable_not_executable,
			"non-executable regular file is rejected");

		const auto wrongClassExecutable = nonExecutableDirectory / "wrong-class" / "gh";
		expect(
			std::filesystem::create_directories(
				wrongClassExecutable.parent_path(),
				error)
				&& !error,
			"wrong-class executable fixture directory is created");
		create_regular_file(wrongClassExecutable, false);
		std::filesystem::permissions(
			wrongClassExecutable,
			std::filesystem::perms::owner_read
				| std::filesystem::perms::owner_write
				| std::filesystem::perms::group_exec
				| std::filesystem::perms::others_exec,
			std::filesystem::perm_options::replace,
			error);
		expect(!error, "wrong-class execute permissions are set");
		candidate            = canonical;
		candidate.Executable = wrongClassExecutable;
		expect_configuration_failure(
			candidate,
			gh_cli_configuration_failure::executable_not_executable,
			"execute bits from an inapplicable permission class are rejected");
#endif

		candidate                         = canonical;
		candidate.NeutralWorkingDirectory = "relative";
		expect_configuration_failure(
			candidate,
			gh_cli_configuration_failure::working_directory_not_absolute,
			"relative working directory is rejected");

		candidate                         = canonical;
		candidate.NeutralWorkingDirectory = layout.Root / "missing-working";
		expect_configuration_failure(
			candidate,
			gh_cli_configuration_failure::working_directory_not_found,
			"missing working directory is rejected");

		const auto workingFile = layout.Root / "working-file";
		create_regular_file(workingFile, false);
		candidate                         = canonical;
		candidate.NeutralWorkingDirectory = workingFile;
		expect_configuration_failure(
			candidate,
			gh_cli_configuration_failure::working_directory_not_directory,
			"regular-file working directory is rejected");

		const auto      repository      = layout.Root / "repository";
		const auto      repositoryChild = repository / "child";
		std::error_code directoryError;
		expect(
			std::filesystem::create_directories(
				repository / ".git",
				directoryError)
				&& !directoryError,
			"repository marker fixture is created");
		expect(
			std::filesystem::create_directory(
				repositoryChild,
				directoryError)
				&& !directoryError,
			"nested repository fixture is created");
		candidate                         = canonical;
		candidate.NeutralWorkingDirectory = repositoryChild;
		expect_configuration_failure(
			candidate,
			gh_cli_configuration_failure::working_directory_is_repository,
			"working directory with a repository ancestor is rejected");

		const auto bareRepository      = layout.Root / "bare-repository";
		const auto bareRepositoryChild = bareRepository / "child";
		directoryError.clear();
		expect(
			std::filesystem::create_directories(
				bareRepository / "objects",
				directoryError)
				&& !directoryError,
			"bare repository objects fixture is created");
		expect(
			std::filesystem::create_directory(
				bareRepository / "refs",
				directoryError)
				&& !directoryError,
			"bare repository refs fixture is created");
		create_regular_file(bareRepository / "HEAD", false);
		expect(
			std::filesystem::create_directory(
				bareRepositoryChild,
				directoryError)
				&& !directoryError,
			"bare repository child fixture is created");
		candidate                         = canonical;
		candidate.NeutralWorkingDirectory = bareRepositoryChild;
		expect_configuration_failure(
			candidate,
			gh_cli_configuration_failure::working_directory_is_repository,
			"working directory below a bare repository is rejected");

		candidate = canonical;
#if defined(__APPLE__)
		candidate.Platform = gh_cli_linux_environment_facts{
			.HomeDirectory     = layout.HomeDirectory,
			.RuntimeDirectory  = layout.RuntimeDirectory,
			.SessionBusAddress = "unix:path=" + path_bytes(layout.RuntimeDirectory / "bus"),
		};
#elif defined(_WIN32)
		candidate.Platform = gh_cli_macos_environment_facts{
			.HomeDirectory = layout.HomeDirectory,
		};
#elif defined(__linux__)
		candidate.Platform = gh_cli_macos_environment_facts{
			.HomeDirectory = layout.HomeDirectory,
		};
#endif
		expect_configuration_failure(
			candidate,
			gh_cli_configuration_failure::irrelevant_platform_facts,
			"facts for another platform are rejected");

		candidate = canonical;
#if defined(__APPLE__)
		candidate.Platform = gh_cli_macos_environment_facts{
			.HomeDirectory = "relative-home",
		};
#elif defined(_WIN32)
		auto windows                 = std::get<gh_cli_windows_environment_facts>(candidate.Platform);
		windows.UserProfileDirectory = "relative-home";
		candidate.Platform           = windows;
#elif defined(__linux__)
		auto linux              = std::get<gh_cli_linux_environment_facts>(candidate.Platform);
		linux.SessionBusAddress = "tcp:host=outside.invalid";
		candidate.Platform      = linux;
#endif
		expect_configuration_failure(
			candidate,
			gh_cli_configuration_failure::invalid_platform_environment,
			"invalid platform session facts are rejected");

		candidate             = canonical;
		candidate.Https.Proxy = "ftp://proxy.example.invalid";
		expect_configuration_failure(
			candidate,
			gh_cli_configuration_failure::invalid_https_proxy,
			"non-HTTP proxy is rejected");

		candidate             = canonical;
		candidate.Https.Proxy = "https://identity:secret@proxy.example.invalid";
		expect_configuration_failure(
			candidate,
			gh_cli_configuration_failure::
				credential_bearing_https_proxy,
			"credential-bearing proxy is rejected");

		candidate               = canonical;
		candidate.Https.NoProxy = "https://outside.invalid";
		expect_configuration_failure(
			candidate,
			gh_cli_configuration_failure::invalid_no_proxy,
			"non-canonical no-proxy value is rejected");

		candidate                       = canonical;
		candidate.Https.CertificateFile = layout.Root / "missing-roots.pem";
#if defined(__linux__)
		expect_configuration_failure(
			candidate,
			gh_cli_configuration_failure::invalid_certificate_file,
			"missing Linux certificate file is rejected");

		const auto unsafe_certificate = layout.CertificateDirectory / "unsafe\nroots.pem";
		create_regular_file(unsafe_certificate, false);
		candidate                       = canonical;
		candidate.Https.CertificateFile = unsafe_certificate;
		expect_configuration_failure(
			candidate,
			gh_cli_configuration_failure::environment_limit,
			"unsafe canonical certificate environment value is rejected");
#else
		expect_configuration_failure(
			candidate,
			gh_cli_configuration_failure::irrelevant_platform_facts,
			"certificate override is rejected on a system-trust platform");
#endif
	}

	auto test_correlation_server_and_wire_drift_are_rejected() -> void
	{
		temporary_layout layout;
		const auto       configuration = validate_gh_cli_configuration(configuration_candidate(layout));
		const auto       cloud         = validate_github_host_profile(cloud_candidate());
		const auto       enterprise    = validate_github_host_profile(enterprise_candidate());
		expect(
			configuration.has_value() && cloud.has_value()
				&& enterprise.has_value(),
			"mismatch-test prerequisites validate");
		const auto canonical = wire_request_for(*cloud, read_operation::get_pull_request);

		auto hostile                        = canonical;
		hostile.Correlation.ProfileRevision = application::github_profile_revision{ 999 };
		expect_invocation_failure(
			*configuration,
			*cloud,
			hostile,
			gh_cli_invocation_failure::profile_correlation_mismatch,
			"profile revision drift is rejected");

		hostile                  = canonical;
		hostile.Correlation.Host = core::git_host_id{ 999 };
		expect_invocation_failure(
			*configuration,
			*cloud,
			hostile,
			gh_cli_invocation_failure::profile_correlation_mismatch,
			"host drift is rejected");

		hostile                       = canonical;
		hostile.Correlation.Operation = read_operation::get_repository;
		expect_invocation_failure(
			*configuration,
			*cloud,
			hostile,
			gh_cli_invocation_failure::profile_correlation_mismatch,
			"operation drift is rejected");

		hostile                  = canonical;
		hostile.Correlation.Page = 2U;
		expect_invocation_failure(
			*configuration,
			*cloud,
			hostile,
			gh_cli_invocation_failure::profile_correlation_mismatch,
			"page drift is rejected");

		hostile     = canonical;
		auto parent = std::get<core::pull_request_request_context>(
			hostile.Correlation.Parent);
		parent.Repository.Name     = "other-repository";
		hostile.Correlation.Parent = parent;
		expect_invocation_failure(
			*configuration,
			*cloud,
			hostile,
			gh_cli_invocation_failure::profile_correlation_mismatch,
			"full pull-request parent drift is rejected");

		hostile = canonical;
		parent  = std::get<core::pull_request_request_context>(
			hostile.Correlation.Parent);
		parent.Request             = { };
		hostile.Correlation.Parent = parent;
		expect_invocation_failure(
			*configuration,
			*cloud,
			hostile,
			gh_cli_invocation_failure::profile_correlation_mismatch,
			"invalid parent correlation is rejected");

		auto serverCross                        = canonical;
		parent                                  = pull_request_parent(*enterprise);
		serverCross.Correlation.Parent          = parent;
		serverCross.Correlation.ProfileRevision = enterprise->revision();
		serverCross.Correlation.Host            = enterprise->host();
		expect_invocation_failure(
			*configuration,
			*enterprise,
			serverCross,
			gh_cli_invocation_failure::server_contract_mismatch,
			"wire server cannot cross the selected profile");

		hostile           = canonical;
		hostile.Wire.Path = "https://api.github.com/repos/octo-org/review-repository";
		expect_invocation_failure(
			*configuration,
			*cloud,
			hostile,
			gh_cli_invocation_failure::invalid_wire_request,
			"absolute caller endpoint is rejected by complete revalidation");

		hostile           = canonical;
		hostile.Wire.Path = "/repos/{owner}/review-repository";
		expect_invocation_failure(
			*configuration,
			*cloud,
			hostile,
			gh_cli_invocation_failure::invalid_wire_request,
			"placeholder endpoint is rejected by complete revalidation");

		hostile           = canonical;
		hostile.Wire.Body = "@outside-file";
		expect_invocation_failure(
			*configuration,
			*cloud,
			hostile,
			gh_cli_invocation_failure::invalid_wire_request,
			"caller-controlled standard-input alias is rejected");

		auto nativeCandidate           = cloud_candidate();
		nativeCandidate.Authentication = application::github_authentication_transport::native_https;
		const auto native              = validate_github_host_profile(nativeCandidate);
		expect(native.has_value(), "native profile fixture validates");
		expect_invocation_failure(
			*configuration,
			*native,
			canonical,
			gh_cli_invocation_failure::profile_transport_mismatch,
			"non-CLI profile cannot compile a CLI invocation");

		auto       sessionRequest         = wire_request_for(*cloud, read_operation::get_viewer_identity);
		const auto session                = session_parent(*cloud);
		sessionRequest.Correlation.Parent = session;
		expect(
			compile_gh_cli_process_request(
				*configuration,
				*cloud,
				sessionRequest)
				.has_value(),
			"viewer read accepts a complete session parent");
		auto viewerPullRequest               = wire_request_for(*cloud, read_operation::get_viewer_identity);
		viewerPullRequest.Correlation.Parent = pull_request_parent(*cloud);
		expect_invocation_failure(
			*configuration,
			*cloud,
			viewerPullRequest,
			gh_cli_invocation_failure::profile_correlation_mismatch,
			"viewer read cannot inherit a pull-request parent correlation");
		auto nonViewerSession               = wire_request_for(*cloud, read_operation::get_repository);
		nonViewerSession.Correlation.Parent = session;
		expect_invocation_failure(
			*configuration,
			*cloud,
			nonViewerSession,
			gh_cli_invocation_failure::profile_correlation_mismatch,
			"repository read cannot lose its pull-request parent correlation");
	}

	auto test_transport_parses_actual_response_and_fixed_failures() -> void
	{
		temporary_layout layout;
		const auto       configuration = validate_gh_cli_configuration(configuration_candidate(layout));
		const auto       cloud         = validate_github_host_profile(cloud_candidate());
		expect(configuration.has_value() && cloud.has_value(), "transport prerequisites validate");
		successful_counting_process process;
		gh_cli_transport            transport{ *cloud, scoped_configuration(*configuration, *cloud), process, synthetic_operation_credential() };
		const auto                  request = wire_request_for(*cloud, read_operation::get_repository);
		const std::string           body    = "{\"private_fixture\":true}\n";
		// Source-derived isolated fixture: cli/cli v2.96.0 pkg/cmd/api/api.go
		// processResponse and printHeaders; no configured executable or credential.
		for (const auto& [link, completeness] : std::array{
				 std::pair{ std::string{ }, page_completeness::complete },
				 std::pair{ std::string{ "Link: <https://api.github.com/repos/o/r?page=2>; rel=\"next\"\r\n" }, page_completeness::has_next_page },
				 std::pair{ std::string{ "Link: <https://api.github.com/repos/o/r?page=1>; rel=\"prev\"\r\n" }, page_completeness::complete } }) {
			process.Result      = github_cli_process_success{ .StandardOutput = std::string{ "HTTP/2.0 206 Partial Content\nContent-Type: application/json\r\n" }.append(link).append("X-Ratelimit-Remaining: 42\r\n\r\n").append(body) };
			const auto  result  = transport.perform(request, { });
			const auto* success = std::get_if<github_wire_success>(&result);
			expect(success != nullptr && success->Correlation == request.Correlation
					   && success->Response.Status == 206 && success->Response.Body == body
					   && success->Response.Completeness == completeness
					   && success->Response.Headers.back().Value == "42",
				   "actual status, body, headers and pagination survive");
			expect(gh_arguments(*process.LastRequest) == expected_arguments(*cloud, request), "perform executes closed include argv");
		}
		for (const auto& output : std::vector<std::string>{
				 "{}", "HTTP/2.0 200 OK\n{}", "HTTP/2.0 600 Error\n\r\n{}",
				 "HTTP/2.0 20x OK\n\r\n{}", "HTTP/2.0 200 OK\nBad Header: x\r\n\r\n{}",
				 "HTTP/2.0 200 OK\nContent-Type: a\r\ncontent-type: b\r\n\r\n{}",
				 "HTTP/2.0 200 OK\nLink: garbage\r\n\r\n{}",
				 "HTTP/2.0 200 OK\nX: a\nb\r\n\r\n{}" }) {
			process.Result     = github_cli_process_success{ .StandardOutput = output };
			const auto  result = transport.perform(request, { });
			const auto* error  = std::get_if<github_wire_error>(&result);
			expect(error != nullptr && error->Correlation == request.Correlation
					   && error->Error.Failure == application::github_provider_failure::malformed_response,
				   "missing response adapter or malformed included output cannot manufacture success");
		}
		process.Result = github_cli_process_success{ .StandardOutput = std::string(4U * 1024U * 1024U + 1U, 'x') };
		auto result    = transport.perform(request, { });
		expect(std::get<github_wire_error>(result).Error.Failure == application::github_provider_failure::response_limit, "untrusted process double cannot bypass output bound");
		for (const auto& [failure, expected] : std::array{
				 std::pair{ github_cli_process_failure::start_failure, application::github_provider_failure::process_startup },
				 std::pair{ github_cli_process_failure::timeout, application::github_provider_failure::timeout },
				 std::pair{ github_cli_process_failure::standard_output_limit, application::github_provider_failure::response_limit } }) {
			process.Result    = std::unexpected{ failure };
			result            = transport.perform(request, { });
			const auto* error = std::get_if<github_wire_error>(&result);
			expect(error != nullptr && error->Correlation == request.Correlation && error->Error.Failure == expected
					   && !error->Error.RetryAfterSeconds,
				   "process failure stays fixed and correlated without stdout");
		}
		process.Result = std::unexpected{ github_cli_process_failure::cancelled };
		expect(std::holds_alternative<github_wire_cancelled>(transport.perform(request, { })), "process cancellation remains cancellation");
		std::stop_source stop;
		process.StopOnRun = &stop;
		process.Result    = github_cli_process_success{ .StandardOutput = "HTTP/1.1 200 OK\n\r\n{}" };
		expect(std::get<github_wire_cancelled>(transport.perform(request, stop.get_token())).Correlation == request.Correlation, "late cancellation suppresses response");
		const auto calls = process.Calls;
		expect(std::holds_alternative<github_wire_cancelled>(transport.perform(request, stop.get_token())) && process.Calls == calls, "pre-cancelled request never launches");
		auto invalid       = request;
		invalid.Wire.Query = "page=2";
		expect(std::holds_alternative<github_wire_error>(transport.perform(invalid, { })) && process.Calls == calls, "invalid read never launches");
	}

	auto test_realistic_headers_project_provider_metadata() -> void
	{
		temporary_layout            layout;
		const auto                  configuration = validate_gh_cli_configuration(configuration_candidate(layout));
		const auto                  cloud         = validate_github_host_profile(cloud_candidate());
		successful_counting_process process;
		gh_cli_transport            transport{ *cloud, scoped_configuration(*configuration, *cloud), process, synthetic_operation_credential() };
		const auto                  request = wire_request_for(*cloud, read_operation::get_viewer_identity);
		for (const int status : { 200, 404 }) {
			const std::string body = status == 200 ? R"({"id":42,"login":"fixture","name":null,"node_id":"MDQ6VXNlcjQy"})" : R"({"message":"Not Found"})";
			process.Result         = github_cli_process_success{
				.StandardOutput = "HTTP/2.0 " + std::to_string(status) + " Response\nContent-Type: application/json\r\nDate: Sun, 13 Sep 2026 12:00:00 GMT\r\nServer: github.com\r\nX-Unknown: first\r\nX-Unknown: second\r\nContent-Length: " + std::to_string(body.size()) + "\r\nX-Ratelimit-Limit: 5000\r\nX-Ratelimit-Remaining: 42\r\nX-Ratelimit-Reset: 1700000000\r\nX-Ratelimit-Resource: core\r\n\r\n" + body,
				.ExitCode       = status == 200 ? 0 : 1,
			};
			const auto  completion = transport.perform(request, { });
			const auto* response   = std::get_if<github_wire_success>(&completion);
			expect(response != nullptr && response->Response.Headers.size() == 5U && response->Response.Body == body, "ordinary and duplicate unknown HTTP headers stay transport-local");
			const auto normalized = normalize_observed_github_viewer(response->Response);
			expect(status == 200 ? normalized.has_value() : (!normalized && normalized.error().Failure == application::github_provider_failure::permission), "realistic gh headers allow existing viewer and 404 normalization");
		}
	}

	auto test_completed_http_errors_preserve_provider_evidence() -> void
	{
		temporary_layout            layout;
		const auto                  configuration = validate_gh_cli_configuration(configuration_candidate(layout));
		const auto                  cloud         = validate_github_host_profile(cloud_candidate());
		successful_counting_process process;
		gh_cli_transport            transport{ *cloud, scoped_configuration(*configuration, *cloud), process, synthetic_operation_credential() };
		const auto                  request = wire_request_for(*cloud, read_operation::get_viewer_identity);
		for (const int status : { 401, 403, 429 }) {
			process.Result = github_cli_process_success{
				.StandardOutput = "HTTP/2.0 " + std::to_string(status) + " Error\nContent-Type: application/json\r\nX-Ratelimit-Limit: 5000\r\nX-Ratelimit-Remaining: 42\r\nX-Ratelimit-Reset: 1700000000\r\nX-Ratelimit-Resource: core\r\nRetry-After: 17\r\n\r\n{\"message\":\"fixture refusal\"}",
				.ExitCode       = 1,
			};
			const auto  completion = transport.perform(request, { });
			const auto* success    = std::get_if<github_wire_success>(&completion);
			expect(success != nullptr && success->Correlation == request.Correlation && success->Response.Status == status, "nonzero completed HTTP status reaches provider unchanged");
			const auto metadata = parse_github_response_metadata(success->Response);
			expect(metadata.has_value() && metadata->RetryAfterSeconds == 17U, "HTTP error retry metadata survives");
			const auto normalized = normalize_observed_github_viewer(success->Response);
			expect(!normalized.has_value() && normalized.error().Failure == (status == 401 ? application::github_provider_failure::authentication : application::github_provider_failure::rate_limit),
				   "HTTP denial reaches existing normalized classification");
			expect(process.LastRequest->PreserveCompletedOutput, "canonical API reads opt into bounded completed output");
		}
		const auto                                 wire = compile_capability_probe_request(*cloud);
		const github_capability_probe_wire_request probe{
			.Correlation = { .Part = 0U, .Session = session_parent(*cloud), .ProfileRevision = cloud->revision(), .Host = cloud->host(), .ObservedAccount = core::github_account_id{ 42 } },
			.Wire        = *wire,
		};
		const auto  limited         = transport.perform_capability_probe(probe, { });
		const auto* limitedResponse = std::get_if<github_capability_probe_wire_success>(&limited);
		expect(limitedResponse != nullptr && limitedResponse->Response.Status == 429
				   && limitedResponse->Response.RateLimitHeaderCount == 5U && limitedResponse->Response.RateLimitHeaders[4].Value == "17",
			   "probe preserves rate-limit status and retry delay");

		for (const auto* body : { R"({"errors":[{"type":"RATE_LIMITED","message":"fixture"}]})", R"({"data":{"partial":true}})", "{}" }) {
			process.Result        = github_cli_process_success{ .StandardOutput = std::string{ "HTTP/2.0 200 OK\nContent-Type: application/json\r\n\r\n" } + body, .ExitCode = 1 };
			const auto read       = transport.perform(request, { });
			const auto capability = transport.perform_capability_probe(probe, { });
			expect(std::get<github_wire_error>(read).Error.Failure == application::github_provider_failure::unavailable
					   && std::get<github_wire_error>(read).Correlation == request.Correlation,
				   "nonzero HTTP 200 fails atomically even with valid-looking JSON");
			expect(std::get<github_capability_probe_wire_error>(capability).Error.Failure == application::github_provider_failure::malformed_response,
				   "nonzero GraphQL 200 without causal metadata fails closed");
		}
		for (const auto* length : { "1", "3", "bad", "2, 2" }) {
			process.Result = github_cli_process_success{ .StandardOutput = std::string{ "HTTP/2.0 200 OK\nContent-Length: " } + length + "\r\n\r\n{}" };
			expect(std::get<github_wire_error>(transport.perform(request, { })).Error.Failure == application::github_provider_failure::malformed_response, "invalid or mismatched content length refuses body");
		}
		process.Result = github_cli_process_success{ .StandardOutput = "HTTP/2.0 200 OK\nContent-Length: 2\r\n\r\n{}" };
		expect(std::holds_alternative<github_wire_success>(transport.perform(request, { })), "exact content length permits clean response");

		process.Result = github_cli_process_success{ .StandardOutput = "fixed synthetic process failure without framing", .ExitCode = 1 };
		expect(std::get<github_wire_error>(transport.perform(request, { })).Error.Failure == application::github_provider_failure::malformed_response, "unframed nonzero output is redacted malformed response");
		expect(std::get<github_capability_probe_wire_error>(transport.perform_capability_probe(probe, { })).Error.Failure == application::github_provider_failure::malformed_response, "probe also rejects unframed nonzero output");
	}

	auto test_nonzero_graphql_errors() -> void
	{
		temporary_layout                           layout;
		const auto                                 configuration = validate_gh_cli_configuration(configuration_candidate(layout));
		const auto                                 cloud         = validate_github_host_profile(cloud_candidate());
		successful_counting_process                process;
		gh_cli_transport                           transport{ *cloud, scoped_configuration(*configuration, *cloud), process, synthetic_operation_credential() };
		const auto                                 request = wire_request_for(*cloud, read_operation::list_review_threads);
		const github_capability_probe_wire_request probe{
			.Correlation = { .Part = 0U, .Session = session_parent(*cloud), .ProfileRevision = cloud->revision(), .Host = cloud->host(), .ObservedAccount = core::github_account_id{ 42 } },
			.Wire        = *compile_capability_probe_request(*cloud),
		};
		const auto check = [&](const std::string& aHeaders, const std::string& aBody) {
			process.Result         = github_cli_process_success{ .StandardOutput = "HTTP/2.0 200 OK\nContent-Type: application/json\r\n" + aHeaders + "\r\n" + aBody, .ExitCode = 1 };
			const auto  read       = transport.perform(request, { });
			const auto  capability = transport.perform_capability_probe(probe, { });
			const auto& readError  = std::get<github_wire_error>(read);
			const auto& probeError = std::get<github_capability_probe_wire_error>(capability);
			expect(application::valid(readError.Error) && application::valid(probeError.Error), "every normalized CLI GraphQL error satisfies the application contract");
			expect(readError.Correlation == request.Correlation && probeError.Correlation == probe.Correlation, "GraphQL error retains exact correlation without partial success");
			expect(readError.Error == probeError.Error && readError.Error.HttpStatus == 200, "read and probe share typed HTTP 200 failure normalization");
			return readError.Error;
		};
		const std::string headers = "X-Ratelimit-Limit: 5000\r\nX-Ratelimit-Remaining: 42\r\nX-Ratelimit-Reset: 1700000000\r\nX-Ratelimit-Resource: graphql\r\n";
		for (const auto* body : { R"({"errors":[{"type":"RATE_LIMITED","message":"private fixture"}],"data":{"partial":true}})", R"({"errors":[{"extensions":{"code":"RATE_LIMITED"}}]})", R"({"errors":[{"extensions":{"type":"RATE_LIMITED"}}]})" }) {
			const auto error = check(headers, body);
			expect(error.Failure == application::github_provider_failure::rate_limit && error.RetryAfterSeconds == 60U && error.RateLimitCause == error.RateLimits.Graphql && error.RateLimitCause->SecondaryLimited && error.RateLimitCause->Remaining == 42U && error.RateLimitCause->ResetUnixSeconds == 1700000000U && !error.RateLimits.Core && !error.RateLimits.Search, "structured GraphQL throttle retains exact causal resource and bounded retry");
			expect(check(headers + "Retry-After: 17\r\n", body).RetryAfterSeconds == 17U, "explicit retry takes precedence over secondary default");
		}
		for (const auto* body : { R"({"errors":[{"type":"FORBIDDEN","message":"private fixture"}]})", R"({"errors":"RATE_LIMITED"})", R"({"errors":[{"type":42}]})", R"({"errors":[)", R"({"data":{"partial":true}})", "{}" }) {
			const auto error = check(headers, body);
			expect(error.Failure == application::github_provider_failure::malformed_response && error.RateLimits.Graphql && !error.RateLimitCause && !error.RetryAfterSeconds, "arbitrary or malformed GraphQL failure retains valid observation but no fabricated throttle");
		}
		const auto oversized = check(headers, std::string(256U, '[') + "0" + std::string(256U, ']'));
		expect(oversized.Failure == application::github_provider_failure::response_limit, "completed GraphQL error parsing enforces structural depth limit");
		const std::string rateBody = R"({"errors":[{"type":"RATE_LIMITED"}]})";
		for (const auto* metadata : { "", "X-Ratelimit-Remaining: 0\r\n", "X-Ratelimit-Limit: 5000\r\nX-Ratelimit-Remaining: 0\r\nX-Ratelimit-Reset: 1700000000\r\nX-Ratelimit-Resource: core\r\n" }) {
			const auto error = check(metadata, rateBody);
			expect(error.Failure == application::github_provider_failure::malformed_response && !error.RateLimitCause && !error.RateLimits.Graphql, "missing partial or wrong-resource tuple cannot fabricate causal GraphQL evidence");
		}
		const auto primary = check("X-Ratelimit-Limit: 5000\r\nX-Ratelimit-Remaining: 0\r\nX-Ratelimit-Reset: 1700000000\r\nX-Ratelimit-Resource: graphql\r\n", rateBody);
		expect(primary.Failure == application::github_provider_failure::rate_limit && !primary.RateLimitCause->SecondaryLimited && !primary.RetryAfterSeconds, "exhausted primary budget retains reset without secondary retry");
	}

	auto test_configuration_file_metadata_changes() -> void
	{
		temporary_layout            layout;
		const auto                  configuration = validate_gh_cli_configuration(configuration_candidate(layout));
		const auto                  cloud         = validate_github_host_profile(cloud_candidate());
		successful_counting_process process;
		const auto                  request = wire_request_for(*cloud, read_operation::get_repository);
		gh_cli_transport            unpinned{ *cloud, *configuration, process };
		const auto                  refused = unpinned.perform(request, { });
		expect(std::holds_alternative<github_wire_error>(refused) && process.Calls == 0U, "unpinned account choice refuses before any API or credential retrieval");
#if !defined(_WIN32)
		const auto directory = layout.HomeDirectory / ".config" / "gh";
		std::filesystem::create_directories(directory);
		gh_cli_transport pinned{ *cloud, scoped_configuration(*configuration, *cloud), process, synthetic_operation_credential() };
		for (const auto* name : { "config.yml", "hosts.yml" }) {
			std::ofstream output{ directory / name };
			output << "ambient unrelated configuration";
			output.close();
			expect(std::holds_alternative<github_wire_success>(pinned.perform(request, { })), "private pinned execution never borrows ambient gh configuration");
		}
#endif
	}

	auto test_detected_configuration_change_refuses_launch() -> void
	{
		temporary_layout            layout;
		const auto                  configuration = validate_gh_cli_configuration(configuration_candidate(layout));
		const auto                  cloud         = validate_github_host_profile(cloud_candidate());
		successful_counting_process process;
		gh_cli_transport            transport{ *cloud, scoped_configuration(*configuration, *cloud), process, synthetic_operation_credential() };
		const auto                  request = wire_request_for(*cloud, read_operation::get_repository);
		{
			std::ofstream changed{ configuration->executable(), std::ios::app };
			changed << "changed fixture bytes";
		}
		const auto result = transport.perform(request, { });
		expect(std::get<github_wire_error>(result).Error.Failure == application::github_provider_failure::cli_unavailable && process.Calls == 0U, "detected executable change refuses launch");
		const auto                                 wire = compile_capability_probe_request(*cloud);
		const github_capability_probe_wire_request probe{
			.Correlation = { .Part = 0U, .Session = session_parent(*cloud), .ProfileRevision = cloud->revision(), .Host = cloud->host(), .ObservedAccount = core::github_account_id{ 42 } },
			.Wire        = *wire,
		};
		expect(std::get<github_capability_probe_wire_error>(transport.perform_capability_probe(probe, { })).Error.Failure == application::github_provider_failure::cli_unavailable && process.Calls == 0U, "probe detects executable change too");
		gh_cli_transport newTransport{ *cloud, scoped_configuration(*configuration, *cloud), process, synthetic_operation_credential() };
		std::error_code  error;
		std::filesystem::create_directory(configuration->neutral_working_directory() / ".git", error);
		expect(!error, "working directory fixture becomes repository");
		expect(std::get<github_wire_error>(newTransport.perform(request, { })).Error.Failure == application::github_provider_failure::cli_unavailable && process.Calls == 0U, "working directory repository change refuses launch");
	}

	[[nodiscard]] auto mutation_request_for(
		const validated_github_host_profile& aProfile)
		-> github_mutation_wire_request
	{
		const auto context = pull_request_parent(aProfile);
		const auto head    = *core::parse_git_commit_oid(
			"1111111111111111111111111111111111111111");
		const auto base = *core::parse_git_commit_oid(
			"2222222222222222222222222222222222222222");
		const auto target = application::review_mutation_target_binding{
			.Semantic = {
				.Host         = context.Host,
				.Repository   = *context.ExpectedRepository,
				.PullRequest  = *context.ExpectedPullRequest,
				.ExpectedHead = head,
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
			.BaseOid                  = base,
			.BaseRef                  = "main",
			.HeadRef                  = "feature/review",
			.Open                     = true,
		};
		const auto identity = application::review_mutation_acting_identity{
			.Account    = context.Account,
			.DatabaseId = 22U,
			.NodeId     = "MDQ6VXNlcjIy",
			.Login      = "fixture-reviewer",
		};
		const auto proposal = core::review_publication_proposal{
			.Draft  = core::review_draft_id{ 31U },
			.Anchor = {
				.Comparison = core::file_comparison_digest{ std::array<std::uint8_t, 32>{ 0x22U } },
				.Side       = core::diff_side::right,
				.Line       = 12U,
			},
			.Body        = "Please rename this.",
			.Disposition = core::review_disposition::comment,
		};
		const auto semantic = application::review_mutation_request{
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
					.Commit   = head,
				},
			},
		};
		const auto wire = compile_review_comment_request(
			semantic, aProfile.server_contract());
		expect(wire.has_value(), "fixture-mutation-wire");
		return {
			.Context = application::github_credential_operation_context{ .Request = context.Request, .ApplicationSession = context.ApplicationSession, .Authority = { .Profile = aProfile.profile(), .ProfileRevision = aProfile.revision(), .Host = aProfile.host(), .Hostname = application::github_canonical_hostname{ aProfile.hostname() }, .Account = context.Account, .ServerContract = aProfile.server_contract() }, .IdentityGeneration = context.IdentityGeneration },
			.Wire    = *wire,
		};
	}

	struct fake_recovery_state
	{
		std::map<std::string, application::review_mutation_recovery_record> Records;
		bool                                                                RejectTransmissionMark = false;
	};

	class fake_recovery final : public application::review_mutation_recovery_port
	{
	public:
		explicit fake_recovery(std::shared_ptr<fake_recovery_state> aState)
			: State{ std::move(aState) }
		{
		}

		auto load(const std::string& aKey)
			-> std::expected<
				std::optional<application::review_mutation_recovery_record>,
				application::review_mutation_recovery_failure> override
		{
			if (const auto found = State->Records.find(aKey);
				found != State->Records.end()) {
				return found->second;
			}
			return std::optional<application::review_mutation_recovery_record>{ };
		}

		auto admit(const application::review_mutation_recovery_record& aRecord)
			-> std::expected<
				application::review_mutation_recovery_admission,
				application::review_mutation_recovery_failure> override
		{
			if (const auto found = State->Records.find(aRecord.IdempotencyKey);
				found != State->Records.end()) {
				return application::review_mutation_recovery_admission{
					.Record = found->second,
				};
			}
			State->Records.emplace(aRecord.IdempotencyKey, aRecord);
			return application::review_mutation_recovery_admission{
				.Record   = aRecord,
				.Inserted = true,
			};
		}

		auto mark_possibly_transmitted(
			const std::string&                  aKey,
			const std::array<std::uint8_t, 32>& aFingerprint)
			-> std::expected<
				application::review_mutation_recovery_record,
				application::review_mutation_recovery_failure> override
		{
			if (State->RejectTransmissionMark) {
				return std::unexpected(
					application::review_mutation_recovery_failure::unavailable);
			}
			const auto found = State->Records.find(aKey);
			if (found == State->Records.end()
				|| found->second.RequestFingerprint != aFingerprint
				|| !found->second.BaselineExactCommentIds.has_value()
				|| found->second.State
					!= application::review_mutation_recovery_state::unresolved) {
				return std::unexpected(
					application::review_mutation_recovery_failure::conflict);
			}
			found->second.Transmission
				= application::review_mutation_recovery_transmission::possibly_transmitted;
			return found->second;
		}

		auto resolve(
			const std::string&                          aKey,
			const std::array<std::uint8_t, 32>&         aFingerprint,
			const application::review_mutation_receipt& aReceipt)
			-> std::expected<
				application::review_mutation_recovery_record,
				application::review_mutation_recovery_failure> override
		{
			const auto found = State->Records.find(aKey);
			if (found == State->Records.end()
				|| found->second.RequestFingerprint != aFingerprint
				|| !found->second.BaselineExactCommentIds.has_value()
				|| found->second.Transmission
					!= application::review_mutation_recovery_transmission::possibly_transmitted) {
				return std::unexpected(
					application::review_mutation_recovery_failure::conflict);
			}
			if (std::ranges::binary_search(
					*found->second.BaselineExactCommentIds,
					aReceipt.ServerCommentId)) {
				return std::unexpected(
					application::review_mutation_recovery_failure::conflict);
			}
			found->second.State   = application::review_mutation_recovery_state::succeeded;
			found->second.Receipt = aReceipt;
			return found->second;
		}

	private:
		std::shared_ptr<fake_recovery_state> State;
	};

	class fixture_mutation_lease final : public application::review_mutation_transport_lease
	{ };

	[[maybe_unused]] void test_windows_pinned_transport_refuses_before_effects()
	{
		for (const auto& originalCandidate : { cloud_candidate(), enterprise_candidate() }) {
			auto             candidate = originalCandidate;
			temporary_layout layout;
			const auto       configuration      = validate_gh_cli_configuration(configuration_candidate(layout));
			candidate.CliAccountLogin           = "fixture-reviewer";
			const auto                  profile = validate_github_host_profile(candidate);
			const auto                  pinned  = pin_gh_cli_configuration(*configuration, profile->hostname(), candidate.CliAccountLogin);
			successful_counting_process process;
			process.EnableAcquisition = true;
			const auto acquired       = acquire_gh_cli_credential(process, *pinned, { });
			expect(!acquired && acquired.error().Failure == github_cli_process_failure::start_failure && !acquired.error().CredentialUnavailable,
				   "Windows refuses the unsupported private configuration before named credential lookup");
			gh_cli_transport transport{ *profile, *pinned, process };
			const auto       readRequest = wire_request_for(*profile, read_operation::get_repository);
			const auto       read        = transport.perform(readRequest, { });
			const auto*      readError   = std::get_if<github_wire_error>(&read);
			expect(readError && readError->Correlation == readRequest.Correlation
					   && readError->Error.Failure == application::github_provider_failure::process_startup && !readError->Error.CliCredentialUnavailable,
				   "Windows pinned read preserves the prelaunch tool refusal, never a missing credential");
			const auto operation = transport.begin_mutation_operation({ });
			expect(!operation && operation.error() == application::review_mutation_failure::credential_tool_failed,
				   "Windows mutation acquisition preserves the unsupported tool refusal");
			const auto                                 wire = compile_capability_probe_request(*profile);
			const github_capability_probe_wire_request probe{
				.Correlation = { .Part = 0U, .Session = session_parent(*profile), .ProfileRevision = profile->revision(), .Host = profile->host(), .ObservedAccount = core::github_account_id{ 42 } },
				.Wire        = *wire,
			};
			const auto  capability      = transport.perform_capability_probe(probe, { });
			const auto* capabilityError = std::get_if<github_capability_probe_wire_error>(&capability);
			expect(capabilityError && capabilityError->Correlation == probe.Correlation
					   && capabilityError->Error.Failure == application::github_provider_failure::process_startup && !capabilityError->Error.CliCredentialUnavailable,
				   "Windows pinned capability read refuses before an API request");
			auto        witnesses     = 0U;
			const auto  mutation      = transport.perform_review_mutation(mutation_request_for(*profile), { }, [&] { ++witnesses; return true; });
			const auto* mutationError = std::get_if<github_mutation_wire_error>(&mutation);
			expect(mutationError && mutationError->Transmission == github_mutation_transmission::definitely_not_sent
					   && witnesses == 0U && process.Acquisitions == 0U && process.Calls == 0U && process.AcquiredCredential.expired()
					   && std::filesystem::is_empty(configuration->neutral_working_directory()),
				   "Windows unsupported pinned transport creates no credential, API call, transmission witness or private configuration");
		}
		std::cout << "GH_CLI_WINDOWS_PINNED_REFUSAL_OK: github.com and GHES, zero sensitive/API/witness effects\n";
	}

	[[maybe_unused]] void test_strict_acquisition_failures()
	{
		temporary_layout layout;
		const auto       configuration = validate_gh_cli_configuration(configuration_candidate(layout));
		auto             candidate     = cloud_candidate();
		candidate.CliAccountLogin      = "fixture-reviewer";
		const auto profile             = validate_github_host_profile(candidate);
		const auto pinned              = pin_gh_cli_configuration(*configuration, profile->hostname(), candidate.CliAccountLogin);
		for (const auto failure : { github_cli_process_failure::nonzero_exit, github_cli_process_failure::timeout,
									github_cli_process_failure::process_crash, github_cli_process_failure::reap_failure,
									github_cli_process_failure::cancelled, github_cli_process_failure::start_failure,
									github_cli_process_failure::standard_output_limit, github_cli_process_failure::invalid_request }) {
			successful_counting_process process;
			process.EnableAcquisition  = true;
			process.AcquisitionFailure = failure;
			gh_cli_transport transport{ *profile, *pinned, process };
			const auto       read  = transport.perform(wire_request_for(*profile, read_operation::get_repository), { });
			const auto*      error = std::get_if<github_wire_error>(&read);
			expect(error != nullptr && error->Error.CliCredentialUnavailable == (failure == github_cli_process_failure::nonzero_exit), "only completed named lookup refusal reports credential unavailable on reads");
			expect((error->Error.Failure == application::github_provider_failure::authentication) == error->Error.CliCredentialUnavailable, "timeout, crash, reap, cancellation and malformed acquisition are tool failures, never authentication");
			if (failure == github_cli_process_failure::timeout) {
				expect(error->Error.Failure == application::github_provider_failure::timeout, "read acquisition keeps typed timeout");
			}
			const auto              wire    = mutation_request_for(*profile);
			const auto              request = std::get<application::review_mutation_request>(wire.Wire.Semantic);
			const auto              binding = *request.LiveBinding;
			review_mutation_session session{ transport, { .CredentialContext = wire.Context, .Target = binding.Target, .ActingIdentity = binding.ActingIdentity }, std::make_shared<fixture_mutation_lease>(), std::make_unique<fake_recovery>(std::make_shared<fake_recovery_state>()) };
			const auto              refused  = session.submit(request);
			const auto              expected = failure == github_cli_process_failure::nonzero_exit ? application::review_mutation_failure::credential_unavailable
				: failure == github_cli_process_failure::cancelled                                 ? application::review_mutation_failure::cancelled
																								   : application::review_mutation_failure::credential_tool_failed;
			expect(!refused && refused.error() == expected && process.Calls == 0U, "inline strict acquisition distinguishes cancellation, credential refusal and tool failure with zero API launches");
		}
		for (const auto failure : { github_cli_process_failure::nonzero_exit, github_cli_process_failure::timeout,
									github_cli_process_failure::process_crash, github_cli_process_failure::reap_failure, github_cli_process_failure::cancelled }) {
			successful_counting_process process;
			process.EnableAcquisition = true;
			process.ProbeFailure      = failure;
			gh_cli_transport transport{ *profile, *pinned, process };
			const auto       read  = transport.perform(wire_request_for(*profile, read_operation::get_repository), { });
			const auto*      error = std::get_if<github_wire_error>(&read);
			expect(error && !error->Error.CliCredentialUnavailable && error->Error.Failure != application::github_provider_failure::authentication
					   && process.Acquisitions == 0U && process.Calls == 0U,
				   "a failed local capability probe never pretends that a named credential is missing");
		}
	}

	[[maybe_unused]] void test_cli_mutation_session_recovery()
	{
		for (const bool mismatch : { false, true }) {
			temporary_layout layout;
			const auto       configuration                   = validate_gh_cli_configuration(configuration_candidate(layout));
			auto             candidate                       = cloud_candidate();
			candidate.CliAccountLogin                        = "fixture-reviewer";
			const auto                               profile = validate_github_host_profile(candidate);
			const auto                               pinned  = pin_gh_cli_configuration(*configuration, profile->hostname(), candidate.CliAccountLogin);
			std::stop_source                         stop;
			const auto                               wire    = mutation_request_for(*profile);
			const auto                               request = std::get<application::review_mutation_request>(wire.Wire.Semantic);
			const auto                               binding = *request.LiveBinding;
			const live_review_mutation_configuration sessionConfiguration{ .CredentialContext = wire.Context, .Target = binding.Target, .ActingIdentity = binding.ActingIdentity };
			auto                                     state = std::make_shared<fake_recovery_state>();
			successful_counting_process              process;
			process.EnableAcquisition = true;
			unsigned posts            = 0;
			unsigned gets             = 0;
			process.OnRun             = [&] {
				const auto args = gh_arguments(*process.LastRequest);
				if (args[6] == "POST") {
					++posts;
					process.MissingCredential = true;
					stop.request_stop();
					expect(state->Records.at(request.IdempotencyKey).Transmission == application::review_mutation_recovery_transmission::possibly_transmitted, "recovery witness is recorded before actual CLI POST dispatch");
					expect(process.LastRequest->StandardInput == wire.Wire.Body, "session dispatches canonical inline body via CLI stdin");
					process.Result = std::unexpected{ github_cli_process_failure::timeout };
					return;
				}
				++gets;
				std::string body = "[]";
				if (args[1] == "user") {
					body = mismatch ? R"({"id":23,"node_id":"U_other","login":"other"})" : R"({"id":22,"node_id":"MDQ6VXNlcjIy","login":"fixture-reviewer"})";
				} else if (args[1].find("comments") == std::string::npos) {
					body = R"({"id":4246446933,"node_id":"PR_kwDOTyvof879G6NV","number":17,"state":"open","head":{"sha":"1111111111111111111111111111111111111111","ref":"feature/review"},"base":{"sha":"2222222222222222222222222222222222222222","ref":"main","repo":{"id":1328277631,"node_id":"R_kgDOTyvofw","full_name":"octo-org/review-repository"}}})";
				}
				process.Result = github_cli_process_success{ .StandardOutput = "HTTP/2.0 200 OK\nContent-Type: application/json\r\n\r\n" + body };
			};
			{
				gh_cli_transport        transport{ *profile, *pinned, process };
				review_mutation_session session{ transport, sessionConfiguration, std::make_shared<fixture_mutation_lease>(), std::make_unique<fake_recovery>(state) };
				const auto              result = session.submit(request, stop.get_token());
				expect(!result, "mismatch or timed out CLI POST cannot report success");
				expect(process.Acquisitions == 1U, "one acquisition covers validation, POST and cancellation reconciliation");
				expect(posts == (mismatch ? 0U : 1U), "observed account mismatch prevents actual CLI POST");
			}
			if (mismatch) {
				continue;
			}
			expect(state->Records.at(request.IdempotencyKey).Transmission == application::review_mutation_recovery_transmission::possibly_transmitted, "ambiguous attempt retains possible transmission across session destruction");
			expect(process.AcquiredCredential.expired(), "completed operation retains no credential in the session");
			const auto previousGets = gets;
			{
				gh_cli_transport        unavailableTransport{ *profile, *pinned, process };
				review_mutation_session unavailable{ unavailableTransport, sessionConfiguration, std::make_shared<fixture_mutation_lease>(), std::make_unique<fake_recovery>(state) };
				const auto              refused = unavailable.submit(request);
				expect(!refused && refused.error() == application::review_mutation_failure::credential_unavailable && gets == previousGets && posts == 1U, "a later missing credential refuses before API without another POST");
			}
			process.MissingCredential = false;
			gh_cli_transport        restartedTransport{ *profile, *pinned, process };
			review_mutation_session restarted{ restartedTransport, sessionConfiguration, std::make_shared<fixture_mutation_lease>(), std::make_unique<fake_recovery>(state) };
			const auto              retried = restarted.submit(request);
			expect(!retried && gets > previousGets && posts == 1U, "recreated session reconciles via CLI GET without second POST");
			const auto retriedAgain = restarted.submit(request);
			expect(!retriedAgain && posts == 1U, "repeated unresolved retry never launches second CLI POST");
		}
	}

	[[maybe_unused]] void test_mutation_closed_shapes_and_ambiguous_results()
	{
		for (const auto& candidate : { cloud_candidate(), enterprise_candidate() }) {
			temporary_layout                layout;
			const auto                      configuration = validate_gh_cli_configuration(configuration_candidate(layout));
			const auto                      profile       = validate_github_host_profile(candidate);
			successful_counting_process     process;
			gh_cli_transport                transport{ *profile, scoped_configuration(*configuration, *profile), process, synthetic_operation_credential() };
			auto                            request         = mutation_request_for(*profile);
			const auto                      semanticRequest = std::get<application::review_mutation_request>(request.Wire.Semantic);
			const auto                      binding         = *semanticRequest.LiveBinding;
			const review_submission_request review{ .Target = binding.Target, .ActingIdentity = binding.ActingIdentity, .Body = "Fixture review" };
			for (const auto& observation : { compile_target_observation_request(binding.Target, profile->server_contract()), compile_viewer_observation_request(binding.ActingIdentity, profile->server_contract()), compile_review_comment_observation_request(semanticRequest, profile->server_contract()), compile_review_submission_observation_request(review, profile->server_contract()) }) {
				expect(observation.has_value(), "reconciliation observation compiles");
				const auto result = transport.perform_review_mutation({ .Context = request.Context, .Wire = *observation }, { }, { });
				expect(std::holds_alternative<github_mutation_wire_success>(result) && gh_arguments(*process.LastRequest)[6] == "GET" && process.LastRequest->StandardInput.empty(), "reconciliation uses bounded GET without write witness");
				// [[BPR-1538]]. The review list and review comment reads may
				// carry a full page of GitHub's objects; everything else keeps
				// the ordinary bound.
				const auto listed = observation->Operation == review_mutation_operation::observe_reviews || observation->Operation == review_mutation_operation::observe_review_comments;
				expect(process.LastRequest->StandardOutputLimit == (listed ? maximum_review_mutation_list_response_bytes : maximum_review_mutation_response_bytes) + 32U * 1024U,
					   "the standard output bound is the operation's answer bound");
			}
			{
				const auto page = compile_review_comment_collection_request(binding.Target, profile->server_contract(), 2U);
				expect(page.has_value() && std::holds_alternative<github_mutation_wire_success>(transport.perform_review_mutation({ .Context = request.Context, .Wire = *page }, { }, { }))
						   && process.LastRequest->StandardOutputLimit == maximum_review_mutation_list_response_bytes + 32U * 1024U,
					   "[[BPR-1538]] a page of the comment collection may carry a full page of GitHub's comments");
			}
			// Every GraphQL-rooted shape is refused before launch: the four
			// the milestone never admitted, and [[BPR-0896]]'s thread
			// reconciliation of a reply, which nothing this transport wrote
			// can need.
			auto reply                   = semanticRequest;
			reply.Proposal.ReplyToThread = "PRRT_fixture_1";
			reply.IdempotencyKey         = application::idempotency_key_for(reply.Proposal, reply.Target);
			const auto resolution        = application::review_thread_resolution_request{
				.Target         = binding.Target.Semantic,
				.ActingIdentity = binding.ActingIdentity.Account,
				.Thread         = "PRRT_fixture_1",
				.Resolve        = true,
				.LiveBinding    = binding,
			};
			auto unresolution    = resolution;
			unresolution.Resolve = false;
			for (const auto& graphql : { compile_review_comment_request(reply, profile->server_contract()), compile_review_comment_observation_request(reply, profile->server_contract()), compile_thread_resolution_request(resolution, profile->server_contract()), compile_thread_resolution_request(unresolution, profile->server_contract()), compile_thread_target_observation_request(resolution, profile->server_contract()) }) {
				expect(graphql.has_value() && graphql->Path.ends_with("graphql"), "GraphQL-rooted shape compiles at the GraphQL root");
				const auto launches = process.Calls;
				const auto refused  = transport.perform_review_mutation({ .Context = request.Context, .Wire = *graphql }, { }, [] { return true; });
				expect(std::holds_alternative<github_mutation_wire_error>(refused) && process.Calls == launches, "a GraphQL-rooted shape never launches the CLI");
			}

			for (bool wholeReview : { false, true }) {
				if (wholeReview) {
					request.Wire = *compile_review_submission_request({ .Target = binding.Target, .ActingIdentity = binding.ActingIdentity, .Disposition = core::review_disposition::approve, .Body = "Fixture approval" }, profile->server_contract());
				}
				std::size_t witnessed = 0;
				const auto  witness   = [&] { ++witnessed; return true; };
				const auto  before    = process.Calls;
				expect(std::holds_alternative<github_mutation_wire_error>(transport.perform_review_mutation(request, { }, { })) && process.Calls == before, "missing durable witness prevents POST");
				process.Result = github_cli_process_success{ .StandardOutput = "HTTP/2.0 201 Created\nContent-Type: application/json\r\n\r\n{}" };
				expect(std::holds_alternative<github_mutation_wire_success>(transport.perform_review_mutation(request, { }, witness)) && witnessed == 1, "canonical mutation launches after witness");
				const auto args = gh_arguments(*process.LastRequest);
				expect(args[0] == "api" && args[1].starts_with("repos/") && args[3] == profile->hostname() && args[6] == "POST" && args.back() == "-" && process.LastRequest->StandardInput == request.Wire.Body, "mutation uses exact host REST endpoint and stdin body");
				for (const auto& arg : args) {
					expect(arg.find(request.Wire.Body) == std::string::npos, "body absent from arguments");
				}
				for (auto failure : { github_cli_process_failure::timeout, github_cli_process_failure::cancelled }) {
					process.Result    = std::unexpected{ failure };
					const auto result = transport.perform_review_mutation(request, { }, witness);
					if (const auto* error = std::get_if<github_mutation_wire_error>(&result)) {
						expect(error->Transmission == github_mutation_transmission::indeterminate, "timeout retains possible transmission");
					} else {
						expect(std::get<github_mutation_wire_cancelled>(result).Transmission == github_mutation_transmission::indeterminate, "cancelled launch retains possible transmission");
					}
				}
				for (const auto& output : { std::string{ "unframed synthetic output" }, std::string{ "HTTP/2.0 201 Created\n\r\n{}" } }) {
					process.Result    = github_cli_process_success{ .StandardOutput = output, .ExitCode = 1 };
					const auto result = transport.perform_review_mutation(request, { }, witness);
					expect(std::get<github_mutation_wire_error>(result).Transmission == github_mutation_transmission::indeterminate, "malformed or nonzero success remains ambiguous");
				}
				std::stop_source stopped;
				stopped.request_stop();
				const auto stoppedCalls = process.Calls;
				expect(std::get<github_mutation_wire_cancelled>(transport.perform_review_mutation(request, stopped.get_token(), witness)).Transmission == github_mutation_transmission::definitely_not_sent && process.Calls == stoppedCalls, "prelaunch cancellation never starts process");

				auto drift = request;
				drift.Wire.Body += ' ';
				const auto calls = process.Calls;
				expect(std::holds_alternative<github_mutation_wire_error>(transport.perform_review_mutation(drift, { }, witness)) && calls == process.Calls, "body drift cannot launch");
				drift                           = request;
				drift.Context.Authority.Account = core::github_account_id{ 999 };
				expect(std::holds_alternative<github_mutation_wire_error>(transport.perform_review_mutation(drift, { }, witness)) && calls == process.Calls, "acting account mismatch cannot launch");
			}
		}
	}

	[[maybe_unused]] void test_staged_cli_catalog()
	{
		using operation = application::staged_review_operation;
		const std::array rows{
			std::pair{ operation::list_reviews, "GET" }, std::pair{ operation::read_review, "GET" }, std::pair{ operation::list_comments, "GET" },
			std::pair{ operation::create_pending, "POST" }, std::pair{ operation::update_comment, "PATCH" }, std::pair{ operation::update_summary, "PUT" }, std::pair{ operation::submit_comment, "POST" }, std::pair{ operation::discard_pending, "DELETE" }, std::pair{ operation::update_description, "PATCH" }, std::pair{ operation::list_issue_comments, "GET" }, std::pair{ operation::create_issue_comment, "POST" }, std::pair{ operation::update_issue_comment, "PATCH" },
			std::pair{ operation::update_pending_comment, "POST" }
		};
		for (const auto& candidate : { cloud_candidate(), enterprise_candidate() }) {
			temporary_layout            layout;
			const auto                  configuration = validate_gh_cli_configuration(configuration_candidate(layout));
			const auto                  profile       = validate_github_host_profile(candidate);
			successful_counting_process process;
			gh_cli_transport            transport{ *profile, scoped_configuration(*configuration, *profile), process, synthetic_operation_credential() };
			const auto                  base    = mutation_request_for(*profile);
			const auto                  binding = *std::get<application::review_mutation_request>(base.Wire.Semantic).LiveBinding;
			for (const auto& [staged, method] : rows) {
				application::staged_review_request semantic{ .Binding = { binding.Target, binding.ActingIdentity }, .Operation = staged };
				const std::string                  text = "Literal `$(touch /nope)`; & \"quoted\"\nsecond line";
				if (staged == operation::read_review || staged == operation::list_comments || staged == operation::update_summary || staged == operation::submit_comment || staged == operation::discard_pending) {
					semantic.Review = application::review_database_id{ 18446744073709551615ULL };
				}
				if (staged == operation::update_comment || staged == operation::update_issue_comment || staged == operation::update_pending_comment) {
					semantic.Comment = 18446744073709551614ULL;
				}
				if (staged == operation::update_pending_comment) {
					semantic.Review      = application::review_database_id{ 18446744073709551615ULL };
					semantic.CommentNode = "PRRC_kwDOTyvof86tA";
				}
				const bool write = std::string_view{ method } != "GET";
				const bool body  = write && std::string_view{ method } != "DELETE";
				if (body) {
					semantic.Body = text;
				}
				if (staged == operation::create_pending) {
					semantic.Lines.push_back({ { .Commit = binding.Target.Semantic.ExpectedHead, .Path = "file.cpp", .FirstLine = 2, .LastLine = 4 }, text });
				}
				const auto compiled = compile_staged_review_request(semantic);
				expect(compiled.has_value(), "finite staged CLI operation compiles");
				github_mutation_wire_request request{ .Context = base.Context, .Wire = *compiled };
				const auto                   calls = process.Calls;
				if (write) {
					expect(std::holds_alternative<github_mutation_wire_error>(transport.perform_review_mutation(request, { }, { })) && process.Calls == calls, "every staged write including DELETE requires witness");
				}
				std::size_t witnesses = 0;
				const auto  witness   = [&] {++witnesses;return true; };
				process.Result        = github_cli_process_success{ .StandardOutput = "HTTP/2.0 200 OK\r\nContent-Type: application/json\r\n\r\n{\"id\":18446744073709551615}" };
				const auto result     = transport.perform_review_mutation(request, { }, witness);
				expect(std::holds_alternative<github_mutation_wire_success>(result) && witnesses == (write ? 1U : 0U), "finite staged verb dispatch consumes exact write witness");
				const auto args = gh_arguments(*process.LastRequest);
				expect(args[0] == "api" && args[3] == profile->hostname() && args[6] == method && process.LastRequest->StandardInput == compiled->Body, "CLI uses exact verb hostname and untouched stdin");
				// [[BPR-1368]]. The pending stop edit, and nothing else, goes to the host's GraphQL root.
				expect((args[1] == "graphql") == (staged == operation::update_pending_comment), "only the pending stop edit uses gh api graphql");
				expect((std::ranges::find(args, "--input") != args.end()) == body, "only nonempty typed JSON uses --input -");
				expect(std::ranges::none_of(args, [&](const auto& aArgument) { return aArgument.contains("touch /nope"); }), "shell metacharacters never enter argv or a shell");
				expect(std::get<github_mutation_wire_success>(result).Response.Body == "{\"id\":18446744073709551615}", "normalization preserves all integer bytes");
				for (const auto status : { 301, 401, 403, 422, 429, 503 }) {
					process.Result    = github_cli_process_success{ .StandardOutput = "HTTP/2.0 " + std::to_string(status) + " status\r\n\r\n{}", .ExitCode = 1 };
					const auto answer = transport.perform_review_mutation(request, { }, witness);
					expect(std::holds_alternative<github_mutation_wire_success>(answer) && std::get<github_mutation_wire_success>(answer).Response.Status == status, "CLI preserves HTTP refusal for shared parser without redirect followup");
				}
				auto changed                      = request;
				changed.Context.Authority.Account = core::github_account_id{ 999 };
				const auto before                 = process.Calls;
				expect(std::holds_alternative<github_mutation_wire_error>(transport.perform_review_mutation(changed, { }, witness)) && process.Calls == before, "staged actor mismatch cannot launch");
				std::stop_source stop;
				stop.request_stop();
				expect(std::holds_alternative<github_mutation_wire_cancelled>(transport.perform_review_mutation(request, stop.get_token(), witness)) && process.Calls == before, "cancelled staged operation never launches");
			}
		}
	}

	auto test_capability_probe_uses_closed_query_and_response() -> void
	{
		temporary_layout            layout;
		const auto                  configuration = validate_gh_cli_configuration(configuration_candidate(layout));
		const auto                  cloud         = validate_github_host_profile(cloud_candidate());
		successful_counting_process process;
		gh_cli_transport            transport{ *cloud, scoped_configuration(*configuration, *cloud), process, synthetic_operation_credential() };
		for (std::uint8_t part = 0U; part < capability_probe_part_count; ++part) {
			const auto wire = compile_capability_probe_request(*cloud, { }, part);
			expect(wire.has_value(), "probe compiles");
			github_capability_probe_wire_request request{
				.Correlation = { .Part = part, .Session = session_parent(*cloud), .ProfileRevision = cloud->revision(), .Host = cloud->host(), .ObservedAccount = core::github_account_id{ 42 } },
				.Wire        = *wire,
			};
			const auto  result  = transport.perform_capability_probe(request, { });
			const auto* success = std::get_if<github_capability_probe_wire_success>(&result);
			expect(success != nullptr && success->Correlation == request.Correlation && success->Response.Status == 200
					   && success->Response.Authenticated && success->Response.ContentType == "application/json" && success->Response.Body == "{}",
				   "probe preserves bounded response and correlation");
			expect(gh_arguments(*process.LastRequest)[1] == "graphql" && process.LastRequest->Arguments.back() == "-" && process.LastRequest->StandardInput == wire->Body, "probe sends exact canonical query by stdin");
			const auto calls  = process.Calls;
			request.Wire.Body = "mutation { unsafe }";
			expect(std::holds_alternative<github_capability_probe_wire_error>(transport.perform_capability_probe(request, { })) && process.Calls == calls, "probe mutation drift never launches");
		}
	}

	void test_gh_help_admission_requires_each_invoked_operation()
	{
		constexpr std::string_view api        = "Makes an authenticated HTTP request. Override the method with `--method`.\n\nFLAGS\n"
												"  -H, --header key:value   Add a HTTP request header\n      --hostname string   The GitHub hostname\n"
												"  -i, --include            Include HTTP response status line and headers\n"
												"      --input file         The file to use as body\n  -X, --method string   The HTTP method\n";
		constexpr std::string_view clone      = "USAGE\n  gh repo clone <repository> [<directory>] [-- <gitflags>...]\n\nFLAGS\n"
												"      --no-upstream                   Do not add an upstream remote\n";
		constexpr std::string_view credential = "Implements git credential helper protocol\n\nUSAGE\n  gh auth git-credential [flags]\n";
		expect(gh_cli_help_admits(gh_cli_help_topic::api, api) && gh_cli_help_admits(gh_cli_help_topic::repo_clone, clone)
				   && gh_cli_help_admits(gh_cli_help_topic::auth_git_credential, credential),
			   "current gh help admits the api, repo clone and credential-helper operations");
		for (const std::string_view flag : { "--header", "--hostname", "--include", "--input", "--method" }) {
			auto       missing  = std::string{ api };
			const auto position = missing.find(std::string{ flag } + " ");
			missing.replace(position, flag.size(), "--unavailable");
			expect(!gh_cli_help_admits(gh_cli_help_topic::api, missing), "each api flag the app passes is required as an option row");
		}
		expect(!gh_cli_help_admits(gh_cli_help_topic::api, "Override the method with --method, --hostname, --include, --header and --input.\n"),
			   "flags mentioned only in prose do not count");
		expect(!gh_cli_help_admits(gh_cli_help_topic::repo_clone, "USAGE\n  gh repo clone <repository> [<directory>]\n      --no-upstream  x\n")
				   && !gh_cli_help_admits(gh_cli_help_topic::repo_clone, "USAGE\n  gh repo clone <repository> [<directory>] [-- <gitflags>...]\n")
				   && !gh_cli_help_admits(gh_cli_help_topic::repo_clone, "  To skip this behavior, use --no-upstream.\n  gh repo clone <repository> [-- <gitflags>...]\n"),
			   "repo clone requires both Git flag passthrough and the no-upstream option row");
		expect(!gh_cli_help_admits(gh_cli_help_topic::auth_git_credential, "unknown command \"git-credential\" for \"gh auth\"\n")
				   && !gh_cli_help_admits(gh_cli_help_topic::auth_git_credential, "  gh auth git-credentialx\n")
				   && !gh_cli_help_admits(gh_cli_help_topic::api, ""),
			   "absent subcommands and empty output are unsupported");
	}

	void test_auth_status_inventory_is_token_free_bounded_and_fail_closed()
	{
		temporary_layout layout;
		const auto       configuration = validate_gh_cli_configuration(configuration_candidate(layout));
		expect(configuration.has_value(), "CLI configuration for auth inventory validates");
		expect(
			!supported_gh_cli_auth_status_version("gh version 2.45.0 (2024-03-04)\n")
				&& !supported_gh_cli_auth_status_version("gh version 2.95.1 (2026-06-01)\n")
				&& supported_gh_cli_auth_status_version("gh version 2.96.0 (2026-07-02)\n")
				&& supported_gh_cli_auth_status_version("gh version 2.100.0 (2026-09-09)\n")
				&& !supported_gh_cli_auth_status_version("gh version 2.96.bad\n")
				&& supported_gh_cli_auth_status_version("gh version 2.120.0 (2027-03-01)\n")
				&& supported_gh_cli_auth_status_version("gh version 3.0.0\n")
				&& !supported_gh_cli_auth_status_version("gh version 2.96\n")
				&& !supported_gh_cli_auth_status_version("gh version 99999999999999999999.0.0\n")
				&& !supported_gh_cli_auth_status_version("gh 2.101.0\n"),
			"the version line requires gh 2.96 or newer, including later majors, and refuses malformed numbers");

		const auto request = compile_gh_cli_auth_status_request(*configuration);
		expect(
			gh_arguments(request)
				== std::vector<std::string>{ "auth", "status", "--active", "--json", "hosts" },
			"auth inventory requests only active JSON hosts without show-token");
		expect(
			request.StandardInput.empty() && request.StandardInputLimit == 1U
				&& request.StandardOutputLimit == 64U * 1024U
				&& request.StandardErrorLimit == 8U * 1024U
				&& request.Timeout == std::chrono::seconds{ 10 }
				&& request.PreserveCompletedOutput,
			"auth inventory has fixed process and output bounds");
		for (const auto& argument : gh_arguments(request)) {
			expect(argument != "--show-token" && argument != "-t", "auth inventory cannot request a token");
		}

		const auto parsed           = parse_gh_cli_auth_status(R"({"hosts":{"github.example.com":[{"active":true,"host":"github.example.com","login":"enterprise-reviewer","state":"success","tokenSource":"keyring","scopes":"repo"}],"github.com":[{"active":false,"host":"github.com","login":"old-account","state":"success"},{"active":true,"host":"github.com","login":"octocat","state":"success"}],"unavailable.example.com":[{"active":true,"host":"unavailable.example.com","login":"broken","state":"failure"}]}})");
		const auto expectedAccounts = std::vector<gh_cli_authenticated_account>{
			{ .Hostname = "github.com", .Login = "octocat" },
			{ .Hostname = "github.example.com", .Login = "enterprise-reviewer" },
		};
		expect(
			parsed.has_value() && *parsed == expectedAccounts,
			"only successful active host/login pairs cross the discovery boundary in stable order");

		for (const auto& hostile : {
				 std::string{ "not-json" },
				 std::string{ R"({"hosts":{"github.com":[{"active":true,"host":"elsewhere.example","login":"octocat","state":"success"}]}})" },
				 std::string{ R"({"hosts":{"github.com":[{"active":true,"host":"github.com","login":"one","state":"success"},{"active":true,"host":"github.com","login":"two","state":"success"}]}})" },
				 std::string{ R"({"hosts":{"https://github.example":[{"active":true,"host":"https://github.example","login":"octocat","state":"success"}]}})" },
			 }) {
			expect(
				!parse_gh_cli_auth_status(hostile),
				"malformed or ambiguous JSON auth inventory fails closed");
		}
		expect(
			parse_gh_cli_auth_status(R"({"hosts":{}})").has_value()
				&& parse_gh_cli_auth_status(R"({"hosts":{}})")->empty(),
			"an authenticated CLI with no active accounts reports an empty inventory");
		expect(
			parse_gh_cli_auth_status(std::string(64U * 1024U + 1U, 'x'))
					.error()
				== gh_cli_auth_status_failure::response_limit,
			"oversized auth inventory is refused before parsing");
	}

	/// [[BPR-1373]]. The full inventory: every account on every host, with
	/// whether gh reports it signed in and whether its token is in the keychain.
	void test_full_inventory_lists_every_account()
	{
		temporary_layout layout;
		const auto       configuration = validate_gh_cli_configuration(configuration_candidate(layout));
		expect(configuration.has_value(), "CLI configuration for the full inventory validates");
		const auto request = compile_gh_cli_auth_inventory_request(*configuration);
		expect(gh_arguments(request) == std::vector<std::string>{ "auth", "status", "--json", "hosts" },
			   "the full inventory asks for every account's JSON hosts, without --active or --show-token");
		expect(request.StandardInput.empty() && request.StandardOutputLimit == 64U * 1024U && request.PreserveCompletedOutput
				   && std::ranges::none_of(request.Environment, [](const auto& aEntry) { return aEntry.Name == "GH_CONFIG_DIR"; }),
			   "the full inventory is bounded and always reads gh's own configuration");

		const auto parsed = parse_gh_cli_auth_inventory(
			R"({"hosts":{"github.com":[{"active":true,"host":"github.com","login":"MikeMarcin","state":"success","tokenSource":"keyring","scopes":"repo"},)"
			R"({"active":false,"host":"github.com","login":"MikeMarcinTest","state":"success","tokenSource":"keyring"},)"
			R"({"active":false,"host":"github.com","login":"stale","state":"error","tokenSource":"keyring"},)"
			R"({"active":false,"host":"github.com","login":"plain","state":"success","tokenSource":"oauth_token"}],)"
			R"("github.example.com":[{"active":true,"host":"github.example.com","login":"enterprise_user","state":"success","tokenSource":"keyring"}]}})");
		const auto expected = std::vector<gh_cli_inventory_account>{
			{ .Hostname = "github.com", .Login = "MikeMarcin", .Active = true, .SignedIn = true, .KeychainHeld = true },
			{ .Hostname = "github.com", .Login = "MikeMarcinTest", .Active = false, .SignedIn = true, .KeychainHeld = true },
			{ .Hostname = "github.com", .Login = "stale", .Active = false, .SignedIn = false, .KeychainHeld = true },
			{ .Hostname = "github.com", .Login = "plain", .Active = false, .SignedIn = true, .KeychainHeld = false },
			{ .Hostname = "github.example.com", .Login = "enterprise_user", .Active = true, .SignedIn = true, .KeychainHeld = true },
		};
		expect(parsed.has_value() && *parsed == expected, "every account crosses, in host then listed order, with its state and token source");
		for (const auto* hostile : {
				 R"({"hosts":{"github.com":[{"active":false,"host":"github.com","login":"twice","state":"success"},{"active":false,"host":"github.com","login":"twice","state":"success"}]}})",
				 R"({"hosts":{"github.com":[{"active":true,"host":"github.com","login":"one","state":"success"},{"active":true,"host":"github.com","login":"two","state":"success"}]}})",
				 R"({"hosts":{"github.com":[{"active":false,"host":"elsewhere.example","login":"octocat","state":"success"}]}})",
				 R"({"hosts":{"GitHub.com":[{"active":false,"host":"GitHub.com","login":"octocat","state":"success"}]}})",
				 R"({"hosts":{"github.com":[{"active":false,"host":"github.com","login":"","state":"success"}]}})",
			 }) {
			expect(!parse_gh_cli_auth_inventory(hostile), "a duplicated, doubly active, misfiled or malformed account fails the whole inventory");
		}
		expect(parse_gh_cli_auth_inventory(std::string(64U * 1024U + 1U, 'x')).error() == gh_cli_auth_status_failure::response_limit,
			   "an oversized inventory is refused before parsing");
	}

	/// [[BPR-1373]]. A pinned launch gets its own private configuration, made
	/// for that launch alone and gone afterwards; an unpinned one is unchanged.
	void test_pinned_launch_gets_a_private_configuration()
	{
#if !defined(_WIN32)
		temporary_layout layout;
		const auto       configuration = validate_gh_cli_configuration(configuration_candidate(layout));
		expect(configuration.has_value() && !configuration->pin(), "a validated configuration starts unpinned");
		for (const auto& [host, login] : std::vector<std::pair<std::string, std::string>>{
				 { "GitHub.com", "octocat" }, { "github.com", "" }, { "github.com", "-octocat" }, { "github.com", "octo cat" }, { "github.com", "octo:cat" }, { "github.com", "octo\ncat" }, { "github.com", std::string(65U, 'a') } }) {
			expect(!pin_gh_cli_configuration(*configuration, host, login), "a pin needs a canonical host and a login in GitHub's alphabet");
		}
		const auto pinned = pin_gh_cli_configuration(*configuration, "github.com", "MikeMarcinTest");
		expect(pinned && pinned->pin() == gh_cli_account_pin{ .Hostname = "github.com", .Login = "MikeMarcinTest" }
				   && !pin_gh_cli_configuration(*pinned, "github.com", "MikeMarcin"),
			   "a configuration pins once, to one host and login");

		const auto& neutral   = configuration->neutral_working_directory();
		const auto  leftovers = [&neutral] {
			auto count = 0U;
			for (const auto& entry : std::filesystem::directory_iterator{ neutral }) {
				count += entry.path().filename().string().starts_with(gh_cli_account_directory_prefix) ? 1U : 0U;
			}
			return count;
		};
		successful_counting_process process;
		const auto                  request = compile_gh_cli_version_request(*configuration);
		expect(run_gh_cli(process, *configuration, request, { }).has_value() && process.LastRequest == request,
			   "an unpinned launch runs exactly the compiled request");

		auto seen     = std::optional<std::filesystem::path>{ };
		auto modes    = std::array<unsigned, 3U>{ };
		auto hosts    = std::string{ };
		auto files    = 0U;
		process.OnRun = [&] {
			const auto& environment = process.LastRequest->Environment;
			for (const auto& entry : environment) {
				expect(entry.Name != "GH_TOKEN" && entry.Name != "GITHUB_TOKEN" && entry.Name != "GH_ENTERPRISE_TOKEN"
						   && entry.Name != "GITHUB_ENTERPRISE_TOKEN",
					   "no token variable is ever set");
			}
			const auto found = std::ranges::find_if(environment, [](const auto& aEntry) { return aEntry.Name == "GH_CONFIG_DIR"; });
			expect(found != environment.end() && std::ranges::count_if(environment, [](const auto& aEntry) { return aEntry.Name == "GH_CONFIG_DIR"; }) == 1,
				   "a pinned launch names exactly one private configuration directory");
			seen = std::filesystem::path{ found->Value };
			struct stat status{ };
			for (std::size_t index = 0U; const auto& path : { *seen, *seen / "config.yml", *seen / "hosts.yml" }) {
				expect(::lstat(path.c_str(), &status) == 0, "the private configuration exists while gh runs");
				modes[index++] = static_cast<unsigned>(status.st_mode & 07777U);
			}
			for ([[maybe_unused]] const auto& entry : std::filesystem::directory_iterator{ *seen }) {
				++files;
			}
			std::ifstream input{ *seen / "hosts.yml" };
			hosts.assign(std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{ });
		};
		expect(run_gh_cli(process, *pinned, request, { }).has_value(), "a pinned launch runs");
		expect(seen && seen->parent_path() == neutral && seen->filename().string().starts_with(gh_cli_account_directory_prefix),
			   "the private configuration is made under the neutral working directory");
		expect(modes == std::array<unsigned, 3U>{ 0500U, 0400U, 0400U } && files == 2U,
			   "the directory is owner-only and read-only, and holds exactly its two read-only files");
		expect(hosts == "github.com:\n    users:\n        MikeMarcinTest:\n    git_protocol: https\n    user: MikeMarcinTest\n",
			   "hosts.yml names only the pinned host and login, and holds no credential");
		expect(!std::filesystem::exists(*seen) && leftovers() == 0U, "the private configuration is removed when the launch ends");
		process.OnRun = { };

		const auto calls    = process.Calls;
		auto       carrying = request;
		carrying.Environment.push_back({ .Name = "GH_CONFIG_DIR", .Value = path_bytes(neutral) });
		expect(run_gh_cli(process, *pinned, carrying, { }).error() == github_cli_process_failure::invalid_request && process.Calls == calls,
			   "a request that already names a configuration directory is refused unlaunched");
		std::filesystem::permissions(neutral, std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec);
		const auto unmade = run_gh_cli(process, *pinned, request, { });
		std::filesystem::permissions(neutral, std::filesystem::perms::owner_all);
		expect(unmade.error() == github_cli_process_failure::start_failure && process.Calls == calls,
			   "a pinned launch whose private configuration cannot be made is refused and never runs unpinned");
		expect(leftovers() == 0U, "a refused launch leaves nothing behind");

		// A gh auth switch rewrites the user's hosts.yml. The unpinned transport
		// refuses on that drift; the pinned one never reads that file.
		const auto cloud     = validate_github_host_profile(cloud_candidate());
		const auto wire      = wire_request_for(*cloud, read_operation::get_repository);
		const auto directory = layout.HomeDirectory / ".config" / "gh";
		std::filesystem::create_directories(directory);
		{
			std::ofstream output{ directory / "hosts.yml" };
			output << "github.com:\n    user: MikeMarcin\n";
		}
		gh_cli_transport unpinnedTransport{ *cloud, *configuration, process };
		gh_cli_transport pinnedTransport{ *cloud, *pinned, process, synthetic_operation_credential() };
		{
			std::ofstream output{ directory / "hosts.yml" };
			output << "github.com:\n    user: MikeMarcinTest-switched\n";
		}
		expect(std::get<github_wire_error>(unpinnedTransport.perform(wire, { })).Error.Failure == application::github_provider_failure::authentication,
			   "an unpinned transport refuses once gh's own configuration changes (the account may have switched)");
		auto pinnedConfiguration = std::string{ };
		process.OnRun            = [&] {
			for (const auto& entry : process.LastRequest->Environment) {
				if (entry.Name == "GH_CONFIG_DIR") {
					pinnedConfiguration = entry.Value;
				}
			}
		};
		expect(std::holds_alternative<github_wire_success>(pinnedTransport.perform(wire, { })) && pinnedTransport.configuration_unchanged()
				   && !pinnedConfiguration.empty() && leftovers() == 0U,
			   "a pinned transport reads through its private configuration across the switch");
		process.OnRun = { };
		std::filesystem::remove_all(directory);
#endif
	}

}

auto main() -> int
try {
	test_configuration_is_canonical_typed_and_closed();
	test_all_operations_compile_to_exact_closed_invocations();
	test_configuration_rejects_ambient_and_unsafe_candidates();
	test_correlation_server_and_wire_drift_are_rejected();
	test_gh_help_admission_requires_each_invoked_operation();
	test_full_inventory_lists_every_account();
	test_pinned_launch_gets_a_private_configuration();
	test_auth_status_inventory_is_token_free_bounded_and_fail_closed();
	for (const std::string_view host : { "github.com", "sub.github.com", "github.localhost", "sub.github.localhost", "tenant.ghe.com", "api.tenant.ghe.com" }) {
		expect(gh_cli_credential_environment(host) == cli_credential_environment::github, "stock-gh public/tenancy host classes use the selected GH_TOKEN");
	}
	for (const std::string_view host : { "ghe.com", "github.example.com", "ghe.example.com", "github.com.example.com" }) {
		expect(gh_cli_credential_environment(host) == cli_credential_environment::enterprise, "stock-gh enterprise classes use selected GH_ENTERPRISE_TOKEN");
	}
	test_detected_configuration_change_refuses_launch();
	test_configuration_file_metadata_changes();
	if constexpr (private_cli_configuration_available) {
		test_strict_acquisition_failures();
		test_cli_mutation_session_recovery();
		test_mutation_closed_shapes_and_ambiguous_results();
		test_staged_cli_catalog();
		test_transport_parses_actual_response_and_fixed_failures();
		test_capability_probe_uses_closed_query_and_response();
		test_completed_http_errors_preserve_provider_evidence();
		test_nonzero_graphql_errors();
		test_realistic_headers_project_provider_metadata();
	} else {
		test_windows_pinned_transport_refuses_before_effects();
		std::cout << "SKIPPED: successful pinned CLI dispatch, strict acquisition and recovery require the POSIX private-account configuration; Windows remains unavailable\n";
	}
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
