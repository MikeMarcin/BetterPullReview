#include "github_read_contract.hpp"
#include "github_graphql_operation_policy.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <new>
#include <string>
#include <utility>

namespace bpr::adapters::github::detail
{
namespace
{
constexpr std::string_view rest_version = "2022-11-28";
constexpr std::size_t maximum_fixture_body_bytes = 4U * 1024U * 1024U;
constexpr std::size_t maximum_search_bytes = 256U;
constexpr qsizetype maximum_cursor_bytes = 1024;

constexpr std::string_view review_threads_document =
    "query GetReviewThreads($owner:String!,$name:String!,"
    "$number:Int!){repository(owner:$owner,name:$name){id "
    "pullRequest(number:$number){id files(first:100){nodes{"
    "path viewerViewedState} pageInfo{hasNextPage endCursor}} "
    "reviewThreads(first:100){nodes{"
    "id isResolved isOutdated viewerCanResolve viewerCanUnresolve "
    "subjectType path line originalLine "
    "comments(first:100){nodes{id bodyText path line originalLine outdated "
    "author{login ... on Node{id}} replyTo{id}} "
    "pageInfo{hasNextPage endCursor}}} "
    "pageInfo{hasNextPage "
    "endCursor}}}}}";

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
    explicit json_structural_preflight(const std::string_view input)
        : Input{input}
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
    static constexpr std::size_t maximum_depth = 32U;
    static constexpr std::size_t maximum_object_members = 64U;
    static constexpr std::size_t maximum_array_elements = 100U;
    static constexpr std::size_t maximum_total_values = 50'000U;
    static constexpr std::size_t maximum_string_bytes = 256U * 1024U;

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
            return string(nullptr, false)
                ? json_preflight_result::valid
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

