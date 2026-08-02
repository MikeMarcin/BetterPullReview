#include "gh_cli_transport.hpp"
#include "github_wire_request_validation.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

#if !defined(_WIN32)
	#include <unistd.h>
#endif

namespace bpr::adapters::github::detail {
	namespace {
		constexpr std::size_t maximum_environment_value_bytes = 4U * 1024U;
		constexpr std::size_t maximum_proxy_bytes             = 2U * 1024U;
		constexpr std::size_t maximum_endpoint_bytes          = 4U * 1024U;
		constexpr std::size_t maximum_standard_input_bytes    = 256U * 1024U;
		constexpr std::size_t maximum_standard_output_bytes   = 4U * 1024U * 1024U;
		constexpr std::size_t maximum_standard_error_bytes    = 64U * 1024U;
		constexpr auto        process_timeout                 = std::chrono::seconds{ 30 };
		constexpr auto        process_reap_timeout            = std::chrono::seconds{ 2 };

		[[nodiscard]] auto path_bytes(const std::filesystem::path& aPath)
			-> std::string
		{
			const auto bytes = aPath.generic_u8string();
			return {
				reinterpret_cast<const char*>(bytes.data()),
				bytes.size(),
			};
		}

		[[nodiscard]] auto safe_environment_value(
			const std::string_view aValue) noexcept -> bool
		{
			return !aValue.empty()
				&& aValue.size() <= maximum_environment_value_bytes
				&& std::ranges::all_of(
					aValue,
					[](const unsigned char aCharacter) {
						return aCharacter >= 0x20U && aCharacter != 0x7fU;
					});
		}

		[[nodiscard]] auto canonical_directory(
			const std::filesystem::path& aCandidate,
			std::filesystem::path&       aResult) -> bool
		{
			if (!aCandidate.is_absolute()) {
				return false;
			}
			std::error_code error;
			if (!std::filesystem::is_directory(aCandidate, error) || error) {
				return false;
			}
			aResult = std::filesystem::canonical(aCandidate, error);
			return !error && aResult.is_absolute();
		}

		[[maybe_unused, nodiscard]] auto canonical_file(
			const std::filesystem::path& aCandidate,
			std::filesystem::path&       aResult) -> bool
		{
			if (!aCandidate.is_absolute()) {
				return false;
			}
			std::error_code error;
			if (!std::filesystem::is_regular_file(aCandidate, error) || error) {
				return false;
			}
			aResult = std::filesystem::canonical(aCandidate, error);
			return !error && aResult.is_absolute();
		}

		[[nodiscard]] auto executable_name(
			const std::filesystem::path& aPath) -> bool
		{
#if defined(_WIN32)
			auto name = path.filename().string();
			std::ranges::transform(
				name,
				name.begin(),
				[](const unsigned char character) {
					if (character >= 'A' && character <= 'Z') {
						return static_cast<char>(character - 'A' + 'a');
					}
					return static_cast<char>(character);
				});
			return name == "gh.exe";
#else
			return aPath.filename() == "gh";
#endif
		}

		[[nodiscard]] auto executable_permissions(
			const std::filesystem::path& aPath) -> bool
		{
#if defined(_WIN32)
			return executable_name(path);
#else
			return ::access(aPath.c_str(), X_OK) == 0;
#endif
		}

		[[nodiscard]] auto repository_ancestor(
			std::filesystem::path aDirectory) -> bool
		{
			for (;;) {
				std::error_code error;
				const auto      markerExists = std::filesystem::exists(aDirectory / ".git", error);
				if (error || markerExists) {
					return true;
				}
				const auto headPath   = aDirectory / "HEAD";
				const auto headExists = std::filesystem::exists(headPath, error);
				if (error) {
					return true;
				}
				const auto headIsFile = headExists && std::filesystem::is_regular_file(headPath, error);
				if (error) {
					return true;
				}
				const auto objectsPath  = aDirectory / "objects";
				const auto objectsExist = std::filesystem::exists(objectsPath, error);
				if (error) {
					return true;
				}
				const auto objectsAreDirectory = objectsExist
					&& std::filesystem::is_directory(objectsPath, error);
				if (error) {
					return true;
				}
				const auto refsPath  = aDirectory / "refs";
				const auto refsExist = std::filesystem::exists(refsPath, error);
				if (error) {
					return true;
				}
				const auto refsAreDirectory = refsExist && std::filesystem::is_directory(refsPath, error);
				if (
					error
					|| (headIsFile && objectsAreDirectory
						&& refsAreDirectory)) {
					return true;
				}
				const auto parent = aDirectory.parent_path();
				if (parent.empty() || parent == aDirectory) {
					return false;
				}
				aDirectory = parent;
			}
		}

