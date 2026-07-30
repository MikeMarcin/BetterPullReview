#include "gh_cli_transport.hpp"

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

namespace bpr::adapters::github::detail
{
namespace
{
constexpr std::size_t maximum_environment_value_bytes = 4U * 1024U;
constexpr std::size_t maximum_proxy_bytes = 2U * 1024U;
constexpr std::size_t maximum_endpoint_bytes = 4U * 1024U;
constexpr std::size_t maximum_standard_input_bytes = 256U * 1024U;
constexpr std::size_t maximum_standard_output_bytes = 4U * 1024U * 1024U;
constexpr std::size_t maximum_standard_error_bytes = 64U * 1024U;
constexpr auto process_timeout = std::chrono::seconds{30};
constexpr auto process_reap_timeout = std::chrono::seconds{2};

[[nodiscard]] auto path_bytes(const std::filesystem::path& path)
    -> std::string
{
    const auto bytes = path.generic_u8string();
    return {
        reinterpret_cast<const char*>(bytes.data()),
        bytes.size(),
    };
}

[[nodiscard]] auto safe_environment_value(
    const std::string_view value) noexcept -> bool
{
    return !value.empty()
        && value.size() <= maximum_environment_value_bytes
        && std::ranges::all_of(
            value,
            [](const unsigned char character) {
                return character >= 0x20U && character != 0x7fU;
            });
}

[[nodiscard]] auto canonical_directory(
    const std::filesystem::path& candidate,
    std::filesystem::path& result) -> bool
{
    if(!candidate.is_absolute())
    {
        return false;
    }
    std::error_code error;
    if(!std::filesystem::is_directory(candidate, error) || error)
    {
        return false;
    }
    result = std::filesystem::canonical(candidate, error);
    return !error && result.is_absolute();
}

[[maybe_unused, nodiscard]] auto canonical_file(
    const std::filesystem::path& candidate,
    std::filesystem::path& result) -> bool
{
    if(!candidate.is_absolute())
    {
        return false;
    }
    std::error_code error;
    if(!std::filesystem::is_regular_file(candidate, error) || error)
    {
        return false;
    }
    result = std::filesystem::canonical(candidate, error);
    return !error && result.is_absolute();
}

[[nodiscard]] auto executable_name(
    const std::filesystem::path& path) -> bool
{
#if defined(_WIN32)
    auto name = path.filename().string();
    std::ranges::transform(
        name,
        name.begin(),
        [](const unsigned char character) {
            if(character >= 'A' && character <= 'Z')
            {
                return static_cast<char>(character - 'A' + 'a');
            }
            return static_cast<char>(character);
        });
    return name == "gh.exe";
#else
    return path.filename() == "gh";
#endif
}

[[nodiscard]] auto executable_permissions(
    const std::filesystem::path& path) -> bool
{
#if defined(_WIN32)
    return executable_name(path);
#else
    return ::access(path.c_str(), X_OK) == 0;
#endif
}

[[nodiscard]] auto repository_ancestor(
    std::filesystem::path directory) -> bool
{
    for(;;)
    {
        std::error_code error;
        const auto marker_exists =
            std::filesystem::exists(directory / ".git", error);
        if(error || marker_exists)
        {
            return true;
        }
        const auto head_path = directory / "HEAD";
        const auto head_exists = std::filesystem::exists(head_path, error);
        if(error)
        {
            return true;
        }
        const auto head_is_file =
            head_exists && std::filesystem::is_regular_file(head_path, error);
        if(error)
        {
            return true;
        }
        const auto objects_path = directory / "objects";
        const auto objects_exist =
            std::filesystem::exists(objects_path, error);
        if(error)
        {
            return true;
        }
        const auto objects_are_directory =
            objects_exist
            && std::filesystem::is_directory(objects_path, error);
        if(error)
        {
            return true;
        }
        const auto refs_path = directory / "refs";
        const auto refs_exist = std::filesystem::exists(refs_path, error);
        if(error)
        {
            return true;
        }
        const auto refs_are_directory =
            refs_exist && std::filesystem::is_directory(refs_path, error);
        if(
            error
            || (
                head_is_file && objects_are_directory
                && refs_are_directory))
        {
            return true;
        }
        const auto parent = directory.parent_path();
        if(parent.empty() || parent == directory)
        {
            return false;
        }
        directory = parent;
    }
}

[[nodiscard]] auto canonical_environment_directory(
    const std::filesystem::path& candidate,
    std::string& value) -> bool
{
    std::filesystem::path canonical;
    if(!canonical_directory(candidate, canonical))
    {
        return false;
    }
    value = path_bytes(canonical);
    return safe_environment_value(value);
}

[[maybe_unused, nodiscard]] auto valid_session_bus_address(
    const std::string_view value) noexcept -> bool
{
    return safe_environment_value(value)
        && (
            value.starts_with("unix:path=/")
            || value.starts_with("unix:abstract="))
        && value.find(';') == std::string_view::npos
        && value.find('\\') == std::string_view::npos;
}

enum class proxy_validation : std::uint8_t
{
    valid,
    invalid,
    credential_bearing,
};

[[nodiscard]] auto validate_proxy(
    const std::string_view value) noexcept -> proxy_validation
{
    if(
        value.empty() || value.size() > maximum_proxy_bytes
        || !safe_environment_value(value))
    {
        return proxy_validation::invalid;
    }
    const auto prefix = value.starts_with("https://")
        ? std::string_view{"https://"}
        : value.starts_with("http://")
        ? std::string_view{"http://"}
        : std::string_view{};
    if(prefix.empty())
    {
        return proxy_validation::invalid;
    }
    const auto authority_and_path = value.substr(prefix.size());
    const auto slash = authority_and_path.find('/');
    const auto authority = authority_and_path.substr(0U, slash);
    const auto path = slash == std::string_view::npos
        ? std::string_view{}
        : authority_and_path.substr(slash);
    if(authority.find('@') != std::string_view::npos)
    {
        return proxy_validation::credential_bearing;
    }
    if(
        authority.empty() || (path != "" && path != "/")
        || value.find('?') != std::string_view::npos
        || value.find('#') != std::string_view::npos
        || value.find('%') != std::string_view::npos
        || !std::ranges::any_of(
            authority,
            [](const unsigned char character) {
                return (
                    character >= 'a' && character <= 'z')
                    || (
                        character >= 'A' && character <= 'Z')
                    || (
                        character >= '0' && character <= '9');
            })
        || !std::ranges::all_of(
            authority,
            [](const unsigned char character) {
                return (
                    character >= 'a' && character <= 'z')
                    || (
                        character >= 'A' && character <= 'Z')
                    || (
                        character >= '0' && character <= '9')
                    || character == '.' || character == '-'
                    || character == ':' || character == '['
                    || character == ']';
            }))
    {
        return proxy_validation::invalid;
    }
    return proxy_validation::valid;
}

[[nodiscard]] auto valid_no_proxy(
    const std::string_view value) noexcept -> bool
{
    if(
        value.empty() || value.size() > maximum_proxy_bytes
        || !safe_environment_value(value)
        || !std::ranges::all_of(
            value,
            [](const unsigned char character) {
                return (
                    character >= 'a' && character <= 'z')
                    || (
                        character >= 'A' && character <= 'Z')
                    || (
                        character >= '0' && character <= '9')
                    || character == '.' || character == '-'
                    || character == '_' || character == '*'
                    || character == ',' || character == ':'
                    || character == '[' || character == ']';
            }))
    {
        return false;
    }
    std::size_t begin = 0U;
    while(begin < value.size())
    {
        const auto end = value.find(',', begin);
        const auto token = value.substr(
            begin,
            end == std::string_view::npos
                ? value.size() - begin
                : end - begin);
        if(token.empty())
        {
            return false;
        }
        if(end == std::string_view::npos)
        {
            return true;
        }
        begin = end + 1U;
    }
    return false;
}

auto append_environment(
    std::vector<github_cli_process_environment_entry>& environment,
    const std::string_view name,
    const std::string_view value) -> void
{
    environment.emplace_back(
        github_cli_process_environment_entry{
            .Name = std::string{name},
            .Value = std::string{value},
        });
}

auto append_fixed_environment(
    std::vector<github_cli_process_environment_entry>& environment,
    const std::string_view setting) -> void
{
    const auto separator = setting.find('=');
    append_environment(
        environment,
        setting.substr(0U, separator),
        setting.substr(separator + 1U));
}

[[nodiscard]] auto bounded_environment(
    const std::vector<github_cli_process_environment_entry>& environment)
    noexcept -> bool
{
    if(environment.size() > github_cli_process_max_environment_count)
    {
        return false;
    }
    std::size_t total = 0U;
    for(const auto& entry : environment)
    {
        const auto windows_empty_path =
            entry.Name == "PATH" && entry.Value.empty();
        if(
            entry.Name.empty()
            || entry.Name.size()
                > github_cli_process_max_environment_name_bytes
            || entry.Value.size() > maximum_environment_value_bytes
            || (!windows_empty_path && !safe_environment_value(entry.Value))
            || entry.Name.size()
                > github_cli_process_max_environment_bytes - total)
        {
            return false;
        }
        total += entry.Name.size();
        if(
            entry.Value.size()
            > github_cli_process_max_environment_bytes - total)
        {
            return false;
        }
        total += entry.Value.size();
    }
    return true;
}

[[nodiscard]] auto append_platform_environment(
    const gh_cli_platform_environment_facts& facts,
    std::vector<github_cli_process_environment_entry>& environment)
    -> std::optional<gh_cli_configuration_failure>
{
#if defined(__APPLE__)
    const auto* platform =
        std::get_if<gh_cli_macos_environment_facts>(&facts);
    if(platform == nullptr)
    {
        return gh_cli_configuration_failure::irrelevant_platform_facts;
    }
    std::string home;
    if(!canonical_environment_directory(platform->HomeDirectory, home))
    {
        return gh_cli_configuration_failure::invalid_platform_environment;
    }
    append_environment(environment, "HOME", home);
#elif defined(_WIN32)
    const auto* platform =
        std::get_if<gh_cli_windows_environment_facts>(&facts);
    if(platform == nullptr)
    {
        return gh_cli_configuration_failure::irrelevant_platform_facts;
    }
    std::string profile;
    std::string roaming;
    std::string local;
    std::string system_root;
    if(
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
            system_root))
    {
        return gh_cli_configuration_failure::invalid_platform_environment;
    }
    append_environment(environment, "USERPROFILE", profile);
    append_environment(environment, "APPDATA", roaming);
    append_environment(environment, "LOCALAPPDATA", local);
    append_environment(environment, "SystemRoot", system_root);
    append_environment(environment, "PATH", "");
#elif defined(__linux__)
    const auto* platform =
        std::get_if<gh_cli_linux_environment_facts>(&facts);
    if(platform == nullptr)
    {
        return gh_cli_configuration_failure::irrelevant_platform_facts;
    }
    std::string home;
    std::string runtime;
    if(
        !canonical_environment_directory(platform->HomeDirectory, home)
        || !canonical_environment_directory(
            platform->RuntimeDirectory,
            runtime)
        || !valid_session_bus_address(platform->SessionBusAddress))
    {
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

[[nodiscard]] auto expected_server_contract(
    const validated_github_host_profile& profile)
    -> std::optional<server_contract>
{
    switch(profile.server_contract())
    {
    case application::github_server_contract::github_com_2022_11_28:
        return server_contract::github_com;
    case application::github_server_contract::ghes_3_18_2022_11_28:
        return server_contract::ghes_3_18;
    }
    return std::nullopt;
}

[[nodiscard]] auto valid_parent_correlation(
    const github_wire_request& request,
    const validated_github_host_profile& profile) -> bool
{
    return std::visit(
        [&request, &profile](const auto& parent) {
            using parent_type = std::remove_cvref_t<decltype(parent)>;
            if constexpr(
                std::same_as<
                    parent_type,
                    core::github_session_request_context>)
            {
                return bpr::application::detail::valid_context(parent)
                    && parent.Profile == profile.profile()
                    && request.Correlation.Operation
                        == read_operation::get_viewer_identity;
            }
            else
            {
                if(
                    request.Correlation.Operation
                        == read_operation::get_viewer_identity
                    || !bpr::application::detail::valid_context(parent)
                    || parent.Profile != profile.profile()
                    || parent.Host != profile.host())
                {
                    return false;
                }
                const auto& semantic = request.Wire.Semantic;
                if(
                    semantic.Owner != parent.Repository.Owner
                    || semantic.Repository != parent.Repository.Name)
                {
                    return false;
                }
                return semantic.PullRequestNumber == 0U
                    || semantic.PullRequestNumber
                        == parent.Number.value();
            }
        },
        request.Correlation.Parent);
}

[[nodiscard]] auto validate_correlation(
    const github_wire_request& request,
    const validated_github_host_profile& profile)
    -> std::optional<gh_cli_invocation_failure>
{
    if(
        request.Correlation.ProfileRevision != profile.revision()
        || request.Correlation.Host != profile.host()
        || request.Correlation.Operation != request.Wire.Operation
        || request.Correlation.Operation
            != request.Wire.Semantic.Operation
        || request.Correlation.Page != request.Wire.Semantic.Page
        || !valid_parent_correlation(request, profile))
    {
        return
            gh_cli_invocation_failure::profile_correlation_mismatch;
    }
    const auto server = expected_server_contract(profile);
    if(
        !server.has_value() || request.Wire.Server != *server
        || request.Wire.Semantic.Server != *server)
    {
        return gh_cli_invocation_failure::server_contract_mismatch;
    }
    return std::nullopt;
}

[[nodiscard]] auto safe_relative_endpoint(
    const std::string_view endpoint) noexcept -> bool
{
    return !endpoint.empty() && endpoint.size() <= maximum_endpoint_bytes
        && endpoint.front() != '-' && endpoint.front() != '@'
        && endpoint.find("://") == std::string_view::npos
        && endpoint.find('{') == std::string_view::npos
        && endpoint.find('}') == std::string_view::npos
        && endpoint.find('\\') == std::string_view::npos
        && std::ranges::none_of(
            endpoint,
            [](const unsigned char character) {
                return character <= 0x20U || character == 0x7fU;
            });
}

[[nodiscard]] auto translated_endpoint(
    const validated_github_host_profile& profile,
    const wire_request& wire) -> std::optional<std::string>
{
    std::string endpoint;
    if(wire.Operation == read_operation::list_review_threads)
    {
        endpoint = "graphql";
    }
    else if(
        profile.server_contract()
        == application::github_server_contract::github_com_2022_11_28)
    {
        if(
            wire.Path.size() < 2U || wire.Path.front() != '/'
            || wire.Path[1U] == '/')
        {
            return std::nullopt;
        }
        endpoint = wire.Path.substr(1U);
    }
    else
    {
        constexpr auto prefix = std::string_view{"/api/v3/"};
        if(!wire.Path.starts_with(prefix))
        {
            return std::nullopt;
        }
        endpoint = wire.Path.substr(prefix.size());
    }
    if(!wire.Query.empty())
    {
        endpoint += "?";
        endpoint += wire.Query;
    }
    if(!safe_relative_endpoint(endpoint))
    {
        return std::nullopt;
    }
    return endpoint;
}

[[nodiscard]] auto wire_error(
    const github_wire_request& request,
    const application::github_provider_failure failure)
    -> github_wire_completion
{
    return github_wire_error{
        .Correlation = request.Correlation,
        .Error =
            application::github_provider_error{
                .Failure = failure,
                .RetryAfterSeconds = std::nullopt,
            },
    };
}
}

validated_gh_cli_configuration::validated_gh_cli_configuration(
    std::filesystem::path executable,
    std::filesystem::path neutral_working_directory,
    std::vector<github_cli_process_environment_entry> environment)
    : Executable{std::move(executable)}
    , NeutralWorkingDirectory{std::move(neutral_working_directory)}
    , Environment{std::move(environment)}
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
    const gh_cli_configuration_candidate& candidate)
    -> gh_cli_configuration_result
{
    if(!candidate.Executable.is_absolute())
    {
        return std::unexpected{
            gh_cli_configuration_failure::executable_not_absolute};
    }
    std::error_code error;
    if(!std::filesystem::exists(candidate.Executable, error) || error)
    {
        return std::unexpected{
            gh_cli_configuration_failure::executable_not_found};
    }
    if(!std::filesystem::is_regular_file(candidate.Executable, error) || error)
    {
        return std::unexpected{
            gh_cli_configuration_failure::executable_not_regular};
    }
    auto executable =
        std::filesystem::canonical(candidate.Executable, error);
    if(error)
    {
        return std::unexpected{
            gh_cli_configuration_failure::executable_not_found};
    }
    if(!executable_name(executable))
    {
        return std::unexpected{
            gh_cli_configuration_failure::executable_name_mismatch};
    }
    if(!executable_permissions(executable))
    {
        return std::unexpected{
            gh_cli_configuration_failure::executable_not_executable};
    }

    if(!candidate.NeutralWorkingDirectory.is_absolute())
    {
        return std::unexpected{
            gh_cli_configuration_failure::working_directory_not_absolute};
    }
    if(
        !std::filesystem::exists(
            candidate.NeutralWorkingDirectory,
            error)
        || error)
    {
        return std::unexpected{
            gh_cli_configuration_failure::working_directory_not_found};
    }
    if(
        !std::filesystem::is_directory(
            candidate.NeutralWorkingDirectory,
            error)
        || error)
    {
        return std::unexpected{
            gh_cli_configuration_failure::working_directory_not_directory};
    }
    auto working_directory = std::filesystem::canonical(
        candidate.NeutralWorkingDirectory,
        error);
    if(error)
    {
        return std::unexpected{
            gh_cli_configuration_failure::working_directory_not_found};
    }
    if(repository_ancestor(working_directory))
    {
        return std::unexpected{
            gh_cli_configuration_failure::working_directory_is_repository};
    }

    constexpr std::array hardening{
        std::string_view{"GH_PROMPT_DISABLED=1"},
        std::string_view{"GH_NO_UPDATE_NOTIFIER=1"},
        std::string_view{"GH_NO_EXTENSION_UPDATE_NOTIFIER=1"},
        std::string_view{"GH_SPINNER_DISABLED=1"},
        std::string_view{"GH_TELEMETRY=0"},
        std::string_view{"NO_COLOR=1"},
        std::string_view{"CLICOLOR=0"},
    };
    std::vector<github_cli_process_environment_entry> environment;
    environment.reserve(16U);
    for(const auto setting : hardening)
    {
        append_fixed_environment(environment, setting);
    }
    if(const auto platform = append_platform_environment(
           candidate.Platform,
           environment);
       platform.has_value())
    {
        return std::unexpected{*platform};
    }

    if(candidate.Https.Proxy.has_value())
    {
        switch(validate_proxy(*candidate.Https.Proxy))
        {
        case proxy_validation::valid:
            append_environment(
                environment,
                "HTTPS_PROXY",
                *candidate.Https.Proxy);
            break;
        case proxy_validation::credential_bearing:
            return std::unexpected{
                gh_cli_configuration_failure::
                    credential_bearing_https_proxy};
        case proxy_validation::invalid:
            return std::unexpected{
                gh_cli_configuration_failure::invalid_https_proxy};
        }
    }
    if(candidate.Https.NoProxy.has_value())
    {
        if(!valid_no_proxy(*candidate.Https.NoProxy))
        {
            return std::unexpected{
                gh_cli_configuration_failure::invalid_no_proxy};
        }
        append_environment(
            environment,
            "NO_PROXY",
            *candidate.Https.NoProxy);
    }

#if !defined(__linux__)
    if(
        candidate.Https.CertificateFile.has_value()
        || candidate.Https.CertificateDirectory.has_value())
    {
        return std::unexpected{
            gh_cli_configuration_failure::irrelevant_platform_facts};
    }
#else
    if(candidate.Https.CertificateFile.has_value())
    {
        std::filesystem::path certificate;
        if(!canonical_file(*candidate.Https.CertificateFile, certificate))
        {
            return std::unexpected{
                gh_cli_configuration_failure::invalid_certificate_file};
        }
        append_environment(
            environment,
            "SSL_CERT_FILE",
            path_bytes(certificate));
    }
    if(candidate.Https.CertificateDirectory.has_value())
    {
        std::filesystem::path certificate_directory;
        if(!canonical_directory(
               *candidate.Https.CertificateDirectory,
               certificate_directory))
        {
            return std::unexpected{
                gh_cli_configuration_failure::invalid_certificate_directory};
        }
        append_environment(
            environment,
            "SSL_CERT_DIR",
            path_bytes(certificate_directory));
    }
#endif

    if(!bounded_environment(environment))
    {
        return std::unexpected{
            gh_cli_configuration_failure::environment_limit};
    }

    const validated_gh_cli_configuration configuration{
        std::move(executable),
        std::move(working_directory),
        std::move(environment),
    };
    return gh_cli_configuration_result{configuration};
}

auto compile_gh_cli_process_request(
    const validated_gh_cli_configuration& configuration,
    const validated_github_host_profile& profile,
    const github_wire_request& request) -> gh_cli_invocation_result
{
    if(
        profile.authentication_transport()
        != application::github_authentication_transport::github_cli)
    {
        return std::unexpected{
            gh_cli_invocation_failure::profile_transport_mismatch};
    }
    if(const auto correlation = validate_correlation(request, profile);
       correlation.has_value())
    {
        return std::unexpected{*correlation};
    }
    if(
        const auto validation =
            validate_outbound_read_request(request.Wire);
        !validation.has_value())
    {
        return std::unexpected{
            gh_cli_invocation_failure::invalid_wire_request};
    }
    const auto endpoint = translated_endpoint(profile, request.Wire);
    if(!endpoint.has_value())
    {
        return std::unexpected{gh_cli_invocation_failure::invalid_route};
    }

    std::vector<std::string> arguments{
        "api",
        *endpoint,
        "--hostname",
        std::string{profile.hostname()},
        "--method",
        request.Wire.Method == http_method::get ? "GET" : "POST",
    };
    for(const auto& header : request.Wire.Headers)
    {
        arguments.emplace_back("--header");
        arguments.emplace_back(header.Name + ": " + header.Value);
    }
    if(request.Wire.Method == http_method::post)
    {
        arguments.emplace_back("--input");
        arguments.emplace_back("-");
    }
    if(
        request.Wire.Body.size() > maximum_standard_input_bytes
        || arguments.size() > 16U)
    {
        return std::unexpected{gh_cli_invocation_failure::request_limit};
    }

    return github_cli_process_request{
        .Executable = path_bytes(configuration.Executable),
        .Arguments = std::move(arguments),
        .Environment = configuration.Environment,
        .WorkingDirectory =
            path_bytes(configuration.NeutralWorkingDirectory),
        .StandardInput = request.Wire.Body,
        .StandardInputLimit = maximum_standard_input_bytes,
        .StandardOutputLimit = maximum_standard_output_bytes,
        .StandardErrorLimit = maximum_standard_error_bytes,
        .Timeout = process_timeout,
        .ReapTimeout = process_reap_timeout,
    };
}

gh_cli_transport::gh_cli_transport(
    const validated_github_host_profile& profile,
    const validated_gh_cli_configuration& configuration,
    github_cli_process_port& process)
    : Profile{profile}
    , Configuration{configuration}
    , Process{process}
{
}

auto gh_cli_transport::availability_reasons() const noexcept
    -> std::array<gh_cli_availability_reason, 4>
{
    return {
        gh_cli_availability_reason::
            executable_identity_unverifiable,
        gh_cli_availability_reason::
            cross_authority_redirect_unverifiable,
        gh_cli_availability_reason::
            ambient_configuration_unverifiable,
        gh_cli_availability_reason::
            credential_identity_binding_unverifiable,
    };
}

auto gh_cli_transport::perform(
    const github_wire_request& request,
    const std::stop_token stop) -> github_wire_completion
{
    if(stop.stop_requested())
    {
        return github_wire_cancelled{
            .Correlation = request.Correlation,
        };
    }
    const auto invocation =
        compile_gh_cli_process_request(Configuration, Profile, request);
    if(!invocation.has_value())
    {
        return wire_error(
            request,
            application::github_provider_failure::unavailable);
    }
    if(stop.stop_requested())
    {
        return github_wire_cancelled{
            .Correlation = request.Correlation,
        };
    }

    static_cast<void>(Process);
    return wire_error(
        request,
        application::github_provider_failure::cli_unavailable);
}
}
