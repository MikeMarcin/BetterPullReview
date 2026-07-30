#include "github_capability_probe.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>
#include <QString>

#include <algorithm>
#include <array>
#include <cctype>
#include <initializer_list>
#include <limits>
#include <new>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>

namespace bpr::adapters::github::detail
{
namespace
{
constexpr std::string_view graphql_document =
    "query BetterPullReviewCapabilityProbeV1{"
    "schemaRoot:__schema{mutationType{...FieldType}}"
    "pullRequest:__type(name:\"PullRequest\"){...FieldType}"
    "threadConnection:__type(name:\"PullRequestReviewThreadConnection\")"
    "{...FieldType}"
    "reviewThread:__type(name:\"PullRequestReviewThread\"){...FieldType}"
    "commentConnection:__type(name:\"PullRequestReviewCommentConnection\")"
    "{...FieldType}"
    "reviewComment:__type(name:\"PullRequestReviewComment\"){...FieldType}"
    "changedFileConnection:__type(name:\"PullRequestChangedFileConnection\")"
    "{...FieldType}"
    "changedFile:__type(name:\"PullRequestChangedFile\"){...FieldType}"
    "pageInfo:__type(name:\"PageInfo\"){...FieldType}"
    "actor:__type(name:\"Actor\"){...FieldType}"
    "node:__type(name:\"Node\"){...FieldType}"
    "threadSubject:__type(name:\"PullRequestReviewThreadSubjectType\")"
    "{...EnumType}"
    "fileViewedState:__type(name:\"FileViewedState\"){...EnumType}"
    "markInput:__type(name:\"MarkFileAsViewedInput\"){...InputType}"
    "markPayload:__type(name:\"MarkFileAsViewedPayload\"){...FieldType}"
    "unmarkInput:__type(name:\"UnmarkFileAsViewedInput\"){...InputType}"
    "unmarkPayload:__type(name:\"UnmarkFileAsViewedPayload\")"
    "{...FieldType}"
    "resolveInput:__type(name:\"ResolveReviewThreadInput\")"
    "{...InputType}"
    "resolvePayload:__type(name:\"ResolveReviewThreadPayload\")"
    "{...FieldType}"
    "unresolveInput:__type(name:\"UnresolveReviewThreadInput\")"
    "{...InputType}"
    "unresolvePayload:__type(name:\"UnresolveReviewThreadPayload\")"
    "{...FieldType}"
    "}"
    "fragment TypeRef on __Type{kind name ofType{kind name}}"
    "fragment FieldType on __Type{kind name "
    "fields(includeDeprecated:true){name args{name defaultValue "
    "type{...TypeRef}} type{...TypeRef}}}"
    "fragment InputType on __Type{kind name inputFields{name defaultValue "
    "type{...TypeRef}}}"
    "fragment EnumType on __Type{kind name "
    "enumValues(includeDeprecated:true){name}}";

constexpr capability_probe_limits hard_limits{};

[[nodiscard]] constexpr auto graphql_name_start(const char value) noexcept
    -> bool
{
    return (value >= 'A' && value <= 'Z')
        || (value >= 'a' && value <= 'z') || value == '_';
}

[[nodiscard]] constexpr auto graphql_name_continue(const char value) noexcept
    -> bool
{
    return graphql_name_start(value) || (value >= '0' && value <= '9');
}

[[nodiscard]] constexpr auto graphql_whitespace(const char value) noexcept
    -> bool
{
    return value == ' ' || value == '\t' || value == '\r'
        || value == '\n' || value == ',';
}

[[nodiscard]] constexpr auto query_only_graphql_document(
    const std::string_view document) noexcept -> bool
{
    std::size_t position = 0U;
    std::size_t brace_depth = 0U;
    std::size_t parenthesis_depth = 0U;
    std::size_t bracket_depth = 0U;
    std::size_t query_operations = 0U;
    bool query_has_selection = false;

    while(position < document.size())
    {
        const auto value = document[position];
        if(graphql_whitespace(value))
        {
            ++position;
            continue;
        }
        if(value == '#')
        {
            while(
                position < document.size()
                && document[position] != '\r'
                && document[position] != '\n')
            {
                ++position;
            }
            continue;
        }
        if(value == '"')
        {
            const bool block =
                document.size() - position >= 3U
                && document.substr(position, 3U) == "\"\"\"";
            position += block ? 3U : 1U;
            bool closed = false;
            while(position < document.size())
            {
                if(
                    block && document.size() - position >= 3U
                    && document.substr(position, 3U) == "\"\"\"")
                {
                    position += 3U;
                    closed = true;
                    break;
                }
                const auto character = document[position++];
                if(!block && character == '"')
                {
                    closed = true;
                    break;
                }
                if(
                    !block && character == '\\'
                    && position < document.size())
                {
                    ++position;
                }
                else if(
                    !block
                    && (character == '\r' || character == '\n'))
                {
                    return false;
                }
            }
            if(!closed)
            {
                return false;
            }
            continue;
        }
        if(graphql_name_start(value))
        {
            const auto start = position++;
            while(
                position < document.size()
                && graphql_name_continue(document[position]))
            {
                ++position;
            }
            if(
                brace_depth == 0U && parenthesis_depth == 0U
                && bracket_depth == 0U)
            {
                const auto token =
                    document.substr(start, position - start);
                if(token == "mutation" || token == "subscription")
                {
                    return false;
                }
                if(token == "query")
                {
                    ++query_operations;
                }
            }
            continue;
        }

        switch(value)
        {
        case '{':
            if(
                brace_depth == 0U && query_operations == 1U
                && parenthesis_depth == 0U && bracket_depth == 0U)
            {
                query_has_selection = true;
            }
            ++brace_depth;
            break;
        case '}':
            if(brace_depth == 0U)
            {
                return false;
            }
            --brace_depth;
            break;
        case '(':
            ++parenthesis_depth;
            break;
        case ')':
            if(parenthesis_depth == 0U)
            {
                return false;
            }
            --parenthesis_depth;
            break;
        case '[':
            ++bracket_depth;
            break;
        case ']':
            if(bracket_depth == 0U)
            {
                return false;
            }
            --bracket_depth;
            break;
        default:
            break;
        }
        ++position;
    }

    return query_operations == 1U && query_has_selection
        && brace_depth == 0U && parenthesis_depth == 0U
        && bracket_depth == 0U;
}

static_assert(query_only_graphql_document(graphql_document));

constexpr std::array<std::string_view, 21> response_aliases{
    "schemaRoot",
    "pullRequest",
    "threadConnection",
    "reviewThread",
    "commentConnection",
    "reviewComment",
    "changedFileConnection",
    "changedFile",
    "pageInfo",
    "actor",
    "node",
    "threadSubject",
    "fileViewedState",
    "markInput",
    "markPayload",
    "unmarkInput",
    "unmarkPayload",
    "resolveInput",
    "resolvePayload",
    "unresolveInput",
    "unresolvePayload",
};

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
    json_structural_preflight(
        const std::string_view input,
        const capability_probe_limits& limits) noexcept
        : Input{input},
          Limits{limits}
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
        return Position == Input.size()
            ? json_preflight_result::valid
            : json_preflight_result::malformed;
    }

private:
    [[nodiscard]] auto value(const std::size_t depth) noexcept
        -> json_preflight_result
    {
        if(Values == Limits.MaximumTotalValues)
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
            if(string(nullptr, false))
            {
                return json_preflight_result::valid;
            }
            return StringLimitExceeded
                ? json_preflight_result::structural_limit
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
        if(depth >= Limits.MaximumDepth)
        {
            return json_preflight_result::structural_limit;
        }
        ++Position;
        whitespace();
        if(consume('}'))
        {
            return json_preflight_result::valid;
        }

        std::array<std::string_view, hard_limits.MaximumObjectMembers> keys;
        std::size_t count = 0U;
        while(true)
        {
            if(count == Limits.MaximumObjectMembers)
            {
                return json_preflight_result::structural_limit;
            }
            std::string_view key;
            if(!string(&key, true))
            {
                return StringLimitExceeded
                    ? json_preflight_result::structural_limit
                    : json_preflight_result::malformed;
            }
            if(
                std::ranges::find(
                    keys.begin(),
                    keys.begin() + static_cast<std::ptrdiff_t>(count),
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
        if(depth >= Limits.MaximumDepth)
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
            if(count == Limits.MaximumArrayElements)
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

    [[nodiscard]] auto string(
        std::string_view* output,
        const bool object_key) noexcept -> bool
    {
        if(!consume('"'))
        {
            return false;
        }
        const auto start = Position;
        while(Position < Input.size())
        {
            if(Position - start > Limits.MaximumStringBytes)
            {
                StringLimitExceeded = true;
                return false;
            }
            const auto character =
                static_cast<unsigned char>(Input[Position++]);
            if(character == '"')
            {
                if(output != nullptr)
                {
                    *output =
                        Input.substr(start, Position - start - 1U);
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
            if(
                escaped == '"' || escaped == '\\' || escaped == '/'
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
        (void)consume('-');
        if(consume('0'))
        {
            if(
                Position < Input.size() && Input[Position] >= '0'
                && Input[Position] <= '9')
            {
                return json_preflight_result::malformed;
            }
        }
        else
        {
            if(
                Position >= Input.size() || Input[Position] < '1'
                || Input[Position] > '9')
            {
                return json_preflight_result::malformed;
            }
            while(
                Position < Input.size() && Input[Position] >= '0'
                && Input[Position] <= '9')
            {
                ++Position;
            }
        }
        if(consume('.'))
        {
            const auto digits = Position;
            while(
                Position < Input.size() && Input[Position] >= '0'
                && Input[Position] <= '9')
            {
                ++Position;
            }
            if(Position == digits)
            {
                return json_preflight_result::malformed;
            }
        }
        if(
            Position < Input.size()
            && (Input[Position] == 'e' || Input[Position] == 'E'))
        {
            ++Position;
            if(
                Position < Input.size()
                && (Input[Position] == '+' || Input[Position] == '-'))
            {
                ++Position;
            }
            const auto digits = Position;
            while(
                Position < Input.size() && Input[Position] >= '0'
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

    auto whitespace() noexcept -> void
    {
        while(
            Position < Input.size()
            && (
                Input[Position] == ' ' || Input[Position] == '\t'
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
    const capability_probe_limits& Limits;
    std::size_t Position = 0U;
    std::size_t Values = 0U;
    bool StringLimitExceeded = false;
};

enum class selected_type_shape : std::uint8_t
{
    fields,
    input_fields,
    enum_values,
};

enum class type_wrapper : std::uint8_t
{
    named,
    non_null,
    list,
};

struct type_requirement
{
    type_wrapper Wrapper = type_wrapper::named;
    std::string_view Kind;
    std::string_view Name;
};

struct argument_requirement
{
    std::string_view Name;
    type_requirement Type;
};

struct selected_schema_type
{
    bool Missing = false;
    QString Kind;
    QString Name;
    bool MembersAvailable = false;
    QJsonArray Members;
};

[[nodiscard]] auto kind_has_selected_members(
    const QString& kind,
    const selected_type_shape shape) -> bool
{
    switch(shape)
    {
    case selected_type_shape::fields:
        return kind == QStringLiteral("OBJECT")
            || kind == QStringLiteral("INTERFACE");
    case selected_type_shape::input_fields:
        return kind == QStringLiteral("INPUT_OBJECT");
    case selected_type_shape::enum_values:
        return kind == QStringLiteral("ENUM");
    }
    return false;
}

[[nodiscard]] auto fact_index(
    const capability_schema_fact fact) noexcept -> std::size_t
{
    return static_cast<std::size_t>(fact);
}

[[nodiscard]] auto unavailable(
    const capability_probe_unavailability reason) noexcept
    -> capability_probe_observation
{
    return {
        .Capabilities = {},
        .Facts = {},
        .Unavailability = reason,
        .Complete = false,
    };
}

[[nodiscard]] auto valid_limits(
    const capability_probe_limits& limits) noexcept -> bool
{
    const auto within = [](const std::size_t value,
                           const std::size_t maximum) {
        return value != 0U && value <= maximum;
    };
    return within(
               limits.MaximumRequestBytes,
               hard_limits.MaximumRequestBytes)
        && within(
            limits.MaximumResponseBytes,
            hard_limits.MaximumResponseBytes)
        && within(limits.MaximumDepth, hard_limits.MaximumDepth)
        && within(
            limits.MaximumObjectMembers,
            hard_limits.MaximumObjectMembers)
        && within(
            limits.MaximumArrayElements,
            hard_limits.MaximumArrayElements)
        && within(
            limits.MaximumMutationFields,
            hard_limits.MaximumMutationFields)
        && within(
            limits.MaximumPullRequestFields,
            hard_limits.MaximumPullRequestFields)
        && within(
            limits.MaximumOtherTypeFields,
            hard_limits.MaximumOtherTypeFields)
        && within(
            limits.MaximumArguments,
            hard_limits.MaximumArguments)
        && within(
            limits.MaximumInputFields,
            hard_limits.MaximumInputFields)
        && within(
            limits.MaximumEnumValues,
            hard_limits.MaximumEnumValues)
        && within(
            limits.MaximumTotalValues,
            hard_limits.MaximumTotalValues)
        && within(
            limits.MaximumStringBytes,
            hard_limits.MaximumStringBytes);
}

[[nodiscard]] auto exact_keys(
    const QJsonObject& object,
    const std::initializer_list<QString> keys) -> bool
{
    if(object.size() != static_cast<qsizetype>(keys.size()))
    {
        return false;
    }
    return std::ranges::all_of(
        keys,
        [&object](const QString& key) { return object.contains(key); });
}

[[nodiscard]] auto valid_kind(const QString& kind) -> bool
{
    static const QSet<QString> kinds{
        QStringLiteral("SCALAR"),
        QStringLiteral("OBJECT"),
        QStringLiteral("INTERFACE"),
        QStringLiteral("UNION"),
        QStringLiteral("ENUM"),
        QStringLiteral("INPUT_OBJECT"),
        QStringLiteral("LIST"),
        QStringLiteral("NON_NULL"),
    };
    return kinds.contains(kind);
}

[[nodiscard]] auto valid_type_reference(const QJsonValue& value) -> bool
{
    if(!value.isObject())
    {
        return false;
    }
    const auto object = value.toObject();
    if(
        !exact_keys(
            object,
            {
                QStringLiteral("kind"),
                QStringLiteral("name"),
                QStringLiteral("ofType"),
            })
        || !object.value("kind").isString())
    {
        return false;
    }

    const auto kind = object.value("kind").toString();
    if(!valid_kind(kind))
    {
        return false;
    }
    const bool wrapper =
        kind == QStringLiteral("LIST")
        || kind == QStringLiteral("NON_NULL");
    if(wrapper)
    {
        if(!object.value("name").isNull())
        {
            return false;
        }
        const auto inner = object.value("ofType");
        if(!inner.isObject())
        {
            return false;
        }
        const auto inner_object = inner.toObject();
        if(
            !exact_keys(
                inner_object,
                {QStringLiteral("kind"), QStringLiteral("name")})
            || !inner_object.value("kind").isString()
            || !valid_kind(inner_object.value("kind").toString()))
        {
            return false;
        }
        const auto inner_kind = inner_object.value("kind").toString();
        const bool inner_wrapper =
            inner_kind == QStringLiteral("LIST")
            || inner_kind == QStringLiteral("NON_NULL");
        return inner_wrapper
            ? inner_object.value("name").isNull()
            : inner_object.value("name").isString()
                && !inner_object.value("name").toString().isEmpty();
    }
    return object.value("name").isString()
        && !object.value("name").toString().isEmpty()
        && object.value("ofType").isNull();
}

[[nodiscard]] auto exact_type(
    const QJsonValue& value,
    const type_requirement& requirement) -> bool
{
    if(!valid_type_reference(value))
    {
        return false;
    }
    const auto object = value.toObject();
    const auto expected_kind =
        QString::fromUtf8(
            requirement.Kind.data(),
            static_cast<qsizetype>(requirement.Kind.size()));
    const auto expected_name =
        QString::fromUtf8(
            requirement.Name.data(),
            static_cast<qsizetype>(requirement.Name.size()));

    switch(requirement.Wrapper)
    {
    case type_wrapper::named:
        return object.value("kind").toString() == expected_kind
            && object.value("name").toString() == expected_name
            && object.value("ofType").isNull();
    case type_wrapper::non_null:
    case type_wrapper::list:
    {
        const auto wrapper = requirement.Wrapper
                == type_wrapper::non_null
            ? QStringLiteral("NON_NULL")
            : QStringLiteral("LIST");
        if(object.value("kind").toString() != wrapper)
        {
            return false;
        }
        const auto inner = object.value("ofType").toObject();
        return inner.value("kind").toString() == expected_kind
            && inner.value("name").toString() == expected_name;
    }
    }
    return false;
}

[[nodiscard]] auto required_argument(const QJsonObject& argument) -> bool
{
    return argument.value("type")
                   .toObject()
                   .value("kind")
                   .toString()
            == QStringLiteral("NON_NULL")
        && argument.value("defaultValue").isNull();
}

[[nodiscard]] auto validate_arguments(
    const QJsonArray& arguments,
    const capability_probe_limits& limits) -> bool
{
    if(
        arguments.size()
        > static_cast<qsizetype>(limits.MaximumArguments))
    {
        return false;
    }
    QSet<QString> names;
    for(const auto& value : arguments)
    {
        if(!value.isObject())
        {
            return false;
        }
        const auto object = value.toObject();
        if(
            !exact_keys(
                object,
                {
                    QStringLiteral("name"),
                    QStringLiteral("defaultValue"),
                    QStringLiteral("type"),
                })
            || !object.value("name").isString()
            || object.value("name").toString().isEmpty()
            || (
                !object.value("defaultValue").isNull()
                && !object.value("defaultValue").isString())
            || !valid_type_reference(object.value("type")))
        {
            return false;
        }
        const auto name = object.value("name").toString();
        if(names.contains(name))
        {
            return false;
        }
        names.insert(name);
    }
    return true;
}

[[nodiscard]] auto validate_field_members(
    const QJsonArray& fields,
    const std::size_t maximum,
    const capability_probe_limits& limits) -> bool
{
    if(fields.size() > static_cast<qsizetype>(maximum))
    {
        return false;
    }
    QSet<QString> names;
    for(const auto& value : fields)
    {
        if(!value.isObject())
        {
            return false;
        }
        const auto object = value.toObject();
        if(
            !exact_keys(
                object,
                {
                    QStringLiteral("name"),
                    QStringLiteral("args"),
                    QStringLiteral("type"),
                })
            || !object.value("name").isString()
            || object.value("name").toString().isEmpty()
            || !object.value("args").isArray()
            || !validate_arguments(
                object.value("args").toArray(),
                limits)
            || !valid_type_reference(object.value("type")))
        {
            return false;
        }
        const auto name = object.value("name").toString();
        if(names.contains(name))
        {
            return false;
        }
        names.insert(name);
    }
    return true;
}

[[nodiscard]] auto validate_input_members(
    const QJsonArray& fields,
    const capability_probe_limits& limits) -> bool
{
    if(
        fields.size()
        > static_cast<qsizetype>(limits.MaximumInputFields))
    {
        return false;
    }
    QSet<QString> names;
    for(const auto& value : fields)
    {
        if(!value.isObject())
        {
            return false;
        }
        const auto object = value.toObject();
        if(
            !exact_keys(
                object,
                {
                    QStringLiteral("name"),
                    QStringLiteral("defaultValue"),
                    QStringLiteral("type"),
                })
            || !object.value("name").isString()
            || object.value("name").toString().isEmpty()
            || (
                !object.value("defaultValue").isNull()
                && !object.value("defaultValue").isString())
            || !valid_type_reference(object.value("type")))
        {
            return false;
        }
        const auto name = object.value("name").toString();
        if(names.contains(name))
        {
            return false;
        }
        names.insert(name);
    }
    return true;
}

[[nodiscard]] auto validate_enum_members(
    const QJsonArray& values,
    const capability_probe_limits& limits) -> bool
{
    if(
        values.size()
        > static_cast<qsizetype>(limits.MaximumEnumValues))
    {
        return false;
    }
    QSet<QString> names;
    for(const auto& value : values)
    {
        if(!value.isObject())
        {
            return false;
        }
        const auto object = value.toObject();
        if(
            !exact_keys(object, {QStringLiteral("name")})
            || !object.value("name").isString()
            || object.value("name").toString().isEmpty())
        {
            return false;
        }
        const auto name = object.value("name").toString();
        if(names.contains(name))
        {
            return false;
        }
        names.insert(name);
    }
    return true;
}

[[nodiscard]] auto parse_selected_type(
    const QJsonValue& value,
    const std::string_view expected_name,
    const selected_type_shape shape,
    const std::size_t maximum_fields,
    const capability_probe_limits& limits,
    selected_schema_type& output) -> bool
{
    if(value.isNull())
    {
        output = {
            .Missing = true,
            .Kind = {},
            .Name = {},
            .MembersAvailable = false,
            .Members = {},
        };
        return true;
    }
    if(!value.isObject())
    {
        return false;
    }

    const auto object = value.toObject();
    const auto member_name = shape == selected_type_shape::fields
        ? QStringLiteral("fields")
        : shape == selected_type_shape::input_fields
        ? QStringLiteral("inputFields")
        : QStringLiteral("enumValues");
    if(
        !exact_keys(
            object,
            {
                QStringLiteral("kind"),
                QStringLiteral("name"),
                member_name,
            })
        || !object.value("kind").isString()
        || !valid_kind(object.value("kind").toString())
        || !object.value("name").isString()
        || object.value("name").toString().isEmpty())
    {
        return false;
    }
    if(!expected_name.empty())
    {
        const auto expected =
            QString::fromUtf8(
                expected_name.data(),
                static_cast<qsizetype>(expected_name.size()));
        if(object.value("name").toString() != expected)
        {
            return false;
        }
    }

    output = {
        .Missing = false,
        .Kind = object.value("kind").toString(),
        .Name = object.value("name").toString(),
        .MembersAvailable = false,
        .Members = {},
    };
    const auto members = object.value(member_name);
    if(members.isNull())
    {
        return !kind_has_selected_members(output.Kind, shape);
    }
    if(!members.isArray())
    {
        return false;
    }
    output.MembersAvailable = true;
    output.Members = members.toArray();
    switch(shape)
    {
    case selected_type_shape::fields:
        return validate_field_members(
            output.Members,
            maximum_fields,
            limits);
    case selected_type_shape::input_fields:
        return validate_input_members(output.Members, limits);
    case selected_type_shape::enum_values:
        return validate_enum_members(output.Members, limits);
    }
    return false;
}

[[nodiscard]] auto find_named_member(
    const QJsonArray& members,
    const std::string_view name) -> std::optional<QJsonObject>
{
    const auto expected = QString::fromUtf8(
        name.data(),
        static_cast<qsizetype>(name.size()));
    for(const auto& value : members)
    {
        const auto object = value.toObject();
        if(object.value("name").toString() == expected)
        {
            return object;
        }
    }
    return std::nullopt;
}

[[nodiscard]] auto field_requirement(
    const selected_schema_type& type,
    const std::string_view expected_kind,
    const std::string_view field_name,
    const type_requirement& return_type,
    const std::initializer_list<argument_requirement> expected_arguments = {})
    -> capability_schema_fact_state
{
    if(type.Missing)
    {
        return capability_schema_fact_state::missing;
    }
    if(
        type.Kind
                != QString::fromUtf8(
                    expected_kind.data(),
                    static_cast<qsizetype>(expected_kind.size()))
        || !type.MembersAvailable)
    {
        return capability_schema_fact_state::incompatible;
    }
    const auto field = find_named_member(type.Members, field_name);
    if(!field.has_value())
    {
        return capability_schema_fact_state::missing;
    }
    if(!exact_type(field->value("type"), return_type))
    {
        return capability_schema_fact_state::incompatible;
    }

    const auto arguments = field->value("args").toArray();
    QSet<QString> expected_names;
    for(const auto& expected : expected_arguments)
    {
        expected_names.insert(
            QString::fromUtf8(
                expected.Name.data(),
                static_cast<qsizetype>(expected.Name.size())));
        const auto argument =
            find_named_member(arguments, expected.Name);
        if(
            !argument.has_value()
            || !exact_type(argument->value("type"), expected.Type))
        {
            return capability_schema_fact_state::incompatible;
        }
    }
    for(const auto& value : arguments)
    {
        const auto argument = value.toObject();
        if(
            !expected_names.contains(
                argument.value("name").toString())
            && required_argument(argument))
        {
            return capability_schema_fact_state::incompatible;
        }
    }
    return capability_schema_fact_state::matched;
}

[[nodiscard]] auto input_requirement(
    const selected_schema_type& type,
    const std::initializer_list<argument_requirement> expected_fields)
    -> capability_schema_fact_state
{
    if(type.Missing)
    {
        return capability_schema_fact_state::missing;
    }
    if(
        type.Kind != QStringLiteral("INPUT_OBJECT")
        || !type.MembersAvailable)
    {
        return capability_schema_fact_state::incompatible;
    }

    QSet<QString> expected_names;
    for(const auto& expected : expected_fields)
    {
        expected_names.insert(
            QString::fromUtf8(
                expected.Name.data(),
                static_cast<qsizetype>(expected.Name.size())));
        const auto field = find_named_member(type.Members, expected.Name);
        if(
            !field.has_value()
            || !exact_type(field->value("type"), expected.Type))
        {
            return capability_schema_fact_state::incompatible;
        }
    }
    for(const auto& value : type.Members)
    {
        const auto field = value.toObject();
        if(
            !expected_names.contains(field.value("name").toString())
            && required_argument(field))
        {
            return capability_schema_fact_state::incompatible;
        }
    }
    return capability_schema_fact_state::matched;
}

[[nodiscard]] auto enum_requirement(
    const selected_schema_type& type,
    const std::initializer_list<std::string_view> required_values)
    -> capability_schema_fact_state
{
    if(type.Missing)
    {
        return capability_schema_fact_state::missing;
    }
    if(type.Kind != QStringLiteral("ENUM") || !type.MembersAvailable)
    {
        return capability_schema_fact_state::incompatible;
    }
    for(const auto expected : required_values)
    {
        if(!find_named_member(type.Members, expected).has_value())
        {
            return capability_schema_fact_state::incompatible;
        }
    }
    return capability_schema_fact_state::matched;
}

[[nodiscard]] auto merge_fact(
    const capability_schema_fact_state left,
    const capability_schema_fact_state right) noexcept
    -> capability_schema_fact_state
{
    if(
        left == capability_schema_fact_state::incompatible
        || right == capability_schema_fact_state::incompatible)
    {
        return capability_schema_fact_state::incompatible;
    }
    if(
        left == capability_schema_fact_state::missing
        || right == capability_schema_fact_state::missing)
    {
        return capability_schema_fact_state::missing;
    }
    if(
        left == capability_schema_fact_state::unknown
        || right == capability_schema_fact_state::unknown)
    {
        return capability_schema_fact_state::unknown;
    }
    return capability_schema_fact_state::matched;
}

template<typename... States>
[[nodiscard]] auto combined_fact(
    const capability_schema_fact_state first,
    const States... remaining) noexcept
    -> capability_schema_fact_state
{
    auto result = first;
    ((result = merge_fact(result, remaining)), ...);
    return result;
}

[[nodiscard]] auto capability_state(
    const std::initializer_list<capability_schema_fact_state> facts)
    noexcept -> core::github_capability_state
{
    if(std::ranges::all_of(facts, [](const auto fact) {
           return fact == capability_schema_fact_state::matched;
       }))
    {
        return core::github_capability_state::supported;
    }
    if(std::ranges::any_of(facts, [](const auto fact) {
           return fact == capability_schema_fact_state::missing
               || fact == capability_schema_fact_state::incompatible;
       }))
    {
        return core::github_capability_state::unsupported;
    }
    return core::github_capability_state::unknown;
}

[[nodiscard]] auto scalar(
    const std::string_view name,
    const bool required = false) -> type_requirement
{
    return {
        .Wrapper =
            required ? type_wrapper::non_null : type_wrapper::named,
        .Kind = "SCALAR",
        .Name = name,
    };
}

[[nodiscard]] auto object_type(
    const std::string_view name,
    const bool required = false) -> type_requirement
{
    return {
        .Wrapper =
            required ? type_wrapper::non_null : type_wrapper::named,
        .Kind = "OBJECT",
        .Name = name,
    };
}

[[nodiscard]] auto interface_type(
    const std::string_view name) -> type_requirement
{
    return {
        .Wrapper = type_wrapper::named,
        .Kind = "INTERFACE",
        .Name = name,
    };
}

[[nodiscard]] auto enum_type(
    const std::string_view name,
    const bool required = false) -> type_requirement
{
    return {
        .Wrapper =
            required ? type_wrapper::non_null : type_wrapper::named,
        .Kind = "ENUM",
        .Name = name,
    };
}

[[nodiscard]] auto input_type(
    const std::string_view name) -> type_requirement
{
    return {
        .Wrapper = type_wrapper::non_null,
        .Kind = "INPUT_OBJECT",
        .Name = name,
    };
}

[[nodiscard]] auto list_of_objects(
    const std::string_view name) -> type_requirement
{
    return {
        .Wrapper = type_wrapper::list,
        .Kind = "OBJECT",
        .Name = name,
    };
}

[[nodiscard]] auto response_has_exact_aliases(
    const QJsonObject& data) -> bool
{
    return data.size()
            == static_cast<qsizetype>(response_aliases.size())
        && std::ranges::all_of(
            response_aliases,
            [&data](const auto alias) {
                return data.contains(QString::fromUtf8(
                    alias.data(),
                    static_cast<qsizetype>(alias.size())));
            });
}

[[nodiscard]] auto field_array_exceeds_limits(
    const QJsonValue& value,
    const std::size_t maximum_fields,
    const capability_probe_limits& limits) -> bool
{
    if(!value.isObject())
    {
        return false;
    }
    const auto fields = value.toObject().value("fields");
    if(!fields.isArray())
    {
        return false;
    }
    const auto array = fields.toArray();
    if(array.size() > static_cast<qsizetype>(maximum_fields))
    {
        return true;
    }
    return std::ranges::any_of(array, [&limits](const auto& field) {
        return field.isObject()
            && field.toObject().value("args").isArray()
            && field.toObject().value("args").toArray().size()
                > static_cast<qsizetype>(limits.MaximumArguments);
    });
}

[[nodiscard]] auto schema_specific_limits_exceeded(
    const QJsonObject& data,
    const capability_probe_limits& limits) -> bool
{
    const auto schema = data.value("schemaRoot");
    if(
        schema.isObject()
        && field_array_exceeds_limits(
            schema.toObject().value("mutationType"),
            limits.MaximumMutationFields,
            limits))
    {
        return true;
    }
    if(
        field_array_exceeds_limits(
            data.value("pullRequest"),
            limits.MaximumPullRequestFields,
            limits))
    {
        return true;
    }

    constexpr std::array<std::string_view, 13> field_aliases{
        "threadConnection",
        "reviewThread",
        "commentConnection",
        "reviewComment",
        "changedFileConnection",
        "changedFile",
        "pageInfo",
        "actor",
        "node",
        "markPayload",
        "unmarkPayload",
        "resolvePayload",
        "unresolvePayload",
    };
    for(const auto alias : field_aliases)
    {
        if(field_array_exceeds_limits(
               data.value(QString::fromUtf8(
                   alias.data(),
                   static_cast<qsizetype>(alias.size()))),
               limits.MaximumOtherTypeFields,
               limits))
        {
            return true;
        }
    }

    constexpr std::array<std::string_view, 4> input_aliases{
        "markInput",
        "unmarkInput",
        "resolveInput",
        "unresolveInput",
    };
    for(const auto alias : input_aliases)
    {
        const auto value = data.value(QString::fromUtf8(
            alias.data(),
            static_cast<qsizetype>(alias.size())));
        if(
            value.isObject()
            && value.toObject().value("inputFields").isArray()
            && value.toObject()
                       .value("inputFields")
                       .toArray()
                       .size()
                > static_cast<qsizetype>(limits.MaximumInputFields))
        {
            return true;
        }
    }

    for(const auto alias : {"threadSubject", "fileViewedState"})
    {
        const auto value = data.value(alias);
        if(
            value.isObject()
            && value.toObject().value("enumValues").isArray()
            && value.toObject()
                       .value("enumValues")
                       .toArray()
                       .size()
                > static_cast<qsizetype>(limits.MaximumEnumValues))
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] auto evaluate_schema(
    const QJsonObject& data,
    const capability_probe_limits& limits,
    capability_probe_observation& output) -> bool
{
    const auto parse_fields =
        [&data, &limits](
            const std::string_view alias,
            const std::string_view type_name,
            const std::size_t maximum,
            selected_schema_type& result) {
            return parse_selected_type(
                data.value(QString::fromUtf8(
                    alias.data(),
                    static_cast<qsizetype>(alias.size()))),
                type_name,
                selected_type_shape::fields,
                maximum,
                limits,
                result);
        };
    const auto parse_input =
        [&data, &limits](
            const std::string_view alias,
            const std::string_view type_name,
            selected_schema_type& result) {
            return parse_selected_type(
                data.value(QString::fromUtf8(
                    alias.data(),
                    static_cast<qsizetype>(alias.size()))),
                type_name,
                selected_type_shape::input_fields,
                limits.MaximumInputFields,
                limits,
                result);
        };
    const auto parse_enum =
        [&data, &limits](
            const std::string_view alias,
            const std::string_view type_name,
            selected_schema_type& result) {
            return parse_selected_type(
                data.value(QString::fromUtf8(
                    alias.data(),
                    static_cast<qsizetype>(alias.size()))),
                type_name,
                selected_type_shape::enum_values,
                limits.MaximumEnumValues,
                limits,
                result);
        };

    selected_schema_type mutation;
    const auto schema_root = data.value("schemaRoot");
    if(!schema_root.isObject())
    {
        return false;
    }
    const auto schema_object = schema_root.toObject();
    if(
        !exact_keys(
            schema_object,
            {QStringLiteral("mutationType")})
        || !parse_selected_type(
            schema_object.value("mutationType"),
            {},
            selected_type_shape::fields,
            limits.MaximumMutationFields,
            limits,
            mutation))
    {
        return false;
    }

    selected_schema_type pull_request;
    selected_schema_type thread_connection;
    selected_schema_type review_thread;
    selected_schema_type comment_connection;
    selected_schema_type review_comment;
    selected_schema_type changed_file_connection;
    selected_schema_type changed_file;
    selected_schema_type page_info;
    selected_schema_type actor;
    selected_schema_type node;
    selected_schema_type thread_subject;
    selected_schema_type viewed_state;
    selected_schema_type mark_input;
    selected_schema_type mark_payload;
    selected_schema_type unmark_input;
    selected_schema_type unmark_payload;
    selected_schema_type resolve_input;
    selected_schema_type resolve_payload;
    selected_schema_type unresolve_input;
    selected_schema_type unresolve_payload;

    if(
        !parse_fields(
            "pullRequest",
            "PullRequest",
            limits.MaximumPullRequestFields,
            pull_request)
        || !parse_fields(
            "threadConnection",
            "PullRequestReviewThreadConnection",
            limits.MaximumOtherTypeFields,
            thread_connection)
        || !parse_fields(
            "reviewThread",
            "PullRequestReviewThread",
            limits.MaximumOtherTypeFields,
            review_thread)
        || !parse_fields(
            "commentConnection",
            "PullRequestReviewCommentConnection",
            limits.MaximumOtherTypeFields,
            comment_connection)
        || !parse_fields(
            "reviewComment",
            "PullRequestReviewComment",
            limits.MaximumOtherTypeFields,
            review_comment)
        || !parse_fields(
            "changedFileConnection",
            "PullRequestChangedFileConnection",
            limits.MaximumOtherTypeFields,
            changed_file_connection)
        || !parse_fields(
            "changedFile",
            "PullRequestChangedFile",
            limits.MaximumOtherTypeFields,
            changed_file)
        || !parse_fields(
            "pageInfo",
            "PageInfo",
            limits.MaximumOtherTypeFields,
            page_info)
        || !parse_fields(
            "actor",
            "Actor",
            limits.MaximumOtherTypeFields,
            actor)
        || !parse_fields(
            "node",
            "Node",
            limits.MaximumOtherTypeFields,
            node)
        || !parse_enum(
            "threadSubject",
            "PullRequestReviewThreadSubjectType",
            thread_subject)
        || !parse_enum(
            "fileViewedState",
            "FileViewedState",
            viewed_state)
        || !parse_input(
            "markInput",
            "MarkFileAsViewedInput",
            mark_input)
        || !parse_fields(
            "markPayload",
            "MarkFileAsViewedPayload",
            limits.MaximumOtherTypeFields,
            mark_payload)
        || !parse_input(
            "unmarkInput",
            "UnmarkFileAsViewedInput",
            unmark_input)
        || !parse_fields(
            "unmarkPayload",
            "UnmarkFileAsViewedPayload",
            limits.MaximumOtherTypeFields,
            unmark_payload)
        || !parse_input(
            "resolveInput",
            "ResolveReviewThreadInput",
            resolve_input)
        || !parse_fields(
            "resolvePayload",
            "ResolveReviewThreadPayload",
            limits.MaximumOtherTypeFields,
            resolve_payload)
        || !parse_input(
            "unresolveInput",
            "UnresolveReviewThreadInput",
            unresolve_input)
        || !parse_fields(
            "unresolvePayload",
            "UnresolveReviewThreadPayload",
            limits.MaximumOtherTypeFields,
            unresolve_payload))
    {
        return false;
    }

    const argument_requirement after{
        .Name = "after",
        .Type = scalar("String"),
    };
    const argument_requirement first{
        .Name = "first",
        .Type = scalar("Int"),
    };

    const auto review_read = combined_fact(
        field_requirement(
            pull_request,
            "OBJECT",
            "reviewThreads",
            object_type(
                "PullRequestReviewThreadConnection",
                true),
            {after, first}),
        field_requirement(
            thread_connection,
            "OBJECT",
            "nodes",
            list_of_objects("PullRequestReviewThread")),
        field_requirement(
            thread_connection,
            "OBJECT",
            "pageInfo",
            object_type("PageInfo", true)),
        field_requirement(
            review_thread,
            "OBJECT",
            "id",
            scalar("ID", true)),
        field_requirement(
            review_thread,
            "OBJECT",
            "isResolved",
            scalar("Boolean", true)),
        field_requirement(
            review_thread,
            "OBJECT",
            "isOutdated",
            scalar("Boolean", true)),
        field_requirement(
            review_thread,
            "OBJECT",
            "subjectType",
            enum_type(
                "PullRequestReviewThreadSubjectType",
                true)),
        field_requirement(
            review_thread,
            "OBJECT",
            "path",
            scalar("String", true)),
        field_requirement(
            review_thread,
            "OBJECT",
            "line",
            scalar("Int")),
        field_requirement(
            review_thread,
            "OBJECT",
            "originalLine",
            scalar("Int")),
        field_requirement(
            review_thread,
            "OBJECT",
            "comments",
            object_type(
                "PullRequestReviewCommentConnection",
                true),
            {after, first}),
        field_requirement(
            comment_connection,
            "OBJECT",
            "nodes",
            list_of_objects("PullRequestReviewComment")),
        field_requirement(
            comment_connection,
            "OBJECT",
            "pageInfo",
            object_type("PageInfo", true)),
        field_requirement(
            review_comment,
            "OBJECT",
            "id",
            scalar("ID", true)),
        field_requirement(
            review_comment,
            "OBJECT",
            "bodyText",
            scalar("String", true)),
        field_requirement(
            review_comment,
            "OBJECT",
            "path",
            scalar("String", true)),
        field_requirement(
            review_comment,
            "OBJECT",
            "line",
            scalar("Int")),
        field_requirement(
            review_comment,
            "OBJECT",
            "originalLine",
            scalar("Int")),
        field_requirement(
            review_comment,
            "OBJECT",
            "outdated",
            scalar("Boolean", true)),
        field_requirement(
            review_comment,
            "OBJECT",
            "author",
            interface_type("Actor")),
        field_requirement(
            review_comment,
            "OBJECT",
            "replyTo",
            object_type("PullRequestReviewComment")),
        field_requirement(
            page_info,
            "OBJECT",
            "hasNextPage",
            scalar("Boolean", true)),
        field_requirement(
            page_info,
            "OBJECT",
            "endCursor",
            scalar("String")),
        field_requirement(
            actor,
            "INTERFACE",
            "login",
            scalar("String", true)),
        field_requirement(
            node,
            "INTERFACE",
            "id",
            scalar("ID", true)),
        enum_requirement(thread_subject, {"FILE", "LINE"}));

    const auto viewed_read = combined_fact(
        field_requirement(
            pull_request,
            "OBJECT",
            "files",
            object_type("PullRequestChangedFileConnection"),
            {after, first}),
        field_requirement(
            changed_file_connection,
            "OBJECT",
            "nodes",
            list_of_objects("PullRequestChangedFile")),
        field_requirement(
            changed_file_connection,
            "OBJECT",
            "pageInfo",
            object_type("PageInfo", true)),
        field_requirement(
            changed_file,
            "OBJECT",
            "path",
            scalar("String", true)),
        field_requirement(
            changed_file,
            "OBJECT",
            "viewerViewedState",
            enum_type("FileViewedState", true)),
        enum_requirement(
            viewed_state,
            {"DISMISSED", "UNVIEWED", "VIEWED"}));

    const argument_requirement client_mutation_id{
        .Name = "clientMutationId",
        .Type = scalar("String"),
    };
    const argument_requirement path{
        .Name = "path",
        .Type = scalar("String", true),
    };
    const argument_requirement pull_request_id{
        .Name = "pullRequestId",
        .Type = scalar("ID", true),
    };
    const argument_requirement thread_id{
        .Name = "threadId",
        .Type = scalar("ID", true),
    };

    const auto mark = combined_fact(
        field_requirement(
            mutation,
            "OBJECT",
            "markFileAsViewed",
            object_type("MarkFileAsViewedPayload"),
            {{
                .Name = "input",
                .Type = input_type("MarkFileAsViewedInput"),
            }}),
        input_requirement(
            mark_input,
            {client_mutation_id, path, pull_request_id}),
        field_requirement(
            mark_payload,
            "OBJECT",
            "clientMutationId",
            scalar("String")),
        field_requirement(
            mark_payload,
            "OBJECT",
            "pullRequest",
            object_type("PullRequest")));

    const auto unmark = combined_fact(
        field_requirement(
            mutation,
            "OBJECT",
            "unmarkFileAsViewed",
            object_type("UnmarkFileAsViewedPayload"),
            {{
                .Name = "input",
                .Type = input_type("UnmarkFileAsViewedInput"),
            }}),
        input_requirement(
            unmark_input,
            {client_mutation_id, path, pull_request_id}),
        field_requirement(
            unmark_payload,
            "OBJECT",
            "clientMutationId",
            scalar("String")),
        field_requirement(
            unmark_payload,
            "OBJECT",
            "pullRequest",
            object_type("PullRequest")));

    const auto resolve_permission = field_requirement(
        review_thread,
        "OBJECT",
        "viewerCanResolve",
        scalar("Boolean", true));
    const auto unresolve_permission = field_requirement(
        review_thread,
        "OBJECT",
        "viewerCanUnresolve",
        scalar("Boolean", true));

    const auto resolve = combined_fact(
        field_requirement(
            mutation,
            "OBJECT",
            "resolveReviewThread",
            object_type("ResolveReviewThreadPayload"),
            {{
                .Name = "input",
                .Type = input_type("ResolveReviewThreadInput"),
            }}),
        input_requirement(
            resolve_input,
            {client_mutation_id, thread_id}),
        field_requirement(
            resolve_payload,
            "OBJECT",
            "clientMutationId",
            scalar("String")),
        field_requirement(
            resolve_payload,
            "OBJECT",
            "thread",
            object_type("PullRequestReviewThread")));

    const auto unresolve = combined_fact(
        field_requirement(
            mutation,
            "OBJECT",
            "unresolveReviewThread",
            object_type("UnresolveReviewThreadPayload"),
            {{
                .Name = "input",
                .Type = input_type("UnresolveReviewThreadInput"),
            }}),
        input_requirement(
            unresolve_input,
            {client_mutation_id, thread_id}),
        field_requirement(
            unresolve_payload,
            "OBJECT",
            "clientMutationId",
            scalar("String")),
        field_requirement(
            unresolve_payload,
            "OBJECT",
            "thread",
            object_type("PullRequestReviewThread")));

    output.Facts[fact_index(
        capability_schema_fact::review_threads_read)] = review_read;
    output.Facts[fact_index(
        capability_schema_fact::viewed_file_state_read)] = viewed_read;
    output.Facts[fact_index(
        capability_schema_fact::mark_file_as_viewed)] = mark;
    output.Facts[fact_index(
        capability_schema_fact::unmark_file_as_viewed)] = unmark;
    output.Facts[fact_index(
        capability_schema_fact::resolve_permission)] =
        resolve_permission;
    output.Facts[fact_index(
        capability_schema_fact::resolve_review_thread)] = resolve;
    output.Facts[fact_index(
        capability_schema_fact::unresolve_permission)] =
        unresolve_permission;
    output.Facts[fact_index(
        capability_schema_fact::unresolve_review_thread)] = unresolve;

    output.Capabilities = {
        .ReviewThreads = capability_state({review_read}),
        .ViewedFileMutations =
            capability_state({viewed_read, mark, unmark}),
        .ResolveThread = capability_state(
            {review_read, resolve_permission, resolve}),
        .UnresolveThread = capability_state(
            {review_read, unresolve_permission, unresolve}),
    };
    output.Unavailability = capability_probe_unavailability::none;
    output.Complete = true;
    return true;
}

[[nodiscard]] auto ascii_fold(const char character) noexcept -> char
{
    return character >= 'A' && character <= 'Z'
        ? static_cast<char>(character - 'A' + 'a')
        : character;
}

[[nodiscard]] auto ascii_equal_fold(
    const std::string_view left,
    const std::string_view right) noexcept -> bool
{
    return left.size() == right.size()
        && std::ranges::equal(
            left,
            right,
            [](const char left_value, const char right_value) {
                return ascii_fold(left_value) == ascii_fold(right_value);
            });
}

[[nodiscard]] auto trim_http_ows(std::string_view value) noexcept
    -> std::string_view
{
    while(
        !value.empty()
        && (value.front() == ' ' || value.front() == '\t'))
    {
        value.remove_prefix(1U);
    }
    while(
        !value.empty()
        && (value.back() == ' ' || value.back() == '\t'))
    {
        value.remove_suffix(1U);
    }
    return value;
}

[[nodiscard]] auto decimal_header_value(std::string_view value) noexcept
    -> std::optional<std::uint64_t>
{
    constexpr std::size_t maximum_decimal_header_bytes = 32U;
    if(value.size() > maximum_decimal_header_bytes)
    {
        return std::nullopt;
    }
    value = trim_http_ows(value);
    if(value.empty())
    {
        return std::nullopt;
    }

    std::uint64_t result = 0U;
    for(const auto character : value)
    {
        if(character < '0' || character > '9')
        {
            return std::nullopt;
        }
        const auto digit =
            static_cast<std::uint64_t>(character - '0');
        if(
            result
            > (
                std::numeric_limits<std::uint64_t>::max() - digit)
                / 10U)
        {
            return std::nullopt;
        }
        result = result * 10U + digit;
    }
    return result;
}

enum class rate_limit_header_kind : std::uint8_t
{
    limit,
    remaining,
    reset,
    retry_after,
    count,
};

struct rate_limit_response_facts
{
    std::optional<std::uint64_t> Limit;
    std::optional<std::uint64_t> Remaining;
    std::optional<std::uint64_t> ResetUnixSeconds;
    std::optional<std::uint64_t> RetryAfterSeconds;

    [[nodiscard]] auto signals_throttling() const noexcept -> bool
    {
        return (
                   Remaining.has_value()
                   && *Remaining == 0U)
            || RetryAfterSeconds.has_value();
    }
};

[[nodiscard]] auto rate_limit_header(
    const std::string_view name) noexcept
    -> std::optional<rate_limit_header_kind>
{
    if(ascii_equal_fold(name, "x-ratelimit-limit"))
    {
        return rate_limit_header_kind::limit;
    }
    if(ascii_equal_fold(name, "x-ratelimit-remaining"))
    {
        return rate_limit_header_kind::remaining;
    }
    if(ascii_equal_fold(name, "x-ratelimit-reset"))
    {
        return rate_limit_header_kind::reset;
    }
    if(ascii_equal_fold(name, "retry-after"))
    {
        return rate_limit_header_kind::retry_after;
    }
    return std::nullopt;
}

[[nodiscard]] auto parse_rate_limit_response_facts(
    const capability_probe_response& response) noexcept
    -> std::optional<rate_limit_response_facts>
{
    constexpr std::size_t maximum_header_name_bytes = 32U;
    if(
        response.RateLimitHeaderCount
        > response.RateLimitHeaders.size())
    {
        return std::nullopt;
    }

    auto facts = rate_limit_response_facts{};
    auto seen = std::array<
        bool,
        static_cast<std::size_t>(rate_limit_header_kind::count)>{};
    for(std::size_t index = 0U;
        index < response.RateLimitHeaders.size();
        ++index)
    {
        const auto& header = response.RateLimitHeaders[index];
        if(index >= response.RateLimitHeaderCount)
        {
            if(!header.Name.empty() || !header.Value.empty())
            {
                return std::nullopt;
            }
            continue;
        }
        if(
            header.Name.empty()
            || header.Name.size() > maximum_header_name_bytes)
        {
            return std::nullopt;
        }
        const auto kind = rate_limit_header(header.Name);
        const auto parsed = decimal_header_value(header.Value);
        if(!kind.has_value() || !parsed.has_value())
        {
            return std::nullopt;
        }
        const auto kind_index = static_cast<std::size_t>(*kind);
        if(seen[kind_index])
        {
            return std::nullopt;
        }
        seen[kind_index] = true;
        switch(*kind)
        {
        case rate_limit_header_kind::limit:
            facts.Limit = *parsed;
            break;
        case rate_limit_header_kind::remaining:
            facts.Remaining = *parsed;
            break;
        case rate_limit_header_kind::reset:
            facts.ResetUnixSeconds = *parsed;
            break;
        case rate_limit_header_kind::retry_after:
            facts.RetryAfterSeconds = *parsed;
            break;
        case rate_limit_header_kind::count:
            return std::nullopt;
        }
    }
    if(
        facts.Limit.has_value() && facts.Remaining.has_value()
        && *facts.Remaining > *facts.Limit)
    {
        return std::nullopt;
    }
    return facts;
}

[[nodiscard]] auto graphql_error_rate_limited(
    const QJsonValue& errors) -> bool
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
        if(
            error.value("type").isString()
            && error.value("type").toString()
                == QStringLiteral("RATE_LIMITED"))
        {
            return true;
        }
        const auto extensions = error.value("extensions");
        if(!extensions.isObject())
        {
            continue;
        }
        const auto extension = extensions.toObject();
        for(const auto key : {"type", "code"})
        {
            if(
                extension.value(key).isString()
                && extension.value(key).toString()
                    == QStringLiteral("RATE_LIMITED"))
            {
                return true;
            }
        }
    }
    return false;
}

[[nodiscard]] auto json_content_type(
    const std::string_view content_type) noexcept -> bool
{
    if(content_type.size() > 256U)
    {
        return false;
    }
    const auto separator = content_type.find(';');
    auto media_type = content_type.substr(0U, separator);
    while(!media_type.empty() && std::isspace(
              static_cast<unsigned char>(media_type.front())) != 0)
    {
        media_type.remove_prefix(1U);
    }
    while(!media_type.empty() && std::isspace(
              static_cast<unsigned char>(media_type.back())) != 0)
    {
        media_type.remove_suffix(1U);
    }
    constexpr std::string_view expected = "application/json";
    if(media_type.size() != expected.size())
    {
        return false;
    }
    for(std::size_t index = 0U; index < media_type.size(); ++index)
    {
        if(ascii_fold(media_type[index]) != expected[index])
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] auto status_failure(const int status) noexcept
    -> capability_probe_unavailability
{
    switch(status)
    {
    case 401:
        return capability_probe_unavailability::authentication;
    case 403:
        return capability_probe_unavailability::permission;
    case 404:
        return capability_probe_unavailability::api_version;
    case 429:
        return capability_probe_unavailability::rate_limit;
    default:
        return capability_probe_unavailability::invalid_status;
    }
}
}

auto valid(const capability_probe_observation& observation) noexcept -> bool
{
    if(
        !valid(observation.Unavailability)
        || !std::ranges::all_of(
            observation.Facts,
            [](const auto state) { return valid(state); }))
    {
        return false;
    }

    if(!observation.Complete)
    {
        return observation.Unavailability
                != capability_probe_unavailability::none
            && observation.Capabilities == core::github_capabilities{}
            && std::ranges::all_of(
                observation.Facts,
                [](const auto state) {
                    return state
                        == capability_schema_fact_state::unknown;
                });
    }
    if(
        observation.Unavailability
            != capability_probe_unavailability::none
        || std::ranges::any_of(
            observation.Facts,
            [](const auto state) {
                return state == capability_schema_fact_state::unknown;
            }))
    {
        return false;
    }

    const auto review = observation.Facts[fact_index(
        capability_schema_fact::review_threads_read)];
    const auto viewed = observation.Facts[fact_index(
        capability_schema_fact::viewed_file_state_read)];
    const auto mark = observation.Facts[fact_index(
        capability_schema_fact::mark_file_as_viewed)];
    const auto unmark = observation.Facts[fact_index(
        capability_schema_fact::unmark_file_as_viewed)];
    const auto resolve_permission = observation.Facts[fact_index(
        capability_schema_fact::resolve_permission)];
    const auto resolve = observation.Facts[fact_index(
        capability_schema_fact::resolve_review_thread)];
    const auto unresolve_permission = observation.Facts[fact_index(
        capability_schema_fact::unresolve_permission)];
    const auto unresolve = observation.Facts[fact_index(
        capability_schema_fact::unresolve_review_thread)];
    return observation.Capabilities
        == core::github_capabilities{
            .ReviewThreads = capability_state({review}),
            .ViewedFileMutations =
                capability_state({viewed, mark, unmark}),
            .ResolveThread =
                capability_state(
                    {review, resolve_permission, resolve}),
            .UnresolveThread =
                capability_state(
                    {review, unresolve_permission, unresolve}),
        };
}

auto capability_probe_graphql_document() noexcept -> std::string_view
{
    return graphql_document;
}

auto capability_probe_document_is_query_only(
    const std::string_view document) noexcept -> bool
{
    return query_only_graphql_document(document);
}

auto compile_capability_probe_request(
    const validated_github_host_profile& profile,
    const capability_probe_limits& limits)
    -> capability_probe_request_result
{
    const auto contract = profile.server_contract();
    if(!application::valid(contract))
    {
        return std::unexpected{
            capability_probe_unavailability::invalid_contract};
    }
    if(!valid_limits(limits))
    {
        return std::unexpected{
            capability_probe_unavailability::invalid_limits};
    }

    try
    {
        const QJsonObject payload{
            {
                QStringLiteral("query"),
                QString::fromUtf8(
                    graphql_document.data(),
                    static_cast<qsizetype>(graphql_document.size())),
            },
        };
        const auto bytes =
            QJsonDocument{payload}.toJson(QJsonDocument::Compact);
        if(
            bytes.size() < 0
            || static_cast<std::size_t>(bytes.size())
                > limits.MaximumRequestBytes)
        {
            return std::unexpected{
                capability_probe_unavailability::response_limit};
        }

        return capability_probe_request{
            .Profile = profile.profile(),
            .Host = profile.host(),
            .ProfileRevision = profile.revision(),
            .Contract = contract,
            .Method = capability_probe_http_method::post,
            .Authority = std::string{profile.rest_authority()},
            .Root = std::string{profile.graphql_root()},
            .Path = std::string{profile.graphql_route()},
            .Headers =
                {
                    capability_probe_header{
                        .Name = "Accept",
                        .Value = "application/json",
                    },
                    capability_probe_header{
                        .Name = "Content-Type",
                        .Value = "application/json",
                    },
                    capability_probe_header{
                        .Name = "User-Agent",
                        .Value = "BetterPullReview/0.1",
                    },
                },
            .Body =
                {
                    bytes.constData(),
                    static_cast<std::size_t>(bytes.size()),
                },
        };
    }
    catch(const std::bad_alloc&)
    {
        return std::unexpected{
            capability_probe_unavailability::allocation_failure};
    }
}

auto normalize_capability_probe(
    const capability_probe_transport_result& response,
    const capability_probe_limits& limits) noexcept
    -> capability_probe_observation
{
    if(!valid_limits(limits))
    {
        return unavailable(
            capability_probe_unavailability::invalid_limits);
    }
    if(!response.has_value())
    {
        const auto reason = response.error();
        return unavailable(
            !valid(reason)
                    || reason == capability_probe_unavailability::none
                ? capability_probe_unavailability::malformed_response
                : reason);
    }

    const auto& value = *response;
    if(!value.Authenticated)
    {
        return unavailable(
            capability_probe_unavailability::authentication);
    }
    const auto rate_limit =
        parse_rate_limit_response_facts(value);
    if(!rate_limit.has_value())
    {
        return unavailable(
            capability_probe_unavailability::malformed_response);
    }
    if(value.Status != 200)
    {
        if(
            value.Status == 403
            && rate_limit->signals_throttling())
        {
            return unavailable(
                capability_probe_unavailability::rate_limit);
        }
        return unavailable(status_failure(value.Status));
    }
    if(!json_content_type(value.ContentType))
    {
        return unavailable(
            capability_probe_unavailability::invalid_content_type);
    }
    if(
        value.Body.empty()
        || value.Body.size() > limits.MaximumResponseBytes)
    {
        return unavailable(
            value.Body.empty()
                ? capability_probe_unavailability::malformed_response
                : capability_probe_unavailability::response_limit);
    }

    try
    {
        const auto preflight =
            json_structural_preflight{value.Body, limits}.run();
        if(preflight != json_preflight_result::valid)
        {
            return unavailable(
                preflight == json_preflight_result::structural_limit
                    ? capability_probe_unavailability::response_limit
                    : capability_probe_unavailability::
                        malformed_response);
        }

        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(
            QByteArray{
                value.Body.data(),
                static_cast<qsizetype>(value.Body.size()),
            },
            &error);
        if(
            error.error != QJsonParseError::NoError
            || !document.isObject())
        {
            return unavailable(
                capability_probe_unavailability::malformed_response);
        }
        const auto envelope = document.object();
        if(envelope.contains("errors"))
        {
            if(
                rate_limit->signals_throttling()
                || graphql_error_rate_limited(
                    envelope.value("errors")))
            {
                return unavailable(
                    capability_probe_unavailability::rate_limit);
            }
            return unavailable(
                capability_probe_unavailability::partial_response);
        }
        if(
            !envelope.contains("data")
            || !envelope.value("data").isObject())
        {
            return unavailable(
                capability_probe_unavailability::partial_response);
        }
        for(auto iterator = envelope.begin();
            iterator != envelope.end();
            ++iterator)
        {
            if(
                iterator.key() != QStringLiteral("data")
                && iterator.key() != QStringLiteral("extensions"))
            {
                return unavailable(
                    capability_probe_unavailability::
                        malformed_response);
            }
        }

        const auto data = envelope.value("data").toObject();
        if(!response_has_exact_aliases(data))
        {
            return unavailable(
                capability_probe_unavailability::partial_response);
        }
        if(schema_specific_limits_exceeded(data, limits))
        {
            return unavailable(
                capability_probe_unavailability::response_limit);
        }

        capability_probe_observation observation;
        if(!evaluate_schema(data, limits, observation))
        {
            return unavailable(
                capability_probe_unavailability::
                    malformed_response);
        }
        return observation;
    }
    catch(const std::bad_alloc&)
    {
        return unavailable(
            capability_probe_unavailability::allocation_failure);
    }
    catch(...)
    {
        return unavailable(
            capability_probe_unavailability::malformed_response);
    }
}
}