		[[nodiscard]] auto canonical_environment_directory(
			const std::filesystem::path& aCandidate,
			std::string&                 aValue) -> bool
		{
			std::filesystem::path canonical;
			if (!canonical_directory(aCandidate, canonical)) {
				return false;
			}
			aValue = path_bytes(canonical);
			return safe_environment_value(aValue);
		}

		[[maybe_unused, nodiscard]] auto valid_session_bus_address(
			const std::string_view aValue) noexcept -> bool
		{
			return safe_environment_value(aValue)
				&& (aValue.starts_with("unix:path=/")
					|| aValue.starts_with("unix:abstract="))
				&& aValue.find(';') == std::string_view::npos
				&& aValue.find('\\') == std::string_view::npos;
		}

		enum class proxy_validation : std::uint8_t {
			valid,
			invalid,
			credential_bearing,
		};

		[[nodiscard]] auto validate_proxy(
			const std::string_view aValue) noexcept -> proxy_validation
		{
			if (
				aValue.empty() || aValue.size() > maximum_proxy_bytes
				|| !safe_environment_value(aValue)) {
				return proxy_validation::invalid;
			}
			const auto prefix = aValue.starts_with("https://")
				? std::string_view{ "https://" }
				: aValue.starts_with("http://")
				? std::string_view{ "http://" }
				: std::string_view{ };
			if (prefix.empty()) {
				return proxy_validation::invalid;
			}
			const auto authorityAndPath = aValue.substr(prefix.size());
			const auto slash            = authorityAndPath.find('/');
			const auto authority        = authorityAndPath.substr(0U, slash);
			const auto path             = slash == std::string_view::npos
				? std::string_view{ }
				: authorityAndPath.substr(slash);
			if (authority.find('@') != std::string_view::npos) {
				return proxy_validation::credential_bearing;
			}
			if (
				authority.empty() || (!path.empty() && path != "/")
				|| aValue.find('?') != std::string_view::npos
				|| aValue.find('#') != std::string_view::npos
				|| aValue.find('%') != std::string_view::npos
				|| !std::ranges::any_of(
					authority,
					[](const unsigned char aCharacter) {
						return (
								   aCharacter >= 'a' && aCharacter <= 'z')
							|| (aCharacter >= 'A' && aCharacter <= 'Z')
							|| (aCharacter >= '0' && aCharacter <= '9');
					})
				|| !std::ranges::all_of(
					authority,
					[](const unsigned char aCharacter) {
						return (
								   aCharacter >= 'a' && aCharacter <= 'z')
							|| (aCharacter >= 'A' && aCharacter <= 'Z')
							|| (aCharacter >= '0' && aCharacter <= '9')
							|| aCharacter == '.' || aCharacter == '-'
							|| aCharacter == ':' || aCharacter == '['
							|| aCharacter == ']';
					})) {
				return proxy_validation::invalid;
			}
			return proxy_validation::valid;
		}

		[[nodiscard]] auto valid_no_proxy(
			const std::string_view aValue) noexcept -> bool
		{
			if (
				aValue.empty() || aValue.size() > maximum_proxy_bytes
				|| !safe_environment_value(aValue)
				|| !std::ranges::all_of(
					aValue,
					[](const unsigned char aCharacter) {
						return (
								   aCharacter >= 'a' && aCharacter <= 'z')
							|| (aCharacter >= 'A' && aCharacter <= 'Z')
							|| (aCharacter >= '0' && aCharacter <= '9')
							|| aCharacter == '.' || aCharacter == '-'
							|| aCharacter == '_' || aCharacter == '*'
							|| aCharacter == ',' || aCharacter == ':'
							|| aCharacter == '[' || aCharacter == ']';
					})) {
				return false;
			}
			std::size_t begin = 0U;
			while (begin < aValue.size()) {
				const auto end   = aValue.find(',', begin);
				const auto token = aValue.substr(
					begin,
					end == std::string_view::npos
						? aValue.size() - begin
						: end - begin);
				if (token.empty()) {
					return false;
				}
				if (end == std::string_view::npos) {
					return true;
				}
				begin = end + 1U;
			}
			return false;
		}

