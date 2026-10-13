#include "gh_cli_transport.hpp"
#include "gh_cli_response.hpp"
#include "github_paged_read.hpp"
#include "github_wire_request_validation.hpp"

#include <glaze/json.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <filesystem>
#include <iterator>
#include <map>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

#if !defined(_WIN32)
	#include <fcntl.h>
	#include <sys/stat.h>
	#include <unistd.h>
#endif

namespace bpr::adapters::github::detail::auth_status_json_detail {
	struct account
	{
		bool        Active = false;
		std::string Host;
		std::string Login;
		std::string State;
		std::string TokenSource;
	};

	struct document
	{
		std::map<std::string, std::vector<account>, std::less<>> Hosts;
	};

	struct options : glz::opts
	{
		static constexpr bool error_on_unknown_keys = false;
		// [[BPR-0970]]. The reader takes a view, which need not end in a NUL;
		// without this glaze reads one byte past it, as every other reader in
		// this adapter already declares.
		static constexpr bool null_terminated = false;
	};
}

namespace glz {
	template <>
	struct meta<bpr::adapters::github::detail::auth_status_json_detail::account>
	{
		using value_type            = bpr::adapters::github::detail::auth_status_json_detail::account;
		static constexpr auto value = object(
			"active", &value_type::Active,
			"host", &value_type::Host,
			"login", &value_type::Login,
			"state", &value_type::State,
			"tokenSource", &value_type::TokenSource);
	};

	template <>
	struct meta<bpr::adapters::github::detail::auth_status_json_detail::document>
	{
		using value_type            = bpr::adapters::github::detail::auth_status_json_detail::document;
		static constexpr auto value = object("hosts", &value_type::Hosts);
	};
}

namespace bpr::adapters::github::detail {
	namespace {
		constexpr std::size_t maximum_environment_value_bytes = 4U * 1024U;
		constexpr std::size_t maximum_proxy_bytes             = 2U * 1024U;
		constexpr std::size_t maximum_endpoint_bytes          = 4U * 1024U;
		constexpr std::size_t maximum_standard_input_bytes    = 256U * 1024U;
		constexpr std::size_t maximum_standard_output_bytes   = maximum_gh_cli_standard_output_bytes;
		constexpr std::size_t maximum_standard_error_bytes    = 64U * 1024U;
		constexpr std::size_t maximum_auth_status_bytes       = 64U * 1024U;
		constexpr std::size_t maximum_auth_status_hosts       = 64U;
		constexpr std::size_t maximum_auth_status_accounts    = 128U;
		constexpr auto        process_timeout                 = std::chrono::seconds{ 30 };
		constexpr auto        process_reap_timeout            = std::chrono::seconds{ 2 };

		[[nodiscard]] auto constrain_gh_writes(github_cli_process_request aRequest,
											   const std::string&         aWritableRoot = { })
			-> github_cli_process_request
		{
#if defined(__APPLE__)
			// gh migrates legacy config even for --version and api. Deny writes
			// before exec, including symlink targets and descendants; observing a
			// change afterwards cannot undo a credential/configuration migration.
			std::vector<std::string> arguments{
				"-p",
				"(version 1)(allow default)(deny file-write*)(allow file-write* (literal \"/dev/null\"))",
				std::move(aRequest.Executable),
			};
			if (!aWritableRoot.empty()) {
				arguments[1] += "(allow file-write* (subpath \"" + aWritableRoot + "\"))";
			}
			arguments.insert(arguments.end(),
							 std::make_move_iterator(aRequest.Arguments.begin()),
							 std::make_move_iterator(aRequest.Arguments.end()));
			aRequest.Executable = "/usr/bin/sandbox-exec";
			aRequest.Arguments  = std::move(arguments);
#endif
			static_cast<void>(aWritableRoot);
			return aRequest;
		}

		[[nodiscard]] auto completed_graphql_failure(const wire_response& aResponse)
			-> application::github_provider_error
		{
			using failure = application::github_provider_failure;
			application::github_provider_error error{
				.Failure                = failure::malformed_response,
				.RetryAfterSeconds      = std::nullopt,
				.RateLimits             = { },
				.RateLimitCause         = std::nullopt,
				.HttpStatus             = static_cast<std::uint16_t>(aResponse.Status),
				.TransportFailureSource = application::diagnostic_transport_failure_source::http_response,
			};
			const auto metadata = parse_github_response_metadata(aResponse);
			if (!metadata || aResponse.Status != 200
				|| aResponse.Completeness != page_completeness::complete
				|| (metadata->RateLimit && metadata->RateLimit->Resource != application::github_rate_limit_resource::graphql)) {
				return error;
			}
			// Reuse the provider's bounded GraphQL error parser. No error prose or
			// partial data enters the failure completion, even for valid-looking data.
			capability_probe_response response{
				.Status        = aResponse.Status,
				.Authenticated = true,
				.ContentType   = "application/json",
				.Body          = aResponse.Body,
			};
			for (const auto& header : aResponse.Headers) {
				const auto name = lowercase_header(header.Name);
				if (name.starts_with("x-ratelimit-") || name == "retry-after") {
					response.RateLimitHeaders[response.RateLimitHeaderCount++] = { .Name = header.Name, .Value = header.Value };
				}
			}
			const auto observation = normalize_capability_probe(response);
			if (metadata->RateLimit) {
				error.RateLimits.Graphql = metadata->RateLimit;
			}
			if (observation.Unavailability == capability_probe_unavailability::response_limit) {
				error.Failure = failure::response_limit;
			} else if (observation.Unavailability == capability_probe_unavailability::rate_limit && metadata->RateLimit) {
				error.Failure           = failure::rate_limit;
				error.RetryAfterSeconds = metadata->RetryAfterSeconds;
				auto cause              = *metadata->RateLimit;
				cause.SecondaryLimited  = cause.Remaining != 0U;
				if (cause.SecondaryLimited && !error.RetryAfterSeconds) {
					error.RetryAfterSeconds = 60U;
				}
				error.RateLimits.Graphql = cause;
				error.RateLimitCause     = cause;
			}
			return error;
		}

