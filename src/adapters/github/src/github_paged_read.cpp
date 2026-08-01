#include "github_paged_read.hpp"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <new>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace bpr::adapters::github::detail
{
namespace
{
constexpr std::size_t maximum_header_name_bytes = 256U;
constexpr std::size_t maximum_header_value_bytes = 8U * 1024U;
constexpr std::uint64_t maximum_retry_after_seconds = 86'400U;
constexpr double maximum_exact_json_integer = 9'007'199'254'740'991.0;
constexpr std::size_t maximum_production_page_bytes = 4U * 1024U * 1024U;
constexpr std::size_t maximum_production_aggregate_bytes = 64U * 1024U * 1024U;
constexpr std::size_t maximum_production_pages = 1'024U;
constexpr std::size_t maximum_production_page_records = 100U;
constexpr std::size_t maximum_production_cursor_bytes = 1'024U;
constexpr std::uint32_t maximum_graphql_sequence = 1'024U;

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

[[nodiscard]] auto
failed(const application::github_provider_failure failure,
       const std::optional<std::uint32_t> retry_after = std::nullopt,
       application::github_rate_limit_state rate_limits = {},
       std::optional<application::github_rate_limit_record> rate_limit_cause =
           std::nullopt) -> std::unexpected<github_paged_read_failure>
{
    return std::unexpected{
        github_paged_read_failure{provider_error(failure,
                                                 retry_after,
                                                 std::move(rate_limits),
                                                 std::move(rate_limit_cause))}};
}

[[nodiscard]] auto failed(const application::github_provider_error& error)
    -> std::unexpected<github_paged_read_failure>
{
    return std::unexpected{github_paged_read_failure{error}};
}

[[nodiscard]] auto
rate_limit_state(const application::github_rate_limit_record& record) noexcept
    -> application::github_rate_limit_state
{
    auto state = application::github_rate_limit_state{};
    static_cast<void>(application::observe(state, record));
    return state;
}

[[nodiscard]] auto cancelled() -> std::unexpected<github_paged_read_failure>
{
    return std::unexpected{
        github_paged_read_failure{github_paged_read_cancelled{}}};
}

[[nodiscard]] auto ascii_fold(const char value) noexcept -> char
{
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a')
                                        : value;
}

[[nodiscard]] auto ascii_equal_fold(const std::string_view left,
                                    const std::string_view right) noexcept
    -> bool
{
    return left.size() == right.size()
           && std::ranges::equal(left,
                                 right,
                                 [](const char a, const char b)
                                 { return ascii_fold(a) == ascii_fold(b); });
}

[[nodiscard]] auto trim_ows(std::string_view value) noexcept -> std::string_view
{
    while(!value.empty() && (value.front() == ' ' || value.front() == '\t'))
    {
        value.remove_prefix(1U);
    }
    while(!value.empty() && (value.back() == ' ' || value.back() == '\t'))
    {
        value.remove_suffix(1U);
    }
    return value;
}

[[nodiscard]] auto decimal(std::string_view value) noexcept
    -> std::optional<std::uint64_t>
{
    value = trim_ows(value);
    if(value.empty() || value.size() > 32U)
    {
        return std::nullopt;
    }
    std::uint64_t result = 0U;
    const auto parsed =
        std::from_chars(value.data(), value.data() + value.size(), result);
    if(parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
    {
        return std::nullopt;
    }
    return result;
}

[[nodiscard]] auto json_content_type(std::string_view value) noexcept -> bool
{
    if(value.size() > 256U)
    {
        return false;
    }
    const auto separator = value.find(';');
    const auto media = trim_ows(value.substr(0U, separator));
    if(!ascii_equal_fold(media, "application/json"))
    {
        return false;
    }
    if(separator == std::string_view::npos)
    {
        return true;
    }
    return ascii_equal_fold(trim_ows(value.substr(separator + 1U)),
                            "charset=utf-8");
}

enum class metadata_header : std::uint8_t
{
    content_type,
    link,
    limit,
    remaining,
    reset,
    resource,
    retry_after,
    count,
};

[[nodiscard]] auto metadata_kind(const std::string_view name) noexcept
    -> std::optional<metadata_header>
{
    if(ascii_equal_fold(name, "Content-Type"))
    {
        return metadata_header::content_type;
    }
    if(ascii_equal_fold(name, "Link"))
    {
        return metadata_header::link;
    }
    if(ascii_equal_fold(name, "X-RateLimit-Limit"))
    {
        return metadata_header::limit;
    }
    if(ascii_equal_fold(name, "X-RateLimit-Remaining"))
    {
        return metadata_header::remaining;
    }
    if(ascii_equal_fold(name, "X-RateLimit-Reset"))
    {
        return metadata_header::reset;
    }
    if(ascii_equal_fold(name, "X-RateLimit-Resource"))
    {
        return metadata_header::resource;
    }
    if(ascii_equal_fold(name, "Retry-After"))
    {
        return metadata_header::retry_after;
    }
    return std::nullopt;
}

[[nodiscard]] auto parse_metadata_impl(const wire_response& response)
    -> github_response_metadata_result
{
    std::array<bool, static_cast<std::size_t>(metadata_header::count)> seen{};
    std::optional<std::string> content_type;
    std::optional<std::uint64_t> limit;
    std::optional<std::uint64_t> remaining;
    std::optional<std::uint64_t> reset;
    std::optional<application::github_rate_limit_resource> resource;
    std::optional<std::uint64_t> retry_after;
    std::optional<std::string> link;

    for(const auto& header : response.Headers)
    {
        if(header.Name.empty() || header.Name.size() > maximum_header_name_bytes
           || header.Value.size() > maximum_header_value_bytes
           || header.Name.find_first_of("\r\n\0") != std::string::npos
           || header.Value.find_first_of("\r\n\0") != std::string::npos)
        {
            return std::unexpected{provider_error(
                application::github_provider_failure::malformed_response)};
        }
        const auto kind = metadata_kind(header.Name);
        if(!kind.has_value())
        {
            return std::unexpected{provider_error(
                application::github_provider_failure::malformed_response)};
        }
        const auto index = static_cast<std::size_t>(*kind);
        if(seen[index])
        {
            return std::unexpected{provider_error(
                application::github_provider_failure::malformed_response)};
        }
        seen[index] = true;

        switch(*kind)
        {
        case metadata_header::content_type:
            content_type = std::string{trim_ows(header.Value)};
            break;
        case metadata_header::link:
            link = std::string{trim_ows(header.Value)};
            break;
        case metadata_header::limit:
            limit = decimal(header.Value);
            break;
        case metadata_header::remaining:
            remaining = decimal(header.Value);
            break;
        case metadata_header::reset:
            reset = decimal(header.Value);
            break;
        case metadata_header::resource:
        {
            const auto value = trim_ows(header.Value);
            if(value == "core")
            {
                resource = application::github_rate_limit_resource::core;
            }
            else if(value == "search")
            {
                resource = application::github_rate_limit_resource::search;
            }
            else if(value == "graphql")
            {
                resource = application::github_rate_limit_resource::graphql;
            }
            break;
        }
        case metadata_header::retry_after:
            retry_after = decimal(header.Value);
            break;
        case metadata_header::count:
            return std::unexpected{provider_error(
                application::github_provider_failure::malformed_response)};
        }
    }

    const auto rate_header_count =
        static_cast<std::size_t>(
            seen[static_cast<std::size_t>(metadata_header::limit)])
        + static_cast<std::size_t>(
            seen[static_cast<std::size_t>(metadata_header::remaining)])
        + static_cast<std::size_t>(
            seen[static_cast<std::size_t>(metadata_header::reset)])
        + static_cast<std::size_t>(
            seen[static_cast<std::size_t>(metadata_header::resource)]);
    if(!content_type.has_value() || !json_content_type(*content_type)
       || (rate_header_count != 0U && rate_header_count != 4U)
       || (rate_header_count == 4U
           && (!limit.has_value() || !remaining.has_value()
               || !reset.has_value() || !resource.has_value()
               || *remaining > *limit))
       || (seen[static_cast<std::size_t>(metadata_header::retry_after)]
           && !retry_after.has_value())
       || (retry_after.has_value()
           && *retry_after > maximum_retry_after_seconds)
       || (link.has_value() && link->empty()))
    {
        return std::unexpected{provider_error(
            application::github_provider_failure::malformed_response)};
    }

    return github_response_metadata{
        .Link = std::move(link),
        .RateLimit =
            rate_header_count == 4U
                ? std::optional<
                      application::
                          github_rate_limit_record>{application::
                                                        github_rate_limit_record{
                                                            .Resource =
                                                                *resource,
                                                            .Limit = *limit,
                                                            .Remaining =
                                                                *remaining,
                                                            .ResetUnixSeconds =
                                                                *reset,
                                                            .SecondaryLimited =
                                                                false,
                                                        }}
                : std::nullopt,
        .RetryAfterSeconds =
            retry_after.has_value()
                ? std::optional{static_cast<std::uint32_t>(*retry_after)}
                : std::nullopt,
    };
}

enum class json_preflight_result : std::uint8_t
{
    valid,
    malformed,
    duplicate_key,
    structural_limit,
};

class json_structural_preflight
{
public:
    json_structural_preflight(const std::string_view input,
                              const std::size_t maximum_string_bytes)
        : Input{input}
        , MaximumStringBytes{maximum_string_bytes}
    {
    }

    [[nodiscard]] auto run() noexcept -> json_preflight_result
    {
        whitespace();
        const auto result = value(0U);
        if(result != json_preflight_result::valid)
        {
            return result;
        }
        whitespace();
        return Position == Input.size() ? json_preflight_result::valid
                                        : json_preflight_result::malformed;
    }

private:
    static constexpr std::size_t maximum_depth = 32U;
    static constexpr std::size_t maximum_raw_object_members = 256U;
    static constexpr std::size_t maximum_raw_array_elements = 10'000U;
    static constexpr std::size_t maximum_total_values = 1'000'000U;

    [[nodiscard]] auto value(const std::size_t depth) noexcept
        -> json_preflight_result
    {
        if(Values == maximum_total_values)
        {
            return json_preflight_result::structural_limit;
        }
        ++Values;
        whitespace();
        if(Position >= Input.size())
        {
            return json_preflight_result::malformed;
        }
        switch(Input[Position])
        {
        case '{':
            return object(depth);
        case '[':
            return array(depth);
        case '"':
            return string(nullptr, false) ? json_preflight_result::valid
                                          : json_preflight_result::malformed;
        case 't':
            return literal("true");
        case 'f':
            return literal("false");
        case 'n':
            return literal("null");
        default:
            return number();
        }
    }

    [[nodiscard]] auto object(const std::size_t depth) noexcept
        -> json_preflight_result
    {
        if(depth >= maximum_depth)
        {
            return json_preflight_result::structural_limit;
        }
        ++Position;
        whitespace();
        if(consume('}'))
        {
            return json_preflight_result::valid;
        }
        std::array<std::string_view, maximum_raw_object_members> keys;
        std::size_t count = 0U;
        while(true)
        {
            if(count == keys.size())
            {
                return json_preflight_result::structural_limit;
            }
            std::string_view key;
            if(!string(&key, true))
            {
                return json_preflight_result::malformed;
            }
            if(std::ranges::find(keys.begin(),
                                 keys.begin()
                                     + static_cast<std::ptrdiff_t>(count),
                                 key)
               != keys.begin() + static_cast<std::ptrdiff_t>(count))
            {
                return json_preflight_result::duplicate_key;
            }
            keys[count++] = key;
            whitespace();
            if(!consume(':'))
            {
                return json_preflight_result::malformed;
            }
            const auto child = value(depth + 1U);
            if(child != json_preflight_result::valid)
            {
                return child;
            }
            whitespace();
            if(consume('}'))
            {
                return json_preflight_result::valid;
            }
            if(!consume(','))
            {
                return json_preflight_result::malformed;
            }
            whitespace();
        }
    }

    [[nodiscard]] auto array(const std::size_t depth) noexcept
        -> json_preflight_result
    {
        if(depth >= maximum_depth)
        {
            return json_preflight_result::structural_limit;
        }
        ++Position;
        whitespace();
        if(consume(']'))
        {
            return json_preflight_result::valid;
        }
        std::size_t count = 0U;
        while(true)
        {
            if(count == maximum_raw_array_elements)
            {
                return json_preflight_result::structural_limit;
            }
            ++count;
            const auto child = value(depth + 1U);
            if(child != json_preflight_result::valid)
            {
                return child;
            }
            whitespace();
            if(consume(']'))
            {
                return json_preflight_result::valid;
            }
            if(!consume(','))
            {
                return json_preflight_result::malformed;
            }
            whitespace();
        }
    }

    [[nodiscard]] auto string(std::string_view* output,
                              const bool object_key) noexcept -> bool
    {
        if(!consume('"'))
        {
            return false;
        }
        const auto start = Position;
        while(Position < Input.size())
        {
            if(Position - start > MaximumStringBytes)
            {
                return false;
            }
            const auto character =
                static_cast<unsigned char>(Input[Position++]);
            if(character == '"')
            {
                if(output != nullptr)
                {
                    *output = Input.substr(start, Position - start - 1U);
                }
                return true;
            }
            if(character < 0x20U)
            {
                return false;
            }
            if(character != '\\')
            {
                continue;
            }
            if(object_key || Position >= Input.size())
            {
                return false;
            }
            const auto escaped = Input[Position++];
            if(escaped == '"' || escaped == '\\' || escaped == '/'
               || escaped == 'b' || escaped == 'f' || escaped == 'n'
               || escaped == 'r' || escaped == 't')
            {
                continue;
            }
            if(escaped != 'u' || Input.size() - Position < 4U)
            {
                return false;
            }
            for(std::size_t index = 0U; index < 4U; ++index)
            {
                const auto digit =
                    static_cast<unsigned char>(Input[Position++]);
                if(std::isxdigit(digit) == 0)
                {
                    return false;
                }
            }
        }
        return false;
    }

    [[nodiscard]] auto number() noexcept -> json_preflight_result
    {
        const auto start = Position;
        static_cast<void>(consume('-'));
        if(consume('0'))
        {
            if(Position < Input.size() && Input[Position] >= '0'
               && Input[Position] <= '9')
            {
                return json_preflight_result::malformed;
            }
        }
        else
        {
            if(Position >= Input.size() || Input[Position] < '1'
               || Input[Position] > '9')
            {
                return json_preflight_result::malformed;
            }
            while(Position < Input.size() && Input[Position] >= '0'
                  && Input[Position] <= '9')
            {
                ++Position;
            }
        }
        if(consume('.'))
        {
            const auto digits = Position;
            while(Position < Input.size() && Input[Position] >= '0'
                  && Input[Position] <= '9')
            {
                ++Position;
            }
            if(Position == digits)
            {
                return json_preflight_result::malformed;
            }
        }
        if(Position < Input.size()
           && (Input[Position] == 'e' || Input[Position] == 'E'))
        {
            ++Position;
            if(Position < Input.size()
               && (Input[Position] == '+' || Input[Position] == '-'))
            {
                ++Position;
            }
            const auto digits = Position;
            while(Position < Input.size() && Input[Position] >= '0'
                  && Input[Position] <= '9')
            {
                ++Position;
            }
            if(Position == digits)
            {
                return json_preflight_result::malformed;
            }
        }
        return Position != start ? json_preflight_result::valid
                                 : json_preflight_result::malformed;
    }

    [[nodiscard]] auto literal(const std::string_view expected) noexcept
        -> json_preflight_result
    {
        if(Input.substr(Position, expected.size()) != expected)
        {
            return json_preflight_result::malformed;
        }
        Position += expected.size();
        return json_preflight_result::valid;
    }

    void whitespace() noexcept
    {
        while(Position < Input.size()
              && (Input[Position] == ' ' || Input[Position] == '\t'
                  || Input[Position] == '\r' || Input[Position] == '\n'))
        {
            ++Position;
        }
    }

    [[nodiscard]] auto consume(const char expected) noexcept -> bool
    {
        if(Position >= Input.size() || Input[Position] != expected)
        {
            return false;
        }
        ++Position;
        return true;
    }

    std::string_view Input;
    std::size_t MaximumStringBytes = 0U;
    std::size_t Position = 0U;
    std::size_t Values = 0U;
};

[[nodiscard]] auto exact_integer(const QJsonValue& value,
                                 const bool positive = false) noexcept -> bool
{
    if(!value.isDouble())
    {
        return false;
    }
    const auto number = value.toDouble();
    return std::isfinite(number) && std::trunc(number) == number
           && number >= (positive ? 1.0 : 0.0)
           && number <= maximum_exact_json_integer;
}

[[nodiscard]] auto integer(const QJsonValue& value) noexcept -> std::uint64_t
{
    return static_cast<std::uint64_t>(value.toDouble());
}

[[nodiscard]] auto uint32(const QJsonValue& value) noexcept
    -> std::optional<std::uint32_t>
{
    if(!exact_integer(value))
    {
        return std::nullopt;
    }
    const auto result = integer(value);
    if(result > std::numeric_limits<std::uint32_t>::max())
    {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(result);
}

[[nodiscard]] auto text(const QJsonValue& value) -> std::string
{
    const auto bytes = value.toString().toUtf8();
    return {
        bytes.constData(),
        static_cast<std::size_t>(bytes.size()),
    };
}

[[nodiscard]] auto nonempty_string(const QJsonValue& value) -> bool
{
    return value.isString() && !value.toString().isEmpty();
}

[[nodiscard]] auto bounded_text(const std::string_view value,
                                const std::size_t maximum,
                                const bool required = false) noexcept -> bool
{
    return (!required || !value.empty()) && value.size() <= maximum
           && value.find('\0') == std::string_view::npos;
}

[[nodiscard]] auto sorted_keys(const QJsonObject& object)
    -> std::vector<std::string>
{
    std::vector<std::string> keys;
    keys.reserve(static_cast<std::size_t>(object.size()));
    for(auto iterator = object.constBegin(); iterator != object.constEnd();
        ++iterator)
    {
        keys.emplace_back(iterator.key().toStdString());
    }
    std::ranges::sort(keys);
    return keys;
}

[[nodiscard]] auto
has_exact_keys(const QJsonObject& object,
               const std::span<const std::string_view> expected) -> bool
{
    std::vector<std::string> keys;
    keys.reserve(expected.size());
    for(const auto key : expected)
    {
        keys.emplace_back(key);
    }
    std::ranges::sort(keys);
    return sorted_keys(object) == keys;
}

[[nodiscard]] auto
has_exact_keys(const QJsonObject& object,
               const std::initializer_list<std::string_view> expected) -> bool
{
    return has_exact_keys(
        object,
        std::span<const std::string_view>{expected.begin(), expected.size()});
}

[[nodiscard]] auto
has_required_keys(const QJsonObject& object,
                  const std::span<const std::string_view> required) -> bool
{
    return std::ranges::all_of(
        required,
        [&object](const std::string_view key)
        { return object.contains(QString::fromUtf8(key)); });
}

[[nodiscard]] auto
has_required_keys(const QJsonObject& object,
                  const std::initializer_list<std::string_view> required)
    -> bool
{
    return has_required_keys(
        object,
        std::span<const std::string_view>{required.begin(), required.size()});
}

[[nodiscard]] auto graphql_success_envelope(const QJsonObject& object) -> bool
{
    return has_exact_keys(object, {"data"})
           || (has_exact_keys(object, {"data", "extensions"})
               && object.value("extensions").isObject());
}

[[nodiscard]] auto one_of(const QJsonValue& value,
                          const std::initializer_list<std::string_view> allowed)
    -> bool
{
    if(!value.isString())
    {
        return false;
    }
    const auto candidate = text(value);
    return std::ranges::find(allowed, candidate) != allowed.end();
}

[[nodiscard]] auto hexadecimal_oid(const QJsonValue& value) -> bool
{
    if(!value.isString())
    {
        return false;
    }
    const auto candidate = text(value);
    return (candidate.size() == 40U || candidate.size() == 64U)
           && std::ranges::all_of(
               candidate,
               [](const char character)
               {
                   return (character >= '0' && character <= '9')
                          || (character >= 'a' && character <= 'f')
                          || (character >= 'A' && character <= 'F');
               });
}

[[nodiscard]] auto valid_line(const QJsonValue& value) noexcept -> bool
{
    return exact_integer(value, true)
           && value.toDouble()
                  <= static_cast<double>(application::maximum_github_line);
}

[[nodiscard]] auto optional_line(const QJsonValue& preferred,
                                 const QJsonValue& fallback)
    -> std::optional<std::uint32_t>
{
    const auto selected = preferred.isNull() ? fallback : preferred;
    return selected.isNull() ? std::nullopt : uint32(selected);
}

[[nodiscard]] auto response_byte_count(const wire_response& response) noexcept
    -> std::optional<std::size_t>
{
    auto total = response.Body.size();
    for(const auto& header : response.Headers)
    {
        if(header.Name.size() > std::numeric_limits<std::size_t>::max() - total)
        {
            return std::nullopt;
        }
        total += header.Name.size();
        if(header.Value.size()
           > std::numeric_limits<std::size_t>::max() - total)
        {
            return std::nullopt;
        }
        total += header.Value.size();
    }
    return total;
}

[[nodiscard]] auto limits_valid(const github_paged_read_limits& limits) noexcept
    -> bool
{
    return application::detail::within_security_ceiling(limits.Records)
           && limits.MaximumPageBytes != 0U
           && limits.MaximumPageBytes <= maximum_production_page_bytes
           && limits.MaximumAggregateResponseBytes != 0U
           && limits.MaximumAggregateResponseBytes
                  <= maximum_production_aggregate_bytes
           && limits.MaximumPageBytes <= limits.MaximumAggregateResponseBytes
           && limits.MaximumTotalPages != 0U
           && limits.MaximumTotalPages <= maximum_production_pages
           && limits.MaximumPageRecords != 0U
           && limits.MaximumPageRecords <= maximum_production_page_records
           && limits.MaximumCursorBytes != 0U
           && limits.MaximumCursorBytes <= maximum_production_cursor_bytes;
}

[[nodiscard]] auto ascii_contains_fold(const std::string_view text,
                                       const std::string_view phrase) noexcept
    -> bool
{
    if(phrase.empty() || phrase.size() > text.size())
    {
        return false;
    }
    for(std::size_t start = 0U; start <= text.size() - phrase.size(); ++start)
    {
        if(ascii_equal_fold(text.substr(start, phrase.size()), phrase))
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] auto
secondary_rate_limit_message(const QJsonDocument* const document) -> bool
{
    if(document == nullptr || !document->isObject())
    {
        return false;
    }
    const auto message = document->object().value("message");
    if(!message.isString())
    {
        return false;
    }
    const auto raw = text(message);
    return bounded_text(raw, 4U * 1'024U, true)
           && ascii_contains_fold(raw, "secondary rate limit");
}

[[nodiscard]] auto graphql_error_rate_limited(const QJsonValue& errors) -> bool
{
    if(!errors.isArray())
    {
        return false;
    }
    for(const auto& value : errors.toArray())
    {
        if(!value.isObject())
        {
            continue;
        }
        const auto error = value.toObject();
        if(error.value("type").isString()
           && error.value("type").toString() == QStringLiteral("RATE_LIMITED"))
        {
            return true;
        }
        const auto extensions = error.value("extensions");
        if(!extensions.isObject())
        {
            continue;
        }
        for(const auto key : {"type", "code"})
        {
            if(extensions.toObject().value(key).isString()
               && extensions.toObject().value(key).toString()
                      == QStringLiteral("RATE_LIMITED"))
            {
                return true;
            }
        }
    }
    return false;
}

[[nodiscard]] constexpr auto
expected_rate_limit_resource(const read_operation operation) noexcept
    -> application::github_rate_limit_resource
{
    if(operation == read_operation::list_review_threads)
    {
        return application::github_rate_limit_resource::graphql;
    }
    if(operation == read_operation::search_pull_requests)
    {
        return application::github_rate_limit_resource::search;
    }
    return application::github_rate_limit_resource::core;
}

[[nodiscard]] constexpr auto rate_limits_are_empty_or_exact(
    const application::github_rate_limit_state& state,
    const application::github_rate_limit_resource expected) noexcept -> bool
{
    if(!application::valid(state) || !application::has_observations(state))
    {
        return application::valid(state);
    }
    const auto count = static_cast<std::size_t>(state.Core.has_value())
                       + static_cast<std::size_t>(state.Search.has_value())
                       + static_cast<std::size_t>(state.Graphql.has_value());
    return count == 1U && application::find(state, expected) != nullptr;
}

[[nodiscard]] auto status_failure(const int status,
                                  const github_response_metadata& metadata,
                                  const bool secondary_rate_limit = false)
    -> application::github_provider_error
{
    const auto ordinary =
        [&metadata](const application::github_provider_failure failure,
                    const std::optional<std::uint32_t> retry_after =
                        std::nullopt)
    {
        return provider_error(failure,
                              retry_after,
                              metadata.RateLimit.has_value()
                                  ? rate_limit_state(*metadata.RateLimit)
                                  : application::github_rate_limit_state{});
    };
    const auto limited = [&metadata, status, secondary_rate_limit]()
    {
        if(!metadata.RateLimit.has_value())
        {
            return provider_error(
                application::github_provider_failure::malformed_response);
        }
        auto cause = *metadata.RateLimit;
        const auto primary = cause.Remaining == 0U;
        cause.SecondaryLimited =
            !primary
            && (status == 429 || secondary_rate_limit
                || (status == 403 && metadata.RetryAfterSeconds.has_value()));
        const auto retry_after =
            metadata.RetryAfterSeconds.has_value()
                ? metadata.RetryAfterSeconds
                : (!primary && (status == 429 || secondary_rate_limit)
                       ? std::optional<std::uint32_t>{60U}
                       : std::nullopt);
        return provider_error(application::github_provider_failure::rate_limit,
                              retry_after,
                              rate_limit_state(cause),
                              cause);
    };
    if(status == 401)
    {
        return ordinary(application::github_provider_failure::authentication);
    }
    if(status == 403)
    {
        if(metadata.RetryAfterSeconds.has_value()
           || (metadata.RateLimit.has_value()
               && metadata.RateLimit->Remaining == 0U)
           || secondary_rate_limit)
        {
            return limited();
        }
        return ordinary(application::github_provider_failure::permission);
    }
    if(status == 429)
    {
        return limited();
    }
    if(status == 404)
    {
        return ordinary(application::github_provider_failure::permission);
    }
    if(status == 408 || status == 504)
    {
        return ordinary(application::github_provider_failure::timeout);
    }
    if(status >= 500 && status <= 599)
    {
        return ordinary(application::github_provider_failure::unavailable,
                        metadata.RetryAfterSeconds);
    }
    if(status >= 400 && status <= 499)
    {
        return ordinary(application::github_provider_failure::api_version);
    }
    return ordinary(application::github_provider_failure::malformed_response);
}

struct parsed_page
{
    wire_request Request;
    github_response_metadata Metadata;
    page_completeness Completeness = page_completeness::unknown;
    QJsonDocument Document;
};

class production_read_execution
{
public:
    production_read_execution(github_wire_transport_port& transport,
                              const validated_github_host_profile& profile,
                              const core::pull_request_request_context& parent,
                              const github_paged_read_limits& limits,
                              const std::stop_token stop,
                              application::github_rate_limit_state& rate_limits)
        : Transport{transport}
        , Profile{profile}
        , Parent{parent}
        , Limits{limits}
        , Stop{stop}
        , RateLimits{rate_limits}
    {
    }

    [[nodiscard]] auto
    enrich(application::github_provider_error error) const noexcept
        -> application::github_provider_error
    {
        auto accumulated = RateLimits;
        if(!application::merge(accumulated, error.RateLimits))
        {
            return provider_error(
                application::github_provider_failure::malformed_response,
                std::nullopt,
                RateLimits);
        }
        error.RateLimits = std::move(accumulated);
        return application::valid(error)
                   ? error
                   : provider_error(application::github_provider_failure::
                                        malformed_response,
                                    std::nullopt,
                                    RateLimits);
    }

    [[nodiscard]] auto enrich(github_paged_read_failure failure) const noexcept
        -> github_paged_read_failure
    {
        if(auto* error =
               std::get_if<application::github_provider_error>(&failure);
           error != nullptr)
        {
            *error = enrich(std::move(*error));
        }
        return failure;
    }

    [[nodiscard]] auto
    terminal_failure(const application::github_provider_failure failure) const
        -> std::unexpected<github_paged_read_failure>
    {
        return failed(failure, std::nullopt, RateLimits);
    }

    [[nodiscard]] auto fetch(const semantic_read_request& semantic)
        -> std::expected<parsed_page, github_paged_read_failure>
    {
        if(Stop.stop_requested())
        {
            return cancelled();
        }
        if(Pages == Limits.MaximumTotalPages)
        {
            return terminal_failure(
                application::github_provider_failure::response_limit);
        }

        const auto compiled = compile_read_request(semantic);
        if(!compiled.has_value())
        {
            return terminal_failure(
                application::github_provider_failure::malformed_response);
        }
        const github_wire_correlation correlation{
            .Parent = Parent,
            .ProfileRevision = Profile.revision(),
            .Host = Profile.host(),
            .Operation = semantic.Operation,
            .Page = semantic.Page,
        };
        const auto completion = Transport.perform(
            github_wire_request{
                .Correlation = correlation,
                .Wire = *compiled,
            },
            Stop);
        if(completion.valueless_by_exception())
        {
            return terminal_failure(
                application::github_provider_failure::malformed_response);
        }
        if(const auto* success = std::get_if<github_wire_success>(&completion);
           success != nullptr)
        {
            if(success->Correlation != correlation)
            {
                return terminal_failure(
                    application::github_provider_failure::malformed_response);
            }
            const auto bytes = response_byte_count(success->Response);
            if(!bytes.has_value() || *bytes > Limits.MaximumPageBytes
               || *bytes
                      > Limits.MaximumAggregateResponseBytes - AggregateBytes)
            {
                return terminal_failure(
                    application::github_provider_failure::response_limit);
            }
            ++Pages;
            AggregateBytes += *bytes;

            const auto metadata =
                parse_github_response_metadata(success->Response);
            if(!metadata.has_value())
            {
                return failed(enrich(metadata.error()));
            }
            const auto expected_resource =
                expected_rate_limit_resource(semantic.Operation);
            if((!metadata->RateLimit.has_value()
                && semantic.Server == server_contract::github_com)
               || (metadata->RateLimit.has_value()
                   && metadata->RateLimit->Resource != expected_resource))
            {
                return terminal_failure(
                    application::github_provider_failure::malformed_response);
            }
            if(metadata->RateLimit.has_value()
               && !application::observe(RateLimits, *metadata->RateLimit))
            {
                return terminal_failure(
                    application::github_provider_failure::malformed_response);
            }
            const auto metadata_failed =
                [this,
                 &metadata](const application::github_provider_failure failure,
                            const std::optional<std::uint32_t> retry_after =
                                std::nullopt,
                            const bool secondary = false)
            {
                auto rate_limits = RateLimits;
                auto cause = metadata->RateLimit;
                if(cause.has_value()
                   && failure
                          == application::github_provider_failure::rate_limit)
                {
                    cause->SecondaryLimited =
                        cause->Remaining != 0U && secondary;
                    static_cast<void>(
                        application::observe(rate_limits, *cause));
                }
                else
                {
                    cause = std::nullopt;
                }
                return failed(failure,
                              retry_after,
                              std::move(rate_limits),
                              std::move(cause));
            };
            if(success->Response.Status != 200)
            {
                std::optional<QJsonDocument> error_document;
                if(!success->Response.Body.empty())
                {
                    const auto error_preflight =
                        json_structural_preflight{success->Response.Body,
                                                  Limits.MaximumPageBytes}
                            .run();
                    if(error_preflight
                       == json_preflight_result::structural_limit)
                    {
                        return metadata_failed(
                            application::github_provider_failure::
                                response_limit);
                    }
                    if(error_preflight == json_preflight_result::valid)
                    {
                        QJsonParseError error;
                        auto candidate = QJsonDocument::fromJson(
                            QByteArray{
                                success->Response.Body.data(),
                                static_cast<qsizetype>(
                                    success->Response.Body.size()),
                            },
                            &error);
                        if(error.error == QJsonParseError::NoError)
                        {
                            error_document = std::move(candidate);
                        }
                    }
                }
                return failed(enrich(status_failure(
                    success->Response.Status,
                    *metadata,
                    error_document.has_value()
                        && secondary_rate_limit_message(&*error_document))));
            }
            if(success->Response.Body.empty())
            {
                return metadata_failed(
                    application::github_provider_failure::malformed_response);
            }
            const auto preflight =
                json_structural_preflight{success->Response.Body,
                                          Limits.MaximumPageBytes}
                    .run();
            if(preflight == json_preflight_result::structural_limit)
            {
                return metadata_failed(
                    application::github_provider_failure::response_limit);
            }
            if(preflight != json_preflight_result::valid)
            {
                return metadata_failed(
                    application::github_provider_failure::malformed_response);
            }
            QJsonParseError parse_error;
            auto document = QJsonDocument::fromJson(
                QByteArray{
                    success->Response.Body.data(),
                    static_cast<qsizetype>(success->Response.Body.size()),
                },
                &parse_error);
            if(parse_error.error != QJsonParseError::NoError)
            {
                return metadata_failed(
                    application::github_provider_failure::malformed_response);
            }
            if(semantic.Operation == read_operation::list_review_threads
               && document.isObject() && document.object().contains("errors"))
            {
                const auto structured_rate_limit = graphql_error_rate_limited(
                    document.object().value("errors"));
                if((metadata->RateLimit.has_value()
                    && metadata->RateLimit->Remaining == 0U)
                   || metadata->RetryAfterSeconds.has_value()
                   || structured_rate_limit)
                {
                    if(!metadata->RateLimit.has_value())
                    {
                        return metadata_failed(
                            application::github_provider_failure::
                                malformed_response);
                    }
                    const auto secondary =
                        metadata->RateLimit->Remaining != 0U
                        && (structured_rate_limit
                            || metadata->RetryAfterSeconds.has_value());
                    return metadata_failed(
                        application::github_provider_failure::rate_limit,
                        metadata->RetryAfterSeconds.has_value()
                            ? metadata->RetryAfterSeconds
                            : (structured_rate_limit
                                       && metadata->RateLimit->Remaining != 0U
                                   ? std::optional<std::uint32_t>{60U}
                                   : std::nullopt),
                        secondary);
                }
                return metadata_failed(
                    application::github_provider_failure::malformed_response);
            }
            if(metadata->RetryAfterSeconds.has_value())
            {
                return metadata_failed(
                    application::github_provider_failure::malformed_response);
            }
            return parsed_page{
                .Request = std::move(*compiled),
                .Metadata = *metadata,
                .Completeness = success->Response.Completeness,
                .Document = std::move(document),
            };
        }
        if(const auto* error = std::get_if<github_wire_error>(&completion);
           error != nullptr)
        {
            if(error->Correlation != correlation
               || !application::valid(error->Error)
               || !rate_limits_are_empty_or_exact(
                   error->Error.RateLimits,
                   expected_rate_limit_resource(semantic.Operation)))
            {
                return terminal_failure(
                    application::github_provider_failure::malformed_response);
            }
            return failed(enrich(error->Error));
        }
        const auto* cancellation =
            std::get_if<github_wire_cancelled>(&completion);
        if(cancellation == nullptr || cancellation->Correlation != correlation)
        {
            return terminal_failure(
                application::github_provider_failure::malformed_response);
        }
        return cancelled();
    }

    [[nodiscard]] auto rate_limits() const noexcept
        -> const application::github_rate_limit_state&
    {
        return RateLimits;
    }

    [[nodiscard]] auto
    bind_repository(const core::repository_id repository) noexcept -> bool
    {
        if(repository.value() == 0U
           || (BoundRepository.has_value() && *BoundRepository != repository))
        {
            return false;
        }
        BoundRepository = repository;
        return true;
    }

    [[nodiscard]] auto bound_repository() const noexcept
        -> std::optional<core::repository_id>
    {
        return BoundRepository;
    }

private:
    github_wire_transport_port& Transport;
    const validated_github_host_profile& Profile;
    const core::pull_request_request_context& Parent;
    const github_paged_read_limits& Limits;
    std::stop_token Stop;
    std::size_t Pages = 0U;
    std::size_t AggregateBytes = 0U;
    application::github_rate_limit_state& RateLimits;
    std::optional<core::repository_id> BoundRepository;
};

[[nodiscard]] auto
server_contract_for(const validated_github_host_profile& profile)
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

[[nodiscard]] auto
semantic_request_for(const server_contract server,
                     const core::pull_request_request_context& context,
                     const read_operation operation,
                     const std::uint32_t page = 1U) -> semantic_read_request
{
    semantic_read_request result{
        .Server = server,
        .Operation = operation,
        .Owner = context.Repository.Owner,
        .Repository = context.Repository.Name,
        .PullRequestNumber = 0U,
        .HeadOid = {},
        .SearchTerms = {},
        .Page = page,
        .GraphqlPage = std::nullopt,
    };
    switch(operation)
    {
    case read_operation::get_pull_request:
    case read_operation::list_pull_request_files:
    case read_operation::list_pull_request_commits:
    case read_operation::list_reviews:
    case read_operation::list_review_threads:
    case read_operation::list_issue_comments:
        result.PullRequestNumber = context.Number.value();
        break;
    case read_operation::get_viewer_identity:
    case read_operation::get_repository:
    case read_operation::list_checks:
    case read_operation::search_pull_requests:
        break;
    }
    return result;
}

[[nodiscard]] auto singleton_complete(const parsed_page& page) noexcept -> bool
{
    return page.Completeness == page_completeness::complete
           && !page.Metadata.Link.has_value();
}

struct link_relation
{
    std::string Url;
    std::string Relation;
};

[[nodiscard]] auto parse_link_relations(const std::string_view value)
    -> std::optional<std::vector<link_relation>>
{
    if(value.empty() || value.size() > maximum_header_value_bytes)
    {
        return std::nullopt;
    }
    std::vector<link_relation> relations;
    std::size_t position = 0U;
    while(position < value.size())
    {
        if(relations.size() == 4U)
        {
            return std::nullopt;
        }
        auto segment_end = value.find(',', position);
        if(segment_end == std::string_view::npos)
        {
            segment_end = value.size();
        }
        const auto segment =
            trim_ows(value.substr(position, segment_end - position));
        const auto close = segment.find('>');
        if(segment.size() < 10U || segment.front() != '<'
           || close == std::string_view::npos || close == 1U)
        {
            return std::nullopt;
        }
        const auto parameter = trim_ows(segment.substr(close + 1U));
        constexpr std::string_view prefix{"; rel=\""};
        if(!parameter.starts_with(prefix)
           || parameter.size() <= prefix.size() + 1U || parameter.back() != '"'
           || parameter.find('"', prefix.size()) != parameter.size() - 1U)
        {
            return std::nullopt;
        }
        const auto relation = parameter.substr(
            prefix.size(), parameter.size() - prefix.size() - 1U);
        if(relation != "next" && relation != "prev" && relation != "first"
           && relation != "last")
        {
            return std::nullopt;
        }
        if(std::ranges::any_of(relations,
                               [relation](const link_relation& existing)
                               { return existing.Relation == relation; }))
        {
            return std::nullopt;
        }
        relations.push_back(link_relation{
            .Url = std::string{segment.substr(1U, close - 1U)},
            .Relation = std::string{relation},
        });
        position = segment_end;
        if(position < value.size())
        {
            if(position + 1U == value.size())
            {
                return std::nullopt;
            }
            ++position;
        }
    }
    return relations.empty() ? std::nullopt
                             : std::optional{std::move(relations)};
}

struct url_query_parameter
{
    std::string_view Name;
    std::string_view Value;
};

[[nodiscard]] auto parse_url_query(const std::string_view query)
    -> std::optional<std::vector<url_query_parameter>>
{
    if(query.empty() || query.size() > maximum_header_value_bytes)
    {
        return std::nullopt;
    }
    std::vector<url_query_parameter> result;
    std::size_t position = 0U;
    while(position < query.size())
    {
        if(result.size() == 4U)
        {
            return std::nullopt;
        }
        auto end = query.find('&', position);
        if(end == std::string_view::npos)
        {
            end = query.size();
        }
        const auto parameter = query.substr(position, end - position);
        const auto equals = parameter.find('=');
        if(parameter.empty() || equals == std::string_view::npos || equals == 0U
           || equals + 1U == parameter.size()
           || parameter.find('=', equals + 1U) != std::string_view::npos)
        {
            return std::nullopt;
        }
        const auto name = parameter.substr(0U, equals);
        if(std::ranges::any_of(result,
                               [name](const url_query_parameter& existing)
                               { return existing.Name == name; }))
        {
            return std::nullopt;
        }
        result.push_back({
            .Name = name,
            .Value = parameter.substr(equals + 1U),
        });
        if(end == query.size())
        {
            break;
        }
        position = end + 1U;
    }
    return result;
}

[[nodiscard]] auto
numeric_repository_alias(const wire_request& request,
                         const core::repository_id repository)
    -> std::optional<std::string>
{
    switch(request.Operation)
    {
    case read_operation::list_pull_request_files:
    case read_operation::list_pull_request_commits:
    case read_operation::list_reviews:
    case read_operation::list_issue_comments:
    case read_operation::list_checks:
        break;
    case read_operation::get_viewer_identity:
    case read_operation::get_repository:
    case read_operation::get_pull_request:
    case read_operation::list_review_threads:
    case read_operation::search_pull_requests:
        return std::nullopt;
    }
    const auto named_repository = std::string{"/repos/"}
                                  + request.Semantic.Owner + "/"
                                  + request.Semantic.Repository;
    const auto position = request.Path.find(named_repository);
    if(position == std::string::npos
       || request.Path.find(named_repository, position + 1U)
              != std::string::npos)
    {
        return std::nullopt;
    }
    return request.Path.substr(0U, position) + "/repositories/"
           + std::to_string(repository.value())
           + request.Path.substr(position + named_repository.size());
}

[[nodiscard]] auto query_value_matches(const read_operation operation,
                                       const std::string_view name,
                                       const std::string_view actual,
                                       const std::string_view expected) noexcept
    -> bool
{
    if(actual == expected)
    {
        return true;
    }
    if(operation != read_operation::search_pull_requests || name != "q")
    {
        return false;
    }

    std::size_t actual_position = 0U;
    std::size_t expected_position = 0U;
    while(expected_position < expected.size())
    {
        if(expected.substr(expected_position).starts_with("%20"))
        {
            if(actual_position == actual.size()
               || actual[actual_position] != '+')
            {
                return false;
            }
            ++actual_position;
            expected_position += 3U;
            continue;
        }
        if(actual_position == actual.size()
           || actual[actual_position] != expected[expected_position])
        {
            return false;
        }
        ++actual_position;
        ++expected_position;
    }
    return actual_position == actual.size();
}

[[nodiscard]] auto
page_number_from_url(const validated_github_host_profile& profile,
                     const parsed_page& page,
                     const core::repository_id repository,
                     const std::string_view url) -> std::optional<std::uint32_t>
{
    if(url.size() > maximum_header_value_bytes || !url.starts_with("https://")
       || std::ranges::any_of(
           url,
           [](const unsigned char character)
           { return character <= 0x20U || character >= 0x7FU; }))
    {
        return std::nullopt;
    }
    constexpr std::size_t scheme_size = std::string_view{"https://"}.size();
    const auto path_start = url.find('/', scheme_size);
    const auto query_start = path_start == std::string_view::npos
                                 ? std::string_view::npos
                                 : url.find('?', path_start);
    if(path_start == std::string_view::npos
       || query_start == std::string_view::npos
       || url.find('?', query_start + 1U) != std::string_view::npos
       || url.find('#') != std::string_view::npos)
    {
        return std::nullopt;
    }
    const auto authority = url.substr(scheme_size, path_start - scheme_size);
    if(authority != profile.rest_authority()
       || authority.find('@') != std::string_view::npos
       || authority.find(':') != std::string_view::npos)
    {
        return std::nullopt;
    }
    const auto path = url.substr(path_start, query_start - path_start);
    const auto query = url.substr(query_start + 1U);
    const auto parameters = parse_url_query(query);
    if(!parameters.has_value())
    {
        return std::nullopt;
    }
    const auto page_parameter = std::ranges::find(
        *parameters, std::string_view{"page"}, &url_query_parameter::Name);
    const auto per_page_parameter = std::ranges::find(
        *parameters, std::string_view{"per_page"}, &url_query_parameter::Name);
    if(page_parameter == parameters->end()
       || per_page_parameter == parameters->end()
       || per_page_parameter->Value != "100")
    {
        return std::nullopt;
    }
    const auto number = decimal(page_parameter->Value);
    if(!number.has_value() || *number == 0U
       || *number > std::numeric_limits<std::uint32_t>::max())
    {
        return std::nullopt;
    }
    const auto relation_page = static_cast<std::uint32_t>(*number);
    if(std::to_string(relation_page) != page_parameter->Value)
    {
        return std::nullopt;
    }

    auto semantic = page.Request.Semantic;
    semantic.Page = relation_page;
    const auto canonical = compile_read_request(semantic);
    if(!canonical.has_value())
    {
        return std::nullopt;
    }
    const auto alias = numeric_repository_alias(*canonical, repository);
    if(path != canonical->Path && (!alias.has_value() || path != *alias))
    {
        return std::nullopt;
    }
    const auto canonical_parameters = parse_url_query(canonical->Query);
    if(!canonical_parameters.has_value()
       || parameters->size() != canonical_parameters->size())
    {
        return std::nullopt;
    }
    for(const auto& expected : *canonical_parameters)
    {
        const auto actual = std::ranges::find(
            *parameters, expected.Name, &url_query_parameter::Name);
        if(actual == parameters->end()
           || !query_value_matches(canonical->Operation,
                                   expected.Name,
                                   actual->Value,
                                   expected.Value))
        {
            return std::nullopt;
        }
    }
    return relation_page;
}

[[nodiscard]] auto
rest_has_next_page(const validated_github_host_profile& profile,
                   const core::repository_id repository,
                   const parsed_page& page,
                   const std::size_t record_count)
    -> std::expected<bool, github_paged_read_failure>
{
    if(page.Completeness == page_completeness::unknown
       || page.Request.Method != http_method::get || page.Request.Query.empty())
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    if(!page.Metadata.Link.has_value())
    {
        if(page.Completeness != page_completeness::complete)
        {
            return failed(
                application::github_provider_failure::malformed_response);
        }
        return false;
    }

    const auto relations = parse_link_relations(*page.Metadata.Link);
    if(!relations.has_value())
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    bool has_next = false;
    std::optional<std::uint32_t> last_page;
    for(const auto& relation : *relations)
    {
        const auto relation_page =
            page_number_from_url(profile, page, repository, relation.Url);
        if(!relation_page.has_value())
        {
            return failed(
                application::github_provider_failure::malformed_response);
        }
        const auto current = page.Request.Semantic.Page;
        if((relation.Relation == "next"
            && (current == std::numeric_limits<std::uint32_t>::max()
                || *relation_page != current + 1U))
           || (relation.Relation == "prev"
               && (current <= 1U || *relation_page != current - 1U))
           || (relation.Relation == "first"
               && (current <= 1U || *relation_page != 1U))
           || (relation.Relation == "last" && *relation_page < current))
        {
            return failed(
                application::github_provider_failure::malformed_response);
        }
        has_next = has_next || relation.Relation == "next";
        if(relation.Relation == "last")
        {
            last_page = *relation_page;
        }
    }

    if((has_next && page.Completeness != page_completeness::has_next_page)
       || (!has_next && page.Completeness != page_completeness::complete)
       || (has_next && record_count != 100U)
       || (last_page.has_value()
           && ((has_next && *last_page <= page.Request.Semantic.Page)
               || (!has_next && *last_page != page.Request.Semantic.Page))))
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    return has_next;
}

[[nodiscard]] auto parse_rest_actor(const QJsonValue& value)
    -> std::optional<application::github_actor_record>
{
    if(value.isNull())
    {
        return std::optional<application::github_actor_record>{};
    }
    if(!value.isObject()
       || !has_required_keys(value.toObject(), {"login", "node_id"})
       || !nonempty_string(value.toObject().value("login"))
       || !nonempty_string(value.toObject().value("node_id")))
    {
        return std::nullopt;
    }
    const auto object = value.toObject();
    return application::github_actor_record{
        .Actor = application::github_actor_id{text(object.value("node_id"))},
        .Login = text(object.value("login")),
    };
}

struct optional_actor_parse
{
    bool Valid = false;
    std::optional<application::github_actor_record> Actor;
};

[[nodiscard]] auto optional_rest_actor(const QJsonValue& value)
    -> optional_actor_parse
{
    if(value.isNull())
    {
        return {.Valid = true, .Actor = std::nullopt};
    }
    const auto actor = parse_rest_actor(value);
    return actor.has_value()
               ? optional_actor_parse{.Valid = true, .Actor = *actor}
               : optional_actor_parse{};
}

[[nodiscard]] auto optional_graphql_actor(const QJsonValue& value)
    -> optional_actor_parse
{
    if(value.isNull())
    {
        return {.Valid = true, .Actor = std::nullopt};
    }
    if(!value.isObject() || !has_exact_keys(value.toObject(), {"id", "login"})
       || !nonempty_string(value.toObject().value("id"))
       || !nonempty_string(value.toObject().value("login")))
    {
        return {};
    }
    const auto object = value.toObject();
    return {
        .Valid = true,
        .Actor =
            application::github_actor_record{
                .Actor = application::github_actor_id{text(object.value("id"))},
                .Login = text(object.value("login")),
            },
    };
}

[[nodiscard]] auto pull_request_state(const QString& state,
                                      const bool merged) noexcept
    -> application::github_pull_request_state
{
    if(merged)
    {
        return application::github_pull_request_state::merged;
    }
    return state == QStringLiteral("open")
               ? application::github_pull_request_state::open
               : application::github_pull_request_state::closed;
}

[[nodiscard]] auto file_change(const QString& value) noexcept
    -> application::github_file_change
{
    if(value == QStringLiteral("added"))
    {
        return application::github_file_change::added;
    }
    if(value == QStringLiteral("modified"))
    {
        return application::github_file_change::modified;
    }
    if(value == QStringLiteral("removed"))
    {
        return application::github_file_change::removed;
    }
    if(value == QStringLiteral("renamed"))
    {
        return application::github_file_change::renamed;
    }
    if(value == QStringLiteral("copied"))
    {
        return application::github_file_change::copied;
    }
    if(value == QStringLiteral("unchanged"))
    {
        return application::github_file_change::unchanged;
    }
    return application::github_file_change::type_changed;
}

[[nodiscard]] auto review_state(const QString& value) noexcept
    -> application::github_review_state
{
    if(value == QStringLiteral("PENDING"))
    {
        return application::github_review_state::pending;
    }
    if(value == QStringLiteral("COMMENTED"))
    {
        return application::github_review_state::commented;
    }
    if(value == QStringLiteral("APPROVED"))
    {
        return application::github_review_state::approved;
    }
    if(value == QStringLiteral("CHANGES_REQUESTED"))
    {
        return application::github_review_state::changes_requested;
    }
    return application::github_review_state::dismissed;
}

[[nodiscard]] auto check_status(const QString& value) noexcept
    -> application::github_check_status
{
    if(value == QStringLiteral("queued"))
    {
        return application::github_check_status::queued;
    }
    if(value == QStringLiteral("in_progress"))
    {
        return application::github_check_status::in_progress;
    }
    if(value == QStringLiteral("completed"))
    {
        return application::github_check_status::completed;
    }
    if(value == QStringLiteral("waiting"))
    {
        return application::github_check_status::waiting;
    }
    if(value == QStringLiteral("requested"))
    {
        return application::github_check_status::requested;
    }
    return application::github_check_status::pending;
}

[[nodiscard]] auto check_conclusion(const QJsonValue& value) noexcept
    -> application::github_check_conclusion
{
    if(value.isNull())
    {
        return application::github_check_conclusion::none;
    }
    const auto conclusion = value.toString();
    if(conclusion == QStringLiteral("success"))
    {
        return application::github_check_conclusion::success;
    }
    if(conclusion == QStringLiteral("failure"))
    {
        return application::github_check_conclusion::failure;
    }
    if(conclusion == QStringLiteral("neutral"))
    {
        return application::github_check_conclusion::neutral;
    }
    if(conclusion == QStringLiteral("cancelled"))
    {
        return application::github_check_conclusion::cancelled;
    }
    if(conclusion == QStringLiteral("skipped"))
    {
        return application::github_check_conclusion::skipped;
    }
    if(conclusion == QStringLiteral("timed_out"))
    {
        return application::github_check_conclusion::timed_out;
    }
    if(conclusion == QStringLiteral("action_required"))
    {
        return application::github_check_conclusion::action_required;
    }
    return application::github_check_conclusion::stale;
}

[[nodiscard]] auto viewed_state(const QString& value) noexcept
    -> std::optional<application::github_file_viewed_state>
{
    if(value == QStringLiteral("DISMISSED"))
    {
        return application::github_file_viewed_state::dismissed;
    }
    if(value == QStringLiteral("UNVIEWED"))
    {
        return application::github_file_viewed_state::unviewed;
    }
    if(value == QStringLiteral("VIEWED"))
    {
        return application::github_file_viewed_state::viewed;
    }
    return std::nullopt;
}

[[nodiscard]] auto bounded_graphql_token(const std::string_view value,
                                         const std::size_t maximum) noexcept
    -> bool
{
    return !value.empty() && value.size() <= maximum
           && std::ranges::all_of(value,
                                  [](const char character)
                                  {
                                      const auto byte =
                                          static_cast<unsigned char>(character);
                                      return byte >= 0x21U && byte <= 0x7eU;
                                  });
}

struct graphql_connection_page
{
    std::uint64_t TotalCount = 0U;
    QJsonArray Nodes;
    std::optional<std::string> NextCursor;
};

[[nodiscard]] auto
parse_graphql_connection(const QJsonValue& value,
                         const github_paged_read_limits& limits)
    -> std::expected<graphql_connection_page, github_paged_read_failure>
{
    if(!value.isObject()
       || !has_exact_keys(value.toObject(),
                          {"nodes", "pageInfo", "totalCount"}))
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    const auto connection = value.toObject();
    if(!exact_integer(connection.value("totalCount"))
       || !connection.value("nodes").isArray()
       || !connection.value("pageInfo").isObject()
       || !has_exact_keys(connection.value("pageInfo").toObject(),
                          {"endCursor", "hasNextPage"}))
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    const auto nodes = connection.value("nodes").toArray();
    const auto page_info = connection.value("pageInfo").toObject();
    if(static_cast<std::size_t>(nodes.size()) > limits.MaximumPageRecords
       || !page_info.value("hasNextPage").isBool())
    {
        return failed(application::github_provider_failure::response_limit);
    }
    const auto total = integer(connection.value("totalCount"));
    const auto has_next = page_info.value("hasNextPage").toBool();
    const auto cursor_value = page_info.value("endCursor");
    std::optional<std::string> next_cursor;
    if(has_next)
    {
        if(static_cast<std::size_t>(nodes.size()) != 100U
           || !cursor_value.isString())
        {
            return failed(
                application::github_provider_failure::malformed_response);
        }
        auto cursor = text(cursor_value);
        if(!bounded_graphql_token(cursor, limits.MaximumCursorBytes)
           || total <= static_cast<std::uint64_t>(nodes.size()))
        {
            return failed(
                application::github_provider_failure::malformed_response);
        }
        next_cursor = std::move(cursor);
    }
    else if((nodes.isEmpty() && !cursor_value.isNull())
            || (!nodes.isEmpty()
                && (!cursor_value.isString()
                    || !bounded_graphql_token(text(cursor_value),
                                              limits.MaximumCursorBytes))))
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    if(total < static_cast<std::uint64_t>(nodes.size()))
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    return graphql_connection_page{
        .TotalCount = total,
        .Nodes = nodes,
        .NextCursor = std::move(next_cursor),
    };
}

[[nodiscard]] auto parse_graphql_comment(const QJsonValue& value)
    -> std::expected<application::github_review_comment_record,
                     github_paged_read_failure>
{
    if(!value.isObject()
       || !has_exact_keys(value.toObject(),
                          {
                              "author",
                              "bodyText",
                              "id",
                              "line",
                              "originalLine",
                              "outdated",
                              "path",
                              "replyTo",
                          }))
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    const auto comment = value.toObject();
    const auto author = optional_graphql_actor(comment.value("author"));
    const auto line = comment.value("line");
    const auto original_line = comment.value("originalLine");
    const auto reply = comment.value("replyTo");
    if(!nonempty_string(comment.value("id"))
       || !comment.value("bodyText").isString()
       || !nonempty_string(comment.value("path"))
       || !comment.value("outdated").isBool() || !author.Valid
       || (!line.isNull() && !valid_line(line))
       || (!original_line.isNull() && !valid_line(original_line))
       || (!reply.isNull()
           && (!reply.isObject() || !has_exact_keys(reply.toObject(), {"id"})
               || !nonempty_string(reply.toObject().value("id")))))
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    std::optional<application::github_comment_id> reply_to;
    if(reply.isObject())
    {
        reply_to =
            application::github_comment_id{text(reply.toObject().value("id"))};
    }
    return application::github_review_comment_record{
        .Comment = application::github_comment_id{text(comment.value("id"))},
        .ReplyTo = std::move(reply_to),
        .Author = author.Actor,
        .Body = text(comment.value("bodyText")),
        .Path = text(comment.value("path")),
        .Line = optional_line(line, original_line),
        .Outdated = comment.value("outdated").toBool(),
        .Deleted = false,
    };
}

struct parsed_graphql_thread
{
    application::github_review_thread_record Record;
    std::uint64_t TotalComments = 0U;
    std::optional<std::string> NextCommentsCursor;
};

[[nodiscard]] auto
parse_graphql_thread(const QJsonValue& value,
                     const application::github_read_shape& shape,
                     const github_paged_read_limits& limits)
    -> std::expected<parsed_graphql_thread, github_paged_read_failure>
{
    if(!value.isObject())
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    std::vector<std::string_view> keys{
        "comments",
        "id",
        "isOutdated",
        "isResolved",
        "line",
        "originalLine",
        "path",
        "subjectType",
    };
    if(shape.ResolvePermission)
    {
        keys.push_back("viewerCanResolve");
    }
    if(shape.UnresolvePermission)
    {
        keys.push_back("viewerCanUnresolve");
    }
    const auto thread = value.toObject();
    if(!has_exact_keys(thread, keys))
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    const auto comments =
        parse_graphql_connection(thread.value("comments"), limits);
    if(!comments.has_value())
    {
        return std::unexpected{comments.error()};
    }
    if(comments->TotalCount > limits.Records.MaximumCommentsPerThread
       || comments->TotalCount > limits.Records.MaximumTotalComments)
    {
        return failed(application::github_provider_failure::response_limit);
    }
    const auto line = thread.value("line");
    const auto original_line = thread.value("originalLine");
    const auto file_subject =
        thread.value("subjectType").toString() == QStringLiteral("FILE");
    const auto line_subject =
        thread.value("subjectType").toString() == QStringLiteral("LINE");
    if(!nonempty_string(thread.value("id"))
       || !thread.value("isOutdated").isBool()
       || !thread.value("isResolved").isBool()
       || !nonempty_string(thread.value("path"))
       || (!file_subject && !line_subject)
       || (!line.isNull() && !valid_line(line))
       || (!original_line.isNull() && !valid_line(original_line))
       || (file_subject && (!line.isNull() || !original_line.isNull()))
       || (line_subject && line.isNull() && original_line.isNull())
       || (shape.ResolvePermission
           && !thread.value("viewerCanResolve").isBool())
       || (shape.UnresolvePermission
           && !thread.value("viewerCanUnresolve").isBool()))
    {
        return failed(application::github_provider_failure::malformed_response);
    }

    application::github_review_thread_record record{
        .Thread = application::github_thread_id{text(thread.value("id"))},
        .Subject = file_subject
                       ? application::github_review_thread_subject::file
                       : application::github_review_thread_subject::line,
        .Path = text(thread.value("path")),
        .Line = optional_line(line, original_line),
        .Resolved = thread.value("isResolved").toBool(),
        .Outdated = thread.value("isOutdated").toBool(),
        .ViewerResolvePermission =
            shape.ResolvePermission
                ? (thread.value("viewerCanResolve").toBool()
                       ? application::github_permission_state::allowed
                       : application::github_permission_state::denied)
                : application::github_permission_state::unavailable,
        .ViewerUnresolvePermission =
            shape.UnresolvePermission
                ? (thread.value("viewerCanUnresolve").toBool()
                       ? application::github_permission_state::allowed
                       : application::github_permission_state::denied)
                : application::github_permission_state::unavailable,
        .Comments = {},
    };
    record.Comments.reserve(static_cast<std::size_t>(comments->TotalCount));
    for(const auto& comment_value : comments->Nodes)
    {
        auto comment = parse_graphql_comment(comment_value);
        if(!comment.has_value())
        {
            return std::unexpected{comment.error()};
        }
        if(comment->Path != record.Path
           || (record.Subject == application::github_review_thread_subject::file
               && comment->Line.has_value())
           || (record.Subject == application::github_review_thread_subject::line
               && !comment->Line.has_value()))
        {
            return failed(
                application::github_provider_failure::malformed_response);
        }
        record.Comments.push_back(std::move(*comment));
    }
    return parsed_graphql_thread{
        .Record = std::move(record),
        .TotalComments = comments->TotalCount,
        .NextCommentsCursor = std::move(comments->NextCursor),
    };
}

[[nodiscard]] auto graphql_repository_and_pull_request(
    const parsed_page& page,
    const application::github_repository_record& repository,
    const application::github_pull_request_record& pull_request,
    QJsonObject* data_output,
    QJsonObject* repository_output,
    QJsonObject* pull_request_output)
    -> std::expected<void, github_paged_read_failure>
{
    if(!singleton_complete(page) || !page.Document.isObject()
       || !graphql_success_envelope(page.Document.object()))
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    const auto data_value = page.Document.object().value("data");
    if(!data_value.isObject()
       || !has_exact_keys(data_value.toObject(), {"repository"}))
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    const auto repository_value = data_value.toObject().value("repository");
    if(!repository_value.isObject()
       || !has_exact_keys(repository_value.toObject(), {"id", "pullRequest"})
       || text(repository_value.toObject().value("id"))
              != repository.Node.value())
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    const auto pull_request_value =
        repository_value.toObject().value("pullRequest");
    if(!pull_request_value.isObject()
       || text(pull_request_value.toObject().value("id"))
              != pull_request.Node.value())
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    *data_output = data_value.toObject();
    *repository_output = repository_value.toObject();
    *pull_request_output = pull_request_value.toObject();
    return {};
}

struct thread_pagination_state
{
    std::uint64_t TotalComments = 0U;
    std::optional<std::string> NextCursor;
    std::unordered_set<std::string> SeenCursors;
};

[[nodiscard]] auto provider_request_valid(
    const validated_github_host_profile& profile,
    const application::github_pull_request_provider_request& request,
    const core::provider_snapshot_id snapshot,
    const github_paged_read_limits& limits) noexcept -> bool
{
    const auto& context = request.Context;
    const auto& authority = request.ProfileAuthority;
    const auto& evidence = request.CapabilityEvidence;
    const auto observation = application::github_capability_probe_observation{
        .Authority = evidence.Authority,
        .ObservedAt = evidence.ObservedAt,
        .Facts = evidence.Facts,
        .SchemaFacts = evidence.SchemaFacts,
    };
    return limits_valid(limits) && snapshot.value() != 0U
           && application::detail::valid_context(context)
           && application::valid(authority) && request.Attempt.valid()
           && request.AccountTransport != nullptr
           && request.Viewer.Account.value() != 0U
           && request.Viewer.Account == context.Account
           && bounded_text(request.Viewer.Node.value(),
                           limits.Records.MaximumIdentityBytes,
                           true)
           && bounded_text(
               request.Viewer.Login, limits.Records.MaximumLoginBytes, true)
           && bounded_text(request.Viewer.DisplayName,
                           limits.Records.MaximumDisplayNameBytes)
           && authority.Profile == context.Profile
           && authority.Profile == profile.profile()
           && authority.ProfileRevision == profile.revision()
           && authority.Host == context.Host && authority.Host == profile.host()
           && authority.Hostname.value() == profile.hostname()
           && authority.ServerContract == profile.server_contract()
           && authority.Authentication == profile.authentication_transport()
           && application::valid(observation)
           && evidence.Authority.Profile == authority.Profile
           && evidence.Authority.ProfileRevision == authority.ProfileRevision
           && evidence.Authority.Host == authority.Host
           && evidence.Authority.Account == context.Account
           && evidence.Authority.ServerContract == authority.ServerContract
           && evidence.Authority.ProbeContractRevision
                  == authority.ProbeContractRevision
           && evidence.Revision == context.Capabilities
           && evidence.ExpiresBefore > evidence.ObservedAt
           && evidence.ExpiresBefore.value() - evidence.ObservedAt.value()
                  <= application::maximum_github_capability_freshness_window
           && evidence.Capabilities
                  == application::projected_capabilities(observation)
           && evidence.ReadShape
                  == application::projected_read_shape(observation);
}

[[nodiscard]] auto
parse_repository(const parsed_page& page,
                 const core::pull_request_request_context& context)
    -> std::expected<application::github_repository_record,
                     github_paged_read_failure>
{
    if(!singleton_complete(page) || !page.Document.isObject()
       || !has_required_keys(page.Document.object(),
                             {
                                 "default_branch",
                                 "full_name",
                                 "id",
                                 "name",
                                 "node_id",
                                 "owner",
                                 "private",
                             }))
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    const auto object = page.Document.object();
    const auto owner = object.value("owner");
    if(!exact_integer(object.value("id"), true)
       || !nonempty_string(object.value("name"))
       || !nonempty_string(object.value("full_name"))
       || !nonempty_string(object.value("node_id"))
       || !object.value("private").isBool()
       || !nonempty_string(object.value("default_branch")) || !owner.isObject()
       || !has_required_keys(owner.toObject(), {"login"})
       || !nonempty_string(owner.toObject().value("login")))
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    const auto raw_owner = text(owner.toObject().value("login"));
    const auto raw_name = text(object.value("name"));
    const auto raw_full_name = text(object.value("full_name"));
    if(!ascii_equal_fold(raw_owner, context.Repository.Owner)
       || !ascii_equal_fold(raw_name, context.Repository.Name)
       || !ascii_equal_fold(raw_full_name, raw_owner + "/" + raw_name))
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    const auto id = core::repository_id{integer(object.value("id"))};
    if(context.ExpectedRepository.has_value()
       && *context.ExpectedRepository != id)
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    return application::github_repository_record{
        .Repository = id,
        .Node = application::github_node_id{text(object.value("node_id"))},
        .Owner = raw_owner,
        .Name = raw_name,
        .DefaultBranch = text(object.value("default_branch")),
        .Private = object.value("private").toBool(),
    };
}

struct parsed_pull_ref
{
    std::string Ref;
    std::string Oid;
    std::optional<core::repository_id> Repository;
    std::optional<std::string> FullName;
};

[[nodiscard]] auto parse_pull_ref(const QJsonValue& value,
                                  const bool repository_required)
    -> std::optional<parsed_pull_ref>
{
    if(!value.isObject()
       || !has_required_keys(value.toObject(), {"ref", "repo", "sha"}))
    {
        return std::nullopt;
    }
    const auto object = value.toObject();
    if(!nonempty_string(object.value("ref"))
       || !hexadecimal_oid(object.value("sha")))
    {
        return std::nullopt;
    }
    parsed_pull_ref result{
        .Ref = text(object.value("ref")),
        .Oid = text(object.value("sha")),
        .Repository = std::nullopt,
        .FullName = std::nullopt,
    };
    const auto repository = object.value("repo");
    if(repository.isNull())
    {
        return repository_required ? std::nullopt
                                   : std::optional{std::move(result)};
    }
    if(!repository.isObject()
       || !has_required_keys(repository.toObject(), {"full_name", "id"})
       || !exact_integer(repository.toObject().value("id"), true)
       || !nonempty_string(repository.toObject().value("full_name")))
    {
        return std::nullopt;
    }
    result.Repository =
        core::repository_id{integer(repository.toObject().value("id"))};
    result.FullName = text(repository.toObject().value("full_name"));
    return result;
}

struct parsed_pull_request
{
    application::github_pull_request_record Record;
    std::size_t ChangedFiles = 0U;
    std::size_t Commits = 0U;
};

[[nodiscard]] auto
parse_pull_request(const parsed_page& page,
                   const core::pull_request_request_context& context,
                   const application::github_repository_record& repository,
                   const application::github_read_limits& limits)
    -> std::expected<parsed_pull_request, github_paged_read_failure>
{
    if(!singleton_complete(page) || !page.Document.isObject()
       || !has_required_keys(page.Document.object(),
                             {
                                 "base",
                                 "body",
                                 "changed_files",
                                 "commits",
                                 "draft",
                                 "head",
                                 "id",
                                 "merged",
                                 "node_id",
                                 "number",
                                 "state",
                                 "title",
                                 "user",
                             }))
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    const auto object = page.Document.object();
    const auto base = parse_pull_ref(object.value("base"), true);
    const auto head = parse_pull_ref(object.value("head"), false);
    const auto author = optional_rest_actor(object.value("user"));
    if(!exact_integer(object.value("id"), true)
       || !exact_integer(object.value("number"), true)
       || integer(object.value("number")) > core::maximum_pull_request_number
       || !exact_integer(object.value("changed_files"))
       || !exact_integer(object.value("commits"), true)
       || !nonempty_string(object.value("node_id"))
       || !nonempty_string(object.value("title"))
       || (!object.value("body").isNull() && !object.value("body").isString())
       || !one_of(object.value("state"), {"open", "closed"})
       || !object.value("draft").isBool() || !object.value("merged").isBool()
       || !base.has_value() || !head.has_value() || !author.Valid
       || (object.value("merged").toBool()
           && object.value("state").toString() != QStringLiteral("closed")))
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    const auto changed_files = integer(object.value("changed_files"));
    const auto commits = integer(object.value("commits"));
    if(changed_files > limits.MaximumFiles || commits > limits.MaximumCommits)
    {
        return failed(application::github_provider_failure::response_limit);
    }
    const auto id = core::pull_request_id{integer(object.value("id"))};
    const auto number = core::pull_request_number{
        static_cast<std::uint32_t>(integer(object.value("number")))};
    const auto repository_full_name = repository.Owner + "/" + repository.Name;
    if(number != context.Number || !base->Repository.has_value()
       || *base->Repository != repository.Repository
       || !base->FullName.has_value()
       || !ascii_equal_fold(*base->FullName, repository_full_name)
       || (context.ExpectedPullRequest.has_value()
           && *context.ExpectedPullRequest != id))
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    return parsed_pull_request{
        .Record =
            application::github_pull_request_record{
                .PullRequest = id,
                .Repository = repository.Repository,
                .Number = number,
                .Node =
                    application::github_node_id{text(object.value("node_id"))},
                .Title = text(object.value("title")),
                .Body = object.value("body").isNull()
                            ? std::string{}
                            : text(object.value("body")),
                .State = pull_request_state(object.value("state").toString(),
                                            object.value("merged").toBool()),
                .Draft = object.value("draft").toBool(),
                .Author = author.Actor,
                .BaseOid = application::commit_oid{base->Oid},
                .HeadOid = application::commit_oid{head->Oid},
                .BaseRef = base->Ref,
                .HeadRef = head->Ref,
            },
        .ChangedFiles = static_cast<std::size_t>(changed_files),
        .Commits = static_cast<std::size_t>(commits),
    };
}

[[nodiscard]] auto parse_file(const QJsonValue& value)
    -> std::expected<application::github_file_record, github_paged_read_failure>
{
    if(!value.isObject())
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    const auto object = value.toObject();
    const auto has_previous = object.contains("previous_filename");
    if(!has_required_keys(object,
                          {"additions", "deletions", "filename", "status"})
       || !nonempty_string(object.value("filename"))
       || !one_of(object.value("status"),
                  {
                      "added",
                      "modified",
                      "removed",
                      "renamed",
                      "copied",
                      "changed",
                      "unchanged",
                  })
       || !exact_integer(object.value("additions"))
       || !exact_integer(object.value("deletions"))
       || (has_previous
           && (!nonempty_string(object.value("previous_filename"))
               || (object.value("status").toString()
                       != QStringLiteral("renamed")
                   && object.value("status").toString()
                          != QStringLiteral("copied")))))
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    std::optional<std::string> previous_path;
    if(has_previous)
    {
        previous_path = text(object.value("previous_filename"));
    }
    return application::github_file_record{
        .Path = text(object.value("filename")),
        .PreviousPath = std::move(previous_path),
        .Change = file_change(object.value("status").toString()),
        .Additions = integer(object.value("additions")),
        .Deletions = integer(object.value("deletions")),
        .ContentKind = application::github_file_content_kind::unknown,
        .ViewedState = application::github_file_viewed_state::unavailable,
    };
}

[[nodiscard]] auto parse_commit(const QJsonValue& value)
    -> std::expected<application::github_commit_record,
                     github_paged_read_failure>
{
    if(!value.isObject()
       || !has_required_keys(value.toObject(),
                             {"author", "commit", "node_id", "sha"}))
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    const auto object = value.toObject();
    const auto author = optional_rest_actor(object.value("author"));
    const auto commit = object.value("commit");
    if(!hexadecimal_oid(object.value("sha"))
       || !nonempty_string(object.value("node_id")) || !author.Valid
       || !commit.isObject()
       || !has_required_keys(commit.toObject(), {"message"})
       || !commit.toObject().value("message").isString())
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    auto headline = text(commit.toObject().value("message"));
    if(const auto newline = headline.find('\n'); newline != std::string::npos)
    {
        headline.resize(newline);
    }
    return application::github_commit_record{
        .Oid = application::commit_oid{text(object.value("sha"))},
        .Node = application::github_node_id{text(object.value("node_id"))},
        .Author = author.Actor,
        .Headline = std::move(headline),
    };
}

[[nodiscard]] auto parse_review(const QJsonValue& value)
    -> std::expected<application::github_review_record,
                     github_paged_read_failure>
{
    if(!value.isObject()
       || !has_required_keys(value.toObject(),
                             {"body",
                              "commit_id",
                              "id",
                              "node_id",
                              "state",
                              "user"}))
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    const auto object = value.toObject();
    const auto author = optional_rest_actor(object.value("user"));
    if(!exact_integer(object.value("id"), true)
       || !nonempty_string(object.value("node_id"))
       || !object.value("body").isString() || !author.Valid
       || !one_of(object.value("state"),
                  {
                      "PENDING",
                      "COMMENTED",
                      "APPROVED",
                      "CHANGES_REQUESTED",
                      "DISMISSED",
                  })
       || (!object.value("commit_id").isNull()
           && !hexadecimal_oid(object.value("commit_id"))))
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    std::optional<application::commit_oid> commit;
    if(!object.value("commit_id").isNull())
    {
        commit = application::commit_oid{text(object.value("commit_id"))};
    }
    return application::github_review_record{
        .DatabaseId = integer(object.value("id")),
        .Review = application::github_review_id{text(object.value("node_id"))},
        .Author = author.Actor,
        .State = review_state(object.value("state").toString()),
        .Commit = std::move(commit),
        .Body = text(object.value("body")),
    };
}

[[nodiscard]] auto parse_issue_comment(const QJsonValue& value)
    -> std::expected<application::github_issue_comment_record,
                     github_paged_read_failure>
{
    if(!value.isObject()
       || !has_required_keys(value.toObject(), {"body", "node_id", "user"}))
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    const auto object = value.toObject();
    const auto author = optional_rest_actor(object.value("user"));
    if(!nonempty_string(object.value("node_id"))
       || !object.value("body").isString() || !author.Valid)
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    return application::github_issue_comment_record{
        .Comment =
            application::github_comment_id{text(object.value("node_id"))},
        .Author = author.Actor,
        .Body = text(object.value("body")),
        .Deleted = false,
    };
}

[[nodiscard]] auto parse_check(const QJsonValue& value,
                               const std::string_view head_oid)
    -> std::expected<application::github_check_record,
                     github_paged_read_failure>
{
    if(!value.isObject()
       || !has_required_keys(value.toObject(),
                             {
                                 "conclusion",
                                 "head_sha",
                                 "name",
                                 "node_id",
                                 "status",
                             }))
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    const auto object = value.toObject();
    const auto completed =
        object.value("status").toString() == QStringLiteral("completed");
    if(!nonempty_string(object.value("node_id"))
       || !nonempty_string(object.value("name"))
       || !hexadecimal_oid(object.value("head_sha"))
       || text(object.value("head_sha")) != head_oid
       || !one_of(object.value("status"),
                  {
                      "queued",
                      "in_progress",
                      "completed",
                      "waiting",
                      "requested",
                      "pending",
                  })
       || (!object.value("conclusion").isNull()
           && !one_of(object.value("conclusion"),
                      {
                          "success",
                          "failure",
                          "neutral",
                          "cancelled",
                          "skipped",
                          "timed_out",
                          "action_required",
                          "stale",
                      }))
       || (completed != !object.value("conclusion").isNull()))
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    return application::github_check_record{
        .Check = application::github_check_id{text(object.value("node_id"))},
        .Name = text(object.value("name")),
        .Status = check_status(object.value("status").toString()),
        .Conclusion = check_conclusion(object.value("conclusion")),
    };
}

[[nodiscard]] auto
parse_search_result(const QJsonValue& value,
                    const application::github_repository_record& repository,
                    const validated_github_host_profile& profile)
    -> std::expected<application::github_search_result_record,
                     github_paged_read_failure>
{
    if(!value.isObject()
       || !has_required_keys(value.toObject(),
                             {"draft",
                              "html_url",
                              "node_id",
                              "number",
                              "pull_request",
                              "repository_url",
                              "state",
                              "title"}))
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    const auto object = value.toObject();
    const auto number = uint32(object.value("number"));
    const auto pull_request = object.value("pull_request");
    if(!number.has_value() || *number == 0U
       || *number > core::maximum_pull_request_number
       || !nonempty_string(object.value("node_id"))
       || !nonempty_string(object.value("title"))
       || !nonempty_string(object.value("repository_url"))
       || !nonempty_string(object.value("html_url")) || !pull_request.isObject()
       || !has_required_keys(pull_request.toObject(), {"url"})
       || !nonempty_string(pull_request.toObject().value("url"))
       || !one_of(object.value("state"), {"open"})
       || !object.value("draft").isBool())
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    const auto expected_repository_url = std::string{profile.rest_root()}
                                         + "/repos/" + repository.Owner + "/"
                                         + repository.Name;
    const auto expected_html_url =
        "https://" + std::string{profile.hostname()} + "/" + repository.Owner
        + "/" + repository.Name + "/pull/" + std::to_string(*number);
    const auto expected_pull_request_url =
        expected_repository_url + "/pulls/" + std::to_string(*number);
    if(text(object.value("repository_url")) != expected_repository_url
       || text(object.value("html_url")) != expected_html_url
       || text(pull_request.toObject().value("url"))
              != expected_pull_request_url)
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    return application::github_search_result_record{
        .Repository = repository.Repository,
        .PullRequestNode =
            application::github_node_id{text(object.value("node_id"))},
        .Number = core::pull_request_number{*number},
        .Title = text(object.value("title")),
        .State = application::github_pull_request_state::open,
        .Draft = object.value("draft").toBool(),
    };
}

template <typename Record, typename Parser>
[[nodiscard]] auto
read_rest_array_collection(production_read_execution& execution,
                           const validated_github_host_profile& profile,
                           semantic_read_request semantic,
                           std::vector<Record>& output,
                           const std::size_t maximum_records,
                           const std::size_t maximum_page_records,
                           Parser&& parser)
    -> std::expected<void, github_paged_read_failure>
{
    while(true)
    {
        auto page = execution.fetch(semantic);
        if(!page.has_value())
        {
            return std::unexpected{page.error()};
        }
        if(!page->Document.isArray())
        {
            return failed(
                application::github_provider_failure::malformed_response);
        }
        const auto records = page->Document.array();
        const auto count = static_cast<std::size_t>(records.size());
        if(count > maximum_page_records || count > maximum_records
           || output.size() > maximum_records - count)
        {
            return failed(application::github_provider_failure::response_limit);
        }
        output.reserve(output.size() + count);
        for(const auto& value : records)
        {
            auto record = parser(value);
            if(!record.has_value())
            {
                return std::unexpected{record.error()};
            }
            output.push_back(std::move(*record));
        }
        const auto repository = execution.bound_repository();
        if(!repository.has_value())
        {
            return failed(
                application::github_provider_failure::malformed_response);
        }
        const auto has_next =
            rest_has_next_page(profile, *repository, *page, count);
        if(!has_next.has_value())
        {
            return std::unexpected{has_next.error()};
        }
        if(!*has_next)
        {
            return {};
        }
        if(semantic.Page == std::numeric_limits<std::uint32_t>::max())
        {
            return failed(application::github_provider_failure::response_limit);
        }
        ++semantic.Page;
    }
}

[[nodiscard]] auto
perform_graphql_projection(production_read_execution& execution,
                           const server_contract server,
                           const core::pull_request_request_context& context,
                           const application::github_read_shape& shape,
                           application::github_pull_request_snapshot& snapshot,
                           const github_paged_read_limits& limits)
    -> std::expected<void, github_paged_read_failure>
{
    if((shape.ResolvePermission || shape.UnresolvePermission)
       && !shape.ReviewThreads)
    {
        return failed(application::github_provider_failure::malformed_response);
    }
    if(!shape.ViewedFileState && !shape.ReviewThreads)
    {
        snapshot.ReviewThreadsAvailability =
            application::github_collection_availability::unavailable;
        return {};
    }

    std::uint32_t sequence = 1U;
    auto initial_semantic = semantic_request_for(
        server, context, read_operation::list_review_threads, sequence);
    initial_semantic.GraphqlPage = github_graphql_page_request{
        .Kind = github_graphql_page_kind::initial,
        .Shape = shape,
        .AfterCursor = {},
        .ThreadId = {},
    };
    auto initial = execution.fetch(initial_semantic);
    if(!initial.has_value())
    {
        return std::unexpected{initial.error()};
    }
    QJsonObject initial_data;
    QJsonObject initial_repository;
    QJsonObject initial_pull_request;
    if(const auto identity =
           graphql_repository_and_pull_request(*initial,
                                               snapshot.Repository,
                                               snapshot.PullRequest,
                                               &initial_data,
                                               &initial_repository,
                                               &initial_pull_request);
       !identity.has_value())
    {
        return std::unexpected{identity.error()};
    }
    std::vector<std::string_view> initial_pull_keys{"id"};
    if(shape.ViewedFileState)
    {
        initial_pull_keys.push_back("files");
    }
    if(shape.ReviewThreads)
    {
        initial_pull_keys.push_back("reviewThreads");
    }
    if(!has_exact_keys(initial_pull_request, initial_pull_keys))
    {
        return failed(application::github_provider_failure::malformed_response);
    }

    if(shape.ViewedFileState)
    {
        auto files = parse_graphql_connection(
            initial_pull_request.value("files"), limits);
        if(!files.has_value())
        {
            return std::unexpected{files.error()};
        }
        if(files->TotalCount > limits.Records.MaximumFiles
           || files->TotalCount != snapshot.Files.size())
        {
            return failed(
                files->TotalCount > limits.Records.MaximumFiles
                    ? application::github_provider_failure::response_limit
                    : application::github_provider_failure::malformed_response);
        }
        std::unordered_map<std::string, application::github_file_viewed_state>
            viewed;
        viewed.reserve(static_cast<std::size_t>(files->TotalCount));
        const auto append_file_nodes = [&viewed](const QJsonArray& nodes)
            -> std::expected<void, github_paged_read_failure>
        {
            for(const auto& value : nodes)
            {
                if(!value.isObject()
                   || !has_exact_keys(value.toObject(),
                                      {"path", "viewerViewedState"})
                   || !nonempty_string(value.toObject().value("path")))
                {
                    return failed(application::github_provider_failure::
                                      malformed_response);
                }
                const auto object = value.toObject();
                const auto state =
                    viewed_state(object.value("viewerViewedState").toString());
                if(!state.has_value()
                   || !viewed.emplace(text(object.value("path")), *state)
                           .second)
                {
                    return failed(application::github_provider_failure::
                                      malformed_response);
                }
            }
            return {};
        };
        if(const auto append = append_file_nodes(files->Nodes);
           !append.has_value())
        {
            return std::unexpected{append.error()};
        }
        std::unordered_set<std::string> seen_cursors;
        auto next_cursor = std::move(files->NextCursor);
        while(next_cursor.has_value())
        {
            if(!seen_cursors.insert(*next_cursor).second)
            {
                return failed(
                    application::github_provider_failure::malformed_response);
            }
            if(sequence == maximum_graphql_sequence)
            {
                return failed(
                    application::github_provider_failure::response_limit);
            }
            ++sequence;
            auto semantic = semantic_request_for(
                server, context, read_operation::list_review_threads, sequence);
            semantic.GraphqlPage = github_graphql_page_request{
                .Kind = github_graphql_page_kind::files,
                .Shape = shape,
                .AfterCursor = *next_cursor,
                .ThreadId = {},
            };
            auto page = execution.fetch(semantic);
            if(!page.has_value())
            {
                return std::unexpected{page.error()};
            }
            QJsonObject data;
            QJsonObject repository;
            QJsonObject pull_request;
            if(const auto identity =
                   graphql_repository_and_pull_request(*page,
                                                       snapshot.Repository,
                                                       snapshot.PullRequest,
                                                       &data,
                                                       &repository,
                                                       &pull_request);
               !identity.has_value())
            {
                return std::unexpected{identity.error()};
            }
            if(!has_exact_keys(pull_request, {"files", "id"}))
            {
                return failed(
                    application::github_provider_failure::malformed_response);
            }
            auto connection =
                parse_graphql_connection(pull_request.value("files"), limits);
            if(!connection.has_value())
            {
                return std::unexpected{connection.error()};
            }
            if(connection->TotalCount != files->TotalCount)
            {
                return failed(
                    application::github_provider_failure::malformed_response);
            }
            if(const auto append = append_file_nodes(connection->Nodes);
               !append.has_value())
            {
                return std::unexpected{append.error()};
            }
            if(viewed.size() > files->TotalCount)
            {
                return failed(
                    application::github_provider_failure::malformed_response);
            }
            next_cursor = std::move(connection->NextCursor);
        }
        if(viewed.size() != files->TotalCount)
        {
            return failed(
                application::github_provider_failure::malformed_response);
        }
        for(auto& file : snapshot.Files)
        {
            const auto found = viewed.find(file.Path);
            if(found == viewed.end())
            {
                return failed(
                    application::github_provider_failure::malformed_response);
            }
            file.ViewedState = found->second;
            viewed.erase(found);
        }
        if(!viewed.empty())
        {
            return failed(
                application::github_provider_failure::malformed_response);
        }
    }

    if(!shape.ReviewThreads)
    {
        snapshot.ReviewThreadsAvailability =
            application::github_collection_availability::unavailable;
        return {};
    }

    auto threads = parse_graphql_connection(
        initial_pull_request.value("reviewThreads"), limits);
    if(!threads.has_value())
    {
        return std::unexpected{threads.error()};
    }
    if(threads->TotalCount > limits.Records.MaximumReviewThreads)
    {
        return failed(application::github_provider_failure::response_limit);
    }
    snapshot.ReviewThreads.reserve(
        static_cast<std::size_t>(threads->TotalCount));
    std::vector<thread_pagination_state> pagination;
    pagination.reserve(static_cast<std::size_t>(threads->TotalCount));
    std::unordered_set<std::string> thread_ids;
    thread_ids.reserve(static_cast<std::size_t>(threads->TotalCount));
    std::size_t total_comments = 0U;
    const auto append_thread_nodes = [&](const QJsonArray& nodes)
        -> std::expected<void, github_paged_read_failure>
    {
        for(const auto& value : nodes)
        {
            auto parsed = parse_graphql_thread(value, shape, limits);
            if(!parsed.has_value())
            {
                return std::unexpected{parsed.error()};
            }
            const auto id = std::string{parsed->Record.Thread.value()};
            if(!thread_ids.insert(id).second)
            {
                return failed(
                    application::github_provider_failure::malformed_response);
            }
            if(parsed->TotalComments > limits.Records.MaximumTotalComments
               || total_comments
                      > limits.Records.MaximumTotalComments
                            - static_cast<std::size_t>(parsed->TotalComments))
            {
                return failed(
                    application::github_provider_failure::response_limit);
            }
            total_comments += static_cast<std::size_t>(parsed->TotalComments);
            thread_pagination_state state{
                .TotalComments = parsed->TotalComments,
                .NextCursor = std::move(parsed->NextCommentsCursor),
                .SeenCursors = {},
            };
            snapshot.ReviewThreads.push_back(std::move(parsed->Record));
            pagination.push_back(std::move(state));
        }
        return {};
    };
    if(const auto append = append_thread_nodes(threads->Nodes);
       !append.has_value())
    {
        return std::unexpected{append.error()};
    }

    std::unordered_set<std::string> thread_cursors;
    auto next_thread_cursor = std::move(threads->NextCursor);
    while(next_thread_cursor.has_value())
    {
        if(!thread_cursors.insert(*next_thread_cursor).second)
        {
            return failed(
                application::github_provider_failure::malformed_response);
        }
        if(sequence == maximum_graphql_sequence)
        {
            return failed(application::github_provider_failure::response_limit);
        }
        ++sequence;
        auto semantic = semantic_request_for(
            server, context, read_operation::list_review_threads, sequence);
        semantic.GraphqlPage = github_graphql_page_request{
            .Kind = github_graphql_page_kind::threads,
            .Shape = shape,
            .AfterCursor = *next_thread_cursor,
            .ThreadId = {},
        };
        auto page = execution.fetch(semantic);
        if(!page.has_value())
        {
            return std::unexpected{page.error()};
        }
        QJsonObject data;
        QJsonObject repository;
        QJsonObject pull_request;
        if(const auto identity =
               graphql_repository_and_pull_request(*page,
                                                   snapshot.Repository,
                                                   snapshot.PullRequest,
                                                   &data,
                                                   &repository,
                                                   &pull_request);
           !identity.has_value())
        {
            return std::unexpected{identity.error()};
        }
        if(!has_exact_keys(pull_request, {"id", "reviewThreads"}))
        {
            return failed(
                application::github_provider_failure::malformed_response);
        }
        auto connection = parse_graphql_connection(
            pull_request.value("reviewThreads"), limits);
        if(!connection.has_value())
        {
            return std::unexpected{connection.error()};
        }
        if(connection->TotalCount != threads->TotalCount)
        {
            return failed(
                application::github_provider_failure::malformed_response);
        }
        if(const auto append = append_thread_nodes(connection->Nodes);
           !append.has_value())
        {
            return std::unexpected{append.error()};
        }
        if(snapshot.ReviewThreads.size() > threads->TotalCount)
        {
            return failed(
                application::github_provider_failure::malformed_response);
        }
        next_thread_cursor = std::move(connection->NextCursor);
    }
    if(snapshot.ReviewThreads.size() != threads->TotalCount)
    {
        return failed(application::github_provider_failure::malformed_response);
    }

    for(std::size_t index = 0U; index < pagination.size(); ++index)
    {
        auto& state = pagination[index];
        auto& thread = snapshot.ReviewThreads[index];
        while(state.NextCursor.has_value())
        {
            if(!state.SeenCursors.insert(*state.NextCursor).second)
            {
                return failed(
                    application::github_provider_failure::malformed_response);
            }
            if(sequence == maximum_graphql_sequence)
            {
                return failed(
                    application::github_provider_failure::response_limit);
            }
            ++sequence;
            auto semantic = semantic_request_for(
                server, context, read_operation::list_review_threads, sequence);
            semantic.GraphqlPage = github_graphql_page_request{
                .Kind = github_graphql_page_kind::comments,
                .Shape = shape,
                .AfterCursor = *state.NextCursor,
                .ThreadId = std::string{thread.Thread.value()},
            };
            auto page = execution.fetch(semantic);
            if(!page.has_value())
            {
                return std::unexpected{page.error()};
            }
            if(!singleton_complete(*page) || !page->Document.isObject()
               || !graphql_success_envelope(page->Document.object()))
            {
                return failed(
                    application::github_provider_failure::malformed_response);
            }
            const auto data_value = page->Document.object().value("data");
            if(!data_value.isObject()
               || !has_exact_keys(data_value.toObject(),
                                  {"node", "repository"}))
            {
                return failed(
                    application::github_provider_failure::malformed_response);
            }
            const auto data = data_value.toObject();
            const auto repository_value = data.value("repository");
            if(!repository_value.isObject()
               || !has_exact_keys(repository_value.toObject(),
                                  {"id", "pullRequest"})
               || text(repository_value.toObject().value("id"))
                      != snapshot.Repository.Node.value())
            {
                return failed(
                    application::github_provider_failure::malformed_response);
            }
            const auto pull_request_value =
                repository_value.toObject().value("pullRequest");
            if(!pull_request_value.isObject()
               || !has_exact_keys(pull_request_value.toObject(), {"id"})
               || text(pull_request_value.toObject().value("id"))
                      != snapshot.PullRequest.Node.value())
            {
                return failed(
                    application::github_provider_failure::malformed_response);
            }
            const auto node_value = data.value("node");
            if(!node_value.isObject()
               || !has_exact_keys(node_value.toObject(), {"comments", "id"})
               || text(node_value.toObject().value("id"))
                      != thread.Thread.value())
            {
                return failed(
                    application::github_provider_failure::malformed_response);
            }
            auto connection = parse_graphql_connection(
                node_value.toObject().value("comments"), limits);
            if(!connection.has_value())
            {
                return std::unexpected{connection.error()};
            }
            if(connection->TotalCount != state.TotalComments)
            {
                return failed(
                    application::github_provider_failure::malformed_response);
            }
            for(const auto& value : connection->Nodes)
            {
                auto comment = parse_graphql_comment(value);
                if(!comment.has_value())
                {
                    return std::unexpected{comment.error()};
                }
                if(comment->Path != thread.Path
                   || (thread.Subject
                           == application::github_review_thread_subject::file
                       && comment->Line.has_value())
                   || (thread.Subject
                           == application::github_review_thread_subject::line
                       && !comment->Line.has_value()))
                {
                    return failed(application::github_provider_failure::
                                      malformed_response);
                }
                thread.Comments.push_back(std::move(*comment));
            }
            if(thread.Comments.size() > state.TotalComments)
            {
                return failed(
                    application::github_provider_failure::malformed_response);
            }
            state.NextCursor = std::move(connection->NextCursor);
        }
        if(thread.Comments.size() != state.TotalComments)
        {
            return failed(
                application::github_provider_failure::malformed_response);
        }
    }
    snapshot.ReviewThreadsAvailability =
        application::github_collection_availability::available;
    return {};
}

[[nodiscard]] auto
read_checks(production_read_execution& execution,
            const validated_github_host_profile& profile,
            semantic_read_request semantic,
            const std::string_view head_oid,
            std::vector<application::github_check_record>& output,
            const github_paged_read_limits& limits)
    -> std::expected<void, github_paged_read_failure>
{
    std::optional<std::uint64_t> total;
    while(true)
    {
        auto page = execution.fetch(semantic);
        if(!page.has_value())
        {
            return std::unexpected{page.error()};
        }
        if(!page->Document.isObject()
           || !has_required_keys(page->Document.object(),
                                 {"check_runs", "total_count"}))
        {
            return failed(
                application::github_provider_failure::malformed_response);
        }
        const auto object = page->Document.object();
        if(!exact_integer(object.value("total_count"))
           || !object.value("check_runs").isArray())
        {
            return failed(
                application::github_provider_failure::malformed_response);
        }
        const auto observed_total = integer(object.value("total_count"));
        if(observed_total > limits.Records.MaximumChecks)
        {
            return failed(application::github_provider_failure::response_limit);
        }
        if(total.has_value() && *total != observed_total)
        {
            return failed(
                application::github_provider_failure::malformed_response);
        }
        total = observed_total;
        const auto records = object.value("check_runs").toArray();
        const auto count = static_cast<std::size_t>(records.size());
        if(count > limits.MaximumPageRecords
           || count > limits.Records.MaximumChecks
           || output.size() > limits.Records.MaximumChecks - count)
        {
            return failed(application::github_provider_failure::response_limit);
        }
        output.reserve(output.size() + count);
        for(const auto& value : records)
        {
            auto check = parse_check(value, head_oid);
            if(!check.has_value())
            {
                return std::unexpected{check.error()};
            }
            output.push_back(std::move(*check));
        }
        if(output.size() > observed_total)
        {
            return failed(
                application::github_provider_failure::malformed_response);
        }
        const auto repository = execution.bound_repository();
        if(!repository.has_value())
        {
            return failed(
                application::github_provider_failure::malformed_response);
        }
        const auto has_next =
            rest_has_next_page(profile, *repository, *page, count);
        if(!has_next.has_value())
        {
            return std::unexpected{has_next.error()};
        }
        if(!*has_next)
        {
            if(output.size() != observed_total)
            {
                return failed(
                    application::github_provider_failure::malformed_response);
            }
            return {};
        }
        if(semantic.Page == std::numeric_limits<std::uint32_t>::max())
        {
            return failed(application::github_provider_failure::response_limit);
        }
        ++semantic.Page;
    }
}

[[nodiscard]] auto read_search_results(
    production_read_execution& execution,
    const validated_github_host_profile& profile,
    semantic_read_request semantic,
    const application::github_repository_record& repository,
    std::vector<application::github_search_result_record>& output,
    const github_paged_read_limits& limits)
    -> std::expected<void, github_paged_read_failure>
{
    std::optional<std::uint64_t> total;
    while(true)
    {
        auto page = execution.fetch(semantic);
        if(!page.has_value())
        {
            return std::unexpected{page.error()};
        }
        if(!page->Document.isObject()
           || !has_required_keys(
               page->Document.object(),
               {"incomplete_results", "items", "total_count"}))
        {
            return failed(
                application::github_provider_failure::malformed_response);
        }
        const auto object = page->Document.object();
        if(!exact_integer(object.value("total_count"))
           || !object.value("items").isArray()
           || !object.value("incomplete_results").isBool()
           || object.value("incomplete_results").toBool())
        {
            return failed(
                application::github_provider_failure::malformed_response);
        }
        const auto observed_total = integer(object.value("total_count"));
        if(observed_total > limits.Records.MaximumSearchResults)
        {
            return failed(application::github_provider_failure::response_limit);
        }
        if(total.has_value() && *total != observed_total)
        {
            return failed(
                application::github_provider_failure::malformed_response);
        }
        total = observed_total;
        const auto records = object.value("items").toArray();
        const auto count = static_cast<std::size_t>(records.size());
        if(count > limits.MaximumPageRecords
           || count > limits.Records.MaximumSearchResults
           || output.size() > limits.Records.MaximumSearchResults - count)
        {
            return failed(application::github_provider_failure::response_limit);
        }
        output.reserve(output.size() + count);
        for(const auto& value : records)
        {
            auto result = parse_search_result(value, repository, profile);
            if(!result.has_value())
            {
                return std::unexpected{result.error()};
            }
            output.push_back(std::move(*result));
        }
        if(output.size() > observed_total)
        {
            return failed(
                application::github_provider_failure::malformed_response);
        }
        const auto bound_repository = execution.bound_repository();
        if(!bound_repository.has_value()
           || *bound_repository != repository.Repository)
        {
            return failed(
                application::github_provider_failure::malformed_response);
        }
        const auto has_next =
            rest_has_next_page(profile, *bound_repository, *page, count);
        if(!has_next.has_value())
        {
            return std::unexpected{has_next.error()};
        }
        if(!*has_next)
        {
            if(output.size() != observed_total)
            {
                return failed(
                    application::github_provider_failure::malformed_response);
            }
            return {};
        }
        if(semantic.Page == std::numeric_limits<std::uint32_t>::max())
        {
            return failed(application::github_provider_failure::response_limit);
        }
        ++semantic.Page;
    }
}

[[nodiscard]] auto validation_failure(
    const application::github_read_validation_error error) noexcept
    -> application::github_provider_failure
{
    switch(error)
    {
    case application::github_read_validation_error::text_too_long:
    case application::github_read_validation_error::collection_too_large:
    case application::github_read_validation_error::allocation_limit:
    case application::github_read_validation_error::total_text_too_large:
        return application::github_provider_failure::response_limit;
    case application::github_read_validation_error::none:
    case application::github_read_validation_error::invalid_context:
    case application::github_read_validation_error::invalid_limits:
    case application::github_read_validation_error::missing_identity:
    case application::github_read_validation_error::identity_mismatch:
    case application::github_read_validation_error::missing_required_text:
    case application::github_read_validation_error::invalid_text:
    case application::github_read_validation_error::invalid_repository:
    case application::github_read_validation_error::invalid_pull_request_number:
    case application::github_read_validation_error::invalid_commit_oid:
    case application::github_read_validation_error::invalid_line:
    case application::github_read_validation_error::invalid_enum:
    case application::github_read_validation_error::invalid_capability_revision:
    case application::github_read_validation_error::invalid_rate_limit:
    case application::github_read_validation_error::duplicate_identity:
    case application::github_read_validation_error::invalid_relationship:
        return application::github_provider_failure::malformed_response;
    }
    return application::github_provider_failure::malformed_response;
}
}

auto parse_github_response_metadata(const wire_response& response) noexcept
    -> github_response_metadata_result
{
    try
    {
        return parse_metadata_impl(response);
    }
    catch(const std::bad_alloc&)
    {
        return std::unexpected{provider_error(
            application::github_provider_failure::response_limit)};
    }
    catch(...)
    {
        return std::unexpected{provider_error(
            application::github_provider_failure::malformed_response)};
    }
}

auto github_error_body_is_secondary_rate_limit(
    const std::string_view body, const std::size_t maximum_bytes) noexcept
    -> bool
{
    if(body.empty() || body.size() > maximum_bytes)
    {
        return false;
    }
    try
    {
        const auto preflight =
            json_structural_preflight{body, maximum_bytes}.run();
        if(preflight != json_preflight_result::valid)
        {
            return false;
        }
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(
            QByteArray{
                body.data(),
                static_cast<qsizetype>(body.size()),
            },
            &error);
        return error.error == QJsonParseError::NoError
               && secondary_rate_limit_message(&document);
    }
    catch(...)
    {
        return false;
    }
}

auto github_graphql_error_body_is_rate_limited(
    const std::string_view body, const std::size_t maximum_bytes) noexcept
    -> bool
{
    if(body.empty() || body.size() > maximum_bytes)
    {
        return false;
    }
    try
    {
        const auto preflight =
            json_structural_preflight{body, maximum_bytes}.run();
        if(preflight != json_preflight_result::valid)
        {
            return false;
        }
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(
            QByteArray{
                body.data(),
                static_cast<qsizetype>(body.size()),
            },
            &error);
        return error.error == QJsonParseError::NoError && document.isObject()
               && graphql_error_rate_limited(
                   document.object().value(QStringLiteral("errors")));
    }
    catch(...)
    {
        return false;
    }
}

auto normalize_observed_github_viewer(
    const wire_response& response,
    const server_contract server,
    const github_paged_read_limits& limits) noexcept
    -> observed_github_viewer_result
{
    try
    {
        if(!limits_valid(limits))
        {
            return std::unexpected{provider_error(
                application::github_provider_failure::response_limit)};
        }
        const auto bytes = response_byte_count(response);
        if(!bytes.has_value() || *bytes > limits.MaximumPageBytes
           || *bytes > limits.MaximumAggregateResponseBytes)
        {
            return std::unexpected{provider_error(
                application::github_provider_failure::response_limit)};
        }
        const auto metadata = parse_github_response_metadata(response);
        if(!metadata.has_value())
        {
            return std::unexpected{metadata.error()};
        }
        if((!metadata->RateLimit.has_value()
            && server == server_contract::github_com)
           || (metadata->RateLimit.has_value()
               && metadata->RateLimit->Resource
                      != application::github_rate_limit_resource::core))
        {
            return std::unexpected{provider_error(
                application::github_provider_failure::malformed_response)};
        }
        auto rate_limits = application::github_rate_limit_state{};
        if(metadata->RateLimit.has_value())
        {
            static_cast<void>(
                application::observe(rate_limits, *metadata->RateLimit));
        }
        const auto metadata_error =
            [&rate_limits](const application::github_provider_failure failure)
        { return provider_error(failure, std::nullopt, rate_limits); };
        if(response.Status != 200)
        {
            return std::unexpected{status_failure(
                response.Status,
                *metadata,
                (response.Status == 403 || response.Status == 429)
                    && github_error_body_is_secondary_rate_limit(
                        response.Body, limits.MaximumPageBytes))};
        }
        if(metadata->RetryAfterSeconds.has_value() || metadata->Link.has_value()
           || response.Completeness != page_completeness::complete
           || response.Body.empty())
        {
            return std::unexpected{metadata_error(
                application::github_provider_failure::malformed_response)};
        }
        const auto preflight =
            json_structural_preflight{response.Body, limits.MaximumPageBytes}
                .run();
        if(preflight == json_preflight_result::structural_limit)
        {
            return std::unexpected{metadata_error(
                application::github_provider_failure::response_limit)};
        }
        if(preflight != json_preflight_result::valid)
        {
            return std::unexpected{metadata_error(
                application::github_provider_failure::malformed_response)};
        }
        QJsonParseError parse_error;
        const auto document = QJsonDocument::fromJson(
            QByteArray{
                response.Body.data(),
                static_cast<qsizetype>(response.Body.size()),
            },
            &parse_error);
        if(parse_error.error != QJsonParseError::NoError || !document.isObject()
           || !has_required_keys(document.object(),
                                 {"id", "login", "name", "node_id"}))
        {
            return std::unexpected{metadata_error(
                application::github_provider_failure::malformed_response)};
        }
        const auto object = document.object();
        if(!exact_integer(object.value("id"), true)
           || !nonempty_string(object.value("login"))
           || !nonempty_string(object.value("node_id"))
           || (!object.value("name").isNull()
               && !object.value("name").isString()))
        {
            return std::unexpected{metadata_error(
                application::github_provider_failure::malformed_response)};
        }
        const auto node = text(object.value("node_id"));
        const auto login = text(object.value("login"));
        const auto name = object.value("name").isNull()
                              ? std::string{}
                              : text(object.value("name"));
        if(!bounded_text(node, limits.Records.MaximumIdentityBytes, true)
           || !bounded_text(login, limits.Records.MaximumLoginBytes, true)
           || !bounded_text(name, limits.Records.MaximumDisplayNameBytes))
        {
            return std::unexpected{metadata_error(
                application::github_provider_failure::response_limit)};
        }
        return observed_github_viewer{
            .Viewer =
                application::github_viewer_record{
                    .Account =
                        core::github_account_id{integer(object.value("id"))},
                    .Node = application::github_actor_id{node},
                    .Login = login,
                    .DisplayName = name,
                },
            .RateLimits = std::move(rate_limits),
        };
    }
    catch(const std::bad_alloc&)
    {
        return std::unexpected{provider_error(
            application::github_provider_failure::response_limit)};
    }
    catch(...)
    {
        return std::unexpected{provider_error(
            application::github_provider_failure::malformed_response)};
    }
}

auto perform_paged_github_read(
    github_wire_transport_port& transport,
    const validated_github_host_profile& profile,
    const application::github_pull_request_provider_request& request,
    const core::provider_snapshot_id snapshot_id,
    const std::stop_token stop,
    const github_paged_read_limits& limits) noexcept -> github_paged_read_result
{
    auto observed_rate_limits = application::github_rate_limit_state{};
    try
    {
        if(!provider_request_valid(profile, request, snapshot_id, limits))
        {
            return failed(
                application::github_provider_failure::malformed_response);
        }
        if(stop.stop_requested())
        {
            return cancelled();
        }
        const auto server = server_contract_for(profile);
        if(!server.has_value())
        {
            return failed(
                application::github_provider_failure::malformed_response);
        }

        production_read_execution execution{transport,
                                            profile,
                                            request.Context,
                                            limits,
                                            stop,
                                            observed_rate_limits};

        auto repository_page = execution.fetch(semantic_request_for(
            *server, request.Context, read_operation::get_repository));
        if(!repository_page.has_value())
        {
            return std::unexpected{repository_page.error()};
        }
        auto repository = parse_repository(*repository_page, request.Context);
        if(!repository.has_value())
        {
            return std::unexpected{execution.enrich(repository.error())};
        }
        if(!execution.bind_repository(repository->Repository))
        {
            return execution.terminal_failure(
                application::github_provider_failure::malformed_response);
        }

        auto pull_request_page = execution.fetch(semantic_request_for(
            *server, request.Context, read_operation::get_pull_request));
        if(!pull_request_page.has_value())
        {
            return std::unexpected{pull_request_page.error()};
        }
        auto pull_request = parse_pull_request(
            *pull_request_page, request.Context, *repository, limits.Records);
        if(!pull_request.has_value())
        {
            return std::unexpected{execution.enrich(pull_request.error())};
        }

        const auto expected_files = pull_request->ChangedFiles;
        const auto expected_commits = pull_request->Commits;
        application::github_pull_request_snapshot result{
            .Profile = request.ProfileAuthority.Profile,
            .Host = request.ProfileAuthority.Host,
            .Account = request.Viewer.Account,
            .Viewer =
                application::github_actor_record{
                    .Actor = request.Viewer.Node,
                    .Login = request.Viewer.Login,
                },
            .Snapshot = snapshot_id,
            .Repository = std::move(*repository),
            .PullRequest = std::move(pull_request->Record),
            .Files = {},
            .Commits = {},
            .Reviews = {},
            .ReviewThreadsAvailability =
                application::github_collection_availability::unavailable,
            .ReviewThreads = {},
            .IssueComments = {},
            .Checks = {},
            .SearchResults = {},
            .CapabilityEvidence =
                application::github_capability_evidence{
                    .Capabilities = request.CapabilityEvidence.Capabilities,
                    .Revision = request.CapabilityEvidence.Revision,
                    .ReadShape = request.CapabilityEvidence.ReadShape,
                    .ContractVersion = 1U,
                },
            .RateLimits = {},
        };

        if(const auto files = read_rest_array_collection(
               execution,
               profile,
               semantic_request_for(*server,
                                    request.Context,
                                    read_operation::list_pull_request_files),
               result.Files,
               limits.Records.MaximumFiles,
               limits.MaximumPageRecords,
               [](const QJsonValue& value) { return parse_file(value); });
           !files.has_value())
        {
            return std::unexpected{execution.enrich(files.error())};
        }
        if(result.Files.size() != expected_files)
        {
            return execution.terminal_failure(
                application::github_provider_failure::malformed_response);
        }

        if(const auto commits = read_rest_array_collection(
               execution,
               profile,
               semantic_request_for(*server,
                                    request.Context,
                                    read_operation::list_pull_request_commits),
               result.Commits,
               limits.Records.MaximumCommits,
               limits.MaximumPageRecords,
               [](const QJsonValue& value) { return parse_commit(value); });
           !commits.has_value())
        {
            return std::unexpected{execution.enrich(commits.error())};
        }
        if(result.Commits.size() != expected_commits || result.Commits.empty()
           || result.Commits.back().Oid != result.PullRequest.HeadOid)
        {
            return execution.terminal_failure(
                application::github_provider_failure::malformed_response);
        }

        if(const auto reviews = read_rest_array_collection(
               execution,
               profile,
               semantic_request_for(
                   *server, request.Context, read_operation::list_reviews),
               result.Reviews,
               limits.Records.MaximumReviews,
               limits.MaximumPageRecords,
               [](const QJsonValue& value) { return parse_review(value); });
           !reviews.has_value())
        {
            return std::unexpected{execution.enrich(reviews.error())};
        }

        if(const auto graphql =
               perform_graphql_projection(execution,
                                          *server,
                                          request.Context,
                                          request.CapabilityEvidence.ReadShape,
                                          result,
                                          limits);
           !graphql.has_value())
        {
            return std::unexpected{execution.enrich(graphql.error())};
        }

        if(const auto comments = read_rest_array_collection(
               execution,
               profile,
               semantic_request_for(*server,
                                    request.Context,
                                    read_operation::list_issue_comments),
               result.IssueComments,
               limits.Records.MaximumIssueComments,
               limits.MaximumPageRecords,
               [](const QJsonValue& value)
               { return parse_issue_comment(value); });
           !comments.has_value())
        {
            return std::unexpected{execution.enrich(comments.error())};
        }

        auto checks_semantic = semantic_request_for(
            *server, request.Context, read_operation::list_checks);
        checks_semantic.HeadOid =
            std::string{result.PullRequest.HeadOid.value()};
        if(const auto checks = read_checks(execution,
                                           profile,
                                           std::move(checks_semantic),
                                           result.PullRequest.HeadOid.value(),
                                           result.Checks,
                                           limits);
           !checks.has_value())
        {
            return std::unexpected{execution.enrich(checks.error())};
        }

        auto search_semantic = semantic_request_for(
            *server, request.Context, read_operation::search_pull_requests);
        search_semantic.SearchTerms = "is:open";
        if(const auto search = read_search_results(execution,
                                                   profile,
                                                   std::move(search_semantic),
                                                   result.Repository,
                                                   result.SearchResults,
                                                   limits);
           !search.has_value())
        {
            return std::unexpected{execution.enrich(search.error())};
        }

        if(stop.stop_requested())
        {
            return cancelled();
        }
        result.RateLimits = execution.rate_limits();
        const auto validation =
            application::validate(result, request.Context, limits.Records);
        if(!validation.valid())
        {
            return execution.terminal_failure(
                validation_failure(validation.Error));
        }
        return result;
    }
    catch(const std::bad_alloc&)
    {
        return failed(application::github_provider_failure::response_limit,
                      std::nullopt,
                      observed_rate_limits);
    }
    catch(...)
    {
        return failed(application::github_provider_failure::malformed_response,
                      std::nullopt,
                      observed_rate_limits);
    }
}
}