		auto append_environment(
			std::vector<github_cli_process_environment_entry>& aEnvironment,
			const std::string_view                             aName,
			const std::string_view                             aValue) -> void
		{
			aEnvironment.emplace_back(
				github_cli_process_environment_entry{
					.Name  = std::string{ aName },
					.Value = std::string{ aValue },
				});
		}

		auto append_fixed_environment(
			std::vector<github_cli_process_environment_entry>& aEnvironment,
			const std::string_view                             aSetting) -> void
		{
			const auto separator = aSetting.find('=');
			append_environment(
				aEnvironment,
				aSetting.substr(0U, separator),
				aSetting.substr(separator + 1U));
		}

		[[nodiscard]] auto bounded_environment(
			const std::vector<github_cli_process_environment_entry>& aEnvironment) noexcept -> bool
		{
			if (aEnvironment.size() > github_cli_process_max_environment_count) {
				return false;
			}
			std::size_t total = 0U;
			for (const auto& entry : aEnvironment) {
				const auto windowsEmptyPath = entry.Name == "PATH" && entry.Value.empty();
				if (
					entry.Name.empty()
					|| entry.Name.size()
						> github_cli_process_max_environment_name_bytes
					|| entry.Value.size() > maximum_environment_value_bytes
					|| (!windowsEmptyPath && !safe_environment_value(entry.Value))
					|| entry.Name.size()
						> github_cli_process_max_environment_bytes - total) {
					return false;
				}
				total += entry.Name.size();
				if (
					entry.Value.size()
					> github_cli_process_max_environment_bytes - total) {
					return false;
				}
				total += entry.Value.size();
			}
			return true;
		}

		[[nodiscard]] auto append_platform_environment(
			const gh_cli_platform_environment_facts&           aFacts,
			std::vector<github_cli_process_environment_entry>& aEnvironment)
			-> std::optional<gh_cli_configuration_failure>
		{
#if defined(__APPLE__)
			const auto* platform = std::get_if<gh_cli_macos_environment_facts>(&aFacts);
			if (platform == nullptr) {
				return gh_cli_configuration_failure::irrelevant_platform_facts;
			}
			std::string home;
			if (!canonical_environment_directory(platform->HomeDirectory, home)) {
				return gh_cli_configuration_failure::invalid_platform_environment;
			}
			append_environment(aEnvironment, "HOME", home);
#elif defined(_WIN32)
			const auto* platform = std::get_if<gh_cli_windows_environment_facts>(&facts);
			if (platform == nullptr) {
				return gh_cli_configuration_failure::irrelevant_platform_facts;
			}
			std::string profile;
			std::string roaming;
			std::string local;
			std::string system_root;
			if (
				!canonical_environment_directory(
					platform->UserProfileDirectory,
					profile)
				|| !canonical_environment_directory(
					platform->RoamingApplicationDataDirectory,
					roaming)
				|| !canonical_environment_directory(
					platform->LocalApplicationDataDirectory,
					local)
				|| !canonical_environment_directory(
					platform->SystemRootDirectory,
					system_root)) {
				return gh_cli_configuration_failure::invalid_platform_environment;
			}
			append_environment(environment, "USERPROFILE", profile);
			append_environment(environment, "APPDATA", roaming);
			append_environment(environment, "LOCALAPPDATA", local);
			append_environment(environment, "SystemRoot", system_root);
			append_environment(environment, "PATH", "");
#elif defined(__linux__)
			const auto* platform = std::get_if<gh_cli_linux_environment_facts>(&facts);
			if (platform == nullptr) {
				return gh_cli_configuration_failure::irrelevant_platform_facts;
			}
			std::string home;
			std::string runtime;
			if (
				!canonical_environment_directory(platform->HomeDirectory, home)
				|| !canonical_environment_directory(
					platform->RuntimeDirectory,
					runtime)
				|| !valid_session_bus_address(platform->SessionBusAddress)) {
				return gh_cli_configuration_failure::invalid_platform_environment;
			}
			append_environment(environment, "HOME", home);
			append_environment(environment, "XDG_RUNTIME_DIR", runtime);
			append_environment(
				environment,
				"DBUS_SESSION_BUS_ADDRESS",
				platform->SessionBusAddress);
#else
			static_cast<void>(facts);
			static_cast<void>(environment);
			return gh_cli_configuration_failure::irrelevant_platform_facts;
#endif
			return std::nullopt;
		}