		[[nodiscard]] auto process_failure(const github_cli_process_failure aFailure)
			-> application::github_provider_failure
		{
			using failure = application::github_provider_failure;
			switch (aFailure) {
			case github_cli_process_failure::start_failure:         return failure::process_startup;
			case github_cli_process_failure::timeout:               return failure::timeout;
			case github_cli_process_failure::standard_output_limit:
			case github_cli_process_failure::standard_error_limit:  return failure::response_limit;
			default:                                                return failure::unavailable;
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
			auto name = aPath.filename().string();
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
			return executable_name(aPath);
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
			const auto* platform = std::get_if<gh_cli_windows_environment_facts>(&aFacts);
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
			append_environment(aEnvironment, "USERPROFILE", profile);
			append_environment(aEnvironment, "APPDATA", roaming);
			append_environment(aEnvironment, "LOCALAPPDATA", local);
			append_environment(aEnvironment, "SystemRoot", system_root);
			append_environment(aEnvironment, "PATH", "");
#elif defined(__linux__)
			const auto* platform = std::get_if<gh_cli_linux_environment_facts>(&aFacts);
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
			append_environment(aEnvironment, "HOME", home);
			append_environment(aEnvironment, "XDG_RUNTIME_DIR", runtime);
			append_environment(
				aEnvironment,
				"DBUS_SESSION_BUS_ADDRESS",
				platform->SessionBusAddress);
#else
			static_cast<void>(aFacts);
			static_cast<void>(aEnvironment);
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
			if (graphql_read(aWire.Operation)) {
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
				endpoint += '?';
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

		[[nodiscard]] auto valid_inventory_login(const std::string_view aLogin) noexcept -> bool
		{
			return !aLogin.empty() && aLogin.size() <= 128U
				&& std::ranges::all_of(aLogin, [](const unsigned char aByte) {
					   return aByte >= 0x21U && aByte <= 0x7eU;
				   });
		}

#if !defined(_WIN32)
		/// [[BPR-1373]]. One launch's private `gh` configuration: created
		/// owner-only, holding exactly the pinned host and login, read-only
		/// while the child runs, and removed with this object.
		class private_account_directory final
		{
		public:
			private_account_directory(const std::filesystem::path& aParent, const gh_cli_account_pin& aPin)
			{
				auto pattern = path_bytes(aParent / (std::string{ gh_cli_account_directory_prefix } + "XXXXXX"));
				pattern.push_back('\0');
				if (::mkdtemp(pattern.data()) == nullptr) {
					return;
				}
				Path             = std::filesystem::path{ pattern.c_str() };
				const auto hosts = aPin.Hostname + ":\n    users:\n        " + aPin.Login + ":\n    git_protocol: https\n    user: "
					+ aPin.Login + "\n";
				if (!write_file("config.yml", "version: \"1\"\n") || !write_file("hosts.yml", hosts)
					|| ::chmod(Path.c_str(), S_IRUSR | S_IXUSR) != 0) {
					remove();
					return;
				}
				Valid = true;
			}
			private_account_directory(const private_account_directory&)                    = delete;
			private_account_directory(private_account_directory&&)                         = delete;
			auto operator=(const private_account_directory&) -> private_account_directory& = delete;
			auto operator=(private_account_directory&&) -> private_account_directory&      = delete;
			~private_account_directory() { remove(); }

			[[nodiscard]] auto valid() const noexcept -> bool { return Valid; }
			[[nodiscard]] auto path() const noexcept -> const std::filesystem::path& { return Path; }

		private:
			[[nodiscard]] auto write_file(const char* aName, const std::string_view aContent) const -> bool
			{
				const auto descriptor = ::open((Path / aName).c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, S_IRUSR);
				if (descriptor < 0) {
					return false;
				}
				auto remaining = aContent;
				while (!remaining.empty()) {
					const auto written = ::write(descriptor, remaining.data(), remaining.size());
					if (written <= 0) {
						static_cast<void>(::close(descriptor));
						return false;
					}
					remaining.remove_prefix(static_cast<std::size_t>(written));
				}
				return ::close(descriptor) == 0;
			}

			void remove() noexcept
			{
				if (Path.empty()) {
					return;
				}
				static_cast<void>(::chmod(Path.c_str(), S_IRWXU));
				static_cast<void>(::unlink((Path / "config.yml").c_str()));
				static_cast<void>(::unlink((Path / "hosts.yml").c_str()));
				static_cast<void>(::rmdir(Path.c_str()));
				Path.clear();
				Valid = false;
			}

			std::filesystem::path Path;
			bool                  Valid = false;
		};
#endif
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

	auto validated_gh_cli_configuration::pin() const noexcept -> const std::optional<gh_cli_account_pin>&
	{
		return Pin;
	}

	auto pin_gh_cli_configuration(
		const validated_gh_cli_configuration& aConfiguration,
		const std::string_view aHostname, const std::string_view aLogin)
		-> std::optional<validated_gh_cli_configuration>
	{
		if (aConfiguration.Pin || !application::valid_github_canonical_hostname(aHostname)
			|| !valid_cli_account_login(aLogin)) {
			return std::nullopt;
		}
		auto pinned = aConfiguration;
		pinned.Pin  = gh_cli_account_pin{ .Hostname = std::string{ aHostname }, .Login = std::string{ aLogin } };
		return std::optional<validated_gh_cli_configuration>{ pinned };
	}

	auto run_gh_cli(
		github_cli_process_port& aProcess, const validated_gh_cli_configuration& aConfiguration,
		github_cli_process_request aRequest, const std::stop_token aStop, cli_transient_credential aCredential) -> github_cli_process_result
	{
		if (std::ranges::any_of(aRequest.Environment, [](const github_cli_process_environment_entry& aEntry) {
				return aEntry.Name == "GH_CONFIG_DIR";
			})) {
			return std::unexpected{ github_cli_process_failure::invalid_request };
		}
		if (!aConfiguration.Pin) {
			if (aCredential) {
				return std::unexpected{ github_cli_process_failure::invalid_request };
			}
			return aProcess.run(aRequest, aStop);
		}
#if defined(_WIN32)
		static_cast<void>(aStop);
		return std::unexpected{ github_cli_process_failure::start_failure };
#else
		const auto directory = std::make_shared<private_account_directory>(aConfiguration.NeutralWorkingDirectory, *aConfiguration.Pin);
		if (!directory->valid()) {
			return std::unexpected{ github_cli_process_failure::start_failure };
		}
		append_environment(aRequest.Environment, "GH_CONFIG_DIR", path_bytes(directory->path()));
		if (!bounded_environment(aRequest.Environment)) {
			return std::unexpected{ github_cli_process_failure::invalid_request };
		}
		if (aCredential) {
			struct launch_owner
			{
				cli_transient_credential                   Secret;
				std::shared_ptr<private_account_directory> Directory;
			};
			auto owner       = std::make_shared<launch_owner>();
			owner->Secret    = std::move(aCredential);
			owner->Directory = directory;
			const cli_transient_credential retained{ owner, owner->Secret.get() };
			return aProcess.run_credentialed(aRequest, retained, gh_cli_credential_environment(aConfiguration.Pin->Hostname), aStop);
		}
		return aProcess.run(aRequest, aStop);
#endif
	}

	auto gh_cli_credential_environment(std::string_view aHostname) noexcept -> cli_credential_environment
	{
		// cli/go-gh v2.16.1 auth.NormalizeHostname/tokenForHost. These host
		// classes are independent of the provider's GitHub.com/GHES routes.
		return aHostname == "github.com" || aHostname.ends_with(".github.com")
				|| aHostname == "github.localhost" || aHostname.ends_with(".github.localhost") || aHostname.ends_with(".ghe.com")
			? cli_credential_environment::github
			: cli_credential_environment::enterprise;
	}

	auto acquire_gh_cli_credential(github_cli_process_port& aProcess, const validated_gh_cli_configuration& aConfiguration, std::stop_token aStop) -> gh_cli_credential_result
	{
		if (!aConfiguration.Pin) {
			return std::unexpected{ gh_cli_credential_error{ github_cli_process_failure::invalid_request } };
		}
		const auto version = run_gh_cli(aProcess, aConfiguration, compile_gh_cli_version_request(aConfiguration), aStop);
		if (!version) {
			return std::unexpected{ gh_cli_credential_error{ version.error() } };
		}
		if (!supported_gh_cli_version(version->StandardOutput)) {
			return std::unexpected{ gh_cli_credential_error{ github_cli_process_failure::start_failure } };
		}
		for (const auto topic : gh_cli_required_help_topics) {
			const auto help = run_gh_cli(aProcess, aConfiguration, compile_gh_cli_help_request(aConfiguration, topic), aStop);
			if (!help) {
				return std::unexpected{ gh_cli_credential_error{ help.error() } };
			}
			if (!gh_cli_help_admits(topic, help->StandardOutput)) {
				return std::unexpected{ gh_cli_credential_error{ github_cli_process_failure::start_failure } };
			}
		}
#if defined(_WIN32)
		return std::unexpected{ gh_cli_credential_error{ github_cli_process_failure::start_failure } };
#else
		const auto directory = std::make_shared<private_account_directory>(aConfiguration.NeutralWorkingDirectory, *aConfiguration.Pin);
		if (!directory->valid()) {
			return std::unexpected{ gh_cli_credential_error{ github_cli_process_failure::start_failure } };
		}
		auto request = constrain_gh_writes(github_cli_process_request{
			.Executable          = path_bytes(aConfiguration.Executable),
			.Arguments           = { "auth", "token", "--hostname", aConfiguration.Pin->Hostname, "--user", aConfiguration.Pin->Login },
			.Environment         = aConfiguration.Environment,
			.WorkingDirectory    = path_bytes(aConfiguration.NeutralWorkingDirectory),
			.StandardInputLimit  = 1U,
			.StandardOutputLimit = cli_sensitive_bytes::capacity,
			.StandardErrorLimit  = 2048U,
			.Timeout             = std::chrono::seconds{ 10 },
			.ReapTimeout         = process_reap_timeout,
		});
		append_environment(request.Environment, "GH_CONFIG_DIR", path_bytes(directory->path()));
		if (!bounded_environment(request.Environment)) {
			return std::unexpected{ gh_cli_credential_error{ github_cli_process_failure::invalid_request } };
		}
		auto acquired = aProcess.acquire_sensitive(request, aStop, directory);
		if (!acquired) {
			return std::unexpected{ gh_cli_credential_error{ acquired.error(), acquired.error() == github_cli_process_failure::nonzero_exit } };
		}
		return *acquired;
#endif
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
		if (aCandidate.Https.CertificateFile.has_value()) {
			std::filesystem::path certificate;
			if (!canonical_file(*aCandidate.Https.CertificateFile, certificate)) {
				return std::unexpected{
					gh_cli_configuration_failure::invalid_certificate_file
				};
			}
			append_environment(
				environment,
				"SSL_CERT_FILE",
				path_bytes(certificate));
		}
		if (aCandidate.Https.CertificateDirectory.has_value()) {
			std::filesystem::path certificate_directory;
			if (!canonical_directory(
					*aCandidate.Https.CertificateDirectory,
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
			"--include",
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

		return constrain_gh_writes(github_cli_process_request{
			.Executable              = path_bytes(aConfiguration.Executable),
			.Arguments               = std::move(arguments),
			.Environment             = aConfiguration.Environment,
			.WorkingDirectory        = path_bytes(aConfiguration.NeutralWorkingDirectory),
			.StandardInput           = aRequest.Wire.Body,
			.StandardInputLimit      = maximum_standard_input_bytes,
			.StandardOutputLimit     = maximum_standard_output_bytes,
			.StandardErrorLimit      = maximum_standard_error_bytes,
			.Timeout                 = process_timeout,
			.ReapTimeout             = process_reap_timeout,
			.PreserveCompletedOutput = true,
		});
	}

	auto compile_gh_cli_version_request(
		const validated_gh_cli_configuration& aConfiguration)
		-> github_cli_process_request
	{
		return constrain_gh_writes(github_cli_process_request{
			.Executable          = path_bytes(aConfiguration.Executable),
			.Arguments           = { "--version" },
			.Environment         = aConfiguration.Environment,
			.WorkingDirectory    = path_bytes(aConfiguration.NeutralWorkingDirectory),
			.StandardInput       = { },
			.StandardInputLimit  = 1U,
			.StandardOutputLimit = 2048U,
			.StandardErrorLimit  = 2048U,
			.Timeout             = std::chrono::seconds{ 5 },
			.ReapTimeout         = process_reap_timeout,
		});
	}

	auto supported_gh_cli_version(const std::string_view aOutput) -> bool
	{
		constexpr auto prefix = std::string_view{ "gh version " };
		if (!aOutput.starts_with(prefix)) {
			return false;
		}
		auto                     remaining = aOutput.substr(prefix.size());
		std::array<unsigned, 3U> version{ };
		for (std::size_t index = 0U; index < version.size(); ++index) {
			const auto parsed = std::from_chars(remaining.data(), remaining.data() + remaining.size(), version[index]);
			if (parsed.ec != std::errc{ } || parsed.ptr == remaining.data()) {
				return false;
			}
			remaining.remove_prefix(static_cast<std::size_t>(parsed.ptr - remaining.data()));
			if (index + 1U < version.size()) {
				if (remaining.empty() || remaining.front() != '.') {
					return false;
				}
				remaining.remove_prefix(1U);
			}
		}
		return (remaining.empty() || remaining.front() == ' ' || remaining.front() == '\n' || remaining.front() == '-')
			&& version >= minimum_gh_cli_version;
	}

	auto supported_gh_cli_auth_status_version(const std::string_view aOutput)
		-> bool
	{
		return supported_gh_cli_version(aOutput);
	}

	auto compile_gh_cli_help_request(
		const validated_gh_cli_configuration& aConfiguration,
		const gh_cli_help_topic               aTopic)
		-> github_cli_process_request
	{
		std::vector<std::string> arguments;
		switch (aTopic) {
		case gh_cli_help_topic::auth_token:          arguments = { "auth", "token", "--help" }; break;
		case gh_cli_help_topic::api:                 arguments = { "api", "--help" }; break;
		case gh_cli_help_topic::repo_clone:          arguments = { "repo", "clone", "--help" }; break;
		case gh_cli_help_topic::auth_git_credential: arguments = { "auth", "git-credential", "--help" }; break;
		}
		return constrain_gh_writes(github_cli_process_request{
			.Executable          = path_bytes(aConfiguration.Executable),
			.Arguments           = std::move(arguments),
			.Environment         = aConfiguration.Environment,
			.WorkingDirectory    = path_bytes(aConfiguration.NeutralWorkingDirectory),
			.StandardInput       = { },
			.StandardInputLimit  = 1U,
			.StandardOutputLimit = 32U * 1024U,
			.StandardErrorLimit  = 2048U,
			.Timeout             = std::chrono::seconds{ 10 },
			.ReapTimeout         = process_reap_timeout,
		});
	}

	auto gh_cli_help_admits(const gh_cli_help_topic aTopic, const std::string_view aHelp) -> bool
	{
		const auto lines = [aHelp](auto aPredicate) {
			auto remaining = aHelp;
			while (!remaining.empty()) {
				const auto end  = remaining.find('\n');
				auto       line = remaining.substr(0U, end);
				remaining.remove_prefix(end == std::string_view::npos ? remaining.size() : end + 1U);
				line.remove_prefix(std::min(line.find_first_not_of(" \t"), line.size()));
				if (aPredicate(line)) {
					return true;
				}
			}
			return false;
		};
		// An option row is `--name ...` or `-x, --name ...`; prose mentions do not count.
		const auto option = [&lines](std::string_view aName) {
			return lines([aName](std::string_view aLine) {
				if (aLine.size() >= 4U && aLine[0] == '-' && aLine[1] != '-' && aLine[2] == ',' && aLine[3] == ' ') {
					aLine.remove_prefix(4U);
				}
				if (!aLine.starts_with("--") || !aLine.substr(2U).starts_with(aName)) {
					return false;
				}
				const auto rest = aLine.substr(2U + aName.size());
				return rest.empty() || rest.front() == ' ' || rest.front() == '=';
			});
		};
		const auto usage = [&lines](std::string_view aCommand, std::string_view aRequired) {
			return lines([aCommand, aRequired](std::string_view aLine) {
				return aLine.starts_with(aCommand) && (aLine.size() == aCommand.size() || aLine[aCommand.size()] == ' ')
					&& aLine.find(aRequired) != std::string_view::npos;
			});
		};
		switch (aTopic) {
		case gh_cli_help_topic::auth_token:
			return option("hostname") && option("user") && usage("gh auth token", "[flags]");
		case gh_cli_help_topic::api:
			return option("hostname") && option("include") && option("method") && option("header") && option("input");
		case gh_cli_help_topic::repo_clone:
			return usage("gh repo clone", "[-- <gitflags>...]") && option("no-upstream");
		case gh_cli_help_topic::auth_git_credential:
			return usage("gh auth git-credential", "gh auth git-credential");
		}
		return false;
	}

	auto compile_gh_cli_auth_status_request(
		const validated_gh_cli_configuration& aConfiguration)
		-> github_cli_process_request
	{
		return constrain_gh_writes(github_cli_process_request{
			.Executable              = path_bytes(aConfiguration.Executable),
			.Arguments               = { "auth", "status", "--active", "--json", "hosts" },
			.Environment             = aConfiguration.Environment,
			.WorkingDirectory        = path_bytes(aConfiguration.NeutralWorkingDirectory),
			.StandardInput           = { },
			.StandardInputLimit      = 1U,
			.StandardOutputLimit     = maximum_auth_status_bytes,
			.StandardErrorLimit      = 8U * 1024U,
			.Timeout                 = std::chrono::seconds{ 10 },
			.ReapTimeout             = process_reap_timeout,
			.PreserveCompletedOutput = true,
		});
	}

	auto compile_gh_cli_auth_inventory_request(
		const validated_gh_cli_configuration& aConfiguration)
		-> github_cli_process_request
	{
		return constrain_gh_writes(github_cli_process_request{
			.Executable              = path_bytes(aConfiguration.Executable),
			.Arguments               = { "auth", "status", "--json", "hosts" },
			.Environment             = aConfiguration.Environment,
			.WorkingDirectory        = path_bytes(aConfiguration.NeutralWorkingDirectory),
			.StandardInput           = { },
			.StandardInputLimit      = 1U,
			.StandardOutputLimit     = maximum_auth_status_bytes,
			.StandardErrorLimit      = 8U * 1024U,
			.Timeout                 = std::chrono::seconds{ 20 },
			.ReapTimeout             = process_reap_timeout,
			.PreserveCompletedOutput = true,
		});
	}

	auto parse_gh_cli_auth_inventory(const std::string_view aOutput)
		-> std::expected<std::vector<gh_cli_inventory_account>, gh_cli_auth_status_failure>
	{
		if (aOutput.size() > maximum_auth_status_bytes) {
			return std::unexpected{ gh_cli_auth_status_failure::response_limit };
		}
		using namespace auth_status_json_detail;
		auto parsed = document{ };
		if (glz::read<options{ }>(parsed, aOutput)) {
			return std::unexpected{ gh_cli_auth_status_failure::malformed_response };
		}
		if (parsed.Hosts.size() > maximum_auth_status_hosts) {
			return std::unexpected{ gh_cli_auth_status_failure::response_limit };
		}
		auto result = std::vector<gh_cli_inventory_account>{ };
		for (const auto& [hostname, accounts] : parsed.Hosts) {
			if (accounts.size() > maximum_auth_status_accounts - result.size()) {
				return std::unexpected{ gh_cli_auth_status_failure::response_limit };
			}
			if (!application::valid_github_canonical_hostname(hostname)) {
				return std::unexpected{ gh_cli_auth_status_failure::malformed_response };
			}
			const auto first  = result.size();
			auto       active = false;
			for (const auto& account : accounts) {
				const auto duplicate = std::ranges::any_of(result.begin() + static_cast<std::ptrdiff_t>(first), result.end(),
														   [&](const gh_cli_inventory_account& aSeen) { return aSeen.Login == account.Login; });
				if (account.Host != hostname || !valid_inventory_login(account.Login) || account.State.size() > 32U
					|| account.TokenSource.size() > 64U || duplicate || (account.Active && active)) {
					return std::unexpected{ gh_cli_auth_status_failure::malformed_response };
				}
				active = active || account.Active;
				result.push_back(gh_cli_inventory_account{
					.Hostname     = hostname,
					.Login        = account.Login,
					.Active       = account.Active,
					.SignedIn     = account.State == "success",
					.KeychainHeld = account.TokenSource == "keyring",
				});
			}
		}
		return result;
	}

	auto parse_gh_cli_auth_status(const std::string_view aOutput)
		-> std::expected<
			std::vector<gh_cli_authenticated_account>,
			gh_cli_auth_status_failure>
	{
		if (aOutput.size() > maximum_auth_status_bytes) {
			return std::unexpected{ gh_cli_auth_status_failure::response_limit };
		}
		using namespace auth_status_json_detail;
		auto parsed = document{ };
		if (glz::read<options{ }>(parsed, aOutput)) {
			return std::unexpected{ gh_cli_auth_status_failure::malformed_response };
		}
		if (parsed.Hosts.size() > maximum_auth_status_hosts) {
			return std::unexpected{ gh_cli_auth_status_failure::response_limit };
		}

		auto result       = std::vector<gh_cli_authenticated_account>{ };
		auto accountCount = std::size_t{ 0U };
		result.reserve(parsed.Hosts.size());
		for (const auto& [hostname, accounts] : parsed.Hosts) {
			if (accounts.size() > maximum_auth_status_accounts - accountCount) {
				return std::unexpected{ gh_cli_auth_status_failure::response_limit };
			}
			accountCount += accounts.size();
			if (!application::valid_github_canonical_hostname(hostname)) {
				return std::unexpected{ gh_cli_auth_status_failure::malformed_response };
			}
			auto admitted = std::optional<gh_cli_authenticated_account>{ };
			for (const auto& account : accounts) {
				const auto validLogin = !account.Login.empty() && account.Login.size() <= 128U
					&& std::ranges::all_of(account.Login, [](const unsigned char aByte) {
						   return aByte >= 0x21U && aByte <= 0x7eU;
					   });
				if (account.Host != hostname || !validLogin || account.State.size() > 32U) {
					return std::unexpected{ gh_cli_auth_status_failure::malformed_response };
				}
				if (!account.Active || account.State != "success") {
					continue;
				}
				if (admitted.has_value()) {
					return std::unexpected{ gh_cli_auth_status_failure::malformed_response };
				}
				admitted = gh_cli_authenticated_account{
					.Hostname = hostname,
					.Login    = account.Login,
				};
			}
			if (admitted.has_value()) {
				result.push_back(std::move(*admitted));
			}
		}
		return result;
	}

	auto compile_gh_cli_exact_blob_request(
		const validated_gh_cli_configuration& aConfiguration, const validated_github_host_profile& aProfile,
		const application::git_smart_exact_blob_request& aRequest) -> std::optional<github_cli_process_request>
	{
#if !defined(__APPLE__)
		static_cast<void>(aConfiguration);
		static_cast<void>(aProfile);
		static_cast<void>(aRequest);
		return std::nullopt;
#else
		const auto& context = aRequest.Context;
		if (!application::valid(context) || !aRequest.ExpectedBlob.valid()
			|| aRequest.ExpectedBlob.algorithm() != core::git_hash_algorithm::sha1
			|| aRequest.MaximumBytes == 0U || aRequest.MaximumBytes > application::maximum_git_exact_blob_bytes
			|| aProfile.authentication_transport() != application::github_authentication_transport::github_cli
			|| context.Authority.Profile != aProfile.profile() || context.Authority.Host != aProfile.host()
			|| context.Authority.ProfileRevision.value() != aProfile.revision().value()) {
			return std::nullopt;
		}
		const auto endpoint = "repos/" + context.Repository.Owner + "/" + context.Repository.Name
			+ "/git/blobs/" + std::string{ core::canonical_hex(aRequest.ExpectedBlob).view() };
		return constrain_gh_writes(github_cli_process_request{
			.Executable          = path_bytes(aConfiguration.Executable),
			.Arguments           = { "api", endpoint, "--hostname", std::string{ aProfile.hostname() }, "--method", "GET",
									 "--header", "Accept: application/vnd.github+json", "--header", "X-GitHub-Api-Version: 2022-11-28" },
			.Environment         = aConfiguration.Environment,
			.WorkingDirectory    = path_bytes(aConfiguration.NeutralWorkingDirectory),
			.StandardInput       = { },
			.StandardInputLimit  = 1U,
			.StandardOutputLimit = aRequest.MaximumBytes * 2U + 4096U > github_cli_process_max_standard_output_bytes
				? github_cli_process_max_standard_output_bytes
				: aRequest.MaximumBytes * 2U + 4096U,
			.StandardErrorLimit  = maximum_standard_error_bytes,
			.Timeout             = process_timeout,
			.ReapTimeout         = process_reap_timeout,
		});
#endif
	}

	auto compile_gh_cli_exact_blobs_request(
		const validated_gh_cli_configuration& aConfiguration, const validated_github_host_profile& aProfile,
		const application::git_smart_exact_blobs_request& aRequest) -> std::optional<github_cli_process_request>
	{
#if !defined(__APPLE__)
		static_cast<void>(aConfiguration);
		static_cast<void>(aProfile);
		static_cast<void>(aRequest);
		return std::nullopt;
#else
		const auto& context = aRequest.Context;
		if (!application::valid(context) || !application::valid(aRequest)
			|| std::ranges::any_of(aRequest.ExpectedBlobs, [](const core::git_blob_oid& aBlob) {
				   return aBlob.algorithm() != core::git_hash_algorithm::sha1;
			   })
			|| aProfile.authentication_transport() != application::github_authentication_transport::github_cli || context.Authority.Profile != aProfile.profile() || context.Authority.Host != aProfile.host() || context.Authority.ProfileRevision.value() != aProfile.revision().value()) {
			return std::nullopt;
		}
		// Fixed text: only the number of aliased objects varies.
		auto query     = std::string{ "query($owner:String!,$name:String!" };
		auto selection = std::string{ };
		auto variables = glz::generic::object_t{
			{ "owner", context.Repository.Owner },
			{ "name", context.Repository.Name },
		};
		for (std::size_t index = 0U; index < aRequest.ExpectedBlobs.size(); ++index) {
			const auto name = "o" + std::to_string(index);
			query += ",$" + name + ":GitObjectID!";
			selection += " b" + std::to_string(index) + ":object(oid:$" + name
				+ "){... on Blob{oid byteSize isBinary isTruncated text}}";
			variables.emplace(name, std::string{ core::canonical_hex(aRequest.ExpectedBlobs[index]).view() });
		}
		// [[BPR-1370]]. The same request names the account that read the
		// blobs, so the read needs no separate account observation.
		query += "){viewer{login databaseId} repository(owner:$owner,name:$name){" + selection + "}}";
		// [[BPR-1359]]. The object itself. Wrapping it in braces as
		// `glz::generic{ object_t{ … } }` builds an array holding it, which
		// GitHub refuses with HTTP 400 ("Body should be a JSON object").
		const auto document = glz::generic::object_t{
			{ "query", std::move(query) },
			{ "variables", std::move(variables) },
		};
		auto body = glz::write_json(document);
		if (!body || body->size() > maximum_standard_input_bytes) {
			return std::nullopt;
		}
		return constrain_gh_writes(github_cli_process_request{
			.Executable          = path_bytes(aConfiguration.Executable),
			.Arguments           = { "api", "graphql", "--hostname", std::string{ aProfile.hostname() }, "--method", "POST", "--input", "-" },
			.Environment         = aConfiguration.Environment,
			.WorkingDirectory    = path_bytes(aConfiguration.NeutralWorkingDirectory),
			.StandardInput       = std::move(*body),
			.StandardInputLimit  = maximum_standard_input_bytes,
			.StandardOutputLimit = github_cli_process_max_standard_output_bytes,
			.StandardErrorLimit  = maximum_standard_error_bytes,
			.Timeout             = process_timeout,
			.ReapTimeout         = process_reap_timeout,
		});
#endif
	}

	auto compile_gh_cli_git_clone_request(
		const validated_gh_cli_configuration&         aConfiguration,
		const validated_github_host_profile&          aProfile,
		const application::git_smart_request_context& aContext, core::git_commit_oid aCommit,
		const std::filesystem::path& aGit, const std::filesystem::path& aRoot,
		std::size_t aIndex) -> std::optional<github_cli_process_request>
	{
#if !defined(__APPLE__)
		static_cast<void>(aConfiguration);
		static_cast<void>(aProfile);
		static_cast<void>(aContext);
		static_cast<void>(aCommit);
		static_cast<void>(aGit);
		static_cast<void>(aRoot);
		static_cast<void>(aIndex);
		return std::nullopt;
#else
		std::error_code error;
		const auto      root = std::filesystem::canonical(aRoot, error);
		const auto      name = aRoot.filename().string();
		if (error || root != aRoot || root.parent_path() != "/private/tmp"
			|| !name.starts_with("bpr-cli-git-") || name.size() > 64U
			|| !std::ranges::all_of(name, [](unsigned char aByte) {
				   return (aByte >= 'a' && aByte <= 'z') || (aByte >= 'A' && aByte <= 'Z')
					   || (aByte >= '0' && aByte <= '9') || aByte == '-';
			   })
			|| aIndex > 1U || !application::valid(aContext) || !aCommit.valid() || aCommit.algorithm() != core::git_hash_algorithm::sha1 || aProfile.authentication_transport() != application::github_authentication_transport::github_cli || aContext.Authority.Profile != aProfile.profile() || aContext.Authority.Host != aProfile.host() || aContext.Authority.ProfileRevision.value() != aProfile.revision().value()) {
			return std::nullopt;
		}
		if (!aGit.is_absolute() || std::filesystem::canonical(aGit, error) != aGit || error
			|| !std::filesystem::is_regular_file(aGit, error) || error || !executable_permissions(aGit)) {
			return std::nullopt;
		}
		auto environment = aConfiguration.Environment;
		append_environment(environment, "PATH", path_bytes(aGit.parent_path()));
		append_fixed_environment(environment, "GIT_CONFIG_NOSYSTEM=1");
		append_fixed_environment(environment, "GIT_CONFIG_GLOBAL=/dev/null");
		append_fixed_environment(environment, "GIT_ATTR_NOSYSTEM=1");
		append_fixed_environment(environment, "GIT_TERMINAL_PROMPT=0");
		append_fixed_environment(environment, "GIT_ALLOW_PROTOCOL=https");
		if (aContext.AcquisitionMode != application::git_smart_acquisition_mode::complete_closure
			&& aContext.AcquisitionMode != application::git_smart_acquisition_mode::sparse_commit_trees) {
			return std::nullopt;
		}
		std::vector<std::string> arguments = { "repo", "clone", "https://" + std::string{ aProfile.hostname() } + "/" + aContext.Repository.Owner + "/" + aContext.Repository.Name + ".git",
											   path_bytes(gh_cli_git_clone_store(root, aIndex)), "--no-upstream", "--",
											   "--bare", "--quiet", "--revision=" + std::string{ core::canonical_hex(aCommit).view() },
											   "--template=" + path_bytes(root / "empty"), "--config", "transfer.unpackLimit=0",
											   "--config", "fetch.unpackLimit=0", "--config", "core.hooksPath=/dev/null",
											   "--config", "core.fsmonitor=false" };
		if (aContext.AcquisitionMode == application::git_smart_acquisition_mode::sparse_commit_trees) {
			arguments.emplace_back("--depth=1");
			arguments.emplace_back("--filter=blob:none");
		}
		return constrain_gh_writes(github_cli_process_request{
									   .Executable              = path_bytes(aConfiguration.Executable),
									   .Arguments               = std::move(arguments),
									   .Environment             = std::move(environment),
									   .WorkingDirectory        = path_bytes(aConfiguration.NeutralWorkingDirectory),
									   .StandardInput           = { },
									   .StandardInputLimit      = 1U,
									   .StandardOutputLimit     = 4096U,
									   .StandardErrorLimit      = maximum_standard_error_bytes,
									   .Timeout                 = std::chrono::minutes{ 2 },
									   .ReapTimeout             = process_reap_timeout,
									   .PreserveCompletedOutput = true,
								   },
								   path_bytes(root));
#endif
	}

	auto gh_cli_git_clone_store(const std::filesystem::path& aRoot, const std::size_t aIndex)
		-> std::filesystem::path
	{
		return aRoot / ("clone-" + std::to_string(aIndex) + ".git");
	}

	auto compile_gh_cli_local_git_request(
		const std::filesystem::path& aGit, const std::filesystem::path& aRoot,
		std::vector<std::string> aArguments, std::string aInput,
		const std::size_t aStandardOutputLimit) -> github_cli_process_request
	{
		auto environment = std::vector<github_cli_process_environment_entry>{ };
		append_fixed_environment(environment, "GIT_CONFIG_NOSYSTEM=1");
		append_fixed_environment(environment, "GIT_CONFIG_GLOBAL=/dev/null");
		append_fixed_environment(environment, "GIT_ALLOW_PROTOCOL=");
		append_fixed_environment(environment, "GIT_NO_LAZY_FETCH=1");
		append_fixed_environment(environment, "GIT_TERMINAL_PROMPT=0");
		append_fixed_environment(environment, "GIT_ATTR_NOSYSTEM=1");
		append_environment(environment, "PATH", path_bytes(aGit.parent_path()));
		return github_cli_process_request{
			.Executable          = path_bytes(aGit),
			.Arguments           = std::move(aArguments),
			.Environment         = std::move(environment),
			.WorkingDirectory    = path_bytes(aRoot),
			.StandardInput       = std::move(aInput),
			.StandardInputLimit  = application::maximum_git_smart_request_bytes,
			.StandardOutputLimit = aStandardOutputLimit,
			.StandardErrorLimit  = 64U * 1024U,
			.Timeout             = std::chrono::minutes{ 2 },
			.ReapTimeout         = std::chrono::seconds{ 2 },
		};
	}

	gh_cli_transport::gh_cli_transport(
		const validated_github_host_profile&  aProfile,
		const validated_gh_cli_configuration& aConfiguration,
		github_cli_process_port& aProcess, cli_transient_credential aCredential)
		: Profile{ aProfile }
		, Configuration{ aConfiguration }
		, Process{ aProcess }
		, Credential{ std::move(aCredential) }
	{
		for (const auto& entry : Configuration.Environment) {
#if defined(_WIN32)
			if (entry.Name == "APPDATA") {
				ConfigurationDirectory = std::filesystem::path{ entry.Value } / "GitHub CLI";
			}
#else
			if (entry.Name == "HOME") {
				ConfigurationDirectory = std::filesystem::path{ entry.Value } / ".config" / "gh";
			}
#endif
		}
		ConfigurationFiles = observe_configuration_files();
		std::error_code error;
		ExecutableSize = std::filesystem::file_size(Configuration.executable(), error);
		if (error) {
			return;
		}
		ExecutableWriteTime   = std::filesystem::last_write_time(Configuration.executable(), error);
		ConfigurationObserved = !error;
	}

	auto gh_cli_transport::acquire_operation(std::stop_token aStop)
		-> std::expected<std::shared_ptr<gh_cli_transport>, gh_cli_credential_error>
	{
		if (Credential) {
			return std::shared_ptr<gh_cli_transport>{ };
		}
		if (!Configuration.pin() || Configuration.pin()->Hostname != Profile.hostname()
			|| Configuration.pin()->Login != Profile.cli_account_login() || !configuration_unchanged()) {
			return std::unexpected{ gh_cli_credential_error{ github_cli_process_failure::invalid_request } };
		}
		try {
			auto secret = acquire_gh_cli_credential(Process, Configuration, aStop);
			if (!secret) {
				return std::unexpected{ secret.error() };
			}
			return std::make_shared<gh_cli_transport>(Profile, Configuration, Process, std::move(*secret));
		} catch (...) {
			return std::unexpected{ gh_cli_credential_error{ github_cli_process_failure::start_failure } };
		}
	}
	auto gh_cli_transport::begin_operation(std::stop_token aStop) -> github_wire_operation
	{
		if (!Credential && (!Configuration.pin() || Configuration.pin()->Hostname != Profile.hostname() || Configuration.pin()->Login != Profile.cli_account_login() || !configuration_unchanged())) {
			return std::unexpected{ application::github_provider_error{ .Failure = application::github_provider_failure::authentication } };
		}
		auto scope = acquire_operation(aStop);
		if (!scope) {
			const bool credentialUnavailable = scope.error().CredentialUnavailable;
			auto       error                 = application::github_provider_error{ .Failure = credentialUnavailable
																					   ? application::github_provider_failure::authentication
																					   : process_failure(scope.error().Failure) };
			error.CliCredentialUnavailable   = credentialUnavailable;
			return std::unexpected{ error };
		}
		return *scope;
	}
	auto gh_cli_transport::begin_mutation_operation(std::stop_token aStop) -> github_mutation_operation
	{
		auto scope = acquire_operation(aStop);
		if (!scope) {
			if (scope.error().Failure == github_cli_process_failure::cancelled) {
				return std::unexpected{ application::review_mutation_failure::cancelled };
			}
			return std::unexpected{ scope.error().CredentialUnavailable ? application::review_mutation_failure::credential_unavailable : application::review_mutation_failure::unavailable };
		}
		return *scope;
	}

	auto gh_cli_transport::observe_configuration_files() const
		-> std::optional<std::array<configuration_metadata, 3>>
	{
		// [[BPR-1373]]. A pinned child reads only its own private pair, which
		// is written for each launch; the user's files are not its input, and a
		// gh auth switch that rewrites them does not reach it.
		if (Configuration.Pin) {
			return std::array<configuration_metadata, 3>{ };
		}
		if (ConfigurationDirectory.empty()) {
			return std::nullopt;
		}
		// Exactly three metadata observations; no directory enumeration or contents.
		// Same-metadata replacement, concurrent changes and keychain changes remain
		// uncontained. This detects ordinary stable changes, not account binding.
		const std::array                      paths{ ConfigurationDirectory, ConfigurationDirectory / "config.yml", ConfigurationDirectory / "hosts.yml" };
		std::array<configuration_metadata, 3> observations{ };
		for (std::size_t index = 0U; index < paths.size(); ++index) {
			std::error_code error;
			const auto      status = std::filesystem::status(paths[index], error);
			if (status.type() == std::filesystem::file_type::not_found
				&& (!error || error == std::errc::no_such_file_or_directory)) {
				continue;
			}
			if (error || (index == 0U ? !std::filesystem::is_directory(status) : !std::filesystem::is_regular_file(status))) {
				return std::nullopt;
			}
			auto& observation     = observations[index];
			observation.Type      = status.type();
			observation.Canonical = std::filesystem::canonical(paths[index], error);
			if (error) {
				return std::nullopt;
			}
			observation.Modified = std::filesystem::last_write_time(paths[index], error);
			if (error) {
				return std::nullopt;
			}
			if (index != 0U) {
				observation.Size = std::filesystem::file_size(paths[index], error);
				if (error) {
					return std::nullopt;
				}
			}
		}
		return observations;
	}

	auto gh_cli_transport::configuration_unchanged() const -> bool
	{
		if (!ConfigurationObserved || !ConfigurationFiles || observe_configuration_files() != ConfigurationFiles) {
			return false;
		}
		std::error_code error;
		const auto      executable = std::filesystem::canonical(Configuration.executable(), error);
		if (error || executable != Configuration.executable() || !executable_permissions(executable)) {
			return false;
		}
		const auto size = std::filesystem::file_size(executable, error);
		if (error || size != ExecutableSize) {
			return false;
		}
		const auto modified = std::filesystem::last_write_time(executable, error);
		if (error || modified != ExecutableWriteTime) {
			return false;
		}
		std::filesystem::path directory;
		return canonical_directory(Configuration.neutral_working_directory(), directory)
			&& directory == Configuration.neutral_working_directory() && !repository_ancestor(directory);
	}

	auto gh_cli_transport::availability_reasons() const noexcept
		-> std::array<gh_cli_availability_reason, 8>
	{
		return {
			gh_cli_availability_reason::
				opt_in_product_scope_in_progress,
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
		if (!Credential) {
			auto scope = begin_operation(aStop);
			if (!scope) {
				return github_wire_error{ .Correlation = aRequest.Correlation, .Error = scope.error() };
			}
			return (*scope)->perform(aRequest, aStop);
		}
		if (aStop.stop_requested()) {
			return github_wire_cancelled{
				.Correlation = aRequest.Correlation,
			};
		}
		if (!configuration_unchanged()) {
			return wire_error(aRequest, application::github_provider_failure::cli_unavailable);
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

		const auto result = run_gh_cli(Process, Configuration, *invocation, aStop, Credential);
		if (aStop.stop_requested() || (!result && result.error() == github_cli_process_failure::cancelled)) {
			return github_wire_cancelled{ .Correlation = aRequest.Correlation };
		}
		if (!configuration_unchanged()) {
			return wire_error(aRequest, application::github_provider_failure::cli_unavailable);
		}
		if (!result) {
			return wire_error(aRequest, process_failure(result.error()));
		}
		auto response = parse_gh_cli_response(result->StandardOutput);
		if (!response) {
			return wire_error(aRequest, response.error());
		}
		// A nonzero 2xx may carry incomplete data; never infer success from JSON.
		if (result->ExitCode != 0 && response->Status < 300) {
			if (graphql_read(aRequest.Correlation.Operation)) {
				return github_wire_error{ .Correlation = aRequest.Correlation, .Error = completed_graphql_failure(*response) };
			}
			return wire_error(aRequest, application::github_provider_failure::unavailable);
		}
		return github_wire_success{ .Correlation = aRequest.Correlation, .Response = std::move(*response) };
	}
	auto gh_cli_transport::perform_review_mutation(
		const github_mutation_wire_request& aRequest, const std::stop_token aStop,
		github_mutation_transmission_witness aWitness) -> github_mutation_wire_completion
	{
		if (!Credential) {
			auto scope = begin_mutation_operation(aStop);
			if (!scope) {
				return github_mutation_wire_error{ };
			}
			return (*scope)->perform_review_mutation(aRequest, aStop, std::move(aWitness));
		}
		using transmission = github_mutation_transmission;
		if (aStop.stop_requested()) {
			return github_mutation_wire_cancelled{ };
		}
		const auto& authority = aRequest.Context.Authority;
		if (Profile.authentication_transport() != application::github_authentication_transport::github_cli
			|| !application::valid(aRequest.Context) || authority.Profile != Profile.profile()
			|| authority.ProfileRevision != Profile.revision() || authority.Host != Profile.host()
			|| authority.Hostname != application::github_canonical_hostname{ Profile.hostname() }
			|| authority.ServerContract != Profile.server_contract() || aRequest.Wire.Server != Profile.server_contract()
			|| !validate_outbound_review_mutation_request(aRequest.Wire) || !configuration_unchanged()) {
			return github_mutation_wire_error{ };
		}
		const auto correlated = std::visit([&](const auto& aSemantic) {
			using semantic_type      = std::remove_cvref_t<decltype(aSemantic)>;
			const auto targetMatches = [&](const application::review_mutation_target_binding& aTarget) {
				return aTarget.Semantic.Host == authority.Host && aTarget.Hostname == authority.Hostname
					&& aTarget.ServerContract == authority.ServerContract;
			};
			if constexpr (std::same_as<semantic_type, application::review_mutation_target_binding>) {
				return targetMatches(aSemantic);
			} else if constexpr (std::same_as<semantic_type, application::review_mutation_acting_identity>) {
				return aSemantic.Account == authority.Account;
			} else if constexpr (std::same_as<semantic_type, review_submission_request>) {
				return targetMatches(aSemantic.Target) && aSemantic.ActingIdentity.Account == authority.Account;
			} else if constexpr (std::same_as<semantic_type, application::staged_review_request>) {
				return targetMatches(aSemantic.Binding.Target) && aSemantic.Binding.ActingIdentity.Account == authority.Account;
			} else if constexpr (std::same_as<semantic_type, application::review_mutation_request>) {
				return aSemantic.LiveBinding && targetMatches(aSemantic.LiveBinding->Target)
					&& aSemantic.ActingIdentity == authority.Account;
			} else {
				return false;
			}
		},
										   aRequest.Wire.Semantic);
		// Only the finite admitted mutation catalog may use this route.
		// [[BPR-0896]]. A reply's reconciliation observes its thread at the
		// GraphQL root too. This transport never writes a reply, so its own
		// writes never need it; a reply attempt written through another
		// transport and recovered here is refused, and stays unresolved
		// rather than being resubmitted.
		if (!correlated || aRequest.Wire.Operation == review_mutation_operation::reply_review_thread
			|| aRequest.Wire.Operation == review_mutation_operation::resolve_review_thread
			|| aRequest.Wire.Operation == review_mutation_operation::unresolve_review_thread
			|| aRequest.Wire.Operation == review_mutation_operation::observe_review_thread
			|| aRequest.Wire.Operation == review_mutation_operation::observe_review_thread_comments) {
			return github_mutation_wire_error{ };
		}
		const auto githubCom = Profile.server_contract() == application::github_server_contract::github_com_2022_11_28;
		const auto prefix    = githubCom ? std::string_view{ "/" } : std::string_view{ "/api/v3/" };
		// [[BPR-1368]]. The one GraphQL write admitted here is a tour stop
		// edited while its review is pending, sent to `gh api graphql`, which
		// names the host's own GraphQL root.
		const auto graphql  = aRequest.Wire.Operation == review_mutation_operation::staged_update_pending_comment;
		const auto exchange = review_mutation_exchange_for(aRequest.Wire.Operation);
		if (!exchange || exchange->Graphql != graphql
			|| (graphql ? aRequest.Wire.Path != (githubCom ? "/graphql" : "/api/graphql") : !aRequest.Wire.Path.starts_with(prefix))) {
			return github_mutation_wire_error{ };
		}
		const auto endpoint = graphql ? std::string{ "graphql" } : aRequest.Wire.Path.substr(prefix.size());
		if (!safe_relative_endpoint(endpoint)) {
			return github_mutation_wire_error{ };
		}
		std::string method;
		switch (aRequest.Wire.Method) {
		case http_method::get:     method = "GET"; break;
		case http_method::post:    method = "POST"; break;
		case http_method::put:     method = "PUT"; break;
		case http_method::patch:   method = "PATCH"; break;
		case http_method::delete_: method = "DELETE"; break;
		default:                   return github_mutation_wire_error{ };
		}
		const bool                 writes = exchange->Witnessed;
		github_cli_process_request invocation{
			.Executable              = path_bytes(Configuration.Executable),
			.Arguments               = { "api", endpoint, "--hostname", std::string{ Profile.hostname() }, "--include", "--method", method },
			.Environment             = Configuration.Environment,
			.WorkingDirectory        = path_bytes(Configuration.NeutralWorkingDirectory),
			.StandardInput           = aRequest.Wire.Body,
			.StandardInputLimit      = maximum_review_mutation_body_bytes,
			.StandardOutputLimit     = maximum_review_mutation_response_bytes + 32U * 1024U,
			.StandardErrorLimit      = maximum_standard_error_bytes,
			.Timeout                 = process_timeout,
			.ReapTimeout             = process_reap_timeout,
			.PreserveCompletedOutput = true,
		};
		for (const auto& header : aRequest.Wire.Headers) {
			invocation.Arguments.emplace_back("--header");
			invocation.Arguments.emplace_back(header.Name + ": " + header.Value);
		}
		if (!aRequest.Wire.Body.empty()) {
			invocation.Arguments.emplace_back("--input");
			invocation.Arguments.emplace_back("-");
		}
		if (aStop.stop_requested()) {
			return github_mutation_wire_cancelled{ };
		}
		// Persist possible transmission before launch. Even launch failure is treated
		// conservatively; no caller may repeat an ambiguous POST.
		if (writes && (!aWitness || !aWitness())) {
			return github_mutation_wire_error{ };
		}
		const auto sent   = writes ? transmission::indeterminate : transmission::definitely_not_sent;
		const auto result = run_gh_cli(Process, Configuration, constrain_gh_writes(std::move(invocation)), aStop, Credential);
		if (aStop.stop_requested() || (!result && result.error() == github_cli_process_failure::cancelled)) {
			return github_mutation_wire_cancelled{ .Transmission = sent };
		}
		if (!result || !configuration_unchanged()) {
			return github_mutation_wire_error{ .Transmission = sent };
		}
		auto response = parse_gh_cli_response(result->StandardOutput);
		if (!response || response->Body.size() > maximum_review_mutation_response_bytes
			|| (result->ExitCode != 0 && response->Status < 300)) {
			return github_mutation_wire_error{ .Transmission = sent };
		}
		return github_mutation_wire_success{ .Response = {
												 .Status = response->Status, .Headers = std::move(response->Headers), .Body = std::move(response->Body) } };
	}

	auto gh_cli_transport::perform_capability_probe(
		const github_capability_probe_wire_request& aRequest,
		const std::stop_token                       aStop) -> github_capability_probe_wire_completion
	{
		if (!Credential) {
			auto scope = begin_operation(aStop);
			if (!scope) {
				return github_capability_probe_wire_error{ .Correlation = aRequest.Correlation, .Error = scope.error() };
			}
			return (*scope)->perform_capability_probe(aRequest, aStop);
		}
		const auto error = [&aRequest](const application::github_provider_failure aFailure) {
			return github_capability_probe_wire_error{
				.Correlation = aRequest.Correlation,
				.Error       = { .Failure = aFailure, .RetryAfterSeconds = std::nullopt, .RateLimits = { }, .RateLimitCause = std::nullopt },
			};
		};
		if (aStop.stop_requested()) {
			return github_capability_probe_wire_cancelled{ .Correlation = aRequest.Correlation };
		}
		if (Profile.authentication_transport() != application::github_authentication_transport::github_cli
			|| !validate_github_capability_probe_wire_request(Profile, aRequest)) {
			return error(application::github_provider_failure::unavailable);
		}
		if (!configuration_unchanged()) {
			return error(application::github_provider_failure::cli_unavailable);
		}
		github_cli_process_request invocation{
			.Executable              = path_bytes(Configuration.Executable),
			.Arguments               = { "api", "graphql", "--hostname", std::string{ Profile.hostname() }, "--include", "--method", "POST" },
			.Environment             = Configuration.Environment,
			.WorkingDirectory        = path_bytes(Configuration.NeutralWorkingDirectory),
			.StandardInput           = aRequest.Wire.Body,
			.StandardInputLimit      = maximum_standard_input_bytes,
			.StandardOutputLimit     = maximum_standard_output_bytes,
			.StandardErrorLimit      = maximum_standard_error_bytes,
			.Timeout                 = process_timeout,
			.ReapTimeout             = process_reap_timeout,
			.PreserveCompletedOutput = true,
		};
		for (const auto& header : aRequest.Wire.Headers) {
			invocation.Arguments.emplace_back("--header");
			invocation.Arguments.emplace_back(header.Name + ": " + header.Value);
		}
		invocation.Arguments.emplace_back("--input");
		invocation.Arguments.emplace_back("-");
		const auto result = run_gh_cli(Process, Configuration, constrain_gh_writes(std::move(invocation)), aStop, Credential);
		if (aStop.stop_requested() || (!result && result.error() == github_cli_process_failure::cancelled)) {
			return github_capability_probe_wire_cancelled{ .Correlation = aRequest.Correlation };
		}
		if (!configuration_unchanged()) {
			return error(application::github_provider_failure::cli_unavailable);
		}
		if (!result) {
			return error(process_failure(result.error()));
		}
		auto parsed = parse_gh_cli_response(result->StandardOutput);
		if (!parsed) {
			return error(parsed.error());
		}
		if (result->ExitCode != 0 && parsed->Status < 300) {
			return github_capability_probe_wire_error{ .Correlation = aRequest.Correlation, .Error = completed_graphql_failure(*parsed) };
		}
		if (parsed->Completeness != page_completeness::complete) {
			return error(application::github_provider_failure::malformed_response);
		}
		capability_probe_response response{ .Status = parsed->Status, .Authenticated = true, .ContentType = { }, .Body = std::move(parsed->Body) };
		for (const auto& header : parsed->Headers) {
			const auto name = lowercase_header(header.Name);
			if (name == "content-type") {
				response.ContentType = header.Value;
			}
			if (name == "x-ratelimit-limit" || name == "x-ratelimit-remaining" || name == "x-ratelimit-reset" || name == "x-ratelimit-resource" || name == "retry-after") {
				response.RateLimitHeaders[response.RateLimitHeaderCount++] = { .Name = header.Name, .Value = header.Value };
			}
		}
		return github_capability_probe_wire_success{ .Correlation = aRequest.Correlation, .Response = std::move(response) };
	}

}
