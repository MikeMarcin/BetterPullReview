#include "native_https_transport.hpp"

#include "github_paged_read.hpp"
#include "github_wire_request_validation.hpp"

#include <QAuthenticator>
#include <QByteArray>
#include <QCoreApplication>
#include <QEvent>
#include <QEventLoop>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslError>
#include <QSslKey>
#include <QSslPreSharedKeyAuthenticator>
#include <QSslSocket>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVariant>

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::adapters::github::detail
{
namespace
{
constexpr std::size_t maximum_url_bytes = 4U * 1024U;
constexpr std::size_t maximum_response_header_count = 64U;
constexpr std::size_t maximum_response_header_field_bytes = 8U * 1024U;
constexpr std::size_t response_read_buffer_bytes = 64U * 1024U;
constexpr std::size_t response_read_chunk_bytes = 16U * 1024U;
constexpr std::size_t maximum_system_proxy_candidates = 16U;
constexpr std::uint32_t maximum_retry_after_seconds = 86'400U;
constexpr auto cancellation_poll_interval = std::chrono::milliseconds{5};
#if defined(BPR_NATIVE_HTTPS_TESTING)
constexpr auto minimum_test_timeout = std::chrono::milliseconds{25};
#endif

enum class proxy_policy : std::uint8_t
{
    system,
#if defined(BPR_NATIVE_HTTPS_TESTING)
    fixed_test,
#endif
};

struct transport_policy
{
    proxy_policy Proxy = proxy_policy::system;
#if defined(BPR_NATIVE_HTTPS_TESTING)
    std::string ProxyHost;
    std::uint16_t ProxyPort = 0U;
    std::uint16_t FallbackProxyPort = 0U;
    std::string TrustedCaCertificatePem;
#endif
    std::chrono::milliseconds TotalTimeout =
        native_https_total_timeout;
    std::size_t ResponseBodyLimit =
        native_https_maximum_response_body_bytes;
};

struct bootstrap_credential
{
    core::github_session_request_context Context;
    application::github_credential_material Material;
    bool Used = false;
};

struct account_credential
{
    application::github_credential_operation_context Context;
    application::github_credential_material Material;
};

using credential_binding =
    std::variant<bootstrap_credential, account_credential>;

enum class native_request_surface : std::uint8_t
{
    rest_read,
    graphql_read,
    capability_probe,
};

struct native_request
{
    native_request_surface Surface = native_request_surface::rest_read;
    application::github_rate_limit_resource RateLimitResource =
        application::github_rate_limit_resource::uninitialized;
    bool RequireRateLimitHeaders = true;
    http_method Method = http_method::get;
    std::string Path;
    std::string Query;
    std::vector<http_header> Headers;
    std::string Body;
};

struct hop_cancelled
{
};

struct hop_redirect
{
    std::string Target;
};

struct hop_proxy_failure
{
    application::github_provider_error Error;
};

struct hop_success
{
    std::string Body;
    std::vector<http_header> Headers;
    page_completeness Completeness = page_completeness::unknown;
};

using hop_result =
    std::variant<hop_success, hop_redirect, hop_proxy_failure,
                 application::github_provider_error, hop_cancelled>;

struct operation_state
{
    operation_state(native_request request, const std::stop_token stop,
                    const std::chrono::steady_clock::time_point deadline)
        : Request{std::move(request)}, Stop{stop}, Deadline{deadline}
    {
    }

    native_request Request;
    std::stop_token Stop;
    std::chrono::steady_clock::time_point Deadline;
    std::optional<hop_result> Result;
    std::atomic_bool Done = false;
};

struct hop_state
{
    hop_state(std::shared_ptr<operation_state> operation,
              const std::size_t response_body_limit)
        : Operation{std::move(operation)},
          ResponseBodyLimit{response_body_limit}
    {
    }

    std::shared_ptr<operation_state> Operation;
    QNetworkReply* Reply = nullptr;
    bool Cancellation = false;
    bool Certificate = false;
    bool EncryptedSignalObserved = false;
    bool Deadline = false;
    bool HeaderLimit = false;
    bool ResponseLimit = false;
    bool Malformed = false;
    bool AuthenticationChallenge = false;
    bool ProxyAuthenticationChallenge = false;
    bool PreSharedKeyChallenge = false;
    bool MetadataChecked = false;
    int Status = 0;
    std::optional<std::string> ContentType;
    std::optional<std::uint64_t> ContentLength;
    std::optional<std::string> Link;
    std::optional<std::string> RetryAfter;
    std::optional<std::string> RateLimitLimit;
    std::optional<std::string> RateLimitRemaining;
    std::optional<std::string> RateLimitReset;
    std::optional<std::string> RateLimitResource;
    std::string Body;
    std::size_t ResponseBodyLimit = native_https_maximum_response_body_bytes;
};

[[nodiscard]] auto provider_error(
    const application::github_provider_failure failure,
    const std::optional<std::uint32_t> retry_after = std::nullopt,
    application::github_rate_limit_state rate_limits = {},
    std::optional<application::github_rate_limit_record> rate_limit_cause =
        std::nullopt) -> application::github_provider_error
{
    return {
        .Failure = failure,
        .RetryAfterSeconds = retry_after,
        .RateLimits = std::move(rate_limits),
        .RateLimitCause = std::move(rate_limit_cause),
    };
}

[[nodiscard]] auto rate_limit_state(
    const std::optional<application::github_rate_limit_record>& record) noexcept
    -> application::github_rate_limit_state
{
    auto state = application::github_rate_limit_state{};
    if(record.has_value())
    {
        static_cast<void>(application::observe(state, *record));
    }
    return state;
}

[[nodiscard]] auto
wire_error(const github_wire_correlation& correlation,
           const application::github_provider_failure failure,
           const std::optional<std::uint32_t> retry_after = std::nullopt)
    -> github_wire_completion
{
    return github_wire_error{
        .Correlation = correlation,
        .Error = provider_error(failure, retry_after),
    };
}

[[nodiscard]] auto unavailable(const github_wire_correlation& correlation)
    -> github_wire_completion
{
    return wire_error(correlation,
                      application::github_provider_failure::unavailable);
}

[[nodiscard]] auto
capability_error(const github_capability_probe_wire_correlation& correlation,
                 const application::github_provider_error error)
    -> github_capability_probe_wire_completion
{
    return github_capability_probe_wire_error{
        .Correlation = correlation,
        .Error = error,
    };
}

[[nodiscard]] auto
capability_error(const github_capability_probe_wire_correlation& correlation,
                 const application::github_provider_failure failure,
                 const std::optional<std::uint32_t> retry_after = std::nullopt)
    -> github_capability_probe_wire_completion
{
    return capability_error(correlation, provider_error(failure, retry_after));
}

[[nodiscard]] auto native_request_from(const github_wire_request& request)
    -> native_request
{
    const auto resource =
        request.Wire.Operation == read_operation::list_review_threads
            ? application::github_rate_limit_resource::graphql
            : (request.Wire.Operation == read_operation::search_pull_requests
                   ? application::github_rate_limit_resource::search
                   : application::github_rate_limit_resource::core);
    return {
        .Surface = request.Wire.Operation == read_operation::list_review_threads
                       ? native_request_surface::graphql_read
                       : native_request_surface::rest_read,
        .RateLimitResource = resource,
        .Method = request.Wire.Method,
        .Path = request.Wire.Path,
        .Query = request.Wire.Query,
        .Headers = request.Wire.Headers,
        .Body = request.Wire.Body,
    };
}

[[nodiscard]] auto
native_request_from(const github_capability_probe_wire_request& request)
    -> native_request
{
    auto headers = std::vector<http_header>{};
    headers.reserve(request.Wire.Headers.size());
    for(const auto& header : request.Wire.Headers)
    {
        headers.push_back(http_header{
            .Name = header.Name,
            .Value = header.Value,
        });
    }
    return {
        .Surface = native_request_surface::capability_probe,
        .RateLimitResource = application::github_rate_limit_resource::graphql,
        .Method = http_method::post,
        .Path = request.Wire.Path,
        .Query = {},
        .Headers = std::move(headers),
        .Body = request.Wire.Body,
    };
}

[[nodiscard]] constexpr auto
ascii_alpha_numeric(const unsigned char value) noexcept -> bool
{
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z')
           || (value >= '0' && value <= '9');
}

[[nodiscard]] constexpr auto ascii_fold(const char value) noexcept -> char
{
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A'))
                                        : value;
}

[[nodiscard]] constexpr auto
ascii_equal_fold(const std::string_view left,
                 const std::string_view right) noexcept -> bool
{
    if(left.size() != right.size())
    {
        return false;
    }
    for(std::size_t index = 0U; index < left.size(); ++index)
    {
        if(ascii_fold(left[index]) != ascii_fold(right[index]))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] auto valid_bearer_material(
    const application::github_credential_material& material) noexcept -> bool
{
    if(!application::valid(material))
    {
        return false;
    }

    bool padding = false;
    for(const auto byte : material.bytes())
    {
        const auto value = std::to_integer<unsigned char>(byte);
        if(value == '=')
        {
            padding = true;
            continue;
        }
        if(padding
           || (!ascii_alpha_numeric(value) && value != '-' && value != '.'
               && value != '_' && value != '~' && value != '+' && value != '/'))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] auto
profile_policy_valid(const validated_github_host_profile& profile) noexcept
    -> bool
{
    return profile.authentication_transport()
               == application::github_authentication_transport::native_https
           && profile.proxy_mode() == application::github_proxy_mode::system
           && profile.trust_mode() == application::github_trust_mode::system;
}

[[nodiscard]] auto valid_request_shape(const native_request& request) noexcept
    -> bool
{
    if(!application::valid(request.RateLimitResource)
       || request.Path.size() > maximum_url_bytes
       || request.Query.size() > maximum_url_bytes
       || request.Headers.size() > 8U
       || request.Body.size() > native_https_maximum_request_body_bytes)
    {
        return false;
    }

    std::size_t header_bytes = 0U;
    for(const auto& header : request.Headers)
    {
        if(
            header.Name.empty() || header.Name.size() > 256U
            || header.Value.size()
                > maximum_response_header_field_bytes
            || header.Name.size()
                > native_https_maximum_response_header_bytes
                    - header_bytes)
        {
            return false;
        }
        header_bytes += header.Name.size();
        if(
            header.Value.size()
                > native_https_maximum_response_header_bytes
                    - header_bytes)
        {
            return false;
        }
        header_bytes += header.Value.size();
    }
    return true;
}

[[nodiscard]] auto authority_matches_profile(
    const application::github_credential_authority_key& authority,
    const validated_github_host_profile& profile) noexcept -> bool
{
    return authority.Profile == profile.profile()
        && authority.ProfileRevision == profile.revision()
        && authority.Host == profile.host()
        && authority.ServerContract == profile.server_contract();
}

#if defined(BPR_NATIVE_HTTPS_TESTING)
[[nodiscard]] auto valid_test_configuration(
    const native_https_test_configuration& configuration) noexcept -> bool
{
    const auto loopback =
        configuration.ProxyHost == "127.0.0.1"
        || configuration.ProxyHost == "::1";
    return loopback
        && configuration.ProxyPort != 0U
        && (
            configuration.FallbackProxyPort == 0U
            || configuration.FallbackProxyPort
                != configuration.ProxyPort)
        && configuration.TrustedCaCertificatePem.size()
            <= native_https_maximum_response_header_bytes
        && configuration.TotalTimeout >= minimum_test_timeout
        && configuration.TotalTimeout <= native_https_total_timeout
        && configuration.ResponseBodyLimit != 0U
        && configuration.ResponseBodyLimit
            <= native_https_maximum_response_body_bytes;
}
#endif

[[nodiscard]] auto text_is_bounded_header_value(
    const QByteArray& value) noexcept -> bool
{
    if(
        value.size()
            > static_cast<qsizetype>(
                maximum_response_header_field_bytes))
    {
        return false;
    }
    return std::ranges::none_of(value, [](const char character) {
        const auto byte = static_cast<unsigned char>(character);
        return byte == 0U || byte == '\r' || byte == '\n';
    });
}

[[nodiscard]] auto text_is_bounded_header_name(
    const QByteArray& value) noexcept -> bool
{
    if(value.isEmpty() || value.size() > 256)
    {
        return false;
    }
    return std::ranges::all_of(value, [](const char character) {
        const auto byte = static_cast<unsigned char>(character);
        return ascii_alpha_numeric(byte) || byte == '-';
    });
}

[[nodiscard]] auto header_name_equal(
    const QByteArray& actual,
    const char* const expected) noexcept -> bool
{
    return actual.compare(expected, Qt::CaseInsensitive) == 0;
}

[[nodiscard]] auto strict_decimal(
    const std::string_view text) noexcept
    -> std::optional<std::uint64_t>
{
    if(text.empty())
    {
        return std::nullopt;
    }
    std::uint64_t value = 0U;
    const auto parsed = std::from_chars(
        text.data(),
        text.data() + text.size(),
        value);
    if(
        parsed.ec != std::errc{}
        || parsed.ptr != text.data() + text.size())
    {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] constexpr auto rate_limit_resource_name(
    const application::github_rate_limit_resource resource) noexcept
    -> std::string_view
{
    switch(resource)
    {
    case application::github_rate_limit_resource::core:
        return "core";
    case application::github_rate_limit_resource::search:
        return "search";
    case application::github_rate_limit_resource::graphql:
        return "graphql";
    case application::github_rate_limit_resource::uninitialized:
        return {};
    }
    return {};
}

[[nodiscard]] auto trimmed_ascii(const QByteArray& value) -> QByteArray
{
    return value.trimmed();
}

[[nodiscard]] auto safe_qbytearray_string(
    const QByteArray& value) -> std::string
{
    return {
        value.constData(),
        static_cast<std::size_t>(value.size()),
    };
}

void wipe(QByteArray& bytes) noexcept
{
    auto* cursor =
        reinterpret_cast<volatile unsigned char*>(bytes.data());
    auto size = bytes.size();
    while(size > 0)
    {
        *cursor = 0U;
        ++cursor;
        --size;
    }
    bytes.clear();
}

[[nodiscard]] auto proxy_text_valid(
    const QString& value,
    const qsizetype maximum) noexcept -> bool
{
    if(value.size() > maximum)
    {
        return false;
    }
    return std::ranges::none_of(value, [](const QChar character) {
        return character.isNull()
            || character == u'\r' || character == u'\n';
    });
}

[[nodiscard]] auto supported_proxy(
    const QNetworkProxy& proxy) noexcept -> bool
{
    switch(proxy.type())
    {
    case QNetworkProxy::NoProxy:
        return true;
    case QNetworkProxy::HttpProxy:
    case QNetworkProxy::Socks5Proxy:
        return proxy.port() != 0U
            && proxy_text_valid(proxy.hostName(), 253)
            && !proxy.hostName().isEmpty()
            && proxy_text_valid(proxy.user(), 2'560)
            && proxy_text_valid(proxy.password(), 2'560);
    case QNetworkProxy::DefaultProxy:
    case QNetworkProxy::HttpCachingProxy:
    case QNetworkProxy::FtpCachingProxy:
        return false;
    }
    return false;
}

[[nodiscard]] auto system_proxy_candidates(const QUrl& url)
    -> std::optional<std::vector<QNetworkProxy>>
{
    const auto candidates =
        QNetworkProxyFactory::systemProxyForQuery(
            QNetworkProxyQuery{url, QNetworkProxyQuery::UrlRequest});
    if(
        candidates.isEmpty()
        || candidates.size()
            > static_cast<qsizetype>(maximum_system_proxy_candidates))
    {
        return std::nullopt;
    }

    std::vector<QNetworkProxy> supported;
    supported.reserve(static_cast<std::size_t>(candidates.size()));
    for(const auto& candidate : candidates)
    {
        if(supported_proxy(candidate))
        {
            supported.push_back(candidate);
        }
    }
    if(supported.empty())
    {
        return std::nullopt;
    }
    return supported;
}

[[nodiscard]] auto build_url(
    const validated_github_host_profile& profile,
    const native_request& request)
    -> std::optional<QUrl>
{
    if(request.Path.empty() || request.Path.front() != '/'
       || request.Path.size() > maximum_url_bytes)
    {
        return std::nullopt;
    }
    if(request.Surface != native_request_surface::rest_read
       && request.Path != profile.graphql_route())
    {
        return std::nullopt;
    }

    std::string encoded = "https://";
    encoded += profile.rest_authority();
    encoded += request.Path;
    if(!request.Query.empty())
    {
        encoded += '?';
        encoded += request.Query;
    }
    if(encoded.size() > maximum_url_bytes)
    {
        return std::nullopt;
    }

    const auto bytes =
        QByteArray{encoded.data(), static_cast<qsizetype>(encoded.size())};
    const auto url = QUrl::fromEncoded(bytes, QUrl::StrictMode);
    if(!url.isValid() || url.isRelative()
       || url.scheme() != QStringLiteral("https")
       || url.host().toUtf8() != profile.rest_authority() || url.port(-1) != -1
       || !url.userInfo().isEmpty() || !url.fragment().isEmpty()
       || url.toEncoded(QUrl::FullyEncoded) != bytes)
    {
        return std::nullopt;
    }
    return url;
}

[[nodiscard]] auto
supported_json_content_type(const std::string_view value) noexcept -> bool
{
    const auto separator = value.find(';');
    const auto media = value.substr(0U, separator);
    if(!ascii_equal_fold(media, "application/json"))
    {
        return false;
    }
    if(separator == std::string_view::npos)
    {
        return true;
    }
    auto parameter = value.substr(separator + 1U);
    while(!parameter.empty() && parameter.front() == ' ')
    {
        parameter.remove_prefix(1U);
    }
    return ascii_equal_fold(parameter, "charset=utf-8");
}

[[nodiscard]] auto
parse_link_completeness(const std::optional<std::string>& link) noexcept
    -> std::optional<page_completeness>
{
    if(!link.has_value())
    {
        return page_completeness::complete;
    }
    if(link->empty())
    {
        return std::nullopt;
    }

    const auto text = std::string_view{*link};
    const auto token_character = [](const char character) noexcept {
        const auto value = static_cast<unsigned char>(character);
        return ascii_alpha_numeric(value) || character == '!'
               || character == '#' || character == '$' || character == '%'
               || character == '&' || character == '\'' || character == '*'
               || character == '+' || character == '-' || character == '.'
               || character == '^' || character == '_' || character == '`'
               || character == '|' || character == '~';
    };
    const auto skip_whitespace = [&text](std::size_t& position) noexcept {
        while(position < text.size()
              && (text[position] == ' ' || text[position] == '\t'))
        {
            ++position;
        }
    };
    const auto valid_relation = [](const std::string_view value,
                                   bool& has_next) noexcept {
        if(value.empty())
        {
            return false;
        }
        std::size_t position = 0U;
        while(position < value.size())
        {
            const auto separator = value.find(' ', position);
            const auto relation =
                value.substr(position, separator == std::string_view::npos
                                           ? value.size() - position
                                           : separator - position);
            if(relation != "next" && relation != "prev" && relation != "first"
               && relation != "last")
            {
                return false;
            }
            has_next = has_next || relation == "next";
            if(separator == std::string_view::npos)
            {
                break;
            }
            position = separator + 1U;
            if(position == value.size() || value[position] == ' ')
            {
                return false;
            }
        }
        return true;
    };

    bool has_next = false;
    std::size_t position = 0U;
    while(position < text.size())
    {
        skip_whitespace(position);
        if(position == text.size() || text[position] != '<')
        {
            return std::nullopt;
        }
        const auto target_end = text.find('>', position + 1U);
        if(target_end == std::string_view::npos || target_end == position + 1U)
        {
            return std::nullopt;
        }
        for(auto target = position + 1U; target < target_end; ++target)
        {
            const auto value = static_cast<unsigned char>(text[target]);
            if(value <= 0x20U || value >= 0x7fU || text[target] == '<'
               || text[target] == '"')
            {
                return std::nullopt;
            }
        }
        position = target_end + 1U;

        bool relation_seen = false;
        while(true)
        {
            skip_whitespace(position);
            if(position == text.size() || text[position] == ',')
            {
                break;
            }
            if(text[position] != ';')
            {
                return std::nullopt;
            }
            ++position;
            skip_whitespace(position);
            const auto name_begin = position;
            while(position < text.size() && token_character(text[position]))
            {
                ++position;
            }
            if(name_begin == position)
            {
                return std::nullopt;
            }
            const auto name = text.substr(name_begin, position - name_begin);
            skip_whitespace(position);
            if(position == text.size() || text[position] != '=')
            {
                return std::nullopt;
            }
            ++position;
            skip_whitespace(position);
            const auto quoted = position < text.size() && text[position] == '"';
            if(quoted)
            {
                ++position;
            }
            const auto value_begin = position;
            if(quoted)
            {
                while(position < text.size() && text[position] != '"')
                {
                    const auto value =
                        static_cast<unsigned char>(text[position]);
                    if(value < 0x20U || value == 0x7fU
                       || text[position] == '\\')
                    {
                        return std::nullopt;
                    }
                    ++position;
                }
                if(position == text.size())
                {
                    return std::nullopt;
                }
            }
            else
            {
                while(position < text.size() && token_character(text[position]))
                {
                    ++position;
                }
            }
            if(value_begin == position)
            {
                return std::nullopt;
            }
            const auto value = text.substr(value_begin, position - value_begin);
            if(quoted)
            {
                ++position;
            }
            if(name == "rel")
            {
                if(relation_seen || !valid_relation(value, has_next))
                {
                    return std::nullopt;
                }
                relation_seen = true;
            }
        }
        if(!relation_seen)
        {
            return std::nullopt;
        }
        if(position == text.size())
        {
            break;
        }
        ++position;
        if(position == text.size())
        {
            return std::nullopt;
        }
    }
    return has_next ? std::optional{page_completeness::has_next_page}
                    : std::optional{page_completeness::complete};
}

[[nodiscard]] auto
parse_retry_after(const std::optional<std::string>& value) noexcept
    -> std::optional<std::uint32_t>
{
    if(!value.has_value())
    {
        return std::nullopt;
    }
    const auto parsed = strict_decimal(*value);
    if(!parsed.has_value() || *parsed > maximum_retry_after_seconds)
    {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(*parsed);
}

[[nodiscard]] auto allowlisted_success_headers(const hop_state& state)
    -> std::vector<http_header>
{
    auto headers = std::vector<http_header>{};
    headers.reserve(7U);
    const auto add = [&headers](const char* const name,
                                const std::optional<std::string>& value) {
        if(value.has_value())
        {
            headers.push_back(http_header{
                .Name = name,
                .Value = *value,
            });
        }
    };
    add("Content-Type", state.ContentType);
    add("Link", state.Link);
    add("X-RateLimit-Limit", state.RateLimitLimit);
    add("X-RateLimit-Remaining", state.RateLimitRemaining);
    add("X-RateLimit-Reset", state.RateLimitReset);
    add("X-RateLimit-Resource", state.RateLimitResource);
    add("Retry-After", state.RetryAfter);
    return headers;
}

[[nodiscard]] auto classify_http_status(const hop_state& state)
    -> application::github_provider_error
{
    const auto remaining = state.RateLimitRemaining.has_value()
                               ? strict_decimal(*state.RateLimitRemaining)
                               : std::nullopt;
    const auto limit = state.RateLimitLimit.has_value()
                           ? strict_decimal(*state.RateLimitLimit)
                           : std::nullopt;
    const auto reset = state.RateLimitReset.has_value()
                           ? strict_decimal(*state.RateLimitReset)
                           : std::nullopt;
    const auto exhausted = remaining.has_value() && *remaining == 0U;
    const auto retry_after = parse_retry_after(state.RetryAfter);
    const auto secondary_message =
        (state.Status == 403 || state.Status == 429)
        && state.ContentType.has_value()
        && supported_json_content_type(*state.ContentType)
        && github_error_body_is_secondary_rate_limit(
            state.Body, native_https_maximum_error_response_body_bytes);
    const auto structured_graphql_rate_limit =
        state.Status == 403
        && state.Operation->Request.Surface != native_request_surface::rest_read
        && state.ContentType.has_value()
        && supported_json_content_type(*state.ContentType)
        && github_graphql_error_body_is_rate_limited(
            state.Body, native_https_maximum_error_response_body_bytes);
    const auto secondary = secondary_message || structured_graphql_rate_limit;
    auto rate_limit =
        limit.has_value() && remaining.has_value() && reset.has_value()
                && state.RateLimitResource.has_value() && *remaining <= *limit
            ? std::optional<
                  application::
                      github_rate_limit_record>{application::
                                                    github_rate_limit_record{
                                                        .Resource =
                                                            state.Operation
                                                                ->Request
                                                                .RateLimitResource,
                                                        .Limit = *limit,
                                                        .Remaining = *remaining,
                                                        .ResetUnixSeconds =
                                                            *reset,
                                                        .SecondaryLimited =
                                                            false,
                                                    }}
            : std::nullopt;
    auto failure = application::github_provider_failure::malformed_response;
    switch(state.Status)
    {
    case 401:
        failure = application::github_provider_failure::authentication;
        break;
    case 403:
        failure = exhausted || retry_after.has_value() || secondary
                      ? application::github_provider_failure::rate_limit
                      : application::github_provider_failure::permission;
        break;
    case 404:
        failure = application::github_provider_failure::permission;
        break;
    case 406:
    case 415:
        failure = application::github_provider_failure::api_version;
        break;
    case 408:
    case 504:
        failure = application::github_provider_failure::timeout;
        break;
    case 429:
        failure = application::github_provider_failure::rate_limit;
        break;
    default:
        if(state.Status >= 500 && state.Status <= 599)
        {
            failure = application::github_provider_failure::unavailable;
        }
        break;
    }
    if(rate_limit.has_value()
       && failure == application::github_provider_failure::rate_limit
       && rate_limit->Remaining != 0U)
    {
        rate_limit->SecondaryLimited =
            state.Status == 429 || secondary
            || (state.Status == 403 && retry_after.has_value());
    }
    auto rate_limits = rate_limit_state(rate_limit);
    auto cause = failure == application::github_provider_failure::rate_limit
                     ? rate_limit
                     : std::optional<application::github_rate_limit_record>{};
    if(failure == application::github_provider_failure::rate_limit
       && !cause.has_value())
    {
        return provider_error(
            application::github_provider_failure::malformed_response,
            std::nullopt, std::move(rate_limits));
    }
    if(cause.has_value())
    {
        static_cast<void>(application::observe(rate_limits, *cause));
    }
    return provider_error(
        failure,
        failure == application::github_provider_failure::rate_limit
            ? (retry_after.has_value()
                   ? retry_after
                   : (cause.has_value() && cause->SecondaryLimited && !exhausted
                          ? std::optional<std::uint32_t>{60U}
                          : std::nullopt))
            : (failure == application::github_provider_failure::unavailable
                   ? retry_after
                   : std::nullopt),
        std::move(rate_limits), std::move(cause));
}

[[nodiscard]] auto recognized_redirect_status(const int status) noexcept
    -> bool
{
    return status == 301 || status == 302 || status == 303
        || status == 307 || status == 308;
}

[[nodiscard]] auto redirect_target(
    QNetworkReply& reply,
    const QUrl& source) -> std::optional<std::string>
{
    const auto attribute =
        reply.attribute(QNetworkRequest::RedirectionTargetAttribute);
    if(!attribute.isValid() || !attribute.canConvert<QUrl>())
    {
        return std::nullopt;
    }
    const auto relative = attribute.toUrl();
    if(!relative.isValid() || !relative.fragment().isEmpty())
    {
        return std::nullopt;
    }
    const auto resolved = source.resolved(relative);
    const auto encoded = resolved.toEncoded(QUrl::FullyEncoded);
    if(
        !resolved.isValid() || resolved.isRelative()
        || encoded.isEmpty() || encoded.size() > 2 * 1024)
    {
        return std::nullopt;
    }
    return safe_qbytearray_string(encoded);
}

void abort_hop(hop_state& state) noexcept
{
    if(state.Reply != nullptr && state.Reply->isRunning())
    {
        state.Reply->abort();
    }
}

class active_hop_guard final
{
public:
    active_hop_guard(
        hop_state& state,
        hop_state*& active) noexcept
        : State{state}
        , Active{active}
    {
        Active = &State;
    }

    active_hop_guard(const active_hop_guard&) = delete;
    auto operator=(const active_hop_guard&)
        -> active_hop_guard& = delete;

    ~active_hop_guard()
    {
        reset();
    }

    void reset() noexcept
    {
        if(Active == &State)
        {
            Active = nullptr;
        }
        auto* const reply = std::exchange(State.Reply, nullptr);
        if(reply == nullptr)
        {
            return;
        }
        if(reply->isRunning())
        {
            reply->abort();
        }
        reply->deleteLater();
        QCoreApplication::sendPostedEvents(
            reply,
            QEvent::DeferredDelete);
    }

private:
    hop_state& State;
    hop_state*& Active;
};

void inspect_metadata_impl(hop_state& state)
{
    if(state.MetadataChecked || state.Reply == nullptr)
    {
        return;
    }
    const auto status =
        state.Reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    if(!status.isValid())
    {
        if(
            state.Reply->isFinished()
            && state.Reply->error() == QNetworkReply::NoError)
        {
            state.MetadataChecked = true;
            state.Malformed = true;
        }
        return;
    }
    state.MetadataChecked = true;
    state.Status = status.toInt();
    if(state.Status < 100 || state.Status > 599)
    {
        state.Malformed = true;
        abort_hop(state);
        return;
    }

    const auto& headers = state.Reply->rawHeaderPairs();
    if(
        static_cast<std::size_t>(headers.size())
        > maximum_response_header_count)
    {
        state.HeaderLimit = true;
        abort_hop(state);
        return;
    }

    std::size_t aggregate = 0U;
    std::size_t content_type_count = 0U;
    std::size_t content_length_count = 0U;
    std::size_t content_encoding_count = 0U;
    std::size_t link_count = 0U;
    std::size_t location_count = 0U;
    std::size_t retry_after_count = 0U;
    std::size_t rate_limit_count = 0U;
    std::size_t rate_remaining_count = 0U;
    std::size_t rate_reset_count = 0U;
    std::size_t rate_resource_count = 0U;
    for(const auto& [name, value] : headers)
    {
        const auto field_size = static_cast<std::size_t>(name.size())
                                + static_cast<std::size_t>(value.size());
        if(!text_is_bounded_header_name(name)
           || !text_is_bounded_header_value(value)
           || field_size
                  > native_https_maximum_response_header_bytes - aggregate)
        {
            state.HeaderLimit = true;
            abort_hop(state);
            return;
        }
        aggregate += field_size;

        if(header_name_equal(name, "Content-Type"))
        {
            ++content_type_count;
            state.ContentType = safe_qbytearray_string(trimmed_ascii(value));
        }
        else if(header_name_equal(name, "Content-Length"))
        {
            ++content_length_count;
            const auto text = safe_qbytearray_string(trimmed_ascii(value));
            const auto parsed = strict_decimal(text);
            if(!parsed.has_value())
            {
                state.Malformed = true;
            }
            else if(
                *parsed
                > (state.Status == 200
                       ? state.ResponseBodyLimit
                       : std::min(
                             state.ResponseBodyLimit,
                             native_https_maximum_error_response_body_bytes)))
            {
                state.ResponseLimit = true;
            }
            else
            {
                state.ContentLength = *parsed;
            }
        }
        else if(header_name_equal(name, "Content-Encoding"))
        {
            ++content_encoding_count;
        }
        else if(header_name_equal(name, "Link"))
        {
            ++link_count;
            state.Link = safe_qbytearray_string(trimmed_ascii(value));
        }
        else if(header_name_equal(name, "Location"))
        {
            ++location_count;
        }
        else if(header_name_equal(name, "Retry-After"))
        {
            ++retry_after_count;
            state.RetryAfter = safe_qbytearray_string(trimmed_ascii(value));
        }
        else if(header_name_equal(name, "X-RateLimit-Limit"))
        {
            ++rate_limit_count;
            state.RateLimitLimit = safe_qbytearray_string(trimmed_ascii(value));
        }
        else if(header_name_equal(name, "X-RateLimit-Remaining"))
        {
            ++rate_remaining_count;
            state.RateLimitRemaining =
                safe_qbytearray_string(trimmed_ascii(value));
        }
        else if(header_name_equal(name, "X-RateLimit-Reset"))
        {
            ++rate_reset_count;
            state.RateLimitReset = safe_qbytearray_string(trimmed_ascii(value));
        }
        else if(header_name_equal(name, "X-RateLimit-Resource"))
        {
            ++rate_resource_count;
            state.RateLimitResource =
                safe_qbytearray_string(trimmed_ascii(value));
        }
    }

    if(content_type_count > 1U || content_length_count > 1U
       || content_encoding_count != 0U || link_count > 1U || location_count > 1U
       || retry_after_count > 1U || rate_limit_count > 1U
       || rate_remaining_count > 1U || rate_reset_count > 1U
       || rate_resource_count > 1U
       || (link_count != 0U
           && state.Operation->Request.Surface
                  != native_request_surface::rest_read)
       || (state.RetryAfter.has_value()
           && !parse_retry_after(state.RetryAfter).has_value())
       || (state.RateLimitLimit.has_value()
           && !strict_decimal(*state.RateLimitLimit).has_value())
       || (state.RateLimitRemaining.has_value()
           && !strict_decimal(*state.RateLimitRemaining).has_value())
       || (state.RateLimitReset.has_value()
           && !strict_decimal(*state.RateLimitReset).has_value()))
    {
        state.Malformed = true;
    }
    const auto rate_tuple_count =
        static_cast<std::size_t>(state.RateLimitLimit.has_value())
        + static_cast<std::size_t>(state.RateLimitRemaining.has_value())
        + static_cast<std::size_t>(state.RateLimitReset.has_value())
        + static_cast<std::size_t>(state.RateLimitResource.has_value());
    const auto redirect_status = state.Status == 301 || state.Status == 302
                                 || state.Status == 303 || state.Status == 307
                                 || state.Status == 308;
    if((rate_tuple_count != 0U && rate_tuple_count != 4U)
       || (rate_tuple_count == 0U
           && state.Operation->Request.RequireRateLimitHeaders
           && !redirect_status)
       || (rate_tuple_count == 4U
           && *state.RateLimitResource
                  != rate_limit_resource_name(
                      state.Operation->Request.RateLimitResource)))
    {
        state.Malformed = true;
    }
    const auto rate_limit = state.RateLimitLimit.has_value()
                                ? strict_decimal(*state.RateLimitLimit)
                                : std::optional<std::uint64_t>{};
    const auto rate_remaining = state.RateLimitRemaining.has_value()
                                    ? strict_decimal(*state.RateLimitRemaining)
                                    : std::optional<std::uint64_t>{};
    if(rate_limit.has_value() && rate_remaining.has_value()
       && *rate_remaining > *rate_limit)
    {
        state.Malformed = true;
    }
    if(state.Malformed || state.ResponseLimit || state.HeaderLimit)
    {
        abort_hop(state);
        return;
    }
}

void inspect_metadata(hop_state& state) noexcept
{
    try
    {
        inspect_metadata_impl(state);
    }
    catch(...)
    {
        state.ResponseLimit = true;
        abort_hop(state);
    }
}

void read_available(hop_state& state) noexcept
{
    try
    {
        if(state.Reply == nullptr || state.ResponseLimit || state.Malformed
           || state.HeaderLimit)
        {
            return;
        }
        inspect_metadata(state);
        if(state.Reply == nullptr)
        {
            return;
        }
        const auto limit =
            state.Status == 200
                ? state.ResponseBodyLimit
                : std::min(state.ResponseBodyLimit,
                           native_https_maximum_error_response_body_bytes);

        std::array<char, response_read_chunk_bytes> buffer{};
        while(state.Reply->bytesAvailable() > 0)
        {
            const auto ceiling = limit + 1U;
            if(state.Body.size() >= ceiling)
            {
                state.ResponseLimit = true;
                abort_hop(state);
                return;
            }
            const auto remaining = ceiling - state.Body.size();
            const auto requested = static_cast<qint64>(
                std::min(remaining, buffer.size()));
            const auto count =
                state.Reply->read(buffer.data(), requested);
            if(count <= 0)
            {
                return;
            }
            state.Body.append(
                buffer.data(),
                static_cast<std::size_t>(count));
            if(state.Body.size() > limit)
            {
                state.ResponseLimit = true;
                abort_hop(state);
                return;
            }
        }
    }
    catch(...)
    {
        state.ResponseLimit = true;
        abort_hop(state);
    }
}

[[nodiscard]] auto cancellation_result(
    const std::shared_ptr<operation_state>& operation) noexcept -> bool
{
    return operation->Stop.stop_requested();
}

[[nodiscard]] auto remaining_before_deadline(
    const std::chrono::steady_clock::time_point deadline) noexcept
    -> std::optional<std::chrono::milliseconds>
{
    const auto now = std::chrono::steady_clock::now();
    if(now >= deadline)
    {
        return std::nullopt;
    }
    return std::chrono::ceil<std::chrono::milliseconds>(deadline - now);
}

[[nodiscard]] auto tls_terminal_valid(
    QNetworkReply& reply,
    const QUrl& requested,
    const bool encrypted_signal_observed) -> bool
{
    if(
        !encrypted_signal_observed
        || !reply.attribute(QNetworkRequest::ConnectionEncryptedAttribute)
             .toBool()
        || reply.attribute(QNetworkRequest::Http2WasUsedAttribute).toBool()
        || reply.url() != requested)
    {
        return false;
    }
    const auto configuration = reply.sslConfiguration();
    return !configuration.peerCertificate().isNull()
        && configuration.peerVerifyMode() == QSslSocket::VerifyPeer
        && configuration.protocol() == QSsl::TlsV1_2OrLater;
}

[[nodiscard]] auto network_failure(
    const QNetworkReply::NetworkError error) noexcept
    -> application::github_provider_failure
{
    switch(error)
    {
    case QNetworkReply::TimeoutError:
    case QNetworkReply::ProxyTimeoutError:
        return application::github_provider_failure::timeout;
    case QNetworkReply::AuthenticationRequiredError:
        return application::github_provider_failure::authentication;
    case QNetworkReply::ContentAccessDenied:
    case QNetworkReply::ContentOperationNotPermittedError:
        return application::github_provider_failure::permission;
    case QNetworkReply::SslHandshakeFailedError:
        return application::github_provider_failure::certificate;
    case QNetworkReply::NoError:
    case QNetworkReply::ConnectionRefusedError:
    case QNetworkReply::RemoteHostClosedError:
    case QNetworkReply::HostNotFoundError:
    case QNetworkReply::OperationCanceledError:
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::NetworkSessionFailedError:
    case QNetworkReply::BackgroundRequestNotAllowedError:
    case QNetworkReply::TooManyRedirectsError:
    case QNetworkReply::InsecureRedirectError:
    case QNetworkReply::UnknownNetworkError:
    case QNetworkReply::ProxyConnectionRefusedError:
    case QNetworkReply::ProxyConnectionClosedError:
    case QNetworkReply::ProxyNotFoundError:
    case QNetworkReply::ProxyAuthenticationRequiredError:
    case QNetworkReply::UnknownProxyError:
    case QNetworkReply::ContentNotFoundError:
    case QNetworkReply::ContentReSendError:
    case QNetworkReply::ContentConflictError:
    case QNetworkReply::ContentGoneError:
    case QNetworkReply::UnknownContentError:
    case QNetworkReply::ProtocolUnknownError:
    case QNetworkReply::ProtocolInvalidOperationError:
    case QNetworkReply::ProtocolFailure:
    case QNetworkReply::InternalServerError:
    case QNetworkReply::OperationNotImplementedError:
    case QNetworkReply::ServiceUnavailableError:
    case QNetworkReply::UnknownServerError:
        return application::github_provider_failure::network;
    }
    return application::github_provider_failure::network;
}

[[nodiscard]] auto proxy_candidate_retryable(
    const QNetworkReply::NetworkError error) noexcept -> bool
{
    switch(error)
    {
    case QNetworkReply::ConnectionRefusedError:
    case QNetworkReply::RemoteHostClosedError:
    case QNetworkReply::HostNotFoundError:
    case QNetworkReply::TimeoutError:
    case QNetworkReply::OperationCanceledError:
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::NetworkSessionFailedError:
    case QNetworkReply::BackgroundRequestNotAllowedError:
    case QNetworkReply::UnknownNetworkError:
    case QNetworkReply::ProxyConnectionRefusedError:
    case QNetworkReply::ProxyConnectionClosedError:
    case QNetworkReply::ProxyNotFoundError:
    case QNetworkReply::ProxyTimeoutError:
    case QNetworkReply::ProxyAuthenticationRequiredError:
    case QNetworkReply::UnknownProxyError:
        return true;
    case QNetworkReply::NoError:
    case QNetworkReply::SslHandshakeFailedError:
    case QNetworkReply::TooManyRedirectsError:
    case QNetworkReply::InsecureRedirectError:
    case QNetworkReply::AuthenticationRequiredError:
    case QNetworkReply::ContentAccessDenied:
    case QNetworkReply::ContentOperationNotPermittedError:
    case QNetworkReply::ContentNotFoundError:
    case QNetworkReply::ContentReSendError:
    case QNetworkReply::ContentConflictError:
    case QNetworkReply::ContentGoneError:
    case QNetworkReply::UnknownContentError:
    case QNetworkReply::ProtocolUnknownError:
    case QNetworkReply::ProtocolInvalidOperationError:
    case QNetworkReply::ProtocolFailure:
    case QNetworkReply::InternalServerError:
    case QNetworkReply::OperationNotImplementedError:
    case QNetworkReply::ServiceUnavailableError:
    case QNetworkReply::UnknownServerError:
        return false;
    }
    return false;
}

[[nodiscard]] auto read_completion(const github_wire_correlation& correlation,
                                   hop_result result) -> github_wire_completion
{
    if(auto* success = std::get_if<hop_success>(&result); success != nullptr)
    {
        return github_wire_success{
            .Correlation = correlation,
            .Response =
                wire_response{
                    .Status = 200,
                    .Headers = std::move(success->Headers),
                    .Body = std::move(success->Body),
                    .Completeness = success->Completeness,
                },
        };
    }
    if(auto* error = std::get_if<application::github_provider_error>(&result);
       error != nullptr)
    {
        return github_wire_error{
            .Correlation = correlation,
            .Error = *error,
        };
    }
    if(std::holds_alternative<hop_cancelled>(result))
    {
        return github_wire_cancelled{.Correlation = correlation};
    }
    return wire_error(correlation,
                      application::github_provider_failure::redirect);
}

[[nodiscard]] auto capability_completion(
    const github_capability_probe_wire_correlation& correlation,
    hop_result result) -> github_capability_probe_wire_completion
{
    if(auto* success = std::get_if<hop_success>(&result); success != nullptr)
    {
        auto response = capability_probe_response{
            .Status = 200,
            .Authenticated = true,
            .ContentType = {},
            .Body = std::move(success->Body),
            .RateLimitHeaders = {},
            .RateLimitHeaderCount = 0U,
        };
        for(auto& header : success->Headers)
        {
            if(header.Name == "Content-Type")
            {
                response.ContentType = std::move(header.Value);
                continue;
            }
            if(response.RateLimitHeaderCount
               >= response.RateLimitHeaders.size())
            {
                return capability_error(
                    correlation,
                    application::github_provider_failure::malformed_response);
            }
            response.RateLimitHeaders[response.RateLimitHeaderCount] =
                capability_probe_header{
                    .Name = std::move(header.Name),
                    .Value = std::move(header.Value),
                };
            ++response.RateLimitHeaderCount;
        }
        if(response.ContentType.empty())
        {
            return capability_error(
                correlation,
                application::github_provider_failure::malformed_response);
        }
        return github_capability_probe_wire_success{
            .Correlation = correlation,
            .Response = std::move(response),
        };
    }
    if(auto* error = std::get_if<application::github_provider_error>(&result);
       error != nullptr)
    {
        return capability_error(correlation, *error);
    }
    if(std::holds_alternative<hop_cancelled>(result))
    {
        return github_capability_probe_wire_cancelled{
            .Correlation = correlation,
        };
    }
    return capability_error(correlation,
                            application::github_provider_failure::redirect);
}
} // namespace

class native_https_transport::implementation final
{
public:
    implementation(
        const validated_github_host_profile& profile,
        bootstrap_credential credential,
        transport_policy policy)
        : Profile{profile}
        , Credential{std::move(credential)}
        , Policy{std::move(policy)}
    {
    }

    implementation(
        const validated_github_host_profile& profile,
        account_credential credential,
        transport_policy policy)
        : Profile{profile}
        , Credential{std::move(credential)}
        , Policy{std::move(policy)}
    {
    }

    implementation(const implementation&) = delete;
    auto operator=(const implementation&) -> implementation& = delete;

    ~implementation()
    {
        Closing.store(true, std::memory_order_release);
        if(Dispatcher != nullptr && Thread.isRunning())
        {
            static_cast<void>(QMetaObject::invokeMethod(
                Dispatcher,
                [this] {
                    if(ActiveHop != nullptr)
                    {
                        ActiveHop->Cancellation = true;
                        abort_hop(*ActiveHop);
                    }
                    if(Manager != nullptr)
                    {
                        Manager->clearAccessCache();
                        Manager->clearConnectionCache();
                    }
                },
                Qt::BlockingQueuedConnection));
            Thread.quit();
            Thread.wait();
        }
        Dispatcher = nullptr;
        Manager = nullptr;
    }

    [[nodiscard]] auto start() noexcept -> bool
    {
        try
        {
            Dispatcher = new(std::nothrow) QObject;
            if(Dispatcher == nullptr)
            {
                return false;
            }
            Dispatcher->moveToThread(&Thread);
            QObject::connect(
                &Thread,
                &QThread::finished,
                Dispatcher,
                &QObject::deleteLater);
            Thread.start();

            auto initialized = false;
            const auto invoked = QMetaObject::invokeMethod(
                Dispatcher,
                [this, &initialized] {
                    try
                    {
                        Manager = new(std::nothrow)
                            QNetworkAccessManager{Dispatcher};
                        if(Manager == nullptr)
                        {
                            return;
                        }
                        Manager->setAutoDeleteReplies(false);
                        Manager->setRedirectPolicy(
                            QNetworkRequest::ManualRedirectPolicy);
                        Manager->setStrictTransportSecurityEnabled(false);
                        connect_manager_signals();
                        initialized = true;
                    }
                    catch(...)
                    {
                        delete Manager;
                        Manager = nullptr;
                    }
                },
                Qt::BlockingQueuedConnection);
            if(!invoked || !initialized)
            {
                Thread.quit();
                Thread.wait();
                Dispatcher = nullptr;
                Manager = nullptr;
                return false;
            }
            return true;
        }
        catch(...)
        {
            if(Thread.isRunning())
            {
                Thread.quit();
                Thread.wait();
            }
            Dispatcher = nullptr;
            Manager = nullptr;
            return false;
        }
    }

    [[nodiscard]] auto perform(
        const github_wire_request& request,
        const std::stop_token stop) -> github_wire_completion
    {
        if(stop.stop_requested())
        {
            return github_wire_cancelled{
                .Correlation = request.Correlation,
            };
        }
        native_request native;
        try
        {
            if(!validate_github_wire_request(Profile, request).has_value())
            {
                return unavailable(request.Correlation);
            }
            native = native_request_from(request);
            native.RequireRateLimitHeaders =
                Profile.server_contract()
                == application::github_server_contract::github_com_2022_11_28;
            if(!valid_request_shape(native))
            {
                return unavailable(request.Correlation);
            }
        }
        catch(...)
        {
            return wire_error(
                request.Correlation,
                application::github_provider_failure::response_limit);
        }
        return read_completion(
            request.Correlation,
            perform_validated(request, std::move(native), stop));
    }

    [[nodiscard]] auto perform_capability_probe(
        const github_capability_probe_wire_request& request,
        const std::stop_token stop) -> github_capability_probe_wire_completion
    {
        if(stop.stop_requested())
        {
            return github_capability_probe_wire_cancelled{
                .Correlation = request.Correlation,
            };
        }
        native_request native;
        try
        {
            if(!validate_github_capability_probe_wire_request(Profile, request)
                    .has_value())
            {
                return capability_error(
                    request.Correlation,
                    application::github_provider_failure::unavailable);
            }
            native = native_request_from(request);
            native.RequireRateLimitHeaders =
                Profile.server_contract()
                == application::github_server_contract::github_com_2022_11_28;
            if(!valid_request_shape(native))
            {
                return capability_error(
                    request.Correlation,
                    application::github_provider_failure::unavailable);
            }
        }
        catch(...)
        {
            return capability_error(
                request.Correlation,
                application::github_provider_failure::response_limit);
        }
        return capability_completion(
            request.Correlation,
            perform_validated(request, std::move(native), stop));
    }

private:
    template <typename Request>
    [[nodiscard]] auto perform_validated(const Request& request,
                                         native_request native,
                                         const std::stop_token stop)
        -> hop_result
    {
        if(Closing.load(std::memory_order_acquire) || Dispatcher == nullptr
           || Manager == nullptr)
        {
            return provider_error(
                application::github_provider_failure::unavailable);
        }
        if(Busy.test_and_set(std::memory_order_acq_rel))
        {
            return provider_error(
                application::github_provider_failure::unavailable);
        }
        struct busy_guard final
        {
            std::atomic_flag& Busy;
            ~busy_guard() { Busy.clear(std::memory_order_release); }
        } guard{Busy};

        if(stop.stop_requested() || Closing.load(std::memory_order_acquire))
        {
            return hop_cancelled{};
        }
        if(!claim_credential(request))
        {
            return provider_error(
                application::github_provider_failure::unavailable);
        }

        std::shared_ptr<operation_state> state;
        try
        {
            state = std::make_shared<operation_state>(
                std::move(native), stop,
                std::chrono::steady_clock::now() + Policy.TotalTimeout);
        }
        catch(...)
        {
            discard_bootstrap();
            return provider_error(
                application::github_provider_failure::response_limit);
        }

        std::stop_callback callback{
            stop, [this, weak = std::weak_ptr<operation_state>{state}] {
                const auto operation = weak.lock();
                if(operation == nullptr)
                {
                    return;
                }
                if(Dispatcher != nullptr)
                {
                    static_cast<void>(QMetaObject::invokeMethod(
                        Dispatcher,
                        [this, weak] {
                            const auto current = weak.lock();
                            if(current != nullptr && ActiveHop != nullptr
                               && ActiveHop->Operation == current)
                            {
                                ActiveHop->Cancellation = true;
                                abort_hop(*ActiveHop);
                            }
                        },
                        Qt::QueuedConnection));
                }
            }};

        auto invoked = false;
        try
        {
            invoked = QMetaObject::invokeMethod(
                Dispatcher,
                [this, state] noexcept {
                    try
                    {
                        state->Result.emplace(execute(state));
                    }
                    catch(...)
                    {
                        try
                        {
                            state->Result.emplace(provider_error(
                                application::github_provider_failure::
                                    response_limit));
                        }
                        catch(...)
                        {
                        }
                    }
                    discard_bootstrap();
                    state->Done.store(true, std::memory_order_release);
                    state->Done.notify_one();
                },
                Qt::QueuedConnection);
        }
        catch(...)
        {
            discard_bootstrap();
            return provider_error(
                application::github_provider_failure::response_limit);
        }
        if(!invoked)
        {
            discard_bootstrap();
            return provider_error(
                application::github_provider_failure::unavailable);
        }

        state->Done.wait(false, std::memory_order_acquire);
        if(!state->Result.has_value())
        {
            return provider_error(
                application::github_provider_failure::unavailable);
        }
        return std::move(*state->Result);
    }

    void connect_manager_signals()
    {
        QObject::connect(
            Manager,
            &QNetworkAccessManager::authenticationRequired,
            Manager,
            [this](QNetworkReply* const reply, QAuthenticator* const auth) {
                static_cast<void>(auth);
                if(ActiveHop != nullptr && ActiveHop->Reply == reply)
                {
                    ActiveHop->AuthenticationChallenge = true;
                    abort_hop(*ActiveHop);
                }
            });
        QObject::connect(
            Manager,
            &QNetworkAccessManager::proxyAuthenticationRequired,
            Manager,
            [this](
                const QNetworkProxy& proxy,
                QAuthenticator* const auth) {
                static_cast<void>(proxy);
                static_cast<void>(auth);
                if(ActiveHop != nullptr)
                {
                    ActiveHop->ProxyAuthenticationChallenge = true;
                    abort_hop(*ActiveHop);
                }
            });
        QObject::connect(
            Manager,
            &QNetworkAccessManager::sslErrors,
            Manager,
            [this](
                QNetworkReply* const reply,
                const QList<QSslError>& errors) {
                static_cast<void>(errors);
                if(ActiveHop != nullptr && ActiveHop->Reply == reply)
                {
                    ActiveHop->Certificate = true;
                    abort_hop(*ActiveHop);
                }
            });
        QObject::connect(
            Manager,
            &QNetworkAccessManager::preSharedKeyAuthenticationRequired,
            Manager,
            [this](
                QNetworkReply* const reply,
                QSslPreSharedKeyAuthenticator* const authenticator) {
                static_cast<void>(authenticator);
                if(ActiveHop != nullptr && ActiveHop->Reply == reply)
                {
                    ActiveHop->PreSharedKeyChallenge = true;
                    abort_hop(*ActiveHop);
                }
            });
    }

    [[nodiscard]] auto credential_matches(
        const github_wire_request& request) const noexcept -> bool
    {
        return std::visit(
            [&request, this](const auto& credential) noexcept {
                using credential_type =
                    std::remove_cvref_t<decltype(credential)>;
                if constexpr(
                    std::same_as<
                        credential_type,
                        bootstrap_credential>)
                {
                    const auto* parent =
                        std::get_if<
                            core::github_session_request_context>(
                            &request.Correlation.Parent);
                    return !credential.Used && parent != nullptr
                        && *parent == credential.Context
                        && request.Correlation.Operation
                            == read_operation::get_viewer_identity;
                }
                else
                {
                    return std::visit(
                        [&credential, this](const auto& parent) noexcept {
                            if(
                                parent.ApplicationSession
                                    != credential.Context.ApplicationSession
                                || parent.Profile != Profile.profile()
                                || parent.IdentityGeneration
                                    != credential.Context.IdentityGeneration)
                            {
                                return false;
                            }
                            using parent_type =
                                std::remove_cvref_t<decltype(parent)>;
                            if constexpr(
                                std::same_as<
                                    parent_type,
                                    core::pull_request_request_context>)
                            {
                                return parent.Host == Profile.host()
                                    && parent.Account
                                        == credential.Context.Authority.Account;
                            }
                            return true;
                        },
                        request.Correlation.Parent);
                }
            },
            Credential);
    }

    [[nodiscard]] auto credential_matches(
        const github_capability_probe_wire_request& request) const noexcept
        -> bool
    {
        const auto* credential = std::get_if<account_credential>(&Credential);
        if(credential == nullptr)
        {
            return false;
        }
        const auto& correlation = request.Correlation;
        return correlation.Session.Request == credential->Context.Request
               && correlation.Session.ApplicationSession
                      == credential->Context.ApplicationSession
               && correlation.Session.Profile == Profile.profile()
               && correlation.Session.IdentityGeneration
                      == credential->Context.IdentityGeneration
               && correlation.ProfileRevision == Profile.revision()
               && correlation.Host == Profile.host()
               && correlation.ObservedAccount
                      == credential->Context.Authority.Account
               && authority_matches_profile(credential->Context.Authority,
                                            Profile);
    }

    template <typename Request>
    [[nodiscard]] auto claim_credential(const Request& request) noexcept -> bool
    {
        if(!credential_matches(request))
        {
            return false;
        }
        if(auto* bootstrap =
               std::get_if<bootstrap_credential>(&Credential);
           bootstrap != nullptr)
        {
            bootstrap->Used = true;
        }
        return true;
    }

    void discard_bootstrap() noexcept
    {
        if(auto* bootstrap =
               std::get_if<bootstrap_credential>(&Credential);
           bootstrap != nullptr)
        {
            bootstrap->Material.reset();
            bootstrap->Used = true;
        }
    }

    [[nodiscard]] auto credential_bytes() const noexcept
        -> std::span<const std::byte>
    {
        return std::visit(
            [](const auto& credential) noexcept {
                return credential.Material.bytes();
            },
            Credential);
    }

    void configure_proxy(const QNetworkProxy& proxy)
    {
        Manager->clearAccessCache();
        Manager->clearConnectionCache();
        Manager->setProxy(proxy);
    }

    [[nodiscard]] auto proxy_candidates(const QUrl& url)
        -> std::optional<std::vector<QNetworkProxy>>
    {
#if defined(BPR_NATIVE_HTTPS_TESTING)
        if(Policy.Proxy == proxy_policy::fixed_test)
        {
            std::vector<QNetworkProxy> candidates;
            candidates.emplace_back(
                QNetworkProxy::HttpProxy,
                QString::fromUtf8(
                    Policy.ProxyHost.data(),
                    static_cast<qsizetype>(Policy.ProxyHost.size())),
                Policy.ProxyPort);
            if(Policy.FallbackProxyPort != 0U)
            {
                candidates.emplace_back(
                    QNetworkProxy::HttpProxy,
                    QString::fromUtf8(
                        Policy.ProxyHost.data(),
                        static_cast<qsizetype>(
                            Policy.ProxyHost.size())),
                    Policy.FallbackProxyPort);
            }
            return candidates;
        }
#endif
        return system_proxy_candidates(url);
    }

    [[nodiscard]] auto ssl_configuration() const
        -> std::optional<QSslConfiguration>
    {
        if(!QSslSocket::supportsSsl())
        {
            return std::nullopt;
        }
        auto configuration = QSslConfiguration::defaultConfiguration();
        configuration.setCaCertificates(
            QSslConfiguration::systemCaCertificates());
        configuration.setLocalCertificateChain({});
        configuration.setPrivateKey(QSslKey{});
        configuration.setBackendConfiguration({});
        configuration.setSessionTicket({});
        configuration.setPreSharedKeyIdentityHint({});
        configuration.setAllowedNextProtocols(
            {QByteArray{QSslConfiguration::NextProtocolHttp1_1}});
        configuration.setPeerVerifyMode(QSslSocket::VerifyPeer);
        configuration.setPeerVerifyDepth(0);
        configuration.setProtocol(QSsl::TlsV1_2OrLater);
        configuration.setSslOption(
            QSsl::SslOptionDisableCompression,
            true);
        configuration.setSslOption(
            QSsl::SslOptionDisableSessionTickets,
            true);
        configuration.setSslOption(
            QSsl::SslOptionDisableSessionSharing,
            true);
        configuration.setSslOption(
            QSsl::SslOptionDisableSessionPersistence,
            true);
        configuration.setSslOption(
            QSsl::SslOptionDisableLegacyRenegotiation,
            true);
#if defined(BPR_NATIVE_HTTPS_TESTING)
        if(!Policy.TrustedCaCertificatePem.empty())
        {
            const auto pem = QByteArray{
                Policy.TrustedCaCertificatePem.data(),
                static_cast<qsizetype>(
                    Policy.TrustedCaCertificatePem.size())};
            const auto certificates =
                QSslCertificate::fromData(pem, QSsl::Pem);
            if(certificates.size() != 1)
            {
                return std::nullopt;
            }
            configuration.addCaCertificate(certificates.front());
        }
#endif
        return configuration;
    }

    [[nodiscard]] auto make_request(
        const native_request& wire, const QUrl& url)
        -> std::optional<QNetworkRequest>
    {
        auto request = QNetworkRequest{url};
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::ManualRedirectPolicy);
        request.setAttribute(QNetworkRequest::CookieLoadControlAttribute,
                             QNetworkRequest::Manual);
        request.setAttribute(QNetworkRequest::CookieSaveControlAttribute,
                             QNetworkRequest::Manual);
        request.setAttribute(QNetworkRequest::AuthenticationReuseAttribute,
                             QNetworkRequest::Manual);
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                             QNetworkRequest::AlwaysNetwork);
        request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
        request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
        request.setAttribute(QNetworkRequest::Http2DirectAttribute, false);
        request.setAttribute(QNetworkRequest::Http2CleartextAllowedAttribute,
                             false);
        request.setMaximumRedirectsAllowed(0);
        request.setDecompressedSafetyCheckThreshold(0);

        for(const auto& header : wire.Headers)
        {
            request.setRawHeader(
                QByteArray{
                    header.Name.data(),
                    static_cast<qsizetype>(header.Name.size())},
                QByteArray{
                    header.Value.data(),
                    static_cast<qsizetype>(header.Value.size())});
        }
        request.setRawHeader("Accept-Encoding", "identity");
        request.setRawHeader("Accept-Language", "*");

        QByteArray authorization{"Bearer "};
        const auto credential = credential_bytes();
        authorization.append(
            reinterpret_cast<const char*>(credential.data()),
            static_cast<qsizetype>(credential.size()));
        request.setRawHeader("Authorization", authorization);
        wipe(authorization);

        const auto tls = ssl_configuration();
        if(!tls.has_value())
        {
            return std::nullopt;
        }
        request.setSslConfiguration(*tls);
        return request;
    }

    [[nodiscard]] auto execute_hop(
        const std::shared_ptr<operation_state>& operation,
        const QUrl& url,
        const QNetworkProxy& proxy) -> hop_result
    {
        if(cancellation_result(operation))
        {
            return hop_cancelled{};
        }
        if(!remaining_before_deadline(operation->Deadline).has_value())
        {
            return provider_error(
                application::github_provider_failure::timeout);
        }
        configure_proxy(proxy);
        if(cancellation_result(operation))
        {
            return hop_cancelled{};
        }
        if(!remaining_before_deadline(operation->Deadline).has_value())
        {
            return provider_error(
                application::github_provider_failure::timeout);
        }
        auto request = make_request(operation->Request, url);
        if(!request.has_value())
        {
            return provider_error(
                application::github_provider_failure::unavailable);
        }

        hop_state state{operation, Policy.ResponseBodyLimit};
        try
        {
            state.Body.reserve(Policy.ResponseBodyLimit + 1U);
        }
        catch(...)
        {
            return provider_error(
                application::github_provider_failure::response_limit);
        }

        QByteArray request_body;
        if(
            operation->Request.Method == http_method::post)
        {
            request_body = QByteArray{
                operation->Request.Body.data(),
                static_cast<qsizetype>(operation->Request.Body.size())};
        }
        if(cancellation_result(operation))
        {
            return hop_cancelled{};
        }
        const auto send_remaining =
            remaining_before_deadline(operation->Deadline);
        if(!send_remaining.has_value())
        {
            return provider_error(
                application::github_provider_failure::timeout);
        }
        request->setTransferTimeout(*send_remaining);
        if(operation->Request.Method == http_method::post)
        {
            state.Reply = Manager->post(*request, request_body);
        }
        else
        {
            state.Reply = Manager->get(*request);
        }
        if(state.Reply == nullptr)
        {
            return provider_error(
                application::github_provider_failure::network);
        }
        active_hop_guard active{state, ActiveHop};
        state.Reply->setReadBufferSize(static_cast<qint64>(std::min(
            Policy.ResponseBodyLimit + 1U, response_read_buffer_bytes)));

        QEventLoop loop;
        QTimer deadline;
        deadline.setSingleShot(true);
        QTimer cancellation;
        cancellation.setInterval(cancellation_poll_interval);

        QObject::connect(state.Reply, &QNetworkReply::encrypted, &loop,
                         [&state] { state.EncryptedSignalObserved = true; });
        QObject::connect(state.Reply, &QNetworkReply::metaDataChanged, &loop,
                         [&state] { inspect_metadata(state); });
        QObject::connect(state.Reply, &QIODevice::readyRead, &loop,
                         [&state] { read_available(state); });
        QObject::connect(state.Reply, &QNetworkReply::finished, &loop,
                         &QEventLoop::quit);
        QObject::connect(&deadline, &QTimer::timeout, &loop, [&state] {
            state.Deadline = true;
            abort_hop(state);
        });
        QObject::connect(&cancellation, &QTimer::timeout, &loop, [&state] {
            if(cancellation_result(state.Operation))
            {
                state.Cancellation = true;
                abort_hop(state);
            }
        });

        const auto wait_remaining =
            remaining_before_deadline(operation->Deadline);
        if(!wait_remaining.has_value())
        {
            state.Deadline = true;
            abort_hop(state);
        }
        else
        {
            deadline.start(*wait_remaining);
            cancellation.start();
            if(cancellation_result(operation))
            {
                state.Cancellation = true;
                abort_hop(state);
            }
            if(!state.Reply->isFinished())
            {
                loop.exec();
            }
        }
        deadline.stop();
        cancellation.stop();
        inspect_metadata(state);
        read_available(state);
        if(state.Cancellation || cancellation_result(operation))
        {
            active.reset();
            return hop_cancelled{};
        }
        if(
            state.Deadline
            || !remaining_before_deadline(operation->Deadline).has_value())
        {
            active.reset();
            return provider_error(
                application::github_provider_failure::timeout);
        }

        const auto error = state.Reply->error();
        const auto target =
            recognized_redirect_status(state.Status)
            ? redirect_target(*state.Reply, url)
            : std::nullopt;
        const auto tls_valid =
            !state.Certificate
            && tls_terminal_valid(
                *state.Reply,
                url,
                state.EncryptedSignalObserved);
        const auto terminal_expired =
            !remaining_before_deadline(operation->Deadline).has_value();
        active.reset();

        if(state.Cancellation || cancellation_result(operation))
        {
            return hop_cancelled{};
        }
        if(state.Deadline || terminal_expired)
        {
            return provider_error(
                application::github_provider_failure::timeout);
        }
        if(state.Certificate)
        {
            return provider_error(
                application::github_provider_failure::certificate);
        }
        if(
            state.AuthenticationChallenge
            || state.PreSharedKeyChallenge)
        {
            return provider_error(
                application::github_provider_failure::authentication);
        }
        if(state.ProxyAuthenticationChallenge)
        {
            return hop_proxy_failure{
                .Error = provider_error(
                    application::github_provider_failure::network),
            };
        }
        if(state.HeaderLimit || state.ResponseLimit)
        {
            return provider_error(
                application::github_provider_failure::response_limit);
        }
        if(state.Malformed)
        {
            return provider_error(
                application::github_provider_failure::malformed_response);
        }
        if(state.Status == 0)
        {
            const auto failure =
                error == QNetworkReply::NoError
                ? application::github_provider_failure::
                      malformed_response
                : network_failure(error);
            auto result = provider_error(failure);
            if(
                error != QNetworkReply::NoError
                && proxy_candidate_retryable(error))
            {
                return hop_proxy_failure{.Error = result};
            }
            return result;
        }
        if(!tls_valid)
        {
            return provider_error(
                application::github_provider_failure::certificate);
        }
        if(recognized_redirect_status(state.Status))
        {
            if(!target.has_value())
            {
                return provider_error(
                    application::github_provider_failure::redirect);
            }
            return hop_redirect{.Target = *target};
        }
        if(state.Status != 200)
        {
            return classify_http_status(state);
        }
        if(error != QNetworkReply::NoError)
        {
            return provider_error(network_failure(error));
        }
        if(
            !state.ContentType.has_value()
            || !supported_json_content_type(*state.ContentType))
        {
            return provider_error(
                application::github_provider_failure::malformed_response);
        }
        if(
            state.ContentLength.has_value()
            && *state.ContentLength != state.Body.size())
        {
            return provider_error(
                application::github_provider_failure::malformed_response);
        }
        const auto completeness =
            parse_link_completeness(state.Link);
        if(!completeness.has_value())
        {
            return provider_error(
                application::github_provider_failure::malformed_response);
        }
        return hop_success{
            .Body = std::move(state.Body),
            .Headers = allowlisted_success_headers(state),
            .Completeness = *completeness,
        };
    }

    [[nodiscard]] auto execute_with_proxy_candidates(
        const std::shared_ptr<operation_state>& operation, const QUrl& url,
        const std::vector<QNetworkProxy>& candidates) -> hop_result
    {
        for(std::size_t index = 0U; index < candidates.size(); ++index)
        {
            auto attempt = execute_hop(operation, url, candidates[index]);
            if(auto* proxy_failure = std::get_if<hop_proxy_failure>(&attempt);
               proxy_failure != nullptr)
            {
                if(index + 1U < candidates.size())
                {
                    continue;
                }
                return proxy_failure->Error;
            }
            return attempt;
        }
        return provider_error(application::github_provider_failure::network);
    }

    [[nodiscard]] auto
    execute(const std::shared_ptr<operation_state>& operation) -> hop_result
    {
        const auto initial = build_url(Profile, operation->Request);
        if(!initial.has_value())
        {
            return provider_error(
                application::github_provider_failure::unavailable);
        }

        auto current = *initial;
        std::vector<std::string> visited;
        visited.reserve(native_https_maximum_redirects + 1U);
        visited.emplace_back(
            safe_qbytearray_string(current.toEncoded(QUrl::FullyEncoded)));

        for(std::uint32_t hop = 0U; hop <= native_https_maximum_redirects;
            ++hop)
        {
            if(cancellation_result(operation))
            {
                return hop_cancelled{};
            }
            if(!remaining_before_deadline(operation->Deadline).has_value())
            {
                return provider_error(
                    application::github_provider_failure::timeout);
            }
            const auto candidates = proxy_candidates(current);
            if(cancellation_result(operation))
            {
                return hop_cancelled{};
            }
            if(!remaining_before_deadline(operation->Deadline).has_value())
            {
                return provider_error(
                    application::github_provider_failure::timeout);
            }
            if(!candidates.has_value() || candidates->empty())
            {
                return provider_error(
                    application::github_provider_failure::network);
            }

            auto result =
                execute_with_proxy_candidates(operation, current, *candidates);
            if(auto* success = std::get_if<hop_success>(&result);
               success != nullptr)
            {
                return std::move(*success);
            }
            if(std::holds_alternative<hop_cancelled>(result))
            {
                return hop_cancelled{};
            }
            if(auto* error =
                   std::get_if<application::github_provider_error>(&result);
               error != nullptr)
            {
                return *error;
            }

            const auto& redirect = std::get<hop_redirect>(result);
            if(hop == native_https_maximum_redirects
               || operation->Request.Method != http_method::get
               || operation->Request.Surface
                      != native_request_surface::rest_read)
            {
                return provider_error(
                    application::github_provider_failure::redirect);
            }
            const auto approved = validate_github_redirect_target(
                Profile, github_request_surface::rest, redirect.Target);
            if(!approved.has_value()
               || std::ranges::find(visited, redirect.Target) != visited.end())
            {
                return provider_error(
                    application::github_provider_failure::redirect);
            }
            visited.push_back(redirect.Target);
            const auto encoded =
                QByteArray{redirect.Target.data(),
                           static_cast<qsizetype>(redirect.Target.size())};
            const auto next = QUrl::fromEncoded(encoded, QUrl::StrictMode);
            if(!next.isValid() || next.toEncoded(QUrl::FullyEncoded) != encoded)
            {
                return provider_error(
                    application::github_provider_failure::redirect);
            }
            current = next;
        }
        return provider_error(application::github_provider_failure::redirect);
    }

    validated_github_host_profile Profile;
    credential_binding Credential;
    transport_policy Policy;
    QThread Thread;
    QObject* Dispatcher = nullptr;
    QNetworkAccessManager* Manager = nullptr;
    hop_state* ActiveHop = nullptr;
    std::atomic_bool Closing = false;
    std::atomic_flag Busy = ATOMIC_FLAG_INIT;
};

namespace
{
[[nodiscard]] auto make_production_policy() -> transport_policy
{
    return {};
}

#if defined(BPR_NATIVE_HTTPS_TESTING)
[[nodiscard]] auto make_test_policy(
    const native_https_test_configuration& test) -> transport_policy
{
    return {
        .Proxy = proxy_policy::fixed_test,
        .ProxyHost = test.ProxyHost,
        .ProxyPort = test.ProxyPort,
        .FallbackProxyPort = test.FallbackProxyPort,
        .TrustedCaCertificatePem =
            test.TrustedCaCertificatePem,
        .TotalTimeout = test.TotalTimeout,
        .ResponseBodyLimit = test.ResponseBodyLimit,
    };
}
#endif

template<typename Credential>
[[nodiscard]] auto make_transport(
    const validated_github_host_profile& profile,
    Credential credential,
    transport_policy policy) noexcept
    -> native_https_transport_factory_result
{
    if(!profile_policy_valid(profile))
    {
        return std::unexpected{
            profile.authentication_transport()
                    != application::github_authentication_transport::
                        native_https
                ? native_https_configuration_failure::
                      profile_transport_mismatch
                : native_https_configuration_failure::
                      profile_policy_mismatch};
    }
    try
    {
        auto implementation =
            std::make_unique<native_https_transport::implementation>(
                profile,
                std::move(credential),
                std::move(policy));
        if(!implementation->start())
        {
            return std::unexpected{
                native_https_configuration_failure::allocation_failure};
        }
        return std::make_unique<native_https_transport>(
            std::move(implementation));
    }
    catch(...)
    {
        return std::unexpected{
            native_https_configuration_failure::allocation_failure};
    }
}
} // namespace

native_https_transport::native_https_transport(
    std::unique_ptr<implementation> implementation) noexcept
    : Implementation{std::move(implementation)}
{
}

native_https_transport::~native_https_transport() = default;

auto native_https_transport::perform(const github_wire_request& request,
                                     const std::stop_token stop)
    -> github_wire_completion
{
    if(Implementation == nullptr)
    {
        return unavailable(request.Correlation);
    }
    return Implementation->perform(request, stop);
}

auto native_https_transport::perform_capability_probe(
    const github_capability_probe_wire_request& request,
    const std::stop_token stop) -> github_capability_probe_wire_completion
{
    if(Implementation == nullptr)
    {
        return capability_error(request.Correlation,
                                application::github_provider_failure::network);
    }
    return Implementation->perform_capability_probe(request, stop);
}

auto make_native_https_bootstrap_transport(
    const validated_github_host_profile& profile,
    core::github_session_request_context context,
    application::github_credential_material credential) noexcept
    -> native_https_transport_factory_result
{
    if(
        !application::detail::valid_context(context)
        || context.Profile != profile.profile())
    {
        return std::unexpected{
            native_https_configuration_failure::
                invalid_bootstrap_context};
    }
    if(!valid_bearer_material(credential))
    {
        return std::unexpected{
            native_https_configuration_failure::
                invalid_credential_material};
    }
    return make_transport(
        profile,
        bootstrap_credential{
            .Context = context,
            .Material = std::move(credential),
        },
        make_production_policy());
}

auto make_native_https_account_transport(
    const validated_github_host_profile& profile,
    application::github_credential_retrieve_success credential) noexcept
    -> native_https_transport_factory_result
{
    if(!application::valid(credential))
    {
        return std::unexpected{
            native_https_configuration_failure::
                invalid_account_credential};
    }
    if(!authority_matches_profile(credential.Context.Authority, profile))
    {
        return std::unexpected{
            native_https_configuration_failure::
                credential_authority_mismatch};
    }
    if(!valid_bearer_material(credential.Credential))
    {
        return std::unexpected{
            native_https_configuration_failure::
                invalid_credential_material};
    }
    return make_transport(
        profile,
        account_credential{
            .Context = credential.Context,
            .Material = std::move(credential.Credential),
        },
        make_production_policy());
}

#if defined(BPR_NATIVE_HTTPS_TESTING)
auto make_native_https_bootstrap_transport_for_test(
    const validated_github_host_profile& profile,
    core::github_session_request_context context,
    application::github_credential_material credential,
    const native_https_test_configuration& configuration) noexcept
    -> native_https_transport_factory_result
{
    if(
        !application::detail::valid_context(context)
        || context.Profile != profile.profile())
    {
        return std::unexpected{
            native_https_configuration_failure::
                invalid_bootstrap_context};
    }
    if(!valid_bearer_material(credential))
    {
        return std::unexpected{
            native_https_configuration_failure::
                invalid_credential_material};
    }
    if(!valid_test_configuration(configuration))
    {
        return std::unexpected{
            native_https_configuration_failure::
                invalid_test_configuration};
    }
    return make_transport(
        profile,
        bootstrap_credential{
            .Context = context,
            .Material = std::move(credential),
        },
        make_test_policy(configuration));
}

auto make_native_https_account_transport_for_test(
    const validated_github_host_profile& profile,
    application::github_credential_retrieve_success credential,
    const native_https_test_configuration& configuration) noexcept
    -> native_https_transport_factory_result
{
    if(!application::valid(credential))
    {
        return std::unexpected{
            native_https_configuration_failure::
                invalid_account_credential};
    }
    if(!authority_matches_profile(credential.Context.Authority, profile))
    {
        return std::unexpected{
            native_https_configuration_failure::
                credential_authority_mismatch};
    }
    if(!valid_bearer_material(credential.Credential))
    {
        return std::unexpected{
            native_https_configuration_failure::
                invalid_credential_material};
    }
    if(!valid_test_configuration(configuration))
    {
        return std::unexpected{
            native_https_configuration_failure::
                invalid_test_configuration};
    }
    return make_transport(
        profile,
        account_credential{
            .Context = credential.Context,
            .Material = std::move(credential.Credential),
        },
        make_test_policy(configuration));
}
#endif

auto observe_native_https_runtime(
    const validated_github_host_profile& profile) noexcept
    -> native_https_runtime_facts
{
    native_https_runtime_facts facts;
    if(!profile_policy_valid(profile))
    {
        return facts;
    }
    try
    {
        facts.TlsSupported = QSslSocket::supportsSsl();
        const auto backend = QSslSocket::activeBackend().toLower();
        if(backend == QStringLiteral("securetransport"))
        {
            facts.TlsBackend =
                native_https_tls_backend::secure_transport;
        }
        else if(backend == QStringLiteral("openssl"))
        {
            facts.TlsBackend = native_https_tls_backend::openssl;
        }
        else if(backend == QStringLiteral("schannel"))
        {
            facts.TlsBackend = native_https_tls_backend::schannel;
        }
        else if(!backend.isEmpty())
        {
            facts.TlsBackend = native_https_tls_backend::other;
        }

        const auto url = QUrl::fromEncoded(
            QByteArray{
                profile.rest_root().data(),
                static_cast<qsizetype>(profile.rest_root().size())},
            QUrl::StrictMode);
        const auto candidates =
            QNetworkProxyFactory::systemProxyForQuery(
                QNetworkProxyQuery{
                    url,
                    QNetworkProxyQuery::UrlRequest});
        const auto candidate_count =
            static_cast<std::uint64_t>(candidates.size());
        facts.ProxyCandidateCount = static_cast<std::uint32_t>(
            std::min(
                candidate_count,
                static_cast<std::uint64_t>(
                    std::numeric_limits<std::uint32_t>::max())));
        if(candidates.isEmpty())
        {
            facts.Proxy =
                native_https_system_proxy_kind::unavailable;
        }
        else
        {
            switch(candidates.front().type())
            {
            case QNetworkProxy::NoProxy:
                facts.Proxy = native_https_system_proxy_kind::direct;
                break;
            case QNetworkProxy::HttpProxy:
                facts.Proxy = native_https_system_proxy_kind::http;
                break;
            case QNetworkProxy::Socks5Proxy:
                facts.Proxy = native_https_system_proxy_kind::socks5;
                break;
            case QNetworkProxy::DefaultProxy:
            case QNetworkProxy::HttpCachingProxy:
            case QNetworkProxy::FtpCachingProxy:
                facts.Proxy =
                    native_https_system_proxy_kind::unsupported;
                break;
            }
        }
        return facts;
    }
    catch(...)
    {
        return facts;
    }
}
} // namespace bpr::adapters::github::detail