		[[nodiscard]] auto validate_correlation(
			const github_wire_request&           aRequest,
			const validated_github_host_profile& aProfile)
			-> std::optional<gh_cli_invocation_failure>
		{
			const auto validation = validate_github_wire_request(aProfile, aRequest);
			if (validation.has_value()) {
				return std::nullopt;
			}
			switch (validation.error()) {
			case github_wire_request_validation_failure::
				profile_correlation_mismatch:
				return gh_cli_invocation_failure::
					profile_correlation_mismatch;
			case github_wire_request_validation_failure::
				server_contract_mismatch:
				return gh_cli_invocation_failure::server_contract_mismatch;
			case github_wire_request_validation_failure::
				invalid_wire_request:
				return gh_cli_invocation_failure::invalid_wire_request;
			}
			return gh_cli_invocation_failure::profile_correlation_mismatch;
		}

		[[nodiscard]] auto safe_relative_endpoint(
			const std::string_view aEndpoint) noexcept -> bool
		{
			return !aEndpoint.empty() && aEndpoint.size() <= maximum_endpoint_bytes
				&& aEndpoint.front() != '-' && aEndpoint.front() != '@'
				&& aEndpoint.find("://") == std::string_view::npos
				&& aEndpoint.find('{') == std::string_view::npos
				&& aEndpoint.find('}') == std::string_view::npos
				&& aEndpoint.find('\\') == std::string_view::npos
				&& std::ranges::none_of(
					aEndpoint,
					[](const unsigned char aCharacter) {
						return aCharacter <= 0x20U || aCharacter == 0x7fU;
					});
		}

		[[nodiscard]] auto translated_endpoint(
			const validated_github_host_profile& aProfile,
			const wire_request&                  aWire) -> std::optional<std::string>
		{
			std::string endpoint;
			if (aWire.Operation == read_operation::list_review_threads) {
				endpoint = "graphql";
			} else if (
				aProfile.server_contract()
				== application::github_server_contract::github_com_2022_11_28) {
				if (
					aWire.Path.size() < 2U || aWire.Path.front() != '/'
					|| aWire.Path[1U] == '/') {
					return std::nullopt;
				}
				endpoint = aWire.Path.substr(1U);
			} else {
				constexpr auto prefix = std::string_view{ "/api/v3/" };
				if (!aWire.Path.starts_with(prefix)) {
					return std::nullopt;
				}
				endpoint = aWire.Path.substr(prefix.size());
			}
			if (!aWire.Query.empty()) {
				endpoint += "?";
				endpoint += aWire.Query;
			}
			if (!safe_relative_endpoint(endpoint)) {
				return std::nullopt;
			}
			return endpoint;
		}

		[[nodiscard]] auto wire_error(
			const github_wire_request&                 aRequest,
			const application::github_provider_failure aFailure)
			-> github_wire_completion
		{
			return github_wire_error{
				.Correlation = aRequest.Correlation,
				.Error       = application::github_provider_error{
					.Failure           = aFailure,
					.RetryAfterSeconds = std::nullopt,
					.RateLimits        = { },
					.RateLimitCause    = std::nullopt,
				},
			};
		}
	}

	validated_gh_cli_configuration::validated_gh_cli_configuration(
		std::filesystem::path                             aExecutable,
		std::filesystem::path                             aNeutralWorkingDirectory,
		std::vector<github_cli_process_environment_entry> aEnvironment)
		: Executable{ std::move(aExecutable) }
		, NeutralWorkingDirectory{ std::move(aNeutralWorkingDirectory) }
		, Environment{ std::move(aEnvironment) }
	{
	}

	auto validated_gh_cli_configuration::executable() const noexcept
		-> const std::filesystem::path&
	{
		return Executable;
	}

	auto validated_gh_cli_configuration::neutral_working_directory()
		const noexcept -> const std::filesystem::path&
	{
		return NeutralWorkingDirectory;
	}

