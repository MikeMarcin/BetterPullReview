#include "gh_cli_transport.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

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

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
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
			request.PullRequestNumber = 17U;
			break;
		case read_operation::get_viewer_identity:
		case read_operation::get_repository:
		case read_operation::list_checks:
		case read_operation::search_pull_requests:
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
		if (aRequest.Wire.Operation == read_operation::list_review_threads) {
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

	class successful_counting_process final : public github_cli_process_port
	{
	public:
		[[nodiscard]] auto run(
			const github_cli_process_request& aRequest,
			const std::stop_token             aStop) -> github_cli_process_result override
		{
			++Calls;
			LastRequest       = aRequest;
			LastStopRequested = aStop.stop_requested();
			return github_cli_process_success{
				.StandardOutput = "synthetic-success",
			};
		}

		std::size_t                               Calls = 0U;
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

		for (const auto* profile : { &*cloud, &*enterprise }) {
			for (const auto operation : read_operation_catalog) {
				const auto request    = wire_request_for(*profile, operation);
				const auto invocation = compile_gh_cli_process_request(
					*configuration,
					*profile,
					request);
				expect(
					invocation.has_value(),
					"every read operation compiles for both server contracts");
				expect(
					invocation->Executable
							== path_bytes(std::filesystem::canonical(
								layout.Executable))
						&& invocation->WorkingDirectory
							== path_bytes(std::filesystem::canonical(
								layout.NeutralDirectory)),
					"invocation retains canonical executable and neutral cwd");
				expect(
					invocation->Arguments
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
					!invocation->Arguments[1U].starts_with('-')
						&& !invocation->Arguments[1U].starts_with('@')
						&& invocation->Arguments[1U].find("://")
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

	auto test_transport_is_fixed_unavailable_without_process_execution() -> void
	{
		temporary_layout layout;
		const auto       configuration = validate_gh_cli_configuration(configuration_candidate(layout));
		const auto       cloud         = validate_github_host_profile(cloud_candidate());
		const auto       enterprise    = validate_github_host_profile(enterprise_candidate());
		expect(
			configuration.has_value() && cloud.has_value()
				&& enterprise.has_value(),
			"fail-closed transport prerequisites validate");
		successful_counting_process process;

		for (const auto* profile : { &*cloud, &*enterprise }) {
			gh_cli_transport transport{ *profile, *configuration, process };
			const std::array expectedReasons{
				gh_cli_availability_reason::
					milestone_one_product_scope_deferred,
				gh_cli_availability_reason::
					executable_identity_unverifiable,
				gh_cli_availability_reason::
					cross_authority_redirect_unverifiable,
				gh_cli_availability_reason::
					ambient_configuration_unverifiable,
				gh_cli_availability_reason::
					credential_identity_binding_unverifiable,
				gh_cli_availability_reason::
					working_directory_identity_unverifiable,
				gh_cli_availability_reason::
					descendant_process_containment_unverifiable,
				gh_cli_availability_reason::
					numeric_process_identity_reuse_unverifiable,
			};
			expect(
				transport.availability_reasons() == expectedReasons,
				"transport exposes the M1 deferral and every CLI trust blocker");
			for (const auto operation : read_operation_catalog) {
				const auto  request    = wire_request_for(*profile, operation);
				const auto  completion = transport.perform(request, std::stop_token{ });
				const auto* error      = std::get_if<github_wire_error>(&completion);
				expect(
					error != nullptr
						&& error->Correlation == request.Correlation
						&& error->Error.Failure
							== application::github_provider_failure::
								cli_unavailable
						&& !error->Error.RetryAfterSeconds.has_value(),
					"valid CLI request preserves correlation and fails closed");
			}
		}
		expect(
			process.Calls == 0U && !process.LastRequest.has_value(),
			"unverifiable CLI contract never reaches the process port");

		gh_cli_transport transport{ *cloud, *configuration, process };
		const auto       stoppedRequest = wire_request_for(*cloud, read_operation::get_viewer_identity);
		std::stop_source stop;
		stop.request_stop();
		const auto  stopped   = transport.perform(stoppedRequest, stop.get_token());
		const auto* cancelled = std::get_if<github_wire_cancelled>(&stopped);
		expect(
			cancelled != nullptr
				&& cancelled->Correlation == stoppedRequest.Correlation
				&& process.Calls == 0U,
			"pre-stopped request returns exact cancellation without launch");

		auto invalid                  = wire_request_for(*cloud, read_operation::get_repository);
		invalid.Wire.Query            = "page=2";
		const auto  invalidCompletion = transport.perform(invalid, std::stop_token{ });
		const auto* error             = std::get_if<github_wire_error>(&invalidCompletion);
		expect(
			error != nullptr
				&& error->Correlation == invalid.Correlation
				&& error->Error.Failure
					== application::github_provider_failure::unavailable
				&& process.Calls == 0U,
			"invalid wire request is typed and never launched");
	}
}

auto main() -> int
{
	test_configuration_is_canonical_typed_and_closed();
	test_all_operations_compile_to_exact_closed_invocations();
	test_configuration_rejects_ambient_and_unsafe_candidates();
	test_correlation_server_and_wire_drift_are_rejected();
	test_transport_is_fixed_unavailable_without_process_execution();
	return EXIT_SUCCESS;
}