        std::array<std::string_view, maximum_object_members> keys;
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
            if(count == maximum_array_elements)
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
            if(Position - start > maximum_string_bytes)
            {
                return false;
            }
            const auto character =
                static_cast<unsigned char>(Input[Position++]);
            if(character == '"')
            {
                if(output != nullptr)
                {
                    *output = Input.substr(
                        start,
                        Position - start - 1U);
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
                Position < Input.size()
                && Input[Position] >= '0' && Input[Position] <= '9')
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
    std::size_t Position = 0U;
    std::size_t Values = 0U;
};

[[nodiscard]] auto failure(const contract_failure_code code)
    -> std::unexpected<contract_failure>
{
    return std::unexpected{contract_failure{.Code = code}};
}

[[nodiscard]] auto ascii_equal_fold(
    const std::string_view left,
    const std::string_view right) noexcept -> bool
{
    if(left.size() != right.size())
    {
        return false;
    }

    for(std::size_t index = 0; index < left.size(); ++index)
    {
        const auto fold = [](const char value) {
            return value >= 'A' && value <= 'Z'
                ? static_cast<char>(value - 'A' + 'a')
                : value;
        };
        if(fold(left[index]) != fold(right[index]))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] auto safe_path_component(
    const std::string_view component) noexcept -> bool
{
    if(
        component.empty() || component.size() > 100U || component == "."
        || component == "..")
    {
        return false;
    }

    return std::ranges::all_of(component, [](const unsigned char character) {
        return std::isalnum(character) != 0 || character == '-'
            || character == '_' || character == '.';
    });
}

[[nodiscard]] auto hexadecimal_oid(const std::string_view oid) noexcept -> bool
{
    if(oid.size() != 40U && oid.size() != 64U)
    {
        return false;
    }

    return std::ranges::all_of(oid, [](const unsigned char character) {
        return std::isxdigit(character) != 0;
    });
}

[[nodiscard]] auto safe_search_terms(
    const std::string_view terms) noexcept -> bool
{
    return terms == "is:open" && terms.size() <= maximum_search_bytes;
}

[[nodiscard]] auto path_prefix(
    const server_contract server,
    const bool graphql) -> std::expected<std::string_view, contract_failure>
{
    switch(server)
    {
    case server_contract::github_com:
        return std::string_view{};
    case server_contract::ghes_3_18:
        return graphql ? std::string_view{"/api"}
                       : std::string_view{"/api/v3"};
    }
    return failure(contract_failure_code::unsupported_server_contract);
}

[[nodiscard]] auto percent_encode(const std::string_view value) -> std::string
{
    constexpr std::string_view hexadecimal = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(value.size());

    for(const auto raw_character : value)
    {
        const auto character = static_cast<unsigned char>(raw_character);
        const auto unreserved =
            std::isalnum(character) != 0 || character == '-'
            || character == '_' || character == '.' || character == '~';
        if(unreserved)
        {
            encoded.push_back(static_cast<char>(character));
            continue;
        }

        encoded.push_back('%');
        encoded.push_back(hexadecimal[(character >> 4U) & 0x0FU]);
        encoded.push_back(hexadecimal[character & 0x0FU]);
    }
    return encoded;
}

[[nodiscard]] auto rest_headers() -> std::vector<http_header>
{
    return {
        http_header{
            .Name = "Accept",
            .Value = "application/vnd.github+json",
        },
        http_header{
            .Name = "User-Agent",
            .Value = "BetterPullReview/0.1",
        },
        http_header{
            .Name = "X-GitHub-Api-Version",
            .Value = std::string{rest_version},
        },
    };
}

[[nodiscard]] auto graphql_headers() -> std::vector<http_header>
{
    return {
        http_header{
            .Name = "Accept",
            .Value = "application/json",
        },
        http_header{
            .Name = "Content-Type",
            .Value = "application/json",
        },
        http_header{
            .Name = "User-Agent",
            .Value = "BetterPullReview/0.1",
        },
    };
}

[[nodiscard]] auto compact_json(const QJsonObject& object) -> std::string
{
    const auto bytes = QJsonDocument{object}.toJson(QJsonDocument::Compact);
    return {bytes.constData(), static_cast<std::size_t>(bytes.size())};
}

[[nodiscard]] auto review_threads_body(
    const semantic_read_request& request) -> std::string
{
    QJsonObject variables;
    variables.insert(
        "name",
        QString::fromUtf8(
            request.Repository.data(),
            static_cast<qsizetype>(request.Repository.size())));
    variables.insert(
        "number",
        static_cast<qint64>(request.PullRequestNumber));
    variables.insert(
        "owner",
        QString::fromUtf8(
            request.Owner.data(),
            static_cast<qsizetype>(request.Owner.size())));

    QJsonObject document;
    document.insert(
        "query",
        QString::fromUtf8(
            review_threads_document.data(),
            static_cast<qsizetype>(review_threads_document.size())));
    document.insert("variables", variables);
    return compact_json(document);
}

[[nodiscard]] auto bounded_graphql_token(
    const std::string_view value,
    const std::size_t maximum) noexcept -> bool
{
    return !value.empty() && value.size() <= maximum
        && std::ranges::all_of(value, [](const char character) {
               const auto byte = static_cast<unsigned char>(character);
               return byte >= 0x21U && byte <= 0x7eU;
           });
}

[[nodiscard]] auto valid_graphql_page(
    const github_graphql_page_request& page,
    const std::uint32_t sequence) noexcept -> bool
{
    const auto& shape = page.Shape;
    if(
        (shape.ResolvePermission || shape.UnresolvePermission)
        && !shape.ReviewThreads)
    {
        return false;
    }

    switch(page.Kind)
    {
    case github_graphql_page_kind::initial:
        return sequence == 1U
            && (shape.ReviewThreads || shape.ViewedFileState)
            && page.AfterCursor.empty() && page.ThreadId.empty();
    case github_graphql_page_kind::files:
        return sequence > 1U && shape.ViewedFileState
            && page.ThreadId.empty()
            && bounded_graphql_token(page.AfterCursor, 1'024U);
    case github_graphql_page_kind::threads:
        return sequence > 1U && shape.ReviewThreads
            && page.ThreadId.empty()
            && bounded_graphql_token(page.AfterCursor, 1'024U);
    case github_graphql_page_kind::comments:
        return sequence > 1U && shape.ReviewThreads
            && bounded_graphql_token(page.AfterCursor, 1'024U)
            && bounded_graphql_token(page.ThreadId, 256U);
    }
    return false;
}

[[nodiscard]] auto page_info_selection() -> std::string
{
    return "pageInfo{hasNextPage endCursor}";
}

[[nodiscard]] auto file_connection_selection(
    const std::string_view arguments,
    const application::github_read_shape& shape) -> std::string
{
    std::string result{"files("};
    result += arguments;
    result += "){totalCount nodes{path";
    if(shape.ViewedFileState)
    {
        result += " viewerViewedState";
    }
    result += "} ";
    result += page_info_selection();
    result += "}";
    return result;
}

[[nodiscard]] auto comment_connection_selection(
    const std::string_view arguments) -> std::string
{
    std::string result{"comments("};
    result += arguments;
    result += "){totalCount nodes{id bodyText path line originalLine "
              "outdated author{login ... on Node{id}} replyTo{id}} ";
    result += page_info_selection();
    result += "}";
    return result;
}

[[nodiscard]] auto thread_connection_selection(
    const std::string_view arguments,
    const application::github_read_shape& shape) -> std::string
{
    std::string result{"reviewThreads("};
    result += arguments;
    result += "){totalCount nodes{id isResolved isOutdated";
    if(shape.ResolvePermission)
    {
        result += " viewerCanResolve";
    }
    if(shape.UnresolvePermission)
    {
        result += " viewerCanUnresolve";
    }
    result += " subjectType path line originalLine ";
    result += comment_connection_selection("first:100");
    result += "} ";
    result += page_info_selection();
    result += "}";
    return result;
}

[[nodiscard]] auto paged_review_body(
    const semantic_read_request& request,
    const github_graphql_page_request& page) -> std::string
{
    std::string query;
    switch(page.Kind)
    {
    case github_graphql_page_kind::initial:
        query =
            "query GetReviewData($owner:String!,$name:String!,"
            "$number:Int!){repository(owner:$owner,name:$name){id "
            "pullRequest(number:$number){id";
        if(page.Shape.ViewedFileState)
        {
            query += " ";
            query += file_connection_selection("first:100", page.Shape);
        }
        if(page.Shape.ReviewThreads)
        {
            query += " ";
            query += thread_connection_selection("first:100", page.Shape);
        }
        query += "}}}";
        break;
    case github_graphql_page_kind::files:
        query =
            "query GetReviewFilesPage($owner:String!,$name:String!,"
            "$number:Int!,$after:String!){repository(owner:$owner,"
            "name:$name){id pullRequest(number:$number){id ";
        query += file_connection_selection(
            "first:100,after:$after",
            page.Shape);
        query += "}}}";
        break;
    case github_graphql_page_kind::threads:
        query =
            "query GetReviewThreadsPage($owner:String!,$name:String!,"
            "$number:Int!,$after:String!){repository(owner:$owner,"
            "name:$name){id pullRequest(number:$number){id ";
        query += thread_connection_selection(
            "first:100,after:$after",
            page.Shape);
        query += "}}}";
        break;
    case github_graphql_page_kind::comments:
        query =
            "query GetReviewCommentsPage($owner:String!,$name:String!,"
            "$number:Int!,$thread:ID!,$after:String!){"
            "repository(owner:$owner,name:$name){id "
            "pullRequest(number:$number){id}} "
            "node(id:$thread){... on PullRequestReviewThread{id ";
        query += comment_connection_selection(
            "first:100,after:$after");
        query += "}}}";
        break;
    }

    QJsonObject variables;
    variables.insert(
        "name",
        QString::fromUtf8(
            request.Repository.data(),
            static_cast<qsizetype>(request.Repository.size())));
    variables.insert(
        "number",
        static_cast<qint64>(request.PullRequestNumber));
    variables.insert(
        "owner",
        QString::fromUtf8(
            request.Owner.data(),
            static_cast<qsizetype>(request.Owner.size())));
    if(page.Kind != github_graphql_page_kind::initial)
    {
        variables.insert(
            "after",
            QString::fromUtf8(
                page.AfterCursor.data(),
                static_cast<qsizetype>(page.AfterCursor.size())));
    }
    if(page.Kind == github_graphql_page_kind::comments)
    {
        variables.insert(
            "thread",
            QString::fromUtf8(
                page.ThreadId.data(),
                static_cast<qsizetype>(page.ThreadId.size())));
    }

    QJsonObject document;
    document.insert(
        "query",
        QString::fromUtf8(
            query.data(),
            static_cast<qsizetype>(query.size())));
    document.insert("variables", variables);
    return compact_json(document);
}

[[nodiscard]] auto repository_path(
    const std::string_view prefix,
    const semantic_read_request& request) -> std::string
{
    return std::string{prefix} + "/repos/" + request.Owner + "/"
        + request.Repository;
}

[[nodiscard]] auto pull_request_path(
    const std::string_view prefix,
    const semantic_read_request& request) -> std::string
{
    return repository_path(prefix, request) + "/pulls/"
        + std::to_string(request.PullRequestNumber);
}

[[nodiscard]] auto needs_repository(const read_operation operation) noexcept
    -> bool
{
    return operation != read_operation::get_viewer_identity;
}

[[nodiscard]] auto needs_pull_request_number(
    const read_operation operation) noexcept -> bool
{
    switch(operation)
    {
    case read_operation::get_pull_request:
    case read_operation::list_pull_request_files:
    case read_operation::list_pull_request_commits:
    case read_operation::list_reviews:
    case read_operation::list_review_threads:
    case read_operation::list_issue_comments:
        return true;
    case read_operation::get_viewer_identity:
    case read_operation::get_repository:
    case read_operation::list_checks:
    case read_operation::search_pull_requests:
        return false;
    }
    return false;
}

[[nodiscard]] auto maximum_page_for(
    const read_operation operation) noexcept -> std::uint32_t
{
    switch(operation)
    {
    case read_operation::list_pull_request_files:
        return 30U;
    case read_operation::list_pull_request_commits:
        return 3U;
    case read_operation::list_reviews:
    case read_operation::list_issue_comments:
    case read_operation::list_checks:
        return 100U;
    case read_operation::search_pull_requests:
        return 10U;
    case read_operation::get_viewer_identity:
    case read_operation::get_repository:
    case read_operation::get_pull_request:
        return 1U;
    case read_operation::list_review_threads:
        return 1'024U;
    }
    return 0U;
}

[[nodiscard]] auto known_read_operation(
    const read_operation operation) noexcept -> bool
{
    return std::ranges::find(read_operation_catalog, operation)
        != read_operation_catalog.end();
}

[[nodiscard]] auto unsafe_header_name(const std::string_view name) noexcept
    -> bool
{
    constexpr std::array<std::string_view, 6> unsafe_names{
        "Authorization",
        "Cookie",
        "Proxy-Authorization",
        "X-GitHub-OTP",
        "X-OAuth-Scopes",
        "X-Accepted-OAuth-Scopes",
    };
    return std::ranges::any_of(unsafe_names, [name](const auto candidate) {
        return ascii_equal_fold(name, candidate);
    });
}

[[nodiscard]] auto validate_graphql_body(
    const std::string_view body,
    const bool paged) -> std::expected<void, contract_failure>
{
    if(body.size() > maximum_fixture_body_bytes)
    {
        return failure(contract_failure_code::response_limit);
    }
    const auto preflight = json_structural_preflight{body}.run();
    if(preflight == json_preflight_result::structural_limit)
    {
        return failure(contract_failure_code::response_limit);
    }
    if(preflight != json_preflight_result::valid)
    {
        return failure(contract_failure_code::unexpected_body);
    }

    QJsonParseError parse_error;
    const auto document = QJsonDocument::fromJson(
        QByteArray{body.data(), static_cast<qsizetype>(body.size())},
        &parse_error);
    if(
        parse_error.error != QJsonParseError::NoError
        || !document.isObject())
    {
        return failure(contract_failure_code::unexpected_body);
    }

    const auto object = document.object();
    if(
        object.size() != 2 || !object.contains("query")
        || !object.value("query").isString()
        || !object.contains("variables")
        || !object.value("variables").isObject())
    {
        return failure(contract_failure_code::unexpected_body);
    }

    const auto encoded_query =
        object.value("query").toString().toUtf8();
    const auto policy = inspect_graphql_operation_policy(
        std::string_view{
            encoded_query.constData(),
            static_cast<std::size_t>(encoded_query.size()),
        });
    if(policy == graphql_operation_policy_result::write_operation)
    {
        return failure(contract_failure_code::mutation_operation);
    }
    if(policy == graphql_operation_policy_result::response_limit)
    {
        return failure(contract_failure_code::response_limit);
    }
    if(policy != graphql_operation_policy_result::query_only)
    {
        return failure(contract_failure_code::unexpected_body);
    }

    const auto variables = object.value("variables").toObject();
    if(
        !variables.value("owner").isString()
        || !variables.value("name").isString()
        || !variables.value("number").isDouble()
        || (!paged && variables.size() != 3))
    {
        return failure(contract_failure_code::unexpected_body);
    }
    return {};
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

[[nodiscard]] auto has_exact_keys(
    const QJsonObject& object,
    const std::initializer_list<std::string_view> expected) -> bool
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

constexpr qsizetype maximum_fixture_page_records = 100;
constexpr double maximum_exact_json_integer = 9'007'199'254'740'991.0;

[[nodiscard]] auto exact_integer(
    const QJsonValue& value,
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

[[nodiscard]] auto nonempty_string(const QJsonValue& value) -> bool
{
    return value.isString() && !value.toString().isEmpty();
}

[[nodiscard]] auto valid_line(const QJsonValue& value) noexcept -> bool
{
    return exact_integer(value, true)
        && value.toDouble()
            <= static_cast<double>(
                std::numeric_limits<std::int32_t>::max());
}

[[nodiscard]] auto valid_pull_request_number(
    const QJsonValue& value) noexcept -> bool
{
    return exact_integer(value, true)
        && value.toDouble()
            <= static_cast<double>(
                core::maximum_pull_request_number);
}

[[nodiscard]] auto valid_json_oid(const QJsonValue& value) -> bool
{
    if(!value.isString())
    {
        return false;
    }
    const auto text = value.toString();
    if(text.size() != 40 && text.size() != 64)
    {
        return false;
    }
    return std::ranges::all_of(text, [](const QChar character) {
        return (
            character >= u'0' && character <= u'9')
            || (character >= u'a' && character <= u'f')
            || (character >= u'A' && character <= u'F');
    });
}

[[nodiscard]] auto one_of(
    const QJsonValue& value,
    const std::initializer_list<std::string_view> allowed) -> bool
{
    if(!value.isString())
    {
        return false;
    }
    const auto text = value.toString();
    return std::ranges::any_of(allowed, [&text](const auto candidate) {
        return text == QString::fromUtf8(
            candidate.data(),
            static_cast<qsizetype>(candidate.size()));
    });
}

[[nodiscard]] auto exact_rest_actor(const QJsonValue& value) -> bool
{
    if(
        !value.isObject()
        || !has_exact_keys(
            value.toObject(),
            {"login", "node_id"}))
    {
        return false;
    }
    const auto object = value.toObject();
    return nonempty_string(object.value("login"))
        && nonempty_string(object.value("node_id"));
}

[[nodiscard]] auto optional_rest_actor(const QJsonValue& value) -> bool
{
    return value.isNull() || exact_rest_actor(value);
}

[[nodiscard]] auto exact_graphql_author(const QJsonValue& value) -> bool
{
    return value.isObject()
        && has_exact_keys(value.toObject(), {"id", "login"})
        && nonempty_string(value.toObject().value("id"))
        && nonempty_string(value.toObject().value("login"));
}

[[nodiscard]] auto exact_page_info(
    const QJsonValue& value,
    const qsizetype node_count) -> bool
{
    if(
        !value.isObject()
        || !has_exact_keys(
            value.toObject(),
            {"endCursor", "hasNextPage"}))
    {
        return false;
    }
    const auto object = value.toObject();
    if(
        !object.value("hasNextPage").isBool()
        || object.value("hasNextPage").toBool())
    {
        return false;
    }
    const auto cursor = object.value("endCursor");
    if(node_count == 0)
    {
        return cursor.isNull();
    }
    return nonempty_string(cursor)
        && cursor.toString().toUtf8().size() <= maximum_cursor_bytes;
}

[[nodiscard]] auto bounded_object_array(
    const QJsonValue& value,
    const std::initializer_list<std::string_view> keys) -> bool
{
    if(!value.isArray())
    {
        return false;
    }
    const auto array = value.toArray();
    return array.size() <= maximum_fixture_page_records
        && std::ranges::all_of(array, [keys](const QJsonValue& item) {
               return item.isObject()
                   && has_exact_keys(item.toObject(), keys);
           });
}

[[nodiscard]] auto validate_viewer_fixture(const QJsonDocument& document)
    -> bool
{
    if(
        !document.isObject()
        || !has_exact_keys(
            document.object(),
            {"id", "login", "name", "node_id"}))
    {
        return false;
    }
    const auto object = document.object();
    return exact_integer(object.value("id"), true)
        && nonempty_string(object.value("login"))
        && (
            object.value("name").isNull()
            || object.value("name").isString())
        && nonempty_string(object.value("node_id"));
}

[[nodiscard]] auto validate_repository_fixture(const QJsonDocument& document)
    -> bool
{
    if(
        !document.isObject()
        || !has_exact_keys(
            document.object(),
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
        return false;
    }
    const auto object = document.object();
    return exact_integer(object.value("id"), true)
        && nonempty_string(object.value("name"))
        && nonempty_string(object.value("full_name"))
        && nonempty_string(object.value("node_id"))
        && object.value("owner").isObject()
        && has_exact_keys(
            object.value("owner").toObject(),
            {"login"})
        && nonempty_string(
            object.value("owner").toObject().value("login"))
        && object.value("private").isBool()
        && nonempty_string(object.value("default_branch"));
}

[[nodiscard]] auto exact_pull_ref(
    const QJsonValue& value,
    const bool repository_required) -> bool
{
    if(
        !value.isObject()
        || !has_exact_keys(value.toObject(), {"ref", "repo", "sha"}))
    {
        return false;
    }
    const auto object = value.toObject();
    const auto repository = object.value("repo");
    if(
        !nonempty_string(object.value("ref"))
        || !valid_json_oid(object.value("sha")))
    {
        return false;
    }
    if(repository.isNull())
    {
        return !repository_required;
    }
    return repository.isObject()
        && has_exact_keys(
            repository.toObject(),
            {"full_name", "id"})
        && exact_integer(repository.toObject().value("id"), true)
        && nonempty_string(
            repository.toObject().value("full_name"));
}

[[nodiscard]] auto validate_pull_request_fixture(
    const QJsonDocument& document) -> bool
{
    if(
        !document.isObject()
        || !has_exact_keys(
            document.object(),
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
        return false;
    }
    const auto object = document.object();
    const auto merged = object.value("merged").toBool();
    return exact_integer(object.value("id"), true)
        && valid_pull_request_number(object.value("number"))
        && exact_integer(object.value("changed_files"))
        && exact_integer(object.value("commits"), true)
        && nonempty_string(object.value("node_id"))
        && one_of(object.value("state"), {"open", "closed"})
        && nonempty_string(object.value("title"))
        && (
            object.value("body").isNull()
            || object.value("body").isString())
        && object.value("draft").isBool()
        && optional_rest_actor(object.value("user"))
        && exact_pull_ref(object.value("base"), true)
        && exact_pull_ref(object.value("head"), false)
        && object.value("merged").isBool()
        && (!merged || object.value("state").toString() == "closed");
}

[[nodiscard]] auto validate_files_fixture(const QJsonDocument& document)
    -> bool
{
    if(
        !document.isArray()
        || document.array().size() > maximum_fixture_page_records)
    {
        return false;
    }
    return std::ranges::all_of(document.array(), [](const QJsonValue& value) {
        if(
            !value.isObject()
            || !(
                has_exact_keys(
                    value.toObject(),
                    {
                        "additions",
                        "deletions",
                        "filename",
                        "status",
                    })
                || has_exact_keys(
                    value.toObject(),
                    {
                        "additions",
                        "deletions",
                        "filename",
                        "previous_filename",
                        "status",
                    })))
        {
            return false;
        }
        const auto object = value.toObject();
        if(
            !nonempty_string(object.value("filename"))
            || !one_of(
                object.value("status"),
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
            || !exact_integer(object.value("deletions")))
        {
            return false;
        }
        const auto has_previous = object.contains("previous_filename");
        const auto status = object.value("status").toString();
        if(
            has_previous
            && (
                (
                    status != "renamed"
                    && status != "copied")
                || !nonempty_string(
                    object.value("previous_filename"))))
        {
            return false;
        }
        return true;
    });
}

[[nodiscard]] auto validate_commits_fixture(const QJsonDocument& document)
    -> bool
{
    if(
        !document.isArray()
        || document.array().size() > maximum_fixture_page_records)
    {
        return false;
    }
    return std::ranges::all_of(document.array(), [](const QJsonValue& value) {
        if(
            !value.isObject()
            || !has_exact_keys(
                value.toObject(),
                {"author", "commit", "node_id", "sha"}))
        {
            return false;
        }
        const auto object = value.toObject();
        const auto commit = object.value("commit");
        return valid_json_oid(object.value("sha"))
            && nonempty_string(object.value("node_id"))
            && optional_rest_actor(object.value("author"))
            && commit.isObject()
            && has_exact_keys(commit.toObject(), {"message"})
            && commit.toObject().value("message").isString();
    });
}

[[nodiscard]] auto validate_reviews_fixture(const QJsonDocument& document)
    -> bool
{
    if(
        !document.isArray()
        || document.array().size() > maximum_fixture_page_records)
    {
        return false;
    }
    return std::ranges::all_of(document.array(), [](const QJsonValue& value) {
        if(
            !value.isObject()
            || !has_exact_keys(
                value.toObject(),
                {
                    "body",
                    "commit_id",
                    "node_id",
                    "state",
                    "user",
                }))
        {
            return false;
        }
        const auto object = value.toObject();
        return nonempty_string(object.value("node_id"))
            && optional_rest_actor(object.value("user"))
            && object.value("body").isString()
            && one_of(
                object.value("state"),
                {
                    "PENDING",
                    "COMMENTED",
                    "APPROVED",
                    "CHANGES_REQUESTED",
                    "DISMISSED",
                })
            && (
                object.value("commit_id").isNull()
                || valid_json_oid(object.value("commit_id")));
    });
}

[[nodiscard]] auto exact_graphql_comment(const QJsonValue& value) -> bool
{
    if(
        !value.isObject()
        || !has_exact_keys(
            value.toObject(),
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
        return false;
    }
    const auto object = value.toObject();
    const auto reply_to = object.value("replyTo");
    const auto line = object.value("line");
    const auto original_line = object.value("originalLine");
    return nonempty_string(object.value("id"))
        && object.value("bodyText").isString()
        && nonempty_string(object.value("path"))
        && (line.isNull() || valid_line(line))
        && (
            original_line.isNull()
            || valid_line(original_line))
        && object.value("outdated").isBool()
        && (
            object.value("author").isNull()
            || exact_graphql_author(object.value("author")))
        && (
            reply_to.isNull()
            || (
                reply_to.isObject()
                && has_exact_keys(reply_to.toObject(), {"id"})
                && nonempty_string(reply_to.toObject().value("id"))));
}

[[nodiscard]] auto validate_review_threads_fixture(
    const QJsonDocument& document) -> bool
{
    if(
        !document.isObject()
        || !has_exact_keys(document.object(), {"data"}))
    {
        return false;
    }
    const auto data = document.object().value("data");
    if(
        !data.isObject()
        || !has_exact_keys(data.toObject(), {"repository"}))
    {
        return false;
    }
    const auto repository = data.toObject().value("repository");
    if(
        !repository.isObject()
        || !has_exact_keys(
            repository.toObject(),
            {"id", "pullRequest"})
        || !nonempty_string(repository.toObject().value("id")))
    {
        return false;
    }
    const auto pull_request = repository.toObject().value("pullRequest");
    if(
        !pull_request.isObject()
        || !has_exact_keys(
            pull_request.toObject(),
            {"files", "id", "reviewThreads"})
        || !nonempty_string(pull_request.toObject().value("id")))
    {
        return false;
    }
    const auto files = pull_request.toObject().value("files");
    if(
        !files.isObject()
        || !has_exact_keys(
            files.toObject(), {"nodes", "pageInfo", "totalCount"})
        || !exact_integer(files.toObject().value("totalCount"))
        || !bounded_object_array(
            files.toObject().value("nodes"),
            {"path", "viewerViewedState"}))
    {
        return false;
    }
    const auto file_nodes = files.toObject().value("nodes").toArray();
    if(
        !exact_page_info(
            files.toObject().value("pageInfo"),
            file_nodes.size())
        || files.toObject().value("totalCount").toDouble()
               != static_cast<double>(file_nodes.size())
        || !std::ranges::all_of(
            file_nodes,
            [](const QJsonValue& value) {
                const auto object = value.toObject();
                return nonempty_string(object.value("path"))
                    && one_of(
                        object.value("viewerViewedState"),
                        {"DISMISSED", "UNVIEWED", "VIEWED"});
            }))
    {
        return false;
    }
    const auto threads = pull_request.toObject().value("reviewThreads");
    if(
        !threads.isObject()
        || !has_exact_keys(
            threads.toObject(),
            {"nodes", "pageInfo", "totalCount"})
        || !exact_integer(threads.toObject().value("totalCount"))
        || !bounded_object_array(
            threads.toObject().value("nodes"),
            {
                "comments",
                "id",
                "isOutdated",
                "isResolved",
                "line",
                "originalLine",
                "path",
                "subjectType",
                "viewerCanResolve",
                "viewerCanUnresolve",
            }))
    {
        return false;
    }
    const auto thread_nodes = threads.toObject().value("nodes").toArray();
    if(
        !exact_page_info(
            threads.toObject().value("pageInfo"),
            thread_nodes.size())
        || threads.toObject().value("totalCount").toDouble()
               != static_cast<double>(thread_nodes.size()))
    {
        return false;
    }

    for(const auto& thread_value : thread_nodes)
    {
        const auto thread = thread_value.toObject();
        const auto comments = thread.value("comments");
        const auto line = thread.value("line");
        const auto original_line = thread.value("originalLine");
        const auto comment_nodes =
            comments.toObject().value("nodes").toArray();
        const auto file_subject =
            thread.value("subjectType").toString() == "FILE";
        const auto line_subject =
            thread.value("subjectType").toString() == "LINE";
        if(
            !nonempty_string(thread.value("id"))
            || !thread.value("isResolved").isBool()
            || !thread.value("isOutdated").isBool()
            || !thread.value("viewerCanResolve").isBool()
            || !thread.value("viewerCanUnresolve").isBool()
            || (!file_subject && !line_subject)
            || !nonempty_string(thread.value("path"))
            || !(
                line.isNull() || valid_line(line))
            || !(
                original_line.isNull()
                || valid_line(original_line))
            || (
                file_subject
                && (!line.isNull() || !original_line.isNull()))
            || (
                line_subject
                && line.isNull() && original_line.isNull())
            || !comments.isObject()
            || !has_exact_keys(
                comments.toObject(),
                {"nodes", "pageInfo", "totalCount"})
            || !exact_integer(comments.toObject().value("totalCount"))
            || !comments.toObject().value("nodes").isArray()
            || comment_nodes.size()
                > maximum_fixture_page_records
            || !exact_page_info(
                comments.toObject().value("pageInfo"),
                comment_nodes.size())
            || comments.toObject().value("totalCount").toDouble()
                   != static_cast<double>(comment_nodes.size())
            || !std::ranges::all_of(
                comment_nodes,
                exact_graphql_comment))
        {
            return false;
        }
        for(const auto& comment_value : comment_nodes)
        {
            const auto comment = comment_value.toObject();
            const auto comment_line = comment.value("line");
            const auto comment_original_line =
                comment.value("originalLine");
            if(
                (
                    file_subject
                    && (
                        !comment_line.isNull()
                        || !comment_original_line.isNull()))
                || (
                    line_subject
                    && comment_line.isNull()
                    && comment_original_line.isNull())
                || comment.value("path") != thread.value("path"))
            {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] auto validate_issue_comments_fixture(
    const QJsonDocument& document) -> bool
{
    if(
        !document.isArray()
        || document.array().size() > maximum_fixture_page_records)
    {
        return false;
    }
    return std::ranges::all_of(document.array(), [](const QJsonValue& value) {
        if(
            !value.isObject()
            || !has_exact_keys(
                value.toObject(),
                {
                    "body",
                    "node_id",
                    "user",
                }))
        {
            return false;
        }
        const auto object = value.toObject();
        return nonempty_string(object.value("node_id"))
            && optional_rest_actor(object.value("user"))
            && object.value("body").isString();
    });
}

[[nodiscard]] auto validate_checks_fixture(const QJsonDocument& document)
    -> bool
{
    if(
        !document.isObject()
        || !has_exact_keys(
            document.object(),
            {"check_runs", "total_count"})
        || !exact_integer(document.object().value("total_count"))
        || !bounded_object_array(
            document.object().value("check_runs"),
            {
                "conclusion",
                "head_sha",
                "name",
                "node_id",
                "status",
            }))
    {
        return false;
    }
    const auto runs = document.object().value("check_runs").toArray();
    if(
        document.object().value("total_count").toDouble()
        != static_cast<double>(runs.size()))
    {
        return false;
    }
    return std::ranges::all_of(runs, [](const QJsonValue& value) {
        const auto object = value.toObject();
        const auto completed =
            object.value("status").toString() == "completed";
        return nonempty_string(object.value("node_id"))
            && nonempty_string(object.value("name"))
            && valid_json_oid(object.value("head_sha"))
            && one_of(
                object.value("status"),
                {
                    "queued",
                    "in_progress",
                    "completed",
                    "waiting",
                    "requested",
                    "pending",
                })
            && (
                object.value("conclusion").isNull()
                || one_of(
                    object.value("conclusion"),
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
            && (
                completed
                    ? !object.value("conclusion").isNull()
                    : object.value("conclusion").isNull());
    });
}

[[nodiscard]] auto exact_search_item(const QJsonValue& value) -> bool
{
    if(
        !value.isObject()
        || !has_exact_keys(
            value.toObject(),
            {
                "draft",
                "html_url",
                "node_id",
                "number",
                "pull_request",
                "repository_url",
                "state",
                "title",
            }))
    {
        return false;
    }
    const auto object = value.toObject();
    const auto pull_request = object.value("pull_request");
    return nonempty_string(object.value("node_id"))
        && valid_pull_request_number(object.value("number"))
        && one_of(object.value("state"), {"open"})
        && nonempty_string(object.value("title"))
        && object.value("draft").isBool()
        && nonempty_string(object.value("repository_url"))
        && nonempty_string(object.value("html_url"))
        && pull_request.isObject()
        && has_exact_keys(pull_request.toObject(), {"url"})
        && nonempty_string(pull_request.toObject().value("url"));
}

[[nodiscard]] auto validate_search_fixture(const QJsonDocument& document)
    -> bool
{
    if(
        !document.isObject()
        || !has_exact_keys(
            document.object(),
            {"incomplete_results", "items", "total_count"})
        || !document.object().value("incomplete_results").isBool()
        || document.object().value("incomplete_results").toBool()
        || !exact_integer(document.object().value("total_count"))
        || !document.object().value("items").isArray()
        || document.object().value("items").toArray().size()
            > maximum_fixture_page_records)
    {
        return false;
    }
    const auto items = document.object().value("items").toArray();
    return document.object().value("total_count").toDouble()
            == static_cast<double>(items.size())
        && std::ranges::all_of(items, exact_search_item);
}

[[nodiscard]] auto response_is_incomplete(
    const read_operation operation,
    const QJsonDocument& document) -> bool
{
    if(operation == read_operation::list_checks && document.isObject())
    {
        const auto object = document.object();
        return object.value("total_count").isDouble()
            && object.value("check_runs").isArray()
            && object.value("total_count").toDouble()
                > static_cast<double>(
                    object.value("check_runs").toArray().size());
    }
    if(
        operation == read_operation::search_pull_requests
        && document.isObject())
    {
        const auto object = document.object();
        return (
            object.value("incomplete_results").isBool()
            && object.value("incomplete_results").toBool())
            || (
                object.value("total_count").isDouble()
                && object.value("items").isArray()
                && object.value("total_count").toDouble()
                    > static_cast<double>(
                        object.value("items").toArray().size()));
    }
    if(
        operation != read_operation::list_review_threads
        || !document.isObject())
    {
        return false;
    }

    const auto pull_request = document.object()
                                  .value("data")
                                  .toObject()
                                  .value("repository")
                                  .toObject()
                                  .value("pullRequest")
                                  .toObject();
    const auto files = pull_request.value("files").toObject();
    if(files.value("pageInfo")
           .toObject()
           .value("hasNextPage")
           .toBool())
    {
        return true;
    }

    const auto threads =
        pull_request.value("reviewThreads").toObject();
    if(threads.value("pageInfo")
           .toObject()
           .value("hasNextPage")
           .toBool())
    {
        return true;
    }
    return std::ranges::any_of(
        threads.value("nodes").toArray(),
        [](const QJsonValue& value) {
            return value.toObject()
                .value("comments")
                .toObject()
                .value("pageInfo")
                .toObject()
                .value("hasNextPage")
                .toBool();
        });
}

[[nodiscard]] auto response_shape_is_valid(
    const read_operation operation,
    const QJsonDocument& document) -> bool
{
    switch(operation)
    {
    case read_operation::get_viewer_identity:
        return validate_viewer_fixture(document);
    case read_operation::get_repository:
        return validate_repository_fixture(document);
    case read_operation::get_pull_request:
        return validate_pull_request_fixture(document);
    case read_operation::list_pull_request_files:
        return validate_files_fixture(document);
    case read_operation::list_pull_request_commits:
        return validate_commits_fixture(document);
    case read_operation::list_reviews:
        return validate_reviews_fixture(document);
    case read_operation::list_review_threads:
        return validate_review_threads_fixture(document);
    case read_operation::list_issue_comments:
        return validate_issue_comments_fixture(document);
    case read_operation::list_checks:
        return validate_checks_fixture(document);
    case read_operation::search_pull_requests:
        return validate_search_fixture(document);
    }
    return false;
}
}

auto operation_name(const read_operation operation) noexcept
    -> std::string_view
{
    switch(operation)
    {
    case read_operation::get_viewer_identity:
        return "GetViewerIdentity";
    case read_operation::get_repository:
        return "GetRepository";
    case read_operation::get_pull_request:
        return "GetPullRequest";
    case read_operation::list_pull_request_files:
        return "ListPullRequestFiles";
    case read_operation::list_pull_request_commits:
        return "ListPullRequestCommits";
    case read_operation::list_reviews:
        return "ListReviews";
    case read_operation::list_review_threads:
        return "ListReviewThreads";
    case read_operation::list_issue_comments:
        return "ListIssueComments";
    case read_operation::list_checks:
        return "ListChecks";
    case read_operation::search_pull_requests:
        return "SearchPullRequests";
    }
    return {};
}

auto read_operation_from_name(const std::string_view name) noexcept
    -> operation_result
{
    for(const auto operation : read_operation_catalog)
    {
        if(operation_name(operation) == name)
        {
            return operation;
        }
    }

    constexpr std::array<std::string_view, 8> mutations{
        "MarkFileViewed",
        "MarkFileUnviewed",
        "CreateReview",
        "SubmitReview",
        "CreateThread",
        "ReplyToThread",
        "ResolveThread",
        "UnresolveThread",
    };
    if(std::ranges::find(mutations, name) != mutations.end())
    {
        return failure(contract_failure_code::mutation_operation);
    }
    return failure(contract_failure_code::unsupported_operation);
}

auto compile_read_request_impl(
    const semantic_read_request& request,
    const bool validate_result)
    -> request_result
{
    if(!known_read_operation(request.Operation))
    {
        return failure(contract_failure_code::unsupported_operation);
    }
    const auto repository_required = needs_repository(request.Operation);
    if(repository_required)
    {
        if(
            !safe_path_component(request.Owner)
            || !safe_path_component(request.Repository))
        {
            return failure(contract_failure_code::invalid_repository);
        }
    }
    else if(!request.Owner.empty() || !request.Repository.empty())
    {
        return failure(contract_failure_code::invalid_repository);
    }
    const auto pull_request_number_required =
        needs_pull_request_number(request.Operation);
    if(
        (
            pull_request_number_required
            && (
                request.PullRequestNumber == 0U
                || request.PullRequestNumber
                    > core::maximum_pull_request_number))
        || (
            !pull_request_number_required
            && request.PullRequestNumber != 0U))
    {
        return failure(
            contract_failure_code::invalid_pull_request_number);
    }
    const auto maximum_operation_page =
        maximum_page_for(request.Operation);
    if(request.Page == 0U || request.Page > maximum_operation_page)
    {
        return failure(contract_failure_code::invalid_page);
    }
    const auto head_oid_required =
        request.Operation == read_operation::list_checks;
    if(
        (head_oid_required && !hexadecimal_oid(request.HeadOid))
        || (!head_oid_required && !request.HeadOid.empty()))
    {
        return failure(contract_failure_code::invalid_commit_oid);
    }
    const auto search_terms_required =
        request.Operation == read_operation::search_pull_requests;
    if(
        (
            search_terms_required
            && !safe_search_terms(request.SearchTerms))
        || (
            !search_terms_required
            && !request.SearchTerms.empty()))
    {
        return failure(contract_failure_code::invalid_search_terms);
    }

    const auto graphql =
        request.Operation == read_operation::list_review_threads;
    if(
        (!graphql && request.GraphqlPage.has_value())
        || (
            request.GraphqlPage.has_value()
            && !valid_graphql_page(
                *request.GraphqlPage,
                request.Page))
        || (
            graphql && !request.GraphqlPage.has_value()
            && request.Page != 1U))
    {
        return failure(contract_failure_code::unexpected_body);
    }
    const auto prefix = path_prefix(request.Server, graphql);
    if(!prefix.has_value())
    {
        return std::unexpected{prefix.error()};
    }

    wire_request result{
        .Semantic = request,
        .Server = request.Server,
        .Operation = request.Operation,
        .Method = graphql ? http_method::post : http_method::get,
        .Path = {},
        .Query = {},
        .Headers = graphql ? graphql_headers() : rest_headers(),
        .Body = {},
    };

    switch(request.Operation)
    {
    case read_operation::get_viewer_identity:
        result.Path = std::string{*prefix} + "/user";
        break;
    case read_operation::get_repository:
        result.Path = repository_path(*prefix, request);
        break;
    case read_operation::get_pull_request:
        result.Path = pull_request_path(*prefix, request);
        break;
    case read_operation::list_pull_request_files:
        result.Path = pull_request_path(*prefix, request) + "/files";
        result.Query = "page=" + std::to_string(request.Page)
            + "&per_page=100";
        break;
    case read_operation::list_pull_request_commits:
        result.Path = pull_request_path(*prefix, request) + "/commits";
        result.Query = "page=" + std::to_string(request.Page)
            + "&per_page=100";
        break;
    case read_operation::list_reviews:
        result.Path = pull_request_path(*prefix, request) + "/reviews";
        result.Query = "page=" + std::to_string(request.Page)
            + "&per_page=100";
        break;
    case read_operation::list_review_threads:
        result.Path = std::string{*prefix} + "/graphql";
        result.Body = request.GraphqlPage.has_value()
            ? paged_review_body(request, *request.GraphqlPage)
            : review_threads_body(request);
        break;
    case read_operation::list_issue_comments:
        result.Path = repository_path(*prefix, request) + "/issues/"
            + std::to_string(request.PullRequestNumber) + "/comments";
        result.Query = "page=" + std::to_string(request.Page)
            + "&per_page=100";
        break;
    case read_operation::list_checks:
        result.Path = repository_path(*prefix, request) + "/commits/"
            + request.HeadOid
            + "/check-runs";
        result.Query = "page=" + std::to_string(request.Page)
            + "&per_page=100";
        break;
    case read_operation::search_pull_requests:
    {
        result.Path = std::string{*prefix} + "/search/issues";
        const auto expression = "is:pr " + request.SearchTerms + " repo:"
            + request.Owner + "/" + request.Repository;
        result.Query = "page=" + std::to_string(request.Page)
            + "&per_page=100&q=" + percent_encode(expression);
        break;
    }
    }

    if(validate_result)
    {
        if(const auto validation = validate_outbound_read_request(result);
           !validation.has_value())
        {
            return std::unexpected{validation.error()};
        }
    }
    return result;
}

auto compile_read_request(const semantic_read_request& request)
    -> request_result
{
    return compile_read_request_impl(request, true);
}

auto validate_outbound_read_request(const wire_request& request)
    -> std::expected<void, contract_failure>
{
    if(!known_read_operation(request.Operation))
    {
        return failure(contract_failure_code::unsupported_operation);
    }
    if(
        std::ranges::any_of(
            request.Headers,
            [](const http_header& header) {
                return unsafe_header_name(header.Name);
            }))
    {
        return failure(contract_failure_code::unsafe_header);
    }

    const auto graphql =
        request.Operation == read_operation::list_review_threads;
    if(graphql)
    {
        if(request.Method != http_method::post)
        {
            return failure(contract_failure_code::unexpected_method);
        }
        if(const auto validation = validate_graphql_body(
               request.Body,
               request.Semantic.GraphqlPage.has_value());
           !validation.has_value())
        {
            return validation;
        }
    }
    else if(request.Method != http_method::get)
    {
        return failure(contract_failure_code::mutation_operation);
    }
    else if(!request.Body.empty())
    {
        return failure(contract_failure_code::unexpected_body);
    }

    const auto canonical =
        compile_read_request_impl(request.Semantic, false);
    if(!canonical.has_value())
    {
        return std::unexpected{canonical.error()};
    }
    if(
        request.Server != canonical->Server
        || request.Semantic.Server != request.Server)
    {
        return failure(contract_failure_code::unexpected_path);
    }
    if(
        request.Operation != canonical->Operation
        || request.Semantic.Operation != request.Operation)
    {
        return failure(contract_failure_code::unexpected_operation);
    }
    if(request.Method != canonical->Method)
    {
        return failure(contract_failure_code::unexpected_method);
    }
    if(request.Path != canonical->Path)
    {
        return failure(contract_failure_code::unexpected_path);
    }
    if(request.Query != canonical->Query)
    {
        return failure(contract_failure_code::unexpected_query);
    }
    if(request.Headers != canonical->Headers)
    {
        return failure(contract_failure_code::unexpected_headers);
    }
    if(request.Body != canonical->Body)
    {
        return failure(contract_failure_code::unexpected_body);
    }
    return {};
}

auto validate_strict_fixture_response(
    const read_operation operation,
    const wire_response& response) -> std::expected<void, contract_failure>
{
    if(!known_read_operation(operation))
    {
        return failure(contract_failure_code::unsupported_operation);
    }
    switch(response.Completeness)
    {
    case page_completeness::complete:
        break;
    case page_completeness::has_next_page:
        return failure(contract_failure_code::response_limit);
    case page_completeness::unknown:
        return failure(
            contract_failure_code::unexpected_response_shape);
    default:
        return failure(
            contract_failure_code::unexpected_response_shape);
    }
    if(response.Status != 200)
    {
        return failure(contract_failure_code::invalid_response_status);
    }
    if(response.Body.size() > maximum_fixture_body_bytes)
    {
        return failure(contract_failure_code::response_limit);
    }

    try
    {
        const std::vector<http_header> expected_headers{
            http_header{
                .Name = "Content-Type",
                .Value = "application/json",
            },
        };
        if(response.Headers != expected_headers)
        {
            return failure(
                contract_failure_code::invalid_response_headers);
        }

        const auto preflight =
            json_structural_preflight{response.Body}.run();
        if(preflight == json_preflight_result::structural_limit)
        {
            return failure(contract_failure_code::response_limit);
        }
        if(preflight == json_preflight_result::duplicate_key)
        {
            return failure(
                contract_failure_code::unexpected_response_fields);
        }
        if(preflight != json_preflight_result::valid)
        {
            return failure(contract_failure_code::malformed_response);
        }

        QJsonParseError parse_error;
        const auto document = QJsonDocument::fromJson(
            QByteArray{
                response.Body.data(),
                static_cast<qsizetype>(response.Body.size()),
            },
            &parse_error);
        if(parse_error.error != QJsonParseError::NoError)
        {
            return failure(contract_failure_code::malformed_response);
        }
        if(response_is_incomplete(operation, document))
        {
            return failure(contract_failure_code::response_limit);
        }
        if(!response_shape_is_valid(operation, document))
        {
            return failure(
                contract_failure_code::unexpected_response_fields);
        }
        return {};
    }
    catch(const std::bad_alloc&)
    {
        return failure(contract_failure_code::response_limit);
    }
}

strict_recorded_transport::strict_recorded_transport(
    std::vector<recorded_exchange> exchanges)
    : Exchanges{std::move(exchanges)}
{
}

auto strict_recorded_transport::perform(const wire_request& request)
    -> response_result
{
    if(const auto validation = validate_outbound_read_request(request);
       !validation.has_value())
    {
        return std::unexpected{validation.error()};
    }
    if(NextExchange >= Exchanges.size())
    {
        return failure(contract_failure_code::recording_exhausted);
    }

    const auto& expected = Exchanges[NextExchange];
    if(request.Operation != expected.Request.Operation)
    {
        return failure(contract_failure_code::unexpected_operation);
    }
    if(request.Method != expected.Request.Method)
    {
        return failure(contract_failure_code::unexpected_method);
    }
    if(request.Path != expected.Request.Path)
    {
        return failure(contract_failure_code::unexpected_path);
    }
    if(request.Query != expected.Request.Query)
    {
        return failure(contract_failure_code::unexpected_query);
    }
    if(request.Headers != expected.Request.Headers)
    {
        return failure(contract_failure_code::unexpected_headers);
    }
    if(request.Body != expected.Request.Body)
    {
        return failure(contract_failure_code::unexpected_body);
    }
    if(request.Server != expected.Request.Server)
    {
        return failure(contract_failure_code::unexpected_path);
    }

    if(const auto validation = validate_strict_fixture_response(
           request.Operation,
           expected.Response);
       !validation.has_value())
    {
        return std::unexpected{validation.error()};
    }
    ++NextExchange;
    return expected.Response;
}

auto strict_recorded_transport::consumed_all() const noexcept -> bool
{
    return NextExchange == Exchanges.size();
}
}