	auto validate_gh_cli_configuration(
		const gh_cli_configuration_candidate& aCandidate)
		-> gh_cli_configuration_result
	{
		if (!aCandidate.Executable.is_absolute()) {
			return std::unexpected{
				gh_cli_configuration_failure::executable_not_absolute
			};
		}
		std::error_code error;
		if (!std::filesystem::exists(aCandidate.Executable, error) || error) {
			return std::unexpected{
				gh_cli_configuration_failure::executable_not_found
			};
		}
		if (!std::filesystem::is_regular_file(aCandidate.Executable, error) || error) {
			return std::unexpected{
				gh_cli_configuration_failure::executable_not_regular
			};
		}
		auto executable = std::filesystem::canonical(aCandidate.Executable, error);
		if (error) {
			return std::unexpected{
				gh_cli_configuration_failure::executable_not_found
			};
		}
		if (!executable_name(executable)) {
			return std::unexpected{
				gh_cli_configuration_failure::executable_name_mismatch
			};
		}
		if (!executable_permissions(executable)) {
			return std::unexpected{
				gh_cli_configuration_failure::executable_not_executable
			};
		}

		if (!aCandidate.NeutralWorkingDirectory.is_absolute()) {
			return std::unexpected{
				gh_cli_configuration_failure::working_directory_not_absolute
			};
		}
		if (
			!std::filesystem::exists(
				aCandidate.NeutralWorkingDirectory,
				error)
			|| error) {
			return std::unexpected{
				gh_cli_configuration_failure::working_directory_not_found
			};
		}
		if (
			!std::filesystem::is_directory(
				aCandidate.NeutralWorkingDirectory,
				error)
			|| error) {
			return std::unexpected{
				gh_cli_configuration_failure::working_directory_not_directory
			};
		}
		auto workingDirectory = std::filesystem::canonical(
			aCandidate.NeutralWorkingDirectory,
			error);
		if (error) {
			return std::unexpected{
				gh_cli_configuration_failure::working_directory_not_found
			};
		}
		if (repository_ancestor(workingDirectory)) {
			return std::unexpected{
				gh_cli_configuration_failure::working_directory_is_repository
			};
		}

		constexpr std::array hardening{
			std::string_view{ "GH_PROMPT_DISABLED=1" },
			std::string_view{ "GH_NO_UPDATE_NOTIFIER=1" },
			std::string_view{ "GH_NO_EXTENSION_UPDATE_NOTIFIER=1" },
			std::string_view{ "GH_SPINNER_DISABLED=1" },
			std::string_view{ "GH_TELEMETRY=0" },
			std::string_view{ "NO_COLOR=1" },
			std::string_view{ "CLICOLOR=0" },
		};
		std::vector<github_cli_process_environment_entry> environment;
		environment.reserve(16U);
		for (const auto setting : hardening) {
			append_fixed_environment(environment, setting);
		}
		if (const auto platform = append_platform_environment(
				aCandidate.Platform,
				environment);
			platform.has_value()) {
			return std::unexpected{ *platform };
		}

		if (aCandidate.Https.Proxy.has_value()) {
			switch (validate_proxy(*aCandidate.Https.Proxy)) {
			case proxy_validation::valid:
				append_environment(
					environment,
					"HTTPS_PROXY",
					*aCandidate.Https.Proxy);
				break;
			case proxy_validation::credential_bearing:
				return std::unexpected{
					gh_cli_configuration_failure::
						credential_bearing_https_proxy
				};
			case proxy_validation::invalid:
				return std::unexpected{
					gh_cli_configuration_failure::invalid_https_proxy
				};
			}
		}
		if (aCandidate.Https.NoProxy.has_value()) {
			if (!valid_no_proxy(*aCandidate.Https.NoProxy)) {
				return std::unexpected{
					gh_cli_configuration_failure::invalid_no_proxy
				};
			}
			append_environment(
				environment,
				"NO_PROXY",
				*aCandidate.Https.NoProxy);
		}

#if !defined(__linux__)
		if (
			aCandidate.Https.CertificateFile.has_value()
			|| aCandidate.Https.CertificateDirectory.has_value()) {
			return std::unexpected{
				gh_cli_configuration_failure::irrelevant_platform_facts
			};
		}
#else
		if (candidate.Https.CertificateFile.has_value()) {
			std::filesystem::path certificate;
			if (!canonical_file(*candidate.Https.CertificateFile, certificate)) {
				return std::unexpected{
					gh_cli_configuration_failure::invalid_certificate_file
				};
			}
			append_environment(
				environment,
				"SSL_CERT_FILE",
				path_bytes(certificate));
		}
		if (candidate.Https.CertificateDirectory.has_value()) {
			std::filesystem::path certificate_directory;
			if (!canonical_directory(
					*candidate.Https.CertificateDirectory,
					certificate_directory)) {
				return std::unexpected{
					gh_cli_configuration_failure::invalid_certificate_directory
				};
			}
			append_environment(
				environment,
				"SSL_CERT_DIR",
				path_bytes(certificate_directory));
		}
#endif

		if (!bounded_environment(environment)) {
			return std::unexpected{
				gh_cli_configuration_failure::environment_limit
			};
		}

		const validated_gh_cli_configuration configuration{
			std::move(executable),
			std::move(workingDirectory),
			std::move(environment),
		};
		return gh_cli_configuration_result{ configuration };
	}

	auto compile_gh_cli_process_request(
		const validated_gh_cli_configuration& aConfiguration,
		const validated_github_host_profile&  aProfile,
		const github_wire_request&            aRequest) -> gh_cli_invocation_result
	{
		if (
			aProfile.authentication_transport()
			!= application::github_authentication_transport::github_cli) {
			return std::unexpected{
				gh_cli_invocation_failure::profile_transport_mismatch
			};
		}
		if (const auto correlation = validate_correlation(aRequest, aProfile);
			correlation.has_value()) {
			return std::unexpected{ *correlation };
		}
		const auto endpoint = translated_endpoint(aProfile, aRequest.Wire);
		if (!endpoint.has_value()) {
			return std::unexpected{ gh_cli_invocation_failure::invalid_route };
		}

		std::vector<std::string> arguments{
			"api",
			*endpoint,
			"--hostname",
			std::string{ aProfile.hostname() },
			"--method",
			aRequest.Wire.Method == http_method::get ? "GET" : "POST",
		};
		for (const auto& header : aRequest.Wire.Headers) {
			arguments.emplace_back("--header");
			arguments.emplace_back(header.Name + ": " + header.Value);
		}
		if (aRequest.Wire.Method == http_method::post) {
			arguments.emplace_back("--input");
			arguments.emplace_back("-");
		}
		if (
			aRequest.Wire.Body.size() > maximum_standard_input_bytes
			|| arguments.size() > 16U) {
			return std::unexpected{ gh_cli_invocation_failure::request_limit };
		}

		return github_cli_process_request{
			.Executable          = path_bytes(aConfiguration.Executable),
			.Arguments           = std::move(arguments),
			.Environment         = aConfiguration.Environment,
			.WorkingDirectory    = path_bytes(aConfiguration.NeutralWorkingDirectory),
			.StandardInput       = aRequest.Wire.Body,
			.StandardInputLimit  = maximum_standard_input_bytes,
			.StandardOutputLimit = maximum_standard_output_bytes,
			.StandardErrorLimit  = maximum_standard_error_bytes,
			.Timeout             = process_timeout,
			.ReapTimeout         = process_reap_timeout,
		};
	}

	gh_cli_transport::gh_cli_transport(
		const validated_github_host_profile&  aProfile,
		const validated_gh_cli_configuration& aConfiguration,
		github_cli_process_port&              aProcess)
		: Profile{ aProfile }
		, Configuration{ aConfiguration }
		, Process{ aProcess }
	{
	}

	auto gh_cli_transport::availability_reasons() const noexcept
		-> std::array<gh_cli_availability_reason, 8>
	{
		return {
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
	}

	auto gh_cli_transport::perform(
		const github_wire_request& aRequest,
		const std::stop_token      aStop) -> github_wire_completion
	{
		if (aStop.stop_requested()) {
			return github_wire_cancelled{
				.Correlation = aRequest.Correlation,
			};
		}
		const auto invocation = compile_gh_cli_process_request(Configuration, Profile, aRequest);
		if (!invocation.has_value()) {
			return wire_error(
				aRequest,
				application::github_provider_failure::unavailable);
		}
		if (aStop.stop_requested()) {
			return github_wire_cancelled{
				.Correlation = aRequest.Correlation,
			};
		}

		static_cast<void>(Process);
		return wire_error(
			aRequest,
			application::github_provider_failure::cli_unavailable);
	}
}
